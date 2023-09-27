
#include <qapplication.h>
#include <cmath>
#include <qthread.h>
#include <QGraphicsLinearLayout>
#include "VipPlotWidget2D.h"
#include "VipPolygon.h"
#include "VipPlotShape.h"
#include "VipSymbol.h"
#include "VipShapeDevice.h"
#include "VipToolTip.h"

/// @brief Create a star polygon
QPolygonF createStar(const QPointF & center, double width)
{
	VipShapeDevice dev;
	dev.setDrawPrimitives(VipShapeDevice::All);
	VipSymbol sym(VipSymbol::Star2);
	sym.setCachePolicy(VipSymbol::NoCache);
	sym.setSize(QSizeF(width, width));
	QPainter p(&dev);
	sym.drawSymbol(&p, center );
	return dev.shape().toFillPolygon();
}
/// @brief Create a rectangular polygon
QPolygonF createRect(const QRectF& r) 
{
	return QPolygonF() << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft() << r.topLeft();
}
/// @brief Create an ellipsoide polygon
QPolygonF createEllipse(const QRectF& r)
{
	QPainterPath ellipse;
	ellipse.addEllipse(r);
	return ellipse.toFillPolygon();
}

/// @brief Create a scene model of 4 shapes which states depend on advance1 [0,1] and advance2 [0,1] and using polygon interpolation.
/// The 4 shapes are spread on 2 groups.
VipSceneModel createSceneModel(double advance1, double advance2) 
{
	static QPolygonF r1_src = createRect(QRectF(0.5, 0.5, 10, 10));
	static QPolygonF r1_dst = createStar(QPointF(5, 5), 30);

	static QPolygonF _r1_src = createRect(QRectF(20, 5, 15, 15));
	static QPolygonF _r1_dst = createStar(QPointF(25, 10), 20);

	static QPolygonF r2_src = createEllipse(QRectF(40, 40, 15, 15));
	static QPolygonF r2_dst = createStar(QPointF(47, 47), 40);

	static QPolygonF _r2_src = createEllipse(QRectF(10, 40, 15, 15));
	static QPolygonF _r2_dst = createStar(QPointF(17, 47), 20);

	QPolygonF poly1 = vipInterpolatePolygons(r1_src, r1_dst, advance1);
	VipShape sh1(poly1);
	sh1.setAttribute("advance", advance1);
	QPolygonF _poly1 = vipInterpolatePolygons(_r1_src, _r1_dst, advance1);
	VipShape _sh1(_poly1);
	_sh1.setAttribute("advance", advance1);
	
	QPolygonF poly2 = vipInterpolatePolygons(r2_src, r2_dst, advance2);
	VipShape sh2(poly2);
	sh2.setAttribute("advance", advance2);
	QPolygonF _poly2 = vipInterpolatePolygons(_r2_src, _r2_dst, advance2);
	VipShape _sh2(_poly2);
	_sh2.setAttribute("advance", advance2);


	VipSceneModel sm;
	sm.add("Group 1" ,sh1);
	sm.add("Group 1", _sh1);
	sm.add("Group 2", sh2);
	sm.add("Group 2", _sh2);

	return sm;
}

/// @brief Thread that generate and set a scene model to a VipPlotSceneModel
class GenSceneModel : public QThread
{
	VipPlotSceneModel* sm;
	bool stop;

public:
	GenSceneModel(VipPlotSceneModel* pl)
	  : QThread(qApp),sm(pl)
	  , stop(false)
	{
	}
	~GenSceneModel() 
	{
		stop = true;
		wait();
	}

	virtual void run() 
	{ 
		double sign1 = 1, sign2 = 1;
		double advance1 = 0, advance2 = 0;
		while (!stop) {
		
			// generate and set the scene model
			VipSceneModel model = createSceneModel(advance1, advance2);
			sm->setSceneModel(model);

			// update the advance variables
			advance1 += 0.005 * sign1;
			advance2 += 0.01 * sign2;
			if (advance1 > 1) {
				advance1 = 1;
				sign1 = -1;
			}
			else if (advance1 < 0) {
				advance1 = 0;
				sign1 = 1;
			}
			if (advance2 > 1) {
				advance2 = 1;
				sign2 = -1;
			}
			else if (advance2 < 0) {
				advance2 = 0;
				sign2 = 1;
			}

			QThread::msleep(5);
		}
	}
};

/// @brief Generate a dark theme stylesheet
QString darkStyleSheet()
{
	QString st = 
		     "VipBaseGraphicsView {"
		     "qproperty-backgroundColor : #2F2F2F;"
		     "}"
		     "QToolTip {"
		     "background : #2F2F2F;"
		     "border : 1px solid #4D4D50;"
		     "color : #F1F1F1;"
		     "}"
		     "VipTipContainer {"
		     "	background-color: 2F2F2F;"
		     "	color : white;"
		     "}"
		     "VipTipLabel {"
		     "	background: #2F2F2F;"
		     "	border: 1px solid #4D4D50;"
		     "	color : #F1F1F1;"
		     "}";
	return st;
}

