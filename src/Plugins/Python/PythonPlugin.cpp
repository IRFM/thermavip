/**
 *
 *  Python library initialization
 *
 *
 */


#include "PythonPlugin.h"
#include "PyHighlighter.h"
#include "PyProcessing.h"
#include "PySignalFusionProcessing.h"
#include "PyRegisterProcessing.h"
#include "CodeEditorWidget.h"
#include "CodeEditor.h"
#include "CurveFit.h"
#include "CustomizePlayer.h"
#include "VipToolWidget.h"
#include "VipLogging.h" 
#include "IOOperationWidget.h"
#include "VipDisplayArea.h"
#include "VipArchive.h"
#include "VipOptions.h"
#include "PyOperation.h"
#include "PyGenerator.h"
#include "VipMimeData.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipProcessingObjectEditor.h"
#include "VipGui.h"

#include <QToolBar>
#include <qlineedit.h>
#include <qtoolbutton.h>
#include <qradiobutton.h>
#include <qgridlayout.h>
#include <qfiledialog.h>
#include <qdesktopservices.h>
#include <qgroupbox.h>

static QGroupBox* createGroup(const QString& label)
{
	QGroupBox* res = new QGroupBox(label);
	res->setFlat(true);
	QFont f = res->font();
	f.setBold(true);
	res->setFont(f);
	return res;
}

