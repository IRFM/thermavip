/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_PY_GENERATOR_H
#define VIP_PY_GENERATOR_H


#include "VipIODevice.h"
#include "VipPyOperation.h"


/// @brief Sequential device that simulates video/plot streaming based on a python expression.
/// 
/// VipPySignalGenerator can be either sequential or temporal based on the start/end times.
/// If the property start_time or end_time is VipInvalidTime, the generator is sequential.
/// 
/// The Python code can be a single or multi line expression like 'value = np.cos(t-st)',
/// where 't' is the current time in seconds and 'st' is the start time, and 'value'
/// is the actual value to generate.
/// 
/// For sequential device, 't' and 'st' are expressed in seconds since Epoch.
/// 
class VIP_CORE_EXPORT VipPySignalGenerator : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty sampling_time)
	VIP_IO(VipProperty start_time)
	VIP_IO(VipProperty end_time)
	VIP_IO(VipProperty expression)
	VIP_IO(VipProperty unit)

	struct ReadThread : public QThread
	{
		VipPySignalGenerator * generator;
		ReadThread(VipPySignalGenerator * gen) : generator(gen) {}
		virtual void run();
	};

	QSharedPointer<ReadThread> m_thread;
	QString	m_code;
	QVariant d_data;
	qint64 m_startTime{0 };

public:
	VipPySignalGenerator(QObject * parent = nullptr)
		:VipTimeRangeBasedGenerator(parent)
	{
		propertyAt(0)->setData(20000000);
		propertyAt(1)->setData(VipInvalidTime);
		propertyAt(2)->setData(VipInvalidTime);
		propertyAt(3)->setData(QString());
		propertyAt(4)->setData(QString());
		m_thread.reset(new ReadThread(this));
	}

	~VipPySignalGenerator()
	{
		close();
	}

	virtual void close();
	virtual bool open(VipIODevice::OpenModes);
	virtual DeviceType deviceType() const;

protected:
	virtual bool readData(qint64 time);
	virtual bool enableStreaming(bool enable);
	QVariant computeValue(qint64 time, bool & ok);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPySignalGenerator*)


#endif
