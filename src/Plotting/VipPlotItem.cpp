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

#include "VipPlotItem.h"
#include "VipAxisColorMap.h"
#include "VipPainter.h"
#include "VipPlotMimeData.h"
#include "VipPlotWidget2D.h"
#include "VipSet.h"
#include "VipShapeDevice.h"
#include "VipStyleSheet.h"
#include "VipUniqueId.h"
#include "VipXmlArchive.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QDateTime>
#include <QDrag>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainterPathStroker>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QtGlobal>

#include <set>

class VipPlotItemDynamicProperty::PrivateData
{
public:
	QPointer<VipPlotItem> parentItem;
	QString name;
};

VipPlotItemDynamicProperty::VipPlotItemDynamicProperty(const QString& name)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->name = name;
}
VipPlotItemDynamicProperty::~VipPlotItemDynamicProperty()
{
}

VipPlotItem* VipPlotItemDynamicProperty::parentItem() const
{
	return d_data->parentItem;
}

QString VipPlotItemDynamicProperty::name() const
{
	return d_data->name;
}

// helper functions
QList<VipAbstractScale*> convert(const QList<QPointer<VipAbstractScale>>& lst)
{
	QList<VipAbstractScale*> res;
	for (int i = 0; i < lst.size(); ++i)
		res << lst[i].data();
	return res;
}
QList<QPointer<VipAbstractScale>> convert(const QList<VipAbstractScale*>& lst)
{
	QList<QPointer<VipAbstractScale>> res;
	for (int i = 0; i < lst.size(); ++i)
		res << lst[i];
	return res;
}

static int staticRegister()
{
	qRegisterMetaType<VipPlotItem::MouseButton>("VipPlotItem::MouseButton");
	qRegisterMetaType<VipPlotItemPointer>();
	return 0;
}
static int _staticRegister = staticRegister();

QPen VipPlotItem::defaultSelectionPen(const VipPlotItem*, const QPen& p)
{
	QPen res = p;
	res.setWidthF(p.width() + 3);
	QColor c = p.color();
	c.setAlpha(50);
	res.setColor(c);
	return res;
}

VipPlotItemManager* VipPlotItemManager::instance()
{
	static VipPlotItemManager inst;
	return &inst;
}

static bool startsWith(const char* pre, const char* str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

class VipPaintItem::PrivateData
{
public:
	QGraphicsObject* graphicsObject;
	bool paintEnabled;
	quint64 globalStyleSheetId;
	VipText title;
	QPainter::RenderHints renderHints;
	QPainter::CompositionMode compositionMode;
	QByteArray styleSheetString;
	VipStyleSheet styleSheet;
	VipStyleSheet inheritedStyleSheet;
	QSet<QByteArray> styleSheetKeys;
	bool dirtyStyleSheet;
	bool insideApply;
	bool ignoreStyleSheet;

	PrivateData()
	  : graphicsObject(nullptr)
	  , paintEnabled(true)
	  , globalStyleSheetId(0)
	  , renderHints(QPainter::Antialiasing | QPainter::TextAntialiasing) // TEST
	  , compositionMode(QPainter::CompositionMode_SourceOver)
	  , dirtyStyleSheet(true)
	  , insideApply(false)
	  , ignoreStyleSheet(false)
	{
	}
};

VipPaintItem::VipPaintItem(QGraphicsObject* obj)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->graphicsObject = obj;
	d_data->graphicsObject->setProperty("VipPaintItem", QVariant::fromValue(this));
}
VipPaintItem::~VipPaintItem()
{
}

void VipPaintItem::updateInternal()
{
	if (!d_data->insideApply)
		this->updateOnStyleSheet();
}

/// @brief Enable/disable item rendering, even if the item is visible
void VipPaintItem::setPaintingEnabled(bool enable)
{
	d_data->paintEnabled = enable;
}
bool VipPaintItem::paintingEnabled() const
{
	return d_data->paintEnabled;
}
/// @brief Returns the item as a QGraphicsObject
QGraphicsObject* VipPaintItem::graphicsObject() const
{
	return d_data->graphicsObject;
}

void VipPaintItem::updateOnStyleSheet()
{
	if (d_data->graphicsObject)
		d_data->graphicsObject->update();
}

void VipPaintItem::updateStyleSheetString()
{
	d_data->styleSheetString = vipStyleSheetToString(d_data->styleSheet);
}

bool VipPaintItem::hasStates(const QSet<QByteArray>& states) const
{
	if (states.isEmpty())
		return true;
	for (const auto& state : states) {
		bool enable = !state.startsWith('!');
		const QByteArray st = enable ? state : state.mid(1);
		if (!hasState(st, enable))
			return false;
	}
	return true;
}

bool VipPaintItem::hasState(const QByteArray& state, bool enable) const
{

	// handle hover and selected
	if (state.startsWith("hover")) {
		bool item_hover = d_data->graphicsObject->property("_vip_hover").toBool();
		return (item_hover == enable);
	}
	if (state.startsWith("selected")) {
		bool item_selected = d_data->graphicsObject->isSelected();
		return (item_selected == enable);
	}

	if (state.startsWith('#')) {
		return d_data->graphicsObject->objectName() == state.mid(1);
	}
	if (state.startsWith('>')) {
		QGraphicsObject* parent = d_data->graphicsObject->parentItem() ? d_data->graphicsObject->parentItem()->toGraphicsObject() : nullptr;
		if (parent && vipIsA(parent, state.mid(1).data()))
			return true;
		return false;
	}

	return false;
}

QList<VipPaintItem*> VipPaintItem::paintItemChildren() const
{

	QList<QGraphicsItem*> items = graphicsObject()->childItems();
	QList<VipPaintItem*> pitems;
	for (QGraphicsItem* it : items) {
		if (QGraphicsObject* obj = it->toGraphicsObject()) {
			if (VipPaintItem* pitem = obj->property("VipPaintItem").value<VipPaintItem*>())
				pitems.push_back(pitem);
		}
	}
	return pitems;
}

void VipPaintItem::internalDispatchStyleSheet(const VipStyleSheet& st) const
{
	// Recursively apply style sheet to children while adding new values

	QList<VipPaintItem*> items = this->paintItemChildren();
	if (items.empty())
		return;
	VipStyleSheet stylesheet = st;
	for (int i = 0; i < items.size(); ++i) {

		VipPaintItem* it = const_cast<VipPaintItem*>(items[i]);

		// Enrich stylesheet with this item's stylesheet
		if (!d_data->styleSheet.isEmpty()) {
			// VipStyleSheet s = vipExtractRelevantStyleSheetFor(d_data->styleSheet, it);
			stylesheet = vipMergeStyleSheet(stylesheet, d_data->styleSheet);
		}
		if (!(stylesheet.isEmpty() && it->d_data->inheritedStyleSheet.isEmpty())) {
			it->d_data->inheritedStyleSheet = stylesheet;
			it->markStyleSheetDirty();
			it->updateInternal();
			it->internalDispatchStyleSheet(stylesheet);
		}
	}
}

void VipPaintItem::dispatchStyleSheetToChildren()
{
	// Find top level parent VipPaintEvent, and cascade from there
	VipPaintItem* parent = this;
	while (true) {
		if (QGraphicsItem* p = parent->graphicsObject()->parentItem())
			if (QGraphicsObject* po = p->toGraphicsObject())
				if (VipPaintItem* it = po->property("VipPaintItem").value<VipPaintItem*>()) {
					parent = it;
					continue;
				}
		break;
	}
	parent->internalDispatchStyleSheet(parent->d_data->styleSheet);
}

void VipPaintItem::markStyleSheetDirty()
{
	if (!d_data->insideApply) // TEST: avoid potential infinit loop
		d_data->dirtyStyleSheet = true;
}
void VipPaintItem::applyStyleSheetIfDirty() const
{
	if (d_data->dirtyStyleSheet || d_data->globalStyleSheetId != VipGlobalStyleSheet::styleSheetId())
		const_cast<VipPaintItem*>(this)->internalApplyStyleSheet(d_data->styleSheet, d_data->inheritedStyleSheet);
}

bool VipPaintItem::internalApplyStyleSheet(const VipStyleSheet& sheet, const VipStyleSheet& inherited)
{
	if (d_data->ignoreStyleSheet)
		return false;
	d_data->dirtyStyleSheet = false;
	d_data->insideApply = true;
	// first, apply global style sheet
	if (!VipGlobalStyleSheet::cstyleSheet().isEmpty()) {
		QString error;
		if (!vipApplyStyleSheet(VipGlobalStyleSheet::cstyleSheet(), this, &error)) {
			qWarning("Apply style sheet error: %s" ,error.toLatin1().data());
			d_data->insideApply = false;
			return false;
		}
		d_data->globalStyleSheetId = VipGlobalStyleSheet::styleSheetId();
	}

	if (!inherited.isEmpty()) {
		QString error;
		QSet<QByteArray> res;
		if (!vipApplyStyleSheet(inherited, this, &error)) {
			qWarning("Apply style sheet error: %s", error.toLatin1().data());
			d_data->insideApply = false;
			return false;
		}
	}
	if (!sheet.isEmpty()) {

		QString error;
		QSet<QByteArray> res;
		if (!vipApplyStyleSheet(sheet, this, &error)) {
			qWarning("Apply style sheet error: %s", error.toLatin1().data());
			d_data->insideApply = false;
			return false;
		}
	}
	d_data->dirtyStyleSheet = false;
	d_data->insideApply = false;
	return true;
}

bool VipPaintItem::internalSetStyleSheet(const QByteArray& ar)
{
	d_data->styleSheet.clear();
	d_data->styleSheetString.clear();

	QString error;
	VipStyleSheet sheet = vipParseStyleSheet(ar, this, &error);
	if (!error.isEmpty()) {
		qWarning("Parse style sheet error: %s", error.toLatin1().data());
		return false;
	}

	if (!internalApplyStyleSheet(sheet, d_data->inheritedStyleSheet))
		return false;

	d_data->styleSheetString = ar;
	d_data->styleSheet = sheet;
	this->updateInternal();
	return true;
}

void VipPaintItem::setStyleSheet(const VipStyleSheet& sheet)
{

	vipRegisterMetaObject(this->graphicsObject()->metaObject());

	if (internalApplyStyleSheet(sheet, d_data->inheritedStyleSheet)) {
		d_data->styleSheetString.clear();
		d_data->styleSheet = sheet;

		this->updateInternal();
		dispatchStyleSheetToChildren();
	}
}

void VipPaintItem::setInheritedStyleSheet(const VipStyleSheet& sheet)
{
	vipRegisterMetaObject(this->graphicsObject()->metaObject());
	d_data->inheritedStyleSheet = sheet;
	internalApplyStyleSheet(d_data->styleSheet, d_data->inheritedStyleSheet);
	this->updateInternal();
}

VipStyleSheet VipPaintItem::setStyleSheet(const QString& style_sheet)
{
	const QByteArray latin = style_sheet.toLatin1();

	if (latin != d_data->styleSheetString) {
		vipRegisterMetaObject(this->graphicsObject()->metaObject());
		if (!internalSetStyleSheet(latin))
			return VipStyleSheet();
		dispatchStyleSheetToChildren();
	}
	return d_data->styleSheet;
}

QString VipPaintItem::styleSheetString() const
{
	return d_data->styleSheetString;
}
const VipStyleSheet& VipPaintItem::styleSheet() const
{
	return d_data->styleSheet;
}
const VipStyleSheet& VipPaintItem::constStyleSheet() const
{
	return d_data->styleSheet;
}
VipStyleSheet& VipPaintItem::styleSheet()
{
	this->markStyleSheetDirty();
	return d_data->styleSheet;
}

