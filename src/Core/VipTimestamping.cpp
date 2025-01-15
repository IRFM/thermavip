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

#include "VipTimestamping.h"

#include <QStringList>
#include <set>

bool vipIsInside(const VipTimeRangeList& lst, qint64 val)
{
	for (int i = 0; i < lst.size(); ++i) {
		if (vipIsInside(lst[i], val))
			return true;
	}

	return false;
}

bool vipIsValid(const VipTimeRange& range)
{
	if (range.first == VipInvalidTime || range.second == VipInvalidTime || range.second < range.first)
		return false;
	return true;
}

VipTimeRange vipIntersectRange(const VipTimeRange& r1, const VipTimeRange& r2)
{
	if (!vipIsValid(r1) || !vipIsValid(r2))
		return VipInvalidTimeRange;

	if (r1.second < r2.first || r1.first > r2.second)
		return VipInvalidTimeRange;

	return VipTimeRange(std::max(r1.first, r2.first), std::min(r1.second, r2.second));
}

VipTimeRange vipUnionRange(const VipTimeRange& r1, const VipTimeRange& r2)
{
	if (!vipIsValid(r1) || !vipIsValid(r2))
		return VipInvalidTimeRange;

	return VipTimeRange(std::min(r1.first, r2.first), std::max(r1.second, r2.second));
}

qint64 vipDistance(const VipTimeRange& pair, qint64 val, qint64* closest)
{
	if (pair.first < pair.second) {
		if (val > pair.second) {
			if (closest)
				*closest = pair.second;
			return val - pair.second;
		}
		else if (val < pair.first) {
			if (closest)
				*closest = pair.first;
			return pair.first - val;
		}
		else {
			if (closest)
				*closest = val;
			return 0;
		}
	}
	else {
		if (val < pair.second) {
			if (closest)
				*closest = pair.second;
			return pair.second - val;
		}
		else if (val > pair.first) {
			if (closest)
				*closest = pair.first;
			return val - pair.first;
		}
		else {
			if (closest)
				*closest = val;
			return 0;
		}
	}
	VIP_UNREACHABLE();
	//	return 0;
}

qint64 vipDistance(const VipTimeRangeList& ranges, qint64 value, qint64* closest, int* index)
{
	qint64 dist = VipMaxTime;
	qint64 close;

	if (index)
		*index = -1;

	for (int i = 0; i < ranges.size(); ++i) {
		qint64 tmp_dist = vipDistance(ranges[i], value, &close);
		if (tmp_dist == 0) {
			if (closest)
				*closest = value;
			if (index)
				*index = i;
			return 0;
		}
		else if (tmp_dist < dist) {
			dist = tmp_dist;
			if (closest)
				*closest = close;
			if (index)
				*index = i;
		}
	}

	return dist;
}

VipTimeRange vipMerge(const VipTimeRange& p1, const VipTimeRange& p2, Vip::Order order, bool* ok)
{
	if (ok)
		*ok = true;

	// vipReorder the pairs in ascending order
	VipTimeRange tp1 = vipReorder(p1, order);
	VipTimeRange tp2 = vipReorder(p2, order);
	VipTimeRange res = VipInvalidTimeRange;

	// find intersection
	if (order != Vip::Descending) {
		if ((tp1.first >= tp2.first && tp1.first <= tp2.second) || (tp2.first >= tp1.first && tp2.first <= tp1.second)) {
			res = VipTimeRange(qMin(tp1.first, tp2.first), qMax(tp1.second, tp2.second));
		}
		else {
			if (ok)
				*ok = false;
		}
	}
	else {
		if ((tp1.first <= tp2.first && tp1.first >= tp2.second) || (tp2.first <= tp1.first && tp2.first >= tp1.second)) {
			res = VipTimeRange(qMax(tp1.first, tp2.first), qMin(tp1.second, tp2.second));
		}
		else {
			if (ok)
				*ok = false;
		}
	}

	return res;
}

