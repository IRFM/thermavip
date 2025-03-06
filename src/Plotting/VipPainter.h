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

#ifndef VIP_PAINTER_H
#define VIP_PAINTER_H

#include "VipCoordinateSystem.h"
#include "VipGlobals.h"
#include <qline.h>
#include <qpalette.h>
#include <qpen.h>

/// \addtogroup Plotting
/// @{

class QTextDocument;
class QPainterPath;
class QPainter;
class QBrush;
class QColor;
class QWidget;
class QPolygonF;
class QRectF;
class QImage;
class QPixmap;
class VipScaleMap;
class VipColorMap;
class VipInterval;
class VipText;

/// \brief A collection of QPainter workarounds
class VIP_PLOTTING_EXPORT VipPainter
{
public:
	/// \brief En/Disable line splitting for the raster paint engine
	///
	/// In some Qt versions the raster paint engine paints polylines of many points
	/// much faster when they are split in smaller chunks: f.e all supported Qt versions
	/// >= Qt 5.0 when drawing an antialiased polyline with a pen width >=2.
	///
	/// The default setting is true.
	///
	/// \sa polylineSplitting()
	static void setPolylineSplitting(bool);
	static bool polylineSplitting();

	/// Enable whether coordinates should be rounded, before they are painted
	/// to a paint engine that floors to integer values. For other paint engines
	/// this ( PDF, SVG ), this flag has no effect.
	/// VipPainter stores this flag only, the rounding itself is done in
	/// the painting code ( f.e the plot items ).
	///
	/// The default setting is true.
	///
	/// \sa roundingAlignment(), isAligning()
	static void setRoundingAlignment(bool);
	static bool roundingAlignment();
	static bool roundingAlignment(QPainter*);

	static bool isOpenGL(QPainter* painter);
	static bool isVectoriel(QPainter* painter);

	static QSize screenResolution();

	static void drawText(QPainter*, double x, double y, const QString&);
	static void drawText(QPainter*, const QPointF&, const QString&);
	static void drawText(QPainter*, double x, double y, double w, double h, int flags, const QString&);
	static void drawText(QPainter*, const QRectF&, int flags, const QString&);

#ifndef QT_NO_RICHTEXT
	/// Draw a text document into a rectangle
	///
	/// \param painter VipPainter
	/// \param rect Traget rectangle
	/// \param flags Alignments/VipText flags, see QPainter::drawText()
	/// \param text VipText document
	static void drawSimpleRichText(QPainter*, const QRectF&, int flags, const QTextDocument&);
#endif

	static void drawRect(QPainter*, double x, double y, double w, double h);
	static void drawRect(QPainter*, const QRectF& rect);
	static void fillRect(QPainter*, const QRectF&, const QBrush&);
	static void drawRoundedRect(QPainter*, const QRectF& rect, double x_radius, double y_radius);

	static void drawEllipse(QPainter*, const QRectF&);
	static void drawPie(QPainter*, const QRectF& r, int a, int alen);

	static void drawLine(QPainter*, double x1, double y1, double x2, double y2);
	static void drawLine(QPainter*, const QPointF& p1, const QPointF& p2);
	static void drawLine(QPainter*, const QLineF&);

	static void drawLineRounded(QPainter*, double x1, double y1, double x2, double y2);
	static void drawLineRounded(QPainter*, const QPointF& p1, const QPointF& p2);

	static void drawPolygon(QPainter*, const QPolygonF&);
	static void drawPolyline(QPainter*, const QPolygonF&);
	static void drawPolyline(QPainter*, const QPointF*, int pointCount);

	static void drawPolygon(QPainter*, const QPolygon&);
	static void drawPolyline(QPainter*, const QPolygon&);
	static void drawPolyline(QPainter*, const QPoint*, int pointCount);

	static void drawPoint(QPainter*, const QPoint&);
	static void drawPoints(QPainter*, const QPolygon&);
	static void drawPoints(QPainter*, const QPoint*, int pointCount);

	static void drawPoint(QPainter*, double x, double y);
	static void drawPoint(QPainter*, const QPointF&);
	static void drawPoints(QPainter*, const QPolygonF&);
	static void drawPoints(QPainter*, const QPointF*, int pointCount);

	static void drawPath(QPainter*, const QPainterPath&);
	static void drawPath(QPainter*, const QPainterPath&, const QPolygonF&);
	static void drawImage(QPainter*, const QRectF&, const QImage&);
	static void drawPixmap(QPainter*, const QRectF&, const QPixmap&);

	static void drawImage(QPainter* painter, const QPolygonF& target, const QImage& image, const QRectF& src);
	static void drawPixmap(QPainter* painter, const QPolygonF& target, const QPixmap& pixmap, const QRectF& src);

	static void drawHandle(QPainter* painter, const QRectF& hRect, Qt::Orientation orientation, const QPalette& palette, int borderWidth);
	static void drawGrip(QPainter* painter, const QRectF& hRect);

