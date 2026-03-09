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
		return (uint)((std::intptr_t)c.source.data() ^ c.dest_type);
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

	/// Constant operand, simply wrap a constant value
	template<class T>
	struct ConstValue : BaseExpression
	{
		template<class U>
		static T import(const U& v)
		{
			if constexpr (std::is_same_v<QVariant, U>)
				return v.value<T>();
			else
				return static_cast<T>(v);
		}

		typedef T value_type;
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		const T value;
		ConstValue() = default;
		ConstValue(const ConstValue& other) = default;
		ConstValue(ConstValue&& other) noexcept = default;
		ConstValue(const T& value)
		  : value(value)
		{
		}
		template<class U>
		ConstValue(const ConstValue<U>& other)
		  : value(import(other.value))
		{
		}
		template<class U>
		ConstValue(const U& other)
		  : value(import(other))
		{
		}
		int dataType() const noexcept { return qMetaTypeId<T>(); }
		bool isEmpty() const noexcept { return false; }
		bool isUnstrided() const noexcept { return true; }
		const VipNDArrayShape& shape() const noexcept
		{
			static const VipNDArrayShape null_shape;
			return null_shape;
		}
		template<class ShapeType>
		const value_type& operator()(const ShapeType&) const noexcept
		{
			return value;
		}
		const value_type& operator[](qsizetype) const noexcept { return value; }
	};

	/// Specialization for QVariant
	template<>
	struct ConstValue<QVariant> : BaseExpression
	{
		typedef NullType value_type;
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		const QVariant value;
		ConstValue() = default;
		ConstValue(const ConstValue& other) = default;
		ConstValue(ConstValue&& other) noexcept = default;
		template<class T>
		ConstValue(const T& value)
		  : value(QVariant::fromValue(value))
		{
		}
		int dataType() const noexcept { return value.userType(); }
		bool isEmpty() const noexcept { return value.isNull(); }
		bool isUnstrided() const noexcept { return true; }
		const VipNDArrayShape& shape() const noexcept
		{
			static const VipNDArrayShape null_shape;
			return null_shape;
		}
		template<class ShapeType>
		const QVariant& operator()(const ShapeType&) const noexcept
		{
			return value;
		}
		const QVariant& operator[](qsizetype) const noexcept { return value; }
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

	/// Wrap a VipNDArrayType or VipNDArrayTypeView
	template<class Array>
	struct ArrayWrapper : BaseExpression
	{
		using value_type = typename Array::value_type;
		static constexpr auto access_type = Array::access_type;

		const Array array;
		const value_type* ptr;
		ArrayWrapper(const Array& ar) noexcept
		  : array(ar)
		  , ptr(ar.ptr())
		{
		}
		int dataType() const noexcept { return array.dataType(); }
		bool isEmpty() const noexcept { return array.isEmpty(); }
		bool isUnstrided() const noexcept { return array.isUnstrided(); }
		const VipNDArrayShape& shape() const noexcept { return GetShape(array); }
		template<class ShapeType>
		const value_type& operator()(const ShapeType& pos) const noexcept
		{
			return array(pos);
		}
		const value_type& operator[](qsizetype i) const noexcept { return ptr[i]; }
	};
	/// Specialization for VipNDArrayType
	template<class T, qsizetype NDims>
	struct ArrayWrapper<VipNDArrayType<T, NDims>> : BaseExpression
	{
		typedef T value_type;
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;

		const VipNDArrayType<T, NDims> array;
		const T* ptr = nullptr;

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
		ArrayWrapper(const ArrayWrapper& other) noexcept
		  : array(other.array)
		  , ptr(other.ptr)
		{
		}
		ArrayWrapper(const VipNDArrayType<T, NDims>& array) noexcept
		  : array(array)
		  , ptr(array.ptr())
		{
		}
		template<qsizetype ONDims>
		ArrayWrapper(const VipNDArrayType<T, ONDims>& array)
		  : array(array)
		  , ptr(array.ptr())
		{
		}
		int dataType() const noexcept { return array.dataType(); }
		bool isEmpty() const noexcept { return array.isEmpty(); }
		bool isUnstrided() const noexcept { return true; }
		const VipNDArrayShape& shape() const noexcept { return GetShape(array); }
		template<class ShapeType>
		const value_type& operator()(const ShapeType& pos) const noexcept
		{
			return ptr[vipFlatOffset<true>(array.strides(), pos)];
		}
		const value_type& operator[](qsizetype i) const noexcept { return ptr[i]; }
	};

	// rebind U to T
	template<class T, class U, bool isArray = std::is_base_of<VipNDArray, U>::value>
	struct rebind
	{
		static const auto& cast(const U& a) { return a; }
	};
	// rebind QVariant
	template<class T>
	struct rebind<T, QVariant, false>
	{
		static auto cast(const QVariant& a) { return a.value<T>(); }
	};
	// rebind VipNDArray inheriting class
	template<class T, class U>
	struct rebind<T, U, true>
	{
		static auto cast(const U& a) { return ArrayWrapper<VipNDArrayType<T>>(a); }
	};
	// rebind VipNDArrayType class (does not change the type)
	template<class T, class U, qsizetype NDims>
	struct rebind<T, VipNDArrayType<U, NDims>, true>
	{
		static auto cast(const VipNDArrayType<U, NDims>& a) { return ArrayWrapper<VipNDArrayType<U, NDims>>(a); }
	};
	// rebind VipNDArrayTypeView class (does not change the type)
	template<class T, class U, qsizetype NDims>
	struct rebind<T, VipNDArrayTypeView<U, NDims>, true>
	{
		static const auto& cast(const VipNDArrayTypeView<U, NDims>& a) { return a; }
	};
	// rebind ConstValue (does not change the type)
	template<class T, class U>
	struct rebind<T, ConstValue<U>, false>
	{
		static const auto& cast(const ConstValue<U>& a) { return a; }
	};
	// rebind ConstValue<QVariant>
	template<class T>
	struct rebind<T, ConstValue<QVariant>, false>
	{
		static auto cast(const ConstValue<QVariant>& a) { return ConstValue<T>(a); }
	};
	// rebind ArrayWrapper
	template<class T, class U>
	struct rebind<T, ArrayWrapper<U>, true>
	{
		static auto cast(const ArrayWrapper<U>& a) { return ArrayWrapper<typename rebind<T, U>::type>(a.array); }
	};
	template<class T>
	struct rebind<T, ArrayWrapper<T>, true>
	{
		static const ArrayWrapper<T>& cast(const ArrayWrapper<T>& a) { return a; }
	};

	template<class T, class U>
	auto rebindExpression(const U& v)
	{
		return rebind<T, U>::cast(v);
	}

	// Check if type inherits NullOperand (nd array or functor expression)
	template<class T, class = void>
	struct IsArrayType : std::false_type
	{
	};
	template<class T>
	struct IsArrayType<T, std::void_t<std::bool_constant<sizeof(T) == sizeof(T)>>> : std::is_base_of<NullOperand, T>
	{
	};
	template<class T>
	constexpr bool IsArrayType_v = IsArrayType<T>::value;

	// Check if type inherits VipArrayBase (nd array object)
	template<class T, class = void>
	struct IsNDArrayType : std::false_type
	{
	};
	template<class T>
	struct IsNDArrayType<T, std::void_t<std::bool_constant<sizeof(T) == sizeof(T)>>> : std::is_base_of<VipArrayBase, T>
	{
	};
	template<class T>
	constexpr bool IsNDArrayType_v = IsNDArrayType<T>::value;

	/// Deduce array type of Array with typedef array_type, and the value type with typedef value_type
	template<class Array, bool IsArray = IsArrayType<Array>::value>
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
	template<class T>
	struct DeduceArrayType<T, false>
	{
		typedef ConstValue<T> array_type;
		typedef T value_type;
	};
	template<>
	struct DeduceArrayType<QVariant, false>
	{
		typedef ConstValue<QVariant> array_type;
		typedef NullType value_type;
	};

	template<class T>
	using DeduceArrayType_t = typename DeduceArrayType<T>::array_type;

	/// Check if given arrays and/or constants define NullType (at least one of them is a VipNDArray)
	template<class... Array>
	struct HasNullType
	{
		// Fold expression
		static constexpr bool value = (... || std::is_same<typename DeduceArrayType<Array>::value_type, NullType>::value);
	};
	template<class... Array>
	struct HasNullType<std::tuple<Array...>>
	{
		// Fold expression
		static constexpr bool value = (... || std::is_same<typename DeduceArrayType<Array>::value_type, NullType>::value);
	};

	/// Check if at least one template argument is a NullOperand (nd array or functor expression)
	template<class... Array>
	struct HasArrayType
	{
		// Fold expression
		static constexpr bool value = (... || IsArrayType_v<Array>);
	};
	/// Check if at least one template argument is a NullOperand (nd array or functor expression)
	template<class... Array>
	struct HasArrayType<std::tuple<Array...>>
	{
		// Fold expression
		static constexpr bool value = (... || IsArrayType_v<Array>);
	};

	template<class... Array>
	constexpr bool HasArrayType_v = HasArrayType<Array...>::value;

	/// @brief Check that all types ar either array or arithmetic
	template<class... T>
	struct IsArrayOrArithmetic
	{
		static constexpr bool value = (... && (std::is_arithmetic_v<T> || detail::HasArrayType_v<T>) );
	};
	template<class... Array>
	constexpr bool IsArrayOrArithmetic_v = IsArrayOrArithmetic<Array...>::value;



	/// Check if type inherits VipNDArray, but is not typed (like VipNDArrayType and VipNDArrayTypeView)
	template<class T>
	struct IsNDArray
	{
		static const bool value = std::is_base_of<VipNDArray, T>::value && HasNullType<T>::value;
	};

	struct FunctorBase
	{
	};

	/// Value type of nd array or functor expression
	template<class F, bool IsFunctorBase = std::is_base_of_v<FunctorBase, F>>
	struct ValueType
	{
		using type = typename F::value_type;
	};

	template<bool hasNullType, class F>
	constexpr auto evalType(const F& f)
	{
		if constexpr (hasNullType)
			return NullType{};
		else
			return f.operator()(vipVector(0));
	}

	template<class F>
	struct ValueType<F, true>
	{
		using arrays = typename F::array_types;
		using type = decltype(evalType<HasNullType<arrays>::value, F>(std::declval<F&>()));
	};

	template<class F>
	using ValueType_t = typename ValueType<F>::type;

	/// @brief Base class for most functor expression classes
	template<class Derived, class... Array>
	class BaseFunctor
	  : public BaseExpression
	  , FunctorBase
	{
	protected:
		template<class Sh>
		void build_shape(const Sh& shape, bool& valid)
		{
			if (valid) {
				if (sh.isEmpty())
					sh = shape;
				else if (!shape.isEmpty())
					valid = (shape == sh);
			}
		}
		void build_type(int dtype)
		{
			if (type == 0)
				type = dtype;
			else if (dtype)
				type = vipHigherArrayType(type, dtype);
		}
		VipNDArrayShape sh;
		int type = 0;

	public:
		using array_types = std::tuple<typename DeduceArrayType<Array>::array_type...>;
		static constexpr qsizetype access_type = (DeduceArrayType<Array>::array_type::access_type & ...);

		const array_types arrays;

		int dataType() const noexcept { return type; }
		bool isEmpty() const noexcept
		{
			return std::apply([](auto&... args) { return (args.isEmpty() || ...); }, arrays);
		}
		bool isUnstrided() const
		{
			return std::apply([](auto&... args) { return (args.isUnstrided() && ...); }, arrays);
		}
		const VipNDArrayShape& shape() const { return sh; }
		BaseFunctor() {}
		template<class... Ar>
		BaseFunctor(const Ar&... ar)
		  : arrays(std::forward<const Ar&>(ar)...)
		{
			bool valid = true;
			std::apply([&](auto&... args) { (build_shape(GetShape(args), valid), ...); }, arrays);

			if constexpr (!HasNullType<Array...>::value)
				type = qMetaTypeId<ValueType_t<Derived>>();
			else
				std::apply([&](auto&... args) { (build_type(args.dataType()), ...); }, arrays);
		}
		// Define dummy access operator to avoid compilation error
		template<class ShapeType>
		NullType operator()(const ShapeType&) const
		{
			return NullType();
		}
		template<class Coord>
		NullType operator[](Coord) const
		{
			return NullType();
		}
	};

	/// @brief Functor expression mapping a function object
	template<class Functor, class... Array>
	struct GenericFunction : BaseFunctor<GenericFunction<Functor, Array...>, Array...>
	{
		using base = BaseFunctor<GenericFunction<Functor, Array...>, Array...>;

		Functor functor;
		GenericFunction() {}
		template<class F, class... Ar>
		GenericFunction(F&& f, Ar&&... args)
		  : base(std::forward<Ar>(args)...)
		  , functor(std::forward<F>(f))
		{
		}
		template<class Coord>
		auto operator()(const Coord& index) const
		{
			return std::apply([&](auto... args) { return functor((args(index))...); }, this->arrays);
		}
		template<class Coord>
		auto operator[](Coord index) const
		{
			return std::apply([&](auto... args) { return functor(args[index]...); }, this->arrays);
		}
	};

	template<class U, class Functor, class... Array>
	auto rebindExpression(const GenericFunction<Functor, Array...>& f)
	{
		using functor = GenericFunction<Functor, Array...>;
		using type = GenericFunction<Functor, decltype(rebindExpression<U>(std::declval<Array&>()))...>;

		if constexpr (std::is_invocable_v<Functor, ValueType_t<decltype(rebindExpression<U>(std::declval<Array&>()))>...>)
			return std::apply([&](auto... args) { return type(f.functor, rebindExpression<U>(args)...); }, f.arrays);
		else
			return NullType{};
	}

}

