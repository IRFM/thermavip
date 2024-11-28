/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "VipSearchLineEdit.h"
#include "VipDisplayArea.h"
#include "VipToolWidget.h"

#ifdef __VIP_USE_WEB_ENGINE
#include "VipWebBrowser.h"
#endif

#include <QStandardPaths>
#include <QListWidget>
#include <QApplication>
#include <QKeyEvent>

#include <memory>
#include <vector>

bool VipDeviceOpenHelper::open(const QString& valid_path) const
{
	return vipGetMainWindow()->openPaths(QStringList() << valid_path).size() > 0;
}


using VipDeviceOpenHelperPtr = std::unique_ptr<VipDeviceOpenHelper>;
static std::vector<VipDeviceOpenHelperPtr> initHelpers()
{
	std::vector<VipDeviceOpenHelperPtr> res;
	res.push_back(VipDeviceOpenHelperPtr(new VipFileOpenHelper()));
	res.push_back(VipDeviceOpenHelperPtr(new VipShortcutsHelper()));
	return res;
}
	
static std::vector<VipDeviceOpenHelperPtr> _open_helpers = initHelpers();

void VipDeviceOpenHelper::registerHelper(VipDeviceOpenHelper* helper) 
{
	_open_helpers.push_back(VipDeviceOpenHelperPtr(helper));
}

/// @brief Returns the VipDeviceOpenHelper that can handle given format
const VipDeviceOpenHelper* VipDeviceOpenHelper::fromFormat(const QString& format)
{
	for (VipDeviceOpenHelperPtr& h : _open_helpers) {
		if (!h->validPathFromFormat(format).isEmpty())
			return h.get();
	}
	return nullptr;
}

/// @brief Returns the VipDeviceOpenHelper that can handle given path
const VipDeviceOpenHelper* VipDeviceOpenHelper::fromValidPath(const QString& valid_path)
{
	for (VipDeviceOpenHelperPtr& h : _open_helpers) {
		if (!h->formatFromValidPath(valid_path).isEmpty())
			return h.get();
	}
	return nullptr;
}

QStringList VipDeviceOpenHelper::possibleFormats(const QString& user_input)
{
	QStringList res;
	for (VipDeviceOpenHelperPtr& h : _open_helpers) {
		res += h->format(user_input);
	}
	return res;
}


#define VIP_HISTORY_LIMIT 50
static QList<VipDeviceOpenHelper::ShortcupPath> _history;

bool VipDeviceOpenHelper::addToHistory(const ShortcupPath& shortcut)
{
	// find duplicate
	for (int i = 0; i < _history.size(); ++i) {
		ShortcupPath& p = _history[i];
		if (p.format == shortcut.format) {
			// move to front
			_history.removeAt(i);
			_history.push_front(shortcut);
			return true;
		}
	}
	
	_history.push_front(shortcut);
	if (_history.size() > VIP_HISTORY_LIMIT)
		_history.pop_back();
	return true;
}

bool VipDeviceOpenHelper::addToHistory(const QString& valid_path)
{
	const VipDeviceOpenHelper* helper = fromValidPath(valid_path);
	if (!helper)
		return false;
	return addToHistory(ShortcupPath{ helper->formatFromValidPath(valid_path), helper });
}

int VipDeviceOpenHelper::addToHistory(const QStringList& valid_paths) 
{
	int res = 0;
	for (const QString& path : valid_paths) {
		if (addToHistory(path))
			++res;
	}
	return res;
}

const QList<VipDeviceOpenHelper::ShortcupPath>& VipDeviceOpenHelper::history()
{
	return _history;
}



QStringList VipFileOpenHelper::format(const QString& user_input) const 
{ 
	QString path = user_input;
	path.replace("\\", "/");

	if (path.startsWith("~")) {
		// Replace '~'
		QString home = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).front();
		if (home.endsWith("/"))
			home = home.mid(0, home.size() - 1);
		path = home + path.mid(1);
	}

	int idx = path.lastIndexOf("/");

	QString folder;
	QString prefix;
	if (idx >= 0) {
		folder = path.mid(0, idx + 1);
		prefix = path.mid(idx + 1);
	}
	else {
		// Consider in home
		folder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).front();
		prefix = path;
	}

	if (!folder.endsWith("/"))
		folder += "/";

	QDir dir(folder);
	if (!dir.exists())
		return QStringList();

	const auto lst = dir.entryInfoList(QDir::NoDot | QDir::Files | QDir::Dirs, QDir::Name);
	QStringList res;

	// find prefix in dir content
	for (const QFileInfo& info : lst) {
		if (info.fileName().startsWith(prefix, Qt::CaseInsensitive))
			res.push_back(folder + info.fileName() + (info.isDir() ? "/" : ""));
	}
	return res;
}


