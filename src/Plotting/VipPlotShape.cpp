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

#include "VipPlotShape.h"
#include "VipLock.h"
#include "VipPainter.h"
#include "VipResizeItem.h"
#include "VipSet.h"
#include "VipShapeDevice.h"
#include "VipSimpleAnnotation.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainterPathStroker>
#include <QTransform>
#include <qapplication.h>
#include <qthread.h>

#include <atomic>
#include <cmath>
#include <limits>
// static QList<QPolygonF> editablePolygon(const QPainterPath & path)
// {
// QPainterPath pa = path.simplified();
// QList<QPolygonF> p = pa.toFillPolygons();
// for(int i=0; i< p.size(); ++i)
// p[i].remove(p[i].size()-1);
// return p;
// }
//
// static QPainterPath toPainterPath(const QList<QPolygonF> & polygons)
// {
// QPainterPath p;
// for(int i=0; i < polygons.size(); ++i)
// {
// QPolygonF tmp = polygons[i];
// //close polygon if necessary
// if(tmp.size())
// {
// if(tmp.back() != tmp.front())
// tmp.push_back(tmp.front());
// }
// p.addPolygon(tmp);
// }
//
// return p;
// }

/// Extract all angles defined by polylines (in degree)
// static QVector<double> ExtractAngles(const QVector<QPointF> points)
// {
// QVector<double> res;
// for(int i=0; i < points.size()-2; ++i)
// {
// //compute the two vectors
// QPointF vec1( points[i+1].x() - points[i].x(), points[i+1].y() - points[i].y() );
// QPointF vec2( points[i+2].x() - points[i+1].x(), points[i+2].y() - points[i+1].y() );
// //normalize vectors
// vec1 /= std::sqrt(vec1.x()*vec1.x() +  vec1.y()*vec1.y());
// vec2 /= std::sqrt(vec2.x()*vec2.x() +  vec2.y()*vec2.y());
// //compute angle ( acos(vec1.vec2) )
// res.append( 180 - (std::acos(vec1.x()*vec2.x() +  vec1.y() * vec2.y())) / 6.28318530718 * 360 );
// }
//
// return res ;
// }

class PolygonPointsMover : public QGraphicsItem
{
	VipPlotShape* m_shape;
	int m_poly;
	int m_point;
	bool m_changed;
	bool m_hasChanged;

public:
	PolygonPointsMover(VipPlotShape* item)
	  : QGraphicsItem(item)
	  , m_shape(item)
	  , m_poly(-1)
	  , m_point(-1)
	  , m_changed(false)
	  , m_hasChanged(false)
	{
		this->setFlag(ItemIsFocusable, true);
		this->setFlag(ItemIsSelectable, true);
		this->setAcceptHoverEvents(true);
		this->setCursor(Qt::CrossCursor);
	}

	void prepareGeometryChange() { QGraphicsItem::prepareGeometryChange(); }

	QList<QPolygonF> polygons() const
	{
		// compute the list of polygons used to represent this shape
		QList<QPolygonF> poly;
		if (m_shape->rawData().type() == VipShape::Polygon)
			poly.append(m_shape->rawData().polygon());
		else if (m_shape->rawData().type() == VipShape::Polyline)
			poly.append(m_shape->rawData().polyline());
		else if (m_shape->rawData().isPolygonBased())
			poly = m_shape->rawData().shape().toSubpathPolygons();

		// for each polygon, remove the last point if equals to first point
		for (int i = 0; i < poly.size(); ++i) {
			if (poly[i].size() && poly[i].last() == poly[i].first())
				poly[i].remove(poly[i].size() - 1);
		}
		return poly;
	}

	void setShape(const QList<QPolygonF>& polygons)
	{
		VipShape sh = m_shape->rawData();
		if (sh.type() == VipShape::Polygon)
			sh.setPolygon(polygons[0]);
		else if (sh.type() == VipShape::Polyline)
			sh.setPolyline(polygons[0]);
		else if (sh.type() == VipShape::Path && sh.isPolygonBased()) {
			// recreate the path based on the polygons
			QPainterPath path;
			for (int i = 0; i < polygons.size(); ++i) {
				const QPolygonF p = polygons[i];
				if (polygons[i].size() > 1 && polygons[i].last() != polygons[i].first())
					path.addPolygon(QPolygonF(polygons[i]) << polygons[i].first());
				else
					path.addPolygon(polygons[i]);
			}
			sh.setShape(path, VipShape::Path, true);
		}
		m_shape->setRawData(sh);
	}

	virtual QPainterPath shape() const
	{
		QPainterPath res;
		res.setFillRule(Qt::WindingFill);
		// compute m_resizer
		if (m_shape->isSelected()) {
			QRectF resizer = QRectF(0, 0, 9, 9);

			const QList<QPolygonF> ps = polygons();
			for (int i = 0; i < ps.size(); ++i) {
				// compute the final shape by adding successive polygons
				const QPolygonF p = m_shape->sceneMap()->transform(ps[i]);
				// create the point movers
				for (int j = 0; j < p.size(); ++j) {
					resizer.moveCenter(p[j]);
					res.addRect(resizer);
				}
			}
		}
		return res;
	}

	virtual QRectF boundingRect() const { return shape().boundingRect(); }

	virtual void paint(QPainter* painter,
			   const QStyleOptionGraphicsItem* // option
			   ,
			   QWidget* // widget
			   = 0)
	{
		if (m_shape->testItemAttribute(VipResizeItem::ClipToScaleRect))
			painter->setClipPath(m_shape->sceneMap()->clipPath(m_shape), Qt::IntersectClip);

		painter->setPen(Qt::black);
		painter->setBrush(Qt::yellow);
		painter->drawPath(shape());

		// draw first id
		// const QList<QPolygonF> ps = polygons();
		// if (ps.size()) {
		// for (int i = 0; i < ps.size(); ++i) {
		// const QPolygonF poly = ps[i];
		// if (poly.size() > 1) {
		//	for (int j = 0; j < poly.size(); ++j) {
		//
		//		QPointF pt = m_shape->sceneMap()->transform(poly[j]);
		//		painter->drawText(pt, QString::number(j));
		//	}
		// }
		// }
		//
		// }
	}

	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event)
	{
		if (m_point >= 0 && m_poly >= 0) {

			if (m_changed) {
				Q_EMIT m_shape->aboutToChangePoints();
				m_changed = false;
			}
			m_hasChanged = true;

			// update the shape based on the new point position
			QPointF new_pos = m_shape->sceneMap()->invTransform((event)->pos());
			QList<QPolygonF> poly = polygons();
			poly[m_poly][m_point] = new_pos;
			setShape(poly);
			this->update();
		}
		else
			event->ignore();
	}

	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event)
	{
		QList<QPolygonF> polygons = this->polygons();
		QPointF pos = (event)->pos();
		m_point = -1;
		m_poly = -1;
		m_changed = true;

		if (m_shape->isSelected()) {
			// find the selected point
			for (int i = 0; i < polygons.size(); ++i) {
				const QPolygonF poly = m_shape->sceneMap()->transform(polygons[i]);
				QRectF resizer = QRectF(0, 0, 9, 9);
				for (int j = 0; j < poly.size(); ++j) {
					resizer.moveCenter(poly[j]);
					if (resizer.contains(pos)) {
						m_point = j;
						m_poly = i;
						break;
					}
				}
				if (m_point >= 0)
					break;
			}
		}

		if (m_point >= 0)
			Q_EMIT m_shape->aboutToChangePoints();

		if (m_point >= 0 && event->button() == Qt::RightButton) {
			// display a menu to remove or add a point
			QMenu menu;
			QAction* add = menu.addAction("Add point");
			QAction* del = polygons[m_poly].size() > 2 ? menu.addAction("Remove point") : nullptr;
			QAction* res = menu.exec(QCursor::pos());
			// add or remove a point
			if (res && m_point >= 0 && m_point < polygons[m_poly].size()) {
				bool change = false;
				if (del && res == del) {
					polygons[m_poly].remove(m_point);
					change = true;
				}
				else if (res == add) {
					polygons[m_poly].insert(m_point, polygons[m_poly].at(m_point));
					change = true;
				}
				m_point = -1;
				m_poly = -1;

				if (change) {
					Q_EMIT m_shape->aboutToChangePoints();
					setShape(polygons);
					this->update();

					Q_EMIT m_shape->finishedChangePoints();
				}
			}
		}
		else if (m_point < 0)
			event->ignore();

		this->setCursor(Qt::CrossCursor);
	}

	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* // event
	)
	{
		this->setCursor(Qt::CrossCursor);
		m_changed = false;
		if (m_hasChanged) {
			m_hasChanged = false;
			Q_EMIT m_shape->finishedChangePoints();
		}
	}

	virtual void keyPressEvent(QKeyEvent* event)
	{
		if ((event)->key() == Qt::Key_Delete)
			removeSelectedPoint();
		else
			event->ignore();
	}

	void removeSelectedPoint()
	{
		if (m_poly < 0 || m_point < 0)
			return;

		QList<QPolygonF> polygons = this->polygons();
		if (m_poly >= polygons.size() || m_point >= polygons[m_poly].size() || polygons[m_poly].size() <= 2)
			return;

		Q_EMIT m_shape->aboutToChangePoints();

		// suppress selected point
		polygons[m_poly].remove(m_point);
		setShape(polygons);
		this->update();
		m_point = -1;
		m_poly = -1;

		Q_EMIT m_shape->finishedChangePoints();
	}
};

