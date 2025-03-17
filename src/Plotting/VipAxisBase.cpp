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

#include "VipAxisBase.h"
#include "VipPainter.h"
#include "VipPlotItem.h"

static int registerAxisBaseKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["title-inverted"] = VipParserPtr(new BoolParser());
		keywords["title-inside"] = VipParserPtr(new BoolParser());
		keywords["use-border-dist-hint-for-layout"] = VipParserPtr(new BoolParser());

		vipSetKeyWordsForClass(&VipAxisBase::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerAxisBaseKeyWords = registerAxisBaseKeyWords();

class VipAxisBase::PrivateData
{
public:
	PrivateData()
	  : titleOffset(0)
	  , length(0)
	  , mapScaleToScene(false)
	  , useBorderDistHintForLayout(false)
	  , titleInside(false)
	  , mergeExponent(false)
	{
	}

	double titleOffset;
	double length;
	VipAxisBase::LayoutFlags layoutFlags;
	bool mapScaleToScene;
	bool useBorderDistHintForLayout;
	bool titleInside;
	bool mergeExponent;
};

VipAxisBase::VipAxisBase(Alignment pos, QGraphicsItem* parent)
  : VipBorderItem(pos, parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	// this->setLength(0);

	d_data->layoutFlags = LayoutFlags();
	if (pos == Right)
		d_data->layoutFlags |= TitleInverted;

	scaleDraw()->setAlignment(VipScaleDraw::Alignment(int(pos)));
	scaleDraw()->setLength(10);
	setScale(0, 100);
	setZValue(10);
}

VipAxisBase::~VipAxisBase()
{
}

/// Toggle an layout flag
///
/// \param flag Layout flag
/// \param on true/false
///
/// \sa testLayoutFlag(), LayoutFlag
void VipAxisBase::setLayoutFlag(LayoutFlag flag, bool on)
{
	if (((d_data->layoutFlags & flag) != 0) != on) {
		if (on)
			d_data->layoutFlags |= flag;
		else
			d_data->layoutFlags &= ~flag;
	}

	emitScaleNeedUpdate();
}

/// Test a layout flag
///
/// \param flag Layout flag
/// \return true/false
/// \sa setLayoutFlag(), LayoutFlag
bool VipAxisBase::testLayoutFlag(LayoutFlag flag) const
{
	return (d_data->layoutFlags & flag);
}

void VipAxisBase::setTitleInverted(bool inverted)
{
	setLayoutFlag(TitleInverted, inverted);
}

bool VipAxisBase::isTitleInverted() const
{
	return testLayoutFlag(TitleInverted);
}

void VipAxisBase::setTitleInside(bool enable)
{
	if (d_data->titleInside != enable) {
		d_data->titleInside = enable;
		markStyleSheetDirty();
		emitGeometryNeedUpdate();
	}
}
bool VipAxisBase::titleInside() const
{
	return d_data->titleInside;
}

void VipAxisBase::setMapScaleToScene(bool enable)
{
	d_data->mapScaleToScene = enable;
	computeScaleDiv();
}

bool VipAxisBase::isMapScaleToScene() const
{
	return d_data->mapScaleToScene;
}

/// Change the alignment
///
/// \param alignment New alignment
/// \sa alignment()
void VipAxisBase::setAlignment(Alignment alignment)
{
	scaleDraw()->setAlignment(VipScaleDraw::Alignment(alignment));
	VipBorderItem::setAlignment(alignment);
	markStyleSheetDirty();
	// if ( !testAttribute( Qt::WA_WState_OwnSizePolicy ) )
	// {
	//  QSizePolicy policy( QSizePolicy::MinimumExpanding,
	//      QSizePolicy::Fixed );
	//  if ( scaleDraw()->orientation() == Qt::Vertical )
	//      policy.transpose();
	//
	//  setSizePolicy( policy );
	//
	//  setAttribute( Qt::WA_WState_OwnSizePolicy, false );
	// }

	emitGeometryNeedUpdate();
}

QPointF VipAxisBase::scalePosition() const
{
	return constScaleDraw()->pos();
}

QPointF VipAxisBase::scaleEndPosition() const
{
	return constScaleDraw()->end();
}

void VipAxisBase::itemGeometryChanged(const QRectF&)
{
	layoutScale();
	if (this->isMapScaleToScene())
		computeScaleDiv();
}

void VipAxisBase::setScaleDraw(VipAbstractScaleDraw* _scaleDraw)
{
	VipScaleDraw* scale = qobject_cast<VipScaleDraw*>(_scaleDraw);
	if ((scale == nullptr) || (scale == scaleDraw()))
		return;

	const VipScaleDraw* sd = scaleDraw();
	if (sd) {
		scale->setAlignment(sd->alignment());
		scale->setScaleDiv(sd->scaleDiv());

		VipValueTransform* transform = nullptr;
		if (sd->scaleMap().transformation())
			transform = sd->scaleMap().transformation()->copy();

		scale->setTransformation(transform);
	}

	VipAbstractScale::setScaleDraw(scale);
}

const VipScaleDraw* VipAxisBase::constScaleDraw() const
{
	return static_cast<const VipScaleDraw*>(VipAbstractScale::constScaleDraw());
}

VipScaleDraw* VipAxisBase::scaleDraw()
{
	return static_cast<VipScaleDraw*>(VipAbstractScale::scaleDraw());
}

void VipAxisBase::getBorderDistHint(double& start, double& end) const
{
	constScaleDraw()->getBorderDistHint(start, end);
	VipAbstractScale::getBorderDistHint(start, end);
}

void VipAxisBase::setUseBorderDistHintForLayout(bool enable)
{
	if (d_data->useBorderDistHintForLayout != enable) {
		d_data->useBorderDistHintForLayout = enable;
		emitScaleNeedUpdate();
	}
}
bool VipAxisBase::useBorderDistHintForLayout() const
{
	return d_data->useBorderDistHintForLayout;
}

bool VipAxisBase::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "title-inverted") == 0) {
		setTitleInverted(value.toBool());
		return true;
	}
	if (strcmp(name, "title-inside") == 0) {
		setTitleInside(value.toBool());
		return true;
	}
	if (strcmp(name, "use-border-dist-hint-for-layout") == 0) {
		setUseBorderDistHintForLayout(value.toBool());
		return true;
	}
	return VipBorderItem::setItemProperty(name, value, index);
}

