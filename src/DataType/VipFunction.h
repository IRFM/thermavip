#pragma once

#include <functional>
#include "VipNDArrayOperations.h"


namespace detail
{

	template<class Functor, class A1, bool hasNullArray = HasNullType<A1>::value>
	struct Function1 : BaseOperator1<typename std::result_of<Functor(typename DeduceArrayType<A1>::value_type)>::type, A1>
	{
		_ENSURE_OPERATOR1_DEF(BaseOperator1<typename std::result_of<Functor(typename DeduceArrayType<A1>::value_type)>::type, A1>)
		Functor functor;
		Function1(){}
		Function1(const A1 & op1, const Functor & f) :base(op1), functor(f) {}
		template<class Coord>
		value_type operator()(const Coord & pos) const { return functor(this->array1(pos)); }
		value_type operator[](int index) const { return functor(this->array1[index]); }
	};

	template<class Functor, class A1>
	struct Function1<Functor,A1,true> : BaseOperator1<NullType, A1>
	{
		Functor functor;
		Function1() {}
		Function1(const A1 & op1, const Functor & f) : BaseOperator1<NullType, A1>(op1), functor(f) {}
	};

	template<class T, class Functor, class A1, bool hasNullType>
	struct rebind<T, Function1<Functor, A1, hasNullType>, false> {

		typedef Function1<Functor, A1, hasNullType> op;
		typedef Function1<Functor, typename rebind<T, typename op::array_type1>::type, false> type;
		static type cast(const op & a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), a.functor); }
	};

	template<class Fun,class A1>
	using try_Function1 = decltype(std::declval<Fun&>()(DeduceArrayType<A1>::value_type()));
	template<class Fun, class A1>
	struct is_valid_functor<Function1<Fun,A1,false> > : is_valid_op2<Fun,A1, try_Function1>{static void apply(){}};




	template<class Functor, class A1, class A2, bool hasNullArray = HasNullType<A1,A2>::value>
	struct Function2 : BaseOperator2<typename std::result_of<Functor(typename DeduceArrayType<A1>::value_type, typename DeduceArrayType<A2>::value_type)>::type, A1, A2>
	{
		_ENSURE_OPERATOR2_DEF(BaseOperator2<typename std::result_of<Functor(typename DeduceArrayType<A1>::value_type, typename DeduceArrayType<A2>::value_type)>::type, A1, A2>)

		Functor functor;
		Function2() {}
		Function2(const A1 & op1, const A2 & op2, const Functor & f) :base(op1, op2), functor(f) {}
		template<class Coord>
		value_type operator()(const Coord & pos) const { return functor(this->array1(pos),this->array2(pos)); }
		value_type operator[](int index) const { return functor(this->array1[index],this->array2[index]); }
	};

	template<class Functor, class A1, class A2>
	struct Function2<Functor, A1,A2, true> : BaseOperator2<NullType, A1, A2>
	{
		Functor functor;
		Function2() {}
		Function2(const A1 & op1, const A2 & op2, const Functor & f) :BaseOperator2<NullType, A1, A2>(op1,op2), functor(f) {}
	};

	template<class T, class Functor, class A1, class A2, bool hasNullType>
	struct rebind<T, Function2<Functor, A1, A2, hasNullType>, false> {

		typedef Function2<Functor,A1, A2, hasNullType> op;
		typedef Function2<Functor, typename rebind<T, typename op::array_type1>::type, typename rebind<T, typename op::array_type2>::type, false> type;
		static type cast(const op & a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), rebind<T, typename op::array_type2>::cast(a.array2), a.functor); }
	};

	template<class Fun, class A1, class A2>
	using try_Function2 = decltype(std::declval<Fun&>()(DeduceArrayType<A1>::value_type(), DeduceArrayType<A2>::value_type()));
	template<class Fun, class A1, class A2>
	struct is_valid_functor<Function2<Fun, A1, A2, false> > : is_valid_op3<Fun,A1, A2, try_Function2> { static void apply() {} };




	template<class Functor, class A1, class A2, class A3, bool hasNullArray = HasNullType<A1, A2, A3>::value>
	struct Function3 : BaseOperator3<typename std::result_of<Functor(typename DeduceArrayType<A1>::value_type, typename DeduceArrayType<A2>::value_type, typename DeduceArrayType<A3>::value_type)>::type, A1, A2, A3>
	{
		_ENSURE_OPERATOR3_DEF(BaseOperator3<typename std::result_of<Functor(typename DeduceArrayType<A1>::value_type, typename DeduceArrayType<A2>::value_type, typename DeduceArrayType<A3>::value_type)>::type, A1, A2, A3>)

		Functor functor;
		Function3() {}
		Function3(const A1 & op1, const A2 & op2, const A3 & op3, const Functor & f) :base(op1, op2,op3), functor(f) {}
		template<class Coord>
		value_type operator()(const Coord & pos) const { return functor(this->array1(pos), this->array2(pos), this->array3(pos)); }
		value_type operator[](int index) const { return functor(this->array1[index], this->array2[index], this->array3[index]); }
	};

	template<class Functor, class A1, class A2, class A3>
	struct Function3<Functor, A1, A2, A3, true> : BaseOperator3<NullType, A1, A2, A3>
	{
		Functor functor;
		Function3() {}
		Function3(const A1 & op1, const A2 & op2, const A3 & op3, const Functor & f) :BaseOperator3<NullType, A1, A2, A3>(op1, op2,op3), functor(f) {}
	};

	template<class T, class Functor, class A1, class A2, class A3, bool hasNullType>
	struct rebind<T, Function3<Functor, A1, A2, A3, hasNullType>, false> {

		typedef Function3<Functor, A1, A2, A3, hasNullType> op;
		typedef Function3<Functor, typename rebind<T, typename op::array_type1>::type, typename rebind<T, typename op::array_type2>::type, typename rebind<T, typename op::array_type3>::type, false> type;
		static type cast(const op & a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), rebind<T, typename op::array_type2>::cast(a.array2), rebind<T, typename op::array_type3>::cast(a.array3), a.functor); }
	};

	template<class Fun, class A1, class A2, class A3>
	using try_Function3 = decltype(std::declval<Fun&>()(DeduceArrayType<A1>::value_type(), DeduceArrayType<A2>::value_type(), DeduceArrayType<A3>::value_type()));
	template<class Fun, class A1, class A2, class A3>
	struct is_valid_functor<Function3<Fun, A1, A2, A3, false> > : is_valid_op4<Fun, A1, A2,A3, try_Function3> { static void apply() {} };

}

