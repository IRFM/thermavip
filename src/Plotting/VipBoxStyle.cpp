/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipBoxStyle.h"
#include "VipPainter.h"
#include "VipPie.h"

#include <QPainter>
#include <QVector2D>
#include <qmath.h>

struct QuadLineIntersection
{

	QPointF startPoint;
	QPointF midPoint;
	QPointF endPoint;

	QuadLineIntersection(const QPointF& pt = QPointF())
	  : startPoint(pt)
	  , midPoint(pt)
	  , endPoint(pt)
	{
	}

	bool isNull() const;
	void reset(const QPointF&);

	void apply(QPainterPath&);

	static QuadLineIntersection fromPolylines(const QPointF* polyline, double radius);
	static QList<QuadLineIntersection> fromPolygon(const QPointF* polygon, double radius);
};

QuadLineIntersection QuadLineIntersection::fromPolylines(const QPointF* p, double radius)
{
	QuadLineIntersection res;

	if (radius == 0) {
		res.startPoint = p[1];
		res.endPoint = p[1];
		res.midPoint = p[1];
		return res;
	}

	QLineF l1(p[0], p[1]);
	QLineF l2(p[2], p[1]);
	double angle = l1.angleTo(l2);

	if (qAbs(angle) == 180 || angle == 0) {
		res.startPoint = p[1];
		res.endPoint = p[1];
		res.midPoint = p[1];
		return res;
	}

	// double length_1 = l1.length()-radius;
	//  double length_2 = l2.length()-radius;
	//
	// if(length_1 >= radius/2)
	// l1.setLength(length_1);
	// else
	// l1.setLength((length_1+radius)/2.0);//l1.p1());
	// if(length_2 > 0)
	// l2.setLength(length_2);
	// else
	// l2.setLength((length_2+radius)/2.0);//l2.p1());

	double length_1 = l1.length();
	double length_2 = l2.length();

	if (length_1 - radius >= length_1 / 2)
		l1.setLength(length_1 - radius);
	else
		l1.setLength(length_1 / 2.0); // l1.p1());

	if (length_2 - radius >= length_2 / 2)
		l2.setLength(length_2 - radius);
	else
		l2.setLength(length_2 / 2.0); // l2.p1());

	res.startPoint = l1.p2();
	res.midPoint = p[1];
	res.endPoint = l2.p2();
	return res;
}

QList<QuadLineIntersection> QuadLineIntersection::fromPolygon(const QPointF* polygon, double radius)
{
	QList<QuadLineIntersection> res;

	QPolygonF p(3);

	// top left
	p[0] = polygon[3];
	p[1] = polygon[0];
	p[2] = polygon[1];
	res.append(fromPolylines(&p[0], radius));

	// top right
	p[0] = polygon[0];
	p[1] = polygon[1];
	p[2] = polygon[2];
	res.append(fromPolylines(&p[0], radius));

	// bottom right
	p[0] = polygon[1];
	p[1] = polygon[2];
	p[2] = polygon[3];
	res.append(fromPolylines(&p[0], radius));

	// bottom left
	p[0] = polygon[2];
	p[1] = polygon[3];
	p[2] = polygon[0];
	res.append(fromPolylines(&p[0], radius));

	return res;
}

bool QuadLineIntersection::isNull() const
{
	return startPoint == endPoint;
}

void QuadLineIntersection::reset(const QPointF& pt)
{
	startPoint = pt;
	midPoint = pt;
	endPoint = pt;
}

void QuadLineIntersection::apply(QPainterPath& p)
{
	p.quadTo(midPoint, endPoint);
}

static QuadLineIntersection qRound(const QuadLineIntersection& inter)
{
	// QuadLineIntersection res;
	//  res.startPoint = qRound(inter.startPoint);
	//  res.endPoint = qRound(inter.endPoint);
	//  return res;
	return inter;
}

VipBoxStyle::VipBoxStyle() {}

VipBoxStyle::VipBoxStyle(const QPen& b_pen, const QBrush& b_brush, double radius)
  : d_data(new PrivateData())
{
	d_data->pen = b_pen;
	d_data->brushGradient.setBrush(b_brush);
	d_data->radius = radius;
	d_data->drawLines = Vip::AllSides;
	d_data->roundedCorners = Vip::AllCorners;
}

