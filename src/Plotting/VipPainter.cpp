#include "VipPainter.h"
#include "VipColorMap.h"
#include "VipCoordinateSystem.h"
#include "VipAbstractScale.h"
#include "VipScaleDraw.h"
#include "VipText.h"
#include <qmath.h>
#include <qwindowdefs.h>
#include <qwidget.h>
#include <qframe.h>
#include <qrect.h>
#include <qpainter.h>
#include <qpalette.h>
#include <qpaintdevice.h>
#include <qpixmap.h>
#include <qstyle.h>
#include <qtextdocument.h>
#include <qabstracttextdocumentlayout.h>
#include <qstyleoption.h>
#include <qpaintengine.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <QVector2D>
#include <qdrawutil.h>

#if QT_VERSION >= 0x050000
#include <qwindow.h>
#endif

#if QT_VERSION < 0x050000

#ifdef Q_WS_X11
#include <qx11info_x11.h>
#endif

#endif

bool VipPainter::d_polylineSplitting = true;
bool VipPainter::d_roundingAlignment = true;


static inline bool vipIsClippingNeeded(
    const QPainter *painter, QRectF &clipRect )
{
    bool doClipping = false;
    const QPaintEngine *pe = painter->paintEngine();
    if ( pe && pe->type() == QPaintEngine::SVG )
    {
        // The SVG paint engine ignores any clipping,

        if ( painter->hasClipping() )
        {
            doClipping = true;
            clipRect = painter->clipRegion().boundingRect();
        }
    }

    return doClipping;
}

template <class T>
static inline void vipDrawPolyline( QPainter *painter,
    const T *points, int pointCount, bool polylineSplitting )
{
    bool doSplit = false;
    if ( polylineSplitting )
    {
        const QPaintEngine *pe = painter->paintEngine();
        if ( pe && pe->type() == QPaintEngine::Raster )
        {
            //  The raster paint engine seems to use some algo with O(n*n).
            //  ( Qt 4.3 is better than Qt 4.2, but remains unacceptable)
            //  To work around this problem, we have to split the polygon into
            //  smaller pieces.
            doSplit = true;
        }
    }

    if ( doSplit )
    {
        const int splitSize = 20;
        for ( int i = 0; i < pointCount; i += splitSize )
        {
            const int n = qMin( splitSize + 1, pointCount - i );
            painter->drawPolyline( points + i, n );
        }
    }
    else
        painter->drawPolyline( points, pointCount );
}

QSize VipPainter::screenResolution()
{
    static QSize screenResolution;
    if ( !screenResolution.isValid() )
    {
        QDesktopWidget *desktop = QApplication::desktop();
        if ( desktop )
        {
            screenResolution.setWidth( desktop->logicalDpiX() );
            screenResolution.setHeight( desktop->logicalDpiY() );
        }
    }

    return screenResolution;
}


static bool vipNeedUnscaledFont(QPainter* painter)
{
	if (painter->font().pixelSize() >= 0)
		return false;
	const QPaintDevice* pd = painter->device();
	if (!pd)
		return false;
	const QSize screenResolution = VipPainter::screenResolution();
	if (pd->logicalDpiX() != screenResolution.width() || pd->logicalDpiY() != screenResolution.height())
		return true;
	return false;
}
static void vipForceUnscaleFont(QPainter* painter) 
{
	QFont pixelFont(painter->font(), QApplication::desktop());
	pixelFont.setPixelSize(QFontInfo(pixelFont).pixelSize());
	painter->setFont(pixelFont);
}

static inline void vipUnscaleFont( QPainter *painter )
{
    if ( painter->font().pixelSize() >= 0 )
        return;

    const QSize screenResolution = VipPainter::screenResolution();

    const QPaintDevice *pd = painter->device();
    if (!pd)
        return;
    if ( pd->logicalDpiX() != screenResolution.width() ||
        pd->logicalDpiY() != screenResolution.height() )
    {
        QFont pixelFont( painter->font(), QApplication::desktop() );
        pixelFont.setPixelSize( QFontInfo( pixelFont ).pixelSize() );

        painter->setFont( pixelFont );
    }
}


bool VipPainter::isX11GraphicsSystem()
{
    static int onX11 = -1;
    if ( onX11 < 0 )
    {
        QPixmap pm( 1, 1 );
        QPainter painter( &pm );

        onX11 = ( painter.paintEngine()->type() == QPaintEngine::X11 ) ? 1 : 0;
    }

    return onX11 == 1;
}


bool VipPainter::isAligning( QPainter *painter )
{
    if ( painter && painter->isActive() )
    {
		if (isVectoriel(painter))
			return false;

        const QTransform tr = painter->transform();
        if ( tr.isRotating() || tr.isScaling() )
        {
            // we might have to check translations too
            return false;
        }
    }

    return true;
}

bool VipPainter::isOpenGL(QPainter *painter)
{
	return painter->paintEngine() &&
		(painter->paintEngine()->type() == QPaintEngine::OpenGL || painter->paintEngine()->type() == QPaintEngine::OpenGL2);
}
bool VipPainter::isVectoriel(QPainter *painter)
{
	return painter->paintEngine() &&
		(	painter->paintEngine()->type() == QPaintEngine::SVG ||
			painter->paintEngine()->type() == QPaintEngine::MacPrinter ||
			painter->paintEngine()->type() == QPaintEngine::Picture ||
			painter->paintEngine()->type() == QPaintEngine::Pdf ||
			painter->paintEngine()->type() == QPaintEngine::PostScript //||
			// painter->paintEngine()->type() == QPaintEngine::User
);
}

void VipPainter::setRoundingAlignment( bool enable )
{
    d_roundingAlignment = enable;
}


void VipPainter::setPolylineSplitting( bool enable )
{
    d_polylineSplitting = enable;
}

//! Wrapper for QPainter::drawPath()
void VipPainter::drawPath( QPainter *painter, const QPainterPath &path )
{
	//QT 5.6 crashes when drawinf text on a GL paint engine, so cash the text first in a QPixmap
	//if (painter->paintEngine()->type() == QPaintEngine::OpenGL || painter->paintEngine()->type() == QPaintEngine::OpenGL2)
	// {
	// painter->fillPath(path, painter->brush());
	// painter->save();
	// painter->setBrush(QBrush());
	// painter->drawPath(path);
	// painter->restore();
	// }
	// else
	 painter->drawPath( path );
}

void VipPainter::drawPath(  QPainter *painter, const QPainterPath &path , const QPolygonF &target)
{
	QVector2D vx(target[1].x()-target[0].x(),target[1].y()-target[0].y());
	QVector2D vy(target[3].x()-target[0].x(),target[3].y()-target[0].y());
	QPointF origin(target[0]);

	QRectF p_rect = path.boundingRect();

	vx /= p_rect.width();
	vy /= p_rect.height();

	QTransform tr = VipCoordinateSystem::changeCoordinateSystem(origin,vx,vy);

	QPainterPath p(path);
	p.translate( - p_rect.topLeft());

	painter->save();
	painter->setTransform(tr,true);
	drawPath(painter,p);
	painter->restore();
}

//! Wrapper for QPainter::drawRect()
void VipPainter::drawRect( QPainter *painter, double x, double y, double w, double h )
{
    drawRect( painter, QRectF( x, y, w, h ) );
}

