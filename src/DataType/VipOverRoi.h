#pragma once

#include <qvector.h>
#include <qregion.h>

#include "VipArrayBase.h"
#include "VipNDRect.h"

/// \addtogroup DataType
/// @{


/// Default Region Of Interest (ROI) for #vipEval and #vipReduce functions
/// using this ROI  will result in effectively eval or reduce a functor expression on its whole shape.
struct VipInfinitRoi
{
	static const int access_type = Vip::Flat | Vip::Position;
	typedef bool value_type;
	template< class ShapeType> bool  operator()(const ShapeType &) const { return true; }
	bool operator[](int) const { return true; }
	bool isUnstrided() const { return true; }
};

/// A Region Of Interest (ROI) mixing a list of rectangles and another ROI.
/// Using this structure within #vipEval or #vipReduce functions will result in
/// evaluating the functor expression on the rectangles only, and only if given ROI
/// is true.
template<int Dim = Vip::None, class OverRoi = VipInfinitRoi>
class VipOverNDRects
{
	const QVector<VipNDRect<Dim> > m_rects;
	const VipNDRect<Dim> * m_ptr;
	const int m_size;
	const OverRoi m_roi;
public:
	static const int access_type = OverRoi::access_type;
	typedef bool value_type;

	VipOverNDRects(const QVector<VipNDRect<Dim> > & rects, const OverRoi & roi = OverRoi())
		:m_rects(rects), m_ptr(m_rects.data()), m_size(m_rects.size()), m_roi(roi)
	{}
	VipOverNDRects(const VipNDRect<Dim> * ptr, int size, const OverRoi & roi = OverRoi())
		:m_ptr(ptr), m_size(size), m_roi(roi)
	{}

	const VipNDRect<Dim> * rects() const { return m_ptr; }
	int size() const { return m_size; }

	template< class ShapeType> bool  operator()(const ShapeType & p) const { return m_roi(p); }
	bool operator[](int i) const { return m_roi[i]; }
	bool isUnstrided() const { return false; }
};

/// Create a #VipOverNDRects object based on a vector of rectangles and a ROI (default to #VipInfinitRoi)
template< int NDim, class OverRoi = VipInfinitRoi>
VipOverNDRects<NDim, OverRoi> vipOverRects(const QVector<VipNDRect<NDim> > & rects, const OverRoi & roi = OverRoi())
{
	return VipOverNDRects<NDim, OverRoi>(rects, roi);
}

/// Create a #VipOverNDRects object based on a vector of rectangles and a ROI (default to #VipInfinitRoi).
/// Note that the rectangles are NOT copied must not be freed before the VipOverNDRects usage.
template< int NDim, class OverRoi = VipInfinitRoi>
VipOverNDRects<NDim, OverRoi> vipOverRects(const VipNDRect<NDim> * rects, int size, const OverRoi & roi = OverRoi())
{
	return VipOverNDRects<NDim, OverRoi>(rects, size, roi);
}

/// Create a #VipOverNDRects object on a rectangle and a ROI (default to #VipInfinitRoi).
template< int NDim, class OverRoi = VipInfinitRoi>
VipOverNDRects<NDim, OverRoi> vipOverRects(const VipNDRect<NDim> & rect, const OverRoi & roi = OverRoi())
{
	return VipOverNDRects<NDim, OverRoi>(&rect, 1, roi);
}

/// Create a #VipOverNDRects object on a vector of QRect and a ROI (default to #VipInfinitRoi).
template< class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QVector<QRect> & rects, const OverRoi & roi = OverRoi())
{
	return VipOverNDRects<2, OverRoi>(reinterpret_cast<const QVector<VipNDRect<2> > &>(rects), roi);
}

/// Create a #VipOverNDRects object on a vector of QRect and a ROI (default to #VipInfinitRoi).
/// Note that the rectangles are NOT copied must not be freed before the VipOverNDRects usage.
template< class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QRect * rects, int size, const OverRoi & roi = OverRoi())
{
	return VipOverNDRects<2, OverRoi>((const VipNDRect<2>*)rects, size, roi);
}

/// Create a #VipOverNDRects object on a QRect and a ROI (default to #VipInfinitRoi).
template< class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QRect & rect, const OverRoi & roi = OverRoi())
{
	return VipOverNDRects<2, OverRoi>((const VipNDRect<2>*)&rect, 1, roi);
}

/// Create a #VipOverNDRects object on a QRegion and a ROI (default to #VipInfinitRoi).
template< class OverRoi = VipInfinitRoi>
VipOverNDRects<2, OverRoi> vipOverRects(const QRegion & reg, const OverRoi & roi = OverRoi())
{
	return vipOverRects(reg.rects(), roi);
}

/// @}
//end DataType
