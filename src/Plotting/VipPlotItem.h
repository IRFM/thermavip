#ifndef VIP_PLOT_ITEM_H
#define VIP_PLOT_ITEM_H

#include <QGraphicsObject>
#include <QMutex>
#include <QPainter>
#include <QPicture>
#include <QVector>
#include <qset.h>

#include "VipCoordinateSystem.h"
#include "VipDataType.h"
#include "VipGlobals.h"
#include "VipInterval.h"
#include "VipRenderObject.h"
#include "VipStyleSheet.h"
#include "VipText.h"

/// \addtogroup Plotting
/// @{

/// Convert a list of QGraphicsItem into a list of \a T (inheriting
/// QGraphicsObject). If \a name is provided, only items having this object name
/// are returned. If \a visible is 1 or 0, only visible or hidden items are
/// returned. If \a selection is 1 or 0, only seleced/unselected items are
/// returned.
template<class T, class U>
QList<T> vipCastItemList(const QList<U>& lst, const QString& name = QString(), int selection = 2, int visible = 2)
{
	QList<T> res;
	for (int i = 0; i < lst.size(); ++i)
		if (QGraphicsItem* it = lst[i])
			if (QGraphicsObject* obj = it->toGraphicsObject())
				if (T tmp = qobject_cast<T>(obj))
					if (name.isEmpty() || name == tmp->objectName())
						if (selection == 2 || selection == (int)tmp->isSelected())
							if (visible == 2 || visible == (int)tmp->isVisible())
								res << tmp;

	return res;
}

// Same as #vipCastItemList, but sort the items using
// #VipPlotItem::selectionOrder()
template<class T, class U>
QList<T> vipCastItemListOrdered(const QList<U>& lst, const QString& name = QString(), int selection = 2, int visible = 2)
{
	QList<T> res = vipCastItemList<T>(lst, name, selection, visible);
	if (res.size() < 2)
		return res;

	QMap<int, T> tmp;
	for (typename QList<T>::iterator it = res.begin(); it != res.end(); ++it) {
		tmp[(*it)->selectionOrder()] = *it;
	}
	return tmp.values();
}

/// Convert a list of QGraphicsItem into a list of \a T (inheriting
/// QGraphicsObject). If \a title is provided, only items having this title are
/// returned. If \a visible is 1 or 0, only visible or hidden items are
/// returned. If \a selection is 1 or 0, only seleced/unselected items are
/// returned.
template<class T, class U>
QList<T> vipCastItemListTitle(const QList<U>& lst, const QString& title = QString(), int selection = 2, int visible = 2)
{
	QList<T> res;
	for (int i = 0; i < lst.size(); ++i)
		if (QGraphicsItem* it = lst[i])
			if (QGraphicsObject* obj = it->toGraphicsObject())
				if (T tmp = qobject_cast<T>(obj))
					if (title.isEmpty() || title == tmp->title().text())
						if (selection == 2 || selection == (int)tmp->isSelected())
							if (visible == 2 || visible == (int)tmp->isVisible())
								res << tmp;

	return res;
}

/// Same as #vipCastItemListTitle, but sort the items using #VipPlotItem::selectionOrder()
template<class T, class U>
QList<T> vipCastItemListTitleOrdered(const QList<U>& lst, const QString& title = QString(), int selection = 2, int visible = 2)
{
	QList<T> res = vipCastItemListTitle<T>(lst, title, selection, visible);
	if (res.size() < 2)
		return res;

	QMap<int, T> tmp;
	for (typename QList<T>::iterator it = res.begin(); it != res.end(); ++it) {
		tmp[(*it)->selectionOrder()] = *it;
	}
	return tmp.values();
}

class QMimeData;
class QGraphicsView;
class VipAxisColorMap;
class VipAbstractScale;
class VipAbstractPlotArea;
class VipPlotItem;
class VipShapeDevice;
class VipColorPalette;

/// Dynamic property for a #VipPlotItem object used for tool tip display.
/// Overload the function value() to returns a custom string based on a
/// coordinate in item's coordinate system. If returns QString is not empty, it
/// will be displayed in the tool tip by replacing the sub-string '#dname'.
///
class VIP_PLOTTING_EXPORT VipPlotItemDynamicProperty
{
	friend class VipPlotItem;

public:
	VipPlotItemDynamicProperty(const QString& name);
	virtual ~VipPlotItemDynamicProperty();

	virtual QString value(const QPointF& pos, VipCoordinateSystem::Type type) = 0;
	VipPlotItem* parentItem() const;
	QString name() const;

private:
	class PrivateData;
	PrivateData* d_data;
};

