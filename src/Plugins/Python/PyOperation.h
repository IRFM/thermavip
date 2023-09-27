#ifndef PY_OPERATION_H
#define PY_OPERATION_H

#include <QProcess>
#include <QVariant>
#include <QMetaType>
#include <QVector>
#include <QThread>
#include <functional>

#include "VipNDArray.h"
#include "PyConfig.h"


PYTHON_EXPORT QString vipGetPythonDirectory(const QString & suffix = "thermavip");
PYTHON_EXPORT QString vipGetPythonScriptsDirectory(const QString & suffix = "thermavip");
PYTHON_EXPORT QString vipGetPythonScriptsPlayerDirectory(const QString & suffix = "thermavip");
PYTHON_EXPORT QStringList vipGetPythonPlayerScripts(const QString & suffix = "thermavip");
PYTHON_EXPORT qint64 threadId();

#ifdef VIP_ENABLE_PYTHON_LINK

struct PYTHON_EXPORT GIL_Locker
{
	bool state;
	GIL_Locker();
	~GIL_Locker();
};


struct PyDelayExec : QObject
{
	Q_OBJECT
	std::function<void()> m_fun;
	bool m_done;
public:
	template<class T> 
	PyDelayExec(const T & fun) : m_fun(fun), m_done(false)
	{}

	void execDelay();
	bool finished() const;
	bool waitForFinished(int msecs = -1);

private Q_SLOTS:
	void execInternal();
};


//PyObject* to QVariant converter function type
typedef QVariant (*python_to_variant)(void*);
//QVariant to PyObject* converter function type
typedef void* (*variant_to_python)(const QVariant & v);

/** Register a new PyObject* to QVariant converter function */
PYTHON_EXPORT void registerToVariantConverter(python_to_variant);
/** Register a new QVariant to PyObject* converter function */
PYTHON_EXPORT void registerToPythonConverter(variant_to_python);
/** Convert a PyObject* to QVariant based on registered converters.
 * Default converter manages numeric, complex, string, byte and numpy array objects.
*/
PYTHON_EXPORT QVariant pythonToVariant(void *);
/** Convert a QVariant to a PyObject*  based on registered converters.
 * Default converter manages numeric, complex, string, byte and numpy array objects.
*/
PYTHON_EXPORT void * variantToPython(const QVariant &);
/** Convert numpy type to Qt meta type id*/
//PYTHON_EXPORT int numpyToQt(int);
/** Convert Qt meta type id to numpy type*/
//PYTHON_EXPORT int qtToNumpy(int);

/** Sleep \a msec milliseconds */
//PYTHON_EXPORT void sleep(int msec);



class PYTHON_EXPORT PyNDArray : public VipNDArray
{
public:

	PyNDArray();
	PyNDArray(const VipNDArray &);
	PyNDArray(void * py_array);
	PyNDArray(int type, const VipNDArrayShape & shape);

	void * array() const;

	PyNDArray & operator=(const VipNDArray & other);
};



#endif // end VIP_ENABLE_PYTHON_LINK

/** Init the Python library */
PYTHON_EXPORT void initPython();
PYTHON_EXPORT void uninitPython();


struct CodeObject
{
	enum Type {
		File,
		Eval,
		Single
	};
#ifdef VIP_ENABLE_PYTHON_LINK
	void * code;
#endif
	QString pycode;
	CodeObject();
	CodeObject(void * code );
	CodeObject(const QString & expr, Type type = File);
	CodeObject(const CodeObject & other);
	CodeObject & operator=(const CodeObject & other);
	bool isNull() const {
#ifdef VIP_ENABLE_PYTHON_LINK
		return !code;
#else
		return pycode.isEmpty();
#endif
	}
	~CodeObject();
};


struct PYTHON_EXPORT PyError
{
	QString traceback;
	QString filename;
	QString functionName;
	int line;

	PyError(bool compute=false);
	PyError(void * tuple);
	PyError(const QString & traceback, const QString & filename = QString(), const QString & functionName = QString(), int line = 0)
		:traceback(traceback), filename(filename), functionName(functionName), line(line)
	{}
	bool isNull();
	void printDebug(std::ostream &);
};

Q_DECLARE_METATYPE(PyError)

/**
IOOperation represents a data stream that can read/write binary data from/to any kind of operation.
It is similar in concept to the QProcess  class but unlike QProcess, it can manage something else than a distant process
(like a Python interpreter).
 */
class PYTHON_EXPORT IOOperation : public QObject
{
	Q_OBJECT
public:
	IOOperation(QObject * parent) : QObject(parent) {}
	virtual ~IOOperation() {}

	/** Read all available standard output */
	virtual QByteArray readAllStandardOutput() = 0;
	/** Read all available standard error */
	virtual QByteArray readAllStandardError() = 0;
	/** Write data to the data stream */
	virtual qint64 write(const QByteArray &) = 0;

	virtual bool isRunning() const = 0;

