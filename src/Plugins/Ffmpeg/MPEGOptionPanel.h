#ifndef MPEG_OPTION_PANEL_H
#define MPEG_OPTION_PANEL_H

#include <QGroupBox>
#include <QLabel>
#include <qspinbox.h>
#include <QLineEdit>



class MPEGSaver;

/// @brief Editor for MPEGSaver device.
/// Allows to modify the recording rate, frequency, codec.
class MPEGOptionPanel : public QGroupBox
{
	Q_OBJECT
	
public:

	MPEGOptionPanel(QWidget * parent = nullptr);
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
