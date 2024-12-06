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

#include "VipText.h"
#include "VipPainter.h"
#include "VipPie.h"
#include "VipShapeDevice.h"

#include <iostream>
#include <limits>
#include <numeric>

#include <QBuffer>
#include <QPicture>
#include <QRawFont>
#include <QTextLayout>
#include <QTextStream>
#include <qabstracttextdocumentlayout.h>
#include <qapplication.h>
#include <qimage.h>
#include <qmap.h>
#include <qmath.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qtextdocument.h>
#include <qtextobject.h>
#include <qwidget.h>

template<class Image>
Image makeImage(const QSize& s)
{
	return Image(s);
}
template<>
QImage makeImage<QImage>(const QSize& s)
{
	QImage res(s.width(), s.height(), QImage::Format_ARGB32);
	return res;
}

template<class Image>
class VipPixmapCache
{
	int d_max_size;
	int d_top_size;
	QMap<int, Image> d_cache;

public:
	VipPixmapCache(int max_size)
	  : d_max_size(max_size)
	  , d_top_size(0)
	{
	}

	Image& pixmap(const QSize& size)
	{

		while (d_top_size > d_max_size && d_cache.size()) {
			d_top_size -= d_cache.begin().key();
			d_cache.erase(d_cache.begin());
		}

		int sizeB = size.width() * size.height() * 4;
		typename QMap<int, Image>::iterator it = d_cache.lowerBound(sizeB);
		if (it != d_cache.end() && it.value().size() == size) {
			it.value().fill(Qt::transparent);
			return it.value();
		}
		else {
			int sizeUp = (sizeB * 2);
			for (; it != d_cache.end(); ++it) {
				if (it.key() > sizeUp)
					break;
				if (it.value().width() >= size.width() && it.value().height() >= size.height()) {
					it.value().fill(Qt::transparent);
					return it.value();
				}
			}
		}

		Image pix = makeImage<Image>(size); //(size);
		pix.fill(Qt::transparent);
		d_cache[sizeB] = pix;
		d_top_size += sizeB;
		return d_cache[sizeB];
	}

	static VipPixmapCache& instance()
	{
		static thread_local VipPixmapCache inst(20000000);
		return inst;
	}
};

static QString taggedRichText(const QString& text, int flags)
{
	QString richText = text;

	// By default QSimpleRichText is Qt::AlignLeft
	if (flags & Qt::AlignJustify) {
		richText.prepend(QString::fromLatin1("<div align=\"justify\">"));
		richText.append(QString::fromLatin1("</div>"));
	}
	else if (flags & Qt::AlignRight) {
		richText.prepend(QString::fromLatin1("<div align=\"right\">"));
		richText.append(QString::fromLatin1("</div>"));
	}
	else if (flags & Qt::AlignHCenter) {
		richText.prepend(QString::fromLatin1("<div align=\"center\">"));
		richText.append(QString::fromLatin1("</div>"));
	}

	return richText;
}

class RichTextDocument : public QTextDocument
{
public:
	RichTextDocument(const QString& text, int flags, const QFont& font)
	{
		setUndoRedoEnabled(false);
		setDefaultFont(font);
		setHtml(text);

		// make sure we have a document layout
		(void)documentLayout();

		QTextOption option = defaultTextOption();
		if (flags & Qt::TextWordWrap)
			option.setWrapMode(QTextOption::WordWrap);
		else
			option.setWrapMode(QTextOption::NoWrap);

		option.setAlignment(static_cast<Qt::Alignment>(flags));
		setDefaultTextOption(option);

		QTextFrame* root = rootFrame();
		QTextFrameFormat fm = root->frameFormat();
		fm.setBorder(0);
		fm.setMargin(0);
		fm.setPadding(0);
		fm.setBottomMargin(0);
		fm.setLeftMargin(0);
		root->setFrameFormat(fm);

		adjustSize();
	}
};

class VipPlainTextEngine::PrivateData
{
public:
	int effectiveAscent(const QFont& font) const
	{
		const QString fontKey = font.key();

		QMap<QString, int>::const_iterator it = d_ascentCache.find(fontKey);
		if (it == d_ascentCache.end()) {
			int ascent = findAscent(font);
			it = d_ascentCache.insert(fontKey, ascent);
		}

		return (*it);
	}

private:
	int findAscent(const QFont& font) const
	{
		static const QString dummy("E");
		static const QColor white(Qt::white);

		const QFontMetrics fm(font);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		QPixmap pm(fm.width(dummy), fm.height());
#else
		QPixmap pm(fm.horizontalAdvance(dummy), fm.height());
#endif
		pm.fill(white);

		QPainter p(&pm);
		p.setFont(font);
		p.drawText(0, 0, pm.width(), pm.height(), 0, dummy);
		p.end();

		const QImage img = pm.toImage();

		int row = 0;
		for (row = 0; row < img.height(); row++) {
			const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(row));

			const int w = pm.width();
			for (int col = 0; col < w; col++) {
				if (line[col] != white.rgb())
					return fm.ascent() - row + 1;
			}
		}

		return fm.ascent();
	}

	mutable QMap<QString, int> d_ascentCache;
};

//! Constructor
VipTextEngine::VipTextEngine() {}

//! Destructor
VipTextEngine::~VipTextEngine() {}

//! Constructor
VipPlainTextEngine::VipPlainTextEngine()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

//! Destructor
VipPlainTextEngine::~VipPlainTextEngine() {}

/// Find the height for a given width
///
/// \param font Font of the text
/// \param flags Bitwise OR of the flags used like in QPainter::drawText
/// \param text VipText to be rendered
/// \param width Width
///
/// \return Calculated height
double VipPlainTextEngine::heightForWidth(const QFont& font, int flags, const QString& text, double width) const
{
	const QFontMetricsF fm(font);
	const QRectF rect = fm.boundingRect(QRectF(0, 0, width, QWIDGETSIZE_MAX), flags, text);

	return rect.height();
}

/// Returns the size, that is needed to render text
///
/// \param font Font of the text
/// \param flags Bitwise OR of the flags used like in QPainter::drawText
/// \param text VipText to be rendered
///
/// \return Caluclated size
QSizeF VipPlainTextEngine::textSize(const QFont& font, int flags, const QString& text) const
{
	const QFontMetricsF fm(font);
	const QRectF rect = fm.boundingRect(QRectF(0, 0, QWIDGETSIZE_MAX, QWIDGETSIZE_MAX), flags, text);

	return rect.size(); //+QSizeF(10,0);
}

/// Return margins around the texts
///
/// \param font Font of the text
/// \param left Return 0
/// \param right Return 0
/// \param top Return value for the top margin
/// \param bottom Return value for the bottom margin
void VipPlainTextEngine::textMargins(const QFont& font, const QString&, double& left, double& right, double& top, double& bottom) const
{
	left = right = top = 0;

	const QFontMetricsF fm(font);
	top = fm.ascent() - d_data->effectiveAscent(font);
	bottom = fm.descent();
}

