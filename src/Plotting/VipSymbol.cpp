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

#include "VipSymbol.h"
#include "VipPainter.h"
#include "VipShapeDevice.h"

#include <QApplication>
#include <QGLWidget>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>
#include <QWidget>
#include <QWindow>
#include <cmath>

#ifdef QT_SVG_LIB
#include <QSvgRenderer>
#endif

/// \return A pixmap that can be used as backing store
///
/// \param widget Widget, for which the backinstore is intended
/// \param size Size of the pixmap
static QPixmap backingStore(QWidget* widget, const QSize& size)
{
	QPixmap pm;

#define QWT_HIGH_DPI 1

#if QT_VERSION >= 0x050000
	qreal pixelRatio = 1.0;

	if (widget && widget->windowHandle()) {
		pixelRatio = widget->windowHandle()->devicePixelRatio();
	}
	else {
		if (qApp)
			pixelRatio = qApp->devicePixelRatio();
	}

	pm = QPixmap(size * pixelRatio);
	pm.setDevicePixelRatio(pixelRatio);
#else
	Q_UNUSED(widget)
	pm = QPixmap(size);
#endif

#if QT_VERSION < 0x050000
#ifdef Q_WS_X11
	if (widget && isX11GraphicsSystem()) {
		if (pm.x11Info().screen() != widget->x11Info().screen())
			pm.x11SetScreen(widget->x11Info().screen());
	}
#endif
#endif

	return pm;
}

namespace Triangle
{
	enum Type
	{
		Left,
		Right,
		Up,
		Down
	};
}

static inline void qwtDrawPixmapSymbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	QSizeF size = symbol.size();
	if (size.isEmpty())
		size = symbol.pixmap().size();

	QPixmap pm = symbol.pixmap();

	QPointF pinPoint(0.5 * size.width(), 0.5 * size.height());
	if (symbol.isPinPointEnabled())
		pinPoint = symbol.pinPoint();

	QRectF pix_rect(0, 0, size.width(), size.height());

	for (int i = 0; i < numPoints; i++) {
		const QPointF pos = points[i] - pinPoint;
		painter->drawPixmap(pix_rect.translated(pos), pm, pix_rect);
	}
}

#ifdef QT_SVG_LIB

static inline void qwtDrawSvgSymbols(QPainter* painter, const QPointF* points, int numPoints, QSvgRenderer* renderer, const VipSymbol& symbol)
{
	if (renderer == nullptr || !renderer->isValid())
		return;

	const QRectF viewBox = renderer->viewBoxF();
	if (viewBox.isEmpty())
		return;

	QSizeF sz = symbol.size();
	if (!sz.isValid())
		sz = viewBox.size();

	const double sx = sz.width() / viewBox.width();
	const double sy = sz.height() / viewBox.height();

	QPointF pinPoint = viewBox.center();
	if (symbol.isPinPointEnabled())
		pinPoint = symbol.pinPoint();

	const double dx = sx * (pinPoint.x() - viewBox.left());
	const double dy = sy * (pinPoint.y() - viewBox.top());

	for (int i = 0; i < numPoints; i++) {
		const double x = points[i].x() - dx;
		const double y = points[i].y() - dy;

		renderer->render(painter, QRectF(x, y, sz.width(), sz.height()));
	}
}

#endif

static inline void qwtDrawEllipseSymbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	painter->setBrush(symbol.brush());
	painter->setPen(symbol.pen());

	const QSizeF size = symbol.size();

	if (VipPainter::roundingAlignment(painter)) {
		const int sw = size.width();
		const int sh = size.height();
		const int sw2 = size.width() / 2;
		const int sh2 = size.height() / 2;

		for (int i = 0; i < numPoints; i++) {
			const int x = qRound(points[i].x());
			const int y = qRound(points[i].y());

			const QRectF r(x - sw2, y - sh2, sw, sh);
			VipPainter::drawEllipse(painter, r);
		}
	}
	else {
		const double sw = size.width();
		const double sh = size.height();
		const double sw2 = 0.5 * size.width();
		const double sh2 = 0.5 * size.height();

		for (int i = 0; i < numPoints; i++) {
			const double x = points[i].x();
			const double y = points[i].y();

			const QRectF r(x - sw2, y - sh2, sw, sh);
			painter->drawEllipse(r);
		}
	}
}

static inline void qwtDrawRectSymbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	const QSizeF size = symbol.size();

	QPen pen = symbol.pen();
	pen.setJoinStyle(Qt::MiterJoin);
	painter->setPen(pen);
	painter->setBrush(symbol.brush());

	if (VipPainter::roundingAlignment(painter) && pen.widthF() == (int)pen.widthF()) {
		QPainter::RenderHints hints = painter->renderHints();
		painter->setRenderHint(QPainter::Antialiasing, false);
		const int sw = size.width();
		const int sh = size.height();
		const int sw2 = size.width() / 2;
		const int sh2 = size.height() / 2;

		for (int i = 0; i < numPoints; i++) {
			const int x = qRound(points[i].x());
			const int y = qRound(points[i].y());

			const QRect r(x - sw2, y - sh2, sw, sh);
			VipPainter::drawRect(painter, r);
		}
		painter->setRenderHints(hints);
	}
	else {
		const double sw = size.width();
		const double sh = size.height();
		const double sw2 = 0.5 * size.width();
		const double sh2 = 0.5 * size.height();

		for (int i = 0; i < numPoints; i++) {
			const double x = points[i].x();
			const double y = points[i].y();

			const QRectF r(x - sw2, y - sh2, sw, sh);
			painter->drawRect(r);
		}
	}
}

static inline void qwtDrawDiamondSymbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	const QSizeF size = symbol.size();

	QPen pen = symbol.pen();
	pen.setJoinStyle(Qt::MiterJoin);
	painter->setPen(pen);
	painter->setBrush(symbol.brush());

	if (VipPainter::roundingAlignment(painter)) {
		for (int i = 0; i < numPoints; i++) {
			const int x = qRound(points[i].x());
			const int y = qRound(points[i].y());

			const int x1 = x - size.width() / 2;
			const int y1 = y - size.height() / 2;
			const int x2 = x1 + size.width();
			const int y2 = y1 + size.height();

			QPolygonF polygon;
			polygon += QPointF(x, y1);
			polygon += QPointF(x1, y);
			polygon += QPointF(x, y2);
			polygon += QPointF(x2, y);

			VipPainter::drawPolygon(painter, polygon);
		}
	}
	else {
		QPolygonF polygon(4);
		for (int i = 0; i < numPoints; i++) {
			const QPointF& pos = points[i];

			const double x1 = pos.x() - 0.5 * size.width();
			const double y1 = pos.y() - 0.5 * size.height();
			const double x2 = x1 + size.width();
			const double y2 = y1 + size.height();

			polygon[0] = QPointF(pos.x(), y1);
			polygon[1] = QPointF(x2, pos.y());
			polygon[2] = QPointF(pos.x(), y2);
			polygon[3] = QPointF(x1, pos.y());

			painter->drawPolygon(polygon);
		}
	}
}

static inline void qwtDrawTriangleSymbols(QPainter* painter, Triangle::Type type, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	const QSizeF size = symbol.size();

	QPen pen = symbol.pen();
	pen.setJoinStyle(Qt::MiterJoin);
	painter->setPen(pen);

	painter->setBrush(symbol.brush());

	const bool doAlign = VipPainter::roundingAlignment(painter);

	double sw2 = 0.5 * size.width();
	double sh2 = 0.5 * size.height();

	if (doAlign) {
		sw2 = std::floor(sw2);
		sh2 = std::floor(sh2);
	}

	QVector<QPointF> triangle(3);
	QPointF* trianglePoints = triangle.data();

	for (int i = 0; i < numPoints; i++) {
		const QPointF& pos = points[i];

		double x = pos.x();
		double y = pos.y();

		if (doAlign) {
			x = qRound(x);
			y = qRound(y);
		}

		const double x1 = x - sw2;
		const double x2 = x1 + size.width();
		const double y1 = y - sh2;
		const double y2 = y1 + size.height();

		switch (type) {
			case Triangle::Left: {
				trianglePoints[0].rx() = x2;
				trianglePoints[0].ry() = y1;

				trianglePoints[1].rx() = x1;
				trianglePoints[1].ry() = y;

				trianglePoints[2].rx() = x2;
				trianglePoints[2].ry() = y2;

				break;
			}
			case Triangle::Right: {
				trianglePoints[0].rx() = x1;
				trianglePoints[0].ry() = y1;

				trianglePoints[1].rx() = x2;
				trianglePoints[1].ry() = y;

				trianglePoints[2].rx() = x1;
				trianglePoints[2].ry() = y2;

				break;
			}
			case Triangle::Up: {
				trianglePoints[0].rx() = x1;
				trianglePoints[0].ry() = y2;

				trianglePoints[1].rx() = x;
				trianglePoints[1].ry() = y1;

				trianglePoints[2].rx() = x2;
				trianglePoints[2].ry() = y2;

				break;
			}
			case Triangle::Down: {
				trianglePoints[0].rx() = x1;
				trianglePoints[0].ry() = y1;

				trianglePoints[1].rx() = x;
				trianglePoints[1].ry() = y2;

				trianglePoints[2].rx() = x2;
				trianglePoints[2].ry() = y1;

				break;
			}
		}

		VipPainter::drawPolygon(painter, triangle);
	}
}

static inline void qwtDrawLineSymbols(QPainter* painter, int orientations, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	const QSizeF size = symbol.size();
	int off = -1;
	QPen pen = symbol.pen();
	if (pen.width() > 1) {
		pen.setCapStyle(Qt::FlatCap);
		if (qCeil(pen.widthF()) % 2 != 0)
			off = 0; // odd pen width
	}

	painter->setPen(pen);
	QPainter::RenderHints hints = painter->renderHints();
	if (!painter->transform().isRotating())
		painter->setRenderHint(QPainter::Antialiasing, false);

	if (VipPainter::roundingAlignment(painter)) {
		const int sw = qFloor(size.width());
		const int sh = qFloor(size.height());
		const int sw2 = size.width() / 2;
		const int sh2 = size.height() / 2;

		for (int i = 0; i < numPoints; i++) {
			if (orientations & Qt::Horizontal) {
				const int x = qRound(points[i].x()) - sw2;
				const int y = qRound(points[i].y());

				painter->drawLine(x, y, x + sw + off, y);
			}
			if (orientations & Qt::Vertical) {
				const int x = qRound(points[i].x());
				const int y = qRound(points[i].y()) - sh2;

				painter->drawLine(x, y, x, y + sh + off);
			}
		}
	}
	else {
		const double sw = size.width();
		const double sh = size.height();
		const double sw2 = 0.5 * size.width();
		const double sh2 = 0.5 * size.height();

		for (int i = 0; i < numPoints; i++) {
			if (orientations & Qt::Horizontal) {
				const double x = points[i].x() - sw2;
				const double y = points[i].y();

				painter->drawLine(x, y, x + sw, y);
			}
			if (orientations & Qt::Vertical) {
				const double y = points[i].y() - sh2;
				const double x = points[i].x();

				painter->drawLine(x, y, x, y + sh);
			}
		}
	}

	painter->setRenderHints(hints);
}

static inline void qwtDrawXCrossSymbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	const QSizeF size = symbol.size();
	double end_w = 0;
	int off = -1;
	QPen pen = symbol.pen();
	if (pen.width() > 1) {
		pen.setCapStyle(Qt::FlatCap);
		off = 0;
	}
	painter->setPen(pen);
	if (!painter->testRenderHint(QPainter::Antialiasing) && pen.widthF() <= 2)
		off = 0;

	if (VipPainter::roundingAlignment(painter)) {
		const int sw = size.width();
		const int sh = size.height();
		const int sw2 = size.width() / 2;
		const int sh2 = size.height() / 2;

		for (int i = 0; i < numPoints; i++) {
			const QPointF& pos = points[i];

			const int x = qRound(pos.x());
			const int y = qRound(pos.y());

			const int x1 = x - sw2;
			const int x2 = x1 + sw; // +off;
			const int y1 = y - sh2;
			const int y2 = y1 + sh; // +off;

			painter->drawLine(x1, y1, x2 + off, y2 + off);
			painter->drawLine(x2 + off, y1 - off, x1, y2);
		}
	}
	else {
		const double sw = size.width();
		const double sh = size.height();
		const double sw2 = 0.5 * size.width();
		const double sh2 = 0.5 * size.height();

		for (int i = 0; i < numPoints; i++) {
			const QPointF& pos = points[i];

			const double x1 = pos.x() - sw2;
			const double x2 = x1 + sw;
			const double y1 = pos.y() - sh2;
			const double y2 = y1 + sh;

			painter->drawLine(x1, y1, x2 + end_w, y2 + end_w);
			painter->drawLine(x1, y2, x2 + end_w, y1 - end_w);
		}
	}
}

static inline void qwtDrawStar1Symbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	const QSizeF size = symbol.size();
	painter->setPen(symbol.pen());

	// painter->setRenderHint(QPainter::Antialiasing, false);
	if (VipPainter::roundingAlignment(painter)) {
		QRect r(0, 0, size.width(), size.height());

		QTransform tr = VipPainter::resetTransform(painter);

		for (int i = 0; i < numPoints; i++) {
			r.moveCenter(points[i].toPoint());

			const double sqrt1_2 = 0.70710678118654752440; // 1/sqrt(2)

			const double d1 = r.width() / 2.0 * (1.0 - sqrt1_2);

			QPointF p1 = vipRound(QPoint(qRound(r.left() + d1), qRound(r.top() + d1)), tr);
			QPointF p2 = vipRound(QPoint(qRound(r.right() - d1), qRound(r.bottom() - d1)), tr);
			painter->drawLine(p1, p2);
			p1 = vipRound(QPoint(qRound(r.left() + d1), qRound(r.bottom() - d1)), tr);
			p2 = vipRound(QPoint(qRound(r.right() - d1), qRound(r.top() + d1)), tr);
			painter->drawLine(p1, p2);

			const QPoint c = r.center();

			p1 = vipRound(QPointF(c.x(), r.top()), tr);
			p2 = vipRound(QPointF(c.x(), r.bottom()), tr);
			painter->drawLine(p1, p2);
			p1 = vipRound(QPointF(r.left(), c.y()), tr);
			p2 = vipRound(QPointF(r.right(), c.y()), tr);
			painter->drawLine(p1, p2);
		}

		painter->setTransform(tr);
	}
	else {
		QRectF r(0, 0, size.width(), size.height());

		for (int i = 0; i < numPoints; i++) {
			r.moveCenter(points[i]);

			const double sqrt1_2 = 0.70710678118654752440; // 1/sqrt(2)

			const QPointF c = r.center();
			const double d1 = r.width() / 2.0 * (1.0 - sqrt1_2);

			painter->drawLine(r.left() + d1, r.top() + d1, r.right() - d1, r.bottom() - d1);
			painter->drawLine(r.left() + d1, r.bottom() - d1, r.right() - d1, r.top() + d1);
			painter->drawLine(c.x(), r.top(), c.x(), r.bottom());
			painter->drawLine(r.left(), c.y(), r.right(), c.y());
		}
	}
}

static inline void qwtDrawStar2Symbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	QPen pen = symbol.pen();
	if (pen.width() > 1)
		pen.setCapStyle(Qt::FlatCap);
	pen.setJoinStyle(Qt::MiterJoin);
	painter->setPen(pen);

	painter->setBrush(symbol.brush());

	const double cos30 = 0.866025; // cos(30�)

	const double dy = 0.25 * symbol.size().height();
	const double dx = 0.5 * symbol.size().width() * cos30 / 3.0;

	QVector<QPointF> star(12);
	QPointF* starPoints = star.data();

	const bool doAlign = VipPainter::roundingAlignment(painter);

	for (int i = 0; i < numPoints; i++) {
		double x = points[i].x();
		double y = points[i].y();
		if (doAlign) {
			x = qRound(x);
			y = qRound(y);
		}

		double x1 = x - 3 * dx;
		double y1 = y - 2 * dy;
		if (doAlign) {
			x1 = qRound(x - 3 * dx);
			y1 = qRound(y - 2 * dy);
		}

		const double x2 = x1 + 1 * dx;
		const double x3 = x1 + 2 * dx;
		const double x4 = x1 + 3 * dx;
		const double x5 = x1 + 4 * dx;
		const double x6 = x1 + 5 * dx;
		const double x7 = x1 + 6 * dx;

		const double y2 = y1 + 1 * dy;
		const double y3 = y1 + 2 * dy;
		const double y4 = y1 + 3 * dy;
		const double y5 = y1 + 4 * dy;

		starPoints[0].rx() = x4;
		starPoints[0].ry() = y1;

		starPoints[1].rx() = x5;
		starPoints[1].ry() = y2;

		starPoints[2].rx() = x7;
		starPoints[2].ry() = y2;

		starPoints[3].rx() = x6;
		starPoints[3].ry() = y3;

		starPoints[4].rx() = x7;
		starPoints[4].ry() = y4;

		starPoints[5].rx() = x5;
		starPoints[5].ry() = y4;

		starPoints[6].rx() = x4;
		starPoints[6].ry() = y5;

		starPoints[7].rx() = x3;
		starPoints[7].ry() = y4;

		starPoints[8].rx() = x1;
		starPoints[8].ry() = y4;

		starPoints[9].rx() = x2;
		starPoints[9].ry() = y3;

		starPoints[10].rx() = x1;
		starPoints[10].ry() = y2;

		starPoints[11].rx() = x3;
		starPoints[11].ry() = y2;

		// painter->drawPolygon(star);
		VipPainter::drawPolygon(painter, star);
	}
}

