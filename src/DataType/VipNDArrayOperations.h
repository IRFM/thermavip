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

#ifndef VIP_NDARRAY_OPERATIONS
#define VIP_NDARRAY_OPERATIONS

#include <type_traits>

#include <qimage.h>
#include <qset.h>
#include <qsharedpointer.h>

#include "VipNDArray.h"

#ifdef _MSC_VER
// Disable warning message 4804 (unsafe use of type 'bool' in operation)
#pragma warning(disable : 4804)
#endif

namespace detail
{

	// record a conversion from VipNDArray to VipNDArrayType
	struct Conversion
	{
		SharedHandle source;	 // source VipNDArray
		SharedHandle dest;	 // dest VipNDArrayType
		std::intptr_t dest_type; // data type of VipNDArrayType
					 // comparison operator, only check for source array and dest type, as we want to find a conversion
		bool operator==(const Conversion& other) const { return source == other.source && dest_type == other.dest_type; }
	};
	// to store in QSet
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	inline uint qHash(const Conversion& c)
	{
		return (uint)( (std::intptr_t)c.source.data() ^ c.dest_type);
	}
	
#else
	inline size_t qHash(const Conversion& c, size_t seed)
	{
		return (size_t)((std::intptr_t)c.source.data() ^ c.dest_type) ^ seed;
	}
#endif
	

	/// \internal
	/// Store all possible conversions from VipNDArray to VipNDArrayType when casting a functor expression.
	/// This is usefull when converting a functor expression involving the same VipNDArray several times.
	/// Using this context, the VipNDArray is converted only once (triggering only one allocation) instead of multiple times.
	struct FunctorContext
	{
		// store all conversions
		QSet<Conversion> conversions;

		// find a conversion for input array and destination data type
		SharedHandle findConversion(const SharedHandle& src, int dst_type) const
		{
			Conversion c;
			c.source = src;
			c.dest_type = dst_type;
			QSet<Conversion>::const_iterator it = conversions.find(c);
			if (it != conversions.end())
				return it->dest;
			return SharedHandle();
		}

		/// In the frame of a functor expression, convert an array containing a QImage to a VipNDArrayType<VipRGB> without copying the data
		inline VipNDArrayType<VipRGB> convertRGB(const VipNDArray& src)
		{
			QImage* img = (QImage*)(src.data());
			StdHandle<VipRGB>* h = new StdHandle<VipRGB>();
			h->own = false;
			h->opaque = img->bits();
			h->shape = vipVector(img->height(), img->width());
			h->size = vipComputeDefaultStrides<Vip::FirstMajor>(h->shape, h->strides);
			return VipNDArrayType<VipRGB>(SharedHandle(h));
		}

		/// Consert input VipNDArray to Dst.
		/// If the conversion already exists, the returned VipNDArrayType uses an existing SharedHandle, avoiding an additional allocation/copy.
		/// Otherwise, the conversion is performed (allocation + copy) and stored within the list of possible conversions.
		template<class Dst>
		Dst convert(const VipNDArray& src)
		{
			const SharedHandle h = findConversion(src.sharedHandle(), qMetaTypeId<typename Dst::value_type>());
			if (h)
				return Dst(h);
			Dst res = (src.dataType() == qMetaTypeId<QImage>() && !src.isView()) ? convertRGB(src) : (src);
			Conversion c;
			c.source = src.sharedHandle();
			c.dest_type = res.dataType();
			c.dest = res.sharedHandle();
			conversions.insert(c);
			return res;
		}

		/// Returns the global context for the current thread
		static FunctorContext& instance()
		{
			thread_local FunctorContext inst;
			return inst;
		}
		static int& refCount()
		{
			thread_local int count = 0;
			return count;
		}
		/// Register the global context for the current thread
		static void addContext() { ++refCount(); }
		/// Unregister the global context for the current thread
		static void removeContext()
		{
			if (--refCount() == 0) {
				instance().conversions.clear();
			}
		}
	};
	/// RAII based class to register/unregsiter the global context for the current thread
	///  We need to unregister the conversions just before the functor evaluation is performed,
	///  since registered conversions hold references on arrays. If the destination array is also present
	///  as input within the functor expression, this will trigger an allocation.
	struct ContextHelper
	{
		ContextHelper() { FunctorContext::addContext(); }
		~ContextHelper() { FunctorContext::removeContext(); }
	};

	/// Compute offset based on strides and position for VipNDArrayType only
	template<qsizetype Dim>
	struct Offset
	{
		template<class Strides>
		static qsizetype compute(const Strides& str, const VipCoordinate<Dim>& pos)
		{
			// default implementation
			qsizetype p = pos.last();
			for (qsizetype i = pos.size() - 2; i >= 0; --i)
				p += str[i] * pos[i];
			return p;
		}
	};
	template<>
	struct Offset<1>
	{
		template<class Strides>
		static qsizetype compute(const Strides&, const VipCoordinate<1>& pos)
		{
			return pos[0];
		}
	};
	template<>
	struct Offset<2>
	{
		template<class Strides>
		static qsizetype compute(const Strides& str, const VipCoordinate<2>& pos)
		{
			return pos[0] * str[0] + pos[1];
		}
	};
	template<>
	struct Offset<3>
	{
		template<class Strides>
		static qsizetype compute(const Strides& str, const VipCoordinate<3>& pos)
		{
			return pos[0] * str[0] + pos[1] * str[1] + pos[2];
		}
	};

	template<class T>
	struct FromValue
	{
		template<class U>
		static const T& value(const U& v)
		{
			return v;
		}
		static T value(const QVariant& v) { return v.value<T>(); }
	};
	/// Constant operand, simply wrap a constant value
	template<class T>
	struct ConstValue : BaseExpression
	{
		typedef T value_type;
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		const T value;
		ConstValue()
		  : value(T())
		{
		}
		ConstValue(const ConstValue& other)
		  : value(other.value)
		{
		}
		ConstValue(const T& value)
		  : value(FromValue<T>::value(value))
		{
		}
		template<class U>
		ConstValue(const ConstValue<U>& other)
		  : value(FromValue<T>::value(other.value))
		{
		}
		template<class U>
		ConstValue(const U& other)
		  : value(other)
		{
		}
		int dataType() const { return qMetaTypeId<T>(); }
		bool isEmpty() const { return false; }
		bool isUnstrided() const { return true; }
		const VipNDArrayShape& shape() const
		{
			static const VipNDArrayShape null_shape;
			return null_shape;
		}
		template<class ShapeType>
		value_type operator()(const ShapeType&) const
		{
			return value;
		}
		value_type operator[](qsizetype) const { return value; }
	};

	/// Specialization for QVariant
	template<>
	struct ConstValue<QVariant> : BaseExpression
	{
		typedef NullType value_type;
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		const QVariant value;
		ConstValue() {}
		template<class T>
		ConstValue(const T& value)
		  : value(QVariant::fromValue(value))
		{
		}
		int dataType() const { return value.userType(); }
		bool isEmpty() const { return value.isNull(); }
		bool isUnstrided() const { return true; }
		const VipNDArrayShape& shape() const
		{
			static const VipNDArrayShape null_shape;
			return null_shape;
		}
		template<class ShapeType>
		const QVariant& operator()(const ShapeType&) const
		{
			return value;
		}
		QVariant operator[](qsizetype) const { return QVariant(); }
	};

	/// Get object shape
	template<class Obj>
	const VipNDArrayShape& GetShape(const Obj& obj) noexcept
	{
		return obj.shape();
	}
	template<class T, qsizetype Dims>
	const VipNDArrayShape& GetShape(const VipNDArrayType<T, Dims>& obj) noexcept
	{
		return obj.VipNDArray::shape();
	}
	template<class T, qsizetype Dims>
	const VipNDArrayShape& GetShape(const VipNDArrayTypeView<T, Dims>& obj) noexcept
	{
		return obj.VipNDArray::shape();
	}

	// TODO
	// Add thread based context to avoid converting the same VipNDArray multiple times when used several times in a functor expression

	/// Wrap a VipNDArrayType or VipNDArrayTypeView
	template<class Array>
	struct ArrayWrapper : BaseExpression
	{
		typedef typename Array::value_type value_type;
		static const qsizetype access_type = Array::access_type;
		const Array& array;
		const value_type* ptr;
		ArrayWrapper() {}
		ArrayWrapper(const Array& ar)
		  : array(ar)
		  , ptr(ar.ptr())
		{
		}
		int dataType() const { return array.dataType(); }
		bool isEmpty() const { return array.isEmpty(); }
		bool isUnstrided() const { return array.isUnstrided(); }
		const VipNDArrayShape& shape() const { return GetShape(array); }
		template<class ShapeType>
		const value_type& operator()(const ShapeType& pos) const
		{
			return array(pos);
		}
		value_type operator[](qsizetype i) const { return ptr[i]; }
	};
	/// Specialization for VipNDArrayType
	template<class T, qsizetype NDims>
	struct ArrayWrapper<VipNDArrayType<T, NDims>> : BaseExpression
	{
		typedef T value_type;
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		const VipNDArrayType<T, NDims> array;
		// VipNDArrayShape _shape;
		//  VipNDArrayShape strides;
		//  int _dataType;
		const T* ptr;
		ArrayWrapper()
		  : ptr(nullptr)
		{
		}
		template<class Other>
		ArrayWrapper(const Other& other)
		  : array(FunctorContext::instance().convert<VipNDArrayType<T, NDims>>(other))
		  , ptr(array.ptr())
		{
		}
		template<class Other>
		ArrayWrapper(const ArrayWrapper<Other>& other)
		  : array(FunctorContext::instance().convert<VipNDArrayType<T, NDims>>(other.array))
		  , ptr(array.ptr())
		{
		}
		ArrayWrapper(const ArrayWrapper& other)
		  : //_shape(other._shape), strides(other.strides), _dataType(other._dataType), ptr(other.ptr)
		  array(other.array)
		  , ptr(other.ptr)
		{
		}
		ArrayWrapper(const VipNDArrayType<T, NDims>& array)
		  : array(array)
		  , ptr(array.ptr())
		{
		} //{
		  // _shape = array.shape();
		  // strides = array.strides();
		  // _dataType = array.dataType();
		  // ptr = array.ptr();
		// }
		template<qsizetype ONDims>
		ArrayWrapper(const VipNDArrayType<T, ONDims>& array)
		  : array(array)
		  , ptr(array.ptr())
		{
		}
		int dataType() const
		{
			return array.dataType(); //_dataType;//
		}
		bool isEmpty() const { return array.isEmpty(); } // ptr == 0; }//array.isEmpty(); }
		bool isUnstrided() const { return true; }
		const VipNDArrayShape& shape() const
		{
			return GetShape(array); // return _shape;
		}				// array.shape(); }
		template<class ShapeType>
		const value_type& operator()(const ShapeType& pos) const
		{
			return ptr[Offset<ShapeType::static_size>::compute(array.strides(), pos)];
		}
		value_type operator[](qsizetype i) const { return ptr[i]; }
	};

