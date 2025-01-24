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

#include <QGraphicsSceneMouseEvent>
#include <QPointer>
#include <qcheckbox.h>

#include "VipDynGridLayout.h"
#include "VipLegendItem.h"

class VipLegendItem::PrivateData
{
public:
	PrivateData()
	  : legendIndex(0)
	  , minimumSymbolSize(0, 0)
	  , maximumSymbolSize(20, 20)
	  , spacing(5)
	  , left(5)
	  , displayMode(DisplayAllItems)
	  , drawCheckbox(true)
	  , box(nullptr)
	  , checked(false)
	{
	}

	QPointer<VipPlotItem> item;
	int legendIndex;
	QSizeF minimumSymbolSize;
	QSizeF maximumSymbolSize;
	double spacing;
	double left;
	QPainter::RenderHints renderHints;
	VipTextStyle textStyle;
	DisplayMode displayMode;
	bool drawCheckbox;
	QCheckBox* box;
	QPixmap boxPixmap;
	bool checked;
};

VipLegendItem::VipLegendItem(VipPlotItem* item, int index, QGraphicsItem* parent)
  : VipBoxGraphicsWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	setPlotItem(item, index);
	this->setGeometry(QRectF(QPointF(0, 0), this->minimumSize()));
}

VipLegendItem::~VipLegendItem()
{
	if (d_data->box)
		delete d_data->box;
}

VipLegend* VipLegendItem::legend() const
{
	if (QGraphicsItem* item = parentItem())
		if (QGraphicsObject* obj = item->toGraphicsObject())
			return qobject_cast<VipLegend*>(obj);
	return nullptr;
}

bool VipLegendItem::emptyLegendText() const
{
	VipText text;
	if (d_data->item && d_data->item->legendNames().size())
		text = d_data->item->legendNames()[d_data->legendIndex];

	return text.isEmpty();
}

void VipLegendItem::updateVisibility()
{
	if (d_data->item) {
		if (emptyLegendText() && displayMode() == DisplayNamedItems) {
			// if(isVisible())
			this->setVisible(false);
		}
		else if (!d_data->item->testItemAttribute(VipPlotItem::VisibleLegend)) {
			// if (isVisible())
			this->setVisible(false);
		}
		else {
			bool vis = d_data->item->isVisible();
			this->setVisible(vis);
		}
	}
	else {
		// if (!isVisible())
		this->setVisible(false);
	}
}

void VipLegendItem::setDisplayMode(DisplayMode mode)
{
	d_data->displayMode = mode;
	updateVisibility();
}

VipLegendItem::DisplayMode VipLegendItem::displayMode() const
{
	return d_data->displayMode;
}

void VipLegendItem::setDrawCheckbox(bool enable)
{
	if (d_data->drawCheckbox != enable) {
		d_data->drawCheckbox = enable;
		updateLegendItem();
	}
}
bool VipLegendItem::drawCheckbox() const
{
	return d_data->drawCheckbox;
}

void VipLegendItem::updateLegendItem()
{
	updateVisibility();

	if (d_data->item && this->isVisible()) {
		VipText text;
		if (d_data->item->legendNames().size())
			text = d_data->item->legendNames()[d_data->legendIndex];

		VipTextStyle st = d_data->textStyle;

		text.setTextStyle(st);
		QSizeF min_size = text.textSize();
		double icon_width = min_size.height(); // by default, icon width = text height
		if (icon_width > maximumSymbolSize().width())
			icon_width = maximumSymbolSize().width();
		min_size += QSizeF(icon_width + left() + spacing(), 0);
		min_size.setHeight(qMax(min_size.height(), minimumSymbolSize().height()));
		if (isCheckable() && d_data->drawCheckbox)
			min_size += QSizeF(d_data->box->width(), 2);
		this->setPreferredSize(min_size);
		this->updateGeometry();
	}
}

void VipLegendItem::setPlotItem(VipPlotItem* item, int legend_index)
{
	d_data->item = item;
	d_data->legendIndex = legend_index;
	updateLegendItem();
}

VipPlotItem* VipLegendItem::plotItem() const
{
	return d_data->item;
}

int VipLegendItem::legendIindex() const
{
	return d_data->legendIndex;
}

void VipLegendItem::setMinimumSymbolSize(const QSizeF& s)
{
	d_data->minimumSymbolSize = s;
	updateLegendItem();
}

QSizeF VipLegendItem::minimumSymbolSize() const
{
	return d_data->minimumSymbolSize;
}

void VipLegendItem::setMaximumSymbolSize(const QSizeF& s)
{
	d_data->maximumSymbolSize = s;
	updateLegendItem();
}

QSizeF VipLegendItem::maximumSymbolSize() const
{
	return d_data->maximumSymbolSize;
}

