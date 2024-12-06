/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, L�o Dubus, Erwan Grelier
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

#ifndef VIP_TRANSFORM_ARRAY_H
#define VIP_TRANSFORM_ARRAY_H

#include "VipNDArrayOperations.h"
#include "VipMath.h"
#include <qtransform.h>

/// \addtogroup DataType
/// @{

namespace Vip
{
	enum TransformSize
	{
		SrcSize,
		TransformBoundingRect
	};
}

/// @}
// end DataType

namespace detail
{
	template<class Coord, qsizetype Pos, bool IsValid = (Coord::static_size == -1 || Coord::static_size > Pos)>
	struct GetCoord
	{
		static qsizetype get(const Coord& c) { return c[Pos]; }
	};
	template<class Coord, qsizetype Pos>
	struct GetCoord<Coord,Pos,false>
	{
		static qsizetype get(const Coord& ) { return 0; }
	};


	template<class Array>
	typename Array::value_type getVal(const Array& ar, qsizetype y, qsizetype x)
	{
		VipCoordinate<2> p; // = { {y,x} };
		p[0] = y;
		p[1] = x;
		return ar(p);
	}
	template<class T>
	T _abs(const T& v)
	{
		return v > 0 ? v : -v;
	}

	template<bool Interp>
	struct InterpVal
	{
		template<class Array, class T>
		static VIP_ALWAYS_INLINE typename Array::value_type apply(const Array& ar, double x, double y, qsizetype w, qsizetype h, const T& background)
		{
			qsizetype _x = (qsizetype)(x + 0.5);
			qsizetype _y = (qsizetype)(y + 0.5);
			if (_x < 0 || _x >= w || _y < 0 || _y >= h)
				return background;

			VipCoordinate<2> p;
			(p[0]) = _y;
			(p[1]) = _x;
			return ar(p);
		}
	};
	template<>
	struct InterpVal<true>
	{
		template<class Array, class T>
		static VIP_ALWAYS_INLINE typename Array::value_type apply(const Array& ar, double x, double y, qsizetype w, qsizetype h, const T& background)
		{
			if (x < -1 || x > w || y < -1 || y > h)
				return background;

			const qsizetype leftCellEdge = x < 0 ? -1 : x;
			const qsizetype topCellEdge = y < 0 ? -1 : y;
			const qsizetype rightCellEdge = leftCellEdge + 1;
			const qsizetype bottomCellEdge = topCellEdge + 1;
			const bool inBottom = bottomCellEdge < h;
			const bool inTop = topCellEdge >= 0;
			const bool inLeft = leftCellEdge >= 0;
			const bool inRight = rightCellEdge < w;

			const typename Array::value_type p1 = inBottom && inLeft ? getVal(ar, bottomCellEdge, leftCellEdge) : background;
			const typename Array::value_type p2 = inTop && inLeft ? getVal(ar, topCellEdge, leftCellEdge) : background;
			const typename Array::value_type p3 = inBottom && inRight ? getVal(ar, bottomCellEdge, rightCellEdge) : background;
			const typename Array::value_type p4 = inTop && inRight ? getVal(ar, topCellEdge, rightCellEdge) : background;

			const double u = _abs(x - leftCellEdge);
			const double v = _abs(bottomCellEdge - y);
			const double v_1 = 1. - v;
			const double u_1 = 1. - u;
			return p1 * (v_1 * u_1) + p2 * (v * u_1) + p3 * (v_1 * u) + p4 * (v * u);
		}
	};

	template<Vip::TransformSize Size, Vip::InterpolationType Inter, class Array, bool hasNullType = HasNullType<Array>::value>
	struct Transform : BaseOperator1<typename DeduceArrayType<Array>::value_type, Array>
	{
		_ENSURE_OPERATOR1_DEF(BaseOperator1<typename DeduceArrayType<Array>::value_type, Array>)

		static const qsizetype access_type = Vip::Position;

		QTransform tr;
		QPoint origin;
		QPointF translate;
		VipNDArrayShape sh;
		const QTransform::TransformationType type;
		value_type background;
		const qsizetype w, h;
		QRect rect;

		Transform()
		  : type(QTransform::TxNone)
		  , w(0)
		  , h(0)
		{
		}
		Transform(const Array& op1, const QTransform& tr, const QRect& rect, const value_type& back, const QPointF additional_translate = QPointF(0, 0))
		  : base(op1)
		  , tr(tr)
		  , origin(0, 0)
		  , translate(additional_translate)
		  , type(tr.type())
		  , background(back)
		  , w(this->array1.shape()[1])
		  , h(this->array1.shape()[0])
		  , rect(rect)
		{
			if (Size == Vip::SrcSize)
				sh = op1.shape();
			else {
				origin = rect.topLeft();
				sh = vipVector(rect.height(), rect.width());
			}
		}

