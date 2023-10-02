#include "VipDrawShape.h"
#include "VipStandardWidgets.h"
#include "VipPlayer.h"
#include "VipXmlArchive.h"
#include "VipPlotSpectrogram.h"
#include "VipExtractComponents.h"
#include "VipResizeItem.h"
#include "VipIODevice.h"
#include "VipPlotShape.h"
#include "VipProcessingObjectEditor.h"
#include "VipDisplayArea.h"
#include "VipLogging.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QGridLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <QLabel>
#include <QBoxLayout>
#include <QTextStream>
#include <qmessagebox.h>
#include <QApplication>

VipDrawGraphicsShape::VipDrawGraphicsShape(VipPlotSceneModel * plotSceneModel, const QString & group )
:VipPlotAreaFilter(),m_group(group),m_scene_model(plotSceneModel)
{
	setAcceptHoverEvents(true);
	this->setFlag(QGraphicsItem::ItemIsFocusable,true);
	this->setFocus();
}
VipDrawGraphicsShape::VipDrawGraphicsShape(VipPlotPlayer * player, const QString & group)
	:VipPlotAreaFilter(), m_group(group), m_player(player)
{
	setAcceptHoverEvents(true);
	this->setFlag(QGraphicsItem::ItemIsFocusable, true);
	this->setFocus();
}

void VipDrawGraphicsShape::reset(VipPlotSceneModel * plotSceneModel, const QString & group )
{
	m_scene_model = plotSceneModel;
	m_player = NULL;
	m_group = group;
}
void VipDrawGraphicsShape::reset(VipPlotPlayer * player, const QString & group)
{
	m_scene_model = NULL;
	m_player = player;
	m_group = group;
}

void VipDrawGraphicsShape::resetPlayer(VipPlayer2D * player, const QString & group )
{
	if (VipPlotPlayer *pl = qobject_cast<VipPlotPlayer*>(player))
		reset(pl, group);
	else if (player)
		reset(player->plotSceneModel(), group);
	else
		reset((VipPlotPlayer*)NULL, group);
}

VipShape VipDrawGraphicsShape::lastShape() const
{
	return m_shape;
}

VipPlotSceneModel* VipDrawGraphicsShape::plotSceneModel() const
{
	return m_scene_model;
}

QList<VipAbstractScale*> VipDrawGraphicsShape::sceneModelScales() const
{
	if (m_scene_model)
		return m_scene_model->axes();
	return QList<VipAbstractScale*>();
}

QString VipDrawGraphicsShape::group() const
{
	return m_group;
}

#include "VipPlotGrid.h"
void VipDrawGraphicsShape::findPlotSceneModel(const QPointF & scene_pos)
{
	if (!area())
		return;
	if (!m_player)
		return;

	//first, get all scene models and their bounding rect in scene coordinate
	QList<VipPlotSceneModel*> models;
	QList<QRectF> rects;

	QList<VipAbstractScale*> scales = m_player->leftScales();
	QRectF bottom = m_player->xScale()->mapToScene(m_player->xScale()->boundingRect()).boundingRect();
	for (int i = 0; i < scales.size(); ++i) {
		VipPlotSceneModel * sm = m_player->findPlotSceneModel(QList<VipAbstractScale*>() << m_player->xScale()<< scales[i]);
		if (sm) {
			models << sm;
			QRectF left = scales[i]->mapToScene(scales[i]->boundingRect()).boundingRect();
			QRectF r = QRectF(left.topLeft(), QPointF(bottom.right(), left.bottom()));
			rects << r;
		}
	}
	//make sure the default plot scene model is the first one
	int index = models.indexOf(m_player->plotSceneModel());
	if (index >= 0) {
		QRectF r = rects[index];
		models.removeAt(index);
		rects.removeAt(index);
		models.push_front(m_player->plotSceneModel());
		rects.push_front(r);
	}

	//find the first plot scene model that contains given point
	for (int i = 0; i < models.size(); ++i) {
		if (rects[i].contains(scene_pos)) {
			m_scene_model = models[i];
			return;
		}
	}
}


void VipDrawGraphicsShape::setLastShape(const VipShape & shape)
{
	m_shape = shape;
	if (m_scene_model) {
		//Save the current state before adding new shape (for undo/redo)
		VipSceneModelState::instance()->pushState(
			qobject_cast<VipPlayer2D*>(VipAbstractPlayer::findAbstractPlayer(m_scene_model)),
			m_scene_model
		);
		m_scene_model->sceneModel().add(m_group, m_shape);
	}
	
	emitFinished();
}

void VipDrawGraphicsShape::setGroup(const QString & l)
{
	m_group = l;
}



VipDrawShapeRect::VipDrawShapeRect(VipPlotSceneModel * scene, const QString & group )
:VipDrawGraphicsShape(scene,group),m_begin(-1,-1),m_end(-1,-1)
{}

VipDrawShapeRect::VipDrawShapeRect(VipPlotPlayer * player, const QString & group )
	: VipDrawGraphicsShape(player,group), m_begin(-1, -1), m_end(-1, -1)
{}

VipDrawShapeRect::~VipDrawShapeRect()
{}

QPainterPath VipDrawShapeRect::shape() const
{
	QPainterPath path;
	path.addRect(boundingRect());
	return path;
}

QRectF  VipDrawShapeRect::boundingRect() const
{
	if (!area())
		return QRectF();
	QRectF tmp = QRectF(area()->scaleToPosition(m_begin, sceneModelScales()), area()->scaleToPosition(m_end, sceneModelScales())).normalized();
	tmp = this->mapFromItem(area(), tmp).boundingRect();
	return tmp;
}

void VipDrawShapeRect::paint(QPainter * painter, const QStyleOptionGraphicsItem * , QWidget *  )
{
	QRectF shape = boundingRect();
	if(!shape.isNull() )
	{
		//painter->setCompositionMode(QPainter::CompositionMode_Difference);
		painter->setPen(QPen(Qt::white,0));
		painter->setBrush(QColor(255, 0, 0,50));
		painter->drawRect(shape);
	}
}

bool VipDrawShapeRect::sceneEvent (  QEvent * event )
{
	if(!area())
		return false;

	if(event->type() == QEvent::GraphicsSceneMousePress)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		findPlotSceneModel(evt->scenePos());
		QPointF pos = evt->pos();
		m_begin = (area()->positionToScale(pos, sceneModelScales()));
		m_end = m_begin;
		return true;
	}
	else if(event->type() == QEvent::GraphicsSceneMouseMove)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		m_end = (area()->positionToScale(evt->pos(), sceneModelScales()));
		this->prepareGeometryChange();
		return true;
	}
	else if(event->type() == QEvent::GraphicsSceneMouseRelease)
	{
		if(m_begin != m_end)
		{
			this->setLastShape(createShape());
		}
		m_begin = m_end = QPoint(-1,-1);
		return true;
	}

	return false;
}

VipShape VipDrawShapeRect::createShape()
{
	return VipShape(QRectF(m_begin,m_end));
}


VipDrawShapeEllipse::VipDrawShapeEllipse(VipPlotSceneModel * scene, const QString & group )
:VipDrawShapeRect(scene,group)
{}
VipDrawShapeEllipse::VipDrawShapeEllipse(VipPlotPlayer * player, const QString & group)
	: VipDrawShapeRect(player, group)
{}


QPainterPath VipDrawShapeEllipse::shape() const
{
	QPainterPath path;
	path.addEllipse(VipDrawShapeRect::boundingRect());
	return path;
}

void VipDrawShapeEllipse::paint(QPainter * painter, const QStyleOptionGraphicsItem * , QWidget *  )
{
	QRectF shape = boundingRect();

	if(!shape.isNull() )
	{
		painter->setRenderHints(QPainter::HighQualityAntialiasing);
		painter->setPen(QPen(Qt::white, 0));
		painter->setBrush(QColor(255, 0, 0, 50));
		painter->drawEllipse(shape);
	}
}

VipShape VipDrawShapeEllipse::createShape()
{
	/*QRectF r = */boundingRect();
	QPainterPath path;
	path.addEllipse(QRectF(m_begin, m_end));
	return VipShape(path);
}



VipDrawShapePoint::VipDrawShapePoint(VipPlotSceneModel * scene, const QString & group )
:VipDrawGraphicsShape(scene,group)
{}
VipDrawShapePoint::VipDrawShapePoint(VipPlotPlayer * player, const QString & group)
	: VipDrawGraphicsShape(player, group)
{}

void VipDrawShapePoint::paint(QPainter * , const QStyleOptionGraphicsItem * , QWidget *  )
{
}

QPainterPath VipDrawShapePoint::shape() const
{
	return QPainterPath();
}

bool VipDrawShapePoint::sceneEvent ( QEvent * event )
{
	if(!area())
			return false;

	if(event->type() == QEvent::GraphicsSceneMousePress)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		findPlotSceneModel(evt->scenePos());
		setLastShape(VipShape(area()->positionToScale(evt->pos(),sceneModelScales())));
		return true;
	}

	return false;
}



VipDrawShapePolygon::VipDrawShapePolygon(VipPlotSceneModel * scene, const QString & group)
	:VipDrawGraphicsShape(scene, group)
{
	qApp->installEventFilter(this);
}
VipDrawShapePolygon::VipDrawShapePolygon(VipPlotPlayer * player, const QString & group)
	: VipDrawGraphicsShape(player, group)
{
	qApp->installEventFilter(this);
}
VipDrawShapePolygon::~VipDrawShapePolygon()
{
	qApp->removeEventFilter(this);
}

VipShape VipDrawShapePolygon::createShape(const QPolygonF & poly)
{
	if(poly.size() > 2)
		return VipShape(poly,VipShape::Polygon);
	return VipShape();
}

QPainterPath VipDrawShapePolygon::shape() const
{
	if (!area())
		return QPainterPath();
	QPainterPath path;
	QPolygonF p = m_polygon;
	p.append(m_pos);
	QRectF r = p.boundingRect();
	r.setTopLeft(area()->scaleToPosition(r.topLeft(), sceneModelScales()));
	r.setBottomRight(area()->scaleToPosition(r.bottomRight(), sceneModelScales()));
	r = r.adjusted(-5, -5, 80, 30);
	path.addRect(r);
	return path;
}

void VipDrawShapePolygon::paint(QPainter * painter, const QStyleOptionGraphicsItem * , QWidget *  )
{
	painter->setPen(QPen(QColor(255, 0, 0, 100), 0));
	painter->setBrush(QColor(255, 0, 0, 50));
	painter->setRenderHints(QPainter::HighQualityAntialiasing|QPainter::Antialiasing);

	QPolygonF poly(m_polygon);
	poly.append(m_pos);
	poly = area()->scaleToPosition(poly, sceneModelScales());

	poly = this->mapFromItem(area(), poly);
	//TEST: remove Qt::WindingFill
	painter->drawPolygon(poly/*,Qt::WindingFill*/);

	//close polygon
	if (m_polygon.size() > 2)
	{
		if ((poly.first() - poly.last()).manhattanLength() < 4)
		{
			//draw a circle and a text to close the polygon
			QRectF rect(0, 0, 9, 9);
			rect.moveCenter(poly.first());

			painter->setPen(QPen(Qt::white, 0));
			painter->setBrush(QColor(255, 255, 255, 100));

			painter->drawEllipse(rect);
			painter->setPen(QPen());

			VipText text(QString("Close polygon"));
			text.draw(painter, poly.first() - QPointF(0, text.textRect().height()));
			//painter->drawText(poly.first(), QString("Close polygon"));
		}
	}
}

bool VipDrawShapePolygon::stopOnKeyPress()
{
	VipShape sh = createShape(m_polygon);
	if (!sh.isNull()) {
		this->setLastShape(sh);
		m_polygon.clear();
		m_pos = QPointF();
		return true;
	}

	m_polygon.clear();
	m_pos = QPointF();
	return false;
}

