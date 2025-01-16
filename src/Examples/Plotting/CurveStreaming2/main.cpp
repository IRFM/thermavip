#include <cmath>
#include <iostream>

#include <qapplication.h>
#include <qthread.h>
#include <qdir.h>
#include <qsurfaceformat.h>

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

struct Curve
{
	VipPlotCurve* curve;
	double factor;
};

/// @brief Generate a cosinus curve of at most 500 points with X values being in seconds
class CurveStreaming : public QThread
{
	QList<Curve> curves;
	bool stop;

public:
	CurveStreaming(const QList<Curve>& cs)
	  : curves(cs)
	  , stop(false)
	{
	}
	~CurveStreaming() { stopThread(); }
	void stopThread()
	{
		stop = true;
		this->wait();
	}
	virtual void run()
	{
		while (!stop) {
			// Update all curves with a noisy signal in a dedicated thread
			for (int i = 0; i < curves.size(); ++i) {
				VipPointVector vec(10000);
				double f = curves[i].factor;
				for (int j = 0; j < vec.size(); ++j)
					vec[j] = VipPoint(j, f + (rand() % 16) - 7);
				curves[i].curve->setRawData(vec);
			}

			QThread::msleep(1);
		}
	}
};

void setup_plot_area(VipPlotArea2D* area)
{
	// show title
	area->titleAxis()->setVisible(true);
	area->titleAxis()->setTitle("<b>Stream 100 curves of 10 000 points each");
	// allow wheel zoom
	area->setMouseWheelZoom(true);
	// allow mouse panning
	area->setMousePanning(Qt::RightButton);

	// hide right and top axes
	area->rightAxis()->setVisible(false);
	area->topAxis()->setVisible(false);

	// bottom axis intersect left one at 0
	area->bottomAxis()->setAxisIntersection(area->leftAxis(), 0);

	// add margins to the plotting area
	area->setMargins(10);
	// Use high update frame rate if possible
	area->setMaximumFrameRate(100);
}

int main(int argc, char** argv)
{
	// Setup opengl settings
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(10);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	VipPlotWidget2D w;
	// Enable threaded OpenGL rendering if necessary
	//w.setRenderingMode(VipPlotWidget2D::OpenGLThread);

	// setup plotting area
	setup_plot_area(w.area());

	VipColorPalette p(VipLinearColorMap::ColorPaletteRandom);
	QList<Curve> curves;

	// Create 100 curves of 10000 points each)
	for (int i = 0; i < 100; ++i) {
		VipPlotCurve* c = new VipPlotCurve();
		c->setPen(QPen(p.color(i)));
		c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
		curves.push_back(Curve{ c, (double)i * 16 * (i % 2 ? 1 : -1) });
	}

	w.resize(1000, 500);
	w.show();

	// Start streaming curves (100 curves of
	CurveStreaming thread(curves);
	thread.start();

	int ret = app.exec();
	thread.stopThread();
	return ret;
}