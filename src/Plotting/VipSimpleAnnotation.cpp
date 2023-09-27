#include "VipSimpleAnnotation.h"
#include "VipAbstractScale.h"
#include "VipQuiver.h"
#include "VipSymbol.h"
#include "VipPainter.h"
#include "VipShapeDevice.h"



void VipAnnotation::setParentShape(VipPlotShape * sh)
{
	m_shape = sh;
}
VipPlotShape * VipAnnotation::parentShape() const
{
	return m_shape;
}



namespace detail
{
	static QMap<QString, annot_func> _annotations;
	void registerAnnotationFunction(const char * name, annot_func fun)
	{
		if (_annotations.isEmpty())
			_annotations["VipSimpleAnnotation"] = createAnnotation<VipSimpleAnnotation>;
		_annotations[name] = fun;
	}
}

VipAnnotation * vipCreateAnnotation(const char * name)
{
	if (detail::_annotations.isEmpty())
		detail::_annotations["VipSimpleAnnotation"] = detail::createAnnotation<VipSimpleAnnotation>;
	QMap<QString, detail::annot_func>::const_iterator it = detail::_annotations.find(name);
	if (it == detail::_annotations.end())
		return NULL;
	return it.value()();
}
QStringList vipAnnotations() {
	return detail::_annotations.keys();
}

QByteArray vipSaveAnnotation(const VipAnnotation * annot)
{
	QByteArray ar;
	QDataStream str(&ar, QIODevice::WriteOnly);
	str.setByteOrder(QDataStream::LittleEndian);

	str << (quint32)strlen(annot->name());
	str.writeRawData(annot->name(), (int)strlen(annot->name()));
	annot->save(str);
	return ar;
}

VipAnnotation * vipLoadAnnotation(const QByteArray & ar)
{
	QDataStream str(ar);
	str.setByteOrder(QDataStream::LittleEndian);
	quint32 len = 0;
	str >> len;

	if (len > 100 || len <= 0 || str.status() != QDataStream::Ok)
		return NULL;
	QByteArray name(len, 0);
	str.readRawData(name.data(), len);
	VipAnnotation * annot = vipCreateAnnotation(name.data());
	if (!annot)
		return NULL;

	if (!annot->load(str)) {
		delete annot;
		return NULL;
	}

	return annot;
}





class VipSimpleAnnotation::PrivateData
{
public:
	VipQuiverPath quiver; //for arrow only
	double text_distance; //for all, distance to the region
	VipText text; //for all
	Qt::Alignment text_alignment; //for all
	VipSymbol symbol; //for arrow only, pen and brush for all
	int end_style; //for arrow only
	double endSize;
	Vip::RegionPositions text_position; //for rect and ellipse only

	PrivateData()
		:text_distance(0), text_alignment(Qt::AlignTop | Qt::AlignCenter), end_style(-1), endSize(7), text_position(Vip::XInside | Vip::YInside)
	{
		symbol.setBrush(QBrush());
		quiver.setExtremityBrush(VipQuiverPath::End, QBrush());
		symbol.setPen(QColor(Qt::red));
		quiver.setPen(QColor(Qt::red));
		quiver.setExtremityPen(VipQuiverPath::End, QColor(Qt::red));
		text.setTextPen(QColor(Qt::red));
	}
};


VipSimpleAnnotation::VipSimpleAnnotation()
{
	m_data = new PrivateData();
}
VipSimpleAnnotation::~VipSimpleAnnotation()
{
	delete m_data;
}

void VipSimpleAnnotation::setPen(const QPen & p)
{
	m_data->symbol.setPen( p);
	m_data->quiver.setPen(p);
	m_data->quiver.setExtremityPen(VipQuiverPath::End, p);
}
const QPen & VipSimpleAnnotation::pen() const
{
	return m_data->symbol.pen();
}

void VipSimpleAnnotation::setBrush(const QBrush& b)
{
	m_data->symbol.setBrush( b);
	m_data->quiver.setExtremityBrush(VipQuiverPath::End, b);
}
const QBrush & VipSimpleAnnotation::brush() const
{
	return m_data->symbol.brush();
}