bool VipDrawShapePolygon::sceneEvent ( QEvent * event )
{
	if(!area())
		return false;

	if(event->type() == QEvent::GraphicsSceneMousePress)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		findPlotSceneModel(evt->scenePos());
		if (m_polygon.size() > 2 && (evt->pos() - area()->scaleToPosition(m_polygon.first(),sceneModelScales())).manhattanLength() < 4)
		{
			VipShape sh = createShape(m_polygon);
			if (!sh.isNull())
				this->setLastShape(sh);
			m_polygon.clear();
			m_pos = QPointF();
		}
		else
		{
			if (m_pos == QPointF() || m_area != area())
			{
				m_pos = area()->positionToScale(evt->pos(), sceneModelScales());
				m_area = area();
			}
			m_polygon.append(m_pos);
		}
		return true;
	}
	else if(event->type() == QEvent::GraphicsSceneHoverMove)
	{
		QGraphicsSceneHoverEvent * evt = static_cast<QGraphicsSceneHoverEvent*>(event);
		findPlotSceneModel(evt->scenePos());
		m_pos = area()->positionToScale(evt->pos(),sceneModelScales());
		m_area = area();
		//SHIFT modifier: draw a horizontal or vertical line
		if (m_polygon.size() && (QApplication::keyboardModifiers() & Qt::ShiftModifier))
		{
			if (qAbs(m_pos.x() - m_polygon.last().x()) > qAbs(m_pos.y() - m_polygon.last().y()))
				m_pos.setY(m_polygon.last().y()); //horizontal
			else
				m_pos.setX(m_polygon.last().x());//vertical
		}

		this->prepareGeometryChange();
		return true;
	} 
	else if(event->type() == QEvent::KeyPress)
	{
		QKeyEvent * evt = static_cast<QKeyEvent*>(event);
		if(evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return || evt->key() == Qt::Key_Escape)
		{
			return stopOnKeyPress();
		}
	}

	return false;
}

bool VipDrawShapePolygon::eventFilter(QObject *, QEvent * event)
{
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent * evt = static_cast<QKeyEvent*>(event);
		if (evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return || evt->key() == Qt::Key_Escape)
			return stopOnKeyPress();
	}
	return false;
}



VipDrawShapePolyline::VipDrawShapePolyline(VipPlotSceneModel * scene, const QString & group )
:VipDrawShapePolygon(scene,group)
{}
VipDrawShapePolyline::VipDrawShapePolyline(VipPlotPlayer * player, const QString & group)
	: VipDrawShapePolygon(player, group)
{}
void VipDrawShapePolyline::paint(QPainter * painter, const QStyleOptionGraphicsItem * , QWidget *  )
{
	painter->setPen(QPen(QColor(255, 0, 0, 50), 0));
	painter->setBrush(Qt::NoBrush);
	painter->setRenderHints(QPainter::HighQualityAntialiasing | QPainter::Antialiasing);

	QPolygonF poly(m_polygon);
	poly.append(m_pos);
	poly = area()->scaleToPosition(poly, sceneModelScales());
	painter->drawPolyline(poly);

	//stop polyline
	if (m_polygon.size() > 1 && poly.size() > 1)
	{
		if ((poly.last() - poly[poly.size()-2]).manhattanLength() < 4)
		{
			//draw a circle and a text to close the polygon
			QRectF rect(0, 0, 9, 9);
			rect.moveCenter(poly[poly.size() - 2]);

			painter->setPen(QPen(Qt::white, 0));
			painter->setBrush(QColor(255, 0, 0, 50));

			painter->drawEllipse(rect);
			//painter->setPen(QPen());
			VipText text(QString("Stop polyline"));
			text.draw(painter, poly[poly.size() - 2] - QPointF(0,text.textRect().height()));
			//painter->drawText(poly[poly.size() - 2], QString("Stop polyline"));
		}
	}
}

VipShape VipDrawShapePolyline::createShape(const QPolygonF & poly)
{
	if(poly.size() > 1)
		return VipShape(QPolygonF(poly),VipShape::Polyline);
	return VipShape();
}

bool VipDrawShapePolyline::sceneEvent(QEvent * event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::GraphicsSceneMousePress)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		findPlotSceneModel(evt->scenePos());
		if (m_polygon.size() > 1 && (evt->pos() - area()->scaleToPosition(m_polygon.last(),sceneModelScales())).manhattanLength() < 4)
		{
			VipShape sh = createShape(m_polygon);
			if (!sh.isNull())
				this->setLastShape(sh);
			m_polygon.clear();
			m_pos = QPointF();
		}
		else
		{
			if (m_pos == QPointF() || m_area != area())
			{
				m_pos = area()->positionToScale(evt->pos(),sceneModelScales());
				m_area = area();
			}
			m_polygon.append(m_pos);
		}
		return true;
	}
	else
		return VipDrawShapePolygon::sceneEvent(event);
}










VipDrawShapeMask::VipDrawShapeMask(VipPlotSceneModel * scene, const QString & group )
:VipDrawGraphicsShape(scene,group)
{}
VipDrawShapeMask::VipDrawShapeMask(VipPlotPlayer * player, const QString & group)
	: VipDrawGraphicsShape(player, group)
{}
VipShape VipDrawShapeMask::createShape(const QPolygonF & poly)
{
	if(poly.size() > 2)
	{
		QPolygonF p = poly;
		if(p.last() != p.first())
			p.append(p.first());

		QPainterPath path;
		path.addPolygon(p);
		return VipShape(path);
	}
	return VipShape();
}

QPainterPath VipDrawShapeMask::shape() const
{
	if (!area())
		return QPainterPath();
	QPainterPath path;
	QPolygonF poly = area()->scaleToPosition(m_polygon,sceneModelScales());
	QRectF r = poly.boundingRect().adjusted(-5,-5,5,5);
	path.addRect(r);
	return path;
}

void VipDrawShapeMask::paint(QPainter * painter, const QStyleOptionGraphicsItem * , QWidget *  )
{
	painter->setPen(QPen(Qt::white, 0));
	painter->setBrush(QColor(255, 0, 0, 50));
	painter->setRenderHints(QPainter::HighQualityAntialiasing);

	QPolygonF poly = area()->scaleToPosition(m_polygon,sceneModelScales());
	//TEST: remove Qt::WindingFill
	painter->drawPolygon(poly/*,Qt::WindingFill*/);
}

bool VipDrawShapeMask::sceneEvent ( QEvent * event )
{
	if(!area())
			return false;

	if(event->type() == QEvent::GraphicsSceneMousePress)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		findPlotSceneModel(evt->scenePos());
		m_polygon.append(area()->positionToScale(evt->pos(),sceneModelScales()));
		return true;
	}
	else if(event->type() == QEvent::GraphicsSceneMouseMove)
	{
		QGraphicsSceneMouseEvent * evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		m_polygon.append(area()->positionToScale(evt->pos(),sceneModelScales()));
		this->prepareGeometryChange();
		return true;
	}
	else if(event->type() == QEvent::GraphicsSceneMouseRelease)
	{
		VipShape sh = createShape(m_polygon);
		if(!sh.isNull())
			this->setLastShape(sh);

		m_polygon.clear();
		return true;
	}

	return false;
}





VipShapeButton::VipShapeButton(QWidget * draw_area, QWidget * parent)
	:QToolButton(parent), m_draw_area(draw_area)
{
	this->setObjectName("Create ROI");
	this->setToolTip("<b>Create a Region Of Intereset</b><br>Click to add a rectangle shape.<br>Use the right arrow to select a different shape.");
	QMenu * addMenu = new QMenu(this);
	connect(addMenu->addAction(vipIcon("rectangle.png"), "Rectangle"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addRect()));
	connect(addMenu->addAction(vipIcon("ellipse.png"), "Ellipse"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addEllipse()));
	connect(addMenu->addAction(vipIcon("polygon.png"), "Polygon"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addPolygon()));
	connect(addMenu->addAction(vipIcon("free_roi.png"), "Free region"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addMask()));
	connect(addMenu->addAction(vipIcon("polyline.png"), "Polyline"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addPolyline()));
	connect(addMenu->addAction(vipIcon("pdc.png"), "Point"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addPixel()));
	addMenu->addSeparator();
	connect(addMenu->addAction(vipIcon("save.png"), "Save ROIs"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer()->editor(), SLOT(saveShapes()));
	connect(addMenu->addAction(vipIcon("open.png"), "Load ROI file"), SIGNAL(triggered(bool)), vipGetSceneModelWidgetPlayer()->editor(), SLOT(openShapes()));

	this->setIconSize(QSize(18, 18));
	this->setCheckable(true);

	this->setPopupMode(QToolButton::MenuButtonPopup);
	this->setMenu(addMenu);
	this->setIcon(vipIcon("roi.png"));
	addMenu->hide();
	//connect(this, SIGNAL(clicked(bool)), vipGetSceneModelWidgetPlayer(), SLOT(addRect()));
	connect(this, SIGNAL(clicked(bool)), this, SLOT(buttonClicked(bool)));

	connect(addMenu, SIGNAL(triggered(QAction *)), this, SLOT(started()));
	connect(vipGetSceneModelWidgetPlayer(), SIGNAL(stopShape()), this, SLOT(finished()));
}
VipShapeButton::~VipShapeButton()
{
	qApp->removeEventFilter(this);
}

bool	VipShapeButton::eventFilter(QObject * /*watched*/, QEvent *event)
{
	if (!m_draw_area)
		return false;

	if (event->type() == QEvent::MouseButtonPress)
	{
		const QPoint pt = m_draw_area->mapFromGlobal(QCursor::pos());
		if (!QRect(0, 0, m_draw_area->width(), m_draw_area->height()).contains(pt))
			finished();

		const QPoint pt2 = this->mapFromGlobal(QCursor::pos());
		if (QRect(0, 0, width(), height()).contains(pt2)) {
			finished();
			return true;
		}
	}

	return false;
}

void VipShapeButton::buttonClicked(bool enable)
{
	if (enable)
	{
		QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(),"addRect",Qt::DirectConnection);
		started();
	}
	else
		finished();
}

void VipShapeButton::started()
{
	qApp->removeEventFilter(this);
	qApp->installEventFilter(this);
	this->blockSignals(true);
	this->setChecked(true);
	this->blockSignals(false);
}
void VipShapeButton::finished()
{
	qApp->removeEventFilter(this);
	this->blockSignals(true);
	this->setChecked(false);
	this->blockSignals(false);

	vipGetSceneModelWidgetPlayer()->blockSignals(true);
	QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "stopAddingShape", Qt::DirectConnection);
	vipGetSceneModelWidgetPlayer()->blockSignals(false);
}






class EditProperty : public QWidget
{
	QLineEdit m_name;
	QLineEdit m_value;
public:
	EditProperty():QWidget()
	{
		QHBoxLayout * lay = new QHBoxLayout();
		lay->addWidget(&m_name);
		lay->addWidget(&m_value);
		setLayout(lay);

		m_name.setPlaceholderText("name");
		m_name.setToolTip("Property name");
		m_value.setPlaceholderText("value");
		m_value.setToolTip("Property value");
	}

	QString name() const {return m_name.text();}
	QVariant value() const {
		QString val = m_value.text();
		QVariant v = val;
		if(v.convert(QMetaType::Double))
			return v;
		else if(!m_value.text().isEmpty())
			return QVariant(m_value.text());
		else
			return QVariant();
	}
};

class DefaultValue : public QWidget
{
	QComboBox m_attributes;
	QLineEdit m_value;
public:
	DefaultValue()
	{
		QHBoxLayout * lay = new QHBoxLayout();
		lay->addWidget(&m_attributes);
		lay->addWidget(&m_value);
		setLayout(lay);

		m_attributes.setToolTip("Attribute to save");
		m_value.setPlaceholderText("default value");
		m_value.setToolTip("Attribute default value. Format:\n'0' : numeric value\n'(2+7j)' : complex value\n'[255,255,0,0]' : ARGB value");
	}

