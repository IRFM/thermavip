#include <QCursor>
#include <QKeyEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QGraphicsPathItem>
#include <qpaintengine.h>
#include <qapplication.h>

#include <cmath>
#include <limits>

#include "VipAbstractScale.h"
#include "VipResizeItem.h"
#include "VipPainter.h"
#include "VipPlotShape.h"
#include "VipPlotWidget2D.h"
#include "VipPlotWidget2D.h"



static bool hasSelectedResizeItem(VipResizeItem* item)
{
	//check if given VipResizeItem manage another selected VipResizeItem
	const PlotItemList lst = item->managedItems();
	for (int i = 0; i < lst.size(); ++i) {
		if (VipResizeItem* child = qobject_cast<VipResizeItem*>(lst[i])) {
			if (child->isSelected()) return true;
			if (hasSelectedResizeItem(child)) return true;
		}
	}
	return false;
}


class GraphicsPathItem : public QGraphicsItem
{
	VipResizeItem * parent;
	QPainterPath sh;
	QPen p;
	QBrush b;

public:
	GraphicsPathItem(VipResizeItem * p) : QGraphicsItem(p), parent(p) {}
	virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * , QWidget *  = 0)
	{
		//check parent item visibility
		const QPainterPath scales = parent->sceneMap()->clipPath(parent);
		if (!scales.intersects(parent->shape()))
			return;


		if (parent->testItemAttribute(VipResizeItem::ClipToScaleRect))
			painter->setClipPath(parent->sceneMap()->clipPath(parent), Qt::IntersectClip);

		//TEST: remove Qt::WindingFill
		sh.setFillRule(Qt::WindingFill);
		painter->setPen(pen());
		painter->setBrush(brush());

		bool has_antialize = painter->testRenderHint(QPainter::Antialiasing);
		if(!has_antialize)
			painter->setRenderHint(QPainter::Antialiasing, true);
		//QT 5.6 crashes when drawing path on a GL paint engine, so draw a polygon instead
		if (painter->paintEngine()->type() == QPaintEngine::OpenGL || painter->paintEngine()->type() == QPaintEngine::OpenGL2)
			painter->drawPolygon(path().toFillPolygon());
		else
			painter->drawPath(path());

		if (!has_antialize)
			painter->setRenderHint(QPainter::Antialiasing, false);

		//VipPainter::drawPath(painter,sh);
		//QGraphicsPathItem::paint(painter, option, widget);
	}

	void setPath(const QPainterPath & path) {
		if (path.boundingRect() != sh.boundingRect())
		{
			sh = path;
			prepareGeometryChange();
			update();
		}
	}

	const QPainterPath & path() const { return sh; }
	virtual QPainterPath shape() const { return sh; }
	virtual QRectF boundingRect() const { return sh.boundingRect(); }

	void setPen(const QPen & pen) { p = pen; update(); }
	const QPen & pen() const { return p; }

	void setBrush(const QBrush & brush) { b = brush; update(); }
	const QBrush & brush() const { return b; }

};



class ResizeItemRotate : public QGraphicsItem
{
	QPointF m_press;
	bool m_pressed;
	bool m_insideRotate;
	bool m_changed;
public:

	ResizeItemRotate(VipResizeItem* parent)
		:QGraphicsItem(parent), m_pressed(false), m_insideRotate(false), m_changed(false)
	{}

	VipResizeItem* parentItem() const { return static_cast<VipResizeItem*>(QGraphicsItem::parentItem()); }

	virtual QRectF boundingRect() const
	{
		QRectF bounding = parentItem()->boundingRect();
		QSizeF size(16, 16);
		QPointF pos(bounding.left() + bounding.width() / 2 - size.width() / 2, bounding.top() - size.height());
		return QRectF(pos, size);
	}
	virtual QPainterPath shape() const {
		QPainterPath p;
		p.addRect(boundingRect());
		return p;
	}
	QPixmap drawCursor() const
	{
		static QPixmap pix;
		QSize s = boundingRect().size().toSize();
		if (pix.size() != s) {
			pix = QPixmap(s);
			pix.fill(Qt::transparent);
			QPainter p(&pix);
			p.setRenderHint(QPainter::HighQualityAntialiasing);
			p.setRenderHint(QPainter::Antialiasing);
			QPainterPath path;
			QRectF arc(3, 1, s.width() - 4, s.height() - 4);
			path.arcMoveTo(arc, -45);
			path.arcTo(arc, -45, 270);
			QPointF pos = path.currentPosition();

			QPainterPath arrow;
			QPointF arrow1 = pos + QPointF(-5, -1);
			QPointF arrow2 = pos + QPointF(1,-4);
			arrow.moveTo(pos);
			arrow.lineTo(arrow1);
			arrow.moveTo(pos);
			arrow.lineTo(arrow2);
			arrow.lineTo(arrow1);

			if(VipResizeItem * item = qobject_cast<VipResizeItem*>(parentItem()->toGraphicsObject()))
				p.setPen(item->pen());
			else
				p.setPen(Qt::black);
			

			//p.setPen(QPen(QColor(255,0,0,100),3));
			//p.setBrush(QBrush());
			//p.drawPath(path);
			//p.setPen(Qt::black);
			
			p.drawPath(path);
			p.setBrush(p.pen().color());
			p.drawPath(arrow);
		}
		return pix;
	}
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget* = 0)
	{
		if (parentItem()->isVisible() && parentItem()->testLibertyDegreeFlag(VipResizeItem::Rotate) &&!m_insideRotate){
			//painter->setPen(Qt::black);
			//painter->setBrush(QBrush());
			//painter->drawArc(boundingRect(), -45*16., 270*16.);
			//painter->setPen(Qt::green);
			//painter->drawRect(boundingRect());
			QRectF b = boundingRect();
			painter->setRenderHint(QPainter::SmoothPixmapTransform);
			painter->drawPixmap(b, drawCursor(), b.translated(-b.topLeft()));
		}
	}
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* evt)
	{
		m_pressed = true;
		m_insideRotate = true;
		m_press = evt->pos();
		const QPixmap pix = drawCursor();
		this->setCursor(QCursor(pix, pix.width() / 2, pix.height() / 2));
	}
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* evt)
	{
		(void)evt;
		m_press = QPointF();
		m_pressed = false;
		m_insideRotate = false;
		this->setCursor(QCursor());

		if (m_changed) {
			m_changed = false;
			Q_EMIT parentItem()->finishedChange();
		}

	}
	void applyTr(VipResizeItem* item, const QTransform& tr)
	{
		PlotItemList lst = item->managedItems();
		for (int i = 0; i < lst.size(); ++i) {
			if (VipResizeItem* it = qobject_cast<VipResizeItem*>(lst[i]))
				applyTr(it, tr);
			else {
				lst[i]->applyTransform(tr);
				lst[i]->update();
			}
		}
	}
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* evt)
	{
		if (!parentItem()->testLibertyDegreeFlag(VipResizeItem::Rotate))
			return;

		if (m_pressed) {
			m_pressed = false;
			Q_EMIT parentItem()->aboutToRotate();
		}
		m_changed = true;

		QPointF pos = evt->pos();

		//compute rotation angle
		QRectF brect = parentItem()->boundingRect();
		QLineF l1(brect.center(), m_press);
		QLineF l2(brect.center(), pos);
		double angle = l1.angle() - l2.angle();

		//bounding rect in axis coordinate
		QRectF bounding = parentItem()->sceneMap()->invTransform(parentItem()->boundingRect()).boundingRect();
		//rotation center in axis coordinates
		QPointF center = bounding.center();


		//compute transform
		QTransform tr;
		tr.translate(center.x(), center.y());
		tr.rotate(angle);
		tr.translate(-center.x(), -center.y());

		//parentItem()->managedItems().first()->applyTransform(tr);
		// parentItem()->managedItems().first()->update();
		// parentItem()->update();

		VipResizeItem* _this = parentItem();
		//first, apply to all managed items
		applyTr(_this, tr);
		//change geometry of all other selected items, except top level parent and children
		QList<VipResizeItem*> children = _this->children();
		QList<VipResizeItem*> shapes = _this->linkedResizeItems();
		VipResizeItem* top = _this->topLevelParentResizeItem();
		for (int i = 0; i < shapes.size(); ++i)
		{
			VipResizeItem* item = shapes[i];
			if (item->isSelected() && item != _this && item != top && children.indexOf(item) < 0 && !hasSelectedResizeItem(item))
			{
				//compute new transform
				QRectF bounding = shapes[i]->sceneMap()->invTransform(shapes[i]->boundingRect()).boundingRect();
				QPointF center = bounding.center();
				QTransform tr;
				tr.translate(center.x(), center.y());
				tr.rotate(angle);
				tr.translate(-center.x(), -center.y());
				applyTr(shapes[i],tr);
			}
		}

		m_press = pos;
	}
	virtual void keyPressEvent(QKeyEvent* evt)
	{
		parentItem()->sceneEventFilter(parentItem(),evt);
	}
};








