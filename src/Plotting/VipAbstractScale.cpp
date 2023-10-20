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

#include "VipAbstractScale.h"
#include "VipBorderItem.h"
#include "VipBoxStyle.h"
#include "VipPainter.h"
#include "VipPlotItem.h"
#include "VipPlotWidget2D.h"
#include "VipScaleDiv.h"
#include "VipScaleDraw.h"
#include "VipScaleEngine.h"
#include "VipScaleMap.h"
#include "VipSet.h"
#include "VipText.h"
#include "VipUniqueId.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPaintEngine>
#include <QPointer>
#include <QTimer>
#include <qpainter.h>

static int registerBoxKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["border"] = VipParserPtr(new PenParser());
		keywords["border-width"] = VipParserPtr(new DoubleParser());
		keywords["border-radius"] = VipParserPtr(new DoubleParser());
		keywords["background"] = VipParserPtr(new ColorParser());

		vipSetKeyWordsForClass(&VipBoxGraphicsWidget::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerBoxKeyWords = registerBoxKeyWords();

static int registerAbstractScaleKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		QMap<QByteArray, int> position;
		position["inside"] = VipScaleDraw::TextInside;
		position["outside"] = VipScaleDraw::TextOutside;

		QMap<QByteArray, int> transform;
		transform["horizontal"] = VipScaleDraw::TextHorizontal;
		transform["parallel"] = VipScaleDraw::TextParallel;
		transform["perpendicular"] = VipScaleDraw::TextPerpendicular;
		transform["curved"] = VipScaleDraw::TextCurved;

		keywords["auto-scale"] = VipParserPtr(new BoolParser());

		VipStandardStyleSheet::addTextStyleKeyWords(keywords, "label-");

		keywords["display"] = VipParserPtr(new BoolParser());
		keywords["pen"] = VipParserPtr(new PenParser());
		keywords["pen-color"] = VipParserPtr(new ColorParser());
		keywords["margin"] = VipParserPtr(new DoubleParser());
		keywords["spacing"] = VipParserPtr(new DoubleParser());
		keywords["inverted"] = VipParserPtr(new BoolParser());

		keywords["label-position"] = VipParserPtr(new EnumParser(position));
		keywords["ticks-position"] = VipParserPtr(new EnumParser(position));
		keywords["ticks-length"] = VipParserPtr(new DoubleParser());
		keywords["label-transform"] = VipParserPtr(new EnumParser(transform));

		vipSetKeyWordsForClass(&VipAbstractScale::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerAbstractScaleKeyWords = registerAbstractScaleKeyWords();

class VipBoxGraphicsWidget::PrivateData
{
public:
	VipBoxStyle style;
	bool updateScheduled;
	QImage pixmap;
	bool dirtyPixmap;
	PrivateData()
	  : updateScheduled(0)
	  , dirtyPixmap(1)
	{
	}
};

VipBoxGraphicsWidget::VipBoxGraphicsWidget(QGraphicsItem* parent)
  : QGraphicsWidget(parent)
  , VipPaintItem(this)
  , VipRenderObject(this)
{
	d_data = new PrivateData();
	this->setAcceptHoverEvents(true);
}

VipBoxGraphicsWidget::~VipBoxGraphicsWidget()
{
	delete d_data;
}

VipBoxStyle& VipBoxGraphicsWidget::boxStyle()
{
	update();
	return d_data->style;
}

const VipBoxStyle& VipBoxGraphicsWidget::boxStyle() const
{
	return d_data->style;
}

void VipBoxGraphicsWidget::setBoxStyle(const VipBoxStyle& style)
{
	d_data->style = style;
	this->markStyleSheetDirty();
	update();
}

VipAbstractPlotArea* VipBoxGraphicsWidget::area() const
{
	QGraphicsItem* p = parentItem();
	while (p) {
		if (VipAbstractPlotArea* a = qobject_cast<VipAbstractPlotArea*>(p->toGraphicsObject()))
			return a;
		p = p->parentItem();
	}
	return nullptr;
}

void VipBoxGraphicsWidget::setGeometry(const QRectF& rect)
{
	QGraphicsWidget::setGeometry(rect);
}

void VipBoxGraphicsWidget::update()
{
	if (!d_data->updateScheduled) {
		d_data->updateScheduled = 1;
		if (VipAbstractPlotArea* a = area()) {
			a->markNeedUpdate();
			d_data->dirtyPixmap = true;
			// only call update() if caching is enabled
			if (cacheMode() != NoCache) {
				QGraphicsWidget::update();
				return;
			}

			return;
		}
		QGraphicsWidget::update();
	}
}

void VipBoxGraphicsWidget::draw(QPainter* painter, QWidget*)
{
	if (!d_data->style.isTransparent()) {
		d_data->style.computeRect(this->boundingRect());
		d_data->style.draw(painter);
	}
}

void VipBoxGraphicsWidget::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	// Reapply style sheet in case of 'hover' selector
	this->setProperty("_vip_hover", true);
	this->markStyleSheetDirty();
	QGraphicsWidget::hoverEnterEvent(event);
}

void VipBoxGraphicsWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	// Reapply style sheet in case of 'hover' selector
	this->setProperty("_vip_hover", false);
	this->markStyleSheetDirty();
	QGraphicsWidget::hoverLeaveEvent(event);
}

QVariant VipBoxGraphicsWidget::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemSelectedHasChanged)
		this->markStyleSheetDirty();
	else if (change == QGraphicsItem::ItemChildAddedChange)
		this->dispatchStyleSheetToChildren();

	return QGraphicsWidget::itemChange(change, value);
}

bool VipBoxGraphicsWidget::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	else if (strcmp(name, "border") == 0) {

		if (value.canConvert<QPen>())
			boxStyle().setBorderPen(value.value<QPen>());
		else if (value.canConvert<QColor>()) {
			boxStyle().borderPen().setColor(value.value<QColor>());
		}
		else
			return false;
		return true;
	}
	else if (strcmp(name, "border-width") == 0) {
		bool ok = false;
		double w = value.toDouble(&ok);
		if (!ok)
			return false;
		boxStyle().borderPen().setWidthF(w);

		return true;
	}
	else if (strcmp(name, "border-radius") == 0) {
		bool ok = false;
		double r = value.toDouble(&ok);
		if (!ok)
			return false;
		boxStyle().setRoundedCorners(Vip::AllCorners);
		boxStyle().setBorderRadius(r);

		return true;
	}
	else if (strcmp(name, "background") == 0) {
		if (value.canConvert<QBrush>())
			boxStyle().setBackgroundBrush(value.value<QBrush>());
		else if (value.canConvert<QColor>()) {
			boxStyle().backgroundBrush().setColor(value.value<QColor>());
		}
		else
			return false;
		return true;
	}

	return VipPaintItem::setItemProperty(name, value, index);
}

void VipBoxGraphicsWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	if (!paintingEnabled())
		return;

	this->applyStyleSheetIfDirty();
	painter->setRenderHints(this->renderHints());
	painter->setCompositionMode(this->compositionMode());
	/* #ifndef VIP_OPENGL_ITEM_CACHING
	if (VipPainter::isOpenGL(painter) && w) { //check if drawing on an opengl widget
		const double margin =  0;
		QRectF r = boundingRect().adjusted(-margin, -margin, margin, margin);
		if (d_data->dirtyPixmap) {
			if (d_data->pixmap.width() < r.size().width() || d_data->pixmap.height() < r.size().height())
				d_data->pixmap = QImage(r.size().toSize(), QImage::Format_ARGB32_Premultiplied);
			d_data->pixmap.fill(Qt::transparent);
			QPainter p(&d_data->pixmap);
			p.setTransform(QTransform().translate(-r.left() - 0.5, -r.top() ));
			draw(&p);
			d_data->dirtyPixmap = false;
		}
		painter->drawImage(r.topLeft(), d_data->pixmap);
	}
	else
#endif*/
	draw(painter);

	d_data->updateScheduled = 0;
}

class VipAbstractScale::PrivateData
{
public:
	PrivateData()
	  : spacing(0)
	  , margin(0)
	  , maxMinor(1)
	  , maxMajor(9)
	  , itemIntervalFactor(0)
	  , cacheFullExtent(-1)
	  , // spacing(2), margin(4),maxMinor(5), maxMajor(10),
	  dirtyScaleDiv(1)
	  , autoScale(true)
	  , scaleInverted(false)
	  , drawTitle(true)
	  , dirtyItems(false)
	  , scaleDraw(nullptr)
	  , scaleEngine(0)
	{
		borderDist[0] = borderDist[1] = 0;
		minBorderDist[0] = minBorderDist[1] = 0;
		maxBorderDist[0] = maxBorderDist[1] = 10000;
		scaleEngine = new VipLinearScaleEngine();
		scaleDraw = new VipScaleDraw();
		lastScaleIntervalUpdate = 0;
		lastScaleIntervalWidth = 0;
		optimizeForStreaming = false;
		optimizeForStreamingFactor = 0.02;
	}

	~PrivateData()
	{
		if (synchronizedWith.size()) {
			synchronizedWith.clear();
		}
		delete scaleEngine;
		delete scaleDraw;
	}

	double spacing;
	double margin;

	double borderDist[2];
	double minBorderDist[2];
	double maxBorderDist[2];

	int maxMinor;
	int maxMajor;

	double itemIntervalFactor;
	double cacheFullExtent;

	bool dirtyScaleDiv;
	bool autoScale;
	bool optimizeForStreaming;
	double optimizeForStreamingFactor;
	bool scaleInverted;
	bool drawTitle;
	bool dirtyItems;

	VipAbstractScaleDraw* scaleDraw;
	VipScaleEngine* scaleEngine;
	VipInterval computedInterval;

	qint64 lastScaleIntervalUpdate;
	vip_double lastScaleIntervalWidth;

	// QPointer<VipAbstractScale> synchronizedWith;
	QList<QPointer<VipAbstractScale>> synchronizedWith;

	QList<VipPlotItem*> plotItems;
};

VipAbstractScale::VipAbstractScale(QGraphicsItem* parent)
  : VipBoxGraphicsWidget(parent)
{
	d_data = new PrivateData();

	this->setFlag(QGraphicsItem::ItemIsSelectable);

	// scaleDivNeedUpdate() is emitted only on VipPlotItem's request, when data changed and scale div might need to be recomputed (in cas of automatic scaling)
	connect(this, SIGNAL(scaleDivNeedUpdate()), this, SLOT(delayedRecomputeScaleDiv()), Qt::QueuedConnection);
	connect(this, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(delayedRecomputeScaleDiv()), Qt::QueuedConnection);
	connect(this, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(delayedRecomputeScaleDiv()), Qt::QueuedConnection);
	connect(this, SIGNAL(scaleDivChanged(bool)), this, SLOT(synchronize()), Qt::DirectConnection);

	// set a dummy scale div
	d_data->scaleDraw->setScaleDiv(this->scaleEngine()->divideScale(0, 100, d_data->maxMajor, d_data->maxMinor, 10));

	// setCacheMode(ItemCoordinateCache);
}

VipAbstractScale::~VipAbstractScale()
{
	// remove synchronized items
	desynchronize();
	delete d_data;
}

VipInterval VipAbstractScale::itemsInterval() const
{
	const QList<VipPlotItem*>& items = this->plotItems();
	if (items.size() < 1)
		return VipInterval();

	int i = 0;
	VipInterval bounds;

	// compute first boundaries

	for (; i < items.size(); ++i) {
		const VipPlotItem* it = items[i];
		if (it->testItemAttribute(VipPlotItem::AutoScale) && it->isVisible()) {
			QList<VipAbstractScale*> axes = it->axes();
			int index = axes.indexOf(const_cast<VipAbstractScale*>(this));
			if (index >= 0) {
				QList<VipInterval> intervals = it->plotBoundingIntervals();
				if (index < intervals.size()) {
					const VipInterval inter = intervals[index];
					if (!bounds.isValid())
						bounds = inter;
					else if (inter.isValid())
						bounds = bounds.unite(inter);
				}
			}
		}
	}

	// compute the union boundaries
	// for (; i < items.size(); ++i)
	//  {
	//  const VipPlotItem* it = items[i];
	//  if (it->testItemAttribute(VipPlotItem::AutoScale) && it->isVisible())
	//  {
	//  QList<VipAbstractScale*> axes = it->axes();
	//  int index = axes.indexOf(const_cast<VipAbstractScale*>(this));
	//  if (index >= 0)
	//  {
	//	QList<VipInterval> intervals = it->plotBoundingIntervals();
	//	if (index < intervals.size())
	//		bounds = bounds.unite(intervals[index]);
	//
	// }
	// }
	// }

	return bounds;
}

