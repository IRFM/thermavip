#include <float.h>
#include <condition_variable>
#include <chrono>

//Qt includes
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCoreApplication>
#include <QScrollBar>
#include <QPicture>
#include <QPointer>
#include <QApplication>
#include <QTimer>
#include <QMimeData>
#include <qopenglcontext.h>
#include <qoffscreensurface.h>
#include <QOpenGLFramebufferObjectFormat>
#include <qopenglpaintdevice.h>
#include <qwindow.h>
#include <qdatetime.h>
#include <QOpenGLTexture>
#include <qpixmapcache.h>
#include <qpaintengine.h>
#include <qopenglfunctions.h>
#include <qopenglwidget.h>
#include <qwaitcondition.h>
#include <qoffscreensurface.h>

//Our includes
#include "VipAxisColorMap.h"
#include "VipPlotGrid.h"
#include "VipPlotWidget2D.h"
#include "VipPlotSpectrogram.h"
#include "VipNDArray.h"
#include "VipNDArrayImage.h"
#include "VipLegendItem.h"
#include "VipDynGridLayout.h"
#include "VipPolarAxis.h"
#include "VipToolTip.h"
#include "VipMultiPlotWidget2D.h"
#include "VipPlotShape.h"
#include "VipCorrectedTip.h"
#include "VipPainter.h"
#include "VipLogging.h"
#include "VipPlotCurve.h"
#include "VipLock.h"
#include "VipSleep.h"
#include "VipPicture.h"

/// @internal List of vertically/horizontally aligned areas
/// This structred is shared by all aligned areas.
struct AlignedArea
{
	Qt::Orientation align;
	QSet<VipAbstractPlotArea*> areas;
};
using SharedAlignedArea = QSharedPointer<AlignedArea>;
Q_DECLARE_METATYPE(SharedAlignedArea)

static int _registerSharedAlignedArea()
{
	qRegisterMetaType<SharedAlignedArea>();
	return 0;
}

/// @brief Returns the SharedAlignedArea (possibly null) associated to given area for given orientation
static SharedAlignedArea getSharedAlignedArea(const VipAbstractPlotArea* area, Qt::Orientation align)
{
	static int _reg = _registerSharedAlignedArea();

	if (align == Qt::Vertical)
		return area->property("_vip_vAlignedArea").value<SharedAlignedArea>();
	else
		return area->property("_vip_hAlignedArea").value<SharedAlignedArea>();
}
/// @brief Remove the SharedAlignedArea object from area for given orientation
static void removeSharedAlignedArea(VipAbstractPlotArea* area, Qt::Orientation align) 
{
	SharedAlignedArea sh = getSharedAlignedArea(area, align);
	if (sh) {
		sh->areas.remove(area);
		if (align == Qt::Vertical)
			area->setProperty("_vip_vAlignedArea", QVariant());
		else
			area->setProperty("_vip_hAlignedArea", QVariant());
	}
}
/// @brief Remove all SharedAlignedArea objects from area
static void removeSharedAlignedArea(VipAbstractPlotArea* area)
{
	removeSharedAlignedArea(area, Qt::Vertical);
	removeSharedAlignedArea(area, Qt::Horizontal);
}

/// @brief Aligned 2 VipAbstractPlotArea vertically or horizontally
static void addSharedAlignedArea(VipAbstractPlotArea* area, VipAbstractPlotArea* aligned_with, Qt::Orientation align)
{
	// Update the SharedAlignedArea of both area and aligned_with in order to use the same SharedAlignedArea object
	// If necessary, move all aligned areas from aligned_with into area
	SharedAlignedArea sh = getSharedAlignedArea(area, align);
	SharedAlignedArea sha = getSharedAlignedArea(aligned_with, align);
	SharedAlignedArea res = sh ? sh : sha;
	const char* name = align == Qt::Vertical ? "_vip_vAlignedArea" : "_vip_hAlignedArea";

	if (sh && sha) {
		// move content of sha to sh and reset the property to all areas within sha
		auto areas = sha->areas;
		for (auto it = areas.begin(); it != areas.end(); ++it) {
			sh->areas.insert(*it);
			(*it)->setProperty(name, QVariant::fromValue(sh));
		}
	}
	if (!res)
		res.reset(new AlignedArea{ align, QSet<VipAbstractPlotArea*>{} });
	if (!sh)
		area->setProperty(name, QVariant::fromValue(res));
	if (!sha)
		aligned_with->setProperty(name, QVariant::fromValue(res));

	res->areas.insert(area);
	res->areas.insert(aligned_with);

	// trigger geometry update
	area->recomputeGeometry();
}

static QSet<VipAbstractPlotArea*> sharedAlignedAreas(const VipAbstractPlotArea* area, Qt::Orientation align)
{
	QSet<VipAbstractPlotArea*> res;
	SharedAlignedArea sh = getSharedAlignedArea(area, align);
	if (sh)
		res = sh->areas;
	return res;
}






struct GraphicsSceneMouseEvent : public QGraphicsSceneMouseEvent
{
	QGraphicsItem * item;
	bool enable;

	GraphicsSceneMouseEvent(Type type, QGraphicsItem * item, bool enable)
		:QGraphicsSceneMouseEvent(type), item(item), enable(enable) {}

	~GraphicsSceneMouseEvent() {
		//if (item)
		//	item->setItemAttribute(VipPlotItem::IgnoreMouseEvents, enable);
	}

	void import(const QGraphicsSceneMouseEvent & src)
	{
		this->setSource(src.source());
		this->setWidget(src.widget());
		this->setAccepted(false);
		this->setPos(src.pos());
		this->setScenePos(src.scenePos());
		this->setScreenPos(src.screenPos());

		this->setButtonDownPos(Qt::LeftButton, src.buttonDownPos(Qt::LeftButton));
		this->setButtonDownPos(Qt::RightButton, src.buttonDownPos(Qt::RightButton));
		this->setButtonDownPos(Qt::MiddleButton, src.buttonDownPos(Qt::MiddleButton));

		this->setButtonDownScenePos(Qt::LeftButton, src.buttonDownScenePos(Qt::LeftButton));
		this->setButtonDownScenePos(Qt::RightButton, src.buttonDownScenePos(Qt::RightButton));
		this->setButtonDownScenePos(Qt::MiddleButton, src.buttonDownScenePos(Qt::MiddleButton));

		this->setButtonDownScreenPos(Qt::LeftButton, src.buttonDownScreenPos(Qt::LeftButton));
		this->setButtonDownScreenPos(Qt::RightButton, src.buttonDownScreenPos(Qt::RightButton));
		this->setButtonDownScreenPos(Qt::MiddleButton, src.buttonDownScreenPos(Qt::MiddleButton));

		this->setLastPos(src.lastPos());
		this->setLastScenePos(src.lastScenePos());
		this->setLastScreenPos(src.lastScreenPos());
		this->setButtons(src.buttons());
		this->setButton(src.button());
		this->setModifiers(src.modifiers());
		this->setSource(src.source());
		this->setFlags(src.flags());
	}
};



VipPlotAreaFilter::VipPlotAreaFilter()
	:QGraphicsObject(), d_area(NULL)
{
}

VipAbstractPlotArea * VipPlotAreaFilter::area() const
{
	return d_area;
}


void VipPlotAreaFilter::emitFinished()
{
	Q_EMIT finished();
}




static int registerRubberBandKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {
		VipStandardStyleSheet::addTextStyleKeyWords(keywords);

		vipSetKeyWordsForClass(&VipRubberBand::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerRubberBandKeyWords = registerRubberBandKeyWords();



class VipRubberBand::PrivateData
{
public:
	PrivateData()
		:mousePressInside(false)
	{
		
	}

	bool mousePressInside;
	QPointF lastHover;
	QPointF start;
	QPointF end;
	VipPoint scaleStart;
	VipPoint scaleEnd;

	VipTextStyle textStyle;

	QPicture additionalPaintCommands;
	QPointer<VipPlotAreaFilter> filter;
};


VipRubberBand::VipRubberBand(VipAbstractPlotArea * parent)
  : VipBoxGraphicsWidget(parent)
{
	d_data = new PrivateData();
	setArea(parent);
	setAcceptHoverEvents(true);
	this->setFlag(ItemIsFocusable, true);

	boxStyle().setBorderPen(QPen());

	QColor c(Qt::blue);
	c.setAlpha(15);
	boxStyle().setBackgroundBrush(QBrush(c));
}

VipRubberBand::~VipRubberBand()
{
	delete d_data;
}


void VipRubberBand::setArea(VipAbstractPlotArea * a)
{
	if (area())
	{
		this->disconnect(area(), SIGNAL(childItemChanged(VipPlotItem*)), this, SLOT(updateGeometry()));
	}

	if (a)
	{
		this->setParentItem(a);
		this->connect(a, SIGNAL(childItemChanged(VipPlotItem*)), this, SLOT(updateGeometry()), Qt::DirectConnection);
		this->setZValue(a->zValue() + 10000);
		this->updateGeometry();
	}
	else
	{
		this->setParentItem(NULL);
	}
}

VipAbstractPlotArea * VipRubberBand::area() const
{
	return static_cast<VipAbstractPlotArea*>(parentItem());
}

void VipRubberBand::setTextStyle(const VipTextStyle & style)
{
	d_data->textStyle = style;
}

const VipTextStyle & VipRubberBand::textStyle() const
{
	return d_data->textStyle;
}

void VipRubberBand::setAdditionalPaintCommands(const QPicture & pic)
{
	if (pic.isNull() && d_data->additionalPaintCommands.isNull())
		return;
	
	d_data->additionalPaintCommands = pic;
	//if (area() && VipAbstractPlotArea::renderingThreads() > 1 )
	//	area()->markNeedUpdate();
	
	QGraphicsWidget::update();
}

const QPicture & VipRubberBand::additionalPaintCommands() const
{
	return d_data->additionalPaintCommands;
}

void VipRubberBand::drawRubberBand(QPainter * painter) const
{
	painter->setPen(QPen());
	if (d_data->start == d_data->end)
		return;

	//recompute the new start and end coordinates based on the scale coordinates
	//this is necessary because during the selection, the scale might have changed due to scroll bars
	//This might happen with VipImageArea2D when scroll bars are enabled and we select an area close to the border
	QPointF d_start = d_data->start;
	QPointF d_end = d_data->end;
	if (d_data->scaleStart != d_data->scaleEnd)
	{
		d_start = area()->scaleToPosition(d_data->scaleStart);
		d_end = area()->scaleToPosition(d_data->scaleEnd);
	}

	QString start_x, end_x, start_y, end_y;
	QList<VipAbstractScale*> scales = static_cast<VipPlotArea2D*>(area())->scales();

	for (int i = 0; i < scales.size(); ++i)
	{
		VipAbstractScale * axis = scales[i];
		if (!axis->isVisible())
			continue;

		vip_double start = axis->scaleDraw()->value(axis->mapFromItem(area(), d_start));
		vip_double end = axis->scaleDraw()->value(axis->mapFromItem(area(), d_end));

		//is it a "x" axis?
		bool is_x_scale = (qobject_cast<VipAxisBase*>(axis) && static_cast<VipAxisBase*>(axis)->orientation() == Qt::Horizontal) ||
			qobject_cast<VipRadialAxis*>(axis);

		if (is_x_scale && !axis->title().isEmpty())
		{
			//if()
			{
				start_x += axis->title().text() + ": ";
				end_x += axis->title().text() + ": ";
			}
			start_x += axis->scaleDraw()->label(start, VipScaleDiv::MajorTick).text() + "\n";
			end_x += axis->scaleDraw()->label(end, VipScaleDiv::MajorTick).text() + "\n";
		}
		else if (!is_x_scale && !axis->title().isEmpty())
		{
			//if(!axis->title().isEmpty())
			{
				start_y += axis->title().text() + ": ";
				end_y += axis->title().text() + ": ";
			}
			start_y += axis->scaleDraw()->label(start, VipScaleDiv::MajorTick).text() + "\n";
			end_y += axis->scaleDraw()->label(end, VipScaleDiv::MajorTick).text() + "\n";
		}
	}

	VipText start_text(start_x +//"\n" +
 start_y, textStyle());
	VipText end_text(end_x +//"\n" +
 end_y, textStyle());

	VipBoxStyle bs = boxStyle();

	if (qobject_cast<VipPlotArea2D*>(area()))
	{
		bs.computeRect(QRectF(d_start, d_end));
	}
	else if (qobject_cast<VipPlotPolarArea2D*>(area()))
	{
		VipPlotPolarArea2D * parea = static_cast<VipPlotPolarArea2D*>(area());
		QPointF center = parea->radialAxis()->scaleDraw()->center();
		QLineF l1(center, d_start);
		QLineF l2(center, d_end);

		VipPie pie(VipPolarCoordinate(l1.length(), l1.angle()), VipPolarCoordinate(l2.length(), l2.angle()));
		pie = pie.normalized();

		bs.computePie(center, pie.normalized());
	}

	painter->setRenderHints(QPainter::Antialiasing);
	bs.draw(painter);



	QPointF start_pos;
	QPointF end_pos;

	if (d_start.x() < d_end.x())
	{
		start_pos.setX(d_start.x() - start_text.textSize().width());
		end_pos.setX(d_end.x());
	}
	else
	{
		start_pos.setX(d_start.x());
		end_pos.setX(d_end.x() - end_text.textSize().width());
	}

	if (d_start.y() < d_end.y())
	{
		start_pos.setY(d_start.y() - start_text.textSize().height());
		end_pos.setY(d_end.y());
	}
	else
	{
		start_pos.setY(d_start.y());
		end_pos.setY(d_end.y() - end_text.textSize().height());
	}

	start_text.draw(painter, start_text.textRect().translated(start_pos));
	end_text.draw(painter, end_text.textRect().translated(end_pos));

}

void VipRubberBand::setRubberBandStart(const QPointF & start)
{
	d_data->start = d_data->end = start;
	d_data->scaleStart = d_data->scaleEnd = this->area()->positionToScale(start);
	update();
}

void VipRubberBand::setRubberBandEnd(const QPointF & end)
{
	d_data->end = end;
	
	d_data->scaleEnd = this->area()->positionToScale(end);
	update();
}


void VipRubberBand::resetRubberBand()
{
	d_data->start = d_data->end = QPointF();
	d_data->scaleStart = d_data->scaleEnd = VipPoint();
	update();
}

const QPointF & VipRubberBand::rubberBandStart() const
{
	return d_data->start;
}

const QPointF & VipRubberBand::rubberBandEnd() const
{
	return d_data->end;
}

double VipRubberBand::rubberBandWidth() const
{
	return qAbs(d_data->start.x() - d_data->end.x());
}

double VipRubberBand::rubberBandHeight() const
{
	return qAbs(d_data->start.y() - d_data->end.y());
}

QRectF VipRubberBand::rubberBandRect() const
{
	return QRectF(d_data->start, d_data->end).normalized();
}

const VipPoint & VipRubberBand::rubberBandScaleStart() const
{
	return d_data->scaleStart;
}

const VipPoint & VipRubberBand::rubberBandScaleEnd() const
{
	return d_data->scaleEnd;
}

bool VipRubberBand::hasRubberBandArea() const
{
	return d_data->start != d_data->end;
}

void VipRubberBand::installFilter(VipPlotAreaFilter * filter)
{
	if (filter != d_data->filter)
	{
		if (VipPlotAreaFilter * f = d_data->filter)
			f->deleteLater();

		d_data->filter = filter;
		if (filter)
		{
			if (filter->d_area)
				filter->d_area->removeFilter();

			filter->d_area = area();

			if (scene() != filter->scene())
				scene()->addItem(filter);
			filter->setParentItem(area());

			filter->setZValue(zValue() + 1);
		}
	}
}

void VipRubberBand::removeFilter()
{
	if (d_data->filter)
	{
		d_data->filter->setParent(NULL);
		d_data->filter->d_area = NULL;
	}
	d_data->filter = NULL;
}

VipPlotAreaFilter * VipRubberBand::filter() const
{
	return d_data->filter;
}

bool VipRubberBand::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	VipTextStyle st = this->textStyle();
	if (VipStandardStyleSheet::handleTextStyleKeyWord(name, value, st)) {
		setTextStyle(st);
		return true;
	}
	return VipBoxGraphicsWidget::setItemProperty(name, value, index);
}

void VipRubberBand::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
	if (!this->paintingEnabled())
		return;
	this->applyStyleSheetIfDirty();

	painter->save();

	//Be carefull, there might be an offset between the rubber band top left position (0,0 in its own coordinate) and
	//the VipAbstractPlotArea top left position (boundingRect().topLeft()).
	//
	//Since the rubber band and the additional painter commands are in VipAbstractPlotArea coordinates, apply this offset to the painter
	QTransform tr;
	QPointF offset = -area()->boundingRect().topLeft();
	tr.translate(offset.x(), offset.y());
	painter->setWorldTransform(tr, true);

	if (VipPlotAreaFilter * f = d_data->filter)
	{
		f->paint(painter, option, widget);
	}
	else
	{
		drawRubberBand(painter);
		if (!d_data->additionalPaintCommands.isNull())
		{
			painter->setRenderHint(QPainter::Antialiasing);
			painter->drawPicture(0, 0, d_data->additionalPaintCommands);
		}
	}
	painter->restore();
}

void VipRubberBand::updateGeometry()
{
	this->setGeometry(parentItem()->boundingRect());
}

void	VipRubberBand::keyPressEvent(QKeyEvent * event)
{
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->keyPressEvent(event);
	}
	else
		area()->keyPressEvent(event);
}
void	VipRubberBand::keyReleaseEvent(QKeyEvent * event)
{
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->keyReleaseEvent(event);
	}
	else
		area()->keyReleaseEvent(event);
}
void	VipRubberBand::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event)
{
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->mouseDoubleClickEvent(event);
	}
	else
		area()->mouseDoubleClickEvent(event);
}
void	VipRubberBand::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->mouseMoveEvent(event);
	}
	else
		area()->mouseMoveEvent(event);
}

static bool _inSimulate = false;
void	VipRubberBand::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
	if (_inSimulate) {
		_inSimulate = false;
		event->ignore();
		return;
	}
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	event->setButtonDownPos(event->button(), event->buttonDownPos(event->button()) + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->mousePressEvent(event);
	}
	else
		area()->mousePressEvent(event);
}
void	VipRubberBand::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->mouseReleaseEvent(event);
	}
	else
		area()->mouseReleaseEvent((event));
}
void	VipRubberBand::hoverEnterEvent(QGraphicsSceneHoverEvent * event)
{
	VipBoxGraphicsWidget::hoverEnterEvent(event);
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->hoverEnterEvent(event);
	}
	else
		area()->hoverEnterEvent(event);
}
void	VipRubberBand::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
	VipBoxGraphicsWidget::hoverLeaveEvent(event);
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->hoverLeaveEvent(event);
	}
	else
		area()->hoverLeaveEvent(event);
}
void	VipRubberBand::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
	if (event->pos() != d_data->lastHover)
	{
		d_data->lastHover = event->pos();
		QPointF offset = area()->boundingRect().topLeft();
		event->setPos(event->pos() + offset);
		if (VipPlotAreaFilter * f = d_data->filter)
		{
			if (!f->sceneEvent(event))
				area()->hoverMoveEvent(event);
		}
		else
			area()->hoverMoveEvent((event));
	}
}

void	VipRubberBand::wheelEvent(QGraphicsSceneWheelEvent * event)
{
	QPointF offset = area()->boundingRect().topLeft();
	event->setPos(event->pos() + offset);
	if (VipPlotAreaFilter * f = d_data->filter)
	{
		if (!f->sceneEvent(event))
			area()->wheelEvent(event);
	}
	else
		area()->wheelEvent(event);
}




VipDrawSelectionOrder::VipDrawSelectionOrder(VipAbstractPlotArea * parent)
	:QGraphicsObject(parent), m_align(Qt::AlignLeft | Qt::AlignHCenter)
{
}

void VipDrawSelectionOrder::setArea(VipAbstractPlotArea * a)
{
	if (a)
	{
		this->setParentItem(a);
		this->setZValue(a->zValue() + 20000);
	}
	else
	{
		this->setParentItem(NULL);
	}
}

VipAbstractPlotArea * VipDrawSelectionOrder::area() const
{
	return static_cast<VipAbstractPlotArea*>(parentItem());
}

void VipDrawSelectionOrder::setFont(const QFont & font)
{
	m_font = font;
	update();
}

QFont VipDrawSelectionOrder::font() const
{
	return m_font;
}

void VipDrawSelectionOrder::setAlignment(Qt::Alignment align)
{
	m_align = align;
}

Qt::Alignment VipDrawSelectionOrder::alignment() const
{
	return m_align;
}

QRectF VipDrawSelectionOrder::boundingRect() const
{
	return area() ? area()->boundingRect() : QRectF();
}
QPainterPath VipDrawSelectionOrder::shape() const
{
	return QPainterPath();
}

void VipDrawSelectionOrder::paint(QPainter * painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	if (area())
	{
		int order = 1;
		QRectF bounding;
		if (VipVMultiPlotArea2D * a = qobject_cast<VipVMultiPlotArea2D*>(area()))
			bounding = a->plotRect();
		else
			bounding = area()->canvas()->boundingRect();
		//reduce area bounding rect to remove ticks
		bounding = bounding.adjusted(10, 10, -10, -10);

		QList<VipPlotItem*> items = vipCastItemListOrdered<VipPlotItem*>(area()->plotItems(), QString(), 1, 1);
		for (int i = 0; i < items.size(); ++i) {
			//ignore VipPlotCanvas and VipPlotGrid
			if (qobject_cast<VipPlotCanvas*>(items[i]) || qobject_cast<VipPlotGrid*>(items[i]) ||
				qobject_cast<VipPlotShape*>(items[i]) || qobject_cast<VipResizeItem*>(items[i]))
				continue;
			//find the best background color
			QColor c = items[i]->majorColor();
			if (c == Qt::transparent)
				continue;

			QPointF pos = items[i]->drawSelectionOrderPosition(m_font, m_align, bounding);
			pos = area()->mapFromItem(items[i], pos);
			VipText text(QString::number(order));
			text.setFont(m_font);
			text.setTextPen(QPen(Qt::white));
			text.setBackgroundBrush(QBrush(items[i]->majorColor()));
			text.setBorderPen(QPen(items[i]->majorColor()));
			text.draw(painter, pos);
			order++;
		}
	}
}





VipPlotAreaFilter::~VipPlotAreaFilter()
{
	if (VipAbstractPlotArea * area = qobject_cast<VipAbstractPlotArea*>(d_area))
		area->rubberBand()->d_data->filter = NULL;
}









/// Additional legend objects used in VipAbstractPlotArea
struct Legend
{
	QPointer<VipLegend> legend;
	QObject * olegend;
	Qt::Alignment alignment;
	int border_margin;
	bool moved;
	Legend(VipLegend * l = NULL, Qt::Alignment align = Qt::AlignTop | Qt::AlignHCenter, int border_margin = 10)
		:legend(l), olegend(l), alignment(align), border_margin(border_margin), moved(false)
	{}

	bool operator==(const Legend & other) { return legend == other.legend; }
	bool operator==(const VipLegend * other) { return legend == other; }
	bool operator!=(const Legend & other) { return legend != other.legend; }
	bool operator!=(const VipLegend * other) { return legend != other; }
};








