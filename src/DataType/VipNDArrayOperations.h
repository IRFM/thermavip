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
#include "VipStaticNDArray.h"
#include "VipRgb.h"
#include "VipComplex.h"

#ifdef _MSC_VER
// Disable warning message 4804 (unsafe use of type 'bool' in operation)
#pragma warning(disable : 4804)
#endif

namespace detail
{
	// Record a conversion from VipNDArray to VipNDArrayType
	struct Conversion
	{
		const VipNDArrayHandle* src; // source VipNDArray
		SharedHandle dst;	     // dest VipNDArrayType
		intptr_t dst_type;	     // data type of VipNDArrayType
		bool operator==(const Conversion& other) const noexcept { return src == other.src && dst_type == other.dst_type; }
	};
	// to store in QSet
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	inline uint qHash(const Conversion& c) noexcept
	{
		return (uint)((std::intptr_t)c.src ^ c.dst_type);
	}

#else
	inline size_t qHash(const Conversion& c, size_t seed) noexcept
	{
		return (size_t)((std::intptr_t)c.src ^ c.dst_type) ^ seed;
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
		SharedHandle findConversion(const VipNDArrayHandle* src, int dst_type) const noexcept
		{
			Conversion c{ src, SharedHandle{}, (intptr_t)dst_type };
			auto it = conversions.find(c);
			return (it != conversions.end()) ? it->dst : SharedHandle{};
		}

		/// In the frame of a functor expression, convert an array containing a QImage to a VipNDArrayType<VipRGB> without copying the data
		inline VipNDArrayType<VipRGB> convertRGB(const SharedHandle& src)
		{
			QImage* img = (QImage*)(src->opaque);
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
		template<class Type>
		SharedHandle convert(const VipNDArrayHandle* src)
		{
			int metatype = qMetaTypeId<Type>();
			if (metatype == src->dataType())
				return SharedHandle((VipNDArrayHandle*)src);

			const SharedHandle h = findConversion(src, metatype);
			if (h)
				return (h);

			SharedHandle src_handle((VipNDArrayHandle*)src);
			VipNDArrayType<Type> res = (src->dataType() == qMetaTypeId<QImage>() && src->handleType() != VipNDArrayHandle::View) ? convertRGB(src_handle) : VipNDArray(src_handle);
			conversions.insert(Conversion{ src, res.sharedHandle(), (intptr_t)res.dataType() });
			return res.sharedHandle();
			/* int metatype = qMetaTypeId<typename Dst::value_type>();

			// If converting to same type, no need to go through the conversion container
			if (metatype == src.dataType())
				return src;

			const SharedHandle h = findConversion(src.sharedHandle(), metatype);
			if (h)
				return Dst(h);
			Dst res = (src.dataType() == qMetaTypeId<QImage>() && !src.isView()) ? convertRGB(src) : (src);
			Conversion c;
			c.source = src.sharedHandle();
			c.dest_type = res.dataType();
			c.dest = res.sharedHandle();
			conversions.insert(c);
			return res;*/
		}

		/// Returns the global context for the current thread
		static FunctorContext& instance() noexcept
		{
			thread_local FunctorContext inst;
			return inst;
		}
		static int& refCount() noexcept
		{
			thread_local int count = 0;
			return count;
		}
		/// Register the global context for the current thread
		static void addContext() noexcept { ++refCount(); }
		/// Unregister the global context for the current thread
		static void removeContext() noexcept
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
		ContextHelper() noexcept { FunctorContext::addContext(); }
		~ContextHelper() noexcept { FunctorContext::removeContext(); }
	};

	/// Constant operand, simply wrap a constant value
	template<class T>
	struct ConstValue : BaseExpression
	{
		template<class U>
		static T import(const U& v)
		{
			if constexpr (std::is_same_v<QVariant, U>)
				return v.template value<T>();
			else
				return static_cast<T>(v);
		}

		using value_type = T;
		static constexpr qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		T value;
		ConstValue(const ConstValue& other) = default;
		ConstValue(ConstValue&& other) noexcept = default;
		ConstValue(const T& value)
		  : value(value)
		{
		}
		ConstValue(T&& value) noexcept
		  : value(std::move(value))
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
		VIP_ALWAYS_INLINE bool alias(const void*) const noexcept { return false; }
		VIP_ALWAYS_INLINE auto dataType() const noexcept
		{
			if constexpr (std::is_same_v<QVariant, T>)
				return value.userType();
			else
				return qMetaTypeId<T>();
		}
		VIP_ALWAYS_INLINE bool isEmpty() const noexcept { return false; }
		VIP_ALWAYS_INLINE bool isUnstrided() const noexcept { return true; }
		VIP_ALWAYS_INLINE auto shape() const noexcept { return VipNDArrayShape{}; }
		template<class Coord>
		VIP_ALWAYS_INLINE const value_type& operator()(const Coord&) const noexcept
		{
			return value;
		}
		VIP_ALWAYS_INLINE const value_type& operator[](qsizetype) const noexcept { return value; }
	};

	template<class T>
	struct IsVipNDArrayType : std::false_type
	{
	};
	template<class T>
	struct IsVipNDArrayType<VipNDArrayType<T>> : std::true_type
	{
	};

	/// Wrap a VipNDArrayType or VipNDArrayTypeView
	template<class Array>
	struct ArrayWrapper : BaseExpression
	{
		using value_type = typename Array::value_type;
		static constexpr auto access_type = Array::access_type;
		static constexpr bool inner_unstrided = IsVipNDArrayType<Array>::value;

		static_assert(!std::is_same_v<value_type, void>);

		const SharedHandle handle;
		const VipNDArrayHandle* handle_p;
		ArrayWrapper(const Array& ar) noexcept
		  : handle_p(ar.handle())
		{
		}
		ArrayWrapper(const ArrayWrapper&) noexcept = default;
		template<class Other>
		ArrayWrapper(const ArrayWrapper<Other>& other)
		  : handle(FunctorContext::instance().convert<value_type>(other.handle_p))
		  , handle_p(handle.data())
		{
			static_assert(std::is_same_v<NullType, typename Other::value_type>);
		}
		VIP_ALWAYS_INLINE auto ptr() const noexcept { return static_cast<const value_type*>(handle_p->opaque); }
		template<class Shape>
		VIP_ALWAYS_INLINE auto ptr(const Shape& pos) const noexcept
		{
			return ptr() + vipFlatOffset<inner_unstrided>(handle_p->strides, pos);
		}
		VIP_ALWAYS_INLINE bool alias(const void* p) const noexcept
		{
			return (char*)p >= (char*)ptr() && (char*)p < (char*)(ptr() + vipFlatOffset<inner_unstrided>(handle_p->strides, handle_p->shape));
		}
		VIP_ALWAYS_INLINE auto dataType() const noexcept { return handle_p->dataType(); }
		VIP_ALWAYS_INLINE bool isEmpty() const noexcept { return !handle_p || handle_p->size == 0; }
		VIP_ALWAYS_INLINE bool isUnstrided() const noexcept
		{
			if constexpr (inner_unstrided)
				return true;
			else {
				bool unstrided = false;
				vipShapeToSize(handle_p->shape, handle_p->strides, &unstrided);
				return unstrided;
			}
		}
		VIP_ALWAYS_INLINE const auto& shape() const noexcept { return handle_p->shape; }
		VIP_ALWAYS_INLINE const auto& strides() const noexcept { return handle_p->strides; }
		VIP_ALWAYS_INLINE qsizetype shape(qsizetype i) const noexcept { return handle_p->shape[i]; }
		VIP_ALWAYS_INLINE qsizetype stride(qsizetype i) const noexcept { return handle_p->strides[i]; }
		VIP_ALWAYS_INLINE qsizetype shapeCount() const noexcept { return handle_p->shape.size(); }
		VIP_ALWAYS_INLINE qsizetype size() const noexcept { return handle_p->size; }
		template<class ShapeType>
		VIP_ALWAYS_INLINE const value_type& operator()(const ShapeType& pos) const noexcept
		{
			return *ptr(pos);
		}
		VIP_ALWAYS_INLINE const value_type& operator[](qsizetype i) const noexcept { return ptr()[i]; }
	};

	/// Wrap a VipStaticNDArray
	template<class T, qsizetype... Dims>
	struct ArrayWrapper<VipStaticNDArray<T, Dims...>> : BaseExpression
	{
		using value_type = T;
		static constexpr auto access_type = Vip::Flat | Vip::Position | Vip::Cwise;

		const value_type* d_p;
		ArrayWrapper(const VipStaticNDArray<T, Dims...>& ar) noexcept
		  : d_p(ar.ptr())
		{
		}
		VIP_ALWAYS_INLINE auto ptr() const noexcept { return d_p; }
		template<class Shape>
		VIP_ALWAYS_INLINE auto ptr(const Shape & pos) const noexcept
		{
			return d_p + vipFlatOffset<true>(VipStaticNDArray<T, Dims...>::strides(), pos);
		}
		VIP_ALWAYS_INLINE bool alias(const void* p) const noexcept { return (char*)p >= (char*)d_p && (char*)p < (char*)(d_p + (Dims * ...)); }
		VIP_ALWAYS_INLINE auto dataType() const noexcept { return qMetaTypeId<T>(); }
		VIP_ALWAYS_INLINE bool isEmpty() const noexcept { return false; }
		VIP_ALWAYS_INLINE bool isUnstrided() const noexcept { return true; }
		VIP_ALWAYS_INLINE auto shape() const noexcept { return vipVector(Dims...); }
		VIP_ALWAYS_INLINE auto strides() const noexcept { return VipStaticNDArray<T, Dims...>::strides(); }
		VIP_ALWAYS_INLINE qsizetype shape(qsizetype i) const noexcept { return shape()[i]; }
		VIP_ALWAYS_INLINE qsizetype stride(qsizetype i) const noexcept { return strides()[i]; }
		VIP_ALWAYS_INLINE qsizetype shapeCount() const noexcept { return (qsizetype)sizeof...(Dims); }
		VIP_ALWAYS_INLINE qsizetype size() const noexcept { return (... * Dims); }
		template<class ShapeType>
		VIP_ALWAYS_INLINE const value_type& operator()(const ShapeType& pos) const noexcept
		{
			return *ptr(pos);
		}
		VIP_ALWAYS_INLINE const value_type& operator[](qsizetype i) const noexcept { return d_p[i]; }
	};

	template<class T, class U>
	struct RebindExpression
	{
		static const auto& cast(const U& a) noexcept { return a; }
	};
	template<class T, class Array>
	struct RebindExpression<T, ArrayWrapper<Array>>
	{
		static auto cast(const ArrayWrapper<Array>& a) noexcept
		{
			if constexpr (std::is_same_v<NullType, typename Array::value_type>)
				return ArrayWrapper<VipNDArrayType<T>>(a);
			else
				return a;
		}
	};
	template<class T>
	struct RebindExpression<T, QVariant>
	{
		static auto cast(const QVariant& a) noexcept { return a.value<T>(); }
	};
	template<class T>
	struct RebindExpression<T, ConstValue<QVariant>>
	{
		static auto cast(const ConstValue<QVariant>& a) noexcept { return ConstValue<T>(a); }
	};

	template<class T>
	struct RebindExpression<T, VipNDArray>
	{
		static auto cast(const VipNDArray& a) noexcept { return VipNDArrayType<T>(a); }
	};

	template<class T, class U>
	auto rebindExpression(const U& v)
	{
		return RebindExpression<T, U>::cast(v);
	}

	template<class T, class U>
	struct RebindType
	{
		using type = decltype(rebindExpression<T>(std::declval<U&>()));
	};
	template<class T, class U>
	using RebindType_t = typename RebindType<T, U>::type;

	// Check if type inherits NullOperand (nd-array or functor expression)
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

	// Check if type inherits VipArrayBase (nd-array object)
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
	template<class T, class = void>
	struct DeduceArrayType
	{
		// Default: consider T is a functor expression
		using array_type = T;
		using value_type = typename T::value_type;
	};
	template<class T>
	struct DeduceArrayType<T, std::enable_if_t<!IsArrayType_v<T>, void>>
	{
		// T is NOT a functor expression or an array
		using array_type = ConstValue<T>;
		using value_type = std::conditional_t<std::is_same_v<T, QVariant>, NullType, T>;
	};
	template<class Array>
	struct DeduceArrayType<Array, std::enable_if_t<IsNDArrayType_v<Array>, void>>
	{
		// T is an array
		using array_type = ArrayWrapper<Array>;
		using value_type = typename Array::value_type;
	};
	template<class Array>
	struct DeduceArrayType<ArrayWrapper<Array>>
	{
		// T is an array wrapper
		using array_type = ArrayWrapper<Array>;
		using value_type = typename Array::value_type;
	};

	template<class T>
	using DeduceArrayType_t = typename DeduceArrayType<T>::array_type;

	struct FunctorBase
	{
	};

	template<class T, class = void>
	struct HasNullTypeInternal : std::is_same<typename DeduceArrayType<T>::value_type, NullType>
	{
	};
	template<class... T>
	struct HasNullTypeInternal<std::tuple<T...>>
	{
		static constexpr bool value = (... || (HasNullTypeInternal<T>::value));
	};
	template<class T>
	struct HasNullTypeInternal<T, std::enable_if_t<std::is_base_of_v<FunctorBase, T>, void>>
	{
		static constexpr bool value = HasNullTypeInternal<typename T::array_types>::value;
	};

	/// Check if given arrays and/or constants define NullType (at least one of them is a VipNDArray or a QVariant)
	template<class... Array>
	struct HasNullType
	{
		// Fold expression
		static constexpr bool value = (... || HasNullTypeInternal<Array>::value);
	};
	template<class... Array>
	struct HasNullType<std::tuple<Array...>>
	{
		// Fold expression
		static constexpr bool value = (... || HasNullTypeInternal<Array>::value);
	};

	/// Check if at least one template argument is a NullOperand (nd-array or functor expression)
	template<class... Array>
	struct HasArrayType
	{
		// Fold expression
		static constexpr bool value = (... || IsArrayType_v<Array>);
	};
	/// Check if at least one template argument is a NullOperand (nd-array or functor expression)
	template<class... Array>
	struct HasArrayType<std::tuple<Array...>>
	{
		// Fold expression
		static constexpr bool value = (... || IsArrayType_v<Array>);
	};

	template<class... Array>
	constexpr bool HasArrayType_v = HasArrayType<Array...>::value;

	/// Check if all arguments are NullOperand (nd-array or functor expression)
	template<class... Array>
	struct AllArrayType
	{
		// Fold expression
		static constexpr bool value = (... && IsArrayType_v<Array>);
	};
	template<class... Array>
	constexpr bool AllArrayType_v = AllArrayType<Array...>::value;

	/// Check if at least one argument is arithmetic
	template<class... Array>
	struct HasArithmeticType
	{
		// Fold expression
		static constexpr bool value = (... || std::is_arithmetic_v<Array>);
	};
	template<class... Array>
	constexpr bool HasArithmeticType_v = HasArithmeticType<Array...>::value;

	/// @brief Check that all types ar either array or arithmetic
	template<class... T>
	struct IsArrayOrArithmetic
	{
		static constexpr bool value = (... && (std::is_arithmetic_v<T> || detail::HasArrayType_v<T>));
	};
	template<class... Array>
	constexpr bool IsArrayOrArithmetic_v = IsArrayOrArithmetic<Array...>::value;

	/// @brief Check that all types ar either array or arithmetic
	template<class... T>
	struct IsArrayOrComplex
	{
		static constexpr bool value = (... && (detail::IsArrayOrArithmetic_v<T> || VipIsComplex_v<T>));
	};
	template<class... Array>
	constexpr bool IsArrayOrComplex_v = IsArrayOrComplex<Array...>::value;

	/// Value type of nd-array or functor expression
	template<class F, bool IsFunctorBase = std::is_base_of_v<FunctorBase, F>>
	struct ValueType
	{
		using type = typename F::value_type;
	};
	template<>
	struct ValueType<NullType, false>
	{
		using type = NullType;
	};
	template<class F>
	struct ValueType<F, true>
	{
		template<bool hasNullType, class Fun>
		static constexpr auto evalType(const Fun& f)
		{
			if constexpr (hasNullType)
				return NullType{};
			else
				return f.operator()(vipVector(0));
		}

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
		bool build_shape(const Sh& shape) noexcept
		{
			if (sh.isEmpty()) {
				sh = shape;
				return true;
			}
			if (!shape.isEmpty())
				return (shape == sh);
			return true;
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

		VIP_ALWAYS_INLINE int dataType() const noexcept { return type; }
		VIP_ALWAYS_INLINE bool alias(const void* p) const noexcept
		{
			return std::apply([p](const auto&... args) { return (args.alias(p) || ...); }, arrays);
		}
		VIP_ALWAYS_INLINE bool isEmpty() const noexcept
		{
			return std::apply([](const auto&... args) { return (args.isEmpty() || ...); }, arrays);
		}
		VIP_ALWAYS_INLINE bool isUnstrided() const noexcept
		{
			return std::apply([](const auto&... args) { return (args.isUnstrided() && ...); }, arrays);
		}
		const VipNDArrayShape& shape() const { return sh; }
		BaseFunctor() noexcept {}
		BaseFunctor(const BaseFunctor&) = default;
		BaseFunctor(BaseFunctor&&) noexcept = default;
		template<class... Ar>
		BaseFunctor(const Ar&... ar)
		  : arrays(std::forward<const Ar&>(ar)...)
		{
			if (!std::apply([&](const auto&... args) { return (build_shape(args.shape()) && ...); }, arrays))
				sh.clear();
			if constexpr (!HasNullType<Array...>::value)
				type = qMetaTypeId<ValueType_t<Derived>>();
			else
				std::apply([&](const auto&... args) { (build_type(args.dataType()), ...); }, arrays);
		}
		// Define dummy access operator to avoid compilation error
		template<class Coord>
		auto operator()(const Coord&) const noexcept
		{
			return NullType();
		}
		auto operator[](qsizetype) const noexcept { return NullType(); }
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
		VIP_ALWAYS_INLINE auto operator()(const Coord& index) const noexcept
		{
			return std::apply([&](const auto&... args) { return functor(args(index)...); }, this->arrays);
		}
		VIP_ALWAYS_INLINE auto operator[](qsizetype index) const noexcept
		{
			return std::apply([&](const auto&... args) { return functor(args[index]...); }, this->arrays);
		}
	};

	template<class U, class Functor, class... Array>
	struct RebindExpression<U, GenericFunction<Functor, Array...>>
	{

		static auto cast(const GenericFunction<Functor, Array...>& f)
		{
			// using functor = GenericFunction<Functor, Array...>;
			using type = GenericFunction<Functor, RebindType_t<U, Array>...>;

			if constexpr (std::is_invocable_v<Functor, ValueType_t<RebindType_t<U, Array>>...>)
				return std::apply([&](const auto&... args) { return type(f.functor, rebindExpression<U>(args)...); }, f.arrays);
			else
				return NullType{};
		}
	};

	/// @brief Base class for algorithms having one/multiple inputs and one output.
	/// Such algorithm can be applied using vipEval(), but their operator() member
	/// take as argument the destination array.
	/// See vipResize() function as an example of such algorithm.
	template<class Functor, class... Array>
	struct ArrayAlgorithm : BaseFunctor<ArrayAlgorithm<Functor, Array...>, Array...>
	{
		using base = BaseFunctor<ArrayAlgorithm<Functor, Array...>, Array...>;
		using functor_type = Functor;
		Functor functor;

		ArrayAlgorithm() {}
		template<class F, class... Ar>
		ArrayAlgorithm(F&& f, Ar&&... args)
		  : base(std::forward<Ar>(args)...)
		  , functor(std::forward<F>(f))
		{
		}

		template<qsizetype N>
		auto operator()(const VipCoordinate<N>&) const
		{
			if constexpr (HasNullType<Array...>::value)
				return NullType{};
			else
				return std::get<0>(std::tuple<ValueType_t<Array...>>{});
		}

		template<class Dst>
		auto apply(Dst& out) const
		{
			static_assert(!HasNullType<Dst>::value && !HasNullType<Array...>::value);
			return std::apply(this->functor, std::tuple_cat(std::tuple<Dst&>(out), this->arrays));
		}

		template<class Dst>
		static constexpr bool callable() noexcept
		{
			// Check whether the functor is callable for given dst array type
			return std::is_invocable_v<Functor, Dst&, Array...>;
		}
	};

	template<class U, class Functor, class... Array>
	struct RebindExpression<U, ArrayAlgorithm<Functor, Array...>>
	{

		static auto cast(const ArrayAlgorithm<Functor, Array...>& f)
		{
			// Cast without checking the functor validity, that will be done in vipEval()
			using type = ArrayAlgorithm<Functor, RebindType_t<U, Array>...>;
			return std::apply([&](const auto&... args) { return type(f.functor, rebindExpression<U>(args)...); }, f.arrays);
		}
	};

	template<class T>
	struct IsArrayAlgorithm : std::false_type
	{
	};
	template<class Functor, class... Array>
	struct IsArrayAlgorithm<ArrayAlgorithm<Functor, Array...>> : std::true_type
	{
	};



	/// Base class for all reductors
	template<class T>
	struct Reductor : BaseReductor
	{
		//! input value type
		using value_type = T;
		//! define the way input array is walked through. Depending on this value, setAt() or setPos() will be called.
		static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;

		int dataType() const noexcept { return qMetaTypeId<T>(); }
		bool isEmpty() const noexcept { return false; }
		bool isUnstrided() const noexcept { return true; }

		//! set a new value for given flat index
		bool setAt(qsizetype, const T&) { return true; }
		//! set a new value for given position
		template<class ShapeType>
		bool setPos(const ShapeType&, const T&)
		{
			return true;
		}
		//! finish the reduction algorithm, returns true on success, false otherwise.
		bool finish() { return true; }
	};
	template<class T>
	struct IsReductor : std::false_type
	{
	};
	template<class T>
	struct IsReductor<Reductor<T>> : std::true_type
	{
	};
}

/// @brief Create a functor expression that applies a function object.
/// Used to create function expression from any function.
template<class Functor, class... Array>
auto vipFunction(const Functor& fun, const Array&... args) noexcept
{
	auto r = detail::GenericFunction<Functor, detail::DeduceArrayType_t<std::decay_t<Array>>...>(fun, std::forward<const Array&>(args)...);
	return r;
}

/// @brief Create an array algorithm that applies a function object.
template<class Functor, class... Array>
auto vipArrayAlgorithm(const Functor& fun, const Array&... args) noexcept
{
	auto r = detail::ArrayAlgorithm<Functor, detail::DeduceArrayType_t<std::decay_t<Array>>...>(fun, std::forward<const Array&>(args)...);
	return r;
}

//
// Create operator overloads for VipNDArray
//

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator+(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l + r)>* = nullptr) { return l + r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator-(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l - r)>* = nullptr) { return l - r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator*(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l * r)>* = nullptr) { return l * r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator/(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l / r)>* = nullptr) { return l / r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator%(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l % r)>* = nullptr) { return l % r; }, l, r);
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator<(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l < r)>* = nullptr) { return l < r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator>(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l > r)>* = nullptr) { return l > r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator<=(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l <= r)>* = nullptr) { return l <= r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator>=(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l >= r)>* = nullptr) { return l >= r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator==(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l == r)>* = nullptr) { return l == r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator!=(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l != r)>* = nullptr) { return l != r; }, l, r);
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator&(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l & r)>* = nullptr) { return l & r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator|(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l | r)>* = nullptr) { return l | r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType<A1, A2>::value, int> = 0>
auto operator^(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l ^ r)>* = nullptr) { return l ^ r; }, l, r);
}

