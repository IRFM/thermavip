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

#ifndef VIP_GLOBALS_H
#define VIP_GLOBALS_H

#include <QLineF>
#include <QMetaType>
#include <QPair>
#include <QPolygonF>
#include <QTransform>

#include <cmath>
#include <limits>

#include "VipConfig.h"
#include "VipInterval.h"
#include "VipLongDouble.h"
#include "VipMath.h"

/// \addtogroup Plotting
/// @{

#ifndef QT_STATIC_CONST
#define QT_STATIC_CONST static const
#endif

#ifndef QT_STATIC_CONST
#define QT_STATIC_CONST static const
#endif

#ifndef QT_STATIC_CONST_IMPL
#define QT_STATIC_CONST_IMPL const
#endif

#ifndef VIP_PLOTTING_STICK_DISTANCE
#define VIP_PLOTTING_STICK_DISTANCE 10
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#define QLINE_INTERSECTS intersects
#else
#define QLINE_INTERSECTS intersect
#endif

namespace Vip
{
	/// @brief Constant representing an invalid value
	const double InvalidValue = std::numeric_limits<double>::quiet_NaN();
	/// @brief Constant representing an invalid position
	const QPointF InvalidPoint = QPointF(InvalidValue, InvalidValue);
	/// @brief Constant representing an infinit interval
	const VipInterval InfinitInterval = VipInterval(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

	/// @brief Absolute or relative value
	enum ValueType
	{
		Absolute,
		Relative
	};

	/// @brief Region position, mainly used to find the position of text around a shape
	enum RegionPosition
	{
		Outside = 0,
		XInside = 0x01,
		YInside = 0x02,
		Inside = XInside | YInside,
		XAutomatic = 0x04,
		YAutomatic = 0x08,
		Automatic = XAutomatic | YAutomatic
	};
	typedef QFlags<RegionPosition> RegionPositions;

	enum Side
	{
		NoSide = 0,
		Top = 0x01,
		Right = 0x02,
		Bottom = 0x04,
		Left = 0x08,
		AllSides = (Top | Right | Bottom | Left)
	};
	typedef QFlags<Side> Sides;

	enum Corner
	{
		NoCorner = 0,
		TopLeft = 0x01,
		TopRight = 0x02,
		BottomRight = 0x04,
		BottomLeft = 0x08,
		AllCorners = (TopLeft | TopRight | BottomRight | BottomLeft)
	};
	typedef QFlags<Corner> Corners;
}

/// @brief Returns true if given value is valid, i.e. not NaN
Q_DECL_CONSTEXPR static inline bool vipIsValid(float value) noexcept
{
	return !vipIsNan(value);
}
Q_DECL_CONSTEXPR static inline bool vipIsValid(double value) noexcept
{
	return !vipIsNan(value);
}
Q_DECL_CONSTEXPR static inline bool vipIsValid(long double value) noexcept
{
	return !vipIsNan(value);
}

Q_DECL_CONSTEXPR static inline bool vipIsValid(const QPointF& pt) noexcept
{
	return !(vipIsNan(pt.x()) || vipIsNan(pt.y()));
}
Q_DECL_CONSTEXPR static inline bool vipIsValid(const VipLongPoint& pt) noexcept
{
	return !(vipIsNan(pt.x()) || vipIsNan(pt.y()));
}

/// @brief Round point after transformation
inline QPointF vipRound(const QPointF& pt, const QTransform& tr = QTransform()) noexcept
{
	const QPointF p = tr.map(pt);
	return QPointF(qRound(p.x()), qRound(p.y()));
}
/// @brief Round line after transformation
inline QLineF vipRound(const QLineF& line, const QTransform& tr = QTransform()) noexcept
{
	return QLineF(vipRound(line.p1(), tr), vipRound(line.p2(), tr));
}
/// @brief Round rect after transforming top left and bottom right points
inline QRectF vipRound(const QRectF& rect, const QTransform& tr = QTransform()) noexcept
{
	return QRectF(vipRound(rect.topLeft(), tr), vipRound(rect.bottomRight(), tr));
}
/// @brief Round polygon after transformation
inline QPolygonF vipRound(const QPolygonF& poly, const QTransform& tr = QTransform()) 
{
	const int size = poly.size();
	QPolygonF polygon(size);
	for (int i = 0; i < size; ++i)
		polygon[i] = vipRound(poly[i], tr);
	return polygon;
}
/// @brief Round polyline after transformation
inline QPolygonF vipRound(const QPointF* points, int pointCount, const QTransform& tr = QTransform())
{
	QPolygonF polygon(pointCount);
	for (int i = 0; i < pointCount; ++i)
		polygon[i] = vipRound(points[i], tr);
	return polygon;
}
/// @brief Extract centered inner square of a rectangle
inline QRectF vipInnerSquare(const QRectF& r) noexcept
{
	QRectF square = r;
	if (square.width() > square.height()) {
		square.setLeft(r.left() + (r.width() - r.height()) / 2.);
		square.setWidth(r.height());
	}
	else {
		square.setTop(r.top() + (square.height() - square.width()) / 2.);
		square.setHeight(square.width());
	}

	return square;
}

// Fuzzy comparisons

inline bool vipFuzzyCompare(double d1, double d2) noexcept
{
	if (d1 == 0 || d2 == 0)
		return qFuzzyCompare(1.0 + d1, 1.0 + d2);
	else
		return qFuzzyCompare(d1, d2);
}
inline bool vipFuzzyCompare(const QPointF& p1, const QPointF& p2) noexcept
{
	return vipFuzzyCompare(p1.x(), p2.x()) && vipFuzzyCompare(p1.y(), p2.y());
}
inline bool vipFuzzyCompare(const QSizeF& s1, const QSizeF& s2) noexcept
{
	return vipFuzzyCompare(s1.width(), s2.width()) && vipFuzzyCompare(s1.height(), s2.height());
}
inline bool vipFuzzyCompare(const QRectF& r1, const QRectF& r2) noexcept
{
	return vipFuzzyCompare(r1.topLeft(), r2.topLeft()) && vipFuzzyCompare(r1.size(), r2.size());
}

/// @brief Compute intersection between a line and a rectangle.
/// Returns at most 2 valid points.
inline QPair<QPointF, QPointF> vipIntersect(const QLineF& line, const QRectF& rect) noexcept
{
	QPair<QPointF, QPointF> res(Vip::InvalidPoint, Vip::InvalidPoint);

	QPointF inter;
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	if (line.intersect(QLineF(rect.topLeft(), rect.topRight()), &inter) == QLineF::BoundedIntersection)
#else
	if (line.QLINE_INTERSECTS(QLineF(rect.topLeft(), rect.topRight()), &inter) == QLineF::BoundedIntersection)
#endif
	{
		if (!vipIsValid(res.first))
			res.first = inter;
		else if (!vipIsValid(res.second)) {
			res.second = inter;
			return res;
		}
	}
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	if (line.intersect(QLineF(rect.topRight(), rect.bottomRight()), &inter) == QLineF::BoundedIntersection)
#else
	if (line.QLINE_INTERSECTS(QLineF(rect.topRight(), rect.bottomRight()), &inter) == QLineF::BoundedIntersection)
#endif
	{
		if (!vipIsValid(res.first))
			res.first = inter;
		else if (!vipIsValid(res.second)) {
			res.second = inter;
			return res;
		}
	}
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	if (line.intersect(QLineF(rect.bottomRight(), rect.bottomLeft()), &inter) == QLineF::BoundedIntersection)
#else
	if (line.QLINE_INTERSECTS(QLineF(rect.bottomRight(), rect.bottomLeft()), &inter) == QLineF::BoundedIntersection)
#endif
	{
		if (!vipIsValid(res.first))
			res.first = inter;
		else if (!vipIsValid(res.second)) {
			res.second = inter;
			return res;
		}
	}
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	if (line.intersect(QLineF(rect.bottomLeft(), rect.topLeft()), &inter) == QLineF::BoundedIntersection)
#else
	if (line.QLINE_INTERSECTS(QLineF(rect.bottomLeft(), rect.topLeft()), &inter) == QLineF::BoundedIntersection)
#endif
	{
		if (!vipIsValid(res.first))
			res.first = inter;
		else if (!vipIsValid(res.second)) {
			res.second = inter;
			return res;
		}
	}
	return res;
}

/// Compare and angle to a range of angles.
/// \param start Start angle ([-360,360]).
/// \param end End angle ([-360,360]). must be > start.
/// \param angle Angle to compare to given range ([-360,360]).
/// \return 0 if \a angle is inside the range [start,end], 1 if angle > end, -1 if angle < start.
inline int vipCompareAngle(double start, double end, double angle) noexcept
{
	// full circle: always inside
	if (end - start == 360)
		return 0;

	end -= start;
	angle -= start;
	start = 0;
	double mid = (end - start) / 2.0 + 180.0;

	if (end < 0)
		end += 360;
	if (angle < 0)
		end += 360;

	if (angle <= end && angle >= start)
		return 0;
	else if (angle < mid)
		return 1;
	else
		return -1;
}

/// @brief Margin class used by VipAbstractPlotArea
struct VipMargins
{
	double left;
	double top;
	double right;
	double bottom;