VipTimeRange vipReorder(const VipTimeRange& pair, Vip::Order order)
{
	VipTimeRange res(pair);

	if (order == Vip::Descending) {
		if (res.first < res.second)
			std::swap(res.first, res.second);
	}
	else {
		if (res.first > res.second)
			std::swap(res.first, res.second);
	}

	return res;
}

VipTimeRangeList vipReorder(const VipTimeRangeList& lst, Vip::Order order, bool merge_ranges)
{
	if (!lst.size())
		return VipTimeRangeList();

	VipTimeRangeList res;

	// first, reorder in ascending order in a std::set, remove invalid times
	std::set<VipTimeRange> reordered;
	for (VipTimeRangeList::const_iterator it = lst.begin(); it != lst.end(); ++it) {
		if (it->first != VipInvalidTime && it->second != VipInvalidTime)
			reordered.insert(vipReorder(*it, Vip::Ascending));
	}

	// vipMerge overlapping ranges
	if (merge_ranges) {
		if (!reordered.size())
			return VipTimeRangeList();

		std::set<VipTimeRange>::const_iterator it = reordered.begin();
		res << *(it++); // insert the first range, increment
		for (; it != reordered.end(); ++it) {
			bool ok;
			VipTimeRange merged = vipMerge(*it, res.last(), Vip::Ascending, &ok);
			if (ok)
				res.last() = merged;
			else
				res << *it;
		}
	}
	else {
		res.reserve((int)reordered.size());
		for (std::set<VipTimeRange>::const_iterator it = reordered.begin(); it != reordered.end(); ++it)
			res << *it;
	}

	// vipReorder if necessary
	if (order == Vip::Descending) {
		VipTimeRangeList desc;
		desc.reserve(res.size());
		for (int i = res.size() - 1; i >= 0; --i)
			desc << vipReorder(res[i], Vip::Descending);
		res = desc;
	}

	return res;
}

VipTimeRange vipBounds(const VipTimeRangeList& lst)
{
	VipTimeRange res(VipInvalidTime, VipInvalidTime);
	for (int i = 0; i < lst.size(); ++i) {
		if (res.first == VipInvalidTime)
			res.first = lst[i].first;
		else {
			res.first = qMin(res.first, lst[i].first);
			res.first = qMin(res.first, lst[i].second);
		}

		if (res.second == VipInvalidTime)
			res.second = lst[i].second;
		else {
			res.second = qMax(res.second, lst[i].first);
			res.second = qMax(res.second, lst[i].second);
		}
	}

	return res;
}

VipTimeRangeList vipClamp(const VipTimeRangeList& lst, qint64 first_time, qint64 last_time)
{
	if (last_time < first_time)
		return VipTimeRangeList();

	VipTimeRangeList res;

	for (int i = 0; i < lst.size(); ++i) {
		VipTimeRange r = lst[i];
		if (first_time <= r.first) {
			if (last_time >= r.second)
				res << r;
			else if (last_time >= r.first) {
				res << VipTimeRange(r.first, last_time);
				break;
			}
			else
				break;
		}
		else if (first_time <= r.second) {
			if (last_time >= r.second)
				res << VipTimeRange(first_time, r.second);
			else if (last_time >= r.first) {
				res << VipTimeRange(first_time, last_time);
				break;
			}
			else
				break;
		}
	}

	return res;
}

VipTimeRange vipReplaceMinMaxTime(const VipTimeRange& range, qint64 min_value, qint64 max_value)
{
	VipTimeRange res = range;

	if (res.first == VipMinTime)
		res.first = min_value;
	else if (res.first == VipMaxTime)
		res.first = max_value;

	if (res.second == VipMinTime)
		res.second = min_value;
	else if (res.second == VipMaxTime)
		res.second = max_value;

	return res;
}

