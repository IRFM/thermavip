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

#include "VipPicture.h"
#include "VipLock.h"
#include "VipSleep.h"

#include <condition_variable>
#include <vector>

#include <qapplication.h>
#include <qboxlayout.h>
#include <qcolor.h>
#include <qdatetime.h>
#include <qevent.h>
#include <qgraphicsview.h>
#include <qguiapplication.h>
#include <qopenglcontext.h>
#include <qopenglpaintdevice.h>
#include <qopenglwidget.h>
#include <qpaintengine.h>
#include <qpainter.h>
#include <qpainterpath.h>
#include <qpointer.h>
#include <qscreen.h>
#include <qscrollarea.h>
#include <qthread.h>
#include <qwindow.h>

namespace detail
{

	class VipWindowContainer::PrivateData
	{
	public:
		PrivateData(QWidget* w)
		  : window(nullptr)
		  , oldFocusWindow(nullptr)
		  , q(w)
		  , usesNativeWidgets(false)
		{
		}
		~PrivateData() {}

		void updateGeometry()
		{
			if (!q->isWindow() && (q->geometry().bottom() <= 0 || q->geometry().right() <= 0))
				/* Qt (e.g. QSplitter) sometimes prefer to hide a widget by *not* calling
				   setVisible(false). This is often done by setting its coordinates to a sufficiently
				   negative value so that its clipped outside the parent. Since a QWindow is not clipped
				   to widgets in general, it needs to be dealt with as a special case.
				*/
				window->setGeometry(q->geometry());
			else if (usesNativeWidgets)
				window->setGeometry(q->rect());
			else
				window->setGeometry(QRect(q->mapTo(q->window(), QPoint()), q->size()));
		}
		void markParentChain()
		{
			/* QWidget* p = q;
			while (p) {
				QWidgetPrivate* d = static_cast<QWidgetPrivate*>(QWidgetPrivate::get(p));
				d->createExtra();
				d->extra->hasWindowContainer = true;
				p = p->parentWidget();
			}*/
		}
		void updateUsesNativeWidgets()
		{
			if (window->parent() == nullptr)
				return;
			if (q->internalWinId()) {
				// Allow use native widgets if the window container is already a native widget
				usesNativeWidgets = true;
				return;
			}
			bool nativeWidgetSet = false;
			QWidget* p = q->parentWidget();
			while (p) {
				if (qobject_cast<QAbstractScrollArea*>(p) != 0) {
					q->winId();
					nativeWidgetSet = true;
					break;
				}
				p = p->parentWidget();
			}
			usesNativeWidgets = nativeWidgetSet;
		}

		bool isStillAnOrphan() const { return window->parent() == &fakeParent; }
		QPointer<QWindow> window;
		QWindow* oldFocusWindow;
		QWindow fakeParent;
		QWidget* q;
		uint usesNativeWidgets : 1;
	};

	VipWindowContainer::VipWindowContainer(QWindow* embeddedWindow, QWidget* parent, Qt::WindowFlags flags)
	  : QWidget(parent, flags)
	{
		VIP_CREATE_PRIVATE_DATA(d_data,this);
		if (Q_UNLIKELY(!embeddedWindow)) {
			qWarning("VipWindowContainer: embedded window cannot be null");
			return;
		}
		// The embedded QWindow must use the same logic as QWidget when it comes to the surface type.
		// Otherwise we may end up with BadMatch failures on X11.
		d_data->window = embeddedWindow;
		QString windowName = d_data->window->objectName();
		if (windowName.isEmpty())
			windowName = QString::fromUtf8(d_data->window->metaObject()->className());
		d_data->fakeParent.setObjectName(windowName + QLatin1String("ContainerFakeParent"));
		d_data->window->setParent(&d_data->fakeParent);
		setAcceptDrops(true);
		connect(QGuiApplication::instance(), SIGNAL(focusWindowChanged(QWindow*)), this, SLOT(focusWindowChanged(QWindow*)));
	}
	QWindow* VipWindowContainer::containedWindow() const
	{
		return d_data->window;
	}
	/*!
	    \internal
	 */
	VipWindowContainer::~VipWindowContainer()
	{
		// Call destroy() explicitly first. The dtor would do this too, but
		// QEvent::PlatformSurface delivery relies on virtuals. Getting
		// SurfaceAboutToBeDestroyed can be essential for OpenGL, Vulkan, etc.
		// QWindow subclasses in particular. Keep these working.
		if (d_data->window)
			d_data->window->destroy();
		delete d_data->window;
	}
	/*!
	    \internal
	 */
	void VipWindowContainer::focusWindowChanged(QWindow* focusWindow)
	{
		d_data->oldFocusWindow = focusWindow;
		if (focusWindow == d_data->window) {
			QWidget* widget = QApplication::focusWidget();
			if (widget)
				widget->clearFocus();
		}
	}
	/*!
	    \internal
	 */
	bool VipWindowContainer::event(QEvent* e)
	{
		if (!d_data->window)
			return QWidget::event(e);
		QEvent::Type type = e->type();
		switch (type) {
			case QEvent::ChildRemoved: {
				QChildEvent* ce = static_cast<QChildEvent*>(e);
				if (ce->child() == d_data->window)
					d_data->window = nullptr;
				break;
			}
			// The only thing we are interested in is making sure our sizes stay
			// in sync, so do a catch-all case.
			case QEvent::Resize:
				d_data->updateGeometry();
				break;
			case QEvent::Move:
				d_data->updateGeometry();
				break;
			case QEvent::PolishRequest:
				d_data->updateGeometry();
				break;
			case QEvent::Show:
				d_data->updateUsesNativeWidgets();
				if (d_data->isStillAnOrphan()) {
					d_data->window->setParent(d_data->usesNativeWidgets ? windowHandle() : window()->windowHandle());
					d_data->fakeParent.destroy();
				}
				if (d_data->window->parent()) {
					d_data->markParentChain();
					d_data->window->show();
				}
				break;
			case QEvent::Hide:
				if (d_data->window->parent())
					d_data->window->hide();
				break;
			case QEvent::FocusIn:
				if (d_data->window->parent()) {
					if (d_data->oldFocusWindow != d_data->window) {
						d_data->window->requestActivate();
					}
					else {
						QWidget* next = nextInFocusChain();
						next->setFocus();
					}
				}
				break;
			case QEvent::Drop:
			case QEvent::DragMove:
			case QEvent::DragLeave:
				QCoreApplication::sendEvent(d_data->window, e);
				return e->isAccepted();
			case QEvent::DragEnter:
				// Don't reject drag events for the entire widget when one
				// item rejects the drag enter
				QCoreApplication::sendEvent(d_data->window, e);
				e->accept();
				return true;

			default:
				break;
		}
		return QWidget::event(e);
	}

}

