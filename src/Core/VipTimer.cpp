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

#include "VipTimer.h"
#include "VipSleep.h"
 
#include <qdatetime.h>
#include <qmutex.h>
#include <qwaitcondition.h>

class VipTimer::PrivateData
{
public:
	PrivateData()
	  : start(0)
	  , interval(0)
	  , singleshot(true)
	  , stop(false)
	  , enable_restart_when_running(true)
	{
	}
	qint64 start;
	qint64 interval;
	bool singleshot;
	bool stop;
	bool enable_restart_when_running;
	QMutex mutex;
	QWaitCondition cond;
};

VipTimer::VipTimer(QObject* parent)
  : QThread(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	this->QThread::start();
}

VipTimer::~VipTimer()
{
	d_data->stop = true;
	stop();
	wait();
}

qint64 VipTimer::interval() const
{
	return d_data->interval;
}

bool VipTimer::singleShot() const
{
	return d_data->singleshot;
}

qint64 VipTimer::elapsed() const
{
	if (d_data->start)
		return QDateTime::currentMSecsSinceEpoch() - d_data->start;
	else
		return 0;
}

bool VipTimer::isRunning() const
{
	return d_data->start != 0;
}

bool VipTimer::restartWhenRunningEnabled() const
{
	return d_data->enable_restart_when_running;
}

void VipTimer::stop()
{
	QMutexLocker lock(&d_data->mutex);
	d_data->start = 0;
	d_data->cond.wakeAll();
}

bool VipTimer::start()
{
	if (d_data->start != 0 && !d_data->enable_restart_when_running)
		return false;

	QMutexLocker lock(&d_data->mutex);
	d_data->start = QDateTime::currentMSecsSinceEpoch();
	d_data->cond.wakeAll();
	return true;
}

void VipTimer::setInterval(qint64 inter)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->interval = inter;
}

void VipTimer::setSingleShot(bool single)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->singleshot = single;
}

void VipTimer::setRestartWhenRunningEnabled(bool enable)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->enable_restart_when_running = enable;
}

void VipTimer::run()
{
	while (!d_data->stop) {
		while (!d_data->start && !d_data->stop) {
			QMutexLocker lock(&d_data->mutex);
			d_data->cond.wait(&d_data->mutex, 20);
		}
		qint64 time = QDateTime::currentMSecsSinceEpoch();
		qint64 elapsed = time - d_data->start;
		qint64 remaining = d_data->interval - elapsed;
		if (d_data->start && remaining <= 0) {
			Q_EMIT timeout();
			QMutexLocker lock(&d_data->mutex);
			if (!d_data->singleshot)
				d_data->start = time;
			else
				d_data->start = 0;
		}
		else // wait by chunks of at most 10ms
			vipSleep(remaining - 1 > 0 ? qMin(remaining - 1, (qint64)10) : 1);
	}
}
