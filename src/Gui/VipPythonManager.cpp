/**
 *
 *  Python library initialization
 *
 *
 */


#include "VipPythonManager.h"
#include "VipTextHighlighter.h"
#include "VipPyProcessing.h"
#include "VipPySignalFusionProcessing.h"
#include "VipPyRegisterProcessing.h"
#include "VipPyEditor.h"
#include "VipPyProcessingEditor.h"
#include "VipTextEditor.h"
#include "VipPyFitProcessing.h"
#include "VipToolWidget.h"
#include "VipLogging.h" 
#include "VipPyShellWidget.h"
#include "VipDisplayArea.h"
#include "VipArchive.h"
#include "VipOptions.h"
#include "VipPyOperation.h"
#include "VipPyGenerator.h"
#include "VipMimeData.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipProcessingObjectEditor.h"
#include "VipGui.h"
#include "VipPyIPython.h"
#include <functional>
#include <QToolBar>
#include <qlineedit.h>
#include <qtoolbutton.h>
#include <qradiobutton.h>
#include <qgridlayout.h>
#include <qfiledialog.h>
#include <qdesktopservices.h>
#include <qgroupbox.h>

VipPythonParameters::VipPythonParameters()
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

	startupCode = new VipTabEditor(Qt::Horizontal,this);
	startupCode->setDefaultColorSchemeType("Python");
	startupCode->newFile();

	style = new VipTabEditor(Qt::Horizontal, this);
	style->setDefaultColorSchemeType("Python");
	style->newFile();
	style->tabBar()->hide();
	styleBox = new QComboBox();
	styleBox->addItems( VipTextEditor::colorSchemesNames("Python"));
	QString quote = QChar('"'); quote = quote.repeated(3);
	style->currentEditor()->setPlainText(quote + "A string" + quote + "\n"
		"# A comment\n"
		"class Foo(object) :\n"
		"    def __init__(self) :\n"
		"        bar = 42\n"
		"        print(bar)");

	
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

	QGroupBox* launch = createOptionGroup("Launch scripts");
	{
		QVBoxLayout* v = new QVBoxLayout();
		v->addWidget(launchInLocal);
		v->addWidget(launchInIPython);
		launch->setLayout(v);
	}
	lay->addWidget(launch, row++, 0, 1, 2);
	lay->addWidget(createOptionGroup("External Python"), row++, 0, 1, 2);
	lay->addWidget(pythonPathLabel, row, 0);
	lay->addWidget(pythonPath, row++, 1);

	QHBoxLayout * h = new QHBoxLayout();
	h->setContentsMargins(0, 0, 0, 0);
	h->setSpacing(0);
	h->addWidget(wdPath);
	h->addWidget(openWD);

	lay->addWidget(wdPathLabel, row, 0);
	lay->addLayout(h, row++, 1);

	lay->addWidget(createOptionGroup("Custom processing/directory management"), row++, 0, 1, 2);

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
		vlay->addWidget(createOptionGroup("Interpreters startup code"));
	}

	vlay->addWidget(startupCode, 3);
	actStartupCode = new QAction(nullptr);
	actStartupCode->setIcon(vipIcon("apply.png"));
	actStartupCode->setText("Apply startup code");
	startupCode->tabBar()->insertAction(startupCode->tabBar()->actions().first(), actStartupCode);

	QHBoxLayout * slay = new QHBoxLayout();
	slay->setContentsMargins(0, 0, 0, 0);
	slay->addWidget(createOptionGroup("Code editor style")); // new QLabel("Code editor style"));
	slay->addWidget(styleBox);
	vlay->addLayout(slay);
	vlay->addWidget(style, 3);
	style->currentEditor()->setReadOnly(true);
	
	setLayout(vlay);

	connect(openWD, SIGNAL(clicked(bool)), this, SLOT(openWorkingDirectory()));
	connect(restart, SIGNAL(clicked(bool)), this, SLOT(restartInterpreter()));
	connect(actStartupCode, SIGNAL(triggered(bool)), this, SLOT(applyStartupCode()));
	connect(styleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeStyle()));
	connect(openProcManager, SIGNAL(clicked(bool)), this, SLOT(openManager()));

	connect(openPythonData, SIGNAL(clicked(bool)), this, SLOT(_openPythonData()));
	connect(openPythonDataScripts, SIGNAL(clicked(bool)), this, SLOT(_openPythonDataScripts()));
}

