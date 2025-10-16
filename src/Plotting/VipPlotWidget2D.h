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

#ifndef VIP_PLOT_WIDGET_2D_H
#define VIP_PLOT_WIDGET_2D_H

#include <QGraphicsView>

#include "VipAxisBase.h"
#include "VipAxisColorMap.h"
#include "VipNDArray.h"
#include "VipPlotItem.h"

/// \addtogroup Plotting
/// @{

class VipPlotGrid;
class VipPlotCanvas;
class VipAxisColorMap;
class VipPolarAxis;
class VipRadialAxis;
class VipAbstractPolarScale;
class VipColorMap;
class VipInterval;
class VipPlotSpectrogram;
class VipBorderLegend;
class VipLegend;
class VipToolTip;
class QImage;
class QPicture;
class VipAbstractPlotArea;

/// @brief Base class for VipAbstractPlotArea filters
///
/// VipPlotAreaFilter filters most events goiing through a VipAbstractPlotArea:
/// mouse, keyboard and painting events.
///
/// Use VipAbstractPlotArea::installFilter() to attach a VipPlotAreaFilter to a VipAbstractPlotArea object.
///
class VIP_PLOTTING_EXPORT VipPlotAreaFilter : public QGraphicsObject
{
	Q_OBJECT
	friend class VipRubberBand;

public:
	VipPlotAreaFilter();
	~VipPlotAreaFilter();

	VipAbstractPlotArea* area() const;

protected:
	void emitFinished();

Q_SIGNALS:
	/// @brief Convenient signal that can be used by sub-classes, but has no builtin effects
	void finished();

private:
	VipAbstractPlotArea* d_area;
};

/// @brief Rubber band drawing on top of a VipAbstractPlotArea
///
/// VipRubberBand is a QGraphicsItem child of a VipAbstractPlotArea and used to filter its events
/// and draw a selection rubber-band.
///
/// A VipAbstractPlotArea always own a VipRubberBand as it is mandatory for its internal event filtering.
/// Use VipAbstractPlotArea::setRubberBand() to affect a new VipRubberBand.
///
/// By default, VipRubberBand can draw a selection rubber band for cartesian and polar coordinate systems.
///
/// VipRubberBand supports stylesheets and adds the following attibutes:
/// -	'color': if the item draw text, defines the text color
/// -	'font': if the item draw text, defines the text font
/// -	'font-size': if the item draw text, defines the font size
/// -	'font-style': if the item draw text, defines the font style
/// -	'font-weight': if the item draw text, defines the font weight
/// -	'font-family': if the item draw text, defines the font familly
/// -	'text-border': if the item draw text, defines the text box border pen
/// -	'text-border-radius': if the item draw text, defines the text box border radius
/// -	'text-background': if the item draw text, defines the text box background
/// -	'text-border-margin': if the item draw text, defines the text box border margin
///
class VIP_PLOTTING_EXPORT VipRubberBand : public VipBoxGraphicsWidget
{
	Q_OBJECT
	friend class VipPlotAreaFilter;
	friend class VipAbstractPlotArea;

public:
	VipRubberBand(VipAbstractPlotArea* parent = nullptr);
	virtual ~VipRubberBand();

	/// @brief Draw the rubber band based on its start  and end position
	/// using its current box style
	virtual void drawRubberBand(QPainter* painter) const;

	/// @brief Returns the parent VipAbstractPlotArea
	VipAbstractPlotArea* area() const;
	/// @brief Returns installed VipPlotAreaFilter if any
	VipPlotAreaFilter* filter() const;

	/// @brief Set/get the rubber band text style.
	/// The default VipRubberBand will display the start/end coordinate values using this text style.
	void setTextStyle(const VipTextStyle& style);
	const VipTextStyle& textStyle() const;

	/// @brief Adds a paint command to this rubber band.
	/// The paint command will be reseted when the mouse move over/leave the parent VipAbstractPlotArea.
	void setAdditionalPaintCommands(const QPicture&);
	const QPicture& additionalPaintCommands() const;

	/// Set the rubber band start point in VipAbstractPlotArea coordinates.
	/// This will reset the end point (end = start).
	void setRubberBandStart(const QPointF& start);
	/// Set the rubber band end point in VipAbstractPlotArea coordinates
	void setRubberBandEnd(const QPointF& end);
	/// Reset the rubber band extremities ( start = end = QPoint() )
	void resetRubberBand();
	/// Returns the rubber band start point in VipAbstractPlotArea coordinates
	const QPointF& rubberBandStart() const;
	/// Returns the rubber band end point in VipAbstractPlotArea coordinates
	const QPointF& rubberBandEnd() const;
	/// Returns the rubber band width in VipAbstractPlotArea coordinates
	double rubberBandWidth() const;
	/// Returns the rubber band height in VipAbstractPlotArea coordinates
	double rubberBandHeight() const;
	/// Returns the rubber band rect in VipAbstractPlotArea coordinates
	QRectF rubberBandRect() const;
	/// Returns the rubber band start point in scale coordinates
	const VipPoint& rubberBandScaleStart() const;
	/// Returns the rubber band end point in scale coordinates
	const VipPoint& rubberBandScaleEnd() const;
	/// Returns true if the rubber band currently defines a non null area
	bool hasRubberBandArea() const;

public Q_SLOTS:
	void updateGeometry();

protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void keyReleaseEvent(QKeyEvent* event);
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual void wheelEvent(QGraphicsSceneWheelEvent* event);
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private:
	void setArea(VipAbstractPlotArea* area);
	void installFilter(VipPlotAreaFilter* filter);
	void removeFilter();

	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief QGraphicsObject used to draw plot items selection order over a VipAbstractPlotArea
///
/// VipDrawSelectionOrder is attached to a VipAbstractPlotArea and used to display VipPlotItem
/// selection order on top of the VipAbstractPlotArea.
///
/// The items selection order might be used by several algorithm defined in Core module,
/// like subtracting 2 curves.
///
/// The selection order is drawn using provided font and a white text over a colored rectangle.
/// The rectangle color is based on VipPlotItem::majorColor().
///
/// The text position for each selected VipPlotItem is computed using VipPlotItem::drawSelectionOrderPosition(alignment())
///
class VipDrawSelectionOrder : public QGraphicsObject
{
	Q_OBJECT
	friend class VipAbstractPlotArea;

public:
	VipDrawSelectionOrder(VipAbstractPlotArea* parent = nullptr);
	/// @brief Returns the parent VipAbstractPlotArea
	VipAbstractPlotArea* area() const;

	/// @brief Set/get the text font
	void setFont(const QFont& font);
	QFont font() const;

