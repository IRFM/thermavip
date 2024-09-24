#include "VipDisplayVTKObject.h"
#include "VipVTKGraphicsView.h"
#include "VipVTKActorParameters.h"

#include "VipPainter.h"
#include "VipPlotGrid.h"

#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkDoubleArray.h>
#include <vtkCompositeDataSet.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkDataSetMapper.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDoubleArray.h>
#include <vtkGraphMapper.h>
#include <vtkHierarchicalBoxDataSet.h>
#include <vtkMath.h>
#include <vtkPointData.h>
#include <vtkRectilinearGrid.h>
#include <vtkStructuredGrid.h>
#include <vtkStructuredPoints.h>
#include <vtkUnstructuredGrid.h>
#include <vtkGraph.h>


#include <QGraphicsSceneMouseEvent>
#include <QLinearGradient>

class VipPlotVTKObject::PrivateData
{
public:
	bool hover{ false };
	QPoint mouse{ QPoint(-1, -1) };
	QColor selectedColor{ QColor::fromRgb(230, 230, 230) };
	QColor color{ QColor::fromRgb(230, 230, 230) };
	QColor edgeColor;
	double highlightColor[3] = { -1, -1, -1 };
	bool selected{ false };
	bool visible{ true };
	bool edgeVisible{ false };
	double opacity{ 1. };
	int layer{ 0 };

	QPointer<VipPlotItem> syncSelect;

	vtkSmartPointer<vtkMapper> mapper;
	vtkSmartPointer<vtkActor> actor;

	//If the plot item does not have a valid VipVTKObject, it won't be added to the VipVTKGraphicsView.
	//Therefore, we store the output VipVTKGraphicsView object to add later the VipPlotVTKObject when it has a valid VipVTKObject.
	QPointer<VipVTKGraphicsView> pendingView;
	std::unique_ptr<VipVTKObject> pendingAttributes;
};

VipPlotVTKObject::VipPlotVTKObject(const VipText & title)
	:VipPlotItemDataType(title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	this->setItemAttribute(VipPlotItem::ColorMapAutoScale);
	//NEWPLOT
	//this->setItemAttribute(VipPlotItem::InstantColorMapUpdate);
	this->setItemAttribute(VipPlotItem::HasLegendIcon);
	//this->setItemAttribute(VipPlotItem::RenderInPixmap);
	this->setItemAttribute(VipPlotItem::HasToolTip, false);
	this->setRenderHints(QPainter::Antialiasing);
	this->setAcceptHoverEvents(true);

	connect(this, SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(receiveVisibilityChanged(VipPlotItem*)));
	connect(this, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(receiveSelectionChanged(VipPlotItem*)));
}

VipPlotVTKObject::~VipPlotVTKObject()
{
	VipVTKGraphicsView* _old = qobject_cast<VipVTKGraphicsView*>(this->view());
	if (_old) {
		if (d_data->actor)
			_old->renderers()[d_data->layer]->RemoveActor(d_data->actor);
		_old->contours()->remove(this);
	}
	// Remove actor from previous renderer
	if (d_data->actor && d_data->actor->GetNumberOfConsumers()) {
		for (int i = 0; i < d_data->actor->GetNumberOfConsumers(); ++i)
			if (vtkObject* c = d_data->actor->GetConsumer(i)) {
				if (c->IsA("vtkRenderer"))
					static_cast<vtkRenderer*>(c)->RemoveActor(d_data->actor);
			}
	}
	
}

QString VipPlotVTKObject::dataName() const {

	/* if (VipDisplayObject* obj = this->property("VipDisplayObject").value<VipDisplayObject*>()) {
		return obj->inputAt(0)->probe().name();
	}*/
	return rawData().dataName();
}

bool VipPlotVTKObject::hasActor() const 
{
	return d_data->actor.GetPointer() != nullptr;
}
vtkSmartPointer<vtkMapper> VipPlotVTKObject::mapper() const
{
	const_cast<VipPlotVTKObject*>(this)->buildMapperAndActor(rawData());
	return d_data->mapper;
}
vtkSmartPointer<vtkActor> VipPlotVTKObject::actor() const
{
	const_cast<VipPlotVTKObject*>(this)->buildMapperAndActor(rawData());
	return d_data->actor;
}

