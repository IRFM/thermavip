/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipPie.h"

VipPie::VipPie(vip_double start_angle, vip_double end_angle, vip_double min_radius, vip_double max_radius, vip_double offset_to_center) noexcept
  : d_start_angle(start_angle)
  , d_end_angle(end_angle)
  , d_min_radius(min_radius)
  , d_max_radius(max_radius)
  , d_offset_to_center(offset_to_center)
{
}

VipPie::VipPie(const VipPolarCoordinate& top_left, const VipPolarCoordinate& bottom_right, vip_double offset_to_center) noexcept
  : d_start_angle(bottom_right.angle())
  , d_end_angle(top_left.angle())
  , d_min_radius(bottom_right.radius())
  , d_max_radius(top_left.radius())
  , d_offset_to_center(offset_to_center)
{
}

VipPie::VipPie(const QRectF& r, vip_double offset_to_center) noexcept
  : d_start_angle(r.left())
  , d_end_angle(r.right())
  , d_min_radius(r.top())
  , d_max_radius(r.bottom())
  , d_offset_to_center(offset_to_center)
{
}

bool VipPie::isEmpty() const noexcept
{
	return d_start_angle == 0 && d_end_angle == 0 && d_min_radius == 0 && d_max_radius == 0;
}

VipPie& VipPie::setAngleRange(vip_double start, vip_double end) noexcept
{
	d_start_angle = start;
	d_end_angle = end;
	return *this;
}

VipPie& VipPie::setStartAngle(vip_double start_angle) noexcept
{
	d_start_angle = start_angle;
	return *this;
}

vip_double VipPie::startAngle() const noexcept
{
	return d_start_angle;
}

VipPie& VipPie::setEndAngle(vip_double end_angle) noexcept
{
	d_end_angle = end_angle;
	return *this;
}

vip_double VipPie::endAngle() const noexcept
{
	return d_end_angle;
}
 
vip_double VipPie::sweepLength() const noexcept
{
	return d_end_angle - d_start_angle;
}

vip_double VipPie::meanAngle() const noexcept
{
	return (d_end_angle + d_start_angle) / 2.0;
}

void VipPie::setRadiusRange(vip_double start, vip_double end) noexcept
{
	d_min_radius = start;
	d_max_radius = end;
}

VipPie& VipPie::setMinRadius(vip_double min_radius) noexcept
{
	d_min_radius = min_radius;
	return *this;
}

vip_double VipPie::minRadius() const noexcept
{
	return d_min_radius;
}

VipPie& VipPie::setMaxRadius(vip_double max_radius) noexcept
{
	d_max_radius = max_radius;
	return *this;
}

vip_double VipPie::maxRadius() const noexcept
{
	return d_max_radius;
}

vip_double VipPie::radiusExtent() const noexcept
{
	return d_max_radius - d_min_radius;
}

vip_double VipPie::meanRadius() const noexcept
{
	return (d_max_radius + d_min_radius) / 2.0;
}

VipPie& VipPie::setOffsetToCenter(vip_double offset_to_center) noexcept
{
	d_offset_to_center = offset_to_center;
	return *this;
}

vip_double VipPie::offsetToCenter() const noexcept
{
	return d_offset_to_center;
}

VipPolarCoordinate VipPie::topLeft() const noexcept
{
	return VipPolarCoordinate(d_max_radius, d_end_angle);
}
VipPolarCoordinate VipPie::bottomRight() const noexcept
{ 
	return VipPolarCoordinate(d_min_radius, d_start_angle);
}

QRectF VipPie::rect() const noexcept
{
	return QRectF(QPointF(d_end_angle, d_max_radius), QPointF(d_start_angle, d_min_radius)).normalized();
}

VipPie VipPie::normalized() const noexcept
{
	VipPie res(*this);
	if (res.d_start_angle > res.d_end_angle)
		qSwap(res.d_start_angle, res.d_end_angle);
	if (res.d_min_radius > res.d_max_radius)
		qSwap(res.d_min_radius, res.d_max_radius);
	return res;
}

VipPie& VipPie::setMeanAngle(vip_double mean_angle) noexcept
{
	vip_double offset = mean_angle - meanAngle();
	d_start_angle += offset;
	d_end_angle += offset;
	return *this;
}

VipPie& VipPie::setMeanRadius(vip_double mean_radius) noexcept
{
	vip_double offset = mean_radius - meanRadius();
	d_min_radius += offset;
	d_max_radius += offset;
	return *this;
}

// make the the types declared with Q_DECLARE_METATYPE are registered
static int reg1 = qMetaTypeId<VipPie>();
static int reg2 = qMetaTypeId<VipPolarCoordinate>();
