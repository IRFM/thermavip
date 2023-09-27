#ifndef VIP_STREAMING_FROM_DEVICE_H
#define VIP_STREAMING_FROM_DEVICE_H


#include "VipIODevice.h"

namespace detail
{
	struct ReadThread;
}

/// @brief Sequential device that simulates streaming based on a temporal VipIODevice that is played repeatedly.
class VIP_CORE_EXPORT VipGeneratorSequential : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipOutput output)

	friend struct detail::ReadThread;
	QSharedPointer<VipIODevice> m_device;
	QSharedPointer<detail::ReadThread> m_thread;

public:
	VipGeneratorSequential(QObject* parent = nullptr);
	~VipGeneratorSequential();

	/// @brief Set the device that will be played repeatedly. 
	/// This must be called before VipIODevice::open()
	void setIODevice(VipIODevice * device);
	VipIODevice * IODevice() const;

	virtual void close();
	virtual bool open(VipIODevice::OpenModes);
	virtual DeviceType deviceType() const { return Sequential; }
	virtual OpenModes supportedModes() const { return VipIODevice::ReadOnly; }

protected:
	void readDeviceTime(qint64 time, qint64 new_time);
	virtual bool enableStreaming(bool enable);
};

VIP_REGISTER_QOBJECT_METATYPE(VipGeneratorSequential*)

#endif