PythonParameters::PythonParameters()
	:VipPageOption()
{
	this->setWindowTitle("Python environment options");

	pythonPathLabel = new QLabel("Python executable");
	pythonPath = new VipFileName(this);
	pythonPath->setMode(VipFileName::Open);
	pythonPath->setFilename("python");

	wdPathLabel = new QLabel("Working directory");
	wdPath = new VipFileName(this);
	wdPath->setMode(VipFileName::OpenDir);
	openWD = new QToolButton(this);
	openWD->setAutoRaise(true);
	openWD->setIcon(vipIcon("open.png"));
	openWD->setToolTip("Open working directory in a file browser");

	openProcManager = new QToolButton();
	openProcManager->setAutoRaise(true);
	openProcManager->setIcon(vipIcon("tools.png"));
	openProcManager->setToolTip("Open custom Python processing manager");

	openPythonData = new QToolButton();
	openPythonData->setIcon(vipIcon("open.png"));
	openPythonData->setAutoRaise(true);
	openPythonDataScripts = new QToolButton();
	openPythonDataScripts->setIcon(vipIcon("open.png"));
	openPythonDataScripts->setAutoRaise(true);


	startupCode = new PyEditor(Qt::Horizontal,this);
	startupCode->NewFile();

	style = new PyEditor(Qt::Horizontal, this);
	style->NewFile();
	style->TabBar()->hide();
	styleBox = new QComboBox();
	styleBox->addItems( CodeEditor::colorSchemesNames("Python"));
	QString quote = QChar('"'); quote = quote.repeated(3);
	style->CurrentEditor()->setPlainText(quote + "A string" + quote + "\n"
		"# A comment\n"
		"class Foo(object) :\n"
		"    def __init__(self) :\n"
		"        bar = 42\n"
		"        print(bar)");

	local = new QRadioButton("Use embeded Python interpreter", this);
	distant = new QRadioButton("Use your own Python installation", this);
	local->setChecked(true);

	launchInLocal = new QRadioButton("Launch script in internal interpreter", this);
	launchInIPython = new QRadioButton("Launch scripts in IPython interpreter (if available)", this);
	launchInIPython->setChecked(true);

	restart = new QToolButton();
	restart->setAutoRaise(true);
	restart->setIcon(vipIcon("restart.png"));
	restart->setToolTip("Restart Python interpreter");


	int row = 0;
	QGridLayout * lay = new QGridLayout();
	lay->setSpacing(5);

	QGroupBox* intern = createGroup("Internal interpreter");
	{
		QVBoxLayout *v = new QVBoxLayout();
		v->addWidget(local);
		v->addWidget(distant);
		intern->setLayout(v);
	}

	//NEW: for now, just hide this option
	intern->hide();

	lay->addWidget(intern, row++, 0, 1, 2);
	//lay->addWidget(local, row++, 0,1,2);
	//lay->addWidget(distant, row++, 0,1,2);


	QGroupBox* launch = createGroup("Launch scripts");
	{
		QVBoxLayout* v = new QVBoxLayout();
		v->addWidget(launchInLocal);
		v->addWidget(launchInIPython);
		launch->setLayout(v);
	}
	lay->addWidget(launch, row++, 0, 1, 2);
	//lay->addWidget(launchInLocal, row++, 0, 1, 2);
	//lay->addWidget(launchInIPython, row++, 0, 1, 2);

	lay->addWidget(createGroup("External Python"), row++, 0, 1, 2);
	
	lay->addWidget(pythonPathLabel, row, 0);
	lay->addWidget(pythonPath, row++, 1);

	QHBoxLayout * h = new QHBoxLayout();
	h->setContentsMargins(0, 0, 0, 0);
	h->setSpacing(0);
	h->addWidget(wdPath);
	h->addWidget(openWD);

	lay->addWidget(wdPathLabel, row, 0);
	lay->addLayout(h, row++, 1);

	lay->addWidget(createGroup("Custom processing/directory management"), row++, 0, 1, 2);

	//add proc manager
	{
		QHBoxLayout * h = new QHBoxLayout();
		h->addWidget(openProcManager);
		h->addWidget(new QLabel("Open custom Python processing manager"));
		lay->addLayout(h, row++,0,1,2);
	}

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->addLayout(lay);

	{
		QHBoxLayout * hlay = new QHBoxLayout();
		hlay->addWidget(openPythonData);
		hlay->addWidget(new QLabel("Open custom Python processing directory"));

		QHBoxLayout * hlay2 = new QHBoxLayout();
		hlay2->addWidget(openPythonDataScripts);
		hlay2->addWidget(new QLabel("Open custom Python scripts directory"));

		vlay->addLayout(hlay);
		vlay->addLayout(hlay2);
		vlay->addWidget(createGroup("Interpreters startup code"));
	}


	/*QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addWidget(restart);
	hlay->addWidget(new QLabel("Restart Python interpreter"));
	vlay->addLayout(hlay);*/

	vlay->addWidget(startupCode, 3);
	actStartupCode = new QAction(nullptr);
	actStartupCode->setIcon(vipIcon("apply.png"));
	actStartupCode->setText("Apply startup code");
	startupCode->TabBar()->insertAction(startupCode->TabBar()->actions().first(), actStartupCode);

	QHBoxLayout * slay = new QHBoxLayout();
	slay->setContentsMargins(0, 0, 0, 0);
	slay->addWidget(createGroup("Code editor style"));//new QLabel("Code editor style"));
	slay->addWidget(styleBox);
	vlay->addLayout(slay);
	vlay->addWidget(style, 3);
	style->CurrentEditor()->setReadOnly(true);
	
	setLayout(vlay);

	connect(openWD, SIGNAL(clicked(bool)), this, SLOT(openWorkingDirectory()));
	connect(restart, SIGNAL(clicked(bool)), this, SLOT(restartInterpreter()));
	connect(actStartupCode, SIGNAL(triggered(bool)), this, SLOT(applyStartupCode()));
	connect(styleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeStyle()));
	connect(openProcManager, SIGNAL(clicked(bool)), this, SLOT(openManager()));

	connect(openPythonData, SIGNAL(clicked(bool)), this, SLOT(_openPythonData()));
	connect(openPythonDataScripts, SIGNAL(clicked(bool)), this, SLOT(_openPythonDataScripts()));
}

void PythonParameters::applyPage()
{
#ifdef VIP_ENABLE_PYTHON_LINK
	/*if (local->isChecked())
		GetPyOptions()->setPyType(PyOptions::Local);
	else
		GetPyOptions()->setPyType(PyOptions::Distant);*/
	//NEW: for now always use Local
	GetPyOptions()->setPyType(PyOptions::Local);
#else
	GetPyOptions()->setPyType(PyOptions::Distant);
#endif

	if (launchInLocal->isChecked())
		GetPyOptions()->setLaunchCode(PyOptions::InLocalInterp);
	else
		GetPyOptions()->setLaunchCode(PyOptions::InIPythonInterp);

	GetPyOptions()->setPython(pythonPath->filename());
	GetPyOptions()->setWorkingDirectory(wdPath->filename());
	if(startupCode->CurrentEditor())
	GetPyOptions()->setStartupCode(startupCode->CurrentEditor()->toPlainText());

	//make sure to recreate the interpreter
	GetPyOptions()->isRunning();

	/*if (GetPyOptions()->isRunning())
		return ParametersPage::Success;
	else
		return ParametersPage::Fail;*/

	CodeEditor::setStdColorSchemeForType("Python", styleBox->currentText());
}

