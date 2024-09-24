
#ifdef _DEBUG
#define VIP_HAS_DEBUG
#undef _DEBUG
extern "C" {
#include "Python.h"

}
#else
extern "C" {
#include "Python.h"
}
#endif

#ifdef VIP_HAS_DEBUG
	#define _DEBUG
	#ifdef _MSC_VER
	// Bug https://github.com/microsoft/onnxruntime/issues/9735
	#define _STL_CRT_SECURE_INVALID_PARAMETER(expr) _CRT_SECURE_INVALID_PARAMETER(expr)
	#endif
#endif

#include "PyProcessing.h"
#include "PyEditor.h"
#include "PyRegisterProcessing.h"
#include "IOOperationWidget.h"
#include "VipProcessingObjectEditor.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"

#include <QPointer>
#include <QBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <qplaintextedit.h>
#include <qradiobutton.h>
#include <qmessagebox.h>
#include <qcoreapplication.h>


static int registerPyProcessingPtr() {
	qRegisterMetaType<PyProcessingPtr>();
	return 0;
}
static int _registerPyProcessingPtr = registerPyProcessingPtr();


PyProcessingLocker::PyProcessingLocker()
{
	while (!PyOptions::operationMutex()->tryLock()) {
		if (QCoreApplication::instance() && QCoreApplication::instance()->thread() == QThread::currentThread())
			vipProcessEvents(nullptr, 10);
		else
			QThread::msleep(5);
	}
}
PyProcessingLocker::~PyProcessingLocker()
{
	PyOptions::operationMutex()->unlock();
}



PyBaseProcessing * _currentPyBaseProcessing = nullptr;

PyBaseProcessing * PyBaseProcessing::currentProcessing()
{
	return _currentPyBaseProcessing;
}

void PyBaseProcessing::mergeData(int t1, int t2)
{
	_currentPyBaseProcessing = this;

	//PyProcessing and PySignalFusionProcessing objects all share the same global python interpreter. 
	//Since we are going to send/retrieve a lot of objects, protect this function to make sure that
	//multiple PyBaseProcessing are not sharing their data
	PyProcessingLocker lock;
	this->applyPyProcessing(t1, t2);

	_currentPyBaseProcessing = nullptr;
}




class PyFunctionProcessing::PrivateData
{
public:
	PyObject * function;
	PyError lastError;
	uint maxExecutionTime;
};

PyFunctionProcessing::PyFunctionProcessing()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->function = nullptr;
	d_data->maxExecutionTime = 5000;
}
PyFunctionProcessing::~PyFunctionProcessing()
{
	if (d_data->function) {
		GIL_Locker lock;
		Py_DECREF(d_data->function);
	}
}

bool PyFunctionProcessing::acceptInput(int, const QVariant & v) const
{
	if (v.userType() == qMetaTypeId<VipNDArray>())
		return true;
	else if (v.userType() == qMetaTypeId<VipPointVector>())
		return true;
	else if (v.userType() == qMetaTypeId<VipComplexPointVector>())
		return true;
	return v.canConvert<QString>();
}

void PyFunctionProcessing::setFunction(void * pyobject)
{
	GIL_Locker lock;
	//PyObject * tmp = (PyObject*)pyobject;
	if (d_data->function)
		Py_DECREF(d_data->function);
	d_data->function = (PyObject*)pyobject;
	if (d_data->function)
		Py_INCREF(d_data->function);
}
void * PyFunctionProcessing::function() const
{
	return d_data->function;
}

