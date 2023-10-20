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

#include <QGraphicsScene>
#include <QMap>
#include <QPainterPathStroker>

#include "VipAxisBase.h"
#include "VipPainter.h"
#include "VipPlotGrid.h"

static int registerGridKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["major-pen"] = VipParserPtr(new PenParser());
		keywords["minor-pen"] = VipParserPtr(new PenParser());
		keywords["major-axis"] = VipParserPtr(new BoolParser());
		keywords["minor-axis"] = VipParserPtr(new BoolParser());
		keywords["above"] = VipParserPtr(new BoolParser());

		vipSetKeyWordsForClass(&VipPlotGrid::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerGridKeyWords = registerGridKeyWords();

class VipPlotGrid::PrivateData
{
public:
	QMap<int, bool> axisEnabled;
	QMap<int, bool> axisMinEnabled;
	QPen minorPen;
	QPen majorPen;
};

VipPlotGrid::VipPlotGrid()
  : VipPlotItem()
{
	d_data = new PrivateData();
	this->setZValue(10);
	this->setItemAttribute(VisibleLegend, false);
	this->setItemAttribute(HasLegendIcon, false);
	this->setItemAttribute(AutoScale, false);
	this->setItemAttribute(ClipToScaleRect, false);
	this->setItemAttribute(Droppable, false);
}

VipPlotGrid::~VipPlotGrid()
{
	delete d_data;
}

void VipPlotGrid::enableAxis(int axis, bool enable)
{
	d_data->axisEnabled[axis] = enable;
	emitItemChanged();
}

bool VipPlotGrid::axisEnabled(int axis) const
{
	QMap<int, bool>::iterator it = d_data->axisEnabled.find(axis);
	if (it != d_data->axisEnabled.end())
		return it.value();
	return true;
}

void VipPlotGrid::enableAxisMin(int axis, bool enable)
{
	d_data->axisMinEnabled[axis] = enable;
	emitItemChanged();
}

bool VipPlotGrid::axisMinEnabled(int axis) const
{
	QMap<int, bool>::iterator it = d_data->axisMinEnabled.find(axis);
	if (it != d_data->axisMinEnabled.end())
		return it.value();
	return true;
}

void VipPlotGrid::setPen(const QPen& p)
{
	d_data->minorPen = p;
	d_data->majorPen = p;
	emitItemChanged();
}

void VipPlotGrid::setMajorPen(const QPen& p)
{
	d_data->majorPen = p;
	emitItemChanged();
}

const QPen& VipPlotGrid::majorPen() const
{
	return d_data->majorPen;
}
QPen& VipPlotGrid::majorPen()
{
	return d_data->majorPen;
}

void VipPlotGrid::setMinorPen(const QPen& p)
{
	d_data->minorPen = p;
	emitItemChanged();
}

const QPen& VipPlotGrid::minorPen() const
{
	return d_data->minorPen;
}
QPen& VipPlotGrid::minorPen()
{
	return d_data->minorPen;
}

void VipPlotGrid::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	VipCoordinateSystemPtr map = m;

	if (map->axes().size() != 2 || !map->axes().constFirst() || !map->axes().constLast())
		return;

	if (coordinateSystemType() == VipCoordinateSystem::Cartesian || coordinateSystemType() == VipCoordinateSystem::Null) {
		drawCartesian(p, *map);
	}
	else if (coordinateSystemType() == VipCoordinateSystem::Polar) {
		drawPolar(p, *static_cast<const VipPolarSystem*>(map.get()));
	}
}

