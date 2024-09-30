#include <QBoxLayout>
#include <QMessageBox>
#include <QTabBar>

#include "VipStandardWidgets.h"
#include "VipTextHighlighter.h"
#include "VipEditorFilter.h"
#include "VipTabEditor.h"
#include "VipCore.h"

class VipTextSearchBar::PrivateData
{
public:
	QPointer<QTextDocument> document;
	QPointer<QTextEdit> edit1;
	QPointer<QPlainTextEdit> edit2;
	int foundStart, foundEnd, line;
	bool restartFromCursor;
	QTextCharFormat format;

	QAction *prev, *next, *reg, *exact, *case_sens, *close;
	QLineEdit* search;
};

VipTextSearchBar::VipTextSearchBar(QWidget* parent)
  : QToolBar(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->foundEnd = d_data->foundStart = d_data->line = 0;
	d_data->restartFromCursor = true;
	this->addSeparator();
	d_data->close = this->addAction(vipIcon("close.png"), "Close search panel");
	d_data->search = new QLineEdit();
	d_data->search->setPlaceholderText("Search filter");
	this->addWidget(d_data->search);
	d_data->prev = this->addAction(vipIcon("search_prev.png"), "Search previous match");
	d_data->next = this->addAction(vipIcon("search_next.png"), "Search next match");
	d_data->reg = this->addAction(vipIcon("search_reg.png"), "Use regular expression");
	d_data->case_sens = this->addAction(vipIcon("search_case_sensitive.png"), "Case sensitive");
	d_data->exact = this->addAction(vipIcon("search_word.png"), "Whole word");

	d_data->reg->setCheckable(true);
	d_data->case_sens->setCheckable(true);
	d_data->exact->setCheckable(true);

	this->setIconSize(QSize(18, 18));

	connect(d_data->close, SIGNAL(triggered(bool)), this, SLOT(close(bool)));
	connect(d_data->prev, SIGNAL(triggered(bool)), this, SLOT(searchPrev()));
	connect(d_data->next, SIGNAL(triggered(bool)), this, SLOT(searchNext()));
	connect(d_data->reg, SIGNAL(triggered(bool)), this, SLOT(setRegExp(bool)));
	connect(d_data->case_sens, SIGNAL(triggered(bool)), this, SLOT(setCaseSensitive(bool)));
	connect(d_data->exact, SIGNAL(triggered(bool)), this, SLOT(setExactMatch(bool)));
}

VipTextSearchBar::~VipTextSearchBar() {}

void VipTextSearchBar::removePreviousFormat()
{
	if (!d_data->document)
		return;
	if (d_data->foundStart >= d_data->foundEnd)
		return;
	/*QTextBlock b = d_data->document->findBlockByLineNumber(d_data->line);
	if (b.isValid()) {
		QVector<QTextLayout::FormatRange> ranges = b.layout()->formats();
		int len = d_data->foundEnd - d_data->foundStart;
		for (int i = 0; i < ranges.size(); ++i) {
			if (ranges[i].start == d_data->foundStart && ranges[i].length == len) {
				ranges.removeAt(i);
				--i;
			}
		}
		b.layout()->setFormats(ranges);
		d_data->document->markContentsDirty(b.position(), b.length());
	}*/

	if (!d_data->edit1 && !d_data->edit2)
		return;
	QList<QTextEdit::ExtraSelection> selections = d_data->edit1 ? d_data->edit1->extraSelections() : d_data->edit2->extraSelections();
	// remove previous
	for (int i = 0; i < selections.size(); ++i)
		if (selections[i].format.property(QTextFormat::UserProperty + 2).toBool()) {
			selections.removeAt(i);
			--i;
		}
	if (d_data->edit1)
		d_data->edit1->setExtraSelections(selections);
	else
		d_data->edit2->setExtraSelections(selections);
}