void VipBoxStyle::update()
{
	if (!d_data)
		d_data = new PrivateData();
}

void VipBoxStyle::setBorderPen(const QPen& p)
{
	update();
	d_data->pen = p;
}

void VipBoxStyle::setBorderPen(const QColor& c)
{
	update();
	if (d_data->pen.style() == Qt::NoPen)
		d_data->pen.setStyle(Qt::SolidLine);
	d_data->pen.setColor(c);
}

void VipBoxStyle::setBorderPen(const QColor& c, double width)
{
	update();
	if (d_data->pen.style() == Qt::NoPen)
		d_data->pen.setStyle(Qt::SolidLine);
	d_data->pen.setColor(c);
	d_data->pen.setWidth(width);
}

const QPen& VipBoxStyle::borderPen() const noexcept
{
	static const QPen default_pen = QPen(Qt::NoPen);
	if (d_data)
		return d_data->pen;
	else
		return default_pen;
}

QPen& VipBoxStyle::borderPen()
{
	update();
	return d_data->pen;
}

void VipBoxStyle::setBackgroundBrush(const QBrush& b)
{
	update();
	d_data->brushGradient.setBrush(b);
}

const QBrush &VipBoxStyle::backgroundBrush() const noexcept
{
	static const QBrush default_brush = QBrush();
	if (d_data)
		return d_data->brushGradient.brush();
	else
		return default_brush;
}

QBrush& VipBoxStyle::backgroundBrush()
{
	update();
	return d_data->brushGradient.brush();
}

void VipBoxStyle::setColor(const QColor& c)
{
	update();
	if (d_data->pen.style() == Qt::NoPen)
		d_data->pen.setStyle(Qt::SolidLine);
	if (d_data->brushGradient.brush().style() == Qt::NoBrush)
		d_data->brushGradient.brush().setStyle(Qt::SolidPattern);

	d_data->pen.setColor(c);
	d_data->brushGradient.brush().setColor(c);
}

void VipBoxStyle::setAdaptativeGradientBrush(const VipAdaptativeGradient& grad)
{
	update();
	d_data->brushGradient = grad;
}

const VipAdaptativeGradient &VipBoxStyle::adaptativeGradientBrush() const noexcept
{
	static const VipAdaptativeGradient default_grad;
	if (d_data)
		return d_data->brushGradient;
	else
		return default_grad;
}

void VipBoxStyle::unsetBrushGradient()
{
	update();
	d_data->brushGradient.unset();
}

void VipBoxStyle::setAdaptativeGradientPen(const VipAdaptativeGradient& grad)
{
	update();
	d_data->penGradient = grad;
}

const VipAdaptativeGradient& VipBoxStyle::adaptativeGradientPen() const noexcept
{
	static const VipAdaptativeGradient default_grad;
	if (d_data)
		return d_data->penGradient;
	else
		return default_grad;
}

void VipBoxStyle::unsetPenGradient()
{
	update();
	d_data->penGradient.unset();
}

void VipBoxStyle::setBorderRadius(double r)
{
	update();
	d_data->radius = r;
}

double VipBoxStyle::borderRadius() const noexcept
{
	if (d_data)
		return d_data->radius;
	else
		return 0;
}

void VipBoxStyle::setDrawLines(Vip::Sides draw_lines)
{
	update();
	d_data->drawLines = draw_lines;
}

void VipBoxStyle::setDrawLine(Vip::Side draw_line, bool on)
{
	update();
	if (on != testDrawLines(draw_line))

	{
		if (on)
			d_data->drawLines |= draw_line;
		else
			d_data->drawLines &= ~draw_line;
	}
}

bool VipBoxStyle::testDrawLines(Vip::Side draw_line) const
{
	if (d_data)
		return d_data->drawLines & draw_line;
	else
		return true;
}

Vip::Sides VipBoxStyle::drawLines() const noexcept
{
	if (d_data)
		return d_data->drawLines;
	else
		return Vip::AllSides;
}

