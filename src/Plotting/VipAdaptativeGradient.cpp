#include "VipAdaptativeGradient.h"
#include "VipPie.h"

#include <QBrush>
#include <QRectF>

static QGradientStops gradientStopsFromColor(const QColor & c, const QVector<double> & stops, const QVector<double> & lightFactors)
{
	QGradientStops gstops(stops.size());
	for(int i=0; i < stops.size(); ++i)
	{
		gstops[i] = QGradientStop(stops[i],c.lighter(static_cast<int>(lightFactors[i])));
	}

	return gstops;
}



static QRadialGradient buildRadialGradient(const QPointF & center, const VipPie & pie, double focalRadius, double focalAngle, Vip::ValueType value_type, const QGradientStops & stops)
{
	double radius = pie.maxRadius();

	QPointF c = center;
	if(pie.offsetToCenter())
	{
		//move center
		QLineF line(center,QPointF(center.x(),center.y()-pie.offsetToCenter()));
		line.setAngle(pie.meanAngle());
		c = line.p2();
	}

	QPointF focal;
	if(value_type == Vip::Relative)
	{
		QLineF line(c, QPointF(c.x(),c.y() - radius * focalRadius));
		line.setAngle(pie.startAngle() + focalAngle * pie.sweepLength());
		focal = line.p2();
	}
	else
	{
		QLineF line(c, QPointF(c.x(),c.y() - focalRadius));
		line.setAngle(focalAngle);
		focal = line.p2();
	}


	QRadialGradient grad(c,pie.maxRadius(),focal);
	double factor = pie.radiusExtent() / pie.maxRadius();

	for(int i=0; i < stops.size(); ++i)
	{
		double new_stop = 1+ (stops[i].first -1) * factor;
		grad.setColorAt(new_stop, stops[i].second);
	}

	return grad;
}

static QRadialGradient buildRadialGradient(const QRectF & rect, double focalRadius, double focalAngle, Vip::ValueType value_type, const QGradientStops & stops)
{
	double radius = qMax(rect.width()/2,rect.height()/2);

	QPointF c = rect.center();
	QPointF focal;
	if(value_type == Vip::Relative)
	{
		QLineF line(c, QPointF(c.x(),c.y() - radius * focalRadius));
		line.setAngle( focalAngle * 360.0);
		focal = line.p2();
	}
	else
	{
		QLineF line(c, QPointF(c.x(),c.y() - focalRadius));
		line.setAngle(focalAngle);
		focal = line.p2();
	}

	QRadialGradient grad(c,radius,focal);
	grad.setStops(stops);

	return grad;
}


static QConicalGradient buildConicalGradient(const QPointF & center, const VipPie & pie, const QGradientStops & stops)
{
	QPointF c = center;
	if(pie.offsetToCenter())
	{
		//move center
		QLineF line(center,QPointF(center.x(),center.y()-pie.offsetToCenter()));
		line.setAngle(pie.meanAngle());
		c = line.p2();
	}

	QConicalGradient grad(c,pie.startAngle());
	double factor = pie.sweepLength() / 360;

	for(int i=0; i < stops.size(); ++i)
	{
		double new_stop = (stops[i].first) * factor;
		grad.setColorAt(new_stop, stops[i].second);
	}

	return grad;
}

static QConicalGradient buildConicalGradient(const QRectF & rect, const QGradientStops & stops)
{
	QPointF c = rect.center();

	QConicalGradient grad(c,0);
	grad.setStops(stops);

	return grad;
}

static QLinearGradient buildLinearGradient(const QRectF & rect, Qt::Orientation orientation, const QGradientStops & stops)
{
	QLinearGradient grad;
	grad.setStops(stops);

	if(orientation == Qt::Horizontal)
	{
		grad.setStart(rect.topLeft());
		grad.setFinalStop(rect.topRight());
	}
	else
	{
		grad.setStart(rect.topLeft());
		grad.setFinalStop(rect.bottomLeft());
	}

	return grad;
}


static QLinearGradient buildLinearGradient(const QPointF & center, const VipPie & pie, Qt::Orientation orientation, const QGradientStops & stops)
{
	return buildLinearGradient(QRectF(center - QPointF(pie.maxRadius(),pie.maxRadius()), center + QPointF(pie.maxRadius(),pie.maxRadius())),orientation,stops);
}









