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

#ifndef VIP_SCALE_DRAW_H
#define VIP_SCALE_DRAW_H

#include <QDateTime>
#include <QFontMetricsF>
#include <QMap>
#include <QPen>
#include <QTransform>
#include <QObject>

#include "VipScaleDiv.h"
#include "VipText.h"

/** \addtogroup Plotting
 *  @{
 */

class VipValueTransform;
class VipScaleMap;
class VipAbstractScaleDraw;

/// @brief Defines additional text that needs to be drawn by a VipValueToText object.
class VipScaleText
{
public:
	VipText text;		    //! text to draw
	vip_double value;	    //! associated value in the scale
	QTransform tr;		    //! potential text transform
	VipScaleDiv::TickType tick; //! associated tick type in the scale

	VipScaleText(const VipText& text = VipText(), vip_double v = 0, const QTransform& tr = QTransform(), VipScaleDiv::TickType tick = VipScaleDiv::MajorTick)
	  : text(text)
	  , value(v)
	  , tr(tr)
	  , tick(tick)
	{
	}
};

/// @brief Base class used to draw scale labels.
///
/// VipValueToText converts scale values to QString and back using VipValueToText::convert()
/// and VipValueToText::fromString().
///
/// The default implementation just convert floating point values to QString using a provided locale (default to current one).
///
/// The Plotting library provides other implementations:
/// -   VipValueToFormattedText: convert floating point values to QString using C style formatting
/// -   VipTimeToText: intepret floating point values as a time and use QDateTime to format as string
///
/// VipValueToText handles other aspects of scale labels:
/// -   Additional labels can be draw by the scale if VipValueToText::additionalText() returns a non empty list
/// -   VipValueToText can manage exponents based on a maximum label length
///
/// Inherits QObject in order to be used through a QPointer by VipFixedScaleEngine
///
class VIP_PLOTTING_EXPORT VipValueToText : public QObject
{
	Q_OBJECT
public:
	/// @brief Type of VipValueToText
	enum ValueToTextType
	{
		ValueToText,
		ValueToFormattedText,
		ValueToDate,
		ValueToTime, // deprecated
		FixedValueToText,
		TimeToText,
		User = 100
	};

	VipValueToText();
	virtual ~VipValueToText();

	/// @brief Returns the VipValueToText type
	virtual ValueToTextType valueToTextType() const { return ValueToText; }

	/// @brief Get/set the locale used for text conversion
	const QLocale& locale() const { return m_locale; }
	void setLocale(const QLocale& loc) { m_locale = loc; }

	/// @brief Set the exponent factor applied to all values.
	/// The exponent itself is not drawn by the VipValueToText and should be drawn by the parent VipAbstractScale.
	/// This function will disable automatic exponent.
	virtual void setExponent(int);
	int exponent() const;

	/// @brief If exponent is not null, returns a HTML string like '*10^2' (for exponent==2).
	QString exponentText() const;

	/// @brief Let the VipValueToText automatically select the best exponent.
	/// The exponent is used only if the maximum label length is greater than maxLabelSize().
	void setAutomaticExponent(bool automatic);
	bool automaticExponent() const;

	/// @brief Set the maximum labe size used to automatically compute the scale exponent
	/// Only works if automaticExponent() is true
	void setMaxLabelSize(int min_label_size);
	int maxLabelSize() const;

	/// @brief Returns the multiplication factor applied to a floating point value before conversion to string.
	/// By default this factor is set to 1, and is adjusted by the exponent mechanism.
	/// It is possible to completely disable the exponent mechanism and use this factor for your own usage.
	double multiplyFactor() const { return d_pow; }
	void setMultiplyFactor(double factor) { d_pow = factor; }

	/// @brief Tells if this implementation support exponents
	/// By default, only raw VipValueToText supports exponents.
	virtual bool supportExponent() const { return valueToTextType() == ValueToText; }

	/// @brief Find the best exponent value for given scale
	virtual int findBestExponent(const VipAbstractScaleDraw* scale) const;

