/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include <QPointer>
#include <QRadialGradient>
#include <QSet>
#include <qmath.h>

#include "VipColorMap.h"
#include "VipPainter.h"
#include "VipPieChart.h"
#include "VipPolarAxis.h"

static const QMap<QByteArray, int>& legendStyles()
{
	static QMap<QByteArray, int> styles;
	if (styles.isEmpty()) {
		styles["backgroundAndBorder"] = VipPieItem::BackgroundAndBorder;
		styles["backgroundOnly"] = VipPieItem::BackgroundOnly;
		styles["backgroundAndDefaultPen"] = VipPieItem::BackgroundAndDefaultPen;
	}
	return styles;
}

static const QMap<QByteArray, int>& textTransforms()
{
	static QMap<QByteArray, int> tr;
	if (tr.isEmpty()) {
		tr["horizontal"] = VipScaleDraw::TextHorizontal;
		tr["parallel"] = VipScaleDraw::TextParallel;
		tr["perpendicular"] = VipScaleDraw::TextPerpendicular;
		tr["curved"] = VipScaleDraw::TextCurved;
	}
	return tr;
}

static const QMap<QByteArray, int>& textPositions()
{
	static QMap<QByteArray, int> tr;
	if (tr.isEmpty()) {
		tr["inside"] = VipScaleDraw::TextInside;
		tr["outside"] = VipScaleDraw::TextOutside;
		tr["automatic"] = VipScaleDraw::TextAutomaticPosition;
	}
	return tr;
}

static const QMap<QByteArray, int>& textDirections()
{
	static QMap<QByteArray, int> tr;
	if (tr.isEmpty()) {
		tr["inside"] = VipText::TowardInside;
		tr["outside"] = VipText::TowardOutside;
		tr["automatic"] = VipText::AutoDirection;
	}
	return tr;
}