#ifdef DrawText
#undef DrawText
#endif

struct TiledPixmapItem
{
	const QRectF rect;
	const QPixmap pixmap;
	const QPointF pos;
};

struct PixmapItem
{
	const QRectF rect;
	const QPixmap pixmap;
	const QRectF src;
	const QTransform tr;
	const bool has_tr{ false };
};

struct ImageItem
{
	const QRectF rect;
	const QImage image;
	const QRectF src;
	const Qt::ImageConversionFlags flags;
	const QTransform tr;
	const bool has_tr{ false };
};

struct TextItem
{
	const QString item;
	const QPointF pos;
	const QFont font;
	const QTransform tr;
	const bool has_tr{ false };
};

using PixmapVector = QVector<PixmapItem>;
using ImageVector = QVector<ImageItem>;
using TextVector = QVector<TextItem>;
using LineVector = QVector<QLine>;
using LineFVector = QVector<QLineF>;
using PointVector = QVector<QPoint>;
using PointFVector = QVector<QPointF>;
using RectVector = QVector<QRect>;
using RectFVector = QVector<QRectF>;

struct PaintEngineState
{
	QPointF brushOrigin;
	QFont font;
	QPen pen;
	QBrush brush;
	QBrush bgBrush; // background brush
	QRegion clipRegion;
	QPainterPath clipPath;
	Qt::ClipOperation clipOperation{ Qt::NoClip };
	QPainter::RenderHints renderHints;
	QTransform worldMatrix; // World transformation matrix, not window and viewport
	qreal opacity{ 1. };
	uint clipEnabled{ 0 };
	QPainter::CompositionMode composition_mode{ QPainter::CompositionMode_SourceOver };
	Qt::BGMode backgroundMode{ Qt::TransparentMode };
	QPaintEngine::DirtyFlags dirty;

public:
	PaintEngineState() {}
	PaintEngineState(const PaintEngineState& state)
	  : brushOrigin(state.brushOrigin)
	  , font(state.font)
	  , pen(state.pen)
	  , brush(state.brush)
	  , bgBrush(state.bgBrush)
	  , clipRegion(state.clipRegion)
	  , clipPath(state.clipPath)
	  , clipOperation(state.clipOperation)
	  , renderHints(state.renderHints)
	  , opacity(state.opacity)
	  , clipEnabled(state.clipEnabled)
	  , composition_mode(state.composition_mode)
	  , backgroundMode(state.backgroundMode)
	  , dirty(state.dirty)
	{
		if (dirty & QPaintEngine::DirtyTransform)
			worldMatrix = state.worldMatrix;
	}

	PaintEngineState(const QPaintEngineState& state)
	  : brushOrigin(state.brushOrigin())
	  , font(state.font())
	  , pen(state.pen())
	  , brush(state.brush())
	  , bgBrush(state.backgroundBrush())
	  , clipRegion(state.clipRegion())
	  , clipPath(state.clipPath())
	  , clipOperation(state.clipOperation())
	  , renderHints(state.renderHints())
	  , opacity(state.opacity())
	  , clipEnabled(state.isClipEnabled())
	  , composition_mode(state.compositionMode())
	  , backgroundMode(state.backgroundMode())
	  , dirty(state.state())
	{
		if (dirty & QPaintEngine::DirtyTransform)
			worldMatrix = state.transform();
	}
};

template<class T>
QVector<T> toVector(const T* begin, const T* end)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	QVector<T> res(static_cast<int>(end - begin));
	std::copy(begin, end, res.begin());
	return res;
#else
	return QVector<T>(begin, end);
#endif
}

struct BaseHolder
{
	virtual ~BaseHolder() {}
};
template<class T>
struct Holder : BaseHolder
{
	T value;
	Holder(const T& val)
	  : value(val)
	{
	}
	template<class U>
	Holder(U&& val)
	  : value(std::forward<U>(val))
	{
	}
};

class CommandHolder
{
	struct alignas(Holder<std::uintptr_t>) Storage
	{
		char data[sizeof(Holder<std::uintptr_t>)];
	};
	Storage storage;
	std::int16_t flag; // 1: inline, 0: heap, -1: nothing
	std::int16_t type; // command type
	VIP_ALWAYS_INLINE void* as_storage() noexcept { return (void*)storage.data; }
	VIP_ALWAYS_INLINE std::uintptr_t* as_address() noexcept { return reinterpret_cast<std::uintptr_t*>(storage.data); }
	VIP_ALWAYS_INLINE const std::uintptr_t* as_address() const noexcept { return reinterpret_cast<const std::uintptr_t*>(storage.data); }

public:
	int count{ 0 };
	CommandHolder* next{ nullptr };
	CommandHolder* prev{ nullptr };

