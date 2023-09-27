#ifndef VIP_CONVOLVE_H
#define VIP_CONVOLVE_H

#include "VipNDArrayOperations.h"
#include "VipNDRect.h"
/// \addtogroup DataType
/// @{

namespace Vip
{
	/// Convolution border treatment
	enum ConvolveBorderRule
	{
		Avoid, //! Directly returns the source value without convolution
		Nearest, //! Uses the nearest valid value to perform the convolution
		Wrap //! Wrap around coordinates
	};
}
/// @}
//end DataType

namespace detail
{

	template <Vip::ConvolveBorderRule Rule>
	struct ArrayConvolve
	{
		template< class Array, class Kernel, class Type, class Coord1, class Coord2, class Coord3, class Coord4>
		static bool apply(int Dim, int NbDim,
								const Array & array,
								const Kernel & kernel,
								const Coord1 & current,
								const Coord2 & k_center,
								const Coord3 & arshape,
								const Coord3 & kshape,
								Coord4 & c_k,
								Coord4 & c_a,
								Type & res
								)
		{
			if(Dim == 0){
				int last = NbDim-1;
				for(int i = 0; i < kshape[last]; ++i){
					//update coordinates
					c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
					res += kernel(c_k) * array(c_a);
				}
				return true;
			}

			int last = NbDim-Dim-1;
			//loop on kernel coordinates
			for(int i = 0; i < kshape[last]; ++i) {
				//update coordinates
				c_a[last] = current[last] +  (c_k[last] = i) - (int)k_center[last];
				ArrayConvolve<Rule>::apply(Dim - 1, NbDim, array, kernel, current, k_center, arshape, kshape, c_k, c_a, res);
			}

			return true;
		}
	};


	template <>
	struct ArrayConvolve<Vip::Avoid>
	{
		template< class Array, class Kernel, class Type, class Coord1, class Coord2, class Coord3, class Coord4>
		static bool apply(int Dim, int NbDim,
			const Array & array,
			const Kernel & kernel,
			const Coord1 & current,
			const Coord2 & k_center,
			const Coord3 & arshape,
			const Coord3 & kshape,
			Coord4 & c_k,
			Coord4 & c_a,
			Type & res
		)
		{
			if (Dim == 0){
				int last = NbDim - 1;
				for (int i = 0; i < kshape[last]; ++i){
					//update coordinates
					c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
					if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
						return false;
					else
						res += kernel(c_k) * array(c_a);
				}

				return true;
			}


			int last = NbDim - Dim - 1;
			//loop on kernel coordinates
			for (int i = 0; i < kshape[last]; ++i) {
				//update coordinates
				c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
				if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
					return false;
				else
					if (!ArrayConvolve<Vip::Avoid>::apply(Dim - 1, NbDim, array, kernel, current, k_center, arshape, kshape, c_k, c_a, res))
						return false;
			}

			return true;
		}
	};

	template <>
	struct ArrayConvolve<Vip::Nearest>
	{
		template< class Array, class Kernel, class Type, class Coord1, class Coord2, class Coord3, class Coord4>
		static bool apply(int Dim, int NbDim,
			const Array & array,
			const Kernel & kernel,
			const Coord1 & current,
			const Coord2 & k_center,
			const Coord3 & arshape,
			const Coord3 & kshape,
			Coord4 & c_k,
			Coord4 & c_a,
			Type & res
		)
		{
			if (Dim == 0){
				int last = NbDim - 1;
				for (int i = 0; i < kshape[last]; ++i){
					//update coordinates
					c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
					if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
						c_a[last] = (c_a[last] > 0) * ((int)arshape[last] - 1);
					res += kernel(c_k) * array(c_a);
				}
				return true;
			}


			int last = NbDim - Dim - 1;
			//loop on kernel coordinates
			for (int i = 0; i < kshape[last]; ++i)	{
				//update coordinates
				c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
				if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
					c_a[last] = (c_a[last] > 0) * ((int)arshape[last] - 1);
				ArrayConvolve<Vip::Nearest>::apply(Dim - 1, NbDim, array, kernel, current, k_center, arshape, kshape, c_k, c_a, res);
			}
			return true;
		}
	};

