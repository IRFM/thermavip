#include "VipPyProcessingEditor.h"
#include "VipPyRegisterProcessing.h"
#include "VipTabEditor.h"
#include "VipPyShellWidget.h"
#include "VipProcessingObjectEditor.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipCore.h"
#include "VipSymbol.h"
#include "VipPlotShape.h"
#include "VipPlotCurve.h"
#include "VipDisplayObject.h"
#include "VipIODevice.h"
#include "VipGui.h"
#include "VipPyFitProcessing.h"

#include <qcombobox.h>
#include <qpushbutton.h>
#include <qclipboard.h>
#include <qapplication.h>
#include <qtooltip.h>
#include <QBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <qplaintextedit.h>
#include <qradiobutton.h>
#include <qmessagebox.h>
#include <qgroupbox.h>
#include <qtreewidget.h>
#include <qheaderview.h>
#include <qgridlayout.h>

#include <set>


class VipPySignalGeneratorEditor::PrivateData
{
public:
	VipTabEditor editor;
	QComboBox unit;
	VipDoubleEdit sampling;

	QRadioButton sequential, temporal;

	QCheckBox usePoolTimeRange;
	VipDoubleEdit start, end;

	// gather in widgets
	QWidget* samplingWidget;
	QWidget* rangeWidget;

	QPointer<VipPySignalGenerator> generator;
};

VipPySignalGeneratorEditor::VipPySignalGeneratorEditor(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QHBoxLayout* slay = new QHBoxLayout();
	slay->setContentsMargins(0, 0, 0, 0);
	slay->addWidget(new QLabel("Sampling"));
	slay->addWidget(&d_data->sampling);
	d_data->samplingWidget = new QWidget();
	d_data->samplingWidget->setLayout(slay);

	QHBoxLayout* rlay = new QHBoxLayout();
	rlay->setContentsMargins(0, 0, 0, 0);
	rlay->addWidget(new QLabel("Start"));
	rlay->addWidget(&d_data->start);
	rlay->addWidget(new QLabel("End"));
	rlay->addWidget(&d_data->end);
	d_data->rangeWidget = new QWidget();
	d_data->rangeWidget->setLayout(rlay);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->editor, 10);
	lay->addWidget(&d_data->unit);
	lay->addWidget(d_data->samplingWidget);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&d_data->sequential);
	lay->addWidget(&d_data->temporal);
	lay->addWidget(&d_data->usePoolTimeRange);
	lay->addWidget(d_data->rangeWidget);
	lay->addStretch(1);
	lay->addWidget(VipLineWidget::createHLine());
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	d_data->editor.setDefaultText("value = (t - st) * 10");
	d_data->editor.setDefaultColorSchemeType("Python");
	if (!d_data->editor.currentEditor())
		d_data->editor.newFile();

	d_data->editor.setToolTip(tr("Python script with a <i>value</i> variable that can be evaluated to numerical value or a numpy array.<br><br>"
				     "Example:<br>"
				     "    <b>value = 2*cos((t-st)/10)</b><br>"
				     "<i>t</i> represents the device time in seconds.<br>"
				     "<i>st</i> represents the device starting time in seconds.<br>"
				     "<i>value</i> represents the output value (numerical value or numpy array).<br>"));

	d_data->sampling.setToolTip(tr("Device sampling time in seconds"));
	/*"For sequential device, the sampling time (in seconds) is used to determine the elapsed time between 2 generated values.<br>"
	"For temporal device, this is the inverse of the device frequency."
));*/

	d_data->sequential.setText("Sequential device");
	d_data->temporal.setText("Temporal device");

	d_data->sequential.setToolTip(tr("Create a sequential (streaming) video or plot device"));
	d_data->temporal.setToolTip(tr("Create a temporal video or plot device"));

	d_data->usePoolTimeRange.setText("Find best time limits");
	d_data->usePoolTimeRange.setToolTip(tr("Use the current workspace to find the best time range"));
	d_data->start.setToolTip(tr("Device start time in seconds"));
	d_data->end.setToolTip(tr("Device end time in seconds"));

	d_data->unit.setEditable(true);
	d_data->unit.lineEdit()->setPlaceholderText("Signal unit (optional)");
	d_data->unit.setToolTip("<b>Signal unit (optional)</b><br>Enter the signal y unit or the image z unit (if the output signal is an image)");

	d_data->sequential.setChecked(true);
	d_data->usePoolTimeRange.setVisible(false);
	d_data->rangeWidget->setVisible(false);
	d_data->editor.setVisible(false);

	connect(&d_data->sequential, SIGNAL(clicked(bool)), this, SLOT(updateVisibility()));
	connect(&d_data->temporal, SIGNAL(clicked(bool)), this, SLOT(updateVisibility()));
	connect(&d_data->usePoolTimeRange, SIGNAL(clicked(bool)), this, SLOT(updateVisibility()));

	// connect(&d_data->editor, SIGNAL(Applied()), this, SLOT(updateGenerator()));
	connect(&d_data->sampling, SIGNAL(valueChanged(double)), this, SLOT(updateGenerator()));
	connect(&d_data->start, SIGNAL(valueChanged(double)), this, SLOT(updateGenerator()));
	connect(&d_data->end, SIGNAL(valueChanged(double)), this, SLOT(updateGenerator()));
	connect(&d_data->sequential, SIGNAL(clicked(bool)), this, SLOT(updateGenerator()));
	connect(&d_data->temporal, SIGNAL(clicked(bool)), this, SLOT(updateGenerator()));
	connect(&d_data->usePoolTimeRange, SIGNAL(clicked(bool)), this, SLOT(updateGenerator()));
}
VipPySignalGeneratorEditor::~VipPySignalGeneratorEditor() {}

void VipPySignalGeneratorEditor::setGenerator(VipPySignalGenerator* gen)
{
	if (gen != d_data->generator) {
		d_data->generator = gen;
		updateWidget();
	}
}

VipPySignalGenerator* VipPySignalGeneratorEditor::generator() const
{
	return d_data->generator;
}

void VipPySignalGeneratorEditor::updateGenerator()
{
	if (VipPySignalGenerator* gen = d_data->generator) {
		gen->propertyAt(0)->setData((qint64)(d_data->sampling.value() * 1000000000ull));
		if (VipTextEditor* ed = d_data->editor.currentEditor())
			gen->propertyAt(3)->setData(ed->toPlainText());

		VipTimeRange range = VipInvalidTimeRange;

		if (d_data->temporal.isChecked()) {
			if (d_data->usePoolTimeRange.isChecked()) {
				if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
					range = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->timeLimits();
					if (range.first == VipInvalidTime)
						range = VipTimeRange(0ull, 10000000000ull);
				}
				else {
					range = VipTimeRange(0ull, 10000000000ull);
				}
			}
			else {
				range.first = ((qint64)(d_data->start.value() * 1000000000ull));
				range.second = ((qint64)(d_data->end.value() * 1000000000ull));
			}
		}

		gen->propertyAt(1)->setData(range.first);
		gen->propertyAt(2)->setData(range.second);
		gen->propertyAt(4)->setData(d_data->unit.currentText());

		updateWidget();

		if ((gen->isOpen() || gen->property("shouldOpen").toBool()) && gen->deviceType() != VipIODevice::Sequential) {
			// new sampling time or time range for temporal device: recompute it
			gen->close();
			if (gen->open(VipIODevice::ReadOnly))
				gen->setProperty("shouldOpen", false);
			else
				gen->setProperty("shouldOpen", true);
			gen->reload();
		}
	}
}

void VipPySignalGeneratorEditor::updateVisibility()
{
	d_data->start.setEnabled(!d_data->usePoolTimeRange.isChecked());
	d_data->end.setEnabled(!d_data->usePoolTimeRange.isChecked());

	d_data->usePoolTimeRange.setVisible(d_data->temporal.isChecked());
	d_data->rangeWidget->setVisible(d_data->temporal.isChecked());
}

