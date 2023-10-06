#include <cmath>
#include <map>

#include <QDateTime>
#include <qlocale.h>
#include <qpainter.h>
#include <qpalette.h>

#include "VipPainter.h"
#include "VipPie.h"
#include "VipScaleDraw.h"
#include "VipScaleMap.h"
#include "VipShapeDevice.h"
#include "VipValueTransform.h"

// static QDateTime addDays(const QDateTime time, double days)
// {
// return time.addMSecs(86400000*days);
// }
//
// static QDateTime addMonths(const QDateTime time, double months)
// {
// QDateTime res = time.addMonths(int(floor(months)));
// double days = res.date().daysInMonth() * (months - floor(months) );
// return addDays(res,days);
// }
//
// static QDateTime addYears(const QDateTime time, double years)
// {
// return addMonths(time,years*12);
// }
//
// static double elapsedDays(const QDateTime from, const QDateTime & to)
// {
// return from.msecsTo(to) / double(86400000);
// }

VipValueToText::VipValueToText()
  : d_exponent(0)
  , d_max_label_size(0)
  , d_autoExponent(false)
  , d_pow(1)
{
}

VipValueToText::~VipValueToText() {}

void VipValueToText::setExponent(int e)
{
	if (d_exponent != e) {
		d_exponent = e;
		d_pow = qPow(10, -e);
	}
}
int VipValueToText::exponent() const
{
	return d_exponent;
}

QString VipValueToText::exponentText() const
{
	if (d_exponent)
		return (" &#215;10<sup>" + QString::number(d_exponent) + "</sup>");
	return QString();
}

void VipValueToText::setAutomaticExponent(bool a)
{
	d_autoExponent = a;
}
bool VipValueToText::automaticExponent() const
{
	return d_autoExponent;
}
int VipValueToText::maxLabelSize() const
{
	return d_max_label_size;
}
void VipValueToText::setMaxLabelSize(int min_label_size)
{
	d_max_label_size = min_label_size;
}

int VipValueToText::findBestExponent(const VipAbstractScaleDraw* scale) const
{
	// get all labels
	bool integer_only = true;
	int max_size = 0;
	qint64 max_val = 0;
	int sum = 0, count = 0;
	const VipScaleDiv::TickList ticks = scale->scaleDiv().ticks(VipScaleDiv::MajorTick);
	for (int i = 0; i < ticks.size(); ++i) {
		vip_double v = ticks[i];
		qint64 iv = (qint64)v;
		if (v != iv) {
			integer_only = false;
			break;
		}
		iv = std::abs(iv);
		int len = (int)(std::log10(iv) + 1); // QString::number((qint64)std::abs(v)).size();
		max_size = qMax(max_size, len);
		if (len > 1) { // avoid 0
			sum += len;
			max_val = (++count == 1) ? iv : qMax(iv, max_val);
		}
	}

	int res = 0;
	if (max_size > d_max_label_size && integer_only) {
		res = (sum / count) - 1;
		if (res < 0)
			res = 0;
		if (max_val < 1)
			res = -res;
	}
	return res;
}

QString VipValueToText::convert(vip_double value, VipScaleDiv::TickType) const
{
	value = value * d_pow;
	if (qFuzzyCompare(value + 1.0, (vip_double)1.0)) {
		value = 0.0;
	}

	if ((qint64)value == value)
		return m_locale.toString((qint64)value);
	else
		return m_locale.toString((double)value);
}

vip_double VipValueToText::fromString(const QString& text, bool* ok) const
{
	vip_double res = m_locale.toDouble(text, ok);
	res /= d_pow;
	return res;
}

QList<VipScaleText> VipValueToText::additionalText(const VipAbstractScaleDraw*) const
{
	return QList<VipScaleText>();
}

VipValueToFormattedText::VipValueToFormattedText(const QString& text)
  : d_text(text)
{
}

void VipValueToFormattedText::setFormat(const QString& format)
{
	d_text = format;
}
QString VipValueToFormattedText::format() const
{
	return d_text;
}

QString VipValueToFormattedText::convert(vip_double value, VipScaleDiv::TickType tick) const
{
	if (d_text.size()) {
		value = value * d_pow;
		if (qFuzzyCompare(value + 1.0, (vip_double)1.0)) {
			value = 0.0;
		}
		return QString::asprintf(d_text.toLatin1().data(), (double)value);
	}
	return VipValueToText::convert(value, tick);
}

VipValueToDate::VipValueToDate(const QString& format, ValueType type, double multiply_factor)
  : d_format(format)
  , d_type(type)
  , d_ref(QDateTime::fromMSecsSinceEpoch(0))
{
	setMultiplyFactor(multiply_factor);
}

QDateTime VipValueToDate::reference() const
{
	return d_ref;
}

void VipValueToDate::setReference(const QDateTime& ref)
{
	d_ref = ref;
}

void VipValueToDate::setFormat(const QString& format)
{
	d_format = format;
}
QString VipValueToDate::format() const
{
	return d_format;
}

QString VipValueToDate::convert(vip_double value, VipScaleDiv::TickType tick) const
{
	if (d_format.isEmpty())
		return VipValueToText::convert(value, tick);

	value *= multiplyFactor();

	QDateTime ref = d_ref;

	switch (d_type) {
		case NanoSeconds: {
			ref = ref.addMSecs(value * 1000000 * multiplyFactor());
			return ref.toString(d_format);
		}
		case MicroSeconds: {
			ref = ref.addMSecs(value * 1000 * multiplyFactor());
			return ref.toString(d_format);
		}
		case MilliSeconds: {
			ref = ref.addMSecs(value * multiplyFactor());
			return ref.toString(d_format);
		}
		case Seconds: {
			ref = ref.addMSecs(value * 1000 * multiplyFactor());
			return ref.toString(d_format);
		}
		case Minutes: {
			ref = ref.addMSecs(value * 60000 * multiplyFactor());
			return ref.toString(d_format);
		}
		case Hours: {
			ref = ref.addMSecs(value * 3600000 * multiplyFactor());
			return ref.toString(d_format);
		}
		case Days: {
			ref = ref.addMSecs(value * 86400000 * multiplyFactor());
			return ref.toString(d_format);
		}
	}

	return QString();
}

vip_double VipValueToDate::fromString(const QString& text, bool* ok) const
{
	if (d_format.isEmpty())
		return VipValueToText::fromString(text, ok);

	QDateTime this_time = QDateTime::fromString(text, d_format);
	if (!this_time.isValid()) {
		if (ok)
			*ok = false;
		return 0;
	}
	else if (ok)
		*ok = true;

	QDateTime ref = d_ref;

	switch (d_type) {
		case NanoSeconds: {
			return (ref.msecsTo(this_time)) * 1000000 / multiplyFactor();
		}
		case MicroSeconds: {
			return (ref.msecsTo(this_time)) * 1000 / multiplyFactor();
		}
		case MilliSeconds: {
			return (ref.msecsTo(this_time)) / multiplyFactor();
		}
		case Seconds: {
			return (ref.msecsTo(this_time)) * 0.001 / multiplyFactor();
		}
		case Minutes: {
			return (ref.msecsTo(this_time)) / 60000 / multiplyFactor();
		}
		case Hours: {
			return (ref.msecsTo(this_time)) / 3600000 / multiplyFactor();
		}
		case Days: {
			return (ref.msecsTo(this_time)) / 86400000 / multiplyFactor();
		}
	}

	return 0.;
}

VipValueToTime::VipValueToTime(TimeType type, double startValue)
  : type(type)
  , startValue(startValue)
  , drawAdditionalText(true)
  , fixedStartValue(false)
  , displayType(Double)
{
	format = "dd/MM/yyyy\nhh:mm:ss.zzz";
}

QString VipValueToTime::convert(vip_double value, VipScaleDiv::TickType) const
{
	if (displayType == Integer) {
		switch (type) {
			case NanoSeconds:
				return locale().toString((qint64)value);
			case NanoSecondsSE:
				return locale().toString((qint64)(value - startValue));
			case MicroSeconds:
				return locale().toString((qint64)(value / 1000));
			case MicroSecondsSE:
				return locale().toString((qint64)((value - startValue) / 1000));
			case MilliSeconds:
				return locale().toString((qint64)(value / 1000000));
			case MilliSecondsSE:
				return locale().toString((qint64)((value - startValue) / 1000000));
			case Seconds:
				return locale().toString((qint64)(value / 1000000000));
			case SecondsSE:
				return locale().toString((qint64)((value - startValue) / 1000000000));
		}
	}
	else if (displayType == AbsoluteDateTime) {
		return QDateTime::fromMSecsSinceEpoch(value / 1000000).toString(format);
	}
	else {
		switch (type) {
			case NanoSeconds:
				return locale().toString((double)value * multiplyFactor());
			case NanoSecondsSE:
				return locale().toString((double)(value - startValue) * multiplyFactor());
			case MicroSeconds:
				return locale().toString((double)value / 1000 * multiplyFactor());
			case MicroSecondsSE:
				return locale().toString((double)(value - startValue) / 1000 * multiplyFactor());
			case MilliSeconds:
				return locale().toString((double)value / 1000000 * multiplyFactor());
			case MilliSecondsSE:
				return locale().toString((double)(value - startValue) / 1000000 * multiplyFactor());
			case Seconds:
				return locale().toString((double)value / 1000000000 * multiplyFactor());
			case SecondsSE:
				return locale().toString((double)(value - startValue) / 1000000000 * multiplyFactor());
		}
	}
	return QString();
}

vip_double VipValueToTime::fromString(const QString& text, bool* ok) const
{
	if (displayType == AbsoluteDateTime) {
		if (ok)
			*ok = true;
		return QDateTime::fromString(text, format).toMSecsSinceEpoch() * 1000000;
	}
	else {
		double m = multiplyFactor();
		if (displayType == Integer)
			m = 1;
		switch (type) {
			case NanoSeconds:
				return text.toDouble(ok) / m;
			case NanoSecondsSE:
				return text.toDouble(ok) / m + startValue;
			case MicroSeconds:
				return text.toDouble(ok) / m * 1000;
			case MicroSecondsSE:
				return text.toDouble(ok) / m * 1000 + startValue;
			case MilliSeconds:
				return text.toDouble(ok) / m * 1000000;
			case MilliSecondsSE:
				return text.toDouble(ok) / m * 1000000 + startValue;
			case Seconds:
				return text.toDouble(ok) / m * 1000000000;
			case SecondsSE:
				return text.toDouble(ok) / m * 1000000000 + startValue;
		}
	}

	if (ok)
		*ok = false;
	return 0;
}

QList<VipScaleText> VipValueToTime::additionalText(const VipAbstractScaleDraw* scale) const
{
	// TOCHECK: comment this
	// retrieve the start value
	if (!fixedStartValue)
		const_cast<vip_double&>(startValue) = scale->scaleDiv().bounds().minValue();

	if (!drawAdditionalText)
		return QList<VipScaleText>();

	QList<VipScaleText> res;

	// QColor c = textStyle(VipScaleDiv::MajorTick).textPen().color();

	if (type == NanoSecondsSE || type == MicroSecondsSE || type == MilliSecondsSE || type == SecondsSE) {
		res = QList<VipScaleText>() << VipScaleText(
			VipText(QDateTime::fromMSecsSinceEpoch(startValue / 1000000).toString(format)), scale->scaleDiv().bounds().minValue(), QTransform(), VipScaleDiv::MajorTick);
	}

	if (res.size()) {
		res.first().text.setAlignment(Qt::AlignCenter);
	}

	return res;
}

VipValueToTime* VipValueToTime::copy() const
{
	VipValueToTime* c = new VipValueToTime(type, startValue);
	c->displayType = displayType;
	c->format = format;
	return c;
}

QString VipValueToTime::timeUnit() const
{
	switch (type) {
		case NanoSeconds:
			return "ns";
		case NanoSecondsSE:
			return "ns";
		case MicroSeconds:
			return "us";
		case MicroSecondsSE:
			return "us";
		case MilliSeconds:
			return "ms";
		case MilliSecondsSE:
			return "ms";
		case Seconds:
			return "s";
		case SecondsSE:
			return "s";
		default:
			return QString();
	}
}

static qint64 year2000 = QDateTime::fromString("2000", "yyyy").toMSecsSinceEpoch() * 1000000;

QString VipValueToTime::convert(vip_double value, TimeType type, const QString& format)
{
	if (type % 2) {
		return QDateTime::fromMSecsSinceEpoch(value / 1000000).toString(format);
	}
	else {
		VipValueToTime converter;
		converter.type = type;
		QString res = converter.convert(value, VipScaleDiv::MajorTick);
		if (type == VipValueToTime::NanoSeconds)
			res += " ns";
		else if (type == VipValueToTime::MicroSeconds)
			res += " us";
		else if (type == VipValueToTime::MilliSeconds)
			res += " ms";
		else
			res += " s";
		return res;
	}
}

VipValueToTime::TimeType VipValueToTime::findBestTimeUnit(const VipInterval& time_interval)
{
	if (time_interval.minValue() > year2000) {
		// if the start time is above nano seconds since year 2000, we can safely consider that this is a date
		// only modify the time type if we are not in (unit) since epoch

		// get the scale time range and deduce the unit from it
		vip_double range = time_interval.width();

		VipValueToTime::TimeType time = VipValueToTime::NanoSecondsSE;

		if (range > 1000000000) // above 1s
			time = VipValueToTime::SecondsSE;
		else if (range > 1000000) // above 1ms
			time = VipValueToTime::MilliSecondsSE;
		else if (range > 1000) // above 1micros
			time = VipValueToTime::MicroSecondsSE;

		return time;
	}
	else {
		// get the scale time range and deduce the unit from it
		vip_double range = time_interval.width();
		VipValueToTime::TimeType time = VipValueToTime::NanoSeconds;

		if (range > 1000000000) // above 1s
			time = VipValueToTime::Seconds;
		else if (range > 1000000) // above 1ms
			time = VipValueToTime::MilliSeconds;
		else if (range > 1000) // above 1micros
			time = VipValueToTime::MicroSeconds;

		return time;
	}
}

VipTimeToText::VipTimeToText(const QString& format, TimeType time_type, TextType type)
  : VipFixedValueToText(QString(), type)
  , d_timeType(time_type)
  , d_labelFormat(format)
  , d_additionalFormat(format)
{
}

