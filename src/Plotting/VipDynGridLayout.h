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

#ifndef VIP_DYN_GRID_LAYOUT_H
#define VIP_DYN_GRID_LAYOUT_H

#include <QGraphicsLayout>
#include <QGraphicsWidget>
#include <QList>
#include <QVector>

#include "VipBoxStyle.h"
#include "VipPimpl.h"

/// \addtogroup Plotting
/// @{

/// \brief The VipDynGridLayout class lays out QGraphicsLayoutItem in a grid,
///      adjusting the number of columns and rows to the current size.
///
/// VipDynGridLayout takes the space it gets, divides it up into rows and
/// columns, and puts each of the widgets it manages into the correct cell(s).
/// It lays out as many number of columns as possible (limited by maxColumns()).
class VIP_PLOTTING_EXPORT VipDynGridLayout : public QGraphicsLayout
{

public:
	/// \param parent Parent widget
	/// \param margin Margin
	/// \param spacing Spacing
	explicit VipDynGridLayout(double space = -1, QGraphicsLayoutItem* parent = nullptr);
	virtual ~VipDynGridLayout();

	void setAlignment(Qt::Alignment alignment);
	Qt::Alignment alignment() const;

	void setMargins(double);

	void setSpacing(double);
	double spacing() const;
	/// Limit the number of columns.
	/// \param maxColumns upper limit, 0 means unlimited
	/// \sa maxColumns()
	void setMaxColumns(uint maxCols);
	/// \brief Return the upper limit for the number of columns.
	///
	/// 0 means unlimited, what is the default.
	///
	/// \return Upper limit for the number of columns
	/// \sa setMaxColumns()
	uint maxColumns() const;

	/// \return Number of rows of the current layout.
	/// \sa numColumns()
	/// \warning The number of rows might change whenever the geometry changes
	uint numRows() const;

	/// \return Number of columns of the current layout.
	/// \sa numRows()
	/// \warning The number of columns might change whenever the geometry changes
	uint numColumns() const;

	/// \brief Add an item to the next free position.
	/// \param item Layout item
	void addItem(QGraphicsLayoutItem*);
	void insertItem(int, QGraphicsLayoutItem*);
	const QList<QGraphicsLayoutItem*>& items() const;
	QList<QGraphicsLayoutItem*> allItems() const;

	bool isVisible(const QGraphicsLayoutItem*) const;

	/// Set whether this layout can make use of more space than sizeHint().
	/// A value of Qt::Vertical or Qt::Horizontal means that it wants to grow in only
	/// one dimension, while Qt::Vertical | Qt::Horizontal means that it wants
	/// to grow in both dimensions. The default value is 0.
	///
	/// \param expanding Or'd orientations
	/// \sa expandingDirections()
	void setExpandingDirections(Qt::Orientations);

	/// \brief Returns whether this layout can make use of more space than sizeHint().
	///
	/// A value of Qt::Vertical or Qt::Horizontal means that it wants to grow in only
	/// one dimension, while Qt::Vertical | Qt::Horizontal means that it wants
	/// to grow in both dimensions.
	///
	/// \return Orientations, where the layout expands
	/// \sa setExpandingDirections()
	Qt::Orientations expandingDirections() const;
	/// Find the item at a specific index
	///
	/// \param index Index
	/// \return Item at a specific index
	/// \sa takeAt()
	virtual QGraphicsLayoutItem* itemAt(int i) const;
	virtual void removeAt(int i);
	void remove(QGraphicsLayoutItem*);

	/// Find the item at a specific index and remove it from the layout
	///
	/// \param index Index
	/// \return Layout item, removed from the layout
	/// \sa itemAt()
	QGraphicsLayoutItem* takeAt(int index);
	//! \return Number of items in the layout
	virtual int count() const;
	/// Invalidate all internal caches
	virtual void invalidate();

	/// Reorganizes columns and rows and resizes managed items within
	/// a rectangle.
	///
	/// \param rect Layout geometry
	virtual void setGeometry(const QRectF& rect);

	/// @brief Remove and delete all QGraphicsLayoutItem objects
	void clear();

	/// \return The preferred height for this layout, given a width.
	/// \sa hasHeightForWidth()
	double heightForWidth(double) const;

	/// Calculate the width of a layout for a given number of
	/// columns.
	///
	/// \param numColumns Given number of columns
	/// \param itemWidth Array of the width hints for all items
	double maxRowWidth(int numCols) const;

	/// \brief Calculate the number of columns for a given width.
	///
	/// The calculation tries to use as many columns as possible
	/// ( limited by maxColumns() )
	///
	/// \param width Available width for all columns
	/// \return Number of columns for a given width
	///
	/// \sa maxColumns(), setMaxColumns()
	uint columnsForWidth(double width) const;

	/// Returns the rectangle that should be covered when the geometry of
	/// this layout is set to \a r, provided that this layout supports
	/// setAlignment().
	///
	/// The result is derived from sizeHint() and expanding(). It is never
	/// larger than \a r.
	QRectF alignmentRect(const QRectF& r) const;

	virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const;

protected:
	/// Calculate the dimensions for the columns and rows for a grid
	/// of numColumns columns.
	///
	/// \param numColumns Number of columns.
	/// \param rowHeight Array where to fill in the calculated row heights.
	/// \param colWidth Array where to fill in the calculated column widths.
	void layoutGrid(uint numCols, QVector<double>& rowHeight, QVector<double>& colWidth) const;

	/// Stretch columns in case of expanding() & QSizePolicy::Horizontal and
	/// rows in case of expanding() & QSizePolicy::Vertical to fill the entire
	/// rect. Rows and columns are stretched with the same factor.
	///
	/// \param rect Bounding rectangle
	/// \param numColumns Number of columns
	/// \param rowHeight Array to be filled with the calculated row heights
	/// \param colWidth Array to be filled with the calculated column widths
	///
	/// \sa setExpanding(), expanding()
	void stretchGrid(const QRectF& rect, uint numCols, QVector<double>& rowHeight, QVector<double>& colWidth) const;

private:
	/// Initialize the layout with default values.
	void init();

	/// Calculate the geometries of the layout items for a layout
	/// with numColumns columns and a given rectangle.
	///
	/// \param rect Rect where to place the items
	/// \param numColumns Number of columns
	/// \return item geometries
	QList<QRectF> layoutItems(const QRectF&, uint numCols) const;
	/// \return the maximum width of all layout items
	double maxItemWidth() const;
	/// \return true: VipDynGridLayout implements heightForWidth().
	/// \sa heightForWidth()
	bool hasHeightForWidth() const;

	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @}
// end Plotting

#endif
