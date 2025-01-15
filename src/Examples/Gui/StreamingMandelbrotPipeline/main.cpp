#include <cmath>
#include <iostream>
#include <thread>

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
#include "VipLegendItem.h"

#include "VipSequentialGenerator.h"
#include "VipExtractStatistics.h"
#include "VipDisplayObject.h"


/// @brief Multithreaded Mandelbrot set image generator
class Mandelbrot
{
	int MAX;

	VIP_ALWAYS_INLINE int mandelbrot(double startReal, double startImag) const
	{
		double zReal = startReal;
		double zImag = startImag;

		for (int counter = 0; counter < MAX; ++counter) {
			double r2 = zReal * zReal;
			double i2 = zImag * zImag;
			if (r2 + i2 > 4.0)
				return counter;
			zImag = 2.0 * zReal * zImag + startImag;
			zReal = r2 - i2 + startReal;
		}
		return MAX;
	}
	
	void updateImageSlice(double zoom, double offsetX, double offsetY, VipNDArrayTypeView<int> image) const
	{
		const int height = image.shape(0);
		const int width = image.shape(1);
		double real = 0 * zoom - width / 2.0 * zoom + offsetX;
		const double imagstart = - height / 2.0 * zoom + offsetY;
		int* img = image.ptr();
		
		#pragma omp parallel for
		for (int y = 0; y < height; y++) {
			double iv = imagstart + y * zoom;
			int* p = img + y * width;
			double rv = real;
			for (int x = 0; x < width; x++, rv += zoom) {
				p[x] = mandelbrot(rv, iv);
			}
		}
	}

public:
	Mandelbrot(int max = 0)
	  : MAX(max)
	{
		if (max == 0)
			MAX = std::thread::hardware_concurrency() * 32 - 1;
	}
	void updateImage(double zoom, double offsetX, double offsetY, VipNDArrayTypeView<int> image) 
	{
		updateImageSlice(zoom, offsetX, offsetY, (image));
	}
};


VipAnyData generateMandelbrot(const VipAnyData& data) 
{
	double zoom = (0.004);
	double offsetX = (-0.745917);
	double offsetY = (0.09995);
	if (data.time() != 0) {
		zoom = data.attribute("zoom").toDouble();
		zoom *= 0.96;
		if (zoom < 2.38339e-13)
			zoom = 0.004;
	}
	Mandelbrot gen(383);
	VipNDArray ar(qMetaTypeId<int>(), vipVector(420, 640));
	gen.updateImage(zoom, offsetX, offsetY, VipNDArrayTypeView<int>(ar));

	VipAnyData res(QVariant::fromValue(ar));
	res.setAttribute("zoom", zoom);
	return res;
}




VipPlotShape* addShape(VipImageArea2D* area, const VipShape &sh )
{
	// Add a shape over VipPlotSpectrogram
	VipPlotShape* psh = new VipPlotShape();
	psh->setRawData(sh);
	psh->setPen(QPen(Qt::red));
	psh->setFlag(QGraphicsItem::ItemIsSelectable, true);
	psh->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
	psh->setPolygonEditable(true);
	psh->setZValue(area->spectrogram()->zValue() + 10);

	// Make the shape movable/resizable
	VipResizeItem* resize = new VipResizeItem();
	resize->setManagedItems(PlotItemList() << psh);
	resize->setLibertyDegrees(VipResizeItem::MoveAndResize | VipResizeItem::Rotate);
	resize->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);

	return psh;
}



void setup_rendering_strategy(VipAbstractPlotArea* area)
{
	//area->setRenderingThreads(12);
	//area->setRenderStrategy(VipPlotArea2D::AutoStrategy);
}


