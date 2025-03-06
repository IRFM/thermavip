#include "VipPolarAxis.h"
#include "VipPieChart.h"
#include "VipPlotGrid.h"
#include "VipPlotWidget2D.h"
#include "VipToolTip.h"
#include <qapplication.h>

#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());
	QApplication app(argc, argv);
	///
	VipPlotPolarWidget2D w;

	// set title and show title axis
	w.area()->setTitle("<b>Countries by Proportion of World Population</b>");
	w.area()->titleAxis()->setVisible(true);

	// Configure the tool tip to only display the underlying item's tool tip
	w.area()->setPlotToolTip(new VipToolTip());
	w.area()->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
	w.area()->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

	///
	// Invert polar scale (not mandatory)
	w.area()->polarAxis()->setScaleInverted(true);
	///
	// hide grid
	w.area()->grid()->setVisible(false);
	///
	// get axes as a list
	QList<VipAbstractScale*> scales;
	w.area()->standardScales(scales);
	// hide axes
	scales[0]->setVisible(false);
	scales[1]->setVisible(false);
	///
	// create pie chart
	VipPieChart* ch = new VipPieChart();
	///
	// Set the pie chart bounding pie in axis coordinates
	ch->setPie(VipPie(0, 100, 20, 100));
	///
	// set the pen width for all items
	auto bs = ch -> itemsBoxStyle();
	bs.borderPen().setWidthF(3);
	ch->setItemsBoxStyle(bs);
	///
	// set the pen color palette to always return white color
	ch->setPenColorPalette(VipColorPalette(Qt::white));
	///
	// legend only draws the item background
	ch->setLegendStyle(VipPieItem::BackgroundOnly);
	///
	// clip item drawinf to its pie, not mandatory
	ch->setClipToPie(true);
	///
	// Set items text to be displayed inside each pie, combination of its title and value
	ch->setText("#title\n#value%.2f");

	///Set the tool tip to be displayed: icon + title + value
	ch->setToolTipText("#licon<b>#title</b>: #value%2.f");

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
	w.show();
	return app.exec();
}
