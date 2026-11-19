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

	/// @brief Load a startup code into the editor without installing it on the
	/// interpreter and without running it. Used for a code that comes from a session
	/// file: applying it stays an explicit action, through the button beside the
	/// editor.
	void setPendingStartupCode(const QString& code);

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



extern "C" {
/**
* C-style functions used to manipulate VipIODevice instances.
* These are used by the Python classes VipInputDevice and VipOutputDevice
*/
VIP_GUI_EXPORT uint64_t _py_create(const char* path, int mode);
VIP_GUI_EXPORT void _py_open(uint64_t, int mode);
VIP_GUI_EXPORT void _py_close(uint64_t);
VIP_GUI_EXPORT void _py_destroy(uint64_t);
VIP_GUI_EXPORT int _py_input_count(uint64_t);
VIP_GUI_EXPORT int _py_output_count(uint64_t);
VIP_GUI_EXPORT int _py_property_count(uint64_t);
VIP_GUI_EXPORT void _py_set_input_count(uint64_t, int);
VIP_GUI_EXPORT void _py_set_property_count(uint64_t, int);
VIP_GUI_EXPORT void _py_set_output_count(uint64_t, int);
VIP_GUI_EXPORT void* _py_input_name(uint64_t, int);
VIP_GUI_EXPORT void* _py_property_name(uint64_t, int);
VIP_GUI_EXPORT void* _py_output_name(uint64_t, int);
VIP_GUI_EXPORT void _py_set_input(uint64_t, int, void*);
VIP_GUI_EXPORT void _py_set_property(uint64_t, int, void*);
VIP_GUI_EXPORT void* _py_get_input(uint64_t, int);
VIP_GUI_EXPORT void* _py_get_output(uint64_t, int);
VIP_GUI_EXPORT void* _py_get_property(uint64_t addr, int i);
VIP_GUI_EXPORT void _py_apply(uint64_t);
VIP_GUI_EXPORT void _py_read(uint64_t, int64_t);
VIP_GUI_EXPORT int64_t _py_pos_to_time(uint64_t, int64_t);
VIP_GUI_EXPORT int64_t _py_time_to_pos(uint64_t, int64_t);
VIP_GUI_EXPORT int64_t _py_get_size(uint64_t);
VIP_GUI_EXPORT void* _py_time_window(uint64_t);
VIP_GUI_EXPORT int _py_device_type(uint64_t);
}

#endif
