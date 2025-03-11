#ifndef PY_NPZ_DEVICE
#define PY_NPZ_DEVICE

#include "PyOperation.h"
#include "VipIODevice.h"

/// @brief Save 2D array objects in NPZ file format.
///
/// PyNPZDevice will vertically stack all 2D arrays given as input,
/// and save the 3D array in the close() member.
/// 
/// The array name in the NPZ file will  be 'arr_...'
/// where '...' is deduced from input names.
/// 
/// 
class PyNPZDevice : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)

public:
	PyNPZDevice(QObject* parent = nullptr);
	~PyNPZDevice();

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int, const QVariant& v) const
	{
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
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

VIP_REGISTER_QOBJECT_METATYPE(PyNPZDevice*)


/// @brief Save 2D array objects in Matlab format.
///
/// MATDevice behaves in the same way as PyNPZDevice,
/// but save the resulting 3D array in a Matlab file.
///
class MATDevice : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)

public:
	MATDevice(QObject* parent = nullptr);
	~MATDevice();

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int, const QVariant& v) const
	{
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
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

#endif