void VipPythonParameters::applyPage()
{
	if (launchInLocal->isChecked())
		VipPyInterpreter::instance()->setLaunchCode(VipPyInterpreter::InLocalInterp);
	else
		VipPyInterpreter::instance()->setLaunchCode(VipPyInterpreter::InIPythonInterp);

	VipPyInterpreter::instance()->setPython(pythonPath->filename());
	VipPyInterpreter::instance()->setWorkingDirectory(wdPath->filename());
	if(startupCode->currentEditor())
	VipPyInterpreter::instance()->setStartupCode(startupCode->currentEditor()->toPlainText());

	//make sure to recreate the interpreter
	VipPyInterpreter::instance()->isRunning();

	VipTextEditor::setStdColorSchemeForType("Python", styleBox->currentText());
}

void VipPythonParameters::updatePage()
{
	if (VipPyInterpreter::instance()->launchCode() == VipPyInterpreter::InLocalInterp)
		launchInLocal->setChecked(true);
	else
		launchInIPython->setChecked(true);

	pythonPath->setFilename(VipPyInterpreter::instance()->python());
	wdPath->setFilename(VipPyInterpreter::instance()->workingDirectory());
	startupCode->currentEditor()->setPlainText(VipPyInterpreter::instance()->startupCode());
	
	if (const VipTextHighlighter * h = VipTextEditor::stdColorSchemeForType("Python"))
		styleBox->setCurrentText(h->name);
}

void VipPythonParameters::changeStyle()
{
	if (const VipTextHighlighter * h = VipTextEditor::colorScheme("Python", styleBox->currentText()))
	{
		style->currentEditor()->setColorScheme(h);
	}
}

void VipPythonParameters::restartInterpreter()
{
	VipPyIOOperation * py = VipPyInterpreter::instance()->pyIOOperation(true);
	if (!py || !py->isRunning())
	{
		VIP_LOG_ERROR("Failed to restart Python interpreter");
	}
}

void VipPythonParameters::openWorkingDirectory()
{
	if (QFileInfo(wdPath->filename()).exists())
	{
		QDesktopServices::openUrl(QFileInfo(wdPath->filename()).canonicalPath());
	}
}

void VipPythonParameters::openManager()
{
	vipOpenProcessingManager();
}

void VipPythonParameters::applyStartupCode()
{
	if (VipPyInterpreter::instance()->isRunning())
	{
		QVariant v =VipPyInterpreter::instance()->execCode(startupCode->currentEditor()->toPlainText()).value();
		VipPyError err = v.value<VipPyError>();
		if (!err.isNull()) {
			VIP_LOG_ERROR(err.traceback);
		}
	}
}

void VipPythonParameters::_openPythonData()
{
	QDesktopServices::openUrl(vipGetPythonDirectory());
}
void VipPythonParameters::_openPythonDataScripts()
{
	QDesktopServices::openUrl(vipGetPythonScriptsDirectory());
}

VipPythonParameters * vipGetPythonParameters()
{
	static VipPythonParameters * params = new VipPythonParameters();
	return params;
}






namespace detail
{

	static QList<VipProcessingObject*> applyPySignalFusion(VipPlotPlayer* pl)
	{
		if (!pl)
			return QList<VipProcessingObject*>();

		VipPySignalFusionProcessing* p = new VipPySignalFusionProcessing();
		VipPySignalFusionProcessingEditor* ed = new VipPySignalFusionProcessingEditor();
		ed->buttons()->hide();
		ed->setPySignalFusionProcessing(p);
		ed->setPlotPlayer(pl);
		VipGenericDialog dial(ed, "Create Python signal fusion algorithm");
		while (dial.exec() == QDialog::Accepted) {
			// add the processing to the player
			if (ed->apply()) {
				if (!p->hasError()) {

					VipProcessingList* lst = new VipProcessingList();
					lst->inputAt(0)->setConnection(p->outputAt(0));
					lst->inputAt(0)->setData(p->outputAt(0)->data());
					lst->update();
					lst->setScheduleStrategy(VipProcessingList::Asynchronous, true);
					lst->setDeleteOnOutputConnectionsClosed(true);

					return QList<VipProcessingObject*>() << lst;
				}
			}
		}

		delete p;
		return QList<VipProcessingObject*>();
	}

