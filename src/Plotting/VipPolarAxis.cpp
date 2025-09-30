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

#include <limits>

#include <QGraphicsScene>
#include <QSet>

#include "VipPainter.h"
#include "VipPlotItem.h"
#include "VipPolarAxis.h"
#include "VipScaleDraw.h"
#include "VipScaleMap.h"
#include "VipSet.h"

#include "VipPieChart.h"
#include <iostream>
// store temporary variables needed to compute VipBorderItem geometries
class PolarStoreGeometry : public QObject //: public BaseStoreGeometry
{

public:
	QList<VipPolarAxis*> linkedPolarAxis;
	QList<VipRadialAxis*> linkedRadialAxis;
	QPointF shared_center;

	PolarStoreGeometry(VipAbstractScale* item)
	  : QObject(item)
	{
	}

	virtual void create(VipAbstractScale* b_item) const { new PolarStoreGeometry(b_item); }

	virtual void computeGeometry(const VipAbstractScale* b_item, const VipMargins&)
	{
		// TOCHANGE
		QList<VipAbstractScale*> linked;

		QRectF outer_rect = static_cast<const VipAbstractPolarScale*>(b_item)->outerRect();

		linkedPolarAxis.clear();
		linkedRadialAxis.clear();

		for (int i = 0; i < linked.size(); ++i) {
			if (qobject_cast<VipPolarAxis*>(linked[i]))
				linkedPolarAxis << static_cast<VipPolarAxis*>(linked[i]);
			else if (qobject_cast<VipRadialAxis*>(linked[i]))
				linkedRadialAxis << static_cast<VipRadialAxis*>(linked[i]);
		}

		// VipPolarAxis sorted by center proximity
		QMap<int, QList<VipPolarAxis*>> axes;
		// radius extent each layer
		QMap<int, double> radiusExtents;
		// free axes (centerProximity() < 0)
		QList<VipPolarAxis*> free;
		// maximum radius
		double max_radius = 0;

		// axes center
		shared_center = Vip::InvalidPoint;

		// extract VipPolarAxis sorted by layers, free VipPolarAxis, and radius extents sorted by layers
		for (int i = 0; i < linkedPolarAxis.size(); ++i) {
			VipPolarAxis* axis = linkedPolarAxis[i];

			// update shared_center for the first iteration
			if (shared_center == Vip::InvalidPoint)
				shared_center = axis->center();

			// Temporally block axes signals
			axis->blockSignals(true);

			// update center and layout scale if necessary (just to get min and max radius)
			axis->setCenter(shared_center);
			// if(axis->axisRect().isEmpty())
			axis->layoutScale();

			max_radius = qMax(max_radius, axis->maxRadius());

			// sort axes by center proximity
			if (axis->centerProximity() < 0)
				free << axis;
			else {
				axes[axis->centerProximity()] << axis;

				// compute layers extent
				QMap<int, double>::iterator it = radiusExtents.find(axis->centerProximity());
				if (it == radiusExtents.end())
					radiusExtents[axis->centerProximity()] = axis->radiusExtent();
				else {
					radiusExtents[axis->centerProximity()] = qMax(axis->radiusExtent(), it.value());
				}
			}
		}

		// update radius according to center proximity a first time
		QMapIterator<int, QList<VipPolarAxis*>> it(axes);
		it.toBack();
		it.previous();
		QList<double> extents = radiusExtents.values();
		double radius = max_radius;
		for (int i = extents.size() - 1; i >= 0; --i) {
			QList<VipPolarAxis*> layer = it.value();
			for (int j = 0; j < layer.size(); ++j) {
				layer[j]->setMinRadius(radius - extents[i]);
				layer[j]->layoutScale();
			}

			radius -= extents[i];
			it.previous();
		}

		// update VipRadialAxis layout
		for (int i = 0; i < linkedRadialAxis.size(); ++i) {
			VipRadialAxis* axis = static_cast<VipRadialAxis*>(linkedRadialAxis[i]);
			axis->blockSignals(true);
			axis->setCenter(shared_center);
			axis->layoutScale();
		}

		// compute the union rect of all axes and items
		QSet<VipPlotItem*> items;
		QRectF union_rect;
		for (int i = 0; i < linkedPolarAxis.size(); ++i) {
			if (linkedPolarAxis[i]->isVisible())
				union_rect |= linkedPolarAxis[i]->axisRect();
			items += vipToSet(linkedPolarAxis[i]->plotItems());
		}
		for (int i = 0; i < linkedRadialAxis.size(); ++i) {
			if (linkedRadialAxis[i]->isVisible())
				union_rect |= linkedRadialAxis[i]->axisRect();
			items += vipToSet(linkedRadialAxis[i]->plotItems());
		}
		for (VipPlotItem* item : items) {
			item->markCoordinateSystemDirty();
			union_rect |= item->shape().boundingRect().translated(item->pos());
		}

		// scale the bounding rect but keep proportions
		double factor = 1;
		double width_on_height = outer_rect.width() / outer_rect.height();
		double axes_width_on_height = union_rect.width() / union_rect.height();

		// compute the transformation to change axes radius and center
		if (axes_width_on_height > width_on_height) {
			factor = outer_rect.width() / union_rect.width();

			// QPointF proportions = (shared_center - union_rect.topLeft()) / QPointF(union_rect.width(),union_rect.height()) * factor;

			QPointF translate = QPointF(outer_rect.left() - union_rect.left(), outer_rect.top() + (outer_rect.height() - factor * union_rect.height()) / 2.0 - union_rect.top());

			QPointF topLeft = union_rect.topLeft() + translate;
			shared_center = (shared_center - union_rect.topLeft()) * factor + topLeft;
		}
		else {
			factor = outer_rect.height() / union_rect.height();
			QPointF translate = QPointF(outer_rect.left() + (outer_rect.width() - factor * union_rect.width()) / 2.0 - union_rect.left(), outer_rect.top() - union_rect.top());
			QPointF topLeft = union_rect.topLeft() + translate;
			shared_center = (shared_center - union_rect.topLeft()) * factor + topLeft;
		}

		// change the center for all axes
		for (int i = 0; i < linkedPolarAxis.size(); ++i)
			linkedPolarAxis[i]->setCenter(shared_center);

		// change the center for all axes
		for (int i = 0; i < linkedRadialAxis.size(); ++i)
			linkedRadialAxis[i]->setCenter(shared_center);

		// change axes radius the outer layer and free axes
		QList<VipPolarAxis*> outers = (--axes.end()).value();
		for (int i = 0; i < outers.size(); ++i) {
			double min_radius = qMax(0.1, outers[i]->minRadius() * factor);
			outers[i]->setMinRadius(min_radius);
			outers[i]->layoutScale();
		}

		for (int i = 0; i < free.size(); ++i) {
			double min_radius = qMax(0.1, free[i]->minRadius() * factor);
			free[i]->setMinRadius(min_radius);
			free[i]->layoutScale();
		}

		// update radius according to center proximity one last time, excluding outer layer
		it = QMapIterator<int, QList<VipPolarAxis*>>(axes);
		it.toBack();
		it.previous();
		it.previous();
		max_radius *= factor;
		radius = max_radius - extents.last();
		for (int i = extents.size() - 2; i >= 0; --i) {
			QList<VipPolarAxis*> layer = it.value();
			for (int j = 0; j < layer.size(); ++j) {
				layer[j]->setMinRadius(radius - extents[i]);
				layer[j]->layoutScale();
			}

			radius -= extents[i];
			it.previous();
		}

		// enable signals, compute the minimum size, set the geometry
		QRectF geom = QRectF(QPointF(0, 0), outer_rect.bottomRight() + outer_rect.topLeft() * 2);

		for (int i = 0; i < linkedPolarAxis.size(); ++i) {
			linkedPolarAxis[i]->setGeometry(geom);
			linkedPolarAxis[i]->blockSignals(false);
		}

		for (int i = 0; i < linkedRadialAxis.size(); ++i) {
			linkedRadialAxis[i]->layoutScale();
			linkedRadialAxis[i]->setGeometry(geom);
			linkedRadialAxis[i]->blockSignals(false);
		}
	}
};