VipTimeRangeList vipToTimeRangeList(const QVector<qint64>& timestamps, qint64 sampling)
{
	VipTimeRangeList ranges;
	VipTimeRange current(timestamps.first(), timestamps.first());

	for (int i = 1; i < timestamps.size(); ++i) {
		qint64 gap = timestamps[i] - current.second;
		if (gap > sampling) {
			ranges << current;
			current = VipTimeRange(timestamps[i], timestamps[i]);
		}
		else {
			current.second = timestamps[i];
		}
	}
	ranges << current;
	return ranges;
}

VipTimeRange vipToTimeRange(const QString& str, bool* ok)
{
	bool check = true;
	if (ok)
		*ok = true;

	QString s = str;

	// remove spaces, tabs, end of lines
	s.remove(" ");
	s.remove("\t");
	s.remove("\n");

	QStringList temp = s.split("-");
	qint64 val1 = VipMinTime;
	qint64 val2 = VipMaxTime;

	// special case: '!-' means 'inf -> -inf'
	if (s == "!-") {
		std::swap(val1, val2);
	}
	else if (s == "-") // means '-inf -> inf'
	{
	}
	// only one value with no '-'
	else if (temp.size() == 1) {
		val1 = val2 = temp[0].toLongLong(&check);
		if (!check) {
			if (ok) {
				*ok = false;
			}
		}
	}
	// two values separated by '-'
	else if (temp.size() == 2) {
		if (!temp[0].isEmpty()) {
			val1 = temp[0].toLongLong(&check);
			if (!check) {
				if (ok) {
					*ok = false;
				}
			}
		}
		if (!temp[1].isEmpty()) {
			val2 = temp[1].toLongLong(&check);
			if (!check) {
				if (ok) {
					*ok = false;
				}
			}
		}
	}
	else {
		if (ok) {
			*ok = false;
		}
		return VipTimeRange(VipInvalidTime, VipInvalidTime);
	}

	return VipTimeRange(val1, val2);
}

VipTimeRangeList vipToTimeRangeList(const QString& str, bool* ok)
{
	if (ok)
		*ok = true;
	VipTimeRangeList res;

	// first split according to ',' then to '-'
	QStringList list = str.split(",", VIP_SKIP_BEHAVIOR::SkipEmptyParts);

	for (int i = 0; i < list.size(); ++i) {
		bool success;
		VipTimeRange r = vipToTimeRange(list[i], &success);
		if (!success) {
			if (ok)
				*ok = false;
			break;
		}
		res.append(r);
	}

	return res;
}

QString vipTimeRangeToString(const VipTimeRange& range)
{
	QString res;

	if (range.first == VipInvalidTime || range.second == VipInvalidTime)
		return res;

	if (range.first != VipMinTime)
		res += QString::number(range.first);

	res += "-";

	if (range.second != VipMaxTime)
		res += QString::number(range.second);

	return res;
}

QString vipTimeRangeListToString(const VipTimeRangeList& range)
{
	QStringList lst;
	for (int i = 0; i < range.size(); ++i) {
		QString tmp = vipTimeRangeToString(range[i]);
		if (!tmp.isEmpty())
			lst.append(tmp);
	}

	return lst.join(',');
}

// VipTimeRangeTransforms toTimeRangeTransforms(const QString & from , const QString & to, bool * ok )
// {
// if(*ok) *ok = true;
//
// VipTimeRangeList lfrom = vipToTimeRangeList(from,ok);
// if(ok && !(*ok))
// return VipTimeRangeTransforms();
//
// VipTimeRangeList lto = vipToTimeRangeList(to,ok);
// if(ok && !(*ok))
// return VipTimeRangeTransforms();
//
// if(lfrom.size() != lto.size())
// {
// if(ok) *ok=false;
// return VipTimeRangeTransforms();
// }
//
// VipTimeRangeTransforms res;
// }

