
#include <qapplication.h>
#include <cmath>
#include "VipMultiPlotWidget2D.h"
#include "VipColorMap.h"
#include "VipPlotCurve.h"
#include "VipLegendItem.h"
///
void format_legend(VipLegend* l)
{
	// Format an inner legend

	// Internal border margin
	l->setMargins(2);
	// Maximum number of columns
	l->setMaxColumns(1);
	// Draw light box around the legend
	l->boxStyle().setBorderPen(Qt::lightGray);
	// Semi transparent background
	l->boxStyle().setBackgroundBrush(QBrush(QColor(255, 255, 255, 200)));
}


int main(int argc, char** argv)
{
	QApplication app(argc, argv);

	// Create the VipVMultiPlotArea2D, and set it to a VipPlotWidget2D
	VipVMultiPlotArea2D* area = new VipVMultiPlotArea2D();
	VipPlotWidget2D w;
	w.setArea(area);

	// Enable zooming/panning
	area->setMousePanning(Qt::RightButton);
	area->setMouseWheelZoom(true);

	// Add small margin around the plot area
	area->setMargins(VipMargins(10, 10, 10, 10));

	// Insert a new left axis at the top.
	// The VipVMultiPlotArea2D will take care of adding the corresponding right and bottom axes.
	area->setInsertionIndex(1);
	area->addScale(new VipAxisBase(VipAxisBase::Left), true);

	// Create an inner legend for the 2 areas
	area->addInnerLegend(new VipLegend(), area->leftMultiAxis()->at(0), Qt::AlignTop | Qt::AlignLeft, 10);
	format_legend(area->innerLegend(0));
	area->addInnerLegend(new VipLegend(), area->leftMultiAxis()->at(1), Qt::AlignTop | Qt::AlignLeft, 10);
	format_legend(area->innerLegend(1));

	// Hide the global legend located at the very bottom of the area
	area->legend()->setVisible(false);

	area->setDefaultLabelOverlapping(true);

	// Color palette used to give a unique color to each curve
	VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);

	// Cos curve on the top area
	VipPlotCurve* c_cos = new VipPlotCurve("cos");
	c_cos->setMajorColor(palette.color(0));
	c_cos->setFunction([](double x) { return std::cos(x); }, VipInterval(-M_PI, M_PI));
	c_cos->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(1), VipCoordinateSystem::Cartesian);

	// Sin curve on the top area
	VipPlotCurve* c_sin = new VipPlotCurve("sin");
	c_sin->setMajorColor(palette.color(1));
	c_sin->setFunction([](double x) { return std::sin(x); }, VipInterval(-M_PI, M_PI));
	c_sin->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(1), VipCoordinateSystem::Cartesian);

	// Atan curve on the bottom area
	VipPlotCurve* c_atan = new VipPlotCurve("atan");
	c_atan->setMajorColor(palette.color(2));
	c_atan->setFunction([](double x) { return std::atan(x); }, VipInterval(-M_PI, M_PI));
	c_atan->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(0), VipCoordinateSystem::Cartesian);

	// Tanh curve on the bottom area
	VipPlotCurve* c_tanh = new VipPlotCurve("tanh");
	c_tanh->setMajorColor(palette.color(3));
	c_tanh->setFunction([](double x) { return std::tanh(x); }, VipInterval(-M_PI, M_PI));
	c_tanh->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(0), VipCoordinateSystem::Cartesian);

	w.resize(500, 500);
	w.show();
	return app.exec();
}
