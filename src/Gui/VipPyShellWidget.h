#ifndef IO_OPERATION_WIDGET_H
#define IO_OPERATION_WIDGET_H

#include <QTextEdit>
#include <QProcess>
#include <QStringList>
#include <QSplitter>

#include "VipToolWidget.h"
#include "VipPyOperation.h"
#include "VipTextEditor.h"

/**
Returns the default shell history file used by VipPyShellWidget
*/
VIP_GUI_EXPORT QString vipGetPythonHistoryFile();

class VIP_GUI_EXPORT VipPyHistoryList
{
	struct Command
	{
		QDate date;
		QString command;
	};
	QString m_historyFile;
	QList<Command> m_commands;
	qint32 m_maxSize;
	qint32 m_pos;
	QDate m_lastDate;

public:
	VipPyHistoryList(qint32 max_size = -1);

	void setHistoryFile(const QString& filename);
	void addCommand(const QString& cmd, const QDate& date = QDate());
	QString next();
	QString previous();
};

/// @brief Base class for internal Python shell.
/// Currently inheritied by VipPyShellWidget and VipIPythonShellWidget
class VipBasePythonShell
{
public:
	virtual ~VipBasePythonShell() {}

	virtual QTextEdit* textEdit() const = 0;

	virtual void execLine(const QString& line) = 0;
	virtual void execLines(const QStringList& lines)
	{
		for (const auto& l : lines)
			execLine(l);
	}
	virtual void addLines(const QStringList& lines) = 0;
	virtual bool isIDLE() const = 0;
	virtual void restartInterpreter() = 0;
	virtual void stopCurrentCode() = 0;

	/// @brief Set the internal VipBaseIOOperation used to execute commands.
	/// This widget does NOT take ownership of the process.
	virtual void setProcess(VipBaseIOOperation* proc) = 0;
	virtual VipBaseIOOperation* process() const = 0;

	virtual void appendText(const QString& str, const QColor& color) = 0;
};

/// @brief Small text editor used as shell widget.
/// VipPyShellWidget executes commands through a VipPyShellWidget object.
class VIP_GUI_EXPORT VipPyShellWidget
  : public QTextEdit
  , public VipBasePythonShell
{
	Q_OBJECT
public:
	VipPyShellWidget(QWidget* parent = nullptr);
	~VipPyShellWidget();

	/// @brief Set the internal VipBaseIOOperation used to execute commands.
	/// This widget does NOT take ownership of the process.
	void setProcess(VipBaseIOOperation* proc);
	VipBaseIOOperation* process() const;

	virtual QTextEdit* textEdit() const { return const_cast<VipPyShellWidget*>(this); }

	QIcon runningIcon() const;
	QIcon finishedIcon() const;

	/// @brief Add text to the end of editor
	virtual void appendText(const QString& str, const QColor& color);
	/// @brief Add a command to the last valid editor position, and execute it
	virtual void execLine(const QString& command);
	
	virtual void addLines(const QStringList& lines);
	virtual bool isIDLE() const;
	virtual void stopCurrentCode();
	/// @brief Paste given text to the last valid editor position
	void pasteText(const QString& text);

public Q_SLOTS:
	void setRunningIcon(const QIcon& icon);
	void setFinishedIcon(const QIcon& icon);
	void stop();
	void start();
	void restart();
	virtual void restartInterpreter();
private Q_SLOTS:
	void newOutput();
	void newError();

Q_SIGNALS:
	void commandAdded();

protected:
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void paintEvent(QPaintEvent* evt);
	virtual void insertFromMimeData(const QMimeData* source);

private:
	qint64 lastPosition() const;
	qint64 lastAppendPosition() const;
	QTextCursor getValidCursor();

	VIP_DECLARE_PRIVATE_DATA();
};

class VipIPythonShellWidget;

/// @brief History text widget used by VipIPythonShellWidget
class VIP_GUI_EXPORT VipIPythonHistory : public VipTextEditor
{
	Q_OBJECT

public:
	VipIPythonHistory(VipIPythonShellWidget* shell);
	~VipIPythonHistory();

	QStringList selectedLines() const;

	void setMaxLines(int);
	int maxLines() const;

public Q_SLOTS:
	void execSelectedLines();
	void updateContent();
	void updateContentIfVisible();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Python shell widget using IPython.
/// This uses the Python file embedded_ipython.py.
/// Only works if Qt and jupyter notebook are installed.
class VIP_GUI_EXPORT VipIPythonShellWidget
  : public QWidget
  , public VipBasePythonShell
{
	Q_OBJECT

public:
	VipIPythonShellWidget(QWidget* parent = nullptr);
	~VipIPythonShellWidget();

	virtual QTextEdit* textEdit() const;

	QString historyFile() const;
	QStringList historyFileLines(int max_lines = 100) const;
	virtual bool isIDLE() const;
	virtual void stopCurrentCode();

	/// @brief Set the internal VipBaseIOOperation used to execute commands.
	/// This widget does NOT take ownership of the process.
	virtual void setProcess(VipBaseIOOperation* proc);
	virtual VipBaseIOOperation* process() const;

public Q_SLOTS:
	virtual void restartInterpreter();
	virtual void addLines(const QStringList& lines);
	void addLine(const QString& line);
	virtual void execLines(const QStringList& lines);
	virtual void execLine(const QString& line);
	virtual void appendText(const QString& str, const QColor& color = Qt::transparent);
	void scrollToEnd();

private Q_SLOTS:
	void newOutput();
	void newError();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Internal Python interpreter shell.
/// Use either a home made Python shell or an IPython one if Qt and jupyter notebook are installed.
class VIP_GUI_EXPORT VipPyInterpreterToolWidget : public VipToolWidget
{
	Q_OBJECT
public:
	VipPyInterpreterToolWidget(VipMainWindow* win);
	~VipPyInterpreterToolWidget();

	QWidget* shellWidget() const;
	QTextEdit* shellEditor() const;
	VipBasePythonShell* shell() const;

	VipTextEditor* historyFile() const;
	QStringList historySelectedLines() const;
	QSplitter* splitter() const;
	bool historyVisible() const;

	virtual bool eventFilter(QObject* watcher, QEvent* evt);

public Q_SLOTS:
	void setHistoryVisible(bool);
	void hideHistory();
	void restartInterpreter();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_GUI_EXPORT VipPyInterpreterToolWidget* vipPyGetPythonInterpreter();

#endif