//! Wrapper for QPainter::drawRect()
void VipPainter::drawRect( QPainter *painter, const QRectF &rect )
{
    const QRectF r = rect;

    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
    const bool rounding = roundingAlignment(painter);

    if ( deviceClipping )
    {
        if ( !clipRect.intersects( r ) )
            return;

        if ( !clipRect.contains( r ) )
        {
            fillRect( painter, r & clipRect, painter->brush() );

            painter->save();
            painter->setBrush( Qt::NoBrush );
            drawPolyline( painter, QPolygonF( r ) );
            painter->restore();

            return;
        }
    }

    if(rounding)
    {
    	QTransform tr = resetTransform(painter);
    	painter->drawRect( vipRound( r, tr ) );
    	painter->setTransform(tr);
    }
    else
    	painter->drawRect( r );
}

void VipPainter::drawRoundedRect( QPainter * painter, const QRectF &rect, double x_radius, double y_radius )
{
    const bool rounding = roundingAlignment(painter);

    if(rounding)
    {
    	QTransform tr = resetTransform(painter);
	    painter->drawRoundedRect(vipRound(rect, tr), x_radius, y_radius);
    	painter->setTransform(tr);
    }
    else
    	painter->drawRoundedRect( rect,x_radius,y_radius  );
}

//! Wrapper for QPainter::fillRect()
void VipPainter::fillRect( QPainter *painter,
    const QRectF &rect, const QBrush &brush )
{
    if ( !rect.isValid() )
        return;

    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
    const bool rounding = roundingAlignment(painter);

    // Performance of Qt4 is horrible for a non trivial brush. Without
    // clipping expect minutes or hours for repainting large rectangles
    // (might result from zooming)

    if ( deviceClipping )
        clipRect &= painter->window();
    else
        clipRect = painter->window();

    if ( painter->hasClipping() )
        clipRect &= painter->clipRegion().boundingRect();

    QRectF r = rect;
    if ( deviceClipping )
        r = r.intersected( clipRect );

    if ( r.isValid() )
    {
    	if(rounding)
    	{
    		QTransform tr = resetTransform(painter);
		    painter->fillRect(vipRound(r, tr), brush);
    		painter->setTransform(tr);
    	}
    	else
    		painter->fillRect( r, brush );
    }
}

//! Wrapper for QPainter::drawPie()
void VipPainter::drawPie( QPainter *painter, const QRectF &rect,
    int a, int alen )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
    const bool rounding = roundingAlignment(painter);
    if ( deviceClipping && !clipRect.contains( rect ) )
        return;

    if(rounding)
    {
    	QTransform tr = resetTransform(painter);
	    painter->drawPie(vipRound(rect, tr), a, alen);
    	painter->setTransform(tr);
    }
    else
    	painter->drawPie( rect, a, alen );
}

//! Wrapper for QPainter::drawEllipse()
void VipPainter::drawEllipse( QPainter *painter, const QRectF &rect )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
    const bool rounding = roundingAlignment(painter);

    if ( deviceClipping && !clipRect.contains( rect ) )
        return;

    if(rounding)
    {
    	QTransform tr = resetTransform(painter);
	    painter->drawEllipse(vipRound(rect, tr));
		painter->setTransform(tr);
    }
    else
    	painter->drawEllipse( rect );
}

//! Wrapper for QPainter::drawText()
void VipPainter::drawText( QPainter *painter, double x, double y,
        const QString &text )
{
    drawText( painter, QPointF( x, y ), text );
}

//! Wrapper for QPainter::drawText()
void VipPainter::drawText( QPainter *painter, const QPointF &pos,
        const QString &text )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    if ( deviceClipping && !clipRect.contains( pos ) )
        return;

    const bool unscaled_font = vipNeedUnscaledFont(painter);
    if (unscaled_font) {
	    painter->save();
	    vipForceUnscaleFont(painter);
    }
    painter->drawText(pos, text);
    if (unscaled_font)
	    painter->restore();
    /* painter->save();
    vipUnscaleFont( painter );
    painter->drawText( pos, text );
    painter->restore();*/
}

//! Wrapper for QPainter::drawText()
void VipPainter::drawText( QPainter *painter,
    double x, double y, double w, double h,
    int flags, const QString &text )
{
    drawText( painter, QRectF( x, y, w, h ), flags, text );
}

//! Wrapper for QPainter::drawText()
void VipPainter::drawText( QPainter *painter, const QRectF &rect,
        int flags, const QString &text )
{
	const bool unscaled_font = vipNeedUnscaledFont(painter);
	if (unscaled_font) {
		painter->save();
		vipForceUnscaleFont(painter);
	}
	//painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::HighQualityAntialiasing | QPainter::TextAntialiasing);
	painter->drawText(rect, flags, text);
	if (unscaled_font)
	    painter->restore();

	/* if (vipNeedUnscaledFont()) 
    painter->save();
    vipUnscaleFont( painter );
    painter->drawText( rect, flags, text );
    painter->restore();*/
}

#ifndef QT_NO_RICHTEXT


void VipPainter::drawSimpleRichText( QPainter *painter, const QRectF &rect,
    int flags, const QTextDocument &text )
{
    QTextDocument *txt = text.clone();

    painter->save();

    QRectF unscaledRect = rect;

    if ( painter->font().pixelSize() < 0 )
    {
	    const QSize res = screenResolution();

        const QPaintDevice *pd = painter->device();
        if ( pd->logicalDpiX() != res.width() ||
            pd->logicalDpiY() != res.height() )
        {
            QTransform transform;
            transform.scale( res.width() / double( pd->logicalDpiX() ),
                res.height() / double( pd->logicalDpiY() ));

            painter->setWorldTransform( transform, true );
            unscaledRect = transform.inverted().mapRect(rect);
        }
    }

    txt->setDefaultFont( painter->font() );
    txt->setPageSize( QSizeF( unscaledRect.width(), QWIDGETSIZE_MAX ) );

    QAbstractTextDocumentLayout* layout = txt->documentLayout();

    const double height = layout->documentSize().height();
    double y = unscaledRect.y();
    if ( flags & Qt::AlignBottom )
        y += ( unscaledRect.height() - height );
    else if ( flags & Qt::AlignVCenter )
        y += ( unscaledRect.height() - height ) / 2;

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor( QPalette::Text, painter->pen().color() );

    painter->translate( unscaledRect.x(), y );
    layout->draw( painter, context );

    painter->restore();
    delete txt;
}

#endif // !QT_NO_RICHTEXT


