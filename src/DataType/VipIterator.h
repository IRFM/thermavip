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

#ifndef VIP_ND_SUBARRAY_ITERATOR_H
#define VIP_ND_SUBARRAY_ITERATOR_H

#include "VipHybridVector.h"
#include <QPair>

/// \addtogroup DataType
/// @{

/// Null transform returning its input
struct VipNullTransform
{
	template<class T>
	T operator()(T v) const
	{
		return v;
	}
};

/// Transform filling with a constant value
template<class T>
struct VipFillTransform
{
	const T value;
	VipFillTransform(const T& val = T())
	  : value(val)
	{
	}
	T operator()(T) const { return value; }
};

namespace Vip
{
	/// \brief Storage orders.
	///
	/// There are two different storage orders for n-dimensional arrays: first-major (corresponding to column-major for matrices)
	/// and last-major (corresponding to row-major).
	///
	/// An array is stored in first major order if the first dimension has the greatest stride value. For a matrix, it means that values are stored row by row.
	///
	/// Consider the following 2*3 matrix:
	///
	/// \f[
	///	A = \begin{bmatrix}
	///	1 & 2
	///	3 & 4
	///	5 & 6
	///	\end{bmatrix}.
	///	\f]
	///
	/// In the case of first-major storage order, the values are laid out in memory like this:
	///
	/// \code 1 2 3 4 5 6 \endcode
	///
	/// On the other hand, last-major order means that the first dimension has the lowest stride value (values stored column by column for a matrix).
	/// For the above matrix, values are laid out like this:
	///
	/// \code 1 4 7 2 5 8 3 6 9 \endcode
	///
	/// The default order in the DataType library is first-major.
	enum Ordering
	{
		FirstMajor = 1,
		LastMajor = 2
	};
}

namespace iter_detail
{

	template<class Shape>
	struct StaticSize
	{
		static const qsizetype value = Shape::static_size;
	};
	template<class Shape>
	struct StaticSize<Shape&>
	{
		static const qsizetype value = Shape::static_size;
	};
	template<class Shape>
	struct StaticSize<const Shape&>
	{
		static const qsizetype value = Shape::static_size;
	};
	template<class Shape>
	struct StaticSize<const Shape>
	{
		static const qsizetype value = Shape::static_size;
	};

}

#define __NDIM(expr) iter_detail::StaticSize<decltype(expr)>::value
#define __NDIM2(expr1, expr2)                                                                                                                                                                          \
	iter_detail::StaticSize<decltype(expr1)>::value > iter_detail::StaticSize<decltype(expr2)>::value ? iter_detail::StaticSize<decltype(expr1)>::value                                            \
													  : iter_detail::StaticSize<decltype(expr2)>::value

namespace detail
{
	template<class StridesType, class PosType, qsizetype Dim>
	struct ComputeOffset
	{
		static qsizetype offset(const StridesType& strides, const PosType& pos, qsizetype previous_offset) noexcept
		{
			return previous_offset + ComputeOffset<StridesType, PosType, Dim - 1>::offset(strides, pos, strides[PosType::static_size - Dim] * pos[PosType::static_size - Dim]);
		}
	};

	template<class StridesType, class PosType>
	struct ComputeOffset<StridesType, PosType, 1>
	{
		static qsizetype offset(const StridesType& strides, const PosType& pos, qsizetype previous_offset) noexcept
		{
			return previous_offset + pos[PosType::static_size - 1] * strides[PosType::static_size - 1];
		}
	};

	template<class StridesType, class PosType>
	struct ComputeOffset<StridesType, PosType, Vip::None>
	{
		static VIP_ALWAYS_INLINE qsizetype offset(const StridesType& strides, const PosType& pos, qsizetype) noexcept
		{
			switch (pos.size()) {
				case 1:
					return strides[0] * pos[0];
				case 2:
					return strides[0] * pos[0] + strides[1] * pos[1];
				case 3:
					return strides[0] * pos[0] + strides[1] * pos[1] + strides[2] * pos[2];
				default: {
					qsizetype res = strides[0] * pos[0];
					for (qsizetype i = 1; i < pos.size(); ++i)
						res += strides[i] * pos[i];
					return res;
				}
			}
		}
	};

	template<bool Unstrided, class StridesType, class PosType, qsizetype Dim>
	struct ComputeFinalOffset
	{
		static VIP_ALWAYS_INLINE qsizetype offset(const StridesType& strides, const PosType& pos) noexcept
		{
			return ComputeOffset<StridesType, VipCoordinate< Dim>, Dim>::offset(strides, pos, 0);
		}
	};

	// small optimization for dimension 1,2 and 3
	template<bool Unstrided, class StridesType, class PosType>
	struct ComputeFinalOffset<Unstrided, StridesType, PosType, 1>
	{
		static VIP_ALWAYS_INLINE qsizetype offset(const StridesType& strides, const PosType& pos) noexcept
		{
			if (StridesType::static_size == 1 && Unstrided)
				return pos[0];
			else
				return pos[0] * strides[0];
		}
	};

	template<bool Unstrided, class StridesType, class PosType>
	struct ComputeFinalOffset<Unstrided, StridesType, PosType, 2>
	{
		static VIP_ALWAYS_INLINE qsizetype offset(const StridesType& strides, const PosType& pos) noexcept
		{
			if (StridesType::static_size == 2 && Unstrided)
				return pos[0] * strides[0] + pos[1];
			else
				return pos[0] * strides[0] + pos[1] * strides[1];
		}
	};

	template<bool Unstrided, class StridesType, class PosType>
	struct ComputeFinalOffset<Unstrided, StridesType, PosType, 3>
	{
		static VIP_ALWAYS_INLINE qsizetype offset(const StridesType& strides, const PosType& pos) noexcept
		{
			if (StridesType::static_size == 3 && Unstrided)
				return pos[0] * strides[0] + pos[1] * strides[1] + pos[2];
			else
				return pos[0] * strides[0] + pos[1] * strides[1] + pos[2] * strides[2];
		}
	};
}

/// Compute the size of shape (#VipHybridVector object) by multiplying all its components
template<class ShapeType>
qsizetype vipShapeToSize(const ShapeType& shape)
{
	if (!shape.size())
		return 0;
	qsizetype res = shape[0];
	for (qsizetype i = 1; i < shape.size(); ++i)
		res *= shape[i];
	return res;
}

/// Compute the size of shape (#VipHybridVector object) by multiplying all its components.
/// In addition, tells if given strides are considered as unstrided for this shape.
template<class ShapeType, class StrideType>
qsizetype vipShapeToSize(const ShapeType& shape, const StrideType& strides, bool* is_unstrided)
{
	if (!shape.size()) {
		*is_unstrided = true;
		return 0;
	}
	*is_unstrided = strides.back() == 1;
	qsizetype size_in = shape.back();
	for (qsizetype i = strides.size() - 2; i >= 0; --i) {
		size_in *= shape[i];
		if (strides[i] != strides[i + 1] * shape[i + 1])
			*is_unstrided = false;
	}
	return size_in;
}

/// For given #Ordering and shape, compute the corresponding default strides.
/// Returns the array size.
template<Vip::Ordering order, class Shape, class Strides>
inline qsizetype vipComputeDefaultStrides(const Shape& shape, Strides& strides)
{
	if (!shape.size())
		return 0;
	strides.resize(shape.size());
	if (order == Vip::FirstMajor) {
		qsizetype size = shape.back();
		strides.back() = 1;
		for (qsizetype i = strides.size() - 2; i >= 0; --i) {
			size *= shape[i];
			strides[i] = strides[i + 1] * shape[i + 1];
		}
		return size;
	}
	else {
		qsizetype size = shape.front();
		strides.front() = 1;
		for (qsizetype i = 1; i < strides.size(); ++i) {
			size *= shape[i];
			strides[i] = strides[i - 1] * shape[i - 1];
		}
		return size;
	}
}

/// Cumulative mutliplication of a shape to extract its size
template<class Vector>
inline qsizetype vipCumMultiply(const Vector& shape)
{
	if (!shape.size())
		return 0;
	qsizetype res = shape[0];
	for (qsizetype i = 1; i < shape.size(); ++i)
		res *= shape[i];
	return res;
}
/// Cumulative mutliplication of a shape (based on top left and top right position) to extract its size
template<class Vector1, class Vector2>
inline qsizetype vipCumMultiplyRect(const Vector1& topLeft, const Vector2& bottomRight)
{
	qsizetype res = bottomRight[0] - topLeft[0];
	for (qsizetype i = 1; i < topLeft.size(); ++i)
		res *= (bottomRight[i] - topLeft[i]);
	return res;
}

/// Compute the flat position of given N-Dimension position based on given strides
template<bool Unstrided, class StridesType, qsizetype Dim>
static VIP_ALWAYS_INLINE qsizetype vipFlatOffset(const StridesType& strides, const VipCoordinate< Dim>& pos) noexcept
{
	static const qsizetype NDims = __NDIM2(strides, pos);
	return detail::ComputeFinalOffset<Unstrided, StridesType, VipCoordinate< Dim>, NDims>::offset(strides, pos);
}

template<bool Unstrided, class StridesType, class VectorType>
static VIP_ALWAYS_INLINE qsizetype vipFlatOffset(const StridesType& strides, const VectorType& pos) noexcept
{
	static const qsizetype NDims = __NDIM2(strides, pos);
	return detail::ComputeFinalOffset<Unstrided, StridesType, VectorType, NDims>::offset(strides, pos);
}

/// Returns true if the flat sizes of 2 shapes are the same
template<class VectorType1, class VectorType2>
inline bool vipCompareShapeSize(const VectorType1& sh1, const VectorType2 sh2)
{
	return vipCumMultiply(sh1) == vipCumMultiply(sh2);
}

namespace iter_detail
{

	template<Vip::Ordering order, qsizetype N, class Shape>
	VIP_INLINE bool setFlatPos(VipCoordinate< N>& coord, const Shape& shape, qsizetype offset)
	{
		static const qsizetype static_size = __NDIM2(coord, shape);
		VipCoordinate< static_size> strides;
		vipComputeDefaultStrides<order>(shape, strides);
		for (qsizetype i = 0; i < strides.size(); ++i) {
			coord[i] = (offset / strides[i]);
			offset = (offset % strides[i]);
		}
		return true;
	}
	template<Vip::Ordering order, qsizetype N, qsizetype N2, qsizetype N3>
	VIP_INLINE bool setFlatPos(VipCoordinate< N>& coord, const VipCoordinate< N2>& start, const VipCoordinate< N3>& end, qsizetype offset)
	{
		static const qsizetype static_size = N > 0 ? N : (N2 > 0 ? N2 : N3);
		VipCoordinate< static_size> sh;
		sh.resize(start.size());
		for (qsizetype i = 0; i < sh.size(); ++i)
			sh[i] = end[i] - start[i];
		VipCoordinate< static_size> strides;
		vipComputeDefaultStrides<order>(sh, strides);
		for (qsizetype i = 0; i < sh.size(); ++i) {
			coord[i] = (offset / strides[i]) + start[i];
			offset = (offset % strides[i]);
		}
		return true;
	}
}

