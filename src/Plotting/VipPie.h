#ifndef VIP_PIE_H
#define VIP_PIE_H

#include <QLineF>
#include <QMetaType>
#include <QRectF>

#include "VipDataType.h"
#include "VipGlobals.h"

/// \addtogroup Plotting
/// @{

/// @brief Simple class representing a polar coordinate with the angle being given in degrees
class VipPolarCoordinate
{
	vip_double d_radius;
	vip_double d_angle;

public:
	VipPolarCoordinate(vip_double radius = 0, vip_double angle = 0)
	  : d_radius(radius)
	  , d_angle(angle)
	{
	}

	VipPolarCoordinate(const QPointF& pt)
	  : d_radius(pt.x())
	  , d_angle(pt.y())
	{
	}

	vip_double radius() const { return d_radius; }
	vip_double angle() const { return d_angle; }

	void setRadius(vip_double radius) { d_radius = radius; }
	void setAngle(vip_double angle) { d_angle = angle; }

	QLineF line(const QPointF& center) const
	{
		QLineF res(center, center + QPointF(d_radius, 0));
		res.setAngle(d_angle + (d_radius > 0 ? 0 : 180));
		return res;
	}

	QPointF position(const QPointF& center) const { return line(center).p2(); }
};

/// @brief Class representing a Pie with he angle being given in degrees
class VIP_PLOTTING_EXPORT VipPie
{
	vip_double d_start_angle;
	vip_double d_end_angle;
	vip_double d_min_radius;
	vip_double d_max_radius;
	vip_double d_offset_to_center;

public:
	/// @brief Construct from a start and end angle, a start and end radius, and an offset to the center
	VipPie(vip_double start_angle = 0, vip_double end_angle = 0, vip_double min_radius = 0, vip_double max_radius = 0, vip_double offset_to_center = 0);
	/// @brief Construct from the top left and bottom right polar coordinates, and an optional offset to the center
	VipPie(const VipPolarCoordinate& top_left, const VipPolarCoordinate& bottom_right, vip_double offset_to_center = 0);
	/// @brief Construct from a rectangle, considering that the left and right values are angles and the top and bottom values are radiuses.
	VipPie(const QRectF& r, vip_double offset_to_center = 0);

	/// @brief Returns true if the pie is empty (all values to 0)
	bool isEmpty() const;

	/// @brief Set start and end angles
	VipPie& setAngleRange(vip_double start, vip_double end);

	/// @brief Set the start angle
	VipPie& setStartAngle(vip_double start_angle);
	vip_double startAngle() const;

	/// @brief Set the end angle
	VipPie& setEndAngle(vip_double end_angle);
	vip_double endAngle() const;

	/// @brief Returns the sweep length in degrees
	vip_double sweepLength() const;
	/// @brief  Returns themean angle
	vip_double meanAngle() const;
	/// @brief Set the mean angle, and keep the sweep length unchanged
	VipPie& setMeanAngle(vip_double mean_angle);

	/// @brief Set the min and max radius
	void setRadiusRange(vip_double start, vip_double end);

	/// @brief Set the min radius
	VipPie& setMinRadius(vip_double min_radius);
	vip_double minRadius() const;

	/// @brief Set the max radius
	VipPie& setMaxRadius(vip_double max_radius);
	vip_double maxRadius() const;

	/// @brief Returns the radius extent
	vip_double radiusExtent() const;
	/// @brief Returns the mean radius
	vip_double meanRadius() const;
	/// @brief Set the mean radius while keeping the radius extent unchanged
	VipPie& setMeanRadius(vip_double mean_radius);

	/// @brief Set the offset to the center
	VipPie& setOffsetToCenter(vip_double offset_to_center);
	vip_double offsetToCenter() const;

	/// @brief Returns the top left position in polar coordinates
	VipPolarCoordinate topLeft() const;
	/// @brief Returns the bottom right position in polar coordinates
	VipPolarCoordinate bottomRight() const;

	/// @brief Returns the pie as a rectangle, with the x axis representing angles and the y axis representing radiuses
	QRectF rect() const;
	/// @brief Normalize the pie
	VipPie normalized() const;
};

Q_DECLARE_METATYPE(VipPolarCoordinate);
Q_DECLARE_METATYPE(VipPie);

/// @}
// end Plotting

#endif
