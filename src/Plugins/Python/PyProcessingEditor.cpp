#include "PyProcessingEditor.h"
#include "PyRegisterProcessing.h"
#include "VipTabEditor.h"
#include "IOOperationWidget.h"
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

#include "CurveFit.h"

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

class PyArrayEditor::PrivateData
{
public:
	VipNDArray array;
	QLabel info;
	QToolButton send;
	QPlainTextEdit editor;
};

PyArrayEditor::PyArrayEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->info.setText("Enter your 2D array. Each column is separated by spaces or tabulations, each row is separated by a new line.");
	d_data->info.setWordWrap(true);
	d_data->send.setAutoRaise(true);
	d_data->send.setToolTip("Click to finish your 2D array");
	d_data->send.setIcon(vipIcon("apply.png"));
	d_data->editor.setMinimumHeight(200);

	QGridLayout* lay = new QGridLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&d_data->info, 0, 0);
	lay->addWidget(&d_data->send, 0, 1);
	lay->addWidget(&d_data->editor, 1, 0, 1, 2);
	setLayout(lay);

	connect(&d_data->send, SIGNAL(clicked(bool)), this, SLOT(finished()));
	connect(&d_data->editor, SIGNAL(textChanged()), this, SLOT(textEntered()));
}

PyArrayEditor::~PyArrayEditor() {}

VipNDArray PyArrayEditor::array() const
{
	return d_data->array;
}

void PyArrayEditor::setText(const QString& text)
{
	// remove any numpy formatting
	QString out = text;
	out.remove("(");
	out.remove(")");
	out.remove("[");
	out.remove("]");
	out.remove(",");
	out.remove("array");
	d_data->editor.setPlainText(out);
	QTextStream str(out.toLatin1());
	str >> d_data->array;
}
void PyArrayEditor::setArray(const VipNDArray& ar)
{
	QString out;
	QTextStream str(&out, QIODevice::WriteOnly);
	str << ar;
	str.flush();
	d_data->editor.setPlainText(out);
	d_data->array = ar;
}

void PyArrayEditor::textEntered()
{
	d_data->array = VipNDArray();

	QString text = d_data->editor.toPlainText();
	text.replace("\t", " ");
	QStringList lines = text.split("\n");
	if (lines.size()) {
		int columns = lines.first().split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts).size();
		bool ok = true;
		for (int i = 1; i < lines.size(); ++i) {
			if (lines[i].split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts).size() != columns) {
				if (lines[i].count('\n') + lines[i].count('\t') + lines[i].count(' ') != lines[i].size()) {
					ok = false;
					break;
				}
			}
		}

		if (ok) {
			QTextStream str(text.toLatin1());
			str >> d_data->array;
			if (!d_data->array.isEmpty()) {
				d_data->send.setIcon(vipIcon("apply_green.png"));
				return;
			}
		}
	}

	d_data->send.setIcon(vipIcon("apply_red.png"));
}

void PyArrayEditor::finished()
{
	if (!d_data->array.isEmpty())
		Q_EMIT changed();
}

class PyDataEditor::PrivateData
{
public:
	VipOtherPlayerData data;
	QRadioButton* editArray;
	QRadioButton* editPlayer;

	QCheckBox* shouldResizeArray;

	PyArrayEditor* editor;
	VipOtherPlayerDataEditor* player;

	QWidget* lineBefore;
	QWidget* lineAfter;
};

