#include "PySignalFusionProcessing.h"
#include "PyRegisterProcessing.h"
#include "PyProcessing.h"
#include "PyOperation.h"

#include <set>

/*static QByteArray formatName(const QByteArray & name)
{
	QByteArray res(name.size(),0);
	for (int i = 0; i < name.size(); ++i) {
		char c = name[i];
		if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
			res[i] = c;
		else
			res[i] = '_';
	}
	if (res[0] >= '0' && res[0] <= '9')
		res = "_" + res;
	return res;
}*/

static int registerPySignalFusionProcessingPtr()
{
	qRegisterMetaType<PySignalFusionProcessingPtr>();
	return 0;
}
static int _registerPySignalFusionProcessingPtr = registerPySignalFusionProcessingPtr();

static QRegExp xreg[50];
static QRegExp yreg[50];
static QRegExp ureg[50];
static QRegExp uxreg[50];
static QRegExp treg[50];

static void findXYmatch(const QString& algo,
			const QString& title,
			const QString& unit,
			const QString& xunit,
			int count,
			std::set<int>& x,
			std::set<int>& y,
			std::set<int>& t,
			std::set<int>& u,
			std::set<int>& ux,
			std::set<int>& merged)
{
	if (xreg[0].isEmpty()) {
		// initialize regular expressions
		for (int i = 0; i < 50; ++i) {
			xreg[i].setPattern("\\bx" + QString::number(i) + "\\b");
			xreg[i].setPatternSyntax(QRegExp::RegExp);
			yreg[i].setPattern("\\by" + QString::number(i) + "\\b");
			yreg[i].setPatternSyntax(QRegExp::RegExp);
			ureg[i].setPattern("\\bu" + QString::number(i) + "\\b");
			ureg[i].setPatternSyntax(QRegExp::RegExp);
			uxreg[i].setPattern("\\bu" + QString::number(i) + "\\b");
			uxreg[i].setPatternSyntax(QRegExp::RegExp);
			treg[i].setPattern("\\bt" + QString::number(i) + "\\b");
			treg[i].setPatternSyntax(QRegExp::RegExp);
		}
	}

	for (int i = 0; i < count; ++i) {
		int xi = xreg[i].indexIn(algo);
		int yi = yreg[i].indexIn(algo);
		int ti = treg[i].indexIn(title);
		int ui = ureg[i].indexIn(unit);
		int uxi = uxreg[i].indexIn(xunit);
		if (xi >= 0)
			x.insert(i);
		if (yi >= 0)
			y.insert(i);
		if (ti >= 0)
			t.insert(i);
		if (ui >= 0)
			u.insert(i);
		if (uxi >= 0)
			ux.insert(i);
		if (xi >= 0 || yi >= 0 || ti >= 0 || ui >= 0)
			merged.insert(i);
	}
}

PySignalFusionProcessing::PySignalFusionProcessing(QObject* parent)
  : PyBaseProcessing(parent)
{
	// prepare inputs/properties/output
	topLevelInputAt(0)->toMultiInput()->resize(2);
	topLevelInputAt(0)->toMultiInput()->setMinSize(2);
	topLevelInputAt(0)->toMultiInput()->setMaxSize(20);
	propertyName("x_algo")->setData(QString());
	propertyName("y_algo")->setData(QString());
	propertyName("output_title")->setData(QString());
	propertyName("output_unit")->setData(QString());
	propertyName("output_x_unit")->setData(QString());
	outputAt(0)->setData(QVariant::fromValue(VipPointVector()));

	setWorkOnSameObjectType(true);
	setResampleEnabled(true);
}

QVariant PySignalFusionProcessing::initializeProcessing(const QVariant& v)
{
	if (PySignalFusionProcessingPtr ptr = v.value<PySignalFusionProcessingPtr>()) {
		topLevelInputAt(0)->toMultiInput()->resize(ptr->topLevelInputAt(0)->toMultiInput()->count());
		topLevelInputAt(0)->toMultiInput()->setMinSize(ptr->topLevelInputAt(0)->toMultiInput()->count());
		topLevelInputAt(0)->toMultiInput()->setMaxSize(ptr->topLevelInputAt(0)->toMultiInput()->count());

		if (ptr->propertyName("x_algo"))
			propertyName("x_algo")->setData(ptr->propertyName("x_algo")->data());
		if (ptr->propertyName("y_algo"))
			propertyName("y_algo")->setData(ptr->propertyName("y_algo")->data());
		if (ptr->propertyName("output_title"))
			propertyName("output_title")->setData(ptr->propertyName("output_title")->data());
		if (ptr->propertyName("output_unit"))
			propertyName("output_unit")->setData(ptr->propertyName("output_unit")->data());
		if (ptr->propertyName("output_x_unit"))
			propertyName("output_x_unit")->setData(ptr->propertyName("output_x_unit")->data());
		if (ptr->propertyName("Time_range"))
			propertyName("Time_range")->setData(ptr->propertyName("Time_range")->data());
		return QVariant(true);
	}
	return QVariant(false);
}