	/// @brief Convert a scale floating point value to text for given tick
	virtual QString convert(vip_double value, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const;

	/// @brief Convert a string value (formatted by this object) into a floating point value.
	/// If the conversion fails and ok is not null, it should be set to false.
	virtual vip_double fromString(const QString& text, bool* ok = nullptr) const;

	/// @brief Returns the list of additional labels to be drawn by the scale
	/// Default implementation returns an empty list.
	virtual QList<VipScaleText> additionalText(const VipAbstractScaleDraw* scale) const;

protected:
	int d_exponent;
	int d_max_label_size;
	bool d_autoExponent;
	double d_pow;
	QLocale m_locale;
};

/// @brief Convert a floating point value to text using C style format.
///
/// VipValueToFormattedText is similar to VipValueToText, but use a C style formatting string
/// (like "%.f2") instead of a locale for float to string conversion.
///
/// If provided format is empty, the current locale will be used.
///
class VIP_PLOTTING_EXPORT VipValueToFormattedText : public VipValueToText
{
	Q_OBJECT
	QString d_text;

public:
	/// @brief Construct from a C style format
	VipValueToFormattedText(const QString& format = QString());

	/// @brief Set the C style format
	void setFormat(const QString& format);
	QString format() const;

	virtual ValueToTextType valueToTextType() const { return ValueToFormattedText; }
	virtual QString convert(vip_double value, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const;
};

/// @brief Convert a time value expressed in VipValueToDate::ValueType unit from a reference date to a string using given format.
///
/// To convert a value to string, VipValueToDate starts from provided QDateTime reference (default to Epoch) and adds
/// given value (multiplied by multiplyFactor()), considering it as nanoseconds, microseconds, milliseconds, seconds, minutes, hours or days
/// depending on ValueType flag.
///
/// The QDateTime is then converted to string using QDateTime::toString() and provided format.
///
class VIP_PLOTTING_EXPORT VipValueToDate : public VipValueToText
{
	Q_OBJECT
public:
	/// @brief Input value type
	enum ValueType
	{
		NanoSeconds,
		MicroSeconds,
		MilliSeconds,
		Seconds,
		Minutes,
		Hours,
		Days
	};

	/// @brief Construct from a format, a ValueType and a multiplication factor
	VipValueToDate(const QString& format = "", ValueType type = NanoSeconds, double multiply_factor = 1);

	virtual QString convert(vip_double value, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const;
	virtual vip_double fromString(const QString& text, bool* ok) const;
	virtual ValueToTextType valueToTextType() const { return ValueToDate; }

	QDateTime reference() const;
	void setReference(const QDateTime&);

	QString format() const;
	void setFormat(const QString& format);

	ValueType inputType() const;
	void setInputType(ValueType);

private:
	QString d_format;
	ValueType d_type;
	QDateTime d_ref;
};

/// @brief Convert a time value (\a qint64 type) into a string representation.
/// The input time value is always considered to be in nano seconds.
/// The output string representation can be: raw nano seconds, nano seconds since Epoch, micro seconds, micro seconds since Epoch,
/// milli seconds, milli seconds since Epoch, seconds, seconds since Epoch.
/// This class is used to modify the plot time scales.
///
/// Currently deprecated, VipTimeToText should be used instead
///
class VIP_PLOTTING_EXPORT VipValueToTime : public VipValueToText
{
	Q_OBJECT
public:
	/**
	Defines how the scale values should be interpreted
	*/
	enum TimeType
	{
		NanoSeconds,
		NanoSecondsSE,
		MicroSeconds,
		MicroSecondsSE,
		MilliSeconds,
		MilliSecondsSE,
		Seconds,
		SecondsSE
	};

	/// Defines how the scale values should be displayed
	enum DisplayType
	{
		Double,
		Integer,
		AbsoluteDateTime
	};

	TimeType type;
	vip_double startValue;
	bool drawAdditionalText;
	bool fixedStartValue;
	DisplayType displayType;
	QString format;

	VipValueToTime(TimeType type = NanoSeconds, double startValue = 0);