void VipPlotVTKObject::range(double* range, int component) const
{
	auto data = rawData();
	auto lock = vipLockVTKObjects(data);


	range[0] = range[1] = vtkMath::Nan();

	vtkDataArray* array = nullptr;
	if (vtkDataSet* set = data.dataSet()) {
		if (d_data->mapper && d_data->mapper->GetScalarMode() == VTK_SCALAR_MODE_USE_POINT_DATA)
			array = set->GetPointData()->GetScalars();
		else if (d_data->mapper && d_data->mapper->GetScalarMode() == VTK_SCALAR_MODE_USE_CELL_DATA)
			array = set->GetCellData()->GetScalars();
	}

	if (!array)
		return;

	// For now, do not use the custom range function

	if (component < 0)
		array->GetRange(range);
	else
		array->GetRange(range, component);

	return;

	/* bool recompute = d_data->attributeModified;
	if (!recompute)
		recompute = (d_data->attributeModifiedTime < array->GetMTime());

	if (recompute) {
		d_data->attributeModified = false;
		d_data->attributeModifiedTime = array->GetMTime();

		d_data->ranges.resize(array->GetNumberOfComponents());
		for (int i = 0; i < d_data->ranges.size(); ++i)
			d_data->ranges[i] = QPair<double, double>(std::numeric_limits<double>::max(), -std::numeric_limits<double>::max());

		for (int i = 0; i < array->GetNumberOfTuples(); ++i)
			for (int c = 0; c < array->GetNumberOfComponents(); ++c) {
				double val = array->GetComponent(i, c);
				if (!vtkMath::IsNan(val)) {
					d_data->ranges[c].first = qMin(d_data->ranges[c].first, array->GetComponent(i, c));
					d_data->ranges[c].second = qMax(d_data->ranges[c].second, array->GetComponent(i, c));
				}
			}
	}

	if (component < 0) {
		if (d_data->ranges.size()) {
			range[0] = d_data->ranges[0].first;
			range[1] = d_data->ranges[0].second;

			for (int i = 1; i < d_data->ranges.size(); ++i) {
				range[0] = qMin(d_data->ranges[i].first, range[0]);
				range[1] = qMax(d_data->ranges[i].second, range[1]);
			}
		}
	}
	else if (component < d_data->ranges.size()) {
		range[0] = d_data->ranges[component].first;
		range[1] = d_data->ranges[component].second;
	}*/
}


void VipPlotVTKObject::bounds(double bounds[6]) const
{
	auto lock = vipLockVTKObjects(rawData());
	if (d_data->actor)
		d_data->actor->GetBounds(bounds);
}


VipInterval VipPlotVTKObject::plotInterval(const VipInterval & ) const
{
	VipVTKObject dat = data().value<VipVTKObject>();
	if (dat)
	{
		double rn[2];
		int comp = -1;
		if(vtkMapper * m = d_data->mapper)
			comp = m->GetArrayId();
		
		this->range(rn, comp);
		if(vtkMath::IsNan(rn[0]))
			return  VipInterval();
		else
			return  VipInterval(rn[0],rn[1]);
	}
	return VipInterval();
}

QPainterPath VipPlotVTKObject::shape() const
{
	if (VipVTKGraphicsView * v = qobject_cast<VipVTKGraphicsView*>(this->view()))
	{
		VipVTKObject dat = data().value<VipVTKObject>();
		return  v->contours()->Shape(this);
	}
	return QPainterPath();
}

QRectF VipPlotVTKObject::boundingRect() const
{
	return shape().boundingRect();
}

void VipPlotVTKObject::setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type)
{
	VipVTKGraphicsView* _old = qobject_cast<VipVTKGraphicsView*>(this->view());
	VipPlotItemDataType::setAxes(axes, type);
	VipVTKGraphicsView* _new = qobject_cast<VipVTKGraphicsView*>(this->view());

	if (_old && _old != _new) {
		if (d_data->actor)
			_old->renderers()[d_data->layer]->RemoveActor(d_data->actor);
		_old->contours()->remove(this);
	}

	if (_new) {
		if (_new != _old)
			this->setColorMap(_new->area()->colorMapAxis());
		this->setZValue(_new->area()->canvas()->zValue() + 10);
	}
	
	buildMapperAndActor(rawData(), true);
}

