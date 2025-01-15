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

#include "VipPlotScatter.h"
#include "VipBorderItem.h"
#include "VipPainter.h"
#include "VipShapeDevice.h"

static int registerScatterKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		QMap<QByteArray, int> unit;
		unit["itemUnit"] = VipPlotScatter::ItemUnit;
		unit["axisUnit"] = VipPlotScatter::AxisUnit;

		keywords["size-unit"] = VipParserPtr(new EnumParser(unit));
		keywords["use-value-as-size"] = VipParserPtr(new BoolParser());
		keywords["text-alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["text-position"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::regionPositionEnum()));
		keywords["text-distance"] = VipParserPtr(new DoubleParser());
		keywords["symbol"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::symbolEnum()));
		keywords["symbol-size"] = VipParserPtr(new DoubleParser());

		vipSetKeyWordsForClass(&VipPlotScatter::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerScatterKeyWords = registerScatterKeyWords();

class VipPlotScatter::PrivateData
{
public:
	PrivateData()
	  : unit(ItemUnit)
	  , useValueAsSize(false)
	  , textAlignment(Qt::AlignTop | Qt::AlignHCenter)
	  , textPosition(Vip::XInside)
	  , textDistance(5)
	{
		symbol.setStyle(VipSymbol::Rect);
		symbol.setSize(QSizeF(10, 10));
		symbol.setCachePolicy(VipSymbol::NoCache);
	}

	QList<VipInterval> bounding;
	QList<VipInterval> boundingInterval;

	VipSymbol symbol;
	SizeUnit unit;
	bool useValueAsSize;

	VipInterval dataValidInterval;
	VipInterval dataInterval;

	Qt::Alignment textAlignment;
	Vip::RegionPositions textPosition;
	QTransform textTransform;
	QPointF textTransformReference;
	double textDistance;
	VipText text;
	QSharedPointer<VipTextStyle> textStyle;
};

VipPlotScatter::VipPlotScatter(const VipText& title)
  : VipPlotItemDataType(title)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	this->setData(QVariant::fromValue(VipScatterPointVector()));
	this->setMajorColor(QColor(Qt::blue));
}

VipPlotScatter::~VipPlotScatter()
{
}

void VipPlotScatter::setSizeUnit(SizeUnit unit)
{
	if (d_data->unit != unit) {

		d_data->unit = unit;
		emitItemChanged();
	}
}
VipPlotScatter::SizeUnit VipPlotScatter::sizeUnit() const
{
	return d_data->unit;
}

void VipPlotScatter::setUseValueAsSize(bool enable)
{
	if (d_data->useValueAsSize != enable) {

		d_data->useValueAsSize = enable;
		emitItemChanged();
	}
}
bool VipPlotScatter::useValueAsSize() const
{
	return d_data->useValueAsSize;
}

VipSymbol& VipPlotScatter::symbol()
{
	return d_data->symbol;
}
const VipSymbol& VipPlotScatter::symbol() const
{
	return d_data->symbol;
}
void VipPlotScatter::setSymbol(const VipSymbol& s)
{
	d_data->symbol = s;
	emitItemChanged();
}

void VipPlotScatter::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->text.setTextStyle(st);
	emitItemChanged();
}

void VipPlotScatter::setTextAlignment(Qt::Alignment align)
{
	d_data->textAlignment = align;
	emitItemChanged();
}

Qt::Alignment VipPlotScatter::textAlignment() const
{
	return d_data->textAlignment;
}

void VipPlotScatter::setTextPosition(Vip::RegionPositions pos)
{
	d_data->textPosition = pos;
	emitItemChanged();
}

Vip::RegionPositions VipPlotScatter::textPosition() const
{
	return d_data->textPosition;
}

void VipPlotScatter::setTextTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform = tr;
	d_data->textTransformReference = ref;
	emitItemChanged();
}
const QTransform& VipPlotScatter::textTransform() const
{
	return d_data->textTransform;
}
const QPointF& VipPlotScatter::textTransformReference() const
{
	return d_data->textTransformReference;
}

void VipPlotScatter::setTextDistance(double vipDistance)
{
	d_data->textDistance = vipDistance;
	emitItemChanged();
}

double VipPlotScatter::textDistance() const
{
	return d_data->textDistance;
}

void VipPlotScatter::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

const VipText& VipPlotScatter::text() const
{
	return d_data->text;
}

VipText& VipPlotScatter::text()
{
	return d_data->text;
}

VipInterval VipPlotScatter::computeInterval(const VipScatterPointVector& vec, const VipInterval& interval) const
{
	VipInterval res;
	for (const VipScatterPoint& p : vec) {
		if (interval.contains(p.value)) {
			if (!res.isValid())
				res = VipInterval(p.value, p.value);
			else {
				res.setMinValue(std::min(res.minValue(), (vip_double)p.value));
				res.setMaxValue(std::max(res.maxValue(), (vip_double)p.value));
			}
		}
	}
	return res;
}

