/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025 Victor Moncada
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

#include "QThreadOpenGLWidget.h"

#include <mutex>
#include <functional>

#include <QPicture>
#include <qapplication.h>
#include <qboxlayout.h>
#include <qcolor.h>
#include <qdatetime.h>
#include <qevent.h>
#include <qgraphicsview.h>
#include <qguiapplication.h>
#include <qopenglcontext.h>
#include <qopenglpaintdevice.h>
#include <QOpenGLWidget>
#include <qpaintengine.h>
#include <qpainter.h>
#include <qpainterpath.h>
#include <qpointer.h>
#include <qscreen.h>
#include <qscrollarea.h>
#include <qthread.h>
#include <qwindow.h>
#include <qmutex.h>
#include <QOpenGLFunctions>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <qwaitcondition.h>
#include <QTextDocument>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>

// Default move copy/construct
#define DEFAULT_MOVE(Type)                                                                                                                                                                             \
	Type(const Type& other) = default;                                                                                                                                                             \
	Type(Type&& other) noexcept = default;                                                                                                                                                         \
	Type& operator=(const Type&) = default;                                                                                                                                                        \
	Type& operator=(Type&&) noexcept = default

// Strongest available function inlining
#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__MINGW32__)
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(__GNUC__)
#define ALWAYS_INLINE inline
#elif (defined _MSC_VER) || (defined __INTEL_COMPILER)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif

// Size in bytes of a CommandBatch
#define COMMAND_SIZE (4096 * 8)

//
// Items stored as paint commands
//

struct TiledPixmapItem
{
	// Tiled pixmap
	QRectF rect;
	QPixmap pixmap;
	QPointF pos;
	TiledPixmapItem() = default;
	DEFAULT_MOVE(TiledPixmapItem);
};
static QRectF itemBoundingRect(const TiledPixmapItem& it) noexcept
{
	return it.rect;
}

struct PixmapItem
{
	// QPixmap item
	QRectF rect;
	QPixmap pixmap;
	QRectF src;
	PixmapItem() = default;
	DEFAULT_MOVE(PixmapItem);
};
static QRectF itemBoundingRect(const PixmapItem& it) noexcept
{
	return it.rect;
}

struct ImageItem
{
	// QImage item
	QRectF rect;
	QImage image;
	QRectF src;
	Qt::ImageConversionFlags flags;
	ImageItem() = default;
	DEFAULT_MOVE(ImageItem);
};
static QRectF itemBoundingRect(const ImageItem& it) noexcept
{
	return it.rect;
}

struct TextItem
{
	// QString item
	QString item;
	QPointF pos;
	QFont font;
	QRectF boundingRect;
	TextItem() = default;
	DEFAULT_MOVE(TextItem);
};

template<class T>
struct StaticVector
{
	// Vector of items (rects, lines, points...)
	// using one static item

	using iterator = const T*;
	using const_iterator = iterator;
	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using size_type = qint64;
	using difference_type = qint64;

	T one;
	qint64 count{ 0 };
	QVector<T> vec;

	StaticVector() {}
	StaticVector(const StaticVector&) = delete;
	StaticVector(StaticVector&& other)
	  : one(std::move(other.one))
	  , count(other.count)
	  , vec(other.vec)
	{
		other.count = 0;
	}
	StaticVector(T&& value) noexcept
	  : one(std::move(value))
	  , count(1)
	{
	}
	template<class Iter>
	StaticVector(Iter first, Iter last)
	{
		auto len = std::distance(first, last);
		if (len == 0)
			return;
		if (len == 1) {
			pushBack(*first);
			return;
		}
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		vec.resize(len);
		std::copy(first, last, vec.begin());
#else
		vec = QVector<T>(first, last);
#endif
		count = (qint64)len;
	}

	const T* data() const noexcept { return count == 1 ? &one : vec.data(); }
	size_t size() const noexcept { return count; }
	const T& back() const noexcept { return count == 1 ? one : vec.back(); }
	T& back() noexcept { return count == 1 ? one : vec.back(); }
	iterator begin() const noexcept { return data(); }
	iterator end() const noexcept { return data() + count; }
	template<class U>
	void pushBack(U&& v)
	{
		if (!count) {
			one = std::forward<U>(v);
			count = 1;
			return;
		}
		if (count == 1)
			vec.push_back(std::move(one));
		vec.push_back(std::forward<U>(v));
		++count;
	}
	template<class U>
	void append(const StaticVector<U>& vec)
	{
		for (const U& v : vec)
			pushBack(v);
	}
	template<class U>
	void append(StaticVector<U>&& vec)
	{
		for (U& v : vec)
			pushBack(std::move(v));
	}
	template<class U>
	void append(const U* vec, int count)
	{
		for (int i = 0; i < count; ++i)
			pushBack(vec[i]);
	}
};

template<class T>
using DrawVector = StaticVector<T>;
using LineVector = DrawVector<QLine>;
using LineFVector = DrawVector<QLineF>;
using PointVector = DrawVector<QPoint>;
using PointFVector = DrawVector<QPointF>;
using RectVector = DrawVector<QRect>;
using RectFVector = DrawVector<QRectF>;

using FunctionItem = std::function<void(QPainter*)>;

struct ClipPath
{
	// QPainterPath clipping
	QPainterPath path;
	qint64 operation;
};
struct ClipRegion
{
	// QRegion clipping
	QRegion region;
	qint64 operation;
};

struct ClipRect
{
	// Clip rectangle used for partial or full update
	QRectF rect;
	QBrush brush;
};

template<class T, class U>
ALWAYS_INLINE DrawVector<T> toVector(const U* begin, const U* end)
{
	// Convert range to StaticVector
	return DrawVector<T>(begin, end);
}

//
// Bounding rectangle of common drawing items
//

static QRectF itemBoundingRect(const QPointF& it) noexcept
{
	return QRectF(it, QSizeF(1, 1));
}
static QRectF itemBoundingRect(const QRectF& it) noexcept
{
	return it;
}
static QRectF itemBoundingRect(const QPainterPath& it) noexcept
{
	return it.boundingRect();
}
static QRectF itemBoundingRect(const QLineF& it) noexcept
{
	return QRectF(it.p1(), it.p2()).normalized();
}

template<class T>
static QRectF itemBoundingRect(const StaticVector<T>& it) noexcept
{
	if (it.count == 1)
		return itemBoundingRect(it.one);
	QRectF r;
	for (const T& v : it)
		r = r.united(itemBoundingRect(v));
	return r;
}
template<class T>
static QRectF itemBoundingRect(const T* items, int count) noexcept
{
	QRectF r;
	for (int i = 0; i < count; ++i)
		r = r.united(itemBoundingRect(items[i]));
	return r;
}

static QRectF itemBoundingRect(const TextItem& it) noexcept
{
	if (!it.boundingRect.isEmpty())
		return it.boundingRect;
	const QFontMetricsF fm(it.font);
	return const_cast<TextItem&>(it).boundingRect = fm.boundingRect(QRectF(0, 0, QWIDGETSIZE_MAX, QWIDGETSIZE_MAX), Qt::AlignTop | Qt::AlignLeft, it.item).translated(it.pos);
}

//
// Helper functions
//

static bool isAscii(const QString& str)
{
	// Check if string is ascii and not rich
	if (Qt::mightBeRichText(str))
		return false;
	for (QChar c : str)
		if (c.unicode() > 127)
			return false;
	return true;
}

struct CharTable
{
	QRectF rects[128];
};

static const CharTable& charTable(const QFont& f)
{
	static QHash<QFont, CharTable> width;
	auto it = width.find(f);
	if (it == width.end()) {
		QFontMetrics m(f);
		CharTable res;
		for (int i = 0; i < 128; ++i) {
			QRectF w = m.boundingRect(QChar(i));
			res.rects[i] = w.translated(-w.topLeft());
		}
		return width.insert(f, res).value();
	}
	return it.value();
}

static QRectF estimateStringRect(const QString& text, const QFont& f)
{
	// Estimate text bounding rect, only works for non rich ascii text.
	const CharTable& table = charTable(f);
	QRectF res;
	for (QChar c : text) {
		QRectF r = table.rects[c.unicode()];
		res.setWidth(res.width() + r.width());
		if (r.height() > res.height())
			res.setHeight(r.height());
	}
	return res;
}

static bool isStraight(const QLineF& line) noexcept
{
	// Check if line is horizontal or vertical
	return line.p1().x() == line.p2().x() || line.p1().y() == line.p2().y();
}
static void drawLinesAliasing(QPainter* p, const QLineF* data, int size)
{
	// Draw lines and remove antialiazing for vertical or horizontal ones
	if (size == 0)
		return;
	auto hint = p->testRenderHint(QPainter::Antialiasing);
	int start = 0;
	bool straight = isStraight(data[0]);
	for (int i = 1; i < size; ++i) {
		bool str = isStraight(data[i]);
		if (str != straight) {
			// draw previous
			p->setRenderHint(QPainter::Antialiasing, !straight);
			p->drawLines(data + start, i - start);
			start = i;
			straight = str;
		}
	}
	// draw last lines
	p->setRenderHint(QPainter::Antialiasing, !straight);
	p->drawLines(data + start, size - start);
	if (straight == hint)
		p->setRenderHint(QPainter::Antialiasing, hint);
}

static bool isVectoriel(QPainter* painter) noexcept
{
	// Check if painter draws on a vectoriel engine
	return painter->paintEngine() && (painter->paintEngine()->type() == QPaintEngine::SVG || painter->paintEngine()->type() == QPaintEngine::MacPrinter ||
					  painter->paintEngine()->type() == QPaintEngine::Picture || painter->paintEngine()->type() == QPaintEngine::Pdf
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
					  || painter->paintEngine()->type() == QPaintEngine::PostScript
#endif
					 );
}

