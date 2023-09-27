
#include <qapplication.h>
#include <QGraphicsLinearLayout>
#include <qboxlayout.h>
#include <QGraphicsLinearLayout>

#include "VipPlotWidget2D.h"
#include "VipToolTip.h"
#include "VipPlotScatter.h"
#include "VipPlotGrid.h"


/// @brief Generate scatter vector of count points along the line going from start to end
VipScatterPointVector generateScatter(int count, const QPointF & start, const QPointF & end) 
{ 
	if (count < 2)
		count = 2;

	QLineF line(start, end);
	double step = 1. / (count - 1);
	VipScatterPointVector res;

	for (int i = 0; i < count; ++i) {
		VipScatterPoint p;
		p.position = line.pointAt(step * i);
		p.position += VipPoint((rand() % 16) * 1, (rand() % 16) * 1);
		p.value = rand() % 32;
		res.push_back(p);
	}
	return res;
}

/// @brief Generate a plotting area containing 2 scatter plots
QPair<VipPlotScatter*, VipPlotScatter*> generateScatterAndArea(const QString& title,
							       const VipScatterPointVector& v1,
							       const VipScatterPointVector & v2) //, int count, const QPointF& start, const QPointF& end)
{
	// Use a color palette to affect a color to each scatter plot
	VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);

	QColor c0 = palette.color(0);
	c0.setAlpha(180);
	QColor c1 = palette.color(1);
	c1.setAlpha(180);

	// Create area, hide top and right axis, activate tool tip
	VipPlotArea2D* area = new VipPlotArea2D();
	area->rightAxis()->setVisible(false);
	area->topAxis()->setVisible(false);
	VipText t = title;
	t.setRenderHints(QPainter::HighQualityAntialiasing|QPainter::TextAntialiasing);
	area->setTitle(t);
	area->setPlotToolTip(new VipToolTip());
	area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
	area->plotToolTip()->setOverlayPen(QPen(Qt::magenta));

	// first scatter plot
	VipPlotScatter* sc1 = new VipPlotScatter("Scatter plot 1");
	sc1->setRawData(v1);
	sc1->symbol().setSize(QSizeF(10, 10));
	sc1->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
	sc1->setToolTipText("Value: #value");

	// second scatter plot
	VipPlotScatter* sc2 = new VipPlotScatter("Scatter plot 2");
	sc2->setRawData(v2);
	sc2->symbol().setSize(QSizeF(10, 10));
	sc2->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);

	// set pen and brush
	sc1->setPen(QPen(c0.lighter()));
	sc2->setPen(QPen(c1.lighter()));

	sc1->setBrush(QBrush(c0));
	sc2->setBrush(QBrush(c1));


	return QPair<VipPlotScatter*, VipPlotScatter*>(sc1, sc2);
}


#include <qsurface.h>

int main(int argc, char** argv)
{
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);

	// Display 4 scatter plot areas horizontally
	VipMultiGraphicsView w;

	QGraphicsLinearLayout* lay = new QGraphicsLinearLayout(Qt::Horizontal);
	w.widget()->setLayout(lay);

	VipScatterPointVector v1 = generateScatter(10, QPointF(-10, -10), QPointF(10, 10));
	VipScatterPointVector v2 = generateScatter(10, QPointF(-10, -10), QPointF(10, 10));

	{
		// generate plot area with 2 scatter plots of fixed size
		auto pair = generateScatterAndArea("<b>Scatter plot with fixed symbol size",v1,v2);
		VipPlotArea2D* area = static_cast<VipPlotArea2D*>(pair.first->area());

		pair.first->symbol().setStyle(VipSymbol::Rect);
		pair.second->symbol().setStyle(VipSymbol::Ellipse);

		lay->addItem(area);
	}
	{

		// generate plot area with 2 scatter plots of variable size and displaying a text
		auto pair = generateScatterAndArea("<b>Scatter plot with variable symbol size and text", v1,v2);
		VipPlotArea2D* area = static_cast<VipPlotArea2D*>(pair.first->area());

		pair.first->symbol().setStyle(VipSymbol::Rect);
		pair.second->symbol().setStyle(VipSymbol::Ellipse);

		pair.first->setUseValueAsSize(true);
		pair.second->setUseValueAsSize(true);

		pair.first->setText("Value: #value");
		pair.second->setText("Value: #value");

		lay->addItem(area);
	}
	{
		// generate plot area with 2 scatter plots of variable size and displaying a text with a color map
		auto pair = generateScatterAndArea("<b>Scatter plot with variable symbol size, text and color map", v1, v2);
		VipPlotArea2D* area = static_cast<VipPlotArea2D*>(pair.first->area());

		pair.first->symbol().setStyle(VipSymbol::Rect);
		pair.second->symbol().setStyle(VipSymbol::Ellipse);

		pair.first->setUseValueAsSize(true);
		pair.second->setUseValueAsSize(true);

		pair.first->setText("Value: #value");
		pair.second->setText("Value: #value");

		pair.first->setPen(QPen());
		pair.second->setPen(QPen());

		auto * map = area->createColorMap(VipAxisBase::Right, VipInterval(), VipLinearColorMap::createColorMap(VipLinearColorMap::Sunset));
		pair.first->setColorMap(map);
		pair.second->setColorMap(map);

		lay->addItem(area);
	}
	{
		// generate plot area with 1 scatter plot representing rectangles arranged in a grid, like a spectrogram
		VipPlotArea2D* area = new VipPlotArea2D();
		area->rightAxis()->setVisible(false);
		area->topAxis()->setVisible(false);
		area->setTitle("<b>Image like scatter plot");
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
		area->plotToolTip()->setOverlayPen(QPen(Qt::magenta));
		area->setMousePanning(Qt::RightButton);
		area->setMouseWheelZoom(true);

		VipScatterPointVector vec(10000);
		for (int i = 0; i < 10000; ++i) {
			int h = i / 100;
			int w = i % 100;
			vec[i].position = VipPoint(w + 0.5, h + 0.5);
			vec[i].value = i;
		}

		VipPlotScatter* sc = new VipPlotScatter("Scatter plot 1");
		sc->setRawData(vec);
		sc->symbol().setSize(QSizeF(1, 1));
		sc->setSizeUnit(VipPlotScatter::AxisUnit);
		sc->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
		sc->setToolTipText("Value: #value");
		sc->setPen(QPen(Qt::black,0.1));

		lay->addItem(area);
		auto* map = area->createColorMap(VipAxisBase::Right, VipInterval(), VipLinearColorMap::createColorMap(VipLinearColorMap::Sunset));
		sc->setColorMap(map);
	}
	w.resize(1000, 500);
	w.show();

	return app.exec();
}
