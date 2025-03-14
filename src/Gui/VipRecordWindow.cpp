#include "VipRecordWindow.h"
#include "VipDisplayArea.h"
#include "VipToolWidget.h"
#include "VipCorrectedTip.h"
#include "VipLogging.h"
#include "VipDisplayArea.h"
#include "VipMPEGLoader.h"
#include "VipStandardWidgets.h"

#include <QWidgetAction>
#include <QGridLayout>
#include <QScreen>
#include <qthread.h>
#include <qdatetime.h>
#include <qmutex.h>
#include <qapplication.h>
/*
double sampling_ms = (1.0 / rec->recordingFps()) * 1000;
qint64 last_time = 0;
qint64 current = QDateTime::currentMSecsSinceEpoch();
if (last_time == 0 || (current - last_time) >= sampling_ms)
{
	last_time = current;
}
*/

// global timeout
static QMutex __mutex;
static bool __should_quit = false;

static bool IsCloseEventReceived()
{
	return false;
	/* struct Thread : QThread
	{
		std::string str;
		virtual void run()
		{
			while (!__should_quit) {
				std::cin >> str;

				QMutexLocker lock(&__mutex);
				__should_quit = true;
			}
		}
	};

	static Thread th;
	if (!th.isRunning())
		th.start();

	return __should_quit;*/
}

#ifdef _MSC_VER
#include "Windows.h"

struct ScreenShot
{
	HDC hScreen;
	int ScreenX, ScreenY;
	HDC hdcMem;
	HBITMAP hBitmap;
	QImage img;
	QRect rect;

	ScreenShot(const QRect& r)
	{
		rect = r;
		hScreen = GetDC(GetDesktopWindow());
		ScreenX = GetDeviceCaps(hScreen, HORZRES);
		ScreenY = GetDeviceCaps(hScreen, VERTRES);

		hdcMem = CreateCompatibleDC(hScreen);
		hBitmap = CreateCompatibleBitmap(hScreen, /*ScreenX, ScreenY*/ r.width(), r.height());
		img = QImage(/*ScreenX, ScreenY*/ r.width(), r.height(), QImage::Format_ARGB32);
	}

	~ScreenShot()
	{
		ReleaseDC(GetDesktopWindow(), hScreen);
		DeleteDC(hdcMem);
		DeleteObject(hBitmap);
	}

	QImage grab()
	{
		BITMAPINFOHEADER bmi = { 0 };
		bmi.biSize = sizeof(BITMAPINFOHEADER);
		bmi.biPlanes = 1;
		bmi.biBitCount = 32;
		bmi.biWidth = rect.width();    // ScreenX;
		bmi.biHeight = -rect.height(); //-ScreenY;
		bmi.biCompression = BI_RGB;
		bmi.biSizeImage = 0;

		HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);
		BitBlt(hdcMem, 0, 0, rect.width(), rect.height() /*ScreenX, ScreenY*/, hScreen, /* 0, 0*/ rect.left(), rect.top(), SRCCOPY);
		SelectObject(hdcMem, hOld);

		GetDIBits(hdcMem, hBitmap, 0, /*ScreenY*/ rect.height(), img.bits(), (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
		return img; // .copy(r);
	}
};
static ScreenShot* sshot = nullptr;

QImage ScreenCap(const QRect& r)
{
	HDC hScreen = GetDC(GetDesktopWindow());
	// int ScreenX = GetDeviceCaps(hScreen, HORZRES);
	// int ScreenY = GetDeviceCaps(hScreen, VERTRES);

	HDC hdcMem = CreateCompatibleDC(hScreen);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, /*ScreenX, ScreenY*/ r.width(), r.height());
	HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);
	BitBlt(hdcMem, 0, 0, /*ScreenX, ScreenY*/ r.width(), r.height(), hScreen, /*0, 0*/ r.left(), r.top(), SRCCOPY);
	SelectObject(hdcMem, hOld);

	BITMAPINFOHEADER bmi = { 0 };
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biPlanes = 1;
	bmi.biBitCount = 32;
	bmi.biWidth = r.width();    // ScreenX;
	bmi.biHeight = -r.height(); // -ScreenY;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = 0; // 3 * ScreenX * ScreenY;

	QImage img(/*bmi.biWidth,- bmi.biHeight,*/ r.width(), r.height(), QImage::Format_ARGB32);

	GetDIBits(hdcMem, hBitmap, 0, /*ScreenY*/ r.height(), img.bits(), (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

	ReleaseDC(GetDesktopWindow(), hScreen);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);

	/*if (r.isEmpty())
		return img;
	else
		return img.copy(r);*/
	return img;
}

