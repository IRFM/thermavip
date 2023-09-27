#pragma once

#include "PyConfig.h"
#include "VipToolWidget.h"
#include "VipDisplayArea.h"

#include <qprocess.h>
#include <qwidget.h>
#include <qtabwidget.h>
#include <qtabbar.h>
#include <qapplication.h>

PYTHON_EXPORT void setIPythonFontSize(int);
PYTHON_EXPORT int iPythonFontSize();

PYTHON_EXPORT void setIPythonStyle(const QString&);
PYTHON_EXPORT QString iPythonStyle();

PYTHON_EXPORT bool isDarkColor(const QColor& c);
PYTHON_EXPORT bool isDarkSkin();

PYTHON_EXPORT QString pyGlobalSharedMemoryName();

/**
* IPythonConsoleProcess maps an IPython external console process.
* It uses the script qtconsole_widget.py as well as Thermavip.py to communicate with the console.
*/
class PYTHON_EXPORT IPythonConsoleProcess : public QProcess
{
	Q_OBJECT
		
public:
	IPythonConsoleProcess(QObject* parent = NULL);
	~IPythonConsoleProcess();

	void setTimeout(int milli_timeout);
	int timeout() const;

	/**
	* Tells if the process will be embedded within a QWidget.
	* If true, the ipython console will first be hidden, and then shown maximized after 500ms.
	*/
	void setEmbedded(bool);
	bool embedded() const;

	/**
	* Start the ipython console process and return its pid.
	* The console is first hidden, and will be shown maximized 500 ms after this function returns.
	* Returns 0 on error.
	* 
	* Note that if the distant console is already running, this will restart it.
	*/
	qint64 start(int font_size = -1, const QString & style = QString(), const QString& shared_memory_name = QString());

	qint64 windowId() const;

	/**
	* If the distant console has run at least once, returns the last used shared memory name.
	*/
	QString sharedMemoryName() const;

	/**
	* Send an object with given name to the process.
	* Returns true on success.
	*/
	bool sendObject(const QString& name, const QVariant& obj);

	/**
	* Retrieve a Python object from the distant console.
	* Returns the object on success, a PyError object on error.
	*/
	QVariant retrieveObject(const QString& name);

	/**
	* Silently execute a Python code into the distant console.
	* Returns true on success.
	*/
	bool execCode(const QString& code);

	/**
	* Push and execute a one line Python code into the ipython interpreter.
	* Returns true on success.
	*/
	bool execLine(const QString& code);

	/**
	* Push and execute a one line Python code into the ipython interpreter.
	* Returns true on success.
	* Does not wait for the line being executed.
	*/
	bool execLineNoWait(const QString& code);

	/**
	* Stop current code being executed and restart interpreter
	*/
	bool restart();


	bool isRunningCode();

	/**Returns the last error string*/
	QString lastError() const;

	/**Find a free shared memory name of the form 'Thermavip-X', where X is a numbed incremented at each trial*/
	static QString findNextMemoryName();
	static bool isFreeName(const QString& name);
	void setStyleSheet(const QString& st);

private:
	class PrivateData;
	PrivateData* m_data;
};


/**
* Widget displaying an ipython console based on IPythonConsoleProcess
*/
class PYTHON_EXPORT IPythonWidget : public QWidget
{
	Q_OBJECT

public:
	IPythonWidget(int font_size = -1, const QString &style = QString(), QWidget* parent = NULL);
	~IPythonWidget();

	IPythonConsoleProcess* process() const;
	bool isRunning() const { return process()->state() == QProcess::Running; }

public Q_SLOTS:
	/**
	* Restart shell
	*/
	bool restart();

	/**
	* Restart full process with initial parameters
	*/
	bool restartProcess();

	void focusChanged(QWidget* old, QWidget* now);
private:
	class PrivateData;
	PrivateData* m_data;
};




class IPythonTabWidget;

/**
*/
class PYTHON_EXPORT IPythonTabBar : public QTabBar
{
	Q_OBJECT
		Q_PROPERTY(QIcon closeIcon READ closeIcon WRITE setCloseIcon)
		Q_PROPERTY(QIcon restartIcon READ restartIcon WRITE setRestartIcon)
		Q_PROPERTY(QIcon hoverCloseIcon READ hoverCloseIcon WRITE setHoverCloseIcon)
		Q_PROPERTY(QIcon hoverRestartIcon READ hoverRestartIcon WRITE setHoverRestartIcon)
		Q_PROPERTY(QIcon selectedCloseIcon READ selectedCloseIcon WRITE setSelectedCloseIcon)
		Q_PROPERTY(QIcon selectedFloatIcon READ selectedRestartIcon WRITE setSelectedRestartIcon)

public:
	IPythonTabBar(IPythonTabWidget* parent);
	~IPythonTabBar();
	IPythonTabWidget* ipythonTabWidget() const;

	QIcon closeIcon() const;
	void setCloseIcon(const QIcon&);
	QIcon restartIcon() const;
	void setRestartIcon(const QIcon&);
	QIcon hoverCloseIcon() const;
	void setHoverCloseIcon(const QIcon&);
	QIcon hoverRestartIcon() const;
	void setHoverRestartIcon(const QIcon&);
	QIcon selectedCloseIcon() const;
	void setSelectedCloseIcon(const QIcon&);
	QIcon selectedRestartIcon() const;
	void setSelectedRestartIcon(const QIcon&);

protected:
	virtual void leaveEvent(QEvent* evt);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseDoubleClickEvent(QMouseEvent* evt);
	virtual void tabInserted(int index);

private Q_SLOTS:
	void closeTab();
	void restartTab();
	void restartTabProcess();
	void updateIcons();
private:
	class PrivateData;
	PrivateData* m_data;
}; 



class PYTHON_EXPORT IPythonTabWidget : public QTabWidget
{
	Q_OBJECT

public:
	IPythonTabWidget(QWidget* parent = NULL);
	~IPythonTabWidget();

	IPythonWidget* widget(int) const;

public Q_SLOTS:
	void closeTab(int index);
	void addInterpreter();
private Q_SLOTS:
	void updateTab();
protected:
	virtual void closeEvent(QCloseEvent*);
private:
	class PrivateData;
	PrivateData* m_data;
};



class PYTHON_EXPORT IPythonToolWidget : public VipToolWidget
{
	Q_OBJECT

public:
	IPythonToolWidget(VipMainWindow* win);
	~IPythonToolWidget();

	IPythonTabWidget* widget() const;

private:
	IPythonTabWidget* m_tabs;
};

PYTHON_EXPORT IPythonToolWidget* GetIPythonToolWidget(VipMainWindow* win = NULL);