namespace detail
{
	// increment a position
	template<Vip::Ordering ordering>
	struct ReachEndIncrement;

	template<>
	struct ReachEndIncrement<Vip::FirstMajor>
	{
		template<class Position, class Shape>
		VIP_ALWAYS_INLINE static bool apply(const Position& pos, const Shape& sh)
		{
			return pos[0] == sh[0];
		}
	};
	template<>
	struct ReachEndIncrement<Vip::LastMajor>
	{
		template<class Position, class Shape>
		VIP_ALWAYS_INLINE static bool apply(const Position& pos, const Shape& sh)
		{
			return pos.back() == sh.back();
		}
	};

	template<Vip::Ordering ordering>
	struct ContinueIncrement;

	template<>
	struct ContinueIncrement<Vip::FirstMajor>
	{
		template<class Position, class Shape>
		VIP_ALWAYS_INLINE static bool apply(const Position& pos, const Shape& sh)
		{
			return pos[0] != sh[0];
		}
	};
	template<>
	struct ContinueIncrement<Vip::LastMajor>
	{
		template<class Position, class Shape>
		VIP_ALWAYS_INLINE static bool apply(const Position& pos, const Shape& sh)
		{
			return pos.back() != sh.back();
		}
	};

	template<class Shape = void>
	struct GetStart
	{
		static VIP_ALWAYS_INLINE qsizetype get(const Shape* start, qsizetype i) { return (*start)[i]; }
	};
	template<>
	struct GetStart<void>
	{
		static VIP_ALWAYS_INLINE qsizetype get(const void*, qsizetype) { return 0; }
	};