static bool registerVipAbstractPolarScale = vipSetKeyWordsForClass(&VipAbstractPolarScale::staticMetaObject);

VipAbstractPolarScale::VipAbstractPolarScale(QGraphicsItem* parent)
  : VipAbstractScale(parent)
{
	// PolarStoreGeometry * geom = new PolarStoreGeometry(this);
	// Q_UNUSED(geom)
}

void VipAbstractPolarScale::setOuterRect(const QRectF& r)
{
	if (d_outerRect != r) {
		d_outerRect = r;
		this->emitGeometryNeedUpdate();
	}
}

QRectF VipAbstractPolarScale::outerRect() const
{
	// TOCHANGE
	//  if(! d_outerRect.isEmpty())
	//  return d_outerRect;
	//  else if(outerItem())
	//  return outerItem()->boundingRect();
	//  else
	return QRectF();
}

VipBoxStyle& VipAbstractPolarScale::axisBoxStyle()
{
	return d_style;
}

const VipBoxStyle& VipAbstractPolarScale::axisBoxStyle() const
{
	return d_style;
}

void VipAbstractPolarScale::setAxisBoxStyle(const VipBoxStyle& st)
{
	d_style = st;
	layoutScale();
}

class VipPolarAxis::PrivateData
{
public:
	PrivateData()
	  : centerProximity(0)
	  , startAngle(0)
	  , endAngle(0)
	  , minRadius(0)
	  , maxRadius(0)
	  , additionalRadius(0)
	  , radius(1)
	{
	}