static inline void qwtDrawHexagonSymbols(QPainter* painter, const QPointF* points, int numPoints, const VipSymbol& symbol)
{
	painter->setBrush(symbol.brush());
	painter->setPen(symbol.pen());

	const double cos30 = 0.866025; // cos(30�)
	const double dx = 0.5 * (symbol.size().width() - cos30);

	const double dy = 0.25 * symbol.size().height();

	QVector<QPointF> hexaPolygon(6);
	QPointF* hexaPoints = hexaPolygon.data();

	const bool doAlign = VipPainter::roundingAlignment(painter);

	for (int i = 0; i < numPoints; i++) {
		double x = points[i].x();
		double y = points[i].y();
		if (doAlign) {
			x = qRound(x);
			y = qRound(y);
		}

		double x1 = x - dx;
		double y1 = y - 2 * dy;
		if (doAlign) {
			x1 = std::ceil(x1);
			y1 = std::ceil(y1);
		}

		const double x2 = x1 + 1 * dx;
		const double x3 = x1 + 2 * dx;

		const double y2 = y1 + 1 * dy;
		const double y3 = y1 + 3 * dy;
		const double y4 = y1 + 4 * dy;

		hexaPoints[0].rx() = x2;
		hexaPoints[0].ry() = y1;

		hexaPoints[1].rx() = x3;
		hexaPoints[1].ry() = y2;

		hexaPoints[2].rx() = x3;
		hexaPoints[2].ry() = y3;

		hexaPoints[3].rx() = x2;
		hexaPoints[3].ry() = y4;

		hexaPoints[4].rx() = x1;
		hexaPoints[4].ry() = y3;

		hexaPoints[5].rx() = x1;
		hexaPoints[5].ry() = y2;

		// painter->drawPolygon(hexaPolygon);
		VipPainter::drawPolygon(painter, hexaPolygon);
	}
}

class VipSymbol::PrivateData
{
public:
	PrivateData(VipSymbol::Style st, const QBrush& br, const QPen& pn, const QSizeF& sz)
	  : style(st)
	  , size(sz)
	  , brush(br)
	  , pen(pn)
	  , isPinPointEnabled(false)
	{
		cache.policy = VipSymbol::AutoCache;
#ifdef QT_SVG_LIB
		svg.renderer = nullptr;
#endif
	}

	~PrivateData()
	{
#ifdef QT_SVG_LIB
		delete svg.renderer;
#endif
	}

	Style style;
	QSizeF size;
	QBrush brush;
	QPen pen;
	QPainter::RenderHints hints;
	bool isPinPointEnabled;
	QPointF pinPoint;

	struct Pixmap
	{
		QPixmap pixmap;

	} pixmap;

#ifdef QT_SVG_LIB
	struct SVG
	{
		QSvgRenderer* renderer;
	} svg;
#endif

	struct PaintCache
	{
		VipSymbol::CachePolicy policy;
		QPixmap pixmap;
		QPainter::RenderHints hints;
		PaintCache()
		  : policy(AutoCache)
		{
		}

	} cache;
};

/// Default Constructor
/// \param style VipSymbol Style
///
/// The symbol is constructed with gray interior,
/// black outline with zero width, no size .
VipSymbol::VipSymbol(Style style)
{
	d_data = new PrivateData(style, QBrush(Qt::gray), QPen(Qt::black, 0), QSizeF());
}

VipSymbol::VipSymbol(const VipSymbol& other)
{
	d_data = new PrivateData(other.d_data->style, other.d_data->brush, other.d_data->pen, other.d_data->size);
	d_data->isPinPointEnabled = other.d_data->isPinPointEnabled;
	d_data->pinPoint = other.d_data->pinPoint;
	d_data->pixmap = other.d_data->pixmap;
	d_data->cache = other.d_data->cache;
}

/// \brief Constructor
/// \param style VipSymbol Style
/// \param brush brush to fill the interior
/// \param pen outline pen
/// \param size size
///
/// \sa setStyle(), setBrush(), setPen(), setSize()
VipSymbol::VipSymbol(VipSymbol::Style style, const QBrush& brush, const QPen& pen, const QSizeF& size)
{
	d_data = new PrivateData(style, brush, pen, size);
}

//! Destructor
VipSymbol::~VipSymbol()
{
	delete d_data;
}

VipSymbol& VipSymbol::operator=(const VipSymbol& other)
{
	delete d_data;
	d_data = new PrivateData(other.d_data->style, other.d_data->brush, other.d_data->pen, other.d_data->size);
	d_data->isPinPointEnabled = other.d_data->isPinPointEnabled;
	d_data->pinPoint = other.d_data->pinPoint;
	d_data->pixmap = other.d_data->pixmap;
	d_data->cache = other.d_data->cache;
	return *this;
}

/// Change the cache policy
///
/// The default policy is AutoCache
///
/// \param policy Cache policy
/// \sa CachePolicy, cachePolicy()
void VipSymbol::setCachePolicy(VipSymbol::CachePolicy policy)
{
	if (d_data->cache.policy != policy) {
		d_data->cache.policy = policy;
		invalidateCache();
	}
}