/// @brief Base class for drawing items (VipPlotItem, VipAbstractScale...)
///
/// VipPaintItem is the base class for most drawing items within Thermavip.
/// Its main goal is to provide a style sheet mechanism to all drawing items
/// like VipPlotItem, VipAbstractScale, VipAbstractPlotArea...
/// 
/// By default, VipPaintItem supports the following properties:
/// -	'qproperty-name': set the QObject property 'name'
/// -	'render-hint': one of 'antialiasing', 'highQualityAntialiasing' or 'noAntialiasing'
/// -	'composition-mode': one of 'compositionMode_SourceOver', 'compositionMode_DestinationOver', ... (see QPainter documentation)
/// -	'title': text value
/// -	'title-font': title font
/// -	'title-font-size': title font size
/// -	'title-font-style': title font style
/// -	'title-font-weight': title font weight
/// -	'title-font-family': title font familly
/// -	'title-text-border': title text box border pen
/// -	'title-text-border-radius': title text box border radius
/// -	'title-text-background': title text box background
/// -	'title-text-border-margin': title text box border margin
/// -	'selected': boolean value
/// -	'visible': boolean value
///
class VIP_PLOTTING_EXPORT VipPaintItem
{
	friend class VipAbstractPlotArea;
	friend class VipPlotItem;
	friend class VipBoxGraphicsWidget;
	friend class VipRubberBand;
	friend VIP_PLOTTING_EXPORT bool vipApplyStyleSheet(const VipStyleSheet& p, VipPaintItem* item, QString* error);

public:
	VipPaintItem(QGraphicsObject* obj);
	virtual ~VipPaintItem();

	/// @brief Enable/disable item rendering, even if the item is visible
	void setPaintingEnabled(bool enable);
	bool paintingEnabled() const;
	/// @brief Returns the item as a QGraphicsObject
	QGraphicsObject* graphicsObject() const;

	VipStyleSheet setStyleSheet(const QString& stylesheet);
	void setStyleSheet(const VipStyleSheet& stylesheet);
	QString styleSheetString() const;
	const VipStyleSheet& styleSheet() const;
	const VipStyleSheet& constStyleSheet() const;
	VipStyleSheet& styleSheet();

	/// @brief Make this item ignore style sheets
	/// If enabled, the item won't apply style sheets to itself and won't propagate them.
	/// This has other side effects: for instance, a VipAbstractPlotArea won't apply its color palette to a VipPlotItem with ignoreStyleSheet()==true.
	///
	/// This function can be overriden, but the new implementation must call the base version.
	virtual void setIgnoreStyleSheet(bool);
	bool ignoreStyleSheet() const;

	/// @brief Update the style sheet string based on the internal VipStyleSheet object
	void updateStyleSheetString();

	/// @brief Set item's title (which is usually displayed in the legend for VipPlotItem)
	virtual void setTitle(const VipText& title);
	/// Returns the item's title.
	/// The title is used in the plot legend.
	///
	/// \sa VipLegend
	const VipText& title() const;
	/// @brief Remove title
	virtual void clearTitle();

	/// Set the item render hints used in #draw() function.
	virtual void setRenderHints(QPainter::RenderHints);
	/// Returns the item's render hints.
	QPainter::RenderHints renderHints() const;

	/// Set the item composition mode used in #draw() function.
	virtual void setCompositionMode(QPainter::CompositionMode);
	/// Returns the item's composition mode.
	QPainter::CompositionMode compositionMode() const;

	/// @brief Returns children VipPaintItem
	virtual QList<VipPaintItem*> paintItemChildren() const;

protected:
	/// @brief Set a property based on its name and an optional index value.
	/// @return true on success, false otherwise.
	/// 
	/// In order to define custom properties for style sheets, you must reimplement this function
	/// and call the base version.
	///
	/// By default, VipPaintItem supports the following properties:
	/// -	'qproperty-name': set the QObject property 'name'
	/// -	'render-hint': one of 'antialiasing', 'highQualityAntialiasing' or 'noAntialiasing'
	/// -	'composition-mode': one of 'compositionMode_SourceOver', 'compositionMode_DestinationOver', ... (see QPainter documentation)
	/// -	'title': text value
	/// -	'selected': boolean value
	/// -	'visible': boolean value
	/// 
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	/// @brief Update the VipPaintItem when its stylesheet is updated
	virtual void updateOnStyleSheet();
	/// @brief Check if item has all defined states (like 'hover', '!selected', ...)
	/// Default implementation just check for 'hover' and 'selected'
	virtual bool hasState(const QByteArray& state, bool enable) const;

	bool hasStates(const QSet<QByteArray>& states) const;

	/// @brief Dispatch this item's stylesheet to its children.
	/// This member is automatically called when setting the item stylesheet,
	/// Or when a new child is added to this item. Internally call paintItemChildren().
	/// You might need to call it yourself if you manage a custom children/parent relationship (like VipPlotItemComposite)
	void dispatchStyleSheetToChildren();

	/// @brief Marke the style sheet as dirty
	/// Call this function when setting a new parameter to the pain item the stylesheet must be reapplied.
	/// Call this function also when the state of the paint item changed and can potentialy modify the
	/// applied style sheet whith custom selectors (like whe hovering on the item)
	void markStyleSheetDirty();

	/// @brief Reapply the style sheet if marked dirty (through markStyleSheetDirty())
	/// Should be called in paint() function.
	void applyStyleSheetIfDirty() const;

private:
	void updateInternal();
	void setInheritedStyleSheet(const VipStyleSheet& stylesheet);
	void internalDispatchStyleSheet(const VipStyleSheet& sheet) const;
	bool internalSetStyleSheet(const QByteArray& ar);
	bool internalApplyStyleSheet(const VipStyleSheet& sheet, const VipStyleSheet& inherited);

	class PrivateData;
	PrivateData* d_data;
};

Q_DECLARE_METATYPE(VipPaintItem*)

