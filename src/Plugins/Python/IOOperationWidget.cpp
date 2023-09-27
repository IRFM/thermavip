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

#include "VipGui.h"
#include "VipDisplayArea.h"

#include "PyOperation.h"
#include "CodeEditor.h"
#include "IOOperationWidget.h"






QString vipGetPythonHistoryFile(const QString & suffix)
{
	QString path = vipGetPythonDirectory(suffix);
	return path + "history.py";
}

CommandList::CommandList(qint32 max_size)
	:m_maxSize(max_size), m_pos(0)
{

}

void CommandList::SetHistoryFile(const QString & filename)
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
				//read date
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

void CommandList::AddCommand(const QString & cmd, const QDate & date)
{
	//do not record twice a command
	if (m_commands.size() && cmd == m_commands.last().command)
		return;

	Command c;
	c.command = cmd;
	c.date = date;
	if (c.date.isNull()) c.date = QDate::currentDate();

	m_commands.append(c);
	if (m_commands.size() > m_maxSize && m_maxSize > 0)
		m_commands.pop_front();
	m_pos = -1;

	QDate current = QDate::currentDate();
	if (!m_historyFile.isEmpty() && !cmd.startsWith("#")) {
		//add to file
		QFile fout(m_historyFile);
		//try to open for 1s
		qint64 start = QDateTime::currentMSecsSinceEpoch();
		while (!fout.open(QFile::WriteOnly | QFile::Append) && (QDateTime::currentMSecsSinceEpoch() - start) < 1000)
			QThread::msleep(10);
		if (!fout.isOpen())
			return;

		fout.seek(fout.size());
		QTextStream str(&fout);
		//update date
		if (current > m_lastDate) {
			str << "\n# " << current.toString("yyyy/MM/dd") << "\n\n";
			m_lastDate = current;
		}
		//write command
		str << cmd << "\n";

	}

}

QString CommandList::Next()
{
	if (m_commands.size() == 0)
		return QString();

	if (m_pos == -1)
		m_pos = m_commands.size() - 1;
	else
		m_pos++;

	if (m_pos >= m_commands.size()) m_pos = 0;

	return m_commands[m_pos].command;
}

QString CommandList::Previous()
{
	if (m_commands.size() == 0)
		return QString();

	if (m_pos == -1)
		m_pos = m_commands.size() - 1;
	else
		m_pos--;

	if (m_pos < 0) m_pos = m_commands.size() - 1;

	return m_commands[m_pos].command;
}



IOOperationWidget::IOOperationWidget(QWidget * parent)
	:QTextEdit(parent), m_last_append_pos(0), m_wait_for_more(false), m_inside_magic_command(false)
{

	
	m_error_color = vipDefaultTextErrorColor(this);
	m_process = NULL;

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


	m_commands.SetHistoryFile(vipGetPythonHistoryFile());
}

IOOperationWidget::~IOOperationWidget()
{
}


IOOperation * IOOperationWidget::Process()
{
	return m_process;
}

void IOOperationWidget::SetProcess(IOOperation * proc)
{
	if (m_process)
	{
		disconnect(m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(NewOutput()));
		disconnect(m_process, SIGNAL(readyReadStandardError()), this, SLOT(NewError()));
	}

	m_process = proc;
	if (m_process)
	{

		NewOutput();
		NewError();
		connect(m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(NewOutput()));
		connect(m_process, SIGNAL(readyReadStandardError()), this, SLOT(NewError()));
	}
}


void IOOperationWidget::NewOutput()
{
	QByteArray bytes = m_process->readAllStandardOutput();
	QTextCodec *codec = QTextCodec::codecForName("IBM850");
	QString str = codec->toUnicode(bytes);
	this->AppendText(str, vipWidgetTextBrush(this).color());
	m_last_output = str;
	if (str.endsWith(">>> ") || str.endsWith("... ") || str.endsWith("] ") || str.endsWith("> "))
		m_wait_for_more = true;
	//printf("out: '%s'\n", str.toLatin1().data());
}


void IOOperationWidget::NewError()
{
	this->moveCursor(QTextCursor::End);
	QByteArray bytes = m_process->readAllStandardError();
	QTextCodec *codec = QTextCodec::codecForName("IBM850");
	QString str = codec->toUnicode(bytes);
	AppendText(str, vipDefaultTextErrorColor(this));// Qt::red);

}

qint64 IOOperationWidget::LastPosition() const
{
	return this->document()->characterCount();
}

qint64 IOOperationWidget::LastAppendPosition() const
{
	return m_last_append_pos;
}