void VipBoxStyle::setRoundedCorners(Vip::Corners rounded_corners)
{
	update();
	d_data->roundedCorners = rounded_corners;
}

void VipBoxStyle::setRoundedCorner(Vip::Corner rounded_corner, bool on)
{
	update();
	if (on != testRoundedCorner(rounded_corner))

	{
		if (on)
			d_data->roundedCorners |= rounded_corner;
		else
			d_data->roundedCorners &= ~rounded_corner;
	}
}

bool VipBoxStyle::testRoundedCorner(Vip::Corner rounded_corner) const
{
	if (d_data)
		return d_data->roundedCorners & rounded_corner;
	else
		return false;
}

Vip::Corners VipBoxStyle::roundedCorners() const noexcept
{
	if (d_data)
		return d_data->roundedCorners;
	else
		return Vip::Corners();
}

bool VipBoxStyle::isTransparentBrush() const noexcept
{
	if (d_data)
		return d_data->brushGradient.isTransparent();
	else
		return true;
}

bool VipBoxStyle::isTransparentPen() const noexcept
{
	if (d_data)
		return (d_data->pen.style() == Qt::NoPen || d_data->pen.color().alpha() == 0);
	else
		return true;
}

bool VipBoxStyle::isTransparent() const noexcept
{
	return !d_data || (isTransparentPen() && isTransparentBrush());
}

const QPainterPath & VipBoxStyle::background() const noexcept
{
	static const QPainterPath default_path;
	if (d_data)
		return d_data->paths.first;
	else
		return default_path;
}

const QPainterPath& VipBoxStyle::border() const noexcept
{
	static const QPainterPath default_path;
	if (d_data)
		return d_data->paths.second;
	else
		return default_path;
}

const PainterPaths& VipBoxStyle::paths() const noexcept
{
	static const PainterPaths default_path;
	if (d_data)
		return d_data->paths;
	else
		return default_path;
}

QRectF VipBoxStyle::boundingRect() const
{
	PainterPaths p = paths();
	return p.first.boundingRect() | p.second.boundingRect();
}

void VipBoxStyle::computePath(const QPainterPath& path)
{
	update();
	d_data->polygon.clear();
	d_data->paths.first = path;
	d_data->paths.second = path;
	d_data->pie = VipPie();
	d_data->rect = path.boundingRect();
}

void VipBoxStyle::computePath(const PainterPaths& paths)
{
	update();
	d_data->polygon.clear();
	d_data->paths = paths;
	d_data->pie = VipPie();
	d_data->rect = paths.first.boundingRect() | paths.second.boundingRect();
}

void VipBoxStyle::computeRect(const QRectF& rect)
{
	QPolygonF p(4);
	p[0] = rect.topLeft();
	p[1] = rect.topRight();
	p[2] = rect.bottomRight();
	p[3] = rect.bottomLeft();
	computeQuadrilateral(p);
}