void drawArcText ( QPainter * //painter
, const QRectF & //rect
, double //start_angle
, double //sweep_length
, int //flags
, const QString & //hw
)
{
	//int drawWidth = width() / 100;
	// QPainter painter(this);
	// QPen pen = painter.pen();
	// pen.setWidth(drawWidth);
	// pen.setColor(Qt::darkGreen);
	// painter.setPen(pen);
//
	// QPainterPath path(QPointF(0.0, 0.0));
//
	// QPointF c1(width()*0.2,height()*0.8);
	// QPointF c2(width()*0.8,height()*0.2);
//
	// path.cubicTo(c1,c2,QPointF(width(),height()));
//
	// //draw the bezier curve
	// painter.drawPath(path);
//
	// //Make the painter ready to draw chars
	// QFont font = painter.font();
	// font.setPixelSize(drawWidth*2);
	// painter.setFont(font);
	// pen.setColor(Qt::red);
	// painter.setPen(pen);
//
	// qreal percentIncrease = (qreal) 1/(hw.size()+1);
	// qreal percent = 0;
//
	// for ( int i = 0; i < hw.size(); i++ ) {
	// percent += percentIncrease;
//
	// QPointF point = path.pointAtPercent(percent);
	// qreal angle = path.angleAtPercent(percent);   // Clockwise is negative
//
	// painter.save();
	// // Move the virtual origin to the point on the curve
	// painter.translate(point);
	// // Rotate to match the angle of the curve
	// // Clockwise is positive so we negate the angle from above
	// painter.rotate(-angle);
	// // Draw a line width above the origin to move the text above the line
	// // and let Qt do the transformations
	// painter.drawText(QPoint(0, -pen.width()),QString(hw[i]));
	// painter.restore();
	// }
}

void VipPainter::drawLineRounded(QPainter * painter, const QPointF &p1, const QPointF &p2)
{
	QTransform tr = resetTransform(painter);
	painter->drawLine(vipRound(p1, tr), vipRound(p2, tr));
	painter->setTransform(tr);
}

//! Wrapper for QPainter::drawLine()
void VipPainter::drawLine( QPainter *painter,
    const QPointF &p1, const QPointF &p2 )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    if ( deviceClipping &&
        !( clipRect.contains( p1 ) && clipRect.contains( p2 ) ) )
    {
        QPolygonF polygon;
        polygon += p1;
        polygon += p2;
        drawPolyline( painter, polygon );
        return;
    }
    else
    {
    	const bool rounding = roundingAlignment(painter);
    	if(rounding)
    	{
			drawLineRounded(painter, p1, p2);
    	}
    	else
    		painter->drawLine( p1, p2 );
    }


}

//! Wrapper for QPainter::drawPolygon()
void VipPainter::drawPolygon( QPainter *painter, const QPolygonF &polygon )
{
    QRectF clipRect;
    //const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    QPolygonF cpa = polygon;
    //if ( deviceClipping )
    //  cpa = QwtClipper::clipPolygonF( clipRect, polygon );

    const bool rounding = roundingAlignment(painter);
    if(rounding)
    {
    	QTransform tr = resetTransform(painter);
	    QPolygonF p = vipRound(cpa, tr);
    	painter->drawPolygon( p );
		painter->setTransform(tr);
    }
    else
    	painter->drawPolygon( cpa );
}

//! Wrapper for QPainter::drawPolyline()
void VipPainter::drawPolyline( QPainter *painter, const QPolygonF &polygon )
{
    QRectF clipRect;
    //const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    QPolygonF cpa = polygon;
    //if ( deviceClipping )
    //  cpa = QwtClipper::clipPolygonF( clipRect, cpa );

    const bool rounding = roundingAlignment(painter);

    if(rounding)
	{
		QTransform tr = resetTransform(painter);
	    QPolygonF p = vipRound(cpa, tr);
		vipDrawPolyline<QPointF>( painter,p.constData(), p.size(), d_polylineSplitting );
		painter->setTransform(tr);
	}
	else
	{
		vipDrawPolyline<QPointF>( painter, cpa.constData(), cpa.size(), d_polylineSplitting );
	}

}

//! Wrapper for QPainter::drawPolyline()
void VipPainter::drawPolyline( QPainter *painter,
    const QPointF *points, int pointCount )
{
    //QRectF clipRect;
    // const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
//
    // if ( deviceClipping )
    // {
    //  QPolygonF polygon( pointCount );
    //  ::memcpy( polygon.data(), points, pointCount * sizeof( QPointF ) );
//
    //  polygon = QwtClipper::clipPolygonF( clipRect, polygon );
    //  vipDrawPolyline<QPointF>( painter,
    //      polygon.constData(), polygon.size(), d_polylineSplitting );
    // }
    // else
    // {
    //  vipDrawPolyline<QPointF>( painter, points, pointCount, d_polylineSplitting );
    // }
	const bool rounding = roundingAlignment(painter);

	if(rounding)
	{
		QTransform tr = resetTransform(painter);
		QPolygonF polygon( pointCount );
		::memcpy( polygon.data(), points, pointCount * sizeof( QPointF ) );
		polygon = vipRound(polygon, tr);
		vipDrawPolyline<QPointF>( painter,polygon.constData(), polygon.size(), d_polylineSplitting );
		painter->setTransform(tr);
	}
	else
	{
		vipDrawPolyline<QPointF>( painter, points, pointCount, d_polylineSplitting );
	}
}

//! Wrapper for QPainter::drawPolygon()
void VipPainter::drawPolygon( QPainter *painter, const QPolygon &polygon )
{
    QRectF clipRect;
    //const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    QPolygon cpa = polygon;
    //if ( deviceClipping )
    //  cpa = QwtClipper::clipPolygon( clipRect, polygon );

    painter->drawPolygon( cpa );
}

//! Wrapper for QPainter::drawPolyline()
void VipPainter::drawPolyline( QPainter *painter, const QPolygon &polygon )
{
    //QRectF clipRect;
    // const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
//
    // QPolygon cpa = polygon;
    // if ( deviceClipping )
    //  cpa = QwtClipper::clipPolygon( clipRect, cpa );

    vipDrawPolyline<QPoint>( painter,
    	polygon.constData(), polygon.size(), d_polylineSplitting );
}

//! Wrapper for QPainter::drawPolyline()
void VipPainter::drawPolyline( QPainter *painter,
    const QPoint *points, int pointCount )
{
    //QRectF clipRect;
    // const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
//
    // if ( deviceClipping )
    // {
    //  QPolygon polygon( pointCount );
    //  ::memcpy( polygon.data(), points, pointCount * sizeof( QPoint ) );
//
    //  polygon = QwtClipper::clipPolygon( clipRect, polygon );
    //  vipDrawPolyline<QPoint>( painter,
    //      polygon.constData(), polygon.size(), d_polylineSplitting );
    // }
    // else
        vipDrawPolyline<QPoint>( painter, points, pointCount, d_polylineSplitting );
}

//! Wrapper for QPainter::drawPoint()
void VipPainter::drawPoint( QPainter *painter, const QPointF &pos )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
    const bool rounding = roundingAlignment(painter);

    if ( deviceClipping && !clipRect.contains( pos ) )
        return;

    if(rounding)
    {
    	QTransform tr = resetTransform(painter);
	    painter->drawPoint(vipRound(pos));
    	painter->setTransform(tr);
    }
    else
    	painter->drawPoint( pos );
}

//! Wrapper for QPainter::drawPoint()
void VipPainter::drawPoint( QPainter *painter, const QPoint &pos )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    if ( deviceClipping )
    {
        const int minX = qCeil( clipRect.left() );
        const int maxX = qFloor( clipRect.right() );
        const int minY = qCeil( clipRect.top() );
        const int maxY = qFloor( clipRect.bottom() );

        if ( pos.x() < minX || pos.x() > maxX
            || pos.y() < minY || pos.y() > maxY )
        {
            return;
        }
    }

    painter->drawPoint( pos );
}

