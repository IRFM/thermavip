#include "VipStyleSheet.h"
#include "VipText.h"
#include "VipColorMap.h"
#include "VipPlotItem.h"
#include "VipAxisColorMap.h"
#include "VipText.h"
#include "VipLogging.h"
#include "VipSet.h"

#include <qbytearray.h>
#include <qmap.h>
#include <qsharedpointer.h>
#include <qvariant.h>
#include <qapplication.h>

#include <vector>
#include <deque>

/// Parse a comment block.
/// start_comment is the start index of the comment ("/*").
/// Returns the index of the first character after the end of comment, or -1 if none.
static int parseComment(const QByteArray & ar, int start_comment)
{
	for (int i = start_comment +2; i < ar.size(); ++i) {
		if (ar[i] == '*' && i < (ar.size() - 1) && ar[i + 1] == '/')
			return i + 2;
	}
	return -1;
}

/// Parse a string.
/// start_string is the position of the ' or " character.
/// Returns the position of the corresponding ' or ", or -1 if none.
static int parseString(const QByteArray & ar, int start_string)
{
	char token = ar[start_string];
	for (int i = start_string + 1; i < ar.size(); ++i)
	{
		if (ar[i] == token)
			return i;
	}
	return -1;
}

static bool cleanLine(const QByteArray & ar, int & start, int & end)
{
	if (ar.isEmpty()) return false;
	start = 0;
	while (start < ar.size() && (ar[start] == ' ' || ar[start] == '\t' || ar[start] == '\r' || ar[start] == '\n'))
		++start;
	if (start == ar.size()) return false;
	end = ar.size() - 1;
	while (end >= start && (ar[end] == ' ' || ar[end] == '\t' || ar[end] == '\r' || ar[end] == '\n'))
		--end;
	if (end < 0) return false;
	++end;
	return true;
}

static bool removeQuote(QByteArray & ar) 
{
	// Remove '' or "" from string if it exists
	int ids = ar.indexOf("'");
	int idd = ar.indexOf('"');
	if (ids == -1 && idd == -1)
		return true; // no double quote, find

	int start = 0;
	char q = '"';

	if (ids != -1) {
		if (idd == -1 || idd > ids) {
			start = ids;
			q = '\'';
		}
		else if (idd != -1) {
			start = idd;
			q = '"';
		}
	}
	else {
		start = idd;
		q = '"';
	}

	ids = ar.lastIndexOf("'");
	idd = ar.lastIndexOf('"');

	if (q == '"' && ids > idd)
		return false; // starts with ", finish with '
	if (q == '\'' && idd > ids)
		return false; // starts with ', finish with "

	int end = q == '"' ? idd : ids;
	ar = ar.mid(start + 1, end - start - 1);
	return true;
}

static QByteArray cleanLine(const QByteArray & ar)
{
	int s, e;
	if (!cleanLine(ar, s, e))return QByteArray();
	return ar.mid(s, e - s);
}

static QByteArray cleanKey(const QByteArray & ar)
{
	QByteArray res;
	res.reserve(ar.size());
	for (int i = 0; i < ar.size(); ++i)
		if (ar[i] != ' ' && ar[i] != '\t' && ar[i] != '\r' && ar[i] != '\n')
			res.append(ar[i]);
	return res;
}

static int numDigits(int x)
{
	x = abs(x);
	return (x < 10 ? 1 :
		(x < 100 ? 2 :
		(x < 1000 ? 3 :
			(x < 10000 ? 4 :
			(x < 100000 ? 5 :
				(x < 1000000 ? 6 :
				(x < 10000000 ? 7 :
					(x < 100000000 ? 8 :
					(x < 1000000000 ? 9 :
						10)))))))));
}

#define ERROR(ret) \
{ if(ok) *ok = false; return ret;}

static QColor parseColor(const QByteArray & ar, int & parse_end, bool *ok = NULL)
{
	//build "default" colors
	static QMap<QByteArray, QColor> default_colors;
	if (default_colors.isEmpty()) {
		default_colors["black"] = Qt::black;
		default_colors["white"] = Qt::white;
		default_colors["red"] = Qt::red;
		default_colors["darkRed"] = Qt::darkRed;
		default_colors["green"] = Qt::green;
		default_colors["darkGreen"] = Qt::darkGreen;
		default_colors["blue"] = Qt::blue;
		default_colors["darkBlue"] = Qt::darkBlue;
		default_colors["cyan"] = Qt::cyan;
		default_colors["darkCyan"] = Qt::darkCyan;
		default_colors["magenta"] = Qt::magenta;
		default_colors["darkMagenta"] = Qt::darkMagenta;
		default_colors["yellow"] = Qt::yellow;
		default_colors["darkYellow"] = Qt::darkYellow;
		default_colors["gray"] = Qt::gray;
		default_colors["darkGray"] = Qt::darkGray;
		default_colors["lightGray"] = Qt::lightGray;
		default_colors["transparent"] = Qt::transparent;
	}
	//cleaning
	int start, end;
	if (!cleanLine(ar, start, end)) {
		ERROR(QColor())
	}

	//find format
	if (ar[start] == '#') {
		//read color in hexadecimal
		bool _ok;
		int val = ar.mid(start + 1).toInt(&_ok,16);
		if (!_ok) ERROR(QColor());
		if (ok) *ok = true;
		parse_end = start + 1 + numDigits(val);
		return QColor(val);
	}
	else if (ar.mid(start, 4) == "rgb(") {
		int index = ar.indexOf(")", start + 4);
		if (index < 0) ERROR(QColor());
		QList<QByteArray> components = ar.mid(start + 4, index - (start + 4)).split(',');
		if(components.size() != 3) ERROR(QColor());
		QColor res;
		bool _ok;
		res.setRed(components[0].toInt(&_ok)); if(!_ok) ERROR(QColor());
		res.setGreen(components[1].toInt(&_ok)); if (!_ok) ERROR(QColor());
		res.setBlue(components[2].toInt(&_ok)); if (!_ok) ERROR(QColor());
		if (ok) *ok = true;
		parse_end = index + 1;
		return res;
	}
	else if (ar.mid(start, 5) == "rgba(") {
		int index = ar.indexOf(")", start + 5);
		if (index < 0) ERROR(QColor());
		QList<QByteArray> components = ar.mid(start + 5, index - (start + 5)).split(',');
		if (components.size() != 4) ERROR(QColor());
		QColor res;
		bool _ok;
		res.setRed(components[0].toInt(&_ok)); if (!_ok) ERROR(QColor());
		res.setGreen(components[1].toInt(&_ok)); if (!_ok) ERROR(QColor());
		res.setBlue(components[2].toInt(&_ok)); if (!_ok) ERROR(QColor());
		res.setAlpha(components[3].toInt(&_ok)); if (!_ok) ERROR(QColor());
		if (ok) *ok = true;
		parse_end = index + 1;
		return res;
	}
	else {
		
		//default colors
		QTextStream str(ar);
		QString color;
		str >> color;
		const QByteArray color_name = color.toLatin1();
		QMap<QByteArray, QColor>::const_iterator it = default_colors.find(color_name);
		if (it == default_colors.cend()) ERROR(QColor());
		if (ok) *ok = true;
		parse_end = start + color_name.size();
		return it.value();
	}
	VIP_UNREACHABLE();
	//	ERROR(QColor());
}

