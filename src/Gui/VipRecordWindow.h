#ifndef VIP_RECORD_WINDOW_H
#define VIP_RECORD_WINDOW_H

#include "VipStandardWidgets.h"
#include "VipDragWidget.h"

#include <qtimer.h>
#include <qevent.h>
#include <qeventloop.h>
#include <qprocess.h>

struct RecordThread;
class VipMPEGSaver;

/// @brief Select a player within current workspace
class VIP_GUI_EXPORT VipPlayerSelection : public VipComboBox
{
	Q_OBJECT
public:
	VipPlayerSelection(QWidget* parent = nullptr);
	~VipPlayerSelection();

	VipBaseDragWidget* selectedWidget() const;

private Q_SLOTS:
	void aboutToShow();
	void selected();

private:
	QPointer<VipBaseDragWidget> m_widget;
};

/// @brief Settings for thermavip window recording
class VIP_GUI_EXPORT VipRecordWindow : public QWidget
{
	Q_OBJECT
	friend struct RecordThread;

public:
	VipRecordWindow(QWidget* parent = nullptr);
	~VipRecordWindow();

	void setRate(double);
	double rate() const;

	void setRecordingFps(int);
	int recordingFps() const;

	void setMovieFps(int);
	int movieFps() const;

	void setFilename(const QString&);
	QString filename() const;

	void setRecordDelay(double secs);
	double recordDelay() const;

	QSize videoSize() const;

	bool recordOnPlay() const;

	void setMouseGrowTime(int msecs); // default to 200ms
	int mouseGrowTime() const;

	void setMousePen(const QPen&);
	QPen mousePen() const;

	void setMouseBrush(const QBrush&);
	QBrush mouseBrush() const;

	void setDrawMouse(bool);
	bool drawMouse() const;

	void setScreenRect(const QRect&);
	QRect screenRect() const;

	void setTimeout(int milli);
	int timeout() const;

	bool isRecording() const;

	QImage grabCurrentImage();

public Q_SLOTS:

	void setRecordOnPlay(bool);

	void start();
	void stop();
	void setRecording(bool);
	void resetParams();
	void waitForFinished();

private Q_SLOTS:
	// manual recording
	void openFile();
	void closeFile();
	void recordCurrentImage();

Q_SIGNALS:
	void started();
	void stopped();
	void stateChanged(bool);

protected:
	virtual void showEvent(QShowEvent* evt);

private Q_SLOTS:
	void grabImage();

private:
	QRect computeRect();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipMainWindow;

/// @brief Open video stream widget
class VIP_GUI_EXPORT OpenStream : public QWidget
{
	Q_OBJECT

public:
	OpenStream();

	QString path() const;
	QStringList recentPaths() const;
	void setRencentPaths(const QStringList& lst);
private Q_SLOTS:
	void openFilePath();
	void open();

private:
	QComboBox m_paths;
	QToolButton m_open;
};

  /// @brief Register the record window feature to the main interface
class VIP_GUI_EXPORT VipRegisterRecordWindow : public QObject
{
	Q_OBJECT
	VipRegisterRecordWindow(VipMainWindow *);

public:
	~VipRegisterRecordWindow();
	static void installRecordWindow(VipMainWindow*);

public Q_SLOTS:
	void setRecording(bool);
	void openVideoStream(QAction* action);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