	VIP_ALWAYS_INLINE CommandHolder()
	  : flag(-1)
	  , type(0)
	{
	}
	template<class T>
	VIP_ALWAYS_INLINE CommandHolder(int command_type, const T& value)
	  : flag(sizeof(Holder<T>) <= sizeof(storage))
	  , type(static_cast<std::int16_t>(command_type))
	{
		if (sizeof(Holder<T>) <= sizeof(storage))
			new (as_storage()) Holder<T>(value);
		else
			as_address()[0] = reinterpret_cast<std::uintptr_t>(new Holder<T>(value));
	}
	VIP_ALWAYS_INLINE CommandHolder(int command_type, const QPaintEngineState& value)
	  : flag(sizeof(Holder<PaintEngineState>) <= sizeof(storage))
	  , type(static_cast<std::int16_t>(command_type))
	{
		if (sizeof(Holder<PaintEngineState>) <= sizeof(storage))
			new (as_storage()) Holder<PaintEngineState>(value);
		else
			as_address()[0] = reinterpret_cast<std::uintptr_t>(new Holder<PaintEngineState>(value));
	}
	/* VIP_ALWAYS_INLINE CommandHolder(CommandHolder&& other) noexcept
	  : flag(other.flag)
	  , type(other.type)
	{
		memcpy(storage.data, other.storage.data, sizeof(storage));
		other.flag = -1;
		other.type = 0;
	}*/
	VIP_ALWAYS_INLINE virtual ~CommandHolder()
	{
		if (flag == 0) {
			BaseHolder* h = reinterpret_cast<Holder<std::uintptr_t>*>(as_address()[0]);
			delete h;
		}
		else if (flag == 1) {
			BaseHolder* h = reinterpret_cast<Holder<std::uintptr_t>*>(storage.data);
			h->~BaseHolder();
		}
	}

	/* VIP_ALWAYS_INLINE CommandHolder& operator=(CommandHolder&& other) noexcept
	{
		memcpy(storage.data, other.storage.data, sizeof(storage));
		flag = (other.flag);
		type = (other.type);
		other.type = 0;
		other.flag = -1;
		return *this;
	}*/

	template<class T>
	VIP_ALWAYS_INLINE const T* value() const noexcept
	{
		switch (flag) {
			case 0:
				return &(reinterpret_cast<const Holder<T>*>(as_address()[0])->value);
			case 1:
				return &(reinterpret_cast<const Holder<T>*>(storage.data)->value);
			default:
				return nullptr;
		}
		VIP_UNREACHABLE();
	}
	template<class T>
	VIP_ALWAYS_INLINE T* value() noexcept
	{
		switch (flag) {
			case 0:
				return &(reinterpret_cast<Holder<T>*>(as_address()[0])->value);
			case 1:
				return &(reinterpret_cast<Holder<T>*>(storage.data)->value);
			default:
				return nullptr;
		}
		VIP_UNREACHABLE();
	}

	VIP_ALWAYS_INLINE int commandType() const noexcept { return type; }
};

struct Command : CommandHolder
{

	enum Type
	{
		DrawNone,
		DrawPixmap,
		DrawTiledPixmap,
		DrawImage,
		DrawLines,
		DrawLinesF,
		DrawPoints,
		DrawPointsF,
		DrawRects,
		DrawRectsF,
		DrawEllipse,
		DrawEllipseF,
		DrawPath,
		DrawOddPolygon,
		DrawOddPolygonF,
		DrawWiddingPolygon,
		DrawWiddingPolygonF,
		DrawPolyline,
		DrawPolylineF,
		DrawText,
		ChangeState
	};

	VIP_ALWAYS_INLINE Command()
	  : CommandHolder()
	{
	}
	VIP_ALWAYS_INLINE Command(const Command&) = delete;
	VIP_ALWAYS_INLINE Command& operator=(const Command&) = delete;

	/* VIP_ALWAYS_INLINE Command(Command&& other)
	  : CommandHolder(std::move(other))
	{
	}
	VIP_ALWAYS_INLINE Command& operator=(Command&& other)
	{
		static_cast<CommandHolder&>(*this) = std::move(static_cast<CommandHolder&>(other));
		return *this;
	}*/

	template<class T>
	VIP_ALWAYS_INLINE Command(Type _type, const T& value)
	  : CommandHolder(_type, value)
	{
	}

