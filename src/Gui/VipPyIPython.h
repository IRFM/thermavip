#pragma once

#include "VipToolWidget.h"
#include "VipDisplayArea.h"

#include <qprocess.h>
#include <qwidget.h>
#include <qtabwidget.h>
#include <qtabbar.h>
#include <qapplication.h>

/// @brief Set/get the global IPython shell font size
VIP_GUI_EXPORT void vipSetIPythonFontSize(int);
VIP_GUI_EXPORT int vipIPythonFontSize();

/// @brief Set/get the IPython shell style
VIP_GUI_EXPORT void vipSetIPythonStyle(const QString&);
VIP_GUI_EXPORT QString vipIPythonStyle();

/// @brief Returns the shared memory name used to communicate with the IPython process
VIP_GUI_EXPORT QString vipPyGlobalSharedMemoryName();

/// @brief VipIPythonShellProcess maps an IPython external console process.
/// It uses the script qtconsole_widget.py as well as Thermavip.py to communicate with the console.
class VIP_GUI_EXPORT VipIPythonShellProcess : public QProcess
{
	Q_OBJECT

	friend QString vipPyGlobalSharedMemoryName();

public:
	VipIPythonShellProcess(QObject* parent = nullptr);
	~VipIPythonShellProcess();

	/// @brief Set/get the timout value used to communicate with the IPython process.
	/// Default to 3s.
	void setTimeout(int milli_timeout);
	int timeout() const;

	/// @brief Tells if the process will be embedded within a QWidget.
	/// If true, the ipython console will first be hidden, and then shown maximized after 500ms.
	void setEmbedded(bool);
	bool embedded() const;

	/// @brief  Start the ipython console process and return its pid.
	///  The console is first hidden, and will be shown maximized 500 ms after this function returns.
	///  Returns 0 on error.
	///  
	///  Note that if the distant console is already running, this will restart it.
	/// 
	qint64 start(int font_size = -1, const QString & style = QString(), const QString& shared_memory_name = QString());

	/// @brief Returns the IPython process window ID
	qint64 windowId() const;

	/// @brief If the distant console has run at least once, returns the last used shared memory name.
	QString sharedMemoryName() const;

	/// @brief Send an object with given name to the IPython process.
	///  Returns true on success.
	bool sendObject(const QString& name, const QVariant& obj);

	/// @brief Retrieve a Python object from the distant console.
	/// Returns the object on success, a VipPyError object on error.
	QVariant retrieveObject(const QString& name);

	/// @brief Silently execute a Python code into the distant console.
	/// Returns true on success.
	bool execCode(const QString& code);

	/// @brief Push and execute a one line Python code into the ipython interpreter.
	/// Returns true on success.
	bool execLine(const QString& code);

	/// @brief Push and execute a one line Python code into the ipython interpreter.
	/// Returns true on success.
	/// Does not wait for the line being executed.
	bool execLineNoWait(const QString& code);

	/// @brief Stop current code being executed and restart interpreter
	bool restart();

	/// @brief Returns true if the current interpreter is currently executing python code.
	bool isRunningCode();

	/// @brief Returns the last error string
	QString lastError() const;

	/// @brief Set the IPython process style sheet (Qt format)
	void setStyleSheet(const QString& st);

private:
	/**Find a free shared memory name of the form 'Thermavip-X', where X is a numbed incremented at each trial*/
	static QString findNextMemoryName();
	static bool isFreeName(const QString& name);
	VIP_DECLARE_PRIVATE_DATA(d_data);
};


/// @brief Widget displaying an ipython console based on VipIPythonShellProcess
class VIP_GUI_EXPORT VipIPythonShellWidget : public QWidget
{
	Q_OBJECT

public:
	VipIPythonShellWidget(int font_size = -1, const QString &style = QString(), QWidget* parent = nullptr);
	~VipIPythonShellWidget();

	VipIPythonShellProcess* process() const;
	bool isRunning() const { return process()->state() == QProcess::Running; }

public Q_SLOTS:
	/// @brief Restart shell
	bool restart();

	/// @brief Restart full process with initial parameters
	bool restartProcess();

private Q_SLOTS:
	void focusChanged(QWidget* old, QWidget* now);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};




class VipIPythonTabWidget;

/**
*/
class VIP_GUI_EXPORT VipIPythonTabBar : public QTabBar
{
	Q_OBJECT
		Q_PROPERTY(QIcon closeIcon READ closeIcon WRITE setCloseIcon)
		Q_PROPERTY(QIcon restartIcon READ restartIcon WRITE setRestartIcon)
		Q_PROPERTY(QIcon hoverCloseIcon READ hoverCloseIcon WRITE setHoverCloseIcon)
		Q_PROPERTY(QIcon hoverRestartIcon READ hoverRestartIcon WRITE setHoverRestartIcon)
		Q_PROPERTY(QIcon selectedCloseIcon READ selectedCloseIcon WRITE setSelectedCloseIcon)
		Q_PROPERTY(QIcon selectedFloatIcon READ selectedRestartIcon WRITE setSelectedRestartIcon)

public:
	VipIPythonTabBar(VipIPythonTabWidget* parent);
	~VipIPythonTabBar();
	VipIPythonTabWidget* ipythonTabWidget() const;

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
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
}; 



class VIP_GUI_EXPORT VipIPythonTabWidget : public QTabWidget
{
	Q_OBJECT

public:
	VipIPythonTabWidget(QWidget* parent = nullptr);
	~VipIPythonTabWidget();

	VipIPythonShellWidget* widget(int) const;

public Q_SLOTS:
	void closeTab(int index);
	void addInterpreter();
private Q_SLOTS:
	void updateTab();
protected:
	virtual void closeEvent(QCloseEvent*);
private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};



class VIP_GUI_EXPORT VipIPythonToolWidget : public VipToolWidget
{
	Q_OBJECT

public:
	VipIPythonToolWidget(VipMainWindow* win);
	~VipIPythonToolWidget();

	VipIPythonTabWidget* widget() const;

private:
	VipIPythonTabWidget* m_tabs;
};

VIP_GUI_EXPORT VipIPythonToolWidget* vipGetIPythonToolWidget(VipMainWindow* win = nullptr);
