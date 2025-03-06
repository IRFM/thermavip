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

#ifndef VIP_PLOT_SHAPE_H
#define VIP_PLOT_SHAPE_H

#include <QPointer>

#include "VipPlotItem.h"
#include "VipResizeItem.h"
#include "VipSceneModel.h"

/// \addtogroup Plotting
/// @{

class VipAnnotation;

/// @brief Plot item displaying a shape passed as a VipShape.
///
/// VipPlotShape is a plot item that draws a shape passed as a VipShape object using VipPlotShape::setData().
/// VipPlotShape also draw additional text(s) inside/outside the shape depending on the item's draw components
/// (see VipPlotShape::setDrawComponents()) and an optional custom text (see VipPlotShape::setText()).
///
/// As other VipPlotItemDataType inheriting classes, VipPlotShape::setData() is thread safe.
///
/// VipPlotShape support stylesheets and defines the following attributes:
/// -	'text-alignment' : see VipPlotShape::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
/// -	'text-position': see VipPlotShape::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
/// -	'text-distance': see VipPlotShape::setTextDistance()
/// -	'polygon-editable': see VipPlotShape::setPolygonEditable()
/// -	'adjust-text-color': equivalent to VipPlotScatter::setAdjustTextColor()
/// -   'components': equivalent to VipPlotScatter::setDrawComponents(), possible value are combinations of 'border|background|fillPixels|id|group|title|attributes'
/// -   'component': enable/disable one component at a time. Usage: 'component[background]: true;'
///
class VIP_PLOTTING_EXPORT VipPlotShape : public VipPlotItemDataType<VipShape>
{
	Q_OBJECT

public:
	/// @brief Components to be drawn
	enum DrawComponent
	{
		/// @brief Shape border using provided pen
		Border = 0x001,
		/// @brief Shape background using provided background
		Background = 0x002,
		/// @brief Draw exactly filled pixels
		FillPixels = 0x004,
		/// @brief Draw the shape id value around or inside the shape
		Id = 0x008,
		/// @brief Draw the shape group value around or inside the shape
		Group = 0x020,
		/// @brief Draw the shape title around or inside the shape
		Title = 0x040,
		/// @brief Draw the shape attributes value around or inside the shape, on the form 'name: value'
		Attributes = 0x080,
	};
	typedef QFlags<DrawComponent> DrawComponents;

	VipPlotShape(const VipText& title = QString());
	virtual ~VipPlotShape();

	/// @brief Get/set the components to be drawn
	void setDrawComponents(DrawComponents);
	void setDrawComponent(DrawComponent, bool on = true);
	bool testDrawComponent(DrawComponent) const;
	DrawComponents dawComponents() const;

	/// Set the annotation object.
	/// A VipAnnotation object is used to draw any kind of annotations arround the item VipShape.
	/// If a VipAnnotation object is provided, its members VipAnnotation::draw and VipAnnotation::shape will be used instead of VipPlotShape ones.
	/// The item takes ownership of the VipAnnotation object.
	///
	/// Another way to set the item's annotation is to define the VipShape attribute '_vip_annotation' containing a QByteArray created with
	/// #vipSaveAnnotation. The '_vip_annotation' attribute will always supersede an annotation set with setAnnotation().
	void setAnnotation(VipAnnotation*);
	/// Returns the internal annotation object.
	///
	/// \sa #VipPlotShape::setAnnotation
	VipAnnotation* annotation() const;

	/// @brief Adjust the text color based on the item's background.
	/// This is very convinient when drawing a VipPlotShape above a VipPlotRasterData in order to keep the text visible
	void setAdjustTextColor(bool);
	bool adjustTextColor() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform() const;
	const QPointF& textTransformReference() const;

	/// @brief Set the distance (in item's coordinate) between the shape and its text
	/// @param distance
	void setTextDistance(double distance);
	double textDistance() const;

	/// @brief Set the text to be drawn inside or around the shape.
	/// If a custom text is passed this way, the flags Id, Group, Title and Attributes are ignored, and only the provided text is drawn.
	/// Note that the text will be formatted with VipPlotShape::formatText().
	void setText(const VipText& text);
	const VipText& text() const;

	virtual void setTextStyle(const VipTextStyle& style);
	virtual VipTextStyle textStyle() const;

