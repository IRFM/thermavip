#ifndef VIP_FUNCTION_TRAITS_H
#define VIP_FUNCTION_TRAITS_H

#include <type_traits>
#include <utility>
#include <tuple>


namespace detail
{

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

	template<class Signature>
	class CallableInfos;

	template<class R, class... Args>
	class CallableInfos<R(Args...)>
	{
	public:
		using return_type = R;
		using signature_type = R(Args...);
		static constexpr size_t nargs = sizeof...(Args);

		template<size_t I>
		struct element
		{
			using type = typename std::tuple_element<I,std::tuple<Args...> >::type;
		};
	};


	
	template<size_t N>
	struct ApplyFunctor
	{
		template<typename F, typename Getter, typename... A>
		static inline auto apply(F&& f, Getter&& t, A&&... a)
		  -> decltype(ApplyFunctor<N - 1>::apply(std::forward<F>(f), std::forward<Getter>(t), std::forward<Getter>(t).template get<N - 1>(), std::forward<A>(a)...))
		{
			return ApplyFunctor<N - 1>::apply(std::forward<F>(f), std::forward<Getter>(t), std::forward<Getter>(t).template get<N - 1>(), std::forward<A>(a)...);
		}
	};

	template<>
	struct ApplyFunctor<0>
	{
		template<typename F, typename Getter, typename... A>
		static inline auto apply(F&& f, Getter&&, A&&... a) -> decltype(std::forward<F>(f)(std::forward<A>(a)...))
		{
			return std::forward<F>(f)(std::forward<A>(a)...);
		}
	};

	template<class TupleRef>
	struct TupleGetter
	{
		TupleRef t;
		template<size_t I>
		auto get() -> decltype(std::get<I>(t))
		{
			return std::get<I>(t);
		}
	};
}


template<class C>
class VipFunctionTraits : detail::CallableInfos<typename detail::Signature<typename std::decay<C>::type>::type>
{
	using base = detail::CallableInfos<typename detail::Signature<typename std::decay<C>::type>::type>;

public:
	using return_type = typename base::return_type;
	using signature_type = typename base::signature_type;
	template<size_t I>
	using element = typename base::template element<I>;
	static constexpr size_t nargs = base::nargs;
};




template<typename F, typename Getter>
inline typename VipFunctionTraits<F>::return_type vipApply(F&& f, Getter&& t)
{
	return detail::ApplyFunctor<VipFunctionTraits<typename std::decay<F>::type>::nargs>::apply(std::forward<F>(f), std::forward<Getter>(t));
}

template<typename F, typename... Args>
inline typename VipFunctionTraits<F>::return_type vipApply(F&& f, std::tuple<Args...> && t)
{
	using tuple_type = std::tuple<Args...>;
	using ref_type = typename std::conditional<std::is_const<decltype(t)>::value,const tuple_type&, tuple_type&>::type;
	using getter = detail::TupleGetter<ref_type>;
	
	return detail::ApplyFunctor<VipFunctionTraits<typename std::decay<F>::type>::nargs>::apply(std::forward<F>(f), getter{ t });
}

template<typename F, typename... Args>
inline typename VipFunctionTraits<F>::return_type vipApply(F&& f, const std::tuple<Args...>& t)
{
	using tuple_type = std::tuple<Args...>;
	using ref_type = const tuple_type&;
	using getter = detail::TupleGetter<ref_type>;

	return detail::ApplyFunctor<VipFunctionTraits<typename std::decay<F>::type>::nargs>::apply(std::forward<F>(f), getter{ t });
}


#endif