	virtual ValueToTextType valueToTextType() const { return ValueToTime; }

	virtual QString convert(vip_double value, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const;

	virtual vip_double fromString(const QString& text, bool* ok) const;
	virtual QList<VipScaleText> additionalText(const VipAbstractScaleDraw* scale) const;
	VipValueToTime* copy() const;

	QString timeUnit() const;

	static QString convert(vip_double value, TimeType type, const QString& format = QString());
	static TimeType findBestTimeUnit(const VipInterval& time_interval);
};

/// @brief Similar to VipValueToFormattedText, but can be used with VipFixedScaleEngine to
/// provide a fixed tick positions.
///
/// When used with VipFixedScaleEngine, the scale using VipFixedValueToText will display fixed tick positions.
/// The tick values will depend on given TextType. If AbsoluteValue, the tick values will correspond to the actual
/// value passed to VipFixedValueToText::convert().
///
/// If DifferenceValue is used, the tick values will correspond to the difference between passed value and VipFixedValueToText::startValue().
/// In this case, the left most display value is constrolled by VipFixedValueToText::additionalText().
///
/// The start value is 0 by default, and is updated by the VipFixedScaleEngine (if used).
///
/// See CurveStreaming test example for more details.
///
class VIP_PLOTTING_EXPORT VipFixedValueToText : public VipValueToFormattedText
{
	Q_OBJECT
public:
	/// @brief Define how text values are computed
	enum TextType
	{
		/// @brief Similar to VipValueToFormattedText::convert(value)
		AbsoluteValue,
		/// @brief Similar to VipValueToFormattedText::convert(value - startValue())
		DifferenceValue,
		/// @brief Similar to VipValueToFormattedText::convert(value - startValue()), and no displayed additionaal text
		DifferenceValueNoAdditional
	};

	VipFixedValueToText(const QString& text = QString(), TextType type = AbsoluteValue)
	  : VipValueToFormattedText(text)
	  , d_start(0)
	  , d_type(type)
	{
	}

	virtual ValueToTextType valueToTextType() const { return FixedValueToText; }

	/// @brief Set the start value, only meaningful with DifferenceValue text style.
	/// Automatically called by VipFixedScaleEngine if used.
	void setStartValue(vip_double start) { d_start = start; }
	vip_double startValue() const { return d_start; }

	/// @brief Set the text type which define the float to string conversion type
	void setTextType(TextType type) { d_type = type; }
	TextType textType() const { return d_type; }

	virtual QString convert(vip_double value, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const
	{
		return VipValueToFormattedText::convert(d_type == AbsoluteValue ? value : value - d_start, tick);
	}

	virtual vip_double fromString(const QString& text, bool* ok = nullptr) const
	{
		vip_double res = VipValueToFormattedText::fromString(text, ok);
		if (d_type != AbsoluteValue)
			res += d_start;
		return res;
	}

	virtual QList<VipScaleText> additionalText(const VipAbstractScaleDraw*) const
	{
		if (d_type != DifferenceValue)
			return QList<VipScaleText>();

		VipScaleText res;
		res.text = (this->VipValueToFormattedText::convert(d_start));
		res.value = d_start;
		return QList<VipScaleText>() << res;
	}

private:
	vip_double d_start;
	TextType d_type;
};

/// @brief A VipValueToFormattedText class that displays date and/or time labels.
/// Depending on the provided TimeType, and once applied the multiplication factor,
/// times are considered to be in milliseconds (0 based) or milliseconds since epoch.
/// Time are formatted to text using the provided labelFormat().
/// If the TextType is set to DifferenceValue, the additional displayed text is formatted using additionalFormat().
///
class VIP_PLOTTING_EXPORT VipTimeToText : public VipFixedValueToText
{
	Q_OBJECT
public:
	enum TimeType
	{
		Milliseconds,
		MillisecondsSE
	};
	VipTimeToText(const QString& format = QString(), TimeType time_type = Milliseconds, TextType type = AbsoluteValue);

	virtual ValueToTextType valueToTextType() const { return TimeToText; }

