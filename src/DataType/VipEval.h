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

#ifndef VIP_EVAL_H
#define VIP_EVAL_H


#include <optional>

#include "VipNDArray.h"
#include "VipNDArrayOperations.h"
#include "VipOverRoi.h"

namespace detail
{
	// Tells if a compile error should be triggered
	template<bool Err>
	struct CError
	{
	};
}



// Forward declaration
template<class Dst, class Src, class OverRoi , bool Err>
bool vipEval(const Dst& _dst, const Src& src, const OverRoi& roi = {}, detail::CError<Err> = {});

namespace detail
{

	struct NullRoi
	{
		static constexpr qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		using value_type = bool;
		template<class ShapeType>
		constexpr bool operator()(const ShapeType&) const noexcept
		{
			return true;
		}
		constexpr bool operator[](qsizetype) const noexcept { return true; }
		constexpr bool isUnstrided() const noexcept { return true; }
	};

	template<class OverRoi>
	struct Eval
	{
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const OverRoi& roi)
		{
			static constexpr auto reduce = std::is_base_of_v<BaseReductor, Dst>;

			using dtype = ValueType_t<Dst>;

			qsizetype size = 0;
			dtype* ptr = nullptr;
			if constexpr (!reduce) {
				size = dst.size();
				ptr = (dtype*)dst.constPtr();
				if (!ptr)
					return false;
			}
			else
				size = vipCumMultiply(src.shape());

			if ((Dst::access_type & Vip::Flat) && (Src::access_type & Vip::Flat) && (OverRoi::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided() && roi.isUnstrided()) {
				if constexpr ((Dst::access_type & Vip::Flat) && (Src::access_type & Vip::Flat) && (OverRoi::access_type & Vip::Flat)) {

					const Src s = src;
					for (qsizetype i = 0; i < size; ++i)
						if (roi[i]) {
							if constexpr (!reduce)
								ptr[i] = (s[i]);
							else
								dst.setAt(i, (s[i]));
						}
					return true;
				}
			}

			if (src.shape().size() == 1) {
				const qsizetype w = src.shape()[0];
				VipCoordinate<1> p = { { 0 } };
				for (p[0] = 0; p[0] < w; ++p[0])
					if (roi(p)) {
						if constexpr (!reduce)
							ptr[dst.stride(0) * p[0]] = (src(p));
						else
							dst.setPos(p, src(p));
					}
			}
			else if (src.shape().size() == 2) {
				const qsizetype h = src.shape()[0];
				const qsizetype w = src.shape()[1];
				VipCoordinate<2> p = { { 0, 0 } };
				for (p[0] = 0; p[0] < h; ++p[0])
					for (p[1] = 0; p[1] < w; ++p[1])
						if (roi(p)) {
							if constexpr (!reduce)
								ptr[vipFlatOffset<false>(dst.strides(), p)] = (src(p));
							else
								dst.setPos(p, src(p));
						}
			}
			else if (src.shape().size() == 3) {
				const qsizetype z = src.shape()[0];
				const qsizetype h = src.shape()[1];
				const qsizetype w = src.shape()[2];
				VipCoordinate<3> p = { { 0, 0, 0 } };
				for (p[0] = 0; p[0] < z; ++p[0])
					for (p[1] = 0; p[1] < h; ++p[1])
						for (p[2] = 0; p[2] < w; ++p[2])
							if (roi(p)) {
								if constexpr (!reduce)
									ptr[vipFlatOffset<false>(dst.strides(), p)] = (src(p));
								else
									dst.setPos(p, src(p));
							}
			}
			else {
				vip_iter_fmajor(src.shape(), c)
				{
					if (roi(c)) {
						if constexpr (!reduce)
							ptr[vipFlatOffset<false>(dst.strides(), c)] = (src(c));
						else
							dst.setPos(c, src(c));
					}
				}
			}
			return true;
		}
	};

