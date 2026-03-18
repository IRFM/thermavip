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

#ifndef VIP_RESIZE_H
#define VIP_RESIZE_H

#include "VipNDArrayOperations.h"
#include "VipMath.h"

/// \addtogroup DataType
/// @{

namespace detail
{
	template<class D, class S>
	auto cast(const S& v)
	{
		return Convert<D, S>::apply(v);
	}

	struct Resize
	{
		template<class SrcIt, class DstIt>
		static void apply(SrcIt src, DstIt dst, qsizetype src_size, qsizetype dst_size)
		{
			using dst_type = typename std::iterator_traits<DstIt>::value_type;
			if (src_size == dst_size) {
				SrcIt end = src + src_size;
				for (; src != end; ++src, ++dst)
					*dst = cast<dst_type>(*src);
				return;
			}

			double dx = (double)(src_size - 1u) / (double)(dst_size - 1u);
			double x = 0.5;

			for (qsizetype i = 0; i < dst_size; ++i, x += dx) {
				dst[i] = cast<dst_type>(src[(qsizetype)x]);
			}
		}
	};

	struct ResizeLinear
	{
		template<class SrcIt, class DstIt>
		static void apply(SrcIt src, DstIt dst, qsizetype src_size, qsizetype dst_size)
		{
			using dst_type = typename std::iterator_traits<DstIt>::value_type;
			if (src_size == dst_size) {
				SrcIt end = src + src_size;
				for (; src != end; ++src, ++dst)
					*dst = cast<dst_type>(*src);
				return;
			}

			double dx = (double)(src_size - 1) / (double)(dst_size - 1);
			double x = dx;

			SrcIt src_it = src;
			DstIt dst_it = dst;
			DstIt dst_end = dst + dst_size;
			*dst_it = cast<dst_type>(*src_it);
			--dst_end;
			++dst_it;
			for (; dst_it != dst_end; ++dst_it, x += dx) {
				if (x >= 1.0) {
					qsizetype xx = (qsizetype)x;
					src_it += xx;
					x -= (double)xx;
				}
				double x1 = 1.0 - x;

				*dst_it = cast<dst_type>(x1 * (*src_it) + x * (*(src_it + 1)));
			}

			dst[dst_size - 1] = cast<dst_type>(src[src_size - 1]);
		}
	};

	struct CubicInterpolation
	{
		template<class T>
		auto operator()(T _y0, T _y1, T _y2, T _y3, double mu) const
		{
			auto y0 = _y0 * 1.;
			auto y1 = _y1 * 1.;
			auto y2 = _y2 * 1.;
			auto y3 = _y3 * 1.;
			return y1 + 0.5 * mu * (y2 - y0 + mu * (2.0 * y0 - 5.0 * y1 + 4.0 * y2 - y3 + mu * (3.0 * (y1 - y2) + y3 - y0)));
		}
	};
	/* struct SplineInterpolation
	{
		template<class T>
		auto operator()(T _y0, T _y1, T _y2, T _y3, double mu) const
		{
			double mu2 = mu * mu;
			auto y0 = _y0 * 1.;
			auto y1 = _y1 * 1.;
			auto y2 = _y2 * 1.;
			auto y3 = _y3 * 1.;
			auto a0 = y3 - y2 - y0 + y1;
			auto a1 = y0 - y1 - a0;
			auto a2 = y2 - y0;
			return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + y1);
		}
	};
	struct CatmullRomInterpolation
	{
		template<class T>
		auto operator()(T _y0, T _y1, T _y2, T _y3, double mu) const
		{
			double mu2 = mu * mu;
			auto y0 = _y0 * 1.;
			auto y1 = _y1 * 1.;
			auto y2 = _y2 * 1.;
			auto y3 = _y3 * 1.;
			return 0.5 * ((2. * y1) + (-y0 + y2) * mu + (2. * y0 - 5. * y1 + 4. * y2 - y3) * mu2 + (-y0 + 3. * y1 - 3. * y2 + y3) * mu2 * mu);
		}
	};*/

	template<class T>
	inline T clamp(T value, T min, T max)
	{
		return min > value ? min : (max < value ? max : value);
	}