	/// @brief Format string used to convert QTime (Milliseconds) or QDateTime (MillisecondsSE) to string
	void setLabelFormat(const QString&);
	QString labelFormat() const;

	/// @brief Format string used to convert QTime (Milliseconds) or QDateTime (MillisecondsSE) to string
	/// For any additional text as returned by additionalText()
	void setAdditionalFormat(const QString&);
	QString additionalFormat() const;

	void setTimeType(TimeType);
	TimeType timeType() const;

	virtual QString convert(vip_double value, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const;
	virtual vip_double fromString(const QString& text, bool* ok = nullptr) const;
	virtual QList<VipScaleText> additionalText(const VipAbstractScaleDraw*) const;

private:
	TimeType d_timeType;
	QString d_labelFormat;
	QString d_additionalFormat;
};

/// @brief A abstract base class for drawing scales
///
///  VipAbstractScaleDraw can be used to draw linear or logarithmic scales.
///
///  After a scale division has been specified as a VipScaleDiv object
///  using setScaleDiv(), the scale can be drawn with the draw() member.
///
class VIP_PLOTTING_EXPORT VipAbstractScaleDraw : public QObject
{
	Q_OBJECT
public:
	/// @brief Components of a scale
	enum ScaleComponent
	{
		//! Backbone = the line where the ticks are located
		Backbone = 0x01,

		//! Ticks
		Ticks = 0x02,

		//! Labels
		Labels = 0x04,

		//! All components
		AllComponents = Backbone | Ticks | Labels
	};
	/// @brief Scale components
	typedef QFlags<ScaleComponent> ScaleComponents;

	/// @brief Labels position
	enum TextPosition
	{
		// Labels are located on the closest side to the plotting area center
		TextInside,
		// Labels are located on the farthest side to the plotting area center
		TextOutside,
		// Automatic position, only used when drawinf text inside pies
		TextAutomaticPosition
	};

	/// @brief Text transformation
	enum TextTransform
	{
		TextHorizontal,
		TextParallel,
		TextPerpendicular,
		TextCurved
	};

	/*!
	   Ticks position
	   \sa setTicksPosition(), ticksPosition()
	*/
	enum TicksPosition
	{
		//! Ticks point to the plot canvas
		TicksInside,
		//! Ticks point to the ticks label
		TicksOutside
	};

	/// @brief For custom labels set with setCustomLabelText() or setCustomLabels(),
	/// defines which text style to be used
	enum CustomTextStyle
	{
		/// @brief Use textStyle(tick), default
		UseTickTextStyle,
		/// @brief Use passed text style
		UseCustomTextStyle
	};

	VipAbstractScaleDraw();
	virtual ~VipAbstractScaleDraw();

	virtual void setScaleDiv(const VipScaleDiv& s);
	const VipScaleDiv& scaleDiv() const;

	virtual void setTransformation(VipValueTransform*);
	const VipValueTransform* transformation() const;

	const VipScaleMap& scaleMap() const;
	VipScaleMap& scaleMap();

	void enableComponent(ScaleComponent, bool enable = true);
	bool hasComponent(ScaleComponent) const;
	void setComponents(int);
	int components() const;

	virtual void setTickLength(VipScaleDiv::TickType, double length);
	double tickLength(VipScaleDiv::TickType) const;
	double maxTickLength() const;

	virtual void setLabelTransform(const QTransform&, VipScaleDiv::TickType tick);
	QTransform labelTransform(VipScaleDiv::TickType tick) const;

	virtual void setLabelTransformReference(const QPointF&, VipScaleDiv::TickType tick);
	QPointF labelTransformReference(VipScaleDiv::TickType tick) const;

	virtual void setTextTransform(TextTransform, VipScaleDiv::TickType);
	TextTransform textTransform(VipScaleDiv::TickType) const;

