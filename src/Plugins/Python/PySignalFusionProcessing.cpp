#include "PySignalFusionProcessing.h"
#include "PyRegisterProcessing.h"
#include "PyProcessing.h"
#include "PyOperation.h"
#include "PyEditor.h"
#include "IOOperationWidget.h"
#include "VipPlayer.h"
#include "VipCore.h"
#include "VipDisplayArea.h"


#include <qclipboard.h>
#include <qapplication.h>
#include <qtooltip.h>
#include <qmessagebox.h>

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

static int registerPySignalFusionProcessingPtr() {
	qRegisterMetaType<PySignalFusionProcessingPtr>();
	return 0;
}
static int _registerPySignalFusionProcessingPtr = registerPySignalFusionProcessingPtr();


static QRegExp xreg[50];
static QRegExp yreg[50];
static QRegExp ureg[50];
static QRegExp uxreg[50];
static QRegExp treg[50];

static void findXYmatch(const QString & algo, const QString & title, const QString & unit, const QString & xunit, int count,
	std::set<int> & x, std::set<int> & y, std::set<int> & t, std::set<int> & u, std::set<int> & ux, std::set<int> & merged)
{
	if (xreg[0].isEmpty()) {
		//initialize regular expressions
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
		if (xi >= 0) x.insert(i);
		if (yi >= 0) y.insert(i);
		if (ti >= 0) t.insert(i);
		if (ui >= 0) u.insert(i);
		if (uxi >= 0) ux.insert(i);
		if (xi >= 0 || yi >= 0 || ti >= 0 || ui >= 0) merged.insert(i);
	}
}