QList<VipInterval> VipPlotScatter::dataBoundingIntervals(const VipScatterPointVector& data) const
{
	if (!data.size())
		return QList<VipInterval>();

	const VipScatterPoint first = data[0];
	vip_double x_min = first.position.x();
	vip_double x_max = first.position.x();
	vip_double y_min = first.position.y();
	vip_double y_max = first.position.y();

	for (int i = 1; i < data.size(); ++i) {
		const VipScatterPoint p = data[i];
		x_min = std::min(p.position.x(), x_min);
		x_max = std::max(p.position.x(), x_max);

		y_max = std::max(y_max, p.position.y());
		y_min = std::min(y_min, p.position.y());
	}
	return QList<VipInterval>() << VipInterval(x_min, x_max) << VipInterval(y_min, y_max);
}

void VipPlotScatter::setData(const QVariant& data)
{
	VipPlotItemDataType::setData(data);
	Locker locker(dataLock());
	const VipScatterPointVector vec = data.value<VipScatterPointVector>();
	d_data->bounding = dataBoundingIntervals(vec);
	d_data->dataValidInterval = Vip::InfinitInterval;
	d_data->dataInterval = computeInterval(vec, Vip::InfinitInterval);
}

VipInterval VipPlotScatter::plotInterval(const VipInterval& interval) const
{
	if (d_data->dataInterval.isValid() && d_data->dataValidInterval == interval)
		return d_data->dataInterval;
	Locker lock(dataLock());
	const_cast<VipPlotScatter*>(this)->d_data->dataValidInterval = interval;
	return const_cast<VipPlotScatter*>(this)->d_data->dataInterval = computeInterval(rawData(), interval);
}

QList<VipInterval> VipPlotScatter::plotBoundingIntervals() const
{
	Locker locker(dataLock());
	QList<VipInterval> res = d_data->bounding;
	if (res.isEmpty()) {
		res = const_cast<PrivateData*>(d_data.get())->bounding = dataBoundingIntervals(rawData());
	}
	res.detach();
	return res;
}

QString VipPlotScatter::formatText(const QString& text, const QPointF& pos) const
{
	const VipScatterPointVector vec = rawData();
	int index = findClosestPos(vec, pos, 0, nullptr);
	if (index == -1)
		return VipPlotItem::formatText(text, pos);

	return VipText::replace(text, "#value", vec[index].value);
}

bool VipPlotScatter::areaOfInterest(const QPointF& pos, int, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const
{
	const VipScatterPointVector vec = rawData();
	QRectF rect;
	int index = findClosestPos(vec, pos, maxDistance, &rect);
	if (index == -1)
		return false;

	out_pos.push_back(sceneMap()->transform(vec[index].position));

	// retrieve symbol path
	VipSymbol s = symbol();
	s.setSize(rect.size());
	s.setCachePolicy(VipSymbol::NoCache);
	VipShapeDevice dev;
	QPainter p(&dev);
	dev.setDrawPrimitives(VipShapeDevice::All);
	s.drawSymbol(&p, rect.center());
	p.end();

	style.computePath(dev.shape());
	legend = 0;
	return true;
}

void VipPlotScatter::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	if (m->axes().size() != 2)
		return;

	const VipBorderItem* x = qobject_cast<const VipBorderItem*>(m->axes().first());
	const VipBorderItem* y = qobject_cast<const VipBorderItem*>(m->axes().last());
	const VipScatterPointVector vec = rawData();
	const bool has_colormap = this->colorMap();

	// Find symbol size
	QSizeF s = symbol().size();
	if (!useValueAsSize()) {
		if (sizeUnit() == AxisUnit) {
			if (x && y) {
				s.setWidth(x->axisRangeToItemUnit(s.width()));
				s.setHeight(y->axisRangeToItemUnit(s.height()));
			}
		}
	}

	VipSymbol sym = symbol();
	QColor default_color = symbol().brush().color();

	for (int i = 0; i < vec.size(); ++i) {
		VipPoint p = m->transform(vec[i].position);
		QSizeF size = s;
		if (useValueAsSize()) {
			if (sizeUnit() == AxisUnit && x && y) {
				size.setWidth(x->axisRangeToItemUnit(vec[i].value));
				size.setHeight(y->axisRangeToItemUnit(vec[i].value));
			}
			else {
				size.setWidth(vec[i].value);
				size.setHeight(vec[i].value);
			}
		}
		QRectF rect(p.toPointF() - QPointF(size.width() / 2, size.height() / 2), size);
		sym.setSize(size);
		if (has_colormap)
			sym.setBrushColor(color(vec[i].value, default_color));
		sym.drawSymbol(painter, rect.center());

		if (!d_data->text.isEmpty()) {
			VipText t = d_data->text;
			t.replace("#value", vec[i].value);
			VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), rect);
		}
	}
}
QRectF VipPlotScatter::drawLegend(QPainter* p, const QRectF& r, int) const
{
	QRectF rect = vipInnerSquare(r);
	VipSymbol s = symbol();
	QSizeF size = s.size();
	if (rect.width() < size.width())
		size.setWidth(rect.width());
	if (rect.height() < size.height())
		size.setHeight(rect.height());
	s.setSize(size);
	s.drawSymbol(p, rect.center());
	return rect;
}