QTextCursor IOOperationWidget::GetValidCursor()
{
	QTextCursor cursor = textCursor();
	int pos = qMin(cursor.anchor(), cursor.position());
	int pos_end = qMax(cursor.anchor(), cursor.position());
	if (pos_end > LastPosition() - 1) pos_end = LastPosition() - 1;
	if (pos_end < LastAppendPosition())
		return QTextCursor();

	cursor.setPosition(qMax(qint64(pos), LastAppendPosition()));
	cursor.setPosition(pos_end, QTextCursor::KeepAnchor);

	return cursor;
}

void IOOperationWidget::AppendText(const QString & str, const QColor & color)
{
	this->moveCursor(QTextCursor::End);
	this->setTextColor(color);
	this->insertPlainText(str);
	this->setTextColor(vipWidgetTextBrush(this).color());

	m_last_append_pos = LastPosition() - 1;
}

void IOOperationWidget::Stop()
{
	m_process->stop();
}

void IOOperationWidget::Start()
{
	m_process->start();
}

void IOOperationWidget::Restart()
{
	m_process->restart();
}

#include <qstyleoption.h>
#include <qstyle.h>
void IOOperationWidget::paintEvent(QPaintEvent * evt)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(viewport());
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
	QTextEdit::paintEvent(evt);
}

void IOOperationWidget::ExecCommand(const QString & command)
{
	if (command.isEmpty())
		return;
	QTextCursor cursor = GetValidCursor();
	cursor.setPosition(LastAppendPosition(), QTextCursor::MoveAnchor);
	cursor.setPosition(this->toPlainText().size(), QTextCursor::KeepAnchor);
	cursor.clearSelection();
	cursor.insertText(command + "\n");

	if (command.startsWith("!")) {
		//exec in main thread
		QString c = command.mid(1);
		EvalResultType t= evalCodeMainThread(c);
		QString to_print = t.first;
		if (to_print.isEmpty())
			to_print = t.second;
		if (!to_print.isEmpty())
			cursor.insertText(to_print + "\n");
		if (m_process)
			m_process->write("\n");
		m_commands.AddCommand(c);
		Q_EMIT NewCommandAdded();
		return;
	}


	if (m_process && m_process->isRunning())
	{
		if (m_process->handleMagicCommand(command)) {

		}
		else
			m_process->write(command.toLatin1());
		m_commands.AddCommand(command);
		Q_EMIT NewCommandAdded();
	}
}

void IOOperationWidget::keyPressEvent(QKeyEvent * event)
{
	if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return))
	{
		QString str = this->toPlainText();
		QString text(str.mid(LastAppendPosition()) + "\n");

		//set cursor to the end
		this->moveCursor(QTextCursor::End);

		QTextEdit::keyPressEvent(event);

		if (m_inside_magic_command) {
			//We are inside a magic command, emit this signal
			//Q_EMIT inputWritten(text);
			//return;
		}

		if (m_process && m_process->isRunning())
		{
			//special treatment for "... " interpreter, when the Python interpreter is in a nested operation
			bool is_nested = (m_last_output == "... " && text != "\n");
			if (is_nested)
				text.remove("\n");

			
			if (text.startsWith("!")) {
				//exec in main thread
				text.remove("\n");
				QString c = text.mid(1);
				EvalResultType t = evalCodeMainThread(c);
				QString to_print = t.first;
				if (to_print.isEmpty())
					to_print = t.second;
				if (!to_print.isEmpty())
					this->AppendText(to_print +"\n",Qt::black);
				if (m_process)
					m_process->write("\n");
				m_commands.AddCommand(text);
				Q_EMIT NewCommandAdded();
				return;
			}

			m_inside_magic_command = true;
			if (m_process->handleMagicCommand(text)) {
				m_inside_magic_command = false;
				m_process->write("\n");
			}
			else {
				m_inside_magic_command = false;
				m_process->write(text.toLatin1());
			}
				
			text.remove("\n");
			if (!text.isEmpty() || m_last_output == "... ") {
				m_commands.AddCommand(text);
				Q_EMIT NewCommandAdded();
			}
		}

	}
	else if (event->key() == Qt::Key_Backspace)
	{
		QTextCursor cursor = GetValidCursor();
		if (!cursor.isNull() && !(cursor.position() == cursor.anchor() && cursor.position() == LastAppendPosition()))
		{
			setTextCursor(cursor);
			QTextEdit::keyPressEvent(event);
		}
	}
	else if (event->key() == Qt::Key_Delete)
	{
		QTextCursor cursor = GetValidCursor();
		if (!cursor.isNull())
		{
			setTextCursor(cursor);
			QTextEdit::keyPressEvent(event);
		}
	}
	else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
	{

		QTextCursor cursor = textCursor();
		QString line = (event->key() == Qt::Key_Up ? m_commands.Previous() : m_commands.Next());
		cursor.setPosition(LastAppendPosition());
		cursor.setPosition(document()->characterCount() - 1, QTextCursor::KeepAnchor);
		cursor.beginEditBlock();
		cursor.deleteChar();
		cursor.insertText(QString(line));
		cursor.endEditBlock();
		setTextCursor(cursor);
	}
	else if (event->key() == Qt::Key_Home)
	{
		QTextCursor cursor = textCursor();
		if (event->modifiers() & Qt::ShiftModifier)
			cursor.setPosition(LastAppendPosition(), QTextCursor::KeepAnchor);
		else
			cursor.setPosition(LastAppendPosition());
		setTextCursor(cursor);
	}
	else if (event->key() == Qt::Key_X && (event->modifiers() & Qt::ControlModifier))
	{
		QTextCursor cursor = GetValidCursor();
		if (!cursor.isNull())
		{
			setTextCursor(cursor);
			this->cut();
		}
	}
	else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier))
	{
		this->copy();
	}
	else if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier))
	{
		QTextCursor cursor = GetValidCursor();
		if (!cursor.isNull())
		{
			setTextCursor(cursor);
			this->paste();
		}
	}
	else if (!event->text().isEmpty() && event->text()[0].isPrint())
	{
		QTextCursor cursor = GetValidCursor();
		if (!cursor.isNull())
			QTextEdit::keyPressEvent(event);

	}
	else
		QTextEdit::keyPressEvent(event);

}


