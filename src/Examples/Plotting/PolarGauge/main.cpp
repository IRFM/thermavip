#include "VipPolarWidgets.h"
#include "VipPlotWidget2D.h"
#include "VipPolarAxis.h"
#include "VipPlotMarker.h"

#include <qapplication.h>
#include <qthread.h>

class GaugeThread: public QThread
{
	QPointer<VipPolarValueGauge> gauge;
	bool stop;

public:
	GaugeThread(VipPolarValueGauge* g)
	  : gauge(g)
	  , stop(false)
	{
	}
	~GaugeThread() 
	{ 
		stop = true;
		wait();
	}

protected:

	virtual void run() 
	{
		double value = vipNan();
		double min = gauge->range().minValue();
		double max = gauge->range().maxValue();

		while (!stop) {
			if (VipPolarValueGauge* g = gauge) {
			
			
				if (vipIsNan(value)) {
					value = rand() / (double)RAND_MAX;
					value = min + value * (max - min);
				}
				else {
					double v = rand() / (double)RAND_MAX;
					v -= 0.5;
					v = v * (max - min) * 0.1;
					value += v;
					if (value > max)
						value = max;
					if (value < min)
						value = min;
				}
				
				QMetaObject::invokeMethod(g, "setValue", Qt::QueuedConnection, Q_ARG(double, value));

				QThread::msleep(20);
				
			}
		}
	}
};

#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());
	QApplication app(argc, argv);
	///
	VipPolarValueGauge* widget = new VipPolarValueGauge();

	VipInterval range(0, 1000);

	widget->setTextFormat("<span>%3.0f&#176;</span>");
	widget->setLightSize(8);
	widget->setShadowSize(5);
	widget->setAngles(-10, 180 + 10);
	widget->setRange(range.minValue(), range.maxValue());
	widget->area()->polarAxis()->scaleDraw()->setLabelInterval(VipInterval(range.minValue() + range.width() / 2, range.maxValue()));
	widget->area()->polarAxis()->scaleDraw()->setSpacing(10);
	// set the central text style
	widget->centralText()->setRelativeFontSize(30, 0);
	widget->setTextVerticalPosition(-25);
	VipText bottom = "TEMPERATURE\nLIMIT";
	bottom.setTextPen(QPen(Qt::white));
	widget->bottomText()->setLabel(bottom);
	widget->bottomText()->setSpacing(0);
	widget->bottomText()->setRelativeFontSize(8, 0);
	widget->setBottomTextVerticalPosition(-20);
	VipValueToFormattedText* v = new VipValueToFormattedText("%3.1f");
	v->setMultiplyFactor(1 / range.width());
	widget->setScaleValueToText(v);
	widget->setValue(0);


	widget->area()->setStyleSheet(
		"VipAbstractPlotArea{ background : #383838;}"
		"VipAbstractScale {title-color: white; label-color: white; pen: white;}"
	);

	///
	widget->resize(400, 400);
	widget->show();

	GaugeThread thread(widget);
	thread.start();


	return app.exec();
}
