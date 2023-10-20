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

#ifndef VIP_INTERVAL_H
#define VIP_INTERVAL_H

#include <QList>
#include <QRectF>
#include <qmetatype.h>

#include "VipLongDouble.h"

/// \addtogroup DataType
/// @{

/// \brief A class representing an interval
///
/// The interval is represented by 2 doubles, the lower and the upper limit.
class VIP_DATA_TYPE_EXPORT VipInterval
{
public:
	/// Flag indicating if a border is included or excluded
	/// \sa setBorderFlags(), borderFlags()
	enum BorderFlag
	{
		//! Min/Max values are inside the interval
		IncludeBorders = 0x00,

		//! Min value is not included in the interval
		ExcludeMinimum = 0x01,

		//! Max value is not included in the interval
		ExcludeMaximum = 0x02,

		//! Min/Max values are not included in the interval
		ExcludeBorders = ExcludeMinimum | ExcludeMaximum
	};

	//! Border flags
	typedef QFlags<BorderFlag> BorderFlags;

	VipInterval();
	VipInterval(vip_double minValue, vip_double maxValue, BorderFlags = IncludeBorders);

	void setInterval(vip_double minValue, vip_double maxValue, BorderFlags = IncludeBorders);

	VipInterval normalized() const;
	VipInterval inverted() const;
	VipInterval limited(vip_double minValue, vip_double maxValue) const;

	bool operator==(const VipInterval&) const;
	bool operator!=(const VipInterval&) const;

	void setBorderFlags(BorderFlags);
	BorderFlags borderFlags() const;

	vip_double minValue() const;
	vip_double maxValue() const;

	vip_double width() const;

	void setMinValue(vip_double);
	void setMaxValue(vip_double);

	bool contains(vip_double value) const;

	bool intersects(const VipInterval&) const;
	VipInterval intersect(const VipInterval&) const;
	VipInterval unite(const VipInterval&) const;

	VipInterval operator|(const VipInterval&) const;
	VipInterval operator&(const VipInterval&) const;

	VipInterval& operator|=(const VipInterval&);
	VipInterval& operator&=(const VipInterval&);

	VipInterval extend(vip_double value) const;
	VipInterval operator|(vip_double) const;
	VipInterval& operator|=(vip_double);

	bool isValid() const;
	bool isNull() const;
	void invalidate();

	VipInterval symmetrize(vip_double value) const;

	static QRectF toRect(const QList<VipInterval>&);
	static QList<VipInterval> fromRect(const QRectF&);

private:
	vip_double d_minValue;
	vip_double d_maxValue;
	BorderFlags d_borderFlags;
};

typedef QList<VipInterval> IntervalList;
Q_DECLARE_TYPEINFO(VipInterval, Q_MOVABLE_TYPE);

/// \brief Default Constructor
///
/// Creates an invalid interval [0.0, -1.0]
/// \sa setInterval(), isValid()
inline VipInterval::VipInterval()
  : d_minValue(0.0)
  , d_maxValue(-1.0)
  , d_borderFlags(IncludeBorders)
{
}

/// Constructor
///
/// Build an interval with from min/max values
///
/// \param minValue Minimum value
/// \param maxValue Maximum value
/// \param borderFlags Include/Exclude borders
inline VipInterval::VipInterval(vip_double minValue, vip_double maxValue, BorderFlags borderFlags)
  : d_minValue(minValue)
  , d_maxValue(maxValue)
  , d_borderFlags(borderFlags)
{
}

/// Assign the limits of the interval
///
/// \param minValue Minimum value
/// \param maxValue Maximum value
/// \param borderFlags Include/Exclude borders
inline void VipInterval::setInterval(vip_double minValue, vip_double maxValue, BorderFlags borderFlags)
{
	d_minValue = minValue;
	d_maxValue = maxValue;
	d_borderFlags = borderFlags;
}

/// Change the border flags
///
/// \param borderFlags Or'd BorderMode flags
/// \sa borderFlags()
inline void VipInterval::setBorderFlags(BorderFlags borderFlags)
{
	d_borderFlags = borderFlags;
}

/// \return Border flags
/// \sa setBorderFlags()
inline VipInterval::BorderFlags VipInterval::borderFlags() const
{
	return d_borderFlags;
}

/// Assign the lower limit of the interval
///
/// \param minValue Minimum value
inline void VipInterval::setMinValue(vip_double minValue)
{
	d_minValue = minValue;
}

