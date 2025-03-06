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

#ifndef VIP_ABSTRACT_SCALE_H
#define VIP_ABSTRACT_SCALE_H

#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QMap>
#include <QPainter>
#include <QSet>

#include "VipArchive.h"
#include "VipPlotItem.h"
#include "VipRenderObject.h"
#include "VipScaleDiv.h"

/// \addtogroup Plotting
/// @{

class VipScaleEngine;
class VipAbstractScaleDraw;
class VipValueTransform;
class VipBoxStyle;
class VipPlotItem;
class VipAbstractPlotArea;

/// VipBoxGraphicsWidget is a QGraphicsWidget that draws its content using a VipBoxStyle.
/// It is the base class of VipAbstractScale.
///
/// VipBoxGraphicsWidget support stylesheets and add the following elements:
/// -	'border': item's border pen or color, like 'red' or '1px solid green' or '1.5px dash rgb(120,120,30)'
/// -	'border-width': item's border width. Can also be specified with 'border' property.
/// -	'border-radius': corner radius, floating point property
/// -	'background': background color, like 'white' or 'rgb(120,120,30)'
///
class VIP_PLOTTING_EXPORT VipBoxGraphicsWidget
  : public QOpenGLGraphicsWidget
  , public VipPaintItem
  , public VipRenderObject
{
	Q_OBJECT
	friend class VipAbstractPlotArea;

public:
	VipBoxGraphicsWidget(QGraphicsItem* parent = nullptr);
	virtual ~VipBoxGraphicsWidget();

	/// Returns the #VipBoxStyle
	VipBoxStyle& boxStyle();
	/// Returns the #VipBoxStyle
	const VipBoxStyle& boxStyle() const;
	/// Set the #VipBoxStyle
	void setBoxStyle(const VipBoxStyle&);

	/// @brief Returns the lowest most VipAbstractPlotArea parent (if any), or null
	VipAbstractPlotArea* area() const;

	virtual void draw(QPainter* painter, QWidget* = 0);

public Q_SLOTS:
	void update();
	virtual void setGeometry(const QRectF& rect) override;
private Q_SLOTS:
	void updateInternal() { QGraphicsWidget::update(); }

protected:
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

	// Reimplement to detect hover events and selection status changes in order to reapply style sheet
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief VipAbstractScale is the base abstract class for all scales.
///
/// VipAbstractScale supports stylesheets and adds the following properties:
/// -	'auto-scale': boolean value like 'true' or 'false' to enable/disable automatic scaling
/// -	'label-color': labels text color
/// -	'label-font': labels font
/// -	'label-font-size': labels font size
/// -	'label-font-style': labels font style
/// -	'label-font-weight': labels font weight
/// -	'label-font-family': labels font familly
/// -	'label-text-border': labels text box border pen
/// -	'label-text-border-radius': labels text box border radius
/// -	'label-text-background': labels text box background
/// -	'label-text-border-margin': labels text box border margin
/// -	'display[]': boolean value telling which element to display, like 'display["backbone"]', 'display["ticks"]', display["labels"] or display["title"]
/// -	'pen': backbone and ticks pen
/// -	'pen-color': backbone and ticks pen color
/// -	'margin': floating point value equivalent to VipAbstractScale::setMargin()
/// -	'spacing': floating point value equivalent to VipAbstractScale::setSpacing()
/// -	'inverted': boolean value equivalent to VipAbstractScale::setScaleInverted()
/// -	'label-position': equivalent to VipAbstractScale::scaleDraw()->setTextPosition(). Example: 'label-position: inside' or 'label-position: outside'
/// -	'ticks-position': equivalent to VipAbstractScale::scaleDraw()->setTicksPosition(). Example: 'ticks-position: inside' or 'ticks-position: outside'
/// -	'tick-length[]': tick length like 'tick-length["major"]:10', 'tick-length["medium"]:7' or 'tick-length["minor"]:4'
/// -	'label-transform:  equivalent to VipAbstractScale::scaleDraw()->setTextTransform(). Example: 'label-transform: horizontal'. Possible values: horizontal, perpendicular, parallel, curved.
///
class VIP_PLOTTING_EXPORT VipAbstractScale : public VipBoxGraphicsWidget
{
	Q_OBJECT

	friend class VipPlotItem;

public:
	VipAbstractScale(QGraphicsItem* parent = nullptr);
	virtual ~VipAbstractScale();

	/// @brief Compute the scale layout (this does not change the item's geometry).
	virtual void layoutScale() = 0;
	/// @brief Recompute the scale div based on all VipPlotItem using this axis
	virtual void computeScaleDiv();

	/// @brief Returns true is automatic scaling is enabled.
	/// If enabled, the axis scale div will automatically be updated based on VipPlotItem using this axis and using the VipPlotItem::AutoScale attribute.
	bool isAutoScale() const;

	/// @brief Optimize auto scaling for streaming, when a lot of scale changes are necessary.
	/// If enabled, the scale will automatically detect when streaming is enabled and modifiy the autoscaling strategy
	/// to avoid scale flickering.
	void setOptimizeFromStreaming(bool enable, double factor = Vip::InvalidValue);
	bool optimizeForStreaming() const;

	/// @brief Set the extension factor when computing items interval.
	/// In autoscaling we might want the items to display additional space around the item for better visualization.
	/// This could be done through this factor that extends the items interval by a factor of f*(interval_max - interval_min).
	/// Usually, a factor of 0.1 adds enough space to the top/bottom or left/right parts of a curve or other item.
	/// Default to 0.
	///
	/// Reimplementations must call the base class member to ensure builtin integrity.
	virtual void setItemIntervalFactor(double f);
	double itemIntervalFactor() const;

	/// @brief Set axis title, displayed below the axis labels
	virtual void setTitle(const VipText& title);
	virtual void clearTitle();

	/// @brief Set the text style used for given tick
	virtual void setTextStyle(const VipTextStyle& p, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick);
	const VipTextStyle& textStyle(VipScaleDiv::TickType = VipScaleDiv::MajorTick) const;

	/// @brief Set an additional transform used to draw the labels of given tick
	void setLabelTransform(const QTransform& tr, VipScaleDiv::TickType tick = VipScaleDiv::MajorTick);
	QTransform labelTransform(VipScaleDiv::TickType = VipScaleDiv::MajorTick) const;

	/// @brief Returns true is title drawing is enabled
	bool isDrawTitleEnabled() const;

	/// @brief Specify distances of the scale's endpoints from the widget's borders.
	/// The actual borders will never be less than minimum border distance.
	virtual void setBorderDist(double start, double end);
	double startBorderDist() const;
	double endBorderDist() const;

	/// @brief Calculate a hint for the border distances.
	///
	/// This member function calculates the distance
	/// of the scale's endpoints from the widget borders which
	/// is required for the mark labels to fit into the widget.
	/// The maximum of this vipDistance an the minimum border vipDistance
	/// is returned.
	///
	/// The minimum border vipDistance depends on the font.
	virtual void getBorderDistHint(double& start, double& end) const;

	/// @brief Set a minimum value for the distances of the scale's endpoints from
	/// the widget borders. This is useful to avoid that the scales
	/// are "jumping", when the tick labels or their positions change often.
	virtual void setMinBorderDist(double start, double end);
	double startMinBorderDist() const;
	double endMinBorderDist() const;
	void getMinBorderDist(double& start, double& end) const;

	/// @brief Set a maximum value for the distances of the scale's endpoints from
	/// the widget borders. This is useful to avoid that the scales
	/// are "jumping", when the tick labels or their positions change often.
	void getMaxBorderDist(double& start, double& end) const;
	double startMaxBorderDist() const;
	double endMaxBorderDist() const;
	virtual void setMaxBorderDist(double start, double end);

	/// @brief Returns the margin to the colorBar/base line.
	double margin() const;
	/// @brief Returns the distance between color bar, scale and title
	double spacing() const;
	/// @brief Returns wether the scale values are inverted
	bool isScaleInverted() const;

	virtual void setScale(vip_double min, vip_double max, vip_double stepSize = 0);
	virtual void setMaxMajor(int maxMajor);
	virtual void setMaxMinor(int maxMinor);
	int maxMajor() const;
	int maxMinor() const;

	/// @brief Set the transformation
	virtual void setTransformation(VipValueTransform*);
	const VipValueTransform* transformation() const;

	/// @brief Set the scale engine
	virtual void setScaleEngine(VipScaleEngine*);
	VipScaleEngine* scaleEngine() const;

	/// @brief Assign a scale division
	/// The scale division determines where to set the tick marks.
	virtual void setScaleDiv(const VipScaleDiv& sd, bool force_check_geometry = false, bool disable_scale_signal = false);
	void setScaleDiv(const VipInterval& bounds, const VipScaleDiv::TickList& majorTicks);
	const VipScaleDiv& scaleDiv() const;

	/// @brief Set a scale draw
	/// scaleDraw has to be created with new and will be deleted in
	/// ~VipAxisBase() or the next call of setScaleDraw().
	/// scaleDraw will be initialized with the attributes of
	/// the previous scaleDraw object.
	virtual void setScaleDraw(VipAbstractScaleDraw*);
	virtual const VipAbstractScaleDraw* constScaleDraw() const;
	virtual VipAbstractScaleDraw* scaleDraw();
	const VipAbstractScaleDraw* scaleDraw() const { return constScaleDraw(); }
	/// @brief Returns true if at least one VipPlotItem using this axis has a unit for this axis
	bool hasUnit() const;
	/// @brief Returns true if no VipPlotItem using this axis has a unit for this axis, excluding \a excluded
	bool hasNoUnit(VipPlotItem* excluded = nullptr) const;

	/// @brief Returns the position in item's coordinate of given axis value. See VipScaleDraw::position().
	QPointF position(vip_double value, double length = 0, Vip::ValueType type = Vip::Absolute) const;
	/// @brief Reutrns axis value for given item's coordinate
	vip_double value(const QPointF& position) const;
	/// @brief Convert a relative axis value to an absolute one, or conversly.
	double convert(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	double angle(vip_double value, Vip::ValueType type = Vip::Absolute) const;
	QPointF start() const;
	QPointF end() const;

	/// @brief Synchronize this scale div with another
	virtual void synchronizeWith(VipAbstractScale* other);
	QSet<VipAbstractScale*> synchronizedWith() const;
	void desynchronize(VipAbstractScale* other);

	virtual void draw(QPainter* painter, QWidget* = 0);

	/// @brief Given a list of scales, returns all independant scales (removing synchronized ones)
	template<class T>
	static QList<VipAbstractScale*> independentScales(const QList<T*> scales);

	/// @brief Returns the union interval of all visible VipPlotItem using this axis and having the property VipPlotItem::AutoScale
	VipInterval itemsInterval() const;

	/// @brief Returns all VipPlotItem using this axis
	const QList<VipPlotItem*>& plotItems() const;
	/// @brief Returns all VipPlotItem using an axis synchronized with this one
	QList<VipPlotItem*> synchronizedPlotItems() const;
	/// @brief Returns the parent QGraphicsView (if any)
	QGraphicsView* view() const;

	/// @brief Similar to QGraphicsObject::setCacheMode
	void setCacheMode(CacheMode mode, const QSize& logicalCacheSize = QSize());
	void prepareGeometryChange();

public Q_SLOTS:
	/// @brief Enable/disable automatic scaling
	virtual void setAutoScale(bool);
	void enableAutoScale() { setAutoScale(true); }
	void disableAutoScale() { setAutoScale(false); }
	virtual void setMargin(double);
	virtual void setSpacing(double td);
	virtual void setScaleInverted(bool invert);
	void enableDrawTitle(bool draw_title);
	/// @brief Triggers an update for all VipPlotItems using this axis or a synchronized one
	void updateItems();
	void desynchronize();

protected Q_SLOTS:
	virtual void emitScaleDivChanged(bool bounds_changed, bool emit_signal);
	virtual void emitScaleDivNeedUpdate();
	virtual void emitScaleNeedUpdate();
	virtual void emitGeometryNeedUpdate();

protected:
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*);
	virtual void updateOnStyleSheet();
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	void invalidateFullExtent();
	double cachedFullExtent() const;
	void setCachedFullExtent(double);
private Q_SLOTS:

	void synchronize();
	void delayedRecomputeScaleDiv();

Q_SIGNALS:

	/// This signal is emitted whene the scale div changed
	void scaleDivChanged(bool bounds_changed);

	/// This signal is emitted whenever the scale item need to be updated (after a call
	/// to #VipAbstractScale::setAxisMaxMajor() or #VipAbstractScale::setSpacing() for instance).
	/// Internal use only.
	void scaleNeedUpdate();
	/// @brief Emitted whenever the scale div needs to be updated, usually when a VipPlotItem with auto scaling changed.
	/// Internal use only.
	void scaleDivNeedUpdate();
	/// @brief Emitted whenever the scale geometry change.
	/// Internal use only
	void geometryNeedUpdate();

	/// @brief Emitted whenever a new VipPlotItem is attached to this axis
	void itemAdded(VipPlotItem*);
	/// @brief Emitted whenever a VipPlotItem is detached from this axis
	void itemRemoved(VipPlotItem*);
	/// @brief Emitted whenever the axis title changes
	void titleChanged(const VipText&);
	/// @brief Emitted whenever the autoscaling parameter of this axis changes
	void autoScaleChanged(bool);
	/// @brief Emitted whenever the visibility of this axis changes
	void visibilityChanged(bool);
	/// @brief Emitted whenever the selection status of this axis changes
	void selectionChanged(bool);

	void mouseButtonPress(VipAbstractScale*, VipPlotItem::MouseButton, double);
	void mouseButtonMove(VipAbstractScale*, VipPlotItem::MouseButton, double);
	void mouseButtonRelease(VipAbstractScale*, VipPlotItem::MouseButton, double);
	void mouseButtonDoubleClick(VipAbstractScale*, VipPlotItem::MouseButton, double);

protected:
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);
	virtual bool sceneEvent(QEvent* event);

public:
	// Internal use only

	static QRectF visualizedSceneRect(const QGraphicsView* view);
	static QTransform globalSceneTransform(const QGraphicsItem* item);
	static QTransform parentTransform(const QGraphicsItem* item);
	static QGraphicsView* view(const QGraphicsItem* item);
	static QList<VipInterval> scaleIntervals(const QList<VipAbstractScale*>& axes);
	static QList<VipPlotItem*> axisItems(const VipAbstractScale* x, const VipAbstractScale* y);

private:
	void addItem(VipPlotItem*);
	void removeItem(VipPlotItem*);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

template<class T>
QList<VipAbstractScale*> VipAbstractScale::independentScales(const QList<T*> scales)
{
	if (!scales.size())
		return QList<VipAbstractScale*>();

	// build the map of scale -> title
	// build the list of synchronize items
	QMap<VipAbstractScale*, VipAbstractScale*> valid_titles;
	QList<QSet<VipAbstractScale*>> synchronized_list;
	for (int i = 0; i < scales.size(); ++i) {
		QSet<VipAbstractScale*> syncs = scales[i]->synchronizedWith();
		syncs << scales[i];
		synchronized_list.append(syncs);

		// try to find a scale with a non empty title
		VipAbstractScale* valid = *syncs.begin();
		if (valid->title().isEmpty())
			for (QSet<VipAbstractScale*>::iterator it = syncs.begin(); it != syncs.end(); ++it)
				if (!(*it)->title().isEmpty()) {
					valid = (*it);
					break;
				}
		// affect this scale with title to all synchronized scales
		for (QSet<VipAbstractScale*>::iterator it = syncs.begin(); it != syncs.end(); ++it)
			valid_titles[*it] = valid;
	}

	QSet<VipAbstractScale*> synchro;
	QList<VipAbstractScale*> res;

	// add the first scale
	VipAbstractScale* scale = valid_titles[scales.first()];
	if (scales.indexOf(qobject_cast<T*>(scale)) >= 0)
		res << scale;
	else
		res << scales.first();
	synchro += (synchronized_list.first());

	// do the others
	for (int i = 1; i < scales.size(); ++i) {
		if (synchro.find(scales[i]) == synchro.end()) {
			VipAbstractScale* _scale = valid_titles[scales[i]];
			if (scales.indexOf(qobject_cast<T*>(_scale)) >= 0)
				res << _scale;
			else
				res << scales[i];
			synchro += synchronized_list[i];
		}
	}

	return res;
}

VIP_REGISTER_QOBJECT_METATYPE(VipAbstractScale*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipAbstractScale* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipAbstractScale* value);

/// Helper widget representing a unique vertical or horizontal axis
class VIP_PLOTTING_EXPORT VipScaleWidget : public QGraphicsView
{
	Q_OBJECT
	// These properties are mainly made for style sheet
	Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)

public:
	VipScaleWidget(VipAbstractScale* scale = nullptr, QWidget* parent = nullptr);
	~VipScaleWidget();

	virtual void setScale(VipAbstractScale* scale);
	VipAbstractScale* scale() const;

	QColor backgroundColor() const;

	bool hasBackgroundColor() const;

	void removeBackgroundColor();

public Q_SLOTS:

	void setBackgroundColor(const QColor& c);
	void recomputeGeometry();

protected:
	virtual void onResize(){};
	virtual void resizeEvent(QResizeEvent*);
	virtual void paintEvent(QPaintEvent*);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipScaleWidget*)

/// @}
// end Plotting

#endif