static QPen parsePen(const QByteArray & ar, bool *ok = NULL)
{
	static QMap<QByteArray, int> styles;
	if (styles.isEmpty()) {
		styles["solid"] = Qt::SolidLine;
		styles["dash"] = Qt::DashLine;
		styles["dot"] = Qt::DotLine;
		styles["dashdot"] = Qt::DashDotLine;
		styles["dashdotdot"] = Qt::DashDotDotLine;
		styles["none"] = Qt::NoPen;
	}

	int start, end;
	if (!cleanLine(ar, start, end)) ERROR(QPen());

	QByteArray tmp = ar.mid(start, end - start);
	QPen res;

	//check if starts with line width
	int index = tmp.indexOf("px");
	if (index >= 0) {
		bool _ok;
		double width = tmp.mid(0, index ).toDouble(&_ok);
		if (!_ok) ERROR(QPen());
		res.setWidthF(width);
		//remove the line width  and clean
		tmp = tmp.mid(index + 2);
		if (!cleanLine(tmp, start, end)) ERROR(QPen());
		tmp = tmp.mid(start, end - start);
	}

	//check for style
	if (tmp[0] == 's' || tmp[0] == 'd' || tmp[0] == 'n') {
		QTextStream str(tmp);
		QString style;
		str >> style;
		QMap<QByteArray, int>::const_iterator it = styles.find(style.toLatin1());
		if (it == styles.cend()) ERROR(QPen());
		res.setStyle((Qt::PenStyle)it.value());
		tmp = str.readAll().toLatin1();
	}

	//read color
	bool _ok;
	int parse_end;
	QColor c = parseColor(tmp, parse_end, &_ok);
	if(!_ok && res.style() != Qt::NoPen) 
		ERROR(QPen());
	res.setColor(c);

	if (ok) *ok = true;
	return res;
}

static QString parseText(const QByteArray & ar, bool *ok = NULL)
{
	//format: 'text'
	int start, end;
	if (!cleanLine(ar, start, end)) {
		if (ok)
			*ok = false;
		ERROR(QString());
	}

	QByteArray tmp = ar.mid(start, end - start);
	if (!removeQuote(tmp)) {
		if (ok)
			*ok = false;
		ERROR(QString());
	}
	if (ok)
		*ok = true;
	return tmp;

}

static int parseEnum(const QByteArray & ar, const QMap<QByteArray, int> & enums, bool *ok = NULL)
{
	int start, end;
	if (!cleanLine(ar, start, end)) ERROR(0);
	QMap<QByteArray, int>::const_iterator it = enums.find(ar.mid(start, end - start));
	if (it == enums.cend()) ERROR(0);
	if (ok) *ok = true;
	return it.value();
}

static int parseOrEnum(const QByteArray & ar, const QMap<QByteArray, int> & enums, bool *ok = NULL)
{
	int start, end;
	if (!cleanLine(ar, start, end)) ERROR(0);
	QByteArray tmp = ar.mid(start, end - start);
	QList<QByteArray> lst = tmp.split('|');
	int res = 0;
	for (int i = 0; i < lst.size(); ++i) {
		int s, e;
		if(!cleanLine(lst[i],s,e))ERROR(0);
		QByteArray val = lst[i].mid(s, e - s);
		QMap<QByteArray, int>::const_iterator it = enums.find(val);
		if (it == enums.cend()) ERROR(0);
		res |= it.value();
	}

	if (ok) *ok = true;
	return res;
}


static QByteArray colorToString(const QColor & c) 
{
	return "rgba(" + QByteArray::number(c.red()) + ", " + QByteArray::number(c.green()) + ", " + QByteArray::number(c.blue()) + ", " + QByteArray::number(c.alpha()) + ")";
}


QVariant PenParser::parse(const QByteArray & ar)
{
	bool ok;
	QPen p = parsePen(ar, &ok);
	return ok ? QVariant::fromValue(p) : QVariant();
}
QByteArray PenParser::toString(const QVariant& v) 
{
	const QPen p = v.value<QPen>();
	QByteArray style;

	if (p.style() == Qt::NoPen)
		return "none";
	else if (p.style() == Qt::SolidLine)
		style = "solid";
	else if (p.style() == Qt::DashLine)
		style = "dash";
	else if (p.style() == Qt::DotLine)
		style = "dot";
	else if (p.style() == Qt::DashDotLine)
		style = "dashdot";
	else if (p.style() == Qt::DashDotDotLine)
		style = "dashdotdot";
	
	return QByteArray::number(p.widthF()) + "px " + style + " " + colorToString(p.color());
}

QVariant BrushParser::parse(const QByteArray& )
{
	//QPen p = parsePen(ar, &ok);
	return QVariant(); // ok ? QVariant::fromValue(p) : QVariant();
}
QByteArray BrushParser::toString(const QVariant& ) 
{
	return QByteArray();
}

