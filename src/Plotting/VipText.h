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

#ifndef VIP_TEXT_H
#define VIP_TEXT_H

#include <QBrush>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QRegExp>
#include <QSharedDataPointer>
#include <QString>

#include "VipBoxStyle.h"

#if defined(Q_CC_MSVC) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

/// \addtogroup Plotting
/// @{

/// \brief Abstract base class for rendering text strings
///
/// A text engine is responsible for rendering texts for a
/// specific text format. They are used by VipText to render a text.
///
/// VipPlainTextEngine and VipRichTextEngine are part of the  library.
/// The implementation of MathMLTextEngine uses code from the
/// Qt solution package. Because of license implications it is built into
/// a separate library.
///
/// \sa VipText::setTextEngine()

class VIP_PLOTTING_EXPORT VipTextEngine
{
public:
	virtual ~VipTextEngine();

	/// Find the height for a given width
	///
	/// \param font Font of the text
	/// \param flags Bitwise OR of the flags used like in QPainter::drawText
	/// \param text VipText to be rendered
	/// \param width Width
	///
	/// \return Calculated height
	virtual double heightForWidth(const QFont& font, int flags, const QString& text, double width) const = 0;

	/// Returns the size, that is needed to render text
	///
	/// \param font Font of the text
	/// \param flags Bitwise OR of the flags like in for QPainter::drawText
	/// \param text VipText to be rendered
	///
	/// \return Calculated size
	virtual QSizeF textSize(const QFont& font, int flags, const QString& text) const = 0;

	/// Test if a string can be rendered by this text engine
	///
	/// \param text VipText to be tested
	/// \return true, if it can be rendered
	virtual bool mightRender(const QString& text) const = 0;

	/// Return margins around the texts
	///
	/// The textSize might include margins around the
	/// text, like QFontMetrics::descent(). In situations
	/// where texts need to be aligned in detail, knowing
	/// these margins might improve the layout calculations.
	///
	/// \param font Font of the text
	/// \param text VipText to be rendered
	/// \param left Return value for the left margin
	/// \param right Return value for the right margin
	/// \param top Return value for the top margin
	/// \param bottom Return value for the bottom margin
	virtual void textMargins(const QFont& font, const QString& text, double& left, double& right, double& top, double& bottom) const = 0;

	/// Draw the text in a clipping rectangle
	///
	/// \param painter VipPainter
	/// \param rect Clipping rectangle
	/// \param flags Bitwise OR of the flags like in for QPainter::drawText()
	/// \param text VipText to be rendered
	virtual void draw(QPainter* painter, const QRectF& rect, int flags, const QString& text) const = 0;

protected:
	VipTextEngine();
};

/// \brief A text engine for plain texts
///
/// VipPlainTextEngine renders texts using the basic Qt classes
/// QPainter and QFontMetrics.
class VIP_PLOTTING_EXPORT VipPlainTextEngine : public VipTextEngine
{
public:
	VipPlainTextEngine();
	virtual ~VipPlainTextEngine();

	virtual double heightForWidth(const QFont& font, int flags, const QString& text, double width) const;

	virtual QSizeF textSize(const QFont& font, int flags, const QString& text) const;

	virtual void draw(QPainter* painter, const QRectF& rect, int flags, const QString& text) const;

	virtual bool mightRender(const QString&) const;

	virtual void textMargins(const QFont&, const QString&, double& left, double& right, double& top, double& bottom) const;

private:
	class PrivateData;
	PrivateData* d_data;
};

#ifndef QT_NO_RICHTEXT

/// \brief A text engine for Qt rich texts
///
/// VipRichTextEngine renders Qt rich texts using the classes
/// of the Scribe framework of Qt.
class VIP_PLOTTING_EXPORT VipRichTextEngine : public VipTextEngine
{
public:
	VipRichTextEngine();

	virtual double heightForWidth(const QFont& font, int flags, const QString& text, double width) const;

	virtual QSizeF textSize(const QFont& font, int flags, const QString& text) const;