void PythonParameters::updatePage()
{
	if (GetPyOptions()->pyType() == PyOptions::Local)
		local->setChecked(true);
	else
		distant->setChecked(true);

	if (GetPyOptions()->launchCode() == PyOptions::InLocalInterp)
		launchInLocal->setChecked(true);
	else
		launchInIPython->setChecked(true);

	pythonPath->setFilename(GetPyOptions()->python());
	wdPath->setFilename(GetPyOptions()->workingDirectory());
	startupCode->CurrentEditor()->setPlainText(GetPyOptions()->startupCode());
	
	if (BaseHighlighter * h = CodeEditor::stdColorSchemeForType("Python"))
		styleBox->setCurrentText(h->name);
}

void PythonParameters::changeStyle()
{
	if (BaseHighlighter * h = CodeEditor::colorScheme("Python", styleBox->currentText()))
	{
		style->CurrentEditor()->setColorScheme(h);
	}
}

void PythonParameters::restartInterpreter()
{
	PyIOOperation * py = GetPyOptions()->pyIOOperation(true);
	if (!py || !py->isRunning())
	{
		VIP_LOG_ERROR("Failed to restart Python interpreter");
	}
}

void PythonParameters::openWorkingDirectory()
{
	if (QFileInfo(wdPath->filename()).exists())
	{
		QDesktopServices::openUrl(QFileInfo(wdPath->filename()).canonicalPath());
	}
}

void PythonParameters::openManager()
{
	PyRegisterProcessing::openProcessingManager();
}

void PythonParameters::applyStartupCode()
{
	if (GetPyOptions()->isRunning())
	{
		QVariant v = GetPyOptions()->wait(GetPyOptions()->execCode(startupCode->CurrentEditor()->toPlainText()));
		PyError err = v.value<PyError>();
		if (!err.isNull()) {
			VIP_LOG_ERROR(err.traceback);
		}
	}
}

void PythonParameters::_openPythonData()
{
	QDesktopServices::openUrl(vipGetPythonDirectory());
}
void PythonParameters::_openPythonDataScripts()
{
	QDesktopServices::openUrl(vipGetPythonScriptsDirectory());
}

PythonParameters * GetPythonParameters()
{
	static PythonParameters * params = new PythonParameters();
	return params;
}






 

static void createPyGenerator()
{
	if (VipIODevice * dev = PySignalGeneratorEditor::createGenerator(false))
	{
		vipGetMainWindow()->openDevices(QList<VipIODevice*>() << dev, nullptr, nullptr);
	}
}

static void createComplexPyGenerator()
{
	if (VipIODevice * dev = PySignalGeneratorEditor::createGenerator(true))
	{
		vipGetMainWindow()->openDevices(QList<VipIODevice*>() << dev, nullptr, nullptr);
	}
}







//static VipToolWidget * python = nullptr;

#include "PyProcess.h"