static void updateCacheMode(VipAbstractPlotArea * w, bool useCache)
{
#ifndef VIP_CUSTOM_ITEM_CACHING
	(void)w;
	(void)useCache;
#else // VIP_CUSTOM_ITEM_CACHING
	if (useCache) {
		if (QPixmapCache::cacheLimit() < 100000)
			QPixmapCache::setCacheLimit(120000);
	}
	if (!w)
		return;
	const QList<VipAbstractScale*> scales = w->allScales();
	for (int i = 0; i < scales.size(); ++i) {
		QGraphicsItem::CacheMode mode = scales[i]->cacheMode();
		if (useCache && mode != QGraphicsItem::DeviceCoordinateCache)
			scales[i]->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
		else if (!useCache && mode != QGraphicsItem::NoCache)
			scales[i]->setCacheMode(QGraphicsItem::NoCache);
	}
#endif
}















static QWindow* window()
{
	//Create a QWindow
	QWindow* win = NULL;
	if (!win) {
		QSurfaceFormat format = QSurfaceFormat::defaultFormat();
		win = new QWindow();
		win->setSurfaceType(QWindow::OpenGLSurface);
		win->setFormat(format);
		win->create();
	}
	return win;
}
static QWindow* globalWindow()
{
	//Create a QWindow
	static QWindow* win = NULL;
	if (!win) {
		QSurfaceFormat format = QSurfaceFormat::defaultFormat();
		win = new QWindow();
		win->setSurfaceType(QWindow::OpenGLSurface);
		win->setFormat(format);
		win->create();
	}
	return win;
}

static QOpenGLContext* context()
{
	//Create a QOpenGLContext
	QOpenGLContext* ctx = NULL;
	if (!ctx) {
		QSurfaceFormat format = QSurfaceFormat::defaultFormat();
		ctx = new QOpenGLContext();
		ctx->setFormat(format);
		if (!ctx->create()) {
			VIP_LOG_WARNING("Cannot create the requested OpenGL context!");
			delete ctx;
			ctx = NULL;
		}
	}
	return ctx;
}
static QOpenGLContext* globalContext()
{
	//Create a QOpenGLContext
	static QOpenGLContext* ctx = NULL;
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		QSurfaceFormat format = QSurfaceFormat::defaultFormat();
		ctx = new QOpenGLContext();
		ctx->setFormat(format);
		if (!ctx->create()) {
			VIP_LOG_WARNING("Cannot create the requested OpenGL context!");
			delete ctx;
			return ctx = NULL;
		}
	}
	return ctx;
}

static QOpenGLFramebufferObject* createBuffer(QOpenGLFramebufferObject* buf, const QSize& size)
{
	//Create/reset a QOpenGLFramebufferObject with given size
	if (!buf || buf->size().width() < size.width() || buf->size().height() < size.height()) { // buf->size() != size
		if (buf) {
			delete buf;
		}

		QOpenGLFramebufferObjectFormat fboFormat;
		fboFormat.setSamples(QSurfaceFormat::defaultFormat().samples());
		fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
		buf = new QOpenGLFramebufferObject(size, fboFormat);
	}
	return buf;
}

static QOpenGLFramebufferObject* globalBuffer(const QSize& size)
{
	static QOpenGLFramebufferObject* buf = NULL;
	//Create/reset a QOpenGLFramebufferObject with given size
	if (!buf || buf->size().width() < size.width() || buf->size().height() < size.height()) { // 
		if (buf) {
			delete buf;
		}

		QOpenGLFramebufferObjectFormat fboFormat;
		fboFormat.setSamples(QSurfaceFormat::defaultFormat().samples());
		fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
		buf = new QOpenGLFramebufferObject(size, fboFormat);
	}
	return buf;
}



struct ImageOrPixmap
{
	QImage image;
	QPixmap pixmap;
	ImageOrPixmap() {}
	ImageOrPixmap(const QImage& img)
		:image(img) {}
	ImageOrPixmap(const QPixmap& pix)
		:pixmap(pix) {}
	QPaintDevice* device() {
		if (!image.isNull())
			return &image;
		return &pixmap;
	}
	bool isNull() const { return image.isNull() && pixmap.isNull(); }
	QSize size() const {
		if (!image.isNull())
			return image.size();
		return pixmap.size();
	}
	void draw(QPainter* p, const QRectF& dst) const {
		if (!image.isNull())
			p->drawImage(dst, image);
		else
			p->drawPixmap(dst.toRect(), pixmap);
	}
};







class VipAbstractPlotArea::PrivateData
{
public:
	PrivateData()
	{

		blegend = new VipBorderLegend(VipBorderLegend::Bottom);
		blegend->setMargin(0);
		blegend->setZValue(10);
		blegend->setExpandToCorners(true);
		blegend->setCanvasProximity(10);
		blegend->setZValue(50);

		legend = new VipLegend();
		legend->layout()->setMaxColumns(5);
		legend->setLegendItemRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
		blegend->setLegend(legend);

		grid = new VipPlotGrid();
		grid->enableAxisMin(0, false);
		grid->enableAxisMin(1, false);
		grid->setZValue(100);
		QPen pen(Qt::DotLine);
		pen.setColor(Qt::gray);
		grid->setPen(pen);
		grid->setTitle(QString("Axes grid"));
		// grid->setDrawSelected(false);

		canvas = new VipPlotCanvas();
		// canvas->boxStyle().setBackgroundBrush(QBrush(Qt::white));
		canvas->setZValue(-1);

		items << canvas << grid;

		title = new VipAxisBase(VipAxisBase::Top);
		title->setExpandToCorners(true);
		title->setCanvasProximity(10);
		title->setMargin(0);
		title->setSpacing(0);
		title->setZValue(1000);
		title->scaleDraw()->enableComponent(VipAbstractScaleDraw::Backbone, false);
		title->scaleDraw()->enableComponent(VipAbstractScaleDraw::Ticks, false);
		title->scaleDraw()->enableComponent(VipAbstractScaleDraw::Labels, false);

		plotToolTip = NULL;
		trackScalesStateEnabled = false;
		defaultLabelOverlapping = false;
		isMousePanning = false;
		firstMousePanning = true;
		mousePanning = Qt::NoButton;
		mouseZoomSelection = Qt::NoButton;
		mouseItemSelection = Qt::NoButton;
		mouseWheelZoom = false;
		zoomMultiplier = (1.15);
		mouseSelectionAndZoom = false;
		mouseSelectionAndZoomMinimumSize = QSizeF(10, 10);
		mousePos = Vip::InvalidPoint;
		dirtyGeometry = false;
		dirty = false;
		markNeedUpdate = false;
		markGeometryDirty = false;
		isGeometryUpdateEnabled = true;
		insideUpdate = false;
		insideComputeScaleDiv = false;

		maxFPS = 60;
		maxMS = 16.67;
		lastUpdate = 0;
		updateTimer.setSingleShot(true);
		dcount = 0;

		maximumScalesStates = 50;
	}

	QPointer<VipRubberBand> rubberBand;
	QPointer<VipDrawSelectionOrder> drawSelection;
	VipAxisBase* title;
	VipPlotGrid* grid;
	VipPlotCanvas* canvas;
	VipBorderLegend* blegend;
	VipLegend* legend;
	QPointer<VipPlotItem> hoverItem;

	VipMargins aligned_margins;

	QList<Legend> legends;

	QPointer<VipPlotItem> lastPressed;

	QPointer<VipToolTip> plotToolTip;
	QList<VipAbstractScale*> scales;
	QList<VipPlotItem*> items;
	QList<scales_state> scalesStates;
	QList<scales_state> redoScalesStates;
	int maximumScalesStates;

	QRectF boundingRect;

	bool isMousePanning;
	bool firstMousePanning;
	Qt::MouseButton mousePanning;
	Qt::MouseButton mouseZoomSelection;
	Qt::MouseButton mouseItemSelection;
	bool mouseWheelZoom;
	bool trackScalesStateEnabled;
	bool defaultLabelOverlapping;
	double zoomMultiplier;
	QPointF mousePress;
	QPointF mousePos;
	QPointF mouseEndPos;

	bool mouseSelectionAndZoom;
	QSizeF mouseSelectionAndZoomMinimumSize;

	bool dirtyGeometry;

	// update management
	bool markNeedUpdate;
	bool isGeometryUpdateEnabled;
	int markGeometryDirty;
	bool insideUpdate;
	bool insideComputeScaleDiv;
	QSet<VipAbstractScale*> dirtyScaleDiv;
	bool dirty;

	int dcount;

	int maxFPS;
	int maxMS;
	qint64 lastUpdate;
	QTimer updateTimer;


	VipColorPalette colorPalette;
	QString colorPaletteName;
	QString colorMapName;

};

