	void apply(QPainter* p) const
	{
		switch (this->commandType()) {

			case DrawNone:
				break;
			case DrawTiledPixmap: {
				const TiledPixmapItem& pix = *this->value<TiledPixmapItem>();
				p->drawTiledPixmap(pix.rect, pix.pixmap, pix.pos);
			} break;
			case DrawPixmap: {
				const PixmapVector& items = *this->value<PixmapVector>();
				for (const PixmapItem& item : items) {
					if (item.has_tr)
						p->setTransform(item.tr);
					p->drawPixmap(item.rect, item.pixmap, item.src);
				}
			} break;
			case DrawImage: {
				const ImageVector& items = *this->value<ImageVector>();
				for (const ImageItem& item : items) {
					if (item.has_tr)
						p->setTransform(item.tr);
					p->drawImage(item.rect, item.image, item.src, item.flags);
				}
			} break;
			case DrawLines: {
				const LineVector& vec = *this->value<LineVector>();
				const QPainter::RenderHints hints = p->renderHints();
				const bool no_antialiaze = !p->transform().isRotating() && (hints & QPainter::Antialiasing) && vec.size() == 1 &&
							   (vec.first().p1().x() == vec.first().p2().x() || vec.first().p1().y() == vec.first().p2().y());
				if (no_antialiaze)
					p->setRenderHint(QPainter::Antialiasing, false);
				p->drawLines(vec.data(), vec.size());
				if (no_antialiaze)
					p->setRenderHints(hints);
			} break;
			case DrawLinesF: {
				const LineFVector& vec = *this->value<LineFVector>();
				const QPainter::RenderHints hints = p->renderHints();
				const bool no_antialiaze = !p->transform().isRotating() && (hints & QPainter::Antialiasing) && vec.size() == 1 &&
							   (vec.first().p1().x() == vec.first().p2().x() || vec.first().p1().y() == vec.first().p2().y());
				if (no_antialiaze)
					p->setRenderHint(QPainter::Antialiasing, false);
				p->drawLines(vec.data(), vec.size());
				if (no_antialiaze)
					p->setRenderHints(hints);
			} break;
			case DrawPoints: {
				const PointVector& vec = *this->value<PointVector>();
				p->drawPoints(vec.data(), vec.size());
			} break;
			case DrawPointsF: {
				const PointFVector& vec = *this->value<PointFVector>();
				p->drawPoints(vec.data(), vec.size());
			} break;
			case DrawRects: {
				const RectVector& vec = *this->value<RectVector>();
				const QPainter::RenderHints hints = p->renderHints();
				const bool no_antialiaze = !p->transform().isRotating() && (hints & QPainter::Antialiasing);
				if (no_antialiaze)
					p->setRenderHint(QPainter::Antialiasing, false);
				p->drawRects(vec.data(), vec.size());
				if (no_antialiaze)
					p->setRenderHints(hints);
			} break;
			case DrawRectsF: {
				const RectFVector& vec = *this->value<RectFVector>();
				const QPainter::RenderHints hints = p->renderHints();
				const bool no_antialiaze = !p->transform().isRotating() && (hints & QPainter::Antialiasing);
				if (no_antialiaze)
					p->setRenderHint(QPainter::Antialiasing, false);
				p->drawRects(vec.data(), vec.size());
				if (no_antialiaze)
					p->setRenderHints(hints);
			} break;
			case DrawEllipse: {
				const QRect& r = *this->value<QRect>();
				p->drawEllipse(r);
			} break;
			case DrawEllipseF: {
				const QRectF& r = *this->value<QRectF>();
				p->drawEllipse(r);
			} break;
			case DrawPath: {
				const QPainterPath& path = *this->value<QPainterPath>();
				p->drawPath(path);
			} break;
			case DrawOddPolygon: {
				const PointVector& vec = *this->value<PointVector>();
				p->drawPolygon(vec.data(), vec.size());
			} break;
			case DrawWiddingPolygon: {
				const PointVector& vec = *this->value<PointVector>();
				p->drawPolygon(vec.data(), vec.size(), Qt::WindingFill);
			} break;
			case DrawPolyline: {
				const PointVector& vec = *this->value<PointVector>();
				p->drawPolyline(vec.data(), vec.size());
			} break;
			case DrawOddPolygonF: {
				const PointFVector& vec = *this->value<PointFVector>();
				p->drawPolygon(vec.data(), vec.size());
			} break;
			case DrawWiddingPolygonF: {
				const PointFVector& vec = *this->value<PointFVector>();
				p->drawPolygon(vec.data(), vec.size(), Qt::WindingFill);
			} break;
			case DrawPolylineF: {
				const PointFVector& vec = *this->value<PointFVector>();
				p->drawPolyline(vec.data(), vec.size());
			} break;
			case ChangeState: {
				const PaintEngineState& state = *this->value<PaintEngineState>();
				QPaintEngine::DirtyFlags flags = state.dirty;

				if (flags & QPaintEngine::DirtyTransform)
					p->setTransform(state.worldMatrix);
				if (flags & QPaintEngine::DirtyBrush)
					p->setBrush(state.brush);
				if (flags & QPaintEngine::DirtyBrushOrigin)
					p->setBrushOrigin(state.brushOrigin);
				if (flags & QPaintEngine::DirtyBackground)
					p->setBackground(state.bgBrush);
				if (flags & QPaintEngine::DirtyBackgroundMode)
					p->setBackgroundMode(state.backgroundMode);
				if (flags & QPaintEngine::DirtyClipPath)
					p->setClipPath(state.clipPath, state.clipOperation);
				if (flags & QPaintEngine::DirtyClipRegion)
					p->setClipRegion(state.clipRegion, state.clipOperation);
				if (flags & QPaintEngine::DirtyCompositionMode)
					p->setCompositionMode(state.composition_mode);
				if (flags & QPaintEngine::DirtyFont)
					p->setFont(state.font);
				if (flags & QPaintEngine::DirtyClipEnabled)
					p->setClipping(state.clipEnabled);
				if (flags & QPaintEngine::DirtyPen)
					p->setPen(state.pen);
				if (flags & QPaintEngine::DirtyHints)
					p->setRenderHints(state.renderHints);
				if (flags & QPaintEngine::DirtyOpacity)
					p->setOpacity(state.opacity);
			} break;
			case DrawText: {
				const TextVector& items = *this->value<TextVector>();
				for (const TextItem& item : items) {
					if (item.has_tr)
						p->setTransform(item.tr);
					p->setFont(item.font);
					p->drawText(item.pos, item.item);
				}
			} break;
		}
	}
};

template<class T>
static Command* push_back_cmd(Command*& cmd, Command::Type cmd_type, const T& value)
{
	if (!cmd) {
		cmd = new Command(cmd_type, value);
		cmd->next = cmd->prev = cmd;
		cmd->count = 1;
		return cmd;
	}
	Command* _new = new Command(cmd_type, value);
	_new->prev = cmd->prev;
	_new->next = cmd;
	cmd->prev->next = _new;
	cmd->prev = _new;
	cmd->count++;
	return _new;
}

static void pop_back_cmd(Command*& cmd)
{
	if (cmd->count == 1) {
		delete cmd;
		cmd = nullptr;
		return;
	}

	CommandHolder* back = cmd->prev;
	back->next->prev = back->prev;
	back->prev->next = back->next;
	cmd->count--;
	delete back;
}

static void clear_cmd(Command*& cmd)
{
	if (!cmd)
		return;
	Command* end = cmd;
	Command* c = cmd;
	do {
		Command* next = static_cast<Command*>(c->next);
		delete c;
		c = next;
	} while (c != end);
	cmd = nullptr;
}

using CommandList = std::vector<Command>;

template<class Device>
struct PicturePaintEngine : public QPaintEngine
{
	// Device must provide emplace_back(), count(), back(), back_back(), pop_back() and send()
	QPaintEngine::Type d_type;
	bool d_batch_rendering;
	Device* device;

public:
	PicturePaintEngine(Device* dev, QPaintEngine::Type type = QPaintEngine::Windows, bool batch_rendering = true)
	  : QPaintEngine(QPaintEngine::AllFeatures)
	  , d_type(type)
	  , d_batch_rendering(batch_rendering)
	  , device(dev)
	{
	}