/// @brief Create a functor expression that applies a function object.
/// Used to create function expression from any function.
template<class Functor, class... Array>
auto vipFunction(const Functor& fun, Array... args)
{
	return detail::GenericFunction<Functor, detail::DeduceArrayType_t<Array>...>(fun, std::forward<Array>(args)...);
}

//
// Create operator overloads for VipNDArray
//

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator+(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l + r)>* = nullptr) { return l + r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator-(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l - r)>* = nullptr) { return l - r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator*(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l * r)>* = nullptr) { return l * r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator/(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l / r)>* = nullptr) { return l / r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator%(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l % r)>* = nullptr) { return l % r; }, l, r);
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator<(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l < r)>* = nullptr) { return l < r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator>(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l > r)>* = nullptr) { return l > r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator<=(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l <= r)>* = nullptr) { return l <= r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator>=(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l >= r)>* = nullptr) { return l >= r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator==(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l == r)>* = nullptr) { return l == r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator!=(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l != r)>* = nullptr) { return l != r; }, l, r);
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator&(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l & r)>* = nullptr) { return l & r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator|(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l | r)>* = nullptr) { return l | r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator^(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l ^ r)>* = nullptr) { return l ^ r; }, l, r);
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator<<(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l << r)>* = nullptr) { return l << r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator>>(const A1& l, const A2& r)
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l >> r)>* = nullptr) { return l >> r; }, l, r);
}

