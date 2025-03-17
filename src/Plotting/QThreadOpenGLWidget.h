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

#ifndef Q_PAINT_OPENGL_WIDGET_H
#define Q_PAINT_OPENGL_WIDGET_H

#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QGraphicsWidget>
#include <QSharedPointer>
#include <QWidget>
#include <QSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>

#include <functional>

#ifdef VIP_BUILD_PLOTTING_LIB
// When built from within thermavip
#include "VipConfig.h"
#define EXPORT_DECL VIP_PLOTTING_EXPORT
#else
#ifdef QTOW_BUILD_LIBRARY
#define EXPORT_DECL Q_DECL_EXPORT
#elif defined(QTOW_USE_LIBRARY)
#define EXPORT_DECL Q_DECL_IMPORT
#else
#define EXPORT_DECL // When copy/paste to other projects
#endif
#endif

/// @brief A QPaintDevice similar to QPicture and optimized for opengl rendering
///
/// QPaintRecord is similar to QPicture: it stores paint commands and replay them
/// using QPaintRecord::play() function.
///
/// The main differences compared to QPicture are:
///
/// -	QPaintRecord is WAY faster to record paint commands.
///		This is because QPicture serialize commands in a buffer to provide faster serialization,
///		a feature not provided by QPaintRecord.
///
/// -	QPaintRecord allows batch rendering: paint commands are (when possible) merged
///		together to reduce drawing commands when rendering the QPaintRecord
///		with the play() member. This is especially usefull when rendering the picture into
///		an opengl based paint device. Batch rendering can be disable using setOptimization() member.
///
/// -	A QPaintRecord object keeps recording commands, whatever the number of QPainter used to
///		draw on it. Use clear() to remove all recorded commands. Use setEnabled() to temporarily
///		disable paint commands recording.
///
///	QPaintRecord uses shared ownership.
///
class EXPORT_DECL QPaintRecord : public QPaintDevice
{
private:
	class PrivateData;
	QSharedPointer<PrivateData> d_ptr;

public:
	/// @brief Optimization flags
	enum Optimization
	{
		BatchCommands = 0x001,	      //! Enable command batching.
		SmallTextAsLine = 0x002,      //! Small text is rendered using lines when possible.
		StraightLineAliasing = 0x004, //! Vertical and horizontal lines are drawn withour antialiazing when possible (no rotation and cosmetic pen)
		ExtractBoundingRect = 0x008   //! Internal use only
	};
	Q_DECLARE_FLAGS(Optimizations, Optimization)

	/// @brief Construct from a QPaintEngine type and optimization flags
	QPaintRecord(QPaintEngine::Type type = QPaintEngine::Windows, Optimizations opts = BatchCommands);
	QPaintRecord(const QPaintRecord& o) noexcept
	  : d_ptr(o.d_ptr)
	{
	}
	QPaintRecord(QPaintRecord&& o) noexcept
	  : d_ptr(std::move(o.d_ptr))
	{
	}
	~QPaintRecord() noexcept = default;

	QPaintRecord& operator=(const QPaintRecord& o) noexcept
	{
		d_ptr = o.d_ptr;
		return *this;
	}
	QPaintRecord& operator=(QPaintRecord&& o) noexcept
	{
		d_ptr = std::move(o.d_ptr);
		return *this;
	}

	/// @brief Set/get the optimization flags
	void setOptimizations(Optimizations) noexcept;
	/// @brief Set/get the optimization flags
	void setOptimization(Optimization, bool) noexcept;
	/// @brief Set/get the optimization flags
	Optimizations optimizations() const noexcept;
	/// @brief Returns true if given optimization flag is set
	bool testOptimization(Optimization) const noexcept;

	/// @brief Returns true is the QPaintRecord is null or empty (size() == 0)
	bool isEmpty() const noexcept;
	/// @brief Returns the number of recorded painting commands
	uint size() const noexcept;
	/// @brief Remove all painting commands
	void clear();
	/// @brief Draw the QPaintRecord on given QPainter object
	bool play(QPainter* p) const;
	/// @brief Draw the QPaintRecord on given QPainter object with custom optimization flags
	bool play(QPainter* p, Optimizations opts) const;

	/// @brief Returns the exact QPaintRecord bounding rectangle
	QRectF boundingRect() const;
	/// @brief Returns an estimation of the QPaintRecord bounding rectangle.
	/// This function ignore non ascii or rich texts as extracting their bounding
	/// boxes is very costly. Ascii texts are still considered.
	QRectF estimateBoundingRect() const;

	void swap(QPaintRecord& other) noexcept { d_ptr.swap(other.d_ptr); }

	/// @brief Enable/disable paint command recording
	void setEnabled(bool enable) noexcept;
	bool isEnabled() const noexcept;