enum Selection {
	All,
	TopLeft,
	TopRight,
	BottomRight,
	BottomLeft,
	Top,
	Right,
	Bottom,
	Left,
	Count
};



class VipResizeItem::PrivateData
{
public:
	PrivateData()
		:boundaries(NoBoundary),
		degrees(MoveAndResize),
		selection(-1),
		spacing(12),
		minimumSize(std::numeric_limits<double>::epsilon() * 10, std::numeric_limits<double>::epsilon() * 10),
		unitMoveAndResize(false),
		aboutTo(false),
		hasChanged(false),
		expandToFullArea(false),
		autodelete(true),
		plotSelection(NULL),
		parent(NULL),
		rotate(NULL)
	{
		boxStyle.setBorderPen(QPen());
	}

	Boundaries 		boundaries;
	LibertyDegrees 	degrees;
	QPointF 		mousePress;
	int 			selection;
	double 			spacing;
	GraphicsPathItem*	resizers[Count];
	QPainterPath resizerPath;
	VipBoxStyle boxStyle;

	QPainterPath customResizer[4]; //custom resizer for left,right, top and bottom

	QSizeF  minimumSize;
	bool unitMoveAndResize;
	bool autodelete;
	bool aboutTo;
	bool hasChanged;
	bool expandToFullArea;

	PlotItemList managed;
	VipPlotItem * plotSelection;
	VipResizeItem * parent;
	ResizeItemRotate* rotate;

	QRectF pressedRect;
	QTransform current;
	QRectF geometry;
};


VipResizeItem::VipResizeItem(const VipText & title)
:VipPlotItem(title), d_data(new PrivateData())
{
	this->setRenderHints(0);
	this->setAcceptHoverEvents(true);
	this->setFlag(QGraphicsItem::ItemIsFocusable,true);
	this->setFlag(QGraphicsItem::ItemIsSelectable,true);

	this->setItemAttribute(AutoScale,false);
	this->setItemAttribute(VisibleLegend,false);
	this->setItemAttribute(HasLegendIcon,false);
	this->setItemAttribute(SupportTransform,true);
	this->setItemAttribute(IsSuppressable,true);
	this->setItemAttribute(ClipToScaleRect, true);

	d_data->resizerPath.addEllipse(QRectF(0,0,8,8));

	for(int i=0; i < Count; ++i)
	{
		d_data->resizers[i] = new GraphicsPathItem(this);
		d_data->resizers[i]->setVisible(false);
		d_data->resizers[i]->setBrush(Qt::yellow);
	}

	d_data->resizers[All]->setBrush(QBrush());
	d_data->resizers[All]->setPen(QPen(Qt::transparent));
	d_data->resizers[All]->setCursor(Qt::SizeAllCursor);
	d_data->resizers[TopLeft]->setCursor(Qt::SizeFDiagCursor);
	d_data->resizers[TopRight]->setCursor(Qt::SizeBDiagCursor);
	d_data->resizers[BottomRight]->setCursor(Qt::SizeFDiagCursor);
	d_data->resizers[BottomLeft]->setCursor(Qt::SizeBDiagCursor);
	d_data->resizers[Top]->setCursor(Qt::SizeVerCursor);
	d_data->resizers[Right]->setCursor(Qt::SizeHorCursor);
	d_data->resizers[Bottom]->setCursor(Qt::SizeVerCursor);
	d_data->resizers[Left]->setCursor(Qt::SizeHorCursor);

	d_data->rotate = new ResizeItemRotate(this);
	d_data->rotate->setVisible(false);
}

VipResizeItem::~VipResizeItem()
{
	if(autoDelete())
	{
		for(int i=0; i < d_data->managed.size(); ++i)
		{
			d_data->managed[i]->deleteLater();
		}
	}
	delete d_data;
}

void VipResizeItem::setCustomLeftResizer(const QPainterPath& p)
{
	d_data->customResizer[Left - Top] = p.translated(-p.boundingRect().topLeft());
}
const QPainterPath& VipResizeItem::customLeftResizer() const {
	return d_data->customResizer[Left - Top];
}
void VipResizeItem::setCustomRightResizer(const QPainterPath& p)
{
	d_data->customResizer[Right - Top] = p.translated(-p.boundingRect().topLeft());
}
const QPainterPath& VipResizeItem::customRightResizer() const {
	return d_data->customResizer[Right - Top];
}
void VipResizeItem::setCustomBottomResizer(const QPainterPath& p)
{
	d_data->customResizer[Bottom - Top] = p.translated(-p.boundingRect().topLeft());
}
const QPainterPath& VipResizeItem::customBottomResizer() const {
	return d_data->customResizer[Bottom - Top];
}
void VipResizeItem::setCustomTopResizer(const QPainterPath& p)
{
	d_data->customResizer[Top - Top] = p.translated(-p.boundingRect().topLeft());
}
const QPainterPath& VipResizeItem::customTopResizer() const {
	return d_data->customResizer[Top - Top];
}

void VipResizeItem::setResizerPen(const QPen& p)
{
	for (int i = 1; i < Count; ++i)
	{
		d_data->resizers[i]->setPen(p);
	}
}
const QPen& VipResizeItem::resizerPen() const
{
	return d_data->resizers[1]->pen();
}