	void setAttributes(const QStringList & lst)
	{
		QMap<QString, int> attrs;
		for (int i = 0; i < lst.size(); ++i)
			attrs.insert(lst[i], 0);
		m_attributes.clear();
		for(QMap<QString, int>::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
			m_attributes.addItem(it.key());
	}

	QComboBox * attributes() {return &m_attributes;}
	QLineEdit * defaultValue() {return &m_value;}

	QString attribute() const {return m_attributes.currentText();}
	QVariant value() const {
		QString val = m_value.text();
		QTextStream str(&val,QIODevice::ReadOnly);
		QColor c;
		if((str >> c).status() == QTextStream::Ok)
			return QVariant::fromValue(c);

		QVariant v = val;
		if(v.convert(QMetaType::Double))
			return v;
		else if(v.convert(qMetaTypeId<complex_d>()))
			return v;
		else if(v.convert(qMetaTypeId<complex_f>()))
			return v;
		else if(!m_value.text().isEmpty())
			return QVariant(m_value.text());
		else
			return QVariant();
	}
};




VipFunctionDispatcher<2> & vipFDShapeStatistics()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}




class ShowHideGroups::PrivateData
{
public:
	QAction* showAll;
	QAction* hideAll;
	QLabel *lastGroup;
	QToolBar bar;

	QList<QCheckBox*> groups;
};
ShowHideGroups::ShowHideGroups(QWidget * parent)
	:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->lastGroup = new QLabel();
	m_data->lastGroup->setText("Change groups visibility");
	m_data->lastGroup->setToolTip("Change groups visibility");
	m_data->showAll = m_data->bar.addAction(vipIcon("show.png"), "Show all groups");
	m_data->hideAll = m_data->bar.addAction(vipIcon("hide.png"), "Hide all groups");
	m_data->bar.addWidget(m_data->lastGroup);
	m_data->bar.setIconSize(QSize(18, 18));

	QVBoxLayout * lay = new QVBoxLayout();
	lay->setSpacing(0);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&m_data->bar);
	setLayout(lay);

	connect(m_data->showAll, SIGNAL(triggered(bool)), this, SLOT(showAll()));
	connect(m_data->hideAll, SIGNAL(triggered(bool)), this, SLOT(hideAll()));
}

ShowHideGroups::~ShowHideGroups()
{
	delete m_data;
}

void ShowHideGroups::computeGroups(const QList<VipPlotSceneModel*> & models)
{
	//get all groups and their visibility
	QMap<QString,bool> groups;

	for (int i = 0; i < models.size(); ++i) {
		if (VipPlotSceneModel * sm = models[i]) {
			const QStringList grs = sm->sceneModel().groups();
			for (int g = 0; g < grs.size(); ++g) {
				//get a shape from this group to check visibility
				bool vis = false;
				vis = sm->groupVisible(grs[g]);
				/*const QList<VipPlotShape*> shapes = sm->shapes(grs[g]);
				if (shapes.size() && shapes.first()) vis = shapes.first()->isVisible();
				//special case: "ROI"
				if (grs[g] == "ROI" && shapes.isEmpty()) vis = true;*/
				groups[grs[g]] = vis;
			}
		}
	}

	//remove previous checkboxes
	for (int i = 0; i < m_data->groups.size(); ++i)
		delete m_data->groups[i];
	m_data->groups.clear();

	//add new checkboxes
	for (QMap<QString, bool>::const_iterator it = groups.begin(); it != groups.end(); ++it) {
		//we take only non empty groups
		if (it.key().size()) {
			QCheckBox * check = new QCheckBox(it.key());
			check->setChecked(it.value());
			m_data->groups.append(check);
			connect(check, SIGNAL(clicked(bool)), this, SLOT(checked()));
			layout()->addWidget(check);
		}
	}
}

QStringList ShowHideGroups::availableGroups() const
{
	QStringList res;
	for (int i = 0; i < m_data->groups.size(); ++i) res << m_data->groups[i]->text();
	return res;
}
QStringList ShowHideGroups::visibleGroups() const
{
	QStringList res;
	for (int i = 0; i < m_data->groups.size(); ++i) 
		if(m_data->groups[i]->isChecked()) res << m_data->groups[i]->text();
	return res;
}
QStringList ShowHideGroups::hiddenGroups() const
{
	QStringList res;
	for (int i = 0; i < m_data->groups.size(); ++i) 
		if (!m_data->groups[i]->isChecked()) res << m_data->groups[i]->text();
	return res;
}
/*QString ShowHideGroups::currentGroup() const {
	return m_data->lastGroup->text();
}*/
/*void ShowHideGroups::setCurrentGroup(const QString & group) {
	return m_data->lastGroup->setText(group);
}*/

void ShowHideGroups::showAll()
{
	for (int i = 0; i < m_data->groups.size(); ++i) {
		m_data->groups[i]->blockSignals(true);
		m_data->groups[i]->setChecked(true);
		m_data->groups[i]->blockSignals(false);
	}
	Q_EMIT changed();
}
void ShowHideGroups::hideAll()
{
	for (int i = 0; i < m_data->groups.size(); ++i) {
		m_data->groups[i]->blockSignals(true);
		m_data->groups[i]->setChecked(false);
		m_data->groups[i]->blockSignals(false);
	}
	Q_EMIT changed();
}

void ShowHideGroups::checked()
{
	Q_EMIT changed();
}













#include "VipTimer.h"


class AttributesEditor : public QWidget
{
	QGridLayout* lay;
	QList<QLabel*> names;
	QList<QLineEdit*> values;
	QList<VipShape> shapes;
	QPointer< VipSceneModelEditor> editor;
public:
	AttributesEditor(VipSceneModelEditor * ed)
		:QWidget(),editor(ed)
	{
		setLayout(lay = new QGridLayout());
	}

	void setShapes(const QList<VipShape>& sh)
	{
		shapes = sh;
		for (int i = 0; i < values.size(); ++i)
			delete values[i];
		for (int i = 0; i < names.size(); ++i)
			delete names[i];
		values.clear();
		names.clear();

		QSet<QString> common;
		QVariantMap attributes;
		for (int i = 0; i < sh.size(); ++i) {
			const QVariantMap attrs = sh[i].attributes();
			if (common.isEmpty()) {
				common = attrs.keys().toSet();
				attributes = attrs;
			}
			else {
				common = common.intersect(attrs.keys().toSet());
				for (QSet<QString>::iterator it = common.begin(); it != common.end(); ++it) {
					QVariant v = attrs[*it];
					if (v.toString() != attributes[*it].toString())
						attributes[*it] = QVariant();
				}
			}
		}
		int row = 0;
		for (QSet<QString>::iterator it = common.begin(); it != common.end(); ++it, ++row) {
			if (it->startsWith("_vip_"))
				continue;
			QLabel* name = new QLabel(*it);
			QLineEdit* value = new QLineEdit(attributes[*it].toString());
			lay->addWidget(name, row, 0);
			lay->addWidget(value, row, 1);
			value->setProperty("key", *it);
			names << name;
			values << value;
			connect(value, &QLineEdit::returnPressed, this, &AttributesEditor::textEdited);
		}
	}

	void textEdited()
	{
		//save current state
		QList<VipPlotSceneModel*> models;
		if (editor && editor->scene()) {
			//push current state for undo
			QList<VipPlotSceneModel*> models = vipCastItemList<VipPlotSceneModel*>(editor->scene()->items());
			for (int i=0; i < models.size(); ++i)
				VipSceneModelState::instance()->pushState(editor->player(), models[i]);
		}

		QString name = sender()->property("key").toString();
		if (!name.isEmpty()) {
			QString value = qobject_cast<QLineEdit*>(sender())->text();
			for (int i = 0; i < shapes.size(); ++i) {
				shapes[i].setAttribute(name, value);
			}
		}
	}
};


class VipSceneModelEditor::PrivateData
{
public:
	VipTextWidget title;
	VipPenButton pen;
	VipPenButton brush;
	QCheckBox titleVisible;
	QCheckBox groupVisible;
	QCheckBox idVisible;
	QCheckBox attrsVisible;
	QCheckBox invertTextColor;
	QCheckBox innerPixels;
	QCheckBox applyAll;
	ShowHideGroups showHide;
	QScrollArea statArea;
	QLabel *statLabel;
	AttributesEditor *editor;
	QSet<QString> inspected;

	QToolBar edition;
	QToolBar io;
	QToolButton save;

	//QPointer<VipPlotSceneModel> sceneModel;
	QPointer<VipPlayer2D> player2D;
	QPointer<QGraphicsScene> scene;
	QPointer<VipPlotSceneModel> lastSelected;

	QVector<QRect> rects;

};

