#include "VipMultiPlotWidget2D.h"
#include "VipPlotGrid.h"


class VipVMultiPlotArea2D::PrivateData
{
public:
	PrivateData()
	{
		yLeft = new VipMultiAxisBase(VipAxisBase::Left);
		yRight = new VipMultiAxisBase(VipAxisBase::Right);

		//add a VipAxisBase to the left and right scale
		left = new VipAxisBase(VipAxisBase::Left);
		left->setMargin(0);
		//left->setMaxBorderDist(0, 0);
		left->setZValue(20);


		yLeft->setMargin(0);
		//yLeft->setMaxBorderDist(0, 0);
		yLeft->setZValue(20);

		yRight->setMargin(0);
		//yRight->setMaxBorderDist(0, 0);
		yRight->setZValue(20);

		to_remove = nullptr;
		inConstructor = true;
		insertionIndex = -1;
	}

	VipMultiAxisBase * yLeft;
	VipMultiAxisBase * yRight;
	VipAxisBase * left;
	VipAbstractScale * to_remove;
	QList<VipPlotGrid*> grids;
	QList<VipPlotCanvas*> canvas;
	QList<VipAxisBase*> haxes;
	int insertionIndex;
	bool inConstructor;

	QPointer<VipAxisBase> lmodel;
	QPointer<VipAxisBase> rmodel;
	QPointer<VipPlotCanvas> cmodel;
	QPointer<VipPlotGrid> gmodel;
};


VipVMultiPlotArea2D::VipVMultiPlotArea2D(QGraphicsItem * parent)
	: VipPlotArea2D(parent)
{

	d_data = new PrivateData();

	//remove the preivous left and right axes
	removeScale(VipPlotArea2D::leftAxis());
	removeScale(VipPlotArea2D::rightAxis());
	delete VipPlotArea2D::leftAxis();
	delete VipPlotArea2D::rightAxis();

	addScale(d_data->yLeft);
	addScale(d_data->yRight);

	grid()->setAxes(bottomAxis(), d_data->left, VipCoordinateSystem::Cartesian);
	canvas()->setAxes(bottomAxis(), d_data->left, VipCoordinateSystem::Cartesian);
	d_data->grids << grid();
	d_data->canvas << canvas();
	d_data->haxes << bottomAxis();

	grid()->setProperty("_vip_no_serialize",true);
	canvas()->setProperty("_vip_no_serialize",true);

	d_data->lmodel = static_cast<VipAxisBase*>(d_data->left);
	d_data->cmodel = d_data->canvas.first();
	d_data->gmodel = d_data->grids.first();
	d_data->inConstructor = false;
	addScale(d_data->left);

	//we can safely delete previous grid and canvas
	delete d_data->grids.first();
	delete d_data->canvas.first();
	d_data->grids.removeFirst();
	d_data->canvas.removeFirst();
	d_data->cmodel = d_data->canvas.first();
	d_data->gmodel = d_data->grids.first();

	d_data->rmodel = static_cast<VipAxisBase*>(d_data->yRight->at(0));
}

VipVMultiPlotArea2D::~VipVMultiPlotArea2D()
{
	delete d_data;
}

VipAxisBase * VipVMultiPlotArea2D::leftAxis() const
{
	for (int i = 0; i < d_data->yLeft->count(); ++i)
		if (const VipAxisBase* ax = qobject_cast<const VipAxisBase*>(d_data->yLeft->at(i)))
			return const_cast<VipAxisBase*>(ax);
	return nullptr;
}

VipAxisBase * VipVMultiPlotArea2D::rightAxis() const
{
	for (int i = 0; i < d_data->yRight->count(); ++i)
		if (const VipAxisBase* ax = qobject_cast<const VipAxisBase*>(d_data->yRight->at(i)))
			return const_cast<VipAxisBase*>(ax);
	return nullptr;
}

VipPlotGrid * VipVMultiPlotArea2D::grid() const
{
	return d_data->grids.size() ? d_data->grids.first() : VipPlotArea2D::grid();
}
VipPlotCanvas * VipVMultiPlotArea2D::canvas() const
{
	return d_data->canvas.size() ? d_data->canvas.first() : VipPlotArea2D::canvas();
}