template<class A1, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
auto operator~(const A1& l)
{
	return vipFunction([](auto l, std::void_t<decltype(~l)>* = nullptr) { return ~l; }, l);
}

template<class A1, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
auto operator!(const A1& l)
{
	return vipFunction([](auto l, std::void_t<decltype(!l)>* = nullptr) { return !l; }, l);
}

/////////////////////////////////
// Operators +=, *=, -=, ...
/////////////////////////////////

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator+=(A1& l, const A2& r)
{
	return l = l + r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator-=(A1& l, const A2& r)
{
	return l = l - r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator*=(A1& l, const A2& r)
{
	return l = l * r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator/=(A1& l, const A2& r)
{
	return l = l / r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator%=(A1& l, const A2& r)
{
	return l = l % r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator&=(A1& l, const A2& r)
{
	return l = l & r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator|=(A1& l, const A2& r)
{
	return l = l | r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator^=(A1& l, const A2& r)
{
	return l = l ^ r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator<<=(A1& l, const A2& r)
{
	return l = l << r;
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
A1& operator>>=(A1& l, const A2& r)
{
	return l = l >> r;
}

/// @brief Tells if convertion using vipCast() is valid
template<class From, class To>
struct VipIsCastable : std::bool_constant<detail::Convert<To, From>::valid>
{
};
template<class From, class To>
constexpr bool VipIsCastable_v = VipIsCastable<From, To>::value;

/// @brief Cast input array or value to given type.
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
auto vipCast(const U& value)
{
	if constexpr (detail::HasArrayType_v<U>)
		return vipFunction(
		  [](const auto& a, std::enable_if_t<VipIsCastable_v<std::decay_t<decltype(a)>, T>, void>* = nullptr) { return detail::Convert<T, std::decay_t<decltype(a)>>::apply(a); }, value);
	else
		return detail::Convert<T, U>::apply(value);
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
template<class A1, class A2>
auto vipMin(const A1& a1, const A2& a2)
{
	if constexpr (detail::HasArrayType_v<A1, A2>)
		return vipFunction([](auto l, auto r, std::void_t<decltype(l < r)>* = nullptr) { return l < r ? l : r; }, a1, a2);
	else
		return a1 < a2 ? a1 : a2;
}

/// \fn vipMax
/// Returns the maximum of 2 expressions. This function works with VipNDArray and functor expressions.
/// Examples:
/// \code
/// VipNDArrayType<int> ar(vipvector(2,2));
/// auto p0 = vipMax(VipNDArray(ar) , 2);		//return a functor expression to evaluate maximum between array and integer
/// auto p1 = vipMax(ar, 2);					//return a functor expression to evaluate maximum between VipNDArrayType and integer
/// auto p2 = vipMax(3, 4.1);					//return 3.0
/// \endcode
template<class A1, class A2>
auto vipMax(const A1& a1, const A2& a2)
{
	if constexpr (detail::HasArrayType_v<A1, A2>)
		return vipFunction([](auto l, auto r, std::void_t<decltype(l > r)>* = nullptr) { return l > r ? l : r; }, a1, a2);
	else
		return a1 > a2 ? a1 : a2;
}

template<class T>
struct VipIsComplex : std::false_type
{
};
template<class T>
struct VipIsComplex<std::complex<T>> : std::true_type
{
};
template<class T>
constexpr bool VipIsComplex_v = VipIsComplex<T>::value;

template<class T>
struct VipIsRgb : std::false_type
{
};
template<class T>
struct VipIsRgb<VipRgb<T>> : std::true_type
{
};
template<class T>
constexpr bool VipIsRgb_v = VipIsRgb<T>::value;

template<class T, std::enable_if_t<std::is_arithmetic_v<T> || VipIsComplex<T>::value || detail::HasArrayType_v<T>, int> = 0>
auto vipReal(const T& v)
{
	if constexpr (detail::HasArrayType_v<T>)
		return vipFunction([](auto l, std::void_t<decltype(vipReal(l))>* = nullptr) { return vipReal(l); }, v);
	else if constexpr (std::is_arithmetic_v<T>)
		return v;
	else
		return v.real();
}

template<class T, std::enable_if_t<std::is_arithmetic_v<T> || VipIsComplex<T>::value || detail::HasArrayType_v<T>, int> = 0>
auto vipImag(const T& v)
{
	if constexpr (detail::HasArrayType_v<T>)
		return vipFunction([](auto l, std::void_t<decltype(vipImag(l))>* = nullptr) { return vipImag(l); }, v);
	else if constexpr (std::is_arithmetic_v<T>)
		return v;
	else
		return v.imag();
}

template<class T, std::enable_if_t<std::is_arithmetic_v<T> || VipIsComplex<T>::value || detail::HasArrayType_v<T>, int> = 0>
auto vipArg(const T& v)
{
	if constexpr (detail::HasArrayType_v<T>)
		return vipFunction([](auto l, std::void_t<decltype(vipArg(l))>* = nullptr) { return vipArg(l); }, v);
	else
		return std::arg(v);
}

template<class T, std::enable_if_t<std::is_arithmetic_v<T> || VipIsComplex<T>::value || detail::HasArrayType_v<T>, int> = 0>
auto vipNorm(const T& v)
{
	if constexpr (detail::HasArrayType_v<T>)
		return vipFunction([](auto l, std::void_t<decltype(vipNorm(l))>* = nullptr) { return vipNorm(l); }, v);
	else
		return std::norm(v);
}

template<class T, std::enable_if_t<std::is_arithmetic_v<T> || VipIsComplex<T>::value || detail::HasArrayType_v<T>, int> = 0>
auto vipConj(const T& v)
{
	if constexpr (detail::HasArrayType_v<T>)
		return vipFunction([](auto l, std::void_t<decltype(vipConj(l))>* = nullptr) { return vipConj(l); }, v);
	else
		return std::conj(v);
}


template<class R, class I, std::enable_if_t<detail::IsArrayOrArithmetic<R,I>::value, int> = 0>
auto vipComplex(const R& real, const I& imag)
{
	if constexpr (detail::HasArrayType_v<R, I>)
		return vipFunction([](auto r, auto i, std::enable_if_t<std::is_arithmetic_v<decltype(r + i)>, int>* = nullptr) { return vipComplex(r, i); }, real, imag);
	else {
		using type = decltype(real + imag);
		if constexpr (std::is_floating_point_v<type>)
			return std::complex<type>((type)real, (type)imag);
		else
			return std::complex<double>((double)real, (double)imag);
	}
}

template<class Rho, class Theta, std::enable_if_t<detail::IsArrayOrArithmetic<Rho, Theta>::value, int> = 0>
auto vipPolar(const Rho& rho, const Theta& theta)
{
	if constexpr (detail::HasArrayType_v<Rho, Theta>)
		return vipFunction(
		  [](auto r, auto t, std::enable_if_t<std::is_arithmetic_v<decltype(r + t)>, int>* = nullptr) { return vipPolar(r,t); }, rho, theta);
	else {
		using type = decltype(rho + theta);
		if constexpr (std::is_floating_point_v<type>)
			return std::polar((type)rho, (type)theta);
		else
			return std::polar((double)rho, (double)theta);
	}
}

template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipAbs(const T& v)
{
	return vipFunction([](auto l, std::void_t<decltype(vipAbs(l))>* = nullptr) { return vipAbs(l); }, v);
}
template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipCeil(const T& v)
{
	return vipFunction([](auto l, std::void_t<decltype(vipCeil(l))>* = nullptr) { return vipCeil(l); }, v);
}
template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipFloor(const T& v)
{
	return vipFunction([](auto l, std::void_t<decltype(vipFloor(l))>* = nullptr) { return vipFloor(l); }, v);
}

template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipRound(const T& v)
{
	return vipFunction([](auto l, std::void_t<decltype(vipRound(l))>* = nullptr) { return vipRound(l); }, v);
}

template<class A1, class A2, class A3, std::enable_if_t<detail::IsArrayOrArithmetic<A1,A2,A3>::value, int> = 0>
auto vipClamp(const A1& a1, const A2& a2, const A3& a3)
{
	if constexpr (detail::HasArrayType_v<A1, A2, A3>)
		return vipFunction(
		  [](auto v1, auto v2, auto v3, std::enable_if_t<detail::IsArrayOrArithmetic_v<decltype(v1), decltype(v2), decltype(v3)>, int>* = nullptr) { return v1 < v2   ? v2
																		      : v1 > v3 ? v3
																				: v1; }, a1, a2, a3);
	else
		return a1 < a2 ? a2 : a1 > a3 ? a3 : a1;
}

template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipIsNan(const T& v)
{
	return vipFunction([](auto l, std::void_t<decltype(vipIsNan(l))>* = nullptr) { return vipIsNan(l); }, v);
}
template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipIsInf(const T& v)
{
	return vipFunction([](auto l, std::void_t<decltype(vipIsInf(l))>* = nullptr) { return vipIsInf(l); }, v);
}

template<class A1, class A2, class A3, std::enable_if_t<detail::HasArrayType_v<A1, A2, A3>, int> = 0>
auto vipWhere(const A1& a1, const A2& a2, const A3& a3)
{
	if constexpr (detail::HasArrayType_v<A1, A2, A3>)
		return vipFunction([](auto v1, auto v2, auto v3, std::void_t<decltype(v1 ? v2 : v3)>* = nullptr) { return v1 ? v2 : v3; }, a1, a2, a3);
	else
		return a1 ? a2 : a3;
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType_v<A1, A2>, int> = 0>
auto vipFuzzyCompare(const A1& a1, const A2& a2)
{
	return vipFunction([](auto v1, auto v2, std::void_t<decltype(vipFuzzyCompare(v1, v2))>* = nullptr) { return vipFuzzyCompare(v1, v2); }, a1, a2);
}

/*
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
*/
#endif