void VipPySignalGeneratorEditor::updateWidget()
{
	if (VipPySignalGenerator* gen = d_data->generator) {
		// complex Python code
		d_data->editor.setVisible(true);
		if (!d_data->editor.currentEditor())
			d_data->editor.newFile();

		d_data->editor.blockSignals(true);
		d_data->editor.currentEditor()->setPlainText(gen->propertyAt(3)->value<QString>());
		d_data->editor.blockSignals(false);

		d_data->sequential.blockSignals(true);
		if (gen->isOpen()) {
			d_data->sequential.setChecked(gen->deviceType() == VipIODevice::Sequential);
			d_data->temporal.setChecked(gen->deviceType() != VipIODevice::Sequential);
			updateVisibility();
		}
		d_data->sequential.blockSignals(false);

		d_data->sampling.blockSignals(true);
		d_data->sampling.setValue(gen->propertyAt(0)->value<qint64>() / 1000000000.0);
		d_data->sampling.blockSignals(false);

		d_data->start.blockSignals(true);
		d_data->end.blockSignals(true);

		VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(VipPlayer2D::dropTarget());

		if (pl) {
			// set the list of possible units
			QList<VipAbstractScale*> scales = pl->leftScales();
			QStringList units;
			for (int i = 0; i < scales.size(); ++i)
				units.append(scales[i]->title().text());

			d_data->unit.clear();
			d_data->unit.addItems(units);
			d_data->unit.setCurrentText(gen->propertyAt(5)->value<QString>());
		}

		if (gen->propertyAt(1)->value<qint64>() == VipInvalidTime && gen->propertyAt(2)->value<qint64>() == VipInvalidTime) {
			d_data->start.setValue(0);
			d_data->end.setValue(10);

			// use the drop target (if this is a VipPlotPlayer) to find a better time range
			if (pl) {
				if (pl->haveTimeUnit()) {
					VipInterval inter = pl->xScale()->scaleDiv().bounds().normalized();
					d_data->start.setValue(inter.minValue() * 1e-9);
					d_data->end.setValue(inter.maxValue() * 1e-9);
				}
			}
		}
		else {
			d_data->start.setValue(gen->propertyAt(1)->value<qint64>() / 1000000000.0);
			d_data->end.setValue(gen->propertyAt(2)->value<qint64>() / 1000000000.0);
		}

		d_data->start.blockSignals(false);
		d_data->end.blockSignals(false);
	}
}

VipPySignalGenerator* VipPySignalGeneratorEditor::createGenerator()
{
	VipPySignalGenerator* gen = new VipPySignalGenerator();
	VipPySignalGeneratorEditor* editor = new VipPySignalGeneratorEditor();
	editor->setGenerator(gen);

	VipGenericDialog dialog(editor, "Edit Python generator");
	dialog.setMinimumWidth(300);
	if (dialog.exec() == QDialog::Accepted) {
		editor->updateGenerator();
		if (gen->open(VipIODevice::ReadOnly))
			return gen;
	}

	delete gen;
	return nullptr;
}

static QWidget* editPySignalGenerator(VipPySignalGenerator* gen)
{
	VipPySignalGeneratorEditor* editor = new VipPySignalGeneratorEditor();
	editor->setGenerator(gen);
	return editor;
}


class VipPyParametersEditor::PrivateData
{
public:
	QList<QWidget*> editors;
	QList<VipPyProcessing::Parameter> params;
	QList<QVariant> previous;
	QPointer<VipPyProcessing> processing;
};