	/// Draw a round frame
	///
	/// \param painter VipPainter
	/// \param rect Frame rectangle
	/// \param palette QPalette::WindowText is used for plain borders
	///              QPalette::Dark and QPalette::Light for raised
	///              or sunken borders
	/// \param lineWidth Line width
	/// \param frameStyle bitwise OR\B4ed value of QFrame::VipShape and QFrame::Shadow
	static void drawRoundFrame(QPainter*, const QRectF&, const QPalette&, int lineWidth, int frameStyle);

	static void drawRoundedFrame(QPainter*, const QRectF&, double xRadius, double yRadius, const QPalette&, int lineWidth, int frameStyle);

	static void drawFrame(QPainter*, const QRectF& rect, const QPalette& palette, QPalette::ColorRole foregroundRole, int lineWidth, int midLineWidth, int frameStyle);

	static void drawFocusRect(QPainter*, const QWidget*);
	static void drawFocusRect(QPainter*, const QWidget*, const QRect&);

	static void drawColorBar(QPainter* painter, VipColorMap&, const VipInterval&, const VipScaleMap&, Qt::Orientation, const QRectF&, QImage* pixmap = nullptr);

	/// Check if the painter is using a paint engine, that aligns
	/// coordinates to integers. Today these are all paint engines
	/// beside QPaintEngine::Pdf and QPaintEngine::SVG.
	///
	/// If we have an integer based paint engine it is also
	/// checked if the painter has a transformation matrix,
	/// that rotates or scales.
	///
	/// \param  painter VipPainter
	/// \return true, when the painter is aligning
	///
	/// \sa setRoundingAlignment()
	static bool isAligning(QPainter* painter);

	/// Check is the application is running with the X11 graphics system
	/// that has some special capabilities that can be used for incremental
	/// painting to a widget.
	///
	/// \return True, when the graphics system is X11
	static bool isX11GraphicsSystem();

	static void fillPixmap(const QWidget*, QPixmap&, const QPoint& offset = QPoint());

	static void drawBackgound(QPainter* painter, const QRectF& rect, const QWidget* widget);

	static QPixmap backingStore(QWidget*, const QSize&);

	static void drawText(QPainter* painter,
			     const VipCoordinateSystemPtr& m,
			     const VipText& text,
			     double textRotation,
			     Vip::RegionPositions textPosition,
			     Qt::Alignment textAlignment,
			     vip_double xLeft,
			     vip_double xRight,
			     vip_double yTop,
			     vip_double yBottom);

	static void drawText(QPainter* painter,
			     const VipText& t,
			     const QTransform& text_tr,
			     const QPointF& ref,
			     double textDistance,
			     Vip::RegionPositions textPosition,
			     Qt::Alignment textAlignment,
			     const QRectF& geometry);

	static QTransform resetTransform(QPainter* painter);

private:
	static void resizePolygon(int size);

	static bool d_polylineSplitting;
	static bool d_roundingAlignment;
	static QPolygonF d_polygon;
};

//!  Wrapper for QPainter::drawPoint()
inline void VipPainter::drawPoint(QPainter* painter, double x, double y)
{
	VipPainter::drawPoint(painter, QPointF(x, y));
}

//! Wrapper for QPainter::drawPoints()
inline void VipPainter::drawPoints(QPainter* painter, const QPolygon& polygon)
{
	drawPoints(painter, polygon.data(), polygon.size());
}

//! Wrapper for QPainter::drawPoints()
inline void VipPainter::drawPoints(QPainter* painter, const QPolygonF& polygon)
{
	drawPoints(painter, polygon.data(), polygon.size());
}

inline void VipPainter::drawLineRounded(QPainter* painter, double x1, double y1, double x2, double y2)
{
	drawLineRounded(painter, QPointF(x1, y1), QPointF(x2, y2));
}

//!  Wrapper for QPainter::drawLine()
inline void VipPainter::drawLine(QPainter* painter, double x1, double y1, double x2, double y2)
{
	VipPainter::drawLine(painter, QPointF(x1, y1), QPointF(x2, y2));
}

//!  Wrapper for QPainter::drawLine()
inline void VipPainter::drawLine(QPainter* painter, const QLineF& line)
{
	VipPainter::drawLine(painter, line.p1(), line.p2());
}

/// \return True, when line splitting for the raster paint engine is enabled.
/// \sa setPolylineSplitting()
inline bool VipPainter::polylineSplitting()
{
	return d_polylineSplitting;
}

/// Check whether coordinates should be rounded, before they are painted
/// to a paint engine that rounds to integer values. For other paint engines
/// ( PDF, SVG ), this flag has no effect.
///
/// \return True, when rounding is enabled
/// \sa setRoundingAlignment(), isAligning()
inline bool VipPainter::roundingAlignment()
{
	return d_roundingAlignment;
}

/// \return roundingAlignment() && isAligning(painter);
/// \param painter VipPainter
inline bool VipPainter::roundingAlignment(QPainter* painter)
{
	return d_roundingAlignment && isAligning(painter);
}

/// @}
// end Plotting

#endif