void VipPaintItem::setIgnoreStyleSheet(bool enable)
{
	d_data->ignoreStyleSheet = enable;
}
bool VipPaintItem::ignoreStyleSheet() const
{
	return d_data->ignoreStyleSheet;
}

void VipPaintItem::setTitle(const VipText& title)
{
	d_data->title = title;
	updateInternal();
}

const VipText& VipPaintItem::title() const
{
	return d_data->title;
}
void VipPaintItem::clearTitle()
{
	setTitle(VipText(QString(), title().textStyle()));
}

void VipPaintItem::setRenderHints(QPainter::RenderHints hints)
{
	if (d_data->renderHints != hints) {
		d_data->renderHints = hints;
		updateInternal();
	}
}
QPainter::RenderHints VipPaintItem::renderHints() const
{
	return d_data->renderHints;
}

void VipPaintItem::setCompositionMode(QPainter::CompositionMode mode)
{
	if (d_data->compositionMode != mode) {
		d_data->compositionMode = mode;
		updateInternal();
	}
}
QPainter::CompositionMode VipPaintItem::compositionMode() const
{
	return d_data->compositionMode;
}

bool VipPaintItem::setItemProperty(const char* name, const QVariant& value, const QByteArray&)
{
	if (value.userType() == 0)
		return false;
	if (startsWith("qproperty-", name)) {
		d_data->graphicsObject->setProperty(name + strlen("qproperty-"), value);
		return true;
	}
	else if (strcmp(name, "render-hint") == 0) {
		int v = value.toInt();
		switch (v) {
			case 0:
				setRenderHints(QPainter::RenderHints());
				return true;
			case 1:
				setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
				return true;
			case 2:
				setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
				return true;
			default:
				return false;
		}
	}
	else if (strcmp(name, "composition-mode") == 0) {
		int v = value.toInt();
		if (v < 0 || v > QPainter::RasterOp_NotDestination)
			return false;
		setCompositionMode((QPainter::CompositionMode)v);
		return true;
	}
	else if (strcmp(name, "selected") == 0) {
		d_data->graphicsObject->setSelected(value.toBool());
		return true;
	}
	else if (strcmp(name, "visible") == 0) {
		d_data->graphicsObject->setVisible(value.toBool());
		return true;
	}
	else if (strcmp(name, "title") == 0) {
		VipText t = title();
		t.setText(value.toString());
		setTitle(t);
		return true;
	}
	else {
		VipText t = title();
		VipTextStyle st = t.textStyle();
		if (VipStandardStyleSheet::handleTextStyleKeyWord(name, value, st, "title-")) {
			t.setTextStyle(st);
			setTitle(t);
			return true;
		}
	}

	return false;
}

/// Default key words and related parsers for VipPaintItem objects

