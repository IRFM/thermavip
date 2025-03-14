#ifndef PYTHON_PLUGIN_H
#define PYTHON_PLUGIN_H

#include "VipOptions.h"
#include "VipIODevice.h"
#include "VipStandardWidgets.h"


class QLabel;
class QLineEdit;
class QToolButton;
class QRadioButton;
class VipPlotPlayer;
class VipTabEditor;

namespace detail
{
	/// Add a tool bar action to VipPlotPlayer objects in order to create new data fusion processings.
	class VIP_GUI_EXPORT PyCustomizePlotPlayer : public QObject
	{
		Q_OBJECT
	public:
		PyCustomizePlotPlayer(VipPlotPlayer* pl);
	};
}

/// @brief Python options page
class VIP_GUI_EXPORT VipPythonParameters : public VipPageOption
{
	Q_OBJECT

	QRadioButton* launchInLocal;
	QRadioButton* launchInIPython;

	QLabel* pythonPathLabel;
	VipFileName* pythonPath;

	QLabel* wdPathLabel;
	VipFileName* wdPath;
	QToolButton* openWD;

	QToolButton* openProcManager;

	QToolButton* openPythonData;
	QToolButton* openPythonDataScripts;

	QAction* actStartupCode;
	VipTabEditor* startupCode;
	VipTabEditor* style;
	QComboBox* styleBox;

	QToolButton* restart;

public:
	VipPythonParameters();

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

VIP_GUI_EXPORT VipPythonParameters* vipGetPythonParameters();


/// @brief Manage Python files when opening a file from Thermavip tool bar
class VIP_GUI_EXPORT PyFileHandler : public VipFileHandler
{
	Q_OBJECT
public:
	virtual bool open(const QString& path, QString* error);
	virtual QString fileFilters() const { return "Python files (*.py)"; }
	bool probe(const QString& filename, const QByteArray&) const
	{
		QString file(removePrefix(filename));
		return (QFileInfo(file).suffix().compare("py", Qt::CaseInsensitive) == 0 && QFileInfo(file).exists());
	}
};
VIP_REGISTER_QOBJECT_METATYPE(PyFileHandler*)

/// @brief Global Python manager, used to update Thermavip interface 
/// by providing Python related features.
class VIP_GUI_EXPORT VipPythonManager : public QObject
{
	Q_OBJECT
	VipPythonManager();

public:
	~VipPythonManager();
	static VipPythonManager * instance();

	void save(VipArchive&);
	void restore(VipArchive&);

private Q_SLOTS:
	void applyCurveFit();
	void applyPySignalFusion();
	void aboutToShowScripts();
	void scriptTriggered(QAction*);

private:
	static bool dispatchCurveFit(QAction* act, VipPlotPlayer* pl);
	static bool dispatchPySignalFusion(QAction* act, VipPlotPlayer* pl);

	QToolButton* showEditor;
};

#endif
