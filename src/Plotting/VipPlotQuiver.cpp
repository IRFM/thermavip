#include <cmath>

#include "VipPlotQuiver.h"
#include "VipAxisColorMap.h"
#include "VipPainter.h"
#include "VipShapeDevice.h"



static int registerQuiverKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {
		
		QMap<QByteArray, int> style;
		style["line"] = VipQuiverPath::Line;
		style["startArrow"] = VipQuiverPath::StartArrow;
		style["startCircle"] = VipQuiverPath::StartCircle;
		style["startSquare"] = VipQuiverPath::StartSquare;
		style["endArrow"] = VipQuiverPath::EndArrow;
		style["endCircle"] = VipQuiverPath::EndCircle;
		style["endSquare"] = VipQuiverPath::EndSquare;

		keywords["arrow-style"] = VipParserPtr(new EnumOrParser(style));
		keywords["arrow-size"] = VipParserPtr(new DoubleParser());
		keywords["text-alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["text-position"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::regionPositionEnum()));
		keywords["text-distance"] = VipParserPtr(new DoubleParser());

		vipSetKeyWordsForClass(&VipPlotQuiver::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerQuiverKeyWords = registerQuiverKeyWords();




class VipPlotQuiver::PrivateData
{
public:

	PrivateData()
	  : textAlignment(Qt::AlignTop | Qt::AlignHCenter)
	  , textPosition(Vip::XInside)
	  , textDistance(0)
	{
	}

	VipQuiverPath quiver;

	QList<VipInterval> bounding;

	VipInterval dataInterval;
	VipInterval dataValidInterval;

	Qt::Alignment textAlignment;
	Vip::RegionPositions textPosition;
	QTransform textTransform;
	QPointF textTransformReference;
	double textDistance;
	VipText text;
	QSharedPointer<VipTextStyle> textStyle;
};

VipPlotQuiver::VipPlotQuiver(const VipText & title )
:VipPlotItemDataType(title)
{
	d_data = new PrivateData();

	setMajorColor(Qt::blue);
	QPen p(Qt::blue);
	p.setJoinStyle(Qt::MiterJoin);
	setPen(p);
}

VipPlotQuiver::~VipPlotQuiver()
{
	delete d_data;
}

void VipPlotQuiver::setData(const QVariant& data) 
{
	VipPlotItemDataType::setData(data);

	Locker locker(dataLock());
	const VipQuiverPointVector vec = data.value<VipQuiverPointVector>();
	d_data->bounding = dataBoundingIntervals(vec);
	d_data->dataValidInterval = Vip::InfinitInterval;
	d_data->dataInterval = computeInterval(vec, Vip::InfinitInterval);
}

QList<VipInterval> VipPlotQuiver::dataBoundingIntervals(const VipQuiverPointVector& vec) const
{
	if (vec.isEmpty())
		return QList<VipInterval>();

	double min_x = std::min(vec.first().position.x(), vec.first().destination.x());
	double max_x = std::max(vec.first().position.x(), vec.first().destination.x());
	double min_y = std::min(vec.first().position.y(), vec.first().destination.y());
	double max_y = std::max(vec.first().position.y(), vec.first().destination.y());
	VipInterval x(min_x, max_x);
	VipInterval y(min_y, max_y);

	for (int i = 1; i < vec.size(); ++i) {
		const VipQuiverPoint& pt = vec[i];
		min_x = std::min(pt.position.x(), pt.destination.x());
		max_x = std::max(pt.position.x(), pt.destination.x());
		min_y = std::min(pt.position.y(), pt.destination.y());
		max_y = std::max(pt.position.y(), pt.destination.y());
		x = x.extend(min_x);
		x = x.extend(max_x);
		y = y.extend(min_y);
		y = y.extend(max_y);
	}
	return QList<VipInterval>() << x << y;
}
VipInterval VipPlotQuiver::computeInterval(const VipQuiverPointVector& vec, const VipInterval& inter) const
{
	if (vec.isEmpty())
		return VipInterval();

	VipInterval res;
	for (int i = 0; i < vec.size(); ++i) {
		const VipQuiverPoint& pt = vec[i];
		if (inter.contains(pt.value)) {
			if (!res.isValid())
				res = VipInterval(pt.value, pt.value);
			else
				res = res.extend(pt.value);
		}
	}

	return res;
}
	
int VipPlotQuiver::findQuiverIndex(const VipQuiverPointVector& vec, const QPointF& pos, double max_dist) const
{
	const VipCoordinateSystemPtr m = this->sceneMap();
	if (!m || m->axes().size() != 2)
		return -1;

	QPainterPathStroker stroke;
	// take a 2px margin + max_dist
	stroke.setWidth(max_dist + 2);

	for (int i = 0; i < vec.size(); ++i) {
		QLineF line(m->transform(vec[i].position), m->transform(vec[i].destination));
		
		QPainterPath p;
		p.moveTo(m->transform(vec[i].position));
		p.lineTo(m->transform(vec[i].destination));

		if (stroke.createStroke(p).contains(pos))
			return i;
	}
	return -1;
}

QString VipPlotQuiver::formatText(const QString& text, const QPointF& pos) const 
{
	QString res = VipPlotItem::formatText(text, pos);
	const VipQuiverPointVector vec = rawData();
	int index = findQuiverIndex(vec, pos, 10);
	if (index < 0)
		return res;

	return VipText::replace(res, "#value", vec[index].value);

}
bool VipPlotQuiver::areaOfInterest(const QPointF& pos, int , double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const 
{
	const VipQuiverPointVector vec = rawData();
	int index = findQuiverIndex(vec, pos, maxDistance);
	if (index < 0)
		return false;

	out_pos << pos;
	legend = 0;

	VipShapeDevice dev;
	dev.setDrawPrimitives(VipShapeDevice::All);
	const VipQuiverPoint p = vec[index];

	VipQuiver q((QPointF)sceneMap()->transform(p.position), (QPointF)sceneMap()->transform(p.destination));
	QPainter painter(&dev);
	d_data->quiver.draw(&painter, q.line());
	painter.end();
	style.computePath(dev.shape());
	return true;
}

QList<VipInterval> VipPlotQuiver::plotBoundingIntervals() const
{
	Locker locker(dataLock());
	QList<VipInterval> res = d_data->bounding;
	if (res.isEmpty()) {
		res = const_cast<PrivateData*>(d_data)->bounding = dataBoundingIntervals(rawData());
	}
	res.detach();
	return res;
}

bool VipPlotQuiver::setItemProperty(const char* name, const QVariant& value, const QByteArray& index) 
{
	if (value.userType() == 0)
		return false;
	/// -	'text-alignment' : see VipPlotQuiver::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
	/// -	'text-position': see VipPlotQuiver::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
	/// -	'text-distance': see VipPlotQuiver::setSpacing()
	/// -	'arrow-size': floating point value defining the arrow size in item's coordinates
	/// -	'arrow-style': style of the arrow (sse VipQuiverPath), combination of 'startArrow|startSquare|startCircle|endArrow|endSquare|endCircle'

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
	if (strcmp(name, "arrow-size") == 0) {
		quiverPath().setLength(VipQuiverPath::Start, value.toDouble());
		quiverPath().setLength(VipQuiverPath::End, value.toDouble());
		return true;
	}
	if (strcmp(name, "arrow-style") == 0) {
		quiverPath().setStyle((VipQuiverPath::QuiverStyles)value.toInt());
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

void VipPlotQuiver::setTextAlignment(Qt::Alignment align)
{
	d_data->textAlignment = align;
	emitItemChanged();
}

Qt::Alignment VipPlotQuiver::textAlignment() const
{
	return d_data->textAlignment;
}

void VipPlotQuiver::setTextPosition(Vip::RegionPositions pos)
{
	d_data->textPosition = pos;
	emitItemChanged();
}

Vip::RegionPositions VipPlotQuiver::textPosition() const
{
	return d_data->textPosition;
}


void VipPlotQuiver::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->text.setTextStyle(st);
	emitItemChanged();
}


void VipPlotQuiver::setTextTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform = tr;
	d_data->textTransformReference = ref;
	emitItemChanged();
}
const QTransform& VipPlotQuiver::textTransform() const
{
	return d_data->textTransform;
}
const QPointF& VipPlotQuiver::textTransformReference() const
{
	return d_data->textTransformReference;
}

void VipPlotQuiver::setTextDistance(double vipDistance)
{
	d_data->textDistance = vipDistance;
	emitItemChanged();
}

double VipPlotQuiver::textDistance() const
{
	return d_data->textDistance;
}

void VipPlotQuiver::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

const VipText& VipPlotQuiver::text() const
{
	return d_data->text;
}

VipText& VipPlotQuiver::text()
{
	return d_data->text;
}


void VipPlotQuiver::draw(QPainter * painter, const VipCoordinateSystemPtr & m) const
{
	QPen p((d_data->quiver.pen()));
	VipQuiverPath quiver = d_data->quiver;

	const VipQuiverPointVector vector = rawData();
	const bool use_colormap = colorMap();
	VipQuiver q;

	VipText t = d_data->text;

	for(int i = 0; i < vector.size(); ++i )
	{
		const VipQuiverPoint& tmp = vector[i];

		if (use_colormap)
		{
			quiver.setColor(color(tmp.value,p.color().rgba()));
		}

		VipQuiver _q((QPointF)m->transform(tmp.position), (QPointF)m->transform(tmp.destination));
		QLineF line = _q.line();
		quiver.draw(painter, line);

		if (!d_data->text.isEmpty()) {
			t.setText(VipText::replace(d_data->text.text(), "#value", tmp.value));
			VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), QRectF(line.p1(), line.p2()).normalized());
		}
	}
}

QRectF VipPlotQuiver::drawLegend(QPainter * painter, const QRectF & r, int index) const
{
	Q_UNUSED(index)
	d_data->quiver.draw(painter,QLineF(QPointF(r.left(),r.center().y()),QPointF(r.right(),r.center().y())));
	return r;
}


VipInterval VipPlotQuiver::plotInterval(const VipInterval& interval) const
{
	if (d_data->dataInterval.isValid() && d_data->dataValidInterval == interval)
		return d_data->dataInterval;
	Locker lock(dataLock());
	const_cast<VipPlotQuiver*>(this)->d_data->dataValidInterval = interval;
	return const_cast<VipPlotQuiver*>(this)->d_data->dataInterval = computeInterval(rawData(), interval);
}

void VipPlotQuiver::setQuiverPath(const VipQuiverPath & q)
{
	d_data->quiver = q;
	emitItemChanged();
}

const VipQuiverPath & VipPlotQuiver::quiverPath() const
{
	return d_data->quiver;
}

VipQuiverPath & VipPlotQuiver::quiverPath()
{
	return d_data->quiver;
}


QDataStream& operator<<(QDataStream& str, const VipQuiverPoint& p)
{
	return str << p.destination << p.position << p.value;
}
QDataStream& operator>>(QDataStream& str, VipQuiverPoint& p)
{
	return str >> p.destination >> p.position >> p.value;
}

static bool register_types()
{
	qRegisterMetaType<VipQuiverPoint>();
	qRegisterMetaType<VipQuiverPointVector>();
	qRegisterMetaTypeStreamOperators<VipQuiverPoint>();
	return true;
}
static bool _register_types = register_types();