	/// @brief Set/get the alignment passed to VipPlotItem::drawSelectionOrderPosition()
	void setAlignment(Qt::Alignment align);
	Qt::Alignment alignment() const;

	virtual QRectF boundingRect() const;
	virtual QPainterPath shape() const;

protected:
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

private:
	void setArea(VipAbstractPlotArea* area);
	Qt::Alignment m_align;
	QFont m_font;
};

namespace Vip
{
	namespace detail
	{
		/// @brief Legend position flag, only used for stylesheet
		enum LegendPosition
		{
			LegendLeft = VipBorderItem::Left,
			LegendRight = VipBorderItem::Right,
			LegendTop = VipBorderItem::Top,
			LegendBottom = VipBorderItem::Bottom,
			LegendInnerLeft = 4,
			LegendInnerRight,
			LegendInnerTop,
			LegendInnerBottom,
			LegendInnerTopLeft,
			LegendInnerTopRight,
			LegendInnerBottomLeft,
			LegendInnerBottomRight,
			LegendNone
		};

		class ItemDirtyNotifier;
		using ItemDirtyNotifierPtr = QSharedPointer<ItemDirtyNotifier>;
	}
}

/// @brief Base class for all types of plotting areas
///
/// VipAbstractPlotArea is the base class for plotting areas like VipPlotArea2D, VipImageArea2D or VipPolarArea2D.
///
/// It defines several axes, optional color map(s), one or more legends, and plotting items are automatically added by setting their axes.
/// Since VipAbstractPlotArea inherits VipBoxGraphicsWidget, it can display a background box using its box style, and can be inserted
/// into QGraphicsLayout objects.
///
/// VipAbstractPlotArea handle input events from children items and allow simple interactions with the plotting area like zooming
/// or mouse panning.
///
/// Legends
/// -------
///
/// By default, VipAbstractPlotArea displays a legend located outside the plotting area at the bottom.
/// It is possible to move this legend around the plotting area by setting its alignment (for instance area->borderLegend()->setAlignment(VipBorderItem::Left);),
/// or to hide it completely.
///
/// In addition, VipAbstractPlotArea supports adding any number of inner legend (see VipAbstractPlotArea::addInnerLegend()) that will
/// be displayed inside the plotting area canvas.
///
/// See 'VStack' test code for an example of inner legend usage.
///
///
/// Items management
/// ----------------
///
/// VipAbstractPlotArea controls the way its internal items (plotting items and scales) are drawn and how their geometry are computed.
/// Updating a scale or an item will trigger a VipAbstractPlotArea update that will then render all items.
///
/// Whatever are the updating frequencies of internal items, VipAbstractPlotArea will adjust its refresh frame rate to keep the GUI
/// responsive. The resfresh rate of a VipAbstractPlotArea is bounded by its VipAbstractPlotArea::maximumFrameRate() value (default to 60).
/// This ensures that no unecessary CPU time is wasted on display even if the refresh rate could be supported.
///
/// By default, all items of a VipAbstractPlotArea are rendered independently using QGraphicsView rendering method, which could use opengl is the viewport is a QOpenGLWidget.
///
///
/// Stylesheets
/// -----------
///
/// VipAbstractPlotArea supports stylesheets and adds the following properties:
/// -	'mouse-selection-and-zoom' : boolean value equivalent to VipAbstractPlotArea::setMouseSelectionAndZoom()
/// -	'mouse-panning': equivalent to VipAbstractPlotArea::setMousePanning(), possible values are 'leftButton', 'rightButton' and 'middleButton'
/// -	'mouse-zoom-selection': equivalent to VipAbstractPlotArea::setMouseZoomSelection(),  possible values are 'leftButton', 'rightButton' and 'middleButton'
/// -	'mouse-item-selection': equivalent to VipAbstractPlotArea::setMouseItemSelection(), possible values are 'leftButton', 'rightButton' and 'middleButton'
/// -	'mouse-wheel-zoom': boolean value equivalent to VipAbstractPlotArea::setMouseWheelZoom()
/// -	'zoom-multiplier': floating point value equivalent to VipAbstractPlotArea::setZoomMultiplier()
/// -	'maximum-frame-rate': equivalent to VipAbstractPlotArea::setMaximumFrameRate()
/// -	'draw-selection-order': boolean value that enable/disable drawing item's selection order
/// -	'colorpalette' set the default color palette for item's color: 'random', 'pastel', 'set1'
/// -	'margins': floating point value that set the margins around the area
/// -	'tool-tip-selection-border': pen used to highlight an element on the pot area
/// -	'tool-tip-selection-background': brush color used to highlight an element on the pot area
///	-	'track-scales-state': boolean value equivalent to VipAbstractPlotArea::setTrackScalesStateEnabled()
/// -	'maximum-scales-states' : equivalent to VipAbstractPlotArea::setMaximumScalesStates()
/// -	'legend-position': main legend position, one of 'none', 'left', 'right', 'top', 'bottom', 'innerLeft', 'innerRight', 'innerTop', 'innerBottom', 'innerTopLeft', 'innerTopRight',
/// 'innerBottomLeft', 'innerBottomRight'
/// -	'legend-border-distance': for inner legends, set distance to borders. For outer legend, set its margin.
///
class VIP_PLOTTING_EXPORT VipAbstractPlotArea : public VipBoxGraphicsWidget
{
	Q_OBJECT
	friend class VipRubberBand;
	friend class VipPlotItem;
	friend class VipAbstractScale;
	friend class VipBoxGraphicsWidget;
	friend class VipAbstractPlotWidget2D;
	friend class VipBaseGraphicsView;
	friend class RenderThread;
	friend class ComputeBorderGeometry;
	friend class VipDisplayObject;
	friend class VipDisplayPlotItem;

public:
	typedef QMap<const VipAbstractScale*, VipInterval> scales_state;

	VipAbstractPlotArea(QGraphicsItem* parent = nullptr);
	virtual ~VipAbstractPlotArea();

	/// @brief If this area is inside a QGraphicsView, returns the visualized scene rectangle within the QGraphicsView
	QRectF visualizedSceneRect() const;

	/// @brief Enable selection of plot items with the same mouse button as zooming.
	///
	/// If enabled, #mouseSelectionAndZoomMinimumSize() is used to select between selection and zooming.
	/// If the area's width and height are lower than #mouseSelectionAndZoomMinimumSize(), item selection is performed.
	/// Otherwise, zooming is performed.
	/// #mouseSelectionAndZoomMinimumSize() is given in VipAbstractPlotArea's item corrdinate.
	virtual void setMouseSelectionAndZoom(bool enable);
	bool mouseSelectionAndZoom() const;
	virtual void setMouseSelectionAndZoomMinimumSize(const QSizeF& s);
	QSizeF mouseSelectionAndZoomMinimumSize() const;