void VipBoxStyle::computeQuadrilateral(const QPolygonF& polygon)
{
	update();
	d_data->pie = VipPie();
	d_data->rect = QRectF();
	d_data->polygon.clear();

	QPolygonF p;

	if ((polygon.size() == 5 && polygon[0] == polygon[4]) || (polygon.size() == 4)) {
		p = polygon.mid(0, 4);
	}
	else {
		d_data->paths = PainterPaths();
		return;
	}

	d_data->rect = p.boundingRect();

	// case 4 points
	if (d_data->roundedCorners == 0 || d_data->radius == 0) {
		QPainterPath background, border;

		if (p.size() == 4 && d_data->drawLines != Vip::AllSides) {
			if (d_data->drawLines & Vip::Top) {
				border.moveTo(p[0]);
				border.lineTo(p[1]);
			}
			else
				border.moveTo(p[1]);

			if (d_data->drawLines & Vip::Right) {
				border.lineTo(p[2]);
			}
			else
				border.moveTo(p[2]);

			if (d_data->drawLines & Vip::Bottom) {
				border.lineTo(p[3]);
			}
			else
				border.moveTo(p[3]);

			if (d_data->drawLines & Vip::Left) {
				border.lineTo(p[0]);
			}

			background.addPolygon(p);
		}
		else {
			if (polygon.size() == 5)
				background.addPolygon(p);
			else {
				if (p.size() == 4)
					p << p.first();
				background.addPolygon(p);
			}
			border = background;
		}

		d_data->paths = QPair<QPainterPath, QPainterPath>(background, border);
	}
	else {

		// for(int i=0; i < p.size(); ++i)
		//  p[i] = qRound(p[i]);

		QList<QuadLineIntersection> lst = QuadLineIntersection::fromPolygon(&p[0], d_data->radius);

		if (!(d_data->roundedCorners & Vip::TopLeft) || !(d_data->drawLines & Vip::Left) || !(d_data->drawLines & Vip::Top)) {
			lst[0].reset(p[0]);
		}

		if (!(d_data->roundedCorners & Vip::TopRight) || !(d_data->drawLines & Vip::Right) || !(d_data->drawLines & Vip::Top)) {
			lst[1].reset(p[1]);
		}

		if (!(d_data->roundedCorners & Vip::BottomRight) || !(d_data->drawLines & Vip::Bottom) || !(d_data->drawLines & Vip::Right)) {
			lst[2].reset(p[2]);
		}

		if (!(d_data->roundedCorners & Vip::BottomLeft) || !(d_data->drawLines & Vip::Bottom) || !(d_data->drawLines & Vip::Left)) {
			lst[3].reset(p[3]);
		}

		QPainterPath background, border;

		background.moveTo(lst[0].startPoint);
		if (!lst[0].isNull())
			lst[0].apply(background);

		background.lineTo(lst[1].startPoint);
		if (!lst[1].isNull())
			lst[1].apply(background);

		// if (d_data->drawLines == Vip::AllSides)
		//  border = background;
		const bool all_sides = (d_data->drawLines == Vip::AllSides);

		if (!all_sides && d_data->drawLines & Vip::Top) {
			border.moveTo(lst[0].startPoint);
			if (!lst[0].isNull())
				lst[0].apply(border);
			border.lineTo(lst[1].startPoint);
		}

		background.lineTo(lst[2].startPoint);
		if (!lst[2].isNull())
			lst[2].apply(background);

		if (!all_sides && d_data->drawLines & Vip::Right) {
			border.moveTo(lst[1].startPoint);
			if (!lst[1].isNull())
				lst[1].apply(border);
			border.lineTo(lst[2].startPoint);
		}

		background.lineTo(lst[3].startPoint);
		if (!lst[3].isNull())
			lst[3].apply(background);

		if (!all_sides && d_data->drawLines & Vip::Bottom) {
			border.moveTo(lst[2].startPoint);
			if (!lst[2].isNull())
				lst[2].apply(border);
			border.lineTo(lst[3].startPoint);
		}

		background.lineTo(lst[0].startPoint);

		if (!all_sides && d_data->drawLines & Vip::Left) {
			border.moveTo(lst[3].startPoint);
			if (!lst[3].isNull())
				lst[3].apply(border);
			border.lineTo(lst[0].startPoint);
		}

		if (all_sides)
			border = background;

		d_data->paths = QPair<QPainterPath, QPainterPath>(background, border);
	}
}