	template<class Interpolation>
	struct ResizeCubic
	{
		template<class SrcIt, class DstIt>
		static void apply(SrcIt src, DstIt dst, qsizetype src_size, qsizetype dst_size)
		{
			using dst_type = typename std::iterator_traits<DstIt>::value_type;

			if (src_size == dst_size) {
				SrcIt end = src + src_size;
				for (; src != end; ++src, ++dst)
					*dst = cast<dst_type>(*src);
				return;
			}

			Interpolation interp;

			double dx = (double)(src_size - 1) / (double)(dst_size - 1);
			double x = dx;

			dst[0] = cast<dst_type>(src[0]);
			dst[dst_size - 1] = cast<dst_type>(src[src_size - 1]);

			qsizetype start = 1 / dx + 1;

			for (qsizetype i = 1; i < start; ++i, x += dx) {
				qsizetype floor_x = (qsizetype)x;
				qsizetype y1 = floor_x;
				qsizetype y0 = floor_x;
				qsizetype y2 = floor_x + 1;
				qsizetype y3 = clamp(floor_x + 2, (qsizetype)0, src_size - 1);
				double mu = x - (double)floor_x;
				dst[i] = cast<dst_type>(interp(src[y0], src[y1], src[y2], src[y3], mu));
			}

			for (qsizetype i = start; i < dst_size - 1; ++i, x += dx) {
				qsizetype floor_x = (qsizetype)x;
				qsizetype y1 = floor_x;
				qsizetype y0 = floor_x - 1;
				qsizetype y2 = floor_x + 1;
				qsizetype y3 = clamp(floor_x + 2, (qsizetype)0, src_size - 1);
				double mu = x - (double)floor_x;
				dst[i] = cast<dst_type>(interp(src[y0], src[y1], src[y2], src[y3], mu));
			}
		}
	};

	template<class ResizeLine>
	struct ResizeLinear2D
	{
		template<class Src, class Dst>
		static void apply(const Src&, Dst&)
		{
		}
	};
	template<>
	struct ResizeLinear2D<detail::ResizeLinear>
	{
		template<class Src, class Dst>
		static void apply(const Src& src, Dst& dst)
		{
			typedef typename Src::value_type src_value_type;
			typedef typename Dst::value_type dst_value_type;

			const double x_ratio = ((double)(src.shape(1) - 1)) / (dst.shape(1) - 1);
			const double y_ratio = ((double)(src.shape(0) - 1)) / (dst.shape(0) - 1);
			const src_value_type* _s = src.ptr();
			dst_value_type* _d = dst.ptr();
			const qsizetype src_stride0 = src.stride(0);
			const qsizetype dst_stride0 = dst.stride(0);
			const qsizetype src_stride1 = src.stride(1);
			const qsizetype dst_stride1 = dst.stride(1);
			const qsizetype src_w_1 = src.shape(1) - 1;
			const qsizetype src_h_1 = src.shape(0) - 1;
			const qsizetype dst_w = dst.shape(1);
			const qsizetype dst_h = dst.shape(0);
			
			VIP_PARALLEL_FOR_NUM_THREADS(vipLoopThreadCount(dst_h))
			for (qsizetype i = 0; i < dst_h; ++i) {
				double dy = i * y_ratio;
				const qsizetype y = (qsizetype)(dy);
				const double y_diff = dy - y;
				const double _1_y_diff = 1. - y_diff;
				dst_value_type* target = _d + i * dst_stride0;
				double dx = 0.;
				for (qsizetype j = 0; j < dst_w; ++j, dx += x_ratio) {
					const qsizetype x = (qsizetype)(dx);
					const double x_diff = dx - x;
					const double _1_x_diff = 1. - x_diff;
					const qsizetype off_right = x >= src_w_1 ? 0 : src_stride1;
					const qsizetype off_bottom = y >= src_h_1 ? 0 : src_stride0;
					const src_value_type* a = _s + y * src_stride0 + x * src_stride1;

					target[j * dst_stride1] = cast<dst_value_type>(*a * (_1_x_diff) * (_1_y_diff) + a[off_right] * (x_diff) * (_1_y_diff) + a[off_bottom] * (y_diff) * (_1_x_diff) +
										       a[off_right + off_bottom] * (x_diff * y_diff));
				}
			}
		}
	};

