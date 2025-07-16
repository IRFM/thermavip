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

#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QSharedPointer>
#include <QVector2D>

#include "VipBorderItem.h"
#include "VipBoxStyle.h"
#include "VipPainter.h"
#include "VipScaleDiv.h"
#include "VipScaleDraw.h"

QPoint sceneToScreenCoordinates(const QGraphicsScene* scene, const QPointF& pos)
{
	if (scene == nullptr)
		return QPoint();
	const QList<QGraphicsView*> views = scene->views();
	if (!views.isEmpty()			    // that scene is displayed in a view...
	    && views.first() != nullptr		    // ... which is not null...
	    && views.first()->viewport() != nullptr // ... and has a viewport
	) {
		const QGraphicsView* v = views.first();
		QPoint viewP = v->mapFromScene(pos);
		return v->viewport()->mapToGlobal(viewP);
	}

	return QPoint();
}

#include <QWindow>

QPointF screenToSceneCoordinates(const QGraphicsScene* scene, const QPoint& pos)
{
	if (scene == nullptr)
		return QPoint();
	const QList<QGraphicsView*> views = scene->views();
	if (!views.isEmpty()			    // that scene is displayed in a view...
	    && views.first() != nullptr		    // ... which is not null...
	    && views.first()->viewport() != nullptr // ... and has a viewport
	) {
		const QGraphicsView* v = views.first();
		QPoint viewP = v->mapFromGlobal(pos); // v->mapFromScene(pos);
		return v->mapToScene(viewP);
	}

	return QPointF();
}

class VipBorderItem::PrivateData
{
public:
	PrivateData(VipBorderItem::Alignment pos)
	  : extent(0)
	  , intersectWith()
	  , intersectValue(0)
	  , intersectValueType(Vip::Absolute)
	  , innerItem(nullptr)
	  , alignment(pos)
	  , expandToCorners(0)
	  , canvasProximity(0)
	  , dirtyGlobalSceneTransform(1)
	  , dirtyParentTransform(1)
	{
	}

	double extent;

	QPointer<VipBorderItem> intersectWith;
	double intersectValue;
	Vip::ValueType intersectValueType;

	QGraphicsItem* innerItem;
	Alignment alignment;
	bool expandToCorners;
	int canvasProximity;
	QRectF boundingRect;
	QRectF boundingRectNoCorners;

	bool dirtyGlobalSceneTransform;
	QTransform globalSceneTransform;

	bool dirtyParentTransform;
	QTransform parentTransform;

	// geometry updates based on inner/outer item geometry changes
	QPointF watchedPos;
	QRectF watchedRect;
};

static bool registerVipBorderItem = vipSetKeyWordsForClass(&VipBorderItem::staticMetaObject);

VipBorderItem::VipBorderItem(Alignment pos, QGraphicsItem* parent)
  : VipAbstractScale(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data,pos);
	this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
	this->setAlignment(pos);
}

VipBorderItem::~VipBorderItem()
{
}

void VipBorderItem::setExpandToCorners(bool expand)
{
	d_data->expandToCorners = expand;
	this->emitGeometryNeedUpdate();
}

bool VipBorderItem::expandToCorners() const
{
	return d_data->expandToCorners;
}

void VipBorderItem::setAxisIntersection(VipBorderItem* other, double other_value, Vip::ValueType type)
{
	d_data->intersectWith = other;
	d_data->intersectValue = other_value;
	d_data->intersectValueType = type;
	emitGeometryNeedUpdate();
}

VipBorderItem* VipBorderItem::axisIntersection() const
{
	return d_data->intersectWith;
}

Vip::ValueType VipBorderItem::axisIntersectionType() const
{
	return d_data->intersectValueType;
}

double VipBorderItem::axisIntersectionValue() const
{
	return d_data->intersectValue;
}

void VipBorderItem::disableAxisIntersection()
{
	d_data->intersectWith = nullptr;
	d_data->intersectValue = 0;
	emitGeometryNeedUpdate();
}

bool VipBorderItem::axisIntersectionEnabled() const
{
	return d_data->intersectWith != nullptr;
}

void VipBorderItem::emitScaleDivNeedUpdate()
{
	// Recompute geometry if axis intersection is wrong
	if (VipBorderItem* inter = d_data->intersectWith) {
		if (inter->parentItem() == this->parentItem()) {
			// grab the theoric 'good' position computed in VipPlotWidget2D.cpp when recomputing the area geometry
			QPointF theoric_pos = this->property("_vip_Pos").value<QPointF>();

			if (this->orientation() == Qt::Vertical) {
				double x = inter->position(this->axisIntersectionValue(), 0, this->axisIntersectionType()).x() + inter->pos().x();
				if (x != theoric_pos.x())
					emitGeometryNeedUpdate();
			}
			else {
				double y = inter->position(this->axisIntersectionValue(), 0, this->axisIntersectionType()).y() + inter->pos().y();
				if (y != theoric_pos.y())
					this->emitGeometryNeedUpdate();
			}
		}
	}
	VipAbstractScale::emitScaleDivNeedUpdate();
}

