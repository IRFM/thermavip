/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipInterval.h"
#include <qalgorithms.h>

/// \brief Normalize the limits of the interval
///
/// If maxValue() < minValue() the limits will be inverted.
/// \return Normalized interval
///
/// \sa isValid(), inverted()
VipInterval VipInterval::normalized() const noexcept
{
	if (d_minValue > d_maxValue) {
		return inverted();
	}
	if (d_minValue == d_maxValue && d_borderFlags == ExcludeMinimum) {
		return inverted();
	}

	return *this;
}

/// Invert the limits of the interval
/// \return Inverted interval
/// \sa normalized()
VipInterval VipInterval::inverted() const noexcept
{
	BorderFlags borderFlags = IncludeBorders;
	if (d_borderFlags & ExcludeMinimum)
		borderFlags |= ExcludeMaximum;
	if (d_borderFlags & ExcludeMaximum)
		borderFlags |= ExcludeMinimum;

	return VipInterval(d_maxValue, d_minValue, borderFlags);
}

/// Test if a value is inside an interval
///
/// \param value Value
/// \return true, if value >= minValue() && value <= maxValue()
bool VipInterval::contains(vip_double value) const noexcept
{
	if (!isValid())
		return false;

	if (value < d_minValue || value > d_maxValue)
		return false;

	if (value == d_minValue && d_borderFlags & ExcludeMinimum)
		return false;

	if (value == d_maxValue && d_borderFlags & ExcludeMaximum)
		return false;

	return true;
}

//! Unite 2 intervals
VipInterval VipInterval::unite(const VipInterval& other) const noexcept
{
	// If one of the intervals is invalid return the other one.
	// If both are invalid return an invalid default interval
	if (!isValid()) {
		if (!other.isValid())
			return VipInterval();
		else
			return other;
	}
	if (!other.isValid())
		return *this;

	VipInterval united;
	BorderFlags flags = IncludeBorders;

	// minimum
	if (d_minValue < other.minValue()) {
		united.setMinValue(d_minValue);
		flags &= d_borderFlags & ExcludeMinimum;
	}
	else if (other.minValue() < d_minValue) {
		united.setMinValue(other.minValue());
		flags &= other.borderFlags() & ExcludeMinimum;
	}
	else // d_minValue == other.minValue()
	{
		united.setMinValue(d_minValue);
		flags &= (d_borderFlags & other.borderFlags()) & ExcludeMinimum;
	}

	// maximum
	if (d_maxValue > other.maxValue()) {
		united.setMaxValue(d_maxValue);
		flags &= d_borderFlags & ExcludeMaximum;
	}
	else if (other.maxValue() > d_maxValue) {
		united.setMaxValue(other.maxValue());
		flags &= other.borderFlags() & ExcludeMaximum;
	}
	else // d_maxValue == other.maxValue() )
	{
		united.setMaxValue(d_maxValue);
		flags &= d_borderFlags & other.borderFlags() & ExcludeMaximum;
	}

	united.setBorderFlags(flags);
	return united;
}

/// \brief Intersect 2 intervals
///
/// \param other VipInterval to be intersect with
/// \return Intersection
VipInterval VipInterval::intersect(const VipInterval& other) const noexcept
{
	if (!other.isValid() || !isValid())
		return VipInterval();

	VipInterval i1 = *this;
	VipInterval i2 = other;

	// swap i1/i2, so that the minimum of i1
	// is smaller then the minimum of i2

	if (i1.minValue() > i2.minValue()) {
		qSwap(i1, i2);
	}
	else if (i1.minValue() == i2.minValue()) {
		if (i1.borderFlags() & ExcludeMinimum)
			qSwap(i1, i2);
	}

	if (i1.maxValue() < i2.minValue()) {
		return VipInterval();
	}

	if (i1.maxValue() == i2.minValue()) {
		if (i1.borderFlags() & ExcludeMaximum || i2.borderFlags() & ExcludeMinimum) {
			return VipInterval();
		}
	}

	VipInterval intersected;
	BorderFlags flags = IncludeBorders;

	intersected.setMinValue(i2.minValue());
	flags |= i2.borderFlags() & ExcludeMinimum;

	if (i1.maxValue() < i2.maxValue()) {
		intersected.setMaxValue(i1.maxValue());
		flags |= i1.borderFlags() & ExcludeMaximum;
	}
	else if (i2.maxValue() < i1.maxValue()) {
		intersected.setMaxValue(i2.maxValue());
		flags |= i2.borderFlags() & ExcludeMaximum;
	}
	else // i1.maxValue() == i2.maxValue()
	{
		intersected.setMaxValue(i1.maxValue());
		flags |= i1.borderFlags() & i2.borderFlags() & ExcludeMaximum;
	}

	intersected.setBorderFlags(flags);
	return intersected;
}

