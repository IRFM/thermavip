
#include <qapplication.h>
#include <qline.h>
#include <qthread.h>
#include <cmath>

#include "VipMultiPlotWidget2D.h"
#include "VipColorMap.h"
#include "VipPlotQuiver.h"
#include "VipToolTip.h"


/// @brief Generate 400 rotating arrows with an x/y step of 2 units
/// @return 
VipQuiverPointVector generateQuivers() 
{
	VipQuiverPointVector res;

	double seconds = QDateTime::currentMSecsSinceEpoch() * 1e-3;
	
	int i = 0;
	for (int y=0; y < 20; ++y)
		for (int x = 0; x < 20; ++x, ++i) {
		
			double factor = std::cos(seconds * 1e-2*i);

			VipQuiverPoint p;
			p.value = i * factor;
			p.position = VipPoint(x*2, y*2);

			QLineF line(p.position, p.position + VipPoint((x + y) / 10., 0));

			double angle = factor * 360;
			line.setAngle(angle);
			p.destination = line.p2();

			res.push_back(p);
		}

	return res;
}


class QuiverGenerator : public QThread
{
	VipPlotQuiver* quiver;
	bool stop;

public:
	QuiverGenerator(VipPlotQuiver* p)
	  : quiver(p)
	  , stop(false)
	{
	}
	~QuiverGenerator() { stop = true;
		wait();
	}

protected:
	virtual void run()
	{ 
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		quiver->resetFpsCounter();

		while (!stop) {
			VipQuiverPointVector vec = generateQuivers();
			quiver->setRawData(vec);
			QThread::msleep(5);

			qint64 current = QDateTime::currentMSecsSinceEpoch();
			if (current - st > 1000) {
				printf("rate: %i\n", quiver->fps());
				st = current;
			}
		}
	}
};

#include <qsurface.h>


int main(int argc, char** argv)
{
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0);
	//format.setMajorVersion(3);
	//format.setMinorVersion(3);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);

	// Create the VipVMultiPlotArea2D, and set it to a VipPlotWidget2D
	
	VipPlotWidget2D w;

	//w.area()->setRenderStrategy(VipAbstractPlotArea::OpenGLOffscreen);
	//w.area()->setRenderingThreads(6);
	//w.setOpenGLRendering(true);
	//w.area()->setMaximumFrameRate(2);
	//VipText::setCacheTextWhenPossible(true);
	

	// Enable zooming/panning
	w.area()->setMousePanning(Qt::RightButton);
	w.area()->setMouseWheelZoom(true);
	w.area()->setTitle("<b>Dynamic quivers plot");
	
	w.area()->setPlotToolTip(new VipToolTip());
	w.area()->plotToolTip()->setDisplayFlags(VipToolTip::ItemsToolTips);
	w.area()->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));

	//set the scale manually
	w.area()->leftAxis()->setScale(-5, 45);
	w.area()->leftAxis()->setAutoScale(false);
	w.area()->bottomAxis()->setScale(-5, 45);
	w.area()->bottomAxis()->setAutoScale(false);

	auto* map = w.area()->createColorMap(VipAxisBase::Right, VipInterval(), VipLinearColorMap::createColorMap(VipLinearColorMap::Sunset));
	
	VipPlotQuiver* p = new VipPlotQuiver("Quivers");
	p->setPen(QPen(Qt::blue));
	p->quiverPath().setStyle(VipQuiverPath::EndArrow);
	p->quiverPath().setAngle(VipQuiverPath::End, 30);
	p->quiverPath().setLength(VipQuiverPath::End, 5);
	p->setColorMap(map);
	p->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(),VipCoordinateSystem::Cartesian);
	p->setToolTipText("#value");
	p->setRawData(generateQuivers());
	//p->setText("#value");

	QuiverGenerator gen(p);
	gen.start();

	// update tool tip
	QObject::connect(p, SIGNAL(dataChanged()), w.area()->plotToolTip(), SLOT(refresh()));
	
	w.resize(500, 500);
	w.show();
	return app.exec();
}
