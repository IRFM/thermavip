#ifndef MPEG_OPTION_PANEL_H
#define MPEG_OPTION_PANEL_H

#include <QGroupBox>
#include <QLabel>
#include <qspinbox.h>
#include <QLineEdit>



class MPEGSaver;

/**
* \class MPEGOptionPanel
* \brief MpegOptionPanel is used to define video
*/
class MPEGOptionPanel : public QGroupBox
{
	Q_OBJECT
	
public:

	MPEGOptionPanel(QWidget * parent = NULL);
	~MPEGOptionPanel();

	void setSaver(MPEGSaver * saver);

	QLabel videoCodec;
	QLabel rate;
	QLabel fps;
	QLineEdit videoCodecText;
	QSpinBox rateText;
	QSpinBox fpsText;
	
	MPEGSaver* saver;
	
private Q_SLOTS:
	void updateSaver();
};

#endif