static int registerPieItemKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["legend-style"] = VipParserPtr(new EnumParser(legendStyles()));
		keywords["clip-to-pie"] = VipParserPtr(new BoolParser());
		keywords["text-transform"] = VipParserPtr(new EnumParser(textTransforms()));
		keywords["text-position"] = VipParserPtr(new EnumParser(textPositions()));
		keywords["text-direction"] = VipParserPtr(new EnumParser(textDirections()));
		keywords["text-inner-distance-to-border"] = VipParserPtr(new DoubleParser());
		keywords["text-inner-distance-to-border-relative"] = VipParserPtr(new BoolParser());
		keywords["text-outer-distance-to-border"] = VipParserPtr(new DoubleParser());
		keywords["text-outer-distance-to-border-relative"] = VipParserPtr(new BoolParser());
		keywords["text-horizontal-distance"] = VipParserPtr(new DoubleParser());
		keywords["text-angle-position"] = VipParserPtr(new DoubleParser());
		keywords["to-text-border"] = VipParserPtr(new PenParser());
		keywords["spacing"] = VipParserPtr(new DoubleParser());
		vipSetKeyWordsForClass(&VipPieItem::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerPieItemKeyWords = registerPieItemKeyWords();

static int registerPieChartKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["legend-style"] = VipParserPtr(new EnumParser(legendStyles()));
		keywords["clip-to-pie"] = VipParserPtr(new BoolParser());
		keywords["text-transform"] = VipParserPtr(new EnumParser(textTransforms()));
		keywords["text-position"] = VipParserPtr(new EnumParser(textPositions()));
		keywords["text-direction"] = VipParserPtr(new EnumParser(textDirections()));
		keywords["text-inner-distance-to-border"] = VipParserPtr(new DoubleParser());
		keywords["text-inner-distance-to-border-relative"] = VipParserPtr(new BoolParser());
		keywords["text-outer-distance-to-border"] = VipParserPtr(new DoubleParser());
		keywords["text-outer-distance-to-border-relative"] = VipParserPtr(new BoolParser());
		keywords["text-horizontal-distance"] = VipParserPtr(new DoubleParser());
		keywords["text-angle-position"] = VipParserPtr(new DoubleParser());
		keywords["to-text-border"] = VipParserPtr(new PenParser());
		keywords["spacing"] = VipParserPtr(new DoubleParser());
		vipSetKeyWordsForClass(&VipPieChart::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerPieChartKeyWords = registerPieChartKeyWords();

VipAbstractPieItem::VipAbstractPieItem(const VipText& title)
  : VipPlotItemDataType<VipPie>(title)
{
	this->setItemAttribute(ClipToScaleRect, false);
	this->setItemAttribute(HasLegendIcon, true);
	this->setItemAttribute(VisibleLegend, true);
	this->setRenderHints(QPainter::Antialiasing);

	// handle synchronization
	// connect(this,SIGNAL(dataChanged()),this,SLOT(synchronize()), Qt::AutoConnection);
}

VipAbstractPieItem::~VipAbstractPieItem() {}

void VipAbstractPieItem::setColor(const QColor& c)
{
	markDirtyShape();

	d_style.setBorderPen(c);

	if (d_style.backgroundBrush().style() != Qt::NoBrush) {
		QBrush brush = d_style.backgroundBrush();
		brush.setColor(c);
		d_style.setBackgroundBrush(brush);
	}
	else {
		d_style.setBackgroundBrush(QBrush(c));
	}
}

void VipAbstractPieItem::setBoxStyle(const VipBoxStyle& bs)
{
	d_style = bs;
	emitItemChanged();
}

const VipBoxStyle& VipAbstractPieItem::boxStyle() const
{
	return d_style;
}

VipBoxStyle& VipAbstractPieItem::boxStyle()
{
	return d_style;
}

class VipPieItem::PrivateData
{
public:
	PrivateData()
	  : value(Vip::InvalidValue)
	  , spacing(0)
	  , legendStyle(VipPieItem::BackgroundAndDefaultPen)
	  , clipToPie(false)
	  , textTransform(VipAbstractScaleDraw::TextHorizontal)
	  , textPosition(VipAbstractScaleDraw::TextAutomaticPosition)
	  , textDirection(VipText::AutoDirection)
	  , textHorizontalDistance(0)
	  , textAnglePosition(0.5)
	{
		innerDistanceToBorder = Vip::Relative;
		textInnerDistanceToBorder = 0.3;
		outerDistanceToBorder = Vip::Absolute;
		textOuterDistanceToBorder = 10;
		textAdditionalTransformReference = QPointF(0, 0);
	}

	double value;
	VipText text;

	double spacing;

	VipPieItem::LegendStyle legendStyle;

	bool clipToPie;
	VipQuiverPath quiverPath;
	VipAbstractScaleDraw::TextTransform textTransform;
	VipAbstractScaleDraw::TextPosition textPosition;
	VipText::TextDirection textDirection;

	Vip::ValueType innerDistanceToBorder;
	double textInnerDistanceToBorder;

	Vip::ValueType outerDistanceToBorder;
	double textOuterDistanceToBorder;

	double textHorizontalDistance;
	double textAnglePosition;

	QTransform textAdditionalTransform;
	QTransform textAdditionalTransformInverted;
	QPointF textAdditionalTransformReference;

	QPolygonF polyline;
	VipTextObject textObject;

	QSharedPointer<VipTextStyle> textStyle;
};

VipPieItem::VipPieItem(const VipText& title)
  : VipAbstractPieItem(title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->quiverPath.setColor(Qt::black);
	d_data->quiverPath.setStyle(VipQuiverPath::QuiverStyles());
	// d_data->quiverPath.setExtremityBrush(VipQuiverPath::End,QBrush(Qt::red));
	//  d_data->quiverPath.setLength(VipQuiverPath::End,5);
	// d_data->quiverPath.setVisible(false);
	this->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
}

VipPieItem::~VipPieItem()
{
}

VipInterval VipPieItem::plotInterval(const VipInterval& interval) const
{
	if (vipIsValid(d_data->value)) {
		if (interval.contains(value()))
			return VipInterval(value(), value());
		return VipInterval();
	}
	else {
		return VipInterval();
	}
}

void VipPieItem::setValue(double value)
{
	if (value != d_data->value || !vipIsValid(d_data->value)) {
		d_data->value = value;
		// no need to mark style sheet dirty
		emitItemChanged(true, true, true, false);
	}
}

double VipPieItem::value() const
{
	return d_data->value;
}

void VipPieItem::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

const VipText& VipPieItem::text() const
{
	return d_data->text;
}

void VipPieItem::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->text.setTextStyle(st);
	emitItemChanged();
}

QPainterPath VipPieItem::shape() const
{
	const_cast<VipPieItem*>(this)->recomputeItem(VipCoordinateSystemPtr());
	QPainterPath p = boxStyle().background() + boxStyle().border() + d_data->textObject.shape();
	return p;
}

QRectF VipPieItem::boundingRect() const
{
	return shape().boundingRect();
}

void VipPieItem::setQuiverPath(const VipQuiverPath& q)
{
	d_data->quiverPath = q;
	emitItemChanged();
}

const VipQuiverPath& VipPieItem::quiverPath() const
{
	return d_data->quiverPath;
}

VipQuiverPath& VipPieItem::quiverPath()
{
	return d_data->quiverPath;
}

QString VipPieItem::formatText(const QString& str, const QPointF& pos) const
{
	QString res = VipPlotItem::formatText(str, pos);
	if (vipIsValid(value()))
		res = VipText::replace(res, "#value", value());
	return res;
}

void VipPieItem::setLegendStyle(VipPieItem::LegendStyle style)
{
	d_data->legendStyle = style;
	emitItemChanged();
}
VipPieItem::LegendStyle VipPieItem::legendStyle() const
{
	return d_data->legendStyle;
}

void VipPieItem::setClipToPie(bool enable)
{
	d_data->clipToPie = enable;
	emitItemChanged();
}
bool VipPieItem::clipToPie() const
{
	return d_data->clipToPie;
}

void VipPieItem::setTextTransform(VipAbstractScaleDraw::TextTransform textTransform)
{
	d_data->textTransform = textTransform;
	emitItemChanged();
}

VipAbstractScaleDraw::TextTransform VipPieItem::textTransform() const
{
	return d_data->textTransform;
}

void VipPieItem::setTextPosition(VipAbstractScaleDraw::TextPosition textPosition)
{
	d_data->textPosition = textPosition;
	emitItemChanged();
}

VipAbstractScaleDraw::TextPosition VipPieItem::textPosition() const
{
	return d_data->textPosition;
}

void VipPieItem::setTextDirection(VipText::TextDirection dir)
{
	d_data->textDirection = dir;
	emitItemChanged();
}
VipText::TextDirection VipPieItem::textDirection() const
{
	return d_data->textDirection;
}

void VipPieItem::setTextAdditionalTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textAdditionalTransform = tr;
	d_data->textAdditionalTransformInverted = tr.inverted();
	d_data->textAdditionalTransformReference = ref;
	emitItemChanged();
}
QTransform VipPieItem::textAdditionalTransform() const
{
	return d_data->textAdditionalTransform;
}
QPointF VipPieItem::textAdditionalTransformReference() const
{
	return d_data->textAdditionalTransformReference;
}