PyDataEditor::PyDataEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->editArray = new QRadioButton("Create a 1D/2D array");
	d_data->editArray->setToolTip("<b>Manually create a 1D/2D array</b><br>This is especially usefull for convolution functions.");
	d_data->editPlayer = new QRadioButton("Take the data from another player");
	d_data->editPlayer->setToolTip("<b>Selecte a data (image, curve,...) from another player</b>");
	d_data->shouldResizeArray = new QCheckBox("Resize array to the current data shape");
	d_data->shouldResizeArray->setToolTip("This usefull if you apply a processing/filter that only works on 2 arrays having the same shape.\n"
					      "Selecting this option ensures you that given image/curve will be resized to the right dimension.");
	d_data->editor = new PyArrayEditor();
	d_data->player = new VipOtherPlayerDataEditor();
	d_data->lineBefore = VipLineWidget::createHLine();
	d_data->lineAfter = VipLineWidget::createHLine();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->lineBefore);
	lay->addWidget(d_data->editArray);
	lay->addWidget(d_data->editPlayer);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(d_data->shouldResizeArray);
	lay->addWidget(d_data->editor);
	lay->addWidget(d_data->player);
	lay->addWidget(d_data->lineAfter);
	// lay->setSizeConstraint(QLayout::SetFixedSize);
	setLayout(lay);

	d_data->editArray->setChecked(true);
	d_data->player->setVisible(false);

	connect(d_data->editArray, SIGNAL(clicked(bool)), d_data->editor, SLOT(setVisible(bool)));
	connect(d_data->editArray, SIGNAL(clicked(bool)), d_data->player, SLOT(setHidden(bool)));
	connect(d_data->editPlayer, SIGNAL(clicked(bool)), d_data->player, SLOT(setVisible(bool)));
	connect(d_data->editPlayer, SIGNAL(clicked(bool)), d_data->editor, SLOT(setHidden(bool)));

	connect(d_data->shouldResizeArray, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(d_data->editor, SIGNAL(changed()), this, SLOT(emitChanged()));
	connect(d_data->player, SIGNAL(valueChanged(const QVariant&)), this, SLOT(emitChanged()));
}

PyDataEditor::~PyDataEditor() {}

VipOtherPlayerData PyDataEditor::value() const
{
	VipOtherPlayerData res;
	if (d_data->editArray->isChecked())
		res = VipOtherPlayerData(VipAnyData(QVariant::fromValue(d_data->editor->array()), 0));
	else
		res = d_data->player->value();
	res.setShouldResizeArray(d_data->shouldResizeArray->isChecked());
	return res;
}

void PyDataEditor::setValue(const VipOtherPlayerData& data)
{
	VipNDArray ar = data.staticData().value<VipNDArray>();

	d_data->editor->blockSignals(true);
	d_data->player->blockSignals(true);
	d_data->shouldResizeArray->blockSignals(true);

	if (!ar.isEmpty() && data.otherPlayerId() < 1) {
		d_data->editArray->setChecked(true);
		d_data->editor->setArray(ar);
	}
	else {
		d_data->editPlayer->setChecked(true);
		d_data->player->setValue(data);
	}
	d_data->player->setVisible(d_data->editPlayer->isChecked());
	d_data->editor->setVisible(!d_data->editPlayer->isChecked());

	d_data->shouldResizeArray->setChecked(data.shouldResizeArray());

	d_data->editor->blockSignals(false);
	d_data->player->blockSignals(false);
	d_data->shouldResizeArray->blockSignals(false);
}

void PyDataEditor::displayVLines(bool before, bool after)
{
	d_data->lineBefore->setVisible(before);
	d_data->lineAfter->setVisible(after);
}

class PyParametersEditor::PrivateData
{
public:
	QList<QWidget*> editors;
	QList<PyProcessing::Parameter> params;
	QList<QVariant> previous;
	QPointer<PyProcessing> processing;
};

PyParametersEditor::PyParametersEditor(PyProcessing* p)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->processing = p;
	d_data->params = p->extractStdProcessingParameters();
	QVariantMap args = p->stdProcessingParameters();

	if (d_data->params.size()) {
		QGridLayout* lay = new QGridLayout();
		lay->setContentsMargins(0, 0, 0, 0);
		// lay->setSizeConstraint(QLayout::SetMinimumSize);

		for (int i = 0; i < d_data->params.size(); ++i) {
			PyProcessing::Parameter pr = d_data->params[i];
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
				PyDataEditor* ed = new PyDataEditor();
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
PyParametersEditor::~PyParametersEditor() {}

void PyParametersEditor::updateProcessing()
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
		else if (PyDataEditor* other = qobject_cast<PyDataEditor*>(ed))
			value = QVariant::fromValue(other->value());
		map[name] = value;
	}

	d_data->processing->setStdProcessingParameters(map);
	d_data->processing->reload();

	Q_EMIT changed();
}