VipSceneModelEditor::VipSceneModelEditor(QWidget * parent )
:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->editor = new AttributesEditor(this);

	QGridLayout * lay = new QGridLayout();
	int row = -1;

	//lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(&m_data->io,++row,0,1,2);

	lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(new QLabel("Shape text style"),++row,0);
	lay->addWidget(&m_data->title,row,1);
	lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(new QLabel("Border pen"),++row,0);
	lay->addWidget(&m_data->pen,row,1);

	lay->addWidget(new QLabel("Background brush"),++row,0);
	lay->addWidget(&m_data->brush,row,1);

	lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(&m_data->edition,++row,0,1,2);
	lay->addWidget(m_data->editor, ++row, 0, 1, 2);

	lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(&m_data->titleVisible,++row,0,1,2);
	lay->addWidget(&m_data->groupVisible,++row,0,1,2);
	lay->addWidget(&m_data->idVisible,++row,0,1,2);
	lay->addWidget(&m_data->attrsVisible, ++row, 0, 1, 2);
	lay->addWidget(&m_data->invertTextColor, ++row, 0, 1, 2);
	lay->addWidget(&m_data->applyAll,++row,0,1,2);

	lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(&m_data->innerPixels,++row,0,1,2);

	lay->addWidget(&m_data->showHide,++row, 0, 1, 2);

	//lay->addWidget(VipLineWidget::createHLine(),++row,0,1,2);

	lay->addWidget(&m_data->statArea,++row,0,1,2);

	lay->setContentsMargins(0,0,0,0);
	setLayout(lay);

	m_data->title.edit()->hide();
	m_data->pen.setMode(VipPenButton::Pen);
	m_data->brush.setMode(VipPenButton::Brush);

	m_data->titleVisible.setText("Display shapes title");
	m_data->groupVisible.setText("Display shapes group");
	m_data->idVisible.setText("Display shapes identifier");
	m_data->attrsVisible.setText("Display shapes attributes");
	m_data->invertTextColor.setText("Adjust text color");
	m_data->invertTextColor.setToolTip("Adjust the shape text color to the background in order to be always visible");
	m_data->innerPixels.setText("Display exact pixels");
	m_data->applyAll.setText("Apply to all shapes");

	m_data->statLabel = new QLabel();
	m_data->statArea.setWidget(m_data->statLabel);
	m_data->statArea.setWidgetResizable(true);
	m_data->statLabel->show();
	m_data->statLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_data->statLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	//TODO: completely remove m_data->statArea
	m_data->statArea.hide();

	QMenu * save_menu = new QMenu();
	connect(save_menu->addAction("Save shapes..."),SIGNAL(triggered(bool)),this,SLOT(saveShapes()));
	connect(save_menu->addAction("Create attribute image for selected shapes..."),SIGNAL(triggered(bool)),this,SLOT(saveShapesAttribute()));
	connect(save_menu->addAction("Save image inside selected shapes bounding rect..."),SIGNAL(triggered(bool)),this,SLOT(saveShapesImage()));
	save_menu->addSeparator();
	connect(save_menu->addAction("Create HDF5 scene model..."), SIGNAL(triggered(bool)), this, SLOT(saveH5ShapesAttribute()));

	m_data->save.setIcon(vipIcon("save_as.png"));
	m_data->save.setText("Save shape...");
	m_data->save.setMenu(save_menu);
	m_data->save.setAutoRaise(true);
	m_data->save.setPopupMode(QToolButton::InstantPopup);
	m_data->save.setMinimumWidth(25);
	m_data->io.addWidget(&m_data->save);
	QAction *open = m_data->io.addAction(vipIcon("open.png"), "Open a shape file...");
	connect(open,SIGNAL(triggered(bool)),this,SLOT(openShapes()));
	m_data->io.setIconSize(QSize(18,18));

	m_data->io.addSeparator();
	QAction *select_all = m_data->io.addAction(vipIcon("select.png"), "Select/deselect all visible shapes");
	QAction *del_all = m_data->io.addAction(vipIcon("del.png"), "Remove all visible selected shapes");

	m_data->edition.setIconSize(QSize(18,18));
	connect(m_data->edition.addAction(vipIcon("or.png"),"Compute the union of selected shapes"),SIGNAL(triggered(bool)),this,SLOT(uniteShapes()));
	connect(m_data->edition.addAction(vipIcon("and.png"),"Compute the intersection of selected shapes"),SIGNAL(triggered(bool)),this,SLOT(intersectShapes()));
	connect(m_data->edition.addAction(vipIcon("substract_roi.png"), "Subtract a shape to another (use the order of selection)"), SIGNAL(triggered(bool)), this, SLOT(subtractShapes()));

	
	
	m_data->edition.addSeparator();
	connect(m_data->edition.addAction(vipIcon("add_attribute.png"),"Set a property to selected shapes"),SIGNAL(triggered(bool)),this,SLOT(addProperty()));
	connect(m_data->edition.addAction(vipIcon("remove_attribute.png"),"Remove a property to selected shapes"),SIGNAL(triggered(bool)),this,SLOT(removeProperty()));

	connect(select_all, SIGNAL(triggered(bool)), this, SLOT(selectUnselectAll()));
	connect(del_all, SIGNAL(triggered(bool)), this, SLOT(deleteSelected()));

	connect(&m_data->title,SIGNAL(changed(const VipText&)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->pen,SIGNAL(penChanged(const QPen&)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->brush,SIGNAL(penChanged(const QPen&)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->titleVisible,SIGNAL(clicked(bool)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->groupVisible,SIGNAL(clicked(bool)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->idVisible,SIGNAL(clicked(bool)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->attrsVisible, SIGNAL(clicked(bool)), this, SLOT(emitSceneModelChanged()));
	connect(&m_data->invertTextColor, SIGNAL(clicked(bool)), this, SLOT(emitSceneModelChanged()));
	connect(&m_data->innerPixels,SIGNAL(clicked(bool)),this,SLOT(emitSceneModelChanged()));
	connect(&m_data->applyAll,SIGNAL(clicked(bool)),this,SLOT(emitSceneModelChanged()));

	//update the scene model when visibility changed through user input...
	connect(&m_data->showHide,SIGNAL(changed()),this,SLOT(emitSceneModelChanged()));
	//...then reset scene model to update other parameters based on visible groups
	connect(&m_data->showHide, SIGNAL(changed()), this, SLOT(resetPlayer()));

}

VipSceneModelEditor::~VipSceneModelEditor()
{
	//vipProcessEvents(NULL, 20);
	delete m_data;
}

void VipSceneModelEditor::resetPlayer()
{
	setPlayer(m_data->player2D);
}

void VipSceneModelEditor::recomputeAttributes()
{
	if (!m_data->scene) {
		m_data->editor->setShapes(QList<VipShape>());
		return;
	}

	//set the new plot scene models
	QList<VipPlotSceneModel*> models;
	if (m_data->scene)
		models = vipCastItemList<VipPlotSceneModel*>(m_data->scene->items());
	//set attributes
	QList<VipShape> shapes;
	for (int i = 0; i < models.size(); ++i) {
		QList<VipPlotShape*> sh = models[i]->shapes(1);
		for (int j = 0; j < sh.size(); ++j)
			shapes << sh[j]->rawData();
	}
	m_data->editor->setShapes(shapes);

}

/*void VipSceneModelEditor::findSelectedSceneModel()
{
	//when the scene selection state change, this might meen that a different scene model has been selected.
	//so find the current selected scene model and call setSceneModel if it changed.

	if (m_data->scene)
	{
		QList<VipPlotSceneModel*> models = vipCastItemList<VipPlotSceneModel*>(m_data->scene->items());

		//find the first VipPlotSceneModel with selected shapes (or the first one if there are no selected shapes) and set it to the scene model editor.
		VipPlotSceneModel * model = NULL;
		for (int i = 0; i < models.size(); ++i)
		{
			if (!model)
				model = models[i];

			if (models[i]->shapes(1).size())
			{
				model = models[i];
				break;
			}
		}

		if (model && model != m_data->sceneModel)
			setSceneModel(model);
	}
}*/

VipPlotSceneModel * VipSceneModelEditor::lastSelected() const
{
	if (!m_data->player2D)
		return  NULL;
	if (!m_data->lastSelected)
		return m_data->player2D->plotSceneModel();
	return m_data->lastSelected;
}

QGraphicsScene* VipSceneModelEditor::scene() const
{
	return m_data->scene;
}

VipPlayer2D* VipSceneModelEditor::player() const
{
	return m_data->player2D;
}

void VipSceneModelEditor::selectionChanged(VipPlotItem * item)
{
	if (VipPlotShape * sh = qobject_cast<VipPlotShape*>(item)) {
		if (sh->isSelected()) {
			if (VipPlotSceneModel * sm = sh->property("VipPlotSceneModel").value<VipPlotSceneModel*>())
				if (sm != m_data->lastSelected) {
					m_data->lastSelected = sm;
					resetPlayer();
				}
		}
	}
	recomputeAttributes();
}

void VipSceneModelEditor::setPlayer(VipPlayer2D * pl)
{
	if (m_data->player2D != pl)
		m_data->lastSelected = NULL;
	if (m_data->player2D) {
		disconnect(m_data->player2D->plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem *)), this, SLOT(selectionChanged(VipPlotItem *)));
	}

	m_data->player2D = pl;
	m_data->scene = pl ? pl->plotWidget2D()->scene() : NULL;
	if (!pl)
		return;

	if (m_data->player2D) {
		connect(m_data->player2D->plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem *)), this, SLOT(selectionChanged(VipPlotItem *)));
	}

	
	m_data->title.blockSignals(true);
	m_data->brush.blockSignals(true);
	m_data->pen.blockSignals(true);
	m_data->groupVisible.blockSignals(true);
	m_data->idVisible.blockSignals(true);
	m_data->attrsVisible.blockSignals(true);
	m_data->invertTextColor.blockSignals(true);
	m_data->innerPixels.blockSignals(true);
	m_data->statArea.blockSignals(true);
	m_data->statLabel->blockSignals(true);
	m_data->titleVisible.blockSignals(true);


	//set the new plot scene models
	QList<VipPlotSceneModel*> models;
	if(m_data->scene )
		models = vipCastItemList<VipPlotSceneModel*>(m_data->scene->items());
	m_data->showHide.computeGroups(models);


	//set attributes
	recomputeAttributes();

	
	QString visible;
	QStringList tmp = m_data->showHide.visibleGroups();
	if (!tmp.isEmpty()) visible = tmp.last();
	if (!visible.isEmpty() && lastSelected()) {
		VipPlotSceneModel * sm = lastSelected();
		m_data->title.setText(VipText(QString(), sm->textStyle(visible)));

		//update brush and pen
		m_data->pen.setPen(sm->pen(visible));
		m_data->brush.setPen(QPen(sm->brush(visible), 1));

		/*QString key = QString::number((quint64)sm) + visible;
		if (m_data->inspected.find(key) == m_data->inspected.end()) {
			if (sm == m_data->player2D->plotSceneModel())
			{
				// for the main VipPlotSceneModel of the player, use the global option displayExactPixels
				if (VipGuiDisplayParamaters::instance()->displayExactPixels())
					sm->setDrawComponent(QString(), VipPlotShape::FillPixels);
				m_data->inspected.insert(key);
			}
		}*/

		//update visibility
		m_data->titleVisible.setChecked(sm->testDrawComponent(visible, VipPlotShape::Title));
		m_data->groupVisible.setChecked(sm->testDrawComponent(visible, VipPlotShape::Group));
		m_data->idVisible.setChecked(sm->testDrawComponent(visible, VipPlotShape::Id));
		m_data->attrsVisible.setChecked(sm->testDrawComponent(visible, VipPlotShape::Attributes));
		m_data->innerPixels.setChecked(sm->testDrawComponent(visible, VipPlotShape::FillPixels));
		m_data->invertTextColor.setChecked(sm->adjustTextColor(visible));

		
	}

	//updateStatistics();

	m_data->title.blockSignals(false);
	m_data->brush.blockSignals(false);
	m_data->pen.blockSignals(false);
	m_data->groupVisible.blockSignals(false);
	m_data->idVisible.blockSignals(false);
	m_data->attrsVisible.blockSignals(false);
	m_data->invertTextColor.blockSignals(false);
	m_data->innerPixels.blockSignals(false);
	m_data->statArea.blockSignals(false);
	m_data->statLabel->blockSignals(false);
	m_data->titleVisible.blockSignals(false);
}

void VipSceneModelEditor::updateSceneModels()
{
	if (!m_data->player2D)
		return;

	//get visible groups
	QStringList visible = m_data->showHide.visibleGroups();

	//compute all the VipPlotSceneModel to apply the parameters to
	QList<VipPlotSceneModel*> models;
	if (!m_data->applyAll.isChecked())
	{
		//use all visible models
		models = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotSceneModel*>();
	}
	else
	{
		QList<VipAbstractPlayer*> players = VipFindChidren::findChildren<VipAbstractPlayer*>();
		for(int i=0; i < players.size(); ++i)
			if(players[i]->plotWidget2D())
				models.append(players[i]->plotWidget2D()->area()->findItems<VipPlotSceneModel*>());
	}

	//the visibility is applied to all models
	for (int m = 0; m < models.size(); ++m)
	{
		VipPlotSceneModel * sm = models[m];

		//update visibility
		QStringList groups = sm->sceneModel().groups();
		for (int i = 0; i < groups.size(); ++i) {
			//avoid emitting the groupsChanged() signal that will trigger the resetPlayer slot
			sm->blockSignals(true);
			sm->setGroupVisible(groups[i], visible.indexOf(groups[i]) >= 0);
			sm->blockSignals(false);
		}
	}

	//the other parameters only to the last selected plot scene model, except if 'apply to all' is checked
	if (!m_data->applyAll.isChecked()) {
		models.clear();
		if (VipPlotSceneModel * sm = lastSelected())
			models << sm;
	}

	for(int m=0; m < models.size(); ++m)
	{
		VipPlotSceneModel * sm = models[m];
		for(int i=0; i < visible.size(); ++i)
		{
			//update text visibility, brush and pen

			if(!sender() || (sender() == &m_data->title)) //check sender to avoid updating all properties when 'All' is selected
				sm->setTextStyle(visible[i],m_data->title.getText().textStyle());
			if(!sender() || (sender() == &m_data->titleVisible))
				sm->setDrawComponent(visible[i],VipPlotShape::Title,m_data->titleVisible.isChecked());
			if(!sender() || (sender() == &m_data->idVisible))
				sm->setDrawComponent(visible[i],VipPlotShape::Id,m_data->idVisible.isChecked());
			if (!sender() || (sender() == &m_data->attrsVisible))
				sm->setDrawComponent(visible[i], VipPlotShape::Attributes, m_data->attrsVisible.isChecked());
			if (!sender() || (sender() == &m_data->invertTextColor))
				sm->setAdjustTextColor(visible[i], m_data->invertTextColor.isChecked());
			if(!sender() || (sender() == &m_data->groupVisible))
				sm->setDrawComponent(visible[i],VipPlotShape::Group,m_data->groupVisible.isChecked());
			if(!sender() || (sender() == &m_data->innerPixels))
				sm->setDrawComponent(visible[i],VipPlotShape::FillPixels,m_data->innerPixels.isChecked());
			if(!sender() || (sender() == &m_data->pen))
				sm->setPen(visible[i],m_data->pen.pen());
			if(!sender() || (sender() == &m_data->brush))
				sm->setBrush(visible[i],m_data->brush.pen().brush());
		}
	}

	if (m_data->applyAll.isChecked())
	{
		//if apply to all, update the default settings
		if (sender() == &m_data->innerPixels)
		{
			if (m_data->innerPixels.isChecked())
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() | VipPlotShape::FillPixels);
			else
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & (~VipPlotShape::FillPixels));
		}
		else if (sender() == &m_data->titleVisible)
		{
			if (m_data->titleVisible.isChecked())
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() | VipPlotShape::Title);
			else
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & (~VipPlotShape::Title));
		}
		else if (sender() == &m_data->idVisible)
		{
			if (m_data->idVisible.isChecked())
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() | VipPlotShape::Id);
			else
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & (~VipPlotShape::Id));
		}
		else if (sender() == &m_data->attrsVisible)
		{
			if (m_data->attrsVisible.isChecked())
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() | VipPlotShape::Attributes);
			else
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & (~VipPlotShape::Attributes));
		}
		else if (sender() == &m_data->groupVisible)
		{
			if (m_data->groupVisible.isChecked())
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() | VipPlotShape::Group);
			else
				VipGuiDisplayParamaters::instance()->setShapeDrawComponents(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & (~VipPlotShape::Group));
		}
		else if (sender() == &m_data->brush)
		{
			VipGuiDisplayParamaters::instance()->setShapeBackgroundBrush(m_data->brush.pen().brush());
		}
		else if (sender() == &m_data->pen)
		{
			VipGuiDisplayParamaters::instance()->setShapeBorderPen(m_data->pen.pen());
		}
	}
	
}

