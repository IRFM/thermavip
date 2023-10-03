#ifndef MPEG_SAVER_H
#define MPEG_SAVER_H

#include "VipIODevice.h"
#include "FfmpegConfig.h"


/// @brief Additional parameters for MPEGSaver
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

/// @brief A VipIODevice used to record a movie in any format supported by ffmpeg.
/// Input images must have ARGB format (see vipIsImageArray() and vipToImage() functions).
/// 
/// Recording parameters are passed using MPEGIODeviceHandler structure.
/// 
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
			const VipNDArray ar = v.value<VipNDArray>();
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



#endif
