#include "VipPyEditor.h"

#include "VipDisplayArea.h"
#include "VipProgress.h"

#include <qsplitter.h>
#include <qboxlayout.h>
#include <qcoreapplication.h>
#include <qtoolbutton.h>
#include <qmenu.h>

#include "VipPyIPython.h"

class VipPyEditor::PrivateData
{
public:
	QPointer<QWidget> runningShell;	    // shell that runs the script, either a VipPyShellWidget or a VipIPythonShellWidget
	QPointer<QObject> runningOperation; // Object reunning the code, either a VipPyIOOperation or a VipIPythonShellProcess
	QToolButton* startButton;
	QString runFileCode;
	bool running;
	bool debug;

	QAction* start;
	QAction* stop;
	QTimer timer;
};

VipPyEditor::VipPyEditor(QWidget* parent)
  : VipTabEditor(Qt::Horizontal, parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->runFileCode = "_vip_stop = 1\n"
			      "def runFile(file):\n"
			      "  global _vip_stop\n"
			      "  _vip_stop = 0\n"
			      "  try:\n"
			      "    exec(open(file).read(),globals(),globals())\n"
			      "  except:\n"
			      "    _vip_stop = 1; raise\n"
			      "  _vip_stop = 1\n"

			      "def debugFile(file):\n"
			      "  _vip_stop = 0\n"
			      "  import pdb; pdb.run(open(file).read(),globals(),globals())\n"
			      "  _vip_stop = 1\n";

	// Restart interpreter when file execution finished (only for VipPyIOOperation)
	connect(this, SIGNAL(fileFinished()), this, SLOT(startInteractiveInterpreter()));

	this->setDefaultColorSchemeType("Python");

	d_data->running = false;
	d_data->debug = false;
	d_data->timer.setSingleShot(false);
	d_data->timer.setInterval(500);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(check()));

	this->tabBar()->addSeparator();

	QToolButton* start = new QToolButton();
	start->setAutoRaise(true);
	start->setIcon(vipIcon("start_streaming.png"));
	start->setToolTip("Run file");
	start->setMenu(new QMenu());
	connect(start->menu()->addAction("Execute in internal console"), SIGNAL(triggered(bool)), this, SLOT(execInInternal()));
	connect(start->menu()->addAction("Execute in IPython console"), SIGNAL(triggered(bool)), this, SLOT(execInIPython()));
	start->menu()->actions()[0]->setCheckable(true);
	start->menu()->actions()[1]->setCheckable(true);
	if (VipPyInterpreter::instance()->launchCode() == VipPyInterpreter::InLocalInterp)
		start->menu()->actions()[0]->setChecked(true);
	else
		start->menu()->actions()[1]->setChecked(true);
	start->setPopupMode(QToolButton::MenuButtonPopup);
	d_data->startButton = start;

	d_data->start = this->tabBar()->addWidget(start);
	d_data->stop = this->tabBar()->addAction(vipIcon("stop.png"), "Stop running");
	d_data->stop->setEnabled(false);

	connect(d_data->startButton->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToDisplayLaunchMode()));
	connect(d_data->startButton, SIGNAL(clicked(bool)), this, SLOT(execFile()));
	connect(d_data->stop, SIGNAL(triggered(bool)), this, SLOT(stopFile()));

	this->newFile();
}

VipPyEditor::~VipPyEditor()
{
	d_data->timer.stop();
	disconnect(&d_data->timer, SIGNAL(timeout()), this, SLOT(check()));
	stopFile(true);
	QCoreApplication::removePostedEvents(this);
	QCoreApplication::removePostedEvents(&d_data->timer);
}

void VipPyEditor::aboutToDisplayLaunchMode()
{
	if (VipPyInterpreter::instance()->launchCode() == VipPyInterpreter::InLocalInterp)
		execInInternal();
	else
		execInIPython();
}

void VipPyEditor::execInInternal()
{
	d_data->startButton->menu()->actions()[0]->setChecked(true);
	d_data->startButton->menu()->actions()[1]->setChecked(false);
	VipPyInterpreter::instance()->setLaunchCode(VipPyInterpreter::InLocalInterp);
}
void VipPyEditor::execInIPython()
{
	d_data->startButton->menu()->actions()[0]->setChecked(false);
	d_data->startButton->menu()->actions()[1]->setChecked(true);
	VipPyInterpreter::instance()->setLaunchCode(VipPyInterpreter::InIPythonInterp);
}

void VipPyEditor::startInteractiveInterpreter()
{
	if (qobject_cast<VipPyIOOperation*>(d_data->runningOperation.data())) {
		if (!VipPyInterpreter::instance()->isRunning()) {
			VipPyInterpreter::instance()->pyIOOperation(true);
		}
	}
}

QWidget* VipPyEditor::shellWidget() const
{
	return d_data->runningShell;
}
QObject* VipPyEditor::interpreter() const
{
	return d_data->runningOperation;
}

bool VipPyEditor::isFileRunning() const
{
	return d_data->running;
}
bool VipPyEditor::isDebugging() const
{
	return d_data->running && d_data->debug;
}

bool VipPyEditor::isRunning()
{
	// Check if VipPyIOOperation still running
	if (qobject_cast<VipPyIOOperation*>(d_data->runningOperation.data())) {
		if (!VipPyInterpreter::instance()->isRunning())
			return false;
		QVariant v = VipPyInterpreter::instance()->retrieveObject("_vip_stop").value(100);
		if (v.canConvert<int>() && v.toInt() == 1)
			return false;
		if (d_data->runningOperation != VipPyInterpreter::instance()->pyIOOperation())
			return false;
		return true;
	}
	// Check if VipIPythonShellWidget is still running
	else {
		if (VipIPythonShellWidget* w = qobject_cast<VipIPythonShellWidget*>(d_data->runningShell)) {
			if (w->process()->state() != QProcess::Running)
				return false;

			if (!w->process()->isRunningCode())
				return false;

			return true;
		}
		return false;
	}

	return false;
}

