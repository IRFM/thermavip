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

#include "VipPlotMarker.h"
#include "VipAbstractScale.h"
#include "VipPainter.h"
#include "VipPlotWidget2D.h"
#include "VipScaleDraw.h"
#include "VipSymbol.h"

#include <qpainter.h>

static int registerMarkerKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		/// -   'style': line style, one of 'noLine', 'HLine', 'VLine', 'cross'
		/// -   'text-border': text box border pen
		/// -   'text-border-radius: text box border radius
		/// -   'text-border-margin: text box border margin
		/// -   'text-color': text color
		/// -   'text-font': text font
		/// -   'symbol': marker's symbol, one of 'none', 'ellipse', 'rect', 'diamond', ....
		/// -   'symbol-size': symbol size in item's coordinates, with width==height
		/// -   'label-alignment': equivalent to VipPlotMarker::setLabelAlignment(), combination of 'left|right|top|bottom|center|hcenter|vcenter'
		/// -   'label-orientation' equivalent to VipPlotMarker::setLabelOrientation(), one of 'vertical' or 'horizontal'
		/// -   'spacing': floating point value, equivalent to VipPlotMarker::setSpacing()
		/// -   'expand-to-full-area': equivalent to VipPlotMarker::setExpandToFullArea()

		QMap<QByteArray, int> style;
		style["noLine"] = VipPlotMarker::NoLine;
		style["HLine"] = VipPlotMarker::HLine;
		style["VLine"] = VipPlotMarker::VLine;
		style["cross"] = VipPlotMarker::Cross;

		keywords["style"] = VipParserPtr(new EnumParser(style));
		keywords["symbol"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::symbolEnum()));
		keywords["symbol-size"] = VipParserPtr(new DoubleParser());
		keywords["label-alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["label-orientation"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::orientationEnum()));
		keywords["spacing"] = VipParserPtr(new DoubleParser());
		keywords["expand-to-full-area"] = VipParserPtr(new BoolParser());

		vipSetKeyWordsForClass(&VipPlotMarker::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerMarkerKeyWords = registerMarkerKeyWords();

class VipPlotMarker::PrivateData
{
public:
	PrivateData()
	  : labelAlignment(Qt::AlignCenter)
	  , labelOrientation(Qt::Horizontal)
	  , spacing(2)
	  , symbol(nullptr)
	  , symbolVisible(false)
	  , expandToFullArea(true)
	  , style(VipPlotMarker::NoLine)
	  , fontSize(0)
	  , fontAxis(-1)
	{
	}

	~PrivateData() { delete symbol; }

	VipText label;

	Qt::Alignment labelAlignment;
	Qt::Orientation labelOrientation;
	int spacing;

	QPen pen;
	VipSymbol* symbol;
	bool symbolVisible;
	bool expandToFullArea;
	LineStyle style;

	QSharedPointer<VipTextStyle> textStyle;

	double fontSize;
	int fontAxis;
};

VipPlotMarker::VipPlotMarker(const VipText& title)
  : VipPlotItemDataType(title)
{
	VIP_CREATE_PRIVATE_DATA();
	this->setItemAttribute(VisibleLegend, false);
}

//! Destructor
VipPlotMarker::~VipPlotMarker()
{
}

void VipPlotMarker::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();

	const VipPoint pos = this->rawData();
	QList<VipInterval> intervals;

	if (d_data->expandToFullArea)
		if (VipAbstractPlotArea* a = area()) {
			if (this->axes().size() == 2) {
				VipInterval x_bounds = a->areaBoundaries(this->axes()[0]);
				VipInterval y_bounds = a->areaBoundaries(this->axes()[1]);
				intervals << x_bounds << y_bounds;
			}
		}
	if (intervals.isEmpty())
		intervals = (VipAbstractScale::scaleIntervals(axes()));

	// draw lines
	drawLines(painter, intervals, m, pos);

	// draw symbol
	if (d_data->symbol && symbolVisible()) {
		d_data->symbol->drawSymbol(painter, m->transform(pos));
	}

	VipPointVector scale_rect;
	scale_rect << VipPoint(intervals[0].minValue(), intervals[1].minValue()) << VipPoint(intervals[0].maxValue(), intervals[1].minValue())
		   << VipPoint(intervals[0].maxValue(), intervals[1].maxValue()) << VipPoint(intervals[0].minValue(), intervals[1].maxValue());

	QRectF paint_rect = QPolygonF(m->transform(scale_rect)).boundingRect().normalized().adjusted(10, 10, -10, -10);
	drawLabel(painter, paint_rect, m, (QPointF)m->transform(pos));

	// if (label().hasTextBoxStyle()) {
	//   qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//   if(el > 1)
	//       vip_debug("VipPlotMarker: %i ms\n", (int)el);
	//  }
}

void VipPlotMarker::drawLines(QPainter* painter, const QList<VipInterval>& scale_rect, const VipCoordinateSystemPtr& m, const VipPoint& pos) const
{
	if (d_data->style == NoLine)
		return;

	painter->setPen((d_data->pen));
	if (d_data->style == VipPlotMarker::HLine || d_data->style == VipPlotMarker::Cross) {
		QPointF p1 = m->transform(scale_rect[0].minValue(), pos.y());
		QPointF p2 = m->transform(scale_rect[0].maxValue(), pos.y());
		VipPainter::drawLine(painter, p1, p2);
	}
	if (d_data->style == VipPlotMarker::VLine || d_data->style == VipPlotMarker::Cross) {
		QPointF p1 = m->transform(pos.x(), scale_rect[1].minValue());
		QPointF p2 = m->transform(pos.x(), scale_rect[1].maxValue());
		VipPainter::drawLine(painter, p1, p2);
	}
}

void VipPlotMarker::drawLabel(QPainter* painter, const QRectF& scale_rect, const VipCoordinateSystemPtr& m, const QPointF& pos) const
{
	Q_UNUSED(m)
	if (d_data->label.isEmpty())
		return;

	VipText label = d_data->label;

	// compute font size
	if (d_data->fontAxis >= 0 && d_data->fontSize > 0) {
		QPointF start, end;
		if (d_data->fontAxis == 0) {
			start = QPointF(0, 0);
			end = QPointF(d_data->fontSize, 0);
		}
		else {
			start = QPointF(0, 0);
			end = QPointF(0, d_data->fontSize);
		}
		start = m->transform(start);
		end = m->transform(end);
		double size = QLineF(start, end).length();
		QFont f = label.font();
		f.setPointSizeF(size);
		label.setFont(f);
	}

	Qt::Alignment align = d_data->labelAlignment;
	QPointF alignPos = pos;

	QSizeF symbolOff(0, 0);

	switch (d_data->style) {
		case VipPlotMarker::VLine: {
			// In VLine-style the y-position is pointless and
			// the alignment flags are relative to the canvas

			if (d_data->labelAlignment & Qt::AlignTop) {
				alignPos.setY(scale_rect.top());
				align &= ~Qt::AlignTop;
				align |= Qt::AlignBottom;
			}
			else if (d_data->labelAlignment & Qt::AlignBottom) {
				// In HLine-style the x-position is pointless and
				// the alignment flags are relative to the canvas

				alignPos.setY(scale_rect.bottom());
				align &= ~Qt::AlignBottom;
				align |= Qt::AlignTop;
			}
			else {
				alignPos.setY(scale_rect.center().y());
			}
			break;
		}
		case VipPlotMarker::HLine: {
			if (d_data->labelAlignment & Qt::AlignLeft) {
				alignPos.setX(scale_rect.left());
				align &= ~Qt::AlignLeft;
				align |= Qt::AlignRight;
			}
			else if (d_data->labelAlignment & Qt::AlignRight) {
				alignPos.setX(scale_rect.right());
				align &= ~Qt::AlignRight;
				align |= Qt::AlignLeft;
			}
			else {
				alignPos.setX(scale_rect.center().x());
			}
			break;
		}
		default: {
			if (d_data->symbol && symbolVisible()) {
				symbolOff = d_data->symbol->size() + QSizeF(1, 1);
				symbolOff /= 2;
			}
		}
	}

	qreal pw2 = d_data->pen.widthF() / 2.0;
	if (pw2 == 0.0)
		pw2 = 0.5;

	const int spacing = d_data->spacing;

	const qreal xOff = qMax(pw2, symbolOff.width());
	const qreal yOff = qMax(pw2, symbolOff.height());

	const QSizeF textSize = label.textSize();

	if (align & Qt::AlignLeft) {
		alignPos.rx() -= xOff + spacing;
		if (d_data->labelOrientation == Qt::Vertical)
			alignPos.rx() -= textSize.height();
		else
			alignPos.rx() -= textSize.width();
	}
	else if (align & Qt::AlignRight) {
		alignPos.rx() += xOff + spacing;
	}
	else {
		if (d_data->labelOrientation == Qt::Vertical)
			alignPos.rx() -= textSize.height() / 2;
		else
			alignPos.rx() -= textSize.width() / 2;
	}

	if (align & Qt::AlignTop) {
		alignPos.ry() -= yOff + spacing;
		if (d_data->labelOrientation != Qt::Vertical)
			alignPos.ry() -= textSize.height();
	}
	else if (align & Qt::AlignBottom) {
		alignPos.ry() += yOff + spacing;
		if (d_data->labelOrientation == Qt::Vertical)
			alignPos.ry() += textSize.width();
	}
	else {
		if (d_data->labelOrientation == Qt::Vertical)
			alignPos.ry() += textSize.width() / 2;
		else
			alignPos.ry() -= textSize.height() / 2;
	}

	painter->translate(alignPos.x(), alignPos.y());
	if (d_data->labelOrientation == Qt::Vertical)
		painter->rotate(-90.0);

	const QRectF textRect(0, 0, textSize.width(), textSize.height());
	label.draw(painter, textRect);
}

void VipPlotMarker::setLineStyle(LineStyle style)
{
	if (style != d_data->style) {
		d_data->style = style;

		emitItemChanged();
	}
}

VipPlotMarker::LineStyle VipPlotMarker::lineStyle() const
{
	return d_data->style;
}

void VipPlotMarker::setSymbol(VipSymbol* symbol)
{
	if (symbol != d_data->symbol) {
		delete d_data->symbol;
		d_data->symbol = symbol;

		emitItemChanged();
	}
}

VipSymbol* VipPlotMarker::symbol() const
{
	return d_data->symbol;
}

void VipPlotMarker::setSymbolVisible(bool vis)
{
	if (vis != d_data->symbolVisible) {
		d_data->symbolVisible = vis;
		emitItemChanged();
	}
}
bool VipPlotMarker::symbolVisible() const
{
	return d_data->symbolVisible;
}

void VipPlotMarker::setLabel(const VipText& label)
{
	d_data->label = label;
	if (d_data->textStyle)
		d_data->label.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

VipText VipPlotMarker::label() const
{
	return d_data->label;
}

void VipPlotMarker::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->label.setTextStyle(st);
	emitItemChanged();
}

VipTextStyle VipPlotMarker::textStyle() const
{
	return d_data->label.textStyle();
}

void VipPlotMarker::setLabelAlignment(Qt::Alignment align)
{
	if (align != d_data->labelAlignment) {
		d_data->labelAlignment = align;
		emitItemChanged();
	}
}

/// \return the label alignment
/// \sa setLabelAlignment(), setLabelOrientation()
Qt::Alignment VipPlotMarker::labelAlignment() const
{
	return d_data->labelAlignment;
}

void VipPlotMarker::setLabelOrientation(Qt::Orientation orientation)
{
	if (orientation != d_data->labelOrientation) {
		d_data->labelOrientation = orientation;
		emitItemChanged();
	}
}

/// \return the label orientation
/// \sa setLabelOrientation(), labelAlignment()
Qt::Orientation VipPlotMarker::labelOrientation() const
{
	return d_data->labelOrientation;
}

void VipPlotMarker::setSpacing(int spacing)
{
	if (spacing < 0)
		spacing = 0;

	if (spacing == d_data->spacing)
		return;

	d_data->spacing = spacing;
	emitItemChanged();
}

/// \return the spacing
/// \sa setSpacing()
int VipPlotMarker::spacing() const
{
	return d_data->spacing;
}

double VipPlotMarker::relativeFontSize() const
{
	return d_data->fontSize;
}

void VipPlotMarker::setRelativeFontSize(double size, int axis)
{
	d_data->fontSize = size;
	d_data->fontAxis = axis;
	emitItemChanged();
}
void VipPlotMarker::disableRelativeFontSize()
{
	d_data->fontAxis = -1;
	emitItemChanged();
}

void VipPlotMarker::setExpandToFullArea(bool enable)
{
	d_data->expandToFullArea = enable;
	emitItemChanged();
}
bool VipPlotMarker::expandToFullArea()
{
	return d_data->expandToFullArea;
}

void VipPlotMarker::setLinePen(const QColor& color, qreal width, Qt::PenStyle style)
{
	setLinePen(QPen(color, width, style));
}

void VipPlotMarker::setLinePen(const QPen& pen)
{
	if (pen != d_data->pen) {
		d_data->pen = pen;

		emitItemChanged();
	}
}

/// \return the line pen
/// \sa setLinePen()
const QPen& VipPlotMarker::linePen() const
{
	return d_data->pen;
}
QPen& VipPlotMarker::linePen()
{
	return d_data->pen;
}

QList<VipInterval> VipPlotMarker::plotBoundingIntervals() const
{
	VipPoint pt = this->rawData();
	return QList<VipInterval>() << VipInterval(pt.x(), pt.x()) << VipInterval(pt.y(), pt.y());
}

QList<VipText> VipPlotMarker::legendNames() const
{
	return QList<VipText>() << title();
}

QRectF VipPlotMarker::drawLegend(QPainter* painter, const QRectF& rect, int) const
{

	if (rect.isEmpty())
		return QRectF();

	painter->setRenderHints(this->renderHints());

	if (d_data->style != VipPlotMarker::NoLine) {
		painter->setPen(d_data->pen);

		if (d_data->style == VipPlotMarker::HLine || d_data->style == VipPlotMarker::Cross) {
			const double y = rect.center().y();

			VipPainter::drawLine(painter, rect.left(), y, rect.right(), y);
		}

		if (d_data->style == VipPlotMarker::VLine || d_data->style == VipPlotMarker::Cross) {
			const double x = rect.center().x();

			VipPainter::drawLine(painter, x, rect.top(), x, rect.bottom());
		}
	}

	if (d_data->symbol) {
		d_data->symbol->drawSymbol(painter, rect);
	}

	return rect;
}

bool VipPlotMarker::hasState(const QByteArray& state, bool enable) const
{
	if (state == "noline")
		return (lineStyle() == NoLine) == enable;
	if (state == "hline")
		return (lineStyle() == HLine) == enable;
	if (state == "vline")
		return (lineStyle() == VLine) == enable;
	if (state == "cross")
		return (lineStyle() == Cross) == enable;
	return VipPlotItem::hasState(state, enable);
}

bool VipPlotMarker::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "style") == 0) {
		LineStyle st = (LineStyle)value.toInt();
		setLineStyle(st);
		return true;
	}
	if (strcmp(name, "symbol") == 0) {
		VipSymbol::Style st = (VipSymbol::Style)value.toInt();
		if (st == VipSymbol::None) {
			setSymbolVisible(false);
		}
		else {
			if (!symbol())
				setSymbol(new VipSymbol(st));
			else
				symbol()->setStyle(st);
			setSymbolVisible(true);
		}
		return true;
	}
	if (strcmp(name, "symbol-size") == 0) {
		if (!symbol())
			setSymbol(new VipSymbol());
		double w = value.toDouble();
		symbol()->setSize(QSizeF(w, w));
		return true;
	}
	if (strcmp(name, "label-alignment") == 0) {
		Qt::Alignment align = (Qt::Alignment)value.toInt();
		setLabelAlignment(align);
		return true;
	}
	if (strcmp(name, "label-orientation") == 0) {
		Qt::Orientation o = (Qt::Orientation)value.toInt();
		setLabelOrientation(o);
		return true;
	}
	if (strcmp(name, "spacing") == 0) {
		setSpacing(value.toDouble());
		return true;
	}
	if (strcmp(name, "expand-to-full-area") == 0) {
		setExpandToFullArea(value.toBool());
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

VipArchive& operator<<(VipArchive& arch, const VipPlotMarker* value)
{
	arch.content("lineStyle", (int)value->lineStyle())
	  .content("linePen", value->linePen())
	  .content("label", value->label())
	  .content("labelAlignment", (int)value->labelAlignment())
	  .content("labelOrientation", (int)value->labelOrientation())
	  .content("spacing", value->spacing());
	if (value->symbol())
		return arch.content("symbol", *value->symbol());
	else
		return arch.content("symbol", VipSymbol());
}

VipArchive& operator>>(VipArchive& arch, VipPlotMarker* value)
{
	value->setLineStyle((VipPlotMarker::LineStyle)arch.read("lineStyle").value<int>());
	value->setLinePen(arch.read("linePen").value<QPen>());
	value->setLabel(arch.read("label").value<VipText>());
	value->setLabelAlignment((Qt::AlignmentFlag)arch.read("labelAlignment").value<int>());
	value->setLabelOrientation((Qt::Orientation)arch.read("labelOrientation").value<int>());
	value->setSpacing(arch.read("spacing").value<double>());
	value->setSymbol(new VipSymbol(arch.read("symbol").value<VipSymbol>()));
	return arch;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipPlotMarker*>();
	vipRegisterArchiveStreamOperators<VipPlotMarker*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