//! Wrapper for QPainter::drawPoints()
void VipPainter::drawPoints( QPainter *painter,
    const QPoint *points, int pointCount )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );

    if ( deviceClipping )
    {
        const int minX = qCeil( clipRect.left() );
        const int maxX = qFloor( clipRect.right() );
        const int minY = qCeil( clipRect.top() );
        const int maxY = qFloor( clipRect.bottom() );

        const QRect r( minX, minY, maxX - minX, maxY - minY );

        QPolygon clippedPolygon( pointCount );
        QPoint *clippedData = clippedPolygon.data();

        int numClippedPoints = 0;
        for ( int i = 0; i < pointCount; i++ )
        {
            if ( r.contains( points[i] ) )
                clippedData[ numClippedPoints++ ] = points[i];
        }
        painter->drawPoints( clippedData, numClippedPoints );
    }
    else
    {
        painter->drawPoints( points, pointCount );
    }
}

//! Wrapper for QPainter::drawPoints()
void VipPainter::drawPoints( QPainter *painter,
    const QPointF *points, int pointCount )
{
    QRectF clipRect;
    const bool deviceClipping = vipIsClippingNeeded( painter, clipRect );
    const bool rounding = roundingAlignment(painter);

    if ( deviceClipping )
    {
        QPolygonF clippedPolygon( pointCount );
        QPointF *clippedData = clippedPolygon.data();

        int numClippedPoints = 0;
        for ( int i = 0; i < pointCount; i++ )
        {
            if ( clipRect.contains( points[i] ) )
                clippedData[ numClippedPoints++ ] = points[i];
        }

        if(rounding)
        {
        	QTransform tr = resetTransform(painter);
		    clippedPolygon = vipRound(clippedPolygon, tr);
        	painter->drawPoints( clippedPolygon.constData(), clippedPolygon.size() );
        	painter->setTransform(tr);
        }
        else
        {
        	painter->drawPoints( clippedData, numClippedPoints );
        }
    }
    else
    {
    	if(rounding)
		{
			QTransform tr = resetTransform(painter);
		    QPolygonF polygon = vipRound(points, pointCount, tr);
			painter->drawPoints( polygon.constData(), polygon.size() );
			painter->setTransform(tr);
		}
		else
		{
			 painter->drawPoints( points, pointCount );
		}

    }
}

//! Wrapper for QPainter::drawImage()
void VipPainter::drawImage( QPainter *painter,
    const QRectF &rect, const QImage &image )
{
    const QRect alignedRect = rect.toAlignedRect();

    if ( alignedRect != rect )
    {
        const QRectF clipRect = rect.adjusted( 0.0, 0.0, -1.0, -1.0 );

        painter->save();
        painter->setClipRect( clipRect, Qt::IntersectClip );
        painter->drawImage( alignedRect, image );
        painter->restore();
    }
    else
    {
        painter->drawImage( alignedRect, image );
    }
}

//! Wrapper for QPainter::drawPixmap()
void VipPainter::drawPixmap( QPainter *painter,
    const QRectF &rect, const QPixmap &pixmap )
{
    const QRect alignedRect = rect.toAlignedRect();

    if ( alignedRect != rect )
    {
        const QRectF clipRect = rect.adjusted( 0.0, 0.0, -1.0, -1.0 );

        painter->save();
        painter->setClipRect( clipRect, Qt::IntersectClip );
        painter->drawPixmap( alignedRect, pixmap );
        painter->restore();
    }
    else
    {
        painter->drawPixmap( alignedRect, pixmap );
    }
}

void VipPainter::drawPixmap( QPainter * painter, const QPolygonF & target, const QPixmap & pixmap, const QRectF & src )
{
	QVector2D vx(target[1].x()-target[0].x(),target[1].y()-target[0].y());
	QVector2D vy(target[3].x()-target[0].x(),target[3].y()-target[0].y());
	QPointF origin(target[0]);

	vx /= pixmap.width();
	vy /= pixmap.height();

	QTransform tr = VipCoordinateSystem::changeCoordinateSystem(origin,vx,vy);
	QRectF dst_rect = tr.inverted().map(target).boundingRect();

	painter->save();
	painter->setTransform(tr,true);
	painter->drawPixmap(dst_rect,pixmap,src);
	painter->restore();
}

void VipPainter::drawImage( QPainter * painter, const QPolygonF & target, const QImage & image, const QRectF & src )
{
	QVector2D vx(target[1].x()-target[0].x(),target[1].y()-target[0].y());
	QVector2D vy(target[3].x()-target[0].x(),target[3].y()-target[0].y());
	QPointF origin(target[0]);

	vx /= image.width();
	vy /= image.height();

	QTransform tr = VipCoordinateSystem::changeCoordinateSystem(origin,vx,vy);
	QRectF dst_rect = tr.inverted().map(target).boundingRect();

	painter->save();
	if(!tr.isIdentity())
		painter->setTransform(tr,true);
	painter->drawImage(dst_rect,image,src);
	painter->restore();
}


void VipPainter::drawHandle( QPainter *painter, const QRectF &hRect, Qt::Orientation orientation, const QPalette & palette, int borderWidth )
{
	const int bw = borderWidth;

	QRect handleRect = hRect.toRect();

	qDrawShadePanel( painter,
		handleRect, palette, false, bw,
		&palette.brush( QPalette::Button ) );

	if ( orientation == Qt::Horizontal )
	{
		int pos = (handleRect.center().x()) +1;
		qDrawShadeLine( painter, pos, handleRect.top() + bw,
				pos, handleRect.bottom() - bw, palette, true, 1 );
	}
	else // Vertical
	{
		int pos = (handleRect.center().y()) +1;
		qDrawShadeLine( painter, handleRect.left() + bw, pos,
			handleRect.right() - bw, pos, palette, true, 1 );
	}
}

void VipPainter::drawGrip( QPainter *painter, const QRectF &hRect)
{
	QRectF r(0,0,19,13);

	QColor c1(119,136,146);
	QColor c2(181,196,205);

	QPolygonF contour;
	contour<< QPointF(2,0) << QPointF(12,0)<< QPointF(18,6)<< QPointF(12,12)<< QPointF(2,12)<< QPointF(0,10)<<QPointF(0,2);
	contour = QTransform().scale(hRect.width()/r.width(),hRect.height()/r.height()).map(contour);
	contour.translate(hRect.topLeft());

	QLinearGradient gradient(QPointF(11,0), QPointF(11,10));
	gradient.setColorAt(0.,c1);
	gradient.setColorAt(1.,c2);

	painter->setBrush(QBrush(gradient));
	painter->setPen(Qt::NoPen);
	painter->drawPolygon(contour);

}

//! Draw a focus rectangle on a widget using its style.
void VipPainter::drawFocusRect( QPainter *painter, const QWidget *widget )
{
    drawFocusRect( painter, widget, widget->rect() );
}

//! Draw a focus rectangle on a widget using its style.
void VipPainter::drawFocusRect( QPainter *painter, const QWidget *widget,
    const QRect &rect )
{
    QStyleOptionFocusRect opt;
    opt.init( widget );
    opt.rect = rect;
    opt.state |= QStyle::State_HasFocus;

    widget->style()->drawPrimitive( QStyle::PE_FrameFocusRect,
        &opt, painter, widget );
}