QString VipFileOpenHelper::validPathFromFormat(const QString& format) const 
{
#ifdef __VIP_USE_WEB_ENGINE
	VipHTTPFileHandler h;
	if (h.probe(format, QByteArray()))
		return format;
#endif

	if (format.startsWith("thermavip://"))
		return format;

	QFileInfo info(format);
	if (info.exists()) {
		QString path = info.canonicalFilePath();
		path.replace("\\", "/");
		if (info.isDir() && !path.endsWith("/"))
			path += "/";
		return path;
	}
	return QString();
}
QString VipFileOpenHelper::formatFromValidPath(const QString& path) const
{
#ifdef __VIP_USE_WEB_ENGINE
	VipHTTPFileHandler h;
	if (h.probe(path, QByteArray()))
		return path;
#endif

	if (path.startsWith("thermavip://"))
		return path;

	// first, we need to remove potential class prefix
	int idx = path.indexOf(":");
	if (idx == 0)
		// invalid path
		return QString();

	if (idx > 0) {
		QByteArray classname = path.mid(0, idx).toLatin1() + "*";
		auto types = vipUserTypes();
		for (int id : types) {
			if (QMetaType(id).name() == (classname)) {
				// remove class name prefix
				return validPathFromFormat(path.mid(idx + 1));
			}
		}
	}

	// the ':' is part of the path (possible on Windows)
	return validPathFromFormat(path);
}

bool VipFileOpenHelper::directOpen(const QString& format) const
{
	QFileInfo info(format);
	return info.exists() && !info.isDir();
}



static QMap<QString, std::function<void()>> _shortcuts;

QStringList VipShortcutsHelper::format(const QString& user_input) const 
{
	QStringList res;
	for (auto it = _shortcuts.begin(); it != _shortcuts.end(); ++it) {
		if (it.key().contains(user_input, Qt::CaseInsensitive))
			res.push_back(it.key());
	}
	return res;
}

QString VipShortcutsHelper::validPathFromFormat(const QString& format) const
{
	return formatFromValidPath(format);
}
QString VipShortcutsHelper::formatFromValidPath(const QString& path) const
{
	auto it = _shortcuts.find(path);
	if (it == _shortcuts.end())
		return QString();
	return it.key();
}

bool VipShortcutsHelper::registerShorcut(const QString& format, const std::function<void()>& fun)
{
	_shortcuts.insert(format, fun);
	return true;
}

bool VipShortcutsHelper::open(const QString& valid_path) const 
{
	auto it = _shortcuts.find(valid_path);
	if (it == _shortcuts.end())
		return false;
	it.value()();
	return true;
}





class PopupListWidget : public QListWidget
{
	QLineEdit* edit;
	QStringList content;

public: 
	PopupListWidget(QLineEdit * e)
	  : QListWidget(e)
	  , edit(e)
	{
		setWindowFlags( Qt::FramelessWindowHint | Qt::Tool);
		setStyleSheet("QListWidget{border: 1px solid gray; border-radius: 5px;}");
		connect(this, &QListWidget::itemClicked, this, &PopupListWidget::setSelectionToLineEdit);
		viewport()->installEventFilter(this);
	}

	void clear() 
	{ 
		QListWidget::clear();
		content = QStringList();
		hide();
	}
	void setContent(const QStringList& lst)
	{
		if (lst != content) {
			content = lst;
			QListWidget::clear();
			addItems(lst);
			if (count())
				setCurrentItem(item(0));

			setFixedSize(edit->width(),//sizeHintForColumn(0) + frameWidth() * 2,
						     sizeHintForRow(0) * count() + 2 * frameWidth() + 10);
		}
	}
	bool setSelectionToLineEdit(bool try_open)
	{
		if (!this->currentItem() && count())
			setCurrentItem(item(0));
		if (this->currentItem()) {
			QString previous = edit->text();
			QString text = this->currentItem()->text();
			edit->setText(text);

			// hide if the current item was the only one
			if (count() == 1)
				this->hide();

			// try to open
			if (try_open) {
				const VipDeviceOpenHelper* helper = VipDeviceOpenHelper::fromFormat(text);
				if (!helper)
					return false;
				if (helper->directOpen(text) || previous == text)
					return helper->open(helper->validPathFromFormat(text));
			}
			return false;
		}

		// try to open
		if (try_open) {
			if (const VipDeviceOpenHelper* helper = VipDeviceOpenHelper::fromFormat(edit->text()))
				return helper->open(helper->validPathFromFormat(edit->text()));
		}
		return false;
	}

protected:

	

	virtual void focusOutEvent(QFocusEvent* evt) 
	{ 
		QWidget* w = qApp->focusWidget();
		if (w && w != this && w != edit && w != viewport())
			this->hide();
	}