	// rebind U to T
	template<class T, class U, bool isArray = std::is_base_of<VipNDArray, U>::value>
	struct rebind
	{
		typedef U type;
		static type cast(const U& a) { return a; }
	};
	// rebind QVariant
	template<class T>
	struct rebind<T, QVariant, false>
	{
		typedef T type;
		static type cast(const QVariant& a) { return a.value<T>(); }
	};
	// rebind VipNDArray inheriting class
	template<class T, class U>
	struct rebind<T, U, true>
	{
		typedef ArrayWrapper<VipNDArrayType<T>> type;
		static type cast(const U& a) { return type(a); }
	};
	// rebind VipNDArrayType class (does not change the type)
	template<class T, class U, qsizetype NDims>
	struct rebind<T, VipNDArrayType<U, NDims>, true>
	{
		typedef ArrayWrapper<VipNDArrayType<U, NDims>> type;
		static type cast(const VipNDArrayType<U, NDims>& a) { return a; }
	};
	// rebind VipNDArrayTypeView class (does not change the type)
	template<class T, class U, qsizetype NDims>
	struct rebind<T, VipNDArrayTypeView<U, NDims>, true>
	{
		typedef VipNDArrayTypeView<U, NDims> type;
		static type cast(const VipNDArrayTypeView<U, NDims>& a) { return a; }
	};
	// rebind ConstValue (does not change the type)
	template<class T, class U>
	struct rebind<T, ConstValue<U>, false>
	{
		typedef ConstValue<U> type;
		static const type& cast(const ConstValue<U>& a) { return a; }
	};
	// rebind ConstValue<QVariant>
	template<class T>
	struct rebind<T, ConstValue<QVariant>, false>
	{
		typedef ConstValue<T> type;
		static type cast(const ConstValue<QVariant>& a) { return a; }
	};
	// rebind ArrayWrapper
	template<class T, class U>
	struct rebind<T, ArrayWrapper<U>, true>
	{
		typedef ArrayWrapper<typename rebind<T, U>::type> type;
		static type cast(const ArrayWrapper<U>& a) { return type(a.array); }
	};
	template<class T>
	struct rebind<T, ArrayWrapper<T>, true>
	{
		typedef ArrayWrapper<T> type;
		static type cast(const ArrayWrapper<T>& a) { return a; }
	};

	template<class T1, template<class> class Op, class = void>
	struct is_valid_op1 : std::false_type
	{
	};

	template<template<class> class Op>
	struct is_valid_op1<NullType, Op> : std::true_type
	{
	};

	template<class T1, template<class> class Op>
	struct is_valid_op1<T1, Op, std::void_t<Op<T1>>> : std::true_type
	{
	};

	template<class T1, class T2, template<class, class> class Op, class = void>
	struct is_valid_op2 : std::false_type
	{
	};

	template<class T1, class T2, template<class, class> class Op>
	struct is_valid_op2<T1, T2, Op, std::void_t<Op<T1, T2>>> : std::true_type
	{
	};

	template<class T1, class T2, class T3, template<class, class, class> class Op, class = void>
	struct is_valid_op3 : std::false_type
	{
	};

	template<class T1, class T2, class T3, template<class, class, class> class Op>
	struct is_valid_op3<T1, T2, T3, Op, std::void_t<Op<T1, T2, T3>>> : std::true_type
	{
	};

	template<class T1, class T2, class T3, class T4, template<class, class, class, class> class Op, class = void>
	struct is_valid_op4 : std::false_type
	{
	};

	template<class T1, class T2, class T3, class T4, template<class, class, class, class> class Op>
	struct is_valid_op4<T1, T2, T3, T4, Op, std::void_t<Op<T1, T2, T3, T4>>> : std::true_type
	{
	};

	namespace helper
	{

		template<typename T>
		struct is_complete_helper
		{
			template<typename U>
			static auto test(U*) -> std::integral_constant<bool, sizeof(U) == sizeof(U)>;
			static auto test(...) -> std::false_type;
			using type = decltype(test((T*)0));
		};

		template<class Array, bool Comp = is_complete_helper<Array>::type::value>
		struct is_array_helper;

		template<class Array>
		struct is_array_helper<Array, true> : std::is_base_of<NullOperand, Array>
		{
		};

		template<class Array>
		struct is_array_helper<Array, false> : std::false_type
		{
		};

	}

	template<typename T>
	struct is_complete : helper::is_complete_helper<T>::type
	{
	};
	template<typename T>
	struct is_nd_array : helper::is_array_helper<T>::type
	{
	};

	/// Deduce array type of Array with typedef array_type, and the value type with typedef value_type
	template<class Array, bool IsArray = is_nd_array<Array>::value>
	struct DeduceArrayType
	{
	};
	template<class Array>
	struct DeduceArrayType<Array, true>
	{
		typedef Array array_type;
		typedef typename Array::value_type value_type;
	};
	template<class T, qsizetype NDims>
	struct DeduceArrayType<VipNDArrayType<T, NDims>, true>
	{
		typedef ArrayWrapper<VipNDArrayType<T, NDims>> array_type;
		typedef T value_type;
	};
	template<class T, qsizetype NDims>
	struct DeduceArrayType<VipNDArrayTypeView<T, NDims>, true>
	{
		typedef ArrayWrapper<VipNDArrayTypeView<T, NDims>> array_type;
		typedef T value_type;
	};
	template<class Array>
	struct DeduceArrayType<Array, false>
	{
		typedef ConstValue<Array> array_type;
		typedef Array value_type;
	};
	template<>
	struct DeduceArrayType<QVariant, false>
	{
		typedef ConstValue<QVariant> array_type;
		typedef NullType value_type;
	};

	/// Check whether a functor is valid or not
	template<class T, class = void>
	struct is_valid_functor : std::is_base_of<VipNDArray, T>
	{
		static void apply() {}
	};
	// template< class T>
	// struct is_valid_functor<T, typename std::enable_if<std::is_same<NullType, typename DeduceArrayType<T>::value_type>::value, void>::type> : std::true_type { static void apply() {} };
	template<class T>
	struct is_valid_functor<ConstValue<T>> : std::true_type
	{
		static void apply() {}
	};
	template<class T>
	struct is_valid_functor<ArrayWrapper<T>> : std::true_type
	{
		static void apply() {}
	};

	/// Check if given arrays and/or constants define NullType (at least one of them is a VipNDArray)
	template<class A1, class A2 = void, class A3 = void>
	struct HasNullType
	{
		typedef typename DeduceArrayType<A1>::value_type t1;
		typedef typename DeduceArrayType<A2>::value_type t2;
		typedef typename DeduceArrayType<A3>::value_type t3;
		static const bool value = std::is_same<t1, NullType>::value || std::is_same<t2, NullType>::value || std::is_same<t3, NullType>::value;
	};
	/// Check if at least one template argument is a NullOperand
	template<class A1, class A2 = void, class A3 = void>
	struct HasArrayType
	{
		static const bool value = std::is_base_of<NullOperand, A1>::value || std::is_base_of<NullOperand, A2>::value || std::is_base_of<NullOperand, A3>::value;
	};

	/// Check if type inherits VipNDArray, but is not typed (like VipNDArrayType and VipNDArrayTypeView)
	template<class T>
	struct IsNDArray
	{
		static const bool value = std::is_base_of<VipNDArray, T>::value && HasNullType<T>::value;
	};
	/// Check if type is a NullOperand but not a VipNDArray
	template<class T>
	struct IsOperand
	{
		static const bool value = !std::is_base_of<VipNDArray, T>::value && std::is_base_of<NullOperand, T>::value;
	};

	/// Check if types are not NullOperand ones
	template<class A1, class A2 = void, class A3 = void>
	struct IsNoOperand
	{
		static const bool value = !std::is_base_of<NullOperand, A1>::value && !std::is_base_of<NullOperand, A2>::value && !std::is_base_of<NullOperand, A2>::value;
	};

	template<class T, class ArrayType>
	struct BaseOperator1 : BaseExpression
	{
		typedef BaseOperator1<T, ArrayType> base_type;
		typedef T value_type;
		typedef typename DeduceArrayType<ArrayType>::array_type array_type1;
		static const qsizetype access_type = array_type1::access_type;
		const array_type1 array1;
		int dataType() const { return std::is_same<T, NullType>::value ? array1.dataType() : qMetaTypeId<T>(); }
		bool isEmpty() const { return array1.isEmpty(); }
		bool isUnstrided() const { return array1.isUnstrided(); }
		const VipNDArrayShape& shape() const { return GetShape(array1); }
		BaseOperator1() {}
		BaseOperator1(const ArrayType& op1)
		  : array1(op1)
		{
		}
		// define dummy access operator to avoid compilation error
		template<class ShapeType>
		value_type operator()(const ShapeType&) const
		{
			return value_type();
		}
		value_type operator[](qsizetype) const { return value_type(); }
	};

#define _ENSURE_OPERATOR1_DEF(...)                                                                                                                                                                     \
	typedef __VA_ARGS__ base;                                                                                                                                                                      \
	typedef typename base::value_type value_type;                                                                                                                                                  \
	typedef typename base::array_type1 array_type1;                                                                                                                                                \
	using base::shape;                                                                                                                                                                             \
	using base::dataType;                                                                                                                                                                          \
	using base::isEmpty;                                                                                                                                                                           \
	using base::isUnstrided;

	static const VipNDArrayShape _null_shape;

