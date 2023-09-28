#ifndef VIP_PROCESSING_FUNCTION_H
#define VIP_PROCESSING_FUNCTION_H

#include "VipProcessingObject.h"

#include <type_traits>
#include <functional>
#include <tuple>

namespace detail
{
	
	template<class R>
	struct TupleInfo
	{
		static constexpr size_t count = 1;

		template<size_t Id>
		static R& get(R& v) noexcept
		{
			return v;
		}
	};
	template<>
	struct TupleInfo<void>
	{
		static constexpr size_t count = 0;

		template<size_t Id, class T>
		static T& get( T& v) noexcept
		{
			return v;
		}
	};
	template<class A, class B>
	struct TupleInfo<std::pair<A, B>>
	{
		static constexpr size_t count = 2;

		template<size_t Id>
		static typename std::conditional<(Id == 0), A, B>::type& get( std::pair<A, B>& v) noexcept
		{
			return std::get<Id>(v);
		}
	};
	template<class... Args>
	struct TupleInfo<std::tuple<Args...>>
	{
		using tuple_type = std::tuple<Args...>;
		static constexpr size_t count = std::tuple_size<tuple_type>::value;

		template<size_t Id>
		static  typename std::tuple_element<Id, tuple_type>::type& get( tuple_type& v) noexcept
		{
			return std::get<Id>(v);
		}
	};


	template<class Signature>
	class Callable;

	template<class R, class... Args>
	class Callable<R(Args...)>
	{
	public:
		using return_type = R;
		using signature = R(Args...);
		using return_info = TupleInfo<R>;
		static constexpr size_t count = return_info::count;
		static constexpr size_t arity = sizeof...(Args);
		static constexpr bool has_return = !std::is_same<void, R>::value;
	};


	template<typename T>
	struct Identity
	{
		typedef T type;
	};

	// ----------
	// Function signature metafunction implementation
	// Also handler for function object case
	// ----------

	template<class T>
	struct Signature : Signature<decltype(&T::operator())>
	{
	};

	// ----------
	// Function signature specializations
	// ----------

	template<class R, class... Args>
	struct Signature<R(Args...)> : Identity<R(Args...)>
	{
	};

	// ----------
	// Function pointer specializations
	// ----------

	template<typename R, class... Args>
	struct Signature<R (*)(Args...)> : Signature<R(Args...)>
	{
	};

	// ----------
	// Member function pointer specializations
	// ----------

	template<typename C, typename R, class... Args>
	struct Signature<R (C::*)(Args...)> : Signature<R(Args...)>
	{
	};

	template<typename C, typename R, class... Args>
	struct Signature<R (C::*)(Args...) const> : Signature<R(Args...)>
	{
	};


	
	/// Forward return type to processing output

	template<class T>
	VipAnyData buildAnyData(T&& t)
	{
		return VipAnyData(QVariant::fromValue(std::move(t)));
	}
	inline VipAnyData buildAnyData(QVariant&& t) { return VipAnyData(std::move(t)); }
	inline VipAnyData buildAnyData(VipAnyData&& t) { return VipAnyData(std::move(t)); }

	template<class CallableType, size_t N = CallableType::count - 1>
	struct ForwardRet
	{
		using return_info = typename CallableType::return_info;

		template<class Tuple>
		static void apply(Tuple& t, VipProcessingObject* o, qint64 time)
		{
			VipAnyData any = buildAnyData(return_info::get<N>(t));
			any.setTime(time);
			any.setSource((qint64)o);
			any.setAttributes(o->attributes());
			o->outputAt(N)->setData(any);
			ForwardRet<ForwardRet, N - 1>::apply(t, o, time);
		}
	};
	template<class CallableType>
	struct ForwardRet<CallableType, 0>
	{
		using return_info = typename CallableType::return_info;

		template<class Tuple>
		static void apply(Tuple& t, VipProcessingObject* o, qint64 time)
		{
			VipAnyData any = buildAnyData(std::move(return_info::get<0>(t)));
			any.setTime(time);
			any.setSource((qint64)o);
			any.setAttributes(o->attributes());
			o->outputAt(0)->setData(any);
		}
	};

	template<class CallableType>
	struct ForwardRet<CallableType, (size_t)-1>
	{
		using return_info = typename CallableType::return_info;

		template<class Tuple>
		static void apply(Tuple& , VipProcessingObject* , qint64 )
		{
		}
	};



	/// Call functor based on VipProcessingObject inputs.
	/// It would be possible to use variadic template for this,
	/// but the specialization solution is in the end WAY less complex
	