PySignalFusionProcessing::PySignalFusionProcessing(QObject * parent)
	:PyBaseProcessing(parent)
{
	//prepare inputs/properties/output
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


QVariant PySignalFusionProcessing::initializeProcessing(const QVariant & v)
{
	if (PySignalFusionProcessingPtr ptr = v.value<PySignalFusionProcessingPtr>()) {
		topLevelInputAt(0)->toMultiInput()->resize(ptr->topLevelInputAt(0)->toMultiInput()->count());
		topLevelInputAt(0)->toMultiInput()->setMinSize(ptr->topLevelInputAt(0)->toMultiInput()->count());
		topLevelInputAt(0)->toMultiInput()->setMaxSize(ptr->topLevelInputAt(0)->toMultiInput()->count());

		if(ptr->propertyName("x_algo"))
			propertyName("x_algo")->setData(ptr->propertyName("x_algo")->data());
		if (ptr->propertyName("y_algo"))
			propertyName("y_algo")->setData(ptr->propertyName("y_algo")->data());
		if(ptr->propertyName("output_title"))
			propertyName("output_title")->setData(ptr->propertyName("output_title")->data());
		if(ptr->propertyName("output_unit"))
			propertyName("output_unit")->setData(ptr->propertyName("output_unit")->data());
		if (ptr->propertyName("output_x_unit"))
			propertyName("output_x_unit")->setData(ptr->propertyName("output_x_unit")->data());
		if(ptr->propertyName("Time_range"))
			propertyName("Time_range")->setData(ptr->propertyName("Time_range")->data());
		return QVariant(true);
	}
	return QVariant(false);
}

bool PySignalFusionProcessing::registerThisProcessing(const QString & category, const QString & name, const QString & description, bool overwrite)
{
	if (name.isEmpty())
		return false;

	VipProcessingObject::Info info = this->info();
	info.classname = name;
	info.category = category.split("/", QString::SkipEmptyParts).join("/"); //remove empty strings
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

void PySignalFusionProcessing::applyPyProcessing(int,int)
{
	//find the inputs that we need
	std::set<int> xinput, yinput, t, u,ux, merged;
	QMap<int, VipPointVector> inputs;
	QMap<int, QString> titles;
	QMap<int, QString> units;
	QString algo = propertyName("y_algo")->value<QString>() + "\n" + propertyName("x_algo")->value<QString>() ;
	QString output_title = propertyName("output_title")->value<QString>();
	QString output_unit = propertyName("output_unit")->value<QString>();
	QString output_x_unit = propertyName("output_x_unit")->value<QString>();

	//VipMultiInput * input = topLevelInputAt(0)->toMultiInput();
	const QVector<VipAnyData> & input = this->inputs();
	

	findXYmatch(algo,output_title, output_unit, output_x_unit, input.size(), xinput, yinput,t,u,ux, merged);
	for (std::set<int>::iterator it = merged.begin(); it != merged.end(); ++it) {
		VipAnyData any = input[*it];
		if (output_title.isEmpty()) output_title = any.name(); //default output name
		inputs[*it] = any.value<VipPointVector>();
		titles[*it] = any.name();
		units[*it] = any.yUnit();
	}
	
	if (inputs.isEmpty() || yinput.size() == 0) {
		setError("invalid 'y' algorithm");
		return;
	}

	//default x and y values
	VipNDArray x_array = vipExtractXValues(inputs.first());
	VipNDArray y_array = vipExtractYValues(inputs.first());

	//send x inputs to the interpreter
	for (std::set<int>::iterator it = xinput.begin(); it != xinput.end(); ++it) {
		PyIOOperation::command_type c = GetPyOptions()->sendObject("x" + QString::number(*it), QVariant::fromValue(vipExtractXValues(inputs[*it])));
		PyError err = GetPyOptions()->wait(c, 1000).value<PyError>();
		if (!err.isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}
	//send y inputs to the interpreter
	for (std::set<int>::iterator it = yinput.begin(); it != yinput.end(); ++it) {
		PyIOOperation::command_type c = GetPyOptions()->sendObject("y" + QString::number(*it), QVariant::fromValue(vipExtractYValues(inputs[*it])));
		PyError err = GetPyOptions()->wait(c, 1000).value<PyError>();
		if (!err.isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
		
	}


	//find output player and send its id
	QList<VipAbstractPlayer*> players = VipAbstractPlayer::findOutputPlayers(this);
	int player = players.size() ? VipUniqueId::id(VipBaseDragWidget::fromChild(players.first())) : 0;
	{
		PyIOOperation::command_type c = GetPyOptions()->sendObject("player", QVariant::fromValue(player));
		PyError err = GetPyOptions()->wait(c, 1000).value<PyError>();
		if (!err.isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}

	//apply
	QString code =
		"try: del x\n"
		"except: pass\n"
		"try: del y\n"
		"except: pass\n"
		+ algo;
	PyIOOperation::command_type c = GetPyOptions()->execCode(code);
	PyError err = GetPyOptions()->wait(c, 5000).value<PyError>();
	if (!err.isNull()) {
		QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
		vip_debug("err: %s\n", err.traceback.toLatin1().data());
		setError(err.traceback);
		return;
	}
	//retrieve x
	c = GetPyOptions()->retrieveObject("x");
	QVariant v = GetPyOptions()->wait(c, 1000);
	if (v.value<PyError>().isNull()) {
		x_array = v.value<VipNDArray>();
	}
	//retrieve y
	c = GetPyOptions()->retrieveObject("y");
	v = GetPyOptions()->wait(c, 1000);
	if (v.value<PyError>().isNull()) {
		y_array = v.value<VipNDArray>();
	}

	//apply algo
	/*if (yinput.size() || xinput.size()) {
		PyIOOperation::command_type c = GetPyOptions()->execCode(algo);
		PyError err = GetPyOptions()->wait(c, 5000).value<PyError>();
		if (!err.isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", err.traceback.toLatin1().data());
			setError(err.traceback);
			return;
		}
	}

	//retrieve result
	if (xinput.size()) {
		PyIOOperation::command_type c = GetPyOptions()->retrieveObject("x");
		QVariant v = GetPyOptions()->wait(c, 1000);
		if (!v.value<PyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<PyError>().traceback.toLatin1().data());
			setError(v.value<PyError>().traceback);
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
		PyIOOperation::command_type c = GetPyOptions()->retrieveObject("y");
		QVariant v = GetPyOptions()->wait(c, 1000);
		if (!v.value<PyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<PyError>().traceback.toLatin1().data());
			setError(v.value<PyError>().traceback);
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

	//replace title and unit
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

	//match word starting with $
	QRegExp exp("\\$(\\w+)\\b");
	//find and relpace match in title
	QSet<QString> matches;
	int index = 0;
	while (index >= 0) {
		index = exp.indexIn(output_title,index);
		if (index >= 0) matches.insert(output_title.mid(++index, exp.matchedLength()-1));
	}
	for (QSet<QString>::const_iterator it = matches.begin(); it != matches.end(); ++it) {
		if (it->isEmpty()) continue;
		QVariant v = GetPyOptions()->wait(GetPyOptions()->retrieveObject(*it), 1000);
		if (!v.value<PyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<PyError>().traceback.toLatin1().data());
			setError(v.value<PyError>().traceback);
			return;
		}
		output_title.replace("$" + *it, v.toString());
	}

	//find and replace match in unit
	matches.clear();
	index = 0;
	while (index >= 0) {
		index = exp.indexIn(output_unit, index);
		if (index >= 0) matches.insert(output_unit.mid(++index, exp.matchedLength()-1));
	}
	for (QSet<QString>::const_iterator it = matches.begin(); it != matches.end(); ++it) {
		if (it->isEmpty()) continue;
		QVariant v = GetPyOptions()->wait(GetPyOptions()->retrieveObject(*it), 1000);
		if (!v.value<PyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<PyError>().traceback.toLatin1().data());
			setError(v.value<PyError>().traceback);
			return;
		}
		output_unit.replace("$" + *it, v.toString());
	}

	//find and replace match in xunit
	matches.clear();
	index = 0;
	while (index >= 0) {
		index = exp.indexIn(output_x_unit, index);
		if (index >= 0) matches.insert(output_x_unit.mid(++index, exp.matchedLength() - 1));
	}
	for (QSet<QString>::const_iterator it = matches.begin(); it != matches.end(); ++it) {
		if (it->isEmpty()) continue;
		QVariant v = GetPyOptions()->wait(GetPyOptions()->retrieveObject(*it), 1000);
		if (!v.value<PyError>().isNull()) {
			QMetaObject::invokeMethod(pyGetPythonInterpreter(), "showAndRaise", Qt::QueuedConnection);
			vip_debug("err: %s\n", v.value<PyError>().traceback.toLatin1().data());
			setError(v.value<PyError>().traceback);
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
	//grab the style sheet
	QVariant value = GetPyOptions()->wait(GetPyOptions()->retrieveObject("stylesheet"), 2000);
	if (value.value<PyError>().isNull())
	{
		stylesheet = value.toString();
	}
	any.setAttribute("stylesheet", stylesheet);


	outputAt(0)->setData(any);
}




#include <qtoolbar.h>
#include <qboxlayout.h>

class PySignalFusionProcessingEditor::PrivateData
{
public:
	PrivateData() :
		editor(Qt::Vertical), popupDepth(0)
	{}
	QPointer<VipPlotPlayer> player;
	QPointer<PySignalFusionProcessing> proc;

	QComboBox resampling;

	QToolButton names;

	QLineEdit title;
	QLineEdit yunit;
	QLineEdit xunit;

	PyEditor editor;

	PyApplyToolBar * buttons;

	int popupDepth;
};



static QString names_toolTip = "<b>Name mapping</b><br>This menu specifies the names of each signals x/y components within the Python script.<br>"
"Click on a signal name to copy it to the clipboard.";

PySignalFusionProcessingEditor::PySignalFusionProcessingEditor(QWidget *parent)
	:QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	QGridLayout * hlay = new QGridLayout();
	hlay->addWidget(new QLabel("Resampling method"), 0, 0);
	hlay->addWidget(&d_data->resampling, 0, 1);
	hlay->addWidget(new QLabel("Output signal name"),1,0);
	hlay->addWidget(&d_data->title,1,1);
	hlay->addWidget(new QLabel("Output signal unit"), 2, 0);
	hlay->addWidget(&d_data->yunit, 2, 1);
	hlay->addWidget(new QLabel("Output signal X unit"), 3, 0);
	hlay->addWidget(&d_data->xunit, 3, 1);
	d_data->title.setPlaceholderText("Output signal name (mandatory)");
	d_data->title.setToolTip("<b>Enter the output signal name (mandatory)</b><br>"
	"The signal name could be either a string (like 'My_signal_name') or<br>"
	"a formula using the input signal titles (like 't0 * t1'). In this case,<br>"
	"t0 and t1 will be expanded to the input signal names.<br><br>"
	"It is also possible to use a Python variable in the expression. $MyVariable will be expanded to MyVariable value.");
	d_data->yunit.setPlaceholderText("Output signal unit (optional)");
	d_data->yunit.setToolTip("<b>Optional signal unit.</b><br>By default, the output unit name will be the same as the first input signal unit.<br><br>"
		"The signal unit could be either a string (like 'My_signal_unit') or<br>"
		"a formula using the input signal units (like 'u0.u1'). In this case,<br>"
		"u0 and u1 will be expanded to the input signal units.<br><br>"
		"It is also possible to use a Python variable in the expression. $MyVariable will be expanded to MyVariable value.");
	d_data->xunit.setPlaceholderText("Output signal X unit (optional)");
	d_data->xunit.setToolTip("<b>Optional signal X unit.</b><br>By default, the output X unit name will be the same as the first input signal unit.<br><br>"
		"The signal unit could be either a string (like 'My_signal_unit') or<br>"
		"a formula using the input signal units (like 'u0.u1'). In this case,<br>"
		"u0 and u1 will be expanded to the input signal units.<br><br>"
		"It is also possible to use a Python variable in the expression. $MyVariable will be expanded to MyVariable value.");
	d_data->resampling.addItems(QStringList() << "union" << "intersection");
	d_data->resampling.setToolTip("Input signals will be resampled based on given method (union or intersection of input time ranges)");

	
	d_data->editor.setUniqueFile(true);
	d_data->editor.TabBar()->actions().first()->setVisible(false);
	d_data->editor.TabBar()->actions()[1]->setVisible(false);
	d_data->editor.TabBar()->actions()[2]->setVisible(false);

	d_data->names.setToolTip(names_toolTip);
	d_data->names.setMenu(new QMenu());
	d_data->names.setPopupMode(QToolButton::InstantPopup);
	d_data->names.setText("Input signals names");
	
	d_data->editor.setToolTip("Python script for the y and x (time) components (mandatory)");
	d_data->editor.CurrentEditor()->setPlaceholderText("Example: y = y0 + y1");

	d_data->buttons = new PyApplyToolBar();

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(&d_data->names);
	lay->addLayout(hlay);
	lay->addWidget(&d_data->editor);
	lay->addWidget(d_data->buttons);
	setLayout(lay);


	connect(d_data->names.menu(), SIGNAL(triggered(QAction*)), this, SLOT(nameTriggered(QAction*)));
	connect(d_data->buttons->applyButton(), SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(d_data->buttons->registerButton(), SIGNAL(clicked(bool)), this, SLOT(registerProcessing()));
	connect(d_data->buttons->manageButton(), SIGNAL(clicked(bool)), this, SLOT(manageProcessing()));
	connect(&d_data->resampling, SIGNAL(currentIndexChanged(int)), this, SLOT(updateProcessing()));

	this->setMinimumWidth(450);
	this->setMinimumHeight(350);
}

PySignalFusionProcessingEditor::~PySignalFusionProcessingEditor()
{
}

void PySignalFusionProcessingEditor::nameTriggered(QAction* a)
{
	//copy selected entry in the menu to clipboard
	if (a) {
		qApp->clipboard()->setText(a->property("name").toString());
	}
}

void PySignalFusionProcessingEditor::registerProcessing()
{
	if (!d_data->proc)
		return;

	//register the current processing
	PySignalFusionProcessingManager * m = new PySignalFusionProcessingManager();
	m->setManagerVisible(false);
	m->setCreateNewVisible(true);
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

void PySignalFusionProcessingEditor::manageProcessing()
{
	PyRegisterProcessing::openProcessingManager();
}

void PySignalFusionProcessingEditor::setPlotPlayer(VipPlotPlayer * player)
{
	if (d_data->player != player) {
		d_data->player = player;

		QList<VipPlotCurve*> tmp = player->plotWidget2D()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
		QString yunit;

		//grab curves from the player
		QMultiMap<QString, QString> curves;
		for (int i = 0; i < tmp.size(); ++i) {
			curves.insert(tmp[i]->title().text(), QString());
			if (yunit.isEmpty()) yunit = tmp[i]->axisUnit(1).text();
		}
		//set the names
		d_data->names.menu()->clear();
		int i = 0;
		QStringList text;
		for (QMultiMap<QString, QString>::const_iterator it = curves.begin(); it != curves.end(); ++it, ++i) {
			QAction * a = d_data->names.menu()->addAction("'" + it.key() + "' as 'x" + QString::number(i) + "', 'y" + QString::number(i));
			text << a->text();
			a->setProperty("name", it.key());
		}
		d_data->names.setToolTip(names_toolTip + "<br><br>" + text.join("<br>"));

	}
}

VipPlotPlayer * PySignalFusionProcessingEditor::plotPlayer() const{return d_data->player;}
PyApplyToolBar * PySignalFusionProcessingEditor::buttons() const{return d_data->buttons;}


void PySignalFusionProcessingEditor::setPySignalFusionProcessing(PySignalFusionProcessing * proc)
{
	if (proc != d_data->proc) {
		d_data->proc = proc;
		updateWidget();
	}
}
PySignalFusionProcessing * PySignalFusionProcessingEditor::getPySignalFusionProcessing() const
{
	return d_data->proc;
}

bool PySignalFusionProcessingEditor::updateProcessing()
{
	QString algo = d_data->editor.CurrentEditor() ? d_data->editor.CurrentEditor()->toPlainText() : QString();
	if (d_data->proc) {

		d_data->proc->propertyName("Time_range")->setData(d_data->resampling.currentText());
		d_data->proc->propertyName("output_title")->setData(d_data->title.text());
		d_data->proc->propertyName("output_unit")->setData(d_data->yunit.text());
		d_data->proc->propertyName("output_x_unit")->setData(d_data->xunit.text());
		QString output_title = d_data->title.text();
		QString output_unit = d_data->yunit.text();
		QString output_x_unit = d_data->xunit.text();

		if (d_data->player) {
			//we need to create the processing inputs and remap the input names
			QList<VipPlotCurve*> tmp = d_data->player->plotWidget2D()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
			QMultiMap<QString, VipPlotCurve*> curves;
			for (int i = 0; i < tmp.size(); ++i)
				curves.insert(tmp[i]->title().text(), tmp[i]);
			tmp = curves.values();

			//find each input x/y
			std::set<int> x, y,u,ux,t, merged;
			findXYmatch(algo, output_title, output_unit, output_x_unit, curves.size(), x, y,t,u,ux, merged);
			if (y.size() == 0)
				return false;


			int inputs = 0;
			for (std::set<int>::iterator it = merged.begin(); it != merged.end(); ++it) {
				//we have an input
				d_data->proc->topLevelInputAt(0)->toMultiInput()->resize(inputs + 1);
				//set connection
				if (VipDisplayObject * disp = tmp[*it]->property("VipDisplayObject").value<VipDisplayObject*>()) {
					d_data->proc->inputAt(inputs)->setConnection(disp->inputAt(0)->connection()->source());
				}
				//rename x/y input
				if (x.find(*it) != x.end()) algo.replace("x" + QString::number(*it), "x" + QString::number(inputs));
				if (y.find(*it) != y.end()) algo.replace("y" + QString::number(*it), "y" + QString::number(inputs));
				//rename title nd unit
				if (t.find(*it) != t.end()) output_title.replace("t" + QString::number(*it), "t" + QString::number(inputs));
				if (u.find(*it) != u.end()) output_unit.replace("u" + QString::number(*it), "u" + QString::number(inputs));
				if (ux.find(*it) != ux.end()) output_x_unit.replace("u" + QString::number(*it), "u" + QString::number(inputs));
				++inputs;
			}
		}

		d_data->proc->propertyName("y_algo")->setData(algo);
		d_data->proc->propertyName("x_algo")->setData(QString());
		d_data->proc->propertyName("output_title")->setData(output_title);
		d_data->proc->propertyName("output_unit")->setData(output_unit);
		d_data->proc->propertyName("output_x_unit")->setData(output_x_unit);
		return true;
	}
	return false;
}

void PySignalFusionProcessingEditor::updateWidget()
{
	//we can only update if there is no player (otherwise too complicated)
	if (!d_data->proc)
		return;
	if (d_data->player)
		return;

	d_data->resampling.blockSignals(true);
	d_data->resampling.setCurrentText(d_data->proc->propertyName("Time_range")->value<QString>());
	d_data->resampling.blockSignals(false);
	d_data->title.setText(d_data->proc->propertyName("output_title")->value<QString>());
	d_data->yunit.setText(d_data->proc->propertyName("output_unit")->value<QString>());
	d_data->xunit.setText(d_data->proc->propertyName("output_x_unit")->value<QString>());

	//grab input names in alphabetical order
	QMultiMap<QString, QString> curves;
	for (int i = 0; i < d_data->proc->inputCount(); ++i) {
		curves.insert(d_data->proc->inputAt(i)->probe().name(), QString());
	}
	//set names
	d_data->names.menu()->clear();
	int i = 0;
	QStringList text;
	for (QMultiMap<QString, QString>::const_iterator it = curves.begin(); it != curves.end(); ++it, ++i) {
		QAction * a = d_data->names.menu()->addAction("'" + it.key() + "' as 'x" + QString::number(i) + "', 'y" + QString::number(i) +"'");
		text << a->text();
		a->setProperty("name", it.key());
	}
	d_data->names.setToolTip(names_toolTip + "<br><br>" + text.join("<br>"));
	
	//update algo editors
	d_data->editor.CurrentEditor()->setPlainText(
		d_data->proc->propertyName("y_algo")->value<QString>() + "\n" + 
		d_data->proc->propertyName("x_algo")->value<QString>());
}


void PySignalFusionProcessingEditor::showError(const QPoint & pos, const QString & error)
{
	QToolTip::showText(pos, error, nullptr, QRect(), 5000);
}
void PySignalFusionProcessingEditor::showErrorDelayed(const QPoint & pos, const QString & error)
{
	if (d_data->popupDepth < 4) {
		d_data->popupDepth++;
		QMetaObject::invokeMethod(this, "showErrorDelayed", Qt::QueuedConnection, Q_ARG(QPoint, pos), Q_ARG(QString, error));
	}
	else {
		d_data->popupDepth = 0;
		QMetaObject::invokeMethod(this, "showError", Qt::QueuedConnection, Q_ARG(QPoint, pos), Q_ARG(QString, error));
	}


}

bool PySignalFusionProcessingEditor::apply()
{
	//check output name
	if (d_data->title.text().isEmpty()) {
		//display a tool tip at the bottom
		QPoint pos = d_data->title.mapToGlobal(QPoint(0, d_data->title.height()));
		showErrorDelayed(pos, "Setting a valid signal name is mandatory!");
		return false;
	}

	//check script
	QString algo = d_data->editor.CurrentEditor() ? d_data->editor.CurrentEditor()->toPlainText() : QString();
	QRegExp reg("[\\s]{0,10}y[\\s]{0,10}=");
	int match = reg.indexIn(algo);
	if (match < 0) {
		//display a tool tip at the bottom of y editor
		QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
		showErrorDelayed(pos, "You must specify a valid script for the y component!\nA valid script must set the 'y' variable: 'y = ...'");
		return false;
	}

	
	if (d_data->proc) {

		if (!updateProcessing()) {
			//display a tool tip at the bottom of y editor
			QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
			showErrorDelayed(pos, "Given script is not valid!\nThe script needs to reference at least 2 input signals (like y0, y1,...)");
			return false;
		}

		QList<VipAnyData> inputs; //save input data, since setScheduleStrategy will clear them
		for (int i = 0; i < d_data->proc->inputCount(); ++i) inputs << d_data->proc->inputAt(i)->probe();

		d_data->proc->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		//set input data
		for (int i = 0; i < d_data->proc->inputCount(); ++i) {
			if(VipOutput * src = d_data->proc->inputAt(i)->connection()->source())
				d_data->proc->inputAt(i)->setData(src->data());
			else
				d_data->proc->inputAt(i)->setData(inputs[i]);
		}
		if (!d_data->proc->update()) {
			//display a tool tip at the bottom of y editor
			QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
			showErrorDelayed(pos, "Given script must use at least 2 different input signals!");
			return false;
		}
		QString err = d_data->proc->error().errorString();
		bool has_error = d_data->proc->hasError();
		//d_data->proc->restore();
		d_data->proc->setScheduleStrategy(VipProcessingObject::Asynchronous, true);
		if (has_error) {
			VipText text("An error occured while applying the processings!\n\n" + d_data->proc->error().errorString());
			//QSize s = text.textSize().toSize();
			QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
			showErrorDelayed(pos, text.text());
			return false;
		}
	}
	return true;
}




//register editor

static PySignalFusionProcessingEditor * editPySignalFusionProcessing(PySignalFusionProcessing * proc)
{
	PySignalFusionProcessingEditor * editor = new PySignalFusionProcessingEditor();
	editor->setPySignalFusionProcessing(proc);
	return editor;
}

int registerEditPySignalFusionProcessing()
{
	vipFDObjectEditor().append<QWidget*(PySignalFusionProcessing*)>(editPySignalFusionProcessing);
	return 0;
}
static int _registerEditPySignalFusionProcessing = registerEditPySignalFusionProcessing();