static int registerBaseKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		QMap<QByteArray, int> renderHint;
		renderHint["antialiasing"] = 1;
		renderHint["highQualityAntialiasing"] = 2;
		renderHint["noAntialiasing"] = 0;

		QMap<QByteArray, int> compositionMode;
		compositionMode["compositionMode_SourceOver"] = QPainter::CompositionMode_SourceOver;
		compositionMode["compositionMode_DestinationOver"] = QPainter::CompositionMode_DestinationOver;
		compositionMode["compositionMode_Clear"] = QPainter::CompositionMode_Clear;
		compositionMode["compositionMode_Source"] = QPainter::CompositionMode_Source;
		compositionMode["compositionMode_Destination"] = QPainter::CompositionMode_Destination;
		compositionMode["compositionMode_SourceIn"] = QPainter::CompositionMode_SourceIn;
		compositionMode["compositionMode_DestinationIn"] = QPainter::CompositionMode_DestinationIn;
		compositionMode["compositionMode_SourceOut"] = QPainter::CompositionMode_SourceOut;
		compositionMode["compositionMode_DestinationOut"] = QPainter::CompositionMode_DestinationOut;
		compositionMode["compositionMode_SourceAtop"] = QPainter::CompositionMode_SourceAtop;
		compositionMode["compositionMode_DestinationAtop"] = QPainter::CompositionMode_DestinationAtop;
		compositionMode["compositionMode_Xor"] = QPainter::CompositionMode_Xor;
		compositionMode["compositionMode_Plus"] = QPainter::CompositionMode_Plus;
		compositionMode["compositionMode_Multiply"] = QPainter::CompositionMode_Multiply;
		compositionMode["compositionMode_Screen"] = QPainter::CompositionMode_Screen;
		compositionMode["compositionMode_Overlay"] = QPainter::CompositionMode_Overlay;
		compositionMode["compositionMode_Darken"] = QPainter::CompositionMode_Darken;
		compositionMode["compositionMode_Lighten"] = QPainter::CompositionMode_Lighten;
		compositionMode["compositionMode_ColorDodge"] = QPainter::CompositionMode_ColorDodge;
		compositionMode["compositionMode_ColorBurn"] = QPainter::CompositionMode_ColorBurn;
		compositionMode["compositionMode_HardLight"] = QPainter::CompositionMode_HardLight;
		compositionMode["compositionMode_SoftLight"] = QPainter::CompositionMode_SoftLight;
		compositionMode["compositionMode_Difference"] = QPainter::CompositionMode_Difference;
		compositionMode["compositionMode_Exclusion"] = QPainter::CompositionMode_Exclusion;
		compositionMode["rasterOp_SourceOrDestination"] = QPainter::RasterOp_SourceOrDestination;
		compositionMode["rasterOp_SourceAndDestination"] = QPainter::RasterOp_SourceAndDestination;
		compositionMode["rasterOp_SourceXorDestination"] = QPainter::RasterOp_SourceXorDestination;
		compositionMode["rasterOp_NotSourceAndNotDestination"] = QPainter::RasterOp_NotSourceAndNotDestination;
		compositionMode["rasterOp_NotSourceOrNotDestination"] = QPainter::RasterOp_NotSourceOrNotDestination;
		compositionMode["rasterOp_NotSourceXorDestination"] = QPainter::RasterOp_NotSourceXorDestination;
		compositionMode["rasterOp_NotSource"] = QPainter::RasterOp_NotSource;
		compositionMode["rasterOp_NotSourceAndDestination"] = QPainter::RasterOp_NotSourceAndDestination;
		compositionMode["rasterOp_SourceAndNotDestination"] = QPainter::RasterOp_SourceAndNotDestination;
		compositionMode["rasterOp_NotSourceOrDestination"] = QPainter::RasterOp_NotSourceOrDestination;
		compositionMode["rasterOp_SourceOrNotDestination"] = QPainter::RasterOp_SourceOrNotDestination;
		compositionMode["rasterOp_ClearDestination"] = QPainter::RasterOp_ClearDestination;
		compositionMode["rasterOp_SetDestination"] = QPainter::RasterOp_SetDestination;
		compositionMode["rasterOp_NotDestination"] = QPainter::RasterOp_NotDestination;

		keywords["render-hint"] = VipParserPtr(new EnumParser(renderHint));
		keywords["composition-mode"] = VipParserPtr(new EnumParser(compositionMode));
		keywords["title"] = VipParserPtr(new TextParser());
		keywords["selected"] = VipParserPtr(new BoolParser());
		keywords["visible"] = VipParserPtr(new BoolParser());

		VipStandardStyleSheet::addTextStyleKeyWords(keywords, "title-");

		vipSetKeyWordsForClass(&QObject::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerBaseKeyWords = registerBaseKeyWords();

/// Default key words and related parsers for VipPlotItem objects

static const QMap<QByteArray, int>& plotItemAttributes()
{
	static QMap<QByteArray, int> attributes;
	if (attributes.isEmpty()) {

		attributes["hasLegendIcon"] = VipPlotItem::HasLegendIcon;
		attributes["visibleLegend"] = VipPlotItem::VisibleLegend;
		attributes["autoScale"] = VipPlotItem::AutoScale;
		attributes["colorMapAutoScale"] = VipPlotItem::ColorMapAutoScale;
		attributes["clipToScaleRect"] = VipPlotItem::ClipToScaleRect;
		attributes["supportTransform"] = VipPlotItem::SupportTransform;
		attributes["droppable"] = VipPlotItem::Droppable;
		attributes["hasToolTip"] = VipPlotItem::HasToolTip;
		attributes["customToolTipOnly"] = VipPlotItem::CustomToolTipOnly;
		attributes["ignoreMouseEvents"] = VipPlotItem::IgnoreMouseEvents;
		attributes["hasSelectionTimer"] = VipPlotItem::HasSelectionTimer;
		attributes["isSuppressable"] = VipPlotItem::IsSuppressable;
		attributes["acceptDropItems"] = VipPlotItem::AcceptDropItems;
	}
	return attributes;
}

static int registerItemKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {
		keywords["attributes"] = VipParserPtr(new EnumOrParser(plotItemAttributes()));
		keywords["attribute"] = VipParserPtr(new BoolParser());
		keywords["colormap"] = VipParserPtr(new EnumOrStringParser(VipStandardStyleSheet::colormapEnum()));
		keywords["colormap-title"] = VipParserPtr(new TextParser());
		keywords["colorpalette"] = VipParserPtr(new EnumOrStringParser(VipStandardStyleSheet::colorPaletteEnum()));
		keywords["selection-border"] = VipParserPtr(new PenParser());
		keywords["border"] = VipParserPtr(new PenParser());
		keywords["border-width"] = VipParserPtr(new DoubleParser());
		keywords["background"] = VipParserPtr(new ColorParser());
		keywords["major-color"] = VipParserPtr(new ColorParser());
		keywords["axis-unit"] = VipParserPtr(new TextParser());
		keywords["tooltip"] = VipParserPtr(new TextParser());

		VipStandardStyleSheet::addTextStyleKeyWords(keywords);

		vipSetKeyWordsForClass(&VipPlotItem::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerItemKeyWords = registerItemKeyWords();

class VipPlotItem::PrivateData
{
public:
	PrivateData()
	  : axisColorMap(nullptr)
	  , type(VipCoordinateSystem::Null)
	  , attributes(VisibleLegend | AutoScale | ClipToScaleRect | ColorMapAutoScale)
	  , timerId(-1)
	  , startTime(0)
	  , fpsCounter(0)
	  , fpsStart(0)
	  , dirtyCoordinateSystem(1)
	  , computeShape(false)
	  , updateScheduled(false)
	  , destruct(false)
	  , destroy_emitted(false)
	  , numThreads(1)
	  , selectionOrder(0)
	  , drawText(true)
	{
		hover = nullEffect;
		select = nullEffect;
		standard = nullEffect;

		selectedPen = Qt::NoPen;
		selectionPenCreator = defaultSelectionPen;
	}

	~PrivateData() {}

	QPointer<QGraphicsObject> clipTo;
	QPointer<VipAxisColorMap> axisColorMap;
	QList<QPointer<VipAbstractScale>> axes;
	VipCoordinateSystem::Type type;
	QVector<VipText> axisUnit;
	QList<QSharedPointer<VipPlotItemDynamicProperty>> dynProperties;

	VipShapeDevice selectedDevice;

	ItemAttributes attributes;

	int timerId;
	qint64 startTime;

	QAtomicInt fpsCounter;
	qint64 fpsStart;

	VipCoordinateSystemPtr sceneMap;
	VipCoordinateSystemPtr cashedDirtyCoordinateSystem;
	VipCoordinateSystemPtr externCoordinateSystem;
	QAtomicInt dirtyCoordinateSystem;

	bool computeShape;
	bool updateScheduled;
	bool destruct;
	bool destroy_emitted;
	int numThreads;
	int selectionOrder;

	QRectF boundingRect;
	QPainterPath shape;

	create_effect_type hover;
	create_effect_type select;
	create_effect_type standard;
	create_selection_pen selectionPenCreator;

	QPen selectedPen;
	QString toolTipText;

	// additional text
	QMap<int, ItemText> texts;
	bool drawText;
	QMap<int, int> mapTexts; // for style sheet only, map the style sheet index to the real index in texts
};

QGraphicsEffect* VipPlotItem::nullEffect(VipPlotItem*)
{
	return nullptr;
}

bool _eventAccepted = false;
bool VipPlotItem::eventAccepted()
{
	return _eventAccepted;
}

void VipPlotItem::setEventAccepted(bool accepted)
{
	_eventAccepted = accepted;
}

VipPlotItem::VipPlotItem(const VipText& title)
  : QOpenGLGraphicsObject()
  , VipPaintItem(this)
  , VipRenderObject(this)
  , d_data(new PrivateData())
{
	setTitle(title);
	this->setFlag(ItemIsSelectable);
	this->setItemAttribute(HasLegendIcon, true);
	this->setItemAttribute(HasToolTip, true);
	this->setItemAttribute(Droppable, true);
	this->setItemAttribute(AcceptDropItems, false);
	this->setFlag(ItemIsFocusable, true);
	this->setAcceptHoverEvents(true);
	this->setAcceptDrops(true);

	// QGraphicsDropShadowEffect * graphics = new QGraphicsDropShadowEffect();
	// graphics->setOffset(QPointF(0,0));
	// graphics->setBlurRadius(8.0);
	// graphics->setEnabled(false);
	// this->setGraphicsEffect(graphics);
}

VipPlotItem::~VipPlotItem()
{
	emitItemDestroyed();

	d_data->destruct = true;

	this->blockSignals(true);
	this->setParent(nullptr);

	// remove item's axes
	for (int i = 0; i < d_data->axes.size(); ++i) {
		if (d_data->axes[i])
			d_data->axes[i]->removeItem(this);
	}
	// remove item's color map
	if (d_data->axisColorMap) {
		disconnect(d_data->axisColorMap, SIGNAL(valueChanged(double)), this, SLOT(update()));
		disconnect(d_data->axisColorMap, SIGNAL(scaleDivChanged(bool)), this, SLOT(update()));
		d_data->axisColorMap->removeItem(this);
	}

	this->blockSignals(false);
}

VipAbstractPlotArea* VipPlotItem::parentPlotArea() const
{
	QGraphicsItem* item = parentItem();
	while (item) {
		if (VipAbstractPlotArea* area = qobject_cast<VipAbstractPlotArea*>(parentItem()->toGraphicsObject()))
			return area;
		item = item->parentItem();
	}
	return nullptr;
}

void VipPlotItem::setSceneMap(const VipCoordinateSystemPtr& map)
{
	if (d_data->externCoordinateSystem != map) {
		d_data->externCoordinateSystem = map;
		// no need to mark style sheet dirty
		emitItemChanged(true, true, true, false);
	}
}

VipCoordinateSystemPtr VipPlotItem::sceneMap() const
{
	// This function is the only thread safe function of VipPlotItem.
	// Indeed, it can be called in a multi threaded context for fatser display.
	// Currently, the only class using this multithreaded display is VipDisplayImage.

	if (d_data->externCoordinateSystem)
		return SHARED_PTR_NAMESPACE::atomic_load(&d_data->externCoordinateSystem);
	if ((int)d_data->dirtyCoordinateSystem) {

		PrivateData* _data = const_cast<PrivateData*>(d_data.get());

		VipCoordinateSystemPtr tmp(vipBuildCoordinateSystem(axes(), d_data->type));
		SHARED_PTR_NAMESPACE::atomic_store(&_data->sceneMap, tmp ? tmp : VipCoordinateSystemPtr(new VipNullCoordinateSystem(axes())));
		//_data->sceneMap =  tmp ? tmp : VipCoordinateSystemPtr(new VipNullCoordinateSystem(axes()));
		_data->dirtyCoordinateSystem = 0;
	}

	VipCoordinateSystemPtr res = SHARED_PTR_NAMESPACE::atomic_load(&d_data->sceneMap);

	// Unused anymore as the VipPlotItemComposite set the sceneMap itself
	/* if (res->type() == VipCoordinateSystem::Null) {
		QVariant v = this->property("VipPlotItemComposite");
		if (!v.isNull())
			return v.value<VipPlotItemComposite*>()->sceneMap();
	}*/
	return res;
}

void VipPlotItem::setVisible(bool visible)
{
	QGraphicsObject::setVisible(visible);
}
void VipPlotItem::setSelected(bool selected)
{
	QGraphicsObject::setSelected(selected);
}

void VipPlotItem::setItemAttributes(ItemAttributes attributes)
{
	d_data->attributes = attributes;
	emitItemChanged();
}

/// Toggle an item attribute
///
/// \param attribute Attribute type
/// \param on true/false
///
/// \sa testItemAttribute(), ItemInterest
void VipPlotItem::setItemAttribute(ItemAttribute attribute, bool on)
{
	if (d_data->attributes.testFlag(attribute) != on) {
		if (on)
			d_data->attributes |= attribute;
		else
			d_data->attributes &= ~attribute;

		emitItemChanged();
	}
}

/// Test an item attribute
///
/// \param attribute Attribute type
/// \return true/false
/// \sa setItemAttribute(), ItemInterest
bool VipPlotItem::testItemAttribute(ItemAttribute attribute) const
{
	return d_data->attributes.testFlag(attribute);
}

VipPlotItem::ItemAttributes VipPlotItem::itemAttributes() const
{
	return d_data->attributes;
}

void VipPlotItem::setHoverEffect(create_effect_type function)
{
	d_data->hover = function;
	emitItemChanged(false, false, false);
}

void VipPlotItem::setSelectedEffect(create_effect_type function)
{
	d_data->select = function;
	emitItemChanged(false, false, false);
}

void VipPlotItem::setStandardEffect(create_effect_type function)
{
	d_data->standard = function;
	emitItemChanged(false, false, false);
}

void VipPlotItem::setClipTo(QGraphicsObject* obj)
{
	d_data->clipTo = obj;
	emitItemChanged(false, false, false);
}
QGraphicsObject* VipPlotItem::clipTo() const
{
	return d_data->clipTo;
}

void VipPlotItem::setSelectedPen(const QPen& pen)
{
	d_data->selectedPen = pen;
	emitItemChanged(false, false, false);
}

VipColorPalette VipPlotItem::colorPalette() const
{
	return VipColorPalette();
}

void VipPlotItem::setSelectionPenCreator(create_selection_pen p)
{
	d_data->selectionPenCreator = p;
	emitItemChanged(false, false, false);
}

QPen VipPlotItem::selectedPen() const
{
	QPen p = d_data->selectedPen;
	if (d_data->selectionPenCreator)
		p = d_data->selectionPenCreator(this, pen());
	return p;
}

void VipPlotItem::setColorMap(VipAxisColorMap* colorMap)
{
	if (d_data->axisColorMap != colorMap) {
		if (d_data->axisColorMap) {
			disconnect(d_data->axisColorMap, SIGNAL(valueChanged(double)), this, SLOT(update()));
			disconnect(d_data->axisColorMap, SIGNAL(scaleDivChanged(bool)), this, SLOT(update()));
			d_data->axisColorMap->removeItem(this);
		}

		if (colorMap) {
			d_data->axisColorMap = colorMap;
			connect(d_data->axisColorMap, SIGNAL(valueChanged(double)), this, SLOT(update()), Qt::DirectConnection);
			connect(d_data->axisColorMap, SIGNAL(scaleDivChanged(bool)), this, SLOT(update()), Qt::DirectConnection);
			d_data->axisColorMap->addItem(this);
		}

		Q_EMIT colorMapChanged(this);
		// no need to mark style sheet dirty
		emitItemChanged(true, true, true, false);
	}
}

VipAxisColorMap* VipPlotItem::colorMap() const
{
	return const_cast<VipAxisColorMap*>(d_data->axisColorMap.data());
}

QRgb VipPlotItem::color(double value, QRgb default_color) const
{
	if (VipAxisColorMap* map = d_data->axisColorMap) {
		if (value != Vip::InvalidValue) {
			return map->colorMap()->rgb(map->gripInterval(), value);
		}
	}

	return default_color;
}

QRgb VipPlotItem::color(double value, const QColor& default_color) const
{
	return color(value, default_color.rgba());
}

VipInterval VipPlotItem::plotInterval(const VipInterval&) const
{
	return VipInterval(0, 0);
}

bool VipPlotItem::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	if (strcmp(name, "selection-border") == 0) {
		if (value.canConvert<QPen>()) {
			setSelectedPen(value.value<QPen>());
			return true;
		}
		else if (value.canConvert<QColor>()) {
			QPen p = selectedPen();
			p.setColor(value.value<QColor>());
			setSelectedPen(p);
			return true;
		}
		else
			return false;
	}
	else if (strcmp(name, "border") == 0) {
		if (value.userType() == qMetaTypeId<QPen>()) {
			QPen p = value.value<QPen>();
			setPen(p);
			return true;
		}
		else if (value.canConvert<QColor>()) {
			QPen p = pen();
			p.setColor(value.value<QColor>());
			setPen(p);
			return true;
		}
		else
			return false;
	}
	else if (strcmp(name, "border-width") == 0) {
		bool ok = false;
		double w = value.toDouble(&ok);
		if (!ok)
			return false;
		QPen p = this->pen();
		p.setWidthF(w);
		setPen(p);

		return true;
	}
	else if (strcmp(name, "background") == 0) {
		if (value.userType() == qMetaTypeId<QBrush>()) {
			setBrush(value.value<QBrush>());
			return true;
		}
		else if (value.canConvert<QColor>()) {
			QBrush b = brush();
			b.setColor(value.value<QColor>());
			setBrush(b);
			return true;
		}
		else
			return false;
	}
	else if (strcmp(name, "major-color") == 0) {
		if (value.canConvert<QColor>()) {
			setMajorColor(value.value<QColor>());
			return true;
		}
		else
			return false;
	}
	else if (strcmp(name, "colormap") == 0) {
		if (value.userType() == QMetaType::QByteArray) {
			if (colorMap()) {
				VipLinearColorMap* map = VipLinearColorMap::createColorMap(VipLinearColorMap::createGradientStops(value.toByteArray().data()));
				colorMap()->setColorMap(map);
			}
		}
		else {

			int v = value.toInt();
			if (v < 0 || v > VipLinearColorMap::Winter)
				return false;
			if (colorMap()) {
				colorMap()->setColorMap((VipLinearColorMap::StandardColorMap)v);
			}
		}

		return true;
	}
	else if (strcmp(name, "axis-unit") == 0) {
		int id = index.toInt();
		VipText t = axisUnit(id);
		t.setText(value.value<QString>());
		setAxisUnit(id, t);
		return true;
	}
	else if (strcmp(name, "colorpalette") == 0) {
		if (value.userType() == QMetaType::QByteArray)
			setColorPalette(VipColorPalette(VipLinearColorMap::createGradientStops(value.toByteArray().data())));
		else
			setColorPalette(VipColorPalette((VipLinearColorMap::StandardColorMap)value.toInt()));
		return true;
	}
	else if (strcmp(name, "colormap-title") == 0) {
		VipText t = colorMap() ? colorMap()->title() : VipText();
		if (value.canConvert<VipText>())
			t = value.value<VipText>();
		else if (value.canConvert<QString>())
			t.setText(value.value<QString>());
		else
			return false;
		if (colorMap())
			colorMap()->setTitle(t);
		return true;
	}
	else if (strcmp(name, "tooltip") == 0) {
		setToolTipText(value.toString());
		return true;
	}
	else if (strcmp(name, "attributes") == 0) {
		this->setItemAttributes((ItemAttributes)value.toInt());
		return true;
	}
	else if (strcmp(name, "attribute") == 0) {
		auto it = plotItemAttributes().find(index);
		if (it == plotItemAttributes().end())
			return false;
		this->setItemAttribute((ItemAttribute)it.value(), value.toBool());
		return true;
	}
	else {
		VipTextStyle st = this->textStyle();
		if (VipStandardStyleSheet::handleTextStyleKeyWord(name, value, st)) {
			setTextStyle(st);
			return true;
		}
	}

	return VipPaintItem::setItemProperty(name, value, index);
}

void VipPlotItem::setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type)
{
	// check that new axes are not equal to the old ones
	bool equal = (axes.size() == d_data->axes.size());
	if (equal) {
		for (int i = 0; i < axes.size(); ++i) {
			if (axes[i] != d_data->axes[i]) {
				equal = false;
				break;
			}
		}
	}
	if (equal)
		return;

	// remove the graphics effect
	this->setGraphicsEffect(nullptr);

	// remove item from any previous scene or parent
	this->setParentItem(nullptr);
	if (this->scene())
		this->scene()->removeItem(this);

	// build the list of previous axis titles
	QVector<VipText> titles;
	for (int i = 0; i < d_data->axes.size(); ++i)
		titles << axisUnit(i);

	// reset axes
	QList<QPointer<VipAbstractScale>> old_axes = d_data->axes;
	d_data->axes = QList<QPointer<VipAbstractScale>>();

	// remove item from previous axes
	for (int i = 0; i < old_axes.size(); ++i) {
		if (old_axes[i])
			old_axes[i]->removeItem(this);
	}

	if (!titles.size())
		titles = d_data->axisUnit;

	d_data->axes = convert(axes);
	d_data->type = type;

	QGraphicsItem* parent = nullptr;
	QGraphicsScene* sc = nullptr;

	for (int i = 0; i < d_data->axes.size(); ++i) {
		if (VipAbstractScale* axe = d_data->axes[i]) {
			// set the previous title if this does not already have a title
			if (i < titles.size() && !titles[i].isEmpty() && axe->title().isEmpty())
				axe->setTitle(titles[i]);

			axe->addItem(this);
			if (!sc)
				sc = axe->scene();
			if (!parent)
				parent = axe->parentItem();
		}
	}

	// recompute the selection order for the new axes
	computeSelectionOrder();

	if (parent)
		setParentItem(parent);
	else if (sc)
		sc->addItem(this);

	Q_EMIT axesChanged(this);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

void VipPlotItem::setAxes(VipAbstractScale* x, VipAbstractScale* y, VipCoordinateSystem::Type type)
{
	setAxes(QList<VipAbstractScale*>() << x << y, type);
}

QList<VipAbstractScale*> VipPlotItem::axes() const
{
	return convert(d_data->axes);
}

VipCoordinateSystem::Type VipPlotItem::coordinateSystemType() const
{
	return d_data->type;
}

bool VipPlotItem::hasAxisUnit(int index) const
{
	if (d_data->axisUnit.size() <= index)
		return false;
	return !d_data->axisUnit[index].isEmpty();
}

void VipPlotItem::setAxisUnit(int index, const VipText& unit)
{
	if (d_data->axisUnit.size() <= index)
		d_data->axisUnit.resize(index + 1);
	else if (d_data->axisUnit[index].text() == unit.text()) // if the text is the same, return and DO NOT emit the axisUnitChanged signal
		return;

	d_data->axisUnit[index] = unit;

	if (index < d_data->axes.size())
		if (VipAbstractScale* scale = d_data->axes[index])
			scale->setTitle(unit);

	Q_EMIT axisUnitChanged(this);
}

const VipText& VipPlotItem::axisUnit(int index) const
{
	if (d_data->axisUnit.size() <= index)
		const_cast<VipPlotItem*>(this)->d_data->axisUnit.resize(index + 1);

	if (d_data->axisUnit[index].isEmpty()) {
		if (index < d_data->axes.size() && d_data->axes[index])
			return d_data->axes[index]->title();
	}

	return d_data->axisUnit[index];
}

QList<VipText> VipPlotItem::axisUnits() const
{
	QList<VipText> text;
	for (int i = 0; i < d_data->axes.size(); ++i)
		text.append(axisUnit(i));
	return text;
}

QGraphicsView* VipPlotItem::view() const
{
	return VipAbstractScale::view(this);
}

VipAbstractPlotArea* VipPlotItem::area() const
{
	QGraphicsItem* p = parentItem();
	while (p) {
		if (VipAbstractPlotArea* a = qobject_cast<VipAbstractPlotArea*>(p->toGraphicsObject()))
			return a;
		p = p->parentItem();
	}
	return nullptr;
}

void VipPlotItem::emitItemChanged(bool update_color_map, bool update_axes, bool update_shape, bool update_style_sheet)
{
	if (update_axes)
		this->markAxesDirty();
	if (update_color_map)
		this->markColorMapDirty();
	if (update_shape)
		this->markCoordinateSystemDirty();
	if (update_style_sheet)
		this->markStyleSheetDirty();
	this->markItemDirty();
	Q_EMIT itemChanged(this);
}

void VipPlotItem::emitItemDestroyed()
{
	if (!d_data->destroy_emitted) {
		d_data->destroy_emitted = true;
		Q_EMIT destroyed(this);
	}
}

bool VipPlotItem::computingShape() const
{
	return d_data->computeShape;
}

void VipPlotItem::markAxesDirty()
{
	if (this->testItemAttribute(VipPlotItem::AutoScale)) {
		for (int i = 0; i < d_data->axes.size(); ++i) {
			if (VipAbstractScale* axis = d_data->axes[i]) {
				if (axis->isAutoScale()) //TEST
					axis->emitScaleDivNeedUpdate();
			}
		}
	}
}

void VipPlotItem::markColorMapDirty()
{
	if (d_data->axisColorMap && this->testItemAttribute(VipPlotItem::ColorMapAutoScale)) {
		if (VipAbstractPlotArea* a = area()) {
			a->markScaleDivDirty(d_data->axisColorMap);
			return;
		}
		d_data->axisColorMap->emitScaleDivNeedUpdate();
		this->update();
	}
}

void VipPlotItem::markCoordinateSystemDirty()
{
	if (d_data->dirtyCoordinateSystem == 0) {
		d_data->dirtyCoordinateSystem = 1;
		this->markDirtyShape();
		this->update();
	}
}

bool VipPlotItem::isDirtyShape() const
{
	return d_data->cashedDirtyCoordinateSystem != sceneMap();
}

void VipPlotItem::markDirtyShape(bool dirty) const
{
	if (dirty)
		const_cast<PrivateData*>(d_data.get())->cashedDirtyCoordinateSystem = VipCoordinateSystemPtr();
	else
		const_cast<PrivateData*>(d_data.get())->cashedDirtyCoordinateSystem = sceneMap();
}

void VipPlotItem::updateOnStyleSheet()
{
	emitItemChanged();
}

void VipPlotItem::drawSelected(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	QPen selection = selectedPen();
	if (selection == Qt::NoPen || selection.color().alpha() == 0) {
		this->draw(painter, m);
		return;
	}

	d_data->selectedDevice.clear();
	QPainter p(&d_data->selectedDevice);
	this->draw(&p, m);
	QPainterPath path = d_data->selectedDevice.shape();

	QRectF b = path.boundingRect();
	if (b.isValid() && !path.isEmpty()) {
		painter->setPen(selection);
		painter->setBrush(QBrush());
		painter->setRenderHints(renderHints());
		painter->drawPath(path);
	}

	this->draw(painter, m);
}

#include "VipPlotCurve.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLTexture>
#include <qdatetime.h>
#include <qoffscreensurface.h>
#include <qopenglcontext.h>
#include <qopenglpaintdevice.h>
#include <qwindow.h>

/* static QWindow* window()
{
	static QWindow* win = nullptr;
	if (!win) {
		QSurfaceFormat format;
		format.setMajorVersion(3);
		format.setMinorVersion(3);

		win = new QWindow();
		win->setSurfaceType(QWindow::OpenGLSurface);
		win->setFormat(format);
		win->create();
	}
	return win;
}
static QOpenGLContext* context()
{
	static QOpenGLContext* ctx = nullptr;
	if (!ctx) {
		QSurfaceFormat format;
		format.setMajorVersion(3);
		format.setMinorVersion(3);
		ctx = new QOpenGLContext();
		ctx->setFormat(format);
		if (!ctx->create())
			qFatal("Cannot create the requested OpenGL context!");
	}
	return ctx;
}
static QOpenGLFramebufferObject* buffer(const QSize& size)
{
	static QOpenGLFramebufferObject* buf = nullptr;
	if (!buf || buf->size() != size) {
		if (buf) {
			delete buf;
		}

		QOpenGLFramebufferObjectFormat fboFormat;
		fboFormat.setSamples(16);
		fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
		buf = new QOpenGLFramebufferObject(size, fboFormat);
	}
	return buf;
}
static QImage createImageWithFBO(VipPlotItem* item)
{
	// qint64 st = QDateTime::currentMSecsSinceEpoch();

	// qint64 st2 = QDateTime::currentMSecsSinceEpoch();

	context()->makeCurrent(window());

	QRectF brect = item->sceneMap()->clipPath(item).boundingRect();
	QSize size = brect.size().toSize();
	const QRect drawRect(0, 0, size.width(), size.height());
	const QSize drawRectSize = drawRect.size();

	// QOpenGLFramebufferObjectFormat fboFormat;
	// fboFormat.setSamples(16);
	// fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

	QOpenGLFramebufferObject* fbo = buffer(drawRectSize); //(drawRectSize, fboFormat);
							      // bool r2 =
	fbo->bind();

	QOpenGLPaintDevice device(drawRectSize);

	QPainter painter;
	painter.begin(&device);

	painter.beginNativePainting();
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	painter.endNativePainting();

	// painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);
	//
	// painter.fillRect(drawRect, Qt::blue);
	//
	// painter.drawTiledPixmap(drawRect, QPixmap(":/qt-project.org/qmessagebox/images/qtlogo-64.png"));
	//
	// painter.setPen(QPen(Qt::green, 5));
	// painter.setBrush(Qt::red);
	// painter.drawEllipse(0, 100, 400, 200);
	// painter.drawEllipse(100, 0, 200, 400);
	//
	// painter.setPen(QPen(Qt::white, 0));
	// QFont font;
	// font.setPointSize(24);
	// painter.setFont(font);
	// painter.drawText(drawRect, "Hello FBO", QTextOption(Qt::AlignCenter));

	painter.setRenderHints(item->renderHints());
	painter.setCompositionMode(item->compositionMode());
	painter.setWorldTransform(QTransform().translate(-brect.left(), -brect.top()));
	item->draw(&painter, item->sceneMap());

	painter.end();

	// qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st2;

	fbo->release();
	QImage img = fbo->toImage();

	// QImage img(fbo->width(), fbo->height(), QImage::Format_RGBA8888);
	// //glActiveTexture(GL_TEXTURE0);
	// painter.beginNativePainting();
	// glBindTexture(GL_TEXTURE_2D, fbo->texture());
	// glDrawPixels(img.width(), img.height(), QOpenGLTexture::RGBA, GL_UNSIGNED_BYTE, img.bits());
	// painter.endNativePainting();
	//
	// fbo->release();

	// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	// vip_debug("opengl: %i , %i ms\n", (int)el, (int)el2);
	return img;
}*/

void VipPlotItem::resetFpsCounter()
{
	d_data->fpsCounter = 0;
	d_data->fpsStart = QDateTime::currentMSecsSinceEpoch();
}

int VipPlotItem::fps() const
{
	return (int)d_data->fpsCounter / ((QDateTime::currentMSecsSinceEpoch() - d_data->fpsStart) * 0.001);
}

void VipPlotItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	if (d_data->destruct)
		return;
	if (!paintingEnabled())
		return;

	d_data->updateScheduled = 0;

	// reapply style sheet if needed
	this->applyStyleSheetIfDirty();

	if (this->testItemAttribute(ClipToScaleRect)) {
		QPainterPath clip = sceneMap()->clipPath(this);
		if (d_data->clipTo)
			clip = clip & d_data->clipTo->shape();
		painter->setClipPath(clip, Qt::IntersectClip);
	}
	else if (d_data->clipTo)
		painter->setClipPath(d_data->clipTo->shape(), Qt::IntersectClip);

	d_data->fpsCounter++;
	
	if (!computingShape()) {
		if (drawThroughCache(painter, option, widget))
			return;
	}

	painter->setRenderHints(renderHints());
	painter->setCompositionMode(compositionMode());

	if (isSelected() && !computingShape())
		this->drawSelected(painter, sceneMap());
	else
		this->draw(painter, sceneMap());

	// draw the additional texts
	if (d_data->drawText) {
		QRectF rect = boundingRect();
		for (QMap<int, ItemText>::const_iterator it = d_data->texts.begin(); it != d_data->texts.end(); ++it) {
			if (!it.value().text.isEmpty()) {
				VipText t = it.value().text;
				t.setText(this->formatText(t.text(), QPointF(0, 0)));
				VipPainter::drawText(painter, t, QTransform(), QPointF(), 0, it.value().position, it.value().alignment, rect);
			}
		}
	}
}

QRectF VipPlotItem::boundingRect() const
{
	if (isDirtyShape()) {
		const QRectF clip = sceneMap()->clipPath(this).boundingRect();

		VipPlotItemData::Mutex* m = qobject_cast<const VipPlotItemData*>(this) ? static_cast<const VipPlotItemData*>(this)->dataLock() : nullptr;
		if (m)
			m->lock();
		const QList<VipInterval> inters = plotBoundingIntervals();
		if (m)
			m->unlock();
		if (inters.size() != 2)
			return QRectF();

		const QPointF p1 = sceneMap()->transform(inters[0].minValue(), inters[1].minValue());
		const QPointF p2 = sceneMap()->transform(inters[0].maxValue(), inters[1].maxValue());
		const_cast<QRectF&>(d_data->boundingRect) = (QRectF(p1, p2).normalized() & clip).adjusted(0, 0, 1, 1);

		if (vipIsNan(p1.x()) || vipIsNan(p1.y()) || vipIsNan(p2.x()) || vipIsNan(p2.y())) {
			d_data->boundingRect = QRectF();
		}
		else if (d_data->boundingRect == QRectF(0, 0, 1, 1)) {
			// The installed QGraphicsEffect create an internal pixmap of the size of the item's bounding rect.
			// So clip the bounding rect with the axes clip path (or the parent bounding rect) to avoid creating a gigantic pixmap.

			if (parentItem() && !testItemAttribute(ClipToScaleRect))
				const_cast<QRectF&>(d_data->boundingRect) = (shape().boundingRect() & parentItem()->boundingRect()).adjusted(0, 0, 1, 1);
			else
				const_cast<QRectF&>(d_data->boundingRect) = (shape().boundingRect() & sceneMap()->clipPath(this).boundingRect()).adjusted(0, 0, 1, 1);
		}
	}
	return d_data->boundingRect;
}

QList<VipInterval> VipPlotItem::plotBoundingIntervals() const
{
	return VipAbstractScale::scaleIntervals(axes());
}

int VipPlotItem::selectionOrder() const
{
	return isSelected() ? d_data->selectionOrder : 0;
}

VipShapeDevice* VipPlotItem::selectedDevice() const
{
	return &d_data->selectedDevice;
}

int VipPlotItem::addText(const ItemText& text)
{
	// get index
	int index = 0;
	for (QMap<int, ItemText>::const_iterator it = d_data->texts.begin(); it != d_data->texts.end(); ++it, ++index) {
		if (it.key() != index)
			break;
	}

	d_data->texts[index] = text;
	emitItemChanged(false, false, true, false);
	return index;
}

int VipPlotItem::addText(const VipText& text, Vip::RegionPositions text_pos, Qt::Alignment text_align)
{
	return addText(ItemText(text, text_pos, text_align));
}

VipText VipPlotItem::text(int index) const
{
	QMap<int, ItemText>::const_iterator it = d_data->texts.find(index);
	if (it != d_data->texts.end())
		return it.value().text;
	return VipText();
}

Vip::RegionPositions VipPlotItem::textPosition(int index) const
{
	QMap<int, ItemText>::const_iterator it = d_data->texts.find(index);
	if (it != d_data->texts.end())
		return it.value().position;
	return Vip::Automatic;
}

Qt::Alignment VipPlotItem::textAlignment(int index) const
{
	QMap<int, ItemText>::const_iterator it = d_data->texts.find(index);
	if (it != d_data->texts.end())
		return it.value().alignment;
	return Qt::AlignCenter;
}

int VipPlotItem::textCount() const
{
	return d_data->texts.size();
}

const QMap<int, VipPlotItem::ItemText>& VipPlotItem::texts() const
{
	return d_data->texts;
}

void VipPlotItem::setDrawText(bool enable)
{
	if (enable != d_data->drawText) {
		d_data->drawText = enable;
		emitItemChanged(false, true, false);
	}
}

bool VipPlotItem::drawText() const
{
	return d_data->drawText;
}

QList<VipPlotItem*> VipPlotItem::linkedItems() const
{
	QSet<VipPlotItem*> res;

	//bool valid_axes = false;
	for (int i = 0; i < d_data->axes.size(); ++i) {
		if (VipAbstractScale* axis = d_data->axes[i]) {
			//valid_axes = true;
			res += vipToSet(axis->plotItems());
		}
	}

	// if(!valid_axes)
	// {
	// if(parentItem())
	// {
	// QList<QGraphicsItem *> items = parentItem()->childItems ();
	//
	// for(int i=0; i < items.size(); ++i)
	// {
	//	QGraphicsObject * obj = items[i]->toGraphicsObject();
	//	if(obj && qobject_cast<VipPlotItem*>(obj))
	//		res.insert(static_cast<VipPlotItem*>(obj));
	// }
	//
	// }
	// else if(scene())
	// {
	// QList<QGraphicsItem *> items = scene()->items ();
	//
	// for(int i=0; i < items.size(); ++i)
	// {
	//	QGraphicsObject * obj = items[i]->toGraphicsObject();
	//	if(obj && qobject_cast<VipPlotItem*>(obj))
	//	{
	//		VipPlotItem * plot = static_cast<VipPlotItem*>(obj);
	//		if(plot->axes() == axes() && !plot->parentItem())
	//			res.insert(plot);
	//	}
	// }
	// }
	// }

	return res.values();
}

QPainterPath VipPlotItem::shape() const
{
	// avoid calling inside destructor
	if (d_data->destruct)
		return QPainterPath();

	if (isDirtyShape()) {
		// recompute shape if dirty
		markDirtyShape(false);
		// set computeShape to avoid calling drawSelected when computing the shape, as shapeFromCoordinateSystem
		// uses draw
		d_data->computeShape = true;

		// Lock data mutex since shapeFromCoordinateSystem might call draw()
		VipPlotItemData::Mutex* m = qobject_cast<const VipPlotItemData*>(this) ? static_cast<const VipPlotItemData*>(this)->dataLock() : nullptr;
		if (m)
			m->lock();
		d_data->shape = this->shapeFromCoordinateSystem(this->sceneMap());
		if (m)
			m->unlock();
		d_data->computeShape = false;
	}

	return d_data->shape;
}

QPainterPath VipPlotItem::shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const
{

	VipShapeDevice device;
	QPainter painter(&device);
	this->draw(&painter, m);

	QPainterPath res;

	if (view()) {
		res = device.shape(7);
		res.addPath(device.shape());
	}
	else {
		res = device.shape();
	}

	return res;
}

void VipPlotItem::addDynamicProperty(VipPlotItemDynamicProperty* prop)
{
	prop->d_data->parentItem = this;
	d_data->dynProperties.push_back(QSharedPointer<VipPlotItemDynamicProperty>(prop));
}

void VipPlotItem::removeDynamicProperty(VipPlotItemDynamicProperty* prop)
{
	for (int i = 0; i < d_data->dynProperties.size(); ++i)
		if (d_data->dynProperties[i].data() == prop) {
			d_data->dynProperties.removeAt(i);
			return;
		}
}

void VipPlotItem::clearDynamicProperties()
{
	d_data->dynProperties.clear();
}

QList<VipPlotItemDynamicProperty*> VipPlotItem::dynamicProperties() const
{
	QList<VipPlotItemDynamicProperty*> res;
	for (int i = 0; i < d_data->dynProperties.size(); ++i)
		res << d_data->dynProperties[i].data();
	return res;
}

QString VipPlotItem::formatText(const QString& str, const QPointF& pos) const
{
	QRegExp _reg("#(\\w+)");
	const QList<VipAbstractScale*> scales = this->sceneMap()->axes();
	const QList<QByteArray> props = dynamicPropertyNames();
	const QList<VipText> legends = legendNames();
	QString res = str;

	bool has_legend = false;
	bool has_property = false;
	bool has_scale = false;

	int offset = 0;
	int index = _reg.indexIn(res, offset);
	while (index >= 0) {
		QString full = res.mid(index, _reg.matchedLength());

		if (full == "#title")
			res.replace(index, _reg.matchedLength(), title().text());
		else if (full == "#lcount")
			res.replace(index, _reg.matchedLength(), QString::number(legendNames().size()));
		else if (full == "#acount")
			res.replace(index, _reg.matchedLength(), QString::number(axes().size()));
		else if (full == "#pcount")
			res.replace(index, _reg.matchedLength(), QString::number(dynamicPropertyNames().size()));
		else if (full.startsWith("#pname")) {
			has_property = true;
			index++;
		}
		else if (full.startsWith("#pvalue")) {
			has_property = true;
			index++;
		}
		else if (full.startsWith("#licon")) {
			has_legend = true;
			index++;
		}
		else if (full.startsWith("#lname")) {
			has_legend = true;
			index++;
		}
		else if (full.startsWith("#atitle")) {
			has_scale = true;
			index++;
		}
		else if (full.startsWith("#avalue")) {
			has_scale = true;
			index++;
		}
		else if (full.startsWith("#p")) {
			full.remove("#p");
			int i = props.indexOf(full.toLatin1());
			if (i >= 0)
				res.replace(index, _reg.matchedLength(), property(props[i].data()).toString());
			else
				res.replace(index, _reg.matchedLength(), QString());
		}
		else if (full.startsWith("#d")) {
			full.remove("#d");
			// find the property value
			QString value;
			bool found = false;
			for (int p = 0; p < d_data->dynProperties.size(); ++p)
				if (d_data->dynProperties[p]->name() == full) {
					value = d_data->dynProperties[p]->value(pos, VipCoordinateSystem::Type(this->sceneMap()->type()));
					found = true;
					break;
				}
			if (found) {
				VipText tmp = res;
				tmp.replace("#d" + full, value, true);
				res = tmp.text();
				// res.replace(index, _reg.matchedLength(), value);
			}
			else
				res.replace(index, _reg.matchedLength(), QString());
		}
		else
			++index;

		index = _reg.indexIn(res, index);
	}

	VipText text = res;
	text.repeatBlock();

	if (has_legend) {
		QList<VipText> ls = legendNames();
		QPixmap legend0;
		for (int i = 0; i < ls.size(); ++i) {
			QPixmap legend = this->legendPixmap(QSize(20, 16), i);
			if (!legend.isNull() && testItemAttribute(HasLegendIcon) && testItemAttribute(VisibleLegend))
				text.replace("#licon" + QString::number(i), QString(vipToHtml(legend, "align ='middle'")));
			text.replace("#lname" + QString::number(i), ls[i].text());
			if (i == 0)
				legend0 = legend;
		}
		if (ls.size())
			text.replace("#licon", QString(vipToHtml(legend0, "align ='middle'")));
	}

	if (has_scale) {
		for (int i = 0; i < scales.size(); ++i) {
			if (scales[i]) {
				QPointF p = scales[i]->mapFromItem(this, pos);
				text.replace("#avalue" + QString::number(i), scales[i]->scaleDraw()->label(scales[i]->scaleDraw()->value(p), VipScaleDiv::MajorTick).text(), true);
				text.replace("#atitle" + QString::number(i), scales[i]->title().text());
			}
		}
	}

	if (has_property) {
		for (int i = 0; i < props.size(); ++i) {
			text.replace("#pname" + QString::number(i), QString(props[i]));
			text.replace("#pvalue" + QString::number(i), property(props[i].data()).toString(), true);
		}
	}

	return text.text();
}

bool VipPlotItem::areaOfInterest(const QPointF&, int, double, VipPointVector&, VipBoxStyle&, int& legend) const
{
	legend = -1;
	return false;
}

QPointF VipPlotItem::drawSelectionOrderPosition(const QFont& font, Qt::Alignment align, const QRectF& area_bounding_rect) const
{
	if (!isSelected())
		return QPointF();

	QList<const VipPlotItem*> selected_items = vipCastItemListOrdered<const VipPlotItem*>(this->linkedItems(), QString(), 1, 1);
	if (selected_items.size() == 0)
		return QPointF();

	int this_index = selected_items.indexOf(this);
	int max_width, max_height;
	// compute maximum text size
	{
		VipText t(QString::number(selected_items.last()->selectionOrder()));
		t.setFont(font);
		QSizeF s = t.textSize();
		max_width = s.width();
		max_height = s.height();
	}

	QRectF bounding;
	if (boundingRect().intersects(area_bounding_rect))
		bounding = boundingRect() & area_bounding_rect;
	else
		bounding = area_bounding_rect;

	VipText t(QString::number(selectionOrder()));
	t.setFont(font);
	QSizeF text_size = t.textSize();

	QPointF res;
	if (align & Qt::AlignLeft)
		res.setX(bounding.left() + this_index * max_width);
	else if (align & Qt::AlignRight)
		res.setX(bounding.right() - text_size.width() - this_index * max_width);
	else
		res.setX(bounding.center().x() - text_size.width() / 2 + this_index * max_width - selected_items.size() * max_width / 2.0);

	if (align & Qt::AlignTop)
		res.setY(bounding.top() + this_index * max_height);
	else if (align & Qt::AlignBottom)
		res.setY(bounding.bottom() - text_size.height() - this_index * max_height);
	else
		res.setY(bounding.center().y() - text_size.height() / 2 + this_index * max_height - selected_items.size() * max_height / 2.0);

	if (res.y() < area_bounding_rect.top())
		res.setY(area_bounding_rect.top());
	if (res.x() < area_bounding_rect.left())
		res.setX(area_bounding_rect.left());
	if (res.y() + text_size.height() > area_bounding_rect.bottom())
		res.setY(area_bounding_rect.bottom() - text_size.height());
	if (res.x() + text_size.width() > area_bounding_rect.right())
		res.setX(area_bounding_rect.right() - text_size.width());

	return res;
}

void VipPlotItem::setToolTipText(const QString& text)
{
	d_data->toolTipText = text;
}

const QString& VipPlotItem::toolTipText() const
{
	return d_data->toolTipText;
}
// void VipPlotItem::resetToolTipText()
// {
// d_data->toolTipText =
//	"#repeat=#lcount#licon%i#endrepeat<b>#title</b>"
//	"#repeat=#acount<b>    #atitle%i = </b>#avalue%i<br>#endrepeat"
//	"#repeat=#pcount<br><b>    #pname%i = </b>#pvalue%i#endrepeat";
// }
QString VipPlotItem::formatToolTip(const QPointF& pos) const
{
	return formatText(d_data->toolTipText, pos);
}

QPicture VipPlotItem::legendPicture(const QRectF& rect, int index) const
{
	QPicture picture;
	QPainter pa;
	pa.begin(&picture);
	drawLegend(&pa, rect, index);
	pa.end();
	return picture;
}

QPixmap VipPlotItem::legendPixmap(const QSize& size, int index) const
{
	QPixmap pix(size.width(), size.height());
	pix.fill(Qt::transparent);
	QPainter painter(&pix);
	drawLegend(&painter, QRectF(0, 0, size.width() - 1, size.height() - 1), index);
	return pix;
}

void VipPlotItem::startTimer(int msec)
{
	stopTimer();

	if (d_data->timerId < 0) {
		d_data->timerId = QObject::startTimer(msec);
		d_data->startTime = QDateTime::currentMSecsSinceEpoch();
	}
}

void VipPlotItem::stopTimer()
{
	if (d_data->timerId >= 0) {
		QObject::killTimer(d_data->timerId);
		d_data->timerId = -1;
		d_data->startTime = 0;
	}
}

bool VipPlotItem::timerRunning() const
{
	return d_data->timerId >= 0;
}

qint64 VipPlotItem::elapsed() const
{
	if (d_data->timerId >= 0)
		return QDateTime::currentMSecsSinceEpoch() - d_data->startTime;
	else
		return 0;
}

void VipPlotItem::timerEvent(QTimerEvent* event)
{
	// force item update
	if (event->timerId() == d_data->timerId)
		this->update();
}

void VipPlotItem::computeSelectionOrder()
{
	// Called whenever this item is selected
	// Recompute the selection order for each linked item
	// This item selection order should be the highest

	QList<VipPlotItem*> items = this->linkedItems();
	int order = 0;

	for (QList<VipPlotItem*>::iterator it = items.begin(); it != items.end(); ++it) {
		if ((*it) != this) {
			if ((*it)->isSelected())
				order = qMax(order, (*it)->selectionOrder());
			else
				(*it)->d_data->selectionOrder = 0;
		}
	}

	d_data->selectionOrder = isSelected() ? order + 1 : 0;
}

QVariant VipPlotItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemVisibleHasChanged) {
		d_data->updateScheduled = false;
		Q_EMIT visibilityChanged(this);
		emitItemChanged();
		Q_EMIT VipPlotItemManager::instance() -> itemVisibilityChanged(VipPlotItemPointer(this), this->isVisible());
	}

	else if (change == QGraphicsItem::ItemSelectedHasChanged) {
		if (isSelected())
			setGraphicsEffect(d_data->select(this));
		else
			setGraphicsEffect(d_data->standard(this));

		computeSelectionOrder();

		Q_EMIT selectionChanged(this);
		emitItemChanged(false, false, false);

		Q_EMIT VipPlotItemManager::instance() -> itemSelectionChanged(VipPlotItemPointer(this), this->isSelected());
	}
	else if (change == QGraphicsItem::ItemChildAddedChange)
		this->dispatchStyleSheetToChildren();

	return QGraphicsObject::itemChange(change, value);
}

