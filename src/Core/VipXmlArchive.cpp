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

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QtXml/QDomCDATASection>

#include "VipCore.h"
#include "VipXmlArchive.h"

static bool toByteArray(const QVariant& v, QByteArray& array)
{
	if (v.userType() == QMetaType::QByteArray) {
		array = v.value<QByteArray>().toBase64();
		return true;
	}
	else if (v.canConvert<QString>() && v.userType() != QMetaType::QStringList) {
		array = v.toString().toLatin1();
		return true;
	}
	else if (v.userType() == qMetaTypeId<QVariantMap>()) {
		QByteArray res;
		QDataStream stream(&res, QIODevice::WriteOnly);
		vipSafeVariantMapSave(stream, v.value<QVariantMap>());
		array = res.toBase64();
		return array.size() > 0;
	}
	else {
		QByteArray res;
		QDataStream stream(&res, QIODevice::WriteOnly);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		if (!QMetaType::save(stream, v.userType(), v.data()))
#else
		if (!QMetaType(v.userType()).save(stream, v.data())) // stream << v;
#endif
			return false;
		array = res.toBase64();
		return array.size() > 0;
	}
}

static bool fromByteArray(const QByteArray& array, QVariant& v)
{
	if (v.userType() == QMetaType::QByteArray) {
		v = QVariant::fromValue(QByteArray::fromBase64(array));
		return true;
	}
	else if (v.canConvert<QString>() && v.userType() != QMetaType::QStringList) {
		int type = v.userType();
		v = QVariant(QString(array));
		return v.convert(VIP_META(type));
	}
	else {
		QDataStream stream(QByteArray::fromBase64(array));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		QMetaType::load(stream, v.userType(), v.data());
#else
		QMetaType(v.userType()).load(stream, v.data());
#endif
		return v.isValid();
	}
}

static void maxLineNumber(const QDomElement& node, int& count)
{
	if (!node.isNull()) {
		count = qMax(count, node.lineNumber());
		QDomElement child = node.firstChildElement();
		maxLineNumber(child, count);
		while (true) {
			QDomElement next = child.nextSiblingElement();
			if (next.isNull())
				break;

			maxLineNumber(next, count);
			child = next;
		}
	}
}

QString VipEditableArchiveSymbol::toString() const
{
	QString res = "name : " + name + "\n";
	res += "info : " + info + "\n";
	res += "style_sheet : " + style_sheet + "\n";
	res += "default_value : " + default_value + "\n";
	res += "location : " + location;
	return res;
}

VipEditableArchiveSymbol& VipEditableArchiveSymbol::operator=(const VipEditableArchiveSymbol& right)
{
	name = right.name;
	id = right.id;
	info = right.info;
	location = right.location;
	style_sheet = right.style_sheet;
	default_value = right.default_value;
	return *this;
}

bool VipEditableArchiveSymbol::operator==(const VipEditableArchiveSymbol& right) const
{
	return name == right.name && id == right.id;
}

QString VipEditableArchiveSymbol::nodeToLocation(QDomNode n)
{
	QString res;

	QDomNode node = n;
	while (!node.isNull() && !node.isDocument()) {
		QString name = node.toElement().tagName();
		QDomNode parent = node.parentNode();

		if (parent.isNull() || parent.isDocument()) {
			// top of the tree, return the resulting location
			return "/" + name + "#0" + res;
		}
		else {
			// find the children index
			QDomNodeList child = parent.toElement().elementsByTagName(name);
			for (int i = 0; i < child.size(); ++i) {
				if (child.at(i) == node) {
					name += "#" + QString::number(i);
					res = "/" + name + res;
					break;
				}
			}
		}

		node = parent;
	}

	return res;
}

QDomNode VipEditableArchiveSymbol::locationToNode(const QString& loc, QDomNode top_node)
{
	QStringList lst = loc.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.size() < 1)
		return QDomNode();

	QDomNode node = top_node;

	for (int i = 1; i < lst.size(); ++i) {
		QStringList temp = lst[i].split("#", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		if (temp.size() != 2)
			return QDomNode();

		QString name = temp[0];
		int index = temp[1].toLong();
		QDomNodeList child = node.toElement().elementsByTagName(name);
		if (index >= child.size())
			return QDomNode();

		node = child.at(index);
	}

	return node;
}

