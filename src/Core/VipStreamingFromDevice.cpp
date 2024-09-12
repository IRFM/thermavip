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

#include "VipStreamingFromDevice.h"
#include "VipSleep.h"

namespace detail
{

	struct ReadThread : public QThread
	{
		VipGeneratorSequential* generator;
		ReadThread(VipGeneratorSequential* gen)
		  : generator(gen)
		{
		}
		virtual void run()
		{
			// qint64 very_start_absolute = vipGetMilliSecondsSinceEpoch() * 1000000;
			qint64 start_absolute = vipGetMilliSecondsSinceEpoch() * 1000000;
			qint64 start_device = generator->m_device->firstTime();
			qint64 prev_read = VipInvalidTime;

			while (VipGeneratorSequential* gen = generator) {
				qint64 time = vipGetMilliSecondsSinceEpoch() * 1000000;
				qint64 current = (time - start_absolute);
				qint64 closest = generator->m_device->closestTime(current + start_device);

				if (closest == prev_read) {
					// we already read that frame
					if (closest == gen->m_device->lastTime()) {
						// restart
						start_absolute = time;
						prev_read = VipInvalidTime;
						closest = generator->m_device->firstTime();
					}
					else {
						vipSleep(2);
						continue;
					}
				}
				prev_read = closest;

				gen->readDeviceTime(closest, time);
			}

			generator = nullptr;
		}
	};

}

VipGeneratorSequential::VipGeneratorSequential(QObject* parent)
  : VipIODevice(parent)
{
	m_thread.reset(new detail::ReadThread(this));
}

VipGeneratorSequential ::~VipGeneratorSequential()
{
	close();
}

void VipGeneratorSequential::close()
{
	VipIODevice::close();
	setStreamingEnabled(false);
	m_device.reset();
}

void VipGeneratorSequential::setIODevice(VipIODevice* device)
{
	if (!isOpen())
		m_device = QSharedPointer<VipIODevice>(device);
}

VipIODevice* VipGeneratorSequential::IODevice() const
{
	return m_device.data();
}

bool VipGeneratorSequential::open(VipIODevice::OpenModes mode)
{
	VipIODevice::close();

	if (!(mode & VipIODevice::ReadOnly))
		return false;

	if (!m_device)
		return false;
	if (!m_device->isOpen())
		if (!m_device->open(mode))
			return false;

	if (m_device && (m_device->openMode() & VipIODevice::ReadOnly)) {
		setOpenMode(mode);
		// read first data
		readDeviceTime(m_device->firstTime(), vipGetNanoSecondsSinceEpoch());
		return true;
	}
	return false;
}

QTransform VipGeneratorSequential::imageTransform() const
{
	if (m_device)
		return m_device->imageTransform();
	return VipIODevice::imageTransform();
}

void VipGeneratorSequential::readDeviceTime(qint64 time, qint64 new_time)
{
	if (m_device->read(time)) {
		VipAnyData any = this->m_device->outputAt(0)->data();
		any.setTime(new_time);
		any.mergeAttributes(this->attributes());
		any.setSource((qint64)this);
		this->outputAt(0)->setData(any);
	}
}

bool VipGeneratorSequential::enableStreaming(bool enable)
{

	if (enable) {
		m_thread->generator = this;
		m_thread->start();
	}
	else {
		m_thread->generator = nullptr;
		m_thread->wait();
	}

	return true;
}

static VipArchive& operator<<(VipArchive& arch, const VipGeneratorSequential* gen)
{
	if (gen)
		arch.content("device", gen->IODevice());
	return arch;
}
static VipArchive& operator>>(VipArchive& arch, VipGeneratorSequential* gen)
{
	if (VipIODevice* dev = arch.read("device").value<VipIODevice*>())
		gen->setIODevice(dev);
	else
		arch.resetError();
	return arch;
}
static int _register = vipAddInitializationFunction(vipRegisterArchiveStreamOperators<VipGeneratorSequential*>);