/// \return Cache policy
/// \sa CachePolicy, setCachePolicy()
VipSymbol::CachePolicy VipSymbol::cachePolicy() const
{
	return d_data->cache.policy;
}

/// Set a pixmap as symbol
///
/// \param pixmap Pixmap
///
/// \sa pixmap(), setGraphic()
///
/// \note the style() is set to VipSymbol::Pixmap
/// \note brush() and pen() have no effect
void VipSymbol::setPixmap(const QPixmap& pixmap)
{
	d_data->style = VipSymbol::Pixmap;
	d_data->pixmap.pixmap = pixmap;
}

/// \return Assigned pixmap
/// \sa setPixmap()
const QPixmap& VipSymbol::pixmap() const
{
	return d_data->pixmap.pixmap;
}

#ifdef QT_SVG_LIB

/// Set a SVG icon as symbol
///
/// \param svgDocument SVG icon
///
/// \sa setGraphic(), setPixmap()
///
/// \note the style() is set to VipSymbol::SvgDocument
/// \note brush() and pen() have no effect
void VipSymbol::setSvgDocument(const QByteArray& svgDocument)
{
	d_data->style = VipSymbol::SvgDocument;
	if (d_data->svg.renderer == nullptr)
		d_data->svg.renderer = new QSvgRenderer();

	d_data->svg.renderer->load(svgDocument);
}

#endif

/// \brief Specify the symbol's size
///
/// If the 'h' parameter is left out or less than 0,
/// and the 'w' parameter is greater than or equal to 0,
/// the symbol size will be set to (w,w).
///
/// \param width Width
/// \param height Height (defaults to -1)
///
/// \sa size()
void VipSymbol::setSize(qreal width, qreal height)
{
	if ((width >= 0) && (height < 0))
		height = width;

	setSize(QSizeF(width, height));
}

/// Set the symbol's size
/// \param size Size
///
/// \sa size()
void VipSymbol::setSize(const QSizeF& size)
{
	if (size.isValid() && size != d_data->size) {
		d_data->size = size;
		if (d_data->cache.policy != NoCache)
			invalidateCache();
	}
}

/// \return Size
/// \sa setSize()
const QSizeF& VipSymbol::size() const
{
	return d_data->size;
}

/// \brief Assign a brush
///
/// The brush is used to draw the interior of the symbol.
/// \param brush Brush
///
/// \sa brush()
void VipSymbol::setBrush(const QBrush& brush)
{
	d_data->brush = brush;
	if (d_data->cache.policy != NoCache)
		invalidateCache();
}

void VipSymbol::setBrushColor(const QColor& c)
{
	if (c != d_data->brush.color()) {
		d_data->brush.setColor(c);
		if (d_data->cache.policy != NoCache)
			invalidateCache();
	}
}

/// \return Brush
/// \sa setBrush()
const QBrush& VipSymbol::brush() const
{
	return d_data->brush;
}

/// Build and assign a pen
///
/// In Qt5 the default pen width is 1.0 ( 0.0 in Qt4 )
/// what makes it non cosmetic ( see QPen::isCosmetic() ).
/// This method has been introduced to hide this incompatibility.
///
/// \param color Pen color
/// \param width Pen width
/// \param style Pen style
///
/// \sa pen(), brush()
void VipSymbol::setPen(const QColor& color, qreal width, Qt::PenStyle style)
{
	setPen(QPen(color, width, style));
}

/// Assign a pen
///
/// The pen is used to draw the symbol's outline.
///
/// \param pen Pen
/// \sa pen(), setBrush()
void VipSymbol::setPen(const QPen& pen)
{
	d_data->pen = pen;
	if (d_data->cache.policy != NoCache)
		invalidateCache();
}

void VipSymbol::setPenColor(const QColor& c)
{
	if (c != d_data->pen.color()) {
		d_data->pen.setColor(c);
		if (d_data->cache.policy != NoCache)
			invalidateCache();
	}
}

/// \return Pen
/// \sa setPen(), brush()
const QPen& VipSymbol::pen() const
{
	return d_data->pen;
}

/// \brief Set the color of the symbol
///
/// Change the color of the brush for symbol types with a filled area.
/// For all other symbol types the color will be assigned to the pen.
///
/// \param color Color
///
/// \sa setBrush(), setPen(), brush(), pen()
void VipSymbol::setColor(const QColor& color)
{
	switch (d_data->style) {
		case VipSymbol::Ellipse:
		case VipSymbol::Rect:
		case VipSymbol::Diamond:
		case VipSymbol::Triangle:
		case VipSymbol::UTriangle:
		case VipSymbol::DTriangle:
		case VipSymbol::RTriangle:
		case VipSymbol::LTriangle:
		case VipSymbol::Star2:
		case VipSymbol::Hexagon: {
			if (d_data->brush.color() != color) {
				d_data->brush.setColor(color);
				if (d_data->cache.policy != NoCache)
					invalidateCache();
			}
			break;
		}
		case VipSymbol::Cross:
		case VipSymbol::XCross:
		case VipSymbol::HLine:
		case VipSymbol::VLine:
		case VipSymbol::Star1: {
			if (d_data->pen.color() != color) {
				d_data->pen.setColor(color);
				if (d_data->cache.policy != NoCache)
					invalidateCache();
			}
			break;
		}
		default: {
			if (d_data->brush.color() != color || d_data->pen.color() != color) {
				if (d_data->cache.policy != NoCache)
					invalidateCache();
			}

			d_data->brush.setColor(color);
			d_data->pen.setColor(color);
		}
	}
}