QVariant FontParser::parse(const QByteArray& ar)
{
	int start, end;
	if (!cleanLine(ar, start, end))
		return false;
	QByteArray line = ar.mid(start, end - start);

	char quote = '"';
	int quote_index = line.indexOf(quote);
	if (quote_index == -1) {
		quote = '\'';
		quote_index = line.indexOf(quote);
		if (quote_index == -1)
			return QVariant();
	}
	int last_quote = line.lastIndexOf(quote);
	if (last_quote == -1)
		return QVariant();
	QByteArray family = line.mid(quote_index + 1, last_quote - quote_index - 1);
	line = line.mid(0, quote_index);

	// parse style and weight
	line.replace('\t', ' ');
	QList<QByteArray> lst = line.split(' ');

	//remove empty elements
	for (int i = 0; i < lst.size(); ++i) {
		if (lst[i].isEmpty()) {
			lst.removeAt(i);
			--i;
		}
	}

	QFont res;
	res.setFamily(family);
	bool has_style = false;
	double size = 0;
	QByteArray type = "pt";
	for ( QByteArray& b : lst) {
		if (b == "normal") {
			if (!has_style) {
				res.setStyle(QFont::StyleNormal);
				has_style = true;
			}
			else
				res.setWeight(QFont::Normal);
		}
		else if (b == "italic")
			res.setStyle(QFont::StyleItalic);
		else if (b == "oblique")
			res.setStyle(QFont::StyleOblique);
		else if (b == "bold")
			res.setWeight(QFont::Bold);
		else if (b == "thin")
			res.setWeight(QFont::Thin);
		else if (b == "extralight")
			res.setWeight(QFont::ExtraLight);
		else if (b == "light")
			res.setWeight(QFont::Light);
		else if (b == "medium")
			res.setWeight(QFont::Medium);
		else if (b == "demibold")
			res.setWeight(QFont::DemiBold);
		else if (b == "extrabold")
			res.setWeight(QFont::ExtraBold);
		else if (b == "black")
			res.setWeight(QFont::Black);

		else if (b == "100")
			res.setWeight(QFont::Thin);
		else if (b == "200")
			res.setWeight(QFont::ExtraLight);
		else if (b == "300")
			res.setWeight(QFont::Light);
		else if (b == "400")
			res.setWeight(QFont::Normal);
		else if (b == "500")
			res.setWeight(QFont::Medium);
		else if (b == "600")
			res.setWeight(QFont::DemiBold);
		else if (b == "700")
			res.setWeight(QFont::Bold);
		else if (b == "800")
			res.setWeight(QFont::ExtraBold);
		else if (b == "900")
			res.setWeight(QFont::Black);
		else {
			if (b == "pt")
				type = "pt";
			else if (b == "px")
				type = "px";
			else {
				if (b.contains("px")) {
					type = "px";
					b.replace("px", "");
				}
				else if (b.contains("pt")) {
					type = "pt";
					b.replace("pt", "");
				}
				size = b.toDouble();
				if (size == 0)
					return QVariant();
				
			}
		}
	}

	if (size == 0) {
		size = qApp->font().pixelSize();
		type = "px";
		if (size == -1) {
			size = qApp->font().pointSizeF();
			type = "pt";
		}
	}

	if (type == "pt")
		res.setPointSizeF(size);
	else
		res.setPixelSize((int)size);

	return QVariant::fromValue(res);
}
QByteArray FontParser::toString(const QVariant& v)
{
	QFont f = v.value<QFont>();

	QByteArray res;

	if (f.style() == QFont::StyleNormal)
		res = "normal";
	else if (f.style() == QFont::StyleItalic)
		res = "italic";
	else 
		res = "oblique";

	res += " ";

	switch (f.weight()) {
		case QFont::Thin:
			res += "thin";
			break;
		case QFont::ExtraLight:
			res += "extralight";
			break;
		case QFont::Light:
			res += "light";
			break;
		case QFont::Normal:
			res += "normal";
			break;
		case QFont::Medium:
			res += "medium";
			break;
		case QFont::DemiBold:
			res += "demibold";
			break;
		case QFont::Bold:
			res += "bold";
			break;
		case QFont::ExtraBold:
			res += "extrabold";
			break;
		case QFont::Black:
			res += "black";
			break;
		default:
			res += "normal";
			break;
	}

	res += " ";

	if (f.pixelSize() == -1)
		res += QByteArray::number(f.pointSizeF()) + "pt";
	else
		res += QByteArray::number(f.pixelSize()) + "px";

	res += " \"";
	res += f.family().toLatin1();
	res += "\"";

	return res;
}

QVariant ColorParser::parse(const QByteArray & ar)
{
	bool ok;
	int parse_end;
	QColor c = parseColor(ar, parse_end, &ok);
	return ok ? QVariant::fromValue(c) : QVariant();
}
QByteArray ColorParser::toString(const QVariant& v)
{
	QColor c = v.value<QColor>();
	return colorToString(c);
}

QVariant EnumParser::parse(const QByteArray & ar)
{
	bool ok;
	int val = parseEnum(ar, enums, &ok);
	return ok ? QVariant::fromValue(val) : QVariant();
}
QByteArray EnumParser::toString(const QVariant& v)
{
	int val = v.toInt();
	for (auto it = enums.begin(); it != enums.end(); ++it) {
		if (it.value() == val)
			return it.key();
	}
	return QByteArray();
}


QVariant EnumOrStringParser::parse(const QByteArray& ar) 
{
	bool ok;
	int val = parseEnum(ar, enums, &ok);
	if (ok)
		return QVariant::fromValue(val);
	QByteArray res = ar;
	removeQuote(res);
	return QVariant(res);
}
QByteArray EnumOrStringParser::toString(const QVariant& v)
{
	if (v.userType() == QMetaType::QString || v.userType() == QMetaType::QByteArray) {
		char quote = '"';
		const QByteArray text = v.toByteArray();
		if (text.contains(quote))
			quote = '\'';
		return quote + text + quote;
	}
	int val = v.toInt();
	for (auto it = enums.begin(); it != enums.end(); ++it) {
		if (it.value() == val)
			return it.key();
	}
	return QByteArray();
}


QVariant EnumOrParser::parse(const QByteArray & ar)
{
	bool ok;
	int val = parseOrEnum(ar, enums, &ok);
	return ok ? QVariant::fromValue(val) : QVariant();
}
QByteArray EnumOrParser::toString(const QVariant& v)
{
	QStringList lst;
	int val = v.toInt();
	for (auto it = enums.begin(); it != enums.end(); ++it) {
		if (it.value() & val)
			lst.push_back(it.key());
	}
	return lst.join("|").toLatin1();
}


QVariant DoubleParser::parse(const QByteArray & ar)
{
	double res;
	QTextStream stream(ar);
	if ((stream >> res).status() == QTextStream::Ok)
		return QVariant::fromValue(res);
	return QVariant();
}
QByteArray DoubleParser::toString(const QVariant& v) 
{
	return v.toByteArray();
}

