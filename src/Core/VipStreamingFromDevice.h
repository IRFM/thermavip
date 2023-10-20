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
	void setIODevice(VipIODevice* device);
	VipIODevice* IODevice() const;

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
