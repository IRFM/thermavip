#include <QColor>
#include <QTextCursor>
#include <QTextCodec>
#include <QKeyEvent>
#include <QTextStream>
#include <QTextBoundaryFinder>
#include <qapplication.h>
#include <qsplitter.h>
#include <qscrollbar.h>
#include <qtoolbutton.h>
#include <QTextBlock>
#include <QBoxLayout>
#include <QScrollBar>
#include <qstyleoption.h>
#include <qstyle.h>

#include "VipGui.h"
#include "VipStandardWidgets.h"
#include "VipDisplayArea.h"
#include "VipEnvironment.h"

#include "VipPyOperation.h"
#include "VipTextEditor.h"
#include "VipPyShellWidget.h"

#include "VipLogging.h"

QString vipGetPythonHistoryFile(const QString& suffix)
{
	// Not under vipGetPythonDirectory(), and not a .py: that directory is scanned at
	// every start and each .py in it that declares a Thermavip class is executed in
	// full. Typing one such class in the console once was enough for the whole
	// command history to be replayed on the next start, outside any interactive
	// context and without a word.
	return vipGetDataDirectory(suffix) + "python_history.txt";
}

VipPyHistoryList::VipPyHistoryList(qint32 max_size)
  : m_maxSize(max_size)
  , m_pos(0)
{
}

void VipPyHistoryList::setHistoryFile(const QString& filename)
{
	m_historyFile = filename;
	m_commands.clear();
	m_lastDate = QDate();
	m_pos = 0;

	QFile fin(filename);
	if (fin.open(QFile::ReadOnly)) {
		QList<QByteArray> lines = fin.readAll().split('\n');

		for (int i = 0; i < lines.size(); ++i) {
			if (lines[i].startsWith("# ")) {
				// read date
				m_lastDate = QDate::fromString(lines[i].mid(2).data(), "yyyy/MM/dd");
			}
			else {
				Command c;
				c.date = m_lastDate;
				c.command = lines[i];
				m_commands.append(c);

				if (m_commands.size() > m_maxSize && m_maxSize > 0)
					m_commands.pop_front();
			}
		}
	}

	if (m_commands.size())
		m_pos = -1;
}

void VipPyHistoryList::addCommand(const QString& cmd, const QDate& date)
{
	// do not record twice a command
	if (m_commands.size() && cmd == m_commands.last().command)
		return;

	Command c;
	c.command = cmd;
	c.date = date;
	if (c.date.isNull())
		c.date = QDate::currentDate();

	m_commands.append(c);
	if (m_commands.size() > m_maxSize && m_maxSize > 0)
		m_commands.pop_front();
	m_pos = -1;

	QDate current = QDate::currentDate();
	if (!m_historyFile.isEmpty() && !cmd.startsWith("#")) {
		// add to file
		QFile fout(m_historyFile);
		// try to open for 1s
		qint64 start = QDateTime::currentMSecsSinceEpoch();
		while (!fout.open(QFile::WriteOnly | QFile::Append) && (QDateTime::currentMSecsSinceEpoch() - start) < 1000)
			QThread::msleep(10);
		if (!fout.isOpen())
			return;

		fout.seek(fout.size());
		QTextStream str(&fout);
		// update date
		if (current > m_lastDate) {
			str << "\n# " << current.toString("yyyy/MM/dd") << "\n\n";
			m_lastDate = current;
		}
		// write command
		str << cmd << "\n";
	}
}

QString VipPyHistoryList::next()
{
	if (m_commands.size() == 0)
		return QString();

	if (m_pos == -1)
		m_pos = m_commands.size() - 1;
	else
		m_pos++;

	if (m_pos >= m_commands.size())
		m_pos = 0;

	return m_commands[m_pos].command;
}

QString VipPyHistoryList::previous()
{
	if (m_commands.size() == 0)
		return QString();

	if (m_pos == -1)
		m_pos = m_commands.size() - 1;
	else
		m_pos--;

	if (m_pos < 0)
		m_pos = m_commands.size() - 1;

	return m_commands[m_pos].command;
}