void PyFunctionProcessing::applyPyProcessing(int, int)
{
	const QVector<VipAnyData> in = inputs();
	if (in.size() != 1) {
		setError("wrong input count (should be 1)");
		return;
	}
	VipAnyData out = create(in.size() ? in.first().data() : QVariant());

	if (!d_data->function) {
		outputAt(0)->setData(out);
		return;
	}

	//get the current data units and names
	QStringList units;
	QStringList names;
	for (int i = 0; i < in.size(); ++i) {
		units << in[i].xUnit() << in[i].yUnit() << in[i].zUnit();
		names << in[i].name();
	}

	//find output player
	QList<VipAbstractPlayer*> players = VipAbstractPlayer::findOutputPlayers(this);
	int player = players.size() ? VipUniqueId::id(VipBaseDragWidget::fromChild(players.first())) : 0;

	//send units, names, time and input data
	PyIOOperation::command_type c = GetPyOptions()->sendObject("units", QVariant::fromValue(units));
	c = GetPyOptions()->sendObject("names", QVariant::fromValue(names));
	c = GetPyOptions()->sendObject("time", QVariant::fromValue(out.time()));
	c = GetPyOptions()->sendObject("player", QVariant::fromValue(player));
	c = GetPyOptions()->sendObject("input_count", QVariant::fromValue(in.size()));
	c = GetPyOptions()->sendObject("this", in[0].data());

	//check sending errors
	d_data->lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
	if (!d_data->lastError.isNull())
	{
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError("cannot send objects to the Python interpreter", VipProcessingObject::WrongInput);

		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	//send function
	{
		GIL_Locker lock;
		PyObject* __main__ = PyImport_ImportModule("__main__");
		PyObject *globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);
		int r = PyDict_SetItemString(globals, "fun", d_data->function);
		if (r != 0) {
			d_data->lastError = PyError(true);
			if (!d_data->lastError.isNull())
			{
				vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
				setError("cannot send objects to the Python interpreter", VipProcessingObject::WrongInput);
				QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
				outputAt(0)->setData(out);
				return;
			}
		}
	}

	const char * code = "this = fun(this)";

	//execute actual processing code
	c = GetPyOptions()->execCode(code);
	d_data->lastError = GetPyOptions()->wait(c, d_data->maxExecutionTime).value<PyError>();
	if (!d_data->lastError.isNull())
	{
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError(d_data->lastError.traceback);
		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	//retrieve 'this' object
	c = GetPyOptions()->retrieveObject("this");
	QVariant value = GetPyOptions()->wait(c, 10000);
	if (!value.value<PyError>().isNull())
	{
		d_data->lastError = value.value<PyError>();
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError(d_data->lastError.traceback);
		pyGetPythonInterpreter()->show();
		pyGetPythonInterpreter()->raise();
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	//output conversion
	//specific cases: VipNDArray to VipPointVector and VipNDArray to VipComplexPointVector
	bool input_is_vector = (in[0].data().userType() == qMetaTypeId<VipPointVector>() || in[0].data().userType() == qMetaTypeId<VipComplexPointVector>());
	if (input_is_vector && value.userType() == qMetaTypeId<VipNDArray>())
	{
		const VipNDArray ar = value.value<VipNDArray>();
		if (ar.shapeCount() == 2 && ar.shape(0) == 2)
		{
			//convert back to VipPointVector or VipComplexPointVector
			if (ar.dataType() == qMetaTypeId<complex_d>() || ar.dataType() == qMetaTypeId<complex_f>())
				value = QVariant::fromValue(value.value<VipComplexPointVector>());
			else
				value = QVariant::fromValue(value.value<VipPointVector>());
		}
	}

	out.setData(value);

	//grab the units
	value = GetPyOptions()->wait(GetPyOptions()->retrieveObject("units"), 2000);
	if (!value.value<PyError>().isNull())
	{
		d_data->lastError = value.value<PyError>();
		setError(d_data->lastError.traceback);
		outputAt(0)->setData(out);
		return;
	}
	QVariantList vunits = value.value<QVariantList>();
	if (vunits.size() > 2)
	{
		out.setXUnit(vunits[0].toString());
		out.setYUnit(vunits[1].toString());
		out.setZUnit(vunits[2].toString());
	}

	QString stylesheet;
	//grab the style sheet
	value = GetPyOptions()->wait(GetPyOptions()->retrieveObject("stylesheet"), 2000);
	if (value.value<PyError>().isNull())
	{
		stylesheet = value.toString();
	}
	out.setAttribute("stylesheet", stylesheet);
	outputAt(0)->setData(out);
}





class PyProcessing::PrivateData
{
public:
	PrivateData() :state(PyProcessing::Init), maxExecutionTime(5000), minDims(-1), maxDims(-1),
		need_resampling(true), need_same_type(true), need_same_sub_type(false), minInputCount(1), maxInputCount(10) {}
	PyProcessing::State state;
	int maxExecutionTime;
	PyError lastError;
	QString std_proc_name;
	QVariant initialize;
	QVariantMap stdProcessingParameters;
	QList<Parameter> extractParameters;
	QString lastExecutedCode;
	VipProcessingObject::Info info;
	int minDims, maxDims;
	bool need_resampling;
	bool need_same_type;
	bool need_same_sub_type;
	int minInputCount, maxInputCount;
};

PyProcessing::PyProcessing(QObject * parent)
	:PyBaseProcessing(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->lastError.traceback = "Uninitialized";
	propertyAt(1)->setData(QString());
	topLevelInputAt(0)->toMultiInput()->setMinSize(1);
	topLevelInputAt(0)->toMultiInput()->resize(1);
	setWorkOnSameObjectType(d_data->need_same_type);
	setSameDataType(d_data->need_same_type);
	setResampleEnabled(d_data->need_same_sub_type, true);
}

PyProcessing::~PyProcessing()
{
}


void PyProcessing::setMaxExecutionTime(int milli)
{
	d_data->maxExecutionTime = milli;
	emitProcessingChanged();
}

int PyProcessing::maxExecutionTime()
{
	return d_data->maxExecutionTime;
}

void PyProcessing::setState(State state)
{
	d_data->state = state;
}

PyError PyProcessing::lastError() const
{
	return d_data->lastError;
}

QVariant PyProcessing::initializeProcessing(const QVariant & v)
{
	d_data->initialize = v;
	if (PyProcessingPtr ptr = v.value<PyProcessingPtr>()) {
		//initialize from a PyProcessingPtr
		topLevelInputAt(0)->toMultiInput()->resize(1);
		propertyName("code")->setData(ptr->propertyName("code")->data());
		propertyName("Time_range")->setData(ptr->propertyName("Time_range")->data());
		propertyName("output_unit")->setData(ptr->propertyName("output_unit")->data());
		return QVariant(true);
	}
	else {
		//initialize the processing based on the Python class name, without the prefix 'Thermavip'
		//d_data->initialize = v.toString();
		return QVariant::fromValue(setStdPyProcessingFile(d_data->initialize.toString()));
	}
}

QList<VipProcessingObject*> PyProcessing::directSources() const
{
	QList<VipProcessingObject*> res = VipProcessingObject::directSources();
	for (QVariantMap::iterator it = d_data->stdProcessingParameters.begin(); it != d_data->stdProcessingParameters.end(); ++it)
	{
		QVariant value = it.value();
		if (value.userType() == qMetaTypeId<VipOtherPlayerData>())
		{
			if (VipProcessingObject * obj = value.value<VipOtherPlayerData>().processing())
				if (obj != this && res.indexOf(obj) < 0)
					res.append(obj);
		}
	}
	return res;
}

void PyProcessing::setStdProcessingParameters(const QVariantMap & args, const VipNDArray & ar)
{
	//for a standard processing (Py file in vipGetPythonDirectory()), set the processing class parameters.
	//this will call the processing memeber 'setParameters'.

	if (d_data->std_proc_name.isEmpty())
		return;

	d_data->stdProcessingParameters = args;
	d_data->extractParameters.clear();

	//build the dict of parameters
	QStringList parameters;
	for (QVariantMap::iterator it = d_data->stdProcessingParameters.begin(); it != d_data->stdProcessingParameters.end(); ++it)
	{
		//for 'other' type, send the object before in the 'other' variable
		if (it.value().userType() == qMetaTypeId<VipOtherPlayerData>())
		{
			VipOtherPlayerData other = it.value().value<VipOtherPlayerData>();
			QVariant value = other.data().data();

			//if the data is an array, resize it based on ar
			const VipNDArray d = value.value<VipNDArray>();
			if (!d.isEmpty() && d.shapeCount() == ar.shapeCount() && (other.isDynamic() || other.shouldResizeArray()))
			{
				VipNDArray resized = d.resize(ar.shape());
				value = QVariant::fromValue(resized);
			}

			//send 'other' that will be passed in **kwargs
			PyIOOperation::command_type c = GetPyOptions()->sendObject("other", value);
			GetPyOptions()->wait(c, 5000);
			parameters << it.key() + "= other";
		}
		else
			parameters << it.key() + "=" + it.value().toString();
	}

	//set the parameters if required
	if (parameters.size())
	{
		QString classname = "Thermavip" + d_data->std_proc_name;
		QString id = QString::number(qint64(this));
		QString code =
			"pr = procs[" + id + "]\n"
			"pr.setParameters(" + parameters.join(",") + ")\n";


		PyIOOperation::command_type c = GetPyOptions()->execCode(code);
		GetPyOptions()->wait(c, 5000);
	}
}

QVariantMap PyProcessing::stdProcessingParameters() const
{
	return d_data->stdProcessingParameters;
}

QList<PyProcessing::Parameter> PyProcessing::extractStdProcessingParameters()
{
	//extract the processing class parameters defined with the memeber 'parameters()'
	//See "ThermavipPyProcessing' Python class for more details
	if (d_data->extractParameters.size())
		return d_data->extractParameters;
	else if (d_data->std_proc_name.isEmpty())
	{
		return QList<PyProcessing::Parameter>();
	}

	QString id = QString::number(qint64(this));
	QString code =
		"try: pr = procs[" + id + "]\n"
		"except: pr= Thermavip" + d_data->std_proc_name + "()\n"
		"tmp = pr.parameters()";

	PyIOOperation::command_type c = GetPyOptions()->execCode(code);
	PyError err = GetPyOptions()->wait(c, 5000).value<PyError>();
	if (!err.isNull())
	{
		vip_debug("err: %s\n", err.traceback.toLatin1().data());
		return QList<Parameter>();
	}
	QVariant v = GetPyOptions()->wait(GetPyOptions()->retrieveObject("tmp"), 5000);
	if (!v.value<PyError>().isNull())
	{
		vip_debug("err: %s\n", v.value<PyError>().traceback.toLatin1().data());
		return QList<Parameter>();
	}
	QList<Parameter> res;
	QVariantMap map = v.value<QVariantMap>();
	for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it)
	{
		QString name = it.key();
		QVariant v = it.value();
		QVariantList lst = v.value<QVariantList>();
		if (lst.size() == 0)
			lst << v;

		if (lst.size())
		{
			Parameter p;
			p.name = name;
			p.type = lst[0].toString();
			if (lst.size() > 1) p.defaultValue = lst[1].toString();
			if (p.type == "int" || p.type == "bool" || p.type == "float")
			{
				if (lst.size() > 2) p.min = lst[2].toString();
				if (lst.size() > 3) p.max = lst[3].toString();
				if (lst.size() > 4) p.step = lst[4].toString();
				res << p;
			}
			else if (p.type == "str")
			{
				for (int i = 2; i < lst.size(); ++i)
					p.enumValues.append(lst[i].toString());
				res << p;
			}
			else if (p.type == "other")
			{
				res << p;
			}
		}

	}

	return  d_data->extractParameters = res;
}

bool PyProcessing::acceptInput(int, const QVariant & v) const
{
	if (d_data->minDims < 0)
		return true;

	if (v.userType() == qMetaTypeId<VipNDArray>())
	{
		int shapeCount = v.value<VipNDArray>().shapeCount();
		return shapeCount >= d_data->minDims && shapeCount <= d_data->maxDims;
	}
	else if (v.userType() == qMetaTypeId<VipPointVector>())
		return 1 >= d_data->minDims && 1 <= d_data->maxDims;
	else if (v.userType() == qMetaTypeId<VipComplexPointVector>())
		return 1 >= d_data->minDims && 1 <= d_data->maxDims;
	return v.canConvert<QString>();
}

PyProcessing::DisplayHint PyProcessing::displayHint() const
{
	if (d_data->std_proc_name.size()) return d_data->info.displayHint;
	return InputTransform;
}

PyProcessing::Info PyProcessing::info() const
{
	if (d_data->info.metatype == 0) {
		if (d_data->std_proc_name.size()) {
			const QList<VipProcessingObject::Info> infos = additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i)
				if (infos[i].classname == d_data->std_proc_name)
				{
					const_cast<PyProcessing::Info&>(d_data->info) = infos[i];
					break;
				}
		}
		else if (PyProcessingPtr ptr = d_data->initialize.value<PyProcessingPtr>()) {
			const QList<VipProcessingObject::Info> infos = additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i)
				if (infos[i].init.value<PyProcessingPtr>() == ptr)
				{
					const_cast<PyProcessing::Info&>(d_data->info) = infos[i];
					break;
				}
		}
		else
			const_cast<PyProcessing::Info&>(d_data->info) = VipProcessingObject::info();
	}
	return d_data->info;
}