VipMultiAxisBase * VipVMultiPlotArea2D::leftMultiAxis() const
{
	return const_cast<VipMultiAxisBase*>(d_data->yLeft);
}
VipMultiAxisBase * VipVMultiPlotArea2D::rightMultiAxis() const
{
	return const_cast<VipMultiAxisBase*>(d_data->yRight);
}

QList<VipPlotGrid*> VipVMultiPlotArea2D::allGrids() const
{
	return d_data->grids;
}
QList<VipPlotCanvas*> VipVMultiPlotArea2D::allCanvas() const
{
	return d_data->canvas;
}
QList<VipAxisBase*> VipVMultiPlotArea2D::horizontalAxes() const
{
	return d_data->haxes;
}

QPainterPath VipVMultiPlotArea2D::plotArea(const VipBorderItem * vertical_scale) const
{
	if (!vertical_scale)
		return QPainterPath();

	int l_index = d_data->yLeft->indexOf(vertical_scale);
	int r_index = d_data->yRight->indexOf(vertical_scale);
	if (l_index >= 0 || r_index >= 0) {

		QPointF vstart = vertical_scale->mapToItem(this, vertical_scale->start());
		QPointF vend = vertical_scale->mapToItem(this, vertical_scale->end());
		QPointF hstart = bottomAxis()->mapToItem(this, bottomAxis()->start());
		QPointF hend = bottomAxis()->mapToItem(this, bottomAxis()->end());

		QPolygonF polygon;
		polygon << QPointF(hstart.x(),vstart.y()) << QPointF(hstart.x(), vend.y())
			<< QPointF(hend.x(), vend.y()) << QPointF(hend.x(), vstart.y());

		QPainterPath path;
		path.addPolygon(polygon);
		path.closeSubpath();
		return path;
	}
	return QPainterPath();
}

void VipVMultiPlotArea2D::setInsertionIndex(int index)
{
	d_data->insertionIndex = index;
}
int VipVMultiPlotArea2D::insertionIndex() const
{
	return d_data->insertionIndex;
}

QRectF VipVMultiPlotArea2D::plotRect() const
{
	if (d_data->yLeft->count() == 0)
		return QRectF();

	QPointF hstart = bottomAxis()->mapToItem(this, bottomAxis()->start());
	QPointF hend = bottomAxis()->mapToItem(this, bottomAxis()->end());
	double vstart = d_data->yLeft->at(0)->mapToItem(this, d_data->yLeft->at(0)->start()).y();
	double vend = d_data->yLeft->at(0)->mapToItem(this, d_data->yLeft->at(0)->end()).y();

	for (int i = 0; i < d_data->yLeft->count(); ++i) {
		VipBorderItem * vertical_scale = d_data->yLeft->at(i);
		double _vstart = vertical_scale->mapToItem(this, vertical_scale->start()).y();
		double _vend = vertical_scale->mapToItem(this, vertical_scale->end()).y();
		vstart = qMin(vstart, _vstart);
		vend = qMax(vend, _vend);
	}
	return QRectF(hstart.x(), vstart, hend.x() - hstart.x(), vend - vstart).normalized();
}