/// \brief Draw the text in a clipping rectangle
///
/// A wrapper for QPainter::drawText.
///
/// \param painter VipPainter
/// \param rect Clipping rectangle
/// \param flags Bitwise OR of the flags used like in QPainter::drawText
/// \param text VipText to be rendered
void VipPlainTextEngine::draw(QPainter* painter, const QRectF& rect, int flags, const QString& text) const
{
	VipPainter::drawText(painter, rect, flags, text);
}

/// Test if a string can be rendered by this text engine.
/// \return Always true. All texts can be rendered by VipPlainTextEngine
bool VipPlainTextEngine::mightRender(const QString&) const
{
	return true;
}

#ifndef QT_NO_RICHTEXT

//! Constructor
VipRichTextEngine::VipRichTextEngine() {}

/// Find the height for a given width
///
/// \param font Font of the text
/// \param flags Bitwise OR of the flags used like in QPainter::drawText()
/// \param text VipText to be rendered
/// \param width Width
///
/// \return Calculated height
double VipRichTextEngine::heightForWidth(const QFont& font, int flags, const QString& text, double width) const
{
	RichTextDocument doc(text, flags, font);

	doc.setPageSize(QSizeF(width, QWIDGETSIZE_MAX));
	return doc.documentLayout()->documentSize().height();
}

/// Returns the size, that is needed to render text
///
/// \param font Font of the text
/// \param flags Bitwise OR of the flags used like in QPainter::drawText()
/// \param text VipText to be rendered
///
/// \return Caluclated size

QSizeF VipRichTextEngine::textSize(const QFont& font, int flags, const QString& text) const
{
	RichTextDocument doc(text, flags, font);

	QTextOption option = doc.defaultTextOption();
	if (option.wrapMode() != QTextOption::NoWrap) {
		option.setWrapMode(QTextOption::NoWrap);
		doc.setDefaultTextOption(option);
		doc.adjustSize();
	}

	return doc.size();
}

/// Draw the text in a clipping rectangle
///
/// \param painter VipPainter
/// \param rect Clipping rectangle
/// \param flags Bitwise OR of the flags like in for QPainter::drawText()
/// \param text VipText to be rendered
void VipRichTextEngine::draw(QPainter* painter, const QRectF& rect, int flags, const QString& text) const
{
	RichTextDocument doc(text, flags, painter->font());
	VipPainter::drawSimpleRichText(painter, rect, flags, doc);
}

/// Wrap text into <div align=...> </div> tags according flags
///
/// \param text VipText
/// \param flags Bitwise OR of the flags like in for QPainter::drawText()
///
/// \return Tagged text
QString VipRichTextEngine::taggedText(const QString& text, int flags) const
{
	return taggedRichText(text, flags);
}

/// Test if a string can be rendered by this text engine
///
/// \param text VipText to be tested
/// \return Qt::mightBeRichText(text);
bool VipRichTextEngine::mightRender(const QString& text) const
{
	return Qt::mightBeRichText(text);
}

/// Return margins around the texts
///
/// \param left Return 0
/// \param right Return 0
/// \param top Return 0
/// \param bottom Return 0
void VipRichTextEngine::textMargins(const QFont&, const QString&, double& left, double& right, double& top, double& bottom) const
{
	left = right = top = bottom = 0;
}

#endif // !QT_NO_RICHTEXT

class TextEngineDict
{
public:
	static TextEngineDict& dict();

	void setTextEngine(VipText::TextFormat, VipTextEngine*);

	const VipTextEngine* textEngine(VipText::TextFormat) const;
	const VipTextEngine* textEngine(const QString&, VipText::TextFormat) const;

private:
	TextEngineDict();
	~TextEngineDict();

	typedef QMap<int, VipTextEngine*> EngineMap;

	inline const VipTextEngine* engine(EngineMap::const_iterator& it) const { return it.value(); }

	EngineMap d_map;
};

TextEngineDict& TextEngineDict::dict()
{
	static TextEngineDict engineDict;
	return engineDict;
}

TextEngineDict::TextEngineDict()
{
	d_map.insert(VipText::PlainText, new VipPlainTextEngine());
#ifndef QT_NO_RICHTEXT
	d_map.insert(VipText::RichText, new VipRichTextEngine());
#endif
}

TextEngineDict::~TextEngineDict()
{
	for (EngineMap::const_iterator it = d_map.begin(); it != d_map.end(); ++it) {
		const VipTextEngine* textEngine = engine(it);
		delete textEngine;
	}
}

const VipTextEngine* TextEngineDict::textEngine(const QString& text, VipText::TextFormat format) const
{
	if (format == VipText::AutoText) {
		for (EngineMap::const_iterator it = d_map.begin(); it != d_map.end(); ++it) {
			if (it.key() != VipText::PlainText) {
				const VipTextEngine* e = engine(it);
				if (e && e->mightRender(text))
					return e;
			}
		}
	}

	EngineMap::const_iterator it = d_map.find(format);
	if (it != d_map.end()) {
		const VipTextEngine* e = engine(it);
		if (e)
			return e;
	}

	it = d_map.find(VipText::PlainText);
	return engine(it);
}

void TextEngineDict::setTextEngine(VipText::TextFormat format, VipTextEngine* engine)
{
	if (format == VipText::AutoText)
		return;

	if (format == VipText::PlainText && engine == nullptr)
		return;

	EngineMap::const_iterator it = d_map.find(format);
	if (it != d_map.end()) {
		const VipTextEngine* e = this->engine(it);
		if (e)
			delete e;

		d_map.remove(format);
	}

	if (engine != nullptr)
		d_map.insert(format, engine);
}

const VipTextEngine* TextEngineDict::textEngine(VipText::TextFormat format) const
{
	const VipTextEngine* e = nullptr;

	EngineMap::const_iterator it = d_map.find(format);
	if (it != d_map.end())
		e = engine(it);

	return e;
}

static QPainter::RenderHints default_text_hints = QPainter::Antialiasing | QPainter::TextAntialiasing;

VipTextStyle::PrivateData::PrivateData()
  : cached(false)
  , margin(0)
  , boxStyle(Qt::NoPen)
  , alignment(Qt::AlignCenter)
  , renderHints(default_text_hints)
{
}

VipTextStyle::PrivateData::PrivateData(const VipTextStyle::PrivateData& other)
  : QSharedData()
  , cached(other.cached)
  , margin(other.margin)
  , font(other.font)
  , textPen(other.textPen)
  , boxStyle(other.boxStyle)
  , alignment(other.alignment)
  , renderHints(other.renderHints)
{
	if (other.textBoxStyle)
		textBoxStyle.reset(new VipBoxStyle(*other.textBoxStyle));
}

VipTextStyle::PrivateData::~PrivateData()
{
}

VipTextStyle::VipTextStyle()
  : d_data(new PrivateData())
{
}

