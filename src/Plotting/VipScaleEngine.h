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

#ifndef VIP_SCALE_ENGINE_H
#define VIP_SCALE_ENGINE_H

#include "VipPlotUtils.h"
#include "VipInterval.h"
#include "VipScaleDiv.h"
#include "VipScaleDraw.h"

#include <QPointer>

/// \addtogroup Plotting
/// @{

class VipValueTransform;
class VipAbstractScale;

/// \brief Arithmetic including a tolerance
class VIP_PLOTTING_EXPORT VipScaleArithmetic
{
public:
	static vip_double ceilEps(vip_double value, vip_double intervalSize);
	static vip_double floorEps(vip_double value, vip_double intervalSize);

	static vip_double divideEps(vip_double interval, vip_double steps);

	static vip_double ceil125(vip_double x);
	static vip_double floor125(vip_double x);
};

/// \brief Base class for scale engines.
///
/// A scale engine tries to find "reasonable" ranges and step sizes
/// for scales.
///
/// The layout of the scale can be varied with setAttribute().
///
/// offers implementations for logarithmic (log10)
/// and linear scales. Contributions for other types of scale engines
/// (date/time, log2 ... ) are welcome.

class VIP_PLOTTING_EXPORT VipScaleEngine
{
public:
	/// Layout attributes
	/// \sa setAttribute(), testAttribute(), reference(),
	///    lowerMargin(), upperMargin()

	enum Attribute
	{
		//! No attributes
		NoAttribute = 0x00,

		//! Build a scale which includes the reference() value.
		IncludeReference = 0x01,

		//! Build a scale which is symmetric to the reference() value.
		Symmetric = 0x02,

		/// The endpoints of the scale are supposed to be equal the
		/// outmost included values plus the specified margins
		/// (see setMargins()).
		/// If this attribute is *not* set, the endpoints of the scale will
		/// be integer multiples of the step size.
		Floating = 0x04,

		//! Turn the scale upside down.
		Inverted = 0x08
	};

	//! Layout attributes
	typedef QFlags<Attribute> Attributes;

	enum ScaleType
	{
		Unknown,
		Linear,
		Log10,
		DateTime,
		User = 100
	};

	explicit VipScaleEngine();
	virtual ~VipScaleEngine();

	virtual int scaleType() const = 0;

	/// Returns true if the scale is linear (not log or power,...).
	/// Currently returns true for Linear and DateTime type
	virtual bool isLinear() const = 0;

	/// Called when the scale engine is about to be used within VipAbstractScale::computeScaleDiv() for automatic axis.
	virtual void onComputeScaleDiv(VipAbstractScale* scale, const VipInterval& items_interval)
	{
		(void)scale;
		(void)items_interval;
	}

	void setAttribute(Attribute, bool on = true);
	bool testAttribute(Attribute) const;

	void setAttributes(Attributes);
	Attributes attributes() const;

	void setReference(vip_double reference);
	vip_double reference() const;

	void setMargins(vip_double lower, vip_double upper);
	vip_double lowerMargin() const;
	vip_double upperMargin() const;

	/// Align and divide an interval
	///
	/// \param maxNumSteps Max. number of steps
	/// \param x1 First limit of the interval (In/Out)
	/// \param x2 Second limit of the interval (In/Out)
	/// \param stepSize Step size (Return value)
	virtual void autoScale(int maxNumSteps, vip_double& x1, vip_double& x2, vip_double& stepSize) const = 0;

	/// \brief Calculate a scale division
	///
	/// \param x1 First interval limit
	/// \param x2 Second interval limit
	/// \param maxMajSteps Maximum for the number of major steps
	/// \param maxMinSteps Maximum number of minor steps
	/// \param stepSize Step size. If stepSize == 0.0, the scaleEngine
	///            calculates one.
	virtual VipScaleDiv divideScale(vip_double x1, vip_double x2, int maxMajSteps, int maxMinSteps, vip_double stepSize = 0.0) const = 0;

	//! \return a transformation
	virtual VipValueTransform* transformation() const = 0;

protected:
	bool contains(const VipInterval&, vip_double val) const;
	VipScaleDiv::TickList strip(const VipScaleDiv::TickList&, const VipInterval&) const;
	vip_double divideInterval(vip_double interval, int numSteps) const;

	VipInterval buildInterval(vip_double v) const;

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// \brief A scale engine for linear scales
///
/// The step size will fit into the pattern
/// \f$\left\{ 1,2,5\right\} \cdot 10^{n}\f$, where n is an integer.

class VIP_PLOTTING_EXPORT VipLinearScaleEngine : public VipScaleEngine
{
public:
	virtual void autoScale(int maxSteps, vip_double& x1, vip_double& x2, vip_double& stepSize) const;