	int centerProximity;

	Qt::Alignment titleAlignment;
	double startAngle;
	double endAngle;
	double minRadius;
	double maxRadius;

	// redefine radius and center since scale draw radius and center might be different because of radiusRatio
	double additionalRadius;
	double radius;
	QPointF center;
	QRectF axisRect;
};

static bool registerVipPolarAxis = vipSetKeyWordsForClass(&VipPolarAxis::staticMetaObject);

VipPolarAxis::VipPolarAxis(QGraphicsItem* parent)
  : VipAbstractPolarScale(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	this->setScaleDraw(new VipPolarScaleDraw());
	this->setMargin(2);
	this->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

	d_data->radius = constScaleDraw()->radius();
	d_data->center = constScaleDraw()->center();
}

VipPolarAxis::~VipPolarAxis()
{
}

QPainterPath VipPolarAxis::shape() const
{
	return axisBoxStyle().background();
}

bool VipPolarAxis::hasState(const QByteArray& state, bool enable) const
{
	if (state == "polar")
		return enable;
	return VipAbstractPolarScale::hasState(state, enable);
}

const VipPolarScaleDraw* VipPolarAxis::constScaleDraw() const
{
	return static_cast<const VipPolarScaleDraw*>(VipAbstractScale::constScaleDraw());
}

VipPolarScaleDraw* VipPolarAxis::scaleDraw()
{
	return static_cast<VipPolarScaleDraw*>(VipAbstractScale::scaleDraw());
}

QRectF VipPolarAxis::axisRect() const
{
	return d_data->axisRect;
}

void VipPolarAxis::setAdditionalRadius(double additionalRadius)
{
	if (d_data->additionalRadius != additionalRadius) {
		d_data->additionalRadius = additionalRadius;
		markStyleSheetDirty();
		this->emitGeometryNeedUpdate();
	}
}

double VipPolarAxis::additionalRadius() const
{
	return d_data->additionalRadius;
}

void VipPolarAxis::computeGeometry(bool compute_intersection_geometry)
{
	(void)compute_intersection_geometry;
	// TOCHANGE
	// Q_UNUSED(compute_intersection_geometry)
	//  QRectF outer_rect = this->outerRect();
	//  if(outer_rect.isEmpty())
	//  {
	//  this->layoutScale();
	//  QRectF r = axisRect();
	//  r.setTopLeft(QPointF(0,0));
	//  this->setGeometry(r);
	//  }
	//  else
	//  {
	//  const PolarStoreGeometry* geom = PolarStoreGeometry::retrieve<PolarStoreGeometry>(this);
	//  Q_UNUSED(geom)
	//  }
}

void VipPolarAxis::setCenterProximity(int p)
{
	if (p != d_data->centerProximity) {

		d_data->centerProximity = p;
		markStyleSheetDirty();
		this->emitGeometryNeedUpdate();
	}
}