void VipVMultiPlotArea2D::applyLabelOverlapping()
{
	QVector< QSharedPointer<QPainterPath> > overlapps;
	for (int i = 0; i < d_data->yLeft->count(); ++i)
		overlapps << d_data->yLeft->at(i)->constScaleDraw()->thisLabelArea();

	for (int i = 0; i < d_data->yLeft->count(); ++i) {
		if (!d_data->yLeft->at(i)->scaleDraw()->labelOverlappingEnabled()) {
			QVector< QSharedPointer<QPainterPath> > copy = overlapps;
			copy.removeAt(i);
			d_data->yLeft->at(i)->scaleDraw()->enableLabelOverlapping(false);
			d_data->yLeft->at(i)->scaleDraw()->clearAdditionalLabelOverlapp();
			d_data->yLeft->at(i)->scaleDraw()->setAdditionalLabelOverlapp(copy);
		}
	}

	overlapps.clear();
	for (int i = 0; i < d_data->yRight->count(); ++i)
		//if(d_data->yRight->at(i)->constScaleDraw()->hasComponent(VipScaleDraw::Labels))
			overlapps << d_data->yRight->at(i)->constScaleDraw()->thisLabelArea();

	//int c = 0;
	for (int i = 0; i < d_data->yRight->count(); ++i) {
		//if (d_data->yRight->at(i)->constScaleDraw()->hasComponent(VipScaleDraw::Labels)) 
 {
			if (!d_data->yRight->at(i)->scaleDraw()->labelOverlappingEnabled()) {
				QVector< QSharedPointer<QPainterPath> > copy = overlapps;
				copy.removeAt(i);//c);
				d_data->yRight->at(i)->scaleDraw()->enableLabelOverlapping(false);
				d_data->yRight->at(i)->scaleDraw()->clearAdditionalLabelOverlapp();
				d_data->yRight->at(i)->scaleDraw()->setAdditionalLabelOverlapp(copy);
				//++c;
			}
		}
	}
}

void VipVMultiPlotArea2D::applyDefaultParameters()
{
	//apply parameters to left scales
	if (VipAxisBase * model = qobject_cast<VipAxisBase*>(d_data->lmodel)){
		for (int i = 0; i < d_data->yLeft->count(); ++i){
			VipBorderItem * it = d_data->yLeft->at(i);
			it->setMargin(model->margin());
			it->setSpacing(model->spacing());
			double st, en;
			model->getMaxBorderDist(st, en);
			it->setMaxBorderDist(st, en);
			model->getMinBorderDist(st, en);
			it->setMinBorderDist(st, en);
			it->setMaxMajor(model->maxMajor());
			it->setMaxMinor(model->maxMinor());
			it->scaleDraw()->setTextStyle(model->textStyle(VipScaleDiv::MajorTick), VipScaleDiv::MajorTick);
			if (VipAxisBase * b = qobject_cast<VipAxisBase*>(it)) {
				b->setTitleInverted(model->isTitleInverted());
				b->scaleDraw()->setComponents(model->scaleDraw()->components());
			}
		}
	}
	//apply parameters to right scales
	if (VipAxisBase * model = qobject_cast<VipAxisBase*>(d_data->rmodel)){
		for (int i = 0; i < d_data->yRight->count(); ++i){
			VipBorderItem * it = d_data->yRight->at(i);
			it->setMargin(model->margin());
			it->setSpacing(model->spacing());
			double st, en;
			model->getMaxBorderDist(st, en);
			it->setMaxBorderDist(st, en);
			model->getMinBorderDist(st, en);
			it->setMinBorderDist(st, en);
			it->setMaxMajor(model->maxMajor());
			it->setMaxMinor(model->maxMinor());
			it->scaleDraw()->setTextStyle(model->textStyle(VipScaleDiv::MajorTick), VipScaleDiv::MajorTick);
			if (VipAxisBase * b = qobject_cast<VipAxisBase*>(it)) {
				b->setTitleInverted(model->isTitleInverted());
				b->scaleDraw()->setComponents(model->scaleDraw()->components());
			}
		}
	}
	//apply parameters to grid
	if(d_data->gmodel)
		for (int i = 0; i < d_data->grids.size(); ++i){
			d_data->grids[i]->setMajorPen(d_data->gmodel->majorPen());
			d_data->grids[i]->setMinorPen(d_data->gmodel->minorPen());
			d_data->grids[i]->enableAxisMin(0,d_data->gmodel->axisMinEnabled(0));
			d_data->grids[i]->enableAxisMin(1, d_data->gmodel->axisMinEnabled(1));
			d_data->grids[i]->enableAxis(0, d_data->gmodel->axisEnabled(0));
			d_data->grids[i]->enableAxis(1, d_data->gmodel->axisEnabled(1));
			//d_data->grids[i]->setDrawSelected(d_data->gmodel->drawSelected());
			d_data->grids[i]->setZValue(d_data->gmodel->zValue());
			d_data->grids[i]->setVisible(d_data->gmodel->isVisible());
		}
	//apply canvas parameters
	if(d_data->cmodel)
		for (int i = 0; i < d_data->canvas.size(); ++i)
		{
			d_data->canvas[i]->setBoxStyle(d_data->cmodel->boxStyle());
			d_data->canvas[i]->setZValue(d_data->cmodel->zValue());
		}
}

