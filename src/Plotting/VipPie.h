/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
	VipPolarCoordinate(vip_double radius = 0, vip_double angle = 0) noexcept
	  : d_radius(radius)
	  , d_angle(angle)
	{
	}

	VipPolarCoordinate(const QPointF& pt) noexcept
	  : d_radius(pt.x())
	  , d_angle(pt.y())
	{
	}

	vip_double radius() const noexcept { return d_radius; }
	vip_double angle() const noexcept { return d_angle; }

	void setRadius(vip_double radius) noexcept { d_radius = radius; }
	void setAngle(vip_double angle) noexcept { d_angle = angle; }

	QLineF line(const QPointF& center) const noexcept
	{
		QLineF res(center, center + QPointF(d_radius, 0));
		res.setAngle(d_angle + (d_radius > 0 ? 0 : 180));
		return res;
	}

	QPointF position(const QPointF& center) const noexcept { return line(center).p2(); }
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
	VipPie(vip_double start_angle = 0, vip_double end_angle = 0, vip_double min_radius = 0, vip_double max_radius = 0, vip_double offset_to_center = 0) noexcept;
	/// @brief Construct from the top left and bottom right polar coordinates, and an optional offset to the center
	VipPie(const VipPolarCoordinate& top_left, const VipPolarCoordinate& bottom_right, vip_double offset_to_center = 0) noexcept;
	/// @brief Construct from a rectangle, considering that the left and right values are angles and the top and bottom values are radiuses.
	VipPie(const QRectF& r, vip_double offset_to_center = 0) noexcept;

	/// @brief Returns true if the pie is empty (all values to 0)
	bool isEmpty() const noexcept;

	/// @brief Set start and end angles
	VipPie& setAngleRange(vip_double start, vip_double end) noexcept;

	/// @brief Set the start angle
	VipPie& setStartAngle(vip_double start_angle) noexcept;
	vip_double startAngle() const noexcept;

	/// @brief Set the end angle
	VipPie& setEndAngle(vip_double end_angle) noexcept;
	vip_double endAngle() const noexcept;

	/// @brief Returns the sweep length in degrees
	vip_double sweepLength() const noexcept;
	/// @brief  Returns themean angle
	vip_double meanAngle() const noexcept;
	/// @brief Set the mean angle, and keep the sweep length unchanged
	VipPie& setMeanAngle(vip_double mean_angle) noexcept;

	/// @brief Set the min and max radius
	void setRadiusRange(vip_double start, vip_double end) noexcept;

	/// @brief Set the min radius
	VipPie& setMinRadius(vip_double min_radius) noexcept;
	vip_double minRadius() const noexcept;

	/// @brief Set the max radius
	VipPie& setMaxRadius(vip_double max_radius) noexcept;
	vip_double maxRadius() const noexcept;

	/// @brief Returns the radius extent
	vip_double radiusExtent() const noexcept;
	/// @brief Returns the mean radius
	vip_double meanRadius() const noexcept;
	/// @brief Set the mean radius while keeping the radius extent unchanged
	VipPie& setMeanRadius(vip_double mean_radius) noexcept;

	/// @brief Set the offset to the center
	VipPie& setOffsetToCenter(vip_double offset_to_center) noexcept;
	vip_double offsetToCenter() const noexcept;

	/// @brief Returns the top left position in polar coordinates
	VipPolarCoordinate topLeft() const noexcept;
	/// @brief Returns the bottom right position in polar coordinates
	VipPolarCoordinate bottomRight() const noexcept;

	/// @brief Returns the pie as a rectangle, with the x axis representing angles and the y axis representing radiuses
	QRectF rect() const noexcept;
	/// @brief Normalize the pie
	VipPie normalized() const noexcept;
};

Q_DECLARE_METATYPE(VipPolarCoordinate);
Q_DECLARE_METATYPE(VipPie);

/// @}
// end Plotting

#endif