void VipResizeItem::setResizerBrush(const QBrush& b)
{
	for (int i = 1; i < Count; ++i)
	{
		d_data->resizers[i]->setBrush(b);
	}
}
const QBrush& VipResizeItem::resizerBrush() const
{
	return d_data->resizers[1]->brush();
}

QPainterPath VipResizeItem::shape() const
{
	QPainterPath p;
	if(d_data->managed.size())
	{
		p = d_data->managed.first()->shape();
		for(int i=1; i < d_data->managed.size(); ++i)
			p = p.united( d_data->managed[i]->shape() );
	}
	else
		p.addRect(d_data->geometry);

	if(isSelected())
	{
		//add the contour
		for(int i=0; i< Count; ++i)
			p.addPath(d_data->resizers[i]->path());
	}

	return p;
}

QPainterPath VipResizeItem::managedItemsPath() const
{
	QPainterPath p;
	if (d_data->managed.size())
	{
		p = d_data->managed.first()->shape();
		for (int i = 1; i < d_data->managed.size(); ++i)
			p = p.united(d_data->managed[i]->shape());
	}
	return p;
}

QRectF VipResizeItem::boundingRect() const
{
	QPainterPath p = managedItemsPath();
	if(p.isEmpty())
		p.addRect(d_data->geometry);

	return addSpacing(p.boundingRect());
}

void VipResizeItem::setGeometry(const QRectF & r)
{
	if(r != d_data->geometry)
	{
		d_data->geometry = r.normalized();
		computeResizers();
		prepareGeometryChange();
		// emit itemChanged() but do not mark the style sheet as dirty
		this->emitItemChanged(true, true, true, false);
		Q_EMIT geometryChanged(d_data->geometry);
	}
}

QRectF VipResizeItem::geometry() const
{
	return d_data->geometry;
}

QList<VipInterval> VipResizeItem::plotBoundingIntervals() const
{
	QRectF rect = geometry();
	return VipInterval::fromRect(rect);
}

void VipResizeItem::setSpacing(double spacing)
{
	if (spacing != d_data->spacing) {

		d_data->spacing = spacing;
		emitItemChanged();
	}
}

double VipResizeItem::spacing() const
{
	return d_data->spacing;
}

void  VipResizeItem::setBoxStyle(const VipBoxStyle & st)
{
	d_data->boxStyle = st;
	this->emitItemChanged();
}
const VipBoxStyle &  VipResizeItem::boxStyle() const
{
	return d_data->boxStyle;
}
VipBoxStyle &  VipResizeItem::boxStyle()
{
	return d_data->boxStyle;
}

void VipResizeItem::setAutoDelete(bool autodelete)
{
	if(autodelete != d_data->autodelete)
	{
		d_data->autodelete = autodelete;
	}
}

bool VipResizeItem::autoDelete() const
{
	return d_data->autodelete;
}

void VipResizeItem::setManagedItems(const PlotItemList & managed)
{
	if(d_data->managed.size())
	{
		//remove previous managed items
		for(int i=0; i < d_data->managed.size(); ++i)
		{
			VipPlotItem * item = d_data->managed[i];
			item->unsetCursor();
			item->removeSceneEventFilter(this);
			if(qobject_cast<VipResizeItem*>(item))
					static_cast<VipResizeItem*>(item)->d_data->parent = NULL;
			disconnect(item,SIGNAL(parentChanged()),this,SLOT(managedItemsChanged()));
			disconnect(item,SIGNAL(itemChanged(VipPlotItem*)),this,SLOT(managedItemsChanged()));
			disconnect(d_data->managed[i],SIGNAL(destroyed(QObject*)),this,SLOT(itemDestroyed(QObject*)));
		}
	}

	d_data->managed = managed;

	if(managed.size())
	{
		double this_z = std::numeric_limits<double>::max();;
		for(int i=0; i < d_data->managed.size(); ++i)
		{
			VipPlotItem * item = d_data->managed[i];
			if(qobject_cast<VipResizeItem*>(item))
					static_cast<VipResizeItem*>(item)->d_data->parent = this;
			connect(item,SIGNAL(parentChanged()),this,SLOT(managedItemsChanged()));
			connect(d_data->managed[i],SIGNAL(itemChanged(VipPlotItem*)),this,SLOT(managedItemsChanged()));
			connect(d_data->managed[i],SIGNAL(destroyed(QObject*)),this,SLOT(itemDestroyed(QObject*)),Qt::DirectConnection);
			if (VipPlotShape* shape = qobject_cast<VipPlotShape*>(d_data->managed[i])) {
				connect(shape, SIGNAL(aboutToChangePoints()), this, SLOT(emitAboutToChangePoints()), Qt::DirectConnection);
				connect(shape, SIGNAL(finishedChangePoints()), this, SLOT(emitFinishedChange()), Qt::DirectConnection);
			}
			item->setSelected(false);
			this_z = qMin(this_z,item->zValue());
			d_data->managed[i]->setCursor(Qt::SizeAllCursor);
		}
		this->setZValue(this_z-1);
		managedItemsChanged();
	}

	emitItemChanged();
}

void VipResizeItem::itemDestroyed(QObject * obj)
{
	VipPlotItem * item = static_cast<VipPlotItem*>(obj);
	if(d_data->managed.indexOf(item) >=0)
	{
		d_data->managed.removeAll(item);
		if(d_data->managed.size() == 0 && autoDelete())
			this->deleteLater();
		else
			emitItemChanged();
	}
}

void VipResizeItem::managedItemsChanged()
{
	if(d_data->managed.size())
	{
		QRectF this_rect;

		//update this geometry according to managed items
		//reparent this item if managed items changed there parent
		bool has_selection=false;
		bool all_hidden = true;
		double this_z = std::numeric_limits<double>::max();;

		for(int i=0; i < d_data->managed.size(); ++i)
		{
			if(d_data->managed[i]->isVisible())
				all_hidden = false;
			if(d_data->managed[i]->isSelected())
				has_selection = true;
			if(!d_data->managed[i]->parentItem() && d_data->managed[i]->scene() && this->scene() != d_data->managed[i]->scene())
				d_data->managed[i]->scene()->addItem(this);
			this_z = qMin(this_z,d_data->managed[i]->zValue());

			//on destroy, some items might emit a itemChanged() signal. Use qobject_cast to be sure that the item is still a valid VipPlotItem.
			if(VipPlotItem * item = qobject_cast<VipPlotItem*>(d_data->managed[i]))
			{
				bool install_filter = false;
				if( scene() != item->scene())
				{
					if(!qobject_cast<VipResizeItem*>(item))
						install_filter = true;
				}

				if(item->axes() != this->axes())
					this->setAxes(item->axes(),item->coordinateSystemType());

				if(install_filter)
					item->installSceneEventFilter(this);

				this_rect |= VipInterval::toRect(item->plotBoundingIntervals());
			}
		}

		if(!this_rect.isEmpty())
			this->setGeometry(this_rect);

		//
		if(has_selection)
		{
			if(VipResizeItem * top = this->topLevelParentResizeItem())
			{
				if(!top->isSelected())
					top->setSelected(true);
			}
			else if(!this->isSelected())
				this->setSelected(true);
		}
		//else
		//	this->setSelected(false);

		this->setVisible(!all_hidden);
		this->setZValue(this_z-1);
	}
}


