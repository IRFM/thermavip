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

#ifndef VIP_PIE_CHART_H
#define VIP_PIE_CHART_H

#include "VipAdaptativeGradient.h"
#include "VipColorMap.h"
#include "VipPie.h"
#include "VipPlotItem.h"
#include "VipQuiver.h"
#include "VipScaleDraw.h"

/// \addtogroup Plotting
/// @{

/// @brief Base class for all pie based VipPlotItem
///
class VIP_PLOTTING_EXPORT VipAbstractPieItem : public VipPlotItemDataType<VipPie>
{
	Q_OBJECT

public:
	VipAbstractPieItem(const VipText& title = VipText());
	virtual ~VipAbstractPieItem();

	void setColor(const QColor&);

	/// @brief Set/get the box style used to draw the pie
	void setBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& boxStyle() const;
	VipBoxStyle& boxStyle();

private:
	VipBoxStyle d_style;
};

/// @brief Plot item representing a pie within a polar coordinate system.
///
/// VipPieItem can be used as an individual plotting item, or indirectly through a VipPieChart item.
///
/// VipPieItem supports stylesheets and define the following attributes:
/// -	'legend-style': see VipPieItem::setLegendStyle(), one of 'backgroundAndBorder', 'backgroundOnly', 'backgroundAndDefaultPen'
/// -	'clip-to-pie': boolean value, see VipPieItem::setClipToPie()
/// -	'text-transform': see VipPieItem::setTextTransform(), one of 'horizontal', 'parallel', 'perpendicular', 'curved'
/// -	'text-position': see VipPieItem::setTextPosition(), one of 'inside', 'outside', 'automatic'
/// -	'text-direction': see VipPieItem::setTextDirection(), one of 'inside', 'outside', 'automatic'
/// -	'text-inner-distance-to-border': see VipPieItem::setTextInnerDistanceToBorder()
/// -	'text-inner-distance-to-border-relative': boolean value telling if 'text-inner-distance-to-border' should be interpreted as a relative or absolute distance
/// -	'text-outer-distance-to-border': see VipPieItem::setTextOuterDistanceToBorder()
/// -	'text-outer-distance-to-border-relative': boolean value telling if 'text-outer-distance-to-border' should be interpreted as a relative or absolute distance
/// -	'text-angle-position': see VipPieItem::setTextAnglePosition()
/// -	'spacing': see VipPieItem::setSpacing()
/// -	'to-text-border': pen used to draw the path from the pie to outer text
///
class VIP_PLOTTING_EXPORT VipPieItem : public VipAbstractPieItem
{
	Q_OBJECT

public:
	/// @brief Define the way item legend is drawn
	enum LegendStyle
	{
		/// @brief Draw using the pie item background brush and border pen
		BackgroundAndBorder,
		/// @brief Draw using the pie item background brush only
		BackgroundOnly,
		/// @brief Draw using the pie item background brush and a cosmetic pen with the color of the parent QWidget text color (usually black)
		BackgroundAndDefaultPen
	};

	VipPieItem(const VipText& title = VipText());
	virtual ~VipPieItem();

	/// @brief Reimplemented from VipPlotItem
	virtual QPainterPath shape() const;
	/// @brief Reimplemented from VipPlotItem
	virtual QRectF boundingRect() const;
	/// @brief Reimplemented from VipPlotItem
	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	/// @brief Set the item's value, or a NaN value to tell that this item does not have a value.
	/// Any '#value' substring within the item text will be replaced by the set value.
	/// @param value possibly NaN value
	void setValue(double value);
	double value() const;

	/// @brief Set the text to be displayed inside or outside this item.
	/// Any '#value' substring within the item text will be replaced by the set value.
	void setText(const VipText&);
	const VipText& text() const;

	/// @brief Reimplemented from VipPlotItem, in order to be responsive to stylesheets.
	virtual void setTextStyle(const VipTextStyle&);
	virtual VipTextStyle textStyle() const { return text().textStyle(); }

	/// @brief Reimplemented from VipPlotItem, return the background color
	virtual QColor majorColor() const { return boxStyle().backgroundBrush().color(); }
	/// @brief Reimplemented from VipPlotItem, set the color of all pens and brushes
	virtual void setMajorColor(const QColor& c)
	{
		boxStyle().setColor(c);
		quiverPath().setColor(c);
	}
	/// @brief Reimplemented from VipPlotItem, set all pens
	virtual void setPen(const QPen& p)
	{
		boxStyle().setBorderPen(p);
		quiverPath().setPen(p);
		quiverPath().setExtremityPen(VipQuiverPath::Start, p);
		quiverPath().setExtremityPen(VipQuiverPath::End, p);
	}
	virtual QPen pen() const { return boxStyle().borderPen(); }
	/// @brief Reimplemented from VipPlotItem, set the background brush
	virtual void setBrush(const QBrush& b) { boxStyle().setBackgroundBrush(b); }
	virtual QBrush brush() const { return boxStyle().backgroundBrush(); }