/// @brief VipPlotItem is the base class for all plotting items: curves, histograms, spectrograms, pie charts...
///
/// VipPlotItem is a plotting item that relies on one or more axes (usually) 2 to draw its content.
///
/// Only a few members must be overridden yo have a valid plot item:
/// -	setPen() and pen(): set/get the pen used to draw the outline of this item. These functions are mandatory to be responsive to stylesheets.
/// -	setBrush() and brush(): set/get the brush used to draw the inner part of this item. These functions are mandatory to be responsive to stylesheets.
/// -	void draw(QPainter*, const VipCoordinateSystemPtr&): draw the item's content using a coordinate system. The coordinate system is used to map coordinates in axis unit
/// to paint device coordinates.
///
/// Since VipPlotItem is a QGraphicsItem, it overrides some members of QGraphicsItem like shape() and boundingRect().
/// By default, the bounding rect is extracted from the shape, and the shape is computed using the draw() member.
/// It is possible to override these members when subclassing VipPlotItem, or to override the function shapeFromCoordinateSystem
/// to benefit from internal VipPlotItem shape caching mechanism.
///
/// VipPlotItem objects are aware of their parent VipBaseGraphicsView stylesheets that can set their pen, brush, color palette or text style (font, size and color).
///
/// In addition to drawing its content with draw() member, a VipPlotItem can display additional static text data using member addText() function.
///
/// VipPlotItem supports stylesheets, and adds the following attributes:
///	-	'selection-border': border pen when selected (see VipPlotItem::setSelectedPen())
/// -	'border': border pen (see VipPlotItem::setPen())
/// -	'border-width': border pen width (see VipPlotItem::setPen())
/// -	'major-color': set item color (both border and background), see VipPlotItem::setMajorColor()
/// -	'colormap': set item color map, one of 'cool', 'jet',...
/// -	'colormap-title': set the item colormap title
/// -	'colorpalette': set item color palette, one of 'random', 'pastel',...
/// -	'axis-unit' set axis unit (and therefore axis title) for given axis index. Example: 'axis-unit[0]: "Time (s)"'
/// -	'tooltip': set item tool tip text (see VipPlotItem::setToolTipText())
/// -	'attributes': set item attributes, combination of 'hasLegendIcon|visibleLegend...'
/// -	'attribute': set/unset one item attribute. Example: 'attribute[hasLegendIcon]: false'
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
class VIP_PLOTTING_EXPORT VipPlotItem
  : public QGraphicsObject
  , public VipPaintItem
  , public VipRenderObject
{
	Q_OBJECT

	Q_ENUMS(MouseButton)

	friend class VipAbstractScale;
	friend class VipAbstractPlotArea;

public:
	/// Small structure to store static text used with #VipPlotItem::addText()
	struct ItemText
	{
		VipText text;
		Vip::RegionPositions position;
		Qt::Alignment alignment;

		ItemText(const VipText& text = VipText(), Vip::RegionPositions pos = Vip::Automatic, Qt::Alignment align = Qt::AlignCenter)
		  : text(text)
		  , position(pos)
		  , alignment(align)
		{
		}
	};
	/// Tells if the last QEvent processed by the scene has been accepted by a
	/// VipPlotItem. This might be used in the parent QGraphicsView when
	/// reimplementing its event handlers. Since, at least for QInputEvent types
	/// (like mouse events), QGraphicsScene accept the event by default, use
	/// VipPlotItem::eventAccepted() to check if the event really has been
	/// accepted by a QGraphicsItem.
	static bool eventAccepted();
	static void setEventAccepted(bool accepted);

	/// Function typedef.
	/// The function must return a new QGraphicsEffect based on a VipPlotItem.
	typedef QGraphicsEffect* (*create_effect_type)(VipPlotItem*);

	/// Function used to create the selection pen based on the item's pen.
	/// Used in #VipPlotItem::selectedPen.
	///
	/// \sa VipPlotItem::selectedPen VipPlotItem::setSelectionPenCreator
	typedef QPen (*create_selection_pen)(const VipPlotItem*, const QPen&);

	/// Default create_effect_type function, returns a NULL QGraphicsEffect.
	static QGraphicsEffect* nullEffect(VipPlotItem*);
	/// Default create_selection_pen function, returns a pen semi transparent and
	/// wider than given pen.
	static QPen defaultSelectionPen(const VipPlotItem*, const QPen&);

	/// \brief Plot Item Attributes
	///
	/// Various aspects of a plot widget depend on the attributes of
	/// the attached plot items. If and how a single plot item
	/// participates in these updates depends on its attributes.
	///
	/// \sa setItemAttribute(), testItemAttribute(), ItemInterest
	enum ItemAttribute
	{
		/// @brief The item has a legend icon
		HasLegendIcon = 0x0001,

		/// @brief The item is represented on the legend.
		VisibleLegend = 0x0002,

		/// @brief  the item is included in the autoscaling calculation
		AutoScale = 0x0004,

		/// @brief  The plotInterval() of the item is included in the color map
		/// autoscaling calculation as long as its width or height is >= 0.0.
		ColorMapAutoScale = 0x0008,

		/// @brief Clip the plot item drawing to its scale clip path (as return by
		/// VipCoordinateSystem::clipPath() )
		ClipToScaleRect = 0x0020,

		/// @brief The plot item supports transformation through
		/// VipPlotItem::applyTransform()
		SupportTransform = 0x0040,

		/// @brief The item can be drag and dropped into another one
		Droppable = 0x0080,

		/// @brief The item displays a tool tip
		HasToolTip = 0x0100,

		/// @brief The item tool tip must only be the custom one set with
		/// #setToolTipText()
		CustomToolTipOnly = 0x0200,

		/// @brief The item ignore mouse events and propagate them to the underneath
		/// items
		IgnoreMouseEvents = 0x0400,

		/// @brief If the item is selected, call VipPlotItem::drawSelected every
		/// 200ms. This can be used to perform additional effects on selection.
		HasSelectionTimer = 0x0800,

		/// @brief The item can be suppressdd through the Del key
		IsSuppressable = 0x1000,

		/// @brief This items support dropping items and will set there axes to its
		/// axes.
		AcceptDropItems = 0x2000,

	};

	//! Plot Item Attributes
	typedef QFlags<ItemAttribute> ItemAttributes;

	/// @brief Mouse button enum, used in signals
	enum MouseButton
	{
		LeftButton = Qt::LeftButton,
		MiddleButton = Qt::MiddleButton,
		RightButton = Qt::RightButton
	};

	/// Constructor.
	/// \a title: optional item's title.
	VipPlotItem(const VipText& title = VipText());
	virtual ~VipPlotItem();

	/// Returns the parent #VipAbstractPlotArea or NULL if a valid parent cannot
	/// be found. This function scans all item's parents until finding a
	/// #VipAbstractPlotArea object.
	///
	/// \sa VipAbstractPlotArea
	VipAbstractPlotArea* parentPlotArea() const;

	/// Set the item's attributes.
	void setItemAttributes(ItemAttributes);

	/// Enable/disable an item's attribute.
	void setItemAttribute(ItemAttribute, bool on = true);

	/// Returns true if given attribute is true, false otherwise.
	bool testItemAttribute(ItemAttribute) const;

	/// Returns all item's attributes.
	ItemAttributes itemAttributes() const;

	/// Set the create_effect_type used to attach a QGraphicsEffect to this item
	/// when hovered.
	void setHoverEffect(create_effect_type function = nullEffect);
	/// Set the create_effect_type used to attach a QGraphicsEffect to this item
	/// when selected.
	void setSelectedEffect(create_effect_type function = nullEffect);
	/// Set the create_effect_type used to attach a QGraphicsEffect to this item
	void setStandardEffect(create_effect_type function = nullEffect);

	void setClipTo(QGraphicsObject*);
	QGraphicsObject* clipTo() const;

	/// Set an item property.
	/// This function mainly used for style sheets, but can also be called
	/// directly by the user. Each subclass of VipPlotItem can override this
	/// function to provide additional properties, but the overload must call the
	/// base function first, and handle the property if the base implementation
	/// cannot.
	/// \sa setStyleSheet
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

	/// Returns the pen used to highlight the item when selected.
	///
	/// \sa setSelectedPen, setSelectionPenCreator, setPen
	QPen selectedPen() const;
	/// Set the global item pen (if any)
	/// \sa pen
	virtual void setPen(const QPen& p) = 0;
	/// Returns the global item pen (if any)
	/// \sa setPen
	virtual QPen pen() const = 0;

	/// Set the global item brush (if any)
	/// \sa bursh
	virtual void setBrush(const QBrush& b) = 0;
	/// Returns the global item brush (if any)
	/// \sa setBrush
	virtual QBrush brush() const = 0;

	/// Returns the item's major color. Usually (and by default), this is the pen
	/// color.
	virtual QColor majorColor() const { return pen().color(); }
	/// Set the item global color. This is a convinient way to set the item's
	/// color theme in one call. By default, its sets the pen and brush color to
	/// given color.
	virtual void setMajorColor(const QColor& c)
	{
		QPen p = pen();
		p.setColor(c);
		setPen(p);
		QBrush b = brush();
		setBrush(b);
	}

	/// @brief Set the item color palette. Default implementation does nothing.
	/// This is usefull for some VipPlotItem like VipPieChart or VipPlotBarChart that rely on color palettes
	/// in order to be responsive to stylesheets.
	virtual void setColorPalette(const VipColorPalette&) {}
	virtual VipColorPalette colorPalette() const;

	/// @brief Set the item text style for VipPlotItem drawing text.
	/// This is usefull for some VipPlotItem like VipPieChart, VipPlotBarChart or VipPlotHistogram that might draw text
	/// in order to be responsive to stylesheets.
	virtual void setTextStyle(const VipTextStyle&) {}
	virtual VipTextStyle textStyle() const { return VipTextStyle(); }

	/// Set the selection pen creation function.
	/// If the function is not NULL and returns a valid pen, it will be used to
	/// draw the item outline when selected.
	void setSelectionPenCreator(create_selection_pen p);

	/// Set the item's color map.
	/// The color map is used to draw the color with a given color based on an
	/// item's value.
	///
	/// \sa colorMap
	virtual void setColorMap(VipAxisColorMap* colorMap);
	/// Returns the item's color map
	VipAxisColorMap* colorMap() const;
	/// Helper function.
	/// Returns the color for given value based on the color map.
	/// If no color map is provided for this item, returns \a default_color.
	QRgb color(double value, QRgb default_color = 0) const;
	/// Helper function.
	/// Returns the color for given value based on the color map.
	/// If no color map is provided for this item, returns \a default_color.
	QRgb color(double value, const QColor& default_color) const;

	/// Returns the interval of values this item provides.
	/// This function is used for the item's color map auto scaling.
	/// \a interval is the validity interval in which values should be. This means
	/// that this function must compute the range of values inside given interval.
	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	/// Set the item's axes and coordinate system.
	/// This will set the item's parent as the parent of the axes.
	/// You must call this function to draw the item based on 2 (or more) axes.
	///
	/// \sa axes, coordinateSystemType
	virtual void setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type);
	/// Overload function.
	/// Set the item's axes and coordinate system.
	void setAxes(VipAbstractScale* x, VipAbstractScale* y, VipCoordinateSystem::Type type);
	/// Returns the item's axes.
	QList<VipAbstractScale*> axes() const;
	/// Returns the item's axis coordinate system.
	VipCoordinateSystem::Type coordinateSystemType() const;

	/// Set the item's unit for given axis index.
	/// If an axis is set for given index, this will set its title.
	void setAxisUnit(int index, const VipText& unit);
	/// Returns the axis unit for given index.
	const VipText& axisUnit(int index) const;
	/// Returns all axis units.
	QList<VipText> axisUnits() const;
	/// Tells if given axis index has a unit.
	bool hasAxisUnit(int index) const;

	/// Set the item's scene map used to transform item's axis coordinates to
	/// device coordinates. Usually you do not need to call this function, as the
	/// scene map is automatically computed by the item. This function bypass the
	/// scene map computed internally.
	void setSceneMap(const VipCoordinateSystemPtr& map);
	/// Returns the item's scene map used to transform item's axis coordinates to
	/// device coordinates. This is either the scene map computed internally or
	/// the one set with #setSceneMap.
	VipCoordinateSystemPtr sceneMap() const;

	/// Returns the view that displays this item (if any).
	QGraphicsView* view() const;

	/// @brief Returns the VipAbstractPlotArea that displays this item (if any)
	VipAbstractPlotArea* area() const;

	/// Returns all items sharing at least one axis with this item.
	QList<VipPlotItem*> linkedItems() const;

	/// Returns the item selection order as compared to the other linked items.
	/// If this item was the last selected one, it will have the highest selection
	/// order value as compared to all the other items sharing the same axes.
	/// Returns 0 if the item is not selected.
	int selectionOrder() const;

	/// @brief Returns the VipShapeDevice that is used to extract the outline of this item and draw the selection state
	VipShapeDevice* selectedDevice() const;

	/// Add a text to be drawn within the item.
	/// You can add as many additional text as you want. This is a convinient way
	/// to draw annotations around your item. The text position depends on \a
	/// text_pos. If \a text_pos is Automatic, the text will be drawn inside the
	/// item's bounding rect, where there is the more available space.
	int addText(const VipText& text, Vip::RegionPositions text_pos = Vip::Automatic, Qt::Alignment text_align = Qt::AlignCenter);
	int addText(const ItemText& text);
	VipText text(int index) const;
	Vip::RegionPositions textPosition(int index) const;
	Qt::Alignment textAlignment(int index) const;
	int textCount() const;
	void setDrawText(bool enable);
	const QMap<int, ItemText>& texts() const;

	/// \sa setDrawText()
	bool drawText() const;

	/// Returns the item bounding rect.
	/// Internally uses shape() to compute the bounding rect.
	virtual QRectF boundingRect() const;
	/// Returns the item shape.
	/// Default implementation uses shapeFromCoordinateSystem() to cache the item
	/// shape whenever the shape is marked as dirty through markDirtyShape() or
	/// markCoordinateSystemDirty().
	virtual QPainterPath shape() const;
	/// Used in shape().
	/// Returns the item shape based on an axis coordinate system.
	/// Reimplement in base class to benefit from internal VipPlotItem shape
	/// caching mechanism. Default implementation uses VIpPlotItem::draw() to
	/// compute the shape.
	virtual QPainterPath shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const;

	/// Returns the item's axes intervals as a VipInterval for each axis.
	/// This function is used to automatically compute the axes limits based on
	/// their items.
	virtual QList<VipInterval> plotBoundingIntervals() const;

	/// Draw the item in given QPainter.
	/// Use the given VipCoordinateSystemPtr to transform item's axis coordinates
	/// in device coordinates.
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const = 0;
	/// Draw the item when selected.
	/// Default implementation draw the outline with the selection pen and
	/// thendraw the item. \sa setSelectedPen, setSelectionPenCreator
	virtual void drawSelected(QPainter*, const VipCoordinateSystemPtr&) const;
	/// Returns all names for this item.
	/// They will be used in the plot area legend (#VipLegend).
	virtual QList<VipText> legendNames() const { return (QList<VipText>() << title()); }
	/// Draw the legend in given QPainter and QRectF.
	/// If the item provides several legend names, draw the legend for given
	/// index. Note that this function should draw the legend graphical part only,
	/// not the legend name.
	virtual QRectF drawLegend(QPainter*, const QRectF&, int // index
	) const
	{
		return QRectF();
	}
	/// Returns the legend drawing as a QPicture.
	QPicture legendPicture(const QRectF& rect, int index) const;
	/// Returns the legend drawing as a QPixmap.
	QPixmap legendPixmap(const QSize& size, int index) const;

	/// Add a dynamic property to this plot item.
	/// This property can be used when displaying a tool tip overing this item.
	/// The plot item will take ownership of the dynamic property.
	///
	/// \sa VipPlotItemDynamicProperty
	void addDynamicProperty(VipPlotItemDynamicProperty* prop);
	void removeDynamicProperty(VipPlotItemDynamicProperty* prop);
	QList<VipPlotItemDynamicProperty*> dynamicProperties() const;

	/// Returns a modified version of given text.
	/// The text might contains several key words that will be replaced by the
	/// actual values they represent. These key words are:
	/// - #title : the item title
	/// - #lcount: number of legends
	/// - #licon[number] : the legend icon [number] in html format
	/// - #lname[number] : the legend name [number]
	/// - #acount: number of axes
	/// - #atitle[number]: the axis [number] title
	/// - #avalue[number]: the axis [number] value for given position
	/// - #pcount: number of dynamic properies
	/// - #pname[number]: the object dynamic property [number] name
	/// - #pvalue[number]: the object dynamic property [number] value as a string
	/// - #p + property_name: replaced by the value of given property
	/// - #d + property_name: replaced by the value of given dynamic property (see
	/// #VipPlotItem::addDynamicProperty)
	///
	/// Optional key words:
	/// - #value if item is based on a double value
	/// - #size if item is based on \a size number of samples
	///
	/// In addition to that, any text between the tags '#repeat=n' and
	/// '#endrepeat' will be repeated n times, with every occurences of '%i'
	/// replaced by the repetition number (starting from 0).
	virtual QString formatText(const QString& str, const QPointF& pos) const;

	/// @brief Tool tip management, axtract an area of interset for a given position in item's coordinates.
	/// use sceneMap() member to convert item's coordinates to scales coordinates.
	/// @param pos position in item's coordinates
	/// @param maxDistance maximum distance to the area of interset in intem's coordinates
	/// @param out_pos represent the points of interset (in item's coordinate)
	/// @param axis tells that we are looking fir the closest value that intersect given axis (-1 to intersect all axis, default behavior)
	/// @param style output box style used to represent the shape of the area of interset. You should just call computePath() or computeRect()... to set the area shape.
	/// @param legend legend index of the area of interset for items having multiple legends (like pie charts or bar charts)
	/// @return true if an area of interset was found for given position, false otherwise
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;

	/// @brief Returns the tool tip text
	const QString& toolTipText() const;

	/// @brief Format the tool tip text (as returned by toolTipText()) be replacing some values using formatText().
	virtual QString formatToolTip(const QPointF& pos) const;

	/// @brief Move this item to the foreground by updating its zValue().
	/// The item z value is set to a value higher to all linked items (see linkedItems() member), except those within 'excluded' list
	void moveToForeground(const QList<VipPlotItem*>& excluded = QList<VipPlotItem*>());
	/// @brief Move this item to the background by updating its zValue().
	/// The item z value is set to a value lower to all linked items (see linkedItems() member), except those within 'excluded' list
	void moveToBackground(const QList<VipPlotItem*>& excluded = QList<VipPlotItem*>());

	/// @brief Tells if an updated is pending (and not yet processed) for this item
	bool updateInProgress() const;

	/// Update the item and optionally the axes in case of auto scaling.
	/// Use this function when the content of the plot item change and might
	/// affect axises with automatic scale. You should not need to call this
	/// function yourself.
	virtual void markAxesDirty();

	/// Update the color map in case of auto scaling.
	/// Use this function when the content of the plot item change and might
	/// affect the color map with automatic scale. You should not need to call
	/// this function yourself.
	virtual void markColorMapDirty();

	/// Update the coordinate system if the axes bounds/intervals changed.
	/// Use this function when item's scales change.
	/// This function is called by axes, and you should not need to call this
	/// function yourself.
	virtual void markCoordinateSystemDirty();

	/// If the parent VipAbstractPlotArea enables drawing item's selection order,
	/// this function gives the top left location (in item's corrdinate) of the
	/// text to draw. This location depends on given text font and text alignment
	/// (based on the item's shape).
	virtual QPointF drawSelectionOrderPosition(const QFont& font, Qt::Alignment alignn, const QRectF& area_bounding_rect) const;

	/// @brief Reset the frame rate counter used to compute the item's display refresh rate
	void resetFpsCounter();
	/// @brief Returns the item display refresh frame rate
	int fps() const;

	//
	// Reimplemented functions from QGraphicsItem, should not be called directly except if you REALLY know
	// what you're doing
	//

	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void dragEnterEvent(QGraphicsSceneDragDropEvent* event);
	virtual void dropEvent(QGraphicsSceneDragDropEvent* event);
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	virtual void timerEvent(QTimerEvent* event);
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);
	virtual bool sceneEvent(QEvent* event);

