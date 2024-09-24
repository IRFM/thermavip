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

#include "VipPlotCurve.h"
#include "VipAbstractScale.h"
#include "VipPainter.h"
#include "VipScaleEngine.h"
#include "VipShapeDevice.h"
#include "VipStyleSheet.h"
#include "VipSymbol.h"

#include <QPainterPathStroker>
#include <qalgorithms.h>
#include <qbitmap.h>
#include <qmath.h>
#include <qpaintengine.h>
#include <qpainter.h>
#include <qpixmap.h>

template<class Point, class Double>
struct PointMerge
{
	QVector<Point> vector;
	Point* buff;
	int buff_size;
	Point min, max;
	int imin, imax;

	PointMerge()
	  : buff_size(0)
	{
		buff = new Point[1000];
	}
	~PointMerge() { delete[] buff; }

	int size() const { return buff_size + vector.size(); }

	const Point last() const { return buff_size ? buff[buff_size - 1] : (vector.size() ? vector.last() : Point()); }

	void finish()
	{
		// add buffer to result
		if (buff_size > 4) {
			// big buffer: add start, min, max and end point
			vector.append(*buff);

			Point inter1, inter2;
			if (imin < imax) {
				inter1 = min;
				inter2 = max;
			}
			else {
				inter1 = max;
				inter2 = min;
			}
			if (inter1.y() != buff->y())
				vector.append(inter1);
			if (inter2.y() != buff[buff_size - 1].y())
				vector.append(inter2);

			vector.append(buff[buff_size - 1]);
		}
		else if (buff_size) {
			// small buffer: add directly to result
			vector.resize(vector.size() + buff_size);
			std::copy(buff, buff + buff_size, vector.data() + vector.size() - buff_size);
		}
		buff_size = 0;
	}

	void add(const Point& pt)
	{

		int x_pos = qRound(pt.x());
		if (buff_size && buff_size < 1000 && qRound(buff[buff_size - 1].x()) == x_pos) {
			// same vertical line
			buff[buff_size++] = pt;
			if (pt.y() < min.y()) {
				min = pt;
				imin = buff_size - 1;
			}
			else if (pt.y() > max.y()) {
				max = pt;
				imax = buff_size - 1;
			}
			// min = std::min(min, pt.y());
			// max = std::max(max, pt.y());
		}
		else {
			finish();
			// add new point to buffer
			buff[buff_size++] = pt;
			min = max = pt;
			imin = imax = buff_size - 1;
		}
	}
};

static bool isPerfectRightCartesiant(QPainter* painter, const VipCoordinateSystemPtr& m)
{
	QTransform tr = painter->worldTransform();
	if (tr.isRotating() || m->type() != VipCoordinateSystem::Cartesian)
		return false;

	QList<VipAbstractScale*> scales = m->axes();
	if (scales.size() != 2)
		return false;
	if (!scales.first() || !scales.last())
		return false;

	QLineF l1(scales.first()->position(scales.first()->scaleDiv().bounds().minValue()), scales.first()->position(scales.first()->scaleDiv().bounds().maxValue()));
	QLineF l2(scales.last()->position(scales.last()->scaleDiv().bounds().minValue()), scales.last()->position(scales.last()->scaleDiv().bounds().maxValue()));

	return (l1.p1().x() == l1.p2().x() && l2.p1().y() == l2.p2().y()) || (l1.p1().y() == l1.p2().y() && l2.p1().x() == l2.p2().x());
}

static QPolygonF computeSteps(const QPolygonF& sample, bool inverted)
{
	QPolygonF polygon(2 * (sample.size()) - 1);

	polygon[0] = sample[0];

	int i, ip;
	for (i = 1, ip = 1; i < sample.size(); i++, ip += 2) {
		const QPointF s = sample[i];

		if (inverted) {
			polygon[ip].rx() = polygon[ip - 1].x();
			polygon[ip].ry() = s.y();
		}
		else {
			polygon[ip].ry() = polygon[ip - 1].y();
			polygon[ip].rx() = s.x();
		}

		polygon[ip + 1] = s;
	}

	polygon.last() = sample.last();

	return polygon;
}

static int registerCurveKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {
		QMap<QByteArray, int> curvestyle;
		QMap<QByteArray, int> curveattribute;
		QMap<QByteArray, int> curvelegend;
		QMap<QByteArray, int> symbol;

		curvestyle["none"] = VipPlotCurve::NoCurve;
		curvestyle["lines"] = VipPlotCurve::Lines;
		curvestyle["sticks"] = VipPlotCurve::Sticks;
		curvestyle["steps"] = VipPlotCurve::Steps;
		curvestyle["dots"] = VipPlotCurve::Dots;

		curveattribute["inverted"] = VipPlotCurve::Inverted;
		curveattribute["closePolyline"] = VipPlotCurve::ClosePolyline;
		curveattribute["fillMultiCurves"] = VipPlotCurve::FillMultiCurves;

		curvelegend["legendNoAttribute"] = VipPlotCurve::LegendNoAttribute;
		curvelegend["legendShowLine"] = VipPlotCurve::LegendShowLine;
		curvelegend["legendShowSymbol"] = VipPlotCurve::LegendShowSymbol;
		curvelegend["legendShowBrush"] = VipPlotCurve::LegendShowBrush;

		keywords["curve-style"] = VipParserPtr(new EnumParser(curvestyle));
		keywords["curve-attribute"] = VipParserPtr(new EnumOrParser(curveattribute));
		keywords["legend"] = VipParserPtr(new EnumOrParser(curvelegend));
		keywords["symbol"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::symbolEnum()));

		keywords["symbol-size"] = VipParserPtr(new DoubleParser());
		keywords["symbol-border"] = VipParserPtr(new PenParser());
		keywords["symbol-background"] = VipParserPtr(new ColorParser());
		keywords["baseline"] = VipParserPtr(new DoubleParser());
		keywords["symbol-condition"] = VipParserPtr(new TextParser());

		keywords["optimize-large-pen-drawing"] = VipParserPtr(new BoolParser());

		vipSetKeyWordsForClass(&VipPlotCurve::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerCurveKeyWords = registerCurveKeyWords();

struct Condition
{
	enum Axis
	{
		X,
		Y
	};
	enum Comparison
	{
		Gr,   //>
		Lr,   //<
		Eq,   //==
		NEq,  //!=
		Greq, //>=
		Lreq  //<=
	};
	Axis axis;
	Comparison comp;
	vip_double value;

	bool isValid(const VipPoint& pt) const
	{
		vip_double v = axis == X ? pt.x() : pt.y();
		switch (comp) {
			case Gr:
				return v > value;
			case Lr:
				return v < value;
			case Greq:
				return v >= value;
			case Lreq:
				return v <= value;
			case Eq:
				return v == value;
			case NEq:
				return v != value;
		}
		return false;
	}
};

struct MultiCondition
{
	enum Separator
	{
		And,
		Or,
		None
	};
	MultiCondition()
	  : separator(None)
	{
	}
	Separator separator;
	Condition c1;
	Condition c2;

	bool isValid(const VipPoint& pt) const
	{
		switch (separator) {
			case And:
				return c1.isValid(pt) && c2.isValid(pt);
			case Or:
				return c1.isValid(pt) || c2.isValid(pt);
			case None:
				return c1.isValid(pt);
		}
		return false;
	}

	static MultiCondition* parse(const QString& condition, QString* error)
	{
		if (condition.isEmpty())
			return nullptr;

		int c_and = condition.count("and");
		int c_or = condition.count("or");
		if (c_and + c_or > 1) {
			if (error)
				*error = "Cannot have more than 2 conditions";
			return nullptr;
		}
		QString sep = c_and == 1 ? "and" : (c_or == 1 ? "or" : "");
		QStringList conds;
		if (sep.isEmpty())
			conds << condition;
		else
			conds = condition.split(sep);

		Condition c[2];

		for (int i = 0; i < conds.size(); ++i) {
			int i_x = conds[i].indexOf("x");
			int i_y = conds[i].indexOf("y");
			if (i_x < 0 && i_y < 0) {
				if (error)
					*error = "Invalid condition (no 'x' or 'y' provided)";
				return nullptr;
			}

			QStringList parts = conds[i].split(" ");
			if (parts.size() != 3) {
				if (error)
					*error = "Invalid condition";
				return nullptr;
			}

			if (parts[1] == ">")
				c[i].comp = Condition::Gr;
			else if (parts[1] == ">=")
				c[i].comp = Condition::Greq;
			else if (parts[1] == "<")
				c[i].comp = Condition::Lr;
			else if (parts[1] == "<=")
				c[i].comp = Condition::Lreq;
			else if (parts[1] == "==")
				c[i].comp = Condition::Eq;
			else if (parts[1] == "!=")
				c[i].comp = Condition::NEq;
			else {
				if (error)
					*error = "Invalid condition: unknown operator '" + parts[1] + "'";
				return nullptr;
			}
			if (parts[0] == "x" || parts[0] == "y") {
				c[i].axis = parts[0] == "x" ? Condition::X : Condition::Y;
				c[i].value = parts[2].toDouble();
			}
			else if (parts[2] == "x" || parts[2] == "y") {
				c[i].axis = parts[2] == "x" ? Condition::X : Condition::Y;
				c[i].value = parts[0].toDouble();
				c[i].comp = (Condition::Comparison)(5 - c[i].comp);
			}
			else {
				if (error)
					*error = "Invalid condition";
				return nullptr;
			}
		}

		// build result;
		MultiCondition* res = new MultiCondition;
		res->c1 = c[0];
		res->c2 = c[1];
		res->separator = sep == "and" ? MultiCondition::And : (sep == "or" ? MultiCondition::Or : MultiCondition::None);
		return res;
	}
};

class VipPlotCurve::PrivateData
{
public:
	PrivateData()
	  : drawn_pcount(0)
	  , style(VipPlotCurve::Lines)
	  , baseline(0.0)
	  , full_continuous(false)
	  , sub_continuous(false)
	  , symbol(new VipSymbol(VipSymbol::Ellipse, QBrush(Qt::lightGray), QPen(Qt::darkGray), QSizeF(9, 9)))
	  , symbolVisible(false)
	  , legendAttributes(LegendShowBrush | LegendShowSymbol | LegendShowLine)
	  , hasSymbol(0)
	  , optimizeLargePenDrawing(true)
	{
		boxStyle.setBorderPen(QPen(Qt::black));
	}

	~PrivateData() { delete symbol; }

	// Draw a function
	std::function<vip_double(vip_double)> function;
	VipInterval scale_interval;
	VipInterval draw_interval;
	int drawn_pcount;
	VipInterval drawn_interval;

	VipPlotCurve::CurveStyle style;
	vip_double baseline;
	bool full_continuous;
	bool sub_continuous;
	QList<bool> continuous;

	const VipSymbol* symbol;
	bool symbolVisible;

	VipBoxStyle boxStyle;
	QMap<int, QPen> subPen;
	QMap<int, QBrush> subBrush;
	// QRectF boundingRect;
	// QList<VipInterval> bounding;
	VipInterval bounding[2];
	QList<VipPointVector> vectors;
	PointMerge<QPointF, double> merge;

	VipPlotCurve::CurveAttributes attributes;
	VipPlotCurve::LegendAttributes legendAttributes;

	QBitmap shapeBitmap;

	QString symbolCondition;
	QSharedPointer<MultiCondition> parseCondition;
	bool hasSymbol;
	bool optimizeLargePenDrawing;
};

/// Constructor
/// \param title Title of the curve
VipPlotCurve::VipPlotCurve(const VipText& title)
  : VipPlotItemDataType((title))
{
	init();
	this->setRenderHints(QPainter::Antialiasing);
	// this->setItemAttribute(RenderInPixmap);
}

//! Destructor
VipPlotCurve::~VipPlotCurve()
{
}

//! Initialize internal members
void VipPlotCurve::init()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	setRawData(VipPointVector());
}

