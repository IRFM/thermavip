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

#include "VipPyGenerator.h"
#include "VipPyProcessing.h"

#include "VipLogging.h"
#include "VipSleep.h"

void VipPySignalGenerator::ReadThread::run()
{
	if (VipPySignalGenerator* gen = generator)
		gen->m_startTime = QDateTime::currentMSecsSinceEpoch();
	while (VipPySignalGenerator* gen = generator) {
		qint64 time = QDateTime::currentMSecsSinceEpoch();

		qint64 st = time;
		if (!gen->readData(time * 1000000))
			break;
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		int sleep = gen->propertyAt(0)->value<int>() / 1000000 - el;
		if (sleep > 0)
			vipSleep(sleep);
	}

	generator = nullptr;
}

void VipPySignalGenerator::close()
{
	VipIODevice::close();
	setStreamingEnabled(false);
	d_data = QVariant();
}

VipPySignalGenerator::DeviceType VipPySignalGenerator::deviceType() const
{
	if (d_data.userType() != 0)
		return Resource;

	qint64 start = propertyAt(1)->value<qint64>();
	qint64 end = propertyAt(2)->value<qint64>();
	if (start == VipInvalidTime || end == VipInvalidTime)
		return Sequential;
	else
		return VipTimeRangeBasedGenerator::deviceType();
}

QVariant VipPySignalGenerator::computeValue(qint64 time, bool& ok)
{
	VipPyCommandList cmds;
	cmds << vipCSendObject("t", time * 1e-9);
	if (deviceType() == Sequential)
		cmds << vipCSendObject("st", m_startTime * 1e-3);
	else
		cmds << vipCSendObject("st", propertyAt(1)->value<qint64>() * 1e-9);
	cmds << vipCExecCode(m_code, "code");
	cmds << vipCRetrieveObject("value");

	QVariant value = VipPyInterpreter::instance()->sendCommands(cmds).value(4000);
	if (value.userType() == qMetaTypeId<VipPyError>()) {
		setError(value.value<VipPyError>().traceback);
		ok = false;
		return QVariant();
	}

	ok = true;
	return value.value<QVariantMap>()["value"];
}

bool VipPySignalGenerator::open(VipIODevice::OpenModes mode)
{
	VipIODevice::close();

	if (!(mode & VipIODevice::ReadOnly))
		return false;

	qint64 sampling = propertyAt(0)->value<qint64>();
	qint64 start = propertyAt(1)->value<qint64>();
	qint64 end = propertyAt(2)->value<qint64>();
	QString code = propertyAt(3)->value<QString>();

	if (code.isEmpty())
		return false;
	if (deviceType() == Temporal && (end - start) <= 0)
		return false;
	if (sampling <= 0)
		return false;

	m_code = code;

	// temporal device, generate the timestamps
	if (deviceType() == Temporal) {
		this->setTimeWindows(start, (end - start) / sampling + 1, sampling);

		// evaluate the first value. If it is a double, generate the full curve, and reset the time window with a size of 1

		bool ok = false;
		QVariant value = computeValue(start, ok);
		if (!ok)
			return false;

		ok = false;
		value.toDouble(&ok);
		if (ok) {
			// generate the curve
			VipPointVector vector;
			for (qint64 time = start; time <= end; time += sampling) {
				bool ok = false;
				QVariant value = computeValue(time, ok);
				if (!ok)
					return false;

				vector.append(QPointF(time, value.toDouble()));
			}
			d_data = QVariant::fromValue(vector);
			if (!readData(0))
				return false;
		}
		else {
			// generate a video device
			// this->setTimeWindows(start, end, sampling);
			if (!readData(start))
				return false;
		}
	}
	else {
		m_startTime = QDateTime::currentMSecsSinceEpoch();
		if (!readData(0))
			return false;
	}

	QStringList lst = code.split("\n", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.size() == 1)
		setAttribute("Name", lst[0]);
	else
		setAttribute("Name", "Python expression");

	setOpenMode(mode);
	return true;
}

bool VipPySignalGenerator::enableStreaming(bool enable)
{
	if (deviceType() != Sequential) {
		m_thread->generator = nullptr;
		m_thread->wait();
		return false;
	}

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

bool VipPySignalGenerator::readData(qint64 time)
{
	// Resource
	if (d_data.userType() != 0) {
		VipAnyData any = create(d_data);
		any.setAttribute("Name", propertyAt(3)->value<QString>());
		any.setXUnit("Time");
		any.setYUnit(propertyAt(4)->value<QString>());
		any.setZUnit(propertyAt(4)->value<QString>());
		outputAt(0)->setData(any);
	}
	// temporal or sequential
	else {
		bool ok = false;
		QVariant value = computeValue(time, ok);
		if (value.userType() == qMetaTypeId<VipPyError>() || !ok)
			return false;

		VipAnyData any = create(value);
		any.setTime(time);
		any.setAttribute("Name", propertyAt(3)->value<QString>());
		any.setXUnit("Time");
		any.setYUnit(propertyAt(4)->value<QString>());
		any.setZUnit(propertyAt(4)->value<QString>());
		outputAt(0)->setData(any);
	}
	return true;
}