	template<>
	struct Eval<VipInfinitRoi>
	{
		/// Evaluate src into dst.
		/// We consider that they both have the right shape.
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const VipInfinitRoi&)
		{
			// src might be empty if the internal_cast fail (because of invalid conversion)
			if (src.isEmpty() || dst.isEmpty())
				return false;

			using dtype = ValueType_t<Dst>;
			const qsizetype size = dst.size();
			dtype* ptr = (dtype*)dst.constPtr();
			if (!ptr)
				return false;
#ifdef _OPENMP
			int threads = vipLoopThreadCount(dst.size());
#else
			int threads = 1;
#endif

			if ((Src::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided()) {
				if constexpr (Src::access_type & Vip::Flat) {
					VIP_PARALLEL_FOR_NUM_THREADS(threads)
					for (qsizetype i = 0; i < size; ++i)
						ptr[i] = (src[i]);
					return true;
				}
			}
			else {
				if (src.shape().size() == 1) {
					const qsizetype w = src.shape()[0];
					VIP_PARALLEL_FOR_NUM_THREADS(threads)
					for (qsizetype i = 0; i < w; ++i)
						ptr[dst.flatIndex(vipVector(i))] = src(vipVector(i));
				}
				else if (src.shape().size() == 2) {
					const qsizetype h = src.shape()[0];
					const qsizetype w = src.shape()[1];
					VIP_PARALLEL_FOR_NUM_THREADS(threads)
					for (qsizetype y = 0; y < h; ++y) {
						VipCoordinate<2> c{ { y, 0 } };
						for (; c[1] < w; ++c[1])
							ptr[dst.flatIndex(c)] = src(c);
					}
				}
				else if (src.shape().size() == 3) {
					const qsizetype l = src.shape()[0];
					const qsizetype h = src.shape()[1];
					const qsizetype w = src.shape()[2];
					VIP_PARALLEL_FOR_NUM_THREADS(threads)
					for (qsizetype z = 0; z < l; ++z) {
						VipCoordinate<3> c{ { z, 0, 0 } };
						for (c[1] = 0; c[1] < h; ++c[1])
							for (c[2] = 0; c[2] < w; ++c[2])
								ptr[dst.flatIndex(c)] = src(c);
					}
				}
				else {
					vip_iter_parallel_fmajor(dst.shape(), c) ptr[vipFlatOffset<false>(dst.strides(), c)] = src(c);
				}
				return true;
			}
		}
	};