	/// @brief Enable mouse panning with given mouse button.
	///
	/// Use Qt::NoButton to disable it.
	virtual void setMousePanning(Qt::MouseButton);
	Qt::MouseButton mousePanning() const;
	/// Returns true if the user is currently panning with the mouse (between mouse press and release).
	bool isMousePanning() const;

	/// @brief Enable mouse zooming with given mouse button.
	///
	/// Use Qt::NoButton to disable it.
	virtual void setMouseZoomSelection(Qt::MouseButton);
	Qt::MouseButton mouseZoomSelection() const;

	/// @brief Enable selecting items in an area with given mouse button.
	///
	/// Use Qt::NoButton to disable it.
	virtual void setMouseItemSelection(Qt::MouseButton);
	Qt::MouseButton mouseItemSelection() const;

	/// @brief Enable zooming with the wheel.
	virtual void setMouseWheelZoom(bool enable);
	bool mouseWheelZoom() const;

	/// @brief Set the wheel zoom multiplier.
	/// Default to 1.15
	virtual void setZoomMultiplier(double mult);
	double zoomMultiplier() const;

	/// @brief Enable/disable zooming on given axis.
	/// This will affect wheel zooming, area zooming and mouse panning.
	virtual void setZoomEnabled(VipAbstractScale* sc, bool enable);
	bool zoomEnabled(VipAbstractScale* sc);

	/// @brief Set the maximum refresh rate of this VipAbstractPlotArea
	///
	/// Default value is 60.
	/// Bounding the refresh rate is usefull to save some CPU time when we don't need a high update frequency.
	/// Going beyond 60 is almost useless as the human eye won't see a difference.
	void setMaximumFrameRate(int);
	int maximumFrameRate() const;

	/// @brief Set the rubber band used to draw the selection area
	virtual void setRubberBand(VipRubberBand* rubberBand);
	VipRubberBand* rubberBand() const;

	/// @brief Set the VipDrawSelectionOrder object used to draw plot items selection order.
	/// Setting a null value is possible and will disable drawing plot items selection order.
	virtual void setDrawSelectionOrder(VipDrawSelectionOrder* drawSelection);
	VipDrawSelectionOrder* drawSelectionOrder() const;

	/// @brief Set the color scheme used for the color map ('autumn', 'bone', ...)
	/// See VipLinearColorMap for more details
	/// This function is meant to be used with Qt stylesheet mechanism using VipBaseGraphicsView class.
	void setColorMap(const QString&);
	QString colorMap() const;

	/// @brief Set the color palette used to set the major color of plot items added to this area ('random', 'pastel', 'set1', ....)
	/// By default, the area does not have a color palette and each plot item will keep its previous color when added.
	/// See VipLinearColorMap for more details.
	/// This function is meant to be used with Qt stylesheet mechanism using VipBaseGraphicsView class.
	void setColorPalette(const QString&);
	/// @brief Set the color palette used to set the major color of plot items added to this area.
	/// By default, the area does not have a color palette and each plot item will keep its previous color when added.
	/// This function is meant to be used with Qt stylesheet mechanism using VipBaseGraphicsView class.
	void setColorPalette(const VipColorPalette&);
	QString colorPaletteName() const;
	VipColorPalette colorPalette() const;

	/// @brief Install an event filter on this area
	virtual void installFilter(VipPlotAreaFilter* filter);
	virtual void removeFilter();
	VipPlotAreaFilter* filter() const;

	/// @brief Set border margins in item's coordinates
	virtual void setMargins(const VipMargins& margins);
	void setMargins(double value) { setMargins(VipMargins(value, value, value, value)); }
	void setMargins(const QRectF& rect);
	VipMargins margins() const;

	/// @brief Set the area VipToolTip object, or a null value to disable area tool tip
	virtual void setPlotToolTip(VipToolTip*);
	VipToolTip* plotToolTip() const;

	/// Returns the list of scales that can represent given point in item coordinates
	virtual QList<VipAbstractScale*> scalesForPos(const QPointF& pos) const;

	/// @brief Returns the area grid
	virtual VipPlotGrid* grid() const;

	/// @brief Returns the area canvas
	virtual VipPlotCanvas* canvas() const;

	/// @brief Returns the area border legend
	VipBorderLegend* borderLegend() const;

	/// @brief Set the default area legend
	///
	/// If own is true, the legend is inserted into the border legend item and the previous one is deleted.
	virtual void setLegend(VipLegend* legend, bool own = true);
	VipLegend* legend() const;

	/// @brief Add a new legend inside the area canvas
	///
	/// The legend will be displayed inside the area canvas at border_margin distance of the closest borders.
	/// Given alignment tells where to place the legend inside the canvas.
	void addInnerLegend(VipLegend* legend, Qt::Alignment alignment, int border_margin);
	/// @brief Add a new legend inside the area canvas
	///
	/// The legend will be displayed inside the area canvas at border_margin distance of the closest borders.
	/// Given alignment tells where to place the legend inside the canvas.
	/// The legend will only display plot items that use provided scale.
	void addInnerLegend(VipLegend* legend, VipAbstractScale* scale, Qt::Alignment alignment, int border_margin);
	/// @brief Returns the legend scale (if any)
	VipAbstractScale* scaleForlegend(VipLegend*) const;
	/// @brief Removes inner legend and returns it
	VipLegend* takeInnerLegend(VipLegend* legend);
	/// @brief Removes inner legend and delete it
	void removeInnerLegend(VipLegend* legend);
	/// @brief Set inner legend alignment
	void setInnerLegendAlignment(int index, Qt::Alignment);
	/// @brief Set inner legend border margin
	void setInnerLegendMargin(int index, int border_margin);
	/// @brief Returns all inner legends
	QList<VipLegend*> innerLegends() const;
	/// @brief Returns inner legends count
	int innerLegendCount() const;
	/// @brief Returns inner legend at given index
	VipLegend* innerLegend(int index) const;
	/// @brief Returns inner legend alignment
	Qt::Alignment innerLegendAlignment(int index) const;
	/// @brief Returns inner legend border margin
	int innerLegendMargin(int index) const;

	/// @brief Reimplemented from VipPaintItem, set plot area title
	virtual void setTitle(const VipText&);
	/// @brief Returns title axis, used to display the plot area title
	VipAxisBase* titleAxis() const;