void VipTimeToText::setLabelFormat(const QString& fmt)
{
	d_labelFormat = fmt;
}
QString VipTimeToText::labelFormat() const
{
	return d_labelFormat;
}

void VipTimeToText::setAdditionalFormat(const QString& fmt)
{
	d_additionalFormat = fmt;
}
QString VipTimeToText::additionalFormat() const
{
	return d_additionalFormat;
}

void VipTimeToText::setTimeType(TimeType type)
{
	d_timeType = type;
}
VipTimeToText::TimeType VipTimeToText::timeType() const
{
	return d_timeType;
}

QString VipTimeToText::convert(vip_double value, VipScaleDiv::TickType) const
{
	if (textType() != AbsoluteValue)
		value -= startValue();
	value *= multiplyFactor();
	if (d_timeType == Milliseconds) {
		QTime time = QTime::fromMSecsSinceStartOfDay(static_cast<int>(qRound(value)));
		return time.toString(d_labelFormat);
	}
	return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(value)).toString(d_labelFormat);
}

vip_double VipTimeToText::fromString(const QString& text, bool* ok) const
{
	vip_double v = 0;
	if (d_timeType == Milliseconds) {
		QTime t = QTime::fromString(text, d_labelFormat);
		if (t.isNull()) {
			if (ok)
				*ok = false;
			return 0;
		}
		v = t.msecsSinceStartOfDay();
	}
	else {
		QDateTime t = QDateTime::fromString(text, d_labelFormat);
		if (t.isNull()) {
			if (ok)
				*ok = false;
			return 0;
		}
		v = t.toMSecsSinceEpoch();
	}
	v /= multiplyFactor();
	if (textType() != AbsoluteValue)
		v += startValue();
	return v;
}

QList<VipScaleText> VipTimeToText::additionalText(const VipAbstractScaleDraw*) const
{
	if (textType() != DifferenceValue)
		return QList<VipScaleText>();

	VipScaleText res;
	if (d_timeType == Milliseconds) {
		QTime time(0, 0, 0);
		time = time.addMSecs(static_cast<int>(qRound(startValue() * multiplyFactor())));
		res.text = time.toString(d_additionalFormat);
	}
	else
		res.text = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(startValue() * multiplyFactor())).toString(d_additionalFormat);

	res.tr = this->additionalTextTransform();
	res.value = startValue();
	return QList<VipScaleText>() << res;
}

/// Returns the vertical angle between [-90:90]
static vip_double verticalAngle(vip_double angle)
{
	while (angle < 0)
		angle += 360;
	while (angle > 360)
		angle -= 360;

	if (angle <= 90)
		return angle;
	else if (angle <= 270)
		return 180 - angle;
	else
		return angle - 360;
}

/// Returns the horizontal angle between [0:180]
static vip_double horizontalAngle(vip_double angle)
{
	while (angle < 0)
		angle += 360;
	while (angle > 360)
		angle -= 360;

	if (angle <= 180)
		return angle;
	else
		return 360 - angle;
}

/// Calculate the transformation that is needed to paint a text
/// depending on its alignment and rotation.
///
/// \param textTransform The text transformation type
/// \param textPosition The text position type
/// \param pos Position where to paint the label
/// \param size Size of the label
///
/// \return Transformation matrix
QTransform textTransformation(VipPolarScaleDraw::TextTransform textTransform, VipAbstractScaleDraw::TextPosition textPosition, double angle, const QPointF& pos, const QSizeF& size)
{
	QTransform transform;
	transform.translate(pos.x(), pos.y());

	switch (textTransform) {
		case VipAbstractScaleDraw::TextHorizontal: {
			double va = verticalAngle(angle);
			double ha = horizontalAngle(angle);
			double dx, dy;

			if (textPosition == VipAbstractScaleDraw::TextOutside) {
				dy = -((va + 90.) / 180.) * size.height();
				dx = -((ha / 180.) * size.width());

				// minor adjustements
				if (ha < 90) {
					// 45° and -45° from horizontal, right side
					double add = 1 - qAbs((ha - 45.) / 45.);
					dx += add;
					if (va > 0)
						dy -= add;
					else
						dy += add;
				}
			}
			else {
				dy = ((va - 90.) / 180.) * size.height();
				dx = -((180. - ha) / 180.) * size.width();

				// minor adjustements
				double add = (1 - qAbs((qAbs(va) - 45.) / 45.)) * 2;
				if (ha < 90)
					// right side
					dx -= add;
				else
					// left side
					dx += add;
				if (va > 0)
					dy += add; // top
				else
					dy -= add; // bottom
			}

			transform.translate(dx, dy);

			break;
		}
		case VipAbstractScaleDraw::TextCurved:
		case VipAbstractScaleDraw::TextParallel: {
			if (textPosition == VipAbstractScaleDraw::TextOutside) {
				if (angle > 0 && angle <= 180) {
					transform.rotate(90 - angle);
					transform.translate(-size.width() / 2.0, -size.height());
				}
				else {
					transform.rotate(270 - angle);
					transform.translate(-size.width() / 2.0, 0);
				}
			}
			else {
				if (angle > 0 && angle <= 180) {
					transform.rotate(90 - angle);
					transform.translate(-size.width() / 2.0, 0);
				}
				else {
					transform.rotate(270 - angle);
					transform.translate(-size.width() / 2.0, -size.height());
				}
			}
			break;
		}
		case VipAbstractScaleDraw::TextPerpendicular: {
			if (textPosition == VipAbstractScaleDraw::TextOutside) {
				if (angle > -90 && angle <= 90) {
					transform.rotate(-angle);
					transform.translate(0, -size.height() / 2);
				}
				else {
					transform.rotate(180 - angle);
					transform.translate(-size.width(), -size.height() / 2);
				}
			}
			else {
				if (angle > -90 && angle <= 90) {
					transform.rotate(-angle);
					transform.translate(-size.width(), -size.height() / 2);
				}
				else {
					transform.rotate(180 - angle);
					transform.translate(0, -size.height() / 2);
				}
			}
			break;
		}
	}

	return transform;
}

class VipAbstractScaleDraw::PrivateData
{
public:
	PrivateData()
	  : ticksPosition(VipAbstractScaleDraw::TicksInside)
	  , textPosition(VipAbstractScaleDraw::TextOutside)
	  , customTextStyle(VipAbstractScaleDraw::UseTickTextStyle)
	  , spacing(2.0)
	  , // divided by 2 , before was 4
	  // penWidth( 0 ),
	  minExtent(0.0)
	  , labelOverlap(false)
	  , dirtyOverlap(true)
	{
		components = VipAbstractScaleDraw::Backbone | VipAbstractScaleDraw::Ticks | VipAbstractScaleDraw::Labels;

		tickLength[VipScaleDiv::MinorTick] = 2.0; // we divided by 2 (before was 4,6 and 8)
		tickLength[VipScaleDiv::MediumTick] = 3.0;
		tickLength[VipScaleDiv::MajorTick] = 4.0;

		drawLabel[VipScaleDiv::MinorTick] = false;
		drawLabel[VipScaleDiv::MediumTick] = false;
		drawLabel[VipScaleDiv::MajorTick] = true;

		rotation[VipScaleDiv::MinorTick] = 0;
		rotation[VipScaleDiv::MediumTick] = 0;
		rotation[VipScaleDiv::MajorTick] = 0;

		textTransform[VipScaleDiv::MinorTick] = TextHorizontal;
		textTransform[VipScaleDiv::MediumTick] = TextHorizontal;
		textTransform[VipScaleDiv::MajorTick] = TextHorizontal;

		valueToText = QSharedPointer<VipValueToText>(new VipValueToText());

		// componentPen[0].setCosmetic(true);
		// componentPen[1].setCosmetic(true);
		// componentPen[2].setCosmetic(true);
		componentPen[0].setCosmetic(false);
		componentPen[1].setCosmetic(false);
		componentPen[2].setCosmetic(false);
		componentPen[0].setWidthF(1);
		componentPen[1].setWidthF(1);
		componentPen[2].setWidthF(1);

		labelArea.reset(new QPainterPath());
	}
	~PrivateData()
	{
		// make sure to clean the label area in case other scale draw use it
		*labelArea = QPainterPath();
	}

	int components;
	VipAbstractScaleDraw::TicksPosition ticksPosition;
	VipAbstractScaleDraw::TextPosition textPosition;
	VipAbstractScaleDraw::CustomTextStyle customTextStyle;
	VipScaleMap map;
	VipScaleDiv scaleDiv;

	double spacing;
	double tickLength[VipScaleDiv::NTickTypes];
	bool drawLabel[VipScaleDiv::NTickTypes];
	QTransform transform[VipScaleDiv::NTickTypes];
	QTransform transformInv[VipScaleDiv::NTickTypes];
	QPointF transformRef[VipScaleDiv::NTickTypes];
	double rotation[VipScaleDiv::NTickTypes];
	TextTransform textTransform[VipScaleDiv::NTickTypes];
	QPen componentPen[3];
	VipTextStyle styles[3];
	std::unique_ptr<VipTextStyle> additionalStyle;
	// int penWidth;

	double minExtent;

	QMap<vip_double, VipScaleText> customLabels;
	std::map<vip_double, VipScaleText> labelCache;
	QSharedPointer<QPainterPath> labelArea;
	QTransform painterTransform;
	QVector<QSharedPointer<QPainterPath>> otherLabelArea;
	bool labelOverlap;
	bool dirtyOverlap;

	VipText labelText[3];
	QSharedPointer<VipValueToText> valueToText;
	QMap<vip_double, VipScaleText> additionalText;

	VipInterval labelInterval;
};

/// \brief Constructor
///
/// The range of the scale is initialized to [0, 100],
/// The spacing (vipDistance between ticks and labels) is
/// set to 4, the tick lengths are set to 4,6 and 8 pixels
VipAbstractScaleDraw::VipAbstractScaleDraw()
{
	d_data = new VipAbstractScaleDraw::PrivateData;
}

//! Destructor
VipAbstractScaleDraw::~VipAbstractScaleDraw()
{
	delete d_data;
}

/// Change the transformation of the scale
/// \param transformation New scale transformation
void VipAbstractScaleDraw::setTransformation(VipValueTransform* transformation)
{
	d_data->map.setTransformation(transformation);
}

const VipValueTransform* VipAbstractScaleDraw::transformation() const
{
	return d_data->map.transformation();
}

/// En/Disable a component of the scale
///
/// \param component Scale component
/// \param enable On/Off
///
/// \sa hasComponent()
void VipAbstractScaleDraw::enableComponent(ScaleComponent component, bool enable)
{
	if (enable)
		d_data->components |= component;
	else
		d_data->components &= ~component;
}

/// Check if a component is enabled
///
/// \param component Component type
/// \return true, when component is enabled
/// \sa enableComponent()
bool VipAbstractScaleDraw::hasComponent(ScaleComponent component) const
{
	return (d_data->components & component);
}

int VipAbstractScaleDraw::components() const
{
	return d_data->components;
}

void VipAbstractScaleDraw::setComponents(int components)
{
	d_data->components = components;
}

/// Change the scale division
/// \param scaleDiv New scale division
void VipAbstractScaleDraw::setScaleDiv(const VipScaleDiv& scaleDiv)
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();

	// check if we need to invalidate the overlapping (only necessary if labels overlapping is not allowed)
	if (!d_data->labelOverlap) {
		vip_double epsilon = scaleDiv.range() / 1000.0;
		if (fabs(d_data->scaleDiv.bounds().minValue() - scaleDiv.bounds().minValue()) > epsilon || fabs(d_data->scaleDiv.bounds().maxValue() - scaleDiv.bounds().maxValue()) > epsilon)
			invalidateOverlap();
		else {
			// bounds are the same, just check the number of ticks
			if (d_data->drawLabel[VipScaleDiv::MajorTick] && d_data->scaleDiv.ticks(VipScaleDiv::MajorTick).size() != scaleDiv.ticks(VipScaleDiv::MajorTick).size())
				invalidateOverlap();
			else if (d_data->drawLabel[VipScaleDiv::MediumTick] && d_data->scaleDiv.ticks(VipScaleDiv::MediumTick).size() != scaleDiv.ticks(VipScaleDiv::MediumTick).size())
				invalidateOverlap();
			else if (d_data->drawLabel[VipScaleDiv::MinorTick] && d_data->scaleDiv.ticks(VipScaleDiv::MinorTick).size() != scaleDiv.ticks(VipScaleDiv::MinorTick).size())
				invalidateOverlap();
		}
	}
	d_data->scaleDiv = scaleDiv;
	d_data->map.setScaleInterval(scaleDiv.lowerBound(), scaleDiv.upperBound());
	// qint64 el1 = QDateTime::currentMSecsSinceEpoch() - st;
	// if (el1)
	// printf("el1 %i\n", (int)el1);
	// st = QDateTime::currentMSecsSinceEpoch();

	if (d_data->valueToText->supportExponent() && d_data->valueToText->automaticExponent()) {
		int exp = d_data->valueToText->findBestExponent(this);
		d_data->valueToText->setExponent(exp);
	}

	// qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st;
	// if (el2)
	// printf("el2 %i\n", (int)el2);
	// st = QDateTime::currentMSecsSinceEpoch();

	invalidateCache();

	// qint64 el3 = QDateTime::currentMSecsSinceEpoch() - st;
	// if (el3)
	// printf("el3 %i\n", (int)el3);
	// st = QDateTime::currentMSecsSinceEpoch();

	// //reset additional text
	// if(d_data->valueToText)
	// {
	// 	d_data->additionalText.clear();
	// 	const QList<VipScaleText> texts = d_data->valueToText->additionalText(this);
	//
	// 	for(int i=0; i < texts.size(); ++i)
	// 		d_data->additionalText[texts[i].value] = texts[i];
	//	}
}

const QMap<vip_double, VipScaleText>& VipAbstractScaleDraw::additionalText() const
{
	if (d_data->additionalText.size() == 0 && d_data->valueToText) {
		// reset additional text
		const QList<VipScaleText> texts = d_data->valueToText->additionalText(this);
		if (texts.size()) {
			const VipTextStyle& style = additionalTextStyle();
			for (int i = 0; i < texts.size(); ++i) {
				VipScaleText st = texts[i];
				st.text.setTextStyle(style);
				const_cast<QMap<vip_double, VipScaleText>&>(d_data->additionalText)[texts[i].value] = st;
			}
		}
	}
	return d_data->additionalText;
}

