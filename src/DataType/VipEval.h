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

#include "VipNDArray.h"
#include "VipNDArrayOperations.h"
#include "VipOverRoi.h"

namespace detail
{
	struct NullRoi
	{
		static constexpr qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
		using value_type = bool ;
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
						ptr[dst.flatIndex(vip_vector(i))] = src(vip_vector(i));
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

	template<class Dst, class Src, class Roi>
	bool evalInternal(Dst& dst, const Src& src, const Roi& roi)
	{
		if constexpr (std::is_base_of_v<BaseReductor,Dst>) {
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
		else if constexpr (IsArrayAlgorithm<Src>::value) {
			(void)roi;
			// Check if the array algorithm is callable with given dst array type
			if constexpr (Src::template callable<Dst>())
				return src.apply(dst);
			else
				return false;
		}
		else {

			if constexpr (!(Src::access_type & Vip::Cwise) ) {
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

	template<bool Err>
	struct CError
	{
	};

} // end detail

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
/// - invalid functor expression for the destination type (trying to convolve an array of QString will return false).
template<class Dst, class Src, class OverRoi = VipInfinitRoi, bool Err = true>
bool vipEval(const Dst& _dst, const Src& src, const OverRoi& roi = {}, detail::CError<Err> = {})
{
	Dst& dst = const_cast<Dst&>(_dst);
	using dst_type = detail::ValueType_t<Dst>;
	using src_type = detail::ValueType_t<Src>;

	if constexpr (std::is_base_of_v<VipNDArray, Src> && std::is_base_of_v<VipNDArray, Dst>) {
		return src.convert(dst);
	}
	else if constexpr (!std::is_same_v<dst_type, detail::NullType>) {
		if constexpr (!std::is_same_v<src_type, detail::NullType>) {
			if constexpr (Err) {

				static_assert(VipIsCastable_v<src_type, dst_type>, "cannot cast input functor type to destination array type");
				if constexpr (!std::is_same_v<src_type, dst_type>)
					return detail::evalInternal(dst, vipCast<dst_type>(src), roi);
				else
					return detail::evalInternal(dst, src, roi);
			}
			else {
				if constexpr (VipIsCastable_v<src_type, dst_type>) {
					if constexpr (!std::is_same_v<src_type, dst_type>)
						return detail::evalInternal(dst, vipCast<dst_type>(src), roi);
					else
						return detail::evalInternal(dst, src, roi);
				}
				else
					return false;
			}
		}
		else {
			using rebing_src = detail::RebindType_t<dst_type, Src>;
			if constexpr (!std::is_same_v<detail::NullType, rebing_src>) {
				if constexpr (std::is_same_v<detail::NullType, detail::ValueType_t<rebing_src>>) {
					static_assert(false, "failed to find a valid rebindExpression() overload");
				}
				else {

					if constexpr (std::is_base_of_v<detail::BaseReductor, Dst> && std::is_same_v<VipNDArray, Src>) {
						// Reduce a VipNDArray: switch over types and cast to avoid an allocation
						switch (src.dataType()) {
							case QMetaType::Char:
								return vipEval(dst, VipNDArrayTypeView<char>(src), roi, detail::CError<false>{});
							case QMetaType::SChar:
								return vipEval(dst, VipNDArrayTypeView<signed char>(src), roi, detail::CError<false>{});
							case QMetaType::UChar:
								return vipEval(dst, VipNDArrayTypeView<quint8>(src), roi, detail::CError<false>{});
							case QMetaType::Short:
								return vipEval(dst, VipNDArrayTypeView<qint16>(src), roi, detail::CError<false>{});
							case QMetaType::UShort:
								return vipEval(dst, VipNDArrayTypeView<quint16>(src), roi, detail::CError<false>{});
							case QMetaType::Int:
								return vipEval(dst, VipNDArrayTypeView<qint32>(src), roi, detail::CError<false>{});
							case QMetaType::UInt:
								return vipEval(dst, VipNDArrayTypeView<quint32>(src), roi, detail::CError<false>{});
							case QMetaType::Long:
								return vipEval(dst, VipNDArrayTypeView<long>(src), roi, detail::CError<false>{});
							case QMetaType::ULong:
								return vipEval(dst, VipNDArrayTypeView<unsigned long>(src), roi, detail::CError<false>{});
							case QMetaType::LongLong:
								return vipEval(dst, VipNDArrayTypeView<qint64>(src), roi, detail::CError<false>{});
							case QMetaType::ULongLong:
								return vipEval(dst, VipNDArrayTypeView<quint64>(src), roi, detail::CError<false>{});
							case QMetaType::Float:
								return vipEval(dst, VipNDArrayTypeView<float>(src), roi, detail::CError<false>{});
							case QMetaType::Double:
								return vipEval(dst, VipNDArrayTypeView<double>(src), roi, detail::CError<false>{});
							case QMetaType::QString:
								return vipEval(dst, VipNDArrayTypeView<QString>(src), roi, detail::CError<false>{});
							case QMetaType::QByteArray:
								return vipEval(dst, VipNDArrayTypeView<QString>(src), roi, detail::CError<false>{});
							default:
								break;
						}
						if (src.dataType() == qMetaTypeId<long double>())
							return vipEval(dst, VipNDArrayTypeView<long double>(src), roi, detail::CError<false>{});
						if (src.dataType() == qMetaTypeId<complex_f>())
							return vipEval(dst, VipNDArrayTypeView<complex_f>(src), roi, detail::CError<false>{});
						if (src.dataType() == qMetaTypeId<complex_d>())
							return vipEval(dst, VipNDArrayTypeView<complex_d>(src), roi, detail::CError<false>{});
						if (src.dataType() == qMetaTypeId<VipRGB>() || src.dataType() == qMetaTypeId<QImage>())
							return vipEval(dst, VipNDArrayTypeView<VipRGB>(src), roi, detail::CError<false>{});
						if (src.dataType() == qMetaTypeId<VipRGBf>())
							return vipEval(dst, VipNDArrayTypeView<VipRGBf>(src), roi, detail::CError<false>{});
					}
					else {

						// Use context helper to avoid converting the same array many times
						detail::ContextHelper ctx;
						// Rebind src to dst type
						return vipEval(dst, detail::rebindExpression<dst_type>(src), roi, detail::CError<false>{});
					}
				}
			}
			else
				return false;
		}
	}
	else {
		if constexpr (!std::is_same_v<src_type, detail::NullType>) {
			if (dst.dataType() == src.dataType()) {
				// Same src and dst data types
				return vipEval(VipNDArrayTypeView<src_type>(dst), src, roi);
			}
		}

		// Unknown dst types, test all dst data type without triggering compilation error
		switch (dst.dataType()) {
			case QMetaType::Char:
				return vipEval(VipNDArrayTypeView<char>(dst), src, roi, detail::CError<false>{});
			case QMetaType::SChar:
				return vipEval(VipNDArrayTypeView<signed char>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Short:
				return vipEval(VipNDArrayTypeView<qint16>(dst), src, roi, detail::CError<false>{});
			case QMetaType::UShort:
				return vipEval(VipNDArrayTypeView<quint16>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Int:
				return vipEval(VipNDArrayTypeView<qint32>(dst), src, roi, detail::CError<false>{});
			case QMetaType::UInt:
				return vipEval(VipNDArrayTypeView<quint32>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Long:
				return vipEval(VipNDArrayTypeView<long>(dst), src, roi, detail::CError<false>{});
			case QMetaType::ULong:
				return vipEval(VipNDArrayTypeView<unsigned long>(dst), src, roi, detail::CError<false>{});
			case QMetaType::LongLong:
				return vipEval(VipNDArrayTypeView<qint64>(dst), src, roi, detail::CError<false>{});
			case QMetaType::ULongLong:
				return vipEval(VipNDArrayTypeView<quint64>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Float:
				return vipEval(VipNDArrayTypeView<float>(dst), src, roi, detail::CError<false>{});
			case QMetaType::Double:
				return vipEval(VipNDArrayTypeView<double>(dst), src, roi, detail::CError<false>{});
			case QMetaType::QByteArray:
				return vipEval(VipNDArrayTypeView<QByteArray>(dst), src, roi, detail::CError<false>{});
			case QMetaType::QString:
				return vipEval(VipNDArrayTypeView<QString>(dst), src, roi, detail::CError<false>{});
			default:
				break;
		}
		if (dst.dataType() == qMetaTypeId<long double>())
			return vipEval(VipNDArrayTypeView<long double>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<complex_f>())
			return vipEval(VipNDArrayTypeView<complex_f>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<complex_d>())
			return vipEval(VipNDArrayTypeView<complex_d>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<VipRGB>() || dst.dataType() == qMetaTypeId<QImage>())
			return vipEval(VipNDArrayTypeView<VipRGB>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<VipRGBf>())
			return vipEval(VipNDArrayTypeView<VipRGBf>(dst), src, roi, detail::CError<false>{});
	}

	return false;
}

/// @}
// end DataType

#endif