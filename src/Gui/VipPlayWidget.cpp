#include "VipPlayWidget.h"
#include "VipIODevice.h"
#include "VipStandardWidgets.h"
#include "VipDisplayObject.h"
#include "VipProgress.h"
#include "VipPlotGrid.h"
#include "VipColorMap.h"
#include "VipPainter.h"
#include "VipScaleEngine.h"
#include "VipDoubleSlider.h"
#include "VipToolTip.h"
#include "VipCorrectedTip.h"

#include <QLineEdit>
#include <QLabel>
#include <QToolBar>
#include <QSpinBox>
#include <QToolButton>
#include <QGridLayout>
#include <QAction>
#include <QMenu>
#include <QCursor>
#include <QTimer>
#include <QLinearGradient>
#include <QGraphicsSceneHoverEvent>
#include <qapplication.h>

//#include <iostream>

#define ITEM_START_HEIGHT 0.1
#define ITEM_END_HEIGHT 0.9

static QString lockedMessage()
{
	static const QString res = "<b>Time ranges are locked!</b><br>"
		"You cannot move the time ranges while locked.<br>"
		"To unlock them, uncheck the " + QString(vipToHtml(vipPixmap("lock.png"), "align ='middle'")) + " icon";
	return res;
}

class VipTimeRangeItem::PrivateData
{
public:
	PrivateData()
	: parentItem(NULL), heights(ITEM_START_HEIGHT,ITEM_END_HEIGHT),left(0),right(0),color(Qt::blue),reverse(false) , selection(-2){}

	VipTimeRange initialTimeRange;
	VipTimeRangeListItem *parentItem;
	QPair<double,double> heights;
	qint64 left,right;
	QPainterPath resizeLeft;
	QPainterPath resizeRight;
	QPainterPath moveArea;
	QColor color;
	bool reverse;

	QPointF pos; //for mouse moving
	QMap<VipTimeRangeItem*,QPair<qint64,qint64> > initPos;
	int selection; //mouse selection (-1=left, 0=middle, 1=right)
};

VipTimeRangeItem::VipTimeRangeItem(VipTimeRangeListItem * item)
:VipPlotItem(VipText())
{
	m_data = new PrivateData();
	m_data->parentItem = item;

	this->setItemAttribute(HasLegendIcon, false);
	this->setItemAttribute(VisibleLegend, false);
	this->setItemAttribute(AutoScale);
	this->setItemAttribute(HasToolTip);
	this->setIgnoreStyleSheet(true);
	if(m_data->parentItem )
	{
		//register into parent
		m_data->parentItem->addItem( this);
		connect( this,SIGNAL( timeRangeChanged()),m_data->parentItem ,SLOT(updateDevice()));
		this->setAxes(m_data->parentItem->axes(),VipCoordinateSystem::Cartesian);
	}


}

VipTimeRangeItem::~VipTimeRangeItem()
{
	if(m_data->parentItem )
		m_data->parentItem->removeItem( this);

	delete m_data;
	//std::cout<<"~VipTimeRangeItem()"<<std::endl;
}

QList<VipInterval> VipTimeRangeItem::plotBoundingIntervals() const
{
	return QList<VipInterval>() << VipInterval(m_data->left,m_data->right) << VipInterval(m_data->heights.first, m_data->heights.second);
}

void VipTimeRangeItem::setInitialTimeRange(const VipTimeRange & range)
{
	m_data->initialTimeRange = range;
	setCurrentTimeRange(range);
}

VipTimeRange VipTimeRangeItem::initialTimeRange() const
{
	return m_data->initialTimeRange;
}

void VipTimeRangeItem::setCurrentTimeRange(const VipTimeRange & r)
{
	if(m_data->left != r.first || m_data->right != r.second)
	{
		//bool first_set = m_data->left == 0 && m_data->right == 0;
		m_data->left = r.first;
		m_data->right = r.second;
		Q_EMIT timeRangeChanged();
		update();
	}
}

void VipTimeRangeItem::setCurrentTimeRange(qint64 left, qint64 right )
{
	if(m_data->left != left || m_data->right != right)
	{
		m_data->left = left;
		m_data->right = right;
		Q_EMIT timeRangeChanged();
		update();
	}
}

VipTimeRange VipTimeRangeItem::currentTimeRange()
{
	VipTimeRange res(m_data->left,m_data->right);
	if(m_data->reverse)
		res = vipReorder(res,Vip::Descending);
	else
		res = vipReorder(res,Vip::Ascending);
	return res;
}

qint64 VipTimeRangeItem::left() const
{
	return m_data->left;
}

qint64 VipTimeRangeItem::right() const
{
	return m_data->right;
}

VipTimeRangeListItem * VipTimeRangeItem::parentItem() const
{
	return m_data->parentItem;
}

void VipTimeRangeItem::setHeights(double start, double end)
{
	if (m_data->heights != QPair<double, double>(start, end))
	{
		m_data->heights = QPair<double, double>(start, end);
		update();
	}
}

QPair<double,double> VipTimeRangeItem::heights() const
{
	return m_data->heights;
}

void VipTimeRangeItem::setColor(const QColor & c)
{
	if (m_data->color != c)
	{
		m_data->color = c;
		update();
	}
}

QColor VipTimeRangeItem::color() const
{
	return m_data->color;
}

void VipTimeRangeItem::draw(QPainter * p, const VipCoordinateSystemPtr & m) const
{
	Q_UNUSED(m)

	shape();
	QRectF r = m_data->moveArea.boundingRect();
	//draw the rectange
	{
	QLinearGradient grad(r.topLeft(),r.bottomLeft());
	QGradientStops stops;
	stops << QGradientStop(0,m_data->color.darker(120));
	stops << QGradientStop(ITEM_START_HEIGHT,m_data->color);
	stops << QGradientStop(ITEM_END_HEIGHT,m_data->color);
	stops << QGradientStop(1,m_data->color.darker(120));
	grad.setStops(stops);

	QPen pen(m_data->color.darker(120));
	QBrush brush(grad);
	p->setPen(pen);
	p->setBrush(brush);

	p->setRenderHint(QPainter::Antialiasing,false);
	p->drawRect(r);//drawRoundedRect(r,3,3);
	}

	QLinearGradient grad(r.topLeft(),r.bottomLeft());
	QGradientStops stops;
	QColor c(255,165,0);
	//stops << QGradientStop(0,c.darker(120));
	// stops << QGradientStop(ITEM_START_HEIGHT,c);
	// stops << QGradientStop(ITEM_END_HEIGHT,c);
	// stops << QGradientStop(1,c.darker(120));
	// grad.setStops(stops);
//
	// QPen pen(c.darker(120));
	QBrush brush(c);//(grad);
	QRectF direction = r;
	if(m_data->reverse)
		direction.setLeft(r.right()-5);
	else
		direction.setRight(r.left()+5);

	//Draw the direction rectangle that indicates where the time range starts.
	//If the time range is too small, do not draw it to avoid overriding the display
	//p->setPen(pen);
	p->setBrush(brush);
	QRectF direction_rect = direction & r;
	if(direction_rect.width() > 4)
		p->drawRect(direction & r);
		//p->drawRoundedRect(direction & r,3,3);

	p->setRenderHints(QPainter::Antialiasing);
	//draw the left and right arrow if the rectangle width is > 2
	if (r.width() > 10)
	{
		p->setPen(QPen(m_data->color.darker(120), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p->setBrush(m_data->color);
		p->drawPath(m_data->resizeLeft);
		p->drawPath(m_data->resizeRight);
	}

	//draw the selection
	if(isSelected())
	{
		p->setPen(QPen(m_data->color.darker(120)));
		p->setBrush(QBrush(m_data->color.darker(120),Qt::BDiagPattern));
		p->drawRect(r);//drawRoundedRect(r,3,3);
	}
}

void VipTimeRangeItem::drawSelected( QPainter * p, const VipCoordinateSystemPtr & m) const
{
	draw(p,m);
}

bool VipTimeRangeItem::applyTransform(const QTransform & tr)
{
	m_data->left = qRound64(tr.map(QPointF(m_data->left,0)).x());
	m_data->right = qRound64(tr.map(QPointF(m_data->right,0)).x());
	update();
	return true;
}

void VipTimeRangeItem::setReverse(bool reverse)
{
	if(reverse != m_data->reverse)
	{
		m_data->reverse = reverse;
		Q_EMIT timeRangeChanged();
		update();
	}
}

bool VipTimeRangeItem::reverse() const
{
	return m_data->reverse;
}

QRectF VipTimeRangeItem::boundingRect() const
{
	return shape().boundingRect();
}

QPainterPath VipTimeRangeItem::shape() const
{
	QRectF r = sceneMap()->transformRect(QRectF(QPointF(m_data->left,m_data->heights.second), QPointF(m_data->right,m_data->heights.first) )).normalized();
	if (r.width() == 0) {
		r.setRight(r.right() + 1);
		r.setLeft(r.left() - 1);
	}
	r.setTop(r.top() - 1);

	m_data->moveArea = QPainterPath();
	m_data->moveArea.addRect( r );

	m_data->resizeLeft = QPainterPath();
	m_data->resizeRight = QPainterPath();

	if(m_data->left != m_data->right)
	{
		QSizeF size(7,9);
		size.setHeight(qMin(r.height(), size.height()));
		QPolygonF leftArrow = QPolygonF() << QPointF(r.left() - size.width(), r.center().y()) << QPointF(r.left(),r.center().y() - size.height()/2) << QPointF(r.left(),r.center().y() + size.height()/2) <<QPointF(r.left() - size.width(), r.center().y()) ;
		QPolygonF rightArrow = QPolygonF() << QPointF(r.right() + size.width(), r.center().y()) << QPointF(r.right(),r.center().y() - size.height()/2) << QPointF(r.right(),r.center().y() + size.height()/2) <<QPointF(r.right() + size.width(), r.center().y());
		m_data->resizeLeft.addPolygon(leftArrow);
		m_data->resizeRight.addPolygon(rightArrow);
	}
	return m_data->resizeLeft  | m_data->resizeRight |m_data->moveArea;
}

void	VipTimeRangeItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
	QPointF p = event->pos();
	if(m_data->resizeLeft.contains(p))
		this->setCursor(Qt::SizeHorCursor);
	else if(m_data->resizeRight.contains(p))
		this->setCursor(Qt::SizeHorCursor);
	else
		this->setCursor(//Qt::SizeAllCursor
QCursor());
}

void	VipTimeRangeItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
	if (parentItem()->area()->timeRangesLocked())
	{
		VipCorrectedTip::showText(QCursor::pos(),lockedMessage(),NULL,QRect(),5000);
		return;
	}

	qint64 diff = qRound64((sceneMap()->invTransform(event->pos()) - sceneMap()->invTransform(m_data->pos)).x());

	if (m_data->selection == -1) {//left arrow
		setCurrentTimeRange(parentItem()->closestTime(m_data->initPos[this].first + diff), m_data->initPos[this].second);
		Q_EMIT parentItem()->itemsTimeRangeChanged(QList<VipTimeRangeItem*>()<<this);
	}
	else if (m_data->selection == 1) {//right arrow
		setCurrentTimeRange(m_data->initPos[this].first, parentItem()->closestTime(m_data->initPos[this].second + diff));
		Q_EMIT parentItem()->itemsTimeRangeChanged(QList<VipTimeRangeItem*>() << this);
	}
	else if(m_data->selection == 0)
	{
		//apply to all selected items
		for (QMap<VipTimeRangeItem*, QPair<qint64, qint64> >::iterator it = m_data->initPos.begin(); it != m_data->initPos.end(); ++it) {
			VipTimeRangeItem * item = it.key();
			item->setCurrentTimeRange(item->parentItem()->closestTime(it.value().first + diff), item->parentItem()->closestTime(it.value().second + diff));
		}

		Q_EMIT parentItem()->itemsTimeRangeChanged(m_data->initPos.keys());
	}

	if (parentItem()) {
		parentItem()->computeToolTip();
	}

	//if(diff != 0)
	//	m_data->pos = event->pos();

	if(m_data->selection != -2)
		emitItemChanged();
}

void	VipTimeRangeItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
	VipPlotItem::mousePressEvent(event);
	m_data->pos = event->pos();
	if(m_data->resizeLeft.contains(m_data->pos))
		m_data->selection = -1;
	else if(m_data->resizeRight.contains(m_data->pos))
		m_data->selection = 1;
	else
		m_data->selection = 0;

	//Small trick: a time range of 1ns means that this is an empty time range.
	//we just to specify a valid time range (> 0 ns) to display the actual VipTimeRangeItem.
	if(m_data->selection != 0 && m_data->initialTimeRange.second - m_data->initialTimeRange.first == 0)//1)
		m_data->selection = 0;

	m_data->initPos.clear();
	m_data->initPos[this] = QPair<qint64,qint64>(m_data->left, m_data->right);
	if (m_data->selection == 0) {
		//compute initial left and right values for all selected items
		QList<VipTimeRangeItem*> items = m_data->parentItem->items();
		for (int i = 0; i < items.size(); ++i)
			if (items[i]->isSelected()) {
				m_data->initPos[items[i]] = QPair<qint64, qint64>(items[i]->left(), items[i]->right());
			}
	}
}

void	VipTimeRangeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent * )
{
	m_data->selection = -2;
}

