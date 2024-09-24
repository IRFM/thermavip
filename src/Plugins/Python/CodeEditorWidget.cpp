#include "CodeEditorWidget.h"

#include "VipDisplayArea.h"
#include "VipProgress.h"

#include <qsplitter.h>
#include <qboxlayout.h>
#include <qcoreapplication.h>
#include <qtoolbutton.h>
#include <qmenu.h>

#include "IPython.h"


class CodeEditorWidget::PrivateData
{
public:
	QPointer<QWidget> runningShell; //shell that runs the script, either a IOOperationWidget or a IPythonWidget
	QPointer< QObject> runningOperation; // Object reunning the code, either a PyIOOperation or a IPythonConsoleProcess
	PyEditor * editor;
	QSplitter * splitter;
	QToolButton* startButton;
	QString runFileCode;
	bool running;
	bool debug;

	QAction * start;
	QAction * stop;
	VipProgress * progress;
	QTimer timer;
};

CodeEditorWidget::CodeEditorWidget(QWidget * parent)
	:QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->progress = nullptr;
	
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

	//Restart interpreter when file execution finished (only for PyIOOperation)
	connect(this, SIGNAL(fileFinished()), this, SLOT(startInteractiveInterpreter()));
	
	d_data->editor = new PyEditor();

	d_data->splitter = new QSplitter(Qt::Vertical);
	d_data->running = false;
	d_data->debug = false;
	d_data->timer.setSingleShot(false);
	d_data->timer.setInterval(500);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(check()));

	d_data->splitter->addWidget(d_data->editor);
	d_data->splitter->setStretchFactor(0, 1);
	//d_data->splitter->addWidget(d_data->ioOperationWidget);


	d_data->editor->TabBar()->addSeparator();

	QToolButton* start = new QToolButton();
	start->setAutoRaise(true);
	start->setIcon(vipIcon("start_streaming.png"));
	start->setToolTip("Run file");
	start->setMenu(new QMenu());
	connect(start->menu()->addAction("Execute in internal console"),SIGNAL(triggered(bool)) ,this, SLOT(execInInternal()));
	connect(start->menu()->addAction("Execute in IPython console"), SIGNAL(triggered(bool)) , this, SLOT(execInIPython()));
	start->menu()->actions()[0]->setCheckable(true);
	start->menu()->actions()[1]->setCheckable(true);
	if (GetPyOptions()->launchCode() == PyOptions::InLocalInterp)
		start->menu()->actions()[0]->setChecked(true);
	else
		start->menu()->actions()[1]->setChecked(true);
	start->setPopupMode(QToolButton::MenuButtonPopup);
	d_data->startButton = start;

	d_data->start = d_data->editor->TabBar()->addWidget(start);
	d_data->stop = d_data->editor->TabBar()->addAction(vipIcon("stop.png"), "Stop running");
	d_data->stop->setEnabled(false);

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(d_data->splitter);
	setLayout(lay);

	connect(d_data->startButton->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToDisplayLaunchMode()));
	connect(d_data->startButton, SIGNAL(clicked(bool)), this, SLOT(execFile(bool)));
	connect(d_data->stop, SIGNAL(triggered(bool)), this, SLOT(stopFile()));
	
	d_data->editor->NewFile();
	
}

CodeEditorWidget::~CodeEditorWidget()
{
	d_data->timer.stop();
	disconnect(&d_data->timer, SIGNAL(timeout()), this, SLOT(check()));
	stopFile(true);
	QCoreApplication::removePostedEvents(this);
	QCoreApplication::removePostedEvents(&d_data->timer);
}

void CodeEditorWidget::aboutToDisplayLaunchMode()
{
	if (GetPyOptions()->launchCode() == PyOptions::InLocalInterp)
		execInInternal();
	else
		execInIPython();
}

