#include <cmath>
#include <iostream>

#include <qapplication.h>
#include <QGraphicsGridLayout>
#include <qthreadpool.h>

#include "VipPlotWidget2D.h"
#include "VipColorMap.h"
#include "VipPlotShape.h"
#include "VipPlotHistogram.h"
#include "VipPlotSpectrogram.h"
#include "VipToolTip.h"
#include "VipPlotCurve.h"
#include "VipColorMap.h"
#include "VipSliderGrip.h"
#include "VipAxisColorMap.h"


/// @brief Generate a cosinus curve of at most 500 points with X values being in seconds
class CurveStreaming : public QThread
{
	QList<VipPoint> points;
	QList<VipPlotCurve*> curves;
	bool stop;

public:
	CurveStreaming(const QList<VipPlotCurve*>& cs)
	  : curves(cs)
	  , stop(false)
	{
	}

	~CurveStreaming() 
	{ 
		stop = true;
		this->wait();
	}

	virtual void run()
	{
		double start = QDateTime::currentMSecsSinceEpoch();
		qint64 start_print = QDateTime::currentMSecsSinceEpoch();
		curves.first()->resetFpsCounter();

		int point_count = 0;

		while (!stop) {
		
			// Continuously add points to the curves up to a maximum of 100k points per curves, without any sleep time

			double x = (QDateTime::currentMSecsSinceEpoch() - start) * 1e-3; 
			double y = std::cos(x*2);
			points.push_back(VipPoint(x, y));
			if (points.size() > 100000)
				points.erase(points.begin());

			for (int i = 0; i < curves.size(); ++i) {
				VipPointVector tmp(points.size());
				if (i > 0) {
					// add y offset of 1
					for (int j = 0; j < points.size(); ++j)
						tmp[j] = points[j] + VipPoint(0, i);
				}
				else
					std::copy(points.begin(), points.end(), tmp.begin());

				point_count += tmp.size();
				curves[i]->setRawData(tmp);
			}

			qint64 current = QDateTime::currentMSecsSinceEpoch(); 
			if (current - start_print > 1000) {
				printf("Curve update rate: %i pts/s\n", point_count);
				printf("Display rate: %i Hz\n", curves.first()->fps());
				start_print = current;
				point_count = 0;
			}
		}
	}
};


void setup_plot_area(VipPlotArea2D* area) 
{
	// show title
	area->titleAxis()->setVisible(true);
	// allow wheel zoom
	//area->setMouseWheelZoom(true);
	// allow mouse panning
	//area->setMousePanning(Qt::RightButton);
	// display tool tip
	area->setPlotToolTip(new VipToolTip());
	area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
	// hide right and top axes
	area->rightAxis()->setVisible(false);
	area->topAxis()->setVisible(false);

	// bottom axis intersect left one at 0
	area->bottomAxis()->setAxisIntersection(area->leftAxis(), 0);
	area->bottomAxis()->setTitle("<b>Time");

	//area->setRenderStrategy(VipAbstractPlotArea::OpenGLOffscreen);
	//area->setRenderingThreads(6);


	area->setTitle("<b>Heavy plotting");
		// Default strategy with non floating scale engine

	area->setMargins(5);
	
}


#include <qsurfaceformat.h>

int main(int argc, char** argv)
{
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	VipGlobalStyleSheet::setStyleSheet(

	  "VipPlotArea2D { background: #474747}"
	  "VipAbstractPlotArea { title-color: white; background: #383838; mouse-wheel-zoom: true; mouse-panning:leftButton; colorpalette: set1; tool-tip-selection-border: yellow; "
	  "tool-tip-selection-background: rgba(255,255,255,30); legend-position: innerTopLeft; legend-border-distance:20; }"
	  "VipPlotItem { title-color: white; color: white; render-hint: antialiasing; }"
	  "VipPlotCurve {border-width: 2; attribute[clipToScaleRect]: true; }"
	  "VipAxisBase {title-color: white; label-color: white; pen: white;}"
	  "VipAxisBase:title {margin: 10;}"
	  "VipPlotGrid { major-pen: 1px dot white; }"
	  "VipLegend { font: bold 10pt 'Arial'; display-mode: allItems; max-columns: 1; color: white; alignment:hcenter|vcenter; expanding-directions:vertical; border:white; border-radius:5px; background: "
	  "rgba(255,255,255,50);}");


	QApplication app(argc, argv);

	

	VipPlotWidget2D w; 
	VipPlotArea2D* area = w.area();

	// area->setMaximumFrameRate(2);
	// area->setRenderStrategy(VipPlotArea2D::OpenGLOffscreen);
	// area->setRenderingThreads(6);
	setup_plot_area(area);

	QList<VipPlotCurve*> curves;
	int count = 3;
	
	for (int i = 0; i < count;  ++i) {

		VipPlotCurve* c = new VipPlotCurve();
		c->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
		c->setTitle("Curve " + QString::number(i + 1));
		curves.push_back(c);
	}
	
	w.resize(1000, 500);
	w.show();

	CurveStreaming thread(curves);
	thread.start();

	return app.exec();
}