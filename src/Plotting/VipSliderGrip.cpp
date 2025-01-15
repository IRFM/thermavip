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

#include "VipSliderGrip.h"
#include "VipAxisColorMap.h"
#include "VipBorderItem.h"
#include "VipPainter.h"
#include "VipPolarAxis.h"
#include "slider.png.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <qlocale.h>

static int registerSliderKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["grip-always-inside-scale"] = VipParserPtr(new BoolParser());
		keywords["single-step-enabled"] = VipParserPtr(new BoolParser());
		keywords["single-step"] = VipParserPtr(new DoubleParser());
		keywords["single-step-reference"] = VipParserPtr(new DoubleParser());
		keywords["tooltip"] = VipParserPtr(new TextParser());
		keywords["tooltip-distance"] = VipParserPtr(new DoubleParser());
		keywords["display-tooltip-value"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["handle-distance"] = VipParserPtr(new DoubleParser());
		keywords["image"] = VipParserPtr(new TextParser());

		vipSetKeyWordsForClass(&VipSliderGrip::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerSliderKeyWords = registerSliderKeyWords();

class VipSliderGrip::PrivateData
{
public:
	PrivateData()
	  : axis(nullptr)
	  , value(0)
	  , gripAlwaysInsideScale(true)
	  , toolTipDistance(0)
	  , toolTipSide(Qt::AlignRight | Qt::AlignVCenter)
	  , textAlignment(Qt::AlignTop | Qt::AlignHCenter)
	  , textPosition(Vip::Outside)
	  , textDistance(5)
	{
	}

	VipAbstractScale* axis;
	double value;
	QPointF selection;
	bool gripAlwaysInsideScale;
	bool singleStepEnabled;
	double singleStep;
	double singleStepReference;
	double handleDistance;
	double toolTipDistance;
	Qt::Alignment toolTipSide;
	QImage image;
	QImage rotatedImage;
	QString toolTipText;

	Qt::Alignment textAlignment;
	Vip::RegionPositions textPosition;
	QTransform textTransform;
	QPointF textTransformReference;
	double textDistance;
	VipText text;
	QSharedPointer<VipTextStyle> textStyle;
};

VipSliderGrip::VipSliderGrip(VipAbstractScale* parent)
  : QGraphicsObject(parent)
  , VipPaintItem(this)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->singleStepEnabled = false;
	d_data->singleStep = 1;
	d_data->singleStepReference = Vip::InvalidValue;
	d_data->handleDistance = 5;
	// d_data->image.loadFromData(reinterpret_cast<const uchar*>(slider_png), 477, "PNG");
	this->setFlag(QGraphicsItem::ItemIsMovable, true);
	this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

	setScale(parent);

	qRegisterMetaType<VipSliderGrip*>();
}

VipSliderGrip::~VipSliderGrip()
{
}

void VipSliderGrip::setScale(VipAbstractScale* s)
{
	if (d_data->axis)
		disconnect(d_data->axis, SIGNAL(scaleDivChanged(bool)), this, SLOT(updatePosition()));

	this->setParentItem(s);
	d_data->axis = s;

	if (d_data->axis)
		connect(d_data->axis, SIGNAL(scaleDivChanged(bool)), this, SLOT(updatePosition()));
}

bool VipSliderGrip::sceneEventFilter(QGraphicsItem* // watched
				     ,
				     QEvent* event)
{
	if (event->type() == QEvent::Paint) {
		setValue(value());
	}

	return false;
}

void VipSliderGrip::setHandleDistance(double vipDistance)
{
	d_data->handleDistance = vipDistance;
	update();
}

double VipSliderGrip::handleDistance() const
{
	return d_data->handleDistance;
}

void VipSliderGrip::setSingleStepEnabled(bool enable)
{
	d_data->singleStepEnabled = enable;
	if (enable)
		setValue(value());
}

bool VipSliderGrip::singleStepEnabled() const
{
	return d_data->singleStepEnabled;
}

void VipSliderGrip::setSingleStep(double singleStep, double reference)
{
	d_data->singleStep = singleStep;
	d_data->singleStepReference = reference;
	setSingleStepEnabled(true);
}

double VipSliderGrip::singleStep() const
{
	return d_data->singleStep;
}

double VipSliderGrip::singleStepReference() const
{
	return d_data->singleStepReference;
}

void VipSliderGrip::setDisplayToolTipValue(Qt::Alignment side)
{
	d_data->toolTipSide = side;
}

Qt::Alignment VipSliderGrip::displayToolTipValue() const
{
	return d_data->toolTipSide;
}

QString VipSliderGrip::formatText(const QString& str, double value) const
{
	QRegExp _reg("#(\\w+)");
	const QList<QByteArray> props = dynamicPropertyNames();
	QString res = str;

	bool has_property = false;

	int offset = 0;
	int index = _reg.indexIn(res, offset);
	while (index >= 0) {
		QString full = res.mid(index, _reg.matchedLength());

		if (full == "#pcount")
			res.replace(index, _reg.matchedLength(), QString::number(dynamicPropertyNames().size()));
		else if (full.startsWith("#pname")) {
			has_property = true;
			index++;
		}
		else if (full.startsWith("#pvalue")) {
			has_property = true;
			index++;
		}
		else if (full.startsWith("#value")) {
			VipText t = res;
			t.replace("#value", value);
			res = t.text();
			// res.replace(index, _reg.matchedLength(), QString::number(value));
		}
		else if (full.startsWith("#p")) {
			full.remove("#p");
			int i = props.indexOf(full.toLatin1());
			if (i >= 0)
				res.replace(index, _reg.matchedLength(), property(props[i].data()).toString());
			else
				res.replace(index, _reg.matchedLength(), QString());
		}
		else
			++index;

		index = _reg.indexIn(res, index);
	}

	VipText text = res;
	text.repeatBlock();

	if (has_property) {
		for (int i = 0; i < props.size(); ++i) {
			text.replace("#pname" + QString::number(i), QString(props[i]));
			text.replace("#pvalue" + QString::number(i), property(props[i].data()).toString());
		}
	}

	return text.text();
}
double VipSliderGrip::closestValue(double v)
{
	if (d_data->singleStepEnabled) {
		const VipInterval inter = d_data->axis->scaleDiv().bounds().normalized();
		const double reference = d_data->singleStepReference == Vip::InvalidValue ? inter.minValue() : d_data->singleStepReference;
		const qint64 rounded = qRound((v - reference) / d_data->singleStep);

		double value = (reference + rounded * d_data->singleStep);
		if (value < inter.minValue())
			value = (reference + (rounded + 1) * d_data->singleStep);
		else if (value > inter.maxValue())
			value = (reference + (rounded - 1) * d_data->singleStep);
		return value;
	}
	else
		return v;
}

QRectF VipSliderGrip::boundingRect() const
{
	if (d_data->image.isNull()) {
		return QRectF();
	}
	else {
		QRectF rect(0, 0, d_data->image.width(), d_data->image.height());
		QTransform tr;
		tr.rotate(-d_data->axis->constScaleDraw()->angle(value()) - 90);
		rect = tr.map(rect).boundingRect();
		rect.moveTopLeft(QPointF(0, 0));
		rect.translate(-rect.width() / 2., -rect.height() / 2.);
		return rect;
	}
}

void VipSliderGrip::updatePosition()
{
	QPointF pt = d_data->axis->constScaleDraw()->position(d_data->value, handleDistance(), Vip::Absolute);
	if (pt != pos() && qAbs(pt.x()) < 10000 && qAbs(pt.y()) < 10000)
		this->setPos(pt);
}

void VipSliderGrip::setValue(double val)
{
	double previous = d_data->value;

	// if(d_data->singleStepEnabled)
	d_data->value = closestValue(val);
	// else
	//  d_data->value = val;

	// bound value
	if (d_data->gripAlwaysInsideScale) {
		VipInterval interval = d_data->axis->scaleDiv().bounds().normalized();
		d_data->value = qMax((double)interval.minValue(), d_data->value);
		d_data->value = qMin((double)interval.maxValue(), d_data->value);
	}

	updatePosition();

	if (previous != d_data->value) {
		Q_EMIT valueChanged(d_data->value);
	}
}

void VipSliderGrip::setGripAlwaysInsideScale(bool inside)
{
	d_data->gripAlwaysInsideScale = inside;
}

bool VipSliderGrip::gripAlwaysInsideScale() const
{
	return d_data->gripAlwaysInsideScale;
}

void VipSliderGrip::setImage(const QImage& handle_image)
{
	d_data->image = handle_image;
	this->update();
}

QImage VipSliderGrip::image() const
{
	return d_data->image;
}

double VipSliderGrip::value() const
{
	return d_data->value;
}

VipAbstractScale* VipSliderGrip::scale()
{
	return d_data->axis;
}

const VipAbstractScale* VipSliderGrip::scale() const
{
	return d_data->axis;
}

void VipSliderGrip::drawHandle(QPainter* painter) const
{
	if (d_data->image.isNull()) {
		VipSliderGrip* _this = const_cast<VipSliderGrip*>(this);
		_this->d_data->image.loadFromData(reinterpret_cast<const uchar*>(slider_png), 477, "PNG");
	}

	QTransform tr;
	tr.rotate(-d_data->axis->constScaleDraw()->angle(value()) - 90);
	d_data->rotatedImage = d_data->image.transformed(tr, Qt::SmoothTransformation);

	if (!d_data->rotatedImage.isNull()) {
		QPointF p = pos();
		// do not draw when the position does not have any sense
		if (qAbs(p.x()) < 10000 && qAbs(p.y()) < 10000) {

			QRectF rect(0, 0, d_data->rotatedImage.width(), d_data->rotatedImage.height());
			rect.translate(-d_data->rotatedImage.width() / 2.0, -d_data->rotatedImage.height() / 2.0);
			VipPainter::drawImage(painter, rect, d_data->rotatedImage, QRectF(QPointF(0, 0), rect.size()));
		}
		//
	}

	if (!d_data->text.isEmpty()) {

		QRectF geom(0, 0, d_data->rotatedImage.width(), d_data->rotatedImage.height());
		geom.translate(-d_data->rotatedImage.width() / 2.0, -d_data->rotatedImage.height() / 2.0);

		VipText t = d_data->text;
		t.setText(VipText::replace(t.text(), "#value", value()));

		// draw text
		VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), geom);
	}
}

