#ifndef VIP_BORDER_ITEM_H
#define VIP_BORDER_ITEM_H

#include "VipAbstractScale.h"

/// \addtogroup Plotting
/// @{

VIP_PLOTTING_EXPORT QPoint sceneToScreenCoordinates(const QGraphicsScene * scene, const QPointF & pos);
VIP_PLOTTING_EXPORT QPointF screenToSceneCoordinates(const QGraphicsScene * scene, const QPoint & pos);

/// @brief Axis class that organizes itself around a rectangular area.
/// 
/// VipBorderItem is the base axis class for cartesian coordinate systems.
/// 
/// When inserted within a VipAbstractPlotArea, it organize itself around the plotting area
/// based on its alignment (Bottom, Top, Left or Right) and its canvas proximity.
/// The canvas proximity is used to organize axes on the same side and tells which
/// one will be the closest to the center of the plotting area (smallest canvas proximity).
/// 
/// VipBorderItem supports defining an intersection value with another axis using
/// VipBorderItem::setAxisIntersection() memeber.
/// 
class VIP_PLOTTING_EXPORT VipBorderItem : public VipAbstractScale
{
	Q_OBJECT

	friend class StoreGeometry;
	friend class ComputeBorderGeometry;
public:

	/// @brief Axis alignment
	enum Alignment
	{
		Bottom,
		Top,
		Left,
		Right
	};

	VipBorderItem(Alignment pos, QGraphicsItem * parent = 0);
	virtual ~VipBorderItem();

	void setExpandToCorners(bool expand);
	bool expandToCorners() const;

	virtual void setAxisIntersection(VipBorderItem * other, double other_value, Vip::ValueType type = Vip::Absolute);
	VipBorderItem * axisIntersection() const;
	Vip::ValueType axisIntersectionType() const;
	void disableAxisIntersection();
	double axisIntersectionValue() const;
	bool axisIntersectionEnabled() const;

	virtual void setAlignment( Alignment align);
	Alignment alignment() const;
	Qt::Orientation orientation() const;

	virtual void setCanvasProximity(int proximity);
	int canvasProximity() const;

	QRectF boundingRectNoCorners() const;
	void setBoundingRectNoCorners(const QRectF & r);

	QTransform globalSceneTransform() const;
	QTransform parentTransform() const;

	virtual double extentForLength(double length) const = 0;

	/// @brief Helper function, convert a distance in axis unit to a distance in item's unit (absolute value, either vertical or horizontal distance).
	/// Only works for linear scale.
	double axisRangeToItemUnit(vip_double) const;
	/// @brief Helper function, convert a distance in item unit to a distance in axis unit (absolute value, either vertical or horizontal distance).
	/// Only works for linear scale.
	vip_double itemRangeToAxisUnit(double) const;

	static int hscrollBarHeight(const QGraphicsView * view);
	static int vscrollBarWidth(const QGraphicsView * view);
	static QRectF visualizedSceneRect(const QGraphicsView * view);

protected:

	virtual QVariant 	itemChange ( GraphicsItemChange change, const QVariant & value );
	virtual void itemGeometryChanged(const QRectF& ) {}
	virtual void emitScaleDivNeedUpdate();
	virtual bool hasState(const QByteArray& state, bool enable) const; 

private:

	static double mapFromView(QGraphicsView * view, int length);
	static int mapToView(QGraphicsView * view, double length);

	class PrivateData;
	PrivateData * d_data;


};


class VipSpacerItem : public VipBorderItem
{
public:

	VipSpacerItem(Alignment pos, QGraphicsItem * parent = 0)
	:VipBorderItem(pos,parent),d_spacing(0)
	{}

	virtual QPointF position(double //value
) const{return QPointF();}
	virtual void layoutScale(){}

	void setSpacing(double spacing)
	{
		if(spacing != d_spacing){
			d_spacing = spacing;
			emitGeometryNeedUpdate();
		}
	}

	double spacing() const
	{
		return d_spacing;
	}

protected:

	virtual double extentForLength(double ) const{return d_spacing;}

private:

	double d_spacing;
};

/// @}
//end Plotting

#endif