template<class A1, class A2, std::enable_if_t<detail::HasArrayType_v<A1, A2> && (detail::AllArrayType_v<A1, A2> || detail::HasArithmeticType_v<A1, A2>), int> = 0>
auto operator<<(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l << r)>* = nullptr) { return l << r; }, l, r);
}
template<class A1, class A2, std::enable_if_t<detail::HasArrayType_v<A1, A2> && (detail::AllArrayType_v<A1, A2> || detail::HasArithmeticType_v<A1, A2>), int> = 0>
auto operator>>(const A1& l, const A2& r) noexcept
{
	return vipFunction([](auto l, auto r, std::void_t<decltype(l >> r)>* = nullptr) { return l >> r; }, l, r);
}

template<class A1, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
auto operator~(const A1& l) noexcept
{
	return vipFunction([](auto l, std::void_t<decltype(~l)>* = nullptr) { return ~l; }, l);
}

template<class A1, std::enable_if_t<detail::HasArrayType<A1>::value, int> = 0>
auto operator!(const A1& l) noexcept
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
	if constexpr (detail::IsArrayAlgorithm<U>::value)
		return detail::rebindExpression<T>(value);
	else if constexpr (detail::HasArrayType_v<U>)
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

template<class R, class I, std::enable_if_t<detail::IsArrayOrArithmetic<R, I>::value, int> = 0>
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
		return vipFunction([](auto r, auto t, std::enable_if_t<std::is_arithmetic_v<decltype(r + t)>, int>* = nullptr) { return vipPolar(r, t); }, rho, theta);
	else {
		using type = decltype(rho + theta);
		if constexpr (std::is_floating_point_v<type>)
			return std::polar((type)rho, (type)theta);
		else
			return std::polar((double)rho, (double)theta);
	}
}