QVariant VipTimeRangeItem::itemChange ( GraphicsItemChange change, const QVariant & value )
{
	if(change == QGraphicsItem::ItemSelectedHasChanged)
	{
		bool selected = false;
		QList<VipTimeRangeItem*> items = m_data->parentItem->items();
		for(int i=0; i < items.size(); ++i)
			if(items[i]->isSelected())
			{
				selected = true;
				break;
			}

		//m_data->parentItem->setVisible(items.size() > 1);
		m_data->parentItem->setSelected(selected);
		m_data->parentItem->prepareGeometryChange();

		Q_EMIT parentItem()->itemSelectionChanged(this);
	}

	return VipPlotItem::itemChange(change,value);
}


VipFunctionDispatcher<2> & vipCreateTimeRangeItemsDispatcher()
{
	static VipFunctionDispatcher<2> inst;
	return inst;
}


class VipTimeRangeListItem::PrivateData
{
public:
	PrivateData():device(NULL), heights(ITEM_START_HEIGHT,ITEM_END_HEIGHT), selection(-2),state(Visible) , drawComponents(VipTimeRangeListItem::All), area(NULL){}

	QList<VipTimeRangeItem *> items;
	QPointer<VipIODevice> device;
	QPair<double,double> heights;
	QColor color;

	QPainterPath resizeLeft;
	QPainterPath resizeRight;
	QPainterPath moveArea;

	QPointF pos;
	VipTimeRange initRange;
	QMap<VipTimeRangeItem*, QPair<qint64, qint64> > initItemRanges;
	int selection;
	int state;
	DrawComponents drawComponents;

	VipTimeRangeListItem::draw_function drawFunction;
	VipTimeRangeListItem::change_time_range_function changeTimeRangeFunction;
	VipTimeRangeListItem::closest_time_function closestTimeFunction;
	VipPlayerArea * area;
};

VipTimeRangeListItem::VipTimeRangeListItem(VipPlayerArea * area)
:VipPlotItem(VipText())
{
	m_data = new PrivateData();
	m_data->area = area;
	this->setItemAttribute(AutoScale);
	this->setItemAttribute(HasLegendIcon,false);
	this->setItemAttribute(VisibleLegend,false);
	this->setRenderHints(QPainter::Antialiasing);

	connect(this,SIGNAL( itemChanged(VipPlotItem*)),this,SLOT(updateDevice()));
}

VipTimeRangeListItem::~VipTimeRangeListItem()
{
	while(m_data->items.size())
		delete m_data->items.first();
	delete m_data;
}

void VipTimeRangeListItem::setDrawComponents(DrawComponents c)
{
	m_data->drawComponents = c;
	update();
}
void VipTimeRangeListItem::setDrawComponent(DrawComponent c, bool on )
{
	if (m_data->drawComponents.testFlag(c) != on)
	{
		if (on) {
			m_data->drawComponents |= c;
		}
		else {
			m_data->drawComponents &= ~c;
		}
		update();
	}
}
bool VipTimeRangeListItem::testDrawComponent(DrawComponent c) const
{
	return m_data->drawComponents.testFlag(c);
}
VipTimeRangeListItem::DrawComponents VipTimeRangeListItem::drawComponents() const
{
	return m_data->drawComponents;
}

void VipTimeRangeListItem::setStates(int st)
{
	m_data->state = st;
}

int VipTimeRangeListItem::states() const
{
	return m_data->state;
}

QList<VipInterval> VipTimeRangeListItem::plotBoundingIntervals() const
{
	if (m_data->device && m_data->device->deviceType() == VipIODevice::Temporal)
	{
		return QList<VipInterval>()
			<< VipInterval(m_data->device->firstTime(), m_data->device->lastTime())
			<< VipInterval(m_data->heights.first, m_data->heights.second);
	}
	return QList<VipInterval>();
}

void VipTimeRangeListItem::reset()
{
	VipIODevice * device = m_data->device;
	setDevice(NULL);

	if(device)
		device->resetTimestampingFilter();

	setDevice(device);
}

void VipTimeRangeListItem::deviceTimestampingChanged()
{
	VipIODevice * dev = m_data->device;
	setDevice(NULL);
	setDevice(dev);
}

void VipTimeRangeListItem::computeToolTip()
{
	if (VipIODevice* device = m_data->device)
	{
		QString name = device->name();
		if (device->outputCount() == 1)
			name = device->outputAt(0)->data().name();
		QString tool_tip = "<b>Name</b>: " + name;
		if (device->deviceType() == VipIODevice::Temporal)
		{
			//for temporal devices, set the duration and size as tool tip text
			if (device->size() != VipInvalidPosition)
				tool_tip += "<br><b>Size</b>: " + QString::number(device->size());
			if (device->firstTime() != VipInvalidTime && device->lastTime() != VipInvalidTime)
			{
				VipValueToTime::TimeType type = VipValueToTime::findBestTimeUnit(VipInterval(device->firstTime(), device->lastTime()));
				tool_tip += "<br><b>Duration</b>: " + VipValueToTime::convert(device->lastTime() - device->firstTime(), VipValueToTime::TimeType(type % 2 ? type - 1 : type)); //remove SE from time type
				tool_tip += "<br><b>Start</b>: " + VipValueToTime::convert(device->firstTime(), type, "dd.MM.yyyy hh:mm:ss.zzz");
				tool_tip += "<br><b>End</b>: " + VipValueToTime::convert(device->lastTime(), type, "dd.MM.yyyy hh:mm:ss.zzz");

				QString date = device->attribute("Date").toString();
				if (!date.isEmpty())
					tool_tip += "<br><b>Date</b>: " + date;

			}
		}

		this->setToolTipText(tool_tip);
		for (int i = 0; i < m_data->items.size(); ++i)
		{
			m_data->items[i]->setToolTipText(tool_tip);
		}
	}
}

void VipTimeRangeListItem::setDevice(VipIODevice* device)
{
	if(m_data->device)
	{
		disconnect(m_data->device, SIGNAL(timestampingChanged()), this, SLOT(deviceTimestampingChanged()));

		//remove the current managed items
		while(m_data->items.size())
			delete m_data->items.first();
		m_data->device = NULL;
	}

	if(device)
	{
		m_data->device = device;

		connect(device, SIGNAL(timestampingChanged()), this, SLOT(deviceTimestampingChanged()));

		const auto lst = vipCreateTimeRangeItemsDispatcher().match(device, this);
		if (lst.size())
			lst.last()(device, this);
		else {

			//create the managed items
			//take into account the possibly installed filter
			const VipTimestampingFilter & filter = m_data->device->timestampingFilter();

			if (!filter.isEmpty())
			{
				//use the VipTimeRangeTransforms in case of filtering
				VipTimeRangeTransforms  trs = filter.validTransforms();
				for (VipTimeRangeTransforms::const_iterator it = trs.begin(); it != trs.end(); ++it)
				{
					VipTimeRangeItem * item = new VipTimeRangeItem(this);
					item->blockSignals(true);
					item->setInitialTimeRange(it.key());
					item->setCurrentTimeRange(it.value());
					item->setColor(m_data->color);
					item->setReverse(it.value().first > it.value().second);
					item->setRenderHints(QPainter::Antialiasing);
					item->blockSignals(false);
				}
			}
			else
			{
				//use the original time window in case of no filtering
				VipTimeRangeList _lst = m_data->device->timeWindow();
				for (int i = 0; i < _lst.size(); ++i)
				{
					VipTimeRangeItem * item = new VipTimeRangeItem(this);
					item->blockSignals(true);
					item->setInitialTimeRange(_lst[i]);
					item->setRenderHints(QPainter::Antialiasing);
					item->setColor(m_data->color);
					item->blockSignals(false);
				}
			}
		}

		setHeights(heights().first,heights().second);

		computeToolTip();
	}

	emitItemChanged();
}

VipIODevice *VipTimeRangeListItem::device() const
{
	return m_data->device;
}

QList<VipTimeRangeItem*> VipTimeRangeListItem::items() const
{
	return m_data->items;
}

QList<qint64> VipTimeRangeListItem::stops() const
{
	QList<qint64> res;
	for(int i=0; i < m_data->items.size(); ++i)
	{
		VipTimeRange range = m_data->items[i]->currentTimeRange();
		res << range.first << range.second;
	}
	return res;
}

VipTimeRangeTransforms VipTimeRangeListItem::transforms() const
{
	QList<VipTimeRangeItem*>  lst = items();
	VipTimeRangeTransforms res;

	bool has_transform = false;
	for(int i=0; i < lst.size(); ++i)
	{
		//VipTimeRange init = lst[i]->initialTimeRange();
		//VipTimeRange cur =  lst[i]->currentTimeRange();
		res [lst[i]->initialTimeRange()] = lst[i]->currentTimeRange();
		if(lst[i]->initialTimeRange() != lst[i]->currentTimeRange())
			has_transform = true;
	}

	if(has_transform)
		return res;
	else
		return VipTimeRangeTransforms();
}

void VipTimeRangeListItem::setHeights(double start, double end)
{
	//if (m_data->heights != QPair<double, double>(start, end))
	{
		m_data->heights = QPair<double, double>(start, end);
		QList<VipTimeRangeItem*>  lst = items();

		for (int i = 0; i < lst.size(); ++i)
			lst[i]->setHeights(start, end);

		update();
		//emitItemChanged();
	}
}

void VipTimeRangeListItem::setColor(const QColor & c)
{
	if (m_data->color != c)
	{
		m_data->color = c;
		QList<VipTimeRangeItem*>  lst = items();

		for (int i = 0; i < lst.size(); ++i)
			lst[i]->setColor(c);

		emitItemChanged();
	}
}

QColor VipTimeRangeListItem::color() const
{
	return m_data->color;
}

void VipTimeRangeListItem::setAdditionalDrawFunction(draw_function fun)
{
	m_data->drawFunction = fun;
	update();
}
VipTimeRangeListItem::draw_function VipTimeRangeListItem::additionalDrawFunction() const
{
	return m_data->drawFunction;
}

void VipTimeRangeListItem::setChangeTimeRangeFunction(change_time_range_function fun)
{
	m_data->changeTimeRangeFunction = fun;
}
VipTimeRangeListItem::change_time_range_function VipTimeRangeListItem::changeTimeRangeFunction() const
{
	return m_data->changeTimeRangeFunction;
}

void VipTimeRangeListItem::setClosestTimeFunction(closest_time_function fun)
{
	m_data->closestTimeFunction = fun;
}
VipTimeRangeListItem::closest_time_function VipTimeRangeListItem::closestTimeFunction() const
{
	return m_data->closestTimeFunction;
}
qint64 VipTimeRangeListItem::closestTime(qint64 time) const
{
	if (m_data->closestTimeFunction)
		return m_data->closestTimeFunction(this, time);
	return time;
}

QPair<double,double> VipTimeRangeListItem::heights() const
{
	return m_data->heights;
}

QRectF VipTimeRangeListItem::itemsBoundingRect() const
{
	QRectF bounding;
	for(int i=0; i < m_data->items.size(); ++i)
		bounding |= m_data->items[i]->boundingRect();
	return bounding;
}

QPair<qint64,qint64> VipTimeRangeListItem::itemsRange() const
{
	if(m_data->items.size())
	{
		qint64 left = m_data->items.first()->left();
		qint64 right = m_data->items.first()->right();
		for(int i=0; i < m_data->items.size(); ++i)
		{
			left = qMin(left, m_data->items[i]->left());
			right = qMax(right, m_data->items[i]->right());
		}
		return QPair<qint64,qint64>(left,right);
	}
	return QPair<qint64,qint64>(0,0);
}

QRectF VipTimeRangeListItem::boundingRect() const
{
	return shape().boundingRect();
}

VipPlayerArea * VipTimeRangeListItem::area() const
{
	return m_data->area;
}

QPainterPath VipTimeRangeListItem::shape() const
{
	if (m_data->items.size() < 2)
		return QPainterPath();

	QRectF bounding= itemsBoundingRect().normalized().adjusted(-2,-2,2,2);

	int width = 5;
	QPainterPath moving;
	moving.addRect(QRectF(bounding.left() - width -2, bounding.top(), width, bounding.height()));
	moving.addRect(QRectF(bounding.right() +2, bounding.top(), width, bounding.height()));

	bounding.adjust(-width-4, 0, width+4, 0);

	//compute move area
	//QPainterPathStroker stroke;
	// stroke.setWidth(4);
	// QPainterPath moving;
	// moving.addRect(bounding);
	m_data->moveArea = moving;//stroke.createStroke(moving);

	//compute arrows
	m_data->resizeLeft = QPainterPath();
	m_data->resizeRight = QPainterPath();

	QSizeF size(7,9);
	QPolygonF leftArrow = QPolygonF() << QPointF(bounding.left() - size.width(), bounding.center().y()) << QPointF(bounding.left(),bounding.center().y() - size.height()/2) <<
			QPointF(bounding.left(),bounding.center().y() + size.height()/2) <<QPointF(bounding.left() - size.width(), bounding.center().y());
	QPolygonF rightArrow = QPolygonF() << QPointF(bounding.right() + size.width(), bounding.center().y()) << QPointF(bounding.right(),bounding.center().y() - size.height()/2) <<
			QPointF(bounding.right(),bounding.center().y() + size.height()/2) << QPointF(bounding.right() + size.width(), bounding.center().y());
	m_data->resizeLeft.addPolygon(leftArrow);
	m_data->resizeRight.addPolygon(rightArrow);

	QPainterPath res = m_data->resizeLeft | m_data->resizeRight;
	//if(isSelected())
		res |= m_data->moveArea;
	return res;
}

void VipTimeRangeListItem::draw(QPainter * p, const VipCoordinateSystemPtr & m) const
{
	drawSelected(p, m);
}