VipTextStyle::VipTextStyle(const VipTextStyle& other)
  : d_data(other.d_data)
{
}

VipTextStyle& VipTextStyle::operator=(const VipTextStyle& other)
{
	d_data = other.d_data;
	return *this;
}
// bool VipTextStyle::operator==(const VipTextStyle &other) const
// {
// return d_data->font == other.d_data->font &&
//	d_data->textPen == other.d_data->textPen &&
//	d_data->boxStyle == other.d_data->boxStyle &&
//	d_data->alignment == other.d_data->alignment &&
//	d_data->renderHints == other.d_data->renderHints;
// }
//
// bool VipTextStyle::operator!=(const VipTextStyle &other) const
// {
// return !(operator==(other));
// }
const QFont& VipTextStyle::font() const
{
	return d_data->font;
}

QFont& VipTextStyle::font()
{
	return d_data->font;
}

void VipTextStyle::setFont(const QFont& font)
{
	d_data->font = font;
}

void VipTextStyle::setCached(bool cached)
{
	d_data->cached = cached;
}
bool VipTextStyle::cached() const
{
	return d_data->cached;
}

const QPen& VipTextStyle::textPen() const
{
	return d_data->textPen;
}

QPen& VipTextStyle::textPen()
{
	return d_data->textPen;
}

void VipTextStyle::setTextPen(const QPen& textPen)
{
	d_data->textPen = textPen;
}

QPen VipTextStyle::borderPen() const
{
	return d_data->boxStyle.borderPen();
}

void VipTextStyle::setBorderPen(const QPen& borderPen)
{
	d_data->boxStyle.setBorderPen(borderPen);
}

QBrush VipTextStyle::backgroundBrush() const
{
	return d_data->boxStyle.backgroundBrush();
}

void VipTextStyle::setBackgroundBrush(const QBrush& backgroundBrush)
{
	d_data->boxStyle.setBackgroundBrush(backgroundBrush);
}

double VipTextStyle::borderRadius() const
{
	return d_data->boxStyle.borderRadius();
}

void VipTextStyle::setBorderRadius(double borderRadius)
{
	d_data->boxStyle.setBorderRadius(borderRadius);
}

void VipTextStyle::setAlignment(Qt::Alignment align)
{
	d_data->alignment = align;
}

Qt::Alignment VipTextStyle::alignment() const
{
	return d_data->alignment;
}

void VipTextStyle::setRenderHints(QPainter::RenderHints renderHints)
{
	d_data->renderHints = renderHints;
}

QPainter::RenderHints VipTextStyle::renderHints() const
{
	return d_data->renderHints;
}

void VipTextStyle::setMargin(double margin)
{
	d_data->margin = margin;
}
double VipTextStyle::margin() const
{
	return d_data->margin;
}

void VipTextStyle::setBoxStyle(const VipBoxStyle& bs)
{
	d_data->boxStyle = bs;
}

const VipBoxStyle& VipTextStyle::boxStyle() const
{
	return d_data->boxStyle;
}

VipBoxStyle& VipTextStyle::boxStyle()
{
	return d_data->boxStyle;
}

void VipTextStyle::setTextBoxStyle(const VipBoxStyle& s)
{
	if (!d_data->textBoxStyle)
		d_data->textBoxStyle .reset(new VipBoxStyle());
	*d_data->textBoxStyle = s;
}
const VipBoxStyle& VipTextStyle::textBoxStyle() const
{
	if (!d_data->textBoxStyle)
		const_cast<VipTextStyle*>(this)->d_data->textBoxStyle.reset( new VipBoxStyle());
	return *d_data->textBoxStyle;
}
VipBoxStyle& VipTextStyle::textBoxStyle()
{
	if (!d_data->textBoxStyle)
		d_data->textBoxStyle.reset( new VipBoxStyle());
	return *d_data->textBoxStyle;
}

bool VipTextStyle::hasTextBoxStyle() const
{
	return d_data->textBoxStyle != nullptr;
}

bool VipTextStyle::operator==(const VipTextStyle& other) const
{
	return other.d_data->alignment == d_data->alignment && other.d_data->boxStyle == d_data->boxStyle && other.d_data->font == d_data->font && other.d_data->renderHints == d_data->renderHints &&
	       other.d_data->textPen == d_data->textPen && d_data->margin == other.d_data->margin;
}

bool VipTextStyle::operator!=(const VipTextStyle& other) const
{
	return !((*this) == other);
}

VipText::PrivateData::PrivateData()
  : QSharedData()
  , textEngine(nullptr)
{
}

VipText::PrivateData::PrivateData(const PrivateData& other)
  : QSharedData()
  , text(other.text)
  , parameters(other.parameters)
  , layoutAttributes(other.layoutAttributes)
  , textEngine(other.textEngine)
{
}

/// Constructor
///
/// \param text VipText content
/// \param textFormat VipText format
VipText::VipText(const QString& text, VipText::TextFormat textFormat)
  : d_data(new PrivateData())
  , d_dirtyTextSize(1)
{
	d_data->text = text;
	d_data->textEngine = textEngine(text, textFormat);
}

VipText::VipText(const QString& text, const VipTextStyle& p, TextFormat textFormat)
  : d_data(new PrivateData())
  , d_dirtyTextSize(1)
{
	d_data->text = text;
	d_data->textEngine = textEngine(text, textFormat);
	d_data->parameters = p;
}

//! Copy constructor
VipText::VipText(const VipText& other)
  : d_data(other.d_data)
  , d_dirtyTextSize(1)
{
}

VipText::VipText(const QString& text, const VipTextEngine* engine)
  : d_data(new PrivateData())
  , d_dirtyTextSize(1)
{
	d_data->text = text;
	d_data->textEngine = engine;
}

//! Assignment operator
VipText& VipText::operator=(const VipText& other)
{
	d_data = other.d_data;
	d_dirtyTextSize = other.d_dirtyTextSize;
	d_textSize = other.d_textSize;
	return *this;
}

//! Relational operator
bool VipText::operator==(const VipText& other) const
{
	return d_data->text == other.d_data->text && d_data->parameters == other.d_data->parameters;
}

//! Relational operator
bool VipText::operator!=(const VipText& other) const // invalidate
{
	return !(other == *this);
}

/// Assign a new text content
///
/// \param text VipText content
/// \param textFormat VipText format
///
/// \sa text()
void VipText::setText(const QString& text, VipText::TextFormat textFormat)
{
	d_data->text = text;
	d_data->textEngine = textEngine(text, textFormat);
	d_dirtyTextSize = 1;
}

/// \return VipText as QString.
/// \sa setText()
const QString& VipText::text() const
{
	return d_data->text;
}

/// \return VipText as QString.
/// \sa setText()
QString& VipText::text()
{
	return d_data->text;
}