static int registerAbstractAreaKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {
		QMap<QByteArray, int> mousebutton;

		mousebutton["leftButton"] = Qt::LeftButton;
		mousebutton["rightButton"] = Qt::RightButton;
		mousebutton["middleButton"] = Qt::MiddleButton;

		QMap<QByteArray, int> position;
		position["none"] = Vip::detail::LegendNone;
		position["left"] = Vip::detail::LegendLeft;
		position["right"] = Vip::detail::LegendRight;
		position["top"] = Vip::detail::LegendTop;
		position["bottom"] = Vip::detail::LegendBottom;
		position["innerLeft"] = Vip::detail::LegendInnerLeft;
		position["innerRight"] = Vip::detail::LegendInnerRight;
		position["innerTop"] = Vip::detail::LegendInnerTop;
		position["innerBottom"] = Vip::detail::LegendInnerBottom;
		position["innerTopRight"] = Vip::detail::LegendInnerTopRight;
		position["innerTopLeft"] = Vip::detail::LegendInnerTopLeft;
		position["innerBottomRight"] = Vip::detail::LegendInnerBottomRight;
		position["innerBottomLeft"] = Vip::detail::LegendInnerBottomLeft;
		
		keywords["mouse-selection-and-zoom"] = VipParserPtr(new BoolParser());
		keywords["mouse-panning"] = VipParserPtr(new EnumParser(mousebutton));
		keywords["mouse-zoom-selection"]= VipParserPtr(new EnumParser(mousebutton));
		keywords["mouse-item-selection"] = VipParserPtr(new EnumParser(mousebutton));
		keywords["mouse-wheel-zoom"] = VipParserPtr(new BoolParser());
		keywords["zoom-multiplier"] = VipParserPtr(new DoubleParser());
		keywords["maximum-frame-rate"] = VipParserPtr(new DoubleParser());
		keywords["draw-selection-order"] = VipParserPtr(new BoolParser());
		//keywords["colormap"] = VipParserPtr(new EnumParser(colorMap));
		keywords["colorpalette"] = VipParserPtr(new EnumOrStringParser(VipStandardStyleSheet::colorPaletteEnum()));
		keywords["colormap"] = VipParserPtr(new EnumOrStringParser(VipStandardStyleSheet::colormapEnum()));
		keywords["margins"] = VipParserPtr(new DoubleParser());
		keywords["tool-tip-selection-border"] = VipParserPtr(new PenParser());
		keywords["tool-tip-selection-background"] = VipParserPtr(new ColorParser());
		keywords["track-scales-state"] = VipParserPtr(new BoolParser());
		keywords["maximum-scales-states"] = VipParserPtr(new DoubleParser());

		keywords["legend-position"] = VipParserPtr(new EnumParser(position));
		keywords["legend-border-distance"] = VipParserPtr(new DoubleParser());
		
		vipSetKeyWordsForClass(&VipAbstractPlotArea::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerAbstractAreaKeyWords = registerAbstractAreaKeyWords();







VipAbstractPlotArea::VipAbstractPlotArea(QGraphicsItem * parent)
	: VipBoxGraphicsWidget()
{

	d_data = new PrivateData();
	d_data->rubberBand = new VipRubberBand(this);
	d_data->drawSelection = new VipDrawSelectionOrder(this);

	addScale(d_data->title, false);
	addScale(d_data->blegend, false);

	this->setParentItem(parent);
	this->setFlag(ItemIsSelectable, false);
	this->setFlag(ItemIsFocusable, true);
	this->setAcceptHoverEvents(true);
	this->setAcceptDrops(false);

	//d_data->title->setVisible(false);
	d_data->title->setObjectName("title");
	d_data->title->setProperty("_vip_title", true);
	d_data->legend->setObjectName("legend");
	d_data->legend->setProperty("_vip_legend", true);
	d_data->grid->setObjectName("grid");
	d_data->canvas->setObjectName("canvas");

	d_data->grid->setItemAttribute(VipPlotItem::Droppable, false);
	d_data->canvas->setItemAttribute(VipPlotItem::Droppable, false);

	connect(d_data->grid, SIGNAL(mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonPressed(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->grid, SIGNAL(mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonMoved(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->grid, SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonReleased(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->grid, SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonDoubleClicked(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->grid, SIGNAL(keyPress(VipPlotItem*, qint64, int, int)), this, SLOT(keyPressed(VipPlotItem*, qint64, int, int)));
	connect(d_data->grid, SIGNAL(keyRelease(VipPlotItem*, qint64, int, int)), this, SLOT(keyReleased(VipPlotItem*, qint64, int, int)));
	connect(d_data->grid, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(receiveChildChanged(VipPlotItem*)), Qt::DirectConnection);
	connect(d_data->grid, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(receiveChildSelectionChanged(VipPlotItem*)), Qt::DirectConnection);
	connect(d_data->grid, SIGNAL(dropped(VipPlotItem*, QMimeData*)), this, SLOT(receiveDropped(VipPlotItem*, QMimeData*)), Qt::DirectConnection);

	connect(d_data->canvas, SIGNAL(mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonPressed(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->canvas, SIGNAL(mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonMoved(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->canvas, SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonReleased(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->canvas, SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonDoubleClicked(VipPlotItem*, VipPlotItem::MouseButton)));
	connect(d_data->canvas, SIGNAL(keyPress(VipPlotItem*, qint64, int, int)), this, SLOT(keyPressed(VipPlotItem*, qint64, int, int)));
	connect(d_data->canvas, SIGNAL(keyRelease(VipPlotItem*, qint64, int, int)), this, SLOT(keyReleased(VipPlotItem*, qint64, int, int)));
	connect(d_data->canvas, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(receiveChildChanged(VipPlotItem*)), Qt::DirectConnection);
	connect(d_data->canvas, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(receiveChildSelectionChanged(VipPlotItem*)), Qt::DirectConnection);
	connect(d_data->canvas, SIGNAL(dropped(VipPlotItem*, QMimeData*)), this, SLOT(receiveDropped(VipPlotItem*, QMimeData*)), Qt::DirectConnection);

	connect(&d_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateInternal()));
	//for now comment this as it triggers too many recomputeGeometry
	//connect(this, SIGNAL(childItemChanged(VipPlotItem*)), this, SLOT(recomputeGeometry()), Qt::QueuedConnection);

}

VipAbstractPlotArea::~VipAbstractPlotArea()
{
	removeSharedAlignedArea(this);
	delete d_data;
}

QRectF VipAbstractPlotArea::visualizedSceneRect() const
{
	QGraphicsScene * sc = scene();
	if (sc)
	{
		QList<QGraphicsView*> views = sc->views();
		if (views.size())
		{
			return VipBorderItem::visualizedSceneRect(views.front());
		}
	}

	return QRectF();
}

void VipAbstractPlotArea::markNeedUpdate()
{
	if (d_data->insideUpdate)
		return;

	d_data->markNeedUpdate = true;
	if (!d_data->dirty) {

		d_data->dirty = true;
		qint64 current = QDateTime::currentMSecsSinceEpoch();
		if (current - d_data->lastUpdate > d_data->maxMS) {
			updateInternal();
		}
		else {
			d_data->updateTimer.start(d_data->maxMS - (current - d_data->lastUpdate));
		}
	}
}
void VipAbstractPlotArea::updateInternal()
{
	d_data->lastUpdate = QDateTime::currentMSecsSinceEpoch();
	update();
}

void VipAbstractPlotArea::markScaleDivDirty(VipAbstractScale * sc)
{
	if (d_data->insideUpdate)
		return;

	d_data->dirtyScaleDiv.insert(sc);
	if (!d_data->dirty) {
		d_data->dirty = true;
		qint64 current = QDateTime::currentMSecsSinceEpoch();
		if (current - d_data->lastUpdate > d_data->maxMS) {
			updateInternal();
		}
		else {
			d_data->updateTimer.start(d_data->maxMS - (current - d_data->lastUpdate));
		}
	}
}

void VipAbstractPlotArea::setGeometryUpdateEnabled(bool enable) 
{
	d_data->isGeometryUpdateEnabled = enable;
}

bool VipAbstractPlotArea::markGeometryDirty()
{
	if (!d_data->isGeometryUpdateEnabled)
		return false;

	d_data->markGeometryDirty = 2;
	if (d_data->insideUpdate) {
		if (!d_data->insideComputeScaleDiv)
			return false;
		return true;
	}
	if (!d_data->dirty) {
		d_data->dirty = true;
		qint64 current = QDateTime::currentMSecsSinceEpoch();
		if (current - d_data->lastUpdate > d_data->maxMS) {
			updateInternal();
		}
		else {
			d_data->updateTimer.start(d_data->maxMS - (current - d_data->lastUpdate));
		}
	}
	return true;
}




#define MODE_OPENGL 1
#define MODE_RASTER 0

static QImage createImageWithFBO(int mode, //QOpenGLFramebufferObject ** buffer, QOpenGLContext * context, QWindow * window,
 const QList<QGraphicsItem*> & items, const QGraphicsItem * parent)
{
	static const int max_width = 4000;

	QTransform tr;
	QRectF bounding = parent->boundingRect();
	QPointF topLeft = parent->boundingRect().topLeft();
	tr.translate(-topLeft.x(), -topLeft.y());
	//limit image size to max_width*max_width
	QSize s = bounding.size().toSize();
	double max = qMax(bounding.width(), bounding.height());
	if (max > max_width) {
		if (max == bounding.width()) {
			double factor = max_width / bounding.width();
			tr.scale(factor, factor);
			s.setWidth(max_width);
			s.setHeight(bounding.height() * factor);
		}
		else {
			double factor = max_width / bounding.height();
			tr.scale(factor, factor);
			s.setHeight(max_width);
			s.setWidth(bounding.width() * factor);
		}
	}

	//qint64 st = QDateTime::currentMSecsSinceEpoch();

	QImage res;
	if (mode == MODE_OPENGL) {


		if (!globalContext())
			return res;
		globalContext()->makeCurrent(globalWindow());
		QOpenGLFramebufferObject * buffer = globalBuffer(s);
		//printf("opengl: %s\n", (const char*)globalContext()->functions()->glGetString(GL_VERSION));
		if (!(globalContext()->functions()->openGLFeatures() & QOpenGLFunctions::Shaders)) {
			return QImage();
		}

		(buffer)->bind();

		QOpenGLPaintDevice device(s);
		QPainter painter;
		painter.begin(&device);

		painter.beginNativePainting();
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		painter.endNativePainting();
		painter.setTransform(tr,false);

		
		for (int i = 0; i < items.size(); ++i) {
			if (!items[i]->isVisible())
				continue;
			painter.save();
			QPointF p = items[i]->pos();
			QTransform t; t.translate(p.x(), p.y());
			t *= items[i]->transform();
			painter.setTransform(t, true);
			QGraphicsItem* o = items[i];
			o->paint(&painter, NULL, NULL);
			painter.restore();
		}

		painter.end();

		//qint64 st = QDateTime::currentMSecsSinceEpoch();
		const QImage tmp = (buffer)->toImage();
		//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		//printf("toImage: %i ms %i\n", (int)el, (int)buffer->hasOpenGLFramebufferBlit());

		res = tmp.copy(QRect(QPoint(0, tmp.height() - s.height()), s));
		//qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st;
		//printf("to image: %i %i ms\n", (int)el,(int)el2);
		(buffer)->release();


	}
	else {
		QImage img(s.width(), s.height(), QImage::Format_ARGB32);
		{
			QPainter painter(&img);
			painter.setTransform(tr);
			for (int i = 0; i < items.size(); ++i) {
				if (!items[i]->isVisible())
					continue;
				painter.save();
				QPointF p = items[i]->pos();
				QTransform t; t.translate(p.x(), p.y());
				t *= items[i]->transform();
				painter.setTransform(t, true);
				items[i]->paint(&painter, NULL, NULL);
				painter.restore();
			}
		}
		res = img;
	}

	//qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st;



	//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//printf("opengl: %i , %i ms\n", (int)el, (int)el2);
	return res;
}


QImage	VipAbstractPlotArea::renderOpengl(const QList<VipPaintItem*>& items) const
{
	QList<QGraphicsItem*> objs;
	for (VipPaintItem* it : items) {
		it->setPaintingEnabled(true);
		if (QGraphicsObject* o = it->graphicsObject())
			objs.push_back(o);
	}
	
	//render
	//qint64 st = QDateTime::currentMSecsSinceEpoch();
	const QImage res =  createImageWithFBO(MODE_OPENGL, objs, this);
	//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//printf("renderOpengl: %i\n", (int)el);
	return res;
}

QImage	VipAbstractPlotArea::renderRaster(const QList<VipPaintItem*> &items) const
{
	QList<QGraphicsItem*> objs;
	for (VipPaintItem* it : items) {
		it->setPaintingEnabled(true);
		if (QGraphicsObject* o = it->graphicsObject())
			objs.push_back(o);
	}
	//render
	return createImageWithFBO(MODE_RASTER, objs, this);
}


void	VipAbstractPlotArea::doUpdateScaleLogic()
{
	d_data->insideUpdate = true;
	d_data->insideComputeScaleDiv = true;
	bool need_update = d_data->markNeedUpdate;

	if (d_data->dirtyScaleDiv.size()) {

		{
			//compute scale div first, that might trigger a geometry update
			QList<VipAbstractScale*> scales = VipAbstractScale::independentScales(d_data->dirtyScaleDiv.values());

			for (int i = 0; i < scales.size(); ++i) {
				scales[i]->computeScaleDiv();
			}

			//for (QSet<VipAbstractScale*>::iterator it = d_data->dirtyScaleDiv.begin(); it != d_data->dirtyScaleDiv.end(); ++it) {
			// (*it)->computeScaleDiv();
			// }
			need_update = true;
			d_data->dcount = 0;
		}


	}

	d_data->insideComputeScaleDiv = false;

	if (d_data->markGeometryDirty-- > 0 || d_data->boundingRect != boundingRect()) {
		d_data->boundingRect = boundingRect();
		recomputeGeometry();
		need_update = true;
		if (d_data->rubberBand) {
			d_data->rubberBand->updateGeometry();
		}
	}

	// refresh tool tip
	//if (VipToolTip* tip = plotToolTip())
	//	tip->refresh();

	d_data->insideUpdate = false;
	d_data->dirty = false;
	if (d_data->markGeometryDirty < 0)
		d_data->markGeometryDirty = 0;
	d_data->markNeedUpdate = false;
	d_data->dirtyScaleDiv.clear();

}

/*
static int __renderThreads = 1;

void VipAbstractPlotArea::setRenderingThreads(int count)
{
	if (__renderThreads != count && count > 0)
	{
		__renderThreads = count;
		// compute screen resolution as it will crash if done for the first time in non GUI thread
		VipPainter::screenResolution();
	}
}

int VipAbstractPlotArea::renderingThreads()
{
	return __renderThreads;
}

bool VipAbstractPlotArea::paintOpenGLInternal(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	//for opengl widget, use paint()
	if (VipPainter::isOpenGL(painter) || VipPainter::isVectoriel(painter) || __renderThreads < 2)
		return false;
	
	if (renderPool().numberOfThreads() != __renderThreads) 
		renderPool().setNumberOfThreads(__renderThreads);

	if (d_data->markNeedUpdate) {
		//qint64 started = QDateTime::currentMSecsSinceEpoch();

		// trigger all areas that need update
		QList< VipAbstractPlotArea*> areas;
		for (int i = 0; i < allAreas.size(); ++i) {
			VipAbstractPlotArea* area = allAreas[i];
			if (area->d_data->markNeedUpdate) {
				area->doUpdateScaleLogic();
				if (area->isVisible() && area->view()->isVisible())
					areas.append(area);
			}
		}
		//qint64 st = QDateTime::currentMSecsSinceEpoch();
		renderPool().push(areas);
		//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		//printf("push: %i ms\n", (int)el);
	}
	


	// draw itself
	VipBoxGraphicsWidget::paint(painter, option, widget);

	const ImageOrPixmap img_opengl = renderPool().retrieve(this);
	
	if (!img_opengl.isNull()) {
		painter->save();
		QRect r = boundingRect().toRect();
		if (r.size() == img_opengl.size()) {
			painter->setRenderHints(QPainter::RenderHints());
			img_opengl.draw(painter, r);
		}
		else {
			painter->setRenderHint(QPainter::SmoothPixmapTransform);
			img_opengl.draw(painter, boundingRect());
		}
		painter->restore();
	}

	return true;
}*/

void	VipAbstractPlotArea::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
	/* bool opengl = VipPainter::isOpenGL(painter);
	if (widget && !opengl && __renderThreads > 1 && paintOpenGLInternal(painter, option, widget))
		return;

	if (renderPool().numberOfThreads() != __renderThreads)
		renderPool().setNumberOfThreads(__renderThreads);*/

	doUpdateScaleLogic() ;

	//draw itself
	VipBoxGraphicsWidget::paint(painter, option, widget);


	//draw children with opengl only when drawing on a non opengl widget
	/* if (widget && !VipPainter::isOpenGL(painter) && ((d_data->renderStrategy == AutoStrategy) || (d_data->renderStrategy == OpenGLOffscreen))) {
		QImage img_raster;
		QImage img_opengl;
		//compute the list of children VipPaintItem
		QList<VipPaintItem*> items = paintItemChildren();
		

		if ((d_data->renderStrategy == AutoStrategy) && d_data->dirtyComputeStrategy == 1) {
			// Note that d_data->dirtyComputeStrategy is 1 when dirty, 2 when non dirty and Default strategy is best,
			// 3 when non dirty and OpenGLOffscreen strategy is best

			//render a first time to initialize opengl context which takes time
			if (d_data->testOpenGL) {
				d_data->testOpenGL = false;
				renderOpengl(items);
			}

			qint64 st = QDateTime::currentMSecsSinceEpoch();
			img_raster = renderRaster(items);
			qint64 el_raster = QDateTime::currentMSecsSinceEpoch() - st;
			st = QDateTime::currentMSecsSinceEpoch();
			img_opengl = renderOpengl(items);
			qint64 el_opengl = QDateTime::currentMSecsSinceEpoch() - st;
			if (el_opengl < el_raster && !img_opengl.isNull()) {
				d_data->dirtyComputeStrategy = 3;
				//printf("opengl better\n");
			}
			else {
				d_data->dirtyComputeStrategy = 2;
				//printf("raster better\n");
			}
			//d_data->dirtyComputeStrategy = 3;
		}
		if ((d_data->renderStrategy == OpenGLOffscreen) || d_data->dirtyComputeStrategy == 3) {
			if (img_opengl.isNull()) {
				//qint64 st = QDateTime::currentMSecsSinceEpoch();
				img_opengl = renderOpengl(items);
				//qint64 el_opengl = QDateTime::currentMSecsSinceEpoch() - st;
				//printf("opengl: %i\n", (int)el_opengl);
			}
			painter->save();

			QRectF r = boundingRect().toRect();
			if(r.size().toSize() == img_opengl.size())
				painter->drawImage(r, img_opengl); //avoid a pixmap transform if possible
			else {
				painter->setRenderHint(QPainter::SmoothPixmapTransform);
				painter->drawImage(boundingRect(), img_opengl);
			}
			painter->restore();

			//disable painting opengl items
			for (int i = 0; i < items.size(); ++i) {
				items[i]->setPaintingEnabled(false);
				
			}
		}
		else {
			//enable all items for painting
			for (int i = 0; i < items.size(); ++i)
				items[i]->setPaintingEnabled(true);
		}
	}
	else if ((d_data->renderStrategy == OpenGLOffscreen) || d_data->dirtyComputeStrategy == 3) {
		//enable all items for painting
		QList<VipPaintItem*> items = paintItemChildren();
		for (VipPaintItem* it : items)
			it->setPaintingEnabled(true);
	}*/

	//qint64 el = QDateTime::currentMSecsSinceEpoch()-st;
	//printf("%i ms, %s\n",(int)el, this->metaObject()->className());
}


void VipAbstractPlotArea::applyLabelOverlapping()
{
	QVector< QSharedPointer<QPainterPath> > overlapps;
	for (int i = 0; i < d_data->scales.size(); ++i)
		overlapps << d_data->scales[i]->constScaleDraw()->thisLabelArea();
	for (int i = 0; i < d_data->scales.size(); ++i) {
		if (!d_data->scales[i]->scaleDraw()->labelOverlappingEnabled()) {
			QVector< QSharedPointer<QPainterPath> > copy = overlapps;
			copy.removeAt(i);
			d_data->scales[i]->scaleDraw()->clearAdditionalLabelOverlapp();
			d_data->scales[i]->scaleDraw()->setAdditionalLabelOverlapp(copy);
		}
	}
}

//void VipAbstractPlotArea::recomputeIfDirty()
// {
// if (d_data->dirtyGeometry)
// {
// d_data->dirtyGeometry = false;
// recomputeGeometry();
// }
// }
//
// void VipAbstractPlotArea::delayRecomputeGeometry()
// {
// d_data->dirtyGeometry = true;
// QMetaObject::invokeMethod(this, "recomputeIfDirty", Qt::QueuedConnection);
// }

void VipAbstractPlotArea::installFilter(VipPlotAreaFilter * filter)
{
	rubberBand()->installFilter(filter);
}

void VipAbstractPlotArea::removeFilter()
{
	rubberBand()->removeFilter();
}

VipPlotAreaFilter * VipAbstractPlotArea::filter() const
{
	return rubberBand()->filter();
}

VipInterval VipAbstractPlotArea::areaBoundaries(const VipAbstractScale * scale) const
{
	if (const VipBorderItem * item = qobject_cast<const VipBorderItem*>(scale))
	{
		QRectF r = this->innerArea().boundingRect();
		if (item->orientation() == Qt::Vertical)
			return VipInterval(
				scale->value(scale->mapFromItem(this, r.topLeft())),
				scale->value(scale->mapFromItem(this, r.bottomLeft()))).normalized();
		else
			return VipInterval(
				scale->value(scale->mapFromItem(this, r.bottomLeft())),
				scale->value(scale->mapFromItem(this, r.bottomRight()))).normalized();
	}
	return VipInterval();
}

void VipAbstractPlotArea::setMouseSelectionAndZoom(bool enable)
{
	d_data->mouseSelectionAndZoom = enable;
}
bool  VipAbstractPlotArea::mouseSelectionAndZoom() const
{
	return d_data->mouseSelectionAndZoom;
}
void  VipAbstractPlotArea::setMouseSelectionAndZoomMinimumSize(const QSizeF & s)
{
	d_data->mouseSelectionAndZoomMinimumSize = s;
}
QSizeF  VipAbstractPlotArea::mouseSelectionAndZoomMinimumSize() const
{
	return d_data->mouseSelectionAndZoomMinimumSize;
}

void VipAbstractPlotArea::setMousePanning(Qt::MouseButton button)
{
	d_data->mousePanning = button;
}

Qt::MouseButton VipAbstractPlotArea::mousePanning() const
{
	return d_data->mousePanning;
}

bool VipAbstractPlotArea::isMousePanning() const
{
	return d_data->isMousePanning;
}

void VipAbstractPlotArea::setMouseZoomSelection(Qt::MouseButton button)
{
	d_data->mouseZoomSelection = button;
}

void VipAbstractPlotArea::setMouseItemSelection(Qt::MouseButton button)
{
	d_data->mouseItemSelection = button;
}

Qt::MouseButton VipAbstractPlotArea::mouseItemSelection() const
{
	return d_data->mouseItemSelection;
}

Qt::MouseButton VipAbstractPlotArea::mouseZoomSelection() const
{
	return d_data->mouseZoomSelection;
}

void VipAbstractPlotArea::setMouseWheelZoom(bool enable)
{
	d_data->mouseWheelZoom = enable;
}

bool VipAbstractPlotArea::mouseWheelZoom() const
{
	return d_data->mouseWheelZoom;
}

void VipAbstractPlotArea::setZoomMultiplier(double mult)
{
	d_data->zoomMultiplier = mult;
}

double VipAbstractPlotArea::zoomMultiplier() const
{
	return d_data->zoomMultiplier;
}

void VipAbstractPlotArea::setZoomEnabled(VipAbstractScale * sc, bool enable)
{
	sc->setProperty("zoom_enabled", enable);
	QSet<VipAbstractScale*> scales = sc->synchronizedWith();
	for (QSet<VipAbstractScale*>::iterator it = scales.begin(); it != scales.end(); ++it)
	{
		(*it)->setProperty("zoom_enabled", enable);
	}
}

bool VipAbstractPlotArea::zoomEnabled(VipAbstractScale * sc)
{
	if (!sc)
		return true;
	QVariant p = sc->property("zoom_enabled");
	if (p.userType() == 0)
		return true;
	return p.toBool();
}


void VipAbstractPlotArea::setMaximumFrameRate(int fps)
{
	d_data->maxFPS = fps;
	d_data->maxMS = static_cast<int>( (1. / fps) * 1000.);
}
int VipAbstractPlotArea::maximumFrameRate() const
{
	return d_data->maxFPS;
}

void VipAbstractPlotArea::setRubberBand(VipRubberBand * rubberBand)
{
	if (d_data->rubberBand != rubberBand)
	{
		if (d_data->rubberBand)
			delete d_data->rubberBand;
		if (rubberBand)
		{
			d_data->rubberBand = rubberBand;
			rubberBand->setArea(this);
		}
		else
			d_data->rubberBand = new VipRubberBand(this);
	}
}

VipRubberBand * VipAbstractPlotArea::rubberBand() const
{
	return const_cast<VipRubberBand*>(d_data->rubberBand.data());
}

void VipAbstractPlotArea::setDrawSelectionOrder(VipDrawSelectionOrder * drawSelection)
{
	if (d_data->drawSelection != drawSelection)
	{
		if (d_data->drawSelection)
			delete d_data->drawSelection;
		
		d_data->drawSelection = drawSelection;
		if (drawSelection)
			drawSelection->setArea(this);
		
	}
}

VipDrawSelectionOrder * VipAbstractPlotArea::drawSelectionOrder() const
{
	return const_cast<VipDrawSelectionOrder*>(d_data->drawSelection.data());
}


void VipAbstractPlotArea::setColorMap(const QString& name) 
{
	d_data->colorMapName = name;
	QGradientStops map = VipLinearColorMap::createGradientStops(name.toLatin1().data());

	if (map.isEmpty())
		return;

	//apply to each color map
	QList<VipAxisColorMap*> axes = this->findItems<VipAxisColorMap*>();
	for (auto it = axes.begin(); it != axes.end(); ++it)
		(*it)->setColorMap((*it)->colorMapInterval(), VipLinearColorMap::createColorMap(map));
}
QString VipAbstractPlotArea::colorMap() const
{
	return d_data->colorMapName;
}

void VipAbstractPlotArea::setColorPalette(const QString& name) 
{
	QGradientStops map = VipLinearColorMap::createGradientStops(name.toLatin1().data());

	if (map.isEmpty()) {
		setColorPalette(VipColorPalette());
		d_data->colorPaletteName.clear();
	}
	else {
		setColorPalette(VipColorPalette(map));
		d_data->colorPaletteName = name;
	}
}

void VipAbstractPlotArea::setColorPalette(const VipColorPalette& palette) 
{
	d_data->colorPaletteName.clear();
	d_data->colorPalette = palette;
	if (palette.count() == 0)
		return;

	applyColorPalette();
}
QString VipAbstractPlotArea::colorPaletteName() const
{
	return d_data->colorPaletteName;
}

VipColorPalette VipAbstractPlotArea::colorPalette() const
{
	return d_data->colorPalette;
}

void VipAbstractPlotArea::applyColorPalette() 
{
	if (d_data->colorPalette.count() == 0)
		return;

	QList<VipPlotItem*> items = this->findItems<VipPlotItem*>();
	QMap<int, VipPlotItem*> sorted;
	for (int i=0; i < items.size(); ++i) {
		VipPlotItem* it = items[i];

		if (qobject_cast<VipPlotCanvas*>(it) || qobject_cast<VipPlotGrid*>(it) || it->ignoreStyleSheet()) {
			items.removeAt(i);
			--i;
			continue;
		}
		
		it->setColorPalette(d_data->colorPalette);
		it->markStyleSheetDirty();

		QVariant v = it->property("_vip_index");
		if (!v.isNull()) {
			int id = v.toInt();
			if (sorted.find(id) == sorted.end()) {
				sorted.insert(id, it);
				items.removeAt(i);
				--i;

				it->setMajorColor(d_data->colorPalette.color(id));
				it->markStyleSheetDirty();
			}
		}
	}

	// set id to remaining items
	for (int i = 0; i < items.size(); ++i) {
		// find next id
		int id = sorted.size();
		int start = 0;
		for (auto it = sorted.begin(); it != sorted.end(); ++it, ++start)
			if (start != it.key()) {
				id = start;
				break;
			}
		//insert
		sorted.insert(id, items[i]);
		items[i]->setMajorColor(d_data->colorPalette.color(id));
		items[i]->markStyleSheetDirty();
		items[i]->setProperty("_vip_index", id);
	}
}

void VipAbstractPlotArea::setMargins(const VipMargins & m)
{
	this->setProperty("margins", QVariant::fromValue(m));
}

void VipAbstractPlotArea::setMargins(const QRectF & rect)
{
	QRectF bounding = boundingRect();
	QRectF r = rect & bounding;
	setMargins(VipMargins(r.left() - bounding.left(), r.top() - bounding.top(), bounding.right() - r.right(), bounding.bottom() - r.bottom()));
}

VipMargins VipAbstractPlotArea::margins() const
{
	QVariant v = this->property("margins");
	return v.value<VipMargins>();
}

VipPlotGrid * VipAbstractPlotArea::grid() const
{
	return const_cast<VipPlotGrid*>(d_data->grid);
}

VipPlotCanvas * VipAbstractPlotArea::canvas() const
{
	return const_cast<VipPlotCanvas*>(d_data->canvas);
}

VipBorderLegend * VipAbstractPlotArea::borderLegend() const
{
	return const_cast<VipBorderLegend*>(d_data->blegend);
}

void VipAbstractPlotArea::setLegend(VipLegend * legend, bool own)
{
	if (d_data->legend != legend)
	{
		QList<VipPlotItem*> items = plotItems();

		if (d_data->legend)
		{
			for (int i = 0; i < items.size(); ++i)
				d_data->legend->removeItem(items[i]);
		}

		if (legend)
		{
			for (int i = 0; i < items.size(); ++i)
				d_data->legend->addItem(items[i]);
		}

		if (d_data->legend && d_data->legend->parentItem() == d_data->blegend)
		{
			delete d_data->legend;
		}

		d_data->legend = legend;

		if (own)
		{
			d_data->blegend->setLegend(d_data->legend);
		}
		else
		{
			d_data->blegend->setLegend(NULL);
		}

	}
}

VipLegend * VipAbstractPlotArea::legend() const
{
	return const_cast<VipLegend*>(d_data->legend);
}

void VipAbstractPlotArea::addInnerLegend(VipLegend * legend,  Qt::Alignment alignment, int border_margin)
{
	addInnerLegend(legend, NULL, alignment, border_margin);
}
VipAbstractScale * VipAbstractPlotArea::scaleForlegend(VipLegend * l) const
{
	return l->property("_vip_scale").value<VipAbstractScale*>();
}

void VipAbstractPlotArea::legendDestroyed(QObject* l)
{
	bool removed = false;
	for (int i = 0; i < d_data->legends.size(); ++i) {
		if (l == d_data->legends[i].olegend) {
			d_data->legends.removeAt(i);
			--i;
			removed = true;
		}
	}
	if(removed)
		resetInnerLegendsPosition();
}

void VipAbstractPlotArea::addInnerLegend(VipLegend * legend, VipAbstractScale * scale, Qt::Alignment alignment, int border_margin)
{
	if (d_data->legends.indexOf(legend) < 0) {
		legend->setCheckState(d_data->legend->checkState());
		legend->setDisplayMode(d_data->legend->displayMode());
		legend->setLegendItemSpacing(d_data->legend->legendItemSpacing());
		legend->setLegendItemLeft(d_data->legend->legendItemLeft());
		legend->setLegendItemRenderHints(d_data->legend->legendItemRenderHints());
		legend->setLegendItemBoxStyle(d_data->legend->legendItemBoxStyle());
		legend->setLegendItemTextStyle(d_data->legend->legendItemTextStyle());
		//if (d_data->legend->hasLegendTextColor())
		//	legend->setLegendTextColor(d_data->legend->legendTextColor());
		//if (d_data->legend->hasLegendTextFont())
		//	legend->setLegendTextFont(d_data->legend->legendTextFont());
		legend->setFlag(QGraphicsItem::ItemIsMovable, true);
		//the legend should be always on top
		legend->setZValue(std::numeric_limits<double>::max());
		legend->setProperty("_vip_scale", QVariant::fromValue(scale));
		legend->setProperty("_vip_inner", true);
		d_data->legends.append(Legend(legend, alignment, border_margin));
		legend->setParentItem(scale ? (QGraphicsObject*)scale : (QGraphicsObject*)this);		

		//add existing items
		QList<VipPlotItem*> items;
		if (scale)
			items = scale->plotItems();
		else
			items = this->plotItems();
		for (int i = 0; i < items.size(); ++i) {
			if (items[i]->testItemAttribute(VipPlotItem::HasLegendIcon)) {
				legend->addItem(items[i]);
			}
		}

		connect(legend, SIGNAL(destroyed(QObject*)), this, SLOT(legendDestroyed(QObject*)));

		resetInnerLegendsPosition();
	}
}

VipLegend* VipAbstractPlotArea::takeInnerLegend(VipLegend * legend)
{
	int i = d_data->legends.indexOf(legend);
	if (i >= 0) {
		VipLegend * l = d_data->legends[i].legend;
		d_data->legends.removeAt(i);
		l->setProperty("_vip_inner", QVariant());
		resetInnerLegendsPosition();
		return l;
	}
	return NULL;
}
void VipAbstractPlotArea::removeInnerLegend(VipLegend * legend)
{
	VipLegend * l = takeInnerLegend(legend);
	if (l)
		l->deleteLater();
}

void VipAbstractPlotArea::setInnerLegendAlignment(int index, Qt::Alignment align)
{
	d_data->legends[index].alignment = align;
	resetInnerLegendsPosition();
}
void VipAbstractPlotArea::setInnerLegendMargin(int index, int border_margin)
{
	d_data->legends[index].border_margin = border_margin;
	resetInnerLegendsPosition();
}

QList<VipLegend*> VipAbstractPlotArea::innerLegends() const
{
	QList<VipLegend*> res;
	for (int i = 0; i < d_data->legends.size(); ++i)
		res.append(d_data->legends[i].legend);
	return res;
}
int VipAbstractPlotArea::innerLegendCount() const
{
	return d_data->legends.size();
}
VipLegend * VipAbstractPlotArea::innerLegend(int index) const
{
	return d_data->legends[index].legend;
}
Qt::Alignment VipAbstractPlotArea::innerLegendAlignment(int index) const
{
	return d_data->legends[index].alignment;
}

int VipAbstractPlotArea::innerLegendMargin(int index) const
{
	return d_data->legends[index].border_margin;
}

void VipAbstractPlotArea::setTitle(const VipText & t)
{
	d_data->title->setTitle(t);
	VipBoxGraphicsWidget::setTitle(t);
}

VipAxisBase * VipAbstractPlotArea::titleAxis() const
{
	return const_cast<VipAxisBase*>(d_data->title);
}

void VipAbstractPlotArea::setDefaultLabelOverlapping(bool enable)
{ 
	d_data->defaultLabelOverlapping = enable;
}
bool VipAbstractPlotArea::defaultLabelOverlapping() const
{
	return d_data->defaultLabelOverlapping;
}



bool VipAbstractPlotArea::internalAddScale(VipAbstractScale * scale, bool //isSpatialCoordinate
)
{
	scale->setParentItem(this);
	updateCacheMode(this, view() ? qobject_cast<QOpenGLWidget*>(view()->viewport()) != NULL : false);
	return true;
}

void VipAbstractPlotArea::addScale(VipAbstractScale * scale, bool isSpatialCoordinate)
{

	if (scale->parentItem() != this)
	{

		if (!internalAddScale(scale, isSpatialCoordinate))
			return;

		scale->scaleDraw()->enableLabelOverlapping(defaultLabelOverlapping());
//		scale->setUpdater(this->updater());

		//connect(scale, SIGNAL(geometryNeedUpdate()), this, SLOT(delayRecomputeGeometry()), Qt::DirectConnection);


		connect(scale, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(addItem(VipPlotItem*)), Qt::DirectConnection);
		connect(scale, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(removeItem(VipPlotItem*)), Qt::DirectConnection);
		connect(scale, SIGNAL(titleChanged(const VipText&)), this, SLOT(receiveTitleChanged(const VipText&)), Qt::DirectConnection);
		if (isSpatialCoordinate)
		{
			d_data->scales << scale;
			//add the items related to this scale
			PlotItemList items = scale->plotItems();
			for (int i = 0; i < items.size(); ++i)
				if (items[i] && d_data->items.indexOf(items[i]) < 0) {
					d_data->items.append(items[i]);
//					d_data->items[i]->setUpdater(this->updater());
				}
		}

		scale->setZValue(grid()->zValue() + 1);

		Q_EMIT scaleAdded(scale);
	}

	applyLabelOverlapping();
	markGeometryDirty();
}

bool VipAbstractPlotArea::internalRemoveScale(VipAbstractScale * scale)
{
	if (scale->parentItem() == this)
	{
		scale->setParentItem(NULL);
	}
	return true;
}

void VipAbstractPlotArea::removeScale(VipAbstractScale * scale)
{

	if (!internalRemoveScale(scale))
		return;

	disconnect(scale, SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(addItem(VipPlotItem*)));
	disconnect(scale, SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(removeItem(VipPlotItem*)));
	disconnect(scale, SIGNAL(titleChanged(const VipText&)), this, SLOT(receiveTitleChanged(const VipText&)));
	//disconnect(scale, SIGNAL(geometryNeedUpdate()), this, SLOT(delayRecomputeGeometry()));

	if (d_data->scales.removeOne(scale))
	{
		//remove the items related to this scale
		QList<VipPlotItem*> items = scale->plotItems();
		for (int i = 0; i < items.size(); ++i) {
			d_data->items.removeOne(items[i]);
		}

		Q_EMIT scaleRemoved(scale);
	}

	applyLabelOverlapping();
	markGeometryDirty();
}

QList<VipAbstractScale*> VipAbstractPlotArea::scales() const
{
	return d_data->scales;
}


QList<VipAbstractScale*> VipAbstractPlotArea::allScales() const
{
	const QList<QGraphicsItem*> items = this->childItems();
	return vipCastItemList<VipAbstractScale*>(items);
}

VipAbstractPlotArea::scales_state VipAbstractPlotArea::scalesState() const
{
	scales_state state;
	QList<VipAbstractScale*> scales = VipAbstractScale::independentScales(this->scales());
	for (int i = 0; i < scales.size(); ++i)
		state[scales[i]] = scales[i]->scaleDiv().bounds().normalized();
	return state;
}

void VipAbstractPlotArea::setScalesState(const scales_state & state)
{
	QList<VipAbstractScale*> scales = VipAbstractScale::independentScales(this->scales());
	for (int i = 0; i < scales.size(); ++i)
	{
		VipAbstractScale * sc = scales[i];
		scales_state::const_iterator it = state.find(sc);
		if (it != state.end())
			sc->setScale(it->minValue(), it->maxValue());
	}
}

void VipAbstractPlotArea::setTrackScalesStateEnabled(bool enable)
{
	d_data->trackScalesStateEnabled = enable;
	d_data->scalesStates.clear();
	d_data->redoScalesStates.clear();
}
bool VipAbstractPlotArea::isTrackScalesStateEnabled() const
{
	return d_data->trackScalesStateEnabled;
}

int VipAbstractPlotArea::maximumScalesStates() const
{
	return d_data->maximumScalesStates;
}

void VipAbstractPlotArea::setMaximumScalesStates(int max) 
{
	if (max < 1)
		max = 1;

	if (d_data->maximumScalesStates != max) {
		d_data->maximumScalesStates = max;
		while (d_data->scalesStates.size() > max)
			d_data->scalesStates.pop_front();
	}
}

void VipAbstractPlotArea::bufferScalesState()
{
	if (d_data->trackScalesStateEnabled)
	{
		scales_state st = scalesState();
		if (d_data->scalesStates.isEmpty() || d_data->scalesStates.last() != st)
		{
			d_data->scalesStates.append(st);
			if (d_data->scalesStates.size() > d_data->maximumScalesStates)
				d_data->scalesStates.pop_front();
		}
	}
}

void VipAbstractPlotArea::undoScalesState()
{
	if (d_data->scalesStates.size())
	{
		d_data->redoScalesStates.append(scalesState());
		if (d_data->redoScalesStates.size() > d_data->maximumScalesStates)
			d_data->redoScalesStates.pop_front();
		scales_state st = d_data->scalesStates.last();
		d_data->scalesStates.pop_back();
		setScalesState(st);
	}
}

void VipAbstractPlotArea::redoScalesState()
{
	if (d_data->redoScalesStates.size())
	{
		bufferScalesState();

		scales_state st = d_data->redoScalesStates.last();
		d_data->redoScalesStates.pop_back();
		setScalesState(st);
	}
}

const QList<VipAbstractPlotArea::scales_state>  & VipAbstractPlotArea::undoStates() const
{
	return d_data->scalesStates;
}
const QList<VipAbstractPlotArea::scales_state>  & VipAbstractPlotArea::redoStates() const
{
	return d_data->redoScalesStates;
}


QByteArray VipAbstractPlotArea::saveSpatialScaleState() const
{
	QByteArray ar;
	{
		QDataStream str(&ar, QIODevice::WriteOnly);
		str.setByteOrder(QDataStream::LittleEndian);

		//save the number of scales
		str << d_data->scales.size();

		//for each scale, save its title and bounds
		//we also save the fact that vip_double is bigger than double
		for (int i = 0; i < d_data->scales.size(); ++i)
			str << d_data->scales[i]->title().text() << d_data->scales[i]->scaleDiv().bounds();
	}
	return ar;
}

void VipAbstractPlotArea::restoreSpatialScaleState(const QByteArray& state)
{
	QDataStream str(state);
	str.setByteOrder(QDataStream::LittleEndian);
	str.device()->setProperty("_vip_LD", vip_LD_support);

	int count;
	str >> count;

	if (count < 1000) {
		//count > 1000 make no sense

		count = qMin(count, d_data->scales.size());
		for (int i = 0; i < count; ++i) {
			QString title;
			VipInterval inter;
			str >> title >> inter;
			inter = inter.normalized();
			if (title == d_data->scales[i]->title().text()) {
				//set interval only if same title
				d_data->scales[i]->setScale(inter.minValue(), inter.maxValue());
			}
		}
	}
}





bool VipAbstractPlotArea::setItemProperty(const char* name, const QVariant& value, const QByteArray& index) 
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "mouse-selection-and-zoom") == 0) {
		setMouseSelectionAndZoom(value.toBool());
		return true;
	}
	if (strcmp(name, "mouse-panning") == 0) {
		setMousePanning((Qt::MouseButton)value.toInt());
		return true;
	}
	if (strcmp(name, "mouse-zoom-selection") == 0) {
		setMouseZoomSelection((Qt::MouseButton)value.toInt());
		return true;
	}
	if (strcmp(name, "mouse-item-selection") == 0) {
		setMouseItemSelection((Qt::MouseButton)value.toInt());
		return true;
	}
	if (strcmp(name, "mouse-wheel-zoom") == 0) {
		setMouseWheelZoom(value.toBool());
		return true;
	}
	if (strcmp(name, "zoom-multiplier") == 0) {
		setZoomMultiplier(value.toDouble());
		return true;
	}
	if (strcmp(name, "maximum-frame-rate") == 0) {
		setMaximumFrameRate(value.toInt());
		return true;
	}
	if (strcmp(name, "draw-selection-order") == 0) {
		if (value.toBool()) {
			if (!drawSelectionOrder())
				setDrawSelectionOrder(new VipDrawSelectionOrder());
		}
		else {
			if (drawSelectionOrder())
				setDrawSelectionOrder(nullptr);
		}
		return true;
	}
	if (strcmp(name, "colorpalette") == 0) {
		if (value.userType() == QMetaType::QByteArray)
			setColorPalette(value.toByteArray());
		else
			setColorPalette(VipColorPalette((VipLinearColorMap::StandardColorMap) value.toInt()));
		return true;
	}
	if (strcmp(name, "colormap") == 0) {

		if (value.userType() == QMetaType::QByteArray)
			setColorMap(value.toByteArray());
		else
			setColorMap(VipLinearColorMap::colorMapToName((VipLinearColorMap::StandardColorMap)value.toInt()));
		return true;
	}
	if (strcmp(name, "margins") == 0) {
		setMargins(value.toDouble());
		return true;
	}
	if (strcmp(name, "tool-tip-selection-border") == 0) {
		if (!plotToolTip())
			setPlotToolTip(new VipToolTip());
		if (value.userType() == qMetaTypeId<QColor>())
			plotToolTip()->setOverlayPen(QPen(value.value<QColor>()));
		else
			plotToolTip()->setOverlayPen(value.value<QPen>());
		return true;
	}
	if (strcmp(name, "tool-tip-selection-background") == 0) {
		if (!plotToolTip())
			setPlotToolTip(new VipToolTip());
		if (value.userType() == qMetaTypeId<QColor>())
			plotToolTip()->setOverlayBrush(QBrush(value.value<QColor>()));
		else
			plotToolTip()->setOverlayBrush(value.value<QBrush>());
		return true;
	}
	
	if (strcmp(name, "legend-position") == 0) {
		setProperty("_vip_legend-position", QVariant::fromValue(value.toInt()));
		resetInnerLegendsStyleSheet();
		return true;
	}
	if (strcmp(name, "legend-border-distance") == 0) {
		borderLegend()->setMargin(value.toDouble());
		resetInnerLegendsStyleSheet();
		return true;
	}
	return VipBoxGraphicsWidget::setItemProperty(name,value,index);
}

void VipAbstractPlotArea::resetInnerLegendsStyleSheet() 
{ 
	QVariant vpos = property("_vip_legend-position");
	if (vpos.isNull())
		return;

	int legend_pos = vpos.toInt();
	if (legend_pos == Vip::detail::LegendNone || legend_pos <= Vip::detail::LegendRight) {
		// remove all inner legends
		for (int i = 0; i <  innerLegendCount(); ++i)
			removeInnerLegend(innerLegend(i));
	}

	if (legend_pos <= Vip::detail::LegendRight) {
		borderLegend()->setAlignment((VipBorderItem::Alignment)legend_pos);
		borderLegend()->setVisible(true);
	}
	else {
		borderLegend()->setVisible(false);
		// compute alignment
		Qt::Alignment align;
		if (legend_pos == Vip::detail::LegendInnerLeft)
			align = Qt::AlignLeft | Qt::AlignVCenter;
		else if (legend_pos == Vip::detail::LegendInnerRight)
			align = Qt::AlignRight | Qt::AlignVCenter;
		else if (legend_pos == Vip::detail::LegendInnerTop)
			align = Qt::AlignTop | Qt::AlignHCenter;
		else if (legend_pos == Vip::detail::LegendInnerBottom)
			align = Qt::AlignBottom | Qt::AlignHCenter;
		else if (legend_pos == Vip::detail::LegendInnerTopLeft)
			align = Qt::AlignLeft | Qt::AlignTop;
		else if (legend_pos == Vip::detail::LegendInnerTopRight)
			align = Qt::AlignRight | Qt::AlignTop;
		else if (legend_pos == Vip::detail::LegendInnerBottomLeft)
			align = Qt::AlignBottom | Qt::AlignLeft;
		else if (legend_pos == Vip::detail::LegendInnerBottomRight)
			align = Qt::AlignBottom | Qt::AlignRight;

		//make sure there is at least one inner legend
		if (innerLegendCount() == 0) {
			addInnerLegend(new VipLegend(), align, qMax(borderLegend()->margin(), 5.));
		}

		// set ALL parameters to ALL inner legends
		for (int i = 0; i < innerLegendCount(); ++i) {
			setInnerLegendAlignment(i,align);
			setInnerLegendMargin(i, qMax(borderLegend()->margin(), 5.));
		}
	}
}



VipAxisColorMap* VipAbstractPlotArea::createColorMap(VipAxisBase::Alignment alignment, const VipInterval & interval, VipColorMap * map)
{
	VipAxisColorMap* axis = new VipAxisColorMap(alignment);
	axis->setCanvasProximity(2);
	axis->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	//axis->boxStyle().setBackgroundBrush(QBrush(Qt::lightGray));
	axis->setRenderHints(QPainter::TextAntialiasing);
	axis->setColorBarEnabled(true);
	axis->setBorderDist(5, 5);
	axis->setScale(interval.minValue(), interval.maxValue());
	axis->setColorMap(interval, map);
	axis->setGripInterval(interval);
	axis->setExpandToCorners(true);
	addScale(axis, false);
	return axis;
}

void VipAbstractPlotArea::addItem(VipPlotItem* item)
{
	//this->blockSignals(true);
	// removeItem(item);
	// this->blockSignals(false);

	if (this->isAutoScale())
		//save the current scales state, before auto scaling apply
		bufferScalesState();

	if (item  && d_data->items.indexOf(item)  < 0)
	{
		d_data->items.append(item);

//		item->setUpdater(this->updater());

		//update main legend
		if (d_data->legend && item->testItemAttribute(VipPlotItem::HasLegendIcon) && item->testItemAttribute(VipPlotItem::VisibleLegend))
		{
			d_data->legend->addItem(item);
		}
		//update additional legends
		for (int i = 0; i < d_data->legends.size(); ++i) {
			if (d_data->legends[i].legend && item->testItemAttribute(VipPlotItem::HasLegendIcon) && item->testItemAttribute(VipPlotItem::VisibleLegend)) {
				VipAbstractScale * sc = scaleForlegend(d_data->legends[i].legend);
				if(!sc || item->axes().indexOf(sc) >= 0)
					d_data->legends[i].legend->addItem(item);
			}
		}
		resetInnerLegendsPosition();

		connect(item, SIGNAL(mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonPressed(VipPlotItem*, VipPlotItem::MouseButton)));
		connect(item, SIGNAL(mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonMoved(VipPlotItem*, VipPlotItem::MouseButton)));
		connect(item, SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonReleased(VipPlotItem*, VipPlotItem::MouseButton)));
		connect(item, SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonDoubleClicked(VipPlotItem*, VipPlotItem::MouseButton)));
		connect(item, SIGNAL(keyPress(VipPlotItem*, qint64, int, int)), this, SLOT(keyPressed(VipPlotItem*, qint64, int, int)));
		connect(item, SIGNAL(keyRelease(VipPlotItem*, qint64, int, int)), this, SLOT(keyReleased(VipPlotItem*, qint64, int, int)));
		connect(item, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(receiveChildChanged(VipPlotItem*)), Qt::DirectConnection);
		connect(item, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(receiveChildSelectionChanged(VipPlotItem*)), Qt::DirectConnection);
		connect(item, SIGNAL(axisUnitChanged(VipPlotItem*)), this, SLOT(receiveChildAxisUnitChanged(VipPlotItem*)), Qt::DirectConnection);
		connect(item, SIGNAL(dropped(VipPlotItem*, QMimeData *)), this, SLOT(receiveDropped(VipPlotItem*, QMimeData*)), Qt::DirectConnection);

		if (qobject_cast<VipPlotItemData*>(item))
		{
			connect(item, SIGNAL(dataChanged()), this, SLOT(receivedDataChanged()), Qt::AutoConnection);
		}

		Q_EMIT itemAdded(item);
	}
}

void VipAbstractPlotArea::removeItem(VipPlotItem* item)
{
	if (this->isAutoScale())
		//save the current scales state, before auto scaling apply
		bufferScalesState();

	if (item)
	{
		
		d_data->items.removeOne(item);
//		if(d_data->items.removeOne(item))
//			item->setUpdater(NULL);

		//update main legend
		if (d_data->legend)
			d_data->legend->removeItem(item);
		//update additional legends
		for (int i = 0; i < d_data->legends.size(); ++i) {
			if (d_data->legends[i].legend) {
				d_data->legends[i].legend->removeItem(item);
			}
		}
		resetInnerLegendsPosition();

		disconnect(item, SIGNAL(mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonPressed(VipPlotItem*, VipPlotItem::MouseButton)));
		disconnect(item, SIGNAL(mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonMoved(VipPlotItem*, VipPlotItem::MouseButton)));
		disconnect(item, SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonReleased(VipPlotItem*, VipPlotItem::MouseButton)));
		disconnect(item, SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonDoubleClicked(VipPlotItem*, VipPlotItem::MouseButton)));
		disconnect(item, SIGNAL(keyPress(VipPlotItem*, qint64, int, int)), this, SLOT(keyPressed(VipPlotItem*, qint64, int, int)));
		disconnect(item, SIGNAL(keyRelease(VipPlotItem*, qint64, int, int)), this, SLOT(keyReleased(VipPlotItem*, qint64, int, int)));
		disconnect(item, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(receiveChildChanged(VipPlotItem*)));
		disconnect(item, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(receiveChildSelectionChanged(VipPlotItem*)));
		disconnect(item, SIGNAL(axisUnitChanged(VipPlotItem*)), this, SLOT(receiveChildAxisUnitChanged(VipPlotItem*)));
		disconnect(item, SIGNAL(dropped(VipPlotItem*, QMimeData *)), this, SLOT(receiveDropped(VipPlotItem*, QMimeData*)));

		if (qobject_cast<VipPlotItemData*>(item))
		{
			disconnect(item, SIGNAL(dataChanged()), this, SLOT(receivedDataChanged()));
		}

		Q_EMIT itemRemoved(item);
	}
}

void VipAbstractPlotArea::receivedDataChanged()
{
	if (VipPlotItem * item = qobject_cast<VipPlotItem*>(sender()))
		Q_EMIT itemDataChanged(item);
}

void VipAbstractPlotArea::resetInnerLegendsPosition()
{
	QList<VipPlotCanvas*> canvas = findItems<VipPlotCanvas*>(QString(), 2, 1);
	QRectF parent;
	for (int i = 0; i < canvas.size(); ++i) {
		parent = parent.united(canvas[i]->mapToItem(this, canvas[i]->boundingRect()).boundingRect());
	}

	double top_space = titleOffset();

	for (int i = 0; i < d_data->legends.size(); ++i)
	{
		if (d_data->legends[i].legend && !d_data->legends[i].moved)
		{
			//compute margin
			double x_margin = 0;
			double y_margin = 0;
			if (d_data->legends[i].border_margin) {
				QPointF p1(0, 0), p2(d_data->legends[i].border_margin, d_data->legends[i].border_margin);
				if (QGraphicsView * v = this->view()) {
					p1 = v->mapToScene(p1.toPoint());
					p2 = v->mapToScene(p2.toPoint());
					p1 = this->mapFromScene(p1);
					p2 = this->mapFromScene(p2);
					x_margin = qAbs(p2.x() - p1.x());
					y_margin = qAbs(p2.y() - p1.y());
				}
			}

			Qt::Alignment align = d_data->legends[i].alignment;
			QSizeF size = d_data->legends[i].legend->effectiveSizeHint(Qt::PreferredSize);

			QPointF pos;
			if (align & Qt::AlignLeft)
				pos.setX(x_margin + parent.left());
			else if (align & Qt::AlignRight)
				pos.setX(parent.right() - size.width() - x_margin);
			else
				pos.setX((parent.width() - size.width()) / 2);

			if (align & Qt::AlignTop)
				pos.setY(y_margin + parent.top() + top_space);
			else if (align & Qt::AlignBottom)
				pos.setY(parent.bottom() - size.height() - y_margin);
			else
				pos.setY((parent.bottom() - size.height()) / 2);

			QRectF geom(pos, size);
			d_data->legends[i].legend->setGeometry(geom);
			//d_data->legends[i].legend->setPos(pos);
		}
	}
}

void VipAbstractPlotArea::mouseButtonPressed(VipPlotItem* item, VipPlotItem::MouseButton button)
{
	d_data->lastPressed = item;
	Q_EMIT mouseButtonPress(item, button);
}

void VipAbstractPlotArea::mouseButtonMoved(VipPlotItem* item, VipPlotItem::MouseButton button)
{
	Q_EMIT mouseButtonMove(item, button);
}

void VipAbstractPlotArea::mouseButtonReleased(VipPlotItem* item, VipPlotItem::MouseButton button)
{
	Q_EMIT mouseButtonRelease(item, button);
}

void VipAbstractPlotArea::mouseButtonDoubleClicked(VipPlotItem*item, VipPlotItem::MouseButton button)
{
	Q_EMIT mouseButtonDoubleClick(item, button);
}

void VipAbstractPlotArea::keyPressed(VipPlotItem * item, qint64 id, int key, int modifiers)
{
	Q_EMIT keyPress(item, id, key, modifiers);
}

void VipAbstractPlotArea::keyReleased(VipPlotItem * item, qint64 id, int key, int modifiers)
{
	Q_EMIT keyRelease(item, id, key, modifiers);
}

void VipAbstractPlotArea::receiveChildChanged(VipPlotItem* item)
{
	Q_EMIT childItemChanged(item);
}

void VipAbstractPlotArea::receiveChildSelectionChanged(VipPlotItem* item)
{
	Q_EMIT childSelectionChanged(item);
}

void VipAbstractPlotArea::receiveChildAxisUnitChanged(VipPlotItem* item)
{
	Q_EMIT childAxisUnitChanged(item);
}

void VipAbstractPlotArea::receiveTitleChanged(const VipText & title)
{
	Q_EMIT titleChanged(title);
}

void VipAbstractPlotArea::receiveDropped(VipPlotItem* item, QMimeData* data)
{
	Q_EMIT dropped(item, data);
}

void VipAbstractPlotArea::setPlotToolTip(VipToolTip* tooltip)
{
	if (tooltip != d_data->plotToolTip)
	{
		if (d_data->plotToolTip)
			delete d_data->plotToolTip;
		d_data->plotToolTip = tooltip;
		if (d_data->plotToolTip)
		{
			d_data->plotToolTip->setPlotArea(this);
		}
	}
}

VipToolTip* VipAbstractPlotArea::plotToolTip() const
{
	return d_data->plotToolTip;
}

void VipAbstractPlotArea::refreshToolTip()
{
	if (d_data->plotToolTip && VipCorrectedTip::isVisible())
		d_data->plotToolTip->refresh();
}

void	VipAbstractPlotArea::simulateMouseClick(const QGraphicsSceneMouseEvent * event)
{
	//send mouse press and release event
	GraphicsSceneMouseEvent *pressed = new GraphicsSceneMouseEvent(QEvent::GraphicsSceneMousePress, NULL, false);
	GraphicsSceneMouseEvent *released = new GraphicsSceneMouseEvent(QEvent::GraphicsSceneMouseRelease, this, false);
	pressed->import(*event);
	released->import(*event);
	_inSimulate = true;
	QApplication::postEvent(scene(), pressed);
	QApplication::postEvent(scene(), released);
}

void	VipAbstractPlotArea::hoverEnterEvent(QGraphicsSceneHoverEvent * event)
{
	QPointF pos = event->pos();
	if (canvas()->shape().contains(pos)) //inside canvas
		Q_EMIT toolTipStarted((pos));

	event->ignore();
}

void	VipAbstractPlotArea::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
	if (d_data->rubberBand)
		d_data->rubberBand->setAdditionalPaintCommands(QPicture());
	Q_EMIT toolTipEnded((event->pos()));
	event->ignore();
}

void VipAbstractPlotArea::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{

	if (d_data->rubberBand)
		d_data->rubberBand->setAdditionalPaintCommands(QPicture());
	Q_EMIT toolTipEnded((event->pos()));

	if ((event->buttons() & d_data->mousePanning) && d_data->mousePos != Vip::InvalidPoint)
	{
		if (d_data->firstMousePanning)
		{
			Q_EMIT  mouseScaleAboutToChange();
			bufferScalesState();
			d_data->firstMousePanning = false;
		}

		QPointF pos = d_data->mousePos;
		d_data->mousePos = event->pos();
		this->translate(event->pos(), event->pos() - pos);
		recomputeGeometry();

	}
	else if ((event->buttons() & d_data->mouseZoomSelection) && d_data->mousePos != Vip::InvalidPoint)
	{
		d_data->mouseEndPos = event->pos();
		d_data->rubberBand->setRubberBandEnd(event->pos());
		d_data->rubberBand->setCursor(QCursor(Qt::CrossCursor));
		this->update();
	}
	else if ((event->buttons() & d_data->mouseItemSelection) && d_data->mousePos != Vip::InvalidPoint)
	{
		d_data->mouseEndPos = event->pos();
		d_data->rubberBand->setRubberBandEnd(event->pos());
		this->update();
	}
	else
		event->ignore();

}

QList<VipAbstractScale*> VipAbstractPlotArea::scalesForPos(const QPointF & pos) const
{
	QRectF r = canvas()->shape().boundingRect();
	if (canvas()->shape().contains(pos))
		return VipAbstractScale::independentScales(scales());
	return QList<VipAbstractScale*>();
}

void	VipAbstractPlotArea::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{

	QPointF pos = event->pos();
	//bool inside_canvas = canvas()->shape().contains(pos);
	QList<VipAbstractScale*> tool_tip_scales = scalesForPos(pos);

	if (tool_tip_scales.size())
	{

		Qt::MouseButtons buttons = QApplication::mouseButtons();

		//display tool tip
		if (d_data->plotToolTip && buttons == 0)
		{
			if (d_data->plotToolTip->plotArea() != this)
				d_data->plotToolTip->setPlotArea(this);
			d_data->plotToolTip->setScales(tool_tip_scales);
			d_data->plotToolTip->setPlotAreaPos(event->pos());
			Q_EMIT toolTipMoved(event->pos());
		}

		//Since the rubber band accept hover events, they won't be propagated to underlying items.
		//So we manually handle hover move, enter and leave events to simulate the standard behavior.

		//first, find the top most VipPlotItem under the mouse (if any)
		VipPlotItem * pitem = NULL;
		QList<QGraphicsItem *> items = scene()->items(event->scenePos());
		for (int i = 0; i < items.size(); ++i)
		{
			pitem = qobject_cast<VipPlotItem*>(items[i]->toGraphicsObject());
			if (pitem)
				break;
		}

		if (pitem)
		{
			//handle hover events
			if (pitem != d_data->hoverItem)
			{
				if (d_data->hoverItem)
					d_data->hoverItem->hoverLeaveEvent(event);
				pitem->hoverEnterEvent(event);
			}
			else
				pitem->hoverMoveEvent(event);

			d_data->hoverItem = pitem;
		}
		else
		{
			//no VipPlotItem under the mouse, send a hover leave event to the last VipPlotItem under the mouse (if any)
			if (d_data->hoverItem)
				d_data->hoverItem->hoverLeaveEvent(event);
			d_data->hoverItem = NULL;
		}

		Q_EMIT mouseHoverMove(d_data->hoverItem);
	}
	else
	{
		//mouse outside the canvas: send a hover leave event to the last VipPlotItem under the mouse (if any)
		if (d_data->hoverItem)
			d_data->hoverItem->hoverLeaveEvent(event);

		//reset additional drawing
		if (d_data->rubberBand)
			d_data->rubberBand->setAdditionalPaintCommands(QPicture());
		Q_EMIT toolTipEnded((event->pos()));
	}
}

QPointF VipAbstractPlotArea::lastMousePressPos() const
{
	return d_data->mousePress;
}

QGraphicsView * VipAbstractPlotArea::view() const
{
	if (scene()) {
		QList<QGraphicsView *> views = scene()->views();
		if (views.size())
			return views.first();
	}
	return NULL;
}

VipPlotItem* VipAbstractPlotArea::lastPressed() const
{
	return d_data->lastPressed;
}

bool VipAbstractPlotArea::mouseInUse() const
{
	return vipIsValid( d_data->mousePos);
}

void VipAbstractPlotArea::setAlignedWith(VipAbstractPlotArea* other, Qt::Orientation align_orientation)
{
	addSharedAlignedArea(this, other, align_orientation);
}
	
QList<VipAbstractPlotArea*> VipAbstractPlotArea::alignedWith(Qt::Orientation align_orientation) const
{
	return sharedAlignedAreas(this, align_orientation).values();
}
void VipAbstractPlotArea::removeAlignment(Qt::Orientation align_orientation)
{
	removeSharedAlignedArea(this, align_orientation);
}

void VipAbstractPlotArea::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
	d_data->firstMousePanning = true;
	d_data->mousePress = event->pos();

	//bool inside_canvas = canvas()->shape().contains(event->pos());
	QList<VipAbstractScale*> scales = scalesForPos(event->pos());

	if ((event->button() & d_data->mousePanning) && scales.size() //inside_canvas
)
	{
		d_data->mousePos = event->pos();
		if (d_data->rubberBand)
			d_data->rubberBand->setCursor(QCursor(Qt::ClosedHandCursor));
		d_data->isMousePanning = true;

	}
	else if ((event->button() & d_data->mouseZoomSelection) //&& inside_canvas
)
	{
		d_data->mousePos = event->pos();
		if (d_data->rubberBand)
		{
			d_data->rubberBand->setRubberBandStart(event->pos());
			//d_data->rubberBand->setCursor(QCursor(Qt::CrossCursor));
		}
	}
	else if ((event->button() & d_data->mouseItemSelection) //&& inside_canvas
)
	{
		d_data->mousePos = event->pos();
		if (d_data->rubberBand)
		{
			d_data->rubberBand->setRubberBandStart(event->pos());
			d_data->rubberBand->setCursor(QCursor(Qt::CrossCursor));
		}
	}
	else
	{
		
		event->ignore();
	}
}

void VipAbstractPlotArea::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
	if (d_data->isMousePanning)
		Q_EMIT endMousePanning();
	d_data->isMousePanning = false;
	d_data->firstMousePanning = true;

	//if (testItemAttribute(IgnoreMouseEvents))
	// {
	// event->ignore();
	// return;
	// }

	d_data->mousePos = Vip::InvalidPoint;

	if (event->button() & d_data->mouseZoomSelection)
	{
		if ((d_data->rubberBand && d_data->rubberBand->hasRubberBandArea()) || mouseSelectionAndZoom())
		{
			if (mouseSelectionAndZoom() &&
				d_data->rubberBand->rubberBandWidth() < d_data->mouseSelectionAndZoomMinimumSize.width() &&
				d_data->rubberBand->rubberBandHeight() < d_data->mouseSelectionAndZoomMinimumSize.height()) {
				//apply selection
				simulateMouseClick(event);
			}
			else if (d_data->rubberBand->hasRubberBandArea()) {
				//apply zooming
				Q_EMIT  mouseScaleAboutToChange();
				bufferScalesState();
				this->zoomOnSelection(d_data->rubberBand->rubberBandStart(), d_data->rubberBand->rubberBandEnd());
				recomputeGeometry();
			}
			Q_EMIT endMouseZooming();

		}
		//else  if(event->pos() == event->buttonDownPos(event->button() ))//TEST
		//	event->ignore();
		if (d_data->rubberBand)
		{
			d_data->rubberBand->setCursor(QCursor(Qt::ArrowCursor));
			d_data->rubberBand->resetRubberBand();
		}
	}
	else if (event->button() & d_data->mouseItemSelection)
	{
		//find items inside selection
		if (d_data->rubberBand && d_data->rubberBand->hasRubberBandArea())
		{
			//select or unselect items under mouse
			bool ctrl_down = (event->modifiers() & Qt::ControlModifier);
			QRectF selection = QRectF(d_data->rubberBand->rubberBandStart(), d_data->rubberBand->rubberBandEnd()).normalized();
			PlotItemList lst = this->plotItems();

			for (int i = 0; i < lst.size(); ++i)
			{
				VipPlotItem* item = lst[i];
				//items under selection area
				if (selection.intersects(item->shape().boundingRect()))
				{
					bool was_selected = item->isSelected();
					bool selected = true;
					if (was_selected && ctrl_down)
						selected = false;
					//if (ctrl_down) selected = !selected;
					item->setSelected(selected);
				}
				else if (!ctrl_down)
				{
					//if item is not under selection area, unselect it unless CTRL is down
					item->setSelected(false);
				}
			}
		}
		else
		{
			//if no selection, simulate mouse click for standard selection behavior
			simulateMouseClick(event);
		}
		if (d_data->rubberBand)
		{
			d_data->rubberBand->setCursor(QCursor(Qt::ArrowCursor));
			d_data->rubberBand->resetRubberBand();
		}
	}
	else if (event->button() & d_data->mousePanning)
	{
		if (d_data->rubberBand)
			d_data->rubberBand->setCursor(QCursor(Qt::ArrowCursor));
		double len = (event->pos() - d_data->mousePress).manhattanLength();
		if (len < 7)//event->buttonDownPos(event->button() ))
		{
			simulateMouseClick(event);
			//event->ignore();
		}
		else
			event->ignore();
	}
	else
	{
		event->ignore();
	}

	d_data->mousePress = Vip::InvalidPoint;
}
void 	VipAbstractPlotArea::wheelEvent(QGraphicsSceneWheelEvent * event)
{
	if (!mouseWheelZoom())
	{
		event->ignore();
		return;
	}

	Q_EMIT  mouseScaleAboutToChange();
	bufferScalesState();

	if (d_data->rubberBand)
		d_data->rubberBand->setAdditionalPaintCommands(QPicture());

	if (event->delta() > 0) {
		//zoom in
		zoomOnPosition(event->pos(), zoomMultiplier());
	}
	else {
		//zoom out
		zoomOnPosition(event->pos(), 1 / zoomMultiplier());

	}

	Q_EMIT endMouseWheel();

	//TODO: remove call twice (now only works properly with 2 calls)
	recomputeGeometry();
	recomputeGeometry();

}