	template<qsizetype Dim, class ResizeLine, class Src, class Dst>
	void resize(const Src& src, Dst& dst)
	{
		// typedef std::vector<typename Src::value_type> TmpLine;
		typedef VipCoordinate<Dim> CoordType;
		typedef typename Src::value_type src_value_type;
		typedef typename Dst::value_type dst_value_type;
		typedef const typename Src::value_type* SrcLineUnstrided;
		typedef typename Dst::value_type* DstLineUnstrided;
		typedef VipLineIterator<const typename Src::value_type> SrcLine;
		typedef VipLineIterator<typename Dst::value_type> DstLine;
		
		
		if (src.shapeCount() == 1) {
			if (src.stride(0) == 1 && dst.stride(0) == 1)
				ResizeLine::apply(SrcLineUnstrided(src.ptr()), DstLineUnstrided(dst.ptr()), src.size(), dst.size());
			else if (src.stride(0) != 1 && dst.stride(0) != 1)
				ResizeLine::apply(SrcLine(src.ptr(), src.stride(0)), DstLine(dst.ptr(), dst.stride(0)), src.size(), dst.size());
			else if (src.stride(0) == 1)
				ResizeLine::apply(SrcLineUnstrided(src.ptr()), DstLine(dst.ptr(), dst.stride(0)), src.size(), dst.size());
			else if (dst.stride(0) == 1)
				ResizeLine::apply(SrcLine(src.ptr(), src.stride(0)), DstLineUnstrided(dst.ptr()), src.size(), dst.size());
			return;
		}
		else if (src.shapeCount() == 2) {
			if constexpr (std::is_same<ResizeLine, detail::Resize>::value) {

				const qsizetype src_w = src.shape(1);
				const qsizetype src_h = src.shape(0);
				const qsizetype dst_w = dst.shape(1);
				const qsizetype dst_h = dst.shape(0);
				const double dx = (double)(src_w - 1u) / (double)(dst_w - 1u);
				const double dy = (double)(src_h - 1u) / (double)(dst_h - 1u);
				const src_value_type* _s = src.ptr();
				dst_value_type* _d = dst.ptr();
				const qsizetype src_stride0 = src.stride(0);
				const qsizetype dst_stride0 = dst.stride(0);
				const qsizetype src_stride1 = src.stride(1);
				const qsizetype dst_stride1 = dst.stride(1);

				VIP_PARALLEL_FOR_NUM_THREADS(vipLoopThreadCount(dst_h))
				for (qsizetype h = 0; h < dst_h; ++h) {
					double x = 0.5;
					double y = 0.5 + h * dy;
					for (qsizetype w = 0; w < dst_w; ++w, x += dx)
						_d[h * dst_stride0 + w * dst_stride1] = cast<dst_value_type>(_s[(qsizetype)y * src_stride0 + (qsizetype)x * src_stride1]);
				}
				return;
			}
			else if constexpr (std::is_same<ResizeLine, detail::ResizeLinear>::value) {
				// We use a struct instead of directly put the code here to avoid generating the linear implementation for types like QColor
				//(which won't compile because of missing operators)
				ResizeLinear2D<ResizeLine>::apply(src, dst);
				return;
			}
		}
		
		CoordType old_shape;
		old_shape = src.shape();
		CoordType new_shape;
		new_shape = dst.shape();
		CoordType tmp_shape;
		tmp_shape = (old_shape);

		// 2 temporary arrays
		VipNDArrayType<typename Dst::value_type> tmp[2];

		for (qsizetype index = 0; index < src.shapeCount(); ++index) {
			tmp_shape[index] = new_shape[index];

			VipNDArrayTypeView<typename Dst::value_type> tmp_dst;

			// select and resize destination array
			if (index == src.shapeCount() - 1) {
				// last dimension: resize directly into dst
				tmp_dst.reset(dst.ptr(), dst.shape(), dst.strides());
			}
			else {
				tmp[index % 2].reset(tmp_shape);
				tmp_dst.reset(tmp[index % 2].ptr(), tmp_shape);
			}

			CIteratorFMajorSkipDim<CoordType> it(tmp_shape, index);
			qsizetype iter_count = it.totalIterationCount();

			qsizetype threads = vipLoopThreadCount(iter_count);
			qsizetype chunk_size = iter_count / threads;
			VIP_PARALLEL_FOR_NUM_THREADS(threads)
			for (qsizetype th = 0; th < threads; ++th) {
				auto iter = it;
				if (th)
					iter.setFlatPosition(th * chunk_size);
				qsizetype count = th == threads - 1 ? (iter_count - chunk_size * (threads - 1)) : chunk_size;
				for (qsizetype i = 0; i < chunk_size; ++i) {
					if (index == 0) {
						ResizeLine::apply(
						  SrcLine(src.ptr(iter.pos), src.stride(index)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index), dst.shape(index));
					}
					else {
						VipNDArrayType<typename Dst::value_type>* tmp_src = &tmp[!(index % 2)];
						ResizeLine::apply(
						  DstLine(tmp_src->ptr(iter.pos), tmp_src->stride(index)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index), dst.shape(index));
					}
					iter.increment();
				}
			}

			/* CIteratorFMajorSkipDim<CoordType> iter(tmp_shape, index);
			qsizetype iter_count = iter.totalIterationCount();
			for (qsizetype pos = 0; pos < iter_count; ++pos) {
				if (index == 0) {
					ResizeLine::apply(SrcLine(src.ptr(iter.pos), src.stride(index)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index), dst.shape(index));
				}
				else {
					VipNDArrayType<typename Dst::value_type>* tmp_src = &tmp[!(index % 2)];
					ResizeLine::apply(DstLine(tmp_src->ptr(iter.pos), tmp_src->stride(index)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index),
			dst.shape(index));
				}
				iter.increment();
			}*/
		}
	}