void VipSceneModelEditor::emitSceneModelChanged()
{
	updateSceneModels();
	Q_EMIT sceneModelChanged();
}

void VipSceneModelEditor::uniteShapes()
{
	if (!m_data->player2D) return;
	//Find selected shapes and make sure they belong to the same VipPlotSceneModel
	QSet<VipPlotSceneModel*> sms;
	VipPlotSceneModel * insert_into = NULL; //result is add to this VipPlotSceneModel
	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	for (int i = 0; i < shapes.size(); ++i) {
		if (VipPlotSceneModel * sm = shapes[i]->property("VipPlotSceneModel").value<VipPlotSceneModel*>())
			sms.insert(sm);
	}
	if (sms.size() != 1)
		shapes.clear();
	else if (!(insert_into = m_data->player2D->findPlotSceneModel((*sms.begin())->axes())))
			shapes.clear();


	if(shapes.size() > 1)
	{
		VipShape sh;
		for(int i=0; i < shapes.size(); ++i)
		{
			sh.unite(shapes[i]->rawData());
			
			if (VipResizeItem * item = shapes[i]->property("VipResizeItem").value<VipResizeItemPtr>())
				item->setSelected(false);
			shapes[i]->setSelected(false);
		}

		if(!sh.shape().isEmpty())
		{
			insert_into->sceneModel().add("ROI",sh);
		}
	}
	else
	{
		QMessageBox::warning(NULL, "Unauthorized operation", "At least 2 shapes from the same scene model must be selected for this operation");
	}
}

void VipSceneModelEditor::intersectShapes()
{
	if (!m_data->player2D) return;
	//Find selected shapes and make sure they belong to the same VipPlotSceneModel
	QSet<VipPlotSceneModel*> sms;
	VipPlotSceneModel * insert_into = NULL; //result is add to this VipPlotSceneModel
	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	for (int i = 0; i < shapes.size(); ++i)
		if (VipPlotSceneModel * sm = shapes[i]->property("VipPlotSceneModel").value<VipPlotSceneModel*>())
			sms.insert(sm);
	if (sms.size() != 1)
		shapes.clear();
	else if (!(insert_into = m_data->player2D->findPlotSceneModel((*sms.begin())->axes())))
		shapes.clear();

	if(shapes.size() > 1)
	{
		VipShape sh = shapes.first()->rawData().copy();
		if (VipResizeItem * item = shapes.first()->property("VipResizeItem").value<VipResizeItemPtr>())
			item->setSelected(false);
		shapes.first()->setSelected(false);

		for(int i=1; i < shapes.size(); ++i)
		{
			sh.intersect(shapes[i]->rawData());
			
			if (VipResizeItem * item = shapes[i]->property("VipResizeItem").value<VipResizeItemPtr>())
				item->setSelected(false);
			shapes[i]->setSelected(false);
		}

		if(!sh.shape().isEmpty())
		{
			insert_into->sceneModel().add("ROI",sh);
		}
	}
	else
	{
		QMessageBox::warning(NULL, "Unauthorized operation", "At least 2 shapes from the same scene model must be selected for this operation");
	}
}


void VipSceneModelEditor::subtractShapes()
{
	if (!m_data->player2D) return;
	//Find selected shapes and make sure they belong to the same VipPlotSceneModel
	QSet<VipPlotSceneModel*> sms;
	VipPlotSceneModel * insert_into = NULL; //result is add to this VipPlotSceneModel
	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	for (int i = 0; i < shapes.size(); ++i)
		if (VipPlotSceneModel * sm = shapes[i]->property("VipPlotSceneModel").value<VipPlotSceneModel*>())
			sms.insert(sm);
	if (sms.size() != 1)
		shapes.clear();
	else if (!(insert_into = m_data->player2D->findPlotSceneModel((*sms.begin())->axes())))
		shapes.clear();

	if (shapes.size() == 2)
	{
		VipShape sh = shapes.first()->rawData().copy();
		if (VipResizeItem * item = shapes.first()->property("VipResizeItem").value<VipResizeItemPtr>())
			item->setSelected(false);
		shapes.first()->setSelected(false);

		sh.subtract(shapes.last()->rawData());
		
		if (!sh.shape().isEmpty())
		{
			insert_into->sceneModel().add("ROI", sh);
		}
	}
	else
	{
		QMessageBox::warning(NULL, "Unauthorized operation", "2 shapes from the same scene model must be selected for this operation");
	}
}


void VipSceneModelEditor::addProperty()
{
	if (!m_data->player2D) return;
	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);

	if(shapes.size())
	{
		EditProperty * edit = new EditProperty();
		VipGenericDialog dialog(edit,"Edit shapes property",this);
		dialog.resize(250,80);
		if(dialog.exec() == QDialog::Accepted)
		{
			QString name = edit->name();
			QVariant value = edit->value();
			if(!name.isEmpty() && !value.isNull() )
			{
				//save current state
				QSet< VipPlotSceneModel*> models;
				for (int i = 0; i < shapes.size(); ++i) {
					if(VipPlotSceneModel * sm = shapes[i]->property("VipPlotSceneModel").value< VipPlotSceneModel*>())
						models.insert(sm);
				}
				for(QSet< VipPlotSceneModel*>::const_iterator it = models.begin(); it != models.end(); ++it)
					VipSceneModelState::instance()->pushState(m_data->player2D, *it);

				for(int i=0; i < shapes.size(); ++i)
					shapes[i]->rawData().setAttribute(name,value);
				recomputeAttributes();
			}
		}
	}
}

void VipSceneModelEditor::removeProperty()
{
	if (!m_data->player2D) return;
	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);

	if(shapes.size())
	{
		QSet<QString> properties;
		for(int i=0; i < shapes.size(); ++i)
			properties |= (shapes[i]->rawData().attributes().keys().toSet());

		if(properties.size())
		{
			QComboBox * box = new QComboBox();
			for (QSet<QString>::iterator it = properties.begin(); it != properties.end(); ++it) {
				if(! it->startsWith("_vip_"))
					box->addItem(*it);
			}
			box->setCurrentIndex(0);

			VipGenericDialog dialog(box,"Remove shapes property",this);
			dialog.resize(80,80);
			if(dialog.exec() == QDialog::Accepted)
			{
				//save current state
				QSet< VipPlotSceneModel*> models;
				for (int i = 0; i < shapes.size(); ++i) {
					if (VipPlotSceneModel* sm = shapes[i]->property("VipPlotSceneModel").value< VipPlotSceneModel*>())
						models.insert(sm);
				}
				for (QSet< VipPlotSceneModel*>::const_iterator it = models.begin(); it != models.end(); ++it)
					VipSceneModelState::instance()->pushState(m_data->player2D, *it);

				//remove the selected property
				QString selected = box->currentText();
				for(int i=0; i < shapes.size(); ++i)
				{
					//remove the attribute from the VipShape and the VipPlotShape
					shapes[i]->rawData().setAttribute(selected,QVariant());
					shapes[i]->setProperty(selected.toLatin1().data(),QVariant());
				}
				recomputeAttributes();
			}
		}
	}

}

void VipSceneModelEditor::saveShapes()
{
	//Save all editable scene models

	if (!m_data->player2D) return;
	 
	
	QStringList filters = VipIODevice::possibleWriteFilters(QString(), QVariantList() << QVariant::fromValue(VipSceneModelList()));
	QList<VipPlotSceneModel*> pmodels = m_data->player2D->plotSceneModels();
	VipSceneModelList models;
	for (int i = 0; i < pmodels.size(); ++i) {
		models << vipCopyVideoSceneModel( pmodels[i]->sceneModel(), qobject_cast<VipVideoPlayer*>(m_data->player2D), NULL);
	}

	if(models.size())
	{
		QString filename = VipFileDialog::getSaveFileName(NULL, "Save shapes", filters.join(";;"));
		if(!filename.isEmpty())
		{
			QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(filename, QVariantList() << QVariant::fromValue(VipSceneModelList()));
			VipIODevice * dev = VipCreateDevice::create(devices, filename);
			if (dev && dev->open(VipIODevice::WriteOnly))
			{
				dev->inputAt(0)->setData(models);
				dev->update();
			}
		}
	}
}

VipMultiNDArray VipSceneModelEditor::createH5ShapeAttributes(const QVariant & background)
{
	// Create a multi component image based on all shape attributes
	
	if (!m_data->player2D)
		return VipMultiNDArray();

	//only available for VipVideoPlayer
	if (!qobject_cast<VipVideoPlayer*>(m_data->player2D))
		return VipMultiNDArray();

	VipVideoPlayer* pl = static_cast<VipVideoPlayer*>(m_data->player2D.data());

	//get the current image
	VipRasterData data = pl->spectrogram()->rawData();
	VipNDArray image = data.extract(data.boundingRect());
	if (image.isEmpty())
		return VipMultiNDArray();

	QList<VipPlotShape*> shapes;
	// Try to save only selected shapes
	shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	// If no selection, save all
	if(shapes.isEmpty())
		shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 2, 1);
	if (!shapes.size())
		return VipMultiNDArray();

	// Compute properties
	QMap<QString, QVariant> properties;
	for (int i = 0; i < shapes.size(); ++i)
		properties.unite(shapes[i]->rawData().attributes());
	QStringList keys = properties.keys();
	for (int i = 0; i < keys.size(); ++i) {
		if (keys[i].startsWith("_vip_")) {
			keys.removeAt(i);
			--i;
		}
		else {
			QString name = keys[i];
			QVariant value = properties[name];
			int data_type = value.userType();
			if (data_type == QMetaType::QString || data_type == QMetaType::QByteArray) {
				// Try to convert to double
				bool ok = false;
				value.toDouble(&ok);
				if (ok)
					data_type = QMetaType::Double;
			}
			printf("name: %s, dt: %i\n", name.toLatin1().data(), data_type);
			if (name.isEmpty() || data_type != QMetaType::Double  )
			{
				keys.removeAt(i);
				--i;
			}
		}
	}
	keys = keys.toSet().toList();
	keys.sort();

	if (!properties.size())
		return VipMultiNDArray();

	QMap<QString, VipNDArray> ars;

	for (int i=0; i < keys.size(); ++i)
	{
		QString name = keys[i];
		QVariant value = properties[name];
		int data_type = QMetaType::Double;//value.userType();

		//create the output image
		VipNDArray output(data_type, image.shape());
		output.fill(background);

		for (int i = 0; i < shapes.size(); ++i)
			shapes[i]->rawData().writeAttribute(name, output, data.boundingRect().topLeft().toPoint());

		ars[name] = output;
	}
	VipMultiNDArray array;
	array.setNamedArrays(ars);
	return array;

	
}