template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipAbs(const T& v) noexcept
{
	return vipFunction([](auto l, std::void_t<decltype(vipAbs(l))>* = nullptr) { return vipAbs(l); }, v);
}
template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipCeil(const T& v) noexcept
{
	return vipFunction([](auto l, std::void_t<decltype(vipCeil(l))>* = nullptr) { return vipCeil(l); }, v);
}
template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipFloor(const T& v) noexcept
{
	return vipFunction([](auto l, std::void_t<decltype(vipFloor(l))>* = nullptr) { return vipFloor(l); }, v);
}

template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipRound(const T& v) noexcept
{
	return vipFunction([](auto l, std::void_t<decltype(vipRound(l))>* = nullptr) { return vipRound(l); }, v);
}

template<class A1, class A2, class A3, std::enable_if_t<detail::IsArrayOrArithmetic<A1, A2, A3>::value, int> = 0>
auto vipClamp(const A1& a1, const A2& a2, const A3& a3)
{
	if constexpr (detail::HasArrayType_v<A1, A2, A3>)
		return vipFunction(
		  [](auto v1, auto v2, auto v3, std::enable_if_t<detail::IsArrayOrArithmetic_v<decltype(v1), decltype(v2), decltype(v3)>, int>* = nullptr) { return v1 < v2   ? v2
																				    : v1 > v3 ? v3
																					      : v1; },
		  a1,
		  a2,
		  a3);
	else
		return a1 < a2 ? a2 : a1 > a3 ? a3 : a1;
}