VipAdaptativeGradient::VipAdaptativeGradient(const QBrush & brush)
:d_data(new PrivateData())
{
	d_data->brush = brush;
}

VipAdaptativeGradient::VipAdaptativeGradient(const QGradientStops & gradientStops, Qt::Orientation orientation )
:d_data(new PrivateData())
{
	d_data->gradientStops = gradientStops;
	setLinear(orientation);
}

VipAdaptativeGradient::VipAdaptativeGradient(const QBrush & brush, const QVector<double> & stops, const QVector<double> & lightFactors, Qt::Orientation orientation )
:d_data(new PrivateData())
{
	d_data->brush = brush;
	d_data->stops = stops;
	d_data->lightFactors = lightFactors;
	setLinear(orientation);
}

VipAdaptativeGradient::VipAdaptativeGradient(const QGradientStops & gradientStops, double focalRadius , double focalAngle, Vip::ValueType value_type )
:d_data(new PrivateData())
{
	d_data->gradientStops = gradientStops;
	setRadial(focalRadius,focalAngle,value_type);
}

VipAdaptativeGradient::VipAdaptativeGradient(const QBrush & brush, const QVector<double> & stops, const QVector<double> & lightFactors, double focalRadius , double focalAngle, Vip::ValueType value_type )
:d_data(new PrivateData())
{
	d_data->brush = brush;
	d_data->stops = stops;
	d_data->lightFactors = lightFactors;
	setRadial(focalRadius,focalAngle,value_type);
}

VipAdaptativeGradient::VipAdaptativeGradient(const QGradientStops & gradientStops )
:d_data(new PrivateData())
{
	d_data->gradientStops = gradientStops;
	setConical();
}

VipAdaptativeGradient::VipAdaptativeGradient(const QBrush & brush, const QVector<double> & stops, const QVector<double> & lightFactors )
:d_data(new PrivateData())
{
	d_data->brush = brush;
	d_data->stops = stops;
	d_data->lightFactors = lightFactors;
	setConical();
}

bool VipAdaptativeGradient::isTransparent() const
{
	bool is_transparent_brush = (d_data->brush.style() == Qt::NoBrush || d_data->brush.color().alpha() == 0);
	return is_transparent_brush && (d_data->gradientStops.isEmpty());
}

void VipAdaptativeGradient::setLinear(Qt::Orientation orientation)
{
	d_data->type = Linear;
	d_data->orientation = orientation;
}

void VipAdaptativeGradient::setRadial(double focalRadius, double focalAngle, Vip::ValueType value_type)
{
	d_data->type = Radial;
	d_data->focalRadius = focalRadius;
	d_data->focalAngle = focalAngle;
	d_data->focalValueType = value_type;
}

void VipAdaptativeGradient::setConical()
{
	d_data->type = Conical;
}

void VipAdaptativeGradient::unset() 
{
	d_data->type = NoGradient;
}

VipAdaptativeGradient::Type VipAdaptativeGradient::type() const
{
	return d_data->type;
}

void VipAdaptativeGradient::setGradientStops(const QGradientStops & stops)
{
	d_data->gradientStops = stops;
	d_data->lightFactors.clear();
	d_data->stops.clear();
}

const QGradientStops & VipAdaptativeGradient::gradientStops() const
{
	return d_data->gradientStops;
}

void VipAdaptativeGradient::setLightFactors(const QVector<double> & stops, const QVector<double> & lightFactors)
{
	d_data->lightFactors = lightFactors;
	d_data->stops = stops;
	d_data->gradientStops.clear();
}

const QVector<double> & VipAdaptativeGradient::lightFactors() const
{
	return d_data->lightFactors;
}

const QVector<double> & VipAdaptativeGradient::stops() const
{
	return d_data->stops;
}

void VipAdaptativeGradient::setBrush(const QBrush & brush)
{
	d_data->brush = brush;
}

const QBrush & VipAdaptativeGradient::brush() const
{
	return d_data->brush;
}

QBrush & VipAdaptativeGradient::brush()
{
	return d_data->brush;
}