void VipPieItem::setTextInnerDistanceToBorder(double dist, Vip::ValueType d)
{
	d_data->textInnerDistanceToBorder = dist;
	d_data->innerDistanceToBorder = d;

	emitItemChanged();
}

double VipPieItem::textInnerDistanceToBorder() const
{
	return d_data->textInnerDistanceToBorder;
}

Vip::ValueType VipPieItem::innerDistanceToBorder() const
{
	return d_data->innerDistanceToBorder;
}

void VipPieItem::setTextOuterDistanceToBorder(double dist, Vip::ValueType d)
{
	d_data->textOuterDistanceToBorder = dist;
	d_data->outerDistanceToBorder = d;

	emitItemChanged();
}

double VipPieItem::textOuterDistanceToBorder() const
{
	return d_data->textOuterDistanceToBorder;
}

Vip::ValueType VipPieItem::outerDistanceToBorder() const
{
	return d_data->outerDistanceToBorder;
}

void VipPieItem::setTextHorizontalDistance(double dist)
{
	d_data->textHorizontalDistance = dist;
	emitItemChanged();
}

double VipPieItem::textHorizontalDistance() const
{
	return d_data->textHorizontalDistance;
}

void VipPieItem::setTextAnglePosition(double normalized_angle)
{
	d_data->textAnglePosition = normalized_angle;
	emitItemChanged();
}

double VipPieItem::textAnglePosition() const
{
	return d_data->textAnglePosition;
}

void VipPieItem::setSpacing(double spacing)
{
	d_data->spacing = spacing;
	emitItemChanged();
}
double VipPieItem::spacing() const
{
	return d_data->spacing;
}

const VipTextObject& VipPieItem::textObject() const
{
	const_cast<VipPieItem*>(this)->recomputeItem(VipCoordinateSystemPtr());
	return d_data->textObject;
}

const QPolygonF& VipPieItem::polyline() const
{
	const_cast<VipPieItem*>(this)->recomputeItem(VipCoordinateSystemPtr());
	return d_data->polyline;
}

PainterPaths VipPieItem::piePath() const
{
	const_cast<VipPieItem*>(this)->recomputeItem(VipCoordinateSystemPtr());
	return boxStyle().paths();
}

void VipPieItem::recomputeItem(const VipCoordinateSystemPtr& cm)
{
	if (textPosition() == VipAbstractScaleDraw::TextAutomaticPosition) {
		recomputeItem(cm, VipAbstractScaleDraw::TextInside);
		if (!boxStyle().background().contains(d_data->textObject.shape())) {
			markDirtyShape();
			recomputeItem(cm, VipAbstractScaleDraw::TextOutside);
		}
	}
	else {
		recomputeItem(cm, textPosition());
	}
}