//! \return Map how to translate between scale and pixel values
const VipScaleMap& VipAbstractScaleDraw::scaleMap() const
{
	return d_data->map;
}

//! \return Map how to translate between scale and pixel values
VipScaleMap& VipAbstractScaleDraw::scaleMap()
{
	return d_data->map;
}

//! \return scale division
const VipScaleDiv& VipAbstractScaleDraw::scaleDiv() const
{
	return d_data->scaleDiv;
}

bool VipAbstractScaleDraw::drawLabelOverlap(QPainter* painter, vip_double v, const VipText& t, VipScaleDiv::TickType tick) const
{
	// TODO: we must fix the label overlapping!

	// if label might overlap, just draw the label
	if (true) { // d_data->labelOverlap || !d_data->dirtyOverlap) { //TODO: optimize
		drawLabel(painter, v, t, tick);
		return true;
	}
	// otherwise, make sure that no overlapping appears
	else {
		VipShapeDevice device;
		QPainter p(&device);
		drawLabel(&p, v, t, tick);
		QPainterPath text_shape = device.shape();
		// if we use other label areas, we need to work in device coordinates since other abstract scale might have a different transform
		if (!d_data->otherLabelArea.isEmpty())
			text_shape = d_data->painterTransform.map(device.shape());

		QRectF bounding = text_shape.boundingRect();
		if (!d_data->labelArea->intersects(text_shape)) {
			// check additional label overlapp
			for (int i = 0; i < d_data->otherLabelArea.size(); ++i) {
				QRectF b = d_data->otherLabelArea[i]->boundingRect();
				if (d_data->otherLabelArea[i]->intersects(text_shape))
					return false;
			}

			const_cast<QPainterPath&>(*d_data->labelArea).addPath(text_shape);
			drawLabel(painter, v, t, tick);
			return true;
		}
	}
	return false;
}

bool VipAbstractScaleDraw::drawTextOverlap(QPainter* painter, const VipText& t) const
{
	// if label might overlap, just draw the label
	if (true) { // d_data->labelOverlap) {
		t.draw(painter, t.textRect());
		return true;
	}
	// otherwise, make sure that no overlapping appears
	else {
		VipShapeDevice device;
		QPainter p(&device);
		t.draw(&p, t.textRect());
		QPainterPath text_shape = device.shape();
		if (!d_data->otherLabelArea.isEmpty())
			text_shape = d_data->painterTransform.map(device.shape());

		if (!d_data->labelArea->intersects(text_shape)) {
			// check additional label overlapp
			for (int i = 0; i < d_data->otherLabelArea.size(); ++i)
				if (d_data->otherLabelArea[i]->intersects(text_shape))
					return false;

			const_cast<QPainterPath&>(*d_data->labelArea).addPath(text_shape);
			t.draw(painter, t.textRect());
			return true;
		}
	}
	return false;
}

void VipAbstractScaleDraw::drawTicks(QPainter* painter) const
{
	if (hasComponent(VipAbstractScaleDraw::Ticks)) {
		// painter->save();

		painter->setPen(this->componentPen(VipAbstractScaleDraw::Ticks));

		for (int tickType = VipScaleDiv::MinorTick; tickType < VipScaleDiv::NTickTypes; tickType++) {
			VipScaleDiv::TickType tick = static_cast<VipScaleDiv::TickType>(tickType);
			const VipScaleDiv::TickList& ticks = d_data->scaleDiv.ticks(tick);
			for (int i = 0; i < ticks.count(); i++) {
				const vip_double v = ticks[i];
				if (d_data->scaleDiv.contains(v))
					drawTick(painter, v, d_data->tickLength[tickType], tick);
			}
		}

		// painter->restore();
	}
}

void VipAbstractScaleDraw::drawLabels(QPainter* painter) const
{
	// d_data->lineCount = 0;
	bool no_overlap = true;
	if (hasComponent(VipAbstractScaleDraw::Labels)) {
		if (!d_data->labelOverlap) {
			*d_data->labelArea = QPainterPath();
			d_data->painterTransform = painter->worldTransform(); // .inverted();
		}

		painter->save();
		painter->setPen(this->componentPen(VipAbstractScaleDraw::Labels)); // ignore pen style

		if (!this->hasCustomLabels()) {
			const QMap<vip_double, VipScaleText>& add_text = additionalText();
			if (add_text.size()) {
				for (QMap<vip_double, VipScaleText>::const_iterator it = add_text.begin(); it != add_text.end(); ++it) {
					const VipScaleText text = it.value();
					if (text.tr.isIdentity())
						no_overlap = no_overlap && this->drawLabelOverlap(painter, text.value, text.text, text.tick);
					else {
						painter->save();
						painter->setWorldTransform(text.tr, true);
						no_overlap = no_overlap && this->drawLabelOverlap(painter, text.value, text.text, text.tick);
						painter->restore();
					}
				}
			}

			// draw labels at major tick position
			const VipScaleDiv::TickList& majorTicks = d_data->scaleDiv.ticks(VipScaleDiv::MajorTick);

			const VipScaleDiv::TickList& mediumTicks = d_data->scaleDiv.ticks(VipScaleDiv::MediumTick);

			const VipScaleDiv::TickList& minorTicks = d_data->scaleDiv.ticks(VipScaleDiv::MinorTick);

			// if(!d_data->labelOverlap)
			// d_data->labelArea = QPainterPath();

			QMap<vip_double, VipScaleText>::const_iterator end = add_text.end();

			if (d_data->drawLabel[VipScaleDiv::MajorTick]) {
				for (int i = 0; i < majorTicks.count(); i++) {
					const vip_double v = majorTicks[i];
					QMap<vip_double, VipScaleText>::const_iterator it = add_text.find(v);
					if (d_data->scaleDiv.contains(v) && (it == end || it->tick != VipScaleDiv::MajorTick))
						no_overlap = no_overlap && drawLabelOverlap(painter, v, tickLabel(v, VipScaleDiv::MajorTick).text, VipScaleDiv::MajorTick);
				}
			}
			if (d_data->drawLabel[VipScaleDiv::MediumTick]) {
				for (int i = 0; i < mediumTicks.count(); i++) {
					const vip_double v = mediumTicks[i];
					QMap<vip_double, VipScaleText>::const_iterator it = add_text.find(v);
					if (d_data->scaleDiv.contains(v) && (it == end || it->tick != VipScaleDiv::MediumTick))
						no_overlap = no_overlap && drawLabelOverlap(painter, v, tickLabel(v, VipScaleDiv::MediumTick).text, VipScaleDiv::MediumTick);
				}
			}
			if (d_data->drawLabel[VipScaleDiv::MinorTick]) {
				for (int i = 0; i < minorTicks.count(); i++) {
					const vip_double v = minorTicks[i];
					QMap<vip_double, VipScaleText>::const_iterator it = add_text.find(v);
					if (d_data->scaleDiv.contains(v) && (it == end || it->tick != VipScaleDiv::MinorTick))
						no_overlap = no_overlap && drawLabelOverlap(painter, v, tickLabel(v, VipScaleDiv::MinorTick).text, VipScaleDiv::MinorTick);
				}
			}
		}
		else {
			// draw custom labels
			for (QMap<vip_double, VipScaleText>::const_iterator it = d_data->customLabels.begin(); it != d_data->customLabels.end(); ++it) {
				const vip_double v = it.key();
				if (d_data->scaleDiv.contains(v)) {
					const VipScaleText text = it.value();
					if (text.tr.isIdentity())
						no_overlap = no_overlap && this->drawLabelOverlap(painter, text.value, text.text, text.tick);
					else {
						painter->save();
						painter->setWorldTransform(text.tr, true);
						no_overlap = no_overlap && drawLabelOverlap(painter, text.value, text.text, text.tick);
						painter->restore();
					}
				}
			}
		}

		painter->restore();

		d_data->dirtyOverlap = !no_overlap;
	}
}

/// \brief Draw the scale
///
/// \param painter    The painter
///
/// \param palette    Palette, text color is used for the labels,
///                 foreground color for ticks and backbone
void VipAbstractScaleDraw::draw(QPainter* painter) const
{
	painter->save();

	drawLabels(painter);
	drawTicks(painter);

	if (hasComponent(VipAbstractScaleDraw::Backbone)) {
		// painter->save();
		painter->setPen(this->componentPen(VipAbstractScaleDraw::Backbone));

		drawBackbone(painter);

		// painter->restore();
	}

	painter->restore();
}

/// \brief Set the spacing between tick and labels
///
/// The spacing is the vipDistance between ticks and labels.
/// The default spacing is 4 pixels.
///
/// \param spacing Spacing
///
/// \sa spacing()
void VipAbstractScaleDraw::setSpacing(double spacing)
{
	if (spacing < 0)
		spacing = 0;

	invalidateOverlap();
	d_data->spacing = spacing;
}

/// \brief Get the spacing
///
/// The spacing is the vipDistance between ticks and labels.
/// The default spacing is 4 pixels.
///
/// \return Spacing
/// \sa setSpacing()
double VipAbstractScaleDraw::spacing() const
{
	return d_data->spacing;
}

/// \brief Set a minimum for the extent
///
/// The extent is calculated from the components of the
/// scale draw. In situations, where the labels are
/// changing and the layout depends on the extent (f.e scrolling
/// a scale), setting an upper limit as minimum extent will
/// avoid jumps of the layout.
///
/// \param minExtent Minimum extent
///
/// \sa extent(), minimumExtent()
void VipAbstractScaleDraw::setMinimumExtent(double minExtent)
{
	if (minExtent < 0.0)
		minExtent = 0.0;

	invalidateOverlap();
	d_data->minExtent = minExtent;
}

/// Get the minimum extent
/// \return Minimum extent
/// \sa extent(), setMinimumExtent()
double VipAbstractScaleDraw::minimumExtent() const
{
	return d_data->minExtent;
}

double VipAbstractScaleDraw::fullExtent() const
{
	double dist = 0;

	for (int i = VipScaleDiv::MinorTick; i < VipScaleDiv::NTickTypes; ++i) {
		VipScaleDiv::TickType type = static_cast<VipScaleDiv::TickType>(i);
		if (drawLabelEnabled(type)) {
			if (textPosition() == TextOutside)
				dist = qMax(dist, extent(type));
		}
	}
	return dist;
}

void VipAbstractScaleDraw::setLabelTransform(const QTransform& tr, VipScaleDiv::TickType tick)
{
	invalidateOverlap();
	d_data->transform[tick] = tr;
	d_data->transformInv[tick] = tr.inverted();
}

QTransform VipAbstractScaleDraw::labelTransform(VipScaleDiv::TickType tick) const
{
	return d_data->transform[tick];
}

void VipAbstractScaleDraw::setLabelTransformReference(const QPointF& ref, VipScaleDiv::TickType tick)
{
	invalidateOverlap();
	d_data->transformRef[tick] = ref;
}
QPointF VipAbstractScaleDraw::labelTransformReference(VipScaleDiv::TickType tick) const
{
	return d_data->transformRef[tick];
}

void VipAbstractScaleDraw::setTextPosition(VipAbstractScaleDraw::TextPosition pos)
{
	invalidateOverlap();
	d_data->textPosition = pos;
}
VipAbstractScaleDraw::TextPosition VipAbstractScaleDraw::textPosition() const
{
	return d_data->textPosition;
}

void VipAbstractScaleDraw::setTextTransform(TextTransform tr, VipScaleDiv::TickType tick)
{
	invalidateOverlap();
	d_data->textTransform[tick] = tr;
}
VipAbstractScaleDraw::TextTransform VipAbstractScaleDraw::textTransform(VipScaleDiv::TickType tick) const
{
	return d_data->textTransform[tick];
}

void VipAbstractScaleDraw::setlabelRotation(double rotation, VipScaleDiv::TickType tick)
{
	invalidateOverlap();
	d_data->rotation[tick] = rotation;
}
double VipAbstractScaleDraw::labelRotation(VipScaleDiv::TickType tick) const
{
	return d_data->rotation[tick];
}

double VipAbstractScaleDraw::labelRotation(double, const QSizeF&, VipScaleDiv::TickType tick) const
{
	return d_data->rotation[tick];
}

void VipAbstractScaleDraw::setLabelInterval(const VipInterval& interval)
{
	d_data->labelInterval = interval;
}
VipInterval VipAbstractScaleDraw::labelInterval() const
{
	return d_data->labelInterval;
}

/// Set the length of the ticks
///
/// \param tickType Tick type
/// \param length New length
///
/// \warning the length is limited to [0..1000]
void VipAbstractScaleDraw::setTickLength(VipScaleDiv::TickType tickType, double length)
{
	if (tickType < VipScaleDiv::MinorTick || tickType > VipScaleDiv::MajorTick) {
		return;
	}

	if (length < 0.0)
		length = 0.0;

	const double maxTickLen = 1000.0;
	if (length > maxTickLen)
		length = maxTickLen;

	d_data->tickLength[tickType] = length;
}

/// \return Length of the ticks
/// \sa setTickLength(), maxTickLength()
double VipAbstractScaleDraw::tickLength(VipScaleDiv::TickType tickType) const
{
	if (tickType < VipScaleDiv::MinorTick || tickType > VipScaleDiv::MajorTick) {
		return 0;
	}

	return d_data->tickLength[tickType];
}

/// \return Length of the longest tick
///
/// Useful for layout calculations
/// \sa tickLength(), setTickLength()
double VipAbstractScaleDraw::maxTickLength() const
{
	double length = 0.0;
	for (int i = 0; i < VipScaleDiv::NTickTypes; i++)
		length = qMax(length, d_data->tickLength[i]);

	return length;
}

void VipAbstractScaleDraw::setTextStyle(const VipTextStyle& p, VipScaleDiv::TickType tick)
{
	d_data->styles[tick] = p;
	invalidateOverlap();
	invalidateCache();

	// Set the text style to custom labels
	for (QMap<double, VipScaleText>::iterator it = d_data->customLabels.begin(); it != d_data->customLabels.end(); ++it) {
		if (it.value().tick == tick)
			it.value().text.setTextStyle(p);
	}
	d_data->labelText[tick].setTextStyle(p);
}

const VipTextStyle& VipAbstractScaleDraw::textStyle(VipScaleDiv::TickType tick) const
{
	return d_data->styles[tick];
}