void VipLegendItem::setSpacing(double spacing)
{
	d_data->spacing = spacing;
	updateLegendItem();
}

double VipLegendItem::spacing()
{
	return d_data->spacing;
}

void VipLegendItem::setLeft(double left)
{
	d_data->left = left;
	updateLegendItem();
}

double VipLegendItem::left()
{
	return d_data->left;
}

void VipLegendItem::setRenderHints(QPainter::RenderHints hints)
{
	d_data->renderHints = hints;
	this->update();
}

QPainter::RenderHints VipLegendItem::renderHints() const
{
	return d_data->renderHints;
}

void VipLegendItem::setTextStyle(const VipTextStyle& ts)
{
	d_data->textStyle = ts;
	updateLegendItem();
}

const VipTextStyle& VipLegendItem::textStyle() const
{
	return d_data->textStyle;
}

VipTextStyle& VipLegendItem::textStyle()
{
	return d_data->textStyle;
}
// void VipLegendItem::update()
// {
// if(d_data->item)
// {
// this->setVisible(d_data->item->testItemAttribute(VipPlotItem::VipLegend));
// }
// VipBoxGraphicsWidget::update();
// }
void VipLegendItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	VipBoxGraphicsWidget::paint(painter, option, widget);

	if (!plotItem())
		return;

	const QList<VipText> legends = plotItem()->legendNames();
	if (d_data->legendIndex >= legends.size())
		return;

	// set check box parent
	// if (d_data->box)
	//  if (this->scene()->views().size())
	//  if (QWidget * p = this->scene()->views().first())
	//	if (p != d_data->box->parentWidget())
	//		d_data->box->setParent(p);

	VipText text = legends[d_data->legendIndex];
	VipTextStyle style = d_data->textStyle;

	text.setTextStyle(style);
	QRectF rect = boundingRect();

	// render check box
	if (d_data->box && d_data->drawCheckbox) {
		painter->drawPixmap(QPoint(0, (rect.height() - d_data->boxPixmap.height()) / 2), d_data->boxPixmap);
		rect.setLeft(rect.left() + d_data->box->width());
	}

	QRectF text_rect = text.textRect(); // painter->boundingRect(QRectF(0,0,100,100),text.text());

	QSizeF symbol_size(rect.width() - text_rect.width() - spacing() - left(), text_rect.height());
	symbol_size.setWidth(qMin(maximumSymbolSize().width(), symbol_size.width()));
	symbol_size.setWidth(qMax(minimumSymbolSize().width(), symbol_size.width()));
	symbol_size.setHeight(qMin(maximumSymbolSize().height(), symbol_size.height()));
	symbol_size.setHeight(qMax(minimumSymbolSize().height(), symbol_size.height()));

	QRectF symbol_rect(QPointF(), symbol_size);

	symbol_rect.moveTopLeft(QPointF(left() + rect.left(), (rect.height() - symbol_size.height()) / 2));
	text_rect.moveTopLeft(QPointF(symbol_rect.right() + spacing(), (rect.height() - text_rect.height()) / 2));

	painter->setRenderHints(d_data->renderHints);

	// draw the legend symbol
	if (d_data->item->testItemAttribute(VipPlotItem::HasLegendIcon)) {
		painter->setClipRect(symbol_rect.adjusted(-1, -1, 1, 1));
		symbol_rect = d_data->item->drawLegend(painter, symbol_rect, d_data->legendIndex);
		painter->setClipping(false);
	}
	else {
		symbol_rect = QRectF(left(), 0, 1, 1);
	}

	// draw the legend text
	text_rect.moveLeft(symbol_rect.right() + spacing());
	text.draw(painter, text_rect);
}

void VipLegendItem::setCheckable(bool checkable)
{
	if (checkable != (bool)d_data->box) {
		if (checkable) {
			d_data->box = new QCheckBox();
			d_data->box->setAutoFillBackground(false);
			d_data->box->setAttribute(Qt::WA_TranslucentBackground, true);
			d_data->box->resize(20, 20);
			d_data->boxPixmap = d_data->box->grab(); // QPixmap::grabWidget(d_data->box);
			d_data->box->setVisible(false);
			d_data->box->setChecked(d_data->checked);
		}
		else {
			delete d_data->box;
			d_data->box = nullptr;
		}
		updateLegendItem();
	}
}
bool VipLegendItem::isCheckable() const
{
	return (bool)d_data->box;
}

void VipLegendItem::setChecked(bool checked)
{
	if (checked != d_data->checked) {
		d_data->checked = checked;
		if (d_data->box) {
			d_data->box->setChecked(checked);
			d_data->boxPixmap = d_data->box->grab(); // QPixmap::grabWidget(d_data->box);
			update();
		}
		Q_EMIT clicked(checked);
	}
}
bool VipLegendItem::isChecked() const
{
	return d_data->checked;
}

void VipLegendItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if (isCheckable() && (event->buttons() & Qt::LeftButton) && event->pos().x() < this->geometry().width() && event->pos().y() < this->geometry().height() && event->pos().x() >= 0 &&
	    event->pos().y() >= 0) {
		setChecked(!isChecked());
	}
}
void VipLegendItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	if (d_data->item && d_data->item->isSelected() && d_data->item->testItemAttribute(VipPlotItem::Droppable) && (event->buttons() & Qt::LeftButton))
		d_data->item->startDragging(event->widget());
}
void VipLegendItem::mouseReleaseEvent(QGraphicsSceneMouseEvent*) {}

static int registerLegendKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		QMap<QByteArray, int> checkstate;
		checkstate["itemBased"] = VipLegend::ItemBased;
		checkstate["checkable"] = VipLegend::Checkable;
		checkstate["checkableVisibility"] = VipLegend::CheckableVisibility;
		checkstate["checkableSelection"] = VipLegend::CheckableSelection;
		checkstate["none"] = VipLegend::NonCheckable;

		QMap<QByteArray, int> displaymode;
		displaymode["allItems"] = VipLegendItem::DisplayAllItems;
		displaymode["namedItems"] = VipLegendItem::DisplayNamedItems;

		keywords["margin"] = VipParserPtr(new DoubleParser());
		keywords["spacing"] = VipParserPtr(new DoubleParser());
		keywords["max-columns"] = VipParserPtr(new DoubleParser());
		keywords["expanding-directions"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::orientationEnum()));
		keywords["alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["check-state"] = VipParserPtr(new EnumParser(checkstate));
		keywords["draw-checkbox"] = VipParserPtr(new BoolParser());
		keywords["display-mode"] = VipParserPtr(new EnumParser(displaymode));
		keywords["item-spacing"] = VipParserPtr(new DoubleParser());
		keywords["minimum-width"] = VipParserPtr(new DoubleParser());
		keywords["maximum-width"] = VipParserPtr(new DoubleParser());
		keywords["minimum-height"] = VipParserPtr(new DoubleParser());
		keywords["maximum-height"] = VipParserPtr(new DoubleParser());

		VipStandardStyleSheet::addTextStyleKeyWords(keywords);

		vipSetKeyWordsForClass(&VipLegend::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerLegendKeyWords = registerLegendKeyWords();

class VipLegend::PrivateData
{
public:
	PrivateData()
	  : legendItemSpacing(5)
	  , legendItemLeft(5)
	  , displayMode(VipLegendItem::DisplayNamedItems)
	  , checkState(ItemBased)
	  , drawCheckbox(true)
	{
	}

	QList<VipPlotItem*> items;
	double legendItemSpacing;
	double legendItemLeft;
	QPainter::RenderHints legendItemRenderHints;
	VipBoxStyle legendItemBoxStyle;
	VipTextStyle legendItemTextStyle;
	VipLegendItem::DisplayMode displayMode;
	CheckState checkState;
	bool drawCheckbox;

	QSizeF minSymbolSize;
	QSizeF maxSymbolSize;
};

VipLegend::VipLegend(QGraphicsItem* parent)
  : VipBoxGraphicsWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->legendItemTextStyle.setAlignment(Qt::AlignLeft);

	this->setLayout(new VipDynGridLayout());
	layout()->setSpacing(0);
	layout()->setMargins(5);
	layout()->setExpandingDirections(Qt::Horizontal | Qt::Vertical);
}

VipLegend::~VipLegend()
{
}

void VipLegend::setCheckState(CheckState st)
{
	d_data->checkState = st;
	QList<VipLegendItem*> legends = legendItems();
	for (int i = 0; i < legends.size(); ++i)
		if (st != ItemBased) {
			legends[i]->setCheckable(st != NonCheckable);
			if (d_data->checkState == CheckableSelection)
				legends[i]->setChecked(legends[i]->plotItem()->isSelected());
			else if (d_data->checkState == CheckableVisibility)
				legends[i]->setChecked(legends[i]->plotItem()->isVisible());
		}
	layout()->invalidate();
}
VipLegend::CheckState VipLegend::checkState() const
{
	return d_data->checkState;
}

void VipLegend::setDrawCheckbox(bool enable)
{
	d_data->drawCheckbox = enable;
	QList<VipLegendItem*> legends = legendItems();
	for (int i = 0; i < legends.size(); ++i)
		legends[i]->setDrawCheckbox(enable);
	layout()->invalidate();
}
bool VipLegend::drawCheckbox() const
{
	return d_data->drawCheckbox;
}

void VipLegend::setDisplayMode(VipLegendItem::DisplayMode mode)
{
	d_data->displayMode = mode;

	QList<VipLegendItem*> legends = legendItems();
	for (int i = 0; i < legends.size(); ++i) {
		legends[i]->setDisplayMode(mode);
	}

	layout()->invalidate();
}

VipLegendItem::DisplayMode VipLegend::displayMode() const
{
	return d_data->displayMode;
}

void VipLegend::setContentsMargins(double left, double top, double right, double bottom)
{
	layout()->setContentsMargins(left, top, right, bottom);
}
void VipLegend::setMargins(double m)
{
	layout()->setMargins(m);
}
void VipLegend::getContentsMargins(double* left, double* top, double* right, double* bottom) const
{
	return layout()->getContentsMargins(left, top, right, bottom);
}

void VipLegend::setLegendAlignment(Qt::Alignment align)
{
	layout()->setAlignment(align);
}
Qt::Alignment VipLegend::legendAlignment() const
{
	return layout()->alignment();
}

void VipLegend::setExpandingDirections(Qt::Orientations o)
{
	layout()->setExpandingDirections(o);
}
Qt::Orientations VipLegend::expandingDirections() const
{
	return layout()->expandingDirections();
}

void VipLegend::setSpacing(double s)
{
	layout()->setSpacing(s);
}
double VipLegend::spacing() const
{
	return layout()->spacing();
}

void VipLegend::setMaxColumns(int maxc)
{
	layout()->setMaxColumns(maxc);
}
int VipLegend::maxColumns() const
{
	return layout()->maxColumns();
}

void VipLegend::addItem(VipPlotItem* item)
{
	removeItem(item);

	if (item) {
		QList<VipText> legends = item->legendNames();
		if (legends.size()) {
			for (int l = 0; l < legends.size(); ++l) {
				VipLegendItem* legend = new VipLegendItem(item, l, this);
				legend->setDisplayMode(displayMode());
				legend->setSpacing(d_data->legendItemSpacing);
				legend->setLeft(d_data->legendItemLeft);
				legend->setRenderHints(item->renderHints());
				legend->setBoxStyle(d_data->legendItemBoxStyle);
				legend->setTextStyle(d_data->legendItemTextStyle);
				legend->setDrawCheckbox(d_data->drawCheckbox);
				if (d_data->minSymbolSize != QSizeF())
					legend->setMinimumSymbolSize(d_data->minSymbolSize);
				if (d_data->maxSymbolSize != QSizeF())
					legend->setMaximumSymbolSize(d_data->maxSymbolSize);
				this->addLegendItem(legend);
			}
		}
		else {
			VipLegendItem* legend = new VipLegendItem(item, 0, this);
			legend->setDisplayMode(displayMode());
			legend->setSpacing(d_data->legendItemSpacing);
			legend->setLeft(d_data->legendItemLeft);
			legend->setRenderHints(item->renderHints());
			legend->setBoxStyle(d_data->legendItemBoxStyle);
			legend->setTextStyle(d_data->legendItemTextStyle);
			legend->setDrawCheckbox(d_data->drawCheckbox);
			if (d_data->minSymbolSize != QSizeF())
				legend->setMinimumSymbolSize(d_data->minSymbolSize);
			if (d_data->maxSymbolSize != QSizeF())
				legend->setMaximumSymbolSize(d_data->maxSymbolSize);
			this->addLegendItem(legend);
		}
	}

	this->layout()->updateGeometry();
}

void VipLegend::insertItem(int index, VipPlotItem* item)
{
	removeItem(item);

	if (item) {
		QList<VipText> legends = item->legendNames();
		if (legends.size()) {
			for (int l = 0; l < legends.size(); ++l) {
				VipLegendItem* legend = new VipLegendItem(item, l, this);
				legend->setDisplayMode(displayMode());
				legend->setSpacing(d_data->legendItemSpacing);
				legend->setLeft(d_data->legendItemLeft);
				legend->setRenderHints(item->renderHints());
				legend->setBoxStyle(d_data->legendItemBoxStyle);
				legend->setTextStyle(d_data->legendItemTextStyle);
				legend->setDrawCheckbox(d_data->drawCheckbox);
				if (d_data->minSymbolSize != QSizeF())
					legend->setMinimumSymbolSize(d_data->minSymbolSize);
				if (d_data->maxSymbolSize != QSizeF())
					legend->setMaximumSymbolSize(d_data->maxSymbolSize);
				this->insertLegendItem(index++, legend);
			}
		}
		else {
			VipLegendItem* legend = new VipLegendItem(item, 0, this);
			legend->setDisplayMode(displayMode());
			legend->setSpacing(d_data->legendItemSpacing);
			legend->setLeft(d_data->legendItemLeft);
			legend->setRenderHints(item->renderHints());
			legend->setBoxStyle(d_data->legendItemBoxStyle);
			legend->setTextStyle(d_data->legendItemTextStyle);
			legend->setDrawCheckbox(d_data->drawCheckbox);
			if (d_data->minSymbolSize != QSizeF())
				legend->setMinimumSymbolSize(d_data->minSymbolSize);
			if (d_data->maxSymbolSize != QSizeF())
				legend->setMaximumSymbolSize(d_data->maxSymbolSize);
			this->insertLegendItem(index, legend);
		}
	}

	this->layout()->updateGeometry();
}

void VipLegend::addLegendItem(VipLegendItem* legendItem)
{
	if (legendItem && layout()->items().indexOf(legendItem) < 0) {
		if (d_data->items.indexOf(legendItem->plotItem()) < 0)
			d_data->items.append(legendItem->plotItem());

		layout()->addItem(legendItem);
		legendItemAdded(legendItem);

		connect(legendItem->plotItem(), SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(itemChanged(VipPlotItem*))); //,Qt::QueuedConnection);

		layout()->invalidate();
		this->update();
	}
}

void VipLegend::insertLegendItem(int index, VipLegendItem* legendItem)
{
	if (legendItem && layout()->items().indexOf(legendItem) < 0) {
		if (d_data->items.indexOf(legendItem->plotItem()) < 0)
			d_data->items.insert(index, legendItem->plotItem());

		layout()->insertItem(index, legendItem);
		legendItemAdded(legendItem);

		connect(legendItem->plotItem(), SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(itemChanged(VipPlotItem*))); //,Qt::QueuedConnection);

		layout()->invalidate();
		this->update();
	}
}

void VipLegend::legendItemAdded(VipLegendItem* legendItem)
{
	connect(legendItem, SIGNAL(clicked(bool)), this, SLOT(receiveClicked(bool)));
	if (d_data->checkState != ItemBased) {
		legendItem->setCheckable(d_data->checkState != NonCheckable);
		if (d_data->checkState == CheckableSelection)
			legendItem->setChecked(legendItem->plotItem()->isSelected());
		else if (d_data->checkState == CheckableVisibility)
			legendItem->setChecked(legendItem->plotItem()->isVisible());
	}
}

void VipLegend::receiveClicked(bool click)
{
	VipLegendItem* item = qobject_cast<VipLegendItem*>(sender());
	if (item) {
		Q_EMIT clicked(item, click);

		if (d_data->checkState == CheckableVisibility)
			item->plotItem()->setVisible(click);
		else if (d_data->checkState == CheckableSelection)
			item->plotItem()->setSelected(click);
	}
}

QVariant VipLegend::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemVisibleHasChanged && isVisible()) {
		// recompute items geometry
		QList<VipLegendItem*> items = legendItems();
		for (int i = 0; i < items.size(); ++i)
			items[i]->updateLegendItem();
	}
	return VipBoxGraphicsWidget::itemChange(change, value);
}