bool PyProcessing::registerThisProcessing(const QString & category, const QString & name, const QString & description, bool overwrite)
{
	if (name.isEmpty())
		return false;

	VipProcessingObject::Info info = this->info();
	info.classname = name;
	info.category = category.split("/", QString::SkipEmptyParts).join("/"); //remove empty strings
	info.description = description;
	info.displayHint = VipProcessingObject::InputTransform;

	PyProcessingPtr init(new PyProcessing());
	init->topLevelInputAt(0)->toMultiInput()->resize(1);
	init->propertyName("code")->setData(this->propertyName("code")->data());
	init->propertyName("Time_range")->setData(this->propertyName("Time_range")->data());
	init->propertyName("output_unit")->setData(this->propertyName("output_unit")->data());
	info.init = QVariant::fromValue(init);

	//check if already registerd
	if (!overwrite) {
		QList<VipProcessingObject::Info> infos = additionalInfoObjects();
		for (int i = 0; i < infos.size(); ++i) {
			if (infos[i].classname == name && infos[i].category == info.category)
				return false;
		}
	}
	//register processing info
	registerAdditionalInfoObject(info);

	return PyRegisterProcessing::saveCustomProcessings();
}

bool PyProcessing::setStdPyProcessingFile(const QString & proc_name)
{
	//check if the processing is registerd
	QList<VipProcessingObject::Info> infos = VipProcessingObject::additionalInfoObjects();
	bool found = false;
	for (int i = 0; i < infos.size(); ++i)
		if (infos[i].classname == proc_name)
		{
			found = true;
			break;
		}
	if (!found)
		return false;

	//update the procs dict
	QString id = QString::number(qint64(this));
	QString exec =
		"#create dictionary of processings\n"
		"try :\n"
		" _procs = dict(procs)\n"
		"except :\n"
		" procs = dict()\n"
		"pr = procs[" + id + "] = Thermavip" + proc_name + "()\n"
		"dims = pr.dims()\n"
		"need_resampling = True\n"
		"need_same_type = True\n"
		"need_same_sub_type = False\n"
		"input_count = (1,1)\n"
		"try:\n"
		"  need_same_type = pr.needSameType()\n"
		"  need_same_sub_type = pr.needSameSubType()\n"
		"  input_count = pr.inputCount()\n"
		"  need_resampling = pr.needResampling()\n"
		"except Exception as e: pass";

	PyIOOperation::command_type c = GetPyOptions()->execCode(exec);
	PyError err = GetPyOptions()->wait(c, 5000).value<PyError>();
	if (!err.isNull())
	{
		vip_debug("err: %s\n", err.traceback.toLatin1().data());
	}

	//retrieve the valid dimensions
	c = GetPyOptions()->retrieveObject("dims");
	QVariant v = GetPyOptions()->wait(c, 2000);
	QVariantList dims = v.value<QVariantList>();
	if (dims.size() == 2)
	{
		d_data->minDims = dims[0].toInt();
		d_data->maxDims = dims[1].toInt();
	}

	//retrieve other class parameters
	d_data->need_resampling = GetPyOptions()->wait(GetPyOptions()->retrieveObject("need_resampling"), 2000).toBool();
	d_data->need_same_type = GetPyOptions()->wait(GetPyOptions()->retrieveObject("need_same_type"), 2000).toBool();
	d_data->need_same_sub_type = GetPyOptions()->wait(GetPyOptions()->retrieveObject("need_same_sub_type"), 2000).toBool();
	QVariantList lst = GetPyOptions()->wait(GetPyOptions()->retrieveObject("input_count"), 2000).value<QVariantList>();
	if (lst.size() == 2) {
		d_data->minInputCount = lst[0].toInt();
		d_data->maxInputCount = lst[1].toInt();
		/*if(d_data->minInputCount > d_data->maxInputCount) std::swap(d_data->minInputCount,d_data->maxInputCount);
		if(d_data->minInputCount <2)d_data->minInputCount = 2;
		if(d_data->maxInputCount <2)d_data->maxInputCount = 2;*/
		VipMultiInput * input = topLevelInputAt(0)->toMultiInput();
		input->setMinSize(d_data->minInputCount);
		input->setMaxSize(d_data->maxInputCount);
		input->resize(d_data->minInputCount);
		setWorkOnSameObjectType(d_data->need_same_type);
		setSameDataType(d_data->need_same_sub_type);
		setResampleEnabled(d_data->need_resampling, true);
	}
	else
		topLevelInputAt(0)->toMultiInput()->resize(1);

	//build the code that will be executed in the apply() member
	QString classname = "Thermavip" + proc_name;
	QString code =

		"#retrieve this processing\n"
		"try :\n"
		" pr = procs[" + id + "]\n"
		"except :\n"
		" pr = procs[" + id + "] = " + classname + "()\n"
		//"if init :\n"
		//" this = pr._reset(this, time)\n"
		//"else :\n"
		"this = pr._apply(this, time)\n"
		"\n"
		"if input_count == 1:\n"
		"  units[0] = pr.unit(0,units[0])\n"
		"  units[1] = pr.unit(1,units[1])\n"
		"  units[2] = pr.unit(2,units[2])\n"
		"else:\n"
		"  units[0] = pr.unit(0,units[0:input_count])\n"
		"  units[1] = pr.unit(1,units[input_count:input_count*2])\n"
		"  units[2] = pr.unit(2,units[input_count*2:input_count*3])\n"
		"\n"
		"if input_count > 1:\n"
		"  name = pr.name(names)\n";

	propertyAt(1)->setData(code);
	d_data->initialize = proc_name;
	d_data->std_proc_name = proc_name;
	d_data->extractParameters.clear();
	return true;
}