	template<class T, class ArrayType1, class ArrayType2>
	struct BaseOperator2 : BaseExpression
	{
		typedef BaseOperator2<T, ArrayType1, ArrayType2> base_type;
		typedef T value_type;
		typedef typename DeduceArrayType<ArrayType1>::array_type array_type1;
		typedef typename DeduceArrayType<ArrayType2>::array_type array_type2;
		static const qsizetype access_type = array_type1::access_type & array_type2::access_type;
		const array_type1 array1;
		const array_type2 array2;
		const VipNDArrayShape* sh;
		int _dataType;

		int dataType() const
		{
			if (_dataType == 0) {
				if (std::is_same<T, NullType>::value) {
					int dtype = vipHigherArrayType(array1.dataType(), array2.dataType());
					if (dtype == 0)
						dtype = array1.dataType();
					const_cast<int&>(_dataType) = dtype;
				}
				else {
					const_cast<int&>(_dataType) = qMetaTypeId<T>();
				}
			}
			return _dataType;
		}
		bool isEmpty() const { return array1.isEmpty() || array2.isEmpty(); }
		const VipNDArrayShape& shape() const { return *sh; }
		bool isUnstrided() const { return array1.isUnstrided() && array2.isUnstrided(); }
		BaseOperator2() {}
		BaseOperator2(const ArrayType1& op1, const ArrayType2& op2)
		  : array1(op1)
		  , array2(op2)
		  , sh(&_null_shape)
		  , _dataType(0)
		{
			if (!array1.shape().isEmpty() && !array2.shape().isEmpty()) {
				if (array1.shape() == array2.shape())
					sh = &GetShape(array1);
			}
			else {
				if (!array1.shape().isEmpty())
					sh = &GetShape(array1);
				else
					sh = &GetShape(array2);
			}
		}
		// define dummy access operator to avoid compilation error
		template<class ShapeType>
		value_type operator()(const ShapeType&) const
		{
			return value_type();
		}
		value_type operator[](qsizetype) const { return value_type(); }
	};

#define _ENSURE_OPERATOR2_DEF(...)                                                                                                                                                                     \
	typedef __VA_ARGS__ base;                                                                                                                                                                      \
	typedef typename base::value_type value_type;                                                                                                                                                  \
	typedef typename base::array_type1 array_type1;                                                                                                                                                \
	typedef typename base::array_type2 array_type2;                                                                                                                                                \
	using base::shape;                                                                                                                                                                             \
	using base::dataType;                                                                                                                                                                          \
	using base::isEmpty;                                                                                                                                                                           \
	using base::isUnstrided;

	template<class T, class ArrayType1, class ArrayType2, class ArrayType3>
	struct BaseOperator3 : BaseExpression
	{
		typedef T value_type;
		typedef typename DeduceArrayType<ArrayType1>::array_type array_type1;
		typedef typename DeduceArrayType<ArrayType2>::array_type array_type2;
		typedef typename DeduceArrayType<ArrayType3>::array_type array_type3;
		static const qsizetype access_type = array_type1::access_type & array_type2::access_type & array_type3::access_type;
		const array_type1 array1;
		const array_type2 array2;
		const array_type3 array3;
		const VipNDArrayShape* sh;
		int _dataType;

		int dataType() const
		{
			if (_dataType == 0) {
				if (std::is_same<T, NullType>::value) {
					int dtype = vipHigherArrayType(array1.dataType(), array2.dataType());
					dtype = vipHigherArrayType(dtype, array3.dataType());
					if (dtype == 0)
						dtype = array1.dataType();
					const_cast<int&>(_dataType) = dtype;
				}
				else {
					const_cast<int&>(_dataType) = qMetaTypeId<T>();
				}
			}
			return _dataType;
		}
		bool isEmpty() const { return array1.isEmpty() || array2.isEmpty() || array3.isEmpty(); }
		const VipNDArrayShape& shape() const { return *sh; }
		bool isUnstrided() const { return array1.isUnstrided() && array2.isUnstrided() && array3.isUnstrided(); }
		BaseOperator3() {}
		BaseOperator3(const ArrayType1& op1, const ArrayType2& op2, const ArrayType3& op3)
		  : array1(op1)
		  , array2(op2)
		  , array3(op3)
		  , sh(&_null_shape)
		  , _dataType(0)
		{
			sh = array1.shape().isEmpty() ? (array2.shape().isEmpty() ? &GetShape(array3) : &GetShape(array2)) : &GetShape(array1);
			if (!array1.shape().isEmpty() && array1.shape() != *sh)
				sh = &_null_shape; //.clear();
			else if (!array2.shape().isEmpty() && array2.shape() != *sh)
				sh = &_null_shape; //.clear();
			else if (!array3.shape().isEmpty() && array3.shape() != *sh)
				sh = &_null_shape; //.clear();
		}
		// define dummy access operator to avoid compilation error
		template<class ShapeType>
		value_type operator()(const ShapeType&) const
		{
			return value_type();
		}
		value_type operator[](qsizetype) const { return value_type(); }
	};

#define _ENSURE_OPERATOR3_DEF(...)                                                                                                                                                                     \
	typedef __VA_ARGS__ base;                                                                                                                                                                      \
	typedef typename base::value_type value_type;                                                                                                                                                  \
	typedef typename base::array_type1 array_type1;                                                                                                                                                \
	typedef typename base::array_type2 array_type2;                                                                                                                                                \
	typedef typename base::array_type3 array_type3;                                                                                                                                                \
	using base::shape;                                                                                                                                                                             \
	using base::dataType;                                                                                                                                                                          \
	using base::isEmpty;                                                                                                                                                                           \
	using base::isUnstrided;

	// create rebind specialization for functor inheriting BaseOperator1
#define _REBIND_HELPER1(functor)                                                                                                                                                                       \
	template<class T, class A1, bool hasNullType>                                                                                                                                                  \
	struct rebind<T, functor<A1, hasNullType>, false>                                                                                                                                              \
	{                                                                                                                                                                                              \
		typedef functor<A1, hasNullType> op;                                                                                                                                                   \
		typedef functor<typename rebind<T, typename op::array_type1>::type, false> type;                                                                                                       \
		static type cast(const op& a) { return type(rebind<T, typename op::array_type1>::cast(a.array1)); }                                                                                    \
	};

	// create rebind specialization for functor inheriting BaseOperator2
#define _REBIND_HELPER2(functor)                                                                                                                                                                       \
	template<class T, class A1, class A2, bool hasNullType>                                                                                                                                        \
	struct rebind<T, functor<A1, A2, hasNullType>, false>                                                                                                                                          \
	{                                                                                                                                                                                              \
		typedef functor<A1, A2, hasNullType> op;                                                                                                                                               \
		typedef functor<typename rebind<T, typename op::array_type1>::type, typename rebind<T, typename op::array_type2>::type, false> type;                                                   \
		static type cast(const op& a) { return type(rebind<T, typename op::array_type1>::cast(a.array1), rebind<T, typename op::array_type2>::cast(a.array2)); }                               \
	};

	// create rebind specialization for functor inheriting BaseOperator3
#define _REBIND_HELPER3(functor)                                                                                                                                                                       \
	template<class T, class A1, class A2, class A3, bool hasNullType>                                                                                                                              \
	struct rebind<T, functor<A1, A2, A3, hasNullType>, false>                                                                                                                                      \
	{                                                                                                                                                                                              \
		typedef functor<A1, A2, A3, hasNullType> op;                                                                                                                                           \
		typedef functor<typename rebind<T, typename op::array_type1>::type, typename rebind<T, typename op::array_type2>::type, typename rebind<T, typename op::array_type3>::type, false>     \
		  type;                                                                                                                                                                                \
		static type cast(const op& a)                                                                                                                                                          \
		{                                                                                                                                                                                      \
			return type(rebind<T, typename op::array_type1>::cast(a.array1), rebind<T, typename op::array_type2>::cast(a.array2), rebind<T, typename op::array_type3>::cast(a.array3));    \
		}                                                                                                                                                                                      \
	};

