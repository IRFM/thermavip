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

#ifndef VIP_STYLE_SHEET_H
#define VIP_STYLE_SHEET_H

#include "VipConfig.h"

#include <qbytearray.h>
#include <qmap.h>
#include <qmetatype.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qvariant.h>
#include <qvector.h>

#define VIP_NOT_A_COLOR QColor(0, 0, 1, 1)

class VipPaintItem;
class VipTextStyle;

/// Base class for parsing a single property in a style sheet
struct Parser
{
	virtual ~Parser() {}
	virtual QVariant parse(const QByteArray&) = 0;
	virtual QByteArray toString(const QVariant&) = 0;
};
typedef QSharedPointer<Parser> VipParserPtr;

/// parse a QPen object
struct VIP_PLOTTING_EXPORT PenParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// parse a QBrush object
struct VIP_PLOTTING_EXPORT BrushParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse a QColor object
struct VIP_PLOTTING_EXPORT ColorParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse a QFont object
struct VIP_PLOTTING_EXPORT FontParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse an enum value
struct VIP_PLOTTING_EXPORT EnumParser : Parser
{
	QMap<QByteArray, int> enums;
	EnumParser(const QMap<QByteArray, int>& enums)
	  : enums(enums)
	{
	}
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse an enum value
struct VIP_PLOTTING_EXPORT EnumOrStringParser : Parser
{
	QMap<QByteArray, int> enums;
	EnumOrStringParser(const QMap<QByteArray, int>& enums)
	  : enums(enums)
	{
	}
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse a combination of enum values with OR separator.
struct VIP_PLOTTING_EXPORT EnumOrParser : Parser
{
	QMap<QByteArray, int> enums;
	EnumOrParser(const QMap<QByteArray, int>& enums)
	  : enums(enums)
	{
	}
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse a floating point value
struct VIP_PLOTTING_EXPORT DoubleParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse a boolean value
struct VIP_PLOTTING_EXPORT BoolParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse a VipText object
struct VIP_PLOTTING_EXPORT TextParser : Parser
{
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};
/// Parse any kind of value (QColor, QPen, float value, boolean value or VipText object)
struct VIP_PLOTTING_EXPORT AnyParser : Parser
{
	static QVector<VipParserPtr> parsers();
	virtual QVariant parse(const QByteArray& ar);
	virtual QByteArray toString(const QVariant&);
};

class ParseValue
{
	QByteArray m_name;
	QMap<QByteArray, QVariant> m_values;
	VipParserPtr m_parser;

public:
	ParseValue() {}
	ParseValue(const QByteArray& name, const QMap<QByteArray, QVariant>& values, const VipParserPtr& parser)
	  : m_name(name)
	  , m_values(values)
	  , m_parser(parser)
	{
	}
	ParseValue(const QByteArray& name, const QMap<QByteArray, QVariant>& values, Parser* parser)
	  : m_name(name)
	  , m_values(values)
	  , m_parser(parser)
	{
	}
	ParseValue(const QByteArray& name, const QVariant& value, const QByteArray& index, const VipParserPtr& parser)
	  : m_name(name)
	  , m_parser(parser)
	{
		m_values[index] = value;
	}
	ParseValue(const QByteArray& name, const QVariant& value, const QByteArray& index, Parser* parser)
	  : m_name(name)
	  , m_parser(parser)
	{
		m_values[index] = value;
	}

	bool isNull() const { return m_values.isEmpty() || m_values.first().userType() == 0; }
	QByteArray name() const { return m_name; }
	QVariant value() const { return m_values.isEmpty() ? QVariant() : m_values.first(); }
	const QMap<QByteArray, QVariant>& values() const { return m_values; }

	void setParser(const VipParserPtr& p) { m_parser = p; }
	void setParser(Parser* p) { m_parser.reset(p); }
	VipParserPtr parser() const { return m_parser; }