void VipPlotVTKObject::drawSelected(QPainter * p, const VipCoordinateSystemPtr &m) const
{
	return draw(p, m);
}

void VipPlotVTKObject::draw(QPainter * p, const VipCoordinateSystemPtr & ) const
{
	p->resetTransform();
	if(VipVTKObject dat = data().value<VipVTKObject>())
		if(isSelected() || d_data->hover)
			if (VipVTKGraphicsView * v = qobject_cast<VipVTKGraphicsView*>(this->view()))
				if(v->contours()->IsEnabled())
				{
					p->setRenderHints(QPainter::Antialiasing );

					//draw the object outlines
					p->setPen(QPen(selectedColor(), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

					//QPolygonF pl = v->contours()->Outline(dat);
					//p->drawLines(pl);
					//TEST
					const QList<QPolygonF> pl = v->contours()->Outlines(this);
					for (int i = 0; i < pl.size(); ++i)
						p->drawPolygon(pl[i]);
					
					//draw the object closest picked point
					QColor c = selectedColor();
					p->setPen(QPen(c, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
					c.setAlpha(200);
					p->setBrush(c);

					QPointF pos;
					QPolygonF cell;
					int object_id = v->contours()->ObjectId(d_data->mouse);
					int point_id = v->contours()->ClosestPointId(object_id, d_data->mouse,&pos,&cell);
					if (point_id >= 0)
					{
						p->drawEllipse(QRectF(pos - QPointF(3,3), pos + QPointF(3, 3)));
					}
					//draw the underlying cell
					if (cell.size())
					{
						p->drawPolygon(cell);
					}

				}
}

QRectF VipPlotVTKObject::drawLegend(QPainter * painter, const QRectF & rect, int ) const
{
	
	painter->setRenderHints(this->renderHints());

	VipBoxStyle bs;
	QColor c = d_data->color;
	if (isSelected())
		c = d_data->selectedColor;
	bs.setBackgroundBrush(c);
	bs.setBorderPen(QPen(c.darker()));
	bs.computeRect(rect);
	bs.drawBackground(painter);
	
	return rect;
}

void VipPlotVTKObject::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
	d_data->hover = true;
}

void VipPlotVTKObject::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
	d_data->hover = false;
	d_data->mouse = QPoint(-1, -1);
}

void VipPlotVTKObject::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
	//set this plot item data as the highlighted object in order to pick cells and points from it
	if (VipVTKObject dat = data().value<VipVTKObject>())
		if (isSelected() || d_data->hover)
			if (VipVTKGraphicsView * v = qobject_cast<VipVTKGraphicsView*>(this->view()))
			{
				d_data->mouse = v->mapFromScene(event->scenePos());
				v->contours()->SetHighlightedData(this);
			}
}

void VipPlotVTKObject::mousePressEvent(QGraphicsSceneMouseEvent * )
{
	
}

void VipPlotVTKObject::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
	event->ignore();
}

void VipPlotVTKObject::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
	if (testItemAttribute(IgnoreMouseEvents ))
	{
		event->ignore();
		return;
	}

	if (event->button() == Qt::LeftButton)
	{
		if (event->buttonDownPos(Qt::LeftButton) == event->pos())
		{
			//select the item
			if (VipVTKObject dat = data().value<VipVTKObject>())
			{
				bool inside_shape = shape().contains(event->pos());//isUnderPoint(event->pos());
				bool ctrl_down = (event->modifiers() & Qt::ControlModifier);
				bool was_selected = isSelected();
				bool selected = inside_shape;
				if (was_selected && ctrl_down)
					selected = false;

				this->setSelected(selected);

				if (!ctrl_down && !(was_selected && selected))
				{
					//unselect all other items
					QList<QGraphicsItem *> items;
					if (parentItem())
						items = parentItem()->childItems();
					else if (scene())
						items = scene()->items();

					for (int i = 0; i < items.size(); ++i)
					{
						if (items[i] != this)
							items[i]->setSelected(false);
					}
				}

				if (!inside_shape)
					event->ignore();
			}
		}
	}
}

void VipPlotVTKObject::geometryChanged()
{
	this->prepareGeometryChange();
}