class CommandBatch;

// Base class for CommandBatch
struct BaseCommandBatch
{
	BaseCommandBatch* prev;		   // Linked list of CommandBatch
	BaseCommandBatch* next;		   // Linked list of CommandBatch
	std::uint16_t count{ 0 };	   // Number of commands
	std::uint16_t tail{ 0 };	   // Command insertion tail
	std::uint16_t tail_end{ 0 };	   // Command Id insertion tail
	std::atomic<bool> finish{ false }; // Finished (full) command
	BaseCommandBatch() { prev = next = this; }
	virtual ~BaseCommandBatch() noexcept {}
	ALWAYS_INLINE CommandBatch* command() noexcept { return reinterpret_cast<CommandBatch*>(this); }
	ALWAYS_INLINE bool isSeparator() const noexcept { return tail == COMMAND_SIZE; }

	void erase()
	{
		prev->next = next;
		next->prev = prev;
		next = prev = this;
	}
	void insert(BaseCommandBatch* after)
	{
		prev = after;
		next = after->next;
		after->next = next->prev = this;
	}
};

// Structure holding a vector of paint commands.
// The commands themselves are stored from the beginning
// of the internal data. The command ids are stored from
// the tail of the internal data in reverse order.
//
class CommandBatch : public BaseCommandBatch
{
public:
	// Types of paint command
	enum Type
	{
		DrawNone = 0,
		DrawPixmap,
		DrawTiledPixmap,
		DrawImage,
		DrawLinesF,
		DrawPointsF,
		DrawRectsF,
		DrawEllipseF,
		DrawPath,
		DrawOddPolygonF,
		DrawWiddingPolygonF,
		DrawPolylineF,
		DrawTextItem,
		DrawRecord,
		DrawClipRect,
		DrawViewBackground,
		DrawFunction,
		DirtyTransform,
		DirtyBrush,
		DirtyBrushOrigin,
		DirtyBackground,
		DirtyBackgroundMode,
		DirtyClipPath,
		DirtyClipRegion,
		DirtyCompositionMode,
		DirtyFont,
		DirtyClipEnabled,
		DirtyPen,
		DirtyHints,
		DirtyOpacity
	};

	struct Entry
	{
		// Entry in the command vector
		int cmd{ 0 };		// Command id
		int cmd_tail{ 0 };	// tail position for the command id
		void* value{ nullptr }; // Actual command value (PixmapItem, QPainterPath, QBrush ...)
		int commandType() const noexcept { return cmd; }
	};

	// Raw buffer of commands.
	// Command values (PixmapItem, QPainterPath, QBrush ...)
	// start at the head of the buffer.
	// Command ids start at the tail in reverse order.
	char data[COMMAND_SIZE - sizeof(BaseCommandBatch)];

	static int bytes(int cmd) noexcept
	{
		// Returns the command value size in bytes
		switch (cmd) {
			case DrawPixmap:
				return sizeof(PixmapItem);
			case DrawClipRect:
			case DrawViewBackground:
				return sizeof(ClipRect);
			case DrawTiledPixmap:
				return sizeof(TiledPixmapItem);
			case DrawImage:
				return sizeof(ImageItem);
			case DrawLinesF:
				return sizeof(LineFVector);
			case DrawRectsF:
				return sizeof(RectFVector);
			case DrawEllipseF:
				return sizeof(QRectF);
			case DrawPath:
				return sizeof(QPainterPath);
			case DrawRecord:
				return sizeof(QPaintRecord);
			case DrawFunction:
				return sizeof(FunctionItem);
			case DirtyTransform:
				return sizeof(QTransform);
			case DirtyBrush:
				return sizeof(QBrush);
			case DirtyBrushOrigin:
				return sizeof(QPointF);
			case DirtyBackground:
				return sizeof(QBrush);
			case DirtyBackgroundMode:
				return 8;
			case DirtyClipPath:
				return sizeof(ClipPath);
			case DirtyClipRegion:
				return sizeof(ClipRegion);
			case DirtyCompositionMode:
				return 8;
			case DirtyFont:
				return sizeof(QFont);
			case DirtyClipEnabled:
				return 8;
			case DirtyPen:
				return sizeof(QPen);
			case DirtyHints:
				return 8;
			case DirtyOpacity:
				return 8;
			case DrawPolylineF:
			case DrawWiddingPolygonF:
			case DrawOddPolygonF:
			case DrawPointsF:
				return sizeof(PointFVector);
			case DrawTextItem:
				return sizeof(TextItem);
			default:
				return 0;
		}
	}
	static QRectF boundingRect(const Entry& e) noexcept
	{
		// Returns the command bounding rect, if any
		switch (e.cmd) {
			case DrawPixmap:
				return itemBoundingRect(*static_cast<PixmapItem*>(e.value));
			case DrawTiledPixmap:
				return itemBoundingRect(*static_cast<TiledPixmapItem*>(e.value));
			case DrawImage:
				return itemBoundingRect(*static_cast<ImageItem*>(e.value));
			case DrawLinesF:
				return itemBoundingRect(*static_cast<LineFVector*>(e.value));
			case DrawRectsF:
				return itemBoundingRect(*static_cast<RectFVector*>(e.value));
			case DrawEllipseF:
				return itemBoundingRect(*static_cast<QRectF*>(e.value));
			case DrawPath:
				return itemBoundingRect(*static_cast<QPainterPath*>(e.value));
			case DrawRecord:
				return static_cast<QPaintRecord*>(e.value)->estimateBoundingRect();
			case DrawPolylineF:
			case DrawWiddingPolygonF:
			case DrawOddPolygonF:
			case DrawPointsF:
				return itemBoundingRect(*static_cast<PointFVector*>(e.value));
			case DrawTextItem:
				return itemBoundingRect(*static_cast<TextItem*>(e.value));
			default:
				return QRectF();
		}
	}

	ALWAYS_INLINE Entry next(const Entry& current) const noexcept
	{
		// Next command
		int next_cmd = data[current.cmd_tail - 1];
		if (next_cmd == 0)
			return Entry{ 0, 0, nullptr };
		char* next = (char*)current.value + bytes(current.cmd);
		return Entry{ next_cmd, current.cmd_tail - 1, next };
	}
	ALWAYS_INLINE Entry begin() const noexcept
	{
		// First command
		int next_cmd = data[sizeof(data) - 1];
		if (next_cmd == 0)
			return Entry{ 0, 0, nullptr };
		return Entry{ next_cmd, sizeof(data) - 1, const_cast<char*>(data) };
	}

	ALWAYS_INLINE Entry back() const noexcept
	{
		// Returns the last command
		return Entry{ data[tail_end + 1], tail_end + 1, const_cast<char*>(data + tail - bytes(data[tail_end + 1])) };
	}

	ALWAYS_INLINE int backType() const noexcept
	{
		// Returns the last command type
		return data[tail_end + 1];
	}

	void destroy(Entry e)
	{
		// Destroy a command
		switch (e.cmd) {
			case DrawPixmap:
				static_cast<PixmapItem*>(e.value)->~PixmapItem();
				break;
			case DrawTiledPixmap:
				static_cast<TiledPixmapItem*>(e.value)->~TiledPixmapItem();
				break;
			case DrawImage:
				static_cast<ImageItem*>(e.value)->~ImageItem();
				break;
			case DrawLinesF:
				static_cast<LineFVector*>(e.value)->~LineFVector();
				break;
			case DrawRectsF:
				static_cast<RectFVector*>(e.value)->~RectFVector();
				break;
			case DrawPath:
				static_cast<QPainterPath*>(e.value)->~QPainterPath();
				break;
			case DrawClipRect:
			case DrawViewBackground:
				static_cast<ClipRect*>(e.value)->~ClipRect();
				break;
			case DrawRecord:
				static_cast<QPaintRecord*>(e.value)->~QPaintRecord();
				break;
			case DrawFunction:
				static_cast<FunctionItem*>(e.value)->~FunctionItem();
				break;
			case DirtyTransform:
				static_cast<QTransform*>(e.value)->~QTransform();
				break;
			case DirtyBackground:
			case DirtyBrush:
				static_cast<QBrush*>(e.value)->~QBrush();
				break;
			case DirtyClipPath:
				static_cast<ClipPath*>(e.value)->~ClipPath();
				break;
			case DirtyClipRegion:
				static_cast<ClipRegion*>(e.value)->~ClipRegion();
				break;
			case DirtyFont:
				static_cast<QFont*>(e.value)->~QFont();
				break;
			case DirtyPen:
				static_cast<QPen*>(e.value)->~QPen();
				break;
			case DrawPolylineF:
			case DrawWiddingPolygonF:
			case DrawOddPolygonF:
			case DrawPointsF:
				static_cast<PointFVector*>(e.value)->~PointFVector();
				break;
			case DrawTextItem:
				static_cast<TextItem*>(e.value)->~TextItem();
				break;
			default:
				break;
		}
	}

	static ALWAYS_INLINE bool removeAntialiazing(QPainter* p) noexcept
	{
		// Check if we can remove antialiazing when drawing lines and rectangles
		const QTransform& tr = p->transform();
		const QPen& pen = p->pen();
		if (tr.isRotating())
			return false;
		if (pen.isCosmetic())
			return true;
		if (tr.isScaling())
			return false;
		return pen.widthF() == 1.;
	}

	CommandBatch() noexcept
	{
		tail_end = sizeof(data) - 1;
		data[tail_end] = 0;
	}