class VipPyShellWidget::PrivateData
{
public:
	QPointer<VipBaseIOOperation> process;
	qint64 last_append_pos{ 0 };
	VipPyHistoryList commands;

	QString last_command;
	QString last_output;
	QStringList last_args;

	QIcon running_icon;
	QIcon finished_icon;
	QColor error_color;
	bool wait_for_more{ false };
	bool inside_magic_command{ false };
};

VipPyShellWidget::VipPyShellWidget(QWidget* parent)
  : QTextEdit(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->error_color = vipDefaultTextErrorColor(this);
	d_data->process = nullptr;

	/*QByteArray style = qApp->styleSheet().toLatin1();
	int index = style.indexOf("QTextEdit");
	if (index >= 0) {
	index = style.indexOf("{", index);
	if (index >= 0) {
	++index;
	int end = style.indexOf("}", index);
	if (end >= 0) {
	setStyleSheet(style.mid(index, end - index));
	vip_debug("style sheet: '%s'\n", style.mid(index, end - index).data());
	}
	}
	}*/

	this->setReadOnly(false);
	this->setUndoRedoEnabled(false);
	this->setWordWrapMode(QTextOption::NoWrap);
	this->setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());

	d_data->commands.setHistoryFile(vipGetPythonHistoryFile());
}

VipPyShellWidget::~VipPyShellWidget() {}

VipBaseIOOperation* VipPyShellWidget::process() const
{
	return d_data->process;
}

void VipPyShellWidget::setProcess(VipBaseIOOperation* proc)
{
	if (d_data->process) {
		disconnect(d_data->process, SIGNAL(readyReadStandardOutput()), this, SLOT(newOutput()));
		disconnect(d_data->process, SIGNAL(readyReadStandardError()), this, SLOT(newError()));
	}

	d_data->process = proc;
	if (d_data->process) {

		newOutput();
		newError();
		connect(d_data->process, SIGNAL(readyReadStandardOutput()), this, SLOT(newOutput()));
		connect(d_data->process, SIGNAL(readyReadStandardError()), this, SLOT(newError()));
	}
}

void VipPyShellWidget::newOutput()
{
	QByteArray bytes = d_data->process->readAllStandardOutput();
	QTextCodec* codec = QTextCodec::codecForName("IBM850");
	QString str = codec->toUnicode(bytes);
	this->appendText(str, vipWidgetTextBrush(this).color());
	d_data->last_output = str;
	if (str.endsWith(">>> ") || str.endsWith("... ") || str.endsWith("] ") || str.endsWith("> "))
		d_data->wait_for_more = true;
	// vip_debug("out: '%s'\n", str.toLatin1().data());
}

void VipPyShellWidget::newError()
{
	this->moveCursor(QTextCursor::End);
	QByteArray bytes = d_data->process->readAllStandardError();
	QTextCodec* codec = QTextCodec::codecForName("IBM850");
	QString str = codec->toUnicode(bytes);
	appendText(str, vipDefaultTextErrorColor(this)); // Qt::red);
}

qint64 VipPyShellWidget::lastPosition() const
{
	return this->document()->characterCount();
}

qint64 VipPyShellWidget::lastAppendPosition() const
{
	return d_data->last_append_pos;
}

QTextCursor VipPyShellWidget::getValidCursor()
{
	QTextCursor cursor = textCursor();
	int pos = qMin(cursor.anchor(), cursor.position());
	int pos_end = qMax(cursor.anchor(), cursor.position());
	if (pos_end > lastPosition() - 1)
		pos_end = lastPosition() - 1;
	if (pos_end < lastAppendPosition())
		return QTextCursor();

	cursor.setPosition(qMax(qint64(pos), lastAppendPosition()));
	cursor.setPosition(pos_end, QTextCursor::KeepAnchor);

	return cursor;
}