#endif

VipPlayerSelection::VipPlayerSelection(QWidget* parent)
  : VipComboBox(parent)
{
	this->setToolTip("Record a widget only, or select 'None' to record the full interface");
	this->addItem("None");

	connect(this, SIGNAL(openPopup()), this, SLOT(aboutToShow()));
	connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(selected()));
}
VipPlayerSelection::~VipPlayerSelection() {}

VipBaseDragWidget* VipPlayerSelection::selectedWidget() const
{
	return m_widget;
}

void VipPlayerSelection::aboutToShow()
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		QList<VipBaseDragWidget*> players = area->findChildren<VipBaseDragWidget*>();

		this->clear();
		this->addItem("None");
		for (int i = 0; i < players.size(); ++i) {
			// only add the VipBaseDragWidget with a visible header
			if (players[i]->isVisible()) {
				this->addItem(players[i]->windowTitle());
			}
		}
	}
}
void VipPlayerSelection::selected()
{
	QString t = currentText();
	if (t == "None") {
		m_widget = QPointer<VipBaseDragWidget>();
		return;
	}

	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		QList<VipBaseDragWidget*> players = area->findChildren<VipBaseDragWidget*>();
		for (int i = 0; i < players.size(); ++i) {
			if (t == players[i]->windowTitle())
				m_widget = players[i];
		}
	}
}

#include "VipMPEGSaver.h"

struct RecordThread : public QThread
{
	VipRecordWindow* rec;
	VipMPEGSaver* encoder;
	QList<QImage> images;
	QMutex mutex;
	bool started;

	RecordThread()
	  : rec(nullptr)
	  , encoder(nullptr)
	  , started(false)
	{
	}

	void addImage(const QImage& img)
	{
		QMutexLocker lock(&mutex);
		images.append(img);
	}

	virtual void run()
	{

		VipRecordWindow* r = rec;
		if (!r) {
			return;
		}

		try {
			QFile::remove(r->filename());

			QThread::msleep(r->recordDelay() * 1000);

			Q_EMIT r->started();
			Q_EMIT r->stateChanged(true);
			started = true;

			QSize s = r->videoSize();
			VIP_LOG_INFO("Start record thread (%i*%i) in file %s\n", s.width(), s.height(), r->filename().toLatin1().data());
			encoder = new VipMPEGSaver();
			encoder->setPath(r->filename());
			VipMPEGIODeviceHandler h;
			h.codec_id = 0;
			h.fps = r->movieFps();
			h.rate = r->rate() * 1000;
			h.width = s.width();
			h.height = s.height();
			encoder->open(VipMPEGSaver::ReadOnly);
			// encoder = new VideoEncoder();
			// encoder->Open(r->filename().toLatin1().data(), s.width(), s.height(), r->movieFps(), r->rate() * 1000);
			qint64 starttime = QDateTime::currentMSecsSinceEpoch();

			while (rec && !__should_quit) {
				VipRecordWindow* _rec = rec;

				while (!__should_quit) {
					// get last image
					QImage img;
					{
						QMutexLocker lock(&mutex);
						if (images.size()) {
							img = images.front();
							images.pop_front();
						}
					}
					if (!img.isNull()) {
						// encoder->AddFrame(img);
						encoder->inputAt(0)->setData(QVariant::fromValue(vipToArray(img)));
						encoder->update();
						if (encoder->hasError()) {
							__should_quit = true;
						}
					}
					else {
						QThread::msleep(1);
						break;
					}

					if ((_rec->timeout() >= 0 && (QDateTime::currentMSecsSinceEpoch() - starttime) > _rec->timeout()) || IsCloseEventReceived()) {
						QMutexLocker lock(&__mutex);
						__should_quit = true;
					}
				}

				if ((_rec->timeout() >= 0 && (QDateTime::currentMSecsSinceEpoch() - starttime) > _rec->timeout()) || IsCloseEventReceived()) {
					QMutexLocker lock(&__mutex);
					__should_quit = true;
				}
			}
		}
		catch (...) {
			{
				QMutexLocker lock(&__mutex);
				__should_quit = true;
			}
			// encoder->Close();
			encoder->close();
			delete encoder;
			encoder = nullptr;
			Q_EMIT r->stopped();
			Q_EMIT r->stateChanged(false);
			return;
		}
		{
			QMutexLocker lock(&__mutex);
			__should_quit = true;
		}
		// encoder->Close();
		encoder->close();
		delete encoder;
		encoder = nullptr;
		Q_EMIT r->stopped();
		Q_EMIT r->stateChanged(false);
	}
};