void VipPlotVTKObject::receiveVisibilityChanged(VipPlotItem* )
{
	if (d_data->actor)
		d_data->actor->SetVisibility(this->isVisible());
}

void VipPlotVTKObject::receiveSelectionChanged(VipPlotItem*)
{
	applyPropertiesInternal();
}

void VipPlotVTKObject::setSelectedColor(const QColor & c)
{
	if (d_data->color != c) {
		d_data->selectedColor = c;
		applyPropertiesInternal();
		emitItemChanged();
	}
}

const QColor & VipPlotVTKObject::selectedColor() const
{
	return d_data->selectedColor;
}

void VipPlotVTKObject::setPen(const QPen& p)
{
	setSelectedColor(p.color());
}
QPen VipPlotVTKObject::pen() const
{
	return QPen(d_data->selectedColor);
}

void VipPlotVTKObject::setBrush(const QBrush& b) 
{
	setSelectedColor( b.color());
}
QBrush VipPlotVTKObject::brush() const
{
	return QBrush(d_data->selectedColor);
}

void VipPlotVTKObject::setColor(const QColor& c)
{
	if (d_data->color != c) {
		d_data->color = c;
		applyPropertiesInternal();
		emitItemChanged();
	}
}
const QColor& VipPlotVTKObject::color() const
{
	return d_data->color;
}


void VipPlotVTKObject::setHighlightColor(const QColor& c)
{
	fromQColor(c,d_data->highlightColor);
	applyPropertiesInternal();
	emitItemChanged();
}
QColor VipPlotVTKObject::highlightColor() const 
{
	if (hasHighlightColor())
		return toQColor(d_data->highlightColor);
	return QColor(Qt::transparent);
}
bool VipPlotVTKObject::hasHighlightColor() const
{
	return !(d_data->highlightColor[0] < 0);
}
void VipPlotVTKObject::removeHighlightColor()
{
	d_data->highlightColor[0] = -1;
	applyPropertiesInternal();
	emitItemChanged();
}

void VipPlotVTKObject::setEdgeColor(const QColor& c)
{
	if (d_data->edgeColor != c) {
		d_data->edgeColor = c;
		applyPropertiesInternal();
		emitItemChanged();
	}
}
const QColor& VipPlotVTKObject::edgeColor() const
{
	return d_data->edgeColor;
}

void VipPlotVTKObject::setOpacity(double op)
{
	if (op != d_data->opacity) {
		d_data->opacity = op;
		applyPropertiesInternal();
		emitItemChanged();
	}
}
double VipPlotVTKObject::opacity() const
{
	return d_data->opacity;
}

bool VipPlotVTKObject::edgeVisible() const
{
	return d_data->edgeVisible;
}
int VipPlotVTKObject::layer() const 
{
	return d_data->layer;
}
void VipPlotVTKObject::setEdgeVisible(bool visible)
{
	if (visible != d_data->edgeVisible) {

		d_data->edgeVisible = visible;
		applyPropertiesInternal();
		emitItemChanged();
	}
}

void VipPlotVTKObject::synchronizeSelectionWith(VipPlotItem* item) 
{
	if (item != d_data->syncSelect){
		if (d_data->syncSelect)
			disconnect(d_data->syncSelect, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(syncSelectionChanged(VipPlotItem*)));
		d_data->syncSelect = item;
		if (item)
			connect(item, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(syncSelectionChanged(VipPlotItem*)));
	}
}
VipPlotItem* VipPlotVTKObject::selectionSynchronizedWith() const 
{
	return d_data->syncSelect;
}

void VipPlotVTKObject::syncSelectionChanged(VipPlotItem*)
{
	if (VipPlotItem* it = d_data->syncSelect)
		setSelected(it->isSelected());
}

void VipPlotVTKObject::setLayer(int layer)
{
	if (layer < 0)
		layer = 0;
	
	if (layer != d_data->layer) {
		VipVTKGraphicsView* view = qobject_cast<VipVTKGraphicsView*>(this->view());
		if (d_data->actor) {
			if (view ) {
				if (layer >= view->renderers().size())
					layer = view->renderers().size() - 1;
				vtkRenderer* ren = view->renderers()[d_data->layer];
				ren->RemoveActor(d_data->actor);
				ren = view->renderers()[layer];
				ren->AddActor(d_data->actor);
			}
		}
		d_data->layer = layer;
		emitItemChanged();
		if (view)
			view->refresh();
	}
}

