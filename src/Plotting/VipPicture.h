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

#ifndef VIP_PICTURE_H
#define VIP_PICTURE_H

#include "VipConfig.h"

#include <qpaintdevice.h>
#include <qpaintengine.h>
#include <qsharedpointer.h>
#include <qwidget.h>

/// @brief A QPaintDevice similar to QPicture and optimized for opengl rendering
///
/// VipPicture is similar to QPicture: it stores paint commands and replay them
/// using VipPicture::play() function.
///
/// The main differences compared to QPicture are:
///
/// -	VipPicture is WAY faster to record paint commands.
///		This is because QPicture serialize commands in a buffer to provide faster serialization,
///		a feature not provided by VipPicture.
///
/// -	VipPicture allows batch rendering: paint commands are (when possible) merged
///		together to reduce state changes and drawing commands when rendering the VipPicture
///		with the play() member. This is especially usefull when rendering the picture into
///		an opengl based paint device. Batch rendering can be disable using setBatchRenderingEnabled() member.
///
/// -	A VipPicture object keeps recording commands, whatever the number of QPainter used to
///		draw on it. Use clear() to remove all recorded commands.
///
///	-	VipPicture uses shared ownership.
///
class VIP_PLOTTING_EXPORT VipPicture : public QPaintDevice
{
public:
	VipPicture(QPaintEngine::Type type = QPaintEngine::Windows);
	~VipPicture();

	VipPicture(const VipPicture&);
	VipPicture& operator=(const VipPicture&);

	void setBatchRenderingEnabled(bool);
	bool isBatchRenderingEnabled() const;

	bool isEmpty() const;
	uint size() const;
	void clear();
	bool play(QPainter* p) const;

	inline void swap(VipPicture& other) noexcept { d_ptr.swap(other.d_ptr); }

	QPaintEngine* paintEngine() const override;

	virtual int metric(QPaintDevice::PaintDeviceMetric metric) const;

private:
	class PrivateData;
	QSharedPointer<PrivateData> d_ptr;
};

namespace detail
{
	/// @brief Similar to QWindowContainer (simplified version), but PUBLIC
	class VIP_PLOTTING_EXPORT VipWindowContainer : public QWidget
	{
		Q_OBJECT
	public:
		explicit VipWindowContainer(QWindow* embeddedWindow, QWidget* parent = nullptr, Qt::WindowFlags f = {});
		~VipWindowContainer();
		QWindow* containedWindow() const;

	protected:
		bool event(QEvent* ev) override;
	private Q_SLOTS:
		void focusWindowChanged(QWindow* focusWindow);

	private:
		
		VIP_DECLARE_PRIVATE_DATA(d_data);
	};

}

/// @brief Widget relying on opengl engine to draw its content
///
/// VipOpenGLWidget is a widget which goal is to draw its content using Qt OpenGL drawing engine.
///
/// In order to use opengl acceleration on a QGraphicsView, the standard approach is to set its viewport
/// to a QOpenGLWidget object. This works in theory, but does not provide much benefits in practice.
/// Indeed, using QOpenGLWidget as viewport does not reduce the CPU usage and the painting takes
/// the same amount of time as the raster engine (at least most of the times).
///
/// VipOpenGLWidget uses a different approach:
///
/// -	Using a QPainter on a VipOpenGLWidget will use its own custom QPaintEngine.
/// -	The paint engine serialize drawing commands in a structure similar to VipPicture.
/// -	Drawing commands are periodically sent to a rendering thread.
/// -	The rendering thread apply the drawing commands in an internal QWindow using QOpenGLPaintDevice.
///
/// This greatly reduces the time spent in QGraphicsView::paintEvent() and allow much higher frame rates.
///
/// Like VipPicture, VipOpenGLWidget allows batch rendering: paint commands are (when possible) merged
///	together to reduce state changes and drawing commands when rendering to the opengl context.
///	Batch rendering can be disable using setBatchRenderingEnabled() member.
///
/// The user must call VipOpenGLWidget::startRendering() before drawing onto a VipOpenGLWidget object,
/// and VipOpenGLWidget::stopRendering() when finished. VipBaseGraphicsView class automatically detect
/// the use of VipOpenGLWidget as viewport and will call these functions when necessary.
///
///
class VIP_PLOTTING_EXPORT VipOpenGLWidget : public detail::VipWindowContainer
{
	Q_OBJECT
public:
	VipOpenGLWidget(QWidget* parent = nullptr);
	~VipOpenGLWidget();

	/// @brief Returns the internal QPaintEngine used to serialize painting commands
	virtual QPaintEngine* paintEngine() const;

	/// @brief Enable/disable batch rendering
	void setBatchRenderingEnabled(bool);
	bool isBatchRenderingEnabled() const;

	/// @brief Set the widget background color
	void setBackgroundColor(const QColor& c);
	QColor backgroundColor() const;

	/// @brief Start rendering. Call this before painting to the VipOpenGLWidget (like at the beginning of paintEvent()).
	void startRendering();
	/// @brief Stop rendering. Call this after painting to the VipOpenGLWidget (like at the end of paintEvent()).
	void stopRendering();

	/// @brief Returns true if we are in-between calls to startRendering() and stopRendering().
	static bool isInPainting();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