void VipSimpleAnnotation::setEndStyle(int style)
{
	m_data->end_style = style;
}
int VipSimpleAnnotation::endStyle() const
{
	return m_data->end_style;
}

void  VipSimpleAnnotation::setEndSize(double s)
{
	m_data->endSize = s;
	m_data->symbol.setSize(QSizeF(s, s));
	m_data->quiver.setLength(VipQuiverPath::End, s);
}
double  VipSimpleAnnotation::endSize() const
{
	return m_data->endSize;
}

void VipSimpleAnnotation::setText(const QString & t)
{
	m_data->text.setText(t);
}
const VipText & VipSimpleAnnotation::text() const
{
	return m_data->text;
}
VipText & VipSimpleAnnotation::text()
{
	return m_data->text;
}

void VipSimpleAnnotation::setTextDistance(double d)
{
	m_data->text_distance = d;
}
double VipSimpleAnnotation::textDistance() const
{
	return m_data->text_distance;
}

void VipSimpleAnnotation::setArrowAngle(double angle)
{
	m_data->quiver.setAngle(VipQuiverPath::End, angle);
}
double VipSimpleAnnotation::arrowAngle() const
{
	return m_data->quiver.angle(VipQuiverPath::End);
}

void VipSimpleAnnotation::setTextAlignment(Qt::Alignment a)
{
	m_data->text_alignment = a;
}
Qt::Alignment VipSimpleAnnotation::textAlignment() const
{
	return m_data->text_alignment;
}

void VipSimpleAnnotation::setTextPosition(Vip::RegionPositions p)
{
	m_data->text_position = p;
}
Vip::RegionPositions VipSimpleAnnotation::textPosition() const
{
	return m_data->text_position;
}

QPainterPath VipSimpleAnnotation::shape(const VipShape & sh, const VipCoordinateSystemPtr & m) const
{
	VipShapeDevice dev;
	QPainterPath res;
	{
		QPainter p(&dev);
		draw(sh, &p, m);
		res = dev.shape();
	}
	return res;
}

void VipSimpleAnnotation::save(QDataStream & stream) const
{
	stream << pen() << brush() << endStyle() << endSize()<< text() << textDistance() << arrowAngle() << (int)textAlignment() << (int)textPosition();
}
bool VipSimpleAnnotation::load(QDataStream & stream)
{
	QPen p;
	QBrush b;
	int es;
	double esi;
	VipText t;
	double td;
	double a;
	int ta;
	int tp;
	stream >> p >> b >> es >>esi>> t >>td>> a >> ta >> tp;
	if (stream.status() != QDataStream::Ok)
		return false;
	if (tp < 0 || tp > Vip::Automatic)
		return false;
	if (ta < 0 || ta > 256)
		return false;
	setPen(p);
	setBrush(b);
	setEndStyle(es);
	setEndSize(esi);
	text() =(t);
	setTextDistance(td);
	setArrowAngle(a);
	setTextAlignment((Qt::Alignment)ta);
	setTextPosition((Vip::RegionPositions)tp);
	return true;
}

void VipSimpleAnnotation::draw(const VipShape & sh, QPainter * painter, const VipCoordinateSystemPtr & m) const
{
	painter->setRenderHints(QPainter::HighQualityAntialiasing | QPainter::TextAntialiasing);

	if (sh.type() == VipShape::Point)
		drawPoint(sh, painter, m);
	else if (sh.type() == VipShape::Polyline)
		drawArrow(sh, painter, m);
	else
		drawShape(sh, painter, m);
}