/*void test_pyprocess()
{
	PyProcess p;
	p.start();

	QVariant v1 = p.evalCode("import numpy as np");
	vip_debug("%s\n", v1.value<PyError>().traceback.toLatin1().data());
	QVariant v2 = p.evalCode("np.sin(2)");
	vip_debug("%s\n", v2.value<PyError>().traceback.toLatin1().data());
	double yy = v2.toDouble();
	

	//VipNDArray array(QMetaType::UShort, vipVector(1080, 720));
	VipArchiveReader r;
	r.setPath("D:/data/movies/AEF21_temp_20170919.0_08.33.34.652_IRCam_Caleo768kL_0904.arch");
	bool op = r.open(VipIODevice::ReadOnly);
	op = r.read(r.firstTime());
	VipNDArray array = r.outputAt(0)->data().value<VipNDArray>();

	QByteArray tmp = variantToBytes(QVariant::fromValue(array));

	qint64 st = QDateTime::currentMSecsSinceEpoch();
	tmp = qCompress(tmp);
	qint64 el = QDateTime::currentMSecsSinceEpoch() -st;
	vip_debug("compressed: %i %i, %i ms\n",array.dataSize()*array.size(), tmp.size(), (int)el);

	for (int i = 0; i < 15; ++i)
	{
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		p.sendObject("i", QVariant::fromValue(array));
		QVariant v = p.wait(p.retrieveObject("i"));
		const VipNDArray ar = v.value<VipNDArray>();
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		//vip_debug("'%s'\n", v.toString().toLatin1().data());
		vip_debug("elapsed: %i\n", (int)el);
		vip_debug("shape: (%i, %i)\n", ar.shape(0), ar.shape(1));
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		QVariant v = p.wait(p.sendObject("i", QVariant::fromValue(array)));
		PyError err = v.value<PyError>();
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		vip_debug("elapsed: %i\n", (int)el);
		if (!err.isNull()) 
			vip_debug("error: %s\n", err.traceback.toLatin1().data());
		else
		{
			st = QDateTime::currentMSecsSinceEpoch();
			v = p.wait(p.retrieveObject("i"));
			PyError err = v.value<PyError>();
			qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
			vip_debug("elapsed: %i\n", (int)el);
			if (!err.isNull())
				vip_debug("error: %s\n", err.traceback.toLatin1().data());
			else
			{
				const VipNDArray ar = v.value<VipNDArray>();
				vip_debug("shape: (%i, %i)\n",ar.shape(0),ar.shape(1));
			}
		}
	}

	p.startInteractiveInterpreter();
	p.sendObject("i", 3);
	p.execCode("i=i*3");
	QVariant v = p.wait(p.retrieveObject("i"));
	PyError err = v.value<PyError>();
	vip_debug("%s\n", err.traceback.toLatin1().data());
	int hh = v.toInt();

	bool stop = true;
}*/


static PythonInterface * _interface;

static void updatePlotPlayer(VipPlotPlayer * player)
{
	if (player && !player->property("PyPlotPlayer").toBool())
		new PyPlotPlayer(player);
}


bool PyFileHandler::open(const QString & path, QString * error)
{
	QFileInfo info(path);
	if (!info.exists() || info.isDir()) {
		if (error) *error = "Unknown file '" + path + "'";
		return false;
	}

	GetCodeEditorToolWidget()->editor()->editor()->OpenFile(path);
	GetCodeEditorToolWidget()->show();
	GetCodeEditorToolWidget()->raise();
	return true;
}

//#include "SharedMemory.h"