	virtual bool handleMagicCommand(const QString& ) { return false; }

public Q_SLOTS:
	/** Starts the I/O operation, should returns just after the startup*/
	virtual bool start() = 0;
	/** Stop the I/O operation, should return just after the operation is completed */
	virtual void stop(bool wait = true) = 0;
	/** Restart the IOOperation*/
	virtual void restart()
	{
		stop();
		start();
	}

Q_SIGNALS:
	/** Emitted when new output data is available to read */
	void readyReadStandardError();
	/** Emitted signal when new error data is available to read */
	void readyReadStandardOutput();
	/** Emitted when the I/O operation has started */
	void started();
	/** Emitted when the I/O operation has finished */
	void finished();

protected Q_SLOTS:
	void emitReadyReadStandardError() {Q_EMIT readyReadStandardError();}
	void emitReadyReadStandardOutput() {Q_EMIT readyReadStandardOutput();}
	void emitStarted() {Q_EMIT started();}
	void emitFinished() {Q_EMIT finished();}
};

/**
A IOOperation that represents a process (managed by a QProcess).
 */
class PYTHON_EXPORT ProcessOperation : public IOOperation
{
	QProcess d_process;

public:
	ProcessOperation(QObject * parent = NULL)
	:IOOperation(parent), d_process(this)
	{
			connect(&d_process,SIGNAL(readyReadStandardOutput()),this,SLOT(emitReadyReadStandardOutput()),Qt::DirectConnection);
			connect(&d_process,SIGNAL(readyReadStandardError()),this,SLOT(emitReadyReadStandardError()),Qt::DirectConnection);
			connect(&d_process,SIGNAL(started()),this,SLOT(emitStarted()),Qt::DirectConnection);
			connect(&d_process,SIGNAL(finished(int , QProcess::ExitStatus)),this,SLOT(emitFinished()),Qt::DirectConnection);
	}

	QProcess * process() const {return const_cast<QProcess*>(&d_process);}
	virtual QByteArray readAllStandardOutput() {return d_process.readAllStandardOutput();}
	virtual QByteArray readAllStandardError() {return d_process.readAllStandardError();}
	virtual qint64 write(const QByteArray & data) {return d_process.write(data);}

	virtual bool start()
	{
		d_process.start();
		return d_process.waitForStarted(2000);
	}

	virtual void stop(bool wait = true)
	{
		d_process.terminate();
		if (wait)
			d_process.waitForFinished();
	}

	virtual bool isRunning() const
	{
		return d_process.state() == QProcess::Running;
	}
};

/**
Base class for all IOOperation managing a Python interpreter.
All interactions with the Python interpreter as asynchrone. They immediately return a command identifier
that can be used to wait for its completion.
 */
class PYTHON_EXPORT PyIOOperation : public IOOperation
{
	Q_OBJECT
public:
	// Command type, used by PyDistant and PyLocal
	typedef uintptr_t command_type;

	PyIOOperation(QObject * parent): IOOperation(parent) {}

	virtual QVariant evalCode(const CodeObject & code, bool * ok = NULL) = 0;
	virtual QVariant setObject(const QString & name, const QVariant & var, bool *ok = NULL) {
		QVariant res = this->wait(this->sendObject(name, var));
		if (ok) *ok = res.userType() == 0;
		return res;
	}
	virtual QVariant getObject(const QString & name, bool *ok = NULL) {
		QVariant res = this->wait(this->retrieveObject(name));
		if (ok) *ok = res.userType() != qMetaTypeId<PyError>();
		return res;
	}

	/** Execute a Python code */
	virtual command_type execCode(const QString & code) = 0;
	/** Send an object and its name to the Python interpreter */
	virtual command_type sendObject(const QString & name, const QVariant & obj) = 0;
	/**Retrieve a Python object from the interpreter using its name and convert it to a QVariant. */
	virtual command_type retrieveObject(const QString & name) = 0;
	/** Wait for a specific operation to be completed, and the return the associated QVariant if this operation was to retrieve a Python object.*/
	virtual QVariant wait(command_type command, int msecs = -1) = 0;

	virtual bool wait(bool * /*alive*/, int /*msecs*/ = -1) { return true; }

	virtual bool isCommandFinished(command_type command) {
		QVariant v = wait(command, 1);
		//the command has not been processed/finished only if v contains a PyError with 'Timeout' traceback
		if (v.userType() == qMetaTypeId<PyError>() && v.value<PyError>().traceback == "Timeout")
			return false;
		return true;
	}

	virtual bool isWaitingForInput()const { return false; }

	virtual bool waitForAskUserInput(int milli = -1) {
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		while (!isWaitingForInput() && (milli < 0 || (QDateTime::currentMSecsSinceEpoch() - st < milli)));
		return isWaitingForInput();
	}

	//These functions should not be used directly

	/** Might be called periodically to stop the current executed code */
	virtual	bool __stopCodeIfNeeded() {return false;}
	/** Add an entry to the standard output */
	virtual	void __addStandardOutput(const QByteArray &) {}
	/** Add an entry to the standard error */
	virtual	void __addStandardError(const QByteArray &) {}
	/** Called whenever the Python code needs an input*/
	virtual	QByteArray __readinput() {return QByteArray();}

public Q_SLOTS:
	virtual void startInteractiveInterpreter();
};