void VipPyShellWidget::appendText(const QString& str, const QColor& color)
{
	this->moveCursor(QTextCursor::End);
	this->setTextColor(color);
	this->insertPlainText(str);
	this->setTextColor(vipWidgetTextBrush(this).color());

	d_data->last_append_pos = lastPosition() - 1;
}

void VipPyShellWidget::stop()
{
	d_data->process->stop();
}

void VipPyShellWidget::start()
{
	d_data->process->start();
}

void VipPyShellWidget::restart()
{
	d_data->process->restart();
}

void VipPyShellWidget::restartInterpreter()
{
	this->clear();
	vip_debug("restartInterpreter\n");
	qint64 interp = (qint64)VipPyInterpreter::instance()->pyIOOperation(true);
	vip_debug("End %lld\n", interp);
}

QIcon VipPyShellWidget::runningIcon() const
{
	return d_data->running_icon;
}
QIcon VipPyShellWidget::finishedIcon() const
{
	return d_data->finished_icon;
}
void VipPyShellWidget::setRunningIcon(const QIcon& icon)
{
	d_data->running_icon = icon;
}
void VipPyShellWidget::setFinishedIcon(const QIcon& icon)
{
	d_data->finished_icon = icon;
}

bool VipPyShellWidget::isIDLE() const
{
	if (auto proc = d_data->process.data())
		return proc->isWaitingForInput();
	return false;
}

void VipPyShellWidget::stopCurrentCode()
{
	VipPyInterpreter::instance()->stop(true);
}

void VipPyShellWidget::paintEvent(QPaintEvent* evt)
{
	QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	opt.init(this);
#else
	opt.initFrom(this);
#endif
	QPainter p(viewport());
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
	QTextEdit::paintEvent(evt);
}

void VipPyShellWidget::execLine(const QString& command)
{
	if (command.isEmpty())
		return;
	QTextCursor cursor = getValidCursor();
	cursor.setPosition(lastAppendPosition(), QTextCursor::MoveAnchor);
	cursor.setPosition(this->toPlainText().size(), QTextCursor::KeepAnchor);
	cursor.clearSelection();
	cursor.insertText(command + "\n");

	if (command.startsWith("!")) {
		// exec in main thread
		QString c = command.mid(1);
		auto t = VipPyLocal::evalCodeMainThread(c);
		QString to_print = t.first;
		if (to_print.isEmpty())
			to_print = t.second;
		if (!to_print.isEmpty())
			cursor.insertText(to_print + "\n");
		if (d_data->process)
			d_data->process->write("\n");
		d_data->commands.addCommand(c);
		Q_EMIT commandAdded();
		return;
	}

	if (d_data->process && d_data->process->isRunning()) {
		if (d_data->process->handleMagicCommand(command)) {
		}
		else
			d_data->process->write(command.toLatin1());
		d_data->commands.addCommand(command);
		Q_EMIT commandAdded();
	}
}

void VipPyShellWidget::addLines(const QStringList& lines)
{
	if (lines.isEmpty())
		return;
	this->moveCursor(QTextCursor::End);
	this->pasteText(lines.join("\n"));
}