public Q_SLOTS:

	/// @brief Start dragging this item from given widget.
	/// Automatically called by mouseMoveEvent() if this item supports Darg & Drop (VipPlotItem::Droppable attribute)
	void startDragging(QWidget* parent);

	/// @brief Trigger a redraw of the item
	void update();

	/// @brief Show/hide the item.
	/// Directly call QGraphicsObject::setVisible()
	void setVisible(bool);
	/// @brief Select/unselect the item.
	/// Directly call QGraphicsObject::setSelected
	void setSelected(bool);
	/// @brief Set the selection pen (pen used when the item is selected)
	void setSelectedPen(const QPen&);

	/// @brief Set the item's tool tip text
	virtual void setToolTipText(const QString& text);

	/// @brief Reimplement this function to provide custom drop mechanism on this item
	/// The default implementation extract all plot items of the mime data if it is a VipPlotMimeData,
	/// and set their axes to this item axes.
	virtual void dropMimeData(const QMimeData*);

	/// @brief Apply a transform to this item in axis coordinates.
	/// Returns true on success, false otherwise.
	/// By default, VipPlotItem does not support transforms.
	/// Other items like VipPlotShape do.
	virtual bool applyTransform(const QTransform&) { return false; }

Q_SIGNALS:

	/// This signal is emitted whenever the item changed.
	/// Children classes should call emitItemChanged() whenever the item changes.
	void itemChanged(VipPlotItem*);

	/// Emitted at the beginning of VipPlotItem destructor
	void destroyed(VipPlotItem*);
	/// Emitted when the item is about to be deleted on key SUPPR
	void aboutToDelete();

	/// This signal is emitted whenever the color map is changed through
	/// #VipPlotItem::setColorMap().
	void colorMapChanged(VipPlotItem*);

	/// This signal is emitted whenever item selection changed
	void selectionChanged(VipPlotItem*);

	/// This signal is emitted whenever the item visibility changed
	void visibilityChanged(VipPlotItem*);

	/// This signal is emitted whenever the item axes changed
	void axesChanged(VipPlotItem*);

	/// This signal is emitted whenever the item axis units changed
	void axisUnitChanged(VipPlotItem*);

	void mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseButtonMove(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton);
	void mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton);
	void keyPress(VipPlotItem*, qint64 id, int key, int modifiers);
	void keyRelease(VipPlotItem*, qint64 id, int key, int modifiers);
	void dropped(VipPlotItem* item, QMimeData* mime);