	PyCustomizePlotPlayer::PyCustomizePlotPlayer(VipPlotPlayer* pl)
	  : QObject(pl)
	{
		pl->setProperty("PyCustomizePlotPlayer", true);
		QAction* act = pl->advancedTools()->menu()->addAction(vipIcon("PYTHON.png"), "Create a Python data fusion processing");
		act->setProperty("_vip_player", QVariant::fromValue(pl));
		connect(act, SIGNAL(triggered(bool)), VipPythonManager::instance(), SLOT(applyPySignalFusion()));

		// make it draggable
		act->setProperty("QMimeData",
				 QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipProcessingObject*>>(std::bind(&applyPySignalFusion, pl), VipCoordinateSystem::Cartesian, act)));
	}

	static void pyCustomizePlotPlayer(VipPlotPlayer* player)
	{
		if (player && !player->property("PyCustomizePlotPlayer").toBool())
			new detail::PyCustomizePlotPlayer(player);
	}

	
	static void pyCreateComplexPyGenerator()
	{
		if (VipIODevice* dev = VipPySignalGeneratorEditor::createGenerator()) {
			vipGetMainWindow()->openDevices(QList<VipIODevice*>() << dev, nullptr, nullptr);
		}
	}

}



bool PyFileHandler::open(const QString & path, QString * error)
{
	QFileInfo info(path);
	if (!info.exists() || info.isDir()) {
		if (error) *error = "Unknown file '" + path + "'";
		return false;
	}

	vipGetPyEditorToolWidget()->editor()->openFile(path);
	vipGetPyEditorToolWidget()->show();
	vipGetPyEditorToolWidget()->raise();
	return true;
}



VipPythonManager* VipPythonManager::instance()
{
	static VipPythonManager inst;
	return &inst;
}

static void uninit_python()
{
	// We MUST delete the VipIPythonToolWidget ourself as it spawn processes which prevent from deleting ALL windows
	if (VipIPythonToolWidget* twidget = vipGetIPythonToolWidget())
		delete twidget;
}