bool VipAxisBase::hasState(const QByteArray& state, bool enable) const
{
	if (state == "title")
		return property("_vip_title").toBool() == enable;
	if (state == "legend")
		return property("_vip_legend").toBool() == enable;
	return VipBorderItem::hasState(state, enable);
}

/// Recalculate the scale's geometry and layout based on
/// the current geometry and fonts.
///
/// \param update_geometry Notify the layout system and call update
///                      to redraw the scale

void VipAxisBase::layoutScale()
{
	double bd0 = 0, bd1 = 0;
	if (d_data->useBorderDistHintForLayout) {
		getBorderDistHint(bd0, bd1);
		if (startBorderDist() > bd0)
			bd0 = startBorderDist();
		if (endBorderDist() > bd1)
			bd1 = endBorderDist();
	}

	double colorBarWidth = 0.;
	if (additionalSpace() != 0.)
		colorBarWidth = additionalSpace(); //+ spacing();

	QRectF r = boundingRectNoCorners();
	// TEST
	if (r == QRectF())
		r = geometry().translated(-pos());

	double x, y, length;

	if (constScaleDraw()->orientation() == Qt::Vertical) {
		y = r.top() + bd0;
		length = r.height() - (bd0 + bd1);

		if (constScaleDraw()->alignment() == VipScaleDraw::LeftScale)
			x = r.right() //- 1.0
			    - margin() - colorBarWidth;
		else
			x = r.left() + margin() + colorBarWidth;
	}
	else {
		x = r.left() + bd0;
		length = r.width() - (bd0 + bd1);

		if (constScaleDraw()->alignment() == VipScaleDraw::BottomScale)
			y = r.top() + margin() + colorBarWidth;
		else
			y = r.bottom() //- 1.0
			    - margin() - colorBarWidth;
	}

	if (constScaleDraw()->pos() != QPointF(x, y)) {
		scaleDraw()->move(x, y);
	}

	if (constScaleDraw()->length() != length) {
		scaleDraw()->setLength(length);
	}

	double scale_draw_extent = constScaleDraw()->fullExtent();

	if (!titleInside()) {
		d_data->titleOffset = margin() + spacing() + colorBarWidth + scale_draw_extent;
	}
	else {
		d_data->titleOffset = margin();
		d_data->titleOffset -= constScaleDraw()->hasComponent(VipScaleDraw::Backbone) ? constScaleDraw()->componentPen(VipScaleDraw::Backbone).widthF() : 0;
		d_data->titleOffset -= title().textSize().height() + 1;
		if (constScaleDraw()->ticksPosition() == VipScaleDraw::TicksInside && constScaleDraw()->hasComponent(VipScaleDraw::Ticks))
			d_data->titleOffset -= constScaleDraw()->tickLength(VipScaleDiv::MajorTick);
		if (constScaleDraw()->textPosition() == VipScaleDraw::TextInside && constScaleDraw()->hasComponent(VipScaleDraw::Labels)) {
			double d = 0;
			if (orientation() == Qt::Vertical) {
				d = constScaleDraw()->maxLabelWidth(VipScaleDiv::MajorTick);
				d = qMax(d, constScaleDraw()->maxLabelWidth(VipScaleDiv::MediumTick));
				d = qMax(d, constScaleDraw()->maxLabelWidth(VipScaleDiv::MinorTick));
			}
			else {
				d = constScaleDraw()->maxLabelHeight(VipScaleDiv::MajorTick);
				d = qMax(d, constScaleDraw()->maxLabelHeight(VipScaleDiv::MediumTick));
				d = qMax(d, constScaleDraw()->maxLabelHeight(VipScaleDiv::MinorTick));
			}
			if (d > 0)
				d += spacing();
			d_data->titleOffset -= d;
		}
	}

	d_data->length = minimumLengthHint();

	// compute the full axis extent, we will use it for title drawing in case of axis intersection
	/*d_data->fullExtent = scale_draw_extent + margin() + extent + 1 + additionalSpace();
	if ((!title().isEmpty() && isDrawTitleEnabled() && !titleInside()) || constScaleDraw()->valueToText()->exponent() != 0) {
		if (!title().isEmpty() && isDrawTitleEnabled() && !titleInside())
			d_data->fullExtent += title().textSize().height() + spacing();
		if (constScaleDraw()->valueToText()->exponent() != 0) {
			if (d_data->mergeExponent)
				d_data->fullExtent += title().textSize().height();
		}
	}*/

	// notifyScaleDivChanged();
	this->update();
}

