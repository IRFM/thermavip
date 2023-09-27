#ifndef VIP_PLOT_MIME_DATA_H
#define VIP_PLOT_MIME_DATA_H

#include <QMimeData>
#include <QList>

#include "VipGlobals.h"
#include "VipCoordinateSystem.h"

/// \addtogroup Plotting
/// @{


class VipPlotItem;

/// @brief Base class for mime data involving VipPlotItem objects
/// 
/// VipPlotMimeData is a QMimeData used to drag and drop VipPlotItem objects.
/// VipPlotItem::startDragging() member internally create a VipPlotMimeData that might be dropped on any other VipPlotItem object.
class VIP_PLOTTING_EXPORT VipPlotMimeData : public QMimeData
{
	Q_OBJECT

public:

	VipPlotMimeData();
	virtual ~VipPlotMimeData();

	/// @brief Set the VipPlotItem objects to drag & drop
	void setPlotData(const QList<VipPlotItem*> &);

	/// @brief Returns the VipPlotItem objects to drop on a specific target 
	/// @param drop_target drop VipPlotItem target, might be null
	/// @param drop_widget drop widget, might be null
	/// @return dropped/created VipPlotItem objects
	virtual QList<VipPlotItem*> plotData(VipPlotItem * drop_target, QWidget * drop_widget) const;

	/// @brief Returns the coordinate system of carried VipPlotItem objects
	virtual VipCoordinateSystem::Type coordinateSystemType() const;

private:

	class PrivateData;
	PrivateData * d_data;
};

/// @}
//end Plotting

#endif