/**
Class used to handle external python requests
*/
/*class ExternalPython : public QThread
{
	bool d_stop;
	SharedMemory d_mem;
	PyLocal d_loc;
	ExternalPython()
		:QThread(), d_stop(false), d_mem("Thermavip", 50000000, true)
	{
		if (d_mem.isValid()) {
			d_loc.start();
			start();
		}
	}

public:
	~ExternalPython() {
		d_stop = true;
		d_loc.stop();
		wait();
	}

	static ExternalPython& instance() {
		static ExternalPython inst;
		return inst;
	}

protected:

	bool sendObject(const QString& name, qint64 timeout = -1, QString* error = nullptr)
	{
		if (error)
			error->clear();
		QString code =
			"import pickle\n"
			"import struct\n"
			"__res = b'" SH_OBJECT "' +struct.pack('i',len('" + name + "')) + b'" + name + "' + pickle.dumps(" + name + ")";

		PyError err = d_loc.wait(d_loc.execCode(code)).value<PyError>();
		if (!err.isNull()) {
			if (error)
				*error = err.traceback;
			vip_debug("%s\n", err.traceback.toLatin1().data());
			return false;
		}

		//send result
		QVariant v = d_loc.wait(d_loc.retrieveObject("__res"));
		QByteArray tmp = v.toByteArray();
		d_mem.write(tmp.data(), tmp.size(), timeout);
		return true;
	}

	void sendError(const QString& err, qint64 timeout = -1)
	{
		d_loc.wait(d_loc.sendObject("__err", err));
		QString code =
			"import struct\n"
			"__res = b'" SH_ERROR_TRACE "' +struct.pack('i',len(__err)) + __err.encode()";

		PyError pyerr = d_loc.wait(d_loc.execCode(code)).value<PyError>();
		if (!pyerr.isNull()) {
			vip_debug("%s\n", pyerr.traceback.toLatin1().data());
			return;
		}
		//send result
		QVariant v = d_loc.wait(d_loc.retrieveObject("__res"));
		QByteArray tmp = v.toByteArray();
		d_mem.write(tmp.data(), tmp.size(), timeout);
	}

	virtual void run()
	{
		qint64 timeout = 1000;

		while (!d_stop) {

			QByteArray ar;
			bool r = d_mem.read(ar, timeout);
			if (r && ar.size()) {

				//interpret read value

				if (ar.startsWith(SH_EXEC_FUN)) {
					//execute internal python function
					ar = ar.mid(strlen(SH_EXEC_FUN));
					QDataStream str(ar);
					str.setByteOrder(QDataStream::LittleEndian);
					int s1 = 0, s2 = 0, s3 = 0;
					str >> s1 >> s2 >> s3; //read function name len, tuple args len, dict args len
					if (!s1 || !s2 || !s3) {
						continue;
					}
					//send pickle versions of variables. name is already the ascii function name.
					QByteArray name(s1, 0), targs(s2, 0), dargs(s3, 0);
					str.readRawData(name.data(), name.size());
					str.readRawData(targs.data(), targs.size());
					str.readRawData(dargs.data(), dargs.size());
					d_loc.sendObject("__targs", targs);
					d_loc.sendObject("__dargs", dargs);

					QString code =
						"import pickle\n"
						"import struct\n"
						"__targs = pickle.loads(__targs)\n"
						"__dargs = pickle.loads(__dargs)\n"
						"__res = " + name + "(*__targs, **__dargs)\n";
					PyError err = d_loc.wait(d_loc.execCode(code)).value<PyError>();
					if (!err.isNull()) {
						vip_debug("%s\n", err.traceback.toLatin1().data());
						sendError(err.traceback, timeout);
						continue;
					}
					QString error;
					if (!sendObject("__res", timeout, &error)) {
						sendError(error, timeout);
					}
				}
			}
		}
	}
};*/

