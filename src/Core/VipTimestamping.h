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

#ifndef VIP_TIMESTAMPING_H
#define VIP_TIMESTAMPING_H

#include <QList>
#include <QMap>
#include <QPair>
#include <QTransform>

#include <limits>

#include "VipConfig.h"

/// \addtogroup Core
/// @{

/// The standard value for an invalid position
static constexpr qint64 VipInvalidPosition = -std::numeric_limits<qint64>::max();
/// The standard value for an invalid time
static constexpr qint64 VipInvalidTime = -std::numeric_limits<qint64>::max();
/// The minimum possible time, which is considered as -infinite
static constexpr qint64 VipMinTime = VipInvalidTime + 1;
/// The maximum possible time, which is considered as infinite
static constexpr qint64 VipMaxTime = std::numeric_limits<qint64>::max();

/// Standard type to represent a time range
typedef QPair<qint64, qint64> VipTimeRange;
/// Standard type to represent a list of time ranges
typedef QList<VipTimeRange> VipTimeRangeList;
/// A vector of time range
typedef QVector<VipTimeRange> VipTimeRangeVector;
/// Standard type to represent time transformations
typedef QMap<VipTimeRange, VipTimeRange> VipTimeRangeTransforms;
/// Standard type to represents timestamps
typedef QVector<qint64> VipTimestamps;

/// The standard value for invalid time range
const VipTimeRange VipInvalidTimeRange = VipTimeRange(VipInvalidTime, VipInvalidTime);

namespace Vip
{
	/// \enum Order
	/// \brief Order of a range
	enum Order
	{
		Descending, //! Order from the maximum to the minimum time
		Ascending   //! Order from the minimum to the maximum time
	};

}

/// Comparison operator for VipTimeRange
VIP_ALWAYS_INLINE bool operator<(const VipTimeRange& t1, const VipTimeRange& t2)
{
	if (t1.first < t2.first)
		return true;
	else if (t1.first == t2.first)
		return t1.second < t2.second;
	else
		return false;
}

/// Retruns true if a value is inside given time range
VIP_ALWAYS_INLINE bool vipIsInside(const VipTimeRange& pair, qint64 val)
{
	if (pair.first < pair.second)
		return val >= pair.first && val <= pair.second;
	else
		return val <= pair.first && val >= pair.second;
}

/// Returns true if a value is inside given time range list
VIP_CORE_EXPORT bool vipIsInside(const VipTimeRangeList& lst, qint64 val);

/// Returns true if a time range is valid
VIP_CORE_EXPORT bool vipIsValid(const VipTimeRange& range);

/// Returns the intersection of r1 and r2.
/// Returns VipInvalidTimeRange if the intersection is null or if one of the time range is invalid.
VIP_CORE_EXPORT VipTimeRange vipIntersectRange(const VipTimeRange& r1, const VipTimeRange& r2);
/// Returns the union of r1 and r2.
/// Returns VipInvalidTimeRange if one of the time range is invalid.
VIP_CORE_EXPORT VipTimeRange vipUnionRange(const VipTimeRange& r1, const VipTimeRange& r2);

/// Returns range.second - range.first ***
VIP_ALWAYS_INLINE qint64 vipRangeWidth(const VipTimeRange& range)
{
	return range.second - range.first;
}

/// Return the distance of a value to a time range. If closest is not nullptr, it is set to the closest valid point.
/// If given value is inside the time range, returned distance is 0 and closest is set to value.
VIP_CORE_EXPORT qint64 vipDistance(const VipTimeRange& pair, qint64 value, qint64* closest);

/// Return the distance of a value to a time range list.
/// If closest is not nullptr, it is set to the closest valid point.
/// If index is not nullptr, it is set to the closest valid index.
/// If given value is inside the time range list, returned distance is 0 and closest is set to value.
VIP_CORE_EXPORT qint64 vipDistance(const VipTimeRangeList& ranges, qint64 value, qint64* closest, int* index = nullptr);

/// \brief Reorder the pair according to the given order.
VIP_CORE_EXPORT VipTimeRange vipReorder(const VipTimeRange& pair, Vip::Order order);

/// \brief Merges 2 time range if they intersect in given order. If they don't, returns VipInvalidTimeRange.
/// If \a ok is not nullptr, it specifies if the two pairs has been merged.
VIP_CORE_EXPORT VipTimeRange vipMerge(const VipTimeRange& p1, const VipTimeRange& p2, Vip::Order order, bool* ok = nullptr);

/// Reorder a time range list in given order.
///  If \a merge_ranges is true, also merge the mergeable time ranges.
VIP_CORE_EXPORT VipTimeRangeList vipReorder(const VipTimeRangeList& lst, Vip::Order order, bool merge_ranges);

/// Returns the bound (minimum and maximum values) of a time range list
VIP_CORE_EXPORT VipTimeRange vipBounds(const VipTimeRangeList& lst);

/// Clamp given time range list based on \a first_time and \a last_time.
/// \a lst must be ordered in Ascending order, and \a first_time must be <= \a last_time.
VIP_CORE_EXPORT VipTimeRangeList vipClamp(const VipTimeRangeList& lst, qint64 first_time, qint64 last_time);

