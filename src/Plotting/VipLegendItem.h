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

#ifndef VIP_LEGEND_ITEM_H
#define VIP_LEGEND_ITEM_H

#include "VipBorderItem.h"
#include "VipPlotItem.h"

/// \addtogroup Plotting
/// @{

class VipLegend;

/// @brief Item representing a VipPlotItem legend inside a VipLegend.
/// VipLegendItem should not be manipulated directly, but through a VipLegend object.
class VIP_PLOTTING_EXPORT VipLegendItem : public VipBoxGraphicsWidget
{
	Q_OBJECT
public:
	/// @brief Display mode
	enum DisplayMode
	{
		/// @brief Display all items
		DisplayAllItems,
		/// @brief Only display items with a non empty name
		DisplayNamedItems
	};

	/// @brief Construct from a VipPlotItem and a legend index
	VipLegendItem(VipPlotItem* item, int index, QGraphicsItem* parent = nullptr);
	~VipLegendItem();

	/// @brief Returns the parent VipLegend (if any)
	VipLegend* legend() const;

	/// @brief Returns true if the legend text is empty
	bool emptyLegendText() const;
	/// @brief Show/hide this legend item based on the VipPlotItem visibility and legend text content
	void updateVisibility();
	/// @brief Fully update this legend item
	void updateLegendItem();

	/// @brief Set/get the display mode
	void setDisplayMode(DisplayMode);
	DisplayMode displayMode() const;

	/// @brief Enable/disable drawing a checkbox for this legend item
	void setDrawCheckbox(bool);
	bool drawCheckbox() const;

	/// @brief Reset the VipPlotItem and its legend index
	void setPlotItem(VipPlotItem* item, int legend_index = 0);
	VipPlotItem* plotItem() const;
	int legendIindex() const;

	/// @brief Set the minimum size of the symbol part of the legeng in item's coordinates
	void setMinimumSymbolSize(const QSizeF&);
	QSizeF minimumSymbolSize() const;

	/// @brief Set the maximum size of the symbol part of the legeng in item's coordinates
	void setMaximumSymbolSize(const QSizeF&);
	QSizeF maximumSymbolSize() const;

	/// @brief Set the legend item checkable
	void setCheckable(bool);
	bool isCheckable() const;

	/// @brief Check/uncheck the legend item check box
	void setChecked(bool);
	bool isChecked() const;

	/// @brief Set the spacing around the legend item
	void setSpacing(double);
	double spacing();

	/// @brief Set the left space before drawing the legend item
	void setLeft(double);
	double left();

	void setRenderHints(QPainter::RenderHints);
	QPainter::RenderHints renderHints() const;

	void setTextStyle(const VipTextStyle&);
	const VipTextStyle& textStyle() const;
	VipTextStyle& textStyle();

	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);

protected:
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

Q_SIGNALS:
	void clicked(bool);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VipDynGridLayout;

/// @brief Legend item used inside a VipAbstractPlotArea
///
/// VipLegend supports stylesheets and add the following properties:
/// -	'check-state': equivalent to VipLegend::setCheckState(), one of 'itemBased', 'checkable', 'checkableVisibility', 'checkableSelection', 'none'
/// -	'draw-checkbox': boolean value equivalent to VipLegend::setDrawCheckbox()
/// -	'display-mode': equivalent to VipLegend::setDisplayMode(), one of 'allItems' and 'namedItems'
/// -	'margin': floating point value equivalent to VipLegend::setMargins()
/// -	'alignment': equivalent to VipLegend::setAlignment(), combination of 'left|right|top|bottom|hcenter|vcenter'
/// -	'expanding-directions': legend expanding direction, combination of 'vertical|horizontal'
/// -	'spacing': floating point value equivalent to VipLegend::setSpacing()
/// -	'item-spacing': space between item image and item text
/// -	'max-columns': equivalent to VipLegend::setMaxColumns()
/// -	'color': text color
/// -	'font': text font
/// -	'minimum-width': minimum symbol width
/// -	'minimum-height': minimum symbol height
/// -	'maximum-width': maximum symbol width
/// -	'maximum-height': maximum symbol height
///
/// In addition, VipLegend is sensible to the selector 'inner' for VipLegend inside a VipAbstractPlotArea (added with VipAbstractPlotArea::addInnerLegend())
///
class VIP_PLOTTING_EXPORT VipLegend : public VipBoxGraphicsWidget
{
	Q_OBJECT

public:
	/// @brief Check state of internal items
	enum CheckState
	{
		/// @brief Let the VipLegendItem itself define if it is checkable or not
		ItemBased,
		/// @brief Items are checkable, and it's up to the user to handle checking
		Checkable,
		/// @brief Items are checkable, and checking/unchecking will affect item's visibility
		CheckableVisibility,
		/// @brief Items are checkable, and checking/unchecking will affect item's selection
		CheckableSelection,
		/// @brief Items are not checkable
		NonCheckable
	};