#include "IPython.h"
#include <functional>
PythonInterface::LoadResult PythonInterface::load()
{
	//Start the ExternalPython instance to handle external Python requests
	//ExternalPython::instance();
	//testSharedMemory();

	

	//load the PySignalFusionProcessing
	PyRegisterProcessing::loadCustomProcessings(true);


	_interface = this;
	
	//test_pyprocess();
	VipFDAddProcessingAction().append<bool(QAction*,VipPlotPlayer*)>(dispatchCurveFit);
	VipFDAddProcessingAction().append<bool(QAction*, VipPlotPlayer*)>(dispatchPySignalFusion);
	

	vipGetMainWindow()->addDockWidget(Qt::BottomDockWidgetArea, pyGetPythonInterpreter());
	pyGetPythonInterpreter()->setFloating(true);
	pyGetPythonInterpreter()->hide();

	QAction * pyaction = vipGetMainWindow()->toolsToolBar()->addAction(vipIcon("python.png"),"Show/hide Python console");
	pyGetPythonInterpreter()->setAction(pyaction);

	vipGetMainWindow()->addDockWidget(Qt::LeftDockWidgetArea, GetCodeEditorToolWidget());
	GetCodeEditorToolWidget()->setFloating(true);
	GetCodeEditorToolWidget()->hide();

	 
	showEditor = new QToolButton();
	showEditor->setIcon(vipIcon("start_streaming.png"));
	showEditor->setToolTip("Show/hide Python code editor");
	showEditor->setAutoRaise(true); 
	showEditor->setMenu(new QMenu(showEditor));
	showEditor->setPopupMode(QToolButton::MenuButtonPopup);
	/*QAction * pyactionEditor =*/ vipGetMainWindow()->toolsToolBar()->addWidget(showEditor);
	GetCodeEditorToolWidget()->setButton(showEditor);
	connect(showEditor->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToShowScripts()));
	connect(showEditor->menu(), SIGNAL(triggered(QAction*)), this, SLOT(scriptTriggered(QAction*)));
	
	initPython(); 
	 
	//TEST
	/*IPythonConsoleProcess* p = new IPythonConsoleProcess();
	qint64 pid = p->start();

	vip_debug("'%s'\n", p->lastError().toLatin1().data());
	
	p->sendObject("toto", 3.45);
	vip_debug("'%s' '%s'\n", p->readAllStandardOutput().data(), p->readAllStandardError().data());
	p->sendObject("tutu", 6);
	vip_debug("'%s' '%s'\n", p->readAllStandardOutput().data(), p->readAllStandardError().data());
	p->sendObject("tata", 6);
	vip_debug("'%s' '%s'\n", p->readAllStandardOutput().data(), p->readAllStandardError().data());
	p->isRunningCode();
	p->sendObject("ok", 9);
	vip_debug("'%s' '%s'\n", p->readAllStandardOutput().data(), p->readAllStandardError().data());
	std::thread(std::bind(&IPythonConsoleProcess::execCode, p, QString("_run=2\nwhile True: pass\n_run=1"))).detach();
	QThread::msleep(5000);
	p->start();
	p->sendObject("_run", 3); 
	QVariant v = p->retrieveObject("_run"); 
	int uu = v.toInt();
	vip_debug("%s\n", v.toByteArray().data());
	vip_debug("%s\n", p->lastError().toLatin1().data());
	bool r = p->execCode("uu=9");  
	vip_debug("%s\n", p->lastError().toLatin1().data());
	r = p->execLine("for i in range(10):\n    print(i)");
	vip_debug("%s\n", p->lastError().toLatin1().data()); 
	r = p->execLine("while True: print(hjk)");
	p->restart();
	r = p->execLine("while True: print(hjk)");
	p->restart(); 
	delete p; */
	
	
	
	
	vipGetOptions()->addPage("Python", GetPythonParameters());
	  
	//add the generators 
	
	QAction * generator = vipGetMainWindow()->generateMenu()->addAction("Generate signal from Python expression...");
	generator->setToolTip("Create a streaming/temporal video or plot from a single line Python expression");
	connect(generator, &QAction::triggered, this, createPyGenerator);
	//make the menu action droppable
	generator->setProperty("QMimeData", QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipIODevice*>(
		PySignalGeneratorEditor::createSimpleGenerator,
		VipCoordinateSystem::Cartesian,
		generator
		)));

	QAction * complex_generator = vipGetMainWindow()->generateMenu()->addAction("Generate signal from Python script...");
	complex_generator->setToolTip("Create a streaming/temporal video or plot from a Python script");
	connect(complex_generator, &QAction::triggered, this, createComplexPyGenerator);
	//make the menu action droppable
	complex_generator->setProperty("QMimeData", QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipIODevice*>(
		PySignalGeneratorEditor::createComplexGenerator,
		VipCoordinateSystem::Cartesian,
		complex_generator
		)));

	//register all files found in the Python directory
	PyAddProcessingDirectory(vipGetPythonDirectory(), QString());
	PyAddProcessingDirectory("./Python", QString());

	//register PyPlotPlayer
	vipFDPlayerCreated().append<void(VipPlotPlayer*)>(updatePlotPlayer);
	vipFDPlayerCreated().append<void(VipAbstractPlayer*)>(customizePlayer);

	vipAddUninitializationFunction(uninitPython);


	BaseHighlighter * h = CodeEditor::stdColorSchemeForType("Python");
	if (isDarkSkin() ) {
		setIPythonStyle("monokai");
		if(!isDarkColor(h->backgroundColor()))
			CodeEditor::setStdColorSchemeForType("Python", "Spyder Dark");
	}
	else {
		if(isDarkColor(h->backgroundColor()))
			CodeEditor::setStdColorSchemeForType("Python", "Pydev");
	}
	setIPythonFontSize(VipGuiDisplayParamaters::instance()->defaultEditorFont().pointSize());

	//Initialize global shared memory
	QString smem_name = pyGlobalSharedMemoryName();
	//Set the shared memory name in thermavip title
	QString main_title = vipGetMainWindow()->mainTitle();
	main_title.replace("Thermavip", smem_name);
	vipGetMainWindow()->setMainTitle(main_title);

	// Initialize IPython tool widget
	if (IPythonToolWidget* twidget = GetIPythonToolWidget(vipGetMainWindow()))
	{
		vipGetMainWindow()->addDockWidget(Qt::BottomDockWidgetArea, twidget);
		GetIPythonToolWidget()->setFloating(false);
		GetIPythonToolWidget()->hide();
	}

	return Success;
}