	virtual bool begin(QPaintDevice* pdev) { return true; }
	virtual void drawEllipse(const QRectF& rect)
	{
		device->emplace_back(Command::DrawEllipseF, rect);
		device->send();
	}
	virtual void drawEllipse(const QRect& rect)
	{
		device->emplace_back(Command::DrawEllipse, rect);
		device->send();
	}
	virtual void drawImage(const QRectF& rectangle, const QImage& image, const QRectF& sr, Qt::ImageConversionFlags flags = Qt::AutoColor)
	{
		if (d_batch_rendering && device->count() > 1) {
			// previous command is also text: merge
			if (device->back().commandType() == Command::DrawImage) {
				ImageVector& vec = *device->back().template value<ImageVector>();
				vec.push_back(ImageItem{ rectangle, image, sr, flags, QTransform(), false });
				return;
			}
			if (device->back().commandType() == Command::ChangeState) {
				PaintEngineState* state = device->back().template value<PaintEngineState>();
				// Get command before ChangeState
				Command& c = device->back_back();
				if (c.commandType() == Command::DrawImage && state->dirty == QPaintEngine::DirtyTransform) {
					// Ok, we as a new text to draw, 2 commands before was also image drawing, and
					// the command in-between is a transform (ony) change: we can merge (and remove the intermediate state change)
					QTransform tr = state->worldMatrix;

					// remove state change
					device->pop_back();
					ImageVector& vec = *device->back().template value<ImageVector>();
					vec.push_back(ImageItem{ rectangle, image, sr, flags, tr, true });
					return;
				}
			}
		}

		// send previous
		device->send();

		ImageVector tmp;
		tmp.push_back(ImageItem{ rectangle, image, sr, flags, QTransform(), false });
		device->emplace_back(Command::DrawImage, tmp);
	}