QRectF VipAxisBase::boundingRect() const
{
	QRectF r = VipAbstractScale::boundingRect();
	const VipScaleDraw* draw = this->constScaleDraw();
	double start, end;
	double len = draw->tickLength(VipScaleDiv::MajorTick);
	getBorderDistHint(start, end);
	switch (orientation()) {
		case Qt::Vertical:
			if (len > 0 && draw->ticksPosition() == VipScaleDraw::TicksOutside) {
				// adjust left and right
				r.setLeft(r.left() - len);
				r.setRight(r.right() + len);
			}
			r.setTop(r.top() - start);
			r.setBottom(r.bottom() + end);
			break;
		default:
			if (len > 0 && draw->ticksPosition() == VipScaleDraw::TicksOutside) {
				r.setTop(r.top() - len);
				r.setBottom(r.bottom() + len);
			}
			r.setLeft(r.left() - start);
			r.setRight(r.right() + end);
			break;
	}
	return r;
}

void VipAxisBase::computeScaleDiv()
{
	// if the Axis d_data->_scaleDivRange is set to MapToScene or Automatic,
	// recompute the scale dive ranges

	if (isMapScaleToScene()) {
		if (!this->scene())
			return;

		VipScaleDiv div = constScaleDraw()->scaleDiv();
		QTransform tr = this->globalSceneTransform();
		QRectF r = this->boundingRectNoCorners();

		// TEST
		if (r == QRectF())
			r = this->geometry().translated(-pos());

		if (constScaleDraw()->orientation() == Qt::Horizontal) {
			r.setLeft(r.left() + startBorderDist());
			r.setWidth(r.width() - endBorderDist());

			QPointF start = tr.map(r.bottomLeft());
			QPointF end = tr.map(r.bottomRight());

			this->setScaleDiv(scaleEngine()->divideScale(start.x(), end.x(), maxMajor(), maxMinor()));
		}
		else {
			r.setTop(r.top() + startBorderDist());
			r.setHeight(r.height() - endBorderDist());

			QPointF start = tr.map(r.topLeft());
			QPointF end = tr.map(r.bottomLeft());

			this->setScaleDiv(scaleEngine()->divideScale(end.y(), start.y(), maxMajor(), maxMinor()));
		}
	}
	else {
		VipAbstractScale::computeScaleDiv();
	}
}

/// Rotate and paint a title according to its position into a given rectangle.
///
/// \param painter VipPainter
/// \param align Alignment
/// \param rect Bounding rectangle

