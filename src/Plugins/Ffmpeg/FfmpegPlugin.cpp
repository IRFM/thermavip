#include "FfmpegPlugin.h"
#include "VideoDecoder.h"

#include "MPEGLoader.h"
#include "RecordWindow.h"
#include "VipDisplayArea.h"
#include "VipLogging.h"
#include "VipCommandOptions.h"

#include <qtoolbutton.h>
#include <qmenu.h>
#include <QDateTime>
#include <qtoolbutton.h>
#include <qthread.h>
#include <QGuiApplication>
#include <qscreen.h>
#include <qboxlayout.h>

static int r0 = VipCommandOptions::instance().addSection("Ffmpeg plugin");
static int r1 = VipCommandOptions::instance().add("record", "record a video of the screen into given filename", VipCommandOptions::ValueRequired);
static int r2 = VipCommandOptions::instance().add("rect", "specify rectangle on the form left:top:width:height", VipCommandOptions::ValueRequired);
static int r3 = VipCommandOptions::instance().add("timeout", "specify a maximum recording time in milliseconds", VipCommandOptions::ValueRequired);
static int r4 = VipCommandOptions::instance().add("ffps", "output file frame rate in Hz", VipCommandOptions::ValueRequired);
static int r5 = VipCommandOptions::instance().add("fps", "recording frame rate in Hz", VipCommandOptions::ValueRequired);
static int r6 = VipCommandOptions::instance().add("rate", "Bit rate in KB/s (default is 30000: high quality)", VipCommandOptions::ValueRequired);




OpenStream::OpenStream()
	:QWidget()
{
	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addWidget(&m_paths);
	hlay->addWidget(&m_open);
	hlay->setContentsMargins(0, 0, 0, 0);
	setLayout(hlay);

	m_paths.setToolTip("Enter network or local video path.\nPress ENTER to open.");
	m_paths.setEditable(true);
	m_open.setAutoRaise(true);
	m_open.setToolTip("Open local video");
	m_open.setText("...");
	m_open.setMaximumWidth(20);

	connect(&m_open, SIGNAL(clicked(bool)), this, SLOT(openFilePath()));
	connect(m_paths.lineEdit(), SIGNAL(returnPressed()), this, SLOT(open()));

	setMinimumWidth(300);
}

QString OpenStream::path() const
{
	return m_paths.currentText();
}
QStringList OpenStream::recentPaths() const
{
	QStringList res;
	for (int i = 0; i < m_paths.count(); ++i)
		res << m_paths.itemText(i);
	return res;
}
void OpenStream::setRencentPaths(const QStringList & lst)
{
	m_paths.clear();
	m_paths.addItems(lst);
	while (m_paths.count() > 20)
		m_paths.removeItem(20);
}

void OpenStream::openFilePath()
{
	MPEGLoader l;
	QString filters = l.fileFilters();
	QString path = VipFileDialog::getOpenFileName(nullptr, "Open video file", filters);
	if (!path.isEmpty()) {
		m_paths.setCurrentText(path);
		open();
	}
}

void OpenStream::open()
{
	if (path().isEmpty())
		return;

	int index = m_paths.findText(path());
	if (index > 0) 
		m_paths.removeItem(index);
	m_paths.insertItem(0, path());
	while (m_paths.count() > 20)
		m_paths.removeItem(20);
	m_paths.setCurrentText(path());

	VipDisplayArea & area = *vipGetMainWindow()->displayArea();
	VipDisplayPlayerArea * plarea = area.currentDisplayPlayerArea();
	if (plarea)
	{
		VipProcessingPool * pool = plarea->processingPool();

		MPEGLoader * loader = new MPEGLoader(pool);
		loader->setPath(this->path());
		if (!loader->open(VipIODevice::ReadOnly))
		{
			delete loader;
			VIP_LOG_ERROR("Cannot open video: " + this->path());
			return;
		}

		VipMultiDragWidget * bdw2 = vipCreateFromBaseDragWidget(vipCreateWidgetFromProcessingObject(loader));
		plarea->addWidget(bdw2);
	}
}