void VipPyShellWidget::keyPressEvent(QKeyEvent* event)
{
	if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)) {
		QString str = this->toPlainText();
		QString text(str.mid(lastAppendPosition()) + "\n");

		// set cursor to the end
		this->moveCursor(QTextCursor::End);

		QTextEdit::keyPressEvent(event);

		if (d_data->inside_magic_command) {
			// We are inside a magic command, emit this signal
			// Q_EMIT inputWritten(text);
			// return;
		}

		if (d_data->process && d_data->process->isRunning()) {
			// special treatment for "... " interpreter, when the Python interpreter is in a nested operation
			bool is_nested = (d_data->last_output == "... " && text != "\n");
			if (is_nested)
				text.remove("\n");

			if (text.startsWith("!")) {
				// exec in main thread
				text.remove("\n");
				QString c = text.mid(1);
				auto t = VipPyLocal::evalCodeMainThread(c);
				QString to_print = t.first;
				if (to_print.isEmpty())
					to_print = t.second;
				if (!to_print.isEmpty())
					this->appendText(to_print + "\n", Qt::black);
				if (d_data->process)
					d_data->process->write("\n");
				d_data->commands.addCommand(text);
				Q_EMIT commandAdded();
				return;
			}

			d_data->inside_magic_command = true;
			if (d_data->process->handleMagicCommand(text)) {
				d_data->inside_magic_command = false;
				d_data->process->write("\n");
			}
			else {
				d_data->inside_magic_command = false;
				d_data->process->write(text.toLatin1());
			}

			text.remove("\n");
			if (!text.isEmpty() || d_data->last_output == "... ") {
				d_data->commands.addCommand(text);
				Q_EMIT commandAdded();
			}
		}
	}
	else if (event->key() == Qt::Key_Backspace) {
		QTextCursor cursor = getValidCursor();
		if (!cursor.isNull() && !(cursor.position() == cursor.anchor() && cursor.position() == lastAppendPosition())) {
			setTextCursor(cursor);
			QTextEdit::keyPressEvent(event);
		}
	}
	else if (event->key() == Qt::Key_Delete) {
		QTextCursor cursor = getValidCursor();
		if (!cursor.isNull()) {
			setTextCursor(cursor);
			QTextEdit::keyPressEvent(event);
		}
	}
	else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {

		QTextCursor cursor = textCursor();
		QString line = (event->key() == Qt::Key_Up ? d_data->commands.previous() : d_data->commands.next());
		cursor.setPosition(lastAppendPosition());
		cursor.setPosition(document()->characterCount() - 1, QTextCursor::KeepAnchor);
		cursor.beginEditBlock();
		cursor.deleteChar();
		cursor.insertText(QString(line));
		cursor.endEditBlock();
		setTextCursor(cursor);
	}
	else if (event->key() == Qt::Key_Home) {
		QTextCursor cursor = textCursor();
		if (event->modifiers() & Qt::ShiftModifier)
			cursor.setPosition(lastAppendPosition(), QTextCursor::KeepAnchor);
		else
			cursor.setPosition(lastAppendPosition());
		setTextCursor(cursor);
	}
	else if (event->key() == Qt::Key_X && (event->modifiers() & Qt::ControlModifier)) {
		QTextCursor cursor = getValidCursor();
		if (!cursor.isNull()) {
			setTextCursor(cursor);
			this->cut();
		}
	}
	else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
		this->copy();
	}
	else if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier)) {
		QTextCursor cursor = getValidCursor();
		if (!cursor.isNull()) {
			setTextCursor(cursor);
			this->paste();
		}
	}
	else if (!event->text().isEmpty() && event->text()[0].isPrint()) {
		QTextCursor cursor = getValidCursor();
		if (!cursor.isNull())
			QTextEdit::keyPressEvent(event);
	}
	else
		QTextEdit::keyPressEvent(event);
}

static QString cleanLine(const QString& line)
{
	if (line.isEmpty())
		return line;
	// remove starting "... " or ">>> " or any other invalid pattern
	if (line[0] == '>' || line[0] == '.' || line[0] == '/' || line[0] == '\\' || line[0] == '[') {
		// find the first space
		for (int i = 1; i < line.size(); ++i) {
			if (line[i] == ' ')
				return line.mid(i + 1);
		}
	}
	return line;
}

