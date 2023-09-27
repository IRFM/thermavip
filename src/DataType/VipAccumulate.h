#pragma once

#include "VipReduction.h"

namespace detail
{
	template<class T, class Func>
	struct FunctorAccum : Reductor<T>
	{
		typedef typename Reductor<T>::value_type value_type;
		Func functor;
		typename Reductor<T>::value_type value;
		FunctorAccum(const Func & fun, const T & start)
			:functor(fun), value(start) {}
		void setAt(int, const value_type & v) {value = functor(value, v);}
		template<class ShapeType> void setPos(const ShapeType &, const value_type & v) {value = functor(value, v);}
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
/// int cum_sum = vipAccumulate(ar, [](int a, int b) {return a + b; }, 0);
/// \endcode
///
/// Note that the start value type determines the return type of vipAccumulate.
/// If \a ok is not NULL, it is set to true or false depending on the accumation result.
template<class Array, class Fun, class T>
T vipAccumulate(const Array & ar, const Fun & functor, const T & start, bool * ok = NULL)
{
	detail::FunctorAccum<T, Fun> f(functor, start);
	bool r = vipReduce(f, ar);
	if (ok) *ok = r;
	return f.value;
}


/// Accumulate array values using binary functor \a fun and start value \a start.
/// The accumulation is only performed on coordinates where \a roi returns true.
/// Example:
/// \code
/// VipNDArrayType<int> ar(vipVector(3, 3));
/// for (int i = 0; i < ar.size(); ++i)
/// ar[i] = i;
/// int cum_sum = vipAccumulate(ar, [](int a, int b) {return a + b; }, 0);
/// \endcode
///
/// Note that the start value type determines the return type of vipAccumulate.
/// If \a ok is not NULL, it is set to true or false depending on the accumation result.
template<class Array, class Fun, class T, class OverRoi>
T vipAccumulate(const Array & ar, const Fun & functor, const T & start_value, const OverRoi & roi, bool * ok = NULL)
{
	detail::FunctorAccum<T, Fun> f(functor, start_value);
	bool r = vipReduce(f, ar, roi);
	if (ok) *ok = r;
	return f.value;
}


/// @}
//end DataType