void PythonInterface::unload()
{
	
	//We MUST delete the IPythonToolWidget ourself as it spawn processes which prevent from deleting ALL windows
	if (IPythonToolWidget* twidget = GetIPythonToolWidget())
		delete twidget;
} 


void PythonInterface::save(VipArchive & stream)
{
	PyOptions & opt = *GetPyOptions();
	stream.content("python", opt.python());
	stream.content("workingDirectory", opt.workingDirectory());
	stream.content("type", (int)opt.pyType());
	stream.content("launchCode", (int)opt.launchCode());
	stream.content("startup", opt.startupCode());
	stream.content("schemes", CodeEditor::stdColorSchemes());

	stream.content("editor", GetCodeEditorToolWidget());
}

void PythonInterface::restore(VipArchive & stream)
{
	PyOptions & opt = *GetPyOptions();

	int type = 0;
	int launchCode = PyOptions::InIPythonInterp;
	QString python = "python";
	//int  protocol = 3;
	QString workingDirectory;
	QMap<QString, QString> schemes;
	stream.content("python", python);
	stream.content("workingDirectory", workingDirectory);
	stream.content("type", type);

	
	//New in 3.3.6
	stream.save();
	if (!stream.content("launchCode", launchCode))
		stream.restore();
	
	QString startup = stream.read("startup").toString();
	stream.content("schemes", schemes);
	
	opt.setPython(python);
	opt.setWorkingDirectory(workingDirectory);
	//NEW: for now always use Local
	//opt.setPyType((PyOptions::PyType)type);
	opt.setPyType(PyOptions::Local);
	opt.setStartupCode(startup);
	opt.setLaunchCode((PyOptions::PyLaunchCode)launchCode);
	
	CodeEditor::setStdColorSchemes(schemes);
	BaseHighlighter* h = CodeEditor::stdColorSchemeForType("Python");
	//Make sure the Python scheme fits with the current skin
	if (isDarkSkin()) {
		if(!h || !isDarkColor(h->backgroundColor()))
			CodeEditor::setStdColorSchemeForType("Python", "Spyder Dark");
	}
	else {
		if(!h || isDarkColor(h->backgroundColor()))
			CodeEditor::setStdColorSchemeForType("Python", "Pydev");
	}

	GetPyOptions()->pyIOOperation();
	GetPythonParameters()->updatePage();
	stream.content("editor", GetCodeEditorToolWidget());

	//Restart IPython is python process is different
	if (python != "python") {
		if (IPythonToolWidget* twidget = GetIPythonToolWidget())
		{
			twidget->widget()->closeTab(0);
			twidget->widget()->addInterpreter();
		}
	}

}

void PythonInterface::applyCurveFit()
{
	if(QAction * act = qobject_cast<QAction*>(sender()) ) {
		if(VipPlotPlayer * pl = act->property("_vip_player").value<VipPlotPlayer*>())
			fitCurve(pl,act->property("_vip_fit").toString());
	}
}


static QList<VipProcessingObject *> applyPySignalFusion(VipPlotPlayer * pl)
{
	if (!pl)
		return QList<VipProcessingObject *>();

	PySignalFusionProcessing * p = new PySignalFusionProcessing();
	PySignalFusionProcessingEditor * ed = new PySignalFusionProcessingEditor();
	ed->buttons()->hide();
	ed->setPySignalFusionProcessing(p);
	ed->setPlotPlayer(pl);
	VipGenericDialog dial(ed, "Create Python signal fusion algorithm");
	while (dial.exec() == QDialog::Accepted)
	{
		//add the processing to the player
		if (ed->apply()) {
			if (!p->hasError()) {

				VipProcessingList * lst = new VipProcessingList();
				lst->inputAt(0)->setConnection(p->outputAt(0));
				lst->inputAt(0)->setData(p->outputAt(0)->data());
				lst->update();
				lst->setScheduleStrategy(VipProcessingList::Asynchronous, true);
				lst->setDeleteOnOutputConnectionsClosed(true);

				return QList<VipProcessingObject *>() << lst;
			}
		}
	}

	delete p;
	return QList<VipProcessingObject *>();
}