class VipRecordWindow::PrivateData
{
public:
	QToolButton reset;
	VipDoubleEdit rate;
	QSpinBox fps;
	QSpinBox movie_fps;
	QDoubleSpinBox recordDelay;
	VipFileName file;
	QCheckBox recordOnPlay;
	VipPlayerSelection player;
	bool recordOnPlayEnabled;
	QTimer timer;
	RecordThread* thread;
	bool first_show;
	qint64 press_date;
	Qt::MouseButtons buttons;
	QPixmap cursor;
	QRect rect;
	QScreen* screen{ nullptr };
	int timeout;

	int grow_time;
	QPen pen;
	QBrush brush;
	bool draw_mouse;

	QProcess process;
	VipMPEGSaver* encoder;
};


void VipRecordWindow::openFile()
{
	closeFile();
	try {
		d_data->rect = computeRect();
		d_data->screen = vipGetMainWindow()->screen();
		d_data->encoder = new VipMPEGSaver();
		QSize s = videoSize();
		d_data->encoder->setAdditionalInfo(VipMPEGIODeviceHandler{ s.width(), s.height(), movieFps(), rate() * 1000, -1, 2 });
		d_data->encoder->setPath(filename());
		if (!d_data->encoder->open(VipIODevice::WriteOnly)) {
			delete d_data->encoder;
			d_data->encoder = nullptr;
		}
		//d_data->encoder->Open(filename().toLatin1().data(), s.width(), s.height(), movieFps(), rate() * 1000);
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("Could not create video encoder: " + QString(e.what()));
		if (d_data->encoder) {
			delete d_data->encoder;
			d_data->encoder = nullptr;
		}
	}
}
void VipRecordWindow::closeFile()
{
	try {
		d_data->rect = QRect();
		if (d_data->encoder) {
			//d_data->encoder->Close();
			d_data->encoder->close();
			delete d_data->encoder;
			d_data->encoder = nullptr;
		}
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("Could not create video encoder: " + QString(e.what()));
		if (d_data->encoder) {
			delete d_data->encoder;
			d_data->encoder = nullptr;
		}
	}
}
void VipRecordWindow::recordCurrentImage()
{
	try {
		if (d_data->encoder) {
			//const QImage img = grabCurrentImage();
			//d_data->encoder->AddFrame(img);
			d_data->encoder->inputAt(0)->setData(vipToArray(grabCurrentImage()));
			d_data->encoder->update();
		}
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("Could not encode image: " + QString(e.what()));
	}
}