bool VipPlotScatter::hasState(const QByteArray& state, bool enable) const
{
	if (state == "itemUnit")
		return (sizeUnit() == ItemUnit) == enable;
	if (state == "axisUnit")
		return (sizeUnit() == AxisUnit) == enable;
	return VipPlotItem::hasState(state, enable);
}

bool VipPlotScatter::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "text-alignment") == 0) {
		setTextAlignment((Qt::Alignment)value.toInt());
		return true;
	}
	if (strcmp(name, "text-position") == 0) {
		setTextPosition((Vip::RegionPositions)value.toInt());
		return true;
	}
	if (strcmp(name, "text-distance") == 0) {
		setTextDistance(value.toDouble());
		return true;
	}
	if (strcmp(name, "size-unit") == 0) {
		setSizeUnit((SizeUnit)value.toInt());
		return true;
	}
	if (strcmp(name, "symbol") == 0) {
		symbol().setStyle((VipSymbol::Style)value.toInt());
		return true;
	}
	if (strcmp(name, "symbol-size") == 0) {
		double w = value.toDouble();
		symbol().setSize(QSizeF(w, w));
		return true;
	}
	if (strcmp(name, "use-value-as-size") == 0) {
		setUseValueAsSize(value.toBool());
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

int VipPlotScatter::findClosestPos(const VipScatterPointVector& vec, const QPointF& pos, double maxDistance, QRectF* out) const
{
	VipCoordinateSystemPtr m = sceneMap();
	if (m->axes().size() != 2)
		return -1;

	const VipBorderItem* x = qobject_cast<const VipBorderItem*>(m->axes().first());
	const VipBorderItem* y = qobject_cast<const VipBorderItem*>(m->axes().last());

	// Find symbol size
	QSizeF s = symbol().size();
	if (!useValueAsSize()) {
		if (sizeUnit() == AxisUnit) {
			if (x && y) {
				s.setWidth(x->axisRangeToItemUnit(s.width()));
				s.setHeight(y->axisRangeToItemUnit(s.height()));
			}
		}
	}

	for (int i = 0; i < vec.size(); ++i) {
		VipPoint p = m->transform(vec[i].position);
		QSizeF size = s;
		if (useValueAsSize()) {
			if (sizeUnit() == AxisUnit && x && y) {

				size.setWidth(x->axisRangeToItemUnit(vec[i].value));
				size.setHeight(y->axisRangeToItemUnit(vec[i].value));
			}
			else {
				size.setWidth((vec[i].value));
				size.setHeight((vec[i].value));
			}
		}
		QRectF rect(p.toPointF() - QPointF(size.width() / 2, size.height() / 2), size);
		if (rect.adjusted(-maxDistance, -maxDistance, maxDistance, maxDistance).contains(pos)) {
			if (out)
				*out = rect;
			return i;
		}
	}
	return -1;
}

QDataStream& operator<<(QDataStream& str, const VipScatterPoint& p)
{
	return str << p.position << p.value;
}
QDataStream& operator>>(QDataStream& str, VipScatterPoint& p)
{
	return str >> p.position >> p.value;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotScatter* value)
{
	arch.content("sizeUnit", (int)value->sizeUnit());
	arch.content("useValueAsSize", value->useValueAsSize());
	arch.content("symbol", value->symbol());

	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotScatter* value)
{
	value->setSizeUnit((VipPlotScatter::SizeUnit)arch.read("sizeUnit").value<int>());
	value->setUseValueAsSize(arch.read("useValueAsSize").value<bool>());
	value->setSymbol(arch.read("symbol").value<VipSymbol>());

	value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	value->setTextTransform(textTransform, textTransformReference);
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	return arch;
}

// regiter types
static bool register_types()
{
	qRegisterMetaType<VipScatterPoint>();
	qRegisterMetaType<VipScatterPointVector>();
	qRegisterMetaTypeStreamOperators<VipScatterPoint>();
	qRegisterMetaType<VipPlotScatter*>();
	vipRegisterArchiveStreamOperators<VipPlotScatter*>();
	return true;
}
static bool _register_types = register_types();