static const QMap<QByteArray, int>& drawComponentValues()
{
	static QMap<QByteArray, int> component;
	if (component.isEmpty()) {

		component["border"] = VipPlotShape::Border;
		component["background"] = VipPlotShape::Background;
		component["fillPixels"] = VipPlotShape::FillPixels;
		component["id"] = VipPlotShape::Id;
		component["group"] = VipPlotShape::Group;
		component["title"] = VipPlotShape::Title;
		component["attributes"] = VipPlotShape::Attributes;
	}
	return component;
}

static int registerShapeKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["components"] = VipParserPtr(new EnumOrParser(drawComponentValues()));
		keywords["component"] = VipParserPtr(new BoolParser());
		keywords["text-alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["text-position"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::regionPositionEnum()));
		keywords["text-distance"] = VipParserPtr(new DoubleParser());
		keywords["polygon-editable"] = VipParserPtr(new BoolParser());
		keywords["adjust-text-color"] = VipParserPtr(new BoolParser());

		vipSetKeyWordsForClass(&VipPlotShape::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerShapeKeyWords = registerShapeKeyWords();

class VipPlotShape::PrivateData
{
public:
	PrivateData()
	  : textDistance(0)
	  , components(Border | Background | Id)
	  , textPosition(Vip::XInside)
	  , textAlignment(Qt::AlignLeft | Qt::AlignBottom)
	  , adjustTextColor(true)
	  , polygonMovers(nullptr)
	  , annotation(nullptr)
	{
	}

	double textDistance;
	QTransform textTransform;
	QPointF textTransformReference;

	QPen pen;
	QBrush brush;
	QPainterPath path;
	QRectF textRect;

	DrawComponents components;
	Vip::RegionPositions textPosition;
	Qt::Alignment textAlignment;
	bool adjustTextColor;

	std::unique_ptr<PolygonPointsMover> polygonMovers;
	std::unique_ptr < VipAnnotation> annotation;
	QByteArray annotationData;

	VipText text;
	QSharedPointer<VipTextStyle> textStyle;
};

VipPlotShape::VipPlotShape(const VipText& title)
  : VipPlotItemDataType(title)
  , d_data(new PrivateData())
{
	this->setFlag(QGraphicsItem::ItemIsFocusable, true);
	this->setItemAttribute(AutoScale, false);
	this->setItemAttribute(SupportTransform, true);
	this->setItemAttribute(VisibleLegend, false);
	this->setItemAttribute(HasLegendIcon, false);
	this->setItemAttribute(IgnoreMouseEvents, true);
	this->setItemAttribute(HasToolTip, false);
	this->setRenderHints(QPainter::Antialiasing);
	this->selectedDevice()->setDrawPrimitive(VipShapeDevice::Text, false);
}

VipPlotShape::~VipPlotShape()
{
	d_data->polygonMovers.reset();
	d_data->annotation.reset();

	Q_EMIT plotShapeDestroyed(this);
}

void VipPlotShape::setAnnotation(VipAnnotation* annot)
{
	if (d_data->annotation.get() != annot) {
		d_data->annotation.reset( annot);
		if (annot)
			annot->setParentShape(this);

		QByteArray ar = rawData().attribute("_vip_annotation").toByteArray();
		QByteArray new_ar = annot ? vipSaveAnnotation(annot) : QByteArray();
		d_data->annotationData = new_ar;
		if (new_ar != ar)
			rawData().setAttribute("_vip_annotation", new_ar);

		emitItemChanged();
	}
}
VipAnnotation* VipPlotShape::annotation() const
{
	QVariant an = rawData().attribute("_vip_annotation");
	if (an.userType() == 0)
		return d_data->annotation.get();
	else {
		QByteArray ar = an.toByteArray();
		if (ar != d_data->annotationData) {
			VipPlotShape* _this = const_cast<VipPlotShape*>(this);
			_this->d_data->annotation.reset(); 
			if (VipAnnotation* annot = vipLoadAnnotation(ar)) {
				_this->d_data->annotation.reset(annot);
				_this->d_data->annotationData = ar;
				annot->setParentShape(_this);
			}
		}
	}
	return d_data->annotation.get();
}

void VipPlotShape::setDrawComponents(DrawComponents components)
{
	if (d_data->components != components) {
		d_data->components = components;
		emitItemChanged();
	}
}

void VipPlotShape::setDrawComponent(DrawComponent c, bool on)
{
	if (d_data->components.testFlag(c) != on) {
		if (on)
			d_data->components |= c;
		else
			d_data->components &= ~c;

		emitItemChanged();
	}
}

bool VipPlotShape::testDrawComponent(DrawComponent c) const
{
	return d_data->components.testFlag(c);
}

VipPlotShape::DrawComponents VipPlotShape::drawComponents() const
{
	return d_data->components;
}

void VipPlotShape::setAdjustTextColor(bool enable)
{
	if (enable != d_data->adjustTextColor) {
		d_data->adjustTextColor = enable;
		emitItemChanged();
	}
}
bool VipPlotShape::adjustTextColor() const
{
	return d_data->adjustTextColor;
}

void VipPlotShape::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

const VipText& VipPlotShape::text() const
{
	return d_data->text;
}

void VipPlotShape::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->text.setTextStyle(st);
	emitItemChanged();
}

VipTextStyle VipPlotShape::textStyle() const
{
	return d_data->textStyle ? *d_data->textStyle : VipTextStyle();
}

void VipPlotShape::setTextPosition(Vip::RegionPositions pos)
{
	if (d_data->textPosition != pos) {
		d_data->textPosition = pos;
		emitItemChanged();
	}
}

Vip::RegionPositions VipPlotShape::textPosition() const
{
	return d_data->textPosition;
}

void VipPlotShape::setTextAlignment(Qt::Alignment align)
{
	if (d_data->textAlignment != align) {
		d_data->textAlignment = align;
		emitItemChanged();
	}
}

Qt::Alignment VipPlotShape::textAlignment() const
{
	return d_data->textAlignment;
}

QString VipPlotShape::formatText(const QString& text, const QPointF& pos) const
{
	static QRegExp _reg("#(\\w+)");

	const VipShape sh = rawData();
	QString res = VipText::replace(text, "#id", sh.id());
	res = VipText::replace(res, "#group", sh.group());
	int offset = 0;
	int index = _reg.indexIn(res, offset);
	const auto attrs = sh.attributes();

	while (index >= 0) {
		QString full = res.mid(index, _reg.matchedLength());

		if (full.startsWith("#p")) {
			full.remove("#p");
			auto it = attrs.find(full);

			if (it != attrs.end()) {
				const QVariant var = it.value();
				bool ok = false;
				double val = var.toDouble(&ok);
				if (!ok)
					res.replace(index, _reg.matchedLength(), var.toString());
				else
					res = VipText::replace(res, "#p" + full, val);
			}
			else
				++index;
		}
		else
			++index;

		index = _reg.indexIn(res, index);
	}

	return VipPlotItem::formatText(res, pos);
}

QString VipPlotShape::formatToolTip(const QPointF& pos) const
{
	return formatText(toolTipText(), pos);
}

bool VipPlotShape::areaOfInterest(const QPointF& pos, int, double, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const
{
	QPainterPath sh = shape();
	if (sh.contains(pos)) {
		out_pos.push_back(pos);
		legend = 0;
		style.computePath(sh);
		return true;
	}
	return false;
}

QList<VipInterval> VipPlotShape::plotBoundingIntervals() const
{
	return VipInterval::fromRect(this->rawData().boundingRect());
}

QRectF VipPlotShape::boundingRect() const
{
	return shape().boundingRect() | d_data->textRect;
}

QPainterPath VipPlotShape::shape() const
{
	QPainterPath additional;
	if (VipAnnotation* annot = annotation())
		additional = annot->shape(rawData(), sceneMap());

	// empty d_data->path if a point is outside the scale area or
	// it won't be drawn
	if (rawData().type() == VipShape::Point && !VipInterval::toRect(this->VipPlotItem::plotBoundingIntervals()).contains(rawData().point()))
		const_cast<QPainterPath&>(d_data->path) = QPainterPath();

	if (!d_data->path.isEmpty())
		return d_data->path;
	else {
		VipShape sh = rawData();
		QPainterPath path;

		if (sh.type() == VipShape::Point) {
			// rectangle around the point
			QRectF rect(0, 0, 7, 7);
			rect.moveCenter((QPointF)sceneMap()->transform(sh.shape().currentPosition()));
			path.addRect(rect);
		}
		else if (sh.type() == VipShape::Polygon) {
			path = sceneMap()->transform(sh.shape());
		}
		else if (sh.type() == VipShape::Polyline) {
			QPolygonF polyline = sh.polyline();

			if (testDrawComponent(VipPlotShape::Border) && polyline.size()) {
				if (testDrawComponent(VipPlotShape::FillPixels)) {
					QVector<QPoint> points = sh.fillPixels();
					QPolygonF polygon;
					// TEST: remove Qt::WindingFill
					path.setFillRule(Qt::WindingFill);
					for (int i = 0; i < points.size(); ++i) {
						QRectF pixel(points[i].x(), points[i].y(), 1, 1);
						pixel = sceneMap()->transformRect(pixel);
						path.addRect(pixel.adjusted(-2, -2, 2, 2));
					}
				}
				else {
					QPainterPathStroker stroker;
					stroker.setWidth(5);
					QPolygonF polygon = sceneMap()->transform(polyline);
					path.addPolygon(polygon);
					path = stroker.createStroke(path);
				}
			}
		}
		else {
			if (!sh.shape().isEmpty())
				path = sceneMap()->transform(sh.shape());
		}
		QRectF r1 = path.boundingRect();
		QRectF r2 = additional.boundingRect();
		if (vipIsNan(r1.left()) || vipIsNan(r2.left()) || vipIsNan(r1.top()) || vipIsNan(r2.top()))
			return QPainterPath();

		return path | additional;
	}
	VIP_UNREACHABLE();
	// return QPainterPath();
}

void VipPlotShape::setPolygonEditable(bool editable)
{
	if (editable && !polygonEditable() && (rawData().isPolygonBased())) {
		if (!d_data->polygonMovers)
			d_data->polygonMovers.reset(new PolygonPointsMover(this));
		else
			d_data->polygonMovers->setVisible(true);
		emitItemChanged();
	}
	else if (d_data->polygonMovers && !editable) {
		// delete d_data->polygonMovers;
		// d_data->polygonMovers = nullptr;
		d_data->polygonMovers->setVisible(false);
		emitItemChanged();
	}
}

bool VipPlotShape::polygonEditable() const
{
	return d_data->polygonMovers && d_data->polygonMovers->isVisible();
}

void VipPlotShape::setPen(const QPen& pen)
{
	if (d_data->pen != pen) {
		d_data->pen = pen;
		emitItemChanged();
	}
}

QPen VipPlotShape::pen() const
{
	return d_data->pen;
}

void VipPlotShape::setBrush(const QBrush& brush)
{
	if (d_data->brush != brush) {
		d_data->brush = brush;
		emitItemChanged();
	}
}

QBrush VipPlotShape::brush() const
{
	return d_data->brush;
}

void VipPlotShape::setTextTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform = tr;
	d_data->textTransformReference = ref;
	emitItemChanged();
}
const QTransform& VipPlotShape::textTransform() const
{
	return d_data->textTransform;
}
const QPointF& VipPlotShape::textTransformReference() const
{
	return d_data->textTransformReference;
}

void VipPlotShape::setTextDistance(double distance)
{
	d_data->textDistance = distance;
	emitItemChanged();
}

double VipPlotShape::textDistance() const
{
	return d_data->textDistance;
}

void VipPlotShape::setData(const QVariant& value)
{
	VipPlotItemDataType::setData(value);

	if (QThread::currentThread() == qApp->thread())
		internalUpdateOnSetData();
	else
		QMetaObject::invokeMethod(this, "internalUpdateOnSetData", Qt::QueuedConnection);
}

void VipPlotShape::internalUpdateOnSetData()
{
	if (d_data->polygonMovers)
		d_data->polygonMovers->update();

	QByteArray annot = rawData().attribute("_vip_annotation").toByteArray();
	if (annot != d_data->annotationData) {
		if (annot.isEmpty()) {
			d_data->annotation.reset();
			d_data->annotationData.clear();
		}
		if (VipAnnotation* a = vipLoadAnnotation(annot)) {
			d_data->annotation.reset(a);
			d_data->annotationData = annot;
			a->setParentShape(this);
		}
	}
}

bool VipPlotShape::applyTransform(const QTransform& tr)
{
	this->rawData().transform(tr);
	return true;
}

QRectF VipPlotShape::drawLegend(QPainter* p, const QRectF& r, int) const
{
	double w = pen().widthF() / 2;
	QRectF rect = r.adjusted(w, w, -w, -w);
	p->setPen(pen());
	p->setBrush(brush());
	p->drawRect(rect);
	return rect;
}

void VipPlotShape::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	if (VipAnnotation* annot = annotation())
		return annot->draw(rawData(), painter, m);

	d_data->path = QPainterPath();
	d_data->textRect = QRectF();

	VipShape sh = this->rawData();

	// draw the shape itself
	if (sh.type() == VipShape::Path)
		drawPath(painter, m, sh);
	else if (sh.type() == VipShape::Polygon)
		drawPolygon(painter, m, sh);
	else if (sh.type() == VipShape::Polyline)
		drawPolyline(painter, m, sh);
	else if (sh.type() == VipShape::Point)
		drawPoint(painter, m, sh);

	VipText text;

	if (d_data->text.isEmpty()) {

		// draw the title and id
		text = VipText(QString(), textStyle());
		QString name = sh.attribute("Name").toString();
		if (!name.isEmpty()) {
			// use the "Name" attribute if possible
			text.setText(name);
		}
		else {
			if (testDrawComponent(VipPlotShape::Id))
				text.setText(QString::number(sh.id()));
			if (testDrawComponent(VipPlotShape::Group))
				text.setText(text.text() + (text.text().isEmpty() ? "" : " ") + sh.group());
			if (testDrawComponent(VipPlotShape::Title))
				text.setText(text.text() + (text.text().isEmpty() ? "" : " ") + title().text());
		}

		if (testDrawComponent(Attributes)) {
			// add attributes
			QString t = text.text();
			static const QString name_key = "Name";
			const QVariantMap attrs = rawData().attributes();
			for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
				if (it.key() != name_key && !it.key().startsWith("_vip_"))
					t += "\n" + it.key() + ": " + it.value().toString();
			}
			text.setText(t);
		}
	}
	else {

		text = d_data->text;
		text.setText(this->formatText(text.text(), QPointF()));
	}

	if (!text.text().isEmpty()) {
		if (text.textStyle().textPen().style() == Qt::NoPen && text.textStyle().boxStyle().isTransparent())
			return;

		QRectF shape_rect = shape().boundingRect();
		{
			VipShapeDevice device;
			QPainter p(&device);
			VipPainter::drawText(&p, text, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), shape_rect);
			p.end();
			d_data->textRect = device.shape().boundingRect();
		}

		VipText t = text;
		const bool is_opengl = VipPainter::isOpenGL(painter);
		if (d_data->adjustTextColor && !is_opengl) {
			painter->save();
			t.setTextPen(QPen(Qt::white));
			painter->setCompositionMode(QPainter::CompositionMode_Difference);
		}

		VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), shape_rect);

		if (d_data->adjustTextColor && !is_opengl)
			painter->restore();
	}
}

