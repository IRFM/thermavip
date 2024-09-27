
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
#include "PyRegisterProcessing.h"

#include <QPointer>
#include <qcoreapplication.h>

static int registerPyProcessingPtr()
{
	qRegisterMetaType<PyProcessingPtr>();
	return 0;
}
static int _registerPyProcessingPtr = registerPyProcessingPtr();

void PyBaseProcessing::newError(const VipErrorData& error)
{
	VipBaseDataFusion::newError(error);
	if (QObject* interp = VipPyInterpreter::instance()->mainInterpreter())
		QMetaObject::invokeMethod(interp, "showAndRaise", Qt::QueuedConnection);
}

class PyFunctionProcessing::PrivateData
{
public:
	PyObject* function;
	VipPyError lastError;
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
		VipGILLocker lock;
		Py_DECREF(d_data->function);
	}
}

bool PyFunctionProcessing::acceptInput(int, const QVariant& v) const
{
	if (v.userType() == qMetaTypeId<VipNDArray>())
		return true;
	else if (v.userType() == qMetaTypeId<VipPointVector>())
		return true;
	else if (v.userType() == qMetaTypeId<VipComplexPointVector>())
		return true;
	return v.canConvert<QString>();
}

void PyFunctionProcessing::setFunction(void* pyobject)
{
	VipGILLocker lock;
	if (d_data->function)
		Py_DECREF(d_data->function);
	d_data->function = (PyObject*)pyobject;
	if (d_data->function)
		Py_INCREF(d_data->function);
}
void* PyFunctionProcessing::function() const
{
	return d_data->function;
}

void PyFunctionProcessing::mergeData(int, int)
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

	// get the current data units and names
	QStringList units;
	QStringList names;
	for (int i = 0; i < in.size(); ++i) {
		units << in[i].xUnit() << in[i].yUnit() << in[i].zUnit();
		names << in[i].name();
	}

	// send function
	{
		VipGILLocker lock;
		PyObject* __main__ = PyImport_ImportModule("__main__");
		PyObject* globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);
		int r = PyDict_SetItemString(globals, "fun", d_data->function);
		if (r != 0) {
			d_data->lastError = VipPyError(compute_error_t{});
			if (!d_data->lastError.isNull()) {
				vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
				setError("cannot send objects to the Python interpreter", VipProcessingObject::WrongInput);
				outputAt(0)->setData(out);
				return;
			}
		}
	}

	// find output player
	// QList<VipAbstractPlayer*> players = VipAbstractPlayer::findOutputPlayers(this);
	// int player = players.size() ? VipUniqueId::id(VipBaseDragWidget::fromChild(players.first())) : 0;

	VipPyCommandList cmds;
	cmds << vipCSendObject("names", names) << vipCSendObject("units", units) << vipCSendObject("input_count", in.size()) << vipCSendObject("this", in[0].data())
	     << vipCSendObject("attributes", in[0].attributes()) << vipCSendObject("stylesheet", QString()) << vipCExecCode("this = fun(this)", "code") << vipCRetrieveObject("names")
	     << vipCRetrieveObject("units") << vipCRetrieveObject("this") << vipCRetrieveObject("attributes") << vipCRetrieveObject("stylesheet");
	QVariant ret = VipPyInterpreter::instance()->sendCommands(cmds).value(d_data->maxExecutionTime);

	d_data->lastError = ret.value<VipPyError>();
	if (!d_data->lastError.isNull()) {
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError(d_data->lastError.traceback);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	const QVariantMap out_vars = ret.value<QVariantMap>();
	QVariant value = out_vars["this"];

	// output conversion
	// specific cases: VipNDArray to VipPointVector and VipNDArray to VipComplexPointVector
	bool input_is_vector = (in[0].data().userType() == qMetaTypeId<VipPointVector>() || in[0].data().userType() == qMetaTypeId<VipComplexPointVector>());
	if (input_is_vector && value.userType() == qMetaTypeId<VipNDArray>()) {
		const VipNDArray ar = value.value<VipNDArray>();
		if (ar.shapeCount() == 2 && ar.shape(0) == 2) {
			// convert back to VipPointVector or VipComplexPointVector
			if (ar.dataType() == qMetaTypeId<complex_d>() || ar.dataType() == qMetaTypeId<complex_f>())
				value = QVariant::fromValue(value.value<VipComplexPointVector>());
			else
				value = QVariant::fromValue(value.value<VipPointVector>());
		}
	}

	out.setData(value);

	// grab the units
	value = out_vars["units"];
	QVariantList vunits = value.value<QVariantList>();
	if (vunits.size() > 2) {
		out.setXUnit(vunits[0].toString());
		out.setYUnit(vunits[1].toString());
		out.setZUnit(vunits[2].toString());
	}

	QString stylesheet = out_vars["stylesheet"].toString();
	out.setAttribute("stylesheet", stylesheet);
	outputAt(0)->setData(out);
}

