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

#ifndef VIP_OVER_ROI_H
#define VIP_OVER_ROI_H

#include <qregion.h>
#include <qvector.h>

#include "VipArrayBase.h"
#include "VipNDRect.h"

/// \addtogroup DataType
/// @{

/// Default Region Of Interest (ROI) for #vipEval and #vipReduce functions
/// using this ROI  will result in effectively eval or reduce a functor expression on its whole shape.
struct VipInfinitRoi
{
	static const qsizetype access_type = Vip::Flat | Vip::Position;
	typedef bool value_type;
	template<class ShapeType>
	bool operator()(const ShapeType&) const
	{
		return true;
	}
	bool operator[](qsizetype) const { return true; }
	bool isUnstrided() const { return true; }
};

/// A Region Of Interest (ROI) mixing a list of rectangles and another ROI.
/// Using this structure within #vipEval or #vipReduce functions will result in
/// evaluating the functor expression on the rectangles only, and only if given ROI
/// is true.
template<qsizetype Dim = Vip::None, class OverRoi = VipInfinitRoi>
class VipOverNDRects
{
	const QVector<VipNDRect<Dim>> m_rects;
	const VipNDRect<Dim>* m_ptr;
	const qsizetype m_size;
	const OverRoi m_roi;

public:
	static const qsizetype access_type = OverRoi::access_type;
	typedef bool value_type;

	VipOverNDRects(const QVector<VipNDRect<Dim>>& rects, const OverRoi& roi = OverRoi())
	  : m_rects(rects)
	  , m_ptr(m_rects.data())
	  , m_size(m_rects.size())
	  , m_roi(roi)
	{
	}
	VipOverNDRects(const VipNDRect<Dim>* ptr, qsizetype size, const OverRoi& roi = OverRoi())
	  : m_ptr(ptr)
	  , m_size(size)
	  , m_roi(roi)
	{
	}

	const VipNDRect<Dim>* rects() const { return m_ptr; }
	qsizetype size() const { return m_size; }

	template<class ShapeType>
	bool operator()(const ShapeType& p) const
	{
		return m_roi(p);
	}
	bool operator[](qsizetype i) const { return m_roi[i]; }
	bool isUnstrided() const { return false; }
};

/// Create a #VipOverNDRects object based on a vector of rectangles and a ROI (default to #VipInfinitRoi)
template<qsizetype NDim, class OverRoi = VipInfinitRoi>
VipOverNDRects<NDim, OverRoi> vipOverRects(const QVector<VipNDRect<NDim>>& rects, const OverRoi& roi = OverRoi())
{
	return VipOverNDRects<NDim, OverRoi>(rects, roi);
}

/// Create a #VipOverNDRects object based on a vector of rectangles and a ROI (default to #VipInfinitRoi).
/// Note that the rectangles are NOT copied must not be freed before the VipOverNDRects usage.
template<qsizetype NDim, class OverRoi = VipInfinitRoi>
VipOverNDRects<NDim, OverRoi> vipOverRects(const VipNDRect<NDim>* rects, qsizetype size, const OverRoi& roi = OverRoi())
{
	return VipOverNDRects<NDim, OverRoi>(rects, size, roi);
}

/// Create a #VipOverNDRects object on a rectangle and a ROI (default to #VipInfinitRoi).
template<qsizetype NDim, class OverRoi = VipInfinitRoi>
VipOverNDRects<NDim, OverRoi> vipOverRects(const VipNDRect<NDim>& rect, const OverRoi& roi = OverRoi())
{
	return VipOverNDRects<NDim, OverRoi>(&rect, 1, roi);
}

/// Create a #VipOverNDRects object on a vector of QRect and a ROI (default to #VipInfinitRoi).
template<class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QVector<QRect>& rects, const OverRoi& roi = OverRoi())
{
	return VipOverNDRects<2, OverRoi>(reinterpret_cast<const QVector<VipNDRect<2>>&>(rects), roi);
}

/// Create a #VipOverNDRects object on a vector of QRect and a ROI (default to #VipInfinitRoi).
/// Note that the rectangles are NOT copied must not be freed before the VipOverNDRects usage.
template<class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QRect* rects, qsizetype size, const OverRoi& roi = OverRoi())
{
	return VipOverNDRects<2, OverRoi>((const VipNDRect<2>*)rects, size, roi);
}

/// Create a #VipOverNDRects object on a QRect and a ROI (default to #VipInfinitRoi).
template<class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QRect& rect, const OverRoi& roi = OverRoi())
{
	return VipOverNDRects<2, OverRoi>((const VipNDRect<2>*)&rect, 1, roi);
}

/// Create a #VipOverNDRects object on a QRegion and a ROI (default to #VipInfinitRoi).
template<class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QRegion& reg, const OverRoi& roi = OverRoi())
{
	return vipOverRects(reg.rects(), roi);
}

/// @}
// end DataType

#endif