void VipTimestampingFilter::setTransforms(const QTransform& tr)
{
	VipTimeRangeTransforms trs;
	for (int i = 0; i < m_inputTimeRange.size(); ++i) {
		VipTimeRange r = m_inputTimeRange[i];
		VipTimeRange out(tr.map(QPointF(r.first, 0)).x(), tr.map(QPointF(r.second, 0)).x());
		trs[r] = out;
	}

	setTransforms(trs);
}

bool VipTimestampingFilter::setTransform(const QTransform& tr, int index)
{
	if (index >= m_transforms.size() || index < 0)
		return false;

	VipTimeRangeTransforms::iterator it = m_transforms.begin();
	std::advance(it , index);

	VipTimeRange r = it.key();
	VipTimeRange out(tr.map(QPointF(r.first, 0)).x(), tr.map(QPointF(r.second, 0)).x());
	*it = (out);
	setTransforms(m_transforms);

	return true;
}

void VipTimestampingFilter::setTransforms(const VipTimeRangeTransforms& trs)
{
	qint64 min_bound = VipMinTime;
	qint64 max_bound = VipMaxTime;

	// compute the upper and lower time values
	if (m_inputTimeRange.size()) {
		min_bound = m_inputTimeRange.first().first;
		max_bound = m_inputTimeRange.last().second;
	}

	m_transforms = trs;
	m_outputTimeRange.clear();
	m_validTransforms.clear();
	m_helper.clear();
	m_invHelper.clear();
	for (VipTimeRangeTransforms::const_iterator it = trs.begin(); it != trs.end(); ++it) {
		VipTimeRange key = it.key();
		VipTimeRange value = it.value();

		// replace the VipMinTime and VipMaxTime by the lower and upper time bounds respectively
		value = vipReplaceMinMaxTime(value, min_bound, max_bound);
		key = vipReplaceMinMaxTime(key, min_bound, max_bound);
		m_outputTimeRange << value;
		m_validTransforms[key] = value;

		// compute the linear transform
		LinearTransform tr;

		if (key.first == key.second) {
			// simple translation
			if (value.first == value.second) {
				tr.second = 1;
				tr.first = value.first - key.first;
			}
			else {
				tr.first = value.first;
				tr.second = 0;
			}
		}
		else {
			tr.second = (value.second - value.first) / double(key.second - key.first);
			tr.first = value.first - key.first * tr.second;
		}
		m_helper[key] = tr;

		// compute the inverse transformation
		if (key.first == key.second) {
			// simple translation
			if (value.first == value.second) {
				tr.second = 1;
				tr.first = key.first - value.first;
			}
			else {
				tr.first = key.first;
				tr.second = 0;
			}
		}
		else {
			tr.second = (key.second - key.first) / double(value.second - value.first);
			tr.first = key.first - value.first * tr.second;
		}
		m_invHelper[value] = tr;
	}

	m_outputTimeRange = vipReorder(m_outputTimeRange, Vip::Ascending, true);
}

const VipTimeRangeTransforms& VipTimestampingFilter::transforms() const
{
	return m_transforms;
}

const VipTimeRangeTransforms& VipTimestampingFilter::validTransforms() const
{
	return m_validTransforms;
}

void VipTimestampingFilter::setInputTimeRangeList(const VipTimeRangeList& lst)
{
	m_inputTimeRange = lst;

	// reset the transform...
	if (m_transforms.size())
		setTransforms(m_transforms);
	// else
	//  {
	//  //...or create new one
	//  for(int i=0; i < lst.size(); ++i)
	//  {
	//  m_transforms[lst[i]] = lst[i];
	//  m_outputTimeRange.append(lst[i]);
	//  }
	//  }
}

const VipTimeRangeList& VipTimestampingFilter::inputTimeRangeList() const
{
	return m_inputTimeRange;
}

const VipTimeRangeList& VipTimestampingFilter::outputTimeRangeList() const
{
	return m_outputTimeRange;
}

