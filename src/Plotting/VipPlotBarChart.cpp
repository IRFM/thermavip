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

#include "VipPlotBarChart.h"
#include "VipAbstractScale.h"
#include "VipAxisColorMap.h"
#include "VipBorderItem.h"
#include "VipColorMap.h"
#include "VipPainter.h"

#include <QMultiMap>

static int registerBarChartKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		QMap<QByteArray, int> style;
		style["stacked"] = VipPlotBarChart::Stacked;
		style["sideBySide"] = VipPlotBarChart::SideBySide;

		QMap<QByteArray, int> widthUnit;
		widthUnit["itemUnit"] = VipPlotBarChart::ItemUnit;
		widthUnit["axisUnit"] = VipPlotBarChart::AxisUnit;

		QMap<QByteArray, int> valueType;
		valueType["scaleValue"] = VipPlotBarChart::ScaleValue;
		valueType["barLength"] = VipPlotBarChart::BarLength;

		QMap<QByteArray, int> textValue;
		textValue["eachValue"] = VipPlotBarChart::EachValue;
		textValue["maxValue"] = VipPlotBarChart::MaxValue;
		textValue["sumValue"] = VipPlotBarChart::SumValue;

		keywords["style"] = VipParserPtr(new EnumOrParser(style));
		keywords["width-unit"] = VipParserPtr(new EnumOrParser(widthUnit));
		keywords["value-type"] = VipParserPtr(new EnumOrParser(valueType));
		keywords["text-value"] = VipParserPtr(new EnumOrParser(textValue));

		keywords["text-alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["text-position"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::regionPositionEnum()));
		keywords["text-distance"] = VipParserPtr(new DoubleParser());

		keywords["border-radius"] = VipParserPtr(new DoubleParser());
		keywords["bar-width"] = VipParserPtr(new DoubleParser());

		vipSetKeyWordsForClass(&VipPlotBarChart::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerBarChartKeyWords = registerBarChartKeyWords();

VipBar::VipBar(double pos, const QVector<double>& values)
  : d_pos(pos)
  , d_values(values)
{
}

VipBar::VipBar(const VipBar& other)
  : d_pos(other.d_pos)
  , d_values(other.d_values)
{
}

VipBar& VipBar::operator=(const VipBar& other)
{
	d_pos = other.d_pos;
	d_values = other.d_values;
	return *this;
}

void VipBar::setPosition(double x)
{
	d_pos = x;
}

double VipBar::position() const
{
	return d_pos;
}

void VipBar::setValues(const QVector<double>& values)
{
	d_values = values;
}

const QVector<double>& VipBar::values() const
{
	return d_values;
}

double VipBar::value(int index) const
{
	return d_values[index];
}

int VipBar::valueCount() const
{
	return d_values.size();
}

class VipPlotBarChart::PrivateData
{
public:
	PrivateData()
	  : spacing(0)
	  , spacingUnit(ItemUnit)
	  , width(20)
	  , widthUnit(ItemUnit)
	  , style(SideBySide)
	  , textValue(EachValue)
	  , textAlignment(Qt::AlignTop | Qt::AlignHCenter)
	  , textPosition(Vip::Outside)
	  , textDistance(5)
	  , baseline(0)
	  , palette(VipLinearColorMap::ColorPaletteRandom)
	  , valueType(ScaleValue)
	{
	}

	double spacing;
	WidthUnit spacingUnit;
	double width;
	WidthUnit widthUnit;
	Style style;
	TextValue textValue;
	Qt::Alignment textAlignment;
	Vip::RegionPositions textPosition;
	double textDistance;
	QTransform textTransform;
	QPointF textTransformReference;
	VipText text;
	QSharedPointer<VipTextStyle> textStyle;

	double baseline;
	VipPlotBarChart::ValueType valueType;
	QVector<VipBoxStyle> boxStyles;
	VipBoxStyle boxStyle;
	VipColorPalette palette;
	QList<VipText> names;

	QRectF plotRect;
	VipInterval plotInterval;

	QVector<QVector<QPolygonF>> barRects;

	int indexOf(const QString& name)
	{
		for (int i = 0; i < names.size(); ++i)
			if (names[i].text() == name)
				return i;
		return -1;
	}
};

VipPlotBarChart::VipPlotBarChart(const VipText& title)
  : VipPlotItemDataType(title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->boxStyle.setBorderPen(QPen(Qt::NoPen));
}

VipPlotBarChart::~VipPlotBarChart()
{
}

void VipPlotBarChart::setData(const QVariant& v)
{
	d_data->plotInterval = VipInterval();

	d_data->plotRect = computePlotBoundingRect(v.value<VipBarVector>(), sceneMap());
	d_data->barRects.clear();
	VipPlotItemDataType::setData(v);
}

VipInterval VipPlotBarChart::plotInterval(const VipInterval& interval) const
{
	const VipBarVector vec = rawData();
	VipInterval inter = VipInterval();

	for (const VipBar& b : vec) {
		for (int i = 0; i < b.valueCount(); ++i) {
			double val = b.value(i);
			if (interval.contains(val)) {
				if (inter.isValid()) {

					inter.setMinValue(std::min(inter.minValue(), val));
					inter.setMaxValue(std::max(inter.maxValue(), val));
				}
				else
					inter = VipInterval(val, val);
			}
		}
	}
	return inter;
}

void VipPlotBarChart::setValueType(ValueType type)
{
	if (d_data->valueType != type) {
		d_data->valueType = type;
		d_data->plotRect = computePlotBoundingRect(rawData(), sceneMap());
		emitItemChanged();
	}
}

VipPlotBarChart::ValueType VipPlotBarChart::valueType() const
{
	return d_data->valueType;
}

void VipPlotBarChart::setBaseline(double reference)
{
	d_data->baseline = reference;
	d_data->plotRect = computePlotBoundingRect(rawData(), sceneMap());
	emitItemChanged();
}

double VipPlotBarChart::baseline() const
{
	return d_data->baseline;
}

void VipPlotBarChart::setSpacing(double spacing, WidthUnit unit)
{
	d_data->spacing = spacing;
	d_data->spacingUnit = unit;
	emitItemChanged();
}

double VipPlotBarChart::spacing() const
{
	return d_data->spacing;
}
VipPlotBarChart::WidthUnit VipPlotBarChart::spacingUnit() const
{
	return d_data->spacingUnit;
}

void VipPlotBarChart::setStyle(Style style)
{
	d_data->style = style;
	d_data->plotRect = computePlotBoundingRect(rawData(), sceneMap());
	emitItemChanged();
}

void VipPlotBarChart::setBarWidth(double width, WidthUnit unit)
{
	d_data->width = width;
	d_data->widthUnit = unit;
	d_data->plotRect = computePlotBoundingRect(rawData(), sceneMap());
	emitItemChanged();
}

double VipPlotBarChart::barWidth() const
{
	return d_data->width;
}
VipPlotBarChart::WidthUnit VipPlotBarChart::barWidthUnit() const
{
	return d_data->widthUnit;
}

VipPlotBarChart::Style VipPlotBarChart::style() const
{
	return d_data->style;
}

void VipPlotBarChart::setTextValue(TextValue style)
{
	d_data->textValue = style;
	emitItemChanged();
}

VipPlotBarChart::TextValue VipPlotBarChart::textValue() const
{
	return d_data->textValue;
}

void VipPlotBarChart::setTextAlignment(Qt::Alignment align)
{
	d_data->textAlignment = align;
	emitItemChanged();
}

Qt::Alignment VipPlotBarChart::textAlignment() const
{
	return d_data->textAlignment;
}

void VipPlotBarChart::setTextPosition(Vip::RegionPositions pos)
{
	d_data->textPosition = pos;
	emitItemChanged();
}

Vip::RegionPositions VipPlotBarChart::textPosition() const
{
	return d_data->textPosition;
}

void VipPlotBarChart::setTextTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform = tr;
	d_data->textTransformReference = ref;
	emitItemChanged();
}
const QTransform& VipPlotBarChart::textTransform() const
{
	return d_data->textTransform;
}
const QPointF& VipPlotBarChart::textTransformReference() const
{
	return d_data->textTransformReference;
}

void VipPlotBarChart::setTextDistance(double vipDistance)
{
	d_data->textDistance = vipDistance;
	emitItemChanged();
}

double VipPlotBarChart::textDistance() const
{
	return d_data->textDistance;
}

void VipPlotBarChart::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

const VipText& VipPlotBarChart::text() const
{
	return d_data->text;
}

void VipPlotBarChart::setBarNames(const QList<VipText>& names)
{
	d_data->names = names;
	emitItemChanged();
}

const QList<VipText>& VipPlotBarChart::barNames() const
{
	return d_data->names;
}

/// @brief Set the color palette used to fill each bar.
void VipPlotBarChart::setColorPalette(const VipColorPalette& p)
{
	d_data->palette = p;
	for (int i = 0; i < d_data->boxStyles.size(); ++i)
		d_data->boxStyles[i].setBackgroundBrush(QBrush(p.color(i)));
	emitItemChanged();
}
VipColorPalette VipPlotBarChart::colorPalette() const
{
	return d_data->palette;
}

/// @brief Reimplemented from VipPlotItem, in order to be responsive to stylesheets.
void VipPlotBarChart::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->text.setTextStyle(st);
	emitItemChanged();
}
VipTextStyle VipPlotBarChart::textStyle() const
{
	return d_data->textStyle ? *d_data->textStyle : VipTextStyle();
}