	VipMargins() noexcept
	  : left(0)
	  , top(0)
	  , right(0)
	  , bottom(0)
	{
	}

	VipMargins(double l, double t, double r, double b) noexcept
	  : left(l)
	  , top(t)
	  , right(r)
	  , bottom(b)
	{
	}

	double totalWidth() const noexcept { return left + right; }
	double totalHeight() const noexcept { return top + bottom; }

	bool operator!=(const VipMargins& other) const noexcept
	{
		return !vipFuzzyCompare(left, other.left) || !vipFuzzyCompare(top, other.top) || !vipFuzzyCompare(right, other.right) || !vipFuzzyCompare(bottom, other.bottom);
	}

	bool operator==(const VipMargins& other) const noexcept { return !((*this) != other); }

	VipMargins& operator+=(const VipMargins& other) noexcept
	{
		left += other.left;
		right += other.right;
		top += other.top;
		bottom += other.bottom;
		return *this;
	}
	VipMargins& operator+=(double val) noexcept
	{
		left += val;
		right += val;
		top += val;
		bottom += val;
		return *this;
	}
	VipMargins& operator-=(const VipMargins& other) noexcept
	{
		left -= other.left;
		right -= other.right;
		top -= other.top;
		bottom -= other.bottom;
		return *this;
	}
	VipMargins& operator-=(double val) noexcept
	{
		left -= val;
		right -= val;
		top -= val;
		bottom -= val;
		return *this;
	}
};

inline VipMargins operator+(const VipMargins& left, const VipMargins& right) noexcept
{
	VipMargins res = left;
	return res += right;
}
inline VipMargins operator+(const VipMargins& left, double val) noexcept
{
	VipMargins res = left;
	return res += val;
}
inline VipMargins operator-(const VipMargins& left, const VipMargins& right) noexcept
{
	VipMargins res = left;
	return res -= right;
}
inline VipMargins operator-(const VipMargins& left, double val) noexcept
{
	VipMargins res = left;
	return res -= val;
}

Q_DECLARE_METATYPE(VipMargins);

Q_DECLARE_OPERATORS_FOR_FLAGS(Vip::RegionPositions)
Q_DECLARE_OPERATORS_FOR_FLAGS(Vip::Sides)
Q_DECLARE_OPERATORS_FOR_FLAGS(Vip::Corners)

/// @}
// end Plotting

#endif
