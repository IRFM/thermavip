#ifndef VIP_PATH_DEVICE_H
#define VIP_PATH_DEVICE_H

#include <QPaintDevice>
#include <QPainterPath>

#include "VipGlobals.h"

/// \addtogroup Plotting
/// @{


class VIP_PLOTTING_EXPORT VipShapeDevice : public QPaintDevice
{
	QPainterPath m_path;
	QPaintEngine  *m_engine;
	int m_drawPrimitives;
public:

	enum DrawPrimitive {
		Text = 0x01,
		Polyline = 0x02,
		Points = 0x04,
		Pixmap = 0x08,
		All = Text|Polyline|Pixmap|Points
	};

	VipShapeDevice();
	virtual ~VipShapeDevice();
	virtual QPaintEngine *	paintEngine () const;

	const QPainterPath & shape() const;
	QPainterPath & shape();
	QPainterPath shape(double penWidth);

	void setDrawPrimitives(int);
	int drawPrimitives() const;

	void setDrawPrimitive(int, bool);
	bool testDrawPrimitive(int) const;

	void setExtractBoundingRectOnly(bool);
	bool extractBoundingRectOnly() const;

	void clear() {
		m_path = QPainterPath();
	}

protected:

	virtual int metric(PaintDeviceMetric metric) const;
};

/// @}
//end Plotting

#endif
