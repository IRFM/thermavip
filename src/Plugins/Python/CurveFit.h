#ifndef CURVE_FIT_H
#define CURVE_FIT_H

#include "VipProcessingObject.h"
#include "VipPlayer.h"

#include "PyConfig.h"


class PYTHON_EXPORT FitProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input_curve)
	VIP_IO(VipOutput output_curve)
	Q_CLASSINFO("category", "Curve fitting")
public:
	FitProcessing() :m_timeUnit("ns"),  m_timeFactor(1){}
	~FitProcessing();

	VipShape fitShape();

	void setTimeUnit(const QString &);
	QString timeUnit() const;
	double timeFactor() const;

	void setPlotPlayer(VipPlotPlayer*);
	VipPlotPlayer * plotPlayer() const;

	virtual void applyFit() = 0;

protected:
	virtual void apply();

protected Q_SLOTS:
	void timeUnitChanged();

private:
	QString m_timeUnit;
	double m_timeFactor;
	QPointer<VipPlotPlayer> m_player;

	static QMutex m_mutex;
};
VIP_REGISTER_QOBJECT_METATYPE(FitProcessing*)

/**
Fit a curve (VipPointVector) by a linear equation y = a*x +b.
The processing output if the fitted curve.
*/
class PYTHON_EXPORT FitLinear : public FitProcessing
{
	Q_OBJECT
		
public:
	FitLinear();

	double getOffset() const;
	double getSlop() const;

	virtual bool acceptInput(int /*index*/, const QVariant & v) const { return v.userType() == qMetaTypeId<VipPointVector>(); }
	virtual DisplayHint displayHint() const { return InputTransform; }
protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
};

VIP_REGISTER_QOBJECT_METATYPE(FitLinear*)


/**
Fit a curve (VipPointVector) by an exponential of equation y = a*exp(bx) +c.
The processing output if the fitted curve.
*/
class PYTHON_EXPORT FitExponential : public FitProcessing
{
	Q_OBJECT
	
public:
	FitExponential();

	double getA() const { return m_a; }
	double getB() const { return m_b; }
	double getC() const { return m_c; }

	virtual bool acceptInput(int /*index*/, const QVariant & v) const { return v.userType() == qMetaTypeId<VipPointVector>(); }
	virtual DisplayHint displayHint() const { return InputTransform; }
protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
	double m_c;
};

VIP_REGISTER_QOBJECT_METATYPE(FitExponential*)

/**
Fit a curve (VipPointVector) by an exponential of equation y = a*exp( (x-b)�/c� ) +d.
The processing output if the fitted curve.
*/
class PYTHON_EXPORT FitGaussian : public FitProcessing
{
	Q_OBJECT

public:
	FitGaussian();

	double getA() const { return m_a; }
	double getB() const { return m_b; }
	double getC() const { return m_c; }
	double getD() const { return m_d; }

	virtual bool acceptInput(int /*index*/, const QVariant & v) const { return v.userType() == qMetaTypeId<VipPointVector>(); }
	virtual DisplayHint displayHint() const { return InputTransform; }
protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
	double m_c;
	double m_d;
};

VIP_REGISTER_QOBJECT_METATYPE(FitGaussian*)

/**
Fit a curve (VipPointVector) by a polynomial equation y = a*x� + bx + c
The processing output if the fitted curve.
*/
class PYTHON_EXPORT FitPolynomial : public FitProcessing
{
	Q_OBJECT

public:
	FitPolynomial();

	double getA() const { return m_a; }
	double getB() const { return m_b; }
	double getC() const { return m_c; }

	virtual bool acceptInput(int /*index*/, const QVariant & v) const { return v.userType() == qMetaTypeId<VipPointVector>(); }
	virtual DisplayHint displayHint() const { return InputTransform; }
protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
	double m_c;
};

VIP_REGISTER_QOBJECT_METATYPE(FitPolynomial*)



#include <qdialog.h>
class VipPlotPlayer;
class VipPlotCurve;

class FitDialogBox : public QDialog
{
	Q_OBJECT

public:
	//fit could be empty or "Linear", "Exponential", "Polynomial", "Gaussian".
	FitDialogBox(VipPlotPlayer * pl,const QString & fit, QWidget * parent = nullptr);
	~FitDialogBox();

	VipPlotCurve * selectedCurve() const;
	int selectedFit() const;

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

void fitCurve( VipPlotPlayer* player, const QString & fit);

/*
class FitObject : public QObject
{
	Q_OBJECT

public:
	FitObject(VipPlotPlayer * player);

public Q_SLOTS:
	void applyFit();
	void checkHasCurves();

private:
	QAction * m_fit_action;
	VipPlotPlayer * m_player;
};
*/
#endif
