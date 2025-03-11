
 #include "VipPlotWidget2D.h"

#include <qapplication.h>
#include <qgraphicslinearlayout.h>

#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());
 	QApplication app(argc, argv);

 	VipMultiGraphicsView w;

 	// Create 2 cartesian plotting area and a polar one
 	VipPlotArea2D* area1 = new VipPlotArea2D();
 	VipPlotArea2D* area2 = new VipPlotArea2D();
 	VipPlotPolarArea2D* area3 = new VipPlotPolarArea2D();

 	// Set the scale of one cartesian area, and aligned it vertically with the other
 	area2->leftAxis()->setScale(10000, 100000);
 	area1->setAlignedWith(area2, Qt::Vertical);

 	// Stack areas vertically in a QGraphicsLinearLayout
 	QGraphicsLinearLayout* lay = new QGraphicsLinearLayout(Qt::Vertical);
 	w.widget()->setLayout(lay);
 	lay->addItem(area1);
 	lay->addItem(area2);
 	lay->addItem(area3);

 	w.resize(500, 1000);
 	w.show();
 	return app.exec();
 }