QBrush VipAdaptativeGradient::createBrush(const QRectF & rect) const
{
	return createBrush(d_data->brush,rect);
}

QBrush VipAdaptativeGradient::createBrush(const QPointF & center, const VipPie & pie) const
{
	return createBrush(d_data->brush,center,pie);
}

QBrush VipAdaptativeGradient::createBrush(const QBrush & other_brush,const QRectF & rect) const
{
	if(d_data->type == NoGradient)
	{
		return other_brush;
	}

	QGradientStops stops = d_data->gradientStops;
	if(d_data->lightFactors.size())
	{
		stops = gradientStopsFromColor(other_brush.color(), d_data->stops, d_data->lightFactors);
	}

	if(d_data->type == Linear)
	{
		return QBrush(buildLinearGradient(rect,d_data->orientation,stops));
	}
	else if(d_data->type == Radial)
	{
		return QBrush(buildRadialGradient(rect,d_data->focalRadius, d_data->focalAngle, d_data->focalValueType,stops));
	}
	else
	{
		return QBrush(buildConicalGradient(rect,stops));
	}
}

QBrush VipAdaptativeGradient::createBrush(const QBrush & other_brush, const QPointF & center, const VipPie & pie) const
{
	if(d_data->type == NoGradient)
	{
		return other_brush;
	}

	QGradientStops stops = d_data->gradientStops;
	if(d_data->lightFactors.size())
	{
		stops = gradientStopsFromColor(other_brush.color(), d_data->stops, d_data->lightFactors);
	}

	if(d_data->type == Linear)
	{
		return QBrush(buildLinearGradient(center,pie,d_data->orientation,stops));
	}
	else if(d_data->type == Radial)
	{
		return QBrush(buildRadialGradient(center,pie,d_data->focalRadius, d_data->focalAngle,d_data->focalValueType,stops));
	}
	else
	{
		return QBrush(buildConicalGradient(center,pie,stops));
	}
}

bool VipAdaptativeGradient::operator==(const VipAdaptativeGradient & other) const
{
	return other.d_data->brush == d_data->brush &&
		other.d_data->focalAngle == d_data->focalAngle &&
		other.d_data->focalRadius == d_data->focalRadius &&
		other.d_data->focalValueType == d_data->focalValueType &&
		other.d_data->gradientStops == d_data->gradientStops &&
		other.d_data->lightFactors == d_data->lightFactors &&
		other.d_data->orientation == d_data->orientation &&
		other.d_data->stops == d_data->stops;
}

bool VipAdaptativeGradient::operator!=(const VipAdaptativeGradient & other) const
{
	return !((*this) == other);
}


#include <QDataStream>


QDataStream & operator<<(QDataStream & stream, const VipAdaptativeGradient & grad)
{
	return stream << (int)grad.type() <<(int)grad.orientation()<<(int)grad.focalValueType()<<grad.focalRadius()<<grad.focalAngle()<<grad.gradientStops() <<grad.lightFactors()<<grad.stops() <<grad.brush();
}

QDataStream & operator>>(QDataStream & stream, VipAdaptativeGradient & grad)
{
	int type,orientation,focalValueType;
	double focalRadius,focalAngle;
	QGradientStops gradientStops;
	QVector<double> stops,lightFactors;
	QBrush brush;

	stream >> type >> orientation >>focalValueType >> focalRadius >> focalAngle >> gradientStops >>lightFactors >> stops >>brush;
	if(type == VipAdaptativeGradient::Linear)
	{
		grad.setLinear(Qt::Orientation(orientation));
	}
	else if(type == VipAdaptativeGradient::Radial)
	{
		grad.setRadial(focalRadius,focalAngle,Vip::ValueType(focalValueType));
	}
	else if(type == VipAdaptativeGradient::Conical)
	{
		grad.setConical();
	}

	if(gradientStops.size())
	{
		grad.setGradientStops(gradientStops);
	}
	else
	{
		grad.setLightFactors(stops,lightFactors);
	}

	grad.setBrush(brush);

	return stream;
}

static int registerStreamOperators()
{
	qRegisterMetaTypeStreamOperators<QGradientStops>("QGradientStops");
	qRegisterMetaTypeStreamOperators<VipAdaptativeGradient>("VipAdaptativeGradient");
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();