	virtual void draw(QPainter* painter, const QRectF& rect, int flags, const QString& text) const;

	virtual bool mightRender(const QString&) const;

	virtual void textMargins(const QFont&, const QString&, double& left, double& right, double& top, double& bottom) const;

private:
	QString taggedText(const QString&, int flags) const;
};

#endif

class VIP_PLOTTING_EXPORT VipTextStyle //: public QSharedData
{
public:
	VipTextStyle();
	VipTextStyle(const VipTextStyle& other);

	VipTextStyle& operator=(const VipTextStyle& other);

	void setFont(const QFont&);
	const QFont& font() const;
	QFont& font();

	void setCached(bool);
	bool cached() const;

	void setTextPen(const QPen&);
	const QPen& textPen() const;
	QPen& textPen();

	void setBorderPen(const QPen&);
	QPen borderPen() const;

	void setBackgroundBrush(const QBrush&);
	QBrush backgroundBrush() const;

	double borderRadius() const;
	void setBorderRadius(double);

	void setAlignment(Qt::Alignment align);
	Qt::Alignment alignment() const;

	void setRenderHints(QPainter::RenderHints);
	QPainter::RenderHints renderHints() const;

	void setMargin(double);
	double margin() const;

	void setBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& boxStyle() const;
	VipBoxStyle& boxStyle();

	void setTextBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& textBoxStyle() const;
	VipBoxStyle& textBoxStyle();
	bool hasTextBoxStyle() const;

	bool operator==(const VipTextStyle& other) const;
	bool operator!=(const VipTextStyle& other) const;

private:
	class VIP_PLOTTING_EXPORT PrivateData : public QSharedData
	{
	public:
		PrivateData();
		PrivateData(const PrivateData& other);
		~PrivateData();

		bool cached;
		double margin;
		QFont font;
		QPen textPen;
		VipBoxStyle boxStyle;
		VipBoxStyle* textBoxStyle;
		Qt::Alignment alignment;
		QPainter::RenderHints renderHints;
	};

	QSharedDataPointer<PrivateData> d_data;
};

/// \brief A class representing a text
///
/// A VipText is a text including a set of attributes how to render it.
///
/// - Format\n
/// A text might include control sequences (f.e tags) describing
/// how to render it. Each format (f.e MathML, TeX, Qt Rich VipText)
/// has its own set of control sequences, that can be handles by
/// a special VipTextEngine for this format.
/// - Background\n
/// A text might have a background, defined by a QPen and QBrush
/// to improve its visibility. The corners of the background might
/// be rounded.
/// - Font\n
/// A text might have an individual font.
/// - Color\n
/// A text might have an individual color.
/// - Render Flags\n
/// Flags from Qt::AlignmentFlag and Qt::TextFlag used like in
/// QPainter::drawText().
///
/// \sa VipTextEngine, TextLabel

class VIP_PLOTTING_EXPORT VipText
{
public:
	/// \brief VipText format
	///
	/// The text format defines the VipTextEngine, that is used to render
	/// the text.
	///
	/// \sa VipTextEngine, setTextEngine()
	enum TextFormat
	{
		/// The text format is determined using VipTextEngine::mightRender() for
		/// all available text engines in increasing order > PlainText.
		/// If none of the text engines can render the text is rendered
		/// like VipText::PlainText.
		AutoText = 0,

		//! Draw the text as it is, using a VipPlainTextEngine.
		PlainText,

		//! Use the Scribe framework (Qt Rich VipText) to render the text.
		RichText,

		/// The number of text formats can be extended using setTextEngine.
		/// Formats >= VipText::OtherFormat are not used by .
		OtherFormat = 100
	};

	/// \brief VipText direction
	///
	/// The VipText direction defines the text direction when drawing text inside a VipPie.
	///
	/// \sa draw()
	enum TextDirection
	{
		/// The bottom part of the text points to the center of the VipPie.
		TowardInside,

		/// The bottom part of the text points to the outside of the VipPie.
		TowardOutside,

