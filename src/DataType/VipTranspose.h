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

#ifndef VIP_TRANSPOSE_H
#define VIP_TRANSPOSE_H

#include "VipNDArrayOperations.h"

namespace Vip
{
	/// Reverse array method, used with #vipReverse
	enum ReverseArray
	{
		ReverseFlat, //! reverse the array considering flat indices
		ReverseAxis  //! reverse rows/columns (if 2D) for specified axis
	};
}

namespace detail
{
	template<class Array>
	struct Transpose : BaseFunctor<Transpose<Array>, Array>
	{
		using base = BaseFunctor<Transpose<Array>, Array>;
		static const qsizetype access_type = Vip::Position;

		Transpose() {}
		Transpose(const Array& op1, const VipNDArrayShape& new_sh)
		  : base(op1)
		{
			this->sh = new_sh;
		}

		template<class Coord>
		VIP_ALWAYS_INLINE auto operator()(const Coord& pos) const
		{
			return std::get<0>(this->arrays)(vipReverse(pos));
		}
	};

	template<class U, class Array>
	struct RebindExpression<U, Transpose<Array>>
	{
		static auto cast(const Transpose<Array>& tr)
		{
			using type = Transpose<RebindType_t<U,Array> >;
			return type(rebindExpression<U>(std::get<0>(tr.arrays)), tr.shape());
		}
	};

	

	template<Vip::ReverseArray Rev>
	struct RevPos
	{
		template<class Coord, class Shape>
		static void apply(const Coord& pos, Coord& rev, const Shape& sh, qsizetype axis)
		{
			rev = pos;
			rev[axis] = sh[axis] - pos[axis] - 1;
		}
	};
	template<>
	struct RevPos<Vip::ReverseFlat>
	{
		template<class Coord, class Shape>
		static void apply(const Coord& pos, Coord& rev, const Shape& sh, qsizetype)
		{
			static constexpr auto static_size = Coord::static_size > Shape::static_size ? Coord::static_size : Shape::static_size;
			if constexpr (static_size == 1) {
				rev[0] = (sh)[0] - pos[0] - 1;
			}
			else if constexpr (static_size == 2) {
				rev[0] = (sh)[0] - pos[0] - 1;
				rev[1] = (sh)[1] - pos[1] - 1;
			}
			else if constexpr (static_size == 3) {
				rev[0] = (sh)[0] - pos[0] - 1;
				rev[1] = (sh)[1] - pos[1] - 1;
				rev[2] = (sh)[2] - pos[2] - 1;
			}
			else {
				rev.resize(pos.size());
				for (qsizetype i = 0; i < pos.size(); ++i)
					rev[i] = (sh)[i] - pos[i] - 1;
			}
		}
	};

	template<Vip::ReverseArray Rev, class Array>
	struct Reverse : BaseFunctor<Reverse<Rev, Array>, Array>
	{
		static const qsizetype access_type = Vip::Position;
		using base = BaseFunctor<Reverse<Rev, Array>, Array>;

		qsizetype size = 0;
		qsizetype axis = 0;
		Reverse() {}
		Reverse(const Array& op1, qsizetype size, qsizetype axis)
		  : base(op1)
		  , size(size)
		  , axis(axis)
		{
		}

		template<class Coord>
		auto operator()(const Coord& pos) const
		{
			Coord p;
			RevPos<Rev>::apply(pos, p, this->shape(), axis);
			return std::get<0>(this->arrays)(p);
		}
	};

	template<class U, Vip::ReverseArray Rev, class Array>
	struct RebindExpression<U, Reverse<Rev, Array>>
	{
		static auto cast(const Reverse<Rev, Array>& r)
		{
			using type = Reverse<Rev,RebindType_t<U,Array> >;
			return type(rebindExpression<U>(std::get<0>(r.arrays)), r.size, r.axis);
		}
	};

}

/// \addtogroup DataType
/// @{

/// Returns a functor expression to transpose input N-D array.
template<class Array>
auto vipTranspose(const Array& array)
{
	return detail::Transpose<detail::DeduceArrayType_t< Array>>(array, vipReverse(array.shape()));
}

/// Returns a functor expression to reverse input N-D array.
/// If \a Rev is Vip::ReverseFlat, this will swap the elements (0,N), (1, N-1), ... considering that the array is flat.
/// If \a Rev is Vip::ReverseAxis, this will swap full rows/columns for given axis.
/// To swap rows on a 2D array, use Vip::ReverseAxis with axis=0.
template<Vip::ReverseArray Rev, class Array>
auto vipReverse(const Array& array, qsizetype axis = 0)
{
	return detail::Reverse<Rev, detail::DeduceArrayType_t<Array>>(array, vipCumMultiply(array.shape()), axis);
}

/// @}
// end DataType

#endif