static QString cleanLine(const QString & line)
{
	if (line.isEmpty()) return line;
	//remove starting "... " or ">>> " or any other invalid pattern
	if (line[0] == '>' || line[0] == '.' || line[0] == '/' || line[0] == '\\' || line[0] == '[') {
		//find the first space
		for (int i = 1; i < line.size(); ++i) {
			if (line[i] == ' ')
				return line.mid(i + 1);
		}
	}
	return line;
}

void IOOperationWidget::PasteText(const QString & text)
{
	QTextCursor c = this->textCursor();
	int max_pos = qMax(c.position(), c.anchor());
	int min_pos = qMin(c.position(), c.anchor());
	if (min_pos < LastAppendPosition())
		return;

	QString full_text = toPlainText();

	if (max_pos != full_text.size()) {
		c.insertText(text); //standard behavior
	}
	else {
		//custom behavior
		QStringList lines = text.split("\n");
		if (lines.isEmpty())
			return;

		//insert first line
		c.insertText(cleanLine(lines.first()));

		if (lines.size() > 1) {
			//exec each line

			for (int i = 1; i < lines.size(); ++i) {

				m_wait_for_more = false;

				//simulate key press
				QKeyEvent evt(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
				this->keyPressEvent(&evt);

				//wait for prompt 
				//PyIOOperation * op = qobject_cast<PyIOOperation*>(Process());
				while (!m_wait_for_more &&/* !(op && op->isWaitingForInput()) &&*/ Process()->isRunning())
					vipProcessEvents(NULL, 1000);

				//insert next line
				this->moveCursor(QTextCursor::End);
				this->insertPlainText(cleanLine(lines[i]));
			}

		}

	}
}

void IOOperationWidget::insertFromMimeData(const QMimeData *source)
{
	//reimplement paste operation
	PasteText(source->text());

}


class PyInterpreterToolWidget::PrivateData
{
public:
	IOOperationWidget * interpreter;
	CodeEditor * history;
	QSplitter * splitter;

	QToolButton * closeHistory;
	QAction * showHistory;
	QAction * restart;
};
PyInterpreterToolWidget::PyInterpreterToolWidget(VipMainWindow * win)
	:VipToolWidget(win)
{
	m_data = new PrivateData();
	m_data->interpreter = new IOOperationWidget();
	m_data->interpreter->SetProcess(GetPyOptions());
	GetPyOptions()->setParent(m_data->interpreter);
	//QObject::connect(GetPyOptions(), SIGNAL(finished()), m_data->interpreter, SLOT(clear()));
	QObject::connect(GetPyOptions(), SIGNAL(started()), GetPyOptions(), SLOT(startInteractiveInterpreter()));
	GetPyOptions()->startInteractiveInterpreter();

	m_data->history = new CodeEditor();
	m_data->splitter = new QSplitter(Qt::Horizontal);
	m_data->splitter->addWidget(m_data->interpreter);
	m_data->splitter->addWidget(m_data->history);

	m_data->closeHistory = new QToolButton(m_data->history);
	m_data->closeHistory->setIcon(vipIcon("close.png"));
	m_data->closeHistory->setToolTip("Hide history file");
	m_data->closeHistory->setAutoRaise(false);
	m_data->closeHistory->setAutoFillBackground(false);
	connect(m_data->closeHistory, SIGNAL(clicked(bool)), this, SLOT(HideHistory()));

	m_data->history->OpenFile(vipGetPythonHistoryFile());
	m_data->history->setReadOnly(true);
	//go to the end of file
	m_data->history->moveCursor(QTextCursor::End);
	m_data->history->ensureCursorVisible();
	//if (m_data->history->verticalScrollBar()->isVisible())
	//	m_data->history->verticalScrollBar()->setValue(m_data->history->verticalScrollBar()->maximum());
	m_data->history->hide();
	m_data->history->installEventFilter(this);
	connect(m_data->interpreter, SIGNAL(NewCommandAdded()), m_data->history, SLOT(reload()));

	m_data->restart = this->titleBarWidget()->toolBar()->addAction(vipIcon("restart.png"), "Restart interpreter");
	m_data->showHistory = this->titleBarWidget()->toolBar()->addAction(vipIcon("visible.png"), "Show/hide history file");
	m_data->showHistory->setCheckable(true);

	connect(m_data->restart, SIGNAL(triggered(bool)), this, SLOT(RestartInterpreter()));
	connect(m_data->showHistory, SIGNAL(triggered(bool)), this, SLOT(SetHistoryVisible(bool)));

	this->setWidget(m_data->splitter);
	this->setWindowTitle("Python internal console");
	this->setObjectName("Python internal console");
}

PyInterpreterToolWidget::~PyInterpreterToolWidget()
{
	m_data->history->removeEventFilter(this);
	delete m_data;
}

IOOperationWidget * PyInterpreterToolWidget::Interpreter() const {
	return m_data->interpreter;
}
CodeEditor * PyInterpreterToolWidget::HistoryFile() const {
	return m_data->history;
}
QSplitter * PyInterpreterToolWidget::Splitter() const {
	return m_data->splitter;
}
bool PyInterpreterToolWidget::HistoryVisible() const {
	return m_data->history->isVisible();
}

void PyInterpreterToolWidget::SetHistoryVisible(bool vis) {
	m_data->showHistory->blockSignals(true);
	m_data->showHistory->setChecked(vis);
	m_data->showHistory->blockSignals(false);
	m_data->history->setVisible(vis);
}
void PyInterpreterToolWidget::HideHistory() {
	SetHistoryVisible(false);
}
void PyInterpreterToolWidget::RestartInterpreter() {
	m_data->interpreter->clear();
	printf("RestartInterpreter\n");
	qint64 interp = (qint64)GetPyOptions()->pyIOOperation(true);
	printf("End %lld\n", interp);
}

bool PyInterpreterToolWidget::eventFilter(QObject *, QEvent * evt)
{
	if (evt->type() == QEvent::Resize || evt->type() == QEvent::Show) {
		int additinal = m_data->history->verticalScrollBar()->isVisible() ?
			m_data->history->verticalScrollBar()->width() : 0;
		m_data->closeHistory->move(m_data->history->width() - m_data->closeHistory->width() - additinal, 0);
	}
	else if (evt->type() == QEvent::KeyPress) {
		QKeyEvent * key = static_cast<QKeyEvent *>(evt);
		if (key->key() == Qt::Key_Return || key->key() == Qt::Key_F5) {

			//run selected lines
			QTextCursor c = m_data->history->textCursor();
			int start = c.selectionStart();
			int end = c.selectionEnd();
			c.setPosition(start);
			int firstLine = c.blockNumber();
			c.setPosition(end, QTextCursor::KeepAnchor);
			int lastLine = c.blockNumber();
			if (c.atBlockStart() && lastLine > firstLine)
				--lastLine;

			QString text;
			for (int i = firstLine; i <= lastLine; ++i)
				text += m_data->history->document()->findBlockByLineNumber(i).text() + "\n";

			m_data->interpreter->moveCursor(QTextCursor::End);
			/*c = m_data->interpreter->textCursor();
			int end_pos = m_data->interpreter->toPlainText().size();
			c.setPosition(end_pos);
			m_data->interpreter->setTextCursor(c);*/
			m_data->interpreter->PasteText(text);
			m_data->interpreter->raise();
			m_data->interpreter->setFocus();
			return true;
		}
	}
	return false;
}


PyInterpreterToolWidget * pyGetPythonInterpreter()
{
	static PyInterpreterToolWidget *python = new PyInterpreterToolWidget(vipGetMainWindow());
	return python;
}