/// \brief Change the render flags
///
/// The default setting is Qt::AlignCenter
///
/// \param alignment Bitwise OR of the flags used like in QPainter::drawText()
///
/// \sa alignment(), VipTextEngine::draw()
/// \note Some alignment might have no effect, depending on the text format.
VipText& VipText::setAlignment(Qt::Alignment align)
{
	d_data->parameters.setAlignment(align);
	return *this;
}

/// \return Render flags
/// \sa setRenderFlags()
Qt::Alignment VipText::alignment() const
{
	return d_data->parameters.alignment();
}

VipText& VipText::setCached(bool cached)
{
	d_data->parameters.setCached(cached);
	return *this;
}
bool VipText::cached() const
{
	return d_data->parameters.cached();
}

static bool& cache_text_when_possible()
{
	static bool inst = true;
	return inst;
}
bool VipText::cacheTextWhenPossible()
{
	return cache_text_when_possible();
}
void VipText::setCacheTextWhenPossible(bool enable)
{
	cache_text_when_possible() = enable;
}

/// Set the font.
///
/// \param font Font
/// \note Setting the font might have no effect, when
///      the text contains control sequences for setting fonts.
VipText& VipText::setFont(const QFont& font)
{
	d_data->parameters.setFont(font);
	d_dirtyTextSize = 1;
	return *this;
}

//! Return the font.
const QFont& VipText::font() const
{
	return d_data->parameters.font();
}

//! Return the font.
QFont& VipText::font()
{
	return d_data->parameters.font();
}

/// Set the pen color used for drawing the text.
///
/// \param color Color
/// \note Setting the color might have no effect, when
///      the text contains control sequences for setting colors.
VipText& VipText::setTextPen(const QPen& pen)
{
	d_data->parameters.setTextPen(pen);
	return *this;
}

//! Return the pen color, used for painting the text
const QPen& VipText::textPen() const
{
	return d_data->parameters.textPen();
}

//! Return the pen color, used for painting the text
QPen& VipText::textPen()
{
	return d_data->parameters.textPen();
}

/// Set the radius for the corners of the border frame
///
/// \param radius Radius of a rounded corner
/// \sa borderRadius(), setBorderPen(), setBackgroundBrush()
VipText& VipText::setBorderRadius(double radius)
{
	d_data->parameters.setBorderRadius(qMax(0.0, radius));
	return *this;
}

/// \return Radius for the corners of the border frame
/// \sa setBorderRadius(), borderPen(), backgroundBrush()
double VipText::borderRadius() const
{
	return d_data->parameters.borderRadius();
}

/// Set the background pen
///
/// \param pen Background pen
/// \sa borderPen(), setBackgroundBrush()
VipText& VipText::setBorderPen(const QPen& pen)
{
	d_data->parameters.setBorderPen(pen);
	return *this;
}

/// \return Background pen
/// \sa setBorderPen(), backgroundBrush()
QPen VipText::borderPen() const
{
	return d_data->parameters.borderPen();
}

/// Set the background brush
///
/// \param brush Background brush
/// \sa backgroundBrush(), setBorderPen()
VipText& VipText::setBackgroundBrush(const QBrush& brush)
{
	d_data->parameters.setBackgroundBrush(brush);
	return *this;
}

/// \return Background brush
/// \sa setBackgroundBrush(), borderPen()
QBrush VipText::backgroundBrush() const
{
	return d_data->parameters.backgroundBrush();
}

VipText& VipText::setRenderHints(QPainter::RenderHints hints)
{
	d_data->parameters.setRenderHints(hints);
	return *this;
}

QPainter::RenderHints VipText::renderHints() const
{
	return d_data->parameters.renderHints();
}

VipText& VipText::setTextStyle(const VipTextStyle& p)
{
	d_data->parameters = p;
	return *this;
}

const VipTextStyle& VipText::textStyle() const
{
	return d_data->parameters;
}

VipTextStyle& VipText::textStyle()
{
	return d_data->parameters;
}

VipText& VipText::setBoxStyle(const VipBoxStyle& bs)
{
	d_data->parameters.setBoxStyle(bs);
	return *this;
}

const VipBoxStyle& VipText::boxStyle() const
{
	return d_data->parameters.boxStyle();
}

VipBoxStyle& VipText::boxStyle()
{
	return d_data->parameters.boxStyle();
}

VipText& VipText::setTextBoxStyle(const VipBoxStyle& p)
{
	d_data->parameters.setTextBoxStyle(p);
	return *this;
}
const VipBoxStyle& VipText::textBoxStyle() const
{
	return d_data->parameters.textBoxStyle();
}
VipBoxStyle& VipText::textBoxStyle()
{
	return d_data->parameters.textBoxStyle();
}
bool VipText::hasTextBoxStyle() const
{
	return d_data->parameters.hasTextBoxStyle();
}

/// Change a layout attribute
///
/// \param attribute Layout attribute
/// \param on On/Off
/// \sa testLayoutAttribute()
VipText& VipText::setLayoutAttribute(LayoutAttribute attribute, bool on)
{
	if (on)
		d_data->layoutAttributes |= attribute;
	else
		d_data->layoutAttributes &= ~attribute;
	d_dirtyTextSize = 1;
	return *this;
}

VipText& VipText::setLayoutAttributes(LayoutAttributes attrs)
{
	d_data->layoutAttributes = attrs;
	d_dirtyTextSize = 1;
	return *this;
}

/// Test a layout attribute
///
/// \param attribute Layout attribute
/// \return true, if attribute is enabled
///
/// \sa setLayoutAttribute()
bool VipText::testLayoutAttribute(LayoutAttribute attribute) const
{
	return d_data->layoutAttributes | attribute;
}

VipText::LayoutAttributes VipText::layoutAttributes() const
{
	return d_data->layoutAttributes;
}

/// Find the height for a given width
///
/// \param defaultFont Font, used for the calculation if the text has no font
/// \param width Width
///
/// \return Calculated height
double VipText::heightForWidth(double width) const
{
	// We want to calculate in screen metrics. So
	// we need a font that uses screen metrics

	double h = 0;

	if (d_data->layoutAttributes & MinimumLayout) {
		double left, right, top, bottom;
		d_data->textEngine->textMargins(font(), d_data->text, left, right, top, bottom);

		h = d_data->textEngine->heightForWidth(font(), alignment(), d_data->text, width + left + right);

		h -= top + bottom;
	}
	else {
		h = d_data->textEngine->heightForWidth(font(), alignment(), d_data->text, width);
	}

	return h;
}

/// Find the height for a given width
///
/// \param defaultFont Font, used for the calculation if the text has no font
///
/// \return Calculated height