template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipIsNan(const T& v) noexcept
{
	return vipFunction([](auto l, std::void_t<decltype(vipIsNan(l))>* = nullptr) { return vipIsNan(l); }, v);
}
template<class T, std::enable_if_t<detail::HasArrayType_v<T>, int> = 0>
auto vipIsInf(const T& v) noexcept
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
auto vipFuzzyCompare(const A1& a1, const A2& a2) noexcept
{
	return vipFunction([](auto v1, auto v2, std::void_t<decltype(vipFuzzyCompare(v1, v2))>* = nullptr) { return vipFuzzyCompare(v1, v2); }, a1, a2);
}

// Trigonometric and math functions

#define TRIGONOMETRIC_FUNCTION(std_name, name)                                                                                                                                                         \
	template<class T, std::enable_if_t<detail::IsArrayOrComplex_v<T>, int> = 0>                                                                                                                    \
	auto name(const T& val)                                                                                                                                                                        \
	{                                                                                                                                                                                              \
		if constexpr (detail::HasArrayType_v<T>)                                                                                                                                               \
			return vipFunction([](auto v, std::void_t<decltype(std_name(v))>* = nullptr) { return std_name(v); }, val);                                                                    \
		else                                                                                                                                                                                   \
			return std_name(val);                                                                                                                                                          \
	}