	void addValue(const QByteArray& index, const QVariant& value) { m_values[index] = value; }
	void addValues(const QMap<QByteArray, QVariant>& values)
	{
		// merge values
		for (QMap<QByteArray, QVariant>::const_iterator it = values.begin(); it != values.end(); ++it)
			m_values[it.key()] = it.value();
	}

	bool operator==(const ParseValue& other) const { return m_name == other.m_name && m_values == other.m_values; }
	bool operator!=(const ParseValue& other) const { return !((*this) == other); }
};

/// @brief Map of Key word -> parser
using VipKeyWords = QMap<QByteArray, VipParserPtr>;
/// @brief Map of Key word -> parsed value
using VipParseResult = QMap<QByteArray, ParseValue>;

/// @brief Combine a list of states ('hover', 'selected', 'left', '!right'...)
/// and associated parsed keywords
struct VipClassState
{
	QSet<QByteArray> selectors;
	VipParseResult parseResults;

	bool operator==(const VipClassState& other) const { return selectors == other.selectors && parseResults == other.parseResults; }
	bool operator!=(const VipClassState& other) const { return selectors == other.selectors && parseResults == other.parseResults; }
};

/// @brief Gather for one class all VipParseResult with their states
using VipClassStates = QVector<VipClassState>;

/// @brief Map of Class name -> VipClassStates
class VIP_PLOTTING_EXPORT VipStyleSheet : public QMap<QByteArray, VipClassStates>
{
	using base_type = QMap<QByteArray, VipClassStates>;

	base_type& toMap() { return *this; }
	const base_type& toMap() const { return *this; }

public:
	VipStyleSheet()
	  : base_type()
	{
	}
	VipStyleSheet(const VipStyleSheet& other)
	  : base_type(other)
	{
	}
	VipStyleSheet(VipStyleSheet&& other)
	  : base_type(std::move(other.toMap()))
	{
	}
	VipStyleSheet& operator=(const VipStyleSheet& other)
	{
		toMap() = other.toMap();
		return *this;
	}
	VipStyleSheet& operator=(VipStyleSheet&& other)
	{
		toMap() = std::move(other.toMap());
		return *this;
	}
	bool operator==(const VipStyleSheet& other) const { return toMap() == other.toMap(); }
	bool operator!=(const VipStyleSheet& other) const { return toMap() != other.toMap(); }

	/// @brief Helper function, returns property value for given class, property name, index and optional class selectors
	/// @param classname class name, like "VipPlotCurve"
	/// @param property_name property name, like "background"
	/// @param index optional property index for properties that support it
	/// @param selectors optional class selectors for classes that support it (with ':' separator)
	/// @return property value if found, null QVariant if not
	QVariant findProperty(const QByteArray& classname, const QByteArray& property_name, const QByteArray& index = QByteArray(), const QByteArray& selectors = QByteArray()) const;