	/// @brief Reimplemented from VipPlotItem, add the '#value' keyword
	virtual QString formatText(const QString& str, const QPointF& pos) const;

	/// @brief Set the legend style
	void setLegendStyle(LegendStyle);
	LegendStyle legendStyle() const;

	/// @brief Set the parameters used to draw the line between the pie item and its text.
	/// Note that this line is only drawn when the text is outside the pie.
	void setQuiverPath(const VipQuiverPath&);
	const VipQuiverPath& quiverPath() const;
	VipQuiverPath& quiverPath();

	/// @brief Clip the pie drawing to its background path.
	/// This is usefull when drawing multiple VipPieItem with a large pen width in order to avoid drawing above each other.
	void setClipToPie(bool);
	bool clipToPie() const;

	/// @brief Set the text transform: horizontall, perpendicular to the polar axis, parallel to the polar axis, or curved along the polar axis.
	void setTextTransform(VipAbstractScaleDraw::TextTransform);
	VipAbstractScaleDraw::TextTransform textTransform() const;

	/// @brief Set the text position: inside or outside the pie, or automatically computed (inside if possible, outside otherwise)
	void setTextPosition(VipAbstractScaleDraw::TextPosition);
	VipAbstractScaleDraw::TextPosition textPosition() const;

	/// @brief Set the text direction for parallel and curved text transform
	void setTextDirection(VipText::TextDirection dir);
	VipText::TextDirection textDirection() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextAdditionalTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	QTransform textAdditionalTransform() const;
	QPointF textAdditionalTransformReference() const;

	/// @brief Set the text distance from the outer border of the pie. This applies for inner text drawing.
	/// The distance can be given in item's coordinate or in relative value to the pie radius extent.
	void setTextInnerDistanceToBorder(double dist, Vip::ValueType d = Vip::Absolute);
	double textInnerDistanceToBorder() const;
	Vip::ValueType innerDistanceToBorder() const;

	/// @brief Set the text distance from the outer border of the pie. This applies for outer text drawing.
	/// The distance can be given in item's coordinate or in relative value to the pie radius extent.
	void setTextOuterDistanceToBorder(double dist, Vip::ValueType d = Vip::Absolute);
	double textOuterDistanceToBorder() const;
	Vip::ValueType outerDistanceToBorder() const;

	/// @brief Set an additional horizontal distance to define the text drawing position.
	/// This only applies to horizontal text transformation.
	void setTextHorizontalDistance(double dist);
	double textHorizontalDistance() const;

	/// @brief Set the text position in polar coordinate by giving a relative distance to the pie sweep length.
	/// For instance, to draw the text in the middle of the pie, use 0.5 (default). To draw the text at the start angle, use 0.
	void setTextAnglePosition(double normalized_angle);
	double textAnglePosition() const;

	// Set pie spacing: remove spacing length from pie left and right borders
	void setSpacing(double);
	double spacing() const;