void VipPlotVTKObject::applyPropertiesInternal()
{
	QColor c = d_data->color;
	if (!isSelected() && hasHighlightColor()) {
		c = toQColor(d_data->highlightColor);
	}
	else if (isSelected())
		c = d_data->selectedColor;

	
	if (d_data->actor) {
		double color[3];
		fromQColor(c, color);

		d_data->actor->GetProperty()->SetOpacity(d_data->opacity);
		d_data->actor->GetProperty()->SetColor(color);
		d_data->actor->GetProperty()->SetEdgeVisibility(d_data->edgeVisible);
		if (d_data->edgeVisible) {
			fromQColor(d_data->edgeColor, color);
			d_data->actor->GetProperty()->SetEdgeColor(color);
		}
	}
}

void VipPlotVTKObject::buildMapperAndActor(const VipVTKObject & obj, bool in_set_data)
{
	bool had_actor = d_data->actor.GetPointer();

	if (!obj) {
		d_data->mapper = vtkSmartPointer<vtkMapper>();
		d_data->actor = vtkSmartPointer<vtkActor>();
		return;
	}

	vtkDataObject* data = obj.data();

	if (!d_data->mapper || d_data->mapper->GetInput() != data) {

		d_data->mapper = vtkSmartPointer<vtkMapper>();

		if (data->IsA("vtkPolyData")) {
			vtkPolyDataMapper* m = vtkPolyDataMapper::New();
			m->SetInputData(static_cast<vtkPolyData*>(data));
			d_data->mapper.TakeReference(m);
		}
		else if (data->IsA("vtkGraph")) {
			vtkGraphMapper* m = vtkGraphMapper::New();
			m->SetInputData(static_cast<vtkGraph*>(data));
			d_data->mapper.TakeReference(m);
		}
		else if (data->IsA("vtkRectilinearGrid")) {
			vtkDataSetMapper* m = vtkDataSetMapper::New();
			m->SetInputData(static_cast<vtkRectilinearGrid*>(data));
			d_data->mapper.TakeReference(m);
		}
		else if (data->IsA("vtkStructuredGrid")) {
			vtkDataSetMapper* m = vtkDataSetMapper::New();
			m->SetInputData(static_cast<vtkStructuredGrid*>(data));
			d_data->mapper.TakeReference(m);
		}
		else if (data->IsA("vtkUnstructuredGrid")) {
			vtkDataSetMapper* m = vtkDataSetMapper::New();
			m->SetInputData(static_cast<vtkUnstructuredGrid*>(data));
			d_data->mapper.TakeReference(m);
		}
		else if (data->IsA("vtkStructuredPoints")) {
			vtkDataSetMapper* m = vtkDataSetMapper::New();
			m->SetInputData(static_cast<vtkStructuredPoints*>(data));
			d_data->mapper.TakeReference(m);
		}
		else if (data->IsA("vtkCompositeDataSet")) {
			vtkCompositePolyDataMapper* m = vtkCompositePolyDataMapper ::New();
			m->SetInputDataObject(static_cast<vtkCompositeDataSet*>(data));
			d_data->mapper.TakeReference(m);
		}

		if (d_data->mapper) {
			VIP_VTK_OBSERVER(d_data->mapper);
		}
	}

	if (!d_data->actor || d_data->actor->GetMapper() != d_data->mapper.GetPointer()) {

		// Remove actor from previous renderer
		if (d_data->actor && d_data->actor->GetNumberOfConsumers()) {
			for (int i = 0; i < d_data->actor->GetNumberOfConsumers(); ++i)
				if (vtkObject* c = d_data->actor->GetConsumer(i)) {
					if (c->IsA("vtkRenderer"))
						static_cast<vtkRenderer*>(c)->RemoveActor(d_data->actor);
				} 
		}
		

		d_data->actor = vtkSmartPointer<vtkActor>::New();
		d_data->actor->SetMapper(d_data->mapper);
		d_data->actor->PickableOn();

		if (!had_actor)
			vipGlobalActorParameters()->apply(this);

		VIP_VTK_OBSERVER(d_data->actor);
		applyPropertiesInternal();
	}

	if (d_data->actor && in_set_data) {

		// add the data to the VipVTKGraphicsView and remove the previous one
		if (VipVTKGraphicsView* view = qobject_cast<VipVTKGraphicsView*>(this->view())) {

			bool reset_camera = d_data->actor->GetNumberOfConsumers() == 0;

			// Add actor to view (if not already done)
			vtkRenderer* ren = view->renderers()[d_data->layer];
			ren->AddActor(d_data->actor);
			view->contours()->add(this);

			d_data->actor->GetProperty()->SetLighting(view->lighting());

			view->emitDataChanged();
			if (reset_camera)
				view->resetCamera();

			//TODO
			//view->refresh();
			//view->propagateSourceProperties();
		}
	}
}