VipRecordWindow::VipRecordWindow(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	QGridLayout* lay = new QGridLayout();

	int row = 0;

	lay->addWidget(&d_data->reset, row++, 0, 1, 2);

	lay->addWidget(new QLabel("Frame Rate (Kbits/s)"), row, 0);
	lay->addWidget(&d_data->rate, row++, 1);

	lay->addWidget(new QLabel("Acquisition FPS"), row, 0);
	lay->addWidget(&d_data->fps, row++, 1);

	lay->addWidget(new QLabel("Movie FPS"), row, 0);
	lay->addWidget(&d_data->movie_fps, row++, 1);

	lay->addWidget(new QLabel("Record delay"), row, 0);
	lay->addWidget(&d_data->recordDelay, row++, 1);

	lay->addWidget(&d_data->file, row++, 0, 1, 2);
	lay->addWidget(VipLineWidget::createHLine(), row++, 0, 1, 2);
	lay->addWidget(&d_data->recordOnPlay, row++, 0, 1, 2);
	lay->addWidget(&d_data->player, row, 0, 1, 2);
	setLayout(lay);

	d_data->reset.setText("Reset parameters");

	d_data->rate.setToolTip("Recording frame rate in Kbits/s");

	d_data->fps.setToolTip("Recording speed, set the sampling time between each frame");
	d_data->fps.setRange(1, 50);

	d_data->movie_fps.setToolTip("Actual movie FPS as saved in the video file");
	d_data->movie_fps.setRange(1, 50);

	d_data->recordDelay.setToolTip("Start recording after X seconds");
	d_data->recordDelay.setRange(0, 10);
	d_data->recordDelay.setValue(0);
	d_data->recordDelay.setSingleStep(0.5);

	d_data->file.setMode(VipFileName::Save);
	d_data->file.setFilters("Video file (*.mp4 *.mpg *.mpeg *.avi *.wmv *.gif *.mov)");
	d_data->file.edit()->setPlaceholderText("Recording file name");
	d_data->file.setDialogParent(vipGetMainWindow());

	d_data->recordOnPlay.setText("Sync. recording on play");
	d_data->recordOnPlay.setToolTip("<b>Start/Stop the recording when clicking the play/stop buttons.</b><br>"
				  "The recording will start when clicking the 'play' button and stop when clicking the 'stop' one.<br>"
				  "One image is recorded every time step.<br>"
				  "This option ignores the 'Acquisition FPS' parameter.");

	d_data->thread = new RecordThread();
	d_data->thread->rec = nullptr;

	d_data->timer.setSingleShot(false);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(grabImage()));
	connect(&d_data->reset, SIGNAL(clicked(bool)), this, SLOT(resetParams()));

	connect(vipGetMainWindow(), SIGNAL(aboutToClose()), this, SLOT(stop()));
	connect(&d_data->recordOnPlay, SIGNAL(clicked(bool)), this, SLOT(setRecordOnPlay(bool)));
	connect(this, SIGNAL(stopped()), this, SLOT(stop()));

	d_data->first_show = false;
	d_data->press_date = 0;
	d_data->buttons = Qt::MouseButtons();
	d_data->cursor = vipPixmap("std_cursor.png");
	d_data->timeout = -1;
	//d_data->handler = nullptr;
	d_data->encoder = nullptr;
	d_data->recordOnPlayEnabled = false;
	resetParams();
}

VipRecordWindow::~VipRecordWindow()
{
	stop();
	delete d_data->thread;
}

void VipRecordWindow::showEvent(QShowEvent*)
{
	if (!d_data->first_show) {
		resetParams();
		d_data->first_show = true;
	}
}

void VipRecordWindow::resetParams()
{
	d_data->rate.setValue(2500);
	d_data->fps.setValue(25);
	d_data->movie_fps.setValue(25);
	d_data->grow_time = 100;
	d_data->draw_mouse = true;
	d_data->pen = QPen(Qt::red);
	d_data->brush = QBrush(Qt::red);
}

void VipRecordWindow::waitForFinished()
{
	if (isRecording())
		d_data->thread->wait();
}

void VipRecordWindow::setRate(double rate)
{
	d_data->rate.setValue(rate);
}
double VipRecordWindow::rate() const
{
	return d_data->rate.value();
}

void VipRecordWindow::setRecordingFps(int fps)
{
	d_data->fps.setValue(fps);
}
int VipRecordWindow::recordingFps() const
{
	return d_data->fps.value();
}

void VipRecordWindow::setMovieFps(int fps)
{
	d_data->movie_fps.setValue(fps);
}
int VipRecordWindow::movieFps() const
{
	return d_data->movie_fps.value();
}

void VipRecordWindow::setFilename(const QString& fname)
{
	d_data->file.setFilename(fname);
}
QString VipRecordWindow::filename() const
{
	return d_data->file.filename();
}

void VipRecordWindow::setRecordDelay(double secs)
{
	d_data->recordDelay.setValue(secs);
}
double VipRecordWindow::recordDelay() const
{
	return d_data->recordDelay.value();
}