int VipPolarAxis::centerProximity() const
{
	return d_data->centerProximity;
}

void VipPolarAxis::computeScaleDrawRadiusAndCenter()
{
	// compute the new scale draw center and radius according to additionalRadius
	double radius = d_data->radius + d_data->additionalRadius;
	double angle = (startAngle() + endAngle()) / 2;
	QLineF line(center(), QPointF(center().x(), center().y() - d_data->radius));
	line.setAngle(angle);
	QPointF p2 = line.p2();
	line.setP2(line.p1());
	line.setP1(p2);
	line.setLength(radius);

	scaleDraw()->setCenter(line.p2());
	scaleDraw()->setRadius(radius);
}


void VipPolarAxis::setCenter(const QPointF& c)
{
	if (c != center()) {
		QPointF prev = d_data->center;
		d_data->center = c;
		if (d_data->additionalRadius != 0) {
			computeScaleDrawRadiusAndCenter();
		}
		else {
			scaleDraw()->setCenter(c);
			scaleDraw()->setRadius(d_data->radius);
		}
		if (!qFuzzyCompare(prev, c))
			emitGeometryNeedUpdate();
	}
}

void VipPolarAxis::setRadius(double r)
{
	if (r != radius()) {
		double prev = d_data->radius;
		d_data->radius = r;
		if (d_data->additionalRadius != 0) {
			computeScaleDrawRadiusAndCenter();
		}
		else {
			scaleDraw()->setRadius(r);
			scaleDraw()->setCenter(d_data->center);
		}
		if (!qFuzzyCompare(prev, r))
			emitGeometryNeedUpdate();
	}
}

void VipPolarAxis::setMaxRadius(double max_radius)
{
	setRadius(radius() + (max_radius - maxRadius()));
}

void VipPolarAxis::setMinRadius(double min_radius)
{
	setRadius(radius() + (min_radius - minRadius()));
}

void VipPolarAxis::setStartAngle(double start)
{
	if (start != startAngle()) {
		scaleDraw()->setStartAngle(start);
		emitGeometryNeedUpdate();
	}
}

void VipPolarAxis::setEndAngle(double end)
{
	if (end != endAngle()) {
		scaleDraw()->setEndAngle(end);
		emitGeometryNeedUpdate();
	}
}

QPointF VipPolarAxis::center() const
{
	return d_data->center; // constScaleDraw()->center();
}

double VipPolarAxis::radius() const
{
	return d_data->radius; // constScaleDraw()->radius();
}

double VipPolarAxis::startAngle() const
{
	return constScaleDraw()->startAngle();
}

double VipPolarAxis::endAngle() const
{
	return constScaleDraw()->endAngle();
}

double VipPolarAxis::sweepLength() const
{
	return constScaleDraw()->sweepLength();
}

void VipPolarAxis::draw(QPainter* painter, QWidget* widget)
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();
	Q_UNUSED(widget)
	painter->setRenderHints(renderHints());

	axisBoxStyle().draw(painter);
	constScaleDraw()->draw(painter);

	// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//  if(el > 1)
	//  vip_debug("VipPolarAxis: %i ms\n", (int)el);
}

double VipPolarAxis::minRadius() const
{
	return d_data->minRadius;
}

double VipPolarAxis::maxRadius() const
{
	return d_data->maxRadius;
}

void VipPolarAxis::getBorderDistHint(double& start, double& end) const
{
	constScaleDraw()->getBorderDistHint(start, end);
	VipAbstractScale::getBorderDistHint(start, end);
}

void VipPolarAxis::getBorderRadius(double& min_radius, double& max_radius) const
{
	double start = radius();
	double end = radius() + constScaleDraw()->fullExtent();
	double title_height = (title().isEmpty() ? 0 : QFontMetrics(title().font()).height());

	if (end < start) {
		min_radius = end - margin();
		max_radius = start + title_height;
	}
	else {
		min_radius = start;
		max_radius = end + margin() + title_height;
	}
}