/// \brief Unite this interval with the given interval.
///
/// \param other VipInterval to be united with
/// \return This interval
VipInterval& VipInterval::operator|=(const VipInterval& other) noexcept
{
	*this = *this | other;
	return *this;
}

/// \brief Intersect this interval with the given interval.
///
/// \param other VipInterval to be intersected with
/// \return This interval
VipInterval& VipInterval::operator&=(const VipInterval& other) noexcept
{
	*this = *this & other;
	return *this;
}

/// \brief Test if two intervals overlap
///
/// \param other VipInterval
/// \return True, when the intervals are intersecting
bool VipInterval::intersects(const VipInterval& other) const noexcept
{
	if (!isValid() || !other.isValid())
		return false;

	VipInterval i1 = *this;
	VipInterval i2 = other;

	// swap i1/i2, so that the minimum of i1
	// is smaller then the minimum of i2

	if (i1.minValue() > i2.minValue()) {
		qSwap(i1, i2);
	}
	else if (i1.minValue() == i2.minValue() && i1.borderFlags() & ExcludeMinimum) {
		qSwap(i1, i2);
	}

	if (i1.maxValue() > i2.minValue()) {
		return true;
	}
	if (i1.maxValue() == i2.minValue()) {
		return !((i1.borderFlags() & ExcludeMaximum) || (i2.borderFlags() & ExcludeMinimum));
	}
	return false;
}

/// Adjust the limit that is closer to value, so that value becomes
/// the center of the interval.
///
/// \param value Center
/// \return VipInterval with value as center
VipInterval VipInterval::symmetrize(vip_double value) const noexcept
{
	if (!isValid())
		return *this;

	const vip_double delta = qMax(qAbs(value - d_maxValue), qAbs(value - d_minValue));

	return VipInterval(value - delta, value + delta);
}

QRectF VipInterval::toRect(const QList<VipInterval>& intervals) noexcept
{
	if (intervals.size() == 2)
		return QRectF(intervals[0].minValue(), intervals[1].minValue(), intervals[0].width(), intervals[1].width());
	else
		return QRectF();
}

QList<VipInterval> VipInterval::fromRect(const QRectF& rect) noexcept
{
	return QList<VipInterval>() << VipInterval(rect.left(), rect.right()).normalized() << VipInterval(rect.top(), rect.bottom()).normalized();
}

/// Limit the interval, keeping the border modes
///
/// \param lowerBound Lower limit
/// \param upperBound Upper limit
///
/// \return Limited interval
VipInterval VipInterval::limited(vip_double lowerBound, vip_double upperBound) const noexcept
{
	if (!isValid() || lowerBound > upperBound)
		return VipInterval();

	vip_double minValue = qMax(d_minValue, lowerBound);
	minValue = qMin(minValue, upperBound);

	vip_double maxValue = qMax(d_maxValue, lowerBound);
	maxValue = qMin(maxValue, upperBound);

	return VipInterval(minValue, maxValue, d_borderFlags);
}

/// \brief Extend the interval
///
/// If value is below minValue(), value becomes the lower limit.
/// If value is above maxValue(), value becomes the upper limit.
///
/// extend() has no effect for invalid intervals
///
/// \param value Value
/// \return extended interval
///
/// \sa isValid()
VipInterval VipInterval::extend(vip_double value) const noexcept
{
	if (!isValid()) {
		return VipInterval(value, value, d_borderFlags);
	}

	return VipInterval(qMin(value, d_minValue), qMax(value, d_maxValue), d_borderFlags);
}

/// Extend an interval
///
/// \param value Value
/// \return Reference of the extended interval
///
/// \sa extend()
VipInterval& VipInterval::operator|=(vip_double value) noexcept
{
	*this = *this | value;
	return *this;
}