void VipPlotBarChart::setBoxStyle(const VipBoxStyle& st)
{
	d_data->boxStyle = st;
}
const VipBoxStyle& VipPlotBarChart::boxStyle() const
{
	return d_data->boxStyle;
}

void VipPlotBarChart::setBoxStyle(const VipBoxStyle& bstyle, int index)
{
	if (d_data->boxStyles.size() <= index) {
		int prev_size = d_data->boxStyles.size();
		d_data->boxStyles.resize(index + 1);
		for (int i = prev_size; i < d_data->boxStyles.size(); ++i) {
			d_data->boxStyles[i] = boxStyle();
			d_data->boxStyles[i].setBackgroundBrush(QBrush(d_data->palette.color(i)));
		}
	}

	d_data->boxStyles[index] = bstyle;
	emitItemChanged();
}

void VipPlotBarChart::setBoxStyle(const VipBoxStyle& bstyle, const QString& name)
{
	int index = d_data->indexOf(name);
	if (index >= 0)
		setBoxStyle(bstyle, index);
}

VipBoxStyle VipPlotBarChart::boxStyle(int index) const
{
	if (index < 0 || index >= d_data->boxStyles.size()) {
		VipBoxStyle st = boxStyle();
		st.setBackgroundBrush(d_data->palette.color(index));
		return st;
	}
	else {
		return d_data->boxStyles[index];
	}
}