void VipPieItem::recomputeItem(const VipCoordinateSystemPtr& cm, VipAbstractScaleDraw::TextPosition text_position)
{
	if (!isDirtyShape()) {
		return;
	}
	markDirtyShape(false);

	VipCoordinateSystemPtr m = cm;
	VipPlotItemComposite* parent = nullptr;

	if (!m)
		m = sceneMap();
	if (m->axes().isEmpty()) {
		if ((parent = property("VipPlotItemComposite").value<VipPlotItemComposite*>()))
			m = parent->sceneMap();
	}
	const QList<VipAbstractScale*> scales = m->axes(); // this->axes();
	if (scales.size() != 2)
		return;
	const VipAbstractPolarScale* sc = qobject_cast<const VipAbstractPolarScale*>(scales.first());
	if (!sc)
		return;

	VipPie original_pie = rawData();
	QPointF center_point = sc->center();

	VipPie pie = static_cast<VipPolarSystem*>(m.get())->polarTransform(original_pie);
	boxStyle().computePie(center_point, pie, d_data->spacing);

	d_data->polyline.clear();
	d_data->textObject = VipTextObject();

	// set the quiver path pen color
	/*int flags = customDisplayFlags(); // vipGetCustomDisplayFlags(parent ? (QGraphicsObject*)parent : (QGraphicsObject*)this);
	if (!(flags & VIP_CUSTOM_PEN)) {
		// use backbone color
		QPen p = d_data->quiverPath.pen();
		p.setColor(scales.first()->scaleDraw()->componentPen(VipScaleDraw::Backbone).color());
		d_data->quiverPath.setPen(p);
	}*/

	// draw the text

	VipText t = this->text();
	t.setText(this->formatText(t.text(), QPointF()));

	if (t.isEmpty())
		return;

	double text_angle = pie.startAngle() + d_data->textAnglePosition * pie.sweepLength();

	QTransform text_transform;
	// VipPie text_pie = pie;

	double innerDistanceToBorder = 0;
	if (d_data->innerDistanceToBorder == Vip::Absolute) {
		innerDistanceToBorder = d_data->textInnerDistanceToBorder;
	}
	else {
		innerDistanceToBorder = d_data->textInnerDistanceToBorder * pie.radiusExtent();
	}
	double outerDistanceToBorder = 0;
	if (d_data->outerDistanceToBorder == Vip::Absolute) {
		outerDistanceToBorder = d_data->textOuterDistanceToBorder;
	}
	else {
		outerDistanceToBorder = d_data->textOuterDistanceToBorder * pie.radiusExtent();
	}

	// horizontal text inside
	if (text_position == VipAbstractScaleDraw::TextInside && textTransform() == VipAbstractScaleDraw::TextHorizontal) {
		QLineF line(center_point, QPointF(center_point.x(), center_point.y() - pie.offsetToCenter() - pie.maxRadius() + innerDistanceToBorder));
		line.setAngle(text_angle);

		QTransform tr;
		tr.translate(line.p2().x(), line.p2().y());
		tr.translate(-t.textSize().width() / 2, -t.textSize().height() / 2);
		QRectF rect = tr.map(t.textRect()).boundingRect();

		d_data->textObject = VipTextObject(t, rect);
	}
	// horizontal text outside
	else if (text_position == VipAbstractScaleDraw::TextOutside && textTransform() == VipAbstractScaleDraw::TextHorizontal) {
		QPolygonF polyline;
		bool left = false;
		if ((text_angle > 90 && text_angle < 270) || (text_angle < -90 && text_angle > -270))
			left = true;

		QLineF line(center_point, QPointF(center_point.x(), center_point.y() - pie.offsetToCenter() - pie.maxRadius()));
		line.setAngle(text_angle);
		if (!d_data->clipToPie && !boxStyle().borderPen().isCosmetic() && !boxStyle().isTransparentPen())
			line.setLength(line.length() + boxStyle().borderPen().widthF() / 2);
		polyline << line.p2();
		line.setLength(line.length() + outerDistanceToBorder);
		polyline << line.p2();

		if (textHorizontalDistance() != 0) {
			polyline << line.p2() + QPointF(left ? -textHorizontalDistance() : textHorizontalDistance(), 0);

			// draw the polyline
			QPainter painter;
			QPair<double, double> additional_lengths = d_data->quiverPath.draw(&painter, polyline);

			QTransform tr;
			tr.translate(polyline.back().x(), polyline.back().y() - t.textSize().height() / 2);
			if (left)
				tr.translate(-t.textSize().width() - 5 - additional_lengths.second, 0);
			else
				tr.translate(5 + additional_lengths.second, 0);

			QRectF rect = tr.map(t.textRect()).boundingRect();
			d_data->textObject = VipTextObject(t, rect);
		}
		else {
			// draw the polyline
			QPainter painter;
			QPair<double, double> additional_lengths = d_data->quiverPath.draw(&painter, polyline);
			line.setLength(line.length() + additional_lengths.second + 5);

			QTransform text_tr = textTransformation(textTransform(), text_position, text_angle, line.p2(), t.textSize());
			d_data->textObject = VipTextObject(t, t.textRect(), text_tr);
		}

		d_data->polyline = polyline;
	}
	// curved text
	else if (textTransform() == VipAbstractScaleDraw::TextCurved) {
		double height = t.textSize().height() * 1.5;

		if (text_position == VipAbstractScaleDraw::TextInside) {
			VipPie tpie(pie);
			tpie.setMeanAngle(text_angle);
			tpie.setMaxRadius(pie.maxRadius() - innerDistanceToBorder);
			tpie.setMinRadius(pie.maxRadius() - innerDistanceToBorder - height);
			d_data->textObject = VipTextObject(t, tpie, center_point);
		}
		else {
			VipPie tpie(pie);
			tpie.setMeanAngle(text_angle);
			tpie.setMinRadius(pie.maxRadius() + outerDistanceToBorder);
			tpie.setMaxRadius(pie.maxRadius() + outerDistanceToBorder + height);
			d_data->textObject = VipTextObject(t, tpie, center_point, textDirection());
		}
	}
	// text perpendicular or parallel
	else {
		QLineF line(center_point, QPointF(center_point.x(), center_point.y() - pie.offsetToCenter() - pie.maxRadius()));
		line.setAngle(text_angle);

		if (!d_data->clipToPie && !boxStyle().borderPen().isCosmetic() && !boxStyle().isTransparentPen())
			line.setLength(line.length() + boxStyle().borderPen().widthF() / 2);

		d_data->polyline << line.p2();

		if (text_position == VipAbstractScaleDraw::TextOutside) {
			line.setLength(line.length() + outerDistanceToBorder);

			d_data->polyline << line.p2();
			line = QLineF(d_data->polyline[0], d_data->polyline[1]);

			// draw the line
			QPainter painter;
			QPair<double, double> additional_lengths = d_data->quiverPath.draw(&painter, line);
			line.setLength(line.length() + additional_lengths.second + 5);
			d_data->polyline[0] = line.p1();
			d_data->polyline[1] = line.p2();
		}
		else {
			line.setLength(line.length() - innerDistanceToBorder - 5);
			d_data->polyline.clear();
		}

		QTransform text_tr = textTransformation(textTransform(), text_position, text_angle, line.p2(), t.textSize());
		d_data->textObject = VipTextObject(t, t.textRect(), text_tr);
	}

	if (!d_data->textAdditionalTransform.isIdentity() && d_data->textTransform != VipAbstractScaleDraw::TextCurved) {
		QTransform tr;
		QPointF ref = textAdditionalTransformReference();
		ref.rx() *= d_data->textObject.rect().width();
		ref.ry() *= d_data->textObject.rect().height();
		QPointF tl = d_data->textObject.rect().topLeft() + ref;
		tl = d_data->textObject.transform().map(tl);
		tr.translate(-tl.x(), -tl.y());
		tr *= d_data->textAdditionalTransform;
		QPointF pt = d_data->textAdditionalTransformInverted.map(tl);
		tr.translate(pt.x(), pt.y());
		d_data->textObject.setTransform(d_data->textObject.transform() * tr);
	}
}