	/// @brief Returns the text object to be drawn
	const VipTextObject& textObject() const;
	/// @brief Returns the pie paths to be drawn (background and border)
	QPair<QPainterPath, QPainterPath> piePath() const;
	/// @brief Returns the polyline from the pie to the text to be drawn
	const QPolygonF& polyline() const;

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QList<VipText> legendNames() const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private:
	void recomputeItem(const VipCoordinateSystemPtr&, VipAbstractScaleDraw::TextPosition);
	void recomputeItem(const VipCoordinateSystemPtr&);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipColorPalette;

/// @brief Class reprenting a pie chart.
///
/// VipPieChart is used to plot a pie chart, where each pie of the chart is internally represented as a VipPieItem.
/// VipPieChart is a VipPlotItemComposite using by default the VipPlotItemComposite::UniqueItem mode. VipPlotItemComposite::Aggregate is also
/// supported if you wish to manipulate (select, hide,...) individual VipPieItem items.
///
/// A VipPieChart organizes its pies within its bounding pie set with VipPieChart::setPie().
/// Individual pies are created using the function VipPieChart::setValues(). Within the Individual bounding pie,
/// each item takes as much angular space as its ratio to the sum value of all items.
///
/// VipPieChart provides the same functions as VipPieItem to control the drawing style of its pies.
/// Note that each individual pie item can be directly manipulated using VipPieItem::pieItemAt().
///
/// VipPieChart supports stylesheets and adds the following attributes:
/// -	'legend-style': see VipPieChart::setLegendStyle(), one of 'backgroundAndBorder', 'backgroundOnly', 'backgroundAndDefaultPen'
/// -	'clip-to-pie': boolean value, see VipPieChart::setClipToPie()
/// -	'text-transform': see VipPieChart::setTextTransform(), one of 'horizontal', 'parallel', 'perpendicular', 'curved'
/// -	'text-position': see VipPieChart::setTextPosition(), one of 'inside', 'outside', 'automatic'
/// -	'text-direction': see VipPieChart::setTextDirection(), one of 'inside', 'outside', 'automatic'
/// -	'text-inner-distance-to-border': see VipPieChart::setTextInnerDistanceToBorder()
/// -	'text-inner-distance-to-border-relative': boolean value telling if 'text-inner-distance-to-border' should be interpreted as a relative or absolute distance
/// -	'text-outer-distance-to-border': see VipPieChart::setTextOuterDistanceToBorder()
/// -	'text-outer-distance-to-border-relative': boolean value telling if 'text-outer-distance-to-border' should be interpreted as a relative or absolute distance
/// -	'text-angle-position': see VipPieChart::setTextAnglePosition()
/// -	'spacing': see VipPieChart::setSpacing()
/// -	'to-text-border': pen used to draw the path from the pie to outer text
///
///
/// Code example:
///
/// \code{.cpp}
///
/// #include "VipPolarAxis.h"
/// #include "VipPieChart.h"
/// #include "VipPlotGrid.h"
/// #include "VipPlotWidget2D.h"
/// #include <qapplication.h>
///
/// int main(int argc, char** argv)
/// {
/// QApplication app(argc, argv);
///
/// VipPlotPolarWidget2D w;
///
/// // set title and show title axis
/// w.area()->setTitle("<b>Countries by Proportion of World Population</b>");
/// w.area()->titleAxis()->setVisible(true);
///
/// // Invert polar scale (not mandatory)
/// w.area()->polarAxis()->setScaleInverted(true);
///
/// // hide grid
/// w.area()->grid()->setVisible(false);
///
/// // get axes as a list
/// QList<VipAbstractScale*> scales;
/// w.area()->standardScales(scales);
/// // hide axes
/// scales[0]->setVisible(false);
/// scales[1]->setVisible(false);
///
/// // create pie chart
/// VipPieChart* ch = new VipPieChart();
///
/// // Set the pie chart bounding pie in axis coordinates
/// ch->setPie(VipPie(0, 100, 20, 100));
///
/// // set the pen width for all items
/// auto bs = ch -> itemsBoxStyle();
/// bs.borderPen().setWidthF(3);
/// ch->setItemsBoxStyle(bs);
///
/// // set the pen color palette to always return white color
/// ch->setPenColorPalette(VipColorPalette(Qt::white));
///
/// // legend only draws the item background
/// ch->setLegendStyle(VipPieItem::BackgroundOnly);
///
/// // clip item drawinf to its pie, not mandatory
/// ch->setClipToPie(true);
///
/// // Set items text to be displayed inside each pie, combination of its title and value
/// ch->setText("#title\n#value%.2f");
///
/// // set the values
/// ch->setValues(QVector<double>() << 18.47 << 17.86 << 4.34 << 3.51 << 2.81 << 2.62 << 2.55 << 2.19 << 1.91 << 1.73 << 1.68 << 40.32);
/// // set the item's titles
/// ch->setTitles(QVector<VipText>() << "China"
/// 				 << "India"
/// 				 << "U.S"
/// 				 << "Indonesia"
/// 				 << "Brazil"
/// 				 << "Pakistan"
/// 				 << "Nigeria"
/// 				 << "Bangladesh"
/// 				 << "Russia"
/// 				 << "Mexico"
/// 				 << "Japan"
/// 				 << "Other");
///
/// // set the pie chart axes
/// ch->setAxes(scales, VipCoordinateSystem::Polar);
///
/// // highlight the highest country by giving it an offset to the center
/// VipPie pie = ch->pieItemAt(0)->rawData();
/// ch->pieItemAt(0)->setRawData(pie.setOffsetToCenter(10));
///
/// w.show();
/// return app.exec();
/// }
///
/// \endcode
///
class VIP_PLOTTING_EXPORT VipPieChart : public VipPlotItemComposite
{
	Q_OBJECT

public:
	VipPieChart(const VipText& title = VipText());
	virtual ~VipPieChart();

	void setPie(const VipPie&);
	const VipPie& pie() const;

	/// @brief Set the legend style
	void setLegendStyle(VipPieItem::LegendStyle);
	VipPieItem::LegendStyle legendStyle() const;