class PyProcessing::PrivateData
{
public:
	PrivateData()
	  : maxExecutionTime(5000)
	  , minDims(-1)
	  , maxDims(-1)
	  , need_resampling(true)
	  , need_same_type(true)
	  , need_same_sub_type(false)
	  , minInputCount(1)
	  , maxInputCount(10)
	{
	}
	int maxExecutionTime;
	VipPyError lastError;
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

PyProcessing::PyProcessing(QObject* parent)
  : PyBaseProcessing(parent)
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

PyProcessing::~PyProcessing() {}

void PyProcessing::setMaxExecutionTime(int milli)
{
	d_data->maxExecutionTime = milli;
	emitProcessingChanged();
}

int PyProcessing::maxExecutionTime()
{
	return d_data->maxExecutionTime;
}

VipPyError PyProcessing::lastError() const
{
	return d_data->lastError;
}

QVariant PyProcessing::initializeProcessing(const QVariant& v)
{
	d_data->initialize = v;
	if (PyProcessingPtr ptr = v.value<PyProcessingPtr>()) {
		// initialize from a PyProcessingPtr
		topLevelInputAt(0)->toMultiInput()->resize(1);
		propertyName("code")->setData(ptr->propertyName("code")->data());
		propertyName("Time_range")->setData(ptr->propertyName("Time_range")->data());
		return QVariant(true);
	}
	else {
		// initialize the processing based on the Python class name, without the prefix 'Thermavip'
		return QVariant::fromValue(setStdPyProcessingFile(d_data->initialize.toString()));
	}
}

QList<VipProcessingObject*> PyProcessing::directSources() const
{
	QList<VipProcessingObject*> res = VipProcessingObject::directSources();
	for (auto it = d_data->stdProcessingParameters.begin(); it != d_data->stdProcessingParameters.end(); ++it) {
		QVariant value = it.value();
		if (value.userType() == qMetaTypeId<VipOtherPlayerData>()) {
			if (VipProcessingObject* obj = value.value<VipOtherPlayerData>().processing())
				if (obj != this && res.indexOf(obj) < 0)
					res.append(obj);
		}
	}
	return res;
}

void PyProcessing::setStdProcessingParameters(const QVariantMap& args, const VipNDArray& ar, VipPyCommandList* cmds)
{
	// for a standard processing (Py file in vipGetPythonDirectory()), set the processing class parameters.
	// this will call the processing memeber 'setParameters'.

	if (d_data->std_proc_name.isEmpty())
		return;

	d_data->stdProcessingParameters = args;
	d_data->extractParameters.clear();

	// build the dict of parameters
	QStringList parameters;
	for (QVariantMap::iterator it = d_data->stdProcessingParameters.begin(); it != d_data->stdProcessingParameters.end(); ++it) {
		// for 'other' type, send the object before in the 'other' variable
		if (it.value().userType() == qMetaTypeId<VipOtherPlayerData>()) {
			VipOtherPlayerData other = it.value().value<VipOtherPlayerData>();
			QVariant value = other.data().data();

			// if the data is an array, resize it based on ar
			const VipNDArray d = value.value<VipNDArray>();
			if (!d.isEmpty() && d.shapeCount() == ar.shapeCount() && (other.isDynamic() || other.shouldResizeArray())) {
				VipNDArray resized = d.resize(ar.shape());
				value = QVariant::fromValue(resized);
			}

			// send 'other' that will be passed in **kwargs
			if (!cmds)
				VipPyInterpreter::instance()->sendObject("other", value).wait(5000);
			else
				cmds->push_back(vipCSendObject("other", value));
			parameters << it.key() + "= other";
		}
		else
			parameters << it.key() + "=" + it.value().toString();
	}

	// set the parameters if required
	if (parameters.size()) {
		QString classname = "Thermavip" + d_data->std_proc_name;
		QString id = QString::number(qint64(this));
		QString code = "pr = procs[" + id +
			       "]\n"
			       "pr.setParameters(" +
			       parameters.join(",") + ")\n";

		if (!cmds)
			VipPyInterpreter::instance()->execCode(code).wait(5000);
		else
			cmds->push_back(vipCExecCode(code, "code"));
	}
}

QVariantMap PyProcessing::stdProcessingParameters() const
{
	return d_data->stdProcessingParameters;
}

QList<PyProcessing::Parameter> PyProcessing::extractStdProcessingParameters()
{
	// extract the processing class parameters defined with the memeber 'parameters()'
	// See "ThermavipPyProcessing' Python class for more details
	if (d_data->extractParameters.size())
		return d_data->extractParameters;
	else if (d_data->std_proc_name.isEmpty()) {
		return QList<PyProcessing::Parameter>();
	}

	QString id = QString::number(qint64(this));
	QString code = "try: pr = procs[" + id +
		       "]\n"
		       "except: pr= Thermavip" +
		       d_data->std_proc_name +
		       "()\n"
		       "tmp = pr.parameters()";

	VipPyError err = VipPyInterpreter::instance()->execCode(code).value(5000).value<VipPyError>();
	if (!err.isNull()) {
		vip_debug("err: %s\n", err.traceback.toLatin1().data());
		return QList<Parameter>();
	}
	QVariant v = VipPyInterpreter::instance()->retrieveObject("tmp").value(5000);
	if (!v.value<VipPyError>().isNull()) {
		vip_debug("err: %s\n", v.value<VipPyError>().traceback.toLatin1().data());
		return QList<Parameter>();
	}
	QList<Parameter> res;
	QVariantMap map = v.value<QVariantMap>();
	for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it) {
		QString name = it.key();
		QVariant v = it.value();
		QVariantList lst = v.value<QVariantList>();
		if (lst.size() == 0)
			lst << v;

		if (lst.size()) {
			Parameter p;
			p.name = name;
			p.type = lst[0].toString();
			if (lst.size() > 1)
				p.defaultValue = lst[1].toString();
			if (p.type == "int" || p.type == "bool" || p.type == "float") {
				if (lst.size() > 2)
					p.min = lst[2].toString();
				if (lst.size() > 3)
					p.max = lst[3].toString();
				if (lst.size() > 4)
					p.step = lst[4].toString();
				res << p;
			}
			else if (p.type == "str") {
				for (int i = 2; i < lst.size(); ++i)
					p.enumValues.append(lst[i].toString());
				res << p;
			}
			else if (p.type == "other") {
				res << p;
			}
		}
	}