void VipAbstractScale::computeScaleDiv()
{
	if (!isAutoScale() || plotItems().size() < 1)
		return;

	VipInterval bounds = itemsInterval();

	if (bounds != d_data->computedInterval) {
		if (bounds.width() == 0) {
			// double val;
			//  int exponent;
			//  val = vipFrexp10(bounds.minValue(), &exponent);
			//  bounds.setMaxValue((val+1) * std::pow(10, exponent));
			bounds.setMinValue(bounds.minValue() - 0.5);
			bounds.setMaxValue(bounds.maxValue() + 0.5);
		}

		bool fast_update = false;
		bool keep_previous_interval = false;
		bool might_stream = d_data->computedInterval.isValid() && d_data->optimizeForStreaming;

		d_data->computedInterval = bounds;
		vip_double stepSize = 0.;
		vip_double x1 = bounds.minValue();
		vip_double x2 = bounds.maxValue();

		if (might_stream) {
			if (d_data->lastScaleIntervalUpdate == 0) {
				d_data->lastScaleIntervalUpdate = QDateTime::currentMSecsSinceEpoch();
				d_data->lastScaleIntervalWidth = x2 - x1;
			}
			else {
				qint64 current = QDateTime::currentMSecsSinceEpoch();
				qint64 elapsed = current - d_data->lastScaleIntervalUpdate;
				d_data->lastScaleIntervalUpdate = current;
				// update at least every 500ms: fast update due to streaming.
				// Do not apply auto scaling to avoid flickering.
				fast_update = elapsed < 300;

				if (fast_update) {
					vip_double current_interval = x2 - x1;
					// if current interval is the same than previous one by a margin of 1%, keep old one to avoid flickering
					vip_double factor = std::abs(current_interval - d_data->lastScaleIntervalWidth) / d_data->lastScaleIntervalWidth;
					if (factor < /*0.02*/ d_data->optimizeForStreamingFactor && d_data->lastScaleIntervalWidth > current_interval) {
						keep_previous_interval = true;
					}
					else {
						d_data->lastScaleIntervalWidth = current_interval;
					}
				}
			}
		}

		scaleEngine()->autoScale(maxMajor(), x1, x2, stepSize);

		if (fast_update) {
			x1 = bounds.minValue();
			x2 = bounds.maxValue();
		}
		if (keep_previous_interval) {
			// x1 = x2 - d_data->lastScaleIntervalWidth;
			x2 = x1 + d_data->lastScaleIntervalWidth;
		}

		scaleEngine()->onComputeScaleDiv(this, VipInterval(x1, x2));
		VipScaleDiv div = scaleEngine()->divideScale(x1, x2, maxMajor(), maxMinor(), stepSize);

		if (d_data->itemIntervalFactor && !d_data->optimizeForStreaming) {
			vip_double add = (bounds.maxValue() - bounds.minValue()) * d_data->itemIntervalFactor;
			if (bounds.minValue() - add < x1)
				bounds.setMinValue(bounds.minValue() - add);
			else
				bounds.setMinValue(x1);
			if (bounds.maxValue() + add > x2)
				bounds.setMaxValue(bounds.maxValue() + add);
			else
				bounds.setMaxValue(x2);

			div.setInterval(bounds);
		}

		this->setScaleDiv(div);
	}
}

void VipAbstractScale::setAutoScale(bool enable)
{
	if (d_data->autoScale != enable) {
		invalidateFullExtent();
		d_data->computedInterval = VipInterval();
		d_data->autoScale = enable;
		this->computeScaleDiv();
		this->update();

		emitScaleNeedUpdate();
		Q_EMIT autoScaleChanged(enable);
	}
}

bool VipAbstractScale::isAutoScale() const
{
	return d_data->autoScale;
}

void VipAbstractScale::setOptimizeFromStreaming(bool enable, double factor)
{
	d_data->optimizeForStreaming = enable;
	if (enable) {
		if (vipIsNan(factor))
			d_data->optimizeForStreamingFactor = 0.02;
		else
			d_data->optimizeForStreamingFactor = factor;
	}
}
bool VipAbstractScale::optimizeForStreaming() const
{
	return d_data->optimizeForStreaming;
}

void VipAbstractScale::setItemIntervalFactor(double f)
{
	d_data->itemIntervalFactor = f;
	emitScaleNeedUpdate();
}
double VipAbstractScale::itemIntervalFactor() const
{
	return d_data->itemIntervalFactor;
}

void VipAbstractScale::setBorderDist(double dist1, double dist2)
{
	if (dist1 != d_data->borderDist[0] || dist2 != d_data->borderDist[1]) {
		invalidateFullExtent();
		d_data->borderDist[0] = dist1;
		d_data->borderDist[1] = dist2;
		emitScaleNeedUpdate();
	}
}

double VipAbstractScale::startBorderDist() const
{
	return d_data->borderDist[0];
}

double VipAbstractScale::endBorderDist() const
{
	return d_data->borderDist[1];
}

void VipAbstractScale::getBorderDistHint(double& start, double& end) const
{
	if (start < d_data->minBorderDist[0])
		start = d_data->minBorderDist[0];
	if (start > d_data->maxBorderDist[0])
		start = d_data->maxBorderDist[0];

	if (end < d_data->minBorderDist[1])
		end = d_data->minBorderDist[1];
	if (end > d_data->maxBorderDist[1])
		end = d_data->maxBorderDist[1];
}

void VipAbstractScale::setMinBorderDist(double start, double end)
{
	if (d_data->minBorderDist[0] != start || d_data->minBorderDist[1] != end) {
		invalidateFullExtent();
		d_data->minBorderDist[0] = start;
		d_data->minBorderDist[1] = end;
		emitScaleNeedUpdate();
	}
}

void VipAbstractScale::getMinBorderDist(double& start, double& end) const
{
	start = d_data->minBorderDist[0];
	end = d_data->minBorderDist[1];
}

void VipAbstractScale::getMaxBorderDist(double& start, double& end) const
{
	start = d_data->maxBorderDist[0];
	end = d_data->maxBorderDist[1];
}

double VipAbstractScale::startMinBorderDist() const
{
	return d_data->minBorderDist[0];
}

double VipAbstractScale::endMinBorderDist() const
{
	return d_data->minBorderDist[1];
}

double VipAbstractScale::startMaxBorderDist() const
{
	return d_data->maxBorderDist[0];
}

double VipAbstractScale::endMaxBorderDist() const
{
	return d_data->maxBorderDist[1];
}

void VipAbstractScale::setMaxBorderDist(double start, double end)
{
	if (d_data->maxBorderDist[0] != start || d_data->maxBorderDist[1] != end) {
		invalidateFullExtent();
		d_data->maxBorderDist[0] = start;
		d_data->maxBorderDist[1] = end;
		emitScaleNeedUpdate();
	}
}

void VipAbstractScale::setTitle(const VipText& title)
{
	// if ( d_data->title != title )
	{
		invalidateFullExtent();
		VipPaintItem::setTitle(title);
		// d_data->title.setRenderHints(title.renderHints() | QPainter::TextAntialiasing);
		// d_data->title.setCached(true);
		Q_EMIT titleChanged(title);
		markStyleSheetDirty();
		emitGeometryNeedUpdate();
	}
}

void VipAbstractScale::clearTitle()
{
	invalidateFullExtent();
	setTitle(VipText(QString(), title().textStyle()));
}