QSize VipRecordWindow::videoSize() const
{
	return d_data->rect.size();
}

void VipRecordWindow::setRecordOnPlay(bool enable)
{
	d_data->recordOnPlay.blockSignals(true);
	d_data->recordOnPlay.setChecked(enable);
	d_data->recordOnPlay.blockSignals(false);

	if (enable != d_data->recordOnPlayEnabled) {
		d_data->recordOnPlayEnabled = enable;
		if (enable) {
			connect(vipGetMainWindow()->displayArea(), SIGNAL(playingStarted()), this, SLOT(openFile()), Qt::DirectConnection);
			connect(vipGetMainWindow()->displayArea(), SIGNAL(playingAdvancedOneFrame()), this, SLOT(recordCurrentImage()), Qt::BlockingQueuedConnection);
			connect(vipGetMainWindow()->displayArea(), SIGNAL(playingStopped()), this, SLOT(closeFile()), Qt::DirectConnection);
		}
		else {
			disconnect(vipGetMainWindow()->displayArea(), SIGNAL(playingStarted()), this, SLOT(openFile()));
			disconnect(vipGetMainWindow()->displayArea(), SIGNAL(playingAdvancedOneFrame()), this, SLOT(recordCurrentImage()));
			disconnect(vipGetMainWindow()->displayArea(), SIGNAL(playingStopped()), this, SLOT(closeFile()));
		}
	}
}
bool VipRecordWindow::recordOnPlay() const
{
	return d_data->recordOnPlayEnabled;
}

/*#include <qwindow.h>
#include <qscreen.h>
QImage VipRecordWindow::grabScreenRect()
{
#ifdef _MSC_VER
	if (d_data->handler)
		return static_cast<ScreenShot*>(d_data->handler)->grab();

	if (VipBaseDragWidget* w = d_data->player.selectedWidget()) {
		d_data->rect = QRect(w->mapToGlobal(QPoint(0, 0)), w->mapToGlobal(QPoint(w->width(), w->height())));
		if (d_data->rect.width() % 2)
			d_data->rect.setRight(d_data->rect.right() - 1);
		if (d_data->rect.height() % 2)
			d_data->rect.setBottom(d_data->rect.bottom() - 1);
	}

	return ScreenCap(d_data->rect);
#else
	QScreen* screen = QGuiApplication::primaryScreen();
	QPixmap pix = screen->grabWindow(0, d_data->rect.left(), d_data->rect.top(), d_data->rect.width(), d_data->rect.height());
	// QPixmap pix  = screen->grabWindow(0);
	return pix.toImage();
#endif
}*/

QImage VipRecordWindow::grabCurrentImage()
{
	QImage img;

	QPoint topleft = d_data->rect.topLeft();
	{
		const QPixmap pix = d_data->screen->grabWindow(0);
		img = QImage(d_data->rect.size(), QImage::Format_ARGB32);
		img.fill(Qt::transparent);
		
		QPainter p(&img);
		p.drawPixmap(QRect(QPoint(0, 0), img.size()), pix, QRect(topleft, img.size()));

		QList<QWidget*> ws = QApplication::topLevelWidgets();
		for (int i = 0; i < ws.size(); ++i) {
			QWidget* w = ws[i];
			if (w != vipGetMainWindow() && w->isVisible()) {
				if (w->parentWidget())
					p.drawPixmap(w->geometry().translated(-topleft), w->grab());
				else
					p.drawPixmap(w->geometry(), w->grab());
			}
		}
		

		if (d_data->draw_mouse) {
			double expand_time = d_data->grow_time; // ms

			p.setRenderHint(QPainter::Antialiasing);
			if (QApplication::mouseButtons() || ((QDateTime::currentMSecsSinceEpoch() - d_data->press_date) < expand_time && d_data->buttons)) {
				if (d_data->buttons == 0 && !((QDateTime::currentMSecsSinceEpoch() - d_data->press_date) < expand_time)) {
					d_data->press_date = QDateTime::currentMSecsSinceEpoch();
					d_data->buttons = QApplication::mouseButtons();
				}

				double radius = ((QDateTime::currentMSecsSinceEpoch() - d_data->press_date) / expand_time) * 9;
				radius = qMin(9., radius);

				p.setPen(d_data->pen);
				p.setBrush(d_data->brush);
				p.drawEllipse(QRectF(-radius / 2, -radius / 2, radius, radius).translated(QCursor::pos() - topleft));
			}
			else {
				if (d_data->buttons) {
					d_data->buttons = Qt::MouseButtons();
					d_data->press_date = QDateTime::currentMSecsSinceEpoch();
				}

				if ((QDateTime::currentMSecsSinceEpoch() - d_data->press_date) < expand_time) {
					double radius = ((QDateTime::currentMSecsSinceEpoch() - d_data->press_date) / expand_time) * 9;
					radius = qMin(9., radius);
					radius = 9 - radius;

					p.setPen(d_data->pen);
					p.setBrush(d_data->brush);
					p.drawEllipse(QRectF(-radius / 2, -radius / 2, radius, radius).translated(QCursor::pos() - topleft));
				}
			}
			// draw cursor
			p.drawPixmap(QRect(0, 0, d_data->cursor.width(), d_data->cursor.height()).translated(QCursor::pos() - topleft), d_data->cursor);
		}
	}
	return img;
}