QVariant BoolParser::parse(const QByteArray & ar)
{
	int s, e;
	if (!cleanLine(ar, s, e)) return QVariant();
	QByteArray tmp = ar.mid(s, s - e);
	if (tmp == "true") return QVariant::fromValue(true);
	else if (tmp == "false") return QVariant::fromValue(false);
	else {
		int res;
		QTextStream stream(ar);
		if ((stream >> res).status() == QTextStream::Ok)
			return QVariant::fromValue((bool)res);
		return QVariant();
	}
}
QByteArray BoolParser::toString(const QVariant& v)
{
	return v.toBool() ? "true" : "false";
}

QVariant TextParser::parse(const QByteArray & ar)
{
	bool ok;
	QString res = parseText(ar, &ok);
	return ok ? QVariant::fromValue(res) : QVariant();
}
QByteArray TextParser::toString(const QVariant& v)
{
	// format: 'text' [text_color] [background_color] [border_color]
	char quote = '"';
	const QByteArray text = v.toByteArray();
	if (text.contains(quote))
		quote = '\'';
	QByteArray res = quote + text + quote;
	return res;
}




QVector<VipParserPtr> AnyParser::parsers() {
	static  QVector<VipParserPtr> p;
	if (p.isEmpty()) {
		p.push_back(VipParserPtr(new PenParser()));
		p.push_back(VipParserPtr(new ColorParser()));
		p.push_back(VipParserPtr(new DoubleParser()));
		p.push_back(VipParserPtr(new BoolParser()));
		p.push_back(VipParserPtr(new TextParser()));
	}
	return p;
}
QVariant AnyParser::parse(const QByteArray & ar)
{
	const QVector<VipParserPtr> par = parsers();
	for (int i = 0; i < par.size(); ++i) {
		QVariant tmp = par[i]->parse(ar);
		if (tmp.userType() != 0)
			return tmp;
	}
	return QVariant();
}
QByteArray AnyParser::toString(const QVariant& ) 
{
	return QByteArray();
}



struct ClassKeyWords
{
	VipKeyWords keywords;
	const QMetaObject* meta;
};


static QMap<QByteArray, ClassKeyWords>& _keyWordsForClass()
{
	static QMap<QByteArray, ClassKeyWords> instance;
	return instance;
}

static QMap<const QMetaObject*, ClassKeyWords>& _delayedRegistered() 
{
	static QMap<const QMetaObject*, ClassKeyWords> inst;
	return inst;
}

static void registerDelayed() 
{
	auto it = _delayedRegistered().begin();
	for (; it != _delayedRegistered().end();) {
		if (it.key()->className()) {
			_keyWordsForClass()[it.key()->className()] = it.value();
			it = _delayedRegistered().erase(it);
		}
		else
			++it;
	}
	
}



void vipRegisterMetaObject(const QMetaObject* metaclass) 
{
	// Register metaclass
	// To be sure that a style sheet of type "MyClass{}" will work even if MyClass does not define specific keywords
	// This function is called in VipPainItem::setStyleSheet()

	if (_keyWordsForClass().find(metaclass->className()) != _keyWordsForClass().end())
		return;
	_keyWordsForClass()[metaclass->className()] = ClassKeyWords{ VipKeyWords(), metaclass };
	metaclass = metaclass->superClass();
	while (metaclass) {
		if (_keyWordsForClass().find(metaclass->className()) != _keyWordsForClass().end())
			return;
		_keyWordsForClass()[metaclass->className()] = ClassKeyWords{ VipKeyWords(), metaclass };
		metaclass = metaclass->superClass();
	}
}




static VipKeyWords addKeyWords(const VipKeyWords& current, const VipKeyWords& new_keys)
{
	VipKeyWords res = current;
	for (VipKeyWords::const_iterator it = new_keys.begin(); it != new_keys.end(); ++it)
		res.insert(it.key(), it.value());
	return res;
}

VipKeyWords vipKeyWordsForClass(const char * classname, bool *ok)
{
	registerDelayed();

	VipKeyWords res;

	auto it = _keyWordsForClass().find(classname);
	if (it == _keyWordsForClass().end()) {
		// unkown class name
		if (ok)
			*ok = false;
		return res;
	}

	if (ok)
		*ok = true;

	const QMetaObject* meta = it.value().meta;
	QList<QByteArray> classnames;
	while (meta) {
		classnames.push_front(meta->className());
		meta = meta->superClass();
	}

	for (auto& name : classnames) {
		auto found = _keyWordsForClass().find(name);
		if (found != _keyWordsForClass().end())
			res = addKeyWords(res, found.value().keywords);
	}
	return res;
}

bool vipSetKeyWordsForClass(const QMetaObject* metaclass, const VipKeyWords& keywords)
{
	_delayedRegistered()[metaclass] = ClassKeyWords{ keywords, metaclass }; 
	return true;
}



#undef ERROR
#define ERROR(str) {if(error) *error = (str); return VipStyleSheet();}


static void addParseValue(VipParseResult & res, ParseValue p)
{
	VipParseResult::iterator it = res.find(p.name());
	if (it != res.end()) {
		it->addValues(p.values());
	}
	else {
		res[p.name()] = p;
	}
}

static void addParseValue(VipParseResult& res, const VipParseResult& src) 
{
	for (auto it = src.begin(); it != src.end(); ++it)
		addParseValue(res, it.value());
}




static VipKeyWords keywordsForItem(VipPaintItem * item)
{
	registerDelayed();

	if (!item)
		return VipKeyWords();

	const QMetaObject * meta = item->graphicsObject()->metaObject();

	while (meta) {
		if (_keyWordsForClass().find(meta->className()) != _keyWordsForClass().end())
			return vipKeyWordsForClass(meta->className());
		
		meta = meta->superClass();
	}
	return VipKeyWords();
}





QVariant VipStyleSheet::findProperty(const QByteArray& classname, const QByteArray& property_name, const QByteArray& index, const QByteArray& selectors) const
{ 
	QSet<QByteArray> sels = vipToSet(selectors.split(':'));
	// remove empty
	for (auto it = sels.begin(); it != sels.end();) {
		if (it->isEmpty())
			it = sels.erase(it);
		else
			++it;
	}

	auto it = find(classname);
	if (it == end())
		return QVariant();

	const VipClassStates& states = it.value();
	for (const VipClassState& state : states) {
		if (state.selectors==sels) {
		
			auto found = state.parseResults.find(property_name);
			if (found == state.parseResults.end())
				return QVariant();
			const QMap<QByteArray, QVariant>& values = found.value().values();
			auto found_index = values.find(index);
			if (found_index == values.end())
				return QVariant();
			return found_index.value();
		}	
	}
	return QVariant();
}