	virtual ~CommandBatch() noexcept
	{
		// Destroy all commands
		Entry e = begin();
		int destroyed = 0;
		while (e.value) {
			destroy(e);
			e = next(e);
			++destroyed;
		}
		Q_ASSERT(destroyed == count);
	}

	template<class T>
	bool pushBack(int cmd, T&& value)
	{
		// Append a command
		using value_type = typename std::decay<T>::type;
		static constexpr std::uint16_t val_size = static_cast<std::uint16_t>(sizeof(value_type));
		static_assert(val_size >= 8, "");

		if (tail + val_size > tail_end - 1) {
			finish = true;
			// Returns false if no more room
			return false;
		}

		new (data + tail) value_type(std::forward<T>(value));
		tail += val_size;
		data[tail_end--] = cmd;
		data[tail_end] = 0;
		++count;
		return true;
	}

	QRectF boundingRect() const
	{
		// Returns the command batch bounding rectangle
		if (isSeparator())
			return QRectF();

		QRectF res;
		Entry e = begin();
		while (e.value) {
			res = res.united(boundingRect(e));
			e = next(e);
		}
		return res;
	}

	QRectF estimateBoundingRect() const
	{
		// Returns the command batch bounding rectangle estimation.
		// Faster version of boundingRect(), where rich or non ascii
		// text items are ignored.

		if (isSeparator())
			return QRectF();

		QRectF res;
		Entry e = begin();
		while (e.value) {
			if (e.cmd == DrawTextItem) {
				const TextItem& it = *static_cast<TextItem*>(e.value);
				if (isAscii(it.item))
					res = res.united(estimateStringRect(it.item, it.font).translated(it.pos));
			}
			else
				res = res.united(boundingRect(e));
			e = next(e);
		}
		return res;
	}

	static qint64 read64(void* p, int offset) noexcept { return *reinterpret_cast<qint64*>((char*)p + offset); }

	void apply(QPainter* p, QPaintRecord::Optimizations opt, const QTransform& worldMatrix = QTransform(), QThreadOpenGLWidget* widget = nullptr, bool drawBackground = true) const
	{
		// Apply recorded commands on given QPainter,
		// using provided optimizations and starting world transform.

		if (isSeparator())
			return;

		const bool vectoriel = isVectoriel(p);

		Entry e = begin();
		while (e.value) {
			switch (e.cmd) {

				case DrawClipRect:
				case DrawViewBackground: {
					ClipRect& r = *static_cast<ClipRect*>(e.value);
					if (!r.rect.isEmpty() && drawBackground) {
						p->setClipRect(r.rect);
						p->fillRect(r.rect, r.brush);
					}
					// call paintGL() after the background rectangle, and only once
					if (widget) {
						p->beginNativePainting();
						widget->paintGL();
						p->endNativePainting();
						widget = nullptr;
					}
				} break;
				case DrawRecord: {
					QPaintRecord& pic = *static_cast<QPaintRecord*>(e.value);
					pic.play(p, opt);
				} break;
				case DrawFunction: {
					FunctionItem& fun = *static_cast<FunctionItem*>(e.value);
					fun(p);
				} break;
				case DrawTiledPixmap: {
					const TiledPixmapItem& pix = *static_cast<TiledPixmapItem*>(e.value);
					p->drawTiledPixmap(pix.rect, pix.pixmap, pix.pos);
				} break;
				case DrawPixmap: {
					const PixmapItem& item = *static_cast<PixmapItem*>(e.value);
					if (item.rect.size() == item.src.size() && item.pixmap.size() == item.src.size().toSize() && item.src.topLeft() == QPointF(0, 0))
						p->drawPixmap(item.rect.topLeft(), item.pixmap);
					else
						p->drawPixmap(item.rect, item.pixmap, item.src);
				} break;
				case DrawImage: {
					const ImageItem& item = *static_cast<ImageItem*>(e.value);
					p->drawImage(item.rect, item.image, item.src, item.flags);
				} break;
				case DrawLinesF: {
					const LineFVector& vec = *static_cast<LineFVector*>(e.value);
					if ((opt & QPaintRecord::StraightLineAliasing) && removeAntialiazing(p))
						drawLinesAliasing(p, vec.data(), (int)vec.size());
					else
						p->drawLines(vec.data(), (int)vec.size());

				} break;
				case DrawPointsF: {
					const PointFVector& vec = *static_cast<PointFVector*>(e.value);
					p->drawPoints(vec.data(), (int)vec.size());
				} break;
				case DrawRectsF: {
					const RectFVector& vec = *static_cast<RectFVector*>(e.value);
					if ((opt & QPaintRecord::StraightLineAliasing) && removeAntialiazing(p)) {
						auto hint = p->testRenderHint(QPainter::Antialiasing);
						if (hint)
							p->setRenderHint(QPainter::Antialiasing, false);
						p->drawRects(vec.data(), (int)vec.size());
						if (hint)
							p->setRenderHint(QPainter::Antialiasing, true);
					}
					else {
						// auto mode = p->compositionMode();
						// TEST: This commented line is necessary for on going opengl backend for qwidget
						// p->setCompositionMode(QPainter::CompositionMode_SourceOver);
						p->drawRects(vec.data(), (int)vec.size());
					}
				} break;
				case DrawEllipseF:
					p->drawEllipse(*static_cast<QRectF*>(e.value));
					break;
				case DrawPath:
					p->drawPath(*static_cast<QPainterPath*>(e.value));
					break;
				case DrawOddPolygonF: {
					const PointFVector& vec = *static_cast<PointFVector*>(e.value);
					p->drawPolygon(vec.data(), (int)vec.size());
				} break;
				case DrawWiddingPolygonF: {
					const PointFVector& vec = *static_cast<PointFVector*>(e.value);
					p->drawPolygon(vec.data(), (int)vec.size(), Qt::WindingFill);
				} break;
				case DrawPolylineF: {
					const PointFVector& vec = *static_cast<PointFVector*>(e.value);
					p->drawPolyline(vec.data(), (int)vec.size());
				} break;
				case DirtyTransform:
					p->setTransform(*static_cast<QTransform*>(e.value) * worldMatrix);
					break;
				case DirtyBrush:
					p->setBrush(*static_cast<QBrush*>(e.value));
					break;
				case DirtyBrushOrigin:
					p->setBrushOrigin(*static_cast<QPointF*>(e.value));
					break;
				case DirtyBackground:
					p->setBackground(*static_cast<QBrush*>(e.value));
					break;
				case DirtyBackgroundMode:
					p->setBackgroundMode((Qt::BGMode) * static_cast<qint64*>(e.value));
					break;
				case DirtyClipPath:
					p->setClipPath(static_cast<ClipPath*>(e.value)->path, (Qt::ClipOperation) static_cast<ClipPath*>(e.value)->operation);
					break;
				case DirtyClipRegion:
					p->setClipRegion(static_cast<ClipRegion*>(e.value)->region, (Qt::ClipOperation) static_cast<ClipRegion*>(e.value)->operation);
					break;
				case DirtyCompositionMode:
					p->setCompositionMode((QPainter::CompositionMode) * static_cast<qint64*>(e.value));
					break;
				case DirtyFont:
					p->setFont(*static_cast<QFont*>(e.value));
					break;
				case DirtyClipEnabled:
					p->setClipping((bool)*static_cast<qint64*>(e.value));
					break;
				case DirtyPen:
					p->setPen(*static_cast<QPen*>(e.value));
					break;
				case DirtyHints: {
					QPainter::RenderHints hints = (QPainter::RenderHints) static_cast<QPainter::RenderHints::Int>(*static_cast<qint64*>(e.value));
					p->setRenderHints(hints, true);
					p->setRenderHints(~hints, false);
				} break;
				case DirtyOpacity:
					p->setOpacity(*static_cast<double*>(e.value));
					break;
				case DrawTextItem: {
					const TextItem& item = *static_cast<TextItem*>(e.value);
					const QTransform& tr = p->transform();
					const QFont f = item.font;
					if (f != p->font())
						p->setFont(f);

					if ((opt & QPaintRecord::SmallTextAsLine) && !vectoriel) {

						// Render text as lines
						QLineF line;
						const QFontMetricsF fm(f);
						QRectF text_rect = itemBoundingRect(item);
						QPointF top = tr.map(text_rect.topLeft());
						QPointF bottom = tr.map(text_rect.bottomLeft());
						double pr = (top - bottom).manhattanLength();
						if (pr < 5) {
							// draw lines instead
							line = (QLineF(text_rect.topLeft(), text_rect.topRight()));
							if ((opt & QPaintRecord::StraightLineAliasing) && removeAntialiazing(p))
								drawLinesAliasing(p, &line, 1);
							else
								p->drawLine(line);
							break;
						}
					}
					p->drawText(item.pos, item.item);
				} break;
				default:
					break;
			}
			e = next(e);
		}
	}
};

#ifdef DEBUG_PAINT_GL_WIDGET
static std::atomic<qint64> cmd_count{ 0 };
static qint64 cmd_time = 0;
#endif

static ALWAYS_INLINE CommandBatch* makeCommand()
{
	// Allocate/construct a CommandBatch
#ifdef DEBUG_PAINT_GL_WIDGET
	++cmd_count;
#endif
	return new CommandBatch();
}
static ALWAYS_INLINE BaseCommandBatch* makeSeparator()
{
	// Create a separator used separate commands
	// between multiple paintEvent().
#ifdef DEBUG_PAINT_GL_WIDGET
	++cmd_count;
#endif
	BaseCommandBatch* r = new BaseCommandBatch();
	r->tail = COMMAND_SIZE;
	r->finish = true;
	return r;
}
static ALWAYS_INLINE void destroyCommand(BaseCommandBatch* c)
{
	// Destroy a BaseCommandBatch
#ifdef DEBUG_PAINT_GL_WIDGET
	--cmd_count;
	qint64 time = QDateTime::currentMSecsSinceEpoch();
	if (time - cmd_time > 1000) {
		cmd_time = time;
		printf("CommandBatch count: %i\n", (int)cmd_count.load());
	}
#endif
	delete c;
}

