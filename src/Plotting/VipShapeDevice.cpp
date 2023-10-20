#include <QPaintEngine>
#include <QPainterPathStroker>

#include "VipShapeDevice.h"


class PathEngine : public QPaintEngine
{
	friend class VipShapeDevice;

	VipShapeDevice * m_device;
	QTransform m_tr;
	double m_penW2;
	bool m_extractBoundingRectOnly;
	QPainterPath * path();
	void drawRect(const QRectF & rect);

public:

	PathEngine(VipShapeDevice *);

	virtual bool	begin ( QPaintDevice * pdev );
	virtual void	drawEllipse ( const QRectF & rect );
	virtual void	drawImage ( const QRectF & rectangle, const QImage & image, const QRectF & sr, Qt::ImageConversionFlags flags = Qt::AutoColor );
	virtual void	drawPath ( const QPainterPath & path );
	virtual void	drawPixmap ( const QRectF & r, const QPixmap & pm, const QRectF & sr );
	virtual void	drawPoints ( const QPointF * points, int pointCount );
	virtual void	drawPoints ( const QPoint * points, int pointCount );
	virtual void	drawPolygon ( const QPointF * points, int pointCount, PolygonDrawMode mode );
	virtual void	drawPolygon ( const QPoint * points, int pointCount, PolygonDrawMode mode );
	virtual void	drawRects ( const QRectF * rects, int rectCount );
	virtual void	drawRects ( const QRect * rects, int rectCount );
	virtual void	drawTextItem ( const QPointF & p, const QTextItem & textItem );
	virtual void	drawTiledPixmap ( const QRectF & rect, const QPixmap & pixmap, const QPointF & p );
	virtual bool	end ();
	virtual Type	type () const;
	virtual void	updateState ( const QPaintEngineState & state );
};


PathEngine::PathEngine(VipShapeDevice * device)
:QPaintEngine (QPaintEngine::AllFeatures), m_device(device), m_penW2(0), m_extractBoundingRectOnly(false)
{}

QPainterPath * PathEngine::path()
{
	return &m_device->shape();
}

bool	PathEngine::begin ( QPaintDevice * pdev )
{
	Q_UNUSED(pdev)
	return true;
}

void PathEngine::drawRect(const QRectF & rect)
{
	if (m_extractBoundingRectOnly) {
		if (m_tr.type() != QTransform::TxNone)
			path()->addRect(m_tr.map(rect).boundingRect().adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2));
		else
			path()->addRect(rect.adjusted(-m_penW2,-m_penW2,m_penW2,m_penW2));
	}
	else {
		if (m_tr.type() != QTransform::TxNone)
			path()->addPolygon(m_tr.map(rect));
		else
			path()->addRect(rect);
	}
}

void	PathEngine::drawEllipse ( const QRectF & rect )
{
	if (m_extractBoundingRectOnly) {
		if (m_tr.type() != QTransform::TxNone)
			path()->addRect(m_tr.map(rect).boundingRect().adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2));
		else
			path()->addRect(rect.adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2));
	}
	else {
		if (m_tr.type() != QTransform::TxNone)
			QPaintEngine::drawEllipse(rect);
		else
			path()->addEllipse(rect);
	}
}

void	PathEngine::drawImage ( const QRectF & rectangle, const QImage & image, const QRectF & sr, Qt::ImageConversionFlags flags )
{
	Q_UNUSED(image)
	Q_UNUSED(flags)
	Q_UNUSED(sr)
	if (m_device->testDrawPrimitive(VipShapeDevice::Pixmap)) {
		if (m_extractBoundingRectOnly) {
			if (m_tr.type() != QTransform::TxNone)
				path()->addRect(m_tr.map(rectangle).boundingRect());
			else
				path()->addRect(rectangle);
		}
		else
			drawRect(rectangle);
	}
}

void	PathEngine::drawPath ( const QPainterPath & p )
{
	QPainterPath tmp = p;
	if(p.currentPosition() != QPointF(0,0))
		tmp.closeSubpath();

	if (m_extractBoundingRectOnly) {
		QRectF r;
		if (m_tr.type() != QTransform::TxNone)
			r = m_tr.map(tmp).boundingRect();
		else
			r = tmp.boundingRect();
		path()->addRect(r.adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2));
	}
	else {
		if (m_tr.type() != QTransform::TxNone)
			path()->addPath(m_tr.map(tmp));
		else
			path()->addPath(tmp);
	}
}