void setup_image_area(VipImageArea2D* area)
{
	setup_rendering_strategy(area);

	
	// show color map
	area->colorMapAxis()->setVisible(true);
	// set color map colors
	area->colorMapAxis()->setColorMap(VipLinearColorMap::Fusion);
	// add a tool tip that only displays the spectrogram custom tool tip
	area->setPlotToolTip(new VipToolTip());
	area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
	// allow zooming/moving with the mouse
	area->setMouseWheelZoom(true);
	area->setMousePanning(Qt::RightButton);

	// Since we are streaming, update the tool tip when the spectrogram data changes
	//QObject::connect(area->spectrogram(), SIGNAL(dataChanged()), area->plotToolTip(), SLOT(refresh()));

	// Display a tool tip over color map grips
	area->colorMapAxis()->grip1()->setToolTipText("#value");
	area->colorMapAxis()->grip2()->setToolTipText("#value");

	area->colorMapAxis()->grip1()->setDisplayToolTipValue(Qt::AlignRight | Qt::AlignVCenter);
	area->colorMapAxis()->grip2()->setDisplayToolTipValue(Qt::AlignRight | Qt::AlignVCenter);

}

void setup_plot_area(VipPlotArea2D* area) 
{
	setup_rendering_strategy(area);

	// allow mouse zooming/panning
	area->setMouseWheelZoom(true);
	area->setMousePanning(Qt::RightButton);
	// add tool tip
	area->setPlotToolTip(new VipToolTip());
	area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
	area->legend()->setVisible(false);
}



#include <qsurfaceformat.h>

