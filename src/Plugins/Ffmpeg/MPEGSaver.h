#ifndef MPEG_SAVER_H
#define MPEG_SAVER_H

#include "VipIODevice.h"
#include "FfmpegConfig.h"


//Additional info for MPEGSaver
struct FFMPEG_EXPORT MPEGIODeviceHandler
{
	int width;
	int height;
	double fps;
	double rate; //bits/s
	int codec_id;

	MPEGIODeviceHandler(int w=0, int h=0,double f = 25, double r = 20000000, int c = -1) //20000Kb/s
	:width(w), height(h), fps(f),rate(r),codec_id(c)
	{}
};

class VideoEncoder;

class FFMPEG_EXPORT MPEGSaver : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput image)
	
	Q_CLASSINFO("image","Input image to save")
	Q_CLASSINFO("description","Save a sequence of image as a mpeg video file. Internally uses ffmpeg.")
public:

	MPEGSaver(QObject * parent = NULL);
	virtual ~MPEGSaver();


	qint32 fullFrameWidth() const;
	qint32 fullFrameHeight() const;

	// set the info directly as a MPEGIODeviceHandler object
	void setAdditionalInfo(const MPEGIODeviceHandler & info);
	MPEGIODeviceHandler additionalInfo() const;

	VideoEncoder * encoder() {return m_encoder;}

	virtual qint64 estimateFileSize() const;

	virtual bool acceptInput(int, const QVariant & v) const {
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
			VipNDArray ar = v.value<VipNDArray>();
			return vipIsImageArray(ar);
		}
		return false;
	}

	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Video file (*.mp4 *.mpg *.mpeg *.avi *.wmv *.gif *.mov)"; }
	virtual void close();

protected:
	virtual void apply();


	MPEGIODeviceHandler	m_info;
	VideoEncoder		*m_encoder;
};


VIP_REGISTER_QOBJECT_METATYPE(MPEGSaver*)




#define CODEC_FORMAT "h264"


class FFMPEG_EXPORT IR_H264_Saver : public VipIODevice
{
	Q_OBJECT
		VIP_IO(VipInput image)
		Q_CLASSINFO("image", "Input image to save")
		Q_CLASSINFO("description", "Save a sequence of 16bpp images. Internally uses ffmpeg.")
public:

	IR_H264_Saver(QObject * parent = NULL);
	virtual ~IR_H264_Saver();

	/**
	0 means lossless.
	Max is 51.
	*/
	void setLossyLevel(int level);
	/**
	From 0 to 8, map to h264 preset:
	ultrafast
	superfast
	veryfast
	faster
	fast
	medium
	slow
	slower
	veryslow
	*/
	void setCompressionLevel(int clevel);
	int lossyLevel() const;
	int compressionLevel() const;

	virtual bool acceptInput(int, const QVariant & v) const {
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
			VipNDArray ar = v.value<VipNDArray>();
			return !ar.isEmpty() && ar.shapeCount() == 2 && ar.dataSize() == 2;
		}
		return false;
	}

	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Video file (*." CODEC_FORMAT ")"; }
	virtual void close();

protected:
	virtual void apply();

	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(IR_H264_Saver*)


#include <qwidget.h>

class FFMPEG_EXPORT IR_H264_Saver_Panel : public QWidget
{
	Q_OBJECT 
public:
	IR_H264_Saver_Panel();
	~IR_H264_Saver_Panel();

	void setSaver(IR_H264_Saver *);
	IR_H264_Saver* saver() const;

private Q_SLOTS:
	void updateSaver() const;
private:
	class PrivateData; 
	PrivateData * m_data;
};
VIP_REGISTER_QOBJECT_METATYPE(IR_H264_Saver_Panel*)



FFMPEG_EXPORT bool remuxH264Bitstream(const char* input, const char* output, int fps);

#endif
