#pragma once



#include "VipStandardProcessing.h"
#include "PyProcessing.h"

#include <qwidget.h>

/**
Data fusion processing that takes as input multiple VipPointVector signals,
and applies a Python processing to the x components and y components.

Within these Python scripts, 'x' and 'y' variables refer to the output x and y values,
and variables 'x0', 'x1',...,'y0','y1',... refer to the input signals x and y.

The processing applies a different Python script to the x and y components.
*/
class PySignalFusionProcessing : public PyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty x_algo)
	VIP_IO(VipProperty y_algo)
	VIP_IO(VipProperty output_title)
	VIP_IO(VipProperty output_unit)
	VIP_IO(VipProperty output_x_unit)
	Q_CLASSINFO("description", "Apply a python script based on given input signals.\n"
		"This processing only takes 1D + time signals as input, and create a new output using\n"
		"a Python script for the x components and the y components.")
	Q_CLASSINFO("category", "Miscellaneous")
public:
	PySignalFusionProcessing(QObject * parent = nullptr);
	virtual DisplayHint displayHint() const { return DisplayOnSameSupport; }
	virtual QVariant initializeProcessing(const QVariant & v);
	virtual bool acceptInput(int /*index*/, const QVariant & v) const { return qMetaTypeId<VipPointVector>() == v.userType(); }
	virtual bool useEventLoop() const { return true; }
	bool registerThisProcessing(const QString & category, const QString & name, const QString & description, bool overwrite = true);

protected:
	virtual void applyPyProcessing(int, int);
};

VIP_REGISTER_QOBJECT_METATYPE(PySignalFusionProcessing*)
typedef QSharedPointer<PySignalFusionProcessing> PySignalFusionProcessingPtr;
Q_DECLARE_METATYPE(PySignalFusionProcessingPtr);

class VipPlotPlayer;
class PyApplyToolBar;
class QPushButton;
class QToolButton;

/**
Editor for PySignalFusionProcessing
*/
class PySignalFusionProcessingEditor : public QWidget
{
	Q_OBJECT

public:
	PySignalFusionProcessingEditor(QWidget *parent = nullptr);
	~PySignalFusionProcessingEditor();

	void setPlotPlayer(VipPlotPlayer * player);
	VipPlotPlayer * plotPlayer() const;

	void setPySignalFusionProcessing(PySignalFusionProcessing * proc);
	PySignalFusionProcessing * getPySignalFusionProcessing() const;

	PyApplyToolBar * buttons() const;
	
public Q_SLOTS:
	bool updateProcessing();
	void updateWidget();
	bool apply();
	void nameTriggered(QAction* a);
	void registerProcessing();
	void manageProcessing();
	void showError(const QPoint & pos, const QString & error);
	void showErrorDelayed(const QPoint & pos, const QString & error);
private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};