bool VipLegend::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	/// -	'check-state': equivalent to VipLegend::setCheckState(), one of 'itemBased', 'checkable', 'checkableVisibility', 'checkableSelection', 'none'
	/// -	'draw-checkbox': boolean value equivalent to VipLegend::setDrawCheckbox()
	/// -	'display-mode': equivalent to VipLegend::setDisplayMode(), one of 'allItems' and 'namedItems'
	/// -	'margin': floating point value equivalent to VipLegend::setMargins()
	/// -	'alignment': equivalent to VipLegend::setAlignment(), combination of 'left|right|top|bottom|hcenter|vcenter'
	/// -	'expanding-directions': legend expanding direction, combination of 'vertical|horizontal'
	/// -	'spacing': floating point value equivalent to VipLegend::setSpacing()
	/// -	'item-spacing': space between item image and item text
	/// -	'max-columns': equivalent to VipLegend::setMaxColumns()
	/// -	'minimum-width': minimum symbol width
	/// -	'minimum-height': minimum symbol height
	/// -	'maximum-width': maximum symbol width
	/// -	'maximum-height': maximum symbol height
	///
	if (strcmp(name, "check-state") == 0) {
		setCheckState((VipLegend::CheckState)value.toInt());
		return true;
	}
	if (strcmp(name, "draw-checkbox") == 0) {
		setDrawCheckbox(value.toBool());
		return true;
	}
	if (strcmp(name, "display-mode") == 0) {
		setDisplayMode((VipLegendItem::DisplayMode)value.toInt());
		return true;
	}
	if (strcmp(name, "margin") == 0) {
		setMargins(value.toDouble());
		return true;
	}
	if (strcmp(name, "alignment") == 0) {
		setLegendAlignment((Qt::Alignment)value.toInt());
		return true;
	}
	if (strcmp(name, "expanding-directions") == 0) {
		setExpandingDirections((Qt::Orientations)value.toInt());
		return true;
	}
	if (strcmp(name, "spacing") == 0) {
		setSpacing(value.toDouble());
		return true;
	}
	if (strcmp(name, "item-spacing") == 0) {
		setLegendItemSpacing(value.toDouble());
		return true;
	}
	if (strcmp(name, "max-columns") == 0) {
		setMaxColumns(value.toInt());
		return true;
	}
	if (strcmp(name, "minimum-width") == 0) {
		QSizeF s = minimumSymbolSize();
		s.setWidth(value.toDouble());
		setMinimumSymbolSize(s);
		return true;
	}
	if (strcmp(name, "minimum-height") == 0) {
		QSizeF s = minimumSymbolSize();
		s.setHeight(value.toDouble());
		setMinimumSymbolSize(s);
		return true;
	}
	if (strcmp(name, "maximum-width") == 0) {
		QSizeF s = maximumSymbolSize();
		s.setWidth(value.toDouble());
		setMaximumSymbolSize(s);
		return true;
	}
	if (strcmp(name, "maximum-height") == 0) {
		QSizeF s = maximumSymbolSize();
		s.setHeight(value.toDouble());
		setMaximumSymbolSize(s);
		return true;
	}
	else {
		if (VipStandardStyleSheet::handleTextStyleKeyWord(name, value, d_data->legendItemTextStyle)) {
			setLegendItemTextStyle(d_data->legendItemTextStyle);
			return true;
		}
	}
	return VipBoxGraphicsWidget::setItemProperty(name, value, index);
}