/// \brief Set and enable a pin point
///
/// The position of a complex symbol is not always aligned to its center
/// ( f.e an arrow, where the peak points to a position ). The pin point
/// defines the position inside of a Pixmap, Graphic, SvgDocument
/// or PainterPath symbol where the represented point has to
/// be aligned to.
///
/// \param pos Position
/// \param enable En/Disable the pin point alignment
///
/// \sa pinPoint(), setPinPointEnabled()
void VipSymbol::setPinPoint(const QPointF& pos, bool enable)
{
	if (d_data->pinPoint != pos) {
		d_data->pinPoint = pos;
		if (d_data->isPinPointEnabled) {
			if (d_data->cache.policy != NoCache)
				invalidateCache();
		}
	}

	setPinPointEnabled(enable);
}

/// \return Pin point
/// \sa setPinPoint(), setPinPointEnabled()
QPointF VipSymbol::pinPoint() const
{
	return d_data->pinPoint;
}

/// En/Disable the pin point alignment
///
/// \param on Enabled, when on is true
/// \sa setPinPoint(), isPinPointEnabled()
void VipSymbol::setPinPointEnabled(bool on)
{
	if (d_data->isPinPointEnabled != on) {
		d_data->isPinPointEnabled = on;
		if (d_data->cache.policy != NoCache)
			invalidateCache();
	}
}

/// \return True, when the pin point translation is enabled
/// \sa setPinPoint(), setPinPointEnabled()
bool VipSymbol::isPinPointEnabled() const
{
	return d_data->isPinPointEnabled;
}

/// Render an array of symbols
///
/// Painting several symbols is more effective than drawing symbols
/// one by one, as a couple of layout calculations and setting of pen/brush
/// can be done once for the complete array.
///
/// \param painter VipPainter
/// \param points Array of points
/// \param numPoints Number of points
void VipSymbol::drawSymbols(QPainter* painter, const QPointF* points, int numPoints) const
{
	if (numPoints <= 0)
		return;

	bool useCache = false;
	bool is_opengl = VipPainter::isOpenGL(painter);
	bool is_raster = painter->paintEngine() && (painter->paintEngine()->type() == QPaintEngine::Raster);
	// Don't use the pixmap, when the paint device
	// could generate scalable vectors

	if (is_raster || VipPainter::roundingAlignment(painter)) {
		if (d_data->cache.policy == VipSymbol::Cache) {
			useCache = true;
		}
		else if (d_data->cache.policy == VipSymbol::AutoCache) {
			useCache = !VipPainter::isVectoriel(painter);
			// if ( painter->paintEngine()->type() == QPaintEngine::Raster )
			//       {
			//           useCache = true;
			//       }
			//       else
			//       {
			//           switch( d_data->style )
			//           {
			//               case VipSymbol::XCross:
			//               case VipSymbol::HLine:
			//               case VipSymbol::VLine:
			//               case VipSymbol::Cross:
			//                   break;
			//
			//               case VipSymbol::Pixmap:
			//               {
			//                   if ( !d_data->size.isEmpty() &&
			//                       d_data->size != d_data->pixmap.pixmap.size() )
			//                   {
			//                       useCache = true;
			//                   }
			//                   break;
			//               }
			//               default:
			//	useCache =  true;
			//           }
			//       }
		}
	}

	if (useCache) {
		double pen_w = 0;
		if (d_data->style != VipSymbol::Pixmap && d_data->style != VipSymbol::SvgDocument && d_data->style != VipSymbol::UserStyle)
			pen_w = pen().widthF();

		const QRectF br = boundingRect();
		const QRectF rect(0, 0, br.width() + pen_w, br.height() + pen_w);

		if (d_data->cache.pixmap.isNull() || painter->renderHints() != d_data->cache.hints) {
			d_data->cache.pixmap = backingStore(nullptr, rect.size().toSize());
			d_data->cache.pixmap.fill(Qt::transparent);

			QPainter p(&d_data->cache.pixmap);
			p.setRenderHints(d_data->cache.hints = painter->renderHints());
			p.translate((-br.topLeft() + QPointF(pen_w / 2, pen_w / 2)).toPoint());

			const QPointF pos;
			renderSymbols(&p, &pos, 1);
		}

		const double dx = br.left();
		const double dy = br.top();
		const double pen_w2 = pen_w / 2;

		QPainter::RenderHints hints = painter->renderHints();
		painter->setRenderHint(QPainter::SmoothPixmapTransform);
		for (int i = 0; i < numPoints; i++) {
			const int left = qRound(points[i].x() + dx - pen_w2);
			const int top = qRound(points[i].y() + dy - pen_w2);
			painter->drawPixmap(left, top, d_data->cache.pixmap);
		}
		painter->setRenderHints(hints);
	}
	else {
		if (is_opengl && d_data->style != VipSymbol::Pixmap && d_data->style != VipSymbol::SvgDocument && d_data->style != VipSymbol::UserStyle) {

			// faster to draw one single shape with opengl
			VipShapeDevice dev;
			QPainter p(&dev);
			renderSymbols(&p, points, numPoints);
			QPainterPath path = dev.shape();
			path.setFillRule(Qt::WindingFill);
			painter->setPen(pen());
			painter->setBrush(brush());
			painter->drawPath(path);
		}
		else
			renderSymbols(painter, points, numPoints);
	}
}

