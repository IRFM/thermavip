#include "CurveFit.h"
#include "PyOperation.h"
#include "VipMath.h"
#include <cmath>

FitManage::FitManage(FitProcessing* fit)
  : QObject(fit)
{
}

FitProcessing* FitManage::parent() const
{
	return qobject_cast<FitProcessing*>(this->QObject::parent());
}

static bool initializeCurveFit()
{
	static bool init = false;
	if (!init && VipPyInterpreter::instance()->isRunning()) {
		auto c = VipPyInterpreter::instance()->execCode("import numpy as np\n"
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
								"  return popt\n");

		init = c.value().value<VipPyError>().isNull();
	}
	return init;
}

/**
\internal
Try to find the starting parameters for exponential fiting.
Returns 0 for standard exponential, 1 for inverse one.
*/
static int exponentialStartParams(const VipPointVector& pts, double& a, double& b, double& c)
{
	// compute the average, min and max of x and y
	double x = 0, y = 0;
	double miny = pts[0].y(), maxy = pts[0].y();
	double minx = pts[0].x(), maxx = pts[0].x();
	for (int i = 0; i < pts.size(); ++i) {
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

	// growing exponential
	if (pts.last().y() > pts.first().y()) {
		if (y > (maxy + miny) / 2) {
			// case inverse exponential
			c = pts.last().y();
			a = qAbs(pts.last().y() - pts.first().y()) / qAbs(pts.last().x() - pts.first().x());
			b = 1; // TODO
			return 1;
		}
		else {
			// standard exponential
			c = pts.first().y();
			a = qAbs(pts.last().y() - pts.first().y()) / qAbs(pts.last().x() - pts.first().x());
			b = (1 / x) * log((y - c) / a);
			return 0;
		}
	}
	else {
		// decreasing exponential
		if (y < (maxy + miny) / 2) {
			// case decay time
			c = pts.last().y();
			b = -1 / (maxx - minx) / 2;
			a = (pts.first().y() - c) / exp(b * pts.first().x());
			return 0;
		}
		else {
			// standard exponential with negative a
			c = pts.first().y();
			a = -qAbs(pts.last().y() - pts.first().y()) / qAbs(pts.last().x() - pts.first().x());
			b = (1 / x) * log((c - y) / a);
			return 0;
		}
	}
}

/**
\internal
Try to find the starting parameters for gaussian fitting.
Equation: y = a * exp(-((x-b)/c)**2) + d
*/
static void gaussianStartParams(const VipPointVector& pts, double& a, double& b, double& c, double& d)
{
	if (pts.size() < 3)
		return;

	double max = pts[0].y(), min = pts[0].y(), max_x = pts[0].x();
	for (int i = 1; i < pts.size(); ++i) {
		if (pts[i].y() < min)
			min = pts[i].y();
		else if (pts[i].y() > max) {
			max = pts[i].y();
			max_x = pts[i].x();
		}
	}

	d = min;
	b = max_x;
	a = (max - min);

	if (a != 0)
		c = (pts[1].x() - b) / (sqrt(-log((pts[1].y() - d) / a)));
	if (vipIsNan(c))
		c = 1;
}

static QVariantList applyCurveFit(const VipAnyData& any,
				  // VipPlotPlayer * pl,
				  const VipInterval& bounds,
				  FitProcessing::Type fit_type,
				  const QString& additional,
				  QString& error,
				  QString& equation,
				  VipPointVector& new_curve,
				  double& start,
				  const QString time_uint = QString(), // optional time unit displayed in the equation
				  double time_factor = 1.	       // optional time factor used for displaying the equation (1 for nano seconds, 1000 for us,...)
)
{
	if (!initializeCurveFit()) {
		error = ("Curve fit module not initialized");
		return QVariantList();
	}

	// get the input curve
	VipPointVector curve = any.value<VipPointVector>();

	if (!curve.size()) {
		error = ("FitLinear: empty input curve");
		return QVariantList();
	}

	// clip the curve to the axes width
	if (bounds.isValid()) {
		VipPointVector tmp;
		for (int i = 0; i < curve.size(); ++i)
			if (bounds.contains(curve[i].x()))
				tmp.append(curve[i]);
		curve = tmp;
	}

	// Find python function
	QString fit_fun;
	switch (fit_type) {
		case FitProcessing::Linear: 
			fit_fun = "fit_linear";
			break;
		case FitProcessing::Exponential:
			fit_fun = "fit_exponential";
			break;
		case FitProcessing::Polynomial:
			fit_fun = "fit_polynomial";
			break;
		case FitProcessing::Gaussian:
			fit_fun = "fit_gaussian";
			break;
		default:
			error = ("unknown fit type");
			return QVariantList();
	}

	new_curve = curve;

	VipPointVector out_curve;
	start = 0;

	if (curve.size()) {
		// do the fitting

		start = curve.first().x();

		if (fit_type == FitProcessing::Exponential) {
			// for exponential fit, we start at 0
			for (int i = 0; i < curve.size(); ++i)
				curve[i].setX(curve[i].x() - start);
		}

		VipNDArrayType<double> x(vipVector(curve.size())), y(vipVector(curve.size()));
		for (int i = 0; i < curve.size(); ++i) {
			x[i] = curve[i].x();
			y[i] = curve[i].y();
		}

		QString add = additional;
		int type = 0;
		if (fit_type == FitProcessing::Exponential) {
			double a = 1, b = 1, c = 1;
			type = exponentialStartParams(curve, a, b, c);
			add = "p0=[" + QString::number(a) + "," + QString::number(b) + "," + QString::number(c) + "]";
		}
		else if (fit_type == FitProcessing::Linear) {
			double a = (curve.last().y() - curve.first().y()) / (curve.last().x() - curve.first().x());
			double b = curve.first().y() - a * curve.first().x();
			add = "p0=[" + QString::number(a) + "," + QString::number(b) + "]";
		}
		else if (fit_type == FitProcessing::Gaussian) {
			double a = 1, b = 1, c = 1, d = 1;
			gaussianStartParams(curve, a, b, c, d);
			add = "p0=[" + QString::number(a) + "," + QString::number(b) + "," + QString::number(c) + "," + QString::number(d) + "]";
		}

		QVariantMap map;
		map["x"] = QVariant::fromValue(VipNDArray(x));
		map["y"] = QVariant::fromValue(VipNDArray(y));
		QString code = add.isEmpty() ? "opt=" + fit_fun + "(x,y)" : "opt=" + fit_fun + "(x,y," + add + ")";

		VipPyCommandList cmds;
		cmds << vipCSendObject("x", VipNDArray(x));
		cmds << vipCSendObject("y", VipNDArray(y));
		cmds << vipCExecCode(code, "code");
		cmds << vipCRetrieveObject("opt");

		QVariant r = VipPyInterpreter::instance()->sendCommands(cmds).value();

		// send the data to the python env
		VipPyError err = r.value<VipPyError>();
		if (!err.isNull()) {
			error = (err.traceback);
			return QVariantList();
		}

		const QVariantMap vals = r.value<QVariantMap>();

		VipNDArrayType<double> ar = vals["opt"].value<VipNDArray>().toDouble();
		QVariantList res;
		for (int i = 0; i < ar.size(); ++i)
			res.push_back(ar[i]);

		QString time = time_uint;
		QString inv_time = time.isEmpty() ? QString() : time + "<sup>-1</sup>";
		QString inv_time_2 = time.isEmpty() ? QString() : time + "<sup>-2</sup>";

		if (ar.size()) {
			if (fit_type == FitProcessing::Exponential) {
				if (type == 0) {
					equation = QString::number(ar[0]) + "* exp(<font size=5><sup>x-" + QString::number(start * time_factor) + time + "</sup>/<sub>" +
						   QString::number((1 / ar[1]) * time_factor) + time + "</sub></font>) + " + QString::number(ar[2]);
				}
			}
			else if (fit_type == FitProcessing::Linear) {
				equation = QString::number(ar[0] / time_factor) + inv_time + "* x + " + QString::number(ar[1]);
			}
			else if (fit_type == FitProcessing::Polynomial) {
				equation = QString::number(ar[0] / (time_factor * time_factor)) + inv_time_2 + "*x<sup>2</sup> + " + QString::number(ar[1] / time_factor) + inv_time + "*x + " +
					   QString::number(ar[2]);
			}
			else if (fit_type == FitProcessing::Gaussian) {
				equation = QString::number(ar[0]) + "* exp(<font size=5><sup> - (x - " + QString::number(ar[1] * time_factor) + time + ")<sup>2</sup></sup>/<sub>" +
					   QString::number(ar[2] * time_factor) + time + "<sup>2</sup></sub></font>) + " + QString::number(ar[3]);
			}
		}
		return res;
	}
	return QVariantList();
}

FitProcessing::~FitProcessing() {}

QString FitProcessing::fitName(Type type) {
	switch (type) {
		case FitProcessing::Linear:
			return "linear";
		case FitProcessing::Exponential:
			return "exponential";
		case FitProcessing::Polynomial:
			return "polynomial";
		case FitProcessing::Gaussian:
			return "gaussian";
		default:
			return QString();
	}
}

void FitProcessing::setTimeUnit(const QString& unit)
{
	if (m_timeUnit != unit) {
		m_timeUnit = unit;
		if (unit == "ns")
			m_timeFactor = 1;
		else if (unit == "us")
			m_timeFactor = 1 / 1000.;
		else if (unit == "ms")
			m_timeFactor = 1 / 1000000.;
		else if (unit == "s")
			m_timeFactor = 1 / 1000000000.;
		else {
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

FitManage* FitProcessing::manager() const
{
	return findChild<FitManage*>();
}

VipInterval FitProcessing::xBounds() const
{
	if (FitManage* m = manager())
		return m->xBounds();
	return VipInterval();
}

void FitProcessing::apply()
{
	applyFit();
}

static VipArchive& operator<<(VipArchive& arch, const FitProcessing* fit)
{
	return arch;
	// return arch.content("player_id", VipUniqueId::id(fit->plotPlayer()));
}
static VipArchive& operator>>(VipArchive& arch, FitProcessing* fit)
{
	// int id = arch.read("player_id").toInt();
	// if (VipPlotPlayer * pl = VipUniqueId::find<VipPlotPlayer>(id))
	//	fit->setPlotPlayer(pl);
	return arch;
}

FitLinear::FitLinear()
  : m_a(0)
  , m_b(0)
{
	outputAt(0)->setData(VipPointVector());
}

double FitLinear::getOffset() const
{
	return m_a;
}
double FitLinear::getSlop() const
{
	return m_b;
}

void FitLinear::applyFit()
{
	QString error, equation;
	VipAnyData any = inputAt(0)->data();
	double start;
	VipPointVector new_curve;
	QVariantList lst = applyCurveFit(any, xBounds(), Linear, QString(), error, equation, new_curve, start, timeUnit(), timeFactor());

	if (error.size()) {
		setError(error);
		return;
	}

	VipPointVector out_curve;

	if (lst.size() == 2) {
		double a = lst[0].toDouble();
		double b = lst[1].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i) {
			out_curve[i] = QPointF(new_curve[i].x(), new_curve[i].x() * a + b);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	if (!equation.isEmpty())
		out.setAttribute("equation", equation);
	outputAt(0)->setData(out);
}

FitExponential::FitExponential()
  : m_a(0)
  , m_b(0)
  , m_c(0)
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
	QVariantList lst = applyCurveFit(any, xBounds(),Exponential, QString(), error, equation, new_curve, start, timeUnit(), timeFactor());

	if (error.size()) {
		setError(error);
		return;
	}

	VipPointVector out_curve;
	if (lst.size() == 3) {
		m_a = lst[0].toDouble();
		m_b = lst[1].toDouble();
		m_c = lst[2].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i) {
			out_curve[i] = QPointF(new_curve[i].x(), exp((new_curve[i].x() - start) * m_b) * m_a + m_c);
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
  : m_a(0)
  , m_b(0)
  , m_c(0)
  , m_d(0)
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
	QVariantList lst = applyCurveFit(any, xBounds(), Gaussian, QString(), error, equation, new_curve, start, timeUnit(), timeFactor());

	if (error.size()) {
		setError(error);
		return;
	}

	VipPointVector out_curve;
	if (lst.size() == 4) {
		m_a = lst[0].toDouble();
		m_b = lst[1].toDouble();
		m_c = lst[2].toDouble();
		m_d = lst[3].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i) {
			double sub = new_curve[i].x() - m_b;
			double sub2 = sub * sub;
			out_curve[i] = QPointF(new_curve[i].x(), m_a * exp(-sub2 / (m_c * m_c)) + m_d);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	if (!equation.isEmpty())
		out.setAttribute("equation", equation);
	outputAt(0)->setData(out);
}

FitPolynomial::FitPolynomial()
  : m_a(0)
  , m_b(0)
  , m_c(0)
{
	outputAt(0)->setData(VipPointVector());
}

void FitPolynomial::applyFit()
{
	QString error, equation;
	VipAnyData any = inputAt(0)->data();
	double start;
	VipPointVector new_curve;
	QVariantList lst = applyCurveFit(any, xBounds(), Polynomial, QString(), error, equation, new_curve, start, timeUnit(), timeFactor());

	if (error.size()) {
		setError(error);
		return;
	}

	VipPointVector out_curve;
	if (lst.size() == 3) {
		m_a = lst[0].toDouble();
		m_b = lst[1].toDouble();
		m_c = lst[2].toDouble();
		out_curve.resize(new_curve.size());
		for (int i = 0; i < new_curve.size(); ++i) {
			out_curve[i] = QPointF(new_curve[i].x(), new_curve[i].x() * new_curve[i].x() * m_a + new_curve[i].x() * m_b + m_c);
		}
	}

	VipAnyData out = create(QVariant::fromValue(out_curve));
	if (!equation.isEmpty())
		out.setAttribute("equation", equation);
	outputAt(0)->setData(out);
}

static int registerFit()
{
	vipRegisterArchiveStreamOperators<FitProcessing*>();
	return 0;
}
static int _registerFit = registerFit();