	//template <>
	// struct ArrayConvolve<Vip::Clip>
	// {
	// template< class Array, class Kernel, class Type, class Coord1, class Coord2, class Coord3, class Coord4>
	// static bool apply(int Dim, int NbDim,
	// const Array & array,
	// const Kernel & kernel,
	// const Coord1 & current,
	// const Coord2 & k_center,
	// const Coord3 & arshape,
	// const Coord3 & kshape,
	// Coord4 & c_k,
	// Coord4 & c_a,
	// Type & res
	// )
	// {
	// if (Dim == 0)
	// {
	//	int last = NbDim - 1;
	//	for (int i = 0; i < kshape[last]; ++i){
	//		//update coordinates
	//		c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
	//		if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
	//			res += kernel(c_k) * 0;//array(c_a);
	//		else
	//			res += kernel(c_k) * array(c_a);
	//	}
	//	return true;
	// }
//
//
	// int last = NbDim - Dim - 1;
	// //loop on kernel coordinates
	// for (int i = 0; i < kshape[last]; ++i){
	//	//update coordinates
	//	c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
	//	if (c_a[last] >= 0 && c_a[last] < (int)arshape[last])
	//		ArrayConvolve<Vip::Clip>::apply(Dim - 1, NbDim, array, kernel, current, k_center, arshape, kshape, c_k, c_a, res);
	// }
	// return true;
	// }
	// };

	template <>
	struct ArrayConvolve<Vip::Wrap>
	{
		template< class Array, class Kernel, class Type, class Coord1, class Coord2, class Coord3, class Coord4>
		static bool apply(int Dim, int NbDim,
			const Array & array,
			const Kernel & kernel,
			const Coord1 & current,
			const Coord2 & k_center,
			const Coord3 & arshape,
			const Coord3 & kshape,
			Coord4 & c_k,
			Coord4 & c_a,
			Type & res
		)
		{
			if (Dim == 0){
				int last = NbDim - 1;
				for (int i = 0; i < kshape[last]; ++i){
					//update coordinates
					c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
					if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
						c_a[last] = (c_a[last] % (int)arshape[last]) + (c_a[last] < 0)*arshape[last];
					res += kernel(c_k) * array(c_a);
				}
				return true;
			}


			int last = NbDim - Dim - 1;
			//loop on kernel coordinates
			for (int i = 0; i < kshape[last]; ++i){
				//update coordinates
				c_a[last] = current[last] + (c_k[last] = i) - (int)k_center[last];
				if (c_a[last] < 0 || c_a[last] >= (int)arshape[last])
					c_a[last] = (c_a[last] % (int)arshape[last]) + (c_a[last] < 0)*arshape[last];
				ArrayConvolve<Vip::Wrap>::apply(Dim - 1, NbDim, array, kernel, current, k_center, arshape, kshape, c_k, c_a, res);
			}
			return true;
		}
	};


