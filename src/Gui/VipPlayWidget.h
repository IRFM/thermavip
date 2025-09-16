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

#ifndef VIP_PLAY_WIDGET_H
#define VIP_PLAY_WIDGET_H

#include "VipPlayer.h"
#include "VipPlotMarker.h"
#include "VipPlotShape.h"
#include "VipPlotWidget2D.h"
#include "VipResizeItem.h"
#include "VipSliderGrip.h"
#include "VipTimestamping.h"

/// \addtogroup Gui
/// @{

class VipIODevice;
class VipProcessingPool;
class VipTimeRangeListItem;


/// @brief Plot item representing a time range.
///
/// VipTimeRangeItem supports mouse interaction in order to move or resize the time range.
/// Therefore, initialTimeRange() returns the initial time range before any user interaction,
/// and currentTimeRange returns the time range after potential user interactions.
/// 
class VIP_GUI_EXPORT VipTimeRangeItem : public VipPlotItem
{
	Q_OBJECT

public:
	VipTimeRangeItem(VipTimeRangeListItem* item);
	~VipTimeRangeItem();

	virtual QRectF boundingRect() const;
	virtual QPainterPath shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const;

	void setInitialTimeRange(const VipTimeRange&);
	VipTimeRange initialTimeRange() const;

	void setCurrentTimeRange(const VipTimeRange&);
	void setCurrentTimeRange(qint64 left, qint64 right);
	VipTimeRange currentTimeRange();
	qint64 left() const;
	qint64 right() const;

	VipTimeRangeListItem* parentItem() const;

	void setHeights(double start, double end);
	QPair<double, double> heights() const;

	void setColor(const QColor&);
	QColor color() const;

