/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_EVAL_H
#define VIP_EVAL_H

#include "VipNDArray.h"
#include "VipNDArrayOperations.h"
#include "VipOverRoi.h"

#ifndef VIP_EVAL_THREAD_COUNT
#define PARALLEL_FOR VIP_PRAGMA(omp parallel for)
#else
#define PARALLEL_FOR VIP_PRAGMA(omp parallel for num_threads(VIP_EVAL_THREAD_COUNT))
#endif

#if defined(VIP_ENABLE_MULTI_THREADING) && !defined(VIP_DISABLE_EVAL_MULTI_THREADING)
#define __EVAL_MULTI_THREADING
#endif

namespace detail
{
	/// \internal
	/// Eval struct provides the static apply member to evaluate a functor expression into an array.
	/// If template parameter ValidFunctor (functor expression invalid for given type or value_type cannot
	/// be cast to output array type), the apply member just returns false.
	template<class DType, bool DstNullType, bool ValidFunctor, class OverRoi>
	struct Eval
	{
		/// Default apply implementation, returns false.
		/// This implementation is called when a functor defined on VipNDArray objects cannot be cast to another data type.
		template<class Dst, class Src>
		static bool apply(Dst&, const Src&, const OverRoi&)
		{
			return false;
		}
	};

	template<class DType>
	struct Eval<DType, false, true, VipInfinitRoi>
	{
		/// Evaluate src into dst.
		/// We consider that they both have the right shape.
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const VipInfinitRoi&)
		{
			// src might be empty if the internal_cast fail (because of invalid conversion)
			if (src.isEmpty())
				return false;

			typedef DType dtype;
			const int size = dst.size();
			dtype* ptr = (dtype*)dst.constHandle()->dataPointer(VipNDArrayShape());
			if (!ptr)
				return false;

#ifdef __EVAL_MULTI_THREADING
			if ((Src::access_type & Vip::Cwise) && (Dst::access_type & Vip::Cwise) && size > 4096) { // parallel access
				if ((Src::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided()) {
					PARALLEL_FOR
					for (int i = 0; i < size; ++i)
						ptr[i] = vipCast<dtype>(src[i]);
					return true;
				}
				else {
					if (src.shape().size() == 1) {
						const int w = src.shape()[0];
						PARALLEL_FOR
						for (int i = 0; i < w; ++i)
							ptr[i * dst.stride(0)] = src(vip_vector(i));
					}
					else if (src.shape().size() == 2) {
						const int h = src.shape()[0];
						const int w = src.shape()[1];
						PARALLEL_FOR
						for (int y = 0; y < h; ++y)
							for (int x = 0; x < w; ++x)
								ptr[y * dst.stride(0) + x * dst.stride(1)] = src(vip_vector(y, x));
					}
					else if (src.shape().size() == 3) {
						const int l = src.shape()[0];
						const int h = src.shape()[1];
						const int w = src.shape()[2];
						PARALLEL_FOR
						for (int z = 0; z < l; ++z)
							for (int y = 0; y < h; ++y)
								for (int x = 0; x < w; ++x)
									ptr[z * dst.stride(0) + y * dst.stride(1) + x * dst.stride(2)] = src(vip_vector(z, y, x));
					}
					else {
						vip_iter_parallel_fmajor(dst.shape(), c) ptr[vipFlatOffset<false>(dst.strides(), c)] = src(c);
					}
					return true;
				}
			}
#endif //__EVAL_MULTI_THREADING

			if ((Src::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided()) {
				const Src s = src;
				for (int i = 0; i < size; ++i)
					ptr[i] = vipCast<dtype>(s[i]);
				return true;
			}

			if (dst.isUnstrided()) {
				if (src.shape().size() == 1) {
					const int w = src.shape()[0];
					VipHybridVector<int, 1> p = { { 0 } }; 
					for (p[0] = 0; p[0] < w; ++p[0]) {
						ptr[p[0]] = vipCast<dtype>(src(p));
					}
				}
				else if (src.shape().size() == 2) {
					const int h = src.shape()[0];
					const int w = src.shape()[1];
					int i = 0;
					VipHybridVector<int, 2> p = { { 0, 0 } };
					for (p[0] = 0; p[0] < h; ++p[0])
						for (p[1] = 0; p[1] < w; ++p[1], ++i)
							ptr[i] = vipCast<dtype>(src(p));
				}
				else if (src.shape().size() == 3) {
					const int z = src.shape()[0];
					const int h = src.shape()[1];
					const int w = src.shape()[2];
					int i = 0;
					VipHybridVector<int, 3> p = { { 0, 0, 0 } };
					for (p[0] = 0; p[0] < z; ++p[0])
						for (p[1] = 0; p[1] < h; ++p[1])
							for (p[2] = 0; p[2] < w; ++p[2], ++i)
								ptr[i] = vipCast<dtype>(src(p));
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (int i = 0; i < size; ++i) {
						ptr[i] = vipCast<dtype>(src(iter.pos));
						iter.increment();
					}
				}
				return true;
			}

			if ((Src::access_type & Vip::Flat) && src.isUnstrided()) {
				if (src.shape().size() == 1) {
					const int w = src.shape()[0];
					for (int i = 0; i < w; ++i)
						ptr[dst.stride(0) * i] = vipCast<dtype>(src[i]);
				}
				else if (src.shape().size() == 2) {
					const int h = src.shape()[0];
					const int w = src.shape()[1];
					int i = 0;
					for (int y = 0; y < h; ++y)
						for (int x = 0; x < w; ++x, ++i)
							ptr[dst.stride(0) * y + dst.stride(1) * x] = vipCast<dtype>(src[i]);
				}
				else if (src.shape().size() == 3) {
					const int z = src.shape()[0];
					const int h = src.shape()[1];
					const int w = src.shape()[2];
					int i = 0;
					for (int d = 0; d < z; ++d)
						for (int y = 0; y < h; ++y)
							for (int x = 0; x < w; ++x, ++i)
								ptr[dst.stride(0) * d + dst.stride(1) * y + dst.stride(2) * x] = vipCast<dtype>(src[i]);
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (int i = 0; i < size; ++i) {
						ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src[i]);
						iter.increment();
					}
				}
				return true;
			}

			if (src.shape().size() == 1) {
				const int w = src.shape()[0];
				VipHybridVector<int, 1> p = { { 0 } }; 
				for (p[0] = 0; p[0] < w; ++p[0])
					ptr[dst.stride(0) * p[0]] = vipCast<dtype>(src(p));
			}
			else if (src.shape().size() == 2) {
				const int h = src.shape()[0];
				const int w = src.shape()[1];
				VipHybridVector<int, 2> p = { { 0, 0 } };
				for (p[0] = 0; p[0] < h; ++p[0])
					for (p[1] = 0; p[1] < w; ++p[1])
						ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
			}
			else if (src.shape().size() == 3) {
				const int z = src.shape()[0];
				const int h = src.shape()[1];
				const int w = src.shape()[2];
				VipHybridVector<int, 3> p = { { 0, 0, 0 } };
				for (p[0] = 0; p[0] < z; ++p[0])
					for (p[1] = 0; p[1] < h; ++p[1])
						for (p[2] = 0; p[2] < w; ++p[2])
							ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
			}
			else {
				CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
				for (int i = 0; i < size; ++i) {
					ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src(iter.pos));
					iter.increment();
				}
			}
			return true;
		}
	};

	template<class DType, class OverRoi>
	struct Eval<DType, false, true, OverRoi>
	{
		/// Evaluate src into dst.
		/// We consider that they both have the right shape.
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const OverRoi& roi)
		{
			typedef DType dtype;

			int size = dst.size();
			dtype* ptr = (dtype*)dst.constHandle()->dataPointer(VipNDArrayShape());
			if (!ptr)
				return false;

			if ((Dst::access_type & Vip::Flat) && (Src::access_type & Vip::Flat) && (OverRoi::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided() && roi.isUnstrided()) {
				const Src s = src;
				for (int i = 0; i < size; ++i)
					if (roi[i])
						ptr[i] = vipCast<dtype>(s[i]);
				return true;
			}

			if ((Dst::access_type & Vip::Flat) && dst.isUnstrided()) {
				if (src.shape().size() == 1) {
					const int w = src.shape()[0];
					VipHybridVector<int, 1> p = { { 0 } }; 
					for (p[0] = 0; p[0] < w; ++p[0])
						if (roi(p))
							ptr[p[0]] = vipCast<dtype>(src(p));
				}
				else if (src.shape().size() == 2) {
					const int h = src.shape()[0];
					const int w = src.shape()[1];
					int i = 0;
					VipHybridVector<int, 2> p = { { 0, 0 } };
					for (p[0] = 0; p[0] < h; ++p[0])
						for (p[1] = 0; p[1] < w; ++p[1], ++i)
							if (roi(p))
								ptr[i] = vipCast<dtype>(src(p));
				}
				else if (src.shape().size() == 3) {
					const int z = src.shape()[0];
					const int h = src.shape()[1];
					const int w = src.shape()[2];
					int i = 0;
					VipHybridVector<int, 3> p = { { 0, 0, 0 } };
					for (p[0] = 0; p[0] < z; ++p[0])
						for (p[1] = 0; p[1] < h; ++p[1])
							for (p[2] = 0; p[2] < w; ++p[2], ++i)
								if (roi(p))
									ptr[i] = vipCast<dtype>(src(p));
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (int i = 0; i < size; ++i) {
						if (roi(iter.pos))
							ptr[i] = vipCast<dtype>(src(iter.pos));
						iter.increment();
					}
				}
				return true;
			}

			if ((Src::access_type & Vip::Flat) && src.isUnstrided()) {
				if (src.shape().size() == 1) {
					const int w = src.shape()[0];
					VipHybridVector<int, 1> p = { { 0 } }; 
					for (p[0] = 0; p[0] < w; ++p[0])
						if (roi(p))
							ptr[dst.stride(0) * p[0]] = vipCast<dtype>(src[p[0]]);
				}
				else if (src.shape().size() == 2) {
					// const Src s = src;
					const int h = src.shape()[0];
					const int w = src.shape()[1];
					VipHybridVector<int, 2> p = { { 0, 0 } };
					int i = 0;
					for (p[0] = 0; p[0] < h; ++p[0])
						for (p[1] = 0; p[1] < w; ++p[1], ++i)
							if (roi(p))
								ptr[dst.stride(0) * p[0] + dst.stride(1) * p[1]] = vipCast<dtype>(src[i]);
				}
				else if (src.shape().size() == 3) {
					const int z = src.shape()[0];
					const int h = src.shape()[1];
					const int w = src.shape()[2];
					VipHybridVector<int, 3> p = { { 0, 0, 0 } };
					int i = 0;
					for (p[0] = 0; p[0] < z; ++p[0])
						for (p[1] = 0; p[1] < h; ++p[1])
							for (p[2] = 0; p[2] < w; ++p[2], ++i)
								if (roi(p))
									ptr[dst.stride(0) * p[0] + dst.stride(1) * p[1] + dst.stride(2) * p[2]] = vipCast<dtype>(src[i]);
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (int i = 0; i < size; ++i) {
						if (roi(iter.pos))
							ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src[i]);
						iter.increment();
					}
				}
				return true;
			}

			if (src.shape().size() == 1) {
				const int w = src.shape()[0];
				VipHybridVector<int, 1> p = { { 0 } }; 
				for (p[0] = 0; p[0] < w; ++p[0])
					if (roi(p))
						ptr[dst.stride(0) * p[0]] = vipCast<dtype>(src(p));
			}
			else if (src.shape().size() == 2) {
				const int h = src.shape()[0];
				const int w = src.shape()[1];
				VipHybridVector<int, 2> p = { { 0, 0 } };
				for (p[0] = 0; p[0] < h; ++p[0])
					for (p[1] = 0; p[1] < w; ++p[1])
						if (roi(p))
							ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
			}
			else if (src.shape().size() == 3) {
				const int z = src.shape()[0];
				const int h = src.shape()[1];
				const int w = src.shape()[2];
				VipHybridVector<int, 3> p = { { 0, 0, 0 } };
				for (p[0] = 0; p[0] < z; ++p[0])
					for (p[1] = 0; p[1] < h; ++p[1])
						for (p[2] = 0; p[2] < w; ++p[2])
							if (roi(p))
								ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
			}
			else {
				CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
				for (int i = 0; i < size; ++i) {
					if (roi(iter.pos))
						ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src(iter.pos));
					iter.increment();
				}
			}
			return true;
		}
	};

	template<class DType, int Dim>
	struct Eval<DType, false, true, VipOverNDRects<Dim>>
	{
		/// Evaluate src into dst.
		/// We consider that they both have the right shape.
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const VipOverNDRects<Dim>& roi)
		{
			typedef DType dtype;
			dtype* ptr = (dtype*)dst.constHandle()->dataPointer(VipNDArrayShape());
			if (!ptr)
				return false;

			if (roi.size() == 0)
				return false;
			if (roi.rects()[0].dimCount() != src.shape().size())
				return false;

			if (roi.rects()[0].dimCount() == 1) {
				for (int r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipHybridVector<int, 1> p = { { 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0]) {
						if (roi(p))
							ptr[p[0] * dst.stride(0)] = vipCast<dtype>(src(p));
					}
				}
			}
			else if (roi.rects()[0].dimCount() == 2) {
				for (int r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipHybridVector<int, 2> p = { { 0, 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0])
						for (p[1] = rect.start(1); p[1] < rect.end(1); ++p[1]) {
							if (roi(p))
								ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
						}
				}
			}
			else if (roi.rects()[0].dimCount() == 3) {
				for (int r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipHybridVector<int, 3> p = { { 0, 0, 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0])
						for (p[1] = rect.start(1); p[1] < rect.end(1); ++p[1])
							for (p[2] = rect.start(2); p[2] < rect.end(2); ++p[2]) {
								if (roi(p))
									ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
							}
				}
			}
			else {
				for (int r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(rect.shape());
					iter.pos = rect.start();
					int size = rect.shapeSize();
					for (int i = 0; i < size; ++i) {
						if (roi(iter.pos))
							ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src(iter.pos));
						iter.increment();
					}
				}
			}
			return true;
		}
	};

	template<class T, class U>
	struct Convertible
	{
		static const bool value = Convert<T, U>::valid;
	};
	template<class T>
	struct Convertible<T, NullType>
	{
		static const bool value = true;
	};
	template<class T>
	struct Convertible<NullType, T>
	{
		static const bool value = true;
	};
	template<>
	struct Convertible<NullType, NullType>
	{
		static const bool value = true;
	};

	/// Rebind functor expression from NullType to a valid data type.
	/// //TODO clear the input functor (i.e. remove all ref VipNDArray) after rebind
	/// //to avoid triggering an allocation if the dest array appear in the functor expression.
	template<class T, class U>
	typename rebind<T, U>::type internal_cast(const U& v)
	{
		ContextHelper h;
		return rebind<T, U>::cast(v);
	}

	/// Rebind a functor expression only if it is valid for given dest type.
	/// For instance, a functor using operator > cannot be cast to std::complex.
	template<class T,
		 class Src,
		 bool nullType = std::is_same<NullType, typename Src::value_type>::value,
		 bool sameType = std::is_same<T, typename Src::value_type>::value,
		 bool isValid = std::is_same<NullType, T>::value || is_valid_functor<decltype(internal_cast<T>(Src()))>::value,
		 bool isConvertible = Convertible<T, typename Src::value_type>::value>
	struct InternalCast // default: use vipCast only
	{
		static const bool valid = isValid && isConvertible;
		static typename Cast<T, Src>::type cast(const Src& src) { return vipCast<T>(src); }
	};
	template<class Src, bool nullType, bool sameType, bool isValid, bool isConvertible>
	struct InternalCast<NullType, Src, nullType, sameType, isValid, isConvertible> // Dest array has NullType
	{
		static const bool valid = true;
		static const Src& cast(const Src& src) { return src; }
	};
	template<class T, class Src>
	struct InternalCast<T, Src, false, true, true, true> // cast Src functor of valid type (not NullType) to the same type: just return it
	{
		static const bool valid = true;
		static const Src& cast(const Src& src) { return src; }
	};
	template<class T, class Src>
	struct InternalCast<T, Src, true, false, true, true> // cast Src functor of NullType to any type: just use internal_cast
	{
		static const bool valid = true;
		static typename rebind<T, Src>::type cast(const Src& src) { return internal_cast<T>(src); }
	};
	template<class T, class Src, bool nullType, bool sameType>
	struct InternalCast<T, Src, nullType, sameType, true, false> // invalid cast (functor invalid for this type)
	{
		static const bool valid = false;
		static const Src& cast(const Src& src) { return src; }
	};
	template<class T, class Src, bool nullType, bool sameType>
	struct InternalCast<T, Src, nullType, sameType, false, true> // invalid cast (functor invalid for this type)
	{
		static const bool valid = false;
		static const Src& cast(const Src& src) { return src; }
	};
	template<class T, class Src, bool nullType, bool sameType>
	struct InternalCast<T, Src, nullType, sameType, false, false> // invalid cast (functor invalid for this type)
	{
		static const bool valid = false;
		static const Src& cast(const Src& src) { return src; }
	};

	/// @brief Check weither Fun(T()) is valid
	template<class Fun>
	class SupportFun
	{
		template<class F>
		static auto test(int) -> decltype(std::declval<F&>()[0], std::true_type());

		template<class>
		static auto test(...) -> std::false_type;

	public:
		static constexpr bool value = decltype(test<Fun>(0))::value;
	};

	template<class Fun, class T, bool IsValid = SupportFun<typename rebind<T, Fun>::type>::value>
	struct EvalForComplexTypes
	{
		template<class Dst, class Src, class OverRoi>
		static bool apply(Dst& dst, const Src& src, const OverRoi& roi)
		{
			return Eval<T, false, InternalCast<T, Src>::valid, OverRoi>::apply(dst, InternalCast<T, Src>::cast(src), roi);
		}
	};
	template<class Fun, class T>
	struct EvalForComplexTypes<Fun, T, false>
	{
		template<class Dst, class Src, class OverRoi>
		static bool apply(Dst&, const Src&, const OverRoi&)
		{
			return false;
		}
	};

	template<class DType, bool ValidFunctor, class OverRoi>
	struct Eval<DType, true, ValidFunctor, OverRoi>
	{
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const OverRoi& roi)
		{
			// src of unkown type
			if (dst.dataType() == QMetaType::Bool) {
				return Eval<bool, false, InternalCast<bool, Src>::valid, OverRoi>::apply(dst, InternalCast<bool, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::Char) {
				return Eval<char, false, InternalCast<char, Src>::valid, OverRoi>::apply(dst, InternalCast<char, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::SChar) {
				return Eval<signed char, false, InternalCast<signed char, Src>::valid, OverRoi>::apply(dst, InternalCast<signed char, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::UChar) {
				return Eval<unsigned char, false, InternalCast<unsigned char, Src>::valid, OverRoi>::apply(dst, InternalCast<unsigned char, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::UShort) {
				return Eval<quint16, false, InternalCast<quint16, Src>::valid, OverRoi>::apply(dst, InternalCast<quint16, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::Short) {
				return Eval<qint16, false, InternalCast<qint16, Src>::valid, OverRoi>::apply(dst, InternalCast<qint16, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::UInt) {
				return Eval<quint32, false, InternalCast<quint32, Src>::valid, OverRoi>::apply(dst, InternalCast<quint32, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::Int) {
				return Eval<qint32, false, InternalCast<qint32, Src>::valid, OverRoi>::apply(dst, InternalCast<qint32, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::ULongLong) {
				return Eval<quint64, false, InternalCast<quint64, Src>::valid, OverRoi>::apply(dst, InternalCast<quint64, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::LongLong) {
				return Eval<qint64, false, InternalCast<qint64, Src>::valid, OverRoi>::apply(dst, InternalCast<qint64, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::Long) {
				return Eval<long, false, InternalCast<long, Src>::valid, OverRoi>::apply(dst, InternalCast<long, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::Float) {
				return Eval<float, false, InternalCast<float, Src>::valid, OverRoi>::apply(dst, InternalCast<float, Src>::cast(src), roi);
			}
			else if (dst.dataType() == QMetaType::Double) {
				return Eval<double, false, InternalCast<double, Src>::valid, OverRoi>::apply(dst, InternalCast<double, Src>::cast(src), roi);
			}
			else if (dst.dataType() == qMetaTypeId<long double>()) {
				return Eval<long double, false, InternalCast<long double, Src>::valid, OverRoi>::apply(dst, InternalCast<long double, Src>::cast(src), roi);
			}
			// non arithmetic types
			else if (dst.dataType() == qMetaTypeId<complex_f>()) {
				return Eval < complex_f, false, InternalCast<complex_f, Src>::valid && Src::valid, OverRoi > ::apply(dst, InternalCast<complex_f, Src>::cast(src), roi);
				// return EvalForComplexTypes<Src, complex_f>::apply(dst, src, roi);
			}
			else if (dst.dataType() == qMetaTypeId<complex_d>()) {
				return Eval < complex_d, false, InternalCast<complex_d, Src>::valid && Src::valid, OverRoi > ::apply(dst, InternalCast<complex_d, Src>::cast(src), roi);
				// return EvalForComplexTypes<Src, complex_d>::apply(dst, src, roi);
			}
			else if (dst.dataType() == qMetaTypeId<VipRGB>()) {
				return Eval < VipRGB, false, InternalCast<VipRGB, Src>::valid && Src::valid, OverRoi > ::apply(dst, InternalCast<VipRGB, Src>::cast(src), roi);
				// return EvalForComplexTypes<Src, VipRGB>::apply(dst, src, roi);
			}
			else if (dst.dataType() == qMetaTypeId<QImage>()) {
				VipNDArrayTypeView<VipRGB> view = dst;
				return Eval < VipRGB, false, InternalCast<VipRGB, Src>::valid && Src::valid, OverRoi > ::apply(view, InternalCast<VipRGB, Src>::cast(src), roi);
				// return EvalForComplexTypes<Src, VipRGB>::apply(view, src, roi);
			}
			else
				return false;
		}
	};

	template<class Src, bool IsSrcArray = std::is_base_of<VipNDArray, Src>::value>
	struct EvalConvert : std::false_type
	{
		template<class Dst>
		static bool apply(Dst&, const Src&)
		{
			return false;
		}
	};
	template<class Src>
	struct EvalConvert<Src, true> : std::true_type
	{
		template<class Dst>
		static bool apply(Dst& dst, const Src& src)
		{
			return src.convert(dst);
		}
	};

}

/// \addtogroup DataType
/// @{

/// Evaluate functor expression \a src into #VipNDArray \a dst over the Region Of Interest \a roi.
///
/// The Region Of Interest is another functor expression which operators (position) and [index] return a boolean value.
/// Use #vipOverRects to evaluate the functor expression on a sub part of input array.
///
/// Dst can be a raw #VipNDArray holding an array of one of the standard types: arithmetic types, complex_d and complex_f,
/// QString, QByteArray of VipRGB. Dst can also be a typed array (like VipNDArrayType and VipNDArrayTypeView) of any type
/// (not limited to the standard types).
///
/// Src is a functor expression built by combining VipNDArray operators or using a function like #vipTransform or #vipConvolve.
/// Src functor can mix typed or raw VipNDArray objects. Raw VipNDArray objects will be casted to Dst type before evaluation
/// (which might trigger one or more allocations/copies). Note that a VipNDArray appearing several times in the fuctor expression
/// will be casted only once.
///
/// It is allowed to use \a dst in the functor expression and this won't trigger a reallocation/copy of dst array.
///
/// This function might return false for several reasons:
/// - source and destination array mismatch,
/// - invalid cast from source to destination array type (using typed arrays only will result on a compilation error)
/// - invalid functor expression for the destinaion type (trying to convolve an array of QString will return false).
template<class Dst, class Src, class OverRoi = VipInfinitRoi>
bool vipEval(Dst& dst, const Src& src, const OverRoi& roi = OverRoi())
{
	try {
		if (detail::EvalConvert<Src>::value) {
			return detail::EvalConvert<Src>::apply(dst, src);
		}
		typedef typename Dst::value_type value_type;
		if (dst.shape() != src.shape())
			return false;
		return detail::Eval<value_type,					  // dst type
				    detail::HasNullType<Dst>::value,		  // dst is null or not
				    detail::InternalCast<value_type, Src>::valid, // is it a valid cast?
				    OverRoi>::apply(dst,
						    detail::InternalCast<value_type, Src>::cast(src), // cast the src to dst type
						    roi);
	}
	catch (...) {
#ifdef VIP_EVAL_THROW
		throw;
#else
		dst.clear();
#endif
	}
	return false;
}

/// @}
// end DataType

#endif