	/**
	Add an additional rotation to the label text, starting to the label top left corner.
	Note that this rotation is just added to the global label transformation.
	*/
	void setLabelRotation(double rotation, VipScaleDiv::TickType tick);
	double labelRotation(VipScaleDiv::TickType tick) const;
	/**
	Retrieves the label rotation for a given value and text size.
	By default, returns the result of #labelRotation(VipScaleDiv::TickType).
	This function can be reimplemented to combine the standard label rotation with label value and text size
	to provide a finer rotation value.
	*/
	virtual double labelRotation(double value, const QSizeF& size, VipScaleDiv::TickType tick) const;

	/**
	 * Set the interval in which tick labels are drawn.
	 * By default, all tick labels are drawn.
	 */
	void setLabelInterval(const VipInterval& interval);
	VipInterval labelInterval() const;

	virtual void setTextStyle(const VipTextStyle& style, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick);
	const VipTextStyle& textStyle(VipScaleDiv::TickType tick = VipScaleDiv::MajorTick) const;
	VipTextStyle& textStyle(VipScaleDiv::TickType tick = VipScaleDiv::MajorTick);

	/// @brief Set the text style used by the additional text .
	/// By default, the addditional text style is this->textStyle(VipScaleDiv::MajorTick).
	void setAdditionalTextStyle(const VipTextStyle& style);
	const VipTextStyle& additionalTextStyle() const;
	/// @brief Reset the addditional text style and use this->textStyle(VipScaleDiv::MajorTick) instead.
	void resetAdditionalTextStyle();

	void setAdditionalTextTransform(const QTransform&);
	const QTransform& additionalTextTransform() const;

	virtual void setComponentPen(int component, const QPen& pen);
	QPen componentPen(ScaleComponent component) const;

	virtual void setTicksPosition(TicksPosition position);
	TicksPosition ticksPosition() const;

	void setTextPosition(TextPosition pos);
	TextPosition textPosition() const;

	virtual void enableDrawLabel(VipScaleDiv::TickType, bool enable);
	bool drawLabelEnabled(VipScaleDiv::TickType) const;

	virtual void enableLabelOverlapping(bool enable);
	bool labelOverlappingEnabled() const;
	QSharedPointer<QPainterPath> thisLabelArea() const;
	/**
	Add the label path of another VipAbstractScaleDraw in order to avoid text superimposition between several scales.
	The other VipAbstractScaleDraw must belong to a VipAbstractScale that shares the same parent item
	as this VipAbstractScale.
	*/
	void addAdditionalLabelOverlapp(const QSharedPointer<QPainterPath>& other);
	QVector<QSharedPointer<QPainterPath>> additionalLabelOverlapp() const;
	void setAdditionalLabelOverlapp(const QVector<QSharedPointer<QPainterPath>>& other);
	void removeAdditionalLabelOverlapp(const QSharedPointer<QPainterPath>& other);
	void clearAdditionalLabelOverlapp();

	void setCustomTextStyle(CustomTextStyle);
	CustomTextStyle customTextStyle() const;

	void setCustomLabelText(const VipText& label_text, VipScaleDiv::TickType tick);
	VipText customLabelText(VipScaleDiv::TickType tick) const;

	virtual void setCustomLabels(const QList<VipScaleText>& custom_labels);
	QList<VipScaleText> customLabels() const;
	bool hasCustomLabels() const;
	// double value(const QString & text, double default_value = 0.) const;

	virtual void setSpacing(double margin);
	double spacing() const;

	/*void setPenWidth( int width );
	int penWidth() const;*/

	virtual void draw(QPainter*) const;

	void setValueToText(VipValueToText*);
	VipValueToText* valueToText() const;
	VipText label(vip_double, VipScaleDiv::TickType tick) const;

	/// @brief Add a text to be drawn on the scale
	/// If id is 0 (or is invalid), this creates a new scale text and returns its id.
	/// If provided id is valid, this updates the corresponding scale text.
	/// This function can be used by any object to add text to an existing scale.
	int addScaleText(int id, const VipScaleText& text);
	/// @brief Remove a scale text based on its id.
	void removeScaleText(int id);
	/// @brief Remove all scale text
	void removeAllScaleText();
	/*!
	  Calculate the extent

	  The extent is the vipDistance from the baseline to the outermost
	  pixel of the scale draw in opposite to its orientation.
	  It is at least minimumExtent() pixels.

	  \param font Font used for drawing the tick labels
	  \return Number of pixels

	  \sa setMinimumExtent(), minimumExtent()
	*/
	virtual double extent(VipScaleDiv::TickType) const = 0;

