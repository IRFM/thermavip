#ifndef VIP_MPEG_LOADER_H
#define VIP_MPEG_LOADER_H

#include "VipIODevice.h"
#include "VipSleep.h"

#include "VipFfmpegConfig.h"

class VideoDecoder;

/// @brief VipIODevice class able to read most MPEG video formats using ffmpeg library
///
/// VipMPEGLoader is either a Temporal or Sequential VipIODevice based on provided path.
/// If the path refers to a local file, VipMPEGLoader will be Temporal.
/// If the path refers to a network stream, VipMPEGLoader will be Sequential.
///
class FFMPEG_EXPORT VipMPEGLoader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)

	Q_CLASSINFO("image", "Output image read from the video file")
	Q_CLASSINFO("description", "Read a sequence of image from a mpeg video file. Internally uses ffmpeg.")
public:
	using draw_function = std::function<void(QImage&)>;

	VipMPEGLoader(QObject* parent = nullptr);
	virtual ~VipMPEGLoader();

	qint32 fullFrameWidth() const;
	qint32 fullFrameHeight() const;

	void setDrawFunction(const draw_function&);
	const draw_function& drawFunction() const;

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

	virtual DeviceType deviceType() const;

	/// @brief Helper function, open path with optional format and options.
	/// This can be used (for instance) to open the webcam (on Windows): open("video=" + action->text(), "dshow");
	bool open(const QString& name, const QString& format, const QMap<QString, QString>& options = QMap<QString, QString>());

	virtual QString fileFilters() const { return "Video file (*.mpg *.mpeg *.avi *.mp4 *.wmv *.gif *.mov *.mkv *.IR *.sdp)"; }

	static QStringList listDevices();

protected:
	virtual bool readData(qint64 time);
	virtual bool enableStreaming(bool enable);

protected:
	struct ReadThread : public QThread
	{
		bool stop;
		VipMPEGLoader* loader;
		ReadThread(VipMPEGLoader* l)
		  : loader(l)
		{
		}
		virtual void run()
		{
			stop = false;
			while (!stop) {
				loader->readCurrentData();
				vipSleep(10);
			}
		}
	};

	VipNDArray fromImage(const QImage& img) const;

	ReadThread m_thread;
	VideoDecoder* m_decoder;
	uint64_t m_last_dts;
	double m_sampling_time;
	int m_count;
	QString m_device_path;
	draw_function m_draw_function;
};

VIP_REGISTER_QOBJECT_METATYPE(VipMPEGLoader*)

#endif