	void setTextPosition(Vip::RegionPositions);
	Vip::RegionPositions textPosition() const;

	void setTextAlignment(Qt::Alignment);
	Qt::Alignment textAlignment() const;

	/// @brief Reimplemented from VipPlotItem
	/// In addition to VipPlotItem:formatText() features, this implementation replaces:
	/// -	Occurrences of '#id' by the shape id (VipShape::id())
	/// -	Occurrences of '#group' by the shape group (VipShape::group())
	/// -	All string starting with '#p...' are replaced by the shape attribute value for given name ('...')
	virtual QString formatText(const QString& text, const QPointF& pos) const;
	virtual QString formatToolTip(const QPointF& pos) const;
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;

	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;
	virtual QPainterPath shape() const;
	virtual QRectF boundingRect() const;
	virtual QList<VipText> legendNames() const { return (QList<VipText>() << title()); }
	virtual void setData(const QVariant& value);

	virtual void setPen(const QPen& pen);
	virtual QPen pen() const;

	virtual void setBrush(const QBrush& brush);
	virtual QBrush brush() const;

	/// @brief Enable/disable editing the shape polygon (if polygon based)
	void setPolygonEditable(bool editable);
	bool polygonEditable() const;

	virtual bool applyTransform(const QTransform& tr);

protected:
	virtual void drawPath(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const;
	virtual void drawPolygon(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const;
	virtual void drawPolyline(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const;
	virtual void drawPoint(QPainter* painter, const VipCoordinateSystemPtr& m, const VipShape& sh) const;
	void setShape(const QPainterPath& path) const;

	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

Q_SIGNALS:
	/// @brief Emitted when the item is about to be destroyed
	void plotShapeDestroyed(VipPlotShape*);
	/// @brief Emitted when a polygon point is about to be manually changed by the user
	void aboutToChangePoints();
	/// @brief Emitted when a polygon point has been manually changed by the user
	void finishedChangePoints();

private Q_SLOTS:
	void internalUpdateOnSetData();

private:
	// center is the shape point, min_size is in screen coordinate
	QRectF ellipseAroundPixel(const QPointF& center, const QSizeF& min_size, const VipCoordinateSystemPtr& m) const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipPlotShape::DrawComponents)

typedef QPointer<VipResizeItem> VipResizeItemPtr;
Q_DECLARE_METATYPE(VipResizeItemPtr)

/// @brief Plot item that displays a scene model passed as a VipSceneModel object
///
/// A VipSceneModel is a collection of VipShape gathered in groups (unique string identifier).
/// As such, VipPlotSceneModel is a collection of VipPlotShape managed through its VipPlotItemComposite
/// base class.
///
/// VipPlotSceneModel uses several optimizations internally to avoid allocating/deallocating too many
/// VipPlotShape objects when the scene model changes, and is therefore very performant to display
/// highly dynamic scene models. It is for instance used to display results of event detection techniques
/// based on Computer Vision/AI on top of videos in firm real time applications.
///
/// A scene model is set using VipPlotSceneModel::setSceneModel(). As VipSceneModel and VipShape are
/// never deeply copied (they use shared ownership), it is also possible to modify inplace a VipSceneModel
/// object. Any modification of a VipSceneModel previously set with setSceneModel() will be automatically
/// reflected in the VipPlotSceneModel.
///
/// By default, VipPlotSceneModel uses the Aggregate composition mode: each internal VipPlotShape
/// is considered as an independant plot item and the user can interact with them. VipPlotSceneModel
/// can use the UniqueItem composition mode in which case internal VipPlotShape objects are hidden
/// to the user, and the VipPlotSceneModel is built as the union of each shape.
///
/// In addition, VipPlotSceneModel provides its own way to modify internal VipPlotShape objects using setMode().
/// If Fixed, the user cannot modify the VipPlotShape objects, only select/unselect them.
/// If Movable, each VipPlotShape can be moved by the user through a VipResizeItem automatically created by the VipPlotSceneModel.
/// If Resizable, each VipPlotShape can be moved/resized by the user through a VipResizeItem automatically created by the VipPlotSceneModel.
///
/// Any move/resize performed by the user will be reflected in the underlying VipSceneModel/
///
/// The styling of VipPlotSceneModel is controlled by the same functions as VipPlotShape (like setPen(), setBrush(), setText()...)
/// but each function requires an additional string parameter which is the scene model group on which the style applies.
/// Passing an empy string will apply the style to all groups.
///
class VIP_PLOTTING_EXPORT VipPlotSceneModel : public VipPlotItemComposite
{
	Q_OBJECT

public:
	enum Mode
	{
		Fixed,
		Movable,
		Resizable
	};

	VipPlotSceneModel(const VipText& title = VipText());
	~VipPlotSceneModel();

	/// @brief Set the interaction mode for internal VipPlotShape objects.
	/// If Fixed, the user cannot modify the VipPlotShape objects, only select/unselect them.
	/// If Movable, each VipPlotShape can be moved by the user through a VipResizeItem automatically created by the VipPlotSceneModel.
	/// If Resizable, each VipPlotShape can be moved/resized by the user through a VipResizeItem automatically created by the VipPlotSceneModel.
	/// @param  mode interaction mode
	void setMode(Mode mode);
	Mode mode() const;

	/// @brief Set the composite mode of this VipPlotSceneModel. The default is Aggregate.
	virtual void setCompositeMode(VipPlotItemComposite::Mode mode);

	/// @brief Get/set the drawn components for the specified group, or for all groups if group is empty (see VipPlotShape::setDrawComponents())
	void setDrawComponents(const QString& group, VipPlotShape::DrawComponents c);
	void setDrawComponent(const QString& group, VipPlotShape::DrawComponent, bool on = true);
	bool testDrawComponent(const QString& group, VipPlotShape::DrawComponent) const;
	VipPlotShape::DrawComponents drawComponents(const QString& group) const;

	/// @brief Set the text color adjustement for the specified group, or for all groups if group is empty (sse VipPlotShape::setAdjustTextColor())
	void setAdjustTextColor(const QString& group, bool);
	bool adjustTextColor(const QString& group) const;

	/// @brief Set the rendering hints for the specified group, or for all groups if group is empty
	void setShapesRenderHints(const QString& group, QPainter::RenderHints hints);
	QPainter::RenderHints shapesRenderHints(const QString& group) const;

	/// @brief Set the text position for the specified group, or for all groups if group is empty (sse VipPlotShape::setTextPosition())
	void setTextPosition(const QString& group, Vip::RegionPositions);
	Vip::RegionPositions textPosition(const QString& group) const;

	/// @brief Set the text alignment for the specified group, or for all groups if group is empty (sse VipPlotShape::setTextAlignment())
	void setTextAlignment(const QString& group, Qt::Alignment);
	Qt::Alignment textAlignment(const QString& group) const;

	/// @brief Set the custom text transform for the specified group, or for all groups if group is empty (sse VipPlotShape::setTextTransform())
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QString& group, const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform(const QString& group) const;
	const QPointF& textTransformReference(const QString& group) const;

	/// @brief Set the custom text distance to its shape for the specified group, or for all groups if group is empty (sse VipPlotShape::setTextDistance())
	void setTextDistance(const QString& group, double distance);
	double textDistance(const QString& group) const;

	/// @brief Set the text style used by all groups (stylesheet aware item)
	virtual void setTextStyle(const VipTextStyle& style);
	/// @brief Set the text style for the specified group, or for all groups if group is empty (sse VipPlotShape::setTextStyle())
	void setTextStyle(const QString& group, const VipTextStyle& style);
	VipTextStyle textStyle(const QString& group) const;
	virtual VipTextStyle textStyle() const;

	/// @brief Set the custom text for the specified group, or for all groups if group is empty (sse VipPlotShape::setText())
	void setText(const QString& group, const VipText& text);
	VipText text(const QString& group) const;

	using VipPlotItemComposite::setToolTipText;
	using VipPlotItemComposite::toolTipText;

	/// @brief Set the tool tip text for the specified group, or for all groups if group is empty
	void setToolTipText(const QString& group, const QString& text);
	QString toolTipText(const QString& group);

	/// @brief Set the shape pen for the specified group, or for all groups if group is empty
	void setPen(const QString& group, const QPen& pen);
	QPen pen(const QString& group) const;

	/// @brief Set the shape brush for the specified group, or for all groups if group is empty
	void setBrush(const QString& group, const QBrush& brush);
	QBrush brush(const QString& group) const;

	/// @brief Set VipResizeItem pen for the specified group, or for all groups if group is empty
	void setResizerPen(const QString& group, const QPen& pen);
	QPen resizerPen(const QString& group) const;

	/// @brief Set VipResizeItem brush for the specified group, or for all groups if group is empty
	void setResizerBrush(const QString& group, const QBrush& brush);
	QBrush resizerBrush(const QString& group) const;

	/// @brief Reinplemented from VipPlotItem, set the shape pen for all groups. Equivalent to setPen(QString(),pen)
	virtual void setPen(const QPen& pen);
	virtual QPen pen() const;

	/// @brief Reinplemented from VipPlotItem, set the shape brush for all groups. Equivalent to setBrush(QString(),brush)
	virtual void setBrush(const QBrush& brush);
	virtual QBrush brush() const;

	/// @brief Reinplemented from VipPlotItem, returns the shape pen color used for all groups
	virtual QColor majorColor() const { return pen().color(); }
	virtual void setMajorColor(const QColor& c)
	{
		QPen p = pen();
		p.setColor(c);
		setPen(p);
	}

	virtual void setIgnoreStyleSheet(bool);

	/// @brief Show/hide shapes for given group
	void setGroupVisible(const QString& group, bool visible);
	bool groupVisible(const QString& group) const;

	/// @brief Returns all internal VipPlotShape objects.
	/// is selection is 0 or 1, returns only selected/unselected shapes.
	QList<VipPlotShape*> shapes(int selection = -1) const;
	/// @brief Returns all internal VipPlotShape objects for given group.
	/// is selection is 0 or 1, returns only selected/unselected shapes.
	QList<VipPlotShape*> shapes(const QString& group, int selection = -1) const;

	/// @brief Returns the VipPlotShape object associated to given VipShape object, or null if not found
	VipPlotShape* findShape(const VipShape& sh) const;

	/// @brief Returns the underlying VipSceneModel object
	VipSceneModel sceneModel() const;

public Q_SLOTS:
	/// @brief Set the scene model managed by this item.
	/// This function is thread safe.
	void setSceneModel(const VipSceneModel& scene);
	void setData(const QVariant& scene) { setSceneModel(scene.value<VipSceneModel>()); }

	/// @brief Reset the content of the internal scene model with given one
	/// This function is thread safe.
	void resetContentWith(const VipSceneModel& scene);

	/// @brief Merge the content of the internal scene model with given one
	/// This function is thread safe.
	void mergeContentWith(const VipSceneModel& scene);

Q_SIGNALS:
	/// Emitted when the internal scene model groups changed
	void groupsChanged();
	/// Emitted when the item scene model changed
	void sceneModelChanged(const VipSceneModel&);
	/// Emitted when an internal VipPlotShape is destroyed
	void shapeDestroyed(VipPlotShape* shape);
	/// Emitted when the selection status of a VipPlotShape changed
	void shapeSelectionChanged(VipPlotShape*);

	void aboutToMove(VipResizeItem*);
	void aboutToResize(VipResizeItem*);
	void aboutToRotate(VipResizeItem*);
	void aboutToChangePoints(VipResizeItem*);
	void aboutToDelete(VipResizeItem*);
	void finishedChange(VipResizeItem*);

protected:
	VipPlotShape* createShape(const VipShape& sh) const;
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private Q_SLOTS:
	void updateShapes();
	void saveShapeSelectionState(VipPlotItem*);
	void saveShapeVisibilityState();
	void plotShapeDestroyed(VipPlotShape* shape);
	void emitAboutToMove();
	void emitAboutToResize();
	void emitAboutToRotate();
	void emitAboutToChangePoints();
	void emitAboutToDelete();
	void setSceneModelInternal();
	void resetSceneModelInternal();
	void resetSceneModelInternalWith();
	void mergeSceneModelInternalWith();

	void resetSceneModel();
	void emitGroupsChanged();
	void emitSceneModelChanged(const VipSceneModel& sm) { Q_EMIT sceneModelChanged(sm); }
	void emitFinishedChange();

private:
	QList<VipPlotShape*> shapeItems() const;
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotShape*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotShape* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotShape* value);

VIP_REGISTER_QOBJECT_METATYPE(VipPlotSceneModel*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotSceneModel* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotSceneModel* value);

/// @}
// end Plotting

#endif
