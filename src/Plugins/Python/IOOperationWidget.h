#ifndef IO_OPERATION_WIDGET_H
#define IO_OPERATION_WIDGET_H

#include <QTextEdit>
#include <QProcess>
#include <QStringList>
#include <QTabWidget>

#include "PyConfig.h"

#include "VipToolWidget.h"



/**
Returns the shell history file
*/
PYTHON_EXPORT QString vipGetPythonHistoryFile(const QString & suffix = "thermavip");

class  PYTHON_EXPORT CommandList
{
	struct Command {
		QDate date;
		QString command;
	};
	QString			m_historyFile;
	QList<Command> 	m_commands;
	qint32			m_maxSize;
	qint32 			m_pos;
	QDate			m_lastDate;

public:

	CommandList(qint32 max_size = -1);

	void SetHistoryFile(const QString & filename);

	void AddCommand(const QString & cmd, const QDate & date = QDate());
	QString Next();
	QString Previous();
};



class IOOperation;
class  PYTHON_EXPORT IOOperationWidget : public QTextEdit
{
	Q_OBJECT
public:
	IOOperationWidget(QWidget * parent = nullptr);
	~IOOperationWidget();

	void SetProcess(IOOperation * proc);
	IOOperation * Process();

	QIcon RunningIcon() const {return m_running_icon;}
	QIcon FinishedIcon() const {return m_finished_icon;}

	void AppendText(const QString & str, const QColor & color);

	void ExecCommand(const QString & command);

	void PasteText(const QString & text);

public slots:
	void SetRunningIcon(const QIcon & icon) {m_running_icon = icon;}
	void SetFinishedIcon(const QIcon & icon) {m_finished_icon = icon;}
	void Stop();
	void Start();
	void Restart();

private slots:
	void NewOutput();
	void NewError();

Q_SIGNALS:
	void NewCommandAdded();

protected:
	virtual void keyPressEvent(QKeyEvent * event);
	virtual void paintEvent(QPaintEvent * evt);
	virtual void insertFromMimeData(const QMimeData *source);
private:
	qint64 LastPosition() const;
	qint64 LastAppendPosition() const;
	QTextCursor GetValidCursor();

	IOOperation * m_process;
	qint64 m_last_append_pos;
	CommandList m_commands;

	QString m_last_command;
	QString m_last_output;
	QStringList m_last_args;

	QIcon m_running_icon;
	QIcon m_finished_icon;
	QColor m_error_color;
	bool m_wait_for_more ;
	bool m_inside_magic_command;
};

//PYTHON_EXPORT IOOperationWidget * GetPythonCommandInterpreter();


class CodeEditor;
class QSplitter;

class  PYTHON_EXPORT PyInterpreterToolWidget : public VipToolWidget
{
	Q_OBJECT
public:
	PyInterpreterToolWidget(VipMainWindow * win);
	~PyInterpreterToolWidget();

	IOOperationWidget * Interpreter() const;
	CodeEditor * HistoryFile() const;
	QSplitter * Splitter() const;
	bool HistoryVisible() const;

	virtual bool eventFilter(QObject * watcher, QEvent * evt);

public Q_SLOTS:
	void SetHistoryVisible(bool);
	void HideHistory();
	void RestartInterpreter();

private:
	class PrivateData;
	PrivateData * m_data;
};

PYTHON_EXPORT PyInterpreterToolWidget * pyGetPythonInterpreter();

#endif