void VipPlotShape::drawPath(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const
{
	if (testDrawComponent(VipPlotShape::Border))
		painter->setPen(d_data->pen);
	else
		painter->setPen(Qt::NoPen);

	if (testDrawComponent(VipPlotShape::Background))
		painter->setBrush(d_data->brush);
	else
		painter->setBrush(QBrush());

	if (testDrawComponent(VipPlotShape::FillPixels)) {
		// draw the exact pixels
		QList<QPolygon> outline = sh.outlines();
		for (int i = 0; i < outline.size(); ++i)
			painter->drawPolygon(m->transform(QPolygonF(outline[i])));
	}
	else {
		// draw the full shape
		QPainterPath path = sh.shape();
		// TEST: remove Qt::WindingFill
		// path.setFillRule(Qt::WindingFill);
		painter->setRenderHint(QPainter::Antialiasing);
		VipPainter::drawPath(painter, m->transform(path));
	}
}

void VipPlotShape::drawPolygon(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const
{
	return drawPath(painter, m, sh);
}

void VipPlotShape::drawPolyline(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const
{
	QPolygonF polyline = sh.polyline();
	QPolygonF polygon = m->transform(polyline);

	if (testDrawComponent(VipPlotShape::Border) && polygon.size()) {
		painter->setPen(Qt::red);
		painter->setBrush(QBrush());

		// draw a circle around the start point and a smaller one around the end point

		QRectF big_ellipse = ellipseAroundPixel(polyline.first(), QSizeF(9, 9), m);
		painter->drawEllipse(big_ellipse);

		QRectF small_ellipse = ellipseAroundPixel(polyline.last(), QSizeF(7, 7), m);
		painter->drawEllipse(small_ellipse);

		if (testDrawComponent(VipPlotShape::FillPixels)) {
			// painter->setPen(brush().color());
			QVector<QPoint> points = sh.fillPixels();

			painter->setBrush(brush());
			painter->setPen(Qt::NoPen);

			QPolygonF poly;
			for (int i = 0; i < points.size(); ++i) {
				QRectF pixel(points[i].x(), points[i].y(), 1, 1);
				poly = m->transform(pixel);
				VipPainter::drawPolygon(painter, poly);
			}
		}
		else {
			painter->setPen(pen());
			VipPainter::drawPolyline(painter, polygon);
		}
	}
}

void VipPlotShape::drawPoint(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const
{
	QPainterPath path;
	// TEST: remove Qt::WindingFill
	path.setFillRule(Qt::WindingFill);

	QRectF pix;
	if (testDrawComponent(VipPlotShape::FillPixels)) {
		// draw a rectangle around the pixel
		QPointF p(std::floor(sh.point().x()), std::floor(sh.point().y()));
		QRectF pixel(p.x(), p.y(), 1, 1);
		QPolygonF poly = m->transform(pixel);
		pix = poly.boundingRect();

		painter->setBrush(brush());
		painter->setPen(QPen());

		if (pix.width() < 9) {
			painter->setRenderHints(QPainter::Antialiasing);
			QRectF ellipse(0, 0, 9, 9);
			ellipse.moveCenter(pix.center());
			painter->drawEllipse(ellipse);
			path.addEllipse(ellipse);
			pix = ellipse;
		}
		else {
			painter->setRenderHints(QPainter::Antialiasing, false);
			VipPainter::drawPolygon(painter, poly);
			path.addPolygon(poly);
		}
	}
	else {
		painter->setRenderHints(QPainter::Antialiasing);

		// draw an ellipse around the point
		QPointF p = sh.point(); // .toPoint();
		QRectF pixel(p.x(), p.y(), 1, 1);
		QRectF ellipse(0, 0, 9, 9);
		ellipse.moveCenter(m->transform(pixel).boundingRect().center());
		painter->setBrush(brush());
		painter->setPen(QPen());
		painter->drawEllipse(ellipse);
		pix = ellipse;
	}

	if (testDrawComponent(VipPlotShape::Border)) {
		painter->setRenderHints(QPainter::Antialiasing, false);
		painter->setPen(pen());

		QLineF left(QPointF(pix.left() - 1, pix.center().y()), QPointF(pix.left() - 7, pix.center().y()));
		QLineF right(QPointF(pix.right(), pix.center().y()), QPointF(pix.right() + 7, pix.center().y()));
		QLineF top(QPointF(pix.center().x(), pix.top() - 1), QPointF(pix.center().x(), pix.top() - 7));
		QLineF bottom(QPointF(pix.center().x(), pix.bottom()), QPointF(pix.center().x(), pix.bottom() + 7));

		VipPainter::drawLine(painter, left);
		VipPainter::drawLine(painter, right);
		VipPainter::drawLine(painter, top);
		VipPainter::drawLine(painter, bottom);
	}

	path.closeSubpath();
	this->setShape(path);
}

QRectF VipPlotShape::ellipseAroundPixel(const QPointF& c, const QSizeF& min_size, const VipCoordinateSystemPtr& m) const
{
	QRectF ellipse(QPointF(0, 0), min_size);

	if (testDrawComponent(VipPlotShape::FillPixels)) {
		QRectF pixel = (m->transform(QRectF(0, 0, 1, 1))).boundingRect();
		ellipse.setWidth(qMax(ellipse.width(), pixel.width() * 2));
		ellipse.setHeight(qMax(ellipse.height(), pixel.height() * 2));

		QPointF center = (c);
		center.setX(((int)center.x()) + 0.5);
		center.setY(((int)center.y()) + 0.5);
		center = m->transform(center);

		ellipse.moveCenter(center);
	}
	else
		ellipse.moveCenter(m->transform(c));

	return ellipse;
}

void VipPlotShape::setShape(const QPainterPath& path) const
{
	const_cast<QPainterPath&>(d_data->path) = path;
}

QVariant VipPlotShape::itemChange(GraphicsItemChange change, const QVariant& value)
{
	// if (change == QGraphicsItem::ItemSelectedHasChanged)
	// {
	// //rawData().setSelected(this->isSelected());
	// }
	// else
	if (change == QGraphicsItem::ItemVisibleHasChanged) {
		if (d_data->polygonMovers) {
			if (isVisible() && !d_data->polygonMovers->isVisible())
				d_data->polygonMovers->setVisible(true);
			else if (!isVisible() && d_data->polygonMovers->isVisible())
				d_data->polygonMovers->setVisible(false);
		}
	}

	return VipPlotItemDataType::itemChange(change, value);
}

bool VipPlotShape::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "text-alignment") == 0) {
		setTextAlignment((Qt::Alignment)value.toInt());
		return true;
	}
	if (strcmp(name, "text-position") == 0) {
		setTextPosition((Vip::RegionPositions)value.toInt());
		return true;
	}
	if (strcmp(name, "text-distance") == 0) {
		setTextDistance(value.toDouble());
		return true;
	}
	if (strcmp(name, "components") == 0) {
		setDrawComponents((DrawComponents)value.toInt());
		return true;
	}
	if (strcmp(name, "component") == 0) {
		auto it = drawComponentValues().find(index);
		if (it == drawComponentValues().end())
			return false;
		setDrawComponent((DrawComponent)it.value(), value.toBool());
		return true;
	}
	if (strcmp(name, "polygon-editable") == 0) {
		setPolygonEditable(value.toBool());
		return true;
	}
	if (strcmp(name, "adjust-text-color") == 0) {
		setAdjustTextColor(value.toBool());
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

static int _registerVipPlotSceneModel = vipSetKeyWordsForClass(&VipPlotSceneModel::staticMetaObject);

class VipPlotSceneModel::PrivateData
{
public:
	QMap<QString, VipPlotShape::DrawComponents> components;
	QMap<QString, Vip::RegionPositions> textPosition;
	QMap<QString, Qt::Alignment> textAlignment;
	QMap<QString, QSharedPointer<VipTextStyle>> textStyle;
	QMap<QString, VipText> text;
	QMap<QString, QPainter::RenderHints> shapesRenderHints;
	QMap<QString, bool> adjustTextColor;
	QMap<QString, bool> visibility;
	QMap<QString, QPen> pen;
	QMap<QString, QBrush> brush;
	QMap<QString, QPen> resizerPen;
	QMap<QString, QBrush> resizerBrush;
	QMap<QString, QTransform> textTransform;
	QMap<QString, QPointF> textTransformReference;
	QMap<QString, double> textDistance;
	QMap<QString, QString> toolTipText;
	Mode mode;
	VipSceneModel sceneModel;
	VipSceneModel newSceneModel;
	int shapeCount;
	bool inHideUnused;
	std::atomic<bool> dirtySM;
	VipSpinlock mutex;

	QMap<QString, bool> selected; // to keep track of selected shapes when changing the scene model
	QMap<QString, bool> visible;  // to keep track of visible shapes when changing the scene model

	PrivateData()
	  : mode(Fixed)
	  , newSceneModel(VipSceneModel::null())
	  , shapeCount(0)
	  , inHideUnused(false)
	  , dirtySM(false)
	{
		components["All"] = VipPlotShape::Border | VipPlotShape::Background | VipPlotShape::Id;
		textPosition["All"] = Vip::XInside;
		textAlignment["All"] = Qt::AlignLeft | Qt::AlignBottom;
		shapesRenderHints["All"] = QPainter::Antialiasing;
		adjustTextColor["All"] = true;
		visibility["All"] = true;
		textTransform["All"] = QTransform();
		textTransformReference["All"] = QPointF();
		textDistance["All"] = 0;
		toolTipText["All"] = QString();
		resizerPen["All"] = QPen();
		resizerBrush["All"] = QBrush();
	}
};

// helper function
// when looking for a value inside a QMap<QString,T>, if this value does not exist, first create and set it to the default value (map["ALL"])
template<class T>
T& getValue(const QMap<QString, T>& map, const QString& group)
{
	typename QMap<QString, T>::const_iterator it = map.find(group);
	if (it != map.end())
		return const_cast<T&>(*it);
	else {
		T& value = const_cast<QMap<QString, T>&>(map)[group];
		return value = map[QString("All")];
	}
}

class PlotSceneModelShape : public VipPlotShape
{
	bool m_inUse;

public:
	PlotSceneModelShape(const VipText& title = QString())
	  : VipPlotShape(title)
	  , m_inUse(true)
	{
	}

	void setInUse(bool use) { m_inUse = use; }
	bool inUse() const { return m_inUse; }
};

VipPlotSceneModel::VipPlotSceneModel(const VipText& title)
  : VipPlotItemComposite(VipPlotItemComposite::Aggregate, title)
{
	VIP_CREATE_PRIVATE_DATA();
	connect(d_data->sceneModel.shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(resetSceneModelInternal()), Qt::DirectConnection);
	connect(d_data->sceneModel.shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged(const VipSceneModel&)), Qt::DirectConnection);
	connect(d_data->sceneModel.shapeSignals(), SIGNAL(groupAdded(const QString&)), this, SLOT(emitGroupsChanged()), Qt::DirectConnection);
	connect(d_data->sceneModel.shapeSignals(), SIGNAL(groupRemoved(const QString&)), this, SLOT(emitGroupsChanged()), Qt::DirectConnection);
	setItemAttribute(HasLegendIcon, false);
	setItemAttribute(VisibleLegend, false);
}

VipPlotSceneModel::~VipPlotSceneModel() = default;

void VipPlotSceneModel::setCompositeMode(VipPlotItemComposite::Mode mode)
{
	// if UniqueItem, remove all VipResizeItem objects
	if (mode == UniqueItem)
		setMode(Fixed);

	VipPlotItemComposite::setCompositeMode(mode);
}

void VipPlotSceneModel::setMode(Mode mode)
{
	if (mode != Fixed && compositeMode() == UniqueItem)
		return;

	if (d_data->mode != mode) {
		d_data->mode = mode;

		QList<VipPlotShape*> shapes = this->shapes();
		for (int i = 0; i < shapes.size(); ++i) {
			VipPlotShape* sh = shapes[i];
			VipResizeItem* item = sh->property("VipResizeItem").value<VipResizeItemPtr>();

			sh->setItemAttribute(VipPlotItem::IgnoreMouseEvents, mode != Fixed);

			if (mode == Fixed) {
				if (item) {
					item->setAutoDelete(false);
					delete item;
					sh->setProperty("VipResizeItem", QVariant::fromValue(VipResizeItemPtr()));
				}
			}
			// attach a VipResizeItem to the shape
			else if ((mode == Movable || mode == Resizable) && !item) {
				item = new VipResizeItem();
				item->setManagedItems(PlotItemList() << sh);
				sh->setProperty("VipResizeItem", QVariant::fromValue(VipResizeItemPtr(item)));
			}

			// change the VipResizeItem resize mode
			if (mode == Movable) {
				item->setLibertyDegrees(VipResizeItem::AllMove);
			}
			else if (mode == Resizable) {
				if (sh->rawData().type() != VipShape::Point && sh->rawData().type() != VipShape::Polyline)
					item->setLibertyDegrees(VipResizeItem::MoveAndResize);
				else
					item->setLibertyDegrees(VipResizeItem::AllMove);
			}
		}
	}
}

VipPlotSceneModel::Mode VipPlotSceneModel::mode() const
{
	return d_data->mode;
}

void VipPlotSceneModel::setDrawComponents(const QString& group, VipPlotShape::DrawComponents c)
{
	d_data->components[group.isEmpty() ? "All" : group] = c;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, VipPlotShape::DrawComponents>::iterator it = d_data->components.begin(); it != d_data->components.end(); ++it)
			it.value() = c;
	}
	updateShapes();
}