VipBoxStyle VipPlotBarChart::boxStyle(const QString& name) const
{
	int index = d_data->indexOf(name);
	return boxStyle(index);
}

VipBoxStyle& VipPlotBarChart::boxStyle(int index)
{
	if (index < 0 || index >= d_data->boxStyles.size()) {
		index = std::abs(index);
		if (index >= d_data->boxStyles.size()) {
			int prev_size = d_data->boxStyles.size();
			d_data->boxStyles.resize(index + 1);
			for (int i = prev_size; i < d_data->boxStyles.size(); ++i) {
				VipBoxStyle st = boxStyle();
				st.setBackgroundBrush(d_data->palette.color(index));
				d_data->boxStyles[i] = st;
			}
		}
	}

	return d_data->boxStyles[index];
}

VipBoxStyle& VipPlotBarChart::boxStyle(const QString& name)
{
	int index = d_data->indexOf(name);
	return boxStyle(index);
}

void VipPlotBarChart::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	const VipBarVector values = this->rawData();

	d_data->barRects.resize(values.size());

	for (int i = 0; i < values.size(); ++i) {
		drawBarValues(painter, m, values.value(i), i);
	}
}

QList<VipText> VipPlotBarChart::legendNames() const
{
	return barNames();
}

QRectF VipPlotBarChart::drawLegend(QPainter* painter, const QRectF& rect, int index) const
{
	VipBoxStyle st = this->boxStyle(index);
	st.setBorderRadius(0);
	QRectF square = vipInnerSquare(rect);
	st.computeRect(square);
	st.draw(painter);
	return square;
}