void VipAbstractScale::enableDrawTitle(bool draw_title)
{
	if (draw_title != d_data->drawTitle) {
		invalidateFullExtent();
		d_data->drawTitle = draw_title;
		emitGeometryNeedUpdate();
	}
}

bool VipAbstractScale::isDrawTitleEnabled() const
{
	return d_data->drawTitle;
}

QVariant VipAbstractScale::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemVisibleChange) {
		emitGeometryNeedUpdate();
	}
	else if (change == QGraphicsItem::ItemVisibleHasChanged) {
		Q_EMIT visibilityChanged(this->isVisible());
	}
	else if (change == QGraphicsItem::ItemSelectedHasChanged) {
		Q_EMIT selectionChanged(this->isSelected());
		markStyleSheetDirty();
	}

	return VipBoxGraphicsWidget::itemChange(change, value);
}

bool VipAbstractScale::sceneEvent(QEvent* event)
{
	bool res = VipBoxGraphicsWidget::sceneEvent(event);

	if (event->type() == QEvent::GraphicsSceneMousePress) {
		QPointF pt = this->mapFromScene(static_cast<QGraphicsSceneMouseEvent*>(event)->scenePos());
		Q_EMIT mouseButtonPress(this, static_cast<VipPlotItem::MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()), this->value(pt));
	}
	else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
		QPointF pt = this->mapFromScene(static_cast<QGraphicsSceneMouseEvent*>(event)->scenePos());
		Q_EMIT mouseButtonRelease(this, static_cast<VipPlotItem::MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()), this->value(pt));
	}
	else if (event->type() == QEvent::GraphicsSceneMouseMove) {
		QPointF pt = this->mapFromScene(static_cast<QGraphicsSceneMouseEvent*>(event)->scenePos());
		Q_EMIT mouseButtonMove(this, static_cast<VipPlotItem::MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()), this->value(pt));
	}
	else if (event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
		QPointF pt = this->mapFromScene(static_cast<QGraphicsSceneMouseEvent*>(event)->scenePos());
		Q_EMIT mouseButtonDoubleClick(this, static_cast<VipPlotItem::MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()), this->value(pt));
	}

	return res;
}

void VipAbstractScale::setMargin(double margin)
{
	invalidateFullExtent();
	margin = qMax(0., margin);
	if (margin != d_data->margin) {
		d_data->margin = margin;
		markStyleSheetDirty();
		emitGeometryNeedUpdate();
	}
}

void VipAbstractScale::setSpacing(double spacing)
{
	invalidateFullExtent();
	spacing = qMax(0., spacing);
	if (spacing != d_data->spacing) {
		d_data->spacing = spacing;
		markStyleSheetDirty();
		emitGeometryNeedUpdate();
	}
}

double VipAbstractScale::margin() const
{
	return d_data->margin;
}

double VipAbstractScale::spacing() const
{
	return d_data->spacing;
}

void VipAbstractScale::setScaleInverted(bool invert)
{
	invalidateFullExtent();
	d_data->scaleInverted = invert;
	this->setScaleDiv(this->scaleDiv());
}

bool VipAbstractScale::isScaleInverted() const
{
	return d_data->scaleInverted;
}

void VipAbstractScale::setTransformation(VipValueTransform* transformation)
{
	invalidateFullExtent();
	d_data->scaleDraw->setTransformation(transformation);

	// set the transformation to all synchronized axis
	for (int i = 0; i < d_data->synchronizedWith.size(); ++i) {
		if (VipAbstractScale* scale = d_data->synchronizedWith[i]) {
			if (scale != this && (scale->transformation() != transformation) &&
			    (!scale->transformation() || !transformation || transformation->type() != scale->transformation()->type())) {
				if (transformation)
					scale->setTransformation(transformation->copy());
				else
					scale->setTransformation(nullptr);
			}
		}
	}

	emitScaleNeedUpdate();
}

const VipValueTransform* VipAbstractScale::transformation() const
{
	return d_data->scaleDraw->transformation();
}

void VipAbstractScale::setScale(vip_double min, vip_double max, vip_double stepSize)
{
	this->setScaleDiv(this->scaleEngine()->divideScale(min, max, d_data->maxMajor, d_data->maxMinor, stepSize));
}

void VipAbstractScale::setMaxMajor(int maxMajor)
{
	d_data->maxMajor = maxMajor;
	VipInterval inter = this->scaleDiv().bounds();
	invalidateFullExtent();
	setScale(inter.minValue(), inter.maxValue(), 0);
}

void VipAbstractScale::setMaxMinor(int maxMinor)
{
	d_data->maxMinor = maxMinor;
	VipInterval inter = this->scaleDiv().bounds();
	invalidateFullExtent();
	setScale(inter.minValue(), inter.maxValue(), 0);
}

int VipAbstractScale::maxMajor() const
{
	return d_data->maxMajor;
}

int VipAbstractScale::maxMinor() const
{
	return d_data->maxMinor;
}

void VipAbstractScale::setScaleEngine(VipScaleEngine* engine)
{
	delete d_data->scaleEngine;
	d_data->scaleEngine = engine;
	setTransformation(d_data->scaleEngine->transformation());
}

VipScaleEngine* VipAbstractScale::scaleEngine() const
{
	return d_data->scaleEngine;
}

void VipAbstractScale::invalidateFullExtent()
{
	d_data->cacheFullExtent = -1;
}
double VipAbstractScale::cachedFullExtent() const
{
	return d_data->cacheFullExtent;
}
void VipAbstractScale::setCachedFullExtent(double ext)
{
	d_data->cacheFullExtent = ext;
}

void VipAbstractScale::setScaleDiv(const VipInterval& bounds, const VipScaleDiv::TickList& majorTicks)
{
	VipScaleDiv div;
	div.setInterval(bounds);
	div.setTicks(VipScaleDiv::MajorTick, majorTicks);
	setScaleDiv(div);
}

void VipAbstractScale::setScaleDiv(const VipScaleDiv& div, bool force_check_geometry, bool disable_scale_signal)
{
	VipScaleDiv scaleDiv;
	if (d_data->scaleInverted)
		scaleDiv = div.inverted();
	else
		scaleDiv = div;

	VipAbstractScaleDraw* sd = d_data->scaleDraw;
	if (sd->scaleDiv() != scaleDiv || force_check_geometry) {
		// only update items if the bounds change
		bool update_items = (scaleDiv.bounds() != sd->scaleDiv().bounds());
		double old_extent = cachedFullExtent(); // sd->fullExtent();
		if (old_extent < 0) {
			old_extent = sd->fullExtent();
		}

		sd->setScaleDiv(scaleDiv);

		emitScaleDivChanged(update_items, !disable_scale_signal);

		// the geometry must be updated if the scale extent changes
		double fe = sd->fullExtent();
		if (fe != old_extent)
			emitGeometryNeedUpdate();
		setCachedFullExtent(fe);
	}
}

