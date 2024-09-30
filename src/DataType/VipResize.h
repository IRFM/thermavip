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

#ifndef VIP_RESIZE_H
#define VIP_RESIZE_H

#include "VipNDArray.h"

/// \addtogroup DataType
/// @{

namespace detail
{
	struct Resize
	{
		template<class SrcIt, class DstIt>
		static void apply(SrcIt src, DstIt dst, int src_size, int dst_size)
		{
			if (src_size == dst_size) {
				SrcIt end = src + src_size;
				for (; src != end; ++src, ++dst)
					*dst = *src;
				return;
			}

			double dx = (double)(src_size - 1u) / (double)(dst_size - 1u);
			double x = 0.5;

			for (int i = 0; i < dst_size; ++i, x += dx) {
				dst[i] = src[(int)x];
			}
		}
	};

	struct ResizeLinear
	{
		template<class SrcIt, class DstIt>
		static void apply(SrcIt src, DstIt dst, int src_size, int dst_size)
		{
			if (src_size == dst_size) {
				SrcIt end = src + src_size;
				for (; src != end; ++src, ++dst)
					*dst = *src;
				return;
			}

			double dx = (double)(src_size - 1) / (double)(dst_size - 1);
			double x = dx;

			SrcIt src_it = src;
			DstIt dst_it = dst;
			DstIt dst_end = dst + dst_size;
			*dst_it = *src_it;
			--dst_end;
			++dst_it;
			for (; dst_it != dst_end; ++dst_it, x += dx) {
				if (x >= 1.0) {
					int xx = (int)x;
					src_it += xx;
					x -= (double)xx;
				}
				double x1 = 1.0 - x;

				*dst_it = x1 * (*src_it) + x * (*(src_it + 1));
			}

			dst[dst_size - 1] = src[src_size - 1];
		}
	};

	struct SplineInterpolation
	{
		double operator()(double y0, double y1, double y2, double y3, double mu) const
		{
			double a0, a1, a2, a3, mu2;

			mu2 = mu * mu;
			a0 = y3 - y2 - y0 + y1;
			a1 = y0 - y1 - a0;
			a2 = y2 - y0;
			a3 = y1;

			return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3);
		}