void VipAxisBase::drawTitle(QPainter* painter, Alignment align, const QRectF& rect) const
{
	QRectF r = rect;
	double angle;
	int flags = title().alignment() & ~(Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter);

	/* int sign = 1;
	QSizeF tsize(0, 0);
	if (titleInside()) {
		sign = -1;
	}*/

	VipText exponent;
	if (constScaleDraw()->valueToText()->supportExponent())
		if (int exp = constScaleDraw()->valueToText()->exponent()) {
			exponent.setTextStyle(title().textStyle());
			exponent.setText(" &#215;10<sup>" + QString::number(exp) + "</sup>");
		}

	// check if we need to merge the title and the exponent
	const double move_exponent = 10;
	int title_size = title().textSize().width();
	d_data->mergeExponent = false;

	if (!exponent.isEmpty()) {
		if (orientation() == Qt::Horizontal && ((r.width() - title_size) / 2 - move_exponent) < exponent.textSize().width()) {
			// if title and exponent do not fit in given rect, extend it to the full bounding rect
			r = this->boundingRect();
			d_data->mergeExponent = true;
		}
		else if (orientation() == Qt::Vertical && ((r.height() - title_size) / 2 - move_exponent) < exponent.textSize().width()) {
			r = this->boundingRect();
			d_data->mergeExponent = true;
		}
	}

	switch (align) {
		case Left:
			angle = -90.0;
			flags |= Qt::AlignTop;

			if (titleInside())
				r.setRect(r.right() - (d_data->titleOffset) - title().textSize().height(), r.bottom(), r.height(), title().textSize().height());
			else
				r.setRect(r.left(), r.bottom(), r.height(), r.width() - (d_data->titleOffset));
			break;

		case Right:
			angle = -90.0;
			flags |= Qt::AlignTop;
			if (titleInside())
				r.setRect(r.left() + (d_data->titleOffset), r.bottom(), r.height(), title().textSize().height());
			else
				r.setRect(r.left() + (d_data->titleOffset), r.bottom(), r.height(), r.width() - (d_data->titleOffset));

			break;

		case Bottom:
			angle = 0.0;
			flags |= Qt::AlignBottom;
			r.setTop(r.top() + (d_data->titleOffset));
			if (titleInside())
				r.setBottom(r.top() + title().textSize().height());

			break;

		case Top:
		default:
			angle = 0.0;
			flags |= Qt::AlignBottom;
			r.setBottom(r.bottom() - (d_data->titleOffset));
			if (titleInside())
				r.setTop(r.bottom() - title().textSize().height());
			break;
	}

	if (d_data->layoutFlags & TitleInverted) {
		if (align == Left || align == Right) {
			// angle = -90;
			angle = -angle;
			r.setRect(r.x() + r.height(), r.y() - r.width(), r.width(), r.height());
		}
	}

	painter->save();

	QTransform tr;
	tr.translate(r.x(), r.y());
	if (angle != 0.0)
		tr.rotate(angle);
	painter->setTransform(tr, true);
	// painter->translate(r.x(), r.y());
	// if (angle != 0.0)
	//	painter->rotate(angle);

	// draw title
	VipText t = title();
	if (!isDrawTitleEnabled())
		t.setText(QString());
	if (d_data->mergeExponent && !exponent.isEmpty())
		t.setText(t.text() + "<br>" + exponent.text());

	// make sure text fits to rect width
	QSizeF ts = t.textSize();
	if (ts.width() > r.width()) {
		QPointF center = r.center();
		r.setWidth(ts.width());
		r.moveCenter(center);
	}

	if (!title().isEmpty()) {
		t.setAlignment(Qt::Alignment(flags));
		t.draw(painter, QRectF(0.0, 0.0, r.width(), r.height()));
	}

	// draw exponent
	if (!d_data->mergeExponent && !exponent.isEmpty()) {
		QRectF tmp(0.0, 0.0, r.width(), r.height());
		bool inverted = isScaleInverted() ^ (scaleDiv().lowerBound() > scaleDiv().upperBound());
		int exp_flags = flags & ~(Qt::AlignCenter);
		switch (align) {
			case Left:
				if (inverted) {
					exponent.setAlignment(Qt::Alignment(exp_flags | Qt::AlignLeft));
					tmp.setLeft(tmp.left() + move_exponent);
				}
				else {
					exponent.setAlignment(Qt::Alignment(exp_flags | Qt::AlignRight));
					tmp.setRight(tmp.right() - move_exponent);
				}
				break;
			case Right:
				if (inverted) {
					exponent.setAlignment(Qt::Alignment(exp_flags | Qt::AlignRight));
					tmp.setRight(tmp.right() - move_exponent);
				}
				else {
					exponent.setAlignment(Qt::Alignment(exp_flags | Qt::AlignLeft));
					tmp.setLeft(tmp.left() + move_exponent);
				}
				break;
			case Top:
			case Bottom:
			default:
				if (inverted) {
					exponent.setAlignment(Qt::Alignment(exp_flags | Qt::AlignLeft));
					tmp.setLeft(tmp.left() + move_exponent);
				}
				else {
					exponent.setAlignment(Qt::Alignment(exp_flags | Qt::AlignRight));
					tmp.setRight(tmp.right() - move_exponent);
				}
				break;
		}
		exponent.draw(painter, tmp);
	}

	painter->restore();
}