void VipPainter::drawRoundFrame( QPainter *painter,
    const QRectF &rect, const QPalette &palette,
    int lineWidth, int frameStyle )
{
    enum Style
    {
        Plain,
        Sunken,
        Raised
    };

    Style style = Plain;
    if ( (frameStyle & QFrame::Sunken) == QFrame::Sunken )
        style = Sunken;
    else if ( (frameStyle & QFrame::Raised) == QFrame::Raised )
        style = Raised;

    const double lw2 = 0.5 * lineWidth;
    QRectF r = rect.adjusted( lw2, lw2, -lw2, -lw2 );

    QBrush brush;

    if ( style != Plain )
    {
        QColor c1 = palette.color( QPalette::Light );
        QColor c2 = palette.color( QPalette::Dark );

        if ( style == Sunken )
            qSwap( c1, c2 );

        QLinearGradient gradient( r.topLeft(), r.bottomRight() );
        gradient.setColorAt( 0.0, c1 );
#if 0
        gradient.setColorAt( 0.3, c1 );
        gradient.setColorAt( 0.7, c2 );
#endif
        gradient.setColorAt( 1.0, c2 );

        brush = QBrush( gradient );
    }
    else // Plain
    {
        brush = palette.brush( QPalette::WindowText );
    }

    painter->save();

    painter->setPen( QPen( brush, lineWidth ) );
    painter->setBrush( Qt::NoBrush );

    painter->drawEllipse( r );

    painter->restore();
}

/// Draw a rectangular frame
///
/// \param painter VipPainter
/// \param rect Frame rectangle
/// \param palette Palette
/// \param foregroundRole Foreground role used for QFrame::Plain
/// \param frameWidth Frame width
/// \param midLineWidth Used for QFrame::Box
/// \param frameStyle bitwise OR\B4ed value of QFrame::VipShape and QFrame::Shadow
void VipPainter::drawFrame( QPainter *painter, const QRectF &rect,
    const QPalette &palette, QPalette::ColorRole foregroundRole,
    int frameWidth, int midLineWidth, int frameStyle )
{
    if ( frameWidth <= 0 || rect.isEmpty() )
        return;

    const int shadow = frameStyle & QFrame::Shadow_Mask;

    painter->save();

    if ( shadow == QFrame::Plain )
    {
        const QRectF outerRect = rect.adjusted( 0.0, 0.0, -1.0, -1.0 );
        const QRectF innerRect = outerRect.adjusted(
            frameWidth, frameWidth, -frameWidth, -frameWidth );

        QPainterPath path;
        path.addRect( outerRect );
        path.addRect( innerRect );

        painter->setPen( Qt::NoPen );
        painter->setBrush( palette.color( foregroundRole ) );

        painter->drawPath( path );
    }
    else
    {
        const int shape = frameStyle & QFrame::Shape_Mask;

        if ( shape == QFrame::Box )
        {
            const QRectF outerRect = rect.adjusted( 0.0, 0.0, -1.0, -1.0 );
            const QRectF midRect1 = outerRect.adjusted(
                frameWidth, frameWidth, -frameWidth, -frameWidth );
            const QRectF midRect2 = midRect1.adjusted(
                midLineWidth, midLineWidth, -midLineWidth, -midLineWidth );

            const QRectF innerRect = midRect2.adjusted(
                frameWidth, frameWidth, -frameWidth, -frameWidth );

            QPainterPath path1;
            path1.moveTo( outerRect.bottomLeft() );
            path1.lineTo( outerRect.topLeft() );
            path1.lineTo( outerRect.topRight() );
            path1.lineTo( midRect1.topRight() );
            path1.lineTo( midRect1.topLeft() );
            path1.lineTo( midRect1.bottomLeft() );

            QPainterPath path2;
            path2.moveTo( outerRect.bottomLeft() );
            path2.lineTo( outerRect.bottomRight() );
            path2.lineTo( outerRect.topRight() );
            path2.lineTo( midRect1.topRight() );
            path2.lineTo( midRect1.bottomRight() );
            path2.lineTo( midRect1.bottomLeft() );

            QPainterPath path3;
            path3.moveTo( midRect2.bottomLeft() );
            path3.lineTo( midRect2.topLeft() );
            path3.lineTo( midRect2.topRight() );
            path3.lineTo( innerRect.topRight() );
            path3.lineTo( innerRect.topLeft() );
            path3.lineTo( innerRect.bottomLeft() );

            QPainterPath path4;
            path4.moveTo( midRect2.bottomLeft() );
            path4.lineTo( midRect2.bottomRight() );
            path4.lineTo( midRect2.topRight() );
            path4.lineTo( innerRect.topRight() );
            path4.lineTo( innerRect.bottomRight() );
            path4.lineTo( innerRect.bottomLeft() );

            QPainterPath path5;
            path5.addRect( midRect1 );
            path5.addRect( midRect2 );

            painter->setPen( Qt::NoPen );

            QBrush brush1 = palette.dark().color();
            QBrush brush2 = palette.light().color();

            if ( shadow == QFrame::Raised )
                qSwap( brush1, brush2 );

            painter->setBrush( brush1 );
            painter->drawPath( path1 );
            painter->drawPath( path4 );

            painter->setBrush( brush2 );
            painter->drawPath( path2 );
            painter->drawPath( path3 );

            painter->setBrush( palette.mid() );
            painter->drawPath( path5 );
        }
#if 0
        // qDrawWinPanel doesn't result in something nice
        // on a scalable document like PDF. Better draw a
        // Panel.

        else if ( shape == QFrame::WinPanel )
        {
            painter->setRenderHint( QPainter::NonCosmeticDefaultPen, true );
            qDrawWinPanel ( painter, rect.toRect(), palette,
                frameStyle & QFrame::Sunken );
        }
        else if ( shape == QFrame::StyledPanel )
        {
        }
#endif
        else
        {
            const QRectF outerRect = rect.adjusted( 0.0, 0.0, -1.0, -1.0 );
            const QRectF innerRect = outerRect.adjusted(
                frameWidth - 1.0, frameWidth - 1.0,
                -( frameWidth - 1.0 ), -( frameWidth - 1.0 ) );

            QPainterPath path1;
            path1.moveTo( outerRect.bottomLeft() );
            path1.lineTo( outerRect.topLeft() );
            path1.lineTo( outerRect.topRight() );
            path1.lineTo( innerRect.topRight() );
            path1.lineTo( innerRect.topLeft() );
            path1.lineTo( innerRect.bottomLeft() );


            QPainterPath path2;
            path2.moveTo( outerRect.bottomLeft() );
            path2.lineTo( outerRect.bottomRight() );
            path2.lineTo( outerRect.topRight() );
            path2.lineTo( innerRect.topRight() );
            path2.lineTo( innerRect.bottomRight() );
            path2.lineTo( innerRect.bottomLeft() );

            painter->setPen( Qt::NoPen );

            QBrush brush1 = palette.dark().color();
            QBrush brush2 = palette.light().color();

            if ( shadow == QFrame::Raised )
                qSwap( brush1, brush2 );

            painter->setBrush( brush1 );
            painter->drawPath( path1 );

            painter->setBrush( brush2 );
            painter->drawPath( path2 );
        }

    }

    painter->restore();
}