bool VipLegend::hasState(const QByteArray& state, bool enable) const
{
	if (state == "inner")
		return property("_vip_inner").toBool() == enable;
	return VipBoxGraphicsWidget::hasState(state, enable);
}

QSizeF VipLegend::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
	return layout()->sizeHint(which, constraint);
}

int VipLegend::removeItem(VipPlotItem* item)
{
	int index = -1;
	if (item && d_data->items.indexOf(item) >= 0) {
		d_data->items.removeOne(item);

		QList<VipLegendItem*> legend_items = legendItems();
		for (int i = 0; i < legend_items.size(); ++i) {
			if (legend_items[i]->plotItem() == item) {
				disconnect(legend_items[i]->plotItem(), SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(itemChanged(VipPlotItem*)));
				layout()->remove(legend_items[i]);

				if (index < 0)
					index = i;
			}
		}

		layout()->invalidate();
	}

	return index;
}

int VipLegend::removeLegendItem(VipLegendItem* legendItem)
{
	if (legendItem && d_data->items.indexOf(legendItem->plotItem()) >= 0) {
		VipPlotItem* item = legendItem->plotItem();
		layout()->remove(legendItem);

		QList<VipLegendItem*> legend_items = legendItems();
		for (int i = 0; i < legend_items.size(); ++i) {
			if (legend_items[i]->plotItem() == item) {
				disconnect(legend_items[i]->plotItem(), SIGNAL(itemChanged(VipPlotItem*)), this, SLOT(itemChanged(VipPlotItem*)));
				layout()->invalidate();
				return i;
			}
		}

		d_data->items.removeOne(item);
		layout()->invalidate();
		this->update();
	}

	return -1;
}