void VipPlotSceneModel::setDrawComponent(const QString& group, VipPlotShape::DrawComponent c, bool on)
{
	if (on)
		getValue(d_data->components, group) |= c;
	else
		getValue(d_data->components, group) &= ~c;

	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, VipPlotShape::DrawComponents>::iterator it = d_data->components.begin(); it != d_data->components.end(); ++it)
			it.value() = getValue(d_data->components, group);
	}

	updateShapes();
}

bool VipPlotSceneModel::testDrawComponent(const QString& group, VipPlotShape::DrawComponent c) const
{
	return getValue(d_data->components, group).testFlag(c);
}

VipPlotShape::DrawComponents VipPlotSceneModel::drawComponents(const QString& group) const
{
	return getValue(d_data->components, group);
}

void VipPlotSceneModel::setAdjustTextColor(const QString& group, bool enable)
{
	d_data->adjustTextColor[group.isEmpty() ? "All" : group] = enable;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, bool>::iterator it = d_data->adjustTextColor.begin(); it != d_data->adjustTextColor.end(); ++it)
			it.value() = enable;
	}
	updateShapes();
}
bool VipPlotSceneModel::adjustTextColor(const QString& group) const
{
	return getValue(d_data->adjustTextColor, group);
}

void VipPlotSceneModel::setShapesRenderHints(const QString& group, QPainter::RenderHints hints)
{
	d_data->shapesRenderHints[group.isEmpty() ? "All" : group] = hints;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, QPainter::RenderHints>::iterator it = d_data->shapesRenderHints.begin(); it != d_data->shapesRenderHints.end(); ++it)
			it.value() = hints;
	}
	updateShapes();
}