	namespace cast
	{
		// Cast bool types to unsigned char for operators (and remove lengthy warning about boolean comparison)
		template<class T>
		const T& invariant_cast(const T& v)
		{
			return v;
		}
		inline const unsigned char invariant_cast(const bool v)
		{
			return (unsigned char)v;
		}
	}

#define _DECLARE_OPERATOR2(Fun, op)                                                                                                                                                                    \
	namespace detail                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		/*@brief Tells if given type supports operator*/                                                                                                                                       \
		template<class T, class U>                                                                                                                                                             \
		class SupportOperator##Fun                                                                                                                                                             \
		{                                                                                                                                                                                      \
			template<class TT, class UU>                                                                                                                                                   \
			static auto test(int) -> decltype(cast::invariant_cast(TT()) op cast::invariant_cast(UU()), std::true_type());                                                                 \
			template<class, class>                                                                                                                                                         \
			static auto test(...) -> std::false_type;                                                                                                                                      \
                                                                                                                                                                                                       \
		public:                                                                                                                                                                                \
			static constexpr bool value = decltype(test<T, U>(0))::value;                                                                                                                  \
		};                                                                                                                                                                                     \
		template<class A, class B, bool Support = SupportOperator##Fun<A, B>::value>                                                                                                           \
		struct _DeduceOperatorType##Fun                                                                                                                                                        \
		{                                                                                                                                                                                      \
			using value_type = decltype(cast::invariant_cast(A()) op cast::invariant_cast(B()));                                                                                           \
		};                                                                                                                                                                                     \
		template<class A, class B>                                                                                                                                                             \
		struct _DeduceOperatorType##Fun<A, B, false>                                                                                                                                           \
		{                                                                                                                                                                                      \
			using value_type = NullType;                                                                                                                                                   \
		};                                                                                                                                                                                     \
		template<class A, class B>                                                                                                                                                             \
		struct DeduceOperatorType##Fun                                                                                                                                                         \
		{                                                                                                                                                                                      \
			static constexpr bool valid = SupportOperator##Fun<typename DeduceArrayType<A>::value_type, typename DeduceArrayType<B>::value_type>::value;                                   \
			using value_type = typename _DeduceOperatorType##Fun<typename DeduceArrayType<A>::value_type, typename DeduceArrayType<B>::value_type>::value_type;                            \
		};                                                                                                                                                                                     \
		template<class A1, class A2, bool hasNullType = HasNullType<A1, A2>::value>                                                                                                            \
		struct Fun : BaseOperator2<typename DeduceOperatorType##Fun<A1, A2>::value_type, A1, A2>                                                                                               \
		{                                                                                                                                                                                      \
			static constexpr bool valid = DeduceOperatorType##Fun<A1, A2>::valid;                                                                                                          \
			typedef BaseOperator2<typename DeduceOperatorType##Fun<A1, A2>::value_type, A1, A2> base_type;                                                                                 \
			Fun() {}                                                                                                                                                                       \
			Fun(const A1& op1, const A2& op2)                                                                                                                                              \
			  : base_type(op1, op2)                                                                                                                                                        \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
			template<class Coord>                                                                                                                                                          \
			typename base_type::value_type operator()(const Coord& pos) const                                                                                                              \
			{                                                                                                                                                                              \
				return cast::invariant_cast(this->array1(pos)) op cast::invariant_cast(this->array2(pos));                                                                             \
			}                                                                                                                                                                              \
			typename base_type::value_type operator[](qsizetype index) const { return cast::invariant_cast(this->array1[index]) op cast::invariant_cast(this->array2[index]); }                  \
		};                                                                                                                                                                                     \
		template<class A1, class A2>                                                                                                                                                           \
		struct Fun<A1, A2, true> : BaseOperator2<NullType, A1, A2>                                                                                                                             \
		{                                                                                                                                                                                      \
			static constexpr bool valid = false;                                                                                                                                           \
			Fun() {}                                                                                                                                                                       \
			Fun(const A1& op1, const A2& op2)                                                                                                                                              \
			  : BaseOperator2<NullType, A1, A2>(op1, op2)                                                                                                                                  \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
		};                                                                                                                                                                                     \
		_REBIND_HELPER2(Fun)                                                                                                                                                                   \
		template<class A1, class A2>                                                                                                                                                           \
		using try_##Fun = typename DeduceOperatorType##Fun<A1, A2>::value_type;                                                                                                                \
		template<class A1, class A2>                                                                                                                                                           \
		struct is_valid_functor<Fun<A1, A2, false>> : is_valid_op2<A1, A2, try_##Fun>                                                                                                          \
		{                                                                                                                                                                                      \
			static void apply() {}                                                                                                                                                         \
		}; /*, std::void_t<decltype(Function(DeduceArrayType<A1>::value_type(),DeduceArrayType<A2>::value_type()))> > :std::true_type{};*/                                                     \
	}                                                                                                                                                                                              \
	template<class A1, class A2>                                                                                                                                                                   \
	typename std::enable_if<detail::HasArrayType<A1, A2>::value, detail::Fun<A1, A2>>::type operator op(const A1& a1, const A2& a2)                                                                \
	{                                                                                                                                                                                              \
		return detail::Fun<A1, A2>(a1, a2);                                                                                                                                                    \
	}

#define _DECLARE_OPERATOR2_FUNCTION(Fun, op, function)                                                                                                                                                 \
	_DECLARE_FUNCTOR_FOR_FUNCTION2(Fun, function)                                                                                                                                                  \
	template<class A1, class A2>                                                                                                                                                                   \
	typename std::enable_if<detail::HasArrayType<A1, A2>::value, detail::Fun<A1, A2>>::type operator op(const A1& a1, const A2& a2)                                                                \
	{                                                                                                                                                                                              \
		return detail::Fun<A1, A2>(a1, a2);                                                                                                                                                    \
	}

#define _DECLARE_FUNCTOR_FOR_FUNCTION1(Functor, Function)                                                                                                                                              \
	namespace detail                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		/*@brief Tells if given type supports operator*/                                                                                                                                       \
		template<class T>                                                                                                                                                                      \
		class Support##Functor                                                                                                                                                                 \
		{                                                                                                                                                                                      \
			template<class TT>                                                                                                                                                             \
			static auto test(int) -> decltype(Function(cast::invariant_cast(TT())), std::true_type());                                                                                     \
			template<class>                                                                                                                                                                \
			static auto test(...) -> std::false_type;                                                                                                                                      \
                                                                                                                                                                                                       \
		public:                                                                                                                                                                                \
			static constexpr bool value = decltype(test<T>(0))::value;                                                                                                                     \
		};                                                                                                                                                                                     \
		template<class A, bool Support = Support##Functor<A>::value>                                                                                                                           \
		struct _DeduceOperatorType##Functor                                                                                                                                                    \
		{                                                                                                                                                                                      \
			using value_type = decltype(Function(cast::invariant_cast(A())));                                                                                                              \
		};                                                                                                                                                                                     \
		template<class A>                                                                                                                                                                      \
		struct _DeduceOperatorType##Functor<A, false>                                                                                                                                          \
		{                                                                                                                                                                                      \
			using value_type = NullType;                                                                                                                                                   \
		};                                                                                                                                                                                     \
		template<class A>                                                                                                                                                                      \
		struct DeduceOperatorType##Functor                                                                                                                                                     \
		{                                                                                                                                                                                      \
			static constexpr bool valid = Support##Functor<typename DeduceArrayType<A>::value_type>::value;                                                                                \
			using value_type = typename _DeduceOperatorType##Functor<typename DeduceArrayType<A>::value_type>::value_type;                                                                 \
		};                                                                                                                                                                                     \
                                                                                                                                                                                                       \
		template<class A1, bool hasNullType = HasNullType<A1>::value>                                                                                                                          \
		struct Functor : BaseOperator1<typename DeduceOperatorType##Functor<A1>::value_type, A1>                                                                                               \
		{                                                                                                                                                                                      \
			static constexpr bool valid = DeduceOperatorType##Functor<A1>::valid;                                                                                                          \
			typedef BaseOperator1<typename DeduceOperatorType##Functor<A1>::value_type, A1> base_type;                                                                                     \
			Functor() {}                                                                                                                                                                   \
			Functor(const A1& op1)                                                                                                                                                         \
			  : base_type(op1)                                                                                                                                                             \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
			template<class Coord>                                                                                                                                                          \
			typename base_type::value_type operator()(const Coord& pos) const                                                                                                              \
			{                                                                                                                                                                              \
				return Function(this->array1(pos));                                                                                                                                    \
			}                                                                                                                                                                              \
			typename base_type::value_type operator[](qsizetype index) const { return Function(this->array1[index]); }                                                                           \
		};                                                                                                                                                                                     \
		template<class A1>                                                                                                                                                                     \
		struct Functor<A1, true> : BaseOperator1<NullType, A1>                                                                                                                                 \
		{                                                                                                                                                                                      \
			static constexpr bool valid = false;                                                                                                                                           \
			typedef BaseOperator1<NullType, A1> base_type;                                                                                                                                 \
			Functor() {}                                                                                                                                                                   \
			Functor(const A1& op1)                                                                                                                                                         \
			  : base_type(op1)                                                                                                                                                             \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
		};                                                                                                                                                                                     \
		_REBIND_HELPER1(Functor)                                                                                                                                                               \
		template<class A1>                                                                                                                                                                     \
		using try_##Functor = decltype(Function(A1()));                                                                                                                                        \
		template<class A1>                                                                                                                                                                     \
		struct is_valid_functor<Functor<A1, false>> : is_valid_op1<typename DeduceArrayType<A1>::value_type, try_##Functor>                                                                    \
		{                                                                                                                                                                                      \
		}; /*, std::void_t<decltype(Function(DeduceArrayType<A1>::value_type()))> > : std::true_type{};*/                                                                                      \
	}

#define _DECLARE_FUNCTOR_FOR_FUNCTION2(Functor, Function)                                                                                                                                              \
	namespace detail                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		/*@brief Tells if given type supports operator*/                                                                                                                                       \
		template<class T, class U>                                                                                                                                                             \
		class Support##Functor                                                                                                                                                                 \
		{                                                                                                                                                                                      \
			template<class TT, class UU>                                                                                                                                                   \
			static auto test(int) -> decltype(Function(cast::invariant_cast(TT()), cast::invariant_cast(UU())), std::true_type());                                                         \
			template<class, class>                                                                                                                                                         \
			static auto test(...) -> std::false_type;                                                                                                                                      \
                                                                                                                                                                                                       \
		public:                                                                                                                                                                                \
			static constexpr bool value = decltype(test<T, U>(0))::value;                                                                                                                  \
		};                                                                                                                                                                                     \
		template<class A, class B, bool Support = Support##Functor<A, B>::value>                                                                                                               \
		struct _DeduceOperatorType##Functor                                                                                                                                                    \
		{                                                                                                                                                                                      \
			using value_type = decltype(Function(cast::invariant_cast(A()), cast::invariant_cast(B())));                                                                                   \
		};                                                                                                                                                                                     \
		template<class A, class B>                                                                                                                                                             \
		struct _DeduceOperatorType##Functor<A, B, false>                                                                                                                                       \
		{                                                                                                                                                                                      \
			using value_type = NullType;                                                                                                                                                   \
		};                                                                                                                                                                                     \
		template<class A, class B>                                                                                                                                                             \
		struct DeduceOperatorType##Functor                                                                                                                                                     \
		{                                                                                                                                                                                      \
			static constexpr bool valid = Support##Functor<typename DeduceArrayType<A>::value_type, typename DeduceArrayType<B>::value_type>::value;                                       \
			using value_type = typename _DeduceOperatorType##Functor<typename DeduceArrayType<A>::value_type, typename DeduceArrayType<B>::value_type>::value_type;                        \
		};                                                                                                                                                                                     \
                                                                                                                                                                                                       \
		template<class A1, class A2, bool hasNullType = HasNullType<A1, A2>::value>                                                                                                            \
		struct Functor : BaseOperator2<typename DeduceOperatorType##Functor<A1, A2>::value_type, A1, A2>                                                                                       \
		{                                                                                                                                                                                      \
			static constexpr bool valid = DeduceOperatorType##Functor<A1, A2>::valid;                                                                                                      \
			typedef BaseOperator2<typename DeduceOperatorType##Functor<A1, A2>::value_type, A1, A2> base_type;                                                                             \
			Functor() {}                                                                                                                                                                   \
			Functor(const A1& op1, const A2& op2)                                                                                                                                          \
			  : base_type(op1, op2)                                                                                                                                                        \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
			template<class Coord>                                                                                                                                                          \
			typename base_type::value_type operator()(const Coord& pos) const                                                                                                              \
			{                                                                                                                                                                              \
				return Function(this->array1(pos), this->array2(pos));                                                                                                                 \
			}                                                                                                                                                                              \
			typename base_type::value_type operator[](qsizetype index) const { return Function(this->array1[index], this->array2[index]); }                                                      \
		};                                                                                                                                                                                     \
		template<class A1, class A2>                                                                                                                                                           \
		struct Functor<A1, A2, true> : BaseOperator2<NullType, A1, A2>                                                                                                                         \
		{                                                                                                                                                                                      \
			static constexpr bool valid = false;                                                                                                                                           \
			typedef BaseOperator2<NullType, A1, A2> base_type;                                                                                                                             \
			Functor() {}                                                                                                                                                                   \
			Functor(const A1& op1, const A2& op2)                                                                                                                                          \
			  : base_type(op1, op2)                                                                                                                                                        \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
		};                                                                                                                                                                                     \
		_REBIND_HELPER2(Functor)                                                                                                                                                               \
		template<class A1, class A2>                                                                                                                                                           \
		using try_##Functor = typename DeduceOperatorType##Functor<A1, A2>::value_type;                                                                                                        \
		template<class A1, class A2>                                                                                                                                                           \
		struct is_valid_functor<Functor<A1, A2, false>> : is_valid_op2<A1, A2, try_##Functor>                                                                                                  \
		{                                                                                                                                                                                      \
		}; /*, std::void_t<decltype(Function(DeduceArrayType<A1>::value_type(),DeduceArrayType<A2>::value_type()))> > :std::true_type{};*/                                                     \
	}