protected:
	/// Emit #VipPlotItem::itemChanged() signal.
	/// If \a update_color_map is true, mark the color map as dirty (they need to
	/// recompute their scale div). If \a update_axes is true, mark the axes as
	/// dirty (it needs to recompute its scale div). If \a update_shape is true,
	/// mark the shape as dirty.
	void emitItemChanged(bool update_color_map = true, bool update_axes = true, bool update_shape = true, bool update_style_sheet = true);

	/// Emit the signal destroyed(VipPlotItem*).
	/// It can be handy to call it in your destructor subclass.
	void emitItemDestroyed();

	/// Tells if we are currently computing the item shape.
	/// Returns true just before a call to #shapeFromCoordinateSystem(), false
	/// otherwise.
	bool computingShape() const;

	/// Mark the item shape as dirty or clean. The shape is automatically dirtied
	/// in markCoordinateSystemDirty(), when the axes scale div change. The
	/// functions markDirtyShape() and isDirtyShape() can be used to write a
	/// custom #shape() method that cashes internally a QPainterPath. Call
	/// markDirtyShape(true) if a class change might produce a new item shape.
	/// Call markDirtyShape(false) inside VipPlotItem::shape() when the shape has
	/// been recomputed.
	void markDirtyShape(bool dirty = true) const;
	bool isDirtyShape() const;

	virtual void updateOnStyleSheet();