class PyProcessingEditor::PrivateData
{
public:
	VipTabEditor editor;
	QPointer<PyProcessing> proc;
	QLabel maxTime;
	QSpinBox maxTimeEdit;
	QLabel resampleText;
	QComboBox resampleBox;
	PyApplyToolBar apply;
	PyParametersEditor* params;
};

PyProcessingEditor::PyProcessingEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
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

PyProcessingEditor::~PyProcessingEditor() {}

PyApplyToolBar* PyProcessingEditor::buttons() const
{
	return &d_data->apply;
}

void PyProcessingEditor::updatePyProcessing()
{
	if (d_data->proc) {
		d_data->proc->setMaxExecutionTime(d_data->maxTimeEdit.value());
		if (d_data->resampleBox.currentText() != d_data->proc->propertyAt(0)->value<QString>()) {
			d_data->proc->propertyAt(0)->setData(d_data->resampleBox.currentText());
			d_data->proc->reload();
		}
	}
}

void PyProcessingEditor::setPyProcessing(PyProcessing* proc)
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

		QList<PyProcessing::Parameter> params = proc->extractStdProcessingParameters();
		if (params.size()) {
			d_data->params = new PyParametersEditor(proc);
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

void PyProcessingEditor::applyRequested()
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

void PyProcessingEditor::uninitRequested()
{
	if (d_data->proc) {
		d_data->proc->propertyAt(1)->setData(VipAnyData(QString(d_data->editor.currentEditor()->toPlainText()), VipInvalidTime));
		d_data->proc->reload();
		d_data->proc->wait();
		// d_data->editor.SetUninit();
	}
}

void PyProcessingEditor::registerProcessing()
{
	if (!d_data->proc)
		return;

	// register the current processing
	PySignalFusionProcessingManager* m = new PySignalFusionProcessingManager();
	m->setManagerVisible(false);
	m->setCreateNewVisible(true);
	m->setCategory("Python/");
	VipGenericDialog dialog(m, "Register new processing");
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = d_data->proc->registerThisProcessing(m->category(), m->name(), m->description(), false);
		if (!ret)
			QMessageBox::warning(nullptr, "Operation failure", "Failed to register this processing.\nPlease make sure you entered a valid name and category.");
		else {
			// make sure to update all processing menu
			vipGetMainWindow()->displayArea()->resetItemSelection();
		}
	}
}

void PyProcessingEditor::manageProcessing()
{
	openProcessingManager();
}

static PyProcessingEditor* editPyProcessing(PyProcessing* proc)
{
	PyProcessingEditor* editor = new PyProcessingEditor();
	editor->setPyProcessing(proc);
	return editor;
}

int registerEditPyProcessing()
{
	vipFDObjectEditor().append<QWidget*(PyProcessing*)>(editPyProcessing);
	return 0;
}
static int _registerEditPyProcessing = registerEditPyProcessing();