#define _DECLARE_FUNCTOR_FOR_FUNCTION3(Functor, Function)                                                                                                                                              \
	namespace detail                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		/*@brief Tells if given type supports operator*/                                                                                                                                       \
		template<class T, class U, class V>                                                                                                                                                    \
		class Support##Functor                                                                                                                                                                 \
		{                                                                                                                                                                                      \
			template<class TT, class UU, class VV>                                                                                                                                         \
			static auto test(int) -> decltype(Function(cast::invariant_cast(TT()), cast::invariant_cast(UU()), cast::invariant_cast(VV())), std::true_type());                             \
			template<class, class, class>                                                                                                                                                  \
			static auto test(...) -> std::false_type;                                                                                                                                      \
                                                                                                                                                                                                       \
		public:                                                                                                                                                                                \
			static constexpr bool value = decltype(test<T, U, V>(0))::value;                                                                                                               \
		};                                                                                                                                                                                     \
		template<class A, class B, class C, bool Support = Support##Functor<A, B, C>::value>                                                                                                   \
		struct _DeduceOperatorType##Functor                                                                                                                                                    \
		{                                                                                                                                                                                      \
			using value_type = decltype(Function(cast::invariant_cast(A()), cast::invariant_cast(B()), cast::invariant_cast(C())));                                                        \
		};                                                                                                                                                                                     \
		template<class A, class B, class C>                                                                                                                                                    \
		struct _DeduceOperatorType##Functor<A, B, C, false>                                                                                                                                    \
		{                                                                                                                                                                                      \
			using value_type = NullType;                                                                                                                                                   \
		};                                                                                                                                                                                     \
		template<class A, class B, class C>                                                                                                                                                    \
		struct DeduceOperatorType##Functor                                                                                                                                                     \
		{                                                                                                                                                                                      \
			static constexpr bool valid =                                                                                                                                                  \
			  Support##Functor<typename DeduceArrayType<A>::value_type, typename DeduceArrayType<B>::value_type, typename DeduceArrayType<C>::value_type>::value;                          \
			using value_type = typename _DeduceOperatorType##Functor<typename DeduceArrayType<A>::value_type,                                                                              \
										 typename DeduceArrayType<B>::value_type,                                                                              \
										 typename DeduceArrayType<C>::value_type>::value_type;                                                                 \
		};                                                                                                                                                                                     \
                                                                                                                                                                                                       \
		template<class A1, class A2, class A3, bool hasNullType = HasNullType<A1, A2, A3>::value>                                                                                              \
		struct Functor : BaseOperator3<typename DeduceOperatorType##Functor<A1, A2, A3>::value_type, A1, A2, A3>                                                                               \
		{                                                                                                                                                                                      \
			static constexpr bool valid = DeduceOperatorType##Functor<A1, A2, A3>::valid;                                                                                                  \
			typedef BaseOperator3<typename DeduceOperatorType##Functor<A1, A2, A3>::value_type, A1, A2, A3> base_type;                                                                     \
			Functor() {}                                                                                                                                                                   \
			Functor(const A1& op1, const A2& op2, const A3& op3)                                                                                                                           \
			  : base_type(op1, op2, op3)                                                                                                                                                   \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
			template<class Coord>                                                                                                                                                          \
			typename base_type::value_type operator()(const Coord& pos) const                                                                                                              \
			{                                                                                                                                                                              \
				return Function(this->array1(pos), this->array2(pos), this->array3(pos));                                                                                              \
			}                                                                                                                                                                              \
			typename base_type::value_type operator[](qsizetype index) const { return Function(this->array1[index], this->array2[index], this->array3[index]); }                                 \
		};                                                                                                                                                                                     \
		template<class A1, class A2, class A3>                                                                                                                                                 \
		struct Functor<A1, A2, A3, true> : BaseOperator3<NullType, A1, A2, A3>                                                                                                                 \
		{                                                                                                                                                                                      \
			static constexpr bool valid = false;                                                                                                                                           \
			typedef BaseOperator3<NullType, A1, A2, A3> base_type;                                                                                                                         \
			Functor() {}                                                                                                                                                                   \
			Functor(const A1& op1, const A2& op2, const A3& op3)                                                                                                                           \
			  : base_type(op1, op2, op3)                                                                                                                                                   \
			{                                                                                                                                                                              \
			}                                                                                                                                                                              \
		};                                                                                                                                                                                     \
		_REBIND_HELPER3(Functor)                                                                                                                                                               \
		template<class A1, class A2, class A3>                                                                                                                                                 \
		using try_##Functor = typename DeduceOperatorType##Functor<A1, A2, A3>::value_type;                                                                                                    \
		template<class A1, class A2, class A3>                                                                                                                                                 \
		struct is_valid_functor<Functor<A1, A2, A3, false>> : is_valid_op3<A1, A2, A3, try_##Functor>                                                                                          \
		{                                                                                                                                                                                      \
		}; /*, std::void_t<decltype(Function(DeduceArrayType<A1>::value_type(),DeduceArrayType<A2>::value_type()))> > :std::true_type{};*/                                                     \
	}

	//
	// Create bits operators working on any types
	//

	template<class T>
	typename std::enable_if<!std::is_integral<T>::value, T>::type bitwise_and(const T& v1, const T& v2)
	{
		T res;
		const char* p1 = (const char*)&v1;
		const char* p2 = (const char*)&v2;
		char* pr = (char*)&res;
		for (qsizetype i = 0; i < sizeof(T); ++i)
			pr[i] = p1[i] & p2[i];
		return res;
	}
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type bitwise_and(const T& v1, const T& v2)
	{
		return v1 & v2;
	}
	template<class T>
	typename std::enable_if<!std::is_integral<T>::value, T>::type bitwise_or(const T& v1, const T& v2)
	{
		T res;
		const char* p1 = (const char*)&v1;
		const char* p2 = (const char*)&v2;
		char* pr = (char*)&res;
		for (qsizetype i = 0; i < sizeof(T); ++i)
			pr[i] = p1[i] | p2[i];
		return res;
	}
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type bitwise_or(const T& v1, const T& v2)
	{
		return v1 | v2;
	}
	template<class T>
	typename std::enable_if<!std::is_integral<T>::value, T>::type bitwise_xor(const T& v1, const T& v2)
	{
		T res;
		const char* p1 = (const char*)&v1;
		const char* p2 = (const char*)&v2;
		char* pr = (char*)&res;
		for (qsizetype i = 0; i < sizeof(T); ++i)
			pr[i] = p1[i] ^ p2[i];
		return res;
	}
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type bitwise_xor(const T& v1, const T& v2)
	{
		return v1 ^ v2;
	}
	template<class T>
	typename std::enable_if<!std::is_integral<T>::value, T>::type lshift(const T& v1, const T& v2)
	{
		T res;
		const char* p1 = (const char*)&v1;
		const char* p2 = (const char*)&v2;
		char* pr = (char*)&res;
		for (qsizetype i = 0; i < sizeof(T); ++i)
			pr[i] = p1[i] << p2[i];
		return res;
	}
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type lshift(const T& v1, const T& v2)
	{
		return v1 << v2;
	}
	template<class T>
	typename std::enable_if<!std::is_integral<T>::value, T>::type rshift(const T& v1, const T& v2)
	{
		T res;
		const char* p1 = (const char*)&v1;
		const char* p2 = (const char*)&v2;
		char* pr = (char*)&res;
		for (qsizetype i = 0; i < sizeof(T); ++i)
			pr[i] = p1[i] >> p2[i];
		return res;
	}
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type rshift(const T& v1, const T& v2)
	{
		return v1 >> v2;
	}

}

//
// Create operator overloads for VipNDArray
//

_DECLARE_OPERATOR2(Add, +)
_DECLARE_OPERATOR2(Mul, *)
_DECLARE_OPERATOR2(Sub, -)
_DECLARE_OPERATOR2(Div, /)
_DECLARE_OPERATOR2(Rem, %)
_DECLARE_OPERATOR2(And, &&)
_DECLARE_OPERATOR2(Or, ||)
_DECLARE_OPERATOR2(Gr, >)
_DECLARE_OPERATOR2(Lr, <)
_DECLARE_OPERATOR2(GrEq, >=)
_DECLARE_OPERATOR2(LrEq, <=)
_DECLARE_OPERATOR2(Eq, ==)
_DECLARE_OPERATOR2(NotEq, !=)
_DECLARE_OPERATOR2_FUNCTION(AndB, &, bitwise_and)
_DECLARE_OPERATOR2_FUNCTION(OrB, |, bitwise_or)
_DECLARE_OPERATOR2_FUNCTION(Xor, ^, bitwise_xor)

