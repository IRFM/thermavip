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
	template<class OverRoi>
	struct Eval
	{
		template<class Dst, class Src>
		static bool apply(Dst& dst, const Src& src, const OverRoi& roi)
		{
			using dtype = ValueType_t<Dst>;

			qsizetype size = dst.size();
			dtype* ptr = (dtype*)dst.constHandle()->dataPointer(VipNDArrayShape());
			if (!ptr)
				return false;

			if ((Dst::access_type & Vip::Flat) && (Src::access_type & Vip::Flat) && (OverRoi::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided() && roi.isUnstrided()) {
				const Src s = src;
				for (qsizetype i = 0; i < size; ++i)
					if (roi[i])
						ptr[i] = vipCast<dtype>(s[i]);
				return true;
			}

			if ((Dst::access_type & Vip::Flat) && dst.isUnstrided()) {
				if (src.shape().size() == 1) {
					const qsizetype w = src.shape()[0];
					VipCoordinate<1> p = { { 0 } };
					for (p[0] = 0; p[0] < w; ++p[0])
						if (roi(p))
							ptr[p[0]] = vipCast<dtype>(src(p));
				}
				else if (src.shape().size() == 2) {
					const qsizetype h = src.shape()[0];
					const qsizetype w = src.shape()[1];
					qsizetype i = 0;
					VipCoordinate<2> p = { { 0, 0 } };
					for (p[0] = 0; p[0] < h; ++p[0])
						for (p[1] = 0; p[1] < w; ++p[1], ++i)
							if (roi(p))
								ptr[i] = vipCast<dtype>(src(p));
				}
				else if (src.shape().size() == 3) {
					const qsizetype z = src.shape()[0];
					const qsizetype h = src.shape()[1];
					const qsizetype w = src.shape()[2];
					qsizetype i = 0;
					VipCoordinate<3> p = { { 0, 0, 0 } };
					for (p[0] = 0; p[0] < z; ++p[0])
						for (p[1] = 0; p[1] < h; ++p[1])
							for (p[2] = 0; p[2] < w; ++p[2], ++i)
								if (roi(p))
									ptr[i] = vipCast<dtype>(src(p));
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos))
							ptr[i] = vipCast<dtype>(src(iter.pos));
						iter.increment();
					}
				}
				return true;
			}

			if ((Src::access_type & Vip::Flat) && src.isUnstrided()) {
				if (src.shape().size() == 1) {
					const qsizetype w = src.shape()[0];
					VipCoordinate<1> p = { { 0 } };
					for (p[0] = 0; p[0] < w; ++p[0])
						if (roi(p))
							ptr[dst.stride(0) * p[0]] = vipCast<dtype>(src[p[0]]);
				}
				else if (src.shape().size() == 2) {
					// const Src s = src;
					const qsizetype h = src.shape()[0];
					const qsizetype w = src.shape()[1];
					VipCoordinate<2> p = { { 0, 0 } };
					qsizetype i = 0;
					for (p[0] = 0; p[0] < h; ++p[0])
						for (p[1] = 0; p[1] < w; ++p[1], ++i)
							if (roi(p))
								ptr[dst.stride(0) * p[0] + dst.stride(1) * p[1]] = vipCast<dtype>(src[i]);
				}
				else if (src.shape().size() == 3) {
					const qsizetype z = src.shape()[0];
					const qsizetype h = src.shape()[1];
					const qsizetype w = src.shape()[2];
					VipCoordinate<3> p = { { 0, 0, 0 } };
					qsizetype i = 0;
					for (p[0] = 0; p[0] < z; ++p[0])
						for (p[1] = 0; p[1] < h; ++p[1])
							for (p[2] = 0; p[2] < w; ++p[2], ++i)
								if (roi(p))
									ptr[dst.stride(0) * p[0] + dst.stride(1) * p[1] + dst.stride(2) * p[2]] = vipCast<dtype>(src[i]);
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos))
							ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src[i]);
						iter.increment();
					}
				}
				return true;
			}

			if (src.shape().size() == 1) {
				const qsizetype w = src.shape()[0];
				VipCoordinate<1> p = { { 0 } };
				for (p[0] = 0; p[0] < w; ++p[0])
					if (roi(p))
						ptr[dst.stride(0) * p[0]] = vipCast<dtype>(src(p));
			}
			else if (src.shape().size() == 2) {
				const qsizetype h = src.shape()[0];
				const qsizetype w = src.shape()[1];
				VipCoordinate<2> p = { { 0, 0 } };
				for (p[0] = 0; p[0] < h; ++p[0])
					for (p[1] = 0; p[1] < w; ++p[1])
						if (roi(p))
							ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
			}
			else if (src.shape().size() == 3) {
				const qsizetype z = src.shape()[0];
				const qsizetype h = src.shape()[1];
				const qsizetype w = src.shape()[2];
				VipCoordinate<3> p = { { 0, 0, 0 } };
				for (p[0] = 0; p[0] < z; ++p[0])
					for (p[1] = 0; p[1] < h; ++p[1])
						for (p[2] = 0; p[2] < w; ++p[2])
							if (roi(p))
								ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
			}
			else {
				CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
				for (qsizetype i = 0; i < size; ++i) {
					if (roi(iter.pos))
						ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src(iter.pos));
					iter.increment();
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
			if (src.isEmpty())
				return false;

			using dtype = ValueType_t<Dst>;
			const qsizetype size = dst.size();
			dtype* ptr = (dtype*)dst.constHandle()->dataPointer(VipNDArrayShape());
			if (!ptr)
				return false;

			if (size > 4096 && vipIterateThreadCount() > 1) { // parallel access
				if ((Src::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided()) {
					VIP_PARALLEL_FOR
					for (qsizetype i = 0; i < size; ++i)
						ptr[i] = (src[i]);
					return true;
				}
				else {
					if (src.shape().size() == 1) {
						const qsizetype w = src.shape()[0];
						VIP_PARALLEL_FOR
						for (qsizetype i = 0; i < w; ++i)
							ptr[i * dst.stride(0)] = src(vip_vector(i));
					}
					else if (src.shape().size() == 2) {
						const qsizetype h = src.shape()[0];
						const qsizetype w = src.shape()[1];
						VIP_PARALLEL_FOR
						for (qsizetype y = 0; y < h; ++y)
							for (qsizetype x = 0; x < w; ++x)
								ptr[y * dst.stride(0) + x * dst.stride(1)] = src(vip_vector(y, x));
					}
					else if (src.shape().size() == 3) {
						const qsizetype l = src.shape()[0];
						const qsizetype h = src.shape()[1];
						const qsizetype w = src.shape()[2];
						VIP_PARALLEL_FOR
						for (qsizetype z = 0; z < l; ++z)
							for (qsizetype y = 0; y < h; ++y)
								for (qsizetype x = 0; x < w; ++x)
									ptr[z * dst.stride(0) + y * dst.stride(1) + x * dst.stride(2)] = src(vip_vector(z, y, x));
					}
					else {
						vip_iter_parallel_fmajor(dst.shape(), c) ptr[vipFlatOffset<false>(dst.strides(), c)] = src(c);
					}
					return true;
				}
			}

			if ((Src::access_type & Vip::Flat) && dst.isUnstrided() && src.isUnstrided()) {
				const Src s = src;
				for (qsizetype i = 0; i < size; ++i)
					ptr[i] = (s[i]);
				return true;
			}

			if (dst.isUnstrided()) {
				if (src.shape().size() == 1) {
					const qsizetype w = src.shape()[0];
					VipCoordinate<1> p = { { 0 } };
					for (p[0] = 0; p[0] < w; ++p[0]) {
						ptr[p[0]] = (src(p));
					}
				}
				else if (src.shape().size() == 2) {
					const qsizetype h = src.shape()[0];
					const qsizetype w = src.shape()[1];
					qsizetype i = 0;
					VipCoordinate<2> p = { { 0, 0 } };
					for (p[0] = 0; p[0] < h; ++p[0])
						for (p[1] = 0; p[1] < w; ++p[1], ++i)
							ptr[i] = (src(p));
				}
				else if (src.shape().size() == 3) {
					const qsizetype z = src.shape()[0];
					const qsizetype h = src.shape()[1];
					const qsizetype w = src.shape()[2];
					qsizetype i = 0;
					VipCoordinate<3> p = { { 0, 0, 0 } };
					for (p[0] = 0; p[0] < z; ++p[0])
						for (p[1] = 0; p[1] < h; ++p[1])
							for (p[2] = 0; p[2] < w; ++p[2], ++i)
								ptr[i] = (src(p));
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (qsizetype i = 0; i < size; ++i) {
						ptr[i] = (src(iter.pos));
						iter.increment();
					}
				}
				return true;
			}

			if ((Src::access_type & Vip::Flat) && src.isUnstrided()) {
				if (src.shape().size() == 1) {
					const qsizetype w = src.shape()[0];
					for (qsizetype i = 0; i < w; ++i)
						ptr[dst.stride(0) * i] = (src[i]);
				}
				else if (src.shape().size() == 2) {
					const qsizetype h = src.shape()[0];
					const qsizetype w = src.shape()[1];
					qsizetype i = 0;
					for (qsizetype y = 0; y < h; ++y)
						for (qsizetype x = 0; x < w; ++x, ++i)
							ptr[dst.stride(0) * y + dst.stride(1) * x] = (src[i]);
				}
				else if (src.shape().size() == 3) {
					const qsizetype z = src.shape()[0];
					const qsizetype h = src.shape()[1];
					const qsizetype w = src.shape()[2];
					qsizetype i = 0;
					for (qsizetype d = 0; d < z; ++d)
						for (qsizetype y = 0; y < h; ++y)
							for (qsizetype x = 0; x < w; ++x, ++i)
								ptr[dst.stride(0) * d + dst.stride(1) * y + dst.stride(2) * x] = (src[i]);
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					for (qsizetype i = 0; i < size; ++i) {
						ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = (src[i]);
						iter.increment();
					}
				}
				return true;
			}

			if (src.shape().size() == 1) {
				const qsizetype w = src.shape()[0];
				VipCoordinate<1> p = { { 0 } };
				for (p[0] = 0; p[0] < w; ++p[0])
					ptr[dst.stride(0) * p[0]] = (src(p));
			}
			else if (src.shape().size() == 2) {
				const qsizetype h = src.shape()[0];
				const qsizetype w = src.shape()[1];
				VipCoordinate<2> p = { { 0, 0 } };
				for (p[0] = 0; p[0] < h; ++p[0])
					for (p[1] = 0; p[1] < w; ++p[1])
						ptr[vipFlatOffset<false>(dst.strides(), p)] = (src(p));
			}
			else if (src.shape().size() == 3) {
				const qsizetype z = src.shape()[0];
				const qsizetype h = src.shape()[1];
				const qsizetype w = src.shape()[2];
				VipCoordinate<3> p = { { 0, 0, 0 } };
				for (p[0] = 0; p[0] < z; ++p[0])
					for (p[1] = 0; p[1] < h; ++p[1])
						for (p[2] = 0; p[2] < w; ++p[2])
							ptr[vipFlatOffset<false>(dst.strides(), p)] = (src(p));
			}
			else {
				CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
				for (qsizetype i = 0; i < size; ++i) {
					ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = (src(iter.pos));
					iter.increment();
				}
			}
			return true;
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
			using dtype = ValueType_t<Dst>;
			dtype* ptr = (dtype*)dst.constHandle()->dataPointer(VipNDArrayShape());
			if (!ptr)
				return false;

			if (roi.size() == 0)
				return false;
			if (roi.rects()[0].dimCount() != src.shape().size())
				return false;

			if (roi.rects()[0].dimCount() == 1) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipCoordinate<1> p = { { 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0]) {
						if (roi(p))
							ptr[p[0] * dst.stride(0)] = vipCast<dtype>(src(p));
					}
				}
			}
			else if (roi.rects()[0].dimCount() == 2) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipCoordinate<2> p = { { 0, 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0])
						for (p[1] = rect.start(1); p[1] < rect.end(1); ++p[1]) {
							if (roi(p))
								ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
						}
				}
			}
			else if (roi.rects()[0].dimCount() == 3) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipCoordinate<3> p = { { 0, 0, 0 } };
					for (p[0] = rect.start(0); p[0] < rect.end(0); ++p[0])
						for (p[1] = rect.start(1); p[1] < rect.end(1); ++p[1])
							for (p[2] = rect.start(2); p[2] < rect.end(2); ++p[2]) {
								if (roi(p))
									ptr[vipFlatOffset<false>(dst.strides(), p)] = vipCast<dtype>(src(p));
							}
				}
			}
			else {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(rect.shape());
					iter.pos = rect.start();
					qsizetype size = rect.shapeSize();
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos))
							ptr[vipFlatOffset<false>(dst.strides(), iter.pos)] = vipCast<dtype>(src(iter.pos));
						iter.increment();
					}
				}
			}
			return true;
		}
	};

	template<bool Err>
	struct CError
	{
	};

}// end detail


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

	if (dst.shape() != src.shape())
		return false;

	detail::ContextHelper ctx;

	if constexpr (std::is_base_of_v<VipNDArray, Src>) {
		return src.convert(dst);
	}
	else if constexpr (!std::is_same_v<dst_type, detail::NullType>) {
		if constexpr (!std::is_same_v<src_type, detail::NullType>) {
			if constexpr (Err) {

				static_assert(VipIsCastable_v<src_type, dst_type>, "cannot cast input functor type to destination array type");
				return detail::Eval<OverRoi>::apply(dst, vipCast<dst_type>(src), roi);
			}
			else {
				if constexpr (VipIsCastable_v<src_type, dst_type>)
					return detail::Eval<OverRoi>::apply(dst, vipCast<dst_type>(src), roi);
				else
					return false;
			}
		}
		else {
			if constexpr (!std::is_same_v<detail::NullType, decltype(detail::rebindExpression<dst_type>(std::declval<Src&>()))>)
				// Rebind src to dst type
				return vipEval(dst, detail::rebindExpression<dst_type>(src),roi, detail::CError<false>{});
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
		if (dst.dataType() == qMetaTypeId<VipRGB>())
			return vipEval(VipNDArrayTypeView<VipRGB>(dst), src, roi, detail::CError<false>{});
		if (dst.dataType() == qMetaTypeId<QImage>())
			return vipEval(VipNDArrayTypeView<VipRGB>(dst), src, roi, detail::CError<false>{});
	}

	return false;
}

/// @}
// end DataType

#endif