QList<VipInterval> VipPlotBarChart::plotBoundingIntervals() const
{
	return QList<VipInterval>() << VipInterval(d_data->plotRect.left(), d_data->plotRect.right()).normalized() << VipInterval(d_data->plotRect.top(), d_data->plotRect.bottom()).normalized();
}

QString VipPlotBarChart::formatToolTip(const QPointF& pos) const
{
	for (int i = 0; i < d_data->barRects.size(); ++i) {
		const QVector<QPolygonF>& vec = d_data->barRects[i];
		for (int j = 0; j < vec.size(); ++j) {
			if (vec[j].boundingRect().contains(pos)) {
				const VipBarVector v = rawData();

				double value = v[i].value(j);
				const QString title = j < d_data->names.size() ? d_data->names[j].text() : QString();

				QString res = this->toolTipText();
				res = VipText::replace(res, "#value", value);
				res = VipText::replace(res, "#title", title);
				if (res.indexOf("#licon") >= 0)
					res = VipText::replace(res, "#licon", QString(vipToHtml(this->legendPixmap(QSize(20, 16), j))));
				res = VipPlotItem::formatText(res, pos);
				return res;
			}
		}
	}
	return QString();
}

bool VipPlotBarChart::areaOfInterest(const QPointF& pos, int, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const
{
	for (int i = 0; i < d_data->barRects.size(); ++i) {
		const QVector<QPolygonF>& vec = d_data->barRects[i];
		for (int j = 0; j < vec.size(); ++j) {
			QPainterPath p;
			p.addPolygon(vec[j]);
			if (maxDistance) {
				QPainterPathStroker stroker;
				stroker.setWidth(maxDistance);
				stroker.setJoinStyle(Qt::MiterJoin); // and other adjustments you need
				p = (stroker.createStroke(p) + p).simplified();
			}
			if (p.contains(pos)) {
				out_pos.push_back(pos);
				style.computeQuadrilateral(vec[j]);
				legend = j;
				return true;
			}
		}
	}
	return false;
}

void VipPlotBarChart::drawBarValues(QPainter* painter, const VipCoordinateSystemPtr& m, const VipBar& values, int index) const
{
	QList<QPolygonF> rects = barValuesRects(values, m);
	d_data->barRects[index].resize(rects.size());
	if (!rects.size())
		return;

	for (int i = 0; i < rects.size(); ++i) {
		rects[i] = m->transform(rects[i]);
		VipBoxStyle bs = boxStyle(i);
		bs.computeQuadrilateral(rects[i]);
		if (colorMap()) {
			bs.backgroundBrush().setColor(color(values.value(i), bs.backgroundBrush().color()));
		}
		bs.draw(painter);
	}
	std::copy(rects.begin(), rects.end(), d_data->barRects[index].begin());

	if (d_data->text.isEmpty())
		return;

	// draw the texts

	if (textValue() == EachValue) {
		for (int i = 0; i < rects.size(); ++i) {

			VipText t = d_data->text;
			QString res = VipPlotItem::formatText(t.text(), QPointF());
			if (index < d_data->names.size())
				res = VipText::replace(res, "#title", d_data->names[index].text());
			res = VipText::replace(res, "#value", values.value(i));
			t.setText(res);

			VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), rects[i].boundingRect());
		}
	}

	else if (textValue() == MaxValue || textValue() == SumValue) {
		double sumValue = 0;
		double maxValue = -std::numeric_limits<double>::max();
		int maxIndex = -1;
		QRectF union_rect = rects[0].boundingRect();

		for (int i = 0; i < rects.size(); ++i) {
			if (i > 0)
				union_rect = union_rect | rects[i].boundingRect();

			sumValue += values.value(i);
			if (values.value(i) > maxValue) {
				maxValue = values.value(i);
				maxIndex = i;
			}
		}

		VipText t = d_data->text;

		if (textValue() == MaxValue) {
			t.setText(VipText::replace(t.text(), "#value", maxValue));
			if (style() == SideBySide) {
				union_rect = rects[maxIndex].boundingRect();
			}
		}
		else {
			t.setText(VipText::replace(t.text(), "#value", sumValue));
		}

		// draw text
		VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), union_rect);
	}
}

double VipPlotBarChart::value(double v) const
{
	if (d_data->valueType == ScaleValue)
		return v;
	else
		return d_data->baseline + v;
}