/// Draw a rectangular frame with rounded borders
///
/// \param painter VipPainter
/// \param rect Frame rectangle
/// \param xRadius x-radius of the ellipses defining the corners
/// \param yRadius y-radius of the ellipses defining the corners
/// \param palette QPalette::WindowText is used for plain borders
///              QPalette::Dark and QPalette::Light for raised
///              or sunken borders
/// \param lineWidth Line width
/// \param frameStyle bitwise OR\B4ed value of QFrame::VipShape and QFrame::Shadow

void VipPainter::drawRoundedFrame( QPainter *painter,
    const QRectF &rect, double xRadius, double yRadius,
    const QPalette &palette, int lineWidth, int frameStyle )
{
    painter->save();
    painter->setRenderHint( QPainter::Antialiasing, true );
    painter->setBrush( Qt::NoBrush );

    double lw2 = lineWidth * 0.5;
    QRectF r = rect.adjusted( lw2, lw2, -lw2, -lw2 );

    QPainterPath path;
    path.addRoundedRect( r, xRadius, yRadius );

    enum Style
    {
        Plain,
        Sunken,
        Raised
    };

    Style style = Plain;
    if ( (frameStyle & QFrame::Sunken) == QFrame::Sunken )
        style = Sunken;
    else if ( (frameStyle & QFrame::Raised) == QFrame::Raised )
        style = Raised;

    if ( style != Plain && path.elementCount() == 17 )
    {
        // move + 4 * ( cubicTo + lineTo )
        QPainterPath pathList[8];

        for ( int i = 0; i < 4; i++ )
        {
            const int j = i * 4 + 1;

            pathList[ 2 * i ].moveTo(
                path.elementAt(j - 1).x, path.elementAt( j - 1 ).y
            );

            pathList[ 2 * i ].cubicTo(
                path.elementAt(j + 0).x, path.elementAt(j + 0).y,
                path.elementAt(j + 1).x, path.elementAt(j + 1).y,
                path.elementAt(j + 2).x, path.elementAt(j + 2).y );

            pathList[ 2 * i + 1 ].moveTo(
                path.elementAt(j + 2).x, path.elementAt(j + 2).y
            );
            pathList[ 2 * i + 1 ].lineTo(
                path.elementAt(j + 3).x, path.elementAt(j + 3).y
            );
        }

        QColor c1( palette.color( QPalette::Dark ) );
        QColor c2( palette.color( QPalette::Light ) );

        if ( style == Raised )
            qSwap( c1, c2 );

        for ( int i = 0; i < 4; i++ )
        {
            QRectF _r = pathList[2 * i].controlPointRect();

            QPen arcPen;
            arcPen.setCapStyle( Qt::FlatCap );
            arcPen.setWidth( lineWidth );

            QPen linePen;
            linePen.setCapStyle( Qt::FlatCap );
            linePen.setWidth( lineWidth );

            switch( i )
            {
                case 0:
                {
                    arcPen.setColor( c1 );
                    linePen.setColor( c1 );
                    break;
                }
                case 1:
                {
                    QLinearGradient gradient;
                    gradient.setStart( _r.topLeft() );
                    gradient.setFinalStop( _r.bottomRight() );
                    gradient.setColorAt( 0.0, c1 );
                    gradient.setColorAt( 1.0, c2 );

                    arcPen.setBrush( gradient );
                    linePen.setColor( c2 );
                    break;
                }
                case 2:
                {
                    arcPen.setColor( c2 );
                    linePen.setColor( c2 );
                    break;
                }
                case 3:
                {
                    QLinearGradient gradient;

                    gradient.setStart( _r.bottomRight() );
                    gradient.setFinalStop( _r.topLeft() );
                    gradient.setColorAt( 0.0, c2 );
                    gradient.setColorAt( 1.0, c1 );

                    arcPen.setBrush( gradient );
                    linePen.setColor( c1 );
                    break;
                }
            }


            painter->setPen( arcPen );
            painter->drawPath( pathList[ 2 * i] );

            painter->setPen( linePen );
            painter->drawPath( pathList[ 2 * i + 1] );
        }
    }
    else
    {
        QPen pen( palette.color( QPalette::WindowText ), lineWidth );
        painter->setPen( pen );
        painter->drawPath( path );
    }

    painter->restore();
}

/// Draw a color bar into a rectangle
///
/// \param painter VipPainter
/// \param colorMap Color map
/// \param interval Value range
/// \param scaleMap Scale map
/// \param orientation Orientation
/// \param rect Traget rectangle
void VipPainter::drawColorBar( QPainter *painter,
         VipColorMap &colorMap, const VipInterval &interval,
        const VipScaleMap &scaleMap, Qt::Orientation orientation,
        const QRectF &rect , QPixmap * pixmap)
{
	colorMap.startDraw();

    QVector<QRgb> colorTable;
    if ( colorMap.format() == VipColorMap::Indexed )
        colorTable = colorMap.colorTable( interval );

    QColor c;

    const QRect devRect = rect.toAlignedRect();

    // We paint to a pixmap first to have something scalable for printing
    // ( f.e. in a Pdf document )
	QPixmap pix;
	if(!pixmap) {
		pixmap = &pix;
	}
	if (pixmap->size() != devRect.size())
		*pixmap = QPixmap(devRect.size());
    pixmap->fill(Qt::transparent);
    QPainter pmPainter( pixmap );
    pmPainter.translate( -devRect.x(), -devRect.y() );

    if ( orientation == Qt::Horizontal )
    {
        VipScaleMap sMap = scaleMap;
        sMap.setPaintInterval( rect.left(), rect.right() );

        for ( int x = devRect.left(); x <= devRect.right(); x++ )
        {
            const double value = sMap.invTransform( x );

            if ( colorMap.format() == VipColorMap::RGB )
                c.setRgba( colorMap.rgb( interval, value ) );
            else
                c = colorTable[colorMap.colorIndex( interval, value )];

            pmPainter.setPen( c );
            pmPainter.drawLine( x, devRect.top(), x, devRect.bottom() );
        }
    }
    else // Vertical
    {
        VipScaleMap sMap = scaleMap;
        sMap.setPaintInterval( rect.bottom(), rect.top() );

        for ( int y = devRect.top(); y <= devRect.bottom(); y++ )
        {
            const double value = sMap.invTransform( y );

            if ( colorMap.format() == VipColorMap::RGB )
                c.setRgba( colorMap.rgb( interval, value ) );
            else
                c = colorTable[colorMap.colorIndex( interval, value )];

            pmPainter.setPen( c );
            pmPainter.drawLine( devRect.left(), y, devRect.right(), y );
        }
    }
    pmPainter.end();

	QRectF tmp = rect;
	//bool is_opengl = isOpenGL(painter);
	//bool is_vectoriel = isVectoriel(painter);
	//if (!is_opengl && !is_vectoriel)// !painter->paintEngine() || !(painter->paintEngine()->type() == QPaintEngine::OpenGL || painter->paintEngine()->type() == QPaintEngine::OpenGL2))
	//	tmp = rect.adjusted(0, 0, 1,1); //non opengl device

    drawPixmap( painter, tmp, *pixmap );

	colorMap.endDraw();
}