/// \return a minimum size hint
QSizeF VipAxisBase::minimumSizeHint() const
{
	const Qt::Orientation o = constScaleDraw()->orientation();

	QFont scale_font = constScaleDraw()->textStyle().font();

	// Border Distance cannot be less than the scale borderDistHint
	// Note, the borderDistHint is already included in minHeight/minWidth
	double length = 0;
	if (d_data->useBorderDistHintForLayout) {
		double mbd1, mbd2;
		getBorderDistHint(mbd1, mbd2);
		length += qMax(0., startBorderDist() - mbd1);
		length += qMax(0., endBorderDist() - mbd2);
	}
	length += constScaleDraw()->minLength();

	double dim = dimForLength(length, scale_font);
	if (length < dim) {
		// compensate for long titles
		length = dim;
		dim = dimForLength(length, scale_font);
	}

	QSizeF size(length // + 2
		    ,
		    dim);
	if (o == Qt::Vertical)
		size.transpose();

	// int left, right, top, bottom;
	// getContentsMargins( &left, &top, &right, &bottom );
	return size; //+ QSize( left + right, top + bottom );
}

double VipAxisBase::minimumLengthHint() const
{
	QSizeF s = minimumSizeHint();
	if (orientation() == Qt::Vertical)
		return s.width();
	else
		return s.height();
}

double VipAxisBase::extentForLength(double length) const
{
	// return d_data->length;
	return dimForLength(length, constScaleDraw()->textStyle().font());
}

/// \brief Find the minimum dimension for a given length.
///      dim is the height, length the width seen in
///      direction of the title.
/// \param length width for horizontal, height for vertical scales
/// \param scaleFont Font of the scale
/// \return height for horizontal, width for vertical scales

double VipAxisBase::dimForLength(double length, const QFont& // scaleFont
) const
{
	Q_UNUSED(length)
	const double extent = // std::ceil
	  (constScaleDraw()->fullExtent());

	double dim = margin() + extent + 1;

	if ((!title().isEmpty() && isDrawTitleEnabled() && !titleInside()) || constScaleDraw()->valueToText()->exponent() != 0) {
		if (!title().isEmpty() && isDrawTitleEnabled() && !titleInside())
			dim += title().textSize().height() + spacing();
		if (constScaleDraw()->valueToText()->exponent() != 0) {
			if (d_data->mergeExponent)
				dim += title().textSize().height();
		}
	}

	if (double space = additionalSpace())
		dim += space;

	return dim;
}

void VipAxisBase::draw(QPainter* painter, QWidget* widget)
{
	painter->save();

	VipBorderItem::draw(painter, widget);
	painter->setRenderHints(this->renderHints());

	// if (this->alignment() == Left) {
	// const QTransform &tr = painter->transform();
	// const QPointF pt = this->pos();
	// bool stop = true;
	// }

	QRectF brect = this->boundingRectNoCorners();
	// TEST
	if (brect == QRectF())
		brect = this->geometry().translated(-pos());

	constScaleDraw()->draw(painter);

	if (constScaleDraw()->orientation() == Qt::Horizontal) {
		brect.setLeft(brect.left() + startBorderDist());
		brect.setWidth(brect.width() - endBorderDist());
	}
	else {
		brect.setTop(brect.top() + startBorderDist());
		brect.setHeight(brect.height() - endBorderDist());
	}

	if ((!title().isEmpty() && isDrawTitleEnabled()) || constScaleDraw()->valueToText()->exponent() != 0) {

		drawTitle(painter, this->alignment(), brect);
	}

	// draw the exponent
	// if (int exp = constScaleDraw()->valueToText()->exponent()) {
	// double len = constScaleDraw()->length();
	// QRectF r = this->boundingRectNoCorners();
	// VipText t;
	// t.setText("1e" + QString::number(exp));
	// t.setTextStyle(constScaleDraw()->textStyle());
	// t.setAlignment(constScaleDraw()->textStyle().alignment());
	// QPointF pos;
	// switch (this->alignment()) {
	// case Left:
	// pos = r.topRight() - QPointF(len + t.textSize().width(), t.textSize().height());
	// break;
	// case Right:
	// //pos = r.topRight() - QPointF(len + t.textSize().width(), t.textSize().height());
	// break;
	// case Top:
	// break;
	// case Bottom:
	// break;
	// }
	// }

	painter->restore();
}

// QRectF VipAxisBase::plottingRect (const VipAxisBase * x, const VipAxisBase * y)
// {
// QTransform tr_x = x->parentTransform();
// QTransform tr_y = y->parentTransform();
//
// QRectF xb = x->boundingRectNoCorners();
// QRectF yb = y->boundingRectNoCorners();
//
// if ( x->orientation() == Qt::Horizontal )
// {
// xb.setLeft( xb.left() + x->startBorderDist() );
// xb.setWidth( xb.width() - x->endBorderDist() );
// yb.setTop( yb.top() + y->startBorderDist() );
// yb.setHeight( yb.height() - y->endBorderDist() );
// }
// else
// {
// yb.setLeft( yb.left() + x->startBorderDist() );
// yb.setWidth( yb.width() - x->endBorderDist() );
// xb.setTop( xb.top() + y->startBorderDist() );
// xb.setHeight( xb.height() - y->endBorderDist() );
// }
//
// xb = tr_x.map(xb).boundingRect();
// yb = tr_y.map(yb).boundingRect();
// QRectF res;
//
// res.setLeft(xb.left());
// res.setRight(xb.right());
// res.setTop(yb.top());
// res.setBottom(yb.bottom());
//
//
// return res;
// }