void VipLegend::setItems(const QList<VipPlotItem*>& items)
{
	layout()->clear();
	d_data->items.clear();
	for (int i = 0; i < items.size(); ++i) {
		addItem(items[i]);
	}
}

const QList<VipPlotItem*>& VipLegend::items() const
{
	return d_data->items;
}

void VipLegend::setLegendItems(const QList<VipLegendItem*>& items)
{
	layout()->clear();
	d_data->items.clear();
	for (int i = 0; i < items.size(); ++i) {
		addLegendItem(items[i]);
	}
}

QList<VipLegendItem*> VipLegend::legendItems() const
{
	QList<QGraphicsLayoutItem*> items = layout()->allItems();
	QList<VipLegendItem*> res;

	for (int i = 0; i < items.size(); ++i)
		res.append(static_cast<VipLegendItem*>(items[i]));

	return res;
}

QList<VipLegendItem*> VipLegend::legendItems(const VipPlotItem* item) const
{
	QList<QGraphicsLayoutItem*> items = layout()->allItems();
	QList<VipLegendItem*> res;

	for (int i = 0; i < items.size(); ++i) {
		VipLegendItem* it = static_cast<VipLegendItem*>(items[i]);
		if (it->plotItem() == item)
			res.append(static_cast<VipLegendItem*>(items[i]));
	}

	return res;
}