bool VipVMultiPlotArea2D::internalAddScale(VipAbstractScale * sc, bool spatial)
{
	if(spatial && !d_data->inConstructor)
		if (VipAxisBase * b = qobject_cast<VipAxisBase*>(sc))
		{
			sc->scaleDraw()->enableLabelOverlapping(this->defaultLabelOverlapping());
			if (b->alignment() == VipBorderItem::Left)
			{

				//do NOT recompute the geometry based on the internal scale geometry
				//disconnect(sc, SIGNAL(geometryNeedUpdate()), this, SLOT(delayRecomputeGeometry()));

				VipAxisBase * right = new VipAxisBase(VipAxisBase::Right);
				right->setScaleDiv(b->scaleDiv());
				right->scaleDraw()->enableComponent(VipScaleDraw::Labels, false);
				right->setMargin(0);
				//right->setMaxBorderDist(0, 0);
				right->setZValue(101);
				//right->setUpdater(updater());
				b->synchronizeWith(right);

				int insert_index = this->insertionIndex();
				if (insert_index < 0 || insert_index >= d_data->yLeft->count()) {
					d_data->yLeft->addScale(b);
					d_data->yRight->addScale(right);
				}
				else {
					d_data->yLeft->insertScale(insert_index,b);
					d_data->yRight->insertScale(insert_index,right);
				}

				//if (b != d_data->left)
				{
					//add a new grid and canvas
					VipPlotGrid * grid = new VipPlotGrid();
					grid->setTitle(QString("Axes grid"));
					//grid->setDrawSelected(false);
					grid->setAxes(bottomAxis(), b, VipCoordinateSystem::Cartesian);
					grid->setZValue(100);
					//grid->setUpdater(updater());
					VipPlotCanvas * canvas = new VipPlotCanvas();
					canvas->setAxes(bottomAxis(), b, VipCoordinateSystem::Cartesian);
					canvas->setZValue(-1);
					//canvas->setUpdater(updater());

					grid->setProperty("_vip_no_serialize",true);
					canvas->setProperty("_vip_no_serialize",true);

					//add a new horizontal axe if necessary
					if (d_data->yLeft->count() > 1) {
						VipAxisBase * haxe = new VipAxisBase(VipAxisBase::Bottom);
						haxe->synchronizeWith(bottomAxis());
						if(insert_index == 0)
							haxe->setAxisIntersection(d_data->yLeft->at(1), 1, Vip::Relative);
						else
							haxe->setAxisIntersection(b, 1, Vip::Relative);
						//haxe->setUpdater(updater());
						haxe->setScaleDiv(bottomAxis()->scaleDiv());
						haxe->scaleDraw()->enableComponent(VipScaleDraw::Labels, false);
						haxe->setMargin(0);
						addScale(haxe, true);
						if (insert_index < 0 || insert_index >= d_data->haxes.size())
							d_data->haxes << haxe;
						else {
							if (insert_index == 0)
								d_data->haxes.insert(1, haxe); //index 0 is always the bottom axis
							else
								d_data->haxes.insert(insert_index, haxe);
						}
					}
					if (insert_index < 0 || insert_index >= d_data->haxes.size()) {
						d_data->canvas << canvas;
						d_data->grids << grid;
					}
					else {
						d_data->canvas.insert(insert_index, canvas);
						d_data->grids.insert(insert_index, grid);
					}

					applyLabelOverlapping();
					applyDefaultParameters();

					Q_EMIT canvasAdded(canvas);
				}
				d_data->insertionIndex = -1;

				return true;
			}
		}

	return VipPlotArea2D::internalAddScale(sc, spatial);
}