	template<class T, Vip::ConvolveBorderRule Rule, int NDims >
	struct ApplyConvolve
	{
		template<class Rect, class Array, class Kernel, class Coord1>
		static T apply(const Rect &, const Array & array, const VipNDArrayShape & ashape, const Kernel & kernel,
			const Coord1 & pos, const VipNDArrayShape & k_center, const VipNDArrayShape & kshape, VipNDArrayShape & c_k, VipNDArrayShape & c_a)
		{
			c_k.resize(k_center.size());
			c_a.resize(k_center.size());
			for (int i = 0; i < k_center.size(); ++i) {
				c_k[i] = 0;
				c_a[i] = 0;
			}
			T temp = 0;
			if (ArrayConvolve<Rule>::apply(k_center.size() - 1, k_center.size(), array, kernel, pos, k_center, ashape, kshape, c_k, c_a, temp))
				return temp;
			return array(pos);
		}
	};
	template<class T, Vip::ConvolveBorderRule Rule>
	struct ApplyConvolve<T, Rule, 1>
	{
		template<class Rect, class Array, class Kernel, class Coord1>
		static VIP_ALWAYS_INLINE T apply(const Rect & valid, const Array & array, const VipNDArrayShape & ashape, const Kernel & kernel,
			const Coord1 & pos, const VipNDArrayShape & k_center, const VipNDArrayShape & kshape, VipNDArrayShape&, VipNDArrayShape&)
		{
			if (valid.contains(pos)) {
				T res = 0;
				VipHybridVector<int, 1> p;
				VipHybridVector<int, 1> pk;
				for (pk[0] = 0; pk[0] < kshape[0]; ++pk[0]) {
					p[0] = pos[0] + pk[0] - k_center[0];
					res += kernel(pk) * array(p);
				}
				return res;
			}
			else if (Rule == Vip::Avoid)
				return array(pos);
			else {
				Coord1 _c_k, _c_a;
				_c_k[0] = 0;
				_c_a[0] = 0;
				T res = 0;
				ArrayConvolve<Rule>::apply(0, 1, array, kernel, pos, k_center, ashape, kshape, _c_k, _c_a, res);
				return res;
			}
		}
	};
	template<class T, Vip::ConvolveBorderRule Rule>
	struct ApplyConvolve<T, Rule, 2>
	{
		template<class Rect, class Array, class Kernel, class Coord1>
		static VIP_ALWAYS_INLINE T apply(const Rect & valid, const Array & array, const VipNDArrayShape & ashape, const Kernel & kernel,
			const Coord1 & pos, const VipNDArrayShape & k_center, const VipNDArrayShape & kshape, VipNDArrayShape&, VipNDArrayShape&)
		{

			if (valid.contains(pos)) {
				T res = 0;
				VipHybridVector<int, 2> p;
				VipHybridVector<int, 2> pk;
				const int kshape0 = kshape[0];
				const int kshape1 = kshape[1];
				const int kcenter0 = pos[0] - k_center[0];
				const int kcenter1 = pos[1] - k_center[1];
				for (pk[0] = 0; pk[0] < kshape0; ++pk[0]) {
					p[0] = kcenter0 + pk[0];
					for (pk[1] = 0; pk[1] < kshape1; ++pk[1]) {
						p[1] = kcenter1 + pk[1];
						res += array(p) * kernel(pk);
					}
				}
				return res;
			}
			else if (Rule == Vip::Avoid)
				return array(pos);
			else {
				VipHybridVector<int, 2> _c_k, _c_a;
				for (int i = 0; i < 2; ++i) {
					_c_k[i] = 0;
					_c_a[i] = 0;
				}
				T res = 0;
				ArrayConvolve<Rule>::apply(1, 2, array, kernel, pos, k_center, ashape, kshape, _c_k, _c_a, res);
				return res;
			}
		}
	};
	template<class T, Vip::ConvolveBorderRule Rule>
	struct ApplyConvolve<T, Rule, 3>
	{
		template< class Rect, class Array, class Kernel, class Coord1>
		static VIP_ALWAYS_INLINE T apply(const Rect & valid, const Array & array, const VipNDArrayShape & ashape, const Kernel & kernel,
			const Coord1 & pos, const VipNDArrayShape & k_center, const VipNDArrayShape & kshape, VipNDArrayShape&, VipNDArrayShape&)
		{
			if (valid.contains(pos)) {
				T res = 0;
				VipHybridVector<int, 3> p;
				VipHybridVector<int, 3> pk;
				for (pk[0] = 0; pk[0] < kshape[0]; ++pk[0]) {
					p[0] = pos[0] + pk[0] - k_center[0];
					for (pk[1] = 0; pk[1] < kshape[1]; ++pk[1]) {
						p[1] = pos[1] + pk[1] - k_center[1];
						for (pk[2] = 0; pk[2] < kshape[2]; ++pk[2]) {
							p[2] = pos[2] + pk[2] - k_center[2];
							res += kernel(pk) * array(p);
						}
					}
				}
				return res;
			}
			else if (Rule == Vip::Avoid)
				return array(pos);
			else {
				Coord1 _c_k, _c_a;
				for (int i = 0; i < 3; ++i) {
					_c_k[i] = 0;
					_c_a[i] = 0;
				}
				T res = 0;
				ArrayConvolve<Rule>::apply(2, 3, array, kernel, pos, k_center, ashape, kshape, _c_k, _c_a, res);
				return res;
			}
		}
	};

