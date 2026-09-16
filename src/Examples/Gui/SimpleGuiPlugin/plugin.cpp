#include "plugin.h"

#include "VipDisplayArea.h"
#include "VipCommandOptions.h"

#include "qmessagebox.h"

bool MyToolWidget::setPlayer(VipAbstractPlayer* pl)
{
	// Adjust QLabel text based on input player

	if (!pl) {
		label->setText(QString());
		return false;
	}

	QString text = "<b>Hi!</b><br><b>Type: </b>" + QString(pl->metaObject()->className()) + "<br>" + "<b>Title: </b>" + pl->windowTitle() + "<br>";
	label->setText(text);
	return true;
}





MyUpdatePlotPlayerInterface::MyUpdatePlotPlayerInterface(VipPlotPlayer* pl)
:QObject(pl) {
	// set this property to the player to notify that MyUpdatePlotPlayerInterface has already been attached to it
	pl->setProperty("MyUpdatePlotPlayerInterface", true);

	// Add an icon to the tool bar
	QAction * act = pl->toolBar()->addAction("Say Hi!");
	connect(act, SIGNAL(triggered(bool)), this, SLOT(sayHiPlot()));
}

void MyUpdatePlotPlayerInterface::sayHiPlot()
{
	QMessageBox::information(nullptr, "Hi", "Hi!");
}

static void addToPlotPlayer(VipPlotPlayer* pl)
{
	// add MyUpdatePlotPlayerInterface to player if not already done
	if (!pl->property("MyUpdatePlotPlayerInterface").toBool())
		new MyUpdatePlotPlayerInterface(pl);
}





RAWSignalReader::RAWSignalReader(QObject* parent)
  : VipIODevice(parent)
{
	outputAt(0)->setData(QVariant::fromValue(VipPointVector()));
}

bool RAWSignalReader::open(VipIODevice::OpenModes mode)
{
	// check open mode
	if (!(mode & ReadOnly))
		return false;

	// get path
	QString p = removePrefix(path());

	// create deviec from path
	QIODevice* d = createDevice(p, QIODevice::ReadOnly);
	// Tested, as the writer below already does: this gives nothing when the file
	// is missing, locked or unreadable, and a dropped file is enough to reach it.
	if (!d)
		return false;

	// read file content
	const qint64 samples = d->size() / (qint64)sizeof(VipPoint);
	VipPointVector vec(samples);
	// Point by point, through the public interface: the vector is a circular
	// buffer whose data() is private and returns the control structure it shares
	// between copies, not the samples.
	for (qsizetype i = 0; i < vec.size(); ++i) {
		VipPoint pt;
		if (d->read(reinterpret_cast<char*>(&pt), sizeof(pt)) != (qint64)sizeof(pt))
			return false;
		vec[i] = pt;
	}

	// set output data
	outputAt(0)->setData(create(QVariant::fromValue(vec)));

	//set open mode
	setOpenMode(mode);
	return true;
}

	
bool RAWSignalReader::readData(qint64)
{
	// just reset data
	VipAnyData any = outputAt(0)->data();
	outputAt(0)->setData(any);
	return true;
}





RAWSignalWriter::RAWSignalWriter(QObject* parent)
	: VipIODevice(parent)
{
}

bool RAWSignalWriter::open(VipIODevice::OpenModes mode)
{
	if (!(mode & WriteOnly))
		return false;

	// get path
	QString p = removePrefix(path());

	// create deviec from path
	QIODevice* d = createDevice(p, QIODevice::WriteOnly);

	if (!d)
		return false;

	setOpenMode(mode);
	return true;
}

void RAWSignalWriter::apply()
{
	// get input data
	VipAnyData any = inputAt(0)->data();
	const VipPointVector v = any.value<VipPointVector>();
	// write to file, point by point for the same reason as the reader
	for (qsizetype i = 0; i < v.size(); ++i) {
		const VipPoint pt = v[i];
		if (device()->write(reinterpret_cast<const char*>(&pt), sizeof(pt)) != (qint64)sizeof(pt))
			return;
	}
}






