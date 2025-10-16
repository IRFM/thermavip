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

#ifndef VIP_PY_PROCESSING_H
#define VIP_PY_PROCESSING_H

#include "VipStandardProcessing.h"
#include "VipPyOperation.h"

/// @brief Base class for Python processings.
///
/// The only interest of this class is to make sure that
/// all Python errors will be displayed in the global
/// Python shell.
/// Sub classes must reimplement mergeData() function.
///
class VIP_CORE_EXPORT VipPyBaseProcessing : public VipBaseDataFusion
{
	Q_OBJECT
public:
	VipPyBaseProcessing(QObject* parent = nullptr)
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
/// VipPyFunctionProcessing will take ownership of the function.
/// The function is evaluated throught the global VipPyInterpreter.
///
class VIP_CORE_EXPORT VipPyFunctionProcessing : public VipPyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)

public:
	VipPyFunctionProcessing();
	/// @ brief Destructor, unref the function object (if not null)
	~VipPyFunctionProcessing();

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int index, const QVariant& v) const;
	virtual bool useEventLoop() const { return true; }

	/// @brief Set the Python function to be used
	/// @param pyobject function object. If not null the PyObject will be ref.
	/// This function will hold the GIL.
	void setFunction(void* pyobject);
	void* function() const;

protected:
	virtual void mergeData(int, int);

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Python processing class with one or more inputs and one output.
///
/// VipPyProcessing executes a Python code to tranform its input.
/// The input and output variable is called 'this' within the
/// script. VipPyProcessing exports several variables, which
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
/// A usefull VipPyProcessing can be resitered using registerThisProcessing()
/// to make it available as a global processing object within Thermavip.
/// Registered VipPyProcessing are serialized/deserialized on Thermavip
/// closing/starting.
///
/// VipPyProcessing can also wrap Python processing classes inheriting ThermavipPyProcessing
/// (defined in Python/ThermavipPyProcessing.py). A Python processing class must
/// inherit ThermavipPyProcessing and the classname must start with 'Thermavip' prefix.
///
/// Python processing classes are registered with VipPyInterpreter::addProcessingFile()
/// or VipPyInterpreter::addProcessingDirectory().
///
class VIP_CORE_EXPORT VipPyProcessing : public VipPyBaseProcessing
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
		    "VipPyProcessing always uses the global Python interpreter.")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/PYTHON.png")

	friend VipArchive& operator<<(VipArchive& ar, VipPyProcessing* p);
	friend VipArchive& operator>>(VipArchive& ar, VipPyProcessing* p);

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

	VipPyProcessing(QObject* parent = nullptr);
	~VipPyProcessing();

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
	/// 
	/// For parameters of type VipOtherPlayerData with shouldResizeArray(), and if the other player data is a VipNDArray,
	/// the other player array is resized to ar.
	/// 
	/// If cmds is not null, all python commands will be added to cmds instead of being evaluated into
	/// the global Python interpreter.
	void setStdProcessingParameters(const QVariantMap& args, const VipNDArray& ar = VipNDArray(), VipPyCommandList* cmds = nullptr);
	QVariantMap stdProcessingParameters() const;

	/// @brief Extract the list of parameters from the Python processing class (inheriting 'ThermavipPyProcessing').
	/// This list of parameters is given by the Python 'parameters()' member function.
	QList<Parameter> extractStdProcessingParameters();

	/// @brief Reimplemented from VipProcessingObject
	virtual QList<VipProcessingObject*> directSources() const;

protected:
	virtual void mergeData(int, int);
	virtual void resetProcessing();
	/// @brief Initialize the processing with:
	/// -	the name of a Python processing class inheriting 'ThermavipPyProcessing', without the 'Thermavip' prefix, or
	/// -	a VipPyProcessingPtr object (for registered VipPyProcessing objects using registerThisProcessing()).
	virtual QVariant initializeProcessing(const QVariant& v);

private:
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_REGISTER_QOBJECT_METATYPE(VipPyProcessing*)
typedef QSharedPointer<VipPyProcessing> VipPyProcessingPtr;
Q_DECLARE_METATYPE(VipPyProcessingPtr);

class VipArchive;
VipArchive& operator<<(VipArchive&, VipPyProcessing*);
VipArchive& operator>>(VipArchive&, VipPyProcessing*);

#endif