/// Returns the size, that is needed to render text
///
/// \param defaultFont Font of the text
/// \return Caluclated size
QSizeF VipText::textSize() const
{
	// We want to calculate in screen metrics. So
	// we need a font that uses screen metrics

	if (d_dirtyTextSize) {
		const_cast<VipText*>(this)->d_textSize = d_data->textEngine->textSize(font(), alignment(), d_data->text);
		const_cast<VipText*>(this)->d_dirtyTextSize = 0;
	}

	QSizeF sz = d_textSize;

	if (d_data->layoutAttributes & MinimumLayout) {
		double left, right, top, bottom;
		d_data->textEngine->textMargins(font(), d_data->text, left, right, top, bottom);
		sz -= QSizeF(left + right, top + bottom);
	}

	return sz;
}

QString VipText::repeatBlock(const QString input)
{
	QString str = input;

	int end = str.indexOf("#endrepeat");
	if (end < 0)
		return str;

	int start = str.lastIndexOf("#repeat", end);
	if (start < 0)
		return str;

	start += 7;

	// read the number of loop
	int end_start_tag1 = str.indexOf("=", start);
	QTextStream stream(&str);
	stream.seek(end_start_tag1 + 1);
	int ntimes = 0;
	stream >> ntimes;
	if (stream.status() != QTextStream::Ok)
		return str;

	int end_start_tag2 = stream.pos();
	if (end_start_tag1 < 0 || end_start_tag2 < 0)
		return str;

	start = start - 7;
	int start_inner = end_start_tag2;
	int end_inner = end;
	end = end + 10;

	QString inner = str.mid(start_inner, end_inner - start_inner);
	QString repeated;

	for (int i = 0; i < ntimes; ++i) {
		QString num = QString::number(i);
		QString _inner = inner;
		_inner.replace("%i", num);
		repeated += _inner;
	}

	str.replace(start, end - start, repeated);
	// this->setText(str);

	return repeatBlock(str);

	// return *this;
}

VipText& VipText::repeatBlock()
{
	QString tmp = repeatBlock(text());
	this->setText(tmp);
	return *this;
}

const VipTextEngine* VipText::textEngine() const
{
	return d_data->textEngine;
}

#include <qpaintengine.h>

void VipText::draw(QPainter* painter, const QPointF& topLeft) const
{
	QRectF rect = this->textRect();
	rect.moveTopLeft(topLeft);
	draw(painter, rect);
}

static QRectF computeRect(const QRectF& outer, const QRectF& inner, int flags)
{
	QRectF res = inner;
	if (flags & Qt::AlignLeft)
		res.moveLeft(outer.left());
	else if (flags & Qt::AlignRight)
		res.moveRight(outer.right());
	else
		res.moveLeft(outer.left() + (outer.width() - inner.width()) / 2);

	if (flags & Qt::AlignTop)
		res.moveTop(outer.top());
	else if (flags & Qt::AlignBottom)
		res.moveBottom(outer.bottom());
	else
		res.moveTop(outer.top() + (outer.height() - inner.height()) / 2);

	return res & outer;
}

#include "VipPicture.h"

/// Draw a text into a rectangle
///
/// \param painter VipPainter
/// \param rect Rectangle
void VipText::draw(QPainter* painter, const QRectF& rect) const
{
	if (!painter->paintEngine())
		return;

	if (renderHints() != painter->renderHints())
		painter->setRenderHints(renderHints());
	if (!this->textStyle().boxStyle().isTransparent()) {
		VipBoxStyle bstyle = this->textStyle().boxStyle();
		double m = this->textStyle().margin();
		bstyle.computeRect(rect.adjusted(-m, -m, m, m));
		bstyle.draw(painter);
	}

	QRectF expandedRect = rect;
	if (layoutAttributes() & MinimumLayout) {
		// We want to calculate in screen metrics. So
		// we need a font that uses screen metrics

		double left, right, top, bottom;
		d_data->textEngine->textMargins(font(), d_data->text, left, right, top, bottom);

		expandedRect.setTop(rect.top() - top);
		expandedRect.setBottom(rect.bottom() + bottom);
		expandedRect.setLeft(rect.left() - left);
		expandedRect.setRight(rect.right() + right);
	}

	// QT 5.6 crashes when drawing text on a GL paint engine, so cash the text first in a QPixmap
	// also, we still need to cach the text if the painter transformation has a rotation, otherwise the rendering is ugly
	const bool is_opengl = VipPainter::isOpenGL(painter);
	const bool should_cache = VipText::cacheTextWhenPossible() ? (is_opengl && !hasTextBoxStyle()) || painter->worldTransform().isRotating() : false;
	const bool is_vectoriel = VipPainter::isVectoriel(painter);
	bool is_cached = ((cached() || should_cache) && !is_vectoriel);

	qreal ratio = painter->paintEngine() ? painter->paintEngine()->paintDevice()->devicePixelRatioF() : 1.;
	if (ratio < 0.01)
		ratio = 1.;
	if (ratio != 1. && !cached())
		is_cached = false;

	if (is_cached) {

		expandedRect.adjust(0, 0, 2, 2);
		QSize s = (expandedRect.size().toSize() + QSize(2, 2)) * ratio;

		static thread_local QPixmap pixmap;
		QSize pixsize(pixmap.width(), pixmap.height());
		if (pixmap.isNull() || pixsize.width() < s.width() || pixsize.width() > 2 * s.width() || pixsize.height() < s.height() || pixsize.height() > 2 * s.height())
			pixmap = QPixmap(s);
		pixmap.fill(Qt::transparent);

		pixmap.setDevicePixelRatio(ratio);
		QPainter p(&pixmap);
		p.setFont(font());
		p.setPen(textPen());
		p.setRenderHints(renderHints());

		if (!hasTextBoxStyle()) {
			d_data->textEngine->draw(&p, expandedRect.translated(-expandedRect.topLeft()), alignment(), d_data->text);
		}
		else {
			QPainterPath path = textAsPath();
			QRectF outer = expandedRect.translated(-expandedRect.topLeft());
			QRectF inner = path.boundingRect();
			QRectF r = computeRect(outer, inner, alignment());
			path.translate(r.topLeft() - inner.topLeft());
			VipBoxStyle st = textBoxStyle();
			st.computePath(path);
			st.draw(&p);
		}

		// Disable SmoothPixmapTransform when possible as it produces ugly output
		const QTransform& tr = painter->transform();
		if (tr.isRotating() || tr.isScaling()) {
			if (!painter->testRenderHint(QPainter::SmoothPixmapTransform))
				painter->setRenderHint(QPainter::SmoothPixmapTransform);
		}
		else if (painter->testRenderHint(QPainter::SmoothPixmapTransform))
			painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
		// Using a different composition mode produces a strange behavior with opengl rendering
		if (is_opengl) {
			if (painter->compositionMode() != QPainter::CompositionMode_SourceOver)
				painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

			// For default QOpenglWidget, this draws a black pixmap if SmoothPixmapTransform is not set (?)
			if (!VipOpenGLWidget::isInPainting()) {

				if (!painter->testRenderHint(QPainter::SmoothPixmapTransform))
					painter->setRenderHint(QPainter::SmoothPixmapTransform);
			}
		}

		VipPainter::drawPixmap(painter, QRectF(expandedRect.topLeft() - QPointF(1, 1), QSizeF(pixmap.size())), pixmap);
	}
	else {
		if (font() != painter->font())
			painter->setFont(font());
		if (textPen() != painter->pen())
			painter->setPen(textPen());

		// with a ratio != 1, the top left corner is slightly shifted
		if (ratio != 1) {
			qreal factor = (ratio - 1) * 5;
			expandedRect.moveTopLeft(expandedRect.topLeft() + QPointF(-factor, -factor));
			expandedRect.adjust(0, 0, factor, factor);
		}

		if (!hasTextBoxStyle()) {
			d_data->textEngine->draw(painter, expandedRect, alignment(), d_data->text);
		}
		else {

			QPainterPath path = textAsPath();
			QRectF outer = expandedRect;
			QRectF inner = path.boundingRect();
			QRectF r = computeRect(outer, inner, alignment());
			path.translate(r.topLeft() - inner.topLeft());
			VipBoxStyle st = textBoxStyle();
			st.computePath(path);
			st.draw(painter);
		}
	}
}