void VipBoxStyle::computePolyline(const QPolygonF& polygon)
{
	update();
	d_data->pie = VipPie();
	d_data->rect = polygon.boundingRect();
	d_data->polygon.clear();

	if (polygon.size() < 3) {
		QPainterPath path;
		path.addPolygon(polygon);
		d_data->paths = QPair<QPainterPath, QPainterPath>(path, path);
		return;
	}

	// generic case, N points

	if (d_data->radius == 0) {
		QPainterPath path;
		path.addPolygon(polygon);
		d_data->paths = QPair<QPainterPath, QPainterPath>(path, path);
		d_data->polygon = polygon;
		return;
	}

	// remove consecutive duplicate points

	QPolygonF no_duplicate;
	no_duplicate.reserve(polygon.size());
	no_duplicate << polygon[0];

	for (int i = 1; i < polygon.size(); ++i) {
		if (polygon[i] != polygon[i - 1]) {
			no_duplicate << polygon[i];
		}
	}

	if (no_duplicate.size() < 3) {
		QPainterPath path;
		path.addPolygon(no_duplicate);
		d_data->paths = QPair<QPainterPath, QPainterPath>(path, path);
		return;
	}

	bool is_closed = (no_duplicate.first() == no_duplicate.last());

	QPainterPath path;

	// compute rounded intersections
	QVector<QuadLineIntersection> quads;
	if (is_closed) {
		quads.resize(no_duplicate.size() - 1);

		QPointF last[3] = { no_duplicate[no_duplicate.size() - 2], no_duplicate.last(), no_duplicate[1] };
		quads.last() = QuadLineIntersection::fromPolylines(last, d_data->radius);

		path.moveTo(quads.last().endPoint);
	}
	else {
		quads.resize(no_duplicate.size() - 2);
		path.moveTo(no_duplicate.first());
	}

	for (int i = 1; i < no_duplicate.size() - 1; ++i) {
		quads[i - 1] = QuadLineIntersection::fromPolylines(&no_duplicate[i - 1], d_data->radius);
	}

	// compute the path

	for (int i = 0; i < quads.size(); ++i) {
		if (!vipFuzzyCompare(path.currentPosition(), quads[i].startPoint))
			path.lineTo(quads[i].startPoint);

		quads[i].apply(path);
	}

	// end the path
	if (!is_closed)
		path.lineTo(no_duplicate.last());

	d_data->paths = QPair<QPainterPath, QPainterPath>(path, path);
}

