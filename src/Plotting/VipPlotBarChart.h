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

#ifndef VIP_PLOT_BAR_CHART_H
#define VIP_PLOT_BAR_CHART_H

#include "VipPlotItem.h"

/// \addtogroup Plotting
/// @{

/// @brief Class representing a group of bars within a VipPlotBarChart
class VIP_PLOTTING_EXPORT VipBar
{
public:
	VipBar(double pos = 0, const QVector<double>& values = QVector<double>());

	VipBar(const VipBar&);
	VipBar& operator=(const VipBar&);

	void setPosition(double x);
	double position() const;

	void setValues(const QVector<double>&);
	const QVector<double>& values() const;

	double value(int index) const;
	int valueCount() const;

private:
	double d_pos;
	QVector<double> d_values;
};

typedef QVector<VipBar> VipBarVector;
Q_DECLARE_METATYPE(VipBar);
Q_DECLARE_METATYPE(VipBarVector);

VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream&, const VipBar&);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream&, VipBar&);

/// @brief Class used to draw bar charts represented a vector of VipBar.
///
/// A bar chart can be created with a cartesian coordinate system only (see VipPlotArea2D class).
/// VipPlotBarChart can represent vertical or horizontal bar series, either stacked on each other or drawn side by side.
///
/// VipPlotBarChart supports stylesheets and adds the following attributes:
/// -	'text-alignment' : see VipPlotBarChart::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
/// -	'text-position': see VipPlotBarChart::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
/// -	'text-distance': see VipPlotBarChart::setTextDistance()
/// -	'style': bar chart style, one of 'stacked', 'sideBySide'
/// -	'border-radius': border radius for the columns
/// -   'text-value': see VipPlotBarChart::setTextValue(), one of 'eachValue', 'maxValue', 'sumValue'
/// -   'value-type': see VipPlotBarChart::setValueType(), one of 'scaleValue', 'barLength'
/// -   'width-unit': see VipPlotBarChart::setBarWidth(), one of 'itemUnit', 'axisUnit'
/// -   'bar-width': width of each bar, see VipPlotBarChart::setBarWidth()
///
///
/// Below is an example of horizontal bart chart using side by side drawing:
///
/// \code{cpp}
///
/// #include "VipPlotWidget2D.h"
/// #include "VipPlotGrid.h"
/// #include "VipPlotBarChart.h"
/// #include "VipLegendItem.h"
/// #include <qapplication.h>
///
///
/// int main(int argc, char** argv)
/// {
/// QApplication app(argc, argv);
///
/// // Create the plot widget
/// VipPlotWidget2D w;
///
/// // Hide top and right axes
/// w.area()->rightAxis()->setVisible(false);
/// w.area()->topAxis()->setVisible(false);
/// // Hide grid
/// w.area()->grid()->setVisible(false);
///
/// // Make the legend expanding vertically
/// w.area()->legend()->setExpandingDirections(Qt::Vertical);
/// // Add a margin around the plotting area
/// w.area()->setMargins(VipMargins(20, 20, 20, 20));
///
/// // Define a text style with bold font used for the axes
/// VipTextStyle st;
/// QFont f = st.font();
/// f.setBold(true);
/// st.setFont(f);
///
/// // Make the majo ticks point toward the axis labels
/// w.area()->leftAxis()->scaleDraw()->setTicksPosition(VipAbstractScaleDraw::TicksInside);
/// // Set custom labels for the left axis
/// w.area()->leftAxis()->scaleDraw()->setCustomLabels(QList<VipScaleText>() << VipScaleText("Cartier", 1) << VipScaleText("Piaget", 2) << VipScaleText("Audemars Piguet", 3) << VipScaleText("Omega",
/// 4)
/// 									 << VipScaleText("Patek Philippe", 5) << VipScaleText("Rolex", 6));
///
/// // The bottom axis uses a custom label text to add a '$' sign before the scale value
/// w.area()->bottomAxis()->scaleDraw()->setCustomLabelText("$#value", VipScaleDiv::MajorTick);
///
/// // Set the text style with bold font to both axes
/// w.area()->leftAxis()->setTextStyle(st);
/// w.area()->bottomAxis()->setTextStyle(st);
///
/// // Create the vector of VipBar
/// QVector<VipBar> bars;
/// bars << VipBar(1, QVector<double>() << 290 << 550 << 900) << VipBar(2, QVector<double>() << 430 << 600 << 220) << VipBar(3, QVector<double>() << 900 << 622 << 110)
///      << VipBar(4, QVector<double>() << 470 << 342 << 200) << VipBar(5, QVector<double>() << 400 << 290 << 150) << VipBar(6, QVector<double>() << 500 << 1000 << 1200);
///
/// // Create the bar chart
/// VipPlotBarChart* c = new VipPlotBarChart();
/// // Set the vector of VipBar
/// c->setRawData(bars);
/// // Set the name for each bar (displayed in the legend)
/// c->setBarNames(VipTextList() << "Q1"
/// 			     << "Q2"
/// 			     << "Q3");
///
/// // Set the bar width in item's unit
/// c->setBarWidth(20, VipPlotBarChart::ItemUnit);
/// // Set the spacing between bars in item's unit
/// c->setSpacing(1, VipPlotBarChart::ItemUnit);
///
/// // Set the text to be draw on top of each bar
/// c->setText(VipText("$#value").setTextPen(QPen(Qt::white)));
/// // Set the text position: inside each bar
/// c->setTextPosition(Vip::Inside);
/// // Align the text to the left of each bar
/// c->setTextAlignment(Qt::AlignLeft);
/// // Set axes (to display horizontal bars, X axis must be the left one)
/// c->setAxes(w.area()->leftAxis(), w.area()->bottomAxis(), VipCoordinateSystem::Cartesian);
///
/// w.show();
/// return app.exec();
/// }
///
class VIP_PLOTTING_EXPORT VipPlotBarChart : public VipPlotItemDataType<VipBarVector, VipBar>
{
	Q_OBJECT

public:
	/// @brief Define the text to display within each bar
	enum TextValue
	{
		/// @brief Display each bar value
		EachValue,
		/// @brief For Stacked bars, only display the maximum value
		MaxValue,
		/// @brief For Stacked bars, only display the sum of group values
		SumValue,
	};