	virtual QPaintEngine* paintEngine() const override;
	virtual int metric(QPaintDevice::PaintDeviceMetric metric) const override;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QPaintRecord::Optimizations)

/// @brief Widget relying on opengl engine to draw its content
///
/// QThreadOpenGLWidget is a widget drawing its content using Qt OpenGL drawing engine,
/// and could be used as a drop in replacement for QOpenGLWidget in most cases.
///
/// In order to use opengl acceleration when drawing on a widget or when using a QGraphicsView,
/// the standard approach is to use QOpenGLWidget class. This works in theory, but does not
/// provide much benefits in practice. Indeed, using QOpenGLWidget does not reduce the CPU
/// usage and the painting takes the same amount of time as the raster engine (at least most of the times).
///
/// QThreadOpenGLWidget uses a different approach:
///
/// -	Using a QPainter on a QThreadOpenGLWidget will use its own custom QPaintEngine.
/// -	The paint engine serializes drawing commands in a structure similar to QPaintRecord.
/// -	Drawing commands are periodically sent to a rendering thread.
/// -	The rendering thread apply the drawing commands in an internal QWindow using QOpenGLPaintDevice.
///		If the opengl thread is slower than the time spent in recording drawing commands, it will
///		discard older commands and only paint the last ones.
///
/// This greatly reduces the time spent in QWidget::paintEvent(), allow higher frame rates and
/// increase the GUI responsiveness.
///
/// Note that QThreadOpenGLWidget::paintEvent() does not wait for the opengl rendering thread to finish its work
/// in order to keep the GUI responsive on high loads. It is possible to partially or totally wait for the rendering
/// thread by specifying a maximum amount of time spent in QThreadOpenGLWidget::paintEvent() using setMaximumPaintTime().
///
/// Like QPaintRecord, QThreadOpenGLWidget allows batch rendering: paint commands are (when possible) merged
///	together to reduce drawing commands when rendering to the opengl context.
///	Batch rendering can be disable using setOptimization() member.
///
/// QThreadOpenGLWidget can be used outside of a QGraphicsView, like a regular widget or to replace QOpenGLWidget.
/// To use regular drawing based on QPainter, a subclass must override paintEvent() much like any other widget.
/// A subclass can also override initializeGL(), paintGL() and resizeGL() members to provide a similar behavior
/// to QOpenGLWidget. In any case, the actual rendering will take place in the dedicated thread.
///
/// QThreadOpenGLWidget provides the drawFunction() method that directly send a drawing function to
/// the rendering thread. Using this function is much faster than direct QPainter based drawing,
/// as it avoids going through the paint commands serialization layer. drawFunction() can be used
/// to perform QPainter based drawing as well as opengl based drawing (see example below).
/// Note that drawFunction() can only be called from within paintEvent() member.
///
/// QThreadOpenGLWidget internally uses a QWindowContainer through QWidget::createWindowContainer(). Therefore,
/// all limitations described in QWidget::createWindowContainer() documentation apply to QThreadOpenGLWidget itself.
/// If you observe display glitches on large applications, QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings)
/// might help.
///
///
/// The following example shows the different ways to use QThreadOpenGLWidget
/// in order to display ellipses:
///
/// \code{cpp}
///
/// // Example of QThreadOpenGLWidget that displays static and dynamic ellipses
/// // by combining all possible types of drawing mechanisms:
/// //  - Overload of QThreadOpenGLWidget::paintGL()
/// //  - Calling drawFunction() from within paintEvent()
/// //  - Using a regular QPainter from within paintEvent()
/// //
/// class MyOpenGLWidget : public QThreadOpenGLWidget
/// {
/// 	static void DrawOpenGLEllipse(const QRectF& r, int num_segments)
/// 	{
/// 		// Draw an ellipse using old school opengl, just for the use case
///
/// 		float cx = r.center().x();
/// 		float cy = r.center().y();
/// 		float rx = r.width() / 2;
/// 		float ry = r.height() / 2;
///
/// 		float theta = 2 * 3.1415926 / float(num_segments);
/// 		float c = std::cosf(theta);//precalculate the sine and cosine
/// 		float s = std::sinf(theta);
/// 		float t;
/// 		float x = 1;//we start at angle = 0
/// 		float y = 0;
/// 		glEnable(GL_MULTISAMPLE);
/// 		glEnable(GL_COLOR_MATERIAL);
/// 		glColor3f(0, 1, 0);
/// 		glBegin(GL_LINE_LOOP);
/// 		for (int ii = 0; ii < num_segments; ii++)
/// 		{
/// 			//apply radius and offset
/// 			glVertex2f(x * rx + cx, y * ry + cy);//output vertex
///
/// 			//apply the rotation matrix
/// 			t = x;
/// 			x = c * x - s * y;
/// 			y = s * t + c * y;
/// 		}
/// 		glEnd();
/// 	}
///
/// public:
/// 	QTimer timer;
///
/// 	MyOpenGLWidget()
/// 		:QThreadOpenGLWidget()
/// 	{
/// 		timer.setSingleShot(false);
/// 		timer.setInterval(10);
/// 		connect(&timer, SIGNAL(timeout()), this, SLOT(update()));
/// 		timer.start();
/// 	}
///
/// 	virtual void paintGL()
/// 	{
/// 		// One way to display an ellipse: override paintGL()
/// 		DrawOpenGLEllipse(QRectF(0, 0, 1, 1), 100);
/// 	}
///
/// 	virtual void paintEvent(QPaintEvent* evt)
/// 	{
/// 		qint64 time = QDateTime::currentMSecsSinceEpoch();
/// 		int size = time % 500;
///
/// 		// Another (faster) way to display an ellipse:
/// 		// send a drawing function to the rendering thread
/// 		// that uses QPainter calls
/// 		drawFunction([size](QPainter* p)
/// 			{
/// 				p->setPen(Qt::red);
/// 				p->setRenderHint(QPainter::Antialiasing);
/// 				p->drawEllipse(QRectF(10, 10, size, size));
/// 			});
///
/// 		// Another (faster) way to display an ellipse:
/// 		// send a drawing function using raw opengl calls
/// 		drawFunction([size](QPainter* p)
/// 			{
/// 				p->beginNativePainting();
/// 				MyOpenGLWidget::DrawOpenGLEllipse(QRectF(10 / 500., 10 / 500., size / 500., size / 500.), 500);
/// 				p->endNativePainting();
///
/// 			});
///
/// 		// Yet another way to draw an ellipse:
/// 		// the regular QWidget way
/// 		QPainter p(this);
/// 		p.setPen(Qt::blue);
/// 		p.setRenderHint(QPainter::Antialiasing);
/// 		p.drawEllipse(QRectF(10, 10, 50, 50));
/// 	}
///
/// };
///
/// \endcode
///
class EXPORT_DECL QThreadOpenGLWidget : public QWidget
{
	Q_OBJECT
	friend class QOpenGLItem;
	friend class OpenGLWindow;
	friend class CommandBatch;

public:
	/// @brief Default constructor
	/// Construct the QThreadOpenGLWidget using default surface format (QSurfaceFormat::defaultFormat()).
	QThreadOpenGLWidget(QWidget* parent = nullptr);