QPainter::RenderHints VipPlotSceneModel::shapesRenderHints(const QString& group) const
{
	return getValue(d_data->shapesRenderHints, group);
}

void VipPlotSceneModel::setTextPosition(const QString& group, Vip::RegionPositions pos)
{
	d_data->textPosition[group.isEmpty() ? "All" : group] = pos;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, Vip::RegionPositions>::iterator it = d_data->textPosition.begin(); it != d_data->textPosition.end(); ++it)
			it.value() = pos;
	}
	updateShapes();
}

Vip::RegionPositions VipPlotSceneModel::textPosition(const QString& group) const
{
	return getValue(d_data->textPosition, group);
}

void VipPlotSceneModel::setTextAlignment(const QString& group, Qt::Alignment align)
{
	d_data->textAlignment[group.isEmpty() ? "All" : group] = align;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, Qt::Alignment>::iterator it = d_data->textAlignment.begin(); it != d_data->textAlignment.end(); ++it)
			it.value() = align;
	}
	updateShapes();
}

Qt::Alignment VipPlotSceneModel::textAlignment(const QString& group) const
{
	return getValue(d_data->textAlignment, group);
}

void VipPlotSceneModel::setTextTransform(const QString& group, const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform[group.isEmpty() ? "All" : group] = tr;
	d_data->textTransformReference[group.isEmpty() ? "All" : group] = ref;
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->textTransform.begin(); it != d_data->textTransform.end(); ++it)
			it.value() = tr;
		for (auto it = d_data->textTransformReference.begin(); it != d_data->textTransformReference.end(); ++it)
			it.value() = ref;
	}
	updateShapes();
}
const QTransform& VipPlotSceneModel::textTransform(const QString& group) const
{
	return getValue(d_data->textTransform, group);
}
const QPointF& VipPlotSceneModel::textTransformReference(const QString& group) const
{
	return getValue(d_data->textTransformReference, group);
}

void VipPlotSceneModel::setTextDistance(const QString& group, double distance)
{
	d_data->textDistance[group.isEmpty() ? "All" : group] = distance;
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->textDistance.begin(); it != d_data->textDistance.end(); ++it)
			it.value() = distance;
	}
	updateShapes();
}
double VipPlotSceneModel::textDistance(const QString& group) const
{
	return getValue(d_data->textDistance, group);
}

void VipPlotSceneModel::setTextStyle(const VipTextStyle& style)
{
	setTextStyle(QString(), style);
}