VipPyParametersEditor::VipPyParametersEditor(VipPyProcessing* p)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->processing = p;
	d_data->params = p->extractStdProcessingParameters();
	QVariantMap args = p->stdProcessingParameters();

	if (d_data->params.size()) {
		QGridLayout* lay = new QGridLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		// lay->setSizeConstraint(QLayout::SetMinimumSize);

		for (int i = 0; i < d_data->params.size(); ++i) {
			VipPyProcessing::Parameter pr = d_data->params[i];
			QVariant value = args[pr.name];

			if (pr.type == "int") {
				QSpinBox* box = new QSpinBox();
				if (pr.min.size())
					box->setMinimum(pr.min.toInt());
				else
					box->setMinimum(-INT_MAX);
				if (pr.max.size())
					box->setMaximum(pr.max.toInt());
				else
					box->setMaximum(INT_MAX);
				if (pr.step.size())
					box->setSingleStep(pr.step.toInt());
				box->setValue(pr.defaultValue.toInt());
				if (value.userType())
					box->setValue(value.toInt());
				connect(box, SIGNAL(valueChanged(int)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(box, i, 1);
				d_data->editors << box;
			}
			else if (pr.type == "float") {
				VipDoubleSpinBox* box = new VipDoubleSpinBox();
				if (pr.min.size())
					box->setMinimum(pr.min.toDouble());
				else
					box->setMinimum(-FLT_MAX);
				if (pr.max.size())
					box->setMaximum(pr.max.toDouble());
				else
					box->setMaximum(FLT_MAX);
				if (pr.step.size())
					box->setSingleStep(pr.step.toDouble());
				else
					box->setSingleStep(0);
				box->setDecimals(6);
				box->setValue(pr.defaultValue.toDouble());
				if (value.userType())
					box->setValue(value.toDouble());
				connect(box, SIGNAL(valueChanged(double)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(box, i, 1);
				d_data->editors << box;
			}
			else if (pr.type == "bool") {
				QCheckBox* box = new QCheckBox(vipSplitClassname(pr.name));
				box->setChecked(pr.defaultValue.toInt());
				if (value.userType())
					box->setChecked(value.toInt());
				connect(box, SIGNAL(clicked(bool)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(box, i, 0, 1, 2);
				d_data->editors << box;
			}
			else if (pr.type == "str" && pr.enumValues.size()) {
				VipComboBox* box = new VipComboBox();
				box->addItems(pr.enumValues);
				box->setCurrentText(pr.defaultValue);
				if (value.userType())
					box->setCurrentText(value.toString());
				connect(box, SIGNAL(valueChanged(const QString&)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(box, i, 1);
				d_data->editors << box;
			}
			else if (pr.type == "other") {
				Vip2DDataEditor* ed = new Vip2DDataEditor();
				if (value.userType())
					ed->setValue(value.value<VipOtherPlayerData>());
				ed->displayVLines(i > 0, i < d_data->params.size() - 1);
				connect(ed, SIGNAL(changed()), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(ed, i, 0, 1, 2);
				d_data->editors << ed;
			}
			else {
				VipLineEdit* line = new VipLineEdit();
				line->setText(pr.defaultValue);
				if (value.userType())
					line->setText(value.value<QString>());
				connect(line, SIGNAL(valueChanged(const QString&)), this, SLOT(updateProcessing()), Qt::QueuedConnection);
				lay->addWidget(new QLabel(vipSplitClassname(pr.name)), i, 0);
				lay->addWidget(line, i, 1);
				d_data->editors << line;
			}

			d_data->previous << QVariant();
		}

		// lay->setSizeConstraint(QLayout::SetFixedSize);
		setLayout(lay);
	}
}
VipPyParametersEditor::~VipPyParametersEditor() {}

void VipPyParametersEditor::updateProcessing()
{
	if (!d_data->processing)
		return;

	QVariantMap map;
	for (int i = 0; i < d_data->params.size(); ++i) {
		QString name = d_data->params[i].name;
		QWidget* ed = d_data->editors[i];
		QVariant value;

		if (QSpinBox* box = qobject_cast<QSpinBox*>(ed))
			value = box->value();
		else if (QDoubleSpinBox* box = qobject_cast<QDoubleSpinBox*>(ed))
			value = box->value();
		else if (QCheckBox* box = qobject_cast<QCheckBox*>(ed))
			value = int(box->isChecked());
		else if (VipComboBox* box = qobject_cast<VipComboBox*>(ed))
			value = "'" + box->currentText() + "'";
		else if (VipLineEdit* line = qobject_cast<VipLineEdit*>(ed))
			value = "'" + line->text() + "'";
		else if (Vip2DDataEditor* other = qobject_cast<Vip2DDataEditor*>(ed))
			value = QVariant::fromValue(other->value());
		map[name] = value;
	}

	d_data->processing->setStdProcessingParameters(map);
	d_data->processing->reload();

	Q_EMIT changed();
}

class VipPyProcessingEditor::PrivateData
{
public:
	VipTabEditor editor;
	QPointer<VipPyProcessing> proc;
	QLabel maxTime;
	QSpinBox maxTimeEdit;
	QLabel resampleText;
	QComboBox resampleBox;
	VipPyApplyToolBar apply;
	VipPyParametersEditor* params;
};

VipPyProcessingEditor::VipPyProcessingEditor()
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->params = nullptr;

	QGridLayout* hlay = new QGridLayout();
	hlay->addWidget(&d_data->maxTime, 0, 0);
	hlay->addWidget(&d_data->maxTimeEdit, 0, 1);
	hlay->addWidget(&d_data->resampleText, 1, 0);
	hlay->addWidget(&d_data->resampleBox, 1, 1);
	hlay->setContentsMargins(0, 0, 0, 0);
	d_data->maxTime.setText("Python script timeout (ms)");
	d_data->maxTimeEdit.setRange(-1, 200000);
	d_data->maxTimeEdit.setValue(5000);
	d_data->maxTimeEdit.setToolTip("Maximum time for the script execution.\n-1 means no maximum time.");
	d_data->resampleText.setText("Resample input signals based on");
	d_data->resampleBox.addItem("union");
	d_data->resampleBox.addItem("intersection");
	d_data->resampleBox.setCurrentIndex(1);

	d_data->editor.setDefaultColorSchemeType("Python");

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(&d_data->editor, 1);
	vlay->addWidget(&d_data->apply);
	vlay->setContentsMargins(0, 0, 0, 0);
	setLayout(vlay);

	connect(d_data->apply.applyButton(), SIGNAL(clicked(bool)), this, SLOT(applyRequested()));
	connect(d_data->apply.registerButton(), SIGNAL(clicked(bool)), this, SLOT(registerProcessing()));
	connect(d_data->apply.manageButton(), SIGNAL(clicked(bool)), this, SLOT(manageProcessing()));
	connect(&d_data->maxTimeEdit, SIGNAL(valueChanged(int)), this, SLOT(updatePyProcessing()));
	connect(&d_data->resampleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePyProcessing()));
}

VipPyProcessingEditor::~VipPyProcessingEditor() {}

VipPyApplyToolBar* VipPyProcessingEditor::buttons() const
{
	return &d_data->apply;
}

void VipPyProcessingEditor::updatePyProcessing()
{
	if (d_data->proc) {
		d_data->proc->setMaxExecutionTime(d_data->maxTimeEdit.value());
		if (d_data->resampleBox.currentText() != d_data->proc->propertyAt(0)->value<QString>()) {
			d_data->proc->propertyAt(0)->setData(d_data->resampleBox.currentText());
			d_data->proc->reload();
		}
	}
}

void VipPyProcessingEditor::setPyProcessing(VipPyProcessing* proc)
{
	d_data->proc = proc;
	if (proc) {
		if (proc->inputCount() > 1 && proc->inputAt(0)->probe().data().userType() == qMetaTypeId<VipPointVector>() && proc->resampleEnabled()) {
			d_data->resampleBox.setVisible(true);
			d_data->resampleText.setVisible(true);
		}
		else {
			d_data->resampleBox.setVisible(false);
			d_data->resampleText.setVisible(false);
		}
		d_data->resampleBox.setCurrentText(proc->propertyAt(0)->data().value<QString>());

		if (d_data->params) {
			d_data->params->setAttribute(Qt::WA_DeleteOnClose);
			d_data->params->close();
		}

		QList<VipPyProcessing::Parameter> params = proc->extractStdProcessingParameters();
		if (params.size()) {
			d_data->params = new VipPyParametersEditor(proc);
			this->layout()->addWidget(d_data->params);
			d_data->editor.hide();
			d_data->apply.hide();
		}
		else if (proc->stdPyProcessingFile().isEmpty()) {
			d_data->editor.show();
			d_data->apply.show();
			if (!d_data->editor.currentEditor())
				d_data->editor.newFile();
			d_data->editor.currentEditor()->setPlainText(proc->propertyAt(1)->data().value<QString>());

			/*if (proc->lastError().isNull())
			d_data->editor.SetApplied();
			else if (proc->lastError().traceback == "Uninitialized")
			d_data->editor.SetUninit();
			else
			d_data->editor.SetError();*/
		}
		else {
			d_data->editor.hide();
			d_data->apply.hide();
		}
	}
}

void VipPyProcessingEditor::applyRequested()
{
	if (d_data->proc) {
		d_data->proc->propertyAt(1)->setData(VipAnyData(QString(d_data->editor.currentEditor()->toPlainText()), VipInvalidTime));
		d_data->proc->reload();
		d_data->proc->wait();
		/*if(d_data->proc->lastError().isNull())
		d_data->editor.SetApplied();
		else
		d_data->editor.SetError();*/
	}
}

void VipPyProcessingEditor::uninitRequested()
{
	if (d_data->proc) {
		d_data->proc->propertyAt(1)->setData(VipAnyData(QString(d_data->editor.currentEditor()->toPlainText()), VipInvalidTime));
		d_data->proc->reload();
		d_data->proc->wait();
		// d_data->editor.SetUninit();
	}
}

void VipPyProcessingEditor::registerProcessing()
{
	if (!d_data->proc)
		return;

	// register the current processing
	VipPySignalFusionProcessingManager* m = new VipPySignalFusionProcessingManager();
	m->setManagerVisible(false);
	m->setCreateNewVisible(true);
	m->setCategory("Python/");
	VipGenericDialog dialog(m, "Register new processing");
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = d_data->proc->registerThisProcessing(m->category(), m->name(), m->description(), m->overwrite());
		if (!ret)
			vipWarning("Operation failure", "Failed to register this processing.\nPlease make sure you entered a valid name and category.");
		else {
			// make sure to update all processing menu
			vipGetMainWindow()->displayArea()->resetItemSelection();
		}
	}
}

void VipPyProcessingEditor::manageProcessing()
{
	vipOpenProcessingManager();
}

static VipPyProcessingEditor* editPyProcessing(VipPyProcessing* proc)
{
	VipPyProcessingEditor* editor = new VipPyProcessingEditor();
	editor->setPyProcessing(proc);
	return editor;
}

int registerEditPyProcessing()
{
	vipFDObjectEditor().append<QWidget*(VipPyProcessing*)>(editPyProcessing);
	vipFDObjectEditor().append<QWidget*(VipPySignalGenerator*)>(editPySignalGenerator);
	return 0;
}
static int _registerEditPyProcessing = registerEditPyProcessing();

class VipPyApplyToolBar::PrivateData
{
public:
	QPushButton* apply;
	QToolButton* save;
	QToolButton* manage;
};
VipPyApplyToolBar::VipPyApplyToolBar(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->apply = new QPushButton();
	d_data->apply->setText("Update/Apply processing");
	d_data->apply->setToolTip("<b>Update/Apply the processing</b><br>"
				  "Use this button to reapply the processing if you modified the Python scripts, the output signal title or the signal unit.");
	d_data->save = new QToolButton();
	d_data->save->setAutoRaise(true);
	d_data->save->setIcon(vipIcon("save.png"));
	d_data->save->setToolTip(
	  "<b>Register this processing</b><br>Register this processing and save it into your session.<br>This new processing will be available through the processing menu shortcut.");
	d_data->manage = new QToolButton();
	d_data->manage->setIcon(vipIcon("tools.png"));
	d_data->manage->setToolTip("<b>Manage registered processing</b><br>Manage (edit/suppress) the processing that you already registered within your session.");
	QHBoxLayout* blay = new QHBoxLayout();
	blay->setContentsMargins(0, 0, 0, 0);
	blay->setSpacing(0);
	blay->addWidget(d_data->apply);
	blay->addWidget(d_data->save);
	blay->addWidget(d_data->manage);
	setLayout(blay);
}
VipPyApplyToolBar::~VipPyApplyToolBar() {}

QPushButton* VipPyApplyToolBar::applyButton() const
{
	return d_data->apply;
}
QToolButton* VipPyApplyToolBar::registerButton() const
{
	return d_data->save;
}
QToolButton* VipPyApplyToolBar::manageButton() const
{
	return d_data->manage;
}

class VipPySignalFusionProcessingManager::PrivateData
{
public:
	QWidget* createWidget;
	QLineEdit* name;
	QLineEdit* category;
	QCheckBox* overwrite;
	QPlainTextEdit* description;

	QWidget* editWidget;
	QTreeWidget* procList;
	QPlainTextEdit* procDescription;
	VipPySignalFusionProcessingEditor* procEditor;
	VipPyProcessingEditor* pyEditor;
};


static QGroupBox* flatGroupBox(const QString& title)
{
	QGroupBox* g = new QGroupBox(title);
	g->setFlat(true);
	QFont f = g->font();
	f.setBold(true);
	g->setFont(f);
	return g;
}

VipPySignalFusionProcessingManager::VipPySignalFusionProcessingManager(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->createWidget = new QWidget();
	d_data->name = new QLineEdit();
	d_data->name->setToolTip("Enter the processing name (mandatory)");
	d_data->name->setPlaceholderText("Processing name");
	d_data->overwrite = new QCheckBox("Overwrite existing processing");
	d_data->overwrite->setChecked(true);
	d_data->category = new QLineEdit();
	d_data->category->setToolTip("<b>Enter the processing category (mandatory)</b><br>You can define as many sub-categories as you need using a '/' separator.");
	d_data->category->setPlaceholderText("Processing category");
	d_data->category->setText("Data Fusion/");
	d_data->description = new QPlainTextEdit();
	d_data->description->setPlaceholderText("Processing short description (optional)");
	d_data->description->setToolTip("Processing short description (optional)");
	d_data->description->setMinimumHeight(100);

	QGridLayout* glay = new QGridLayout();
	int row = 0;
	glay->addWidget(flatGroupBox("Register new processing"), row++, 0, 1, 2);
	glay->addWidget(new QLabel("Processing name: "), row, 0);
	glay->addWidget(d_data->name, row++, 1);
	glay->addWidget(new QLabel("Processing category: "), row, 0);
	glay->addWidget(d_data->category, row++, 1);
	glay->addWidget(d_data->overwrite, row++, 0, 1, 2);
	glay->addWidget(d_data->description, row++, 0, 1, 2);
	d_data->createWidget->setLayout(glay);

	d_data->editWidget = new QWidget();
	d_data->procList = new QTreeWidget();
	d_data->procList->header()->show();
	d_data->procList->setColumnCount(2);
	d_data->procList->setColumnWidth(0, 200);
	d_data->procList->setColumnWidth(1, 200);
	d_data->procList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->procList->setFrameShape(QFrame::NoFrame);
	d_data->procList->setIndentation(10);
	d_data->procList->setHeaderLabels(QStringList() << "Name"
							<< "Category");
	d_data->procList->setMinimumHeight(150);
	d_data->procList->setMaximumHeight(200);
	d_data->procList->setContextMenuPolicy(Qt::CustomContextMenu);

	d_data->procDescription = new QPlainTextEdit();
	d_data->procDescription->setPlaceholderText("Processing short description (optional)");
	d_data->procDescription->setToolTip("Processing short description (optional)");
	d_data->procDescription->setMinimumHeight(100);

	d_data->procEditor = new VipPySignalFusionProcessingEditor();
	d_data->procEditor->buttons()->manageButton()->hide();
	d_data->procEditor->buttons()->registerButton()->hide();
	d_data->pyEditor = new VipPyProcessingEditor();
	d_data->pyEditor->buttons()->manageButton()->hide();
	d_data->pyEditor->buttons()->registerButton()->hide();
	d_data->pyEditor->setMaximumHeight(400);
	d_data->pyEditor->setMinimumHeight(200);
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(flatGroupBox("Edit registered processing"));
	vlay->addWidget(d_data->procList);
	vlay->addWidget(d_data->procDescription);
	vlay->addWidget(d_data->procEditor);
	vlay->addWidget(d_data->pyEditor);
	d_data->editWidget->setLayout(vlay);
	d_data->procEditor->setEnabled(false);
	d_data->procDescription->setEnabled(false);
	d_data->procDescription->setMaximumHeight(120);
	d_data->pyEditor->hide();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(d_data->createWidget);
	lay->addWidget(d_data->editWidget);
	setLayout(lay);

	connect(d_data->procList, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(itemClicked(QTreeWidgetItem*, int)));
	connect(d_data->procList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(itemDoubleClicked(QTreeWidgetItem*, int)));
	connect(d_data->procList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showMenu(const QPoint&)));
	connect(d_data->procList, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
	connect(d_data->procDescription, SIGNAL(textChanged()), this, SLOT(descriptionChanged()));
}

VipPySignalFusionProcessingManager::~VipPySignalFusionProcessingManager() {}

QString VipPySignalFusionProcessingManager::name() const
{
	return d_data->name->text();
}
QString VipPySignalFusionProcessingManager::category() const
{
	return d_data->category->text();
}
QString VipPySignalFusionProcessingManager::description() const
{
	return d_data->description->toPlainText();
}

bool VipPySignalFusionProcessingManager::overwrite() const
{
	return d_data->overwrite->isChecked();
}

void VipPySignalFusionProcessingManager::setName(const QString& name)
{
	d_data->name->setText(name);
}
void VipPySignalFusionProcessingManager::setCategory(const QString& cat)
{
	d_data->category->setText(cat);
}
void VipPySignalFusionProcessingManager::setDescription(const QString& desc)
{
	d_data->description->setPlainText(desc);
}

void VipPySignalFusionProcessingManager::setOverwrite(bool enable)
{
	d_data->overwrite->setChecked( enable);
}

void VipPySignalFusionProcessingManager::setManagerVisible(bool vis)
{
	d_data->editWidget->setVisible(vis);
}
bool VipPySignalFusionProcessingManager::managerVisible() const
{
	return d_data->editWidget->isVisible();
}

void VipPySignalFusionProcessingManager::setCreateNewVisible(bool vis)
{
	d_data->createWidget->setVisible(vis);
}
bool VipPySignalFusionProcessingManager::createNewVisible() const
{
	return d_data->createWidget->isVisible();
}

void VipPySignalFusionProcessingManager::updateWidget()
{
	// get all processing infos and copy them to d_data->processings
	QList<VipProcessingObject::Info> infos = VipPyRegisterProcessing::customProcessing();

	// we set valid inputs to the processing in order to be applied properly
	VipPointVector v(100);
	for (int i = 0; i < 100; ++i)
		v[i] = VipPoint(i * 1000, i * 1000);

	// update tree widget
	d_data->procList->clear();
	for (int i = 0; i < infos.size(); ++i) {
		VipProcessingObject::Info info = infos[i];
		VipProcessingObject* tmp = nullptr;
		// copy the init member
		if (VipPySignalFusionProcessingPtr p = info.init.value<VipPySignalFusionProcessingPtr>()) {
			VipPySignalFusionProcessingPtr init(new VipPySignalFusionProcessing());
			init->topLevelInputAt(0)->toMultiInput()->resize(p->topLevelInputAt(0)->toMultiInput()->count());
			init->propertyName("x_algo")->setData(p->propertyName("x_algo")->data());
			init->propertyName("y_algo")->setData(p->propertyName("y_algo")->data());
			init->propertyName("output_title")->setData(p->propertyName("output_title")->data());
			init->propertyName("output_unit")->setData(p->propertyName("output_unit")->data());
			init->propertyName("Time_range")->setData(p->propertyName("Time_range")->data());
			info.init = QVariant::fromValue(init);
			tmp = init.data();
		}
		else if (VipPyProcessingPtr p = info.init.value<VipPyProcessingPtr>()) {
			VipPyProcessingPtr init(new VipPyProcessing());
			init->topLevelInputAt(0)->toMultiInput()->resize(p->topLevelInputAt(0)->toMultiInput()->count());
			init->propertyName("code")->setData(p->propertyName("code")->data());
			init->propertyName("Time_range")->setData(p->propertyName("Time_range")->data());
			info.init = QVariant::fromValue(init);
			tmp = init.data();
		}
		else
			continue;

		// set processing inputs
		for (int j = 0; j < tmp->inputCount(); ++j) {
			VipAnyData any;
			any.setData(QVariant::fromValue(v));
			any.setName("Input " + QString::number(j));
			tmp->inputAt(j)->setData(any);
		}

		// create tree item
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, info.classname);
		item->setText(1, info.category);
		item->setToolTip(0, info.description);
		item->setToolTip(1, info.description);
		QFont f = item->font(0);
		f.setBold(true);
		item->setFont(0, f);
		item->setData(0, 1000, QVariant::fromValue(info));
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		d_data->procList->addTopLevelItem(item);
	}

	if (d_data->procList->topLevelItemCount() == 0) {
		// No additional processing: add a dummy item
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, "No registered processing available");
		item->setToolTip(0, "No registered processing available");
		d_data->procList->addTopLevelItem(item);
	}
	else {
		// select the first one
		d_data->procList->topLevelItem(0)->setSelected(true);
	}
}

void VipPySignalFusionProcessingManager::itemDoubleClicked(QTreeWidgetItem* item, int column)
{
	d_data->procList->editItem(item, column);
}

bool VipPySignalFusionProcessingManager::applyChanges()
{
	// remove all registered VipPySignalFusionProcessing
	QList<VipProcessingObject::Info> infos = VipPyRegisterProcessing::customProcessing();
	for (int i = 0; i < infos.size(); ++i)
		VipProcessingObject::removeInfoObject(infos[i]);

	// add new ones
	infos.clear();
	for (int i = 0; i < d_data->procList->topLevelItemCount(); ++i) {
		// get the internal VipProcessingObject::Info object and set its classname and category (might have been modified by the user)
		VipProcessingObject::Info info = d_data->procList->topLevelItem(i)->data(0, 1000).value<VipProcessingObject::Info>();
		info.classname = d_data->procList->topLevelItem(i)->text(0);
		info.category = d_data->procList->topLevelItem(i)->text(1);
		if (info.metatype) {
			VipProcessingObject::registerAdditionalInfoObject(info);
			infos.append(info);
		}
	}

	// save processings
	return VipPyRegisterProcessing::saveCustomProcessings(infos);
}

void VipPySignalFusionProcessingManager::removeSelection()
{
	// remove selected registered processings
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		delete items[i];
	}
}

void VipPySignalFusionProcessingManager::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Delete)
		removeSelection();
}