	/// @brief Construct from a surface format, optimization flags and parent widget.
	QThreadOpenGLWidget(const QSurfaceFormat& format, QPaintRecord::Optimizations opts = QPaintRecord::BatchCommands, QWidget* parent = nullptr);
	~QThreadOpenGLWidget();

	/// @brief Set/get the optimization flags
	void setOptimizations(QPaintRecord::Optimizations) noexcept;
	void setOptimization(QPaintRecord::Optimization, bool) noexcept;
	QPaintRecord::Optimizations optimizations() const noexcept;
	bool testOptimization(QPaintRecord::Optimization) const noexcept;

	/// @brief Set the maximum painting time, i.e. the maximum time
	/// between startRendering() and stopRendering() calls.
	/// If, after stopRendering(), some remaining time is available,
	/// QThreadOpenGLWidget will wait (if possible) for the actual opengl drawing.
	/// Default value is 0, meaning that the GUI thread never waits for the
	/// opengl drawing.
	void setMaximumPaintTime(uint msecs = 0) noexcept;
	uint maximumPaintTime() const noexcept;

	/// @brief Returns the surface format used to create the underlying QWindow object
	QSurfaceFormat format() const;

	/// @brief Returns the opengl context used by the rendering thread
	QOpenGLContext* context() const;

	/// @brief Returns the internal opengl window
	QWindow* openGLWindow() const;

	/// @brief Convenient function, returns the widget content as a QImage.
	/// Note that QWidget::render() works on QThreadOpenGLWidget, but the
	/// background is never drawn. This function ensures background drawing
	/// if draw_background is true.
	QImage toImage(bool draw_background = true) const;

protected:
	/// @brief Send a drawing function to the rendering thread.
	/// Should only be called from within paintEvent().
	/// This function can be used to send standard drawing
	/// calls based on QPainter, as well as opengl code.
	/// For opengl based drawing, QPaint::beginNativePainting()
	/// and QPainter::endNativePainting() must be called explicitly.
	/// The opengl context can be accessed from the provided function
	/// using QOpenGLContext::currentContext().
	void drawFunction(const std::function<void(QPainter*)>& fun);

