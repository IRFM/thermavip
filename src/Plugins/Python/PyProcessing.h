#ifndef PY_PROCESSING_H
#define PY_PROCESSING_H

#include "VipStandardProcessing.h"
#include "PyConfig.h"
#include "PyOperation.h"

/// @brief Base class for Python processings.
///
/// The only interest of this class is to make sure that
/// all Python errors will be displayed in the global
/// Python shell.
/// Sub classes must reimplement mergeData() function.
///
class PYTHON_EXPORT PyBaseProcessing : public VipBaseDataFusion
{
	Q_OBJECT
public:
	PyBaseProcessing(QObject* parent = nullptr)
	  : VipBaseDataFusion(parent)
	{
	}

protected:
	/// @brief Reimplemented from VipProcessingObject
	virtual void newError(const VipErrorData& error);
};

/// @biref Processing with one input and output that applies a python function.
///
/// The signature of the python function is:
/// 	def my_func(data, params)
///
/// where params is a dictionnary of parameters.
/// The function should return a unique value.
///
/// PyFunctionProcessing will take ownership of the function.
///
class PYTHON_EXPORT PyFunctionProcessing : public PyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)

public:
	PyFunctionProcessing();
	/// @ brief Destructor, unref the function object (if not null)
	~PyFunctionProcessing();

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int index, const QVariant& v) const;
	virtual bool useEventLoop() const { return true; }

	/// @brief Set the Python function to be used
	/// @param pyobject function object. If not null the PyObject will be ref.
	void setFunction(void* pyobject);
	void* function() const;

protected:
	virtual void mergeData(int, int);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Python processing class with one or more inputs and one output.
///
/// PyProcessing executes a Python code to tranform its input.
/// The input and output variable is called 'this' within the
/// script. PyProcessing exports several variables, which
/// are reused by the output:
/// -	'names': list of inputs names
/// -	'name': output name, set by default to the first input name
/// -	'time': inputs time in nanoseconds
/// -	'stylesheet': string stylesheet applied to output
/// -	'units': list of input units (X,Y,Z for all inputs).
///		The first 3 values are used for the output unit (X, Y, Z)
/// -	'attributes': dictionnary of all inputs attributes.
///		Used as output attributes.
/// -	'input_count': number of inputs
/// -	'this': input value (if one input), or list of inputs values.
///
/// A usefull PyProcessing can be resitered using registerThisProcessing()
/// to make it available as a global processing object within Thermavip.
/// Registered PyProcessing are serialized/deserialized on Thermavip
/// closing/starting.
///
/// PyProcessing can also wrap Python processing classes inheriting ThermavipPyProcessing
/// (defined in Python/ThermavipPyProcessing.py). A Python processing class must
/// inherit ThermavipPyProcessing and the classname must start with 'Thermavip' prefix.
///
/// Python processing classes are registered with VipPyInterpreter::addProcessingFile()
/// or VipPyInterpreter::addProcessingDirectory().
///
class PYTHON_EXPORT PyProcessing : public PyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty code)

	Q_CLASSINFO("description",
		    "Apply a python script based on given input.\n"
		    "The processing input is mapped to the Python environement this way:\n"
		    "\t 'this' variable represents the input/output data\n"
		    "\t 'time' variable represents the input data time\n"
		    "\t 'name' variable represents the input/output data name\n"
		    "\t 'units' [X, Y, Z] units, represents the input/output data units\n"
		    "\t 'attributes' dictionnary representing the input/output data attributes\n"
		    "\t 'stylesheet' string variable represents the css style sheet applied to the output data.\n"
		    "PyProcessing always uses the global Python interpreter.")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/python.png")

	friend VipArchive& operator<<(VipArchive& ar, PyProcessing* p);
	friend VipArchive& operator>>(VipArchive& ar, PyProcessing* p);

public:
	/// @brief Structure defining a parameter for a Python prcessing class inheriting 'ThermavipPyProcessing'
	struct Parameter
	{
		QStringList enumValues; // for string enums
		QString defaultValue;	// default parameter value
		QString type;		// type: 'bool' ,'int', 'float', 'str', 'other' (data comming from another player or array edited by the user)
		QString min;		// minimum value (for numerical parameters only)
		QString max;		// maximum value (for numerical parameters only)
		QString step;		// step (for 'int' and 'float' only)
		QString name;		// parameter name
	};

	PyProcessing(QObject* parent = nullptr);
	~PyProcessing();

	/// @brief Reimplemented from VipProcessingObject
	virtual DisplayHint displayHint() const;
	/// @brief Reimplemented from VipProcessingObject
	virtual bool acceptInput(int index, const QVariant& v) const;
	/// @brief Reimplemented from VipProcessingObject
	virtual bool useEventLoop() const { return true; }
	/// @brief Reimplemented from VipProcessingObject
	virtual Info info() const;

	/// @brief Register this processing in order to be serialized and usable within Thermavip
	/// @param category processing category
	/// @param name processing name
	/// @param description short description
	/// @param overwrite overwrite any previously registered processing with the same name
	/// @return true on success
	bool registerThisProcessing(const QString& category, const QString& name, const QString& description, bool overwrite = true);

	/// @brief Returns the last processing error (if any)
	VipPyError lastError() const;

	/// @brief Set/get the maximum Python code execution time.
	/// The Python code takes longer to execute, a processing
	/// error will be set using VipProcessingObject::setError().
	void setMaxExecutionTime(int milli);
	int maxExecutionTime();

	/// @brief Set the name of a Python processing class inheriting 'ThermavipPyProcessing', without the 'Thermavip' suffix.
	/// This will generate the Python code executed in the mergeData() member.
	/// The Python processing class must have been registered first with VipPyInterpreter::addProcessingFile()
	/// or VipPyInterpreter::addProcessingDirectory().
	///
	bool setStdPyProcessingFile(const QString& proc_name);
	QString stdPyProcessingFile() const;

	/// @brief Set the Python processing class parameters (if this processing is based on 'ThermavipPyProcessing' mechanism).
	/// These parameters will be passed in the **kwargs argument of the 'setParameters' member function.
	/// The 2 last parameters are reserved.
	void setStdProcessingParameters(const QVariantMap& args, const VipNDArray& ar = VipNDArray(), VipPyCommandList* cmds = nullptr);
	QVariantMap stdProcessingParameters() const;

	/// @brief Extract the list of parameters from the Python processing class (inheriting 'ThermavipPyProcessing').
	/// This list of parameters is given by the 'parameters()' member function.
	QList<Parameter> extractStdProcessingParameters();

	/// @brief Reimplemented from VipProcessingObject
	virtual QList<VipProcessingObject*> directSources() const;

protected:
	virtual void mergeData(int, int);
	virtual void resetProcessing();
	/// @brief Initialize the processing with:
	/// -	the name of a Python processing class inheriting 'ThermavipPyProcessing', without the 'Thermavip' prefix, or
	/// -	a PyProcessingPtr object (for registered PyProcessing objects using registerThisProcessing()).
	virtual QVariant initializeProcessing(const QVariant& v);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(PyProcessing*)
typedef QSharedPointer<PyProcessing> PyProcessingPtr;
Q_DECLARE_METATYPE(PyProcessingPtr);

class VipArchive;
VipArchive& operator<<(VipArchive&, PyProcessing*);
VipArchive& operator>>(VipArchive&, PyProcessing*);

#endif
