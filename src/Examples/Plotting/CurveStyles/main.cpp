#include <cmath>
#include <qapplication.h>
#include "VipColorMap.h"
#include "VipPlotCurve.h"
#include "VipSymbol.h"
#include "VipPlotWidget2D.h"
#include "VipToolTip.h"


static VipPointVector offset(const VipPointVector& vec, const VipPoint& off) {
	VipPointVector res = vec;
	for (int i = 0; i < res.size(); ++i)
		res[i] += off;
	return res;
}

#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());

    QApplication app(argc, argv);

	VipPlotWidget2D w;
    w.area()->setMouseWheelZoom(true);
    w.area()->setMousePanning(Qt::RightButton);

	w.area()->bottomAxis()->setTitle("X axis");
    w.area()->leftAxis()->setTitle("Y axis");

	// configure tooltip
    w.area()->setPlotToolTip(new VipToolTip());
    w.area()->plotToolTip()->setDisplayFlags(VipToolTip::ItemsTitles | VipToolTip::ItemsLegends | VipToolTip::ItemsToolTips | VipToolTip::Axes);
    w.area()->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

    // generate curve
    VipPointVector vec;
    for (int i = 0; i < 50; ++i)
	    vec.push_back(VipPoint(i * 0.15, std::cos(i * 0.15)));

    VipPoint yoffset(0, 0);

    VipColorPalette p(VipLinearColorMap::ColorPaletteRandom);

    {
	    VipPlotCurve* c = new VipPlotCurve("Lines");
	    c->setRawData(vec);
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
    }
    yoffset += VipPoint(0, 1);
   {
	    VipPlotCurve* c = new VipPlotCurve("Sticks");
	    c->setRawData(offset(vec, yoffset));
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setStyle(VipPlotCurve::Sticks);
	    c->setBaseline(yoffset.y());
	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
    }
    yoffset += VipPoint(0, 1);
    {
	    VipPlotCurve* c = new VipPlotCurve("Steps");
	    c->setRawData(offset(vec, yoffset));
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setStyle(VipPlotCurve::Steps);
	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
   }
    yoffset += VipPoint(0, 1);
    {
	    VipPlotCurve* c = new VipPlotCurve("Dots");
	    c->setRawData(offset(vec, yoffset));
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setStyle(VipPlotCurve::Dots);
	    c->setPen(QPen(p.color((int)yoffset.y()), 3));
	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
    }
    yoffset += VipPoint(0, 1);
    {
	    VipPlotCurve* c = new VipPlotCurve("Baseline Filled");
	    c->setRawData(offset(vec, yoffset));
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setBrush(QBrush(c->majorColor().lighter()));
	    c->setBaseline(yoffset.y());
	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
    }
    yoffset += VipPoint(0, 1);
    {

	    VipPlotCurve* c = new VipPlotCurve("Inner Filled");
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setSubBrush(0, QBrush(c->majorColor().lighter()));
	    c->setCurveAttribute(VipPlotCurve::FillMultiCurves);
	    VipPointVector v = offset(vec, yoffset);
	    v += Vip::InvalidPoint;
	    yoffset += VipPoint(0, 1);
	    v += offset(vec, yoffset);
	    c->setRawData(v);

	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
   }
   yoffset += VipPoint(0, 1);
    for (int i = VipSymbol::Ellipse; i <= VipSymbol::Hexagon; ++i) {
	    VipPlotCurve* c = new VipPlotCurve(VipSymbol::nameForStyle(VipSymbol::Style(i)));
	    c->setMajorColor(p.color((int)yoffset.y()));
	    c->setSymbol(new VipSymbol(VipSymbol::Style(i)));
	    c->symbol()->setPen(p.color((int)yoffset.y()));
	    c->symbol()->setBrush(QBrush(p.color((int)yoffset.y()).lighter()));
	    c->symbol()->setSize(13, 13);
	    c->setRawData(offset(vec, yoffset));
	    c->setSymbolVisible(true);
	    c->setStyle(VipPlotCurve::NoCurve);
	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	    c->setToolTipText("<b>X:</b> #avalue0<br><b>Y:</b> #avalue1");
	    yoffset += VipPoint(0, 1);
    }
///
    w.show();
   return app.exec();
}