#include <qbitmap.h>
QPainterPath VipSymbol::extractShape(QBitmap* bitmap, const QRect& rect, const QPointF* points, int numPoints) const
{
	if ((double)rect.width() * (double)rect.height() > 20000000) {
		// rect to big, add all symbols separatly
		QPainterPath res;

		const QRectF br = boundingRect();
		const int dx = br.left();
		const int dy = br.top();

		for (int i = 0; i < numPoints; i++) {
			const double left = (points[i].x()) + dx;
			const double top = (points[i].y()) + dy;

			res.addRect(QRectF(left, top, br.width(), br.height()));
		}
		return res;
	}
	else {
		// use a QBitmap to extract the shape
		bool own = (bitmap == nullptr);
		if (bitmap) {
			if (bitmap->width() != rect.width() / 2 || bitmap->height() != rect.height() / 2)
				*bitmap = QBitmap(rect.width() / 2, rect.height() / 2);
		}
		else
			bitmap = new QBitmap(rect.width() / 2, rect.height() / 2);

		bitmap->fill(Qt::color0);

		{
			QPainter p(bitmap);
			const QRect br = boundingRect().toRect();
			const int dx = br.left();
			const int dy = br.top();

			for (int i = 0; i < numPoints; i++) {
				const int left = (points[i].x() + dx - rect.left()) / 2;
				const int top = (points[i].y() + dy - rect.top()) / 2;

				p.fillRect(left, top, br.width() / 2, br.height() / 2, Qt::color1);
			}
		}
		QPainterPath res;
		res.addRegion(QRegion(*bitmap));
		res = QTransform().scale(2, 2).map(res);
		res.translate(rect.topLeft());
		if (own)
			delete bitmap;
		return res;
	}
}

QPainterPath VipSymbol::shape(const QPointF& pos) const
{
	VipShapeDevice device;
	QPainter painter(&device);
	// disable cache, otherwise the shape will always be a rectangle since we draw a pixmap
	VipSymbol::CachePolicy policy = cachePolicy();
	const_cast<VipSymbol*>(this)->setCachePolicy(VipSymbol::NoCache);
	drawSymbol(&painter, pos);
	const_cast<VipSymbol*>(this)->setCachePolicy(policy);
	QPainterPath p = device.shape();
	p.closeSubpath();
	return p;
}

/// \brief Draw the symbol into a rectangle
///
/// The symbol is painted centered and scaled into the target rectangle.
/// It is always painted uncached and the pin point is ignored.
///
/// This method is primarily intended for drawing a symbol to
/// the legend.
///
/// \param painter VipPainter
/// \param rect Target rectangle for the symbol
void VipSymbol::drawSymbol(QPainter* painter, const QRectF& rect) const
{
	if (d_data->style == VipSymbol::None)
		return;

	if (d_data->style == VipSymbol::SvgDocument) {
#ifdef QT_SVG_LIB
		if (d_data->svg.renderer) {
			QRectF scaledRect;

			QSizeF sz = d_data->svg.renderer->viewBoxF().size();
			if (!sz.isEmpty()) {
				sz.scale(rect.size(), Qt::KeepAspectRatio);
				scaledRect.setSize(sz);
				scaledRect.moveCenter(rect.center());
			}
			else {
				scaledRect = rect;
			}

			d_data->svg.renderer->render(painter, scaledRect);
		}
#endif
	}
	else {
		if (size().isEmpty() && d_data->style != VipSymbol::Path && d_data->style != VipSymbol::Pixmap && d_data->style != VipSymbol::UserStyle)
			return;

		const QRectF br = boundingRect();

		// scale the symbol size to fit into rect.

		const double ratio = qMin(rect.width() / br.width(), rect.height() / br.height());

		painter->save();

		painter->translate(rect.center());
		painter->scale(ratio, ratio);

		const bool isPinPointEnabled = d_data->isPinPointEnabled;
		d_data->isPinPointEnabled = false;

		const QPointF pos;
		renderSymbols(painter, &pos, 1);

		d_data->isPinPointEnabled = isPinPointEnabled;

		painter->restore();
	}
}