		/// Let the drawing function choose the right direction to ensure the best readability.
		AutoDirection
	};

	/// \brief Layout Attributes
	/// The layout attributes affects some aspects of the layout of the text.
	enum LayoutAttribute
	{
		/// Layout the text without its margins. This mode is useful if a
		/// text needs to be aligned accurately, like the tick labels of a scale.
		/// If VipTextEngine::textMargins is not implemented for the format
		/// of the text, MinimumLayout has no effect.
		MinimumLayout = 0x01
	};

	//! Layout attributes
	typedef QFlags<LayoutAttribute> LayoutAttributes;

	VipText(const QString& = QString(), TextFormat textFormat = AutoText);
	VipText(const QString&, const VipTextStyle&, TextFormat textFormat = AutoText);
	VipText(const char* str)
	  : VipText(QString(str))
	{
	}
	VipText(const VipText&);

	VipText& operator=(const VipText&);

	bool operator==(const VipText&) const;
	bool operator!=(const VipText&) const;

	void setText(const QString&, VipText::TextFormat textFormat = AutoText);
	const QString& text() const;
	QString& text();

	bool isNull() const;
	bool isEmpty() const;

	VipText& setCached(bool);
	bool cached() const;

	/// @brief Activate potential text caching application wide.
	///
	/// By default, VipText draws its text using a cache pixmap only if cached() is true.
	/// If cacheTextWhenPossible() is true, caching will be activated on some scenarios
	/// (opengl backend or text rotation) even if  cached() is false.
	///
	static void setCacheTextWhenPossible(bool);
	static bool cacheTextWhenPossible();

	VipText& setFont(const QFont&);
	const QFont& font() const;
	QFont& font();

	VipText& setTextPen(const QPen&);
	const QPen& textPen() const;
	QPen& textPen();

	VipText& setBorderPen(const QPen&);
	QPen borderPen() const;

	VipText& setBackgroundBrush(const QBrush&);
	QBrush backgroundBrush() const;

	double borderRadius() const;
	VipText& setBorderRadius(double);

	VipText& setAlignment(Qt::Alignment flags);
	Qt::Alignment alignment() const;

	VipText& setRenderHints(QPainter::RenderHints);
	QPainter::RenderHints renderHints() const;

	VipText& setBoxStyle(const VipBoxStyle& p);
	const VipBoxStyle& boxStyle() const;
	VipBoxStyle& boxStyle();

	VipText& setTextBoxStyle(const VipBoxStyle& p);
	const VipBoxStyle& textBoxStyle() const;
	VipBoxStyle& textBoxStyle();
	bool hasTextBoxStyle() const;

	VipText& setTextStyle(const VipTextStyle& p);
	const VipTextStyle& textStyle() const;
	VipTextStyle& textStyle();

	VipText& setLayoutAttribute(LayoutAttribute, bool on = true);
	VipText& setLayoutAttributes(LayoutAttributes attrs);
	bool testLayoutAttribute(LayoutAttribute) const;
	LayoutAttributes layoutAttributes() const;

	double heightForWidth(double width) const;
	QSizeF textSize() const;
	QRectF textRect() const { return QRectF(QPointF(0, 0), textSize()); }

	template<class... Args>
	VipText& sprintf(Args... parameters)
	{
		setText(QString::asprintf(text().toLatin1().data(), std::forward<Args>(parameters)...));
		return *this;
	}

	/// @brief Replace occurrences of text 'str' by given arithmetic value.
	/// Note that this text might contain the string 'str' directly followed by a formatting specifier on the form '%...' (printf syntax).
	/// In this case, the formatting specifier is also replaced and used to format 'value'.
	/// @tparam T arithmetic value type
	/// @param str string to replace
	/// @param value arithmetic value
	/// @return reference to this modified VipText object
	template<class T>
	VipText& replace(const QString& str, const T& value);
	VipText& replace(const QString& str, const QString& value, bool possible_numeric = false);