/// Specify an attribute how to draw the legend icon
///
/// \param attribute Attribute
/// \param on On/Off
/// /sa testLegendAttribute(). legendIcon()
void VipPlotCurve::setLegendAttribute(LegendAttribute attribute, bool on)
{
	if (on != testLegendAttribute(attribute)) {
		if (on)
			d_data->legendAttributes |= attribute;
		else
			d_data->legendAttributes &= ~attribute;

		emitItemChanged(false, false, false);
		////legendChanged();
	}
}

/// \return True, when attribute is enabled
/// \sa setLegendAttribute()
bool VipPlotCurve::testLegendAttribute(LegendAttribute attribute) const
{
	return (d_data->legendAttributes & attribute);
}

VipPlotCurve::LegendAttributes VipPlotCurve::legendAttributes() const
{
	return d_data->legendAttributes;
}

void VipPlotCurve::setLegendAttributes(VipPlotCurve::LegendAttributes attributes)
{
	if (d_data->legendAttributes != attributes) {
		d_data->legendAttributes = attributes;
		emitItemChanged(false, false, false);
	}
}

/// Set the curve's drawing style
///
/// \param style Curve style
/// \sa style()
void VipPlotCurve::setStyle(CurveStyle style)
{
	if (style != d_data->style) {
		d_data->style = style;

		emitItemChanged();
	}
}

/// \return Style of the curve
/// \sa setStyle()
VipPlotCurve::CurveStyle VipPlotCurve::style() const
{
	return d_data->style;
}

/// \brief Assign a symbol
///
/// The curve will take the ownership of the symbol, hence the previously
/// set symbol will be delete by setting a new one. If \p symbol is
/// \c nullptr no symbol will be drawn.
///
/// \param symbol VipSymbol
/// \sa symbol()
void VipPlotCurve::setSymbol(VipSymbol* symbol)
{
	if (symbol != d_data->symbol) {
		delete d_data->symbol;
		d_data->symbol = symbol;

		emitItemChanged();
	}
}

/// \return Current symbol or nullptr, when no symbol has been assigned
/// \sa setSymbol()
VipSymbol* VipPlotCurve::symbol() const
{
	return const_cast<VipSymbol*>(d_data->symbol);
}

void VipPlotCurve::setSymbolVisible(bool vis)
{
	if (d_data->symbolVisible != vis) {
		d_data->symbolVisible = vis;
		emitItemChanged();
	}
}
bool VipPlotCurve::symbolVisible() const
{
	return d_data->symbolVisible;
}

QString VipPlotCurve::setSymbolCondition(const QString& condition)
{
	if (d_data->symbolCondition != condition) {
		d_data->symbolCondition = condition;
		QString error;
		d_data->parseCondition = QSharedPointer<MultiCondition>(MultiCondition::parse(condition, &error));
		d_data->hasSymbol = false;
		emitItemChanged();
		return error;
	}
	return QString();
}
QString VipPlotCurve::symbolCondition() const
{
	return d_data->symbolCondition;
}

void VipPlotCurve::setOptimizeLargePenDrawing(bool enable)
{
	if (enable != d_data->optimizeLargePenDrawing) {
		d_data->optimizeLargePenDrawing = enable;
		emitItemChanged();
	}
}
bool VipPlotCurve::optimizeLargePenDrawing() const
{
	return d_data->optimizeLargePenDrawing;
}

int VipPlotCurve::findClosestPos(const VipPointVector& data, const VipPoint& pos, int axis, double maxDistance, bool continuous) const
{
	int index = -1;
	double dist = std::numeric_limits<double>::max();
	VipPoint item_pos = pos;

	if (data.isEmpty())
		return -1;

	// find first and last non NaN indexes
	int first = 0, last = data.size() - 1;
	while (first < data.size() && (vipIsNan(data[first].x()) || vipIsNan(data[first].y())))
		++first;
	if (first >= data.size())
		return -1;

	while (last >= 0 && (vipIsNan(data[last].x()) || vipIsNan(data[last].y())))
		--last;
	if (last < 0)
		return -1;

	// if the plot is continuous, the scale engine linear and we don't request the area for y scale, we can search only
	// for a sub part of the curve
	bool can_query_sub_part = axis == 0 && axes().size() && axes()[0]->scaleEngine()->isLinear() && continuous;
	double min_x = 0, max_x = 0;
	if (can_query_sub_part) {
		VipPoint pos_min = VipPoint(pos.x() - maxDistance, pos.y());
		VipPoint pos_max = VipPoint(pos.x() + maxDistance, pos.y());
		min_x = sceneMap()->invTransform(pos_min).x();
		max_x = sceneMap()->invTransform(pos_max).x();
		// inverted scale
		if (max_x < min_x)
			std::swap(min_x, max_x);

		// check if we are outside the vector boundaries
		if (data[first].x() > max_x || data[last].x() < min_x)
			return -1;
	}

	VipCoordinateSystemPtr map = sceneMap();

	// try to find a point at a vipDistance < maxDistance (in item's coordinates)
	for (int i = 0; i < data.size(); ++i) {
		if (vipIsNan(data[i].x()) || vipIsNan(data[i].y()))
			continue;

		if (can_query_sub_part) {
			// optimize search if can_query_sub_part is true
			if (data[i].x() < min_x)
				continue;
			else if (data[i].x() > max_x)
				break;
		}

		const VipPoint p = map->transform(data[i]);

		if (axis == 0) {
			item_pos.setY(p.y()); // y should always be valid
		}
		else if (axis == 1) {
			item_pos.setX(p.x()); // x should always be valid
		}

		const VipPoint diff = p - item_pos;
		if (diff.x() > maxDistance || diff.y() > maxDistance)
			continue;

		const double d = qSqrt(diff.x() * diff.x() + diff.y() * diff.y());
		if (d < maxDistance && d < dist) {
			dist = d;
			index = i;
		}
	}

	return index;
}