QVariant VipAbstractPlotArea::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value) 
{
	if (change == QGraphicsItem::ItemChildAddedChange)
		applyColorPalette();
	return VipBoxGraphicsWidget::itemChange(change, value);
}

VipPoint VipAbstractPlotArea::positionToScale(const QPointF & pos, bool *ok) const
{
	QList<VipAbstractScale*> scales;
	standardScales(scales);
	if (scales.size() == 2)
	{
		vip_double x = scales.first()->scaleDraw()->value(scales.first()->mapFromItem(this, pos));
		vip_double y = scales.last()->scaleDraw()->value(scales.last()->mapFromItem(this, pos));
		VipPoint res(x, y);
		if (ok) *ok = true;
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return VipPoint();
	}
}

VipPoint VipAbstractPlotArea::positionToScale(const QPointF & pos, const QList<VipAbstractScale*> & scales, bool *ok) const
{
	if (scales.size() == 2)
	{
		vip_double x = scales.first()->scaleDraw()->value(scales.first()->mapFromItem(this, pos));
		vip_double y = scales.last()->scaleDraw()->value(scales.last()->mapFromItem(this, pos));
		VipPoint res(x, y);
		if (ok) *ok = true;
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return VipPoint();
	}
}

QPointF VipAbstractPlotArea::scaleToPosition(const VipPoint & scale_value, bool *ok) const
{
	QList<VipAbstractScale*> scales;
	standardScales(scales);
	if (scales.size() == 2)
	{
		double x = this->mapFromItem(scales.first(), scales.first()->scaleDraw()->position(scale_value.x())).x();
		double y = this->mapFromItem(scales.last(), scales.last()->scaleDraw()->position(scale_value.y())).y();
		QPointF res(x, y);
		if (ok) *ok = true;
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return QPointF();
	}
}

