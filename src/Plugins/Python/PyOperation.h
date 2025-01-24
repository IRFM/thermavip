#ifndef PY_OPERATION_H
#define PY_OPERATION_H

#include <QProcess>
#include <QVariant>
#include <QMetaType>
#include <QVector>
#include <QThread>
#include <QFileInfo>
#include <functional>

#include "VipNDArray.h"
#include "VipCore.h"
#include "PyConfig.h"

/// @brief Returns the Python data directory
PYTHON_EXPORT QString vipGetPythonDirectory(const QString& suffix = "thermavip");
/// @brief Returns the Python scripts directory
PYTHON_EXPORT QString vipGetPythonScriptsDirectory(const QString& suffix = "thermavip");
/// @brief Returns the directory containing scripts dedicated to player customization
PYTHON_EXPORT QString vipGetPythonScriptsPlayerDirectory(const QString& suffix = "thermavip");
/// @brief Returns all Python files in the player's scripts directory
PYTHON_EXPORT QStringList vipGetPythonPlayerScripts(const QString& suffix = "thermavip");
/// @brief Returns the current thread Id
// PYTHON_EXPORT qint64 vipPyThreadId();

/// @brief PyObject* to QVariant converter function type
typedef QVariant (*python_to_variant)(void*);
/// @brief QVariant to PyObject* converter function type
typedef void* (*variant_to_python)(const QVariant& v);

/// @brief Register a new PyObject* to QVariant converter function
PYTHON_EXPORT void vipRegisterToVariantConverter(python_to_variant);
/// @brief Register a new QVariant to PyObject* converter function
PYTHON_EXPORT void vipRegisterToPythonConverter(variant_to_python);
/// @brief  Convert a PyObject* to QVariant based on registered converters.
/// Default converter manages numeric, complex, string, byte and numpy array objects.
/// The GIL must be held.
PYTHON_EXPORT QVariant vipPythonToVariant(void*);
/// @brief Convert a QVariant to a PyObject*  based on registered converters.
/// Default converter manages numeric, complex, string, byte and numpy array objects.
/// The GIL must be held.
PYTHON_EXPORT void* vipVariantToPython(const QVariant&);
/// @brief Convert numpy type to Qt meta type id
PYTHON_EXPORT int vipNumpyToQt(int);
/// @brief Convert Qt meta type id to numpy type
PYTHON_EXPORT int vipQtToNumpy(int);
/// @brief Convert numpy_array to VipNDArray. The GIL must be held.
PYTHON_EXPORT VipNDArray vipFromNumpyArray(void* numpy_array);
/// @brief Convert any Python object to string. The GIL must be held.
PYTHON_EXPORT QString vipFromPyString(void* obj);
/// @brief Current thread Id
PYTHON_EXPORT quint64 vipPyThreadId();

class VipPyError;
class VipPyLocal;
/// @brief Low level function, execute python code.
/// The GIL must be held.
/// Returns a non null VipPyError on error, the result of evaluated code on success.
PYTHON_EXPORT QVariant vipExecPythonCode(const QString& code, VipPyLocal* local = nullptr);
/// @brief Send a variable to Python global dictionnary.
/// The GIL must be held.
/// Returns sent variable on success, a non null VipPyError on error.
PYTHON_EXPORT QVariant vipSendPythonVariable(const QString& name, const QVariant& value, VipPyLocal* local = nullptr);
/// @brief Retrieve a Python variable from the globals dictionnary.
/// The GIL must be held.
/// Returns the variable on success, a non null VipPyError on error.
PYTHON_EXPORT QVariant vipRetrievePythonVariable(const QString& name, VipPyLocal* local = nullptr);

/// @brief Init the Python library
PYTHON_EXPORT void initPython();
/// @brief Uninit the Python library
PYTHON_EXPORT void uninitPython();

/// @brief GIL RAII locker class
class PYTHON_EXPORT VipGILLocker
{
	bool state;

public:
	VipGILLocker();
	~VipGILLocker();
};