void VipSliderGrip::setToolTipText(const QString& text)
{
	d_data->toolTipText = text;
}

const QString& VipSliderGrip::toolTipText() const
{
	return d_data->toolTipText;
}

void VipSliderGrip::setToolTipDistance(double dist)
{
	d_data->toolTipDistance = dist;
}

double VipSliderGrip::toolTipDistance() const
{
	return d_data->toolTipDistance;
}

void VipSliderGrip::setTextAlignment(Qt::Alignment align)
{
	d_data->textAlignment = align;
	update();
}

Qt::Alignment VipSliderGrip::textAlignment() const
{
	return d_data->textAlignment;
}

void VipSliderGrip::setTextPosition(Vip::RegionPositions pos)
{
	d_data->textPosition = pos;
	update();
}

Vip::RegionPositions VipSliderGrip::textPosition() const
{
	return d_data->textPosition;
}

void VipSliderGrip::setTextTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform = tr;
	d_data->textTransformReference = ref;
	update();
}
const QTransform& VipSliderGrip::textTransform() const
{
	return d_data->textTransform;
}
const QPointF& VipSliderGrip::textTransformReference() const
{
	return d_data->textTransformReference;
}

void VipSliderGrip::setTextDistance(double vipDistance)
{
	d_data->textDistance = vipDistance;
	update();
}