void VipSceneModelEditor::saveH5ShapesAttribute()
{
	//TODO
	
	VipMultiNDArray array = createH5ShapeAttributes(QVariant(vipNan()));
	if (array.arrayCount() == 0) {
		VIP_LOG_ERROR("Could not create multi component image for these shapes");
		return;
	}

	// Use the H5StillImageWriter from the H5StillImùage plugin

	VipIODevice* dev = vipCreateVariant("H5StillImageWriter*").value<VipIODevice*>();
	if (!dev) {
		VIP_LOG_ERROR("Plugin H5StillImage is not available, creating H5 attribute file failed");
		return ;
	}

	QString filename = VipFileDialog::getSaveFileName(vipGetMainWindow(), "Create H5 scene model", "HDF5 files (*.h5)");
	if (filename.isEmpty()) {
		delete dev;
		return;
	}
	
	dev->setPath(filename);
	dev->open(VipIODevice::WriteOnly);

	VipAnyData any(QVariant::fromValue(VipNDArray(array)), 0);
	dev->inputAt(0)->setData(any);

	dev->update();
	dev->close();
	delete dev;
	return ;
}

void VipSceneModelEditor::saveShapesAttribute()
{
	if (!m_data->player2D)
		return;

	//only available for VipVideoPlayer
	if(!qobject_cast<VipVideoPlayer*>(m_data->player2D))
		return;

	VipVideoPlayer * pl = static_cast<VipVideoPlayer*>(m_data->player2D.data());

	//get the current image
	VipRasterData data = pl->spectrogram()->rawData();
	VipNDArray image = data.extract(data.boundingRect());
	if(image.isEmpty())
		return;

	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	if(shapes.size())
	{
		QMap<QString,QVariant> properties;
		for(int i=0; i < shapes.size(); ++i)
			properties.unite(shapes[i]->rawData().attributes());
		QStringList keys = properties.keys();
		for(int i=0; i < keys.size(); ++i) {
			if (keys[i].startsWith("_vip_") ){
				keys.removeAt(i);
				--i;
			} 
		}

		if(properties.size())
		{
			DefaultValue * dvalue = new DefaultValue();
			dvalue->setAttributes(keys);
			VipGenericDialog dialog(dvalue,"Default attribute value",this);
			dialog.resize(300,80);
			if(dialog.exec() == QDialog::Accepted)
			{
				QString name = dvalue->attribute();
				QVariant value = dvalue->value();
				int data_type = properties[name].userType();

				QString filename = VipFileDialog::getSaveFileName(NULL,"Save attribute image","TEXT file (*.txt)");
				if(!filename.isEmpty())
				{
					//create the output image
					VipNDArray output(data_type,image.shape());
					output.fill(value);

					for(int i=0; i < shapes.size(); ++i)
						shapes[i]->rawData().writeAttribute(name,output,data.boundingRect().topLeft().toPoint());

					//save the image
					QFile fout(filename);
					fout.open(QFile::WriteOnly|QFile::Text);
					QTextStream stream(&fout);
					stream << output;
				}
			}
		}
	}
}

void VipSceneModelEditor::saveShapesImage()
{
	if(!m_data->player2D)
		return;

	//get the current image
	if (!qobject_cast<VipVideoPlayer*>(m_data->player2D))
		return;

	VipVideoPlayer * pl = static_cast<VipVideoPlayer*>(m_data->player2D.data());
	VipRasterData data = pl->spectrogram()->rawData();

	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	if(shapes.size())
	{

			QString filename = VipFileDialog::getSaveFileName(NULL,"Save image","TEXT file (*.txt)");
			if(!filename.isEmpty())
			{
				//compute the bounding rect
				QRectF bounding;
				for(int i=0; i < shapes.size(); ++i)
					bounding |= shapes[i]->rawData().boundingRect();

				//extract image and save it
				VipNDArray output = data.extract(bounding);

				QFile fout(filename);
				fout.open(QFile::WriteOnly|QFile::Text);
				QTextStream stream(&fout);
				stream << output;
			}

	}
}

QList<VipShape> VipSceneModelEditor::openShapes(const QString & filename, VipPlayer2D* pl, bool remove_old)
{
	if (!pl)
		return QList<VipShape>();

	if (filename.isEmpty())
		return QList<VipShape>();
	
	QList<VipShape> res;
	QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(filename, QByteArray(), QVariant::fromValue(VipSceneModel()));
	VipIODevice * dev = VipCreateDevice::create(devices, filename);
	if (dev && dev->open(VipIODevice::ReadOnly))
	{
		if (dev->deviceType() == VipIODevice::Resource)
		{
			VipSceneModelList lst;
			VipAnyData any = dev->outputAt(0)->data();
			if (any.data().userType() == qMetaTypeId<VipSceneModel>()) lst << any.value<VipSceneModel>();
			else lst = any.value<VipSceneModelList>();
			if (lst.size()) {
				for (int i = 0; i < lst.size(); ++i) {
					res += lst[i].shapes();
					lst[i] = vipCopyVideoSceneModel(lst[i], NULL, qobject_cast<VipVideoPlayer*>(pl));
				}
				pl->addSceneModels(lst, remove_old);
			}
			delete dev;
		}
		else
		{
			dev->setParent(pl->processingPool());
			vipCreatePlayersFromProcessing(dev,pl);
		}
		return res;
	}
	return res;
}

void VipSceneModelEditor::openShapes()
{
	if (!m_data->player2D)
		return;

	
	QStringList filters = VipIODevice::possibleReadFilters(QString(), QByteArray(), QVariant::fromValue(VipSceneModel()));
	QString filename = VipFileDialog::getOpenFileName(NULL, "Load shapes", filters.join(";;"));
	openShapes(filename, m_data->player2D);
}

void VipSceneModelEditor::selectUnselectAll()
{
	if (!m_data->player2D)
		return;

	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 2, 1);
	bool all_selected = true;
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (!shapes[i]->isSelected())
		{
			all_selected = false;
			break;
		}
	}
	for (int i = 0; i < shapes.size(); ++i)
	{
		shapes[i]->setSelected(!all_selected);
		if (VipResizeItemPtr resize = shapes[i]->property("VipResizeItem").value<VipResizeItemPtr>())
			resize->setSelected(!all_selected);
	}
}

void VipSceneModelEditor::deleteSelected()
{
	if (!m_data->player2D)
		return;

	QList<VipPlotShape*> shapes = m_data->player2D->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	QList<VipPlotSceneModel*> models = m_data->player2D->plotSceneModels();
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (VipPlotSceneModel * sm = shapes[i]->property("VipPlotSceneModel").value<VipPlotSceneModel*>()) {
			if(models.indexOf(sm) >= 0 || shapes[i]->testItemAttribute(VipPlotItem::IsSuppressable))
				sm->sceneModel().remove(shapes[i]->rawData());
		}
	}
}




ShapeToolBar::ShapeToolBar(VipSceneModelWidgetPlayer * tool)
		:VipToolWidgetToolBar(tool)
	{
		setWindowTitle("Edit ROI tool bar");
		setObjectName("Edit ROI tool bar");
		addShape = new QToolButton();
		addMenu = new QMenu(this);
		connect(addMenu->addAction(vipIcon("rectangle.png"), "Rectangle"), SIGNAL(triggered(bool)), tool, SLOT(addRect()));
		connect(addMenu->addAction(vipIcon("ellipse.png"), "Ellipse"), SIGNAL(triggered(bool)), tool, SLOT(addEllipse()));
		connect(addMenu->addAction(vipIcon("polygon.png"), "Polygon"), SIGNAL(triggered(bool)), tool, SLOT(addPolygon()));
		connect(addMenu->addAction(vipIcon("free_roi.png"), "Free region"), SIGNAL(triggered(bool)), tool, SLOT(addMask()));
		connect(addMenu->addAction(vipIcon("polyline.png"), "Polyline"), SIGNAL(triggered(bool)), tool, SLOT(addPolyline()));
		connect(addMenu->addAction(vipIcon("pdc.png"), "Point"), SIGNAL(triggered(bool)), tool, SLOT(addPixel()));
		addShape->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		addShape->setIconSize(QSize(18, 18));
		addShape->setPopupMode(QToolButton::MenuButtonPopup);
		addShape->setMenu(addMenu);
		addShape->setText("Add shape: Rectangle");
		addShape->setToolTip("Draw selected shape on a player");
		//m_addShape->setMaximumWidth(150);
		addShape->setCheckable(true);
		addShape->setIcon(vipIcon("rectangle.png"));
		addMenu->hide();
		connect(addShape, SIGNAL(clicked(bool)), tool, SLOT(addShapeClicked(bool)));
		this->addWidget(addShape);
		
	}


VipSceneModelWidgetPlayer::VipSceneModelWidgetPlayer(VipMainWindow * window)
:VipToolWidgetPlayer(window),m_toolBar(NULL)
{
	setWindowTitle("Edit ROI");
	setObjectName("Edit ROI");
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	m_editor = new VipSceneModelEditor();
	m_addShape = new QToolButton();
	m_addMenu = new QMenu(m_addShape);
	m_last_shape = &VipSceneModelWidgetPlayer::addRect;

	QVBoxLayout * lay = new QVBoxLayout();

	lay->addWidget(m_addShape);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(m_editor);
	lay->addStretch(1);
	//lay->setContentsMargins(0,0,0,0);
	QWidget * w = new QWidget();
	w->setLayout(lay);
	this->setWidget(w);

	connect(m_addMenu->addAction(vipIcon("rectangle.png"),"Rectangle"),SIGNAL(triggered ( bool )),this,SLOT(addRect()));
	connect(m_addMenu->addAction(vipIcon("ellipse.png"),"Ellipse"),SIGNAL(triggered ( bool )),this,SLOT(addEllipse()));
	connect(m_addMenu->addAction(vipIcon("polygon.png"),"Polygon"),SIGNAL(triggered ( bool )),this,SLOT(addPolygon()));
	connect(m_addMenu->addAction(vipIcon("free_roi.png"),"Free region"),SIGNAL(triggered ( bool )),this,SLOT(addMask()));
	connect(m_addMenu->addAction(vipIcon("polyline.png"),"Polyline"),SIGNAL(triggered ( bool )),this,SLOT(addPolyline()));
	connect(m_addMenu->addAction(vipIcon("pdc.png"),"Point"),SIGNAL(triggered ( bool )),this,SLOT(addPixel()));
	m_addShape->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_addShape->setIconSize(QSize(18,18));
	m_addShape->setPopupMode ( QToolButton::MenuButtonPopup );
	m_addShape->setMenu(m_addMenu);
	m_addShape->setText("Add shape: Rectangle");
	m_addShape->setToolTip("Click to draw the selected shape type");
	//m_addShape->setMaximumWidth(150);
	m_addShape->setCheckable(true);
	m_addShape->setIcon(vipIcon("rectangle.png"));
	m_addMenu->hide();

	m_draw = new VipDrawShapeRect((VipPlotPlayer*)NULL,"ROI");
	m_draw->hide();
	connect(m_draw, SIGNAL(finished()), this, SLOT(stopAddingShape()));
	connect(m_addShape,SIGNAL(clicked(bool)),this,SLOT(addShapeClicked(bool)));
}

void VipSceneModelWidgetPlayer::addRect()
{
	if(m_area && m_player)
	{
		/*if(!m_plot)
		{
			QList<VipAbstractScale*>  axes;
			VipCoordinateSystem::Type type = m_area->standardScales(axes);
			m_plot = new VipPlotSceneModel();
			m_plot->setAxes(axes,type);
		}
		*/
		m_addShape->setText("Add shape: Rectangle");
		m_addShape->setIcon(vipIcon("rectangle.png"));
		m_addShape->setChecked(true);

		if (toolBar())
		{
			toolBar()->addShape->setText("Add shape: Rectangle");
			toolBar()->addShape->setIcon(vipIcon("rectangle.png"));
			toolBar()->addShape->setChecked(true);
		}

		if(m_draw)
			delete m_draw.data();
		if(VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw = new VipDrawShapeRect(pl, "ROI");
		else
			m_draw = new VipDrawShapeRect(m_player->plotSceneModel(),"ROI");
		m_area->installFilter(m_draw);

		connect(m_draw,SIGNAL(finished()),this,SLOT(stopAddingShape()));

		m_addShape->blockSignals(true);
		m_addShape->setChecked(true);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(true);
			toolBar()->addShape->blockSignals(false);
		}
	}
	else
	{
		m_addShape->blockSignals(true);
		m_addShape->setChecked(false);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(false);
			toolBar()->addShape->blockSignals(false);
		}
	}
	m_last_shape = &VipSceneModelWidgetPlayer::addRect;
}

