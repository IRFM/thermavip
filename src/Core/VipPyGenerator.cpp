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

#include "VipProgress.h"
#include "VipPyGenerator.h"
#include "VipPyProcessing.h"

#include "VipLogging.h"
#include "VipSleep.h"

void VipPySignalGenerator::ReadThread::run()
{
	if (VipPySignalGenerator* gen = generator.load(std::memory_order_acquire))
		gen->m_startTime = QDateTime::currentMSecsSinceEpoch();
	while (VipPySignalGenerator* gen = generator.load(std::memory_order_acquire)) {
		qint64 time = QDateTime::currentMSecsSinceEpoch();

		qint64 st = time;
		if (!gen->readData(time * 1000000))
			break;
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		// The same property is read as qint64 when the device opens. Read as an int,
		// a period above 2.1 seconds overflows and the sleep is skipped, so the loop
		// spins with no pause at all.
		const qint64 sleep = gen->propertyAt(0)->value<qint64>() / 1000000 - el;
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

	// Three outcomes, not one. An interpreter that is absent, closed, or that
	// failed to start gives a null future, whose value is an empty variant: that
	// is not an error object, so it used to pass for a success and the device
	// opened and published nothing.
	const VipPyFuture future = VipPyInterpreter::instance()->sendCommands(cmds);
	if (future.isNull()) {
		setError("Python interpreter is not available", VipProcessingObject::WrongInput);
		ok = false;
		return QVariant();
	}

	const QVariant value = future.value(4000);
	if (value.userType() == qMetaTypeId<VipPyError>()) {
		setError(value.value<VipPyError>().traceback);
		ok = false;
		return QVariant();
	}

	// value() and contains(), not operator[]: the non const one inserts a default
	// built entry when the key is missing, and reported it as a result.
	const QVariantMap result = value.value<QVariantMap>();
	if (!result.contains("value")) {
		setError("the Python code did not define the 'value' variable", VipProcessingObject::WrongInput);
		ok = false;
		return QVariant();
	}

	ok = true;
	return result.value("value");
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
	// The code below is a property, and properties come back from session files.
	// One session is opened at every start without asking, so running it would
	// mean running whatever that file chose.
	if (!vipCanRunRestoredPythonCode(this)) {
		setError("Python code restored from a session file was not run");
		return false;
	}
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
			// One synchronous round trip to the interpreter per sample, on the calling
			// thread, which is the GUI one. The count is (end - start) / sampling and
			// nothing bounded it: an hour at the default sampling is 180000 round
			// trips with the interface frozen throughout, and the three properties
			// come back from session files, where they can ask for far more than that.
			static constexpr qint64 maxGeneratedPoints = 10 * 1000 * 1000;
			const qint64 count = (end - start) / sampling + 1;
			if (count > maxGeneratedPoints) {
				setError("Too many points to generate: " + QString::number(count));
				return false;
			}

			VipProgress progress;
			progress.setRange(0, (double)count);
			progress.setText("Generating curve...");
			progress.setCancelable(true);

			VipPointVector vector;
			vector.reserve(count);
			qint64 index = 0;
			for (qint64 time = start; time <= end; time += sampling, ++index) {
				bool ok = false;
				QVariant value = computeValue(time, ok);
				if (!ok)
					return false;

				// The first sample's conversion is checked above, and it decides the
				// strategy; the ones after it were not. QVariant::toDouble returns 0.0
				// on failure, so an expression that changes nature partway through the
				// range filled the curve with zeros no one could tell from measured
				// ones.
				bool converted = false;
				const double y = value.toDouble(&converted);
				if (!converted) {
					setError("Expression did not produce a number at time " + QString::number(time));
					return false;
				}
				vector.append(QPointF(time, y));

				if ((index & 0xff) == 0) {
					progress.setValue((double)index);
					if (progress.canceled()) {
						setError("Curve generation cancelled");
						return false;
					}
				}
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