bool VipPlotItem::sceneEvent(QEvent* event)
{
	// set _eventAccepted to tell if the scene event was accepted.
	// This might beused in the parent QGraphicsView when reimplementing its event handlers.
	// Since, at least for QInputEvent types, QGraphicsScene accept the event by default, use VipPlotItem::eventAccepted()
	// to check if the event really has been accepted by a QGraphicsItem

	QPointer<VipPlotItem> _this = this;
	bool res = QGraphicsObject::sceneEvent(event);

	// since the item might be deleted in QGraphicsObject::sceneEvent (like when perssing Suppr key), check if it still exists
	if (!_this)
		return res;

	if (event->type() == QEvent::GraphicsSceneMousePress) {
		Q_EMIT mouseButtonPress(this, static_cast<MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()));
		Q_EMIT VipPlotItemManager::instance()->itemClicked(VipPlotItemPointer( this), static_cast<MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()));
	}
	else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
		Q_EMIT mouseButtonRelease(this, static_cast<MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()));
	}
	else if (event->type() == QEvent::GraphicsSceneMouseMove) {
		Q_EMIT mouseButtonMove(this, static_cast<MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()));
	}
	else if (event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
		Q_EMIT mouseButtonDoubleClick(this, static_cast<MouseButton>(static_cast<QGraphicsSceneMouseEvent*>(event)->button()));
	}
	else if (event->type() == QEvent::KeyPress) {
		QKeyEvent* evt = static_cast<QKeyEvent*>(event);
		Q_EMIT keyPress(this, evt->timestamp(), evt->key(), evt->modifiers());
	}
	else if (event->type() == QEvent::KeyRelease) {
		QKeyEvent* evt = static_cast<QKeyEvent*>(event);
		Q_EMIT keyRelease(this, evt->timestamp(), evt->key(), evt->modifiers());
	}
	return res;
}

void VipPlotItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if (testItemAttribute(IgnoreMouseEvents)) {
		event->ignore();
		return;
	}

	// if(!(flags() & ItemIsSelectable))
	// {
	// event->ignore();
	// return;
	// }

	bool inside_shape = shape().contains(event->pos()); // isUnderPoint(event->pos());
	bool ctrl_down = (event->modifiers() & Qt::ControlModifier);
	bool was_selected = isSelected();
	bool selected = inside_shape;
	if (was_selected && ctrl_down)
		selected = false;

	this->setSelected(selected);

	if (!ctrl_down && !(was_selected && selected)) {
		// unselect all other items
		QList<QGraphicsItem*> items;
		if (parentItem())
			items = parentItem()->childItems();
		else if (scene())
			items = scene()->items();

		for (int i = 0; i < items.size(); ++i) {
			if (items[i] != this)
				items[i]->setSelected(false);
		}
	}

	if (!inside_shape)
		event->ignore();

	this->update();
}

void VipPlotItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	event->ignore();
}

void VipPlotItem::startDragging(QWidget* parent)
{
	// get selected items
	QList<VipPlotItem*> items = linkedItems();
	QList<VipPlotItem*> selected;

	for (int i = 0; i < items.size(); ++i) {
		if (items[i]->isSelected() && items[i]->testItemAttribute(Droppable))
			selected << items[i];
	}

	QDrag drag(parent);
	VipPlotMimeData* mime = new VipPlotMimeData;
	mime->setPlotData(selected);
	drag.setMimeData(mime);
	drag.exec();
}

void VipPlotItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	if (testItemAttribute(IgnoreMouseEvents)) {
		event->ignore();
		return;
	}

	if (testItemAttribute(Droppable) && (event->buttons() & Qt::LeftButton)) {
		startDragging(event->widget());
	}
}

void VipPlotItem::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Delete) {
		PlotItemList lst = linkedItems();
		for (int i = 0; i < lst.size(); ++i) {
			if (lst[i]->isSelected() && lst[i]->testItemAttribute(IsSuppressable)) {
				VipPlotItem* tmp = lst[i];
				Q_EMIT tmp->aboutToDelete();
				tmp->deleteLater();
			}
		}
	}
	else if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
		PlotItemList lst = linkedItems();
		for (int i = 0; i < lst.size(); ++i)
			lst[i]->setSelected(true);
	}
	else
		event->ignore();
}

void VipPlotItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
	if (const VipPlotMimeData* mime = qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
		// we can only drop items with the same coordinate system
		if (mime->coordinateSystemType() != this->coordinateSystemType()) {
			event->setAccepted(false);
			return;
		}
		event->setAccepted(true);
	}
	else
		event->setAccepted(false);
}

void VipPlotItem::dropMimeData(const QMimeData* mimedata)
{
	if (const VipPlotMimeData* mime = qobject_cast<const VipPlotMimeData*>(mimedata)) {
		if (testItemAttribute(AcceptDropItems)) {
			// drop the items by setting there axes
			QWidget* target = nullptr;
			if (scene() && scene()->views().size())
				target = scene()->views().first();

			QList<VipPlotItem*> items = mime->plotData(this, target);

			for (int i = 0; i < items.size(); ++i) {
				if (items[i]->axes() != axes()) {
					items[i]->setAxes(axes(), this->coordinateSystemType());
				}
			}
		}
	}

	Q_EMIT dropped(this, const_cast<QMimeData*>(mimedata));
}

void VipPlotItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
	dropMimeData(event->mimeData());
}

void VipPlotItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	Q_UNUSED(event)
	if (!isSelected())
		setGraphicsEffect(d_data->hover(this));

	// Reapply style sheet in case of 'hover' selector
	this->setProperty("_vip_hover", true);
	this->markStyleSheetDirty();
}

void VipPlotItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	Q_UNUSED(event)
	if (!isSelected())
		setGraphicsEffect(d_data->standard(this));

	// Reapply style sheet in case of 'hover' selector
	this->setProperty("_vip_hover", false);
	this->markStyleSheetDirty();
}

void VipPlotItem::update()
{
	if (!d_data->updateScheduled) {
		d_data->updateScheduled = 1;
		this->markItemDirty();
		if (VipAbstractPlotArea* a = area()) {
			a->markNeedUpdate();
			if (cacheMode() != NoCache)
				QGraphicsObject::update();
			return;
		}
		QGraphicsObject::update();
	}
}

bool VipPlotItem::updateInProgress() const
{
	return d_data->updateScheduled;
}

void VipPlotItem::moveToForeground(const PlotItemList& excluded)
{
	QList<VipPlotItem*> items = linkedItems();
	if (items.size() <= 1)
		return;

	// compute the maximum z value
	double z = items.first()->zValue();
	for (int i = 1; i < items.size(); ++i)
		if (excluded.indexOf(items[i]) < 0)
			z = qMax(z, items[i]->zValue());

	// this z value become the maximum value
	this->setZValue(z);

	for (int i = 0; i < items.size(); ++i)
		if (excluded.indexOf(items[i]) < 0)
			if (items[i] != this && items[i]->zValue() == z)
				items[i]->setZValue(z - 1);
}

void VipPlotItem::moveToBackground(const PlotItemList& excluded)
{
	QList<VipPlotItem*> items = linkedItems();
	if (items.size() <= 1)
		return;

	// compute the minimum z value
	double z = items.first()->zValue();
	for (int i = 1; i < items.size(); ++i)
		if (excluded.indexOf(items[i]) < 0)
			z = qMin(z, items[i]->zValue());

	// this z value become the maximum value
	this->setZValue(z);

	for (int i = 0; i < items.size(); ++i)
		if (excluded.indexOf(items[i]) < 0)
			if (items[i] != this && items[i]->zValue() == z)
				items[i]->setZValue(z + 1);
}

VipPlotItemComposite::VipPlotItemComposite(Mode mode, const VipText& title)
  : VipPlotItem(title)
  , d_mode(mode)
{
	setFlag(VipPlotItem::ItemIsSelectable, mode != Aggregate);
}

VipPlotItemComposite::~VipPlotItemComposite()
{
	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i])
			delete item;
}

QPainterPath VipPlotItemComposite::shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const
{
	if (d_mode == Aggregate)
		return QPainterPath();

	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i]) {
			item->blockSignals(true);
			item->setSceneMap(sceneMap());
			item->blockSignals(false);
		}

	QPainterPath res;
	for (int i = 0; i < d_items.size(); ++i)
		res |= d_items[i]->shapeFromCoordinateSystem(m);
	return res;
}

void VipPlotItemComposite::setColorMap(VipAxisColorMap* colorMap)
{
	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i])
			item->setColorMap(colorMap);
	VipPlotItem::setColorMap(colorMap);
}

QList<VipInterval> VipPlotItemComposite::plotBoundingIntervals() const
{
	if (d_mode == Aggregate) {
		QList<VipInterval> res;
		int axes_count = axes().size();
		for (int i = 0; i < axes_count; ++i)
			res << VipInterval();
		return res;
	}

	QList<VipInterval> res;
	if (d_items.size()) {
		for (int i = 0; i < d_items.size(); ++i)
			if (VipPlotItem* item = d_items[i]) {
				if (res.isEmpty())
					res = item->plotBoundingIntervals();
				else {
					QList<VipInterval> tmp = item->plotBoundingIntervals();
					if (tmp.size() == res.size()) {
						for (int j = 0; j < tmp.size(); ++j)
							res[j] = res[j] | tmp[j];
					}
				}
			}
	}
	return res;
}

void VipPlotItemComposite::setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type)
{
	VipPlotItem::setAxes(axes, type);

	if (d_mode == Aggregate) {
		for (int i = 0; i < d_items.size(); ++i)
			if (VipPlotItem* item = d_items[i])
				item->setAxes(axes, type);
	}
	else {
		// remove axes
		for (int i = 0; i < d_items.size(); ++i)
			if (VipPlotItem* item = d_items[i]) {

				item->setAxes(QList<VipAbstractScale*>(), VipCoordinateSystem::Null);

				item->blockSignals(true);
				item->setSceneMap(sceneMap());
				item->blockSignals(false);
			}
	}
}

void VipPlotItemComposite::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	if (d_mode == Aggregate)
		return;

	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i]) {
			if (d_savePainterBetweenItems)
				p->save();
			item->draw(p, m);
			if (d_savePainterBetweenItems)
				p->restore();
		}
}

QList<VipText> VipPlotItemComposite::legendNames() const
{
	if (d_mode == Aggregate)
		return QList<VipText>();

	QList<VipText> res;
	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i])
			res.append(item->legendNames());

	return res;
}

QRectF VipPlotItemComposite::drawLegend(QPainter* p, const QRectF& r, int index) const
{
	if (d_mode == Aggregate)
		return QRectF();

	int count = 0;
	for (int i = 0; i < d_items.size(); ++i) {
		if (VipPlotItem* item = d_items[i]) {
			QList<VipText> tmp = item->legendNames();
			if (index < count + tmp.size()) {
				return item->drawLegend(p, r, index - count);
			}
			count += tmp.size();
		}
	}
	return QRectF();
}

bool VipPlotItemComposite::areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const
{
	if (d_mode == Aggregate)
		return false;

	int count = 0;
	for (int i = 0; i < d_items.size(); ++i) {
		if (VipPlotItem* item = d_items[i]) {
			if (item->areaOfInterest(pos, axis, maxDistance, out_pos, style, legend)) {
				legend += count;
				return true;
			}
			count += item->legendNames().size();
		}
	}

	return false;
}

QString VipPlotItemComposite::formatToolTip(const QPointF& pos) const
{
	if (d_mode == Aggregate)
		return VipPlotItem::formatToolTip(pos);

	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i])
			if (item->shapeFromCoordinateSystem(sceneMap()).contains(pos))
				return item->formatToolTip(pos);

	return QString();
	// VipPlotItem::formatToolTip(pos);
}

void VipPlotItemComposite::setToolTipText(const QString& text)
{
	VipPlotItem::setToolTipText(text);
	if (d_mode == UniqueItem) {
		for (int i = 0; i < d_items.size(); ++i)
			if (VipPlotItem* item = d_items[i])
				item->setToolTipText(text);
	}
}

void VipPlotItemComposite::receiveItemChanged()
{
	if (d_mode == Aggregate)
		return;

	emitItemChanged();
}

void VipPlotItemComposite::itemAxesChanged(VipPlotItem* item)
{
	if (d_mode == UniqueItem && item->axes().isEmpty()) {
	}
	// if the axes were changed externally, remove the item from the composite item
	else if (item->axes() != axes())
		this->takeItem(this->indexOf(item));
}

void VipPlotItemComposite::markColorMapDirty()
{
	if (d_mode == Aggregate)
		return;

	VipPlotItem::markColorMapDirty();
	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i])
			item->markColorMapDirty();
}

void VipPlotItemComposite::markCoordinateSystemDirty()
{
	if (d_mode == Aggregate)
		return;

	VipPlotItem::markCoordinateSystemDirty();
	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i]) {

			item->markCoordinateSystemDirty();

			item->blockSignals(true);
			item->setSceneMap(sceneMap());
			item->blockSignals(false);
		}
}
void VipPlotItemComposite::setIgnoreStyleSheet(bool enable)
{
	VipPlotItem::setIgnoreStyleSheet(enable);
	for (int i = 0; i < d_items.size(); ++i)
		if (VipPlotItem* item = d_items[i]) {
			item->setIgnoreStyleSheet(enable);
		}
}
void VipPlotItemComposite::setCompositeMode(Mode mode)
{
	d_mode = mode;
	setFlag(VipPlotItem::ItemIsSelectable, mode != Aggregate);
	setAxes(axes(), coordinateSystemType());

	if (d_mode == UniqueItem) {

		for (int i = 0; i < d_items.size(); ++i)
			if (VipPlotItem* item = d_items[i])
				item->setToolTipText(toolTipText());
	}
}

VipPlotItemComposite::Mode VipPlotItemComposite::compositeMode() const
{
	return d_mode;
}

void VipPlotItemComposite::setSavePainterBetweenItems(bool enable)
{
	d_savePainterBetweenItems = enable;
}
bool VipPlotItemComposite::savePainterBetweenItems() const
{
	return d_savePainterBetweenItems;
}

bool VipPlotItemComposite::append(VipPlotItem* item)
{
	QPointer<VipPlotItem> ptr = item;
	if (item && d_items.indexOf(ptr) < 0) {
		d_items.append(ptr);
		connect(item, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(receiveItemChanged()));
		if (d_mode == Aggregate)
			item->setAxes(axes(), coordinateSystemType());
		else
			item->setParentItem(nullptr);
		item->setColorMap(this->colorMap());
		if (d_mode == UniqueItem)
			item->setToolTipText(this->toolTipText());
		connect(item, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(itemAxesChanged(VipPlotItem*)));
		receiveItemChanged();
		this->itemAdded(item);
		item->setProperty("VipPlotItemComposite", QVariant::fromValue(this));
		Q_EMIT plotItemAdded(item);

		this->dispatchStyleSheetToChildren();

		return true;
	}
	return false;
}

bool VipPlotItemComposite::remove(VipPlotItem* item)
{
	if (d_items.removeOne(QPointer<VipPlotItem>(item))) {
		this->itemRemoved(item);
		if (item)
			delete item;
		receiveItemChanged();
		Q_EMIT plotItemRemoved(item);
		return true;
	}
	return false;
}