_DECLARE_FUNCTOR_FOR_FUNCTION2(ShiftL, lshift)
template<class A1, class A2>
typename std::enable_if<detail::HasArrayType<A1>::value, detail::ShiftL<A1, A2>>::type operator<<(const A1& a1, const A2& a2)
{
	return detail::ShiftL<A1, A2>(a1, a2);
}
_DECLARE_FUNCTOR_FOR_FUNCTION2(ShiftR, rshift)
template<class A1, class A2>
typename std::enable_if<detail::HasArrayType<A1>::value, detail::ShiftL<A1, A2>>::type operator>>(const A1& a1, const A2& a2)
{
	return detail::ShiftR<A1, A2>(a1, a2);
}

//
// Create the ~ operator for VipNDArray
//
namespace detail
{

	template<class T>
	typename std::enable_if<!std::is_integral<T>::value, T>::type reverse(const T& v1)
	{
		T res;
		const char* p1 = (const char*)&v1;
		char* pr = (char*)&res;
		for (qsizetype i = 0; i < sizeof(T); ++i)
			pr[i] = ~p1[i];
		return res;
	}
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type reverse(const T& v1)
	{
		return ~v1;
	}

	// operator ~
	template<class A1, bool hasNullType = HasNullType<A1>::value>
	struct ReverseBits : BaseOperator1<typename DeduceArrayType<A1>::value_type, A1>
	{
		ReverseBits() {}
		ReverseBits(const A1& op1)
		  : BaseOperator1<typename DeduceArrayType<A1>::value_type, A1>(op1)
		{
		}
		template<class Coord>
		typename DeduceArrayType<A1>::value_type operator()(const Coord& pos) const
		{
			return ~this->array1(pos);
		}
		typename DeduceArrayType<A1>::value_type operator[](qsizetype index) const { return reverse(this->array1[index]); }
	};
	template<class A1>
	struct ReverseBits<A1, true> : BaseOperator1<NullType, A1>
	{
		ReverseBits() {}
		ReverseBits(const A1& op1)
		  : BaseOperator1<NullType, A1>(op1)
		{
		}
	};
	_REBIND_HELPER1(ReverseBits)
	template<class A1>
	using try_ReverseBits = decltype(reverse(typename DeduceArrayType<A1>::value_type()));
	template<class A1>
	struct is_valid_functor<ReverseBits<A1, false>> : is_valid_op1<A1, try_ReverseBits>
	{
	};
}
template<class A1>
typename std::enable_if<detail::HasArrayType<A1>::value, detail::ReverseBits<A1>>::type operator~(const A1& a1)
{
	return detail::ReverseBits<A1>(a1);
}

//
// Create the ! operator for VipNDArray
//
namespace detail
{

	// operator !
	template<class A1, bool hasNullType = HasNullType<A1>::value>
	struct Not : BaseOperator1<typename DeduceArrayType<A1>::value_type, A1>
	{
		Not() {}
		Not(const A1& op1)
		  : BaseOperator1<typename DeduceArrayType<A1>::value_type, A1>(op1)
		{
		}
		template<class Coord>
		typename DeduceArrayType<A1>::value_type operator()(const Coord& pos) const
		{
			return !this->array1(pos);
		}
		typename DeduceArrayType<A1>::value_type operator[](qsizetype index) const { return !(this->array1[index]); }
	};
	template<class A1>
	struct Not<A1, true> : BaseOperator1<NullType, A1>
	{
		Not() {}
		Not(const A1& op1)
		  : BaseOperator1<NullType, A1>(op1)
		{
		}
	};
	_REBIND_HELPER1(Not)
	template<class A1>
	using try_Not = decltype(!(typename DeduceArrayType<A1>::value_type()));
	template<class A1>
	struct is_valid_functor<Not<A1, false>> : is_valid_op1<A1, try_Not>
	{
	};

}
template<class A1>
typename std::enable_if<detail::HasArrayType<A1>::value, detail::Not<A1>>::type operator!(const A1& a1)
{
	return detail::Not<A1>(a1);
}

/////////////////////////////////
// Operators +=, *=, -=, ...
/////////////////////////////////