qint64 VipTimestampingFilter::transform(qint64 time, bool* inside) const
{
	if (inside)
		*inside = true;
	if (m_helper.isEmpty())
		return time;

	LinearTransform closest_tr = m_helper.begin().value();
	qint64 closest_time = VipMaxTime;
	qint64 dist_to_closest = VipMaxTime;

	for (LinearTransformHelper::const_iterator it = m_helper.begin(); it != m_helper.end(); ++it) {
		const VipTimeRange& key = it.key();

		// time inside interval, just apply the transform
		if (vipIsInside(key, time)) {
			const LinearTransform& tr = it.value();
			return qRound64(tr.first + time * tr.second);
		}
		// otherwise, check if this interval is the closest to time
		else {
			qint64 close;
			qint64 dist = vipDistance(key, time, &close);
			if (dist < dist_to_closest) {
				closest_tr = it.value();
				dist_to_closest = dist;
				closest_time = close;
			}
		}
	}

	if (inside)
		*inside = false;
	return qRound64(closest_tr.first + closest_time * closest_tr.second);
}

qint64 VipTimestampingFilter::invTransform(qint64 time, bool* inside) const
{
	if (inside)
		*inside = true;
	if (m_invHelper.isEmpty())
		return time;

	LinearTransform closest_tr = m_invHelper.begin().value();
	qint64 closest_time = VipMaxTime;
	qint64 dist_to_closest = VipMaxTime;

	for (LinearTransformHelper::const_iterator it = m_invHelper.begin(); it != m_invHelper.end(); ++it) {
		const VipTimeRange& key = it.key();

		// time inside interval, just apply the transform
		if (vipIsInside(key, time)) {
			const LinearTransform& tr = it.value();
			return qRound64(tr.first + time * tr.second);
		}
		// otherwise, check if this interval is the closest to time
		else {
			qint64 close;
			qint64 dist = vipDistance(key, time, &close);
			if (dist < dist_to_closest) {
				closest_tr = it.value();
				dist_to_closest = dist;
				closest_time = close;
			}
		}
	}

	if (inside)
		*inside = false;
	return qRound64(closest_tr.first + closest_time * closest_tr.second);
}

bool VipTimestampingFilter::isEmpty() const
{
	return m_transforms.isEmpty();
}

void VipTimestampingFilter::reset()
{
	m_inputTimeRange.clear();
	m_outputTimeRange.clear();
	m_validTransforms.clear();
	m_transforms.clear();
	m_helper.clear();
	m_invHelper.clear();
}

#include <QDataStream>

QDataStream& operator<<(QDataStream& stream, const VipTimestampingFilter& filter)
{
	return stream << filter.transforms();
}

QDataStream& operator>>(QDataStream& stream, VipTimestampingFilter& filter)
{
	VipTimeRangeTransforms trs;
	stream >> trs;
	filter.setTransforms(trs);
	return stream;
}

static VipTimeRange toTimeRange(const QString& str)
{
	return vipToTimeRange(str);
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipTimestampingFilter>();
	qRegisterMetaTypeStreamOperators<VipTimeRange>("VipTimeRange");
	qRegisterMetaTypeStreamOperators<VipTimeRangeList>("VipTimeRangeList");
	qRegisterMetaTypeStreamOperators<VipTimeRangeVector>("VipTimeRangeVector");
	qRegisterMetaTypeStreamOperators<VipTimestamps>("VipTimestamps");
	qRegisterMetaTypeStreamOperators<VipTimeRangeTransforms>("VipTimeRangeTransforms");
	qRegisterMetaTypeStreamOperators<VipTimestampingFilter>("VipTimestampingFilter");

	QMetaType::registerConverter<VipTimeRange, QString>(vipTimeRangeToString);
	QMetaType::registerConverter<QString, VipTimeRange>(toTimeRange);

	return 0;
}
static int _registerStreamOperators = registerStreamOperators();