	template<class T>
	static QString replace(const QString& input, const QString& str, const T& value);
	static QString replace(const QString& input, const QString& str, const QString& value, bool possible_numeric = false);

	/// Replace the  content inside the tags '<span repeat=ntimes></span> \a ntimes
	/// while replacing all occurences of '%i' by the current repetition number.
	VipText& repeatBlock();
	static QString repeatBlock(const QString input);

	const VipTextEngine* textEngine() const;

	void draw(QPainter* painter, const QPointF& topLeft) const;
	void draw(QPainter* painter, const QRectF& rect) const;
	void draw(QPainter* painter, const QPointF& center, const VipPie& pie, TextDirection dir = AutoDirection) const;

	static const VipTextEngine* textEngine(const QString& text, VipText::TextFormat = AutoText);

	static const VipTextEngine* textEngine(VipText::TextFormat);
	static void setTextEngine(VipText::TextFormat, VipTextEngine*);

	QPainterPath textAsPath() const;

private:
	VipText(const QString&, const VipTextEngine* engine);

	class PrivateData : public QSharedData
	{
	public:
		PrivateData();
		PrivateData(const PrivateData& other);

		QString text;
		VipTextStyle parameters;
		LayoutAttributes layoutAttributes;
		const VipTextEngine* textEngine;
	};

	QSharedDataPointer<PrivateData> d_data;

	bool d_dirtyTextSize;
	QSizeF d_textSize;
	QPixmap d_cash;
};

//! \return text().isNull()
inline bool VipText::isNull() const
{
	return text().isNull();
}

//! \return text().isEmpty()
inline bool VipText::isEmpty() const
{
	return text().isEmpty();
}

static QString formattingSequence(const QString& str, int start)
{
	static const QRegExp exp(QString("[diuoxXfFeEgGaAcspn%]"));
	if (start >= str.size())
		return QString();

	if (str[start] == QChar('%')) {
		int end = exp.indexIn(str, start + 1);
		if (end >= 0)
			return str.mid(start, end - start + 1);
	}

	return QString();
}
// template<class T>
// void vipReplaceFormattedText(QString & text, const QString & to_replace, const T & value)
// {
// int index = text.indexOf(to_replace);
// while (index >= 0)
// {
// QString format = formattingSequence(text, index + to_replace.size());
// QString replace;
//
// if (format.isEmpty())
// {
//	replace = QString("%1").arg(value);
// }
// else if (format == "%%")
// {
//	replace = QString("%1%").arg(value);
// }
// else
// {
//	char data[50];
//	memset(data, 0, sizeof(data));
//	snprintf(data, 50, format.toLatin1().data(), value);
//	replace = QString(data);
// }
//
// text.replace(index, to_replace.size() + format.size(), replace);
// index = index + replace.size();
// index = text.indexOf(to_replace, index);
// }
// }

template<class T>
VipText& VipText::replace(const QString& str, const T& value)
{
	QString tmp = replace(this->text(), str, value);
	this->setText(tmp);
	return *this;
}

template<class T>
QString VipText::replace(const QString& input, const QString& str, const T& value)
{
	QString new_str = input;
	int index = new_str.indexOf(str);
	while (index >= 0) {
		QString format = formattingSequence(new_str, index + str.size());
		QString replace;

		if (format.isEmpty()) {
			replace = QString("%1").arg(value);
		}
		else if (format == "%%") {
			replace = QString("%1%").arg(value);
		}
		else {
			char data[50];

			memset(data, 0, sizeof(data));

			if (format.contains('i') || format.contains('d'))
				snprintf(data, 50, format.toLatin1().data(), (int)value);
			else if (format.contains('o') || format.contains('u') || format.contains('u') || format.contains('x') || format.contains('X'))
				snprintf(data, 50, format.toLatin1().data(), (unsigned)value);
			else if (format.contains('c'))
				snprintf(data, 50, format.toLatin1().data(), (char)value);
			else if (format.contains('L'))
				snprintf(data, 50, format.toLatin1().data(), (long double)value);
			else
				snprintf(data, 50, format.toLatin1().data(), (double)value);
			replace = QString(data);
		}

		new_str.replace(index, str.size() + format.size(), replace);
		index = index + replace.size();
		index = new_str.indexOf(str, index);
	}

	return (new_str);
}