void VipPieItem::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	Q_UNUSED(m)
	const_cast<VipPieItem*>(this)->recomputeItem(m);

	// draw pie
	VipBoxStyle bstyle = boxStyle();

	const bool use_clip = d_data->clipToPie && !bstyle.isTransparentPen() && !bstyle.borderPen().isCosmetic();
	if (use_clip) {
		painter->save();
		painter->setClipPath(bstyle.background(), Qt::IntersectClip);
	}
	if (colorMap() && vipIsValid(value())) {
		QBrush b = boxStyle().backgroundBrush();
		b.setColor(color(value()));
		bstyle.draw(painter, b);
	}
	else {

		bstyle.draw(painter);
	}
	if (use_clip)
		painter->restore();

	// draw polyline

	d_data->quiverPath.draw(painter, d_data->polyline);

	// draw text
	d_data->textObject.draw(painter);
}

bool VipPieItem::areaOfInterest(const QPointF& pos, int, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const
{
	const VipBoxStyle bstyle = boxStyle();
	QPainterPath p = bstyle.background();
	if (maxDistance) {
		QPainterPathStroker stroker;
		stroker.setWidth(maxDistance);
		stroker.setJoinStyle(Qt::MiterJoin); // and other adjustments you need
		p = (stroker.createStroke(p) + p).simplified();
	}
	if (p.contains(pos)) {
		out_pos.push_back(pos);
		style.computePath(bstyle.background());
		legend = 0;
		return true;
	}
	return false;
}

bool VipPieItem::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "legend-style") == 0) {
		setLegendStyle((LegendStyle)value.toInt());
		return true;
	}
	if (strcmp(name, "clip-to-pie") == 0) {
		setClipToPie(value.toBool());
		return true;
	}
	if (strcmp(name, "text-transform") == 0) {
		setTextTransform((VipScaleDraw::TextTransform)value.toInt());
		return true;
	}
	if (strcmp(name, "text-position") == 0) {
		setTextPosition((VipScaleDraw::TextPosition)value.toInt());
		return true;
	}
	if (strcmp(name, "text-direction") == 0) {
		setTextDirection((VipText::TextDirection)value.toInt());
		return true;
	}
	if (strcmp(name, "text-inner-distance-to-border") == 0) {
		setTextInnerDistanceToBorder(value.toDouble(), d_data->innerDistanceToBorder);
		return true;
	}
	if (strcmp(name, "text-inner-distance-to-border-relative") == 0) {
		setTextInnerDistanceToBorder(textInnerDistanceToBorder(), value.toBool() ? Vip::Relative : Vip::Absolute);
		return true;
	}
	if (strcmp(name, "text-outer-distance-to-border") == 0) {
		setTextOuterDistanceToBorder(value.toDouble(), d_data->outerDistanceToBorder);
		return true;
	}
	if (strcmp(name, "text-outer-distance-to-border-relative") == 0) {
		setTextOuterDistanceToBorder(textOuterDistanceToBorder(), value.toBool() ? Vip::Relative : Vip::Absolute);
		return true;
	}
	if (strcmp(name, "text-angle-position") == 0) {
		setTextAnglePosition(value.toDouble());
		return true;
	}
	if (strcmp(name, "spacing") == 0) {
		setSpacing(value.toDouble());
		return true;
	}
	if (strcmp(name, "to-text-border") == 0) {
		VipQuiverPath p = quiverPath();
		p.setPen(value.value<QPen>());
		setQuiverPath(p);
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

QList<VipText> VipPieItem::legendNames() const
{
	return QList<VipText>() << title();
}

QRectF VipPieItem::drawLegend(QPainter* painter, const QRectF& r, int) const
{
	QRectF square = vipInnerSquare(r);
	square = square.adjusted(1.5, 1.5, -1.5, -1.5).normalized();
	VipBoxStyle style = boxStyle();
	style.setBorderRadius(0);
	style.computeRect(square);

	QPainter::RenderHints hints = painter->renderHints();

	QGraphicsView* w = nullptr;
	if (d_data->legendStyle == BackgroundAndDefaultPen) {
		w = this->view();
		if (!w)
			if (VipPlotItemComposite* parent = this->property("VipPlotItemComposite").value<VipPlotItemComposite*>())
				w = parent->view();

		// if painter does not define a rotation, remove antialiazing
		if (w && !painter->transform().isRotating())
			painter->setRenderHints(QPainter::RenderHints());
	}

	if (colorMap() && vipIsValid(value())) {
		QBrush b = boxStyle().backgroundBrush();
		b.setColor(color(value()));

		if (d_data->legendStyle == BackgroundOnly)
			style.setBorderPen(QPen(Qt::NoPen));
		else if (d_data->legendStyle == BackgroundAndDefaultPen) {
			if (w)
				style.setBorderPen(QPen(w->palette().color(QPalette::Text)));
		}

		style.draw(painter, b);
	}
	else {
		if (d_data->legendStyle == BackgroundOnly)
			style.setBorderPen(QPen(Qt::NoPen));
		else if (d_data->legendStyle == BackgroundAndDefaultPen) {
			if (w)
				style.setBorderPen(QPen(w->palette().color(QPalette::Text)));
		}
		style.draw(painter);
	}

	painter->setRenderHints(hints);

	return square;
}

class VipPieChart::PrivateData
{
public:
	PrivateData()
	  : pie(0, 100, 0, 100, 0)
	  , sumValue(0)
	{
		defaultItem .reset( new VipPieItem());
		defaultItem->setText(VipText("#value%.1f"));
		defaultItem->boxStyle().setBackgroundBrush(QBrush(Qt::blue));
		defaultItem->boxStyle().setBorderPen(QPen(Qt::NoPen));

		brushColorPalette = VipColorPalette(VipLinearColorMap::ColorPaletteRandom);
		penColorPalette = brushColorPalette.lighter();
	}

	VipPie pie;

	VipColorPalette brushColorPalette;
	VipColorPalette penColorPalette;

	QVector<double> values;
	QVector<VipText> titles;
	double sumValue;

	std::unique_ptr<VipPieItem> defaultItem;

	// shape handling
	QPainterPath shape;
	QRectF boundingRect;
};

VipPieChart::VipPieChart(const VipText& title)
  : VipPlotItemComposite(UniqueItem, title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	// this->setFlag(ItemIsSelectable,false);
	// this->setAcceptHoverEvents(false);
	// this->setItemAttribute(VisibleLegend,false);
	this->setItemAttribute(ClipToScaleRect, false);
	this->setRenderHints(QPainter::Antialiasing);
	this->setSavePainterBetweenItems(true);
}

VipPieChart::~VipPieChart()
{
}

QRectF VipPieChart::boundingRect() const
{
	// recompute the shape if needed
	shape();
	return d_data->boundingRect;
}

QPainterPath VipPieChart::shape() const
{
	if (!isDirtyShape()) {
		return d_data->shape;
	}
	else {
		markDirtyShape(false);
		VipPieChart* _this = const_cast<VipPieChart*>(this);
		_this->d_data->shape = QPainterPath();
		_this->d_data->boundingRect = QRectF();

		const VipCoordinateSystemPtr m = sceneMap();
		const QList<VipAbstractScale*> scales = this->axes();
		if (scales.size() != 2)
			return QPainterPath();
		const VipAbstractPolarScale* sc = qobject_cast<const VipAbstractPolarScale*>(scales.first());
		if (!sc)
			return QPainterPath();

		QPointF center_point = sc->center();
		VipPie p = static_cast<VipPolarSystem*>(m.get())->polarTransform(pie());

		for (int i = 0; i < this->count(); ++i) {
			_this->d_data->shape += this->at(i)->shape();
		}

		VipBoxStyle st;
		st.computePie(center_point, p);
		_this->d_data->shape += st.background();

		_this->d_data->boundingRect = _this->d_data->shape.boundingRect();
		return d_data->shape;
	}
}

void VipPieChart::setPie(const VipPie& p)
{
	d_data->pie = p;
	double start_angle = p.startAngle();

	for (int i = 0; i < this->count(); ++i) {
		VipPieItem* item = static_cast<VipPieItem*>(at(i));
		VipPie tmp = item->rawData();
		tmp.setMaxRadius(p.maxRadius());
		tmp.setMinRadius(p.minRadius());
		tmp.setOffsetToCenter(p.offsetToCenter());
		tmp.setStartAngle(start_angle);
		double range = item->value() / d_data->sumValue;
		start_angle += range;
		tmp.setEndAngle(start_angle);

		item->setRawData(tmp);
	}

	emitItemChanged();
}

const VipPie& VipPieChart::pie() const
{
	return d_data->pie;
}

/// @brief Set the legend style
void VipPieChart::setLegendStyle(VipPieItem::LegendStyle style)
{
	d_data->defaultItem->setLegendStyle(style);
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setLegendStyle(style);
	emitItemChanged();
}
VipPieItem::LegendStyle VipPieChart::legendStyle() const
{
	return d_data->defaultItem->legendStyle();
}

const QVector<double>& VipPieChart::values() const
{
	return d_data->values;
}
const QVector<VipText>& VipPieChart::titles() const
{
	return d_data->titles;
}

void VipPieChart::setTitles(const QVector<VipText>& titles)
{
	d_data->titles = titles;
	if (d_data->values.size() && d_data->titles.size() != d_data->values.size())
		d_data->titles.resize(d_data->values.size());

	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTitle(d_data->titles[i]);
	emitItemChanged();
}

void VipPieChart::setValues(const QVector<double>& values, const QVector<VipText>& titles)
{
	if (values.size() != this->count())
		this->clear();

	if (titles.size()) {
		d_data->titles = titles;
	}
	d_data->titles.resize(values.size());

	// compute sum
	d_data->sumValue = 0;
	for (auto it = values.begin(); it != values.end(); ++it)
		d_data->sumValue += *it;

	double start_angle = d_data->pie.startAngle();
	for (int i = 0; i < values.size(); ++i) {
		VipPieItem* item = nullptr;
		if (values.size() != this->count())
			item = createItem(i);
		else
			item = static_cast<VipPieItem*>(this->at(i));

		item->setValue(values[i]);
		item->setTitle(d_data->titles[i]);

		VipPie p = d_data->pie;
		double range = (item->value() / d_data->sumValue) * d_data->pie.sweepLength();
		p.setStartAngle(start_angle);
		start_angle += range;
		p.setEndAngle(start_angle);

		if (item->rawData().offsetToCenter())
			p.setOffsetToCenter(item->rawData().offsetToCenter());
		item->setRawData(p);
	}

	d_data->values = values;
	emitItemChanged();
}

void VipPieChart::setQuiverPath(const VipQuiverPath& q)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setQuiverPath(q);
	d_data->defaultItem->setQuiverPath(q);
	emitItemChanged();
}