QString VipText::replace(const QString& input, const QString& str, const QString& value, bool possible_numeric)
{
	bool ok = false;
	double vd;
	if (possible_numeric) {
		// if(value.contains('.') || value.contains(',') || value.contains('e',Qt::CaseInsensitive))
		vd = QLocale().toDouble(value, &ok);
		// else
		// vd = QLocale().toInt(value, &ok);
	}

	if (ok) {
		return replace(input, str, vd);
	}
	else {

		QString new_str = input;
		int index = new_str.indexOf(str);
		while (index >= 0) {
			new_str.replace(index, str.size(), value);
			index = index + value.size();
			index = new_str.indexOf(str, index);
		}

		return (new_str);
	}
}

VipText& VipText::replace(const QString& str, const QString& value, bool possible_numeric)
{
	QString tmp = replace(this->text(), str, value, possible_numeric);
	this->setText(tmp);
	return *this;
}

/// \internal
/// Represents a set of character with there properties
struct TextChar
{
	QVector<quint32> indexes;
	QVector<QPointF> positions;
	QRawFont raw_font;
	QTextCharFormat format;
	double line_y;
};

static QList<QList<TextChar>> getCharactersPerLines(const VipText& t)
{
	QList<QList<TextChar>> textChar;

	RichTextDocument doc(t.text(), t.alignment(), t.font());
	if (t.textEngine() == VipText::textEngine(VipText::PlainText)) {
		doc.setPlainText(t.text());
		doc.adjustSize();
	}

	// QTextBlock block = doc.begin();
	double start_line_y = doc.begin().layout()->position().y();
	double line_y = -std::numeric_limits<double>::max();
	;

	// get all QGlyphRun and associated QTextCharFormat
	for (QTextBlock block = doc.begin(); block != doc.end(); block = block.next()) {
		double _line_y = block.layout()->position().y() - start_line_y;
		if (_line_y != line_y) {
			// new line
			textChar << QList<TextChar>();
			line_y = _line_y;
		}

		QTextBlock::iterator it = block.begin();
		QList<QGlyphRun> glyphes = it.fragment().glyphRuns();

		if (!glyphes.size() && it.fragment().length()) {
			// the text fragments do not contain glyphes
			glyphes = block.layout()->glyphRuns();

			// create one TextChar per character, sorted by x position
			QMap<double, TextChar> mchars;
			for (int g = 0; g < glyphes.size(); ++g) {
				QVector<QPointF> positions = glyphes[g].positions();
				QVector<quint32> indexes = glyphes[g].glyphIndexes();
				for (int i = 0; i < positions.size(); ++i) {
					TextChar tc;
					tc.indexes.append(indexes[i]);
					tc.positions.append(positions[i]);
					tc.raw_font = glyphes[g].rawFont();
					tc.line_y = line_y;
					mchars[positions[i].x()] = tc;
				}
			}

			QList<TextChar> lchars = mchars.values();
			int current_index = 0;

			// apply the fragments format to eacg character
			for (; !(it.atEnd()); ++it) {
				QTextFragment frag = it.fragment();
				for (int i = current_index; i < current_index + frag.length(); ++i)
					lchars[i].format = frag.charFormat();
				current_index += frag.length();
			}

			textChar.back() << lchars;
		}
		else {
			// glyphes are set for each fragment (way easier)
			for (; !(it.atEnd()); ++it) {

				QTextFragment currentFragment = it.fragment();
				QTextCharFormat format = currentFragment.charFormat();
				const QList<QGlyphRun> glp = currentFragment.glyphRuns();

				for (int i = 0; i < glp.size(); ++i) {
					TextChar tc;
					tc.indexes = glp[i].glyphIndexes();
					tc.positions = glp[i].positions();
					tc.raw_font = glp[i].rawFont();
					tc.format = format;
					tc.line_y = line_y;
					textChar.back() << tc;
				}
			}
		}
	}

	return textChar;
}

QPainterPath VipText::textAsPath() const
{
	QList<QList<TextChar>> paths = getCharactersPerLines(*this);
	QPainterPath res;
	for (int line = 0; line < paths.size(); ++line) {
		QList<TextChar>& chars = paths[line];
		if (chars.isEmpty())
			continue;
		double y_offset = chars.first().line_y;
		for (int c = 0; c < chars.size(); ++c) {
			TextChar& tc = chars[c];
			for (int i = 0; i < tc.indexes.size(); ++i) {
				QPainterPath character = tc.raw_font.pathForGlyph(tc.indexes[i]);
				character.translate(QPointF(0, y_offset) + tc.positions[i]);
				res.addPath(character);
				res.closeSubpath();
			}
		}
	}
	return res;
}