// RenderingThread safe FIFO of CommandBatch
class CommandQueue
{
	BaseCommandBatch end;	     // end node for the linked list of BaseCommandBatch
	std::atomic<int> count{ 0 }; // total number of commands
	QRectF bRect;		     // exact bounding rectangle
	QRectF eRect;		     // estimated bounding rectangle
	QMutex lock;		     // mutex

	template<class T>
	bool emplaceBackSlow(int type, T&& cmd)
	{
		CommandBatch* c = makeCommand();
		c->pushBack(type, std::forward<T>(cmd));
		std::lock_guard<QMutex> ll(lock);
		c->insert(end.prev);
		++count;
		return true;
	}

	void invalidateRects() noexcept { bRect = eRect = QRectF(); }

public:
	ALWAYS_INLINE CommandQueue() noexcept {}
	ALWAYS_INLINE ~CommandQueue() noexcept { clear(); }
	ALWAYS_INLINE int size() const noexcept { return count.load(std::memory_order_relaxed); }
	ALWAYS_INLINE CommandBatch* getLast() noexcept { return end.prev->command(); }
	ALWAYS_INLINE int lastCount() noexcept { return (int)getLast()->count; }
	ALWAYS_INLINE auto back() noexcept { return getLast()->back(); }
	ALWAYS_INLINE auto backType() noexcept { return getLast()->backType(); }

	void clear()
	{
		// Destroy all BaseCommandBatch
		// and reset internal state
		std::lock_guard<QMutex> ll(lock);
		invalidateRects();
		BaseCommandBatch* first = end.next;
		while (first != &end) {
			BaseCommandBatch* next = first->next;
			destroyCommand(first);
			first = next;
		}
		end.next = end.prev = &end;
		count = 0;
	}

	template<class T>
	bool emplaceBack(int type, T&& cmd)
	{
		// Push back a command.
		// Returns true if a CommandBatch allocation was required,
		// false otherwise.
		invalidateRects();
		if (end.prev != &end) {
			if (end.prev->command()->pushBack(type, std::forward<T>(cmd))) {
				++count;
				return false;
			}
		}
		return emplaceBackSlow(type, std::forward<T>(cmd));
	}

	void addSeparator()
	{
		// Add a separator (end of drawing) and a new empty command
		auto* s = makeSeparator();
		auto* c = makeCommand();
		std::lock_guard<QMutex> ll(lock);
		if (end.prev != &end)
			end.prev->finish = true;
		s->insert(end.prev);
		c->insert(end.prev);
	}

	int popFrontUntilSeparator()
	{
		// Remove all front commands until the last separator.
		// If the last batch of commands is unfinished, go back
		// of one additional separator.
		// Partial updates (starting with DrawClipRect) are not removed.

		BaseCommandBatch* first = nullptr;
		BaseCommandBatch* last_sep = nullptr;
		{
			std::lock_guard<QMutex> ll(lock);
			first = end.next;

			// make sure the remaining part does not have a clipping rect
			// as partial updates should not be removed
			if (first != &end && first->command()->begin().cmd == CommandBatch::DrawClipRect)
				return 0;

			BaseCommandBatch* c = end.prev;
			while (c != &end) {
				if (c->isSeparator()) {
					last_sep = c;
					break;
				}
				c = c->prev;
			}

			// no last separator
			if (!last_sep) {
				// printf("no last sep\n");
				return 0;
			}

			// After last sep, there is an unfinished command
			if (last_sep->next == end.prev && !last_sep->next->finish) {
				// Find previous separator
				c = last_sep->prev;
				last_sep = nullptr;
				while (c != &end) {
					if (c->isSeparator()) {
						last_sep = c;
						break;
					}
					c = c->prev;
				}
				if (!last_sep)
					return 0;
			}

			end.next = last_sep->next;
			last_sep->next->prev = &end;
		}
		invalidateRects();

		int res = 0;
		// Remove everything until last_sep
		BaseCommandBatch* c = first;
		while (c != last_sep) {
			BaseCommandBatch* next = c->next;
			c->erase();
			count -= (int)c->count;
			destroyCommand(c);
			c = next;
			++res;
		}
		last_sep->erase();
		destroyCommand(last_sep);
		return res + 1;
	}

	BaseCommandBatch* popCommandFront() noexcept
	{
		// Extract the first batch of commands and return it

		std::lock_guard<QMutex> ll(lock);
		if (end.next != &end && end.next->finish) {
			invalidateRects();
			BaseCommandBatch* c = end.next;
			c->erase();
			count -= (int)c->count;
			return c;
		}
		return nullptr;
	}

	void apply(QPainter* p, QPaintRecord::Optimizations opt, const QTransform& worldMatrix)
	{
		// Apply commands
		std::lock_guard<QMutex> ll(lock);
		BaseCommandBatch* c = end.next;
		while (c != &end) {
			c->command()->apply(p, opt, worldMatrix);
			c = c->next;
		}
	}

	QRectF boundingRect()
	{
		// Full commands bounding rectangle
		QRectF rect;
		std::lock_guard<QMutex> ll(lock);
		if (!bRect.isEmpty())
			return bRect;
		BaseCommandBatch* c = end.next;
		while (c != &end) {
			rect = rect.united(c->command()->boundingRect());
			c = c->next;
		}
		return bRect = rect;
	}

	QRectF estimateBoundingRect()
	{
		// Full commands bounding rectangle estimation
		QRectF rect;
		std::lock_guard<QMutex> ll(lock);
		if (!bRect.isEmpty())
			return bRect;
		if (!eRect.isEmpty())
			return eRect;
		BaseCommandBatch* c = end.next;
		while (c != &end) {
			rect = rect.united(c->command()->estimateBoundingRect());
			c = c->next;
		}
		return eRect = rect;
	}
};

// QPaintEngine class that records painting commands
// in a CommandQueue.
template<class Device>
struct PicturePaintEngine : public QPaintEngine
{
	// Device must provide emplaceBack(), count(), back()
	QPaintEngine::Type d_type;
	QPaintRecord::Optimizations d_optimizations{ QPaintRecord::BatchCommands };
	QRectF d_device_rect; // If valid, used to discard paint commands that are outside the rectangle.
	QTransform d_last_tr;
	Device* d_device;
	bool d_enable{ true };
	std::function<void(bool, QPaintEngine*)> d_begin_end;

public:
	PicturePaintEngine(Device* dev, QPaintEngine::Type type = QPaintEngine::Windows)
	  : QPaintEngine(QPaintEngine::AllFeatures)
	  , d_type(type)
	  , d_device(dev)
	{
	}

	template<class Item>
	bool discard_impl(const Item& it) const noexcept
	{
		QRectF r = itemBoundingRect(it);
		QRectF first(d_last_tr.map(r.topLeft()), d_last_tr.map(r.bottomRight()));
		QRectF second(d_last_tr.map(r.bottomLeft()), d_last_tr.map(r.topRight()));
		return !first.united(second).intersects(d_device_rect);
	}
	template<class Item>
	bool discard_impl(const Item* items, int count) const noexcept
	{
		QRectF r = itemBoundingRect(items, count);
		QRectF first(d_last_tr.map(r.topLeft()), d_last_tr.map(r.bottomRight()));
		QRectF second(d_last_tr.map(r.bottomLeft()), d_last_tr.map(r.topRight()));
		return !first.united(second).intersects(d_device_rect);
	}
	template<class Item>
	ALWAYS_INLINE bool discard(const Item& it) const noexcept
	{
		// Check if given item can be discarded based on its bounding rectangle
		// and the device rectangle
		if (std::is_same<TextItem, Item>::value || d_device_rect.isEmpty())
			return false;
		return discard_impl(it);
	}
	template<class Item>
	ALWAYS_INLINE bool discard(const Item* items, int count) const noexcept
	{
		// Check if given items can be discarded based on its bounding rectangle
		// and the device rectangle
		if (std::is_same<TextItem, Item>::value || d_device_rect.isEmpty())
			return false;
		return discard_impl(items, count);
	}
	ALWAYS_INLINE bool discard(const TextItem& it) const noexcept
	{
		// Check if given TextItem can be discarded based on its bounding rectangle
		// and the device rectangle. Only work for ascii and non rich text.
		if (d_device_rect.isEmpty())
			return false;
		if (QThread::currentThread() == qApp->thread() && isAscii(it.item)) {
			return discard_impl(estimateStringRect(it.item, it.font).translated(it.pos));
		}
		return false;
	}
	ALWAYS_INLINE bool discard(const QPaintRecord& it) const noexcept
	{
		if (d_device_rect.isEmpty())
			return false;
		QRectF r = (d_optimizations & QPaintRecord::ExtractBoundingRect) ? it.estimateBoundingRect() : it.boundingRect();
		return discard_impl(r);
	}
	ALWAYS_INLINE bool batchCommands() const noexcept { return (d_optimizations & QPaintRecord::BatchCommands); }