void	PathEngine::drawPixmap ( const QRectF & r, const QPixmap & pm, const QRectF & sr )
{
	Q_UNUSED(pm)
	Q_UNUSED(sr)
	if (m_device->testDrawPrimitive(VipShapeDevice::Pixmap)) {
		if (m_extractBoundingRectOnly) {
			if (m_tr.type() != QTransform::TxNone)
				path()->addRect(m_tr.map(r).boundingRect());
			else
				path()->addRect(r);
		}
		else
			drawRect(r);
	}
}

void	PathEngine::drawPoints ( const QPointF * points, int pointCount )
{
	if (!m_device->testDrawPrimitive(VipShapeDevice::Points))
		return;

	if (m_extractBoundingRectOnly) {
		QPolygonF p(pointCount);
		std::copy(points, points + pointCount, p.begin());
		if (m_tr.type() != QTransform::TxNone)
			p = m_tr.map(p);
		QRectF r = p.boundingRect().adjusted( -m_penW2, -m_penW2, m_penW2, m_penW2);
		path()->addRect(r);
	}
	else {
		if (m_tr.type() != QTransform::TxNone)
		{
			for (int i = 0; i < pointCount; ++i)
			{
				QPointF p = m_tr.map(points[i]);
				path()->moveTo(p);
				path()->lineTo(p + QPointF(0.1, 0.1));
			}
		}
		else
		{
			for (int i = 0; i < pointCount; ++i)
			{
				path()->moveTo(points[i]);
				path()->lineTo(points[i] + QPointF(0.1, 0.1));
			}
		}
	}
}

void	PathEngine::drawPoints ( const QPoint * points, int pointCount )
{
	if (!m_device->testDrawPrimitive(VipShapeDevice::Points))
		return;

	if (m_extractBoundingRectOnly) {
		QPolygon pi(pointCount);
		std::copy(points, points + pointCount, pi.begin());
		QPolygonF p;
		if (m_tr.type() != QTransform::TxNone)
			p = m_tr.map(pi);
		else {
			p = pi;
		}
		QRectF r = p.boundingRect().adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2);
		path()->addRect(r);
	}
	else {
		if (m_tr.type() != QTransform::TxNone)
		{
			for (int i = 0; i < pointCount; ++i)
			{
				QPointF p = m_tr.map(QPointF(points[i]));
				path()->moveTo(p);
				path()->lineTo(p + QPointF(0.1, 0.1));
			}
		}
		else
		{
			for (int i = 0; i < pointCount; ++i)
			{
				QPointF p(points[i]);
				path()->moveTo(p);
				path()->lineTo(p + QPointF(0.1, 0.1));
			}
		}
	}
}

void	PathEngine::drawPolygon ( const QPointF * points, int pointCount, PolygonDrawMode mode )
{
	if (!m_device->testDrawPrimitive(VipShapeDevice::Polyline) && mode == QPaintEngine::PolylineMode)
		return;

	if(!pointCount)
		return;

	if (m_extractBoundingRectOnly) {
		QPolygonF p(pointCount);
		std::copy(points, points + pointCount, p.begin());
		if (m_tr.type() != QTransform::TxNone)
			p = m_tr.map(p);
		QRectF r = p.boundingRect().adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2);
		path()->addRect(r);
	}
	else {
		QPolygonF p(pointCount + (mode != PolylineMode ? 1 : 0));
		if (m_tr.type() != QTransform::TxNone)
		{
			std::copy(points, points + pointCount, p.data());
		}
		else
		{
			for (int i = 0; i < pointCount; ++i)
				p[i] = m_tr.map(points[i]);
		}
		if (pointCount && mode != PolylineMode)
			p.last() = p.first();

		if (mode == QPaintEngine::WindingMode)
			path()->setFillRule(Qt::WindingFill);
		else
			path()->setFillRule(Qt::OddEvenFill);

		path()->addPolygon(p);
	}
}

void	PathEngine::drawPolygon ( const QPoint * points, int pointCount, PolygonDrawMode mode )
{
	if(!pointCount)
		return;

	if (m_extractBoundingRectOnly) {
		QPolygon pi(pointCount);
		std::copy(points, points + pointCount, pi.begin());
		QPolygonF p;
		if (m_tr.type() != QTransform::TxNone)
			p = m_tr.map(pi);
		else {
			p = pi;
		}
		QRectF r = p.boundingRect().adjusted(-m_penW2, -m_penW2, m_penW2, m_penW2);
		path()->addRect(r);
	}
	else {
		QPolygonF p(pointCount + (mode != PolylineMode ? 1 : 0));
		if (m_tr.type() != QTransform::TxNone)
		{
			std::copy(points, points + pointCount, p.data());
		}
		else
		{
			for (int i = 0; i < pointCount; ++i)
				p[i] = m_tr.map(QPointF(points[i]));
		}
		if (pointCount && mode != PolylineMode)
			p.last() = p.first();


		if (mode == QPaintEngine::WindingMode)
			path()->setFillRule(Qt::WindingFill);
		else
			path()->setFillRule(Qt::OddEvenFill);

		path()->addPolygon(p);
	}
}