	/// @brief Add a scale to this area
	/// The scale will become a child of this area.
	/// Any scale inheriting VipBorderItem will be organized around the plotting area
	/// depending on its alignment and canvas proximity (see VipBorderItem::setAlignment() and VipBorderItem::setCanvasProximity())
	/// The isSpatialCoordinate should tell if the scale is a spatial one (used to set VipPlotItem axes) or another type
	/// (like color scales). For instance, all zoom/panning operations only operate on spatial scales.
	virtual void addScale(VipAbstractScale*, bool isSpatialCoordinate = true);

	/// @brief Remove scale from area without deletting it
	virtual void removeScale(VipAbstractScale*);
	/// Returns the spatial scales
	QList<VipAbstractScale*> scales() const;
	/// Returns all scales
	QList<VipAbstractScale*> allScales() const;

	/// @brief Returns the current scales state.
	/// The scales state stores, for each scale, its current scale div.
	/// This feature is used to build a kind of undo/redo system for scales
	/// with members undoScalesState(), redoScalesState() and bufferScalesState()
	scales_state scalesState() const;
	void setScalesState(const scales_state& state);

	/// Returns given axis interval (in scale coordinates) to represent the full plotting area (based on #innerArea()).
	virtual VipInterval areaBoundaries(const VipAbstractScale* scale) const;
	/// Returns the inner plotting area in item coordinates
	virtual QPainterPath innerArea() const = 0;

	/// @brief Enable/disable label overlapping for scales.
	/// Label overlapping is used to avoid scales from drawing their labels on top of each other.
	/// This feature is disabled by default as costly to compute.
	void setDefaultLabelOverlapping(bool);
	bool defaultLabelOverlapping() const;

	/// @brief Tells if scales state tracking is enabled.
	/// See setTrackScalesStateEnabled() for more details.
	bool isTrackScalesStateEnabled() const;
	/// @brief Returns the maximum number of buffered scales states
	/// See setTrackScalesStateEnabled() for more details.
	int maximumScalesStates() const;
	/// @brief Returns the list of undo scale states
	const QList<scales_state>& undoStates() const;
	/// @brief Returns the list of undo scale states
	const QList<scales_state>& redoStates() const;

	/// Helper function, save the spatial scale bounds.
	/// You can restore them with restoreSpatialScaleState().
	QByteArray saveSpatialScaleState() const;

	/// Returns true if all spatial scales are automatic
	bool isAutoScale() const;

	/// Returns true if the mouse is currently used for a mouse panning like operation
	bool mouseInUse() const;

	/// @brief Align this plot area with other.
	/// Usefull when multiple areas are inserted into a QGraphicsLayout, as it
	/// ensures that the outer scales are properly aligned.
	/// For instance, when inserting areas into a vertical layout, you should use with
	/// function with Qt::Vertical alignment to make sure that left/right area scales are aligned
	/// with upper/lower areas scales.
	void setAlignedWith(VipAbstractPlotArea* other, Qt::Orientation align_orientation);

	/// @brief Returns all areas aligned with this area for given orientation
	QList<VipAbstractPlotArea*> alignedWith(Qt::Orientation align_orientation) const;

	/// @brief Remove this area from its alignment for given orientation
	void removeAlignment(Qt::Orientation align_orientation);

	/// @brief Build the standard scale list and returns the area coordinate system.
	/// This is a convenient function to add VipPlotItem object to this area.
	/// Example:
	/// \code{cpp}
	/// QList<VipAbstractScale*> axes;
	/// auto csystem = my_area->standardScales(axes);
	/// //...
	/// VipPlotCurve * c = new VipPlotCurve();
	/// c->setAxes(axes,csystem);
	/// \endcode
	///
	virtual VipCoordinateSystem::Type standardScales(QList<VipAbstractScale*>& axes) const = 0;

	/// @brief Creates and returns a VipAxisColorMap
	///
	/// The VipAxisColorMap is created with given alignment (Left, Right, Top or Bottom), scale div interval and VipColorMap.
	/// The VipColorMap itself can be created quickly using VipLinearColorMap::createColorMap().
	virtual VipAxisColorMap* createColorMap(VipAxisBase::Alignment, const VipInterval&, VipColorMap*);

	/// @brief Returns all VipPlotItem those shapes contain given position in item's coordinates.
	/// If pos is invalid (default), returns all VipPlotItem this area contains.
	virtual PlotItemList plotItems(const QPointF& pos = Vip::InvalidPoint) const;

	/// @brief Returns all VipPlotItem those shapes contain given position in item's coordinates, with a distance tolerance of maxDistance.
	/// For each found item, fill out_points, out_styles and out_legends using VipPlotItem::areaOfInterest().
	/// This function is mainly used to display a tool tip over the VipAbstractPlotArea based on the mouse position.
	virtual PlotItemList plotItems(const QPointF& pos, int axis, double maxDistance, QList<VipPointVector>& out_points, VipBoxStyleList& out_styles, QList<int>& out_legends) const;

	/// @brief Zoom on the rectangle represented by start and end positions in item's coordinates.
	/// The zoom is applied to all spacial scales.
	virtual void zoomOnSelection(const QPointF& start, const QPointF& end) = 0;
	/// @brief Zoom in/out by a given factor centered on pos.
	/// The zoom is applied to all spacial scales.
	virtual void zoomOnPosition(const QPointF& pos, double factor) = 0;
	/// @brief Update all spatial scales intervals by moving the point fromPt of dp delta.
	/// Used for mouse panning.
	virtual void translate(const QPointF& fromPt, const QPointF& dp) = 0;

	/// @brief Reimplemented from QGraphicsItem
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);

	/// @brief Returns all children items inheriting QGraphicsObject
	/// @param name if non empty, only returns objects with given name
	/// @param selection if 0 or 1, only returns selected/unselected objects
	/// @param visible if 0 or 1, only returns visible/hidden objects
	template<class T>
	QList<T> findItems(const QString& name = QString(), int selection = 2, int visible = 2)
	{
		return vipCastItemList<T>(this->childItems(), name, selection, visible);
	}

	/// @brief Returns all children items inheriting QGraphicsObject
	/// Same as findItems(), but returns items ordered by selection
	template<class T>
	QList<T> findItemsOrdered(const QString& name = QString(), int selection = 2, int visible = 2)
	{
		return vipCastItemListOrdered<T>(this->childItems(), name, selection, visible);
	}

	/// @brief Helper function, convert a point in this item coordinates to scale coordinates.
	/// This only works if this area defines 2 standard scales.
	VipPoint positionToScale(const QPointF& pos, bool* ok = nullptr) const;
	VipPoint positionToScale(const QPointF& pos, const QList<VipAbstractScale*>& scales, bool* ok = nullptr) const;