class VipMultiAxisBase::PrivateData
{
public:
	QList<QPointer<VipBorderItem>> scales;
	double scaleSpacing;
	LayoutFlags layoutFlags;
};

VipMultiAxisBase::VipMultiAxisBase(Alignment pos, QGraphicsItem* parent)
  : VipBorderItem(pos, parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->scaleSpacing = 0;
	d_data->layoutFlags = LayoutFlags();
	if (pos == Right)
		d_data->layoutFlags |= TitleInverted;

	setScale(0, 100);
	this->setZValue(10);
	this->setProperty("_vip_ignoreToolTip", true);
}
VipMultiAxisBase::~VipMultiAxisBase()
{
	for (int i = 0; i < count(); ++i)
		if (at(i))
			delete at(i);
}

void VipMultiAxisBase::setAlignment(Alignment align)
{
	for (int i = 0; i < count(); ++i)
		if (at(i))
			at(i)->setAlignment(align);
		else {
			d_data->scales.removeAt(i);
			--i;
		}
	VipBorderItem::setAlignment(align);
}

/// Toggle an layout flag
///
/// \param flag Layout flag
/// \param on true/false
///
/// \sa testLayoutFlag(), LayoutFlag
void VipMultiAxisBase::setLayoutFlag(LayoutFlag flag, bool on)
{
	if (((d_data->layoutFlags & flag) != 0) != on) {
		if (on)
			d_data->layoutFlags |= flag;
		else
			d_data->layoutFlags &= ~flag;
	}

	emitScaleNeedUpdate();
}

/// Test a layout flag
///
/// \param flag Layout flag
/// \return true/false
/// \sa setLayoutFlag(), LayoutFlag
bool VipMultiAxisBase::testLayoutFlag(LayoutFlag flag) const
{
	return (d_data->layoutFlags & flag);
}

void VipMultiAxisBase::setTitleInverted(bool inverted)
{
	setLayoutFlag(TitleInverted, inverted);
}

bool VipMultiAxisBase::isTitleInverted() const
{
	return testLayoutFlag(TitleInverted);
}

void VipMultiAxisBase::setItemIntervalFactor(double f)
{
	for (int i = 0; i < d_data->scales.size(); ++i)
		d_data->scales[i]->setItemIntervalFactor(f);
	VipAbstractScale::setItemIntervalFactor(f);
}

VipMultiAxisBase* VipMultiAxisBase::fromScale(VipBorderItem* item)
{
	return item->property("_vip_VipMultiAxisBase").value<VipMultiAxisBase*>();
}

void VipMultiAxisBase::setScaleSpacing(double space)
{
	d_data->scaleSpacing = space;
	markStyleSheetDirty();
	emitGeometryNeedUpdate();
}
double VipMultiAxisBase::scaleSpacing() const
{
	return d_data->scaleSpacing;
}

void VipMultiAxisBase::addScale(VipBorderItem* it)
{
	if (indexOf(it) < 0) {
		d_data->scales.append(it);
		it->setAlignment(this->alignment());
		it->setProperty("_vip_VipMultiAxisBase", QVariant::fromValue((QObject*)this));
		it->setProperty("_vip_ignore_geometry", true);
		it->setItemIntervalFactor(this->itemIntervalFactor());
		updateParents();
		emitGeometryNeedUpdate();
		connect(it, SIGNAL(geometryNeedUpdate()), this, SLOT(emitGeometryNeedUpdate()), Qt::DirectConnection);
	}
}

void VipMultiAxisBase::insertScale(int index, VipBorderItem* it)
{
	if (indexOf(it) < 0) {
		d_data->scales.insert(index, it);
		it->setProperty("_vip_VipMultiAxisBase", QVariant::fromValue((QObject*)this));
		it->setProperty("_vip_ignore_geometry", true);
		it->setItemIntervalFactor(this->itemIntervalFactor());
		updateParents();
		emitGeometryNeedUpdate();
		connect(it, SIGNAL(geometryNeedUpdate()), this, SLOT(emitGeometryNeedUpdate()), Qt::DirectConnection);
	}
}
VipBorderItem* VipMultiAxisBase::takeItem(int index)
{
	VipBorderItem* it = at(index);
	it->setProperty("_vip_VipMultiAxisBase", QVariant::fromValue((QObject*)nullptr));
	it->setProperty("_vip_ignore_geometry", QVariant());
	it->setParentItem(nullptr);
	d_data->scales.removeAt(index);
	emitGeometryNeedUpdate();
	disconnect(it, SIGNAL(geometryNeedUpdate()), this, SLOT(emitGeometryNeedUpdate()));
	return it;
}
void VipMultiAxisBase::remove(VipBorderItem* it)
{
	d_data->scales.removeOne(it);
	delete it;
	emitGeometryNeedUpdate();
}
int VipMultiAxisBase::indexOf(const VipBorderItem* it) const
{
	return d_data->scales.indexOf(const_cast<VipBorderItem*>(it));
}
int VipMultiAxisBase::count() const
{
	return d_data->scales.size();
}
VipBorderItem* VipMultiAxisBase::at(int index)
{
	return d_data->scales[index];
}
const VipBorderItem* VipMultiAxisBase::at(int index) const
{
	return d_data->scales[index];
}

