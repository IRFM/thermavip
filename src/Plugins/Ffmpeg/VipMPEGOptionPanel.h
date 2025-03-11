#ifndef VIP_MPEG_OPTION_PANEL_H
#define VIP_MPEG_OPTION_PANEL_H

#include <QGroupBox>
#include <QLabel>
#include <qspinbox.h>
#include <QLineEdit>

#include "VipFfmpegConfig.h"

class VipMPEGSaver;

/// @brief Editor for VipMPEGSaver device.
/// Allows to modify the recording rate, frequency, codec.
class FFMPEG_EXPORT VipMPEGOptionPanel : public QGroupBox
{
	Q_OBJECT

public:
	VipMPEGOptionPanel(QWidget* parent = nullptr);
	~VipMPEGOptionPanel();

	void setSaver(VipMPEGSaver* saver);

	QSpinBox rateText;
	QSpinBox fpsText;
	QSpinBox threads;
	VipMPEGSaver* saver;

private Q_SLOTS:
	void updateSaver();
};

#endif