VipTextStyle& VipAbstractScaleDraw::textStyle(VipScaleDiv::TickType tick)
{
	invalidateCache();
	return d_data->styles[tick];
}

void VipAbstractScaleDraw::setAdditionalTextStyle(const VipTextStyle& style)
{
	d_data->additionalStyle.reset(new VipTextStyle(style));
}
const VipTextStyle& VipAbstractScaleDraw::additionalTextStyle() const
{
	return d_data->additionalStyle ? *d_data->additionalStyle : this->textStyle();
}
void VipAbstractScaleDraw::resetAdditionalTextStyle()
{
	d_data->additionalStyle.reset();
}

void VipAbstractScaleDraw::setComponentPen(int component, const QPen& pen)
{
	if (component & Backbone)
		d_data->componentPen[0] = pen;
	if (component & Ticks)
		d_data->componentPen[1] = pen;
	if (component & Labels) {
		d_data->componentPen[2] = pen;
		// also set the pen to the text styles
		textStyle(VipScaleDiv::MinorTick).setTextPen(pen);
		textStyle(VipScaleDiv::MediumTick).setTextPen(pen);
		textStyle(VipScaleDiv::MajorTick).setTextPen(pen);
	}
}

QPen VipAbstractScaleDraw::componentPen(ScaleComponent component) const
{
	if (component == Backbone)
		return d_data->componentPen[0];
	if (component == Ticks)
		return d_data->componentPen[1];
	if (component == Labels)
		return d_data->componentPen[2];

	return QPen();
}

void VipAbstractScaleDraw::setTicksPosition(VipAbstractScaleDraw::TicksPosition position)
{
	d_data->ticksPosition = position;
}

VipAbstractScaleDraw::TicksPosition VipAbstractScaleDraw::ticksPosition() const
{
	return d_data->ticksPosition;
}

void VipAbstractScaleDraw::enableDrawLabel(VipScaleDiv::TickType tick, bool enable)
{
	d_data->drawLabel[tick] = enable;
}

bool VipAbstractScaleDraw::drawLabelEnabled(VipScaleDiv::TickType tick) const
{
	return d_data->drawLabel[tick];
}

void VipAbstractScaleDraw::enableLabelOverlapping(bool enable)
{
	invalidateOverlap();
	d_data->labelOverlap = enable;
}

bool VipAbstractScaleDraw::labelOverlappingEnabled() const
{
	return d_data->labelOverlap;
}
QSharedPointer<QPainterPath> VipAbstractScaleDraw::thisLabelArea() const
{
	return d_data->labelArea;
}
void VipAbstractScaleDraw::addAdditionalLabelOverlapp(const QSharedPointer<QPainterPath>& other)
{
	if (d_data->otherLabelArea.indexOf(other) < 0)
		d_data->otherLabelArea.push_back(other);
}
QVector<QSharedPointer<QPainterPath>> VipAbstractScaleDraw::additionalLabelOverlapp() const
{
	return d_data->otherLabelArea;
}
void VipAbstractScaleDraw::setAdditionalLabelOverlapp(const QVector<QSharedPointer<QPainterPath>>& other)
{
	invalidateOverlap();
	d_data->otherLabelArea = other;
}
void VipAbstractScaleDraw::removeAdditionalLabelOverlapp(const QSharedPointer<QPainterPath>& other)
{
	invalidateOverlap();
	d_data->otherLabelArea.removeOne(other);
}
void VipAbstractScaleDraw::clearAdditionalLabelOverlapp()
{
	invalidateOverlap();
	d_data->otherLabelArea.clear();
}

void VipAbstractScaleDraw::setCustomTextStyle(CustomTextStyle style)
{
	d_data->customTextStyle = style;
	invalidateCache();
}
VipAbstractScaleDraw::CustomTextStyle VipAbstractScaleDraw::customTextStyle() const
{
	return d_data->customTextStyle;
}

void VipAbstractScaleDraw::setCustomLabels(const QList<VipScaleText>& custom_labels)
{
	d_data->customLabels.clear();
	for (int i = 0; i < custom_labels.size(); ++i)
		d_data->customLabels[custom_labels[i].value] = custom_labels[i];
	invalidateCache();
}

QList<VipScaleText> VipAbstractScaleDraw::customLabels() const
{
	return d_data->customLabels.values();
}

void VipAbstractScaleDraw::setCustomLabelText(const VipText& label_text, VipScaleDiv::TickType tick)
{
	d_data->labelText[tick] = label_text;
	invalidateCache();
}

VipText VipAbstractScaleDraw::customLabelText(VipScaleDiv::TickType tick) const
{
	return d_data->labelText[tick];
}

bool VipAbstractScaleDraw::hasCustomLabels() const
{
	return d_data->customLabels.size() != 0;
}
// double VipAbstractScaleDraw::value(const QString & text, double default_value ) const
// {
// QList<double> t = d_data->customLabels.keys(VipText(text));
// if(t.size())
// return t.front();
// else
// return default_value;
// }

void VipAbstractScaleDraw::setValueToText(VipValueToText* v)
{
	d_data->valueToText = QSharedPointer<VipValueToText>(v);
	invalidateOverlap();
	invalidateCache();
}

VipValueToText* VipAbstractScaleDraw::valueToText() const
{
	return const_cast<VipValueToText*>(d_data->valueToText.data());
}

/// \brief Convert a value into its representing label
///
/// The value is converted to a plain text using
/// QLocale().toString(value).
/// This method is often overloaded by applications to have individual
/// labels.
///
/// \param value Value
/// \return Label string.
VipText VipAbstractScaleDraw::label(vip_double value, VipScaleDiv::TickType tick) const
{
	if (d_data->labelInterval.isValid() && !d_data->labelInterval.contains(value))
		return VipText();
	else if (!d_data->labelText[tick].isEmpty()) {
		QString tmp = d_data->labelText[tick].text();
		tmp = VipText::replace(tmp, "#value", value);
		return VipText(tmp, customTextStyle() == UseCustomTextStyle ? d_data->labelText[tick].textStyle() : textStyle(tick));
	}
	else {

		return VipText(d_data->valueToText->convert(value, tick), textStyle(tick));
	}
}

VipScaleDiv::TickList VipAbstractScaleDraw::labelTicks(VipScaleDiv::TickType tick) const
{
	VipScaleDiv::TickList values;

	if (d_data->customLabels.size()) {
		// return the custom labels
		for (QMap<vip_double, VipScaleText>::const_iterator it = d_data->customLabels.begin(); it != d_data->customLabels.end(); ++it)
			if (it->tick == tick)
				values.append(it.key());
	}
	else {
		// return the scale div plus the additional text in ascending order
		values = scaleDiv().ticks(tick);
		if (d_data->labelInterval.isValid()) {
			VipScaleDiv::TickList tmp;
			for (int i = 0; i < values.size(); ++i)
				if (d_data->labelInterval.contains(values[i]))
					tmp.append(values[i]);
			values = tmp;
		}

		const QMap<vip_double, VipScaleText>& add_text = additionalText();
		if (add_text.size()) {
			int i = 1;
			for (QMap<vip_double, VipScaleText>::const_iterator it = add_text.begin(); it != add_text.end(); ++it) {
				if (it->tick == tick) {
					vip_double v = it.key();
					for (; i < values.size(); ++i) {
						if (v >= values[i - 1] || v <= values[i]) {
							values.insert(i, v);
							++i;
							break;
						}
					}
				}
			}
		}
	}

	return values;
}

void VipAbstractScaleDraw::addLabelTransform(QTransform& textTransform, const QSizeF& textSize, VipScaleDiv::TickType tick) const
{
	if (!d_data->transform[tick].isIdentity()) {
		QTransform tr;
		QPointF ref = d_data->transformRef[tick];
		ref.rx() *= textSize.width();
		ref.ry() *= textSize.height();
		QPointF tl = ref;
		tl = textTransform.map(tl);
		tr.translate(-tl.x(), -tl.y());
		tr *= d_data->transform[tick];
		QPointF pt = d_data->transformInv[tick].map(tl);
		tr.translate(pt.x(), pt.y());
		textTransform *= tr;
	}
}

/// \brief Convert a value into its representing label and cache it.
///
/// The conversion between value and label is called very often
/// in the layout and painting code. Unfortunately the
/// calculation of the label sizes might be slow (really slow
/// for rich text in Qt4), so it's necessary to cache the labels.
///
/// \param value Value
///
/// \return Tick label
VipScaleText VipAbstractScaleDraw::tickLabel(vip_double value, VipScaleDiv::TickType tick) const
{
	std::map<vip_double, VipScaleText>::iterator it = d_data->labelCache.find(value);
	if (it == d_data->labelCache.end()) {
		VipScaleText lbl;
		// first, look inside the additional text
		const QMap<vip_double, VipScaleText>& add_text = additionalText();
		QMap<vip_double, VipScaleText>::const_iterator it_add = add_text.find(value);
		if (it_add != add_text.end()) {
			lbl = it_add.value();
		}
		// if no custom label, returns the standrad approach
		else if (!hasCustomLabels()) {
			lbl.text = label(value, tick);
		}
		// use custom label
		else {
			QMap<vip_double, VipScaleText>::const_iterator found = d_data->customLabels.find(value);
			if (found != d_data->customLabels.end() && found->tick == tick) {
				lbl = found.value();
				if (customTextStyle() != UseCustomTextStyle)
					lbl.text.setTextStyle(textStyle(tick));
			}
		}

		lbl.text.setLayoutAttribute(VipText::MinimumLayout);

		(void)lbl.text.textSize(); // initialize the internal cache

		// remove from cache entries that are outside vipBounds
		VipInterval interval = this->scaleDiv().bounds().normalized();

		std::map<vip_double, VipScaleText>::iterator it_remove = d_data->labelCache.lower_bound(interval.minValue());
		d_data->labelCache.erase(d_data->labelCache.begin(), it_remove);

		it_remove = d_data->labelCache.upper_bound(interval.maxValue());
		d_data->labelCache.erase(it_remove, d_data->labelCache.end());

		it = d_data->labelCache.insert(std::map<vip_double, VipScaleText>::value_type(value, lbl)).first;
	}

	return (it->second);
}

/// Invalidate the cache used by tickLabel()
///
/// The cache is invalidated, when a new VipScaleDiv is set. If
/// the labels need to be changed. while the same VipScaleDiv is set,
/// invalidateCache() needs to be called manually.
void VipAbstractScaleDraw::invalidateCache()
{
	d_data->labelCache.clear();
	d_data->additionalText.clear();
	invalidateOverlap();
}

void VipAbstractScaleDraw::invalidateOverlap()
{
	d_data->dirtyOverlap = true;
}

class VipScaleDraw::PrivateData
{
public:
	PrivateData()
	  : len(0)
	  , alignment(VipScaleDraw::BottomScale)
	  , orientation(Qt::Horizontal)
	  , ignoreTextTransform(false)
	{
	}

	QPointF pos;
	double len;

	Alignment alignment;
	Qt::Orientation orientation;
	bool ignoreTextTransform;
};

/// \brief Constructor
///
/// The range of the scale is initialized to [0, 100],
/// The position is at (0, 0) with a length of 100.
/// The orientation is VipAbstractScaleDraw::Bottom.
VipScaleDraw::VipScaleDraw()
{
	d_data = new VipScaleDraw::PrivateData;
	setLength(100);
}

//! Destructor
VipScaleDraw::~VipScaleDraw()
{
	delete d_data;
}

/// Return alignment of the scale
/// \sa setAlignment()
/// \return Alignment of the scale
VipScaleDraw::Alignment VipScaleDraw::alignment() const
{
	return d_data->alignment;
}

/// Set the alignment of the scale
///
/// \param align Alignment of the scale
///
/// The default alignment is VipScaleDraw::BottomScale
/// \sa alignment()
void VipScaleDraw::setAlignment(Alignment align)
{
	d_data->alignment = align;
	d_data->orientation = computeOrientation();
}

/// Return the orientation
///
/// TopScale, BottomScale are horizontal (Qt::Horizontal) scales,
/// LeftScale, RightScale are vertical (Qt::Vertical) scales.
///
/// \return Orientation of the scale
///
/// \sa alignment()
Qt::Orientation VipScaleDraw::orientation() const
{
	return d_data->orientation;
}

void VipScaleDraw::setIgnoreLabelTransform(bool ignore)
{
	d_data->ignoreTextTransform = ignore;
}

bool VipScaleDraw::ignoreLabelTransform() const
{
	return d_data->ignoreTextTransform;
}

Qt::Orientation VipScaleDraw::computeOrientation() const
{
	switch (d_data->alignment) {
		case TopScale:
		case BottomScale:
			return Qt::Horizontal;
		case LeftScale:
		case RightScale:
		default:
			return Qt::Vertical;
	}
}

/// \brief Determine the minimum border vipDistance
///
/// This member function returns the minimum space
/// needed to draw the mark labels at the scale's endpoints.
///
/// \param font Font
/// \param start Start border vipDistance
/// \param end End border vipDistance
void VipScaleDraw::getBorderDistHint(double& start, double& end, VipScaleDiv::TickType tick) const
{
	start = 0;
	end = 0;

	if (!hasComponent(VipAbstractScaleDraw::Labels))
		return;

	const VipScaleDiv::TickList& ticks = scaleDiv().ticks(tick);
	if (ticks.count() == 0)
		return;

	// const QFont & font =
	textStyle(tick).font();

	// Find the ticks, that are mapped to the borders.
	// minTick is the tick, that is mapped to the top/left-most position
	// in widget coordinates.

	vip_double minTick = ticks[0];
	double minPos = scaleMap().transform(minTick);
	vip_double maxTick = minTick;
	double maxPos = minPos;

	for (int i = 1; i < ticks.count(); i++) {
		const double tickPos = scaleMap().transform(ticks[i]);
		if (tickPos < minPos) {
			minTick = ticks[i];
			minPos = tickPos;
		}
		if (tickPos > scaleMap().transform(maxTick)) {
			maxTick = ticks[i];
			maxPos = tickPos;
		}
	}

	double e = 0.0;
	double s = 0.0;
	if (orientation() == Qt::Vertical) {
		s = -labelRect(minTick, tick).top();
		s -= qAbs(minPos - (scaleMap().p2()));

		e = labelRect(maxTick, tick).bottom();
		e -= qAbs(maxPos - scaleMap().p1());
	}
	else {
		s = -labelRect(minTick, tick).left();
		s -= qAbs(minPos - scaleMap().p1());

		e = labelRect(maxTick, tick).right();
		e -= qAbs(maxPos - scaleMap().p2());
	}

	if (s < 0.0)
		s = 0.0;
	if (e < 0.0)
		e = 0.0;

	start = s; // std::ceil( s );
	end = e;   // std::ceil( e );
}

