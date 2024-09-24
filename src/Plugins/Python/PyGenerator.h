#ifndef PY_GENERATOR_H
#define PY_GENERATOR_H


#include "VipIODevice.h"
#include "PyOperation.h"


/**
Sequential device that simulates video/plot streaming based on a python expression.
*/
class PYTHON_EXPORT PySignalGenerator : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty sampling_time)
	VIP_IO(VipProperty start_time)
	VIP_IO(VipProperty end_time)
	VIP_IO(VipProperty expression)
	VIP_IO(VipProperty complex_expression)
	VIP_IO(VipProperty unit)

	struct ReadThread : public QThread
	{
		PySignalGenerator * generator;
		ReadThread(PySignalGenerator * gen) : generator(gen) {}
		virtual void run();
	};

	QSharedPointer<ReadThread> m_thread;
	CodeObject	m_code;
	QVariant d_data;

public:
	PySignalGenerator(QObject * parent = nullptr)
		:VipTimeRangeBasedGenerator(parent)
	{
		propertyAt(0)->setData(20000000);
		propertyAt(1)->setData(VipInvalidTime);
		propertyAt(2)->setData(VipInvalidTime);
		propertyAt(3)->setData(QString());
		propertyAt(4)->setData(false);
		propertyAt(5)->setData(QString());
		m_thread.reset(new ReadThread(this));
	}

	~PySignalGenerator()
	{
		close();
	}

	virtual void close();
	virtual bool open(VipIODevice::OpenModes);
	virtual DeviceType deviceType() const;

protected:
	virtual bool readData(qint64 time);
	virtual bool enableStreaming(bool enable);

	QVariant computeValue(qint64 time, bool & ok);
};

VIP_REGISTER_QOBJECT_METATYPE(PySignalGenerator*)




#include <QWidget>

class PYTHON_EXPORT PySignalGeneratorEditor : public QWidget
{
	Q_OBJECT

public:
	PySignalGeneratorEditor(QWidget * parent = nullptr);
	~PySignalGeneratorEditor();

	void setGenerator(PySignalGenerator * gen);
	PySignalGenerator * generator() const;

	static PySignalGenerator * createSimpleGenerator() {return createGenerator(false);}
	static PySignalGenerator * createComplexGenerator() { return createGenerator(true); }
	static PySignalGenerator * createGenerator(bool complex_script);

private Q_SLOTS:
	void updateGenerator();
	void updateWidget();
	void updateVisibility();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