struct compute_error_t
{
};

/// @brief Class representing a Python exception traceback
struct PYTHON_EXPORT VipPyError
{
	QString traceback;
	QString filename;
	QString functionName;
	int line{ 0 };

	VipPyError() = default;
	/// @brief Build the VipPyError object from the last error that occured (using PyErr_Occurred).
	/// In this case, the GIL must be held.
	VipPyError(compute_error_t);
	VipPyError(const QString& traceback, const QString& filename = QString(), const QString& functionName = QString(), int line = 0)
	  : traceback(traceback)
	  , filename(filename)
	  , functionName(functionName)
	  , line(line)
	{
	}

	VipPyError(const VipPyError&) = default;
	VipPyError& operator=(const VipPyError&) = default;
	VipPyError(VipPyError&&) noexcept = default;
	VipPyError& operator=(VipPyError&&) noexcept = default;

	operator const void*() const noexcept { return isNull() ? nullptr : this; }

	bool isNull() const noexcept;
	void printDebug(std::ostream&);
};
Q_DECLARE_METATYPE(VipPyError)

/// @brief Command type executed by a VipPyIOOperation.
///
/// A VipPyCommand is executed by a VipPyIOOperation in
/// either synchronous or asynchronous mode.
///
/// A VipPyCommand supports 3 types of operation:
/// -	Execute Python code ('string' member)
/// -	Send an object to Python global dictionnary ('string' is the name, 'object' is the variable to send)
/// -	Retrieve an object from Python global dictionnary ('string' is the object name).
///
/// In addition, VipPyCommand might have a non empty identifier.
/// The identifier is used to retrieve the command result when
/// executing multiple commands using VipPyIOOperation::execCommands() or
/// VipPyIOOperation::sendCommands() which return a QVariantMap.
/// If no unique id is provided, the command id will be the 'string' member.
///
class VipPyCommand
{
public:
	enum Type
	{
		ExecCode,
		SendObject,
		RetrieveObject
	};
	Type type{ ExecCode };
	QString id;	 //! command unique identifier
	QString string;	 //! Python code to execute (ExecCode) or variable name (SendObject and RetrieveObject)
	QVariant object; //! Object to send (SendObject)

	VipPyCommand() noexcept = default;
	VIP_DEFAULT_MOVE(VipPyCommand);
	
	/// @brief Returns the command unique identifier
	QString buildId() const noexcept { return id.isEmpty() ? string : id; }
};
Q_DECLARE_METATYPE(VipPyCommand)
using VipPyCommandList = QVector<VipPyCommand>;

/// @brief Helper function, build a VipPyCommand to execute Python code
inline VipPyCommand vipCExecCode(const QString& code, const QString& id = QString())
{
	return VipPyCommand{ VipPyCommand::ExecCode, id, code, QVariant() };
}
/// @brief Helper function, build a VipPyCommand to send object to Python global dictionnary
inline VipPyCommand vipCSendObject(const QString& name, const QVariant& object, const QString& id = QString())
{
	return VipPyCommand{ VipPyCommand::SendObject, id, name, object };
}
/// @brief Helper function, build a VipPyCommand to send object to Python global dictionnary
template<class T>
inline VipPyCommand vipCSendObject(const QString& name, const T& object, const QString& id = QString())
{
	return VipPyCommand{ VipPyCommand::SendObject, id, name, QVariant::fromValue(object) };
}
/// @brief Helper function, build a VipPyCommand to retrieve an object from Python global dictionnary
inline VipPyCommand vipCRetrieveObject(const QString& name, const QString& id = QString())
{
	return VipPyCommand{ VipPyCommand::RetrieveObject, id, name, QVariant() };
}

/// @brief VipBaseIOOperation represents a data stream that can
/// read/write binary data from/to any kind of operation.
/// It is similar in concept to the QProcess  class but
/// unlike QProcess, it can manage something else than a
/// distant process (like a Python interpreter).
///
class PYTHON_EXPORT VipBaseIOOperation : public QObject
{
	Q_OBJECT
public:
	VipBaseIOOperation(QObject* parent)
	  : QObject(parent)
	{
	}
	virtual ~VipBaseIOOperation() {}

