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

#ifndef VIP_SLIDER_GIP_H
#define VIP_SLIDER_GIP_H

#include <QGraphicsObject>

#include "VipPlotItem.h"

/// \addtogroup Plotting
/// @{

class VipAbstractScale;
class VipAxisColorMap;
class QImage;

/// @brief Grip item attached to a scale
///
/// VipSliderGrip is a grip attached to a VipAbstractScale in order to define interactive sliders.
/// VipSliderGrip works with vertical/horizontal axes, as well as polar/radial axes.
///
/// VipSliderGrip displays an image (see VipSliderGrip::setImage()) as a grip. By default, a 'standard' image grip is provided.
/// Note that the provided image must point to the right. It will then be rotated to point toward the scale text when needed.
///
/// VipSliderGrip supports stylesheets and defines the following attributes:
/// -	'grip-always-inside-scale' : equivalent to setGripAlwaysInsideScale()
/// -	'single-step-enabled: equivalent to setSingleStepEnabled()
/// -	'single-step': first parameter of setSingleStep()
/// -	'single-step-reference': second parameter of setSingleStep()
/// -	'tooltip': tool tip text, equivalent to setToolTipText()
/// -	'tooltip-distance': equivalent to setToolTipDistance()
/// -	'display-tooltip-value': equivalent to setDisplayToolTipValue(), combination of 'left|top|right|bottom|center|vcenter|hcenter'
/// -	'handle-distance': equivalent to setHandleDistance()
/// -	'image': path to a valid handle image
///
/// in addition, VipSliderGrip defines the following selectors: 'left', 'right', 'bottom', 'top', 'radial', 'polar' depending on which type of scale it is attached to.
///
class VIP_PLOTTING_EXPORT VipSliderGrip
  : public QGraphicsObject
  , public VipPaintItem
{
	Q_OBJECT

public:
	VipSliderGrip(VipAbstractScale* parent);
	virtual ~VipSliderGrip();

	virtual QRectF boundingRect() const;

	/// @brief Attach the grip to a scale
	virtual void setScale(VipAbstractScale*);
	VipAbstractScale* scale();
	const VipAbstractScale* scale() const;

	/// @brief Returns the current grip value
	double value() const;

	void setMaxImageSize(const QSizeF&);
	QSizeF maxImageSize() const;

	/// Make sure that the grip is always visible and inside the current scale div (default to true)
	void setGripAlwaysInsideScale(bool);
	bool gripAlwaysInsideScale() const;

	/// @brief Enable/disable single step
	void setSingleStepEnabled(bool);
	bool singleStepEnabled() const;

	/// @brief Set the step based on a reference value
	/// Only works if singleStepEnabled() is true.
	/// @param singleStep step value
	/// @param reference reference value
	void setSingleStep(double singleStep, double reference = Vip::InvalidValue);
	double singleStep() const;
	double singleStepReference() const;

	/// @brief Set the tool tip to be displayed when moving the grip.
	/// Occurrences of '#value' will be replaced by the current handle value.
	/// @param text tool tip text
	void setToolTipText(const QString& text);
	const QString& toolTipText() const;

	/// @brief Set the bar text alignment within its bar based on the text position
	void setTextAlignment(Qt::Alignment align);
	Qt::Alignment textAlignment() const;

	/// @brief Set the grip text position: inside or outside the grip
	void setTextPosition(Vip::RegionPositions pos);
	Vip::RegionPositions textPosition() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform() const;
	const QPointF& textTransformReference() const;

	/// @brief Set the distance (in item's coordinate) between a grip border and its text
	/// @param distance
	void setTextDistance(double distance);
	double textDistance() const;

	/// @brief Set the text to be drawn within the grip.
	/// Each occurrence of the content '#value' will be replaced by the grip current value.
	void setText(const VipText& text);
	const VipText& text() const;
	VipText& text();

	/// @brief Set the distance between the tooltip and the handle
	void setToolTipDistance(double dist);
	double toolTipDistance() const;

	/// @brief Defines in which side (around the handle) the tool tip is displayed. Default to Qt::AlignRight|Qt::AlignVCenter.
	void setDisplayToolTipValue(Qt::Alignment side);
	Qt::Alignment displayToolTipValue() const;

	/// Set the handle image.
	/// The handle must point to the right.
	void setImage(const QImage&);
	QImage image() const;

	/// @brief Set the handle distance to the scale text
	void setHandleDistance(double vipDistance);
	virtual double handleDistance() const;

	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event);

public Q_SLOTS:

	void setValue(double);
	void updatePosition();

Q_SIGNALS:

	void valueChanged(double);

	void mouseButtonPress(VipSliderGrip*, VipPlotItem::MouseButton);
	void mouseButtonMove(VipSliderGrip*, VipPlotItem::MouseButton);
	void mouseButtonRelease(VipSliderGrip*, VipPlotItem::MouseButton);

private Q_SLOTS:
	void updatePositionInternal(bool InPaint);
	void setValueInternal(double value, bool InPaint);
	void moveTo(const QPointF&);

protected:
	virtual void drawHandle(QPainter* painter) const;
	virtual double closestValue(double v);

	virtual QString formatText(const QString& str, double value) const;

	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

	virtual void keyPressEvent(QKeyEvent* event);

	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	virtual bool hasState(const QByteArray& state, bool enable) const;

	// Reimplement to detect hover events and selection status changes in order to reapply style sheet
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

Q_DECLARE_METATYPE(VipSliderGrip*)


/// @}
// end Plotting

#endif