bool VipBorderItem::hasState(const QByteArray& state, bool enable) const
{
	if (state == ("left")) {
		bool is_side = alignment() == Left;
		return (is_side == enable);
	}
	else if (state == ("top")) {
		bool is_side = alignment() == Top;
		return (is_side == enable);
	}
	else if (state == ("right")) {
		bool is_side = alignment() == Right;
		return (is_side == enable);
	}
	else if (state == ("bottom")) {
		bool is_side = alignment() == Bottom;
		return (is_side == enable);
	}
	return VipAbstractScale::hasState(state, enable);
}

void VipBorderItem::setAlignment(Alignment align)
{
	if (d_data->alignment != align) {
		d_data->alignment = align;
		this->markStyleSheetDirty();
		this->emitGeometryNeedUpdate();
	}
}

VipBorderItem::Alignment VipBorderItem::alignment() const
{
	return d_data->alignment;
}

Qt::Orientation VipBorderItem::orientation() const
{
	Alignment align = alignment();
	if (align == Top || align == Bottom)
		return Qt::Horizontal;
	else
		return Qt::Vertical;
}

void VipBorderItem::setCanvasProximity(int proximity)
{
	if (d_data->canvasProximity != proximity) {
		d_data->canvasProximity = proximity;
		emitGeometryNeedUpdate();
	}
}

int VipBorderItem::canvasProximity() const
{
	return d_data->canvasProximity;
}

QRectF VipBorderItem::boundingRectNoCorners() const
{
	if (d_data->boundingRectNoCorners == QRectF()) {
		if (isVisible() && area())
			const_cast<VipBorderItem*>(this)->emitGeometryNeedUpdate();
	}

	return d_data->boundingRectNoCorners;
}

void VipBorderItem::setBoundingRectNoCorners(const QRectF& r)
{
	d_data->boundingRectNoCorners = r;
}

int VipBorderItem::hscrollBarHeight(const QGraphicsView* view)
{
	return (view->horizontalScrollBar()->isVisible()) ? view->horizontalScrollBar()->frameGeometry().height() + 2 : 0;
}

int VipBorderItem::vscrollBarWidth(const QGraphicsView* view)
{
	return (view->verticalScrollBar()->isVisible()) ? view->verticalScrollBar()->frameGeometry().width() + 2 : 0;
}

QRectF VipBorderItem::visualizedSceneRect(const QGraphicsView* view)
{
	// to receive the currently visible area, map the widget bounds to the scene
	QPointF topLeft = view->mapToScene(0, 0);
	int w = view->width();
	int h = view->height();
	QPointF bottomRight = view->mapToScene(w - vscrollBarWidth(view) //-3
					       ,
					       h - hscrollBarHeight(view) //-3
	);
	return QRectF(topLeft, bottomRight);
}

QTransform VipBorderItem::globalSceneTransform() const
{
	if (d_data->dirtyGlobalSceneTransform) {
		d_data->globalSceneTransform = VipAbstractScale::globalSceneTransform(this);
		d_data->dirtyGlobalSceneTransform = 0;
	}

	return d_data->globalSceneTransform;
}

QTransform VipBorderItem::parentTransform() const
{
	if (d_data->dirtyParentTransform) {
		d_data->parentTransform = VipAbstractScale::parentTransform(this);
		d_data->dirtyParentTransform = 0;
	}

	return d_data->parentTransform;
}

double VipBorderItem::axisRangeToItemUnit(vip_double dist) const
{
	if (alignment() == Right || alignment() == Left) {
		return qAbs((scaleDraw()->position(dist) - scaleDraw()->position(0)).y());
	}
	else {
		return qAbs((scaleDraw()->position(dist) - scaleDraw()->position(0)).x());
	}
}

vip_double VipBorderItem::itemRangeToAxisUnit(double dist) const
{
	if (alignment() == Right || alignment() == Left) {
		return qAbs((scaleDraw()->value(QPointF(0, dist)) - scaleDraw()->value(QPointF(0, 0))));
	}
	else {
		return qAbs((scaleDraw()->value(QPointF(dist, 0)) - scaleDraw()->value(QPointF(0, 0))));
	}
}

double VipBorderItem::mapFromView(QGraphicsView* view, int length)
{
	return view->mapToScene(QPoint(length, 0.)).x() - view->mapToScene(QPoint(0, 0.)).x();
}

int VipBorderItem::mapToView(QGraphicsView* view, double length)
{
	return view->mapFromScene(QPointF(length, 0.)).x() - view->mapFromScene(QPointF(0., 0.)).x();
}

QVariant VipBorderItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemTransformChange || change == QGraphicsItem::ItemRotationChange || change == QGraphicsItem::ItemScaleChange || change == QGraphicsItem::ItemPositionChange) {
		d_data->dirtyGlobalSceneTransform = 1;
		d_data->dirtyParentTransform = 1;
		this->emitGeometryNeedUpdate();
	}

	return VipAbstractScale::itemChange(change, value);
}
