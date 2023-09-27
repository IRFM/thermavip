#ifndef VIP_SEQUENTIAL_GENERATOR_H
#define VIP_SEQUENTIAL_GENERATOR_H

#include "VipIODevice.h"

#include <functional>

/// @brief A VipIODevice that continuously generates a new data based on a sampling time and a generator function.
///
/// VipSequentialGenerator generates an output data based on provided generator function.
/// This function must have the signature QVariant(const QVariant& v, qint64 time),
/// where *v* is the previous generated value (or empty QVariant for the first value),
/// and *time* is the previous value time in nanoseconds since Epoch (0 for the first value).
/// 
/// The generator function is called with a sampling time  given by the *sampling_s* property (in seconds).
/// 
class VIP_CORE_EXPORT VipSequentialGenerator : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipOutput output)	//output data
	VIP_IO(VipProperty sampling_s) //sampling time in seconds, default to 0.01 (10ms)

public:
	/// @brief Generator function type, takes the previous data and previous time (ns) as parameters
	using generator_function = std::function<QVariant(const QVariant&, qint64)>;

	VipSequentialGenerator(QObject* parent = nullptr);
	~VipSequentialGenerator();

	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual VipIODevice::DeviceType deviceType() const {return Sequential;}
	virtual bool open(VipIODevice::OpenModes);

	void setGeneratorFunction(const generator_function&);
	generator_function generatorFunction() const;

protected:
	virtual bool enableStreaming(bool);

private:
	class PrivateData;
	PrivateData* d_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipSequentialGenerator*)

#endif
