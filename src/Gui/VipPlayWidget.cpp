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

#include "VipPlayWidget.h"
#include "VipColorMap.h"
#include "VipCorrectedTip.h"
#include "VipDisplayObject.h"
#include "VipDoubleSlider.h"
#include "VipIODevice.h"
#include "VipPainter.h"
#include "VipPlotGrid.h"
#include "VipProgress.h"
#include "VipScaleEngine.h"
#include "VipStandardWidgets.h"
#include "VipToolTip.h"

#include <QAction>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QSpinBox>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <qapplication.h>

//#include <iostream>

#define ITEM_START_HEIGHT 0.1
#define ITEM_END_HEIGHT 0.9

static QString lockedMessage()
{
	static const QString res = "<b>Time ranges are locked!</b><br>"
				   "You cannot move the time ranges while locked.<br>"
				   "To unlock them, uncheck the " +
				   QString(vipToHtml(vipPixmap("lock.png"), "align ='middle'")) + " icon";
	return res;
}

class VipTimeRangeItem::PrivateData
{
public:
	PrivateData()
	  : parentItem(nullptr)
	  , heights(ITEM_START_HEIGHT, ITEM_END_HEIGHT)
	  , left(0)
	  , right(0)
	  , color(Qt::blue)
	  , reverse(false)
	  , selection(-2)
	{
	}

	VipTimeRange initialTimeRange;
	VipTimeRangeListItem* parentItem;
	QPair<double, double> heights;
	qint64 left, right;
	QRectF moveAreaRect;
	QRectF boundingRect;
	QColor color;
	bool reverse;

	QPointF pos; // for mouse moving
	QMap<VipTimeRangeItem*, QPair<qint64, qint64>> initPos;
	int selection; // mouse selection (-1=left, 0=middle, 1=right)
};

VipTimeRangeItem::VipTimeRangeItem(VipTimeRangeListItem* item)
  : VipPlotItem(VipText())
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->parentItem = item;

	this->setItemAttribute(HasLegendIcon, false);
	this->setItemAttribute(VisibleLegend, false);
	this->setItemAttribute(AutoScale);
	this->setItemAttribute(HasToolTip);
	this->setIgnoreStyleSheet(true);
	if (d_data->parentItem) {
		// register into parent
		d_data->parentItem->addItem(this);
		connect(this, SIGNAL(timeRangeChanged()), d_data->parentItem, SLOT(updateDevice()));
		this->setAxes(d_data->parentItem->axes(), VipCoordinateSystem::Cartesian);
	}
}

VipTimeRangeItem::~VipTimeRangeItem()
{
	if (d_data->parentItem)
		d_data->parentItem->removeItem(this);
}

QList<VipInterval> VipTimeRangeItem::plotBoundingIntervals() const
{
	return QList<VipInterval>() << VipInterval(d_data->left, d_data->right) << VipInterval(d_data->heights.first, d_data->heights.second);
}

void VipTimeRangeItem::setInitialTimeRange(const VipTimeRange& range)
{
	d_data->initialTimeRange = range;
	setCurrentTimeRange(range);
}

VipTimeRange VipTimeRangeItem::initialTimeRange() const
{
	return d_data->initialTimeRange;
}

void VipTimeRangeItem::setCurrentTimeRange(const VipTimeRange& r)
{
	if (d_data->left != r.first || d_data->right != r.second) {
		// bool first_set = d_data->left == 0 && d_data->right == 0;
		d_data->left = r.first;
		d_data->right = r.second;

		Q_EMIT timeRangeChanged();
		emitItemChanged();
	}
}

void VipTimeRangeItem::setCurrentTimeRange(qint64 left, qint64 right)
{
	if (d_data->left != left || d_data->right != right) {
		d_data->left = left;
		d_data->right = right;
		Q_EMIT timeRangeChanged();
		emitItemChanged();
	}
}

VipTimeRange VipTimeRangeItem::currentTimeRange()
{
	VipTimeRange res(d_data->left, d_data->right);
	if (d_data->reverse)
		res = vipReorder(res, Vip::Descending);
	else
		res = vipReorder(res, Vip::Ascending);
	return res;
}

qint64 VipTimeRangeItem::left() const
{
	return d_data->left;
}

qint64 VipTimeRangeItem::right() const
{
	return d_data->right;
}

VipTimeRangeListItem* VipTimeRangeItem::parentItem() const
{
	return d_data->parentItem;
}

void VipTimeRangeItem::setHeights(double start, double end)
{
	if (d_data->heights != QPair<double, double>(start, end)) {
		d_data->heights = QPair<double, double>(start, end);
		emitItemChanged();
	}
}

QPair<double, double> VipTimeRangeItem::heights() const
{
	return d_data->heights;
}

void VipTimeRangeItem::setColor(const QColor& c)
{
	if (d_data->color != c) {
		d_data->color = c;
		emitItemChanged();
	}
}

QColor VipTimeRangeItem::color() const
{
	return d_data->color;
}

static QLinearGradient& buildVerticalGradient(const QRectF& r, const QColor& c)
{
	static QLinearGradient grad;
	const QGradientStops stops = grad.stops();
	if (!stops.size() || stops[1].second != c || grad.start().y() != r.top() || grad.finalStop().y() != r.bottom()) {
		grad = QLinearGradient(r.topLeft(), r.bottomLeft());
		QGradientStops stops;
		stops << QGradientStop(0, c.darker(120));
		stops << QGradientStop(ITEM_START_HEIGHT, c);
		stops << QGradientStop(ITEM_END_HEIGHT, c);
		stops << QGradientStop(1, c.darker(120));
		grad.setStops(stops);
	}
	return grad;
}

/* QRectF VipTimeRangeItem::boundingRect() const
{
	return shape().boundingRect();
}*/

// QPainterPath VipTimeRangeItem::shape() const
QPainterPath VipTimeRangeItem::shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const
{
	VipTimeRangeItem* _this = const_cast<VipTimeRangeItem*>(this);

	QRectF r = m->transformRect(QRectF(QPointF(d_data->left, d_data->heights.second), QPointF(d_data->right, d_data->heights.first))).normalized();
	if (r.width() == 0) {
		r.setRight(r.right() + 1);
		r.setLeft(r.left() - 1);
	}
	r.setTop(r.top() - 1);

	/* if (r != d_data->moveAreaRect) {
		_this->prepareGeometryChange();
		_this->QGraphicsObject::update();
	}*/
	_this->d_data->moveAreaRect = r;

	QPainterPath p;
	if (d_data->left != d_data->right) {
		r.setLeft(r.left() - 9);
		r.setRight(r.right() + 9);
	}
	d_data->boundingRect = r;
	p.addRect(r);
	return p;
}

QRectF VipTimeRangeItem::boundingRect() const
{
	return shape().boundingRect();
	if (this->isDirtyShape()) {
		shape();
	}
	return d_data->boundingRect;
}

void VipTimeRangeItem::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	Q_UNUSED(m)

	if (this->isDirtyShape()) {
		shape();
	}
	QRectF r = d_data->moveAreaRect;
	// draw the rectange
	{
		const QLinearGradient& grad = buildVerticalGradient(r, d_data->color);

		p->setRenderHint(QPainter::Antialiasing, false);
		if (d_data->left != d_data->right) {

			QPen pen(d_data->color.darker(120));
			QBrush brush(grad);
			p->setPen(pen);
			p->setBrush(brush);

			p->drawRect(r);
		}
		else {
			QPen pen(QBrush(grad), 1);
			p->setPen(pen);
			double x = r.center().x();
			p->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
		}
	}

	QRectF direction = r;
	if (d_data->reverse)
		direction.setLeft(r.right() - 5);
	else
		direction.setRight(r.left() + 5);

	// Draw the direction rectangle that indicates where the time range starts.
	// If the time range is too small, do not draw it to avoid overriding the display
	// p->setPen(pen);

	QRectF direction_rect = direction & r;
	if (direction_rect.width() > 4) {

		// QColor c(255, 165, 0);
		QColor c = d_data->color.darker(150);
		QBrush brush(c);
		p->setBrush(brush);
		p->drawRect(direction_rect);
	}

	// draw the left and right arrow if the rectangle width is > 10
	/* if (r.width() > 10 && !d_data->resizeLeft.isEmpty() && !d_data->resizeRight.isEmpty()) {
		p->setRenderHints(QPainter::Antialiasing);
		p->setPen(QPen(d_data->color.darker(120), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p->setBrush(d_data->color);
		p->drawPath(d_data->resizeLeft);
		p->drawPath(d_data->resizeRight);
	}*/

	// draw the selection
	if (isSelected()) {
		p->setRenderHints(QPainter::Antialiasing);
		p->setPen(QPen(d_data->color.darker(120)));
		p->setBrush(QBrush(d_data->color.darker(120), Qt::BDiagPattern));
		p->drawRect(r); // drawRoundedRect(r,3,3);
	}
}

void VipTimeRangeItem::drawSelected(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	draw(p, m);
}

bool VipTimeRangeItem::applyTransform(const QTransform& tr)
{
	d_data->left = qRound64(tr.map(QPointF(d_data->left, 0)).x());
	d_data->right = qRound64(tr.map(QPointF(d_data->right, 0)).x());
	update();
	return true;
}

void VipTimeRangeItem::setReverse(bool reverse)
{
	if (reverse != d_data->reverse) {
		d_data->reverse = reverse;

		Q_EMIT timeRangeChanged();
		emitItemChanged();
	}
}

bool VipTimeRangeItem::reverse() const
{
	return d_data->reverse;
}

int VipTimeRangeItem::selection(const QPointF& pos) const
{
	if (d_data->left != d_data->right) {
		QRectF left = d_data->moveAreaRect;
		left.setRight(left.left() - 9);
		QRectF right = d_data->moveAreaRect;
		right.setLeft(right.right() + 9);

		if (left.contains(pos))
			return -1;
		else if (right.contains(pos))
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

void VipTimeRangeItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
	QPointF p = event->pos();
	int s = selection(p);
	if (s != 0)
		this->setCursor(Qt::SizeHorCursor);
	else
		this->setCursor( // Qt::SizeAllCursor
		  QCursor());
}

void VipTimeRangeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	if (parentItem()->area()->timeRangesLocked()) {
		VipCorrectedTip::showText(QCursor::pos(), lockedMessage(), nullptr, QRect(), 5000);
		return;
	}

	qint64 diff = qRound64((sceneMap()->invTransform(event->pos()) - sceneMap()->invTransform(d_data->pos)).x());

	if (d_data->selection == -1) { // left arrow
		setCurrentTimeRange(parentItem()->closestTime(d_data->initPos[this].first + diff), d_data->initPos[this].second);
		Q_EMIT parentItem()->itemsTimeRangeChanged(QList<VipTimeRangeItem*>() << this);
	}
	else if (d_data->selection == 1) { // right arrow
		setCurrentTimeRange(d_data->initPos[this].first, parentItem()->closestTime(d_data->initPos[this].second + diff));
		Q_EMIT parentItem()->itemsTimeRangeChanged(QList<VipTimeRangeItem*>() << this);
	}
	else if (d_data->selection == 0) {
		// apply to all selected items
		for (QMap<VipTimeRangeItem*, QPair<qint64, qint64>>::iterator it = d_data->initPos.begin(); it != d_data->initPos.end(); ++it) {
			VipTimeRangeItem* item = it.key();
			item->setCurrentTimeRange(item->parentItem()->closestTime(it.value().first + diff), item->parentItem()->closestTime(it.value().second + diff));
		}

		Q_EMIT parentItem()->itemsTimeRangeChanged(d_data->initPos.keys());
	}

	if (parentItem()) {
		parentItem()->computeToolTip();
	}

	// if(diff != 0)
	//	d_data->pos = event->pos();

	if (d_data->selection != -2)
		emitItemChanged();
}

void VipTimeRangeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	VipPlotItem::mousePressEvent(event);
	d_data->pos = event->pos();
	d_data->selection = selection(d_data->pos);

	// Small trick: a time range of 1ns means that this is an empty time range.
	// we just to specify a valid time range (> 0 ns) to display the actual VipTimeRangeItem.
	if (d_data->selection != 0 && d_data->initialTimeRange.second - d_data->initialTimeRange.first == 0) // 1)
		d_data->selection = 0;

	d_data->initPos.clear();
	d_data->initPos[this] = QPair<qint64, qint64>(d_data->left, d_data->right);
	if (d_data->selection == 0) {
		// compute initial left and right values for all selected items
		QList<VipTimeRangeItem*> items = d_data->parentItem->items();
		for (int i = 0; i < items.size(); ++i)
			if (items[i]->isSelected()) {
				d_data->initPos[items[i]] = QPair<qint64, qint64>(items[i]->left(), items[i]->right());
			}
	}
}

void VipTimeRangeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent*)
{
	d_data->selection = -2;
}

QVariant VipTimeRangeItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemSelectedHasChanged) {
		bool selected = false;
		QList<VipTimeRangeItem*> items = d_data->parentItem->items();
		for (int i = 0; i < items.size(); ++i)
			if (items[i]->isSelected()) {
				selected = true;
				break;
			}

		// d_data->parentItem->setVisible(items.size() > 1);
		d_data->parentItem->setSelected(selected);
		d_data->parentItem->prepareGeometryChange();

		Q_EMIT parentItem()->itemSelectionChanged(this);
	}

	return VipPlotItem::itemChange(change, value);
}

VipFunctionDispatcher<2>& vipCreateTimeRangeItemsDispatcher()
{
	static VipFunctionDispatcher<2> inst;
	return inst;
}

class VipTimeRangeListItem::PrivateData
{
public:
	PrivateData()
	  : device(nullptr)
	  , heights(ITEM_START_HEIGHT, ITEM_END_HEIGHT)
	  , selection(-2)
	  , state(Visible)
	  , drawComponents(VipTimeRangeListItem::All)
	  , area(nullptr)
	{
	}

	QList<VipTimeRangeItem*> items;
	QPointer<VipIODevice> device;
	QPair<double, double> heights;
	QColor color;

	QPainterPath resizeLeft;
	QPainterPath resizeRight;
	QPainterPath moveArea;

	QPointF pos;
	VipTimeRange initRange;
	QMap<VipTimeRangeItem*, QPair<qint64, qint64>> initItemRanges;
	int selection;
	int state;
	DrawComponents drawComponents;

