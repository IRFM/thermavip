#ifndef VIP_RIR_DEVICE_H
#define VIP_RIR_DEVICE_H


#include "VipIODevice.h"
#include "VipPlayer.h"




/// @brief IO device able to read video file format supported by the librir.
/// This includes:
/// -	HCC infrared video files
/// -	MP4 infrared video files compressed with h264 or hevc codecs
/// -	PCR raw video files
/// 
class VIP_ANNOTATION_EXPORT VipRIRDevice : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)
	VIP_IO(VipProperty Calibration) // selected calibration as an integer
	VIP_IO(VipProperty BadPixels) // selected calibration as an integer

public:
	VipRIRDevice(QObject* parent = nullptr);
	~VipRIRDevice();

	virtual bool open(VipIODevice::OpenModes mode);
	virtual QString fileFilters() const { return "Librir infrared video files (*.pcr *.bin *.h264 *.h265 *.hcc)"; }
	virtual void close();
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual int camera() const { return m_file; }
	virtual QStringList calibrations() const { return m_calibrations; }
	virtual QSize imageSize() const { return m_size; }
	virtual int getRawValue(int x, int y);

	VipNDArray lastRawImage() const { return m_raw; }

	virtual QString fileName() const;

	
protected:
	virtual bool readData(qint64 time);

private:
	int m_file;
	int m_count;
	QSize m_size;
	VipNDArray m_raw;
	QStringList m_calibrations;
};

VIP_REGISTER_QOBJECT_METATYPE(VipRIRDevice*)



/**
Widget to edit a VipRIRDevice instance (either WEST_IR_Device or WEST_BIN_PCR_Device)
*/
class VipRIRDeviceEditor : public QObject
{
	Q_OBJECT

public:
	VipRIRDeviceEditor(VipVideoPlayer* player);
	~VipRIRDeviceEditor();

	void setDevice(VipRIRDevice* dev);
	VipRIRDevice* device() const;

public Q_SLOTS:
	void updateDevice();
	void setBadPixels(bool);

Q_SIGNALS:
	void deviceUpdated();

private:
	class PrivateData;
	PrivateData* m_data;
};



/**
Customize a VipVideoPlayer for VipRIRDevice instance
*/
class CustomizeRIRVideoPlayer : public QObject
{
	Q_OBJECT
	QPointer<VipRIRDevice> m_device;
	QPointer<VipVideoPlayer> m_player;
	VipRIRDeviceEditor* m_options;

public:
	CustomizeRIRVideoPlayer(VipVideoPlayer* player, VipRIRDevice* device);
};

#endif