void VipSceneModelWidgetPlayer::addEllipse()
{
	if(m_area && m_player)
	{
		/*if(!m_plot)
		{
			QList<VipAbstractScale*>  axes;
			VipCoordinateSystem::Type type = m_area->standardScales(axes);
			m_plot = new VipPlotSceneModel();
			m_plot->setAxes(axes,type);
		}*/

		m_addShape->setText("Add shape: Ellipse");
		m_addShape->setIcon(vipIcon("ellipse.png"));
		m_addShape->setChecked(true);
		if (toolBar())
		{
			toolBar()->addShape->setText("Add shape: Ellipse");
			toolBar()->addShape->setIcon(vipIcon("ellipse.png"));
			toolBar()->addShape->setChecked(true);
		}

		if(m_draw)
			delete m_draw.data();
		if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw = new VipDrawShapeEllipse(pl, "ROI");
		else
			m_draw = new VipDrawShapeEllipse(m_player->plotSceneModel(), "ROI");
		//m_draw = new VipDrawShapeEllipse(m_plot->sceneModel(),"ROI");
		m_area->installFilter(m_draw);

		connect(m_draw,SIGNAL(finished()),this,SLOT(stopAddingShape()));

		m_addShape->blockSignals(true);
		m_addShape->setChecked(true);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(true);
			toolBar()->addShape->blockSignals(false);
		}
	}
	else
	{
		m_addShape->blockSignals(true);
		m_addShape->setChecked(false);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(false);
			toolBar()->addShape->blockSignals(false);
		}
	}
	m_last_shape = &VipSceneModelWidgetPlayer::addEllipse;
}


void VipSceneModelWidgetPlayer::addPolygon()
{
	if(m_area && m_player)
	{
		/*if(!m_plot)
		{
			QList<VipAbstractScale*>  axes;
			VipCoordinateSystem::Type type = m_area->standardScales(axes);
			m_plot = new VipPlotSceneModel();
			m_plot->setAxes(axes,type);
		}*/

		m_addShape->setText("Add shape: Polygon");
		m_addShape->setIcon(vipIcon("polygon.png"));
		m_addShape->setChecked(true);
		if (toolBar())
		{
			toolBar()->addShape->setText("Add shape: Polygon");
			toolBar()->addShape->setIcon(vipIcon("polygon.png"));
			toolBar()->addShape->setChecked(true);
		}

		if(m_draw)
			delete m_draw.data();
		if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw = new VipDrawShapePolygon(pl, "ROI");
		else
			m_draw = new VipDrawShapePolygon(m_player->plotSceneModel(), "ROI");
		//m_draw = new VipDrawShapePolygon(m_plot->sceneModel(),"ROI");
		m_area->installFilter(m_draw);

		connect(m_draw,SIGNAL(finished()),this,SLOT(stopAddingShape()));

		m_addShape->blockSignals(true);
		m_addShape->setChecked(true);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(true);
			toolBar()->addShape->blockSignals(false);
		}
	}
	else
	{
		m_addShape->blockSignals(true);
		m_addShape->setChecked(false);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(false);
			toolBar()->addShape->blockSignals(false);
		}
	}
	m_last_shape = &VipSceneModelWidgetPlayer::addPolygon;
}

void VipSceneModelWidgetPlayer::addPolyline()
{
	if(m_area && m_player)
	{
		/*if(!m_plot)
		{
			QList<VipAbstractScale*>  axes;
			VipCoordinateSystem::Type type = m_area->standardScales(axes);
			m_plot = new VipPlotSceneModel();
			m_plot->setAxes(axes,type);
		}*/

		m_addShape->setText("Add shape: Polyline");
		m_addShape->setIcon(vipIcon("polyline.png"));
		m_addShape->setChecked(true);
		if (toolBar())
		{
			toolBar()->addShape->setText("Add shape: Polyline");
			toolBar()->addShape->setIcon(vipIcon("polyline.png"));
			toolBar()->addShape->setChecked(true);
		}

		if(m_draw)
			delete m_draw.data();
		if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw = new VipDrawShapePolyline(pl, "Polylines");
		else
			m_draw = new VipDrawShapePolyline(m_player->plotSceneModel(), "Polylines");
		//m_draw = new VipDrawShapePolyline(m_plot->sceneModel(),"Polylines");
		m_area->installFilter(m_draw);

		connect(m_draw,SIGNAL(finished()),this,SLOT(stopAddingShape()));

		m_addShape->blockSignals(true);
		m_addShape->setChecked(true);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(true);
			toolBar()->addShape->blockSignals(false);
		}
	
	}
	else
	{
		m_addShape->blockSignals(true);
		m_addShape->setChecked(false);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(false);
			toolBar()->addShape->blockSignals(false);
		}
	}
	m_last_shape = &VipSceneModelWidgetPlayer::addPolyline;
}

void VipSceneModelWidgetPlayer::addMask()
{
	if(m_area && m_player)
	{
		m_addShape->setText("Add shape: Free region");
		m_addShape->setIcon(vipIcon("free_roi.png"));
		m_addShape->setChecked(true);
		if (toolBar())
		{
			toolBar()->addShape->setText("Add shape: Free region");
			toolBar()->addShape->setIcon(vipIcon("free_roi.png"));
			toolBar()->addShape->setChecked(true);
		}

		if(m_draw)
			delete m_draw.data();
		if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw = new VipDrawShapeMask(pl, "ROI");
		else
			m_draw = new VipDrawShapeMask(m_player->plotSceneModel(), "ROI");
		//m_draw = new VipDrawShapeMask(m_plot->sceneModel(),"ROI");
		m_area->installFilter(m_draw);

		connect(m_draw,SIGNAL(finished()),this,SLOT(stopAddingShape()));

		m_addShape->blockSignals(true);
		m_addShape->setChecked(true);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(true);
			toolBar()->addShape->blockSignals(false);
		}
	}
	else
	{
		m_addShape->blockSignals(true);
		m_addShape->setChecked(false);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(false);
			toolBar()->addShape->blockSignals(false);
		}
	}
	m_last_shape = &VipSceneModelWidgetPlayer::addMask;
}


void VipSceneModelWidgetPlayer::addPixel()
{
	if (m_area && m_player)
	{
		m_addShape->setText("Add shape: Point");
		m_addShape->setIcon(vipIcon("pdc.png"));
		m_addShape->setChecked(true);
		if (toolBar())
		{
			toolBar()->addShape->setText("Add shape: Point");
			toolBar()->addShape->setIcon(vipIcon("pdc.png"));
			toolBar()->addShape->setChecked(true);
		}

		if (m_draw)
			delete m_draw.data();
		if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw = new VipDrawShapePoint(pl, "Points");
		else
			m_draw = new VipDrawShapePoint(m_player->plotSceneModel(), "Points");

		m_area->installFilter(m_draw);
		connect(m_draw, SIGNAL(finished()), this, SLOT(stopAddingShape()));

		m_addShape->blockSignals(true);
		m_addShape->setChecked(true);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(true);
			toolBar()->addShape->blockSignals(false);
		}
	}
	else
	{
		m_addShape->blockSignals(true);
		m_addShape->setChecked(false);
		m_addShape->blockSignals(false);
		if (toolBar())
		{
			toolBar()->addShape->blockSignals(true);
			toolBar()->addShape->setChecked(false);
			toolBar()->addShape->blockSignals(false);
		}
	}
	m_last_shape = &VipSceneModelWidgetPlayer::addPixel;
}

void VipSceneModelWidgetPlayer::stopAddingShape()
{
	if(m_draw && m_area)
		m_area->removeFilter();

	if(m_draw)
		m_draw->hide();

	m_addShape->blockSignals(true);
	m_addShape->setChecked(false);
	m_addShape->blockSignals(false);
	if (toolBar())
	{
		toolBar()->addShape->blockSignals(true);
		toolBar()->addShape->setChecked(false);
		toolBar()->addShape->blockSignals(false);
	}

	Q_EMIT stopShape();
}

void VipSceneModelWidgetPlayer::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Z && (evt->modifiers() & Qt::CTRL)) {
		evt->accept();
		VipSceneModelState::instance()->undo();
		return;
	}
	else if (evt->key() == Qt::Key_Y && (evt->modifiers() & Qt::CTRL)) {
		evt->accept();
		VipSceneModelState::instance()->redo();
		return;
	}
	evt->ignore();
}

void VipSceneModelWidgetPlayer::addShapeClicked(bool checked)
{
	if(!checked)
		stopAddingShape();
	/*else if(m_draw && m_plot)
	{
		m_draw->reset(m_plot->sceneModel(),m_draw->group());
		m_draw->show();
		if(m_area)
			m_area->installFilter(m_draw);
	}*/
	else if (m_draw && m_player)
	{
		if(VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(m_player))
			m_draw->reset(pl, m_draw->group());
		else
			m_draw->reset(m_player->plotSceneModel(), m_draw->group());
		m_draw->show();
		if (m_area)
			m_area->installFilter(m_draw);
	}
	else if (checked &&/* m_plot &&*/ m_last_shape)
	{
		((*this).*m_last_shape)();
	}
}

/*VipPlotSceneModel * VipSceneModelWidgetPlayer::drawSceneModelForPlayer(VipAbstractPlayer* pl)
{
	VipPlotSceneModel * model = NULL;
	if (VipPlayer2D* pl2D = qobject_cast<VipPlayer2D*>(pl)) {
		//returns the VipPlayer2D internal VipPlotSceneModel
		model = pl2D->plotSceneModel();
	}
	
	if (!model) {
		//find the first VipPlotSceneModel with selected shapes (or the first one if there are no selected shapes) and set it to the scene model editor.
		QList<VipPlotSceneModel*> models = pl->plotWidget2D()->area()->findItems<VipPlotSceneModel*>();
		for (int i = 0; i < models.size(); ++i){
			if (!model) model = models[i];
			if (models[i]->shapes(1).size()) {
				model = models[i];
				break;
			}
		}
	}
	return model;
}*/

bool VipSceneModelWidgetPlayer::setPlayer(VipAbstractPlayer * pl)
{
	if(!pl)
		return false;
	if (!qobject_cast<VipPlayer2D*>(pl))
		return false;

	if (qobject_cast<VipPlayer2D*>(m_player)) {
		disconnect(m_player, SIGNAL(sceneModelAdded(VipPlotSceneModel*)), this, SLOT(resetPlayer()));
		disconnect(m_player, SIGNAL(sceneModelRemoved(VipPlotSceneModel*)), this, SLOT(resetPlayer()));
		disconnect(m_player, SIGNAL(sceneModelGroupsChanged(VipPlotSceneModel*)), this, SLOT(resetPlayer()));
	}
	m_player = qobject_cast<VipPlayer2D*>(pl);
	if (qobject_cast<VipPlayer2D*>(m_player)) {
		connect(m_player, SIGNAL(sceneModelAdded(VipPlotSceneModel*)), this, SLOT(resetPlayer()));
		connect(m_player, SIGNAL(sceneModelRemoved(VipPlotSceneModel*)), this, SLOT(resetPlayer()));
		connect(m_player, SIGNAL(sceneModelGroupsChanged(VipPlotSceneModel*)), this, SLOT(resetPlayer()));
	}

	m_editor->setPlayer(m_player);
	 
	//then, update the drawing filter on the first scene model, which is usually the one we draw on
	//if(m_plot = drawSceneModelForPlayer(pl)){
		bool filter_installed = false;
		if(m_area && m_area->filter())
			filter_installed = true;

		m_area = pl->plotWidget2D()->area();
		if(m_draw && m_area && filter_installed && m_addShape->isChecked()) {
			m_area->installFilter(m_draw);
			m_draw->resetPlayer(/*m_plot->sceneModel()*/m_player, m_draw->group());
			m_draw->show();
		}
	//}
	return true;
}

void VipSceneModelWidgetPlayer::resetPlayer()
{
	if(isVisible())
		setPlayer(m_player);
}

