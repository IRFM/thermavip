
#include "VipMPEGOptionPanel.h"
#include "VipMPEGSaver.h"
#include "VipStandardWidgets.h"
#include <QGridLayout>

VipMPEGOptionPanel::VipMPEGOptionPanel(QWidget* parent)
  : QGroupBox("Encoding options", parent)
  , rateText(this)
  , fpsText(this)
  , saver(nullptr)
{
	QGridLayout* grid = new QGridLayout();

	rateText.setRange(0, 30000);
	rateText.setValue(20000);
	fpsText.setRange(0, 100);
	fpsText.setValue(25);
	threads.setRange(1, 12);
	threads.setValue(2);

	grid->addWidget(new QLabel("Rate(Kb/s)"), 0, 0);
	grid->addWidget(&rateText, 0, 1);

	grid->addWidget(new QLabel("Frames per second"), 1, 0);
	grid->addWidget(&fpsText, 1, 1);

	grid->addWidget(new QLabel("Encoding threads"), 2, 0);
	grid->addWidget(&threads, 2, 1);

	connect(&rateText, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));
	connect(&fpsText, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));
	connect(&threads, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));
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

	threads.setValue(s->additionalInfo().threads);

	rateText.blockSignals(false);
	fpsText.blockSignals(false);
}

void VipMPEGOptionPanel::updateSaver()
{
	VipMPEGIODeviceHandler info{ 320, 240, fpsText.value(), rateText.value() * 1000., -1, threads.value() };
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