QString darkPlotStyleSheet()
{
	QString st = "VipPlotShape {"
		     "border-width: 1.5;"
		     "color : white;"
		     "title-color: white;"
		     "}"
		     "VipPlotSceneModel {"
		     "border-width: 1.5;"
		     "color : white;"
		     "title-color: white;"
		     "}"
		     "VipAbstractScale {"
		     "label-color: white;"
		     "title-color: white;"
		     "pen: white;"
		     "}";
		     
	return st;
}

int main(int argc, char** argv)
{


	QApplication app(argc, argv);

	// grid of 2 horizontal plot areas
	VipMultiGraphicsView w;
	w.setStyleSheet(darkStyleSheet());
	w.widget()->setStyleSheet(darkPlotStyleSheet());

	QGraphicsLinearLayout* lay = new QGraphicsLinearLayout(Qt::Horizontal);
	w.widget()->setLayout(lay);

	{
		// create plot area that will contain the dynamic scene model
		VipPlotArea2D* area = new VipPlotArea2D();
		area->setTitle("<b>Dynamic scene model");

		// add tool tip
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
		area->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

		// add a VipPlotSceneModel using VipPlotSceneModel::UniqueItem (considered as a unique item)
		VipPlotSceneModel* plot = new VipPlotSceneModel();
		plot->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
		plot->setCompositeMode(VipPlotSceneModel::UniqueItem);

		// define color for each group
		VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);
		QColor c0 = palette.color(0);
		c0.setAlpha(150);
		QColor c1 = palette.color(1);
		c1.setAlpha(150);


		// set pen a nd brush for each group
		plot->setBrush("Group 1", QBrush(c0));
		plot->setPen("Group 1", QPen(c0.lighter()));
		plot->setAdjustTextColor(QString(), false);

		plot->setBrush("Group 2", QBrush(c1));
		plot->setPen("Group 2", QPen(c1.lighter()));

		// set text to display and tool tip for all groups
		plot->setText(QString(), "#group:\n\tadvance: #padvance");
		plot->setToolTipText(QString(), "#group:<br>advance: #padvance");
		plot->setItemAttribute(VipPlotItem::HasToolTip, true);

		// set the text style for all groups
		VipTextStyle st;
		st.setAlignment(Qt::AlignTop | Qt::AlignLeft);
		plot->setTextStyle(st);

		// fix the scale and disable auto scaling
		area->bottomAxis()->setScale(-20, 80);
		area->bottomAxis()->setAutoScale(false);
		area->leftAxis()->setScale(-20, 80);
		area->leftAxis()->setAutoScale(false);

		// refresh the tool tip on scene model update
		//QObject::connect(plot, SIGNAL(sceneModelChanged(const VipSceneModel&)), area->plotToolTip(), SLOT(refresh()));

		lay->addItem(area);

		// launch thread
		GenSceneModel* gen = new GenSceneModel(plot);
		gen->start();
	}
	{
		// create plot area that will contain the static scene model
		VipPlotArea2D* area = new VipPlotArea2D();
		area->setTitle("<b>Static editable scene model");
		// setup tooltip
		area->setPlotToolTip(new VipToolTip());
		area->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);

		// create VipPlotSceneModel, set its mode to Resizable (the compositeMode() is already set to Aggregate):
		// each shape can be manually moved/resized/rotated
		VipPlotSceneModel* plot = new VipPlotSceneModel();
		plot->setAxes(area->bottomAxis(), area->leftAxis(), VipCoordinateSystem::Cartesian);
		plot->setMode(VipPlotSceneModel::Resizable);

		// all the rest is similar to above area

		VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);
		QColor c0 = palette.color(0);
		c0.setAlpha(150);
		QColor c1 = palette.color(1);
		c1.setAlpha(150);


		plot->setBrush("Group 1", QBrush(c0));
		plot->setPen("Group 1", QPen(c0.lighter()));
		plot->setAdjustTextColor(QString(), false);

		plot->setBrush("Group 2", QBrush(c1));
		plot->setPen("Group 2", QPen(c1.lighter()));

		plot->setResizerPen(QString(),QPen(Qt::white));

		plot->setText(QString(), "#group:\n\tadvance: #padvance");
		plot->setToolTipText(QString(), "#group:<br>advance: #padvance");
		plot->setItemAttribute(VipPlotItem::HasToolTip, true);

		VipTextStyle st;
		st.setAlignment(Qt::AlignTop | Qt::AlignLeft);
		plot->setTextStyle(st);
	
		plot->setSceneModel(createSceneModel(0,0));

		area->bottomAxis()->setScale(-20, 80);
		area->bottomAxis()->setAutoScale(false);
		area->leftAxis()->setScale(-20, 80);
		area->leftAxis()->setAutoScale(false);

		QObject::connect(plot, SIGNAL(sceneModelChanged(const VipSceneModel&)), area->plotToolTip(), SLOT(refresh()));

		lay->addItem(area);

		area->bottomAxis()->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
		area->leftAxis()->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
		area->rightAxis()->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	}

	w.setStyleSheet(darkStyleSheet());
	w.resize(1000, 500);

	w.show();

	return app.exec();
}