void VipPySignalFusionProcessingManager::showMenu(const QPoint&)
{
	// QMenu menu;
}

void VipPySignalFusionProcessingManager::descriptionChanged()
{
	// udpate selected processing description
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();
	if (items.size() == 1) {
		VipProcessingObject::Info info = items.first()->data(0, 1000).value<VipProcessingObject::Info>();
		info.description = d_data->procDescription->toPlainText();
		items.first()->setData(0, 1000, QVariant::fromValue(info));
	}
}

void VipPySignalFusionProcessingManager::selectionChanged()
{
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();
	d_data->procDescription->setEnabled(items.size() == 1 && !items.first()->data(0, 1000).isNull());
	d_data->procEditor->setEnabled(items.size() > 0 && !items.first()->data(0, 1000).isNull());
	d_data->pyEditor->setEnabled(items.size() > 0 && !items.first()->data(0, 1000).isNull());
	if (items.isEmpty() || items.first()->data(0, 1000).isNull())
		d_data->procDescription->setPlainText(QString());

	if (items.size() == 1) {
		itemClicked(items.first(), 0);
	}
}

void VipPySignalFusionProcessingManager::itemClicked(QTreeWidgetItem* item, int)
{
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();

	d_data->procDescription->setEnabled(items.size() == 1);

	QTreeWidgetItem* selected = nullptr;
	if (item->isSelected())
		selected = item;
	else if (items.size())
		selected = items.last();

	d_data->procEditor->setEnabled(selected);
	d_data->pyEditor->setEnabled(selected);

	if (selected) {
		VipProcessingObject::Info info = selected->data(0, 1000).value<VipProcessingObject::Info>();
		if (info.metatype) {
			// set description
			d_data->procDescription->setPlainText(info.description);

			// set the processing editor
			if (VipPySignalFusionProcessingPtr ptr = info.init.value<VipPySignalFusionProcessingPtr>()) {
				d_data->procEditor->setEnabled(true);
				d_data->pyEditor->hide();
				d_data->procEditor->show();
				d_data->procEditor->setPySignalFusionProcessing(ptr.data());
			}
			else if (VipPyProcessingPtr ptr = info.init.value<VipPyProcessingPtr>()) {
				d_data->pyEditor->setEnabled(true);
				d_data->procEditor->hide();
				d_data->pyEditor->show();
				d_data->pyEditor->setPyProcessing(ptr.data());
			}
			else {
				d_data->pyEditor->hide();
				d_data->procEditor->show();
				d_data->procEditor->setEnabled(true);
				d_data->pyEditor->setEnabled(false);
			}
		}
		else {
			d_data->pyEditor->hide();
			d_data->procEditor->show();
			d_data->procEditor->setEnabled(true);
			d_data->pyEditor->setEnabled(false);
		}
	}
}