	virtual bool begin(QPaintDevice* pdev)
	{
		if (d_begin_end)
			d_begin_end(true, this);
		return true;
	}
	virtual bool end()
	{
		if (d_begin_end)
			d_begin_end(false, this);
		return true;
	}
	virtual void drawEllipse(const QRectF& rect)
	{
		if (d_enable && !discard(rect))
			d_device->emplaceBack(CommandBatch::DrawEllipseF, rect);
	}
	virtual void drawEllipse(const QRect& rect)
	{
		QRectF r = rect;
		if (d_enable && !discard(r))
			d_device->emplaceBack(CommandBatch::DrawEllipseF, r);
	}
	virtual void drawImage(const QRectF& rectangle, const QImage& image, const QRectF& sr, Qt::ImageConversionFlags flags = Qt::AutoColor)
	{
		if (!d_enable || discard(rectangle))
			return;
		ImageItem{ rectangle, image, sr, flags };
		d_device->emplaceBack(CommandBatch::DrawImage, ImageItem{ rectangle, image, sr, flags });
	}

	virtual void drawLines(const QLineF* lines, int lineCount)
	{
		if (!d_enable || discard(lines, lineCount))
			return;
		if (batchCommands() && d_device->count() > 0 && d_device->backType() == CommandBatch::DrawLinesF) {
			auto e = d_device->back();
			LineFVector& vec = *static_cast<LineFVector*>(e.value);
			vec.append(lines, lineCount);
		}
		d_device->emplaceBack(CommandBatch::DrawLinesF, toVector<QLineF>(lines, lines + lineCount));
	}
	virtual void drawLines(const QLine* lines, int lineCount)
	{
		if (!d_enable || discard(lines, lineCount))
			return;
		if (batchCommands() && d_device->count() > 0 && d_device->backType() == CommandBatch::DrawLinesF) {
			LineFVector& vec = *static_cast<LineFVector*>(d_device->back().value);
			vec.append(lines, lineCount);
		}
		d_device->emplaceBack(CommandBatch::DrawLinesF, toVector<QLineF>(lines, lines + lineCount));
	}
	virtual void drawPath(const QPainterPath& path)
	{
		if (d_enable && !discard(path))
			d_device->emplaceBack(CommandBatch::DrawPath, path);
	}
	virtual void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr)
	{
		if (!d_enable || discard(r))
			return;
		d_device->emplaceBack(CommandBatch::DrawPixmap, PixmapItem{ r, pm, sr });
	}
	virtual void drawPoints(const QPointF* points, int pointCount)
	{
		if (!d_enable || discard(points, pointCount))
			return;
		if (batchCommands() && d_device->count() > 0 && d_device->backType() == CommandBatch::DrawPointsF) {
			PointFVector& vec = *static_cast<PointFVector*>(d_device->back().value);
			vec.append(points, pointCount);
			return;
		}
		d_device->emplaceBack(CommandBatch::DrawPointsF, toVector<QPointF>(points, points + pointCount));
	}
	virtual void drawPoints(const QPoint* points, int pointCount)
	{
		if (!d_enable || discard(points, pointCount))
			return;
		if (batchCommands() && d_device->count() > 0 && d_device->backType() == CommandBatch::DrawPointsF) {
			PointFVector& vec = *static_cast<PointFVector*>(d_device->back().value);
			vec.append(points, pointCount);
			return;
		}
		d_device->emplaceBack(CommandBatch::DrawPointsF, toVector<QPointF>(points, points + pointCount));
	}
	virtual void drawPolygon(const QPointF* points, int pointCount, QPaintEngine::PolygonDrawMode mode)
	{
		if (!d_enable || discard(points, pointCount))
			return;
		auto pts = toVector<QPointF>(points, points + pointCount);
		switch (mode) {
			case QPaintEngine::OddEvenMode:
			case QPaintEngine::ConvexMode:
				d_device->emplaceBack(CommandBatch::DrawOddPolygonF, std::move(pts));
				break;
			case QPaintEngine::WindingMode:
				d_device->emplaceBack(CommandBatch::DrawWiddingPolygonF, std::move(pts));
				break;
			case QPaintEngine::PolylineMode:
				d_device->emplaceBack(CommandBatch::DrawPolylineF, std::move(pts));
				break;
		}
	}
	virtual void drawPolygon(const QPoint* points, int pointCount, QPaintEngine::PolygonDrawMode mode)
	{
		if (!d_enable || discard(points, pointCount))
			return;
		auto pts = toVector<QPointF>(points, points + pointCount);
		switch (mode) {
			case QPaintEngine::OddEvenMode:
			case QPaintEngine::ConvexMode:
				d_device->emplaceBack(CommandBatch::DrawOddPolygonF, std::move(pts));
				break;
			case QPaintEngine::WindingMode:
				d_device->emplaceBack(CommandBatch::DrawWiddingPolygonF, std::move(pts));
				break;
			case QPaintEngine::PolylineMode:
				d_device->emplaceBack(CommandBatch::DrawPolylineF, std::move(pts));
				break;
		}
	}
	virtual void drawRects(const QRectF* rects, int rectCount)
	{
		if (!d_enable || discard(rects, rectCount))
			return;
		if (batchCommands() && d_device->count() > 0 && d_device->backType() == CommandBatch::DrawRectsF) {
			RectFVector& vec = *static_cast<RectFVector*>(d_device->back().value);
			vec.append(rects, rectCount);
			return;
		}
		d_device->emplaceBack(CommandBatch::DrawRectsF, toVector<QRectF>(rects, rects + rectCount));
	}
	virtual void drawRects(const QRect* rects, int rectCount)
	{
		if (!d_enable || discard(rects, rectCount))
			return;
		if (batchCommands() && d_device->count() > 0 && d_device->backType() == CommandBatch::DrawRectsF) {
			RectFVector& vec = *static_cast<RectFVector*>(d_device->back().value);
			vec.append(rects, rectCount);
			return;
		}
		d_device->emplaceBack(CommandBatch::DrawRectsF, toVector<QRectF>(rects, rects + rectCount));
	}
	virtual void drawTextItem(const QPointF& p, const QTextItem& textItem)
	{
		if (!d_enable)
			return;
		TextItem item{ textItem.text(), p, textItem.font() };
		if (discard(item))
			return;
		d_device->emplaceBack(CommandBatch::DrawTextItem, std::move(item));
	}
	virtual void drawTiledPixmap(const QRectF& rect, const QPixmap& pixmap, const QPointF& p)
	{
		if (!d_enable)
			return;
		if (!discard(rect))
			d_device->emplaceBack(CommandBatch::DrawTiledPixmap, TiledPixmapItem{ rect, pixmap, p });
	}
	virtual void updateState(const QPaintEngineState& state)
	{
		if (!d_enable)
			return;
		QPaintEngine::DirtyFlags flags = state.state();
		if (flags == 0)
			return;

		if (flags & QPaintEngine::DirtyBrush)
			d_device->emplaceBack(CommandBatch::DirtyBrush, (state.brush()));
		if (flags & QPaintEngine::DirtyBrushOrigin)
			d_device->emplaceBack(CommandBatch::DirtyBrushOrigin, (state.brushOrigin()));
		if (flags & QPaintEngine::DirtyBackground)
			d_device->emplaceBack(CommandBatch::DirtyBackground, state.backgroundBrush());
		if (flags & QPaintEngine::DirtyBackgroundMode)
			d_device->emplaceBack(CommandBatch::DirtyBackgroundMode, (qint64)(state.backgroundMode()));
		if (flags & QPaintEngine::DirtyClipPath)
			d_device->emplaceBack(CommandBatch::DirtyClipPath, ClipPath{ state.clipPath(), state.clipOperation() });
		if (flags & QPaintEngine::DirtyClipRegion)
			d_device->emplaceBack(CommandBatch::DirtyClipRegion, ClipRegion{ state.clipRegion(), state.clipOperation() });
		if (flags & QPaintEngine::DirtyCompositionMode)
			d_device->emplaceBack(CommandBatch::DirtyCompositionMode, (qint64)(state.compositionMode()));
		if (flags & QPaintEngine::DirtyFont)
			d_device->emplaceBack(CommandBatch::DirtyFont, (state.font()));
		if (flags & QPaintEngine::DirtyTransform) {
			d_device->emplaceBack(CommandBatch::DirtyTransform, (state.transform()));
			d_last_tr = state.transform();
		}
		if (flags & QPaintEngine::DirtyClipEnabled)
			d_device->emplaceBack(CommandBatch::DirtyClipEnabled, (qint64)(state.isClipEnabled()));
		if (flags & QPaintEngine::DirtyPen)
			d_device->emplaceBack(CommandBatch::DirtyPen, (state.pen()));
		if (flags & QPaintEngine::DirtyHints)
			d_device->emplaceBack(CommandBatch::DirtyHints, (qint64)(state.renderHints()));
		if (flags & QPaintEngine::DirtyOpacity)
			d_device->emplaceBack(CommandBatch::DirtyOpacity, (double)(state.opacity()));
	}
	virtual QPaintEngine::Type type() const { return d_type; }
};

class QPaintRecord::PrivateData
{
public:
	static int& cc()
	{
		static int c = 0;
		return c;
	}
	PrivateData()
	  : engine(this)
	{
	}
	~PrivateData() noexcept {}
	CommandQueue commands;
	QPaintEngine::Type type{ QPaintEngine ::Windows };
	PicturePaintEngine<PrivateData> engine;

	ALWAYS_INLINE int count() noexcept { return commands.lastCount(); }
	template<class T>
	ALWAYS_INLINE void emplaceBack(CommandBatch::Type cmd_type, T&& value)
	{
		commands.emplaceBack(cmd_type, std::forward<T>(value));
	}
	ALWAYS_INLINE auto back() noexcept { return commands.back(); }
	ALWAYS_INLINE auto backType() noexcept { return commands.backType(); }
};