void VipBoxStyle::computePie(const QPointF& c, const VipPie& pie, double spacing)
{
	update();
	double angle_start = pie.startAngle();
	double angle_end = pie.endAngle();
	double min_distance_to_center = pie.minRadius();
	double max_distance_to_center = pie.maxRadius();
	double offset_to_center = pie.offsetToCenter();

	d_data->pie = pie;
	d_data->center = c;
	d_data->rect = QRectF();
	;
	d_data->polygon.clear();

	// case line
	if (angle_start == angle_end) {
		QLineF line(c, QPointF(c.x(), c.y() - max_distance_to_center - offset_to_center));
		line.setAngle(angle_start);
		line.setP1(line.pointAt((min_distance_to_center + offset_to_center) / line.length()));

		QPainterPath border;
		border.moveTo(line.p1());
		border.lineTo(line.p2());

		d_data->paths = QPair<QPainterPath, QPainterPath>(QPainterPath(), border);
		return;
	}
	// case circle
	else if ((angle_end - angle_start) == 360) {
		QRectF outer_circle(c - QPointF(max_distance_to_center, max_distance_to_center), c + QPointF(max_distance_to_center, max_distance_to_center));
		QRectF inner_circle(c - QPointF(min_distance_to_center, min_distance_to_center), c + QPointF(min_distance_to_center, min_distance_to_center));

		QPainterPath background;

		background.addEllipse(outer_circle);
		if (min_distance_to_center) {
			QPainterPath inner;
			inner.addEllipse(inner_circle);
			background = background.subtracted(inner);
		}

		d_data->paths = QPair<QPainterPath, QPainterPath>(background, background);
		return;
	}

	QPointF center(c);
	if (offset_to_center != 0) {
		// vertical line
		QLineF line(center, QPointF(center.x(), center.y() - offset_to_center));
		line.setAngle((angle_start + angle_end) / 2.0);
		center = line.p2();
	}

	while (angle_end < angle_start)
		angle_end += 2 * M_PI;

	const double radius = max_distance_to_center;
	const double min_radius = min_distance_to_center;
	const double radius_angle = qAsin(borderRadius() / radius) * Vip::ToDegree;
	const QRectF bounding(center - QPointF(radius, radius), QSizeF(radius * 2, radius * 2));
	const QRectF min_bounding(center - QPointF(min_radius, min_radius), QSizeF(min_radius * 2, min_radius * 2));

	// vertical line
	QLineF line(center, QPointF(center.x(), center.y() - max_distance_to_center));
	QLineF line_start(line);
	QLineF line_end(line);
	line_start.setAngle(angle_start);
	line_end.setAngle(angle_end);

	QLineF line_start_2(line);
	QLineF line_end_2(line);
	line_start_2.setAngle(angle_start + radius_angle);
	line_end_2.setAngle(angle_end - radius_angle);

	double percent = min_distance_to_center / max_distance_to_center;
	line_start.setP1(line_start.pointAt(percent));
	line_start_2.setP1(line_start_2.pointAt(percent));
	line_end.setP1(line_end.pointAt(percent));
	line_end_2.setP1(line_end_2.pointAt(percent));

	QPointF polyline[3];
	QList<QuadLineIntersection> lst;

	polyline[0] = center;
	polyline[1] = line_start.p2();
	polyline[2] = line_start_2.p2();
	lst << QuadLineIntersection::fromPolylines(polyline, borderRadius());

	polyline[0] = line_end_2.p2();
	polyline[1] = line_end.p2();
	polyline[2] = center;
	lst << QuadLineIntersection::fromPolylines(polyline, borderRadius());

	polyline[0] = line_end.p2();
	polyline[1] = line_end.p1();
	polyline[2] = line_end_2.p1();
	lst << QuadLineIntersection::fromPolylines(polyline, borderRadius());

	polyline[0] = line_start_2.p1();
	polyline[1] = line_start.p1();
	polyline[2] = line_start.p2();
	lst << QuadLineIntersection::fromPolylines(polyline, borderRadius());

	// rounding
	for (int i = 0; i < lst.size(); ++i)
		lst[i] = qRound(lst[i]);

	QPolygonF p;
	p << line_start.p2() << line_end.p2() << line_end.p1() << line_start.p1();

	if (!(d_data->roundedCorners & Vip::TopLeft) || !(d_data->drawLines & Vip::Left) || !(d_data->drawLines & Vip::Top)) {
		lst[0].reset(p[0]);
	}

	if (!(d_data->roundedCorners & Vip::TopRight) || !(d_data->drawLines & Vip::Right) || !(d_data->drawLines & Vip::Top)) {
		lst[1].reset(p[1]);
	}

	if (!(d_data->roundedCorners & Vip::BottomRight) || !(d_data->drawLines & Vip::Bottom) || !(d_data->drawLines & Vip::Right)) {
		lst[2].reset(p[2]);
	}

	if (!(d_data->roundedCorners & Vip::BottomLeft) || !(d_data->drawLines & Vip::Bottom) || !(d_data->drawLines & Vip::Left)) {
		lst[3].reset(p[3]);
	}

	// should we vipMerge the bottom points?
	// bool merge_bottom = (d_data->roundedCorners & BottomLeft) && (d_data->roundedCorners & BottomRight) &&
	//  (d_data->drawLines & Right) && (d_data->drawLines & Left) && (d_data->drawLines & Bottom) && d_data->radius >= min_distance_to_center;

	QPainterPath background, border;

	background.moveTo(lst[0].startPoint);
	if (!lst[0].isNull())
		lst[0].apply(background);

	double _start_angle = QLineF(center, lst[0].endPoint).angle();
	double _end_angle = QLineF(center, lst[1].startPoint).angle();
	// draw the arc to the right side
	while (_end_angle <= _start_angle)
		_end_angle += 360;
	double sweep_length = _end_angle - _start_angle;

	background.arcTo(bounding, _start_angle, sweep_length);
	if (!lst[1].isNull())
		lst[1].apply(background);

	if (d_data->drawLines & Vip::Top) {
		border.moveTo(lst[0].startPoint);
		if (!lst[0].isNull())
			lst[0].apply(border);
		border.arcTo(bounding, _start_angle, sweep_length);
	}

	background.lineTo(lst[2].startPoint);
	if (!lst[2].isNull())
		lst[2].apply(background);

	if (d_data->drawLines & Vip::Right) {
		if (!(d_data->drawLines & Vip::Top))
			border.moveTo(lst[1].startPoint);
		if (!lst[1].isNull())
			lst[1].apply(border);
		border.lineTo(lst[2].startPoint);
	}

	_start_angle = QLineF(center, lst[2].endPoint).angle();
	_end_angle = QLineF(center, lst[3].startPoint).angle();
	while (_end_angle >= _start_angle)
		_end_angle -= 360;
	sweep_length = _end_angle - _start_angle;

	background.arcTo(min_bounding, _start_angle, sweep_length);
	if (!lst[3].isNull())
		lst[3].apply(background);

	if (d_data->drawLines & Vip::Bottom) {
		if (!(d_data->drawLines & Vip::Right))
			border.moveTo(lst[2].startPoint);
		if (!lst[2].isNull())
			lst[2].apply(border);
		border.arcTo(min_bounding, _start_angle, sweep_length);
		// border.lineTo(lst[3].startPoint);
	}

	background.lineTo(lst[0].startPoint);

	if (d_data->drawLines & Vip::Left) {
		if (!(d_data->drawLines & Vip::Bottom))
			border.moveTo(lst[3].startPoint);
		if (!lst[3].isNull())
			lst[3].apply(border);
		border.lineTo(lst[0].startPoint);
	}

	// if(d_data->drawLines == AllLines)
	//  border.closeSubpath();

	if (spacing) {
		// remove spacing from left and right borders
		QPainterPathStroker stroker;
		stroker.setWidth(spacing * 2);

		QPainterPath _p;
		_p.addPolygon(QPolygonF() << line_start.p1() << line_start.p2());
		_p.addPolygon(QPolygonF() << line_end.p1() << line_end.p2());

		QPainterPath str = stroker.createStroke(_p);
		background = background.subtracted(str);
		border = border.subtracted(str);
	}

	d_data->paths = QPair<QPainterPath, QPainterPath>(background, border);
}