void VipScaleDraw::getBorderDistHint(double& start, double& end) const
{
	start = 0;
	end = 0;

	// easiest solution, draw the labels, extract the shape, and compute the vipDistance hints from it
	// VipShapeDevice shape;
	// QPainter painter(&shape);
	// drawLabels(&painter);
	// QRectF rect = shape.shape().boundingRect();
	// if(!rect.isEmpty())
	// {
	// if(orientation() == Qt::Horizontal)
	// {
	// start = (scaleMap().p1()-rect.left());
	// end = (rect.right() -scaleMap().p2());
	// }
	// else
	// {
	// start = (rect.bottom()-scaleMap().p1());
	// end = (scaleMap().p2()-rect.top());
	// }
	// }
	//
	// if(start < 0)
	// start=0;
	// if(end < 0)
	// end=0;

	for (int i = 0; i < VipScaleDiv::NTickTypes; ++i) {
		VipScaleDiv::TickType type = static_cast<VipScaleDiv::TickType>(i);
		if (drawLabelEnabled(type)) {
			double s = 0, e = 0;
			getBorderDistHint(s, e, type);
			start = qMax(start, s);
			end = qMax(end, e);
		}
	}

	// compute the dist hint for additional texts
	const QMap<vip_double, VipScaleText>& add_text = additionalText();
	VipInterval inter = scaleDiv().bounds();
	for (QMap<vip_double, VipScaleText>::const_iterator it = add_text.begin(); it != add_text.end(); ++it) {
		double s, e;
		QRectF rect = labelRect(it.key(), VipScaleDiv::MajorTick).translated(pos().x(), pos().y());
		if (orientation() == Qt::Horizontal) {
			s = (scaleMap().transform(inter.minValue()) - rect.left());
			e = (rect.right() - scaleMap().transform(inter.maxValue()));
		}
		else {
			s = (rect.bottom() - scaleMap().transform(inter.minValue()));
			e = (scaleMap().transform(inter.maxValue()) - rect.top());
		}

		if (s < 0.0)
			s = 0.0;
		if (e < 0.0)
			e = 0.0;

		if (s > start)
			start = s;
		if (e > end)
			end = e;
	}

	if (this->orientation() == Qt::Vertical) {
		// if (start) start += 10;
		if (end)
			end += 1;
	}
}

/// Determine the minimum distance between two labels, that is necessary
/// that the texts don't overlap.
///
/// \param font Font
/// \return The maximum width of a label
///
/// \sa getBorderDistHint()

double VipScaleDraw::minLabelDist(VipScaleDiv::TickType tick) const
{
	if (!hasComponent(VipAbstractScaleDraw::Labels))
		return 0;

	const VipScaleDiv::TickList& ticks = scaleDiv().ticks(tick);
	if (ticks.isEmpty())
		return 0;

	const QFont& font = textStyle(tick).font();
	const QFontMetrics fm(font);

	const bool vertical = (orientation() == Qt::Vertical);

	QRectF bRect1;
	QRectF bRect2 = labelRect(ticks[0], tick);
	if (vertical) {
		bRect2.setRect(-bRect2.bottom(), 0.0, bRect2.height(), bRect2.width());
	}

	double maxDist = 0.0;

	for (int i = 1; i < ticks.count(); i++) {
		bRect1 = bRect2;
		bRect2 = labelRect(ticks[i], tick);
		if (vertical) {
			bRect2.setRect(-bRect2.bottom(), 0.0, bRect2.height(), bRect2.width());
		}

		double dist = fm.leading(); // space between the labels
		if (bRect1.right() > 0)
			dist += bRect1.right();
		if (bRect2.left() < 0)
			dist += -bRect2.left();

		if (dist > maxDist)
			maxDist = dist;
	}

	double angle = 0; //( labelRotation() ) * Vip::ToRadian;
	if (vertical)
		angle += M_PI / 2;

	const double sinA = std::sin(angle); // qreal -> double
	if (qFuzzyCompare(sinA + 1.0, 1.0))
		return (maxDist);

	const int fmHeight = fm.ascent() - 2;

	// The vipDistance we need until there is
	// the height of the label font. This height is needed
	// for the neighbored label.

	double labelDist = fmHeight / std::sin(angle) * std::cos(angle);
	if (labelDist < 0)
		labelDist = -labelDist;

	// For text orientations close to the scale orientation

	if (labelDist > maxDist)
		labelDist = maxDist;

	// For text orientations close to the opposite of the
	// scale orientation

	if (labelDist < fmHeight)
		labelDist = fmHeight;

	return // std::ceil
	  (labelDist);
}

double VipScaleDraw::minLabelDist() const
{
	double dist = 0;

	for (int i = 0; i < VipScaleDiv::NTickTypes; ++i) {
		VipScaleDiv::TickType type = static_cast<VipScaleDiv::TickType>(i);
		if (drawLabelEnabled(type)) {
			dist = qMax(dist, minLabelDist(type));
		}
	}
	return dist;
}

/// Calculate the width/height that is needed for a
/// vertical/horizontal scale.
///
/// The extent is calculated from the pen width of the backbone,
/// the major tick length, the spacing and the maximum width/height
/// of the labels.
///
/// \param font Font used for painting the labels
/// \return Extent
///
/// \sa minLength()
double VipScaleDraw::extent(VipScaleDiv::TickType tick) const
{
	double d = 0;

	if (hasComponent(VipAbstractScaleDraw::Labels)) {
		if (orientation() == Qt::Vertical)
			d = maxLabelWidth(tick);
		else
			d = maxLabelHeight(tick);

		if (d > 0)
			d += spacing();
	}

	if (hasComponent(VipAbstractScaleDraw::Ticks)) {
		// new in 2.2.18: add test tick position
		if (ticksPosition() == TicksOutside)
			d += tickLength(tick);
	}

	if (hasComponent(VipAbstractScaleDraw::Backbone)) {
		const double pw = qMax(1.,							     // penWidth()
				       this->componentPen(VipAbstractScaleDraw::Backbone).widthF()); // pen width can be zero
		d += pw;
	}

	d = qMax(d, minimumExtent());
	return d;
}

QPointF VipScaleDraw::position(vip_double value, double length, Vip::ValueType type) const
{
	if (type == Vip::Absolute) {
		const double tval = scaleMap().transform(value);
		switch (alignment()) {
			case RightScale: {
				return QPointF(d_data->pos.x() - length, tval);
			}
			case LeftScale: {
				return QPointF(d_data->pos.x() + length, tval);
			}

			case BottomScale: {
				return QPointF(tval, d_data->pos.y() - length);
			}
			case TopScale: {
				return QPointF(tval, d_data->pos.y() + length);
			}
		}
	}
	else {
		switch (alignment()) {
			case RightScale: {
				return pos() + (end() - pos()) * (double)value + QPointF(-length, 0);
			}
			case LeftScale: {
				return pos() + (end() - pos()) * (double)value + QPointF(length, 0);
			}

			case BottomScale: {
				return pos() + (end() - pos()) * (double)value + QPointF(0, -length);
			}
			case TopScale: {
				return pos() + (end() - pos()) * (double)value + QPointF(0, length);
			}
		}
	}

	return QPointF();
}

vip_double VipScaleDraw::value(const QPointF& position) const
{
	switch (alignment()) {
		case RightScale:
		case LeftScale: {
			return scaleMap().invTransform(position.y());
		}

		case BottomScale:
		case TopScale: {
			return scaleMap().invTransform(position.x());
		}
	}
	return 0;
}

vip_double VipScaleDraw::convert(vip_double value, Vip::ValueType type) const
{
	if (type == Vip::Absolute) {
		const vip_double tval = scaleMap().transform(value);
		switch (alignment()) {
			case RightScale:
			case LeftScale: {
				return (tval - pos().y()) / (end().y() - pos().y());
			}
			case BottomScale:
			case TopScale: {
				return (tval - pos().x()) / (end().x() - pos().x());
			}
		}
	}
	else {
		switch (alignment()) {
			case RightScale:
			case LeftScale: {
				return this->value(QPointF(pos().x(), pos().y() + (end().y() - pos().y()) * value));
			}
			case BottomScale:
			case TopScale: {
				return this->value(QPointF(pos().x() + (end().x() - pos().x()) * value, pos().y()));
			}
		}
	}

	return 0;
}

vip_double VipScaleDraw::angle(vip_double value, Vip::ValueType) const
{
	Q_UNUSED(value)
	switch (alignment()) {
		case RightScale:
			return -90;
		case LeftScale:
			return 90;
		case BottomScale:
			return 180;
		case TopScale:
			return 0;
	}

	return 0;
}

/// Calculate the minimum length that is needed to draw the scale
///
/// \param font Font used for painting the labels
/// \return Minimum length that is needed to draw the scale
///
/// \sa extent()
double VipScaleDraw::minLength() const
{
	double startDist, endDist;
	getBorderDistHint(startDist, endDist);

	const VipScaleDiv& sd = scaleDiv();

	const uint minorCount = sd.ticks(VipScaleDiv::MinorTick).count() + sd.ticks(VipScaleDiv::MediumTick).count();
	const uint majorCount = sd.ticks(VipScaleDiv::MajorTick).count();

	int lengthForLabels = 0;
	if (hasComponent(VipAbstractScaleDraw::Labels))
		lengthForLabels = minLabelDist() * majorCount;

	int lengthForTicks = 0;
	if (hasComponent(VipAbstractScaleDraw::Ticks)) {
		const double pw = qMax(1.,							     // penWidth()
				       this->componentPen(VipAbstractScaleDraw::Backbone).widthF()); // penwidth can be zero
		lengthForTicks =								     // std::ceil
		  ((majorCount + minorCount) * (pw + 1.0));
	}

	return startDist + endDist + qMax(lengthForLabels, lengthForTicks);
}

/// Find the position, where to paint a label
///
/// The position has a vipDistance that depends on the length of the ticks
/// in direction of the alignment().
///
/// \param value Value
/// \return Position, where to paint a label
QPointF VipScaleDraw::labelPosition(vip_double value, VipScaleDiv::TickType tick) const
{
	const double tval = scaleMap().transform(value);
	double dist = spacing();
	double backbone_w = hasComponent(VipAbstractScaleDraw::Backbone) ? qMax(1., // penWidth()
										this->componentPen(VipAbstractScaleDraw::Backbone).widthF())
									 : 0;
	dist += backbone_w;

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipScaleDraw*>(this)->setTextPosition(TextOutside);

	if (hasComponent(VipAbstractScaleDraw::Ticks)) {

		if ((ticksPosition() == VipAbstractScaleDraw::TicksOutside && textPosition() == TextOutside) ||
		    (ticksPosition() == VipAbstractScaleDraw::TicksInside && textPosition() == TextInside)) {

			if (tick == VipScaleDiv::MajorTick)
				dist += tickLength(VipScaleDiv::MajorTick);
			else if (tick == VipScaleDiv::MediumTick)
				dist += tickLength(VipScaleDiv::MediumTick);
			else if (tick == VipScaleDiv::MinorTick)
				dist += tickLength(VipScaleDiv::MinorTick);
		}
	}

	double px = 0;
	double py = 0;

	switch (alignment()) {
		case RightScale: {
			if (textPosition() == TextOutside)
				px = d_data->pos.x() + dist;
			else
				px = d_data->pos.x() - dist;
			py = tval;
			break;
		}
		case LeftScale: {
			if (textPosition() == TextOutside)
				px = d_data->pos.x() - dist;
			else
				px = d_data->pos.x() + dist;
			py = tval;
			break;
		}
		case BottomScale: {
			px = tval;
			if (textPosition() == TextOutside)
				py = d_data->pos.y() + dist;
			else
				py = d_data->pos.y() - dist;
			break;
		}
		case TopScale: {
			px = tval;
			if (textPosition() == TextOutside)
				py = d_data->pos.y() - dist;
			else
				py = d_data->pos.y() + dist;
			break;
		}
	}

	return QPointF(px, py);
}

void VipScaleDraw::drawTicks(QPainter* painter) const
{
	bool remove_antialiazing = !painter->transform().isRotating();
	QPainter::RenderHints saved = painter->renderHints();
	if (remove_antialiazing) {
		painter->setRenderHint(QPainter::Antialiasing, false);
	}

	VipAbstractScaleDraw::drawTicks(painter);

	if (remove_antialiazing) {
		painter->setRenderHints(saved);
	}
}

/// Draw a tick
///
/// \param painter VipPainter
/// \param value Value of the tick
/// \param len Length of the tick
///
/// \sa drawBackbone(), drawLabel()
void VipScaleDraw::drawTick(QPainter* painter, vip_double value, double len, VipScaleDiv::TickType tick) const
{
	Q_UNUSED(tick)
	if (len <= 0)
		return;

	QPointF pos = d_data->pos;
	double tval = scaleMap().transform(value);
	const double pw = componentPen(Backbone).widthF() / 2;

	switch (alignment()) {
		case LeftScale: {
			double x1, x2;

			if (ticksPosition() == VipAbstractScaleDraw::TicksOutside) {
				x1 = pos.x() - pw;
				x2 = x1 - len;
			}
			else {
				x1 = pos.x() + pw;
				x2 = x1 + len;
			}

			VipPainter::drawLine(painter, x1, tval, x2, tval);
			break;
		}

		case RightScale: {
			double x1, x2;

			if (ticksPosition() == VipAbstractScaleDraw::TicksOutside) {
				x1 = pos.x() + pw;
				x2 = x1 + len;
			}
			else {
				x1 = pos.x() - pw;
				x2 = x1 - len;
			}

			VipPainter::drawLine(painter, x1, tval, x2, tval);
			break;
		}

		case BottomScale: {
			double y1, y2;

			if (ticksPosition() == VipAbstractScaleDraw::TicksOutside) {
				y1 = pos.y() + pw;
				y2 = y1 + len;
			}
			else {
				y1 = pos.y() - pw;
				y2 = y1 - len;
			}

			VipPainter::drawLine(painter, tval, y1, tval, y2);
			break;
		}

		case TopScale: {
			double y1, y2;

			if (ticksPosition() == VipAbstractScaleDraw::TicksOutside) {
				y1 = pos.y() - pw;
				y2 = y1 - len;
			}
			else {
				y1 = pos.y() + pw;
				y2 = y1 + len;
			}

			VipPainter::drawLine(painter, tval, y1, tval, y2);
			break;
		}
	}
}