const VipScaleDiv& VipAbstractScale::scaleDiv() const
{
	return d_data->scaleDraw->scaleDiv();
}

void VipAbstractScale::setScaleDraw(VipAbstractScaleDraw* scaleDraw)
{
	if ((scaleDraw == nullptr) || (scaleDraw == d_data->scaleDraw))
		return;

	const VipAbstractScaleDraw* sd = d_data->scaleDraw;
	if (sd) {
		scaleDraw->setScaleDiv(sd->scaleDiv());

		VipValueTransform* transform = nullptr;
		if (sd->scaleMap().transformation())
			transform = sd->scaleMap().transformation()->copy();

		scaleDraw->setTransformation(transform);
		scaleDraw->enableLabelOverlapping(sd->labelOverlappingEnabled());
		scaleDraw->setAdditionalLabelOverlapp(sd->additionalLabelOverlapp());
	}

	delete d_data->scaleDraw;
	d_data->scaleDraw = scaleDraw;
	markStyleSheetDirty();
	emitGeometryNeedUpdate();
}

const VipAbstractScaleDraw* VipAbstractScale::constScaleDraw() const
{
	return d_data->scaleDraw;
}

VipAbstractScaleDraw* VipAbstractScale::scaleDraw()
{
	invalidateFullExtent();
	return d_data->scaleDraw;
}

bool VipAbstractScale::hasUnit() const
{
	for (int i = 0; i < d_data->plotItems.size(); ++i) {
		VipPlotItem* it = d_data->plotItems[i];
		int index = it->axes().indexOf(const_cast<VipAbstractScale*>(this));
		if (index >= 0) {
			if (it->hasAxisUnit(index))
				return true;
		}
	}
	return false;
}

bool VipAbstractScale::hasNoUnit(VipPlotItem* excluded) const
{
	for (int i = 0; i < d_data->plotItems.size(); ++i) {
		VipPlotItem* it = d_data->plotItems[i];
		if (it == excluded)
			continue;
		int index = it->axes().indexOf(const_cast<VipAbstractScale*>(this));
		if (index >= 0) {
			if (it->hasAxisUnit(index))
				return false;
		}
	}
	return true;
}

QPointF VipAbstractScale::position(vip_double value, double length, Vip::ValueType type) const
{
	return constScaleDraw()->position(value, length, type);
}

vip_double VipAbstractScale::value(const QPointF& position) const
{
	return constScaleDraw()->value(position);
}

double VipAbstractScale::convert(vip_double value, Vip::ValueType type) const
{
	return constScaleDraw()->convert(value, type);
}

double VipAbstractScale::angle(vip_double value, Vip::ValueType type) const
{
	return constScaleDraw()->angle(value, type);
}

QPointF VipAbstractScale::start() const
{
	return constScaleDraw()->start();
}

QPointF VipAbstractScale::end() const
{
	return constScaleDraw()->end();
}

void VipAbstractScale::synchronizeWith(VipAbstractScale* other)
{
	QPointer<VipAbstractScale> item = other;

	if (d_data->synchronizedWith.indexOf(item) < 0 && other != this && other) {
		desynchronize(other);

		const VipScaleDiv& div = other->scaleDiv();
		this->setScaleDiv(div);

		d_data->synchronizedWith << other;
		other->synchronizeWith(this);
	}
}

QSet<VipAbstractScale*> VipAbstractScale::synchronizedWith() const
{
	// build the list of all synchronized items
	QSet<VipAbstractScale*> synchronized;

	for (int i = 0; i < d_data->synchronizedWith.size(); ++i) {
		if (VipAbstractScale* sync = d_data->synchronizedWith[i]) {
			synchronized.insert(sync);
			for (int j = 0; j < sync->d_data->synchronizedWith.size(); ++j) {
				if (VipAbstractScale* s = sync->d_data->synchronizedWith[j])
					if (s != this)
						synchronized.insert(s);
			}
		}
	}

	synchronized.remove(const_cast<VipAbstractScale*>(this));
	return synchronized;
}

void VipAbstractScale::desynchronize(VipAbstractScale* other)
{
	QPointer<VipAbstractScale> item = other;

	if (d_data->synchronizedWith.indexOf(item) >= 0) {
		d_data->synchronizedWith.removeOne(item);
		other->desynchronize(this);
	}
}

void VipAbstractScale::desynchronize()
{
	for (int i = 0; i < d_data->synchronizedWith.size(); ++i) {
		if (d_data->synchronizedWith[i])
			desynchronize(d_data->synchronizedWith[i].data());
	}

	d_data->synchronizedWith.clear();
}

void VipAbstractScale::synchronize()
{
	const QSet<VipAbstractScale*> synchronized = synchronizedWith();
	const VipScaleDiv& div = this->scaleDiv();

	// update synchronized items
	for (QSet<VipAbstractScale*>::const_iterator it = synchronized.begin(); it != synchronized.end(); ++it) {
		VipAbstractScale* sync = const_cast<VipAbstractScale*>((*it));
		// sync->disconnect(SIGNAL(scaleDivChanged(bool)),sync,SLOT(synchronize()));
		sync->setScaleDiv(div, false, true);
		// sync->connect(sync,SIGNAL(scaleDivChanged(bool)),sync,SLOT(synchronize()),Qt::DirectConnection);
	}
}

void VipAbstractScale::draw(QPainter* painter, QWidget* w)
{
	d_data->dirtyItems = false;
	VipBoxGraphicsWidget::draw(painter, w);
}

void VipAbstractScale::setTextStyle(const VipTextStyle& p, VipScaleDiv::TickType tick)
{
	scaleDraw()->setTextStyle(p, tick);
	markStyleSheetDirty();
	emitGeometryNeedUpdate();
}

const VipTextStyle& VipAbstractScale::textStyle(VipScaleDiv::TickType tick) const
{
	return constScaleDraw()->textStyle(tick);
}

void VipAbstractScale::setLabelTransform(const QTransform& tr, VipScaleDiv::TickType tick)
{
	scaleDraw()->setLabelTransform(tr, tick);
	emitGeometryNeedUpdate();
}

QTransform VipAbstractScale::labelTransform(VipScaleDiv::TickType tick) const
{
	return constScaleDraw()->labelTransform(tick);
}

const QList<VipPlotItem*>& VipAbstractScale::plotItems() const
{
	return d_data->plotItems;
}

QList<VipPlotItem*> VipAbstractScale::synchronizedPlotItems() const
{
	QList<VipPlotItem*> items;
	for (int i = 0; i < d_data->synchronizedWith.size(); ++i)
		items.append(d_data->synchronizedWith[i]->plotItems());

	return vipToSet(items).values();
}

