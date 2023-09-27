#include <cmath>
#include <iostream>

#include <qapplication.h>

#include "VipSequentialGenerator.h"
#include "VipDisplayObject.h"
#include "VipPlotWidget2D.h"
#include "VipToolTip.h"
#include "VipPlotCurve.h"
#include "VipProcessingFunction.h"

QVariant generate_cos(const QVariant& , qint64 prev_ns) {
	return std::cos(QDateTime::currentMSecsSinceEpoch() * 0.001);
}

double generate_fast_cos(double v)
{
	return std::cos(QDateTime::currentMSecsSinceEpoch() * 0.01)
		* std::abs(v) + 0.5;
}

struct generate_rectangular
{
	double operator()(double v) const
	{
		if (v > 0)
			v = 1;
		else if (v < 0)
			v = -1;
		return v * 0.75;
	}
};

/// Generate a simple pipeline of 2 processings: source VipSequentialGenerator and destination VipDisplayCurve
template<class Fun>
void generate_pipeline(VipProcessingPool* pool, VipPlotArea2D * area, const Fun & generator)
{
	// Build generator
	VipSequentialGenerator* gen = new VipSequentialGenerator(pool);
	// set generator function
	gen->setGeneratorFunction(generator);
	// set sampling time to 10ms
	gen->propertyAt(0)->setData(0.01); 
	// open
	gen->open(VipIODevice::ReadOnly);

	//create fast cos processing and connect to generator
	VipProcessingObject* fcos = vipProcessingFunction(generate_fast_cos);
	// Set its strategy to Asynchronous
	fcos->setScheduleStrategy(VipProcessingObject::Asynchronous);
	fcos->inputAt(0)->setConnection(gen->outputAt(0));

	// create rectangular processing and connect to generator
	VipProcessingObject* rect = vipProcessingFunction(generate_rectangular{});
	// Set its strategy to Asynchronous
	rect->setScheduleStrategy(VipProcessingObject::Asynchronous);
	rect->inputAt(0)->setConnection(gen->outputAt(0));


	// Build display object for generator
	VipDisplayCurve* display_gen = new VipDisplayCurve(pool);
	display_gen->item()->setTitle("cos");
	// Set time window to 10s
	display_gen->propertyName("Sliding_time_window")->setData(10); 
	// set axes
	display_gen->item()->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
	// connect generator to display
	gen->outputAt(0)->setConnection(display_gen->inputAt(0));


	// Build display object for fast cos
	VipDisplayCurve* display_fcos = new VipDisplayCurve(pool);
	display_fcos->item()->setTitle("fast cos");
	// Set time window to 10s
	display_fcos->propertyName("Sliding_time_window")->setData(10);
	// set axes
	display_fcos->item()->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
	// connect generator to display
	fcos->outputAt(0)->setConnection(display_fcos->inputAt(0));

	// Build display object for rectangular
	VipDisplayCurve* display_rect = new VipDisplayCurve(pool);
	display_rect->item()->setTitle("rect");
	// Set time window to 10s
	display_rect->propertyName("Sliding_time_window")->setData(10);
	// set axes
	display_rect->item()->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
	// connect generator to display
	rect->outputAt(0)->setConnection(display_rect->inputAt(0));
}

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

	area->bottomAxis()->setTitle(VipText("<b>Time (s)").setAlignment(Qt::AlignLeft));
	area->leftAxis()->setTitle(VipText("<b>Value (s)").setAlignment(Qt::AlignLeft));

	// Axes intersect each other in the middle
	area->leftAxis()->setAxisIntersection(area->bottomAxis(), 0.5, Vip::Relative);
	area->bottomAxis()->setAxisIntersection(area->leftAxis(), 0.5, Vip::Relative);

	// Make sure bottom axis text on the sides are visible
	area->bottomAxis()->setUseBorderDistHintForLayout(true);

	// Increase maximum major steps
	area->bottomAxis()->setMaxMajor(25);
	area->leftAxis()->setMaxMajor(25);

	VipTimeToText* vt = new VipTimeToText("ss.z", VipTimeToText::MillisecondsSE, VipTimeToText::DifferenceValueNoAdditional);
	//vt->setAdditionalFormat("<b>dd.MM.yyyy<br>hh:mm:ss");
	vt->setMultiplyFactor(1e-6);
	VipTextStyle st;
	st.setAlignment(Qt::AlignRight | Qt::AlignBottom);
	area->bottomAxis()->scaleDraw()->setAdditionalTextStyle(st);
	VipFixedScaleEngine* engine = new VipFixedScaleEngine(vt);
	engine->setMaxIntervalWidth(10000000000.);//10s
	area->bottomAxis()->scaleDraw()->setValueToText(vt);
	area->bottomAxis()->setScaleEngine(engine);

	area->setTitle("<b>Streaming pipeline");

	area->setMargins(5);

	area->setPlotToolTip(new VipToolTip());
	area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsTitles | VipToolTip::ItemsPos | VipToolTip::ItemsToolTips | VipToolTip::ItemsLegends);
	
}


#include <qsurfaceformat.h>

VipAnyData test(const VipAnyData& v)
{
	return v;
}

int main(int argc, char** argv)
{

	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	VipGlobalStyleSheet::setStyleSheet(

	  //"VipPlotArea2D { background: #474747;}"
	  "VipAbstractPlotArea { title-color: white; background: #383838; mouse-wheel-zoom: true; mouse-panning:leftButton; colorpalette: set1; tool-tip-selection-border: yellow; "
	  "tool-tip-selection-background: rgba(255,255,255,30); legend-position: innerTopLeft; legend-border-distance:20; }"
	  "VipPlotItem { title-color: white; color: white; render-hint: antialiasing; }"
	  "VipPlotCurve {border-width: 2; attribute[clipToScaleRect]: true; }"
	  "VipAxisBase {title-color: white; label-color: white; pen: white;}"
	  "VipAxisBase:title {margin: 10;}"
	  "VipPlotGrid { major-pen: 1px dot white; }"
	  "VipPlotCanvas {background: #333333; border : 1px solid green;} "
	  "VipLegend { font: bold 10pt 'Arial'; display-mode: allItems; max-columns: 1; color: white; alignment:hcenter|vcenter; expanding-directions:vertical; border:white; border-radius:5px; background: "
	  "rgba(255,255,255,50); maximum-width: 16;}");


	QApplication app(argc, argv);


	VipPlotWidget2D w; 
	VipPlotArea2D* area = w.area();
	
	setup_plot_area(area);

	VipProcessingPool pool;

	generate_pipeline(&pool, area,  generate_cos);

	// start streaming
	pool.setStreamingEnabled(true);

	w.show();
	return app.exec();
}