void VipPolarAxis::layoutScale()
{
	double s_angle, e_angle;
	getBorderDistHint(s_angle, e_angle);
	getBorderRadius(d_data->minRadius, d_data->maxRadius);

	d_data->startAngle = startAngle() - s_angle;
	d_data->endAngle = endAngle() + e_angle;

	if ((d_data->endAngle - d_data->startAngle) > 360.0) {
		vip_double space = (360.0 - (endAngle() - startAngle())) / 2.0;
		d_data->startAngle = startAngle() - space;
		d_data->endAngle = endAngle() + space;
	}

	vip_double spaceBefore = radius() - d_data->minRadius;
	vip_double spaceAfter = d_data->maxRadius - radius();
	vip_double minRadius = constScaleDraw()->radius() - spaceBefore;
	vip_double maxRadius = constScaleDraw()->radius() + spaceAfter;
	VipPoint drawCenter = scaleDraw()->center();
	axisBoxStyle().computePie((QPointF)drawCenter, VipPie(d_data->startAngle, d_data->endAngle, minRadius, maxRadius));

	d_data->axisRect = axisBoxStyle().background().boundingRect() | axisBoxStyle().border().boundingRect();

	this->update();
}

class VipRadialAxis::PrivateData
{
public:
	PrivateData()
	  : startDist(0)
	  , endDist(0)
	  , startRadius(0)
	  , endRadius(1)
	  , angle(0)
	  , startRadiusAxis(nullptr)
	  , endRadiusAxis(nullptr)
	  , angleAxis(nullptr)
	  , angleType(Vip::Relative)
	{
	}

	double startDist;
	double endDist;
	double startRadius;
	double endRadius;
	double angle;
	Qt::Alignment titleAlignment;
	QPolygonF polygon;
	QRectF axisRect;

	VipPolarAxis* startRadiusAxis;
	VipPolarAxis* endRadiusAxis;
	VipPolarAxis* angleAxis;
	Vip::ValueType angleType;
};

static bool registerVipRadialAxis = vipSetKeyWordsForClass(&VipRadialAxis::staticMetaObject);

VipRadialAxis::VipRadialAxis(QGraphicsItem* parent)
  : VipAbstractPolarScale(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	this->setScaleDraw(new VipRadialScaleDraw());
	this->setMargin(2);
	this->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
}

VipRadialAxis::~VipRadialAxis()
{
}

bool VipRadialAxis::hasState(const QByteArray& state, bool enable) const
{
	if (state == "radial")
		return enable;
	return VipAbstractPolarScale::hasState(state, enable);
}

QPainterPath VipRadialAxis::shape() const
{
	return axisBoxStyle().background();
}

const VipRadialScaleDraw* VipRadialAxis::constScaleDraw() const
{
	return static_cast<const VipRadialScaleDraw*>(VipAbstractScale::constScaleDraw());
}

VipRadialScaleDraw* VipRadialAxis::scaleDraw()
{
	return static_cast<VipRadialScaleDraw*>(VipAbstractScale::scaleDraw());
}

void VipRadialAxis::computeGeometry(bool compute_intersection_geometry)
{
	(void)compute_intersection_geometry;
	// TOCHANGE
	// Q_UNUSED(compute_intersection_geometry)
	//  QRectF outer_rect = this->outerRect();
	//  if(outer_rect.isEmpty())
	//  {
	//  this->layoutScale();
	//  QRectF r = axisRect();
	//  r.setTopLeft(QPointF(0,0));
	//  this->setGeometry(r);
	//  }
	//  else
	//  {
	//  //this will recompute the whole geometry
	//  const PolarStoreGeometry* geom = PolarStoreGeometry::retrieve<PolarStoreGeometry>(this);
	//  Q_UNUSED(geom)
	//  }
}

void VipRadialAxis::setCenter(const QPointF& c)
{
	if (c != center()) {
		QPointF prev = center();
		scaleDraw()->setCenter(c);
		if (!qFuzzyCompare(prev, c))
			emitGeometryNeedUpdate();
	}
}

void VipRadialAxis::setRadiusRange(double start_radius, double end_radius)
{
	setStartRadius(start_radius, d_data->startRadiusAxis);
	setEndRadius(end_radius, d_data->endRadiusAxis);
}

void VipRadialAxis::setStartRadius(double start_radius, VipPolarAxis* axis)
{
	if (start_radius != d_data->startRadius || axis != d_data->startRadiusAxis) {
		d_data->startRadius = start_radius;
		d_data->startRadiusAxis = axis;
		emitGeometryNeedUpdate();
	}
}