VipPythonManager::VipPythonManager()
{
	//load the VipPySignalFusionProcessing
	VipPyRegisterProcessing::loadCustomProcessings(true);

	
	//test_pyprocess();
	VipFDAddProcessingAction().append<bool(QAction*,VipPlotPlayer*)>(dispatchCurveFit);
	VipFDAddProcessingAction().append<bool(QAction*, VipPlotPlayer*)>(dispatchPySignalFusion);
	

	vipGetMainWindow()->addDockWidget(Qt::BottomDockWidgetArea, vipPyGetPythonInterpreter());
	vipPyGetPythonInterpreter()->setFloating(true);
	vipPyGetPythonInterpreter()->hide();

	QAction * pyaction = vipGetMainWindow()->toolsToolBar()->addAction(vipIcon("PYTHON.png"),"Show/hide Python console");
	vipPyGetPythonInterpreter()->setAction(pyaction);

	vipGetMainWindow()->addDockWidget(Qt::LeftDockWidgetArea, vipGetPyEditorToolWidget());
	vipGetPyEditorToolWidget()->setFloating(true);
	vipGetPyEditorToolWidget()->hide();

	 
	showEditor = new QToolButton();
	showEditor->setIcon(vipIcon("CODE.png"));
	showEditor->setToolTip("Show/hide Python code editor");
	showEditor->setAutoRaise(true); 
	vipGetMainWindow()->toolsToolBar()->addWidget(showEditor);
	vipGetPyEditorToolWidget()->setButton(showEditor);

	// Disable to shortcut menu for now
	/* showEditor->setMenu(new QMenu(showEditor));
	showEditor->setPopupMode(QToolButton::MenuButtonPopup);
	connect(showEditor->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToShowScripts()));
	connect(showEditor->menu(), SIGNAL(triggered(QAction*)), this, SLOT(scriptTriggered(QAction*)));
	*/
	
	
	/* {
		VipGILLocker lock;
		vipPyInit_thermavip();
	}*/
	
	
	vipGetOptions()->addPage("Python", vipGetPythonParameters(),vipIcon("PYTHON.png"));
	  
	//add the generators 

	QAction * complex_generator = vipGetMainWindow()->generateMenu()->addAction("Generate signal from Python script...");
	complex_generator->setToolTip("Create a streaming/temporal video or plot from a Python script");
	connect(complex_generator, &QAction::triggered, this, detail::pyCreateComplexPyGenerator);
	//make the menu action droppable
	complex_generator->setProperty("QMimeData", QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipIODevice*>(
		VipPySignalGeneratorEditor::createGenerator,
		VipCoordinateSystem::Cartesian,
		complex_generator
		)));

	//register all files found in the Python directory
	VipPyInterpreter::instance()->addProcessingDirectory(vipGetPythonDirectory());
	VipPyInterpreter::instance()->addProcessingDirectory("./Python");

	//register PyCustomizePlotPlayer
	vipFDPlayerCreated().append<void(VipPlotPlayer*)>(detail::pyCustomizePlotPlayer);

	// TODO: put back at some point?
	//vipFDPlayerCreated().append<void(VipAbstractPlayer*)>(customizePlayer);

	//vipAddUninitializationFunction(uninitPython);


	const VipTextHighlighter * h = VipTextEditor::stdColorSchemeForType("Python");
	if (vipIsDarkSkin() ) {
		vipSetIPythonStyle("monokai");
		if(!vipIsDarkColor(h->backgroundColor()))
			VipTextEditor::setStdColorSchemeForType("Python", "Spyder Dark");
	}
	else {
		if(vipIsDarkColor(h->backgroundColor()))
			VipTextEditor::setStdColorSchemeForType("Python", "Pydev");
	}
	vipSetIPythonFontSize(VipGuiDisplayParamaters::instance()->defaultEditorFont().pointSize());

	//Initialize global shared memory
	QString smem_name = vipPyGlobalSharedMemoryName();
	//Set the shared memory name in thermavip title
	QString main_title = vipGetMainWindow()->mainTitle();
	main_title.replace("Thermavip", smem_name);
	vipGetMainWindow()->setMainTitle(main_title);

	// Initialize IPython tool widget
	if (VipIPythonToolWidget* twidget = vipGetIPythonToolWidget(vipGetMainWindow()))
	{
		vipGetMainWindow()->addDockWidget(Qt::BottomDockWidgetArea, twidget);
		vipGetIPythonToolWidget()->setFloating(false);
		vipGetIPythonToolWidget()->hide();
	}

	vipAddUninitializationFunction(uninit_python);
}

VipPythonManager ::~VipPythonManager()
{
	
}



void VipPythonManager::save(VipArchive & stream)
{
	VipPyInterpreter & opt = *VipPyInterpreter::instance();
	stream.content("python", opt.python());
	stream.content("workingDirectory", opt.workingDirectory());
	stream.content("type", opt.pyType());
	stream.content("launchCode", (int)opt.launchCode());
	stream.content("startup", opt.startupCode());
	stream.content("schemes", VipTextEditor::stdColorSchemes());

	stream.content("editor", vipGetPyEditorToolWidget());
}

