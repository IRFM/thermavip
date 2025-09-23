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

#include "VipDynGridLayout.h"

#include <QApplication>
#include <QStyle>

#include <limits>

class VipDynGridLayout::PrivateData
{
public:
	PrivateData()
	  : spacing(0)
	  , expanding(Qt::Horizontal)
	  , isDirty(true)
	{
	}

	void updateLayoutCache();

	mutable QList<QGraphicsLayoutItem*> itemList;
	mutable QList<QGraphicsLayoutItem*> hiddenItemList;

	uint maxColumns;
	uint numRows;
	uint numColumns;

	double spacing;

	Qt::Alignment alignment;
	Qt::Orientations expanding;

	bool isDirty;
	QVector<QSizeF> itemSizeHints;

	QRectF rect;
};

void VipDynGridLayout::PrivateData::updateLayoutCache()
{
	itemSizeHints.resize(itemList.count());

	int index = 0;

	for (QList<QGraphicsLayoutItem*>::iterator it = itemList.begin(); it != itemList.end(); ++it, index++) {
		itemSizeHints[index] = (*it)->effectiveSizeHint(Qt::PreferredSize);
	}

	isDirty = false;
}

VipDynGridLayout::VipDynGridLayout(double spacing, QGraphicsLayoutItem* parent)
  : QGraphicsLayout(parent)
{
	init();
	setSpacing(spacing);
}

void VipDynGridLayout::init()
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->maxColumns = d_data->numRows = d_data->numColumns = 0;
}

VipDynGridLayout::~VipDynGridLayout()
{
	for (int i = 0; i < d_data->itemList.size(); i++)
		delete d_data->itemList[i];
}

void VipDynGridLayout::setAlignment(Qt::Alignment alignment)
{
	d_data->alignment = alignment;
}

Qt::Alignment VipDynGridLayout::alignment() const
{
	return d_data->alignment;
}

void VipDynGridLayout::invalidate()
{
	d_data->isDirty = true;

	QList<QGraphicsLayoutItem*> hidden = d_data->hiddenItemList;
	QList<QGraphicsLayoutItem*> visible = d_data->itemList;

	// remove hidden items, add visible ones

	for (int i = 0; i < hidden.size(); ++i) {
		if (isVisible(hidden[i])) {
			d_data->itemList.append(hidden[i]);
			d_data->hiddenItemList.removeOne(hidden[i]);
		}
	}

	for (int i = 0; i < visible.size(); ++i) {
		if (!isVisible(visible[i])) {
			d_data->itemList.removeOne(visible[i]);
			d_data->hiddenItemList.append(visible[i]);
		}
	}

	QGraphicsLayout::invalidate();
}

void VipDynGridLayout::setMargins(double margin)
{
	this->setContentsMargins(margin, margin, margin, margin);
}

void VipDynGridLayout::setSpacing(double spacing)
{
	d_data->spacing = spacing;
}

double VipDynGridLayout::spacing() const
{
	return d_data->spacing;
}

bool VipDynGridLayout::isVisible(const QGraphicsLayoutItem* item) const
{
	QGraphicsItem* it = item->graphicsItem();
	if (it && !it->isVisible())
		return false;
	else
		return true;
}

void VipDynGridLayout::setMaxColumns(uint maxColumns)
{
	d_data->maxColumns = maxColumns;
}

uint VipDynGridLayout::maxColumns() const
{
	return d_data->maxColumns;
}

void VipDynGridLayout::addItem(QGraphicsLayoutItem* item)
{
	d_data->itemList.append(item);
	item->setParentLayoutItem(this);
	invalidate();
}

void VipDynGridLayout::insertItem(int index, QGraphicsLayoutItem* item)
{
	d_data->itemList.insert(index, item);
	item->setParentLayoutItem(this);
	invalidate();
}

const QList<QGraphicsLayoutItem*>& VipDynGridLayout::items() const
{
	return d_data->itemList;
}

QList<QGraphicsLayoutItem*> VipDynGridLayout::allItems() const
{
	return d_data->itemList + d_data->hiddenItemList;
}