const PlotItemList & VipResizeItem::managedItems() const
{
	return d_data->managed;
}

VipResizeItem * VipResizeItem::parentResizeItem() const
{
	return const_cast<VipResizeItem*>(this)->d_data->parent;
}

VipResizeItem * VipResizeItem::topLevelParentResizeItem() const
{
	VipResizeItem * parent = parentResizeItem();
	while(parent)
	{
		VipResizeItem * new_parent = parent->parentResizeItem();
		if(!new_parent)
			return parent;
		parent = new_parent;
	}
	return parent;
}

QList<VipResizeItem*> VipResizeItem::directChildren() const
{
	QList<VipResizeItem*> res;
	for(int i=0; i< d_data->managed.size(); ++i)
	{
		VipPlotItem* item = d_data->managed[i];
		if(qobject_cast<VipResizeItem*>(item))
			res <<static_cast<VipResizeItem*>(item);
	}
	return res;
}

QList<VipResizeItem*> VipResizeItem::children() const
{
	QList<VipResizeItem*> res = directChildren();
	QList<VipResizeItem*> child= res;
	for(int i=0; i < child.size(); ++i)
		res << child[i]->children();
	return res;
}
// PlotItemList VipResizeItem::leafs() const
// {
// PlotItemList res;
// if(d_data->managed.size() <=1)
// res= d_data->managed;
// else
// {
// for(int i=0; i < d_data->managed.size(); ++i)
// {
//	VipPlotItem * item = d_data->managed[i];
//	if(qobject_cast<VipResizeItem*>(item))
//		res << static_cast<VipResizeItem*>(item)->leafs();
//	else
//		res << item;
// }
// }
//
// return res;
// }

QRectF VipResizeItem::boundRect() const
{
	return VipInterval::toRect(VipAbstractScale::scaleIntervals(axes()));;
}

void VipResizeItem::setBoundaries(Boundaries b)
{
	d_data->boundaries =b;
	emitItemChanged();
}


void VipResizeItem::setBoundaryFlag(VipResizeItem::BoundaryFlag flag, bool on )
{
    if ( bool( d_data->boundaries & flag ) == on )
        return;

    if ( on )
    	d_data->boundaries |= flag;
    else
    	d_data->boundaries &= ~flag;

    emitItemChanged();
}



bool VipResizeItem::testBoundaryFlag(VipResizeItem::BoundaryFlag flag ) const
{
    return d_data->boundaries & flag;
}


VipResizeItem::Boundaries VipResizeItem::boundaries() const
{
	return d_data->boundaries;
}


void VipResizeItem::setLibertyDegrees(LibertyDegrees d)
{
	if (d_data->degrees != d) {
		d_data->degrees = d;
		d_data->rotate->setVisible(isVisible() && isSelected() && testLibertyDegreeFlag(Rotate));
		emitItemChanged();
	}
}

/// Modify an attribute of the liberty degree, used to enable/disable moving/resizing
///
/// \param flag liberty degree flag
/// \param on On/Off
///
/// \sa testLibertyDegreeFlag(), libertyDegrees()
void VipResizeItem::setLibertyDegreeFlag(VipResizeItem::LibertyDegreeFlag flag, bool on )
{
    if ( bool( d_data->degrees & flag ) == on )
        return;

    if ( on )
    	d_data->degrees |= flag;
    else
    	d_data->degrees &= ~flag;

	d_data->rotate->setVisible(isVisible() && isSelected() && testLibertyDegreeFlag(Rotate));
    emitItemChanged();
}


/// Test an attribute of the liberty degree, used to enable/disable moving/resizing
///
/// \param flag liberty degree flag
/// \return true, is enabled
///
/// The default setting is NoMoveOrResize
///
/// \sa setLibertyDegreeFlag(), libertyDegrees()
bool VipResizeItem::testLibertyDegreeFlag(VipResizeItem::LibertyDegreeFlag flag ) const
{
    return d_data->degrees & flag;
}

/// Return the liberty degree flags
///
/// \return liberty degree flags
///
/// The default setting is NoMoveOrResize
///
/// \sa setLibertyDegreeFlag(), testBoundaryFlag()
VipResizeItem::LibertyDegrees VipResizeItem::libertyDegrees() const
{
	return d_data->degrees;
}

void VipResizeItem::setExpandToFullArea(bool enable)
{
	if (d_data->expandToFullArea != enable)
	{
		d_data->expandToFullArea = enable;
		emitItemChanged();
	}
}
bool VipResizeItem::expandToFullArea() const
{
	return d_data->expandToFullArea;
}

bool VipResizeItem::moveEnabled() const
{
	return (d_data->degrees & HorizontalMove) || (d_data->degrees & VerticalMove);
}

bool VipResizeItem::resizeEnabled() const
{
	return (d_data->degrees & HorizontalResize) || (d_data->degrees & VerticalResize);
}

void VipResizeItem::setUnitMoveAndResize(bool unit)
{
	if (unit != d_data->unitMoveAndResize) {
		d_data->unitMoveAndResize = unit;
		emitItemChanged();
	}
}

bool VipResizeItem::unitMoveAndResize() const
{
	return d_data->unitMoveAndResize;
}

void VipResizeItem::setMinimumSize(const QSizeF & s)
{
	if (d_data->minimumSize != s) {
		d_data->minimumSize = s;
		emitItemChanged();
	}
}

QSizeF VipResizeItem::minimumSize() const
{
	return d_data->minimumSize;
}

QList<VipResizeItem*> VipResizeItem::linkedResizeItems() const
{
	//return all VipResizeItem objects linked to this item (sharing the same scene and axes)
	QList<VipPlotItem*> linked = this->linkedItems();
	QList<VipResizeItem*> res;

	for(int i=0; i < linked.size(); ++i)
	{
		QGraphicsObject * obj = linked[i]->toGraphicsObject();
		if(obj && qobject_cast<VipResizeItem*>(obj))
			res << static_cast<VipResizeItem*>(obj);
	}

	return res;
}