// void serialize_EditableArchiveSymbol(VipEditableArchiveSymbol & symbol, VipArchive & stream, unsigned int version)
// {
// stream.Content("name",symbol.name);
// stream.Content("info",symbol.info);
// stream.Content("style_sheet",symbol.style_sheet);
// stream.Content("id",symbol.id);
// }
//
// REGISTER_DYNAMIC_SERIALIZE_TYPE(VipEditableArchiveSymbol,"VipEditableArchiveSymbol",0,serialize_EditableArchiveSymbol)
//

class VipXArchive::PrivateData
{
public:
	struct Parameters
	{
		QDomNode node;
		QDomNode last_node;
		QDomElement top_node;
		bool auto_range;
		int max_lines;
	};

	Parameters parameters;
	QList<Parameters> saved;
};

VipXArchive::VipXArchive()
  : VipArchive(Text, MetaDataOnContent | MetaDataOnNodeStart | Comment)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->parameters.auto_range = false;
	d_data->parameters.max_lines = 0;
}

VipXArchive::~VipXArchive()
{
}

void VipXArchive::setAutoRangeEnabled(bool enable)
{
	d_data->parameters.auto_range = enable;
	if (enable && isOpen()) {
		computeNodeList();
	}
}

bool VipXArchive::autoRangeEnabled() const
{
	return d_data->parameters.auto_range;
}

void VipXArchive::computeNodeList()
{
	// create the full node list, and set the range and current value for this archive progress
	d_data->parameters.max_lines = 0;

	maxLineNumber(d_data->parameters.node.ownerDocument().firstChildElement(), d_data->parameters.max_lines);
	setRange(0, d_data->parameters.max_lines);

	if (!d_data->parameters.node.toElement().isNull()) {
		setValue(d_data->parameters.node.lineNumber());
	}
}

bool VipXArchive::open(QDomNode n)
{
	d_data->parameters.node = n;
	d_data->parameters.last_node = n;
	d_data->parameters.top_node = n.toElement();
	if (!n.isNull()) {
		if (autoRangeEnabled()) {
			computeNodeList();
		}
		return true;
	}
	return false;
}

void VipXArchive::doSave()
{
	d_data->saved.append(d_data->parameters);
}

void VipXArchive::doRestore()
{
	if (d_data->saved.size()) {
		d_data->parameters = d_data->saved.back();
		d_data->saved.pop_back();
	}
}

QDomNode VipXArchive::topNode() const
{
	QDomNode node = d_data->parameters.node;
	if (node.isDocument())
		return node.toDocument().documentElement();

	while (!node.isNull() && !node.parentNode().isNull() && !node.parentNode().isDocument()) {
		node = node.parentNode();
	}

	return node;
	// return currentNode().ownerDocument();
}

QDomNode VipXArchive::currentNode() const
{
	return d_data->parameters.node;
}

QDomNode VipXArchive::lastNode() const
{
	return d_data->parameters.last_node;
}

void VipXArchive::setCurrentNode(const QDomNode& current)
{
	d_data->parameters.node = current;
}

void VipXArchive::setLastNode(const QDomNode& last)
{
	d_data->parameters.last_node = last;
}

bool VipXArchive::hasChild(const QString& name) const
{
	return !d_data->parameters.node.firstChildElement(name).isNull();
}

bool VipXArchive::hasAttribute(const QString& name) const
{
	return d_data->parameters.node.toElement().hasAttribute(name);
}

bool VipXArchive::hasContent() const
{
	return (d_data->parameters.node.toElement().text().length() > 0);
}