QList<QPolygonF> VipPlotBarChart::barValuesRects(const VipBar& bv, const VipCoordinateSystemPtr& m) const
{

	double bspacing = spacing();
	double bwidth = barWidth();
	if (m && m->axes().size() == 2) {

		if (spacingUnit() == ItemUnit) {
			if (style() == SideBySide) {
				bspacing = static_cast<const VipBorderItem*>(m->axes().first())->itemRangeToAxisUnit(bspacing);
			}
			else {
				bspacing = static_cast<const VipBorderItem*>(m->axes().last())->itemRangeToAxisUnit(bspacing);
			}
		}
		if (barWidthUnit() == ItemUnit) {
			bwidth = static_cast<const VipBorderItem*>(m->axes().first())->itemRangeToAxisUnit(bwidth);
		}
	}
	else {
		bspacing = 0;
		bwidth = 0.1;
	}

	if (style() == SideBySide) {
		double total_width = bv.valueCount() * bwidth + (bv.valueCount() - 1) * bspacing;
		double x_start_pos = bv.position() - total_width / 2.0;

		QList<QPolygonF> res;

		for (int i = 0; i < bv.valueCount(); ++i) {
			QPolygonF p;
			p << QPointF(x_start_pos, value(bv.value(i))) << QPointF(x_start_pos + bwidth, value(bv.value(i))) << QPointF(x_start_pos + bwidth, baseline())
			  << QPointF(x_start_pos, baseline());
			res << p;

			x_start_pos += bwidth + bspacing;
		}

		return res;
	}
	else {
		if (valueType() == BarLength) {
			QList<QPolygonF> res;

			double x_left = bv.position() - bwidth / 2.0;
			double y_end = baseline();

			for (int i = 0; i < bv.valueCount(); ++i) {
				double value = qAbs(bv.value(i));
				QPolygonF p;
				p << QPointF(x_left, y_end + value) << QPointF(x_left + bwidth, y_end + value) << QPointF(x_left + bwidth, y_end) << QPointF(x_left, y_end);

				res << p;
				y_end += value + bspacing;
			}

			return res;
		}
		else {
			// order the values by vipDistance to the baseline
			QMultiMap<double, int> distance_to_index;
			for (int i = 0; i < bv.valueCount(); ++i) {
				distance_to_index.insert(qAbs(bv.value(i) - baseline()), i);
			}

			double top = baseline() - bspacing;
			double bottom = baseline() + bspacing;
			double x_left = bv.position() - bwidth / 2.0;
			QVector<QPolygonF> res(bv.valueCount());

			for (QMultiMap<double, int>::iterator it = distance_to_index.begin(); it != distance_to_index.end(); ++it) {
				int index = it.value();
				double value = bv.value(index);

				if (value > baseline()) {
					top += bspacing;
					if (top > value)
						top = value;

					QPolygonF p;
					p << QPointF(x_left, value) << QPointF(x_left + bwidth, value) << QPointF(x_left + bwidth, top) << QPointF(x_left, top);

					res[index] = p;
					top = value;
				}
				else {
					bottom -= bspacing;
					if (bottom < value)
						bottom = value;

					QPolygonF p;
					p << QPointF(x_left, value) << QPointF(x_left + bwidth, value) << QPointF(x_left + bwidth, bottom) << QPointF(x_left, bottom);

					res[index] = p;
					bottom = value;
				}
			}

			return res.toList();
		}
	}
}

QRectF VipPlotBarChart::computePlotBoundingRect(const VipBarVector& values, const VipCoordinateSystemPtr& m) const
{
	QRectF result;

	for (int i = 0; i < values.size(); ++i) {
		QList<QPolygonF> rects = barValuesRects(values[i], m);
		for (int j = 0; j < rects.size(); ++j) {
			if (result.isEmpty())
				result = rects[j].boundingRect();

			result = result.united(rects[j].boundingRect());
		}
	}

	return result;
}