/// Draws the baseline of the scale
/// \param painter VipPainter
///
/// \sa drawTick(), drawLabel()
void VipScaleDraw::drawBackbone(QPainter* painter) const
{
	const QPointF& pos = d_data->pos;
	const double penWidth = componentPen(Backbone).widthF();
	const double len = d_data->len;

	// pos indicates a border not the center of the backbone line
	// so we need to shift its position depending on the pen width
	// and the alignment of the scale

	double off = 0.5 * penWidth;

	painter->setPen(componentPen(Backbone));

	bool remove_antialiazing = !painter->transform().isRotating();
	QPainter::RenderHints saved = painter->renderHints();
	if (remove_antialiazing) {
		painter->setRenderHint(QPainter::Antialiasing, false);
	}

	switch (alignment()) {
		case LeftScale: {
			double x = pos.x() - off + 0.5;

			VipPainter::drawLine(painter, x, pos.y(), x, pos.y() + len);
			break;
		}
		case RightScale: {
			double x = pos.x() + off - 0.5;

			VipPainter::drawLine(painter, x, pos.y(), x, pos.y() + len);
			break;
		}
		case TopScale: {
			double y = pos.y() - off + 0.5;

			VipPainter::drawLine(painter, pos.x(), y, pos.x() + len, y);
			break;
		}
		case BottomScale: {
			double y = pos.y() + off - 0.5;

			VipPainter::drawLine(painter, pos.x(), y, pos.x() + len, y);
			break;
		}
	}

	if (remove_antialiazing) {
		painter->setRenderHints(saved);
	}
}

/// \brief Move the position of the scale
///
/// The meaning of the parameter pos depends on the alignment:
/// <dl>
/// <dt>VipScaleDraw::LeftScale
/// <dd>The origin is the topmost point of the
///   backbone. The backbone is a vertical line.
///   Scale marks and labels are drawn
///   at the left of the backbone.
/// <dt>VipScaleDraw::RightScale
/// <dd>The origin is the topmost point of the
///   backbone. The backbone is a vertical line.
///   Scale marks and labels are drawn
///   at the right of the backbone.
/// <dt>VipScaleDraw::TopScale
/// <dd>The origin is the leftmost point of the
///   backbone. The backbone is a horizontal line.
///   Scale marks and labels are drawn
///   above the backbone.
/// <dt>VipScaleDraw::BottomScale
/// <dd>The origin is the leftmost point of the
///   backbone. The backbone is a horizontal line
///   Scale marks and labels are drawn
///   below the backbone.
/// </dl>
///
/// \param pos Origin of the scale
///
/// \sa pos(), setLength()
void VipScaleDraw::move(const QPointF& pos)
{
	d_data->pos = pos;
	updateMap();
}

/// \return Origin of the scale
/// \sa move(), length()
QPointF VipScaleDraw::pos() const
{
	return d_data->pos;
}

QPointF VipScaleDraw::start() const
{
	return d_data->pos;
}

QPointF VipScaleDraw::end() const
{
	if (this->orientation() == Qt::Horizontal)
		return d_data->pos + QPointF(length(), 0.);
	else
		return d_data->pos + QPointF(0., length());
}

/// Set the length of the backbone.
///
/// The length doesn't include the space needed for
/// overlapping labels.
///
/// \param length Length of the backbone
///
/// \sa move(), minLabelDist()
void VipScaleDraw::setLength(double length)
{
#if 1
	if (length >= 0 && length < 10)
		length = 10;

	// why should we accept negative lengths ???
	if (length < 0 && length > -10)
		length = -10;
#else
	length = qMax(length, 10);
#endif

	d_data->len = length;
	updateMap();
}

/// \return the length of the backbone
/// \sa setLength(), pos()
double VipScaleDraw::length() const
{
	return d_data->len;
}

/// Draws the label for a major scale tick
///
/// \param painter VipPainter
/// \param value Value
///
/// \sa drawTick(), drawBackbone(), boundingLabelRect()
void VipScaleDraw::drawLabel(QPainter* painter, vip_double value, const VipText& lbl, VipScaleDiv::TickType tick) const
{
	if (lbl.isEmpty())
		return;

	QPointF pos = labelPosition(value, tick);

	QSizeF labelSize = lbl.textSize();

	const QTransform transform = labelTransformation(value, pos, labelSize, tick, lbl.alignment());

	const QTransform tr = painter->worldTransform();

	QRect text_rect(QPoint(0, 0), labelSize.toSize());
	painter->setWorldTransform(transform, true);

	if (d_data->ignoreTextTransform) {
		QTransform _tr = painter->worldTransform();
		painter->resetTransform();
		text_rect = _tr.mapRect(text_rect);
	}

	lbl.draw(painter, text_rect);

	painter->setWorldTransform(tr, false);
}

/// \brief Find the bounding rectangle for the label.
///
/// The coordinates of the rectangle are absolute ( calculated from pos() ).
/// in direction of the tick.
///
/// \param font Font used for painting
/// \param value Value
///
/// \return Bounding rectangle
/// \sa labelRect()
QRectF VipScaleDraw::boundingLabelRect(vip_double value, VipScaleDiv::TickType tick) const
{
	VipScaleText lbl = tickLabel(value, tick);
	if (lbl.text.text().isEmpty())
		return QRectF();

	const QPointF pos = labelPosition(value, tick);
	QSizeF labelSize = lbl.text.textSize();

	const QTransform transform = labelTransformation(value, pos, labelSize, tick, textStyle(tick).alignment()) * lbl.tr;
	return transform.mapRect(QRectF(QPoint(0, 0), labelSize.toSize()));
}

QTransform VipScaleDraw::labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const
{
	QPointF pos = labelPosition(value, tick);
	QSizeF labelSize = text.textSize();
	return labelTransformation(value, pos, labelSize, tick, text.alignment());
}

/// Calculate the transformation that is needed to paint a label
/// depending on its alignment and rotation.
///
/// \param pos Position where to paint the label
/// \param size Size of the label
///
/// \return Transformation matrix
/// \sa setLabelAlignment(), setLabelRotation()
QTransform VipScaleDraw::labelTransformation(vip_double value, const QPointF& pos, const QSizeF& size, VipScaleDiv::TickType tick, Qt::Alignment text_alignment) const
{
	QTransform transform;
	transform.translate(pos.x(), pos.y());

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipScaleDraw*>(this)->setTextPosition(TextOutside);

	int flags = text_alignment;

	double x = 0, y = 0;
	double rotate = 0;
	switch (alignment()) {
		case RightScale: {
			if (textTransform(tick) == TextHorizontal || textTransform(tick) == TextPerpendicular) {

				if (textPosition() == TextOutside)
					x = 0.0;
				else
					x = -size.width();

				if (flags & Qt::AlignVCenter)
					y = -0.5 * size.height();
				else if (flags & Qt::AlignTop)
					y = -size.height();
				else
					y = 0;
			}
			else {
				if (textPosition() == TextOutside)
					x = size.height();
				else
					x = 0;
				//-size.height();
				if (flags & Qt::AlignVCenter)
					y = -0.5 * size.width();
				else if (flags & Qt::AlignTop)
					y = -size.width();
				else
					y = 0;
				rotate = 90;
			}
			break;
		}
		case LeftScale: {
			if (textTransform(tick) == TextHorizontal || textTransform(tick) == TextPerpendicular) {
				if (textPosition() == TextOutside)
					x = -size.width();
				else
					x = 0;
				if (flags & Qt::AlignVCenter)
					y = -0.5 * size.height();
				else if (flags & Qt::AlignTop)
					y = -size.height();
				else
					y = 0;
			}
			else {
				if (textPosition() == TextOutside)
					x = -size.height();
				else
					x = 0;
				if (flags & Qt::AlignVCenter)
					y = 0.5 * size.width();
				else if (flags & Qt::AlignTop)
					y = 0;
				else
					y = size.width();
				rotate = -90;
			}
			break;
		}
		case BottomScale: {
			if (textTransform(tick) == TextHorizontal || textTransform(tick) == TextParallel || textTransform(tick) == TextCurved) {
				if (textPosition() == TextOutside)
					y = 0;
				else
					y = -size.height();
				if (flags & Qt::AlignHCenter)
					x = -0.5 * size.width();
				else if (flags & Qt::AlignLeft)
					x = -size.width();
				else
					x = 0;
			}
			else {
				if (textPosition() == TextOutside)
					y = 0;
				else
					y = -size.width();
				if (flags & Qt::AlignHCenter)
					x = 0.5 * size.height();
				else if (flags & Qt::AlignLeft)
					x = 0;
				else
					x = size.height();
				rotate = 90;
			}
			break;
		}
		case TopScale: {
			if (textTransform(tick) == TextHorizontal || textTransform(tick) == TextParallel || textTransform(tick) == TextCurved) {
				if (textPosition() == TextOutside)
					y = -size.height();
				else
					y = 0;
				if (flags & Qt::AlignHCenter)
					x = -0.5 * size.width();
				else if (flags & Qt::AlignLeft)
					x = -size.width();
				else
					x = 0;
			}
			else {
				if (textPosition() == TextOutside)
					y = 0;
				else
					y = size.width();
				if (flags & Qt::AlignHCenter)
					x = -0.5 * size.height();
				else if (flags & Qt::AlignLeft)
					x = -size.height();
				else
					x = 0;
				rotate = -90;
			}
			break;
		}
	}

	transform.translate(x, y);
	if (rotate)
		transform.rotate(rotate);
	addLabelTransform(transform, size, tick);
	if (double rot = this->labelRotation(value, size, tick))
		transform.rotate(rot);

	return transform;
}

/// Find the bounding rectangle for the label. The coordinates of
/// the rectangle are relative to spacing + tick length from the backbone
/// in direction of the tick.
///
/// \param font Font used for painting
/// \param value Value
///
/// \return Bounding rectangle that is needed to draw a label
QRectF VipScaleDraw::labelRect(vip_double value, VipScaleDiv::TickType tick) const
{
	VipScaleText lbl = tickLabel(value, tick);
	if (lbl.text.text().isEmpty())
		return QRectF(0.0, 0.0, 0.0, 0.0);

	const QPointF pos = labelPosition(value, tick);
	const QSizeF labelSize = lbl.text.textSize();
	const QTransform transform = labelTransformation(value, pos, labelSize, tick, textStyle(tick).alignment()) * lbl.tr;

	QRectF br = transform.mapRect(QRectF(QPointF(0, 0), labelSize));
	br.translate(-pos.x(), -pos.y());

	return br;
}

/// Calculate the size that is needed to draw a label
///
/// \param font Label font
/// \param value Value
///
/// \return Size that is needed to draw a label
QSizeF VipScaleDraw::labelSize(vip_double value, VipScaleDiv::TickType tick) const
{
	return labelRect(value, tick).size();
}

/// \param font Font
/// \return the maximum width of a label
double VipScaleDraw::maxLabelWidth(VipScaleDiv::TickType tick) const
{
	double maxWidth = 0.0;

	const VipScaleDiv::TickList ticks = labelTicks(tick);
	for (int i = 0; i < ticks.count(); i++) {
		const vip_double v = ticks[i];
		if (scaleDiv().contains(v)) {
			const double w = labelSize(v, tick).width();
			if (w > maxWidth)
				maxWidth = w;
		}
	}

	return // std::ceil
	  (maxWidth);
}

/// \param font Font
/// \return the maximum height of a label
double VipScaleDraw::maxLabelHeight(VipScaleDiv::TickType tick) const
{
	double maxHeight = 0.0;

	const VipScaleDiv::TickList ticks = labelTicks(tick);
	for (int i = 0; i < ticks.count(); i++) {
		const vip_double v = ticks[i];
		if (scaleDiv().contains(v)) {
			const double h = labelSize(v, tick).height();
			if (h > maxHeight)
				maxHeight = h;
		}
	}

	return // std::ceil
	  (maxHeight);
}

void VipScaleDraw::updateMap()
{
	invalidateOverlap();
	const QPointF pos = d_data->pos;
	double len = d_data->len;

	VipScaleMap& sm = scaleMap();
	if (orientation() == Qt::Vertical)
		sm.setPaintInterval(pos.y() + len, pos.y());
	else
		sm.setPaintInterval(pos.x(), pos.x() + len);
}

class VipPolarScaleDraw::PrivateData
{
public:
	PrivateData()
	  : center(0, 0)
	  , radius(1)
	  , startAngle(0)
	  , endAngle(360)
	{
		drawLast[0] = drawLast[1] = drawLast[2] = true;
	}

	QPointF center;
	double radius;
	double startAngle;
	double endAngle;

	bool drawLast[3];
};

/// \brief Constructor
///
/// The range of the scale is initialized to [0, 100],
/// The position is at (0, 0) with a length of 100.
/// The orientation is VipAbstractScaleDraw::Bottom.
VipPolarScaleDraw::VipPolarScaleDraw()
{
	d_data = new VipPolarScaleDraw::PrivateData;
	this->setTicksPosition(TicksOutside);
}

//! Destructor
VipPolarScaleDraw::~VipPolarScaleDraw()
{
	delete d_data;
}

void VipPolarScaleDraw::setScaleDiv(const VipScaleDiv& s)
{
	VipAbstractScaleDraw::setScaleDiv(s);
	checkDrawLast();
}

void VipPolarScaleDraw::checkDrawLast(VipScaleDiv::TickType type)
{
	// if start and end angle are the same, no need to go further
	if (d_data->endAngle - 360 == d_data->startAngle) {
		d_data->drawLast[type] = false;
	}
	else {
		const VipScaleDiv::TickList ticks = labelTicks(type);
		if (ticks.size() > 2) {
			QPolygonF p1 = labelPolygon(ticks.first(), type);
			QPolygonF p2 = labelPolygon(ticks.last(), type);
			QPolygonF res = p1.intersected(p2);
			d_data->drawLast[type] = (res.size() == 0);
		}
	}
}