/// \addtogroup DataType
/// @{

/// Returns a functor expression that calls unary functor \a fun on the elements of N-D array \a array.
/// Example:
///
/// \code
/// VipNDArrayType<int> ar(vipVector(3,3));
/// //...
/// //multiply by 2
/// ar = vipFunction(ar,[](int a){return a*2;});
/// \endcode
template<class A1, class Functor>
detail::Function1<Functor, A1> vipFunction(const A1 & array, const Functor & fun)
{
	return detail::Function1<Functor, A1>(array, fun);
}

/// Returns a functor expression that calls binary functor \a fun on the elements of N-D arrays \a a1 and \a a2.
/// Example:
///
/// \code
/// VipNDArrayType<int> ar(vipVector(3,3)), ar2(vipVector(3,3));
/// //...
/// //multiply ar by ar2
/// ar = vipFunction(ar,ar2,[](int a, int b){return a*b;});
/// \endcode
template<class A1, class A2, class Functor>
detail::Function2<Functor, A1, A2> vipFunction(const A1 & a1, const A2 & a2, const Functor & fun)
{
	return detail::Function2<Functor, A1,A2>(a1,a2, fun);
}

/// Returns a functor expression that calls ternary functor \a fun on the elements of N-D arrays \a a1, \a a2 and \a a3.
/// Example:
///
/// \code
/// VipNDArrayType<int> ar(vipVector(3,3)), ar2(vipVector(3,3)), ar3(vipVector(3,3));
/// //...
/// //apply ar*ar2*ar3
/// ar = vipFunction(ar,ar2,ar3,[](int a, int b, int c){return a*b*c;});
/// \endcode
template<class A1, class A2, class A3, class Functor>
detail::Function3<Functor, A1, A2,A3> vipFunction(const A1 & a1, const A2 & a2, const A3 & a3, const Functor & fun)
{
	return detail::Function3<Functor, A1, A2, A3>(a1, a2, a3, fun);
}


/// @}
//end DataType