void VipTimeRangeListItem::drawSelected( QPainter  *p, const VipCoordinateSystemPtr & m) const
{
	if (m_data->items.size())
	{
		//draw the device name
		if (zValue() <= m_data->items.first()->zValue())
			const_cast<VipTimeRangeListItem*>(this)->setZValue(m_data->items.first()->zValue() + 1);

		QRectF bounding = itemsBoundingRect();
		QPointF left = bounding.bottomLeft() + QPointF(20,-1);//m->transform(QPointF(m_data->items.first()->left(), this->heights().first));

		if (m_data->drawComponents & Text)
		{
			p->save();
			p->setRenderHints(QPainter::TextAntialiasing);
			//p->setCompositionMode(QPainter::CompositionMode_Difference);
			QColor c = m_data->items.first()->color();
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

		//set the items color if necessary
		QVariant v = device()->property("_vip_color");
		if (v.userType() == qMetaTypeId<QColor>()) {
			QColor c = v.value<QColor>();
			if (c != Qt::transparent) {
				const_cast<VipTimeRangeListItem*>(this)->setColor(c);
			}

		}
	}

	shape();



	if ((m_data->drawComponents & MovingArea) && m_data->items.size() > 1)
	{
		p->setRenderHint(QPainter::Antialiasing, false);

		//draw moving area
		p->setPen(QColor(0x4D91AE));
		p->setBrush(QBrush(QColor(0x22BDFF)));
		p->drawPath(m_data->moveArea);
	}
	if ((m_data->drawComponents & ResizeArea) && m_data->items.size() > 1)
	{
		p->setRenderHint(QPainter::Antialiasing, true);

		//draw arrows
		//p->setPen(QColor(0xEEE058).darker(180));
		//p->setBrush(QBrush(QColor(0xEEE058)));
		p->setPen(QColor(0x4D91AE));
		p->setBrush(QBrush(QColor(0x22BDFF)));
		p->drawPath(m_data->resizeLeft);
		p->drawPath(m_data->resizeRight);
	}

	if (m_data->drawFunction)
		m_data->drawFunction(this, p, m);
}

void VipTimeRangeListItem::split(VipTimeRangeItem * , qint64 )
{
	//VipTimeRange range(qRound64(item->left()),qRound64(item->right()));
	// if(time > range.first && time < range.second)
	// {
	// VipTimeRange r1(range.first,time)
	// }
}

bool VipTimeRangeListItem::applyTransform(const QTransform & tr)
{
	for(int i=0; i < m_data->items.size(); ++i)
		m_data->items[i]->applyTransform(tr);
	update();
	return true;
}

void	VipTimeRangeListItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
	QPointF p = event->pos();
	if(m_data->resizeLeft.contains(p))
		this->setCursor(Qt::SizeHorCursor);
	else if(m_data->resizeRight.contains(p))
		this->setCursor(Qt::SizeHorCursor);
	else
		this->setCursor(Qt::SizeAllCursor);
}

void	VipTimeRangeListItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
	if (area()->timeRangesLocked())
	{
		VipCorrectedTip::showText(QCursor::pos(), lockedMessage(), NULL, QRect(), 5000);
		return;
	}

	QPair<double,double> range = m_data->initRange;
	QPair<double,double> previous = range;

	//compute the new time range
	VipCoordinateSystemPtr m = sceneMap();
	double diff = (m->invTransform(event->pos()) - m->invTransform(m_data->pos)).x();
	if(m_data->selection == -1) //left arrow
		range.first += diff;
	else if(m_data->selection == 1) //left arrow
		range.second += diff;
	else if(m_data->selection == 0)
	{
		range.first += diff;
		range.second += diff;
	}

	//m_data->pos = event->pos();

	//compute the transform between old and new range
	QTransform tr;
	double dx = (range.second-range.first)/(previous.second-previous.first);
	double translate = range.first - previous.first*dx;
	tr.translate(translate,translate);
	tr.scale(dx,dx);

	//apply it
	for(int i=0; i < m_data->items.size(); ++i)
	{
		VipTimeRangeItem * it = m_data->items[i];

		VipTimeRange itrange = m_data->initItemRanges[it];

		QPointF times(itrange.first, itrange.second);
		times = tr.map(times);

		qint64 new_left = closestTime( qRound64(times.x()));
		qint64 new_right = closestTime(qRound64(times.y()));

		//block signals to avoid a call to updatDevice() FOR EACH items
		it->blockSignals(true);
		it->setCurrentTimeRange(new_left, new_right);
		it->blockSignals(false);
	}

	Q_EMIT itemsTimeRangeChanged(m_data->items);

	//update the device, this will also update the time scale widget
	this->updateDevice();
	this->computeToolTip();
}

void	VipTimeRangeListItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
	m_data->pos = event->pos();
	m_data->initRange = itemsRange();
	m_data->initItemRanges.clear();
	for (int i = 0; i < m_data->items.size(); ++i)
		m_data->initItemRanges[m_data->items[i]] = QPair<qint64, qint64>(m_data->items[i]->left(), m_data->items[i]->right());

	if(m_data->resizeLeft.contains(m_data->pos))
		m_data->selection = -1;
	else if(m_data->resizeRight.contains(m_data->pos))
		m_data->selection = 1;
	else
		m_data->selection = 0;
}

void	VipTimeRangeListItem::mouseReleaseEvent(QGraphicsSceneMouseEvent * )
{
	m_data->selection = -2;
}

QVariant VipTimeRangeListItem::itemChange(GraphicsItemChange change, const QVariant & value)
{
	if (change == QGraphicsItem::ItemVisibleHasChanged)
	{
		for (int i = 0; i < m_data->items.size(); ++i)
			m_data->items[i]->setVisible(this->isVisible());
	}
	return VipPlotItem::itemChange(change, value);
}

void VipTimeRangeListItem::addItem(VipTimeRangeItem* item)
{
	m_data->items.append(item);
	item->setZValue(this->zValue());
}

void VipTimeRangeListItem::removeItem(VipTimeRangeItem* item)
{
	m_data->items.removeAll(item);
}

void VipTimeRangeListItem::updateDevice()
{
	updateDevice(false);
}

//update the VipIODevice timestamping filter
void VipTimeRangeListItem::updateDevice(bool reload)
{
	bool selected = false;
	for(int i=0; i < m_data->items.size(); ++i)
		if( m_data->items[i]->isSelected())
		{
			selected = true;
			break;
		}


	this->setSelected(selected);
	this->prepareGeometryChange();

	if(m_data->device)
	{
		VipTimeRangeTransforms trs = transforms();
		if(trs.size())
		{
			VipTimestampingFilter filter;
			filter.setTransforms(trs);
			if (m_data->changeTimeRangeFunction)
				m_data->changeTimeRangeFunction(this, filter);
			else
				m_data->device->setTimestampingFilter(filter);
		}
		if (reload)
			m_data->device->reload();
	}
}









class TimeSliderGrip : public VipSliderGrip
{
public:
	TimeSliderGrip(VipProcessingPool * pool , VipAbstractScale * parent)
	:VipSliderGrip(parent),d_pool(pool)
	{
		setValue(0);
		setDisplayToolTipValue(Qt::AlignHCenter | Qt::AlignBottom);
		setToolTipText("<b>Time</b>: #value");
		setToolTipDistance(11);
	}

	void setProcessingPool(VipProcessingPool * pool)
	{
		d_pool = pool;
	}

	VipProcessingPool * processingPool() {return d_pool;}

protected:

	virtual double closestValue(double v)
	{
		if (d_pool)
		{
			qint64 tmp = d_pool->closestTime(v);
			if (tmp != VipInvalidTime)
				return tmp;
			else
				return v;
		}
		else
			return v;
	}

	virtual  bool sceneEventFilter(QGraphicsItem * watched, QEvent * event)
	{
		if(qobject_cast<VipPlotMarker*>(watched->toGraphicsObject()))
		{
			if(event->type() == QEvent::GraphicsSceneMouseMove)
			{
				this->mouseMoveEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
				return true;
			}
			else if(event->type() == QEvent::GraphicsSceneMousePress)
			{
				this->mousePressEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
				return true;
			}
			else if(event->type() == QEvent::GraphicsSceneMouseRelease)
			{
				this->mouseReleaseEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
				return true;
			}
		}

		return false;
	}

private:
	VipProcessingPool * d_pool;
};


class TimeSliderGripNoLimits : public TimeSliderGrip
{
public:
	TimeSliderGripNoLimits(VipProcessingPool * pool , VipAbstractScale * parent)
	:TimeSliderGrip(pool,parent) {}

