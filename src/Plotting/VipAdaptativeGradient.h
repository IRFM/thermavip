#ifndef VIP_ADAPTATIVE_GRADIENT_H
#define VIP_ADAPTATIVE_GRADIENT_H

#include <QSharedDataPointer>
#include <QGradient>
#include <QVector>
#include "VipGlobals.h"

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
	VipAdaptativeGradient(const QBrush & brush = QBrush());

	/// @brief Construct linear gradient using QGradientStops object ranging from 0 to 1
	VipAdaptativeGradient(const QGradientStops & gradientStops, Qt::Orientation orientation );
	/// @brief Construct linear gradient using a brush, relative stop values (from 0 to 1) and light factors.
	/// At each stop, the brush color will be lightened by the corresponding factor using QColor::lighter() function.
	VipAdaptativeGradient(const QBrush & brush, const QVector<double> & stops, const QVector<double> & lightFactors, Qt::Orientation orientation );

	/// @brief Construct a radial gradient from a QGradientStops object, a focal radius and focal angle (see QRadialGradient class for more details).
	VipAdaptativeGradient(const QGradientStops & gradientStops, double focalRadius , double focalAngle, Vip::ValueType value_type = Vip::Relative );
	/// @brief Construct a radial gradient from a brush, relative stop values (from 0 to 1) and light factors, a focal radius and focal angle (see QRadialGradient class for more details).
	/// At each stop, the brush color will be lightened by the corresponding factor using QColor::lighter() function.
	VipAdaptativeGradient(const QBrush & brush,const QVector<double> & stops, const QVector<double> & lightFactors, double focalRadius , double focalAngle, Vip::ValueType value_type = Vip::Relative );

	/// @brief Construct a conical gradient using a QGradientStops object
	VipAdaptativeGradient(const QGradientStops & gradientStops );
	/// @brief Construct a conical gradient using a brush, relative stop values (from 0 to 1) and light factors.
	/// At each stop, the brush color will be lightened by the corresponding factor using QColor::lighter() function.
	VipAdaptativeGradient(const QBrush & brush,const QVector<double> & stops, const QVector<double> & lightFactors );

	/// @brief Tells is the gradient is transparent
	bool isTransparent() const;

	/// @brief Set the gradient type to linear and keep previously set brush, stops, light factors...
	void setLinear(Qt::Orientation orientation = Qt::Horizontal);
	/// @brief Set the gradient type to radial and keep previously set brush, stops, light factors...
	void setRadial(double focalRadius = 0, double focalAngle = 0, Vip::ValueType value_type = Vip::Relative );
	/// @brief Set the gradient type to conical and keep previously set brush, stops, light factors...
	void setConical();
	/// @brief Set the gradient type to NoGradient (the default brush will be used)
	void unset();
	/// @brief Returns the gradient type
	Type type() const;

	/// @brief For linear gradient, returns its orientation
	Qt::Orientation orientation() const {return d_data->orientation;}
	/// @brief For radial gradient, returns its focal radius
	double focalRadius() const {return d_data->focalRadius;}
	/// @brief For radial gradient, returns its focal angle
	double focalAngle() const {return d_data->focalAngle;}
	/// @brief For radial gradient, returns its focal value type (relative or absolute)
	Vip::ValueType focalValueType() const {return d_data->focalValueType;}


	void setGradientStops(const QGradientStops & );
	const QGradientStops & gradientStops() const;

	void setLightFactors(const QVector<double> & stops, const QVector<double> & lightFactors);
	const QVector<double> & lightFactors() const;
	const QVector<double> & stops() const;

	void setBrush(const QBrush & brush);
	const QBrush & brush() const;
	QBrush & brush();

	QBrush createBrush(const QRectF & rect) const;
	QBrush createBrush(const QPointF & center, const VipPie & pie) const;

	QBrush createBrush(const QBrush & other_brush, const QRectF & rect) const;
	QBrush createBrush(const QBrush & other_brush, const QPointF & center, const VipPie & pie) const;

	bool operator==(const VipAdaptativeGradient & other) const;
	bool operator!=(const VipAdaptativeGradient & other) const;

private:


	struct PrivateData : QSharedData
	{
		PrivateData()
		: type(NoGradient),
		  orientation(Qt::Horizontal),
		  focalRadius(0),
		  focalAngle(0),
		  focalValueType(Vip::Relative)
		{}

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


Q_DECLARE_METATYPE(QGradientStops)
Q_DECLARE_METATYPE(VipAdaptativeGradient)

class QDataStream;

VIP_PLOTTING_EXPORT QDataStream & operator<<(QDataStream & stream, const VipAdaptativeGradient & grad);
VIP_PLOTTING_EXPORT QDataStream & operator>>(QDataStream & stream, VipAdaptativeGradient & grad);

// class Brush
// {
// public:
//
// Brush();
// Brush(const QBrush& , const VipAdaptativeGradient & = VipAdaptativeGradient());
//
// void setBrush(const QBrush &);
// const QBrush & brush() const;
//
// void setAdaptativeGradient(const VipAdaptativeGradient&);
// const VipAdaptativeGradient& adaptativeGradient() const;
//
// QBrush createBrush() const;
// operator QBrush() const;
// };
//
//
// class Pen
// {
// public:
//
// Pen();
// Pen(const Pen& , const VipAdaptativeGradient & = VipAdaptativeGradient());
//
// void setBrush(const Pen &);
// const Pen & brush() const;
//
// void setAdaptativeGradient(const VipAdaptativeGradient&);
// const VipAdaptativeGradient& adaptativeGradient() const;
//
// Pen createBrush() const;
// operator QBrush() const;
// };

/// @}
//end Plotting

#endif
