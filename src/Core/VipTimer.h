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

#ifndef VIP_TIMER_H
#define VIP_TIMER_H

#include "VipConfig.h"
#include "VipPimpl.h"

#include <QThread>

/// A timer class similar to the QTimer one, except that it is supports concurrent access and start/stop from any thread.
class VIP_CORE_EXPORT VipTimer : public QThread
{
	Q_OBJECT

public:
	VipTimer(QObject* parent = nullptr);
	~VipTimer();

	/// Returns the time interval in miliseconds
	qint64 interval() const;
	/// Returns true if the timer uses single shot (default is true)
	bool singleShot() const;
	/// Returns the elapsed time in miliseconds since the timer was started, or 0 if the timer is not started
	qint64 elapsed() const;
	/// Returns true if the timer is currently running
	bool isRunning() const;
	/// Returns true is the timer can be restared when it is already running (default is true)
	bool restartWhenRunningEnabled() const;

public Q_SLOTS:
	/// Stops the timer
	void stop();
	/// Starts the timer and returns true on success.
	///  If the timer is already running and restartWhenRunningEnabled is false, this function does NOT restart the timer and returns false.
	bool start();
	/// Set the timer time interval
	void setInterval(qint64 inter);
	/// Set the timer single shot. A single shot timer automatically stops after the first timeout.
	void setSingleShot(bool single);
	/// Enable/disable the timer restart when it is already running
	void setRestartWhenRunningEnabled(bool enable);

Q_SIGNALS:
	void timeout();

protected:
	virtual void run();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

#endif