/// Draw a text into a VipPie
///
/// \param painter VipPainter
/// \param pie VipPie
void VipText::draw(QPainter* painter, const QPointF& c, const VipPie& pie, TextDirection dir) const
{
	// draw pie
	painter->setRenderHints(renderHints());
	VipBoxStyle bstyle = this->textStyle().boxStyle();
	bstyle.computePie(c, pie);
	bstyle.draw(painter);

	// draw text
	if (d_data->text.isEmpty())
		return;

	painter->save();

	const QRectF rect = textRect();
	const double height = rect.height();
	const QList<QList<TextChar>> textChar = getCharactersPerLines(*this);
	const double angle_mean = (pie.endAngle() + pie.startAngle()) / 2.0;

	// recompute the center if necessary
	QPointF center(c);
	if (pie.offsetToCenter() != 0) {
		QLineF line(center, QPointF(center.x(), center.y() - pie.offsetToCenter()));
		line.setAngle(angle_mean);
		center = line.p2();
	}

	// compute direction if automatic
	if (dir == AutoDirection) {
		double angle = angle_mean;
		if (angle < 0)
			angle += 360;
		else if (angle > 360)
			angle -= 360;

		if (angle >= 0 && angle < 180)
			dir = TowardInside;
		else
			dir = TowardOutside;
	}

	// get the right base radius (bottom line of the text)
	double base_radius = 0;

	if (dir == TowardInside) {
		base_radius = pie.minRadius();
		if (this->alignment() & Qt::AlignTop) {
			base_radius = pie.maxRadius() - height;
		}
		else if (this->alignment() & Qt::AlignVCenter) {
			base_radius = pie.minRadius() + (pie.radiusExtent() - height) / 2.0;
		}
	}
	else if (dir == TowardOutside) {
		base_radius = pie.maxRadius();
		if (this->alignment() & Qt::AlignTop) {
			base_radius = pie.minRadius() + height;
		}
		else if (this->alignment() & Qt::AlignVCenter) {
			base_radius = pie.maxRadius() - (pie.radiusExtent() - height) / 2.0;
		}
	}

	// compute the angle and radius of each character for each line

	QList<QList<double>> angle_positions;
	QList<QList<double>> radiuses;

	for (int line = 0; line < textChar.size(); ++line) {
		angle_positions << QList<double>();
		radiuses << QList<double>();

		const QList<TextChar>& tchar = textChar[line];

		for (const TextChar& tc : tchar) {
			if (dir == TowardInside) {
				for (int i = 0; i < tc.indexes.size(); ++i) {
					double y = tc.positions[i].y();
					radiuses.back() << (base_radius + height - y - tc.line_y);
					angle_positions.back() << 2 * qAsin((tc.positions[i].x() / 2.0) / radiuses.back().back()) * Vip::ToDegree;
				}
			}
			else if (dir == TowardOutside) {
				for (int i = 0; i < tc.indexes.size(); ++i) {
					double y = tc.positions[i].y();
					radiuses.back() << (base_radius - height + y + tc.line_y);
					double angle = 2 * qAsin((tc.positions[i].x() / 2.0) / (radiuses.back().back() - y / 2.0)) * Vip::ToDegree;
					angle_positions.back() << (angle_mean - (angle - angle_mean));
				}
			}
		}

		// add the angle after last character
		const TextChar& tc = tchar.back();
		if (dir == TowardInside)
			angle_positions.back() << angle_positions.back().back() +
						    2 * qAsin((tc.raw_font.averageCharWidth() / 2.0) / (radiuses.back().back() - tc.positions.back().y() / 2.0)) * Vip::ToDegree;
		else if (dir == TowardOutside)
			angle_positions.back() << angle_positions.back().back() -
						    2 * qAsin((tc.raw_font.averageCharWidth() / 2.0) / (radiuses.back().back() - tc.positions.back().y() / 2.0)) * Vip::ToDegree;
	}

	// compute the right start offset angle for each line and the angle of each character

	QList<double> start_offset_angle;
	for (int line = 0; line < angle_positions.size(); ++line) {
		QList<double>& angles = angle_positions[line];
		double min = *std::min_element(angles.begin(), angles.end());
		double max = *std::max_element(angles.begin(), angles.end());
		double total_angle = max - min;

		for (int i = 0; i < angles.size(); ++i) {
			angles[i] -= min;
		}

		if (dir == TowardInside) {
			if (this->alignment() & Qt::AlignRight) {
				start_offset_angle << pie.sweepLength() - total_angle;
			}
			else if (this->alignment() & Qt::AlignHCenter) {
				start_offset_angle << (pie.sweepLength() - total_angle) / 2;
			}
			else {
				start_offset_angle << 0;
			}
		}
		else if (dir == TowardOutside) {
			if (this->alignment() & Qt::AlignRight) {
				start_offset_angle << 0;
			}
			else if (this->alignment() & Qt::AlignHCenter) {
				start_offset_angle << (pie.sweepLength() - total_angle) / 2;
			}
			else {
				start_offset_angle << pie.sweepLength() - total_angle;
			}
		}
	}

	// draw each character
	painter->setRenderHints(QPainter::Antialiasing);

	// QTransform tr;
	// tr.translate(center.x(), center.y());
	// tr.rotate(360 - pie.endAngle());

	VipBoxStyle textBStyle;
	if (hasTextBoxStyle())
		textBStyle = textBoxStyle();

	for (int line = 0; line < textChar.size(); ++line) {
		int index = 0;
		const QList<TextChar>& tchar = textChar[line];

		QTransform tr;
		tr.translate(center.x(), center.y());

		for (const TextChar& tc : tchar) {

			if (!hasTextBoxStyle()) {
				if (tc.format.foreground() == QBrush())
					painter->setBrush(QBrush(textPen().color()));
				else
					painter->setBrush(tc.format.foreground());
				painter->setPen(tc.format.penProperty(QTextFormat::OutlinePen)); // QPen(Qt::transparent));
			}

			for (int i = 0; i < tc.indexes.size(); ++i) {
				double last_rotation = 360 - pie.endAngle() + start_offset_angle[line] + angle_positions[line][index];
				tr.rotate(last_rotation);

				double radius = radiuses[line][index];
				tr.translate(radius, 0);

				double char_angle = 90;
				if (dir == TowardOutside)
					char_angle = -90;
				tr.rotate(char_angle);

				QTransform painter_tr = painter->transform();
				painter->setTransform(tr, true);
				QPainterPath character = tc.raw_font.pathForGlyph(tc.indexes[i]);

				if (!hasTextBoxStyle())
					painter->drawPath(character);
				else {
					textBStyle.computePath(character);
					textBStyle.draw(painter);
				}

				painter->setTransform(painter_tr);

				tr.rotate(-char_angle);
				tr.translate(-radius, 0);
				tr.rotate(-last_rotation);
				++index;
			}
		}
	}

	painter->restore();
}

/// Find the text engine for a text format
///
/// In case of VipText::AutoText the first text engine
/// (beside VipPlainTextEngine) is returned, where VipTextEngine::mightRender
/// returns true. If there is none VipPlainTextEngine is returned.
///
/// If no text engine is registered for the format VipPlainTextEngine
/// is returnd.
///
/// \param text VipText, needed in case of AutoText
/// \param format VipText format
///
/// \return Corresponding text engine
const VipTextEngine* VipText::textEngine(const QString& text, VipText::TextFormat format)
{
	return TextEngineDict::dict().textEngine(text, format);
}

/// Assign/Replace a text engine for a text format
///
/// With setTextEngine it is possible to extend  with
/// other types of text formats.
///
/// For VipText::PlainText it is not allowed to assign a engine == nullptr.
///
/// \param format VipText format
/// \param engine VipText engine
///
/// \sa MathMLTextEngine
/// \warning Using VipText::AutoText does nothing.
void VipText::setTextEngine(VipText::TextFormat format, VipTextEngine* engine)
{
	TextEngineDict::dict().setTextEngine(format, engine);
}