	VipTimeRangeListItem::draw_function drawFunction;
	VipTimeRangeListItem::change_time_range_function changeTimeRangeFunction;
	VipTimeRangeListItem::closest_time_function closestTimeFunction;
	VipPlayerArea* area;
};

VipTimeRangeListItem::VipTimeRangeListItem(VipPlayerArea* area)
  : VipPlotItem(VipText())
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->area = area;
	this->setItemAttribute(AutoScale);
	this->setItemAttribute(HasLegendIcon, false);
	this->setItemAttribute(VisibleLegend, false);
	this->setRenderHints(QPainter::Antialiasing);

	connect(this, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(updateDevice()));
}

VipTimeRangeListItem::~VipTimeRangeListItem()
{
	while (d_data->items.size())
		delete d_data->items.first();
}

void VipTimeRangeListItem::setDrawComponents(DrawComponents c)
{
	d_data->drawComponents = c;
	update();
}
void VipTimeRangeListItem::setDrawComponent(DrawComponent c, bool on)
{
	if (d_data->drawComponents.testFlag(c) != on) {
		if (on) {
			d_data->drawComponents |= c;
		}
		else {
			d_data->drawComponents &= ~c;
		}
		update();
	}
}
bool VipTimeRangeListItem::testDrawComponent(DrawComponent c) const
{
	return d_data->drawComponents.testFlag(c);
}
VipTimeRangeListItem::DrawComponents VipTimeRangeListItem::drawComponents() const
{
	return d_data->drawComponents;
}

void VipTimeRangeListItem::setStates(int st)
{
	d_data->state = st;
}

int VipTimeRangeListItem::states() const
{
	return d_data->state;
}

QList<VipInterval> VipTimeRangeListItem::plotBoundingIntervals() const
{
	if (d_data->device && d_data->device->deviceType() == VipIODevice::Temporal) {
		return QList<VipInterval>() << VipInterval(d_data->device->firstTime(), d_data->device->lastTime()) << VipInterval(d_data->heights.first, d_data->heights.second);
	}
	return QList<VipInterval>();
}

void VipTimeRangeListItem::reset()
{
	VipIODevice* device = d_data->device;
	setDevice(nullptr);

	if (device)
		device->resetTimestampingFilter();

	setDevice(device);
}

void VipTimeRangeListItem::deviceTimestampingChanged()
{
	VipIODevice* dev = d_data->device;
	setDevice(nullptr);
	setDevice(dev);
}

void VipTimeRangeListItem::computeToolTip()
{
	if (VipIODevice* device = d_data->device) {
		QString name = device->name();
		if (device->outputCount() == 1)
			name = device->outputAt(0)->data().name();
		QString tool_tip = "<b>Name</b>: " + name;
		if (device->deviceType() == VipIODevice::Temporal) {
			// for temporal devices, set the duration and size as tool tip text
			if (device->size() != VipInvalidPosition)
				tool_tip += "<br><b>Size</b>: " + QString::number(device->size());
			if (device->firstTime() != VipInvalidTime && device->lastTime() != VipInvalidTime) {
				VipValueToTime::TimeType type = VipValueToTime::findBestTimeUnit(VipInterval(device->firstTime(), device->lastTime()));
				tool_tip += "<br><b>Duration</b>: " +
					    VipValueToTime::convert(device->lastTime() - device->firstTime(), VipValueToTime::TimeType(type % 2 ? type - 1 : type)); // remove SE from time type
				tool_tip += "<br><b>Start</b>: " + VipValueToTime::convert(device->firstTime(), type, "dd.MM.yyyy hh:mm:ss.zzz");
				tool_tip += "<br><b>End</b>: " + VipValueToTime::convert(device->lastTime(), type, "dd.MM.yyyy hh:mm:ss.zzz");

				QString date = device->attribute("Date").toString();
				if (!date.isEmpty())
					tool_tip += "<br><b>Date</b>: " + date;
			}
		}

		this->setToolTipText(tool_tip);
		for (int i = 0; i < d_data->items.size(); ++i) {
			d_data->items[i]->setToolTipText(tool_tip);
		}
	}
}

void VipTimeRangeListItem::setDevice(VipIODevice* device)
{
	if (d_data->device) {
		disconnect(d_data->device, SIGNAL(timestampingChanged()), this, SLOT(deviceTimestampingChanged()));

		// remove the current managed items
		while (d_data->items.size())
			delete d_data->items.first();
		d_data->device = nullptr;
	}

	if (device) {
		d_data->device = device;

		connect(device, SIGNAL(timestampingChanged()), this, SLOT(deviceTimestampingChanged()));

		const auto lst = vipCreateTimeRangeItemsDispatcher().match(device, this);
		if (lst.size())
			lst.last()(device, this);
		else {

			// create the managed items
			// take into account the possibly installed filter
			const VipTimestampingFilter& filter = d_data->device->timestampingFilter();

			if (!filter.isEmpty()) {
				// use the VipTimeRangeTransforms in case of filtering
				VipTimeRangeTransforms trs = filter.validTransforms();
				for (VipTimeRangeTransforms::const_iterator it = trs.begin(); it != trs.end(); ++it) {
					VipTimeRangeItem* item = new VipTimeRangeItem(this);
					item->blockSignals(true);
					item->setInitialTimeRange(it.key());
					item->setCurrentTimeRange(it.value());
					item->setColor(d_data->color);
					item->setReverse(it.value().first > it.value().second);
					item->setRenderHints(QPainter::Antialiasing);
					item->setFlag(ItemHasNoContents);
					item->blockSignals(false);
				}
			}
			else {
				// use the original time window in case of no filtering
				VipTimeRangeList _lst = d_data->device->timeWindow();
				for (int i = 0; i < _lst.size(); ++i) {
					VipTimeRangeItem* item = new VipTimeRangeItem(this);
					item->blockSignals(true);
					item->setInitialTimeRange(_lst[i]);
					item->setRenderHints(QPainter::Antialiasing);
					item->setColor(d_data->color);
					item->setFlag(ItemHasNoContents);
					item->blockSignals(false);
				}
			}
		}

		setHeights(heights().first, heights().second);

		computeToolTip();
	}

	emitItemChanged();
}

VipIODevice* VipTimeRangeListItem::device() const
{
	return d_data->device;
}

const QList<VipTimeRangeItem*> &VipTimeRangeListItem::items() const
{
	return d_data->items;
}

QList<qint64> VipTimeRangeListItem::stops() const
{
	QList<qint64> res;
	for (int i = 0; i < d_data->items.size(); ++i) {
		VipTimeRange range = d_data->items[i]->currentTimeRange();
		res << range.first << range.second;
	}
	return res;
}

VipTimeRangeTransforms VipTimeRangeListItem::transforms() const
{
	const QList<VipTimeRangeItem*> lst = items();
	VipTimeRangeTransforms res;

	bool has_transform = false;
	for (int i = 0; i < lst.size(); ++i) {
		res[lst[i]->initialTimeRange()] = lst[i]->currentTimeRange();
		if (lst[i]->initialTimeRange() != lst[i]->currentTimeRange())
			has_transform = true;
	}

	if (has_transform)
		return res;
	else
		return VipTimeRangeTransforms();
}

void VipTimeRangeListItem::setHeights(double start, double end)
{
	// if (d_data->heights != QPair<double, double>(start, end))
	{
		d_data->heights = QPair<double, double>(start, end);
		QList<VipTimeRangeItem*> lst = items();

		for (int i = 0; i < lst.size(); ++i)
			lst[i]->setHeights(start, end);

		update();
		// emitItemChanged();
	}
}

void VipTimeRangeListItem::setColor(const QColor& c)
{
	if (d_data->color != c) {
		d_data->color = c;
		QList<VipTimeRangeItem*> lst = items();

		for (int i = 0; i < lst.size(); ++i)
			lst[i]->setColor(c);

		emitItemChanged();
	}
}

QColor VipTimeRangeListItem::color() const
{
	return d_data->color;
}

void VipTimeRangeListItem::setAdditionalDrawFunction(draw_function fun)
{
	d_data->drawFunction = fun;
	update();
}
VipTimeRangeListItem::draw_function VipTimeRangeListItem::additionalDrawFunction() const
{
	return d_data->drawFunction;
}

void VipTimeRangeListItem::setChangeTimeRangeFunction(change_time_range_function fun)
{
	d_data->changeTimeRangeFunction = fun;
}
VipTimeRangeListItem::change_time_range_function VipTimeRangeListItem::changeTimeRangeFunction() const
{
	return d_data->changeTimeRangeFunction;
}

void VipTimeRangeListItem::setClosestTimeFunction(closest_time_function fun)
{
	d_data->closestTimeFunction = fun;
}
VipTimeRangeListItem::closest_time_function VipTimeRangeListItem::closestTimeFunction() const
{
	return d_data->closestTimeFunction;
}
qint64 VipTimeRangeListItem::closestTime(qint64 time) const
{
	if (d_data->closestTimeFunction)
		return d_data->closestTimeFunction(this, time);
	return time;
}

QPair<double, double> VipTimeRangeListItem::heights() const
{
	return d_data->heights;
}

QRectF VipTimeRangeListItem::itemsBoundingRect() const
{
	QRectF bounding;
	for (int i = 0; i < d_data->items.size(); ++i)
		bounding |= d_data->items[i]->boundingRect();
	return bounding;
}

QPair<qint64, qint64> VipTimeRangeListItem::itemsRange() const
{
	if (d_data->items.size()) {
		qint64 left = d_data->items.first()->left();
		qint64 right = d_data->items.first()->right();
		for (int i = 0; i < d_data->items.size(); ++i) {
			left = qMin(left, d_data->items[i]->left());
			right = qMax(right, d_data->items[i]->right());
		}
		return QPair<qint64, qint64>(left, right);
	}
	return QPair<qint64, qint64>(0, 0);
}

QRectF VipTimeRangeListItem::boundingRect() const
{
	return shape().boundingRect();
}

VipPlayerArea* VipTimeRangeListItem::area() const
{
	return d_data->area;
}

QPainterPath VipTimeRangeListItem::shape() const
{
	if (d_data->items.size() < 2)
		return QPainterPath();

	QRectF bounding = itemsBoundingRect().normalized().adjusted(-2, -2, 2, 2);

	int width = 5;
	QPainterPath moving;
	moving.addRect(QRectF(bounding.left() - width - 2, bounding.top(), width, bounding.height()));
	moving.addRect(QRectF(bounding.right() + 2, bounding.top(), width, bounding.height()));

	bounding.adjust(-width - 4, 0, width + 4, 0);

	// compute move area
	// QPainterPathStroker stroke;
	//  stroke.setWidth(4);
	//  QPainterPath moving;
	//  moving.addRect(bounding);
	d_data->moveArea = moving; // stroke.createStroke(moving);

	// compute arrows
	d_data->resizeLeft = QPainterPath();
	d_data->resizeRight = QPainterPath();

	QSizeF size(7, 9);
	QPolygonF leftArrow = QPolygonF() << QPointF(bounding.left() - size.width(), bounding.center().y()) << QPointF(bounding.left(), bounding.center().y() - size.height() / 2)
					  << QPointF(bounding.left(), bounding.center().y() + size.height() / 2) << QPointF(bounding.left() - size.width(), bounding.center().y());
	QPolygonF rightArrow = QPolygonF() << QPointF(bounding.right() + size.width(), bounding.center().y()) << QPointF(bounding.right(), bounding.center().y() - size.height() / 2)
					   << QPointF(bounding.right(), bounding.center().y() + size.height() / 2) << QPointF(bounding.right() + size.width(), bounding.center().y());
	d_data->resizeLeft.addPolygon(leftArrow);
	d_data->resizeRight.addPolygon(rightArrow);

	QPainterPath res = d_data->resizeLeft | d_data->resizeRight;
	// if(isSelected())
	res |= d_data->moveArea;
	return res;
}

void VipTimeRangeListItem::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	drawSelected(p, m);
}

void VipTimeRangeListItem::drawSelected(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	if (d_data->items.size()) {

		// TEST: draw items
		for (const VipTimeRangeItem* it : d_data->items) {
			it->draw(p, m);
		}

		// draw the device name
		if (zValue() <= d_data->items.first()->zValue())
			const_cast<VipTimeRangeListItem*>(this)->setZValue(d_data->items.first()->zValue() + 1);

		QRectF bounding = itemsBoundingRect();
		QPointF left = bounding.bottomLeft() + QPointF(20, -1); // m->transform(QPointF(d_data->items.first()->left(), this->heights().first));

		if (d_data->drawComponents & Text) {
			p->save();
			p->setRenderHints(QPainter::TextAntialiasing);
			// p->setCompositionMode(QPainter::CompositionMode_Difference);
			QColor c = d_data->items.first()->color();
			c.setRed(255 - c.red());
			c.setGreen(255 - c.green());
			c.setBlue(255 - c.blue());
			p->setPen(c);
			QFont f = p->font();
			f.setPointSizeF(bounding.height() / 1.5);
			p->setFont(f);
			p->drawText(left, device()->name());
			p->restore();
		}

		// set the items color if necessary
		QVariant v = device()->property("_vip_color");
		if (v.userType() == qMetaTypeId<QColor>()) {
			QColor c = v.value<QColor>();
			if (c != Qt::transparent) {
				const_cast<VipTimeRangeListItem*>(this)->setColor(c);
			}
		}
	}

	shape();

	if ((d_data->drawComponents & MovingArea) && d_data->items.size() > 1) {
		p->setRenderHint(QPainter::Antialiasing, false);

		// draw moving area
		p->setPen(QColor(0x4D91AE));
		p->setBrush(QBrush(QColor(0x22BDFF)));
		p->drawPath(d_data->moveArea);
	}
	if ((d_data->drawComponents & ResizeArea) && d_data->items.size() > 1) {
		p->setRenderHint(QPainter::Antialiasing, true);

		// draw arrows
		// p->setPen(QColor(0xEEE058).darker(180));
		// p->setBrush(QBrush(QColor(0xEEE058)));
		p->setPen(QColor(0x4D91AE));
		p->setBrush(QBrush(QColor(0x22BDFF)));
		p->drawPath(d_data->resizeLeft);
		p->drawPath(d_data->resizeRight);
	}

	if (d_data->drawFunction)
		d_data->drawFunction(this, p, m);
}