#include <qtoolbar.h>
#include <qboxlayout.h>

class VipPySignalFusionProcessingEditor::PrivateData
{
public:
	PrivateData()
	  : editor(Qt::Vertical)
	  , popupDepth(0)
	{
	}
	QPointer<VipPlotPlayer> player;
	QPointer<VipPySignalFusionProcessing> proc;

	QComboBox resampling;

	QToolButton names;

	QLineEdit title;
	QLineEdit yunit;
	QLineEdit xunit;

	VipTabEditor editor;

	VipPyApplyToolBar* buttons;

	int popupDepth;
};

static QString names_toolTip = "<b>Name mapping</b><br>This menu specifies the names of each signals x/y components within the Python script.<br>"
			       "Click on a signal name to copy it to the clipboard.";

VipPySignalFusionProcessingEditor::VipPySignalFusionProcessingEditor(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QGridLayout* hlay = new QGridLayout();
	hlay->addWidget(new QLabel("Resampling method"), 0, 0);
	hlay->addWidget(&d_data->resampling, 0, 1);
	hlay->addWidget(new QLabel("Output signal name"), 1, 0);
	hlay->addWidget(&d_data->title, 1, 1);
	hlay->addWidget(new QLabel("Output signal unit"), 2, 0);
	hlay->addWidget(&d_data->yunit, 2, 1);
	hlay->addWidget(new QLabel("Output signal X unit"), 3, 0);
	hlay->addWidget(&d_data->xunit, 3, 1);
	d_data->title.setPlaceholderText("Output signal name (mandatory)");
	d_data->title.setToolTip("<b>Enter the output signal name (mandatory)</b><br>"
				 "The signal name could be either a string (like 'My_signal_name') or<br>"
				 "a formula using the input signal titles (like 't0 * t1'). In this case,<br>"
				 "t0 and t1 will be expanded to the input signal names.<br><br>"
				 "It is also possible to use a Python variable in the expression. $MyVariable will be expanded to MyVariable value.");
	d_data->yunit.setPlaceholderText("Output signal unit (optional)");
	d_data->yunit.setToolTip("<b>Optional signal unit.</b><br>By default, the output unit name will be the same as the first input signal unit.<br><br>"
				 "The signal unit could be either a string (like 'My_signal_unit') or<br>"
				 "a formula using the input signal units (like 'u0.u1'). In this case,<br>"
				 "u0 and u1 will be expanded to the input signal units.<br><br>"
				 "It is also possible to use a Python variable in the expression. $MyVariable will be expanded to MyVariable value.");
	d_data->xunit.setPlaceholderText("Output signal X unit (optional)");
	d_data->xunit.setToolTip("<b>Optional signal X unit.</b><br>By default, the output X unit name will be the same as the first input signal unit.<br><br>"
				 "The signal unit could be either a string (like 'My_signal_unit') or<br>"
				 "a formula using the input signal units (like 'u0.u1'). In this case,<br>"
				 "u0 and u1 will be expanded to the input signal units.<br><br>"
				 "It is also possible to use a Python variable in the expression. $MyVariable will be expanded to MyVariable value.");
	d_data->resampling.addItems(QStringList() << "union"
						  << "intersection");
	d_data->resampling.setToolTip("Input signals will be resampled based on given method (union or intersection of input time ranges)");

	d_data->editor.setDefaultColorSchemeType("Python");
	d_data->editor.setUniqueFile(true);
	d_data->editor.tabBar()->actions().first()->setVisible(false);
	d_data->editor.tabBar()->actions()[1]->setVisible(false);
	d_data->editor.tabBar()->actions()[2]->setVisible(false);

	d_data->names.setToolTip(names_toolTip);
	d_data->names.setMenu(new QMenu());
	d_data->names.setPopupMode(QToolButton::InstantPopup);
	d_data->names.setText("Input signals names");

	d_data->editor.setToolTip("Python script for the y and x (time) components (mandatory)");
	d_data->editor.currentEditor()->setPlaceholderText("Example: y = y0 + y1");

	d_data->buttons = new VipPyApplyToolBar();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->names);
	lay->addLayout(hlay);
	lay->addWidget(&d_data->editor);
	lay->addWidget(d_data->buttons);
	setLayout(lay);

	connect(d_data->names.menu(), SIGNAL(triggered(QAction*)), this, SLOT(nameTriggered(QAction*)));
	connect(d_data->buttons->applyButton(), SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(d_data->buttons->registerButton(), SIGNAL(clicked(bool)), this, SLOT(registerProcessing()));
	connect(d_data->buttons->manageButton(), SIGNAL(clicked(bool)), this, SLOT(manageProcessing()));
	connect(&d_data->resampling, SIGNAL(currentIndexChanged(int)), this, SLOT(updateProcessing()));

	this->setMinimumWidth(450);
	this->setMinimumHeight(350);
}

