#include "VipProgress.h"
#include "VipCore.h"

#include <QMutex>
#include <QThread>
#include <QCoreApplication>
#include <QDateTime>



 Q_DECLARE_METATYPE(VipProgress*)
 static double _id = qRegisterMetaType<VipProgress*>();

class VipProgress::PrivateData
{
public:
	PrivateData():min(0),max(100),value(0),invRange(0),intValue(0),cancelable(false),modal(false),cancel(false), lastTime(0) {}

	double min,max,value,invRange;
	int intValue;
	QString text;
	bool cancelable;
	bool modal;
	bool cancel;

	//for processing gui events
	qint64 lastTime;
};


VipProgress::VipProgress(double min, double max, const QString & text)
{
	m_data = new PrivateData();

	if(progressManager())
	{
		if(QThread::currentThread() == QCoreApplication::instance()->thread())
			QMetaObject::invokeMethod(progressManager(), "addProgress", Qt::DirectConnection, Q_ARG(QObjectPointer, this));
		else
			QMetaObject::invokeMethod(progressManager(),"addProgress", Qt::QueuedConnection,Q_ARG(QObjectPointer,this));
	}

	setRange(min,max);
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
	if(min != max)
	{
		m_data->min = min;
		m_data->max = max;
		m_data->invRange = 1.0/(max-min);
	}
}

void VipProgress::setText(const QString & text)
{
	m_data->text = text;
	if(progressManager())
		QMetaObject::invokeMethod(progressManager(),"setText", Qt::QueuedConnection, Q_ARG(QObjectPointer,this), Q_ARG(QString,text));

	vipProcessEvents(NULL, 50);
}

void VipProgress::setValue(double value)
{
	m_data->value = value;
	int v = qRound( (value - m_data->min)*m_data->invRange * 100);
	bool new_value = false;
	if(v != m_data->intValue)
	{
		new_value = true;
		m_data->intValue = v;
		if(progressManager())
			QMetaObject::invokeMethod(progressManager(),"setValue", Qt::QueuedConnection, Q_ARG(QObjectPointer,this), Q_ARG(int,v));
	}

	qint64 time = QDateTime::currentMSecsSinceEpoch();

	if(time- m_data->lastTime > 200)
	{
		//process gui events every 200ms
		int r = vipProcessEvents(NULL,1);
		m_data->lastTime = QDateTime::currentMSecsSinceEpoch();
		if (r == -3 && QCoreApplication::instance()->thread() == QThread::currentThread() && new_value) //recursice call and we are in the main thread
		{
			//reset value to show the progress
			QCoreApplication::processEvents();
			//QMetaObject::invokeMethod(progressManager(), "setValue", Qt::DirectConnection, Q_ARG(QObjectPointer, this), Q_ARG(int, v));
		}
	}
}

void VipProgress::setCancelable(bool cancelable)
{
	m_data->cancelable = cancelable;
	if(progressManager())
		QMetaObject::invokeMethod(progressManager(),"setCancelable", Qt::QueuedConnection, Q_ARG(QObjectPointer,this), Q_ARG(bool,cancelable));
}

void VipProgress::setModal(bool modal)
{
	m_data->modal = modal;
	if(progressManager())
		QMetaObject::invokeMethod(progressManager(),"setModal", Qt::QueuedConnection, Q_ARG(QObjectPointer,this), Q_ARG(bool,modal));
}

void VipProgress::cancelRequested()
{
	m_data->cancel = true;
}

bool VipProgress::canceled() const
{
	return m_data->cancel;
}

double VipProgress::min() const
{
	return m_data->min;
}

double VipProgress::max() const
{
	return m_data->max;
}

QString VipProgress::text() const
{
	return m_data->text;
}

double VipProgress::value() const
{
	return m_data->value;
}

bool VipProgress::isCancelable() const
{
	return m_data->cancelable;
}

bool VipProgress::isModal() const
{
	return m_data->modal;
}




static QObject* defaultManager()
{
	static DefaultProgressManager manager;
	return &manager;
}

static QObject * & currentManager()
{
	static QObject * current = defaultManager();
	return current;
}

void VipProgress::setProgressManager(QObject * manager)
{
	currentManager() = manager;
}

void VipProgress::resetProgressManager()
{
	currentManager() = defaultManager();
}

QObject * VipProgress::progressManager()
{
	return currentManager();
}