void 	VipResizeItem::computeResizers()
{
	if(d_data->degrees == NoMoveOrResize)
		return;

	QRectF bounding = boundingRect();
	QRectF resizer = d_data->resizerPath.boundingRect();
	double w = bounding.width();
	double h = bounding.height();

	d_data->resizers[TopLeft]->setPath(d_data->resizerPath.translated(bounding.topLeft()));
	d_data->resizers[TopRight]->setPath(d_data->resizerPath.translated(bounding.topRight() + QPointF(-resizer.width(),0)));
	d_data->resizers[BottomRight]->setPath(d_data->resizerPath.translated(bounding.bottomRight() + QPointF(-resizer.width(),-resizer.height())));
	d_data->resizers[BottomLeft]->setPath(d_data->resizerPath.translated(bounding.bottomLeft() + QPointF(0,-resizer.height())));

	if (!d_data->customResizer[Top - Top].isEmpty()) {
		QRectF resizer = d_data->customResizer[Top - Top].boundingRect();
		d_data->resizers[Top]->setPath(d_data->customResizer[Top - Top].translated(bounding.topLeft() + QPointF(w / 2 - resizer.width() / 2, 0)));
	}
	else
		d_data->resizers[Top]->setPath(d_data->resizerPath.translated(bounding.topLeft() + QPointF(w/2 - resizer.width()/2, 0 )));

	if (!d_data->customResizer[Bottom - Top].isEmpty()) {
		QRectF resizer = d_data->customResizer[Bottom - Top].boundingRect();
		d_data->resizers[Bottom]->setPath(d_data->customResizer[Bottom - Top].translated(bounding.topLeft() + QPointF(w / 2 - resizer.width() / 2, h - resizer.height())));
	}
	else
		d_data->resizers[Bottom]->setPath(d_data->resizerPath.translated(bounding.topLeft() + QPointF(w/2 - resizer.width()/2, h - resizer.height() )));

	if (!d_data->customResizer[Left - Top].isEmpty()) {
		QRectF resizer = d_data->customResizer[Left - Top].boundingRect();
		d_data->resizers[Left]->setPath(d_data->customResizer[Left - Top].translated(bounding.topLeft() + QPointF(0, h / 2 - resizer.height() / 2)));
	}
	else
		d_data->resizers[Left]->setPath(d_data->resizerPath.translated(bounding.topLeft() + QPointF(0,h/2 - resizer.height()/2)));

	if (!d_data->customResizer[Right - Top].isEmpty()) {
		QRectF resizer = d_data->customResizer[Right - Top].boundingRect();
		d_data->resizers[Right]->setPath(d_data->customResizer[Right - Top].translated(bounding.topLeft() + QPointF(w - resizer.width(), h / 2 - resizer.height() / 2)));
	}
	else
		d_data->resizers[Right]->setPath(d_data->resizerPath.translated(bounding.topLeft() + QPointF(w-resizer.width(),h/2 - resizer.height()/2 )));

	QPainterPath all;
	all.addRect(QRectF(bounding.left(),bounding.top(),bounding.width(),resizer.height())); //top
	all.addRect(QRectF(bounding.left(),bounding.top(),resizer.width(),bounding.height())); //left
	all.addRect(QRectF(bounding.left(),bounding.top() +bounding.height() - resizer.height() ,bounding.width(),resizer.height())); //bottom
	all.addRect(QRectF(bounding.right()-resizer.width(),bounding.top(),resizer.width(),bounding.height())); //right
	all.addRect(d_data->resizers[Top]->path().boundingRect());
	all.addRect(d_data->resizers[Bottom]->path().boundingRect());
	all.addRect(d_data->resizers[Left]->path().boundingRect());
	all.addRect(d_data->resizers[Right]->path().boundingRect());
	d_data->resizers[All]->setPath(all);
}

QRectF			VipResizeItem::addSpacing(const QRectF & r) const
{
	double spacing =  d_data->spacing ;
	return QRectF(r).adjusted(-spacing,-spacing,spacing,spacing);
}

QRectF			VipResizeItem::removeSpacing(const QRectF & r) const
{
	double spacing = d_data->spacing ;
	return QRectF(r).adjusted(spacing,spacing,-spacing,-spacing);
}

void 	VipResizeItem::changeGeometry(const QRectF & from, const QRectF & to, QRectF * new_bounding_rect, bool * ignore_x, bool * ignore_y)
{
	QRectF scale_rect = boundRect();
	if(scale_rect.isEmpty())
		scale_rect = to;

	QRectF old_rect = from;
	QRectF new_rect = to;

	bool is_moving = vipFuzzyCompare(old_rect.size() , new_rect.size());
	bool is_resizing = (!is_moving && !vipFuzzyCompare(old_rect, new_rect));

	if(is_moving)
	{
		if( ! (d_data->degrees & HorizontalMove) )
		{
			new_rect.moveCenter( QPointF(old_rect.center().x(), new_rect.center().y() ) );
		}

		if( ! (d_data->degrees & VerticalMove) )
		{
			new_rect.moveCenter( QPointF( new_rect.center().x(), old_rect.center().y() ) );
		}
	}
	else if( is_resizing )
	{
		if( ! (d_data->degrees & HorizontalResize) )
		{
			new_rect.setLeft(old_rect.left());
			new_rect.setRight(old_rect.right());
		}

		if( ! (d_data->degrees & VerticalResize) )
		{
			new_rect.setTop(old_rect.top());
			new_rect.setBottom(old_rect.bottom());
		}
	}

	if(d_data->boundaries != NoBoundary)
	{
		if(is_moving)
		{
			if( (d_data->boundaries & LeftBoundary) && new_rect.left() < scale_rect.left())
			{
				new_rect.moveLeft( scale_rect.left() );
				*ignore_x = true;
			}
			if( (d_data->boundaries & RightBoundary) && new_rect.right() > scale_rect.right() )
			{
				new_rect.moveRight( scale_rect.right() );
				*ignore_x = true;
			}
			if( (d_data->boundaries & TopBoundary) && new_rect.top() < scale_rect.top())
			{
				new_rect.moveTop( scale_rect.top() );
				*ignore_y = true;
			}
			if( (d_data->boundaries & BottomBoundary) && new_rect.bottom() > scale_rect.bottom())
			{
				new_rect.moveBottom( scale_rect.bottom() );
				*ignore_y = true;
			}
		}
		else if(is_resizing)
		{
			if( (d_data->boundaries & LeftBoundary) && new_rect.left() < scale_rect.left())
			{
				new_rect.setLeft(scale_rect.left());
				*ignore_x = true;
			}
			if( (d_data->boundaries & RightBoundary) && new_rect.right() > scale_rect.right() )
			{
				new_rect.setRight(scale_rect.right());
				*ignore_x = true;
			}
			if( (d_data->boundaries & TopBoundary) && new_rect.top() < scale_rect.top())
			{
				new_rect.setTop( scale_rect.top());
				*ignore_y = true;
			}
			if( (d_data->boundaries & BottomBoundary) && new_rect.bottom() > scale_rect.bottom())
			{
				new_rect.setBottom( scale_rect.bottom() );
				*ignore_y = true;
			}
		}
	}

	if (d_data->degrees & ExpandHorizontal || d_data->degrees & ExpandVertical)
	{
		if (d_data->expandToFullArea)
			if (VipAbstractPlotArea* a = area())
			{
				if (this->axes().size() == 2)
				{
					VipInterval x_bounds = a->areaBoundaries(this->axes()[0]);
					VipInterval y_bounds = a->areaBoundaries(this->axes()[1]);
					scale_rect = QRectF(x_bounds.minValue(), y_bounds.minValue(), x_bounds.width(), y_bounds.width());

					// use a more tolerant fuzzy compare as the scale_rect might be the same as new_rect but with slightly different values
					// due to floating point arithmetic precision
					if (scale_rect.left() != new_rect.left()) {
						if (fabs(scale_rect.left() - new_rect.left()) < new_rect.width() * 0.001)
							scale_rect.setLeft(new_rect.left());
					}
					if (scale_rect.right() != new_rect.right()) {
						if (fabs(scale_rect.right() - new_rect.right()) < new_rect.width() * 0.001)
							scale_rect.setRight(new_rect.right());
					}
					if (scale_rect.top() != new_rect.top()) {
						if (fabs(scale_rect.top() - new_rect.top()) < new_rect.height() * 0.001)
							scale_rect.setTop(new_rect.top());
					}
					if (scale_rect.bottom() != new_rect.bottom()) {
						if (fabs(scale_rect.bottom() - new_rect.bottom()) < new_rect.height() * 0.001)
							scale_rect.setBottom(new_rect.bottom());
					}
				}
			}

		if (d_data->degrees & ExpandHorizontal)
		{
			new_rect.setLeft(scale_rect.left());
			new_rect.setRight(scale_rect.right());
		}
		else
		{
			new_rect.setTop(scale_rect.top());
			new_rect.setBottom(scale_rect.bottom());
		}
	}

	//check for minimum size
	if(new_rect.width() < d_data->minimumSize.width())
	{
		if(new_rect.left() == old_rect.left())
		{
			//new_rect.setLeft(old_rect.left());
			new_rect.setRight(old_rect.left() + d_data->minimumSize.width());
		}
		else
		{
			new_rect.setRight(old_rect.right());
			new_rect.setLeft(old_rect.right() - d_data->minimumSize.width());
		}

		*ignore_x = true;
	}
	if(new_rect.height() < d_data->minimumSize.height())
	{
		if(new_rect.top() == old_rect.top())
		{
			new_rect.setBottom(old_rect.top() + d_data->minimumSize.height());
		}
		else
		{
			new_rect.setBottom(old_rect.bottom());
			new_rect.setTop(old_rect.bottom() - d_data->minimumSize.height());
		}
		*ignore_y = true;
	}

	if(d_data->unitMoveAndResize)
	{
		new_rect = new_rect.toRect();
	}

	*new_bounding_rect = new_rect;

}