static inline void vipFillRect( const QWidget *widget, QPainter *painter,
    const QRect &rect, const QBrush &brush)
{
    if ( brush.style() == Qt::TexturePattern )
    {
        painter->save();

        painter->setClipRect( rect );
        painter->drawTiledPixmap(rect, brush.texture(), rect.topLeft());

        painter->restore();
    }
    else if ( brush.gradient() )
    {
        painter->save();

        painter->setClipRect( rect );
        painter->fillRect(0, 0, widget->width(),
            widget->height(), brush);

        painter->restore();
    }
    else
    {
        painter->fillRect(rect, brush);
    }
}

/// Fill a pixmap with the content of a widget
///
/// In Qt >= 5.0 QPixmap::fill() is a nop, in Qt 4.x it is buggy
/// for backgrounds with gradients. Thus fillPixmap() offers
/// an alternative implementation.
///
/// \param widget Widget
/// \param pixmap Pixmap to be filled
/// \param offset Offset
///
/// \sa QPixmap::fill()
void VipPainter::fillPixmap( const QWidget *widget,
    QPixmap &pixmap, const QPoint &offset )
{
    const QRect rect( offset, pixmap.size() );

    QPainter painter( &pixmap );
    painter.translate( -offset );

    const QBrush autoFillBrush =
        widget->palette().brush( widget->backgroundRole() );

    if ( !( widget->autoFillBackground() && autoFillBrush.isOpaque() ) )
    {
        const QBrush bg = widget->palette().brush( QPalette::Window );
        vipFillRect( widget, &painter, rect, bg);
    }

    if ( widget->autoFillBackground() )
        vipFillRect( widget, &painter, rect, autoFillBrush);

    if ( widget->testAttribute(Qt::WA_StyledBackground) )
    {
        painter.setClipRegion( rect );

        QStyleOption opt;
        opt.initFrom( widget );
        widget->style()->drawPrimitive( QStyle::PE_Widget,
            &opt, &painter, widget );
    }
}

/// Fill rect with the background of a widget
///
/// \param painter VipPainter
/// \param rect Rectangle to be filled
/// \param widget Widget
///
/// \sa QStyle::PE_Widget, QWidget::backgroundRole()
void VipPainter::drawBackgound( QPainter *painter,
    const QRectF &rect, const QWidget *widget )
{
    if ( widget->testAttribute( Qt::WA_StyledBackground ) )
    {
        QStyleOption opt;
        opt.initFrom( widget );
        opt.rect = rect.toAlignedRect();

        widget->style()->drawPrimitive(
            QStyle::PE_Widget, &opt, painter, widget);
    }
    else
    {
        const QBrush brush =
            widget->palette().brush( widget->backgroundRole() );

        painter->fillRect( rect, brush );
    }
}

/// \return A pixmap that can be used as backing store
///
/// \param widget Widget, for which the backinstore is intended
/// \param size Size of the pixmap
QPixmap VipPainter::backingStore( QWidget *widget, const QSize &size )
{
    QPixmap pm;

#define QWT_HIGH_DPI 1

#if QT_VERSION >= 0x050000 && QWT_HIGH_DPI
    qreal pixelRatio = 1.0;

    if ( widget && widget->windowHandle() )
    {
        pixelRatio = widget->windowHandle()->devicePixelRatio();
    }
    else
    {
        if ( qApp )
            pixelRatio = qApp->devicePixelRatio();
    }

    pm = QPixmap( size * pixelRatio );
    pm.setDevicePixelRatio( pixelRatio );
#else
    Q_UNUSED( widget )
    pm = QPixmap( size );
#endif

#if QT_VERSION < 0x050000
#ifdef Q_WS_X11
    if ( widget && isX11GraphicsSystem() )
    {
        if ( pm.x11Info().screen() != widget->x11Info().screen() )
            pm.x11SetScreen( widget->x11Info().screen() );
    }
#endif
#endif

    return pm;
}

void VipPainter::drawText(QPainter * painter, const VipCoordinateSystemPtr & m,
    		const VipText & t,
			double textRotation,
			Vip::RegionPositions textPosition,
			Qt::Alignment textAlignment,
			vip_double xLeft, vip_double xRight, vip_double yTop, vip_double yBottom )
{

		QLineF vertical(QPointF((xLeft+xRight)/2,(yTop+yBottom)/2), QPointF((xLeft+xRight)/2, yTop));
		QLineF horizontal(QPointF((xLeft+xRight)/2,(yTop+yBottom)/2), QPointF( xRight, (yTop+yBottom)/2));

		double angle = textRotation + m->axes()[0]->constScaleDraw()->angle(vertical.p1().x());
		QTransform text_tr;
		text_tr.rotate(angle);
		QPolygonF textPolygon = text_tr.map(t.textRect());
		QRectF textRect = textPolygon.boundingRect();
		QPointF textOffset = textRect.topLeft() - textPolygon[0];
		textRect = m->invTransform(textRect).boundingRect();

		//compute text x and y vipDistance in scale coordinates
		VipPoint dist = m->invTransform(QPointF(10,10)) - m->invTransform(QPointF(0,0));

		//compute text center boundaries
		if(textPosition & Vip::XInside)
		{
			horizontal.setLength(qMax((vip_double)0., horizontal.length() - textRect.width()/2 - qAbs(dist.x())));
		}
		else if(textPosition & Vip::XAutomatic)
		{
			const double len = horizontal.length();
			if(len- qAbs(dist.x()) >=  textRect.width()/2)
				horizontal.setLength(len - textRect.width()/2 - qAbs(dist.x()));
			else
				horizontal.setLength(len + textRect.width()/2 + qAbs(dist.x()));
		}
		else
		{
			horizontal.setLength(horizontal.length() + textRect.width()/2 + qAbs(dist.x()));
		}

		if(textPosition & Vip::YInside)
		{
			vertical.setLength(qMax((vip_double)0., vertical.length() - textRect.height()/2 - qAbs(dist.y())));
		}
		else if(textPosition & Vip::YAutomatic)
		{
			const double len = vertical.length();
			if(len- qAbs(dist.y()) >=  textRect.height()/2)
				vertical.setLength(len - textRect.height()/2 - qAbs(dist.y()));
			else
				vertical.setLength(len + textRect.height()/2 + qAbs(dist.y()));
		}
		else
		{
			vertical.setLength(vertical.length() + textRect.height()/2 + qAbs(dist.y()));
		}

		//compute text center position based on alignment
		QPointF pos = horizontal.p1();

		if(textAlignment & Qt::AlignLeft)
			pos.setX(horizontal.p1().x() - horizontal.dx() );
		else if(textAlignment & Qt::AlignRight)
			pos.setX(horizontal.p2().x());


		//TEST: inverted the 2 conditions
		if(textAlignment & Qt::AlignTop)
			pos.setY(vertical.p1().y() - vertical.dy());
		else if(textAlignment & Qt::AlignBottom)
			pos.setY(vertical.p2().y());


		pos = m->transform(pos);

		textRect = QPolygonF(m->transform(textRect)).boundingRect();
		pos -= textOffset + QPointF(textRect.width()/2,textRect.height()/2);

		if (textPosition & Vip::XInside) {
			//make sure text is inside boundaries
			if (textAlignment & Qt::AlignLeft)
				pos.rx() += (int)(t.textSize().width() / 2.0 + 0.5);
			else if (textAlignment & Qt::AlignRight)
				pos.rx() -= (int)(t.textSize().width() / 2.0 + 0.5);
		}
		if (textPosition & Vip::YInside) {
			//make sure text is inside boundaries
			if (textAlignment & Qt::AlignTop)
				pos.ry() += (int)(t.textSize().height() / 2.0 + t.borderPen().width()/2.0 + 0.5);
			else if (textAlignment & Qt::AlignBottom)
				pos.ry() -= (int)(t.textSize().height() / 2.0 + t.borderPen().width() / 2.0 + 0.5);
		}

		//textRect = t.textRect();
		// textRect.moveTopLeft(pos);
		// t.draw(painter, textRect);

		painter->save();

		QTransform tr;
		tr.translate(pos.x(),pos.y());
		tr.rotate(angle);
		painter->setTransform(tr,true);
		t.draw(painter,t.textRect());
		painter->restore();
}




