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

#include <QPainter>
#include <cmath>

#include "VipPainter.h"
#include "VipQuiver.h"

VipQuiver::VipQuiver(const QPointF& origin, const QVector2D vector)
  : m_origin(origin)
  , m_vector(vector)
{
}

VipQuiver::VipQuiver(const QPointF& p1, const QPointF& p2)
  : m_origin(p1)
  , m_vector(p2 - p1)
{
}

VipQuiver::VipQuiver(const QLineF& line)
  : m_origin(line.p1())
  , m_vector(line.p2() - line.p1())
{
}

const QPointF& VipQuiver::origin() const
{
	return m_origin;
}

const QVector2D& VipQuiver::vector() const
{
	return m_vector;
}

QLineF VipQuiver::line() const
{
	return QLineF(m_origin, m_origin + m_vector.toPointF());
}

QPointF VipQuiver::p1() const
{
	return m_origin;
}

QPointF VipQuiver::p2() const
{
	return m_origin + m_vector.toPointF();
}

double VipQuiver::length() const
{
	return m_vector.length();
}

void VipQuiver::setOrigin(const QPointF& o)
{
	m_origin = o;
}

void VipQuiver::setVector(const QVector2D& v)
{
	m_vector = v;
}

void VipQuiver::setLine(const QLineF& l)
{
	m_origin = l.p1();
	m_vector = QVector2D(l.p2() - l.p1());
}

QLineF VipQuiver::draw(QPainter* painter, const QPen* extremityPen, const QBrush* extremityBrush, const double* angles, const double* lengths, int style)
{
	const VipQuiver& q = *this;
	const QPointF p0 = q.origin();
	const QPointF p1 = q.line().p2();
	const double sq = q.vector().lengthSquared();
	const double ssq = std::sqrt(sq);

	QPointF start = p0, end = p1;

	// draw the arrows or squares

	if (style & VipQuiverPath::EndArrow) {
		const double l = (lengths[1] >= 0) ? lengths[1] : -lengths[1] * ssq / 100;
		const double deg = (angles[1] * M_PI / 180);
		const double ang = (sq > 0) ? std::atan2(-q.vector().y(), -q.vector().x()) : 0.0f;
		const double cl = std::cos(ang - deg), sl = std::sin(ang - deg);
		const double cr = std::cos(ang + deg), sr = std::sin(ang + deg);
		const double xl = p1.x() + (l * cl), yl = p1.y() + (l * sl), xr = p1.x() + (l * cr), yr = p1.y() + (l * sr), xc = p1.x() + ((l + 1) * (cl + cr)) / 2,
			     yc = p1.y() + ((l + 1) * (sl + sr)) / 2;

		QPointF pc(xc, yc);
		QPointF pr(xr, yr);
		QPointF pl(xl, yl);

		// since this function might be used to compute start and end border vipDistance with a dummy painter,
		// check for the painter to be active
		if (painter->paintEngine()) {
			painter->setPen(extremityPen[VipQuiverPath::End]);
			painter->setBrush(extremityBrush[VipQuiverPath::End]);
			VipPainter::drawPolygon(painter, QPolygonF() << p1 << pl << pr);
		}

		end = (pr + pl) / 2.0;
	}

	if (style & VipQuiverPath::StartArrow) {
		const double l = (lengths[0] >= 0) ? lengths[0] : -lengths[0] * ssq / 100;
		const double deg = (angles[0] * M_PI / 180);

		const QPointF _p1 = p0;
		const double ang = (sq > 0) ? std::atan2(q.vector().y(), q.vector().x()) : 0.0f;
		const double cl = std::cos(ang - deg), sl = std::sin(ang - deg);
		const double cr = std::cos(ang + deg), sr = std::sin(ang + deg);
		const double xl = _p1.x() + (l * cl), yl = _p1.y() + (l * sl), xr = _p1.x() + (l * cr), yr = _p1.y() + (l * sr), xc = _p1.x() + ((l + 1) * (cl + cr)) / 2,
			     yc = _p1.y() + ((l + 1) * (sl + sr)) / 2;

		QPointF pc(xc, yc);
		QPointF pr(xr, yr);
		QPointF pl(xl, yl);

		if (painter->paintEngine()) {
			painter->setPen(extremityPen[VipQuiverPath::Start]);
			painter->setBrush(extremityBrush[VipQuiverPath::Start]);
			VipPainter::drawPolygon(painter, QPolygonF() << _p1 << pl << pr);
		}

		start = (pr + pl) / 2.0;
	}

	if ((style & VipQuiverPath::StartSquare) || (style & VipQuiverPath::EndSquare)) {
		if (style & VipQuiverPath::StartSquare) {
			const double l = (lengths[0] >= 0) ? lengths[0] : -lengths[0] * ssq / 100;
			QLineF seg(p0, line().pointAt(l / std::sqrt(2) / ssq));
			double angle = seg.angle();
			QPointF _p0 = p0;
			QPointF _p2 = line().pointAt(l / ssq);
			seg.setAngle(angle + 45);
			QPointF _p1 = seg.p2();
			seg.setAngle(angle - 45);
			QPointF _p3 = seg.p2();

			QPolygonF square = QPolygonF() << _p0 << _p1 << _p2 << _p3;

			if (painter->paintEngine()) {
				painter->setPen(extremityPen[VipQuiverPath::Start]);
				painter->setBrush(extremityBrush[VipQuiverPath::Start]);
				VipPainter::drawPolygon(painter, square);
			}

			start = _p2;
		}
		if (style & VipQuiverPath::EndSquare) {
			const double l = (lengths[1] >= 0) ? lengths[1] : -lengths[1] * ssq / 100;
			QLineF seg(p0, line().pointAt(l / std::sqrt(2) / ssq));
			double angle = seg.angle();
			QPointF _p0 = p0;
			QPointF _p2 = line().pointAt(l / ssq);
			seg.setAngle(angle + 45);
			QPointF _p1 = seg.p2();
			seg.setAngle(angle - 45);
			QPointF _p3 = seg.p2();

			QPolygonF square = QPolygonF() << _p0 << _p1 << _p2 << _p3;
			square = square.translated(line().pointAt((ssq - l) / ssq) - p0);

			if (painter->paintEngine()) {
				painter->setPen(extremityPen[VipQuiverPath::End]);
				painter->setBrush(extremityBrush[VipQuiverPath::End]);
				VipPainter::drawPolygon(painter, square);
			}

			end = _p0;
		}
	}

	if ((style & VipQuiverPath::StartCircle) || (style & VipQuiverPath::EndCircle)) {
		if (style & VipQuiverPath::StartCircle) {
			const double l = (lengths[0] >= 0) ? lengths[0] : -lengths[0] * ssq / 100;
			QPointF offset(l * 0.5, l * 0.5);
			QPointF center = p0; // line().pointAt(offset.x()/ssq);

			if (painter->paintEngine()) {
				painter->setPen(extremityPen[VipQuiverPath::Start]);
				painter->setBrush(extremityBrush[VipQuiverPath::Start]);
				VipPainter::drawEllipse(painter, QRectF(center - offset, center + offset));
			}

			if (p0 != p1)
				start = QLineF(p0, p1).pointAt(l * 0.5 / ssq);
		}
		if (style & VipQuiverPath::EndCircle) {
			const double l = (lengths[1] >= 0) ? lengths[1] : -lengths[1] * ssq / 100;
			QPointF offset(l * 0.5, l * 0.5);
			QPointF center = p1; // line().pointAt((ssq-offset.x())/ssq);

			if (painter->paintEngine()) {
				painter->setPen(extremityPen[VipQuiverPath::End]);
				painter->setBrush(extremityBrush[VipQuiverPath::End]);
				VipPainter::drawEllipse(painter, QRectF(center - offset, center + offset));
			}

			if (p0 != p1)
				end = QLineF(p0, p1).pointAt((ssq - l * 0.5) / ssq);
		}
	}

	return QLineF(start, end);
}

