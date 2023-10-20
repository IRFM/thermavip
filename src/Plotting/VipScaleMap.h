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


#ifndef VIP_SCALE_MAP_H
#define VIP_SCALE_MAP_H

#include <QRectF>
#include <QTransform>

#include "VipValueTransform.h"

/// \addtogroup Plotting
/// @{

/// \brief A scale map
///
/// VipScaleMap offers transformations from the coordinate system
/// of a scale into the linear coordinate system of a paint device
/// and vice versa.
class VIP_PLOTTING_EXPORT VipScaleMap
{
public:
	VipScaleMap();
	VipScaleMap(const VipScaleMap&);

	~VipScaleMap();

	VipScaleMap& operator=(const VipScaleMap&);

	void setTransformation(VipValueTransform*);
	const VipValueTransform* transformation() const;

	void setPaintInterval(vip_double p1, vip_double p2);
	void setScaleInterval(vip_double s1, vip_double s2);

	vip_double distanceToOrigin(vip_double s) const;
	vip_double distanceToOriginInt64(qint64 s) const;

	vip_double invDistanceToOrigin(vip_double p) const;

	vip_double transform(vip_double s) const;
	vip_double invTransform(vip_double p) const;
	qint64 invTransformTime(vip_double p) const;

	vip_double p1() const;
	vip_double p2() const;

	vip_double s1() const;
	vip_double s2() const;

	vip_double pDist() const;
	vip_double sDist() const;

	bool isInverting() const;

private:
	void updateFactor();

	vip_double d_s1, d_s2; // scale interval boundaries
	vip_double d_p1, d_p2; // paint device interval boundaries

	vip_double d_cnv; // conversion factor
	vip_double d_abs_cnv;
	vip_double d_ts1;

	VipValueTransform* d_transform;
};

/// \return First border of the scale interval
inline vip_double VipScaleMap::s1() const
{
	return d_s1;
}

/// \return Second border of the scale interval
inline vip_double VipScaleMap::s2() const
{
	return d_s2;
}

/// \return First border of the paint interval
inline vip_double VipScaleMap::p1() const
{
	return d_p1;
}

/// \return Second border of the paint interval
inline vip_double VipScaleMap::p2() const
{
	return d_p2;
}

/// \return qwtAbs(p2() - p1())
inline vip_double VipScaleMap::pDist() const
{
	return qAbs(d_p2 - d_p1);
}

/// \return qwtAbs(s2() - s1())
inline vip_double VipScaleMap::sDist() const
{
	return qAbs(d_s2 - d_s1);
}

inline vip_double VipScaleMap::distanceToOrigin(vip_double s) const
{
	if (d_transform)
		s = d_transform->transform(s);

	return (s - d_ts1) * d_abs_cnv;
}
inline vip_double VipScaleMap::distanceToOriginInt64(qint64 s) const
{
	if (d_transform)
		return (d_transform->transform(s) - d_ts1) * d_abs_cnv;

	return (s - (qint64)d_ts1) * d_abs_cnv;
}

inline vip_double VipScaleMap::invDistanceToOrigin(vip_double p) const
{
	vip_double s = d_ts1 + (p) / d_abs_cnv;
	if (d_transform)
		s = d_transform->invTransform(s);

	return s;
}

/// Transform a point related to the scale interval into an point
/// related to the interval of the paint device
///
/// \param s Value relative to the coordinates of the scale
/// \return Transformed value
///
/// \sa invTransform()
inline vip_double VipScaleMap::transform(vip_double s) const
{
	if (d_transform)
		s = d_transform->transform(s);

	return d_p1 + (s - d_ts1) * d_cnv;
}

/// Transform an paint device value into a value in the
/// interval of the scale.
///
/// \param p Value relative to the coordinates of the paint device
/// \return Transformed value
///
/// \sa transform()
inline vip_double VipScaleMap::invTransform(vip_double p) const
{
	vip_double s = d_ts1 + (p - d_p1) / d_cnv;
	if (d_transform)
		s = d_transform->invTransform(s);

	return s;
}

inline qint64 VipScaleMap::invTransformTime(vip_double p) const
{
	qint64 s = qRound64(d_ts1) + qRound64((p - d_p1) / d_cnv);
	if (d_transform)
		s = qRound64(d_transform->invTransform(s));

	return s;
}

//! \return True, when ( p1() < p2() ) != ( s1() < s2() )
inline bool VipScaleMap::isInverting() const
{
	return ((d_p1 < d_p2) != (d_s1 < d_s2));
}

/// Initialize the map with a transformation
inline void VipScaleMap::setTransformation(VipValueTransform* transform)
{
	if (transform != d_transform) {
		if (d_transform)
			delete d_transform;
		d_transform = transform;
	}

	setScaleInterval(d_s1, d_s2);
}

//! Get the transformation
inline const VipValueTransform* VipScaleMap::transformation() const
{
	return d_transform;
}

/// \brief Specify the borders of the scale interval
/// \param s1 first border
/// \param s2 second border
/// \warning scales might be aligned to
/// transformation depending boundaries
inline void VipScaleMap::setScaleInterval(vip_double s1, vip_double s2)
{
	d_s1 = s1;
	d_s2 = s2;

	if (d_transform) {
		d_s1 = d_transform->bounded(d_s1);
		d_s2 = d_transform->bounded(d_s2);
	}

	updateFactor();
}

/// \brief Specify the borders of the paint device interval
/// \param p1 first border
/// \param p2 second border
inline void VipScaleMap::setPaintInterval(vip_double p1, vip_double p2)
{
	d_p1 = p1;
	d_p2 = p2;

	updateFactor();
}

inline void VipScaleMap::updateFactor()
{
	d_ts1 = d_s1;
	vip_double ts2 = d_s2;

	if (d_transform) {
		d_ts1 = d_transform->transform(d_ts1);
		ts2 = d_transform->transform(ts2);
	}

	d_cnv = 1.0;
	if (d_ts1 != ts2)
		d_cnv = (d_p2 - d_p1) / (ts2 - d_ts1);

	d_abs_cnv = qAbs(d_p2 - d_p1) / (ts2 - d_ts1);
}

/// @}
// end Plotting

#endif