	template<qsizetype Dim>
	struct Eval<VipOverNDRects<Dim>>
	{
		/// Evaluate src into dst.
		/// We consider that they both have the right shape.
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const VipOverNDRects<Dim>& roi)
		{
			static constexpr auto reduce = std::is_base_of_v<BaseReductor, Dst>;

			using dtype = ValueType_t<Dst>;
			dtype* ptr = nullptr;
			if constexpr (!reduce) {
				ptr = (dtype*)dst.constPtr();
				if (!ptr)
					return false;
			}

			if (roi.size() == 0)
				return false;
			if (roi.rects()[0].dimCount() != src.shape().size())
				return false;

			VipCoordinate<Dim> start;
			start.resize(src.shape().size());
			start.fill(0);

			VipNDRect<Dim> im_rect(start, src.shape());

			if (roi.rects()[0].dimCount() == 1) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim> rect = roi.rects()[r] & im_rect;
					VipCoordinate<1> p = { { 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0]) {
						if (roi(p)) {
							if constexpr (!reduce)
								ptr[p[0] * dst.stride(0)] = (src(p));
							else
								dst.setPos(p, src(p));
						}
					}
				}
			}
			else if (roi.rects()[0].dimCount() == 2) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim> rect = roi.rects()[r] & im_rect;
					VipCoordinate<2> p = { { 0, 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0])
						for (p[1] = rect.start(1); p[1] < rect.end(1); ++p[1]) {
							if (roi(p)) {
								if constexpr (!reduce)
									ptr[vipFlatOffset<false>(dst.strides(), p)] = (src(p));
								else
									dst.setPos(p, src(p));
							}
						}
				}
			}
			else if (roi.rects()[0].dimCount() == 3) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim> rect = roi.rects()[r] & im_rect;
					VipCoordinate<3> p = { { 0, 0, 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0])
						for (p[1] = rect.start(1); p[1] < rect.end(1); ++p[1])
							for (p[2] = rect.start(2); p[2] < rect.end(2); ++p[2]) {
								if (roi(p)) {
									if constexpr (!reduce)
										ptr[vipFlatOffset<false>(dst.strides(), p)] = (src(p));
									else
										dst.setPos(p, src(p));
								}
							}
				}
			}
			else {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r] & im_rect;
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(rect.shape());
					iter.pos = rect.start();
					qsizetype size = rect.shapeSize();
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos)) {
							if constexpr (!reduce)
								ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = (src(iter.pos));
							else
								dst.setPos(iter.pos, src(iter.pos));
						}
						iter.increment();
					}
				}
			}
			return true;
		}
	};

	template<class Reductor, class SrcType, class = void>
	struct IsReductorCallable : std::false_type
	{
	};
	template<class Reductor, class SrcType>
	struct IsReductorCallable<Reductor, SrcType, std::void_t<decltype(std::declval < Reductor&>().setPos(vipVector(0), SrcType{}))>> : std::true_type
	{
	};

	template<class Dst, class Src, class Roi>
	bool evalInternal(Dst& dst, const Src& src, const Roi& roi)
	{
		if constexpr (std::is_base_of_v<BaseReductor, Dst>) {

			using in_type = ValueType_t<Src>;
			if constexpr (IsReductorCallable<Dst, in_type>::value) {

				if constexpr (std::is_same_v<Roi, VipInfinitRoi>) {
					if (Eval<NullRoi>::apply(dst, src, NullRoi{}))
						return dst.finish();
					else
						return false;
				}
				else {
					if (Eval<Roi>::apply(dst, src, roi))
						return dst.finish();
					else
						return false;
				}
			}
			else
				return false;
		}
		else if constexpr (IsArrayAlgorithm<Src>::value) {
			(void)roi;
			// Check if the array algorithm is callable with given dst array type
			if constexpr (Src::template callable<Dst>()) {
				auto fun = [&]() { return src.apply(dst); };
				using ret = std::invoke_result_t<decltype(fun)>;
				if constexpr (std::is_void_v<ret>) {
					src.apply(dst);
					return true;
				}
				else
					return src.apply(dst);
			}
			else
				return false;
		}
		else {

			if constexpr (!(Src::access_type & Vip::Cwise)) {
				// We must check aliasing
				if (src.alias(dst.constPtr())) {
					// Aliasing: go through a temporary array
					VipNDArrayType<typename Dst::value_type> tmp(dst.shape());
					if (Eval<Roi>::apply(tmp, src, roi)) {
						dst = tmp;
						return true;
					}
					else
						return false;
				}
			}

			return Eval<Roi>::apply(dst, src, roi);
		}
	}




	

	template<class T, class Dst, class Src, class OverRoi = VipInfinitRoi>
	bool evalInDst(Dst& dst, const Src& src, const OverRoi& roi = {})
	{
		using rebing_src = RebindType_t<T, Src>;
		if constexpr (std::is_same_v<NullType, rebing_src>)
			// Cannot rebind src
			return false;
		else if constexpr (std::is_same_v<NullType, ValueType_t<rebing_src>>)
			// Cannot rebind src
			return false;
		else {
			// Rebind src, use ContextHelper to avoid multiple conversions
			detail::ContextHelper ctx;
			return vipEval(dst, rebindExpression<T>(src), roi, CError<false>{});
		}
	}
	


} // end detail

/// \addtogroup DataType
/// @{

