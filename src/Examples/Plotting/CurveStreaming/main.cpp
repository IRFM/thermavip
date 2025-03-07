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

#include "VipCircularVector.h"

/// @brief Generate a cosinus curve of at most 500 points with X values being in seconds
class CurveStreaming : public QThread
{
	VipPointVector vec;
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

		while (!stop) {
		
			double x = (QDateTime::currentMSecsSinceEpoch() - start) * 1e-3; 
			double y = std::cos(x*2);

			for (int i = 0; i < curves.size(); ++i)
				curves[i]->updateSamples([x, y](VipPointVector& vec) {
					vec.push_back(VipPoint(x, y));
					if (vec.size() > 500)
						vec.erase(vec.begin());
					});

			/* vec.push_back(VipPoint(x, y));
			if (vec.size() > 500)
				vec.erase(vec.begin());
			for (int i = 0; i < curves.size(); ++i)
				curves[i]->setRawData(vec);
			*/
			qint64 current = QDateTime::currentMSecsSinceEpoch(); 
			if (current - start_print > 1000) {
				printf("Rate: %i\n", curves.first()->fps());
				start_print = current;
			}

			QThread::msleep(10);

			//TODO:remove
			//if (QDateTime::currentMSecsSinceEpoch() - start > 2000)
			//	break;
		}
	}
};


void setup_plot_area(VipPlotArea2D* area, int setup_x_scale) 
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

	// Adjust the bottom scale update strategy

	if (setup_x_scale == 0) {
		area->setTitle("<b>Default behavior, X scale use the closest integer boundaries");
		// Default strategy with non floating scale engine


	}
	if (setup_x_scale == 1) {
		area->setTitle("<b>X scale is adjusted according to the curve exact boundaries");
		// Use floating scale engine
		area->bottomAxis()->scaleEngine()->setAttribute(VipScaleEngine::Floating, true);
	}
	else if (setup_x_scale == 2) {
		area->setTitle("<b>X scale tick positions are fixed, only displayed values are updated");
		// Use fixed tick positions through VipFixedValueToText and VipFixedScaleEngine
		VipFixedValueToText* vt = new VipFixedValueToText();
		area->bottomAxis()->scaleDraw()->setValueToText(vt);
		area->bottomAxis()->setScaleEngine(new VipFixedScaleEngine(vt));
	}
	else if (setup_x_scale == 3) {
		area->setTitle("<b>X scale tick positions are fixed, display values as time");
		// Use fixed tick positions through VipFixedValueToText and VipFixedScaleEngine
		VipTimeToText* vt = new VipTimeToText("hh:mm:ss");
		vt->setMultiplyFactor(1000); // convert s to ms
		area->bottomAxis()->scaleDraw()->setValueToText(vt);
		area->bottomAxis()->setScaleEngine(new VipFixedScaleEngine(vt));
	}

	else if (setup_x_scale == 4) {
		area->setTitle("<b>X scale tick positions are fixed, display difference value from origin");
		// Use fixed tick positions through VipFixedValueToText and VipFixedScaleEngine,
		// and display tick values as the difference between the actual tick value and the left-most tick value.
		// Only the left-most tick text is updated
		VipFixedValueToText* vt = new VipFixedValueToText(QString(),VipFixedValueToText::DifferenceValue);
		area->bottomAxis()->scaleDraw()->setValueToText(vt);
		area->bottomAxis()->setScaleEngine(new VipFixedScaleEngine(vt));
		
		// Align additional text to the right, and translate it by 10 pixels horizontally
		VipTextStyle st = area->bottomAxis()->scaleDraw()->textStyle();
		st.setAlignment(Qt::AlignRight);
		area->bottomAxis()->scaleDraw()->setAdditionalTextStyle(st);
		vt->additionalTextTransform().translate(10, 0);
	}

	else if (setup_x_scale == 5) {
		area->setTitle("<b>X scale tick positions are fixed, display difference value from origin as time");
		// Use fixed tick positions through VipFixedValueToText and VipFixedScaleEngine,
		// and display tick values as the difference between the actual tick value and the left-most tick value.
		// Only the left-most tick text is updated
		VipTimeToText* vt = new VipTimeToText("ss", VipTimeToText::Milliseconds, VipTimeToText::DifferenceValue);
		vt->setMultiplyFactor(1000); // convert s to ms
		vt->setAdditionalFormat("hh:mm:ss");
		area->bottomAxis()->scaleDraw()->setValueToText(vt);
		area->bottomAxis()->setScaleEngine(new VipFixedScaleEngine(vt));


		// Align additional text to the right, and translate it by 10 pixels horizontally
		VipTextStyle st = area->bottomAxis()->scaleDraw()->textStyle();
		st.setAlignment(Qt::AlignRight);
		area->bottomAxis()->scaleDraw()->setAdditionalTextStyle(st);
		vt->additionalTextTransform().translate(10, 0);
	}

	area->setMargins(5);
	
}


#include <qsurfaceformat.h>

#include <QDir>
int main(int argc, char** argv)
{
	//TOREMOVE
	//detail::testVipCircularVector<size_t,10000000>();

	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	VipGlobalStyleSheet::setStyleSheet(

	  "VipMultiGraphicsWidget { background: #474747}"
	  "VipAbstractPlotArea { title-color: white; background: #383838; mouse-wheel-zoom: true; mouse-panning:leftButton; colorpalette: set1; tool-tip-selection-border: yellow; "
	  "tool-tip-selection-background: rgba(255,255,255,30); legend-position: innerTopLeft; legend-border-distance:20; }"
	  "VipPlotItem { title-color: white; color: white;}"
	  "VipPlotCurve {border-width: 2; title: 'My curve'; attribute[clipToScaleRect]: true; }"
	  "VipAxisBase {title-color: white; label-color: white; pen: white;}"
	  "VipAxisBase:title {margin: 10;}"
	  "VipPlotGrid { major-pen: 1px dot white; }"
	  "VipLegend { font: bold 10pt 'Arial'; display-mode: allItems; color: white; alignment:hcenter|vcenter; expanding-directions:vertical; border:white; border-radius:5px; background: "
	  "rgba(255,255,255,50);}");


	QApplication app(argc, argv);

	

	VipMultiGraphicsView w; 
	//w.setViewport(new QPaintOpenGLWidget());
	w.setRenderingMode(VipMultiGraphicsView::OpenGLThread);
	VipText::setCacheTextWhenPossible(false);

	QGraphicsGridLayout* grid = new QGraphicsGridLayout();
	

	int width = 3;
	int height = 2;
	int i = 0;

	QList<VipPlotCurve*> curves;

	for (int y=0; y < height; ++y)
		for (int x = 0; x < width; ++x, ++i) {
			
			VipPlotArea2D* area = new VipPlotArea2D();
			
			//area->setMaximumFrameRate(2);
			//area->setRenderStrategy(VipPlotArea2D::OpenGLOffscreen);
			//area->setRenderingThreads(6);
			grid->addItem(area, y, x);
			setup_plot_area(area,i);

			VipPlotCurve* c = new VipPlotCurve();
			c->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
			c->setPen(QColor(0x0178BB));
			// disable clipping
			c->setItemAttribute(VipPlotItem::ClipToScaleRect, false);

			curves.push_back(c);
		}

	w.widget()->setLayout(grid);

	
	w.resize(1000, 500);
	w.showMaximized();

	CurveStreaming thread(curves);
	thread.start();

	return app.exec();
}