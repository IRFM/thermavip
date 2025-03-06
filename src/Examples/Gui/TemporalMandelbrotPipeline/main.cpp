#include <cmath>
#include <iostream>

#include <qapplication.h>
#include <QGraphicsGridLayout>
#include <qboxlayout.h>

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
#include "VipDoubleSlider.h"

#include "VipExtractStatistics.h"
#include "VipDisplayObject.h"

#include "mandelbrot.h"




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
#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());

	// Setup opengl features (in case we render using opengl)
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	VipMultiGraphicsView w;
	
	// Optional, use opengl rendering
	//w.setRenderingMode(VipMultiGraphicsView::OpenGLThread);
	//VipText::setCacheTextWhenPossible(false);
	// Create all widgets/plotting areas

	QGraphicsGridLayout* grid = new QGraphicsGridLayout();

	int width = 2;
	int height = 1;

	QList<VipImageArea2D*> areas;
	for (int y=0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			VipImageArea2D* area = new VipImageArea2D();
			grid->addItem(area, y, x);
			areas.push_back(area);
			setup_image_area(area);
		}

	// Here we use a VipSceneModel to use its internal signal mechanims in order to have curve/histogram that update themselves when moving the shapes manually
	VipSceneModel model;
	VipShape polyline_shape(QPolygonF() << QPointF(100, 70) << QPointF(300, 300) << QPointF(500, 350), VipShape::Polyline);
	VipShape rect_shape(QRectF(300, 200, 50, 50));
	model.add(polyline_shape);
	model.add(rect_shape);

	VipPlotShape* poly = addShape(areas.first(), polyline_shape);
	VipPlotShape* rect = addShape(areas.first(), rect_shape);
	rect->setBrush(QColor(255, 0, 0, 70));

	// add a row with: one histogram, a polyline trace, a time trace

	VipPlotArea2D* hist = new VipPlotArea2D();
	setup_plot_area(hist);
	hist->setTitle("<b>Histogram over Region Of Intereset (ROI)");
	

	VipPlotArea2D* polyline = new VipPlotArea2D();
	setup_plot_area(polyline);
	polyline->setTitle("<b>Values along polyline");
	

	grid->addItem(hist, height, 0);
	grid->addItem(polyline, height, 1);

	w.widget()->setLayout(grid);
	
	



	// Setup pipeline


	// Note that, in this case (no serialization, no multiple VipIODevice), using a processing pool
	// is not mandatory. We use it to allocate processings on the heap and assign them a parent
	// in order to be properly destroyed at exit.

	VipProcessingPool pool;

	// Create mandelbrot generator
	MandelbrotDevice* gen = new MandelbrotDevice(&pool);
	gen->open(VipIODevice::ReadOnly);

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
	extracth->setShape(rect->rawData());
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
	extractp->setShape(poly->rawData());
	extractp->inputAt(0)->setConnection(gen->outputAt(0));
	extractp->topLevelOutputAt(0)->toMultiOutput()->resize(1);

	// Create display polyline
	VipDisplayCurve* p = new VipDisplayCurve(&pool);
	p->item()->setAxes(polyline->bottomAxis(), polyline->leftAxis(), VipCoordinateSystem::Cartesian);
	p->item()->boxStyle().setBorderPen(QPen(QColor(0x0178BB), 1.5));
	extractp->outputAt(0)->setConnection(p->inputAt(0));



	// create main widget
	QWidget* mainwidget = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();
	mainwidget->setLayout(vlay);
	vlay->addWidget(&w,1);
	vlay->addWidget(new PlayWidget(&pool));

	mainwidget->resize(1000, 700);
	mainwidget->show();
	//mainwidget->showMaximized();

	return app.exec();
}