	struct AnyData
	{
		VipAnyData any;
		operator const QVariant&() const { return any.data(); }
		operator const VipAnyData&() const { return any; }
		template<class T>
		operator T() const{return any.value<T>();}
	};
	template<size_t I>
	AnyData get(VipProcessingObject* o)
	{
		return AnyData{o->inputAt(I)->data()};
	}

	template<class Callable, class Ret, size_t Arity = 0>
	struct CallFunctor
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun();
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable,void,0>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 )
		{
			fun();
		}
	};


	template<class Callable, class Ret>
	struct CallFunctor<Callable, Ret, 1>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun(get<0>(o));
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable, void, 1>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 )
		{
			fun(get<0>(o));
		}
	};

	template<class Callable, class Ret>
	struct CallFunctor<Callable, Ret, 2>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun(get<0>(o), get<1>(o));
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable, void, 2>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			fun(get<0>(o), get<1>(o));
		}
	};

	template<class Callable, class Ret>
	struct CallFunctor<Callable, Ret, 3>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun(get<0>(o), get<1>(o), get<2>(o));
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable, void, 3>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 )
		{
			fun(get<0>(o), get<1>(o), get<2>(o));
		}
	};

	template<class Callable, class Ret>
	struct CallFunctor<Callable, Ret, 4>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun(get<0>(o), get<1>(o), get<2>(o), get<3>(o));
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable, void, 4>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 )
		{
			fun(get<0>(o), get<1>(o), get<2>(o), get<3>(o));
		}
	};

	template<class Callable, class Ret>
	struct CallFunctor<Callable, Ret, 5>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun(get<0>(o), get<1>(o), get<2>(o), get<3>(o), get<4>(o));
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable, void, 5>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 )
		{
			fun(get<0>(o), get<1>(o), get<2>(o), get<3>(o), get<4>(o));
		}
	};

	template<class Callable, class Ret>
	struct CallFunctor<Callable, Ret, 6>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			Ret res = fun(get<0>(o), get<1>(o), get<2>(o), get<3>(o), get<4>(o), get<5>(o));
			ForwardRet<Callable>::apply(res, o, time);
		}
	};
	template<class Callable>
	struct CallFunctor<Callable, void, 6>
	{
		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 )
		{
			fun(get<0>(o), get<1>(o), get<2>(o), get<3>(o), get<4>(o), get<5>(o));
		}
	};


	
	class VIP_CORE_EXPORT VipBaseProcessingFunction : public VipProcessingObject
	{
		Q_OBJECT
		VIP_IO(VipMultiInput inputs)
		VIP_IO(VipMultiOutput outputs)

	public:
		VipBaseProcessingFunction(QObject* parent = nullptr);
	};

	template<class Sig>
	class VipProcessingFunction : public VipBaseProcessingFunction
	{
		using signature_type = Sig;
		using callable_type = Callable<Sig>;

		static_assert(callable_type::arity > 0, "VipProcessingFunction must have at least one input");

		std::function<Sig> m_fun;
	public:
		template<class Functor>
		VipProcessingFunction(const Functor& fun, QObject* parent = nullptr)
		  : VipBaseProcessingFunction(parent)
		  , m_fun(fun)
		{
			topLevelInputAt(0)->toMultiInput()->resize(callable_type::arity);
			topLevelOutputAt(0)->toMultiOutput()->resize(callable_type::count);
		}

	protected:
		virtual void apply() { 
			
			// Get input time
			qint64 this_time = callable_type::arity == 0 ? this->time() : inputAt(0)->time();
			if (this_time == VipInvalidTime && callable_type::arity != 0)
				this_time = this->time();

			// Call function
			CallFunctor<callable_type, typename callable_type::return_type, callable_type::arity>::call(m_fun, this, this_time);

		}
	};
}


/// @brief Create a VipProcessingObject based on a function object
/// 
/// vipProcessingFunction creates a VipProcessingObject based on a function object.
/// The resulting VipProcessingObject will have any number of inputs/outputs based 
/// on the function object signature, which must be fully defined (template functions
/// are not supported).
/// 
/// The VipProcessingObject will have one input per function argument. Each function
/// argument must be:
/// -	A VipAnyData object (gives access to input time and attributes) or,
/// -	A QVariant object or,
/// -	A value type convertible to QVariant.
/// 
/// The VipProcessingObject will have a number of outputs based on the function return type:
/// -	No output if the function returns void,
/// -	2 outputs if the function returns a std::pair<>,
/// -	N outputs if the function returns std::tuple<>,
/// -	1 output if the function returns anything else.
/// 
template<class Functor>
VipProcessingObject* vipProcessingFunction(const Functor & fun, QObject * parent = nullptr)
{
	using signature = typename detail::Signature<Functor>::type;
	return new detail::VipProcessingFunction<signature>(fun, parent);
}

#endif