void VipPyEditor::check()
{
	bool running = true;

	if (d_data->runningShell) {
		// Check if file is still running
		if (d_data->running && !isRunning()) {
			running = false;
		}
	}
	else {
		running = false;
	}

	if (!running) {
		// no running shell, stop
		d_data->running = 0;
		d_data->debug = 0;
		d_data->timer.stop();
		d_data->stop->setEnabled(false);
		d_data->start->setEnabled(true);

		Q_EMIT fileFinished();
	}
}

void VipPyEditor::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_F5)
		execFile();
}

#include "VipProcessingObject.h"

void VipPyEditor::execFile()
{
	stopFile();

	QWidget* exec_in_shell = nullptr;
	if (VipPyInterpreter::instance()->launchCode() == VipPyInterpreter::InIPythonInterp) {
		if (vipGetIPythonToolWidget() && vipGetIPythonToolWidget()->widget()->count())
			exec_in_shell = qobject_cast<VipIPythonShellWidget*>(vipGetIPythonToolWidget()->widget()->currentWidget());
	}
	if (!exec_in_shell) {
		exec_in_shell = vipPyGetPythonInterpreter()->interpreter();
	}
	d_data->runningShell = exec_in_shell;
	if (!exec_in_shell)
		return;

	VipIPythonShellWidget* ipython = qobject_cast<VipIPythonShellWidget*>(exec_in_shell);

	// TODO
	/* if (!ipython && qobject_cast<PyProcess*>(VipPyInterpreter::instance()->pyIOOperation())) {
		//for external python process (PyProcess), make sure to register all new VipIODevice first
		//because a call to VipPyProcessing::setStdPyProcessingFile will freeze the GUI (try to execute a python command while the
		//python file is running -> deadlock)
		VipProcessingObject::allObjects();

	}*/

	if (VipTextEditor* ed = this->currentEditor()) {
		this->save();
		QString file = ed->fileInfo().canonicalFilePath();
		if (!file.isEmpty()) {

			file.replace("\\", "/");
			d_data->running = 1;
			d_data->debug = 0;

			// Execute in global python interpreter
			if (!ipython) {
				VipPyInterpreter::instance()->execCode("_vip_stop=0").wait(1000);
				VipPyInterpreter::instance()->execCode(d_data->runFileCode).wait(1000);
				d_data->runningOperation = VipPyInterpreter::instance()->pyIOOperation();
				vipPyGetPythonInterpreter()->interpreter()->execCommand("runFile('" + file + "')");
				vipPyGetPythonInterpreter()->show();
				vipPyGetPythonInterpreter()->raise();
			}
			// Execute in IPython
			else {
				if (ipython->process()->isRunningCode()) {
					// do not run file if the ipython console is already running something
					d_data->running = 0;
					d_data->debug = 0;
					return;
				}
				ipython->process()->execCode("_vip_stop=0");
				ipython->process()->execCode(d_data->runFileCode);
				d_data->runningOperation = ipython->process();
				vipGetIPythonToolWidget()->show();
				vipGetIPythonToolWidget()->raise();
				// exec in separate thread
				// std::thread(std::bind(&VipIPythonShellProcess::execLine, ipython->process(), ("runFile('" + file + "')"))).detach();
				ipython->process()->execLineNoWait("runFile('" + file + "')");
			}
			d_data->timer.start();
			d_data->stop->setEnabled(true);
			d_data->start->setEnabled(false);
		}
	}
}
void VipPyEditor::debugFile() {}

void VipPyEditor::stopFile(bool wait)
{
	if (d_data->running) {

		// Stop VipPyIOOperation
		if (qobject_cast<VipPyIOOperation*>(d_data->runningOperation.data())) {
			VipPyInterpreter::instance()->stop(wait);
		}
		// Stop VipIPythonShellWidget
		else {
			if (VipIPythonShellWidget* w = qobject_cast<VipIPythonShellWidget*>(d_data->runningShell))
				w->restartProcess();
		}

		// d_data->running = 0;
		// d_data->debug = 0;
		/*d_data->stop->setEnabled(false);
		d_data->start->setEnabled(true);
		d_data->timer.stop();*/

		// d_data->ioOperation->stop(wait);

		// if(wait)
		//	d_data->ioOperation->start();
		// Q_EMIT fileFinished();
	}
}
void VipPyEditor::nextStep() {}
void VipPyEditor::stepIn() {}
void VipPyEditor::stepOut() {}
void VipPyEditor::pause() {}
void VipPyEditor::_continue() {}

#include "VipArchive.h"
static VipArchive& operator<<(VipArchive& arch, const VipPyEditorToolWidget* w)
{
	return arch.content("state", w->editor()->saveState());
}
static VipArchive& operator>>(VipArchive& arch, VipPyEditorToolWidget* w)
{
	QByteArray ar = arch.read("state").toByteArray();
	if (ar.size())
		w->editor()->restoreState(ar);
	return arch;
}

VIP_GUI_EXPORT VipPyEditorToolWidget* vipGetPyEditorToolWidget()
{
	static VipPyEditorToolWidget* python = nullptr;
	if (!python) {
		python = new VipPyEditorToolWidget(vipGetMainWindow());
		vipRegisterArchiveStreamOperators<VipPyEditorToolWidget*>();
	}
	return python;
}