void VipLegend::clear()
{
	layout()->clear();
	d_data->items.clear();
	layout()->invalidate();
}

int VipLegend::count() const
{
	return d_data->items.size();
}

int VipLegend::count(const VipPlotItem* item) const
{
	int total = 0;
	const QList<VipLegendItem*> legend_items = legendItems();
	for (int i = 0; i < legend_items.size(); ++i) {
		if (legend_items[i]->plotItem() == item) {
			++total;
		}
	}
	return total;
}

void VipLegend::setLegendItemSpacing(double spacing)
{
	d_data->legendItemSpacing = spacing;
	QList<VipLegendItem*> legend_items = legendItems();
	for (int i = 0; i < legend_items.size(); ++i)
		legend_items[i]->setSpacing(spacing);
	layout()->invalidate();
	setGeometry(geometry());
}

double VipLegend::legendItemSpacing() const
{
	return d_data->legendItemSpacing;
}

void VipLegend::setLegendItemLeft(double left)
{
	d_data->legendItemLeft = left;
	QList<VipLegendItem*> legend_items = legendItems();
	for (int i = 0; i < legend_items.size(); ++i)
		legend_items[i]->setLeft(left);
	layout()->invalidate();
}

double VipLegend::legendItemLeft() const
{
	return d_data->legendItemLeft;
}

void VipLegend::setLegendItemRenderHints(QPainter::RenderHints hints)
{
	d_data->legendItemRenderHints = hints;
	QList<VipLegendItem*> legend_items = legendItems();
	for (int i = 0; i < legend_items.size(); ++i)
		legend_items[i]->setRenderHints(hints);
	update();
}

QPainter::RenderHints VipLegend::legendItemRenderHints() const
{
	return d_data->legendItemRenderHints;
}

void VipLegend::setLegendItemBoxStyle(const VipBoxStyle& style)
{
	d_data->legendItemBoxStyle = style;
	QList<VipLegendItem*> legend_items = legendItems();
	for (int i = 0; i < legend_items.size(); ++i)
		legend_items[i]->setBoxStyle(style);
	update();
}

const VipBoxStyle& VipLegend::legendItemBoxStyle() const
{
	return d_data->legendItemBoxStyle;
}
VipBoxStyle& VipLegend::legendItemBoxStyle()
{
	return d_data->legendItemBoxStyle;
}

void VipLegend::setLegendItemTextStyle(const VipTextStyle& t_style)
{
	d_data->legendItemTextStyle = t_style;
	QList<VipLegendItem*> legend_items = legendItems();
	for (int i = 0; i < legend_items.size(); ++i)
		legend_items[i]->setTextStyle(t_style);
	layout()->invalidate();
}

const VipTextStyle& VipLegend::legendItemTextStyle() const
{
	return d_data->legendItemTextStyle;
}

void VipLegend::setMinimumSymbolSize(const QSizeF& s)
{
	d_data->minSymbolSize = s;
	if (s != QSizeF()) {
		QList<VipLegendItem*> legend_items = legendItems();
		for (int i = 0; i < legend_items.size(); ++i)
			legend_items[i]->setMinimumSymbolSize(s);
	}
}
QSizeF VipLegend::minimumSymbolSize() const
{
	return d_data->minSymbolSize;
}

void VipLegend::setMaximumSymbolSize(const QSizeF& s)
{
	d_data->maxSymbolSize = s;
	if (s != QSizeF()) {
		QList<VipLegendItem*> legend_items = legendItems();
		for (int i = 0; i < legend_items.size(); ++i)
			legend_items[i]->setMaximumSymbolSize(s);
	}
}
QSizeF VipLegend::maximumSymbolSize() const
{
	return d_data->maxSymbolSize;
}

VipDynGridLayout* VipLegend::layout() const
{
	return static_cast<VipDynGridLayout*>(VipBoxGraphicsWidget::layout());
}

QRectF VipLegend::preferredGeometry(const QRectF& bounding_rect, Qt::Alignment align)
{
	QRectF legend_rect = bounding_rect;

	double l = 0, r = 0, t = 0, b = 0;
	this->getContentsMargins(&l, &t, &r, &b);

	if (!(layout()->expandingDirections() & Qt::Vertical)) {
		legend_rect.setHeight(layout()->heightForWidth(bounding_rect.width()) + t + b);
	}
	if (!(layout()->expandingDirections() & Qt::Horizontal)) {
		legend_rect.setWidth(layout()->maxRowWidth(layout()->columnsForWidth(bounding_rect.width())) + l + r);
	}

	if (align & Qt::AlignLeft) {
		legend_rect.moveLeft(bounding_rect.left());
	}
	else if (align & Qt::AlignRight) {
		legend_rect.moveRight(bounding_rect.right());
	}
	else {
		legend_rect.moveLeft(bounding_rect.left() + (bounding_rect.width() - legend_rect.width()) / 2);
	}

	if (align & Qt::AlignTop) {
		legend_rect.moveTop(bounding_rect.top());
	}
	else if (align & Qt::AlignBottom) {
		legend_rect.moveBottom(bounding_rect.bottom());
	}
	else {
		legend_rect.moveTop(bounding_rect.top() + (bounding_rect.height() - legend_rect.height()) / 2);
	}

	return legend_rect;
}