/// Replace \a VipMinTime by \a min_value and \a VipMaxTime by \a max_value in given time range
VIP_CORE_EXPORT VipTimeRange vipReplaceMinMaxTime(const VipTimeRange& range, qint64 min_value, qint64 max_value);

/// Crete a list of VipTimeRange based on ordered timestamps and a sampling time used to split time ranges.
VIP_CORE_EXPORT VipTimeRangeList vipToTimeRangeList(const QVector<qint64>& timestamps, qint64 sampling);

/// Create a time range from a string representation.
///  The string must follow the rule of 'printing pages':
///  - The string '4-7' is interpreted as the range [4,7]
///  - The string '4-' is interpreset as [4,VipMaxTime]
///  - The string '-7' is interpreted as [VipMinTime,7]
///  - The string '-' is interpreted as [VipMinTime,VipMaxTime]
///  - The string '!-' is interpreted as [VipMaxTime,VipMinTime]
///  - The string '4' is interpreted as [4,4]
VIP_CORE_EXPORT VipTimeRange vipToTimeRange(const QString&, bool* ok = nullptr);

/// Create a time range list from a string representation.
///  A time range follow the rules of #vipToTimeRange. Each time range are separated by a comma.
VIP_CORE_EXPORT VipTimeRangeList vipToTimeRangeList(const QString&, bool* ok = nullptr);

/// Convert a VipTimeRange into a string representation that can be read back through vipToTimeRange()
VIP_CORE_EXPORT QString vipTimeRangeToString(const VipTimeRange& range);

/// Convert a VipTimeRangeList into a string representation that can be read back through vipToTimeRangeList()
VIP_CORE_EXPORT QString vipTimeRangeListToString(const VipTimeRangeList& range);

/// \a VipTimestampingFilter represent time transformations.
/// A time transformation is a linear transformation computed from an input time range list and an output time range list of the same size.
/// The time transformations are set with #VipTimestampingFilter::setTransforms.
/// Use #VipTimestampingFilter::transform to transform a time value and #VipTimestampingFilter::invTransform to revert back the time.
class VIP_CORE_EXPORT VipTimestampingFilter
{
	typedef QPair<double, double> LinearTransform;
	typedef QMap<VipTimeRange, LinearTransform> LinearTransformHelper;

	VipTimeRangeList m_inputTimeRange;
	VipTimeRangeList m_outputTimeRange;
	VipTimeRangeTransforms m_transforms;
	VipTimeRangeTransforms m_validTransforms;
	LinearTransformHelper m_helper;
	LinearTransformHelper m_invHelper;

public:
	/// Reset the time filter
	void reset();
	/// Returns true is the time filter is empty. If empty you will have transform(time) == time and invTransform(time) == time.
	bool isEmpty() const;

	/// Set the time transformations by applying a QTransform to the input time range list.
	///  \sa setInputTimeRangeList()
	void setTransforms(const QTransform& tr);
	/// If the time transformations are already set, apply an additional transformation (\a tr) to given time range index
	bool setTransform(const QTransform& tr, int index);
	/// Set the time transformations from a map of VipTimeRange (old time range) -> VipTimeRange (corresponding new time range).
	///  The input time range list must have been set first.
	void setTransforms(const VipTimeRangeTransforms&);
	/// Returns the transformations as set with #VipTimestampingFilter::setTransforms
	const VipTimeRangeTransforms& transforms() const;
	/// Returns the valid transforms.
	///  When setting the time transformations through #VipTimestampingFilter::setTransforms, you might give infinite times (VipMinTime and VipMaxTime).
	///  For instance, if you just want to reverse a time range list, you can set a transformation of [VipMinTime,VipMaxTime] -> [VipMaxTime,VipMinTime].
	///  Internally, VipMinTime and VipMaxTime values are replaces by the minimum and maximum values of the input time range, and validTransforms() reflect that.
	const VipTimeRangeTransforms& validTransforms() const;

	/// Set the input time list. A time transformation is only valid for a given input time range list.
	///  Setting the input time range list will reapply the previous transformations set with #VipTimestampingFilter::setTransforms.
	void setInputTimeRangeList(const VipTimeRangeList&);
	/// Returns the input time range list
	const VipTimeRangeList& inputTimeRangeList() const;
	/// Returns the output time range list. This computed by applying the transformations to the input time range list.
	const VipTimeRangeList& outputTimeRangeList() const;

	/// Transform a given time value. If \a inside is not nullptr, it is set to true if given time is inside the transformation range.
	qint64 transform(qint64, bool* inside = nullptr) const;
	/// Returns the inverse transform of given time.You should always have transform(invTransform(time)) == time.
	qint64 invTransform(qint64, bool* inside = nullptr) const;
};

Q_DECLARE_METATYPE(VipTimeRange)
Q_DECLARE_METATYPE(VipTimeRangeList)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(VipTimeRangeVector)
#endif
Q_DECLARE_METATYPE(VipTimestamps)
Q_DECLARE_METATYPE(VipTimeRangeTransforms)
Q_DECLARE_METATYPE(VipTimestampingFilter)

class QDataStream;
VIP_CORE_EXPORT QDataStream& operator<<(QDataStream&, const VipTimestampingFilter& filter);
VIP_CORE_EXPORT QDataStream& operator>>(QDataStream&, VipTimestampingFilter& filter);

/// @}
// end Core

#endif