void VipPyShellWidget::pasteText(const QString& text)
{
	QTextCursor c = this->textCursor();
	int max_pos = qMax(c.position(), c.anchor());
	int min_pos = qMin(c.position(), c.anchor());
	if (min_pos < lastAppendPosition())
		return;

	QString full_text = toPlainText();

	if (max_pos != full_text.size()) {
		c.insertText(text); // standard behavior
	}
	else {
		// custom behavior
		QStringList lines = text.split("\n");
		if (lines.isEmpty())
			return;

		// Everything past the first line is executed, one line at a time, with no
		// confirmation — a single line ending in a newline included. Asked for once,
		// before anything runs.
		int executable = lines.size() - 1;
		if (executable > 0 && lines.last().isEmpty())
			--executable;
		if (executable > 0) {
			const QString question = executable == 1 ? QString("Run the pasted line in the interpreter?") : QString("Run the %1 pasted lines in the interpreter?").arg(executable);
			if (QMessageBox::Yes != vipQuestion("Run pasted code", question)) {
				c.insertText(text);
				return;
			}
		}

		// insert first line
		c.insertText(cleanLine(lines.first()));

		if (lines.size() > 1) {
			// exec each line

			for (int i = 1; i < lines.size(); ++i) {

				d_data->wait_for_more = false;

				// simulate key press
				QKeyEvent evt(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
				this->keyPressEvent(&evt);

				// wait for prompt
				// VipPyIOOperation * op = qobject_cast<VipPyIOOperation*>(process());
				while (!d_data->wait_for_more && /* !(op && op->isWaitingForInput()) &&*/ process()->isRunning())
					vipProcessEvents(nullptr, 1000);

				// insert next line
				this->moveCursor(QTextCursor::End);
				this->insertPlainText(cleanLine(lines[i]));
			}
		}
	}
}

void VipPyShellWidget::insertFromMimeData(const QMimeData* source)
{
	// reimplement paste operation
	pasteText(source->text());
}

class VipIPythonHistory::PrivateData
{
public:
	QPointer<VipIPythonShellWidget> shell;
	int maxLines = 100;
	QTimer timer;
};

VipIPythonHistory::VipIPythonHistory(VipIPythonShellWidget* shell)
  : VipTextEditor()
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->shell = shell;
	this->setReadOnly(true);
	this->setColorScheme(stdColorSchemeForExt("py"));

	d_data->timer.setSingleShot(false);
	d_data->timer.setInterval(1000);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(updateContentIfVisible()));
	d_data->timer.start();
}
VipIPythonHistory::~VipIPythonHistory()
{
	disconnect(&d_data->timer, SIGNAL(timeout()), this, SLOT(updateContentIfVisible()));
	d_data->timer.stop();
}

void VipIPythonHistory::setMaxLines(int lines)
{
	d_data->maxLines = qMin(10, lines);
}
int VipIPythonHistory::maxLines() const
{
	return d_data->maxLines;
}

void VipIPythonHistory::updateContentIfVisible()
{
	if (isVisible() && !isHidden())
		updateContent();
}

void VipIPythonHistory::updateContent()
{
	if (auto s = d_data->shell) {
		auto lines = s->historyFileLines(d_data->maxLines);
		QString text = lines.join("\n");
		if (text != toPlainText()) {
			this->clear();
			this->insertPlainText(text);
		}
	}
}

QStringList VipIPythonHistory::selectedLines() const
{
	// run selected lines
	QTextCursor c = textCursor();
	int start = c.selectionStart();
	int end = c.selectionEnd();
	c.setPosition(start);
	int firstLine = c.blockNumber();
	c.setPosition(end, QTextCursor::KeepAnchor);
	int lastLine = c.blockNumber();
	if (c.atBlockStart() && lastLine > firstLine)
		--lastLine;

	QStringList lines;

	for (int i = firstLine; i <= lastLine; ++i)
		lines += document()->findBlockByLineNumber(i).text();

	return lines;
}

void VipIPythonHistory::execSelectedLines()
{
	auto lines = selectedLines();
	if (lines.isEmpty())
		return;

	if (auto s = d_data->shell) {
		if (lines.back().trimmed().endsWith(":"))
			s->addLines(lines);
		else
			s->execLines(lines);
	}
}

