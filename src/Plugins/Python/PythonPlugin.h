#ifndef PYTHON_PLUGIN_H
#define PYTHON_PLUGIN_H

#include "VipPlugin.h"
#include "VipOptions.h"
#include "VipIODevice.h"
#include "VipStandardWidgets.h"
#include "PyConfig.h"
#include "PyEditor.h"

#include <qwidget.h>


class QLabel;
class QLineEdit;
class QToolButton;
class QRadioButton;
class VipPlotPlayer;

class PythonParameters : public VipPageOption
{
	Q_OBJECT

	QRadioButton * local;
	QRadioButton * distant;

	QRadioButton* launchInLocal;
	QRadioButton* launchInIPython;

	QLabel * pythonPathLabel;
	VipFileName *pythonPath;
	
	QLabel * wdPathLabel;
	VipFileName *wdPath;
	QToolButton *openWD;

	QToolButton * openProcManager;

	QToolButton * openPythonData;
	QToolButton * openPythonDataScripts;

	QAction * actStartupCode;
	PyEditor * startupCode;
	PyEditor * style;
	QComboBox * styleBox;

	QToolButton *restart;

public:
	PythonParameters();

public Q_SLOTS:
	virtual void applyPage();
	virtual void updatePage();
private Q_SLOTS:
	void openWorkingDirectory();
	void applyStartupCode();
	void restartInterpreter();
	void changeStyle();
	void openManager();
	void _openPythonData();
	void _openPythonDataScripts();
};

PythonParameters * GetPythonParameters();


/**
Add a tool bar action to VipPlotPlayer objects in order to create new data fusion processings.
*/
class PyPlotPlayer : public QObject
{
	Q_OBJECT
public:
	PyPlotPlayer(VipPlotPlayer * pl);
};


/**
Manage Python files when opening a file from Thermavip tool bar
*/
class PyFileHandler : public VipFileHandler
{
	Q_OBJECT
public:
	virtual bool open(const QString & path, QString * error);
	virtual QString fileFilters() const { return "Python files (*.py)"; }
	bool probe(const QString &filename, const QByteArray & ) const
	{
		QString file(removePrefix(filename));
		return (QFileInfo(file).suffix().compare("py", Qt::CaseInsensitive) == 0 && QFileInfo(file).exists());
	}
};
VIP_REGISTER_QOBJECT_METATYPE(PyFileHandler*)


class PYTHON_EXPORT PythonInterface : public QObject, public VipPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "thermadiag.thermavip.VipPluginInterface")
	Q_INTERFACES(VipPluginInterface)
public:

	virtual LoadResult load();
	virtual QByteArray pluginVersion() { return "5.0.0"; }
	virtual void unload();
	virtual QString author() {return "Victor Moncada (victor.moncada@cea.fr)";}
	virtual QString description() {return "Provides a small Python environment";}
	virtual QString link() {return QString();}
	virtual void save(VipArchive &);
	virtual void restore(VipArchive &);

private Q_SLOTS:
	void applyCurveFit();
	void applyPySignalFusion();
	void aboutToShowScripts();
	void scriptTriggered(QAction*);
private:
	static bool dispatchCurveFit(QAction * act, VipPlotPlayer * pl);
	static bool dispatchPySignalFusion(QAction * act, VipPlotPlayer * pl);

	QToolButton * showEditor;
};

#endif