void VipPainter::drawText(QPainter* painter, const VipText& t, const QTransform & text_tr, const QPointF & ref_pos, double textDistance, Vip::RegionPositions textPosition, Qt::Alignment textAlignment, const QRectF& geometry)
{
	
	QRectF textRect = t.textRect();

    double textDistance2 = textDistance * 2;
	if (textPosition & Vip::XAutomatic) {
		textPosition = textPosition & (~Vip::XAutomatic);
		if (textRect.width() + textDistance2 < geometry.width())
			textPosition |= Vip::XInside;
	}
	if (textPosition & Vip::YAutomatic) {
		textPosition = textPosition & (~Vip::YAutomatic);
		if (textRect.height() + textDistance2 < geometry.height())
			textPosition |= Vip::YInside;
	}

    double xDistance = textDistance;
	double yDistance = textDistance;
    if (textDistance2 > (geometry.width() - textRect.width())) {
		xDistance = (geometry.width() - textRect.width()) / 2;
	    if (xDistance < 0)
		    xDistance = 0;
    }
    if (textDistance2 > (geometry.height() - textRect.height())) {
	    yDistance = (geometry.height() - textRect.height()) / 2;
	    if (yDistance < 0)
		    yDistance = 0;
    }

    QPointF middle(geometry.left() + (geometry.width() - textRect.width()) * 0.5, geometry.top() + (geometry.height() - textRect.height()) * 0.5);

	// compute text center
	QPointF pos;
	if (textPosition & Vip::XInside) {
		if (textAlignment & Qt::AlignLeft)
			pos.setX(std::min(geometry.left() + xDistance,middle.x()) );
		else if (textAlignment & Qt::AlignRight)
			pos.setX(std::max(geometry.right() - xDistance - textRect.width(), middle.x()) );
		else
			pos.setX(middle.x());
	}
	else {
		if (textAlignment & Qt::AlignLeft)
			pos.setX(geometry.left() - xDistance - textRect.width() );
		else
			pos.setX(geometry.right() + xDistance );
	}
	if (textPosition & Vip::YInside) {
		if (textAlignment & Qt::AlignTop)
			pos.setY(std::min(geometry.top() + yDistance  , middle.y()));
		else if (textAlignment & Qt::AlignBottom)
			pos.setY(std::max(geometry.bottom() - yDistance - textRect.height() , middle.y()));
		else
			pos.setY(middle.y());
	}
	else {
		if (textAlignment & Qt::AlignTop)
			pos.setY(geometry.top() - yDistance - textRect.height() );
		else
			pos.setY(geometry.bottom() + yDistance );
	}

    painter->save();

    QTransform textTransform;
    textTransform.translate(pos.x(), pos.y());

    if (!text_tr.isIdentity()) {
	    QTransform tr;
	    QPointF ref = ref_pos;
	    ref.rx() *= textRect.width();
	    ref.ry() *= textRect.height();
	    QPointF tl = ref;
	    tl = textTransform.map(tl);
	    tr.translate(-tl.x(), -tl.y());
	    tr *= text_tr;
	    QPointF pt = text_tr.inverted().map(tl);
	    tr.translate(pt.x(), pt.y());
	    textTransform *= tr;
    }

	painter->setTransform(textTransform, true);
    t.draw(painter, textRect);
	painter->restore();
}


/* void VipPainter::drawText(QPainter* painter,
	const VipText & t,
	double textRotation,
	double textDistance,
	Vip::RegionPositions textPosition,
	Qt::Alignment textAlignment,
	const QRectF & geometry)
{
	QTransform text_tr;
	text_tr.rotate(textRotation);
	QPolygonF textPolygon = text_tr.map(t.textRect());
	QRectF textRect = textPolygon.boundingRect();
	QPointF textOffset = textRect.topLeft() - textPolygon[0];



	if (textPosition & Vip::XAutomatic) {
		textPosition = textPosition & (~Vip::XAutomatic);
		if (textRect.width() < geometry.width())
			textPosition |= Vip::XInside;
	}
	if (textPosition & Vip::YAutomatic) {
		textPosition = textPosition & (~Vip::YAutomatic);
		if (textRect.height() < geometry.height())
			textPosition |= Vip::YInside;
	}
	//compute text center
	QPointF pos;
	if (textPosition & Vip::XInside) {
		if (textAlignment & Qt::AlignLeft)
			pos.setX(geometry.left() + textDistance + textRect.width()*0.5);
		else if(textAlignment & Qt::AlignRight)
			pos.setX(geometry.right() - textDistance - textRect.width()*0.5);
		else
			pos.setX(geometry.left() + (geometry.width() - textRect.width())*0.5);
	}
	else {
		if (textAlignment & Qt::AlignLeft)
			pos.setY(geometry.left() - textDistance - textRect.width()*0.5);
		else
			pos.setY(geometry.right() + textDistance + textRect.width()*0.5);
	}
	if (textPosition & Vip::YInside) {
		if (textAlignment & Qt::AlignTop)
			pos.setY(geometry.top() + textDistance + textRect.height()*0.5);
		else if (textAlignment & Qt::AlignBottom)
			pos.setY(geometry.bottom() - textDistance - textRect.height()*0.5);
		else
			pos.setY(geometry.top() + (geometry.height() - textRect.height())*0.5);
	}
	else {
		if (textAlignment & Qt::AlignTop)
			pos.setY(geometry.top() - textDistance - textRect.height()*0.5);
		else
			pos.setY(geometry.bottom() + textDistance + textRect.height()*0.5);
	}
	pos -= textOffset +QPointF(//textRect.width() / 2
0, textRect.height() / 2);

	const QTransform current = painter->transform();
	if (textRotation) {
		QTransform tr;
		tr.translate(pos.x(), pos.y());
		tr.rotate(textRotation);
		painter->setTransform(tr, true);
		t.draw(painter, t.textRect());
		painter->setTransform(current);
	}
	else
		t.draw(painter, t.textRect());
}*/

QTransform VipPainter::resetTransform(QPainter * painter)
{
	QTransform tr = painter->transform();
	painter->resetTransform();
	return tr;
}
