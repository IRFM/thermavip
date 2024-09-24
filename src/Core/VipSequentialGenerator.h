/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
	VIP_IO(VipOutput output)       // output data
	VIP_IO(VipProperty sampling_s) // sampling time in seconds, default to 0.01 (10ms)

public:
	/// @brief Generator function type, takes the previous data and previous time (ns) as parameters
	using generator_function = std::function<VipAnyData(const VipAnyData&)>;

	VipSequentialGenerator(QObject* parent = nullptr);
	VipSequentialGenerator(const generator_function& fun, double sampling, QObject* parent = nullptr);
	~VipSequentialGenerator();

	void setGeneratorFunction(const generator_function& fun);
	generator_function generatorFunction() const;

	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Sequential; }
	virtual bool open(VipIODevice::OpenModes);

protected:
	virtual bool enableStreaming(bool);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipSequentialGenerator*)

#endif