void CodeEditorWidget::execInInternal()
{
	d_data->startButton->menu()->actions()[0]->setChecked(true);
	d_data->startButton->menu()->actions()[1]->setChecked(false);
	GetPyOptions()->setLaunchCode(PyOptions::InLocalInterp);
}
void CodeEditorWidget::execInIPython()
{
	d_data->startButton->menu()->actions()[0]->setChecked(false);
	d_data->startButton->menu()->actions()[1]->setChecked(true);
	GetPyOptions()->setLaunchCode(PyOptions::InIPythonInterp);
}

void CodeEditorWidget::startInteractiveInterpreter()
{
	if (qobject_cast<PyIOOperation*>(d_data->runningOperation.data())) {
		if (!GetPyOptions()->isRunning()) {
			GetPyOptions()->pyIOOperation(true);
		}
	}
}

PyEditor * CodeEditorWidget::editor() const
{
	return d_data->editor;
}

QWidget* CodeEditorWidget::shellWidget() const
{
	return d_data->runningShell;
}
QObject* CodeEditorWidget::interpreter() const
{
	return d_data->runningOperation;
}


bool CodeEditorWidget::isFileRunning() const
{
	return d_data->running;
}
bool CodeEditorWidget::isDebugging() const
{
	return d_data->running && d_data->debug;
}