double VipMultiAxisBase::titleExtent() const
{
	if ((!title().isEmpty() && isDrawTitleEnabled())) {
		if (!title().isEmpty() && isDrawTitleEnabled())
			return title().textSize().height() + spacing();
	}
	return 0;
}

double VipMultiAxisBase::extentForLength(double length) const
{
	if (d_data->scales.isEmpty())
		return 0;
	double ext = 0;
	for (int i = 0; i < count(); ++i) {
		if (at(i)) {
			if (at(i)->isVisible()) {
				if (ext == 0)
					ext = at(i)->extentForLength(length);
				else
					ext = qMax(ext, at(i)->extentForLength(length));
			}
		}
		else {
			d_data->scales.removeAt(i);
			--i;
		}
	}

	ext += titleExtent();
	ext += margin();
	return ext;
}

void VipMultiAxisBase::itemGeometryChanged(const QRectF&)
{
	layoutScale();
}

void VipMultiAxisBase::updateParents()
{
	for (int i = 0; i < count(); ++i) {
		if (VipBorderItem* it = at(i)) {
			if (parentItem()) {
				if (it->parentItem() != parentItem())
					it->setParentItem(parentItem());
			}
			else if (scene() && it->scene() != scene()) {
				scene()->addItem(it);
			}
			if (it->isVisible() != isVisible())
				it->setVisible(isVisible());
		}
		else {
			d_data->scales.removeAt(i);
			--i;
		}
	}
}

QVariant VipMultiAxisBase::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemVisibleHasChanged) {
		updateParents();
	}
	else if (change == QGraphicsItem::ItemParentHasChanged) {
		updateParents();
	}

	return VipBorderItem::itemChange(change, value);
}

void VipMultiAxisBase::layoutScale()
{
	// get number of visible items
	int vis_items = 0;
	for (int i = 0; i < count(); ++i)
		if (at(i))
			vis_items += (int)at(i)->isVisible();
		else {
			d_data->scales.removeAt(i);
			--i;
		}

	QRectF r = boundingRectNoCorners();
	// TEST
	if (r == QRectF())
		r = this->geometry();
	else
		r = r.translated(pos());

	if (orientation() == Qt::Vertical) {
		double length = r.height() / vis_items;
		double bottom = r.bottom();
		for (int i = 0; i < count(); ++i) {
			if (VipBorderItem* it = at(i)) {
				if (!it->isVisible())
					continue;
				double top = bottom -= length;
				double width = it->extentForLength(length);
				double right, left;
				if (alignment() == VipBorderItem::Left) {
					right = r.right();
					left = right - width;
					left -= margin();
				}
				else {
					left = r.left();
					right = left + width;
					left += margin();
				}

				double item_length = length;
				if (i == 0) {
					top += scaleSpacing() / 2;
					item_length -= scaleSpacing() / 2;
				}
				else if (i == count() - 1) {
					item_length -= scaleSpacing() / 2;
				}
				else {
					top += scaleSpacing() / 2;
					item_length -= scaleSpacing();
				}

				it->setGeometry(QRectF(left, top, width, item_length));
				it->setBoundingRectNoCorners(QRectF(0, 0, width, item_length));
				it->layoutScale();
			}
			else {
				d_data->scales.removeAt(i);
				--i;
			}
		}
	}
	else {
		double length = r.width() / vis_items;
		double left = r.left();
		for (int i = 0; i < count(); ++i) {
			if (VipBorderItem* it = at(i)) {
				if (!it->isVisible())
					continue;
				double height = it->extentForLength(length);
				double top, bottom;
				if (alignment() == VipBorderItem::Top) {
					bottom = r.bottom();
					top = bottom - height;
					top -= margin();
				}
				else {
					top = r.top();
					bottom = top + height;
					top += margin();
				}

				double item_length = length;
				double item_left = left;
				if (i == 0) {
					item_length -= scaleSpacing() / 2;
				}
				else if (i == count() - 1) {
					item_length -= scaleSpacing() / 2;
					item_left += scaleSpacing() / 2;
				}
				else {
					item_left += scaleSpacing() / 2;
					item_length -= scaleSpacing();
				}

				it->setGeometry(QRectF(item_left, top, item_length, height));
				it->setBoundingRectNoCorners(QRectF(0, 0, item_length, height));
				it->layoutScale();

				left += length;
			}
			else {
				d_data->scales.removeAt(i);
				--i;
			}
		}
	}

	for (int i = 0; i < count(); ++i)
		at(i)->updateItems();
}