void VipRadialAxis::setEndRadius(double end_radius, VipPolarAxis* axis)
{
	if (end_radius != d_data->endRadius || axis != d_data->endRadiusAxis) {
		d_data->endRadius = end_radius;
		d_data->endRadiusAxis = axis;
		emitGeometryNeedUpdate();
	}
}

void VipRadialAxis::setAngle(double _angle, VipPolarAxis* axis, Vip::ValueType type)
{
	if (_angle != d_data->angle || axis != d_data->angleAxis || type != d_data->angleType) {
		d_data->angle = _angle;
		d_data->angleAxis = axis;
		d_data->angleType = type;
		emitGeometryNeedUpdate();
	}
}

QPointF VipRadialAxis::center() const
{
	return constScaleDraw()->center();
}

double VipRadialAxis::startRadius() const
{
	return d_data->startRadius;
}

double VipRadialAxis::endRadius() const
{
	return d_data->endRadius;
}

double VipRadialAxis::angle() const
{
	return d_data->angle;
}

void VipRadialAxis::draw(QPainter* painter, QWidget* widget)
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();

	Q_UNUSED(widget)
	painter->setRenderHints(renderHints());

	axisBoxStyle().draw(painter);
	constScaleDraw()->draw(painter);

	// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//  if(el > 1)
	//  vip_debug("VipRadialAxis: %i ms\n", (int)el);
}

void VipRadialAxis::getBorderDistHint(double& start, double& end) const
{
	constScaleDraw()->getBorderDistHint(start, end);
	VipAbstractScale::getBorderDistHint(start, end);
}

void VipRadialAxis::layoutScale()
{
	// recompute scale draw start radius, end radius and angle
	if (d_data->startRadiusAxis)
		scaleDraw()->setStartRadius(d_data->startRadius * d_data->startRadiusAxis->radius());
	else
		scaleDraw()->setStartRadius(d_data->startRadius);

	if (d_data->endRadiusAxis)
		scaleDraw()->setEndRadius(d_data->endRadius * d_data->endRadiusAxis->radius());
	else
		scaleDraw()->setEndRadius(d_data->endRadius);

	if (!d_data->angleAxis)
		scaleDraw()->setAngle(d_data->angle);
	else if (d_data->angleType == Vip::Relative)
		scaleDraw()->setAngle(d_data->angleAxis->startAngle() + d_data->angle * d_data->angleAxis->sweepLength());
	else
		scaleDraw()->setAngle(QLineF(center(), d_data->angleAxis->constScaleDraw()->position(d_data->angle, 0, Vip::Absolute)).angle());

	getBorderDistHint(d_data->startDist, d_data->endDist);
	double length = constScaleDraw()->fullExtent();
	double title_height = (title().isEmpty() ? 0 : QFontMetrics(title().font()).height());
	if (length > 0) {
		length = length + title_height + margin();
	}
	else {
		length = length - title_height - margin();
	}

	QLineF line(QPointF(center().x(), center().y()), QPointF(center().x() + constScaleDraw()->endRadius() + d_data->endDist, center().y()));
	line.setAngle(constScaleDraw()->angle());
	line.setP1(line.pointAt((constScaleDraw()->startRadius() - d_data->startDist) / (constScaleDraw()->endRadius() + d_data->endDist)));

	// d_data->maxRadius = qMax(qAbs(startRadius())+ d_data->startDist , qAbs(endRadius())+ d_data->endDist) ;

	d_data->polygon.clear();
	d_data->polygon.append(line.p1());
	d_data->polygon.append(line.p2());

	QLineF ln2 = line.normalVector();
	ln2.translate(line.p2() - ln2.p1());
	ln2.setLength(length);

	QLineF ln1 = line.normalVector();
	ln1.setLength(length);

	d_data->polygon.append(ln2.p2());
	d_data->polygon.append(ln1.p2());

	axisBoxStyle().computeQuadrilateral(d_data->polygon);
	d_data->axisRect = axisBoxStyle().background().boundingRect() | axisBoxStyle().border().boundingRect();

	this->update();
}

QRectF VipRadialAxis::axisRect() const
{
	return d_data->axisRect;
}