void VipPlotSceneModel::setTextStyle(const QString& group, const VipTextStyle& style)
{
	d_data->textStyle[group.isEmpty() ? "All" : group].reset(new VipTextStyle(style));
	d_data->text[group.isEmpty() ? "All" : group].setTextStyle(style);
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->textStyle.begin(); it != d_data->textStyle.end(); ++it) {
			it.value().reset(new VipTextStyle(style));
		}
		for (auto it = d_data->text.begin(); it != d_data->text.end(); ++it)
			it.value().setTextStyle(style);
	}
	updateShapes();
}

VipTextStyle VipPlotSceneModel::textStyle(const QString& group) const
{
	auto ts = getValue(d_data->textStyle, group);
	if (ts)
		return *ts;
	return VipTextStyle();
}

VipTextStyle VipPlotSceneModel::textStyle() const
{
	return textStyle(QString());
}

void VipPlotSceneModel::setText(const QString& group, const VipText& text)
{
	auto t = text;
	auto style = getValue(d_data->textStyle, group);
	if (style)
		t.setTextStyle(*style);
	d_data->text[group.isEmpty() ? "All" : group] = t;
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->text.begin(); it != d_data->text.end(); ++it)
			it.value() = t;
	}
	updateShapes();
}

VipText VipPlotSceneModel::text(const QString& group) const
{
	return getValue(d_data->text, group);
}

void VipPlotSceneModel::setToolTipText(const QString& group, const QString& text)
{
	d_data->toolTipText[group.isEmpty() ? "All" : group] = text;
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->toolTipText.begin(); it != d_data->toolTipText.end(); ++it)
			it.value() = text;
	}
	updateShapes();
}
QString VipPlotSceneModel::toolTipText(const QString& group)
{
	return getValue(d_data->toolTipText, group);
}

void VipPlotSceneModel::setResizerPen(const QString& group, const QPen& pen)
{
	d_data->resizerPen[group.isEmpty() ? "All" : group] = pen;
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->resizerPen.begin(); it != d_data->resizerPen.end(); ++it)
			it.value() = pen;
	}
	updateShapes();
}
QPen VipPlotSceneModel::resizerPen(const QString& group) const
{
	return getValue(d_data->resizerPen, group);
}

void VipPlotSceneModel::setResizerBrush(const QString& group, const QBrush& brush)
{
	d_data->resizerBrush[group.isEmpty() ? "All" : group] = brush;
	if (group == "All" || group.isEmpty()) {
		for (auto it = d_data->resizerBrush.begin(); it != d_data->resizerBrush.end(); ++it)
			it.value() = brush;
	}
	updateShapes();
}
QBrush VipPlotSceneModel::resizerBrush(const QString& group) const
{
	return getValue(d_data->resizerBrush, group);
}

void VipPlotSceneModel::setPen(const QString& group, const QPen& pen)
{
	d_data->pen[group.isEmpty() ? "All" : group] = pen;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, QPen>::iterator it = d_data->pen.begin(); it != d_data->pen.end(); ++it)
			it.value() = pen;
	}
	updateShapes();
}

QPen VipPlotSceneModel::pen(const QString& group) const
{
	return getValue(d_data->pen, group);
}

void VipPlotSceneModel::setBrush(const QString& group, const QBrush& brush)
{
	d_data->brush[group.isEmpty() ? "All" : group] = brush;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, QBrush>::iterator it = d_data->brush.begin(); it != d_data->brush.end(); ++it)
			it.value() = brush;
	}
	updateShapes();
}

QBrush VipPlotSceneModel::brush(const QString& group) const
{
	return getValue(d_data->brush, group);
}

void VipPlotSceneModel::setPen(const QPen& pen)
{
	setPen(QString(), pen);
}
QPen VipPlotSceneModel::pen() const
{
	return pen(QString());
}

void VipPlotSceneModel::setBrush(const QBrush& brush)
{
	setBrush(QString(), brush);
}
QBrush VipPlotSceneModel::brush() const
{
	return brush(QString());
}

void VipPlotSceneModel::setIgnoreStyleSheet(bool enable)
{
	VipPlotItemComposite::setIgnoreStyleSheet(enable);
	QList<VipPlotShape*> shapes = shapeItems();
	for (VipPlotShape* sh : shapes) {
		sh->setIgnoreStyleSheet(enable);
		if (VipResizeItem* it = sh->property("VipResizeItem").value<VipResizeItem*>())
			it->setIgnoreStyleSheet(enable);
	}
}

void VipPlotSceneModel::setGroupVisible(const QString& group, bool visible)
{
	d_data->visibility[group.isEmpty() ? "All" : group] = visible;
	if (group == "All" || group.isEmpty()) {
		for (QMap<QString, bool>::iterator it = d_data->visibility.begin(); it != d_data->visibility.end(); ++it)
			it.value() = visible;
	}
	updateShapes();
	emitGroupsChanged();
}
bool VipPlotSceneModel::groupVisible(const QString& group) const
{
	return getValue(d_data->visibility, group);
}

QList<VipPlotShape*> VipPlotSceneModel::shapeItems() const
{
	const QList<QPointer<VipPlotItem>>& its = items();
	QList<VipPlotShape*> res;

	for (int i = 0; i < its.size(); ++i) {
		if (VipPlotShape* sh = qobject_cast<VipPlotShape*>(its[i].data()))
			res.append(sh);
	}

	return res;
}

QList<VipPlotShape*> VipPlotSceneModel::shapes(int selection) const
{
	QList<VipPlotShape*> items = shapeItems();
	if (selection < 0 || selection > 1) {
		QList<VipPlotShape*> res;
		for (int i = 0; i < items.size(); ++i)
			if (static_cast<PlotSceneModelShape*>(items[i])->inUse())
				res.append(items[i]);
		return res;
	}
	else {
		QList<VipPlotShape*> res;
		for (int i = 0; i < items.size(); ++i)
			if (static_cast<PlotSceneModelShape*>(items[i])->inUse() && int(items[i]->isSelected()) == selection)
				res.append(items[i]);
		return res;
	}
}

void VipPlotSceneModel::resetSceneModelInternal()
{
	bool expect = false;
	if (d_data->dirtySM.compare_exchange_strong(expect, true)) {
		if (QThread::currentThread() == qApp->thread())
			resetSceneModel();
		else
			QMetaObject::invokeMethod(this, "resetSceneModel", Qt::QueuedConnection);
	}
}

void VipPlotSceneModel::setSceneModel(const VipSceneModel& scene)
{
	if (QThread::currentThread() == qApp->thread()) {
		d_data->newSceneModel = scene;
		setSceneModelInternal();
	}
	else {
		VipUniqueLock<VipSpinlock> lock(d_data->mutex);
		VipSceneModel prev = d_data->newSceneModel;
		d_data->newSceneModel = scene;
		if (prev.isNull())
			QMetaObject::invokeMethod(this, "setSceneModelInternal", Qt::QueuedConnection);
	}
}

void VipPlotSceneModel::resetContentWith(const VipSceneModel& scene)
{
	if (QThread::currentThread() == qApp->thread()) {
		d_data->newSceneModel = scene;
		resetSceneModelInternalWith();
	}
	else {
		VipUniqueLock<VipSpinlock> lock(d_data->mutex);
		VipSceneModel prev = d_data->newSceneModel;
		d_data->newSceneModel = scene;
		if (prev.isNull())
			QMetaObject::invokeMethod(this, "resetSceneModelInternalWith", Qt::QueuedConnection);
	}
}

void VipPlotSceneModel::resetSceneModelInternalWith()
{
	VipSceneModel scene = VipSceneModel::null();
	{
		VipUniqueLock<VipSpinlock> lock(d_data->mutex);
		scene = d_data->newSceneModel;
		if (scene.isNull())
			return;
		d_data->newSceneModel = VipSceneModel::null();
		if (d_data->sceneModel == scene)
			return;
	}

	d_data->sceneModel.reset(scene);
	resetSceneModel();
}

void VipPlotSceneModel::mergeContentWith(const VipSceneModel& scene)
{
	if (QThread::currentThread() == qApp->thread()) {
		d_data->newSceneModel = scene;
		mergeSceneModelInternalWith();
	}
	else {
		VipUniqueLock<VipSpinlock> lock(d_data->mutex);
		VipSceneModel prev = d_data->newSceneModel;
		d_data->newSceneModel = scene;
		if (prev.isNull())
			QMetaObject::invokeMethod(this, "mergeSceneModelInternalWith", Qt::QueuedConnection);
	}
}