	double fullExtent() const;

	/**
	 Returns the position of given value.
	 if \a type is #Vip::Absolute, given value is in scale division coordinate.
	 Otherwise, value is inside [0,1] and is relative to the backbone length.
	 \a length is the vipDistance perpendicular to the backbone, in the  opposite direction from the labels.
	 */
	virtual QPointF position(vip_double value, double length = 0, Vip::ValueType type = Vip::Absolute) const = 0;

	/**
	Returns the scale value at given position.
	 */
	virtual vip_double value(const QPointF& position) const = 0;

	/**
	Convert a relative value to an absolute one, or conversly.
	 */
	virtual vip_double convert(vip_double value, Vip::ValueType type = Vip::Absolute) const = 0;

	/**
	Returns the backbone tangential angle at given value.
	if \a type is #Vip::Absolute, given value is in scale division coordinate.
	Otherwise, value is inside [0,1] and is relative to the backbone length.
	The angle direction must leave the scale label to the left.
	 */
	virtual vip_double angle(vip_double value, Vip::ValueType type = Vip::Absolute) const = 0;

	virtual void getBorderDistHint(double& start, double& end) const = 0;

	virtual QPointF start() const = 0;
	virtual QPointF end() const = 0;

	virtual QTransform labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const = 0;

	virtual void setMinimumExtent(double);
	double minimumExtent() const;

	VipScaleText tickLabel(vip_double value, VipScaleDiv::TickType tick) const;
	bool drawLabelOverlap(QPainter* painter, vip_double value, const VipText& t, VipScaleDiv::TickType tick) const;
	bool drawTextOverlap(QPainter* painter, const VipText& t) const;

	/** Invalidate the internal label cache. Usually you do not need to call it explicitly. */
	void invalidateCache();
	void invalidateOverlap();
	const QMap<vip_double, VipScaleText>& additionalText() const;

protected:
	/*!
	   Draw a tick

	   \param painter VipPainter
	   \param value Value of the tick
	   \param len Length of the tick

	   \sa drawBackbone(), drawLabel()
	*/
	virtual void drawTick(QPainter* painter, vip_double value, double len, VipScaleDiv::TickType tick) const = 0;
	virtual void drawTicks(QPainter*) const;

	/*!
	  Draws the baseline of the scale
	  \param painter VipPainter

	  \sa drawTick(), drawLabel()
	*/
	virtual void drawBackbone(QPainter* painter) const = 0;

	/*!
	    Draws the label for a major scale tick

	    \param painter VipPainter
	    \param value Value

	    \sa drawTick(), drawBackbone()
	*/
	virtual void drawLabel(QPainter* painter, vip_double value, const VipText& t, VipScaleDiv::TickType tick) const = 0;
	virtual void drawLabels(QPainter*) const;

	/**
	Returns, for a given tick type, all values that display a label
	 */
	VipScaleDiv::TickList labelTicks(VipScaleDiv::TickType tick) const;

	void addLabelTransform(QTransform& textTransform, const QSizeF& textSize, VipScaleDiv::TickType tick) const;

private:
	VipAbstractScaleDraw(const VipAbstractScaleDraw&);
	VipAbstractScaleDraw& operator=(const VipAbstractScaleDraw&);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipAbstractScaleDraw::ScaleComponents)

VIP_PLOTTING_EXPORT QTransform
textTransformation(VipAbstractScaleDraw::TextTransform textTransform, VipAbstractScaleDraw::TextPosition textPosition, double angle, const QPointF& pos, const QSizeF& size);

/*!
  \brief A class for drawing scales

  VipScaleDraw can be used to draw linear or logarithmic scales.
  A scale has a position, an alignment and a length, which can be specified .
  The labels can be rotated and aligned
  to the ticks using setLabelRotation() and setLabelAlignment().

  After a scale division has been specified as a QwtScaleDiv object
  using QwtAbstractScaleDraw::setScaleDiv(const QwtScaleDiv &s),
  the scale can be drawn with the QwtAbstractScaleDraw::draw() member.
*/
class VIP_PLOTTING_EXPORT VipScaleDraw : public VipAbstractScaleDraw
{
	Q_OBJECT
public:
	/*!
	    Alignment of the scale draw
	    \sa setAlignment(), alignment()
	 */
	enum Alignment
	{
		//! The scale is below
		BottomScale,

