#ifndef IO_OPERATION_WIDGET_H
#define IO_OPERATION_WIDGET_H

#include <QTextEdit>
#include <QProcess>
#include <QStringList>
#include <QTabWidget>

#include "VipToolWidget.h"



/**
Returns the shell history file
*/
VIP_GUI_EXPORT QString vipGetPythonHistoryFile(const QString & suffix = "thermavip");

class  VIP_GUI_EXPORT VipPyHistoryList
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

	VipPyHistoryList(qint32 max_size = -1);

	void setHistoryFile(const QString & filename);
	void addCommand(const QString & cmd, const QDate & date = QDate());
	QString next();
	QString previous();
};



class VipBaseIOOperation;

/// @brief Small text editor used as shell widget.
/// VipPyShellWidget executes commands through a VipPyShellWidget object.
class  VIP_GUI_EXPORT VipPyShellWidget : public QTextEdit
{
	Q_OBJECT
public:
	VipPyShellWidget(QWidget * parent = nullptr);
	~VipPyShellWidget();

	/// @brief Set the internal VipBaseIOOperation used to execute commands.
	/// This widget does NOT take ownership of the process.
	void setProcess(VipBaseIOOperation * proc);
	VipBaseIOOperation * process();

	QIcon runningIcon() const;
	QIcon finishedIcon() const;

	/// @brief Add text to the end of editor
	void appendText(const QString & str, const QColor & color);
	/// @brief Add a command to the last valid editor position, and execute it
	void execCommand(const QString & command);
	/// @brief Paste given text to the last valid editor position
	void pasteText(const QString & text);

public slots:
	void setRunningIcon(const QIcon& icon);
	void setFinishedIcon(const QIcon& icon);
	void stop();
	void start();
	void restart();

private slots:
	void newOutput();
	void newError();

Q_SIGNALS:
	void commandAdded();

protected:
	virtual void keyPressEvent(QKeyEvent * event);
	virtual void paintEvent(QPaintEvent * evt);
	virtual void insertFromMimeData(const QMimeData *source);
private:
	qint64 lastPosition() const;
	qint64 lastAppendPosition() const;
	QTextCursor getValidCursor();

	VIP_DECLARE_PRIVATE_DATA();
};



class VipTextEditor;
class QSplitter;

/// @brief Internal Python interpreter shell.
class  VIP_GUI_EXPORT VipPyInterpreterToolWidget : public VipToolWidget
{
	Q_OBJECT
public:
	VipPyInterpreterToolWidget(VipMainWindow * win);
	~VipPyInterpreterToolWidget();

	VipPyShellWidget * interpreter() const;
	VipTextEditor * historyFile() const;
	QSplitter * splitter() const;
	bool historyVisible() const;

	virtual bool eventFilter(QObject * watcher, QEvent * evt);

public Q_SLOTS:
	void setHistoryVisible(bool);
	void hideHistory();
	void restartInterpreter();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_GUI_EXPORT VipPyInterpreterToolWidget * vipPyGetPythonInterpreter();

#endif