bool VipVMultiPlotArea2D::internalRemoveScale(VipAbstractScale * sc)
{
	if (sc == d_data->yLeft || sc == d_data->yRight || sc == bottomAxis() || sc == topAxis())
		return false;

	if(!d_data->inConstructor)
		if (VipAxisBase * b = qobject_cast<VipAxisBase*>(sc))
		{
			int l_index = d_data->yLeft->indexOf(b);
			int r_index = d_data->yRight->indexOf(b);
			if (l_index >= 0 || r_index >= 0) {

				int index = l_index >= 0 ? l_index : r_index;

				if (d_data->to_remove == sc)
					return false;
				d_data->to_remove = sc;
				//avoid removing the last scale
				//if (d_data->yLeft->count() == 1)
				//	return false;

				//remove the scale from the left AND right ones
				VipAbstractScale *left = d_data->yLeft->takeItem(index);
				left->setParentItem(nullptr);
				VipAbstractScale *right = d_data->yRight->takeItem(index);
				right->setParentItem(nullptr);

				Q_EMIT canvasRemoved(d_data->canvas.at(index));

				//remove grid and canvas
				d_data->grids[index]->setAxes(QList<VipAbstractScale*>(), VipCoordinateSystem::Null);
				d_data->canvas[index]->setAxes(QList<VipAbstractScale*>(), VipCoordinateSystem::Null);
				delete d_data->grids.takeAt(index);
				delete d_data->canvas.takeAt(index);

				//internalRemoveScale should not remove the scale being removed, so do NOT delete the left one,
				//only the right one
				delete right;

				//remove horizontal axe
				if (d_data->haxes[index] != bottomAxis()) {
					removeScale(d_data->haxes[index]);
					delete d_data->haxes.takeAt(index);
				}
				else {
					removeScale(d_data->haxes[1]);
					delete d_data->haxes.takeAt(1);
				}

				//recompute default models
				if (!d_data->lmodel)
					for (int i = 0; i < d_data->yLeft->count(); ++i)
						if (!d_data->lmodel) d_data->lmodel = qobject_cast<VipAxisBase*>(d_data->yLeft->at(i));
				if (!d_data->rmodel)
					for (int i = 0; i < d_data->yRight->count(); ++i)
						if (!d_data->rmodel) d_data->rmodel = qobject_cast<VipAxisBase*>(d_data->yRight->at(i));
				if (!d_data->cmodel)
					d_data->cmodel = d_data->canvas.first();
				if (!d_data->gmodel)
					d_data->gmodel = d_data->grids.first();

				applyLabelOverlapping();

				d_data->to_remove = nullptr;
				return true;
			}
		}

	return VipPlotArea2D::internalRemoveScale(sc);
}

QList<VipAbstractScale*> VipVMultiPlotArea2D::scalesForPos(const QPointF & pos) const
{
	QList<VipAbstractScale*> res; //= VipAbstractScale::independentScales(scales());
	// res.removeOne(d_data->yLeft);
	// res.removeOne(d_data->yRight);

	for (int i = 0; i < d_data->yLeft->count(); ++i)
	{
		QPainterPath p = plotArea(d_data->yLeft->at(i));
		if (p.contains(pos)) {
			res << bottomAxis() << d_data->yLeft->at(i);
			break;
		}
	}
	return res;
}

//VipInterval VipVMultiPlotArea2D::areaBoundaries(const VipAbstractScale * scale) const
// {
// if (const VipBorderItem * item = qobject_cast<const VipBorderItem*>(scale))
// {
// if (item->orientation() == Qt::Vertical)
// {
//	QPointF topleft = d_data->yLeft->geometry().topLeft();
//	QPointF bottomleft = d_data->yLeft->geometry().bottomLeft();
//	return VipInterval(
//		scale->value(scale->mapFromItem(this, topleft)),
//		scale->value(scale->mapFromItem(this, bottomleft))).normalized();
// }
// else
// {
//	QPointF bottomleft = bottomAxis()->geometry().bottomLeft();
//	QPointF bottomright = bottomAxis()->geometry().bottomRight();
//	return VipInterval(
//		scale->value(scale->mapFromItem(this, bottomleft)),
//		scale->value(scale->mapFromItem(this, bottomright))).normalized();
// }
// }
// return VipInterval();
// }