void VipIPythonHistory::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return)
		execSelectedLines();
	else
		evt->setAccepted(false);
}

class VipIPythonShellWidget::PrivateData
{
public:
	VipPyObject console;
	QWidget* internal = nullptr;
	QTextEdit* textEdit = nullptr;
	QPointer<VipBaseIOOperation> process;
};

VipIPythonShellWidget::VipIPythonShellWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	VipGILLocker lock;

	VipPyObject::import("Thermavip");
	auto embedd = VipPyObject::import("Thermavip.EmbeddedIPython");
	if (!embedd) {
		VIP_LOG_ERROR(VipPyError(compute_error_t{}).toString());
		return;
	}
	QVariantMap args;
	if (vipIsDarkSkin())
		args["scheme"] = QString("dark");
	else
		args["scheme"] = QString("gray");
	args["global_ns"] = vipToVariant(VipPyObjectPtr(new VipPyObject(VipPyObject::eval("globals()"))));

	d_data->console = embedd.callMember("EmbeddedIPython", QVariantList() << QVariant(), args);
	d_data->internal = qobject_cast<QWidget*>(vipQObjectFromPython(d_data->console));
	if (!d_data->internal) {
		VIP_LOG_ERROR(VipPyError(compute_error_t{}).toString());
		return;
	}

	d_data->textEdit = d_data->internal->findChild<QTextEdit*>();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->internal);
	setLayout(lay);
}

VipIPythonShellWidget::~VipIPythonShellWidget() {}

QTextEdit* VipIPythonShellWidget::textEdit() const
{
	return d_data->textEdit;
}

struct PrintPyError
{
	~PrintPyError()
	{
		VipPyError err(compute_error_t{});
		if (err) {
			VIP_LOG_ERROR(err.toString());
		}
	}
};

QString VipIPythonShellWidget::historyFile() const
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	return d_data->console.callMember("get_history_filename").toVariant().value<QString>();
}
QStringList VipIPythonShellWidget::historyFileLines(int max_lines) const
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	return d_data->console.callMember("get_history_lines", QVariantList() << max_lines).toVariant().value<QStringList>();
}

void VipIPythonShellWidget::stopCurrentCode()
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	d_data->console.callMember("stop_current_code");
}

bool VipIPythonShellWidget::isIDLE() const
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	return d_data->console.getMember("is_idle").toVariant().toBool();
}

void VipIPythonShellWidget::restartInterpreter()
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	d_data->console.callMember("restart_session");
}
void VipIPythonShellWidget::addLines(const QStringList& lines)
{
	VipPyClearError clear;
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	d_data->console.callMember("add_input_lines", QVariantList() << vipToVariant(lines));
}
void VipIPythonShellWidget::addLine(const QString& line)
{
	addLines(QStringList() << line);
}
void VipIPythonShellWidget::execLines(const QStringList& lines)
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	for (const auto& l : lines)
		d_data->console.callMember("execute", QVariantList() << vipToVariant(l));
}
void VipIPythonShellWidget::execLine(const QString& line)
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	d_data->console.callMember("execute", QVariantList() << vipToVariant(line));
}

void VipIPythonShellWidget::appendText(const QString& str, const QColor& color)
{
	if (!textEdit())
		return;

	QVariantList args;
	args << str;
	if (color != Qt::transparent)
		args << color.name();

	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	d_data->console.callMember("append_output", args);

	if (auto err = VipPyError(compute_error_t{})) {
		VIP_LOG_ERROR(err.toString());
	}
}

void VipIPythonShellWidget::scrollToEnd()
{
	VipGILLocker lock;
	PrintPyError err;
	VipPyError::clear();
	d_data->console.callMember("scroll_to_end");
}