/// Assign the upper limit of the interval
///
/// \param maxValue Maximum value
inline void VipInterval::setMaxValue(vip_double maxValue)
{
	d_maxValue = maxValue;
}

//! \return Lower limit of the interval
inline vip_double VipInterval::minValue() const
{
	return d_minValue;
}

//! \return Upper limit of the interval
inline vip_double VipInterval::maxValue() const
{
	return d_maxValue;
}

/// A interval is valid when minValue() <= maxValue().
/// In case of VipInterval::ExcludeBorders it is true
/// when minValue() < maxValue()
///
/// \return True, when the interval is valid
inline bool VipInterval::isValid() const
{
	if ((d_borderFlags & ExcludeBorders) == 0)
		return d_minValue <= d_maxValue;
	else
		return d_minValue < d_maxValue;
}

/// \brief Return the width of an interval
///
/// The width of invalid intervals is 0.0, otherwise the result is
/// maxValue() - minValue().
///
/// \return VipInterval width
/// \sa isValid()
inline vip_double VipInterval::width() const
{
	return isValid() ? (d_maxValue - d_minValue) : 0.0;
}

/// \brief Intersection of two intervals
///
/// \param other VipInterval to intersect with
/// \return Intersection of this and other
///
/// \sa intersect()
inline VipInterval VipInterval::operator&(const VipInterval& other) const
{
	return intersect(other);
}

/// Union of two intervals
///
/// \param other VipInterval to unite with
/// \return Union of this and other
///
/// \sa unite()
inline VipInterval VipInterval::operator|(const VipInterval& other) const
{
	return unite(other);
}

/// \brief Compare two intervals
///
/// \param other VipInterval to compare with
/// \return True, when this and other are equal
inline bool VipInterval::operator==(const VipInterval& other) const
{
	return (d_minValue == other.d_minValue) && (d_maxValue == other.d_maxValue) && (d_borderFlags == other.d_borderFlags);
}
/// \brief Compare two intervals
///
/// \param other VipInterval to compare with
/// \return True, when this and other are not equal
inline bool VipInterval::operator!=(const VipInterval& other) const
{
	return (!(*this == other));
}

/// Extend an interval
///
/// \param value Value
/// \return Extended interval
/// \sa extend()
inline VipInterval VipInterval::operator|(vip_double value) const
{
	return extend(value);
}

//! \return true, if isValid() && (minValue() >= maxValue())
inline bool VipInterval::isNull() const
{
	return isValid() && d_minValue >= d_maxValue;
}

/// Invalidate the interval
///
/// The limits are set to interval [0.0, -1.0]
/// \sa isValid()
inline void VipInterval::invalidate()
{
	d_minValue = 0.0;
	d_maxValue = -1.0;
}

Q_DECLARE_OPERATORS_FOR_FLAGS(VipInterval::BorderFlags)
Q_DECLARE_METATYPE(VipInterval)

//! \brief A sample of the types (x1-x2, y) or (x, y1-y2)
class VIP_DATA_TYPE_EXPORT VipIntervalSample
{
public:
	VipIntervalSample();
	VipIntervalSample(vip_double, const VipInterval&);
	VipIntervalSample(vip_double value, vip_double min, vip_double max);

	bool operator==(const VipIntervalSample&) const;
	bool operator!=(const VipIntervalSample&) const;

	//! Value
	vip_double value;

	//! VipInterval
	VipInterval interval;
};

/// Constructor
/// The value is set to 0.0, the interval is invalid
inline VipIntervalSample::VipIntervalSample()
  : value(0.0)
{
}

//! Constructor
inline VipIntervalSample::VipIntervalSample(vip_double v, const VipInterval& intv)
  : value(v)
  , interval(intv)
{
}

//! Constructor
inline VipIntervalSample::VipIntervalSample(vip_double v, vip_double min, vip_double max)
  : value(v)
  , interval(min, max)
{
}

//! Compare operator
inline bool VipIntervalSample::operator==(const VipIntervalSample& other) const
{
	return value == other.value && interval == other.interval;
}

//! Compare operator
inline bool VipIntervalSample::operator!=(const VipIntervalSample& other) const
{
	return !(*this == other);
}

Q_DECLARE_METATYPE(VipIntervalSample)

/// @}
// end DataType

#endif