void PythonInterface::applyPySignalFusion()
{
	if (QAction * act = qobject_cast<QAction*>(sender())) {
		if (VipPlotPlayer * pl = act->property("_vip_player").value<VipPlotPlayer*>())
		{
			QList<VipProcessingObject*> lst = ::applyPySignalFusion(pl);
			if (lst.size()) {
				vipCreatePlayersFromProcessing(lst.first(), pl);

				vipGetProcessingEditorToolWidget()->show();
				vipGetProcessingEditorToolWidget()->setProcessingObject(lst.first()->inputAt(0)->connection()->source()->parentProcessing());
				QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);

			}
			return;

			PySignalFusionProcessing * p = new PySignalFusionProcessing();
			PySignalFusionProcessingEditor * ed = new PySignalFusionProcessingEditor();
			ed->buttons()->hide();
			ed->setPySignalFusionProcessing(p);
			ed->setPlotPlayer(pl);
			VipGenericDialog dial(ed, "Create Python signal fusion algorithm");
			while (dial.exec() == QDialog::Accepted)
			{
				//add the processing to the player
				if (ed->apply()) {
					if (!p->hasError()) {
						QString n = p->outputAt(0)->data().yUnit();
						vipCreatePlayersFromProcessing(p, pl);

						vipGetProcessingEditorToolWidget()->show();
						vipGetProcessingEditorToolWidget()->setProcessingObject(p);
						QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);

						return;
					}
				}
			}
			
			delete p;
		}
	} 
} 
 

bool PythonInterface::dispatchCurveFit(QAction * act, VipPlotPlayer * pl)
{
	if(act->text().startsWith("Fit")) {
		QString fit = act->text().mid(4);
		if(fit == "Linear" || fit == "Exponential"|| fit == "Polynomial"|| fit == "Gaussian") {
			act->setProperty("_vip_player",QVariant::fromValue(pl));
			act->setProperty("_vip_fit",fit);
			QObject::connect(act,SIGNAL(triggered(bool)),_interface,SLOT(applyCurveFit()));
			return true;
		}
	}
	return false;
}

bool  PythonInterface::dispatchPySignalFusion(QAction * act, VipPlotPlayer * pl)
{
	if (act->text().startsWith("Py Signal Fusion Processing")) {
		act->setProperty("_vip_player", QVariant::fromValue(pl));
		QObject::connect(act, SIGNAL(triggered(bool)), _interface, SLOT(applyPySignalFusion()));
		return true;
	}
	return false;
}

void PythonInterface::aboutToShowScripts()
{
	showEditor->menu()->clear();
	QDir dir(vipGetPythonScriptsDirectory());
	QFileInfoList lst= dir.entryInfoList(QStringList() << "*.py", QDir::Files);
	for (int i = 0; i < lst.size(); ++i)
		showEditor->menu()->addAction(lst[i].fileName())->setProperty("path", lst[i].canonicalFilePath());
}
void PythonInterface::scriptTriggered(QAction* act)
{
	QString path = act->property("path").toString();

	GetCodeEditorToolWidget()->editor()->editor()->OpenFile(path);
	GetCodeEditorToolWidget()->editor()->execFile(true);
}





PyPlotPlayer::PyPlotPlayer(VipPlotPlayer * pl)
	:QObject(pl)
{
	pl->setProperty("PyPlotPlayer", true);
	QAction * act = pl->advancedTools()->menu()->addAction(vipIcon("python.png"), "Create a Python data fusion processing");
	act->setProperty("_vip_player", QVariant::fromValue(pl));
	connect(act, SIGNAL(triggered(bool)), _interface, SLOT(applyPySignalFusion()));

	//make it draggable
	act->setProperty("QMimeData", QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipProcessingObject*> >(
		std::bind(&applyPySignalFusion, pl), VipCoordinateSystem::Cartesian,act)
		));
}