QGraphicsLayoutItem* VipDynGridLayout::itemAt(int index) const
{
	if (index < 0 || index >= d_data->itemList.count())
		return nullptr;

	return d_data->itemList.at(index);
}

void VipDynGridLayout::removeAt(int i)
{
	if (i < 0 || i >= d_data->itemList.count())
		return;

	delete takeAt(i);
}

void VipDynGridLayout::remove(QGraphicsLayoutItem* item)
{
	int i = d_data->itemList.indexOf(item);
	if (i >= 0) {
		delete takeAt(i);
	}
	else {
		i = d_data->hiddenItemList.indexOf(item);
		if (i >= 0) {
			QGraphicsLayoutItem* it = d_data->hiddenItemList[i];
			d_data->hiddenItemList.removeAt(i);
			delete it;
		}
	}
}

QGraphicsLayoutItem* VipDynGridLayout::takeAt(int index)
{
	if (index < 0 || index >= d_data->itemList.count())
		return nullptr;

	d_data->isDirty = true;
	QGraphicsLayoutItem* item = d_data->itemList.takeAt(index);
	item->setParentLayoutItem(nullptr);
	return item;
}

int VipDynGridLayout::count() const
{
	return d_data->itemList.count();
}

void VipDynGridLayout::setExpandingDirections(Qt::Orientations expanding)
{
	d_data->expanding = expanding;
}

Qt::Orientations VipDynGridLayout::expandingDirections() const
{
	return d_data->expanding;
}

void VipDynGridLayout::setGeometry(const QRectF& rect)
{
	QGraphicsLayoutItem::setGeometry(rect);

	if (count() == 0)
		return;

	d_data->numColumns = columnsForWidth(rect.width());
	d_data->numRows = count() / d_data->numColumns;
	if (count() % d_data->numColumns)
		d_data->numRows++;

	QList<QRectF> itemGeometries = layoutItems(rect, d_data->numColumns);

	int index = 0;
	for (QList<QGraphicsLayoutItem*>::iterator it = d_data->itemList.begin(); it != d_data->itemList.end(); ++it) {
		QRectF r(itemGeometries[index]);
		(*it)->setGeometry(r);
		index++;
	}
}

void VipDynGridLayout::clear()
{
	for (QList<QGraphicsLayoutItem*>::iterator it = d_data->itemList.begin(); it != d_data->itemList.end(); ++it) {
		(*it)->setParentLayoutItem(nullptr);
		delete (*it);
	}

	d_data->itemList.clear();
	this->invalidate();
}

uint VipDynGridLayout::columnsForWidth(double width) const
{
	if (count() == 0)
		return 0;

	uint maxColumns = count();
	if (d_data->maxColumns > 0)
		maxColumns = qMin(d_data->maxColumns, maxColumns);

	if (maxRowWidth(maxColumns) <= width)
		return maxColumns;

	for (uint numColumns = 2; numColumns <= maxColumns; numColumns++) {
		const double rowWidth = maxRowWidth(numColumns);
		if (rowWidth > width)
			return numColumns - 1;
	}

	return 1; // At least 1 column
}

double VipDynGridLayout::maxRowWidth(int numColumns) const
{
	int col;

	QVector<double> colWidth(numColumns);
	for (col = 0; col < numColumns; col++)
		colWidth[col] = 0;

	if (d_data->isDirty)
		d_data->updateLayoutCache();

	for (int index = 0; index < d_data->itemSizeHints.count(); index++) {
		col = index % numColumns;
		colWidth[col] = qMax(colWidth[col], d_data->itemSizeHints[int(index)].width());
	}

	double l = 0, r = 0;
	//, t = 0, b = 0;
	// this->getContentsMargins(&l,&t,&r,&b);

	int rowWidth = l + r + (numColumns - 1) * spacing();
	for (col = 0; col < numColumns; col++)
		rowWidth += colWidth[col];

	return rowWidth;
}