	virtual VipScaleDiv divideScale(vip_double x1, vip_double x2, int numMajorSteps, int numMinorSteps, vip_double stepSize = 0.0) const;

	virtual VipValueTransform* transformation() const;

	virtual int scaleType() const { return Linear; }
	virtual bool isLinear() const { return true; }

protected:
	VipInterval align(const VipInterval&, vip_double stepSize) const;

	void buildTicks(const VipInterval&, vip_double stepSize, int maxMinSteps, VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes]) const;

	VipScaleDiv::TickList buildMajorTicks(const VipInterval& interval, vip_double stepSize) const;

	void buildMinorTicks(const VipScaleDiv::TickList& majorTicks, int maxMinMark, vip_double step, VipScaleDiv::TickList&, VipScaleDiv::TickList&) const;
};

/// \brief A scale engine for logarithmic (base 10) scales
///
/// The step size is measured in *decades*
/// and the major step size will be adjusted to fit the pattern
/// \f$\left\{ 1,2,3,5\right\} \cdot 10^{n}\f$, where n is a natural number
/// including zero.
///
/// \warning the step size as well as the margins are measured in *decades*.

class VIP_PLOTTING_EXPORT VipLog10ScaleEngine : public VipScaleEngine
{
public:
	virtual void autoScale(int maxSteps, vip_double& x1, vip_double& x2, vip_double& stepSize) const;

	virtual VipScaleDiv divideScale(vip_double x1, vip_double x2, int numMajorSteps, int numMinorSteps, vip_double stepSize = 0.0) const;

	virtual VipValueTransform* transformation() const;

	virtual int scaleType() const { return Log10; }
	virtual bool isLinear() const { return false; }

protected:
	VipInterval log10(const VipInterval&) const;
	VipInterval pow10(const VipInterval&) const;

	VipInterval align(const VipInterval&, vip_double stepSize) const;

	void buildTicks(const VipInterval&, vip_double stepSize, int maxMinSteps, VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes]) const;

	VipScaleDiv::TickList buildMajorTicks(const VipInterval& interval, vip_double stepSize) const;

	VipScaleDiv::TickList buildMinorTicks(const VipScaleDiv::TickList& majorTicks, int maxMinMark, vip_double step) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipScaleEngine::Attributes)

class VipFixedValueToText;

/// @brief Linear scale engine similar to VipLinearScaleEngine
///
/// VipFixedScaleEngine provides, by default; the same behavior as VipLinearScaleEngine.
/// If a valid VipFixedValueToText is passed to the constructor, it will compute
/// the scale div always considering the start value as 0, and will update VipFixedValueToText::startValue().
///
/// This allows fixed scale ticks positions with evolving tick texts.
///
/// See CurveStreaming test example for more details.
///
class VIP_PLOTTING_EXPORT VipFixedScaleEngine : public VipLinearScaleEngine
{
	QPointer <VipFixedValueToText> d_vt;
	double d_maxIntervalWidth;

public:
	VipFixedScaleEngine(VipFixedValueToText* vt = nullptr);

	void setMaxIntervalWidth(double);
	double maxIntervalWidth() const noexcept;

	virtual void onComputeScaleDiv(VipAbstractScale* scale, const VipInterval& items_interval);
	virtual void autoScale(int maxSteps, vip_double& x1, vip_double& x2, vip_double& stepSize) const;
	virtual VipScaleDiv divideScale(vip_double x1, vip_double x2, int maxMajSteps, int maxMinSteps, vip_double stepSize = 0.0) const;
};

class VipValueToTime;

/// @brief Class engine used with VipValueToTime text transform.
/// Deprecated!
class VIP_PLOTTING_EXPORT VipDateTimeScaleEngine : public VipLinearScaleEngine
{
	QPointer < VipValueToTime> m_vt;

public:
	VipDateTimeScaleEngine()
	  : m_vt(nullptr)
	{
	}

	void setValueToTime(VipValueToTime* vt) { m_vt = vt; }
	VipValueToTime* valueToTime() const { return const_cast<VipValueToTime*>(m_vt.data()); }

	virtual void onComputeScaleDiv(VipAbstractScale* scale, const VipInterval& items_interval);
	virtual int scaleType() const { return DateTime; }
	virtual bool isLinear() const { return true; }
	virtual void autoScale(int maxSteps, vip_double& x1, vip_double& x2, vip_double& stepSize) const;
	virtual VipScaleDiv divideScale(vip_double x1, vip_double x2, int maxMajSteps, int maxMinSteps, vip_double stepSize = 0.0) const;
};

/// @}
// end Plotting

#endif