VipShapeButton * VipSceneModelWidgetPlayer::createPlayerButton(VipAbstractPlayer * pl)
{
	return new VipShapeButton(pl->plotWidget2D(), NULL);
}

ShapeToolBar * VipSceneModelWidgetPlayer::toolBar()
{
	//For now, disable the tool bar shortcut (more confusing than anything)
	return NULL;
	/*if (!m_toolBar)
	{
		m_toolBar = new ShapeToolBar(this);
		m_toolBar->setIconSize(QSize(18, 18));
		m_toolBar->addShape->setToolTip("<b>Shortcup:</b> draw selected shape on a player.<br><br>To see all <i>Regions Of Interest</i> features, click on the left icon.");
	}
	return m_toolBar;*/
}


VipSceneModelWidgetPlayer * vipGetSceneModelWidgetPlayer(VipMainWindow * window)
{
	static VipSceneModelWidgetPlayer * win = new VipSceneModelWidgetPlayer(window);
	return win;
}



/*
* 
* 
* Undo/redo functions/classes for Regions of Interest edition.
* 
* 
*/

/**
* Undo/redo stacks are stored per couple VipPlayer2D/VipPlotSceneModel
*/
struct SMStateKey
{
	VipPlayer2D* player;
	VipPlotSceneModel* sm;
	SMStateKey(VipPlayer2D* p = NULL, VipPlotSceneModel* s = NULL)
		:player(p), sm(s) {}

	bool operator<(const SMStateKey& other) const {
		if (player < other.player)
			return true;
		if (player > other.player)
			return false;
		return sm < other.sm;
	}
};
/**
* A state in the un/redo stacks
*/
struct SMState
{
	QPointer<VipPlayer2D> player;
	QPointer<VipPlotSceneModel> sm;
	QByteArray state;
	SMState(VipPlayer2D* p = NULL, VipPlotSceneModel* s = NULL, const QByteArray& ar = QByteArray())
		:player(p), sm(s), state(ar) {}
};



class VipSceneModelState::PrivateData
{
public:
	QMap< SMStateKey, QList< SMState> > _undoStates;
	QMap< SMStateKey, QList< SMState> > _redoStates;

};

VipSceneModelState::VipSceneModelState()
{
	m_data = new PrivateData();
}
VipSceneModelState::~VipSceneModelState()
{
	delete m_data;
}
	
VipSceneModelState* VipSceneModelState::instance()
{
	static VipSceneModelState* inst = new VipSceneModelState();
	return inst;
}


QByteArray VipSceneModelState::saveState(VipPlotSceneModel* sm)
{
	VipXOStringArchive arch;
	QList<VipPlotShape*> shapes = sm->shapes();
	for (int i = 0; i < shapes.size(); ++i) {
		if (shapes[i]->isSelected())
			shapes[i]->rawData().setAttribute("_vip_selected", true);
		else
			shapes[i]->rawData().setAttribute("_vip_selected", false);
		if (shapes[i]->isVisible())
			shapes[i]->rawData().setAttribute("_vip_visible", true);
		else
			shapes[i]->rawData().setAttribute("_vip_visible", false);
	}
	arch.content(sm->sceneModel());
	return arch.toString().toLatin1();
}

bool  VipSceneModelState::restoreState(VipPlotSceneModel* psm, const QByteArray& ar)
{
	VipXIStringArchive arch(ar);

	VipSceneModel sm;
	if (!arch.content(sm))
		return false;

	psm->sceneModel().clear();
	psm->sceneModel().add(sm);
	//TEST: comment this as it should be automatically called
	//psm->resetSceneModel();
	QList<VipPlotShape*> shapes = psm->shapes();
	for (int i = 0; i < shapes.size(); ++i) {
		if (shapes[i]->rawData().attribute("_vip_selected").toBool())
			shapes[i]->setSelected(true);
		else
			shapes[i]->setSelected(false);
		if (shapes[i]->rawData().attribute("_vip_visible").toBool())
			shapes[i]->setVisible(true);
		else
			shapes[i]->setVisible(false);
	}
	return true;
}


QPair<VipPlayer2D*, VipPlotSceneModel*> VipSceneModelState::currentSceneModel() const
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		if (VipDragWidget* w = area->dragWidgetHandler()->focusWidget())
			if (VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget())) {
				QList<VipPlotSceneModel*> models = pl->plotSceneModels();
				VipPlotSceneModel* found = pl->plotSceneModel();
				if (models.size() >  1 && pl->plotWidget2D()) {
					//If we have the choice between several scene models, try to find the most recent one based on last pressed item
					if (VipPlotItem* last = pl->plotWidget2D()->area()->lastPressed()) {
						//get VipPlotSceneModel sharing the same axes
						for (int i = 0; i < models.size(); ++i)
							if (models[i]->axes() == last->axes()) {
								found = models[i];
								break;
							}
					}
				}
				if (found) {
					return QPair<VipPlayer2D*, VipPlotSceneModel*>(pl,found);
				}
			}
	return QPair<VipPlayer2D*, VipPlotSceneModel*>(NULL,NULL);
}
	
void VipSceneModelState::undo()
{
	QPair<VipPlayer2D*, VipPlotSceneModel*> sm = currentSceneModel();
	if(sm.first)
		undo(sm.first, sm.second);
}
	
void VipSceneModelState::redo()
{
	QPair<VipPlayer2D*, VipPlotSceneModel*> sm = currentSceneModel();
	if (sm.first)
		redo(sm.first, sm.second);
}

void VipSceneModelState::connectSceneModel(VipPlotSceneModel* sm)
{
	disconnectSceneModel(sm);
	connect(sm, SIGNAL(aboutToMove(VipResizeItem*)), this, SLOT(receivedAboutToChange()), Qt::DirectConnection);
	connect(sm, SIGNAL(aboutToResize(VipResizeItem*)), this, SLOT(receivedAboutToChange()), Qt::DirectConnection);
	connect(sm, SIGNAL(aboutToRotate(VipResizeItem*)), this, SLOT(receivedAboutToChange()), Qt::DirectConnection);
	connect(sm, SIGNAL(aboutToChangePoints(VipResizeItem*)), this, SLOT(receivedAboutToChange()), Qt::DirectConnection);
	connect(sm, SIGNAL(aboutToDelete(VipResizeItem*)), this, SLOT(receivedAboutToChange()), Qt::DirectConnection);
}
void VipSceneModelState::disconnectSceneModel(VipPlotSceneModel* sm)
{
	disconnect(sm, SIGNAL(aboutToMove(VipResizeItem*)), this, SLOT(receivedAboutToChange()));
	disconnect(sm, SIGNAL(aboutToResize(VipResizeItem*)), this, SLOT(receivedAboutToChange()));
	disconnect(sm, SIGNAL(aboutToRotate(VipResizeItem*)), this, SLOT(receivedAboutToChange()));
	disconnect(sm, SIGNAL(aboutToChangePoints(VipResizeItem*)), this, SLOT(receivedAboutToChange()));
	disconnect(sm, SIGNAL(aboutToDelete(VipResizeItem*)), this, SLOT(receivedAboutToChange()));
}

void VipSceneModelState::receivedAboutToChange()
{
	/*
	Received on of the following from a VipPlotSceneModel:
	void aboutToMove(VipResizeItem*);
	void aboutToResize(VipResizeItem*);
	void aboutToRotate(VipResizeItem*);
	void aboutToChangePoints(VipResizeItem*);
	*/

	if (VipPlotSceneModel* sm = qobject_cast<VipPlotSceneModel*>(sender())) {
		//find parent VipPlayer2D
		if (VipPlayer2D* player = qobject_cast<VipPlayer2D*>(VipAbstractPlayer::findAbstractPlayer(sm))) {

			//push state
			pushState(player, sm);
		}
	}
}


void VipSceneModelState::cleanStates()
{
	//Remove deleted players/scene models

	QList< QMap< SMStateKey, QList< SMState> >::iterator > to_remove;
	for (QMap< SMStateKey, QList< SMState> >::iterator it = m_data->_undoStates.begin(); it != m_data->_undoStates.end(); ++it)
		if (!it.value().size() || ((!it.value().first().player || !it.value().first().sm)))
			to_remove.append(it);
	for (int i = 0; i < to_remove.size(); ++i)
		m_data->_undoStates.erase(to_remove[i]);

	to_remove.clear();
	for (QMap< SMStateKey, QList< SMState> >::iterator it = m_data->_redoStates.begin(); it != m_data->_redoStates.end(); ++it)
		if (!it.value().size() || ((!it.value().first().player || !it.value().first().sm)))
			to_remove.append(it);
	for (int i = 0; i < to_remove.size(); ++i)
		m_data->_redoStates.erase(to_remove[i]);
}

void VipSceneModelState::pushState(VipPlayer2D* player, VipPlotSceneModel* sm, const QByteArray& ar )
{
	cleanStates();

	if (!player || !sm)
		return;

	QByteArray state = ar;
	if (state.isEmpty())
		state = saveState(sm);
	QList< SMState>& states = m_data->_undoStates[SMStateKey(player, sm)];
	states.append(SMState(player, sm, state));
	if (states.size() > 50)
		states.pop_front();

	//clear redo stack
	m_data->_redoStates[SMStateKey(player, sm)].clear();
}

void VipSceneModelState::undo(VipPlayer2D* player, VipPlotSceneModel* sm)
{
	cleanStates();

	//push current state to the redo stack
	QList< SMState>& redo_states = m_data->_redoStates[SMStateKey(player, sm)];
	redo_states.append(SMState(player,sm,saveState(sm)));
	if (redo_states.size() > 50)
		redo_states.pop_front();

	//undo
	QList< SMState>& undo_states = m_data->_undoStates[SMStateKey(player, sm)];
	if (undo_states.size() > 0) {
		SMState last = undo_states.last();
		//restoreState(ar);
		restoreState(last.sm, last.state);
		undo_states.pop_back();

		Q_EMIT undoDone();
	}
}
void VipSceneModelState::redo(VipPlayer2D* player, VipPlotSceneModel* sm)
{
	cleanStates();

	//redo
	QList< SMState>& redo_states = m_data->_redoStates[SMStateKey(player, sm)];
	if (redo_states.size() > 0) {

		//push current state to undo stack
		QList< SMState>& undo_states = m_data->_undoStates[SMStateKey(player, sm)];
		undo_states.append(SMState(player, sm, saveState(sm)));
		if (undo_states.size() > 50)
			undo_states.pop_front();

		SMState state = redo_states.last();
		restoreState(state.sm, state.state);
		redo_states.pop_back();

		Q_EMIT redoDone();
	}
}



static VipSceneModelEditor * editSceneModel(VipPlotShape* shape)
{
	if(VipSceneModel  model = shape->rawData().parent())
	{
		//find the parent VipPlotSceneModel object
		QList<VipPlotSceneModel*> plot_models = vipCastItemList<VipPlotSceneModel*>(shape->linkedItems(),QString(),2,1);
		VipPlotSceneModel * found = NULL;
		for(int i=0; i < plot_models.size(); ++i)
			if(plot_models[i]->indexOf(shape) >= 0)
			{
				found = plot_models[i];
				break;
			}
		if (found) {
			if(VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(VipAbstractPlayer::findAbstractPlayer(found)))
			{
				VipSceneModelEditor * editor = new VipSceneModelEditor();
				editor->setPlayer(pl);
				return editor;
			}
		}
	}

	return NULL;
}

static QWidget * editResizeItem(VipResizeItem* item)
{
	PlotItemList lst = item->managedItems();
	for(int i=0; i < lst.size(); ++i)
	{
		if(lst[i]->isSelected() && vipHasObjectEditor(QVariant::fromValue(lst[i])))
		{
			return vipObjectEditor(QVariant::fromValue(lst[i]));
		}
	}
	return NULL;
}


static int registerSceneModelEditor()
{
	vipFDObjectEditor().append<VipSceneModelEditor *(VipPlotShape*)>(editSceneModel);
	vipFDObjectEditor().append<QWidget *(VipResizeItem*)>(editResizeItem);
	return 0;
}

static int _registerSceneModelEditor = vipAddInitializationFunction(registerSceneModelEditor);