		const VipNDArrayShape& shape() const { return sh; }

		template<class Coord>
		value_type operator()(const Coord& pos) const
		{
			if (type == QTransform::TxNone)
				return this->array1(pos);

			const double fx = GetCoord<Coord,1>::get( pos) + origin.x(); // (Size == Vip::TransformBoundingRect ? origin.x() : 0);
			const double fy = GetCoord<Coord,0>::get(pos) + origin.y(); // (Size == Vip::TransformBoundingRect ? origin.y() : 0);
			double x = fx, y = fy;

			if (type == QTransform::TxTranslate) {
				x += tr.dx();
				y += tr.dy();
			}
			else if (type == QTransform::TxScale) {
				x = tr.m11() * fx + tr.dx();
				y = tr.m22() * fy + tr.dy();
			}
			else {
				x = tr.m11() * fx + tr.m21() * fy + tr.dx();
				y = tr.m12() * fx + tr.m22() * fy + tr.dy();
				if (type == QTransform::TxProject) {
					qreal _w = 1. / (tr.m13() * fx + tr.m23() * fy + tr.m33());
					x *= _w;
					y *= _w;
				}
			}
			x += translate.x();
			y += translate.y();
			return InterpVal<Inter != Vip::NoInterpolation>::apply(this->array1, x, y, w, h, background);
		}
	};

	template<Vip::TransformSize Size, Vip::InterpolationType Inter, class Array>
	struct Transform<Size, Inter, Array, true> : BaseOperator1<NullType, Array>
	{
		_ENSURE_OPERATOR1_DEF(BaseOperator1<NullType, Array>)

		QTransform tr;
		VipNDArrayShape sh;
		QVariant background;
		QRect rect;
		QPointF translate;
		Transform() {}
		template<class T>
		Transform(const Array& op1, const QTransform& tr, const QRect& rect, const T& back, const QPointF& additional_translate = QPointF())
		  : BaseOperator1<NullType, Array>(op1)
		  , tr(tr)
		  , background(QVariant::fromValue(back))
		  , rect(rect)
		  , translate(additional_translate)
		{
			if (Size == Vip::SrcSize)
				sh = op1.shape();
			else
				sh = vipVector(rect.height(), rect.width());
		}
		const VipNDArrayShape& shape() const { return sh; }
	};

	template<class T, Vip::TransformSize Size, Vip::InterpolationType Inter, class Array, bool hasNullType>
	struct rebind<T, Transform<Size, Inter, Array, hasNullType>, false>
	{

		typedef Transform<Size, Inter, Array, hasNullType> op;
		typedef rebind<T, typename op::array_type1> rebind_type;
		typedef Transform<Size, Inter, typename rebind_type::type, false> transform;
		typedef transform type;

		static transform cast(const op& a) { return transform(rebind_type::cast(a.array1), a.tr, a.rect, internal_cast<T>(a.background), a.translate); }
	};

	template<Vip::TransformSize Size, Vip::InterpolationType Inter, class Array>
	struct is_valid_functor<Transform<Size, Inter, Array, false>> : std::true_type
	{
	};

}

/// \addtogroup DataType
/// @{

/// Returns a functor expression to transform input \a array based on given QTransform and background value.
///
/// The output shape depends on \a Size template parameter:
/// - #Vip::SrcSize: same shape as the input array
/// - #Vip::TransformBoundingRect: bounding rect of the transformation.
///
/// Output array is interpolated using the \a Inter template parameter. Only Vip::NoInterpolation
/// and Vip::LinearInterpolation are valid values.
template<Vip::TransformSize Size, Vip::InterpolationType Inter = Vip::NoInterpolation, class Array, class T>
detail::Transform<Size, Inter, Array> vipTransform(const Array& array, const QTransform& tr, const T& background, const QPointF& additional_translate = QPointF())
{
	QRect r;
	if (Size == Vip::TransformBoundingRect)
		r = tr.mapRect(QRect(0, 0, array.shape()[1], array.shape()[0]));
	return detail::Transform<Size, Inter, Array>(array, tr.inverted(), r, background, additional_translate);
}

/// @}
// end DataType

#endif