QGraphicsView* VipAbstractScale::view() const
{
	return VipAbstractScale::view(this);
}

QGraphicsView* VipAbstractScale::view(const QGraphicsItem* item)
{
	if (!item->scene())
		return nullptr;

	QList<QGraphicsView*> v = item->scene()->views();
	if (!v.size())
		return nullptr;

	return v[0];
}

QList<VipPlotItem*> VipAbstractScale::axisItems(const VipAbstractScale* x, const VipAbstractScale* y)
{
	QList<VipPlotItem*> x_items;
	QList<VipPlotItem*> y_items;

	if (x)
		x_items = x->d_data->plotItems;
	if (y)
		y_items = y->d_data->plotItems;

	QList<VipPlotItem*> items;
	for (int i = 0; i < x_items.size(); ++i) {
		if (y_items.indexOf(x_items[i]) >= 0)
			items.append(x_items[i]);
	}

	return items;
}

QList<VipInterval> VipAbstractScale::scaleIntervals(const QList<VipAbstractScale*>& axes)
{
	QList<VipInterval> res;
	for (int i = 0; i < axes.size(); ++i) {
		if (axes[i])
			res << axes[i]->scaleDiv().bounds().normalized();
		else
			res << VipInterval();
	}
	return res;
}

QTransform VipAbstractScale::globalSceneTransform(const QGraphicsItem* item)
{
	QTransform tr;

	if (item->flags() & ItemIgnoresTransformations) {
		QGraphicsView* v = view(item);
		if (v)
			tr = item->deviceTransform(v->viewportTransform()) * v->viewportTransform().inverted();
	}
	else
		tr = item->sceneTransform();

	return tr;
}

QTransform VipAbstractScale::parentTransform(const QGraphicsItem* item)
{
	if (!item->parentItem())
		return globalSceneTransform(item);

	QTransform tr;

	// if((item->flags() & ItemIgnoresTransformations) || (item->parentItem()->flags() & ItemIgnoresTransformations))
	//  {
	//  tr = globalSceneTransform(item) * globalSceneTransform(item->parentItem()).inverted();
	//  }
	//  else
	tr = item->itemTransform(item->parentItem());

	return tr;
}

void VipAbstractScale::emitScaleDivChanged(bool bounds_changed, bool emit_signal)
{
	if (emit_signal)
		Q_EMIT scaleDivChanged(bounds_changed);
	update();
	updateItems();
}

void VipAbstractScale::emitScaleNeedUpdate()
{
	Q_EMIT scaleNeedUpdate();
	this->markStyleSheetDirty();
	if (VipAbstractPlotArea* a = this->area()) {
		a->markNeedUpdate();
		return;
	}
	update();
}

void VipAbstractScale::updateOnStyleSheet()
{
	emitScaleNeedUpdate();
}

static int scaleDivType(const QByteArray& name)
{
	if (name.isEmpty())
		return VipScaleDiv::MajorTick;
	if (name == "minor")
		return VipScaleDiv::MinorTick;
	else if (name == "medium")
		return VipScaleDiv::MediumTick;
	else if (name == "major")
		return VipScaleDiv::MajorTick;
	return -1;
}

bool VipAbstractScale::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "auto-scale") == 0) {
		setAutoScale(value.toBool());
		return true;
	}
	if (strcmp(name, "display") == 0) {
		bool val = value.toBool();
		if (index == "backbone")
			scaleDraw()->enableComponent(VipScaleDraw::Backbone, val);
		else if (index == "ticks")
			scaleDraw()->enableComponent(VipScaleDraw::Ticks, val);
		else if (index == "labels")
			scaleDraw()->enableComponent(VipScaleDraw::Labels, val);
		else if (index == "title")
			this->enableDrawTitle(val);
		else
			return false;
		return true;
	}
	if (strcmp(name, "pen") == 0) {
		QPen p = scaleDraw()->componentPen(VipScaleDraw::Backbone);
		if (value.userType() == qMetaTypeId<QColor>())
			p.setColor((value.value<QColor>()));
		else
			p = (value.value<QPen>());
		scaleDraw()->setComponentPen(VipScaleDraw::Backbone | VipScaleDraw::Ticks, p);

		return true;
	}
	if (strcmp(name, "pen-color") == 0) {
		QPen p = scaleDraw()->componentPen(VipScaleDraw::Backbone);
		if (value.userType() == qMetaTypeId<QColor>())
			p.setColor((value.value<QColor>()));
		else
			p = (value.value<QPen>());
		scaleDraw()->setComponentPen(VipScaleDraw::Backbone | VipScaleDraw::Ticks, p);

		return true;
	}
	if (strcmp(name, "margin") == 0) {
		setMargin(value.toDouble());
		return true;
	}
	if (strcmp(name, "spacing") == 0) {
		setSpacing(value.toDouble());
		return true;
	}
	if (strcmp(name, "inverted") == 0) {
		setScaleInverted(value.toBool());
		return true;
	}
	if (strcmp(name, "label-position") == 0) {
		int id = value.toInt();
		if (id == VipScaleDraw::TextInside)
			scaleDraw()->setTextPosition(VipScaleDraw::TextInside);
		else
			scaleDraw()->setTextPosition(VipScaleDraw::TextOutside);
		return true;
	}
	if (strcmp(name, "ticks-position") == 0) {
		int id = value.toInt();
		if (id == VipScaleDraw::TicksInside)
			scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
		else
			scaleDraw()->setTicksPosition(VipScaleDraw::TicksOutside);
		return true;
	}
	if (strcmp(name, "ticks-length") == 0) {
		int type = scaleDivType(index);
		if (type == -1)
			return false;
		scaleDraw()->setTickLength((VipScaleDiv::TickType)type, value.toDouble());
		return true;
	}
	if (strcmp(name, "label-transform") == 0) {
		int type = scaleDivType(index);
		if (type == -1)
			return false;
		int id = value.toInt();
		if (id == VipScaleDraw::TextHorizontal)
			scaleDraw()->setTextTransform(VipScaleDraw::TextHorizontal, (VipScaleDiv::TickType)type);
		else if (id == VipScaleDraw::TextPerpendicular)
			scaleDraw()->setTextTransform(VipScaleDraw::TextPerpendicular, (VipScaleDiv::TickType)type);
		else if (id == VipScaleDraw::TextCurved)
			scaleDraw()->setTextTransform(VipScaleDraw::TextCurved, (VipScaleDiv::TickType)type);
		else if (id == VipScaleDraw::TextParallel)
			scaleDraw()->setTextTransform(VipScaleDraw::TextParallel, (VipScaleDiv::TickType)type);
		else
			return false;
		return true;
	}
	else {

		int type = scaleDivType(index);
		if (type != -1) {
			VipTextStyle st = textStyle((VipScaleDiv::TickType)type);
			if (VipStandardStyleSheet::handleTextStyleKeyWord(name, value, st, "label-")) {
				setTextStyle(st, (VipScaleDiv::TickType)type);
				if (index.isEmpty()) {
					// set text style to ALL
					setTextStyle(st, VipScaleDiv::MediumTick);
					setTextStyle(st, VipScaleDiv::MinorTick);
					scaleDraw()->setAdditionalTextStyle(st);
				}
				return true;
			}
		}
	}
	return VipBoxGraphicsWidget::setItemProperty(name, value, index);
}