QPointF VipAbstractPlotArea::scaleToPosition(const VipPoint & scale_value, const QList<VipAbstractScale*> & scales, bool *ok) const
{
	if (scales.size() == 2)
	{
		double x = this->mapFromItem(scales.first(), scales.first()->scaleDraw()->position(scale_value.x())).x();
		double y = this->mapFromItem(scales.last(), scales.last()->scaleDraw()->position(scale_value.y())).y();
		QPointF res(x, y);
		if (ok) *ok = true;
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return QPointF();
	}
}

VipPointVector VipAbstractPlotArea::positionToScale(const QVector<QPointF> & positions, bool *ok) const
{
	QList<VipAbstractScale*> scales;
	standardScales(scales);
	if (scales.size() == 2)
	{
		if (ok) *ok = true;
		VipPointVector res(positions.size());
		VipAbstractScale * scale_x = scales.first();
		VipAbstractScale * scale_y = scales.last();
		for (int i = 0; i < positions.size(); ++i)
		{
			const vip_double x = scale_x->scaleDraw()->value(scale_x->mapFromItem(this, positions[i]));
			const vip_double y = scale_y->scaleDraw()->value(scale_y->mapFromItem(this, positions[i]));
			res[i] = VipPoint(x, y);
		}
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return VipPointVector();
	}
}

VipPointVector VipAbstractPlotArea::positionToScale(const QVector<QPointF> & positions, const QList<VipAbstractScale*> & scales, bool *ok) const
{
	if (scales.size() == 2)
	{
		if (ok) *ok = true;
		VipPointVector res(positions.size());
		VipAbstractScale * scale_x = scales.first();
		VipAbstractScale * scale_y = scales.last();
		for (int i = 0; i < positions.size(); ++i)
		{
			const vip_double x = scale_x->scaleDraw()->value(scale_x->mapFromItem(this, positions[i]));
			const vip_double y = scale_y->scaleDraw()->value(scale_y->mapFromItem(this, positions[i]));
			res[i] = VipPoint(x, y);
		}
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return VipPointVector();
	}
}

QVector<QPointF> VipAbstractPlotArea::scaleToPosition(const VipPointVector & scale_values, bool *ok) const
{
	QList<VipAbstractScale*> scales;
	standardScales(scales);
	if (scales.size() == 2)
	{
		if (ok) *ok = true;
		QVector<QPointF> res(scale_values.size());
		VipAbstractScale * scale_x = scales.first();
		VipAbstractScale * scale_y = scales.last();
		for (int i = 0; i < scale_values.size(); ++i)
		{
			const double x = this->mapFromItem(scale_x, scale_x->scaleDraw()->position(scale_values[i].x())).x();
			const double y = this->mapFromItem(scale_y, scale_y->scaleDraw()->position(scale_values[i].y())).y();
			res[i] = QPointF(x, y);
		}
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return QVector<QPointF>();
	}
}

QVector<QPointF> VipAbstractPlotArea::scaleToPosition(const VipPointVector & scale_values, const QList<VipAbstractScale*> & scales, bool *ok) const
{
	if (scales.size() == 2)
	{
		if (ok) *ok = true;
		QVector<QPointF> res(scale_values.size());
		VipAbstractScale * scale_x = scales.first();
		VipAbstractScale * scale_y = scales.last();
		for (int i = 0; i < scale_values.size(); ++i)
		{
			const double x = this->mapFromItem(scale_x, scale_x->scaleDraw()->position(scale_values[i].x())).x();
			const double y = this->mapFromItem(scale_y, scale_y->scaleDraw()->position(scale_values[i].y())).y();
			res[i] = QPointF(x, y);
		}
		return res;
	}
	else
	{
		if (ok) *ok = false;
		return QVector<QPointF>();
	}
}

void VipAbstractPlotArea::setAutoScale(bool auto_scale)
{
	if (auto_scale)
		bufferScalesState();

	QList<VipAbstractScale*> sc = scales();
	for (int i = 0; i < sc.size(); ++i)
	{
		sc[i]->setAutoScale(auto_scale);
	}
	Q_EMIT autoScaleChanged(auto_scale);
}

bool VipAbstractPlotArea::isAutoScale() const
{
	QList<VipAbstractScale*> sc = scales();
	for (int i = 0; i < sc.size(); ++i)
	{
		if (!sc[i]->isAutoScale())
			return false;
	}
	return true;
}

void VipAbstractPlotArea::enableAutoScale()
{
	setAutoScale(true);
}

void VipAbstractPlotArea::disableAutoScale()
{
	setAutoScale(false);
}

QList<VipPlotItem*> VipAbstractPlotArea::plotItems(const QPointF & pos) const
{
	QList<VipPlotItem*> res;

	const bool valid_pos = vipIsValid(pos);

	for (QList<VipPlotItem*>::iterator it = d_data->items.begin(); it != d_data->items.end(); ++it)//QSet<VipPlotItem*>::iterator it = set.begin(); it != set.end(); ++it)
	{
		VipPlotItem * item = *it;
		if (valid_pos && item->shape().contains(pos))
			res << item;
		else if (!valid_pos)
			res << item;
	}

	return res;
}



PlotItemList VipAbstractPlotArea::plotItems(const QPointF & pos, int axis, double maxDistance, QList<VipPointVector> & out_points, VipBoxStyleList & out_styles, QList<int> & out_legends) const
{
	PlotItemList res;

	for (int i = 0; i < d_data->items.size(); ++i)
	{
		if (VipPlotItem * item = d_data->items[i])
		{
			VipPointVector out;
			VipBoxStyle st;
			int legend_index = -1;
			QPointF item_pos = mapToItem(item, pos);
			bool r = item->areaOfInterest(item_pos, axis, maxDistance, out, st, legend_index);

			if (r || item->shape().contains(item_pos))
			{
				res << item;
				out_points << out;
				out_styles << st;
				out_legends << legend_index;
			}
		}
	}

	return res;
}











/// \internal
/// Compute the geometry for VipBorderItem
class ComputeBorderGeometry
{

	/// Returns the inner and outer plotting area (if any).
	void computeRects(const VipMargins & margins)
	{
		outerRect = parent->boundingRect();
		innerRect = outerRect;
		innerRect.setLeft(innerRect.left() + left);
		innerRect.setRight(innerRect.right() - right);
		innerRect.setTop(innerRect.top() + top);
		innerRect.setBottom(innerRect.bottom() - bottom);
		outerRect.adjust(margins.left, margins.top, -margins.right, -margins.bottom);
		innerRect.adjust(margins.left, margins.top, -margins.right, -margins.bottom);
	}

public:

	QGraphicsWidget * parent;
	QList<VipBorderItem*> linkedBorders;
	QMap<VipBorderItem*, QPair<double, double> > offsets;
	QMap<VipBorderItem*, double> extents;
	QMap<VipBorderItem*, double> parentExtents;
	double left;
	double right;
	double top;
	double bottom;
	QRectF innerRect;
	QRectF outerRect;

	ComputeBorderGeometry()
		: parent(NULL), left(0), right(0), top(0), bottom(0) {}


	void computeItemsGeometry(const VipMargins & margins)
	{
		//const VipBorderItem* item = static_cast<const VipBorderItem*>(b_item);

		VipMargins marg = margins;

		//compute linkedBorders items
		//linkedBorders = linkedItems<VipBorderItem>();

		//compute borders length and offsets


		left = right = top = bottom = 0;

		//first, compute the inner rect without border items
		computeRects(marg);

		VipMargins m; //potential margins due to border dist hint

		for (int i = 0; i < 2; ++i)
		{
			left = right = top = bottom = 0;

			//compute the different extents in 2 passes
			for (QList<VipBorderItem*>::const_iterator it = linkedBorders.begin(); it != linkedBorders.end(); ++it)
			{
				VipBorderItem * item = *it;
				if (!(*it)->isVisible() || (*it)->axisIntersectionEnabled() || item->property("_vip_ignore_geometry").toBool())
					continue;


				QRectF r;
				if ((*it)->expandToCorners())
					r = outerRect;
				else
					r = innerRect;

				double length;
				if ((*it)->orientation() == Qt::Vertical)
					length = r.height();
				else
					length = r.width();

				//if ((*it)->ignoreTransformations())
				//	length = (*it)->view()->mapFromScene(QPoint(length, 0.)).x() - (*it)->view()->mapFromScene(QPoint(0, 0.)).x();


				double extent = (*it)->extentForLength(length);
				extents[(*it)] = extent;


				double parentExtent = extent;
				//if ((*it)->ignoreTransformations())
				//	parentExtent = (*it)->view()->mapToScene(QPoint(parentExtent, 0.)).x() - (*it)->view()->mapToScene(QPoint(0, 0.)).x();

				parentExtents[(*it)] = parentExtent;

				//ignore scales inside a VipMultiAxisBase when computing the inner rect
				if (VipMultiAxisBase::fromScale(item))
					parentExtent = 0;

				double start_dist = 0, end_dist = 0;
				//compute total length for each sides
				if ((*it)->alignment() == VipBorderItem::Left)
				{
					//TODO: we replaced (*it)->scaleDraw()->getBorderDistHint with (*it)->getBorderDistHint. Is is ok?
					left += parentExtent;
					(*it)->getBorderDistHint(start_dist, end_dist);
					m.bottom = qMax(m.bottom, start_dist);
					m.top = qMax(m.top, end_dist);
				}
				else if ((*it)->alignment() == VipBorderItem::Right)
				{
					right += parentExtent;
					(*it)->getBorderDistHint(start_dist, end_dist);
					m.bottom = qMax(m.bottom, start_dist);
					m.top = qMax(m.top, end_dist);
				}
				else if ((*it)->alignment() == VipBorderItem::Top)
				{
					top += parentExtent;
					(*it)->getBorderDistHint(start_dist, end_dist);
					m.left = qMax(m.left, start_dist);
					m.right = qMax(m.right, end_dist);
				}
				else if ((*it)->alignment() == VipBorderItem::Bottom)
				{
					bottom += parentExtent;
					(*it)->getBorderDistHint(start_dist, end_dist);
					m.left = qMax(m.left, start_dist);
					m.right = qMax(m.right, end_dist);
				}
			}

			computeRects(marg);
		}

		//left = qMax(left,m.left);
		// right = qMax(right,m.right);
		// top = qMax(top,m.top);
		// bottom = qMax(bottom,m.bottom);
		if (m.left > left) marg.left += m.left - left;
		if (m.right > right) marg.right += m.right - right;
		if (m.top > top) marg.top += m.top - top;
		if (m.bottom > bottom) marg.bottom += m.bottom - bottom;
		computeRects(marg);

		//compute the different offsets

		for (QList<VipBorderItem*>::const_iterator it = linkedBorders.begin(); it != linkedBorders.end(); ++it)
		{
			VipBorderItem * item = *it;
			if (!item->isVisible() || item->axisIntersectionEnabled() || item->property("_vip_ignore_geometry").toBool())
				continue;

			//compute offsets for each items
			int index = linkedBorders.indexOf(item);
			QPair<double, double> off(0., 0.);

			for (int i = 0; i < linkedBorders.size(); ++i)
			{
				VipBorderItem * ax = linkedBorders[i];

				if (!ax->isVisible() || ax == *it || ax->alignment() != (*it)->alignment())
					continue;

				double extent = parentExtents[ax];

				if (!ax->axisIntersectionEnabled())
				{
					if (ax->canvasProximity() < (*it)->canvasProximity())
						off.first += extent;
					else if (ax->canvasProximity() == (*it)->canvasProximity() && i < index)
						off.first += extent;
					else
						off.second += extent;
				}
			}


			offsets[*it] = off;

		}

	}