#include "VipLegendItem.h"

void VipVMultiPlotArea2D::resetInnerLegendsPosition()
{
	QRectF area = plotRect();
	QList<VipPlotCanvas*> canvas = findItems<VipPlotCanvas*>(QString(), 2, 1);
	QRectF parent2;
	for (int i = 0; i < canvas.size(); ++i) {
		parent2 = parent2.united(canvas[i]->mapToItem(this, canvas[i]->boundingRect()).boundingRect());
	}

	double top_space = titleOffset();

	for (int i = 0; i < innerLegendCount(); ++i)
	{
		VipLegend * l = innerLegend(i);
		if (l->items().size() == 0)
			continue;

		double space = 0;
		int border_margin = innerLegendMargin(i);
		Qt::Alignment align = innerLegendAlignment(i);

		if (l)
		{
			//find parent rect
			QRectF parent = area;
			if (VipAbstractScale * sc = scaleForlegend(l)) {
				int index = d_data->yLeft->indexOf(static_cast<VipBorderItem*>(sc));
				if(index < 0)
					index = d_data->yRight->indexOf(static_cast<VipBorderItem*>(sc));
				if (index >= 0) {
					//get the canvas rect
					parent = plotArea(d_data->yLeft->at(index)).boundingRect();
					parent = sc->mapFromItem(this, parent).boundingRect();
					if (index == d_data->yLeft->count()-1)
						space = top_space;
					//QPointF p = sc->pos();
					//bool stop = true;
				}
			}

			//compute margin
			double x_margin = 0;
			double y_margin = 0;
			if (border_margin) {
				QPointF p1(0, 0), p2(border_margin, border_margin);
				if (QGraphicsView * v = this->view()) {
					p1 = v->mapToScene(p1.toPoint());
					p2 = v->mapToScene(p2.toPoint());
					p1 = this->mapFromScene(p1);
					p2 = this->mapFromScene(p2);
					x_margin = qAbs(p2.x() - p1.x());
					y_margin = qAbs(p2.y() - p1.y());
				}
			}

			//Compute additional margins due to axis ticks
			double right_margin = 0;
			double left_margin = 0;
			double top_margin = 0;
			double bottom_margin = 0;
			QList< VipAbstractScale*> scales = this->scales();
			for (int j = 0; j < scales.size(); ++j) {
				if (VipBorderItem* it = qobject_cast<VipBorderItem*>(scales[j])) {
					if (it->alignment() == VipBorderItem::Right && it->scaleDraw()->ticksPosition() == VipScaleDraw::TicksOutside && it->scaleDraw()->hasComponent(VipScaleDraw::Ticks)) {
						right_margin = std::max(right_margin, it->scaleDraw()->tickLength(VipScaleDiv::MajorTick));
						right_margin = std::max(right_margin, it->scaleDraw()->tickLength(VipScaleDiv::MediumTick));
						right_margin = std::max(right_margin, it->scaleDraw()->tickLength(VipScaleDiv::MinorTick));
					}
					else if (it->alignment() == VipBorderItem::Left && it->scaleDraw()->ticksPosition() == VipScaleDraw::TicksOutside && it->scaleDraw()->hasComponent(VipScaleDraw::Ticks)) {
						left_margin = std::max(left_margin, it->scaleDraw()->tickLength(VipScaleDiv::MajorTick));
						left_margin = std::max(left_margin, it->scaleDraw()->tickLength(VipScaleDiv::MediumTick));
						left_margin = std::max(left_margin, it->scaleDraw()->tickLength(VipScaleDiv::MinorTick));
					}
					else if (it->alignment() == VipBorderItem::Top && it->scaleDraw()->ticksPosition() == VipScaleDraw::TicksOutside && it->scaleDraw()->hasComponent(VipScaleDraw::Ticks)) {
						top_margin = std::max(top_margin, it->scaleDraw()->tickLength(VipScaleDiv::MajorTick));
						top_margin = std::max(top_margin, it->scaleDraw()->tickLength(VipScaleDiv::MediumTick));
						top_margin = std::max(top_margin, it->scaleDraw()->tickLength(VipScaleDiv::MinorTick));
					}
					else if (it->alignment() == VipBorderItem::Bottom && it->scaleDraw()->ticksPosition() == VipScaleDraw::TicksOutside && it->scaleDraw()->hasComponent(VipScaleDraw::Ticks)) {
						bottom_margin = std::max(bottom_margin, it->scaleDraw()->tickLength(VipScaleDiv::MajorTick));
						bottom_margin = std::max(bottom_margin, it->scaleDraw()->tickLength(VipScaleDiv::MediumTick));
						bottom_margin = std::max(bottom_margin, it->scaleDraw()->tickLength(VipScaleDiv::MinorTick));
					}
				}
			}

			QSizeF size = l->sizeHint(Qt::PreferredSize);//l->effectiveSizeHint(Qt::PreferredSize);

			QPointF pos;
			if (align & Qt::AlignLeft)
				pos.setX(x_margin + left_margin +  parent.left());
			else if (align & Qt::AlignRight)
				pos.setX(parent.right() - size.width() - right_margin- x_margin);
			else
				pos.setX((parent.width() - size.width()) / 2);

			if (align & Qt::AlignTop)
				pos.setY(y_margin + top_margin + parent.top() + space);
			else if (align & Qt::AlignBottom)
				pos.setY(parent.bottom() - size.height() - bottom_margin - y_margin);
			else
				pos.setY((parent.bottom() - size.height()) / 2);

			QRectF geom(pos, size);
			l->setGeometry(geom);
			//l->setPos(pos);
		}
	}
}


