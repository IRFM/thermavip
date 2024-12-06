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

#ifndef VIP_REDUCTION_H
#define VIP_REDUCTION_H

#include "VipEval.h"
#include "VipOverRoi.h"

namespace detail
{

	template<bool SrcNullType, bool ValidCast, class OverRoi = VipInfinitRoi>
	struct Reduce
	{
		template<class Reductor, class Src, class Offset>
		static bool apply(Reductor&, const Src&, const OverRoi&, const Offset&)
		{
			return false;
		}
	};

	template<class OverRoi>
	struct Reduce<false, true, OverRoi>
	{
		/// Reduce src.
		template<class Reductor, class Src, class Offset>
		static bool apply(Reductor& red, const Src& src, const OverRoi& roi, const Offset& src_offset)
		{
			VipNDArrayShape offset;
			offset.resize(src_offset.size());
			if (src_offset.size())
				offset = src_offset;
			else
				offset.fill(0);

			typedef typename Reductor::value_type type;
			if (src_offset.size() == 0 && (Src::access_type & Vip::Flat) && (Reductor::access_type & Vip::Flat) && (OverRoi::access_type & Vip::Flat) && src.isUnstrided() &&
			    roi.isUnstrided()) {
				qsizetype size = vipCumMultiply(src.shape());
				for (qsizetype i = 0; i < size; ++i) {
					if (std::is_same<VipInfinitRoi, OverRoi>::value)
						red.setAt(i, vipCast<type>(src[i]));
					else if (roi[i])
						red.setAt(i, vipCast<type>(src[i]));
				}
			}
			else if ((Src::access_type & Vip::Flat) && src.isUnstrided()) {
				if (src.shape().size() == 1) {
					qsizetype w = src.shape()[0] + offset[0];
					VipCoordinate<1> p;
					for (p[0] = offset[0]; p[0] < w; ++p[0]) {
						if (roi(p)) {
							red.setPos(p, vipCast<type>(src[p[0] - offset[0]]));
						}
					}
				}
				else if (src.shape().size() == 2) {
					qsizetype h = src.shape()[0] + offset[0];
					qsizetype w = src.shape()[1] + offset[1];
					qsizetype i = 0;
					VipCoordinate<2> p;
					for (p[0] = offset[0]; p[0] < h; ++p[0])
						for (p[1] = offset[1]; p[1] < w; ++p[1], ++i) {
							if (roi(p)) {
								red.setPos(p, vipCast<type>(src[i]));
							}
						}
				}
				else if (src.shape().size() == 3) {
					qsizetype z = src.shape()[0] + offset[0];
					qsizetype h = src.shape()[1] + offset[1];
					qsizetype w = src.shape()[2] + offset[2];
					qsizetype i = 0;
					VipCoordinate<3> p;
					for (p[0] = offset[0]; p[0] < z; ++p[0])
						for (p[1] = offset[1]; p[1] < h; ++p[1])
							for (p[2] = offset[2]; p[2] < w; ++p[2], ++i) {
								if (roi(p)) {
									red.setPos(p, vipCast<type>(src[i]));
								}
							}
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					qsizetype size = vipCumMultiply(src.shape());
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos)) {
							red.setPos(iter.pos, vipCast<type>(src[i]));
						}
						iter.increment();
					}
				}
			}
			else {
				if (src.shape().size() == 1) {
					qsizetype w = src.shape()[0] + offset[0];
					VipCoordinate<1> p;
					VipCoordinate<1> src_p;
					for (p[0] = offset[0]; p[0] < w; ++p[0]) {
						if (roi(p)) {
							src_p[0] = p[0] - offset[0];
							red.setPos(p, vipCast<type>(src(src_p)));
						}
					}
				}
				else if (src.shape().size() == 2) {
					qsizetype h = src.shape()[0] + offset[0];
					qsizetype w = src.shape()[1] + offset[1];
					VipCoordinate<2> p;
					VipCoordinate<2> src_p;
					for (p[0] = offset[0]; p[0] < h; ++p[0])
						for (p[1] = offset[1]; p[1] < w; ++p[1]) {
							if (roi(p)) {
								src_p[0] = p[0] + offset[0];
								src_p[1] = p[1] + offset[1];
								red.setPos(p, vipCast<type>(src(src_p)));
							}
						}
				}
				else if (src.shape().size() == 3) {
					qsizetype z = src.shape()[0] + offset[0];
					qsizetype h = src.shape()[1] + offset[1];
					qsizetype w = src.shape()[2] + offset[2];
					VipCoordinate<3> p;
					VipCoordinate<3> src_p;
					for (p[0] = offset[0]; p[0] < z; ++p[0])
						for (p[1] = offset[1]; p[1] < h; ++p[1])
							for (p[2] = offset[2]; p[2] < w; ++p[2]) {
								if (roi(p)) {
									src_p[0] = p[0] + offset[0];
									src_p[1] = p[1] + offset[1];
									src_p[2] = p[2] + offset[2];
									red.setPos(p, vipCast<type>(src(src_p)));
								}
							}
				}
				else {
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(src.shape());
					qsizetype size = iter.totalIterationCount();
					VipNDArrayShape pos;
					pos.resize(offset.size());
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos)) {
							if (src_offset.size() == 0)
								red.setPos(iter.pos, vipCast<type>(src(iter.pos)));
							else {
								for (qsizetype j = 0; j < src_offset.size(); ++j)
									pos[j] = iter.pos[j] + offset[j];
								red.setPos(pos, vipCast<type>(src(iter.pos)));
							}
						}
						iter.increment();
					}
				}
			}
			return true;
		}
	};

	template<qsizetype Dim>
	struct Reduce<false, true, VipOverNDRects<Dim>>
	{
		/// Reduce src.
		template<class Reductor, class Src, class Offset>
		static bool apply(Reductor& red, const Src& src, const VipOverNDRects<Dim>& roi, const Offset& src_offset)
		{
			typedef typename Reductor::value_type type;
			if (roi.size() == 0)
				return false;
			if (roi.rects()[0].dimCount() != src.shape().size())
				return false;

			VipNDArrayShape offset;
			offset.resize(roi.rects()[0].dimCount());
			if (src_offset.size())
				offset = src_offset;
			else
				offset.fill(0);

			if (roi.rects()[0].dimCount() == 1) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipCoordinate<1> p;
					VipCoordinate<1> src_p;
					const qsizetype start = std::max(offset[0], rect.start(0));
					const qsizetype end = std::min(rect.end(0), src.shape()[0] + offset[0]);
					for (p[0] = start; p[0] < end; ++p[0]) {
						if (roi(p)) {
							src_p[0] = p[0] - offset[0];
							red.setPos(p, vipCast<type>(src(src_p)));
						}
					}
				}
			}
			else if (roi.rects()[0].dimCount() == 2) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipCoordinate<2> p;
					VipCoordinate<2> src_p;
					const qsizetype start0 = std::max(offset[0], rect.start(0));
					const qsizetype end0 = std::min(rect.end(0), src.shape()[0] + offset[0]);
					const qsizetype start1 = std::max(offset[1], rect.start(1));
					const qsizetype end1 = std::min(rect.end(1), src.shape()[1] + offset[1]);
					for (p[0] = start0; p[0] < end0; ++p[0])
						for (p[1] = start1; p[1] < end1; ++p[1]) {
							if (roi(p)) {
								src_p[0] = p[0] - offset[0];
								src_p[1] = p[1] - offset[1];
								red.setPos(p, vipCast<type>(src(src_p)));
							}
						}
				}
			}
			else if (roi.rects()[0].dimCount() == 3) {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					VipCoordinate<3> p;
					VipCoordinate<3> src_p;
					const qsizetype start0 = std::max(offset[0], rect.start(0));
					const qsizetype end0 = std::min(rect.end(0), src.shape()[0] + offset[0]);
					const qsizetype start1 = std::max(offset[1], rect.start(1));
					const qsizetype end1 = std::min(rect.end(1), src.shape()[1] + offset[1]);
					const qsizetype start2 = std::max(offset[2], rect.start(2));
					const qsizetype end2 = std::min(rect.end(2), src.shape()[2] + offset[2]);
					for (p[0] = start0; p[0] < end0; ++p[0])
						for (p[1] = start1; p[1] < end1; ++p[1])
							for (p[2] = start2; p[2] < end2; ++p[2]) {
								if (roi(p)) {
									src_p[0] = p[0] - offset[0];
									src_p[1] = p[1] - offset[1];
									src_p[2] = p[2] - offset[2];
									red.setPos(p, vipCast<type>(src(src_p)));
								}
							}
				}
			}
			else {
				for (qsizetype r = 0; r < roi.size(); ++r) {
					const VipNDRect<Dim>& rect = roi.rects()[r];
					CIteratorFMajorNoSkip<VipNDArrayShape> iter(rect.shape());
					iter.pos = rect.start();
					qsizetype size = rect.shapeSize();
					VipNDArrayShape pos;
					pos.resize(offset.size());
					for (qsizetype i = 0; i < size; ++i) {
						if (roi(iter.pos)) {
							if (src_offset.size() == 0)
								red.setPos(iter.pos, vipCast<type>(src(iter.pos)));
							else {
								for (qsizetype j = 0; j < src_offset.size(); ++j)
									pos[j] = iter.pos[j] + offset[j];
								red.setPos(pos, vipCast<type>(src(iter.pos)));
							}
						}
						iter.increment();
					}
				}
			}
			return true;
		}
	};

	template<class OverRoi>
	struct Reduce<true, true, OverRoi>
	{
		template<class Reductor, class Src, class Offset = VipNDArrayShape>
		static bool apply(Reductor& dst, const Src& src, const OverRoi& roi, const Offset& off = Offset())
		{
			typedef typename Reductor::value_type dtype;
			static const bool valid = InternalCast<dtype, Src>::valid;
			// src of unkown type
			if (src.dataType() == QMetaType::Bool) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<bool, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::Char) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<char, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::SChar) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<signed char, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::UChar) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<unsigned char, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::UShort) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<quint16, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::Short) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<qint16, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::UInt) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<quint32, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::Int) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<qint32, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::ULongLong) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<quint64, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::LongLong) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<qint64, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::Long) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<long, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::ULong) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<unsigned long, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::Float) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<float, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == QMetaType::Double) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<double, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == qMetaTypeId<long double>()) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<long double, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == qMetaTypeId<complex_f>()) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<complex_f, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == qMetaTypeId<complex_d>()) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<complex_d, Src>::cast(src), roi, off);
			}
			else if (src.dataType() == qMetaTypeId<VipRGB>()) {
				return Reduce<false, valid, OverRoi>::apply(dst, InternalCast<VipRGB, Src>::cast(src), roi, off);
			}
			else
				return false;
		}
	};

	/// Base class for all reductors
	template<class T>
	struct Reductor : BaseReductor
	{
		//! input value type
		typedef T value_type;
		//! define the way input array is walked through. Depending on this value, setAt() or setPos() will be called.
		static const qsizetype access_type = Vip::Flat | Vip::Position;
		//! set a new value for given flat index
		void setAt(qsizetype, const T&) {}
		//! set a new value for given position
		template<class ShapeType>
		void setPos(const ShapeType&, const T&)
		{
		}
		//! finish the reduction algorithm, returns true on success, false otherwise.
		bool finish() { return true; }
	};

	template<class T, class = void>
	struct is_valid_reductor : std::false_type
	{
	};

	// template< class T>
	// struct is_valid_reductor<T , std::void_t<decltype(T().setAt(0,typename T::value_type()))> > :std::true_type {};

}