	template<Vip::ConvolveBorderRule Rule, class A, class Kernel, bool hasNullType = HasNullType<A, Kernel>::value>
	struct Convolve : BaseOperator2<typename DeduceArrayType<A>::value_type, A, Kernel>
	{
		_ENSURE_OPERATOR2_DEF( BaseOperator2<typename DeduceArrayType<A>::value_type, A, Kernel> )
		static const int access_type = Vip::Position;
		//typedef BaseOperator2<typename DeduceArrayType<A>::value_type, A, Kernel> base_type;
		//typedef typename base_type::value_type value_type;
		VipNDRect<Vip::None> valid_rect;
		const VipNDArrayShape kshape;
		const VipNDArrayShape sh;
		const VipNDArrayShape kcenter;
		VipNDArrayShape c_k, c_a;

		Convolve() {}
		Convolve(const A & op1, const Kernel & k, const VipNDArrayShape & kcenter)
			: base(op1, k), kshape(k.shape()), sh(op1.shape()), kcenter(kcenter) {
			valid_rect.resize(k.shape().size());
			for (int i = 0; i < sh.size(); ++i) {
				valid_rect.setStart(i, kcenter[i]);
				valid_rect.setEnd(i, sh[i] - kshape[i] + kcenter[i]);
			}
		}

		const VipNDArrayShape & shape() const { return  this->array1.shape(); }
		int dataType() const { return  this->array1.dataType(); }

		template< class Coord>
		VIP_ALWAYS_INLINE value_type operator()(const Coord & pos) const {
			typedef decltype(typename array_type1::value_type() * typename array_type2::value_type()) op_type;
			return ApplyConvolve<op_type, Rule, Coord::static_size>::apply(valid_rect, this->array1, sh,  this->array2, pos, kcenter, kshape,
				const_cast<VipNDArrayShape&>(c_k), const_cast<VipNDArrayShape&>(c_a));
		}
	};

	template<Vip::ConvolveBorderRule Rule, class A, class Kernel>
	struct Convolve<Rule, A, Kernel, true> : BaseOperator2<NullType, A, Kernel>
	{
		const VipNDArrayShape kcenter;
		Convolve() {}
		Convolve(const A & op1, const Kernel & k, const VipNDArrayShape & kcenter) : BaseOperator2<NullType, A, Kernel>(op1, k), kcenter(kcenter) {}
		const VipNDArrayShape & shape() const { return  this->array1.shape(); }
	};

	template<class T, Vip::ConvolveBorderRule Rule, class A1, class A2, bool hasNullType>
	struct rebind<T, Convolve<Rule, A1, A2, hasNullType>, false> {

		typedef Convolve<Rule, A1, A2, hasNullType> op;
		typedef Convolve<Rule, typename rebind<T, typename op::array_type1>::type, typename rebind<T, typename op::array_type2>::type, false> type;
		static type cast(const op & a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), rebind<T, typename op::array_type2>::cast(a.array2), a.kcenter); }
	};

	template<class T1, class T2>
	using try_convolve = decltype(T1()*T2() + T1());

	template<Vip::ConvolveBorderRule Rule, class A, class Kernel>
	struct is_valid_functor<Convolve<Rule, A, Kernel, false> > : is_valid_op2 < typename DeduceArrayType<A>::value_type, typename DeduceArrayType<Kernel>::value_type, try_convolve > {};

}//end detail


/// \addtogroup DataType
/// @{

/// Create a functor expression to convolve input \a array with given \a kernel centered on \a kcenter.
/// Borders are handled differently depending on the \a Rule enum value.
///
/// \sa vipEval
template<Vip::ConvolveBorderRule Rule, class A, class Kernel, class Coord>
detail::Convolve<Rule, A, Kernel> vipConvolve(const A & array, const Kernel & k, const Coord & kcenter)
{
	return detail::Convolve<Rule, A, Kernel>(array, k, kcenter);
}

/// @}
//end DataType






#endif