bool VipPlotCurve::areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const
{
	Locker locker(dataLock());

	legend = 0;
	if (axis == 0 && d_data->vectors.size() > 1 && d_data->sub_continuous) {
		// special case: we look for the points of interest on a vertical line that intersects multiple curves
		// that might overlapp on the x axis

		QPainterPath path;

		for (int i = 0; i < d_data->vectors.size(); ++i) {
			int index = findClosestPos(d_data->vectors[i], pos, axis, maxDistance, d_data->continuous[i]);
			if (index >= 0) {
				VipPoint found = sceneMap()->transform(d_data->vectors[i][index]);
				out_pos << found;
				if (symbol() && symbolVisible() && symbol()->style() != VipSymbol::None) {
					path |= symbol()->shape((QPointF)found);
				}
				else {
					QPainterPath p;
					p.addEllipse(QRectF(-5, -5, 11, 11));
					path |= p.translated((QPointF)found);
				}
			}
		}
		style.computePath(path);
		style.setBackgroundBrush(QBrush());
		style.setBorderPen(QPen(Qt::magenta, 2));
		return !out_pos.isEmpty();
	}

	const VipPointVector raw = rawData();
	int index = findClosestPos(raw, pos, axis, maxDistance, d_data->full_continuous);
	if (index >= 0) {
		VipPoint found = sceneMap()->transform(raw[index]);
		out_pos << found;
		if (symbol() && symbolVisible() && symbol()->style() != VipSymbol::None) {
			style.computePath(symbol()->shape((QPointF)found));
		}
		else {
			QPainterPath p;
			p.addEllipse(QRectF(-5, -5, 11, 11));
			style.computePath(p.translated((QPointF)found));
		}
		style.setBackgroundBrush(QBrush());
		style.setBorderPen(QPen(Qt::magenta, 2));
		return !out_pos.isEmpty();
	}
	return false;

	// VipPoint found;
	//  int index = -1;
	//  double dist = std::numeric_limits<double>::max();
	//  legend = 0;
	//
	// VipPoint item_pos = pos;
	//
	// //if the plot is contigous, the scale engine linear and we don't request the area for y scale, we can search only
	// //for a sub part of the curve
	// bool can_query_sub_part = axis != 1 && axes().size() && axes()[0]->scaleEngine()->isLinear() && d_data->full_continuous;
	// double min_x, max_x;
	// if (can_query_sub_part) {
	// VipPoint pos_min = VipPoint(pos.x() - maxDistance, pos.y());
	// VipPoint pos_max = VipPoint(pos.x() + maxDistance, pos.y());
	// min_x = sceneMap()->invTransform(pos_min).x();
	// max_x = sceneMap()->invTransform(pos_max).x();
	// //inverted scale
	// if (max_x < min_x)
	// std::swap(min_x, max_x);
	// }
	//
	// //try to find a point at a vipDistance < maxDistance (in item's coordinates)
	// const QVector<VipPoint> data = rawData();
	// for(int i=0; i < data.size(); ++i)
	// {
	// if (can_query_sub_part) {
	// //optimize search if can_query_sub_part is true
	// if (data[i].x() < min_x)
	//	continue;
	// else if (data[i].x() > max_x)
	//	break;
	// }
	//
	// const VipPoint p = sceneMap()->transform(data[i]);
	//
	// if (axis == 0) {
	// item_pos.setY(p.y()); //y should always be valid
	// }
	// else if(axis == 1) {
	// item_pos.setX(p.x()); //x should always be valid
	// }
	//
	// const VipPoint diff= p- item_pos;
	// if(diff.x() > maxDistance || diff.y() > maxDistance)
	// continue;
	//
	// const double d = qSqrt(diff.x()*diff.x() + diff.y()*diff.y());
	// if( d < maxDistance && d < dist)
	// {
	// dist = d;
	// found = p;
	// index = i;
	// }
	//
	// }
	//
	// if(index >= 0)
	// {
	// out_pos = found;
	// if(symbol() && symbolVisible())
	// {
	// VipShapeDevice device;
	// QPainter painter(&device);
	// //disable cache, otherwise the shape will always be a rectangle since we draw a pixmap
	// VipSymbol s(*symbol());
	// s.setCachePolicy(VipSymbol::NoCache);
	// s.drawSymbol(&painter, found);
	// QPainterPath p = device.shape();
	// p.closeSubpath();
	// style.computePath(p);
	// style.setBackgroundBrush(QBrush());
	// style.setBorderPen(QPen(Qt::magenta,2));
	// }
	// else
	// {
	// QPainterPath path;
	// path.addEllipse(QRectF(-5,-5,11,11));
	// path = path.translated(found);
	// style.computePath(path);
	// QColor color(Qt::red);
	// style.setBorderPen(QPen(color,2));
	// color.setAlpha(125);
	// style.setBackgroundBrush(QBrush(color));
	//
	// }
	// return true;
	// }
	//
	// return false;
}