#include <qmap.h>
static bool diff(const QMultiMap<QString, int>& m1, const QMultiMap<QString, int>& m2)
{
	if (m1.size() != m2.size() || m1.size() == 0)
		return true;
	QMultiMap<QString, int>::const_iterator it1 = m1.begin();
	QMultiMap<QString, int>::const_iterator it2 = m2.begin();
	for (; it1 != m1.end(); ++it1, ++it2) {
		if (it1.key() != it2.key())
			return true;
		if (it1.value() == it2.value())
			continue;
		if (std::abs(it1.value() - it2.value()) >= 4)
			return true;
	}
	return false;
}

static QMultiMap<QString, int> _progress_status;

void VipRecordWindow::grabImage()
{
	QMultiMap<QString, int> current = vipGetMultiProgressWidget()->currentProgresses();
	if (diff(current, _progress_status)) {
		// ok, update image and progress status
		_progress_status = current;
	}
	else {
		// same status, do not save the image
		return;
	}

	const QImage img = grabCurrentImage();
	if (!img.isNull())
		d_data->thread->addImage(img);
}

void VipRecordWindow::setMouseGrowTime(int msecs)
{
	d_data->grow_time = msecs;
}
int VipRecordWindow::mouseGrowTime() const
{
	return d_data->grow_time;
}

void VipRecordWindow::setMousePen(const QPen& p)
{
	d_data->pen = p;
}
QPen VipRecordWindow::mousePen() const
{
	return d_data->pen;
}

void VipRecordWindow::setMouseBrush(const QBrush& b)
{
	d_data->brush = b;
}
QBrush VipRecordWindow::mouseBrush() const
{
	return d_data->brush;
}

void VipRecordWindow::setDrawMouse(bool enable)
{
	d_data->draw_mouse = enable;
}
bool VipRecordWindow::drawMouse() const
{
	return d_data->draw_mouse;
}

void VipRecordWindow::setScreenRect(const QRect& r)
{
	d_data->rect = r;
}
QRect VipRecordWindow::screenRect() const
{
	return d_data->rect;
}

void VipRecordWindow::setTimeout(int milli)
{
	d_data->timeout = milli;
}
int VipRecordWindow::timeout() const
{
	return d_data->timeout;
}

bool VipRecordWindow::isRecording() const
{
	return d_data->thread->isRunning() || d_data->process.state() == QProcess::Running;
}

/* void VipRecordWindow::setRecordExternalProcess(bool enable)
{
	d_data->process.write("exit\n");
	d_data->process.closeWriteChannel();
	d_data->process.terminate();

	if (enable) {
		QRect r = vipGetMainWindow()->geometry();
		QString rect = "--rect=" + QString::number(r.left()) + ":" + QString::number(r.top()) + ":" + QString::number(r.width()) + ":" + QString::number(r.height());
		QString command = "Thermavip --no_splashscreen --plugins=Ffmpeg " + rect + " --rate=" + QString::number(this->rate()) + " --fps=" + QString::number(recordingFps()) +
				  " --ffps=" + QString::number(movieFps()) + " --record=" + filename();

		d_data->process.start(command);
		d_data->process.waitForStarted();
	}
}*/

