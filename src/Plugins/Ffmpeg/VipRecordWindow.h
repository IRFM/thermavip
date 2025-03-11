#pragma once

#include "VipStandardWidgets.h"
#include "VipDragWidget.h"
#include "VipFfmpegConfig.h"

#include <qtimer.h>
#include <qevent.h>
#include <qeventloop.h>
#include <qprocess.h>

struct RecordThread;
class VipMPEGSaver;

/// @brief Select a player within current workspace
class PlayerSelection : public VipComboBox
{
	Q_OBJECT
public:
	PlayerSelection(QWidget* parent = nullptr);
	~PlayerSelection();

	VipBaseDragWidget* selectedWidget() const;

private Q_SLOTS:
	void aboutToShow();
	void selected();

private:
	QPointer<VipBaseDragWidget> m_widget;
};

/// @brief Settings for thermavip window recording
class FFMPEG_EXPORT VipRecordWindow : public QWidget
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
	void setState(bool);
	void resetParams();
	void waitForEnded();

	void setRecordExternalProcess(bool);

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

	QToolButton m_reset;
	VipDoubleEdit m_rate;
	QSpinBox m_fps;
	QSpinBox m_movie_fps;
	QDoubleSpinBox m_recordDelay;
	VipFileName m_file;
	QCheckBox m_recordOnPlay;
	PlayerSelection m_player;
	bool m_recordOnPlayEnabled;
	QTimer m_timer;
	RecordThread* m_thread;
	bool m_first_show;
	qint64 m_press_date;
	Qt::MouseButtons m_buttons;
	QPixmap m_cursor;
	QRect m_rect;
	QScreen* m_screen{ nullptr };
	int m_timeout;

	int m_grow_time;
	QPen m_pen;
	QBrush m_brush;
	bool m_draw_mouse;

	QProcess m_process;
	VipMPEGSaver* m_encoder;
};