template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator+=(Array& a1, const T& a2)
{
	return a1 = a1 + a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator-=(Array& a1, const T& a2)
{
	return a1 = a1 - a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator*=(Array& a1, const T& a2)
{
	return a1 = a1 * a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator/=(Array& a1, const T& a2)
{
	return a1 = a1 / a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator%=(Array& a1, const T& a2)
{
	return a1 = a1 % a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator&=(Array& a1, const T& a2)
{
	return a1 = a1 & a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator|=(Array& a1, const T& a2)
{
	return a1 = a1 | a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator<<=(Array& a1, const T& a2)
{
	return a1 = a1 << a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator>>=(Array& a1, const T& a2)
{
	return a1 = a1 >> a2;
}
template<class Array, class T>
typename std::enable_if<detail::HasArrayType<Array>::value, Array>::type& operator^=(Array& a1, const T& a2)
{
	return a1 = a1 ^ a2;
}

namespace detail
{

	//
	// Create the vipCast function
	//

	/// Base Cast implementation, use static_cast
	template<class T, class U, class Enable = void>
	struct Cast
	{
		typedef T type;
		static T cast(const U& u) { return Convert<T, U>::apply(u); }
	};

	/// functor use with functor expressions, uses the Cast structure
	template<class T, class A1, bool hasNullType = HasNullType<A1>::value>
	struct CastOp : public BaseOperator1<T, A1>
	{
		typedef Cast<T, typename BaseOperator1<T, A1>::value_type> cast;
		CastOp() {}
		CastOp(const A1& op1)
		  : BaseOperator1<T, A1>(op1)
		{
		}
		template<class Coord>
		T operator()(const Coord& pos) const
		{
			return cast::cast(this->array1(pos));
		}
		T operator[](qsizetype index) const { return cast::cast(this->array1[index]); }
	};
	template<class T, class A1>
	struct CastOp<T, A1, true> : public BaseOperator1<T, A1>
	{
		CastOp() {}
		CastOp(const A1& op1)
		  : BaseOperator1<T, A1>(op1)
		{
		}
	};
	/// rebind specialization for CastOp, create a CastOp with the same array type but returning a different type
	template<class T, class U, class A1, bool hasNullType>
	struct rebind<T, CastOp<U, A1, hasNullType>, false>
	{
		typedef CastOp<U, A1, hasNullType> op;
		typedef CastOp<T, A1, false> type;
		static type cast(const op& a) { return type(a.array1); }
	};
	template<class T, class A1>
	struct is_valid_functor<CastOp<T, A1>> : std::true_type
	{
	};

	//
	// Specializations for Cast struct
	//

	/// cast to same type
	template<class T>
	struct Cast<T, T>
	{
		typedef const T& type;
		static const T& cast(const T& u) { return u; }
	};
	/// cast VipNDArray : return a VipNDArrayType
	template<class T, class U>
	struct Cast<T, U, typename std::enable_if<IsNDArray<U>::value>::type>
	{
		typedef VipNDArrayType<T> type;
		static type cast(const U& u) { return type(u); }
	};
	/// cast Operator : return a CastOp
	template<class T, class U>
	struct Cast<T, U, typename std::enable_if<IsOperand<U>::value>::type>
	{
		typedef CastOp<T, typename rebind<T, U>::type> type;
		static type cast(const U& u) { return type(rebind<T, U>::cast(u)); }
	};
	/// cast VipNDArrayType : return a CastOp
	template<class T, class U, qsizetype NDims>
	struct Cast<T, VipNDArrayType<U, NDims>>
	{
		typedef CastOp<T, VipNDArrayType<U, NDims>> type;
		static type cast(const VipNDArrayType<U, NDims>& u) { return type(u); }
	};
	/// cast VipNDArrayType to same type : return array
	template<class T, qsizetype NDims>
	struct Cast<T, VipNDArrayType<T, NDims>>
	{
		typedef const VipNDArrayType<T, NDims>& type;
		static type cast(const VipNDArrayType<T, NDims>& u) { return u; }
	};
	/// cast VipNDArrayTypeView : return a CastOp
	template<class T, class U, qsizetype NDims>
	struct Cast<T, VipNDArrayTypeView<U, NDims>>
	{
		typedef CastOp<T, VipNDArrayTypeView<U, NDims>> type;
		static type cast(const VipNDArrayTypeView<U, NDims>& u) { return type(u); }
	};
	/// cast VipNDArrayTypeView to same type: return array
	template<class T, qsizetype NDims>
	struct Cast<T, VipNDArrayTypeView<T, NDims>>
	{
		typedef const VipNDArrayTypeView<T, NDims>& type;
		static type cast(const VipNDArrayTypeView<T, NDims>& u) { return u; }
	};
}

/// Cast input array or value to given type.
/// This function provides a different behavior depending on the input type:
///
/// \code
/// VipNDArrayType<int> ar(vipvector(2,2));
/// auto p0 = vipCast<int>(2);					//return input directly as const reference
/// auto p1 = vipCast<int>(2.0);				//static_cast from double to int
/// auto p2 = vipCast<int>(ar);					//return input directly as const reference
/// auto p3 = vipCast<double>(ar);				//return a functor to cast input integer array as double array.
/// //this functor can be used in functor expressions.
/// auto p4 = vipCast<double>(VipNDArray(ar));  //directly return a VipNDArrayType<double>, which might trigger an allocation
/// auto p5 = vipCast<double>(ar * 2);			//return a functor to cast input functor expression as double array.
/// //this functor can be used in functor expressions.
/// auto p6 = vipCast<double>(VipNDArray(ar) * 2);	//return a functor to cast input functor expression as double array.
/// //this functor can be used in functor expressions.
/// //can trigger an allocation as all VipNDArray within functor expression are
/// //casted to VipNDArraytype<double>.
/// \endcode
template<class T, class U>
typename detail::Cast<T, U>::type vipCast(const U& value)
{
	return detail::Cast<T, U>::cast(value);
}

/// Create a version of the unary function \a fun that works with functor expression.
/// \a Functor is the name of the functor structure created in the detail namespace.
///
/// Note that the standard version of \a fun (working on numerical values) must be defined with
/// SFINAE to avoid declaration conflicts. See the #vipConjugate function as an example.
#define VIP_CREATE_FUNCTION1(Functor, fun)                                                                                                                                                             \
	_DECLARE_FUNCTOR_FOR_FUNCTION1(Functor, fun)                                                                                                                                                   \
	template<class T1>                                                                                                                                                                             \
	typename std::enable_if<!detail::IsNoOperand<T1>::value, detail::Functor<T1>>::type fun(const T1& v1)                                                                                          \
	{                                                                                                                                                                                              \
		return detail::Functor<T1>(v1);                                                                                                                                                        \
	}

/// Create a version of the binary function \a fun that works with functor expression.
/// \a Functor is the name of the functor structure created in the detail namespace.
///
/// Note that the standard version of \a fun (working on numerical values) must be defined with
/// SFINAE to avoid declaration conflicts. See the #vipMin function as an example.
#define VIP_CREATE_FUNCTION2(Functor, fun)                                                                                                                                                             \
	_DECLARE_FUNCTOR_FOR_FUNCTION2(Functor, fun)                                                                                                                                                   \
	template<class T1, class T2>                                                                                                                                                                   \
	typename std::enable_if<!detail::IsNoOperand<T1, T2>::value, detail::Functor<T1, T2>>::type fun(const T1& v1, const T2& v2)                                                                    \
	{                                                                                                                                                                                              \
		return detail::Functor<T1, T2>(v1, v2);                                                                                                                                                \
	}

/// Create a version of the ternary function \a fun that works with functor expression.
/// \a Functor is the name of the functor structure created in the detail namespace.
///
/// Note that the standard version of \a fun (working on numerical values) must be defined with
/// SFINAE to avoid declaration conflicts. See the #vipClamp function as an example.
#define VIP_CREATE_FUNCTION3(Functor, fun)                                                                                                                                                             \
	_DECLARE_FUNCTOR_FOR_FUNCTION3(Functor, fun)                                                                                                                                                   \
	template<class T1, class T2, class T3>                                                                                                                                                         \
	typename std::enable_if<!detail::IsNoOperand<T1, T2, T3>::value, detail::Functor<T1, T2, T3>>::type fun(const T1& v1, const T2& v2, const T3& v3)                                              \
	{                                                                                                                                                                                              \
		return detail::Functor<T1, T2, T3>(v1, v2, v3);                                                                                                                                        \
	}

/// \fn vipMin
/// Returns the minimum of 2 expressions. This function works with VipNDArray and functor expressions.
/// Examples:
/// \code
/// VipNDArrayType<int> ar(vipvector(2,2));
/// auto p0 = vipMin(VipNDArray(ar) , 2);		//return a functor expression to evaluate minimum between array and integer
/// auto p1 = vipMin(ar, 2);					//return a functor expression to evaluate minimum between VipNDArrayType and integer
/// auto p2 = vipMin(3, 4.1);					//return 3.0
/// \endcode
template<class T1, class T2>
typename std::enable_if<detail::IsNoOperand<T1, T2>::value, decltype(T1() + T2())>::type vipMin(const T1& v1, const T2& v2)
{
	return v1 < v2 ? v1 : v2;
}
VIP_CREATE_FUNCTION2(MinFun, vipMin)

/// \fn vipMax
/// Returns the maximum of 2 expressions. This function works with VipNDArray and functor expressions.
/// Examples:
/// \code
/// VipNDArrayType<int> ar(vipvector(2,2));
/// auto p0 = vipMax(VipNDArray(ar) , 2);		//return a functor expression to evaluate maximum between array and integer
/// auto p1 = vipMax(ar, 2);					//return a functor expression to evaluate maximum between VipNDArrayType and integer
/// auto p2 = vipMax(3, 4.1);					//return 3.0
/// \endcode
template<class T1, class T2>
typename std::enable_if<detail::IsNoOperand<T1, T2>::value, decltype(T1() + T2())>::type vipMax(const T1& v1, const T2& v2)
{
	return v1 > v2 ? v1 : v2;
}
VIP_CREATE_FUNCTION2(MaxFun, vipMax)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type vipReal(const T& v)
{
	return v;
}
template<class T>
T vipReal(const std::complex<T>& c)
{
	return c.real();
}
VIP_CREATE_FUNCTION1(RealFun, vipReal)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type vipImag(const T& v)
{
	return (T)0;
}
template<class T>
T vipImag(const std::complex<T>& c)
{
	return c.imag();
}
VIP_CREATE_FUNCTION1(ImagFun, vipImag)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type vipArg(const T& v)
{
	return 0;
}
template<class T>
T vipArg(const std::complex<T>& c)
{
	return std::arg(c);
}
VIP_CREATE_FUNCTION1(ArgFun, vipArg)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type vipNorm(const T& v)
{
	return std::norm(std::complex<T>(v));
}
template<class T>
T vipNorm(const std::complex<T>& c)
{
	return std::norm(c);
}
VIP_CREATE_FUNCTION1(NormFun, vipNorm)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type vipConjugate(const T& v)
{
	return v;
}
template<class T>
T vipConjugate(const std::complex<T>& c)
{
	return std::complex<T>(c.real(), -c.imag());
}
VIP_CREATE_FUNCTION1(ConjugateFun, vipConjugate)

VIP_CREATE_FUNCTION1(AbsFun, vipAbs)
VIP_CREATE_FUNCTION1(CeilFun, vipCeil)
VIP_CREATE_FUNCTION1(FloorFun, vipFloor)
VIP_CREATE_FUNCTION1(RoundFun, vipRound)

template<class T, class MA, class MI>
typename std::enable_if<detail::IsNoOperand<T, MA, MI>::value, T>::type vipClamp(const T& v, const MI& min, const MA& max)
{
	return v < min ? (T)min : (v > max ? (T)max : v);
}
VIP_CREATE_FUNCTION3(ClampFun, vipClamp)

VIP_CREATE_FUNCTION1(IsNanFun, vipIsNan)
VIP_CREATE_FUNCTION1(IsInfFun, vipIsInf)

template<class T1, class T2>
typename std::enable_if<detail::IsNoOperand<T1, T2>::value, T1>::type vipReplaceNan(const T1& v, const T2& value)
{
	return vipIsNan(v) ? static_cast<T1>(value) : v;
}
VIP_CREATE_FUNCTION2(ReplaceNanFun, vipReplaceNan)

template<class T1, class T2>
typename std::enable_if<detail::IsNoOperand<T1, T2>::value, T1>::type vipReplaceInf(const T1& v, const T2& value)
{
	return vipIsInf(v) ? static_cast<T1>(value) : v;
}
VIP_CREATE_FUNCTION2(ReplaceInfFun, vipReplaceInf)

template<class T1, class T2>
typename std::enable_if<detail::IsNoOperand<T1, T2>::value, T1>::type vipReplaceNanInf(const T1& v, const T2& value)
{
	return (vipIsNan(v) || vipIsInf(v)) ? static_cast<T1>(value) : v;
}
VIP_CREATE_FUNCTION2(ReplaceNanInfFun, vipReplaceNanInf)

template<class Cond, class T1, class T2>
typename std::enable_if<detail::IsNoOperand<Cond, T1, T2>::value, T1>::type vipWhere(const Cond& condition, const T1& v1, const T2& v2)
{
	return condition ? v1 : static_cast<T2>(v2);
}
// Do NOT use VIP_CREATE_FUNCTION3 for vipWhere as it will always compute both values for a given condition.
// This must be avoided as one of the branch can be very costly.
// VIP_CREATE_FUNCTION3(WhereFun, vipWhere)
namespace detail
{

	template<class A1, class A2, class A3, bool hasNullType = HasNullType<A1, A2, A3>::value>
	struct WhereFun
	  : BaseOperator3<decltype(vipWhere(typename DeduceArrayType<A1>::value_type(), typename DeduceArrayType<A2>::value_type(), typename DeduceArrayType<A3>::value_type())), A1, A2, A3>
	{
		typedef BaseOperator3<decltype(vipWhere(typename DeduceArrayType<A1>::value_type(), typename DeduceArrayType<A2>::value_type(), typename DeduceArrayType<A3>::value_type())),
				      A1,
				      A2,
				      A3>
		  base_type;
		WhereFun() {}
		WhereFun(const A1& op1, const A2& op2, const A3& op3)
		  : base_type(op1, op2, op3)
		{
		}
		template<class Coord>
		typename base_type::value_type operator()(const Coord& pos) const
		{
			return (this->array1(pos) ? this->array2(pos) : this->array3(pos));
		}
		typename base_type::value_type operator[](qsizetype index) const { return this->array1[index] ? this->array2[index] : this->array3[index]; }
	};
	template<class A1, class A2, class A3>
	struct WhereFun<A1, A2, A3, true> : BaseOperator3<NullType, A1, A2, A3>
	{
		typedef BaseOperator3<NullType, A1, A2, A3> base_type;
		WhereFun() {}
		WhereFun(const A1& op1, const A2& op2, const A3& op3)
		  : base_type(op1, op2, op3)
		{
		}
	};
	_REBIND_HELPER3(WhereFun)
	template<class A1, class A2, class A3>
	using try_WhereFun = decltype(vipWhere(typename DeduceArrayType<A1>::value_type(), typename DeduceArrayType<A2>::value_type(), typename DeduceArrayType<A3>::value_type()));
	template<class A1, class A2, class A3>
	struct is_valid_functor<WhereFun<A1, A2, A3, false>> : is_valid_op3<A1, A2, A3, try_WhereFun>
	{
	}; //, std::void_t<decltype(vipWhere(DeduceArrayType<A1>::value_type(),DeduceArrayType<A2>::value_type()))> > :std::true_type{};
}
template<class T1, class T2, class T3>
typename std::enable_if<!detail::IsNoOperand<T1, T2, T3>::value, detail::WhereFun<T1, T2, T3>>::type vipWhere(const T1& v1, const T2& v2, const T3& v3)
{

	return detail::WhereFun<T1, T2, T3>(v1, v2, v3);
}

VIP_CREATE_FUNCTION2(FuzzyCompareFun, vipFuzzyCompare)
VIP_CREATE_FUNCTION2(FuzzyIsNullFun, vipFuzzyIsNull)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::complex<T>>::type vipSetReal(const std::complex<T>& c, const T& real)
{
	return std::complex<T>(real, c.imag());
}
VIP_CREATE_FUNCTION2(SetRealFun, vipSetReal)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::complex<T>>::type vipSetImag(const std::complex<T>& c, const T& imag)
{
	return std::complex<T>(c.real(), imag);
}
VIP_CREATE_FUNCTION2(SetImagFun, vipSetImag)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::complex<T>>::type vipSetArg(const std::complex<T>& c, double arg)
{
	return std::polar(std::norm(c), arg);
}
VIP_CREATE_FUNCTION2(SetArgFun, vipSetArg)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::complex<T>>::type vipSetMagnitude(const std::complex<T>& c, double mag)
{
	return std::polar(mag, std::arg(c));
}
VIP_CREATE_FUNCTION2(SetMagnitudeFun, vipSetMagnitude)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::complex<T>>::type vipMakeComplex(const T& real, const T& imag)
{
	return std::complex<T>(real, imag);
}
VIP_CREATE_FUNCTION2(MakeComplexFun, vipMakeComplex)

inline complex_d vipMakeComplexd(double real, double imag)
{
	return complex_d(real, imag);
}
VIP_CREATE_FUNCTION2(MakeComplexdFun, vipMakeComplexd)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::complex<T>>::type vipMakeComplexPolar(const T& mag, const T& phase)
{
	return std::polar(mag, phase);
}
VIP_CREATE_FUNCTION2(MakeComplexPolarFun, vipMakeComplexPolar)

inline complex_d vipMakeComplexPolard(double mag, double phase)
{
	return std::polar(mag, phase);
}
VIP_CREATE_FUNCTION2(MakeComplexPolardFun, vipMakeComplexPolard)

#include "VipRgb.h"

template<class T>
inline T vipRed(const VipRgb<T>& rgb)
{
	return rgb.r;
}
VIP_CREATE_FUNCTION1(RedFun, vipRed)

template<class T>
inline T vipGreen(const VipRgb<T>& rgb)
{
	return rgb.g;
}
VIP_CREATE_FUNCTION1(GreenFun, vipGreen)

template<class T>
inline T vipBlue(const VipRgb<T>& rgb)
{
	return rgb.b;
}
VIP_CREATE_FUNCTION1(BlueFun, vipBlue)

template<class T>
inline T vipAlpha(const VipRgb<T>& rgb)
{
	return rgb.a;
}
VIP_CREATE_FUNCTION1(AlphaFun, vipAlpha)

template<class T>
inline VipRgb<T> vipSetRed(const VipRgb<T>& rgb, T r)
{
	return VipRgb<T>(r, rgb.g, rgb.b, rgb.a);
}
VIP_CREATE_FUNCTION2(SetRedFun, vipSetRed)

template<class T>
inline VipRgb<T> vipSetGreen(const VipRgb<T>& rgb, T g)
{
	return VipRgb<T>(rgb.r, g, rgb.b, rgb.a);
}
VIP_CREATE_FUNCTION2(SetGreenFun, vipSetGreen)

template<class T>
inline VipRgb<T> vipSetBlue(const VipRgb<T>& rgb, T b)
{
	return VipRgb<T>(rgb.r, rgb.g, b, rgb.a);
}
VIP_CREATE_FUNCTION2(SetBlueFun, vipSetBlue)

template<class T>
inline VipRgb<T> vipSetAlpha(const VipRgb<T>& rgb, T a)
{
	return VipRgb<T>(rgb.r, rgb.g, rgb.b, a);
}
VIP_CREATE_FUNCTION2(SetAlphaFun, vipSetAlpha)

template<class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, VipRgb<T>>::type vipMakeRgb(T r, T g, T b)
{
	return VipRgb<T>(r, g, b);
}
VIP_CREATE_FUNCTION3(MakeRgbFun, vipMakeRgb)

inline VipRGB vipMakeRGB(quint8 r, quint8 g, quint8 b)
{
	return VipRGB(r, g, b);
}
VIP_CREATE_FUNCTION3(MakeRGBFun, vipMakeRGB)

VIP_CREATE_FUNCTION1(SignFun, vipSign)

namespace detail
{
	template<class T, class = void>
	struct modf_type
	{
		typedef T type;
	};
	template<class T>
	struct modf_type<T, typename std::enable_if<std::is_integral<T>::value, void>::type>
	{
		typedef double type;
	};
}
#define _DOUBLE typename detail::modf_type<T>::type

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipCos(const T& v)
{
	return std::cos(v);
}
VIP_CREATE_FUNCTION1(CosFun, vipCos)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipSin(const T& v)
{
	return std::sin(v);
}
VIP_CREATE_FUNCTION1(SinFun, vipSin)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipTan(const T& v)
{
	return std::tan(v);
}
VIP_CREATE_FUNCTION1(TanFun, vipTan)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipACos(const T& v)
{
	return std::acos(v);
}
VIP_CREATE_FUNCTION1(ACosFun, vipACos)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipASin(const T& v)
{
	return std::asin(v);
}
VIP_CREATE_FUNCTION1(ASinFun, vipASin)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipATan(const T& v)
{
	return std::atan(v);
}
VIP_CREATE_FUNCTION1(ATanFun, vipATan)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipATan2(const T& v1, const T& v2)
{
	return std::atan2(v1, v2);
}
VIP_CREATE_FUNCTION2(ATan2Fun, vipATan2)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipCosh(const T& v)
{
	return std::cosh(v);
}
VIP_CREATE_FUNCTION1(CoshFun, vipCosh)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipSinh(const T& v)
{
	return std::sinh(v);
}
VIP_CREATE_FUNCTION1(SinhFun, vipSinh)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipTanh(const T& v)
{
	return std::tanh(v);
}
VIP_CREATE_FUNCTION1(TanhFun, vipTanh)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipACosh(const T& v)
{
	return std::acosh(v);
}
VIP_CREATE_FUNCTION1(ACoshFun, vipACosh)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipASinh(const T& v)
{
	return std::asinh(v);
}
VIP_CREATE_FUNCTION1(ASinhFun, vipASinh)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipATanh(const T& v)
{
	return std::atanh(v);
}
VIP_CREATE_FUNCTION1(ATanhFun, vipATanh)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipExp(const T& v)
{
	return std::exp(v);
}
VIP_CREATE_FUNCTION1(ExpFun, vipExp)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipSignificand(const T& v)
{
	int exp;
	return std::frexp(v, &exp);
}
VIP_CREATE_FUNCTION1(SignificandFun, vipSignificand)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, int>::type vipExponent(const T& v)
{
	int exp;
	std::frexp(v, &exp);
	return exp;
}
VIP_CREATE_FUNCTION1(ExponentFun, vipExponent)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLdexp(const T& v, int exp)
{
	return std::ldexp(v, exp);
}
VIP_CREATE_FUNCTION2(LdexpFun, vipLdexp)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLog(const T& v)
{
	return std::log(v);
}
VIP_CREATE_FUNCTION1(LogFun, vipLog)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLog10(const T& v)
{
	return std::log10(v);
}
VIP_CREATE_FUNCTION1(Log10Fun, vipLog10)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipFractionalPart(const T& v)
{
	typename detail::modf_type<T>::type intpart;
	return std::modf(v, &intpart);
}
VIP_CREATE_FUNCTION1(FractionalPartFun, vipFractionalPart)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipIntegralPart(const T& v)
{
	typename detail::modf_type<T>::type intpart;
	std::modf(v, &intpart);
	return intpart;
}
VIP_CREATE_FUNCTION1(IntegralPartFun, vipIntegralPart)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipExp2(const T& v)
{
	return std::exp2(v);
}
VIP_CREATE_FUNCTION1(Exp2Fun, vipExp2)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipExpm1(const T& v)
{
	return std::expm1(v);
}
VIP_CREATE_FUNCTION1(Expm1Fun, vipExpm1)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, int>::type vipIlogb(const T& v)
{
	return std::ilogb(v);
}
VIP_CREATE_FUNCTION1(IlogbFun, vipIlogb)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLog1p(const T& v)
{
	return std::log1p(v);
}
VIP_CREATE_FUNCTION1(Log1pFun, vipLog1p)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLog2(const T& v)
{
	return std::log2(v);
}
VIP_CREATE_FUNCTION1(Log2Fun, vipLog2)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLogb(const T& v)
{
	return std::logb(v);
}
VIP_CREATE_FUNCTION1(LogbFun, vipLogb)

template<class T, class T2>
typename std::enable_if<std::is_arithmetic<T>::value && std::is_arithmetic<T2>::value, _DOUBLE>::type vipPow(const T& v, const T2& p)
{
	return std::pow(v, p);
}
VIP_CREATE_FUNCTION2(PowFun, vipPow)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipSqrt(const T& v)
{
	return std::sqrt(v);
}
VIP_CREATE_FUNCTION1(SqrtFun, vipSqrt)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipCbrt(const T& v)
{
	return std::cbrt(v);
}
VIP_CREATE_FUNCTION1(CbrtFun, vipCbrt)

template<class T, class T2>
typename std::enable_if<std::is_arithmetic<T>::value && std::is_arithmetic<T2>::value, _DOUBLE>::type vipHypot(const T& v, const T2& p)
{
	return std::hypot(v, p);
}
VIP_CREATE_FUNCTION1(HypotFun, vipHypot)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipErf(const T& v)
{
	return std::erf(v);
}
VIP_CREATE_FUNCTION1(ErfFun, vipErf)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipErfc(const T& v)
{
	return std::erfc(v);
}
VIP_CREATE_FUNCTION1(ErfcFun, vipErfc)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipTGamma(const T& v)
{
	return std::tgamma(v);
}
VIP_CREATE_FUNCTION1(TGammaFun, vipTGamma)

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, _DOUBLE>::type vipLGamma(const T& v)
{
	return std::lgamma(v);
}
VIP_CREATE_FUNCTION1(LGammaFun, vipLGamma)

#endif
