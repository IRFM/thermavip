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

#ifndef VIP_ADAPTATIVE_GRADIENT_H
#define VIP_ADAPTATIVE_GRADIENT_H

#include "VipGlobals.h"
#include <QGradient>
#include <QSharedDataPointer>
#include <QVector>

/// \addtogroup Plotting
/// @{

class QRectF;
class VipPie;

/// Class used to build linear, radial or conical gradient for VipBoxStyle objects.
/// VipAdaptativeGradient internally uses relatives gradient stops to represent a color gradient, and the actual brush is built using the createBrush() member.
class VIP_PLOTTING_EXPORT VipAdaptativeGradient
{
public:
	/// @brief Gradient type
	enum Type
	{
		NoGradient,
		Linear,
		Radial,
		Conical
	};

	/// @brief Construct from a brush object
	VipAdaptativeGradient(const QBrush& brush = QBrush());

	/// @brief Construct linear gradient using QGradientStops object ranging from 0 to 1
	VipAdaptativeGradient(const QGradientStops& gradientStops, Qt::Orientation orientation);
	/// @brief Construct linear gradient using a brush, relative stop values (from 0 to 1) and light factors.
	/// At each stop, the brush color will be lightened by the corresponding factor using QColor::lighter() function.
	VipAdaptativeGradient(const QBrush& brush, const QVector<double>& stops, const QVector<double>& lightFactors, Qt::Orientation orientation);

	/// @brief Construct a radial gradient from a QGradientStops object, a focal radius and focal angle (see QRadialGradient class for more details).
	VipAdaptativeGradient(const QGradientStops& gradientStops, double focalRadius, double focalAngle, Vip::ValueType value_type = Vip::Relative);
	/// @brief Construct a radial gradient from a brush, relative stop values (from 0 to 1) and light factors, a focal radius and focal angle (see QRadialGradient class for more details).
	/// At each stop, the brush color will be lightened by the corresponding factor using QColor::lighter() function.
	VipAdaptativeGradient(const QBrush& brush, const QVector<double>& stops, const QVector<double>& lightFactors, double focalRadius, double focalAngle, Vip::ValueType value_type = Vip::Relative);

	/// @brief Construct a conical gradient using a QGradientStops object
	VipAdaptativeGradient(const QGradientStops& gradientStops);
	/// @brief Construct a conical gradient using a brush, relative stop values (from 0 to 1) and light factors.
	/// At each stop, the brush color will be lightened by the corresponding factor using QColor::lighter() function.
	VipAdaptativeGradient(const QBrush& brush, const QVector<double>& stops, const QVector<double>& lightFactors);

	/// @brief Tells is the gradient is transparent
	bool isTransparent() const;

	/// @brief Set the gradient type to linear and keep previously set brush, stops, light factors...
	void setLinear(Qt::Orientation orientation = Qt::Horizontal);
	/// @brief Set the gradient type to radial and keep previously set brush, stops, light factors...
	void setRadial(double focalRadius = 0, double focalAngle = 0, Vip::ValueType value_type = Vip::Relative);
	/// @brief Set the gradient type to conical and keep previously set brush, stops, light factors...
	void setConical();
	/// @brief Set the gradient type to NoGradient (the default brush will be used)
	void unset();
	/// @brief Returns the gradient type
	Type type() const;

	/// @brief For linear gradient, returns its orientation
	Qt::Orientation orientation() const { return d_data->orientation; }
	/// @brief For radial gradient, returns its focal radius
	double focalRadius() const { return d_data->focalRadius; }
	/// @brief For radial gradient, returns its focal angle
	double focalAngle() const { return d_data->focalAngle; }
	/// @brief For radial gradient, returns its focal value type (relative or absolute)
	Vip::ValueType focalValueType() const { return d_data->focalValueType; }

	void setGradientStops(const QGradientStops&);
	const QGradientStops& gradientStops() const;

	void setLightFactors(const QVector<double>& stops, const QVector<double>& lightFactors);
	const QVector<double>& lightFactors() const;
	const QVector<double>& stops() const;

	void setBrush(const QBrush& brush);
	const QBrush& brush() const;
	QBrush& brush();

	QBrush createBrush(const QRectF& rect) const;
	QBrush createBrush(const QPointF& center, const VipPie& pie) const;

	QBrush createBrush(const QBrush& other_brush, const QRectF& rect) const;
	QBrush createBrush(const QBrush& other_brush, const QPointF& center, const VipPie& pie) const;

	bool operator==(const VipAdaptativeGradient& other) const;
	bool operator!=(const VipAdaptativeGradient& other) const;

private:
	struct PrivateData : QSharedData
	{
		PrivateData()
		  : type(NoGradient)
		  , orientation(Qt::Horizontal)
		  , focalRadius(0)
		  , focalAngle(0)
		  , focalValueType(Vip::Relative)
		{
		}

		Type type;
		QBrush brush;
		QGradientStops gradientStops;
		QVector<double> lightFactors;
		QVector<double> stops;

		Qt::Orientation orientation;
		double focalRadius;
		double focalAngle;
		Vip::ValueType focalValueType;
	};

	QSharedDataPointer<PrivateData> d_data;
};

Q_DECLARE_METATYPE(QGradientStop)
Q_DECLARE_METATYPE(QGradientStops)
Q_DECLARE_METATYPE(VipAdaptativeGradient)

class QDataStream;

VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipAdaptativeGradient& grad);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipAdaptativeGradient& grad);

/// @}
// end Plotting

#endif