int main(int argc, char** argv)
{
	// Setup opengl features (in case we render using opengl)
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	VipMultiGraphicsView w;
	
	// Optional, use opengl rendering
	//w.setOpenGLRendering(true);
	w.setRenderingMode(VipMultiGraphicsView::OpenGLThread);
	VipText::setCacheTextWhenPossible(false);
	// Create all widgets/plotting areas

	QGraphicsGridLayout* grid = new QGraphicsGridLayout();

	int width = 3;
	int height = 2;

	QList<VipImageArea2D*> areas;
	for (int y=0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			VipImageArea2D* area = new VipImageArea2D();
			grid->addItem(area, y, x);
			areas.push_back(area);
			setup_image_area(area);
		}

	VipPlotShape * poly = addShape(areas.first(), VipShape(QPolygonF() << QPointF(100, 70) << QPointF(300, 300) << QPointF(500, 350), VipShape::Polyline));
	VipPlotShape* rect = addShape(areas.first(), VipShape(QRectF(300, 200, 50, 50)));
	rect->setBrush(QColor(255, 0, 0, 70));

	// add a row with: one histogram, a polyline trace, a time trace

	VipPlotArea2D* hist = new VipPlotArea2D();
	setup_plot_area(hist);
	hist->setTitle("<b>Histogram over Region Of Intereset (ROI)");
	

	VipPlotArea2D* polyline = new VipPlotArea2D();
	setup_plot_area(polyline);
	polyline->setTitle("<b>Values along polyline");
	

	VipPlotArea2D* trace = new VipPlotArea2D();
	setup_plot_area(trace);
	trace->setTitle("<b>Time trace of the macimum value inside Region Of Intereset (ROI)");
	trace->titleAxis()->setVisible(true);
	trace->bottomAxis()->setOptimizeFromStreaming(true);
	
	VipTimeToText* vt = new VipTimeToText("ss", VipTimeToText::MillisecondsSE, VipTimeToText::DifferenceValue);
	vt->setMultiplyFactor(1e-6); // ns to ms
	vt->setAdditionalFormat("<b>dd.MM.yyyy<br>hh:mm:ss");
	VipTextStyle st;
	st.setAlignment(Qt::AlignRight | Qt::AlignBottom);
	trace->bottomAxis()->scaleDraw()->setAdditionalTextStyle(st);
	VipFixedScaleEngine* engine = new VipFixedScaleEngine(vt);
	engine->setMaxIntervalWidth(1e10);//10s
	trace->bottomAxis()->scaleDraw()->setValueToText(vt);
	trace->bottomAxis()->setScaleEngine(engine);
	trace->topAxis()->setVisible(false);
	trace->rightAxis()->setVisible(false);
	
	grid->addItem(hist, height, 0);
	grid->addItem(polyline, height, 1);
	grid->addItem(trace, height, 2);

	w.widget()->setLayout(grid);
	
	w.resize(1000, 500);
	w.showMaximized();



	// Setup pipeline


	// Note that, in this case (no serialization, no multiple VipIODevice), using a processing pool
	// is not mandatory. We use it to allocate processings on the heap and assign them a parent
	// in order to be properly destroyed at exit.

	VipProcessingPool pool;

	// Create mandelbrot generator
	VipSequentialGenerator* gen = new VipSequentialGenerator(generateMandelbrot , 0.05,&pool);

	// Create one VipDisplayImage per VipImageArea2D
	for (int i = 0; i < areas.size(); ++i) {
		VipDisplayImage* img = new VipDisplayImage(&pool);
		// set tool tip text for the spectrogram
		img->item()->setToolTipText("<b>X</b>: #avalue0%i<br><b>Y</b>: #avalue1%i<br><b>Value</b>: #value");
		// antialiazed rendering
		img->item()->setRenderHints(QPainter::Antialiasing);
		// ignore the mouse event in order to forward them to the higher level items (in this case VipPlotShape)
		img->item()->setItemAttribute(VipPlotItem::IgnoreMouseEvents, true);

		img->inputAt(0)->setConnection(gen->outputAt(0));
		areas[i]->setSpectrogram(img->item());
		areas[i]->colorMapAxis()->setVisible(true); // show again color map
	}

	
	// Create extract histogram
	VipExtractHistogram* extracth = new VipExtractHistogram(&pool);
	extracth->setScheduleStrategy(VipExtractHistogram::Asynchronous);
	extracth->propertyName("bins")->setData(20);
	extracth->setFixedShape(rect->rawData());
	extracth->inputAt(0)->setConnection(gen->outputAt(0));
	extracth->topLevelOutputAt(0)->toMultiOutput()->resize(1);
	
	// Create display histogram
	VipDisplayHistogram* h = new VipDisplayHistogram(&pool);
	h->item()->setAxes(hist->bottomAxis(), hist->leftAxis(), VipCoordinateSystem::Cartesian);
	h->item()->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
	h->item()->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
	extracth->outputAt(0)->setConnection(h->inputAt(0));

	// Create extract polyline
	VipExtractPolyline* extractp = new VipExtractPolyline(&pool);
	extractp->setScheduleStrategy(VipExtractHistogram::Asynchronous);
	extractp->setFixedShape(poly->rawData());
	extractp->inputAt(0)->setConnection(gen->outputAt(0));
	extractp->topLevelOutputAt(0)->toMultiOutput()->resize(1);

	// Create display polyline
	VipDisplayCurve* p = new VipDisplayCurve(&pool);
	p->item()->setAxes(polyline->bottomAxis(), polyline->leftAxis(), VipCoordinateSystem::Cartesian);
	p->item()->boxStyle().setBorderPen(QPen(QColor(0x0178BB), 1.5));
	extractp->outputAt(0)->setConnection(p->inputAt(0));


	// Create extract time trace
	VipExtractStatistics* extracts = new VipExtractStatistics(&pool);
	extracts->setStatistics(VipShapeStatistics::Mean);
	extracts->setScheduleStrategy(VipExtractHistogram::Asynchronous);
	extracts->setFixedShape(poly->rawData());
	extracts->inputAt(0)->setConnection(gen->outputAt(0));

	// Create display time trace
	VipDisplayCurve* t = new VipDisplayCurve(&pool);
	t->item()->setAxes(trace->bottomAxis(), trace->leftAxis(), VipCoordinateSystem::Cartesian);
	t->item()->boxStyle().setBorderPen(QPen(QColor(0x0178BB), 1.5));
	t->propertyName("Sliding_time_window")->setData(11); // 11s sliding windows
	extracts->outputName("mean")->setConnection(t->inputAt(0));


	// Start streaming
	pool.setStreamingEnabled(true);

	return app.exec();
}