	/// @brief Read all available standard output
	virtual QByteArray readAllStandardOutput() = 0;
	/// @brief  Read all available standard error
	virtual QByteArray readAllStandardError() = 0;
	/// @brief  Write data to the data stream
	virtual qint64 write(const QByteArray&) = 0;
	/// @brief Returns true if the underlying process/operation is running
	virtual bool isRunning() const = 0;
	/// @brief For shell mode, handle special commands
	virtual bool handleMagicCommand(const QString&) { return false; }

public Q_SLOTS:
	/// @brief Starts the I/O operation, should returns just after the startup
	virtual bool start() = 0;
	/// @brief Stop the I/O operation, should return just after the operation is completed
	virtual void stop(bool wait = true) = 0;
	/// @brief Restart the VipBaseIOOperation
	virtual void restart()
	{
		stop();
		start();
	}

Q_SIGNALS:
	/// @brief Emitted when new output data is available to read
	void readyReadStandardError();
	/// @brief Emitted signal when new error data is available to read
	void readyReadStandardOutput();
	/// @brief Emitted when the I/O operation has started
	void started();
	/// @brief Emitted when the I/O operation has finished
	void finished();

protected Q_SLOTS:
	void emitReadyReadStandardError() { Q_EMIT readyReadStandardError(); }
	void emitReadyReadStandardOutput() { Q_EMIT readyReadStandardOutput(); }
	void emitStarted() { Q_EMIT started(); }
	void emitFinished() { Q_EMIT finished(); }
};

/// @brief Base handler class for VipPyFuture
class VipBasePyRunnable
{
public:
	virtual ~VipBasePyRunnable() noexcept {}
	virtual bool isFinished() const = 0;
	virtual bool wait(int milli = -1) const = 0;
	virtual QVariant value(int milli = -1) const = 0;
};

/// @brief Class similar to QFuture used by VipPyIOOperation asynchronous Python operations execution.
///
/// VipPyFuture destructor does not wait for the underlying Python operation to finish.
///
/// VipPyFuture::wait() can be safely called from the main thread, even if the
/// underlying Python operation uses the main event loop.
///
/// All VipPyFuture members are thread safe.
///
class VipPyFuture
{
	QSharedPointer<VipBasePyRunnable> d_run;

public:
	VipPyFuture() {}
	VipPyFuture(const QSharedPointer<VipBasePyRunnable>& run)
	  : d_run(run)
	{
	}
	VipPyFuture(VipPyFuture&&) noexcept = default;
	VipPyFuture& operator=(VipPyFuture&&) noexcept = default;

	bool isNull() const noexcept { return !d_run; }

	/// @brief Tells if the Python operation is finished
	bool isFinished() const { return d_run ? d_run->isFinished() : true; }
	/// @brief Wait up to milli milliseconds for the Python operation to be finished
	/// Returns true if the operation is finished.
	bool wait(int milli = -1) const
	{
		if (d_run)
			return d_run->wait(milli);
		return true;
	}
	/// @brief If the Python operation has finished, returns its result.
	/// Otherwise, wait specified amount of time for the operation to finish.
	/// If the operation did not finish in time, returns a "Timeout" VipPyError.
	QVariant value(int milli = -1) const { return d_run ? d_run->value(milli) : QVariant(); }
};

/// @brief Base class for all VipBaseIOOperation managing a Python interpreter.
///
/// All interactions with the Python interpreter are asynchronous and return a VipPyFuture object.
///
class PYTHON_EXPORT VipPyIOOperation : public VipBaseIOOperation
{
	Q_OBJECT
	friend class PythonRedirect;
	friend QVariant vipExecPythonCode(const QString& code, VipPyLocal* local);

public:
	// Command type, used by VipPyLocal
	typedef uintptr_t command_type;

