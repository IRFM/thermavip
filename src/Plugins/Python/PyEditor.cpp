#include <QBoxLayout>
#include <QMessageBox>
#include <QTabBar>

#include "VipStandardWidgets.h"
#include "PyHighlighter.h"
#include "PyEditorFilter.h"
#include "PyEditor.h"
#include "PyOperation.h"
#include "VipCore.h"



class PySearch::PrivateData
{
public:
	QPointer<QTextDocument> document;
	QPointer<QTextEdit> edit1;
	QPointer<QPlainTextEdit> edit2;
	int foundStart, foundEnd, line;
	bool restartFromCursor;
	QTextCharFormat format;

	QAction * prev, *next, *reg, *exact, *case_sens, *close;
	QLineEdit * search;
};

PySearch::PySearch(QWidget * parent)
	:QToolBar(parent)
{
	m_data = new PrivateData();
	m_data->foundEnd = m_data->foundStart = m_data->line = 0;
	m_data->restartFromCursor = true;
	this->addSeparator();
	m_data->close = this->addAction(vipIcon("close.png"), "Close search panel");
	m_data->search = new QLineEdit();
	m_data->search->setPlaceholderText("Search filter");
	this->addWidget(m_data->search);
	m_data->prev = this->addAction(vipIcon("search_prev.png"), "Search previous match");
	m_data->next = this->addAction(vipIcon("search_next.png"), "Search next match");
	m_data->reg = this->addAction(vipIcon("search_reg.png"), "Use regular expression");
	m_data->case_sens = this->addAction(vipIcon("search_case_sensitive.png"), "Case sensitive");
	m_data->exact = this->addAction(vipIcon("search_word.png"), "Whole word");

	m_data->reg->setCheckable(true);
	m_data->case_sens->setCheckable(true);
	m_data->exact->setCheckable(true);

	this->setIconSize(QSize(18, 18));

	connect(m_data->close, SIGNAL(triggered(bool)), this, SLOT(close(bool)));
	connect(m_data->prev, SIGNAL(triggered(bool)), this, SLOT(searchPrev()));
	connect(m_data->next, SIGNAL(triggered(bool)), this, SLOT(searchNext()));
	connect(m_data->reg, SIGNAL(triggered(bool)), this, SLOT(setRegExp(bool)));
	connect(m_data->case_sens, SIGNAL(triggered(bool)), this, SLOT(setCaseSensitive(bool)));
	connect(m_data->exact, SIGNAL(triggered(bool)), this, SLOT(setExactMatch(bool)));
}

PySearch::~PySearch()
{
	delete m_data;
}

void PySearch::removePreviousFormat()
{
	if (!m_data->document)
		return;
	if (m_data->foundStart >= m_data->foundEnd) 
		return;
	/*QTextBlock b = m_data->document->findBlockByLineNumber(m_data->line);
	if (b.isValid()) {
		QVector<QTextLayout::FormatRange> ranges = b.layout()->formats();
		int len = m_data->foundEnd - m_data->foundStart;
		for (int i = 0; i < ranges.size(); ++i) {
			if (ranges[i].start == m_data->foundStart && ranges[i].length == len) {
				ranges.removeAt(i);
				--i;
			}
		}
		b.layout()->setFormats(ranges);
		m_data->document->markContentsDirty(b.position(), b.length());
	}*/

	if (!m_data->edit1 && !m_data->edit2) return;
	QList<QTextEdit::ExtraSelection> selections = m_data->edit1 ? m_data->edit1->extraSelections() : m_data->edit2->extraSelections();
	//remove previous
	for (int i = 0; i < selections.size(); ++i)
		if (selections[i].format.property(QTextFormat::UserProperty + 2).toBool()) {
			selections.removeAt(i);
			--i;
		}
	if (m_data->edit1)m_data->edit1->setExtraSelections(selections);
	else m_data->edit2->setExtraSelections(selections);
}

void PySearch::format(const QTextBlock & b, int start, int end)
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
		m_data->document->markContentsDirty(b.position(), b.length());
	}*/

	if (!m_data->edit1 && !m_data->edit2) return;
	removePreviousFormat();
	QList<QTextEdit::ExtraSelection> selections = m_data->edit1 ? m_data->edit1->extraSelections() : m_data->edit2->extraSelections();

	QTextEdit::ExtraSelection selection;
	selection.format.setBackground(Qt::yellow);
	selection.format.setProperty(QTextFormat::UserProperty + 2, true);
	selection.cursor = QTextCursor(m_data->document);
	selection.cursor.setPosition(b.position() + start);
	selection.cursor.setPosition(b.position() + end, QTextCursor::KeepAnchor);
	selections.append(selection);
	if (m_data->edit1)m_data->edit1->setExtraSelections(selections);
	else m_data->edit2->setExtraSelections(selections);
}