bool VipPlotBarChart::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	/// -	'text-alignment' : see VipPlotBarChart::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
	/// -	'text-position': see VipPlotBarChart::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
	/// -	'text-distance': see VipPlotBarChart::setTextDistance()
	/// -	'style': bar chart style, one of 'stacked', 'sideBySide'
	/// -	'border-radius': border radius for the columns
	/// -   'text-value': see VipPlotBarChart::setTextValue(), one of 'eachValue', 'maxValue', 'sumValue'
	/// -   'value-type': see VipPlotBarChart::setValueType(), one of 'scaleValue', 'barLength'
	/// -   'width-unit': see VipPlotBarChart::setBarWidth(), one of 'itemUnit', 'axisUnit'
	/// -   'bar-width': width of each bar, see VipPlotBarChart::setBarWidth()
	///
	if (strcmp(name, "text-alignment") == 0) {
		setTextAlignment((Qt::Alignment)value.toInt());
		return true;
	}
	if (strcmp(name, "text-position") == 0) {
		setTextPosition((Vip::RegionPositions)value.toInt());
		return true;
	}
	if (strcmp(name, "text-distance") == 0) {
		setTextDistance(value.toDouble());
		return true;
	}
	if (strcmp(name, "border-radius") == 0) {
		VipBoxStyle st = boxStyle();
		st.setBorderRadius(value.toDouble());
		st.setRoundedCorners(Vip::AllCorners);
		setBoxStyle(st);
		for (int i = 0; i < d_data->boxStyles.size(); ++i) {
			d_data->boxStyles[i].setBorderRadius(value.toDouble());
			d_data->boxStyles[i].setRoundedCorners(Vip::AllCorners);
		}
		return true;
	}
	if (strcmp(name, "style") == 0) {
		setStyle((Style)value.toInt());
		return true;
	}
	if (strcmp(name, "text-value") == 0) {
		setTextValue((TextValue)value.toInt());
		return true;
	}
	if (strcmp(name, "value-type") == 0) {
		setValueType((ValueType)value.toInt());
		return true;
	}
	if (strcmp(name, "width-unit") == 0) {
		setBarWidth(barWidth(), (WidthUnit)value.toInt());
		return true;
	}
	if (strcmp(name, "bar-width") == 0) {
		setBarWidth(value.toDouble(), barWidthUnit());
		return true;
	}

	return VipPlotItem::setItemProperty(name, value, index);
}

QDataStream& operator<<(QDataStream& str, const VipBar& b)
{
	return str << b.position() << b.values();
}
QDataStream& operator>>(QDataStream& str, VipBar& b)
{
	QVector<double> values;
	double position;
	str >> position, values;
	b.setValues(values);
	b.setPosition(position);
	return str;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotBarChart* value)
{
	arch.content("boxStyle", value->boxStyle());
	arch.content("valueType", (int)value->valueType());
	arch.content("baseline", value->baseline());
	arch.content("spacing", value->spacing());
	arch.content("spacingUnit", (int)value->spacingUnit());
	arch.content("barWidth", value->barWidth());
	arch.content("barWidthUnit", (int)value->barWidthUnit());
	arch.content("style", (int)value->style());

	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());
	arch.content("barNames", value->barNames());
	return arch;
}
VipArchive& operator>>(VipArchive& arch, VipPlotBarChart* value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setValueType((VipPlotBarChart::ValueType)arch.read("valueType").value<int>());
	value->setBaseline(arch.read("baseline").value<double>());
	double spacing = arch.read("spacing").value<double>();
	int spacingUnit = arch.read("spacingUnit").value<int>();
	value->setSpacing(spacing, (VipPlotBarChart::WidthUnit)spacingUnit);
	double barWidth = arch.read("barWidth").value<double>();
	int barWidthUnit = arch.read("barWidthUnit").value<int>();
	value->setBarWidth(barWidth, (VipPlotBarChart::WidthUnit)barWidthUnit);
	value->setStyle((VipPlotBarChart::Style)arch.read("style").value<int>());
	value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	value->setTextTransform(textTransform, textTransformReference);
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	value->setBarNames(arch.read("barNames").value<VipTextList>());

	return arch;
}

static bool register_types()
{
	qRegisterMetaType<VipBar>();
	qRegisterMetaType<VipBarVector>();
	qRegisterMetaTypeStreamOperators<VipBar>();

	qRegisterMetaType<VipPlotBarChart*>();
	vipRegisterArchiveStreamOperators<VipPlotBarChart*>();

	return true;
}
static bool _register_types = register_types();