	void computeItemGeometry(VipBorderItem * item, bool compute_intersection_geometry)
	{
		if (item->property("_vip_ignore_geometry").toBool())
			return;

		//do no modify position or size if the computed inner and outer rects are invalid
		if (!outerRect.isValid())
			return;

		//first, compute the geometry of intersected axis and avoid
		//infinite recursion in case of cross intersection
		if (item->axisIntersectionEnabled() && compute_intersection_geometry && item->axisIntersection()->parentItem() == parent)
			computeItemGeometry(item->axisIntersection(),false);

		//the new item geometry
		QPointF new_pos;
		QRectF new_rect;

		//compute the geometry
		{
			QRectF surrounded_rect = innerRect;
			QPair<double, double> off = offsets[item];
			double this_ext = extents[item];
			double ext = parentExtents[item];
			double width = item->orientation() == Qt::Vertical ? surrounded_rect.height() : surrounded_rect.width();

			if (item->orientation() == Qt::Vertical)
				new_rect = QRectF(0, 0, this_ext, width);
			else
				new_rect = QRectF(0, 0, width, this_ext);

			if (item->alignment() == VipBorderItem::Left)
				new_pos = QPointF(surrounded_rect.left() - ext - off.first, surrounded_rect.top());
			else if (item->alignment() == VipBorderItem::Right)
				new_pos = QPointF(surrounded_rect.right() + off.first, surrounded_rect.top());
			else if (item->alignment() == VipBorderItem::Top)
				new_pos = QPointF(surrounded_rect.left(), surrounded_rect.top() - ext - off.first);
			else if (item->alignment() == VipBorderItem::Bottom)
				new_pos = QPointF(surrounded_rect.left(), surrounded_rect.bottom() + off.first);

			if (VipBorderItem * inter = item->axisIntersection()){
				if (inter->parentItem() == item->parentItem()){
					if (item->orientation() == Qt::Vertical)
						new_pos.setX(inter->position(item->axisIntersectionValue(), 0, item->axisIntersectionType()).x() + inter->pos().x());
					else
						new_pos.setY(inter->position(item->axisIntersectionValue(), 0, item->axisIntersectionType()).y() + inter->pos().y());

					// record the theoric 'good' position as a property
					item->setProperty("_vip_Pos", QVariant::fromValue(new_pos));

					// If axisIntersectionEnabled() is true, then the rect height (or width) is 0, we must set the right value to draw the title
					double length;
					if (item->orientation() == Qt::Vertical)
						length = new_rect.height();
					else
						length = new_rect.width();

					double fullExtent = item->extentForLength(length);
					if (item->alignment() == VipBorderItem::Bottom)
						new_rect.setBottom(new_rect.bottom() + fullExtent);
					else if (item->alignment() == VipBorderItem::Left) {
						new_pos.rx() -= fullExtent;
						new_rect.setRight(new_rect.right() + fullExtent);
					}
					else if (item->alignment() == VipBorderItem::Top) {
						new_pos.ry() -= fullExtent;
						new_rect.setBottom(new_rect.bottom() + fullExtent);
					}
					else if (item->alignment() == VipBorderItem::Right)
						new_rect.setRight(new_rect.right() + fullExtent);
					
				}
			}
		}


		//take into account corners
		if (item->expandToCorners()){
			QRectF rect = (new_rect);

			if (item->orientation() == Qt::Horizontal){
				new_pos.setX(new_pos.x() - this->left);
				new_rect.setRight(new_rect.right() + this->left + this->right);
				rect.moveLeft(rect.left() + this->left);
			}
			else{

				new_pos.setY(new_pos.y() - this->top);
				new_rect.setBottom(new_rect.bottom() + this->top + this->bottom);
				rect.moveTop(rect.top() + this->top);
			}
			item->setBoundingRectNoCorners(rect);
		}


		//update geometry only if needed
		if (new_pos != item->pos() || new_rect != item->boundingRect())
		{
			//item->setPos(new_pos);
			if (!item->expandToCorners())
				item->setBoundingRectNoCorners(new_rect);

			item->itemGeometryChanged(new_rect);
			item->setGeometry(new_rect.translated(new_pos));
			
			//fully update items
			item->updateItems();
		}
		else
		{
			item->itemGeometryChanged(new_rect);
		}
		item->update();
	}


	static void recomputeGeometry(VipAbstractPlotArea * area, QRectF & innerRect, QRectF & outerRect, bool compute_aligned = false)
	{
		if (compute_aligned)
			area->d_data->aligned_margins = VipMargins();

		ComputeBorderGeometry c;
		c.parent = area;
		QList<QGraphicsItem*> items = area->childItems();
		for (QList<QGraphicsItem*>::const_iterator it = items.cbegin(); it != items.cend(); ++it) {
			if (VipBorderItem * item = qobject_cast<VipBorderItem*>((*it)->toGraphicsObject())) {
				c.linkedBorders.append(item);
				item->prepareGeometryChange();
				item->QGraphicsObject::update(); //force an update in case of item caching (see QGraphicsItem::setCacheMode).
				//indeed, the item is not properly updated with caching except with an explicit call to update()
			}
		}
		c.computeItemsGeometry(area->margins() + area->d_data->aligned_margins);
		for (int i = 0; i < c.linkedBorders.size(); ++i)
			c.computeItemGeometry(c.linkedBorders[i], true);

		innerRect = c.innerRect;
		outerRect = c.outerRect;

		// Compute vertical and horizontal alignment
		if (compute_aligned) {

			// First do horizontally aligned areas
			{
				// Map of area -> inner rectangle
				QMap<VipAbstractPlotArea*, QRectF> areaInnerRects;
				areaInnerRects.insert(area, innerRect);
				double top = innerRect.top();
				double bottom = innerRect.bottom();
				QGraphicsItem* area_parent = area->parentItem();
				const QSet<VipAbstractPlotArea*> aligned = sharedAlignedAreas(area, Qt::Horizontal);
				if (aligned.size()) {

					// Build the map of aligned areas -> inner rects, and compute the top and bottom coordinates
					for (auto it = aligned.begin(); it != aligned.end(); ++it) {
						if (*it != area && (*it)->parentItem() == area_parent) {
							VipAbstractPlotArea* a = const_cast<VipAbstractPlotArea*>(*it);
							QRectF outer, inner;
							a->d_data->aligned_margins.top = a->d_data->aligned_margins.bottom = 0;
							recomputeGeometry(a, inner, outer);
							areaInnerRects.insert(a, inner);
							top = std::max(inner.top(), top);
							bottom = std::min(inner.bottom(), bottom);
						}
					}

					// Now, align areas by adjusting there margins
					for (auto it = areaInnerRects.begin(); it != areaInnerRects.end(); ++it) {
						VipAbstractPlotArea* a = it.key();
						QRectF inner = it.value();
						//VipMargins margins = a->margins();
						bool need_update = false;
						if (!vipFuzzyCompare(inner.top(), top)) {
							need_update = true;
							//margins.top += top - inner.top();
							a->d_data->aligned_margins.top = top - inner.top();
						}
						if (!vipFuzzyCompare(inner.bottom(), bottom)) {
							need_update = true;
							a->d_data->aligned_margins.bottom = inner.bottom() - bottom;
							//margins.bottom += inner.bottom() - bottom;
						}
						//if (need_update)
							//a->setMargins(margins);
					}
				}
			}
			// Then do vertically aligned areas
			{
				// Map of area -> inner rectangle
				QMap<VipAbstractPlotArea*, QRectF> areaInnerRects;
				areaInnerRects.insert(area, innerRect);
				double left = innerRect.left();
				double right = innerRect.right();
				QGraphicsItem* area_parent = area->parentItem();
				const QSet<VipAbstractPlotArea*> aligned = sharedAlignedAreas(area, Qt::Vertical);
				if (aligned.size()) {

					// Build the map of aligned areas -> inner rects, and compute the top and bottom coordinates
					for (auto it = aligned.begin(); it != aligned.end(); ++it) {
						if (*it != area && (*it)->parentItem() == area_parent) {
							VipAbstractPlotArea* a = const_cast<VipAbstractPlotArea*>(*it);
							a->d_data->aligned_margins.left = a->d_data->aligned_margins.right = 0;
							QRectF outer, inner;
							recomputeGeometry(a, inner, outer);
							areaInnerRects.insert(a, inner);
							left = std::max(inner.left(), left);
							right = std::min(inner.right(), right);
						}
					}

					// Now, align areas by adjusting there margins
					for (auto it = areaInnerRects.begin(); it != areaInnerRects.end(); ++it) {
						VipAbstractPlotArea* a = it.key();
						QRectF inner = it.value();
						//VipMargins margins = a->margins();
						bool need_update = false;
						if (!vipFuzzyCompare(inner.left(), left)) {
							need_update = true;
							a->d_data->aligned_margins.left = left - inner.left();
							//margins.left += left - inner.left();
						}
						if (!vipFuzzyCompare(inner.right(), right)) {
							need_update = true;
							a->d_data->aligned_margins.right = inner.right() - right;
							//margins.right += inner.right() - right;
						}
						if (need_update)
							a->recomputeGeometry(false);
							//a->setMargins(a->margins());
					}
				}
			}
		}
	}

};







/// \internal
/// Compute the geometry for VipBorderItem
class ComputePolarGeometry
{
public:

	QList<VipPolarAxis*> linkedPolarAxis;
	QList<VipRadialAxis*> linkedRadialAxis;
	QPointF shared_center;

	ComputePolarGeometry()
	{}

	QSet<VipPlotItem*> visible(const PlotItemList& lst)
	{
		QSet<VipPlotItem*> res;
		for (int i = 0; i < lst.size(); ++i) {
			VipPlotItem* it = lst[i];
			if (it->isVisible()) {
				res.insert(it);
			}
		}
		return res;
	}

	void computeGeometry(const QList< VipAbstractPolarScale*> & scales, const QRectF & outer_rect)
	{
		linkedPolarAxis.clear();
		linkedRadialAxis.clear();

		for (int i = 0; i < scales.size(); ++i)
		{
			if (qobject_cast<VipPolarAxis*>(scales[i]))
				linkedPolarAxis << static_cast<VipPolarAxis*>(scales[i]);
			else if (qobject_cast<VipRadialAxis*>(scales[i]))
				linkedRadialAxis << static_cast<VipRadialAxis*>(scales[i]);
		}

		//get parent VipAbstractPlotArea if any and disable geometry update (which might leed to infinit recursion)
		QSet<VipAbstractPlotArea*> areas;
		for (int i = 0; i < scales.size(); ++i)
			if (scales[i]->area()) {
				areas.insert(scales[i]->area());
				//disable geometry update
				scales[i]->area()->setGeometryUpdateEnabled(false);
			}



		//VipPolarAxis sorted by center proximity
		QMap<int, QList<VipPolarAxis*> > axes;
		//radius extent each layer
		QMap<int, double > radiusExtents;
		//free axes (centerProximity() < 0)
		QList<VipPolarAxis*> free;
		//maximum radius
		double max_radius = 0;

		//axes center
		shared_center = Vip::InvalidPoint;

		//extract VipPolarAxis sorted by layers, free VipPolarAxis, and radius extents sorted by layers
		for (int i = 0; i < linkedPolarAxis.size(); ++i)
		{
			VipPolarAxis* axis = linkedPolarAxis[i];

			//update shared_center for the first iteration
			if (!vipIsValid(shared_center ))
				shared_center = axis->center();

			//Temporally block axes signals
			axis->blockSignals(true);

			//update center and layout scale if necessary (just to get min and max radius)
			axis->setCenter(shared_center);
			//if(axis->axisRect().isEmpty())
			axis->layoutScale();

			max_radius = qMax(max_radius, axis->maxRadius());

			//sort axes by center proximity
			if (axis->centerProximity() < 0)
				free << axis;
			else
			{
				axes[axis->centerProximity()] << axis;

				//compute layers extent
				QMap<int, double >::iterator it = radiusExtents.find(axis->centerProximity());
				if (it == radiusExtents.end())
					radiusExtents[axis->centerProximity()] = axis->radiusExtent();
				else
				{
					radiusExtents[axis->centerProximity()] = qMax(axis->radiusExtent(), it.value());
				}
			}
		}

		//update radius according to center proximity a first time
		QMapIterator<int, QList<VipPolarAxis*> > it(axes);
		it.toBack(); it.previous();
		QList<double> extents = radiusExtents.values();
		double radius = max_radius;
		for (int i = extents.size() - 1; i >= 0; --i)
		{
			QList<VipPolarAxis*> layer = it.value();
			for (int j = 0; j < layer.size(); ++j)
			{
				layer[j]->setMinRadius(radius - extents[i]);
				layer[j]->layoutScale();
			}

			radius -= extents[i];
			it.previous();
		}

		//update VipRadialAxis layout
		for (int i = 0; i < linkedRadialAxis.size(); ++i)
		{
			VipRadialAxis* axis = static_cast<VipRadialAxis*>(linkedRadialAxis[i]);
			axis->blockSignals(true);
			axis->setCenter(shared_center);
			axis->layoutScale();
		}

		//compute the union rect of all axes and items
		QSet<VipPlotItem*> items;
		QRectF union_rect;
		for (VipPolarAxis * axis: linkedPolarAxis)
		{
			if (axis->isVisible())
				union_rect |= axis->axisRect();
			items += visible(axis->plotItems());
		}
		for (VipRadialAxis * axis: linkedRadialAxis)
		{
			if (axis->isVisible())
				union_rect |= axis->axisRect();
			items += visible(axis->plotItems());
		}
		for (VipPlotItem * item : items)
		{
			item->markCoordinateSystemDirty();
			union_rect |= item->shape().boundingRect().translated(item->pos());
		}





		//scale the bounding rect but keep proportions
		double factor = 1;
		double width_on_height = outer_rect.width() / outer_rect.height();
		double axes_width_on_height = union_rect.width() / union_rect.height();

		//compute the transformation to change axes radius and center
		if (axes_width_on_height > width_on_height)
		{
			factor = outer_rect.width() / union_rect.width();

			//QPointF proportions = (shared_center - union_rect.topLeft()) / QPointF(union_rect.width(),union_rect.height()) * factor;

			QPointF translate = QPointF(outer_rect.left() - union_rect.left(),
				outer_rect.top() + (outer_rect.height() - factor * union_rect.height()) / 2.0 - union_rect.top());

			QPointF topLeft = union_rect.topLeft() + translate;
			shared_center = (shared_center - union_rect.topLeft()) * factor + topLeft;
		}
		else
		{
			factor = outer_rect.height() / union_rect.height();
			QPointF translate = QPointF(outer_rect.left() + (outer_rect.width() - factor * union_rect.width()) / 2.0 - union_rect.left(),
				outer_rect.top() - union_rect.top());
			QPointF topLeft = union_rect.topLeft() + translate;
			shared_center = (shared_center - union_rect.topLeft()) * factor + topLeft;
		}



		//change the center for all axes
		for (int i = 0; i < linkedPolarAxis.size(); ++i)
			linkedPolarAxis[i]->setCenter(shared_center);

		//change the center for all axes
		for (int i = 0; i < linkedRadialAxis.size(); ++i)
			linkedRadialAxis[i]->setCenter(shared_center);


		//change axes radius the outer layer and free axes
		QList<VipPolarAxis*> outers = (--axes.end()).value();
		for (int i = 0; i < outers.size(); ++i)
		{
			double min_radius = qMax(0.1, outers[i]->minRadius() * factor);
			outers[i]->setMinRadius(min_radius);
			outers[i]->layoutScale();
		}

		for (int i = 0; i < free.size(); ++i)
		{
			double min_radius = qMax(0.1, free[i]->minRadius() * factor);
			free[i]->setMinRadius(min_radius);
			free[i]->layoutScale();
		}

		//update radius according to center proximity one last time, excluding outer layer
		it = QMapIterator<int, QList<VipPolarAxis*> >(axes);
		it.toBack(); it.previous();  it.previous();
		max_radius *= factor;
		radius = max_radius - extents.last();
		for (int i = extents.size() - 2; i >= 0; --i)
		{
			QList<VipPolarAxis*> layer = it.value();
			for (int j = 0; j < layer.size(); ++j)
			{
				layer[j]->setMinRadius(radius - extents[i]);
				layer[j]->layoutScale();
			}

			radius -= extents[i];
			it.previous();
		}

		//enable signals, compute the minimum size, set the geometry
		QRectF geom = QRectF(QPointF(0, 0), outer_rect.bottomRight() + outer_rect.topLeft() * 2);

		for (int i = 0; i < linkedPolarAxis.size(); ++i)
		{
			linkedPolarAxis[i]->setGeometry(geom);
			linkedPolarAxis[i]->blockSignals(false);
		}

		for (int i = 0; i < linkedRadialAxis.size(); ++i)
		{
			linkedRadialAxis[i]->layoutScale();
			linkedRadialAxis[i]->setGeometry(geom);
			linkedRadialAxis[i]->blockSignals(false);
		}

		//enable back geometry update
		for (VipAbstractPlotArea * a : areas)
			a->setGeometryUpdateEnabled(true);

	}


};














class VipPlotArea2D::PrivateData
{
public:
	PrivateData()
	{
		yLeft = new VipAxisBase(VipAxisBase::Left);
		yRight = new VipAxisBase(VipAxisBase::Right);
		xTop = new VipAxisBase(VipAxisBase::Top);
		xBottom = new VipAxisBase(VipAxisBase::Bottom);

		yLeft->setMargin(0);
		//yLeft->setMaxBorderDist(0, 0);
		yLeft->setZValue(20);

		yRight->setMargin(0);
		//yRight->setMaxBorderDist(0, 0);
		yRight->setZValue(20);

		xTop->setMargin(0);
		//xTop->setMaxBorderDist(0, 0);
		xTop->setZValue(10);
		xTop->setExpandToCorners(true);

		xBottom->setMargin(0);
		//xBottom->setMaxBorderDist(0, 0);
		xBottom->setZValue(10);
		xBottom->setExpandToCorners(true);

		yLeft->synchronizeWith(yRight);
		xTop->synchronizeWith(xBottom);
	}

	VipAxisBase * yLeft;
	VipAxisBase * yRight;
	VipAxisBase * xTop;
	VipAxisBase * xBottom;
	QRectF innerRect, outerRect;
	QList<VipAxisBase*> axes;
};

static bool registerVipPlotArea2D = vipSetKeyWordsForClass(&VipPlotArea2D::staticMetaObject);


VipPlotArea2D::VipPlotArea2D(QGraphicsItem * parent)
	: VipAbstractPlotArea(parent)
{

	d_data = new PrivateData();
	addScale(d_data->xTop);
	addScale(d_data->xBottom);
	addScale(d_data->yLeft);
	addScale(d_data->yRight);

	grid()->setAxes(d_data->xBottom, d_data->yLeft, VipCoordinateSystem::Cartesian);
	canvas()->setAxes(d_data->xBottom, d_data->yLeft, VipCoordinateSystem::Cartesian);

	d_data->xTop->setObjectName("Top axis");
	d_data->xBottom->setObjectName("Bottom axis");
	d_data->yLeft->setObjectName("Left axis");
	d_data->yRight->setObjectName("Right axis");

}

VipPlotArea2D::~VipPlotArea2D()
{
	delete d_data;
}

VipAxisBase * VipPlotArea2D::leftAxis() const
{
	return const_cast<VipAxisBase*>(d_data->yLeft);
}

VipAxisBase * VipPlotArea2D::rightAxis() const
{
	return const_cast<VipAxisBase*>(d_data->yRight);
}

VipAxisBase * VipPlotArea2D::topAxis() const
{
	return const_cast<VipAxisBase*>(d_data->xTop);
}

VipAxisBase * VipPlotArea2D::bottomAxis() const
{
	return const_cast<VipAxisBase*>(d_data->xBottom);
}


QList<VipAxisBase*> VipPlotArea2D::axes() const
{
	QList<VipAxisBase*> res;
	QList<VipAbstractScale*> _scales = this->scales();

	for (int i = 0; i < _scales.size(); ++i)
	{
		if (qobject_cast<VipAxisBase*>(_scales[i]))
			res << static_cast<VipAxisBase*>(_scales[i]);
	}

	return res;
}

VipCoordinateSystem::Type VipPlotArea2D::standardScales(QList<VipAbstractScale*> & axes) const
{
	axes << bottomAxis() << leftAxis();
	return VipCoordinateSystem::Cartesian;
}

bool VipPlotArea2D::internalRemoveScale(VipAbstractScale * scale)
{
	//we cannot remove on of the predifined scale
	//if (scale == d_data->xBottom || scale == d_data->xTop || scale == d_data->yLeft || scale == d_data->yRight)
	//	return false;

	return VipAbstractPlotArea::internalRemoveScale(scale);
}

double VipPlotArea2D::titleOffset() const
{
	double space = 0;
	if (titleAxis()->titleInside() && titleAxis()->isVisible() && !title().isEmpty()) {
		space += topAxis()->isVisible() ? topAxis()->boundingRect().height() : 0;
		if (topAxis()->constScaleDraw()->hasComponent(VipScaleDraw::Backbone))
			space += topAxis()->constScaleDraw()->componentPen(VipScaleDraw::Backbone).widthF();
		if (topAxis()->constScaleDraw()->hasComponent(VipScaleDraw::Ticks))
			space += topAxis()->constScaleDraw()->tickLength(VipScaleDiv::MajorTick);
		space += title().textSize().height();
	}
	return space;
}

QRectF VipPlotArea2D::outerRect() const
{
	return d_data->outerRect;
}
QRectF VipPlotArea2D::innerRect() const
{
	return d_data->innerRect;
}

void VipPlotArea2D::recomputeGeometry(bool recompute_aligned_areas )
{
	if (titleAxis()->titleInside()) {
		double spacing = topAxis()->isVisible() ? topAxis()->boundingRect().height() : 0;
		if (topAxis()->constScaleDraw()->hasComponent(VipScaleDraw::Backbone))
			spacing += topAxis()->constScaleDraw()->componentPen(VipScaleDraw::Backbone).widthF();
		if (topAxis()->constScaleDraw()->hasComponent(VipScaleDraw::Ticks))
			spacing += topAxis()->constScaleDraw()->tickLength(VipScaleDiv::MajorTick);
		titleAxis()->setSpacing(spacing);
	}
	else
		titleAxis()->setSpacing(0);

	ComputeBorderGeometry::recomputeGeometry(this, d_data->innerRect, d_data->outerRect, recompute_aligned_areas);


	this->resetInnerLegendsPosition();

	this->update();
}


void VipPlotArea2D::zoomOnSelection(const QPointF & start, const QPointF & end)
{
	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	for (int i = 0; i < items.size(); ++i)
	{
		VipAbstractScale * axis = items[i];
		if (zoomEnabled(axis))
		{
			QPointF axis_start = axis->mapFromItem(this, start);
			QPointF axis_end = axis->mapFromItem(this, end);

			VipInterval interval(axis->scaleDraw()->value(axis_start), axis->scaleDraw()->value(axis_end));
			interval = interval.normalized();
			axis->setScale(interval.minValue(), interval.maxValue());
		}

	}

}

void VipPlotArea2D::zoomOnPosition(const QPointF & item_pos, double sc)
{
	vip_double zoomValue = (sc - 1);
	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	for (int i = 0; i < items.size(); ++i)
	{
		VipAxisBase * axis = static_cast<VipAxisBase*>(items[i]);
		if (zoomEnabled(axis))
		{
			//mapFromItem(this,scenePos) to mapFromScene(scenePos)
			vip_double pos = axis->scaleDraw()->value(axis->mapFromItem(this, item_pos));

			VipInterval interval = axis->scaleDiv().bounds();
			VipInterval new_interval(interval.minValue() + (pos - interval.minValue())*zoomValue,
				interval.maxValue() - (interval.maxValue() - pos)*zoomValue);


			axis->setScale(new_interval.minValue(), new_interval.maxValue());
		}
	}

}

QPainterPath VipPlotArea2D::innerArea() const
{
	QPainterPath p;
	p.addRect(d_data->innerRect);
	return p;
}

void VipPlotArea2D::translate(const QPointF &, const QPointF & dp)
{
	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	for (int i = 0; i < items.size(); ++i)
	{
		VipAxisBase * axis = static_cast<VipAxisBase*>(items[i]);
		if (zoomEnabled(axis))
		{
			vip_double start = axis->scaleDraw()->value(axis->scaleDraw()->pos() - dp);
			vip_double end = axis->scaleDraw()->value(axis->scaleDraw()->end() - dp);

			//for images only, clamp to image bounding rect
			if (VipImageArea2D * area = qobject_cast<VipImageArea2D*>(this)) {
				QRectF imrect = area->spectrogram()->imageBoundingRect();
				vip_double w = end - start;
				if (axis->orientation() == Qt::Vertical) {
					if (start < imrect.top()) {
						start = imrect.top();
						end = start + w;
					}
					if (end > imrect.bottom()) {
						end = imrect.bottom();
						start = end - w;
					}
					if (start < imrect.top()) continue;
					if (end > imrect.bottom()) continue;
				}
				else {
					if (start < imrect.left()) {
						start = imrect.left();
						end = start + w;
					}
					if (end > imrect.right()) {
						end = imrect.right();
						start = end - w;
					}
					if (start < imrect.left()) continue;
					if (end > imrect.right()) continue;
				}
			}

			VipInterval interval(start, end);
			//keep the initial axis scale orientation
			if (axis->orientation() == Qt::Vertical)
				interval = interval.inverted();

			axis->setScale(interval.minValue(), interval.maxValue());
		}
	}
}





