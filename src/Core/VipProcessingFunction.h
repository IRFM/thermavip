/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_PROCESSING_FUNCTION_H
#define VIP_PROCESSING_FUNCTION_H

#include "VipFunctionTraits.h"
#include "VipProcessingObject.h"

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
		static T& get(T& v) noexcept
		{
			return v;
		}
	};
	template<class A, class B>
	struct TupleInfo<std::pair<A, B>>
	{
		static constexpr size_t count = 2;

		template<size_t Id>
		static typename std::conditional<(Id == 0), A, B>::type& get(std::pair<A, B>& v) noexcept
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
		static typename std::tuple_element<Id, tuple_type>::type& get(tuple_type& v) noexcept
		{
			return std::get<Id>(v);
		}
	};

	/// Forward return type to processing output

	template<class T>
	VipAnyData buildAnyData(T&& t)
	{
		return VipAnyData(QVariant::fromValue(std::move(t)));
	}
	inline VipAnyData buildAnyData(QVariant&& t)
	{
		return VipAnyData(std::move(t));
	}
	inline VipAnyData buildAnyData(VipAnyData&& t)
	{
		return VipAnyData(std::move(t));
	}

	template<size_t N>
	struct ForwardRet
	{
		template<class Tuple>
		static void apply(Tuple& t, VipProcessingObject* o, qint64 time)
		{
			using info = TupleInfo<Tuple>;
			VipAnyData any = buildAnyData(std::move(info::template get<N>(t)));
			any.setTime(time);
			any.setSource((qint64)o);
			any.setAttributes(o->attributes());
			o->outputAt(N)->setData(any);
			ForwardRet<N - 1>::apply(t, o, time);
		}
	};
	template<>
	struct ForwardRet<0>
	{
		template<class Tuple>
		static void apply(Tuple& t, VipProcessingObject* o, qint64 time)
		{
			using info = TupleInfo<Tuple>;
			VipAnyData any = buildAnyData(std::move(info::template get<0>(t)));
			any.setTime(time);
			any.setSource((qint64)o);
			any.setAttributes(o->attributes());
			o->outputAt(0)->setData(any);
		}
	};
	template<>
	struct ForwardRet<(size_t)-1>
	{
		template<class Tuple>
		static void apply(Tuple&, VipProcessingObject*, qint64)
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
		operator T() const
		{
			return any.value<T>();
		}
	};
	template<size_t I>
	AnyData get(VipProcessingObject* o)
	{
		return AnyData{ o->inputAt(I)->data() };
	}

	struct ProcessingGetter
	{
		VipProcessingObject* o;
		template<size_t I>
		AnyData get()
		{
			return AnyData{ o->inputAt(static_cast<int>(I))->data() };
		}
	};

	template<class Signature, class Ret = typename VipFunctionTraits<Signature>::return_type>
	struct CallFunctorForward
	{
		using return_type = typename VipFunctionTraits<Signature>::return_type;

		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			auto ret = vipApply(fun, ProcessingGetter{ o });
			ForwardRet<VipFunctionTraits<Signature>::nargs - 1>::apply(ret, o, time);
		}
	};
	template<class Signature>
	struct CallFunctorForward<Signature, void>
	{
		using return_type = typename VipFunctionTraits<Signature>::return_type;

		template<class Functor>
		static void call(const Functor& fun, VipProcessingObject* o, qint64 time)
		{
			vipApply(fun, ProcessingGetter{ o });
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
		static_assert(VipFunctionTraits<Sig>::nargs > 0, "VipProcessingFunction must have at least one input");

		std::function<Sig> m_fun;

	public:
		template<class Functor>
		VipProcessingFunction(const Functor& fun, QObject* parent = nullptr)
		  : VipBaseProcessingFunction(parent)
		  , m_fun(fun)
		{
			topLevelInputAt(0)->toMultiInput()->resize(VipFunctionTraits<Sig>::nargs);
			topLevelOutputAt(0)->toMultiOutput()->resize(VipFunctionTraits<Sig>::nargs);
		}

	protected:
		virtual void apply()
		{

			// Get input time
			qint64 this_time = VipFunctionTraits<Sig>::nargs == 0 ? this->time() : inputAt(0)->time();
			if (this_time == VipInvalidTime && VipFunctionTraits<Sig>::nargs != 0)
				this_time = this->time();

			// Call function
			CallFunctorForward<Sig>::call(m_fun, this, this_time);
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
VipProcessingObject* vipProcessingFunction(const Functor& fun, QObject* parent = nullptr)
{
	using signature = typename VipFunctionTraits<Functor>::signature_type;
	return new detail::VipProcessingFunction<signature>(fun, parent);
}

#endif