QList<VipEditableArchiveSymbol> VipXArchive::editableSymbols(QDomNode node) const
{
	if (node.isNull() && !topNode().isNull())
		return editableSymbols(topNode());

	if (node.isNull())
		return QList<VipEditableArchiveSymbol>();

	// walk through the node to find the editable symbol
	QList<VipEditableArchiveSymbol> res;

	QDomElement elem = node.toElement();
	if (elem.hasAttribute("content_editable") && elem.hasAttribute("style_sheet")) {
		res.append(VipEditableArchiveSymbol(elem.tagName(),
						    elem.attribute("content_editable"),
						    VipEditableArchiveSymbol::nodeToLocation(node),
						    elem.attribute("style_sheet"),
						    elem.text(),
						    elem.attribute("editable_id").toLong()));
	}

	QDomNodeList children = node.childNodes();
	for (int i = 0; i < children.size(); ++i)
		res.append(editableSymbols(children.at(i)));

	return res;
}

void VipXArchive::setEditableSymbols(const QList<VipEditableArchiveSymbol>& lst)
{
	for (int i = 0; i < lst.size(); ++i) {
		QDomNode node = VipEditableArchiveSymbol::locationToNode(lst[i].location, topNode());
		if (!node.isNull()) {
			node.toElement().setAttribute("content_editable", lst[i].info);
			node.toElement().setAttribute("style_sheet", lst[i].style_sheet);
			node.toElement().setAttribute("editable_id", QString::number(lst[i].id));

			QVariantMap map;
			setContent(node, lst[i].default_value, map);
		}
	}
}

void VipXArchive::check_node(const QDomNode& n, const QString& error)
{
	if (n.isNull())
		setError(error, -1);
}

void VipXArchive::set_current_value(const QDomNode& n)
{
	if (autoRangeEnabled() && !n.isNull() && d_data->parameters.max_lines) {
		setValue(n.lineNumber());
	}
}

// QList<SerializeObject> VipXArchive::GetAttributeList() const
// {
// QList<SerializeObject> res;
// if(!currentNode().isNull())
// {
// QDomNamedNodeMap attr = currentNode().attributes();
// for(int i=0; i< attr.count(); ++i)
// {
//	res.append( SerializeObject(attr.item(i).nodeName(), attr.item(i).nodeValue()) );
// }
// }
//
// return res;
// }
//
// QList<SerializeObject> VipXArchive::GetDataList() const
// {
// QList<SerializeObject> res;
// if(!currentNode().isNull())
// {
// QDomElement child =	currentNode().firstChildElement ();
// while(!child.isNull())
// {
//	if(child.firstChildElement ().isNull())
//	{
//		res.append( SerializeObject(child.nodeName(),child.nodeValue()) );
//
//		//check if editable
//		QDomNamedNodeMap attr = child.attributes();
//		QString edit_comment = attr.namedItem("content_editable").nodeValue();
//		QString edit_style_sheet = attr.namedItem("style_sheet").nodeValue();
//		bool editable = !edit_style_sheet.isEmpty() && ! edit_comment.isEmpty();
//		res.last().editable = editable;
//		res.last().edit_comment = edit_comment;
//		res.last().edit_style_sheet = edit_style_sheet;
//
//	}
//
//	child = child.nextSiblingElement();
// }
//
// }
//
// return res;
// }
//
// QStringList VipXArchive::GetNodeList() const
// {
// QStringList res;
// if(!currentNode().isNull())
// {
// QDomElement child =	currentNode().firstChildElement ();
// while(!child.isNull())
// {
//	if(!child.firstChildElement ().isNull())
//	{
//		res.append(child.nodeName());
//	}
//
//	child = child.nextSiblingElement();
// }
//
// }
//
// return res;
// }

QDomNode VipXArchive::setContent(QDomNode n, const QString& text, const QVariantMap& map)
{
	QDomElement node = n.toElement();
	if (node.isNull())
		return QDomNode();

	QDomNode res;

	if (node.text().isEmpty())
		res = node.appendChild(node.ownerDocument().createTextNode(text));
	else {
		// find the text child and replace it
		QDomNodeList child = node.childNodes();
		for (int c = 0; c < child.size(); ++c) {
			if (child.at(c).isText()) {
				res = node.replaceChild(node.ownerDocument().createTextNode(text), child.at(c));
				break;
			}
		}
	}

	// set the attributes
	for (QVariantMap::const_iterator it = map.begin(); it != map.end(); ++it) {
		QByteArray ar;
		toByteArray(it.value(), ar);
		node.setAttribute(it.key(), QString(ar));
	}

	return node;
}

