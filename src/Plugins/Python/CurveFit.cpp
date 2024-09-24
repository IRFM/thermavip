#include "CurveFit.h"
#include "PyOperation.h"
#include "VipMath.h"
#include <cmath>

static bool initializeCurveFit()
{
	static bool init = false;
	if (!init && GetPyOptions()->isRunning())
	{
		PyIOOperation::command_type com = GetPyOptions()->execCode(
			"import numpy as np\n"
			"from scipy.optimize import curve_fit\n"
			"\n"
			"def func_lin(x, a, b) :\n"
			"  return a * x + b\n"
			"\n"
			"def func_pol(x, a, b, c) :\n"
			"  return a * x*x + b*x + c\n"
			"\n"
			"def func_exp(x, a, b, c) :\n"
			"  return a * np.exp(b * x) + c\n"
			"\n"
			"def func_gaussian(x, a, b, c, d) :\n"
			"  return a * np.exp(-((x - b)/c)**2) + d\n"
			"\n"
			"def fit_exponential(x, y, **kwarg) :\n"
			"  popt, pcov = curve_fit(func_exp, x, y, **kwarg)\n"
			"  return popt\n"
			"\n"
			"def fit_gaussian(x, y, **kwarg) :\n"
			"  popt, pcov = curve_fit(func_gaussian, x, y, **kwarg)\n"
			"  return popt\n"
			"\n"
			"def fit_linear(x, y, **kwarg) :\n"
			"  popt, pcov = curve_fit(func_lin, x, y, **kwarg)\n"
			"  return popt\n"
			"\n"
			"def fit_polynomial(x, y, **kwarg) :\n"
			"  popt, pcov = curve_fit(func_pol, x, y, **kwarg)\n"
			"  return popt\n"
		);

		init = GetPyOptions()->wait(com).value<PyError>().isNull();

	}
	return init;
}

/**
\internal
Try to find the starting parameters for exponential fiting.
Returns 0 for standard exponential, 1 for inverse one.
*/
static int exponentialStartParams(const VipPointVector & pts, double & a, double & b, double & c)
{
	//compute the average, min and max of x and y
	double x = 0, y = 0;
	double miny = pts[0].y(), maxy = pts[0].y();
	double minx = pts[0].x(), maxx = pts[0].x();
	for (int i = 0; i < pts.size(); ++i)
	{
		x += pts[i].x();
		y += pts[i].y();
		if (pts[i].y() > maxy)
			maxy = pts[i].y();
		else if (pts[i].y() < miny)
			miny = pts[i].y();
		if (pts[i].x() > maxx)
			maxx = pts[i].x();
		else if (pts[i].x() < minx)
			minx = pts[i].x();
	}
	x /= pts.size();
	y /= pts.size();

	//growing exponential
	if (pts.last().y() > pts.first().y())
	{
		if (y > (maxy + miny) / 2)
		{
			//case inverse exponential
			c = pts.last().y();
			a = qAbs(pts.last().y() - pts.first().y()) / qAbs(pts.last().x() - pts.first().x());
			b = 1; //TODO
			return 1;
		}
		else
		{
			//standard exponential
			c = pts.first().y();
			a = qAbs(pts.last().y() - pts.first().y())/ qAbs(pts.last().x() - pts.first().x());
			b = (1/x) * log((y - c) / a);
			return 0;
		}
	}
	else
	{
		//decreasing exponential
		if (y < (maxy + miny) / 2)
		{
			//case decay time
			c = pts.last().y();
			b = -1/(maxx - minx) / 2;
			a = (pts.first().y() - c) / exp(b*pts.first().x());
			return 0;
		}
		else
		{
			//standard exponential with negative a
			c = pts.first().y();
			a = -qAbs(pts.last().y() - pts.first().y()) / qAbs(pts.last().x() - pts.first().x());
			b = (1 / x) * log((c-y) / a);
			return 0;
		}
	}
}