	virtual void setPen(const QPen&) {}
	virtual QPen pen() const { return QPen(); }
	virtual void setBrush(const QBrush& b) { setColor(b.color()); }
	virtual QBrush brush() const { return QBrush(color()); }

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void drawSelected(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;
	virtual bool applyTransform(const QTransform&);

	bool reverse() const;

public Q_SLOTS:
	void setReverse(bool);

Q_SIGNALS:
	void timeRangeChanged();

protected:
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

private:
	int selection(const QPointF& pos) const;
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_METATYPE(VipTimeRangeItem*)
typedef QList<VipTimeRangeItem*> VipTimeRangeItemList;
Q_DECLARE_METATYPE(VipTimeRangeItemList)

/// Dispatcher used to create a list of VipTimeRangeItem based on a VipIODevice and a parent VipTimeRangeListItem.
/// Signature:
/// VipTimeRangeItemList (VipIODevice*, VipTimeRangeListItem*);
VIP_GUI_EXPORT VipFunctionDispatcher<2>& vipCreateTimeRangeItemsDispatcher();

class VipPlayerArea;
class VIP_GUI_EXPORT VipTimeRangeListItem : public VipPlotItem
{
	Q_OBJECT
	friend class VipTimeRangeItem;

public:
	enum State // visibility state
	{
		Visible = 0,		       // the item is visible
		HiddenForPlayer = 0x01,	       // the item is hidden because its player is hidden
		HiddenForHideTimeRanges = 0x02 // the item is hidden because we don't want to see the time ranges
	};
	enum DrawComponent
	{
		None = 0,
		Text = 0x01,
		MovingArea = 0x02,
		ResizeArea = 0x04,
		All = Text | MovingArea | ResizeArea
	};
	Q_DECLARE_FLAGS(DrawComponents, DrawComponent);

	typedef std::function<void(const VipTimeRangeListItem*, QPainter*, const VipCoordinateSystemPtr&)> draw_function;
	typedef std::function<void(const VipTimeRangeListItem*, const VipTimestampingFilter&)> change_time_range_function;
	typedef std::function<qint64(const VipTimeRangeListItem*, qint64)> closest_time_function;

	VipTimeRangeListItem(VipPlayerArea*);
	~VipTimeRangeListItem();

	void setDrawComponents(DrawComponents);
	void setDrawComponent(DrawComponent, bool on = true);
	bool testDrawComponent(DrawComponent) const;
	DrawComponents drawComponents() const;

	void setStates(int);
	int states() const;

	virtual QPainterPath shape() const;
	virtual QRectF boundingRect() const;

	void setDevice(VipIODevice*);
	VipIODevice* device() const;

	const QList<VipTimeRangeItem*> &items() const;
	QList<qint64> stops() const;
	VipTimeRangeTransforms transforms() const;

	void setHeights(double start, double end);
	QPair<double, double> heights() const;

	void setColor(const QColor&);
	QColor color() const;

	void setAdditionalDrawFunction(draw_function);
	draw_function additionalDrawFunction() const;

	void setChangeTimeRangeFunction(change_time_range_function);
	change_time_range_function changeTimeRangeFunction() const;

	void setClosestTimeFunction(closest_time_function);
	closest_time_function closestTimeFunction() const;
	qint64 closestTime(qint64) const;

	virtual void setPen(const QPen&) {}
	virtual QPen pen() const { return QPen(); }
	virtual void setBrush(const QBrush& b) { setColor(b.color()); }
	virtual QBrush brush() const { return QBrush(color()); }
	virtual void setMajorColor(const QColor& c) { setColor(c); }

	VipTimeRangeItem* find(qint64 time) const;
	void split(VipTimeRangeItem* item, qint64 time);

	QRectF itemsBoundingRect() const;
	QPair<qint64, qint64> itemsRange() const;

	VipPlayerArea* area() const;

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void drawSelected(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;
	bool applyTransform(const QTransform& tr);
	void updateDevice(bool reload);
public Q_SLOTS:

	// update the VipIODevice timestamping filter
	void updateDevice();
	void reset();
	// Compute the item tool tip text
	void computeToolTip();
private Q_SLOTS:
	void deviceTimestampingChanged();

Q_SIGNALS:
	/// Emitted whenever a VipTimeRangeItem is selected or unselected
	void itemSelectionChanged(VipTimeRangeItem*);
	/// Emitted whenever one or more VipTimeRangeItem have their time range changed
	void itemsTimeRangeChanged(const VipTimeRangeItemList&);

protected:
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

private:
	void addItem(VipTimeRangeItem*);
	void removeItem(VipTimeRangeItem*);
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};
Q_DECLARE_OPERATORS_FOR_FLAGS(VipTimeRangeListItem::DrawComponents)

class VIP_GUI_EXPORT VipPlayerArea : public VipPlotArea2D
{
	Q_OBJECT
public:
	VipPlayerArea();
	~VipPlayerArea();

	void setProcessingPool(VipProcessingPool*);
	VipProcessingPool* processingPool() const;

	VipSliderGrip* timeSliderGrip() const;
	VipPlotMarker* timeMarker() const;

	VipSliderGrip* limit1SliderGrip() const;
	VipPlotMarker* limit1Marker() const;

	VipSliderGrip* limit2SliderGrip() const;
	VipPlotMarker* limit2Marker() const;

	VipAxisBase* timeScale() const;

	/// Return all VipTimeRangeItem stops (start and end), except for given VipTimeRangeItem.
	QList<qint64> stops(const QList<VipTimeRangeItem*>& excluded = QList<VipTimeRangeItem*>()) const;

	QList<VipTimeRangeListItem*> timeRangeListItems() const;
	VipTimeRangeListItem* findItem(VipIODevice* device) const;
	void timeRangeItems(QList<VipTimeRangeItem*>& selected, QList<VipTimeRangeItem*>& not_selected) const;
	void timeRangeListItems(QList<VipTimeRangeListItem*>& selected, QList<VipTimeRangeListItem*>& not_selected) const;

	void setTimeRangeVisible(bool visible);
	bool timeRangeVisible() const;

	bool limitsEnabled() const;
	bool timeRangesLocked() const;

	int visibleItemCount() const;

	void setSelectionTimeRange(const VipTimeRange& r);
	VipTimeRange selectionTimeRange() const;

	void setTimeRangeSelectionBrush(const QBrush&);
	QBrush timeRangeSelectionBrush() const;

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

public Q_SLOTS:

	void setTime(double);
	void setTime64(qint64 t);
	void setLimit1(double);
	void setLimit2(double);
	void setLimitsEnable(bool);
	void setTimeRangesLocked(bool locked);

	void moveToForeground();
	void moveToBackground();
	void splitSelection();
	void reverseSelection();
	void resetSelection();
	void resetAllTimeRanges();
	void alignToZero(bool enable);
	void computeStartDate();

	void updateArea(bool check_item_visibility);
	void updateAreaDevices();
	void defferedUpdateAreaDevices();
	void updateProcessingPool();
	void defferedUpdateProcessingPool();
	void addMissingDevices();

private Q_SLOTS:

	// void deviceAdded(QObject*);
	// void deviceRemoved(QObject*);
	void mouseMoved(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseReleased(VipPlotItem*, VipPlotItem::MouseButton);

Q_SIGNALS:
	void processingPoolChanged(VipProcessingPool*);
	void devicesChanged();

private:
	// int findBestColor(VipTimeRangeListItem *);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipValueToTime;
class VIP_GUI_EXPORT VipPlayWidget : public QFrame
{
	Q_OBJECT
	Q_PROPERTY(QColor sliderColor READ sliderColor WRITE setSliderColor)
	Q_PROPERTY(QColor sliderFillColor READ sliderFillColor WRITE setSliderFillColor)
public:
	typedef VipValueToTime::TimeType (*function_type)(VipPlayWidget*);

	VipPlayWidget(QWidget* parent = nullptr);
	~VipPlayWidget();

	QColor sliderColor() const;
	void setSliderColor(const QColor&);

	QColor sliderFillColor() const;
	void setSliderFillColor(const QColor&);

	VipPlayerArea* area() const;
	VipValueToTime* valueToTime() const;

	void setProcessingPool(VipProcessingPool*);
	VipProcessingPool* processingPool() const;

	void setTimeType(VipValueToTime::TimeType);
	VipValueToTime::TimeType timeType() const;

	bool timeRangeVisible() const;
	virtual QSize sizeHint() const;

	void setPlayWidgetHidden(bool hidden);
	bool playWidgetHidden() const;

	bool isAutoScale() const;
	bool isAlignedToZero() const;
	bool isLimitsEnabled() const;
	bool isMaxSpeed() const;
	double playSpeed() const;
	bool timeRangesLocked() const;

	static void setTimeUnitFunction(function_type fun);
	static function_type timeUnitFunction();

public Q_SLOTS:
	void setLimitsEnabled(bool);
	void setTimeRangeVisible(bool visible);
	void setAutoScale(bool);
	void setAlignedToZero(bool);
	void setLimitsEnable(bool);
	void setMaxSpeed(bool);
	void setPlaySpeed(double);
	void disableAutoScale();
	void updatePlayer();
	void setTimeRangesLocked(bool locked);

private Q_SLOTS:

	void timeEdited();
	void selectionClicked(bool);

	void playForward();
	void playBackward();
	void timeChanged();
	void timeUnitChanged();
	void deviceAdded();
	void resetAllTimeRanges();
	void updatePlayerInternal();

	void selectionItem();
	void selectionItemArea();
	void selectionZoomArea();
	void toolTipChanged();

protected:
	virtual void showEvent(QShowEvent* evt);

Q_SIGNALS:
	void valueToTimeChanged(VipValueToTime*);

private:
	void toolTipFlagsChanged(VipToolTip::DisplayFlags flags);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlayWidget*)

VIP_GUI_EXPORT VipArchive& operator<<(VipArchive&, VipPlayWidget*);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive&, VipPlayWidget*);

/// @}
// end Gui

#endif