/// Render the symbol to series of points
///
/// \param painter Qt painter
/// \param points Positions of the symbols
/// \param numPoints Number of points
void VipSymbol::renderSymbols(QPainter* painter, const QPointF* points, int numPoints) const
{
	if (d_data->style == VipSymbol::None)
		return;

	switch (d_data->style) {
		case VipSymbol::Ellipse: {
			qwtDrawEllipseSymbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Rect: {
			qwtDrawRectSymbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Diamond: {
			qwtDrawDiamondSymbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Cross: {
			qwtDrawLineSymbols(painter, Qt::Horizontal | Qt::Vertical, points, numPoints, *this);
			break;
		}
		case VipSymbol::XCross: {
			qwtDrawXCrossSymbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Triangle:
		case VipSymbol::UTriangle: {
			qwtDrawTriangleSymbols(painter, Triangle::Up, points, numPoints, *this);
			break;
		}
		case VipSymbol::DTriangle: {
			qwtDrawTriangleSymbols(painter, Triangle::Down, points, numPoints, *this);
			break;
		}
		case VipSymbol::RTriangle: {
			qwtDrawTriangleSymbols(painter, Triangle::Right, points, numPoints, *this);
			break;
		}
		case VipSymbol::LTriangle: {
			qwtDrawTriangleSymbols(painter, Triangle::Left, points, numPoints, *this);
			break;
		}
		case VipSymbol::HLine: {
			qwtDrawLineSymbols(painter, Qt::Horizontal, points, numPoints, *this);
			break;
		}
		case VipSymbol::VLine: {
			qwtDrawLineSymbols(painter, Qt::Vertical, points, numPoints, *this);
			break;
		}
		case VipSymbol::Star1: {
			qwtDrawStar1Symbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Star2: {
			qwtDrawStar2Symbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Hexagon: {
			qwtDrawHexagonSymbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::Pixmap: {
			qwtDrawPixmapSymbols(painter, points, numPoints, *this);
			break;
		}
		case VipSymbol::SvgDocument: {
#ifdef QT_SVG_LIB
			qwtDrawSvgSymbols(painter, points, numPoints, d_data->svg.renderer, *this);
#endif
			break;
		}
		default:;
	}
}

/// Calculate the bounding rectangle for a symbol
/// at position (0,0).
///
/// \return Bounding rectangle
QRectF VipSymbol::boundingRect() const
{
	QRectF rect;

	bool pinPointTranslation = false;

	switch (d_data->style) {
		case VipSymbol::Ellipse:
		case VipSymbol::Rect:
		case VipSymbol::Hexagon:
		case VipSymbol::None: {
			qreal pw = 0.0;
			if (d_data->pen.style() != Qt::NoPen)
				pw = qMax(d_data->pen.widthF(), qreal(1.0));

			rect.setSize(d_data->size + QSizeF(pw, pw));
			rect.moveCenter(QPointF(0.0, 0.0));

			break;
		}
		case VipSymbol::XCross:
		case VipSymbol::Diamond:
		case VipSymbol::Triangle:
		case VipSymbol::UTriangle:
		case VipSymbol::DTriangle:
		case VipSymbol::RTriangle:
		case VipSymbol::LTriangle:
		case VipSymbol::Star1:
		case VipSymbol::Star2: {
			qreal pw = 0.0;
			if (d_data->pen.style() != Qt::NoPen)
				pw = qMax(d_data->pen.widthF(), qreal(1.0));

			rect.setSize(d_data->size + QSizeF(2 * pw, 2 * pw));
			rect.moveCenter(QPointF(0.0, 0.0));
			break;
		}
		case VipSymbol::Pixmap: {
			if (d_data->size.isEmpty())
				rect.setSize(d_data->pixmap.pixmap.size());
			else
				rect.setSize(d_data->size);

			pinPointTranslation = true;

			break;
		}
#ifdef QT_SVG_LIB
		case VipSymbol::SvgDocument: {
			if (d_data->svg.renderer)
				rect = d_data->svg.renderer->viewBoxF();

			if (d_data->size.isValid() && !rect.isEmpty()) {
				QSizeF sz = rect.size();

				const double sx = d_data->size.width() / sz.width();
				const double sy = d_data->size.height() / sz.height();

				QTransform transform;
				transform.scale(sx, sy);

				rect = transform.mapRect(rect);
			}
			pinPointTranslation = true;
			break;
		}
#endif
		default: {
			rect.setSize(d_data->size);
			rect.moveCenter(QPointF(0.0, 0.0));
		}
	}

	if (pinPointTranslation) {
		QPointF pinPoint(0.0, 0.0);
		if (d_data->isPinPointEnabled)
			pinPoint = rect.center() - d_data->pinPoint;

		rect.moveCenter(pinPoint);
	}

	// QRect r;
	// r.setLeft( std::floor( rect.left() ) );
	// r.setTop( std::floor( rect.top() ) );
	// r.setRight( std::ceil( rect.right() ) );
	// r.setBottom( std::ceil( rect.bottom() ) );
	//
	// if ( d_data->style != VipSymbol::Pixmap )
	//   r.adjust( -1, -1, 1, 1 ); // for antialiasing

	return rect;
}

/// Invalidate the cached symbol pixmap
///
/// The symbol invalidates its cache, whenever an attribute is changed
/// that has an effect ob how to display a symbol. In case of derived
/// classes with individual styles ( >= VipSymbol::UserStyle ) it
/// might be necessary to call invalidateCache() for attributes
/// that are relevant for this style.
///
/// \sa CachePolicy, setCachePolicy(), drawSymbols()
void VipSymbol::invalidateCache()
{
	if (!d_data->cache.pixmap.isNull())
		d_data->cache.pixmap = QPixmap();
}

/// Specify the symbol style
///
/// \param style Style
/// \sa style()
void VipSymbol::setStyle(VipSymbol::Style style)
{
	if (d_data->style != style) {
		d_data->style = style;
		invalidateCache();
	}
}

/// \return Current symbol style
/// \sa setStyle()
VipSymbol::Style VipSymbol::style() const
{
	return d_data->style;
}

const char* VipSymbol::nameForStyle(Style style)
{
	switch (style) {
		case Ellipse:
			return "Ellipse";
		case Rect:
			return "Rect";
		case Diamond:
			return "Diamond";
		case Triangle:
			return "Triangle";
		case DTriangle:
			return "DTriangle";
		case UTriangle:
			return "UTriangle";
		case LTriangle:
			return "LTriangle";
		case RTriangle:
			return "RTriangle";
		case Cross:
			return "Cross";
		case XCross:
			return "XCross";
		case HLine:
			return "HLine";
		case VLine:
			return "VLine";
		case Star1:
			return "Star1";
		case Star2:
			return "Star2";
		case Hexagon:
			return "Hexagon";
		case Path:
			return "Path";
		case Pixmap:
			return "Pixmap";
		case SvgDocument:
			return "SvgDocument";
		default:
			return "UserStyle";
	}
}

QDataStream& operator<<(QDataStream& stream, const VipSymbol& s)
{
	return stream << (int)s.cachePolicy() << s.size() << s.pinPoint() << s.isPinPointEnabled() << s.brush() << s.pen() << (int)s.style() << s.pixmap();
}

QDataStream& operator>>(QDataStream& stream, VipSymbol& s)
{
	int cachePolicy, style;
	QBrush brush;
	QPen pen;
	QSizeF size;
	QPointF pinPoint;
	bool isPinPointEnabled;
	QPixmap pixmap;

	stream >> cachePolicy >> size >> pinPoint >> isPinPointEnabled >> brush >> pen >> style >> pixmap;
	s.setCachePolicy((VipSymbol::CachePolicy)cachePolicy);
	s.setSize(size);
	s.setPinPoint(pinPoint);
	s.setPinPointEnabled(isPinPointEnabled);
	s.setBrush(brush);
	s.setPen(pen);
	if (!pixmap.isNull())
		s.setPixmap(pixmap);
	s.setStyle((VipSymbol::Style)style);

	return stream;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipSymbol>();
	qRegisterMetaTypeStreamOperators<VipSymbol>("VipSymbol");
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();