static int registerPlotPolarKeywords()
{
	VipKeyWords keys;
	keys["inner-margin"] = VipParserPtr(new DoubleParser());

	vipSetKeyWordsForClass(&VipPlotPolarArea2D::staticMetaObject, keys);
	return 0;
}
static int _registerPlotPolarKeywords = registerPlotPolarKeywords();


class VipPlotPolarArea2D::PrivateData
{
public:
	PrivateData()
		:margin(5)
	{
		paxis = new VipPolarAxis();
		raxis = new VipRadialAxis();
		paxis->setZValue(20);
		raxis->setZValue(20);
		paxis->setCenter(QPointF(100, 100));
		raxis->setCenter(QPointF(100, 100));
		paxis->setRadius(100);
		raxis->setEndRadius(100);
	}

	double margin;
	VipPolarAxis *paxis;
	VipRadialAxis * raxis;
	QRectF innerRect;
	QRectF outerRect;
	QList<VipAbstractPolarScale*> axes;
};

VipPlotPolarArea2D::VipPlotPolarArea2D(QGraphicsItem * parent)
	:VipAbstractPlotArea(parent)
{
	d_data = new PrivateData();

	addScale(d_data->raxis);
	addScale(d_data->paxis);


	canvas()->setAxes(d_data->raxis, d_data->paxis, VipCoordinateSystem::Polar);
	grid()->setAxes(d_data->raxis, d_data->paxis, VipCoordinateSystem::Polar);
	grid()->setRenderHints(QPainter::Antialiasing);
	grid()->enableAxisMin(0, false);
	grid()->enableAxisMin(1, false);

	grid()->setVisible(true);

	polarAxis()->setStartAngle(0);
	polarAxis()->setEndAngle(360);
	radialAxis()->setStartRadius(0, polarAxis());
	radialAxis()->setEndRadius(1, polarAxis());
	radialAxis()->setAngle(90);
}

VipPlotPolarArea2D::~VipPlotPolarArea2D()
{
	delete d_data;
}

bool VipPlotPolarArea2D::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	if (strcmp(name, "inner-margin") == 0) {
		setInnerMargin(value.toDouble());
		return true;
	}
	return VipAbstractPlotArea::setItemProperty(name, value, index);
}

VipCoordinateSystem::Type VipPlotPolarArea2D::standardScales(QList<VipAbstractScale*> & axes) const
{
	axes << d_data->raxis << d_data->paxis;
	return VipCoordinateSystem::Polar;
}

void VipPlotPolarArea2D::setInnerMargin(double margin)
{
	if (margin != d_data->margin)
	{
		d_data->margin = margin;
		//emitItemChanged();
		//recomputeGeometry();
	}
}

double VipPlotPolarArea2D::innerMargin() const
{
	return d_data->margin;
}

VipPolarAxis * VipPlotPolarArea2D::polarAxis() const
{
	return const_cast<VipPolarAxis*>(d_data->paxis);
}

VipRadialAxis * VipPlotPolarArea2D::radialAxis() const
{
	return const_cast<VipRadialAxis*>(d_data->raxis);
}

QList<VipAbstractPolarScale*> VipPlotPolarArea2D::axes() const
{
	QList<VipAbstractPolarScale*> res;
	QList<VipAbstractScale*> _scales = this->scales();

	for (int i = 0; i < _scales.size(); ++i)
	{
		if (qobject_cast<VipAbstractPolarScale*>(_scales[i]))
			res << static_cast<VipAbstractPolarScale*>(_scales[i]);
	}

	return res;
}


void VipPlotPolarArea2D::zoomOnSelection(const QPointF & start, const QPointF & end)
{
	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	for (int i = 0; i < items.size(); ++i)
	{
		VipAbstractScale * axis = items[i];
		if (zoomEnabled(axis))
		{
			QPointF axis_start = axis->mapFromItem(this, start);
			QPointF axis_end = axis->mapFromItem(this, end);

			VipInterval interval(axis->scaleDraw()->value(axis_start), axis->scaleDraw()->value(axis_end));
			interval = interval.normalized();
			axis->setScale(interval.minValue(), interval.maxValue());
		}
	}


}

void VipPlotPolarArea2D::zoomOnPosition(const QPointF & item_pos, double sc)
{
	vip_double zoomValue = (sc - 1);
	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	for (int i = 0; i < items.size(); ++i)
	{
		VipAbstractScale * axis = items[i];
		if (zoomEnabled(axis))
		{
			vip_double pos = axis->scaleDraw()->value(item_pos);

			VipInterval interval = axis->scaleDiv().bounds();
			VipInterval new_interval(interval.minValue() + (pos - interval.minValue())*zoomValue,
				interval.maxValue() - (interval.maxValue() - pos)*zoomValue);
			axis->setScale(new_interval.minValue(), new_interval.maxValue());
		}
	}

}

QPainterPath VipPlotPolarArea2D::innerArea() const
{
	return canvas()->shape();
}

void VipPlotPolarArea2D::translate(const QPointF & fromPt, const QPointF & dp)
{
	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	for (int i = 0; i < items.size(); ++i)
	{
		if (!qobject_cast<VipAbstractPolarScale*>(items[i]))
			continue;

		VipAbstractPolarScale * axis = static_cast<VipAbstractPolarScale*>(items[i]);
		if (zoomEnabled(axis))
		{
			vip_double start = 0;
			vip_double end = 0;

			if (qobject_cast<VipPolarAxis*>(axis))
			{
				VipPolarAxis * paxis = static_cast<VipPolarAxis*>(axis);
				vip_double dangle = QLineF(paxis->scaleDraw()->center(), fromPt).
					angleTo(QLineF(paxis->scaleDraw()->center(), fromPt + dp));

				QLineF l1(paxis->scaleDraw()->center(), paxis->scaleDraw()->center() - QPointF(0, paxis->scaleDraw()->radius()));
				l1.setAngle(paxis->scaleDraw()->startAngle() - dangle);

				QLineF l2(paxis->scaleDraw()->center(), paxis->scaleDraw()->center() - QPointF(0, paxis->scaleDraw()->radius()));
				l2.setAngle(paxis->scaleDraw()->endAngle() - dangle);

				if (dangle > 180) {
					start = paxis->scaleDraw()->value(l1.p2());
					vip_double diff = start - paxis->scaleDiv().bounds().minValue();
					end = paxis->scaleDiv().bounds().maxValue() + diff;
				}
				else {
					end = paxis->scaleDraw()->value(l2.p2());
					vip_double diff = end - paxis->scaleDiv().bounds().maxValue();
					start = paxis->scaleDiv().bounds().minValue() + diff;
				}
				//start = axis->scaleDraw()->scaleMap().invTransform( paxis->scaleDraw()->startAngle()+dangle );
				//end = axis->scaleDraw()->scaleMap().invTransform( paxis->scaleDraw()->endAngle()+dangle );

			}
			else
			{
				VipRadialAxis * raxis = static_cast<VipRadialAxis*>(axis);

				QLineF l1(raxis->scaleDraw()->center(), raxis->scaleDraw()->center() - QPointF(0, raxis->scaleDraw()->endRadius()));
				l1.setAngle(raxis->scaleDraw()->angle());
				QLineF l2(raxis->scaleDraw()->center(), fromPt);
				l2.setLength(raxis->scaleDraw()->endRadius());
				l2.setP2(l2.p2() + dp);
				vip_double dradius = l2.length() - l1.length();

				start = axis->scaleDraw()->scaleMap().invTransform(raxis->scaleDraw()->startRadius() - dradius);
				end = axis->scaleDraw()->scaleMap().invTransform(raxis->scaleDraw()->endRadius() - dradius);
			}

			VipInterval interval(start, end);
			interval = interval.normalized();
			axis->setScale(interval.minValue(), interval.maxValue());
		}
	}
}

void VipPlotPolarArea2D::recomputeGeometry(bool recompute_aligned_areas)
{
	ComputeBorderGeometry::recomputeGeometry(this, d_data->innerRect, d_data->outerRect, recompute_aligned_areas);
	this->resetInnerLegendsPosition();

	QRectF inner_rect = d_data->innerRect;
	QRectF inner_rect_adjusted = inner_rect.adjusted(d_data->margin, d_data->margin, -d_data->margin, -d_data->margin);

	ComputePolarGeometry c;
	c.computeGeometry(this->axes(), inner_rect_adjusted);

	this->update();
}




/* struct DisplayRateFilter : QObject
{
	int d_displayRate;
	int d_displayTime;
	qint64 d_lastUpdate;
	QTimer d_timer;
	QPointer<QWidget> d_widget;

	DisplayRateFilter(QWidget * widget, int displayRate = 30)
		: QObject(widget), d_displayRate(0), d_displayTime(0), d_lastUpdate(0), d_widget(widget)
	{
		setDisplayRate(displayRate);
		d_timer.setSingleShot(true);
		connect(&d_timer, SIGNAL(timeout()), widget, SLOT(update()), Qt::DirectConnection);
		widget->installEventFilter(this);
	}
	~DisplayRateFilter()
	{
		if (d_widget)
			disconnect(&d_timer, SIGNAL(timeout()), d_widget, SLOT(update()));
		d_timer.stop();
	}

	void setDisplayRate(int rate)
	{
		d_displayRate = rate;
		d_displayTime = (1.0 / rate) * 1000;
		d_lastUpdate = 0;
	}

	virtual bool eventFilter(QObject * , QEvent * evt)
	{
		if (evt->type() == QEvent::Paint) {
			qint64 current = QDateTime::currentMSecsSinceEpoch();
			if (d_lastUpdate == 0) {
				d_lastUpdate = current;
				return false;
			}
			qint64 elapsed = current - d_lastUpdate;
			if (elapsed > d_displayTime) {
				d_lastUpdate = current;
				return false;
			}
			d_timer.start(d_displayTime - elapsed);
			return true;
		}
		return false;
	}
};

void vipSetMaxDisplayRate(QGraphicsView * view, int displayRate)
{
	DisplayRateFilter * filter = static_cast<DisplayRateFilter*>(view->findChild<QObject*>("_vip_displayRateFilter"));
	if (displayRate <= 0) {
		if(filter)
			delete filter;
		view->setProperty("_vip_displayRateFilter", QVariant());
		view->setProperty("_vip_displayRate", QVariant());
		return;
	}

	if (!filter) {
		filter = new DisplayRateFilter(view->viewport(), displayRate);
		view->setProperty("_vip_displayRateFilter", QVariant::fromValue((QObject*)filter));
	}
	else
		filter->setDisplayRate(displayRate);
	view->setProperty("_vip_displayRate", displayRate);
}
int vipMaxDisplayRate(QGraphicsView * view)
{
	return view->property("_vip_displayRate").toInt();
}

*/





class VipBaseGraphicsView::PrivateData
{
public:
	
	QSharedPointer<QColor> backgroundColor;
	bool useInternalViewport;
};


VipBaseGraphicsView::VipBaseGraphicsView(QGraphicsScene * sc, QWidget* parent)
	:QGraphicsView(parent), VipRenderObject(this)
{
	m_data = new PrivateData();
	m_data->useInternalViewport = false;


#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#else
	//TEST
	this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	
	this->setAttribute(Qt::WA_PaintUnclipped);
	this->viewport()->setAttribute(Qt::WA_PaintUnclipped);

	this->setAttribute(Qt::WA_NoSystemBackground);
	this->setAttribute(Qt::WA_OpaquePaintEvent);
	this->viewport()->setAttribute(Qt::WA_NoSystemBackground);
	this->viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
	

#endif

	this->setFrameShape(QFrame::NoFrame);

	if (sc)
		this->setScene(sc);
	else
		this->setScene(new QGraphicsScene());
	this->scene()->setParent(this);

	this->scene()->setItemIndexMethod(QGraphicsScene::NoIndex);
	//if (qobject_cast<)
	updateCacheMode(false);

	setMouseTracking(true);
}

VipBaseGraphicsView::VipBaseGraphicsView(QWidget* parent)
  : VipBaseGraphicsView(nullptr, parent)
{
}

VipBaseGraphicsView::~VipBaseGraphicsView()
{
	if (QGraphicsScene * sc = scene())
	{
		setScene(NULL);
		delete sc;
	}
	delete m_data;
}


void VipBaseGraphicsView::updateCacheMode(bool enable_cache) 
{
	if (!this->scene())
		return;
	QList<QGraphicsItem*> items = this->scene()->items();
	for (int i = 0; i < items.size(); ++i)
		if (VipAbstractPlotArea* a = qobject_cast<VipAbstractPlotArea*>(items[i]->toGraphicsObject()))
			::updateCacheMode(a, enable_cache);
}

void VipBaseGraphicsView::keyPressEvent(QKeyEvent* event)
{
	if (!scene() || !isInteractive()) {
		QAbstractScrollArea::keyPressEvent(event);
		return;
	}
	QApplication::sendEvent(scene(), event);
}

void VipBaseGraphicsView::setupViewport(QWidget* viewport)
{
	QGraphicsView::setupViewport(viewport);
	Q_EMIT viewportChanged(viewport);
}

void VipBaseGraphicsView::setRenderingMode(RenderingMode mode)
{
	if (m_data->useInternalViewport)
		return;

	if (mode == OpenGL) {
		if (renderingMode() == OpenGL)
			return;
		this->setViewport(new QOpenGLWidget());
		this->setAttribute(Qt::WA_PaintUnclipped, false);
		this->setAttribute(Qt::WA_NoSystemBackground, false);
		this->setAttribute(Qt::WA_OpaquePaintEvent, false);
		this->viewport()->setStyleSheet("QOpenGLWidget{background: transparent;}");

		// enable back item's painting
		QList<QGraphicsItem*> items = scene()->items();
		for (int i = 0; i < items.size(); ++i) {
			if (QGraphicsObject* o = items[i]->toGraphicsObject())
				if (VipPaintItem* it = o->property("VipPaintItem").value<VipPaintItem*>())
					it->setPaintingEnabled(true);
		}
		updateCacheMode(true);
		update();
		return;
	}

	// restore attributes
	this->setAttribute(Qt::WA_PaintUnclipped, true);
	this->setAttribute(Qt::WA_NoSystemBackground, true);
	this->setAttribute(Qt::WA_OpaquePaintEvent, true);

	if (mode == OpenGLThread) {
		if (renderingMode() == OpenGLThread)
			return;
		setViewport(new VipOpenGLWidget());

		// enable back item's painting
		QList<QGraphicsItem*> items = scene()->items();
		for (int i = 0; i < items.size(); ++i) {
			if (QGraphicsObject* o = items[i]->toGraphicsObject())
				if (VipPaintItem* it = o->property("VipPaintItem").value<VipPaintItem*>())
					it->setPaintingEnabled(true);
		}
		updateCacheMode(true);
		update();
		return;
	}

	if (mode == Raster) {
		if (renderingMode() == Raster)
			return;
		setViewport(new QWidget());
		updateCacheMode(false);
		update();
		return;
	}
}

VipBaseGraphicsView::RenderingMode VipBaseGraphicsView::renderingMode() const
{
	if (qobject_cast<QOpenGLWidget*>(viewport()))
		return OpenGL;
	if (qobject_cast<VipOpenGLWidget*>(viewport()))
		return OpenGLThread;
	return Raster;
}
bool VipBaseGraphicsView::isOpenGLBasedRendering() const
{
	return qobject_cast<QOpenGLWidget*>(viewport()) != nullptr || qobject_cast<VipOpenGLWidget*>(viewport()) != nullptr;
}

void VipBaseGraphicsView::setUseInternalViewport(bool enable)
{
	m_data->useInternalViewport = enable;
}
bool VipBaseGraphicsView::useInternalViewport() const {
	return m_data->useInternalViewport;
}

void VipBaseGraphicsView::startRender(VipRenderState &)
{
	updateCacheMode( false);
}

void VipBaseGraphicsView::endRender(VipRenderState &)
{
	updateCacheMode(isOpenGLBasedRendering());
}

bool VipBaseGraphicsView::renderObject(QPainter * p, const QPointF & pos, bool draw_background)
{
	//default behavior for QWidget: use QWidget::render
	if (isVisible())
	{
		if (!draw_background)
		{
			//TODO: test with tokida
			if (false)//QOpenGLWidget * gl = qobject_cast<QOpenGLWidget*>(viewport()))
			{
				//(void)gl;
				if (scene())
				{

					p->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
					QRectF visible = mapToScene(viewport()->geometry()).boundingRect();

					QRectF target(QPointF(0, 0), p->worldTransform().map(QRectF(QPointF(0, 0), this->size())).boundingRect().size());

					QPixmap pix1(target.size().toSize()), pix2(target.size().toSize());
					{
						//fill with this technique to set the transparent background
						QPainter painter(&pix1);
						painter.setCompositionMode(QPainter::CompositionMode_Clear);
						painter.fillRect(0, 0, pix1.width(), pix1.height(), QColor(230, 230, 230, 0));
					}

					pix2.fill(Qt::transparent);
					QPainter pa1(&pix1), pa2(&pix2);

					pa1.setTransform(QTransform().scale(double(target.width()) / this->width(), double(target.height()) / this->height()));
					pa1.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
					this->QWidget::render(&pa1, QPoint(0, 0), QRegion(), QWidget::DrawChildren);


					pa2.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
					scene()->render(&pa2, target, visible);

					p->save();
					p->setCompositionMode(QPainter::CompositionMode_Source);
					p->drawPixmap(QRectF(pos, this->size()), pix1, target);
					p->restore();
					p->drawPixmap(QRectF(pos, this->size()), pix2, target);
				}
			}
			else
			{

				QRectF target = geometry();
				target.moveTopLeft(pos);
				this->render(p, target);

			}
			return false;
		}
		else {
			//QOpenGLWidget* w = qobject_cast<QOpenGLWidget*>(this->viewport());
			QWidget::render(p, pos.toPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
			/*if (w) {
				QBrush b = this->palette().background();
				QColor c = this->palette().color(QPalette::Window);
				b.setColor(c);
				p->fillRect(QRect(pos.toPoint(), this->size()), b);
			}*/
			return true;
		}

	}
	return false;
}

void VipBaseGraphicsView::paintEvent(QPaintEvent * evt)
{
	QColor c;
	if (hasBackgroundColor())
		c = backgroundColor();
	else
		c = qApp->palette(this).color(QPalette::Window);

	VipOpenGLWidget* w = qobject_cast<VipOpenGLWidget*>(viewport());
	if(w)
		w->startRendering();
	//qint64 st = QDateTime::currentMSecsSinceEpoch();
	{
		QPainter p(viewport());
		p.fillRect(QRect(0, 0, width(), height()), c);
	}
	QGraphicsView::paintEvent(evt);
	if(w)
		w->stopRendering();
	//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//printf("el : %i ms\n", (int)el);
}


QRectF VipBaseGraphicsView::visualizedSceneRect() const
{
	return VipBorderItem::visualizedSceneRect(this);
}

QColor VipBaseGraphicsView::backgroundColor() const
{
	return m_data->backgroundColor ? *m_data->backgroundColor : QColor();
}
bool VipBaseGraphicsView::hasBackgroundColor() const
{
	return m_data->backgroundColor;
}
void VipBaseGraphicsView::removeBackgroundColor()
{
	m_data->backgroundColor.reset();
}
void VipBaseGraphicsView::setBackgroundColor(const QColor & color)
{
	m_data->backgroundColor.reset(new QColor(color));
	update();
}





VipAbstractPlotWidget2D::VipAbstractPlotWidget2D(QWidget* parent)
  : VipBaseGraphicsView(parent)
{
}

VipAbstractPlotWidget2D::VipAbstractPlotWidget2D(QGraphicsScene* scene, QWidget* parent)
  : VipBaseGraphicsView(scene,parent)
{
}
	

void VipAbstractPlotWidget2D::setArea(VipAbstractPlotArea* area)
{
	
	d_area = area;
	
	if (area && !scene()->focusItem())
		area->setFocus();

	this->updateCacheMode(isOpenGLBasedRendering());
}

VipAbstractPlotArea* VipAbstractPlotWidget2D::area() const
{
	return d_area;
}

VipAxisColorMap* VipAbstractPlotWidget2D::createColorMap(VipAxisBase::Alignment a, const VipInterval& i, VipColorMap* m)
{
	return area()->createColorMap(a, i, m);
}

void VipAbstractPlotWidget2D::resizeEvent(QResizeEvent* event)
{
	QGraphicsView::resizeEvent(event);
	this->recomputeGeometry();
}




#include "VipMultiPlotWidget2D.h"


VipPlotWidget2D::VipPlotWidget2D(QWidget* parent, QGraphicsScene * sc, AreaType type)
	: VipAbstractPlotWidget2D(sc,parent)
{
	if (type == Simple)
		d_area = new VipPlotArea2D();
	else
		d_area = new VipVMultiPlotArea2D();

	//Set-up the scene
	//QGraphicsScene * sc = new QGraphicsScene(this);
	//setScene(sc);
	viewport()->setMouseTracking(true);

	scene()->addItem(d_area);
	scene()->setSceneRect(QRectF(0, 0, 1000, 1000));

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

	area()->setGeometry(visualizedSceneRect());

	VipAbstractPlotWidget2D::setArea(d_area);
}

VipPlotWidget2D:: ~VipPlotWidget2D()
{}

VipPlotArea2D * VipPlotWidget2D::area() const
{
	return const_cast<VipPlotArea2D*>(d_area);
}

void VipPlotWidget2D::setArea(VipPlotArea2D * area)
{
	if (area != d_area && area)
	{
		if (d_area)
			delete d_area;

		d_area = area;
		scene()->addItem(d_area);
		d_area->setGeometry(visualizedSceneRect());
		VipAbstractPlotWidget2D::setArea(area);
	}
}

void VipPlotWidget2D::recomputeGeometry()
{
	QRectF scene_rect = visualizedSceneRect();
	d_area->setGeometry(scene_rect);
	d_area->recomputeGeometry();
};






VipPlotPolarWidget2D::VipPlotPolarWidget2D(QWidget* parent, QGraphicsScene * sc)
	:VipAbstractPlotWidget2D(sc, parent)
{
	d_area = new VipPlotPolarArea2D();

	//Set-up the scene
	//QGraphicsScene * sc = new QGraphicsScene(this);
	//setScene(sc);
	viewport()->setMouseTracking(true);

	scene()->addItem(d_area);
	scene()->setSceneRect(QRectF(0, 0, 1000, 1000));


	//setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

	area()->setGeometry(sceneRect());
	VipAbstractPlotWidget2D::setArea(d_area);
}

VipPlotPolarWidget2D::~VipPlotPolarWidget2D()
{

}


void VipPlotPolarWidget2D::setArea(VipPlotPolarArea2D* area)
{
	if (area != d_area && area)
	{
		if (d_area)
			delete d_area;

		d_area = area;
		scene()->addItem(d_area);
		d_area->setGeometry(visualizedSceneRect());
		VipAbstractPlotWidget2D::setArea(area);
	}
}

VipPlotPolarArea2D * VipPlotPolarWidget2D::area() const
{
	return d_area;
}


void VipPlotPolarWidget2D::recomputeGeometry()
{
	//this->setSceneRect(QRectF(0, 0, width(), height()));
	// d_area->setGeometry(QRectF(0, 0, width(), height()));
	QRectF scene_rect = QRectF(0, 0, width(), height()); //sceneRect();
	setSceneRect(scene_rect);
	d_area->setGeometry(scene_rect);
	d_area->recomputeGeometry();
}






static int registerImageAreaKeywords()
{
	VipKeyWords keys;
	keys["keep-aspect-ratio"] = VipParserPtr(new BoolParser());

	vipSetKeyWordsForClass(&VipImageArea2D::staticMetaObject, keys);
	return 0;
}
static int _registerImageAreaKeywords = registerImageAreaKeywords();

class VipImageArea2D::PrivateData
{
public:
	PrivateData() : spectrogram(NULL), colorMap(NULL), keepAspectRatio(true){}
	QPointer<VipPlotSpectrogram> spectrogram;
	VipAxisColorMap * colorMap;
	QRectF imageRect;
	bool keepAspectRatio;
};



VipImageArea2D::VipImageArea2D(QGraphicsItem * parent)
	:VipPlotArea2D(parent)
{
	d_data = new PrivateData;
	d_data->colorMap = this->createColorMap(VipAxisBase::Right, VipInterval(0, 100), VipLinearColorMap::createColorMap(VipLinearColorMap::Jet));
	setSpectrogram(new VipPlotSpectrogram());

	leftAxis()->setAutoScale(false);
	rightAxis()->setAutoScale(false);
	topAxis()->setAutoScale(false);
	bottomAxis()->setAutoScale(false);
	grid()->setZValue(d_data->spectrogram->zValue() + 1);

	connect(leftAxis(), SIGNAL(scaleDivChanged(bool)), this, SLOT(emitVisualizedAreaChanged()));
	connect(bottomAxis(), SIGNAL(scaleDivChanged(bool)), this, SLOT(emitVisualizedAreaChanged()));
}

VipImageArea2D::~VipImageArea2D()
{
	//if(d_data->spectrogram)
	// delete d_data->spectrogram.data();

	delete d_data;
}


bool VipImageArea2D::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	if (strcmp(name, "keep-aspect-ratio") == 0) {
		setKeepAspectRatio(value.toBool());
		return true;
	}
	return VipPlotArea2D::setItemProperty(name, value, index);
}


