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

#ifndef VIP_VECTORS_H
#define VIP_VECTORS_H

#include <qpoint.h>
#include <qvector.h>

#include "VipComplex.h"
#include "VipInterval.h"
#include "VipCircularVector.h"

template<class T>
using VipSampleVector = VipCircularVector<T>;

using VipIntervalSampleVector = VipSampleVector<VipIntervalSample>;
Q_DECLARE_METATYPE(VipIntervalSampleVector)

using VipPointVector = VipSampleVector<VipPoint>;
Q_DECLARE_METATYPE(VipPointVector)

/// @brief Convert a VipPointVector to QVector<QPointF>
inline QVector<QPointF> vipToPointF(const VipPointVector& v)
{
	QVector<QPointF> res(v.size());
	std::copy(v.begin(), v.end(), res.begin());
	return res;
}

/// @brief Convert a QVector<QPointF> to VipPointVector
inline VipPointVector vipToPointVector(const QVector<QPointF>& v)
{
	VipPointVector res(v.size());
	auto it = v.begin();
	res.for_each(0, v.size(), [&it](VipPoint& p) { p = *it++; });
	return res;
}

/// @brief Extract VipPointVector bounding rectangle
inline QRectF vipBoundingRect(const VipPointVector& v) 
{
	if (v.size() == 0)
		return QRectF();
	else if (v.size() == 1)
		return QRectF(v.first(), v.first());
	else {
		QRectF r = QRectF(v[0], v[1]).normalized();
		for (qsizetype i = 2; i < v.size(); ++i) {
			const VipPoint p =v[i];
			if (p.x() > r.right())
				r.setRight(p.x());
			else if (p.x() < r.left())
				r.setLeft(p.x());
			if (p.y() > r.bottom())
				r.setBottom(p.y());
			else if (p.y() < r.top())
				r.setTop(p.y());
		}
		return r;
	}

}


/// @brief Combination of floating point x value and complex y value
class VipComplexPoint
{
	vip_double xp;
	complex_d yp;

public:
	VipComplexPoint() noexcept
	  : xp(0)
	  , yp(0)
	{
	}
	VipComplexPoint(vip_double x, const complex_d& y) noexcept
	  : xp(x)
	  , yp(y)
	{
	}

	vip_double x() const noexcept { return xp; }
	complex_d y() const noexcept { return yp; }

	vip_double& rx() noexcept { return xp; }
	complex_d& ry() noexcept { return yp; }

	void setX(vip_double x) noexcept { xp = x; }
	void setY(const complex_d& y) noexcept { yp = y; }
};

using VipComplexPointVector = VipSampleVector<VipComplexPoint>;
Q_DECLARE_METATYPE(VipComplexPoint)
Q_DECLARE_METATYPE(VipComplexPointVector)

#endif