	virtual bool eventFilter(QObject* watched, QEvent* event)
	{
		if (event->type() == QEvent::KeyPress) {
			QKeyEvent* evt = static_cast<QKeyEvent*>(event);
			if (evt->key() == Qt::Key_Up || evt->key() == Qt::Key_Down) {
				QListWidget::keyPressEvent(evt);
				return true;
			}
			if (evt->key() == Qt::Key_Tab) {
				setSelectionToLineEdit(false);
				evt->accept();
				return true;
			}
			if (evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return) {
				if(setSelectionToLineEdit(true)) {
					evt->accept();
					return true;
				}
			}
			QApplication::instance()->sendEvent(edit, evt);
			return true;
		}
		return false;
	}

	virtual void keyPressEvent(QKeyEvent* evt) 
	{ 
		eventFilter(this, evt);
	}
};



static void registerToolWidgets()
{
	static bool init = false;
	if (init)
		return;
	init = true;

	// Register all VipToolWidget objects as shortcuts.
	// We do this in a lazy way to let plugins the time
	// to add new VipToolWidget.
	auto lst = vipGetMainWindow()->findChildren<VipToolWidget*>();
	for (VipToolWidget* w : lst) {
		QString name = w->objectName();
		if (!name.isEmpty()) {
			VipShortcutsHelper::registerShorcut(name, [w] { w->setVisible(true); });
		}
	}
}


class VipSearchLineEdit::PrivateData
{
public:
	PopupListWidget* history;
	//QAction* open;
	QAction* displayHistory;
};

VipSearchLineEdit::VipSearchLineEdit(QWidget* parent)
  : QLineEdit(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	this->setPlaceholderText("Search or open path");
	this->setToolTip("<b>Open file/signal or browse history</b><br>"
			 "Press ENTER to open entered file/signal.<br>"
			 "Press TAB to select entry from the completer.");

	this->setClearButtonEnabled(true);
	this->setMinimumWidth(200);
	//d_data->open = this->addAction(vipIcon("open.png"), QLineEdit::LeadingPosition);
	//d_data->open->setToolTip("Open path");
	d_data->displayHistory = this->addAction(vipIcon("search.png"), QLineEdit::TrailingPosition);
	d_data->displayHistory->setToolTip("Display history");
	
	d_data->history = new PopupListWidget(this);
	
	//connect(d_data->open, SIGNAL(triggered(bool)), this, SLOT(returnPressed()));
	connect(d_data->displayHistory, SIGNAL(triggered(bool)), this, SLOT(displayHistory()));
	connect(this, SIGNAL(textChanged(const QString &)), this, SLOT(textEntered()));
}

VipSearchLineEdit ::~VipSearchLineEdit()
{
	delete d_data->history;
}

void VipSearchLineEdit::focusOutEvent(QFocusEvent* evt)
{
	QWidget* w = qApp->focusWidget();
	if (w && w != this && w != d_data->history && w != d_data->history->viewport())
		d_data->history->hide();
}

void VipSearchLineEdit::showHistoryWidget(const QStringList& lst)
{
	QPoint global_pos;
	if (parentWidget())
		global_pos = parentWidget()->mapToGlobal(this->pos());
	else
		global_pos = this->pos();
	d_data->history->move(global_pos + QPoint(0, 5 + this->height()));
	d_data->history->setMaximumWidth(this->width());
	d_data->history->setMaximumHeight(800);
	if (!lst.isEmpty())
		d_data->history->setContent(lst);
	if (d_data->history->count()) {
		d_data->history->show();
		d_data->history->raise();
	}
}

void VipSearchLineEdit::returnPressed()
{
	d_data->history->setSelectionToLineEdit(true);
}
void VipSearchLineEdit::textEntered() 
{
	registerToolWidgets();

	QString user_input = this->text();
	QStringList formats;
	if (!user_input.isEmpty())
		formats = VipDeviceOpenHelper::possibleFormats(user_input);
	if (formats.isEmpty())
		d_data->history->clear();
	showHistoryWidget(formats);
}

void VipSearchLineEdit::displayHistory()
{
	const auto& history = VipDeviceOpenHelper::history();
	QStringList entries;
	for (const auto& s : history) {
		entries.push_back(s.format);
	}
	d_data->history->clear();
	showHistoryWidget(entries);
}

bool VipSearchLineEdit::event(QEvent* event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent* evt = static_cast<QKeyEvent*>(event);
		if (evt->key() == Qt::Key_Space && (evt->modifiers() & Qt::CTRL)) {
			evt->accept();
			if (d_data->history->count())
				showHistoryWidget(QStringList());
			return true;
		}
		if (evt->key() == Qt::Key_Tab || evt->key() == Qt::Key_Up || evt->key() == Qt::Key_Down) {
			QApplication::instance()->sendEvent(d_data->history, evt);
			return true;
		}
		if (evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return) {
			evt->accept();
			returnPressed();
			return true;
		}
	}
	return QLineEdit::event( event);
}