void VipAbstractScale::paint(QPainter* painter, const QStyleOptionGraphicsItem* opt, QWidget* w)
{
	VipBoxGraphicsWidget::paint(painter, opt, w);
}

void VipAbstractScale::emitScaleDivNeedUpdate()
{
	if (VipAbstractPlotArea* a = this->area()) {
		a->markScaleDivDirty(this);
		// TEST
		update();
		return;
	}
	d_data->dirtyScaleDiv = 1;
	Q_EMIT scaleDivNeedUpdate();
	update();
}

void VipAbstractScale::setCacheMode(CacheMode mode, const QSize& logicalCacheSize)
{
	QGraphicsObject::setCacheMode(mode, logicalCacheSize);
	emitGeometryNeedUpdate();
	update();
}

void VipAbstractScale::prepareGeometryChange()
{
	QGraphicsObject::prepareGeometryChange();
}

void VipAbstractScale::emitGeometryNeedUpdate()
{
	// this->markStyleSheetDirty();
	if (VipAbstractPlotArea* a = this->area()) {
		if (a->markGeometryDirty()) {
			updateItems();
			// if (cacheMode() != NoCache)
			//	prepareGeometryChange();
		}
		else {
			update();
			updateItems();
		}
		return;
	}

	Q_EMIT geometryNeedUpdate();
	update();
	updateItems();
}

void VipAbstractScale::updateItems()
{
	d_data->dirtyItems = true;
	if (d_data->plotItems.isEmpty())
		return;
	// update items for this axis and synchronized axes
	QSet<VipPlotItem*> items = vipToSet(d_data->plotItems);
	for (int s = 0; s < d_data->synchronizedWith.size(); ++s)
		items.unite(vipToSet(d_data->synchronizedWith[s]->d_data->plotItems));

	for (QSet<VipPlotItem*>::iterator it = items.begin(); it != items.end(); ++it)
		(*it)->markCoordinateSystemDirty();
}

void VipAbstractScale::delayedRecomputeScaleDiv()
{
	if (d_data->dirtyScaleDiv) {
		d_data->dirtyScaleDiv = 0;
		this->computeScaleDiv();
	}
}

void VipAbstractScale::addItem(VipPlotItem* item)
{
	if (d_data->plotItems.indexOf(item) < 0) {
		d_data->plotItems.append(item);
		d_data->dirtyScaleDiv = 1;
		Q_EMIT itemAdded(item);
	}
}

void VipAbstractScale::removeItem(VipPlotItem* item)
{
	if (d_data->plotItems.removeAll(item)) {
		d_data->dirtyScaleDiv = 1;
		Q_EMIT itemRemoved(item);
	}
}

#include <QBoxLayout>
#include <QCoreApplication>

class VipScaleWidget::PrivateData
{
public:
	VipAbstractScale* scale;

	struct Parameters
	{
		QSharedPointer<QColor> axisColor;
		QSharedPointer<QColor> axisTextColor;
		QSharedPointer<QFont> axisTextFont;
		QSharedPointer<QColor> axisTitleColor;
		QSharedPointer<QFont> axisTitleFont;
		QSharedPointer<QColor> backgroundColor;
	};

	Parameters params;
	QList<Parameters> states;
	bool enableRecomputeGeometry;
};

VipScaleWidget::VipScaleWidget(VipAbstractScale* scale, QWidget* parent)
  : QGraphicsView(parent)
{
	d_data = new PrivateData();
	d_data->scale = nullptr;
	d_data->enableRecomputeGeometry = true;

	QGraphicsScene* sc = new QGraphicsScene();
	this->setScene(sc);
	this->viewport()->setMouseTracking(true);
	this->scene()->setSceneRect(QRectF(0, 0, 1000, 1000));

	this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->setFrameShape(QFrame::NoFrame);

	this->setBackgroundBrush(QBrush());
	setScale(scale);
}

VipScaleWidget::~VipScaleWidget()
{
	delete d_data;
}

QColor VipScaleWidget::backgroundColor() const
{
	return d_data->params.backgroundColor ? *d_data->params.backgroundColor : QColor();
}

bool VipScaleWidget::hasBackgroundColor() const
{
	return d_data->params.backgroundColor;
}

void VipScaleWidget::removeBackgroundColor()
{
	d_data->params.backgroundColor.reset();
}

void VipScaleWidget::setBackgroundColor(const QColor& c)
{
	d_data->params.backgroundColor.reset(new QColor(c));
	update();
}

void VipScaleWidget::paintEvent(QPaintEvent* evt)
{
	QColor c;
	if (hasBackgroundColor()) {
		c = backgroundColor();
		// else
		//	c = qApp->palette(this).color(QPalette::Window);
		QPainter p(viewport());
		p.fillRect(QRect(0, 0, width(), height()), c);
	}
	QGraphicsView::paintEvent(evt);
}

void VipScaleWidget::recomputeGeometry()
{
	if (!d_data->enableRecomputeGeometry)
		return;

	if (VipBorderItem* it = qobject_cast<VipBorderItem*>(scale())) {
		d_data->enableRecomputeGeometry = false;
		// it->recomputeGeometry();
		d_data->enableRecomputeGeometry = true;
		if (it->orientation() == Qt::Vertical) {
			double len = it->extentForLength(sceneRect().height());
			QPoint p1 = mapFromScene(QPointF(0, 0));
			QPoint p2 = mapFromScene(QPointF(len, 0));
			setMaximumWidth(p2.x() - p1.x());
			setMinimumWidth(p2.x() - p1.x());
		}
		else {
			double len = it->extentForLength(sceneRect().width());
			QPoint p1 = mapFromScene(QPointF(0, 0));
			QPoint p2 = mapFromScene(QPointF(0, len));
			setMaximumHeight(p2.y() - p1.y());
			setMaximumHeight(p2.y() - p1.y());
		}
	}
}

void VipScaleWidget::setScale(VipAbstractScale* scale)
{
	if (d_data->scale) {
		disconnect(d_data->scale, SIGNAL(geometryNeedUpdate()), this, SLOT(recomputeGeometry()));

		delete d_data->scale;
	}

	d_data->scale = scale;
	if (scale) {
		scale->setGeometry(QRectF(0, 0, 1000, 1000));
		scale->setMinimumSize(QSizeF(0, 0));
		scene()->addItem(scale);
		connect(scale, SIGNAL(geometryNeedUpdate()), this, SLOT(recomputeGeometry()));
	}
}