void VipPolarScaleDraw::checkDrawLast()
{
	checkDrawLast(VipScaleDiv::MinorTick);
	checkDrawLast(VipScaleDiv::MediumTick);
	checkDrawLast(VipScaleDiv::MajorTick);
}

void VipPolarScaleDraw::setCenter(const QPointF& center)
{
	d_data->center = center;
	updateMap();
}

void VipPolarScaleDraw::setRadius(double radius)
{
	if (radius > 0) {
		d_data->radius = radius;
		checkDrawLast();
		updateMap();
	}
}
void VipPolarScaleDraw::setStartAngle(double start)
{
	while (start > 360)
		start -= 360;

	while (start < -360)
		start += 360;

	d_data->startAngle = start;

	checkDrawLast();
	updateMap();
}

void VipPolarScaleDraw::setEndAngle(double end)
{
	while (end > 360)
		end -= 360;

	while (end < -360)
		end += 360;

	d_data->endAngle = end;

	checkDrawLast();
	updateMap();
}

QPointF VipPolarScaleDraw::center() const
{
	return d_data->center;
}

double VipPolarScaleDraw::radius() const
{
	return d_data->radius;
}

double VipPolarScaleDraw::startAngle() const
{
	return d_data->startAngle;
}

double VipPolarScaleDraw::endAngle() const
{
	return d_data->endAngle;
}

double VipPolarScaleDraw::sweepLength() const
{
	return d_data->endAngle - d_data->startAngle;
}

void VipPolarScaleDraw::getDLengthHint(double& d_s_angle, double& d_e_angle, VipScaleDiv::TickType tick) const
{
	d_s_angle = d_e_angle = 0;

	double sAngle = startAngle();
	if (sAngle < 0)
		sAngle += 360;

	double eAngle = endAngle();
	if (eAngle < 0)
		eAngle += 360;

	if (hasComponent(VipAbstractScaleDraw::Labels)) {
		const VipScaleDiv::TickList ticks = labelTicks(tick);
		if (ticks.size() == 0)
			return;

		QRectF r = labelPolygon(ticks[0], tick).boundingRect();

		if (!r.isEmpty()) {
			const QPointF r_c = r.center();
			const QPointF c = center();
			double s_angle = 0;

			if (c.x() <= r_c.x() && c.y() <= r_c.y()) {
				s_angle = (QLineF(c, r.bottomLeft()).angle() - sAngle);
			}
			else if (c.x() <= r_c.x() && c.y() >= r_c.y()) {
				s_angle = (QLineF(c, r.bottomRight()).angle() - sAngle);
			}
			else if (c.x() >= r_c.x() && c.y() <= r_c.y()) {
				s_angle = (QLineF(c, r.topLeft()).angle() - sAngle);
			}
			else if (c.x() >= r_c.x() && c.y() >= r_c.y()) {
				s_angle = (QLineF(c, r.topRight()).angle() - sAngle);
			}

			if (s_angle > 180)
				s_angle -= 360;
			else if (s_angle < -180)
				s_angle += 360;
			if (s_angle < 0)
				d_s_angle = qAbs(s_angle);
		}

		r = labelPolygon(ticks[ticks.count() - 1], tick).boundingRect();

		if (!r.isEmpty()) {
			const QPointF r_c = r.center();
			const QPointF c = center();
			double e_angle = 0;

			if (c.x() <= r_c.x() && c.y() <= r_c.y()) {
				e_angle = (QLineF(c, r.topRight()).angle() - eAngle);
			}
			else if (c.x() <= r_c.x() && c.y() >= r_c.y()) {
				e_angle = (QLineF(c, r.topLeft()).angle() - eAngle);
			}
			else if (c.x() >= r_c.x() && c.y() <= r_c.y()) {
				e_angle = (QLineF(c, r.bottomRight()).angle() - eAngle);
			}
			else if (c.x() >= r_c.x() && c.y() >= r_c.y()) {
				e_angle = (QLineF(c, r.bottomLeft()).angle() - eAngle);
			}

			if (e_angle > 180)
				e_angle -= 360;
			else if (e_angle < -180)
				e_angle += 360;
			if (e_angle > 0)
				d_e_angle = qAbs(e_angle);
		}
	}
}

void VipPolarScaleDraw::getBorderDistHint(double& d_s_angle, double& d_e_angle) const
{
	d_s_angle = d_e_angle = 0;
	for (int i = 0; i < VipScaleDiv::NTickTypes; ++i) {
		VipScaleDiv::TickType type = static_cast<VipScaleDiv::TickType>(i);
		if (drawLabelEnabled(type)) {
			double s, e;
			getDLengthHint(s, e, type);
			d_s_angle = qMax(d_s_angle, s);
			d_e_angle = qMax(d_e_angle, e);
		}
	}
}

/// Calculate the width/height that is needed for a
/// vertical/horizontal scale.
///
/// The extent is calculated from the pen width of the backbone,
/// the major tick length, the spacing and the maximum width/height
/// of the labels.
///
/// \param font Font used for painting the labels
/// \return Extent
///
/// \sa minLength()
double VipPolarScaleDraw::extent(VipScaleDiv::TickType tick) const
{
	double d = 0;
	double factor = 1;

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipPolarScaleDraw*>(this)->setTextPosition(TextOutside);

	if (textPosition() == TextInside)
		factor = -1;

	if (hasComponent(VipAbstractScaleDraw::Labels)) {
		double w = radius();

		const VipScaleDiv::TickList ticks = labelTicks(tick);
		for (int i = 0; i < ticks.count(); i++) {
			// ignore the last tick if it is not drawn
			if (i == ticks.count() - 1 && !d_data->drawLast[tick])
				continue;

			const vip_double v = ticks[i];
			if (scaleDiv().contains(v)) {
				QPolygonF p = labelPolygon(ticks[i], tick);
				for (int j = 0; j < p.size(); ++j) {
					const double len = QLineF(center(), p[j]).length();
					if (textPosition() == TextOutside) {
						if (len > w)
							w = len;
					}
					else {
						if (len < w)
							w = len;
					}
				}
			}
		}

		d = qAbs(radius() - w);
	}

	if (hasComponent(VipAbstractScaleDraw::Ticks) && d == 0) {
		d += maxTickLength();
	}

	if (hasComponent(VipAbstractScaleDraw::Backbone)) {
		const double pw = qMax(1.,							     // penWidth()
				       this->componentPen(VipAbstractScaleDraw::Backbone).widthF()); // pen width can be zero
		d += pw;
	}

	d = qMax(d, minimumExtent());

	return d * factor;
}

/// Find the position, where to paint a label
///
/// The position has a vipDistance that depends on the length of the ticks
/// in direction of the alignment().
///
/// \param value Value
/// \return Position, where to paint a label
QPointF VipPolarScaleDraw::labelPosition(vip_double value, double& angle, VipScaleDiv::TickType tick) const
{
	const double tval = scaleMap().transform(value);
	double dist;

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipPolarScaleDraw*>(this)->setTextPosition(TextOutside);

	double length = sweepLength() * Vip::ToRadian * radius();
	angle = startAngle() + (sweepLength() * tval / length);

	// Text that are located on 45° (on diagonals) are too close to the backbone.
	// Below is a way to take them away

	// clean angle to have it between 0 and 360
	/* while (angle < 0)
		angle += 360;
	while (angle > 360) angle -= 360;
	//get reduced angle between 0 and 90
	double reduced;
	if (angle < 90) reduced = angle;
	else if (angle < 180) reduced = angle - 90;
	else if(angle < 270) reduced = angle - 180;
	else reduced = angle - 270;
	double additional = 1 - qAbs((reduced - 45.)/45.);
	if (angle < 90 && textPosition() == TextOutside)
		additional *= 2;
	if (textPosition() == TextInside)
		additional *=2;*/

	if (textPosition() == TextOutside) {
		dist = spacing();
		if (hasComponent(VipAbstractScaleDraw::Backbone))
			dist += qMax(1., // penWidth()
				     this->componentPen(VipAbstractScaleDraw::Backbone).widthF());

		if (hasComponent(VipAbstractScaleDraw::Ticks) && ticksPosition() == VipAbstractScaleDraw::TicksOutside)
			dist += tickLength(tick);
	}
	else {
		dist = -spacing();
		if (hasComponent(VipAbstractScaleDraw::Backbone))
			dist -= qMax(1., // penWidth()
				     this->componentPen(VipAbstractScaleDraw::Backbone).widthF());

		if (hasComponent(VipAbstractScaleDraw::Ticks) && ticksPosition() == VipAbstractScaleDraw::TicksInside)
			dist -= tickLength(tick);
	}

	QLineF line(center(), QPointF(center().x(), center().y() - radius() - dist));
	line.setAngle(angle);

	return line.p2();
}

QPointF VipPolarScaleDraw::start() const
{
	QLineF line(center(), QPointF(center().x(), center().y() - radius()));
	line.setAngle(startAngle());
	return line.p2();
}

QPointF VipPolarScaleDraw::end() const
{
	QLineF line(center(), QPointF(center().x(), center().y() - radius()));
	line.setAngle(endAngle());
	return line.p2();
}

QPointF VipPolarScaleDraw::position(vip_double value, double len, Vip::ValueType type) const
{
	if (textPosition() == TextInside)
		len *= -1;

	if (type == Vip::Absolute) {
		double tval = scaleMap().transform(value);
		QLineF line(center(), QPointF(center().x(), center().y() - radius()));
		double length = (sweepLength()) * Vip::ToRadian * radius();
		double angle = startAngle() + (sweepLength() * tval / length);
		line.setAngle(angle);
		if (length) {
			line = QLineF(line.p2(), line.p1());
			line.setLength(len);
		}

		return line.p2();
	}
	else {
		QLineF line(center(), QPointF(center().x(), center().y() - radius()));
		double angle = startAngle() + sweepLength() * value;
		line.setAngle(angle);
		if (len) {
			line = QLineF(line.p2(), line.p1());
			line.setLength(len);
		}

		return line.p2();
	}
}

vip_double VipPolarScaleDraw::value(const QPointF& position) const
{
	QLineF line(center(), position);
	double angle = line.angle();
	double dangle = angle - startAngle();

	int compare = vipCompareAngle(startAngle(), endAngle(), angle);
	if (compare >= 0) {
		if (dangle < 0)
			dangle += 360;
	}
	else {
		if (dangle > 0)
			dangle -= 360;
	}

	double tval = dangle * Vip::ToRadian * radius(); //* length / sweepLength();
	return scaleMap().invTransform(tval);
}

vip_double VipPolarScaleDraw::angle(vip_double value, Vip::ValueType type) const
{
	double angleOffset = -90;
	if (textPosition() == TextInside)
		angleOffset = 90;

	if (type == Vip::Absolute) {
		double tval = scaleMap().transform(value);
		QLineF line(center(), QPointF(center().x(), center().y() - radius()));
		double length = (sweepLength()) * Vip::ToRadian * radius();
		double angle = startAngle() + (sweepLength() * tval / length);
		return angle + angleOffset;
	}
	else {
		double angle = startAngle() + (sweepLength() * value);
		return angle + angleOffset;
	}
}

vip_double VipPolarScaleDraw::convert(vip_double value, Vip::ValueType type) const
{
	if (type == Vip::Absolute) {
		vip_double tval = scaleMap().transform(value);
		vip_double length = (sweepLength()) * Vip::ToRadian * radius();
		return tval / length;
	}
	else {
		vip_double length = (sweepLength()) * Vip::ToRadian * radius();
		vip_double pos = value * length;
		return scaleMap().invTransform(pos);
	}
}

double VipPolarScaleDraw::arcLength() const
{
	return (sweepLength()) * Vip::ToRadian * radius();
}

/// Draw a tick
///
/// \param painter VipPainter
/// \param value Value of the tick
/// \param len Length of the tick
///
/// \sa drawBackbone(), drawLabel()
void VipPolarScaleDraw::drawTick(QPainter* painter, vip_double value, double len, VipScaleDiv::TickType tick) const
{
	if (len <= 0)
		return;

	// last tick with endAngle == startAngle: do not draw
	if (d_data->endAngle - 360 == d_data->startAngle) {
		if (value == labelTicks(tick).last())
			return;
	}

	double tval = scaleMap().transform(value);
	double dist;
	double penWidth = qMax(componentPen(Backbone).widthF(), 1.0) * 0.5;

	if (ticksPosition() == VipAbstractScaleDraw::TicksInside) {
		dist = -len;
		penWidth = -penWidth;
	}
	else {
		dist = len;
	}

	QLineF line(center(), QPointF(center().x(), center().y() - radius() - penWidth));
	double length = (sweepLength()) * Vip::ToRadian * radius();
	double angle = startAngle() + (sweepLength() * tval / length);
	line.setAngle(angle);

	if (!vipIsValid(line.p1()) || !vipIsValid(line.p2()))
		return;

	QPointF start = line.p2();
	line.setLength(radius() + dist);
	QPointF end = line.p2();

	// check that last tick does not overlap with first one
	// const QList<double> &ticks = scaleDiv().ticks( VipScaleDiv::MajorTick );
	// if(ticks.size() > 2 && value == ticks.last() && value == ticks.first())
	// {
	// return;
	// }

	painter->drawLine(start, end);
}

/// Draws the baseline of the scale
/// \param painter VipPainter
///
/// \sa drawTick(), drawLabel()
void VipPolarScaleDraw::drawBackbone(QPainter* painter) const
{
	// const double penWidth = componentPen(Backbone).widthF();
	QRectF rect(center() - QPointF(radius(), radius()), QSizeF(radius() * 2, radius() * 2));
	double st = startAngle();
	double span = sweepLength();
	painter->drawArc(rect, st * 16, span * 16);
}

QTransform VipPolarScaleDraw::labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const
{
	double angle;
	QPointF pos = labelPosition(value, angle, tick);
	QSizeF labelSize = text.textSize();
	QTransform tr = textTransformation(textTransform(tick), textPosition(), angle, pos, labelSize);
	addLabelTransform(tr, labelSize, tick);
	if (double rot = labelRotation(value, text.textSize(), tick))
		tr.rotate(rot);
	return tr;
}