	VipPyIOOperation(QObject* parent)
	  : VipBaseIOOperation(parent)
	{
	}

	/// @brief Immediate command evaluation.
	/// Returns the command result (VipPyError object on error).
	virtual QVariant execCommand(const VipPyCommand& cmd) { return sendCommand(cmd).value(); }

	/// @brief Immediate commands evaluation.
	/// Execude all commands and store their results in a QVariantMap.
	/// Command execution will stop at the first error, and a VipPyError object is returned.
	/// Returns the commands results on success (QVariantMap object).
	/// The member VipPyCommand::buildId() is used as keys for the QVariantMap.
	virtual QVariant execCommands(const VipPyCommandList& cmds) { return sendCommands(cmds).value(); }

	/// @brief Asynchronous command evaluation.
	/// Returns a VipPyFuture object.
	virtual VipPyFuture sendCommand(const VipPyCommand& cmd) = 0;

	/// @brief Asynchronous commands evaluation.
	/// Execude all commands asynchronously and store their results in a QVariantMap.
	/// Command execution will stop at the first error, and a VipPyError object is returned.
	/// On success, the VipPyFuture will hold a QVariantMap object.
	/// The member VipPyCommand::buildId() is used as keys for the QVariantMap.
	virtual VipPyFuture sendCommands(const VipPyCommandList& cmds) = 0;

	/// @brief Helper function, similar to 'sendCommand(vipCExecCode(code))'
	VipPyFuture execCode(const QString& code) { return sendCommand(vipCExecCode(code)); }
	/// @brief Helper function, similar to 'sendCommand(vipCSendObject(name, var))'
	VipPyFuture sendObject(const QString& name, const QVariant& var) { return sendCommand(vipCSendObject(name, var)); }
	/// @brief Helper function, similar to 'sendCommand(vipCSendObject(name, var))'
	template<class T>
	VipPyFuture sendObject(const QString& name, const T& var)
	{
		return sendCommand(vipCSendObject(name, QVariant::fromValue(var)));
	}
	/// @brief Helper function, similar to 'sendCommand(vipCRetrieveObject(name))'
	VipPyFuture retrieveObject(const QString& name) { return sendCommand(vipCRetrieveObject(name)); }

	/// @brief Immediate code evaluation
	/// Returns evaluated code (might be empty), or a VipPyError object on error.
	QVariant evalCode(const QString& code) { return execCommand(vipCExecCode(code)); }
	/// @brief Immeediatly set a Python object.
	/// Returns an empty QVariant on success, VipPyError object on error.
	QVariant setObject(const QString& name, const QVariant& var) { return execCommand(vipCSendObject(name, var)); }
	template<class T>
	QVariant setObject(const QString& name, const T& var)
	{
		return execCommand(vipCSendObject(name, QVariant::fromValue(var)));
	}
	/// @brief Returns a Python object.
	/// Returns VipPyError object on error.
	QVariant getObject(const QString& name) { return execCommand(vipCRetrieveObject(name)); }

	/// @brief Initialize the Python interpreter with given parameters.
	/// Returns true on success.
	/// This function must be called once after the object constructor.
	virtual bool initialize(const QVariantMap& params) { return true; }

	/// @brief Wait for all pending operations to be processed.
	/// Stop waiting if alive is false.
	/// Returns false if pending operations are remaining.
	virtual bool wait(bool* /*alive*/, int /*msecs*/ = -1) = 0;

	/// @brief Returns true if the executed Python code is waiting for a user input
	virtual bool isWaitingForInput() const = 0;

protected:
	// These functions should not be used directly

	/** Might be called periodically to stop the current executed code */
	virtual bool __stopCodeIfNeeded() { return false; }
	/** Add an entry to the standard output */
	virtual void __addStandardOutput(const QByteArray&) {}
	/** Add an entry to the standard error */
	virtual void __addStandardError(const QByteArray&) {}
	/** Called whenever the Python code needs an input*/
	virtual QByteArray __readinput() { return QByteArray(); }

public Q_SLOTS:
	/// @brief Start a Python interpreter asynchronously and returns immediatly.
	/// This does not prevent further operations to be performed.
	virtual void startInteractiveInterpreter();
};