VipQuiverPath::VipQuiverPath()
  : d_style(EndArrow)
  , d_visible(true)
{
	d_angles[0] = d_angles[1] = 30;
	d_lengths[0] = d_lengths[1] = 10;
}

void VipQuiverPath::setPen(const QPen& p)
{
	d_pathPen = p;
}

const QPen& VipQuiverPath::pen() const
{
	return d_pathPen;
}

void VipQuiverPath::setExtremityPen(Extremity ext, const QPen& p)
{
	d_extremityPen[ext] = p;
}

const QPen& VipQuiverPath::extremetyPen(Extremity ext) const
{
	return d_extremityPen[ext];
}

void VipQuiverPath::setExtremityBrush(Extremity ext, const QBrush& brush)
{
	d_extremityBrush[ext] = brush;
}

const QBrush& VipQuiverPath::extremetyBrush(Extremity ext) const
{
	return d_extremityBrush[ext];
}

void VipQuiverPath::setColor(const QColor& color)
{
	d_pathPen.setColor(color);
	d_extremityPen[Start].setColor(color);
	d_extremityPen[End].setColor(color);
	d_extremityBrush[Start].setColor(color);
	d_extremityBrush[End].setColor(color);
	d_extremityBrush[Start].setStyle(Qt::SolidPattern);
	d_extremityBrush[End].setStyle(Qt::SolidPattern);
}

void VipQuiverPath::setStyle(const QuiverStyles& st)
{
	d_style = st;
}

VipQuiverPath::QuiverStyles VipQuiverPath::style() const
{
	return d_style;
}

void VipQuiverPath::setAngle(Extremity ext, double value)
{
	d_angles[ext] = value;
}

double VipQuiverPath::angle(Extremity ext) const
{
	return d_angles[ext];
}

void VipQuiverPath::setLength(Extremity ext, double len)
{
	d_lengths[ext] = len;
}

double VipQuiverPath::length(Extremity ext) const
{
	return d_lengths[ext];
}

void VipQuiverPath::setVisible(bool vis)
{
	d_visible = vis;
}