void VipPlotSceneModel::mergeSceneModelInternalWith()
{
	VipSceneModel scene = VipSceneModel::null();
	{
		VipUniqueLock<VipSpinlock> lock(d_data->mutex);
		scene = d_data->newSceneModel;
		if (scene.isNull())
			return;
		d_data->newSceneModel = VipSceneModel::null();
		if (d_data->sceneModel == scene)
			return;
	}

	d_data->sceneModel.add(scene);
	resetSceneModel();
}

void VipPlotSceneModel::setSceneModelInternal()
{

	VipSceneModel scene = VipSceneModel::null();
	{
		VipUniqueLock<VipSpinlock> lock(d_data->mutex);
		scene = d_data->newSceneModel;
		if (scene.isNull())
			return;
		d_data->newSceneModel = VipSceneModel::null();
		if (d_data->sceneModel == scene)
			return;
	}

	disconnect(d_data->sceneModel.shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(resetSceneModelInternal()));
	disconnect(d_data->sceneModel.shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged(const VipSceneModel&)));
	disconnect(d_data->sceneModel.shapeSignals(), SIGNAL(groupAdded(const QString&)), this, SLOT(emitGroupsChanged()));
	disconnect(d_data->sceneModel.shapeSignals(), SIGNAL(groupRemoved(const QString&)), this, SLOT(emitGroupsChanged()));

	connect(scene.shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(resetSceneModelInternal()), Qt::DirectConnection);
	connect(scene.shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged(const VipSceneModel&)), Qt::DirectConnection);
	connect(scene.shapeSignals(), SIGNAL(groupAdded(const QString&)), this, SLOT(emitGroupsChanged()), Qt::DirectConnection);
	connect(scene.shapeSignals(), SIGNAL(groupRemoved(const QString&)), this, SLOT(emitGroupsChanged()), Qt::DirectConnection);

	QSet<QString> prev_groups = vipToSet(d_data->sceneModel.groups());

	d_data->sceneModel = scene;
	resetSceneModel();

	QSet<QString> new_groups = vipToSet(d_data->sceneModel.groups());
	if (new_groups != prev_groups)
		Q_EMIT groupsChanged();
	Q_EMIT sceneModelChanged(scene);
}

VipSceneModel VipPlotSceneModel::sceneModel() const
{
	return d_data->sceneModel;
}

static int findShapeId(const QList<VipPlotShape*>& lst, const VipShape& sh)
{
	for (int i = 0; i < lst.size(); ++i)
		if (lst[i]->rawData().id() == sh.id())
			return i;
	return -1;
}

void VipPlotSceneModel::resetSceneModel()
{
	d_data->dirtySM = false;
	// try to reuse the maximum number of previous shapes
	QList<VipPlotShape*> shs = shapeItems();
	QStringList groups = d_data->sceneModel.groups();
	d_data->shapeCount = 0;

	for (int i = 0; i < groups.size(); ++i) {
		const QString group = groups[i];
		const VipShapeList gr_shapes = d_data->sceneModel.shapes(group);
		bool visible = groupVisible(group);
		for (int s = 0; s < gr_shapes.size(); ++s) {
			VipShape sh = gr_shapes[s];
			VipPlotShape* shape;
			// if (d_data->shapeCount < shs.size())
			// {
			// shape = shs[d_data->shapeCount];
			// shape->setRawData(sh);
			// //shape->setPolygonEditable(mode() != Fixed && sh.isPolygonBased());
			// }
			if (shs.size()) {
				int index = findShapeId(shs, sh);
				if (index == -1)
					index = 0;
				shape = shs[index];
				shs.removeAt(index);
				const VipShape old = shape->rawData();
				if (old != sh)
					shape->setRawData(sh);
			}
			else {
				shape = createShape(sh);
				shape->setZValue(1000);
				shape->setRawData(sh);

				connect(shape, SIGNAL(plotShapeDestroyed(VipPlotShape*)), this, SLOT(plotShapeDestroyed(VipPlotShape*)), Qt::DirectConnection);
				connect(shape, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(saveShapeSelectionState(VipPlotItem*)), Qt::DirectConnection);
				// connect(shape, SIGNAL(visibilityChanged(VipPlotItem *)), this, SLOT(saveShapeVisibilityState()), Qt::DirectConnection);

				// if(mode() != Fixed && sh.isPolygonBased())//(sh.type() == VipShape::Polygon || sh.type() == VipShape::Polyline))
				{
					// shape->setPolygonEditable(mode() != Fixed && sh.isPolygonBased());
				}

				if (mode() == Movable || mode() == Resizable) {
					VipResizeItem* item = new VipResizeItem();
					item->setManagedItems(PlotItemList() << shape);
					shape->setProperty("VipResizeItem", QVariant::fromValue(VipResizeItemPtr(item)));

					connect(item, SIGNAL(aboutToMove()), this, SLOT(emitAboutToMove()), Qt::DirectConnection);
					connect(item, SIGNAL(aboutToResize()), this, SLOT(emitAboutToResize()), Qt::DirectConnection);
					connect(item, SIGNAL(aboutToRotate()), this, SLOT(emitAboutToRotate()), Qt::DirectConnection);
					connect(item, SIGNAL(aboutToChangePoints()), this, SLOT(emitAboutToChangePoints()), Qt::DirectConnection);
					connect(item, SIGNAL(aboutToDelete()), this, SLOT(emitAboutToDelete()), Qt::DirectConnection);
					connect(item, SIGNAL(finishedChange()), this, SLOT(emitFinishedChange()), Qt::DirectConnection);

					// change the VipResizeItem resize mode
					if (mode() == Movable)
						item->setLibertyDegrees(VipResizeItem::AllMove);
					else if (mode() == Resizable && sh.type() != VipShape::Point && sh.type() != VipShape::Polyline)
						item->setLibertyDegrees(VipResizeItem::MoveAndResize | VipResizeItem::Rotate);
					else
						item->setLibertyDegrees(VipResizeItem::AllMove);

					if (sh.type() == VipShape::Point) {
						item->boxStyle().setBorderPen(QPen(Qt::NoPen));
					}
				}
				else if (mode() == Fixed) {
					// for fixed shape, we still want to be able to select them with the mouse.
					// since ther isn't a VipResizeItem to take care of this, remove the IgnoreMouseEvents attribute from the shape.
					// also remove the Droppable attribute (we cannot move them, and we don't want to drag and drop them)
					shape->setItemAttribute(VipPlotItem::IgnoreMouseEvents, false);
					shape->setItemAttribute(VipPlotItem::Droppable, false);
				}

				this->append(shape);
			}

			static_cast<PlotSceneModelShape*>(shape)->setInUse(true);

			++d_data->shapeCount;

			// set all the parameters
			shape->setPolygonEditable(mode() != Fixed && sh.isPolygonBased());
			shape->setTextStyle(textStyle(group));
			shape->setText(text(group));
			shape->setTextPosition(textPosition(group));
			shape->setTextAlignment(textAlignment(group));
			shape->setAdjustTextColor(adjustTextColor(group));
			shape->setTextTransform(textTransform(group), textTransformReference(group));
			shape->setTextDistance(textDistance(group));
			shape->setIgnoreStyleSheet(this->ignoreStyleSheet());

			QString tool_tip = toolTipText(group);
			if (tool_tip.isEmpty())
				tool_tip = this->toolTipText();
			shape->setToolTipText(tool_tip);

			shape->setDrawComponents(drawComponents(group));
			shape->setPen(pen(group));
			// shape->setSelected(sh.isSelected());
			shape->setBrush(brush(group));

			shape->blockSignals(true);
			shape->setVisible(visible);

			if (VipResizeItem* resize = shape->property("VipResizeItem").value<VipResizeItemPtr>()) {
				resize->setPen(resizerPen(group));
				resize->setBrush(resizerBrush(group));
				resize->setIgnoreStyleSheet(this->ignoreStyleSheet());
				if (resize->isVisible() != visible)
					resize->setVisible(visible);
			}
			// restore the selection state since hidding a QGraphicsItem drops the selection (and we hide unused shapes)
			QMap<QString, bool>::const_iterator it = d_data->selected.find(sh.identifier());
			if (it != d_data->selected.end()) {
				if (shape->isSelected() != it.value()) {
					shape->setSelected(it.value());
					if (VipResizeItem* resize = shape->property("VipResizeItem").value<VipResizeItemPtr>())
						resize->setSelected(it.value()); // this will also unselect the shape
				}
			}
			else if (shape->isSelected()) {
				shape->setSelected(false);
				if (VipResizeItem* resize = shape->property("VipResizeItem").value<VipResizeItemPtr>())
					resize->setSelected(false); // this will also unselect the shape
			}

			// restore the visibility state
			// it = d_data->visible.find(sh.identifier());
			// if (it != d_data->visible.end())
			// shape->setVisible(it.value());

			shape->blockSignals(false);
		}
	}

	d_data->inHideUnused = true;
	// hide all unused shapes
	// for (int i = d_data->shapeCount; i < shs.size(); ++i)
	for (int i = 0; i < shs.size(); ++i) {
		VipResizeItem* resize = shs[i]->property("VipResizeItem").value<VipResizeItemPtr>();
		if (resize)
			resize->setVisible(false);
		shs[i]->setVisible(false);

		static_cast<PlotSceneModelShape*>(shs[i])->setInUse(false);
	}
	d_data->inHideUnused = false;
}

