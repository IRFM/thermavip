
#include "VipMPEGOptionPanel.h"
#include "VipMPEGSaver.h"
#include "VipStandardWidgets.h"
#include <QGridLayout>

VipMPEGOptionPanel::VipMPEGOptionPanel(QWidget* parent)
  : QGroupBox("Encoding options", parent)
  , videoCodec("Video codec", this)
  , rate("Rate(Kb/s)", this)
  , fps("Frames per second", this)
  , videoCodecText(this)
  , rateText(this)
  , fpsText(this)
  , saver(nullptr)
{
	videoCodec.hide();
	videoCodecText.hide();
	QGridLayout* grid = new QGridLayout();

	rateText.setRange(0, 30000);
	rateText.setValue(20000);
	fpsText.setRange(0, 100);
	fpsText.setValue(25);

	grid->addWidget(&videoCodec, 0, 0);
	grid->addWidget(&videoCodecText, 0, 1);

	grid->addWidget(&rate, 3, 0);
	grid->addWidget(&rateText, 3, 1);

	grid->addWidget(&fps, 4, 0);
	grid->addWidget(&fpsText, 4, 1);

	connect(&rateText, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));
	connect(&fpsText, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));

	setLayout(grid);
}

VipMPEGOptionPanel::~VipMPEGOptionPanel() {}

void VipMPEGOptionPanel::setSaver(VipMPEGSaver* s)
{
	saver = s;
	rateText.blockSignals(true);
	fpsText.blockSignals(true);

	rateText.setValue(s->additionalInfo().rate / 1000.0);
	fpsText.setValue(s->additionalInfo().fps);

	rateText.blockSignals(false);
	fpsText.blockSignals(false);
}

void VipMPEGOptionPanel::updateSaver()
{
	VipMPEGIODeviceHandler info(320, 240, fpsText.value(), rateText.value() * 1000);
	saver->setAdditionalInfo(info);
}

static QWidget* editMPEGSaver(VipMPEGSaver* obj)
{
	VipMPEGOptionPanel* editor = new VipMPEGOptionPanel();
	editor->setSaver(obj);
	return editor;
}

static int registerEditor()
{
	vipFDObjectEditor().append<QWidget*(VipMPEGSaver*)>(editMPEGSaver);
	return 0;
}

static int _registerEditors = registerEditor();