VipPySignalFusionProcessingEditor::~VipPySignalFusionProcessingEditor() {}

void VipPySignalFusionProcessingEditor::nameTriggered(QAction* a)
{
	// copy selected entry in the menu to clipboard
	if (a) {
		qApp->clipboard()->setText(a->property("name").toString());
	}
}

void VipPySignalFusionProcessingEditor::registerProcessing()
{
	if (!d_data->proc)
		return;

	// register the current processing
	VipPySignalFusionProcessingManager* m = new VipPySignalFusionProcessingManager();
	m->setManagerVisible(false);
	m->setCreateNewVisible(true);
	VipGenericDialog dialog(m, "Register new processing");
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = d_data->proc->registerThisProcessing(m->category(), m->name(), m->description(), m->overwrite());
		if (!ret)
			vipWarning("Operation failure", "Failed to register this processing.\nPlease make sure you entered a valid name and category.");
		else {
			// make sure to update all processing menu
			vipGetMainWindow()->displayArea()->resetItemSelection();
		}
	}
}

void VipPySignalFusionProcessingEditor::manageProcessing()
{
	vipOpenProcessingManager();
}

void VipPySignalFusionProcessingEditor::setPlotPlayer(VipPlotPlayer* player)
{
	if (d_data->player != player) {
		d_data->player = player;

		QList<VipPlotCurve*> tmp = player->plotWidget2D()->area()->findItems<VipPlotCurve*>(QString(), 1, 1);
		QString yunit;

		// grab curves from the player
		QMultiMap<QString, QString> curves;
		for (int i = 0; i < tmp.size(); ++i) {
			curves.insert(tmp[i]->title().text(), QString());
			if (yunit.isEmpty())
				yunit = tmp[i]->axisUnit(1).text();
		}
		// set the names
		d_data->names.menu()->clear();
		int i = 0;
		QStringList text;
		for (QMultiMap<QString, QString>::const_iterator it = curves.begin(); it != curves.end(); ++it, ++i) {
			QAction* a = d_data->names.menu()->addAction("'" + it.key() + "' as 'x" + QString::number(i) + "', 'y" + QString::number(i));
			text << a->text();
			a->setProperty("name", it.key());
		}
		d_data->names.setToolTip(names_toolTip + "<br><br>" + text.join("<br>"));
	}
}

VipPlotPlayer* VipPySignalFusionProcessingEditor::plotPlayer() const
{
	return d_data->player;
}
VipPyApplyToolBar* VipPySignalFusionProcessingEditor::buttons() const
{
	return d_data->buttons;
}

void VipPySignalFusionProcessingEditor::setPySignalFusionProcessing(VipPySignalFusionProcessing* proc)
{
	if (proc != d_data->proc) {
		d_data->proc = proc;
		updateWidget();
	}
}
VipPySignalFusionProcessing* VipPySignalFusionProcessingEditor::getPySignalFusionProcessing() const
{
	return d_data->proc;
}

static QRegExp xreg[50];
static QRegExp yreg[50];
static QRegExp ureg[50];
static QRegExp uxreg[50];
static QRegExp treg[50];

static void findXYmatch(const QString& algo,
			const QString& title,
			const QString& unit,
			const QString& xunit,
			int count,
			std::set<int>& x,
			std::set<int>& y,
			std::set<int>& t,
			std::set<int>& u,
			std::set<int>& ux,
			std::set<int>& merged)
{
	if (xreg[0].isEmpty()) {
		// initialize regular expressions
		for (int i = 0; i < 50; ++i) {
			xreg[i].setPattern("\\bx" + QString::number(i) + "\\b");
			xreg[i].setPatternSyntax(QRegExp::RegExp);
			yreg[i].setPattern("\\by" + QString::number(i) + "\\b");
			yreg[i].setPatternSyntax(QRegExp::RegExp);
			ureg[i].setPattern("\\bu" + QString::number(i) + "\\b");
			ureg[i].setPatternSyntax(QRegExp::RegExp);
			uxreg[i].setPattern("\\bu" + QString::number(i) + "\\b");
			uxreg[i].setPatternSyntax(QRegExp::RegExp);
			treg[i].setPattern("\\bt" + QString::number(i) + "\\b");
			treg[i].setPatternSyntax(QRegExp::RegExp);
		}
	}

	for (int i = 0; i < count; ++i) {
		int xi = xreg[i].indexIn(algo);
		int yi = yreg[i].indexIn(algo);
		int ti = treg[i].indexIn(title);
		int ui = ureg[i].indexIn(unit);
		int uxi = uxreg[i].indexIn(xunit);
		if (xi >= 0)
			x.insert(i);
		if (yi >= 0)
			y.insert(i);
		if (ti >= 0)
			t.insert(i);
		if (ui >= 0)
			u.insert(i);
		if (uxi >= 0)
			ux.insert(i);
		if (xi >= 0 || yi >= 0 || ti >= 0 || ui >= 0)
			merged.insert(i);
	}
}