	/// @brief Helper function, convert a point in scale coordinates to this item coordinates.
	/// This only works if this area defines 2 standard scales.
	QPointF scaleToPosition(const VipPoint& scale_value, bool* ok = nullptr) const;
	QPointF scaleToPosition(const VipPoint& scale_value, const QList<VipAbstractScale*>& scales, bool* ok = nullptr) const;

	/// @brief Helper function, convert a vector of points in this item coordinates to scale coordinates.
	/// This only works if this area defines 2 standard scales.
	VipPointVector positionToScale(const QVector<QPointF>& positions, bool* ok = nullptr) const;
	VipPointVector positionToScale(const QVector<QPointF>& positions, const QList<VipAbstractScale*>& scales, bool* ok = nullptr) const;

	/// @brief Helper function, convert a vector of points in scale coordinates to this item coordinates.
	/// This only works if this area defines 2 standard scales.
	QVector<QPointF> scaleToPosition(const VipPointVector& scale_values, bool* ok = nullptr) const;
	QVector<QPointF> scaleToPosition(const VipPointVector& scale_values, const QList<VipAbstractScale*>& scales, bool* ok = nullptr) const;

	/// @brief Returns the last mouse position on clicking.
	/// The position is valid until releasing the mouse.
	QPointF lastMousePressPos() const;

	/// @brief Returns the parent QGraphicsView (if any)
	QGraphicsView* view() const;

	/// @brief Returns the last VipPlotItem which triggered a mouseButtonPressed() signal
	VipPlotItem* lastPressed() const;

	/// @brief Internal use only
	void setCustomUpdateFunction(const std::function<void()>& f);
	
public Q_SLOTS:

	/// @brief Enable/disable automatic scaling for spatial scales
	virtual void setAutoScale(bool auto_scale);
	/// @brief Enable automatic scaling for spatial scales
	void enableAutoScale();
	/// @brief Disable automatic scaling for spatial scales
	void disableAutoScale();

	/// @brief Enable/disable tracking of scales states
	///
	/// If enabled, spacial scales states will be saved before each scale operation like zooming or panning.
	/// This allows undo/redo operations on scales states using undoScalesState() and redoScalesState().
	///
	/// The internal buffer of states is set to a maximum size of 10 states.
	/// Use setMaximumScalesStates() to increase/decrease this number.
	virtual void setTrackScalesStateEnabled(bool);
	/// @brief Undo the last scale operation
	void undoScalesState();
	/// @brief Redo the last scale oepration
	void redoScalesState();
	/// @brief Set the maximum number of buffered scales states.
	/// See setTrackScalesStateEnabled() for more details.
	void setMaximumScalesStates(int);

	/// @brief Bufferize the current scales states.
	/// If isTrackScalesStateEnabled() is true, this function is automatically called before any user interaction like mouse zooming/panning.
	/// Users can also call this function before programmatically setting the scales divisions/intervals to allow
	/// undoing the change with undoScalesState().
	void bufferScalesState();

	/// @brief Restore spatial scales state using a QByteArray previously generated with saveSpatialScaleState().
	void restoreSpatialScaleState(const QByteArray&);

	/// @brief Refresh the current content of displayed tool tip
	void refreshToolTip();

	/// @brief Recompute the area geometry.
	/// Should not be called directly
	virtual void recomputeGeometry(bool recompute_aligned_areas = true) = 0;

	/// @brief Enable/disable automatic geometry update
	/// Should not be called directly
	void setGeometryUpdateEnabled(bool enable);

Q_SIGNALS:

	/// Emitted when the mouse move over an item
	void mouseHoverMove(VipPlotItem*);

	/// Emitted when automatic scaling changed
	void autoScaleChanged(bool);

	/// Emitted when a VipPlotItemData's data changed
	void itemDataChanged(VipPlotItem*);

	/// Emitted when a new VipPlotItem is added to this area
	void itemAdded(VipPlotItem*);

	/// Emitted when a VipPlotItem is removed from this area
	void itemRemoved(VipPlotItem*);

	/// Emitted when a new scale is added to this area
	void scaleAdded(VipAbstractScale*);

	/// Emitted when a scale is removed from this area
	void scaleRemoved(VipAbstractScale*);

	/// Emitted when area title changed
	void titleChanged(const VipText&);

	/// Emitted when a child VipPlotItem changed
	void childItemChanged(VipPlotItem*);

	/// Emitted when a child VipPlotItem selection status changed
	void childSelectionChanged(VipPlotItem*);

	/// Emitted when a child VipPlotItem axis unit changed
	void childAxisUnitChanged(VipPlotItem*);

	/// Emitted when a child VipPlotItem received a mouse press event
	void mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton);

	/// Emitted when a child VipPlotItem received a mouse move event
	void mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton);

	/// Emitted when a child VipPlotItem received a mouse release event
	void mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton);

	/// Emitted when a child VipPlotItem received a double click event
	void mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton);

	/// Emitted when a child VipPlotItem received a key press event
	void keyPress(VipPlotItem*, qint64 id, int key, int modifiers);

	/// Emitted when a child VipPlotItem received a key release event
	void keyRelease(VipPlotItem*, qint64 id, int key, int modifiers);

	/// Emitted when a child VipPlotItem emitted a dropped signal
	void dropped(VipPlotItem* item, QMimeData* mime);

	/// Emitted when the tool tip is shown
	void toolTipStarted(const QPointF&);
	/// Emitted when the tool tip is moved
	void toolTipMoved(const QPointF&);
	/// Emitted when the tool tip is hidden
	void toolTipEnded(const QPointF&);

	/// Emitted when a mouse panning operation just ended
	void endMousePanning();
	/// Emitted when a mouse zooming operation just ended
	void endMouseZooming();
	/// Emitted when a wheel zooming operation just ended
	void endMouseWheel();
	// emitted whenever the scales are about to change due to a mouse operation (zoom, panning, wheel...)
	void mouseScaleAboutToChange();

private Q_SLOTS:

	void receivedDataChanged();
	void legendDestroyed(QObject*);
	void updateInternal();