		//! The scale is above
		TopScale,

		//! The scale is left
		LeftScale,

		//! The scale is right
		RightScale
	};

	VipScaleDraw();
	virtual ~VipScaleDraw();

	double minLabelDist(VipScaleDiv::TickType tick) const;
	virtual double extent(VipScaleDiv::TickType tick) const;

	void getBorderDistHint(double& start, double& end) const;
	double minLabelDist() const;
	double minLength() const;

	virtual QPointF position(vip_double value, double length, Vip::ValueType type) const;
	virtual vip_double value(const QPointF& position) const;
	virtual vip_double angle(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	virtual vip_double convert(vip_double value, Vip::ValueType type = Vip::Absolute) const;

	virtual QTransform labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const;

	void move(double x, double y);
	void move(const QPointF&);
	void setLength(double length);

	Alignment alignment() const;
	void setAlignment(Alignment);

	Qt::Orientation orientation() const;

	void setIgnoreLabelTransform(bool);
	bool ignoreLabelTransform() const;

	QPointF pos() const;
	virtual QPointF start() const;
	virtual QPointF end() const;
	double length() const;

	double maxLabelHeight(VipScaleDiv::TickType tick) const;
	double maxLabelWidth(VipScaleDiv::TickType tick) const;

	QPointF labelPosition(vip_double val, VipScaleDiv::TickType tick) const;
	QRectF labelRect(vip_double val, VipScaleDiv::TickType tick) const;
	QSizeF labelSize(vip_double val, VipScaleDiv::TickType tick) const;
	QRectF boundingLabelRect(vip_double val, VipScaleDiv::TickType tick) const;

protected:
	QTransform labelTransformation(vip_double, const QPointF&, const QSizeF&, VipScaleDiv::TickType tick, Qt::Alignment text_alignment) const;

	virtual void drawTicks(QPainter*) const;
	virtual void drawTick(QPainter*, vip_double val, double len, VipScaleDiv::TickType tick) const;
	virtual void drawBackbone(QPainter*) const;
	virtual void drawLabel(QPainter*, vip_double val, const VipText& t, VipScaleDiv::TickType tick) const;

private:
	VipScaleDraw(const VipScaleDraw&);
	VipScaleDraw& operator=(const VipScaleDraw& other);

	void getBorderDistHint(double& start, double& end, VipScaleDiv::TickType tick) const;
	Qt::Orientation computeOrientation() const;
	void updateMap();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/*!
   Move the position of the scale

   \param x X coordinate
   \param y Y coordinate

   \sa move(const QPointF &)
*/
inline void VipScaleDraw::move(double x, double y)
{
	move(QPointF(x, y));
}

/*!
  \brief A class for drawing scales

  VipScaleDraw can be used to draw linear or logarithmic scales.
  A scale has a position, an alignment and a length, which can be specified .
  The labels can be rotated and aligned
  to the ticks using setLabelRotation() and setLabelAlignment().

  After a scale division has been specified as a QwtScaleDiv object
  using QwtAbstractScaleDraw::setScaleDiv(const QwtScaleDiv &s),
  the scale can be drawn with the QwtAbstractScaleDraw::draw() member.
*/
class VIP_PLOTTING_EXPORT VipPolarScaleDraw : public VipAbstractScaleDraw
{
	Q_OBJECT
public:
	VipPolarScaleDraw();
	virtual ~VipPolarScaleDraw();

	virtual void setScaleDiv(const VipScaleDiv& s);

	virtual double extent(VipScaleDiv::TickType) const;
	virtual QPointF position(vip_double value, double length, Vip::ValueType type) const;
	virtual vip_double value(const QPointF& position) const;
	virtual vip_double angle(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	virtual vip_double convert(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	virtual QPointF start() const;
	virtual QPointF end() const;

	virtual QTransform labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const;

	virtual void setCenter(const QPointF& center);
	virtual void setRadius(double radius);
	virtual void setStartAngle(double start);
	virtual void setEndAngle(double end);

	QPointF center() const;
	double radius() const;
	double startAngle() const;
	double endAngle() const;
	double sweepLength() const;

	virtual void getBorderDistHint(double& d_s_angle, double& d_e_angle) const;

	QPointF labelPosition(vip_double val, double& angle, VipScaleDiv::TickType tick) const;
	double arcLength() const;

protected:
	QPolygonF labelPolygon(vip_double val, VipScaleDiv::TickType tick) const;
	void checkDrawLast(VipScaleDiv::TickType tick);
	void checkDrawLast();

	virtual void drawTick(QPainter*, vip_double val, double len, VipScaleDiv::TickType tick) const;
	virtual void drawBackbone(QPainter*) const;
	virtual void drawLabel(QPainter*, vip_double val, const VipText& t, VipScaleDiv::TickType tick) const;

private:
	VipPolarScaleDraw(const VipPolarScaleDraw&);
	VipPolarScaleDraw& operator=(const VipPolarScaleDraw& other);

	void getDLengthHint(double& d_s_angle, double& d_e_angle, VipScaleDiv::TickType) const;
	void updateMap();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/*!
  \brief A class for drawing scales

  VipScaleDraw can be used to draw linear or logarithmic scales.
  A scale has a position, an alignment and a length, which can be specified .
  The labels can be rotated and aligned
  to the ticks using setLabelRotation() and setLabelAlignment().

  After a scale division has been specified as a QwtScaleDiv object
  using QwtAbstractScaleDraw::setScaleDiv(const QwtScaleDiv &s),
  the scale can be drawn with the QwtAbstractScaleDraw::draw() member.
*/
class VIP_PLOTTING_EXPORT VipRadialScaleDraw : public VipAbstractScaleDraw
{
	Q_OBJECT
public:
	VipRadialScaleDraw();
	virtual ~VipRadialScaleDraw();

	virtual double extent(VipScaleDiv::TickType tick) const;
	virtual QPointF position(vip_double value, double length, Vip::ValueType type) const;
	virtual vip_double value(const QPointF& position) const;
	virtual vip_double angle(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	virtual vip_double convert(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	virtual QPointF start() const;
	virtual QPointF end() const;

	virtual QTransform labelTransformation(vip_double value, const VipText& text, VipScaleDiv::TickType tick) const;

	virtual void setCenter(const QPointF& center);
	virtual void setStartRadius(double start_radius);
	virtual void setEndRadius(double end_radius);
	virtual void setAngle(double angle);

	QPointF center() const;
	double startRadius() const;
	double endRadius() const;
	double angle() const;
	double length() const;

	void getBorderDistHint(double& start, double& end) const;

	QPointF labelPosition(vip_double val, VipScaleDiv::TickType tick) const;

	const QLineF& scaleLine() const;

protected:
	QPolygonF labelPolygon(vip_double val, VipScaleDiv::TickType tick) const;

	virtual void drawTick(QPainter*, vip_double val, double len, VipScaleDiv::TickType tick) const;
	virtual void drawBackbone(QPainter*) const;
	virtual void drawLabel(QPainter*, vip_double val, const VipText& t, VipScaleDiv::TickType tick) const;

private:
	VipRadialScaleDraw(const VipPolarScaleDraw&);
	VipRadialScaleDraw& operator=(const VipPolarScaleDraw& other);

	virtual void getBorderDistHint(double& start, double& end, VipScaleDiv::TickType tick) const;
	void computeScaleLine();
	void updateMap();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/** @}*/
// end Plotting

#endif