	/// @brief Initialize the opengl context.
	/// This function is atomatically called at the beginning
	/// of the rendering thread.
	virtual void initializeGL() {}

	/// @brief Performs opengl drawing.
	/// This function is called within the rendering loop when the widget
	/// needs to be updated.
	virtual void paintGL() {}

	/// @brief Function called from the rendering thread when the widget size changes.
	virtual void resizeGL(int w, int h)
	{
		Q_UNUSED(w);
		Q_UNUSED(h);
	}

protected:
	virtual bool event(QEvent* evt) override;
	virtual bool eventFilter(QObject* watched, QEvent* evt) override;
	virtual QPaintEngine* paintEngine() const override;

private Q_SLOTS:
	void updateParent();
	void changed(const QList<QRectF>& rects);
	void changed() { changed(QList<QRectF>()); }
	void rubberBandChanged(QRect rubberBandRect, QPointF fromScenePoint, QPointF toScenePoint);
	void init(bool show = false);

	/// @brief Start rendering in this QThreadOpenGLWidget.
	/// This method should usually not be called directly,
	/// as QPainter::begin() will do it automatically, and
	/// this function is called before paintEvent().
	/// However, if using multiple QPainter objects do draw on the
	/// QThreadOpenGLWidget, this function might be called explicitly
	/// (along with stopRendering()) to avoid partial drawings.
	void startRendering();
	void stopRendering();

private:
	class PrivateData;
	PrivateData* d_data;
};

/// @brief Base class for QGraphicsItem using caching mechanism based on QPaintRecord.
///
/// A QGraphicsItem inheriting QOpenGLItem can use a dedicated cache mechanism when
/// displayed over a QThreadOpenGLWidget.
///
/// The QGraphicsItem paint() method should call drawThroughCache() in the following way:
///
/// \code{cpp}
///
/// void MyGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) const
/// {
///		if(drawThroughCache(painter,option,widget))
///			return;
///
///		//...
///		// Actual drawing
///		//...
/// }
///
/// \endcode
///
/// As long as the item is not marked as dirty using markItemDirty(), the item will draw itself
/// using a QPaintRecord object. The internal QPaintRecord is regenerated (and drawn) whenever
/// the item is marked as dirty and during the next call to QGraphicsItem::paint().
///
/// Using a QGraphicsItem inheriting QOpenGLItem and a QThreadOpenGLWidget viewport greatly reduce
/// the time spent in QGraphicsView::paintEvent() and increase the overall GUI responsiveness.
///
///
class EXPORT_DECL QOpenGLItem
{
public:
	QOpenGLItem(QGraphicsItem* this_item);
	virtual ~QOpenGLItem();

	/// @brief Mark the item as dirty.
	/// Note that this just set an internal boolean value,
	/// and does not trigger and update.
	/// This function is thread safe.
	void markItemDirty() noexcept;

	/// @brief Draw the item through the cache mechanism.
	/// Returns true if the item was drawn.
	bool drawThroughCache(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) const;

private:
	class PrivateData;
	PrivateData* d_data;
};

/// @brief QGraphicsItem inheriting QOpenGLItem
class EXPORT_DECL QOpenGLGraphicsItem
  : public QGraphicsItem
  , public QOpenGLItem
{
public:
	QOpenGLGraphicsItem(QGraphicsItem* parent = nullptr)
	  : QGraphicsItem(parent)
	  , QOpenGLItem(this)
	{
	}
};

/// @brief QGraphicsObject inheriting QOpenGLItem
class EXPORT_DECL QOpenGLGraphicsObject
  : public QGraphicsObject
  , public QOpenGLItem
{
	Q_OBJECT
public:
	QOpenGLGraphicsObject(QGraphicsItem* parent = nullptr)
	  : QGraphicsObject(parent)
	  , QOpenGLItem(this)
	{
	}
};

/// @brief QGraphicsWidget inheriting QOpenGLItem
class EXPORT_DECL QOpenGLGraphicsWidget
  : public QGraphicsWidget
  , public QOpenGLItem
{
	Q_OBJECT
public:
	QOpenGLGraphicsWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = Qt::WindowFlags())
	  : QGraphicsWidget(parent, wFlags)
	  , QOpenGLItem(this)
	{
		connect(this, &QOpenGLGraphicsWidget::layoutChanged, this, &QOpenGLGraphicsWidget::slotMarkDirty);
		connect(this, &QOpenGLGraphicsWidget::geometryChanged, this, &QOpenGLGraphicsWidget::slotMarkDirty);
	}

private Q_SLOTS:
	void slotMarkDirty() noexcept { this->markItemDirty(); }
};

#endif