double VipSliderGrip::textDistance() const
{
	return d_data->textDistance;
}

void VipSliderGrip::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	update();
}

const VipText& VipSliderGrip::text() const
{
	return d_data->text;
}

VipText& VipSliderGrip::text()
{
	return d_data->text;
}

void VipSliderGrip::paint(QPainter* painter,
			  const QStyleOptionGraphicsItem* // option
			  ,
			  QWidget* // widget
)
{
	if (d_data->selection == QPointF())
		setValue(value());

	if (!paintingEnabled())
		return;

	this->applyStyleSheetIfDirty();
	//auto r = this->renderHints();
	// TEST: comment setRenderHints that crash sometimes (?)
	// painter->setRenderHints(r);
	auto c = this->compositionMode();
	painter->setCompositionMode(c);

	this->drawHandle(painter);
}

void VipSliderGrip::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	// Reapply style sheet in case of 'hover' selector
	this->setProperty("_vip_hover", true);
	this->markStyleSheetDirty();
	QGraphicsObject::hoverEnterEvent(event);
}

void VipSliderGrip::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	// Reapply style sheet in case of 'hover' selector
	this->setProperty("_vip_hover", false);
	this->markStyleSheetDirty();
	QGraphicsObject::hoverLeaveEvent(event);
}

QVariant VipSliderGrip::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == QGraphicsItem::ItemSelectedHasChanged)
		this->markStyleSheetDirty();
	else if (change == QGraphicsItem::ItemChildAddedChange)
		this->dispatchStyleSheetToChildren();

	return QGraphicsObject::itemChange(change, value);
}

