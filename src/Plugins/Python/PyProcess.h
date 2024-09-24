#ifndef PY_PROCESS_H
#define PY_PROCESS_H

#include "PyOperation.h"

#define PY_CODE_ERROR 0
#define PY_CODE_INT 1
#define PY_CODE_LONG 2
#define PY_CODE_DOUBLE 3
#define PY_CODE_COMPLEX 4
#define PY_CODE_STRING 5
#define PY_CODE_BYTES 6
#define PY_CODE_LIST 7
#define PY_CODE_DICT 8
#define PY_CODE_POINT_VECTOR 9
#define PY_CODE_COMPLEX_POINT_VECTOR 10
#define PY_CODE_INTERVAL_SAMPLE_VECTOR 11
#define PY_CODE_NDARRAY 12
#define PY_CODE_NONE 13


/** 
Serialization function, convert raw bytes to QVariant.
The bytes must have been created with #variantToBytes function.
*/
PYTHON_EXPORT QVariant bytesToVariant(const QByteArray &, int * len = nullptr);
/**
Deserialization function, convert a QVariant to a QByteArray object that can be sent to another process, a file or network.
*/
PYTHON_EXPORT QByteArray variantToBytes(const QVariant &);


PYTHON_EXPORT void test_serialize();


class VipNetworkConnection;

/**
A PyIOOperation class that manage a local Python interpreter.
Local means that the interpreter leaves within your application and does not rely on any other process.
It is much faster than the PyDistant class, but is bound to the Python environment you provide and the Python version and flavor you link against.
*/
class PYTHON_EXPORT PyProcess : public PyIOOperation
{
	Q_OBJECT
	friend struct PyProcRunnable;
	friend struct PyProcRunThread;
public:
	PyProcess(QObject * parent = nullptr);
	PyProcess(const QString & pyprocess, QObject * parent = nullptr);
	~PyProcess();

	void setInterpreter(const QString & name);
	QString interpreter() const;

	QThread * thread() const;
	
	virtual QByteArray readAllStandardOutput();
	virtual QByteArray readAllStandardError();
	virtual qint64 write(const QByteArray & data);

	QVariant evalCode(const QString & code, bool * ok = nullptr);
	virtual QVariant evalCode(const CodeObject & code, bool * ok = nullptr) { return evalCode(code.pycode, ok); }
	virtual command_type execCode(const QString & code);
	virtual command_type sendObject(const QString & name, const QVariant & obj);
	virtual command_type retrieveObject(const QString & name);
	virtual QVariant wait(command_type, int msecs = -1);

	virtual bool start();
	virtual void stop(bool wait = true);
	bool isRunning() const;

	virtual void startInteractiveInterpreter();

private:
	void __addStandardOutput(const QByteArray &);
	void __addStandardError(const QByteArray &);
	void __waitForInput();
	QProcess * process() const;
	VipNetworkConnection * connection() const;
	int state() const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};


#endif

