#ifndef MPEG_LOADER_H
#define MPEG_LOADER_H

#include "VipIODevice.h"
#include "FfmpegConfig.h"
 
class VideoDecoder;

class FFMPEG_EXPORT MPEGLoader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)

	Q_CLASSINFO("image", "Output image read from the video file")
	Q_CLASSINFO("description","Read a sequence of image from a mpeg video file. Internally uses ffmpeg.")
public:


	MPEGLoader(QObject * parent = NULL);
	virtual ~MPEGLoader();

	qint32 fullFrameWidth() const;
	qint32 fullFrameHeight() const;
	
	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

	virtual DeviceType deviceType() const;

	//helper function
	bool open(const QString & name, const QString & format, const QMap<QString, QString> & options = QMap<QString, QString>());

	virtual QString fileFilters() const { return "Video file (*.mpg *.mpeg *.avi *.mp4 *.wmv *.gif *.mov *.mkv *.IR *.sdp)"; }

	static QStringList listDevices();
protected:
	virtual bool readData(qint64 time);
	virtual bool enableStreaming(bool enable);

protected:

	struct ReadThread : public QThread
	{
		bool stop;
		MPEGLoader * loader;
		ReadThread(MPEGLoader * l):loader(l) {}
		virtual void run()
		{
			stop = false;
			while (!stop)
			{
				loader->readCurrentData();
				QThread::msleep(10);
			}
		}
	};

	VipNDArray fromImage(const QImage & img) const;

	ReadThread		m_thread;
	VideoDecoder	*m_decoder;
	uint64_t		m_last_dts;
	double			m_sampling_time;
	int				m_count;
	QString			m_device_path;

};

VIP_REGISTER_QOBJECT_METATYPE(MPEGLoader*)



#include "MPEGSaver.h"

class FFMPEG_EXPORT IR_H264_Loader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)

		Q_CLASSINFO("image", "Output image read from the IR video file")
		Q_CLASSINFO("description", "Read a sequence of image from a IR video file. Internally uses ffmpeg.")
public:


	IR_H264_Loader(QObject * parent = NULL);
	virtual ~IR_H264_Loader();

	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

	virtual DeviceType deviceType() const {
		return Temporal;
	}

	virtual QString fileFilters() const { return "Video file (*." CODEC_FORMAT ")"; }

protected:
	virtual bool readData(qint64 time);

private:
	
	class PrivateData;
	PrivateData * m_data;
};
//VIP_REGISTER_QOBJECT_METATYPE(IR_H264_Loader*)


#endif