void	PathEngine::drawRects ( const QRectF * rects, int rectCount )
{
	for(int i=0; i < rectCount; ++i)
		drawRect(rects[i]);
}

void	PathEngine::drawRects ( const QRect * rects, int rectCount )
{
	for(int i=0; i < rectCount; ++i)
		drawRect(QRectF(rects[i]));
}

void	PathEngine::drawTextItem ( const QPointF & p, const QTextItem & textItem )
{
	if (!m_device->testDrawPrimitive(VipShapeDevice::Text))
		return;

	const QFontMetricsF fm( textItem.font() );
	const QRectF rect = fm.boundingRect(
		QRectF( 0, 0, INT_MAX, INT_MAX ), 0, textItem.text() );
	drawRect(rect.translated(p - QPointF(0,rect.height())));
}

void	PathEngine::drawTiledPixmap ( const QRectF & rect, const QPixmap & , const QPointF &  )
{
	if (!m_device->testDrawPrimitive(VipShapeDevice::Pixmap))
		return;
	drawRect(rect);
}

bool	PathEngine::end ()
{
	return true;
}

QPaintEngine::Type	PathEngine::type () const
{
	return QPaintEngine::User;
}

void	PathEngine::updateState ( const QPaintEngineState & _state )
{
	if(_state.state() & QPaintEngine::DirtyTransform)
	{
		m_penW2 = _state.pen().widthF()/2;
		m_tr = _state.transform();
	}
}






VipShapeDevice::VipShapeDevice()
: m_engine(nullptr), m_drawPrimitives(All)
{
	m_engine = new PathEngine(this);
}

VipShapeDevice::~VipShapeDevice()
{
	delete m_engine;
}

const QPainterPath & VipShapeDevice::shape() const
{
	return m_path;
}

QPainterPath & VipShapeDevice::shape()
{
	return m_path;
}

QPainterPath VipShapeDevice::shape(double penWidth)
{
	//TEST
	QRectF r = m_path.boundingRect();
	if (std::isinf(r.width()) || std::isinf(r.height()) || r.width()==0 || r.height()==0)
		return QPainterPath();
	//
	QPainterPathStroker stroke;
	stroke.setWidth(penWidth);
	return stroke.createStroke(shape());
}

QPaintEngine *	VipShapeDevice::paintEngine () const
{
	return const_cast<QPaintEngine*>(m_engine);
}

void VipShapeDevice::setDrawPrimitives(int p)
{
	m_drawPrimitives = p;
}
int VipShapeDevice::drawPrimitives() const
{
	return m_drawPrimitives;
}

void VipShapeDevice::setDrawPrimitive(int p, bool enable)
{
	if (enable)
		m_drawPrimitives |= p;
	else
		m_drawPrimitives &= ~p;
}
bool VipShapeDevice::testDrawPrimitive(int p) const
{
	return m_drawPrimitives & p;
}

void VipShapeDevice::setExtractBoundingRectOnly(bool enable) {
	static_cast<PathEngine*>(m_engine)->m_extractBoundingRectOnly = enable;
}
bool VipShapeDevice::extractBoundingRectOnly() const{
	return static_cast<PathEngine*>(m_engine)->m_extractBoundingRectOnly;
}

int VipShapeDevice::metric(PaintDeviceMetric metric) const
{

	switch (metric) {
	case PdmWidth:
		return INT_MAX;
		break;

	case PdmHeight:
		return INT_MAX;
		break;

	case PdmWidthMM:
		return INT_MAX;
		break;

	case PdmHeightMM:
		return INT_MAX;
		break;

	case PdmNumColors:
		return 2;
		break;

	case PdmDepth:
		return 1;
		break;

	case PdmDpiX:
		return 300;
		break;

	case PdmDpiY:
		return 300;
		break;

	case PdmPhysicalDpiX:
		return 300;
		break;

	case PdmPhysicalDpiY:
		return 300;
		break;

	case PdmDevicePixelRatio:
		return 1;
		break;

	case PdmDevicePixelRatioScaled:
		return 1;
		break;

	default:
		qWarning("VipShapeDevice::metric(): Unhandled metric type %d", metric);
		break;
	}
	return 0;
}