template<>
inline QString VipText::replace(const QString& input, const QString& str, const long double& value)
{
	QString new_str = input;
	int index = new_str.indexOf(str);
	while (index >= 0) {
		QString format = formattingSequence(new_str, index + str.size());
		QString replace;

		if (format.isEmpty()) {
			replace = QString("%1").arg(vipLongDoubleToString(value));
		}
		else if (format == "%%") {
			replace = QString("%1%").arg(vipLongDoubleToString(value));
		}
		else {
			char data[50];
			memset(data, 0, sizeof(data));
			if (format.contains('i') || format.contains('d'))
				snprintf(data, 50, format.toLatin1().data(), (int)value);
			else if (format.contains('o') || format.contains('u') || format.contains('u') || format.contains('x') || format.contains('X'))
				snprintf(data, 50, format.toLatin1().data(), (unsigned)value);
			else if (format.contains('c'))
				snprintf(data, 50, format.toLatin1().data(), (char)value);
			else if (format.contains('L'))
				snprintf(data, 50, format.toLatin1().data(), (long double)value);
			else
				snprintf(data, 50, format.toLatin1().data(), (double)value);
			replace = QString(data);
		}

		new_str.replace(index, str.size() + format.size(), replace);
		index = index + replace.size();
		index = new_str.indexOf(str, index);
	}

	return (new_str);
}

class QPicture;
VIP_PLOTTING_EXPORT QByteArray vipToHtml(const QPixmap& pixmap, const QByteArray& additional_attributes = QByteArray(), bool* ok = nullptr);
VIP_PLOTTING_EXPORT QByteArray vipToHtml(const QImage& image, const QByteArray& additional_attributes = QByteArray(), bool* ok = nullptr);
VIP_PLOTTING_EXPORT QByteArray vipToHtml(const QPicture& picture, const QByteArray& additional_attributes = QByteArray(), bool* ok = nullptr);

class VIP_PLOTTING_EXPORT VipTextObject
{
public:
	VipTextObject(const VipText& text = VipText(), const QRectF& rect = QRectF(), const QTransform& tr = QTransform());
	VipTextObject(const VipText& text, const VipPie& pie, const QPointF& center, VipText::TextDirection dir = VipText::AutoDirection, const QTransform& tr = QTransform());
	VipTextObject(const VipTextObject&);

	VipTextObject& operator=(const VipTextObject&);

	//
	// generic functions
	//

	void setText(const VipText&);
	const VipText& text() const;
	VipText& text();

	void setTransform(const QTransform&);
	const QTransform& transform() const;

	const QPainterPath& shape() const;
	void draw(QPainter*) const;

	//
	// draw text into a rect
	//

	void setRect(const QRectF&);
	const QRectF& rect() const;

	//
	// draw text into a pie
	//

	void setPie(const VipPie& pie);
	void setPie(const VipPie& pie, const QPointF& c);
	void setPie(const VipPie& pie, const QPointF& c, VipText::TextDirection dir);
	const VipPie& pie() const;

	void setCenter(const QPointF& c);
	const QPointF& center() const;

	void setTextDirection(VipText::TextDirection dir);
	VipText::TextDirection textDirection() const;

private:
	class PrivateData;
	PrivateData* d_data;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipText::LayoutAttributes);
Q_DECLARE_METATYPE(VipTextStyle);
Q_DECLARE_METATYPE(VipText);
typedef QList<VipText> VipTextList;
Q_DECLARE_METATYPE(VipTextList);
Q_DECLARE_METATYPE(VipTextObject);

class QDataStream;
VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipTextStyle& style);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipTextStyle& style);

VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipText& text);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipText& text);

/// @}
// end Plotting

#endif