QPaintRecord::QPaintRecord(QPaintEngine::Type type, Optimizations opts)
  : d_ptr(new PrivateData())
{
	d_ptr->type = type;
	d_ptr->engine.d_type = type;
	d_ptr->engine.d_optimizations = opts;
}

void QPaintRecord::setOptimizations(QPaintRecord::Optimizations opt) noexcept
{
	d_ptr->engine.d_optimizations = opt;
}
void QPaintRecord::setOptimization(QPaintRecord::Optimization opt, bool enable) noexcept
{
	d_ptr->engine.d_optimizations.setFlag(opt, enable);
}
QPaintRecord::Optimizations QPaintRecord::optimizations() const noexcept
{
	return d_ptr->engine.d_optimizations;
}
bool QPaintRecord::testOptimization(Optimization opt) const noexcept
{
	return d_ptr->engine.d_optimizations.testFlag(opt);
}

bool QPaintRecord::isEmpty() const noexcept
{
	return size() == 0;
}

uint QPaintRecord::size() const noexcept
{
	return d_ptr->commands.size();
}

static int defaultDpiX()
{
	if (QCoreApplication::instance()->testAttribute(Qt::AA_Use96Dpi))
		return 96;
	if (const QScreen* screen = QGuiApplication::primaryScreen())
		return qRound(screen->logicalDotsPerInchX());
	// PI has not been initialised, or it is being initialised. Give a default dpi
	return 100;
}
static int defaultDpiY()
{
	if (QCoreApplication::instance()->testAttribute(Qt::AA_Use96Dpi))
		return 96;
	if (const QScreen* screen = QGuiApplication::primaryScreen())
		return qRound(screen->logicalDotsPerInchY());
	// PI has not been initialised, or it is being initialised. Give a default dpi
	return 100;
}

bool QPaintRecord::play(QPainter* p, Optimizations opts) const
{
	QTransform worldMatrix = p->transform();
	worldMatrix.scale(qreal(p->device()->logicalDpiX()) / qreal(defaultDpiX()), qreal(p->device()->logicalDpiY()) / qreal(defaultDpiY()));
	p->setTransform(worldMatrix);
	d_ptr->commands.apply(p, opts, worldMatrix);
	return true;
}

bool QPaintRecord::play(QPainter* p) const
{
	return play(p, d_ptr->engine.d_optimizations);
}

QRectF QPaintRecord::boundingRect() const
{
	return const_cast<CommandQueue&>(d_ptr->commands).boundingRect();
}
QRectF QPaintRecord::estimateBoundingRect() const
{
	return const_cast<CommandQueue&>(d_ptr->commands).estimateBoundingRect();
}

void QPaintRecord::clear()
{
	QSharedPointer<PrivateData> ptr(new PrivateData());
	ptr->type = d_ptr->type;
	ptr->engine.d_type = d_ptr->engine.d_type;
	ptr->engine.d_optimizations = d_ptr->engine.d_optimizations;
	d_ptr = std::move(ptr);
}

void QPaintRecord::setEnabled(bool enable) noexcept
{
	d_ptr->engine.d_enable = enable;
}
bool QPaintRecord::isEnabled() const noexcept
{
	return d_ptr->engine.d_enable;
}

QPaintEngine* QPaintRecord::paintEngine() const
{
	return &d_ptr->engine;
}

int QPaintRecord::metric(PaintDeviceMetric m) const
{
	int val;
	switch (m) {
		case PdmWidth:
			val = INT_MAX;
			break;
		case PdmHeight:
			val = INT_MAX;
			break;
		case PdmWidthMM:
			val = INT_MAX;
			break;
		case PdmHeightMM:
			val = INT_MAX;
			break;
		case PdmDpiX:
		case PdmPhysicalDpiX:
			val = defaultDpiX();
			break;
		case PdmDpiY:
		case PdmPhysicalDpiY:
			val = defaultDpiY();
			break;
		case PdmNumColors:
			val = 16777216;
			break;
		case PdmDepth:
			val = 24;
			break;
		case PdmDevicePixelRatio:
			val = 1;
			break;
		case PdmDevicePixelRatioScaled:
			val = 1 * QPaintDevice::devicePixelRatioFScale();
			break;
		default:
			val = 0;
			qWarning("QPaintRecord::metric: Invalid metric command");
	}
	return val;
}

// Internal OpenGL QWindow that renders its content in a dedicated thread
class OpenGLWindow : public QWindow
{
public:
	struct RenderingThread : QThread
	{
		QWindow* window{ nullptr };
		QThreadOpenGLWidget* parent{ nullptr };
		CommandQueue* commands{ nullptr };

		QMutex lock;
		QWaitCondition cond;
		std::atomic<bool> finished{ false };
		std::atomic<QOpenGLContext*> context{ nullptr };

		// Members used for offscreen rendering
		QOffscreenSurface surface;
		std::atomic<bool> inBuffer{ false };
		QImage bufferImage;

		void wait_for(int ms)
		{
			std::lock_guard<QMutex> ll(lock);
			cond.wait(&lock, ms);
		}
		virtual void run()
		{
			// Create the opengl context
			QOpenGLContext thread_context;
			thread_context.setFormat(window->requestedFormat());
			thread_context.create();
			context.store(&thread_context);
			bool first = true;
			QSize surface_size;

			while (QWindow* v = window) {

				// retrieve commands
				commands->popFrontUntilSeparator();
				BaseCommandBatch* cmd = commands->popCommandFront();
				if (!cmd) {
					wait_for(1);
					continue;
				}

				// Start rendering loop
				// qint64 st = QDateTime::currentMSecsSinceEpoch();

				const bool offscreenRendering = inBuffer.load();
				std::unique_ptr<QOpenGLFramebufferObject> frame;
				if (offscreenRendering) {
					// Offscreen drawinf
					thread_context.makeCurrent(&surface);
					QOpenGLFramebufferObjectFormat format;
					format.setSamples(surface.format().samples());
					format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
					frame.reset(new QOpenGLFramebufferObject(v->size(), format));
					frame->setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
					frame->bind();
					inBuffer.store(false);
				}
				else
					thread_context.makeCurrent(v);

				if (first) {
					QOpenGLFunctions* glFuncs = QOpenGLContext::currentContext()->functions();

					// First rendering: draw background
					QColor back = parent->palette().color(QPalette::Window);
					glFuncs->glClearColor(back.red() * 0.00390625, back.green() * 0.00390625, back.blue() * 0.00390625, back.alpha() * 0.00390625);
					glFuncs->glClear(GL_COLOR_BUFFER_BIT);

					// Call initializeGL()
					parent->initializeGL();
					first = false;
				}

				QSize actual_size = v->size();
				if (surface_size != actual_size) {
					// Call resizeGL() in case of window size change
					surface_size = actual_size;
					parent->resizeGL(actual_size.width(), actual_size.height());
				}

				// Initialize the painter
				QOpenGLPaintDevice device(actual_size);
				QPainter p;
				p.begin(&device);

				// Apply the first command which is a background drawing,
				// then call paintGL()
				if (cmd) {
					cmd->command()->apply(&p, parent->optimizations(), QTransform(), parent, !offscreenRendering);
					destroyCommand(cmd);
				}
				cmd = commands->popCommandFront();
				if (!cmd || !cmd->isSeparator()) {
					// Apply remaining commands
					do {
						if (cmd) {
							cmd->command()->apply(&p, parent->optimizations());
							destroyCommand(cmd);
						}
						cmd = commands->popCommandFront();
						if (!cmd)
							wait_for(1);
					} while ((!cmd || !cmd->isSeparator()) && window);
					if (cmd)
						destroyCommand(cmd);
				}
				p.end();

				// Swap buffers
				if (offscreenRendering) {
					frame->release();
					thread_context.swapBuffers(&surface);
					bufferImage = frame->toImage();
					frame.reset();
				}
				else
					thread_context.swapBuffers(v);

				thread_context.doneCurrent();

				// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
				// printf("ogl: %i ms\n", (int)el);

				finished.store(true);
				cond.notify_one();
			}
		}
	};

	RenderingThread thread;
	PicturePaintEngine<OpenGLWindow> trueEngine;
	CommandQueue commands;
	ClipRect clipRect;
	QTransform tr;
	QPointF center;
	QThreadOpenGLWidget* widget{ nullptr };
	int inRendering{ 0 };

	qint64 lastRequestActive{ 0 };
	qint64 startPainting{ 0 };
	uint maximumPaintTime{ 0 };

public:
	// members for PicturePaintEngine
	ALWAYS_INLINE int count() noexcept { return commands.lastCount(); }
	template<class T>
	void emplaceBack(CommandBatch::Type cmd_type, T&& value)
	{
		if (commands.emplaceBack(cmd_type, std::forward<T>(value)))
			thread.cond.notify_one();
	}
	ALWAYS_INLINE auto back() noexcept { return commands.back(); }
	ALWAYS_INLINE auto backType() noexcept { return commands.backType(); }

	OpenGLWindow(QThreadOpenGLWidget* top, const QSurfaceFormat& format)
	  : QWindow()
	  , trueEngine(this, QPaintEngine::OpenGL2)
	  , widget(top)
	{
		setSurfaceType(QSurface::OpenGLSurface);
		setFormat(format);
		trueEngine.d_begin_end = [this](bool begin, QPaintEngine*) {
			if (begin)
				startRendering();
			else
				stopRendering();
		};
		thread.window = this;
		thread.parent = top;
		thread.commands = &commands;
		thread.surface.setFormat(this->requestedFormat());
		thread.surface.create();
		thread.start();
	}
	~OpenGLWindow()
	{
		thread.window = nullptr;
		thread.wait();
	}