private:
	void startTimer(int msec);
	void stopTimer();
	bool timerRunning() const;
	qint64 elapsed() const;
	void computeSelectionOrder();

	class PrivateData;
	PrivateData* d_data;
};

Q_DECLARE_METATYPE(VipPlotItem*)
Q_DECLARE_METATYPE(VipPlotItem::MouseButton);
Q_DECLARE_OPERATORS_FOR_FLAGS(VipPlotItem::ItemAttributes)
typedef QList<VipPlotItem*> PlotItemList;

/// @brief Singleton class used to notify whever a VipPlotItem visibility or selection changed,
/// or when an items is clicked over.
class VIP_PLOTTING_EXPORT VipPlotItemManager : public QObject
{
	Q_OBJECT
	friend class VipPlotItem;
	VipPlotItemManager() {}

public:
	static VipPlotItemManager* instance();

Q_SIGNALS:
	void itemSelectionChanged(VipPlotItem* item, bool selected);
	void itemVisibilityChanged(VipPlotItem* item, bool visible);
	void itemClicked(VipPlotItem* item, int button);
};

/// @brief Composite VipPlotItem composed of several internal VipPlotItem objects.
///
class VIP_PLOTTING_EXPORT VipPlotItemComposite : public VipPlotItem
{
	Q_OBJECT

public:
	/// @brief Composite mode
	enum Mode
	{
		/// @brief The VipPlotItemComposite is seen as a unique item, and internal items axes are not set.
		/// The shape of the VipPlotItemComposite is the union shape of all children VipPlotItem,
		/// and the VipPlotItemComposite will draw itself its children VipPlotItem.
		UniqueItem,
		/// @brief The VipPlotItemComposite is a collection of independant VipPlotItem
		Aggregate
	};

