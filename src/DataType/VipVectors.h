#pragma once

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
		:QVector<VipPoint>() 
	{}
	VipPointVector(int size)
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
	VipPointVector( VipPointVector&& other) noexcept
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
			for (int i = 2; i < size(); ++i) {
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
		for (int i = 0; i < size(); ++i)
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