bool VipStyleSheet::setProperty(const QByteArray& classname, const QByteArray& property_name, const QVariant& value, const QByteArray& index, const QByteArray& selectors, bool all)
{
	const VipKeyWords keys = vipKeyWordsForClass(classname);
	auto it_key = keys.find(property_name);
	if (it_key == keys.end())
		return false;

	VipParserPtr parser = it_key.value();

	QSet<QByteArray> sels = vipToSet(selectors.split(':'));
	// remove empty
	for (auto it = sels.begin(); it != sels.end();) {
		if (it->isEmpty())
			it = sels.erase(it);
		else
			++it;
	}

	if (all && !sels.isEmpty())
		// forbidden
		return false;

	// find classname, create it if necessary
	auto it_class = find(classname);
	if (it_class == end()) {
		if (all)
			return false;
		it_class = this->insert(classname, VipClassStates());
	}

	// find state
	VipClassStates& states = it_class.value();
	QVector<VipClassState*> state ;
	if (!all) {

		for (VipClassState& st : states) {
			if (st.selectors == sels) {
				// found selector
				state.push_back( &st);
				break;
			}
		}
		// create state if necessary
		if (state.isEmpty()) {
			states.push_back(VipClassState{ sels, VipParseResult{} });
			state.push_back( &states.back());
		}
	}
	else {
		for (VipClassState& st : states)
			state.push_back(&st);
	}
	if (state.isEmpty())
		return false;

	for (int i = 0; i < state.size(); ++i) {
		VipClassState* st = state[i];
		auto it_prop = st->parseResults.find(property_name);
		if (it_prop == st->parseResults.end())
			st->parseResults.insert(property_name, ParseValue(property_name, value, index, parser));
		else
			it_prop.value().addValue(index, value);
	}
	return true;
}




static int indexOfChar(const QByteArray & ar, char c, int start) 
{
	// Find next index of c while skipping strings
	for (int i = start; i < ar.size(); ++i) {
	
		if (ar[i] == '"' || ar[i] == '\'') {

			int end = parseString(ar, i);
			if (end < 0)
				return -1;
			i = end;
		}
		else if (ar[i] == c)
			return i;
	}
	return -1;
}

static QList<QByteArray> splitOverString(const QByteArray& ar, char sep) 
{
	// split based on sep  while skipping strings
	QList<QByteArray> res;

	int last_start = 0;
	for (int i=0; i < ar.size(); ++i) {
		if (ar[i] == '"' || ar[i] == '\'') {

			int end = parseString(ar, i);
			if (end < 0)
				break;
			i = end;
		}
		else if (ar[i] == sep) {
			res.push_back(ar.mid(last_start, i - last_start));
			last_start = i + 1;
		}
	}
	res.push_back(ar.mid(last_start, ar.size() - last_start));
	return res;
}


VipStyleSheet vipParseStyleSheet(const QByteArray & ar, VipPaintItem * item, QString * error)
{
	QByteArray style_sheet = ar; //replace comments by spaces
	QByteArray reformated = ar; //replace comments by spaces, replace {} inside strings by spaces

	for (int i = 0; i < ar.size(); ++i) {
		if (ar[i] == '"' || ar[i] == (char)39) {
			//find full comment
			int end = parseString(ar, i);
			if (end < 0) ERROR("Unbalanced string starting at pos " + QString::number(i));
			//replace string content by spaces
			for (int j = i + 1; j < end; ++j) reformated[j] = ' ';
			i = end;
		}
		else if (ar[i] == '/' && i < ar.size() - 1 && ar[i + 1] == '*') {
			//find full comment
			int end = parseComment(ar, i);
			if(end < 0) ERROR("Unbalanced comment block starting at pos " + QString::number(i));
			//replace string content by spaces
			for (int j = i; j < end; ++j) {
				reformated[j] = ' ';
				style_sheet[j] = ' ';
			}
			i = end - 1;
		}
	}


	//find blocks (start with '{', ends with '}') and block names
	QMultiMap<QByteArray,QByteArray> blocks;

	int index = 0;
	while (true) {
		int start = indexOfChar(reformated,'{', index);
		if (start < 0)
			break;

		//we found a starting block, find the name before
		QByteArray name = style_sheet.mid(index, start - index);
		int s, e;
		if (!cleanLine(name, s, e)) ERROR("A block start ('{') should preceded by a class name at pos " + QString::number(start));
		name = name.mid(s, e - s);

		//find end of block
		int end = indexOfChar(reformated,'}', start + 1);
		if (end < 0) ERROR("Unbalanced block (missing '}') starting at pos " + QString::number(start));

		blocks.insert(name, style_sheet.mid(start + 1, end - start -1));
		index = end + 1;
	}

	
	//if no block, create a VipPlotItem (default) one
	if (blocks.isEmpty()) {
		blocks.insert("QObject", style_sheet);
	}
	

	VipStyleSheet res;

	VipKeyWords item_keywords = keywordsForItem(item);

	//decode each block
	for (QMultiMap<QByteArray, QByteArray>::const_iterator it = blocks.begin(); it != blocks.end(); ++it)
	{
		// selectors like 'hover', '!hover', 'selected'...
		QSet<QByteArray> selectors;

		QByteArray name = it.key();

		
		// handle inheritance selector (using '>')
		QList<QByteArray> split = name.split('>');
		if (split.size() > 2) {
			ERROR("Invalid selector >: " + cleanLine(name));
		}
		if (split.size() == 2) {
			// add '>parentClass' selector
			name = cleanLine(split[1]);
			selectors << '>' + cleanLine(split[0]);
		}

		// handle property selectors based on ':'
		split = name.split(':');
		name = split[0];
		for (int i = 1; i < split.size(); ++i) {
			QByteArray _ar = split[i];
			if (!_ar.isEmpty()) {
				
				selectors << _ar;
			}
		}

		//handle name selectors (using '#')
		split = name.split('#');
		if (split.size() > 2) {
			ERROR("Invalid selector #: " + cleanLine(name));
		}
		if (split.size() == 2) {
			// add '#objectName' selector
			name = split[0];
			selectors << '#' + split[1];
		}

		VipClassState state;
		state.selectors = selectors;


		const QByteArray block = it.value();

		bool class_found;
		VipKeyWords keywords = addKeyWords(item_keywords, vipKeyWordsForClass(name.data(), &class_found));
		if (keywords.isEmpty() || !class_found) 
			ERROR("Unknown block name: " + cleanLine(name));

		//split the block using semicolon
		QList<QByteArray> directives = splitOverString(block, ';');
		//parse each directive
		for (int i = 0; i < directives.size(); ++i) {

			//handle empty directive (possible and valid)
			if (cleanLine(directives[i]).isEmpty())
				continue;

			//split name/value using ':'
			QList<QByteArray> pair = splitOverString(directives[i],':');
			if (pair.size() != 2) ERROR("Syntax error: unbalanced ':'");

			//clean name
			int s, e;
			if(!cleanLine(pair[0],s,e)) ERROR("Syntax error: unbalanced ':'");
			QByteArray value_name = pair[0].mid(s, e - s);
			QByteArray clean = cleanKey(value_name);

			//check if name is an array ( 'name[0]' or 'name[1]' ... )
			int start = value_name.indexOf('[');
			QByteArray num = 0;
			if (start >= 0) {
				int end = value_name.indexOf(']', start + 1);
				if (end < start + 1) ERROR("unbalanced '[' in block name " + clean);
				num = value_name.mid(start + 1, end - start - 1);
				if(!removeQuote(num))
					ERROR("Wrong value format inside '[]'");
				value_name = value_name.mid(0, start);
			}

			ParseValue p;
			if (value_name.startsWith("qproperty-")) {
				//this a qt property, use AnyParser
				AnyParser parser;
				QVariant value = parser.parse(pair[1]);
				if (value.userType() == 0)
					ERROR("Unable to parse value of " + value_name + ", content is '" + pair[1] + "'");
				p = ParseValue(value_name, value, num, new AnyParser());
			}
			else {

				//find the parser
				VipKeyWords::const_iterator found = keywords.find(value_name);
				if (found == keywords.end())
					ERROR("Unknown key name: " + value_name);
				//parse
				QVariant value = found.value()->parse(pair[1]);
				if (value.userType() == 0) {
					ERROR("Unable to parse value of " + value_name + ", content is '" + pair[1] + "', parser is " + QByteArray(typeid(*found.value().get()).name()));
				}
				p = ParseValue(value_name, value, num, found.value());				
			}

			// add to result
			addParseValue(state.parseResults, p);

			
			//VipStyleSheet::iterator itr = res.find(name);
			//if (itr == res.end()) itr = res.insert(name, VipParseResult());
			//addParseValue(itr.value(), p);
		}

		// add to style sheet
		auto in_stylesheet = res.find(name);
		if (in_stylesheet != res.end()) {
			in_stylesheet.value() << state;
		}
		else {
			res.insert(name, VipClassStates() << state);
		}
			
	}

	if (error) error->clear();
	return res;
}