void VipImageArea2D::setKeepAspectRatio(bool enable)
{
	if (d_data->keepAspectRatio != enable) {
		d_data->keepAspectRatio = enable;
		recomputeGeometry();
	}
}
bool VipImageArea2D::keepAspectRatio() const
{
	return d_data->keepAspectRatio;
}

void VipImageArea2D::setSpectrogram(VipPlotSpectrogram * spectrogram)
{
	if (d_data->spectrogram == spectrogram)
		return;

	if (d_data->spectrogram)
		delete d_data->spectrogram;

	d_data->spectrogram = spectrogram;

	if (spectrogram)
	{
		d_data->spectrogram->setAxes(QList<VipAbstractScale*>() << this->bottomAxis() << this->leftAxis(), VipCoordinateSystem::Cartesian);
		d_data->spectrogram->setItemAttribute(VipPlotItem::ClipToScaleRect);
		d_data->spectrogram->setHoverEffect();;
		d_data->spectrogram->setSelectedEffect();

		if (d_data->colorMap)
		{
			d_data->spectrogram->setColorMap(d_data->colorMap);

			//hide or show the color map
			const VipRasterData data = d_data->spectrogram->rawData();
			int data_type = data.dataType();
			if (data_type == qMetaTypeId<QImage>() || data_type == 0)
				d_data->colorMap->setVisible(false);
			else
				d_data->colorMap->setVisible(true);
		}

		grid()->setZValue(d_data->spectrogram->zValue() + 1);
		connect(d_data->spectrogram, SIGNAL(imageRectChanged(const QRectF &)), this, SLOT(receiveNewRect(const QRectF &)), Qt::AutoConnection);
	}
}

void VipImageArea2D::setAxisColorMap(VipAxisColorMap * map)
{
	if (d_data->colorMap == map)
		return;

	if (d_data->colorMap)
		delete d_data->colorMap;

	d_data->colorMap = map;

	if (d_data->spectrogram && map)
		d_data->spectrogram->setColorMap(map);
}

double VipImageArea2D::zoom() const
{
	return qAbs(bottomAxis()->position(1).x() - bottomAxis()->position(0).x());
}

QRectF VipImageArea2D::imageBoundingRect() const
{
	return d_data->spectrogram->imageBoundingRect();
}

QRectF VipImageArea2D::imageRect() const
{
	QRectF r = d_data->spectrogram->imageBoundingRect();
	r.setLeft(0);
	r.setTop(0);
	return r;
}

QRectF VipImageArea2D::visualizedImageRect() const
{
	QPointF top_left(bottomAxis()->scaleDiv().bounds().minValue(), leftAxis()->scaleDiv().bounds().maxValue());
	QPointF bottom_right(bottomAxis()->scaleDiv().bounds().maxValue(), leftAxis()->scaleDiv().bounds().minValue());
	return QRectF(top_left, bottom_right).normalized();
}


void VipImageArea2D::receiveNewRect(const QRectF & rect)
{
	if (d_data->imageRect != rect)
	{
		d_data->imageRect = rect;
		bottomAxis()->setScale(0, rect.right());
		leftAxis()->setScale(rect.bottom(), 0);
		this->recomputeGeometry(rect);
	}
}

void VipImageArea2D::setArray(const VipNDArray & ar, const QPointF & image_offset)
{
	VipRasterData data(ar, image_offset);
	d_data->spectrogram->setRawData(data);
}

void VipImageArea2D::setImage(const QImage & image, const QPointF & image_offset)
{
	this->setArray(vipToArray(image), image_offset);
}

void VipImageArea2D::setPixmap(const QPixmap & image, const QPointF & image_offset)
{
	this->setArray(vipToArray(image.toImage()), image_offset);
}

VipNDArray VipImageArea2D::array() const
{
	return d_data->spectrogram->rawData().extract(d_data->spectrogram->imageBoundingRect());
}

VipPlotSpectrogram * VipImageArea2D::spectrogram() const
{
	return const_cast<VipPlotSpectrogram*>(d_data->spectrogram.data());
}

VipAxisColorMap* VipImageArea2D::colorMapAxis() const
{
	return const_cast<VipAxisColorMap*>(d_data->colorMap);
}

void VipImageArea2D::emitVisualizedAreaChanged()
{
	Q_EMIT visualizedAreaChanged();
}

void VipImageArea2D::recomputeGeometry(const QRectF& visualized_image_rect, bool recompute_aligned_areas)
{
	if (d_data->spectrogram->imageBoundingRect().isValid())
	{
		QRectF inner_rect = innerRect();
		QRectF outer_rect = outerRect();

		double left_axis = (inner_rect.left() - outer_rect.left());
		double top_axis = (inner_rect.top() - outer_rect.top());
		double right_axis = (outer_rect.right() - inner_rect.right());
		double bottom_axis = (outer_rect.bottom() - inner_rect.bottom());

		QRectF scene_rect = boundingRect();//visualizedSceneRect();
		QRectF usable_scene_rect = scene_rect;
		usable_scene_rect.setLeft(usable_scene_rect.left() + left_axis);
		usable_scene_rect.setTop(usable_scene_rect.top() + top_axis);
		usable_scene_rect.setRight(usable_scene_rect.right() - right_axis);
		usable_scene_rect.setBottom(usable_scene_rect.bottom() - bottom_axis);

		QRectF im_bounding_rect = d_data->spectrogram->imageBoundingRect().normalized();
		QRectF requested_rect = visualized_image_rect;

		if (requested_rect.left() < im_bounding_rect.left())
			requested_rect.setLeft(im_bounding_rect.left());
		if (requested_rect.right() > im_bounding_rect.right())
			requested_rect.setRight(im_bounding_rect.right());
		if (requested_rect.top() < im_bounding_rect.top())
			requested_rect.setTop(im_bounding_rect.top());
		if (requested_rect.bottom() > im_bounding_rect.bottom())
			requested_rect.setBottom(im_bounding_rect.bottom());

		//
		//adjust the requested rect to the real image rect without changing the visualized width/height ratio
		//

		//change the left and right position if necessary
		//EDIT: commented and replaced by above if conditions
		//if (requested_rect.width() > im_bounding_rect.width())
		// {
		// //center horizontally
		// requested_rect.moveLeft(im_bounding_rect.left() - (requested_rect.width() - im_bounding_rect.width()) / 2);
		// }
		// else if (requested_rect.left() < im_bounding_rect.left())
		// {
		// //move to the right
		// requested_rect.moveLeft(im_bounding_rect.left());
		// }
		// else if (requested_rect.right() > im_bounding_rect.right())
		// {
		// //move to the left
		// requested_rect.moveRight(im_bounding_rect.right());
		// }
//
		// //change the top and bottom position if necessary
		// if (requested_rect.height() > im_bounding_rect.height())
		// {
		// //center horizontally
		// requested_rect.moveTop(im_bounding_rect.top() - (requested_rect.height() - im_bounding_rect.height()) / 2);
		// }
		// else if (requested_rect.top() < im_bounding_rect.top())
		// {
		// //move to the right
		// requested_rect.moveTop(im_bounding_rect.top());
		// }
		// else if (requested_rect.bottom() > im_bounding_rect.bottom())
		// {
		// //move to the left
		// requested_rect.moveBottom(im_bounding_rect.bottom());
		// }



		double scene_w_on_h = usable_scene_rect.width() / usable_scene_rect.height();
		double image_w_on_h = requested_rect.width() / requested_rect.height();

		//
		// if necessary, expand the requested rect width or height to show more pixels
		//
		if (scene_w_on_h > image_w_on_h)
		{
			double missing_pixels = im_bounding_rect.width() - requested_rect.width();
			if (missing_pixels > 0)
			{
				//we can enlarge the width to show more pixels

				//the height dictate the image pixel size
				double im_pixel_size = usable_scene_rect.height() / requested_rect.height();
				double requested_width = requested_rect.width() * im_pixel_size;
				double additional_pixels = qMin(missing_pixels, (usable_scene_rect.width() - requested_width) / im_pixel_size);
				//adjust requested rect width
				requested_rect.setLeft(requested_rect.left() - additional_pixels / 2);
				requested_rect.setRight(requested_rect.right() + additional_pixels / 2);
				if (requested_rect.left() < im_bounding_rect.left())
					requested_rect.moveLeft(im_bounding_rect.left());
				else if (requested_rect.right() > im_bounding_rect.right())
					requested_rect.moveRight(im_bounding_rect.right());
			}
		}
		else
		{
			double missing_pixels = im_bounding_rect.height() - requested_rect.height();
			if (missing_pixels > 0)
			{
				//we can enlarge the height to show more pixels

				//the width dictate the image pixel size
				double im_pixel_size = usable_scene_rect.width() / requested_rect.width();
				double requested_height = requested_rect.height() * im_pixel_size;
				double additional_pixels = qMin(missing_pixels, (usable_scene_rect.height() - requested_height) / im_pixel_size);
				//adjust requested rect height
				requested_rect.setTop(requested_rect.top() - additional_pixels / 2);
				requested_rect.setBottom(requested_rect.bottom() + additional_pixels / 2);
				if (requested_rect.top() < im_bounding_rect.top())
					requested_rect.moveTop(im_bounding_rect.top());
				else if (requested_rect.bottom() > im_bounding_rect.bottom())
					requested_rect.moveBottom(im_bounding_rect.bottom());
			}
		}

		//if requested rect englobes image rect, try to reduce its size by fitting to the imag rect width/height
		if (requested_rect.width() > im_bounding_rect.width() && requested_rect.height() > im_bounding_rect.height())
		{
			double request_w_on_h = requested_rect.width() / requested_rect.height();
			double im_w_on_h = im_bounding_rect.width() / im_bounding_rect.height();
			if (request_w_on_h > im_w_on_h)
			{
				//reduce width
				double new_width = requested_rect.height() * im_w_on_h;
				new_width = qMax(new_width, im_bounding_rect.width());
				requested_rect.setLeft(im_bounding_rect.center().x() - new_width / 2);
				requested_rect.setRight(im_bounding_rect.center().x() + new_width / 2);
			}
			else
			{
				//reduce height
				double new_height = requested_rect.width() / im_w_on_h;
				new_height = qMax(new_height, im_bounding_rect.height());
				requested_rect.setTop(im_bounding_rect.center().y() - new_height / 2);
				requested_rect.setBottom(im_bounding_rect.center().y() + new_height / 2);
			}
		}

		//
		// now, place the requested rect inside the scene rect
		//

		image_w_on_h = requested_rect.width() / requested_rect.height();
		if (scene_w_on_h > image_w_on_h)
		{
			usable_scene_rect.setLeft(usable_scene_rect.left() + (usable_scene_rect.width() - usable_scene_rect.height() * image_w_on_h) / 2);
			usable_scene_rect.setWidth(usable_scene_rect.height() * image_w_on_h);
		}
		else
		{
			usable_scene_rect.setTop(usable_scene_rect.top() + (usable_scene_rect.height() - usable_scene_rect.width() / image_w_on_h) / 2);
			usable_scene_rect.setHeight(usable_scene_rect.width() / image_w_on_h);
		}
		usable_scene_rect.setLeft(usable_scene_rect.left() - left_axis);
		usable_scene_rect.setTop(usable_scene_rect.top() - top_axis);
		usable_scene_rect.setRight(usable_scene_rect.right() + right_axis);
		usable_scene_rect.setBottom(usable_scene_rect.bottom() + bottom_axis);
		scene_rect = scene_rect & usable_scene_rect;

		//update area scales
		this->bottomAxis()->setScale(requested_rect.left(), requested_rect.right());
		this->leftAxis()->setScale(requested_rect.bottom(), requested_rect.top());

		//update area geometry
		if (scene_rect.isValid() && keepAspectRatio())
		{
			//this->blockSignals(true);
			this->setMargins(scene_rect);
			//this->blockSignals(false);
			this->update();
		}
	}

	VipPlotArea2D::recomputeGeometry(recompute_aligned_areas);
}

void VipImageArea2D::recomputeGeometry(bool recompute_aligned_areas)
{
	//VipPlotArea2D::recomputeGeometry();
	recomputeGeometry(visualizedImageRect(), recompute_aligned_areas);
}

void VipImageArea2D::setVisualizedImageRect(const QRectF& rect) 
{
	recomputeGeometry(rect);
}



VipImageWidget2D::VipImageWidget2D(QWidget* parent, QGraphicsScene * sc)
	: VipAbstractPlotWidget2D(sc, parent), d_mouseInsideCanvas(false), d_scrollBarEnabled(true)
{
	d_area = new VipImageArea2D();

	//Set-up the scene
	//QGraphicsScene * sc = new QGraphicsScene(this);
	//setScene(sc);
	viewport()->setMouseTracking(true);

	scene()->addItem(d_area);
	scene()->setSceneRect(QRectF(0, 0, 1000, 1000));
	area()->setGeometry(visualizedSceneRect());

	//setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	horizontalScrollBar()->disconnect();
	verticalScrollBar()->disconnect();
	horizontalScrollBar()->setSingleStep(1);
	verticalScrollBar()->setSingleStep(1);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	connect(horizontalScrollBar(), SIGNAL(actionTriggered(int)), this, SLOT(hScrollBarsMoved()), Qt::QueuedConnection);
	connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), this, SLOT(vScrollBarsMoved()), Qt::QueuedConnection);
	connect(area(), SIGNAL(visualizedAreaChanged()), this, SLOT(computeScrollBars()), Qt::QueuedConnection);

	VipAbstractPlotWidget2D::setArea(d_area);

	d_timer = new QTimer;
	d_timer->setSingleShot(false);
	d_timer->setInterval(300);

	connect(d_timer, SIGNAL(timeout()), this, SLOT(mouseTimer()));

}

VipImageWidget2D::~VipImageWidget2D()
{
	disconnect(d_timer, SIGNAL(timeout()), this, SLOT(mouseTimer()));
	d_timer->stop();
	delete d_timer;
}

VipImageArea2D * VipImageWidget2D::area() const
{
	return const_cast<VipImageArea2D*>(d_area);
}

void VipImageWidget2D::recomputeGeometry()
{
	QRectF scene_rect = visualizedSceneRect();
	d_area->setGeometry(scene_rect);
	d_area->recomputeGeometry();
	computeScrollBars();
}

void VipImageWidget2D::setScrollBarEnabled(bool enable)
{
	d_scrollBarEnabled = enable;
	if (!enable)
	{
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		area()->recomputeGeometry();
	}
	else
	{
		computeScrollBars();
	}
}

bool VipImageWidget2D::scrollBarEnabled() const
{
	return d_scrollBarEnabled;
}

void VipImageWidget2D::computeScrollBars()
{
	if (!d_scrollBarEnabled)
		return;

	horizontalScrollBar()->disconnect();
	verticalScrollBar()->disconnect();


	QRectF visualized_image_rect = area()->visualizedImageRect();
	QRectF image_rect = area()->imageRect();
	bool state_changed = false;

	if (visualized_image_rect.width() < image_rect.width())
	{
		if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOn)
		{
			setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
			state_changed = true;
		}
		horizontalScrollBar()->setRange(0, std::ceil(image_rect.width() - visualized_image_rect.width()));
		horizontalScrollBar()->setValue(visualized_image_rect.left());
	}
	else
	{
		if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
		{
			setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			state_changed = true;
		}
	}

	if (visualized_image_rect.height() < image_rect.height())
	{
		if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOn)
		{
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
			state_changed = true;
		}
		verticalScrollBar()->setRange(0, std::ceil(image_rect.height() - visualized_image_rect.height()));
		verticalScrollBar()->setValue(visualized_image_rect.top());
	}
	else
	{
		if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
		{
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			state_changed = true;
		}
	}

	if (state_changed)
	{
		area()->recomputeGeometry();
	}

	connect(horizontalScrollBar(), SIGNAL(actionTriggered(int)), this, SLOT(hScrollBarsMoved()), Qt::QueuedConnection);
	connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), this, SLOT(vScrollBarsMoved()), Qt::QueuedConnection);

}

void VipImageWidget2D::vScrollBarsMoved()
{
	QRectF new_visualizedImageRect = area()->visualizedImageRect();
	new_visualizedImageRect.moveTop(verticalScrollBar()->value());
	new_visualizedImageRect.moveBottom(qMin(new_visualizedImageRect.bottom(), area()->imageRect().bottom()));

	disconnect(area(), SIGNAL(visualizedAreaChanged()), this, SLOT(computeScrollBars()));
	area()->setVisualizedImageRect(new_visualizedImageRect);
	connect(area(), SIGNAL(visualizedAreaChanged()), this, SLOT(computeScrollBars()), Qt::QueuedConnection);
}

void VipImageWidget2D::hScrollBarsMoved()
{
	QRectF new_visualizedImageRect = area()->visualizedImageRect();
	new_visualizedImageRect.moveLeft(horizontalScrollBar()->value());
	new_visualizedImageRect.moveRight(qMin(new_visualizedImageRect.right(), area()->imageRect().right()));

	disconnect(area(), SIGNAL(visualizedAreaChanged()), this, SLOT(computeScrollBars()));
	area()->setVisualizedImageRect(new_visualizedImageRect);
	connect(area(), SIGNAL(visualizedAreaChanged()), this, SLOT(computeScrollBars()), Qt::QueuedConnection);
}


void	VipImageWidget2D::mouseMoveEvent(QMouseEvent * event)
{
	VipAbstractPlotWidget2D::mouseMoveEvent(event);
}

void	VipImageWidget2D::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton)
	{
		QPointF pos = this->mapToScene(event->pos());
		if (this->area() && this->area()->spectrogram()) {
			pos = this->area()->spectrogram()->mapFromScene(pos);
			d_mouseInsideCanvas = this->area()->spectrogram()->shape().contains(pos);
			d_timer->start();
		}
		else
			d_mouseInsideCanvas = false;

	}
	VipAbstractPlotWidget2D::mousePressEvent(event);
}

void	VipImageWidget2D::mouseReleaseEvent(QMouseEvent * event)
{
	d_timer->stop();
	d_mouseInsideCanvas = false;
	VipAbstractPlotWidget2D::mouseReleaseEvent(event);
}

void VipImageWidget2D::mouseTimer()
{
	//disable for now as it does not work well with other functionalities

	/* if (!(QApplication::mouseButtons() & Qt::LeftButton))
		return;
	if (!d_mouseInsideCanvas)
		return;

	QPointF pos = this->mapToScene(this->mapFromGlobal(QCursor::pos()));
	QRectF rect = this->area()->innerRect();
	QRectF color_map_rect = this->area()->colorMapAxis() ? this->area()->colorMapAxis()->geometry() : QRectF();
	color_map_rect.moveTopLeft(color_map_rect.topLeft() - rect.topLeft());
	pos -= rect.topLeft();
	rect.moveTopLeft(QPointF(0, 0));

	//do nothing if the mouse is inside the color map
	if (color_map_rect.isValid() && color_map_rect.contains(pos))
		return;

	int vipDistance = 20;

	if (pos.x() < vipDistance)
	{
		//left border
		if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
		{
			this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - 10);
			hScrollBarsMoved();
		}
	}
	else if (pos.x() > rect.width() - vipDistance)
	{
		//right border
		if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
		{
			this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() + 10);
			hScrollBarsMoved();
		}
	}

	if (pos.y() < vipDistance && pos.y() >= 0)
	{
		//top border
		if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
		{
			this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - 10);
			vScrollBarsMoved();
		}
	}
	else if (pos.y() > rect.height() - vipDistance)
	{
		//bottom border
		if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
		{
			this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() + 10);
			vScrollBarsMoved();
		}
	}*/
}






static bool registerVipMultiGraphicsWidget = vipSetKeyWordsForClass(&VipMultiGraphicsWidget::staticMetaObject);


VipMultiGraphicsWidget::VipMultiGraphicsWidget(QGraphicsItem * parent)
  : VipBoxGraphicsWidget(parent)
{ 
}



VipMultiGraphicsView::VipMultiGraphicsView(QGraphicsScene* scene, QWidget* parent)
  : VipBaseGraphicsView(scene, parent)
{
	d_widget = new VipMultiGraphicsWidget();
	scene->addItem(d_widget);

	this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}
VipMultiGraphicsView::VipMultiGraphicsView(QWidget* parent)
  : VipMultiGraphicsView(new QGraphicsScene(), parent)
{
}

VipMultiGraphicsWidget* VipMultiGraphicsView::widget() const
{
	return const_cast<VipMultiGraphicsWidget*>(d_widget);
}

void VipMultiGraphicsView::resizeEvent(QResizeEvent* event)
{
	QGraphicsView::resizeEvent(event);
	setSceneRect(QRectF(0, 0, width(), height()));
	QRectF scene_rect = visualizedSceneRect();
	d_widget->setGeometry(scene_rect);
}