bool PySignalFusionProcessing::registerThisProcessing(const QString& category, const QString& name, const QString& description, bool overwrite)
{
	if (name.isEmpty())
		return false;

	VipProcessingObject::Info info = this->info();
	info.classname = name;
	info.category = category.split("/", QString::SkipEmptyParts).join("/"); // remove empty strings
	info.description = description;
	info.displayHint = VipProcessingObject::DisplayOnSameSupport;

	PySignalFusionProcessingPtr init(new PySignalFusionProcessing());
	init->topLevelInputAt(0)->toMultiInput()->resize(this->topLevelInputAt(0)->toMultiInput()->count());
	init->propertyName("x_algo")->setData(this->propertyName("x_algo")->data());
	init->propertyName("y_algo")->setData(this->propertyName("y_algo")->data());
	init->propertyName("output_title")->setData(this->propertyName("output_title")->data());
	init->propertyName("output_unit")->setData(this->propertyName("output_unit")->data());
	init->propertyName("output_x_unit")->setData(this->propertyName("output_x_unit")->data());
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

void PySignalFusionProcessing::mergeData(int, int)
{
	// find the inputs that we need
	std::set<int> xinput, yinput, t, u, ux, merged;
	QMap<int, VipPointVector> inputs;
	QMap<int, QString> titles;
	QMap<int, QString> units;
	QString algo = propertyName("y_algo")->value<QString>() + "\n" + propertyName("x_algo")->value<QString>();
	QString output_title = propertyName("output_title")->value<QString>();
	QString output_unit = propertyName("output_unit")->value<QString>();
	QString output_x_unit = propertyName("output_x_unit")->value<QString>();

	// VipMultiInput * input = topLevelInputAt(0)->toMultiInput();
	const QVector<VipAnyData>& input = this->inputs();

	findXYmatch(algo, output_title, output_unit, output_x_unit, input.size(), xinput, yinput, t, u, ux, merged);
	for (std::set<int>::iterator it = merged.begin(); it != merged.end(); ++it) {
		VipAnyData any = input[*it];
		if (output_title.isEmpty())
			output_title = any.name(); // default output name
		inputs[*it] = any.value<VipPointVector>();
		titles[*it] = any.name();
		units[*it] = any.yUnit();
	}

	if (inputs.isEmpty() || yinput.size() == 0) {
		setError("invalid 'y' algorithm");
		return;
	}

	// default x and y values
	VipNDArray x_array = vipExtractXValues(inputs.first());
	VipNDArray y_array = vipExtractYValues(inputs.first());

	// send x inputs to the interpreter
	for (std::set<int>::iterator it = xinput.begin(); it != xinput.end(); ++it) {
		VipPyError err = VipPyInterpreter::instance()->sendObject("x" + QString::number(*it), QVariant::fromValue(vipExtractXValues(inputs[*it]))).value(1000).value<VipPyError>();
		if (!err.isNull()) {
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}
	// send y inputs to the interpreter
	for (std::set<int>::iterator it = yinput.begin(); it != yinput.end(); ++it) {
		VipPyError err = VipPyInterpreter::instance()->sendObject("y" + QString::number(*it), QVariant::fromValue(vipExtractYValues(inputs[*it]))).value(1000).value<VipPyError>();
		if (!err.isNull()) {
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}

	// find output player and send its id
	/* QList<VipAbstractPlayer*> players = VipAbstractPlayer::findOutputPlayers(this);
	int player = players.size() ? VipUniqueId::id(VipBaseDragWidget::fromChild(players.first())) : 0;
	{
		auto err = VipPyInterpreter::instance()->sendObject("player", QVariant::fromValue(player)).value(1000).value<VipPyError>();
		if (!err.isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}*/

	// apply
	QString code = "try: del x\n"
		       "except: pass\n"
		       "try: del y\n"
		       "except: pass\n" +
		       algo;
	auto err = VipPyInterpreter::instance()->execCode(code).value(1000).value<VipPyError>();
	if (!err.isNull()) {
		vip_debug("err: %s\n", err.traceback.toLatin1().data());
		setError(err.traceback);
		return;
	}
	// retrieve x
	QVariant v = VipPyInterpreter::instance()->retrieveObject("x").value(1000);
	if (v.value<VipPyError>().isNull()) {
		x_array = v.value<VipNDArray>();
	}
	// retrieve y
	v = VipPyInterpreter::instance()->retrieveObject("y").value(1000);
	if (v.value<VipPyError>().isNull()) {
		y_array = v.value<VipNDArray>();
	}

	// apply algo
	/*if (yinput.size() || xinput.size()) {
		VipPyIOOperation::command_type c = VipPyInterpreter::instance()->execCode(algo);
		VipPyError err = VipPyInterpreter::instance()->wait(c, 5000).value<VipPyError>();
		if (!err.isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}

	//retrieve result
	if (xinput.size()) {
		VipPyIOOperation::command_type c = VipPyInterpreter::instance()->retrieveObject("x");
		QVariant v = VipPyInterpreter::instance()->wait(c, 1000);
		if (!v.value<VipPyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<VipPyError>().traceback.toLatin1().data());
			setError(v.value<VipPyError>().traceback);
			return;
		}
		//get 'x' variable
		x_array = v.value<VipNDArray>();
	}
	if (x_array.isEmpty()) {
		setError("invalid 'x' array");
		return;
	}

	//read y
	if (yinput.size()) { //always true
		VipPyIOOperation::command_type c = VipPyInterpreter::instance()->retrieveObject("y");
		QVariant v = VipPyInterpreter::instance()->wait(c, 1000);
		if (!v.value<VipPyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<VipPyError>().traceback.toLatin1().data());
			setError(v.value<VipPyError>().traceback);
			return;
		}
		//get 'y' variable
		y_array = v.value<VipNDArray>();
	}
	if (y_array.isEmpty()) {
		setError("invalid 'y' array");
		return;
	}*/

	if (x_array.size() != y_array.size() || y_array.size() == 0) {
		setError("invalid algorithms ('x' and 'y' does not have the same size, or nullptr y)");
		return;
	}

	// replace title and unit
	for (int i = 0; i < input.size(); ++i) {
		int ti = treg[i].indexIn(output_title);
		if (ti >= 0) {
			int len = 1 + (i > 9 ? 2 : 1);
			output_title.replace(ti, len, titles[i]);
		}
		int ui = ureg[i].indexIn(output_unit);
		if (ui >= 0) {
			int len = 1 + (i > 9 ? 2 : 1);
			output_unit.replace(ui, len, units[i]);
		}
		int uxi = uxreg[i].indexIn(output_x_unit);
		if (uxi >= 0) {
			int len = 1 + (i > 9 ? 2 : 1);
			output_x_unit.replace(uxi, len, units[i]);
		}
	}

	// match word starting with $
	QRegExp exp("\\$(\\w+)\\b");
	// find and relpace match in title
	QSet<QString> matches;
	int index = 0;
	while (index >= 0) {
		index = exp.indexIn(output_title, index);
		if (index >= 0)
			matches.insert(output_title.mid(++index, exp.matchedLength() - 1));
	}
	for (QSet<QString>::const_iterator it = matches.begin(); it != matches.end(); ++it) {
		if (it->isEmpty())
			continue;
		QVariant v = VipPyInterpreter::instance()->retrieveObject(*it).value(1000);
		if (!v.value<VipPyError>().isNull()) {
			vip_debug("err: %s\n", v.value<VipPyError>().traceback.toLatin1().data());
			setError(v.value<VipPyError>().traceback);
			return;
		}
		output_title.replace("$" + *it, v.toString());
	}

	// find and replace match in unit
	matches.clear();
	index = 0;
	while (index >= 0) {
		index = exp.indexIn(output_unit, index);
		if (index >= 0)
			matches.insert(output_unit.mid(++index, exp.matchedLength() - 1));
	}
	for (QSet<QString>::const_iterator it = matches.begin(); it != matches.end(); ++it) {
		if (it->isEmpty())
			continue;
		QVariant v = VipPyInterpreter::instance()->retrieveObject(*it).value(1000);
		if (!v.value<VipPyError>().isNull()) {
			vip_debug("err: %s\n", v.value<VipPyError>().traceback.toLatin1().data());
			setError(v.value<VipPyError>().traceback);
			return;
		}
		output_unit.replace("$" + *it, v.toString());
	}

	// find and replace match in xunit
	matches.clear();
	index = 0;
	while (index >= 0) {
		index = exp.indexIn(output_x_unit, index);
		if (index >= 0)
			matches.insert(output_x_unit.mid(++index, exp.matchedLength() - 1));
	}
	for (QSet<QString>::const_iterator it = matches.begin(); it != matches.end(); ++it) {
		if (it->isEmpty())
			continue;
		QVariant v = VipPyInterpreter::instance()->retrieveObject(*it).value(1000);
		if (!v.value<VipPyError>().isNull()) {
			vip_debug("err: %s\n", v.value<VipPyError>().traceback.toLatin1().data());
			setError(v.value<VipPyError>().traceback);
			return;
		}
		output_x_unit.replace("$" + *it, v.toString());
	}

	VipPointVector res = vipCreatePointVector(x_array, y_array);
	VipAnyData any = create(QVariant::fromValue(res));
	any.setName(output_title);
	if (!output_unit.isEmpty())
		any.setYUnit(output_unit);
	if (!output_x_unit.isEmpty())
		any.setXUnit(output_x_unit);

	QString stylesheet;
	// grab the style sheet
	QVariant value = VipPyInterpreter::instance()->retrieveObject("stylesheet").value(2000);
	if (value.value<VipPyError>().isNull()) {
		stylesheet = value.toString();
	}
	any.setAttribute("stylesheet", stylesheet);

	outputAt(0)->setData(any);
}