	template<qsizetype Size>
	struct DecrementCoordFirstMajor
	{
		template<class Position, class Shape>
		static void apply_inner(Position& pos, const Shape& sh)
		{
			pos.back() = sh.back() - 1;
			qsizetype index = pos.size() - 2;
			while (--pos[index] < 0) {
				if (index == 0)
					break;
				pos[index] = sh[index] - 1;
				--index;
			}
		}
		template<class Position, class Shape>
		static VIP_ALWAYS_INLINE void apply(Position& pos, const Shape& sh, qsizetype dimCount)
		{
			if (--pos[dimCount - 1] < 0)
				apply_inner(pos, sh);
		}
	};
	template<>
	struct DecrementCoordFirstMajor<1>
	{
		template<class Position, class Shape>
		static VIP_ALWAYS_INLINE void apply(Position& pos, const Shape& sh, qsizetype dimCount)
		{
			--pos[0];
		}
	};
	template<>
	struct DecrementCoordFirstMajor<2>
	{
		template<class Position, class Shape>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype)
		{
			if (--pos[1] < 0) {
				pos[1] = sh[1] - 1;
				--pos[0];
			}
		}
	};

	template<qsizetype Size, Vip::Ordering ordering, qsizetype Incr>
	struct IncrementCoord;

	template<qsizetype Size, Vip::Ordering ordering>
	struct IncrementCoordCheckContinue;

	template<qsizetype Size>
	struct IncrementCoord<Size, Vip::FirstMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		static void apply_inner(Position& pos, const Shape& sh, const StartShape* start = nullptr)
		{
			pos.back() = GetStart<StartShape>::get(start, pos.size() - 1);
			qsizetype index = pos.size() - 2;
			while (++pos[index] == sh[index]) {
				if (index == 0)
					break;
				pos[index] = GetStart<StartShape>::get(start, index);
				--index;
			}
		}

		template<class Position, class Shape, class StartShape = void>
		static VIP_ALWAYS_INLINE void apply(Position& pos, const Shape& sh, qsizetype dimCount, const StartShape* start = nullptr)
		{
			--dimCount;
			if (++pos[dimCount] == sh[dimCount])
				apply_inner(pos, sh, start);
		}
	};
	template<qsizetype Size, qsizetype Incr>
	struct IncrementCoord<Size, Vip::FirstMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		static void apply(Position& pos, const Shape& sh, qsizetype dimCount, const StartShape* start = nullptr)
		{
			--dimCount;
			if (sh[dimCount] - pos[dimCount] > Incr) {
				pos[dimCount] += Incr;
			}
			else {
				qsizetype remaining = Incr - (sh[dimCount] - pos[dimCount]) + 1;
				pos.back() = sh.back() - 1 + GetStart<StartShape>::get(start, pos.size() - 1);
				for (qsizetype i = 0; i < remaining; ++i)
					IncrementCoord<Size, Vip::FirstMajor, 1>::apply(pos, sh, dimCount);
			}
		}
	};
	template<qsizetype Size>
	struct IncrementCoordCheckContinue<Size, Vip::FirstMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		static bool apply_end_inner(Position& pos, const Shape& sh, const StartShape* start = nullptr)
		{
			pos.back() = GetStart<StartShape>::get(start, pos.size() - 1);
			qsizetype index = pos.size() - 2;
			while (++pos[index] == sh[index]) {
				if (index == 0)
					return pos[0] != sh[0];
				pos[index] = GetStart<StartShape>::get(start, index);
				--index;
			}
			return true;
		}

		template<class Position, class Shape, class StartShape = void>
		static VIP_ALWAYS_INLINE bool apply(Position& pos, const Shape& sh, qsizetype dimCount, const StartShape* start = nullptr)
		{

			if (++pos[dimCount - 1] == sh[dimCount - 1]) {
				return apply_end_inner(pos, sh, start);
			}
			return true;
		}
	};

	template<>
	struct IncrementCoord<1, Vip::FirstMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape&, qsizetype, const StartShape* = nullptr)
		{
			++pos[0];
		}
	};
	template<qsizetype Incr>
	struct IncrementCoord<1, Vip::FirstMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape&, qsizetype, const StartShape* = nullptr)
		{
			pos[0] += Incr;
		}
	};
	template<>
	struct IncrementCoordCheckContinue<1, Vip::FirstMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* = nullptr)
		{
			return ++pos[0] != sh[0];
		}
	};

	template<>
	struct IncrementCoord<2, Vip::FirstMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[1] == sh[1]) {
				pos[1] = GetStart<StartShape>::get(start, 1);
				++pos[0];
			}
		}
	};
	template<qsizetype Incr>
	struct IncrementCoord<2, Vip::FirstMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			pos[1] += Incr;
			while (pos[1] >= sh[1]) {
				pos[1] = pos[1] - sh[1] + GetStart<StartShape>::get(start, 1);
				++pos[0];
			}
		}
	};
	template<>
	struct IncrementCoordCheckContinue<2, Vip::FirstMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[1] == sh[1]) {
				pos[1] = GetStart<StartShape>::get(start, 1);
				return ++pos[0] != sh[0];
			}
			return true;
		}
	};

	template<>
	struct IncrementCoord<3, Vip::FirstMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[2] == sh[2]) {
				pos[2] = GetStart<StartShape>::get(start, 2);
				if (++pos[1] == sh[1]) {
					pos[1] = GetStart<StartShape>::get(start, 1);
					++pos[0];
				}
			}
		}
	};
	template<qsizetype Incr>
	struct IncrementCoord<3, Vip::FirstMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			pos[2] += Incr;
			while (pos[2] >= sh[2]) {
				pos[2] = pos[2] - sh[2] + GetStart<StartShape>::get(start, 2);
				if (++pos[1] == sh[1]) {
					pos[1] = GetStart<StartShape>::get(start, 1);
					++pos[0];
				}
			}
		}
	};
	template<>
	struct IncrementCoordCheckContinue<3, Vip::FirstMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[2] == sh[2]) {
				pos[2] = GetStart<StartShape>::get(start, 2);
				if (++pos[1] == sh[1]) {
					pos[1] = GetStart<StartShape>::get(start, 1);
					return ++pos[0] != sh[0];
				}
			}
			return true;
		}
	};

	template<qsizetype Size>
	struct IncrementCoord<Size, Vip::LastMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[0] == sh[0]) {
				// update position
				for (qsizetype i = 1; i < pos.size(); ++i) {
					if (++pos[i] == sh[i]) {
						pos[i] = GetStart<StartShape>::get(start, i);
						if (i == pos.size() - 1)
							pos[i] = sh[i]; // last iteration
					}
					else
						break;
				}
				pos[0] = 0;
			}
		}
	};
	template<qsizetype Size, qsizetype Incr>
	struct IncrementCoord<Size, Vip::LastMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (sh[0] - pos[0] > Incr) {
				pos[0] += Incr;
			}
			else {
				qsizetype remaining = Incr - sh[0] + pos[0] + 1;
				pos[0] = sh[0] - 1 + GetStart<StartShape>::get(start, 0);
				for (qsizetype i = 0; i < remaining; ++i)
					IncrementCoord<Size, Vip::LastMajor, 1>::apply(pos, sh, 0);
			}
		}
	};
	template<qsizetype Size>
	struct IncrementCoordCheckContinue<Size, Vip::LastMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[0] == sh[0]) {
				// update position
				for (qsizetype i = 1; i < pos.size(); ++i) {
					if (++pos[i] == sh[i]) {
						pos[i] = GetStart<StartShape>::get(start, i);
						if (i == pos.size() - 1)
							pos[i] = sh[i]; // last iteration
					}
					else
						break;
				}
				pos[0] = 0;
				return pos.back() != sh.back();
			}
			return true;
		}
	};

	template<>
	struct IncrementCoord<2, Vip::LastMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[0] == sh[0]) {
				pos[0] = GetStart<StartShape>::get(start, 0);
				++pos[1];
			}
		}
	};
	template<qsizetype Incr>
	struct IncrementCoord<2, Vip::LastMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (sh[0] - pos[0] > Incr)
				pos[0] += Incr;
			else {
				qsizetype remaining = Incr - sh[0] + pos[0] + 1;
				pos[0] = sh[0] - 1 + GetStart<StartShape>::get(start, 0);
				for (qsizetype i = 0; i < remaining; ++i)
					IncrementCoord<2, Vip::LastMajor, 1>::apply(pos, sh, 0);
			}
		}
	};
	template<>
	struct IncrementCoordCheckContinue<2, Vip::LastMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[0] == sh[0]) {
				pos[0] = GetStart<StartShape>::get(start, 0);
				;
				return ++pos[1] != sh[1];
			}
			return true;
		}
	};

	template<>
	struct IncrementCoord<1, Vip::LastMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape&, qsizetype, const StartShape* = nullptr)
		{
			++pos[0];
		}
	};
	template<qsizetype Incr>
	struct IncrementCoord<1, Vip::LastMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape&, qsizetype, const StartShape* = nullptr)
		{
			pos[0] += Incr;
		}
	};
	template<>
	struct IncrementCoordCheckContinue<1, Vip::LastMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* = nullptr)
		{
			return ++pos[0] != sh[0];
		}
	};

	template<>
	struct IncrementCoord<3, Vip::LastMajor, 1>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[0] == sh[0]) {
				pos[0] = GetStart<StartShape>::get(start, 0);
				if (++pos[1] == sh[1]) {
					pos[1] = GetStart<StartShape>::get(start, 1);
					++pos[2];
				}
			}
		}
	};
	template<qsizetype Incr>
	struct IncrementCoord<3, Vip::LastMajor, Incr>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static void apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			pos[0] += Incr;
			while (pos[0] >= sh[0]) {
				pos[0] = pos[0] - sh[0] + GetStart<StartShape>::get(start, 0);
				if (++pos[1] == sh[1]) {
					pos[1] = GetStart<StartShape>::get(start, 1);
					++pos[2];
				}
			}
		}
	};
	template<>
	struct IncrementCoordCheckContinue<3, Vip::LastMajor>
	{
		template<class Position, class Shape, class StartShape = void>
		VIP_ALWAYS_INLINE static bool apply(Position& pos, const Shape& sh, qsizetype, const StartShape* start = nullptr)
		{
			if (++pos[0] == sh[0]) {
				pos[0] = GetStart<StartShape>::get(start, 0);
				if (++pos[1] == sh[1]) {
					pos[1] = GetStart<StartShape>::get(start, 1);
					return ++pos[2] != sh[2];
				}
			}
			return true;
		}
	};

	// template<Ordering order>
	//  struct Increment;
	//
	// template<>
	// struct Increment<FirstMajor>
	// {
	// template< class Position, class VipShape>
	// static bool apply(Position & pos, const VipShape & shape)
	// {
	// if(++pos.back() != shape.back())
	//	return false;
	// {
	//	pos.back() = 0;
	//	for(qsizetype i = pos.size()-2; i!= Vip::None; --i)
	//	{
	//		++pos[i];
	//		if(i!=0)
	//		{
	//			if(pos[i] == shape[i] ) pos[i] = 0;
	//			else break;
	//		}
	//	}
	//	return true;
	// }
	// }
	// };
	//
	// template<>
	// struct Increment<LastMajor>
	// {
	// template< class Position, class VipShape>
	// static bool apply(Position & pos, const VipShape & shape)
	// {
	// if(++pos[0] == shape.front())
	// {
	//	//update position
	//	pos[0] = 0;
	//	for(qsizetype i=1; i < shape.size(); ++i) {
	//		if(++pos[i] == shape[i] ) {
	//			pos[i] = 0;
	//			if(i==shape.size()-1) pos[i] = shape.back(); //last iteration
	//		}
	//		else
	//			break;
	//	}
	//
	//	return true;
	// }
	// else
	//	return false;
	//
	//
	// }
	// };

	///
	template<class ShapeType>
	struct CIteratorFMajorNoSkip
	{
		typedef qsizetype skip_type;
		typedef ShapeType shape_type;
		typedef ShapeType position_type;

		// array shape
		const ShapeType shape;
		// current position
		position_type pos;
		// total size
		qsizetype size;

		CIteratorFMajorNoSkip(const ShapeType& sh, skip_type = 0)
		  : shape(sh)
		{
			size = vipCumMultiply(shape);
			pos.resize(sh.size());
			pos.fill(0);
		}

		inline qsizetype innerDimensionIndex() const { return shape.size() - 1; }
		qsizetype innerPosition() const { return pos[innerDimensionIndex()]; }
		qsizetype totalIterationCount() const { return size; }

		qsizetype flatPosition() const
		{
			qsizetype stride = 1;
			qsizetype flat = 0;
			for (qsizetype i = shape.size() - 1; i >= 0; --i) {
				flat += stride * pos[i];
				stride *= shape[i];
			}

			return flat;
		}

		void setFlatPosition(qsizetype offset)
		{
			ShapeType strides;
			vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides);
			for (qsizetype i = 0; i < shape.size(); ++i) {
				pos[i] = (offset / strides[i]);
				offset = (offset % strides[i]);
			}
		}

		void advance(qsizetype offset) { setFlatPosition(offset + flatPosition()); }

		void increment()
		{
			IncrementCoord<ShapeType::static_size, Vip::FirstMajor, 1>::apply(pos, shape, pos.size());
			// return Increment<FirstMajor>::apply(pos,shape);
		}

		bool decrement()
		{
			--pos[innerDimensionIndex()];
			if (pos[innerDimensionIndex()] == qsizetype(-1)) {
				// update position
				pos[innerDimensionIndex()] = shape.back() - 1;
				for (qsizetype i = shape.size() - 2; i >= 0; --i) {
					if (--pos[i] == (qsizetype)(-1)) {
						pos[i] = shape[i] - 1;
						if (i == 0)
							pos[i] = 0; // last iteration
					}
					else
						break;
				}

				return true;
			}
			else
				return false;
		}
	};

	template<class ShapeType>
	struct CIteratorFMajorSkipDim
	{
		typedef qsizetype skip_type;
		typedef ShapeType shape_type;
		typedef ShapeType position_type;

		// array shape
		const ShapeType shape;
		// current position
		position_type pos;
		// the dimension to skip
		qsizetype skip;
		// total size
		qsizetype size;

		inline qsizetype innerDimensionIndex() const
		{
			if (skip == (shape.size() - 1) && shape.size() > 1)
				return shape.size() - 2;
			else
				return shape.size() - 1;
		}

		CIteratorFMajorSkipDim(const ShapeType& sh, qsizetype skip_dim = 0)
		  : shape(sh)
		  , skip(skip_dim)
		{
			// ARRAY_CHECK_PREDICATE(!(skip == 0 && sh->size()==1),
			//  "CIteratorFMajorSkipDim: cannot skip the first dimension is there is one shape");

			size = vipCumMultiply(sh) / sh[skip];
			pos.resize(sh.size());
			pos.fill(0);
		}

		qsizetype innerPosition() const { return pos[shape.size() - 1]; }
		qsizetype totalIterationCount() const { return size; }

		qsizetype flatPosition() const
		{
			qsizetype stride = 1;
			qsizetype flat = 0;
			for (qsizetype i = shape.size() - 1; i >= 0; --i) {
				flat += stride * pos[i];
				stride *= shape[i];
			}
			return flat;
		}

		void setFlatPosition(qsizetype offset)
		{
			VipNDArrayShape new_shape(shape.size() - 1);
			qsizetype index = 0;
			for (qsizetype i = 0; i < shape.size(); ++i)
				if (i != skip)
					new_shape[index++] = pos[i];

			index = 0;

			VipNDArrayShape strides;
			vipComputeDefaultStrides<Vip::FirstMajor>(new_shape, strides);
			for (qsizetype i = 0; i < strides.size(); ++i) {
				if (i != skip) {
					pos[i] = (offset / strides[index]);
					offset = (offset % strides[index]);
					++index;
				}
			}
		}

		void increment()
		{
			++pos[innerDimensionIndex()];

			if (pos[innerDimensionIndex()] == shape[innerDimensionIndex()]) {
				// update position
				pos[innerDimensionIndex()] = 0;
				for (qsizetype i = innerDimensionIndex() - 1; i >= 0; --i) {
					if (i == (qsizetype)skip)
						continue;
					else if (++pos[i] == shape[i])
						pos[i] = 0;
					else
						break;
				}

				// return true;
			}
			// else
			//	return false;
		}

		bool decrement()
		{
			--pos[innerDimensionIndex()];

			if (pos[innerDimensionIndex()] == qsizetype(-1)) {
				// update position
				pos[innerDimensionIndex()] = shape[innerDimensionIndex()] - 1;
				for (qsizetype i = innerDimensionIndex() - 1; i >= 0; --i) {
					if (i == (qsizetype)skip)
						continue;
					else if (--pos[i] == (qsizetype)(-1)) {
						pos[i] = shape[i] - 1;
					}
					else
						break;
				}

				return true;
			}
			else
				return false;
		}
	};

	///
	template<class ShapeType>
	struct CIteratorFMajorSkipRect
	{

		typedef ShapeType shape_type;
		typedef ShapeType position_type;
		typedef QPair<position_type, position_type> rect_type;
		typedef rect_type skip_type;

		// array shape
		const ShapeType shape;
		// current position
		position_type pos;
		rect_type rect;
		// total size
		qsizetype size;

		CIteratorFMajorSkipRect(const ShapeType& sh, const rect_type& r = rect_type())
		  : shape(sh)
		  , rect(r)
		{
			pos.resize(sh.size());
			pos.fill(0);

			size = vipCumMultiply(shape) - vipCumMultiplyRect(rect.first, rect.second);

			// ARRAY_CHECK_PREDICATE(cumulativeSum((r.second-r.first) >= 1u) == sh->size(), "CIteratorFMajorSkipRect: given rectangle is too big");
			//  ARRAY_CHECK_PREDICATE(cumulativeSum(r.second<= sh->fullShape()) == sh->size(), "CIteratorFMajorSkipRect: given rectangle is outside the shape");
			//  ARRAY_CHECK_PREDICATE(cumulativeSum(r.first < r.second) == sh->size(), "CIteratorFMajorSkipRect: given rectangle is empty");

			// check that the starting position (0) is valid, and update it if necessary
			for (qsizetype i = 0; i < shape.size(); ++i)
				if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
					return;

			pos[innerDimensionIndex()] = rect.second[innerDimensionIndex()];
		}

		inline qsizetype innerDimensionIndex() const { return shape.size() - 1; }
		qsizetype innerPosition() const { return pos[innerDimensionIndex()]; }
		qsizetype totalIterationCount() const { return size; }

		void increment()
		{
			++pos[innerDimensionIndex()];

			qsizetype inner_pos = pos[innerDimensionIndex()];

			if (inner_pos != shape.back()) {
				// as long as the inner position is not inside the rect, do nothing
				if (inner_pos < rect.first[innerDimensionIndex()] || inner_pos >= rect.second[innerDimensionIndex()])
					return; // false;

				// check that all other dims are inside
				for (qsizetype i = 0; i < shape.size() - 1; ++i)
					if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
						return; // false;

				// push the inner position outside the rect
				pos[innerDimensionIndex()] = rect.second[innerDimensionIndex()];
				if (rect.second[innerDimensionIndex()] == shape.back()) {
					pos[innerDimensionIndex()] = 0;
					for (qsizetype i = innerDimensionIndex() - 1; i >= 0; --i) {
						++pos[i];
						if (pos[i] == shape[i])
							pos[i] = 0;
						else
							break;
					}
				}

				return; // true;
			}
			else {
				pos[innerDimensionIndex()] = 0;
				for (qsizetype i = innerDimensionIndex() - 1; i >= 0; --i) {
					++pos[i];
					if (pos[i] == shape[i])
						pos[i] = 0;
					else
						break;
				}

				if (rect.first[innerDimensionIndex()] == 0) {
					qsizetype i = 0;
					for (; i < shape.size() - 1; ++i)
						if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
							break;
					// all dimensions inside rect
					if (i == shape.size() - 1)
						pos[innerDimensionIndex()] = rect.second[innerDimensionIndex()];
				}

				return; // true;
			}
		}

		bool decrement()
		{
			--pos[innerDimensionIndex()];

			if (pos[innerDimensionIndex()] != qsizetype(-1)) {
				// as long as the inner position is not inside the rect, do nothing
				if (pos[innerDimensionIndex()] < rect.first[innerDimensionIndex()] || pos[innerDimensionIndex()] >= rect.second[innerDimensionIndex()])
					return false;

				// check that all other dims are inside
				for (qsizetype i = 0; i < shape.size() - 1; ++i)
					if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
						return false;

				// push the inner position outside the rect
				pos[innerDimensionIndex()] = rect.first[innerDimensionIndex()] - 1;
				if (rect.first[innerDimensionIndex()] == 0) {
					pos[innerDimensionIndex()] = shape.back() - 1;
					for (qsizetype i = shape.size() - 2; i >= 0; --i) {
						if (--pos[i] == (qsizetype)(-1)) {
							pos[i] = shape[i] - 1;
						}
						else
							break;
					}
				}

				return true;
			}
			else {
				pos[innerDimensionIndex()] = shape.back() - 1;
				for (qsizetype i = shape.size() - 2; i >= 0; --i) {
					if (--pos[i] == (qsizetype)(-1)) {
						pos[i] = shape[i] - 1;
					}
					else
						break;
				}

				if (rect.second[innerDimensionIndex()] == shape.back()) {
					qsizetype i = 0;
					for (; i < shape.size() - 1; ++i)
						if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
							break;
					// all dimensions inside rect
					if (i == shape.size() - 1)
						pos[innerDimensionIndex()] = rect.first[innerDimensionIndex()] - 1;
				}

				return true;
			}
		}
	};

	///
	template<class ShapeType>
	struct CIteratorLMajorNoSkip
	{

		typedef qsizetype skip_type;
		typedef ShapeType shape_type;
		typedef typename ShapeType::full_shape_type position_type;

		// array shape
		const ShapeType shape;
		// current position
		position_type pos;
		// total size;
		qsizetype size;

		CIteratorLMajorNoSkip(const ShapeType& sh, skip_type = 0)
		  : shape(sh)
		{
			pos.resize(sh->size());
			pos.fill(0);
			size = vipCumMultiply(sh);
		}

		inline qsizetype innerDimensionIndex() const { return 0; }
		qsizetype innerPosition() const { return pos[0]; }
		qsizetype totalIterationCount() const { return size; }

		qsizetype flatPosition() const
		{
			qsizetype stride = 1;
			qsizetype flat = 0;
			for (qsizetype i = 0; i < shape.size(); ++i) {
				flat += stride * pos[i];
				stride *= shape[i];
			}
			return flat;
		}

		void setFlatPosition(qsizetype offset)
		{
			ShapeType strides;
			vipComputeDefaultStrides<Vip::LastMajor>(shape, strides);
			for (qsizetype i = shape.size() - 1; i >= 0; --i) {
				pos[i] = (offset / strides[i]);
				offset = (offset % strides[i]);
			}
		}

		void advance(qsizetype offset) { setFlatPosition(offset + flatPosition()); }

		void increment()
		{
			IncrementCoord<ShapeType::static_size, Vip::LastMajor, 1>::apply(pos, shape, pos.size());
			// return Increment<LastMajor>::apply(pos,shape);
		}

		bool decrement()
		{
			--pos[0];
			if (pos[0] == qsizetype(-1)) {
				// update position
				pos[0] = shape.front() - 1;
				for (qsizetype i = 1; i < shape.size(); ++i) {
					if (--pos[i] == (qsizetype)(-1)) {
						pos[i] = shape[i] - 1;
						if (i == shape.size() - 1)
							pos[i] = 0; // last iteration
					}
					else
						break;
				}

				return true;
			}
			else
				return false;
		}
	};

	template<class ShapeType>
	struct CIteratorLMajorSkipDim
	{
		typedef qsizetype skip_type;
		typedef ShapeType shape_type;
		typedef ShapeType position_type;

		// array shape
		const ShapeType shape;
		// current position
		position_type pos;
		// the dimension to skip
		qsizetype skip;
		// total size
		qsizetype size;

		inline qsizetype innerDimensionIndex() const
		{
			if (skip == 0 && shape.size() > 1)
				return 1;
			else
				return 0;
		}

		CIteratorLMajorSkipDim(const ShapeType& sh, qsizetype skip_dim = 0)
		  : shape(sh)
		  , skip(skip_dim)
		{
			// ARRAY_CHECK_PREDICATE(!(skip == 0 && sh->size()==1),
			//  "CIteratorLMajorSkipDim: cannot skip the first dimension is there is one shape");

			pos.resize(sh->size());
			pos.fill(0);
			size = vipCumMultiply(sh) / shape[skip];
		}

		qsizetype innerPosition() const { return pos[0]; }
		qsizetype totalIterationCount() const { return size; }

		qsizetype flatPosition() const
		{
			qsizetype stride = 1;
			qsizetype flat = 0;
			for (qsizetype i = 0; i < shape.size(); ++i) {
				flat += stride * pos[i];
				stride *= shape[i];
			}
			return flat;
		}

		void increment()
		{
			++pos[innerDimensionIndex()];

			if (pos[innerDimensionIndex()] == shape.shape(innerDimensionIndex())) {
				// update position
				pos[innerDimensionIndex()] = 0;
				for (qsizetype i = innerDimensionIndex() + 1; i < shape.size(); ++i) {
					if (i == (qsizetype)skip)
						continue;
					else if (++pos[i] == shape[i])
						pos[i] = 0;
					else
						break;
				}

				// return true;
			}
			// else
			//	return false;
		}

		bool decrement()
		{
			--pos[innerDimensionIndex()];

			if (pos[innerDimensionIndex()] == qsizetype(-1)) {
				// update position
				pos[innerDimensionIndex()] = shape.shape(innerDimensionIndex()) - 1;
				for (qsizetype i = innerDimensionIndex() + 1; i < shape.size(); ++i) {
					if (i == (qsizetype)skip)
						continue;
					else if (--pos[i] == (qsizetype)(-1)) {
						pos[i] = shape[i] - 1;
					}
					else
						break;
				}

				return true;
			}
			else
				return false;
		}
	};

	///
	template<class ShapeType>
	struct CIteratorLMajorSkipRect
	{

		typedef ShapeType shape_type;
		typedef ShapeType position_type;
		typedef QPair<position_type, position_type> rect_type;
		typedef rect_type skip_type;

		// array shape
		const ShapeType shape;
		// current position
		position_type pos;
		rect_type rect;
		// total size
		qsizetype size;

		CIteratorLMajorSkipRect(const ShapeType& sh, const rect_type& r = rect_type())
		  : shape(sh)
		  , rect(r)
		{
			pos.resize(sh->size());
			pos.fill(0);

			// ARRAY_CHECK_PREDICATE(cumulativeSum((r.second-r.first) >= 1u) == sh->size(), "CIteratorFMajorSkipRect: given rectangle is too big");
			//  ARRAY_CHECK_PREDICATE(cumulativeSum(r.second<= sh->fullShape()) == sh->size(), "CIteratorFMajorSkipRect: given rectangle is outside the shape");
			//  ARRAY_CHECK_PREDICATE(cumulativeSum(r.first < r.second) == sh->size(), "CIteratorFMajorSkipRect: given rectangle is empty");

			for (qsizetype i = 0; i < shape.size(); ++i)
				if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
					return;

			pos[innerDimensionIndex()] = rect.second[innerDimensionIndex()];
			size = vipCumMultiply(sh) - vipCumMultiplyRect(rect.first, rect.second);
		}

		inline qsizetype innerDimensionIndex() const { return 0; }
		qsizetype innerPosition() const { return pos[0]; }
		qsizetype totalIterationCount() const { return size; }

		qsizetype flatPosition() const
		{
			qsizetype stride = 1;
			qsizetype flat = 0;
			for (qsizetype i = 0; i < shape.size(); ++i) {
				flat += stride * pos[i];
				stride *= shape[i];
			}
			return flat;
		}

		void increment()
		{
			++pos[innerDimensionIndex()];

			qsizetype inner_pos = pos[innerDimensionIndex()];

			if (inner_pos != shape.front()) {
				// as long as the inner position is not inside the rect, do nothing
				if (inner_pos < rect.first[innerDimensionIndex()] || inner_pos >= rect.second[innerDimensionIndex()])
					return; // false;

				// check that all other dims are inside
				for (qsizetype i = 1; i < shape.size(); ++i)
					if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
						return; // false;

				// push the inner position outside the rect
				pos[innerDimensionIndex()] = rect.second[innerDimensionIndex()];
				if (rect.second[innerDimensionIndex()] == shape.front()) {
					pos[innerDimensionIndex()] = 0;
					for (qsizetype i = innerDimensionIndex() + 1; i < shape.size(); ++i) {
						++pos[i];
						if (pos[i] == shape[i])
							pos[i] = 0;
						else
							break;
					}
				}

				return; // true;
			}
			else {
				pos[innerDimensionIndex()] = 0;
				for (qsizetype i = innerDimensionIndex() + 1; i < shape.size(); ++i) {
					++pos[i];
					if (pos[i] == shape[i])
						pos[i] = 0;
					else
						break;
				}

				if (rect.first[innerDimensionIndex()] == 0) {
					qsizetype i = 1;
					for (; i < shape.size(); ++i)
						if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
							break;
					// all dimensions inside rect
					if (i == shape.size())
						pos[innerDimensionIndex()] = rect.second[innerDimensionIndex()];
				}

				return; // true;
			}
		}

		bool decrement()
		{
			--pos[innerDimensionIndex()];

			if (pos[innerDimensionIndex()] != qsizetype(-1)) {
				// as long as the inner position is not inside the rect, do nothing
				if (pos[innerDimensionIndex()] < rect.first[innerDimensionIndex()] || pos[innerDimensionIndex()] >= rect.second[innerDimensionIndex()])
					return false;

				// check that all other dims are inside
				for (qsizetype i = 1; i < shape.size(); ++i)
					if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
						return false;

				// push the inner position outside the rect
				pos[innerDimensionIndex()] = rect.first[innerDimensionIndex()] - 1;
				if (rect.first[innerDimensionIndex()] == 0) {
					pos[innerDimensionIndex()] = shape.back() - 1;
					for (qsizetype i = innerDimensionIndex() + 1; i < shape.size(); ++i) {
						if (--pos[i] == (qsizetype)(-1)) {
							pos[i] = shape[i] - 1;
						}
						else
							break;
					}
				}

				return true;
			}
			else {
				pos[innerDimensionIndex()] = shape.front() - 1;
				for (qsizetype i = innerDimensionIndex() + 1; i < shape.size(); ++i) {
					if (--pos[i] == (qsizetype)(-1)) {
						pos[i] = shape[i] - 1;
					}
					else
						break;
				}

				if (rect.second[innerDimensionIndex()] == shape.front()) {
					qsizetype i = 1;
					for (; i < shape.size(); ++i)
						if (pos[i] < rect.first[i] || pos[i] >= rect.second[i])
							break;
					// all dimensions inside rect
					if (i == shape.size())
						pos[innerDimensionIndex()] = rect.first[innerDimensionIndex()] - 1;
				}

				return true;
			}
		}
	};

} // end detail