	virtual double closestValue(double v)
		{
			if(processingPool())
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
	TimeSliderGrip * grip;
	QColor sliderColor;
	QColor sliderFillColor;

	TimeScale(VipProcessingPool* pool, QGraphicsItem* parent = NULL)
		:VipAxisBase(Bottom, parent), lineWidth(4), gripZValue(2000), grip(NULL)
	{
		sliderColor = QColor(0xC9DEFA);
		sliderFillColor = QColor(0xFF5963);
		grip = new TimeSliderGrip(pool,this);
		grip->setImage(vipPixmap("handle.png").toImage().scaled(QSize(16,16),Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
		grip->setHandleDistance(-2);
		grip->setZValue(2000);
		//VipAdaptativeGradient grad(QGradientStops() << QGradientStop(0, QColor(0x97B8E0)) << QGradientStop(1, QColor(0xC9DEFA)), Qt::Vertical);
		lineBoxStyle.setBorderRadius(1);
		//lineBoxStyle.setAdaptativeGradientBrush(grad);
		lineBoxStyle.setBorderPen(QPen(Qt::NoPen));
		lineBoxStyle.setRoundedCorners(Vip::AllCorners);
		scaleDraw()->setSpacing(6);
		setCanvasProximity(2);
		setExpandToCorners(true);
		this->setTitle(QString("Time"));
		this->enableDrawTitle(false);
	}

	QRectF sliderRect(  ) const
	{
	    QRectF cr = boundingRectNoCorners();
	    double margin = this->margin();
		cr.setLeft( this->constScaleDraw()->pos().x() );
		cr.setWidth( this->constScaleDraw()->length() +1 );
		cr.setTop( cr.top() + margin + this->spacing() //+ 3
);
		cr.setHeight( lineWidth );
	    return cr;
	}
	//NEWPLOT
	virtual void draw ( QPainter * painter, QWidget * widget  )
	{
		VipAxisBase::draw(painter,widget);

		double half_width = lineWidth/2.0;
		QRectF rect = sliderRect( ) ;
		QRectF line_rect = QRectF(QPointF(rect.left(),rect.center().y()-half_width) , QPointF(rect.right(),rect.center().y()+half_width));

		VipBoxStyle st = lineBoxStyle;
		st.setBackgroundBrush(sliderColor);
		st.computeRect(line_rect);
		st.draw(painter);

		//draw filled area
		line_rect.setRight(this->position(grip->value()).x());
		st.setBackgroundBrush(sliderFillColor);
		st.computeRect(line_rect);
		st.draw(painter);

		//draw selection time range
		if (VipPlayerArea *p = qobject_cast<VipPlayerArea*>(parentItem()->toGraphicsObject())) {
			VipTimeRange r = p->selectionTimeRange();
			if (r.first != r.second ) {
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

	virtual void	mousePressEvent ( QGraphicsSceneMouseEvent * event )
	{
		if(grip)
		{
			double time = this->value(event->pos());
			grip->setValue(time);
		}
	}

	virtual void computeScaleDiv()
	{
		if (!isAutoScale())
			return;

		const QList<VipPlotItem*> & items = this->plotItems();
		if (items.isEmpty())
			return;

		//compute first boundaries

		int i = 0;
		VipInterval bounds;

		for (; i < items.size(); ++i)
		{
			if(VipTimeRangeListItem * it = qobject_cast<VipTimeRangeListItem*>(items[i]))
				if ( !(it->states() & VipTimeRangeListItem::HiddenForPlayer))
				{
					bounds = VipInterval(it->device()->firstTime(), it->device()->lastTime());
					if (bounds.isValid()) {
						++i;
						break;
					}
				}
		}

		//compute the union boundaries

		for (; i < items.size(); ++i)
		{
			if (VipTimeRangeListItem * it = qobject_cast<VipTimeRangeListItem*>(items[i]))
				if (!(it->states() & VipTimeRangeListItem::HiddenForPlayer))
				{
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
			if (VipPlayerArea * a = qobject_cast<VipPlayerArea*>(this->area())) {
				//get the parent VipPlayWidget to update its geometry
				QWidget * p = a->view();
				VipPlayWidget * w = NULL;
				while (p) {
					if (w = qobject_cast<VipPlayWidget*>(p))
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
	PrivateData():visible(true), timeRangesLocked(true), highlightedTime ( VipInvalidTime), boundTime(VipInvalidTime), highlightedItem(NULL), adjustFactor(0), dirtyProcessingPool(false){}

	QList<VipTimeRangeListItem*> items;
	QPointer<VipProcessingPool> pool;
	bool visible;
	bool timeRangesLocked;
	TimeScale * timeScale;

	qint64 highlightedTime;
	qint64 boundTime;
	VipPlotItem * highlightedItem;

	VipPlotMarker * highlightedMarker;
	VipPlotMarker * timeMarker;

	VipPlotMarker * limit1Marker;
	VipPlotMarker * limit2Marker;

	TimeSliderGrip * timeSliderGrip;
	TimeSliderGrip * limit1Grip;
	TimeSliderGrip * limit2Grip;

	VipTimeRange selectionTimeRange;
	QBrush timeRangeSelectionBrush ;

	//VipColorPalette palette;
	int adjustFactor;

	bool dirtyProcessingPool;
};

VipPlayerArea::VipPlayerArea( )
:VipPlotArea2D()
{
	m_data = new PrivateData();
	m_data->timeRangeSelectionBrush = QBrush(QColor(0x8290FC));
	m_data->selectionTimeRange = VipInvalidTimeRange;
	//m_data->palette = VipColorPalette(VipLinearColorMap::ColorPaletteRandom);//VipColorPalette(VipLinearColorMap::ColorPaletteStandard);

	m_data->timeScale = new TimeScale(NULL);
	this->addScale(m_data->timeScale,true);
	this->bottomAxis()->setVisible(false);
	m_data->timeScale->setZValue(2000);


	m_data->highlightedMarker = new VipPlotMarker();
	m_data->highlightedMarker->setLineStyle(VipPlotMarker::VLine);
	m_data->highlightedMarker->setLinePen(QPen(QBrush(Qt::red),1,Qt::DashLine));
	m_data->highlightedMarker->setVisible(false);
	m_data->highlightedMarker->setAxes(m_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
	m_data->highlightedMarker->setZValue(1000);
	m_data->highlightedMarker->setFlag(VipPlotItem::ItemIsSelectable,false);
	m_data->highlightedMarker->setIgnoreStyleSheet(true);

	m_data->limit1Marker = new VipPlotMarker();
	m_data->limit1Marker->setLineStyle(VipPlotMarker::VLine);
	m_data->limit1Marker->setLinePen(QPen(QBrush(QColor(0xFF5963)),1));
	m_data->limit1Marker->setAxes(m_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
	m_data->limit1Marker->setZValue(1000);
	m_data->limit1Marker->setFlag(VipPlotItem::ItemIsSelectable,false);
	m_data->limit1Marker->setIgnoreStyleSheet(true);

	m_data->limit2Marker = new VipPlotMarker();
	m_data->limit2Marker->setLineStyle(VipPlotMarker::VLine);
	m_data->limit2Marker->setLinePen(QPen(QBrush(QColor(0xFF5963)),1));
	m_data->limit2Marker->setAxes(m_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
	m_data->limit2Marker->setZValue(1000);
	m_data->limit2Marker->setFlag(VipPlotItem::ItemIsSelectable,false);
	m_data->limit2Marker->setIgnoreStyleSheet(true);

	m_data->timeMarker = new VipPlotMarker();
	m_data->timeMarker->setLineStyle(VipPlotMarker::VLine);
	m_data->timeMarker->setLinePen(QPen(QBrush(QColor(0xFF5963)),1));
	m_data->timeMarker->setAxes(m_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
	m_data->timeMarker->setZValue(1000);
	m_data->timeMarker->setFlag(VipPlotItem::ItemIsSelectable,false);
	m_data->timeMarker->setItemAttribute(VipPlotItem::AutoScale, false);
	m_data->timeMarker->setRenderHints(QPainter::RenderHints());
	m_data->timeMarker->setIgnoreStyleSheet(true);

	m_data->limit1Grip = new TimeSliderGripNoLimits(NULL,m_data->timeScale);
	m_data->limit1Grip->setHandleDistance(-10);
	m_data->limit1Grip->setImage(vipPixmap("slider_limit.png").toImage().transformed(QTransform().rotate(90)));
	m_data->limit1Grip->setGripAlwaysInsideScale(false);

	m_data->limit2Grip = new TimeSliderGripNoLimits(NULL,m_data->timeScale);
	m_data->limit2Grip->setHandleDistance(-10);
	m_data->limit2Grip->setImage(vipPixmap("slider_limit.png").toImage().transformed(QTransform().rotate(90)));
	m_data->limit2Grip->setGripAlwaysInsideScale(false);

	m_data->timeSliderGrip = m_data->timeScale->grip;
	m_data->timeSliderGrip->setZValue(2000); //just above time marker
	//m_data->timeSliderGrip->setHandleDistance(-10);
	m_data->timeSliderGrip->setGripAlwaysInsideScale(false);
	//tile grip always above limits grips
	//m_data->timeSliderGrip->setZValue(qMax(m_data->limit1Grip->zValue(),m_data->limit2Grip->zValue())+1);

	//m_data->timeScale->setMargin(15);
	//m_data->timeScale->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	//m_data->timeScale->scaleDraw()->setSpacing(20);

	//setMouseTracking(true);
	this->setMousePanning(Qt::RightButton);
	//this->setMouseItemSelection(Qt::LeftButton);
	this->setMouseWheelZoom(true);
	this->setZoomEnabled(this->leftAxis(), false);
	this->setAutoScale(true);
	this->setDrawSelectionOrder(NULL);

	connect(this,SIGNAL(mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton)),this,SLOT(mouseMoved(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(this,SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)),this,SLOT(mouseReleased(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(m_data->timeSliderGrip,SIGNAL(valueChanged(double)),this,SLOT(setTime(double)));
	connect(m_data->limit1Grip,SIGNAL(valueChanged(double)),this,SLOT(setLimit1(double)));
	connect(m_data->limit2Grip,SIGNAL(valueChanged(double)),this,SLOT(setLimit2(double)));

	this->grid()->setVisible(false);//setFlag(VipPlotItem::ItemIsSelectable,false);
	this->leftAxis()->setVisible(false);
	this->rightAxis()->setVisible(false);
	this->topAxis()->setVisible(false);
	this->titleAxis()->setVisible(false);

	this->leftAxis()->setUseBorderDistHintForLayout(true);
	this->rightAxis()->setUseBorderDistHintForLayout(true);
	this->topAxis()->setUseBorderDistHintForLayout(true);
	this->bottomAxis()->setUseBorderDistHintForLayout(true);

	VipToolTip * tip = new VipToolTip();
	tip->setRegionPositions( Vip::RegionPositions(Vip::XInside ));
	tip->setAlignment(Qt::AlignTop | Qt::AlignRight);
	this->setPlotToolTip(tip);

}

VipPlayerArea::~VipPlayerArea()
{
	delete m_data;
}

bool VipPlayerArea::timeRangesLocked() const
{
	return m_data->timeRangesLocked;
}

void VipPlayerArea::setTimeRangesLocked(bool locked)
{
	m_data->timeRangesLocked = locked;
}

void VipPlayerArea::setTimeRangeVisible(bool visible)
{
	m_data->visible  = visible;
	for(int i=0; i < m_data->items.size(); ++i)
	{
		if (visible)
			m_data->items[i]->setStates(m_data->items[i]->states() & (~VipTimeRangeListItem::HiddenForHideTimeRanges));
		else
			m_data->items[i]->setStates(m_data->items[i]->states() | VipTimeRangeListItem::HiddenForHideTimeRanges);

		bool item_visible = m_data->items[i]->states() == VipTimeRangeListItem::Visible;//visible && m_data->items[i]->device()->isEnabled();
		m_data->items[i]->setVisible(item_visible);
	}

	m_data->timeMarker->setVisible(visible);
	if(visible && (m_data->pool->modes() & VipProcessingPool::UseTimeLimits))
	{
		m_data->limit1Marker->setVisible(visible);
		m_data->limit2Marker->setVisible(visible);
	}
}

bool VipPlayerArea::timeRangeVisible() const
{
	return m_data->visible;
}

bool VipPlayerArea::limitsEnabled() const
{
	return m_data->pool->testMode(VipProcessingPool::UseTimeLimits);
}

int VipPlayerArea::visibleItemCount() const
{
	int c = 0;
	for (int i = 0; i < m_data->items.size(); ++i)
		if (!(m_data->items[i]->states() & VipTimeRangeListItem::HiddenForPlayer))
			++c;
	return c;
}


void VipPlayerArea::setSelectionTimeRange(const VipTimeRange& r)
{
	m_data->selectionTimeRange = r;
	update();
}
VipTimeRange VipPlayerArea::selectionTimeRange() const
{
	return m_data->selectionTimeRange;
}

void VipPlayerArea::setTimeRangeSelectionBrush(const QBrush& b)
{
	m_data->timeRangeSelectionBrush = b;
	update();
}
QBrush VipPlayerArea::timeRangeSelectionBrush() const
{
	return m_data->timeRangeSelectionBrush;
}

void	VipPlayerArea::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	VipPlotArea2D::paint(painter, option, widget);

	//draw the selected time ranges
	//painter->setPen(Qt::NoPen);
	// painter->setBrush(m_data->timeRangeSelectionBrush);
//
	// VipPlotCanvas* c = this->canvas();
//
	// qint64 f = m_data->selectionTimeRange.first;
	// qint64 s = m_data->selectionTimeRange.second;
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
	m_data->limit1Marker->setRawData(QPointF(t,0));
	m_data->limit1Grip->blockSignals(true);
	m_data->limit1Grip->setValue(t);
	m_data->limit1Grip->blockSignals(false);
	if(sender() != processingPool())
		m_data->pool->setStopBeginTime(m_data->pool->closestTimeNoLimits(t));
}

void VipPlayerArea::setLimit2(double t)
{
	m_data->limit2Marker->setRawData(QPointF(t,0));
	m_data->limit2Grip->blockSignals(true);
	m_data->limit2Grip->setValue(t);
	m_data->limit2Grip->blockSignals(false);
	if(sender() != processingPool())
		m_data->pool->setStopEndTime(m_data->pool->closestTimeNoLimits(t));
}

void VipPlayerArea::setLimitsEnable(bool enable)
{
	m_data->limit1Marker->setItemAttribute(VipPlotItem::AutoScale,enable);
	m_data->limit1Marker->setVisible(enable && m_data->visible);
	m_data->limit1Grip->setVisible(enable);
	m_data->limit2Marker->setItemAttribute(VipPlotItem::AutoScale,enable);
	m_data->limit2Marker->setVisible(enable && m_data->visible);
	m_data->limit2Grip->setVisible(enable);
	m_data->pool->setMode(VipProcessingPool::UseTimeLimits,enable);
	if(enable)
	{
		qint64 first = m_data->pool->stopBeginTime();
		qint64 last = m_data->pool->stopEndTime();
		setLimit1(first);
		setLimit2(last);

	}
}

void VipPlayerArea::setTime64(qint64 t )
{
	setTime(t);
}

void VipPlayerArea::setTime(double t )
{
	//set the selection time range
	if (qApp->keyboardModifiers() & Qt::SHIFT) {
		if (m_data->selectionTimeRange == VipInvalidTimeRange) {
			m_data->selectionTimeRange = VipTimeRange(t, t);
		}
		else {
			m_data->selectionTimeRange.second = t;
		}
	}
	else {
		m_data->selectionTimeRange = VipTimeRange(t, t);
	}

	if(t < m_data->pool->firstTime() )
		t = m_data->pool->firstTime();
	else if(t > m_data->pool->lastTime() )
		t = m_data->pool->lastTime();

	m_data->timeMarker->setRawData(QPointF(t,0));
	m_data->timeSliderGrip->blockSignals(true);
	m_data->timeSliderGrip->setValue(t);
	m_data->timeSliderGrip->blockSignals(false);
	if (sender() != processingPool())
	{
		m_data->pool->read(t);
		VipProcessingObjectList objects = m_data->pool->leafs(false);
		objects.wait();
	}

}

VipTimeRangeListItem * VipPlayerArea::findItem(VipIODevice * device)const
{
	for (int i = 0; i < m_data->items.size(); ++i)
		if (m_data->items[i]->device() == device)
			return m_data->items[i];
	return NULL;
}

void VipPlayerArea::updateAreaDevices()
{
	updateArea(false);
}

void VipPlayerArea::updateArea(bool check_item_visibility)
{
	if (!m_data->pool)
	{
		for (int i = 0; i < m_data->items.size(); ++i)
		{
			//m_data->items[i]->device()->resetTimestampingFilter();
			delete m_data->items[i];
		}
		m_data->items.clear();
	}
	else
	{
		VipProcessingObjectList lst = m_data->pool->processing("VipIODevice");



		int visibleItemCount = 0;

		QList<VipIODevice*> devices;
		for (int i = 0; i < lst.size(); ++i)
		{
			VipIODevice * device = qobject_cast<VipIODevice*>(lst[i]);

			if (device && (device->openMode() & VipIODevice::ReadOnly) && device->deviceType() == VipIODevice::Temporal && (device->size() != 1 || device->property("_vip_showTimeLine").toInt() == 1))
			{
				devices.append(device);
				//retrieve all display object linked to this device
				QList<VipDisplayObject*> displays = vipListCast<VipDisplayObject*>(device->allSinks());
				//do not show this device time range if the display widgets are hidden (only for non new device)

				VipTimeRangeListItem * item = findItem(device);

				bool hidden = displays.size() > 0 && item && check_item_visibility;
				if ( hidden)
					for (int d = 0; d < displays.size(); ++d)
					{
							if (displays[d]->isVisible())
							{
								hidden = false;
								break;
							}
					}


				//if the display is hidden, remove it from time computations (time window, next, previous and closest time) from the processing pool
				if (check_item_visibility)
				{
					if (hidden)
					{
						//only disable temporal devics that contribute to the processing pool time limits
						device->setEnabled(false);
					}
					else
					{
						//enable the device, read the data at processing pool time if needed
						bool need_reload = !device->isEnabled();
						device->setEnabled(true);
						if (need_reload)
							device->read(m_data->pool->time());
					}
				}

				if (!item)
				{
					item = new VipTimeRangeListItem(this);
					item->setAxes(m_data->timeScale, this->leftAxis(), VipCoordinateSystem::Cartesian);
					item->setDevice(device);
					//item->setColor(m_data->palette.color(findBestColor(item)));
					m_data->items << item;
				}

				item->setHeights(visibleItemCount + ITEM_START_HEIGHT, visibleItemCount + ITEM_END_HEIGHT);

				if (device->isEnabled())
				{
					++visibleItemCount;
					item->setStates(item->states() & (~VipTimeRangeListItem::HiddenForPlayer));
				}
				else
					item->setStates(item->states() | VipTimeRangeListItem::HiddenForPlayer);

			}
		}

		//remove items with invalid devices
		for (int i = 0; i < m_data->items.size(); ++i)
			if (devices.indexOf(m_data->items[i]->device()) < 0)
			{
				delete m_data->items[i];
				m_data->items.removeAt(i);
				--i;
			}

		//update time slider
		qint64 time = m_data->pool->time();
		if (time < m_data->pool->firstTime())
			time = m_data->pool->firstTime();
		else if (time > m_data->pool->lastTime())
			time = m_data->pool->lastTime();

		m_data->timeMarker->setRawData(QPointF(time, 0));
		m_data->timeSliderGrip->blockSignals(true);
		m_data->timeSliderGrip->setValue(time);
		m_data->timeSliderGrip->blockSignals(false);

		//update items visibility
		setTimeRangeVisible(m_data->visible);

	}

	this->computeStartDate();
	this->setAutoScale(true);
	Q_EMIT devicesChanged();
}

void VipPlayerArea::setProcessingPool(VipProcessingPool * pool)
{
	if(pool != m_data->pool)
	{
		if(m_data->pool)
		{
			disconnect(m_data->pool,SIGNAL(objectAdded(QObject*)),this,SLOT(updateAreaDevices()));
			disconnect(m_data->pool,SIGNAL(objectRemoved(QObject*)),this,SLOT(updateAreaDevices()));
			disconnect(m_data->pool, SIGNAL(timestampingChanged()), this, SLOT(addMissingDevices()));
			disconnect(m_data->pool,SIGNAL(timeChanged(qint64)),this,SLOT(setTime(qint64)));

			for(int i=0; i < m_data->items.size(); ++i)
			{
				//m_data->items[i]->device()->resetTimestampingFilter();
				delete m_data->items[i];
			}
		}

		m_data->items.clear();
		m_data->pool = pool;

		if(pool)
		{
			m_data->timeSliderGrip->setProcessingPool(pool);
			m_data->limit1Grip->setProcessingPool(pool);
			m_data->limit2Grip->setProcessingPool(pool);

			connect(m_data->pool,SIGNAL(objectAdded(QObject*)),this,SLOT(updateAreaDevices()));//,Qt::QueuedConnection);
			connect(m_data->pool,SIGNAL(objectRemoved(QObject*)),this,SLOT(updateAreaDevices()));//,Qt::DirectConnection);
			connect(m_data->pool,SIGNAL(timeChanged(qint64)),this,SLOT(setTime64(qint64)),Qt::QueuedConnection);
			connect(m_data->pool, SIGNAL(timestampingChanged()), this, SLOT(addMissingDevices()), Qt::QueuedConnection);

			//VipProcessingObjectList lst = pool->processing("VipIODevice");
//
			// //update time slider
			// qint64 time = m_data->pool->time();
			// if (time < m_data->pool->firstTime())
			// time = m_data->pool->firstTime();
			// else if (time > m_data->pool->lastTime())
			// time = m_data->pool->lastTime();
//
			// m_data->timeMarker->setRawData(QPointF(time, 0));
			// m_data->timeSliderGrip->blockSignals(true);
			// m_data->timeSliderGrip->setValue(time);
			// m_data->timeSliderGrip->blockSignals(false);
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
			// item->setAxes(m_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
			// item->setDevice(device);
			// item->setHeights(count + ITEM_START_HEIGHT, count+ITEM_END_HEIGHT);
			// item->setColor(m_data->palette.color(m_data->items.size()));
			// ++count;
//
			// m_data->items << item;
//
			// }
			// }
//
			// //update items visibility
			// setTimeRangeVisible(m_data->visible);
		}

		updateArea(true);

		Q_EMIT processingPoolChanged(m_data->pool);
	}
}

VipProcessingPool * VipPlayerArea::processingPool() const
{
	return m_data->pool;
}

VipSliderGrip * VipPlayerArea::timeSliderGrip() const
{
	return const_cast<TimeSliderGrip*>(m_data->timeSliderGrip);
}

VipPlotMarker * VipPlayerArea::timeMarker() const
{
	return const_cast<VipPlotMarker*>(m_data->timeMarker);
}

VipSliderGrip * VipPlayerArea::limit1SliderGrip() const
{
	return const_cast<TimeSliderGrip*>(m_data->limit1Grip);
}

VipPlotMarker * VipPlayerArea::limit1Marker() const
{
	return const_cast<VipPlotMarker*>(m_data->limit1Marker);
}

VipSliderGrip * VipPlayerArea::limit2SliderGrip() const
{
	return const_cast<TimeSliderGrip*>(m_data->limit2Grip);
}

VipPlotMarker * VipPlayerArea::limit2Marker() const
{
	return const_cast<VipPlotMarker*>(m_data->limit2Marker);
}

VipAxisBase * VipPlayerArea::timeScale() const
{
	return const_cast<TimeScale*>(m_data->timeScale);
}


QList<qint64> VipPlayerArea::stops(const QList<VipTimeRangeItem *> & excluded) const
{
	QList<qint64> res;

	for(int i=0; i <m_data->items.size(); ++i)
	{
		const QList<VipTimeRangeItem*> items = m_data->items[i]->items();
		for(int j=0; j< items.size(); ++j)
		{
			if(excluded.indexOf(items[j]) < 0)
			{
				VipTimeRange range = items[j]->currentTimeRange();
				res << range.first;
				res << range.second;
			}
		}
	}

	return res;
}

QList<VipTimeRangeListItem *> VipPlayerArea::timeRangeListItems() const
{
	QList<VipPlotItem*> items = this->plotItems();
	QList<VipTimeRangeListItem *> res;
	for(int i=0; i < items.size(); ++i)
		if(VipTimeRangeListItem * item = qobject_cast<VipTimeRangeListItem*>(items[i]))
			res << item;
	return res;
}

void VipPlayerArea::timeRangeItems(QList<VipTimeRangeItem *> & selected, QList<VipTimeRangeItem *> & not_selected) const
{
	QList<VipPlotItem*> items = this->plotItems();
	for(int i=0; i < items.size(); ++i)
	{
		if(VipTimeRangeItem * item = qobject_cast<VipTimeRangeItem*>(items[i]))
		{
			if(item->isSelected())
				selected << item;
			else
				not_selected << item;
		}
	}
}

void VipPlayerArea::timeRangeListItems(QList<VipTimeRangeListItem *> & selected, QList<VipTimeRangeListItem *> & not_selected) const
{
	QList<VipPlotItem*> items = this->plotItems();
	for(int i=0; i < items.size(); ++i)
	{
		if(VipTimeRangeListItem * item = qobject_cast<VipTimeRangeListItem*>(items[i]))
		{
			if(item->isSelected())
				selected << item;
			else
				not_selected << item;
		}
	}
}

void VipPlayerArea::addMissingDevices()
{
	//first, compute the list of present devices in the player area
	QSet<VipIODevice*> devices;
	for (int i = 0; i < m_data->items.size(); ++i)
		devices.insert(m_data->items[i]->device());

	//now, for each devices in the processing pool, add the temporal devices with a size != 1 that are not already present in the player area.
	//this case might happen when a temporal device changes its timestamping and size.

	if (this->processingPool())
	{
		QList<VipIODevice*> pool_devices = this->processingPool()->findChildren<VipIODevice*>();
		for (int i = 0; i < pool_devices.size(); ++i)
		{
			VipIODevice * dev = pool_devices[i];
			if (!devices.contains(dev) && dev->isOpen() && dev->supportedModes() == VipIODevice::ReadOnly && dev->deviceType() == VipIODevice::Temporal && (dev->size() != 1 || dev->property("_vip_showTimeLine").toInt()==1))
			{
				//deviceAdded(dev);
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
// for (int i = 0; i < m_data->items.size(); ++i)
//	if (m_data->items[i]->device() == device)
//		return;
//
// int count = m_data->items.size();
// VipTimeRangeListItem * item = new VipTimeRangeListItem();
// item->setAxes(m_data->timeScale,this->leftAxis(),VipCoordinateSystem::Cartesian);
// item->setDevice(device);
// item->setHeights(count + ITEM_START_HEIGHT, count+ITEM_END_HEIGHT);
// item->setColor(m_data->palette.color(m_data->items.size()));
// m_data->items << item;
//
// //update items visibility
// setTimeRangeVisible(m_data->visible);
//
// this->setAutoScale(true);
// Q_EMIT devicesChanged();
// }
// }
//
// void VipPlayerArea::deviceRemoved(QObject* obj)
// {
// //remove all items with a NULL device pointer or equal to obj
// for(int i=0; i < m_data->items.size(); ++i)
// {
// VipIODevice * device = m_data->items[i]->device();
// if(! device ||  device == obj)
// {
//	delete m_data->items[i];// ->deleteLater();
//	m_data->items.removeAt(i);
//	--i;
// }
// }
//
// //update items visibility
// setTimeRangeVisible(m_data->visible);
// Q_EMIT devicesChanged();
// }

void VipPlayerArea::moveToForeground()
{
	QList<VipTimeRangeItem *> selected,not_selected;
	timeRangeItems(selected,not_selected);

	//set the selected to 1000, set to 999 the others> 1000
	for(int i=0; i < selected.size(); ++i)
		selected[i]->setZValue(1000);

	for(int i=0; i < not_selected.size(); ++i)
		if(not_selected[i]->zValue() >= 1000)
			not_selected[i]->setZValue(999);
}

void VipPlayerArea::moveToBackground()
{
	QList<VipTimeRangeItem *> selected,not_selected;
	timeRangeItems(selected,not_selected);

	//set the selected to 100, set to 101 the others < 100
	for(int i=0; i < selected.size(); ++i)
		selected[i]->setZValue(100);

	for(int i=0; i < not_selected.size(); ++i)
		if(not_selected[i]->zValue() <= 100)
			not_selected[i]->setZValue(101);
}

void VipPlayerArea::splitSelection()
{
	//not implemented yet
}

void VipPlayerArea::reverseSelection()
{
	QList<VipTimeRangeItem *> selected,not_selected;
	timeRangeItems(selected,not_selected);
	for(int i=0; i < selected.size(); ++i)
		selected[i]->setReverse(!selected[i]->reverse());
}

void VipPlayerArea::resetSelection()
{
	QList<VipTimeRangeListItem *> selected,not_selected;
	timeRangeListItems(selected,not_selected);
	for(int i=0; i < selected.size(); ++i)
		selected[i]->reset();
}

void VipPlayerArea::resetAllTimeRanges()
{
	QList<VipTimeRangeListItem *> selected,not_selected;
	timeRangeListItems(selected,not_selected);
	for(int i=0; i < selected.size(); ++i)
		selected[i]->reset();
	for(int i=0; i < not_selected.size(); ++i)
		not_selected[i]->reset();

}

void VipPlayerArea::alignToZero(bool enable)
{
	QList<VipTimeRangeListItem *> all;
	timeRangeListItems(all,all);

	if (!enable) {
		//reset filter for all
		for (int i = 0; i < all.size(); ++i) {
			if (VipIODevice * device = all[i]->device()) {
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

	for(int i=0; i < all.size(); ++i)
	{
		VipIODevice * device = all[i]->device();
		if(device)
		{
			qint64 first_time = VipInvalidTime; //find lowest time

			if(device->timestampingFilter().isEmpty())
			{
				first_time = device->firstTime();
				//create the timestamping filter
				VipTimeRangeTransforms trs;
				VipTimeRangeList window = device->timeWindow();
				for(const VipTimeRange & win : window)
					trs[win] = VipTimeRange(win.first - first_time, win.second - first_time);
				VipTimestampingFilter filter;
				filter.setTransforms(trs);
				device->setTimestampingFilter(filter);
				device->setProperty("_vip_timestampingFilter", QVariant::fromValue(VipTimestampingFilter()));
			}
			else
			{
				//use the already existing transforms and translate them
				first_time = vipBounds(device->timestampingFilter().outputTimeRangeList()).first;
				if (first_time != 0) {
					VipTimestampingFilter filter = device->timestampingFilter();
					const VipTimestampingFilter saved = filter;

					VipTimeRangeTransforms trs = filter.transforms();
					for (VipTimeRangeTransforms::iterator it = trs.begin(); it != trs.end(); ++it)
					{
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
	//For date time x axis (since epoch), compute the start date of the union of all plot items.
	//Then, set this date to the bottom and top VipValueToTime.
	//This will prevent the start date (displayed at the bottom left corner) to change
	//when zooming or mouse panning.

	VipValueToText * v = this->timeScale()->scaleDraw()->valueToText();
	VipValueToTime * vb = static_cast<VipValueToTime*>(this->timeScale()->scaleDraw()->valueToText());

	if (v->valueToTextType() == VipValueToText::ValueToTime &&
		vb->type % 2 &&
		vb->displayType != VipValueToTime::AbsoluteDateTime)
	{
		vb->fixedStartValue = true;
		vb->startValue = this->timeScale()->itemsInterval().minValue();
	}
	else
	{
		if (v->valueToTextType() == VipValueToText::ValueToTime)
		{
			vb->fixedStartValue = false;
			vb->startValue = this->timeScale()->scaleDiv().bounds().minValue();
		}
	}
}

void VipPlayerArea::updateProcessingPool()
{
	m_data->dirtyProcessingPool = false;
	updateArea(true);
}

void VipPlayerArea::defferedUpdateProcessingPool()
{
	if (!m_data->dirtyProcessingPool)
	{
		m_data->dirtyProcessingPool = true;
		QMetaObject::invokeMethod(this, "updateProcessingPool", Qt::QueuedConnection);
	}
}

void VipPlayerArea::mouseReleased(VipPlotItem* item, VipPlotItem::MouseButton button)
{
	if (item) {
		m_data->selectionTimeRange = VipInvalidTimeRange;
	}

	if(button == VipPlotItem::LeftButton)
	{
		if(m_data->highlightedTime != VipInvalidTime)
		{
			//while moving an item, one of the item boundaries highlighted a time.
			//now we need to translate the item to fit the highlighted time.

			//compute the translation
			QTransform tr;
			tr.translate(double(m_data->highlightedTime - m_data->boundTime),0);

			//apply the transform to all VipTimeRangeItem
			if( VipTimeRangeListItem * t_litem = qobject_cast<VipTimeRangeListItem*>(m_data->highlightedItem))
			{
				QList<VipTimeRangeItem *> items = t_litem->items();
				for(int i=0; i < items.size(); ++i)
					items[i]->applyTransform(tr);
			}
			else if( VipTimeRangeItem * t_item = qobject_cast<VipTimeRangeItem*>(m_data->highlightedItem))
			{
				t_item->applyTransform(tr);
			}
		}

		//invalidate highlightedTime
		m_data->highlightedTime = VipInvalidTime;
		m_data->boundTime = VipInvalidTime;
		m_data->highlightedItem = NULL;
		m_data->highlightedMarker->setVisible(false);
	}
	else if(button == VipPlotItem::RightButton)
	{
		//display a contextual menu
		QMenu menu;
		connect(menu.addAction(vipIcon("foreground.png"),"Move selection to foreground"),SIGNAL(triggered(bool)),this,SLOT(moveToForeground()));
		connect(menu.addAction(vipIcon("background.png"),"Move selection to background"),SIGNAL(triggered(bool)),this,SLOT(moveToBackground()));
		menu.addSeparator();
		//connect(menu.addAction("Split time range on current time"),SIGNAL(triggered(bool)),this,SLOT(splitSelection()));
		connect(menu.addAction("Reverse time range"),SIGNAL(triggered(bool)),this,SLOT(reverseSelection()));
		menu.addSeparator();
		connect(menu.addAction("Reset selected time range"),SIGNAL(triggered(bool)),this,SLOT(resetSelection()));

		menu.exec(QCursor::pos());
	}
}

void VipPlayerArea::mouseMoved(VipPlotItem* item, VipPlotItem::MouseButton )
{

	//all the possible time stops
	QList<qint64> all_stops;
	//the stops of the current item we are moving
	QList<qint64> item_stops;
	//invalidate highlightedTime
	m_data->highlightedTime = VipInvalidTime;

	//if the whole VipTimeRangeListItem is moved
	if( VipTimeRangeListItem * t_litem = qobject_cast<VipTimeRangeListItem*>(item))
	{
		all_stops = stops(t_litem->items());
		item_stops = t_litem->stops();

		m_data->highlightedItem = t_litem;
	}
	//if only a VipTimeRangeItem is moved
	else if( VipTimeRangeItem * t_item = qobject_cast<VipTimeRangeItem*>(item))
	{
		VipTimeRange range = t_item->currentTimeRange();
		item_stops << range.first << range.second;
		all_stops = stops(QList<VipTimeRangeItem *>() << t_item);

		m_data->highlightedItem = t_item;
	}

	//now, find the closest possible stop with a vipDistance under 5px to the item stops
	qint64 closest = VipInvalidTime;
	qint64 closest_item = VipInvalidTime;
	qint64 dist = VipMaxTime;

	//closest stop
	for(int i=0; i < item_stops.size(); ++i)
	{
		qint64 item_stop = item_stops[i];
		for(int j=0; j < all_stops.size(); ++j)
		{
			qint64 tmp_dist = qAbs(item_stop - all_stops[j]);
			if(tmp_dist < dist)
			{
				dist = tmp_dist;
				closest = all_stops[j];
				closest_item = item_stop;
			}
		}
	}

	//check the vipDistance in pixels
	if(closest != VipInvalidTime)
	{
		double dist_pixel = qAbs(m_data->timeScale->position(closest).x() - m_data->timeScale->position(closest_item).x());
		if(dist_pixel < 5)
		{
			m_data->highlightedTime = closest;
			m_data->boundTime = closest_item;
			m_data->highlightedMarker->setVisible(true);
			m_data->highlightedMarker->setRawData(QPointF(closest,0));
			return;
		}
	}

	m_data->highlightedMarker->setVisible(false);
}

/*int VipPlayerArea::findBestColor(VipTimeRangeListItem* item)
{
	if (m_data->items.indexOf(item) >= 0)
		return 0;

	QMap<int, int> colorIndexes;
	for (int i = 0; i < m_data->items.size(); ++i)
	{
		int index = m_data->items[i]->property("color_index").toInt();
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
	item->setProperty("color_index", m_data->items.size());
	return m_data->items.size();
}*/





static qint64 year2000 = QDateTime::fromString("2000", "yyyy").toMSecsSinceEpoch() * 1000000;

static VipValueToTime::TimeType findBestTimeUnit(VipPlayWidget * w)
{
	VipTimeRange r = w->processingPool() ? w->processingPool()->timeLimits() : VipTimeRange(0,0);
	//VipValueToTime::TimeType current = w->timeType();

	if (r.first > year2000)
	{
		//if the start time is above nano seconds since year 2000, we can safely consider that this is a date
		//only modify the time type if we are not in (unit) since epoch
		//if (current % 2 == 0)
		{
			//get the scale time range and deduce the unit from it
			double range = r.second - r.first;//div.range();

			VipValueToTime::TimeType time = VipValueToTime::NanoSecondsSE;

			if (range > 1000000000) //above 1s
				time = VipValueToTime::SecondsSE;
			else if (range > 1000000) //above 1ms
				time = VipValueToTime::MilliSecondsSE;
			else if (range > 1000) //above 1micros
				time = VipValueToTime::MicroSecondsSE;

			return time;
		}
	}
	else
	{
		//if (current % 2 == 1)
		{
			//get the scale time range and deduce the unit from it
			double range = r.second - r.first;//div.range();
			VipValueToTime::TimeType time = VipValueToTime::NanoSeconds;

			if (range > 1000000000) //above 1s
				time = VipValueToTime::Seconds;
			else if (range > 1000000) //above 1ms
				time = VipValueToTime::MilliSeconds;
			else if (range > 1000) //above 1micros
				time = VipValueToTime::MicroSeconds;

			return time;
		}
	}
	VIP_UNREACHABLE();
	//return current;
}




class VipPlayWidget::PrivateData
{
public:

//playing tool bar
	QToolBar * playToolBar;
	QAction * first;
	QAction * previous;
	QAction * playForward;
	QAction * playBackward;
	QAction * next;
	QAction * last;
	QAction * marks;
	QAction * repeat;
	QAction * maxSpeed;
	QAction * speed;
	QAction * lock;
	VipDoubleSliderWidget * speedWidget;

	//time/number
	QLabel * time;
	QLineEdit * timeEdit;
	QSpinBox * frameEdit;
	VipValueToTimeButton *timeUnit;

	//modify timestamping
	QAction * autoScale;
	QAction * select;
	QAction * visible;
	QAction * reset;
	QAction * startAtZero;
	QToolButton * selectionMode;

	QAction *hidden;
	QAction *axes;
	QAction *title;
	QAction *legend;
	QAction *position;
	QAction *properties;

	//sequential tool bar
	QToolBar * sequential;

	//timestamping area
	VipPlayerArea * playerArea;
	VipPlotWidget2D * playerWidget;

	int height;
	bool dirtyTimeLines;
	bool playWidgetHidden;
	bool alignedToZero;
};

static QList<VipPlayWidget*> playWidgets;
static VipToolTip::DisplayFlags playWidgetsFlags;

VipPlayWidget::VipPlayWidget(QWidget * parent)
	:QFrame(parent)
{

	m_data = new PrivateData();
	m_data->dirtyTimeLines = false;
	m_data->playWidgetHidden = false;
	m_data->alignedToZero = false;
	m_data->height = 0;

	//playing tool bar
	m_data->speedWidget = new VipDoubleSliderWidget(VipBorderItem::Bottom);
	m_data->speedWidget->slider()->setScaleEngine(new VipLog10ScaleEngine());
	m_data->speedWidget->slider()->grip()->setImage(vipPixmap("slider_handle.png").toImage());
	m_data->speedWidget->setRange(0.1, 100);
	m_data->speedWidget->setValue(1);
	m_data->speedWidget->setMaximumHeight(40);
	m_data->speedWidget->setMaximumWidth(200);
	m_data->speedWidget->slider()->grip()->setTransform(QTransform().translate(0, 5));//.rotate(180));
	static_cast<VipAxisBase*>(m_data->speedWidget->scale())->setUseBorderDistHintForLayout(true);
	m_data->speedWidget->setStyleSheet("background: transparent;");
	 
	m_data->playToolBar = new QToolBar();
	m_data->playToolBar->setStyleSheet("QToolBar{background-color:transparent;}");
	m_data->playToolBar->setIconSize(QSize(20, 20));
	m_data->first = m_data->playToolBar->addAction(vipIcon("first.png"), "First time");
	m_data->previous = m_data->playToolBar->addAction(vipIcon("previous.png"), "Previous time");
	m_data->playBackward = m_data->playToolBar->addAction(vipIcon("playbackward.png"), "Play backward");
	m_data->playForward = m_data->playToolBar->addAction(vipIcon("play.png"), "Play forward");
	m_data->next = m_data->playToolBar->addAction(vipIcon("next.png"), "Next time");
	m_data->last = m_data->playToolBar->addAction(vipIcon("last.png"), "Last time");
	m_data->marks = m_data->playToolBar->addAction(vipIcon("slider_limit.png"), "Enable/disable time marks");
	m_data->marks->setCheckable(true);
	m_data->repeat = m_data->playToolBar->addAction(vipIcon("repeat.png"), "Repeat");
	m_data->repeat->setCheckable(true);
	m_data->maxSpeed = m_data->playToolBar->addAction(vipIcon("speed.png"), "Use maximum speed");
	m_data->maxSpeed->setCheckable(true);
	m_data->maxSpeed->setChecked(true);
	m_data->speed = m_data->playToolBar->addWidget(m_data->speedWidget);
	m_data->speed->setVisible(false);
	m_data->playToolBar->addSeparator();
	m_data->autoScale = m_data->playToolBar->addAction(vipIcon("axises.png"), "Auto scale");
	m_data->autoScale->setCheckable(true);
	m_data->autoScale->setChecked(true);
	m_data->selectionMode = new QToolButton(this);
	m_data->select = m_data->playToolBar->addWidget(m_data->selectionMode);
	m_data->select->setCheckable(true);
	m_data->select->setVisible(false); //hide it for now
	m_data->visible = m_data->playToolBar->addAction(vipIcon("time_lines.png"), "Show/hide the time lines");
	m_data->visible->setCheckable(true);
	m_data->visible->setChecked(true);
	m_data->playToolBar->addSeparator();
	m_data->startAtZero = m_data->playToolBar->addAction(vipIcon("align_left.png"), "Align time ranges to zero");
	m_data->startAtZero->setCheckable(true);
	m_data->reset = m_data->playToolBar->addAction(vipIcon("reset.png"), "Reset all time ranges");
	m_data->lock = m_data->playToolBar->addAction(vipIcon("lock.png"), "Lock/Unlock time ranges");
	m_data->lock->setCheckable(true);

	m_data->selectionMode->setToolTip(QString("Selection mode"));
	m_data->selectionMode->setIcon(vipIcon("select_item.png"));
	m_data->selectionMode->setAutoRaise(true);
	QMenu * menu = new QMenu();
	QAction * select = menu->addAction(vipIcon("select_item.png"), "Select items");
	QAction * selectArea = menu->addAction(vipIcon("select_area.png"), "Select items in area");
	QAction * zoomArea = menu->addAction(vipIcon("zoom_area.png"), "Zoom on area");
	m_data->selectionMode->setMenu(menu);
	m_data->selectionMode->setPopupMode(QToolButton::InstantPopup);
	menu->addSeparator();
	m_data->hidden = menu->addAction("Tool tip hidden");
	m_data->hidden->setData((int)VipToolTip::Hidden);
	m_data->hidden->setCheckable(true);
	m_data->axes = menu->addAction("Tool tip: show axis values");
	m_data->axes->setData((int)VipToolTip::Axes);
	m_data->axes->setCheckable(true);
	m_data->title = menu->addAction("Tool tip: show item titles");
	m_data->title->setData((int)VipToolTip::ItemsTitles);
	m_data->title->setCheckable(true);
	m_data->legend = menu->addAction("Tool tip: show item legends");
	m_data->legend->setData((int)VipToolTip::ItemsLegends);
	m_data->legend->setCheckable(true);
	m_data->position = menu->addAction("Tool tip: show item positions");
	m_data->position->setData((int)VipToolTip::ItemsPos);
	m_data->position->setCheckable(true);
	m_data->properties = menu->addAction("Tool tip: show item properties");
	m_data->properties->setData((int)VipToolTip::ItemsProperties);
	m_data->properties->setCheckable(true);
	connect(select, SIGNAL(triggered(bool)), this, SLOT(selectionItem()), Qt::QueuedConnection);
	connect(selectArea, SIGNAL(triggered(bool)), this, SLOT(selectionItemArea()), Qt::QueuedConnection);
	connect(zoomArea, SIGNAL(triggered(bool)), this, SLOT(selectionZoomArea()), Qt::QueuedConnection);
	connect(m_data->hidden, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(m_data->axes, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(m_data->title, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(m_data->legend, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(m_data->position, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(m_data->properties, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(m_data->lock, SIGNAL(triggered(bool)), this, SLOT(setTimeRangesLocked(bool)));



	//time/number
	m_data->time = new QLabel("<b>Time</b>");
	m_data->time->setMaximumWidth(70);
	m_data->timeEdit = new VipDoubleEdit();
	m_data->timeEdit->setMaximumWidth(100);
	m_data->timeEdit->setFrame(false);
	m_data->frameEdit = new QSpinBox();
	m_data->frameEdit->setMaximumWidth(100);
	m_data->frameEdit->setFrame(false);
	m_data->frameEdit->setToolTip("Frame position");
	m_data->timeUnit = new VipValueToTimeButton();



	QHBoxLayout * timeLay = new QHBoxLayout();
	timeLay->addWidget(m_data->time);
	timeLay->addWidget(m_data->timeEdit);
	timeLay->addWidget(m_data->timeUnit);
	timeLay->addWidget(m_data->frameEdit);
	timeLay->setContentsMargins(0, 0, 0, 0);
	m_data->timeEdit->setMaximumWidth(100);
	m_data->frameEdit->setMaximumWidth(100);


	//timestamping area
	m_data->playerArea = new VipPlayerArea();
	m_data->playerWidget = new VipPlotWidget2D();
	m_data->playerWidget->setArea(m_data->playerArea);
	m_data->playerArea->setMargins(VipMargins(10, 0, 10, 0));
	m_data->playerArea->timeMarker()->installSceneEventFilter(m_data->playerArea->timeSliderGrip());
	m_data->playerArea->limit1Marker()->installSceneEventFilter(m_data->playerArea->limit1SliderGrip());
	m_data->playerArea->limit2Marker()->installSceneEventFilter(m_data->playerArea->limit2SliderGrip());
	m_data->playerArea->timeScale()->scaleDraw()->enableLabelOverlapping(false);
	//m_data->playerWidget->setStyleSheet("VipPlotWidget2D{background-color: transparent;}");

	VipValueToTime * v = new VipValueToTime();
	VipDateTimeScaleEngine * engine = new VipDateTimeScaleEngine();
	engine->setValueToTime(v);
	m_data->playerArea->timeScale()->setScaleEngine(engine);
	m_data->playerArea->timeScale()->scaleDraw()->setValueToText(v);


	m_data->sequential = new QToolBar();
	m_data->sequential->setObjectName("Sequential_toolbar");
	m_data->sequential->setIconSize(QSize(18, 18));
	m_data->sequential->addSeparator();

	QHBoxLayout * toolbarLayout = new QHBoxLayout();
	toolbarLayout->setContentsMargins(0, 0, 0, 0);
	toolbarLayout->addLayout(timeLay);
	QWidget * line = VipLineWidget::createSunkenVLine();
	toolbarLayout->addWidget(line);
	toolbarLayout->addWidget(m_data->playToolBar);
	toolbarLayout->addWidget(m_data->sequential);
	//line->show();


	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addWidget(m_data->playerWidget);
	vlay->addLayout(toolbarLayout);
	vlay->setContentsMargins(2, 6, 2, 2);
	setLayout(vlay);


	connect(m_data->visible, SIGNAL(triggered(bool)), this, SLOT(setTimeRangeVisible(bool)));
	connect(m_data->timeUnit, SIGNAL(timeUnitChanged()), this, SLOT(timeUnitChanged()));

	connect(m_data->playerArea, SIGNAL(devicesChanged()), this, SLOT(updatePlayer()));
	connect(m_data->playerArea, SIGNAL(processingPoolChanged(VipProcessingPool*)), this, SLOT(updatePlayer()));

	connect(m_data->playerArea, SIGNAL(mouseScaleAboutToChange()), this, SLOT(disableAutoScale()));
	connect(m_data->playerArea, SIGNAL(autoScaleChanged(bool)), this, SLOT(setAutoScale(bool)));

	toolTipFlagsChanged(m_data->playerArea->plotToolTip()->displayFlags());
	setTimeRangesLocked(true);

	VipUniqueId::id(this);
	playWidgets.append(this);
}

VipPlayWidget::~VipPlayWidget()
{
	playWidgets.removeOne(this);
	delete m_data;
}


QColor VipPlayWidget::sliderColor() const
{
	return static_cast<TimeScale*>(m_data->playerArea->timeScale())->sliderColor;
}
void VipPlayWidget::setSliderColor(const QColor& c)
{
	static_cast<TimeScale*>(m_data->playerArea->timeScale())->sliderColor = c;
}

QColor VipPlayWidget::sliderFillColor() const
{
	return static_cast<TimeScale*>(m_data->playerArea->timeScale())->sliderFillColor;
}
void VipPlayWidget::setSliderFillColor(const QColor& c)
{
	static_cast<TimeScale*>(m_data->playerArea->timeScale())->sliderFillColor = c;
	m_data->playerArea->timeMarker()->setPen(c);
}

void VipPlayWidget::selectionItem()
{
	m_data->playerArea->setMouseZoomSelection(Qt::NoButton);
	m_data->playerArea->setMouseItemSelection(Qt::NoButton);
	m_data->selectionMode->setIcon(vipIcon("select_item.png"));
}

void VipPlayWidget::selectionItemArea()
{
	m_data->playerArea->setMouseZoomSelection(Qt::NoButton);
	m_data->playerArea->setMouseItemSelection(Qt::LeftButton);
	m_data->selectionMode->setIcon(vipIcon("select_area.png"));
}

void VipPlayWidget::selectionZoomArea()
{
	m_data->playerArea->setMouseZoomSelection(Qt::LeftButton);
	m_data->playerArea->setMouseItemSelection(Qt::NoButton);
	m_data->selectionMode->setIcon(vipIcon("zoom_area.png"));
}

void VipPlayWidget::toolTipChanged()
{
	QAction * act = qobject_cast<QAction*>(this->sender());
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
	m_data->hidden->blockSignals(true);
	m_data->axes->blockSignals(true);
	m_data->title->blockSignals(true);
	m_data->legend->blockSignals(true);
	m_data->position->blockSignals(true);
	m_data->properties->blockSignals(true);

	m_data->hidden->setChecked(flags & VipToolTip::Hidden);
	m_data->axes->setChecked(flags & VipToolTip::Axes);
	m_data->title->setChecked(flags & VipToolTip::ItemsTitles);
	m_data->legend->setChecked(flags & VipToolTip::ItemsLegends);
	m_data->position->setChecked(flags & VipToolTip::ItemsPos);
	m_data->properties->setChecked(flags & VipToolTip::ItemsProperties);

	m_data->hidden->blockSignals(false);
	m_data->axes->blockSignals(false);
	m_data->title->blockSignals(false);
	m_data->legend->blockSignals(false);
	m_data->position->blockSignals(false);
	m_data->properties->blockSignals(false);

	playWidgetsFlags = flags;
	if (VipToolTip * tip = m_data->playerArea->plotToolTip())
	{
		tip->setDisplayFlags(flags);
	}
}

void VipPlayWidget::setTimeType(VipValueToTime::TimeType type)
{
	if(type != timeType())
		m_data->timeUnit->setValueToTime(type);
}

VipValueToTime::TimeType VipPlayWidget::timeType() const
{
	return m_data->timeUnit->valueToTime();
}

VipPlayerArea * VipPlayWidget::area() const
{
	return const_cast<VipPlayerArea*>(m_data->playerArea);
}

VipValueToTime * VipPlayWidget::valueToTime() const
{
	return static_cast<VipValueToTime*>(m_data->playerArea->timeScale()->scaleDraw()->valueToText());
}

void VipPlayWidget::showEvent(QShowEvent * )
{
	if (m_data->playWidgetHidden)
		hide();//evt->ignore();
}

QSize VipPlayWidget::sizeHint() const
{
	QSize size = QWidget::sizeHint();

	//compute the best height: 100 + height of VipPlayerArea
	QList<VipTimeRangeListItem *> items = area()->timeRangeListItems();
	size.setHeight(100 + items.size() * 30);
	return size;
}

void VipPlayWidget::setPlayWidgetHidden(bool hidden)
{
	m_data->playWidgetHidden = hidden;
	if (hidden)
		hide();
	else
		updatePlayer();
}

bool VipPlayWidget::playWidgetHidden() const
{
	return m_data->playWidgetHidden;
}

bool VipPlayWidget::isAutoScale() const
{
	return m_data->playerArea->isAutoScale();
}

void VipPlayWidget::setAutoScale(bool enable)
{
	if (enable != m_data->playerArea->isAutoScale())
	{
		m_data->playerArea->computeStartDate();
		m_data->playerArea->setAutoScale(enable);
		m_data->autoScale->blockSignals(true);
		m_data->autoScale->setChecked(enable);
		m_data->autoScale->blockSignals(false);
	}
}

void VipPlayWidget::disableAutoScale()
{
	setAutoScale(false);
}


void VipPlayWidget::setProcessingPool(VipProcessingPool * pool)
{
	if(processingPool())
	{
		disconnect(m_data->first,SIGNAL(triggered(bool)),processingPool(),SLOT(first()));
		disconnect(m_data->previous,SIGNAL(triggered(bool)),processingPool(),SLOT(previous()));
		disconnect(m_data->playForward,SIGNAL(triggered(bool)),this,SLOT(playForward()));
		disconnect(m_data->playBackward,SIGNAL(triggered(bool)),this,SLOT(playBackward()));
		disconnect(m_data->next,SIGNAL(triggered(bool)),processingPool(),SLOT(next()));
		disconnect(m_data->last,SIGNAL(triggered(bool)),processingPool(),SLOT(last()));
		disconnect(m_data->marks,SIGNAL(triggered(bool)),this,SLOT(setLimitsEnable(bool)));
		disconnect(m_data->repeat,SIGNAL(triggered(bool)),processingPool(),SLOT(setRepeat(bool)));
		disconnect(m_data->maxSpeed,SIGNAL(triggered(bool)),this,SLOT(setMaxSpeed(bool)));
		disconnect(m_data->speedWidget,SIGNAL(valueChanged(double)),processingPool(),SLOT(setPlaySpeed(double)));

		disconnect(m_data->timeEdit,SIGNAL(returnPressed()),this,SLOT(timeEdited()));
		disconnect(m_data->frameEdit,static_cast<void (QSpinBox::*)(int)>( &QSpinBox::valueChanged),processingPool(),&VipProcessingPool::seekPos);

		disconnect(m_data->autoScale,SIGNAL(triggered(bool)),this,SLOT(setAutoScale(bool)));
		disconnect(m_data->select,SIGNAL(triggered(bool)),this,SLOT(selectionClicked(bool)));
		disconnect(m_data->reset,SIGNAL(triggered(bool)),this,SLOT(resetAllTimeRanges()));
		disconnect(m_data->startAtZero,SIGNAL(triggered(bool)),m_data->playerArea,SLOT(alignToZero(bool)));

		disconnect(processingPool(),SIGNAL(processingChanged(VipProcessingObject*)),this,SLOT(updatePlayer()));
		disconnect(processingPool(),SIGNAL(timeChanged(qint64)),this,SLOT(timeChanged()));

		disconnect(processingPool(), SIGNAL(objectAdded(QObject*)), this, SLOT(deviceAdded()));
	}


	m_data->playerArea->setProcessingPool(pool);

	if(processingPool())
	{
		// connections
		connect(m_data->first,SIGNAL(triggered(bool)),processingPool(),SLOT(first()));
		connect(m_data->previous,SIGNAL(triggered(bool)),processingPool(),SLOT(previous()));
		connect(m_data->playForward,SIGNAL(triggered(bool)),this,SLOT(playForward()));
		connect(m_data->playBackward,SIGNAL(triggered(bool)),this,SLOT(playBackward()));
		connect(m_data->next,SIGNAL(triggered(bool)),processingPool(),SLOT(next()));
		connect(m_data->last,SIGNAL(triggered(bool)),processingPool(),SLOT(last()));
		connect(m_data->marks,SIGNAL(triggered(bool)),this,SLOT(setLimitsEnable(bool)));
		connect(m_data->repeat,SIGNAL(triggered(bool)),processingPool(),SLOT(setRepeat(bool)));
		connect(m_data->maxSpeed,SIGNAL(triggered(bool)),this,SLOT(setMaxSpeed(bool)));
		connect(m_data->speedWidget,SIGNAL(valueChanged(double)),processingPool(),SLOT(setPlaySpeed(double)));

		//time/number
		connect(m_data->timeEdit,SIGNAL(returnPressed()),this,SLOT(timeEdited()));
		connect(m_data->frameEdit,static_cast<void (QSpinBox::*)(int)>( &QSpinBox::valueChanged),processingPool(),&VipProcessingPool::seekPos);

		//modify timestamping
		connect(m_data->autoScale, SIGNAL(triggered(bool)), this, SLOT(setAutoScale(bool)));
		connect(m_data->select,SIGNAL(triggered(bool)),this,SLOT(selectionClicked(bool)));
		connect(m_data->reset,SIGNAL(triggered(bool)),this,SLOT(resetAllTimeRanges()));
		connect(m_data->startAtZero,SIGNAL(triggered(bool)),m_data->playerArea,SLOT(alignToZero(bool)));

		connect(processingPool(),SIGNAL(processingChanged(VipProcessingObject*)),this,SLOT(updatePlayer()));
		connect(processingPool(),SIGNAL(timeChanged(qint64)),this,SLOT(timeChanged()));

		connect(processingPool(), SIGNAL(objectAdded(QObject*)), this, SLOT(deviceAdded()));
	}

	timeChanged();
	updatePlayer();
}

void VipPlayWidget::setAlignedToZero(bool enable)
{
	if (enable != m_data->alignedToZero) {
		m_data->startAtZero->blockSignals(true);
		m_data->startAtZero->setChecked(enable);
		m_data->startAtZero->blockSignals(false);
		m_data->playerArea->alignToZero(enable);
	}
	m_data->alignedToZero = enable;
}

bool VipPlayWidget::isAlignedToZero() const
{
	return m_data->startAtZero->isChecked();
}

void VipPlayWidget::setLimitsEnabled(bool enable)
{
	m_data->playerArea->setLimitsEnable(enable);
	m_data->marks->blockSignals(true);
	m_data->marks->setChecked(enable);
	m_data->marks->blockSignals(false);
}

bool VipPlayWidget::isLimitsEnabled() const
{
	return m_data->playerArea->limitsEnabled();
}

bool VipPlayWidget::isMaxSpeed() const
{
	return m_data->maxSpeed->isChecked();
}

double VipPlayWidget::playSpeed() const
{
	return processingPool()->playSpeed();
}

void VipPlayWidget::setPlaySpeed(double speed)
{
	m_data->speedWidget->setValue(speed);
}

void VipPlayWidget::deviceAdded()
{
	//TEST
	if(isAlignedToZero())
		setAlignedToZero(isAlignedToZero());
}

void VipPlayWidget::resetAllTimeRanges()
{
	m_data->playerArea->resetAllTimeRanges();
	m_data->startAtZero->blockSignals(true);
	m_data->startAtZero->setChecked(false);
	m_data->startAtZero->blockSignals(false);
}

void VipPlayWidget::setTimeRangeVisible(bool visible)
{
	m_data->playerArea->setTimeRangeVisible(visible);
	m_data->visible->blockSignals(true);
	m_data->visible->setChecked(visible);
	m_data->visible->blockSignals(false);
	updatePlayer();
}

bool VipPlayWidget::timeRangeVisible() const
{
	return m_data->playerArea->timeRangeVisible();
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
	if (!m_data->dirtyTimeLines)
	{
		m_data->dirtyTimeLines = true;
		QMetaObject::invokeMethod(this, "updatePlayerInternal", Qt::QueuedConnection);
	}
}

void VipPlayWidget::updatePlayerInternal()
{
	m_data->dirtyTimeLines = false;

	if(!processingPool())
		return;

	if(timeUnitFunction() && m_data->timeUnit->automaticUnit())
	{
		this->setTimeType( timeUnitFunction()(this) );
		m_data->timeUnit->setAutomaticUnit(true);
	}

	m_data->first->blockSignals(true);
	m_data->previous->blockSignals(true);
	m_data->playForward->blockSignals(true);
	m_data->playBackward->blockSignals(true);
	m_data->next->blockSignals(true);
	m_data->last->blockSignals(true);
	m_data->marks->blockSignals(true);
	m_data->repeat->blockSignals(true);
	m_data->maxSpeed->blockSignals(true);
	m_data->speed->blockSignals(true);
	m_data->timeEdit->blockSignals(true);
	m_data->frameEdit->blockSignals(true);
	m_data->speedWidget->blockSignals(true);

	bool max_speed = !processingPool()->testMode(VipProcessingPool::UsePlaySpeed);
	m_data->maxSpeed->setChecked(max_speed);
	m_data->speed->setVisible(!max_speed);
	m_data->speedWidget->setValue(processingPool()->playSpeed());
	m_data->repeat->setChecked(processingPool()->testMode(VipProcessingPool::Repeat));
	m_data->marks->setChecked(processingPool()->testMode(VipProcessingPool::UseTimeLimits));
	m_data->playerArea->setLimitsEnable(processingPool()->testMode(VipProcessingPool::UseTimeLimits));

	if(processingPool()->isPlaying())
	{
		if(processingPool()->testMode(VipProcessingPool::Backward))
			m_data->playBackward->setIcon(vipIcon("pause.png"));
		else
			m_data->playForward->setIcon(vipIcon("pause.png"));
	}
	else
	{
		m_data->playBackward->setIcon(vipIcon("playbackward.png"));
		m_data->playForward->setIcon(vipIcon("play.png"));
	}

	bool has_frame = processingPool()->deviceType() == VipIODevice::Temporal &&  //area()->timeRangeListItems().size() == 1 &&
 processingPool()->size() > 0;
	if(has_frame)
	{
		m_data->frameEdit->setRange(0,processingPool()->size());
		m_data->frameEdit->setValue(processingPool()->timeToPos(processingPool()->time()));
	}
	m_data->timeEdit->setText(m_data->playerArea->timeScale()->scaleDraw()->label(processingPool()->time(),VipScaleDiv::MajorTick).text());

	m_data->first->blockSignals(false);
	m_data->previous->blockSignals(false);
	m_data->playForward->blockSignals(false);
	m_data->playBackward->blockSignals(false);
	m_data->next->blockSignals(false);
	m_data->last->blockSignals(false);
	m_data->marks->blockSignals(false);
	m_data->repeat->blockSignals(false);
	m_data->maxSpeed->blockSignals(false);
	m_data->speed->blockSignals(false);
	m_data->timeEdit->blockSignals(false);
	m_data->frameEdit->blockSignals(false);
	m_data->speedWidget->blockSignals(false);

	QList<VipTimeRangeListItem *> items = area()->timeRangeListItems() ;

	int visibleItemCount = m_data->playerArea->visibleItemCount();
	bool show_temporal = (processingPool()->deviceType() == VipIODevice::Temporal) && processingPool()->size() != 1 && visibleItemCount;

	if (!show_temporal)
	{
		//if no temporal devices, disable time limits
		processingPool()->blockSignals(true);
		processingPool()->setMode(VipProcessingPool::UseTimeLimits, false);
		processingPool()->blockSignals(false);
	}

	m_data->time->setVisible(show_temporal);
	m_data->timeEdit->setVisible(show_temporal);
	m_data->timeUnit->setVisible(show_temporal);
	m_data->frameEdit->setVisible(show_temporal && has_frame);
	m_data->playerWidget->setVisible(show_temporal);
	m_data->playToolBar->setVisible(show_temporal);
	//TODO: play widget not drawn with opengl enabled (all black)
	//if (show_temporal)
	//	m_data->playerWidget->repaint();

	//int dpi = logicalDpiY();
	// double millimeter_to_pixel = (dpi / 2.54 / 10);
	// double _50 = 50 / millimeter_to_pixel;
	// double _15 = 15 / millimeter_to_pixel;
	// double _20 = 20 / millimeter_to_pixel;
	// double _30 = 30 / millimeter_to_pixel;

	//compute the best height
	int height = 0;
	if(show_temporal)
	{
		height = 35 + m_data->playerArea->timeScale()->scaleDraw()->fullExtent();
		if (timeRangeVisible())
			height += visibleItemCount * 14;
		layout()->setSpacing(2);
	}
	else
		layout()->setSpacing(0);

	if (m_data->speed->isVisible()) {
		int th = m_data->playToolBar->height();
		int sh = m_data->speedWidget->height();
		if(sh > th)
			height += sh-th;
	}

	//add bottom margin (NEW)
	height += 10;


	this->setVisible(show_temporal);// height > 0);
	if (height != m_data->height)
	{
		m_data->height = height;
		this->setMaximumHeight(height);
		this->setMinimumHeight(height);

		//try to update the time scale (TODO: find a better way)
		int w = m_data->playerWidget->width();
		int h = m_data->playerWidget->height();
		m_data->playerWidget->resize(w + 1, h + 1);
		m_data->playerWidget->resize(w, h);
		m_data->playerWidget->recomputeGeometry();
		vipProcessEvents(NULL, 100);
	}

}

void VipPlayWidget::timeChanged()
{
	if (!processingPool())
		return;

	m_data->frameEdit->blockSignals(true);
	m_data->timeEdit->blockSignals(true);

	bool has_frame = processingPool()->size() > 0;
	//m_data->frameEdit->setVisible(has_frame);
	if(has_frame)
	{
		//m_data->frameEdit->setRange(0,processingPool()->size());
		m_data->frameEdit->setValue(processingPool()->timeToPos(processingPool()->time()));
	}
	m_data->timeEdit->setText(m_data->playerArea->timeScale()->scaleDraw()->label(processingPool()->time(),VipScaleDiv::MajorTick).text());

	m_data->frameEdit->blockSignals(false);
	m_data->timeEdit->blockSignals(false);
}

void VipPlayWidget::timeUnitChanged()
{
	VipValueToTime * v = m_data->timeUnit->currentValueToTime().copy();
	static_cast<VipDateTimeScaleEngine*>(m_data->playerArea->timeScale()->scaleEngine())->setValueToTime(v);
	m_data->playerArea->timeScale()->scaleDraw()->setValueToText(v);

	m_data->playerArea->computeStartDate();

	//We change the time unit, so get back to auto scale and recompute it
	m_data->playerArea->setAutoScale(false);
	m_data->playerArea->setAutoScale(true);

	updatePlayer();

	Q_EMIT valueToTimeChanged(v);
}

VipProcessingPool * VipPlayWidget::processingPool() const
{
	return m_data->playerArea->processingPool();
}

void VipPlayWidget::timeEdited()
{
	//retrieve the time inside timeEdit and update the VipProcessingPool's time
	QString value = m_data->timeEdit->text();
	bool ok;
	double time = m_data->playerArea->timeScale()->scaleDraw()->valueToText()->fromString(value,&ok);
	if(ok)
	{
		m_data->timeEdit->setStyleSheet("QLabel { border: 0px solid transparent; }");
		processingPool()->seek(time);
	}
	else
	{
		m_data->timeEdit->setStyleSheet("QLabel { border: 1px solid red; }");
	}
}

void VipPlayWidget::setMaxSpeed(bool enable)
{
	m_data->maxSpeed->blockSignals(true);
	m_data->maxSpeed->blockSignals(enable);
	m_data->maxSpeed->blockSignals(false);

	m_data->speed->setVisible(!enable);
	m_data->speedWidget->setVisible(!enable);
	processingPool()->setMode(VipProcessingPool::UsePlaySpeed,!enable);
}

void VipPlayWidget::playForward()
{
	if(!processingPool())
		return;

	if(processingPool()->isPlaying())
		processingPool()->stop();
	else
	{
		processingPool()->setMode(VipProcessingPool::Backward,false);
		processingPool()->play();
	}
}

void VipPlayWidget::playBackward()
{
	if(!processingPool())
		return;

	if(processingPool()->isPlaying())
		processingPool()->stop();
	else
	{
		processingPool()->setMode(VipProcessingPool::Backward,true);
		processingPool()->play();
	}
}

void VipPlayWidget::selectionClicked(bool enable)
{
	//enable/disable the mouse item selection
	if(enable)
		m_data->playerArea->setMouseItemSelection(Qt::LeftButton);
	else
		m_data->playerArea->setMouseItemSelection(Qt::NoButton);
}

void VipPlayWidget::setLimitsEnable(bool enable)
{
	if(enable)
	{
		qint64 first = processingPool()->stopBeginTime();
		qint64 last = processingPool()->stopEndTime();
		if(first == VipInvalidTime)
		{
			first = processingPool()->firstTime();
			processingPool()->setStopBeginTime(first);
		}
		if(last == VipInvalidTime)
		{
			last = processingPool()->lastTime();
			processingPool()->setStopEndTime(last);
		}
	}

	m_data->playerArea->setLimitsEnable(enable);
}

bool VipPlayWidget::timeRangesLocked() const
{
	return m_data->playerArea->timeRangesLocked();
}

void VipPlayWidget::setTimeRangesLocked(bool locked)
{
	m_data->lock->blockSignals(true);
	m_data->lock->setChecked(locked);
	m_data->lock->blockSignals(false);
	m_data->playerArea->setTimeRangesLocked(locked);
}



VipArchive & operator<<(VipArchive & arch, VipPlayWidget * w)
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

	//axis scale div
	arch.content("x_scale", w->area()->bottomAxis()->scaleDiv());
	arch.content("y_scale", w->area()->leftAxis()->scaleDiv());

	arch.content("locked", w->timeRangesLocked());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlayWidget * w)
{
	w->setAlignedToZero(arch.read("aligned").toBool());
	w->setTimeRangeVisible(arch.read("visible_ranges").toInt());
	w->setAutoScale(arch.read("auto_scale").toInt());
	w->setLimitsEnable( arch.read("time_limits").toInt());
	qint64 l1 = arch.read("time_limit1").toLongLong();
	qint64 l2 = arch.read("time_limit2").toLongLong();
	if(l1 != VipInvalidTime)
		w->area()->setLimit1(l1);
	if (l2 != VipInvalidTime)
		w->area()->setLimit2(l2);
	w->setMaxSpeed(arch.read("max_speed").toBool());
	w->setPlaySpeed(arch.read("speed").toDouble());
	w->area()->setTime(arch.read("time").toDouble());

	VipScaleDiv divx = arch.read("x_scale").value<VipScaleDiv>();
	VipScaleDiv divy = arch.read("y_scale").value<VipScaleDiv>();

	if (!w->isAutoScale())
	{
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
