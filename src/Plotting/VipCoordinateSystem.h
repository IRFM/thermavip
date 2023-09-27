#ifndef VIP_COORDINATE_SYSTEM_H
#define VIP_COORDINATE_SYSTEM_H

#include "VipScaleMap.h"
#include "VipPie.h"
#include "VipDataType.h"

#include <QSharedPointer>
#include <QTransform>
#include <QVector2D>
#include <QPointer>

#include <memory>
#define SHARED_PTR_NAMESPACE std


/// \addtogroup Plotting
/// @{

class VipAbstractScale;
class VipPlotItem;

/// Base class for coordinate system transformation.
/// A VipCoordinateSystem is used to translate axes coordinates to paint device coordinates.
class VIP_PLOTTING_EXPORT VipCoordinateSystem
{
	QList<VipAbstractScale*> d_axes;
	QTransform d_additional;

protected:

	void setAxes(const QList<VipAbstractScale*> & axes)
	{
		d_axes = axes;
	}

public:
	/// @brief Coordinate system type
	enum Type
	{
		Null,
		Cartesian,
		Polar,
		MonoAxis,
		UserType
	};

	/// @brief Construct from a list of axes (usually 2)
	VipCoordinateSystem(const QList<VipAbstractScale*> & axes)
	:d_axes(axes) {}

	virtual ~VipCoordinateSystem() {}

	const QList<VipAbstractScale*> & axes() const {return d_axes;}

	/// @brief Set an additional transform to convert from axes coordinates to paint device coordinates
	virtual void setAdditionalTransform(const QTransform & tr) {d_additional = tr;}
	const QTransform & additionalTransform() const {return d_additional;}

	/// @brief Returns the coordinate system type
	virtual int type() const = 0;
	/// @brief Transform axes coordinates to paint device coordinates
	virtual QPointF transform(const VipPoint &) const = 0;
	/// @brief Transform paint device coordinates to axes coordinates
	virtual VipPoint invTransform(const QPointF &) const = 0;
	/// @brief Returns the plotting area defined by this coordinate system and used to clip the drawing of given plot item
	virtual QPainterPath clipPath(const VipPlotItem *) const = 0;
	/// @brief Returns a deep copy of the coordinate system
	virtual VipCoordinateSystem * copy() const = 0;

	QPointF transform(vip_double c1, vip_double c2) const { return this->transform(VipPoint(c1, c2)); }
	virtual QPolygonF transform(const QRectF & r) const;
	virtual QVector<QPointF> transform(const VipPointVector & polygon) const;
	virtual QVector<QPointF> transform(const QVector<QPointF> & polygon) const;
	virtual QVector<QPointF> transform(const QVector<QPoint> & polygon) const;

	virtual QPainterPath transform(const QPainterPath & path, const QRectF & bounding_rect = QRectF()) const;
	QRectF transformRect(const QRectF & r) const;

	virtual VipPointVector invTransform(const QRectF & r) const;
	virtual VipPointVector invTransform(const VipPointVector & polygon) const;
	virtual VipPointVector invTransform(const  QVector<QPointF> & polygon) const;
	virtual QRectF invTransformRect(const QRectF & r) const;


	static QTransform changeCoordinateSystem(const QPointF & origin, const QVector2D & x, const QVector2D & y);
	static QTransform changeCoordinateSystem(const QPointF & origin_x, const QVector2D & x, const QPointF & origin_y, const QVector2D & y);

};

typedef SHARED_PTR_NAMESPACE::shared_ptr<VipCoordinateSystem> VipCoordinateSystemPtr;


/// Invariant coordinate system
class VipNullCoordinateSystem : public VipCoordinateSystem
{
public:

	VipNullCoordinateSystem(const QList<VipAbstractScale*> & axes)
	:VipCoordinateSystem(axes) {}

	virtual QPointF transform(const VipPoint & p) const {return p;}
	virtual VipPoint invTransform(const QPointF & p) const {return p;}
	virtual QPainterPath clipPath(const VipPlotItem *) const;
	virtual int type() const {return Null;}
	virtual VipNullCoordinateSystem * copy() const {return new VipNullCoordinateSystem(axes());}
};


/// Transform from cartesian coordinate system to paint device system
class VIP_PLOTTING_EXPORT  VipCartesianSystem : public VipCoordinateSystem
{
public:

	VipCartesianSystem(const QList<VipAbstractScale*> & axes);

	virtual void setAdditionalTransform(const QTransform & tr);
	virtual QPointF transform(const VipPoint &) const;
	virtual VipPoint invTransform(const QPointF &) const;
	virtual QPainterPath clipPath(const VipPlotItem *) const;
	virtual int type() const {return Cartesian;}
	virtual VipCartesianSystem * copy() const;