void VipPlotVTKObject::setData(const QVariant & d)
{
	VipVTKObject newdata = d.value<VipVTKObject>();
	VipVTKObject current = data().value<VipVTKObject>();
	if (newdata != current)
	{
		if (title().isEmpty() && newdata)
			setTitle(VipText(QFileInfo(newdata.dataName()).fileName()));
	}

	//actually set the data
	VipPlotItemDataType<VipVTKObject>::setData(d);

	buildMapperAndActor(newdata,true);

	if (newdata)
	{
		this->setProperty("Global informations", newdata.description(-1, -1));
	}
}




VipVTKObjectList fromPlotVipVTKObject(const PlotVipVTKObjectList& lst) 
{
	VipVTKObjectList res;
	res.reserve(lst.size());
	for (VipPlotVTKObject* it : lst)
		res.push_back(it->rawData());
	return res;
}





VipDisplayVTKObject::VipDisplayVTKObject(QObject * parent)
	:VipDisplayPlotItem(parent), m_modified(0),m_object(nullptr)
{
	setItem(new VipPlotVTKObject());
}

void VipDisplayVTKObject::formatItem(VipPlotItem * item, const VipAnyData & any)
{
	VipDisplayPlotItem::formatItem(item, any);
	if (VipVTKObject current = any.value<VipVTKObject>())
		item->setTitle(QFileInfo(current.dataName()).fileName());
}

void VipDisplayVTKObject::displayData(const VipAnyDataList & lst)
{
	if (VipPlotVTKObject* it = item())
	{
		if (lst.size())
		{
			VipAnyData data = lst.back();
			if (VipVTKObject ptr = data.value<VipVTKObject>())
			{
				if (ptr.data())
				{
					if (ptr.data() != m_object || (qint64)ptr.data()->GetMTime() > m_modified)
					{
						m_object = ptr.data();
						m_modified = ptr.data()->GetMTime();
						it->setData(data.data());
						formatItem(it, data);
					}
				}
			}
		}		
	}
}












class VipPlotFieldOfView::PrivateData
{
public:
	PrivateData() : color(Qt::transparent) {}
	QColor color;
	//QPointer<FOVItem> item;
	QPointer<VipDisplayFieldOfView> display;
	QPixmap pixmap;
};

VipPlotFieldOfView::VipPlotFieldOfView(const VipText & title)
	:VipPlotItemDataType(title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	this->setRenderHints(QPainter::Antialiasing);
	this->setItemAttribute(VipPlotItem::HasToolTip, true);
	this->setItemAttribute(VipPlotItem::HasLegendIcon,false);
	this->setRenderHints(QPainter::Antialiasing );
}

VipPlotFieldOfView::~VipPlotFieldOfView()
{
	//TODO
	/* if (d_data->item)
	{
		if (VTK3DPlayer* w = VTK3DPlayer::fromChild(this->view()))
		{
			w->fov()->removeFieldOfView(d_data->item);
		}
	}*/

	bool stop = true;
}

VipInterval VipPlotFieldOfView::plotInterval(const VipInterval &) const
{
	return VipInterval();
}

void VipPlotFieldOfView::drawSelected(QPainter * p, const VipCoordinateSystemPtr &m) const
{
	return draw(p, m);
}

