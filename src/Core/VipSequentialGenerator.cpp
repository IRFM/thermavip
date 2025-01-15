/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipSequentialGenerator.h"
#include "VipSleep.h"
#include <qthread.h>

class VipSequentialGenerator::PrivateData : public QThread
{
public:
	bool stop{ true };
	VipSequentialGenerator* parent{ nullptr };
	VipAnyData prev;
	qint64 prev_time{ 0 };
	VipSequentialGenerator::generator_function fun;

	PrivateData(VipSequentialGenerator* p)
	  : parent(p)
	{
	}

	virtual void run()
	{
		while (!stop) {
			// compute current time
			qint64 time = QDateTime::currentMSecsSinceEpoch();
			// compute new value
			prev = fun(prev);
			prev.setTime(time * 1000000ll);
			prev.setSource(parent);
			prev.mergeAttributes(parent->attributes());

			// get sampling time
			qint64 sampling_ms = static_cast<qint64>(parent->propertyAt(0)->value<double>() * 1000.);
			// send output value
			parent->outputAt(0)->setData(prev);

			// elapsed time
			qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - time;

			// wait before next iteration
			if (elapsed < sampling_ms) {
				vipSleep(sampling_ms - elapsed);
			}
		}
	}
};

VipSequentialGenerator::VipSequentialGenerator(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data,this);
	propertyAt(0)->setData(0.01);
}

VipSequentialGenerator::VipSequentialGenerator(const generator_function& fun, double sampling, QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data,this);
	propertyAt(0)->setData(sampling);
	setGeneratorFunction(fun);
}

VipSequentialGenerator::~VipSequentialGenerator()
{
	d_data->stop = true;
	d_data->wait();
}

bool VipSequentialGenerator::open(VipIODevice::OpenModes modes)
{
	if (!(modes & ReadOnly))
		return false;

	setOpenMode(modes);
	return true;
}

void VipSequentialGenerator::setGeneratorFunction(const generator_function& fun)
{
	setStreamingEnabled(false);
	d_data->fun = fun;
	if (fun)
		setOpenMode(VipIODevice::WriteOnly);
}

VipSequentialGenerator::generator_function VipSequentialGenerator::generatorFunction() const
{
	return d_data->fun;
}

bool VipSequentialGenerator::enableStreaming(bool enable)
{
	if (!d_data->fun)
		return false;

	// stop first
	d_data->stop = true;
	d_data->wait();

	if (enable) {
		d_data->stop = false;
		d_data->start();
	}
	return true;
}