void VipVMultiPlotArea2D::recomputeGeometry()
{
	//recompute axes geometry, but avoid the ones inside the left and right VipMultiAxisBase objects
	//if (titleAxis()->titleInside()) {
	// double spacing = topAxis()->isVisible() ? topAxis()->boundingRect().height() : 0;
	// if (topAxis()->constScaleDraw()->hasComponent(VipScaleDraw::Backbone))
	// spacing += topAxis()->constScaleDraw()->componentPen(VipScaleDraw::Backbone).widthF();
	// if (topAxis()->constScaleDraw()->hasComponent(VipScaleDraw::Ticks))
	// spacing += topAxis()->constScaleDraw()->tickLength(VipScaleDiv::MajorTick);
	// titleAxis()->setSpacing(spacing);
	// }
	// else
	// titleAxis()->setSpacing(0);
//
	// //starts by the left and right multi axes
	// d_data->yLeft->recomputeGeometry();
	// d_data->yRight->computeGeometry(true);
//
	// QList<VipBorderItem*> items = bottomAxis()->linkedBorderItems();
	// for (int i = 0; i < items.size(); ++i)
	// {
	// if(items[i] != d_data->yLeft && items[i] != d_data->yRight)
	// if (items[i]->innerOuterParent())
	//	items[i]->computeGeometry(true);
	// }
//
	// this->resetInnerLegendsPosition();
	// this->update();

	//TOCKECK
	VipPlotArea2D::recomputeGeometry();
}