/// Evaluate functor expression src into array dst over the Region Of Interest roi.
///
/// The Region Of Interest is another functor expression which operators (position) and [index] return a boolean value.
/// Use vipOverRects to evaluate the functor expression on a sub part of input array.
///
/// Dst can be a raw VipNDArray holding an array of one of the standard types: arithmetic types, complex_d and complex_f,
/// QString, QByteArray, VipRGB and VipRGBf. Dst can also be a typed array (like VipNDArrayType, VipNDArrayTypeView, VipArrayView, vipStaticNDArray) of any type
/// (not limited to the standard types).
///
/// Src is a functor expression built by combining VipNDArray operators or using a function like vipTransform() or vipConvolve().
/// Src functor can mix typed or raw VipNDArray objects. Raw VipNDArray objects will be casted to the deduced expression type before evaluation
/// (which might trigger one or more allocations/copies). Note that a VipNDArray appearing several times in the fuctor expression
/// will be casted only once.
///
/// It is allowed to use dst in the functor expression and this won't trigger a reallocation/copy of dst array.
///
/// This function might return false for several reasons:
/// - source and destination array shape mismatch,
/// - invalid cast from source to destination array type (using typed arrays only will result on a compilation error)
/// - invalid functor expression for the destination type (trying to convolve an array of QString will return false).
/// 
/// This function will never reset the dst array.
/// 
/// vipEval() is also used to evaluate reduction algorithms (inheriting detail::BaseReductor) and array algorithms (inheriting detail::ArrayAlgorithm).
/// For instance, vipResize() uses internally vipEval() to apply a resizing algorithm.
/// 
template<class Dst, class Src, class OverRoi = VipInfinitRoi, bool Err = true>
bool vipEval(const Dst& _dst, const Src& src, const OverRoi& roi , detail::CError<Err> )
{
	static constexpr auto reduce = std::is_base_of_v<detail::BaseReductor, Dst>;

	Dst& dst = const_cast<Dst&>(_dst);
	using dst_type = detail::ValueType_t<Dst>;
	using src_type = detail::ValueType_t<Src>;

	if constexpr (std::is_base_of_v<VipNDArray, Src> && std::is_base_of_v<VipNDArray, Dst>) {
		return src.convert(dst);
	}
	else if constexpr (!std::is_same_v<dst_type, detail::NullType>) {
		if constexpr (!std::is_same_v<src_type, detail::NullType>) {
			if constexpr (Err && !detail::IsArrayAlgorithm<Src>::value) {

				static_assert(VipIsCastable_v<src_type, dst_type>, "cannot cast input functor type to destination array type");
				if constexpr (!std::is_same_v<src_type, dst_type> && !reduce)
					return detail::evalInternal(dst, vipCast<dst_type>(src), roi);
				else
					return detail::evalInternal(dst, src, roi);
			}
			else {
				if constexpr (detail::IsArrayAlgorithm<Src>::value) {
					return detail::evalInternal(dst, src, roi);
				}
				else if constexpr (VipIsCastable_v<src_type, dst_type>) {
					if constexpr (!std::is_same_v<src_type, dst_type> && !reduce)
						return detail::evalInternal(dst, vipCast<dst_type>(src), roi);
					else
						return detail::evalInternal(dst, src, roi);
				}
				else
					return false;
			}
		}
		else {
			// Src type unknwon at compile time

			// Rebind src to its actual type
			switch (src.dataType()) {
				case QMetaType::Char:
					return detail::evalInDst<char>(dst, src, roi);
				case QMetaType::SChar:
					return detail::evalInDst<signed char>(dst, src, roi);
				case QMetaType::UChar:
					return detail::evalInDst<unsigned char>(dst, src, roi);
				case QMetaType::Short:
					return detail::evalInDst<qint16>(dst, src, roi);
				case QMetaType::UShort:
					return detail::evalInDst<quint16>(dst, src, roi);
				case QMetaType::Int:
					return detail::evalInDst<qint32>(dst, src, roi);
				case QMetaType::UInt:
					return detail::evalInDst<quint32>(dst, src, roi);
				case QMetaType::Long:
					return detail::evalInDst<long>(dst, src, roi);
				case QMetaType::ULong:
					return detail::evalInDst<unsigned long>(dst, src, roi);
				case QMetaType::LongLong:
					return detail::evalInDst<qint64>(dst, src, roi);
				case QMetaType::ULongLong:
					return detail::evalInDst<quint64>(dst, src, roi);
				case QMetaType::Float:
					return detail::evalInDst<float>(dst, src, roi);
				case QMetaType::Double:
					return detail::evalInDst<double>(dst, src, roi);
				case QMetaType::QString:
					return detail::evalInDst<QString>(dst, src, roi);
				case QMetaType::QByteArray:
					return detail::evalInDst<QByteArray>(dst, src, roi);
				default:
					break;
			}
			if (src.dataType() == qMetaTypeId<long double>())
				return detail::evalInDst<long double>(dst, src, roi);
			if (src.dataType() == qMetaTypeId<complex_f>())
				return detail::evalInDst<complex_f>(dst, src, roi);
			if (src.dataType() == qMetaTypeId<complex_d>())
				return detail::evalInDst<complex_d>(dst, src, roi);
			if (src.dataType() == qMetaTypeId<VipRGB>())
				return detail::evalInDst<VipRGB>(dst, src, roi);
			if (src.dataType() == qMetaTypeId<VipRGBf>())
				return detail::evalInDst<VipRGBf>(dst, src, roi);
		}
	}
	else {
		if constexpr (!std::is_same_v<src_type, detail::NullType>) {
			if (dst.dataType() == src.dataType()) {
				// Same src and dst data types
				return vipEval(VipArrayView<src_type>(dst), src, roi);
			}
		}

		// Unknown dst types, test all dst data type without triggering compilation error
		switch (dst.dataType()) {
			case QMetaType::Char:
				return vipEval(VipArrayView<char>(dst), src, roi, detail::CError<false>{});
			case QMetaType::SChar:
				return vipEval(VipArrayView<signed char>(dst), src, roi, detail::CError<false>{});
			case QMetaType::UChar:
				return vipEval(VipArrayView<unsigned char>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Short:
				return vipEval(VipArrayView<qint16>(dst), src, roi, detail::CError<false>{});
			case QMetaType::UShort:
				return vipEval(VipArrayView<quint16>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Int:
				return vipEval(VipArrayView<qint32>(dst), src, roi, detail::CError<false>{});
			case QMetaType::UInt:
				return vipEval(VipArrayView<quint32>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Long:
				return vipEval(VipArrayView<long>(dst), src, roi, detail::CError<false>{});
			case QMetaType::ULong:
				return vipEval(VipArrayView<unsigned long>(dst), src, roi, detail::CError<false>{});
			case QMetaType::LongLong:
				return vipEval(VipArrayView<qint64>(dst), src, roi, detail::CError<false>{});
			case QMetaType::ULongLong:
				return vipEval(VipArrayView<quint64>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Float:
				return vipEval(VipArrayView<float>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Double:
				return vipEval(VipArrayView<double>(dst), src, roi, detail::CError<false>{});
			case QMetaType::QByteArray:
				return vipEval(VipArrayView<QByteArray>(dst), src, roi, detail::CError<false>{});
			case QMetaType::QString:
				return vipEval(VipArrayView<QString>(dst), src, roi, detail::CError<false>{});
			default:
				break;
		}
		if (dst.dataType() == qMetaTypeId<long double>())
			return vipEval(VipArrayView<long double>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<complex_f>())
			return vipEval(VipArrayView<complex_f>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<complex_d>())
			return vipEval(VipArrayView<complex_d>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<VipRGB>() )
			return vipEval(VipArrayView<VipRGB>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<VipRGBf>())
			return vipEval(VipArrayView<VipRGBf>(dst), src, roi, detail::CError<false>{});
	}
	return false;
}




namespace detail
{
	template<class T, class Dst, class Src, class OverRoi = VipInfinitRoi, bool Err = true>
	bool evalResetDst(Dst& dst, const Src& src, const OverRoi& roi = {}, CError<Err> err = {})
	{
		using rebing_src = RebindType_t<T, Src>;
		if constexpr (std::is_same_v<NullType, rebing_src>)
			// Cannot rebind src
			return false;
		else if constexpr (std::is_same_v<NullType, ValueType_t<rebing_src>>)
			// Cannot rebind src
			return false;
		else {
			// Use context helper to avoid converting the same array many times
			detail::ContextHelper ctx;

			// Reset dst to the new type and shape
			using new_src_type = ValueType_t<rebing_src>;
			dst.reset(src.shape(), qMetaTypeId<new_src_type>());
			return vipEval(VipArrayView<new_src_type>(dst), rebindExpression<T>(src), roi, err);
		}
	}
}

/// @brief Evaluate functor expression src into array dst over the Region Of Interest roi.
///
/// This function is similar to vipEval(). Unlike vipEval(), vipEvalResetDst() will
/// reset the destination array based on source expression if needed.
/// 
/// vipEvalResetDst() is used by VipNDArray construct and assignement from a functor expression.
/// 
template<class Dst, class Src, class OverRoi = VipInfinitRoi, bool Err = true>
bool vipEvalResetDst(const Dst& _dst, const Src& src, const OverRoi& roi = {}, detail::CError<Err> err = {})
{
	Dst& dst = const_cast<Dst&>(_dst);
	using dst_type = detail::ValueType_t<Dst>;
	using src_type = detail::ValueType_t<Src>;

	if constexpr (!std::is_same_v<dst_type, detail::NullType> || detail::IsArrayAlgorithm<Src>::value)
		// Dst has a static type: just call vipEval.
		// Likewise, do not reset output array for array algorithms.
		return vipEval(dst, src, roi, err);
	else if constexpr (!std::is_same_v<src_type, detail::NullType>) {
		// Src type known at compile time
		dst.reset(src.shape(), src.dataType());
		return vipEval(VipArrayView<src_type>(dst), src, roi, err);
	}
	else {
		// Dst is a VipNDArray, we can reset it to the input type
		
		// Rebind src to dst type
		switch (src.dataType()) {
			case QMetaType::Char:
				return detail::evalResetDst<char>(dst, src, roi, err);
			case QMetaType::SChar:
				return detail::evalResetDst<signed char>(dst, src, roi, err);
			case QMetaType::UChar:
				return detail::evalResetDst<unsigned char>(dst, src, roi, err);
			case QMetaType::Short:
				return detail::evalResetDst<qint16>(dst, src, roi, err);
			case QMetaType::UShort:
				return detail::evalResetDst<quint16>(dst, src, roi, err);
			case QMetaType::Int:
				return detail::evalResetDst<qint32>(dst, src, roi, err);
			case QMetaType::UInt:
				return detail::evalResetDst<quint32>(dst, src, roi, err);
			case QMetaType::Long:
				return detail::evalResetDst<long>(dst, src, roi, err);
			case QMetaType::ULong:
				return detail::evalResetDst<unsigned long>(dst, src, roi, err);
			case QMetaType::LongLong:
				return detail::evalResetDst<qint64>(dst, src, roi, err);
			case QMetaType::ULongLong:
				return detail::evalResetDst<quint64>(dst, src, roi, err);
			case QMetaType::Float:
				return detail::evalResetDst<float>(dst, src, roi, err);
			case QMetaType::Double:
				return detail::evalResetDst<double>(dst, src, roi, err);
			case QMetaType::QString:
				return detail::evalResetDst<QString>(dst, src, roi, err);
			case QMetaType::QByteArray:
				return detail::evalResetDst<QByteArray>(dst, src, roi, err);
			default:
				break;
		}
		if (src.dataType() == qMetaTypeId<long double>())
			return detail::evalResetDst<long double>(dst, src, roi, err);
		if (src.dataType() == qMetaTypeId<complex_f>())
			return detail::evalResetDst<complex_f>(dst, src, roi, err);
		if (src.dataType() == qMetaTypeId<complex_d>())
			return detail::evalResetDst<complex_d>(dst, src, roi, err);
		if (src.dataType() == qMetaTypeId<VipRGB>())
			return detail::evalResetDst<VipRGB>(dst, src, roi, err);
		if (src.dataType() == qMetaTypeId<VipRGBf>())
			return detail::evalResetDst<VipRGBf>(dst, src, roi, err);
	}
	return false;
}
	


/// @brief Apply an accumulator function to an array expression.
/// @param fun Accumulator function. Signature: T(T prev_value, U new_value)
/// @param start_value Start accumulator value
/// @param src Input array expression
/// @param roi Restrict accumulator evaluation to given Region Of Interest
/// @return The accumulator result wrapped in a std::optional in case vipEval() returns false.
template<class Fun, class T, class Src, class OverRoi = VipInfinitRoi>
std::optional<T> vipAccumulate(const Fun& f, const T& start_value, const Src& src, const OverRoi& roi = {})
{
	auto acc = vipAccumulator(f, start_value);
	if (vipEval(acc, src, roi))
		return std::optional<T>{ acc.value };
	else
		return {};
}

/// @}
// end DataType

#endif