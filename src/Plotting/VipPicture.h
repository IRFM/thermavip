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
		class PrivateData;
		PrivateData* d_data;
	};

}



class VIP_PLOTTING_EXPORT VipOpenGLWidget : public detail::VipWindowContainer
{
	Q_OBJECT
public:
	VipOpenGLWidget(QWidget* parent = nullptr);
	~VipOpenGLWidget();

	virtual QPaintEngine* paintEngine() const;

	void setBatchRenderingEnabled(bool);
	bool isBatchRenderingEnabled() const;

	void setBackgroundColor(const QColor& c);
	QColor backgroundColor() const;

	void startRendering();
	void stopRendering();

	static bool isInPainting();

private:
	class PrivateData;
	PrivateData* d_data;
};







#endif
