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

#ifndef VIP_ACCUMULATE_H
#define VIP_ACCUMULATE_H

#include "VipReduction.h"

namespace detail
{
	template<class T, class Func>
	struct FunctorAccum : Reductor<T>
	{
		typedef typename Reductor<T>::value_type value_type;
		Func functor;
		typename Reductor<T>::value_type value;
		FunctorAccum(const Func& fun, const T& start)
		  : functor(fun)
		  , value(start)
		{
		}
		void setAt(qsizetype, const value_type& v) { value = functor(value, v); }
		template<class ShapeType>
		void setPos(const ShapeType&, const value_type& v)
		{
			value = functor(value, v);
		}
	};
}

/// \addtogroup DataType
/// @{

/// Accumulate array values using binary functor \a fun and start value \a start.
/// Example:
/// \code
/// VipNDArrayType<int> ar(vipVector(3, 3));
/// for (int i = 0; i < ar.size(); ++i)
/// ar[i] = i;
/// int cum_sum = vipAccumulate(ar, [](qsizetype a, qsizetype b) {return a + b; }, 0);
/// \endcode
///
/// Note that the start value type determines the return type of vipAccumulate.
/// If \a ok is not nullptr, it is set to true or false depending on the accumation result.
template<class Array, class Fun, class T>
T vipAccumulate(const Array& ar, const Fun& functor, const T& start, bool* ok = nullptr)
{
	detail::FunctorAccum<T, Fun> f(functor, start);
	bool r = vipReduce(f, ar);
	if (ok)
		*ok = r;
	return f.value;
}

/// Accumulate array values using binary functor \a fun and start value \a start.
/// The accumulation is only performed on coordinates where \a roi returns true.
/// Example:
/// \code
/// VipNDArrayType<int> ar(vipVector(3, 3));
/// for (int i = 0; i < ar.size(); ++i)
/// ar[i] = i;
/// int cum_sum = vipAccumulate(ar, [](qsizetype a, qsizetype b) {return a + b; }, 0);
/// \endcode
///
/// Note that the start value type determines the return type of vipAccumulate.
/// If \a ok is not nullptr, it is set to true or false depending on the accumation result.
template<class Array, class Fun, class T, class OverRoi>
T vipAccumulate(const Array& ar, const Fun& functor, const T& start_value, const OverRoi& roi, bool* ok = nullptr)
{
	detail::FunctorAccum<T, Fun> f(functor, start_value);
	bool r = vipReduce(f, ar, roi);
	if (ok)
		*ok = r;
	return f.value;
}

/// @}
// end DataType

#endif