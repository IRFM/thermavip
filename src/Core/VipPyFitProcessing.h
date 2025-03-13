/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VIP_PY_FIT_PROCESSING_H
#define VIP_PY_FIT_PROCESSING_H

#include "VipProcessingObject.h"

class VipPyFitProcessing;

/// @brief VipPyFitProcessing manager
///
/// A VipFitManage object can be created as a child of a
/// VipPyFitProcessing object. In this case, its
/// xBounds() member is used to select a sub-part
/// of the input curve to compute the fit
/// parameters.
///
class VIP_CORE_EXPORT VipFitManage : public QObject
{
	Q_OBJECT
public:
	VipFitManage(VipPyFitProcessing*);

	VipPyFitProcessing* parent() const;
	virtual VipInterval xBounds() const = 0;
};

/// @brief Base class for fit functions.
///
/// VipPyFitProcessing can work on any type of curve,
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
class VIP_CORE_EXPORT VipPyFitProcessing : public VipProcessingObject
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

	VipPyFitProcessing()
	  : m_timeUnit("ns")
	  , m_timeFactor(1)
	{
	}
	~VipPyFitProcessing();

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
	VipFitManage* manager() const;

private:
	QString m_timeUnit;
	double m_timeFactor;
};
VIP_REGISTER_QOBJECT_METATYPE(VipPyFitProcessing*)

/// @brief Fit a curve (VipPointVector) by a linear equation y = a*x +b.
///
class VIP_CORE_EXPORT VipPyFitLinear : public VipPyFitProcessing
{
	Q_OBJECT

public:
	VipPyFitLinear();

	double getOffset() const;
	double getSlop() const;

protected:
	virtual void applyFit();

private:
	double m_a;
	double m_b;
};
VIP_REGISTER_QOBJECT_METATYPE(VipPyFitLinear*)

/// @brief Fit a curve (VipPointVector) by an exponential of equation y = a*exp(b*x) + c.
///
class VIP_CORE_EXPORT VipPyFitExponential : public VipPyFitProcessing
{
	Q_OBJECT

public:
	VipPyFitExponential();

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
VIP_REGISTER_QOBJECT_METATYPE(VipPyFitExponential*)

/// @brief Fit a curve (VipPointVector) by an exponential of equation y = a*exp( (x-b)²/c² ) + d.
///
class VIP_CORE_EXPORT VipPyFitGaussian : public VipPyFitProcessing
{
	Q_OBJECT

public:
	VipPyFitGaussian();

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
VIP_REGISTER_QOBJECT_METATYPE(VipPyFitGaussian*)

/// @brief Fit a curve (VipPointVector) by a polynomial equation y = a*x² + b*x + c
///
class VIP_CORE_EXPORT VipPyFitPolynomial : public VipPyFitProcessing
{
	Q_OBJECT

public:
	VipPyFitPolynomial();

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
VIP_REGISTER_QOBJECT_METATYPE(VipPyFitPolynomial*)

#endif