void VipSimpleAnnotation::drawShape(const VipShape & sh, QPainter * painter, const VipCoordinateSystemPtr & m) const
{
	//draw the path
	QPainterPath path = m->transform(sh.shape());
	painter->setPen(pen());
	painter->setBrush(brush());
	VipPainter::drawPath(painter, path);

	//draw the text
	QRectF brect = path.boundingRect();
	QRectF trect = text().textRect();
	QPointF text_pos;

	//set X position
	if (textPosition() & Vip::XInside) {
		if (textAlignment() & Qt::AlignLeft)
			text_pos.setX(textDistance() + brect.left());
		else if (textAlignment() & Qt::AlignRight)
			text_pos.setX(brect.right() - textDistance() - trect.width());
		else
			text_pos.setX(brect.left() + (brect.width() - trect.width()) / 2);
	}
	else {
		if (textAlignment() & Qt::AlignLeft)
			text_pos.setX(brect.left() - textDistance() - trect.width());
		else
			text_pos.setX(brect.right() + textDistance());
	}

	//set Y position
	if (textPosition() & Vip::YInside) {
		if (textAlignment() & Qt::AlignTop)
			text_pos.setY(textDistance() + brect.top());
		else if (textAlignment() & Qt::AlignBottom)
			text_pos.setY(brect.bottom() - textDistance() - trect.height());
		else
			text_pos.setY(brect.top() + (brect.height() - trect.height()) / 2);
	}
	else {
		if (textAlignment() & Qt::AlignTop)
			text_pos.setY(brect.top() - textDistance() - trect.height());
		else
			text_pos.setY(brect.bottom() + textDistance());
	}


	text().draw(painter, text_pos);
}

void VipSimpleAnnotation::drawArrow(const VipShape & sh, QPainter * painter, const VipCoordinateSystemPtr & m) const
{
	QPolygonF polyline = m->transform(sh.polyline());

	if (polyline.size()) {
		//draw the polyline
		if (m_data->end_style == Arrow) {
			m_data->quiver.setStyle(VipQuiverPath::EndArrow);
			m_data->quiver.draw(painter, polyline);
		}
		else {
			painter->setPen(pen());

			QPointF last = polyline.last();

			//clip polyline to symbol shape
			if (endStyle() > 0 && endStyle() < Arrow) {
				QPainterPath p;
				const QPolygonF saved = polyline;
				p.addPolygon(polyline << (polyline.first()+QPointF(1,1)));
				m_data->symbol.setStyle(VipSymbol::Style(endStyle()));
				QPainterPath s = m_data->symbol.shape(last);
				p = p.subtracted(s);
				polyline = p.toFillPolygon();
				if (polyline.size() < saved.size())
					polyline = saved;
				else
					polyline = polyline.mid(0, saved.size());
			}

			VipPainter::drawPolyline(painter, polyline);

			//draw the symbol
			if (endStyle() >= 0 && endStyle() < Arrow) {
				m_data->symbol.setStyle(VipSymbol::Style(endStyle()));
				m_data->symbol.drawSymbol(painter, last);
			}
		}

		//draw the text
		QRectF trect = text().textRect();
		QPointF center = polyline.first();
		QPointF text_pos;
		if (textAlignment() & Qt::AlignLeft)
			text_pos.setX(center.x() - trect.width() - textDistance());
		else if (textAlignment() & Qt::AlignRight)
			text_pos.setX(center.x() + textDistance());
		else
			text_pos.setX(center.x() - trect.width() / 2);
		if (textAlignment() & Qt::AlignTop)
			text_pos.setY(center.y() - trect.height() - textDistance());
		else if (textAlignment() & Qt::AlignBottom)
			text_pos.setY(center.y() + textDistance());
		else
			text_pos.setY(center.y() - trect.height() / 2);

		text().draw(painter, text_pos);
	}
}

