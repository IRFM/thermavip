#include "VipPlotWidget2D.h"
#include "VipPlotGrid.h"
#include "VipPlotBarChart.h"
#include "VipLegendItem.h"
#include "VipToolTip.h"
#include <qapplication.h>


#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());

	QApplication app(argc, argv);
	///
	// Create the plot widget
	VipPlotWidget2D w;
	w.setMouseTracking(true);
	w.area()->setMouseWheelZoom(true);
	w.area()->setMousePanning(Qt::RightButton);
	///
	// Hide top and right axes
	w.area()->rightAxis()->setVisible(false);
	w.area()->topAxis()->setVisible(false);
	// Hide grid
	w.area()->grid()->setVisible(false);
	///
	// Make the legend expanding vertically
	w.area()->legend()->setExpandingDirections(Qt::Vertical);
	// Add a margin around the plotting area
	w.area()->setMargins(20);

	// setup tool tip
	w.area()->setPlotToolTip(new VipToolTip());
	w.area()->plotToolTip()->setDisplayFlags( VipToolTip::ItemsToolTips);
	w.area()->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

	///
	// Define a text style with bold font used for the axes
	VipTextStyle st;
	QFont f = st.font();
	f.setBold(true);
	st.setFont(f);
	///
	// Make the majo ticks point toward the axis labels
	w.area()->leftAxis()->scaleDraw()->setTicksPosition(VipAbstractScaleDraw::TicksInside);
	// Set custom labels for the left axis
	w.area()->leftAxis()->scaleDraw()->setCustomLabels(QList<VipScaleText>() << VipScaleText("Cartier", 1) << VipScaleText("Piaget", 2) << VipScaleText("Audemars Piguet", 3) << VipScaleText("Omega",
	4)
										 << VipScaleText("Patek Philippe", 5) << VipScaleText("Rolex", 6));
	///
	// The bottom axis uses a custom label text to add a '$' sign before the scale value
	w.area()->bottomAxis()->scaleDraw()->setCustomLabelText("$#value", VipScaleDiv::MajorTick);
	///
	// Set the text style with bold font to both axes
	w.area()->leftAxis()->setTextStyle(st);
	w.area()->bottomAxis()->setTextStyle(st);
	///
	// Create the vector of VipBar
	QVector<VipBar> bars;
	bars << VipBar(1, QVector<double>() << 290 << 550 << 900) << VipBar(2, QVector<double>() << 430 << 600 << 220) << VipBar(3, QVector<double>() << 900 << 622 << 110)
		 << VipBar(4, QVector<double>() << 470 << 342 << 200) << VipBar(5, QVector<double>() << 400 << 290 << 150) << VipBar(6, QVector<double>() << 500 << 1000 << 1200);
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
	c->setBarWidth(20, VipPlotBarChart::ItemUnit);
	// Set the spacing between bars in item's unit
	c->setSpacing(1, VipPlotBarChart::ItemUnit);
	///
	// Set the text to be draw on top of each bar
	c->setText(VipText("$#value").setTextPen(QPen(Qt::white)));
	// Set the text position: inside each bar
	c->setTextPosition(Vip::Inside);
	// Align the text to the left of each bar
	c->setTextAlignment(Qt::AlignLeft);
	// Set axes (to display horizontal bars, X axis must be the left one)
	c->setAxes(w.area()->leftAxis(), w.area()->bottomAxis(), VipCoordinateSystem::Cartesian);

	c->setToolTipText("#licon <b>#title</b>: #value");

	///
	w.show();
return app.exec();
}