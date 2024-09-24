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

#include "VipProgress.h"
#include "VipCore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMutex>
#include <QThread>

Q_DECLARE_METATYPE(VipProgress*)
static double _id = qRegisterMetaType<VipProgress*>();

class VipProgress::PrivateData
{
public:
	PrivateData()
	  : min(0)
	  , max(100)
	  , value(0)
	  , invRange(0)
	  , intValue(0)
	  , cancelable(false)
	  , modal(false)
	  , cancel(false)
	  , lastTime(0)
	{
	}

	double min, max, value, invRange;
	int intValue;
	QString text;
	bool cancelable;
	bool modal;
	bool cancel;

	// for processing gui events
	qint64 lastTime;
};

VipProgress::VipProgress(double min, double max, const QString& text)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	if (progressManager()) {
		if (QThread::currentThread() == QCoreApplication::instance()->thread())
			QMetaObject::invokeMethod(progressManager(), "addProgress", Qt::DirectConnection, Q_ARG(QObjectPointer, this));
		else
			QMetaObject::invokeMethod(progressManager(), "addProgress", Qt::QueuedConnection, Q_ARG(QObjectPointer, this));
	}

	setRange(min, max);
	setText(text);
}

VipProgress::~VipProgress()
{
	if (progressManager()) {
		QMetaObject::invokeMethod(progressManager(), "removeProgress", Qt::QueuedConnection, Q_ARG(QObjectPointer, this));
	}
}

void VipProgress::setRange(double min, double max)
{
	if (min != max) {
		d_data->min = min;
		d_data->max = max;
		d_data->invRange = 1.0 / (max - min);
	}
}

void VipProgress::setText(const QString& text)
{
	d_data->text = text;
	if (progressManager())
		QMetaObject::invokeMethod(progressManager(), "setText", Qt::QueuedConnection, Q_ARG(QObjectPointer, this), Q_ARG(QString, text));

	vipProcessEvents(nullptr, 50);
}

void VipProgress::setValue(double value)
{
	d_data->value = value;
	int v = qRound((value - d_data->min) * d_data->invRange * 100);
	bool new_value = false;
	if (v != d_data->intValue) {
		new_value = true;
		d_data->intValue = v;
		if (progressManager())
			QMetaObject::invokeMethod(progressManager(), "setValue", Qt::QueuedConnection, Q_ARG(QObjectPointer, this), Q_ARG(int, v));
	}

	qint64 time = QDateTime::currentMSecsSinceEpoch();

	if (time - d_data->lastTime > 200) {
		// process gui events every 200ms
		int r = vipProcessEvents(nullptr, 1);
		d_data->lastTime = QDateTime::currentMSecsSinceEpoch();
		if (r == -3 && QCoreApplication::instance()->thread() == QThread::currentThread() && new_value) // recursice call and we are in the main thread
		{
			// reset value to show the progress
			QCoreApplication::processEvents();
			// QMetaObject::invokeMethod(progressManager(), "setValue", Qt::DirectConnection, Q_ARG(QObjectPointer, this), Q_ARG(int, v));
		}
	}
}

void VipProgress::setCancelable(bool cancelable)
{
	d_data->cancelable = cancelable;
	if (progressManager())
		QMetaObject::invokeMethod(progressManager(), "setCancelable", Qt::QueuedConnection, Q_ARG(QObjectPointer, this), Q_ARG(bool, cancelable));
}

void VipProgress::setModal(bool modal)
{
	d_data->modal = modal;
	if (progressManager())
		QMetaObject::invokeMethod(progressManager(), "setModal", Qt::QueuedConnection, Q_ARG(QObjectPointer, this), Q_ARG(bool, modal));
}

void VipProgress::cancelRequested()
{
	d_data->cancel = true;
}

bool VipProgress::canceled() const
{
	return d_data->cancel;
}

double VipProgress::min() const
{
	return d_data->min;
}

double VipProgress::max() const
{
	return d_data->max;
}

QString VipProgress::text() const
{
	return d_data->text;
}

double VipProgress::value() const
{
	return d_data->value;
}

bool VipProgress::isCancelable() const
{
	return d_data->cancelable;
}

bool VipProgress::isModal() const
{
	return d_data->modal;
}

static QObject* defaultManager()
{
	static DefaultProgressManager manager;
	return &manager;
}

static QObject*& currentManager()
{
	static QObject* current = defaultManager();
	return current;
}

void VipProgress::setProgressManager(QObject* manager)
{
	currentManager() = manager;
}

void VipProgress::resetProgressManager()
{
	currentManager() = defaultManager();
}

QObject* VipProgress::progressManager()
{
	return currentManager();
}