void VipPlotGrid::drawCartesian(QPainter* p, const VipCoordinateSystem& m) const
{
	// check for vertical or horizontal line, and remove antialiazing
	QPointF diff_x = (m.transform(VipPoint(0, 0)) - m.transform(VipPoint(1, 0)));
	QPointF diff_y = (m.transform(VipPoint(0, 0)) - m.transform(VipPoint(0, 1)));
	bool remove_antialiazing = (vipFuzzyCompare(diff_x.y(), 0) && vipFuzzyCompare(diff_y.x(), 0) && !p->transform().isRotating());
	QPainter::RenderHints saved = p->renderHints();
	if (remove_antialiazing) {
		p->setRenderHint(QPainter::Antialiasing, false);
	}

	const QList<VipInterval> intervals = VipAbstractScale::scaleIntervals(axes());

	if (axisEnabled(0) && intervals[1].isValid()) {
		const VipScaleDiv::TickList major = axes()[0]->scaleDiv().ticks(VipScaleDiv::MajorTick);
		p->setPen((majorPen()));
		VipInterval inter = intervals[0];
		for (int i = 0; i < major.size(); ++i) {
			vip_double value = major[i];
			if (value != inter.minValue() && value != inter.maxValue())
				VipPainter::drawLine(p, m.transform(VipPoint(value, intervals[1].minValue())), m.transform(VipPoint(value, intervals[1].maxValue())));
		}

		if (axisMinEnabled(0)) {
			const VipScaleDiv::TickList minor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[0]->scaleDiv().ticks(VipScaleDiv::MinorTick);
			p->setPen((minorPen()));

			for (int i = 0; i < minor.size(); ++i) {
				vip_double value = minor[i];
				if (value != inter.minValue() && value != inter.maxValue())
					VipPainter::drawLine(p, m.transform(VipPoint(value, intervals[1].minValue())), m.transform(VipPoint(value, intervals[1].maxValue())));
			}
		}
	}

	if (axisEnabled(1) && intervals[0].isValid()) {
		const VipScaleDiv::TickList major = axes()[1]->scaleDiv().ticks(VipScaleDiv::MajorTick);
		p->setPen((majorPen()));
		VipInterval inter = intervals[1];
		for (int i = 0; i < major.size(); ++i) {
			vip_double value = major[i];
			if (value != inter.minValue() && value != inter.maxValue())
				VipPainter::drawLine(p, m.transform(VipPoint(intervals[0].minValue(), value)), m.transform(VipPoint(intervals[0].maxValue(), value)));
		}

		if (axisMinEnabled(1)) {
			const VipScaleDiv::TickList minor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[1]->scaleDiv().ticks(VipScaleDiv::MinorTick);
			p->setPen((minorPen()));

			for (int i = 0; i < minor.size(); ++i) {
				vip_double value = minor[i];
				if (value != inter.minValue() && value != inter.maxValue())
					VipPainter::drawLine(p, m.transform(VipPoint(intervals[0].minValue(), value)), m.transform(VipPoint(intervals[0].maxValue(), value)));
			}
		}
	}

	if (remove_antialiazing) {
		p->setRenderHints(saved);
	}
}