const VipQuiverPath& VipPieChart::quiverPath() const
{
	return d_data->defaultItem->quiverPath();
}

void VipPieChart::setClipToPie(bool enable)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setClipToPie(enable);
	d_data->defaultItem->setClipToPie(enable);
	emitItemChanged();
}
bool VipPieChart::clipToPie() const
{
	return d_data->defaultItem->clipToPie();
}

void VipPieChart::setSpacing(double sp)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setSpacing(sp / 2);
	d_data->defaultItem->setSpacing(sp);
	emitItemChanged();
}
double VipPieChart::spacing() const
{
	return d_data->defaultItem->spacing();
}

void VipPieChart::setTextTransform(VipAbstractScaleDraw::TextTransform tr)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextTransform(tr);
	d_data->defaultItem->setTextTransform(tr);
	emitItemChanged();
}

VipAbstractScaleDraw::TextTransform VipPieChart::textTransform() const
{
	return d_data->defaultItem->textTransform();
}

void VipPieChart::setTextPosition(VipAbstractScaleDraw::TextPosition tp)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextPosition(tp);

	d_data->defaultItem->setTextPosition(tp);
	emitItemChanged();
}

VipAbstractScaleDraw::TextPosition VipPieChart::textPosition() const
{
	return d_data->defaultItem->textPosition();
}