bool VipPySignalFusionProcessingEditor::updateProcessing()
{
	QString algo = d_data->editor.currentEditor() ? d_data->editor.currentEditor()->toPlainText() : QString();
	if (d_data->proc) {

		d_data->proc->propertyName("Time_range")->setData(d_data->resampling.currentText());
		d_data->proc->propertyName("output_title")->setData(d_data->title.text());
		d_data->proc->propertyName("output_unit")->setData(d_data->yunit.text());
		d_data->proc->propertyName("output_x_unit")->setData(d_data->xunit.text());
		QString output_title = d_data->title.text();
		QString output_unit = d_data->yunit.text();
		QString output_x_unit = d_data->xunit.text();

		if (d_data->player) {
			// we need to create the processing inputs and remap the input names
			QList<VipPlotCurve*> tmp = d_data->player->plotWidget2D()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
			QMultiMap<QString, VipPlotCurve*> curves;
			for (int i = 0; i < tmp.size(); ++i)
				curves.insert(tmp[i]->title().text(), tmp[i]);
			tmp = curves.values();

			// find each input x/y
			std::set<int> x, y, u, ux, t, merged;
			findXYmatch(algo, output_title, output_unit, output_x_unit, curves.size(), x, y, t, u, ux, merged);
			if (y.size() == 0)
				return false;

			int inputs = 0;
			for (std::set<int>::iterator it = merged.begin(); it != merged.end(); ++it) {
				// we have an input
				d_data->proc->topLevelInputAt(0)->toMultiInput()->resize(inputs + 1);
				// set connection
				if (VipDisplayObject* disp = tmp[*it]->property("VipDisplayObject").value<VipDisplayObject*>()) {
					d_data->proc->inputAt(inputs)->setConnection(disp->inputAt(0)->connection()->source());
				}
				// rename x/y input
				if (x.find(*it) != x.end())
					algo.replace("x" + QString::number(*it), "x" + QString::number(inputs));
				if (y.find(*it) != y.end())
					algo.replace("y" + QString::number(*it), "y" + QString::number(inputs));
				// rename title nd unit
				if (t.find(*it) != t.end())
					output_title.replace("t" + QString::number(*it), "t" + QString::number(inputs));
				if (u.find(*it) != u.end())
					output_unit.replace("u" + QString::number(*it), "u" + QString::number(inputs));
				if (ux.find(*it) != ux.end())
					output_x_unit.replace("u" + QString::number(*it), "u" + QString::number(inputs));
				++inputs;
			}
		}

		d_data->proc->propertyName("y_algo")->setData(algo);
		d_data->proc->propertyName("x_algo")->setData(QString());
		d_data->proc->propertyName("output_title")->setData(output_title);
		d_data->proc->propertyName("output_unit")->setData(output_unit);
		d_data->proc->propertyName("output_x_unit")->setData(output_x_unit);
		return true;
	}
	return false;
}

void VipPySignalFusionProcessingEditor::updateWidget()
{
	// we can only update if there is no player (otherwise too complicated)
	if (!d_data->proc)
		return;
	if (d_data->player)
		return;

	d_data->resampling.blockSignals(true);
	d_data->resampling.setCurrentText(d_data->proc->propertyName("Time_range")->value<QString>());
	d_data->resampling.blockSignals(false);
	d_data->title.setText(d_data->proc->propertyName("output_title")->value<QString>());
	d_data->yunit.setText(d_data->proc->propertyName("output_unit")->value<QString>());
	d_data->xunit.setText(d_data->proc->propertyName("output_x_unit")->value<QString>());

	// grab input names in alphabetical order
	QMultiMap<QString, QString> curves;
	for (int i = 0; i < d_data->proc->inputCount(); ++i) {
		curves.insert(d_data->proc->inputAt(i)->probe().name(), QString());
	}
	// set names
	d_data->names.menu()->clear();
	int i = 0;
	QStringList text;
	for (QMultiMap<QString, QString>::const_iterator it = curves.begin(); it != curves.end(); ++it, ++i) {
		QAction* a = d_data->names.menu()->addAction("'" + it.key() + "' as 'x" + QString::number(i) + "', 'y" + QString::number(i) + "'");
		text << a->text();
		a->setProperty("name", it.key());
	}
	d_data->names.setToolTip(names_toolTip + "<br><br>" + text.join("<br>"));

	// update algo editors
	d_data->editor.currentEditor()->setPlainText(d_data->proc->propertyName("y_algo")->value<QString>() + "\n" + d_data->proc->propertyName("x_algo")->value<QString>());
}

void VipPySignalFusionProcessingEditor::showError(const QPoint& pos, const QString& error)
{
	QToolTip::showText(pos, error, nullptr, QRect(), 5000);
}
void VipPySignalFusionProcessingEditor::showErrorDelayed(const QPoint& pos, const QString& error)
{
	if (d_data->popupDepth < 4) {
		d_data->popupDepth++;
		QMetaObject::invokeMethod(this, "showErrorDelayed", Qt::QueuedConnection, Q_ARG(QPoint, pos), Q_ARG(QString, error));
	}
	else {
		d_data->popupDepth = 0;
		QMetaObject::invokeMethod(this, "showError", Qt::QueuedConnection, Q_ARG(QPoint, pos), Q_ARG(QString, error));
	}
}

bool VipPySignalFusionProcessingEditor::apply()
{
	// check output name
	if (d_data->title.text().isEmpty()) {
		// display a tool tip at the bottom
		QPoint pos = d_data->title.mapToGlobal(QPoint(0, d_data->title.height()));
		showErrorDelayed(pos, "Setting a valid signal name is mandatory!");
		return false;
	}

	// check script
	QString algo = d_data->editor.currentEditor() ? d_data->editor.currentEditor()->toPlainText() : QString();
	QRegExp reg("[\\s]{0,10}y[\\s]{0,10}=");
	int match = reg.indexIn(algo);
	if (match < 0) {
		// display a tool tip at the bottom of y editor
		QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
		showErrorDelayed(pos, "You must specify a valid script for the y component!\nA valid script must set the 'y' variable: 'y = ...'");
		return false;
	}

	if (d_data->proc) {

		if (!updateProcessing()) {
			// display a tool tip at the bottom of y editor
			QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
			showErrorDelayed(pos, "Given script is not valid!\nThe script needs to reference at least 2 input signals (like y0, y1,...)");
			return false;
		}

		VipAnyDataList inputs; // save input data, since setScheduleStrategy will clear them
		for (int i = 0; i < d_data->proc->inputCount(); ++i)
			inputs << d_data->proc->inputAt(i)->probe();

		d_data->proc->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		// set input data
		for (int i = 0; i < d_data->proc->inputCount(); ++i) {
			if (VipOutput* src = d_data->proc->inputAt(i)->connection()->source())
				d_data->proc->inputAt(i)->setData(src->data());
			else
				d_data->proc->inputAt(i)->setData(inputs[i]);
		}
		if (!d_data->proc->update()) {
			// display a tool tip at the bottom of y editor
			QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
			showErrorDelayed(pos, "Given script must use at least 2 different input signals!");
			return false;
		}
		QString err = d_data->proc->error().errorString();
		bool has_error = d_data->proc->hasError();
		// d_data->proc->restore();
		d_data->proc->setScheduleStrategy(VipProcessingObject::Asynchronous, true);
		if (has_error) {
			VipText text("An error occured while applying the processings!\n\n" + d_data->proc->error().errorString());
			// QSize s = text.textSize().toSize();
			QPoint pos = d_data->editor.mapToGlobal(QPoint(0, 0));
			showErrorDelayed(pos, text.text());
			return false;
		}
	}
	return true;
}

// register editor

static VipPySignalFusionProcessingEditor* editPySignalFusionProcessing(VipPySignalFusionProcessing* proc)
{
	VipPySignalFusionProcessingEditor* editor = new VipPySignalFusionProcessingEditor();
	editor->setPySignalFusionProcessing(proc);
	return editor;
}

int registerEditPySignalFusionProcessing()
{
	vipFDObjectEditor().append<QWidget*(VipPySignalFusionProcessing*)>(editPySignalFusionProcessing);
	return 0;
}
static int _registerEditPySignalFusionProcessing = registerEditPySignalFusionProcessing();

void vipOpenProcessingManager()
{
	VipPySignalFusionProcessingManager* m = new VipPySignalFusionProcessingManager();
	m->setManagerVisible(true);
	m->setCreateNewVisible(false);
	m->updateWidget();
	VipGenericDialog dialog(m, "Manage registered processing");
	dialog.setMaximumHeight(800);
	dialog.setMinimumWidth(500);
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = m->applyChanges();
		if (!ret)
			vipWarning("Operation failure", "Failed to modify registered processing.");
	}
}





class VipFitDialogBox::PrivateData
{
public:
	QLabel curvesLabel;
	QComboBox curves;
	QLabel fitLabel;
	QComboBox fit;

	QPushButton ok, cancel;

	VipPlotPlayer* player;
};