	/// @brief Reimplemented from VipPlotItem, return the background color
	virtual QColor majorColor() const
	{
		// return boxStyle().backgroundBrush().color();
		return QColor();
	}
	/// @brief Reimplemented from VipPlotItem, set the color of all pens and brushes
	virtual void setMajorColor(const QColor&)
	{
		// boxStyle().setColor(c);
	}
	/// @brief Reimplemented from VipPlotItem, set all pens
	virtual void setPen(const QPen& p)
	{
		VipBoxStyle st = itemsBoxStyle();
		st.setBorderPen(p);
		setItemsBoxStyle(st);
	}
	virtual QPen pen() const { return itemsBoxStyle().borderPen(); }
	/// @brief Reimplemented from VipPlotItem, set the background brush
	virtual void setBrush(const QBrush& b)
	{
		VipBoxStyle st = itemsBoxStyle();
		st.setBackgroundBrush(b);
		setItemsBoxStyle(st);
	}
	virtual QBrush brush() const { return itemsBoxStyle().backgroundBrush(); }

	virtual QRectF boundingRect() const;
	virtual QPainterPath shape() const;
	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;

	void setValues(const QVector<double>& values, const QVector<VipText>& titles = QVector<VipText>());
	void setTitles(const QVector<VipText>& titles);
	const QVector<double>& values() const;
	const QVector<VipText>& titles() const;

	/// @brief Set the VipQuiverPath used to draw the line between outside text and the pie external boundary.
	/// By default, VipPieChart will try to use the pen color used to draw axes backbones, unless VIP_CUSTOM_BACKBONE_TICKS_COLOR property is set.
	void setQuiverPath(const VipQuiverPath&);
	const VipQuiverPath& quiverPath() const;

	/// @brief Clip the pie drawing to its background path.
	/// This is usefull when drawing multiple VipPieItem with a large pen width in order to avoid drawing above each other.
	void setClipToPie(bool);
	bool clipToPie() const;

	void setSpacing(double);
	double spacing() const;

	void setTextTransform(VipAbstractScaleDraw::TextTransform);
	VipAbstractScaleDraw::TextTransform textTransform() const;

	void setTextPosition(VipAbstractScaleDraw::TextPosition);
	VipAbstractScaleDraw::TextPosition textPosition() const;

	/// @brief Set the text direction for parallel and curved text transform
	void setTextDirection(VipText::TextDirection dir);
	VipText::TextDirection textDirection() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextAdditionalTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	QTransform textAdditionalTransform() const;
	QPointF textAdditionalTransformReference() const;

	void setTextHorizontalDistance(double dist);
	double textHorizontalDistance() const;

	void setTextAnglePosition(double normalized_angle);
	double textAnglePosition() const;

	/// @brief Set the text to be display inside/outside each pie.
	/// By default, VipPieChart will use the area axes label color, unless VIP_CUSTOM_LABEL_PEN property is set.
	void setText(const VipText& t);
	VipText text() const;

	/// @brief Set the color palette used to fill each pie.
	void setBrushColorPalette(const VipColorPalette&);
	const VipColorPalette& brushColorPalette() const;

	/// @brief Set the color palette used to draw the border of each pie.
	void setPenColorPalette(const VipColorPalette&);
	const VipColorPalette& penColorPalette() const;

	/// @brief Set the color palette used for both border and filling of each pie
	virtual void setColorPalette(const VipColorPalette&);
	virtual VipColorPalette colorPalette() const { return brushColorPalette(); }

	/// @brief Reimplemented from VipPlotItem, in order to be responsive to stylesheets.
	virtual void setTextStyle(const VipTextStyle&);
	virtual VipTextStyle textStyle() const;

	/// @brief Set the text distance from the outer border of the pie. This applies for inner text drawing.
	/// The distance can be given in item's coordinate or in relative value to the pie radius extent.
	void setTextInnerDistanceToBorder(double dist, Vip::ValueType d = Vip::Absolute);
	double textInnerDistanceToBorder() const;
	Vip::ValueType innerDistanceToBorder() const;

	/// @brief Set the text distance from the outer border of the pie. This applies for outer text drawing.
	/// The distance can be given in item's coordinate or in relative value to the pie radius extent.
	void setTextOuterDistanceToBorder(double dist, Vip::ValueType d = Vip::Absolute);
	double textOuterDistanceToBorder() const;
	Vip::ValueType outerDistanceToBorder() const;

	void setItemsBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& itemsBoxStyle() const;

	VipPieItem* pieItemAt(int index) { return static_cast<VipPieItem*>(this->at(index)); }
	const VipPieItem* pieItemAt(int index) const { return static_cast<const VipPieItem*>(this->at(index)); }
	using VipPlotItemComposite::count;

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private:
	VipPieItem* createItem(int index);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
// end Plotting
#endif
