/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_SCALE_DIV_H
#define VIP_SCALE_DIV_H

#include <qlist.h>
#include <qvector.h>

#include "VipGlobals.h"
#include "VipInterval.h"

/// \addtogroup Plotting
/// @{

/// @brief A class representing a scale division
///
/// A scale division is defined by its boundaries and 3 list
/// for the positions of the major, medium and minor ticks.
///
/// The upperLimit() might be smaller than the lowerLimit()
/// to indicate inverted scales.
///
/// Scale divisions can be calculated from a VipScaleEngine.
///
/// \sa VipScaleEngine::divideScale()
class VIP_PLOTTING_EXPORT VipScaleDiv
{
public:
	//! Scale tick types
	enum TickType
	{
		//! No ticks
		NoTick = -1,

		//! Minor ticks
		MinorTick,

		//! Medium ticks
		MediumTick,

		//! Major ticks
		MajorTick,

		//! Number of valid tick types
		NTickTypes
	};

	typedef QVector<vip_double> TickList;

	explicit VipScaleDiv(vip_double lowerBound = 0.0, vip_double upperBound = 0.0);

	explicit VipScaleDiv(vip_double lowerBound, vip_double upperBound, TickList[NTickTypes]);

	explicit VipScaleDiv(const VipInterval& vipBounds, TickList[NTickTypes]);

	explicit VipScaleDiv(vip_double lowerBound, vip_double upperBound, const TickList& minorTicks, const TickList& mediumTicks, const TickList& majorTicks);

	bool operator==(const VipScaleDiv&) const;
	bool operator!=(const VipScaleDiv&) const;

	void setInterval(vip_double lowerBound, vip_double upperBound);
	void setInterval(const VipInterval& vipBounds);

	void setLowerBound(vip_double);
	vip_double lowerBound() const;

	void setUpperBound(vip_double);
	vip_double upperBound() const;

	vip_double range() const;
	VipInterval bounds() const;

	bool contains(vip_double value) const;

	void setTicks(int tickType, const TickList&);
	TickList ticks(int tickType) const;
	TickList& ticks(int tickType);

	bool isEmpty() const;
	bool isIncreasing() const;

	void invert();
	VipScaleDiv inverted() const;

	VipScaleDiv bounded(vip_double lowerBound, vip_double upperBound) const;

private:
	void computeEpsilon();

	vip_double d_lowerBound;
	vip_double d_upperBound;
	vip_double d_epsilon;
	TickList d_ticks[NTickTypes];
};

/// \return First boundary
/// \sa upperBound()
inline vip_double VipScaleDiv::lowerBound() const
{
	return d_lowerBound;
}

/// \return upper bound
/// \sa lowerBound()
inline vip_double VipScaleDiv::upperBound() const
{
	return d_upperBound;
}

/// \return upperBound() - lowerBound()
inline vip_double VipScaleDiv::range() const
{
	return d_upperBound - d_lowerBound;
}

inline VipInterval VipScaleDiv::bounds() const
{
	return VipInterval(d_lowerBound, d_upperBound);
}

//! Check if the scale division is empty( lowerBound() == upperBound() )
inline bool VipScaleDiv::isEmpty() const
{
	return (d_lowerBound == d_upperBound);
}

//! Check if the scale division is increasing( lowerBound() <= upperBound() )
inline bool VipScaleDiv::isIncreasing() const
{
	return d_lowerBound <= d_upperBound;
}

/// Return if a value is between lowerBound() and upperBound()
///
/// \param value Value
/// \return true/false
inline bool VipScaleDiv::contains(vip_double value) const
{
	const vip_double min = qMin(d_lowerBound, d_upperBound);
	const vip_double max = qMax(d_lowerBound, d_upperBound);

	return value + d_epsilon >= min && value <= max + d_epsilon;
}

/// Assign ticks
///
/// \param type MinorTick, MediumTick or MajorTick
/// \param ticks Values of the tick positions
inline void VipScaleDiv::setTicks(int type, const TickList& ticks)
{
	if (type >= 0 && type < NTickTypes)
		d_ticks[type] = ticks;
}

/// Return a list of ticks
///
/// \param type MinorTick, MediumTick or MajorTick
/// \return Tick list
inline VipScaleDiv::TickList VipScaleDiv::ticks(int type) const
{
	if (type >= 0 && type < NTickTypes)
		return d_ticks[type];

	return TickList();
}

inline VipScaleDiv::TickList& VipScaleDiv::ticks(int tickType)
{
	return d_ticks[tickType];
}

Q_DECLARE_METATYPE(VipScaleDiv);
typedef QList<vip_double> DoubleList;
Q_DECLARE_METATYPE(DoubleList)

typedef QVector<vip_double> DoubleVector;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(DoubleVector)
#endif

class QDataStream;
VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipScaleDiv& div);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipScaleDiv& div);

class VipArchive;
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipScaleDiv& value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipScaleDiv& value);

/// @}
// end Plotting

#endif