/// @brief A VipPyIOOperation class that manage a local Python interpreter.
/// Local means that the interpreter leaves within the application address space
/// and does not rely on any other process.
///
class PYTHON_EXPORT VipPyLocal : public VipPyIOOperation
{
	Q_OBJECT
public:
	VipPyLocal(QObject* parent = nullptr);
	~VipPyLocal();

	/// @brief Returns all VipPyLocal instances
	static QList<VipPyLocal*> instances();
	/// @brief Returns the VipPyLocal object (if any) with given thread Id
	static VipPyLocal* instance(quint64 thread_id);

	/// @brief Return the global PyObject dictionary
	void* globalDict();
	/// @brief Returns the Pylocal thread that executes asynchronous operations.
	QThread* thread();

	/// @brief Read (and flush) pending standard output
	virtual QByteArray readAllStandardOutput();
	/// @brief Read (and flush) pending standard error
	virtual QByteArray readAllStandardError();
	/// @brief Write a line to the Python interpreter.
	/// The line will be used for the next call to input() or similar Python functions.
	/// Returns the number of bytes written.
	virtual qint64 write(const QByteArray& data);

	/// @brief Reimplemented from VipPyIOOperation.
	virtual QVariant execCommand(const VipPyCommand& cmd);
	/// @brief Reimplemented from VipPyIOOperation.
	/// Execute all commands using a single GIL hold
	/// which avoid interuptions by other threads.
	virtual QVariant execCommands(const VipPyCommandList& cmds);

	/// @brief Reimplemented from VipPyIOOperation.
	virtual VipPyFuture sendCommand(const VipPyCommand& cmd);
	/// @brief Reimplemented from VipPyIOOperation.
	/// Commands will be executed using a single GIL hold
	/// which avoid interuptions by other threads.
	virtual VipPyFuture sendCommands(const VipPyCommandList& cmds);

	/// @brief Reimplemented from VipPyIOOperation.
	virtual bool wait(bool* alive, int msecs = -1);
	/// @brief Reimplemented from VipPyIOOperation.
	virtual bool isWaitingForInput() const;
	/// @brief Reimplemented from VipPyIOOperation.
	virtual void startInteractiveInterpreter();
	/// @brief Reimplemented from VipPyIOOperation.
	virtual bool handleMagicCommand(const QString&);

	/// @brief Start the VipPyLocal asynchronous thread
	virtual bool start();
	/// @brief Stop the VipPyLocal asynchronous thread
	/// If wait is true, the function wait for the execution of all pending operations.
	virtual void stop(bool wait = true);
	/// @brief Returns true if the asynchronous thread is running
	virtual bool isRunning() const;
	/// @brief Returns true if the VipPyLocal is waiting to be stopped through a call to stop()
	bool isStopping() const;

	void setWriteToProcess(QProcess*);
	QProcess* writeToProcess() const;

	/// @brief Evaluate Python code in the main GUI thread
	/// @return Pair of QString(result), QString(error)
	static QPair<QString, QString> evalCodeMainThread(const QString& code);

private Q_SLOTS:
	void writeBytesFromProcess();

protected:
	// These functions should not be used directly
	virtual bool __stopCodeIfNeeded();
	virtual void __addStandardOutput(const QByteArray&);
	virtual void __addStandardError(const QByteArray&);
	virtual QByteArray __readinput();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};
VIP_REGISTER_QOBJECT_METATYPE(VipPyLocal*)