void VipPlotFieldOfView::draw(QPainter * p, const VipCoordinateSystemPtr & ) const
{
	p->resetTransform();
	
	//draw

	VipFieldOfView fov = rawData();
	if (VipVTKGraphicsView * view = qobject_cast<VipVTKGraphicsView*>(this->view()))
	{
		//if (d_data->item && d_data->item->isSelected())
		//TODO
		if (isSelected() || property("_force_select").toBool())
		{
			QPoint pos = view->transformToView(fov.pupil);

			//create the path
			int height = 20;
			int width = 10;

			QPoint top = pos;
			top.setY(top.y() - height);

			QRectF rect(top.x() - width / 2., top.y(), width, width);

			QPainterPath path;
			path.moveTo(rect.left(),rect.center().y());
			path.arcTo(rect,180, -180);
			path.quadTo(pos,pos);
			path.quadTo(QPointF(rect.left(), rect.center().y()), QPointF(rect.left(), rect.center().y()));

			//remove circle inside
			QRectF circle(0, 0, width / 2., width / 2.);
			circle.moveCenter(QPointF(top.x(), top.y() + height / 3.0));
			QPainterPath cpath;
			cpath.addEllipse(circle);
			path = path.subtracted(cpath);

			//create the brush
			QGradientStops stops;
			stops.append(QGradientStop(0, d_data->color));
			stops.append(QGradientStop(0.5, d_data->color.lighter()));
			stops.append(QGradientStop(1, d_data->color));
			QLinearGradient grad(QPointF(rect.left(), rect.top()), QPointF(rect.right(), rect.top()));
			grad.setStops(stops);

			//draw in a pixmap
			d_data->pixmap = QPixmap(width+2, height+2);
			d_data->pixmap.fill(Qt::transparent);
			QPainter painter(&d_data->pixmap);
			painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);

			painter.setPen(d_data->color);
			painter.setBrush(QBrush(grad));
			painter.setTransform(QTransform().translate(-rect.left() + 1, -rect.top() + 1));
			painter.drawPath(path);

			p->drawPixmap(rect.left() - 1, rect.top() - 1, d_data->pixmap);

			//draw the camera name just above
			VipText text = title();
			
			text.setTextPen(d_data->color);
			QRectF text_rect = text.textRect();
			text_rect.moveCenter(QPointF(top.x(), top.y() - text_rect.height()));
			text.draw(p, text_rect);
		}
	}
}

void VipPlotFieldOfView::geometryChanged()
{
	this->prepareGeometryChange();
}

/*void VipPlotFieldOfView::mousePressEvent(QGraphicsSceneMouseEvent * event);
void VipPlotFieldOfView::mouseReleaseEvent(QGraphicsSceneMouseEvent * event);
void VipPlotFieldOfView::mouseMoveEvent(QGraphicsSceneMouseEvent * event);
*/

void VipPlotFieldOfView::setSelectedColor(const QColor & c)
{
	if (c != d_data->color) {

		d_data->color = c;
		//TODO
		/* if (d_data->item) {
			double color[3];
			color[0] = d_data->color.redF();
			color[1] = d_data->color.greenF();
			color[2] = d_data->color.blueF();
			d_data->item->setColor(color);
		}*/
		emitColorChanged();
		emitItemChanged();
	}
}

const QColor & VipPlotFieldOfView::selectedColor() const
{
	return d_data->color;
}

void VipPlotFieldOfView::setData(const QVariant & d)
{
	//actually set the data
	VipPlotItemDataType<VipFieldOfView>::setData(d);

	//set the title
	const VipFieldOfView fov = d.value<VipFieldOfView>();
	if (fov.isNull())
		return;

	if (title().text() != fov.name)
		setTitle(VipText(fov.name));

	//TODO
	/* if (d_data->item) {

		if (d_data->color == Qt::transparent) {
			const double* color = d_data->item->color();
			d_data->color = QColor(color[0] * 255, color[1] * 255, color[2] * 255);
		}
		else {
			double color[3];
			color[0] = d_data->color.redF();
			color[1] = d_data->color.greenF();
			color[2] = d_data->color.blueF();
			d_data->item->setColor(color);
		}
	}*/
}