static void insideRect(const QRectF& r, const QPolygonF& pts, QVector<QLineF>& out)
{
	QLineF left(r.topLeft(), r.bottomLeft());
	QLineF top(r.topLeft(), r.topRight());
	QLineF right(r.topRight(), r.bottomRight());
	QLineF bottom(r.bottomLeft(), r.bottomRight());
	QPointF inter;

	for (int i = 1; i < pts.size(); ++i) {
		QLineF l(pts[i - 1], pts[i]);

		bool c1 = r.contains(l.p1());
		bool c2 = r.contains(l.p2());

		if (c1 && c2) {
			out.append(l);
		}
		else if (c1) {
			if (l.QLINE_INTERSECTS(left, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(l.p1(), inter));
			else if (l.QLINE_INTERSECTS(top, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(l.p1(), inter));
			else if (l.QLINE_INTERSECTS(right, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(l.p1(), inter));
			else if (l.QLINE_INTERSECTS(bottom, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(l.p1(), inter));
		}
		else if (c2) {
			if (l.QLINE_INTERSECTS(left, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(inter, l.p2()));
			else if (l.QLINE_INTERSECTS(top, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(inter, l.p2()));
			else if (l.QLINE_INTERSECTS(right, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(inter, l.p2()));
			else if (l.QLINE_INTERSECTS(bottom, &inter) == QLineF::BoundedIntersection)
				out.append(QLineF(inter, l.p2()));
		}
		else {
			QPointF p1, p2;
			if (l.QLINE_INTERSECTS(left, &inter) == QLineF::BoundedIntersection) {
				if (p1 == QPointF())
					p1 = inter;
				else
					p2 = inter;
			}
			if (l.QLINE_INTERSECTS(top, &inter) == QLineF::BoundedIntersection) {
				if (p1 == QPointF())
					p1 = inter;
				else
					p2 = inter;
			}
			if (l.QLINE_INTERSECTS(right, &inter) == QLineF::BoundedIntersection) {
				if (p1 == QPointF())
					p1 = inter;
				else
					p2 = inter;
			}
			if (l.QLINE_INTERSECTS(bottom, &inter) == QLineF::BoundedIntersection) {
				if (p1 == QPointF())
					p1 = inter;
				else
					p2 = inter;
			}
			if (p1 != QPointF() && p2 != QPointF()) {
				out.append(QLineF(p1, p2));
			}
		}
	}
}

/// Draw an interval of the curve
///
/// \param painter VipPainter
/// \param xMap Maps x-values into pixel coordinates.
/// \param yMap Maps y-values into pixel coordinates.
/// \param from Index of the first point to be painted
/// \param to Index of the last point to be painted. If to < 0 the
///      curve will be painted to its last point.
///
/// \sa drawCurve(), drawSymbols(),

void VipPlotCurve::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();
	QList<QPolygonF> drawn_polygons;

	if (d_data->function) {
		// cheap metric to get the number of points
		int point_count = 1000;
		if (QGraphicsView* v = this->view())
			point_count = v->width() + v->height();

		// get x scale interval
		if (m->axes().size() != 2)
			return;
		VipInterval x_inter = m->axes().first()->scaleDiv().bounds();

		if (d_data->drawn_pcount != point_count || d_data->drawn_interval != x_inter) {

			if (d_data->draw_interval.isValid())
				x_inter = x_inter.intersect(d_data->draw_interval);

			vip_double step = x_inter.width() / point_count;
			vip_double x = x_inter.minValue();
			VipPointVector vec(point_count + 1);
			for (int i = 0; i < vec.size(); ++i) {
				vec[i] = VipPoint(x, d_data->function(x));
				x += step;
			}
			d_data->vectors = QList<VipPointVector>() << vec;
			d_data->continuous = QList<bool>() << true;
			d_data->drawn_pcount = point_count;
			d_data->drawn_interval = x_inter;
		}
	}

	for (int i = 0; i < d_data->vectors.size(); ++i) {
		// compute the polygons to be drawn
		drawn_polygons << computeSimplified(painter, m, d_data->vectors[i], d_data->continuous[i]);
		if (d_data->style == Steps)
			drawn_polygons.last() = computeSteps(drawn_polygons.last(), (d_data->attributes & Inverted));
	}

	if (testCurveAttribute(FillMultiCurves) && isSubContinuous() && drawn_polygons.size() > 1) {
		// fill the space between curves

		for (int i = 1; i < drawn_polygons.size(); ++i) {
			const QPolygonF p1 = drawn_polygons[i - 1];
			const QPolygonF p2 = drawn_polygons[i];
			// test overlapping on x axis
			if (p1.isEmpty() || p2.isEmpty()) //|| !VipInterval(p1.first().x(), p1.last().x()).intersects(VipInterval(p2.first().x(), p2.last().x())))
				continue;

			VipBoxStyle bstyle = d_data->boxStyle;
			bstyle.setBorderPen(QPen(Qt::NoPen));
			if (hasSubBrush(i - 1))
				bstyle.setBackgroundBrush(subBrush(i - 1));

			if (!bstyle.isTransparentBrush()) {

				QPolygonF full(p1.size() + p2.size() + 1);
				std::copy(p1.begin(), p1.end(), full.begin());
				std::copy(p2.rbegin(), p2.rend(), full.data() + p1.size());
				full.last() = full.first();

				bstyle.computePolyline(full);
				bstyle.drawBackground(painter);
			}
		}
	}

	for (int i = 0; i < drawn_polygons.size(); ++i) {
		// draw the curves

		// Qt 4.0.0 is slow when drawing lines, but it's even
		// slower when the painter has a brush. So we don't
		// set the brush before we really need it.

		// const VipPointVector points = drawn_polygons[i];
		// const VipPointVector simplified = computeSimplified(painter, m, points, d_data->continuous[i]);

		const QPolygonF simplified = drawn_polygons[i];

		bool drawSelected = isSelected() && selectedPen() != Qt::NoPen && selectedPen().color().alpha() != 0 && boxStyle().borderPen() != Qt::NoPen &&
				    boxStyle().borderPen().color().alpha() != 0 &&
				    style() != NoCurve //&&
				    // !(symbolVisible() && symbol() && symbol()->style() != VipSymbol::None)
				    && !computingShape();

		if (drawSelected) {
			// get the paint rect
			QRectF prect = m->clipPath(this).boundingRect();

			if (d_data->continuous[i]) //&& simplified.size()*0.7 > prect.width())
			{
				// use this method if the point density is high
				// extract the curve enveloppe
				double factor = (simplified.size() / prect.width()) * 2;
				factor = std::max(factor, 2.);
				double length = 0;
				QPolygonF enveloppe = extractEnveloppe(simplified, qRound(factor), &length);

				painter->save();

				QPen p = selectedPen();
				// p.setColor(applySelectionColor(pen().color()));

				if (length < 30000) // 40000
				{
					// small length: draw polyline (better rendering)
					painter->setPen(p);
					painter->setBrush(QBrush());
					// qint64 st = QDateTime::currentMSecsSinceEpoch();
					painter->drawPolygon(enveloppe);
					// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
					// vip_debug("drawPolygon1: %i\n", (int)el);
				}
				else if (enveloppe.size()) {
					// big length: draw succession of lines (WAY faster)
					// qint64 st = QDateTime::currentMSecsSinceEpoch();
					QVector<QLineF> lines;

					if (length > 60000) {
						lines.reserve(enveloppe.size() - 1);
						insideRect(m->clipPath(this).boundingRect(), enveloppe, lines);
					}
					else {
						lines.resize(enveloppe.size() - 1);
						for (int j = 1; j < enveloppe.size(); ++j)
							lines[j - 1] = (QLineF(enveloppe[j - 1], enveloppe[j]));
					}

					painter->setPen(p);
					painter->setBrush(QBrush());
					painter->drawLines(lines.data(), lines.size());
					// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
					// vip_debug("drawPolygon2 %i %i: %i\n", lines.size(),(int)length,(int)el);
				}
				painter->restore();
			}
			else {
				// draw the curve outline with the selection pen
				painter->save();
				painter->setPen(selectedPen());
				painter->setBrush(QBrush());
				drawCurve(painter, d_data->style, m, simplified, true, d_data->continuous[i], i);

				painter->restore();
			}
		}

		painter->save();
		drawCurve(painter, d_data->style, m, simplified, false, d_data->continuous[i], i);
		painter->restore();

		if (d_data->symbol && symbolVisible() && symbol()->style() != VipSymbol::None) {
			painter->save();
			drawSymbols(painter, *d_data->symbol, m, d_data->vectors[i], d_data->continuous[i], i);
			painter->restore();
		}
	}

	// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	// vip_debug("curve %s: %i ms\n",title().text().toLatin1().data(), (int)el);
}

void VipPlotCurve::drawSelected(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	this->draw(painter, m);
}

QPolygonF VipPlotCurve::computeSimplified(QPainter* painter, const VipCoordinateSystemPtr& m, const VipPointVector& points, bool continuous) const
{
	if (VipPainter::isVectoriel(painter))
		return m->transform(points);

	bool cartesian = isPerfectRightCartesiant(painter, m);

	if (d_data->style == Sticks) {
		if (!continuous)
			return m->transform(points);

		// extract simplified sticks
		QPolygonF res;
		VipInterval x_inter = m->axes().first()->scaleDiv().bounds().normalized();

		int last_x = INT_MAX;
		for (int i = 0; i < points.size(); ++i) {
			if (points[i].x() >= x_inter.minValue()) {
				QPointF tr = m->transform(points[i]);
				QPointF base = m->transform(VipPoint(points[i].x(), d_data->baseline));
				int x = qRound(tr.x());

				// add the max y first
				if (base.y() < tr.y())
					std::swap(base.ry(), tr.ry());

				if (!cartesian || res.isEmpty() || x != last_x) {
					// just add the stick
					res.append(base);
					res.append(tr);
					last_x = x;
				}
				else {
					if (base.y() > res[res.size() - 2].y())
						res[res.size() - 2].setY(base.y());
					if (tr.y() < res[res.size() - 1].y())
						res[res.size() - 1].setY(tr.y());
				}
			}
			if (points[i].x() > x_inter.maxValue())
				break;
		}
		return res;
	}
	else if (style() == Dots) {
		// extract simplified dots
		QPolygonF res;
		VipInterval x_inter = m->axes().first()->scaleDiv().bounds().normalized();
		for (int i = 0; i < points.size(); ++i) {
			if (points[i].x() >= x_inter.minValue()) {
				QPointF tr = m->transform(points[i]);
				if (res.size() && res.last().toPoint() == tr.toPoint()) {
					// ignore point
				}
				else
					res.append(tr);
			}
			if (points[i].x() > x_inter.maxValue())
				break;
		}
		return res;
	}
	else if (style() == NoCurve) {
		// no curve: return empty vector
		return QPolygonF();
	}
	else if (style() == Lines || style() == Steps) {
		// Lines and Steps: points merging

		if (cartesian && continuous && points.size() && points.size() > 500) {
			VipInterval x_inter = m->axes().first()->scaleDiv().bounds().normalized();
			VipInterval y_inter = m->axes().last()->scaleDiv().bounds().normalized();

			// x and y downsampling
			d_data->merge.vector.clear();
			int sign = 0;
			for (int i = 0; i < points.size(); ++i) {
				const VipPoint pt = points[i];

				if (pt.x() < x_inter.minValue() && i < points.size() - 1 && points[i + 1].x() < x_inter.minValue())
					continue;

				int cur_sign = (pt.y() < y_inter.minValue()) ? -1 : ((pt.y() > y_inter.maxValue()) ? 1 : 0);
				if (cur_sign != sign) {
					if (sign == 0) {
						// we were inside and went outside
						d_data->merge.add(m->transform(pt));
					}
					else {
						// we were outside and went inside, or we went from outside top to outside bottom (or inverse)
						// in any case , add the previous value and this one
						// if (merge.last() != points[i - 1]) //TOCHECK
						d_data->merge.add(m->transform(points[i - 1]));
						d_data->merge.add(m->transform(pt));
					}
				}
				else if (cur_sign == 0)
					// input.append(pt);
					d_data->merge.add(m->transform(pt));

				sign = cur_sign;

				// outside scale bounds
				if (pt.x() > x_inter.maxValue())
					break;
			}
			d_data->merge.finish();
			return d_data->merge.vector;
		}
		else
			return m->transform(points);
	}

	return QPolygonF();
}

static double norm(const QPointF& pt)
{
	return qSqrt(pt.x() * pt.x() + pt.y() * pt.y());
}

QPolygonF VipPlotCurve::extractEnveloppe(const QPolygonF& points, int factor, double* length) const
{
	int size = ((points.size() / factor) - 1) * factor;
	if (size < 0)
		return points;

	QPolygonF upper;
	upper.reserve(size / factor);
	QPolygonF lower;
	lower.reserve(size / factor);

	double one_on_factor = 1.0 / (double)factor;
	for (int i = 0; i < size; i += factor) {
		double dist = points[i + factor].x() - points[i].x();
		if (dist > factor) {
			int end = i + factor;
			for (int j = i; j <= end; ++j) {
				if (upper.size()) {
					*length += norm(points[j] - upper.last());
					*length += norm(points[j] - lower.last());
				}
				upper.append(points[j]);
				lower.append(points[j]);
			}
		}
		else {
			double x = points[i].x();
			double ymin, ymax;
			ymin = ymax = points[i].y();
			for (int j = i + 1; j < i + factor; ++j) {
				x += points[j].x();
				ymin = std::min(ymin, points[j].y());
				ymax = std::max(ymax, points[j].y());
			}

			x *= one_on_factor;
			upper.append(QPointF(x, ymax));
			lower.append(QPointF(x, ymin));

			if (upper.size() > 1) {
				*length += norm(upper.last() - upper[upper.size() - 2]);
				*length += norm(lower.last() - lower[lower.size() - 2]);
			}
		}
	}

	for (int i = size; i < points.size(); ++i) {
		if (upper.size() > 1) {
			*length += norm(points[i] - upper.last());
			*length += norm(points[i] - lower.last());
		}
		upper.append(points[i]);
		lower.append(points[i]);
	}

	std::reverse(lower.begin(), lower.end());

	return upper + lower;
}

/// \brief Draw the line part (without symbols) of a curve interval.
/// \param painter VipPainter
/// \param style curve style, see VipPlotCurve::CurveStyle
/// \param xMap x map
/// \param yMap y map
/// \param from index of the first point to be painted
/// \param to index of the last point to be painted
/// \sa draw(), drawDots(), drawLines(), drawSteps(), drawSticks()
QPolygonF VipPlotCurve::drawCurve(QPainter* painter, int style, const VipCoordinateSystemPtr& m, const QPolygonF& simplified, bool draw_selected, bool continuous, int index) const
{
	switch (style) {
		case Lines:
			return drawLines(painter, m, simplified, draw_selected, continuous, index);
			break;
		case Sticks:
			return drawSticks(painter, m, simplified, draw_selected, continuous, index);
			break;
		case Steps:
			return drawSteps(painter, m, simplified, draw_selected, continuous, index);
			break;
		case Dots:
			return drawDots(painter, m, simplified, draw_selected, continuous, index);
			break;
		case NoCurve:
		default:
			break;
	}
	return QPolygonF();
}

/// \brief Draw lines
///
/// If the CurveAttribute Fitted is enabled a QwtCurveFitter tries
/// to interpolate/smooth the curve, before it is painted.
///
/// \param painter VipPainter
/// \param xMap x map
/// \param yMap y map
/// \param from index of the first point to be painted
/// \param to index of the last point to be painted
///
/// \sa setCurveAttribute(), setCurveFitter(), draw(),
///   drawLines(), drawDots(), drawSteps(), drawSticks()

static bool supportFastDrawPolygon(const QPen& p)
{
	return p.widthF() > 1 && p.widthF() < 12 && p.color().alpha() == 255 && p.style() == Qt::SolidLine;
}

static void drawPolygonHelper(const QPolygonF& poly, QPainter* painter, const QPen& pen)
{
	// polyline drawing optimization for pen width > 1
	// the rendering is not as good, but could be more than 10 times faster,
	// which is HUGE for streaming purposes

	// draw by steps of 0.5
	/* QPen p = pen;
	double step_size = 0.5;
	//0.5;
	int steps = std::round(p.widthF() / step_size);
	QTransform tr;
	tr.translate((-steps / 2.) * step_size, (-steps / 2.) * step_size);
	painter->save();
	painter->setTransform(tr, true);
	p.setWidth(0);
	painter->setPen(p);
	for (int i = 0; i < steps; ++i) {
		painter->drawPolyline(poly);
		painter->setTransform(QTransform().translate(step_size, step_size), true);
	}
	painter->restore();
	*/
	painter->save();
	QPen p = pen;
	p.setJoinStyle(Qt::RoundJoin);
	painter->setPen(p);
	for (int i = 1; i < poly.size(); ++i) {
		painter->drawLine(poly[i - 1], poly[i]);
	}
	painter->restore();
}

QPolygonF VipPlotCurve::drawLines(QPainter* painter,
				  const VipCoordinateSystemPtr& m,
				  const QPolygonF& points,
				  bool draw_selected,
				  bool // continuous
				  ,
				  int index) const
{

	QPolygonF polyline;
	polyline = points; // m->transform(points);

	VipBoxStyle bstyle = d_data->boxStyle;
	if (hasSubPen(index))
		bstyle.setBorderPen(subPen(index));

	if (draw_selected) {
		bstyle.setBorderPen(selectedPen());
		bstyle.setBackgroundBrush(QBrush());
	}

	const bool doFill =
	  (boxStyle().backgroundBrush().style() != Qt::NoBrush && (boxStyle().backgroundBrush().color().alpha() > 0)) && (!testCurveAttribute(FillMultiCurves) || d_data->full_continuous);

	if (doFill && !draw_selected) {
		if (this->testCurveAttribute(ClosePolyline)) {
			// closePolyline( painter, m, points, polyline);
			closePolyline(painter, m, polyline);
			bstyle.computePolyline(polyline);
			bstyle.draw(painter);
		}
		else {
			QPolygonF border = polyline;
			// closePolyline( painter, m, points, polyline);
			closePolyline(painter, m, polyline);
			bstyle.computePolyline(polyline);
			bstyle.drawBackground(painter);
			bstyle.computePolyline(border);
			bstyle.drawBorder(painter);
		}
	}
	else {
		if (this->testCurveAttribute(ClosePolyline))
			closePolyline(painter, m, polyline);

		if (draw_selected) {
			QPen p = selectedPen();
			// p.setColor(applySelectionColor(pen().color()));
			bstyle.setBorderPen(p);
		}

		if (draw_selected && polyline.size() > 300 && bstyle.borderRadius() == 0) {
			// optimize selection drawing which is too slow (because of pen width)
			QVector<QLineF> lines(polyline.size() - 1);
			for (int i = 1; i < polyline.size(); ++i)
				lines[i - 1] = QLineF(polyline[i - 1], polyline[i]);
			painter->setPen(bstyle.borderPen());
			painter->drawLines(lines.data(), lines.size());
		}
		else {
			if (bstyle.borderRadius() == 0) {
				// optimize drawing for simple curves
				painter->setPen(bstyle.borderPen());
				painter->setBrush(QBrush());
				if (d_data->optimizeLargePenDrawing && !VipPainter::isOpenGL(painter) && !VipPainter::isVectoriel(painter) && supportFastDrawPolygon(bstyle.borderPen())) {
					// polyline drawing optimization for pen width > 1
					drawPolygonHelper(polyline, painter, bstyle.borderPen());
				}
				else {
					painter->drawPolyline(polyline);
				}
			}
			else {
				bstyle.computePolyline(polyline);
				bstyle.drawBorder(painter);
			}
		}
	}

	return polyline;
}

/// Draw sticks
///
/// \param painter VipPainter
/// \param xMap x map
/// \param yMap y map
/// \param from index of the first point to be painted
/// \param to index of the last point to be painted
///
/// \sa draw(), drawCurve(), drawDots(), drawLines(), drawSteps()
QPolygonF VipPlotCurve::drawSticks(QPainter* painter, const VipCoordinateSystemPtr& m, const QPolygonF& points, bool draw_selected, bool, int index) const
{
	painter->save();

	// const bool doFill = ( d_data->boxStyle.backgroundBrush().style() != Qt::NoBrush );
	//
	// const QVector<VipPoint> polygon = points;
	// QVector<VipPoint> tr_polygon(polygon.size());
	// QPainterPath path;
	// QVector<VipPoint> p(2);
	//
	//
	// bool is_cartesian = isPerfectRightCartesiant(painter, m);
	//
	// if (is_cartesian)
	// {
	// double baseline =m->axes()[1]->position(d_data->baseline).y();
	// for (int i = 0; i < polygon.size(); i++)
	// {
	// p[1] = (polygon[i]);
	// tr_polygon[i] = p[1];
	// p[0] = (VipPoint(polygon[i].x(), baseline));
	// path.addPolygon(p);
	// }
	// }
	// else
	// {
	// for (int i = 0; i < polygon.size(); i++)
	// {
	// VipPoint inv = m->invTransform(polygon[i]);
	// inv.setY(d_data->baseline);
	// inv = m->transform(inv);
	// p[1] = (polygon[i]);
	// tr_polygon[i] = p[1];
	// p[0] = inv;
	// path.addPolygon(p);
	// }
	// }
	//
	// VipBoxStyle bstyle = d_data->boxStyle;
	// if (draw_selected)
	// {
	// bstyle.setBorderPen(selectedPen());
	// bstyle.setBackgroundBrush(QBrush());
	// }
	//
	// if(doFill)
	// {
	// closePolyline(painter, m, tr_polygon);
	// bstyle.computePolyline(tr_polygon);
	// bstyle.drawBackground(painter);
	// }
	//
	// bstyle.computePath(path);
	// bstyle.drawBorder(painter);

	if (draw_selected) {

		QPen p = selectedPen();
		// p.setColor(applySelectionColor(pen().color()));

		painter->setPen(p);
		painter->setBrush(QBrush());
		painter->drawLines(points);
	}
	else {
		const bool doFill = (boxStyle().backgroundBrush().style() != Qt::NoBrush && (boxStyle().backgroundBrush().color().alpha() > 0)) && !testCurveAttribute(FillMultiCurves);

		if (doFill) {
			QPolygonF top, bottom;
			for (int i = 0; i < points.size(); i += 2) {
				QPointF p1 = points[i];
				QPointF p2 = points[i + 1];
				if (p1.y() < p2.y())
					std::swap(p1.ry(), p2.ry());
				top.append(p1);
				bottom.append(p2);
			}
			// revsere bottom
			int s = bottom.size() / 2;
			for (int i = 0; i < s; ++i)
				std::swap(bottom[i], bottom[bottom.size() - i - 1]);
			QPolygonF polyline = top + bottom;

			VipBoxStyle bstyle = boxStyle();
			bstyle.setBorderPen(QPen(Qt::NoPen));
			if (this->testCurveAttribute(ClosePolyline)) {
				closePolyline(painter, m, polyline);
				bstyle.computePolyline(polyline);
				bstyle.draw(painter);
			}
			else {
				QPolygonF border = polyline;
				closePolyline(painter, m, polyline);
				bstyle.computePolyline(polyline);
				bstyle.drawBackground(painter);
				bstyle.computePolyline(border);
				bstyle.drawBorder(painter);
			}
		}

		// draw border
		QPen p = d_data->boxStyle.borderPen();
		if (hasSubPen(index))
			p = (subPen(index));
		painter->setPen(p);
		painter->setBrush(QBrush());
		painter->drawLines(points);
	}

	painter->restore();
	return points;
}

/// Draw dots
///
/// \param painter VipPainter
/// \param xMap x map
/// \param yMap y map
/// \param from index of the first point to be painted
/// \param to index of the last point to be painted
///
/// \sa draw(), drawCurve(), drawSticks(), drawLines(), drawSteps()
QPolygonF VipPlotCurve::drawDots(QPainter* painter, const VipCoordinateSystemPtr& m, const QPolygonF& pts, bool draw_selected, bool, int index) const
{
	const bool doFill = (d_data->boxStyle.backgroundBrush().style() != Qt::NoBrush) && !testCurveAttribute(FillMultiCurves);

	const QPolygonF polygon = pts;
	QPolygonF points = // m->transform
	  (polygon);

	VipBoxStyle bstyle = d_data->boxStyle;
	if (hasSubPen(index))
		bstyle.setBorderPen(subPen(index));

	if (draw_selected) {
		bstyle.setBorderPen(selectedPen());
		bstyle.setBackgroundBrush(QBrush());
	}

	if (doFill) {
		// closePolyline( painter, m, polygon, points );
		closePolyline(painter, m, points);
		bstyle.computePolyline(points);
		bstyle.drawBackground(painter);
	}

	QPen p = bstyle.borderPen();
	if (draw_selected) {
		// p.setColor(applySelectionColor(pen().color()));
	}

	if (bstyle.adaptativeGradientPen().type() != VipAdaptativeGradient::NoGradient) {
		QRectF bounding(d_data->bounding[0].minValue(), d_data->bounding[1].minValue(), d_data->bounding[0].width(), d_data->bounding[1].width());
		p.setBrush(bstyle.adaptativeGradientPen().createBrush(p.brush(), QPolygonF(m->transform(bounding)).boundingRect()));
	}

	painter->setBrush(QBrush());
	painter->setPen(p);
	VipPainter::drawPoints(painter, points.data(), points.size());

	return points;
}

/// Draw step function
///
/// The direction of the steps depends on Inverted attribute.
///
/// \param painter VipPainter
/// \param xMap x map
/// \param yMap y map
/// \param from index of the first point to be painted
/// \param to index of the last point to be painted
///
/// \sa CurveAttribute, setCurveAttribute(),
///   draw(), drawCurve(), drawDots(), drawLines(), drawSticks()
QPolygonF VipPlotCurve::drawSteps(QPainter* painter, const VipCoordinateSystemPtr& m, const QPolygonF& points, bool draw_selected, bool, int index) const
{
	const QPolygonF polygon = points;

	QPolygonF polygon_tr = // m->transform
	  (polygon);
	VipBoxStyle bstyle = d_data->boxStyle;
	if (hasSubPen(index))
		bstyle.setBorderPen(subPen(index));

	if (draw_selected) {
		bstyle.setBorderPen(selectedPen());
		bstyle.setBackgroundBrush(QBrush());
	}

	const bool doFill = (d_data->boxStyle.backgroundBrush().style() != Qt::NoBrush) && !testCurveAttribute(FillMultiCurves);

	if (doFill && !draw_selected) {
		if (this->testCurveAttribute(ClosePolyline)) {
			// closePolyline( painter, m, polygon, polygon_tr);
			closePolyline(painter, m, polygon_tr);
			bstyle.computePolyline(polygon_tr);
			bstyle.draw(painter);
		}
		else {
			QPolygonF border = polygon_tr;
			// closePolyline( painter, m, polygon, polygon_tr);
			closePolyline(painter, m, polygon_tr);
			bstyle.computePolyline(polygon_tr);
			bstyle.drawBackground(painter);
			bstyle.computePolyline(border);
			bstyle.drawBorder(painter);
		}
	}
	else {
		if (this->testCurveAttribute(ClosePolyline))
			// closePolyline(painter,m, polygon, polygon_tr);
			closePolyline(painter, m, polygon_tr);

		if (draw_selected) {
			QPen p = selectedPen();
			// p.setColor(applySelectionColor(pen().color()));
			bstyle.setBorderPen(p);
		}

		bstyle.computePolyline(polygon_tr);
		bstyle.drawBorder(painter);
	}

	return polygon_tr;
}

/// Specify an attribute for drawing the curve
///
/// \param attribute Curve attribute
/// \param on On/Off
///
/// /sa testCurveAttribute(), setCurveFitter()
void VipPlotCurve::setCurveAttribute(CurveAttribute attribute, bool on)
{
	if (bool(d_data->attributes & attribute) == on)
		return;

	if (on)
		d_data->attributes |= attribute;
	else
		d_data->attributes &= ~attribute;

	emitItemChanged();
}

/// \return true, if attribute is enabled
/// \sa setCurveAttribute()
bool VipPlotCurve::testCurveAttribute(CurveAttribute attribute) const
{
	return d_data->attributes & attribute;
}

void VipPlotCurve::setCurveAttributes(CurveAttributes attributes)
{
	if (d_data->attributes != attributes) {
		d_data->attributes = attributes;
		emitItemChanged();
	}
}

VipPlotCurve::CurveAttributes VipPlotCurve::curveAttributes() const
{
	return d_data->attributes;
}

void VipPlotCurve::setBoxStyle(const VipBoxStyle& bs)
{
	d_data->boxStyle = bs;
	emitItemChanged();
}

const VipBoxStyle& VipPlotCurve::boxStyle() const
{
	return d_data->boxStyle;
}

VipBoxStyle& VipPlotCurve::boxStyle()
{
	return d_data->boxStyle;
}

void VipPlotCurve::setPen(const QPen& p)
{
	d_data->boxStyle.setBorderPen(p);
}
void VipPlotCurve::setPenColor(const QColor& c)
{
	QPen p = d_data->boxStyle.borderPen();
	p.setColor(c);
	d_data->boxStyle.setBorderPen(p);
}
QPen VipPlotCurve::pen() const
{
	return d_data->boxStyle.borderPen();
}

void VipPlotCurve::setBrush(const QBrush& b)
{

	d_data->boxStyle.setBackgroundBrush(b);
}
void VipPlotCurve::setBrushColor(const QColor& c)
{
	QBrush b = d_data->boxStyle.backgroundBrush();
	b.setColor(c);
	d_data->boxStyle.setBackgroundBrush(b);
}
QBrush VipPlotCurve::brush() const
{
	return d_data->boxStyle.backgroundBrush();
}

void VipPlotCurve::setSubPen(int index, const QPen& p)
{
	d_data->subPen[index] = p;
}
QPen VipPlotCurve::subPen(int index) const
{
	QMap<int, QPen>::const_iterator it = d_data->subPen.find(index);
	if (it != d_data->subPen.end())
		return it.value();
	return QPen(Qt::NoPen);
}
bool VipPlotCurve::hasSubPen(int index, QPen* p) const
{
	QMap<int, QPen>::const_iterator it = d_data->subPen.find(index);
	if (it != d_data->subPen.end()) {
		if (p)
			*p = it.value();
		return true;
	}
	if (p)
		*p = QPen(Qt::NoPen);
	return false;
}

void VipPlotCurve::setSubBrush(int index, const QBrush& b)
{
	d_data->subBrush[index] = b;
}
QBrush VipPlotCurve::subBrush(int index) const
{
	QMap<int, QBrush>::const_iterator it = d_data->subBrush.find(index);
	if (it != d_data->subBrush.end())
		return it.value();
	return QBrush();
}
bool VipPlotCurve::hasSubBrush(int index, QBrush* p) const
{
	QMap<int, QBrush>::const_iterator it = d_data->subBrush.find(index);
	if (it != d_data->subBrush.end()) {
		if (p)
			*p = it.value();
		return true;
	}
	if (p)
		*p = QBrush();
	return false;
}

/// \brief Complete a polygon to be a closed polygon including the
///      area between the original polygon and the baseline.
///
/// \param painter VipPainter
/// \param xMap X map
/// \param yMap Y map
/// \param polygon Polygon to be completed
// int VipPlotCurve::closePolyline(QPainter *,const VipCoordinateSystemPtr & m, const QVector<VipPoint> & rawPolygon,QVector<VipPoint> & polygon ) const
//  {
//   if ( polygon.size() < 2 )
//       return 0;
//
//   if(d_data->baseline != Vip::InvalidValue)
//   {
//  const double baseline = d_data->baseline;
//  const VipPoint p1(rawPolygon.last().x(),baseline);
//  const VipPoint p2(rawPolygon.first().x(),baseline);
//
//  polygon += m->transform(p1);
//  polygon += m->transform(p2);
//  polygon += m->transform(rawPolygon.first());
//  return 3;
//   }
//   else
//   {
//   	polygon += polygon.first();
//   	return 1;
//   }
//
//  }

int VipPlotCurve::closePolyline(QPainter*, const VipCoordinateSystemPtr& m, QPolygonF& polygon) const
{
	if (polygon.size() < 2)
		return 0;

	if (!vipIsNan(d_data->baseline)) // != Vip::InvalidValue) //TOCHECK
	{
		const vip_double baseline = d_data->baseline;
		const VipPoint p1(m->invTransform(polygon.last()).x(), baseline);
		const VipPoint p2(m->invTransform(polygon.first()).x(), baseline);

		polygon += (QPointF)m->transform(p1);
		polygon += (QPointF)m->transform(p2);
		polygon += polygon.first();
		return 3;
	}
	else {
		polygon += polygon.first();
		return 1;
	}
}

/// Draw symbols
///
/// \param painter VipPainter
/// \param symbol Curve symbol
/// \param xMap x map
/// \param yMap y map
/// \param from Index of the first point to be painted
/// \param to Index of the last point to be painted
///
/// \sa setSymbol(), drawSeries(), drawCurve()
void VipPlotCurve::drawSymbols(QPainter* painter, const VipSymbol& symbol, const VipCoordinateSystemPtr& m, const VipPointVector& pts, bool continuous, int) const
{
	QPolygonF points;

	// only keep the points inside the scales
	VipInterval x_inter = m->axes().first()->scaleDiv().bounds().normalized();
	VipInterval y_inter = m->axes().last()->scaleDiv().bounds().normalized();

	points.reserve(1000);
	QPoint prev(-1, -1);
	d_data->hasSymbol = false;
	for (int i = 0; i < pts.size(); ++i) {
		if (x_inter.contains(pts[i].x()) && y_inter.contains(pts[i].y())) {
			if (d_data->parseCondition) {
				if (!d_data->parseCondition->isValid(pts[i]))
					continue;
			}
			QPointF p = m->transform(pts[i]);
			if (continuous) {
				QPoint painter_pos = p.toPoint();
				if (painter_pos == prev) {
					prev = painter_pos;
					continue;
				}
				prev = painter_pos;
			}
			points.append(p);
		}
	}

	d_data->hasSymbol = points.size() > 0;

	if (this->computingShape()) {
		// this is WAY faster than drawing the symbols into a VipShapeDevice
		QPainterPath r = symbol.extractShape(&d_data->shapeBitmap, m->clipPath(this).boundingRect().toRect(), points.data(), points.size());
		painter->drawPath(r);
	}
	else {

		bool draw_selection = isSelected() && (selectedPen() != Qt::NoPen && selectedPen().color().alpha() != 0) && symbol.style() != VipSymbol::Pixmap &&
				      symbol.style() != VipSymbol::SvgDocument && symbol.style() != VipSymbol::UserStyle && !computingShape();

		if (draw_selection) {
			VipSymbol s(symbol);
			s.setBrush(QBrush());
			QPen p = selectedPen();
			// p.setColor(applySelectionColor(pen().color()));
			s.setPen(p);
			s.drawSymbols(painter, points);
		}

		symbol.drawSymbols(painter, points);
	}
}

/// \brief Set the value of the baseline
///
/// The baseline is needed for filling the curve with a brush or
/// the Sticks drawing style.
///
/// The default value is 0.0.
///
/// \param value Value of the baseline
/// \sa baseline(), setBrush(), setStyle()
void VipPlotCurve::setBaseline(vip_double value)
{
	if (d_data->baseline != value) {
		d_data->baseline = value;
		emitItemChanged();
	}
}

/// \return Value of the baseline
/// \sa setBaseline()
vip_double VipPlotCurve::baseline() const
{
	return d_data->baseline;
}

QList<VipText> VipPlotCurve::legendNames() const
{
	return QList<VipText>() << this->title();
}

QRectF VipPlotCurve::drawLegend(QPainter* painter, const QRectF& rect, int) const
{
	painter->save();
	painter->setRenderHints(this->renderHints());

	if (d_data->legendAttributes == 0 || (d_data->legendAttributes & VipPlotCurve::LegendShowBrush)) {
		const bool doFill = ((d_data->boxStyle.backgroundBrush().style() != Qt::NoBrush) && style() != NoCurve) || testCurveAttribute(FillMultiCurves);
		if (doFill) {
			VipBoxStyle bs = d_data->boxStyle;
			QBrush b = brush();
			// find the first non transparent brush
			for (QMap<int, QBrush>::const_iterator it = d_data->subBrush.begin(); it != d_data->subBrush.end(); ++it) {
				if (it.value().color().alpha() != 0 && it.value().style() != Qt::NoBrush) {
					b = it.value();
					break;
				}
			}
			bs.setBackgroundBrush(b);
			bs.setBorderPen(QPen(Qt::transparent));
			if (testCurveAttribute(FillMultiCurves) && d_data->sub_continuous && d_data->subBrush.size() && d_data->vectors.size() > 1)
				bs.setBackgroundBrush(d_data->subBrush.first());

			bs.computeRect(rect);
			bs.drawBackground(painter);
		}
	}

	if (d_data->legendAttributes & VipPlotCurve::LegendShowLine && style() != NoCurve) {
		VipBoxStyle bs = d_data->boxStyle;
		QPen p = pen();
		// find the first non transparent pen
		for (QMap<int, QPen>::const_iterator it = d_data->subPen.begin(); it != d_data->subPen.end(); ++it) {
			if (it.value().color().alpha() != 0 && it.value().style() != Qt::NoPen) {
				p = it.value();
				break;
			}
		}
		bs.setBorderPen(p);
		QPolygonF line(2);
		line[0] = QPointF(rect.left(), rect.center().y());
		line[1] = QPointF(rect.right(), line[0].y());
		bs.computePolyline(line);
		bs.drawBorder(painter);
	}

	if (d_data->legendAttributes & VipPlotCurve::LegendShowSymbol && symbolVisible() && symbol() && symbol()->style() != VipSymbol::None) {
		if (d_data->symbol && d_data->hasSymbol) {
			d_data->symbol->drawSymbol(painter, rect);
		}
	}

	painter->restore();
	return rect;
}

QList<VipInterval> VipPlotCurve::plotBoundingIntervals() const
{
	// if (rawData().isEmpty())
	//  return QList<VipInterval>();
	//
	// QList<VipInterval> tmp = d_data->bounding;
	// return tmp;
	return QList<VipInterval>() << d_data->bounding[0] << d_data->bounding[1];
}

QPointF VipPlotCurve::drawSelectionOrderPosition(const QFont& font, Qt::Alignment align, const QRectF& area_bounding_rect) const
{
	QPointF res = VipPlotItem::drawSelectionOrderPosition(font, align, area_bounding_rect);
	if (d_data->sub_continuous) {
		// TODO, find a better location
	}
	return res;
}

void VipPlotCurve::setData(const QVariant& v)
{
	dataBoundingRect(v.value<VipPointVector>());
	VipPlotItemDataType::setData(v);
}

const QList<VipPointVector>& VipPlotCurve::vectors() const
{
	return d_data->vectors;
}
const QList<bool> VipPlotCurve::continuousVectors() const
{
	return d_data->continuous;
}
bool VipPlotCurve::isFullContinuous() const
{
	return d_data->full_continuous;
}
bool VipPlotCurve::isSubContinuous() const
{
	return d_data->sub_continuous;
}

void VipPlotCurve::resetFunction()
{
	d_data->function = std::function<vip_double(vip_double)>();
	d_data->scale_interval = VipInterval();
	d_data->draw_interval = VipInterval();
	d_data->drawn_pcount = 0;
	d_data->drawn_interval = VipInterval();
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

void VipPlotCurve::setFunction(const std::function<vip_double(vip_double)>& fun, const VipInterval& scale_interval, const VipInterval& draw_interval)
{
	d_data->function = fun;
	d_data->scale_interval = scale_interval.normalized();
	d_data->draw_interval = draw_interval;
	if (d_data->draw_interval.isValid())
		d_data->scale_interval = d_data->draw_interval.intersect(d_data->scale_interval);

	d_data->drawn_pcount = 0;
	d_data->drawn_interval = VipInterval();

	setRawData(VipPointVector());

	// Extract minimum X/Y for the scale interval

	double step = d_data->scale_interval.width() / 1000;
	vip_double x = d_data->scale_interval.minValue();
	vip_double miny, maxy;
	miny = maxy = d_data->function(x);
	x += step;
	for (int i = 0; i < 1000; ++i) {
		vip_double v = d_data->function(x);
		miny = std::min(miny, v);
		maxy = std::max(maxy, v);
		x += step;
	}
	dataLock()->lock();

	d_data->bounding[0] = d_data->scale_interval;
	d_data->bounding[1] = VipInterval(miny, maxy);
	d_data->sub_continuous = true;

	dataLock()->unlock();

	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

QPainterPath VipPlotCurve::shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch() ;
	VipShapeDevice device;
	QPainter painter(&device);
	this->draw(&painter, m);

	QPainterPath res;

	if (view()) {
		res = device.shape(7);
		if (!(boxStyle().backgroundBrush().color().alpha() == 0 || boxStyle().backgroundBrush().style() == Qt::NoBrush))
			res.addPath(device.shape());
	}
	else {
		if (boxStyle().backgroundBrush().color().alpha() == 0 || boxStyle().backgroundBrush().style() == Qt::NoBrush)
			res = device.shape(7);
		else
			res = device.shape();
	}

	// qint64 en2 = QDateTime::currentMSecsSinceEpoch() - st;
	// vip_debug("shape %s: %i\n", title().text().toLatin1().data(), (int)en2);
	return res;
}

bool VipPlotCurve::hasState(const QByteArray& state, bool enable) const
{
	//'none', 'lines', 'sticks', dots ', ' steps'.
	if (state == "none")
		return (style() == NoCurve) == enable;
	if (state == "lines")
		return (style() == Lines) == enable;
	if (state == "sticks")
		return (style() == Sticks) == enable;
	if (state == "dots")
		return (style() == Dots) == enable;
	if (state == "steps")
		return (style() == Steps) == enable;
	return VipPlotItem::hasState(state, enable);
}

bool VipPlotCurve::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (strcmp(name, "curve-style") == 0) {
		int v = value.toInt();
		if (v < NoCache || v > Dots)
			return false;
		this->setStyle((VipPlotCurve::CurveStyle)v);
		return true;
	}
	else if (strcmp(name, "curve-attribute") == 0) {
		int v = value.toInt();
		if (v < 0 || v > 7)
			return false;
		this->setCurveAttributes((VipPlotCurve::CurveAttributes)v);
		return true;
	}
	else if (strcmp(name, "legend") == 0) {
		int v = value.toInt();
		if (v < 0 || v > 7)
			return false;
		this->setLegendAttributes((VipPlotCurve::LegendAttributes)v);
		return true;
	}
	else if (strcmp(name, "symbol") == 0) {
		int v = value.toInt();
		if (v < -1 || v > VipSymbol::Hexagon)
			return false;
		VipSymbol sym = this->symbol() ? *this->symbol() : VipSymbol();
		sym.setStyle((VipSymbol::Style)v);
		setSymbol(new VipSymbol(sym));
		setSymbolVisible(true);
		return true;
	}
	else if (strcmp(name, "symbol-size") == 0) {
		double v = value.toDouble();
		VipSymbol sym = this->symbol() ? *this->symbol() : VipSymbol();
		sym.setSize(QSizeF(v, v));
		setSymbol(new VipSymbol(sym));
		setSymbolVisible(true);
		return true;
	}
	else if (strcmp(name, "symbol-border") == 0) {
		QPen p = value.value<QPen>();
		VipSymbol sym = this->symbol() ? *this->symbol() : VipSymbol();
		sym.setPen(p);
		setSymbol(new VipSymbol(sym));
		setSymbolVisible(true);
		return true;
	}
	else if (strcmp(name, "symbol-background") == 0) {
		// QBrush b = value.value<QBrush>();
		VipSymbol sym = this->symbol() ? *this->symbol() : VipSymbol();
		if (value.userType() == qMetaTypeId<QBrush>())
			sym.setBrush(value.value<QBrush>());
		else {
			QBrush b = sym.brush();
			b.setColor(value.value<QColor>());
			sym.setBrush(b);
		}
		setSymbol(new VipSymbol(sym));
		setSymbolVisible(true);
		return true;
	}
	else if (strcmp(name, "baseline") == 0) {
		this->setBaseline(value.toDouble());
		return true;
	}
	else if (strcmp(name, "symbol-condition") == 0) {
		this->setSymbolCondition(value.toString());
		return true;
	}
	else if (strcmp(name, "optimize-large-pen-drawing") == 0) {
		this->setOptimizeLargePenDrawing(value.toBool());
		return true;
	}
	else {
		return VipPlotItem::setItemProperty(name, value, index);
	}
}

inline static bool isPointInside(const QPainterPath& shape, const QRectF& bounding, int shape_coord, const VipPoint& pt)
{
	if (shape.isEmpty())
		return true;
	switch (shape_coord) {
		default:
		case 0:
			return shape.contains((QPointF)pt);
		case 1:
			return pt.x() >= bounding.left() && pt.x() <= bounding.right();
		case 2:
			return pt.y() >= bounding.top() && pt.y() <= bounding.bottom();
		case 3:
			return pt.x() >= bounding.left() && pt.x() <= bounding.right() && pt.y() >= bounding.top() && pt.y() <= bounding.bottom();
	}
}

QList<VipInterval> VipPlotCurve::dataBoundingRect(const VipPointVector& samples,
						  QList<VipPointVector>& out_vectors,
						  QList<bool>& continuous,
						  bool& full_continuous,
						  bool& sub_continuous,
						  const QPainterPath& shape,
						  int shape_coord)
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();
	full_continuous = sub_continuous = false;
	continuous.clear();
	QList<VipPointVector> vectors;

	if (!samples.size()) {
		out_vectors.clear();
		return QList<VipInterval>();
	}

	// find NaN values and split input
	const VipPointVector input = samples;

	QRectF bounding = shape.boundingRect();
	VipPoint topleft, bottomright;
	bool sub_cont = sub_continuous = true;
	bool first = true;
	vip_double start_x = input.first().x();

	int start = -1;
	for (int i = 0; i < input.size(); ++i) {
		if (vipIsNan(input[i].x()) || vipIsNan(input[i].y()) || !std::isfinite(input[i].y()) || !isPointInside(shape, bounding, shape_coord, input[i])) {
			int len = i - start - 1;
			if (len) {
				vectors.append(input.mid(start + 1, len));
				continuous.append(sub_cont);
				sub_continuous = sub_continuous && sub_cont;
				sub_cont = true;
			}

			start = i;
		}
		else {
			// update bounds
			if (first) {
				topleft = VipPoint(input[i]);
				bottomright = VipPoint(input[i]);
				first = false;
			}
			else {
				if (input[i].x() < topleft.x())
					topleft.setX(input[i].x());
				if (input[i].y() < topleft.y())
					topleft.setY(input[i].y());

				if (input[i].x() > bottomright.x())
					bottomright.setX(input[i].x());
				if (input[i].y() > bottomright.y())
					bottomright.setY(input[i].y());
			}
		}

		// check if x coordinate is increasing
		sub_cont = (input[i].x() < start_x) ? false : sub_cont;
		start_x = input[i].x();
	}

	if (start < input.size() - 1) {
		int len = input.size() - start - 1;
		if (len) {
			vectors.append(input.mid(start + 1, len));
			continuous.append(sub_cont);
			sub_continuous = sub_cont && sub_continuous;
		}
	}

	// compute full_continous
	if (sub_continuous) {
		full_continuous = true;
		const QList<VipPointVector>& vecs = vectors;
		for (int i = 1; i < vecs.size(); ++i) {
			if (vecs[i].first().x() < vecs[i - 1].last().x()) {
				full_continuous = false;
				break;
			}
		}
	}

	// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	// vip_debug("dataBoundingRect: %i ms\n", (int)el);
	out_vectors = vectors;

	return QList<VipInterval>() << VipInterval(topleft.x(), bottomright.x()) << VipInterval(topleft.y(), bottomright.y());
}

void VipPlotCurve::addSamples(const VipPoint* pts, int numPoints)
{
	updateSamples([&](VipPointVector& v) {
		for (int i = 0; i < numPoints; ++i)
			v.push_back(pts[i]);
	});
}

void VipPlotCurve::dataBoundingRect(const VipPointVector& samples)
{
	dataLock()->lock();
	d_data->merge.vector.reserve(samples.size());

	QList<VipInterval> bounds = dataBoundingRect(samples, d_data->vectors, d_data->continuous, d_data->full_continuous, d_data->sub_continuous);
	if (bounds.size() == 2) {
		d_data->bounding[0] = bounds[0];
		d_data->bounding[1] = bounds[1];
	}
	else {
		d_data->bounding[0] = VipInterval();
		d_data->bounding[1] = VipInterval();
	}
	dataLock()->unlock();
}

VipArchive& operator<<(VipArchive& arch, const VipPlotCurve* value)
{
	arch.content("legendAttributes", (int)value->legendAttributes());
	arch.content("curveAttributes", (int)value->curveAttributes());
	arch.content("boxStyle", value->boxStyle());
	arch.content("baseline", value->baseline());
	arch.content("curveStyle", (int)value->style());
	if (value->symbol())
		arch.content("symbol", *value->symbol());
	else
		arch.content("symbol", VipSymbol());
	arch.content("symbolVisible", value->symbolVisible());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotCurve* value)
{
	value->setLegendAttributes(VipPlotCurve::LegendAttributes(arch.read("legendAttributes").value<int>()));
	value->setCurveAttributes(VipPlotCurve::CurveAttributes(arch.read("curveAttributes").value<int>()));
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setBaseline(arch.read("baseline").value<double>());
	value->setStyle(VipPlotCurve::CurveStyle(arch.read("curveStyle").value<int>()));
	value->setSymbol(new VipSymbol(arch.read("symbol").value<VipSymbol>()));
	value->setSymbolVisible(arch.read("symbolVisible").toBool());
	return arch;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipPlotCurve*>();
	vipRegisterArchiveStreamOperators<VipPlotCurve*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