	return d_data->extractParameters = res;
}

bool PyProcessing::acceptInput(int, const QVariant& v) const
{
	if (d_data->minDims < 0)
		return true;

	if (v.userType() == qMetaTypeId<VipNDArray>()) {
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
	if (d_data->std_proc_name.size())
		return d_data->info.displayHint;
	return InputTransform;
}

PyProcessing::Info PyProcessing::info() const
{
	if (d_data->info.metatype == 0) {
		if (d_data->std_proc_name.size()) {
			const QList<VipProcessingObject::Info> infos = additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i)
				if (infos[i].classname == d_data->std_proc_name) {
					const_cast<PyProcessing::Info&>(d_data->info) = infos[i];
					break;
				}
		}
		else if (PyProcessingPtr ptr = d_data->initialize.value<PyProcessingPtr>()) {
			const QList<VipProcessingObject::Info> infos = additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i)
				if (infos[i].init.value<PyProcessingPtr>() == ptr) {
					const_cast<PyProcessing::Info&>(d_data->info) = infos[i];
					break;
				}
		}
		else
			const_cast<PyProcessing::Info&>(d_data->info) = VipProcessingObject::info();
	}
	return d_data->info;
}

bool PyProcessing::registerThisProcessing(const QString& category, const QString& name, const QString& description, bool overwrite)
{
	if (name.isEmpty())
		return false;

	VipProcessingObject::Info info = this->info();
	info.classname = name;
	info.category = category.split("/", QString::SkipEmptyParts).join("/"); // remove empty strings
	info.description = description;
	info.displayHint = VipProcessingObject::InputTransform;

	PyProcessingPtr init(new PyProcessing());
	init->topLevelInputAt(0)->toMultiInput()->resize(1);
	init->propertyName("code")->setData(this->propertyName("code")->data());
	init->propertyName("Time_range")->setData(this->propertyName("Time_range")->data());
	info.init = QVariant::fromValue(init);

	// check if already registerd
	if (!overwrite) {
		QList<VipProcessingObject::Info> infos = additionalInfoObjects();
		for (int i = 0; i < infos.size(); ++i) {
			if (infos[i].classname == name && infos[i].category == info.category)
				return false;
		}
	}
	// register processing info
	registerAdditionalInfoObject(info);

	return PyRegisterProcessing::saveCustomProcessings();
}