void VipSimpleAnnotation::drawPoint(const VipShape & sh, QPainter * painter, const VipCoordinateSystemPtr & m) const
{

	QPointF pos = m->transform(sh.point());
	//draw the symbol
	if (endStyle() >= 0 && endStyle() < Arrow) {
		m_data->symbol.setStyle(VipSymbol::Style(endStyle()));
		m_data->symbol.drawSymbol(painter, pos);
	}
	//draw the text
	QRectF trect = text().textRect();
	QPointF center = pos;
	QPointF text_pos;
	if (textAlignment() & Qt::AlignLeft)
		text_pos.setX(center.x() - trect.width() - textDistance());
	else if (textAlignment() & Qt::AlignRight)
		text_pos.setX(center.x() + textDistance());
	else
		text_pos.setX(center.x() - trect.width() / 2);
	if (textAlignment() & Qt::AlignTop)
		text_pos.setY(center.y() - trect.height() - textDistance());
	else if (textAlignment() & Qt::AlignBottom)
		text_pos.setY(center.y() + textDistance());
	else
		text_pos.setY(center.y() - trect.height() / 2);

	text().draw(painter, text_pos);
}



#include "VipStyleSheet.h"

static EnumOrParser & parseAlignment() {
	static QMap<QByteArray, int> values;
	static QSharedPointer<EnumOrParser> parser;
	if (values.isEmpty()) {
		values.insert("left", Qt::AlignLeft);
		values.insert("top", Qt::AlignTop);
		values.insert("right", Qt::AlignRight);
		values.insert("bottom", Qt::AlignBottom);
		values.insert("hcenter", Qt::AlignHCenter);
		values.insert("vcenter", Qt::AlignVCenter);
		values.insert("center", Qt::AlignCenter);
		parser.reset(new EnumOrParser(values));
	}
	return *parser;
}
static EnumOrParser & parsePosition() {
	static QMap<QByteArray, int> values;
	static QSharedPointer<EnumOrParser> parser;
	if (values.isEmpty()) {
		values.insert("xinside", Vip::XInside);
		values.insert("yinside", Vip::YInside);
		values.insert("inside", Vip::Inside);
		values.insert("outside", Vip::Outside);
		parser.reset(new EnumOrParser(values));
	}
	return *parser;
}
static EnumParser & parseSymbol() {
	static QMap<QByteArray, int> values;
	static QSharedPointer<EnumParser> parser;
	if (values.isEmpty()) {
		values.insert("none", VipSimpleAnnotation::None);
		values.insert("ellipse", VipSimpleAnnotation::Ellipse);
		values.insert("rect", VipSimpleAnnotation::Rect);
		values.insert("diamond", VipSimpleAnnotation::Diamond);
		values.insert("triangle", VipSimpleAnnotation::Triangle);
		values.insert("dtriangle", VipSimpleAnnotation::DTriangle);
		values.insert("utriangle", VipSimpleAnnotation::UTriangle);
		values.insert("ltriangle", VipSimpleAnnotation::LTriangle);
		values.insert("rtriangle", VipSimpleAnnotation::RTriangle);
		values.insert("cross", VipSimpleAnnotation::Cross);
		values.insert("xcross", VipSimpleAnnotation::XCross);
		values.insert("hline", VipSimpleAnnotation::HLine);
		values.insert("vline", VipSimpleAnnotation::VLine);
		values.insert("star1", VipSimpleAnnotation::Star1);
		values.insert("star2", VipSimpleAnnotation::Star2);
		values.insert("hexagon", VipSimpleAnnotation::Hexagon);
		parser.reset(new EnumParser(values));
	}
	return *parser;
}