class QThread;



/**
A PyIOOperation class that manage a local Python interpreter.
Local means that the interpreter leaves within your application and does not rely on any other process.
It is much faster than the PyDistant class, but is bound to the Python environment you provide and the Python version and flavor you link against.
 */
class PYTHON_EXPORT PyLocal : public PyIOOperation
{
	Q_OBJECT
public:
	PyLocal(QObject * parent =NULL);
	~PyLocal();

	static QList<PyLocal*> instances();
	static PyLocal * instance(qint64 thread_id);

	/** Return the global PyObject dictionary */
	void * globalDict();
	QThread * thread();

	virtual QByteArray readAllStandardOutput() ;
	virtual QByteArray readAllStandardError() ;
	virtual qint64 write(const QByteArray & data) ;

	virtual command_type execCode(const QString & code);
	virtual command_type sendObject(const QString & name, const QVariant & obj);
	virtual command_type retrieveObject(const QString & name);
	virtual QVariant wait(command_type, int msecs = -1);
	virtual bool wait(bool * alive, int msecs = -1);

	virtual bool start();
	virtual void stop(bool wait = true);
	bool isRunning() const;
	/**
	Returns true if the PyLocal is waiting to be stopped through a call to #stop()
	*/
	bool isStopping() const;
	virtual bool isWaitingForInput() const;


	//These functions should not be used directly
	bool __stopCodeIfNeeded();
	void __addStandardOutput(const QByteArray &);
	void __addStandardError(const QByteArray &);
	QByteArray __readinput();

	virtual void startInteractiveInterpreter();

	virtual QVariant evalCode(const CodeObject & code,  bool * ok = NULL);
	virtual QVariant evalCode(const QString & code, bool * ok = NULL);
	virtual QVariant setObject(const QString & name, const QVariant & var, bool *ok = NULL);
	virtual QVariant getObject(const QString & name, bool *ok = NULL);

	virtual bool handleMagicCommand(const QString&);

	void setWriteToProcess(QProcess*);
	QProcess *writeToProcess() const;

private Q_SLOTS:
	void writeBytesFromProcess();
private:
	class PrivateData;
	PrivateData * d_data;
};


class QMutex;

/**
A PyIOOperation class that manage either a PyDistant or PyLocal interpreter.
 */
class  PYTHON_EXPORT PyOptions : public PyIOOperation
{
	Q_OBJECT
public:

	enum PyType
	{
		Local,
		Distant
	};
	enum PyLaunchCode
	{
		InLocalInterp,
		InIPythonInterp
	};

	PyOptions(QObject * parent = NULL);
	~PyOptions();

	/** Set/get the type of interpreter (local or distant) */
	void setPyType(PyType);
	PyType pyType() const;

	/** Set/get the python executable path (distant only)*/
	void setPython(const QString & );
	const QString & python() const;

	/** Set/get the working directory that the interpreter will be initialized with */
	void setWorkingDirectory(const QString &);
	const QString & workingDirectory() const;

	void setStartupCode(const QString & code);
	QString startupCode() const;

	void setLaunchCode(PyLaunchCode l);
	PyLaunchCode launchCode() const;

	/** Delete the underlying PyIOOperation object */
	void clear();

	virtual bool start();
	virtual void stop(bool wait = true);
	virtual bool isRunning() const;
	virtual bool isWaitingForInput()const;

	/** Return the underlying PyIOOperation object.
	If at least one option has been modified (or if you call this function for the first time), it will
	stop and delete the previous PyIOOperation and create/launch the new one.
	 */
	PyIOOperation * pyIOOperation(bool create_new = false) const;

	virtual QByteArray readAllStandardOutput() ;
	virtual QByteArray readAllStandardError() ;
	virtual qint64 write(const QByteArray & data) ;

	virtual QVariant evalCode(const CodeObject & code, bool * ok = NULL);
	virtual command_type execCode(const QString & code);
	virtual command_type sendObject(const QString & name, const QVariant & obj);
	virtual command_type retrieveObject(const QString & name);
	virtual QVariant wait(command_type, int msecs = -1);

	virtual bool handleMagicCommand(const QString&);

	virtual void startInteractiveInterpreter();

	static PyIOOperation * createNew(QObject * parent = NULL);

	/**
	If you need to preform operations sequentially (send objects in a specific order,...),
	use this mutex
	*/
	static QMutex * operationMutex();

protected Q_SLOTS:
		void printOutput();
		void printError();

private:
	class PrivateData;
	PrivateData * d_data;
};

PYTHON_EXPORT PyOptions * GetPyOptions();
PYTHON_EXPORT void PyAddProcessingDirectory(const QString & dir, const QString & prefix, bool register_processings = true);


typedef QPair<QString, QString> EvalResultType;
PYTHON_EXPORT EvalResultType evalCodeMainThread(const QString & code);

#endif