QByteArray vipStyleSheetToString(const VipStyleSheet & st) 
{
	QByteArray res;
	QTextStream str(&res, QIODevice::WriteOnly);

	if (st.size() == 1 && st.firstKey() == "QObject") {
		// special case: no class name
		Q_ASSERT(st.first().size() == 1);

		const VipClassState state = st.first().first();
		Q_ASSERT(state.selectors.size() == 0);
		for (auto it = state.parseResults.begin(); it != state.parseResults.end(); ++it) {
			if (!res.isEmpty())
				str << "; ";
			QByteArray property = it.key();
			VipParserPtr parser = it.value().parser();
			const QMap<QByteArray, QVariant> values = it.value().values();
			for (auto index = values.begin(); index != values.end(); ++index) {
				
				if (index.key().isEmpty())
					str << property <<  ": " << parser->toString(index.value()) << "; ";
				else
					str << property << "[" << index.key()<<"]: " << parser->toString(index.value()) << "; ";
			}
			
		}
		str.flush();
		str.reset();
		return res;
	}

	for (auto it = st.begin(); it != st.end(); ++it) {
		QByteArray classname = it.key();
		const VipClassStates states = it.value();

		for (const VipClassState& state : states) {
			
			// build class name + selectors
			QByteArray full_class_name = classname;
			if (!state.selectors.isEmpty())
				full_class_name += ":" + state.selectors.values().join(":");

			//write it
			str << full_class_name << "\n{\n";

			// write properties
			for (auto parse = state.parseResults.begin(); parse != state.parseResults.end(); ++parse) {
				
				QByteArray property = parse.key();
				VipParserPtr parser = parse.value().parser();
				const QMap<QByteArray, QVariant> values = parse.value().values();
				for (auto index = values.begin(); index != values.end(); ++index) {

					if (index.key().isEmpty())
						str << "\t"<< property << ": " << parser->toString(index.value()) << ";\n";
					else
						str << "\t" << property << "[" << index.key() << "]: " << parser->toString(index.value()) << ";\n";
				}
			}

			str << "}\n";
		}
	}

	str.flush();
	str.reset();
	return res;
}



#undef ERROR
#define ERROR(str) {if(error) *error = (str); return false;}

bool vipApplyStyleSheet(const VipStyleSheet & p, VipPaintItem * item, QString * error)
{
	if (p.isEmpty())
		return true;
	if (!item)
		ERROR("Null item");

	//grab the ParseResult that must be used
	std::deque<VipParseResult> to_use;

	const QMetaObject * meta = item->graphicsObject()->metaObject();
	while (meta) {
		VipStyleSheet::const_iterator it = p.find(meta->className());
		if (it != p.cend()) {
			const VipClassStates& states = it.value();
			std::vector<VipParseResult> class_results;
			for (const VipClassState& state : states) {
				if (item->hasStates(state.selectors))
					class_results.push_back(state.parseResults);
			}
			to_use.insert(to_use.begin(), class_results.begin(), class_results.end());
		}
		//	to_use << it.value();
		//if (strcmp(meta->className(), "VipPlotItem") == 0)
		//	break; //VipPlotItem is the bottom most class

		meta = meta->superClass();
	}

	//apply from the lower class to the higher one
	for (int i = 0; i < to_use.size(); ++i) {
		//get the VipParseResult
		const VipParseResult parse = to_use[i];
		for (VipParseResult::const_iterator it = parse.begin(); it != parse.end(); ++it) {
			//get all values (usually just one) for given property name
			const QMap<QByteArray, QVariant> & values = it.value().values();
			for(auto itv = values.begin(); itv != values.end(); ++itv)
				if (!item->setItemProperty(it.value().name().data(), itv.value(), itv.key())) {
					ERROR("Unable to set property " + it.value().name())
				}
		}
	}

	if (error) error->clear();
	return true;
}