	//additional non mandatory functions
	virtual QPolygonF transform(const QRectF & r) const;
	virtual QVector<QPointF> transform(const VipPointVector & polygon) const;
	virtual QVector<QPointF> transform(const QVector<QPointF> & polygon) const;
	virtual QVector<QPointF> transform(const QVector<QPoint> & polygon) const;
	virtual VipPointVector invTransform(const QRectF & r) const;
	virtual VipPointVector invTransform(const VipPointVector & polygon) const;
	virtual VipPointVector invTransform(const  QVector<QPointF> & polygon) const;

private:

	VipScaleMap mx;
	VipScaleMap my;
	QTransform axis_tr;
	QTransform global_tr;
	QTransform inv_global_tr;
};


/// @brief Transform from polar coordinates to paint device coordinates
class VIP_PLOTTING_EXPORT VipPolarSystem : public VipCoordinateSystem
{
public:

	VipPolarSystem(const QList<VipAbstractScale*> & axes);

	virtual int type() const {return Polar;}
	virtual bool isRadialPolar() const {return true;}
	virtual bool isPolarRadial() const {return false;}

	virtual VipPolarCoordinate toPolar(const VipPoint &) const = 0;

	QPointF polarTransformToPoint(const VipPolarCoordinate &) const;
	VipPolarCoordinate polarTransform(const VipPolarCoordinate &) const;
	VipPolarCoordinate polarInvTransform(const VipPolarCoordinate &) const;
	VipPie polarTransform(const VipPie &) const;
	VipPie polarInvTransform(const VipPie &) const;

	VipPoint center() const {return d_center;}
	vip_double startRadius() const {return d_startRadius;}
	vip_double endRadius() const {return d_endRadius;}
	vip_double startAngle() const {return d_start_angle;}
	vip_double endAngle() const {return d_end_angle;}
	vip_double sweepLength() const {return d_sweep_length;}
	vip_double arcLength() const {return d_arc_length;}
	VipPie pie() const {return VipPie(d_start_angle,d_end_angle,d_startRadius,d_endRadius);}

protected:

	VipScaleMap 	d_mradius;
	VipScaleMap 	d_mangle;
	VipPoint 		d_center;
	vip_double 		d_startRadius;
	vip_double 		d_endRadius;
	vip_double 		d_start_angle;
	vip_double 		d_end_angle;
	vip_double 		d_sweep_length;
	vip_double 		d_arc_length;
};


/// Transform polar coordinate system to paint device system, with the radius as first (x) coordinate and the angle as second (y) coordinate
class VIP_PLOTTING_EXPORT VipRadialPolarSystem : public VipPolarSystem
{
public:

	VipRadialPolarSystem(const QList<VipAbstractScale*> & axes);

	virtual VipPolarCoordinate toPolar(const VipPoint &) const;
	virtual QPointF transform(const VipPoint &) const;
	virtual VipPoint invTransform(const QPointF &) const;
	virtual QPainterPath clipPath(const VipPlotItem *) const;
	virtual VipRadialPolarSystem * copy() const;
};
/// Transform polar coordinate system to paint device system, with the angle as first (x) coordinate and the radius as second (y) coordinate
class VIP_PLOTTING_EXPORT VipPolarRadialSystem : public VipPolarSystem
{
public:

	VipPolarRadialSystem(const QList<VipAbstractScale*> & axes);
	virtual bool isRadialPolar() const {return false;}
	virtual bool isPolarRadial() const {return true;}
	virtual VipPolarCoordinate toPolar(const VipPoint &) const;
	virtual QPointF transform(const VipPoint &) const;
	virtual VipPoint invTransform(const QPointF &) const;
	virtual QPainterPath clipPath(const VipPlotItem *) const;
	virtual VipPolarRadialSystem * copy() const;
};



class VIP_PLOTTING_EXPORT  VipMonoAxisSystem : public VipCoordinateSystem
{
public:

	VipMonoAxisSystem(const QList<VipAbstractScale*> & axes);

	virtual QPointF transform(const VipPoint &) const;
	virtual VipPoint invTransform(const QPointF &) const;
	virtual QPainterPath clipPath(const VipPlotItem *) const;
	virtual int type() const {return MonoAxis;}
	virtual VipMonoAxisSystem * copy() const;
};


/// @brief Coordinate system handler, used to build custom coordinate systems
class VIP_PLOTTING_EXPORT VipHandleCoordinateSystem
{
public:
	virtual ~VipHandleCoordinateSystem() {}
	virtual int type() const = 0;
	virtual VipCoordinateSystem * build(const QList<VipAbstractScale*> & axes, int type) const = 0;
};

VIP_PLOTTING_EXPORT void vipRegisterCoordinateSystem(VipHandleCoordinateSystem * );
VIP_PLOTTING_EXPORT VipCoordinateSystem * vipBuildCoordinateSystem(const QList<VipAbstractScale*> & axes, int type);

/// @}
//end Plotting

#endif
