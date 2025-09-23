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

#ifndef VIP_SYMBOL_H
#define VIP_SYMBOL_H

#include <QMetaType>
#include <QPointF>
#include <QVector>
#include <QtGlobal>

#include "VipPlotUtils.h"
#include "VipPimpl.h"

/// \addtogroup Plotting
/// @{

class QPainter;
class QSizeF;
class QRectF;
class QBrush;
class QPen;
class QColor;
class QPainterPath;
class QPixmap;
class QByteArray;

//! A class for drawing symbols
class VIP_PLOTTING_EXPORT VipSymbol
{
public:
	/// VipSymbol Style
	/// \sa setStyle(), style()
	enum Style
	{
		//! No symbol
		None = -1,

		//! Ellipse or circle
		Ellipse,

		//! Rectangle
		Rect,

		//!  Diamond
		Diamond,

		//! Triangle pointing upwards
		Triangle,

		//! Triangle pointing downwards
		DTriangle,

		//! Triangle pointing upwards
		UTriangle,

		//! Triangle pointing left
		LTriangle,

		//! Triangle pointing right
		RTriangle,

		//! Cross (+)
		Cross,

		//! Diagonal cross (X)
		XCross,

		//! Horizontal line
		HLine,

		//! Vertical line
		VLine,

		//! X combined with +
		Star1,

		//! Six-pointed star
		Star2,

		//! Hexagon
		Hexagon,

		/// The symbol is represented by a painter path, where the
		/// origin ( 0, 0 ) of the path coordinate system is mapped to
		/// the position of the symbol.
		///
		/// \sa setPath(), path()
		Path,

		/// The symbol is represented by a pixmap. The pixmap is centered
		/// or aligned to its pin point.
		///
		/// \sa setPinPoint()
		Pixmap,

		/// The symbol is represented by a SVG graphic. The graphic is centered
		/// or aligned to its pin point.
		///
		/// \sa setPinPoint()
		SvgDocument,

		/// Styles >= VipSymbol::UserSymbol are reserved for derived
		/// classes of VipSymbol that overload drawSymbols() with
		/// additional application specific symbol types.
		UserStyle = 1000
	};

	/// Depending on the render engine and the complexity of the
	/// symbol shape it might be faster to render the symbol
	/// to a pixmap and to paint this pixmap.
	///
	/// F.e. the raster paint engine is a pure software renderer
	/// where in cache mode a draw operation usually ends in
	/// raster operation with the the backing store, that are usually
	/// faster, than the algorithms for rendering polygons.
	/// But the opposite can be expected for graphic pipelines
	/// that can make use of hardware acceleration.
	///
	/// The default setting is AutoCache
	///
	/// \sa setCachePolicy(), cachePolicy()
	///
	/// \note The policy has no effect, when the symbol is painted
	///     to a vector graphics format ( PDF, SVG ).
	/// \warning Since Qt 4.8 raster is the default backend on X11

	enum CachePolicy
	{
		//! Don't use a pixmap cache
		NoCache,

		//! Always use a pixmap cache
		Cache,

		/// Use a cache when one of the following conditions is true:
		///
		/// - The symbol is rendered with the software
		///  renderer ( QPaintEngine::Raster )
		AutoCache
	};

public:
	VipSymbol(Style = Ellipse);
	VipSymbol(const VipSymbol& other);
	VipSymbol(Style, const QBrush&, const QPen&, const QSizeF&);
	VipSymbol(const QPainterPath&, const QBrush&, const QPen&);

	VipSymbol& operator=(const VipSymbol& other);

	virtual ~VipSymbol();

	void setCachePolicy(CachePolicy);
	CachePolicy cachePolicy() const;

	void setSize(const QSizeF&);
	void setSize(qreal width, qreal height = -1);
	const QSizeF& size() const;

	void setPinPoint(const QPointF& pos, bool enable = true);
	QPointF pinPoint() const;

	void setPinPointEnabled(bool);
	bool isPinPointEnabled() const;

	virtual void setColor(const QColor&);

	void setBrush(const QBrush& b);
	void setBrushColor(const QColor&);
	const QBrush& brush() const;

	void setPen(const QColor&, qreal width = 0.0, Qt::PenStyle = Qt::SolidLine);
	void setPenColor(const QColor&);
	void setPen(const QPen&);
	const QPen& pen() const;

	void setStyle(Style);
	Style style() const;
	static const char* nameForStyle(Style);

	void setPixmap(const QPixmap&);
	const QPixmap& pixmap() const;

#ifndef QWT_NO_SVG
	void setSvgDocument(const QByteArray&);
#endif

	void drawSymbol(QPainter*, const QRectF&) const;
	void drawSymbol(QPainter*, const QPointF&) const;
	void drawSymbols(QPainter*, const QVector<QPointF>&) const;
	void drawSymbols(QPainter*, const QPointF*, int numPoints) const;

	QPainterPath extractShape(QBitmap* bitmap, const QRect& rect, const QPointF*, int numPoints) const;

	QPainterPath shape(const QPointF& pos = QPointF(0, 0)) const;

	virtual QRectF boundingRect() const;
	void invalidateCache();

protected:
	virtual void renderSymbols(QPainter*, const QPointF*, int numPoints) const;

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// \brief Draw the symbol at a specified position
///
/// \param painter VipPainter
/// \param pos Position of the symbol in screen coordinates
inline void VipSymbol::drawSymbol(QPainter* painter, const QPointF& pos) const
{
	drawSymbols(painter, &pos, 1);
}

/// \brief Draw symbols at the specified points
///
/// \param painter VipPainter
/// \param points Positions of the symbols in screen coordinates

inline void VipSymbol::drawSymbols(QPainter* painter, const QVector<QPointF>& points) const
{
	drawSymbols(painter, points.data(), points.size());
}

Q_DECLARE_METATYPE(VipSymbol)

class QDataStream;
VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipSymbol& s);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipSymbol& s);

/// @}
// end Plotting

#endif
