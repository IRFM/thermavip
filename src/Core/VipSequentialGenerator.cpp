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
			prev = fun(prev);
			prev.setTime(time * 1000000ll);
			prev.setSource(parent);
			prev.mergeAttributes(parent->attributes());

			// get sampling time
			qint64 sampling_ms = static_cast<qint64>(parent->propertyAt(0)->value<double>() * 1000.);
			//send output value
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
	d_data = new PrivateData(this);
	propertyAt(0)->setData(0.01);
}

VipSequentialGenerator::VipSequentialGenerator(const generator_function& fun, double sampling, QObject* parent)
  : VipIODevice(parent)
{
	d_data = new PrivateData(this);
	propertyAt(0)->setData(sampling);
	setGeneratorFunction(fun);
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

	if (enable ) {
		d_data->stop = false;
		d_data->start();
	}
	return true;
}