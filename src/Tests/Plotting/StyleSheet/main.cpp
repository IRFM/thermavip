
#include <qapplication.h>
#include <QGraphicsLinearLayout>
#include <qboxlayout.h>

#include "VipPlotWidget2D.h"
#include "VipPlotCurve.h"
#include "VipToolTip.h"
#include "VipPlotBarChart.h"
#include "VipPieChart.h"
#include "VipPlotGrid.h"
#include "VipLegendItem.h"
#include "VipPolarAxis.h"
#include "VipPlotHistogram.h"
#include "VipPlotSpectrogram.h"


/// @brief Generate a gray theme stylesheet
QString grayStyleSheet()
{
	return "VipAbstractPlotArea {background: white; colormap: jet; colorpalette: set1;}"
	       "VipPlotGrid {major-pen[0]: 1px solid white; major-pen[1]: 1px solid white; above: false;}"
	       "VipPlotCanvas {background: #F2F2F2; }";
}

/// @brief Generate a dark theme stylesheet
QString darkStyleSheet()
{
	return "VipMultiGraphicsWidget { background: #474747}"
	       "VipAbstractPlotArea { title-color: white; background: #383838; colorpalette: 'random:120'; tool-tip-selection-border: yellow; "
	       "tool-tip-selection-background: rgba(255,255,255,30); }"
	       "VipPlotItem { title-color: white; color: white;}"
	       "VipPlotCurve {border-width: 2; }"
	       "VipAbstractScale {title-color: white; label-color: white; pen: white;}"
	       // Hide right and top (not title) scales
	       "VipAbstractScale:top:!title {visible:false;}"
	       "VipAbstractScale:right {visible:false;}"
	       // Show right and top scales for spectrograms
	       "VipImageArea2D > VipAbstractScale:right {visible:true;}"
	       "VipImageArea2D > VipAbstractScale:top {visible:true;}"
	       "VipAbstractScale:title {margin: 10;}"
	       "VipAxisColorMap {color-bar-width:10;}"
	       "VipAxisColorMap > VipSliderGrip {handle-distance:0;}"
	       "VipPieChart {to-text-border: white; }"
	       "VipPlotGrid { major-pen: 1px dot white;  }"
	       "VipPlotSpectrogram {colormap: sunset;}"
	       "VipLegend { color: white; alignment:hcenter|vcenter; expanding-directions:vertical;}";

}


double norm_pdf(double x, double mu, double sigma)
{
	return 1.0 / (sigma * sqrt(2.0 * M_PI)) * exp(-(pow((x - mu) / sigma, 2) / 2.0));
}