void VipTimeRangeListItem::split(VipTimeRangeItem*, qint64)
{
	// VipTimeRange range(qRound64(item->left()),qRound64(item->right()));
	//  if(time > range.first && time < range.second)
	//  {
	//  VipTimeRange r1(range.first,time)
	//  }
}

bool VipTimeRangeListItem::applyTransform(const QTransform& tr)
{
	for (int i = 0; i < d_data->items.size(); ++i)
		d_data->items[i]->applyTransform(tr);
	update();
	return true;
}

void VipTimeRangeListItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
	QPointF p = event->pos();
	if (d_data->resizeLeft.contains(p))
		this->setCursor(Qt::SizeHorCursor);
	else if (d_data->resizeRight.contains(p))
		this->setCursor(Qt::SizeHorCursor);
	else
		this->setCursor(Qt::SizeAllCursor);
}

void VipTimeRangeListItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	if (area()->timeRangesLocked()) {
		VipCorrectedTip::showText(QCursor::pos(), lockedMessage(), nullptr, QRect(), 5000);
		return;
	}

	QPair<double, double> range = d_data->initRange;
	QPair<double, double> previous = range;

	// compute the new time range
	VipCoordinateSystemPtr m = sceneMap();
	double diff = (m->invTransform(event->pos()) - m->invTransform(d_data->pos)).x();
	if (d_data->selection == -1) // left arrow
		range.first += diff;
	else if (d_data->selection == 1) // left arrow
		range.second += diff;
	else if (d_data->selection == 0) {
		range.first += diff;
		range.second += diff;
	}

	// d_data->pos = event->pos();

	// compute the transform between old and new range
	QTransform tr;
	double dx = (range.second - range.first) / (previous.second - previous.first);
	double translate = range.first - previous.first * dx;
	tr.translate(translate, translate);
	tr.scale(dx, dx);

	// apply it
	for (int i = 0; i < d_data->items.size(); ++i) {
		VipTimeRangeItem* it = d_data->items[i];

		VipTimeRange itrange = d_data->initItemRanges[it];

		QPointF times(itrange.first, itrange.second);
		times = tr.map(times);

		qint64 new_left = closestTime(qRound64(times.x()));
		qint64 new_right = closestTime(qRound64(times.y()));

		// block signals to avoid a call to updatDevice() FOR EACH items
		it->blockSignals(true);
		it->setCurrentTimeRange(new_left, new_right);
		it->blockSignals(false);
	}

	Q_EMIT itemsTimeRangeChanged(d_data->items);

	// update the device, this will also update the time scale widget
	this->updateDevice();
	this->computeToolTip();
}

void VipTimeRangeListItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	d_data->pos = event->pos();
	d_data->initRange = itemsRange();
	d_data->initItemRanges.clear();
	for (int i = 0; i < d_data->items.size(); ++i)
		d_data->initItemRanges[d_data->items[i]] = QPair<qint64, qint64>(d_data->items[i]->left(), d_data->items[i]->right());

	if (d_data->resizeLeft.contains(d_data->pos))
		d_data->selection = -1;
	else if (d_data->resizeRight.contains(d_data->pos))
		d_data->selection = 1;
	else
		d_data->selection = 0;
}

void VipTimeRangeListItem::mouseReleaseEvent(QGraphicsSceneMouseEvent*)
{
	d_data->selection = -2;
}

QVariant VipTimeRangeListItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemVisibleHasChanged) {
		for (int i = 0; i < d_data->items.size(); ++i)
			d_data->items[i]->setVisible(this->isVisible());
	}
	return VipPlotItem::itemChange(change, value);
}

void VipTimeRangeListItem::addItem(VipTimeRangeItem* item)
{
	d_data->items.append(item);
	item->setZValue(this->zValue());
}

void VipTimeRangeListItem::removeItem(VipTimeRangeItem* item)
{
	d_data->items.removeAll(item);
}

void VipTimeRangeListItem::updateDevice()
{
	updateDevice(false);
}

// update the VipIODevice timestamping filter
void VipTimeRangeListItem::updateDevice(bool reload)
{
	bool selected = false;
	for (int i = 0; i < d_data->items.size(); ++i)
		if (d_data->items[i]->isSelected()) {
			selected = true;
			break;
		}

	this->setSelected(selected);
	this->prepareGeometryChange();

	if (d_data->device) {
		VipTimeRangeTransforms trs = transforms();
		if (trs.size()) {
			VipTimestampingFilter filter;
			filter.setTransforms(trs);
			if (d_data->changeTimeRangeFunction)
				d_data->changeTimeRangeFunction(this, filter);
			else
				d_data->device->setTimestampingFilter(filter);
		}
		if (reload)
			d_data->device->reload();
	}
}

class TimeSliderGrip : public VipSliderGrip
{
public:
	TimeSliderGrip(VipProcessingPool* pool, VipAbstractScale* parent)
	  : VipSliderGrip(parent)
	  , d_pool(pool)
	{
		setValue(0);
		setDisplayToolTipValue(Qt::AlignHCenter | Qt::AlignBottom);
		setToolTipText("<b>Time</b>: #value");
		setToolTipDistance(11);
	}

	void setProcessingPool(VipProcessingPool* pool) { d_pool = pool; }

	VipProcessingPool* processingPool() { return d_pool; }

protected:
	virtual double closestValue(double v)
	{
		if (d_pool) {
			qint64 tmp = d_pool->closestTime(v);
			if (tmp != VipInvalidTime)
				return tmp;
			else
				return v;
		}
		else
			return v;
	}

	virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event)
	{
		if (qobject_cast<VipPlotMarker*>(watched->toGraphicsObject())) {
			if (event->type() == QEvent::GraphicsSceneMouseMove) {
				this->mouseMoveEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
				return true;
			}
			else if (event->type() == QEvent::GraphicsSceneMousePress) {
				this->mousePressEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
				return true;
			}
			else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
				this->mouseReleaseEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
				return true;
			}
		}

		return false;
	}

private:
	VipProcessingPool* d_pool;
};

class TimeSliderGripNoLimits : public TimeSliderGrip
{
public:
	TimeSliderGripNoLimits(VipProcessingPool* pool, VipAbstractScale* parent)
	  : TimeSliderGrip(pool, parent)
	{
	}

	virtual double closestValue(double v)
	{
		if (processingPool())
			return processingPool()->closestTimeNoLimits(v);
		else
			return v;
	}
};

class TimeScale : public VipAxisBase
{
public:
	VipBoxStyle lineBoxStyle;
	double lineWidth;
	double gripZValue;
	TimeSliderGrip* grip;
	QColor sliderColor;
	QColor sliderFillColor;

	TimeScale(VipProcessingPool* pool, QGraphicsItem* parent = nullptr)
	  : VipAxisBase(Bottom, parent)
	  , lineWidth(4)
	  , gripZValue(2000)
	  , grip(nullptr)
	{
		sliderColor = QColor(0xC9DEFA);
		sliderFillColor = QColor(0xFF5963);
		grip = new TimeSliderGrip(pool, this);
		grip->setImage(vipPixmap("handle.png").toImage().scaled(QSize(16, 16), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		grip->setHandleDistance(-2);
		grip->setZValue(2000);
		// VipAdaptativeGradient grad(QGradientStops() << QGradientStop(0, QColor(0x97B8E0)) << QGradientStop(1, QColor(0xC9DEFA)), Qt::Vertical);
		lineBoxStyle.setBorderRadius(1);
		// lineBoxStyle.setAdaptativeGradientBrush(grad);
		lineBoxStyle.setBorderPen(QPen(Qt::NoPen));
		lineBoxStyle.setRoundedCorners(Vip::AllCorners);
		scaleDraw()->setSpacing(6);
		setCanvasProximity(2);
		setExpandToCorners(true);
		this->setTitle(QString("Time"));
		this->enableDrawTitle(false);
	}

	QRectF sliderRect() const
	{
		QRectF cr = boundingRectNoCorners();
		double margin = this->margin();
		cr.setLeft(this->constScaleDraw()->pos().x());
		cr.setWidth(this->constScaleDraw()->length() + 1);
		cr.setTop(cr.top() + margin + this->spacing() //+ 3
		);
		cr.setHeight(lineWidth);
		return cr;
	}
	// NEWPLOT
	virtual void draw(QPainter* painter, QWidget* widget)
	{
		VipAxisBase::draw(painter, widget);

		double half_width = lineWidth / 2.0;
		QRectF rect = sliderRect();
		QRectF line_rect = QRectF(QPointF(rect.left(), rect.center().y() - half_width), QPointF(rect.right(), rect.center().y() + half_width));

		VipBoxStyle st = lineBoxStyle;
		st.setBackgroundBrush(sliderColor);
		st.computeRect(line_rect);
		st.draw(painter);

		// draw filled area
		line_rect.setRight(this->position(grip->value()).x());
		st.setBackgroundBrush(sliderFillColor);
		st.computeRect(line_rect);
		st.draw(painter);

		// draw selection time range
		if (VipPlayerArea* p = qobject_cast<VipPlayerArea*>(parentItem()->toGraphicsObject())) {
			VipTimeRange r = p->selectionTimeRange();
			if (r.first != r.second) {
				double f = position(r.first).x();
				double s = position(r.second).x();
				if (s < f)
					std::swap(s, f);
				line_rect.setLeft(f);
				line_rect.setRight(s);
				painter->setPen(Qt::NoPen);
				painter->setBrush(p->timeRangeSelectionBrush());
				painter->drawRect(line_rect);
			}
		}
	}

	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event)
	{
		if (grip) {
			double time = this->value(event->pos());
			grip->setValue(time);
		}
	}

	virtual void computeScaleDiv()
	{
		if (!isAutoScale())
			return;

		const QList<VipPlotItem*>& items = this->plotItems();
		if (items.isEmpty())
			return;

		// compute first boundaries

		int i = 0;
		VipInterval bounds;

		for (; i < items.size(); ++i) {
			if (VipTimeRangeListItem* it = qobject_cast<VipTimeRangeListItem*>(items[i]))
				if (!(it->states() & VipTimeRangeListItem::HiddenForPlayer)) {
					bounds = VipInterval(it->device()->firstTime(), it->device()->lastTime());
					if (bounds.isValid()) {
						++i;
						break;
					}
				}
		}

		// compute the union boundaries

		for (; i < items.size(); ++i) {
			if (VipTimeRangeListItem* it = qobject_cast<VipTimeRangeListItem*>(items[i]))
				if (!(it->states() & VipTimeRangeListItem::HiddenForPlayer)) {
					VipInterval tmp(it->device()->firstTime(), it->device()->lastTime());
					if (tmp.isValid())
						bounds = bounds.unite(tmp);
				}
		}
		vip_double stepSize = 0.;
		vip_double x1 = bounds.minValue();
		vip_double x2 = bounds.maxValue();
		scaleEngine()->autoScale(maxMajor(), x1, x2, stepSize);
		const VipInterval prev_bounds = this->scaleDiv().bounds();
		this->setScaleDiv(scaleEngine()->divideScale(x1, x2, maxMajor(), maxMinor(), stepSize));
		if (prev_bounds != this->scaleDiv().bounds()) {
			if (VipPlayerArea* a = qobject_cast<VipPlayerArea*>(this->area())) {
				// get the parent VipPlayWidget to update its geometry
				QWidget* p = a->view();
				VipPlayWidget* w = nullptr;
				while (p) {
					if ((w = qobject_cast<VipPlayWidget*>(p)))
						break;
					p = p->parentWidget();
				}
				if (w)
					w->updatePlayer();
			}
		}
	}
};

class VipPlayerArea::PrivateData
{
public:
	PrivateData()
	  : visible(true)
	  , timeRangesLocked(true)
	  , highlightedTime(VipInvalidTime)
	  , boundTime(VipInvalidTime)
	  , highlightedItem(nullptr)
	  , adjustFactor(0)
	  , dirtyProcessingPool(false)
	{
	}

	QList<VipTimeRangeListItem*> items;
	QPointer<VipProcessingPool> pool;
	bool visible;
	bool timeRangesLocked;
	TimeScale* timeScale;

	qint64 highlightedTime;
	qint64 boundTime;
	VipPlotItem* highlightedItem;

	VipPlotMarker* highlightedMarker;
	VipPlotMarker* timeMarker;

	VipPlotMarker* limit1Marker;
	VipPlotMarker* limit2Marker;

	TimeSliderGrip* timeSliderGrip;
	TimeSliderGrip* limit1Grip;
	TimeSliderGrip* limit2Grip;

	VipTimeRange selectionTimeRange;
	QBrush timeRangeSelectionBrush;

	// VipColorPalette palette;
	int adjustFactor;

	bool dirtyProcessingPool;
};

VipPlayerArea::VipPlayerArea()
  : VipPlotArea2D()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->timeRangeSelectionBrush = QBrush(QColor(0x8290FC));
	d_data->selectionTimeRange = VipInvalidTimeRange;
	// d_data->palette = VipColorPalette(VipLinearColorMap::ColorPaletteRandom);//VipColorPalette(VipLinearColorMap::ColorPaletteStandard);

	d_data->timeScale = new TimeScale(nullptr);
	this->addScale(d_data->timeScale, true);
	this->bottomAxis()->setVisible(false);
	d_data->timeScale->setZValue(2000);

	d_data->highlightedMarker = new VipPlotMarker();
	d_data->highlightedMarker->setLineStyle(VipPlotMarker::VLine);
	d_data->highlightedMarker->setLinePen(QPen(QBrush(Qt::red), 1, Qt::DashLine));
	d_data->highlightedMarker->setVisible(false);
	d_data->highlightedMarker->setAxes(d_data->timeScale, this->leftAxis(), VipCoordinateSystem::Cartesian);
	d_data->highlightedMarker->setZValue(1000);
	d_data->highlightedMarker->setFlag(VipPlotItem::ItemIsSelectable, false);
	d_data->highlightedMarker->setIgnoreStyleSheet(true);