QBrush VipBoxStyle::createBackgroundBrush() const
{
	if (!d_data)
		return QBrush();

	if (!d_data->pie.isEmpty())
		return d_data->brushGradient.createBrush(d_data->center, d_data->pie);
	else if (!d_data->rect.isEmpty())
		return d_data->brushGradient.createBrush(d_data->rect);

	return d_data->brushGradient.brush();
}

QPen VipBoxStyle::createBorderPen(const QPen& _pen) const
{
	if (!d_data)
		return _pen;

	QPen pen = _pen;

	if (!d_data->pie.isEmpty() && d_data->penGradient.type() != VipAdaptativeGradient::NoGradient)
		pen.setBrush(d_data->penGradient.createBrush(pen.brush(), d_data->center, d_data->pie));
	else if (!d_data->rect.isEmpty() && d_data->penGradient.type() != VipAdaptativeGradient::NoGradient)
		pen.setBrush(d_data->brushGradient.createBrush(pen.brush(), d_data->rect));

	return pen;
}

bool VipBoxStyle::hasBrushGradient() const noexcept
{
	return d_data->brushGradient.type() != VipAdaptativeGradient::NoGradient;
}
bool VipBoxStyle::hasPenGradient() const noexcept
{
	return d_data->penGradient.type() != VipAdaptativeGradient::NoGradient;
}

void VipBoxStyle::drawBackground(QPainter* painter) const
{

	if (!d_data || isTransparentBrush())
		return;
	drawBackground(painter, createBackgroundBrush());
}

void VipBoxStyle::drawBackground(QPainter* painter, const QBrush& brush) const
{
	if (!d_data || isTransparentBrush())
		return;

	painter->setBrush(brush);
	painter->setPen(QPen(Qt::NoPen));

	if (d_data->polygon.size()) {
		if (d_data->polygon.first() == d_data->polygon.last())
			VipPainter::drawPolygon(painter, d_data->polygon);
		else
			VipPainter::drawPolyline(painter, d_data->polygon);
	}
	else
		painter->drawPath(background());
}

void VipBoxStyle::drawBorder(QPainter* painter) const
{
	if (!d_data || d_data->pen.style() == Qt::NoPen)
		return;
	drawBorder(painter, createBorderPen(d_data->pen));
}