/**
\internal
Try to find the starting parameters for gaussian fitting.
Equation: y = a * exp(-((x-b)/c)**2) + d
*/
static void gaussianStartParams(const VipPointVector & pts, double & a, double & b, double & c, double & d)
{
	if (pts.size() <3)
		return;

	double max = pts[0].y(), min = pts[0].y(), max_x = pts[0].x();
	for (int i = 1; i < pts.size(); ++i)
	{
		if (pts[i].y() < min)
			min = pts[i].y();
		else if (pts[i].y() > max)
		{
			max = pts[i].y();
			max_x = pts[i].x();
		}
	}

	d = min;
	b = max_x;
	a = (max - min);

	if (a != 0)
		c = (pts[1].x() - b) / ( sqrt( -log( (pts[1].y()-d)/a ) ) );
	if (vipIsNan(c))
		c = 1;
}



static QVariantList applyCurveFit(const VipAnyData & any, 
	VipPlotPlayer * pl, 
	const QString fit_func, 
	const QString &additional , 
	QString & error, 
	QString & equation, 
	VipPointVector & new_curve, 
	double & start,
	const QString time_uint = QString(), //optional time unit displayed in the equation
	double time_factor = 1. //optional time factor used for displaying the equation (1 for nano seconds, 1000 for us,...)
	)
{
	if (!initializeCurveFit())
	{
		error = ("Curve fit module not initialized");
		return QVariantList();
	}

	//get the input curve
	VipPointVector curve = any.value<VipPointVector>();

	if (!curve.size())
	{
		error = ("FitLinear: empty input curve");
		return QVariantList();
	}

	//clip the curve to the axes width
	if (pl)
	{
		VipInterval bounds = pl->defaultXAxis()->scaleDiv().bounds();
		if (pl->displayVerticalWindow()) {
			QRectF r = pl->verticalWindow()->rawData().polygon().boundingRect();
			VipInterval inter(r.left(), r.right());
			VipInterval intersect = inter.intersect(bounds);
			if (intersect.isValid())
				bounds = intersect;

			VipPointVector tmp;
			for (int i = 0; i < curve.size(); ++i)
				if (bounds.contains( curve[i].x() ))
					tmp.append(curve[i]);
			curve = tmp;
		}
	}
	
	new_curve = curve;

	VipPointVector out_curve;
	start = 0;

	if (curve.size())
	{
		//do the fitting

		start = curve.first().x();

		if (fit_func == "fit_exponential")
		{
			//for exponential fit, we start at 0
			for (int i = 0; i < curve.size(); ++i)
				curve[i].setX(curve[i].x() - start);
		}

		VipNDArrayType<double> x(vipVector(curve.size())), y(vipVector(curve.size()));
		for (int i = 0; i < curve.size(); ++i)
		{
			x[i] = curve[i].x();
			y[i] = curve[i].y();
		}

		//send the data to the python env
		PyError err1 = GetPyOptions()->wait(GetPyOptions()->sendObject("x", QVariant::fromValue(VipNDArray(x)))).value<PyError>();
		PyError err2 = GetPyOptions()->wait(GetPyOptions()->sendObject("y", QVariant::fromValue(VipNDArray(y)))).value<PyError>();
		if (!err1.isNull())
		{
			error = (err1.traceback);
			return QVariantList();
		}
		if (!err2.isNull())
		{
			error = (err2.traceback);
			return QVariantList();
		}

		QString add = additional;
		int type = 0;
		if (fit_func == "fit_exponential")
		{
			double a = 1, b = 1, c = 1;
			type = exponentialStartParams(curve, a, b, c);
			add = "p0=[" + QString::number(a) + "," + QString::number(b) + "," + QString::number(c) + "]";
		}
		else if (fit_func == "fit_linear")
		{
			double a = (curve.last().y() - curve.first().y()) / (curve.last().x() - curve.first().x());
			double b = curve.first().y() - a*curve.first().x();
			add = "p0=[" + QString::number(a) + "," + QString::number(b) + "]";
		}
		else if (fit_func == "fit_gaussian")
		{
			double a = 1, b = 1, c = 1, d = 1;
			gaussianStartParams(curve, a, b, c, d);
			add = "p0=[" + QString::number(a) + "," + QString::number(b) + "," + QString::number(c) + "," + QString::number(d) + "]";
		}

		QString code = add.isEmpty() ? "opt=" + fit_func + "(x,y)" : "opt=" + fit_func + "(x,y," + add + ")";
		//apply the script
		PyError err3 = GetPyOptions()->wait(GetPyOptions()->execCode(code)).value<PyError>();

		if (!err3.isNull())
		{
			error = "Unable to find fitting parameters";//(err3.traceback);
			return QVariantList();
		}

		//retrieve the result
		QVariant v = GetPyOptions()->wait(GetPyOptions()->retrieveObject("opt"));
		PyError err4 = v.value<PyError>();
		if (!err4.isNull())
		{
			error = (err4.traceback);
			return QVariantList();
		}

		VipNDArrayType<double> ar = v.value<VipNDArray>().toDouble();
		QVariantList res;
		for (int i = 0; i < ar.size(); ++i)
			res += ar[i];

		QString time = time_uint;
		QString inv_time = time.isEmpty() ? QString() : time + "<sup>-1</sup>";
		QString inv_time_2 = time.isEmpty() ? QString() : time + "<sup>-2</sup>";

		if (ar.size())
		{
			if (fit_func == "fit_exponential")
			{
				if (type == 0)
				{
					equation = QString::number(ar[0]) + "* exp(<font size=5><sup>x-" + QString::number(start * time_factor) + time +
						"</sup>/<sub>" + QString::number((1 / ar[1]) * time_factor) + time + "</sub></font>) + " + QString::number(ar[2]);
				}
			}
			else if (fit_func == "fit_linear")
			{
				equation = QString::number(ar[0] / time_factor) + inv_time + "* x + " + QString::number(ar[1]);
			}
			else if (fit_func == "fit_polynomial")
			{
				equation = QString::number(ar[0] / (time_factor*time_factor)) + inv_time_2 + 
					"*x<sup>2</sup> + " + QString::number(ar[1]/time_factor) + inv_time + "*x + "
					+ QString::number(ar[2]);
			}
			else if (fit_func == "fit_gaussian")
			{
				equation = QString::number(ar[0]) + "* exp(<font size=5><sup> - (x - " + QString::number(ar[1] * time_factor) + time +
					")<sup>2</sup></sup>/<sub>" + QString::number(ar[2] * time_factor) + time + "<sup>2</sup></sub></font>) + " + QString::number(ar[3]);
			}
		}
		return  res;
	}
	return QVariantList();
}



