#pragma once

#include "PyOperation.h"
#include "VipIODevice.h"


class NPZDevice : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)

public:
	NPZDevice(QObject* parent = nullptr);
	~NPZDevice();

	virtual bool probe(const QString& filename, const QByteArray&) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool acceptInput(int, const QVariant& v) const {
		if(v.userType() == qMetaTypeId<VipNDArray>())
		{
			const VipNDArray ar = v.value<VipNDArray>();
			if (vipIsImageArray(ar))
				return false;
			return true;
		}
		return v.canConvert<VipNDArray>();
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Python files (*.npz)"; }
	virtual void close();

protected:
	virtual void apply();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(NPZDevice*)





class MATDevice : public VipIODevice
{
	Q_OBJECT
		VIP_IO(VipInput input)

public:
	MATDevice(QObject* parent = nullptr);
	~MATDevice();

	virtual bool probe(const QString& filename, const QByteArray&) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool acceptInput(int, const QVariant& v) const {
		if (v.userType() == qMetaTypeId<VipNDArray>())
		{
			const VipNDArray ar = v.value<VipNDArray>();
			if (vipIsImageArray(ar))
				return false;
			return true;
		}
		return v.canConvert<VipNDArray>();
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Matlab files (*.mat)"; }
	virtual void close();

protected:
	virtual void apply();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(MATDevice*)