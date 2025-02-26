#include "VipRecordWindow.h"
#include "VipDisplayArea.h"
#include "VipToolWidget.h"
#include "VipCorrectedTip.h"
#include "VipLogging.h"
#include "p_VideoEncoder.h"

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

PlayerSelection::PlayerSelection(QWidget* parent)
  : VipComboBox(parent)
{
	this->setToolTip("Record a widget only, or select 'None' to record the full interface");
	this->addItem("None");

	connect(this, SIGNAL(openPopup()), this, SLOT(aboutToShow()));
	connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(selected()));
}
PlayerSelection::~PlayerSelection() {}

VipBaseDragWidget* PlayerSelection::selectedWidget() const
{
	return m_widget;
}

void PlayerSelection::aboutToShow()
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
void PlayerSelection::selected()
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

void VipRecordWindow::openFile()
{
	closeFile();
	try {
		m_rect = computeRect();
		m_screen = vipGetMainWindow()->screen();
		m_encoder = new VideoEncoder();
		QSize s = videoSize();
		m_encoder->Open(filename().toLatin1().data(), s.width(), s.height(), movieFps(), rate() * 1000);
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("Could not create video encoder: " + QString(e.what()));
		if (m_encoder) {
			delete m_encoder;
			m_encoder = nullptr;
		}
	}
}
void VipRecordWindow::closeFile()
{
	try {
		m_rect = QRect();
		if (m_encoder) {
			m_encoder->Close();
			delete m_encoder;
			m_encoder = nullptr;
		}
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("Could not create video encoder: " + QString(e.what()));
		if (m_encoder) {
			delete m_encoder;
			m_encoder = nullptr;
		}
	}
}
void VipRecordWindow::recordCurrentImage()
{
	try {
		if (m_encoder) {
			const QImage img = grabCurrentImage();
			m_encoder->AddFrame(img);
		}
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("Could not encode image: " + QString(e.what()));
	}
}

VipRecordWindow::VipRecordWindow(QWidget* parent)
  : QWidget(parent)
{
	QGridLayout* lay = new QGridLayout();

	int row = 0;

	lay->addWidget(&m_reset, row++, 0, 1, 2);

	lay->addWidget(new QLabel("Frame Rate (Kbits/s)"), row, 0);
	lay->addWidget(&m_rate, row++, 1);

	lay->addWidget(new QLabel("Acquisition FPS"), row, 0);
	lay->addWidget(&m_fps, row++, 1);

	lay->addWidget(new QLabel("Movie FPS"), row, 0);
	lay->addWidget(&m_movie_fps, row++, 1);

	lay->addWidget(new QLabel("Record delay"), row, 0);
	lay->addWidget(&m_recordDelay, row++, 1);

	lay->addWidget(&m_file, row++, 0, 1, 2);
	lay->addWidget(VipLineWidget::createHLine(), row++, 0, 1, 2);
	lay->addWidget(&m_recordOnPlay, row++, 0, 1, 2);
	lay->addWidget(&m_player, row, 0, 1, 2);
	setLayout(lay);

	m_reset.setText("Reset parameters");

	m_rate.setToolTip("Recording frame rate in Kbits/s");

	m_fps.setToolTip("Recording speed, set the sampling time between each frame");
	m_fps.setRange(1, 50);

	m_movie_fps.setToolTip("Actual movie FPS as saved in the video file");
	m_movie_fps.setRange(1, 50);

	m_recordDelay.setToolTip("Start recording after X seconds");
	m_recordDelay.setRange(0, 10);
	m_recordDelay.setValue(0);
	m_recordDelay.setSingleStep(0.5);

	m_file.setMode(VipFileName::Save);
	m_file.setFilters("Video file (*.mp4 *.mpg *.mpeg *.avi *.wmv *.gif *.mov)");
	m_file.edit()->setPlaceholderText("Recording file name");
	m_file.setDialogParent(vipGetMainWindow());

	m_recordOnPlay.setText("Sync. recording on play");
	m_recordOnPlay.setToolTip("<b>Start/Stop the recording when clicking the play/stop buttons.</b><br>"
				  "The recording will start when clicking the 'play' button and stop when clicking the 'stop' one.<br>"
				  "One image is recorded every time step.<br>"
				  "This option ignores the 'Acquisition FPS' parameter.");

	m_thread = new RecordThread();
	m_thread->rec = nullptr;

	m_timer.setSingleShot(false);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(grabImage()));
	connect(&m_reset, SIGNAL(clicked(bool)), this, SLOT(resetParams()));

	connect(vipGetMainWindow(), SIGNAL(aboutToClose()), this, SLOT(stop()));
	connect(&m_recordOnPlay, SIGNAL(clicked(bool)), this, SLOT(setRecordOnPlay(bool)));
	connect(this, SIGNAL(stopped()), this, SLOT(stop()));

	m_first_show = false;
	m_press_date = 0;
	m_buttons = Qt::MouseButtons();
	m_cursor = vipPixmap("std_cursor.png");
	m_timeout = -1;
	//m_handler = nullptr;
	m_encoder = nullptr;
	m_recordOnPlayEnabled = false;
	resetParams();
}