bool VipPlotGrid::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "major-pen") == 0) {
		setMajorPen(value.value<QPen>());
		return true;
	}
	if (strcmp(name, "minor-pen") == 0) {
		setMinorPen(value.value<QPen>());
		return true;
	}
	if (strcmp(name, "major-axis") == 0) {
		enableAxis(index.toInt(), value.toBool());
		return true;
	}
	if (strcmp(name, "minor-axis") == 0) {
		enableAxisMin(index.toInt(), value.toBool());
		return true;
	}
	if (strcmp(name, "above") == 0) {
		if (value.toBool()) {
			double max_z = property("_vip_max_z").toDouble();
			if (max_z)
				setZValue(max_z);
		}
		else {
			double max_z = property("_vip_max_z").toDouble();
			max_z = qMax(max_z, zValue());
			setProperty("_vip_max_z", max_z);
			QList<VipPlotCanvas*> canvas = vipCastItemList<VipPlotCanvas*>(linkedItems());
			double zval = canvas.size() ? canvas.first()->zValue() + 0.1 : 0;
			setZValue(zval);
		}
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

bool VipPlotGrid::hasState(const QByteArray& state, bool enable) const
{
	if (state == "cartesian") {
		return (coordinateSystemType() == VipCoordinateSystem::Cartesian) == enable;
	}
	if (state == "polar") {
		return (coordinateSystemType() == VipCoordinateSystem::Polar) == enable;
	}
	return VipPlotItem::hasState(state, enable);
}

static QPainterPath strokePath(const QPainterPath& p, double width)
{
	QPainterPathStroker stroke;
	stroke.setWidth(width);
	return stroke.createStroke(p);
}

static QPainterPath pathRadius(const VipScaleDiv::TickList& angles, const VipPolarSystem& m)
{
	QPainterPath res;
	for (int i = 0; i < angles.size(); ++i) {
		vip_double angle = angles[i];
		angle = m.polarTransform(VipPolarCoordinate(m.startRadius(), angle)).angle();
		QLineF line(m.center(), QPointF(m.center().x(), m.center().y() - m.endRadius()));
		line.setAngle(angle);
		line.setP1(line.pointAt(m.startRadius() / m.endRadius()));
		QPainterPath p;
		p.moveTo(line.p1());
		p.lineTo(line.p2());
		res.addPath(strokePath(p, 7));
	}
	return res;
}
static QPainterPath pathArc(const VipScaleDiv::TickList& radiuses, const VipPolarSystem& m)
{
	QPainterPath res;
	for (int i = 0; i < radiuses.size(); ++i) {
		vip_double radius = radiuses[i];
		radius = m.polarTransform(VipPolarCoordinate(radius, m.startAngle())).radius();
		QRectF rect((QPointF)m.center() - QPointF(radius, radius), QSizeF(radius * 2, radius * 2));
		QPainterPath p;
		p.arcMoveTo(rect, m.startAngle());
		p.arcTo(rect, m.startAngle(), m.sweepLength());
		res.addPath(strokePath(p, 7));
	}
	return res;
}

QPainterPath VipPlotGrid::shape() const
{
	const VipPolarSystem* m = static_cast<const VipPolarSystem*>(sceneMap().get());
	if (m->axes().size() != 2 || !m->axes().constFirst() || !m->axes().constLast())
		return QPainterPath();

	if (coordinateSystemType() == VipCoordinateSystem::Cartesian)
		return VipPlotItem::shape();

	VipScaleDiv::TickList anglesMajor;
	VipScaleDiv::TickList anglesMinor;
	VipScaleDiv::TickList radiusMajor;
	VipScaleDiv::TickList radiusMinor;

	if (m->isRadialPolar()) {

		if (axes()[0]) {
			if (axisEnabled(0))
				radiusMajor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(0))
				radiusMinor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[0]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
		if (axes()[1]) {
			if (axisEnabled(1))
				anglesMajor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(1))
				anglesMinor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[1]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
	}
	else {
		if (axes()[0]) {
			if (axisEnabled(0))
				anglesMajor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(0))
				anglesMinor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[0]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
		if (axes()[1]) {
			if (axisEnabled(1))
				radiusMajor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(1))
				radiusMinor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[1]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
	}

	QPainterPath res;
	res.addPath(pathRadius(anglesMajor, *m));
	res.addPath(pathRadius(anglesMinor, *m));
	res.addPath(pathArc(radiusMajor, *m));
	res.addPath(pathArc(radiusMinor, *m));
	return res;
}

void VipPlotGrid::drawRadius(QPainter* painter, const VipScaleDiv::TickList& angles, const QPen& pen, const VipPolarSystem& m) const
{
	painter->setPen(pen);
	for (int i = 0; i < angles.size(); ++i) {
		vip_double angle = angles[i];
		angle = m.polarTransform(VipPolarCoordinate(m.startRadius(), angle)).angle();
		QLineF line(m.center(), QPointF(m.center().x(), m.center().y() - m.endRadius()));
		line.setAngle(angle);
		line.setP1(line.pointAt(m.startRadius() / m.endRadius()));
		VipPainter::drawLine(painter, line);
	}
}

void VipPlotGrid::drawArc(QPainter* painter, const VipScaleDiv::TickList& radiuses, const QPen& pen, const VipPolarSystem& m) const
{
	painter->setPen(pen);
	for (int i = 0; i < radiuses.size(); ++i) {
		vip_double radius = radiuses[i];
		radius = m.polarTransform(VipPolarCoordinate(radius, m.startAngle())).radius();
		QRectF rect((QPointF)m.center() - QPointF(radius, radius), QSizeF(radius * 2, radius * 2));
		painter->drawArc(rect, m.startAngle() * 16, m.sweepLength() * 16);
	}
}

void VipPlotGrid::drawPolar(QPainter* p, const VipPolarSystem& m) const
{
	VipScaleDiv::TickList anglesMajor;
	VipScaleDiv::TickList anglesMinor;
	VipScaleDiv::TickList radiusMajor;
	VipScaleDiv::TickList radiusMinor;

	if (m.isRadialPolar()) {

		if (axes()[0]) {
			if (axisEnabled(0))
				radiusMajor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(0))
				radiusMinor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[0]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
		if (axes()[1]) {
			if (axisEnabled(1))
				anglesMajor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(1))
				anglesMinor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[1]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
	}
	else {
		if (axes()[0]) {
			if (axisEnabled(0))
				anglesMajor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(0))
				anglesMinor = axes()[0]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[0]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
		if (axes()[1]) {
			if (axisEnabled(1))
				radiusMajor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MajorTick);
			if (axisMinEnabled(1))
				radiusMinor = axes()[1]->scaleDiv().ticks(VipScaleDiv::MinorTick) + axes()[1]->scaleDiv().ticks(VipScaleDiv::MediumTick);
		}
	}

	this->drawRadius(p, anglesMajor, majorPen(), m);
	this->drawRadius(p, anglesMinor, minorPen(), m);
	this->drawArc(p, radiusMajor, majorPen(), m);
	this->drawArc(p, radiusMinor, minorPen(), m);
}

class VipPlotCanvas::PrivateData
{
public:
	VipBoxStyle boxStyle;
	QPolygonF polygon;
};