QString PyProcessing::stdPyProcessingFile() const
{
	return d_data->std_proc_name;
}

void PyProcessing::resetProcessing()
{
	d_data->state = Init;
}

void PyProcessing::applyPyProcessing(int, int)
{
	//initialize the standard processing (if any) based on 'ThermavipPyProcessing' class
	if (d_data->initialize.toString().size() && d_data->std_proc_name != d_data->initialize.toString())
	{
		setStdPyProcessingFile(d_data->initialize.toString());
	}

	if (d_data->std_proc_name.size())
	{
		//Resend the Python processing class parameters if:
		// - One of the parameter is a dynamic VipOtherPlayerData
		// - One of the parameter is a VipOtherPlayerData that needs resizing
		// - The current code is different that the last one executed
		bool reset_parameters = (d_data->lastExecutedCode.isEmpty() || d_data->lastExecutedCode != propertyAt(1)->value<QString>());
		if (!reset_parameters)
		{
			for (QVariantMap::iterator it = d_data->stdProcessingParameters.begin(); it != d_data->stdProcessingParameters.end(); ++it)
			{
				if (it.value().userType() == qMetaTypeId<VipOtherPlayerData>())
				{
					if (it.value().value<VipOtherPlayerData>().isDynamic())
					{
						reset_parameters = true;
						break;
					}
					else if (it.value().value<VipOtherPlayerData>().shouldResizeArray())
					{
						reset_parameters = true;
						break;
					}
				}

			}
		}

		if (reset_parameters)
		{
			//reset the parameters
			setStdProcessingParameters(d_data->stdProcessingParameters, inputs()[0].value<VipNDArray>());
		}
	}

	const QVector<VipAnyData> in = inputs();
	VipAnyData out = create(in.size() ? in.first().data() : QVariant());

	/*if (d_data->state == Stop)
	{
	if(in.size()==1)
	outputAt(0)->setData(out);
	return;
	}*/

	//get the current data units and names
	QStringList units;
	QStringList names;
	for (int i = 0; i < in.size(); ++i) {
		units << in[i].xUnit() << in[i].yUnit() << in[i].zUnit();
		names << in[i].name();
	}

	//find output player
	QList<VipAbstractPlayer*> players = VipAbstractPlayer::findOutputPlayers(this);
	int player = players.size() ? VipUniqueId::id(VipBaseDragWidget::fromChild(players.first())) : 0;

	//send units, names, time and input data
	PyIOOperation::command_type c = GetPyOptions()->sendObject("units", QVariant::fromValue(units));
	c = GetPyOptions()->sendObject("names", QVariant::fromValue(names));
	c = GetPyOptions()->sendObject("time", QVariant::fromValue(out.time()));
	c = GetPyOptions()->sendObject("player", QVariant::fromValue(player));
	c = GetPyOptions()->sendObject("input_count", QVariant::fromValue(in.size()));
	//c = GetPyOptions()->sendObject("init", (int)d_data->state);
	if (in.size() == 1)
		c = GetPyOptions()->sendObject("this", in[0].data());
	else {
		QVariantList lst;
		for (int i = 0; i < in.size(); ++i) lst.append(in[i].data());
		c = GetPyOptions()->sendObject("this", lst);
	}

	//check sending errors
	d_data->lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
	if (!d_data->lastError.isNull())
	{
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError("cannot send objects to the Python interpreter", VipProcessingObject::WrongInput);

		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	//execute actual processing code
	QString code = propertyAt(1)->data().value<QString>();
	c = GetPyOptions()->execCode(code); 
	
	d_data->lastError = GetPyOptions()->wait(c, d_data->maxExecutionTime).value<PyError>();
	if (!d_data->lastError.isNull())
	{
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError(d_data->lastError.traceback);
		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	//retrieve 'this' object
	c = GetPyOptions()->retrieveObject("this");
	QVariant value = GetPyOptions()->wait(c, 10000);
	if (!value.value<PyError>().isNull())
	{
		d_data->lastError = value.value<PyError>();
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError(d_data->lastError.traceback);
		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	//output conversion
	//specific cases: VipNDArray to VipPointVector and VipNDArray to VipComplexPointVector
	bool input_is_vector = (in[0].data().userType() == qMetaTypeId<VipPointVector>() || in[0].data().userType() == qMetaTypeId<VipComplexPointVector>());
	if (input_is_vector && value.userType() == qMetaTypeId<VipNDArray>())
	{
		const VipNDArray ar = value.value<VipNDArray>();
		if (ar.shapeCount() == 2 && ar.shape(0) == 2)
		{
			//convert back to VipPointVector or VipComplexPointVector
			if (ar.dataType() == qMetaTypeId<complex_d>() || ar.dataType() == qMetaTypeId<complex_f>())
				value = QVariant::fromValue(value.value<VipComplexPointVector>());
			else
				value = QVariant::fromValue(value.value<VipPointVector>());
		}
	}

	out.setData(value);

	//grab the units
	value = GetPyOptions()->wait(GetPyOptions()->retrieveObject("units"), 2000);
	if (!value.value<PyError>().isNull())
	{
		d_data->lastError = value.value<PyError>();
		setError(d_data->lastError.traceback);
		outputAt(0)->setData(out);
		return;
	}
	QVariantList vunits = value.value<QVariantList>();
	if (vunits.size() > 2)
	{
		out.setXUnit(vunits[0].toString());
		out.setYUnit(vunits[1].toString());
		out.setZUnit(vunits[2].toString());
	}

	QString stylesheet;
	//grab the style sheet
	value = GetPyOptions()->wait(GetPyOptions()->retrieveObject("stylesheet"), 2000);
	if (value.value<PyError>().isNull())
	{
		stylesheet = value.toString();
	}
	out.setAttribute("stylesheet", stylesheet);


	//grab the name for multi inputs only
	if (in.size() > 1) {
		value = GetPyOptions()->wait(GetPyOptions()->retrieveObject("name"), 2000);
		if (!value.value<PyError>().isNull())
		{
			d_data->lastError = value.value<PyError>();
			setError(d_data->lastError.traceback);
			outputAt(0)->setData(out);
			return;
		}
		QString name = value.value<QString>();
		out.setName(name);
	}

	outputAt(0)->setData(out);
	d_data->lastExecutedCode = code;
}



#include "VipArchive.h"
VipArchive & operator<<(VipArchive& ar, PyProcessing* p)
{
	ar.content("maxExecutionTime", p->maxExecutionTime());
	ar.content("stdPyProcessingFile", p->stdPyProcessingFile());
	ar.content("stdProcessingParameters", p->stdProcessingParameters());

	//added in 2.2.14
	//we need to save the init parameter to retrieve it at reloading
	/*VipProcessingObject::Info info = p->info();
	if (PyProcessingPtr ptr = info.init.value<PyProcessingPtr>())
	ar.content("registered", info.category + info.classname);
	else
	ar.content("registered", QString());*/
	return ar;
}

VipArchive & operator>>(VipArchive& ar, PyProcessing* p)
{
	p->setMaxExecutionTime(ar.read("maxExecutionTime").toInt());
	p->setStdPyProcessingFile(ar.read("stdPyProcessingFile").toString());
	p->setStdProcessingParameters(ar.read("stdProcessingParameters").value<QVariantMap>());

	QVariantMap std = p->stdProcessingParameters();
	VipOtherPlayerData data;
	for (QVariantMap::iterator it = std.begin(); it != std.end(); ++it)
	{
		if (it.value().userType() == qMetaTypeId<VipOtherPlayerData>())
		{
			data = it.value().value<VipOtherPlayerData>();
			break;
		}
	}

	//added in 2.2.14
	//find the registered processing info (if any)
	/*ar.save();
	QString registered;
	if (ar.content("registered", registered)) {
	if (registered.size()) {
	//find the corresponding info object
	const QList<VipProcessingObject::Info> infos = VipProcessingObject::additionalInfoObjects();
	for (int i = 0; i < infos.size(); ++i){
	if (infos[i].category + infos[i].classname == registered) {
	p->d_data->info = infos[i];
	break;
	}
	}
	}
	}
	else
	ar.restore();*/


	return ar;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<PyProcessing*>();
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();






class PyArrayEditor::PrivateData
{
public:
	VipNDArray array;
	QLabel info;
	QToolButton send;
	QPlainTextEdit editor;
};

PyArrayEditor::PyArrayEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->info.setText("Enter your 2D array. Each column is separated by spaces or tabulations, each row is separated by a new line.");
	d_data->info.setWordWrap(true);
	d_data->send.setAutoRaise(true);
	d_data->send.setToolTip("Click to finish your 2D array");
	d_data->send.setIcon(vipIcon("apply.png"));
	d_data->editor.setMinimumHeight(200);

	QGridLayout *lay = new QGridLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&d_data->info, 0, 0);
	lay->addWidget(&d_data->send, 0, 1);
	lay->addWidget(&d_data->editor, 1, 0, 1, 2);
	setLayout(lay);

	connect(&d_data->send, SIGNAL(clicked(bool)), this, SLOT(finished()));
	connect(&d_data->editor, SIGNAL(textChanged()), this, SLOT(textEntered()));
}

PyArrayEditor::~PyArrayEditor()
{
}

VipNDArray PyArrayEditor::array() const
{
	return d_data->array;
}

void  PyArrayEditor::setText(const QString & text)
{
	//remove any numpy formatting
	QString out = text;
	out.remove("(");
	out.remove(")");
	out.remove("[");
	out.remove("]");
	out.remove(",");
	out.remove("array");
	d_data->editor.setPlainText(out);
	QTextStream str(out.toLatin1());
	str >> d_data->array;
}
void  PyArrayEditor::setArray(const VipNDArray & ar)
{
	QString out;
	QTextStream str(&out, QIODevice::WriteOnly);
	str << ar;
	str.flush();
	d_data->editor.setPlainText(out);
	d_data->array = ar;
}

void PyArrayEditor::textEntered()
{
	d_data->array = VipNDArray();

	QString text = d_data->editor.toPlainText();
	text.replace("\t", " ");
	QStringList lines = text.split("\n");
	if (lines.size())
	{
		int columns = lines.first().split(" ", QString::SkipEmptyParts).size();
		bool ok = true;
		for (int i = 1; i < lines.size(); ++i)
		{
			if (lines[i].split(" ", QString::SkipEmptyParts).size() != columns)
			{
				if (lines[i].count('\n') + lines[i].count('\t') + lines[i].count(' ') != lines[i].size())
				{
					ok = false;
					break;
				}
			}
		}

		if (ok)
		{
			QTextStream str(text.toLatin1());
			str >> d_data->array;
			if (!d_data->array.isEmpty())
			{
				d_data->send.setIcon(vipIcon("apply_green.png"));
				return;
			}
		}
	}

	d_data->send.setIcon(vipIcon("apply_red.png"));
}

void PyArrayEditor::finished()
{
	if (!d_data->array.isEmpty())
		Q_EMIT changed();
}




class PyDataEditor::PrivateData
{
public:
	VipOtherPlayerData data;
	QRadioButton * editArray;
	QRadioButton * editPlayer;

	QCheckBox * shouldResizeArray;

	PyArrayEditor * editor;
	VipOtherPlayerDataEditor * player;

	QWidget *lineBefore;
	QWidget *lineAfter;
};

PyDataEditor::PyDataEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->editArray = new QRadioButton("Create a 1D/2D array");
	d_data->editArray->setToolTip("<b>Manually create a 1D/2D array</b><br>This is especially usefull for convolution functions.");
	d_data->editPlayer = new QRadioButton("Take the data from another player");
	d_data->editPlayer->setToolTip("<b>Selecte a data (image, curve,...) from another player</b>");
	d_data->shouldResizeArray = new QCheckBox("Resize array to the current data shape");
	d_data->shouldResizeArray->setToolTip("This usefull if you apply a processing/filter that only works on 2 arrays having the same shape.\n"
		"Selecting this option ensures you that given image/curve will be resized to the right dimension.");
	d_data->editor = new PyArrayEditor();
	d_data->player = new VipOtherPlayerDataEditor();
	d_data->lineBefore = VipLineWidget::createHLine();
	d_data->lineAfter = VipLineWidget::createHLine();

	QVBoxLayout * lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->lineBefore);
	lay->addWidget(d_data->editArray);
	lay->addWidget(d_data->editPlayer);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(d_data->shouldResizeArray);
	lay->addWidget(d_data->editor);
	lay->addWidget(d_data->player);
	lay->addWidget(d_data->lineAfter);
	//lay->setSizeConstraint(QLayout::SetFixedSize);
	setLayout(lay);

	d_data->editArray->setChecked(true);
	d_data->player->setVisible(false);

	connect(d_data->editArray, SIGNAL(clicked(bool)), d_data->editor, SLOT(setVisible(bool)));
	connect(d_data->editArray, SIGNAL(clicked(bool)), d_data->player, SLOT(setHidden(bool)));
	connect(d_data->editPlayer, SIGNAL(clicked(bool)), d_data->player, SLOT(setVisible(bool)));
	connect(d_data->editPlayer, SIGNAL(clicked(bool)), d_data->editor, SLOT(setHidden(bool)));

	connect(d_data->shouldResizeArray, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(d_data->editor, SIGNAL(changed()), this, SLOT(emitChanged()));
	connect(d_data->player, SIGNAL(valueChanged(const QVariant &)), this, SLOT(emitChanged()));
}

PyDataEditor::~PyDataEditor()
{
}

VipOtherPlayerData PyDataEditor::value() const
{
	VipOtherPlayerData res;
	if (d_data->editArray->isChecked())
		res = VipOtherPlayerData(VipAnyData(QVariant::fromValue(d_data->editor->array()), 0));
	else
		res = d_data->player->value();
	res.setShouldResizeArray(d_data->shouldResizeArray->isChecked());
	return res;
}

void PyDataEditor::setValue(const VipOtherPlayerData & data)
{
	VipNDArray ar = data.staticData().value<VipNDArray>();

	d_data->editor->blockSignals(true);
	d_data->player->blockSignals(true);
	d_data->shouldResizeArray->blockSignals(true);

	if (!ar.isEmpty() && data.otherPlayerId() < 1)
	{
		d_data->editArray->setChecked(true);
		d_data->editor->setArray(ar);
	}
	else
	{
		d_data->editPlayer->setChecked(true);
		d_data->player->setValue(data);
	}
	d_data->player->setVisible(d_data->editPlayer->isChecked());
	d_data->editor->setVisible(!d_data->editPlayer->isChecked());

	d_data->shouldResizeArray->setChecked(data.shouldResizeArray());

	d_data->editor->blockSignals(false);
	d_data->player->blockSignals(false);
	d_data->shouldResizeArray->blockSignals(false);
}

void PyDataEditor::displayVLines(bool before, bool after)
{
	d_data->lineBefore->setVisible(before);
	d_data->lineAfter->setVisible(after);
}






class PyParametersEditor::PrivateData
{
public:
	QList<QWidget*> editors;
	QList<PyProcessing::Parameter> params;
	QList<QVariant> previous;
	QPointer<PyProcessing> processing;
};

PyParametersEditor::PyParametersEditor(PyProcessing * p)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->processing = p;
	d_data->params = p->extractStdProcessingParameters();
	QVariantMap args = p->stdProcessingParameters();

	if (d_data->params.size())
	{
		QGridLayout * lay = new QGridLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		//lay->setSizeConstraint(QLayout::SetMinimumSize);

		for (int i = 0; i < d_data->params.size(); ++i)
		{
			PyProcessing::Parameter pr = d_data->params[i];
			QVariant value = args[pr.name];

			if (pr.type == "int") {
				QSpinBox * box = new QSpinBox();
				if (pr.min.size()) box->setMinimum(pr.min.toInt());
				else box->setMinimum(-INT_MAX);
				if (pr.max.size())  box->setMaximum(pr.max.toInt());
				else box->setMaximum(INT_MAX);
				if (pr.step.size()) box->setSingleStep(pr.step.toInt());
				box->setValue(pr.defaultValue.toInt());
				if (value.userType()) box->setValue(value.toInt());
				connect(box, SIGNAL(valueChanged(int)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(box, i, 1);
				d_data->editors << box;
			}
			else if (pr.type == "float") {
				VipDoubleSpinBox * box = new VipDoubleSpinBox();
				if (pr.min.size()) box->setMinimum(pr.min.toDouble());
				else box->setMinimum(-FLT_MAX);
				if (pr.max.size()) box->setMaximum(pr.max.toDouble());
				else box->setMaximum(FLT_MAX);
				if (pr.step.size()) box->setSingleStep(pr.step.toDouble());
				else box->setSingleStep(0);
				box->setDecimals(6);
				box->setValue(pr.defaultValue.toDouble());
				if (value.userType()) box->setValue(value.toDouble());
				connect(box, SIGNAL(valueChanged(double)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(box, i, 1);
				d_data->editors << box;
			}
			else if (pr.type == "bool") {
				QCheckBox * box = new QCheckBox(vipSplitClassname(pr.name));
				box->setChecked(pr.defaultValue.toInt());
				if (value.userType()) box->setChecked(value.toInt());
				connect(box, SIGNAL(clicked(bool)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(box, i, 0, 1, 2);
				d_data->editors << box;
			}
			else if (pr.type == "str" && pr.enumValues.size()) {
				VipComboBox * box = new VipComboBox();
				box->addItems(pr.enumValues);
				box->setCurrentText(pr.defaultValue);
				if (value.userType()) box->setCurrentText(value.toString());
				connect(box, SIGNAL(valueChanged(const QString &)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(box, i, 1);
				d_data->editors << box;
			}
			else if (pr.type == "other") {
				PyDataEditor * ed = new PyDataEditor();
				if (value.userType()) ed->setValue(value.value<VipOtherPlayerData>());
				ed->displayVLines(i > 0, i < d_data->params.size() - 1);
				connect(ed, SIGNAL(changed()), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(ed, i, 0, 1, 2);
				d_data->editors << ed;
			}
			else {
				VipLineEdit * line = new VipLineEdit();
				line->setText(pr.defaultValue);
				if (value.userType()) line->setText(value.value<QString>());
				connect(line, SIGNAL(valueChanged(const QString &)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(line, i, 1);
				d_data->editors << line;
			}

			d_data->previous << QVariant();
		}

		//lay->setSizeConstraint(QLayout::SetFixedSize);
		setLayout(lay);
	}

}
PyParametersEditor::~PyParametersEditor()
{
}

void PyParametersEditor::updateProcessing()
{
	if (!d_data->processing)
		return;

	QVariantMap map;
	for (int i = 0; i < d_data->params.size(); ++i)
	{
		QString name = d_data->params[i].name;
		QWidget * ed = d_data->editors[i];
		QVariant value;

		if (QSpinBox * box = qobject_cast<QSpinBox*>(ed))
			value = box->value();
		else if (QDoubleSpinBox * box = qobject_cast<QDoubleSpinBox*>(ed))
			value = box->value();
		else if (QCheckBox * box = qobject_cast<QCheckBox*>(ed))
			value = int(box->isChecked());
		else if (VipComboBox * box = qobject_cast<VipComboBox*>(ed))
			value = "'" + box->currentText() + "'";
		else if (VipLineEdit * line = qobject_cast<VipLineEdit*>(ed))
			value = "'" + line->text() + "'";
		else if (PyDataEditor * other = qobject_cast<PyDataEditor*>(ed))
			value = QVariant::fromValue(other->value());
		map[name] = value;
	}

	d_data->processing->setStdProcessingParameters(map);
	d_data->processing->reload();

	Q_EMIT changed();
}






class PyProcessingEditor::PrivateData
{
public:
	PyEditor editor;
	QPointer<PyProcessing> proc;
	QLabel maxTime;
	QSpinBox maxTimeEdit;
	QLabel resampleText;
	QComboBox resampleBox;
	PyApplyToolBar apply;
	PyParametersEditor * params;
};

PyProcessingEditor::PyProcessingEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->params = nullptr;

	QGridLayout * hlay = new QGridLayout();
	hlay->addWidget(&d_data->maxTime, 0, 0);
	hlay->addWidget(&d_data->maxTimeEdit, 0, 1);
	hlay->addWidget(&d_data->resampleText, 1, 0);
	hlay->addWidget(&d_data->resampleBox, 1, 1);
	hlay->setContentsMargins(0, 0, 0, 0);
	d_data->maxTime.setText("Python script timeout (ms)");
	d_data->maxTimeEdit.setRange(-1, 200000);
	d_data->maxTimeEdit.setValue(5000);
	d_data->maxTimeEdit.setToolTip("Maximum time for the script execution.\n-1 means no maximum time.");
	d_data->resampleText.setText("Resample input signals based on");
	d_data->resampleBox.addItem("union");
	d_data->resampleBox.addItem("intersection");
	d_data->resampleBox.setCurrentIndex(1);

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(&d_data->editor, 1);
	vlay->addWidget(&d_data->apply);
	vlay->setContentsMargins(0, 0, 0, 0);
	setLayout(vlay);

	connect(d_data->apply.applyButton(), SIGNAL(clicked(bool)), this, SLOT(applyRequested()));
	connect(d_data->apply.registerButton(), SIGNAL(clicked(bool)), this, SLOT(registerProcessing()));
	connect(d_data->apply.manageButton(), SIGNAL(clicked(bool)), this, SLOT(manageProcessing()));
	connect(&d_data->maxTimeEdit, SIGNAL(valueChanged(int)), this, SLOT(updatePyProcessing()));
	connect(&d_data->resampleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePyProcessing()));
}

PyProcessingEditor::~PyProcessingEditor()
{
}

PyApplyToolBar * PyProcessingEditor::buttons() const
{
	return &d_data->apply;
}

void PyProcessingEditor::updatePyProcessing()
{
	if (d_data->proc)
	{
		d_data->proc->setMaxExecutionTime(d_data->maxTimeEdit.value());
		if (d_data->resampleBox.currentText() != d_data->proc->propertyAt(0)->value<QString>()) {
			d_data->proc->propertyAt(0)->setData(d_data->resampleBox.currentText());
			d_data->proc->reload();
		}
	}
}


void PyProcessingEditor::setPyProcessing(PyProcessing * proc)
{
	d_data->proc = proc;
	if (proc)
	{
		if (proc->inputCount() > 1 && proc->inputAt(0)->probe().data().userType() == qMetaTypeId<VipPointVector>() && proc->resampleEnabled()) {
			d_data->resampleBox.setVisible(true);
			d_data->resampleText.setVisible(true);
		}
		else {
			d_data->resampleBox.setVisible(false);
			d_data->resampleText.setVisible(false);
		}
		d_data->resampleBox.setCurrentText(proc->propertyAt(0)->data().value<QString>());

		if (d_data->params)
		{
			d_data->params->setAttribute(Qt::WA_DeleteOnClose);
			d_data->params->close();
		}

		QList<PyProcessing::Parameter> params = proc->extractStdProcessingParameters();
		if (params.size())
		{
			d_data->params = new PyParametersEditor(proc);
			this->layout()->addWidget(d_data->params);
			d_data->editor.hide();
			d_data->apply.hide();
		}
		else if (proc->stdPyProcessingFile().isEmpty())
		{
			d_data->editor.show();
			d_data->apply.show();
			if (!d_data->editor.CurrentEditor())
				d_data->editor.NewFile();
			d_data->editor.CurrentEditor()->setPlainText(proc->propertyAt(1)->data().value<QString>());

			/*if (proc->lastError().isNull())
			d_data->editor.SetApplied();
			else if (proc->lastError().traceback == "Uninitialized")
			d_data->editor.SetUninit();
			else
			d_data->editor.SetError();*/
		}
		else
		{
			d_data->editor.hide();
			d_data->apply.hide();
		}


	}
}

void PyProcessingEditor::applyRequested()
{
	if (d_data->proc)
	{
		d_data->proc->setState(PyProcessing::Init);
		d_data->proc->propertyAt(1)->setData(VipAnyData(QString(d_data->editor.CurrentEditor()->toPlainText()), VipInvalidTime));
		d_data->proc->reload();
		d_data->proc->wait();
		/*if(d_data->proc->lastError().isNull())
		d_data->editor.SetApplied();
		else
		d_data->editor.SetError();*/
	}
}

void PyProcessingEditor::uninitRequested()
{
	if (d_data->proc)
	{
		d_data->proc->setState(PyProcessing::Uninit);
		d_data->proc->propertyAt(1)->setData(VipAnyData(QString(d_data->editor.CurrentEditor()->toPlainText()), VipInvalidTime));
		d_data->proc->reload();
		d_data->proc->wait();
		//d_data->editor.SetUninit();
	}
}


void PyProcessingEditor::registerProcessing()
{
	if (!d_data->proc)
		return;

	//register the current processing
	PySignalFusionProcessingManager * m = new PySignalFusionProcessingManager();
	m->setManagerVisible(false);
	m->setCreateNewVisible(true);
	m->setCategory("Python/");
	VipGenericDialog dialog(m, "Register new processing");
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = d_data->proc->registerThisProcessing(m->category(), m->name(), m->description(), false);
		if (!ret)
			QMessageBox::warning(nullptr, "Operation failure", "Failed to register this processing.\nPlease make sure you entered a valid name and category.");
		else {
			//make sure to update all processing menu
			vipGetMainWindow()->displayArea()->resetItemSelection();
		}
	}
}

void PyProcessingEditor::manageProcessing()
{
	PyRegisterProcessing::openProcessingManager();
}


static PyProcessingEditor * editPyProcessing(PyProcessing * proc)
{
	PyProcessingEditor * editor = new PyProcessingEditor();
	editor->setPyProcessing(proc);
	return editor;
}

int registerEditPyProcessing()
{
	vipFDObjectEditor().append<QWidget*(PyProcessing*)>(editPyProcessing);
	return 0;
}
static int _registerEditPyProcessing = registerEditPyProcessing();