/// Apply given reduction algorithm (inheriting detail::BaseReductor) to \a src array or functor expression.
/// Returns true on success, false otherwise.
template<class Reductor, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
bool vipReduce(Reductor& red, const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	if (src.isEmpty())
		return false;
	bool res = detail::Reduce<detail::HasNullType<Src>::value,
				  detail::InternalCast<typename Reductor::value_type, Src>::valid,
				  OverRoi>::apply(red, // detail::InternalCast<typename Reductor::value_type, Src>::cast(src)
						  src,
						  roi,
						  off);
	if (res)
		return red.finish();
	return res;
}

namespace Vip
{
	/// Array statistic values extracted with #VipArrayStats
	enum ArrayStats
	{
		Min = 0x001,	 //! minimum element
		Max = 0x002,	 //! maximum element
		MinPos = 0x004,	 //! minimum element position
		MaxPos = 0x008,	 //! maximum element position
		Mean = 0x01,	 //! aaverage value
		Sum = 0x02,	 //! cumulative sum
		Multiply = 0x04, //! cumulative multiplication
		Std = 0x08,	 //! standard deviation
		AllStats = Min | Max | MinPos | MaxPos | Mean | Sum | Multiply | Std
	};
}

namespace detail
{
	template<class T, class = void>
	struct ComputeMinMaxStd
	{
		static bool min(T&, const T&) { return false; }
		static bool max(T&, const T&) { return false; }
		static void std(T&, T&, const T&, const T&, qsizetype) {}
	};
	template<class T>
	struct ComputeMinMaxStd<T, typename std::enable_if<has_lesser_operator<T, T>::value, void>::type>
	{
		static bool min(T& min, const T& val)
		{
			if (val < min) {
				min = val;
				return true;
			}
			return false;
		}
		static bool max(T& max, const T& val)
		{
			if (val > max) {
				max = val;
				return true;
			}
			return false;
		}
		static void std(T& std, T& var, const T& sum2, const T& mean, qsizetype count)
		{
			if (count > 1) {
				var = (sum2 - count * mean * mean) / (count - 1);
				std = std::sqrt(var);
			}
		}
	};
}