VipXOArchive::VipXOArchive() {}

bool VipXOArchive::open(QDomNode n)
{
	if (!VipXArchive::open(n)) {
		setMode(NotOpen);
		setError("Invalid node", -1);
		return false;
	}
	else {
		setMode(Write);
		return true;
	}
}

QDomElement VipXOArchive::add_child(QDomNode n, const QString& name)
{
	QDomElement child = n.ownerDocument().createElement(name);
	n.appendChild(child);
	return child;
}

// void VipXOArchive::doEditable(const QString & comment, const QString & style_sheet)
// {
// attribute("content_editable",comment);
// attribute("style_sheet",style_sheet);
// attribute("editable_id",0);
// }

void VipXOArchive::doContent(QString& name, QVariant& value, QVariantMap& metadata, bool)
{
	if (name.isEmpty())
		name = "object";

	bool serialized = false;
	if (value.userType() >= QMetaType::User) {
		// use the serialize dispatcher
		VipFunctionDispatcher<2>::function_list_type lst = serializeFunctions(value);
		if (lst.size()) {
			// call all serialize functions in a new node
			if (name.isEmpty())
				name = "object";
			setLastNode(setContent(add_child(currentNode(), name), QString(), metadata));
			lastNode().toElement().setAttribute("type_name", VipAny(value).type().name);
			QDomNode node = lastNode();
			setCurrentNode(lastNode());

			for (int i = 0; i < lst.size(); ++i) {
				lst[i](value, this);
				if (hasError())
					break;
			}

			serialized = true;
			setCurrentNode(node.parentNode());
			setLastNode(node);
		}
	}

	if (!serialized) {
		QByteArray ar;
		toByteArray(value, ar);
		// create a node with the given value
		setLastNode(setContent(add_child(currentNode(), name), QString(ar), metadata));
		lastNode().toElement().setAttribute("type_name", VipAny(value).type().name);
		check_node(currentNode(), "Invalid XML currentNode(): unable to write content " + name);
	}

	if (!hasError()) {
		// write attributes
		for (QVariantMap::const_iterator it = metadata.begin(); it != metadata.end(); ++it) {
			lastNode().toElement().setAttribute(it.key(), it.value().toString());
		}
	}
}

void VipXOArchive::doStart(QString& name, QVariantMap& metadata, bool)
{
	QDomElement child;

	child = add_child(currentNode(), name); // nodeName());
	// child.setAttribute(attributeName(),name);

	check_node(child, "Invalid XML currentNode(): unable to Start currentNode() " + name);
	if (!hasError()) {
		setCurrentNode(child);
		for (QVariantMap::const_iterator it = metadata.begin(); it != metadata.end(); ++it)
			currentNode().toElement().setAttribute(it.key(), it.value().toString());
	}
}

void VipXOArchive::doEnd()
{
	QDomNode parent = currentNode().parentNode();
	check_node(parent, "Invalid XML currentNode(): unable to End currentNode()");
	if (!hasError()) {
		setLastNode(currentNode());
		setCurrentNode(parent);
	}
}

void VipXOArchive::doComment(QString& data)
{
	QDomCDATASection node = currentNode().ownerDocument().createCDATASection(data);
	currentNode().appendChild(node);
	setLastNode(node);
}

VipXIArchive::VipXIArchive()
  : VipXArchive()
{
	setMode(NotOpen);
}

bool VipXIArchive::open(QDomNode n)
{
	if (!VipXArchive::open(n)) {
		setMode(NotOpen);
		setError("Invalid node");
		return false;
	}
	else {
		setMode(Read);
		return true;
	}
}