	/// @brief Helper function, set property for given class, property name, optional index and optional selectors
	/// @param classname class name, like "VipPlotCurve"
	/// @param property_name property name, like "background"
	/// @param value value to set
	/// @param index optional property index for properties that support it
	/// @param selectors optional class selectors for classes that support it (with ':' separator)
	/// @param all if selectors is empty and all is true, set the property to all available class selectors in the style sheet, but do not create new ones.
	/// @return true if at least one property was set, false otherwise
	bool setProperty(const QByteArray& classname,
			 const QByteArray& property_name,
			 const QVariant& value,
			 const QByteArray& index = QByteArray(),
			 const QByteArray& selectors = QByteArray(),
			 bool all = false);
};

Q_DECLARE_METATYPE(VipStyleSheet)

/// @brief Register a QMetaObject in order for the corresponding type to work with stylesheets, even if it does not provide additional keywords
/// Does not need to be called if vipSetKeyWordsForClass() was called for this meta object.
/// This function must be called AFTER static object initialization
VIP_PLOTTING_EXPORT void vipRegisterMetaObject(const QMetaObject* metaclass);

/// @brief Returns all key words a given class is sensible to
/// @param classname class name
/// @param ok if not null, set to true on success, false otherwise
/// @return key words
VIP_PLOTTING_EXPORT VipKeyWords vipKeyWordsForClass(const char* classname, bool* ok = nullptr);

/// @brief Register keywords for given QMetaObject
/// This function can be called with empty keywords just to register a class (like vipRegisterMetaObject()).
/// Its advantage is that it can be called during static objects initialization.
/// @param metaclass QMetaObject pointer
/// @param keywords new keywords (will override previous ones, if any)
VIP_PLOTTING_EXPORT bool vipSetKeyWordsForClass(const QMetaObject* metaclass, const VipKeyWords& keywords = VipKeyWords());

/// @brief Returns the inheritance list for object, starting from QObject up to the last class
VIP_PLOTTING_EXPORT QList<QByteArray> vipClassNames(const QObject* object);
/// @brief Returns true if object is a (or inherit) classname
VIP_PLOTTING_EXPORT bool vipIsA(const QObject* object, const char* classname);

/// @brief Parse a style sheet and returns a VipStyleSheet object.
/// The style sheet is not applied.
/// @param ar style sheet to parse
/// @param item optional item to which the style sheet will be applied, only meaningful if the style sheet does not contain a class name
/// @param error if not null, set it to the error string in case of syntax error
/// @return VipStyleSheet object
VIP_PLOTTING_EXPORT VipStyleSheet vipParseStyleSheet(const QByteArray& ar, VipPaintItem* item, QString* error);

/// @brief Apply a style sheet to a VipPaintItem
/// @param st input VipStyleSheet
/// @param item item to apply the style sheet to
/// @param error output error, might be null
/// @return true on success, false otherwise
VIP_PLOTTING_EXPORT bool vipApplyStyleSheet(const VipStyleSheet& st, VipPaintItem* item, QString* error);

/// @brief Convert a style sheet object to string
/// @param st input VipStyleSheet
/// @return string representation of input style sheet
VIP_PLOTTING_EXPORT QByteArray vipStyleSheetToString(const VipStyleSheet& st);

/// @brief Merge 2 style sheets
/// @param src first style sheet
/// @param additional second style sheet, might override properties in src
/// @return merged style sheet
VIP_PLOTTING_EXPORT VipStyleSheet vipMergeStyleSheet(const VipStyleSheet& src, const VipStyleSheet& additional);

/// @brief VipGlobalStyleSheet manages the global application style sheet used by all plotting items.
///
/// VipGlobalStyleSheet is used to define an application wide style sheet for all plotting items.
/// It is the equivalent for plotting item of qApp->setStyleSheet() for widgets.
///
/// VipGlobalStyleSheet should be used to define application skins.
///
class VIP_PLOTTING_EXPORT VipGlobalStyleSheet
{
	friend class VipPaintItem;
	static quint64 styleSheetId();

public:
	/// @brief Set the global style sheet.
	/// All VipPainItem objects will use it at their next paint() calls.
	/// @param str style sheet string
	/// @return true on success, false on parsing error
	static bool setStyleSheet(const QString& str);
	/// @brief Set the global style sheet.
	/// All VipPainItem objects will use it at their next paint() calls.
	/// @param st style sheet object
	static void setStyleSheet(const VipStyleSheet& st);
	/// @brief Returns the global style sheet.
	/// Note that this version triggers a reapplication of the global style sheet for all VipPainItem objects, as opposed to cstyleSheet().
	/// @return global style sheet object
	static VipStyleSheet& styleSheet();
	/// @brief Returns the global style sheet.
	/// @return global style sheet object
	static const VipStyleSheet& cstyleSheet();
	/// @brief Returns the global style sheet string
	static QString styleSheetString();
	/// @brief Update the global style sheet string based on the global VipStyleSheet object.
	/// Used to reflect changes made with VipGlobalStyleSheet::styleSheet().setProperty() to the global style sheet string.
	static void updateStyleSheetString();
};

/// @brief Define some standard enum types for style sheets, to be used with EnumParse or EnumOrParser
class VIP_PLOTTING_EXPORT VipStandardStyleSheet
{
public:
	/// @brief Helper function, add to VipKeyWords all key words needed to setup a text style
	///
	/// This will add the keywords:
	/// -	'font': full font definition
	/// -	'font-size': font size in points
	/// -	'font-style': font style, one of 'normal', 'italic' or 'oblique'
	/// -	'font-weight': font weight, one of 'thin', 'extralight', 'light', 'normal', 'medium', 'demibold', 'bold', 'extrabold', 'black'
	/// -	'font-family': font family
	/// -	'text-color': text color
	/// -	'text-background': text box background color
	/// -	'text-border': text box border pen
	/// -	'text-border-radius': text box border radius
	/// -	'text-border-margin': margin between text and text box border
	///
	/// If prefix is not empty, it will be added to the prebious key words.
	/// For instance, if prefix is 'inner-', this function defines the key words 'inner-font', 'inner-font-size',...
	///
	/// @param keywords in/out keywords
	/// @param prefix optional prefix
	static void addTextStyleKeyWords(VipKeyWords& keywords, const QByteArray& prefix = QByteArray());