QMutex FitProcessing::m_mutex;

FitProcessing::~FitProcessing()
{
}

void FitProcessing::setTimeUnit(const QString & unit)
{
	if (m_timeUnit != unit)
	{
		m_timeUnit = unit;
		if (unit == "ns")
			m_timeFactor = 1;
		else if (unit == "us")
			m_timeFactor = 1 / 1000.;
		else if (unit == "ms")
			m_timeFactor = 1 / 1000000.;
		else if (unit == "s")
			m_timeFactor = 1 / 1000000000.;
		else
		{
			m_timeUnit = QString();
			m_timeFactor = 1;
		}

		reload();
	}
}
QString FitProcessing::timeUnit() const
{
	return m_timeUnit;
}

double FitProcessing::timeFactor() const
{
	return m_timeFactor;
}

void FitProcessing::setPlotPlayer(VipPlotPlayer* pl)
{
	if (pl != m_player)
	{
		if (m_player) {
			disconnect(m_player, SIGNAL(timeUnitChanged(const QString&)), this, SLOT(timeUnitChanged()));
			disconnect(m_player->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
			disconnect(m_player->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(reload()));
		}
		m_player = pl;
		if (m_player)
		{
			connect(m_player, SIGNAL(timeUnitChanged(const QString&)), this, SLOT(timeUnitChanged()));
			connect(m_player->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
			connect(m_player->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(reload()));
			timeUnitChanged();
		}
	}
}
VipPlotPlayer * FitProcessing::plotPlayer() const
{
	if (m_player)
		return m_player;
	QList<VipDisplayObject*> displays = vipListCast< VipDisplayObject*>(this->allSinks());
	for (int i = 0; i < displays.size(); ++i)
		if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(displays[i]->widget())) {
			const_cast<FitProcessing*>(this)->setPlotPlayer(pl);
			return const_cast<FitProcessing*>(this)->m_player = pl;
		}
	return m_player;
}

void FitProcessing::timeUnitChanged()
{
	if (m_player)
		setTimeUnit(m_player->timeUnit());
}

void FitProcessing::apply()
{
	QMutexLocker lock(&m_mutex);
	applyFit();
}



static VipArchive & operator<<(VipArchive & arch, const FitProcessing * fit)
{
	return arch;
	//return arch.content("player_id", VipUniqueId::id(fit->plotPlayer()));
}
static VipArchive & operator>>(VipArchive & arch, FitProcessing * fit)
{
	//int id = arch.read("player_id").toInt();
	//if (VipPlotPlayer * pl = VipUniqueId::find<VipPlotPlayer>(id))
	//	fit->setPlotPlayer(pl);
	return arch;
}





FitLinear::FitLinear()
	:m_a(0), m_b(0)
{
	outputAt(0)->setData(VipPointVector());
}

double FitLinear::getOffset() const { return m_a; }
double FitLinear::getSlop() const { return m_b; }


void FitLinear::applyFit()
{
	QString error, equation;
	VipAnyData any = inputAt(0)->data();
	double start;
	VipPointVector new_curve;
	QVariantList lst = applyCurveFit(any, plotPlayer(), "fit_linear", QString(), error, equation, new_curve, start,timeUnit(),timeFactor());

	if (error.size())
	{
		setError(error);
		return;
	}


	
	VipPointVector out_curve;
	
	if (lst.size() == 2)
	{
		double a = lst[0].toDouble();
		double b = lst[1].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i)
		{
			out_curve[i] = QPointF(new_curve[i].x(), new_curve[i].x()*a + b);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	if (!equation.isEmpty())
		out.setAttribute("equation",equation);
	outputAt(0)->setData(out);
}








FitExponential::FitExponential()
	:m_a(0), m_b(0), m_c(0)
{
	outputAt(0)->setData(VipPointVector());
}

void FitExponential::applyFit()
{
	QString error, equation;
	VipAnyData any = inputAt(0)->data();

	VipPointVector curve = any.value<VipPointVector>();
	VipPointVector new_curve;
	double start;
	QVariantList lst = applyCurveFit(any, plotPlayer(), "fit_exponential", QString(), error, equation, new_curve,start, timeUnit(), timeFactor());
	
	if (error.size())
	{
		setError(error);
		return;
	}

	VipPointVector out_curve;
	if (lst.size() == 3)
	{
		m_a = lst[0].toDouble();
		m_b = lst[1].toDouble();
		m_c = lst[2].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i)
		{
			out_curve[i] = QPointF(new_curve[i].x(), exp((new_curve[i].x()-start)*m_b)*m_a + m_c);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	out.setXUnit(any.xUnit());
	out.setYUnit(any.yUnit());
	out.setZUnit(any.zUnit());

	if (!equation.isEmpty())
		out.setAttribute("equation", equation);
	outputAt(0)->setData(out);
}



FitGaussian::FitGaussian()
	:m_a(0), m_b(0), m_c(0), m_d(0) 
{
	outputAt(0)->setData(VipPointVector());
}

void FitGaussian::applyFit()
{
	QString error, equation;
	VipAnyData any = inputAt(0)->data();

	VipPointVector curve = any.value<VipPointVector>();
	VipPointVector new_curve;
	double start;
	QVariantList lst = applyCurveFit(any, plotPlayer(), "fit_gaussian", QString(), error, equation, new_curve, start, timeUnit(), timeFactor());

	if (error.size())
	{
		setError(error);
		return;
	}

	VipPointVector out_curve;
	if (lst.size() == 4)
	{
		m_a = lst[0].toDouble();
		m_b = lst[1].toDouble();
		m_c = lst[2].toDouble();
		m_d = lst[3].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i)
		{
			double sub = new_curve[i].x() - m_b;
			double sub2 = sub*sub;
			out_curve[i] = QPointF(new_curve[i].x(), m_a * exp(-sub2/(m_c*m_c)) + m_d);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	if (!equation.isEmpty())
		out.setAttribute("equation", equation);
	outputAt(0)->setData(out);
}




FitPolynomial::FitPolynomial()
	:m_a(0), m_b(0), m_c(0)
{
	outputAt(0)->setData(VipPointVector());
}

void FitPolynomial::applyFit()
{
	QString error, equation;
	VipAnyData any = inputAt(0)->data();
	double start;
	VipPointVector new_curve;
	QVariantList lst = applyCurveFit(any, plotPlayer(), "fit_polynomial",QString(), error, equation, new_curve, start, timeUnit(), timeFactor());

	if (error.size())
	{
		setError(error);
		return;
	}

	VipPointVector out_curve;
	if (lst.size() == 3)
	{
		m_a = lst[0].toDouble();
		m_b = lst[1].toDouble();
		m_c = lst[2].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i)
		{
			out_curve[i] = QPointF(new_curve[i].x(), new_curve[i].x()*new_curve[i].x()*m_a + new_curve[i].x()*m_b +m_c);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	if (!equation.isEmpty())
		out.setAttribute("equation", equation);
	outputAt(0)->setData(out);
}









#include "VipSymbol.h"
#include "VipPlotShape.h"
#include "VipPlotCurve.h"
#include "VipPlayer.h"
#include "VipDisplayObject.h"
#include "VipIODevice.h"
#include "VipStandardWidgets.h"
#include "VipGui.h"
#include <qboxlayout.h>
#include <qgridlayout.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qlabel.h>


class FitDialogBox::PrivateData
{
public:

	QLabel curvesLabel;
	QComboBox curves;
	QLabel fitLabel;
	QComboBox fit;

	QPushButton ok, cancel;

	VipPlotPlayer * player;
};

FitDialogBox::FitDialogBox(VipPlotPlayer * pl,const QString & fit, QWidget * parent)
	:QDialog(parent)
{
	//retrieve all visible and selected curves
	QList<VipPlotCurve*> curves = pl->viewer()->area()->findItems<VipPlotCurve*>(QString(), 1, 1);

	
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->player = pl;

	QGridLayout * lay = new QGridLayout();
	lay->addWidget(&d_data->curvesLabel, 0, 0);
	lay->addWidget(&d_data->curves, 0, 1);
	lay->addWidget(&d_data->fitLabel, 1, 0);
	lay->addWidget(&d_data->fit, 1, 1);

	d_data->curvesLabel.setText(tr("Select curve to fit:"));
	d_data->fitLabel.setText(tr("Select the fit type:"));
	
	for (int i = 0; i < curves.size(); ++i)
		d_data->curves.addItem(curves[i]->title().text());

	d_data->ok.setText(tr("Ok"));
	d_data->cancel.setText(tr("Cancel"));

	d_data->fit.addItem("Linear");
	d_data->fit.addItem("Exponential");
	d_data->fit.addItem("Polynomial");
	d_data->fit.addItem("Gaussian");
	d_data->fit.setCurrentText(fit);

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addStretch(1);
	hlay->addWidget(&d_data->ok);
	hlay->addWidget(&d_data->cancel);

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addLayout(lay);
	vlay->addWidget(VipLineWidget::createSunkenHLine());
	vlay->addLayout(hlay);
	setLayout(vlay);

	connect(&d_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(&d_data->cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));

	this->setWindowTitle("Fit plot");
}

FitDialogBox::~FitDialogBox()
{
}

VipPlotCurve * FitDialogBox::selectedCurve() const
{
	QList<VipPlotCurve*> curves = d_data->player->viewer()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
	for (int i = 0; i < curves.size(); ++i)
		if (curves[i]->title().text() == d_data->curves.currentText())
			return curves[i];
	return nullptr;
}

/**0=linear, 1 = exponential, 2 = polynomial*/
int FitDialogBox::selectedFit() const
{
	return d_data->fit.currentIndex();
}


static void fitCurveShape( VipPlotCurve * curve, VipPlotPlayer * player, int fit_type)
{
	VipProcessingPool * pool = player->processingPool();

	VipOutput * src = nullptr;
	if (VipDisplayObject * disp = curve->property("VipDisplayObject").value<VipDisplayObject*>())
		src = disp->inputAt(0)->connection()->source();

	FitProcessing * fit = nullptr;
	if (fit_type == 0)
		fit = new FitLinear();
	else if (fit_type == 1)
		fit = new FitExponential();
	else if(fit_type == 2)
		fit = new FitPolynomial();
	else
		fit = new FitGaussian();
	fit->setParent(pool);

	fit->inputAt(0)->setData(curve->rawData());
	if (src)
		fit->inputAt(0)->setConnection(src);
	fit->update();
	fit->setScheduleStrategy(VipProcessingObject::Asynchronous);
	fit->setDeleteOnOutputConnectionsClosed(true);
	fit->setPlotPlayer(player);

	/*if (src)
	{
		fit->inputAt(0)->setConnection(src);
	}*/

	VipDisplayCurve * disp = static_cast<VipDisplayCurve*>(vipCreateDisplayFromData(fit->outputAt(0)->data(), player));
	disp->setParent(pool);
	disp->inputAt(0)->setConnection(fit->outputAt(0));

	QPen pen = curve->boxStyle().borderPen();
	pen.setStyle(Qt::DotLine);
	pen.setWidth(2);
	disp->item()->boxStyle().setBorderPen(pen);

	/*VipSymbol* sym = new VipSymbol(VipSymbol::Ellipse);
	sym->setSize(7, 7);
	sym->setPen(curve->boxStyle().borderPen());
	QColor c = curve->boxStyle().borderPen().color();
	c.setAlpha(120);
	sym->setBrush(c);
	disp->item()->setSymbol(sym);*/
	disp->item()->setTitle("Fit " + curve->title().text());
	fit->setAttribute("Name", "Fit " + curve->title().text());

	VipText text(QString("<b>Fit</b>: #pequation"));
	QColor c = curve->boxStyle().borderPen().color();
	c.setAlpha(120);
	text.setBackgroundBrush(c);
	text.setTextPen(QPen(vipWidgetTextBrush(player).color()));
	disp->item()->addText(text);

	vipCreatePlayersFromProcessing(disp, player, nullptr, curve);
}

void fitCurve( VipPlotPlayer* player, const QString & fit)
{
	if (!player)
		return;
	FitDialogBox dial(player,fit);
	if (dial.exec() == QDialog::Accepted)
	{
		fitCurveShape( dial.selectedCurve(), player, dial.selectedFit());
	}
}


/*

FitObject::FitObject(VipPlotPlayer * player)
	:QObject(player), m_player(player)
{
	player->setProperty("__vip_PyFirCurve", true);

	m_fit_action = player->toolBar()->addAction(vipIcon("fit.png"), tr("<b>Fit curve...</b>"
		"<br>Fit a curve with a linear, exponential or polynomial equation."
		"<br>You can apply the fit on a sub-part of the curve using a ROI."));

	connect(m_fit_action, SIGNAL(triggered(bool)), this, SLOT(applyFit()));

	checkHasCurves();
}

void FitObject::applyFit()
{
	//fitCurve(qobject_cast<VipPlotPlayer*>(parent()));
}

void FitObject::checkHasCurves()
{
	if (!qobject_cast<VipPlotPlayer*>(parent()))
		return;

	QList<VipPlotCurve*> curves = static_cast<VipPlotPlayer*>(parent())->viewer()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
	m_fit_action->setVisible(curves.size() > 0);
}




static void createPlotPlayer(VipPlotPlayer *pl)
{
	if(!pl->property("__vip_PyFirCurve").toBool())
		new FitObject(pl);
}

static void itemAddedOrRemoved(VipPlotItem* , VipPlotPlayer* pl)
{
	if (FitObject * fit = pl->findChild<FitObject*>())
	{
		fit->checkHasCurves();
	}
}

static int registerFit()
{
	vipFDPlayerCreated().append<void(VipPlotPlayer *)>(createPlotPlayer);
	VipFDItemAddedOnPlayer().append<void(VipPlotItem*, VipPlotPlayer*)>(itemAddedOrRemoved);
	VipFDItemRemovedFromPlayer().append<void(VipPlotItem*, VipPlotPlayer*)>(itemAddedOrRemoved);
	vipRegisterArchiveStreamOperators<FitProcessing*>();
	return 0;
}
static int _registerFit = registerFit();
*/
static int registerFit()
{
	vipRegisterArchiveStreamOperators<FitProcessing*>();
	return 0;
}
static int _registerFit = registerFit();