/// Draws the label for a major scale tick
///
/// \param painter VipPainter
/// \param value Value
///
/// \sa drawTick(), drawBackbone()
void VipPolarScaleDraw::drawLabel(QPainter* painter, vip_double value, const VipText& lbl, VipScaleDiv::TickType tick) const
{
	if (lbl.isEmpty())
		return;

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipPolarScaleDraw*>(this)->setTextPosition(TextOutside);

	double angle;
	QPointF pos = labelPosition(value, angle, tick);

	QSizeF labelSize = lbl.textSize();

	QTransform transform = textTransformation(textTransform(tick), textPosition(), angle, pos, labelSize);
	addLabelTransform(transform, labelSize, tick);
	if (double rot = labelRotation(value, labelSize, tick))
		transform.rotate(rot);

	// check that it does not overlap with first label
	const VipScaleDiv::TickList ticks = labelTicks(tick);
	if (ticks.size() && value == ticks.last()) {
		if (!d_data->drawLast[tick])
			return;
	}

	painter->save();

	if (textTransform(tick) != TextCurved) {
		QRect text_rect(QPoint(0, 0), labelSize.toSize());
		painter->setWorldTransform(transform, true);
		lbl.draw(painter, text_rect);
	}
	else {
		// draw curved text

		double height = labelSize.height();
		double radius = QLineF(center(), pos).length();
		VipPie tpie(angle, angle);

		if (textPosition() == VipAbstractScaleDraw::TextInside) {
			tpie.setMaxRadius(radius);
			tpie.setMinRadius(radius - height);
			lbl.draw(painter, center(), tpie);
		}
		else {
			tpie.setMinRadius(radius);
			tpie.setMaxRadius(radius + height);
			lbl.draw(painter, center(), tpie);
		}
	}

	painter->restore();
}

QPolygonF VipPolarScaleDraw::labelPolygon(vip_double value, VipScaleDiv::TickType tick) const
{
	VipScaleText lbl = tickLabel(value, tick);
	if (lbl.text.isEmpty())
		return QRectF();

	double angle;
	QPointF pos = labelPosition(value, angle, tick);
	QSizeF labelSize = lbl.text.textSize();

	QTransform transform = textTransformation(textTransform(tick), textPosition(), angle, pos, labelSize) * lbl.tr;
	addLabelTransform(transform, labelSize, tick);
	if (double rot = labelRotation(value, labelSize, tick))
		transform.rotate(rot);

	return transform.map(lbl.text.textRect());
}

void VipPolarScaleDraw::updateMap()
{
	double start = 0;
	double len = (endAngle() - startAngle()) * Vip::ToRadian * radius();
	scaleMap().setPaintInterval(start, start + len);
}

class VipRadialScaleDraw::PrivateData
{
public:
	PrivateData()
	  : center(0, 0)
	  , startRadius(0)
	  , endRadius(1)
	  , angle(0)
	{
	}

	QPointF center;
	double startRadius;
	double endRadius;
	double angle;
	QLineF scaleLine;
};

/// \brief Constructor
///
/// The range of the scale is initialized to [0, 100],
/// The position is at (0, 0) with a length of 100.
/// The orientation is VipAbstractScaleDraw::Bottom.
VipRadialScaleDraw::VipRadialScaleDraw()
{
	d_data = new PrivateData;
	setTicksPosition(TicksOutside);
}

//! Destructor
VipRadialScaleDraw::~VipRadialScaleDraw()
{
	delete d_data;
}

void VipRadialScaleDraw::setCenter(const QPointF& center)
{
	d_data->center = center;
	computeScaleLine();
	updateMap();
}

void VipRadialScaleDraw::setStartRadius(double radius)
{
	d_data->startRadius = radius;
	computeScaleLine();
	updateMap();
}

void VipRadialScaleDraw::setEndRadius(double radius)
{
	d_data->endRadius = radius;
	computeScaleLine();
	updateMap();
}

void VipRadialScaleDraw::setAngle(double start)
{
	d_data->angle = start;
	computeScaleLine();
	updateMap();
}

QPointF VipRadialScaleDraw::center() const
{
	return d_data->center;
}

double VipRadialScaleDraw::startRadius() const
{
	return d_data->startRadius;
}

double VipRadialScaleDraw::endRadius() const
{
	return d_data->endRadius;
}

double VipRadialScaleDraw::angle() const
{
	return d_data->angle;
}

double VipRadialScaleDraw::length() const
{
	return qAbs(d_data->endRadius - d_data->startRadius);
}

/// Shortest vipDistance between line and point
static double vipDistance(const QLineF& l, const QPointF& p)
{
	QLineF l2(l);
	l2.translate(p - l2.p1());
	QLineF normal = l2.normalVector();
	QPointF intersect;
	normal.intersects(l, &intersect);
	return QLineF(intersect, p).length();
}

void VipRadialScaleDraw::getBorderDistHint(double& start, double& end, VipScaleDiv::TickType tick) const
{
	start = end = 0;

	const VipScaleDiv::TickList ticks = labelTicks(tick);
	if (ticks.size() < 2)
		return;

	QLineF line = scaleLine();
	QLineF normal1 = line.normalVector();
	QLineF normal2 = normal1.translated(line.p2() - normal1.p1());

	QPolygonF t_polygon = labelPolygon(ticks.first(), tick);
	double w = 0;

	for (int j = 0; j < t_polygon.size(); ++j) {
		const double len = vipDistance(normal1, t_polygon[j]);
		if (len > w)
			w = len;
	}

	start = w;

	t_polygon = labelPolygon(ticks.last(), tick);
	w = 0;

	for (int j = 0; j < t_polygon.size(); ++j) {
		const double len = vipDistance(normal2, t_polygon[j]);
		if (len > w)
			w = len;
	}

	end = w;
}

void VipRadialScaleDraw::getBorderDistHint(double& start, double& end) const
{
	start = end = 0;
	for (int i = 0; i < VipScaleDiv::NTickTypes; ++i) {
		VipScaleDiv::TickType type = static_cast<VipScaleDiv::TickType>(i);
		if (drawLabelEnabled(type)) {
			double s, e;
			getBorderDistHint(s, e, type);
			start = qMax(start, s);
			end = qMax(end, e);
		}
	}
}

/// Calculate the width/height that is needed for a
/// vertical/horizontal scale.
///
/// The extent is calculated from the pen width of the backbone,
/// the major tick length, the spacing and the maximum width/height
/// of the labels.
///
/// \param font Font used for painting the labels
/// \return Extent
///
/// \sa minLength()
double VipRadialScaleDraw::extent(VipScaleDiv::TickType tick) const
{
	double d = 0;
	double factor = 1;

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipRadialScaleDraw*>(this)->setTextPosition(TextOutside);

	if (textPosition() == TextInside)
		factor = -1;

	QLineF line = scaleLine();

	if (hasComponent(VipAbstractScaleDraw::Labels)) {
		double w = 0;

		const VipScaleDiv::TickList ticks = labelTicks(tick);
		for (int i = 0; i < ticks.count(); i++) {
			const vip_double v = ticks[i];
			if (scaleDiv().contains(v)) {
				QPolygonF p = labelPolygon(ticks[i], tick);

				for (int j = 0; j < p.size(); ++j) {
					const double len = vipDistance(line, p[j]);
					if (len > w)
						w = len;
				}
			}
		}

		d = w;
	}

	if (hasComponent(VipAbstractScaleDraw::Ticks) && d == 0) {
		d += maxTickLength();
	}

	if (hasComponent(VipAbstractScaleDraw::Backbone)) {
		const double pw = qMax(1.,							     // penWidth()
				       this->componentPen(VipAbstractScaleDraw::Backbone).widthF()); // pen width can be zero
		d += pw;
	}

	d = qMax(d, minimumExtent());

	return d * factor;
}

QPointF VipRadialScaleDraw::start() const
{
	return scaleLine().p1();
}

QPointF VipRadialScaleDraw::end() const
{
	return scaleLine().p2();
}

QPointF VipRadialScaleDraw::position(vip_double val, double len, Vip::ValueType type) const
{
	if (textPosition() == TextInside)
		len *= -1;

	QPointF pt;
	if (type == Vip::Absolute) {
		double tval = scaleMap().transform(val) - startRadius();
		pt = scaleLine().pointAt(tval / length());
	}
	else {
		pt = scaleLine().pointAt(val);
	}

	if (len) {
		QLineF line = scaleLine();
		line.setP1(pt);
		line.setLength(len);
		line.setAngle(angle() + 90);
		pt = line.p2();
	}

	return pt;
}

vip_double VipRadialScaleDraw::convert(vip_double value, Vip::ValueType type) const
{

	QPointF pt;
	if (type == Vip::Absolute) {
		vip_double tval = scaleMap().transform(value) - startRadius();
		return (tval / length());
	}
	else {
		vip_double pos = value * length() + startRadius();
		return scaleMap().invTransform(pos);
	}
}

vip_double VipRadialScaleDraw::value(const QPointF& position) const
{
	QLineF line(center(), position);
	// line.setP1(line.pointAt(startRadius()/line.length()));
	return scaleMap().invTransform(line.length());
}

vip_double VipRadialScaleDraw::angle(vip_double value, Vip::ValueType) const
{
	Q_UNUSED(value)
	if (textPosition() == TextInside)
		return angle() - 180;
	else
		return angle();
}

const QLineF& VipRadialScaleDraw::scaleLine() const
{
	return d_data->scaleLine;
}

/// Find the position, where to paint a label
///
/// The position has a vipDistance that depends on the length of the ticks
/// in direction of the alignment().
///
/// \param value Value
/// \return Position, where to paint a label
QPointF VipRadialScaleDraw::labelPosition(vip_double value, VipScaleDiv::TickType tick) const
{
	double dist;

	if (textPosition() == TextAutomaticPosition)
		const_cast<VipRadialScaleDraw*>(this)->setTextPosition(TextOutside);

	if (textPosition() == TextOutside) {
		dist = spacing();
		if (hasComponent(VipAbstractScaleDraw::Backbone))
			dist += qMax(1., // penWidth()
				     this->componentPen(VipAbstractScaleDraw::Backbone).widthF());

		if (hasComponent(VipAbstractScaleDraw::Ticks) && ticksPosition() == VipAbstractScaleDraw::TicksOutside)
			dist += tickLength(tick);
	}
	else {
		dist = -spacing();
		if (hasComponent(VipAbstractScaleDraw::Backbone))
			dist -= qMax(1., // penWidth()
				     this->componentPen(VipAbstractScaleDraw::Backbone).widthF());

		if (hasComponent(VipAbstractScaleDraw::Ticks) && ticksPosition() == VipAbstractScaleDraw::TicksInside)
			dist -= tickLength(tick);
	}

	QLineF line = scaleLine();
	QPointF pos = position(value, 0, Vip::Absolute);
	line = line.normalVector();
	line.translate(pos - line.p1());
	line.setLength(dist);

	return line.p2();
}

/// Draw a tick
///
/// \param painter VipPainter
/// \param value Value of the tick
/// \param len Length of the tick
///
/// \sa drawBackbone(), drawLabel()
void VipRadialScaleDraw::drawTick(QPainter* painter, vip_double value, double len, VipScaleDiv::TickType tick) const
{
	Q_UNUSED(tick)
	if (len <= 0)
		return;

	double dist;

	if (ticksPosition() == VipAbstractScaleDraw::TicksInside)
		dist = -len;
	else
		dist = len;

	QLineF line = scaleLine();
	QPointF pos = position(value, 0, Vip::Absolute);
	line = line.normalVector();
	line.translate(pos - line.p1());
	line.setLength(dist);

	painter->drawLine(line.p1(), line.p2());
}

/// Draws the baseline of the scale
/// \param painter VipPainter
///
/// \sa drawTick(), drawLabel()
void VipRadialScaleDraw::drawBackbone(QPainter* painter) const
{
	QLineF line = scaleLine();
	painter->drawLine(line.p1(), line.p2());
}

QTransform VipRadialScaleDraw::labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const
{
	double angle = 0;
	QPointF pos = labelPosition(value, tick);
	QSizeF labelSize = text.textSize();
	QTransform tr = textTransformation(textTransform(tick), textPosition(), angle, pos, labelSize);
	addLabelTransform(tr, labelSize, tick);
	if (double rot = labelRotation(value, text.textSize(), tick))
		tr.rotate(rot);
	return tr;
}

/// Draws the label for a major scale tick
///
/// \param painter VipPainter
/// \param value Value
///
/// \sa drawTick(), drawBackbone()
void VipRadialScaleDraw::drawLabel(QPainter* painter, vip_double value, const VipText& lbl, VipScaleDiv::TickType tick) const
{
	if (lbl.isEmpty())
		return;

	QPointF pos = labelPosition(value, tick);

	QSizeF labelSize = lbl.textSize();

	QTransform transform = textTransformation(textTransform(tick), textPosition(), angle() + 90, pos, labelSize);
	addLabelTransform(transform, labelSize, tick);
	if (double rot = labelRotation(value, labelSize, tick))
		transform.rotate(rot);

	painter->save();

	QRect text_rect(QPoint(0, 0), labelSize.toSize());
	painter->setWorldTransform(transform, true);
	lbl.draw(painter, text_rect);

	painter->restore();
}

QPolygonF VipRadialScaleDraw::labelPolygon(vip_double value, VipScaleDiv::TickType tick) const
{
	VipScaleText lbl = tickLabel(value, tick);
	if (lbl.text.isEmpty())
		return QRectF();

	QPointF pos = labelPosition(value, tick);
	QSizeF labelSize = lbl.text.textSize();

	QTransform transform = textTransformation(textTransform(tick), textPosition(), angle() + 90, pos, labelSize) * lbl.tr;
	addLabelTransform(transform, labelSize, tick);
	if (double rot = labelRotation(value, labelSize, tick))
		transform.rotate(rot);
	return transform.map(lbl.text.textRect());
}

void VipRadialScaleDraw::updateMap()
{
	double start = startRadius();
	double len = length();
	scaleMap().setPaintInterval(start, start + len);
}

void VipRadialScaleDraw::computeScaleLine()
{
	d_data->scaleLine = QLineF(QPointF(center().x(), center().y()), QPointF(center().x(), center().y() - endRadius()));
	d_data->scaleLine.setAngle(angle());
	d_data->scaleLine.setP1(d_data->scaleLine.pointAt(startRadius() / endRadius()));
}