//open webcams
void FfmpegInterface::openVideoStream(QAction * action)
{
	VipDisplayArea & area = *vipGetMainWindow()->displayArea();
	VipDisplayPlayerArea * plarea = area.currentDisplayPlayerArea();
	if (plarea)
	{
		VipProcessingPool * pool = plarea->processingPool();

		MPEGLoader * loader = new MPEGLoader(pool);
		if (!loader->open("video=" + action->text(), "dshow"))
		{
			delete loader;
			VIP_LOG_ERROR("Cannot open video stream: " + action->text());
			return;
		}

		//loader->setPath(action->text());

		VipMultiDragWidget * bdw2 = vipCreateFromBaseDragWidget(vipCreateWidgetFromProcessingObject(loader));
		plarea->addWidget(bdw2);
	}
	
}

#include <qwidgetaction.h>

FfmpegInterface::LoadResult FfmpegInterface::load()
{
	VipCommandOptions::instance().parse(QCoreApplication::instance()->arguments());
	if (VipCommandOptions::instance().count("record"))
	{
		//command line recording of the screen

		QString filename = VipCommandOptions::instance().value("record").toString();
		double rate = 30000;
		QRect rect = QGuiApplication::primaryScreen()->geometry();
		QRect screen = rect;
		int ffps = 15;
		int fps = 15;
		int timeout = -1;

		if (VipCommandOptions::instance().count("rate")) rate = VipCommandOptions::instance().value("rate").toDouble();
		if (VipCommandOptions::instance().count("ffps")) ffps = VipCommandOptions::instance().value("ffps").toDouble();
		if (VipCommandOptions::instance().count("fps")) fps = VipCommandOptions::instance().value("fps").toDouble();
		if (VipCommandOptions::instance().count("timeout")) timeout = VipCommandOptions::instance().value("timeout").toDouble();
		if (VipCommandOptions::instance().count("rect")) {
			QString tmp = VipCommandOptions::instance().value("rect").toString();
			QStringList lst = tmp.split(":");
			if (lst.size() != 4) {
				VIP_LOG_ERROR("Wrong argument value for 'rect'");
				return FfmpegInterface::ExitProcess;
			}
			rect = QRect(lst[0].toInt(), lst[1].toInt(), lst[2].toInt(), lst[3].toInt());
			rect = rect.intersected(screen);
		}

		RecordWindow record;
		record.setFilename(filename);
		record.setRate(rate);
		record.setRecordingFps(fps);
		record.setMovieFps(ffps);
		record.setScreenRect(rect);
		record.setOutputSize(rect.size());
		record.setTimeout(timeout);

		QEventLoop loop;

		connect(&record, SIGNAL(stopped()), &record, SLOT(stop()));
		connect(&record, SIGNAL(stopped()), &loop, SLOT(quit()));

		QMetaObject::invokeMethod(&record, "start", Qt::QueuedConnection);
		
		loop.exec(/*QEventLoop::WaitForMoreEvents|*/QEventLoop::AllEvents);
		

		return FfmpegInterface::ExitProcess;
	}


	/*QImage im("C:/Users/Moncada/Desktop/complex_img3.png");

	VideoCapture * vc = Init("out.mov", im.width(), im.height(), 15, 25000);
	if (vc)
	{
		for (int i = 0; i < 200; ++i)
			AddFrame(im, vc);
		Finish(vc);
	}*/

	//retrieve the list of available video devices
	QStringList lst = VideoDecoder::list_devices();
	
	if (true)//lst.size())
	{ 
		QToolButton * open = new QToolButton();
		open->setAutoRaise(true);
		open->setPopupMode(QToolButton::InstantPopup);
		open->setIcon(vipIcon("webcam.png"));
		open->setToolTip("Open local webcam");

		QMenu * menu = new QMenu(open);
		for (int i = 0; i < lst.size(); ++i)
			menu->addAction(lst[i]);
		menu->addSeparator();
		QWidgetAction * act = new QWidgetAction(menu);
		act->setDefaultWidget(m_open_stream = new OpenStream());
		menu->addAction(act);

		open->setMenu(menu);
		QObject::connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(openVideoStream(QAction*)));

		vipGetMainWindow()->toolsToolBar()->addWidget(open)->setToolTip("Open local webcam or network stream");
	}

	//add button to make movies of thermavip (successive screenshots)
	m_rec = new QToolButton();
	m_rec->setIcon(vipIcon("record.png"));
	m_rec->setToolTip("<b>Record your actions</b><br>Create a video from successive screenshots of Thermavip in order to record your actions.<br>"
	"Check/uncheck this button to start/stop the recording.<br>Use the right arrow to modify the recording parameters.");
	m_rec->setAutoRaise(true);
	m_rec->setCheckable(true);
	VipDragMenu * menu = new VipDragMenu();
	m_rec_win = new RecordWindow();
	menu->setWidget(m_rec_win);
	m_rec->setMenu(menu);
	m_rec->setPopupMode(QToolButton::MenuButtonPopup);
	connect(m_rec, SIGNAL(clicked(bool)), this, SLOT(setRecording(bool)));
	connect(m_rec_win, SIGNAL(stateChanged(bool)), this, SLOT(setRecording(bool)),Qt::QueuedConnection);

	vipGetMainWindow()->closeBar()->insertWidget(vipGetMainWindow()->closeBar()->minimizeButton, m_rec);


	return FfmpegInterface::Success;

	
}