bool PyProcessing::setStdPyProcessingFile(const QString& proc_name)
{
	// check if the processing is registerd
	QList<VipProcessingObject::Info> infos = VipProcessingObject::additionalInfoObjects();
	bool found = false;
	for (int i = 0; i < infos.size(); ++i)
		if (infos[i].classname == proc_name) {
			found = true;
			break;
		}
	if (!found)
		return false;

	// update the procs dict
	QString id = QString::number(qint64(this));
	QString exec = "#create dictionary of processings\n"
		       "try :\n"
		       " _procs = dict(procs)\n"
		       "except :\n"
		       " procs = dict()\n"
		       "pr = procs[" +
		       id + "] = Thermavip" + proc_name +
		       "()\n"
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

	VipPyError err = VipPyInterpreter::instance()->execCode(exec).value(5000).value<VipPyError>();
	if (!err.isNull()) {
		vip_debug("err: %s\n", err.traceback.toLatin1().data());
	}

	// retrieve the valid dimensions
	QVariant v = VipPyInterpreter::instance()->retrieveObject("dims").value(2000);
	QVariantList dims = v.value<QVariantList>();
	if (dims.size() == 2) {
		d_data->minDims = dims[0].toInt();
		d_data->maxDims = dims[1].toInt();
	}

	// retrieve other class parameters
	d_data->need_resampling = VipPyInterpreter::instance()->retrieveObject("need_resampling").value(2000).toBool();
	d_data->need_same_type = VipPyInterpreter::instance()->retrieveObject("need_same_type").value(2000).toBool();
	d_data->need_same_sub_type = VipPyInterpreter::instance()->retrieveObject("need_same_sub_type").value(2000).toBool();
	QVariantList lst = VipPyInterpreter::instance()->retrieveObject("input_count").value(2000).value<QVariantList>();
	if (lst.size() == 2) {
		d_data->minInputCount = lst[0].toInt();
		d_data->maxInputCount = lst[1].toInt();
		/*if(d_data->minInputCount > d_data->maxInputCount) std::swap(d_data->minInputCount,d_data->maxInputCount);
		if(d_data->minInputCount <2)d_data->minInputCount = 2;
		if(d_data->maxInputCount <2)d_data->maxInputCount = 2;*/
		VipMultiInput* input = topLevelInputAt(0)->toMultiInput();
		input->setMinSize(d_data->minInputCount);
		input->setMaxSize(d_data->maxInputCount);
		input->resize(d_data->minInputCount);
		setWorkOnSameObjectType(d_data->need_same_type);
		setSameDataType(d_data->need_same_sub_type);
		setResampleEnabled(d_data->need_resampling, true);
	}
	else
		topLevelInputAt(0)->toMultiInput()->resize(1);

	// build the code that will be executed in the apply() member
	QString classname = "Thermavip" + proc_name;
	QString code =

	  "#retrieve this processing\n"
	  "try :\n"
	  " pr = procs[" +
	  id +
	  "]\n"
	  "except :\n"
	  " pr = procs[" +
	  id + "] = " + classname +
	  "()\n"
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

void PyProcessing::resetProcessing() {}

void PyProcessing::mergeData(int, int)
{
	VipPyCommandList cmds;

	// initialize the standard processing (if any) based on 'ThermavipPyProcessing' class
	if (d_data->initialize.toString().size() && d_data->std_proc_name != d_data->initialize.toString()) {
		setStdPyProcessingFile(d_data->initialize.toString());
	}

	if (d_data->std_proc_name.size()) {
		// Resend the Python processing class parameters if:
		// - One of the parameter is a dynamic VipOtherPlayerData
		// - One of the parameter is a VipOtherPlayerData that needs resizing
		// - The current code is different that the last one executed
		bool reset_parameters = (d_data->lastExecutedCode.isEmpty() || d_data->lastExecutedCode != propertyAt(1)->value<QString>());
		if (!reset_parameters) {
			for (QVariantMap::iterator it = d_data->stdProcessingParameters.begin(); it != d_data->stdProcessingParameters.end(); ++it) {
				if (it.value().userType() == qMetaTypeId<VipOtherPlayerData>()) {
					if (it.value().value<VipOtherPlayerData>().isDynamic()) {
						reset_parameters = true;
						break;
					}
					else if (it.value().value<VipOtherPlayerData>().shouldResizeArray()) {
						reset_parameters = true;
						break;
					}
				}
			}
		}

		if (reset_parameters) {
			// reset the parameters
			setStdProcessingParameters(d_data->stdProcessingParameters, inputs()[0].value<VipNDArray>(), &cmds);
		}
	}

	const QVector<VipAnyData> in = inputs();
	if (in.isEmpty()) {
		setError("no valid input", WrongInputNumber);
		return;
	}

	VipAnyData out = create(in.first().data());

	// get the current data units and names
	QStringList units;
	QStringList names;
	VipAnyData attrs;
	for (int i = 0; i < in.size(); ++i) {
		units << in[i].xUnit() << in[i].yUnit() << in[i].zUnit();
		names << in[i].name();
		attrs.mergeAttributes(in[i].attributes());
	}

	// find output player
	// QList<VipAbstractPlayer*> players = VipAbstractPlayer::findOutputPlayers(this);
	// int player = players.size() ? VipUniqueId::id(VipBaseDragWidget::fromChild(players.first())) : 0;

	cmds << vipCSendObject("units", units);
	cmds << vipCSendObject("names", names);
	cmds << vipCSendObject("time", out.time());
	// cmds << vipCSendObject("player", player);
	cmds << vipCSendObject("input_count", in.size());
	cmds << vipCSendObject("stylesheet", QString());
	cmds << vipCSendObject("name", names.first());
	cmds << vipCSendObject("attributes", attrs.attributes());

	if (in.size() == 1)
		cmds << vipCSendObject("this", in[0].data());
	else {
		QVariantList lst;
		for (int i = 0; i < in.size(); ++i)
			lst.append(in[i].data());
		cmds << vipCSendObject("this", lst);
	}

	// execute actual processing code
	QString code = propertyAt(1)->data().value<QString>();
	cmds << vipCExecCode(code, "code");
	cmds << vipCRetrieveObject("this");
	cmds << vipCRetrieveObject("units");
	cmds << vipCRetrieveObject("stylesheet");
	cmds << vipCRetrieveObject("name");
	cmds << vipCRetrieveObject("attributes");

	QVariant res = VipPyInterpreter::instance()->sendCommands(cmds).value(d_data->maxExecutionTime);
	d_data->lastError = res.value<VipPyError>();
	if (!d_data->lastError.isNull()) {
		vip_debug("err: %s\n", d_data->lastError.traceback.toLatin1().data());
		setError(d_data->lastError.traceback);
		if (in.size() == 1)
			outputAt(0)->setData(out);
		return;
	}

	QVariantMap map = res.value<QVariantMap>();
	QVariant value = map["this"];

	// output conversion
	// specific cases: VipNDArray to VipPointVector and VipNDArray to VipComplexPointVector
	bool input_is_vector = (in[0].data().userType() == qMetaTypeId<VipPointVector>() || in[0].data().userType() == qMetaTypeId<VipComplexPointVector>());
	if (input_is_vector && value.userType() == qMetaTypeId<VipNDArray>()) {
		const VipNDArray ar = value.value<VipNDArray>();
		if (ar.shapeCount() == 2 && ar.shape(0) == 2) {
			// convert back to VipPointVector or VipComplexPointVector
			if (ar.dataType() == qMetaTypeId<complex_d>() || ar.dataType() == qMetaTypeId<complex_f>())
				value = QVariant::fromValue(value.value<VipComplexPointVector>());
			else
				value = QVariant::fromValue(value.value<VipPointVector>());
		}
	}

	out.setData(value);
	QVariantMap attributes = map["attributes"].value<QVariantMap>();
	out.mergeAttributes(attributes);

	// grab the units
	value = map["units"];
	QVariantList vunits = value.value<QVariantList>();
	if (vunits.size() > 2) {
		out.setXUnit(vunits[0].toString());
		out.setYUnit(vunits[1].toString());
		out.setZUnit(vunits[2].toString());
	}

	// grab the style sheet
	QString stylesheet = map["stylesheet"].toString();
	out.setAttribute("stylesheet", stylesheet);

	// grab the name for multi inputs only
	if (in.size() > 1) {
		QString name = map["name"].toString();
		out.setName(name);
	}

	outputAt(0)->setData(out);
	d_data->lastExecutedCode = code;
}

#include "VipArchive.h"
VipArchive& operator<<(VipArchive& ar, PyProcessing* p)
{
	ar.content("maxExecutionTime", p->maxExecutionTime());
	ar.content("stdPyProcessingFile", p->stdPyProcessingFile());
	ar.content("stdProcessingParameters", p->stdProcessingParameters());

	// added in 2.2.14
	// we need to save the init parameter to retrieve it at reloading
	/*VipProcessingObject::Info info = p->info();
	if (PyProcessingPtr ptr = info.init.value<PyProcessingPtr>())
	ar.content("registered", info.category + info.classname);
	else
	ar.content("registered", QString());*/
	return ar;
}

VipArchive& operator>>(VipArchive& ar, PyProcessing* p)
{
	p->setMaxExecutionTime(ar.read("maxExecutionTime").toInt());
	p->setStdPyProcessingFile(ar.read("stdPyProcessingFile").toString());
	p->setStdProcessingParameters(ar.read("stdProcessingParameters").value<QVariantMap>());

	QVariantMap std = p->stdProcessingParameters();
	VipOtherPlayerData data;
	for (QVariantMap::iterator it = std.begin(); it != std.end(); ++it) {
		if (it.value().userType() == qMetaTypeId<VipOtherPlayerData>()) {
			data = it.value().value<VipOtherPlayerData>();
			break;
		}
	}

	// added in 2.2.14
	// find the registered processing info (if any)
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