bool VipSliderGrip::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	/// -	'grip-always-inside-scale' : equivalent to setGripAlwaysInsideScale()
	/// -	'single-step-enabled: equivalent to setSingleStepEnabled()
	/// -	'single-step': first parameter of setSingleStep()
	/// -	'single-step-reference': second parameter of setSingleStep()
	/// -	'tooltip': tool tip text, equivalent to setToolTipText()
	/// -	'tooltip-distance': equivalent to setToolTipDistance()
	/// -	'display-tooltip-value': equivalent to setDisplayToolTipValue(), combination of 'left|top|right|bottom|center|vcenter|hcenter'
	/// -	'handle-distance': equivalent to setHandleDistance()
	/// -	'image': path to a valid handle image

	if (value.userType() == 0)
		return false;

	if (strcmp(name, "grip-always-inside-scale") == 0) {
		setGripAlwaysInsideScale(value.toBool());
		return true;
	}
	if (strcmp(name, "single-step-enabled") == 0) {
		setSingleStepEnabled(value.toBool());
		return true;
	}
	if (strcmp(name, "single-step") == 0) {
		setSingleStep(value.toDouble(), singleStepReference());
		return true;
	}
	if (strcmp(name, "single-step-reference") == 0) {
		setSingleStep(singleStep(), value.toDouble());
		return true;
	}
	if (strcmp(name, "tooltip") == 0) {
		setToolTipText(value.value<QString>());
		return true;
	}
	if (strcmp(name, "tooltip-distance") == 0) {
		setToolTipDistance(value.toDouble());
		return true;
	}
	if (strcmp(name, "display-tooltip-value") == 0) {
		setDisplayToolTipValue((Qt::Alignment)value.toInt());
		return true;
	}
	if (strcmp(name, "handle-distance") == 0) {
		setHandleDistance(value.toDouble());
		return true;
	}
	if (strcmp(name, "image") == 0) {
		setImage(QImage(value.value<QString>()));
		return true;
	}
	return VipPaintItem::setItemProperty(name, value, index);
}

bool VipSliderGrip::hasState(const QByteArray& state, bool enable) const
{
	if (state == "left") {
		if (const VipBorderItem* it = qobject_cast<const VipBorderItem*>(scale()))
			return (it->alignment() == VipBorderItem::Left) == enable;
		return false;
	}
	if (state == "right") {
		if (const VipBorderItem* it = qobject_cast<const VipBorderItem*>(scale()))
			return (it->alignment() == VipBorderItem::Right) == enable;
		return false;
	}
	if (state == "top") {
		if (const VipBorderItem* it = qobject_cast<const VipBorderItem*>(scale()))
			return (it->alignment() == VipBorderItem::Top) == enable;
		return false;
	}
	if (state == "bottom") {
		if (const VipBorderItem* it = qobject_cast<const VipBorderItem*>(scale()))
			return (it->alignment() == VipBorderItem::Bottom) == enable;
		return false;
	}
	if (state == "radial") {
		if (/* const VipRadialAxis* it =*/ qobject_cast<const VipRadialAxis*>(scale()))
			return enable;
		return false;
	}
	if (state == "polar") {
		if (/* const VipPolarAxis* it = */qobject_cast<const VipPolarAxis*>(scale()))
			return enable;
		return false;
	}
	return VipPaintItem::hasState(state, enable);
}

