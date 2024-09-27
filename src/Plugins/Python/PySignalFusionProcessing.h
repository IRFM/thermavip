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
	Q_CLASSINFO("description",
		    "Apply a python script based on given input signals.\n"
		    "This processing only takes 1D + time signals as input, and create a new output using\n"
		    "a Python script for the x components and the y components.")
	Q_CLASSINFO("category", "Miscellaneous")
public:
	PySignalFusionProcessing(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return DisplayOnSameSupport; }
	virtual QVariant initializeProcessing(const QVariant& v);
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return qMetaTypeId<VipPointVector>() == v.userType(); }
	virtual bool useEventLoop() const { return true; }
	bool registerThisProcessing(const QString& category, const QString& name, const QString& description, bool overwrite = true);

protected:
	virtual void mergeData(int, int);
};

VIP_REGISTER_QOBJECT_METATYPE(PySignalFusionProcessing*)
typedef QSharedPointer<PySignalFusionProcessing> PySignalFusionProcessingPtr;
Q_DECLARE_METATYPE(PySignalFusionProcessingPtr);