VipAbstractScale* VipScaleWidget::scale() const
{
	return const_cast<VipAbstractScale*>(d_data->scale);
}

void VipScaleWidget::resizeEvent(QResizeEvent* evt)
{
	if (d_data->scale) {
		this->setSceneRect(this->viewport()->geometry());
		d_data->scale->setGeometry(this->sceneRect());
		if (VipBorderItem* item = qobject_cast<VipBorderItem*>(d_data->scale))
			item->setBoundingRectNoCorners(this->sceneRect());
		d_data->scale->layoutScale();
		this->onResize();
	}

	QGraphicsView::resizeEvent(evt);
}

VipArchive& operator<<(VipArchive& arch, const VipAbstractScale* value)
{
	arch.content("id", VipUniqueId::id(value));
	arch.content("boxStyle", value->boxStyle());
	arch.content("isAutoScale", value->isAutoScale());
	arch.content("title", value->title());
	arch.content("majorTextStyle", value->textStyle(VipScaleDiv::MajorTick));
	arch.content("mediumTextStyle", value->textStyle(VipScaleDiv::MediumTick));
	arch.content("minorTextStyle", value->textStyle(VipScaleDiv::MinorTick));
	arch.content("majorTransform", value->labelTransform(VipScaleDiv::MajorTick));
	arch.content("mediumTransform", value->labelTransform(VipScaleDiv::MediumTick));
	arch.content("minorTransform", value->labelTransform(VipScaleDiv::MinorTick));
	arch.content("isDrawTitleEnabled", value->isDrawTitleEnabled());
	arch.content("startBorderDist", value->startBorderDist());
	arch.content("endBorderDist", value->endBorderDist());
	arch.content("startMinBorderDist", value->startMinBorderDist());
	arch.content("endMinBorderDist", value->endMinBorderDist());
	arch.content("startMaxBorderDist", value->startMaxBorderDist());
	arch.content("endMaxBorderDist", value->endMaxBorderDist());
	arch.content("margin", value->margin());
	arch.content("spacing", value->spacing());
	arch.content("isScaleInverted", value->isScaleInverted());
	arch.content("maxMajor", value->maxMajor());
	arch.content("maxMinor", value->maxMinor());
	// new in 3.0.1
	arch.content("autoExponent", value->constScaleDraw()->valueToText()->automaticExponent());
	arch.content("minLabelSize", value->constScaleDraw()->valueToText()->maxLabelSize());
	arch.content("exponent", value->constScaleDraw()->valueToText()->exponent());

	arch.content("scaleDiv", value->scaleDiv());
	arch.content("renderHints", (int)value->renderHints());
	arch.content("visible", (int)value->isVisible());
	// save the y scale engine type
	arch.content("yScaleEngine", value->scaleEngine() ? (int)value->scaleEngine()->scaleType() : 0);

	arch.content("styleSheet", value->styleSheetString());

	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipAbstractScale* value)
{
	VipUniqueId::setId(value, arch.read("id").toInt());
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setAutoScale(arch.read("isAutoScale").value<bool>());
	value->setTitle(arch.read("title").value<VipText>());
	value->setTextStyle(arch.read("majorTextStyle").value<VipTextStyle>(), VipScaleDiv::MajorTick);
	value->setTextStyle(arch.read("mediumTextStyle").value<VipTextStyle>(), VipScaleDiv::MediumTick);
	value->setTextStyle(arch.read("minorTextStyle").value<VipTextStyle>(), VipScaleDiv::MinorTick);
	value->setLabelTransform(arch.read("majorTransform").value<QTransform>(), VipScaleDiv::MajorTick);
	value->setLabelTransform(arch.read("mediumTransform").value<QTransform>(), VipScaleDiv::MediumTick);
	value->setLabelTransform(arch.read("minorTransform").value<QTransform>(), VipScaleDiv::MinorTick);
	value->enableDrawTitle(arch.read("isDrawTitleEnabled").value<bool>());
	double startBorderDist = arch.read("startBorderDist").value<double>();
	double endBorderDist = arch.read("endBorderDist").value<double>();
	value->setBorderDist(startBorderDist, endBorderDist);
	double startMinBorderDist = arch.read("startMinBorderDist").value<double>();
	double endMinBorderDist = arch.read("endMinBorderDist").value<double>();
	value->setMinBorderDist(startMinBorderDist, endMinBorderDist);
	double startMaxBorderDist = arch.read("startMaxBorderDist").value<double>();
	double endMaxBorderDist = arch.read("endMaxBorderDist").value<double>();
	value->setMaxBorderDist(startMaxBorderDist, endMaxBorderDist);
	value->setMargin(arch.read("margin").value<double>());
	value->setSpacing(arch.read("spacing").value<double>());
	value->setScaleInverted(arch.read("isScaleInverted").value<bool>());
	value->setMaxMajor(arch.read("maxMajor").value<int>());
	value->setMaxMinor(arch.read("maxMinor").value<int>());

	// new in 3.0.1
	arch.save();
	bool autoExponent = false;
	int minLabelSize = 0, exponent = 0;
	if (arch.content("autoExponent", autoExponent)) {
		arch.content("minLabelSize", minLabelSize);
		arch.content("exponent", exponent);
		value->scaleDraw()->valueToText()->setAutomaticExponent(autoExponent);
		value->scaleDraw()->valueToText()->setMaxLabelSize(minLabelSize);
		value->scaleDraw()->valueToText()->setExponent(exponent);
	}
	else
		arch.restore();

	value->setScaleDiv(arch.read("scaleDiv").value<VipScaleDiv>());
	value->setRenderHints((QPainter::RenderHints)arch.read("renderHints").value<int>());
	value->setVisible(arch.read("visible").toBool());
	int engine = arch.read("yScaleEngine").toInt();
	if (!value->scaleEngine() || engine != value->scaleEngine()->scaleType()) {
		if (engine == VipScaleEngine::Linear)
			value->setScaleEngine(new VipLinearScaleEngine());
		else if (engine == VipScaleEngine::Log10)
			value->setScaleEngine(new VipLog10ScaleEngine());
	}

	arch.resetError();

	arch.save();
	QString st;
	if (arch.content("styleSheet", st)) {
		if (!st.isEmpty())
			value->setStyleSheet(st);
	}
	else
		arch.restore();

	return arch;
}

static int register_types()
{
	qRegisterMetaType<VipAbstractScale*>();
	vipRegisterArchiveStreamOperators<VipAbstractScale*>();
	return 0;
}
static int _register_types = register_types();