/// @brief Set the internal VipBaseIOOperation used to execute commands.
/// This widget does NOT take ownership of the process.
void VipIPythonShellWidget::setProcess(VipBaseIOOperation* proc)
{
	if (d_data->process) {
		disconnect(d_data->process, SIGNAL(readyReadStandardOutput()), this, SLOT(newOutput()));
		disconnect(d_data->process, SIGNAL(readyReadStandardError()), this, SLOT(newError()));
	}

	d_data->process = proc;
	if (d_data->process) {

		proc->disconnect(SIGNAL(readyReadStandardOutput()));
		proc->disconnect(SIGNAL(readyReadStandardError()));

		newOutput();
		newError();
		connect(d_data->process, SIGNAL(readyReadStandardOutput()), this, SLOT(newOutput()));
		connect(d_data->process, SIGNAL(readyReadStandardError()), this, SLOT(newError()));
	}
}
VipBaseIOOperation* VipIPythonShellWidget::process() const
{
	return d_data->process.data();
}

void VipIPythonShellWidget::newOutput()
{
	QByteArray bytes = d_data->process->readAllStandardOutput();
	QTextCodec* codec = QTextCodec::codecForName("IBM850");
	QString str = codec->toUnicode(bytes);
	this->appendText(str, vipWidgetTextBrush(this).color());

	scrollToEnd();
}

void VipIPythonShellWidget::newError()
{
	QByteArray bytes = d_data->process->readAllStandardError();
	QTextCodec* codec = QTextCodec::codecForName("IBM850");
	QString str = codec->toUnicode(bytes);
	appendText(str, vipDefaultTextErrorColor(this)); // Qt::red);

	scrollToEnd();
}

class VipPyInterpreterToolWidget::PrivateData
{
public:
	QPointer<VipPyShellWidget> stdShell;
	QPointer<VipIPythonShellWidget> ipythonShell;
	VipBasePythonShell* shell = nullptr;

	VipTextEditor* history;
	QSplitter* splitter;

	QToolButton* closeHistory;
	QAction* showHistory;
	QAction* restart;
};
VipPyInterpreterToolWidget::VipPyInterpreterToolWidget(VipMainWindow* win)
  : VipToolWidget(win)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->ipythonShell = new VipIPythonShellWidget();
	if (!d_data->ipythonShell->textEdit()) {
		delete d_data->ipythonShell.data();
		d_data->ipythonShell = nullptr;
		d_data->stdShell = new VipPyShellWidget();
	}
	d_data->shell = d_data->ipythonShell ? static_cast<VipBasePythonShell*>(d_data->ipythonShell.data()) : static_cast<VipBasePythonShell*>(d_data->stdShell.data());
	d_data->shell->setProcess(VipPyInterpreter::instance());

	if (d_data->stdShell) {

		QObject::connect(VipPyInterpreter::instance(), SIGNAL(started()), VipPyInterpreter::instance(), SLOT(startInteractiveInterpreter()));
		VipPyInterpreter::instance()->startInteractiveInterpreter();
		d_data->history = new VipTextEditor();
		d_data->history->openFile(vipGetPythonHistoryFile());
		d_data->history->setReadOnly(true);
		// go to the end of file
		d_data->history->moveCursor(QTextCursor::End);
		d_data->history->ensureCursorVisible();
		connect(d_data->stdShell, SIGNAL(commandAdded()), d_data->history, SLOT(reload()));
	}
	else {
		d_data->history = new VipIPythonHistory(d_data->ipythonShell);
	}

	d_data->splitter = new QSplitter(Qt::Horizontal);
	d_data->splitter->addWidget(shellWidget());
	d_data->splitter->addWidget(d_data->history);

	d_data->closeHistory = new QToolButton(d_data->history);
	d_data->closeHistory->setIcon(vipIcon("close.png"));
	d_data->closeHistory->setToolTip("Hide history file");
	d_data->closeHistory->setAutoRaise(false);
	d_data->closeHistory->setAutoFillBackground(false);
	connect(d_data->closeHistory, SIGNAL(clicked(bool)), this, SLOT(hideHistory()));

	// if (d_data->history->verticalScrollBar()->isVisible())
	//	d_data->history->verticalScrollBar()->setValue(d_data->history->verticalScrollBar()->maximum());
	d_data->history->hide();
	d_data->history->installEventFilter(this);

	d_data->restart = this->titleBarWidget()->toolBar()->addAction(vipIcon("restart.png"), "Restart interpreter");
	d_data->showHistory = this->titleBarWidget()->toolBar()->addAction(vipIcon("visible.png"), "Show/hide history file");
	d_data->showHistory->setCheckable(true);

	connect(d_data->restart, SIGNAL(triggered(bool)), this, SLOT(restartInterpreter()));
	connect(d_data->showHistory, SIGNAL(triggered(bool)), this, SLOT(setHistoryVisible(bool)));

	this->setWidget(d_data->splitter);
	this->setWindowTitle("Python internal console");
	this->setObjectName("Python internal console");

	VipPyInterpreter::instance()->setMainInterpreter(this);
}