bool CodeEditorWidget::isRunning()
{
	//Check if PyIOOperation still running
	if (qobject_cast<PyIOOperation*>(d_data->runningOperation.data())) {
		if (!GetPyOptions()->isRunning())
			return false;
		PyIOOperation::command_type c = GetPyOptions()->retrieveObject("_vip_stop");
		QVariant v = GetPyOptions()->wait(c, 5);
		if (v.canConvert<int>() && v.toInt() == 1)
			return false;
		if (d_data->runningOperation != GetPyOptions()->pyIOOperation())
			return false;
		return true;
	}
	//Check if IPythonWidget is still running
	else {
		if (IPythonWidget* w = qobject_cast<IPythonWidget*>(d_data->runningShell)) {
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

void CodeEditorWidget::check()
{
	bool running = true;

	if (d_data->runningShell)
	{
		//Check if file is still running
		if (d_data->progress && d_data->progress->canceled())
			stopFile(false);

		if (d_data->running && !isRunning()) {
			running = false;
		}
	}
	else {
		running = false;
	}

	if(!running){
		//no running shell, stop
		d_data->running = 0;
		d_data->debug = 0;
		d_data->timer.stop();
		d_data->stop->setEnabled(false);
		d_data->start->setEnabled(true);

		if (d_data->progress) {
			delete d_data->progress;
			d_data->progress = nullptr;
		}

		Q_EMIT fileFinished();
	}
	
}

void CodeEditorWidget::keyPressEvent(QKeyEvent * evt)
{
	if (evt->key() == Qt::Key_F5)
		execFile(false);
}


#include "VipProcessingObject.h"
#include "PyProcess.h"

void CodeEditorWidget::execFile(bool show_progress)
{
	stopFile();

	
	QWidget* exec_in_shell = nullptr;
	if (GetPyOptions()->launchCode() == PyOptions::InIPythonInterp) {
		if(GetIPythonToolWidget() && GetIPythonToolWidget()->widget()->count())
			exec_in_shell = qobject_cast<IPythonWidget*>(GetIPythonToolWidget()->widget()->currentWidget());
	}
	if (!exec_in_shell) {
		exec_in_shell = pyGetPythonInterpreter()->Interpreter();
	}
	d_data->runningShell = exec_in_shell;
	if (!exec_in_shell)
		return;

	IPythonWidget* ipython = qobject_cast<IPythonWidget*>(exec_in_shell);

	if (!ipython && qobject_cast<PyProcess*>(GetPyOptions()->pyIOOperation())) {
		//for external python process (PyProcess), make sure to register all new VipIODevice first
		//because a call to PyProcessing::setStdPyProcessingFile will freeze the GUI (try to execute a python command while the
		//python file is running -> deadlock)
		VipProcessingObject::allObjects();

	}
	
	if (CodeEditor * ed = d_data->editor->CurrentEditor())
	{
		d_data->editor->Save();
		QString file = ed->FileInfo().canonicalFilePath();
		if (!file.isEmpty()) {

			if (show_progress) {
				d_data->progress = new VipProgress();
				d_data->progress->setText("<b>Exec file </b>" + ed->FileInfo().fileName());
				d_data->progress->setCancelable(true);
			}

			//NEW
			
			file.replace("\\", "/");
			d_data->running = 1;
			d_data->debug = 0;

			// Execute in global python interpreter
			if (!ipython) {
				GetPyOptions()->wait(GetPyOptions()->execCode("_vip_stop=0"), 1000);
				GetPyOptions()->wait(GetPyOptions()->execCode(d_data->runFileCode), 1000);
				d_data->runningOperation = GetPyOptions()->pyIOOperation();
				pyGetPythonInterpreter()->Interpreter()->ExecCommand("runFile('" + file + "')");
				pyGetPythonInterpreter()->show();
				pyGetPythonInterpreter()->raise();
			}
			//Execute in IPython
			else {
				if (ipython->process()->isRunningCode()) {
					//do not run file if the ipython console is already running something
					d_data->running = 0;
					d_data->debug = 0;
					return;
				}
				ipython->process()->execCode("_vip_stop=0");
				ipython->process()->execCode(d_data->runFileCode);
				d_data->runningOperation = ipython->process();
				GetIPythonToolWidget()->show();
				GetIPythonToolWidget()->raise();
				//exec in separate thread
				//std::thread(std::bind(&IPythonConsoleProcess::execLine, ipython->process(), ("runFile('" + file + "')"))).detach();
				ipython->process()->execLineNoWait("runFile('" + file + "')");
			}
			d_data->timer.start();
			d_data->stop->setEnabled(true);
			d_data->start->setEnabled(false);
		}
	}
}
void CodeEditorWidget::debugFile()
{
	
}

void CodeEditorWidget::stopFile(bool wait)
{
	if (d_data->progress) {
		delete d_data->progress;
		d_data->progress = nullptr;
	}
	if (d_data->running) {
		
		//Stop PyIOOperation
		if (qobject_cast<PyIOOperation*>(d_data->runningOperation.data())) {
			GetPyOptions()->stop(wait);
		}
		//Stop IPythonWidget
		else {
			if (IPythonWidget* w = qobject_cast<IPythonWidget*>(d_data->runningShell))
				w->restartProcess();
		}

		//d_data->running = 0;
		//d_data->debug = 0;
		/*d_data->stop->setEnabled(false);
		d_data->start->setEnabled(true);
		d_data->timer.stop();*/
		
		//d_data->ioOperation->stop(wait);

		//if(wait)
		//	d_data->ioOperation->start();
		//Q_EMIT fileFinished();
	}
}
void CodeEditorWidget::nextStep()
{

}
void CodeEditorWidget::stepIn()
{

}
void CodeEditorWidget::stepOut()
{

}
void CodeEditorWidget::pause()
{

}
void CodeEditorWidget::_continue()
{

}


#include "VipArchive.h"
static VipArchive & operator<<(VipArchive & arch, const CodeEditorToolWidget * w)
{
	return arch.content("state", w->editor()->editor()->SaveState());
}
static VipArchive & operator>>(VipArchive & arch, CodeEditorToolWidget * w)
{
	QByteArray ar = arch.read("state").toByteArray();
	if (ar.size())
		w->editor()->editor()->RestoreState(ar);
	return arch;
}



PYTHON_EXPORT CodeEditorToolWidget * GetCodeEditorToolWidget()
{
	static CodeEditorToolWidget *python = nullptr;
	if (!python) {
		python = new CodeEditorToolWidget(vipGetMainWindow());
		vipRegisterArchiveStreamOperators<CodeEditorToolWidget*>();
	}
	return python;
}