	void startRendering()
	{
		// Start rendering from the GUI thread.
		// Automatically called by QPainter::begin()
		// or before paintEvent().

		// Check for multiple calls to startRendering()
		if (inRendering++ > 0)
			return;

		// Check again for QGraphicsView parent in case
		// the QGraphicsScene was changed using QGraphicsView::setScene()
		widget->updateParent();

		// Store the start time
		startPainting = QDateTime::currentMSecsSinceEpoch();

		// Init
		thread.finished = false;
		trueEngine.d_device_rect = QRectF();
		bool hasClipRect = false;

		// Always start by sending to the rendering thread a ClipRect object

		if (!clipRect.rect.isEmpty()) {
			QGraphicsView* view = qobject_cast<QGraphicsView*>(widget->parentWidget());
			QPointF c = view->mapToScene(view->rect()).boundingRect().center();
			if (view->transform() == tr && c == center) {
				// We have a valid clipping rectangle in case of partial update
				trueEngine.d_device_rect = QRectF(view->mapFromScene(clipRect.rect).boundingRect()).adjusted(-10, -10, 10, 10);
				emplaceBack(CommandBatch::DrawClipRect, ClipRect{ QRectF(view->mapFromScene(clipRect.rect).boundingRect()), clipRect.brush });
				hasClipRect = true;
			}
			else {
				// Something changed after setting the clipping rect: full update
				tr = view->transform();
				center = c;
			}
			// Reset clipping rect
			clipRect.rect = QRectF();
		}
		if (!hasClipRect) {
			// We still need to draw the widget background.
			// By default, we use the widget palette to find
			// the background brush. If the widget is a QGraphicsView
			// viewport, try to use the QGraphicsView background brush
			// instead.

			// Reset device rectangle to ensure that all items are drawn
			trueEngine.d_device_rect = QRectF();

			// Find background brush
			QBrush background = widget->palette().brush(QPalette::Window);
			if (QGraphicsView* view = qobject_cast<QGraphicsView*>(widget->parentWidget())) {
				QBrush br = view->backgroundBrush();
				if (br.style() != Qt::NoBrush && br.color().alpha() == 255)
					background = view->backgroundBrush();
			}

			// Send the draw background command
			emplaceBack(CommandBatch::DrawViewBackground, ClipRect{ QRectF(widget->rect()), background });
		}
	}
	void stopRendering()
	{
		// Ensure this is the last stopRendering() call
		if (--inRendering > 0)
			return;

		Q_ASSERT(inRendering == 0);

		// Send a separator command to notify end of drawing
		commands.addSeparator();

		// Compute time spent in drawing functions
		auto el = QDateTime::currentMSecsSinceEpoch() - startPainting;

		// If necessary, wait for the opengl rendering
		while (!thread.finished.load()) {
			el = QDateTime::currentMSecsSinceEpoch() - startPainting;
			if (el > maximumPaintTime)
				return;
			thread.wait_for(1);
		}
	}

	QPaintEngine* paintEngine() const { return const_cast<PicturePaintEngine<OpenGLWindow>*>(&trueEngine); }
};

class QThreadOpenGLWidget::PrivateData
{
public:
	PrivateData(QThreadOpenGLWidget* w)
	  : window(nullptr)
	{
	}
	~PrivateData() {}

	QPointer<OpenGLWindow> window;
	QPointer<QWidget> widget;

	QSurfaceFormat surfaceFormat{ QSurfaceFormat::defaultFormat() };
	QPaintRecord::Optimizations optimizations{ QPaintRecord::BatchCommands };
	uint maximumPaintTime{ 0 };

	// For QGraphicsScene only
	QPointer<QGraphicsView> view;
	QPointer<QGraphicsScene> scene;
	QTransform tr;
	QPointF center;
	QRectF prev;
	bool ignoreNext{ false };
	bool hadRubberBand{ false };
	QRectF rubberBand;
};

void QThreadOpenGLWidget::init(bool show_widget)
{
	// Initialize the QThreadOpenGLWidget

	if (d_data->window)
		return;

	d_data->window = new OpenGLWindow(this, d_data->surfaceFormat);
	d_data->widget = QWidget::createWindowContainer(d_data->window, this);

	while (!d_data->window->thread.context.load())
		std::this_thread::yield();

	d_data->widget->move(0, 0);
	d_data->widget->resize(this->size());
	d_data->widget->installEventFilter(this);
	d_data->window->installEventFilter(this);

	d_data->widget->setAcceptDrops(true);
	setAcceptDrops(true);

	// This is mandatory for non flickering (and actually visible) drawing
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_OpaquePaintEvent);

	d_data->widget->setMouseTracking(true);
	setMouseTracking(true);

	// Remove background from widget or we will
	// have issues with QWidget::render().
	d_data->widget->setAttribute(Qt::WA_NoSystemBackground);
	d_data->widget->setAttribute(Qt::WA_TranslucentBackground);
	d_data->widget->setAttribute(Qt::WA_PaintOnScreen);
	d_data->widget->setAttribute(Qt::WA_TransparentForMouseEvents);

	d_data->window->maximumPaintTime = d_data->maximumPaintTime;
	d_data->window->trueEngine.d_optimizations = d_data->optimizations;

	if (parent() && show_widget)
		show();

	if (QGraphicsView* view = qobject_cast<QGraphicsView*>(parent())) {
		if (view->scene())
			view->scene()->update();
	}
	this->update();
}

QThreadOpenGLWidget::QThreadOpenGLWidget(const QSurfaceFormat& format, QPaintRecord::Optimizations opts, QWidget* parent)
  : QWidget(parent)
{
	d_data = new PrivateData(this);
	d_data->surfaceFormat = format;
	d_data->optimizations = opts;
	init();
}

QThreadOpenGLWidget::QThreadOpenGLWidget(QWidget* parent)
  : QWidget(parent)
{
	d_data = new PrivateData(this);
	init();
}

QThreadOpenGLWidget::~QThreadOpenGLWidget()
{
	delete d_data;
}

QPaintEngine* QThreadOpenGLWidget::paintEngine() const
{
	const_cast<QThreadOpenGLWidget*>(this)->init();
	return d_data->window->paintEngine();
}

void QThreadOpenGLWidget::setOptimizations(QPaintRecord::Optimizations opt) noexcept
{
	d_data->optimizations = opt;
	if (d_data->window)
		d_data->window->trueEngine.d_optimizations = opt;
}
void QThreadOpenGLWidget::setOptimization(QPaintRecord::Optimization opt, bool enable) noexcept
{
	d_data->optimizations.setFlag(opt, enable);
	if (d_data->window)
		d_data->window->trueEngine.d_optimizations = opt;
}
QPaintRecord::Optimizations QThreadOpenGLWidget::optimizations() const noexcept
{
	return d_data->optimizations;
}
bool QThreadOpenGLWidget::testOptimization(QPaintRecord::Optimization opt) const noexcept
{
	return d_data->optimizations.testFlag(opt);
}

void QThreadOpenGLWidget::setMaximumPaintTime(uint msecs) noexcept
{
	d_data->maximumPaintTime = msecs;
	if (d_data->window)
		d_data->window->maximumPaintTime = msecs;
}
uint QThreadOpenGLWidget::maximumPaintTime() const noexcept
{
	return d_data->maximumPaintTime;
}

void QThreadOpenGLWidget::startRendering()
{
	init();
	d_data->window->startRendering();
}
void QThreadOpenGLWidget::stopRendering()
{
	init();
	d_data->window->stopRendering();
}

void QThreadOpenGLWidget::drawFunction(const std::function<void(QPainter*)>& fun)
{
	init();
	d_data->window->emplaceBack(CommandBatch::DrawFunction, fun);
}

QSurfaceFormat QThreadOpenGLWidget::format() const
{
	return d_data->surfaceFormat;
}

QOpenGLContext* QThreadOpenGLWidget::context() const
{
	return d_data->window->thread.context.load(std::memory_order_relaxed);
}

QWindow* QThreadOpenGLWidget::openGLWindow() const
{
	return d_data->window;
}

QImage QThreadOpenGLWidget::toImage(bool draw_background) const
{
	QImage img(this->size(), QImage::Format_ARGB32);
	img.fill(Qt::transparent);
	{
		QPainter p(&img);
		if (draw_background)
			p.fillRect(QRect(0, 0, width(), height()), this->palette().brush(QPalette::Window));

		const_cast<QThreadOpenGLWidget*>(this)->render(&p, QPoint(), QRegion(), QWidget::RenderFlags{});
	}
	return img;
}

void QThreadOpenGLWidget::updateParent()
{
	// Check for QGraphicsView parent

	auto* view = qobject_cast<QGraphicsView*>(parentWidget());
	if (view == d_data->view) {
		if (!view || view->scene() == d_data->scene)
			// nothing to do
			return;
	}

	if (d_data->scene) {
		disconnect(d_data->scene, SIGNAL(changed(const QList<QRectF>&)), this, SLOT(changed(const QList<QRectF>&)));
		disconnect(d_data->scene, SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(changed()));
	}
	if (d_data->view) {
		disconnect(d_data->view, SIGNAL(rubberBandChanged(QRect, QPointF, QPointF)), this, SLOT(rubberBandChanged(QRect, QPointF, QPointF)));
	}
	d_data->scene = nullptr;
	d_data->view = view;
	if (d_data->view) {
		d_data->scene = d_data->view->scene();
		if (d_data->scene) {
			connect(d_data->scene, SIGNAL(changed(const QList<QRectF>&)), this, SLOT(changed(const QList<QRectF>&)));
			connect(d_data->scene, SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(changed()));
		}
		connect(d_data->view, SIGNAL(rubberBandChanged(QRect, QPointF, QPointF)), this, SLOT(rubberBandChanged(QRect, QPointF, QPointF)));
	}
}