	/// @brief Bart chart style
	enum Style
	{
		/// @brief Bars are stacked
		Stacked,
		/// @brief Bars are displayed side by side
		SideBySide
	};

	/// @brief For Stacked bars only, tells if a bar value represent a scale value or a bar length
	enum ValueType
	{
		ScaleValue,
		BarLength,
	};

	/// @brief Unit of bar spacing and width
	enum WidthUnit
	{
		/// @brief Unit is in scale coordinate
		AxisUnit,
		/// @brief Unit is in item's coordinate
		ItemUnit
	};

	explicit VipPlotBarChart(const VipText& title = VipText());
	virtual ~VipPlotBarChart();

	/// @brief Set data, must be a VipBarVector
	virtual void setData(const QVariant&);

	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	/// @brief Set/get the default box style used to build each bar box style
	void setBoxStyle(const VipBoxStyle& st);
	const VipBoxStyle& boxStyle() const;

	/// @brief Reimplemented from VipPlotItem
	virtual void setPen(const QPen& p)
	{
		VipBoxStyle s = boxStyle();
		s.setBorderPen(p);
		setBoxStyle(s);
	}
	/// @brief Reimplemented from VipPlotItem
	virtual QPen pen() const { return boxStyle().borderPen(); }
	/// @brief Reimplemented from VipPlotItem
	virtual void setBrush(const QBrush& b)
	{
		VipBoxStyle s = boxStyle();
		s.setBackgroundBrush(b);
		setBoxStyle(s);
	}
	/// @brief Reimplemented from VipPlotItem
	virtual QBrush brush() const { return boxStyle().backgroundBrush(); }

	/// @brief Set the value type for Stacked bar chart only
	void setValueType(ValueType type);
	ValueType valueType() const;

	/// @brief Set the baseline (Y coordinate) from which bars are drawn
	void setBaseline(double reference);
	double baseline() const;

	/// @brief Set the space between bars in scale or item's coordinate
	void setSpacing(double space, WidthUnit unit);
	double spacing() const;
	WidthUnit spacingUnit() const;

	/// @brief Set the bar width in scale or item's coordinate
	void setBarWidth(double width, WidthUnit unit);
	double barWidth() const;
	WidthUnit barWidthUnit() const;

	/// @brief Set the bar chart style (Stacked or SideBySide)
	void setStyle(Style style);
	Style style() const;

	/// @brief Set the type of text to be drawn inside each bar
	void setTextValue(TextValue style);
	TextValue textValue() const;

	/// @brief Set the bar text alignment within its bar based on the text position
	void setTextAlignment(Qt::Alignment align);
	Qt::Alignment textAlignment() const;

	/// @brief Set the bar text position: inside or outside the bar
	void setTextPosition(Vip::RegionPositions pos);
	Vip::RegionPositions textPosition() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform() const;
	const QPointF& textTransformReference() const;

	/// @brief Set the distance (in item's coordinate) between a bar border and its text
	/// @param distance
	void setTextDistance(double distance);
	double textDistance() const;

	/// @brief Set the text to be drawn within each bar.
	/// Each occurrence of the content '#value' will be replaced by the bar value, the maximum group value or the group sum depending on the ValueType.
	void setText(const VipText& text);
	const VipText& text() const;

	/// @brief Set the bar names as displayed in the legend
	/// The 'names' list must contain as many elements as each group of bar (VipBar object).
	void setBarNames(const QList<VipText>& names);
	const QList<VipText>& barNames() const;

	/// @brief Set the color palette used for both border and filling of each bar
	virtual void setColorPalette(const VipColorPalette&);
	virtual VipColorPalette colorPalette() const;

	/// @brief Reimplemented from VipPlotItem, in order to be responsive to stylesheets.
	virtual void setTextStyle(const VipTextStyle&);
	VipTextStyle textStyle() const;

	/// @brief Set/get the box style used to draw the bar at given index qwithin a group of bar (VipBar object).
	void setBoxStyle(const VipBoxStyle&, int);
	void setBoxStyle(const VipBoxStyle&, const QString&);
	VipBoxStyle boxStyle(int) const;
	VipBoxStyle boxStyle(const QString&) const;
	VipBoxStyle& boxStyle(int);
	VipBoxStyle& boxStyle(const QString&);

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;

	virtual QList<VipText> legendNames() const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;

	virtual QList<VipInterval> plotBoundingIntervals() const;
	virtual QString formatToolTip(const QPointF& pos) const;
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;

protected:
	void drawBarValues(QPainter*, const VipCoordinateSystemPtr&, const VipBar&, int) const;

	QList<QPolygonF> barValuesRects(const VipBar&, const VipCoordinateSystemPtr&) const;
	QRectF computePlotBoundingRect(const VipBarVector&, const VipCoordinateSystemPtr&) const;
	double value(double v) const;

	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotBarChart*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotBarChart* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotBarChart* value);

/// @}
// end Plotting

#endif
