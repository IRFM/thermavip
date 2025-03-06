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

	VipInterval() noexcept;
	VipInterval(vip_double minValue, vip_double maxValue, BorderFlags = IncludeBorders) noexcept;

	void setInterval(vip_double minValue, vip_double maxValue, BorderFlags = IncludeBorders) noexcept;

	VipInterval normalized() const noexcept;
	VipInterval inverted() const noexcept;
	VipInterval limited(vip_double minValue, vip_double maxValue) const noexcept;

	bool operator==(const VipInterval&) const noexcept;
	bool operator!=(const VipInterval&) const noexcept;

	void setBorderFlags(BorderFlags) noexcept;
	BorderFlags borderFlags() const noexcept;

	vip_double minValue() const noexcept;
	vip_double maxValue() const noexcept;

	vip_double width() const noexcept;

	void setMinValue(vip_double) noexcept;
	void setMaxValue(vip_double) noexcept;

	bool contains(vip_double value) const noexcept;

	bool intersects(const VipInterval&) const noexcept;
	VipInterval intersect(const VipInterval&) const noexcept;
	VipInterval unite(const VipInterval&) const noexcept;

	VipInterval operator|(const VipInterval&) const noexcept;
	VipInterval operator&(const VipInterval&) const noexcept;

	VipInterval& operator|=(const VipInterval&) noexcept;
	VipInterval& operator&=(const VipInterval&) noexcept;

	VipInterval extend(vip_double value) const noexcept;
	VipInterval operator|(vip_double) const noexcept;
	VipInterval& operator|=(vip_double) noexcept;

	bool isValid() const noexcept;
	bool isNull() const noexcept;
	void invalidate() noexcept;

	VipInterval symmetrize(vip_double value) const noexcept;

	static QRectF toRect(const QList<VipInterval>&) noexcept;
	static QList<VipInterval> fromRect(const QRectF&) noexcept;

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
VIP_ALWAYS_INLINE VipInterval::VipInterval() noexcept
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
VIP_ALWAYS_INLINE VipInterval::VipInterval(vip_double minValue, vip_double maxValue, BorderFlags borderFlags) noexcept
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
VIP_ALWAYS_INLINE void VipInterval::setInterval(vip_double minValue, vip_double maxValue, BorderFlags borderFlags) noexcept
{
	d_minValue = minValue;
	d_maxValue = maxValue;
	d_borderFlags = borderFlags;
}

/// Change the border flags
///
/// \param borderFlags Or'd BorderMode flags
/// \sa borderFlags()
VIP_ALWAYS_INLINE void VipInterval::setBorderFlags(BorderFlags borderFlags) noexcept
{
	d_borderFlags = borderFlags;
}

/// \return Border flags
/// \sa setBorderFlags()
VIP_ALWAYS_INLINE VipInterval::BorderFlags VipInterval::borderFlags() const noexcept
{
	return d_borderFlags;
}

/// Assign the lower limit of the interval
///
/// \param minValue Minimum value
VIP_ALWAYS_INLINE void VipInterval::setMinValue(vip_double minValue) noexcept
{
	d_minValue = minValue;
}

/// Assign the upper limit of the interval
///
/// \param maxValue Maximum value
VIP_ALWAYS_INLINE void VipInterval::setMaxValue(vip_double maxValue) noexcept
{
	d_maxValue = maxValue;
}

//! \return Lower limit of the interval
VIP_ALWAYS_INLINE vip_double VipInterval::minValue() const noexcept
{
	return d_minValue;
}

//! \return Upper limit of the interval
VIP_ALWAYS_INLINE vip_double VipInterval::maxValue() const noexcept
{
	return d_maxValue;
}

/// A interval is valid when minValue() <= maxValue().
/// In case of VipInterval::ExcludeBorders it is true
/// when minValue() < maxValue()
///
/// \return True, when the interval is valid
VIP_ALWAYS_INLINE bool VipInterval::isValid() const noexcept
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
VIP_ALWAYS_INLINE vip_double VipInterval::width() const noexcept
{
	return isValid() ? (d_maxValue - d_minValue) : 0.0;
}

/// \brief Intersection of two intervals
///
/// \param other VipInterval to intersect with
/// \return Intersection of this and other
///
/// \sa intersect()
VIP_ALWAYS_INLINE VipInterval VipInterval::operator&(const VipInterval& other) const noexcept
{
	return intersect(other);
}

/// Union of two intervals
///
/// \param other VipInterval to unite with
/// \return Union of this and other
///
/// \sa unite()
VIP_ALWAYS_INLINE VipInterval VipInterval::operator|(const VipInterval& other) const noexcept
{
	return unite(other);
}

/// \brief Compare two intervals
///
/// \param other VipInterval to compare with
/// \return True, when this and other are equal
VIP_ALWAYS_INLINE bool VipInterval::operator==(const VipInterval& other) const noexcept
{
	return (d_minValue == other.d_minValue) && (d_maxValue == other.d_maxValue) && (d_borderFlags == other.d_borderFlags);
}
/// \brief Compare two intervals
///
/// \param other VipInterval to compare with
/// \return True, when this and other are not equal
VIP_ALWAYS_INLINE bool VipInterval::operator!=(const VipInterval& other) const noexcept
{
	return (!(*this == other));
}

/// Extend an interval
///
/// \param value Value
/// \return Extended interval
/// \sa extend()
VIP_ALWAYS_INLINE VipInterval VipInterval::operator|(vip_double value) const noexcept
{
	return extend(value);
}

//! \return true, if isValid() && (minValue() >= maxValue())
VIP_ALWAYS_INLINE bool VipInterval::isNull() const noexcept
{
	return isValid() && d_minValue >= d_maxValue;
}

/// Invalidate the interval
///
/// The limits are set to interval [0.0, -1.0]
/// \sa isValid()
VIP_ALWAYS_INLINE void VipInterval::invalidate() noexcept
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
	/// Constructor
	/// The value is set to 0.0, the interval is invalid
	VIP_ALWAYS_INLINE VipIntervalSample() noexcept
	  : value(0.0)
	{
	}

	//! Constructor
	VIP_ALWAYS_INLINE VipIntervalSample(vip_double v, const VipInterval& intv) noexcept
	  : value(v)
	  , interval(intv)
	{
	}

	//! Constructor
	VIP_ALWAYS_INLINE VipIntervalSample(vip_double v, vip_double min, vip_double max) noexcept
	  : value(v)
	  , interval(min, max)
	{
	}

	//! Compare operator
	VIP_ALWAYS_INLINE bool operator==(const VipIntervalSample& other) const noexcept { return value == other.value && interval == other.interval; }

	//! Compare operator
	VIP_ALWAYS_INLINE bool operator!=(const VipIntervalSample& other) const noexcept { return !(*this == other); }

	//! Value
	vip_double value;

	//! VipInterval
	VipInterval interval;
};


Q_DECLARE_METATYPE(VipIntervalSample)

/// @}
// end DataType

#endif