VipStyleSheet vipExtractRelevantStyleSheetFor(const VipStyleSheet& p, VipPaintItem* item) 
{
	VipStyleSheet res;
	for (auto it = p.begin(); it != p.end(); ++it) {
		const QByteArray classname = it.key();
		if (classname == "QObject")
			continue;
		// find class name in item inheritance tree
		const QMetaObject* meta = item->graphicsObject()->metaObject();
		while (meta) {
			if (classname == meta->className()) {
				res.insert(classname, it.value());
				break;
			}
			meta = meta->superClass();
		}
	}
	return res;
}


VipStyleSheet vipMergeStyleSheet(const VipStyleSheet& src, const VipStyleSheet& additional) 
{
	VipStyleSheet res = src;
	for (auto it = additional.begin(); it != additional.end(); ++it) {
	
		const QByteArray classname = it.key();

		// ignore global style sheet without class name
		if (classname == "QObject")
			continue;

		auto found = res.find(classname);
		if (found == res.end())
			res[classname] = it.value();
		else {
			VipClassStates& dst_states = found.value();
			const VipClassStates &src_states = it.value();
			for (const VipClassState& state : src_states) {
				// find a similar state in dst
				bool added = false;
				for (int i = 0; i < dst_states.size(); ++i) {
					if (dst_states[i].selectors == state.selectors) {
						addParseValue( dst_states[i].parseResults, state.parseResults);
						added = true;
						break;
					}
				}
				if (!added)
					dst_states.push_back(state);
			}

		}
	}
	return res;
}


QList<QByteArray> vipClassNames(const QObject* object)
{
	QList<QByteArray> res;
	const QMetaObject* meta = object->metaObject();
	while (meta) {
		res.push_front(meta->className());
		meta = meta->superClass();
	}
	return res;
}

bool vipIsA(const QObject* object, const char* classname) 
{
	const QMetaObject* meta = object->metaObject();
	while (meta) {
		if (strcmp(meta->className(), classname) == 0)
			return true;
		meta = meta->superClass();
	}
	return false;
}







static int registerVipStyleSheet = qRegisterMetaType<VipStyleSheet>();


static quint64& _globalStyleSheetId()
{
	static quint64 inst = 0;
	return inst;
}
static VipStyleSheet& _globalStyleSheet()
{
	static VipStyleSheet inst ;
	return inst;
}
static QString& _globalStyleSheetString()
{
	static QString inst;
	return inst;
}

quint64 VipGlobalStyleSheet::styleSheetId() 
{
	return _globalStyleSheetId();
}

bool VipGlobalStyleSheet::setStyleSheet(const QString& str) 
{
	QString error;
	VipStyleSheet st = vipParseStyleSheet(str.toLatin1(), nullptr, &error);
	if (!error.isEmpty()) {
		qWarning(error.toLatin1().data());
		return false;
	}

	_globalStyleSheetString() = str;
	_globalStyleSheet() = st;
	_globalStyleSheetId()++;
	return true;
}
void VipGlobalStyleSheet::setStyleSheet(const VipStyleSheet& st)
{
	_globalStyleSheetString() = QString();
	_globalStyleSheet() = st;
	_globalStyleSheetId()++;
}

VipStyleSheet& VipGlobalStyleSheet::styleSheet() 
{
	_globalStyleSheetId()++;
	return _globalStyleSheet();
}
const VipStyleSheet& VipGlobalStyleSheet::cstyleSheet() 
{
	return _globalStyleSheet();
}
QString VipGlobalStyleSheet::styleSheetString() 
{
	return _globalStyleSheetString();
}
void VipGlobalStyleSheet::updateStyleSheetString() 
{
	_globalStyleSheetString() = vipStyleSheetToString(cstyleSheet());
}






#include "VipColorMap.h"
#include "VipSymbol.h"



void VipStandardStyleSheet::addTextStyleKeyWords(VipKeyWords& keywords, const QByteArray& prefix) 
{
	keywords[prefix + "font"] = VipParserPtr(new FontParser());
	keywords[prefix + "font-size"] = VipParserPtr(new DoubleParser());
	keywords[prefix + "font-style"] = VipParserPtr(new EnumParser(fontStyleEnum()));
	keywords[prefix + "font-weight"] = VipParserPtr(new EnumParser(fontWeightEnum()));
	keywords[prefix + "font-family"] = VipParserPtr(new TextParser());
	keywords[prefix + "color"] = VipParserPtr(new ColorParser());
	keywords[prefix + "text-background"] = VipParserPtr(new ColorParser());
	keywords[prefix + "text-border"] = VipParserPtr(new PenParser());
	keywords[prefix + "text-border-radius"] = VipParserPtr(new DoubleParser());
	keywords[prefix + "text-border-margin"] = VipParserPtr(new DoubleParser());
}

bool VipStandardStyleSheet::handleTextStyleKeyWord(const char* name, const QVariant& value, VipTextStyle& style, const QByteArray& prefix)
{
	if (name == prefix + "font") {
		style.setFont(value.value<QFont>());
		return true;
	}
	else if (name == prefix + "font-size") {
		QFont f = style.font();
		f.setPointSizeF(value.toDouble());
		style.setFont(f);
	}
	else if (name == prefix + "font-style") {
		QFont f = style.font();
		f.setStyle((QFont::Style)value.toInt());
		style.setFont(f);
	}
	else if (name == prefix + "font-weight") {
		QFont f = style.font();
		f.setWeight((QFont::Weight)value.toInt());
		style.setFont(f);
	}
	else if (name == prefix + "font-family") {
		QFont f = style.font();
		f.setFamily(value.toString());
		style.setFont(f);
	}
	else if (name == prefix + "color") {
		style.setTextPen(QPen(value.value<QColor>()));
	}
	else if (name == prefix + "text-background") {
		style.boxStyle().setBackgroundBrush(QBrush(value.value<QColor>()));
	}
	else if (name == prefix + "text-border") {
		style.boxStyle().setBorderPen(value.value<QPen>());
	}
	else if (name == prefix + "text-border-radius") {
		style.boxStyle().setBorderRadius(value.toDouble());
		style.boxStyle().setRoundedCorners(Vip::AllCorners);
	}
	else if (name == prefix + "text-border-margin") {
		style.setMargin(value.toDouble());
	}
	else
		return false;
	return true;
}