	virtual void drawLines(const QLineF* lines, int lineCount)
	{
		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::DrawLinesF) {
			LineFVector& vec = *device->back().template value<LineFVector>();
			if (lineCount == 1) {
				const QLineF& l = vec.back();
				bool prev_no_alias = l.p1().x() == l.p2().x() || l.p1().y() == l.p2().y();
				bool current_no_alias = lines[0].p1().x() == lines[0].p2().x() || lines[0].p1().y() == lines[0].p2().y();
				if (current_no_alias == prev_no_alias) {
					// Merge
					vec.push_back(lines[0]);
					return;
				}
			}
		}
		// send previous
		device->send();
		device->emplace_back(Command::DrawLinesF, toVector(lines, lines + lineCount));
	}
	virtual void drawLines(const QLine* lines, int lineCount)
	{
		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::DrawLines) {
			LineVector& vec = *device->back().template value<LineVector>();
			if (lineCount == 1) {
				const QLine& l = vec.back();
				bool prev_no_alias = l.p1().x() == l.p2().x() || l.p1().y() == l.p2().y();
				bool current_no_alias = lines[0].p1().x() == lines[0].p2().x() || lines[0].p1().y() == lines[0].p2().y();
				if (current_no_alias == prev_no_alias) {
					// Merge
					vec.push_back(lines[0]);
					return;
				}
			}
		}
		// send previous
		device->send();
		device->emplace_back(Command::DrawLines, toVector(lines, lines + lineCount));
	}
	virtual void drawPath(const QPainterPath& path)
	{
		device->emplace_back(Command::DrawPath, path);
		device->send();
	}
	virtual void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr)
	{

		if (d_batch_rendering && device->count() > 1) {
			// previous command is also text: merge
			if (device->back().commandType() == Command::DrawPixmap) {
				PixmapVector& vec = *device->back().template value<PixmapVector>();
				vec.push_back(PixmapItem{ r, pm, sr, QTransform(), false });
				return;
			}
			if (device->back().commandType() == Command::ChangeState) {
				PaintEngineState* state = device->back().template value<PaintEngineState>();
				// Get command before ChangeState
				Command& c = device->back_back();
				if (c.commandType() == Command::DrawPixmap && state->dirty == QPaintEngine::DirtyTransform) {
					// Ok, we as a new text to draw, 2 commands before was also pixmap drawing, and
					// the command in-between is a transform (ony) change: we can merge (and remove the intermediate state change)
					QTransform tr = state->worldMatrix;

					// remove state change
					device->pop_back();
					PixmapVector& vec = *device->back().template value<PixmapVector>();
					vec.push_back(PixmapItem{ r, pm, sr, tr, true });
					return;
				}
			}
		}
		// send previous
		device->send();

		PixmapVector tmp;
		tmp.push_back(PixmapItem{ r, pm, sr, QTransform(), false });
		device->emplace_back(Command::DrawPixmap, tmp);
	}
	virtual void drawPoints(const QPointF* points, int pointCount)
	{

		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::DrawPointsF) {
			PointFVector& vec = *device->back().template value<PointFVector>();
			for (int i = 0; i < pointCount; ++i)
				vec.push_back(points[i]);
			return;
		}
		// send previous
		device->send();

		device->emplace_back(Command::DrawPointsF, toVector(points, points + pointCount));
	}
	virtual void drawPoints(const QPoint* points, int pointCount)
	{
		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::DrawPoints) {
			PointVector& vec = *device->back().template value<PointVector>();
			for (int i = 0; i < pointCount; ++i)
				vec.push_back(points[i]);
			return;
		}
		// send previous
		device->send();
		device->emplace_back(Command::DrawPoints, toVector(points, points + pointCount));
	}
	virtual void drawPolygon(const QPointF* points, int pointCount, QPaintEngine::PolygonDrawMode mode)
	{
		switch (mode) {
			case QPaintEngine::OddEvenMode:
			case QPaintEngine::ConvexMode:
				device->emplace_back(Command::DrawOddPolygonF, (toVector(points, points + pointCount)));
				break;
			case QPaintEngine::WindingMode:
				device->emplace_back(Command::DrawWiddingPolygonF, (toVector(points, points + pointCount)));
				break;
			case QPaintEngine::PolylineMode:
				device->emplace_back(Command::DrawPolylineF, (toVector(points, points + pointCount)));
				break;
		}
		device->send();
	}
	virtual void drawPolygon(const QPoint* points, int pointCount, QPaintEngine::PolygonDrawMode mode)
	{
		switch (mode) {
			case QPaintEngine::OddEvenMode:
			case QPaintEngine::ConvexMode:
				device->emplace_back(Command::DrawOddPolygon, (toVector(points, points + pointCount)));
				break;
			case QPaintEngine::WindingMode:
				device->emplace_back(Command::DrawWiddingPolygon, (toVector(points, points + pointCount)));
				break;
			case QPaintEngine::PolylineMode:
				device->emplace_back(Command::DrawPolyline, (toVector(points, points + pointCount)));
				break;
		}
		device->send();
	}
	virtual void drawRects(const QRectF* rects, int rectCount)
	{
		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::DrawRectsF) {
			RectFVector& vec = *device->back().template value<RectFVector>();
			for (int i = 0; i < rectCount; ++i)
				vec.push_back(rects[i]);
			return;
		}
		// send previous
		device->send();
		device->emplace_back(Command::DrawRectsF, toVector(rects, rects + rectCount));
	}
	virtual void drawRects(const QRect* rects, int rectCount)
	{
		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::DrawRects) {
			RectVector& vec = *device->back().template value<RectVector>();
			for (int i = 0; i < rectCount; ++i)
				vec.push_back(rects[i]);
			return;
		}
		// send previous
		device->send();
		device->emplace_back(Command::DrawRects, toVector(rects, rects + rectCount));
	}
	virtual void drawTextItem(const QPointF& p, const QTextItem& textItem)
	{
		if (d_batch_rendering && device->count() > 1) {
			// previous command is also text: merge
			if (device->back().commandType() == Command::DrawText) {
				TextVector& vec = *device->back().template value<TextVector>();
				vec.push_back(TextItem{ textItem.text(), p, textItem.font(), QTransform(), false });
				return;
			}
			if (device->back().commandType() == Command::ChangeState) {
				PaintEngineState* state = device->back().template value<PaintEngineState>();
				// Get command before ChangeState
				Command& c = device->back_back();
				if (c.commandType() == Command::DrawText && state->dirty == QPaintEngine::DirtyTransform) {
					// Ok, we as a new text to draw, 2 commands before was also text drawing, and
					// the command in-between is a transform (ony) change: we can merge (and remove the intermediate state change)
					QTransform tr = state->worldMatrix;

					// remove state change
					device->pop_back();
					TextVector& vec = *device->back().template value<TextVector>();
					vec.push_back(TextItem{ textItem.text(), p, textItem.font(), tr, true });
					return;
				}
			}
		}

		// send previous
		device->send();

		TextVector tmp;
		tmp.push_back(TextItem{ textItem.text(), p, textItem.font(), QTransform(), false });
		device->emplace_back(Command::DrawText, tmp);
	}
	virtual void drawTiledPixmap(const QRectF& rect, const QPixmap& pixmap, const QPointF& p)
	{
		device->emplace_back(Command::DrawTiledPixmap, TiledPixmapItem{ rect, pixmap, p });
		device->send();
	}
	virtual void updateState(const QPaintEngineState& state)
	{
		QPaintEngine::DirtyFlags flags = state.state();
		if (flags == 0)
			return;

		if (d_batch_rendering && device->count() > 0 && device->back().commandType() == Command::ChangeState) {

			PaintEngineState& last = *device->back().template value<PaintEngineState>();
			if (!(state.state() & QPaintEngine::DirtyTransform)) {
				// Merge
				last.dirty |= flags;

				if (flags & QPaintEngine::DirtyBrush)
					last.brush = (state.brush());
				if (flags & QPaintEngine::DirtyBrushOrigin)
					last.brushOrigin = state.brushOrigin();
				if (flags & QPaintEngine::DirtyBackground)
					last.bgBrush = state.backgroundBrush();
				if (flags & QPaintEngine::DirtyBackgroundMode)
					last.backgroundMode = (state.backgroundMode());
				if (flags & QPaintEngine::DirtyClipPath) {
					last.clipOperation = state.clipOperation();
					last.clipPath = (state.clipPath());
				}
				if (flags & QPaintEngine::DirtyClipRegion) {
					last.clipOperation = state.clipOperation();
					last.clipRegion = (state.clipRegion());
				}
				if (flags & QPaintEngine::DirtyCompositionMode)
					last.composition_mode = (state.compositionMode());
				if (flags & QPaintEngine::DirtyFont)
					last.font = (state.font());
				// if (flags & QPaintEngine::DirtyTransform)
				//	last.worldMatrix = (state.transform());
				if (flags & QPaintEngine::DirtyClipEnabled)
					last.clipEnabled = (state.isClipEnabled());
				if (flags & QPaintEngine::DirtyPen)
					last.pen = (state.pen());
				if (flags & QPaintEngine::DirtyHints)
					last.renderHints = (state.renderHints());
				if (flags & QPaintEngine::DirtyOpacity)
					last.opacity = (state.opacity());

				return;
			}
		}
		device->emplace_back(Command::ChangeState, state);
	}
	virtual QPaintEngine::Type type() const { return d_type; }
	virtual bool end() { return true; }
};

class VipPicture::PrivateData
{
public:
	PrivateData()
	  : engine(this)
	{
	}
	~PrivateData() { clear_cmd(commands); }
	Command* commands{ nullptr };
	QPaintEngine::Type type{ QPaintEngine ::Windows };
	PicturePaintEngine<PrivateData> engine;
	bool isBatchRenderingEnabled{ true };

	int count() const noexcept { return commands ? commands->count : 0; }
	template<class T>
	void emplace_back(Command::Type cmd_type, const T& value)
	{
		push_back_cmd(commands, cmd_type, value);
	}
	void pop_back() { pop_back_cmd(commands); }
	Command& back() noexcept { return *static_cast<Command*>(commands->prev); }
	const Command& back() const noexcept { return *static_cast<Command*>(commands->prev); }
	Command& back_back() noexcept { return *static_cast<Command*>(commands->prev->prev); }
	const Command& back_back() const noexcept { return *static_cast<Command*>(commands->prev->prev); }
	void send() {}
};

