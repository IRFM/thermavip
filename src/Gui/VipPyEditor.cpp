#include "VipPyEditor.h"

#include "VipDisplayArea.h"
#include "VipProgress.h"
#include "VipArchive.h"

#include <qsplitter.h>
#include <qboxlayout.h>
#include <qcoreapplication.h>
#include <qtoolbutton.h>
#include <qmenu.h>

class VipPyEditor::PrivateData
{
public:
	QPointer<QObject> runningOperation;
	QPointer<QWidget> runningShell;

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
	d_data->startButton = start;

	d_data->start = this->tabBar()->addWidget(start);
	d_data->stop = this->tabBar()->addAction(vipIcon("stop.png"), "Stop running");
	d_data->stop->setEnabled(false);

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

void VipPyEditor::startInteractiveInterpreter()
{
	if (qobject_cast<VipPyIOOperation*>(d_data->runningOperation.data())) {
		if (!VipPyInterpreter::instance()->isRunning()) {
			VipPyInterpreter::instance()->pyIOOperation(true);
		}
	}
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

	d_data->runningShell = vipPyGetPythonInterpreter()->shellWidget();

	if (VipTextEditor* ed = this->currentEditor()) {
		this->save();
		QString file = ed->fileInfo().canonicalFilePath();
		if (!file.isEmpty()) {

			file.replace("\\", "/");
			d_data->running = 1;
			d_data->debug = 0;

			// Execute in global python interpreter
			VipPyInterpreter::instance()->execCode("_vip_stop=0").wait(1000);
			VipPyInterpreter::instance()->execCode(d_data->runFileCode).wait(1000);
			d_data->runningOperation = VipPyInterpreter::instance()->pyIOOperation();
			vipPyGetPythonInterpreter()->shell()->execLine("runFile('" + file + "')");
			vipPyGetPythonInterpreter()->show();
			vipPyGetPythonInterpreter()->raise();

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

		vipPyGetPythonInterpreter()->shell()->stopCurrentCode();
		// Stop VipPyIOOperation
		//if (qobject_cast<VipPyIOOperation*>(d_data->runningOperation.data())) {
		//	VipPyInterpreter::instance()->stop(wait);
		//}
		

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
