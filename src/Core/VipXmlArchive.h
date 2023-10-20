/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_XML_ARCHIVE_H
#define VIP_XML_ARCHIVE_H

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include "VipArchive.h"

/// \addtogroup Core
/// @{

/// Represents a list of symbols in a XML archive which can be edited through the same editor.
/// The symbols must be marked as editable inside the archive (see function vipEditableSymbol()).
class VIP_CORE_EXPORT VipEditableArchiveSymbol
{
public:
	/// Symbol name
	QString name;

	/// Some information about the symbols
	QString info;

	/// Location of the symbol inside the archive (of the form: Node_name#0/node_name#2/...).
	QString location;

	/// The symbol steel sheet, used to create the right widget for symbol edition (see QObjectFactory class for more details).
	/// The editing widget must provide the property 'value' in order to update the symbol's value inside the archive.
	QString style_sheet;

	/// The symbol default value
	QString default_value;

	/// VipSymbol id. Symbols with the same id will use the same widget to be edited.
	/// Default value is 0. A value of 0 means that this symbol cannot be edited.
	int id;

	// default ctor
	VipEditableArchiveSymbol(const QString& n = QString(), const QString& inf = QString(), const QString& loc = QString(), const QString& ss = QString(), const QString& df = QString(), int i = 0)
	  : name(n)
	  , info(inf)
	  , location(loc)
	  , style_sheet(ss)
	  , default_value(df)
	  , id(i)
	{
	}

	// copy ctor
	VipEditableArchiveSymbol(const VipEditableArchiveSymbol& right)
	  : name(right.name)
	  , info(right.info)
	  , location(right.location)
	  , style_sheet(right.style_sheet)
	  , default_value(right.default_value)
	  , id(right.id)
	{
	}

	// return a string representation of this object
	QString toString() const;
	// copy operator
	VipEditableArchiveSymbol& operator=(const VipEditableArchiveSymbol& right);
	// comparison operator. Equivalent to (name == right.name && id = right.id)
	bool operator==(const VipEditableArchiveSymbol& right) const;
	// convert a node to its string location
	static QString nodeToLocation(QDomNode node);
	// return the node using its location and the top element node.
	static QDomNode locationToNode(const QString& loc, QDomNode top_node);
};

/// @brief Base class for XML archives
///
/// VipXArchive is the base class for all XML based archives.
///
/// In addition to the default XML serialization features, it
/// provides 2 additional mechanisms:
/// -	Editable symbols through VipXArchive::editableSymbols() and VipXArchive::setEditableSymbols().
///		Editable symbols are XML nodes that can be modified through a widget editor.
///		They are marked with the attributes 'content_editable', 'style_sheet' and 'editable_id'.
///		See vipEditableSymbol() function for more details.
/// -	Automatic range detection (default to false). If enabled, the archive will
///		automatically emit the signals rangeUpdated(), valueUpdated() and textUpdated().
///		These signals could be connected to the corresponding slots of a VipProgress
///		to display the progress on archive reading.
///
struct VIP_CORE_EXPORT VipXArchive : public VipArchive
{
	VipXArchive();

	virtual ~VipXArchive();

	/// Returns the list of VipEditableArchiveSymbol in this archive.
	QList<VipEditableArchiveSymbol> editableSymbols(QDomNode node = QDomNode()) const;

	/// Set the list of VipEditableArchiveSymbol to use for this archive.
	/// This will modify the content of the archive.
	void setEditableSymbols(const QList<VipEditableArchiveSymbol>& lst);

	/// Save the current archive status (read mode and position)
	virtual void save();
	/// Reset the archive status. Each call to \a restore must match to a call to #save().
	virtual void restore();

	/// @brief Returns the current XML node
	QDomNode currentNode() const;
	/// @brief Returns the top level XML node
	QDomNode topNode() const;

	/// @brief Automatic range detection
	/// If enabled, the archive will automatically emit the signals rangeUpdated(), valueUpdated() and textUpdated().
	///	These signals could be connected to the corresponding slots of a VipProgress to display the progress on archive reading.
	void setAutoRangeEnabled(bool enable);
	bool autoRangeEnabled() const;

	/// Convenient function, set a QDomElement content (create it or replace it)
	static QDomNode setContent(QDomNode n, const QString& text, const QVariantMap& attributes = QVariantMap());

protected:
	virtual bool open(QDomNode n);
	QDomNode lastNode() const;
	void setCurrentNode(const QDomNode&);
	void setLastNode(const QDomNode&);
	bool hasChild(const QString& name) const;
	bool hasAttribute(const QString& name) const;
	bool hasContent() const;

	void check_node(const QDomNode& n, const QString& error);
	void set_current_value(const QDomNode& n);

private:
	void computeNodeList();
	class PrivateData;
	PrivateData* m_data;
};

/// Basic XML output archive used to write into a QDomNode
class VIP_CORE_EXPORT VipXOArchive : public VipXArchive
{
public:
	VipXOArchive();

protected:
	QDomElement add_child(QDomNode n, const QString& name);

	virtual void doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata);
	virtual void doStart(QString& name, QVariantMap& metadata, bool read_metadata);
	virtual void doEnd();
	virtual void doComment(QString& text);

	virtual bool open(QDomNode n);
};

/// Basic XML input archive used to read data from a QDomNode
class VIP_CORE_EXPORT VipXIArchive : public VipXArchive
{
public:
	VipXIArchive();

protected:
	virtual bool open(QDomNode n);
	virtual void doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata);
	virtual void doStart(QString& name, QVariantMap& metadata, bool read_metadata);
	virtual void doEnd();
	virtual void doComment(QString& text);
};

/// XML output archive providing an easy way to write into a buffer
class VIP_CORE_EXPORT VipXOStringArchive : public VipXOArchive
{
	QDomDocument doc;

public:
	VipXOStringArchive();
	QString toString();
	void reset();

protected:
	virtual bool open(QDomNode n);
};

/// XML output archive providing an easy way to write into a file
class VIP_CORE_EXPORT VipXOfArchive : public VipXOArchive
{
	QString file;
	QDomDocument doc;

public:
	VipXOfArchive(const QString& filename = QString());
	~VipXOfArchive();

	bool open(const QString& filename);
	void close();

protected:
	virtual bool open(QDomNode n);
};

/// XML input archive providing an easy way to read data from a buffer
class VIP_CORE_EXPORT VipXIStringArchive : public VipXIArchive
{
	QDomDocument doc;

public:
	VipXIStringArchive(const QString& buffer = QString());
	bool open(const QString& buffer);

protected:
	virtual bool open(QDomNode n);
};

/// XML input archive providing an easy way to read data from an XML file
class VIP_CORE_EXPORT VipXIfArchive : public VipXIArchive
{
	QDomDocument doc;

public:
	VipXIfArchive(const QString& filename = QString());
	bool open(const QString& filename);

protected:
	virtual bool open(QDomNode n);
};

/// @}
// end Core

#endif