	struct ResizeInternal
	{
		Vip::InterpolationType inter;

		template<class Dst, class Src>
		bool operator()(Dst& d, const Src& s, std::enable_if_t<VipIsCastable_v<typename Src::value_type, typename Dst::value_type>, void>* = nullptr) const
		{
			using intype = typename Src::value_type;
			using outype = typename Dst::value_type;
			if constexpr (!(std::is_arithmetic_v<intype> || VipIsComplex_v<intype> || VipIsRgb_v<intype>) ||
				      !(std::is_arithmetic_v<outype> || VipIsComplex_v<outype> || VipIsRgb_v<outype>)) {
				// Unsupported interpolation
				detail::resize<Vip::None, detail::Resize>(s, d);
			}
			else {
				if (inter == Vip::NoInterpolation)
					detail::resize<Vip::None, detail::Resize>(s, d);
				else if (inter == Vip::LinearInterpolation)
					detail::resize<Vip::None, detail::ResizeLinear>(s, d);
				else
					detail::resize<Vip::None, detail::ResizeCubic<detail::CubicInterpolation>>(s, d);
			}
			return true;
		}
	};

}

template<class Src, class Dst>
bool vipResize(const Src& src, Dst& dst, Vip::InterpolationType inter)
{
	if (src.isEmpty() || dst.isEmpty() || src.shapeCount() != dst.shapeCount())
		return false;

	return vipEval(dst, vipArrayAlgorithm(detail::ResizeInternal{inter}, src));
}


/* template<class Src, class Dst>
void vipResizeNoInterpolation(const Src& src, Dst& dst)
{
	detail::resize<Vip::None, detail::Resize>(src, dst);
}

template<class Src, class Dst>
void vipResizeLinear(const Src& src, Dst& dst)
{
	detail::resize<Vip::None, detail::ResizeLinear>(src, dst);
}

template<class Src, class Dst>
void vipResizeCubic(const Src& src, Dst& dst)
{
	detail::resize<Vip::None, detail::ResizeCubic<detail::CubicInterpolation>>(src, dst);
}
*/


/// @}
// end DataType

#endif