class PyApplyToolBar::PrivateData
{
public:
	QPushButton* apply;
	QToolButton* save;
	QToolButton* manage;
};
PyApplyToolBar::PyApplyToolBar(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
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
PyApplyToolBar::~PyApplyToolBar() {}

QPushButton* PyApplyToolBar::applyButton() const
{
	return d_data->apply;
}
QToolButton* PyApplyToolBar::registerButton() const
{
	return d_data->save;
}
QToolButton* PyApplyToolBar::manageButton() const
{
	return d_data->manage;
}

class PySignalFusionProcessingManager::PrivateData
{
public:
	QGroupBox* createWidget;
	QLineEdit* name;
	QLineEdit* category;
	QPlainTextEdit* description;

	QGroupBox* editWidget;
	QTreeWidget* procList;
	QPlainTextEdit* procDescription;
	PySignalFusionProcessingEditor* procEditor;
	PyProcessingEditor* pyEditor;
};

PySignalFusionProcessingManager::PySignalFusionProcessingManager(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->createWidget = new QGroupBox("Register new processing");
	d_data->name = new QLineEdit();
	d_data->name->setToolTip("Enter the processing name (mandatory)");
	d_data->name->setPlaceholderText("Processing name");
	d_data->category = new QLineEdit();
	d_data->category->setToolTip("<b>Enter the processing category (mandatory)</b><br>You can define as many sub-categories as you need using a '/' separator.");
	d_data->category->setPlaceholderText("Processing category");
	d_data->category->setText("Data Fusion/");
	d_data->description = new QPlainTextEdit();
	d_data->description->setPlaceholderText("Processing short description (optional)");
	d_data->description->setToolTip("Processing short description (optional)");
	d_data->description->setMinimumHeight(100);

	QGridLayout* glay = new QGridLayout();
	glay->addWidget(new QLabel("Processing name: "), 0, 0);
	glay->addWidget(d_data->name, 0, 1);
	glay->addWidget(new QLabel("Processing category: "), 1, 0);
	glay->addWidget(d_data->category, 1, 1);
	glay->addWidget(d_data->description, 2, 0, 1, 2);
	d_data->createWidget->setLayout(glay);

	d_data->editWidget = new QGroupBox("Edit registered processing");
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

	d_data->procEditor = new PySignalFusionProcessingEditor();
	d_data->procEditor->buttons()->manageButton()->hide();
	d_data->procEditor->buttons()->registerButton()->hide();
	d_data->pyEditor = new PyProcessingEditor();
	d_data->pyEditor->buttons()->manageButton()->hide();
	d_data->pyEditor->buttons()->registerButton()->hide();
	d_data->pyEditor->setMaximumHeight(400);
	d_data->pyEditor->setMinimumHeight(200);
	QVBoxLayout* vlay = new QVBoxLayout();
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

PySignalFusionProcessingManager::~PySignalFusionProcessingManager() {}

QString PySignalFusionProcessingManager::name() const
{
	return d_data->name->text();
}
QString PySignalFusionProcessingManager::category() const
{
	return d_data->category->text();
}
QString PySignalFusionProcessingManager::description() const
{
	return d_data->description->toPlainText();
}

void PySignalFusionProcessingManager::setName(const QString& name)
{
	d_data->name->setText(name);
}
void PySignalFusionProcessingManager::setCategory(const QString& cat)
{
	d_data->category->setText(cat);
}
void PySignalFusionProcessingManager::setDescription(const QString& desc)
{
	d_data->description->setPlainText(desc);
}

void PySignalFusionProcessingManager::setManagerVisible(bool vis)
{
	d_data->editWidget->setVisible(vis);
}
bool PySignalFusionProcessingManager::managerVisible() const
{
	return d_data->editWidget->isVisible();
}

void PySignalFusionProcessingManager::setCreateNewVisible(bool vis)
{
	d_data->createWidget->setVisible(vis);
}
bool PySignalFusionProcessingManager::createNewVisible() const
{
	return d_data->createWidget->isVisible();
}

void PySignalFusionProcessingManager::updateWidget()
{
	// get all processing infos and copy them to d_data->processings
	QList<VipProcessingObject::Info> infos = PyRegisterProcessing::customProcessing();

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
		if (PySignalFusionProcessingPtr p = info.init.value<PySignalFusionProcessingPtr>()) {
			PySignalFusionProcessingPtr init(new PySignalFusionProcessing());
			init->topLevelInputAt(0)->toMultiInput()->resize(p->topLevelInputAt(0)->toMultiInput()->count());
			init->propertyName("x_algo")->setData(p->propertyName("x_algo")->data());
			init->propertyName("y_algo")->setData(p->propertyName("y_algo")->data());
			init->propertyName("output_title")->setData(p->propertyName("output_title")->data());
			init->propertyName("output_unit")->setData(p->propertyName("output_unit")->data());
			init->propertyName("Time_range")->setData(p->propertyName("Time_range")->data());
			info.init = QVariant::fromValue(init);
			tmp = init.data();
		}
		else if (PyProcessingPtr p = info.init.value<PyProcessingPtr>()) {
			PyProcessingPtr init(new PyProcessing());
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

void PySignalFusionProcessingManager::itemDoubleClicked(QTreeWidgetItem* item, int column)
{
	d_data->procList->editItem(item, column);
}

bool PySignalFusionProcessingManager::applyChanges()
{
	// remove all registered PySignalFusionProcessing
	QList<VipProcessingObject::Info> infos = PyRegisterProcessing::customProcessing();
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
	return PyRegisterProcessing::saveCustomProcessings(infos);
}

void PySignalFusionProcessingManager::removeSelection()
{
	// remove selected registered processings
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		delete items[i];
	}
}

void PySignalFusionProcessingManager::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Delete)
		removeSelection();
}

void PySignalFusionProcessingManager::showMenu(const QPoint&)
{
	// QMenu menu;
}

void PySignalFusionProcessingManager::descriptionChanged()
{
	// udpate selected processing description
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();
	if (items.size() == 1) {
		VipProcessingObject::Info info = items.first()->data(0, 1000).value<VipProcessingObject::Info>();
		info.description = d_data->procDescription->toPlainText();
		items.first()->setData(0, 1000, QVariant::fromValue(info));
	}
}

void PySignalFusionProcessingManager::selectionChanged()
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

void PySignalFusionProcessingManager::itemClicked(QTreeWidgetItem* item, int)
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
			if (PySignalFusionProcessingPtr ptr = info.init.value<PySignalFusionProcessingPtr>()) {
				d_data->procEditor->setEnabled(true);
				d_data->pyEditor->hide();
				d_data->procEditor->show();
				d_data->procEditor->setPySignalFusionProcessing(ptr.data());
			}
			else if (PyProcessingPtr ptr = info.init.value<PyProcessingPtr>()) {
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

class PySignalFusionProcessingEditor::PrivateData
{
public:
	PrivateData()
	  : editor(Qt::Vertical)
	  , popupDepth(0)
	{
	}
	QPointer<VipPlotPlayer> player;
	QPointer<PySignalFusionProcessing> proc;

	QComboBox resampling;

	QToolButton names;

	QLineEdit title;
	QLineEdit yunit;
	QLineEdit xunit;

	VipTabEditor editor;

	PyApplyToolBar* buttons;

	int popupDepth;
};

static QString names_toolTip = "<b>Name mapping</b><br>This menu specifies the names of each signals x/y components within the Python script.<br>"
			       "Click on a signal name to copy it to the clipboard.";

PySignalFusionProcessingEditor::PySignalFusionProcessingEditor(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

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

	d_data->buttons = new PyApplyToolBar();

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

PySignalFusionProcessingEditor::~PySignalFusionProcessingEditor() {}

void PySignalFusionProcessingEditor::nameTriggered(QAction* a)
{
	// copy selected entry in the menu to clipboard
	if (a) {
		qApp->clipboard()->setText(a->property("name").toString());
	}
}

void PySignalFusionProcessingEditor::registerProcessing()
{
	if (!d_data->proc)
		return;

	// register the current processing
	PySignalFusionProcessingManager* m = new PySignalFusionProcessingManager();
	m->setManagerVisible(false);
	m->setCreateNewVisible(true);
	VipGenericDialog dialog(m, "Register new processing");
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = d_data->proc->registerThisProcessing(m->category(), m->name(), m->description(), false);
		if (!ret)
			QMessageBox::warning(nullptr, "Operation failure", "Failed to register this processing.\nPlease make sure you entered a valid name and category.");
		else {
			// make sure to update all processing menu
			vipGetMainWindow()->displayArea()->resetItemSelection();
		}
	}
}

void PySignalFusionProcessingEditor::manageProcessing()
{
	openProcessingManager();
}

void PySignalFusionProcessingEditor::setPlotPlayer(VipPlotPlayer* player)
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

VipPlotPlayer* PySignalFusionProcessingEditor::plotPlayer() const
{
	return d_data->player;
}
PyApplyToolBar* PySignalFusionProcessingEditor::buttons() const
{
	return d_data->buttons;
}

void PySignalFusionProcessingEditor::setPySignalFusionProcessing(PySignalFusionProcessing* proc)
{
	if (proc != d_data->proc) {
		d_data->proc = proc;
		updateWidget();
	}
}
PySignalFusionProcessing* PySignalFusionProcessingEditor::getPySignalFusionProcessing() const
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

bool PySignalFusionProcessingEditor::updateProcessing()
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

void PySignalFusionProcessingEditor::updateWidget()
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

void PySignalFusionProcessingEditor::showError(const QPoint& pos, const QString& error)
{
	QToolTip::showText(pos, error, nullptr, QRect(), 5000);
}
void PySignalFusionProcessingEditor::showErrorDelayed(const QPoint& pos, const QString& error)
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

bool PySignalFusionProcessingEditor::apply()
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

static PySignalFusionProcessingEditor* editPySignalFusionProcessing(PySignalFusionProcessing* proc)
{
	PySignalFusionProcessingEditor* editor = new PySignalFusionProcessingEditor();
	editor->setPySignalFusionProcessing(proc);
	return editor;
}

int registerEditPySignalFusionProcessing()
{
	vipFDObjectEditor().append<QWidget*(PySignalFusionProcessing*)>(editPySignalFusionProcessing);
	return 0;
}
static int _registerEditPySignalFusionProcessing = registerEditPySignalFusionProcessing();

void openProcessingManager()
{
	PySignalFusionProcessingManager* m = new PySignalFusionProcessingManager();
	m->setManagerVisible(true);
	m->setCreateNewVisible(false);
	m->updateWidget();
	VipGenericDialog dialog(m, "Manage registered processing");
	dialog.setMaximumHeight(800);
	dialog.setMinimumWidth(500);
	if (dialog.exec() == QDialog::Accepted) {
		bool ret = m->applyChanges();
		if (!ret)
			QMessageBox::warning(nullptr, "Operation failure", "Failed to modify registered processing.");
	}
}





class FitDialogBox::PrivateData
{
public:
	QLabel curvesLabel;
	QComboBox curves;
	QLabel fitLabel;
	QComboBox fit;

	QPushButton ok, cancel;

	VipPlotPlayer* player;
};

FitDialogBox::FitDialogBox(VipPlotPlayer* pl, const QString& fit, QWidget* parent)
  : QDialog(parent)
{
	// retrieve all visible and selected curves
	QList<VipPlotCurve*> curves = pl->viewer()->area()->findItems<VipPlotCurve*>(QString(), 1, 1);

	VIP_CREATE_PRIVATE_DATA(d_data);
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

FitDialogBox::~FitDialogBox() {}

VipPlotCurve* FitDialogBox::selectedCurve() const
{
	QList<VipPlotCurve*> curves = d_data->player->viewer()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
	for (int i = 0; i < curves.size(); ++i)
		if (curves[i]->title().text() == d_data->curves.currentText())
			return curves[i];
	return nullptr;
}

int FitDialogBox::selectedFit() const
{
	return d_data->fit.currentIndex();
}

FitProcessing* fitCurve(VipPlotCurve* curve, VipPlotPlayer* player, int fit_type)
{
	VipProcessingPool* pool = player->processingPool();

	VipOutput* src = nullptr;
	if (VipDisplayObject* disp = curve->property("VipDisplayObject").value<VipDisplayObject*>())
		src = disp->inputAt(0)->connection()->source();

	FitProcessing* fit = nullptr;
	if (fit_type == 0)
		fit = new FitLinear();
	else if (fit_type == 1)
		fit = new FitExponential();
	else if (fit_type == 2)
		fit = new FitPolynomial();
	else
		fit = new FitGaussian();
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

	QString name = "Fit " + FitProcessing::fitName((FitProcessing::Type)fit_type) + " " + curve->title().text();
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

FitProcessing* fitCurve(VipPlotPlayer* player, const QString& fit)
{
	if (!player)
		return nullptr;
	FitDialogBox dial(player, fit);
	if (dial.exec() == QDialog::Accepted) {
		return fitCurve(dial.selectedCurve(), player, dial.selectedFit());
	}
	return nullptr;
}

namespace detail
{

	AttachFitToPlayer::AttachFitToPlayer(FitProcessing* fit, VipPlotPlayer* pl) 
		: FitManage(fit)
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
		FitProcessing* fit = this->parent();
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
}