void VipVMultiPlotArea2D::zoomOnSelection(const QPointF & start, const QPointF & end)
{
	//we only zoom horizontally, except if we only have one left axis

	QList<VipAbstractScale*> items = VipAbstractScale::independentScales(axes());

	QList<VipAbstractScale*> left_scales;
	for (int i = 0; i < items.size(); ++i) {
		if (VipBorderItem * it = qobject_cast<VipBorderItem*>(items[i]))
			if (zoomEnabled(it) && it->alignment() == VipBorderItem::Left)
				left_scales << items[i];
	}
	bool enable_v_zomm = left_scales.size() == 1;
	if (!enable_v_zomm) {
		QList<VipAbstractScale*> inter;
		//find the scale intersecting the area
		for (int i = 0; i < left_scales.size(); ++i) {
			QList<VipPlotCanvas*> c = vipCastItemList<VipPlotCanvas*>(left_scales[i]->plotItems());
			if (c.size() == 1) {
				QRectF r = c.first()->boundingRect();
				if (r.intersects(QRectF(start, end).normalized()))
					inter.append(left_scales[i]);
			}
		}
		if (inter.size() == 1) {
			enable_v_zomm = true;
			left_scales = inter;
		}
	}

	for (int i = 0; i < items.size(); ++i)
	{
		VipAbstractScale * axis = items[i];
		if (VipBorderItem * it = qobject_cast<VipBorderItem*>(axis)) {
			if (zoomEnabled(axis) && it->alignment() == VipBorderItem::Bottom) {
				QPointF axis_start = axis->mapFromItem(this, start);
				QPointF axis_end = axis->mapFromItem(this, end);
				VipInterval interval(axis->scaleDraw()->value(axis_start), axis->scaleDraw()->value(axis_end));
				interval = interval.normalized();
				axis->setScale(interval.minValue(), interval.maxValue());
			}
			else if (enable_v_zomm && zoomEnabled(axis) && it->alignment() == VipBorderItem::Left && left_scales.indexOf(axis) == 0) {
				QPointF axis_start = axis->mapFromItem(this, start);
				QPointF axis_end = axis->mapFromItem(this, end);
				VipInterval interval(axis->scaleDraw()->value(axis_start), axis->scaleDraw()->value(axis_end));
				interval = interval.normalized();
				axis->setScale(interval.minValue(), interval.maxValue());
			}
		}
	}

}

void VipVMultiPlotArea2D::zoomOnPosition(const QPointF & item_pos, double sc)
{
	//only zoom on a couple of axis

	//find the bottom/left axes involved
	QPointF mouse_pos = item_pos;// this->mapFromScene(scenePos);
	VipAxisBase *left = nullptr, *bottom = nullptr;
	for (int i = 0; i < d_data->yLeft->count(); ++i)
	{
		QPainterPath p = plotArea(qobject_cast<VipAxisBase*>(d_data->yLeft->at(i)));
		if (p.contains(mouse_pos)) {
			left = qobject_cast<VipAxisBase*>(d_data->yLeft->at(i));
			bottom = bottomAxis();
			break;
		}
	}


	QList<VipAbstractScale*> items;
	if (bottom) items << bottom << left;
	vip_double zoomValue = (sc - 1);

	for (int i = 0; i < items.size(); ++i)
	{
		VipAbstractScale * axis = items[i];
		if (zoomEnabled(axis))
		{
			vip_double pos = axis->scaleDraw()->value(axis->mapFromItem(this, item_pos));

			VipInterval interval = axis->scaleDiv().bounds();
			VipInterval new_interval(interval.minValue() + (pos - interval.minValue())*zoomValue,
				interval.maxValue() - (interval.maxValue() - pos)*zoomValue);
			axis->setScale(new_interval.minValue(), new_interval.maxValue());
		}
	}

}

void VipVMultiPlotArea2D::translate(const QPointF &, const QPointF & dp)
{
	//only zoom on a couple of axis

	//find the bottom/left axes involved
	QPointF mouse_pos = (this->lastMousePressPos());
	VipAxisBase *left = nullptr, *bottom = nullptr;
	for (int i = 0; i < d_data->yLeft->count(); ++i)
	{
		QPainterPath p = plotArea(qobject_cast<VipAxisBase*>(d_data->yLeft->at(i)));
		if (p.contains(mouse_pos)) {
			left = qobject_cast<VipAxisBase*>(d_data->yLeft->at(i));
			bottom = bottomAxis();
			break;
		}
	}

	QList<VipAbstractScale*> items;
	if (bottom) items << bottom << left;

	for (int i = 0; i < items.size(); ++i)
	{
		VipAxisBase * axis = static_cast<VipAxisBase*>(items[i]);
		if (zoomEnabled(axis))
		{

			vip_double start = axis->scaleDraw()->value(axis->scaleDraw()->pos() - dp);
			vip_double end = axis->scaleDraw()->value(axis->scaleDraw()->end() - dp);

			VipInterval interval(start, end);
			//keep the initial axis scale orientation
			if (axis->orientation() == Qt::Vertical)
				interval = interval.inverted();

			axis->setScale(interval.minValue(), interval.maxValue());
		}
	}
}