VipFitDialogBox::VipFitDialogBox(VipPlotPlayer* pl, const QString& fit, QWidget* parent)
  : QDialog(parent)
{
	// retrieve all visible and selected curves
	QList<VipPlotCurve*> curves = pl->viewer()->area()->findItems<VipPlotCurve*>(QString(), 1, 1);

	VIP_CREATE_PRIVATE_DATA();
	d_data->player = pl;

	QGridLayout* lay = new QGridLayout();
	lay->addWidget(&d_data->curvesLabel, 0, 0);
	lay->addWidget(&d_data->curves, 0, 1);
	lay->addWidget(&d_data->fitLabel, 1, 0);
	lay->addWidget(&d_data->fit, 1, 1);

	d_data->curvesLabel.setText(tr("Select curve to fit:"));
	d_data->fitLabel.setText(tr("Select the fit type:"));

	for (int i = 0; i < curves.size(); ++i)
		d_data->curves.addItem(curves[i]->title().text());

	d_data->ok.setText(tr("Ok"));
	d_data->cancel.setText(tr("Cancel"));

	d_data->fit.addItem("Linear");
	d_data->fit.addItem("Exponential");
	d_data->fit.addItem("Polynomial");
	d_data->fit.addItem("Gaussian");
	d_data->fit.setCurrentText(fit);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addStretch(1);
	hlay->addWidget(&d_data->ok);
	hlay->addWidget(&d_data->cancel);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(lay);
	vlay->addWidget(VipLineWidget::createSunkenHLine());
	vlay->addLayout(hlay);
	setLayout(vlay);

	connect(&d_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(&d_data->cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));

	this->setWindowTitle("Fit plot");
}

VipFitDialogBox::~VipFitDialogBox() {}

VipPlotCurve* VipFitDialogBox::selectedCurve() const
{
	QList<VipPlotCurve*> curves = d_data->player->viewer()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
	for (int i = 0; i < curves.size(); ++i)
		if (curves[i]->title().text() == d_data->curves.currentText())
			return curves[i];
	return nullptr;
}

int VipFitDialogBox::selectedFit() const
{
	return d_data->fit.currentIndex();
}

VipPyFitProcessing* vipFitCurve(VipPlotCurve* curve, VipPlotPlayer* player, int fit_type)
{
	VipProcessingPool* pool = player->processingPool();

	VipOutput* src = nullptr;
	if (VipDisplayObject* disp = curve->property("VipDisplayObject").value<VipDisplayObject*>())
		src = disp->inputAt(0)->connection()->source();

	VipPyFitProcessing* fit = nullptr;
	if (fit_type == 0)
		fit = new VipPyFitLinear();
	else if (fit_type == 1)
		fit = new VipPyFitExponential();
	else if (fit_type == 2)
		fit = new VipPyFitPolynomial();
	else
		fit = new VipPyFitGaussian();
	fit->setParent(pool);

	fit->inputAt(0)->setData(curve->rawData());
	if (src)
		fit->inputAt(0)->setConnection(src);
	fit->update();
	fit->setScheduleStrategy(VipProcessingObject::Asynchronous);
	fit->setDeleteOnOutputConnectionsClosed(true);
	//fit->setPlotPlayer(player);
	new detail::AttachFitToPlayer(fit, player);

	/*if (src)
	{
		fit->inputAt(0)->setConnection(src);
	}*/

	VipDisplayCurve* disp = static_cast<VipDisplayCurve*>(vipCreateDisplayFromData(fit->outputAt(0)->data(), player));
	disp->setParent(pool);
	disp->inputAt(0)->setConnection(fit->outputAt(0));

	QPen pen = curve->boxStyle().borderPen();
	pen.setStyle(Qt::DotLine);
	pen.setWidth(2);
	disp->item()->boxStyle().setBorderPen(pen);

	QString name = "Fit " + VipPyFitProcessing::fitName((VipPyFitProcessing::Type)fit_type) + " " + curve->title().text();
	disp->item()->setTitle(name);
	fit->setAttribute("Name", name);

	VipText text(QString("<b>Fit</b>: #pequation"));
	QColor c = curve->boxStyle().borderPen().color();
	c.setAlpha(120);
	text.setBackgroundBrush(c);
	text.setTextPen(QPen(vipWidgetTextBrush(player).color()));
	disp->item()->addText(text);

	vipCreatePlayersFromProcessing(disp, player, nullptr, curve);
	
	// Apply the pen as a style sheet for session saving/loading
	disp->item()->styleSheet().setProperty("VipPlotItem", "border", QVariant::fromValue(pen));
	disp->item()->updateStyleSheetString();

	return fit;
}

VipPyFitProcessing* vipFitCurve(VipPlotPlayer* player, const QString& fit)
{
	if (!player)
		return nullptr;
	VipFitDialogBox dial(player, fit);
	if (dial.exec() == QDialog::Accepted) {
		return vipFitCurve(dial.selectedCurve(), player, dial.selectedFit());
	}
	return nullptr;
}

namespace detail
{

	AttachFitToPlayer::AttachFitToPlayer(VipPyFitProcessing* fit, VipPlotPlayer* pl) 
		: VipFitManage(fit)
	  , m_player(pl)
	{
		// remove previous AttachFitToPlayer
		QList<AttachFitToPlayer*> lst = fit->findChildren<AttachFitToPlayer*>();
		for (int i = 0; i < lst.size(); ++i)
			if (lst[i] != this)
				delete lst[i];

		if (pl) {
			connect(pl, SIGNAL(timeUnitChanged(const QString&)), this, SLOT(timeUnitChanged()));
			connect(pl->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), fit, SLOT(reload()));
			connect(pl->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), fit, SLOT(reload()));
			timeUnitChanged();
		}
		
	}

	VipInterval AttachFitToPlayer::xBounds() const
	{
		if (VipPlotPlayer* pl = player()) {
			VipInterval bounds = pl->defaultXAxis()->scaleDiv().bounds();
			if (pl->displayVerticalWindow()) {
				QRectF r = pl->verticalWindow()->rawData().polygon().boundingRect();
				VipInterval inter(r.left(), r.right());
				VipInterval intersect = inter.intersect(bounds);
				if (intersect.isValid())
					bounds = intersect;
			}
			return bounds;
		}
		return VipInterval();
	}

	VipPlotPlayer* AttachFitToPlayer::player() const
	{
		VipPyFitProcessing* fit = this->parent();
		if (!fit)
			return nullptr;
		if (m_player)
			return m_player;
		QList<VipDisplayObject*> displays = vipListCast<VipDisplayObject*>(fit->allSinks());
		for (int i = 0; i < displays.size(); ++i)
			if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(displays[i]->widget())) {
				connect(pl, SIGNAL(timeUnitChanged(const QString&)), this, SLOT(timeUnitChanged()));
				connect(pl->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), fit, SLOT(reload()));
				connect(pl->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), fit, SLOT(reload()));
				return const_cast<AttachFitToPlayer*>(this)->m_player = pl;
			}
		return m_player;
	}

	void AttachFitToPlayer::timeUnitChanged()
	{
		if (VipPlotPlayer * pl = player())
			parent()->setTimeUnit(pl->timeUnit());
	}

	static void attachFitToPlayer(VipDisplayCurve* d, VipPlotCurve* c)
	{
		if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(VipPlotPlayer::findAbstractPlayer(d))) {

			QList<VipPyFitProcessing*> fits = vipListCast<VipPyFitProcessing*>(d->allSources());
			for (VipPyFitProcessing* f : fits) {
				new AttachFitToPlayer(f, pl);
			}
		}
	}

	static int registerAttachFit()
	{
		// Make sure attachFitToPlayer is called every time a VipPlotCurve is added to a VipPlotPlayer.
		// This is the only way to ensure that a VipPyFitProcessing has an associated AttachFitToPlayer
		// on session loading.
		VipFDDisplayObjectSetItem().append<void(VipDisplayCurve*, VipPlotCurve*)>(attachFitToPlayer);
		return 0;
	}
	static int _registerAttachFit = registerAttachFit();
}