static bool registerVipPlotCanvas = vipSetKeyWordsForClass(&VipPlotCanvas::staticMetaObject);

VipPlotCanvas::VipPlotCanvas()
  : VipPlotItem()
{
	d_data = new PrivateData;

	this->setFlag(ItemIsSelectable, false);
	this->setItemAttribute(VisibleLegend, false);
	// this->setItemAttribute(IgnoreMouseEvents,true);
	this->setItemAttribute(HasLegendIcon, false);
	this->setItemAttribute(AutoScale, false);
	this->setItemAttribute(ClipToScaleRect, false);
	this->setItemAttribute(Droppable, false);
	// this->setRenderHints(QPainter::Antialiasing);
	this->setAcceptHoverEvents(false);
}

VipPlotCanvas::~VipPlotCanvas()
{
	delete d_data;
}

QPainterPath VipPlotCanvas::shape() const
{
	if (isDirtyShape()) {
		VipPlotCanvas* _this = const_cast<VipPlotCanvas*>(this);
		_this->markDirtyShape(false);

		if (sceneMap()->type() == VipCoordinateSystem::Polar) {
			const VipPolarSystem& map = static_cast<const VipPolarSystem&>(*sceneMap());
			_this->d_data->boxStyle.computePie((QPointF)map.center(), VipPie(map.startAngle(), map.endAngle(), qMax((vip_double)0.0, map.startRadius()), map.endRadius()));
		}
		else if (sceneMap()->type() == VipCoordinateSystem::Cartesian) {
			QRectF bounding;
			if (this->parentItem())
				bounding = this->parentItem()->boundingRect();
			else if (this->scene())
				bounding = this->scene()->sceneRect();

			VipInterval x, y;

			if (this->axes()[0])
				x = this->axes()[0]->scaleDiv().bounds();
			else
				x = VipInterval(bounding.left(), bounding.right());

			if (this->axes()[1])
				y = this->axes()[1]->scaleDiv().bounds();
			else
				y = VipInterval(bounding.top(), bounding.bottom());

			if (d_data->polygon.size() != 4)
				d_data->polygon.resize(4);
			d_data->polygon[0] = this->sceneMap()->transform(x.minValue(), y.minValue());
			d_data->polygon[1] = this->sceneMap()->transform(x.minValue(), y.maxValue());
			d_data->polygon[2] = this->sceneMap()->transform(x.maxValue(), y.maxValue());
			d_data->polygon[3] = this->sceneMap()->transform(x.maxValue(), y.minValue());

			_this->d_data->boxStyle.computeQuadrilateral(d_data->polygon);
		}
	}

	return d_data->boxStyle.background();
}

void VipPlotCanvas::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	Q_UNUSED(m)

	(void)shape();

	d_data->boxStyle.draw(p);
}

void VipPlotCanvas::setBoxStyle(const VipBoxStyle& bs)
{
	d_data->boxStyle = bs;
	emitItemChanged();
}

const VipBoxStyle& VipPlotCanvas::boxStyle() const
{
	return d_data->boxStyle;
}

VipBoxStyle& VipPlotCanvas::boxStyle()
{
	return d_data->boxStyle;
}

bool VipPlotCanvas::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	if (strcmp(name, "background") == 0) {

		setBrush(QBrush(value.value<QColor>()));
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

VipArchive& operator<<(VipArchive& arch, const VipPlotGrid* value)
{
	arch.content("minorPen", value->minorPen());
	arch.content("majorPen", value->majorPen());
	arch.content("_vip_customDisplay", value->property("_vip_customDisplay").toInt());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotGrid* value)
{
	value->setMinorPen(arch.read("minorPen").value<QPen>());
	value->setMajorPen(arch.read("majorPen").value<QPen>());
	// new in 2.2.18
	int _vip_customDisplay;
	if (arch.content("_vip_customDisplay", _vip_customDisplay))
		value->setProperty("_vip_customDisplay", _vip_customDisplay);
	else
		arch.restore();
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotCanvas* value)
{
	arch.content("boxStyle", value->boxStyle());
	// new in 2.2.18
	arch.content("_vip_customDisplay", value->property("_vip_customDisplay").toInt());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotCanvas* value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	// new in 2.2.18
	int _vip_customDisplay;
	if (arch.content("_vip_customDisplay", _vip_customDisplay))
		value->setProperty("_vip_customDisplay", _vip_customDisplay);
	else
		arch.restore();
	return arch;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipPlotGrid*>();
	qRegisterMetaType<VipPlotCanvas*>();

	vipRegisterArchiveStreamOperators<VipPlotGrid*>();
	vipRegisterArchiveStreamOperators<VipPlotCanvas*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
