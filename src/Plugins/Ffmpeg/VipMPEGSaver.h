#ifndef MPEG_SAVER_H
#define MPEG_SAVER_H

#include "VipIODevice.h"
#include "VipFfmpegConfig.h"

/// @brief Additional parameters for VipMPEGSaver
struct FFMPEG_EXPORT VipMPEGIODeviceHandler
{
	int width{ 0 };
	int height{ 0 };
	int fps{ 25 };
	double rate{ 20000000. }; // bits/s
	int codec_id{ -1 };
	int threads{ 1 };
};

class VideoEncoder;

/// @brief A VipIODevice used to record a movie in any format supported by ffmpeg.
/// Input images must have ARGB format (see vipIsImageArray() and vipToImage() functions).
///
/// Recording parameters are passed using VipMPEGIODeviceHandler structure.
///
class FFMPEG_EXPORT VipMPEGSaver : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput image)

	Q_CLASSINFO("image", "Input image to save")
	Q_CLASSINFO("description", "Save a sequence of image as a mpeg video file. Internally uses ffmpeg.")
public:
	VipMPEGSaver(QObject* parent = nullptr);
	virtual ~VipMPEGSaver();

	qint32 fullFrameWidth() const;
	qint32 fullFrameHeight() const;

	// set the info directly as a VipMPEGIODeviceHandler object
	void setAdditionalInfo(const VipMPEGIODeviceHandler& info);
	VipMPEGIODeviceHandler additionalInfo() const;

	VideoEncoder* encoder() { return m_encoder; }

	virtual qint64 estimateFileSize() const;

	virtual bool acceptInput(int, const QVariant& v) const
	{
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
			const VipNDArray ar = v.value<VipNDArray>();
			return vipIsImageArray(ar);
		}
		return false;
	}

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Video file (*.mp4 *.mpg *.mpeg *.avi *.wmv *.gif *.mov)"; }
	virtual void close();

protected:
	virtual void apply();

	VipMPEGIODeviceHandler m_info;
	VideoEncoder* m_encoder;
};

VIP_REGISTER_QOBJECT_METATYPE(VipMPEGSaver*)

#endif
