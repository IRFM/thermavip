#ifndef PY_CURVE_FIT_H
#define PY_CURVE_FIT_H

#include "VipProcessingObject.h"

#include "PyConfig.h"

class FitProcessing;

/// @brief FitProcessing manager
///
/// A FitManage object can be created as a child of a
/// FitProcessing object. In this case, its
/// xBounds() member is used to select a sub-part
/// of the input curve to compute the fit
/// parameters.
///
class PYTHON_EXPORT FitManage : public QObject
{
	Q_OBJECT
public:
	FitManage(FitProcessing*);

	FitProcessing* parent() const;
	virtual VipInterval xBounds() const = 0;
};

/// @brief Base class for fit functions.
///
/// FitProcessing can work on any type of curve,
/// but is mainly used to process temporal curves.
/// That's why it provides the members setTimeUnit(),
/// timeUnit() and timefactor().
///
/// By default, input curve x values are considered
/// in nanoseconds, and the time factor is set to 1.
///
/// Changing the time unit with setTimeUnit() will
/// also change the time factor.
///
/// The processing output if the fitted curve.
///
/// The HTML formatted fit equation is set in
/// the output data as attribute 'equation'.
///
class PYTHON_EXPORT FitProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input_curve)
	VIP_IO(VipOutput output_curve)
	Q_CLASSINFO("category", "Curve fitting")
public:
	// Different types of fit
	enum Type
	{
		Linear,
		Exponential,
		Polynomial,
		Gaussian
	};
	static QString fitName(Type);

	FitProcessing()
	  : m_timeUnit("ns")
	  , m_timeFactor(1)
	{
	}
	~FitProcessing();

	/// @brief Set the time factor to:
	/// -	's', time factor of 1e-9,
	/// -	'us', time factor of 1e-6,
	/// -	'ms', time factor of 1e-3,
	/// -	'ns', time factor of 1.
	///
	/// Setting the time unit to a
	/// different value will set the
	/// time factor to 1 and disable
	/// printing the time unit.
	///
	void setTimeUnit(const QString&);
	QString timeUnit() const;
	double timeFactor() const;

	/// @brief Reimplemented from VipProcessingObject
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipPointVector>(); }
	/// @brief Reimplemented from VipProcessingObject
	virtual DisplayHint displayHint() const { return InputTransform; }

protected:
	virtual void apply();
	virtual void applyFit() = 0;
	VipInterval xBounds() const;
	FitManage* manager() const;

private:
	QString m_timeUnit;
	double m_timeFactor;
};
VIP_REGISTER_QOBJECT_METATYPE(FitProcessing*)

/// @brief Fit a curve (VipPointVector) by a linear equation y = a*x +b.
///
class PYTHON_EXPORT FitLinear : public FitProcessing
{
	Q_OBJECT

public:
	FitLinear();

	double getOffset() const;
	double getSlop() const;

protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
};
VIP_REGISTER_QOBJECT_METATYPE(FitLinear*)

/// @brief Fit a curve (VipPointVector) by an exponential of equation y = a*exp(b*x) + c.
///
class PYTHON_EXPORT FitExponential : public FitProcessing
{
	Q_OBJECT

public:
	FitExponential();

	double getA() const { return m_a; }
	double getB() const { return m_b; }
	double getC() const { return m_c; }

protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
	double m_c;
};
VIP_REGISTER_QOBJECT_METATYPE(FitExponential*)

/// @brief Fit a curve (VipPointVector) by an exponential of equation y = a*exp( (x-b)²/c² ) + d.
///
class PYTHON_EXPORT FitGaussian : public FitProcessing
{
	Q_OBJECT

public:
	FitGaussian();

	double getA() const { return m_a; }
	double getB() const { return m_b; }
	double getC() const { return m_c; }
	double getD() const { return m_d; }

protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
	double m_c;
	double m_d;
};
VIP_REGISTER_QOBJECT_METATYPE(FitGaussian*)

/// @brief Fit a curve (VipPointVector) by a polynomial equation y = a*x² + b*x + c
///
class PYTHON_EXPORT FitPolynomial : public FitProcessing
{
	Q_OBJECT

public:
	FitPolynomial();

	double getA() const { return m_a; }
	double getB() const { return m_b; }
	double getC() const { return m_c; }

protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
	double m_c;
};
VIP_REGISTER_QOBJECT_METATYPE(FitPolynomial*)

#endif