/// Reduction algorithm extracting the min, max, sum and mean values of an array or a functor expression.
/// This is also the return type of function #vipArrayStats.
///
/// Depending on the value of \a Stats, not all statistics are extracted to optimize the computation.
template<class T, int Stats = Vip::AllStats>
class VipArrayStats : public detail::Reductor<T>
{
	bool first;
	T sum2;

public:
	typedef decltype(T() * T()) value_type;
	qsizetype count;
	T min, max, mean, sum, multiply, std, var;
	VipNDArrayShape minPos, maxPos;

	VipArrayStats()
	  : first(true)
	  , sum2(0)
	  , count(0)
	  , min(0)
	  , max(0)
	  , mean(0)
	  , sum(0)
	  , multiply(0)
	  , std(0)
	  , var(0)
	{
	}
	static const qsizetype access_type = (Vip::Position | (((Stats & Vip::MinPos) | (Stats & Vip::MaxPos)) ? 0 : Vip::Flat));

	void setAt(qsizetype, const T& value)
	{
		if (vipIsNan(value))
			return;
		++count;
		if (first) {
			min = max = value;
			multiply = 1;
			first = false;
		}
		if (Stats & Vip::Min) {
			// if (value < min) min = value;
			detail::ComputeMinMaxStd<T>::min(min, value);
		}
		if (Stats & Vip::Max) {
			// if (value > max) max = value;
			detail::ComputeMinMaxStd<T>::max(max, value);
		}
		if (Stats & Vip::Multiply) {
			multiply *= value;
		}
		if (Stats & Vip::Mean || Stats & Vip::Sum || Stats & Vip::Std) {
			sum += value;
			if (Stats & Vip::Std)
				sum2 += value * value;
		}
	}
	template<class ShapeType>
	void setPos(const ShapeType& pos, const T& value)
	{
		if (vipIsNan(value))
			return;
		++count;
		if (first) {
			min = max = value;
			maxPos = minPos = pos;
			multiply = 1;
			first = false;
		}
		if (Stats & Vip::Min) {
			if (detail::ComputeMinMaxStd<T>::min(min, value))
				if (Stats & Vip::MinPos)
					minPos = pos;
		}
		if (Stats & Vip::Max) {
			if (detail::ComputeMinMaxStd<T>::max(max, value))
				if (Stats & Vip::MaxPos)
					maxPos = pos;
		}
		if (Stats & Vip::Multiply) {
			multiply *= value;
		}
		if (Stats & Vip::Mean || Stats & Vip::Sum || Stats & Vip::Std) {
			sum += value;
			if (Stats & Vip::Std)
				sum2 += value * value;
		}
	}
	bool finish()
	{
		if ((Stats & Vip::Mean) || Stats & Vip::Std) {
			if (!count)
				return false;
			mean = sum / (double)count;
			if (Stats & Vip::Std) {
				detail::ComputeMinMaxStd<T>::std(std, var, sum2, mean, count);
			}
		}

		return true;
	}
};