protected Q_SLOTS:

	/// @brief Called when a new VipPlotItem is added to this area.
	/// New Implementations should call the base version of this function.
	virtual void addItem(VipPlotItem*);
	/// @brief Called when a VipPlotItem is removed from this area.
	/// New Implementations should call the base version of this function.
	virtual void removeItem(VipPlotItem*);

	/// @brief Called when a new scale is added to this area.
	/// Default implementation just set the scale parent to this area.
	virtual bool internalAddScale(VipAbstractScale*, bool isSpatialCoordinate = true);
	/// @brief Called when removing a scale from this area.
	virtual bool internalRemoveScale(VipAbstractScale*);

	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	// Reemit PlotItems's signals

	void mouseButtonPressed(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseButtonMoved(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseButtonReleased(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseButtonDoubleClicked(VipPlotItem*, VipPlotItem::MouseButton);
	void keyPressed(VipPlotItem*, qint64, int key, int modifiers);
	void keyReleased(VipPlotItem*, qint64, int key, int modifiers);
	void receiveChildChanged(VipPlotItem*);
	void receiveChildSelectionChanged(VipPlotItem*);
	void receiveChildAxisUnitChanged(VipPlotItem*);
	void receiveTitleChanged(const VipText&);
	void receiveDropped(VipPlotItem*, QMimeData*);

	/// @brief Reposition inner legends on geometry change
	virtual void resetInnerLegendsPosition();
	/// @brief Reset inner/outer legend based on style sheet parameters
	virtual void resetInnerLegendsStyleSheet();

protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual void wheelEvent(QGraphicsSceneWheelEvent* event);

	virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value);

	virtual void simulateMouseClick(const QGraphicsSceneMouseEvent* event);
	/// Returns the distance between the top scale and the bottom part of the title.
	/// Returns 0 by default. Derived classes should reimplement this to take into account title inside the plot area.
	/// This distance is used to compute the position of the inner legends.
	virtual double titleOffset() const { return 0; }

	/// @brief Apply the current color palette to all items
	void applyColorPalette();

private:
	void doUpdateScaleLogic();
	void markNeedUpdate();
	void markScaleDivDirty(VipAbstractScale*);
	bool markGeometryDirty();
	void applyLabelOverlapping();
	
	void setNotifier(const Vip::detail::ItemDirtyNotifierPtr & notifier);
	Vip::detail::ItemDirtyNotifierPtr notifier();

	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief A plotting area displaying 4 cartesian axes and suitable for most plot items
///
/// VipPlotArea2D is the "standard" plotting area used to represent plotting items like curves, histograms,...
/// in a cartesian system.
///
/// By default, VipPlotArea2D displays 4 axes (left right, top and bottom scales), a top title axis,
/// a bottom legend, an inner grid and canvas.
///
class VIP_PLOTTING_EXPORT VipPlotArea2D : public VipAbstractPlotArea
{
	Q_OBJECT

public:
	VipPlotArea2D(QGraphicsItem* parent = nullptr);
	virtual ~VipPlotArea2D();

	/// @brief Returns the left axis
	virtual VipAxisBase* leftAxis() const;
	/// @brief Returns the right axis
	virtual VipAxisBase* rightAxis() const;
	/// @brief Returns the top axis
	virtual VipAxisBase* topAxis() const;
	/// @brief Returns the bottom axis
	virtual VipAxisBase* bottomAxis() const;
	/// @brief Returns all spacial axes
	virtual QList<VipAxisBase*> axes() const;

	/// @brief Reimplemented from VipAbstractPlotArea
	virtual void recomputeGeometry(bool recompute_aligned_areas = true);
	/// @brief Reimplemented from VipAbstractPlotArea
	virtual void zoomOnSelection(const QPointF& start, const QPointF& end);
	/// @brief Reimplemented from VipAbstractPlotArea
	virtual void zoomOnPosition(const QPointF& pos, double factor);
	/// @brief Reimplemented from VipAbstractPlotArea
	virtual void translate(const QPointF& fromPt, const QPointF& dp);
	/// @brief Reimplemented from VipAbstractPlotArea
	virtual QPainterPath innerArea() const;
	/// @brief Reimplemented from VipAbstractPlotArea
	virtual VipCoordinateSystem::Type standardScales(QList<VipAbstractScale*>& axes) const;

	/// @brief Retruns the area outer rectangle
	QRectF outerRect() const;
	/// @brief Returns the are inner rectangle, i.e. the canvas area
	QRectF innerRect() const;

protected:
	bool internalRemoveScale(VipAbstractScale* scale);
	virtual double titleOffset() const;

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief A plotting area displaying a polar and radial axis
///
/// VipPlotPolarArea2D is used to represent plotting items in a polar coordinate system like pie charts.
///
/// By default, VipPlotPolarArea2D displays a polar axis, a radial axis, a top title axis,
/// a bottom legend, an inner grid and canvas.
///
/// VipPlotPolarArea2D supports adding additional border scales (inherting VipBorderItem), in which case
/// the polar area is organized inside the area defined by the border scales. For instance, the title axis is
/// a Top border scale.
///
/// VipPlotPolarArea2D also supports adding extra polar or radial scales.
///
/// VipPlotPolarArea2D supports stylesheets and adds the following attributes:
/// -	'inner-margin': floating point value equivalent to VipPlotPolarArea2D::setInnerMargin()
///
class VIP_PLOTTING_EXPORT VipPlotPolarArea2D : public VipAbstractPlotArea
{
	Q_OBJECT

public:
	VipPlotPolarArea2D(QGraphicsItem* parent = nullptr);
	virtual ~VipPlotPolarArea2D();

	/// @brief Set the space between the inner most border scale and the outer most polar scale
	void setInnerMargin(double margin);
	double innerMargin() const;

	/// @brief Returns the default polar axis
	VipPolarAxis* polarAxis() const;
	/// @brief Returns the default radial axis
	VipRadialAxis* radialAxis() const;
	/// @brief Returns all spacial polar or radial scales
	QList<VipAbstractPolarScale*> axes() const;

	virtual void recomputeGeometry(bool recompute_aligned_areas = true);
	virtual void zoomOnSelection(const QPointF& start, const QPointF& end);
	virtual void zoomOnPosition(const QPointF& pos, double factor);
	virtual void translate(const QPointF& fromPt, const QPointF& dp);
	virtual QPainterPath innerArea() const;
	virtual VipCoordinateSystem::Type standardScales(QList<VipAbstractScale*>& axes) const;

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Cartesian plotting area displaying an image
///
/// VipImageArea2D is a VipPlotArea2D customized to display
/// color images or spectrograms based on an internal VipPlotSpectrogram.
///
/// By default, VipImageArea2D keeps the image aspect ratio, which means
/// that the displayed image (and surrounding axes) does not necessarly
/// take the full item geometry.
///
/// VipImageArea2D provides left, right, top and bottom scales, a title axis
/// and a color map displayed on the right.
///
/// Mouse panning and wheel zooming are bounded to the image size.
///
/// VipImageArea2D supports stylesheets and adds the following attributes:
/// -	'keep-aspect-ratio': boolean value equivalent to VipImageArea2D::setKeepAspectRatio()
///
class VIP_PLOTTING_EXPORT VipImageArea2D : public VipPlotArea2D
{
	Q_OBJECT

public:
	VipImageArea2D(QGraphicsItem* parent = nullptr);
	virtual ~VipImageArea2D();

	/// @brief Enable/disable keeping the image aspect ratio
	void setKeepAspectRatio(bool enable);
	bool keepAspectRatio() const;

	/// @brief Set the spectrogram to be used and delete the previous one
	void setSpectrogram(VipPlotSpectrogram* spectrogram);
	/// @brief Set the color map axis to be used and delete the previous one
	void setAxisColorMap(VipAxisColorMap* map);

	/// @brief Returns the image bounding rectangle.
	/// Equivalent to spectrogram()->imageBoundingRect().
	QRectF imageBoundingRect() const;

	/// @brief Returns the image rectangle
	/// The image rectangle is its bounding rectangle with the left and top position set to 0
	QRectF imageRect() const;

	/// @brief Returns the currently visualized image rectangle
	QRectF visualizedImageRect() const;

	/// @brief Returns the current zoom factor
	/// The zoom factor is equivalent to the size of one pixel in item's coordinates/
	double zoom() const;

	/// @brief Set the array to be displayed
	void setArray(const VipNDArray& ar, const QPointF& image_offset);
	/// @brief Set the image to be displayed
	void setImage(const QImage& image, const QPointF& image_offset);
	/// @brief Set the pixmap to be displayed
	/// The pixmap is internally converted to QImage
	void setPixmap(const QPixmap& image, const QPointF& image_offset);

	/// @brief Returns the currently displayed array
	VipNDArray array() const;
	/// @brief Returns the internal spectrogram
	VipPlotSpectrogram* spectrogram() const;
	/// @brief Returns the color map axis
	VipAxisColorMap* colorMapAxis() const;

	/// @brief Reimplemented from VipPlotArea2D
	virtual void recomputeGeometry(bool recompute_aligned_areas = true);

public Q_SLOTS:
	/// @brief Set the visualized image rectangle.
	/// The result might differ depending if keepAspectRatio() is true or not.
	void setVisualizedImageRect(const QRectF&);

Q_SIGNALS:
	/// @brief Emitted when the visualized image rectangle changed
	void visualizedAreaChanged();

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index);

private Q_SLOTS:
	void emitVisualizedAreaChanged();
	void receiveNewRect(const QRectF&);

private:
	void recomputeGeometry(const QRectF& visualized_image_rect, bool recompute_aligned_areas = true);

	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Base class for Thermavip QGraphicsView widgets
///
/// VipBaseGraphicsView is the base class for all QGraphicsView widgets within Thermavip.
/// A VipBaseGraphicsView can contain one unique VipAbstractPlotArea like subclasses
/// VipPlotWidget2D, VipImageWidget2D and VipPlotPolarWidget2D, or might contain
/// several areas when using a QGraphicsLayout (see VipMultiGraphicsView class).
///
/// VipBaseGraphicsView mainly provides 2 featrues:
/// -	Easy use of opengl rendering with VipBaseGraphicsView::setRenderingMode()
/// -	Definition of a background color with the property 'backgroundColor'
///
/// VipBaseGraphicsView is a VipRenderObject and its content can be saved as an image or pdf/ps/svg file
/// (see VipRenderObject class for more details).
///
class VIP_PLOTTING_EXPORT VipBaseGraphicsView
  : public QGraphicsView
  , public VipRenderObject
{
	Q_OBJECT
	Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)

public:
	/// @brief Rendering mode, modify the viewport widget
	enum RenderingMode
	{
		Raster,	     // Use a QWidget viewport
		OpenGL,	     // Use a QOpenGLWidget viewport
		OpenGLThread // Use a QThreadOpenGLWidget
	};
	VipBaseGraphicsView(QWidget* parent = nullptr);
	VipBaseGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);
	virtual ~VipBaseGraphicsView();

	/// @brief Returns the part of the scene that is currently visualized by this QGraphicsView
	QRectF visualizedSceneRect() const;

	/// @brief Set the rendering mode
	///
	/// The mode could be one of:
	/// -	Raster: use a QWidget viewport,
	/// -	OpenGL: use a QOpenGLWidget viewport,
	/// -	OpenGLThread: use a QThreadOpenGLWidget viewport (fastest rendering)
	///
	/// Note that for opengl rendering, the scales cache mode is set to QGraphicsItem::DeviceCoordinateCache,
	/// if VIP_CUSTOM_ITEM_CACHING is defined. Otherwise (default), the user is responsible of the items caching strategy.
	///
	void setRenderingMode(RenderingMode mode);
	RenderingMode renderingMode() const;
	bool isOpenGLBasedRendering() const;

	/// @brief Enable/disable usage of custom viewport.
	/// If true, setRenderingMode() has no effect.
	void setUseInternalViewport(bool);
	bool useInternalViewport() const;

	/// @brief Set/unset/get the view background color
	QColor backgroundColor() const;
	bool hasBackgroundColor() const;

public Q_SLOTS:

	void removeBackgroundColor();
	void setBackgroundColor(const QColor& color);

Q_SIGNALS:
	void viewportChanged(QWidget* w);

protected:
	/// Any sub class reimplementing this function should call this function ensure the built-in compatibility.
	virtual void startRender(VipRenderState&);
	virtual void endRender(VipRenderState&);
	virtual bool renderObject(QPainter* p, const QPointF& pos, bool draw_background);
	virtual void paintEvent(QPaintEvent* evt);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void setupViewport(QWidget* viewport);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Base class for QGraphicsView that only contain one VipAbstractPlotArea
///
/// VipAbstractPlotWidget2D is the base class for all 2D plotting widgets containing a unique plot area,
/// like VipPlotWidget2D, VipImageWidget2D and VipPlotPolarWidget2D.
///
class VIP_PLOTTING_EXPORT VipAbstractPlotWidget2D : public VipBaseGraphicsView
{
	Q_OBJECT

public:
	VipAbstractPlotWidget2D(QWidget* parent = nullptr);
	VipAbstractPlotWidget2D(QGraphicsScene* scene, QWidget* parent = nullptr);

	/// @brief Returns the managed VipAbstractPlotArea
	virtual VipAbstractPlotArea* area() const;
	/// @brief Helper function, equivalent to area()->createColorMap()
	virtual VipAxisColorMap* createColorMap(VipAxisBase::Alignment, const VipInterval&, VipColorMap*);

	/// @brief Recompute the area geometry, classed in resizeEvent()
	virtual void recomputeGeometry() = 0;

protected:
	/// Any sub class reimplementing this function should call this function ensure the built-in compatibility.
	virtual void setArea(VipAbstractPlotArea* area);

	virtual void resizeEvent(QResizeEvent* event);

private:
	QPointer<VipAbstractPlotArea> d_area;
};

/// @brief A VipAbstractPlotWidget2D that contains a VipPlotArea2D
///
/// Based on the AreaType passed to the constructor, VipPlotWidget2D
/// can manage a VipVMultiPlotArea2D instead of the default VipPlotArea2D.
///
class VIP_PLOTTING_EXPORT VipPlotWidget2D : public VipAbstractPlotWidget2D
{
	Q_OBJECT;

public:
	enum AreaType
	{
		Simple, // VipPlotArea2D object
		VMulti	// VipVMultiPlotArea2D object
	};

	VipPlotWidget2D(QWidget* parent = nullptr, QGraphicsScene* scene = nullptr, AreaType type = Simple);
	virtual ~VipPlotWidget2D();

	virtual VipPlotArea2D* area() const;
	virtual void setArea(VipAbstractPlotArea*);

	virtual void recomputeGeometry();

private:
	VipPlotArea2D* d_area;
};

/// @brief A VipAbstractPlotWidget2D that contains a VipPlotPolarArea2D
///
class VIP_PLOTTING_EXPORT VipPlotPolarWidget2D : public VipAbstractPlotWidget2D
{
	Q_OBJECT;

public:
	VipPlotPolarWidget2D(QWidget* parent = nullptr, QGraphicsScene* scene = nullptr);
	virtual ~VipPlotPolarWidget2D();

	virtual VipPlotPolarArea2D* area() const;
	void setArea(VipAbstractPlotArea* area);
	virtual void recomputeGeometry();

private:
	VipPlotPolarArea2D* d_area;
};

/// @brief A VipAbstractPlotWidget2D that contains a VipImageArea2D
///
class VIP_PLOTTING_EXPORT VipImageWidget2D : public VipAbstractPlotWidget2D
{
	Q_OBJECT

public:
	VipImageWidget2D(QWidget* parent = nullptr, QGraphicsScene* scene = nullptr);
	~VipImageWidget2D();

	VipImageArea2D* area() const;
	virtual void recomputeGeometry();

	/// @brief Enable/disable scroll bars.
	/// If true, vertical and/or horizontal scroll bars will be displayed
	/// when the visualized image rectangle is smaller than the full image rectangle.
	void setScrollBarEnabled(bool enable);
	bool scrollBarEnabled() const;

protected:
	/// Overload mouse events to move the scroll bars when the user is trying to select an area close to the borders
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);

private Q_SLOTS:

	void computeScrollBars();
	void vScrollBarsMoved();
	void hScrollBarsMoved();
	void mouseTimer();

private:
	VipImageArea2D* d_area;
	QTimer* d_timer;
	bool d_mouseInsideCanvas;
	bool d_scrollBarEnabled;
};

class QGridLayout;

/// @brief Graphics item used in VipMultiGraphicsView as the top level widget item.
///
/// This class is only provided in order to be customized through style sheets.
///
class VIP_PLOTTING_EXPORT VipMultiGraphicsWidget : public VipBoxGraphicsWidget
{
	Q_OBJECT

public:
	VipMultiGraphicsWidget(QGraphicsItem* parent = nullptr);
};

/// @brief VipMultiGraphicsView is a QGraphicsView widget containing a unique VipBoxGraphicsWidget that is always resized to fit the display area.
///
/// VipMultiGraphicsView contains one unique QGraphicsWidget item (VipBoxGraphicsWidget) that is resized to fit the QGraphicsView display area.
/// This QGraphicsWidget should be used to add a layout with multiple VipAbstractPlotArea in order to display several plots within the same widget.
///
/// Usage:
/// \code{cpp}
///
/// #include "VipPlotWidget2D.h"
///
/// #include <qapplication.h>
/// #include <qgraphicslinearlayout.h>
///
/// int main(int argc, char** argv)
/// {
/// 	QApplication app(argc, argv);
///
/// 	VipMultiGraphicsView w;
///
/// 	// Create 2 cartesian plotting area and a polar one
/// 	VipPlotArea2D* area1 = new VipPlotArea2D();
/// 	VipPlotArea2D* area2 = new VipPlotArea2D();
/// 	VipPlotPolarArea2D* area3 = new VipPlotPolarArea2D();
///
/// 	// Set the scale of one cartesian area, and aligned it vertically with the other
/// 	area2->leftAxis()->setScale(10000, 100000);
/// 	area1->setAlignedWith(area2, Qt::Vertical);
///
/// 	// Stack areas vertically in a QGraphicsLinearLayout
/// 	QGraphicsLinearLayout* lay = new QGraphicsLinearLayout(Qt::Vertical);
/// 	w.widget()->setLayout(lay);
/// 	lay->addItem(area1);
/// 	lay->addItem(area2);
/// 	lay->addItem(area3);
///
/// 	w.resize(500, 1000);
/// 	w.show();
/// 	return app.exec();
/// }
/// \endcode
///
class VIP_PLOTTING_EXPORT VipMultiGraphicsView : public VipBaseGraphicsView
{
	Q_OBJECT

	VipMultiGraphicsWidget* d_widget;

public:
	/// @brief Default constructor
	VipMultiGraphicsView(QWidget* parent = nullptr);
	/// @brief Construct from a QGraphicsScene object
	VipMultiGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);

	/// @brief Returns the top level VipBoxGraphicsWidget used to represent multiple plotting area organized in a layout
	VipMultiGraphicsWidget* widget() const;

protected:
	virtual void resizeEvent(QResizeEvent* event);
};

VIP_REGISTER_QOBJECT_METATYPE(VipMultiGraphicsWidget*)
VIP_REGISTER_QOBJECT_METATYPE(VipAbstractPlotArea*)
VIP_REGISTER_QOBJECT_METATYPE(VipPlotArea2D*)
VIP_REGISTER_QOBJECT_METATYPE(VipPlotPolarArea2D*)
VIP_REGISTER_QOBJECT_METATYPE(VipImageArea2D*)
VIP_REGISTER_QOBJECT_METATYPE(VipBaseGraphicsView*)
VIP_REGISTER_QOBJECT_METATYPE(VipMultiGraphicsView*)
VIP_REGISTER_QOBJECT_METATYPE(VipPlotWidget2D*)
VIP_REGISTER_QOBJECT_METATYPE(VipPlotPolarWidget2D*)
VIP_REGISTER_QOBJECT_METATYPE(VipImageWidget2D*)

// TODO: add serialization functions for all types

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotArea2D* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotArea2D* value);

/// @}
// end Plotting

#endif