int	VipResizeItem::itemUnderMouse(const QPointF & pt)
{
	QPointF pos = pt;//sceneMap().invTransform(pt);

	if( resizeEnabled() )
	{
		for(int i=1; i < Count; ++i)
		{
			if(d_data->resizers[i]->path().contains(pos))
			{
				return i;
			}
		}
	}

	if(d_data->resizers[All]->path().contains(pt) || shape().contains(pt))
		return 0;

	return -1;
}
void	VipResizeItem::mousePressEvent ( QGraphicsSceneMouseEvent * event )
{
	//forward event to parent if this VipResizeItem belongs to a group of non selected top level parent
	VipResizeItem *top =  topLevelParentResizeItem();
	if(top && !top->isSelected())
	{
		event->ignore();
		return;
	}

	d_data->selection = itemUnderMouse(event->pos());
	//TEST
	d_data->mousePress = sceneMap()->invTransform(event->pos());
	d_data->current = QTransform();
	d_data->pressedRect = sceneMap()->invTransformRect(removeSpacing(boundingRect()));

	//Handle multi selection with CTRL modifier
	bool ctrl_down = ( event->modifiers() & Qt::ControlModifier);
	//ignore CTRL modifier when clicking on a resizer (it is used for centered resize, see mouseMoveEvent)
	if (d_data->selection > 0)
		ctrl_down = false;
	bool was_selected = isSelected();
	bool selected = true;
	if(was_selected && ctrl_down )
		selected = false;

	if (d_data->selection >= 0)
		d_data->aboutTo = true;

	this->setSelected( (d_data->selection >= 0) && selected);

	if(!ctrl_down && !(was_selected && selected))
	{
		//unselect all other items except the top level parent VipResizeItem (if any) and the unique managed item (if any)
		VipPlotItem * managed = d_data->managed.size() ==1 ? d_data->managed.first() : NULL;

		QList<QGraphicsItem *> items;
		if(parentItem())
				items = parentItem()->childItems ();
		else if(scene())
			items = scene()->items();

		for(int i=0; i < items.size(); ++i)
		{
			if(items[i] != this && items[i] != top && items[i] != managed)
				items[i]->setSelected(false);
		}

	}

	this->update();
}

void	VipResizeItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event )
{
	Q_UNUSED(event)
	d_data->selection = -1;
	d_data->aboutTo = false;

	if (d_data->hasChanged) {
		d_data->hasChanged = false;
		Q_EMIT finishedChange();
	}
}


static bool scaleInverted(VipAbstractScale * sc)
{
	//inverted scale evolve in the oposite direction than the pixels
	QPoint p1 = sc->position(sc->scaleDiv().bounds().minValue()).toPoint();
	QPoint p2 = sc->position(sc->scaleDiv().bounds().maxValue()).toPoint();
	if (p1.x() == p2.x()) //y axis
	{
		bool inv = sc->scaleDiv().bounds().minValue() < sc->scaleDiv().bounds().maxValue();
		return (inv && !sc->isScaleInverted()) || (!inv && sc->isScaleInverted());
	}
	else //x axis
	{
		bool inv = sc->scaleDiv().bounds().minValue() > sc->scaleDiv().bounds().maxValue();
		return (inv && !sc->isScaleInverted()) || (!inv && sc->isScaleInverted());
	}
}


static double signOf(double value, double sign)
{
	if (sign >= 0) {
		if (value < 0) return -value;
		return value;
	}
	else {
		if (value >= 0) return -value;
		return value;
	}
}


void VipResizeItem::recomputeFullGeometry()
{
	d_data->current = QTransform();
	QTransform inv = d_data->current.isIdentity() ? d_data->current :  d_data->current.inverted();
	QRectF rect = sceneMap()->invTransformRect(removeSpacing(boundingRect()));
	QRectF old_rect = rect;

	if (rect.width() < 0 || rect.height() < 0)
		return;

	bool ignore_x = false, ignore_y = false;
	this->changeGeometry(old_rect, rect, &rect, &ignore_x, &ignore_y);

	if (!vipFuzzyCompare(old_rect, rect))
	{
		QTransform tr = this->computeTransformation(old_rect, rect);

		//apply the transform to all managed items
		for (int i = 0; i < d_data->managed.size(); ++i)
		{
			if(!inv.isIdentity())
				d_data->managed[i]->applyTransform(inv);
			d_data->managed[i]->applyTransform(tr);
			d_data->managed[i]->update();
		}
		//reset the geometry
		this->setGeometry(rect);
		
		//change geometry of all other selected items, except top level parent and children
		QList<VipResizeItem*> children = this->children();
		QList<VipResizeItem*> shapes = linkedResizeItems();
		VipResizeItem* top = this->topLevelParentResizeItem();
		for (int i = 0; i < shapes.size(); ++i)
		{
			VipResizeItem* item = shapes[i];
			if (item->isSelected() && item != this && item != top && children.indexOf(item) < 0)
			{
				shapes[i]->applyTransform(inv);
				shapes[i]->applyTransform(tr);
			}
		}

		d_data->current = tr;
	}
}