	d_data->limit1Marker = new VipPlotMarker();
	d_data->limit1Marker->setLineStyle(VipPlotMarker::VLine);
	d_data->limit1Marker->setLinePen(QPen(QBrush(QColor(0xFF5963)), 1));
	d_data->limit1Marker->setAxes(d_data->timeScale, this->leftAxis(), VipCoordinateSystem::Cartesian);
	d_data->limit1Marker->setZValue(1000);
	d_data->limit1Marker->setFlag(VipPlotItem::ItemIsSelectable, false);
	d_data->limit1Marker->setIgnoreStyleSheet(true);

	d_data->limit2Marker = new VipPlotMarker();
	d_data->limit2Marker->setLineStyle(VipPlotMarker::VLine);
	d_data->limit2Marker->setLinePen(QPen(QBrush(QColor(0xFF5963)), 1));
	d_data->limit2Marker->setAxes(d_data->timeScale, this->leftAxis(), VipCoordinateSystem::Cartesian);
	d_data->limit2Marker->setZValue(1000);
	d_data->limit2Marker->setFlag(VipPlotItem::ItemIsSelectable, false);
	d_data->limit2Marker->setIgnoreStyleSheet(true);

	d_data->timeMarker = new VipPlotMarker();
	d_data->timeMarker->setLineStyle(VipPlotMarker::VLine);
	d_data->timeMarker->setLinePen(QPen(QBrush(QColor(0xFF5963)), 1));
	d_data->timeMarker->setAxes(d_data->timeScale, this->leftAxis(), VipCoordinateSystem::Cartesian);
	d_data->timeMarker->setZValue(1000);
	d_data->timeMarker->setFlag(VipPlotItem::ItemIsSelectable, false);
	d_data->timeMarker->setItemAttribute(VipPlotItem::AutoScale, false);
	d_data->timeMarker->setRenderHints(QPainter::RenderHints());
	d_data->timeMarker->setIgnoreStyleSheet(true);

	d_data->limit1Grip = new TimeSliderGripNoLimits(nullptr, d_data->timeScale);
	d_data->limit1Grip->setHandleDistance(-10);
	d_data->limit1Grip->setImage(vipPixmap("slider_limit.png").toImage().transformed(QTransform().rotate(90)));
	d_data->limit1Grip->setGripAlwaysInsideScale(false);

	d_data->limit2Grip = new TimeSliderGripNoLimits(nullptr, d_data->timeScale);
	d_data->limit2Grip->setHandleDistance(-10);
	d_data->limit2Grip->setImage(vipPixmap("slider_limit.png").toImage().transformed(QTransform().rotate(90)));
	d_data->limit2Grip->setGripAlwaysInsideScale(false);

	d_data->timeSliderGrip = d_data->timeScale->grip;
	d_data->timeSliderGrip->setZValue(2000); // just above time marker
	// d_data->timeSliderGrip->setHandleDistance(-10);
	d_data->timeSliderGrip->setGripAlwaysInsideScale(false);
	// tile grip always above limits grips
	// d_data->timeSliderGrip->setZValue(qMax(d_data->limit1Grip->zValue(),d_data->limit2Grip->zValue())+1);

	// d_data->timeScale->setMargin(15);
	// d_data->timeScale->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	// d_data->timeScale->scaleDraw()->setSpacing(20);

	// setMouseTracking(true);
	this->setMousePanning(Qt::RightButton);
	// this->setMouseItemSelection(Qt::LeftButton);
	this->setMouseWheelZoom(true);
	this->setZoomEnabled(this->leftAxis(), false);
	this->setAutoScale(true);
	this->setDrawSelectionOrder(nullptr);