QMap<QByteArray, int> VipStandardStyleSheet::fontStyleEnum()
{
	static QMap<QByteArray, int> style;
	if (style.isEmpty()) {
		style["normal"] = QFont::StyleNormal;
		style["italic"] = QFont::StyleItalic;
		style["oblique"] = QFont::StyleOblique;
	}
	return style;
}

QMap<QByteArray, int> VipStandardStyleSheet::fontWeightEnum()
{
	static QMap<QByteArray, int> weight;
	if (weight.isEmpty()) {
		weight["thin"] = QFont::Thin;
		weight["extralight"] = QFont::ExtraLight;
		weight["light"] = QFont::Light;
		weight["normal"] = QFont::Normal;
		weight["medium"] = QFont::Medium;
		weight["demibold"] = QFont::DemiBold;
		weight["bold"] = QFont::Bold;
		weight["extrabold"] = QFont::ExtraBold;
		weight["black"] = QFont::Black;
	}
	return weight;
}

QMap<QByteArray, int> VipStandardStyleSheet::alignmentEnum()
{
	static QMap<QByteArray, int> alignment;
	if (alignment.isEmpty()) {

		alignment["left"] = Qt::AlignLeft;
		alignment["right"] = Qt::AlignRight;
		alignment["top"] = Qt::AlignTop;
		alignment["bottom"] = Qt::AlignBottom;
		alignment["hcenter"] = Qt::AlignHCenter;
		alignment["vcenter"] = Qt::AlignVCenter;
		alignment["center"] = Qt::AlignCenter;
	}
	return alignment;
}


QMap<QByteArray, int> VipStandardStyleSheet::orientationEnum()
{
	QMap<QByteArray, int> orientation;
	if (orientation.isEmpty()) {
		orientation["vertical"] = Qt::Vertical;
		orientation["horizontal"] = Qt::Horizontal;
	}
	return orientation;
}

QMap<QByteArray, int> VipStandardStyleSheet::regionPositionEnum()
{
	QMap<QByteArray, int> textPosition;
	if (textPosition.isEmpty()) {

		textPosition["outside"] = Vip::Outside;
		textPosition["xinside"] = Vip::XInside;
		textPosition["yinside"] = Vip::YInside;
		textPosition["inside"] = Vip::Inside;
		textPosition["xautomatic"] = Vip::XAutomatic;
		textPosition["yautomatic"] = Vip::YAutomatic;
		textPosition["automatic"] = Vip::Automatic;
	}
	return textPosition;
}

QMap<QByteArray, int> VipStandardStyleSheet::colormapEnum()
{
	QMap<QByteArray, int> colorMap;
	if (colorMap.isEmpty()) {

		
		colorMap["autumn"] = VipLinearColorMap::Autumn;
		colorMap["bone"] = VipLinearColorMap::Bone;
		colorMap["burd"] = VipLinearColorMap::BuRd;
		colorMap["cool"] = VipLinearColorMap::Cool;
		colorMap["copper"] = VipLinearColorMap::Copper;
		colorMap["gray"] = VipLinearColorMap::Gray;
		colorMap["hot"] = VipLinearColorMap::Hot;
		colorMap["hsv"] = VipLinearColorMap::Hsv;
		colorMap["jet"] = VipLinearColorMap::Jet;
		colorMap["fusion"] = VipLinearColorMap::Fusion;
		colorMap["pink"] = VipLinearColorMap::Pink;
		colorMap["rainbow"] = VipLinearColorMap::Rainbow;
		colorMap["spring"] = VipLinearColorMap::Spring;
		colorMap["summer"] = VipLinearColorMap::Summer;
		colorMap["sunset"] = VipLinearColorMap::Sunset;
		colorMap["viridis"] = VipLinearColorMap::Viridis;
		colorMap["white"] = VipLinearColorMap::White;
		colorMap["winter"] = VipLinearColorMap::Winter;
	}
	return colorMap;
}

QMap<QByteArray, int> VipStandardStyleSheet::colorPaletteEnum()
{
	QMap<QByteArray, int> colorMap;
	if (colorMap.isEmpty()) {
		// Matplotlib color palettes
		colorMap["standard"] = VipLinearColorMap::ColorPaletteStandard;
		colorMap["random"] = VipLinearColorMap::ColorPaletteRandom;
		colorMap["pastel"] = VipLinearColorMap::ColorPalettePastel;
		colorMap["pastel1"] = VipLinearColorMap::ColorPalettePastel1;
		colorMap["pastel2"] = VipLinearColorMap::ColorPalettePastel2;
		colorMap["paired"] = VipLinearColorMap::ColorPalettePaired;
		colorMap["accent"] = VipLinearColorMap::ColorPaletteAccent;
		colorMap["dark2"] = VipLinearColorMap::ColorPaletteDark2;
		colorMap["set1"] = VipLinearColorMap::ColorPaletteSet1;
		colorMap["set2"] = VipLinearColorMap::ColorPaletteSet2;
		colorMap["set3"] = VipLinearColorMap::ColorPaletteSet3;
		colorMap["tab10"] = VipLinearColorMap::ColorPaletteTab10;
	}
	return colorMap;
}



QMap<QByteArray, int> VipStandardStyleSheet::symbolEnum()
{
	QMap<QByteArray, int> symbol;
	if (symbol.isEmpty()) {

		
		symbol["none"] = VipSymbol::None;
		symbol["ellipse"] = VipSymbol::Ellipse;
		symbol["rect"] = VipSymbol::Rect;
		symbol["diamond"] = VipSymbol::Diamond;
		symbol["triangle"] = VipSymbol::Triangle;
		symbol["dtriangle"] = VipSymbol::DTriangle;
		symbol["utriangle"] = VipSymbol::UTriangle;
		symbol["rtriangle"] = VipSymbol::LTriangle;
		symbol["ltriangle"] = VipSymbol::RTriangle;
		symbol["cross"] = VipSymbol::Cross;
		symbol["xcross"] = VipSymbol::XCross;
		symbol["hline"] = VipSymbol::HLine;
		symbol["vline"] = VipSymbol::VLine;
		symbol["star1"] = VipSymbol::Star1;
		symbol["star2"] = VipSymbol::Star2;
		symbol["hexagon"] = VipSymbol::Hexagon;
	}
	return symbol;
}