void FfmpegInterface::save(VipArchive & arch)
{
	arch.content("recentPaths", m_open_stream ? m_open_stream->recentPaths() : QStringList());
}
void FfmpegInterface::restore(VipArchive & arch)
{
	if (m_open_stream) {
		QStringList lst = arch.read("recentPaths").toStringList();
		m_open_stream->setRencentPaths(lst);
	}
}

void FfmpegInterface::setRecording(bool enable)
{
	if (!m_rec_win || !m_rec)
		return;

	vip_debug("FfmpegInterface::setRecording %i\n", (int)enable);

	if (enable != m_rec_win->isRecording())
	{
		m_rec_win->setState(enable);//setRecordExternaProcess(enable);
	}
	m_rec->blockSignals(true);
	m_rec->setChecked(enable);
	m_rec->blockSignals(false);
}

/*
#include "VideoEncoder.h"
#include "VideoDecoder.h"

int main(int argc, char** argv)
{
	VideoEncoder encoder;
	QImage img("C:/Users/moncada/Desktop/photos/logo_simple.png");
	img = img.scaled(50, 50);
	img = img.convertToFormat(QImage::Format_ARGB32);
	 
	encoder.Open("C:/Users/moncada/Desktop/test.wmv", img.width(), img.height(), 25, 20000);
	for (int i = 0; i < 100; ++i)
	{
		img = img.transformed(QTransform().scale(1.1, 1.1));
		img = img.copy(0,0,50, 50);
		encoder.AddFrame(img);
	}
	img.save("C:/Users/moncada/Desktop/test.png");
	encoder.Close();
	

	return 0;


	VideoDecoder decoder;
	decoder.Open("C:/Users/moncada/Desktop/test.wmv");
	double total_time = decoder.GetTotalTime();
	double rate = decoder.GetRate();
	double fps = decoder.GetFps();
	//decoder.SeekFrame(12);
	double time = 0;
	double sampling = 1.0 / fps;
	int i = 0;
	while (true)
	{
		QImage img = decoder.GetFrameByTime(time);
		img.save("C:/Users/moncada/Desktop/tmp/test" + QString::number(i) + ".png");
		time += sampling;
		//if (img.isNull() || !decoder.MoveNextFrame())
		//	break;
		//else
			++i;
	}

	return 0;
}
*/