void VipBoxStyle::drawBorder(QPainter* painter, const QPen& pen) const
{
	if (!d_data || d_data->pen.style() == Qt::NoPen)
		return;

	// There is currently a bug in Qt that provokes an infinit loop in QDashStroker::processCurrentSubpath() for some
	// weird polylines. We try to detect these cases and avoid them
	bool has_wrong_pos = qAbs(d_data->rect.left()) > 100000 || qAbs(d_data->rect.top()) > 100000;
	bool has_wrong_size = qAbs(d_data->rect.width()) > 100000 || qAbs(d_data->rect.height()) > 100000;
	if ((has_wrong_pos && has_wrong_size) && d_data->polygon.isEmpty())
		return;

	painter->setPen(pen);
	painter->setBrush(QBrush());

	if (d_data->polygon.size()) {
		if (d_data->polygon.first() == d_data->polygon.last())
			VipPainter::drawPolygon(painter, d_data->polygon);
		else
			VipPainter::drawPolyline(painter, d_data->polygon);
	}
	else
		painter->drawPath(border());
}

void VipBoxStyle::draw(QPainter* painter) const
{
	if (!d_data || isTransparent())
		return;

	draw(painter, createBackgroundBrush(), createBorderPen(d_data->pen));
}

void VipBoxStyle::draw(QPainter* painter, const QBrush& brush) const
{
	if (!d_data || isTransparent())
		return;

	draw(painter, brush, createBorderPen(d_data->pen));
}

void VipBoxStyle::draw(QPainter* painter, const QBrush& brush, const QPen& pen) const
{
	if (!d_data || isTransparent())
		return;

	if (d_data->drawLines == Vip::AllSides) {
		// There is currently a bug in Qt that provokes an infinit loop in QDashStroker::processCurrentSubpath() for some
		// weird polylines. We try to detect these cases and avoid them
		bool has_wrong_pos = qAbs(d_data->rect.left()) > 100000 || qAbs(d_data->rect.top()) > 100000;
		bool has_wrong_size = qAbs(d_data->rect.width()) > 100000 || qAbs(d_data->rect.height()) > 100000;
		if ((has_wrong_pos && has_wrong_size) && d_data->polygon.isEmpty())
			return;

		// draw border and background in one pass
		painter->setBrush(brush);
		painter->setPen(pen);

		if (d_data->polygon.size()) {
			if (d_data->polygon.first() == d_data->polygon.last())
				VipPainter::drawPolygon(painter, d_data->polygon);
			else
				VipPainter::drawPolyline(painter, d_data->polygon);
		}
		else
			painter->drawPath(background());
	}
	else {
		// painter->save();

		drawBackground(painter, brush);
		drawBorder(painter, pen);

		// painter->restore();
	}
}

bool VipBoxStyle::operator==(const VipBoxStyle& other) const noexcept
{
	return other.d_data->brushGradient == d_data->brushGradient && other.d_data->drawLines == d_data->drawLines && other.d_data->pen == d_data->pen &&
	       other.d_data->penGradient == d_data->penGradient && other.d_data->brushGradient == d_data->brushGradient && other.d_data->radius == d_data->radius &&
	       other.d_data->roundedCorners == d_data->roundedCorners;
}

bool VipBoxStyle::operator!=(const VipBoxStyle& other) const noexcept
{
	return !((*this) == other);
}

#include <QDataStream>

QDataStream& operator<<(QDataStream& stream, const VipBoxStyle& style)
{
	return stream << style.borderPen() << style.backgroundBrush() << style.adaptativeGradientBrush() << style.adaptativeGradientPen() << style.borderRadius() << (int)style.drawLines()
		      << (int)style.roundedCorners();
}

QDataStream& operator>>(QDataStream& stream, VipBoxStyle& style)
{
	QPen pen;
	QBrush brush;
	VipAdaptativeGradient apen, abrush;
	double borderRadius;
	int drawLines, roundedCorners;

	stream >> pen >> brush >> abrush >> apen >> borderRadius >> drawLines >> roundedCorners;

	style.setBorderPen(pen);
	style.setBackgroundBrush(brush);
	style.setAdaptativeGradientBrush(abrush);
	style.setAdaptativeGradientPen(apen);
	style.setBorderRadius(borderRadius);
	style.setDrawLines(Vip::Sides(drawLines));
	style.setRoundedCorners(Vip::Corners(roundedCorners));

	return stream;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipBoxStyle>();
	qRegisterMetaTypeStreamOperators<VipBoxStyle>("VipBoxStyle");
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();