	connect(this, SIGNAL(mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseMoved(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(this, SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseReleased(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->timeSliderGrip, SIGNAL(valueChanged(double)), this, SLOT(setTime(double)));
	connect(d_data->limit1Grip, SIGNAL(valueChanged(double)), this, SLOT(setLimit1(double)));
	connect(d_data->limit2Grip, SIGNAL(valueChanged(double)), this, SLOT(setLimit2(double)));

	this->grid()->setVisible(false); // setFlag(VipPlotItem::ItemIsSelectable,false);
	this->leftAxis()->setVisible(false);
	this->rightAxis()->setVisible(false);
	this->topAxis()->setVisible(false);
	this->titleAxis()->setVisible(false);

	this->leftAxis()->setUseBorderDistHintForLayout(true);
	this->rightAxis()->setUseBorderDistHintForLayout(true);
	this->topAxis()->setUseBorderDistHintForLayout(true);
	this->bottomAxis()->setUseBorderDistHintForLayout(true);

	VipToolTip* tip = new VipToolTip();
	tip->setRegionPositions(Vip::RegionPositions(Vip::XInside));
	tip->setAlignment(Qt::AlignTop | Qt::AlignRight);
	this->setPlotToolTip(tip);
}

VipPlayerArea::~VipPlayerArea()
{
}

bool VipPlayerArea::timeRangesLocked() const
{
	return d_data->timeRangesLocked;
}

void VipPlayerArea::setTimeRangesLocked(bool locked)
{
	d_data->timeRangesLocked = locked;
}

void VipPlayerArea::setTimeRangeVisible(bool visible)
{
	d_data->visible = visible;
	for (int i = 0; i < d_data->items.size(); ++i) {
		if (visible)
			d_data->items[i]->setStates(d_data->items[i]->states() & (~VipTimeRangeListItem::HiddenForHideTimeRanges));
		else
			d_data->items[i]->setStates(d_data->items[i]->states() | VipTimeRangeListItem::HiddenForHideTimeRanges);

		bool item_visible = d_data->items[i]->states() == VipTimeRangeListItem::Visible; // visible && d_data->items[i]->device()->isEnabled();
		d_data->items[i]->setVisible(item_visible);
	}

	d_data->timeMarker->setVisible(visible);
	if (visible && (d_data->pool->modes() & VipProcessingPool::UseTimeLimits)) {
		d_data->limit1Marker->setVisible(visible);
		d_data->limit2Marker->setVisible(visible);
	}
}

bool VipPlayerArea::timeRangeVisible() const
{
	return d_data->visible;
}

bool VipPlayerArea::limitsEnabled() const
{
	return d_data->pool->testMode(VipProcessingPool::UseTimeLimits);
}

int VipPlayerArea::visibleItemCount() const
{
	int c = 0;
	for (int i = 0; i < d_data->items.size(); ++i)
		if (!(d_data->items[i]->states() & VipTimeRangeListItem::HiddenForPlayer))
			++c;
	return c;
}

void VipPlayerArea::setSelectionTimeRange(const VipTimeRange& r)
{
	d_data->selectionTimeRange = r;
	update();
}
VipTimeRange VipPlayerArea::selectionTimeRange() const
{
	return d_data->selectionTimeRange;
}

void VipPlayerArea::setTimeRangeSelectionBrush(const QBrush& b)
{
	d_data->timeRangeSelectionBrush = b;
	update();
}
QBrush VipPlayerArea::timeRangeSelectionBrush() const
{
	return d_data->timeRangeSelectionBrush;
}

void VipPlayerArea::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	VipPlotArea2D::paint(painter, option, widget);

	// draw the selected time ranges
	// painter->setPen(Qt::NoPen);
	//  painter->setBrush(d_data->timeRangeSelectionBrush);
	//
	// VipPlotCanvas* c = this->canvas();
	//
	// qint64 f = d_data->selectionTimeRange.first;
	// qint64 s = d_data->selectionTimeRange.second;
	// if (s < f)
	// std::swap(f, s);
	// QRectF cb = c->boundingRect();
	// double _f = timeScale()->position(f).x();
	// double _s = timeScale()->position(s).x();
	// QRectF rect(QPointF(cb.top(), _f), QPointF(cb.bottom(), _s));
	// painter->drawRect(rect);
}

void VipPlayerArea::setLimit1(double t)
{
	d_data->limit1Marker->setRawData(QPointF(t, 0));
	d_data->limit1Grip->blockSignals(true);
	d_data->limit1Grip->setValue(t);
	d_data->limit1Grip->blockSignals(false);
	if (sender() != processingPool())
		d_data->pool->setStopBeginTime(d_data->pool->closestTimeNoLimits(t));
}

void VipPlayerArea::setLimit2(double t)
{
	d_data->limit2Marker->setRawData(QPointF(t, 0));
	d_data->limit2Grip->blockSignals(true);
	d_data->limit2Grip->setValue(t);
	d_data->limit2Grip->blockSignals(false);
	if (sender() != processingPool())
		d_data->pool->setStopEndTime(d_data->pool->closestTimeNoLimits(t));
}

void VipPlayerArea::setLimitsEnable(bool enable)
{
	d_data->limit1Marker->setItemAttribute(VipPlotItem::AutoScale, enable);
	d_data->limit1Marker->setVisible(enable && d_data->visible);
	d_data->limit1Grip->setVisible(enable);
	d_data->limit2Marker->setItemAttribute(VipPlotItem::AutoScale, enable);
	d_data->limit2Marker->setVisible(enable && d_data->visible);
	d_data->limit2Grip->setVisible(enable);
	d_data->pool->setMode(VipProcessingPool::UseTimeLimits, enable);
	if (enable) {
		qint64 first = d_data->pool->stopBeginTime();
		qint64 last = d_data->pool->stopEndTime();
		setLimit1(first);
		setLimit2(last);
	}
}

void VipPlayerArea::setTime64(qint64 t)
{
	setTime(t);
}

void VipPlayerArea::setTime(double t)
{
	// set the selection time range
	if (qApp->keyboardModifiers() & Qt::SHIFT) {
		if (d_data->selectionTimeRange == VipInvalidTimeRange) {
			d_data->selectionTimeRange = VipTimeRange(t, t);
		}
		else {
			d_data->selectionTimeRange.second = t;
		}
	}
	else {
		d_data->selectionTimeRange = VipTimeRange(t, t);
	}

	if (t < d_data->pool->firstTime())
		t = d_data->pool->firstTime();
	else if (t > d_data->pool->lastTime())
		t = d_data->pool->lastTime();

	d_data->timeMarker->setRawData(QPointF(t, 0));
	d_data->timeSliderGrip->blockSignals(true);
	d_data->timeSliderGrip->setValue(t);
	d_data->timeSliderGrip->blockSignals(false);
	if (sender() != processingPool()) {
		d_data->pool->read(t);
		VipProcessingObjectList objects = d_data->pool->leafs(false);
		objects.wait();
	}
}

VipTimeRangeListItem* VipPlayerArea::findItem(VipIODevice* device) const
{
	for (int i = 0; i < d_data->items.size(); ++i)
		if (d_data->items[i]->device() == device)
			return d_data->items[i];
	return nullptr;
}

void VipPlayerArea::updateAreaDevices()
{
	updateArea(false);
}

void VipPlayerArea::updateArea(bool check_item_visibility)
{
	if (!d_data->pool) {
		for (int i = 0; i < d_data->items.size(); ++i) {
			// d_data->items[i]->device()->resetTimestampingFilter();
			delete d_data->items[i];
		}
		d_data->items.clear();
	}
	else {
		VipProcessingObjectList lst = d_data->pool->processing("VipIODevice");

		int visibleItemCount = 0;

		QList<VipIODevice*> devices;
		for (int i = 0; i < lst.size(); ++i) {
			VipIODevice* device = qobject_cast<VipIODevice*>(lst[i]);

			if (device && (device->openMode() & VipIODevice::ReadOnly) && device->deviceType() == VipIODevice::Temporal &&
			    (device->size() != 1 || device->property("_vip_showTimeLine").toInt() == 1)) {
				devices.append(device);
				// retrieve all display object linked to this device
				QList<VipDisplayObject*> displays = vipListCast<VipDisplayObject*>(device->allSinks());
				// do not show this device time range if the display widgets are hidden (only for non new device)

				VipTimeRangeListItem* item = findItem(device);

				bool hidden = displays.size() > 0 && item && check_item_visibility;
				if (hidden)
					for (int d = 0; d < displays.size(); ++d) {
						if (displays[d]->isVisible()) {
							hidden = false;
							break;
						}
					}

				// if the display is hidden, remove it from time computations (time window, next, previous and closest time) from the processing pool
				if (check_item_visibility) {
					if (hidden) {
						// only disable temporal devics that contribute to the processing pool time limits
						device->setEnabled(false);
					}
					else {
						// enable the device, read the data at processing pool time if needed
						bool need_reload = !device->isEnabled();
						device->setEnabled(true);
						if (need_reload)
							device->read(d_data->pool->time());
					}
				}

				if (!item) {
					item = new VipTimeRangeListItem(this);
					item->setAxes(d_data->timeScale, this->leftAxis(), VipCoordinateSystem::Cartesian);
					item->setDevice(device);
					// item->setColor(d_data->palette.color(findBestColor(item)));
					d_data->items << item;
				}

				item->setHeights(visibleItemCount + ITEM_START_HEIGHT, visibleItemCount + ITEM_END_HEIGHT);

				if (device->isEnabled()) {
					++visibleItemCount;
					item->setStates(item->states() & (~VipTimeRangeListItem::HiddenForPlayer));
				}
				else
					item->setStates(item->states() | VipTimeRangeListItem::HiddenForPlayer);
			}
		}

		// remove items with invalid devices
		for (int i = 0; i < d_data->items.size(); ++i)
			if (devices.indexOf(d_data->items[i]->device()) < 0) {
				delete d_data->items[i];
				d_data->items.removeAt(i);
				--i;
			}

		// update time slider
		qint64 time = d_data->pool->time();
		if (time < d_data->pool->firstTime())
			time = d_data->pool->firstTime();
		else if (time > d_data->pool->lastTime())
			time = d_data->pool->lastTime();

		d_data->timeMarker->setRawData(QPointF(time, 0));
		d_data->timeSliderGrip->blockSignals(true);
		d_data->timeSliderGrip->setValue(time);
		d_data->timeSliderGrip->blockSignals(false);

		// update items visibility
		setTimeRangeVisible(d_data->visible);
	}

	this->computeStartDate();
	this->setAutoScale(true);
	Q_EMIT devicesChanged();
}

void VipPlayerArea::setProcessingPool(VipProcessingPool* pool)
{
	if (pool != d_data->pool) {
		if (d_data->pool) {
			disconnect(d_data->pool, SIGNAL(objectAdded(QObject*)), this, SLOT(updateAreaDevices()));
			disconnect(d_data->pool, SIGNAL(objectRemoved(QObject*)), this, SLOT(updateAreaDevices()));
			disconnect(d_data->pool, SIGNAL(timestampingChanged()), this, SLOT(addMissingDevices()));
			disconnect(d_data->pool, SIGNAL(timeChanged(qint64)), this, SLOT(setTime(qint64)));

			for (int i = 0; i < d_data->items.size(); ++i) {
				// d_data->items[i]->device()->resetTimestampingFilter();
				delete d_data->items[i];
			}
		}

		d_data->items.clear();
		d_data->pool = pool;

		if (pool) {
			d_data->timeSliderGrip->setProcessingPool(pool);
			d_data->limit1Grip->setProcessingPool(pool);
			d_data->limit2Grip->setProcessingPool(pool);

			connect(d_data->pool, SIGNAL(objectAdded(QObject*)), this, SLOT(updateAreaDevices()));	 //,Qt::QueuedConnection);
			connect(d_data->pool, SIGNAL(objectRemoved(QObject*)), this, SLOT(updateAreaDevices())); //,Qt::DirectConnection);
			connect(d_data->pool, SIGNAL(timeChanged(qint64)), this, SLOT(setTime64(qint64)), Qt::QueuedConnection);
			connect(d_data->pool, SIGNAL(timestampingChanged()), this, SLOT(addMissingDevices()), Qt::QueuedConnection);

			// VipProcessingObjectList lst = pool->processing("VipIODevice");
			//
			// //update time slider
			// qint64 time = d_data->pool->time();
			// if (time < d_data->pool->firstTime())
			// time = d_data->pool->firstTime();
			// else if (time > d_data->pool->lastTime())
			// time = d_data->pool->lastTime();
			//
			// d_data->timeMarker->setRawData(QPointF(time, 0));
			// d_data->timeSliderGrip->blockSignals(true);
			// d_data->timeSliderGrip->setValue(time);
			// d_data->timeSliderGrip->blockSignals(false);
			//
			// int count = 0;
			//
			//
			// for(int i=0; i < lst.size(); ++i)
			// {
			// VipIODevice * device = qobject_cast<VipIODevice*>(lst[i]);
			//
			// if( (device->openMode() & VipIODevice::ReadOnly) && device->deviceType() == VipIODevice::Temporal && device->size() != 1)
			// {
			// //retrieve all display object linked to this device
			// QList<VipDisplayObject*> displays = vipListCast<VipDisplayObject*>(device->allSinks());
			// //do not show this device time range if the display widgets are hidden
			// bool hidden = displays.size() > 0;
			// for(int d=0; d < displays.size(); ++d)
			//	if(displays[d]->isVisible())
			//	{
			//		hidden = false;
			//		break;
			//	}
			//
			//
			// //if the display is hidden, remove it from time computations (time window, next, previous and closest time) from the processing pool
			// if (hidden)
			// {
			//	//only disable temporal devics that contribute to the processing pool time limits
			//	if(device->deviceType() == VipIODevice::Temporal)
			//		device->setEnabled(false);
			//	continue;
			// }
			// else
			// {
			//	//enable the device, read the data at processing pool time if needed
			//	bool need_reload = !device->isEnabled();
			//	device->setEnabled( true);
			//	if (need_reload)
			//		device->read(pool->time());
			// }
			//
			// VipTimeRangeListItem * item = new VipTimeRangeListItem();
			// item->setAxes(d_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
			// item->setDevice(device);
			// item->setHeights(count + ITEM_START_HEIGHT, count+ITEM_END_HEIGHT);
			// item->setColor(d_data->palette.color(d_data->items.size()));
			// ++count;
			//
			// d_data->items << item;
			//
			// }
			// }
			//
			// //update items visibility
			// setTimeRangeVisible(d_data->visible);
		}

		updateArea(true);

		Q_EMIT processingPoolChanged(d_data->pool);
	}
}

VipProcessingPool* VipPlayerArea::processingPool() const
{
	return d_data->pool;
}

VipSliderGrip* VipPlayerArea::timeSliderGrip() const
{
	return const_cast<TimeSliderGrip*>(d_data->timeSliderGrip);
}

VipPlotMarker* VipPlayerArea::timeMarker() const
{
	return const_cast<VipPlotMarker*>(d_data->timeMarker);
}

VipSliderGrip* VipPlayerArea::limit1SliderGrip() const
{
	return const_cast<TimeSliderGrip*>(d_data->limit1Grip);
}

VipPlotMarker* VipPlayerArea::limit1Marker() const
{
	return const_cast<VipPlotMarker*>(d_data->limit1Marker);
}

VipSliderGrip* VipPlayerArea::limit2SliderGrip() const
{
	return const_cast<TimeSliderGrip*>(d_data->limit2Grip);
}

VipPlotMarker* VipPlayerArea::limit2Marker() const
{
	return const_cast<VipPlotMarker*>(d_data->limit2Marker);
}

VipAxisBase* VipPlayerArea::timeScale() const
{
	return const_cast<TimeScale*>(d_data->timeScale);
}

QList<qint64> VipPlayerArea::stops(const QList<VipTimeRangeItem*>& excluded) const
{
	QList<qint64> res;

	for (int i = 0; i < d_data->items.size(); ++i) {
		const QList<VipTimeRangeItem*> items = d_data->items[i]->items();
		for (int j = 0; j < items.size(); ++j) {
			if (excluded.indexOf(items[j]) < 0) {
				VipTimeRange range = items[j]->currentTimeRange();
				res << range.first;
				res << range.second;
			}
		}
	}

	return res;
}

QList<VipTimeRangeListItem*> VipPlayerArea::timeRangeListItems() const
{
	QList<VipPlotItem*> items = this->plotItems();
	QList<VipTimeRangeListItem*> res;
	for (int i = 0; i < items.size(); ++i)
		if (VipTimeRangeListItem* item = qobject_cast<VipTimeRangeListItem*>(items[i]))
			res << item;
	return res;
}

void VipPlayerArea::timeRangeItems(QList<VipTimeRangeItem*>& selected, QList<VipTimeRangeItem*>& not_selected) const
{
	QList<VipPlotItem*> items = this->plotItems();
	for (int i = 0; i < items.size(); ++i) {
		if (VipTimeRangeItem* item = qobject_cast<VipTimeRangeItem*>(items[i])) {
			if (item->isSelected())
				selected << item;
			else
				not_selected << item;
		}
	}
}

void VipPlayerArea::timeRangeListItems(QList<VipTimeRangeListItem*>& selected, QList<VipTimeRangeListItem*>& not_selected) const
{
	QList<VipPlotItem*> items = this->plotItems();
	for (int i = 0; i < items.size(); ++i) {
		if (VipTimeRangeListItem* item = qobject_cast<VipTimeRangeListItem*>(items[i])) {
			if (item->isSelected())
				selected << item;
			else
				not_selected << item;
		}
	}
}

void VipPlayerArea::addMissingDevices()
{
	// first, compute the list of present devices in the player area
	QSet<VipIODevice*> devices;
	for (int i = 0; i < d_data->items.size(); ++i)
		devices.insert(d_data->items[i]->device());

	// now, for each devices in the processing pool, add the temporal devices with a size != 1 that are not already present in the player area.
	// this case might happen when a temporal device changes its timestamping and size.

	if (this->processingPool()) {
		QList<VipIODevice*> pool_devices = this->processingPool()->findChildren<VipIODevice*>();
		for (int i = 0; i < pool_devices.size(); ++i) {
			VipIODevice* dev = pool_devices[i];
			if (!devices.contains(dev) && dev->isOpen() && dev->supportedModes() == VipIODevice::ReadOnly && dev->deviceType() == VipIODevice::Temporal &&
			    (dev->size() != 1 || dev->property("_vip_showTimeLine").toInt() == 1)) {
				// deviceAdded(dev);
				updateArea(true);
			}
		}
	}
}
// void VipPlayerArea::deviceAdded(QObject* obj)
// {
// VipIODevice * device = qobject_cast<VipIODevice*>(obj);
// if(device && device->supportedModes() == VipIODevice::ReadOnly && device->deviceType() == VipIODevice::Temporal && device->size() != 1)
// {
// //chekc that the device is not already added
// for (int i = 0; i < d_data->items.size(); ++i)
//	if (d_data->items[i]->device() == device)
//		return;
//
// int count = d_data->items.size();
// VipTimeRangeListItem * item = new VipTimeRangeListItem();
// item->setAxes(d_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
// item->setDevice(device);
// item->setHeights(count + ITEM_START_HEIGHT, count+ITEM_END_HEIGHT);
// item->setColor(d_data->palette.color(d_data->items.size()));
// d_data->items << item;
//
// //update items visibility
// setTimeRangeVisible(d_data->visible);
//
// this->setAutoScale(true);
// Q_EMIT devicesChanged();
// }
// }
//
// void VipPlayerArea::deviceRemoved(QObject* obj)
// {
// //remove all items with a nullptr device pointer or equal to obj
// for(int i=0; i < d_data->items.size(); ++i)
// {
// VipIODevice * device = d_data->items[i]->device();
// if(! device ||  device == obj)
// {
//	delete d_data->items[i];// ->deleteLater();
//	d_data->items.removeAt(i);
//	--i;
// }
// }
//
// //update items visibility
// setTimeRangeVisible(d_data->visible);
// Q_EMIT devicesChanged();
// }

void VipPlayerArea::moveToForeground()
{
	QList<VipTimeRangeItem*> selected, not_selected;
	timeRangeItems(selected, not_selected);

	// set the selected to 1000, set to 999 the others> 1000
	for (int i = 0; i < selected.size(); ++i)
		selected[i]->setZValue(1000);

	for (int i = 0; i < not_selected.size(); ++i)
		if (not_selected[i]->zValue() >= 1000)
			not_selected[i]->setZValue(999);
}

void VipPlayerArea::moveToBackground()
{
	QList<VipTimeRangeItem*> selected, not_selected;
	timeRangeItems(selected, not_selected);

	// set the selected to 100, set to 101 the others < 100
	for (int i = 0; i < selected.size(); ++i)
		selected[i]->setZValue(100);

	for (int i = 0; i < not_selected.size(); ++i)
		if (not_selected[i]->zValue() <= 100)
			not_selected[i]->setZValue(101);
}

void VipPlayerArea::splitSelection()
{
	// not implemented yet
}

void VipPlayerArea::reverseSelection()
{
	QList<VipTimeRangeItem*> selected, not_selected;
	timeRangeItems(selected, not_selected);
	for (int i = 0; i < selected.size(); ++i)
		selected[i]->setReverse(!selected[i]->reverse());
}

void VipPlayerArea::resetSelection()
{
	QList<VipTimeRangeListItem*> selected, not_selected;
	timeRangeListItems(selected, not_selected);
	for (int i = 0; i < selected.size(); ++i)
		selected[i]->reset();
}

void VipPlayerArea::resetAllTimeRanges()
{
	QList<VipTimeRangeListItem*> selected, not_selected;
	timeRangeListItems(selected, not_selected);
	for (int i = 0; i < selected.size(); ++i)
		selected[i]->reset();
	for (int i = 0; i < not_selected.size(); ++i)
		not_selected[i]->reset();
}

void VipPlayerArea::alignToZero(bool enable)
{
	QList<VipTimeRangeListItem*> all;
	timeRangeListItems(all, all);

	if (!enable) {
		// reset filter for all
		for (int i = 0; i < all.size(); ++i) {
			if (VipIODevice* device = all[i]->device()) {
				const VipTimestampingFilter prev = device->property("_vip_timestampingFilter").value<VipTimestampingFilter>();
				if (prev.isEmpty())
					device->resetTimestampingFilter();
				else {

					device->setTimestampingFilter(prev);
				}
				all[i]->setDevice(device);
			}
		}
		processingPool()->reload();
		return;
	}

	for (int i = 0; i < all.size(); ++i) {
		VipIODevice* device = all[i]->device();
		if (device) {
			qint64 first_time = VipInvalidTime; // find lowest time

			if (device->timestampingFilter().isEmpty()) {
				first_time = device->firstTime();
				// create the timestamping filter
				VipTimeRangeTransforms trs;
				VipTimeRangeList window = device->timeWindow();
				for (const VipTimeRange& win : window)
					trs[win] = VipTimeRange(win.first - first_time, win.second - first_time);
				VipTimestampingFilter filter;
				filter.setTransforms(trs);
				device->setTimestampingFilter(filter);
				device->setProperty("_vip_timestampingFilter", QVariant::fromValue(VipTimestampingFilter()));
			}
			else {
				// use the already existing transforms and translate them
				first_time = vipBounds(device->timestampingFilter().outputTimeRangeList()).first;
				if (first_time != 0) {
					VipTimestampingFilter filter = device->timestampingFilter();
					const VipTimestampingFilter saved = filter;

					VipTimeRangeTransforms trs = filter.transforms();
					for (VipTimeRangeTransforms::iterator it = trs.begin(); it != trs.end(); ++it) {
						it.value().first -= first_time;
						it.value().second -= first_time;
					}

					filter.setTransforms(trs);
					device->setTimestampingFilter(filter);
					device->setProperty("_vip_timestampingFilter", QVariant::fromValue(saved));
				}
			}

			all[i]->setDevice(device);
		}
	}
	processingPool()->reload();
}

void VipPlayerArea::computeStartDate()
{
	// For date time x axis (since epoch), compute the start date of the union of all plot items.
	// Then, set this date to the bottom and top VipValueToTime.
	// This will prevent the start date (displayed at the bottom left corner) to change
	// when zooming or mouse panning.

	VipValueToText* v = this->timeScale()->scaleDraw()->valueToText();
	VipValueToTime* vb = static_cast<VipValueToTime*>(this->timeScale()->scaleDraw()->valueToText());

	if (v->valueToTextType() == VipValueToText::ValueToTime && vb->type % 2 && vb->displayType != VipValueToTime::AbsoluteDateTime) {
		vb->fixedStartValue = true;
		vb->startValue = this->timeScale()->itemsInterval().minValue();
	}
	else {
		if (v->valueToTextType() == VipValueToText::ValueToTime) {
			vb->fixedStartValue = false;
			vb->startValue = this->timeScale()->scaleDiv().bounds().minValue();
		}
	}
}

void VipPlayerArea::updateProcessingPool()
{
	d_data->dirtyProcessingPool = false;
	updateArea(true);
}

void VipPlayerArea::defferedUpdateProcessingPool()
{
	if (!d_data->dirtyProcessingPool) {
		d_data->dirtyProcessingPool = true;
		QMetaObject::invokeMethod(this, "updateProcessingPool", Qt::QueuedConnection);
	}
}

void VipPlayerArea::mouseReleased(VipPlotItem* item, VipPlotItem::MouseButton button)
{
	if (item) {
		d_data->selectionTimeRange = VipInvalidTimeRange;
	}

	if (button == VipPlotItem::LeftButton) {
		if (d_data->highlightedTime != VipInvalidTime) {
			// while moving an item, one of the item boundaries highlighted a time.
			// now we need to translate the item to fit the highlighted time.

			// compute the translation
			QTransform tr;
			tr.translate(double(d_data->highlightedTime - d_data->boundTime), 0);

			// apply the transform to all VipTimeRangeItem
			if (VipTimeRangeListItem* t_litem = qobject_cast<VipTimeRangeListItem*>(d_data->highlightedItem)) {
				QList<VipTimeRangeItem*> items = t_litem->items();
				for (int i = 0; i < items.size(); ++i)
					items[i]->applyTransform(tr);
			}
			else if (VipTimeRangeItem* t_item = qobject_cast<VipTimeRangeItem*>(d_data->highlightedItem)) {
				t_item->applyTransform(tr);
			}
		}

		// invalidate highlightedTime
		d_data->highlightedTime = VipInvalidTime;
		d_data->boundTime = VipInvalidTime;
		d_data->highlightedItem = nullptr;
		d_data->highlightedMarker->setVisible(false);
	}
	else if (button == VipPlotItem::RightButton) {
		// display a contextual menu
		QMenu menu;
		connect(menu.addAction(vipIcon("foreground.png"), "Move selection to foreground"), SIGNAL(triggered(bool)), this, SLOT(moveToForeground()));
		connect(menu.addAction(vipIcon("background.png"), "Move selection to background"), SIGNAL(triggered(bool)), this, SLOT(moveToBackground()));
		menu.addSeparator();
		// connect(menu.addAction("Split time range on current time"),SIGNAL(triggered(bool)),this,SLOT(splitSelection()));
		connect(menu.addAction("Reverse time range"), SIGNAL(triggered(bool)), this, SLOT(reverseSelection()));
		menu.addSeparator();
		connect(menu.addAction("Reset selected time range"), SIGNAL(triggered(bool)), this, SLOT(resetSelection()));

		menu.exec(QCursor::pos());
	}
}

void VipPlayerArea::mouseMoved(VipPlotItem* item, VipPlotItem::MouseButton)
{

	// all the possible time stops
	QList<qint64> all_stops;
	// the stops of the current item we are moving
	QList<qint64> item_stops;
	// invalidate highlightedTime
	d_data->highlightedTime = VipInvalidTime;

	// if the whole VipTimeRangeListItem is moved
	if (VipTimeRangeListItem* t_litem = qobject_cast<VipTimeRangeListItem*>(item)) {
		all_stops = stops(t_litem->items());
		item_stops = t_litem->stops();

		d_data->highlightedItem = t_litem;
	}
	// if only a VipTimeRangeItem is moved
	else if (VipTimeRangeItem* t_item = qobject_cast<VipTimeRangeItem*>(item)) {
		VipTimeRange range = t_item->currentTimeRange();
		item_stops << range.first << range.second;
		all_stops = stops(QList<VipTimeRangeItem*>() << t_item);

		d_data->highlightedItem = t_item;
	}

	// now, find the closest possible stop with a vipDistance under 5px to the item stops
	qint64 closest = VipInvalidTime;
	qint64 closest_item = VipInvalidTime;
	qint64 dist = VipMaxTime;

	// closest stop
	for (int i = 0; i < item_stops.size(); ++i) {
		qint64 item_stop = item_stops[i];
		for (int j = 0; j < all_stops.size(); ++j) {
			qint64 tmp_dist = qAbs(item_stop - all_stops[j]);
			if (tmp_dist < dist) {
				dist = tmp_dist;
				closest = all_stops[j];
				closest_item = item_stop;
			}
		}
	}

	// check the vipDistance in pixels
	if (closest != VipInvalidTime) {
		double dist_pixel = qAbs(d_data->timeScale->position(closest).x() - d_data->timeScale->position(closest_item).x());
		if (dist_pixel < 5) {
			d_data->highlightedTime = closest;
			d_data->boundTime = closest_item;
			d_data->highlightedMarker->setVisible(true);
			d_data->highlightedMarker->setRawData(QPointF(closest, 0));
			return;
		}
	}

	d_data->highlightedMarker->setVisible(false);
}

/*int VipPlayerArea::findBestColor(VipTimeRangeListItem* item)
{
	if (d_data->items.indexOf(item) >= 0)
		return 0;

	QMap<int, int> colorIndexes;
	for (int i = 0; i < d_data->items.size(); ++i)
	{
		int index = d_data->items[i]->property("color_index").toInt();
		colorIndexes[index] = index;
	}

	int i = 0;
	for (QMap<int, int>::iterator it = colorIndexes.begin(); it != colorIndexes.end(); ++it,++i)
	{
		if (it.key() != i)
		{
			item->setProperty("color_index", i);
			return i;
		}
	}
	item->setProperty("color_index", d_data->items.size());
	return d_data->items.size();
}*/

static qint64 year2000 = QDateTime::fromString("2000", "yyyy").toMSecsSinceEpoch() * 1000000;

static VipValueToTime::TimeType findBestTimeUnit(VipPlayWidget* w)
{
	VipTimeRange r = w->processingPool() ? w->processingPool()->timeLimits() : VipTimeRange(0, 0);
	// VipValueToTime::TimeType current = w->timeType();

	if (r.first > year2000) {
		// if the start time is above nano seconds since year 2000, we can safely consider that this is a date
		// only modify the time type if we are not in (unit) since epoch
		// if (current % 2 == 0)
		{
			// get the scale time range and deduce the unit from it
			double range = r.second - r.first; // div.range();

			VipValueToTime::TimeType time = VipValueToTime::NanoSecondsSE;

			if (range > 1000000000) // above 1s
				time = VipValueToTime::SecondsSE;
			else if (range > 1000000) // above 1ms
				time = VipValueToTime::MilliSecondsSE;
			else if (range > 1000) // above 1micros
				time = VipValueToTime::MicroSecondsSE;

			return time;
		}
	}
	else {
		// if (current % 2 == 1)
		{
			// get the scale time range and deduce the unit from it
			double range = r.second - r.first; // div.range();
			VipValueToTime::TimeType time = VipValueToTime::NanoSeconds;

			if (range > 1000000000) // above 1s
				time = VipValueToTime::Seconds;
			else if (range > 1000000) // above 1ms
				time = VipValueToTime::MilliSeconds;
			else if (range > 1000) // above 1micros
				time = VipValueToTime::MicroSeconds;

			return time;
		}
	}
	VIP_UNREACHABLE();
	// return current;
}

class VipPlayWidget::PrivateData
{
public:
	// playing tool bar
	QToolBar* playToolBar;
	QAction* first;
	QAction* previous;
	QAction* playForward;
	QAction* playBackward;
	QAction* next;
	QAction* last;
	QAction* marks;
	QAction* repeat;
	QAction* maxSpeed;
	QAction* speed;
	QAction* lock;
	VipDoubleSliderWidget* speedWidget;

	// time/number
	QLabel* time;
	QLineEdit* timeEdit;
	QSpinBox* frameEdit;
	VipValueToTimeButton* timeUnit;

	// modify timestamping
	QAction* autoScale;
	QAction* select;
	QAction* visible;
	QAction* reset;
	QAction* startAtZero;
	QToolButton* selectionMode;

	QAction* hidden;
	QAction* axes;
	QAction* title;
	QAction* legend;
	QAction* position;
	QAction* properties;

	// sequential tool bar
	QToolBar* sequential;

	// timestamping area
	VipPlayerArea* playerArea;
	VipPlotWidget2D* playerWidget;

	int height;
	bool dirtyTimeLines;
	bool playWidgetHidden;
	bool alignedToZero;
};

static QList<VipPlayWidget*> playWidgets;
static VipToolTip::DisplayFlags playWidgetsFlags;

VipPlayWidget::VipPlayWidget(QWidget* parent)
  : QFrame(parent)
{

	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->dirtyTimeLines = false;
	d_data->playWidgetHidden = false;
	d_data->alignedToZero = false;
	d_data->height = 0;

	// playing tool bar
	d_data->speedWidget = new VipDoubleSliderWidget(VipBorderItem::Bottom);
	d_data->speedWidget->slider()->setScaleEngine(new VipLog10ScaleEngine());
	d_data->speedWidget->slider()->grip()->setImage(vipPixmap("slider_handle.png").toImage());
	d_data->speedWidget->setRange(0.1, 100);
	d_data->speedWidget->setValue(1);
	d_data->speedWidget->setMaximumHeight(40);
	d_data->speedWidget->setMaximumWidth(200);
	d_data->speedWidget->slider()->grip()->setTransform(QTransform().translate(0, 5)); //.rotate(180));
	static_cast<VipAxisBase*>(d_data->speedWidget->scale())->setUseBorderDistHintForLayout(true);
	d_data->speedWidget->setStyleSheet("background: transparent;");

	d_data->playToolBar = new QToolBar();
	d_data->playToolBar->setStyleSheet("QToolBar{background-color:transparent;}");
	d_data->playToolBar->setIconSize(QSize(20, 20));
	d_data->first = d_data->playToolBar->addAction(vipIcon("first.png"), "First time");
	d_data->previous = d_data->playToolBar->addAction(vipIcon("previous.png"), "Previous time");
	d_data->playBackward = d_data->playToolBar->addAction(vipIcon("playbackward.png"), "Play backward");
	d_data->playForward = d_data->playToolBar->addAction(vipIcon("play.png"), "Play forward");
	d_data->next = d_data->playToolBar->addAction(vipIcon("next.png"), "Next time");
	d_data->last = d_data->playToolBar->addAction(vipIcon("last.png"), "Last time");
	d_data->marks = d_data->playToolBar->addAction(vipIcon("slider_limit.png"), "Enable/disable time marks");
	d_data->marks->setCheckable(true);
	d_data->repeat = d_data->playToolBar->addAction(vipIcon("repeat.png"), "Repeat");
	d_data->repeat->setCheckable(true);
	d_data->maxSpeed = d_data->playToolBar->addAction(vipIcon("speed.png"),
							  "<b>Use maximum speed</b><br>"
							  "If using a play speed, the speed will be capped in order to not miss frames");
	d_data->maxSpeed->setCheckable(true);
	d_data->maxSpeed->setChecked(true);
	d_data->speed = d_data->playToolBar->addWidget(d_data->speedWidget);
	d_data->speed->setVisible(false);
	d_data->playToolBar->addSeparator();
	d_data->autoScale = d_data->playToolBar->addAction(vipIcon("axises.png"), "Auto scale");
	d_data->autoScale->setCheckable(true);
	d_data->autoScale->setChecked(true);
	d_data->selectionMode = new QToolButton(this);
	d_data->select = d_data->playToolBar->addWidget(d_data->selectionMode);
	d_data->select->setCheckable(true);
	d_data->select->setVisible(false); // hide it for now
	d_data->visible = d_data->playToolBar->addAction(vipIcon("time_lines.png"), "Show/hide the time lines");
	d_data->visible->setCheckable(true);
	d_data->visible->setChecked(true);
	d_data->playToolBar->addSeparator();
	d_data->startAtZero = d_data->playToolBar->addAction(vipIcon("align_left.png"), "Align time ranges to zero");
	d_data->startAtZero->setCheckable(true);
	d_data->reset = d_data->playToolBar->addAction(vipIcon("reset.png"), "Reset all time ranges");
	d_data->lock = d_data->playToolBar->addAction(vipIcon("lock.png"), "Lock/Unlock time ranges");
	d_data->lock->setCheckable(true);

	d_data->selectionMode->setToolTip(QString("Selection mode"));
	d_data->selectionMode->setIcon(vipIcon("select_item.png"));
	d_data->selectionMode->setAutoRaise(true);
	QMenu* menu = new QMenu();
	QAction* select = menu->addAction(vipIcon("select_item.png"), "Select items");
	QAction* selectArea = menu->addAction(vipIcon("select_area.png"), "Select items in area");
	QAction* zoomArea = menu->addAction(vipIcon("zoom_area.png"), "Zoom on area");
	d_data->selectionMode->setMenu(menu);
	d_data->selectionMode->setPopupMode(QToolButton::InstantPopup);
	menu->addSeparator();
	d_data->hidden = menu->addAction("Tool tip hidden");
	d_data->hidden->setData((int)VipToolTip::Hidden);
	d_data->hidden->setCheckable(true);
	d_data->axes = menu->addAction("Tool tip: show axis values");
	d_data->axes->setData((int)VipToolTip::Axes);
	d_data->axes->setCheckable(true);
	d_data->title = menu->addAction("Tool tip: show item titles");
	d_data->title->setData((int)VipToolTip::ItemsTitles);
	d_data->title->setCheckable(true);
	d_data->legend = menu->addAction("Tool tip: show item legends");
	d_data->legend->setData((int)VipToolTip::ItemsLegends);
	d_data->legend->setCheckable(true);
	d_data->position = menu->addAction("Tool tip: show item positions");
	d_data->position->setData((int)VipToolTip::ItemsPos);
	d_data->position->setCheckable(true);
	d_data->properties = menu->addAction("Tool tip: show item properties");
	d_data->properties->setData((int)VipToolTip::ItemsProperties);
	d_data->properties->setCheckable(true);
	connect(select, SIGNAL(triggered(bool)), this, SLOT(selectionItem()), Qt::QueuedConnection);
	connect(selectArea, SIGNAL(triggered(bool)), this, SLOT(selectionItemArea()), Qt::QueuedConnection);
	connect(zoomArea, SIGNAL(triggered(bool)), this, SLOT(selectionZoomArea()), Qt::QueuedConnection);
	connect(d_data->hidden, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(d_data->axes, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(d_data->title, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(d_data->legend, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(d_data->position, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(d_data->properties, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(d_data->lock, SIGNAL(triggered(bool)), this, SLOT(setTimeRangesLocked(bool)));

	// time/number
	d_data->time = new QLabel("<b>Time</b>");
	d_data->time->setMaximumWidth(70);
	d_data->timeEdit = new VipDoubleEdit();
	d_data->timeEdit->setMaximumWidth(100);
	d_data->timeEdit->setFrame(false);
	d_data->frameEdit = new QSpinBox();
	d_data->frameEdit->setMaximumWidth(100);
	d_data->frameEdit->setFrame(false);
	d_data->frameEdit->setToolTip("Frame position");
	d_data->timeUnit = new VipValueToTimeButton();

	QHBoxLayout* timeLay = new QHBoxLayout();
	timeLay->addWidget(d_data->time);
	timeLay->addWidget(d_data->timeEdit);
	timeLay->addWidget(d_data->timeUnit);
	timeLay->addWidget(d_data->frameEdit);
	timeLay->setContentsMargins(0, 0, 0, 0);
	d_data->timeEdit->setMaximumWidth(100);
	d_data->frameEdit->setMaximumWidth(100);

	// timestamping area
	d_data->playerArea = new VipPlayerArea();
	d_data->playerWidget = new VipPlotWidget2D();

	d_data->playerWidget->setArea(d_data->playerArea);
	d_data->playerArea->setMargins(VipMargins(10, 0, 10, 0));
	d_data->playerArea->timeMarker()->installSceneEventFilter(d_data->playerArea->timeSliderGrip());
	d_data->playerArea->limit1Marker()->installSceneEventFilter(d_data->playerArea->limit1SliderGrip());
	d_data->playerArea->limit2Marker()->installSceneEventFilter(d_data->playerArea->limit2SliderGrip());
	d_data->playerArea->timeScale()->scaleDraw()->enableLabelOverlapping(false);

	// TEST
	/*d_data->playerWidget->setAttribute(Qt::WA_PaintUnclipped, false);
	d_data->playerWidget->viewport()->setAttribute(Qt::WA_PaintUnclipped, false);

	d_data->playerWidget->setAttribute(Qt::WA_NoSystemBackground, false);
	d_data->playerWidget->setAttribute(Qt::WA_OpaquePaintEvent, false);
	d_data->playerWidget->viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
	d_data->playerWidget->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);*/

	VipValueToTime* v = new VipValueToTime();
	VipDateTimeScaleEngine* engine = new VipDateTimeScaleEngine();
	engine->setValueToTime(v);
	d_data->playerArea->timeScale()->setScaleEngine(engine);
	d_data->playerArea->timeScale()->scaleDraw()->setValueToText(v);

	d_data->sequential = new QToolBar();
	d_data->sequential->setObjectName("Sequential_toolbar");
	d_data->sequential->setIconSize(QSize(18, 18));
	d_data->sequential->addSeparator();

	QHBoxLayout* toolbarLayout = new QHBoxLayout();
	toolbarLayout->setContentsMargins(0, 0, 0, 0);
	toolbarLayout->addLayout(timeLay);
	QWidget* line = VipLineWidget::createSunkenVLine();
	toolbarLayout->addWidget(line);
	toolbarLayout->addWidget(d_data->playToolBar);
	toolbarLayout->addWidget(d_data->sequential);
	// line->show();

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->playerWidget);
	vlay->addLayout(toolbarLayout);
	vlay->setContentsMargins(2, 6, 2, 2);
	setLayout(vlay);

	connect(d_data->visible, SIGNAL(triggered(bool)), this, SLOT(setTimeRangeVisible(bool)));
	connect(d_data->timeUnit, SIGNAL(timeUnitChanged()), this, SLOT(timeUnitChanged()));

	connect(d_data->playerArea, SIGNAL(devicesChanged()), this, SLOT(updatePlayer()));
	connect(d_data->playerArea, SIGNAL(processingPoolChanged(VipProcessingPool*)), this, SLOT(updatePlayer()));

	connect(d_data->playerArea, SIGNAL(mouseScaleAboutToChange()), this, SLOT(disableAutoScale()));
	connect(d_data->playerArea, SIGNAL(autoScaleChanged(bool)), this, SLOT(setAutoScale(bool)));

	toolTipFlagsChanged(d_data->playerArea->plotToolTip()->displayFlags());
	setTimeRangesLocked(true);

	VipUniqueId::id(this);
	playWidgets.append(this);
}

VipPlayWidget::~VipPlayWidget()
{
	playWidgets.removeOne(this);
}

QColor VipPlayWidget::sliderColor() const
{
	return static_cast<TimeScale*>(d_data->playerArea->timeScale())->sliderColor;
}
void VipPlayWidget::setSliderColor(const QColor& c)
{
	static_cast<TimeScale*>(d_data->playerArea->timeScale())->sliderColor = c;
}

QColor VipPlayWidget::sliderFillColor() const
{
	return static_cast<TimeScale*>(d_data->playerArea->timeScale())->sliderFillColor;
}
void VipPlayWidget::setSliderFillColor(const QColor& c)
{
	static_cast<TimeScale*>(d_data->playerArea->timeScale())->sliderFillColor = c;
	d_data->playerArea->timeMarker()->setPen(c);
}

void VipPlayWidget::selectionItem()
{
	d_data->playerArea->setMouseZoomSelection(Qt::NoButton);
	d_data->playerArea->setMouseItemSelection(Qt::NoButton);
	d_data->selectionMode->setIcon(vipIcon("select_item.png"));
}

void VipPlayWidget::selectionItemArea()
{
	d_data->playerArea->setMouseZoomSelection(Qt::NoButton);
	d_data->playerArea->setMouseItemSelection(Qt::LeftButton);
	d_data->selectionMode->setIcon(vipIcon("select_area.png"));
}

void VipPlayWidget::selectionZoomArea()
{
	d_data->playerArea->setMouseZoomSelection(Qt::LeftButton);
	d_data->playerArea->setMouseItemSelection(Qt::NoButton);
	d_data->selectionMode->setIcon(vipIcon("zoom_area.png"));
}

void VipPlayWidget::toolTipChanged()
{
	QAction* act = qobject_cast<QAction*>(this->sender());
	if (!act)
		return;

	VipToolTip::DisplayFlag flag = (VipToolTip::DisplayFlag)act->data().toInt();
	bool enabled = act->isChecked();

	VipToolTip::DisplayFlags flags = playWidgetsFlags;

	if (enabled)
		flags |= flag;
	else
		flags &= (~flag);

	playWidgetsFlags = flags;

	for (int i = 0; i < playWidgets.size(); ++i)
		playWidgets[i]->toolTipFlagsChanged(flags);
}

void VipPlayWidget::toolTipFlagsChanged(VipToolTip::DisplayFlags flags)
{
	d_data->hidden->blockSignals(true);
	d_data->axes->blockSignals(true);
	d_data->title->blockSignals(true);
	d_data->legend->blockSignals(true);
	d_data->position->blockSignals(true);
	d_data->properties->blockSignals(true);

	d_data->hidden->setChecked(flags & VipToolTip::Hidden);
	d_data->axes->setChecked(flags & VipToolTip::Axes);
	d_data->title->setChecked(flags & VipToolTip::ItemsTitles);
	d_data->legend->setChecked(flags & VipToolTip::ItemsLegends);
	d_data->position->setChecked(flags & VipToolTip::ItemsPos);
	d_data->properties->setChecked(flags & VipToolTip::ItemsProperties);

	d_data->hidden->blockSignals(false);
	d_data->axes->blockSignals(false);
	d_data->title->blockSignals(false);
	d_data->legend->blockSignals(false);
	d_data->position->blockSignals(false);
	d_data->properties->blockSignals(false);

	playWidgetsFlags = flags;
	if (VipToolTip* tip = d_data->playerArea->plotToolTip()) {
		tip->setDisplayFlags(flags);
	}
}

void VipPlayWidget::setTimeType(VipValueToTime::TimeType type)
{
	if (type != timeType())
		d_data->timeUnit->setValueToTime(type);
}

VipValueToTime::TimeType VipPlayWidget::timeType() const
{
	return d_data->timeUnit->valueToTime();
}

VipPlayerArea* VipPlayWidget::area() const
{
	return const_cast<VipPlayerArea*>(d_data->playerArea);
}

VipValueToTime* VipPlayWidget::valueToTime() const
{
	return static_cast<VipValueToTime*>(d_data->playerArea->timeScale()->scaleDraw()->valueToText());
}

void VipPlayWidget::showEvent(QShowEvent*)
{
	if (d_data->playWidgetHidden)
		hide(); // evt->ignore();
}

QSize VipPlayWidget::sizeHint() const
{
	QSize size = QWidget::sizeHint();

	// compute the best height: 100 + height of VipPlayerArea
	QList<VipTimeRangeListItem*> items = area()->timeRangeListItems();
	size.setHeight(100 + items.size() * 30);
	return size;
}

void VipPlayWidget::setPlayWidgetHidden(bool hidden)
{
	d_data->playWidgetHidden = hidden;
	if (hidden)
		hide();
	else
		updatePlayer();
}

bool VipPlayWidget::playWidgetHidden() const
{
	return d_data->playWidgetHidden;
}

bool VipPlayWidget::isAutoScale() const
{
	return d_data->playerArea->isAutoScale();
}

void VipPlayWidget::setAutoScale(bool enable)
{
	if (enable != d_data->playerArea->isAutoScale()) {
		d_data->playerArea->computeStartDate();
		d_data->playerArea->setAutoScale(enable);
		d_data->autoScale->blockSignals(true);
		d_data->autoScale->setChecked(enable);
		d_data->autoScale->blockSignals(false);
	}
}

void VipPlayWidget::disableAutoScale()
{
	setAutoScale(false);
}

void VipPlayWidget::setProcessingPool(VipProcessingPool* pool)
{
	if (processingPool()) {
		disconnect(d_data->first, SIGNAL(triggered(bool)), processingPool(), SLOT(first()));
		disconnect(d_data->previous, SIGNAL(triggered(bool)), processingPool(), SLOT(previous()));
		disconnect(d_data->playForward, SIGNAL(triggered(bool)), this, SLOT(playForward()));
		disconnect(d_data->playBackward, SIGNAL(triggered(bool)), this, SLOT(playBackward()));
		disconnect(d_data->next, SIGNAL(triggered(bool)), processingPool(), SLOT(next()));
		disconnect(d_data->last, SIGNAL(triggered(bool)), processingPool(), SLOT(last()));
		disconnect(d_data->marks, SIGNAL(triggered(bool)), this, SLOT(setLimitsEnable(bool)));
		disconnect(d_data->repeat, SIGNAL(triggered(bool)), processingPool(), SLOT(setRepeat(bool)));
		disconnect(d_data->maxSpeed, SIGNAL(triggered(bool)), this, SLOT(setMaxSpeed(bool)));
		disconnect(d_data->speedWidget, SIGNAL(valueChanged(double)), processingPool(), SLOT(setPlaySpeed(double)));

		disconnect(d_data->timeEdit, SIGNAL(returnPressed()), this, SLOT(timeEdited()));
		disconnect(d_data->frameEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), processingPool(), &VipProcessingPool::seekPos);

		disconnect(d_data->autoScale, SIGNAL(triggered(bool)), this, SLOT(setAutoScale(bool)));
		disconnect(d_data->select, SIGNAL(triggered(bool)), this, SLOT(selectionClicked(bool)));
		disconnect(d_data->reset, SIGNAL(triggered(bool)), this, SLOT(resetAllTimeRanges()));
		disconnect(d_data->startAtZero, SIGNAL(triggered(bool)), d_data->playerArea, SLOT(alignToZero(bool)));

		disconnect(processingPool(), SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(updatePlayer()));
		disconnect(processingPool(), SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged()));

		disconnect(processingPool(), SIGNAL(objectAdded(QObject*)), this, SLOT(deviceAdded()));
	}

	d_data->playerArea->setProcessingPool(pool);

	if (processingPool()) {
		// connections
		connect(d_data->first, SIGNAL(triggered(bool)), processingPool(), SLOT(first()));
		connect(d_data->previous, SIGNAL(triggered(bool)), processingPool(), SLOT(previous()));
		connect(d_data->playForward, SIGNAL(triggered(bool)), this, SLOT(playForward()));
		connect(d_data->playBackward, SIGNAL(triggered(bool)), this, SLOT(playBackward()));
		connect(d_data->next, SIGNAL(triggered(bool)), processingPool(), SLOT(next()));
		connect(d_data->last, SIGNAL(triggered(bool)), processingPool(), SLOT(last()));
		connect(d_data->marks, SIGNAL(triggered(bool)), this, SLOT(setLimitsEnable(bool)));
		connect(d_data->repeat, SIGNAL(triggered(bool)), processingPool(), SLOT(setRepeat(bool)));
		connect(d_data->maxSpeed, SIGNAL(triggered(bool)), this, SLOT(setMaxSpeed(bool)));
		connect(d_data->speedWidget, SIGNAL(valueChanged(double)), processingPool(), SLOT(setPlaySpeed(double)));

		// time/number
		connect(d_data->timeEdit, SIGNAL(returnPressed()), this, SLOT(timeEdited()));
		connect(d_data->frameEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), processingPool(), &VipProcessingPool::seekPos);

		// modify timestamping
		connect(d_data->autoScale, SIGNAL(triggered(bool)), this, SLOT(setAutoScale(bool)));
		connect(d_data->select, SIGNAL(triggered(bool)), this, SLOT(selectionClicked(bool)));
		connect(d_data->reset, SIGNAL(triggered(bool)), this, SLOT(resetAllTimeRanges()));
		connect(d_data->startAtZero, SIGNAL(triggered(bool)), d_data->playerArea, SLOT(alignToZero(bool)));

		connect(processingPool(), SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(updatePlayer()));
		connect(processingPool(), SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged()));

		connect(processingPool(), SIGNAL(objectAdded(QObject*)), this, SLOT(deviceAdded()));
	}

	timeChanged();
	updatePlayer();
}

void VipPlayWidget::setAlignedToZero(bool enable)
{
	if (enable != d_data->alignedToZero) {
		d_data->startAtZero->blockSignals(true);
		d_data->startAtZero->setChecked(enable);
		d_data->startAtZero->blockSignals(false);
		d_data->playerArea->alignToZero(enable);
	}
	d_data->alignedToZero = enable;
}

bool VipPlayWidget::isAlignedToZero() const
{
	return d_data->startAtZero->isChecked();
}

void VipPlayWidget::setLimitsEnabled(bool enable)
{
	d_data->playerArea->setLimitsEnable(enable);
	d_data->marks->blockSignals(true);
	d_data->marks->setChecked(enable);
	d_data->marks->blockSignals(false);
}

bool VipPlayWidget::isLimitsEnabled() const
{
	return d_data->playerArea->limitsEnabled();
}

bool VipPlayWidget::isMaxSpeed() const
{
	return d_data->maxSpeed->isChecked();
}

double VipPlayWidget::playSpeed() const
{
	return processingPool()->playSpeed();
}

void VipPlayWidget::setPlaySpeed(double speed)
{
	d_data->speedWidget->setValue(speed);
}

void VipPlayWidget::deviceAdded()
{
	// TEST
	if (isAlignedToZero())
		setAlignedToZero(isAlignedToZero());
}

void VipPlayWidget::resetAllTimeRanges()
{
	d_data->playerArea->resetAllTimeRanges();
	d_data->startAtZero->blockSignals(true);
	d_data->startAtZero->setChecked(false);
	d_data->startAtZero->blockSignals(false);
}

void VipPlayWidget::setTimeRangeVisible(bool visible)
{
	d_data->playerArea->setTimeRangeVisible(visible);
	d_data->visible->blockSignals(true);
	d_data->visible->setChecked(visible);
	d_data->visible->blockSignals(false);
	updatePlayer();
}

bool VipPlayWidget::timeRangeVisible() const
{
	return d_data->playerArea->timeRangeVisible();
}

static VipPlayWidget::function_type _function = findBestTimeUnit;

void VipPlayWidget::setTimeUnitFunction(function_type fun)
{
	_function = fun;

	QList<VipPlayWidget*> widgets = VipUniqueId::objects<VipPlayWidget>();
	for (int i = 0; i < widgets.size(); ++i)
		widgets[i]->updatePlayer();
}

VipPlayWidget::function_type VipPlayWidget::timeUnitFunction()
{
	return _function;
}

void VipPlayWidget::updatePlayer()
{
	if (!d_data->dirtyTimeLines) {
		d_data->dirtyTimeLines = true;
		QMetaObject::invokeMethod(this, "updatePlayerInternal", Qt::QueuedConnection);
	}
}

void VipPlayWidget::updatePlayerInternal()
{
	d_data->dirtyTimeLines = false;

	if (!processingPool())
		return;

	if (timeUnitFunction() && d_data->timeUnit->automaticUnit()) {
		this->setTimeType(timeUnitFunction()(this));
		d_data->timeUnit->setAutomaticUnit(true);
	}

	d_data->first->blockSignals(true);
	d_data->previous->blockSignals(true);
	d_data->playForward->blockSignals(true);
	d_data->playBackward->blockSignals(true);
	d_data->next->blockSignals(true);
	d_data->last->blockSignals(true);
	d_data->marks->blockSignals(true);
	d_data->repeat->blockSignals(true);
	d_data->maxSpeed->blockSignals(true);
	d_data->speed->blockSignals(true);
	d_data->timeEdit->blockSignals(true);
	d_data->frameEdit->blockSignals(true);
	d_data->speedWidget->blockSignals(true);

	bool max_speed = !processingPool()->testMode(VipProcessingPool::UsePlaySpeed);
	d_data->maxSpeed->setChecked(max_speed);
	d_data->speed->setVisible(!max_speed);
	d_data->speedWidget->setValue(processingPool()->playSpeed());
	d_data->repeat->setChecked(processingPool()->testMode(VipProcessingPool::Repeat));
	d_data->marks->setChecked(processingPool()->testMode(VipProcessingPool::UseTimeLimits));
	d_data->playerArea->setLimitsEnable(processingPool()->testMode(VipProcessingPool::UseTimeLimits));

	if (processingPool()->isPlaying()) {
		if (processingPool()->testMode(VipProcessingPool::Backward))
			d_data->playBackward->setIcon(vipIcon("pause.png"));
		else
			d_data->playForward->setIcon(vipIcon("pause.png"));
	}
	else {
		d_data->playBackward->setIcon(vipIcon("playbackward.png"));
		d_data->playForward->setIcon(vipIcon("play.png"));
	}

	bool has_frame = processingPool()->deviceType() == VipIODevice::Temporal && // area()->timeRangeListItems().size() == 1 &&
			 processingPool()->size() > 0;
	if (has_frame) {
		d_data->frameEdit->setRange(0, processingPool()->size());
		d_data->frameEdit->setValue(processingPool()->timeToPos(processingPool()->time()));
	}
	d_data->timeEdit->setText(d_data->playerArea->timeScale()->scaleDraw()->label(processingPool()->time(), VipScaleDiv::MajorTick).text());

	d_data->first->blockSignals(false);
	d_data->previous->blockSignals(false);
	d_data->playForward->blockSignals(false);
	d_data->playBackward->blockSignals(false);
	d_data->next->blockSignals(false);
	d_data->last->blockSignals(false);
	d_data->marks->blockSignals(false);
	d_data->repeat->blockSignals(false);
	d_data->maxSpeed->blockSignals(false);
	d_data->speed->blockSignals(false);
	d_data->timeEdit->blockSignals(false);
	d_data->frameEdit->blockSignals(false);
	d_data->speedWidget->blockSignals(false);

	QList<VipTimeRangeListItem*> items = area()->timeRangeListItems();

	int visibleItemCount = d_data->playerArea->visibleItemCount();
	bool show_temporal = (processingPool()->deviceType() == VipIODevice::Temporal) && processingPool()->size() != 1 && visibleItemCount;

	if (!show_temporal) {
		// if no temporal devices, disable time limits
		processingPool()->blockSignals(true);
		processingPool()->setMode(VipProcessingPool::UseTimeLimits, false);
		processingPool()->blockSignals(false);
	}

	d_data->time->setVisible(show_temporal);
	d_data->timeEdit->setVisible(show_temporal);
	d_data->timeUnit->setVisible(show_temporal);
	d_data->frameEdit->setVisible(show_temporal && has_frame);
	d_data->playerWidget->setVisible(show_temporal);
	d_data->playToolBar->setVisible(show_temporal);
	// TODO: play widget not drawn with opengl enabled (all black)
	// if (show_temporal)
	//	d_data->playerWidget->repaint();

	// int dpi = logicalDpiY();
	//  double millimeter_to_pixel = (dpi / 2.54 / 10);
	//  double _50 = 50 / millimeter_to_pixel;
	//  double _15 = 15 / millimeter_to_pixel;
	//  double _20 = 20 / millimeter_to_pixel;
	//  double _30 = 30 / millimeter_to_pixel;

	// compute the best height
	int height = 0;
	if (show_temporal) {
		height = 35 + d_data->playerArea->timeScale()->scaleDraw()->fullExtent();
		if (timeRangeVisible())
			height += visibleItemCount * 14;
		layout()->setSpacing(2);
	}
	else
		layout()->setSpacing(0);

	if (d_data->speed->isVisible()) {
		int th = d_data->playToolBar->height();
		int sh = d_data->speedWidget->height();
		if (sh > th)
			height += sh - th;
	}

	// add bottom margin (NEW)
	height += 10;

	this->setVisible(show_temporal); // height > 0);
	if (height != d_data->height) {
		d_data->height = height;
		this->setMaximumHeight(height);
		this->setMinimumHeight(height);

		// try to update the time scale (TODO: find a better way)
		int w = d_data->playerWidget->width();
		int h = d_data->playerWidget->height();
		d_data->playerWidget->resize(w + 1, h + 1);
		d_data->playerWidget->resize(w, h);
		d_data->playerWidget->recomputeGeometry();
		vipProcessEvents(nullptr, 100);
	}
}

void VipPlayWidget::timeChanged()
{
	if (!processingPool())
		return;

	d_data->frameEdit->blockSignals(true);
	d_data->timeEdit->blockSignals(true);

	bool has_frame = processingPool()->size() > 0;
	// d_data->frameEdit->setVisible(has_frame);
	if (has_frame) {
		// d_data->frameEdit->setRange(0,processingPool()->size());
		d_data->frameEdit->setValue(processingPool()->timeToPos(processingPool()->time()));
	}
	d_data->timeEdit->setText(d_data->playerArea->timeScale()->scaleDraw()->label(processingPool()->time(), VipScaleDiv::MajorTick).text());

	d_data->frameEdit->blockSignals(false);
	d_data->timeEdit->blockSignals(false);
}

void VipPlayWidget::timeUnitChanged()
{
	VipValueToTime* v = d_data->timeUnit->currentValueToTime().copy();
	static_cast<VipDateTimeScaleEngine*>(d_data->playerArea->timeScale()->scaleEngine())->setValueToTime(v);
	d_data->playerArea->timeScale()->scaleDraw()->setValueToText(v);

	d_data->playerArea->computeStartDate();

	// We change the time unit, so get back to auto scale and recompute it
	d_data->playerArea->setAutoScale(false);
	d_data->playerArea->setAutoScale(true);

	updatePlayer();

	Q_EMIT valueToTimeChanged(v);
}

VipProcessingPool* VipPlayWidget::processingPool() const
{
	return d_data->playerArea->processingPool();
}

void VipPlayWidget::timeEdited()
{
	// retrieve the time inside timeEdit and update the VipProcessingPool's time
	QString value = d_data->timeEdit->text();
	bool ok;
	double time = d_data->playerArea->timeScale()->scaleDraw()->valueToText()->fromString(value, &ok);
	if (ok) {
		d_data->timeEdit->setStyleSheet("QLabel { border: 0px solid transparent; }");
		processingPool()->seek(time);
	}
	else {
		d_data->timeEdit->setStyleSheet("QLabel { border: 1px solid red; }");
	}
}

void VipPlayWidget::setMaxSpeed(bool enable)
{
	d_data->maxSpeed->blockSignals(true);
	d_data->maxSpeed->blockSignals(enable);
	d_data->maxSpeed->blockSignals(false);

	d_data->speed->setVisible(!enable);
	d_data->speedWidget->setVisible(!enable);
	processingPool()->setMode(VipProcessingPool::UsePlaySpeed, !enable);
}

void VipPlayWidget::playForward()
{
	if (!processingPool())
		return;

	if (processingPool()->isPlaying())
		processingPool()->stop();
	else {
		processingPool()->setMode(VipProcessingPool::Backward, false);
		processingPool()->play();
	}
}

void VipPlayWidget::playBackward()
{
	if (!processingPool())
		return;

	if (processingPool()->isPlaying())
		processingPool()->stop();
	else {
		processingPool()->setMode(VipProcessingPool::Backward, true);
		processingPool()->play();
	}
}

void VipPlayWidget::selectionClicked(bool enable)
{
	// enable/disable the mouse item selection
	if (enable)
		d_data->playerArea->setMouseItemSelection(Qt::LeftButton);
	else
		d_data->playerArea->setMouseItemSelection(Qt::NoButton);
}

void VipPlayWidget::setLimitsEnable(bool enable)
{
	if (enable) {
		qint64 first = processingPool()->stopBeginTime();
		qint64 last = processingPool()->stopEndTime();
		if (first == VipInvalidTime) {
			first = processingPool()->firstTime();
			processingPool()->setStopBeginTime(first);
		}
		if (last == VipInvalidTime) {
			last = processingPool()->lastTime();
			processingPool()->setStopEndTime(last);
		}
	}

	d_data->playerArea->setLimitsEnable(enable);
}

bool VipPlayWidget::timeRangesLocked() const
{
	return d_data->playerArea->timeRangesLocked();
}

void VipPlayWidget::setTimeRangesLocked(bool locked)
{
	d_data->lock->blockSignals(true);
	d_data->lock->setChecked(locked);
	d_data->lock->blockSignals(false);
	d_data->playerArea->setTimeRangesLocked(locked);
}

VipArchive& operator<<(VipArchive& arch, VipPlayWidget* w)
{
	arch.content("aligned", w->isAlignedToZero());
	arch.content("visible_ranges", w->timeRangeVisible());
	arch.content("auto_scale", w->isAutoScale());
	arch.content("time_limits", w->isLimitsEnabled());
	arch.content("time_limit1", w->processingPool()->stopBeginTime());
	arch.content("time_limit2", w->processingPool()->stopEndTime());
	arch.content("max_speed", w->isMaxSpeed());
	arch.content("speed", w->playSpeed());
	arch.content("time", w->area()->timeSliderGrip()->value());

	// axis scale div
	arch.content("x_scale", w->area()->bottomAxis()->scaleDiv());
	arch.content("y_scale", w->area()->leftAxis()->scaleDiv());

	arch.content("locked", w->timeRangesLocked());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlayWidget* w)
{
	w->setAlignedToZero(arch.read("aligned").toBool());
	w->setTimeRangeVisible(arch.read("visible_ranges").toInt());
	w->setAutoScale(arch.read("auto_scale").toInt());
	w->setLimitsEnable(arch.read("time_limits").toInt());
	qint64 l1 = arch.read("time_limit1").toLongLong();
	qint64 l2 = arch.read("time_limit2").toLongLong();
	if (l1 != VipInvalidTime)
		w->area()->setLimit1(l1);
	if (l2 != VipInvalidTime)
		w->area()->setLimit2(l2);
	w->setMaxSpeed(arch.read("max_speed").toBool());
	w->setPlaySpeed(arch.read("speed").toDouble());
	w->area()->setTime(arch.read("time").toDouble());

	VipScaleDiv divx = arch.read("x_scale").value<VipScaleDiv>();
	VipScaleDiv divy = arch.read("y_scale").value<VipScaleDiv>();

	if (!w->isAutoScale()) {
		w->area()->leftAxis()->setScaleDiv(divx);
		w->area()->bottomAxis()->setScaleDiv(divy);
	}

	w->setTimeRangesLocked(arch.read("locked").toBool());

	return arch;
}

static int registerFunctions()
{
	vipRegisterArchiveStreamOperators<VipPlayWidget*>();
	return 0;
}

static int _registerFunctions = vipAddInitializationFunction(registerFunctions);