void VipPieChart::setTextDirection(VipText::TextDirection dir)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextDirection(dir);

	d_data->defaultItem->setTextDirection(dir);
	emitItemChanged();
}
VipText::TextDirection VipPieChart::textDirection() const
{
	return d_data->defaultItem->textDirection();
}

void VipPieChart::setTextAdditionalTransform(const QTransform& tr, const QPointF& ref)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextAdditionalTransform(tr, ref);

	d_data->defaultItem->setTextAdditionalTransform(tr, ref);
	emitItemChanged();
}
QTransform VipPieChart::textAdditionalTransform() const
{
	return d_data->defaultItem->textAdditionalTransform();
}
QPointF VipPieChart::textAdditionalTransformReference() const
{
	return d_data->defaultItem->textAdditionalTransformReference();
}

void VipPieChart::setTextHorizontalDistance(double dist)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextHorizontalDistance(dist);

	d_data->defaultItem->setTextHorizontalDistance(dist);
	emitItemChanged();
}

double VipPieChart::textHorizontalDistance() const
{
	return d_data->defaultItem->textHorizontalDistance();
}

void VipPieChart::setTextAnglePosition(double normalized_angle)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextAnglePosition(normalized_angle);

	d_data->defaultItem->setTextAnglePosition(normalized_angle);
	emitItemChanged();
}

double VipPieChart::textAnglePosition() const
{
	return d_data->defaultItem->textAnglePosition();
}

void VipPieChart::setBrushColorPalette(const VipColorPalette& palette)
{
	d_data->brushColorPalette = palette;

	for (int i = 0; i < count(); ++i) {
		pieItemAt(i)->boxStyle().backgroundBrush().setColor(palette.color(i));
		pieItemAt(i)->update();
	}
	emitItemChanged();
}

const VipColorPalette& VipPieChart::brushColorPalette() const
{
	return d_data->brushColorPalette;
}

void VipPieChart::setPenColorPalette(const VipColorPalette& palette)
{
	d_data->penColorPalette = palette;
	for (int i = 0; i < count(); ++i) {
		QPen p = pieItemAt(i)->boxStyle().borderPen();
		p.setColor(palette.color(i));
		pieItemAt(i)->boxStyle().setBorderPen(p);
		pieItemAt(i)->update();
	}
	emitItemChanged();
}

const VipColorPalette& VipPieChart::penColorPalette() const
{
	return d_data->penColorPalette;
}

void VipPieChart::setColorPalette(const VipColorPalette& palette)
{
	setBrushColorPalette(palette);
	// setPenColorPalette(palette);
}

void VipPieChart::setTextStyle(const VipTextStyle& st)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextStyle(st);
	d_data->defaultItem->setTextStyle(st);
	emitItemChanged();
}

VipTextStyle VipPieChart::textStyle() const
{
	return d_data->defaultItem->textStyle();
}

void VipPieChart::setTextInnerDistanceToBorder(double dist, Vip::ValueType d)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextInnerDistanceToBorder(dist, d);

	d_data->defaultItem->setTextInnerDistanceToBorder(dist, d);
	emitItemChanged();
}

double VipPieChart::textInnerDistanceToBorder() const
{
	return d_data->defaultItem->textInnerDistanceToBorder();
}