void VipXIArchive::doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata)
{

	QDomElement node;

	// find the right node with given name, or the first available one if an empty name is provided
	if (!name.isEmpty()) {
		if ((lastNode().parentNode() != currentNode()))
			node = currentNode().firstChildElement(name);
		else {
			node = lastNode().nextSiblingElement();
			while (!node.isNull() && node.tagName() != name)
				node = node.nextSiblingElement();
		}
	}
	else {
		if ((lastNode().parentNode() != currentNode()))
			node = currentNode().firstChildElement(name);
		else
			node = lastNode().nextSiblingElement();
		name = node.nodeName();
	}

	check_node(node, "Invalid XML currentNode(): unable to read content " + name);
	if (hasError())
		return;

	set_current_value(node);

	// initialize the variant if necessary

	QString type_name = node.attribute("type_name");

	// special case: empty variant
	if (type_name.isEmpty()) {
		setLastNode(node);
		setCurrentNode(lastNode().parentNode());
		value.clear();
		return;
	}

	if (!value.isValid()) {
		value = vipCreateVariant(type_name.toLatin1().data());
		if (!value.isValid() || ((QMetaType(value.userType()).flags() & QMetaType::PointerToQObject) && value.value<QObject*>() == nullptr)) {
			setError("Cannot create QVariant value with type name ='" + type_name + "'");
			return;
		}
	}

	// use deserialize dispatcher
	setCurrentNode(node);
	setLastNode(node);

	bool serialized = false;
	if (value.userType() >= QMetaType::User) {
		VipFunctionDispatcher<2>::function_list_type lst = deserializeFunctions(value);
		if (lst.size()) {
			for (int i = 0; i < lst.size(); ++i) {
				value = lst[i](value, this);
				if (hasError())
					break;
			}

			serialized = true;
		}
	}

	if (!serialized) {
		if (!fromByteArray(node.text().toLatin1(), value))
			setError("Cannot create QVariant value with type name ='" + type_name + "'");
	}

	setLastNode(node);
	setCurrentNode(lastNode().parentNode());

	// read attributes
	if (read_metadata && !hasError()) {
		QDomNamedNodeMap map = lastNode().attributes();
		for (int i = 0; i < map.size(); ++i) {
			QDomAttr attr = map.item(i).toAttr();
			metadata[attr.name()] = attr.value();
		}
	}
}

void VipXIArchive::doStart(QString& name, QVariantMap& metadata, bool read_metadata)
{
	QDomElement node;

	// if name is empty, Start the Next node, fill name with the node's name
	if (name.isEmpty()) {
		// find the right node
		if ((lastNode().parentNode() != currentNode()))
			node = currentNode().firstChildElement();
		else {
			node = lastNode().nextSiblingElement();
		}

		const_cast<QString&>(name) = node.tagName();
	}
	else // find the first node with the right name
	{
		if ((lastNode().parentNode() != currentNode()))
			node = currentNode().firstChildElement(name);
		else {
			node = lastNode().nextSiblingElement();
			while (!node.isNull() && node.tagName() != name)
				node = node.nextSiblingElement();
		}
	}

	if (node.isNull()) {
		setError("Invalid XML node: unable to Start node " + name);
	}
	else {
		setCurrentNode(node);

		// read attributes
		if (read_metadata) {
			QDomNamedNodeMap map = currentNode().attributes();
			for (int i = 0; i < map.size(); ++i) {
				QDomAttr attr = map.item(i).toAttr();
				metadata[attr.name()] = attr.value();
			}
		}
	}
}

void VipXIArchive::doEnd()
{
	QDomNode parent = currentNode().parentNode();
	check_node(parent, "Unable to End currentNode()");
	if (!hasError()) {
		setLastNode(currentNode());
		setCurrentNode(parent);
	}
}

void VipXIArchive::doComment(QString& data)
{
	// find the right node
	QDomNode node;
	if ((lastNode().parentNode() != currentNode()))
		node = currentNode().firstChild();
	else
		node = lastNode().nextSibling();

	// find the Next CDATA section
	while (!node.isNull() && !node.isCDATASection())
		node = node.nextSibling();

	check_node(node, "Invalid XML currentNode(): unable to find CDATA section");
	if (!hasError()) {
		setLastNode(node);
		data = node.toCDATASection().data();
	}
}

VipXOStringArchive::VipXOStringArchive()
  : VipXOArchive()
  , doc(QString())
{
	setCurrentNode(doc);
	setMode(Write);
}

QString VipXOStringArchive::toString()
{
	return doc.toString();
}

void VipXOStringArchive::reset()
{
	resetError();
	setCurrentNode(doc = QDomDocument(QString()));
	setLastNode(QDomNode());
}

