#ifndef PY_PROCESSING_H
#define PY_PROCESSING_H

#include "VipStandardProcessing.h"
#include "PyConfig.h"
#include "PyOperation.h"


/**
Scop lock for #PyOptions::operationMutex().
This locker process the main event loop while trying to grab the lock.
*/
struct PyProcessingLocker
{
	PyProcessingLocker();
	~PyProcessingLocker();
};

/**
Base class for Python processings.
The only interest of this class is to make sure that all Python based processing are applied in a serial way,
to avoid collision of Python variable names.

At any moment tou can query the PyBaseProcessing that is currently being processed with #currentProcessing().

Sub classes must reimplement #applyPyProcessing() function.
*/
class PYTHON_EXPORT PyBaseProcessing : public VipBaseDataFusion
{
	Q_OBJECT
public:
	PyBaseProcessing(QObject * parent = NULL)
		:VipBaseDataFusion(parent)
	{}

	static PyBaseProcessing * currentProcessing();

protected:
	virtual void mergeData(int, int);
	virtual void applyPyProcessing(int, int) = 0;
};


/**
Processing with one input and output that apply a python function.
The signature of the python function is:
	def my_func(data, params)

where params is a dictionnary.
The function should return a tuple(data, params).
*/
class PYTHON_EXPORT PyFunctionProcessing : public PyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)
		
public:
	PyFunctionProcessing();
	~PyFunctionProcessing();

	virtual DisplayHint displayHint() const {	return InputTransform;}
	virtual bool acceptInput(int index, const QVariant & v) const;
	virtual bool useEventLoop() const { return true; }

	void setFunction(void * pyobject);
	void * function() const;

protected:
	virtual void applyPyProcessing(int, int);

private:
	class PrivateData;
	PrivateData * m_data;
};



class PYTHON_EXPORT PyProcessing : public PyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty code)
	VIP_IO(VipProperty output_unit)

	Q_CLASSINFO("description", "Apply a python script based on given input.\n"
		"The processing input is mapped to the Python environement this way:\n"
		"\t 'this' variable represents the input data\n"
		"\t 'time' variable represents the input data time\n"
		"\t 'stylesheet' string variable represents the css style sheet applied to the output data.\n"
		//"\t 'init' is an integer set to 1 if the script is called for the first time, -1 if called for the last time, 0 otherwise\n"
		"PyProcessing always uses the global Python interpreter.")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/python.png")

	friend VipArchive & operator<<(VipArchive& ar, PyProcessing* p);
	friend VipArchive & operator>>(VipArchive& ar, PyProcessing* p);

public:
	enum State
	{
		Stop = -2,
		Uninit = -1,
		Standard = 0,
		Init = 1
	};

	/**
	Structure defining a parameter for a Python prcessing class inheriting 'ThermavipPyProcessing'
	*/
	struct Parameter
	{
		QStringList enumValues; //for string enums
		QString defaultValue; //default parameter value
		QString type; //type: 'bool' ,'int', 'float', 'str', 'other' (data comming from another player or array edited by the user)
		QString min; //minimum value (for numerical parameters only)
		QString max; //maximum value (for numerical parameters only)
		QString step; //step (for 'int' and 'float' only)
		QString name; //parameter name
	};

	PyProcessing(QObject * parent = NULL);
	~PyProcessing();

	virtual DisplayHint displayHint() const;
	virtual bool acceptInput(int index, const QVariant & v) const;
	virtual bool useEventLoop() const { return true; }
	virtual Info info() const;

	bool registerThisProcessing(const QString & category, const QString & name, const QString & description, bool overwrite = true);

	void setState(State state);
	PyError lastError() const;

	void setMaxExecutionTime(int milli);
	int maxExecutionTime();

	/**
	Set the name of a Python processing class inheriting 'ThermavipPyProcessing', without the 'Thermavip' suffix
	This class will be used in the #apply() member.
	*/
	bool setStdPyProcessingFile(const QString & proc_name);
	QString stdPyProcessingFile() const;

	/**
	Set the Python processing class parameters (if this processing is based on 'ThermavipPyProcessing' mechanism).
	These parameters will be passed in the **kwargs argument of the 'setParameters' member function.
	*/
	void setStdProcessingParameters(const QVariantMap & args, const VipNDArray & ar = VipNDArray());
	QVariantMap stdProcessingParameters() const;

	/**
	Extract the list of parameters from the Python processing class (inheriting 'ThermavipPyProcessing').
	This list of parameters is given by the 'parameters()' member function.
	*/
	QList<Parameter> extractStdProcessingParameters();

	/**
	Initialize the processing with the name a Python processing class inheriting 'ThermavipPyProcessing', without the 'Thermavip' suffix.
	Similar to #setStdPyProcessingFile().
	*/
	virtual QVariant initializeProcessing(const QVariant & v);

	virtual QList<VipProcessingObject*> directSources() const;


protected:
	virtual void applyPyProcessing(int,int);
	virtual void resetProcessing();

private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(PyProcessing*)
typedef QSharedPointer<PyProcessing> PyProcessingPtr;
Q_DECLARE_METATYPE(PyProcessingPtr);

class VipArchive;
VipArchive & operator<<(VipArchive& , PyProcessing*);
VipArchive & operator>>(VipArchive& , PyProcessing*);


#include "VipStandardProcessing.h"
#include "qwidget.h"



/**
Edit a small 2D array
*/
class PyArrayEditor : public QWidget
{
	Q_OBJECT
public:
	PyArrayEditor();
	~PyArrayEditor();

	VipNDArray array() const;
	void setText(const QString & text);
	void setArray(const VipNDArray & ar);

private Q_SLOTS:
	void textEntered();
	void finished();
Q_SIGNALS:
	void changed();
private:
	class PrivateData;
	PrivateData * m_data;
};

/**
Edit a data which either a 2D array manually edited (through PyArrayEditor), or a data comming from another player (through VipOtherPlayerDataEditor)
*/
class PyDataEditor : public QWidget
{
	Q_OBJECT
public:
	PyDataEditor();
	~PyDataEditor();

	VipOtherPlayerData value() const;
	void setValue(const VipOtherPlayerData & data);
	void displayVLines(bool before, bool after);

private Q_SLOTS:
	void emitChanged() { Q_EMIT changed(); }
Q_SIGNALS:
	void changed();
private:
	class PrivateData;
	PrivateData * m_data;
};


/**
Editor for PyProcessing based on a Python processing class inheriting 'ThermavipPyProcessing'
*/
class PyParametersEditor : public QWidget
{
	Q_OBJECT
public:
	PyParametersEditor(PyProcessing *);
	~PyParametersEditor();

private Q_SLOTS:
	void updateProcessing();

Q_SIGNALS:
	void changed();
private:
	class PrivateData;
	PrivateData * m_data;
};



class PyApplyToolBar;

/**
Global editor for the PyProcessing class.
*/
class PyProcessingEditor : public QWidget
{
	Q_OBJECT
public:
	PyProcessingEditor();
	~PyProcessingEditor();

	PyApplyToolBar * buttons() const;
	void setPyProcessing(PyProcessing * );

private Q_SLOTS:
	void updatePyProcessing();
	void applyRequested();
	void uninitRequested();
	void registerProcessing();
	void manageProcessing();
private:
	class PrivateData;
	PrivateData * m_data;
};


#endif