#define MATH_FUNCTION(std_name, name)                                                                                                                                                                  \
	template<class T, std::enable_if_t<detail::IsArrayOrArithmetic_v<T>, int> = 0>                                                                                                                 \
	auto name(const T& val)                                                                                                                                                                        \
	{                                                                                                                                                                                              \
		if constexpr (detail::HasArrayType_v<T>)                                                                                                                                               \
			return vipFunction([](auto v, std::void_t<decltype(std_name(v))>* = nullptr) { return std_name(v); }, val);                                                                    \
		else                                                                                                                                                                                   \
			return std_name(val);                                                                                                                                                          \
	}

TRIGONOMETRIC_FUNCTION(std::cos, vipCos)
TRIGONOMETRIC_FUNCTION(std::sin, vipSin)
TRIGONOMETRIC_FUNCTION(std::tan, vipTan)
TRIGONOMETRIC_FUNCTION(std::cosh, vipCosh)
TRIGONOMETRIC_FUNCTION(std::sinh, vipSinh)
TRIGONOMETRIC_FUNCTION(std::tanh, vipTanh)
TRIGONOMETRIC_FUNCTION(std::acos, vipACos)
TRIGONOMETRIC_FUNCTION(std::asin, vipASin)
TRIGONOMETRIC_FUNCTION(std::atan, vipATan)
TRIGONOMETRIC_FUNCTION(std::acosh, vipACosh)
TRIGONOMETRIC_FUNCTION(std::asinh, vipASinh)
TRIGONOMETRIC_FUNCTION(std::atanh, vipATanh)

MATH_FUNCTION(std::log, vipLog)
MATH_FUNCTION(std::log10, vipLog10)
MATH_FUNCTION(std::log2, vipLog2)
MATH_FUNCTION(std::log1p, vipLog1p)
MATH_FUNCTION(std::exp, vipExp)
MATH_FUNCTION(std::exp2, vipExp2)
MATH_FUNCTION(std::pow, vipPow)
MATH_FUNCTION(std::sqrt, vipSqrt)

#endif