Vip::ValueType VipPieChart::innerDistanceToBorder() const
{
	return d_data->defaultItem->innerDistanceToBorder();
}

void VipPieChart::setTextOuterDistanceToBorder(double dist, Vip::ValueType d)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setTextOuterDistanceToBorder(dist, d);

	d_data->defaultItem->setTextOuterDistanceToBorder(dist, d);
	emitItemChanged();
}

double VipPieChart::textOuterDistanceToBorder() const
{
	return d_data->defaultItem->textOuterDistanceToBorder();
}

Vip::ValueType VipPieChart::outerDistanceToBorder() const
{
	return d_data->defaultItem->outerDistanceToBorder();
}

bool VipPieChart::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "legend-style") == 0) {
		setLegendStyle((VipPieItem::LegendStyle)value.toInt());
		return true;
	}
	if (strcmp(name, "clip-to-pie") == 0) {
		setClipToPie(value.toBool());
		return true;
	}
	if (strcmp(name, "text-transform") == 0) {
		setTextTransform((VipScaleDraw::TextTransform)value.toInt());
		return true;
	}
	if (strcmp(name, "text-position") == 0) {
		setTextPosition((VipScaleDraw::TextPosition)value.toInt());
		return true;
	}
	if (strcmp(name, "text-direction") == 0) {
		setTextDirection((VipText::TextDirection)value.toInt());
		return true;
	}
	if (strcmp(name, "text-inner-distance-to-border") == 0) {
		setTextInnerDistanceToBorder(value.toDouble(), innerDistanceToBorder());
		return true;
	}
	if (strcmp(name, "text-inner-distance-to-border-relative") == 0) {
		setTextInnerDistanceToBorder(textInnerDistanceToBorder(), value.toBool() ? Vip::Relative : Vip::Absolute);
		return true;
	}
	if (strcmp(name, "text-outer-distance-to-border") == 0) {
		setTextOuterDistanceToBorder(value.toDouble(), outerDistanceToBorder());
		return true;
	}
	if (strcmp(name, "text-outer-distance-to-border-relative") == 0) {
		setTextOuterDistanceToBorder(textOuterDistanceToBorder(), value.toBool() ? Vip::Relative : Vip::Absolute);
		return true;
	}
	if (strcmp(name, "text-angle-position") == 0) {
		setTextAnglePosition(value.toDouble());
		return true;
	}
	if (strcmp(name, "spacing") == 0) {
		setSpacing(value.toDouble());
		return true;
	}
	if (strcmp(name, "to-text-border") == 0) {
		VipQuiverPath p = quiverPath();
		p.setPen(value.value<QPen>());
		setQuiverPath(p);
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

void VipPieChart::setText(const VipText& t)
{
	for (int i = 0; i < count(); ++i)
		pieItemAt(i)->setText(t);

	d_data->defaultItem->setText(t);
	emitItemChanged();
}

VipText VipPieChart::text() const
{
	return d_data->defaultItem->text();
}

void VipPieChart::setItemsBoxStyle(const VipBoxStyle& bs)
{
	for (int i = 0; i < count(); ++i) {
		VipBoxStyle tmp = pieItemAt(i)->boxStyle();
		QPen pen = bs.borderPen();
		QBrush brush = bs.backgroundBrush();
		// pen.setColor(tmp.borderPen().color());
		brush.setColor(tmp.backgroundBrush().color());

		tmp = bs;
		tmp.setBorderPen(pen);
		tmp.setBackgroundBrush(brush);

		pieItemAt(i)->setBoxStyle(tmp);
	}

	d_data->defaultItem->setBoxStyle(bs);
	emitItemChanged();
}

const VipBoxStyle& VipPieChart::itemsBoxStyle() const
{
	return d_data->defaultItem->boxStyle();
}

VipPieItem* VipPieChart::createItem(int index)
{
	VipPieItem* item = new VipPieItem();
	item->setText(d_data->defaultItem->text());
	item->setClipToPie(this->clipToPie());
	item->setTextAdditionalTransform(this->textAdditionalTransform(), this->textAdditionalTransformReference());
	item->setTextTransform(this->textTransform());
	item->setTextDirection(this->textDirection());
	item->setTextInnerDistanceToBorder(this->textInnerDistanceToBorder(), this->innerDistanceToBorder());
	item->setTextOuterDistanceToBorder(this->textOuterDistanceToBorder(), this->outerDistanceToBorder());
	item->setQuiverPath(this->quiverPath());
	item->setLegendStyle(this->legendStyle());
	VipBoxStyle st = this->itemsBoxStyle();
	QPen p = st.borderPen();
	QBrush b = st.backgroundBrush();
	// p.setColor(d_data->penColorPalette.color(index));
	b.setColor(d_data->brushColorPalette.color(index));
	st.setBackgroundBrush(b);
	st.setBorderPen(p);
	item->setBoxStyle(st);

	this->append(item);

	return item;
}

void VipPieChart::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	VipPlotItemComposite::draw(p, m);
}

VipInterval VipPieChart::plotInterval(const VipInterval& interval) const
{
	VipInterval res;

	for (int i = 0; i < count(); ++i) {
		VipInterval tmp = pieItemAt(i)->plotInterval(interval);
		if (tmp.isValid() && !res.isValid())
			res = tmp;
		else if (tmp.isValid() && res.isValid())
			res = res.unite(tmp);
	}

	return res;
}
