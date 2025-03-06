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
		const double imagstart = -height / 2.0 * zoom + offsetY;
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
	void updateImage(double zoom, double offsetX, double offsetY, VipNDArrayTypeView<int> image) { updateImageSlice(zoom, offsetX, offsetY, (image)); }
};

struct Histogram
{
	VipShape shape;
	VipPlotHistogram * hist;
};
struct Poly
{
	VipShape shape;
	VipPlotCurve* curve;
};
struct TimeTrace
{
	VipShape shape;
	VipPlotCurve* curve;
};

/// @brief In on thread, generate a Mandelbrot image and set it to several VipPlotSpectrogram,
/// Compute histogram inside ROI, compute time trace inside ROI, and axtract values along polyline
class MandelbrotGen : public QThread
{
public:
	QList<VipImageArea2D*> areas;
	QList<Histogram> hist;
	QList<Poly> poly;
	QList<TimeTrace> traces;
	double offsetX ;
	double offsetY ;
	double zoom;
	int width, height;
	Mandelbrot gen;
	bool stop_thread;

	MandelbrotGen(const QList<VipImageArea2D*> _areas, const QList<Histogram>& _hist, const QList<Poly>& _poly, const QList<TimeTrace>& _traces,
		int w, int h, int max = 0)
	  : areas(_areas)
	  , hist(_hist)
	  , poly(_poly)
	  , traces(_traces)
	  , offsetX(-0.745917)
	  , offsetY(0.09995)
	  , zoom(0.004)
	  , width(w)
	  , height(h)
	  , gen(max)
	  , stop_thread(false)
	{
	}
	~MandelbrotGen() 
	{ 
		stop();
	}
	void stop() 
	{ 
		stop_thread = true;
		this->wait();
	}
	virtual void run() 
	{
		stop_thread = false;
		while (!stop_thread) 
		{
			// Generate Mandelbrot image
			VipNDArrayType<int> img(vip_vector(height, width));
			gen.updateImage(zoom, offsetX, offsetY, VipNDArrayTypeView<int>(img));

			// Set image to all spectrograms
			for (int i=0; i < areas.size(); ++i)
				areas[i]->spectrogram()->setData(QVariant::fromValue(VipNDArray(img)));

			// compute histograms and set it to all VipPlotSpectrogram
			for (int i = 0; i < hist.size(); ++i) {
				VipIntervalSampleVector h = hist[i].shape.histogram(50, img);
				hist[i].hist->setRawData(h);
			}
			// compute polylines and set it to all VipPlotCurve
			for (int i = 0; i < poly.size(); ++i) {
				VipPointVector v = poly[i].shape.polyline(img);
				poly[i].curve->setRawData(v);
			}
			// compute time traces
			for (int i = 0; i < traces.size(); ++i) {
				VipShapeStatistics v = traces[i].shape.statistics(VipNDArray(img),QPoint(),nullptr,VipShapeStatistics::Mean);

				// Update VipPlotCurve content,
				// and only keep the last 10s
				qint64 ms_time = QDateTime::currentMSecsSinceEpoch();
				VipPointVector vec = traces[i].curve->rawData();
				vec.push_back(VipPoint(ms_time, v.average));
				
				int start = 0;
				if (start < vec.size() && (vec.last().x() - vec[start].x()) > 10000LL)
					start++;
				if (start)
					vec = vec.mid(start);
				traces[i].curve->setRawData(vec);
			}

			// update zoom parameter for next Mandelbrot image
			zoom *= 0.96;
			if (zoom <2.38339e-13)
				zoom = 0.004;
		}
	}
};

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

	// set tool tip text for the spectrogram
	area->spectrogram()->setToolTipText("<b>X</b>: #avalue0%i<br><b>Y</b>: #avalue1%i<br><b>Value</b>: #value");
	// antialiazed rendering
	area->spectrogram()->setRenderHints(QPainter::Antialiasing);
	// ignore the mouse event in order to forward them to the higher level items (in this case VipPlotShape)
	area->spectrogram()->setItemAttribute(VipPlotItem::IgnoreMouseEvents, true);

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
	w.setRenderingMode(VipBaseGraphicsView::OpenGLThread);

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
	VipPlotHistogram* h = new VipPlotHistogram();
	h->setAxes(hist->bottomAxis(), hist->leftAxis(),VipCoordinateSystem::Cartesian);
	h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
	h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));

	VipPlotArea2D* polyline = new VipPlotArea2D();
	setup_plot_area(polyline);
	polyline->setTitle("<b>Values along polyline");
	VipPlotCurve* p = new VipPlotCurve();
	p->setAxes(polyline->bottomAxis(), polyline->leftAxis(), VipCoordinateSystem::Cartesian);
	p->boxStyle().setBorderPen(QPen(QColor(0x0178BB),1.5));

	VipPlotArea2D* trace = new VipPlotArea2D();
	setup_plot_area(trace);
	trace->setTitle("<b>Time trace of the macimum value inside Region Of Intereset (ROI)");
	trace->titleAxis()->setVisible(true);
	trace->bottomAxis()->setOptimizeFromStreaming(true);
	
	VipTimeToText* vt = new VipTimeToText("ss", VipTimeToText::MillisecondsSE, VipTimeToText::DifferenceValue);
	vt->setAdditionalFormat("<b>dd.MM.yyyy<br>hh:mm:ss");
	VipTextStyle st;
	st.setAlignment(Qt::AlignRight | Qt::AlignBottom);
	trace->bottomAxis()->scaleDraw()->setAdditionalTextStyle(st);
	VipFixedScaleEngine* engine = new VipFixedScaleEngine(vt);
	trace->bottomAxis()->scaleDraw()->setValueToText(vt);
	trace->bottomAxis()->setScaleEngine(engine);

	trace->topAxis()->setVisible(false);
	trace->rightAxis()->setVisible(false);
	VipPlotCurve* t = new VipPlotCurve();
	t->setAxes(trace->bottomAxis(), trace->leftAxis(), VipCoordinateSystem::Cartesian);
	t->boxStyle().setBorderPen(QPen(QColor(0x0178BB),1.5));

	grid->addItem(hist, height, 0);
	grid->addItem(polyline, height, 1);
	grid->addItem(trace, height, 2);

	w.widget()->setLayout(grid);
	
	w.resize(1000, 500);
	w.showMaximized();


	MandelbrotGen gen(areas, 
		QList<Histogram>() << Histogram{rect->rawData(),h}, 
		QList<Poly>() << Poly{poly->rawData(),p}, 
		QList<TimeTrace>() << TimeTrace{ rect->rawData(), t },
		640, 420,383);
	gen.start();

	return app.exec();
}