bool VipQuiverPath::isVisible() const
{
	return d_visible;
}

QPair<double, double> VipQuiverPath::draw(QPainter* painter, const QLineF& line) const
{
	if (!d_visible)
		return QPair<double, double>(0, 0);

	VipQuiver q(line);
	QLineF extremities = q.draw(painter, d_extremityPen, d_extremityBrush, d_angles, d_lengths, d_style);

	if (painter->paintEngine()) {
		painter->setPen(d_pathPen);
		VipPainter::drawLine(painter, extremities);
	}
	QPair<double, double> res(0, 0);
	if (style() & StartCircle) {
		res.first = QLineF(extremities.p1(), line.p1()).length();
	}
	if (style() & EndCircle) {
		res.second = QLineF(extremities.p2(), line.p2()).length();
	}

	return res;
}

QPair<double, double> VipQuiverPath::draw(QPainter* painter, const QPointF* polyline, int size) const
{
	if (!d_visible)
		return QPair<double, double>(0, 0);

	if (size == 1) {
		return draw(painter, QLineF(polyline[0], polyline[0]));
	}
	else if (size == 2) {
		return draw(painter, QLineF(polyline[0], polyline[1]));
	}
	else if (size > 2) {
		// recompute the polyline by removing duplicate points, compute its total length
		QPolygonF poly;
		double len = 0;
		for (int i = 0; i < size - 1; ++i) {
			if (polyline[i] != polyline[i + 1]) {
				poly << polyline[i];
				len += QLineF(polyline[i], polyline[i + 1]).length();
			}
		}

		poly << polyline[size - 1];

		if (poly.size() <= 2)
			return draw(painter, poly.data(), poly.size());

		QPair<double, double> res(0, 0);

		// draw the first arrow
		QLineF l1(poly[0], poly[1]);
		l1.setLength(len);
		VipQuiver q1(l1);
		l1 = q1.draw(painter, d_extremityPen, d_extremityBrush, d_angles, d_lengths, d_style & 7);
		if (style() & StartCircle) {
			res.first = QLineF(l1.p1(), poly[0]).length();
		}

		poly[0] = l1.p1();

		// draw the second arrow
		QLineF l2(poly[poly.size() - 1], poly[poly.size() - 2]);
		l2.setLength(len);
		QPointF tmp = l2.p1();
		l2.setP1(l2.p2());
		l2.setP2(tmp);
		VipQuiver q2(l2);
		l2 = q2.draw(painter, d_extremityPen, d_extremityBrush, d_angles, d_lengths, d_style & 56);
		if (style() & EndCircle) {
			res.second = QLineF(l2.p2(), poly.back()).length();
		}

		poly.back() = l2.p2();

		// draw the polyline
		if (painter->paintEngine()) {
			painter->setPen(d_pathPen);
			painter->setBrush(QBrush());
			VipPainter::drawPolyline(painter, poly);
		}

		return res;
	}

	return QPair<double, double>(0, 0);
}

QPair<double, double> VipQuiverPath::draw(QPainter* painter, const QPolygonF& polyline) const
{
	return draw(painter, polyline.data(), polyline.size());
}

QDataStream& operator<<(QDataStream& str, const VipQuiverPath& path)
{
	return str << path.pen() << path.extremetyPen(VipQuiverPath::Start) << path.extremetyPen(VipQuiverPath::End) << path.extremetyBrush(VipQuiverPath::Start)
		   << path.extremetyBrush(VipQuiverPath::End) << (int)path.style() << path.angle(VipQuiverPath::Start) << path.angle(VipQuiverPath::End) << path.length(VipQuiverPath::Start)
		   << path.length(VipQuiverPath::End) << path.isVisible();
}

QDataStream& operator>>(QDataStream& str, VipQuiverPath& path)
{
	QPen pen, startp, endp;
	QBrush startb, endb;
	int style = 0;
	double starta = 0, enda = 0;
	double startl = 0, endl = 0;
	bool vis = true;

	str >> pen >> startp >> endp >> startb >> endb >> style >> starta >> enda >> startl >> endl >> vis;
	if (str.status() != QDataStream::Ok)
		return str;

	path.setPen(pen);
	path.setExtremityPen(VipQuiverPath::Start, startp);
	path.setExtremityPen(VipQuiverPath::End, endp);
	path.setExtremityBrush(VipQuiverPath::Start, startb);
	path.setExtremityBrush(VipQuiverPath::End, endb);
	path.setStyle((VipQuiverPath::QuiverStyles)style);
	path.setAngle(VipQuiverPath::Start, starta);
	path.setAngle(VipQuiverPath::End, enda);
	path.setLength(VipQuiverPath::Start, startl);
	path.setLength(VipQuiverPath::End, endl);
	path.setVisible(vis);
	return str;
}

static bool register_types()
{
	qRegisterMetaType<VipQuiver>();
	qRegisterMetaType<VipQuiverPath>();
	qRegisterMetaTypeStreamOperators<VipQuiverPath>();
	return true;
}
static bool _register_types = register_types();