VipRecordWindow::~VipRecordWindow()
{
	stop();
	delete m_thread;
}

void VipRecordWindow::showEvent(QShowEvent*)
{
	if (!m_first_show) {
		resetParams();
		m_first_show = true;
	}
}

void VipRecordWindow::resetParams()
{
	m_rate.setValue(2500);
	m_fps.setValue(25);
	m_movie_fps.setValue(25);
	m_grow_time = 100;
	m_draw_mouse = true;
	m_pen = QPen(Qt::red);
	m_brush = QBrush(Qt::red);
}

void VipRecordWindow::waitForEnded()
{
	if (isRecording())
		m_thread->wait();
}

void VipRecordWindow::setRate(double rate)
{
	m_rate.setValue(rate);
}
double VipRecordWindow::rate() const
{
	return m_rate.value();
}

void VipRecordWindow::setRecordingFps(int fps)
{
	m_fps.setValue(fps);
}
int VipRecordWindow::recordingFps() const
{
	return m_fps.value();
}

void VipRecordWindow::setMovieFps(int fps)
{
	m_movie_fps.setValue(fps);
}
int VipRecordWindow::movieFps() const
{
	return m_movie_fps.value();
}

void VipRecordWindow::setFilename(const QString& fname)
{
	m_file.setFilename(fname);
}
QString VipRecordWindow::filename() const
{
	return m_file.filename();
}

void VipRecordWindow::setRecordDelay(double secs)
{
	m_recordDelay.setValue(secs);
}
double VipRecordWindow::recordDelay() const
{
	return m_recordDelay.value();
}

QSize VipRecordWindow::videoSize() const
{
	return m_rect.size();
}

