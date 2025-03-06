#include <cmath>
#include <qapplication.h>
#include "VipPlotHistogram.h"
#include "VipPlotWidget2D.h"
#include "VipToolTip.h"

double norm_pdf(double x, double mu, double sigma)
{
	return 1.0 / (sigma * sqrt(2.0 * M_PI)) * exp(-(pow((x - mu) / sigma, 2) / 2.0));
}
///
VipIntervalSampleVector offset(const VipIntervalSampleVector& hist, double y)
{
	VipIntervalSampleVector res = hist;
	for (int i = 0; i < res.size(); ++i)
		res[i].value += y;
	return res;
}

#include <QDir>
int main(int argc, char** argv)
{
	// To debug from the thermavip folder
	QCoreApplication::addLibraryPath(QDir::currentPath().toLatin1());
	QApplication app(argc, argv);
///	
	VipPlotWidget2D w;
	w.area()->setMouseWheelZoom(true);
	w.area()->setMousePanning(Qt::RightButton);
	w.area()->setMargins(VipMargins(10, 10, 10, 10));
	w.area()->titleAxis()->setVisible(true);
	w.area()->setTitle("<b>Various histogram styles</b>");

	//configure tooltip
	w.area()->setPlotToolTip(new VipToolTip());
	w.area()->plotToolTip()->setDisplayFlags(VipToolTip::ItemsTitles | VipToolTip::ItemsLegends | VipToolTip::ItemsToolTips);
	w.area()->plotToolTip()->setOverlayPen(QPen(Qt::magenta, 3));
	  ///	
	// generate histogram
	VipIntervalSampleVector hist;
	for (int i = -10; i < 10; ++i) {
		VipIntervalSample s;
		s.interval = VipInterval(i, i + 1);
		s.value = norm_pdf(i, 0, 2) * 5;
		hist.push_back(s);
	}
///	
	double yoffset = 0;
	{
		VipPlotHistogram* h = new VipPlotHistogram("Columns with text");
		h->setRawData(hist);
		h->setStyle(VipPlotHistogram::Columns);
		h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
		h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
		h->setText("#value%.2f");
		h->setToolTipText("<b>From</b> #min<br><b>To</b> #max<br><b>Values</b>: #value");
		h->setTextPosition(Vip::XInside);
		h->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	}
	yoffset += 1.5;
	{
		VipPlotHistogram* h = new VipPlotHistogram("Outline");
		h->setRawData(offset(hist, yoffset));
		h->setBaseline(yoffset);
		h->setStyle(VipPlotHistogram::Outline);
		h->setToolTipText("<b>From</b> #min<br><b>To</b> #max<br><b>Values</b>: #value");
		h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
		h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
		h->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	}
	yoffset += 1.5;
	{
		VipPlotHistogram* h = new VipPlotHistogram("Lines");
		h->setRawData(offset(hist, yoffset));
		h->setBaseline(yoffset);
		h->setStyle(VipPlotHistogram::Lines);
		h->boxStyle().setBackgroundBrush(QBrush(QColor(0x0178BB)));
		h->boxStyle().setBorderPen(QPen(QColor(0x0178BB).lighter()));
		h->setToolTipText("<b>From</b> #min<br><b>To</b> #max<br><b>Values</b>: #value");
		h->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
	}
///	
	w.show();
	return app.exec();
}