VipPyInterpreterToolWidget::~VipPyInterpreterToolWidget()
{
	d_data->history->removeEventFilter(this);
}

QWidget* VipPyInterpreterToolWidget::shellWidget() const
{
	return d_data->ipythonShell ? static_cast<QWidget*>(d_data->ipythonShell.data()) : static_cast<QWidget*>(d_data->stdShell.data());
}
QTextEdit* VipPyInterpreterToolWidget::shellEditor() const
{
	return d_data->shell->textEdit();
}
VipBasePythonShell* VipPyInterpreterToolWidget::shell() const
{
	return d_data->shell;
}

VipTextEditor* VipPyInterpreterToolWidget::historyFile() const
{
	return d_data->history;
}
QSplitter* VipPyInterpreterToolWidget::splitter() const
{
	return d_data->splitter;
}
bool VipPyInterpreterToolWidget::historyVisible() const
{
	return d_data->history->isVisible();
}

QStringList VipPyInterpreterToolWidget::historySelectedLines() const
{
	// run selected lines
	QTextCursor c = d_data->history->textCursor();
	int start = c.selectionStart();
	int end = c.selectionEnd();
	c.setPosition(start);
	int firstLine = c.blockNumber();
	c.setPosition(end, QTextCursor::KeepAnchor);
	int lastLine = c.blockNumber();
	if (c.atBlockStart() && lastLine > firstLine)
		--lastLine;

	QStringList lines;

	for (int i = firstLine; i <= lastLine; ++i)
		lines += d_data->history->document()->findBlockByLineNumber(i).text();

	return lines;
}

void VipPyInterpreterToolWidget::setHistoryVisible(bool vis)
{
	d_data->showHistory->blockSignals(true);
	d_data->showHistory->setChecked(vis);
	d_data->showHistory->blockSignals(false);
	d_data->history->setVisible(vis);
}
void VipPyInterpreterToolWidget::hideHistory()
{
	setHistoryVisible(false);
}
void VipPyInterpreterToolWidget::restartInterpreter()
{
	d_data->shell->restartInterpreter();
}

bool VipPyInterpreterToolWidget::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::Resize || evt->type() == QEvent::Show) {
		int additional = d_data->history->verticalScrollBar()->isVisible() ? d_data->history->verticalScrollBar()->width() : 0;
		d_data->closeHistory->move(d_data->history->width() - d_data->closeHistory->width() - additional, 0);
	}
	else if (evt->type() == QEvent::KeyPress) {
		QKeyEvent* key = static_cast<QKeyEvent*>(evt);
		if (key->key() == Qt::Key_Return || key->key() == Qt::Key_F5) {

			d_data->shell->addLines(historySelectedLines());
			shellWidget()->raise();
			shellWidget()->setFocus();
			return true;
		}
	}
	return false;
}

VipPyInterpreterToolWidget* vipPyGetPythonInterpreter()
{
	static VipPyInterpreterToolWidget* python = new VipPyInterpreterToolWidget(vipGetMainWindow());
	return python;
}