int VipPlotItemComposite::count() const
{
	return d_items.size();
}

int VipPlotItemComposite::indexOf(VipPlotItem* item) const
{
	return d_items.indexOf(QPointer<VipPlotItem>(item));
}

VipPlotItem* VipPlotItemComposite::takeItem(int index)
{
	QPointer<VipPlotItem> item = at(index);
	d_items.removeOne(item);
	if (item) {
		disconnect(item, SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(receiveItemChanged()));
		disconnect(item, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(itemAxesChanged(VipPlotItem*)));
		receiveItemChanged();
		this->itemRemoved(item);
		item->setProperty("VipPlotItemComposite", QVariant());
		Q_EMIT plotItemRemoved(item);
	}
	return item;
}

VipPlotItem* VipPlotItemComposite::at(int index) const
{
	return d_items[index];
}

const QList<QPointer<VipPlotItem>>& VipPlotItemComposite::items() const
{
	return d_items;
}

void VipPlotItemComposite::clear()
{
	while (count())
		delete takeItem(0);
}

QList<VipPaintItem*> VipPlotItemComposite::paintItemChildren() const
{
	QList<VipPaintItem*> res = VipPlotItem::paintItemChildren();

	for (const VipPlotItem* it : d_items)
		res.push_back(const_cast<VipPlotItem*>(it));

	return res;
}

#include <qmutex.h>
#include <qthread.h>

class VipPlotItemData::PrivateData
{
public:
	PrivateData()
	  : inDestroy(false)
	  , max_sample(std::numeric_limits<int>::max())
	  , lastDataTime(0)
	  , lastPaintTime(0)
	  , autoMarkDirty(true)
	{
	}
	bool inDestroy;
	QVariant data;
	// QMutex dataLock;
	VipPlotItemData::Mutex dataLock;
	int max_sample;
	qint64 lastDataTime;
	qint64 lastPaintTime;
	bool autoMarkDirty;
};
VipPlotItemData::VipPlotItemData(const VipText& title)
  : VipPlotItem(title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	connect(this, SIGNAL(colorMapChanged(VipPlotItem*)), this, SLOT(resetData()), Qt::DirectConnection);
}

VipPlotItemData::~VipPlotItemData()
{
	d_data->inDestroy = true;
	{
		Locker acq(&d_data->dataLock);
	}
}

void VipPlotItemData::setAutoMarkDirty(bool enable)
{
	d_data->autoMarkDirty = enable;
}
bool VipPlotItemData::autoMarkDirty() const
{
	return d_data->autoMarkDirty;
}

void VipPlotItemData::setInternalData(const QVariant& value)
{
	if (d_data->inDestroy)
		return;
	{
		Locker acq(&d_data->dataLock);
		d_data->data = value;
		d_data->lastDataTime = QDateTime::currentMSecsSinceEpoch();
	}
	Q_EMIT dataChanged();
}

void VipPlotItemData::setData(const QVariant& d)
{
	setInternalData(d);
	if (d_data->autoMarkDirty && !d_data->inDestroy) {
		if (QThread::currentThread() == qApp->thread())
			markDirty();
		else
			QMetaObject::invokeMethod(this, "markDirty", Qt::QueuedConnection);
	}
}

QVariant VipPlotItemData::takeData()
{
	if (d_data->inDestroy)
		return QVariant();
	return std::exchange(d_data->data, QVariant());
}

VipPlotItemData::Mutex* VipPlotItemData::dataLock() const
{
	return const_cast<VipPlotItemData::Mutex*>(&d_data->dataLock);
}

qint64 VipPlotItemData::lastDataTime() const
{
	return d_data->lastDataTime;
}
qint64 VipPlotItemData::lastPaintTime() const
{
	return d_data->lastPaintTime;
}

void VipPlotItemData::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	d_data->lastPaintTime = QDateTime::currentMSecsSinceEpoch();
	Locker acq(&d_data->dataLock);
	VipPlotItem::paint(painter, option, widget);
}

void VipPlotItemData::markDirty()
{
	this->markAxesDirty();
	this->markDirtyShape();
	this->markColorMapDirty();
	this->update();
}

void VipPlotItemData::resetData()
{
	this->setData(this->data());
}

QVariant VipPlotItemData::data() const
{
	Locker acq(&d_data->dataLock);
	QVariant res = d_data->data;
	res.detach();
	return res;
}

VipPlotItem* vipCopyPlotItem(const VipPlotItem* item)
{
	VipXOStringArchive arch;
	arch.content("item", QVariant::fromValue(const_cast<VipPlotItem*>(item)));

	VipXIStringArchive iarch(arch.toString());
	iarch.setProperty("_vip_no_id_or_scale", true);
	return iarch.read("item").value<VipPlotItem*>();
}

QByteArray vipSavePlotItemState(const VipPlotItem* item)
{
	VipXOStringArchive arch;
	arch.content("item", QVariant::fromValue(const_cast<VipPlotItem*>(item)));
	return arch.toString().toLatin1();
}

bool vipRestorePlotItemState(VipPlotItem* item, const QByteArray& state)
{
	VipXIStringArchive iarch(state);
	iarch.setProperty("_vip_no_id_or_scale", true);
	return iarch.content("item", item);
}

VipArchive& operator<<(VipArchive& arch, const VipPlotItem* value)
{
	arch.content("id", VipUniqueId::id(value))
	  .content("title", value->title())
	  .content("attributes", (int)value->itemAttributes())
	  .content("renderHints", (int)value->renderHints())
	  .content("compositionMode", (int)value->compositionMode())
	  .content("selectedPen", value->selectedPen())
	  .content("axisUnits", value->axisUnits())
	  .content("visible", value->isVisible());

	// save text style and color palette (4.2.0)
	arch.content("testStyle", value->textStyle());
	arch.content("colorPalette", value->colorPalette());

	// save the color map
	if (value->colorMap())
		// new in 2.2.17: save id as a VipAbstractScale instead of VipAxisColorMap
		arch.content("colorMap", VipUniqueId::id<VipAbstractScale>(value->colorMap()));
	else
		arch.content("colorMap", 0);

	// save the axes
	arch.content("coordinateSystem", (int)value->coordinateSystemType());
	QList<VipAbstractScale*> scales = value->axes();
	arch.content("axisCount", scales.size());
	for (int i = 0; i < scales.size(); ++i)
		arch.content("axisId", VipUniqueId::id(scales[i]));

	// save the properties
	QList<QByteArray> names = value->dynamicPropertyNames();
	QVariantMap properties;
	for (int i = 0; i < names.size(); ++i)
		if (!names[i].startsWith("_q_")) {
			QVariant v = value->property(names[i]);
			if (v.userType() > 0 && v.userType() < QMetaType::User) {
				properties[names[i]] = v;
			}
		}

	arch.content("properties", properties);

	// save the additional texts
	const QMap<int, VipPlotItem::ItemText> texts = value->texts();

	arch.content("textCount", texts.size());
	arch.start("texts");
	for (QMap<int, VipPlotItem::ItemText>::const_iterator it = texts.begin(); it != texts.end(); ++it) {
		arch.content("text", it.value().text);
		arch.content("position", (int)it.value().position);
		arch.content("alignment", (int)it.value().alignment);
	}
	arch.end();

	arch.content("styleSheet", value->styleSheetString());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotItem* value)
{
	int id = arch.read("id").value<int>();
	if (!arch.property("_vip_no_id_or_scale").toBool())
		VipUniqueId::setId(value, id);
	value->setTitle(arch.read("title").value<VipText>());
	value->setItemAttributes(VipPlotItem::ItemAttributes(arch.read("attributes").value<int>()));
	value->setRenderHints(QPainter::RenderHints(arch.read("renderHints").value<int>()));
	value->setCompositionMode(QPainter::CompositionMode(arch.read("compositionMode").value<int>()));
	value->setSelectedPen(arch.read("selectedPen").value<QPen>());
	QList<VipText> units = arch.read("axisUnits").value<QList<VipText>>();
	for (int i = 0; i < units.size(); ++i)
		value->setAxisUnit(i, units[i]);
	value->setVisible(arch.read("visible").toBool());

	// read text style and color palette (4.2.0)
	VipTextStyle style;
	VipColorPalette palette;
	arch.save();
	arch.content("testStyle", style);
	if (arch.content("colorPalette", palette)) {
		value->setTextStyle(style);
		value->setColorPalette(palette);
	}
	else
		arch.restore();

	// load the color map
	id = arch.read("colorMap").toInt();
	if (id && !arch.property("_vip_no_id_or_scale").toBool()) {
		// new in 2.2.17: interpret id as a VipAbstractScale instead of VipAxisColorMap
		VipAxisColorMap* axis = qobject_cast<VipAxisColorMap*>(VipUniqueId::find<VipAbstractScale>(id));
		if (!axis)
			axis = VipUniqueId::find<VipAxisColorMap>(id);
		if (axis)
			value->setColorMap(axis);
	}

	// try to set the axes
	int coordinateSystem = arch.read("coordinateSystem").toInt();
	int count = arch.read("axisCount").toInt();
	if (count) {
		QList<VipAbstractScale*> scales;
		for (int i = 0; i < count; ++i) {
			VipAbstractScale* scale = VipUniqueId::find<VipAbstractScale>(arch.read("axisId").toInt());
			scales.append(scale);
		}
		if (!arch.property("_vip_no_id_or_scale").toBool())
			value->setAxes(scales, (VipCoordinateSystem::Type)coordinateSystem);
	}

	arch.save();
	QVariantMap properties;
	if (arch.content("properties", properties)) {
		for (QVariantMap::const_iterator it = properties.begin(); it != properties.end(); ++it)
			value->setProperty(it.key().toLatin1().data(), it.value());
	}
	else
		arch.restore();

	// read additional texts
	count = arch.read("textCount").toInt();
	if (count && (bool)arch.start("texts")) {
		while (arch) {
			VipText text = arch.read("text").value<VipText>();
			Vip::RegionPositions position = (Vip::RegionPositions)arch.read("position").toInt();
			Qt::Alignment alignment = (Qt::Alignment)arch.read("alignment").toInt();

			if (arch) {
				value->addText(text, position, alignment);
			}
		}
		arch.end();
	}
	arch.resetError();

	arch.save();
	QString st;
	if (arch.content("styleSheet", st))
		value->setStyleSheet(st);
	else
		arch.restore();

	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotItemData* value)
{
	// arch.content("maxSampleCount", value->maxSampleCount());
	QVariant v = value->data();
	if (v.userType() == qMetaTypeId<VipPointVector>()) {
		// for VipPointVector only, downsample to 100 points to avoid having too big session files
		const VipPointVector pts = v.value<VipPointVector>();
		if (pts.size() > 100) {
			double step = pts.size() / 100.0;
			VipPointVector tmp;
			for (double s = 0; s < pts.size(); s += step) {
				int index = (int)s;
				tmp.push_back(pts[index]);
			}
			v = vipToVariant(tmp);
		}
	}
	
	arch.content("data", v);
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotItemData* value)
{
	// value->setMaxSampleCount(arch.read("maxSampleCount").toInt());
	value->setData(arch.read("data"));
	return arch;
}

static int register_types()
{
	qRegisterMetaType<VipPlotItem*>();
	qRegisterMetaType<VipPlotItemData*>();
	vipRegisterArchiveStreamOperators<VipPlotItem*>();
	vipRegisterArchiveStreamOperators<VipPlotItemData*>();
	return 0;
}
static int _register_types = register_types();