/// @brief Global VipPyIOOperation class managing a VipPyLocal or another type of interpreter.
/// Use VipPyInterpreter::instance to retrieve the global interpreter.
/// The global interpreter is used by the Thermavip Python shell as well as Python processing objects.
///
/// All members are thread safe.
///
class PYTHON_EXPORT VipPyInterpreter : public VipPyIOOperation
{
	Q_OBJECT
	VipPyInterpreter(QObject* parent = nullptr);

public:
	/// @brief Tells where to execute code the main Python editor widget
	enum PyLaunchCode
	{
		InLocalInterp,	//! execute the code in the local interpreter
		InIPythonInterp //! execute the code in the IPython shell (if available)
	};
	~VipPyInterpreter();

	static VipPyInterpreter* instance();

	/// @brief Set/get the type of interpreter (class name like "VipPyLocal", which is the default)
	void setPyType(const QString& name);
	QString pyType() const;

	void setPython(const QString&);
	QString python();

	/// @brief Set/get the working directory that the interpreter will be initialized with
	void setWorkingDirectory(const QString&);
	QString workingDirectory() const;

	/// @brief Set/get the startup code
	void setStartupCode(const QString& code);
	QString startupCode() const;

	/// @brief Set/get the parameters used to initialize the internal VipPyIOOperation object after construction
	void setParameters(const QVariantMap& params);
	QVariantMap parameters() const;

	/// @brief Tells if the code editor should execute Python code in the IPython shell or internal shell
	void setLaunchCode(PyLaunchCode l);
	PyLaunchCode launchCode() const;

	/// @brief Set the main interpreter object that should output Python errors
	/// This interpreter should provide the slot showAndRaise() that will
	/// be called on Python processing errors.
	void setMainInterpreter(QObject*);
	QObject* mainInterpreter() const;

	/// @brief Delete the underlying VipPyIOOperation object
	void clear();

	/// @brief Returns the underlying VipPyIOOperation object.
	/// If at least one option has been modified (or if you call this function for the first time),
	/// it will stop and delete the previous VipPyIOOperation and create/launch the new one.
	VipPyIOOperation* pyIOOperation(bool create_new = false) const;

	// Reimplemented from VipPyIOOperation

	virtual bool start();
	virtual void stop(bool wait = true);
	virtual bool isRunning() const;
	virtual bool isWaitingForInput() const;
	virtual bool wait(bool* alive, int msecs = -1);

	virtual QByteArray readAllStandardOutput();
	virtual QByteArray readAllStandardError();
	virtual qint64 write(const QByteArray& data);

	virtual QVariant execCommand(const VipPyCommand& cmd);
	virtual QVariant execCommands(const VipPyCommandList& cmds);

	virtual VipPyFuture sendCommand(const VipPyCommand& cmd);
	virtual VipPyFuture sendCommands(const VipPyCommandList& cmds);

	virtual bool handleMagicCommand(const QString&);

	virtual void startInteractiveInterpreter();

	/// @brief Add a Python file in order to find and register all
	/// Python processing that can be wrapped using PyProcessing class.
	///
	/// This function will execute the file and then:
	/// -	Look for all classes that inherit ThermavipPyProcessing and starts with 'Thermavip' prefix
	/// -	If register_processings is true, found classes are registered using
	///		VipProcessingObject::registerAdditionalInfoObject() in order to be available within Thermavip.
	/// -	Provided category will be used as VipProcessingObject category.
	///
	/// Returns the found classnames.
	///
	QStringList addProcessingFile(const QFileInfo& filename, const QString& category, bool register_processings = true);

	/// @brief Recursively scan a folder and call addProcessingFile()
	/// on each found Python file.
	///
	QStringList addProcessingDirectory(const QString& dir, bool register_processings = true);

protected Q_SLOTS:
	void printOutput();
	void printError();

private:
	VipPyIOOperation* reset(bool create_new);
	VipPyIOOperation* pyIOOperationInternal(bool create_new, bool* is_recursive) const;
	QStringList addProcessingDirectoryInternal(const QString& dir, const QString& prefix, bool register_processings);
	/**
	If you need to preform operations sequentially (send objects in a specific order,...),
	use this mutex
	*/
	// static QMutex* operationMutex();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