QRect VipRecordWindow::computeRect()
{
	QRect rect = vipGetMainWindow()->geometry();
	if (VipBaseDragWidget* w = d_data->player.selectedWidget()) 
		rect = QRect(w->mapToGlobal(QPoint(0, 0)), w->mapToGlobal(QPoint(w->width(), w->height())));
	
	if (rect.width() % 2)
		rect.setRight(rect.right() - 1);
	if (rect.height() % 2)
		rect.setBottom(rect.bottom() - 1);
	return rect;
}

void VipRecordWindow::start()
{
	stop();
	{
		QMutexLocker lock(&__mutex);
		__should_quit = false;
	}

	d_data->rect = computeRect();
	d_data->screen = vipGetMainWindow()->screen();

/* #ifdef _MSC_VER
	if (!d_data->handler && !d_data->rect.isEmpty()) {
		d_data->handler = new ScreenShot(d_data->rect);
	}
#endif*/

	d_data->timer.setInterval(1000.0 / recordingFps());

	d_data->thread->rec = this;
	d_data->thread->start();
	if (!d_data->recordOnPlayEnabled)
		d_data->timer.start();
	while (!d_data->thread->started) {
		QThread::msleep(1);
	}
}
void VipRecordWindow::stop()
{
/* #ifdef _MSC_VER
	if (d_data->handler) {
		delete static_cast<ScreenShot*>(d_data->handler);
		d_data->handler = nullptr;
	}
#endif*/

	d_data->timer.stop();
	d_data->thread->rec = nullptr;
	d_data->thread->wait();
	d_data->thread->images.clear();
	d_data->rect = QRect();
	d_data->screen = nullptr;
}

void VipRecordWindow::setRecording(bool start)
{
	vip_debug("setRecording %i\n", (int)start);
	if (start)
		this->start();
	else
		this->stop();
}

/*

#ifdef _MSC_VER
#include "qt_windows.h"

volatile int done = 0;

bool IsCloseEventReceived()
{
	//MSG msg;
	//return PeekMessage(&msg, nullptr, WM_CLOSE, WM_CLOSE, PM_NOREMOVE);
	return done;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
	case WM_QUIT:
	case WM_DESTROY:
		done = 1;
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static int registerCallback()
{
	WNDCLASSEX wc;

	// register WindowProcedure() as message callback function
	wc.lpfnWndProc = WndProc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = nullptr;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = L"test";
	wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	// assign other properties...

	if (!RegisterClassEx(&wc))
		return 0;
	return 0;
}
static int _registerCallback = registerCallback();

#else

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

volatile sig_atomic_t done = 0;

void term(int signum)
{
	done = 1;
}

bool IsCloseEventReceived()
{
	return done == 1;
}

static int registerHandler()
{
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;
	sigaction(SIGTERM, &action, nullptr);
	return 0;
}
static int _registerHandler = registerHandler();



#endif
*/




