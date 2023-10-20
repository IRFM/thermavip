#ifndef VIP_TIMER_H
#define VIP_TIMER_H

#include <qthread.h>
#include "VipConfig.h"

/// A timer class similar to the QTimer one, except that it is supports concurrent access and start/stop from any thread.
class VIP_CORE_EXPORT VipTimer : public QThread
{
	Q_OBJECT

public:
	VipTimer(QObject * parent = nullptr);
	~VipTimer();

	///Returns the time interval in miliseconds
	qint64 interval() const;
	///Returns true if the timer uses single shot (default is true)
	bool singleShot() const;
	///Returns the elapsed time in miliseconds since the timer was started, or 0 if the timer is not started
	qint64 elapsed() const;
	///Returns true if the timer is currently running
	bool isRunning() const;
	///Returns true is the timer can be restared when it is already running (default is true)
	bool restartWhenRunningEnabled() const;

public Q_SLOTS:
	///Stops the timer
	void stop();
	///Starts the timer and returns true on success.
	/// If the timer is already running and restartWhenRunningEnabled is false, this function does NOT restart the timer and returns false.
	bool start();
	///Set the timer time interval
	void setInterval(qint64 inter);
	///Set the timer single shot. A single shot timer automatically stops after the first timeout.
	void setSingleShot(bool single);
	///Enable/disable the timer restart when it is already running
	void setRestartWhenRunningEnabled(bool enable);

Q_SIGNALS:
	void timeout();

protected:
	virtual void run();

private:
	class PrivateData;
	PrivateData * m_data;
};

#endif