		template<class T>
		std::complex<T> operator()(std::complex<T> y0, std::complex<T> y1, std::complex<T> y2, std::complex<T> y3, double _mu) const
		{
			std::complex<T> a0, a1, a2, a3;
			const T mu = static_cast<T>(_mu);
			const T mu2 = mu * mu;

			a0 = y3 - y2 - y0 + y1;
			a1 = y0 - y1 - a0;
			a2 = y2 - y0;
			a3 = y1;

			return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3);
		}
	};

	struct CatmullRomInterpolation
	{
		double operator()(double y0, double y1, double y2, double y3, double mu) const
		{
			double mu2 = mu * mu;
			return 0.5 * ((2 * y1) + (-y0 + y2) * mu + (2 * y0 - 5 * y1 + 4 * y2 - y3) * mu2 + (-y0 + 3 * y1 - 3 * y2 + y3) * mu2 * mu);
		}
	};

	template<class T>
	inline T clamp(T value, T min, T max)
	{
		return min > value ? min : (max < value ? max : value);
	}

	template<class Interpolation>
	struct ResizeCubic
	{
		template<class SrcIt, class DstIt>
		static void apply(SrcIt src, DstIt dst, int src_size, int dst_size)
		{
			if (src_size == dst_size) {
				SrcIt end = src + src_size;
				for (; src != end; ++src, ++dst)
					*dst = *src;
				return;
			}

			Interpolation interp;

			double dx = (double)(src_size - 1) / (double)(dst_size - 1);
			double x = dx;

			dst[0] = src[0];
			dst[dst_size - 1] = src[src_size - 1];

			int start = 1 / dx + 1;

			for (int i = 1; i < start; ++i, x += dx) {
				int floor_x = (int)x;
				int y1 = floor_x;
				int y0 = floor_x;
				int y2 = floor_x + 1;
				int y3 = clamp(floor_x + 2, 0, src_size - 1);
				double mu = x - (double)floor_x;
				dst[i] = interp(src[y0], src[y1], src[y2], src[y3], mu);
			}

			for (int i = start; i < dst_size - 1; ++i, x += dx) {
				int floor_x = (int)x;
				int y1 = floor_x;
				int y0 = floor_x - 1;
				int y2 = floor_x + 1;
				int y3 = clamp(floor_x + 2, 0, src_size - 1);
				double mu = x - (double)floor_x;
				dst[i] = interp(src[y0], src[y1], src[y2], src[y3], mu);
			}
		}
	};

	template<class T, class U>
	struct is_same_interp
	{
		enum
		{
			value = false
		};
	};
	template<class T>
	struct is_same_interp<T, T>
	{
		enum
		{
			value = true
		};
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
			const int src_stride0 = src.stride(0);
			const int dst_stride0 = dst.stride(0);
			const int src_stride1 = src.stride(1);
			const int dst_stride1 = dst.stride(1);
			const int src_w_1 = src.shape(1) - 1;
			const int src_h_1 = src.shape(0) - 1;
			const int dst_w = dst.shape(1);
			const int dst_h = dst.shape(0);
			double dy = 0.;
			for (int i = 0; i < dst_h; ++i, dy += y_ratio) {
				const int y = (int)(dy);
				const double y_diff = dy - y;
				const double _1_y_diff = 1. - y_diff;
				dst_value_type* target = _d + i * dst_stride0;
				double dx = 0.;
				for (int j = 0; j < dst_w; ++j, dx += x_ratio) {
					const int x = (int)(dx);
					const double x_diff = dx - x;
					const double _1_x_diff = 1. - x_diff;
					const int off_right = x >= src_w_1 ? 0 : src_stride1;
					const int off_bottom = y >= src_h_1 ? 0 : src_stride0;
					const src_value_type* a = _s + y * src_stride0 + x * src_stride1;
					const src_value_type* b = a + off_right;	      // right
					const src_value_type* c = a + off_bottom;	      // bottom
					const src_value_type* d = a + off_right + off_bottom; // bottom right

					target[j * dst_stride1] = *a * (_1_x_diff) * (_1_y_diff) + *b * (x_diff) * (_1_y_diff) + *c * (y_diff) * (_1_x_diff) + *d * (x_diff * y_diff);
				}
			}
		}
	};

	template<int Dim, class ResizeLine, class Src, class Dst>
	void resize(const Src& src, Dst& dst)
	{
		// typedef std::vector<typename Src::value_type> TmpLine;
		typedef VipHybridVector<int, Dim> CoordType;
		typedef typename Src::value_type src_value_type;
		typedef typename Dst::value_type dst_value_type;
		typedef const typename Src::value_type* SrcLineUnstrided;
		typedef typename Dst::value_type* DstLineUnstrided;
		typedef VipLineIterator<const typename Src::value_type> SrcLine;
		typedef VipLineIterator<typename Dst::value_type> DstLine;

		if (src.shapeCount() == 1) {
			if (src.stride(0) == 1 && dst.stride(0) == 1)
				ResizeLine::apply(SrcLineUnstrided(src.data()), DstLineUnstrided(dst.data()), src.size(), dst.size());
			else if (src.stride(0) != 1 && dst.stride(0) != 1)
				ResizeLine::apply(SrcLine(src.data(), src.stride(0)), DstLine(dst.data(), dst.stride(0)), src.size(), dst.size());
			else if (src.stride(0) == 1)
				ResizeLine::apply(SrcLineUnstrided(src.data()), DstLine(dst.data(), dst.stride(0)), src.size(), dst.size());
			else if (dst.stride(0) == 1)
				ResizeLine::apply(SrcLine(src.data(), src.stride(0)), DstLineUnstrided(dst.data()), src.size(), dst.size());
			return;
		}
		else if (src.shapeCount() == 2 && is_same_interp<ResizeLine, detail::Resize>::value) {
			const int src_w = src.shape(1);
			const int src_h = src.shape(0);
			const int dst_w = dst.shape(1);
			const int dst_h = dst.shape(0);
			const double dx = (double)(src_w - 1u) / (double)(dst_w - 1u);
			const double dy = (double)(src_h - 1u) / (double)(dst_h - 1u);
			double y = 0.5;
			const src_value_type* _s = src.ptr();
			dst_value_type* _d = dst.ptr();
			const int src_stride0 = src.stride(0);
			const int dst_stride0 = dst.stride(0);
			const int src_stride1 = src.stride(1);
			const int dst_stride1 = dst.stride(1);
			for (int h = 0; h < dst_h; ++h, y += dy) {
				double x = 0.5;
				for (int w = 0; w < dst_w; ++w, x += dx)
					_d[h * dst_stride0 + w * dst_stride1] = _s[(int)y * src_stride0 + (int)x * src_stride1];
			}
			return;
		}
		else if (src.shapeCount() == 2 && is_same_interp<ResizeLine, detail::ResizeLinear>::value) {
			// We use a struct instead of directly put the code here to avoid generating the linear implementation for types like QColor
			//(which won't compile because of missing operators)
			ResizeLinear2D<ResizeLine>::apply(src, dst);
			return;
		}

		CoordType old_shape;
		old_shape = src.shape();
		CoordType new_shape;
		new_shape = dst.shape();
		CoordType tmp_shape;
		tmp_shape = (old_shape);

		// 2 temporary arrays
		VipNDArrayType<typename Dst::value_type> tmp[2];

		for (int index = 0; index < src.shapeCount(); ++index) {
			tmp_shape[index] = new_shape[index];

			VipNDArrayTypeView<typename Dst::value_type> tmp_dst;

			// select and resize destination array
			if (index == src.shapeCount() - 1) {
				// last dimension: resize directly into dst
				tmp_dst.reset(dst.ptr(), dst.shape());
			}
			else {
				tmp[index % 2].reset(tmp_shape);
				tmp_dst.reset(tmp[index % 2].ptr(), tmp_shape);
			}

			CIteratorFMajorSkipDim<CoordType> iter(tmp_shape, index);
			int iter_count = iter.totalIterationCount();

			for (int pos = 0; pos < iter_count; ++pos) {
				if (index == 0) {
					ResizeLine::apply(SrcLine(src.ptr(iter.pos), src.stride(index)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index), dst.shape(index));
				}
				else if (index == src.shapeCount() - 1) {
					VipNDArrayType<typename Dst::value_type>* tmp_src = &tmp[!(index % 2)];
					ResizeLine::apply(DstLineUnstrided(tmp_src->ptr(iter.pos)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index), dst.shape(index));
				}
				else {
					VipNDArrayType<typename Dst::value_type>* tmp_src = &tmp[!(index % 2)];
					ResizeLine::apply(
					  DstLine(tmp_src->ptr(iter.pos), tmp_src->stride(index)), DstLine(tmp_dst.ptr(iter.pos), tmp_dst.stride(index)), src.shape(index), dst.shape(index));
				}
				iter.increment();
			}
		}
	}
}

template<class Src, class Dst>
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
void vipResizeSpline(const Src& src, Dst& dst)
{
	detail::resize<Vip::None, detail::ResizeCubic<detail::SplineInterpolation>>(src, dst);
}

template<class Src, class Dst>
void vipResizeCatmullRom(const Src& src, Dst& dst)
{
	detail::resize<Vip::None, detail::ResizeCubic<detail::CatmullRomInterpolation>>(src, dst);
}

/// @}
// end DataType

#endif