void VipTextSearchBar::format(const QTextBlock& b, int start, int end)
{
	/*removePreviousFormat();
	if (b.isValid()) {
		QVector<QTextLayout::FormatRange> ranges = b.layout()->formats();
		QTextLayout::FormatRange f;
		f.format = b.charFormat();
		f.start = start;
		f.length = end - start;
		f.format.setBackground(QBrush(Qt::yellow));
		ranges.append(f);
		b.layout()->setFormats(ranges);
		d_data->document->markContentsDirty(b.position(), b.length());
	}*/

	if (!d_data->edit1 && !d_data->edit2)
		return;
	removePreviousFormat();
	QList<QTextEdit::ExtraSelection> selections = d_data->edit1 ? d_data->edit1->extraSelections() : d_data->edit2->extraSelections();

	QTextEdit::ExtraSelection selection;
	selection.format.setBackground(Qt::yellow);
	selection.format.setProperty(QTextFormat::UserProperty + 2, true);
	selection.cursor = QTextCursor(d_data->document);
	selection.cursor.setPosition(b.position() + start);
	selection.cursor.setPosition(b.position() + end, QTextCursor::KeepAnchor);
	selections.append(selection);
	if (d_data->edit1)
		d_data->edit1->setExtraSelections(selections);
	else
		d_data->edit2->setExtraSelections(selections);
}

bool VipTextSearchBar::exactMatch() const
{
	return d_data->exact->isChecked();
}
bool VipTextSearchBar::regExp() const
{
	return d_data->reg->isChecked();
}
bool VipTextSearchBar::caseSensitive() const
{
	return d_data->case_sens->isChecked();
}