/// \brief Find the text engine for a text format
///
/// textEngine can be used to find out if a text format is supported.
///
/// \param format VipText format
/// \return The text engine, or nullptr if no engine is available.
const VipTextEngine* VipText::textEngine(VipText::TextFormat format)
{
	return TextEngineDict::dict().textEngine(format);
}

QByteArray vipToHtml(const QPixmap& pixmap, const QByteArray& additional_attributes, bool* ok)
{
	QByteArray array;
	QBuffer buffer(&array);
	buffer.open(QBuffer::WriteOnly);
	bool save = pixmap.save(&buffer, "PNG");
	if (ok)
		*ok = save;
	return "<img src='data:image/png;base64," + array.toBase64() + "' " + additional_attributes + " >";
}

QByteArray vipToHtml(const QImage& image, const QByteArray& additional_attributes, bool* ok)
{
	QByteArray array;
	QBuffer buffer(&array);
	buffer.open(QBuffer::WriteOnly);
	bool save = image.save(&buffer, "PNG");
	if (ok)
		*ok = save;
	return "<img src='data:image/png;base64," + array.toBase64() + "' " + additional_attributes + " >";
}

QByteArray vipToHtml(const QPicture& picture, const QByteArray& additional_attributes, bool* ok)
{
	QRectF bounding = picture.boundingRect();
	QPixmap pix(bounding.width(), bounding.height());
	QPainter painter(&pix);
	painter.setTransform(QTransform().translate(-bounding.left(), -bounding.top()));
	const_cast<QPicture&>(picture).play(&painter);
	return vipToHtml(pix, additional_attributes, ok);
}

class VipTextObject::PrivateData
{
public:
	VipText text;
	QTransform transform;
	QPainterPath shape;
	QVariant data;
	bool dirtyShape;

	QRectF rect;

	VipPie pie;
	QPointF center;
	VipText::TextDirection dir;

	PrivateData()
	  : dirtyShape(1)
	  , dir(VipText::AutoDirection)
	{
	}
};

VipTextObject::VipTextObject(const VipText& text, const QRectF& rect, const QTransform& tr)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->text = text;
	d_data->rect = rect;
	d_data->transform = tr;
}

VipTextObject::VipTextObject(const VipText& text, const VipPie& pie, const QPointF& center, VipText::TextDirection dir, const QTransform& tr)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->text = text;
	d_data->pie = pie;
	d_data->dir = dir;
	d_data->center = center;
	d_data->transform = tr;
}

VipTextObject::VipTextObject(const VipTextObject& other)
{
	VIP_CREATE_PRIVATE_DATA(d_data, *other.d_data);
}

VipTextObject::~VipTextObject() = default;

VipTextObject& VipTextObject::operator=(const VipTextObject& other)
{
	*d_data = *other.d_data;
	return *this;
}

void VipTextObject::setText(const VipText& text)
{
	d_data->text = text;
	d_data->dirtyShape = 1;
}

const VipText& VipTextObject::text() const
{
	return d_data->text;
}

VipText& VipTextObject::text()
{
	d_data->dirtyShape = 1;
	return d_data->text;
}

void VipTextObject::setRect(const QRectF& rect)
{
	d_data->dirtyShape = 1;
	d_data->rect = rect;
}

const QRectF& VipTextObject::rect() const
{
	return d_data->rect;
}

void VipTextObject::setPie(const VipPie& pie)
{
	d_data->dirtyShape = 1;
	d_data->rect = QRectF();
	d_data->pie = pie;
}

void VipTextObject::setPie(const VipPie& pie, const QPointF& c)
{
	d_data->dirtyShape = 1;
	d_data->rect = QRectF();
	d_data->pie = pie;
	d_data->center = c;
}

void VipTextObject::setPie(const VipPie& pie, const QPointF& c, VipText::TextDirection dir)
{
	d_data->dirtyShape = 1;
	d_data->rect = QRectF();
	d_data->pie = pie;
	d_data->dir = dir;
	d_data->center = c;
}

const VipPie& VipTextObject::pie() const
{
	return d_data->pie;
}

void VipTextObject::setCenter(const QPointF& c)
{
	d_data->dirtyShape = 1;
	d_data->center = c;
}

const QPointF& VipTextObject::center() const
{
	return d_data->center;
}

void VipTextObject::setTextDirection(VipText::TextDirection dir)
{
	d_data->dirtyShape = 1;
	d_data->dir = dir;
}

VipText::TextDirection VipTextObject::textDirection() const
{
	return d_data->dir;
}

void VipTextObject::setTransform(const QTransform& tr)
{
	d_data->dirtyShape = 1;
	d_data->transform = tr;
}

const QTransform& VipTextObject::transform() const
{
	return d_data->transform;
}

const QPainterPath& VipTextObject::shape() const
{
	if (d_data->dirtyShape) {
		VipShapeDevice device;
		QPainter painter(&device);
		draw(&painter);
		const_cast<VipTextObject&>(*this).d_data->shape = device.shape();
		d_data->dirtyShape = 0;
	}

	return d_data->shape;
}

void VipTextObject::draw(QPainter* painter) const
{
	if (d_data->rect.isEmpty()) {
		painter->setTransform(d_data->transform, true);
		d_data->text.draw(painter, d_data->center, d_data->pie, d_data->dir);
	}
	else {
		painter->setTransform(d_data->transform, true);
		d_data->text.draw(painter, d_data->rect);
	}
}

#include <QDataStream>

QDataStream& operator<<(QDataStream& stream, const VipTextStyle& style)
{
	return stream << style.font() << style.textPen() << (int)style.alignment() << (int)style.renderHints() << style.boxStyle();
}

QDataStream& operator>>(QDataStream& stream, VipTextStyle& style)
{
	QFont font;
	QPen pen;
	int alignment, renderHints;
	VipBoxStyle boxStyle;
	stream >> font >> pen >> alignment >> renderHints >> boxStyle;
	style.setFont(font);
	style.setTextPen(pen);
	style.setAlignment(Qt::Alignment(alignment));
	style.setRenderHints(QPainter::RenderHints(renderHints));
	style.setBoxStyle(boxStyle);
	return stream;
}

QDataStream& operator<<(QDataStream& stream, const VipText& text)
{
	return stream << text.textStyle() << text.text();
}

QDataStream& operator>>(QDataStream& stream, VipText& text)
{
	QString t;
	VipTextStyle style;
	stream >> style >> t;
	text.setText(t);
	text.setTextStyle(style);
	return stream;
}

static int registerStreamOperators()
{
	qRegisterMetaTypeStreamOperators<VipTextStyle>("VipTextStyle");
	qRegisterMetaTypeStreamOperators<VipText>("VipText");
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();

// make the the types declared with Q_DECLARE_METATYPE are registered
static int reg1 = qMetaTypeId<VipTextStyle>();
static int reg2 = qMetaTypeId<VipTextList>();
static int reg3 = qMetaTypeId<VipText>();
static int reg4 = qMetaTypeId<VipTextObject>();
