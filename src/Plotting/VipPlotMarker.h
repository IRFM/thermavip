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

#ifndef VIP_PLOT_MARKER_H
#define VIP_PLOT_MARKER_H

#include <qbrush.h>
#include <qfont.h>
#include <qpen.h>
#include <qstring.h>

#include "VipPlotItem.h"
#include "VipText.h"

/// \addtogroup Plotting
/// @{

class VipSymbol;

/// \brief A class for drawing markers
///
/// A marker can be a horizontal line, a vertical line,
/// a symbol, a label or any combination of them, which can
/// be drawn around a center point inside a bounding rectangle.
///
/// The setSymbol() member assigns a symbol to the marker.
/// The symbol is drawn at the specified point.
///
/// With setLabel(), a label can be assigned to the marker.
/// The setLabelAlignment() member specifies where the label is
/// drawn. All the Align*-constants in Qt::AlignmentFlags (see Qt documentation)
/// are valid. The interpretation of the alignment depends on the marker's
/// line style. The alignment refers to the center point of
/// the marker, which means, for example, that the label would be printed
/// left above the center point if the alignment was set to
/// Qt::AlignLeft | Qt::AlignTop.
///
/// VipPlotMarker supports stylesheets and defines additional attributes:
/// -   'style': line style, one of 'noLine', 'HLine', 'VLine', 'cross'
/// -   'symbol': marker's symbol, one of 'none', 'ellipse', 'rect', 'diamond', ....
/// -   'symbol-size': symbol size in item's coordinates, with width==height
/// -   'label-alignment': equivalent to VipPlotMarker::setLabelAlignment(), combination of 'left|right|top|bottom|center|hcenter|vcenter'
/// -   'label-orientation' equivalent to VipPlotMarker::setLabelOrientation(), one of 'vertical' or 'horizontal'
/// -   'spacing': floating point value, equivalent to VipPlotMarker::setSpacing()
/// -   'expand-to-full-area': equivalent to VipPlotMarker::setExpandToFullArea()
///
/// The line pen is controlled with attribute 'border'.
///
/// VipPlotMarker alos defines the selectors 'noline', 'hline', 'vline' and 'cross'
///
class VIP_PLOTTING_EXPORT VipPlotMarker : public VipPlotItemDataType<VipPoint>
{
	Q_OBJECT

public:
	/// Line styles.
	/// \sa setLineStyle(), lineStyle()
	enum LineStyle
	{
		//! No line
		NoLine,

		//! A horizontal line
		HLine,

		//! A vertical line
		VLine,

		//! A crosshair
		Cross
	};
	//! Sets alignment to Qt::AlignCenter, and style to VipPlotMarker::NoLine
	explicit VipPlotMarker(const VipText& title = VipText());
	virtual ~VipPlotMarker();

	void setLineStyle(LineStyle st);
	LineStyle lineStyle() const;

	/// Build and assign a line pen
	///
	/// In Qt5 the default pen width is 1.0 ( 0.0 in Qt4 ) what makes it
	/// non cosmetic ( see QPen::isCosmetic() ). This method has been introduced
	/// to hide this incompatibility.
	///
	/// \param color Pen color
	/// \param width Pen width
	/// \param style Pen style
	///
	/// \sa pen(), brush()
	void setLinePen(const QColor&, qreal width = 0.0, Qt::PenStyle = Qt::SolidLine);

	/// Specify a pen for the line.
	///
	/// \param pen New pen
	/// \sa linePen()
	void setLinePen(const QPen& p);
	const QPen& linePen() const;
	QPen& linePen();

	virtual QColor majorColor() const { return linePen().color(); }
	virtual void setMajorColor(const QColor& c) { linePen().setColor(c); }

	virtual void setPen(const QPen& p) { setLinePen(p); }
	virtual QPen pen() const { return linePen(); }

	virtual void setBrush(const QBrush& b)
	{
		// set the label background brush
		VipText t = label();
		t.setBackgroundBrush(b);
		setLabel(t);
	}
	virtual QBrush brush() const { return label().backgroundBrush(); }

	void setSymbol(VipSymbol*);
	VipSymbol* symbol() const;

	void setSymbolVisible(bool);
	bool symbolVisible() const;

	void setLabel(const VipText&);
	VipText label() const;

	virtual void setTextStyle(const VipTextStyle&);
	virtual VipTextStyle textStyle() const;

	/// \brief Set the alignment of the label
	///
	/// In case of VipPlotMarker::HLine the alignment is relative to the
	/// y position of the marker, but the horizontal flags correspond to the
	/// canvas rectangle. In case of VipPlotMarker::VLine the alignment is
	/// relative to the x position of the marker, but the vertical flags
	/// correspond to the canvas rectangle.
	///
	/// In all other styles the alignment is relative to the marker's position.
	///
	/// \param align Alignment.
	/// \sa labelAlignment(), labelOrientation()
	void setLabelAlignment(Qt::Alignment);
	Qt::Alignment labelAlignment() const;

	/// \brief Set the orientation of the label
	///
	/// When orientation is Qt::Vertical the label is rotated by 90.0 degrees
	/// ( from bottom to top ).
	///
	/// \param orientation Orientation of the label
	///
	/// \sa labelOrientation(), setLabelAlignment()
	void setLabelOrientation(Qt::Orientation);
	Qt::Orientation labelOrientation() const;

	/// \brief Set the spacing
	///
	/// When the label is not centered on the marker position, the spacing
	/// is the vipDistance between the position and the label.
	///
	/// \param spacing Spacing
	/// \sa spacing(), setLabelAlignment()
	void setSpacing(int);
	int spacing() const;

	double relativeFontSize() const;
	void setRelativeFontSize(double size, int axis);
	void disableRelativeFontSize();

	/// If set to true (default), the marker line will expend to the full plotting area, not only the marker axises.
	/// This is especially relevent for a marker inside a #VipVMultiPlotArea2D object.
	void setExpandToFullArea(bool);
	bool expandToFullArea();
	/// Draw the marker
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QList<VipText> legendNames() const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int) const;

	virtual QList<VipInterval> plotBoundingIntervals() const;

protected:
	virtual bool hasState(const QByteArray& state, bool enable) const;
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	virtual void drawLines(QPainter*, const QList<VipInterval>&, const VipCoordinateSystemPtr&, const VipPoint&) const;
	virtual void drawLabel(QPainter*, const QRectF&, const VipCoordinateSystemPtr&, const QPointF&) const;

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotMarker*)

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotMarker* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotMarker* value);

/// @}
// end Plotting

#endif
