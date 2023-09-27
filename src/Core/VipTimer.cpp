#include "VipTimer.h"
#include "VipSleep.h"

#include <qmutex.h>
#include <qdatetime.h>
#include <qwaitcondition.h>

class VipTimer::PrivateData
{
public:
	PrivateData() : start(0), interval(0), singleshot(true), stop(false), enable_restart_when_running(true){}
	qint64 start;
	qint64 interval;
	bool singleshot;
	bool stop;
	bool enable_restart_when_running;
	QMutex mutex;
	QWaitCondition cond;
};


VipTimer::VipTimer(QObject * parent )
	:QThread(parent)
{
	m_data = new PrivateData();
	this->QThread::start();
}

VipTimer::~VipTimer()
{
	m_data->stop = true;
	stop();
	wait();
	delete m_data;
}

qint64 VipTimer::interval() const
{
	return m_data->interval;
}

bool VipTimer::singleShot() const
{
	return m_data->singleshot;
}

qint64 VipTimer::elapsed() const
{
	if (m_data->start)
		return QDateTime::currentMSecsSinceEpoch() - m_data->start;
	else
		return 0;
}

bool VipTimer::isRunning() const
{
	return m_data->start != 0;
}

bool VipTimer::restartWhenRunningEnabled() const
{
	return m_data->enable_restart_when_running;
}

void VipTimer::stop()
{
	QMutexLocker lock(&m_data->mutex);
	m_data->start = 0;
	m_data->cond.wakeAll();
}

bool VipTimer::start()
{
	if (m_data->start != 0 && !m_data->enable_restart_when_running)
		return false;

	QMutexLocker lock(&m_data->mutex);
	m_data->start = QDateTime::currentMSecsSinceEpoch();
	m_data->cond.wakeAll();
	return true;
}

void VipTimer::setInterval(qint64 inter)
{
	QMutexLocker lock(&m_data->mutex);
	m_data->interval = inter;
}

void VipTimer::setSingleShot(bool single)
{
	QMutexLocker lock(&m_data->mutex);
	m_data->singleshot = single;
}

void VipTimer::setRestartWhenRunningEnabled(bool enable)
{
	QMutexLocker lock(&m_data->mutex);
	m_data->enable_restart_when_running = enable;
}

void VipTimer::run()
{
	while (!m_data->stop)
	{
		while(!m_data->start && !m_data->stop)
		{
			QMutexLocker lock(&m_data->mutex);
			m_data->cond.wait(&m_data->mutex,20);
		}
		qint64 time = QDateTime::currentMSecsSinceEpoch();
		qint64 elapsed = time - m_data->start;
		qint64 remaining = m_data->interval - elapsed;
		if (m_data->start && remaining <= 0 )
		{
			Q_EMIT timeout();
			QMutexLocker lock(&m_data->mutex);
			if (!m_data->singleshot)
				m_data->start = time;
			else
				m_data->start = 0;
		}
		else //wait by chunks of at most 10ms
			vipSleep(remaining -1 > 0 ? qMin(remaining -1,(qint64)10) : 1);
	}
}

