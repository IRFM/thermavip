#include "VipSequentialGenerator.h"
#include "VipSleep.h"
#include <qthread.h>


class VipSequentialGenerator::PrivateData : public QThread
{
public:
	bool stop{ true };
	VipSequentialGenerator* parent{ nullptr };
	QVariant prev;
	qint64 prev_time{ 0 };
	VipSequentialGenerator::generator_function fun;

	PrivateData(VipSequentialGenerator * p)
	  : parent(p)
	{
	}

	virtual void run()
	{ 
		while (!stop) 
		{
			// compute current time
			qint64 time = QDateTime::currentMSecsSinceEpoch() ;
			// compute new value
			QVariant v = fun(prev, prev_time * 1000000ll);
			prev = v;
			prev_time = time;

			// get sampling time
			qint64 sampling_ms = static_cast<qint64>(parent->propertyAt(0)->value<double>() * 1000.);
			//send output value
			VipAnyData any = parent->create(v);
			any.setTime(time * 1000000ll);
			parent->outputAt(0)->setData(any);

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
	d_data = new PrivateData(this);
}

VipSequentialGenerator::~VipSequentialGenerator()
{
	d_data->stop = true;
	d_data->wait();
	delete d_data;
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

	if (enable ) {
		d_data->stop = false;
		d_data->start();
	}
	return true;
}