OpenStream::OpenStream()
  : QWidget()
{
	QHBoxLayout* hlay = new QHBoxLayout();
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
void OpenStream::setRencentPaths(const QStringList& lst)
{
	m_paths.clear();
	m_paths.addItems(lst);
	while (m_paths.count() > 20)
		m_paths.removeItem(20);
}

void OpenStream::openFilePath()
{
	VipMPEGLoader l;
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

	VipDisplayArea& area = *vipGetMainWindow()->displayArea();
	VipDisplayPlayerArea* plarea = area.currentDisplayPlayerArea();
	if (plarea) {
		VipProcessingPool* pool = plarea->processingPool();

		VipMPEGLoader* loader = new VipMPEGLoader(pool);
		loader->setPath(this->path());
		if (!loader->open(VipIODevice::ReadOnly)) {
			delete loader;
			VIP_LOG_ERROR("Cannot open video: " + this->path());
			return;
		}

		VipMultiDragWidget* bdw2 = vipCreateFromBaseDragWidget(vipCreateWidgetFromProcessingObject(loader));
		plarea->addWidget(bdw2);
	}
}



class VipRegisterRecordWindow::PrivateData
{
public:
	VipRecordWindow* rec_win;
	OpenStream* open_stream;
	QToolButton* rec;
};

VipRegisterRecordWindow::~VipRegisterRecordWindow() {}


VipRegisterRecordWindow::VipRegisterRecordWindow(VipMainWindow * win)
  : QObject(win)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	// retrieve the list of available video devices
	QStringList lst = VipMPEGLoader::listDevices();

	
	QToolButton* open = new QToolButton();
	open->setAutoRaise(true);
	open->setPopupMode(QToolButton::InstantPopup);
	open->setIcon(vipIcon("webcam.png"));
	open->setToolTip("Open a connected camera");
	{
		QMenu* menu = new QMenu(open);
		for (int i = 0; i < lst.size(); ++i)
			menu->addAction(lst[i]);
		menu->addSeparator();
		QWidgetAction* act = new QWidgetAction(menu);
		act->setDefaultWidget(d_data->open_stream = new OpenStream());
		menu->addAction(act);

		open->setMenu(menu);
		QObject::connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(openVideoStream(QAction*)));

		if (QAction* act = win->fileToolBar()->findChild<QAction*>("GenerateButton")) {
			win->fileToolBar()->insertWidget(act, open)->setToolTip("Open local webcam or network stream");
		}
		else
			win->fileToolBar()->addWidget(open)->setToolTip("Open local webcam or network stream");
	}

	// add button to make movies of thermavip (successive screenshots)
	d_data->rec = new QToolButton();
	d_data->rec->setIcon(vipIcon("RECORD.png"));
	d_data->rec->setToolTip("<b>Record your actions</b><br>Create a video from successive screenshots of Thermavip in order to record your actions.<br>"
			  "Check/uncheck this button to start/stop the recording.<br>Use the right arrow to modify the recording parameters.");
	d_data->rec->setAutoRaise(true);
	d_data->rec->setCheckable(true);
	VipDragMenu* menu = new VipDragMenu();
	d_data->rec_win = new VipRecordWindow();
	menu->setWidget(d_data->rec_win);
	d_data->rec->setMenu(menu);
	d_data->rec->setPopupMode(QToolButton::MenuButtonPopup);
	connect(d_data->rec, SIGNAL(clicked(bool)), this, SLOT(setRecording(bool)));
	connect(d_data->rec_win, SIGNAL(stateChanged(bool)), this, SLOT(setRecording(bool)), Qt::QueuedConnection);

	win->closeBar()->insertWidget(win->closeBar()->minimizeButton, d_data->rec);
}

void VipRegisterRecordWindow::installRecordWindow(VipMainWindow* win)
{
	/* static VipRegisterRecordWindow* inst = */new VipRegisterRecordWindow(win);
}

void VipRegisterRecordWindow::setRecording(bool enable)
{
	if (!d_data->rec_win || !d_data->rec)
		return;

	vip_debug("VipRegisterRecordWindow::setRecording %i\n", (int)enable);

	if (enable != d_data->rec_win->isRecording()) 
		d_data->rec_win->setRecording(enable); 
	
	d_data->rec->blockSignals(true);
	d_data->rec->setChecked(enable);
	d_data->rec->blockSignals(false);
}


// open webcams
void VipRegisterRecordWindow::openVideoStream(QAction* action)
{
	VipDisplayArea& area = *vipGetMainWindow()->displayArea();
	VipDisplayPlayerArea* plarea = area.currentDisplayPlayerArea();
	if (plarea) {
		VipProcessingPool* pool = plarea->processingPool();

		VipMPEGLoader* loader = new VipMPEGLoader(pool);
		if (!loader->open("video=" + action->text(), "dshow")) {
			delete loader;
			VIP_LOG_ERROR("Cannot open video stream: " + action->text());
			return;
		}

		// loader->setPath(action->text());

		VipMultiDragWidget* bdw2 = vipCreateFromBaseDragWidget(vipCreateWidgetFromProcessingObject(loader));
		plarea->addWidget(bdw2);
	}
}