bool PySearch::exactMatch() const
{
	return m_data->exact->isChecked();
}
bool PySearch::regExp() const
{
	return m_data->reg->isChecked();
}
bool PySearch::caseSensitive() const
{
	return m_data->case_sens->isChecked();
}

void PySearch::setEditor(QTextEdit  * ed)
{
	if(m_data->edit1)
		disconnect(m_data->edit1, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
	m_data->edit1 = ed;
	m_data->edit2 = nullptr;
	if (ed) {
		connect(m_data->edit1, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
		setDocument(ed->document());
	}	
}
void PySearch::setEditor(QPlainTextEdit  * ed)
{
	if (m_data->edit2)
		disconnect(m_data->edit2, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
	m_data->edit1 = nullptr;
	m_data->edit2 = ed;
	if (ed) {
		connect(m_data->edit2, SIGNAL(cursorPositionChanged()), this, SLOT(restartFromCursor()));
		setDocument(ed->document());
	}
}

void PySearch::setDocument(QTextDocument * ed)
{
	if (m_data->document) {
		removePreviousFormat();
		m_data->foundStart = m_data->foundEnd = m_data->line = 0;
		disconnect(m_data->document, SIGNAL(contentsChanged()), this, SLOT(restartFromCursor()));
	}
	m_data->document = ed;
	if (ed) {
		connect(m_data->document, SIGNAL(contentsChanged()), this, SLOT(restartFromCursor()));
	}
}
QTextDocument* PySearch::document() const
{
	return m_data->document;
}

QLineEdit * PySearch::search() const
{
	return m_data->search;
}

QPair<int, int> PySearch::lastFound(int * line) const
{
	if (*line)
		*line = m_data->line;
	return QPair<int, int>(m_data->foundStart, m_data->foundEnd);
}

void PySearch::search(bool forward)
{
	QString pattern = m_data->search->text();
	Qt::CaseSensitivity case_sens = m_data->case_sens->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
	QRegExp exp;
	if (regExp())
	exp = QRegExp(pattern, case_sens, QRegExp::Wildcard);
	else
		exp = QRegExp(pattern, case_sens, QRegExp::FixedString);

	QTextCursor c(m_data->document);
	if (m_data->foundStart == m_data->foundEnd || m_data->restartFromCursor)
	{
		if (m_data->edit1)
			c = m_data->edit1->textCursor();
		else if (m_data->edit2)
			c = m_data->edit2->textCursor();

		m_data->restartFromCursor = false;
	}
	else {
		QTextBlock b = m_data->document->findBlockByLineNumber(m_data->line);
		if (forward) {
			c.setPosition(m_data->foundEnd + b.position());
			c.setPosition(m_data->foundEnd + b.position(), QTextCursor::KeepAnchor);
		}
		else {
			c.setPosition(m_data->foundStart + b.position());
			c.setPosition(m_data->foundStart + b.position(), QTextCursor::KeepAnchor);
		}
	}

	QTextDocument::FindFlags flags = 0;
	if (m_data->case_sens->isChecked())
		flags |= QTextDocument::FindCaseSensitively;
	if (m_data->exact->isChecked())
		flags |= QTextDocument::FindWholeWords;
	if(!forward)
		flags |= QTextDocument::FindBackward;

	QTextCursor found = m_data->document->find(exp, c, flags);
	if (found.anchor() == found.position()) {
		//restart from beginning
		if (forward) {
			c = QTextCursor(m_data->document);
			c.setPosition(0);
			c.setPosition(0, QTextCursor::KeepAnchor);
			found = m_data->document->find(exp, c, flags);
		}
		else {
			c = QTextCursor(m_data->document);
			c.setPosition(m_data->document->lastBlock().position() + m_data->document->lastBlock().length());
			c.setPosition(m_data->document->lastBlock().position() + m_data->document->lastBlock().length(), QTextCursor::KeepAnchor);
			found = m_data->document->find(exp, c, flags);
		}
	}
	if (found.anchor() != found.position()) {
		QTextBlock b = m_data->document->findBlock(found.position());
		if (b.isValid()) {
			int bpos = b.position();
			//format(b, found.anchor() - bpos, found.position() - bpos);
			m_data->foundStart = found.anchor() - bpos;
			m_data->foundEnd = found.position() - bpos;
			m_data->line = b.firstLineNumber();

			/*QTextCharFormat f = found.blockCharFormat();
			f.setBackground(Qt::yellow);
			found.setCharFormat(f);*/

			if (m_data->edit1) {
				m_data->edit1->setTextCursor(found);
				m_data->edit1->ensureCursorVisible();
			}
			else if (m_data->edit2) {
				m_data->edit2->setTextCursor(found);
				m_data->edit2->ensureCursorVisible();
			}
			
		}
	}
	
}

void PySearch::searchNext()
{
	search(true);
}

void PySearch::searchPrev()
{
	search(false);
}
void PySearch::setExactMatch(bool enable)
{
	m_data->exact->blockSignals(true);
	m_data->exact->setChecked(enable);
	m_data->exact->blockSignals(false);
}
void PySearch::setRegExp(bool enable)
{
	m_data->reg->blockSignals(true);
	m_data->reg->setChecked(enable);
	m_data->reg->blockSignals(false);
}
void PySearch::setCaseSensitive(bool enable)
{
	m_data->case_sens->blockSignals(true);
	m_data->case_sens->setChecked(enable);
	m_data->case_sens->blockSignals(false);
}
void PySearch::close(bool)
{
	Q_EMIT closeRequested();
}

void PySearch::restartFromCursor()
{
	m_data->restartFromCursor = true;
}

void PySearch::showEvent(QShowEvent *)
{

}
void PySearch::hideEvent(QHideEvent *)
{
	removePreviousFormat();
	m_data->foundStart = m_data->foundEnd = m_data->line = 0;
}





PyToolBar::PyToolBar( QWidget * parent )
:QToolBar(parent)
{
	newfile = new QWidgetAction (this);
	newfile->setIcon(vipIcon("new.png"));
	newfile->setText(QString("New file"));
	addAction(newfile);

	open = new QWidgetAction (this);
	open->setIcon(vipIcon("open_dir.png"));
	open->setText(QString("Open file"));
	addAction(open);

	save = new QWidgetAction (this);
	save->setIcon(vipIcon("save.png"));
	save->setText(QString("Save file"));
	addAction(save);

	saveAs = new QWidgetAction (this);
	saveAs->setIcon(vipIcon("save_as.png"));
	saveAs->setText(QString("Save file as..."));
	addAction(saveAs);

	saveAll = new QWidgetAction (this);
	saveAll->setIcon(vipIcon("save_all.png"));
	saveAll->setText(QString("Save all"));
	addAction(saveAll);

	addSeparator();

	unindent = new QWidgetAction (this);
	unindent->setIcon(vipIcon("unindent.png"));
	unindent->setText(QString("Decrease indent"));
	addAction(unindent);

	indent = new QWidgetAction (this);
	indent->setIcon(vipIcon("indent.png"));
	indent->setText(QString("Increase indent"));
	addAction(indent);

	addSeparator();

	comment = new QWidgetAction (this);
	comment->setIcon(vipIcon("comment.png"));
	comment->setText(QString("Comment selection"));
	addAction(comment);

	uncomment = new QWidgetAction (this);
	uncomment->setIcon(vipIcon("uncomment.png"));
	uncomment->setText(QString("Uncomment selection"));
	addAction(uncomment);


	setIconSize(QSize(18,18));

}






PyEditor::PyEditor(Qt::Orientation tool_bar_orientation, QWidget * parent )
:QWidget(parent)
{
	m_tab.setTabsClosable (true);
	m_unique = false;

	m_search = new PySearch();
	//m_searchAction = m_bar.addWidget(m_search);

	QBoxLayout * lay;
	if (tool_bar_orientation == Qt::Horizontal) {
		lay = new QVBoxLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		lay->addWidget(&m_bar);
		lay->addWidget(&m_tab);
		lay->setSpacing(0);
	}
	else {
		m_bar.setOrientation(Qt::Vertical);
		lay = new QHBoxLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		lay->addWidget(&m_bar);
		lay->addWidget(&m_tab);
		lay->setSpacing(0);
	}

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->setSpacing(0);
	vlay->addLayout(lay);
	vlay->addWidget(m_search);
	setLayout(vlay);
	
	//m_tab.setStyleSheet("QTabWidget {background: transparent;}");
	

	connect(m_bar.newfile, SIGNAL(triggered(bool)), this, SLOT(NewFile()));
	connect(m_bar.open, SIGNAL(triggered(bool)), this, SLOT(Load()));
	connect(m_bar.save, SIGNAL(triggered(bool)), this, SLOT(Save()));
	connect(m_bar.saveAs, SIGNAL(triggered(bool)), this, SLOT(SaveAs()));
	connect(m_bar.saveAll, SIGNAL(triggered(bool)), this, SLOT(SaveAll()));

	connect(m_bar.unindent, SIGNAL(triggered(bool)), this, SLOT(Unindent()));
	connect(m_bar.indent, SIGNAL(triggered(bool)), this, SLOT(Indent()));
	connect(m_bar.comment, SIGNAL(triggered(bool)), this, SLOT(Comment()));
	connect(m_bar.uncomment, SIGNAL(triggered(bool)), this, SLOT(Uncomment()));

	connect(&m_tab,SIGNAL(tabCloseRequested ( int)),this,SLOT(AboutToClose(int)));
	connect(&m_tab,SIGNAL(currentChanged(int)), this, SLOT(CurrentChanged(int)));

	m_search->hide();
	connect(m_search, SIGNAL(closeRequested()), this, SLOT(CloseSearch()));
}

PyToolBar * PyEditor::TabBar() const
{
	return const_cast<PyToolBar*>( &m_bar);
}
CodeEditor * PyEditor::CurrentEditor() const
{
	return 	static_cast<CodeEditor*>(m_tab.currentWidget ());
}

CodeEditor * PyEditor::Editor(int pos) const
{
	return static_cast<CodeEditor*>(m_tab.widget (pos));
}

int PyEditor::EditorCount() const
{
	return m_tab.count();
}

int PyEditor::CurrentIndex() const
{
	return m_tab.currentIndex();
}

void PyEditor::setUniqueFile(bool unique)
{
	if (m_unique == unique)
		return;
	m_unique = unique;
	if (m_unique) {
		//close all editors
		for (int i = 0; i < EditorCount(); ++i)
			delete Editor(i);
		//create a new one
		CreateEditor();
		//hide actions
		m_tab.TabBar()->hide();
		m_bar.newfile->setVisible(false);
		m_bar.saveAll->setVisible(false);
	}
	else {
		m_tab.TabBar()->setVisible(true);
		m_bar.newfile->setVisible(false);
		m_bar.saveAll->setVisible(false);
	}
}
bool PyEditor::uniqueFile() const
{
	return m_unique;
}

int PyEditor::CreateEditor(const QString & filename)
{
	//find filename in all opened files, update it
	if(!filename.isEmpty())
	{
		//try to open in an existing editor with the same filename
		for(int i=0; i< EditorCount(); ++i)
		{
			if(Editor(i)->FileInfo() == QFileInfo(filename))
			{
				Editor(i)->OpenFile(filename);
				m_tab.setCurrentIndex(i);
				return i;
			}
		}

		//try to open in an empty editor
		for (int i = 0; i< EditorCount(); ++i)
		{
			if (Editor(i)->isEmpty())
			{
				Editor(i)->OpenFile(filename);
				m_tab.setCurrentIndex(i);
				return i;
			}
		}
	}

	//create the new editor
	int index=-1;
	if(filename.isEmpty())
	{
		int id = nextId();
		QString fname = "New" + QString::number(id);
		index = m_tab.addTab(new CodeEditor(), fname);
		Editor(index)->OpenFile(fname);
		Editor(index)->setPlainText(m_default_code);
		Editor(index)->setProperty("new_id", id);
		Editor(index)->setProperty("filename", fname);
		m_ids.insert(id, id);
	}
	else
	{
		index = m_tab.addTab(new CodeEditor(),QFileInfo(filename).completeBaseName());
		m_tab.setTabToolTip (index, QFileInfo(filename).canonicalFilePath());
		Editor(index)->OpenFile(filename);
	}



	m_tab.setCurrentIndex(index);
	if (BaseHighlighter * h = CodeEditor::stdColorSchemeForType("Python"))
	{
		h = h->clone(CurrentEditor()->document());
		h->updateEditor(CurrentEditor());
		h->rehighlight();
	}
	PyEditorFilter * filter = new PyEditorFilter(CurrentEditor());
	connect(filter,SIGNAL(SaveTriggered()),this,SLOT(Save()));
	connect(filter, SIGNAL(SearchTriggered()), this, SLOT(ShowSearchAndFocus()));
	connect(CurrentEditor()->document(),SIGNAL(modificationChanged (bool)),this,SLOT(ModificationChanged (bool)));
	SetHeaderBarVisibility();
	Q_EMIT modified(false);
	return index;
}

void PyEditor::SetDefaultCode(const QString & code)
{
	m_default_code = code;
}

void PyEditor::SetCurrentIndex(int index)
{
	m_tab.setCurrentIndex(index);
}

CodeEditor * PyEditor::Load()
{
	QString filename = VipFileDialog::getOpenFileName(nullptr,"Open python file","Python file (*.py)");
	if(!filename.isEmpty())
	{
		if (m_unique) {
			//just update the current editor
			if (CurrentEditor()) {
				QFile in(filename);
				if (in.open(QFile::ReadOnly | QFile::Text))
					CurrentEditor()->setPlainText(in.readAll());
			}
			return CurrentEditor();
		}
		else {
			int index = CreateEditor(filename);
			if (index >= 0) return Editor(index);
		}
	}

	return nullptr;
}

CodeEditor * PyEditor::OpenFile(const QString & filename)
{
	if (m_unique) {
		//just update the current editor
		if (CurrentEditor()) {
			QFile in(filename);
			if (in.open(QFile::ReadOnly | QFile::Text))
				CurrentEditor()->setPlainText(in.readAll());
		}
		return CurrentEditor();
	}

	int index= CreateEditor(filename);
	if(index >= 0) return Editor(index);
	else return nullptr;
}

bool PyEditor::Save(CodeEditor* editor)
{
	if(editor)
	{
		if(editor->FileInfo().exists())
		{
			return editor->SaveToFile(editor->FileInfo().absoluteFilePath());
			//m_tab.setTabText(m_tab.indexOf (editor),editor->FileInfo().completeBaseName());
			//editor->document()->setModified(false);
		}
		else
		{
			QString filename = VipFileDialog::getSaveFileName2(nullptr, vipGetPythonScriptsDirectory(), "Save python file","Python file (*.py)");
			if(!filename.isEmpty())
			{
				return editor->SaveToFile(filename);
				//m_tab.setTabText(m_tab.indexOf (editor),editor->FileInfo().completeBaseName());
				//editor->document()->setModified(false);
			}
		}

	}

	return false;
}

void PyEditor::Save()
{
	Save(CurrentEditor());
}

void PyEditor::SaveAs()
{
	if(CurrentEditor())
	{
		QString path = CurrentEditor()->FileInfo().exists() ?
			CurrentEditor()->FileInfo().canonicalFilePath() :
			vipGetPythonScriptsDirectory();

		QString filename = VipFileDialog::getSaveFileName2(nullptr, path,"Save python file","Python file (*.py)");
		if(!filename.isEmpty())
		{
			//m_tab.setTabText(m_tab.currentIndex (),CurrentEditor()->FileInfo().completeBaseName());
			CurrentEditor()->SaveToFile(filename);
		}
	}
}

void PyEditor::SaveAll()
{
	for(int i=0; i< EditorCount(); ++i)
	{
		Save(Editor(i));
	}
}

CodeEditor * PyEditor::NewFile()
{
	int index= CreateEditor();
	if(index >= 0) return Editor(index);
	else return nullptr;
}

void PyEditor::Comment()
{
	if( CurrentEditor() )
	{
		PyEditorFilter filter(CurrentEditor());
		filter.CommentSelection();
	}
}

void PyEditor::Uncomment()
{
	if( CurrentEditor() )
	{
		PyEditorFilter filter(CurrentEditor());
		filter.UncommentSelection();
	}
}

void PyEditor::Indent()
{
	if( CurrentEditor() )
	{
		PyEditorFilter filter(CurrentEditor());
		filter.IndentSelection();
	}
}

void PyEditor::Unindent()
{
	if( CurrentEditor() )
	{
		PyEditorFilter filter(CurrentEditor());
		filter.UnindentSelection();
	}
}

void PyEditor::CloseSearch()
{
	m_search->setVisible(false);
}
void PyEditor::ShowSearch()
{
	m_search->setVisible(true);
}
void PyEditor::ShowSearchAndFocus()
{
	m_search->setVisible(true);
	m_search->search()->setFocus(Qt::MouseFocusReason);
	m_search->search()->setText(this->CurrentEditor()->textCursor().selectedText());
}
void PyEditor::SetSearchVisible(bool vis)
{
	m_search->setVisible(vis);
}

QByteArray PyEditor::SaveState() const
{ 
	QByteArray res;
	{
		QDataStream str(&res, QIODevice::WriteOnly);
		str.setByteOrder(QDataStream::LittleEndian);
		str << (quint32)EditorCount();
		str << (quint32)m_tab.currentIndex();
		for (int i = 0; i < EditorCount(); ++i)
		{
			QByteArray name = Editor(i)->FileInfo().exists() ?
				Editor(i)->FileInfo().canonicalFilePath().toLatin1() :
				filename(Editor(i)).toLatin1();

			QByteArray code = Editor(i)->FileInfo().exists() ? QByteArray() : Editor(i)->toPlainText().toLatin1();

			//write name and code with their length
			str << name <<  code;
		}
	}
	return res;
}
void PyEditor::RestoreState(const QByteArray & state)
{
	m_tab.clear();
	QDataStream str(state);
	str.setByteOrder(QDataStream::LittleEndian);
	try {
		quint32 count = 0, current=0;
		str >> count >> current;
		if (count > 100) count = 0;
		for(int i=0; i < (int)count; ++i) {
			QByteArray name, code;
			str >> name >> code;

			if (name.startsWith("New")) {
				int index = CreateEditor("");
				Editor(index)->setPlainText(code);
			}
			else {
				/*int index =*/ CreateEditor(name);
			}
		}
		if ((int)current < m_tab.count())
			m_tab.setCurrentIndex(current);
	}
	catch (...)
	{

	}
}

void PyEditor::SetHeaderBarVisibility()
{
	if(m_tab.count() > 1)
		m_tab.TabBar()->show();
	else
		m_tab.TabBar()->hide();
}

void PyEditor::AboutToClose(int index)
{
	bool ask_for_save = false;
	int id = Editor(index)->property("new_id").toInt();

	if (!(id > 0 && Editor(index)->toPlainText().isEmpty())) {
		ask_for_save = Editor(index)->document()->isModified();
	}
	
	if(ask_for_save)
	{
		if( QMessageBox::question(this,"Save before closing","Do you want to save editor's content before closing it?", QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
		{
			Save(Editor(index));
		}
	}

	if (id > 0)
		m_ids.remove(id);

	m_tab.removeTab(index);
	SetHeaderBarVisibility();
}

int PyEditor::nextId() const
{
	int i = 1;
	for (QMap<int, int>::const_iterator it = m_ids.begin(); it != m_ids.end(); ++it, ++i)
		if (i != it.key())
			return i;
	return m_ids.size() + 1;
}
 
QString PyEditor::filename(CodeEditor * ed) const
{
	QString name = CurrentEditor()->FileInfo().fileName();
	if (name.isEmpty())
		name = ed->property("filename").toString();
	return name;
}
QString PyEditor::canonicalFilename(CodeEditor * ed) const
{
	QString name = CurrentEditor()->FileInfo().canonicalFilePath();
	if (name.isEmpty())
		name = ed->property("filename").toString();
	return name;
}

void PyEditor::CurrentChanged(int)
{
	if (CurrentEditor())
		m_search->setEditor(CurrentEditor());
	else
		m_search->setEditor((QTextEdit*)nullptr);
}

void PyEditor::ModificationChanged (bool modify)
{
	//vip_debug("fname: %s\n", filename(CurrentEditor()).toLatin1().data());
	if(modify)
	{
		m_tab.setTabText(m_tab.currentIndex(),"*"+ filename(CurrentEditor()));
		m_tab.setTabToolTip(m_tab.currentIndex(), canonicalFilename(CurrentEditor()));
	}
	else
	{
		m_tab.setTabText(m_tab.currentIndex(), filename(CurrentEditor()));
		m_tab.setTabToolTip(m_tab.currentIndex(), canonicalFilename(CurrentEditor()));
	}
	Q_EMIT modified(modify);
}