void QThreadOpenGLWidget::rubberBandChanged(QRect rubberBandRect, QPointF fromScenePoint, QPointF toScenePoint)
{
	if (fromScenePoint == QPointF() && toScenePoint == QPointF()) {
		d_data->rubberBand = QRectF();
		d_data->hadRubberBand = true;
	}
	else {
		d_data->rubberBand = (QRectF(fromScenePoint, toScenePoint));
	}
}

void QThreadOpenGLWidget::changed(const QList<QRectF>& rects)
{
	// Alot of complicated stuff to make partial update from QGraphicsView
	// working. And it's still not perfect...

	if (d_data->view->viewportUpdateMode() == QGraphicsView::FullViewportUpdate)
		return;
	if (!d_data->window)
		return;

	d_data->window->clipRect.brush = this->palette().brush(QPalette::Window);

	if (!d_data->rubberBand.isEmpty()) {
		// During rubber band, update the full viewport
		QRectF r;
		for (int i = 0; i < rects.size(); ++i)
			r = r.united(rects[i]);
		QRectF tmp = r;
		r = r.united(d_data->prev);
		d_data->prev = tmp;
		r = r.united(d_data->rubberBand);
		d_data->window->clipRect.rect = r;
		return;
	}

	if (d_data->hadRubberBand) {
		d_data->hadRubberBand = false;
		d_data->window->clipRect.rect = QRectF();
		return;
	}

	if (d_data->ignoreNext) {
		// After a non null rects, there is always an empty one
		// that must be ignored.
		d_data->ignoreNext = false;
		return;
	}

	QPointF center = d_data->view->mapToScene(d_data->view->rect()).boundingRect().center();
	QRectF r;
	if (d_data->tr == d_data->view->transform() && d_data->center == center) {
		for (int i = 0; i < rects.size(); ++i)
			r = r.united(rects[i]);
		if (!r.isEmpty())
			d_data->ignoreNext = true;
		QRectF tmp = r;
		r = r.united(d_data->prev);
		d_data->prev = tmp;
	}
	else {
		d_data->tr = d_data->view->transform();
		d_data->center = center;
	}

	d_data->window->clipRect.rect = r;
}

bool QThreadOpenGLWidget::eventFilter(QObject* watched, QEvent* event)
{
	switch (event->type()) {
		// Do NOT filter drag events, they are already managed by QWindowContainer
		// case QEvent::DragEnter:
		// case QEvent::DragLeave:
		// case QEvent::DragMove:
		case QEvent::Drop:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseMove:
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		case QEvent::TouchBegin:
		case QEvent::TouchEnd:
		case QEvent::TouchCancel:
		case QEvent::TouchUpdate:
		case QEvent::Wheel:
		case QEvent::HoverEnter:
		case QEvent::HoverLeave:
		case QEvent::HoverMove:
		case QEvent::Enter:
		case QEvent::Leave:
			if (watched == d_data->window)
				// Forward event to the QThreadOpenGLWidget
				qApp->sendEvent(this, event);
			break;
		case QEvent::Move:
			// Scroll events will move the position of the widget container,
			// but we want it to be fixed at (0,0) !
			if (watched == d_data->widget) {
				QPoint pt = d_data->widget->pos();
				if (pt != QPoint(0, 0))
					d_data->widget->move(0, 0);
				return true;
			}
			break;

		case QEvent::Expose: {
			if (watched == d_data->window && !d_data->window->isExposed() && d_data->widget->isHidden()) {
				// The widget container should never be hidden.
				// This might happen when  creating a QOpenGLWidget
				// which has a side effect of destroying other QWindow
				// instance (?). In this case, no choice but to recreate
				// the window.
				d_data->widget->removeEventFilter(this);
				d_data->window->removeEventFilter(this);
				d_data->widget->deleteLater();
				QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection, Q_ARG(bool, true));
			}
			break;
		}
		default:
			break;
	}
	return false;
}

bool QThreadOpenGLWidget::event(QEvent* e)
{
	QEvent::Type type = e->type();
	switch (type) {
		case QEvent::ParentChange:
			// Do special stuff when the parent is a QGraphicsView
			updateParent();
			break;
		case QEvent::Show: {
			// Initialize the widget on first show
			// in case of destruction during QOpenGLWidget creation (????)
			init();
			// For unkown reason, setting the layout in init(),
			// called in the constructor, prevent the widget from
			// being displayed properly. Setting the layout at first
			// show works...
			QLayout* lay = layout();
			if (!lay) {
				lay = new QHBoxLayout();
				lay->addWidget(d_data->widget);
				lay->setContentsMargins(0, 0, 0, 0);
				setLayout(lay);
			}
		} break;
		case QEvent::Paint: {
			// Paint event (when the parent is not a QGraphicsView):
			// ensure that startRendering() and stopRendering() are
			// properly called.
			this->startRendering();
			uint max_time = this->maximumPaintTime();
			if (sharedPainter()) {
				// Shared painter in case of QWidget::render()
				d_data->window->thread.inBuffer.store(true);
				this->setMaximumPaintTime(UINT_MAX);
			}
			this->paintEvent(static_cast<QPaintEvent*>(e));
			this->stopRendering();
			if (sharedPainter()) {
				// Shared painter: draw the FBO content
				this->setMaximumPaintTime(max_time);
				QPainter p(this);
				p.drawImage(QPoint(0, 0), d_data->window->thread.bufferImage);
			}
			return true;
		}
		case QEvent::Resize:
			d_data->widget->move(0, 0);
			d_data->widget->resize(this->size());
			break;
		default:
			break;
	}
	return QWidget::event(e);
}

class QOpenGLItem::PrivateData
{

public:
	struct PainterState
	{
		QPainter::RenderHints hints{};
		QPainter::CompositionMode compositionMode{};
		QStyle::State state{};
		PainterState() {}
		DEFAULT_MOVE(PainterState);
		bool operator==(const PainterState& o) const noexcept { return hints == o.hints && compositionMode == o.compositionMode && state == o.state; }
		bool operator!=(const PainterState& o) const noexcept { return !(*this == o); }
		PainterState(const QPainter* p, const QStyleOptionGraphicsItem* options)
		  : hints(p->renderHints())
		  , compositionMode(p->compositionMode())
		  , state(options->state)
		{
		}
		void toPainter(QPainter* p) const
		{
			p->setRenderHints(hints);
			p->setCompositionMode(compositionMode);
		}
	};

	QGraphicsItem* item{ nullptr };
	std::atomic<bool> dirty{ true };
	bool inDrawThroughCache{ false };
	PainterState state;
	QPaintRecord picture;

	PrivateData()
	  : picture(QPaintEngine::OpenGL2)
	{
	}
};

QOpenGLItem::QOpenGLItem(QGraphicsItem* this_item)
{
	d_data = new PrivateData();
	d_data->item = this_item;
}
QOpenGLItem::~QOpenGLItem()
{
	delete d_data;
}

void QOpenGLItem::markItemDirty() noexcept
{
	d_data->dirty.store(true);
}

struct BoolLocker
{
	bool& _val;
	BoolLocker(bool& val)
	  : _val(val)
	{
		val = true;
	}
	~BoolLocker() { _val = false; }
};

bool QOpenGLItem::drawThroughCache(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) const
{
	// Avoid recursive calls
	if (d_data->inDrawThroughCache)
		return false;

	if (painter->device() != painter->paintEngine()->paintDevice()) {
		// We are drawing using a shared painter, don't go through the rendering thread
		return false;
	}

	// Get the QThreadOpenGLWidget widget
	QThreadOpenGLWidget* ogl = qobject_cast<QThreadOpenGLWidget*>(widget);
	if (!ogl)
		// Disable caching for non QThreadOpenGLWidget viewport.
		// This could still work, but won't provide any benefits.
		return false;

	ogl->init();

	// We need to save the painter state.
	// However, the QGraphicsView might do it for us,
	// so check it first.

	bool save_painter_state = true;
	if (QGraphicsView* view = qobject_cast<QGraphicsView*>(ogl->parent()))
		if (!(view->optimizationFlags() & QGraphicsView::DontSavePainterState))
			save_painter_state = false;

	if (save_painter_state)
		painter->save();

	// Tells that we are inside drawThroughCache()
	BoolLocker lock(d_data->inDrawThroughCache);

	// Retrieve painter state
	PrivateData::PainterState state(painter, option);
	const bool same_state = state == d_data->state;
	if (!same_state || d_data->dirty.load(std::memory_order_relaxed)) {
		// Regenerate picture if dirty flag is set
		// or if we changed the painter state (selection, hovering...)
		d_data->picture.clear();
		d_data->picture.setOptimizations(ogl->optimizations());
		{
			QPainter p(&d_data->picture);
			state.toPainter(&p);

			// Set (almost) impossible values to make sure that
			// state changes (like setting the pen) are always
			// recorded.
			d_data->picture.setEnabled(false);
			const QColor no_color = QColor(121, 155, 7, 1);
			p.setPen(QPen(no_color));
			p.setBrush(QBrush(no_color));
			d_data->picture.setEnabled(true);

			d_data->item->paint(&p, option, widget);
		}
		d_data->dirty.store(false);
		d_data->state = state;
	}

	// Send the picture to the rendering pipeline
	if (!ogl->d_data->window->trueEngine.discard(d_data->picture))
		ogl->d_data->window->emplaceBack(CommandBatch::DrawRecord, d_data->picture);

	// Restore painter state
	if (save_painter_state)
		painter->restore();

	return true;
}