VipMultiGraphicsView* createPlotWidget(const QString& title)
{
	// create a VipMultiGraphicsView and add 5 different plotting areas
	VipMultiGraphicsView* w = new VipMultiGraphicsView();
	
	QGraphicsLinearLayout* lay = new QGraphicsLinearLayout(Qt::Horizontal);
	w->widget()->setLayout(lay);

	/// Add curves
	{ 

		VipPlotArea2D* area = new VipPlotArea2D();
		area->setTitle("<b>Curves");
		
		area->leftAxis()->setTitle(title);

		// create 3 curves
		VipPointVector c1, c2, c3;
		c1 << VipPoint(0.5, 0.5) << VipPoint(2, 1.5) << VipPoint(3, 3) << VipPoint(4, 3.5) << VipPoint(5.5, 6);
		for (int i = 0; i < c1.size(); ++i) {
			c2.push_back(VipPoint(c1[i].x(), c1[i].y() * 1.2) + VipPoint(0, 2));
			c3.push_back(VipPoint(c1[i].x(), c1[i].y() * 1.4) + VipPoint(0, 4));
		}

		VipPlotCurve* _c1 = new VipPlotCurve("Curve 1");
		_c1->setRawData(c1);

		VipPlotCurve* _c2 = new VipPlotCurve("Curve 2");
		_c2->setRawData(c2);

		VipPlotCurve* _c3 = new VipPlotCurve("Curve 3");
		_c3->setRawData(c3);

		_c1->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
		_c2->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
		_c3->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);

		area->bottomAxis()->setTitle("<b>X axis");

		lay->addItem(area);
	}
	/// Add bar chart
	{
		// Create the plot widget
		VipPlotArea2D* area = new VipPlotArea2D();
		area->setTitle("<b>Bar chart");

		area->setMouseWheelZoom(true);
		area->setMousePanning(Qt::RightButton);
		///
		// Hide top and right axes
		area->rightAxis()->setVisible(false);
		area->topAxis()->setVisible(false);
		// Hide grid
		area->grid()->setVisible(false);
		///
		// Make the legend expanding vertically
		area->legend()->setExpandingDirections(Qt::Vertical);
		// Add a margin around the plotting area
		area->setMargins(20);

		// setup tool tip
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
		area->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

		///
		// Define a text style with bold font used for the axes
		VipTextStyle st;
		QFont f = st.font();
		f.setBold(true);
		st.setFont(f);
		///
		// Make the majo ticks point toward the axis labels
		area->leftAxis()->scaleDraw()->setTicksPosition(VipAbstractScaleDraw::TicksInside);
		// Set custom labels for the left axis
		area->leftAxis()->scaleDraw()->setCustomLabels(QList<VipScaleText>() << VipScaleText("Cartier", 1) << VipScaleText("Piaget", 2) 
											 << VipScaleText("Omega", 3) <<  VipScaleText("Rolex", 4));
		///
		// The bottom axis uses a custom label text to add a '$' sign before the scale value
		area->bottomAxis()->scaleDraw()->setCustomLabelText("$#value", VipScaleDiv::MajorTick);
		///
		// Set the text style with bold font to both axes
		area->leftAxis()->setTextStyle(st);
		area->bottomAxis()->setTextStyle(st);
		///
		// Create the vector of VipBar
		QVector<VipBar> bars;
		bars << VipBar(1, QVector<double>() << 290 << 550 << 900) << VipBar(2, QVector<double>() << 430 << 600 << 220) 
		     << VipBar(3, QVector<double>() << 470 << 342 << 200) << VipBar(4, QVector<double>() << 500 << 1000 << 1200);
		///
		// Create the bar chart
		VipPlotBarChart* c = new VipPlotBarChart();
		// Set the vector of VipBar
		c->setRawData(bars);
		// Set the name for each bar (displayed in the legend)
		c->setBarNames(VipTextList() << "Q1"
					     << "Q2"
					     << "Q3");
		///
		// Set the bar width in item's unit
		c->setBarWidth(15, VipPlotBarChart::ItemUnit);
		// Set the spacing between bars in item's unit
		c->setSpacing(1, VipPlotBarChart::ItemUnit);
		///
		// Set the text to be draw on top of each bar
		c->setText(VipText("$#value"));
		// Set the text position: inside each bar
		c->setTextPosition(Vip::Inside);
		// Align the text to the left of each bar
		c->setTextAlignment(Qt::AlignLeft);
		// Set axes (to display horizontal bars, X axis must be the left one)
		c->setAxes(area->leftAxis(), area->bottomAxis(), VipCoordinateSystem::Cartesian);

		c->setToolTipText("#licon <b>#title</b>: #value");

		lay->addItem(area);
	}
	/// Add pie chart
	{
		VipPlotPolarArea2D* area = new VipPlotPolarArea2D();
		area->setTitle("<b>Pie chart");
		// set title and show title axis

		// Configure the tool tip to only display the underlying item's tool tip
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
		area->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

		///
		// Invert polar scale (not mandatory)
		area->polarAxis()->setScaleInverted(true);
		///
		// hide grid
		area->grid()->setVisible(false);
		///
		// get axes as a list
		QList<VipAbstractScale*> scales;
		area->standardScales(scales);
		// hide axes
		scales[0]->setVisible(false);
		scales[1]->setVisible(false);
		///
		// create pie chart
		VipPieChart* ch = new VipPieChart();
		///
		// Set the pie chart bounding pie in axis coordinates
		ch->setPie(VipPie(0, 100, 20, 100));
		
		// legend only draws the item background
		ch->setLegendStyle(VipPieItem::BackgroundOnly);
		///
		// clip item drawinf to its pie, not mandatory
		ch->setClipToPie(true);
		///
		// Set items text to be displayed inside each pie, combination of its title and value
		ch->setText("#title\n#value%.2f");

		/// Set the tool tip to be displayed: icon + title + value
		ch->setToolTipText("#licon<b>#title</b>: #value%2.f");

		// set spacing between pie items
		ch->setSpacing(3);
		///
		// set the values
		ch->setValues(QVector<double>() << 18.47 << 17.86 << 4.34 << 3.51 << 2.81 << 2.62 << 2.55 << 2.19 << 1.91 << 1.73 << 1.68 << 40.32);
		// set the item's titles
		ch->setTitles(QVector<VipText>() << "China"
						 << "India"
						 << "U.S"
						 << "Indonesia"
						 << "Brazil"
						 << "Pakistan"
						 << "Nigeria"
						 << "Bangladesh"
						 << "Russia"
						 << "Mexico"
						 << "Japan"
						 << "Other");
		///
		// set the pie chart axes
		ch->setAxes(scales, VipCoordinateSystem::Polar);
		///
		// highlight the highest country by giving it an offset to the center
		VipPie pie = ch->pieItemAt(0)->rawData();
		ch->pieItemAt(0)->setRawData(pie.setOffsetToCenter(10));
		///

		lay->addItem(area);
	}
	/// Add histogram
	{
		
		VipPlotArea2D* area = new VipPlotArea2D();
		area->setTitle("<b>Histogram");
		area->setMousePanning(Qt::RightButton);
		area->setMargins(VipMargins(10, 10, 10, 10));
		area->rightAxis()->setVisible(false);
		area->topAxis()->setVisible(false);
		// configure tooltip
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsTitles | VipToolTip::ItemsLegends | VipToolTip::ItemsToolTips);
		area->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

		// generate histogram
		VipIntervalSampleVector hist;
		for (int i = -10; i < 10; ++i) {
			VipIntervalSample s;
			s.interval = VipInterval(i, i + 1);
			s.value = norm_pdf(i, 0, 2) * 5;
			hist.push_back(s);
		}

		VipPlotHistogram* h = new VipPlotHistogram();
		h->setRawData(hist);
		h->setStyle(VipPlotHistogram::Columns);
		h->setText("#value%.2f");
		h->setToolTipText("<b>From</b> #min<br><b>To</b> #max<br><b>Values</b>: #value");
		h->setTextPosition(Vip::XInside);
		h->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);

		lay->addItem(area);
	}
	/// Add spectrogram
	{
		VipImageArea2D* area = new VipImageArea2D();
		area->setTitle("<b>Spectrogram");
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);

		VipNDArrayType<int> img(vipVector(500, 400));
		for (int i = 0; i < img.size(); ++i)
			img[i] = i;
		area->spectrogram()->setData(QVariant::fromValue(VipNDArray(img)));
		area->spectrogram()->setToolTipText("<b>X</b>: #avalue0%i<br><b>Y</b>: #avalue1%i<br><b>Value</b>: #value");
		area->colorMapAxis()->setVisible(true);
		area->setMouseWheelZoom(true);
		area->setMousePanning(Qt::RightButton);

		lay->addItem(area);
	}
	w->resize(1500, 500);
	return w;
}


#include <qsurfaceformat.h>


class RenderWidget : public QWidget, public VipRenderObject
{
public:
	RenderWidget(QWidget* parent = nullptr)
	  : QWidget(parent)
	  , VipRenderObject(this)
	{
	}
};

int main(int argc, char** argv)
{
	// define opengl features if we wish to use it
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);

	auto* w1 = createPlotWidget("<b>No stylesheet");
	auto* w2 = createPlotWidget("<b>Gray stylesheet");
	auto* w3 = createPlotWidget("<b>Dark stylesheet");

	RenderWidget* w = new RenderWidget();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(w1);
	lay->addWidget(w2);
	lay->addWidget(w3);
	w->setLayout(lay);

	w2->widget()->setStyleSheet(grayStyleSheet());
	w3->widget()->setStyleSheet(darkStyleSheet());

	w->resize(1000, 1000);
	w->showMaximized();

	// Test saving functions
	VipRenderObject::saveAsImage(w, "screenshot_with_background.png");
	QColor c = Qt::white;
	VipRenderObject::saveAsImage(w, "screenshot_no_background.png",&c);
	
	return app.exec();
}