VipPicture::VipPicture(QPaintEngine::Type type)
  : d_ptr(new PrivateData())
{
	d_ptr->type = type;
	d_ptr->engine.d_type = type;
	d_ptr->engine.d_batch_rendering = d_ptr->isBatchRenderingEnabled;
}

VipPicture::~VipPicture() {}

VipPicture::VipPicture(const VipPicture& other)
  : d_ptr(other.d_ptr)
{
}
VipPicture& VipPicture::operator=(const VipPicture& other)
{
	d_ptr = other.d_ptr;
	return *this;
}

void VipPicture::setBatchRenderingEnabled(bool enable)
{
	d_ptr->isBatchRenderingEnabled = enable;
	d_ptr->engine.d_batch_rendering = enable;
}
bool VipPicture::isBatchRenderingEnabled() const
{
	return d_ptr->isBatchRenderingEnabled;
}

bool VipPicture::isEmpty() const
{
	return size() == 0;
}

uint VipPicture::size() const
{
	return d_ptr->count();
}

bool VipPicture::play(QPainter* p) const
{
	if (!d_ptr->commands)
		return true;

	Command* c = d_ptr->commands;
	Command* end = c;
	do {

		c->apply(p);
		c = static_cast<Command*>(c->next);

	} while (c != end);

	return true;
}

void VipPicture::clear()
{
	clear_cmd(d_ptr->commands);
}

QPaintEngine* VipPicture::paintEngine() const
{
	return &d_ptr->engine;
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

int VipPicture::metric(PaintDeviceMetric m) const
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
			qWarning("VipPicture::metric: Invalid metric command");
	}
	return val;
}

class DummyPaintEngine : public QPaintEngine
{

public:
	DummyPaintEngine()
	  : QPaintEngine(QPaintEngine::AllFeatures)
	{
	}
	virtual bool begin(QPaintDevice* pdev) { return true; }
	virtual void drawEllipse(const QRectF& rect) {}
	virtual void drawEllipse(const QRect& rect) {}
	virtual void drawImage(const QRectF& rectangle, const QImage& image, const QRectF& sr, Qt::ImageConversionFlags flags = Qt::AutoColor) {}

	virtual void drawLines(const QLineF* lines, int lineCount) {}
	virtual void drawLines(const QLine* lines, int lineCount) {}
	virtual void drawPath(const QPainterPath& path) {}
	virtual void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) {}
	virtual void drawPoints(const QPointF* points, int pointCount) {}
	virtual void drawPoints(const QPoint* points, int pointCount) {}
	virtual void drawPolygon(const QPointF* points, int pointCount, QPaintEngine::PolygonDrawMode mode) {}
	virtual void drawPolygon(const QPoint* points, int pointCount, QPaintEngine::PolygonDrawMode mode) {}
	virtual void drawRects(const QRectF* rects, int rectCount) {}
	virtual void drawRects(const QRect* rects, int rectCount) {}
	virtual void drawTextItem(const QPointF& p, const QTextItem& textItem) {}
	virtual void drawTiledPixmap(const QRectF& rect, const QPixmap& pixmap, const QPointF& p) {}
	virtual void updateState(const QPaintEngineState& state) {}

	virtual QPaintEngine::Type type() const { return QPaintEngine::Windows; }
	virtual bool end() { return true; }
};

static bool& is_in_opengl_widget_paint()
{
	static bool inst = false;
	return inst;
}

struct OpenGLWindow : public QWindow
{
	struct Thread : QThread
	{
		QOpenGLContext* thread_context{ nullptr };
		QWindow* widget{ nullptr };
		Command* to_draw{ nullptr };

		VipSpinlock lock;
		std::condition_variable_any cond;
		std::atomic<bool> inRender{ false };
		std::atomic<bool> finished{ false };
		QColor back{ Qt::transparent };

		void setBackground(const QColor& c) { back = c; }
		QColor background() const { return back; }

		void add(Command* cmd)
		{
			VipUniqueLock<VipSpinlock> ll(lock);
			if (!to_draw) {
				// vip_debug("set %i \n", cmd->count);
				to_draw = cmd;
			}
			else {
				// merge commands
				to_draw->prev->next = cmd;
				to_draw->prev = cmd->prev;
				cmd->prev->next = to_draw;
				cmd->prev = to_draw;
				to_draw->count += cmd->count;
				cmd->count = 0;
			}
			cond.notify_one();
		}

		void startRendering()
		{
			inRender = true;
			finished = false;
		}
		void stopRendering()
		{
			inRender = false;
			// to_draw = p;
			// cond.notify_one();
		}

		virtual void run()
		{
			thread_context = new QOpenGLContext;
			thread_context->setFormat(widget->requestedFormat());
			thread_context->create();

			while (QWindow* v = widget) {

				// wait for window to be exposed
				if (!v->isExposed()) {
					this->finished = true;
					vipSleep(5);
					continue;
				}

				// retrieve commands
				Command* cmd = nullptr;
				{
					VipUniqueLock<VipSpinlock> ll(lock);
					if (!to_draw)
						cond.wait_for(lock, std::chrono::milliseconds(1));
					cmd = to_draw;
					to_draw = nullptr;
				}
				if (!cmd) {
					if (this->inRender.load() == false)
						this->finished.store(true);
					continue;
				}

				// Start rendering loop
				thread_context->makeCurrent(v);
				glClearColor(back.red() * 0.00390625, back.green() * 0.00390625, back.blue() * 0.00390625, back.alpha() * 0.00390625);
				glClear(GL_COLOR_BUFFER_BIT);
				QOpenGLPaintDevice device(v->size());
				QPainter p;
				p.begin(&device);

				do {
					if (!cmd) {
						std::this_thread::yield();
					}
					else {

						Command* c = cmd;
						Command* end = c;

						// draw and delete all commands
						int drawn = 0;
						int to_draw_count = cmd->count;
						do {
							c->apply(&p);
							Command* next = static_cast<Command*>(c->next);
							delete c;
							c = next;
							++drawn;
						} while (c != end);

						// if (to_draw_count > 200)
						//	vip_debug("drawn: %i\n", to_draw_count);
						if (to_draw_count != drawn)
							vip_debug("error drawing!!!\n");
					}
					cmd = nullptr;
					VipUniqueLock<VipSpinlock> ll(lock);
					if (!to_draw)
						cond.wait(lock, [&]() { return to_draw != nullptr || inRender.load(std::memory_order_relaxed) == false; });
					cmd = to_draw;
					to_draw = nullptr;

				} while (inRender.load() || cmd);

				p.end();

				thread_context->swapBuffers(v);
				thread_context->doneCurrent();

				// qint64 el = QDateTime::currentMSecsSinceEpoch() -st;
				// vip_debug("ogl: %i ms\n", (int)el);
				finished = true;
				cond.notify_one();
			}

			delete thread_context;
		}
	};