	/// @brief Handle text style key word, use in VipPaintItem::setItemProperty()
	/// @param name key name
	/// @param prefix prefix to be added to the key
	/// @param style in/out text style
	/// @return true if one key was successfully set, false if no key was found or on error.
	static bool handleTextStyleKeyWord(const char* name, const QVariant& value, VipTextStyle& style, const QByteArray& prefix = QByteArray());

	/// @brief Returns possible font style enum values, convertible to QFont::Style
	/// Possible values: 'normal', 'italic', 'oblique'
	static QMap<QByteArray, int> fontStyleEnum();

	/// @brief Returns possible font weight enum values, convertible to QFont::Weight
	/// Possible values: 'thin', 'extralight', 'light', 'normal', 'medium', 'demibold', 'bold', 'extrablack', 'black'
	static QMap<QByteArray, int> fontWeightEnum();

	/// @brief Returns possible alignment enum values, convertible to Qt::Alignment
	/// Possible values are: 'left', 'right', 'top', 'bottom', 'center', vcenter', 'hcenter'
	static QMap<QByteArray, int> alignmentEnum();

	/// @brief Returns possible orientation enum values, convertible to Qt::Orientations
	/// Possible values are: 'vertical', 'horizontal'
	static QMap<QByteArray, int> orientationEnum();

	/// @brief Returns possible region position enum values, convertible to Vip::RegionPositions
	/// Possible values are: 'outside', 'xinside', 'yinside', 'inside', 'xautomatic', 'yautomatic', 'automatic'
	static QMap<QByteArray, int> regionPositionEnum();

	/// @brief Returns possible symbol enum values, convertible to VipSymbol::Style
	/// Possible values are: 'none', 'ellipse', 'rect', 'diamond', 'triangle', 'dtriangle', 'utriangle', 'rtriangle', 'ltriangle', 'cross', 'xcross', 'hline', 'vline', 'star1', 'star2', 'hexagon'
	static QMap<QByteArray, int> symbolEnum();

	/// @brief Returns possible colormap enum values, convertible to VipLinearColorMap::StandardColorMap
	/// Possible values are: 'autumn', 'bone', 'burd', 'cool', 'copper', 'gray', 'hot', 'hsv', 'jet', 'fusion', 'pink', 'rainbow', 'spring', 'summer', 'sunset', 'white', 'winter'
	static QMap<QByteArray, int> colormapEnum();

	/// @brief Returns possible color palette enum values, convertible to VipLinearColorMap::StandardColorMap
	/// Possible values are: 'standard', 'random', 'pastel', 'pastel1', 'pastel2', 'paired', 'accent', 'dark2', 'set1', 'set2', 'set3', 'tab10'
	static QMap<QByteArray, int> colorPaletteEnum();
};

#endif