void VipMultiAxisBase::getBorderDistHint(double& start, double& end) const
{
	if (!d_data->scales.size()) {
		start = end = 0;
		return;
	}

	double bstart = 0, bend = 0;
	double estart = 0, eend = 0;
	d_data->scales.first()->getBorderDistHint(bstart, bend);
	d_data->scales.last()->getBorderDistHint(estart, eend);
	start = bstart;
	end = eend;
}

void VipMultiAxisBase::draw(QPainter* painter, QWidget* widget)
{
	painter->save();
	painter->setRenderHints(this->renderHints());
	VipBorderItem::draw(painter, widget);

	QRectF brect = this->boundingRectNoCorners();

	if (brect == QRectF())
		brect = this->geometry().translated(-pos());

	if (this->orientation() == Qt::Horizontal) {
		brect.setLeft(brect.left() + startBorderDist());
		brect.setWidth(brect.width() - endBorderDist());
	}
	else {
		brect.setTop(brect.top() + startBorderDist());
		brect.setHeight(brect.height() - endBorderDist());
	}

	if ((!title().isEmpty() && isDrawTitleEnabled()))
		drawTitle(painter, this->alignment(), brect);

	painter->restore();
}

void VipMultiAxisBase::drawTitle(QPainter* painter, Alignment align, const QRectF& rect) const
{
	QRectF r = rect;
	double angle;
	int flags = title().alignment() & ~(Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter);

	int sign = 1;
	QSizeF tsize(0, 0);

	double titleOffset;
	if (orientation() == Qt::Vertical)
		titleOffset = geometry().width() - titleExtent();
	else
		titleOffset = geometry().height() - titleExtent();

	switch (align) {
		case Left:
			angle = -90.0;
			flags |= Qt::AlignTop;
			r.setRect(r.left(), r.bottom(), r.height(), r.width() - (titleOffset + tsize.width()) * sign);
			break;

		case Right:
			angle = -90.0;
			flags |= Qt::AlignTop;
			r.setRect(r.left() + (titleOffset + tsize.width()) * sign, r.bottom(), r.height(), r.width() - (titleOffset + tsize.width()) * sign);
			break;

		case Bottom:
			angle = 0.0;
			flags |= Qt::AlignBottom;
			r.setTop(r.top() + (titleOffset + tsize.height()) * sign);
			break;

		case Top:
		default:
			angle = 0.0;
			flags |= Qt::AlignBottom;
			r.setBottom(r.bottom() - (titleOffset + tsize.height()) * sign);
			break;
	}

	if (d_data->layoutFlags & TitleInverted) {
		if (align == Left || align == Right) {
			angle = -angle;
			r.setRect(r.x() + r.height(), r.y() - r.width(), r.width(), r.height());
		}
	}

	painter->save();

	painter->translate(r.x(), r.y());
	if (angle != 0.0)
		painter->rotate(angle);

	// draw title
	VipText t = title();
	if (!isDrawTitleEnabled())
		t.setText(QString());

	// make sure text fits to rect width
	QSizeF ts = t.textSize();
	// double diffx = 0;
	if (ts.width() > r.width()) {
		// double left = r.left();
		QPointF center = r.center();
		r.setWidth(ts.width());
		r.moveCenter(center);
		// diffx = r.left() - left;
	}

	if (!title().isEmpty()) {
		t.setAlignment(Qt::Alignment(flags));
		t.draw(painter,
		       QRectF( // diffx
			 0.0,
			 0.0,
			 r.width(),
			 r.height()));
	}

	painter->restore();
}

VipArchive& operator<<(VipArchive& arch, const VipAxisBase* value)
{
	arch.content("isMapScaleToScene", value->isMapScaleToScene());
	arch.content("isTitleInverted", value->isTitleInverted());
	arch.content("titleInside", value->titleInside());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipAxisBase* value)
{
	value->setMapScaleToScene(arch.read("isMapScaleToScene").value<bool>());
	value->setTitleInverted(arch.read("isTitleInverted").value<bool>());
	arch.save();
	// since 2.2.18
	bool titleInside;
	if (arch.content("titleInside", titleInside))
		value->setTitleInside(titleInside);
	else
		arch.restore();
	return arch;
}

static bool register_types()
{
	qRegisterMetaType<VipAxisBase*>();
	vipRegisterArchiveStreamOperators<VipAxisBase*>();

	return true;
}
static bool _register_types = register_types();