/// Iterator over a strided array
template<class TYPE, qsizetype Dim = Vip::None>
struct VipNDSubArrayConstIterator
{
	// array shape
	VipCoordinate< Dim> pos;
	VipCoordinate< Dim> shape;
	VipCoordinate< Dim> strides;
	const TYPE* ptr;
	qsizetype abspos;
	qsizetype full_size;

	VIP_ALWAYS_INLINE void increment() noexcept
	{
		++abspos;
		detail::IncrementCoord<Dim, Vip::FirstMajor, 1>::apply(pos, shape, pos.size());
	}
	VIP_ALWAYS_INLINE void decrement() noexcept
	{
		if (abspos == full_size) { // end
			pos.resize(shape.size());
			iter_detail::setFlatPos<Vip::FirstMajor>(pos, shape, --abspos);
		}
		else {
			--abspos;
			detail::DecrementCoordFirstMajor<Dim>::apply(pos, shape, pos.size());
		}
	}

public:
	typedef std::random_access_iterator_tag iterator_category;
	typedef TYPE value_type;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;
	typedef TYPE* pointer;
	typedef const TYPE* const_pointer;
	typedef TYPE& reference;
	typedef const TYPE& const_reference;

	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator() noexcept {}
	template<class ShapeType>
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator(const ShapeType& sh, const ShapeType& st, const TYPE* ptr, qsizetype full_size) noexcept // begin
	  : shape(sh)
	  , strides(st)
	  , ptr(ptr)
	  , abspos(0)
	  , full_size(full_size) // begin
	{
		pos.resize(sh.size());
		pos.fill(0);
	}
	template<class ShapeType>
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator(const ShapeType& sh, const ShapeType& st, const TYPE* ptr, qsizetype full_size, qsizetype pos) noexcept // end
	  : shape(sh)
	  , strides(st)
	  , ptr(ptr)
	  , abspos(pos)
	  , full_size(full_size)
	{
	}
	VIP_ALWAYS_INLINE const_reference operator*() const noexcept { return ptr[vipFlatOffset<false>(strides, pos)]; }
	VIP_ALWAYS_INLINE const_pointer operator->() const noexcept { return std::pointer_traits<const_pointer>::pointer_to(**this); }
	void setAbsPos(qsizetype new_pos) noexcept
	{
		if (abspos != new_pos) {
			abspos = new_pos;
			pos.resize(shape.size());
			iter_detail::setFlatPos<Vip::FirstMajor>(pos, shape, abspos);
		}
	}
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator& operator++() noexcept
	{
		increment();
		return (*this);
	}
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator operator++(int) noexcept
	{
		VipNDSubArrayConstIterator temp(*this);
		increment();
		return temp;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator& operator--() noexcept
	{
		decrement();
		return (*this);
	}
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator operator--(int) noexcept
	{
		VipNDSubArrayConstIterator temp(*this);
		decrement();
		return temp;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator& operator+=(qsizetype diff) noexcept
	{
		if (diff == 1)
			return ++(*this);
		else if (diff == 2)
			return ++(++(*this));
		else if (diff == 3)
			return ++(++(++(*this)));
		setAbsPos(abspos + diff);
		return *this;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayConstIterator& operator-=(qsizetype diff) noexcept
	{
		if (diff == 1)
			return --(*this);
		else if (diff == 2)
			return --(--(*this));
		else if (diff == 3)
			return --(--(--(*this)));
		setAbsPos(abspos - diff);
		return *this;
	}
};

template<class TYPE, qsizetype Dim = Vip::None>
struct VipNDSubArrayIterator : public VipNDSubArrayConstIterator<TYPE, Dim>
{
public:
	using base_type = VipNDSubArrayConstIterator<TYPE, Dim>;
	typedef std::random_access_iterator_tag iterator_category;
	typedef TYPE value_type;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;
	typedef TYPE* pointer;
	typedef const TYPE* const_pointer;
	typedef TYPE& reference;
	typedef const TYPE& const_reference;

	VipNDSubArrayIterator() noexcept {}
	VipNDSubArrayIterator(const VipNDSubArrayConstIterator<TYPE, Dim>& other)
	  : base_type(other)
	{
	}
	template<class ShapeType>
	VIP_ALWAYS_INLINE VipNDSubArrayIterator(const ShapeType& sh, const ShapeType& st, const TYPE* ptr, qsizetype full_size) noexcept // begin
	  : base_type(sh, st, ptr, full_size)
	{
	}
	template<class ShapeType>
	VIP_ALWAYS_INLINE VipNDSubArrayIterator(const ShapeType& sh, const ShapeType& st, const TYPE* ptr, qsizetype full_size, qsizetype pos) noexcept // end
	  : base_type(sh, st, ptr, full_size, pos)
	{
	}

	VIP_ALWAYS_INLINE reference operator*() noexcept { return const_cast<reference>(base_type::operator*()); }
	VIP_ALWAYS_INLINE pointer operator->() noexcept { return std::pointer_traits<pointer>::pointer_to(**this); }
	VIP_ALWAYS_INLINE VipNDSubArrayIterator& operator++() noexcept
	{
		base_type::operator++();
		return *this;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayIterator operator++(int) noexcept
	{
		VipNDSubArrayIterator _Tmp = *this;
		base_type::operator++();
		return _Tmp;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayIterator& operator--() noexcept
	{
		base_type::operator--();
		return *this;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayIterator operator--(int) noexcept
	{
		VipNDSubArrayIterator _Tmp = *this;
		base_type::operator--();
		return _Tmp;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayIterator& operator+=(difference_type diff) noexcept
	{
		base_type::operator+=(diff);
		return *this;
	}
	VIP_ALWAYS_INLINE VipNDSubArrayIterator& operator-=(difference_type diff) noexcept
	{
		base_type::operator-=(diff);
		return *this;
	}
};

template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE VipNDSubArrayConstIterator<TYPE, Dim> operator+(const VipNDSubArrayConstIterator<TYPE, Dim>& it, typename VipNDSubArrayConstIterator<TYPE, Dim>::difference_type diff) noexcept
{
	VipNDSubArrayConstIterator<TYPE, Dim> res = it;
	res += diff;
	return res;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE VipNDSubArrayIterator<TYPE, Dim> operator+(const VipNDSubArrayIterator<TYPE, Dim>& it, typename VipNDSubArrayIterator<TYPE, Dim>::difference_type diff) noexcept
{
	VipNDSubArrayIterator<TYPE, Dim> res = it;
	res += diff;
	return res;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE VipNDSubArrayConstIterator<TYPE, Dim> operator-(const VipNDSubArrayConstIterator<TYPE, Dim>& it, typename VipNDSubArrayConstIterator<TYPE, Dim>::difference_type diff) noexcept
{
	VipNDSubArrayConstIterator<TYPE, Dim> res = it;
	res -= diff;
	return res;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE VipNDSubArrayIterator<TYPE, Dim> operator-(const VipNDSubArrayIterator<TYPE, Dim>& it, typename VipNDSubArrayIterator<TYPE, Dim>::difference_type diff) noexcept
{
	VipNDSubArrayIterator<TYPE, Dim> res = it;
	res -= diff;
	return res;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE typename VipNDSubArrayConstIterator<TYPE, Dim>::difference_type operator-(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos - it2.abspos;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE bool operator==(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos == it2.abspos; // it1.bucket == it2.bucket && it1.ptr == it2.ptr;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE bool operator!=(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos != it2.abspos; // it1.ptr != it2.ptr || it1.bucket != it2.bucket ;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE bool operator<(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos < it2.abspos; // it1.bucket == it2.bucket ? it1.index() < it2.index() : it1.bucket < it2.bucket;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE bool operator>(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos > it2.abspos; // it1.bucket == it2.bucket ? it1.index() > it2.index() : it1.bucket > it2.bucket;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE bool operator<=(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos <= it2.abspos; // it1.bucket == it2.bucket ? it1.index() <= it2.index() : it1.bucket < it2.bucket;
}
template<class TYPE, qsizetype Dim>
VIP_ALWAYS_INLINE bool operator>=(const VipNDSubArrayConstIterator<TYPE, Dim>& it1, const VipNDSubArrayConstIterator<TYPE, Dim>& it2) noexcept
{
	return it1.abspos >= it2.abspos; // it1.bucket == it2.bucket ? it1.index() >= it2.index() : it1.bucket > it2.bucket;
}

// template< class TYPE, qsizetype Dim = Vip::None,  bool InnerUnstrided = false>
//  struct VipNDSubArrayIterator
//  {
//  //array shape
//  VipHybridVector<qsizetype,Dim> shape;
//  //array strides
//  VipHybridVector<qsizetype,Dim> strides;
//  //current pointers in the source data
//  VipHybridVector<TYPE*,Dim> 	 cur_ptr;
//  //end pointers in the source data
//  VipHybridVector<TYPE*,Dim> 	 end_ptr;
//  //data pointer
//  TYPE * data;
//  //end of the inner dimension pointer
//  TYPE * end;
//
//  inline void increment()
//  {
//  //incr src pointer
//  if(InnerUnstrided)
//	++data;
//  else
//	data += strides.back();
//
//  if(data == end)
//	update_position();
//  }
//
//  inline void update_position()
//  {
//  for(qsizetype i= shape.size()-1; i>=0; --i)
//  {
//	if(data == end_ptr[i])
//	{
//		//if we reach the first dimension maximum, return
//		if(i==0)
//			return;
//
//		//incr previous dimension
//		cur_ptr[i-1] += strides[i-1];
//		data = cur_ptr[i-1];
//	}
//	else
//	{
//		for(qsizetype j=i+1; j< shape.size(); j++ )
//		{
//			//reset current pointers and end pointer
//			cur_ptr[j]=data;
//			end_ptr[j]=data + shape[j] * strides[j];
//		}
//		break;
//	}
//  }
//
//  end = data  + shape.back() * strides.back();
//  }
//
//
//  VipNDArrayShape position() const
//  {
//  VipNDArrayShape pos(shape.size());
//  for(qsizetype i=0; i < shape.size(); ++i)
//  {
//	pos[i] = (data - cur_ptr[i])/strides[i];
//  }
//  return pos;
//  }
//
//
//
//  public:
//
//  typedef std::forward_iterator_tag iterator_category;
//  typedef TYPE value_type;
//  typedef qsizetype size_type ;
//  typedef qsizetype difference_type;
//  typedef TYPE* pointer;
//  typedef const TYPE* const_pointer;
//  typedef TYPE& reference;
//  typedef const TYPE& const_reference;
//
//  VipNDSubArrayIterator()
//  :data(nullptr),end(nullptr)
//  {}
//
//  VipNDSubArrayIterator(const VipNDSubArrayIterator & other)
//  :shape(other.shape),
//  strides(other.strides),
//  cur_ptr(other.cur_ptr),
//  end_ptr(other.end_ptr),
//  data(other.data),
//  end(other.end)
//  {}
//
//  VipNDSubArrayIterator(const VipNDArrayShape & sh, const VipNDArrayShape & st, TYPE * ptr, bool begin = true)
//  : data(ptr),
//  end(ptr)
//  {
//  shape = sh;
//  strides = st;
//  cur_ptr.resize(strides.size());
//  cur_ptr.fill(ptr);
//  end_ptr.resize(strides.size());
//  end_ptr.fill(ptr);
//  #ifdef __GNUC__
//  #pragma GCC diagnostic push
//  #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
//  #endif
//  if(!begin)
//	//data point to the end of the array
//	data = ptr + shape[0] * strides[0];
//  else
//  {
//	end = ptr  + shape.back() * strides.back();
//	for(qsizetype i=0; i< shape.size(); ++i)
//		end_ptr[i] = ptr + shape[i] * strides[i];
//  }
//  #ifdef __GNUC__
//  #pragma GCC diagnostic pop
//  #endif
//  }
//
//  VipNDSubArrayIterator(TYPE * end_ptr)
//  :data(end_ptr),end(end_ptr){}
//
//  //iterator like interface
//  reference	operator*()
//  {
//  return *data;
//  }
//
//  const_reference	 operator*() const
//  {
//  return *data;
//  }
//
//  pointer	operator->()
//  {
//  return data;
//  }
//
//  const_pointer	operator->() const
//  {
//  return data;
//  }
//
//  template< class Vector>
//  reference operator()(const Vector & pos)
//  {
//  return *(data + vipFlatOffset<InnerUnstrided>(strides,pos));
//  }
//
//  template< class Vector>
//  const_reference operator()(const Vector & pos) const
//  {
//  return *(data + vipFlatOffset<InnerUnstrided>(strides,pos));
//  }
//
//  bool operator!=(const VipNDSubArrayIterator & other)
//  {
//  return data != other.data;
//  }
//
//  bool operator==(const VipNDSubArrayIterator & other)
//  {
//  return data == other.data;
//  }
//
//  VipNDSubArrayIterator&	operator++()
//  {
//  increment();
//  return (*this);
//  }
//
//  VipNDSubArrayIterator	operator++(qsizetype)
//  {
//  VipNDSubArrayIterator temp(*this);
//  increment();
//  return temp;
//  }
//
//  VipNDSubArrayIterator & operator=(const VipNDSubArrayIterator & other)
//  {
//  (shape) = other.shape;
//  (strides) = other.strides;
//  data = other.data;
//  end = other.end;
//
//  cur_ptr.resize(other.cur_ptr.size());
//  end_ptr.resize(other.end_ptr.size());
//
//  for(qsizetype i=0; i < shape.size(); ++i)
//  {
//	cur_ptr[i] = other.cur_ptr[i];
//	end_ptr[i] = other.end_ptr[i];
//  }
//
//  return *this;
//  }
//  };

/// Iterator over strided array of unkown type
template<qsizetype Dim>
struct VipNDSubArrayIterator<void, Dim>
{
	// array shape
	VipCoordinate<Dim> shape;
	// array strides
	VipCoordinate<Dim> strides;
	// current pointers in the source data
	VipHybridVector<void*, Dim> cur_ptr;
	// end pointers in the source data
	VipHybridVector<void*, Dim> end_ptr;
	// data pointer
	void* data;
	// end of the inner dimension pointer
	void* end;
	// data size
	qsizetype data_size;

	VIP_ALWAYS_INLINE void increment()
	{
		// incr src pointer
		data = static_cast<char*>(data) + data_size * strides.back();
		if (data == end)
			update_position();
	}

	inline void update_position()
	{
		for (qsizetype i = shape.size() - 1; i >= 0; --i) {
			if (data == end_ptr[i]) {
				// if we reach the first dimension maximum, return
				if (i == 0)
					return;

				// incr previous dimension
				cur_ptr[i - 1] = static_cast<char*>(cur_ptr[i - 1]) + data_size * strides[i - 1];
				data = cur_ptr[i - 1];
			}
			else {
				for (qsizetype j = i + 1; j < shape.size(); j++) {
					// reset current pointers and end pointer
					cur_ptr[j] = data;
					end_ptr[j] = static_cast<char*>(data) + data_size * shape[j] * strides[j];
				}
				break;
			}
		}

		end = static_cast<char*>(data) + data_size * shape.back() * strides.back();
	}

public:
	typedef std::forward_iterator_tag iterator_category;
	typedef void* value_type;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;
	typedef void* pointer;
	typedef const void* const_pointer;
	typedef void* reference;
	typedef const void* const_reference;

	VipNDSubArrayIterator(qsizetype data_size)
	  : data(nullptr)
	  , end(nullptr)
	  , data_size(data_size)
	{
	}

	VipNDSubArrayIterator(const VipNDSubArrayIterator& other)
	  : shape(other.shape)
	  , strides(other.strides)
	  , cur_ptr(other.cur_ptr)
	  , end_ptr(other.end_ptr)
	  , data(other.data)
	  , end(other.end)
	  , data_size(other.data_size)
	{
	}

	VipNDSubArrayIterator(const VipNDArrayShape& sh, const VipNDArrayShape& st, void* ptr, qsizetype data_size, bool begin = true)
	  : data(ptr)
	  , end(ptr)
	  , data_size(data_size)
	{
		shape = sh;
		strides = st;
		cur_ptr.resize(strides.size());
		cur_ptr.fill(ptr);
		end_ptr.resize(strides.size());
		end_ptr.fill(ptr);

		if (!begin)
			// data point to the end of the array
			data = static_cast<char*>(ptr) + data_size * shape[0] * strides[0];
		else {
			end = static_cast<char*>(ptr) + data_size * shape.back() * strides.back();
			for (qsizetype i = 0; i < shape.size(); ++i)
				end_ptr[i] = static_cast<char*>(ptr) + data_size * shape[i] * strides[i];
		}
	}

	VipNDSubArrayIterator(void* end_ptr)
	  : data(end_ptr)
	  , end(end_ptr)
	  , data_size(0)
	{
	}

	// iterator like interface
	void* operator*() { return data; }
	const void* operator*() const { return data; }
	pointer operator->() { return data; }
	const_pointer operator->() const { return data; }

	bool operator!=(const VipNDSubArrayIterator& other) { return data != other.data; }
	bool operator==(const VipNDSubArrayIterator& other) { return data == other.data; }

	VipNDSubArrayIterator& operator++()
	{
		increment();
		return (*this);
	}

	VipNDSubArrayIterator operator++(int)
	{
		VipNDSubArrayIterator temp(*this);
		increment();
		return temp;
	}

	VipNDSubArrayIterator& operator=(const VipNDSubArrayIterator& other)
	{
		(shape) = other.shape;
		(strides) = other.strides;
		data = other.data;
		end = other.end;
		data_size = other.data_size;

		cur_ptr.resize(other.cur_ptr.size());
		end_ptr.resize(other.end_ptr.size());

		for (qsizetype i = 0; i < shape.size(); ++i) {
			cur_ptr[i] = other.cur_ptr[i];
			end_ptr[i] = other.end_ptr[i];
		}

		return *this;
	}
};

struct VipVoidIterator
{
	// data pointer
	void* data;
	// data size
	qsizetype data_size;

public:
	typedef std::forward_iterator_tag iterator_category;
	typedef void* value_type;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;
	typedef void* pointer;
	typedef const void* const_pointer;
	typedef void* reference;
	typedef const void* const_reference;

	VipVoidIterator(void* data = nullptr, qsizetype data_size = 0)
	  : data(data)
	  , data_size(data_size)
	{
	}

	VipVoidIterator(const VipVoidIterator& other)
	  : data(other.data)
	  , data_size(other.data_size)
	{
	}

	// iterator like interface
	reference operator*() { return data; }
	const_reference operator*() const { return data; }
	pointer operator->() { return data; }
	const_pointer operator->() const { return data; }

	bool operator!=(const VipVoidIterator& other) { return data != other.data; }
	bool operator==(const VipVoidIterator& other) { return data == other.data; }

	VipVoidIterator& operator++()
	{
		data = static_cast<char*>(data) + data_size;
		return (*this);
	}

	VipVoidIterator operator++(int)
	{
		VipVoidIterator temp(*this);
		data = static_cast<char*>(data) + data_size;
		return temp;
	}

	VipVoidIterator& operator=(const VipVoidIterator& other)
	{
		data = other.data;
		data_size = other.data_size;
		return *this;
	}
};

template<class TYPE>
class VipLineIterator
{
	// data pointer
	TYPE* data;
	qsizetype stride;

public:
	typedef std::bidirectional_iterator_tag iterator_category;
	typedef TYPE value_type;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;
	typedef TYPE* pointer;
	typedef const TYPE* const_pointer;
	typedef TYPE& reference;
	typedef const TYPE& const_reference;

	VipLineIterator(const TYPE* p = nullptr, qsizetype stride = 1)
	  : data(const_cast<TYPE*>(p))
	  , stride(stride)
	{
	}

	reference operator[](qsizetype pos) { return data[pos * this->stride]; }
	const_reference operator[](qsizetype pos) const { return data[pos * this->stride]; }

	reference operator*() { return *data; }
	const_reference operator*() const { return *data; }
	pointer operator->() { return data; }
	const_pointer operator->() const { return data; }

	bool operator!=(const VipLineIterator& other) const { return data != other.data; }
	bool operator==(const VipLineIterator& other) const { return data == other.data; }

	VipLineIterator& operator++()
	{
		data += this->stride;
		return (*this);
	}

	VipLineIterator operator++(int)
	{
		VipLineIterator temp(*this);
		data += this->stride;
		return temp;
	}

	VipLineIterator& operator--()
	{
		data -= this->stride;
		return (*this);
	}

	VipLineIterator operator--(int)
	{
		VipLineIterator temp = *this;
		data -= this->stride;
		return temp;
	}

	VipLineIterator operator+(qsizetype pos) const
	{
		VipLineIterator res = *this;
		res.data += pos * this->stride;
		return res;
	}

	VipLineIterator& operator+=(qsizetype pos)
	{
		data += pos * this->stride;
		return *this;
	}

	VipLineIterator operator-(qsizetype pos) const
	{
		VipLineIterator res = *this;
		res.data -= pos * this->stride;
		return res;
	}

	VipLineIterator& operator-=(qsizetype pos)
	{
		data -= pos * this->stride;
		return *this;
	}

	qsizetype operator-(const VipLineIterator& other) const { return (data - other.data) / this->stride; }
};

namespace detail
{
	template<typename InputIterator, typename OutputIterator, typename Transform>
	inline OutputIterator stdTransform(InputIterator begin, InputIterator end, OutputIterator dest, Transform tr)
	{
		for (; begin != end; ++dest, ++begin)
			*dest = tr(*begin);
		return dest;
	}

	template<typename InputIterator, typename Transform>
	inline void stdInplaceTransform(InputIterator begin, InputIterator end, Transform tr)
	{
		for (; begin != end; ++begin)
			*begin = tr(*begin);
	}

	template<typename InputIterator, typename OutputIterator, typename Transform>
	inline OutputIterator stdApplyBinary(InputIterator begin, InputIterator end, OutputIterator dest, Transform tr)
	{
		for (; begin != end; ++dest, ++begin)
			tr(*begin, *dest);
		return dest;
	}

	struct VoidToVoid
	{
		int inType;
		int outType;
		VoidToVoid(int i = 0, int o = 0)
		  : inType(i)
		  , outType(o)
		{
		}
		void operator()(const void* src, void* dst) { 
			#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			QMetaType::convert(src, inType, dst, outType); 
			#else
			QMetaType::convert(QMetaType(inType), src, QMetaType(outType), dst); 
			#endif
		}
	};

	template<class T, class U, class CONVERTER>
	struct Copy
	{
		static void apply(const T* start, const T* end, U* other, CONVERTER c) { detail::stdTransform(start, end, other, c); }
	};

	template<class T, class U>
	struct Copy<T, U, VipNullTransform>
	{
		static void apply(const T* start, const T* end, U* other, VipNullTransform) { std::copy(start, end, other); }
	};

	template<class T, class CONVERTER>
	struct CopyInplace
	{
		static void apply(T* start, T* end, CONVERTER c) { detail::stdInplaceTransform(start, end, c); }
	};

	template<class T>
	struct CopyInplace<T, VipNullTransform>
	{
		static void apply(T* start, T* end, VipNullTransform) {}
	};

	template<class T, class U>
	struct CopyInplace<T, VipFillTransform<U>>
	{
		static void apply(T* start, T* end, VipFillTransform<U> tr) { std::fill(start, end, tr.value); }
	};

} // end detail

/// Apply convert function inplace on given possibly strided N-D array
template<class INTYPE, class CONVERTER>
bool vipInplaceArrayTransform(INTYPE* in, const VipNDArrayShape& in_shape, const VipNDArrayShape& in_strides, CONVERTER c)
{
	bool in_unstrided;
	qsizetype size_in = vipShapeToSize(in_shape, in_strides, &in_unstrided);
	if (!size_in)
		return false;

	if (in_unstrided)
		detail::CopyInplace<INTYPE, CONVERTER>::apply(in, in + size_in, c);
	else if (in_shape.size() == 1) {
		const qsizetype s = in_shape[0];
		for (qsizetype i = 0; i < s; ++i)
			in[i * in_strides[0]] = c(in[i * in_strides[0]]);
	}
	else if (in_shape.size() == 2) {
		const qsizetype h = in_shape[0];
		const qsizetype w = in_shape[1];
		const qsizetype instride0 = in_strides[0];
		const qsizetype instride1 = in_strides[1];
		if (in_strides.last() == 1) {
			for (qsizetype y = 0; y < h; ++y)
				for (qsizetype x = 0; x < w; ++x)
					in[y * instride0 + x] = c(in[y * instride0 + x]);
		}
		else {
			for (qsizetype y = 0; y < h; ++y)
				for (qsizetype x = 0; x < w; ++x)
					in[y * instride0 + x * instride1] = c(in[y * instride0 + x * instride1]);
		}
	}
	else if (in_shape.size() == 3) {
		const qsizetype d = in_shape[0];
		const qsizetype h = in_shape[1];
		const qsizetype w = in_shape[2];
		const qsizetype instride0 = in_strides[0];
		const qsizetype instride1 = in_strides[1];
		const qsizetype instride2 = in_strides[2];
		if (in_strides.last() == 1) {
			for (qsizetype z = 0; z < d; ++z)
				for (qsizetype y = 0; y < h; ++y)
					for (qsizetype x = 0; x < w; ++x)
						in[z * instride0 + y * instride1 + x] = c(in[z * instride0 + y * instride1 + x]);
		}
		else {
			for (qsizetype z = 0; z < d; ++z)
				for (qsizetype y = 0; y < h; ++y)
					for (qsizetype x = 0; x < w; ++x)
						in[z * instride0 + y * instride1 + x * instride2] = c(in[z * instride0 + y * instride1 + x * instride2]);
		}
	}
	else
		detail::stdInplaceTransform(VipNDSubArrayIterator<INTYPE>(in_shape, in_strides, in, size_in), VipNDSubArrayIterator<INTYPE>(in_shape, in_strides, in, size_in, size_in), c);

	return true;
}

/// Apply a function to transform an input array pointer into an output one.
///  \param in input pointer
///  \param in_shape input array shape
///  \param in_strides input array strides
///  \param out output pointer
///  \param out_shape output array shape
///  \param out_strides output array strides
///  \param c function that converts a value of type INTYPE to a value of type OUTTYPE
template<class INTYPE, class OUTTYPE, class CONVERTER>
bool vipArrayTransform(const INTYPE* in,
		       const VipNDArrayShape& in_shape,
		       const VipNDArrayShape& in_strides,
		       OUTTYPE* out,
		       const VipNDArrayShape& out_shape,
		       const VipNDArrayShape& out_strides,
		       CONVERTER c)
{
	bool in_unstrided;
	bool other_unstrided;
	qsizetype size_in = vipShapeToSize(in_shape, in_strides, &in_unstrided);
	qsizetype size_out = vipShapeToSize(out_shape, out_strides, &other_unstrided);

	if (size_out != size_in || !size_in || !size_out)
		return false;

	if (in_unstrided && other_unstrided)
		detail::Copy<INTYPE, OUTTYPE, CONVERTER>::apply(in, in + size_out, out, c);
	else if (in_shape.size() == 1) {
		const qsizetype s = in_shape[0];
		for (qsizetype i = 0; i < s; ++i)
			out[i * out_strides[0]] = c(in[i * in_strides[0]]);
	}
	else if (in_shape.size() == 2 && in_shape == out_shape) {
		const qsizetype h = in_shape[0];
		const qsizetype w = in_shape[1];
		const qsizetype instride0 = in_strides[0];
		const qsizetype outstride0 = out_strides[0];
		const qsizetype instride1 = in_strides[1];
		const qsizetype outstride1 = out_strides[1];
		if (in_strides.last() == 1 && out_strides.last() == 1) {
			for (qsizetype y = 0; y < h; ++y)
				for (qsizetype x = 0; x < w; ++x)
					out[y * outstride0 + x] = c(in[y * instride0 + x]);
		}
		else {
			for (qsizetype y = 0; y < h; ++y)
				for (qsizetype x = 0; x < w; ++x)
					out[y * outstride0 + x * outstride1] = c(in[y * instride0 + x * instride1]);
		}
	}
	else if (in_shape.size() == 3 && in_shape == out_shape) {
		const qsizetype d = in_shape[0];
		const qsizetype h = in_shape[1];
		const qsizetype w = in_shape[2];
		const qsizetype instride0 = in_strides[0];
		const qsizetype outstride0 = out_strides[0];
		const qsizetype instride1 = in_strides[1];
		const qsizetype outstride1 = out_strides[1];
		const qsizetype instride2 = in_strides[2];
		const qsizetype outstride2 = out_strides[2];
		if (in_strides.last() == 1 && out_strides.last() == 1) {
			for (qsizetype z = 0; z < d; ++z)
				for (qsizetype y = 0; y < h; ++y)
					for (qsizetype x = 0; x < w; ++x)
						out[z * outstride0 + y * outstride1 + x] = c(in[z * instride0 + y * instride1 + x]);
		}
		else {
			for (qsizetype z = 0; z < d; ++z)
				for (qsizetype y = 0; y < h; ++y)
					for (qsizetype x = 0; x < w; ++x)
						out[z * outstride0 + y * outstride1 + x * outstride2] = c(in[z * instride0 + y * instride1 + x * instride2]);
		}
	}
	else if (in_unstrided)
		detail::stdTransform(in, in + size_out, VipNDSubArrayIterator<OUTTYPE>(out_shape, out_strides, out, size_out), c);
	else if (other_unstrided)
		detail::stdTransform(VipNDSubArrayConstIterator<INTYPE>(in_shape, in_strides, in, size_in), VipNDSubArrayConstIterator<INTYPE>(in_shape, in_strides, in, size_in, size_in), out, c);
	else
		detail::stdTransform(VipNDSubArrayConstIterator<INTYPE>(in_shape, in_strides, in, size_in),
				     VipNDSubArrayConstIterator<INTYPE>(in_shape, in_strides, in, size_in, size_in),
				     VipNDSubArrayIterator<OUTTYPE>(out_shape, out_strides, out, size_out),
				     c);

	return true;
}

/// Convert an input array pointer into an output one using the QMetaType converter functions.
///  \param in input pointer
///  \param in_type input type (qt meta type)
///  \param in_shape input array shape
///  \param in_strides input array strides
///  \param out output pointer
///  \param out_type output type (qt meta type)
///  \param out_shape output array shape
///  \param out_strides output array strides
inline bool vipArrayTransformVoid(const void* in,
				  int in_type,
				  const VipNDArrayShape& in_shape,
				  const VipNDArrayShape& in_strides,
				  void* out,
				  int out_type,
				  const VipNDArrayShape& out_shape,
				  const VipNDArrayShape& out_strides)
{
	bool in_unstrided;
	bool other_unstrided;
	qsizetype size_in = vipShapeToSize(in_shape, in_strides, &in_unstrided);
	qsizetype size_out = vipShapeToSize(out_shape, out_strides, &other_unstrided);

	if (size_out != size_in || !size_in || !size_out)
		return false;

	if (in_type == 0 || out_type == 0)
		return false;

	void* in_ptr = const_cast<void*>(in);
	qsizetype inSizeOf = QMetaType(in_type).sizeOf();
	qsizetype outSizeOf = QMetaType(out_type).sizeOf();

	if (in_unstrided && other_unstrided)
		detail::stdApplyBinary(VipVoidIterator(in_ptr, inSizeOf),
				       VipVoidIterator(static_cast<char*>(in_ptr) + size_out * inSizeOf, inSizeOf),
				       VipVoidIterator(out, outSizeOf),
				       detail::VoidToVoid(in_type, out_type));
	else if (in_unstrided)
		detail::stdApplyBinary(VipVoidIterator(in_ptr, inSizeOf),
				       VipVoidIterator(static_cast<char*>(in_ptr) + size_out * inSizeOf, inSizeOf),
				       VipNDSubArrayIterator<void>(out_shape, out_strides, out, outSizeOf),
				       detail::VoidToVoid(in_type, out_type));
	else if (other_unstrided)
		detail::stdApplyBinary(VipNDSubArrayIterator<void>(in_shape, in_strides, in_ptr, inSizeOf),
				       VipNDSubArrayIterator<void>(in_shape, in_strides, in_ptr, inSizeOf, false),
				       VipVoidIterator(out, outSizeOf),
				       detail::VoidToVoid(in_type, out_type));
	else
		detail::stdApplyBinary(VipNDSubArrayIterator<void>(in_shape, in_strides, in_ptr, inSizeOf),
				       VipNDSubArrayIterator<void>(in_shape, in_strides, in_ptr, inSizeOf, false),
				       VipNDSubArrayIterator<void>(out_shape, out_strides, out, outSizeOf),
				       detail::VoidToVoid(in_type, out_type));

	return true;
}

namespace iter_detail
{
	template<qsizetype Incr, Vip::Ordering order, class Position, class Shape, class StartShape = void>
	VIP_ALWAYS_INLINE void incrementPos(Position& pos, const Shape& sh, qsizetype dimCount, const StartShape* start = nullptr)
	{
		detail::IncrementCoord<Position::static_size, order, Incr>::apply(pos, sh, dimCount, start);
	}

	template<Vip::Ordering order, class Position, class Shape, class StartShape = void>
	VIP_ALWAYS_INLINE bool incrementCheckContinue(Position& pos, const Shape& sh, qsizetype dimCount, const StartShape* start = nullptr)
	{
		return detail::IncrementCoordCheckContinue<Position::static_size, order>::apply(pos, sh, dimCount, start);
	}

	template<qsizetype N>
	VIP_ALWAYS_INLINE VipCoordinate<N> initStart(const VipCoordinate<N>&)
	{
		VipCoordinate<N> start;
		start.fill(0);
		return start;
	}
	template<>
	VIP_ALWAYS_INLINE VipCoordinate<Vip::None> initStart(const VipCoordinate<Vip::None>& sh)
	{
		VipCoordinate<Vip::None> start;
		start.resize(sh.size());
		start.fill(0);
		return start;
	}
}

/// Iterate over all dimensions of \a shape in given \a ordering.
/// The iteration position is stored in \a coord as a #VipHybridVector.
/// Note that \a coord has the same static size as \a shape. For best performances,
/// you should always provide a shape having a static size.
/// Use one of the #vipVector overloads to create a static sized vector.
///
/// Example:
/// \code
/// VipNDArrayType<int> ar(vipVector(2,2));
/// //fill with 0
/// vip_iter(Vip::FirstMajor, vipVector<2>(ar.shape()), coord)
/// ar(coord) = 0;
/// \endcode
#define vip_iter(ordering, shape, coord)                                                                                                                                                               \
	/* Create the coord vector used in the iteration process */                                                                                                                                    \
	if (VipCoordinate<__NDIM(shape)> coord = iter_detail::initStart<__NDIM(shape)>(shape))                                                                                                  \
		if (VipCoordinate<__NDIM(shape)> sh = shape)                                                                                                                                    \
			/* Remove 1 to the inner shape since the for loop add 1 at the beginning */                                                                                                    \
			if ((ordering == Vip::FirstMajor ? coord.back() = Vip::None : coord[0] = Vip::None) | 1)                                                                                       \
				if (qsizetype dimCount = shape.size())                                                                                                                                       \
					/* Go! */                                                                                                                                                      \
					while (iter_detail::incrementCheckContinue<ordering>(coord, sh, dimCount))

/// Iterate over all dimensions of \a shape in given \a ordering in parallel using openmp.
/// The iteration position is stored in \a coord as a #VipHybridVector.
/// Note that \a coord has the same static size as \a shape. For best performances,
/// you should always provide a shape having a static size.
/// Use one of the #vipVector overloads to create a static sized vector.
///
/// \sa vip_iter
#define vip_iter_parallel(ordering, shape, coord)                                                                                                                                                      \
	/*Create the iteration shape (end bounds) */                                                                                                                                                   \
	if (VipCoordinate<__NDIM(shape)> sh = shape)                                                                                                                                            \
		/* Get the full iteration count */                                                                                                                                                     \
		if (qsizetype _size = vipCumMultiply(sh))                                                                                                                                                    \
			/* Get the number of thread, or 1 if the iteration count < thread_count*/                                                                                                      \
			if (qsizetype thread_count = _size >= (qsizetype)vipOmpThreadCount() ? (qsizetype)vipOmpThreadCount() : _size)                                                                                   \
				/* Get number of chunk per thread*/                                                                                                                                    \
				if (qsizetype chunk_size = _size / thread_count)                                                                                                                             \
					/* Parallel directive */                                                                                                                                       \
					VIP_PRAGMA(omp parallel for)                                                                                                                                   \
	for (qsizetype i = 0; i < thread_count; ++i)                                                                                                                                                         \
		/* End iteration for this thread, and take care of the remaining values*/                                                                                                              \
		if (qsizetype end_iter = (i == thread_count - 1 ? (chunk_size + _size - chunk_size * thread_count) : chunk_size))                                                                            \
			/* Create the coord vector used in the iteration process */                                                                                                                    \
			if (VipCoordinate<__NDIM(shape)> coord = iter_detail::initStart<__NDIM(shape)>(sh))                                                                                     \
				/* Advance coord */                                                                                                                                                    \
				if (iter_detail::setFlatPos<ordering>(coord, sh, i * chunk_size))                                                                                                      \
					if (qsizetype dimCount = sh.size())                                                                                                                                  \
						/* Go! */                                                                                                                                              \
						for (qsizetype j = 0; j < end_iter; ++j, iter_detail::incrementPos<1, ordering>(coord, sh, dimCount))

/// Iterate over all dimensions of the range [start,end) in given \a ordering.
/// The iteration position is stored in \a coord as a #VipHybridVector.
/// Note that \a coord has the same static size as \a start or \a end (only one of them needs to be statis).
/// For best performances, you should always provide a start or end vector having a static size.
/// Use one of the #vipVector overloads to create a static sized vector.
///
/// Example:
/// \code
/// VipNDArrayType<qsizetype> ar(vipVector(1000,1000));
/// ar.fill(1)
///
/// //only let the border to 1, fill the inside with 0
/// vip_iter_range(Vip::FirstMajor, vipVector(1,1),vipVector(999,999), coord)
/// ar(coord) = 0;
/// \endcode
#define vip_iter_range(ordering, start, end, coord)                                                                                                                                                    \
	/* Create the coord vector used in the iteration process */                                                                                                                                    \
	if (VipCoordinate<__NDIM2(start, end)> coord = start /*iter_detail::initStartCopy(start)*/)                                                                                             \
		if (VipCoordinate<__NDIM2(start, end)> sh = end)                                                                                                                                \
			if (VipCoordinate<__NDIM2(start, end)> st = start)                                                                                                                      \
				/* Remove 1 to the inner shape since the for loop add 1 at the beginning */                                                                                            \
				if ((ordering == Vip::FirstMajor ? coord.back() = st.back() - 1 : coord[0] = st[0] - 1) | 1)                                                                           \
					if (qsizetype dimCount = shape.size())                                                                                                                               \
						/* Go! And pass the start parameter to incrementCheckContinue */                                                                                       \
						while (iter_detail::incrementCheckContinue<ordering>(coord, sh, dimCount, &st))

/// Iterate over all dimensions of the range [start,end) in given \a ordering in parallel using openmp.
/// The iteration position is stored in \a coord as a #VipHybridVector.
/// Note that \a coord has the same static size as \a start or \a end (only one of them needs to be statis).
/// For best performances, you should always provide a start or end vector having a static size.
/// Use one of the #vipVector overloads to create a static sized vector.
///
/// \sa vip_iter_range
#define vip_iter_range_parallel(ordering, start, end, coord)                                                                                                                                           \
	/*Create the iteration shape (end bounds) */                                                                                                                                                   \
	if (VipCoordinate<__NDIM2(start, end)> sh = end)                                                                                                                                        \
		if (VipCoordinate<__NDIM2(start, end)> st = start)                                                                                                                              \
			/*Get the total iteration count */                                                                                                                                             \
			if (qsizetype size = vipCumMultiplyRect(st, sh))                                                                                                                                     \
				if (qsizetype thread_count = size > (qsizetype)vipOmpThreadCount() ? (qsizetype)vipOmpThreadCount() : size)                                                                              \
					if (qsizetype chunk_size = size / thread_count)                                                                                                                      \
						VIP_PRAGMA(omp parallel for)                                                                                                                           \
	for (qsizetype i = 0; i < thread_count; ++i)                                                                                                                                                         \
		if (qsizetype end_iter = (i == thread_count - 1 ? (chunk_size + size - chunk_size * thread_count) : chunk_size))                                                                             \
			if (VipCoordinate<__NDIM2(start, end)> coord = st)                                                                                                                      \
				if (setFlatPos<ordering>(coord, st, sh, i * chunk_size))                                                                                                               \
					if (qsizetype dimCount = sh.size())                                                                                                                                  \
						for (qsizetype j = 0; j < end_iter; ++j, iter_detail::incrementPos<1, ordering>(coord, sh, dimCount, &st))

/// Equivalent to #vip_iter(Vip::FirstMajor,shape,coord)
#define vip_iter_fmajor(shape, coord) vip_iter(Vip::FirstMajor, shape, coord)
/// Equivalent to #vip_iter(Vip::LastMajor,shape,coord)
#define vip_iter_lmajor(shape, coord) vip_iter(Vip::LastMajor, shape, coord)

/// Equivalent to #vip_iter_range(Vip::FirstMajor,start, end, coord)
#define vip_iter_range_fmajor(start, end, coord) vip_iter_range(Vip::FirstMajor, start, end, coord)
/// Equivalent to #vip_iter_range(Vip::LastMajor,start, end, coord)
#define vip_iter_range_lmajor(start, end, coord) vip_iter_range(Vip::LastMajor, start, end, coord)

/// Equivalent to #vip_iter_parallel(Vip::FirstMajor,shape,coord)
#define vip_iter_parallel_fmajor(shape, coord) vip_iter_parallel(Vip::FirstMajor, shape, coord)
/// Equivalent to #vip_iter_parallel(Vip::LastMajor,shape,coord)
#define vip_iter_parallel_lmajor(shape, coord) vip_iter_parallel(Vip::LastMajor, shape, coord)

/// Equivalent to #vip_iter_range_parallel(Vip::FirstMajor,start, end, coord)
#define vip_iter_range_parallel_fmajor(start, end, coord) vip_iter_range_parallel(Vip::FirstMajor, start, end, coord)
/// Equivalent to #vip_iter_range_parallel(Vip::LastMajor,start, end, coord)
#define vip_iter_range_parallel_lmajor(start, end, coord) vip_iter_range_parallel(Vip::LastMajor, start, end, coord)

/// @}
// end DataType

#endif