void VipRecordWindow::setRecordOnPlay(bool enable)
{
	m_recordOnPlay.blockSignals(true);
	m_recordOnPlay.setChecked(enable);
	m_recordOnPlay.blockSignals(false);

	if (enable != m_recordOnPlayEnabled) {
		m_recordOnPlayEnabled = enable;
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
	return m_recordOnPlayEnabled;
}

/*#include <qwindow.h>
#include <qscreen.h>
QImage VipRecordWindow::grabScreenRect()
{
#ifdef _MSC_VER
	if (m_handler)
		return static_cast<ScreenShot*>(m_handler)->grab();

	if (VipBaseDragWidget* w = m_player.selectedWidget()) {
		m_rect = QRect(w->mapToGlobal(QPoint(0, 0)), w->mapToGlobal(QPoint(w->width(), w->height())));
		if (m_rect.width() % 2)
			m_rect.setRight(m_rect.right() - 1);
		if (m_rect.height() % 2)
			m_rect.setBottom(m_rect.bottom() - 1);
	}

	return ScreenCap(m_rect);
#else
	QScreen* screen = QGuiApplication::primaryScreen();
	QPixmap pix = screen->grabWindow(0, m_rect.left(), m_rect.top(), m_rect.width(), m_rect.height());
	// QPixmap pix  = screen->grabWindow(0);
	return pix.toImage();
#endif
}*/

QImage VipRecordWindow::grabCurrentImage()
{
	QImage img;

	QPoint topleft = m_rect.topLeft();
	{
		const QPixmap pix = m_screen->grabWindow(0);
		img = QImage(m_rect.size(), QImage::Format_ARGB32);
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
		

		if (m_draw_mouse) {
			double expand_time = m_grow_time; // ms

			p.setRenderHint(QPainter::Antialiasing);
			if (QApplication::mouseButtons() || ((QDateTime::currentMSecsSinceEpoch() - m_press_date) < expand_time && m_buttons)) {
				if (m_buttons == 0 && !((QDateTime::currentMSecsSinceEpoch() - m_press_date) < expand_time)) {
					m_press_date = QDateTime::currentMSecsSinceEpoch();
					m_buttons = QApplication::mouseButtons();
				}

				double radius = ((QDateTime::currentMSecsSinceEpoch() - m_press_date) / expand_time) * 9;
				radius = qMin(9., radius);

				p.setPen(m_pen);
				p.setBrush(m_brush);
				p.drawEllipse(QRectF(-radius / 2, -radius / 2, radius, radius).translated(QCursor::pos() - topleft));
			}
			else {
				if (m_buttons) {
					m_buttons = Qt::MouseButtons();
					m_press_date = QDateTime::currentMSecsSinceEpoch();
				}

				if ((QDateTime::currentMSecsSinceEpoch() - m_press_date) < expand_time) {
					double radius = ((QDateTime::currentMSecsSinceEpoch() - m_press_date) / expand_time) * 9;
					radius = qMin(9., radius);
					radius = 9 - radius;

					p.setPen(m_pen);
					p.setBrush(m_brush);
					p.drawEllipse(QRectF(-radius / 2, -radius / 2, radius, radius).translated(QCursor::pos() - topleft));
				}
			}
			// draw cursor
			p.drawPixmap(QRect(0, 0, m_cursor.width(), m_cursor.height()).translated(QCursor::pos() - topleft), m_cursor);
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
		m_thread->addImage(img);
}

void VipRecordWindow::setMouseGrowTime(int msecs)
{
	m_grow_time = msecs;
}
int VipRecordWindow::mouseGrowTime() const
{
	return m_grow_time;
}

void VipRecordWindow::setMousePen(const QPen& p)
{
	m_pen = p;
}
QPen VipRecordWindow::mousePen() const
{
	return m_pen;
}

void VipRecordWindow::setMouseBrush(const QBrush& b)
{
	m_brush = b;
}
QBrush VipRecordWindow::mouseBrush() const
{
	return m_brush;
}

void VipRecordWindow::setDrawMouse(bool enable)
{
	m_draw_mouse = enable;
}
bool VipRecordWindow::drawMouse() const
{
	return m_draw_mouse;
}

void VipRecordWindow::setScreenRect(const QRect& r)
{
	m_rect = r;
}
QRect VipRecordWindow::screenRect() const
{
	return m_rect;
}

void VipRecordWindow::setTimeout(int milli)
{
	m_timeout = milli;
}
int VipRecordWindow::timeout() const
{
	return m_timeout;
}

bool VipRecordWindow::isRecording() const
{
	return m_thread->isRunning() || m_process.state() == QProcess::Running;
}

void VipRecordWindow::setRecordExternalProcess(bool enable)
{
	m_process.write("exit\n");
	m_process.closeWriteChannel();
	m_process.terminate();

	if (enable) {
		QRect r = vipGetMainWindow()->geometry();
		QString rect = "--rect=" + QString::number(r.left()) + ":" + QString::number(r.top()) + ":" + QString::number(r.width()) + ":" + QString::number(r.height());
		QString command = "Thermavip --no_splashscreen --plugins=Ffmpeg " + rect + " --rate=" + QString::number(this->rate()) + " --fps=" + QString::number(recordingFps()) +
				  " --ffps=" + QString::number(movieFps()) + " --record=" + filename();

		m_process.start(command);
		m_process.waitForStarted();
	}
}

QRect VipRecordWindow::computeRect()
{
	QRect rect = vipGetMainWindow()->geometry();
	if (VipBaseDragWidget* w = m_player.selectedWidget()) 
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

	m_rect = computeRect();
	m_screen = vipGetMainWindow()->screen();

/* #ifdef _MSC_VER
	if (!m_handler && !m_rect.isEmpty()) {
		m_handler = new ScreenShot(m_rect);
	}
#endif*/

	m_timer.setInterval(1000.0 / recordingFps());

	m_thread->rec = this;
	m_thread->start();
	if (!m_recordOnPlayEnabled)
		m_timer.start();
	while (!m_thread->started) {
		QThread::msleep(1);
	}
}
void VipRecordWindow::stop()
{
/* #ifdef _MSC_VER
	if (m_handler) {
		delete static_cast<ScreenShot*>(m_handler);
		m_handler = nullptr;
	}
#endif*/

	m_timer.stop();
	m_thread->rec = nullptr;
	m_thread->wait();
	m_thread->images.clear();
	m_rect = QRect();
	m_screen = nullptr;
}

void VipRecordWindow::setState(bool start)
{
	vip_debug("setState %i\n", (int)start);
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
