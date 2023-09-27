#pragma once

#include "VipNDArrayOperations.h"

namespace Vip
{
	/// Reverse array method, used with #vipReverse
	enum ReverseArray
	{
		ReverseFlat, //! reverse the array considering flat indices
		ReverseAxis //! reverse rows/columns (if 2D) for specified axis
	};
}

namespace detail
{
	template<class Array, bool hasNullType = HasNullType<Array>::value>
	struct Transpose : BaseOperator1<typename DeduceArrayType<Array>::value_type, Array>
	{
		typedef BaseOperator1<typename DeduceArrayType<Array>::value_type, Array> base_type;
		typedef typename base_type::value_type value_type;

		static const int access_type = Vip::Position;
		VipNDArrayShape sh;
		Transpose() {}
		Transpose(const Array & op1, const VipNDArrayShape & sh)
			: base_type(op1), sh(sh) {}

		const VipNDArrayShape & shape() const { return sh; }
		template< class Coord>
		VIP_ALWAYS_INLINE value_type operator()(const Coord & pos) const{
			Coord in;
			vipReverse(pos, in);
			return this->array1(in);
		}
	};

	template<class Array>
	struct Transpose<Array, true> : BaseOperator1<NullType, Array>
	{

		VipNDArrayShape sh;
		Transpose() {}
		Transpose(const Array & op1, const VipNDArrayShape & sh)
			: BaseOperator1<NullType, Array>(op1), sh(sh) {}
		const VipNDArrayShape & shape() const { return sh; }
	};

	template<class T, class Array, bool hasNullType>
	struct rebind<T, Transpose<Array, hasNullType>, false> {

		typedef Transpose<Array, hasNullType> op;
		typedef Transpose<typename rebind<T, typename op::array_type1>::type, false> type;
		static type cast(const op & a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), a.sh); }
	};

	template< class Array>
	struct is_valid_functor<Transpose<Array, false> > : std::true_type {};


	template<Vip::ReverseArray Rev>
	struct RevPos
	{
		template< class Coord, class Shape>
		static void apply(const Coord & pos, Coord & rev, const Shape & sh, int axis){
			rev = pos;
			rev[axis] = sh[axis] - pos[axis] - 1;
		}
	};
	template<>
	struct RevPos<Vip::ReverseFlat>
	{
		template< class Coord, class Shape>
		static void apply(const Coord & pos, Coord & rev, const Shape & sh, int ){
			rev.resize(pos.size());
			for (int i = 0; i < pos.size(); ++i)
				rev[i] = (sh)[i] - pos[i] - 1;
		}
	};


	template<Vip::ReverseArray Rev, class Array, bool hasNullType = HasNullType<Array>::value>
	struct Reverse : BaseOperator1<typename DeduceArrayType<Array>::value_type, Array>
	{
		static const int access_type = Vip::Position | (Rev == Vip::ReverseFlat ? Vip::Flat : 0);
		typedef BaseOperator1<typename DeduceArrayType<Array>::value_type, Array> base_type;
		typedef typename base_type::value_type value_type;

		int size;
		int axis;
		const VipNDArrayShape * sh;
		Reverse() {}
		Reverse(const Array & op1, int size, int axis)
			: base_type(op1), size(size), axis(axis), sh(&this->array1.shape()) {}

		template< class Coord>
		VIP_ALWAYS_INLINE value_type operator()(const Coord & pos) const {
			Coord p;
			RevPos<Rev>::apply(pos, p, *sh, axis);
			return this->array1(p);

		}
		VIP_ALWAYS_INLINE value_type operator[](int index) const {
			return this->array1[size - index - 1];
		}
	};

	template<Vip::ReverseArray Rev, class Array>
	struct Reverse<Rev,Array, true> : BaseOperator1<NullType, Array>
	{
		int size;
		int axis;
		Reverse() {}
		Reverse(const Array & op1, int size, int axis)
			: BaseOperator1<NullType, Array>(op1), size(size), axis(axis) {}
	};

	template<class T, Vip::ReverseArray Rev, class Array, bool hasNullType>
	struct rebind<T, Reverse<Rev,Array, hasNullType>, false> {

		typedef Reverse<Rev,Array, hasNullType> op;
		typedef Reverse<Rev,typename rebind<T, typename op::array_type1>::type, false> type;
		static type cast(const op & a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), a.size, a.axis); }
	};

	template< Vip::ReverseArray Rev, class Array>
	struct is_valid_functor<Reverse<Rev,Array, false> > : std::true_type {};
}


/// \addtogroup DataType
/// @{

/// Returns a functor expression to transpose input N-D array.
template<class Array>
detail::Transpose<Array> vipTranspose(const Array & array)
{
	VipNDArrayShape sh;
	vipReverse(array.shape(), sh);
	return detail::Transpose<Array>(array, sh);
}

/// Returns a functor expression to reverse input N-D array.
/// If \a Rev is Vip::ReverseFlat, this will swap the elements (0,N), (1, N-1), ... considering that the array is flat.
/// If \a Rev is Vip::ReverseAxis, this will swap full rows/columns for given axis.
/// To swap rows on a 2D array, use Vip::ReverseAxis with axis=0.
template<Vip::ReverseArray Rev,class Array>
detail::Reverse<Rev,Array> vipReverse(const Array & array, int axis = 0)
{
	return detail::Reverse<Rev, Array>(array, vipCumMultiply(array.shape()), axis);
}


/// @}
//end DataType