	VipPlotItemComposite(Mode mode = UniqueItem, const VipText& title = VipText());
	~VipPlotItemComposite();

	using VipPlotItem::setAxes;

	virtual QPainterPath shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const;
	virtual void setColorMap(VipAxisColorMap* colorMap);
	virtual QList<VipInterval> plotBoundingIntervals() const;
	virtual void setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type);
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QList<VipText> legendNames() const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;
	virtual QString formatToolTip(const QPointF& pos) const;
	virtual void setToolTipText(const QString& text);
	virtual void markColorMapDirty();
	virtual void markCoordinateSystemDirty();
	virtual void setIgnoreStyleSheet(bool);

	/// @brief Set the composite mode
	virtual void setCompositeMode(Mode mode);
	Mode compositeMode() const;

	/// @brief Save and restore the QPainter state in-between item's drawing.
	/// Only used with UniqueItem mode.
	void setSavePainterBetweenItems(bool);
	bool savePainterBetweenItems() const;

	/// @brief Add an item.
	/// The VipPlotItemComposite will take ownership of this item.
	bool append(VipPlotItem*);
	bool remove(VipPlotItem*);
	int count() const;
	int indexOf(VipPlotItem*) const;
	VipPlotItem* at(int index) const;
	VipPlotItem* takeItem(int index);
	const QList<QPointer<VipPlotItem>>& items() const;
	void clear();