SimpleGuiInterface::LoadResult SimpleGuiInterface::load()
{
	// Load the plugin. This does the following:
	// -	Add the command line argument --say-hi. If detected, print 'Hi!' and exit.
	// -	Register a function that customize all VipPlotPlayer instances by adding a button that displays a message box saying 'Hi!'
	// -	Add a new tool widget that print 'Hi!' and a few information on the selected player.
	// -	Register a new write device (RAWSignalWriter) that can save VipPointVector into a .rawsig file.
	// -	Register a new read device (RAWSignalReader) that can read .rawsig files.

	// Register new command line options
	VipCommandOptions::instance().addSection("Simple Gui plugin");
	VipCommandOptions::instance().add("say-hi", "print 'Hi!'");

	// Parse again command line arguments
	VipCommandOptions::instance().parse(QCoreApplication::instance()->arguments());
	if (VipCommandOptions::instance().count("say-hi")) {
		// print 'Hi!' and returns ExitProcess to tell that Thermavip should exit
		printf("Hi!\n");
		return ExitProcess;
	}

	// First thing, register addToPlotPlayer function that will customize all VipPlotPlayer objects
	vipFDPlayerCreated().append<void(VipPlotPlayer*)>(addToPlotPlayer);

	// Add the new tool widget
	QAction * act = vipGetMainWindow()->toolsToolBar()->addAction(vipIcon("database.png"), "Useless tool");
	MyToolWidget* tool = new MyToolWidget(vipGetMainWindow());
	vipGetMainWindow() ->addDockWidget(Qt::LeftDockWidgetArea, tool);
	tool->setFloating(true);
	tool->setAction(act);



	// Create a new workspace that will display 6 plot players displaying the same curve
	VipDisplayPlayerArea* workspace = new VipDisplayPlayerArea();
	vipGetMainWindow()->displayArea()->addWidget(workspace);

	// Generate cosinus and sinus curves
	VipPointVector cosinus, sinus;
	for (int i = 0; i < 100; ++i)
	{
		cosinus.append(VipPoint(i, std::cos(i * 0.1)));
		sinus.append(VipPoint(i, std::sin(i * 0.1)));
	}

	QList<VipPlotPlayer*> players;
	for (int i = 0; i < 6; ++i) {

		// We could directly use vipCreatePlayersFromData() and that would work.
		// But we want to create a pipeline VipAnyResource -> VipProcessingList -> VipDisplayCurve
		VipAnyResource* cdevice = new VipAnyResource(workspace->processingPool());
		cdevice->setData(QVariant::fromValue(cosinus));
		cdevice->setAttribute("XUnit", "X");
		cdevice->setAttribute("YUnit", "Y");
		cdevice->setAttribute("Name", "cosinus");

		VipAnyResource* sdevice = new VipAnyResource(workspace->processingPool());
		sdevice->setData(QVariant::fromValue(sinus));
		sdevice->setAttribute("XUnit", "X");
		sdevice->setAttribute("YUnit", "Y");
		sdevice->setAttribute("Name", "sinus");

		// Create player from data. The list can come back empty, and first() on an
		// empty list is undefined; and the player created is not necessarily the
		// kind this loop collects.
		const QList<VipAbstractPlayer*> created = vipCreatePlayersFromProcessing(cdevice, nullptr);
		if (created.isEmpty())
			continue;
		VipAbstractPlayer* pl = created.first();
		// Add data to existing player
		vipCreatePlayersFromProcessing(sdevice, pl);

		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl))
			players.push_back(plot);
	}


	// Organize players in workspace in a 2*3 grid

	// retrieve main VipMultiDragWidget from workspace
	VipMultiDragWidget* mw = workspace->mainDragWidget();

	for (int i = 0; i < 6; ++i) {

		int w = i % 2;
		int h = i / 2;
		if (i%2 == 0)
			// add row to the main (vertical) splitter
			mw->mainResize(h +1);

		// add column to the sub horizontal splitter
		mw->subResize(h, w + 1);
		//add player (encapsulated inside a VipDragWidget)
		mw->setWidget(h, w, vipCreateFromWidgets(QWidgetList()<<players[i]));
	}


	return Success;
}