void VipSliderGrip::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	if (d_data->selection != QPointF()) {
		double value = d_data->value;
		d_data->value = d_data->axis->constScaleDraw()->value(d_data->axis->mapFromScene(event->scenePos()));

		// vipBounds value
		if (d_data->gripAlwaysInsideScale) {
			VipInterval interval = d_data->axis->scaleDiv().bounds().normalized();
			d_data->value = qMax((double)interval.minValue(), d_data->value);
			d_data->value = qMin((double)interval.maxValue(), d_data->value);
		}

		// if(d_data->singleStepEnabled)
		d_data->value = closestValue(d_data->value);

		updatePosition();
		if (value != d_data->value) {
			Q_EMIT valueChanged(d_data->value);

			if (scene() && displayToolTipValue() != 0 && scale() && !d_data->toolTipText.isEmpty()) {
				QPointF p = sceneToScreenCoordinates(scene(), mapToScene(boundingRect().center()));
				QRectF r = this->mapToScene(boundingRect()).boundingRect();
				r = QRectF(sceneToScreenCoordinates(scene(), r.topLeft()), sceneToScreenCoordinates(scene(), r.bottomRight()));
				r.moveCenter(p);
				QString val = scale()->constScaleDraw()->label(d_data->value, VipScaleDiv::MajorTick).text();

				double v = QLocale().toDouble(val); // val.toDouble();
				VipText text = formatText(d_data->toolTipText, v);
				QSizeF s = text.textSize();

				if (displayToolTipValue() & Qt::AlignTop)
					p.setY(r.top() - s.height() - d_data->toolTipDistance);
				else if (displayToolTipValue() & Qt::AlignBottom)
					p.setY(r.bottom() + d_data->toolTipDistance);
				else
					p.setY(p.y() - s.height() / 2);

				if (displayToolTipValue() & Qt::AlignLeft)
					p.setX(r.left() - s.width() - d_data->toolTipDistance);
				else if (displayToolTipValue() & Qt::AlignRight)
					p.setX(r.right() + d_data->toolTipDistance);
				else
					p.setX(p.x() - s.width() / 2);

				p -= QPointF(1, 17);

				QToolTip::showText(p.toPoint(), text.text());
			}
		}
	}

	/*if (displayValue())
	{
		QPoint p = sceneToScreenCoordinates(scene(), event->scenePos());
		QToolTip::showText ( p, QString::number(d_data->value) );
	}*/

	Q_EMIT mouseButtonMove(this, static_cast<VipPlotItem::MouseButton>(event->button()));
}
void VipSliderGrip::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	d_data->selection = this->mapToItem(d_data->axis, event->pos());
	Q_EMIT mouseButtonPress(this, static_cast<VipPlotItem::MouseButton>(event->button()));
}

void VipSliderGrip::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	d_data->selection = QPointF();
	Q_EMIT mouseButtonRelease(this, static_cast<VipPlotItem::MouseButton>(event->button()));
}

void VipSliderGrip::keyPressEvent(QKeyEvent* event)
{
	if (this->singleStepEnabled()) {

		if (VipBorderItem* it = qobject_cast<VipBorderItem*>(this->scale())) {
			if (it->orientation() == Qt::Horizontal) {
				VipInterval inter = it->scaleDiv().bounds();
				if (event->key() == Qt::Key_Right) {
					double val = this->value();
					if (inter.width() > 0) {
						val += this->singleStep();
						if (val > inter.maxValue())
							val = inter.maxValue();
					}
					else {
						val -= this->singleStep();
						if (val < inter.minValue())
							val = inter.minValue();
					}
					setValue(val);
				}
				else if (event->key() == Qt::Key_Left) {
					double val = this->value();
					if (inter.width() > 0) {
						val -= this->singleStep();
						if (val < inter.minValue())
							val = inter.minValue();
					}
					else {
						val += this->singleStep();
						if (val > inter.maxValue())
							val = inter.maxValue();
					}
					setValue(val);
				}
			}
			else {
				VipInterval inter = it->scaleDiv().bounds();
				if (event->key() == Qt::Key_Up) {
					double val = this->value();
					if (inter.width() > 0) {
						val += this->singleStep();
						if (val > inter.maxValue())
							val = inter.maxValue();
					}
					else {
						val -= this->singleStep();
						if (val < inter.minValue())
							val = inter.minValue();
					}
					setValue(val);
				}
				else if (event->key() == Qt::Key_Down) {
					double val = this->value();
					if (inter.width() > 0) {
						val -= this->singleStep();
						if (val < inter.minValue())
							val = inter.minValue();
					}
					else {
						val += this->singleStep();
						if (val > inter.maxValue())
							val = inter.maxValue();
					}
					setValue(val);
				}
			}
		}
	}
}

VipColorMapGrip::VipColorMapGrip(VipAxisColorMap* parent)
  : VipSliderGrip(parent)
{
	this->setValue(0);
	vipRegisterMetaObject(&VipColorMapGrip::staticMetaObject);
}

VipColorMapGrip::~VipColorMapGrip() {}

VipAxisColorMap* VipColorMapGrip::colorMapAxis()
{
	return static_cast<VipAxisColorMap*>(this->scale());
}

const VipAxisColorMap* VipColorMapGrip::colorMapAxis() const
{
	return static_cast<const VipAxisColorMap*>(this->scale());
}

double VipColorMapGrip::handleDistance() const
{
	if (colorMapAxis()->orientation() == Qt::Vertical) {
		return qAbs(colorMapAxis()->colorBarRect().center().x() - colorMapAxis()->constScaleDraw()->pos().x()) + VipSliderGrip::handleDistance();
	}
	else {
		return qAbs(colorMapAxis()->colorBarRect().center().y() - colorMapAxis()->constScaleDraw()->pos().y()) + VipSliderGrip::handleDistance();
	}
}