protected:
	/// @brief Handler function called whenever an item is added
	virtual void itemAdded(VipPlotItem*) {}
	/// @brief Handler function called whenever an item is removed
	virtual void itemRemoved(VipPlotItem*) {}

	/// @brief Reimplemented from VipPaintItem
	virtual QList<VipPaintItem*> paintItemChildren() const;

private Q_SLOTS:
	void receiveItemChanged();
	void itemAxesChanged(VipPlotItem*);

Q_SIGNALS:
	void plotItemAdded(VipPlotItem*);
	void plotItemRemoved(VipPlotItem*);

private:
	QList<QPointer<VipPlotItem>> d_items;
	Mode d_mode;
	bool d_savePainterBetweenItems;
};

/// @brief Base VipPlotItem for classes representing a data that can be stored in a QVariant
///
/// A VipPlotItemData draw its content based on a data set with VipPlotItemData::setData().
/// Several items use this way of updateing their contents: VipPlotRasterData, VipPlotCurve, VipPlotHistogram...
///
/// The functions VipPlotItemData::setData() and VipPlotItemData::data() are thread safe,a nd protected
/// by a QMutex object returned by VipPlotItemData::dataLock().
///
/// When (or if) overriding VipPlotItemData::setData(), the new member must also be thread safe and call the
/// base implementation.
///
class VIP_PLOTTING_EXPORT VipPlotItemData : public VipPlotItem
{
	Q_OBJECT

public:
	typedef QMutex Mutex;
	typedef QMutexLocker Locker;

	VipPlotItemData(const VipText& title = VipText());
	~VipPlotItemData();

	/// If true (default), VipPlotItemData::setData() will automatically call
	/// VipPlotItem::markDirty() from the main thread. You can disable this
	/// behavior to call VipPlotItem::markDirty() yourself.
	void setAutoMarkDirty(bool);
	bool autoMarkDirty() const;

	/// @brief Returns the object data set with VipPlotItemData::setData()
	virtual QVariant data() const;

	/// @brief Returns the mutex object used in setData() and data().
	/// You should use in your setData() implementation to keep it thread safe.
	Mutex* dataLock() const;

	/// @brief Returns the last time (in milliseconds since epoch) the data was set with setData()
	qint64 lastDataTime() const;
	/// @brief Returns the last time (in milliseconds since epoch) the item was redrawn
	qint64 lastPaintTime() const;

public Q_SLOTS:

	/// @brief Set the item's data
	virtual void setData(const QVariant&);

	/// @brief Reset the data, equivalent to setData(data())
	void resetData();

	/// @brief Mark the object as dirty after setting the data
	void markDirty();
Q_SIGNALS:

	void dataChanged();

private Q_SLOTS:

protected:
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	void setInternalData(const QVariant& value);

	/// Return current data and reset internal one.
	/// Does NOT call setData(), which must be called later.
	/// The lock must be held before calling this function.
	QVariant takeData();

private:
	class PrivateData;
	PrivateData* d_data;
};

/// @brief Typied version of VipPlotItemData
template<class Data, class Sample = Data>
class VipPlotItemDataType : public VipPlotItemData
{
public:
	using data_type = Data;
	using sample_type = Sample;

	VipPlotItemDataType(const VipText& title = VipText())
	  : VipPlotItemData(title)
	{
	}

	void setRawData(const Data& raw_data) { this->setData(QVariant::fromValue(raw_data)); }
	Data rawData() const { return this->data().template value<Data>(); }

	template<class F>
	void updateData(F&& fun)
	{
		
		this->dataLock()->lock();
		Data vec = takeData().template value<Data>();
		try {
			std::forward<F>(fun)(vec);
		}
		catch (...) {
			this->dataLock()->unlock();
			setRawData(vec);
			throw;
		}
		this->dataLock()->unlock();
		setRawData(vec);
	}
};

/// @}
// end Plotting

#endif
