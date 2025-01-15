/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_RESIZE_ITEM_H
#define VIP_RESIZE_ITEM_H

#include "VipPlotItem.h"

/// \addtogroup Plotting
/// @{

/// @brief A plotting item used to move/resize/rotate other plot items
///
/// VipResizeItem is a VipPlotItem which goal is to make other VipPlotItem objects
/// resizable, movable and/or rotatable. These transforms are applied using
/// VipPlotItem::applyTransform() function. Currently, only VipPlotShape supports this.
///
/// By default, VipResizeItem draws nothing. Only when managed items are selected, VipResizeItem
/// will displays a bounding rectangle with optional resizers around managed items. VipResizeItem
/// was developped to mimic the behavior of items in PowerPoint which can be moved around, resized
/// or rotated.
///
/// VipResizeItem can manage one or more VipPlotItem using VipResizeItem::setManagedItems(). Managed
/// items can also contain one or more VipResizeItem to provide a behavior similar to PowerPoint
/// grouping function.
///
/// By default, VipResizeItem 'owns' managed items. Deleting all managed items will automatically destroy the
/// VipResizeItem, and deleting the VipResizeItem will destroy all managed items. This behavior is controlled
/// by the autoDelete() member.
///
/// VipResizeItem can restrain the potential transforms applied to managed items using the LibertyDegrees flag.
///
class VIP_PLOTTING_EXPORT VipResizeItem : public VipPlotItem
{
	Q_OBJECT

public:
	typedef void (*transform_function)(QGraphicsObject*, const QTransform& tr);

	/// @brief Boundary flag, tells if the item can be moved/resized outside the scales current boundaries
	enum BoundaryFlag
	{
		NoBoundary = 0x0000,
		LeftBoundary = 0x0001,
		RightBoundary = 0x0002,
		TopBoundary = 0x0004,
		BottomBoundary = 0x0008,
		AllBoundaries = LeftBoundary | RightBoundary | TopBoundary | BottomBoundary
	};
	Q_DECLARE_FLAGS(Boundaries, BoundaryFlag);

	/// @brief Liberty of degree flag, tells which kind of transform this item supports
	enum LibertyDegreeFlag
	{
		NoMoveOrResize = 0x0000,		       // None
		HorizontalMove = 0x0001,		       // Horizontal move only
		VerticalMove = 0x0002,			       // Vertical move only
		AllMove = HorizontalMove | VerticalMove,       // All kind of move
		HorizontalResize = 0x0004,		       // Horizontal resize only
		VerticalResize = 0x0008,		       // Vertical resize only
		AllResize = HorizontalResize | VerticalResize, // All kind of resize
		MoveAndResize = AllMove | AllResize,	       // All kind of move/resize
		ExpandHorizontal = 0x0010,		       // Expand the item horizontally to fill available area space
		ExpandVertical = 0x0020,		       // Expand the item vertically to fill available area space
		Rotate = 0x0040,			       // Allow rotations

	};
	Q_DECLARE_FLAGS(LibertyDegrees, LibertyDegreeFlag);

	VipResizeItem(const VipText& title = VipText());
	virtual ~VipResizeItem();

	/// @brief Reimplemented from VipPlotItem, returns the item shape
	virtual QPainterPath shape() const;
	/// @brief Reimplemented from VipPlotItem, returns the item bounding rectangle
	virtual QRectF boundingRect() const;
	/// @brief Reimplemented from VipPlotItem
	virtual QList<VipInterval> plotBoundingIntervals() const;

	/// @brief Returns the union shape of managed items
	QPainterPath managedItemsPath() const;
	/// @brief Returns the VipResizeItem geometry in scale coordinates
	QRectF geometry() const;

	/// @brief Set a custom shape for the left resizer (used to resize the item horizontally)
	void setCustomLeftResizer(const QPainterPath& p);
	const QPainterPath& customLeftResizer() const;
	/// @brief Set a custom shape for the right resizer (used to resize the item horizontally)
	void setCustomRightResizer(const QPainterPath& p);
	const QPainterPath& customRightResizer() const;
	/// @brief Set a custom shape for the bottom resizer (used to resize the item vertically)
	void setCustomBottomResizer(const QPainterPath& p);
	const QPainterPath& customBottomResizer() const;
	/// @brief Set a custom shape for the top resizer (used to resize the item vertically)
	void setCustomTopResizer(const QPainterPath& p);
	const QPainterPath& customTopResizer() const;

	/// @brief Set the pen used to draw resizers
	void setResizerPen(const QPen& p);
	const QPen& resizerPen() const;

	/// @brief Set the brush used to draw resizers
	void setResizerBrush(const QBrush& b);
	const QBrush& resizerBrush() const;

	/// @brief Set managed items
	/// If autoDelete() is true, previously managed items will be deleted
	void setManagedItems(const PlotItemList& managed);
	/// @brief Returns currently managed items
	const PlotItemList& managedItems() const;

	/// @brief Returns the parent VipResizeItem if this item belong to a higher level VipResizeItem
	VipResizeItem* parentResizeItem() const;
	/// @brief Returns the top level parent VipResizeItem if this item belong to a higher level VipResizeItem
	VipResizeItem* topLevelParentResizeItem() const;