bool VipXOStringArchive::open(QDomNode n)
{
	doc = QDomDocument();
	setCurrentNode(n);
	setLastNode(n);
	if (n.isDocument())
		return !(doc = n.toDocument()).isNull();
	else
		return !doc.appendChild(n).isNull();
}

VipXOfArchive::VipXOfArchive(const QString& filename)
  : VipXOArchive()
{
	if (!filename.isEmpty())
		open(filename);
}

VipXOfArchive::~VipXOfArchive()
{
	close();
}

bool VipXOfArchive::open(const QString& filename)
{
	close();

	QFile fout(filename);
	if (!fout.open(QIODevice::WriteOnly | QIODevice::Text)) {
		setError("Unable to open file: " + filename);
		return false;
	}

	setMode(Write);
	setCurrentNode(doc = QDomDocument(QString()));
	file = filename;
	return true;
}

void VipXOfArchive::close()
{
	resetError();
	setMode(NotOpen);
	QString filename = file;
	file.clear();
	if (!currentNode().isNull()) {
		QFile fout(filename);
		if (!fout.open(QIODevice::WriteOnly | QIODevice::Text)) {
			setMode(NotOpen);
			setCurrentNode(QDomNode());
			setError("Unable to open file: " + filename);
			return;
		}
		QString temp(doc.toString());
		fout.write(temp.toLatin1().data(), temp.length());
		fout.close();
	}
	setCurrentNode(doc = QDomDocument());
}

bool VipXOfArchive::open(QDomNode n)
{
	doc = QDomDocument(QString());
	setCurrentNode(n);
	setLastNode(n);
	if (n.isDocument())
		return !(doc = n.toDocument()).isNull();
	else
		return !doc.appendChild(n).isNull();
}

VipXIStringArchive::VipXIStringArchive(const QString& buffer)
  : VipXIArchive()
{
	if (!buffer.isEmpty()) {
		open(buffer);
	}
}

bool VipXIStringArchive::open(const QString& buffer)
{
	setMode(NotOpen);
	if (!doc.setContent(buffer))
		return false;
	if (open(doc)) {
		setMode(Read);
		return true;
	}
	return false;
}

bool VipXIStringArchive::open(QDomNode n)
{
	// doc = QDomDocument("");
	setCurrentNode(n);
	setLastNode(n);
	if (n.isDocument())
		return !(doc = n.toDocument()).isNull();
	else
		return !doc.appendChild(n).isNull();
}

VipXIfArchive::VipXIfArchive(const QString& filename)
  : VipXIArchive()
{
	if (!filename.isEmpty())
		open(filename);
}

bool VipXIfArchive::open(const QString& filename)
{
	resetError();
	setMode(NotOpen);
	setCurrentNode(QDomNode());

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)) {
		setError("Unable to open file: " + filename);
		return false;
	}

	QString error;
	int errorLine, errorCol;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QDomDocument::ParseResult r = doc.setContent(&file);
	if (!r) {
		error = r.errorMessage;
		errorLine = r.errorLine;
		errorCol = r.errorColumn;
		setError(QString::asprintf("error at line %d, col %d:\n%s\n", errorLine, errorCol, error.toLatin1().data()));
		vip_debug("error at line %d, col %d:\n%s\n", errorLine, errorCol, error.toLatin1().data());
		file.close();
		return false;
	}
#else

	if (!doc.setContent(&file, &error, &errorLine, &errorCol)) {
		setError(QString::asprintf("error at line %d, col %d:\n%s\n", errorLine, errorCol, error.toLatin1().data()));
		vip_debug("error at line %d, col %d:\n%s\n", errorLine, errorCol, error.toLatin1().data());
		file.close();
		return false;
	}
#endif
	file.close();
	if (!doc.isNull()) {
		setMode(Read);
		setCurrentNode(doc);
		setLastNode(doc);
		return true;
	}
	else {
		return false;
	}
}

bool VipXIfArchive::open(QDomNode n)
{
	doc = QDomDocument("");
	setCurrentNode(n);
	setLastNode(n);
	if (n.isDocument())
		return !(doc = n.toDocument()).isNull();
	else
		return !doc.appendChild(n).isNull();
}