void	VipResizeItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event )
{
	if(d_data->selection == -1 || d_data->degrees == NoMoveOrResize || !this->isSelected())
		return;

	QTransform inv = d_data->current.inverted();
	QRectF rect = d_data->pressedRect;//inv.mapRect(boundingRect());
	QRectF old_rect = rect;
	QPointF pos = sceneMap()->invTransform(event->pos());
	QPointF move = pos - d_data->mousePress;

	bool xInv = scaleInverted(axes()[0]);
	bool yInv = scaleInverted(axes()[1]);



	if(d_data->selection == 0 && moveEnabled())
	{
		if (d_data->aboutTo) {
			d_data->aboutTo = false;
			Q_EMIT aboutToMove();
		}
		rect.translate( move );
		d_data->hasChanged = true;
	}
	else
	{
		if (d_data->aboutTo) {
			d_data->aboutTo = false;
			Q_EMIT aboutToResize();
		}
		d_data->hasChanged = true;

		bool ctrl = event->modifiers() & Qt::CTRL;
		bool shift = event->modifiers() & Qt::SHIFT;
		QPointF move_shift = move;

		if (shift) {
			//Pressing shift enables proportional resize
			double ratio = d_data->pressedRect.width() / d_data->pressedRect.height();
			double ratio2 = qAbs(move_shift.x() / move_shift.y());
			if (ratio2 > ratio)
				move_shift.setX(signOf(move_shift.y() * ratio,move.x()));
			else
				move_shift.setY(signOf(move_shift.x() / ratio, move.y()));
		}
		if (ctrl) {
			//For centerd resize, we need to multiply by 2 the transform as we shift the rect back to its center
			move *= 2;
			move_shift *= 2;
		}

		if (d_data->selection == TopLeft)
		{
			//For TopLeft, SHIFT move must have the same sign
			if (xInv == yInv && vipSign(move_shift.x()) != vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			else if (xInv != yInv && vipSign(move_shift.x()) == vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			if (xInv && yInv)
				rect.setBottomRight(rect.bottomRight() + move_shift);
			else if (xInv)
				rect.setTopRight(rect.topRight() + move_shift);
			else if (yInv)
				rect.setBottomLeft(rect.bottomLeft() + move_shift);
			else
				rect.setTopLeft(rect.topLeft() + move_shift);

		}
		else if (d_data->selection == TopRight)
		{
			//For TopRight, SHIFT move must have different sign
			if (xInv == yInv && vipSign(move_shift.x()) == vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			else if (xInv != yInv && vipSign(move_shift.x()) != vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			if (xInv && yInv)
				rect.setBottomLeft(rect.bottomLeft() + move_shift);
			else if (xInv)
				rect.setTopLeft(rect.topLeft() + move_shift);
			else if (yInv)
				rect.setBottomRight(rect.bottomRight() + move_shift);
			else
				rect.setTopRight(rect.topRight() + move_shift);
		}
		else if (d_data->selection == BottomRight)
		{
			//For BottomRight, SHIFT move must have the same sign
			if (xInv == yInv && vipSign(move_shift.x()) != vipSign(move_shift.y()))
				move_shift = QPointF(0,0);
			else if (xInv != yInv && vipSign(move_shift.x()) == vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			if (xInv && yInv)
				rect.setTopLeft(rect.topLeft() + move_shift);
			else if (xInv)
				rect.setBottomLeft(rect.bottomLeft() + move_shift);
			else if (yInv)
				rect.setTopRight(rect.topRight() + move_shift);
			else
				rect.setBottomRight(rect.bottomRight() + move_shift);
		}
		else if (d_data->selection == BottomLeft)
		{
			//For BottomLeft, SHIFT move must have different sign
			if (xInv == yInv && vipSign(move_shift.x()) == vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			else if (xInv != yInv && vipSign(move_shift.x()) != vipSign(move_shift.y()))
				move_shift = QPointF(0, 0);
			if (xInv && yInv)
				rect.setTopRight(rect.topRight() + move_shift);
			else if (xInv)
				rect.setBottomRight(rect.bottomRight() + move_shift);
			else if (yInv)
				rect.setTopLeft(rect.topLeft() + move_shift);
			else
				rect.setBottomLeft(rect.bottomLeft() + move_shift);
		}
		else if (d_data->selection == Left)
		{
			if (xInv)
				rect.setRight(rect.right() + move.x());
			else
				rect.setLeft(rect.left() + move.x());
		}
		else if (d_data->selection == Right)
		{
			if (xInv)
				rect.setLeft(rect.left() + move.x());
			else
				rect.setRight(rect.right() + move.x());
		}
		else if (d_data->selection == Top)
		{
			if (yInv)
				rect.setBottom(rect.bottom() + move.y());
			else
				rect.setTop(rect.top() + move.y());
		}
		else if (d_data->selection == Bottom)
		{
			if (yInv)
				rect.setTop(rect.top() + move.y());
			else
				rect.setBottom(rect.bottom() + move.y());
		}

		if (ctrl) {
			//Pressing CTRL force resize to keep the previous center
			QPointF center = d_data->pressedRect.center();
			rect.moveCenter(center);
		}
	}

	if(rect.width() < 0 || rect.height() < 0)
		return;

	bool ignore_x = false, ignore_y = false;

	this->changeGeometry(old_rect,rect,&rect,&ignore_x,&ignore_y);

	if (!vipFuzzyCompare(old_rect, rect))
	{

		QTransform tr = this->computeTransformation(old_rect,rect);

		//apply the transform to all managed items
		for(int i=0; i < d_data->managed.size(); ++i)
		{
			d_data->managed[i]->applyTransform(inv);
			d_data->managed[i]->applyTransform(tr);
			d_data->managed[i]->update();
		}
		//reset the geometry
		this->setGeometry(rect);
		
		//change geometry of all other selected items, except top level parent and children
		QList<VipResizeItem*> children = this->children();
		QList<VipResizeItem*> shapes = linkedResizeItems() ;
		VipResizeItem * top = this->topLevelParentResizeItem();
		for(int i=0; i < shapes.size(); ++i)
		{
			VipResizeItem * item = shapes[i];
			if(item->isSelected() && item != this && item != top && children.indexOf(item)<0)
			{
				shapes[i]->applyTransform(inv);
				shapes[i]->applyTransform(tr);
			}
		}

		d_data->current = tr;
	}
	else
	{
	}

	event->accept();
}



void	VipResizeItem::keyPressEvent(QKeyEvent* event)
{
	if (!sceneEventFilter(this, event))
		event->ignore();
}

bool	VipResizeItem::sceneEventFilter(QGraphicsItem * watched, QEvent * evt)
{
	Q_UNUSED(watched)
	if(evt->type() == QEvent::KeyPress)
	{
		if (!isSelected()) {
			//if not selected, forward to the first selected parent VipResizeItem
			VipResizeItem* p = parentResizeItem();
			while (p) {
				if (p->isSelected())
					return p->sceneEventFilter(p, evt);
				p = p->parentResizeItem();
			}
			return false;
		}

		QKeyEvent * event = static_cast<QKeyEvent*>(evt);
		bool handled = false;

		QList<VipAbstractScale*> scales = this->axes();
		if(scales.size() == 2)
		{
			handled = true;
			//move by one unit
			bool x_inverted = scales[0] ? (scales[0]->scaleDiv().range() < 0) : false;
			bool y_inverted = scales[1] ? (scales[1]->scaleDiv().range() > 0) : false;
			QTransform tr;

			if(event->key() == Qt::Key_Up)
				tr.translate(0,!y_inverted ? -1 : 1);
			else if(event->key() == Qt::Key_Down)
				tr.translate(0,!y_inverted ? 1 : -1);
			else if(event->key() == Qt::Key_Left)
				tr.translate(!x_inverted ? -1 : 1, 0);
			else if(event->key() == Qt::Key_Right)
				tr.translate(!x_inverted ? 1 : -1, 0);
			else
				handled = false;

			bool is_identity = tr.isIdentity();
			if(!is_identity)
				this->applyTransform(tr);

			//change geometry of all other selected items, except top level parent and children
			QList<VipResizeItem*> children = this->children();
			QList<VipResizeItem*> shapes = linkedResizeItems() ;
			VipResizeItem * top = this->topLevelParentResizeItem();
			for(int i=0; i < shapes.size(); ++i)
			{
				VipResizeItem * item = shapes[i];
				if(item->isSelected() && item != this && item != top && children.indexOf(item)<0 && !hasSelectedResizeItem(item))
				{
					if (!is_identity)
						shapes[i]->applyTransform(tr);
				}
			}

			if (!is_identity) {
				Q_EMIT finishedChange();
			}
		}

		return (handled);
	}
	return false;
}


bool VipResizeItem::applyTransform(const QTransform & tr)
{

	//the transform is applied to the VipResizeItem itself, which shape might include the resizers (depending on item's selection).
	//so we need to translate this transform into managed item's transform, only in case of selection.
	//we also need to take care of the minimum size.

	QTransform transform = tr;
	QRectF bounding = boundingRect();

	//apply the transform to the bounding rect in scale unit
	QRectF old_rect = sceneMap()->invTransformRect(  bounding );
	QRectF rect = tr.map(old_rect).boundingRect();

	//now go back to device unit and remove spacing
	old_rect = (bounding);
	rect = (sceneMap()->transformRect(rect));
	if(isSelected())
	{
		old_rect = removeSpacing(old_rect);
		rect = removeSpacing(rect);
	}


	//go back to scale unit and compute the new transform
	old_rect =  sceneMap()->invTransformRect( old_rect);
	rect =  sceneMap()->invTransformRect( rect);

	bool unused;
	this->changeGeometry(old_rect,rect,&rect,&unused,&unused);
	if (!vipFuzzyCompare(old_rect, rect))
		transform = this->computeTransformation(old_rect,rect);

	//apply the transform to all managed items
	for(int i=0; i < d_data->managed.size(); ++i)
			d_data->managed[i]->applyTransform(transform);


	//reset the geometry and stuff
	this->setGeometry(tr.map(this->geometry()).boundingRect());
	
	this->emitNewTransform(tr);
	return true;
}


void VipResizeItem::emitNewTransform(const QTransform & tr)
{
	Q_EMIT newTransform(tr);
}


QTransform VipResizeItem::computeTransformation(const QRectF & old_rect, const QRectF & new_rect) const
{
	QTransform tr;

	if(old_rect.isEmpty() || new_rect.isEmpty())
		return tr;

	double dx = new_rect.width()/old_rect.width();
	double dy = new_rect.height()/old_rect.height();
	QPointF translate = new_rect.topLeft() - QPointF(old_rect.left()*dx,old_rect.top()*dy);
	tr.translate(translate.x(),translate.y());
	tr.scale(dx,dy);

	return tr;
}



void VipResizeItem::draw(QPainter * painter, const VipCoordinateSystemPtr & m) const
{
	Q_UNUSED(m);
	Q_UNUSED(painter);

	if (testLibertyDegreeFlag(ExpandHorizontal) || testLibertyDegreeFlag(ExpandVertical))
		const_cast<VipResizeItem*>(this)->recomputeFullGeometry();
	//recompute resizer positions and sizes
	//const_cast<VipResizeItem*>(this)->computeResizers();

	//TEST
	//painter->setBrush(QBrush(Qt::yellow));
	//painter->drawRect(m->transformRect(geometry()));
}

void VipResizeItem::drawSelected(QPainter * painter, const VipCoordinateSystemPtr &) const
{
	if (d_data->selection == -1 && (testLibertyDegreeFlag(ExpandHorizontal) || testLibertyDegreeFlag(ExpandVertical)))
		const_cast<VipResizeItem*>(this)->recomputeFullGeometry();

	//recompute resizer positions and sizes
	const_cast<VipResizeItem*>(this)->computeResizers();
	//painter->setBrush(QBrush());
	//painter->setPen(QPen());
	QRectF r = boundingRect();
	QRectF resizer = d_data->resizerPath.boundingRect();
	r.adjust(resizer.width()/2,resizer.height()/2,-resizer.width()/2,-resizer.height()/2);

	VipBoxStyle st = d_data->boxStyle;
	st.computeRect(r);
	st.draw(painter);

	//VipPainter::drawRect(painter,r);
}

void VipResizeItem::setAxes(const QList<VipAbstractScale*> & axes, VipCoordinateSystem::Type type)
{
	//make sure that the VipResizeItem object and managed items always share the same axes
	//block the itemChanged(VipPlotItem*) signal to avoid infinit loop
	this->blockSignals(true);
	for(int i=0; i < d_data->managed.size(); ++i)
		if(d_data->managed[i]->axes() != axes)
			d_data->managed[i]->setAxes(axes,type);
	this->blockSignals(false);
	VipPlotItem::setAxes(axes,type);
}

//if this item is selected:
//		select managed item if it is unique
//		raise managed items above resizer
//
//if it is unselected:
//		unselect managed items
//		raise resizer above managed items

QVariant VipResizeItem::itemChange ( GraphicsItemChange change, const QVariant & value )
{
	if(change == QGraphicsItem::ItemSelectedChange)
	{
		bool selected = value.value<bool>();

		//if(selected && d_data->managed.size() == 1)
		// {
		// d_data->managed.first()->setSelected(true);
		// }
		if (d_data->managed.size() == 1) {
			if(d_data->managed.first()->isSelected() != selected)
				d_data->managed.first()->setSelected(selected);
		}

		d_data->resizers[All]->setVisible(selected && d_data->degrees);
		d_data->resizers[Left]->setVisible(selected && (d_data->degrees & HorizontalResize));
		d_data->resizers[Right]->setVisible(selected && (d_data->degrees & HorizontalResize));
		d_data->resizers[Top]->setVisible(selected && (d_data->degrees & VerticalResize));
		d_data->resizers[Bottom]->setVisible(selected && (d_data->degrees & VerticalResize));
		d_data->resizers[TopLeft]->setVisible(selected && (d_data->degrees & HorizontalResize)&& (d_data->degrees & VerticalResize));
		d_data->resizers[TopRight]->setVisible(selected && (d_data->degrees & HorizontalResize)&& (d_data->degrees & VerticalResize));
		d_data->resizers[BottomLeft]->setVisible(selected && (d_data->degrees & HorizontalResize)&& (d_data->degrees & VerticalResize));
		d_data->resizers[BottomRight]->setVisible(selected && (d_data->degrees & HorizontalResize)&& (d_data->degrees & VerticalResize));

		d_data->rotate->setVisible(isVisible() && selected && testLibertyDegreeFlag( Rotate));
	}

	return VipPlotItem::itemChange(change,value);
}