	VipLegend(QGraphicsItem* parent = nullptr);
	~VipLegend();

	/// @brief Set the item's check state
	void setCheckState(CheckState);
	CheckState checkState() const;

	/// @brief If items are checkable, draw (or not) a checkbox
	void setDrawCheckbox(bool);
	bool drawCheckbox() const;

	/// @brief Set the global display mode
	void setDisplayMode(VipLegendItem::DisplayMode);
	VipLegendItem::DisplayMode displayMode() const;

	/// @brief Set margins around the legend
	void setContentsMargins(double left, double top, double right, double bottom);
	void setContentsMargins(const VipMargins& m) { setContentsMargins(m.left, m.top, m.right, m.bottom); }
	void setMargins(double);
	void getContentsMargins(double* left, double* top, double* right, double* bottom) const;

	void setLegendAlignment(Qt::Alignment);
	Qt::Alignment legendAlignment() const;

	void setExpandingDirections(Qt::Orientations);
	Qt::Orientations expandingDirections() const;

	void setSpacing(double);
	double spacing() const;

	void setMaxColumns(int);
	int maxColumns() const;

	void addItem(VipPlotItem*);
	void addLegendItem(VipLegendItem*);
	void insertItem(int, VipPlotItem*);
	void insertLegendItem(int, VipLegendItem*);

	int removeItem(VipPlotItem*);
	int removeLegendItem(VipLegendItem*);

	void setItems(const QList<VipPlotItem*>& items);
	const QList<VipPlotItem*>& items() const;

	void setLegendItems(const QList<VipLegendItem*>& items);
	QList<VipLegendItem*> legendItems() const;
	QList<VipLegendItem*> legendItems(const VipPlotItem*) const;

	void clear();

	int count() const;
	int count(const VipPlotItem*) const;

	void setLegendItemSpacing(double spacing);
	double legendItemSpacing() const;

	void setLegendItemLeft(double left);
	double legendItemLeft() const;

	void setLegendItemRenderHints(QPainter::RenderHints);
	QPainter::RenderHints legendItemRenderHints() const;

	void setLegendItemBoxStyle(const VipBoxStyle& style);
	const VipBoxStyle& legendItemBoxStyle() const;
	VipBoxStyle& legendItemBoxStyle();

	void setLegendItemTextStyle(const VipTextStyle& t_style);
	const VipTextStyle& legendItemTextStyle() const;

	void setMinimumSymbolSize(const QSizeF&);
	QSizeF minimumSymbolSize() const;

	void setMaximumSymbolSize(const QSizeF&);
	QSizeF maximumSymbolSize() const;

	QRectF preferredGeometry(const QRectF& bounding_rect, Qt::Alignment align);

	VipDynGridLayout* layout() const;
	virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const;

Q_SIGNALS:
	void clicked(VipLegendItem*, bool);

private Q_SLOTS:
	void itemChanged(VipPlotItem*);
	void receiveClicked(bool);

protected:
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);
	/// @brief Set properties through style sheet
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	/// @brief Make the legend style sheet aware of the selector 'inner'
	virtual bool hasState(const QByteArray& state, bool enable) const;

private:
	void legendItemAdded(VipLegendItem*);
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Legend item within a vertical or horizontal scale
class VIP_PLOTTING_EXPORT VipBorderLegend : public VipBorderItem
{
	Q_OBJECT
public:
	VipBorderLegend(Alignment pos, QGraphicsItem* parent = 0);
	~VipBorderLegend();

	void setLegend(VipLegend*);
	VipLegend* legend();
	const VipLegend* legend() const;
	VipLegend* takeLegend();

	void setMargin(double);
	double margin() const;

protected:
	virtual QPointF position(double value) const
	{
		Q_UNUSED(value)
		return QPointF();
	}
	virtual void layoutScale() {}

	double extentForLength(double length) const;
	virtual void itemGeometryChanged(const QRectF&);

private:
	VipLegend* d_legend;
	double d_margin;
	double d_length;
	QRectF d_max_rect;
	Qt::Alignment d_align;
};

/// @}
// end Plotting

#endif