QPair<VipShape,VipSimpleAnnotation*> vipAnnotation(const QString & type, const QString & text,
	const QPointF & start, const QPointF & end , const QVariantMap & attributes , QString * error )
{
	VipSimpleAnnotation * a = new VipSimpleAnnotation();
	VipShape shape;

	//set the default parameters
	a->setPen(QColor(Qt::red));
	a->setBrush(QBrush());
	a->setEndStyle(VipSimpleAnnotation::Ellipse);
	a->setEndSize(9);
	a->setText(text);
	//a->text().setTextPen(vipWidgetTextBrush(pl).color());
	a->text().boxStyle().setDrawLines(Vip::NoSide);
	a->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);

	//parse annotation type
	if (type == "line") {
		if (end == QPointF()) {
			shape.setPoint(start);
			a->setTextDistance(a->endSize());
			a->setEndStyle(VipSimpleAnnotation::Ellipse);
			a->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
		}
		else {
			shape.setPolyline(QPolygonF() << start << end);
		}
	}
	else if (type == "arrow") {
		shape.setPolyline(QPolygonF() << start << end);
		a->setTextDistance(a->endSize());
		a->setEndStyle(VipSimpleAnnotation::Arrow);
		a->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
	}
	else if (type == "rectangle" || type == "ellipse") {
		if(type == "rectangle")
			shape.setRect(QRectF(start, end).normalized());
		else {
			QPainterPath p;
			p.addEllipse(QRectF(start, end).normalized());
			shape.setShape(p);
		}
		a->text().boxStyle().setDrawLines(Vip::NoSide);
		a->setTextPosition(Vip::XInside);
	}
	else if (type == "textbox") {
		shape.setPoint(start);
		a->setPen(QColor(Qt::transparent));
		a->setBrush(QBrush());
		a->setEndStyle(VipSimpleAnnotation::None);
	}
	else {
		delete a;
		if (error)
			*error = "unrecognized type: " + type;
		return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
	}

	//parse attributes
	// - "textcolor" : annotation text pen given as a QPen, QColor or a QString (uses #PenParser from stylesheet mechanism)
	// - "textbackground" : annotation text background color given as a QColor or QString
	// - "textborder" : annotation text outline (border box pen)
	// - "textradius" : annotation text border radius of the border box
	// - "textsize" : size in points of the text font
	// - "bold" : use bold font for the text
	// - "italic" : use italic font for the text
	// - "fontfamilly": font familly for the text
	// - "border" : shape pen
	// - "background" : shape brush
	// - "distance" : distance between the annotation text and the shape
	// - "alignment" : annotation text alignment around the shape (combination of 'left', 'right', 'top', 'bottom', 'hcenter', vcenter', 'center')
	// - "position" : text position around the shape (combination of 'xinside', 'yinside', 'inside', 'outside')
	// - "symbol" : for 'line' only, symbol for the end point (one of 'none', 'ellipse', 'rect', 'diamond', 'triangle', 'dtriangle', 'utriangle', 'ltriangle', 'rtriangle', 'cross', 'xcross', 'hline', 'vline', 'star1', 'star2', 'hexagon')
	// - "symbolsize" : for 'line' and 'arrow', symbol size for the end point
	{
		QVariantMap::const_iterator it = attributes.find("textcolor");
		if (it != attributes.end()) {
			QVariant v = it.value();
			if (v.userType() == qMetaTypeId<QPen>()) a->text().setTextPen(v.value<QPen>());
			else if (v.userType() == qMetaTypeId<QColor>()) a->text().textPen().setColor(v.value<QColor>());
			else {
				QByteArray val = v.toByteArray();
				PenParser p;
				v = p.parse(val);
				if (v.userType() == 0) {
					delete a;
					if (error) *error = "wrong 'textcolor' attribute: " + val;
					return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
				}
				a->text().setTextPen(v.value<QPen>());
			}
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("textbackground");
		if (it != attributes.end()) {
			QVariant v = it.value();
			if (v.userType() == qMetaTypeId<QColor>()) a->text().setBackgroundBrush(QBrush(v.value<QColor>()));
			else {
				QByteArray val = v.toByteArray();
				ColorParser p;
				v = p.parse(val);
				if (v.userType() == 0) {
					delete a;
					if (error) *error = "wrong 'textbackground' attribute: " + val;
					return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
				}
				a->text().setBackgroundBrush(QBrush(v.value<QColor>()));
			}
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("textsize");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			double size = v.toDouble(&ok);
			if (!ok) {
				delete a;
				if (error) *error = "wrong 'textsize' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			QFont f = a->text().font();
			f.setPointSizeF(size);
			a->text().setFont(f);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("bold");
		if (it != attributes.end()) {
			BoolParser p;
			bool bold = p.parse(it.value().toByteArray()).toBool();
			QFont f = a->text().font();
			f.setBold(bold);
			a->text().setFont(f);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("italic");
		if (it != attributes.end()) {
			BoolParser p;
			bool italic = p.parse(it.value().toByteArray()).toBool();
			QFont f = a->text().font();
			f.setItalic(italic);
			a->text().setFont(f);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("fontfamilly");
		if (it != attributes.end()) {
			QString familly = it.value().toString();
			QFont f = a->text().font();
			f.setFamily(familly);
			a->text().setFont(f);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("textborder");
		if (it != attributes.end()) {
			QVariant v = it.value();
			if (v.userType() == qMetaTypeId<QPen>()) a->text().setBorderPen(v.value<QPen>());
			else if (v.userType() == qMetaTypeId<QColor>()) a->text().setBorderPen(v.value<QColor>());
			else {
				QByteArray val = v.toByteArray();
				PenParser p;
				v = p.parse(val);
				if (v.userType() == 0) {
					delete a;
					if (error) *error = "wrong 'textborder' attribute: " + val;
					return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
				}
				a->text().setBorderPen(v.value<QPen>());
			}
			a->text().boxStyle().setDrawLines(Vip::AllSides);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("textradius");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			double dist = v.toDouble(&ok);
			if (!ok) {
				delete a;
				if (error) *error = "wrong 'textradius' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			a->text().boxStyle().setBorderRadius(dist);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("border");
		if (it != attributes.end()) {
			QVariant v = it.value();
			if (v.userType() == qMetaTypeId<QPen>()) a->setPen(v.value<QPen>());
			else if (v.userType() == qMetaTypeId<QColor>()) a->setPen(QPen(v.value<QColor>()));
			else {
				QByteArray val = v.toByteArray();
				PenParser p;
				v = p.parse(val);
				if (v.userType() == 0) {
					delete a;
					if (error) *error = "wrong 'border' attribute: " + val;
					return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
				}
				a->setPen(v.value<QPen>());
			}
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("background");
		if (it != attributes.end()) {
			QVariant v = it.value();
			if (v.userType() == qMetaTypeId<QColor>()) a->setBrush(QBrush(v.value<QColor>()));
			else {
				QByteArray val = v.toByteArray();
				ColorParser p;
				v = p.parse(val);
				if (v.userType() == 0) {
					delete a;
					if (error) *error = "wrong 'background' attribute: " + val;
					return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
				}
				a->setBrush(QBrush(v.value<QColor>()));
			}
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("distance");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			double dist = v.toDouble(&ok);
			if (!ok) {
				delete a;
				if (error) *error = "wrong 'distance' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			a->setTextDistance(dist);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("alignment");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			int align = v.toInt(&ok);
			if (!ok)
				align = parseAlignment().parse(v.toByteArray()).toInt(&ok);
			if(!ok) {
				delete a;
				if (error) *error = "wrong 'alignment' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			a->setTextAlignment((Qt::Alignment)align);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("position");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			int pos = v.toInt(&ok);
			if (!ok)
				pos = parsePosition().parse(v.toByteArray()).toInt(&ok);
			if (!ok) {
				delete a;
				if (error) *error = "wrong 'position' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			a->setTextPosition((Vip::RegionPositions)pos);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("symbol");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			int sym = v.toInt(&ok);
			if (!ok)
				sym = parseSymbol().parse(v.toByteArray()).toInt(&ok);
			if (!ok) {
				delete a;
				if (error) *error = "wrong 'symbol' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			a->setEndStyle((VipSimpleAnnotation::EndStyle)sym);
		}
	}
	{
		QVariantMap::const_iterator it = attributes.find("symbolsize");
		if (it != attributes.end()) {
			QVariant v = it.value();
			bool ok;
			double size = v.toDouble(&ok);
			if (!ok) {
				delete a;
				if (error) *error = "wrong 'symbolsize' attribute: " + v.toString();
				return QPair<VipShape, VipSimpleAnnotation*>(VipShape(), NULL);
			}
			a->setEndSize(size);
		}
	}

	return QPair<VipShape, VipSimpleAnnotation*>(shape,a);
}