void VipPlotFieldOfView::setAxes(const QList<VipAbstractScale*> & axes, VipCoordinateSystem::Type type)
{
	VipVTKGraphicsView* old = qobject_cast<VipVTKGraphicsView*>(this->view());
	VipVTKGraphicsView* _new = qobject_cast<VipVTKGraphicsView*>(axes.first()->view()); // VTK3DPlayer::fromChild(axes.first()->view());
	if (old != _new) {

		//TODO
		/* if (d_data->item) {
			d_data->item->setPlotFov(nullptr);
			old->fov()->removeFieldOfView(d_data->item);
		}*/
		if (_new) {
			//TODO
			/* d_data->item = _new->fov()->addFieldOfView(this);

			if (d_data->color == Qt::transparent) {
				const double* color = d_data->item->color();
				d_data->color = QColor(color[0] * 255, color[1] * 255, color[2] * 255);
			}
			else {
				double color[3];
				color[0] = d_data->color.redF();
				color[1] = d_data->color.greenF();
				color[2] = d_data->color.blueF();
				d_data->item->setColor(color);
			}*/
		}
	}
	VipPlotItemDataType::setAxes(axes, type);
}


class VipDisplayFieldOfView::PrivateData
{
public:
	QPointer<FOVItem> item;
	VipAnyData previous;
};

VipDisplayFieldOfView::VipDisplayFieldOfView(QObject * parent)
	:VipDisplayPlotItem(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	setItem(new VipPlotFieldOfView());
}

VipDisplayFieldOfView::~VipDisplayFieldOfView()
{
}

void VipDisplayFieldOfView::setFOVItem(FOVItem * item)
{
	//TODO
	/* d_data->item = item;
	if (item)
	{
		if (VipVTKGraphicsView * view = item->view())
			this->item()->setAxes(view->area()->bottomAxis(), view->area()->leftAxis(), VipCoordinateSystem::Cartesian);
	}*/
}

FOVItem * VipDisplayFieldOfView::getFOVItem() const
{
	return nullptr;
	//return d_data->item;
}

void VipDisplayFieldOfView::displayData(const VipAnyDataList & lst)
{
	if (!this->item())
		return;

	if (lst.size())
	{
		VipAnyData data = lst.back();

		VipFieldOfView cur = data.value<VipFieldOfView>();
		VipFieldOfView prev = d_data->previous.value<VipFieldOfView>();
		bool same_fov = (cur == prev);

		if(!same_fov)
			this->item()->setData(data.data());

		if (FOVItem * item = getFOVItem())
		{
			d_data->previous = data;
		}
	}
}










VipArchive& operator<<(VipArchive& arch, const VipPlotVTKObject* pl)
{
	arch.content("color", pl->color());
	arch.content("selectedColor", pl->selectedColor());
	arch.content("edgeColor", pl->edgeColor());
	arch.content("edgeVisible", pl->edgeVisible());
	arch.content("opacity", pl->opacity());
	arch.content("layer", pl->layer());

	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotVTKObject* pl)
{
	QColor color = arch.read("color").value<QColor>();
	QColor selectedColor = arch.read("selectedColor").value<QColor>();
	QColor edgeColor = arch.read("color").value<QColor>();
	bool edgeVisible = arch.read("edgeVisible").value<bool>();
	double opacity = arch.read("opacity").value<double>();
	int layer = arch.read("layer").value<int>();

	if (arch) {
		pl->setColor(color);
		pl->setSelectedColor(selectedColor);
		pl->setEdgeColor(edgeColor);
		pl->setEdgeVisible(edgeVisible);
		pl->setOpacity(opacity);
		pl->setLayer(layer);
	}

	return arch;
}





VipArchive & operator<<(VipArchive & arch, const VipPlotFieldOfView * pl)
{
	arch.content("selectedColor", pl->selectedColor());
	arch.content("opacity", pl->opacity());
	
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotFieldOfView* pl)
{
	QColor selectedColor = arch.read("selectedColor").value<QColor>();
	double opacity = arch.read("opacity").value<double>();

	if (arch) {
		pl->setSelectedColor(selectedColor);
		pl->setOpacity(opacity);
	}

	return arch;
}





static int registerObjects()
{
	vipRegisterArchiveStreamOperators<VipPlotVTKObject*>();
	vipRegisterArchiveStreamOperators<VipPlotFieldOfView*>();
	return 0;
}

static bool _registerObjects = vipAddInitializationFunction(registerObjects);