double VipDynGridLayout::maxItemWidth() const
{
	if (count() == 0)
		return 0;

	if (d_data->isDirty)
		d_data->updateLayoutCache();

	double w = 0;
	for (int i = 0; i < d_data->itemSizeHints.count(); i++) {
		const double itemW = d_data->itemSizeHints[i].width();
		if (itemW > w)
			w = itemW;
	}

	return w;
}

QSizeF VipDynGridLayout::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
	Q_UNUSED(constraint)
	if (which == Qt::MinimumSize)
		return QSizeF(0, 0);
	else if (which == Qt::MaximumSize)
		return QSizeF(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
	else if (which == Qt::PreferredSize) {
		if (count() == 0)
			return QSizeF();

		double l, r, t, b;
		this->getContentsMargins(&l, &t, &r, &b);

		uint numColumns = count();
		if (d_data->maxColumns > 0)
			numColumns = qMin(d_data->maxColumns, numColumns);

		uint numRows = count() / numColumns;
		if (count() % numColumns)
			numRows++;

		QVector<double> rowHeight(numRows);
		QVector<double> colWidth(numColumns);

		layoutGrid(numColumns, rowHeight, colWidth);

		double h = t + b + (numRows - 1) * spacing();
		for (uint row = 0; row < numRows; row++)
			h += rowHeight[row];

		double w = l + r + (numColumns - 1) * spacing();
		for (uint col = 0; col < numColumns; col++)
			w += colWidth[col];

		return QSizeF(w, h);
	}
	else if (which == Qt::MinimumDescent)
		return QSizeF(0, 0);
	return QSizeF(0, 0);
}

QRectF VipDynGridLayout::alignmentRect(const QRectF& r) const
{
	return r;
	/* QSizeF s = sizeHint(Qt::PreferredSize);
    Qt::Alignment a = alignment();

    // This is a hack to obtain the real maximum size.
    VipDynGridLayout *that = const_cast<VipDynGridLayout *>(this);
    that->setAlignment(Qt::Alignment());
    QSizeF ms = that->maximumSize();
    that->setAlignment(a);

    if ((expandingDirections() & Qt::Horizontal) ||
	 !(a & Qt::AlignHorizontal_Mask)) {
	s.setWidth(qMin(r.width(), ms.width()));
    }
    if ((expandingDirections() & Qt::Vertical) ||
	 !(a & Qt::AlignVertical_Mask)) {
	s.setHeight(qMin(r.height(), ms.height()));
    } else if (hasHeightForWidth()) {
	double hfw = heightForWidth(s.width());
	if (hfw < s.height())
	    s.setHeight(qMin(hfw, ms.height()));
    }

    s = s.boundedTo(r.size());
    double x = r.x();
    double y = r.y();

    if (a & Qt::AlignBottom)
	y += (r.height() - s.height());
    else if (!(a & Qt::AlignTop))
	y += (r.height() - s.height()) / 2;

    a = QStyle::visualAlignment(QApplication::layoutDirection(), a);
    if (a & Qt::AlignRight)
	x += (r.width() - s.width());
    else if (!(a & Qt::AlignLeft))
	x += (r.width() - s.width()) / 2;

    return QRect(x, y, s.width(), s.height());*/
}