	Thread thread;
	DummyPaintEngine dummyEngine;
	PicturePaintEngine<OpenGLWindow> trueEngine;
	Command* commands{ nullptr };
	bool inRender{ false };
	QWidget* top_level{ nullptr };
	qint64 lastRequestActive{ 0 };
	int sentCount{ 0 };

public:
	// members for PicturePaintEngine

	int count() const noexcept { return commands ? commands->count : 0; }
	template<class T>
	void emplace_back(Command::Type cmd_type, const T& value)
	{
		push_back_cmd(commands, cmd_type, value);
	}
	void pop_back() { pop_back_cmd(commands); }
	Command& back() noexcept { return *static_cast<Command*>(commands->prev); }
	const Command& back() const noexcept { return *static_cast<Command*>(commands->prev); }
	Command& back_back() noexcept { return *static_cast<Command*>(commands->prev->prev); }
	const Command& back_back() const noexcept { return *static_cast<Command*>(commands->prev->prev); }
	void send()
	{
		if (commands && commands->count > 20) {
			sentCount += commands->count;
			thread.add(commands);
			commands = nullptr;
		}
	}

	OpenGLWindow(QWidget* top)
	  : QWindow()
	  , trueEngine(this, QPaintEngine::OpenGL2)
	  , top_level(top)
	{
		setSurfaceType(QSurface::OpenGLSurface);
		setFormat(QSurfaceFormat::defaultFormat());
		thread.widget = this;
		thread.start();
	}
	~OpenGLWindow()
	{
		thread.widget = nullptr;
		thread.wait();
		clear_cmd(commands);
	}

	void startRendering()
	{
		is_in_opengl_widget_paint() = true;
		sentCount = 0;
		inRender = true;
		thread.startRendering();
	}
	void stopRendering()
	{

		if (commands) {
			sentCount += commands->count;
			thread.add(commands);
			commands = nullptr;
		}

		inRender = false;
		thread.stopRendering();

		if (sentCount != 0) {
			// Only wait if at least one paint command has been emitted
			while (!thread.finished.load()) {
				VipUniqueLock<VipSpinlock> ll(thread.lock);
				thread.cond.wait_for(thread.lock, std::chrono::milliseconds(1));
			}
		}
		is_in_opengl_widget_paint() = false;
		// vip_debug("count: %i\n", sentCount);
	}

	virtual void keyPressEvent(QKeyEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void keyReleaseEvent(QKeyEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void mouseDoubleClickEvent(QMouseEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void mouseMoveEvent(QMouseEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void mousePressEvent(QMouseEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void mouseReleaseEvent(QMouseEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void tabletEvent(QTabletEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void touchEvent(QTouchEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void wheelEvent(QWheelEvent* ev) { qApp->sendEvent(top_level, ev); }
	virtual void focusInEvent(QFocusEvent* ev)
	{
		qint64 time = QDateTime::currentMSecsSinceEpoch();
		if (time - lastRequestActive < 100)
			return;

		top_level->setFocus();
		this->requestActivate();
		lastRequestActive = QDateTime::currentMSecsSinceEpoch();
	}
	virtual void focusOutEvent(QFocusEvent* ev) { qApp->sendEvent(top_level, ev); }

	/* virtual bool event(QEvent* event)
	{
		switch (event->type()) {
			case QEvent::UpdateRequest:
			case QEvent::UpdateLater:
			case QEvent::Paint:
				qApp->notify(top_level, event);
				return true;
			default:
				return QWindow::event(event);
		}
	}*/

	QPaintEngine* paintEngine() const
	{
		if (inRender)
			return const_cast<PicturePaintEngine<OpenGLWindow>*>(&trueEngine);
		return const_cast<DummyPaintEngine*>(&dummyEngine);
	}
};

class VipOpenGLWidget::PrivateData
{
public:
	OpenGLWindow* window{ nullptr };
};

VipOpenGLWidget::VipOpenGLWidget(QWidget* parent)
  : detail::VipWindowContainer(new OpenGLWindow(this), parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->window = static_cast<OpenGLWindow*>(this->containedWindow());
	d_data->window->show();

	this->setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_OpaquePaintEvent);
	this->setMouseTracking(true);
}

VipOpenGLWidget::~VipOpenGLWidget()
{
}

QPaintEngine* VipOpenGLWidget::paintEngine() const
{
	return d_data->window->paintEngine();
}

void VipOpenGLWidget::setBatchRenderingEnabled(bool enable)
{
	d_data->window->trueEngine.d_batch_rendering = (enable);
}
bool VipOpenGLWidget::isBatchRenderingEnabled() const
{
	return d_data->window->trueEngine.d_batch_rendering;
}

void VipOpenGLWidget::startRendering()
{
	d_data->window->startRendering();
}
void VipOpenGLWidget::stopRendering()
{
	d_data->window->stopRendering();
}

bool VipOpenGLWidget::isInPainting()
{
	return is_in_opengl_widget_paint();
}

void VipOpenGLWidget::setBackgroundColor(const QColor& c)
{
	d_data->window->thread.setBackground(c);
}
QColor VipOpenGLWidget::backgroundColor() const
{
	return d_data->window->thread.background();
}