void VipTextSearchBar::setEditor(QTextEdit* ed)
{
	if (d_data->edit1)
		disconnect(d_data->edit1, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
	d_data->edit1 = ed;
	d_data->edit2 = nullptr;
	if (ed) {
		connect(d_data->edit1, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
		setDocument(ed->document());
	}
}
void VipTextSearchBar::setEditor(QPlainTextEdit* ed)
{
	if (d_data->edit2)
		disconnect(d_data->edit2, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
	d_data->edit1 = nullptr;
	d_data->edit2 = ed;
	if (ed) {
		connect(d_data->edit2, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
		setDocument(ed->document());
	}
}

void VipTextSearchBar::setDocument(QTextDocument* ed)
{
	if (d_data->document) {
		removePreviousFormat();
		d_data->foundStart = d_data->foundEnd = d_data->line = 0;
		disconnect(d_data->document, SIGNAL(contentsChanged()), this, SLOT(restartFromCursor()));
	}
	d_data->document = ed;
	if (ed) {
		connect(d_data->document, SIGNAL(contentsChanged()), this, SLOT(restartFromCursor()));
	}
}
QTextDocument* VipTextSearchBar::document() const
{
	return d_data->document;
}

QLineEdit* VipTextSearchBar::search() const
{
	return d_data->search;
}

QPair<int, int> VipTextSearchBar::lastFound(int* line) const
{
	if (*line)
		*line = d_data->line;
	return QPair<int, int>(d_data->foundStart, d_data->foundEnd);
}

void VipTextSearchBar::search(bool forward)
{
	QString pattern = d_data->search->text();
	Qt::CaseSensitivity case_sens = d_data->case_sens->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
	QRegExp exp;
	if (regExp())
		exp = QRegExp(pattern, case_sens, QRegExp::Wildcard);
	else
		exp = QRegExp(pattern, case_sens, QRegExp::FixedString);

	QTextCursor c(d_data->document);
	if (d_data->foundStart == d_data->foundEnd || d_data->restartFromCursor) {
		if (d_data->edit1)
			c = d_data->edit1->textCursor();
		else if (d_data->edit2)
			c = d_data->edit2->textCursor();

		d_data->restartFromCursor = false;
	}
	else {
		QTextBlock b = d_data->document->findBlockByLineNumber(d_data->line);
		if (forward) {
			c.setPosition(d_data->foundEnd + b.position());
			c.setPosition(d_data->foundEnd + b.position(), QTextCursor::KeepAnchor);
		}
		else {
			c.setPosition(d_data->foundStart + b.position());
			c.setPosition(d_data->foundStart + b.position(), QTextCursor::KeepAnchor);
		}
	}

	QTextDocument::FindFlags flags ;
	if (d_data->case_sens->isChecked())
		flags |= QTextDocument::FindCaseSensitively;
	if (d_data->exact->isChecked())
		flags |= QTextDocument::FindWholeWords;
	if (!forward)
		flags |= QTextDocument::FindBackward;

	QTextCursor found = d_data->document->find(exp, c, flags);
	if (found.anchor() == found.position()) {
		// restart from beginning
		if (forward) {
			c = QTextCursor(d_data->document);
			c.setPosition(0);
			c.setPosition(0, QTextCursor::KeepAnchor);
			found = d_data->document->find(exp, c, flags);
		}
		else {
			c = QTextCursor(d_data->document);
			c.setPosition(d_data->document->lastBlock().position() + d_data->document->lastBlock().length());
			c.setPosition(d_data->document->lastBlock().position() + d_data->document->lastBlock().length(), QTextCursor::KeepAnchor);
			found = d_data->document->find(exp, c, flags);
		}
	}
	if (found.anchor() != found.position()) {
		QTextBlock b = d_data->document->findBlock(found.position());
		if (b.isValid()) {
			int bpos = b.position();
			// format(b, found.anchor() - bpos, found.position() - bpos);
			d_data->foundStart = found.anchor() - bpos;
			d_data->foundEnd = found.position() - bpos;
			d_data->line = b.firstLineNumber();

			/*QTextCharFormat f = found.blockCharFormat();
			f.setBackground(Qt::yellow);
			found.setCharFormat(f);*/

			if (d_data->edit1) {
				d_data->edit1->setTextCursor(found);
				d_data->edit1->ensureCursorVisible();
			}
			else if (d_data->edit2) {
				d_data->edit2->setTextCursor(found);
				d_data->edit2->ensureCursorVisible();
			}
		}
	}
}

void VipTextSearchBar::searchNext()
{
	search(true);
}

void VipTextSearchBar::searchPrev()
{
	search(false);
}
void VipTextSearchBar::setExactMatch(bool enable)
{
	d_data->exact->blockSignals(true);
	d_data->exact->setChecked(enable);
	d_data->exact->blockSignals(false);
}
void VipTextSearchBar::setRegExp(bool enable)
{
	d_data->reg->blockSignals(true);
	d_data->reg->setChecked(enable);
	d_data->reg->blockSignals(false);
}
void VipTextSearchBar::setCaseSensitive(bool enable)
{
	d_data->case_sens->blockSignals(true);
	d_data->case_sens->setChecked(enable);
	d_data->case_sens->blockSignals(false);
}
void VipTextSearchBar::close(bool)
{
	Q_EMIT closeRequested();
}

void VipTextSearchBar::restartFromCursor()
{
	d_data->restartFromCursor = true;
}

void VipTextSearchBar::showEvent(QShowEvent*) {}
void VipTextSearchBar::hideEvent(QHideEvent*)
{
	removePreviousFormat();
	d_data->foundStart = d_data->foundEnd = d_data->line = 0;
}

VipDefaultTextBar::VipDefaultTextBar(QWidget* parent)
  : QToolBar(parent)
{
	newfile = new QWidgetAction(this);
	newfile->setIcon(vipIcon("new.png"));
	newfile->setText(QString("New file"));
	addAction(newfile);

	open = new QWidgetAction(this);
	open->setIcon(vipIcon("open_dir.png"));
	open->setText(QString("Open file"));
	addAction(open);

	save = new QWidgetAction(this);
	save->setIcon(vipIcon("save.png"));
	save->setText(QString("Save file"));
	addAction(save);

	saveAs = new QWidgetAction(this);
	saveAs->setIcon(vipIcon("save_as.png"));
	saveAs->setText(QString("Save file as..."));
	addAction(saveAs);

	saveAll = new QWidgetAction(this);
	saveAll->setIcon(vipIcon("save_all.png"));
	saveAll->setText(QString("Save all"));
	addAction(saveAll);

	addSeparator();

	unindent = new QWidgetAction(this);
	unindent->setIcon(vipIcon("unindent.png"));
	unindent->setText(QString("Decrease indent"));
	addAction(unindent);

	indent = new QWidgetAction(this);
	indent->setIcon(vipIcon("indent.png"));
	indent->setText(QString("Increase indent"));
	addAction(indent);

	addSeparator();

	comment = new QWidgetAction(this);
	comment->setIcon(vipIcon("comment.png"));
	comment->setText(QString("Comment selection"));
	addAction(comment);

	uncomment = new QWidgetAction(this);
	uncomment->setIcon(vipIcon("uncomment.png"));
	uncomment->setText(QString("Uncomment selection"));
	addAction(uncomment);

	setIconSize(QSize(18, 18));
}

class VipTabEditor::PrivateData
{
public:
	VipTextEditorTabWidget tab;
	VipDefaultTextBar bar;
	VipTextSearchBar* search;
	QAction* searchAction;
	QString default_code;
	bool unique;
	QMap<int, int> ids;
	QString defaultColorSchemeType;
	QString defaultSaveDirectory;
};

VipTabEditor::VipTabEditor(Qt::Orientation tool_bar_orientation, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->tab.setTabsClosable(true);
	d_data->unique = false;

	d_data->search = new VipTextSearchBar();
	// d_data->searchAction = d_data->bar.addWidget(d_data->search);

	QBoxLayout* lay;
	if (tool_bar_orientation == Qt::Horizontal) {
		lay = new QVBoxLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		lay->addWidget(&d_data->bar);
		lay->addWidget(&d_data->tab);
		lay->setSpacing(0);
	}
	else {
		d_data->bar.setOrientation(Qt::Vertical);
		lay = new QHBoxLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		lay->addWidget(&d_data->bar);
		lay->addWidget(&d_data->tab);
		lay->setSpacing(0);
	}

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->setSpacing(0);
	vlay->addLayout(lay);
	vlay->addWidget(d_data->search);
	setLayout(vlay);

	// d_data->tab.setStyleSheet("QTabWidget {background: transparent;}");

	connect(d_data->bar.newfile, SIGNAL(triggered(bool)), this, SLOT(newFile()));
	connect(d_data->bar.open, SIGNAL(triggered(bool)), this, SLOT(open()));
	connect(d_data->bar.save, SIGNAL(triggered(bool)), this, SLOT(save()));
	connect(d_data->bar.saveAs, SIGNAL(triggered(bool)), this, SLOT(saveAs()));
	connect(d_data->bar.saveAll, SIGNAL(triggered(bool)), this, SLOT(saveAll()));

	connect(d_data->bar.unindent, SIGNAL(triggered(bool)), this, SLOT(unindent()));
	connect(d_data->bar.indent, SIGNAL(triggered(bool)), this, SLOT(indent()));
	connect(d_data->bar.comment, SIGNAL(triggered(bool)), this, SLOT(comment()));
	connect(d_data->bar.uncomment, SIGNAL(triggered(bool)), this, SLOT(uncomment()));

	connect(&d_data->tab, SIGNAL(tabCloseRequested(int)), this, SLOT(aboutToClose(int)));
	connect(&d_data->tab, SIGNAL(currentChanged(int)), this, SLOT(currentChanged(int)));

	d_data->search->hide();
	connect(d_data->search, SIGNAL(closeRequested()), this, SLOT(closeSearch()));
}

VipTabEditor ::~VipTabEditor() {}

VipTextEditorTabWidget* VipTabEditor::tabWidget() const
{
	return const_cast<VipTextEditorTabWidget*>(&d_data->tab);
}

VipDefaultTextBar* VipTabEditor::tabBar() const
{
	return const_cast<VipDefaultTextBar*>(&d_data->bar);
}
VipTextEditor* VipTabEditor::currentEditor() const
{
	return static_cast<VipTextEditor*>(d_data->tab.currentWidget());
}

VipTextEditor* VipTabEditor::editor(int pos) const
{
	return static_cast<VipTextEditor*>(d_data->tab.widget(pos));
}

int VipTabEditor::count() const
{
	return d_data->tab.count();
}

int VipTabEditor::currentIndex() const
{
	return d_data->tab.currentIndex();
}

void VipTabEditor::setUniqueFile(bool unique)
{
	if (d_data->unique == unique)
		return;
	d_data->unique = unique;
	if (d_data->unique) {
		// close all editors
		for (int i = 0; i < count(); ++i)
			delete editor(i);
		// create a new one
		createEditor();
		// hide actions
		d_data->tab.tabBar()->hide();
		d_data->bar.newfile->setVisible(false);
		d_data->bar.saveAll->setVisible(false);
	}
	else {
		d_data->tab.tabBar()->setVisible(true);
		d_data->bar.newfile->setVisible(false);
		d_data->bar.saveAll->setVisible(false);
	}
}
bool VipTabEditor::uniqueFile() const
{
	return d_data->unique;
}

void VipTabEditor::setEditorFilename(VipTextEditor* editor, const QString& filename)
{
	QString suffix = QFileInfo(filename).suffix();

	const VipTextHighlighter* h = nullptr;
	if (suffix.isEmpty())
		h = VipTextEditor::stdColorSchemeForType(defaultColorSchemeType());
	else
		h = VipTextEditor::stdColorSchemeForExt(suffix);
	if (!h)
		h = VipTextEditor::stdColorSchemeForExt("txt");
	if (!h)
		return;

	editor->setColorScheme(h);
}

int VipTabEditor::createEditor(const QString& filename)
{
	// find filename in all opened files, update it
	if (!filename.isEmpty()) {
		// try to open in an existing editor with the same filename
		for (int i = 0; i < count(); ++i) {
			if (editor(i)->fileInfo() == QFileInfo(filename)) {
				editor(i)->openFile(filename);
				d_data->tab.setCurrentIndex(i);
				return i;
			}
		}

		// try to open in an empty editor
		for (int i = 0; i < count(); ++i) {
			if (editor(i)->isEmpty()) {
				if (editor(i)->openFile(filename))
					setEditorFilename(editor(i), filename);
				d_data->tab.setCurrentIndex(i);
				return i;
			}
		}
	}

	// create the new editor
	int index = -1;
	if (filename.isEmpty()) {
		int id = nextId();
		QString fname = "New" + QString::number(id);
		index = d_data->tab.addTab(new VipTextEditor(), fname);
		editor(index)->openFile(fname);
		editor(index)->setPlainText(d_data->default_code);
		editor(index)->setProperty("new_id", id);
		editor(index)->setProperty("filename", fname);
		d_data->ids.insert(id, id);
	}
	else {
		index = d_data->tab.addTab(new VipTextEditor(), QFileInfo(filename).fileName());
		d_data->tab.setTabToolTip(index, QFileInfo(filename).canonicalFilePath());
		editor(index)->openFile(filename);
	}

	connect(editor(index), SIGNAL(saveTriggered()), this, SLOT(save()));
	connect(editor(index), SIGNAL(searchTriggered()), this, SLOT(showSearchAndFocus()));

	d_data->tab.setCurrentIndex(index);

	setEditorFilename(editor(index), filename);

	connect(currentEditor()->document(), SIGNAL(modificationChanged(bool)), this, SLOT(modificationChanged(bool)));
	setHeaderBarVisibility();
	Q_EMIT modified(false);
	return index;
}

void VipTabEditor::setDefaultText(const QString& code)
{
	d_data->default_code = code;
}
QString VipTabEditor::defaultText() const
{
	return d_data->default_code;
}

void VipTabEditor::setCurrentIndex(int index)
{
	d_data->tab.setCurrentIndex(index);
}

QList<VipTextEditor*> VipTabEditor::open()
{
	QStringList filenames = VipFileDialog::getOpenFileNames(nullptr, "Open files", VipTextEditor::supportedFilters());
	QList<VipTextEditor*> res;
	if (!filenames.isEmpty()) {
		for (const QString& filename : filenames) {

			if (d_data->unique) {
				// just update the current editor
				if (currentEditor()) {
					QFile in(filename);
					if (in.open(QFile::ReadOnly | QFile::Text))
						currentEditor()->setPlainText(in.readAll());
				}
				res.push_back(currentEditor());
			}
			else {
				int index = createEditor(filename);
				if (index >= 0)
					res.push_back(editor(index));
			}
		}
	}

	return res;
}

VipTextEditor* VipTabEditor::openFile(const QString& filename)
{
	if (d_data->unique) {
		// just update the current editor
		if (currentEditor()) {
			QFile in(filename);
			if (in.open(QFile::ReadOnly | QFile::Text))
				currentEditor()->setPlainText(in.readAll());
		}
		return currentEditor();
	}

	int index = createEditor(filename);
	if (index >= 0)
		return editor(index);
	else
		return nullptr;
}

bool VipTabEditor::save(VipTextEditor* editor)
{
	if (editor) {
		if (editor->fileInfo().exists()) {
			return editor->saveToFile(editor->fileInfo().absoluteFilePath());
		}
		else {
			QString filters = VipTextEditor::supportedFilters();
			QString filename = VipFileDialog::getSaveFileName2(nullptr, defaultSaveDirectory(), "Save file", filters);
			if (!filename.isEmpty()) {
				if (editor->saveToFile(filename)) {
					setEditorFilename(editor, filename);
					return true;
				}
			}
		}
	}

	return false;
}

void VipTabEditor::save()
{
	save(currentEditor());
}

void VipTabEditor::saveAs()
{
	if (currentEditor()) {
		QString path = currentEditor()->fileInfo().exists() ? currentEditor()->fileInfo().canonicalFilePath() : defaultSaveDirectory();

		QString filename = VipFileDialog::getSaveFileName2(nullptr, path, "Save file", VipTextEditor::supportedFilters());
		if (!filename.isEmpty()) {
			if (currentEditor()->saveToFile(filename))
				setEditorFilename(currentEditor(), filename);
		}
	}
}

void VipTabEditor::saveAll()
{
	for (int i = 0; i < count(); ++i) {
		save(editor(i));
	}
}

VipTextEditor* VipTabEditor::newFile()
{
	int index = createEditor();
	if (index >= 0)
		return editor(index);
	else
		return nullptr;
}

void VipTabEditor::comment()
{
	if (auto* editor = currentEditor()) {
		if (auto* f = editor->editorFilter())
			f->commentSelection();
	}
}

void VipTabEditor::uncomment()
{
	if (auto* editor = currentEditor()) {
		if (auto* f = editor->editorFilter())
			f->uncommentSelection();
	}
}

void VipTabEditor::indent()
{
	if (auto* editor = currentEditor()) {
		if (auto* f = editor->editorFilter())
			f->indentSelection();
	}
}

void VipTabEditor::unindent()
{
	if (auto* editor = currentEditor()) {
		if (auto* f = editor->editorFilter())
			f->unindentSelection();
	}
}

void VipTabEditor::closeSearch()
{
	d_data->search->setVisible(false);
}
void VipTabEditor::showSearch()
{
	d_data->search->setVisible(true);
}
void VipTabEditor::showSearchAndFocus()
{
	d_data->search->setVisible(true);
	d_data->search->search()->setFocus(Qt::MouseFocusReason);
	d_data->search->search()->setText(this->currentEditor()->textCursor().selectedText());
}
void VipTabEditor::setSearchVisible(bool vis)
{
	d_data->search->setVisible(vis);
}

QByteArray VipTabEditor::saveState() const
{
	QByteArray res;
	{
		QDataStream str(&res, QIODevice::WriteOnly);
		str.setByteOrder(QDataStream::LittleEndian);
		str << (quint32)count();
		str << (quint32)d_data->tab.currentIndex();
		for (int i = 0; i < count(); ++i) {
			QByteArray name = editor(i)->fileInfo().exists() ? editor(i)->fileInfo().canonicalFilePath().toLatin1() : filename(editor(i)).toLatin1();

			QByteArray code = editor(i)->fileInfo().exists() ? QByteArray() : editor(i)->toPlainText().toLatin1();

			// write name and code with their length
			str << name << code;
		}
	}
	return res;
}
void VipTabEditor::restoreState(const QByteArray& state)
{
	d_data->tab.clear();
	QDataStream str(state);
	str.setByteOrder(QDataStream::LittleEndian);
	try {
		quint32 count = 0, current = 0;
		str >> count >> current;
		if (count > 100)
			count = 0;
		for (int i = 0; i < (int)count; ++i) {
			QByteArray name, code;
			str >> name >> code;

			if (name.startsWith("New")) {
				int index = createEditor("");
				editor(index)->setPlainText(code);
			}
			else {
				/*int index =*/createEditor(name);
			}
		}
		if ((int)current < d_data->tab.count())
			d_data->tab.setCurrentIndex(current);
	}
	catch (...) {
	}
}

void VipTabEditor::setHeaderBarVisibility()
{
	if (d_data->tab.count() > 1)
		d_data->tab.tabBar()->show();
	else
		d_data->tab.tabBar()->hide();
}

void VipTabEditor::aboutToClose(int index)
{
	bool ask_for_save = false;
	int id = editor(index)->property("new_id").toInt();

	if (!(id > 0 && editor(index)->toPlainText().isEmpty())) {
		ask_for_save = editor(index)->document()->isModified();
	}

	if (ask_for_save) {
		if (QMessageBox::question(this, "Save before closing", "Do you want to save editor's content before closing it?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
			save(editor(index));
		}
	}

	if (id > 0)
		d_data->ids.remove(id);

	d_data->tab.removeTab(index);
	setHeaderBarVisibility();
}

int VipTabEditor::nextId() const
{
	int i = 1;
	for (QMap<int, int>::const_iterator it = d_data->ids.begin(); it != d_data->ids.end(); ++it, ++i)
		if (i != it.key())
			return i;
	return d_data->ids.size() + 1;
}

QString VipTabEditor::filename(VipTextEditor* ed) const
{
	QString name = currentEditor()->fileInfo().fileName();
	if (name.isEmpty())
		name = ed->property("filename").toString();
	return name;
}
QString VipTabEditor::canonicalFilename(VipTextEditor* ed) const
{
	QString name = currentEditor()->fileInfo().canonicalFilePath();
	if (name.isEmpty())
		name = ed->property("filename").toString();
	return name;
}

QString VipTabEditor::defaultColorSchemeType() const
{
	return d_data->defaultColorSchemeType;
}

void VipTabEditor::setDefaultColorSchemeType(const QString& type)
{
	d_data->defaultColorSchemeType = type;
}

void VipTabEditor::setDefaultSaveDirectory(const QString& dir)
{
	d_data->defaultSaveDirectory = dir;
}

QString VipTabEditor::defaultSaveDirectory() const
{
	return d_data->defaultSaveDirectory;
}

void VipTabEditor::currentChanged(int)
{
	if (currentEditor())
		d_data->search->setEditor(currentEditor());
	else
		d_data->search->setEditor((QTextEdit*)nullptr);
}

void VipTabEditor::modificationChanged(bool modify)
{
	// vip_debug("fname: %s\n", filename(currentEditor()).toLatin1().data());
	if (modify) {
		d_data->tab.setTabText(d_data->tab.currentIndex(), "*" + filename(currentEditor()));
		d_data->tab.setTabToolTip(d_data->tab.currentIndex(), canonicalFilename(currentEditor()));
	}
	else {
		d_data->tab.setTabText(d_data->tab.currentIndex(), filename(currentEditor()));
		d_data->tab.setTabToolTip(d_data->tab.currentIndex(), canonicalFilename(currentEditor()));
	}
	Q_EMIT modified(modify);
}