void VipLegend::itemChanged(VipPlotItem* item)
{
	// legend count change for this item, update the legend items
	if (item->legendNames().size() != this->count(item)) {
		int index = removeItem(item);
		this->insertItem(index, item);
	}

	// pass the renderHints attribute of the VipPlotItem to the VipLegendItem, update VipLegendItem visibility
	QList<VipLegendItem*> legends = legendItems(item);
	for (int i = 0; i < legends.size(); ++i) {
		legends[i]->setRenderHints(item->renderHints());
		legends[i]->updateLegendItem();
	}

	if (d_data->checkState == CheckableVisibility) {
		legends = legendItems(item);
		for (int i = 0; i < legends.size(); ++i)
			legends[i]->setChecked(item->isVisible());
	}
	else if (d_data->checkState == CheckableSelection) {
		legends = legendItems(item);
		for (int i = 0; i < legends.size(); ++i)
			legends[i]->setChecked(item->isSelected());
	}

	layout()->invalidate();
}

static bool registerVipBorderItem = vipSetKeyWordsForClass(&VipBorderLegend::staticMetaObject);

VipBorderLegend::VipBorderLegend(Alignment pos, QGraphicsItem* parent)
  : VipBorderItem(pos, parent)
  , d_legend(nullptr)
  , d_margin(0)
  , d_length(0)
{
	// Z value just above standard axis
	this->setZValue(21);
	this->setCanvasProximity(1);
}

VipBorderLegend::~VipBorderLegend() {}

void VipBorderLegend::setLegend(VipLegend* legend)
{
	if (d_legend) {
		delete d_legend;
		d_legend = nullptr;
	}
	d_legend = legend;
	if (d_legend) {
		d_legend->setParentItem(this);
		d_legend->setGeometry(this->boundingRect());
	}
}

VipLegend* VipBorderLegend::legend()
{
	if (!d_legend)
		return nullptr;
	if (d_legend->parentItem() != this)
		return d_legend = nullptr;
	else
		return d_legend;
}

const VipLegend* VipBorderLegend::legend() const
{
	if (!d_legend)
		return nullptr;
	if (d_legend->parentItem() != this)
		return const_cast<VipBorderLegend*>(this)->d_legend = nullptr;
	else
		return d_legend;
}

VipLegend* VipBorderLegend::takeLegend()
{
	if (d_legend) {
		d_legend->setParentItem(nullptr);
		VipLegend* res = d_legend;
		d_legend = nullptr;
		return res;
	}
	return nullptr;
}

void VipBorderLegend::setMargin(double margin)
{
	d_margin = margin;
	emitGeometryNeedUpdate();
}

double VipBorderLegend::margin() const
{
	return d_margin;
}

// void VipBorderLegend::setLegendAlignment(Qt::Alignment align)
//  {
//  d_align = align;
//  emitGeometryNeedUpdate();
//  }
//
//  Qt::Alignment VipBorderLegend::legendAlignment() const
//  {
//  return d_align;
//  }

double VipBorderLegend::extentForLength(double length) const
{
	if (!legend())
		return 0;

	if (d_legend->legendItems().size() == 0)
		return 0;

	if (!d_legend->isVisible())
		return 0;

	if (d_length != length) {
		const_cast<double&>(d_length) = length;
		d_legend->layout()->invalidate();
	}

	double ext;
	if (this->orientation() == Qt::Horizontal) {
		ext = d_legend->layout()->heightForWidth(length - 2 * d_margin);
	}
	else {
		ext = d_legend->layout()->maxRowWidth(1);
	}

	return ext + 2 * d_margin;
}

void VipBorderLegend::itemGeometryChanged(const QRectF& r)
{
	Q_UNUSED(r)
	if (legend()) {
		QRectF max_rect = this->boundingRect().adjusted(d_margin, d_margin, -d_margin, -d_margin);
		if (d_max_rect != max_rect) {
			d_max_rect = max_rect;
			d_legend->layout()->invalidate();
		}
		double left, top, right, bottom;
		d_legend->getContentsMargins(&left, &top, &right, &bottom);
		d_legend->setGeometry(d_legend->preferredGeometry(max_rect, d_legend->legendAlignment()));
	}
}