#include <type_traits>
#include <utility>

namespace detail
{
	template<class T>
	using try_assign = decltype(typename T::value_type());

	template<class T, template<class> class Op, class = void>
	struct is_valid : std::false_type
	{
	};

	template<class T, template<class> class Op>
	struct is_valid<T, Op, std::void_t<Op<T>>> : std::true_type
	{
	};

	template<class T>
	using is_copy_assignable = is_valid<T, try_assign>;

	template<class T, int Stats>
	struct is_valid_reductor<VipArrayStats<T, Stats>, std::void_t<decltype(T() + T())>> : std::true_type
	{
	};
}

/// Extracts the array or functor expression statistics and returns them as a VipArrayStats object.
///
/// Note that the statistics value type can be specified as template parameter and might be different from the source value type.
/// Indeed, it is better to compute the sum of a 8 bits integer array as a 32 bits integer instead of a 8 bits integer
/// to avoid overflow.
///
/// It is also advised to use floating point type when computing the mean value or the standard deviation.
template<class T, int Stats = Vip::AllStats, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
VipArrayStats<T, Stats> vipArrayStats(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	VipArrayStats<T, Stats> stats;
	if (!vipReduce(stats, src, roi, off))
		stats = VipArrayStats<T, Stats>();
	return stats;
}
/// Returns the minimum value of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
T vipArrayMin(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	return vipArrayStats<T, Vip::Min>(src, roi, off).min;
}
/// Returns the maximum value of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
T vipArrayMax(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	return vipArrayStats<T, Vip::Max>(src, roi, off).max;
}
/// Returns the cumulative sum of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
T vipArrayCumSum(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	return vipArrayStats<T, Vip::Sum>(src, roi, off).sum;
}
/// Returns the cumulative multiplication of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
T vipArrayCumMultiply(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	return vipArrayStats<T, Vip::Multiply>(src, roi, off).multiply;
}
/// Returns the mean value of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
T vipArrayMean(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	return vipArrayStats<T, Vip::Mean>(src, roi, off).mean;
}
/// Returns the standard deviation of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi, class Offset = VipNDArrayShape>
T vipArrayStd(const Src& src, const OverRoi& roi = OverRoi(), const Offset& off = VipNDArrayShape())
{
	return vipArrayStats<T, Vip::Std>(src, roi, off).std;
}

#endif