void VipPlotSceneModel::emitAboutToMove()
{
	Q_EMIT aboutToMove(qobject_cast<VipResizeItem*>(sender()));
}
void VipPlotSceneModel::emitAboutToResize()
{
	Q_EMIT aboutToResize(qobject_cast<VipResizeItem*>(sender()));
}
void VipPlotSceneModel::emitAboutToRotate()
{
	Q_EMIT aboutToRotate(qobject_cast<VipResizeItem*>(sender()));
}
void VipPlotSceneModel::emitAboutToChangePoints()
{
	Q_EMIT aboutToChangePoints(qobject_cast<VipResizeItem*>(sender()));
}
void VipPlotSceneModel::emitAboutToDelete()
{
	Q_EMIT aboutToDelete(qobject_cast<VipResizeItem*>(sender()));
}

void VipPlotSceneModel::emitFinishedChange()
{
	Q_EMIT finishedChange(qobject_cast<VipResizeItem*>(sender()));
}

void VipPlotSceneModel::plotShapeDestroyed(VipPlotShape* shape)
{
	int index = this->indexOf(shape);
	if (index >= 0)
		this->takeItem(index);
	VipShape sh = shape->rawData();
	d_data->sceneModel.remove(sh);

	Q_EMIT shapeDestroyed(shape);
}

void VipPlotSceneModel::emitGroupsChanged()
{
	Q_EMIT groupsChanged();
}

void VipPlotSceneModel::saveShapeSelectionState(VipPlotItem* item)
{
	if (!d_data->inHideUnused) {
		// save the selection state
		d_data->selected.clear();
		QList<VipPlotShape*> shs = shapes(); // shapeItems();

		for (int i = 0; i < shs.size(); ++i)
			if (shs[i]->isVisible()) {
				d_data->selected[shs[i]->rawData().identifier()] = shs[i]->isSelected();
			}
	}

	Q_EMIT shapeSelectionChanged(qobject_cast<VipPlotShape*>(item));
}

void VipPlotSceneModel::saveShapeVisibilityState()
{
	// save the visibility state
	d_data->visible.clear();
	QList<VipPlotShape*> shs = shapes(); // shapeItems();

	for (int i = 0; i < shs.size(); ++i) {
		d_data->visible[shs[i]->rawData().identifier()] = shs[i]->isVisible();
	}
}

void VipPlotSceneModel::updateShapes()
{
	QList<VipPlotShape*> shs = shapes();
	for (int i = 0; i < shs.size(); ++i) {
		VipPlotShape* shape = shs[i];
		const QString group = shape->rawData().group();
		// set all the parameters
		shape->blockSignals(true);
		shape->setTextPosition(textPosition(group));
		shape->setTextAlignment(textAlignment(group));
		shape->setDrawComponents(drawComponents(group));
		shape->setTextStyle(textStyle(group));
		shape->setText(text(group));
		shape->setPen(pen(group));
		shape->blockSignals(false);
		shape->setBrush(brush(group));
		shape->setRenderHints(shapesRenderHints(group));
		shape->setAdjustTextColor(adjustTextColor(group));
		shape->setTextTransform(textTransform(group), textTransformReference(group));
		shape->setTextDistance(textDistance(group));
		QString tool_tip = toolTipText(group);
		if (tool_tip.isEmpty())
			tool_tip = toolTipText();
		shape->setToolTipText(tool_tip);
		shape->setItemAttribute(VipPlotItem::IsSuppressable, this->testItemAttribute(VipPlotItem::IsSuppressable));
		bool vis = groupVisible(group);
		shape->setVisible(vis);
		if (VipResizeItem* resize = shape->property("VipResizeItem").value<VipResizeItemPtr>()) {
			resize->setPen(resizerPen(group));
			resize->setBrush(resizerBrush(group));
			resize->setVisible(vis);
		}
	}
}

VipPlotShape* VipPlotSceneModel::createShape(const VipShape&) const
{
	VipPlotShape* shape = new PlotSceneModelShape();
	shape->setProperty("VipPlotSceneModel", QVariant::fromValue((VipPlotSceneModel*)this));
	shape->setItemAttribute(VipPlotItem::IsSuppressable, this->testItemAttribute(VipPlotItem::IsSuppressable));
	return shape;
}

bool VipPlotSceneModel::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "border-width") == 0) {
		// handle boder-width key ourselves
		d_data->pen["All"].setWidthF(value.toDouble());
		for (auto it = d_data->pen.begin(); it != d_data->pen.end(); ++it)
			it.value().setWidthF(value.toDouble());
		return true;
	}
	return VipPlotItemComposite::setItemProperty(name, value, index);
}

// void VipPlotSceneModel::itemAdded(VipPlotItem * item)
// {
// if (VipPlotShape * sh = qobject_cast<VipPlotShape*>(item))
// {
// sceneModel().add(sh->rawData());
// }
// }

VipPlotShape* VipPlotSceneModel::findShape(const VipShape& sh) const
{
	const QList<VipPlotShape*> its = shapes();

	for (int i = 0; i < its.size(); ++i) {
		if (VipPlotShape* shape = qobject_cast<VipPlotShape*>(its[i])) {
			if (shape->rawData() == sh)
				return shape;
		}
	}

	return nullptr;
}

QList<VipPlotShape*> VipPlotSceneModel::shapes(const QString& group, int selection) const
{
	if (group == "All")
		return shapes(selection);
	else if (group == "None")
		return QList<VipPlotShape*>();

	const QList<VipPlotShape*> its = shapes(selection);
	QList<VipPlotShape*> res;

	for (int i = 0; i < its.size(); ++i) {
		if (VipPlotShape* shape = qobject_cast<VipPlotShape*>(its[i])) {
			if (shape->rawData().group() == group)
				res << shape;
		}
	}
	return res;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotShape* value)
{
	arch.content("dawComponents", (int)value->drawComponents());
	arch.content("textStyle", value->textStyle());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("adjustTextColor", (int)value->adjustTextColor());

	// new in 4.2.0
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());

	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotShape* value)
{
	value->setDrawComponents((VipPlotShape::DrawComponents)arch.read("dawComponents").value<int>());
	value->setTextStyle(arch.read("textStyle").value<VipTextStyle>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());
	value->setTextAlignment((Qt::AlignmentFlag)arch.read("textAlignment").value<int>());
	arch.save();
	value->setAdjustTextColor(arch.read("adjustTextColor").value<bool>());
	if (!arch)
		arch.restore();
	else {

		arch.save();
		QTransform textTransform = arch.read("textTransform").value<QTransform>();
		QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
		if (arch) {

			value->setTextTransform(textTransform, textTransformReference);
			value->setTextDistance(arch.read("textDistance").value<double>());
			value->setText(arch.read("text").value<VipText>());
		}
		else
			arch.restore();
	}
	arch.resetError();
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotSceneModel* value)
{
	// mark internal shapes as non serializable, they will recreated when reloading the VipPlotSceneModel
	for (int i = 0; i < value->count(); ++i) {
		if (VipPlotShape* sh = qobject_cast<VipPlotShape*>(value->at(i))) {
			sh->setProperty("_vip_no_serialize", true);
			if (VipResizeItem* re = (sh->property("VipResizeItem").value<VipResizeItemPtr>()))
				re->setProperty("_vip_no_serialize", true);
		}
	}

	return arch.content("mode", (int)value->mode()).content("sceneModel", value->sceneModel());
}

VipArchive& operator>>(VipArchive& arch, VipPlotSceneModel* value)
{
	value->setMode((VipPlotSceneModel::Mode)arch.read("mode").toInt());
	value->setSceneModel(arch.read("sceneModel").value<VipSceneModel>());
	return arch;
}

static bool register_types()
{
	qRegisterMetaType<VipPlotShape*>();
	vipRegisterArchiveStreamOperators<VipPlotShape*>();

	qRegisterMetaType<VipPlotSceneModel*>();
	vipRegisterArchiveStreamOperators<VipPlotSceneModel*>();

	return true;
}
static bool _register_types = register_types();