	/// @brief Returns managed items that are of type VipResizeItem
	QList<VipResizeItem*> directChildren() const;
	/// @brief Returns ALL managed items (recursively) that are of type VipResizeItem
	QList<VipResizeItem*> children() const;

	/// @brief Set the space between the managed items bounding rect and the VipResizeItem bounding rect (in item's coordinates)
	void setSpacing(double spacing);
	double spacing() const;

	/// @brief Set/get the box style used to draw the VipResizeItem when selected
	void setBoxStyle(const VipBoxStyle& st);
	const VipBoxStyle& boxStyle() const;
	VipBoxStyle& boxStyle();

	/// @brief Reimplemented from VipPlotItem, set the box style pen
	virtual void setPen(const QPen& p) { boxStyle().setBorderPen(p); }
	virtual QPen pen() const { return boxStyle().borderPen(); }

	/// @brief Reimplemented from VipPlotItem, set the box style brush
	virtual void setBrush(const QBrush& b) { boxStyle().setBackgroundBrush(b); }
	virtual QBrush brush() const { return boxStyle().backgroundBrush(); }

	/// @brief Enable/disable auto deletion
	/// If enabled, destroying the VipResizeItem will destroy managed items and conversely
	void setAutoDelete(bool autodelete);
	bool autoDelete() const;

	/// @brief Set item boundaries.
	/// Boundaries prevent the VipResizeItem from being moved/resized outside of the current scales bounds.
	void setBoundaries(Boundaries);
	void setBoundaryFlag(BoundaryFlag flag, bool on = true);
	bool testBoundaryFlag(BoundaryFlag flag) const;
	Boundaries boundaries() const;

	/// @brief Set the degrees of freedom.
	/// Tells if managed items can be moved, resized and/or rotated.
	void setLibertyDegrees(LibertyDegrees);
	void setLibertyDegreeFlag(VipResizeItem::LibertyDegreeFlag flag, bool on = true);
	bool testLibertyDegreeFlag(VipResizeItem::LibertyDegreeFlag flag) const;
	LibertyDegrees libertyDegrees() const;

	/// @brief If ExpandHorizontal or ExpandVertical is defined, tells if the expension is limitted to the VipResizeItem axes or to the full area
	void setExpandToFullArea(bool);
	bool expandToFullArea() const;

	/// @brief Enable/disable unit move and resize.
	/// If enabled, managed items can only be moved/resized by step of 1 scale unit.
	/// This is only usefull when the VipResizeItem is above an image (using VipPlotSpectrogram)
	void setUnitMoveAndResize(bool unit);
	bool unitMoveAndResize() const;

	/// @brief Set the minimum size of managed item bounding rectangle.
	/// This will prevent resizing managed items to a too small box.
	/// The size is in scales coordinates.
	void setMinimumSize(const QSizeF&);
	QSizeF minimumSize() const;

	/// @brief Helper function, returns true if this VipResizeItem allows move operations
	bool moveEnabled() const;
	/// @brief Helper function, returns true if this VipResizeItem allows resize operations
	bool resizeEnabled() const;

	/// @brief Reimplemented from VipPlotItem, apply a transform to this item and managed items
	virtual bool applyTransform(const QTransform& tr);
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void drawSelected(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type);
	using VipPlotItem::setAxes;

	/// @brief Returns all linked VipResizeItem objects
	/// Linked items are VipPlotItem objects sharing the same axes.
	QList<VipResizeItem*> linkedResizeItems() const;

	/// @brief Reimplemented from QGraphicsItem
	virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event);

Q_SIGNALS:

	/// @brief Emitted when a new transform is applied to this item programatically using VipResizeItem::applyTransform
	void newTransform(const QTransform& tr);
	/// @brief Emitted the item geometry changed manually or programatically
	void geometryChanged(const QRectF& r);
	/// @brief Emitted when the item is about to be moved
	void aboutToMove();
	/// @brief Emitted when the item is about to be resized
	void aboutToResize();
	/// @brief Emitted when the item is about to be rotated
	void aboutToRotate();
	/// @brief If this item manages a VipPlotShape displaying a polygon, Emitted when the shape polygon is about to be modified manually
	void aboutToChangePoints();
	/// @brief Emitted when a change has been performed (move, resize, rotate or polygon change)
	void finishedChange();

private Q_SLOTS:

	void managedItemsChanged();
	void itemDestroyed(QObject*);
	void emitAboutToChangePoints() { Q_EMIT aboutToChangePoints(); }
	void emitFinishedChange() { Q_EMIT finishedChange(); }

protected:
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);

	void emitNewTransform(const QTransform& tr);
	void setGeometry(const QRectF& r);
	void computeResizers();
	void recomputeFullGeometry();
	void changeGeometry(const QRectF& from, const QRectF& to, QRectF* new_bounding_rect, bool* ignore_x, bool* ignore_y);
	int itemUnderMouse(const QPointF& pt);

	QTransform computeTransformation(const QRectF& old_rect, const QRectF& new_rect) const;

	QRectF addSpacing(const QRectF&) const;
	QRectF removeSpacing(const QRectF&) const;
	QRectF boundRect() const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipResizeItem::Boundaries)
Q_DECLARE_OPERATORS_FOR_FLAGS(VipResizeItem::LibertyDegrees)

/// @}
// end Plotting

#endif