QList<QRectF> VipDynGridLayout::layoutItems(const QRectF& rect, uint numColumns) const
{
	QList<QRectF> itemGeometries;
	if (numColumns == 0 || count() == 0)
		return itemGeometries;

	uint numRows = count() / numColumns;
	if (numColumns % count())
		numRows++;

	if (numRows == 0)
		return itemGeometries;

	QVector<double> rowHeight(numRows);
	QVector<double> colWidth(numColumns);

	layoutGrid(numColumns, rowHeight, colWidth);

	bool expandH, expandV;
	expandH = expandingDirections() & Qt::Horizontal;
	expandV = expandingDirections() & Qt::Vertical;

	if (expandH || expandV)
		stretchGrid(rect, numColumns, rowHeight, colWidth);

	const int maxColumns = d_data->maxColumns;
	d_data->maxColumns = numColumns;
	const QRectF alignedRect = alignmentRect(rect);
	d_data->maxColumns = maxColumns;

	const double xOffset = alignedRect.x(); // expandH ? 0 : alignedRect.x();
	const double yOffset = alignedRect.y(); // expandV ? 0 : alignedRect.y();

	QVector<double> colX(numColumns);
	QVector<double> rowY(numRows);

	const double xySpace = spacing();
	double t = 0, l = 0;
	// double l=0,r=0,t=0,b=0;
	// this->getContentsMargins(&l,&t,&r,&b);

	rowY[0] = yOffset + t;
	for (uint ro = 1; ro < numRows; ro++)
		rowY[ro] = rowY[ro - 1] + rowHeight[ro - 1] + xySpace;

	colX[0] = xOffset + l;
	for (uint c = 1; c < numColumns; c++)
		colX[c] = colX[c - 1] + colWidth[c - 1] + xySpace;

	const int itemCount = d_data->itemList.size();
	for (int i = 0; i < itemCount; i++) {
		const int row = i / numColumns;
		const int col = i % numColumns;

		QRectF itemGeometry(colX[col], rowY[row], colWidth[col], rowHeight[row]);
		itemGeometries.append(itemGeometry);
	}

	return itemGeometries;
}

void VipDynGridLayout::layoutGrid(uint numColumns, QVector<double>& rowHeight, QVector<double>& colWidth) const
{
	if (numColumns <= 0)
		return;

	if (d_data->isDirty)
		d_data->updateLayoutCache();

	for (int index = 0; index < d_data->itemSizeHints.count(); index++) {
		const int row = index / numColumns;
		const int col = index % numColumns;

		const QSizeF& size = d_data->itemSizeHints[int(index)];

		rowHeight[row] = (col == 0) ? size.height() : qMax(rowHeight[row], size.height());
		colWidth[col] = (row == 0) ? size.width() : qMax(colWidth[col], size.width());
	}
}

bool VipDynGridLayout::hasHeightForWidth() const
{
	return true;
}

double VipDynGridLayout::heightForWidth(double width) const
{
	if (count() == 0)
		return 0;

	const uint numColumns = columnsForWidth(width);
	uint numRows = count() / numColumns;
	if (count() % numColumns)
		numRows++;

	QVector<double> rowHeight(numRows);
	QVector<double> colWidth(numColumns);

	layoutGrid(numColumns, rowHeight, colWidth);

	double l, t, r, b;
	this->getContentsMargins(&l, &t, &r, &b);

	double h = t + b + (numRows - 1) * spacing();
	for (uint row = 0; row < numRows; row++)
		h += rowHeight[row];

	return h;
}

void VipDynGridLayout::stretchGrid(const QRectF& rect, uint numColumns, QVector<double>& rowHeight, QVector<double>& colWidth) const
{
	if (numColumns == 0 || count() == 0)
		return;

	bool expandH, expandV;
	expandH = expandingDirections() & Qt::Horizontal;
	expandV = expandingDirections() & Qt::Vertical;

	double l, t, r, b;
	this->getContentsMargins(&l, &t, &r, &b);

	if (expandH) {
		double xDelta = rect.width() - (l + r) - (numColumns - 1) * spacing();
		for (uint col = 0; col < numColumns; col++)
			xDelta -= colWidth[col];

		if (xDelta > 0) {
			for (uint col = 0; col < numColumns; col++) {
				const double space = xDelta / (numColumns - col);
				colWidth[col] += space;
				xDelta -= space;
			}
		}
	}

	if (expandV) {
		uint numRows = count() / numColumns;
		if (count() % numColumns)
			numRows++;

		double yDelta = rect.height() - (t + b) - (numRows - 1) * spacing();
		for (uint row = 0; row < numRows; row++)
			yDelta -= rowHeight[row];

		if (yDelta > 0) {
			for (uint row = 0; row < numRows; row++) {
				const double space = yDelta / (numRows - row);
				rowHeight[row] += space;
				yDelta -= space;
			}
		}
	}
}

uint VipDynGridLayout::numRows() const
{
	return d_data->numRows;
}

uint VipDynGridLayout::numColumns() const
{
	return d_data->numColumns;
}