void VipPythonManager::restore(VipArchive & stream)
{
	VipPyInterpreter & opt = *VipPyInterpreter::instance();

	QString type ;
	int launchCode = VipPyInterpreter::InIPythonInterp;
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
	//opt.setPyType((VipPyInterpreter::PyType)type);
	//opt.setPyType(VipPyInterpreter::Local);
	opt.setStartupCode(startup);
	opt.setLaunchCode((VipPyInterpreter::PyLaunchCode)launchCode);
	
	VipTextEditor::setStdColorSchemes(schemes);
	const VipTextHighlighter* h = VipTextEditor::stdColorSchemeForType("Python");
	//Make sure the Python scheme fits with the current skin
	if (vipIsDarkSkin()) {
		if(!h || !vipIsDarkColor(h->backgroundColor()))
			VipTextEditor::setStdColorSchemeForType("Python", "Spyder Dark");
	}
	else {
		if(!h || vipIsDarkColor(h->backgroundColor()))
			VipTextEditor::setStdColorSchemeForType("Python", "Pydev");
	}

	VipPyInterpreter::instance()->pyIOOperation();
	vipGetPythonParameters()->updatePage();
	stream.content("editor", vipGetPyEditorToolWidget());

	//Restart IPython is python process is different
	if (python != "python") {
		if (VipIPythonToolWidget* twidget = vipGetIPythonToolWidget())
		{
			twidget->widget()->closeTab(0);
			twidget->widget()->addInterpreter();
		}
	}

}

void VipPythonManager::applyCurveFit()
{
	if(QAction * act = qobject_cast<QAction*>(sender()) ) {
		if(VipPlotPlayer * pl = act->property("_vip_player").value<VipPlotPlayer*>())
			vipFitCurve(pl,act->property("_vip_fit").toString());
	}
}



void VipPythonManager::applyPySignalFusion()
{
	if (QAction * act = qobject_cast<QAction*>(sender())) {
		if (VipPlotPlayer * pl = act->property("_vip_player").value<VipPlotPlayer*>())
		{
			QList<VipProcessingObject*> lst = detail::applyPySignalFusion(pl);
			if (lst.size()) {
				vipCreatePlayersFromProcessing(lst.first(), pl);

				vipGetProcessingEditorToolWidget()->show();
				vipGetProcessingEditorToolWidget()->setProcessingObject(lst.first()->inputAt(0)->connection()->source()->parentProcessing());
				QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);

			}
			return;

			VipPySignalFusionProcessing * p = new VipPySignalFusionProcessing();
			VipPySignalFusionProcessingEditor * ed = new VipPySignalFusionProcessingEditor();
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
 

bool VipPythonManager::dispatchCurveFit(QAction * act, VipPlotPlayer * pl)
{
	if(act->text().startsWith("Fit")) {
		QString fit = act->text().mid(4);
		if(fit == "Linear" || fit == "Exponential"|| fit == "Polynomial"|| fit == "Gaussian") {
			act->setProperty("_vip_player",QVariant::fromValue(pl));
			act->setProperty("_vip_fit",fit);
			QObject::connect(act, SIGNAL(triggered(bool)), VipPythonManager::instance(), SLOT(applyCurveFit()));
			return true;
		}
	}
	return false;
}

bool  VipPythonManager::dispatchPySignalFusion(QAction * act, VipPlotPlayer * pl)
{
	if (act->text().startsWith("Py Signal Fusion Processing")) {
		act->setProperty("_vip_player", QVariant::fromValue(pl));
		QObject::connect(act, SIGNAL(triggered(bool)), VipPythonManager::instance(), SLOT(applyPySignalFusion()));
		return true;
	}
	return false;
}

void VipPythonManager::aboutToShowScripts()
{
	showEditor->menu()->clear();
	QDir dir(vipGetPythonScriptsDirectory());
	QFileInfoList lst= dir.entryInfoList(QStringList() << "*.py", QDir::Files);
	for (int i = 0; i < lst.size(); ++i)
		showEditor->menu()->addAction(lst[i].fileName())->setProperty("path", lst[i].canonicalFilePath());
}
void VipPythonManager::scriptTriggered(QAction* act)
{
	QString path = act->property("path").toString();

	vipGetPyEditorToolWidget()->editor()->openFile(path);
	vipGetPyEditorToolWidget()->editor()->execFile();
}


