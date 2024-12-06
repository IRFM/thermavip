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

#ifndef VIP_VECTORS_H
#define VIP_VECTORS_H

#include <qpoint.h>
#include <qvector.h>

#include "VipComplex.h"
#include "VipInterval.h"

typedef QVector<VipIntervalSample> VipIntervalSampleVector;
Q_DECLARE_METATYPE(VipIntervalSampleVector)

/// @brief 2D serie of points
class VIP_DATA_TYPE_EXPORT VipPointVector : public QVector<VipPoint>
{
public:
	VipPointVector()
	  : QVector<VipPoint>()
	{
	}
	VipPointVector(qsizetype size)
	  : QVector<VipPoint>(size)
	{
	}
	VipPointVector(const QVector<VipPoint>& other)
	  : QVector<VipPoint>(other)
	{
	}
	VipPointVector(const QVector<QPointF>& other)
	  : QVector<VipPoint>(other.size())
	{
		std::copy(other.begin(), other.end(), begin());
	}
	VipPointVector(const VipPointVector& other)
	  : QVector<VipPoint>(other)
	{
	}
	VipPointVector(VipPointVector&& other) noexcept
	  : QVector<VipPoint>(std::move(other))
	{
	}

	VipPointVector& operator=(const VipPointVector& other)
	{
		static_cast<QVector<VipPoint>&>(*this) = static_cast<const QVector<VipPoint>&>(other);
		return *this;
	}
	VipPointVector& operator=(VipPointVector&& other) noexcept
	{
		static_cast<QVector<VipPoint>&>(*this) = std::move(static_cast<const QVector<VipPoint>&>(other));
		return *this;
	}
	VipPointVector& operator=(const QVector<VipPoint>& other)
	{
		static_cast<QVector<VipPoint>&>(*this) = other;
		return *this;
	}

	QRectF boundingRect() const
	{
		if (size() == 0)
			return QRectF();
		else if (size() == 1)
			return QRectF(first(), first());
		else {
			QRectF r = QRectF(at(0), at(1)).normalized();
			for (qsizetype i = 2; i < size(); ++i) {
				const VipPoint p = (*this)[i];
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

	QVector<QPointF> toPointF() const
	{
		QVector<QPointF> res(size());
		for (qsizetype i = 0; i < size(); ++i)
			res[i] = (*this)[i].toPointF();
		return res;
	}
};
Q_DECLARE_METATYPE(VipPointVector)

/// @brief Combination of floating point x value and complex y value
class VipComplexPoint
{
	vip_double xp;
	complex_d yp;

public:
	VipComplexPoint()
	  : xp(0)
	  , yp(0)
	{
	}
	VipComplexPoint(vip_double x, const complex_d& y)
	  : xp(x)
	  , yp(y)
	{
	}

	vip_double x() const { return xp; }
	complex_d y() const { return yp; }

	vip_double& rx() { return xp; }
	complex_d& ry() { return yp; }

	void setX(vip_double x) { xp = x; }
	void setY(const complex_d& y) { yp = y; }
};

typedef QVector<VipComplexPoint> VipComplexPointVector;
Q_DECLARE_METATYPE(VipComplexPoint)
Q_DECLARE_METATYPE(VipComplexPointVector)

#endif