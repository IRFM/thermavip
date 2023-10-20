#include "VipProcessingObjectEditor.h"
#include "VipDataType.h"
#include "VipDisplayArea.h"
#include "VipDisplayObject.h"
#include "VipExtractStatistics.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipPlayer.h"
#include "VipPlotItem.h"
#include "VipProcessingObjectTree.h"
#include "VipProgress.h"
#include "VipStandardProcessing.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"
#include "VipTimer.h"
#include "VipXmlArchive.h"
#include "VipQuiver.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QRadioButton>
#include <QToolButton>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <qtimer.h>

#define __VIP_MAX_DISPLAYED_EDITORS 5

class VipOtherPlayerDataEditor::PrivateData
{
public:
	VipOtherPlayerData data;
	QCheckBox dynamic;
	VipComboBox players;
	VipComboBox displays;
	QLabel tdisplays;
	QComboBox operation;

	QWidget* lineBefore;
	QWidget* lineAfter;
};

VipOtherPlayerDataEditor::VipOtherPlayerDataEditor()
  : QWidget()
{
	m_data = new PrivateData();
	m_data->tdisplays.setText("Operation on data:");

	m_data->lineBefore = VipLineWidget::createHLine();
	m_data->lineAfter = VipLineWidget::createHLine();

	m_data->players.setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
	m_data->players.setMaximumWidth(200);
	m_data->displays.setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
	m_data->displays.setMaximumWidth(200);

	QGridLayout* lay = new QGridLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(m_data->lineBefore, 0, 0, 1, 2);
	lay->addWidget(new QLabel("Dynamic operation:"), 1, 0);
	lay->addWidget(&m_data->dynamic, 1, 1);
	lay->addWidget(new QLabel("Operation on player:"), 2, 0);
	lay->addWidget(&m_data->players, 2, 1);
	lay->addWidget(&m_data->tdisplays, 3, 0);
	lay->addWidget(&m_data->displays, 3, 1);
	lay->addWidget(m_data->lineAfter, 4, 0, 1, 2);

	m_data->tdisplays.hide();
	m_data->displays.hide();

	m_data->dynamic.setToolTip("If checked, the operation will be performed on the current image from the selected player.<br>"
				   "Otherwise the operation will always be performed on the same data (image or curve). You can reset this processing to change the data.");
	m_data->players.setToolTip("Apply the operation on selected player: add, subtract, multiply or divide this data (image or curve) with the selected player's data.");
	m_data->displays.setToolTip("<b>There are several items in this player</b><br>Select the item to apply the operation on");
	setLayout(lay);

	connect(&m_data->players, SIGNAL(openPopup()), this, SLOT(showPlayers()));
	connect(&m_data->dynamic, SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(&m_data->players, SIGNAL(activated(int)), this, SLOT(apply()));
	connect(&m_data->displays, SIGNAL(openPopup()), this, SLOT(showDisplays()));
	connect(&m_data->displays, SIGNAL(activated(int)), this, SLOT(apply()));
}

VipOtherPlayerDataEditor::~VipOtherPlayerDataEditor()
{
	delete m_data;
}
// void VipOtherPlayerDataEditor::displayVLines(bool before, bool after)
// {
// m_data->lineBefore->setVisible(before);
// m_data->lineAfter->setVisible(after);
// }

struct player_id
{
	int id;
	QString title;
	player_id(int id = 0, const QString& title = QString())
	  : id(id)
	  , title(title)
	{
	}
	bool operator<(const player_id& other) const { return title < other.title; }
};
void VipOtherPlayerDataEditor::showPlayers()
{

	m_data->players.blockSignals(true);
	m_data->players.clear();
	QWidget* w = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (w) {

		int current_index = -1;
		int current_id = m_data->data.otherPlayerId();

		// compute the list of all VipVideoPlayer in the current workspace
		QList<VipPlayer2D*> instances = w->findChildren<VipPlayer2D*>();

		// remove the player containing this VipOperationBetweenPlayers

		std::vector<player_id> players;
		for (int i = 0; i < instances.size(); ++i) {
			VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(instances[i]);
			QString title = parent ? parent->windowTitle() : instances[i]->windowTitle();
			// m_data->players.addItem(title, VipUniqueId::id(instances[i]));
			players.push_back(player_id(VipUniqueId::id(instances[i]), title));
			if (current_id == VipUniqueId::id(instances[i]))
				current_index = m_data->players.count() - 1;
		}
		std::sort(players.begin(), players.end());
		for (size_t i = 0; i < players.size(); ++i) {
			m_data->players.addItem(players[i].title, players[i].id);
			if (current_id == players[i].id)
				current_index = m_data->players.count() - 1;
		}
		if (current_index >= 0)
			m_data->players.setCurrentIndex(current_index);
	}
	m_data->players.blockSignals(false);
}

void VipOtherPlayerDataEditor::showDisplays()
{
	m_data->displays.clear();
	QVariant player = m_data->players.currentData();
	if (player.userType() != QMetaType::Int)
		return;

	int current_id = m_data->data.otherDisplayIndex();
	int current_index = -1;

	m_data->displays.blockSignals(true);
	VipPlayer2D* pl = VipUniqueId::find<VipPlayer2D>(player.toInt());
	if (pl) {
		QList<VipDisplayObject*> displays = pl->displayObjects();
		for (int i = 0; i < displays.size(); ++i) {
			QString text = displays[i]->title();
			m_data->displays.addItem(text, VipUniqueId::id(displays[i]));
			if (VipUniqueId::id(displays[i]) == current_id)
				current_index = m_data->displays.count() - 1;
		}

		m_data->displays.setVisible(displays.size() > 1);
		m_data->tdisplays.setVisible(displays.size() > 1);
	}
	if (current_index >= 0)
		m_data->displays.setCurrentIndex(current_index);

	m_data->displays.blockSignals(false);
}

VipOtherPlayerData VipOtherPlayerDataEditor::value() const
{
	return m_data->data;
}

void VipOtherPlayerDataEditor::setValue(const VipOtherPlayerData& _data)
{
	m_data->data = _data;

	m_data->dynamic.blockSignals(true);
	m_data->dynamic.setChecked(_data.isDynamic());
	m_data->dynamic.blockSignals(false);
	showPlayers();
	showDisplays();

	apply();
}

void VipOtherPlayerDataEditor::apply()
{

	QVariant player = m_data->players.currentData();
	if (player.userType() != QMetaType::Int)
		return;

	VipPlayer2D* pl = VipUniqueId::find<VipPlayer2D>(player.toInt());
	if (pl) {
		VipDisplayObject* display = nullptr;
		QList<VipDisplayObject*> displays = pl->displayObjects();
		if (displays.size() > 1) {
			display = VipUniqueId::find<VipDisplayObject>(m_data->displays.currentData().toInt());
			if (!display)
				showDisplays();
		}
		else if (displays.size() == 1)
			display = displays.front();

		m_data->displays.setVisible(displays.size() > 1);
		m_data->tdisplays.setVisible(displays.size() > 1);

		if (display) {
			// set all properties
			VipOutput* out = display->inputAt(0)->connection()->source();
			VipProcessingObject* proc = out->parentProcessing();
			bool isDynamic = m_data->dynamic.isChecked();
			// bool isSame = (isDynamic == m_data->data.isDynamic() && proc == m_data->data.processing() && proc->indexOf(out) == m_data->data.outputIndex() &&
			// m_data->data.otherPlayerId() == player.toInt() && m_data->data.otherDisplayIndex() == VipUniqueId::id(display));
			m_data->data = VipOtherPlayerData(isDynamic, proc, m_data->data.parentProcessingObject(), proc->indexOf(out), player.toInt(), VipUniqueId::id(display));

			// if (!isSame)
			Q_EMIT valueChanged(QVariant::fromValue(m_data->data));
		}
	}
}

class VipFindDataButton::PrivateData
{
public:
	PrivateData()
	  : index(0)
	{
	}
	QPointer<VipPlayer2D> player;
	QPointer<VipProcessingObject> processing;
	int index;
};

VipFindDataButton::VipFindDataButton(QWidget* parent)
  : QToolButton(parent)
{
	m_data = new PrivateData();

	this->setPopupMode(QToolButton::InstantPopup);
	this->setText("No data selected");
	this->setToolTip("Select a processing output");
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	this->setToolTip("QToolButton{text-align: left;}");

	QMenu* menu = new QMenu();
	this->setMenu(menu);
	menu->setToolTipsVisible(true);

	connect(menu, SIGNAL(aboutToShow()), this, SLOT(menuShow()));
	connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(menuSelected(QAction*)));
}

VipFindDataButton::~VipFindDataButton()
{
	delete m_data;
}

VipOutput* VipFindDataButton::selectedData() const
{
	if (m_data->processing && m_data->index < m_data->processing->outputCount())
		return m_data->processing->outputAt(m_data->index);
	return nullptr;
}

void VipFindDataButton::setSelectedData(VipOutput* output)
{
	m_data->processing = output ? output->parentProcessing() : nullptr;
	m_data->index = m_data->processing ? m_data->processing->indexOf(output) : 0;
	if (!output) {
		this->setText("No data selected");
		this->setToolTip("Select a processing output");
		return;
	}

	QList<VipDisplayObject*> disp = vipListCast<VipDisplayObject*>(m_data->processing->allSinks());
	if (disp.size()) {
		if (VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(VipAbstractPlayer::findAbstractPlayer(disp.first()))) {
			if (VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(pl)) {
				QString text = "Player " + QString::number(VipUniqueId::id(parent)) + ": " + disp.first()->inputAt(0)->data().name();
				QString tool_tip = "<b>Player: </b>" + parent->windowTitle() + "<br><b>Data name: </b>" + disp.first()->inputAt(0)->data().name() + "<br><b>Data type: </b>" +
						   vipSplitClassname(disp.first()->inputAt(0)->data().data().typeName());

				this->setText(text);
				this->setToolTip(tool_tip);
			}
		}
	}
	Q_EMIT selectionChanged(m_data->processing, m_data->index);
}

void VipFindDataButton::resizeEvent(QResizeEvent* evt)
{
	elideText();
	QToolButton::resizeEvent(evt);
}

void VipFindDataButton::elideText()
{
	QFontMetrics m(font());
	this->setText(m.elidedText(this->text(), Qt::ElideRight, this->width()));
}

void VipFindDataButton::menuShow()
{
	this->menu()->clear();
	// compute the list of all players in the current workspace
	if (VipDisplayPlayerArea* w = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		QList<VipPlayer2D*> instances = w->findChildren<VipPlayer2D*>();

		for (int i = 0; i < instances.size(); ++i) {
			VipPlayer2D* pl = instances[i];
			if (VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(instances[i])) {
				QString title = parent->windowTitle();

				QList<VipDisplayObject*> objects = pl->displayObjects();

				// if the player has more than one display object, store them in a submenu
				QMenu* current = menu();
				if (objects.size() > 1) {
					current = new QMenu("Player " + parent->windowTitle());
					current->setToolTipsVisible(true);
				}

				for (int d = 0; d < objects.size(); ++d) {
					VipDisplayObject* disp = objects[d];
					if (disp->inputAt(0)->connection() && disp->inputAt(0)->connection()->source()) {
						QAction* act = current->addAction("");

						// set the action text
						QString text = "Player " + QString::number(VipUniqueId::id(parent)) + ": " + disp->inputAt(0)->data().name();
						QString tool_tip = "<b>Player: </b>" + title + "<br><b>Data name: </b>" + disp->inputAt(0)->data().name() + "<br><b>Data type: </b>" +
								   vipSplitClassname(disp->inputAt(0)->data().data().typeName());

						act->setToolTip(tool_tip);
						act->setText(text);

						act->setProperty("text", text);
						act->setProperty("tool_tip", tool_tip);
						act->setProperty("player", QVariant::fromValue(pl));
						VipProcessingObject* p = disp->inputAt(0)->connection()->source()->parentProcessing();
						int index = p->indexOf(disp->inputAt(0)->connection()->source());
						act->setProperty("processing", QVariant::fromValue(p));
						act->setProperty("output", index);
					}
				}

				if (current != menu())
					menu()->addMenu(current);
			}
		}
	}
}

void VipFindDataButton::menuSelected(QAction* act)
{
	m_data->processing = act->property("processing").value<VipProcessingObject*>();
	m_data->index = act->property("output").value<int>();
	if (m_data->processing && m_data->index < m_data->processing->outputCount()) {
		setToolTip(act->property("tool_tip").toString());
		setText(act->property("text").toString());

		Q_EMIT selectionChanged(m_data->processing, m_data->index);
	}
	else {
		setToolTip("Select a processing output");
		setText("No data selected");
	}
	elideText();
}

/// Small widget representing a processing input for VipEditDataFusionProcessing
struct InputWidget : public QWidget
{
	VipFindDataButton data;
	QToolButton add;
	QToolButton remove;
	InputWidget()
	{
		add.setAutoRaise(true);
		add.setText("+");
		add.setToolTip("Add a new input");
		add.setMaximumHeight(10);
		add.setStyleSheet("padding: 0px; margin: 0px;");
		remove.setAutoRaise(true);
		remove.setText(QChar(0x02DF));
		remove.setToolTip("Remove this input");
		remove.setStyleSheet("padding: 0px; margin: 0px;");
		remove.setMaximumHeight(10);

		QVBoxLayout* vlay = new QVBoxLayout();
		vlay->setContentsMargins(0, 0, 0, 0);
		vlay->addWidget(&add);
		vlay->addWidget(&remove);

		QHBoxLayout* lay = new QHBoxLayout();
		lay->addLayout(vlay);
		lay->addWidget(&data);
		lay->setContentsMargins(0, 0, 0, 0);
		setLayout(lay);
	}
};

class VipEditDataFusionProcessing::PrivateData
{
public:
	QPointer<VipBaseDataFusion> processing;
	VipUniqueProcessingObjectEditor editor;
	QListWidget inputList;
};

VipEditDataFusionProcessing::VipEditDataFusionProcessing(QWidget* parent)
  : QWidget(parent)
{
	m_data = new PrivateData();

	QGroupBox* box = new QGroupBox("Processing inputs");
	// box->setFlat(true);

	QVBoxLayout* blay = new QVBoxLayout();
	blay->addWidget(&m_data->inputList);
	box->setLayout(blay);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(&m_data->editor);
	vlay->addWidget(box);
	// vlay->addWidget(&m_data->inputList);
	setLayout(vlay);

	m_data->inputList.setToolTip("Setup processing inputs");
}

VipEditDataFusionProcessing::~VipEditDataFusionProcessing()
{
	delete m_data;
}

void VipEditDataFusionProcessing::setDataFusionProcessing(VipBaseDataFusion* p)
{
	// set the processing and update the input list accordingly

	m_data->editor.setProcessingObject(p);
	m_data->processing = p;

	QList<InputWidget*> inputs;

	if (p) {
		VipMultiInput* multi = p->topLevelInputAt(0)->toMultiInput();
		QPair<int, int> input_count(multi->minSize() ? multi->minSize() : 1, multi->maxSize());
		if (p->inputCount() < input_count.first)
			p->topLevelInputAt(0)->toMultiInput()->resize(input_count.first);

		int count = p->inputCount();
		if (count == 0)
			count = 1;
		for (int i = 0; i < count; ++i) {
			InputWidget* input = new InputWidget();
			inputs << input;
			input->add.setVisible(count < input_count.second);
			input->remove.setVisible(count > input_count.first);
			if (i < p->inputCount())
				input->data.setSelectedData(p->inputAt(i)->connection()->source());

			connect(&input->data, SIGNAL(selectionChanged(VipProcessingObject*, int)), this, SLOT(updateProcessing()));
			connect(&input->add, SIGNAL(clicked(bool)), this, SLOT(addInput()));
			connect(&input->remove, SIGNAL(clicked(bool)), this, SLOT(removeInput()));
		}
	}

	m_data->inputList.clear();
	for (int i = 0; i < inputs.size(); ++i) {
		QListWidgetItem* item = new QListWidgetItem(&m_data->inputList);
		item->setSizeHint(inputs[i]->sizeHint());
		m_data->inputList.addItem(item);
		m_data->inputList.setItemWidget(item, inputs[i]);
	}
}

VipBaseDataFusion* VipEditDataFusionProcessing::dataFusionProcessing() const
{
	return m_data->processing;
}

int VipEditDataFusionProcessing::indexOfInput(QObject* obj)
{
	for (int i = 0; i < m_data->inputList.count(); ++i) {
		if (&static_cast<InputWidget*>(m_data->inputList.itemWidget(m_data->inputList.item(i)))->add == obj ||
		    &static_cast<InputWidget*>(m_data->inputList.itemWidget(m_data->inputList.item(i)))->remove == obj)
			return i;
	}
	return 0;
}

void VipEditDataFusionProcessing::addInput()
{
	// add input and reset processing
	if (m_data->processing)
		m_data->processing->topLevelInputAt(0)->toMultiInput()->insert(indexOfInput(sender()) + 1);
	setDataFusionProcessing(m_data->processing);
}
void VipEditDataFusionProcessing::removeInput()
{
	// remove input and reset processing
	if (m_data->processing)
		m_data->processing->topLevelInputAt(0)->toMultiInput()->removeAt(indexOfInput(sender()));
	setDataFusionProcessing(m_data->processing);
}

void VipEditDataFusionProcessing::updateProcessing()
{
	// get the list of VipOutput
	QList<VipOutput*> outputs;
	for (int i = 0; i < m_data->inputList.count(); ++i) {
		InputWidget* input = static_cast<InputWidget*>(m_data->inputList.itemWidget(m_data->inputList.item(i)));
		if (VipOutput* out = input->data.selectedData()) {
			outputs.append(out);
		}
		else
			return;
	}
	// set the outputs to the processing inputs
	if (m_data->processing) {
		m_data->processing->topLevelInputAt(0)->toMultiInput()->resize(outputs.size());
		for (int i = 0; i < outputs.size(); ++i) {
			m_data->processing->inputAt(i)->setConnection(outputs[i]);
			m_data->processing->inputAt(i)->buffer()->clear();
			m_data->processing->inputAt(i)->setData(outputs[i]->data());
		}
	}
	m_data->processing->reload();
}

class VipProcessingObjectEditor::PrivateData
{
public:
	QRadioButton oneInput;
	QRadioButton multiInput;
	QCheckBox enable;
	QPointer<VipProcessingObject> proc;
};

VipProcessingObjectEditor::VipProcessingObjectEditor()
  : QWidget()
{
	m_data = new PrivateData();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_data->oneInput);
	lay->addWidget(&m_data->multiInput);
	lay->addWidget(&m_data->enable);
	lay->setSpacing(0);
	setLayout(lay);

	m_data->oneInput.setText("Run when at least one input is new");
	m_data->oneInput.setToolTip("The processing will be triggered at each new input data");
	m_data->multiInput.setText("Run when all inputs are new");
	m_data->multiInput.setToolTip("The processing will be triggered when all input data are new");
	m_data->enable.setText("Enable the processing");

	connect(&m_data->oneInput, SIGNAL(clicked(bool)), this, SLOT(updateProcessingObject()));
	connect(&m_data->multiInput, SIGNAL(clicked(bool)), this, SLOT(updateProcessingObject()));
	connect(&m_data->enable, SIGNAL(clicked(bool)), this, SLOT(updateProcessingObject()));
}

VipProcessingObjectEditor::~VipProcessingObjectEditor()
{
	delete m_data;
}

void VipProcessingObjectEditor::setProcessingObject(VipProcessingObject* obj)
{
	m_data->proc = obj;
	if (obj) {
		m_data->oneInput.blockSignals(true);
		m_data->multiInput.blockSignals(true);
		m_data->enable.blockSignals(true);

		m_data->multiInput.setChecked(obj->testScheduleStrategy(VipProcessingObject::AllInputs));
		m_data->enable.setChecked(obj->isEnabled());

		m_data->oneInput.blockSignals(false);
		m_data->multiInput.blockSignals(false);
		m_data->enable.blockSignals(false);
	}
}

void VipProcessingObjectEditor::updateProcessingObject()
{
	if (m_data->proc) {
		m_data->proc->setEnabled(m_data->enable.isChecked());
		m_data->proc->setScheduleStrategy(VipProcessingObject::AllInputs, m_data->multiInput.isChecked());
	}
}

class VipIODeviceEditor::PrivateData
{
public:
	QCheckBox openRead;
	QCheckBox openWrite;
	QLabel info;
	QPointer<VipIODevice> device;
};

VipIODeviceEditor::VipIODeviceEditor()
  : QWidget()
{
	m_data = new PrivateData();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_data->openRead);
	lay->addWidget(&m_data->openWrite);
	lay->addWidget(&m_data->info);
	lay->setSpacing(0);
	setLayout(lay);

	m_data->openRead.setText("Open the device in Read mode");
	m_data->openRead.setToolTip("Open/close the device in Read mode");
	m_data->openWrite.setText("Open the device in Write mode");
	m_data->openWrite.setToolTip("Open/close the device in Write mode");

	connect(&m_data->openRead, SIGNAL(clicked(bool)), this, SLOT(updateIODevice()));
	connect(&m_data->openWrite, SIGNAL(clicked(bool)), this, SLOT(updateIODevice()));
}

VipIODeviceEditor::~VipIODeviceEditor()
{
	delete m_data;
}

void VipIODeviceEditor::setIODevice(VipIODevice* obj)
{
	m_data->device = obj;
	if (obj) {
		m_data->openRead.blockSignals(true);
		m_data->openWrite.blockSignals(true);

		m_data->openRead.setChecked(obj->openMode() & VipIODevice::ReadOnly);
		m_data->openWrite.setChecked(obj->openMode() & VipIODevice::WriteOnly);
		m_data->openRead.setVisible(obj->supportedModes() & VipIODevice::ReadOnly);
		m_data->openWrite.setVisible(obj->supportedModes() & VipIODevice::WriteOnly);

		QFontMetrics m(font());
		QString name = m.elidedText(obj->attribute("Name").toString(), Qt::ElideRight, 200);

		QStringList text;
		if (obj->hasAttribute("Name"))
			text += "<b>Name</b>: " + name;
		if (obj->hasAttribute("Author"))
			text += "<b>Author</b>: " + obj->attribute("Author").toString();
		if (obj->hasAttribute("Date"))
			text += "<b>Date</b>: " + QDateTime::fromMSecsSinceEpoch(obj->attribute("Date").toLongLong() / 1000000).toString("dd/MM/yyyy,  hh:mm:ss");
		if (obj->hasAttribute("Comment"))
			text += "<b>Comment</b>: " + obj->attribute("Author").toString();

		m_data->info.setText(text.join("<br>"));
		m_data->info.setToolTip(text.join("<br>"));
		m_data->openRead.blockSignals(false);
		m_data->openWrite.blockSignals(false);
	}
}

void VipIODeviceEditor::updateIODevice()
{
	if (m_data->device) {
		if (m_data->openRead.isChecked() && !(m_data->device->openMode() & VipIODevice::ReadOnly)) {
			m_data->device->close();
			m_data->device->open(VipIODevice::ReadOnly);
		}
		else if (!m_data->openRead.isChecked() && (m_data->device->openMode() & VipIODevice::ReadOnly))
			m_data->device->close();

		if (m_data->openWrite.isChecked() && !(m_data->device->openMode() & VipIODevice::WriteOnly)) {
			m_data->device->close();
			m_data->device->open(VipIODevice::WriteOnly);
		}
		else if (!m_data->openWrite.isChecked() && (m_data->device->openMode() & VipIODevice::WriteOnly))
			m_data->device->close();
	}
}

class ProcessingListWidgetItem : public QListWidgetItem
{
public:
	ProcessingListWidgetItem(VipProcessingObject* obj)
	  : QListWidgetItem(nullptr, QListWidgetItem::UserType)
	  , processing(obj)
	{
		// QString text = obj->objectName();
		// if(text.isEmpty())
		QString text = obj->info().classname;
		text = vipSplitClassname(text);

		QString obj_name = obj->property("_vip_processingName").toString();
		if (!obj_name.isEmpty())
			text = obj_name;

		setText(text);
		setIcon(obj->icon());
		setToolTip(obj->description());

		// setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
		setFlags(flags() | Qt::ItemIsUserCheckable); // set checkable flag
		setCheckState(obj->isEnabled() ? Qt::Checked : Qt::Unchecked);

		if (!obj->isVisible())
			setHidden(true);
	}

	QPointer<VipProcessingObject> processing;
};

class ListWidget : public QListWidget
{
	VipProcessingListEditor* editor;

public:
	int find(VipProcessingObject* it)
	{
		for (int i = 0; i < count(); ++i)
			if (static_cast<ProcessingListWidgetItem*>(item(i))->processing == it)
				return i;

		return -1;
	}

	ListWidget(VipProcessingListEditor* editor)
	  : editor(editor)
	{
		// setDragDropMode(InternalMove);
	}

protected:
	virtual void keyPressEvent(QKeyEvent* evt)
	{
		if (evt->key() == Qt::Key_Delete) {
			qDeleteAll(this->selectedItems());
			editor->updateProcessingList();
			editor->clearEditor();
		}
		else if (evt->key() == Qt::Key_A && (evt->modifiers() & Qt::ControlModifier)) {
			for (int i = 0; i < count(); ++i)
				item(i)->setSelected(true);
			editor->updateProcessingList();
		}
	}

	virtual void dragEnterEvent(QDragEnterEvent* evt)
	{
		if (evt->mimeData()->hasFormat("processing/processing-list"))
			evt->acceptProposedAction();
		else
			return QListWidget::dragEnterEvent(evt);
	}

	virtual void dropEvent(QDropEvent* evt)
	{
		// if (evt->mimeData()->hasFormat("processing/processing-list"))
		// {
		// //find the right insertion position
		// int insertPos = this->count();
		// QListWidgetItem * item = itemAt(evt->pos());
		// if(item)
		// {
		// QRect rect = this->visualItemRect(item);
		// insertPos = this->indexFromItem(item).row();
		// if(evt->pos().y() > rect.center().y())
		//	insertPos++;
		// }
		//
		// //insert the items
		// QStringList classnames = QString(evt->mimeData()->data("processing/processing-list")).split("\n");
		// for(int i=0; i < classnames.size(); ++i)
		// {
		// VipProcessingObject * obj = vipCreateVariant((classnames[i].toLatin1() + "*").data()).value<VipProcessingObject*>();
		// if(obj)
		// {
		//	this->insertItem(insertPos,new ProcessingListWidgetItem(obj));
		//	insertPos++;
		// }
		// }
		// }
		// else
		// {
		// QListWidget::dropEvent(evt);
		// }
		// find the right insertion position

		int insertPos = this->count();
		QListWidgetItem* item = itemAt(evt->pos());
		if (item) {
			QRect rect = this->visualItemRect(item);
			insertPos = this->indexFromItem(item).row();
			if (evt->pos().y() > rect.center().y())
				insertPos++;
		}

		if (evt->mimeData()->hasFormat("processing/processing-list")) {
			// insert the items
			QStringList classnames = QString(evt->mimeData()->data("processing/processing-list")).split("\n");
			for (int i = 0; i < classnames.size(); ++i) {
				VipProcessingObject* obj = vipCreateVariant((classnames[i].toLatin1() + "*").data()).value<VipProcessingObject*>();
				if (obj) {
					this->insertItem(insertPos, new ProcessingListWidgetItem(obj));
					insertPos++;
				}
			}
		}
		else {
			QObject* src = evt->source();
			if (src == this) {
				// internal move
				QList<QListWidgetItem*> items = selectedItems();
				if (items.size() == 1) {

					int index = indexFromItem(items.first()).row();
					if (index != insertPos) {
						if (index < insertPos)
							insertPos--;
						takeItem(index);
						insertItem(insertPos, items.first());
					}
				}
			}
		}

		this->editor->updateProcessingList();
	}

	virtual void mousePressEvent(QMouseEvent* evt)
	{
		QListWidget::mousePressEvent(evt);
		if (evt->buttons() & Qt::RightButton) {
			QMenu menu;
			menu.setToolTipsVisible(true);
			QAction* copy = menu.addAction(vipIcon("copy.png"), "Copy selected processing");
			QAction* cut = menu.addAction(vipIcon("cut.png"), "Cut selected processing");
			menu.addSeparator();
			QAction* paste = menu.addAction(vipIcon("paste.png"), "Paste copied processing");
			paste->setToolTip("New processing will be inserted before the selected one");

			connect(copy, SIGNAL(triggered(bool)), editor, SLOT(copySelection()));
			connect(cut, SIGNAL(triggered(bool)), editor, SLOT(cutSelection()));
			connect(paste, SIGNAL(triggered(bool)), editor, SLOT(pasteCopiedItems()));

			menu.exec(evt->screenPos().toPoint());
		}
	}
};

class VipProcessingListEditor::PrivateData
{
public:
	ListWidget* list;
	VipProcessingObjectMenu* tree;
	QToolBar* toolBar;
	QToolButton* addProcessing;

	VipUniqueProcessingObjectEditor* editor;
	QPointer<VipProcessingList> processingList;
	QList<QPointer<VipProcessingObject>> hidden;

	QList<VipProcessingObject::Info> infos;
	QList<int> user_types;

	VipTimer timer;
};

VipProcessingListEditor::VipProcessingListEditor()
{
	m_data = new PrivateData();
	m_data->editor = new VipUniqueProcessingObjectEditor();
	m_data->editor->setShowExactProcessingOnly(true);

	m_data->user_types = vipUserTypes();
	m_data->list = new ListWidget(this);
	m_data->tree = new VipProcessingObjectMenu();
	connect(m_data->tree, SIGNAL(selected(const VipProcessingObject::Info&)), this, SLOT(addSelectedProcessing()));

	m_data->list->setDragDropMode(QAbstractItemView::InternalMove);
	m_data->list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_data->list->setDragDropOverwriteMode(false);
	m_data->list->setDefaultDropAction(Qt::TargetMoveAction);
	m_data->list->setToolTip("Stack of processing");

	m_data->toolBar = new QToolBar();
	m_data->toolBar->setIconSize(QSize(18, 18));

	m_data->addProcessing = new QToolButton();
	m_data->addProcessing->setAutoRaise(true);
	m_data->addProcessing->setMenu(m_data->tree);
	m_data->addProcessing->setPopupMode(QToolButton::InstantPopup);
	m_data->addProcessing->setIcon(vipIcon("processing.png"));
	m_data->addProcessing->setText("Add a processing");
	m_data->addProcessing->setToolTip("<b>Add a new processing into the processing list</b><br>The processing will be added at the end of the list. Use the mouse to change the processing order.");
	m_data->addProcessing->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_data->addProcessing->setIconSize(QSize(18, 18));
	m_data->toolBar->addWidget(m_data->addProcessing);
	connect(m_data->addProcessing, SIGNAL(clicked(bool)), this, SLOT(updateProcessingTree()));
	connect(m_data->tree, SIGNAL(aboutToShow()), this, SLOT(updateProcessingTree()), Qt::DirectConnection);

	QAction* showList = m_data->toolBar->addAction(vipIcon("down.png"), "Show/Hide processing list");
	showList->setCheckable(true);
	showList->setChecked(true);
	connect(showList, SIGNAL(triggered(bool)), m_data->list, SLOT(setVisible(bool)));

	m_data->toolBar->addSeparator();
	QAction* copy = m_data->toolBar->addAction(vipIcon("copy.png"), "Copy selected processing");
	QAction* cut = m_data->toolBar->addAction(vipIcon("cut.png"), "Cut selected processing");
	m_data->toolBar->addSeparator();
	QAction* paste = m_data->toolBar->addAction(vipIcon("paste.png"), "Paste copied processing.\nNew processing will be inserted before the selected one");

	connect(copy, SIGNAL(triggered(bool)), this, SLOT(copySelection()));
	connect(cut, SIGNAL(triggered(bool)), this, SLOT(cutSelection()));
	connect(paste, SIGNAL(triggered(bool)), this, SLOT(pasteCopiedItems()));

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->addWidget(m_data->toolBar);
	vlay->addWidget(m_data->list);
	// TEST: add stretch factor
	vlay->addWidget(m_data->editor, 1);

	setLayout(vlay);

	connect(m_data->list, SIGNAL(itemSelectionChanged()), this, SLOT(selectedItemChanged()));
	connect(m_data->list, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)));

	m_data->timer.setSingleShot(true);
	m_data->timer.setInterval(500);
	connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(resetProcessingList()));
}

VipProcessingListEditor::~VipProcessingListEditor()
{
	disconnect(&m_data->timer, SIGNAL(timeout()), this, SLOT(resetProcessingList()));
	m_data->timer.stop();
	delete m_data;
}

void VipProcessingListEditor::selectedItemChanged()
{
	QList<QListWidgetItem*> items = m_data->list->selectedItems();
	if (items.size()) {
		VipProcessingObject* obj = static_cast<ProcessingListWidgetItem*>(items.back())->processing;
		if (obj) {
			m_data->editor->setProcessingObject(obj);
			Q_EMIT selectionChanged(m_data->editor);
			m_data->editor->show();
			VipUniqueProcessingObjectEditor::geometryChanged(m_data->editor->parentWidget());
		}
	}
}

void VipProcessingListEditor::clearEditor()
{
	m_data->editor->setProcessingObject(nullptr);
}

void VipProcessingListEditor::resetProcessingList()
{
	// if(isVisible())
	setProcessingList(processingList());
}

void VipProcessingListEditor::updateProcessingTree()
{
	if (m_data->processingList) {
		// create a list of all VipProcessingObject that can be inserted into a VipProcessingList
		QVariantList lst;
		lst.append(m_data->processingList->inputAt(0)->probe().data());

		QList<int> current_types = vipUserTypes();
		if (m_data->infos.isEmpty() || current_types != m_data->user_types) {
			m_data->user_types = current_types;
			m_data->infos = VipProcessingObject::validProcessingObjects(lst, 1, VipProcessingObject::InputTransform).values();

			// only keep the Info with inputTransformation to true
			for (int i = 0; i < m_data->infos.size(); ++i) {
				if (m_data->infos[i].displayHint != VipProcessingObject::InputTransform) {
					m_data->infos.removeAt(i);
					--i;
				}
			}
		}

		m_data->tree->setProcessingInfos(m_data->infos);
	}
}

void VipProcessingListEditor::setProcessingList(VipProcessingList* lst)
{
	if (m_data->processingList)
		disconnect(m_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &m_data->timer, SLOT(start()));

	m_data->list->clear();
	m_data->processingList = lst;

	// clean the hidden processing list
	for (int i = 0; i < m_data->hidden.size(); ++i) {
		if (!m_data->hidden[i]) {
			m_data->hidden.removeAt(i);
			--i;
		}
	}

	if (lst) {
		this->setObjectName(lst->objectName());
		connect(m_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &m_data->timer, SLOT(start()));

		QList<VipProcessingObject*> objects = lst->processings();
		for (int i = 0; i < objects.size(); ++i) {
			m_data->list->addItem(new ProcessingListWidgetItem(objects[i]));
			if (m_data->hidden.indexOf(objects[i]) >= 0 || !objects[i]->isVisible())
				m_data->list->item(m_data->list->count() - 1)->setHidden(true);
		}
	}

	// reset the ListWidget size
	m_data->list->setMaximumHeight(m_data->list->count() * 30 + 30);
	VipUniqueProcessingObjectEditor::geometryChanged(this);

	// update the possible VipProcessingObject
	updateProcessingTree();
}

VipProcessingList* VipProcessingListEditor::processingList() const
{
	return m_data->processingList;
}

VipUniqueProcessingObjectEditor* VipProcessingListEditor::editor() const
{
	return m_data->editor;
}

QToolButton* VipProcessingListEditor::addProcessingButton() const
{
	return m_data->addProcessing;
}

QListWidget* VipProcessingListEditor::list() const
{
	return m_data->list;
}

void VipProcessingListEditor::addProcessings(const QList<VipProcessingObject::Info>& infos)
{
	if (!m_data->processingList) {
		VIP_LOG_ERROR("No processing list available");
		return;
	}
	m_data->processingList->blockSignals(true);

	QList<VipProcessingObject*> added;
	for (int i = 0; i < infos.size(); ++i) {
		VipProcessingObject* obj = infos[i].create();
		if (obj) {
			added.append(obj);
			m_data->processingList->append(obj);
		}
	}

	m_data->processingList->blockSignals(false);

	if (added.size()) {
		setProcessingList(m_data->processingList);
		m_data->processingList->reload();

		// select the added VipProcessingObject
		for (int i = 0; i < added.size(); ++i) {
			m_data->list->item(m_data->list->find(added[i]))->setSelected(true);
		}
	}

	// m_data->processingList->blockSignals(false);
	m_data->addProcessing->menu()->hide();

	// TODO: reemit signal for other editors
	disconnect(m_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &m_data->timer, SLOT(start()));
	m_data->processingList->emitProcessingChanged();
	connect(m_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &m_data->timer, SLOT(start()));
}

void VipProcessingListEditor::selectObject(VipProcessingObject* obj)
{
	m_data->list->clearSelection();
	for (int i = 0; i < m_data->list->count(); ++i) {
		ProcessingListWidgetItem* item = static_cast<ProcessingListWidgetItem*>(m_data->list->item(i));
		if (item->processing == obj) {
			//m_data->list->setItemSelected(item, true);
			item->setSelected(true);
			break;
		}
	}
}

QListWidgetItem* VipProcessingListEditor::item(VipProcessingObject* obj) const
{
	for (int i = 0; i < m_data->list->count(); ++i) {
		ProcessingListWidgetItem* item = static_cast<ProcessingListWidgetItem*>(m_data->list->item(i));
		if (item->processing == obj)
			return item;
	}
	return nullptr;
}

static QString __copied_items;

QString VipProcessingListEditor::copiedItems()
{
	return __copied_items;
}

QList<VipProcessingObject*> VipProcessingListEditor::copiedProcessing()
{
	if (__copied_items.isEmpty())
		return QList<VipProcessingObject*>();

	QList<VipProcessingObject*> res;
	VipXIStringArchive arch(__copied_items);
	if (arch.start("processing")) {
		while (true) {
			if (VipProcessingObject* proc = arch.read().value<VipProcessingObject*>())
				res.append(proc);
			else
				break;
		}
	}
	return res;
}

void VipProcessingListEditor::copySelection()
{
	QList<QListWidgetItem*> items = m_data->list->selectedItems();
	if (items.size()) {
		VipXOStringArchive arch;
		arch.start("processing");
		for (int i = 0; i < items.size(); ++i) {
			arch.content(static_cast<ProcessingListWidgetItem*>(items[i])->processing.data());
		}
		arch.end();
		__copied_items = arch.toString();
	}
}
void VipProcessingListEditor::cutSelection()
{
	QList<QListWidgetItem*> items = m_data->list->selectedItems();
	copySelection();
	if (m_data->processingList)
		for (int i = 0; i < items.size(); ++i) {
			m_data->processingList->remove(static_cast<ProcessingListWidgetItem*>(items[i])->processing.data());
		}
}

void VipProcessingListEditor::pasteCopiedItems()
{
	if (m_data->processingList) {
		QList<QListWidgetItem*> items = m_data->list->selectedItems();
		int index = items.size() ? m_data->list->row(items.last()) : -1;

		QList<VipProcessingObject*> procs = copiedProcessing();
		if (index < 0) {
			for (int i = 0; i < procs.size(); ++i)
				m_data->processingList->append(procs[i]);
		}
		else {
			for (int i = 0; i < procs.size(); ++i, ++index)
				m_data->processingList->insert(index, procs[i]);
		}
	}
}

void VipProcessingListEditor::setProcessingVisible(VipProcessingObject* obj, bool visible)
{
	if (!visible && m_data->hidden.indexOf(obj) < 0)
		m_data->hidden.append(obj);
	else if (visible)
		m_data->hidden.removeOne(obj);

	for (int i = 0; i < m_data->list->count(); ++i) {
		ProcessingListWidgetItem* item = static_cast<ProcessingListWidgetItem*>(m_data->list->item(i));
		if (item->processing == obj) {
			if (visible)
				item->setHidden(false);
			else
				item->setHidden(true);
			item->processing->setVisible(visible);
		}
	}
}

void VipProcessingListEditor::addSelectedProcessing()
{
	if (!m_data->processingList)
		return;

	QList<VipProcessingObject::Info> infos = QList<VipProcessingObject::Info>() << m_data->tree->selectedProcessingInfo();
	addProcessings(infos);
}

void VipProcessingListEditor::itemChanged(QListWidgetItem* item)
{
	ProcessingListWidgetItem* it = static_cast<ProcessingListWidgetItem*>(item);
	if (it->processing)
		it->processing->setEnabled(item->checkState() == Qt::Checked);
	if (m_data->processingList)
		m_data->processingList->reload();
}

void VipProcessingListEditor::updateProcessingList()
{
	if (!m_data->processingList)
		return;

	m_data->processingList->blockSignals(true);

	// update the VipProcessingList after a change in the ListWidget (deletetion or drag/drop)

	// first, remove all VipProcessingObject for the VipProcessingList without deleting them
	QList<VipProcessingObject*> removed;
	while (m_data->processingList->size() > 0)
		removed.append(m_data->processingList->take(0));

	// now add all items from the ListWidget
	for (int i = 0; i < m_data->list->count(); ++i) {
		m_data->processingList->append(static_cast<ProcessingListWidgetItem*>(m_data->list->item(i))->processing);
	}

	// delete orphan objects
	for (int i = 0; i < removed.size(); ++i)
		if (m_data->processingList->indexOf(removed[i]) < 0)
			delete removed[i];

	// for the update of the VipProcessingList
	m_data->processingList->update(true);

	// reset the ListWidget size
	m_data->list->setMaximumHeight(m_data->list->count() * 30 + 30);
	VipUniqueProcessingObjectEditor::geometryChanged(this);

	m_data->processingList->blockSignals(false);

	// TODO: reemit signal for other editors, but not this one
	disconnect(m_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &m_data->timer, SLOT(start()));
	m_data->processingList->emitProcessingChanged();
	connect(m_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &m_data->timer, SLOT(start()));
}

class VipSplitAndMergeEditor::PrivateData
{
public:
	QToolButton method;
	QMenu* methods;

	QList<VipProcessingListEditor*> editors;
	QList<VipUniqueProcessingObjectEditor*> procEditors;
	QPointer<VipSplitAndMerge> proc;
	QHBoxLayout* procsLayout;
};

VipSplitAndMergeEditor::VipSplitAndMergeEditor(QWidget* parent)
  : QWidget(parent)
{
	m_data = new PrivateData();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&m_data->method);
	lay->addLayout(m_data->procsLayout = new QHBoxLayout());
	m_data->procsLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	m_data->methods = new QMenu(&m_data->method);
	m_data->method.setMenu(m_data->methods);
	m_data->method.setPopupMode(QToolButton::InstantPopup);
	m_data->method.setToolTip("<b>Select the split method</b><br>The input data will be splitted in several components according to given method."
				  "You can then add different processings for each component. Each component will be merged back to construct the output data.");

	connect(m_data->methods, SIGNAL(aboutToShow()), this, SLOT(computeMethods()));
	connect(m_data->methods, SIGNAL(triggered(QAction*)), this, SLOT(newMethod(QAction*)));
}

VipSplitAndMergeEditor::~VipSplitAndMergeEditor()
{
	delete m_data;
}

void VipSplitAndMergeEditor::computeMethods()
{
	setProcessing(processing());
}

void VipSplitAndMergeEditor::setProcessing(VipSplitAndMerge* proc)
{
	m_data->proc = proc;
	if (proc) {
		// Create the methods menu
		// Find the possible split methods
		QStringList methods = VipSplitAndMerge::possibleMethods(proc->inputAt(0)->probe().data());
		if (methods.isEmpty()) {
			// No split method found: try with the parent VipProcessingList (if any) input data
			if ( // VipProcessingList * lst =
			  proc->property("VipProcessingList").value<VipProcessingList*>()) {
				methods = VipSplitAndMerge::possibleMethods(proc->inputAt(0)->probe().data());
			}
		}
		m_data->methods->blockSignals(true);
		m_data->methods->clear();
		for (int i = 0; i < methods.size(); ++i) {
			QAction* act = m_data->methods->addAction(methods[i]);
			act->setCheckable(true);
			if (methods[i] == proc->method())
				act->setChecked(true);
		}
		if (proc->method().isEmpty()) {
			m_data->method.setText("No splitting/merging applied");
		}
		else
			m_data->method.setText(proc->method());
		m_data->methods->blockSignals(false);

		// if (methods.size() && methods.indexOf(proc->method()) < 0)
		// {
		// //the proc does not have a method set, set it!
		// proc->setMethod(methods.first())
		// }

		// create the editors
		if (m_data->editors.size() != proc->componentCount()) {
			// delete all editors
			for (int i = 0; i < m_data->editors.size(); ++i)
				delete m_data->editors[i];
			m_data->editors.clear();

			// delete all proc editors
			for (int i = 0; i < m_data->procEditors.size(); ++i)
				delete m_data->procEditors[i];
			m_data->procEditors.clear();

			for (int i = 0; i < proc->componentCount(); ++i) {
				m_data->editors.append(new VipProcessingListEditor());
				m_data->procsLayout->addWidget(m_data->editors.back());

				m_data->procEditors.append(m_data->editors.back()->editor());
				this->layout()->addWidget(m_data->procEditors.back());
				m_data->procEditors.back()->hide();

				connect(m_data->editors.back(), SIGNAL(selectionChanged(VipUniqueProcessingObjectEditor*)), this, SLOT(receiveSelectionChanged(VipUniqueProcessingObjectEditor*)));
			}
		}

		// customize editors
		QStringList components = proc->components();
		if (components.size() == proc->componentCount()) {
			for (int i = 0; i < m_data->editors.size(); ++i) {
				m_data->editors[i]->addProcessingButton()->setText(components[i]);
				m_data->editors[i]->addProcessingButton()->setToolTip("Add processing for '" + components[i] + "' component'");
			}

			// set the processing list
			for (int i = 0; i < m_data->editors.size(); ++i)
				m_data->editors[i]->setProcessingList(proc->componentProcessings(i));
		}
	}
}

VipSplitAndMerge* VipSplitAndMergeEditor::processing() const
{
	return m_data->proc;
}

void VipSplitAndMergeEditor::newMethod(QAction* act)
{
	QList<QAction*> actions = m_data->methods->actions();
	// uncheck other actions
	m_data->methods->blockSignals(true);
	for (int i = 0; i < actions.size(); ++i)
		actions[i]->setChecked(act == actions[i]);
	m_data->methods->blockSignals(false);
	m_data->method.setText(act->text());

	// set the processing method, and reset the processing to update the display
	if (m_data->proc) {
		m_data->proc->setMethod(act->text());
		setProcessing(m_data->proc);
	}
}

void VipSplitAndMergeEditor::receiveSelectionChanged(VipUniqueProcessingObjectEditor* ed)
{
	// a processing has been selected in on of the editors

	for (int i = 0; i < m_data->procEditors.size(); ++i) {
		// hide all processing editors except the one that is shown
		m_data->procEditors[i]->setVisible(m_data->procEditors[i] == ed);
	}

	if (VipProcessingListEditor* editor = qobject_cast<VipProcessingListEditor*>(sender())) {
		// unselect all rpocessing for all other VipProcessingListEditor
		for (int i = 0; i < m_data->editors.size(); ++i) {
			if (m_data->editors[i] != editor) {
				QListWidget* list = m_data->editors[i]->list();
				for (int j = 0; j < list->count(); ++j)
					list->item(j)->setSelected(false);
			}
		}
	}

	Q_EMIT selectionChanged(ed);
}

class VipExtractComponentEditor::PrivateData
{
public:
	QPointer<VipExtractComponent> extractComponent;
	VipComboBox components;
};

VipExtractComponentEditor::VipExtractComponentEditor()
{
	m_data = new PrivateData();
	m_data->components.setToolTip("Select the component to extract");
	m_data->components.setEditable(false);
	// m_data->components.QComboBox::setEditText("Invariant");
	m_data->components.setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_data->components.setMinimumWidth(100);
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&m_data->components);
	hlay->setContentsMargins(0, 0, 0, 0);
	setLayout(hlay);

	connect(&m_data->components, SIGNAL(currentTextChanged(const QString&)), this, SLOT(updateExtractComponent()));
	connect(&m_data->components, SIGNAL(openPopup()), this, SLOT(updateComponentChoice()));
}

VipExtractComponentEditor::~VipExtractComponentEditor()
{
	delete m_data;
}

void VipExtractComponentEditor::setExtractComponent(VipExtractComponent* extract)
{
	if (m_data->extractComponent) {
		disconnect(m_data->extractComponent, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(extractComponentChanged()));
	}

	m_data->extractComponent = extract;
	if (extract) {
		QString comp = m_data->extractComponent->propertyAt(0)->data().value<QString>();

		// TEST: remove Invariant
		// if (comp.isEmpty())
		//	comp = "Invariant";
		updateComponentChoice();
		m_data->components.blockSignals(true);
		if (m_data->components.findText(comp) < 0) {
			m_data->components.addItem(comp);
		}
		m_data->components.setCurrentText(comp);
		m_data->components.blockSignals(false);
		connect(extract, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(extractComponentChanged()));
	}
}

int VipExtractComponentEditor::componentCount() const
{
	return m_data->components.count();
}

QComboBox* VipExtractComponentEditor::choices() const
{
	return &m_data->components;
}

void VipExtractComponentEditor::updateComponentChoice()
{
	if (m_data->extractComponent) {
		m_data->components.blockSignals(true);

		QStringList components = m_data->extractComponent->supportedComponents();
		if (components != m_data->components.items()) {
			// update the components

			m_data->components.clear();
			m_data->components.addItems(components);
			// set the default component
			QString default_component = m_data->extractComponent->defaultComponent();
			if (default_component.isEmpty()) {
				m_data->components.setCurrentIndex(0);
			}
			else
				m_data->components.setCurrentText(m_data->extractComponent->defaultComponent());
		}

		QString comp = m_data->extractComponent->propertyAt(0)->data().value<QString>();
		// TEST: remove Invariant
		if (!components.size() && (comp.isEmpty() //|| comp == "Invariant"
					   )) {
			if (m_data->components.isVisible())
				m_data->components.hide();
		}
		else if (m_data->components.isHidden()) {
			m_data->components.show();
		}

		if (!comp.isEmpty()) {
			if (comp != m_data->components.currentText()) {
				m_data->components.setCurrentText(comp);
			}
		}
		else {
			comp = m_data->components.currentText();
			m_data->extractComponent->propertyAt(0)->setData(comp);
		}

		m_data->components.blockSignals(false);
	}
}

void VipExtractComponentEditor::updateExtractComponent()
{
	if (m_data->extractComponent) {
		int index = m_data->components.currentIndex();
		QStringList choices = m_data->extractComponent->supportedComponents();
		if (index < choices.size()) {
			// set the new component
			m_data->extractComponent->propertyAt(0)->setData(choices[index]);
			m_data->extractComponent->reload();
			Q_EMIT componentChanged(choices[index]);
		}
	}
}

void VipExtractComponentEditor::extractComponentChanged()
{
	QString comp = m_data->extractComponent->propertyAt(0)->data().value<QString>();
	m_data->components.blockSignals(true);
	m_data->components.setCurrentText(comp);
	m_data->components.blockSignals(false);
}

class VipConvertEditor::PrivateData
{
public:
	QPointer<VipConvert> convert;
	VipComboBox types;
};

static QMap<int, QPair<int, QString>>& __conversions()
{
	static QMap<int, QPair<int, QString>> map;
	if (map.isEmpty()) {
		map[0] = QPair<int, QString>(0, "No conversion");
		map[1] = QPair<int, QString>(QMetaType::Bool, "bool (1 byte)");
		map[2] = QPair<int, QString>(QMetaType::Char, "signed char (1 bytes)");
		map[3] = QPair<int, QString>(QMetaType::UChar, "unsigned char (1 bytes)");
		map[4] = QPair<int, QString>(QMetaType::Short, "signed short (2 bytes)");
		map[5] = QPair<int, QString>(QMetaType::UShort, "unsigned short (2 bytes)");
		map[6] = QPair<int, QString>(QMetaType::Int, "signed int (4 bytes)");
		map[7] = QPair<int, QString>(QMetaType::UInt, "unsigned int (4 bytes)");
		map[8] = QPair<int, QString>(QMetaType::LongLong, "signed long (8 bytes)");
		map[9] = QPair<int, QString>(QMetaType::ULongLong, "unsigned long (8 bytes)");
		map[10] = QPair<int, QString>(QMetaType::Float, "float (4 bytes)");
		map[11] = QPair<int, QString>(QMetaType::Double, "double (8 bytes)");
		map[12] = QPair<int, QString>(qMetaTypeId<complex_f>(), "complex float (8 bytes)");
		map[13] = QPair<int, QString>(qMetaTypeId<complex_d>(), "complex double (16 bytes)");
	}
	return map;
}

static int __indexForType(int type)
{
	int i = 0;
	for (QMap<int, QPair<int, QString>>::const_iterator it = __conversions().begin(); it != __conversions().end(); ++it, ++i)
		if (it.value().first == type)
			return i;
	return 0;
}

VipConvertEditor::VipConvertEditor()
{
	m_data = new PrivateData();
	m_data->types.setToolTip("Select the output type");
	m_data->types.setEditable(false);

	for (QMap<int, QPair<int, QString>>::const_iterator it = __conversions().begin(); it != __conversions().end(); ++it)
		m_data->types.addItem(it.value().second);

	m_data->types.QComboBox::setCurrentIndex(0);
	m_data->types.setSizeAdjustPolicy(QComboBox::AdjustToContents);
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&m_data->types);
	hlay->setContentsMargins(0, 0, 0, 0);
	setLayout(hlay);

	connect(&m_data->types, SIGNAL(currentTextChanged(const QString&)), this, SLOT(updateConversion()));
}

VipConvertEditor::~VipConvertEditor()
{
	delete m_data;
}

VipConvert* VipConvertEditor::convert() const
{
	return m_data->convert;
}

void VipConvertEditor::setConvert(VipConvert* tr)
{
	if (m_data->convert) {
		disconnect(m_data->convert, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(convertChanged()));
	}

	m_data->convert = tr;
	if (tr) {
		int type = m_data->convert->propertyAt(0)->data().value<int>();
		m_data->types.blockSignals(true);
		m_data->types.setCurrentIndex(__indexForType(type));
		m_data->types.blockSignals(false);

		connect(tr, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(conversionChanged()));
	}
}

QComboBox* VipConvertEditor::types() const
{
	return &m_data->types;
}

void VipConvertEditor::conversionChanged()
{
	if (m_data->convert) {
		int type = m_data->convert->propertyAt(0)->data().value<int>();
		m_data->types.blockSignals(true);
		m_data->types.setCurrentIndex(__indexForType(type));
		m_data->types.blockSignals(false);
	}
}

void VipConvertEditor::updateConversion()
{
	if (m_data->convert) {
		int index = m_data->types.currentIndex();
		int type = __conversions()[index].first;

		// set the new type
		m_data->convert->propertyAt(0)->setData(type);
		m_data->convert->reload();
		Q_EMIT conversionChanged(type);
	}
}

class VipDisplayImageEditor::PrivateData
{
public:
	VipExtractComponentEditor editor;
	QPointer<VipDisplayImage> display;
	QTimer updateTimer;
};

VipDisplayImageEditor::VipDisplayImageEditor()
  : QWidget()
{
	m_data = new PrivateData();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&m_data->editor);

	QHBoxLayout* th_lay = new QHBoxLayout();
	th_lay->setContentsMargins(0, 0, 0, 0);

	lay->addLayout(th_lay);
	setLayout(lay);

	m_data->editor.setToolTip("Select a component to display");
	m_data->editor.hide();
	this->setToolTip("Select a component to display");
	connect(&m_data->editor, SIGNAL(componentChanged(const QString&)), this, SLOT(updateDisplayImage()));
	connect(&m_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));

	m_data->updateTimer.setSingleShot(false);
	m_data->updateTimer.setInterval(500);
	m_data->updateTimer.start();
}

VipExtractComponentEditor* VipDisplayImageEditor::editor() const
{
	return &m_data->editor;
}

VipDisplayImageEditor::~VipDisplayImageEditor()
{
	m_data->updateTimer.stop();
	disconnect(&m_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));
	delete m_data;
}

void VipDisplayImageEditor::setDisplayImage(VipDisplayImage* d)
{
	m_data->display = d;
	if (d) {
		m_data->editor.setExtractComponent(d->extractComponent());
	}
}

VipDisplayImage* VipDisplayImageEditor::displayImage() const
{
	return m_data->display;
}

void VipDisplayImageEditor::updateDisplayImage()
{
	if (m_data->display) {
		VipAnyData any = m_data->display->inputAt(0)->data();
		m_data->display->inputAt(0)->setData(any);
	}
}

void VipDisplayImageEditor::updateExtractorVisibility()
{
	if (m_data->display)
		m_data->editor.setVisible(VipGenericExtractComponent::HasComponents(m_data->display->inputAt(0)->probe().value<VipNDArray>()));
}

class VipDisplayCurveEditor::PrivateData
{
public:
	VipExtractComponentEditor editor;
	QPointer<VipDisplayCurve> display;
	QTimer updateTimer;
};

VipDisplayCurveEditor::VipDisplayCurveEditor()
  : QWidget()
{
	m_data = new PrivateData();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&m_data->editor);
	setLayout(lay);

	m_data->editor.setToolTip("Select a component to display");
	m_data->editor.hide();
	this->setToolTip("Select a component to display");
	connect(&m_data->editor, SIGNAL(componentChanged(const QString&)), this, SLOT(updateDisplayCurve()));
	connect(&m_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));

	m_data->updateTimer.setSingleShot(false);
	m_data->updateTimer.setInterval(500);
	m_data->updateTimer.start();
}

VipDisplayCurveEditor::~VipDisplayCurveEditor()
{
	m_data->updateTimer.stop();
	disconnect(&m_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));
	delete m_data;
}

void VipDisplayCurveEditor::setDisplay(VipDisplayCurve* d)
{
	m_data->display = d;
	if (d) {
		m_data->editor.setExtractComponent(d->extractComponent());
	}
}

VipDisplayCurve* VipDisplayCurveEditor::display() const
{
	return m_data->display;
}

void VipDisplayCurveEditor::updateDisplayCurve()
{
	if (m_data->display) {
		VipAnyData any = m_data->display->inputAt(0)->data();
		m_data->display->inputAt(0)->setData(any);
	}
}

void VipDisplayCurveEditor::updateExtractorVisibility()
{
	if (m_data->display) {
		VipAnyData any = m_data->display->inputAt(0)->probe();
		if (any.data().userType() == qMetaTypeId<VipComplexPoint>() || any.data().userType() == qMetaTypeId<VipComplexPointVector>())
			m_data->editor.setVisible(true);
		else
			m_data->editor.setVisible(false);
	}
}

class VipSwitchEditor::PrivateData
{
public:
	VipComboBox box;
	QPointer<VipSwitch> sw;
};

VipSwitchEditor::VipSwitchEditor()
{
	m_data = new PrivateData();

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addWidget(&m_data->box);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	m_data->box.setToolTip("Select the input number");

	connect(&m_data->box, SIGNAL(currentIndexChanged(int)), this, SLOT(updateSwitch()));
	connect(&m_data->box, SIGNAL(openPopup()), this, SLOT(resetSwitch()));
}

VipSwitchEditor::~VipSwitchEditor()
{
	delete m_data;
}

void VipSwitchEditor::resetSwitch()
{
	setSwitch(m_data->sw);
}

void VipSwitchEditor::setSwitch(VipSwitch* sw)
{
	m_data->sw = sw;

	if (sw) {
		m_data->box.blockSignals(true);

		m_data->box.clear();
		for (int i = 0; i < sw->inputCount(); ++i) {
			VipAnyData any = sw->inputAt(i)->probe();
			if (any.name().isEmpty())
				m_data->box.addItem(QString::number(i));
			else
				m_data->box.addItem(any.name());
		}

		m_data->box.setCurrentIndex(sw->propertyAt(0)->data().value<int>());
		m_data->box.blockSignals(false);
	}
}

void VipSwitchEditor::updateSwitch()
{
	if (m_data->sw) {
		m_data->sw->propertyAt(0)->setData(m_data->box.currentIndex());
		m_data->sw->update(true);
	}
}

class VipClampEditor::PrivateData
{
public:
	QCheckBox clampMax;
	VipDoubleEdit max;
	QCheckBox clampMin;
	VipDoubleEdit min;
	QPointer<VipClamp> clamp;
};

VipClampEditor::VipClampEditor()
{
	m_data = new PrivateData();

	QGridLayout* lay = new QGridLayout();
	lay->addWidget(&m_data->clampMax, 0, 0);
	lay->addWidget(&m_data->max, 0, 1);
	lay->addWidget(&m_data->clampMin, 1, 0);
	lay->addWidget(&m_data->min, 1, 1);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	m_data->clampMax.setText("Clamp max value");
	m_data->max.setToolTip("Max value");
	m_data->clampMin.setText("Clamp min value");
	m_data->max.setToolTip("Min value");

	connect(&m_data->clampMax, SIGNAL(clicked(bool)), this, SLOT(updateClamp()));
	connect(&m_data->clampMin, SIGNAL(clicked(bool)), this, SLOT(updateClamp()));
	connect(&m_data->max, SIGNAL(valueChanged(double)), this, SLOT(updateClamp()));
	connect(&m_data->min, SIGNAL(valueChanged(double)), this, SLOT(updateClamp()));
}

VipClampEditor::~VipClampEditor()
{
	delete m_data;
}

void VipClampEditor::setClamp(VipClamp* c)
{
	m_data->clamp = c;
	if (c) {
		double min = c->propertyAt(0)->data().value<double>();
		double max = c->propertyAt(1)->data().value<double>();
		m_data->clampMax.blockSignals(true);
		m_data->clampMax.setChecked(max == max);
		m_data->clampMax.blockSignals(false);
		m_data->clampMin.blockSignals(true);
		m_data->clampMin.setChecked(min == min);
		m_data->clampMin.blockSignals(false);

		m_data->max.blockSignals(true);
		m_data->max.setValue(max);
		m_data->max.blockSignals(false);
		m_data->min.blockSignals(true);
		m_data->min.setValue(min);
		m_data->min.blockSignals(false);
	}
}

VipClamp* VipClampEditor::clamp() const
{
	return m_data->clamp;
}

void VipClampEditor::updateClamp()
{
	if (m_data->clamp) {
		if (m_data->clampMax.isChecked())
			m_data->clamp->propertyAt(1)->setData(m_data->max.value());
		else
			m_data->clamp->propertyAt(1)->setData(vipNan());

		if (m_data->clampMin.isChecked())
			m_data->clamp->propertyAt(0)->setData(m_data->min.value());
		else
			m_data->clamp->propertyAt(0)->setData(vipNan());

		m_data->clamp->reload();
	}
}

class VipTextFileReaderEditor::PrivateData
{
public:
	QPointer<VipTextFileReader> reader;
	QRadioButton image;
	QRadioButton xyxy_column;
	QRadioButton xyyy_column;
	QRadioButton xyxy_row;
	QRadioButton xyyy_row;
	QLabel label;
};

VipTextFileReaderEditor::VipTextFileReaderEditor()
{
	m_data = new PrivateData();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_data->label);
	lay->addWidget(&m_data->image);
	lay->addWidget(&m_data->xyxy_column);
	lay->addWidget(&m_data->xyyy_column);
	lay->addWidget(&m_data->xyxy_row);
	lay->addWidget(&m_data->xyyy_row);
	setLayout(lay);

	m_data->label.setText("<b>Interpret text file as:</b>");
	m_data->image.setText("An image sequence");
	m_data->xyxy_column.setText("Columns of X1 Y1...Xn Yn");
	m_data->xyyy_column.setText("Columns of X Y1...Yn");
	m_data->xyxy_row.setText("Rows of X1 Y1...Xn Yn");
	m_data->xyyy_row.setText("Rows of X Y1...Yn");
	m_data->image.setChecked(true);

	connect(&m_data->image, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&m_data->xyxy_column, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&m_data->xyyy_column, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&m_data->xyxy_row, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&m_data->xyyy_row, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
}

VipTextFileReaderEditor::~VipTextFileReaderEditor()
{
	delete m_data;
}

void VipTextFileReaderEditor::setTextFileReader(VipTextFileReader* reader)
{
	m_data->reader = reader;
	if (reader) {
		if (reader->type() == VipTextFileReader::Image)
			m_data->image.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYXYColumn)
			m_data->xyxy_column.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYYYColumn)
			m_data->xyyy_column.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYXYRow)
			m_data->xyxy_row.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYYYRow)
			m_data->xyyy_row.setChecked(true);
		else
			m_data->image.setChecked(true);
	}
}

void VipTextFileReaderEditor::updateTextFileReader()
{
	if (m_data->reader) {
		if (m_data->image.isChecked())
			m_data->reader->setType(VipTextFileReader::Image);
		if (m_data->xyxy_column.isChecked())
			m_data->reader->setType(VipTextFileReader::XYXYColumn);
		if (m_data->xyyy_column.isChecked())
			m_data->reader->setType(VipTextFileReader::XYYYColumn);
		if (m_data->xyxy_row.isChecked())
			m_data->reader->setType(VipTextFileReader::XYXYRow);
		if (m_data->xyyy_row.isChecked())
			m_data->reader->setType(VipTextFileReader::XYYYRow);
	}
}

class VipTextFileWriterEditor::PrivateData
{
public:
	QPointer<VipTextFileWriter> writer;
	QRadioButton stack;
	QRadioButton multi_file;
	QSpinBox digits;
	QLabel digits_label;
	QLabel label;
};

VipTextFileWriterEditor::VipTextFileWriterEditor()
{
	m_data = new PrivateData();

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&m_data->digits_label);
	hlay->addWidget(&m_data->digits);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_data->label);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&m_data->stack);
	lay->addWidget(&m_data->multi_file);
	lay->addLayout(hlay);
	setLayout(lay);

	m_data->label.setText("<b>File saving options</b><br><b>Warning:</b>These options are only useful for temporal sequences.");
	m_data->stack.setText("Stack the data in the same file");
	m_data->stack.setToolTip("For temporal sequences, all data (images, curves,...) will be saved in the same file with a blank line separator.");
	m_data->multi_file.setText("Create one file per data");
	m_data->multi_file.setToolTip("For temporal sequences, all data (images, curves,...) will be saved in separate files. All files will end with a unique number starting to 0.");
	m_data->digits.setValue(4);
	m_data->digits.setToolTip("Each file name will end by a number\nincremented for each new data.\nSet the number digits.");
	m_data->digits.hide();
	m_data->digits_label.hide();
	m_data->digits.setRange(1, 8);
	m_data->stack.setChecked(true);
	m_data->digits_label.setText("Digit number");

	connect(&m_data->stack, SIGNAL(clicked(bool)), this, SLOT(updateTextFileWriter()));
	connect(&m_data->multi_file, SIGNAL(clicked(bool)), this, SLOT(updateTextFileWriter()));
	connect(&m_data->digits, SIGNAL(valueChanged(int)), this, SLOT(updateTextFileWriter()));
}

VipTextFileWriterEditor::~VipTextFileWriterEditor()
{
	delete m_data;
}

void VipTextFileWriterEditor::setTextFileWriter(VipTextFileWriter* writer)
{
	m_data->writer = writer;
	if (writer) {
		if (writer->type() == VipTextFileWriter::MultipleFiles)
			m_data->multi_file.setChecked(true);
		else
			m_data->stack.setChecked(true);
		m_data->digits.setVisible(m_data->multi_file.isChecked());
		m_data->digits_label.setVisible(m_data->multi_file.isChecked());
	}
}

void VipTextFileWriterEditor::updateTextFileWriter()
{
	if (m_data->writer) {
		if (m_data->multi_file.isChecked())
			m_data->writer->setType(VipTextFileWriter::MultipleFiles);
		else
			m_data->writer->setType(VipTextFileWriter::StackData);
		m_data->digits.setVisible(m_data->multi_file.isChecked());
		m_data->digits_label.setVisible(m_data->multi_file.isChecked());
		m_data->writer->setDigitsNumber(m_data->digits.value());
	}
}

class VipImageWriterEditor::PrivateData
{
public:
	QPointer<VipImageWriter> writer;
	QRadioButton stack;
	QRadioButton multi_file;
	QSpinBox digits;
	QLabel label;
};

VipImageWriterEditor::VipImageWriterEditor()
{
	m_data = new PrivateData();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_data->label);
	lay->addWidget(&m_data->stack);
	lay->addWidget(&m_data->multi_file);
	lay->addWidget(&m_data->digits);
	setLayout(lay);

	m_data->label.setText("<b>File saving options</b>");
	m_data->stack.setText("Stack the images in the same file");
	m_data->multi_file.setText("Create one file per image");
	m_data->digits.setValue(4);
	m_data->digits.setToolTip("Each file name will end by a number\nincremented for each new image.\nSet the number digits.");
	m_data->digits.hide();
	m_data->digits.setRange(1, 8);
	m_data->stack.setChecked(true);

	connect(&m_data->stack, SIGNAL(clicked(bool)), this, SLOT(updateImageWriter()));
	connect(&m_data->multi_file, SIGNAL(clicked(bool)), this, SLOT(updateImageWriter()));
	connect(&m_data->digits, SIGNAL(valueChanged(int)), this, SLOT(updateImageWriter()));
}

VipImageWriterEditor::~VipImageWriterEditor()
{
	delete m_data;
}

void VipImageWriterEditor::setImageWriter(VipImageWriter* writer)
{
	m_data->writer = writer;
	if (writer) {
		if (writer->type() == VipImageWriter::MultipleImages)
			m_data->multi_file.setChecked(true);
		else
			m_data->stack.setChecked(true);
		m_data->digits.setVisible(m_data->multi_file.isChecked());
	}
}

void VipImageWriterEditor::updateImageWriter()
{
	if (m_data->writer) {
		if (m_data->multi_file.isChecked())
			m_data->writer->setType(VipImageWriter::MultipleImages);
		else
			m_data->writer->setType(VipImageWriter::StackImages);
		m_data->digits.setVisible(m_data->multi_file.isChecked());
		m_data->writer->setDigitsNumber(m_data->digits.value());
	}
}

class VipCSVWriterEditor::PrivateData
{
public:
	QLabel resampleText;
	QComboBox resample;

	QRadioButton useBounds;
	QRadioButton useFixValue;
	VipDoubleEdit fixValue;

	QLabel saveAsCSVText;
	QComboBox saveAsCSV;

	QPointer<VipCSVWriter> processing;
};
VipCSVWriterEditor::VipCSVWriterEditor()
{
	m_data = new PrivateData();

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&m_data->resampleText);
	hlay->addWidget(&m_data->resample);
	hlay->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* hlay2 = new QHBoxLayout();
	hlay2->addWidget(&m_data->useFixValue);
	hlay2->addWidget(&m_data->fixValue);
	hlay2->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(10, 0, 0, 0);
	vlay->addWidget(&m_data->useBounds);
	vlay->addLayout(hlay2);

	QHBoxLayout* hlay3 = new QHBoxLayout();
	hlay3->addWidget(&m_data->saveAsCSVText);
	hlay3->addWidget(&m_data->saveAsCSV);
	hlay3->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->addLayout(hlay);
	layout->addLayout(vlay);
	layout->addWidget(VipLineWidget::createHLine());
	layout->addLayout(hlay3);
	setLayout(layout);

	m_data->resampleText.setText("Resample using input signals");
	m_data->resampleText.setToolTip("When saving multiple signals, they will be resampled to contain the same number of points with the same X values.\n"
					"The time interval used for resampling can be computed either with the union or intersection of input signals.");
	m_data->resample.addItem("union");
	m_data->resample.addItem("intersection");
	m_data->resample.setCurrentText("intersection");
	m_data->resample.setToolTip(m_data->resampleText.toolTip());

	m_data->useBounds.setChecked(true);
	m_data->useBounds.setText("Use closest value");
	m_data->useBounds.setToolTip("For 'union' only, the resampling algorithm might need to create new values outside the signal bounds.\n"
				     "Select this option to always pick the closest  available value.");
	m_data->useFixValue.setText("Use fixed value");
	m_data->useFixValue.setToolTip("For 'union' only, the resampling algorithm might need to create new values outside the signal bounds.\n"
				       "Select this option to set the new points to a fixed value.");
	m_data->fixValue.setValue(0);

	m_data->saveAsCSVText.setText("Select file format");
	m_data->saveAsCSVText.setToolTip("Select 'CSV' to create a real CSV file with the signals units.\n"
					 "Select 'TEXT' to save the raw signals in ascii format, without additional metadata.");
	m_data->saveAsCSV.addItem("CSV");
	m_data->saveAsCSV.addItem("TEXT");
	m_data->saveAsCSV.setToolTip(m_data->saveAsCSVText.toolTip());

	connect(&m_data->resample, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
	connect(&m_data->resample, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCSVWriter()));
	connect(&m_data->useBounds, SIGNAL(clicked(bool)), this, SLOT(updateCSVWriter()));
	connect(&m_data->useFixValue, SIGNAL(clicked(bool)), this, SLOT(updateCSVWriter()));
	connect(&m_data->fixValue, SIGNAL(valueChanged(double)), this, SLOT(updateCSVWriter()));
	connect(&m_data->saveAsCSV, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCSVWriter()));

	updateWidgets();
}

VipCSVWriterEditor::~VipCSVWriterEditor()
{
	delete m_data;
}

void VipCSVWriterEditor::updateWidgets()
{
	if (m_data->resample.currentText() == "union") {
		m_data->useBounds.setEnabled(true);
		m_data->useFixValue.setEnabled(true);
		m_data->fixValue.setEnabled(true);
	}
	else {
		m_data->useBounds.setEnabled(false);
		m_data->useFixValue.setEnabled(false);
		m_data->fixValue.setEnabled(false);
	}
}

void VipCSVWriterEditor::setCSVWriter(VipCSVWriter* w)
{
	m_data->processing = w;
	if (w) {
		if (w->resampleMode() & ResampleIntersection)
			m_data->resample.setCurrentText("intersection");
		else
			m_data->resample.setCurrentText("union");

		m_data->fixValue.setValue(w->paddValue());
		m_data->useFixValue.setChecked(w->resampleMode() & ResamplePadd0);
		if (w->writeTextFile())
			m_data->saveAsCSV.setCurrentText("TEXT");
		else
			m_data->saveAsCSV.setCurrentText("CSV");
	}
}

void VipCSVWriterEditor::updateCSVWriter()
{
	if (m_data->processing) {
		int r = ResampleInterpolation;
		if (m_data->resample.currentText() == "intersection")
			r |= ResampleIntersection;
		else
			r |= ResampleUnion;
		if (m_data->useFixValue.isChecked()) {
			r |= ResamplePadd0;
			m_data->processing->setPaddValue(m_data->fixValue.value());
		}
		m_data->processing->setResampleMode(r);
		m_data->processing->setWriteTextFile(m_data->saveAsCSV.currentText() == "TEXT");
	}
}

class VipDirectoryReaderEditor::PrivateData
{
public:
	QPointer<VipDirectoryReader> reader;
	QLineEdit file_suffixes;
	QCheckBox recursive;
	QSpinBox file_count;
	QSpinBox file_start;
	QCheckBox alphabetical_order;

	QRadioButton independent_data;
	QRadioButton sequence_of_data;

	QCheckBox fixed_size;
	QSpinBox width;
	QSpinBox height;
	QCheckBox smooth;

	QMap<const QMetaObject*, VipUniqueProcessingObjectEditor*> editors;
	QPushButton applyToAllDevices;

	/// VipDirectoryReaderEditor has 2 different ways to edit a VipDirectoryReader.
	/// If the VipDirectoryReader is closed, it will display widgets to select the files in the input directory (suffixes, order,...).
	/// If the VipDirectoryReader is opened, it will display a VipUniqueProcessingObjectEditor for each type of opened VipIODevice.

	QWidget* closedOptions;
	QWidget* openedOptions;
};

VipDirectoryReaderEditor::VipDirectoryReaderEditor()
{
	m_data = new PrivateData();

	// create the options for when the device is closed

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_data->file_suffixes);
	lay->addWidget(&m_data->recursive);

	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("File count"));
		hlay->addWidget(&m_data->file_count);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}

	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("File start"));
		hlay->addWidget(&m_data->file_start);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}

	lay->addWidget(&m_data->alphabetical_order);
	lay->addWidget(&m_data->independent_data);
	lay->addWidget(&m_data->sequence_of_data);

	QGroupBox* images = new QGroupBox("Video file options");
	images->setFlat(true);
	lay->addWidget(images);

	lay->addWidget(&m_data->fixed_size);
	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("Width"));
		hlay->addWidget(&m_data->width);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}
	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("Height"));
		hlay->addWidget(&m_data->height);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}
	lay->addWidget(&m_data->smooth);
	m_data->closedOptions = new QWidget();
	m_data->closedOptions->setLayout(lay);

	m_data->file_suffixes.setToolTip("Supported extensions with comma separators");
	m_data->file_suffixes.setPlaceholderText("Supported extensions");

	m_data->recursive.setText("Read subdirectories");

	m_data->file_count.setRange(-1, 1000000);
	m_data->file_count.setValue(-1);
	m_data->file_count.setToolTip("Set the maximum file number\n(-1 means all files in the directory)");

	m_data->file_start.setRange(0, 1000000);
	m_data->file_start.setValue(0);
	m_data->file_start.setToolTip("Set start file index\n(all files before the index are skipped)");

	m_data->alphabetical_order.setText("Sort files alphabetically");
	m_data->alphabetical_order.setChecked(true);

	m_data->independent_data.setText("Read as independent data files");
	m_data->sequence_of_data.setText("Read as a sequence of data files");
	m_data->independent_data.setChecked(true);

	m_data->fixed_size.setText("Use a fixed size");
	m_data->fixed_size.setChecked(false);
	m_data->fixed_size.setToolTip("All images are resized with given size");

	m_data->width.setRange(2, 5000);
	m_data->width.setValue(320);
	m_data->width.setToolTip("Image width");
	m_data->width.setEnabled(false);

	m_data->height.setRange(2, 5000);
	m_data->height.setValue(320);
	m_data->height.setToolTip("Image width");
	m_data->height.setEnabled(false);

	m_data->smooth.setText("Smooth resize");
	m_data->smooth.setChecked(false);
	m_data->smooth.setEnabled(false);

	connect(&m_data->fixed_size, SIGNAL(clicked(bool)), &m_data->width, SLOT(setEnabled(bool)));
	connect(&m_data->fixed_size, SIGNAL(clicked(bool)), &m_data->height, SLOT(setEnabled(bool)));
	connect(&m_data->fixed_size, SIGNAL(clicked(bool)), &m_data->smooth, SLOT(setEnabled(bool)));

	m_data->file_suffixes.setFocus();

	// create the options for when the device is opened
	m_data->openedOptions = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(&m_data->applyToAllDevices);
	m_data->openedOptions->setLayout(vlay);

	m_data->applyToAllDevices.setText("Apply to all devices");
	connect(&m_data->applyToAllDevices, SIGNAL(clicked(bool)), this, SLOT(apply()));

	// create the final layout
	QVBoxLayout* final_lay = new QVBoxLayout();
	final_lay->addWidget(m_data->closedOptions);
	final_lay->addWidget(m_data->openedOptions);
	setLayout(final_lay);
}

VipDirectoryReaderEditor::~VipDirectoryReaderEditor()
{
	delete m_data;
}

void VipDirectoryReaderEditor::setDirectoryReader(VipDirectoryReader* reader)
{
	m_data->reader = reader;
	if (reader) {
		m_data->closedOptions->setVisible(!reader->isOpen());
		m_data->openedOptions->setVisible(reader->isOpen());

		// options when the device is closed
		if (!reader->isOpen()) {
			m_data->file_suffixes.setText(reader->supportedSuffixes().join(","));
			m_data->recursive.setChecked(reader->recursive());
			m_data->file_count.setValue(reader->fileCount());
			m_data->file_start.setValue(reader->fileStart());
			m_data->alphabetical_order.setChecked(reader->alphabeticalOrder());
			m_data->sequence_of_data.setChecked(reader->type() == VipDirectoryReader::SequenceOfData);
			m_data->fixed_size.setChecked(reader->fixedSize() != QSize());
			if (m_data->fixed_size.isChecked()) {
				m_data->width.setValue(reader->fixedSize().width());
				m_data->height.setValue(reader->fixedSize().height());
			}
			m_data->smooth.setChecked(reader->smoothResize());
		}
		// options when the device is already opened
		else {
			// remove the previous editors
			while (m_data->editors.size())
				if (m_data->editors.begin().value())
					delete m_data->editors.begin().value();
			m_data->editors.clear();

			// compute all device types and add the editors
			for (int i = 0; i < reader->deviceCount(); ++i) {
				VipIODevice* dev = reader->deviceAt(i);
				const QMetaObject* meta = dev->metaObject();
				if (m_data->editors.find(meta) == m_data->editors.end()) {
					// create the editor
					VipUniqueProcessingObjectEditor* editor = new VipUniqueProcessingObjectEditor();
					if (editor->setProcessingObject(dev)) {
						// add the editor to the layout if setProcessingObject is successfull
						m_data->editors[meta] = editor;
						static_cast<QVBoxLayout*>(m_data->openedOptions->layout())->insertWidget(0, editor);
					}
					else {
						// do not add the editor to the layout, but add the meta object to editors to avoid this meta object for next devices
						delete editor;
						m_data->editors[meta] = nullptr;
					}
				}
			}
		}
	}
}

#include <qmessagebox.h>
void VipDirectoryReaderEditor::apply()
{
	if (!m_data->reader)
		return;

	VipDirectoryReader* r = m_data->reader;

	if (!r->isOpen()) {
		// set the options for closed VipDirectoryReader
		r->setSupportedSuffixes(m_data->file_suffixes.text());
		r->setRecursive(m_data->recursive.isChecked());
		r->setFileCount(m_data->file_count.value());
		r->setFileStart(m_data->file_start.value());
		r->setAlphabeticalOrder(m_data->alphabetical_order.isChecked());
		r->setType(m_data->sequence_of_data.isChecked() ? VipDirectoryReader::SequenceOfData : VipDirectoryReader::IndependentData);
		if (m_data->fixed_size.isChecked())
			r->setFixedSize(QSize(m_data->width.value(), m_data->height.value()));
		r->setSmoothResize(m_data->smooth.isChecked());

		// now, for each found extension, set the template
		QStringList suffixes = r->suffixes();
		for (int i = 0; i < suffixes.size(); ++i) {
			VipPath name = "test." + suffixes[i];
			name.setMapFileSystem(r->mapFileSystem());
			VipIODevice* dev = VipCreateDevice::create(name);
			r->setSuffixTemplate(suffixes[i], dev);
		}
	}
	else {
		// import the options to all devices in this VipDirectoryReader
		VipProgress progress;
		progress.setText("Apply parameters...");
		progress.setCancelable(true);
		progress.setRange(0, r->deviceCount());

		for (int i = 0; i < r->deviceCount(); ++i) {
			progress.setValue(i);
			if (progress.canceled())
				break;

			VipIODevice* dev = r->deviceAt(i);
			const QMetaObject* meta = dev->metaObject();
			if (m_data->editors.find(meta) != m_data->editors.end()) {
				if (VipUniqueProcessingObjectEditor* editor = m_data->editors[meta]) {
					editor->processingObject()->copyParameters(dev);
				}
			}
		}

		// reload the device
		r->reload();
	}
}

class VipOperationBetweenPlayersEditor::PrivateData
{
public:
	QPointer<VipOperationBetweenPlayers> processing;
	VipOtherPlayerDataEditor editor;
	QComboBox operation;
};

VipOperationBetweenPlayersEditor::VipOperationBetweenPlayersEditor()
  : QWidget()
{
	m_data = new PrivateData();

	QGridLayout* lay = new QGridLayout();

	lay->addWidget(new QLabel("Operator:"), 0, 0);
	lay->addWidget(&m_data->operation, 0, 1);
	lay->addWidget(&m_data->editor, 1, 0, 1, 2);

	m_data->operation.setToolTip("Select the operation to perform (addition, subtraction, multiplication, division, or binary operation)");
	m_data->operation.addItems(QStringList() << "+"
						 << "-"
						 << "*"
						 << "/"
						 << "&"
						 << "|"
						 << "^");
	m_data->operation.setCurrentIndex(1);

	setLayout(lay);

	connect(&m_data->editor, SIGNAL(valueChanged(const QVariant&)), this, SLOT(apply()));
	connect(&m_data->operation, SIGNAL(currentIndexChanged(int)), this, SLOT(apply()));
}

VipOperationBetweenPlayersEditor::~VipOperationBetweenPlayersEditor()
{
	delete m_data;
}

void VipOperationBetweenPlayersEditor::setProcessing(VipOperationBetweenPlayers* proc)
{
	if (proc != m_data->processing) {
		m_data->processing = proc;

		if (proc) {
			m_data->editor.blockSignals(true);
			m_data->editor.setValue(proc->propertyAt(1)->value<VipOtherPlayerData>());
			m_data->editor.blockSignals(false);

			m_data->operation.blockSignals(true);
			m_data->operation.setCurrentText(proc->propertyName("Operator")->value<QString>());
			m_data->operation.blockSignals(false);
		}
	}
}

void VipOperationBetweenPlayersEditor::apply()
{
	if (VipOperationBetweenPlayers* proc = m_data->processing) {
		proc->propertyAt(0)->setData(m_data->operation.currentText());
		proc->propertyAt(1)->setData(m_data->editor.value());
		proc->wait();
		proc->reload();
	}
}

VipCropEditor::VipCropEditor()
  : QWidget()
{
	start.setPlaceholderText("Start point");
	end.setPlaceholderText("End point");
	shape.setRange(0, 1000);
	shape.setValue(0);
	start.setToolTip("Top left corner of the crop with a comma separator\nExample: '10' or '10, 20' or '10, 20, 15',...\nFor 2D->3D arrays, the first component is the height");
	end.setToolTip("Bottom right corner of the crop with a comma separator\nExample: '10' or '10, 20' or '10, 20, 15',...\nFor 2D->3D arrays, the first component is the height");
	shape.setToolTip("Use a shape to define the crop(use the shape id)");
	apply.setText("Apply");
	apply.setAutoRaise(true);
	apply.setToolTip("Apply the crop on given shape id");
	apply.setIcon(vipIcon("apply.png"));
	connect(&start, SIGNAL(returnPressed()), this, SLOT(updateCrop()));
	connect(&end, SIGNAL(returnPressed()), this, SLOT(updateCrop()));
	connect(&apply, SIGNAL(clicked(bool)), this, SLOT(updateCrop()));

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(&start);
	vlay->addWidget(&end);
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(new QLabel("VipImageCrop on shape"));
	hlay->addWidget(&shape);
	hlay->addWidget(&apply);
	vlay->addLayout(hlay);
	setLayout(vlay);
}

void VipCropEditor::setCrop(VipImageCrop* th)
{
	if (th) {
		m_crop = th;
		start.blockSignals(true);
		end.blockSignals(true);
		shape.blockSignals(true);
		start.setText(th->propertyName("Top_left")->value<QString>());
		end.setText(th->propertyName("Bottom_right")->value<QString>());
		start.blockSignals(false);
		end.blockSignals(false);
		shape.blockSignals(false);
	}
}

void VipCropEditor::updateCrop()
{
	if (m_crop) {
		if (sender() == &apply) {
			int shape_id = shape.value();
			// try to use the shape
			if (shape_id > 0) {
				VipShape sh = m_crop->sceneModel().find("ROI", shape_id);
				if (!sh.isNull()) {
					VipNDArray ar = m_crop->inputAt(0)->data().value<VipNDArray>();
					QRect r = sh.boundingRect().toRect() & QRect(0, 0, ar.shape(1), ar.shape(0));

					start.blockSignals(true);
					end.blockSignals(true);
					start.setText(QString::number(r.top()) + ", " + QString::number(r.left()));
					end.setText(QString::number(r.bottom()) + ", " + QString::number(r.right()));
					start.blockSignals(false);
					end.blockSignals(false);
				}
			}
		}
		m_crop->propertyName("Top_left")->setData(start.text());
		m_crop->propertyName("Bottom_right")->setData(end.text());
		m_crop->reload();
	}
}

VipResizeEditor::VipResizeEditor()
  : QWidget()
{
	shape.setPlaceholderText("New shape");
	interp.addItem("No interpolation");
	interp.addItem("Linear interpolation");
	interp.addItem("Cubic interpolation");
	interp.setCurrentIndex(0);

	shape.setToolTip("New shape values with a comma separator\nExample: '10' or '10, 20' or '10, 20, 15',...\nFor 2D->3D arrays, the first component is the height");
	interp.setToolTip("Resizing interpolation");
	connect(&shape, SIGNAL(returnPressed()), this, SLOT(updateResize()));
	connect(&interp, SIGNAL(currentIndexChanged(int)), this, SLOT(updateResize()));

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(&shape);
	vlay->addWidget(&interp);
	setLayout(vlay);
}

void VipResizeEditor::setResize(VipResize* th)
{
	if (th) {
		m_resize = th;
		shape.blockSignals(true);
		interp.blockSignals(true);
		shape.setText(th->propertyName("New_size")->value<QString>());
		interp.setCurrentIndex(th->propertyName("Interpolation")->value<int>());
		shape.blockSignals(false);
		interp.blockSignals(false);
	}
}

void VipResizeEditor::updateResize()
{
	if (m_resize) {
		m_resize->propertyName("New_size")->setData(shape.text());
		m_resize->propertyName("Interpolation")->setData(interp.currentIndex());
		m_resize->reload();
	}
}

#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qlistwidget.h>

#include "VipStandardWidgets.h"

class TrListWidgetItem : public QListWidgetItem
{
public:
	Transform::TrType type;
	VipDoubleEdit *x, *y;

	TrListWidgetItem(QListWidget* parent, Transform::TrType type)
	  : QListWidgetItem(parent)
	  , type(type)
	  , x(nullptr)
	  , y(nullptr)
	{

		if (type == Transform::Translate) {
			x = new VipDoubleEdit();
			x->setValue(0);
			y = new VipDoubleEdit();
			y->setValue(0);
			QHBoxLayout* l = new QHBoxLayout();
			l->setContentsMargins(0, 0, 0, 0);
			l->addWidget(new QLabel("Translate: "));
			l->addWidget(x);
			l->addWidget(new QLabel(", "));
			l->addWidget(y);
			QWidget* w = new QWidget();
			w->setLayout(l);
			parent->setItemWidget(this, w);
		}
		else if (type == Transform::Rotate) {
			x = new VipDoubleEdit();
			x->setValue(0);
			QHBoxLayout* l = new QHBoxLayout();
			l->setContentsMargins(0, 0, 0, 0);
			l->addWidget(new QLabel("Rotate: "));
			l->addWidget(x);
			QWidget* w = new QWidget();
			w->setLayout(l);
			parent->setItemWidget(this, w);
		}
		else if (type == Transform::Scale) {
			x = new VipDoubleEdit();
			x->setValue(1);
			y = new VipDoubleEdit();
			y->setValue(1);
			QHBoxLayout* l = new QHBoxLayout();
			l->setContentsMargins(0, 0, 0, 0);
			l->addWidget(new QLabel("Scale: "));
			l->addWidget(x);
			l->addWidget(new QLabel(", "));
			l->addWidget(y);
			QWidget* w = new QWidget();
			w->setLayout(l);
			parent->setItemWidget(this, w);
		}
		else if (type == Transform::Shear) {
			x = new VipDoubleEdit();
			x->setValue(0);
			y = new VipDoubleEdit();
			y->setValue(0);
			QHBoxLayout* l = new QHBoxLayout();
			l->setContentsMargins(0, 0, 0, 0);
			l->addWidget(new QLabel("Shear: "));
			l->addWidget(x);
			l->addWidget(new QLabel(", "));
			l->addWidget(y);
			QWidget* w = new QWidget();
			w->setLayout(l);
			parent->setItemWidget(this, w);
		}

		QList<QLabel*> labels = parent->itemWidget(this)->findChildren<QLabel*>();
		for (int i = 0; i < labels.size(); ++i)
			labels[i]->setAttribute(Qt::WA_TransparentForMouseEvents, true);

		QObject::connect(x, SIGNAL(valueChanged(double)), parent->parentWidget(), SLOT(updateProcessing()));
		if (y)
			QObject::connect(y, SIGNAL(valueChanged(double)), parent->parentWidget(), SLOT(updateProcessing()));
	}
};

class TrListWidget : public QListWidget
{
	VipGenericImageTransformEditor* editor;

public:
	TrListWidget(VipGenericImageTransformEditor* ed)
	  : QListWidget()
	  , editor(ed)
	{
	}

protected:
	void mousePressEvent(QMouseEvent* evt)
	{
		QListWidget::mousePressEvent(evt);
		if (evt->button() == Qt::RightButton) {
			QMenu menu;
			connect(menu.addAction("Add translation"), SIGNAL(triggered(bool)), editor, SLOT(addTranslation()));
			connect(menu.addAction("Add scaling"), SIGNAL(triggered(bool)), editor, SLOT(addScaling()));
			connect(menu.addAction("Add rotation"), SIGNAL(triggered(bool)), editor, SLOT(addRotation()));
			connect(menu.addAction("Add shear"), SIGNAL(triggered(bool)), editor, SLOT(addShear()));
			menu.addSeparator();
			connect(menu.addAction("Remove selection"), SIGNAL(triggered(bool)), editor, SLOT(removeSelectedTransform()));
			menu.exec(evt->globalPos());
		}
	}
	virtual void keyPressEvent(QKeyEvent* evt)
	{
		if (evt->key() == Qt::Key_Delete) {
			qDeleteAll(this->selectedItems());
			editor->updateProcessing();
			editor->recomputeSize();
		}
		else if (evt->key() == Qt::Key_A && (evt->modifiers() & Qt::ControlModifier)) {
			for (int i = 0; i < count(); ++i)
				item(i)->setSelected(true);
		}
	}

	/*virtual void dragEnterEvent(QDragEnterEvent * evt)
	{
		//if (evt->mimeData()->hasFormat("transform/transform-list"))
			evt->acceptProposedAction();
		//else
		//	return QListWidget::dragEnterEvent(evt);
	}
	*/
	virtual void dropEvent(QDropEvent* evt)
	{
		QListWidget::dropEvent(evt);
		/*
		//if (evt->mimeData()->hasFormat("transform/transform-list"))
		{
			//find the right insertion position
			int insertPos = this->count();
			QListWidgetItem * item = itemAt(evt->pos());
			if (item)
			{
				QRect rect = this->visualItemRect(item);
				insertPos = this->indexFromItem(item).row();
				if (evt->pos().y() > rect.center().y())
					insertPos++;
			}

			//insert the items
			QByteArray data(evt->mimeData()->data("transform/transform-list"));
			QDataStream str(data);
			TransformList lst;
			str >> lst;
			for (int i = 0; i < lst.size(); ++i)
			{
				TrListWidgetItem * it = new TrListWidgetItem(this, lst[i].type);
				it->x->setValue(lst[i].x);
				if (it->y)
					it->y->setValue(lst[i].y);

				this->insertItem(insertPos, it);
				insertPos++;
			}
		}
		*/

		this->editor->updateProcessing();
	}
};

class VipGenericImageTransformEditor::PrivateData
{
public:
	QCheckBox interp;
	QCheckBox size;
	QLineEdit back;
	TrListWidget* trs;
	QPointer<VipGenericImageTransform> proc;
};

VipGenericImageTransformEditor::VipGenericImageTransformEditor()
  : QWidget()
{
	m_data = new PrivateData();
	m_data->trs = new TrListWidget(this);
	QVBoxLayout* l = new QVBoxLayout();
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(&m_data->interp);
	l->addWidget(&m_data->size);
	l->addWidget(&m_data->back);
	l->addWidget(m_data->trs);
	setLayout(l);

	m_data->interp.setText("Linear interpolation");
	m_data->interp.setToolTip("Apply a linear interpolation to the output image");
	m_data->size.setText("Output size fit the transform size");
	m_data->size.setToolTip("If checked, the output image size will be computed based on the transformation in order to contain the whole image.\n"
				"Otherwise, the output image size is the same as the input one.");
	m_data->back.setToolTip("Background value.\nFor numerical image, just enter an integer or floating point value.\n"
				"For complex image, enter a complex value on the form '(x+yj)'.\n"
				"For color image, enter a ARGB value on the form '[A,R,G,B]'.\n"
				"Press ENTER to validate.");
	m_data->back.setText("0");
	m_data->trs->setToolTip("Consecutive image transforms.\n"
				"Right click to add or remove a tranform.");
	// m_data->trs.installEventFilter(this);
	m_data->trs->setDragDropMode(QAbstractItemView::InternalMove);
	m_data->trs->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_data->trs->setDragDropOverwriteMode(false);
	m_data->trs->setDefaultDropAction(Qt::TargetMoveAction);
	m_data->trs->setViewMode(QListView::ListMode);

	connect(&m_data->interp, SIGNAL(clicked(bool)), this, SLOT(updateProcessing()));
	connect(&m_data->size, SIGNAL(clicked(bool)), this, SLOT(updateProcessing()));
	connect(&m_data->back, SIGNAL(returnPressed()), this, SLOT(updateProcessing()));
}
VipGenericImageTransformEditor::~VipGenericImageTransformEditor()
{
	// m_data->trs.removeEventFilter(this);
	delete m_data;
}

void VipGenericImageTransformEditor::setProcessing(VipGenericImageTransform* p)
{
	if (p != m_data->proc) {
		m_data->proc = p;
		updateWidget();
	}
}
VipGenericImageTransform* VipGenericImageTransformEditor::processing() const
{
	return m_data->proc;
}

void VipGenericImageTransformEditor::updateProcessing()
{
	if (!m_data->proc)
		return;

	// build transform
	TransformList trs;
	for (int i = 0; i < m_data->trs->count(); ++i) {
		TrListWidgetItem* it = static_cast<TrListWidgetItem*>(m_data->trs->item(i));
		trs << Transform(it->type, it->x->value(), it->y ? it->y->value() : 0);
	}

	m_data->proc->propertyAt(0)->setData(QVariant::fromValue(trs));
	m_data->proc->propertyAt(1)->setData(m_data->interp.isChecked() ? (int)Vip::LinearInterpolation : (int)Vip::NoInterpolation);
	m_data->proc->propertyAt(2)->setData(m_data->size.isChecked() ? (int)Vip::TransformBoundingRect : (int)Vip::SrcSize);

	QString back = m_data->back.text();
	QVariant value;
	QTextStream str(back.toLatin1());
	double d;
	VipRGB rgb;
	complex_d c;
	if ((str >> rgb).status() == QTextStream::Ok)
		value = QVariant::fromValue(rgb);
	str.resetStatus();
	str.seek(0);
	if (value.userType() == 0 && (str >> c).status() == QTextStream::Ok)
		value = QVariant::fromValue(c);
	str.resetStatus();
	str.seek(0);
	if (value.userType() == 0 && (str >> d).status() == QTextStream::Ok)
		value = QVariant::fromValue(d);
	if (value.userType() == 0)
		m_data->back.setStyleSheet("QLineEdit {border:1px solid red;}");
	else {
		m_data->back.setStyleSheet("");
		m_data->proc->propertyAt(3)->setData(value);
	}

	m_data->proc->reload();
}

void VipGenericImageTransformEditor::updateWidget()
{
	const TransformList trs = m_data->proc->propertyAt(0)->value<TransformList>();
	int interp = m_data->proc->propertyAt(1)->value<int>();
	int size = m_data->proc->propertyAt(2)->value<int>();
	QVariant back = m_data->proc->propertyAt(3)->value<QVariant>();

	m_data->interp.setChecked(interp != Vip::NoInterpolation);
	m_data->size.setChecked(size == Vip::TransformBoundingRect);
	m_data->back.setText(back.toString());
	m_data->trs->clear();

	for (int i = 0; i < trs.size(); ++i) {
		TrListWidgetItem* it = new TrListWidgetItem(m_data->trs, trs[i].type);
		it->x->setValue(trs[i].x);
		if (it->y)
			it->y->setValue(trs[i].y);
	}

	recomputeSize();
}

void VipGenericImageTransformEditor::recomputeSize()
{
	m_data->trs->setMaximumHeight(m_data->trs->count() * 30 + 30);
	VipUniqueProcessingObjectEditor::geometryChanged(this);
}

void VipGenericImageTransformEditor::addTranslation()
{
	addTransform(Transform::Translate);
}
void VipGenericImageTransformEditor::addScaling()
{
	addTransform(Transform::Scale);
}
void VipGenericImageTransformEditor::addRotation()
{
	addTransform(Transform::Rotate);
}
void VipGenericImageTransformEditor::addShear()
{
	addTransform(Transform::Shear);
}
void VipGenericImageTransformEditor::addTransform(Transform::TrType type)
{
	/*TrListWidgetItem * it =*/new TrListWidgetItem(m_data->trs, type);
	recomputeSize();
}
void VipGenericImageTransformEditor::removeSelectedTransform()
{
	QList<QListWidgetItem*> items = m_data->trs->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		delete items[i];
	}
	updateProcessing();
}

/**
A VipPlotAreaFilter used to draw warping points
*/
class DrawWarpingPoints : public VipPlotAreaFilter
{
public:
	QPointF begin;
	QPointF end;
	QCursor cursor;
	VipQuiverPath quiver;
	VipWarpingEditor* warping;

	DrawWarpingPoints(VipAbstractPlotArea* area, VipWarpingEditor* parent);
	~DrawWarpingPoints();
	virtual QRectF boundingRect() const;
	virtual QPainterPath shape() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool sceneEvent(QEvent* event);

	/**
	Filter QApplication event to detect mouse event.
	If a mouse press event is detected outside the QGraphicsView of the VipVideoPlayer, this filter is automatically destroyed.
	*/
	virtual bool eventFilter(QObject* watched, QEvent* event);

private:
};

typedef QVector<QPair<QPoint, QPoint>> DeformationField;
Q_DECLARE_METATYPE(DeformationField)

class PlotWarpingPoints : public VipPlotItemDataType<DeformationField>
{
public:
	PlotWarpingPoints(const VipText& title = VipText());
	virtual ~PlotWarpingPoints();

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;

	virtual QList<VipText> legendNames() const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;

	/**Set/get the VipQuiverPath used to draw quivers*/
	VipQuiverPath& quiverPath() const;

	virtual void setPen(const QPen& p) { quiverPath().setPen(p); }
	virtual QPen pen() const { return quiverPath().pen(); }

	virtual void setBrush(const QBrush& b)
	{
		quiverPath().setExtremityBrush(VipQuiverPath::Start, b);
		quiverPath().setExtremityBrush(VipQuiverPath::End, b);
	}
	virtual QBrush brush() const { return quiverPath().extremetyBrush(VipQuiverPath::Start); }

	/**Set/get the VipPlotMarker used to draw fixed points*/
	VipSymbol& symbol() const;

private:
	class PrivateData;
	PrivateData* m_data;
};

DrawWarpingPoints::DrawWarpingPoints(VipAbstractPlotArea* area, VipWarpingEditor* p)
  : warping(p)
{
	this->setParent(p);
	qApp->installEventFilter(this);
	area->installFilter(this);
	cursor = area->cursor();
	area->setCursor(Qt::CrossCursor);

	quiver.setStyle(VipQuiverPath::EndArrow);
	quiver.setPen(QColor(Qt::white));
	quiver.setLength(VipQuiverPath::End, 8);
	quiver.setExtremityBrush(VipQuiverPath::End, QBrush(Qt::white));
	quiver.setExtremityPen(VipQuiverPath::End, QColor(Qt::white));
}

DrawWarpingPoints::~DrawWarpingPoints()
{
	qApp->removeEventFilter(this);
	if (area())
		area()->setCursor(cursor);
}

QRectF DrawWarpingPoints::boundingRect() const
{
	return QRectF(area()->scaleToPosition(begin), area()->scaleToPosition(end));
}

QPainterPath DrawWarpingPoints::shape() const
{
	QPainterPath path;
	path.addRect(boundingRect());
	return path;
}

void DrawWarpingPoints::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
	if (!(begin == QPointF() && end == QPointF())) {
		QPointF src = area()->scaleToPosition(begin);
		QPointF dst = area()->scaleToPosition(end);

		if (begin != end) {
			painter->setRenderHint(QPainter::Antialiasing);
			quiver.draw(painter, QLineF(src, dst));
		}
		else {
			painter->setPen(Qt::black);
			painter->drawLine(QPointF(src.x(), src.y() - 5), QPointF(src.x(), src.y() + 5));
			painter->drawLine(QPointF(src.x() - 5, src.y()), QPointF(src.x() + 5, src.y()));
		}
	}
}

bool DrawWarpingPoints::sceneEvent(QEvent* event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::GraphicsSceneMousePress) {
		QGraphicsSceneMouseEvent* evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		if (evt->buttons() != Qt::LeftButton)
			return false;
		begin = vipRound(area()->positionToScale(evt->pos()));
		end = begin;
		warping->StartDeformation(begin.toPoint());
		return true;
	}
	else if (event->type() == QEvent::GraphicsSceneMouseMove) {
		QGraphicsSceneMouseEvent* evt = static_cast<QGraphicsSceneMouseEvent*>(event);
		if (evt->buttons() != Qt::LeftButton)
			return false;
		end = vipRound(area()->positionToScale(evt->pos()));
		warping->MovePoint(end.toPoint());
		this->prepareGeometryChange();
		return true;
	}
	else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
		warping->EndDeformation();
		begin = end = QPointF();

		return true;
	}

	return false;
}

bool DrawWarpingPoints::eventFilter(QObject* /*watched*/, QEvent* event)
{
	if (!area())
		return false;

	if (event->type() == QEvent::MouseButtonPress) {
		if (VipImageArea2D* a = qobject_cast<VipImageArea2D*>(area())) {
			QPoint pt = QCursor::pos();
			QRect view_rect = area()->view()->mapFromScene(a->visualizedSceneRect()).boundingRect();
			view_rect = QRect(area()->view()->mapToGlobal(view_rect.topLeft()), area()->view()->mapToGlobal(view_rect.bottomRight()));
			if (!view_rect.contains(pt)) {
				this->deleteLater();
				// TODO: uncheck a button in the parent VipWarpingEditor
				warping->SetDrawingEnabled(false);
			}
		}
	}

	return false;
}

class PlotWarpingPoints::PrivateData
{
public:
	VipQuiverPath quiver;
	VipSymbol symbol;
};

PlotWarpingPoints::PlotWarpingPoints(const VipText& title)
  : VipPlotItemDataType(title)
{
	m_data = new PrivateData();
	this->setItemAttribute(VipPlotItem::HasLegendIcon, false);
	this->setItemAttribute(VipPlotItem::AutoScale, false);
	this->setItemAttribute(VipPlotItem::IsSuppressable, false);
	this->setRenderHints(QPainter::Antialiasing);

	m_data->quiver.setPen(QColor(Qt::red));
	m_data->quiver.setStyle(VipQuiverPath::EndArrow);
	m_data->quiver.setLength(VipQuiverPath::End, 8);
	m_data->quiver.setExtremityBrush(VipQuiverPath::End, QBrush(Qt::red));
	m_data->quiver.setExtremityPen(VipQuiverPath::End, QColor(Qt::red));
	m_data->symbol.setStyle(VipSymbol::Cross);
	m_data->symbol.setPen(QColor(Qt::black));
	m_data->symbol.setSize(QSizeF(7, 7));
}

PlotWarpingPoints::~PlotWarpingPoints()
{
	delete m_data;
}

void PlotWarpingPoints::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	DeformationField field = rawData();
	for (int i = 0; i < field.size(); ++i) {
		if (field[i].first == field[i].second) {
			painter->setRenderHint(QPainter::Antialiasing, false);
			m_data->symbol.drawSymbol(painter, m->transform(field[i].first));
		}
		else {
			painter->setRenderHint(QPainter::Antialiasing, true);
			m_data->quiver.draw(painter, QLineF(m->transform(field[i].first), m->transform(field[i].second)));
		}
	}
}

QList<VipText> PlotWarpingPoints::legendNames() const
{
	return QList<VipText>();
}

QRectF PlotWarpingPoints::drawLegend(QPainter*, const QRectF&, int /* index*/) const
{
	return QRectF();
}

VipQuiverPath& PlotWarpingPoints::quiverPath() const
{
	return const_cast<VipQuiverPath&>(m_data->quiver);
}

VipSymbol& PlotWarpingPoints::symbol() const
{
	return const_cast<VipSymbol&>(m_data->symbol);
}



class VipWarpingEditor::PrivateData
{
public:
	QToolButton save;
	QToolButton load;
	QToolButton reset;

	QRadioButton from_players;
	VipComboBox players;
	QToolButton compute;

	QRadioButton from_points;
	QToolButton start_drawing;
	QToolButton undo_points;
	QToolButton display_points;

	QPointer<VipWarping> warping;
	QPointer<PlotWarpingPoints> plot_points;
	QPointer<DrawWarpingPoints> draw_points;

	DeformationField drawn_points;
};

VipWarpingEditor::VipWarpingEditor(QWidget* parent)
  : QWidget(parent)
{
	d_data = new PrivateData();

	QHBoxLayout* hlay1 = new QHBoxLayout();
	hlay1->addWidget(&d_data->save);
	hlay1->addWidget(&d_data->load);
	hlay1->addWidget(&d_data->reset);
	hlay1->addWidget(VipLineWidget::createSunkenVLine());
	hlay1->addWidget(&d_data->players);
	hlay1->addWidget(&d_data->compute);
	hlay1->addWidget(&d_data->start_drawing);
	hlay1->addWidget(&d_data->undo_points);
	hlay1->addWidget(&d_data->display_points);
	hlay1->addStretch(1);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->from_players);
	lay->addWidget(&d_data->from_points);
	lay->addLayout(hlay1);
	lay->setContentsMargins(0, 0, 0, 0);

	setLayout(lay);

	d_data->from_players.setText("Apply warping from 2 sets of points");
	d_data->from_points.setText("Draw the deformation field on the player");
	d_data->from_players.setChecked(true);
	connect(&d_data->from_players, SIGNAL(clicked(bool)), this, SLOT(SetSourcePointsFromPlayers()));
	connect(&d_data->from_points, SIGNAL(clicked(bool)), this, SLOT(SetSourcePointsFromDrawing()));

	d_data->save.setToolTip("Save computed warping");
	d_data->save.setIcon(vipIcon("save_as.png"));
	d_data->save.setAutoRaise(true);
	d_data->load.setToolTip("Load a previously computed warping");
	d_data->load.setIcon(vipIcon("open_file.png"));
	d_data->load.setAutoRaise(true);
	d_data->reset.setToolTip("Reset warping");
	d_data->reset.setIcon(vipIcon("reset.png"));
	d_data->reset.setAutoRaise(true);
	connect(&d_data->save, SIGNAL(clicked(bool)), this, SLOT(SaveTransform()));
	connect(&d_data->load, SIGNAL(clicked(bool)), this, SLOT(LoadTransform()));
	connect(&d_data->reset, SIGNAL(clicked(bool)), this, SLOT(ResetWarping()));

	d_data->compute.setToolTip("Compute warping");
	d_data->compute.setIcon(vipIcon("apply.png"));
	d_data->compute.setAutoRaise(true);
	ComputePlayerList();
	connect(&d_data->players, SIGNAL(openPopup()), this, SLOT(ComputePlayerList()));
	connect(&d_data->compute, SIGNAL(clicked(bool)), this, SLOT(ChangeWarping()));

	d_data->start_drawing.setToolTip("<b>Start drawing the deformation field</b><br>A simple click defines a fixed point<br>Moving the mouse while pressing it defines a deformation");
	d_data->start_drawing.setIcon(vipIcon("deformation_field.png"));
	d_data->start_drawing.setCheckable(true);
	d_data->start_drawing.setChecked(true);
	d_data->start_drawing.setAutoRaise(true);
	connect(&d_data->start_drawing, SIGNAL(clicked(bool)), this, SLOT(SetDrawingEnabled(bool)));

	d_data->undo_points.setToolTip("Undo the last deformation");
	d_data->undo_points.setIcon(vipIcon("undo.png"));
	d_data->undo_points.setAutoRaise(true);
	connect(&d_data->undo_points, SIGNAL(clicked(bool)), this, SLOT(Undo()));

	d_data->display_points.setToolTip("Show/Hide the deformation field");
	d_data->display_points.setIcon(vipIcon("open_fov.png"));
	d_data->display_points.setCheckable(true);
	d_data->display_points.setChecked(true);
	d_data->display_points.setAutoRaise(true);
	connect(&d_data->display_points, SIGNAL(clicked(bool)), this, SLOT(SetDrawnPointsVisible(bool)));
	d_data->plot_points = new PlotWarpingPoints();
	d_data->plot_points->setVisible(true);

	SetSourcePointsFromPlayers();

	this->setMinimumHeight(80);
}

VipWarpingEditor::~VipWarpingEditor()
{
	SaveParametersToWarpingObject();

	if (d_data->draw_points)
		d_data->draw_points->deleteLater();
	if (d_data->plot_points)
		d_data->plot_points->deleteLater();

	delete d_data;
}

void VipWarpingEditor::ComputePlayerList()
{
	QWidget* w = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (!w)
		return;

	QList<VipVideoPlayer*> instances = w->findChildren<VipVideoPlayer*>();

	d_data->players.clear();
	for (int i = 0; i < instances.size(); ++i) {
		VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(instances[i]);
		QString title = parent ? parent->windowTitle() : instances[i]->windowTitle();
		d_data->players.addItem(title, VipUniqueId::id(instances[i]));
	}
}

void VipWarpingEditor::SaveTransform()
{
	if (!d_data->warping)
		return;

	if (d_data->warping->warping().size()) {
		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save warping file", "VipWarping file (*.warp)");
		if (!filename.isEmpty()) {
			QFile out(filename);
			if (out.open(QFile::WriteOnly)) {
				const VipPointVector warp = d_data->warping->warping();
				out.write((const char*)warp.data(), warp.size() * sizeof(QPointF));
				out.close();
			}
			else {
				VIP_LOG_ERROR("VipWarping: cannot save in file " + filename);
			}
		}
	}
	else {
		VIP_LOG_ERROR("VipWarping: you have to compute a warping before saving it");
	}
}

void VipWarpingEditor::LoadTransform()
{
	QString filename = VipFileDialog::getOpenFileName((nullptr), "Open a warping file", "VipWarping file (*.warp)");
	if (!filename.isEmpty()) {
		QFile in(filename);
		if (in.open(QFile::ReadOnly)) {
			int size = (int)in.size() / sizeof(QPointF);
			VipPointVector warp(size);
			in.read((char*)warp.data(), in.size());
			d_data->warping->setWarping(warp);
			d_data->warping->reload();
		}
		else {
			VIP_LOG_ERROR("VipWarping: cannot open input file " + filename);
		}
	}
}

void VipWarpingEditor::setWarpingTransform(VipWarping* tr)
{
	if (tr != d_data->warping) {
		d_data->warping = tr;
		LoadParametersFromWarpingObject();
	}
}

VipWarping* VipWarpingEditor::warpingTransform() const
{
	return d_data->warping;
}

VipVideoPlayer* VipWarpingEditor::FindOutputPlayer() const
{
	if (d_data->warping)
		return qobject_cast<VipVideoPlayer*>(VipPlayer2D::findPlayer2D(d_data->warping->sceneModel()));
	return nullptr;
}

PlotWarpingPoints* VipWarpingEditor::PlotPoints() const
{
	if (!d_data->plot_points)
		const_cast<QPointer<PlotWarpingPoints>&>(d_data->plot_points) = new PlotWarpingPoints();
	else if (d_data->warping) {
		if (VipVideoPlayer* player = FindOutputPlayer())
			const_cast<QPointer<PlotWarpingPoints>&>(d_data->plot_points)->setAxes(player->spectrogram()->axes(), VipCoordinateSystem::Cartesian);
	}

	return d_data->plot_points;
}

void VipWarpingEditor::ChangeWarping()
{
	if (!d_data->warping)
		return;

	// ComputePlayerList();
	int id = d_data->players.currentData().toInt();
	if (id == 0) {
		VIP_LOG_ERROR("VipWarping: Cannot find selected player");
		return;
	}

	VipVideoPlayer* player = VipUniqueId::find<VipVideoPlayer>(id);
	if (!player) {
		VIP_LOG_ERROR("VipWarping: Cannot find selected player");
		return;
	}

	QList<VipShape> to = player->plotSceneModel()->sceneModel().shapes("Points");
	QList<VipShape> from = d_data->warping->sceneModel().shapes("Points");

	/*QPointF to_p[10];
	QPointF from_p[10];
	for (int i = 0; i < to.size(); ++i) to_p[i] = to[i].point();
	for (int i = 0; i < from.size(); ++i) from_p[i] = from[i].point();*/

	VipNDArray from_array = d_data->warping->inputAt(0)->probe().value<VipNDArray>();
	VipNDArray to_array = player->spectrogram()->rawData().extract(player->spectrogram()->rawData().boundingRect());

	if (from_array.isEmpty() || to_array.isEmpty() || from_array.shapeCount() != 2 || to_array.shapeCount() != 2) {
		VIP_LOG_ERROR("VipWarping: wrong array shape (must be 2)");
		return;
	}

	if (from.size() != to.size()) {
		VIP_LOG_ERROR("VipWarping: Source player and destination player do not have the same number of points (" + QString::number(to.size()) + " and " + QString::number(from.size()) + ")");
		return;
	}

	if (from.size() == 0) {
		VIP_LOG_ERROR("VipWarping: You need to define at least 1 point of interest");
		return;
	}

	QPointF mapping(double(from_array.shape(1)) / to_array.shape(1), double(from_array.shape(0)) / to_array.shape(0));

	QMap<int, QPoint> p_from, p_to;
	for (int i = 0; i < from.size(); ++i) {
		p_from[from[i].id()] = from[i].point().toPoint();
		QPointF _to = to[i].point();
		_to.setX(_to.x() * mapping.x());
		_to.setY(_to.y() * mapping.y());
		p_to[to[i].id()] = _to.toPoint();
	}

	VipPointVector warp = vipWarping(p_from.values().toVector(), p_to.values().toVector(), from_array.shape(1), from_array.shape(0));

	d_data->warping->setWarping(warp);
	d_data->warping->reload();
}

void VipWarpingEditor::SetSourcePointsFromPlayers(bool from_players)
{
	d_data->from_players.blockSignals(true);
	d_data->from_points.blockSignals(true);

	d_data->from_players.setChecked(from_players);
	d_data->from_points.setChecked(!from_players);

	d_data->from_players.blockSignals(false);
	d_data->from_points.blockSignals(false);

	d_data->players.setVisible(from_players);
	d_data->compute.setVisible(from_players);

	d_data->start_drawing.setVisible(!from_players);
	d_data->undo_points.setVisible(!from_players);
	d_data->display_points.setVisible(!from_players);

	SetDrawingEnabled(!from_players);

	if (!from_players)
		ComputeWarpingFromDrawnPoints();
}

void VipWarpingEditor::SetSourcePointsFromPlayers()
{
	SetSourcePointsFromPlayers(true);
}

void VipWarpingEditor::SetSourcePointsFromDrawing()
{
	SetSourcePointsFromPlayers(false);
}

void VipWarpingEditor::ResetWarping()
{
	if (d_data->warping) {
		d_data->warping->setWarping(VipPointVector());
		d_data->warping->reload();
	}

	d_data->drawn_points.clear();
	PlotPoints()->setRawData(DeformationField());

	SaveParametersToWarpingObject();
}

void VipWarpingEditor::SetDrawingEnabled(bool enable)
{
	d_data->start_drawing.blockSignals(true);
	d_data->start_drawing.setChecked(enable);
	d_data->start_drawing.blockSignals(false);

	if (!enable) {
		if (d_data->draw_points)
			d_data->draw_points->deleteLater();
		return;
	}
	else {
		if (d_data->draw_points)
			d_data->draw_points->deleteLater();
		if (VipVideoPlayer* player = FindOutputPlayer()) {
			d_data->draw_points = new DrawWarpingPoints(player->viewer()->area(), this);
		}
		else
			SetDrawingEnabled(false);
	}
}

void VipWarpingEditor::Undo()
{
	if (d_data->drawn_points.size()) {
		d_data->drawn_points.removeLast();
		PlotPoints()->setRawData(d_data->drawn_points);
	}
	ComputeWarpingFromDrawnPoints();
}

void VipWarpingEditor::SetDrawnPointsVisible(bool visible)
{
	PlotPoints()->setVisible(visible);
	SaveParametersToWarpingObject();
}

bool VipWarpingEditor::DrawnPointsVisible() const
{
	return PlotPoints()->isVisible();
}

void VipWarpingEditor::StartDeformation(const QPoint& src)
{
	d_data->drawn_points.append(QPair<QPoint, QPoint>(src, src));
}
void VipWarpingEditor::MovePoint(const QPoint& dst)
{
	d_data->drawn_points.last().second = dst;
	ComputeWarpingFromDrawnPoints();
}
void VipWarpingEditor::EndDeformation()
{
	PlotPoints()->setRawData(d_data->drawn_points);
	ComputeWarpingFromDrawnPoints();
}

void VipWarpingEditor::SaveParametersToWarpingObject()
{
	if (d_data->warping) {
		d_data->warping->setProperty("deformationField", QVariant::fromValue(d_data->drawn_points));
		d_data->warping->setProperty("deformationFieldVisible", PlotPoints()->isVisible());
		d_data->warping->setProperty("usePoints", d_data->from_points.isChecked());
	}
}

void VipWarpingEditor::LoadParametersFromWarpingObject()
{
	if (d_data->warping) {
		if (d_data->warping->property("deformationField").userType() == qMetaTypeId<DeformationField>()) {
			d_data->drawn_points = d_data->warping->property("deformationField").value<DeformationField>();
			SetSourcePointsFromPlayers(!d_data->warping->property("usePoints").toBool());
			SetDrawnPointsVisible(d_data->warping->property("deformationFieldVisible").toBool());
			PlotPoints()->setRawData(d_data->drawn_points);
		}
	}
}

void VipWarpingEditor::ComputeWarpingFromDrawnPoints()
{
	if (d_data->warping) {
		VipNDArray from_array = d_data->warping->inputAt(0)->probe().value<VipNDArray>();
		if (!from_array.isEmpty()) {
			if (d_data->drawn_points.size()) {
				QVector<QPoint> src(d_data->drawn_points.size()), dst(d_data->drawn_points.size());
				for (int i = 0; i < d_data->drawn_points.size(); ++i) {
					src[i] = d_data->drawn_points[i].first;
					dst[i] = d_data->drawn_points[i].second;
				}
				VipPointVector warp = vipWarping(src, dst, from_array.shape(1), from_array.shape(0));
				d_data->warping->setWarping(warp);
			}
			else
				d_data->warping->setWarping(VipPointVector());
			d_data->warping->reload();
		}
	}

	SaveParametersToWarpingObject();
}

static QWidget* editWarping(VipWarping* tr)
{
	VipWarpingEditor* editor = new VipWarpingEditor();
	editor->setWarpingTransform(tr);
	return editor;
}

static QWidget* editCrop(VipImageCrop* th)
{
	VipCropEditor* editor = new VipCropEditor();
	editor->setCrop(th);
	return editor;
}

static QWidget* editResize(VipResize* th)
{
	VipResizeEditor* editor = new VipResizeEditor();
	editor->setResize(th);
	return editor;
}

static QWidget* editGenericImageTransform(VipGenericImageTransform* th)
{
	VipGenericImageTransformEditor* editor = new VipGenericImageTransformEditor();
	editor->setProcessing(th);
	return editor;
}

static QWidget* editComponentLabelling(VipComponentLabelling* th)
{
	VipComponentLabellingEditor* editor = new VipComponentLabellingEditor();
	editor->setComponentLabelling(th);
	return editor;
}

static QWidget* editIODevice(VipIODevice* obj)
{
	VipIODeviceEditor* editor = new VipIODeviceEditor();
	editor->setIODevice(obj);
	return editor;
}

static QWidget* editProcessingList(VipProcessingList* obj)
{
	VipProcessingListEditor* editor = new VipProcessingListEditor();
	editor->setProcessingList(obj);
	return editor;
}

static QWidget* editSplitAndMerge(VipSplitAndMerge* obj)
{
	VipSplitAndMergeEditor* editor = new VipSplitAndMergeEditor();
	editor->setProcessing(obj);
	return editor;
}

static QWidget* editExtractComponent(VipExtractComponent* obj)
{
	VipExtractComponentEditor* editor = new VipExtractComponentEditor();
	editor->setExtractComponent(obj);
	return editor;
}

static QWidget* editConversion(VipConvert* obj)
{
	VipConvertEditor* editor = new VipConvertEditor();
	editor->setConvert(obj);
	return editor;
}

static QWidget* editDisplayImage(VipDisplayImage* obj)
{
	VipDisplayImageEditor* editor = new VipDisplayImageEditor();
	editor->setDisplayImage(obj);
	return editor;
}

static QWidget* editSwitch(VipSwitch* obj)
{
	VipSwitchEditor* editor = new VipSwitchEditor();
	editor->setSwitch(obj);
	return editor;
}

static QWidget* editClamp(VipClamp* obj)
{
	VipClampEditor* editor = new VipClampEditor();
	editor->setClamp(obj);
	return editor;
}

static QWidget* editTextFileReader(VipTextFileReader* obj)
{
	if (obj->type() != VipTextFileReader::Unknown)
		return nullptr;
	QStringList lst = obj->removePrefix(obj->path()).split(";");
	if (lst.size() == 2)
		return nullptr;
	VipTextFileReaderEditor* editor = new VipTextFileReaderEditor();
	editor->setTextFileReader(obj);
	return editor;
}

static QWidget* editTextFileWriter(VipTextFileWriter* obj)
{
	VipTextFileWriterEditor* editor = new VipTextFileWriterEditor();
	editor->setTextFileWriter(obj);
	return editor;
}

static QWidget* editImageWriter(VipImageWriter* obj)
{
	VipImageWriterEditor* editor = new VipImageWriterEditor();
	editor->setImageWriter(obj);
	return editor;
}

static QWidget* editCSVWriter(VipCSVWriter* obj)
{
	VipCSVWriterEditor* editor = new VipCSVWriterEditor();
	editor->setCSVWriter(obj);
	return editor;
}

static QWidget* editDirectoryReader(VipDirectoryReader* obj)
{
	VipDirectoryReaderEditor* editor = new VipDirectoryReaderEditor();
	editor->setDirectoryReader(obj);
	return editor;
}

static QWidget* editOperationBetweenPlayers(VipOperationBetweenPlayers* obj)
{
	VipOperationBetweenPlayersEditor* editor = new VipOperationBetweenPlayersEditor();
	editor->setProcessing(obj);
	return editor;
}

static int registerEditors()
{
	// For now remove this editor which is pretty much useless
	// vipFDObjectEditor().append<QWidget*(VipProcessingObject*)>(editProcessingObject);
	vipFDObjectEditor().append<QWidget*(VipIODevice*)>(editIODevice);
	vipFDObjectEditor().append<QWidget*(VipProcessingList*)>(editProcessingList);
	vipFDObjectEditor().append<QWidget*(VipSplitAndMerge*)>(editSplitAndMerge);
	vipFDObjectEditor().append<QWidget*(VipExtractComponent*)>(editExtractComponent);
	vipFDObjectEditor().append<QWidget*(VipConvert*)>(editConversion);
	vipFDObjectEditor().append<QWidget*(VipDisplayImage*)>(editDisplayImage);
	vipFDObjectEditor().append<QWidget*(VipSwitch*)>(editSwitch);
	vipFDObjectEditor().append<QWidget*(VipClamp*)>(editClamp);
	vipFDObjectEditor().append<QWidget*(VipTextFileReader*)>(editTextFileReader);
	vipFDObjectEditor().append<QWidget*(VipTextFileWriter*)>(editTextFileWriter);
	vipFDObjectEditor().append<QWidget*(VipImageWriter*)>(editImageWriter);
	vipFDObjectEditor().append<QWidget*(VipCSVWriter*)>(editCSVWriter);
	vipFDObjectEditor().append<QWidget*(VipDirectoryReader*)>(editDirectoryReader);
	vipFDObjectEditor().append<QWidget*(VipOperationBetweenPlayers*)>(editOperationBetweenPlayers);
	vipFDObjectEditor().append<QWidget*(VipWarping*)>(editWarping);
	vipFDObjectEditor().append<QWidget*(VipImageCrop*)>(editCrop);
	vipFDObjectEditor().append<QWidget*(VipResize*)>(editResize);
	vipFDObjectEditor().append<QWidget*(VipGenericImageTransform*)>(editGenericImageTransform);
	vipFDObjectEditor().append<QWidget*(VipComponentLabelling*)>(editComponentLabelling);
	return 0;
}

static int _registerEditors = vipAddInitializationFunction(registerEditors);

#include <QBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <qheaderview.h>

class PropertyEditor;
class PropertyWidget : public QTreeWidget
{
public:
	QList<PropertyEditor*> editors;

	PropertyWidget()
	  : QTreeWidget()
	{
		this->header()->hide();
		this->setSelectionMode(QAbstractItemView::NoSelection);
		this->setFrameShape(QFrame::NoFrame);
		this->setIndentation(10);
		this->setStyleSheet("QTreeWidget {background: transparent;}");
		this->setObjectName("_vip_PropertyWidget");
		// connect(this, SIGNAL(itemExpanded(QTreeWidgetItem *)), this, SLOT(resetSize()));
		// connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem *)), this, SLOT(resetSize()));
	}

	QSize itemSizeHint(QTreeWidgetItem* item) const
	{
		if (!item->isHidden()) {
			int h = 0, w = 0;
			for (int i = 0; i < this->columnCount(); ++i) {
				w += this->sizeHintForColumn(i);
				h = qMax(h, this->rowHeight(indexFromItem(item, i)) + 3);
			}

			if (item->isExpanded()) {
				for (int i = 0; i < item->childCount(); ++i) {
					QSize s = itemSizeHint(item->child(i));
					h += s.height();
					w = qMax(w, s.width());
				}
			}
			return QSize(w, h);
		}
		return QSize(0, 0);
	}

	void resetSize()
	{
		int h = 0, w = 0;
		for (int i = 0; i < topLevelItemCount(); ++i) {
			QSize tmp = itemSizeHint(this->topLevelItem(i));
			h += tmp.height();
			w = qMax(w, tmp.width());
		}

		setMinimumHeight(h);
		resize(w, h);
	}

	virtual QSize sizeHint() const
	{
		int h = 0, w = 0;
		for (int i = 0; i < topLevelItemCount(); ++i) {
			QSize tmp = itemSizeHint(const_cast<PropertyWidget*>(this)->topLevelItem(i));
			h += tmp.height();
			w = qMax(w, tmp.width());
		}
		return QSize(w, h);
	}

	QTreeWidgetItem* find(QTreeWidgetItem* root, const QString& category)
	{
		if (category.isEmpty())
			return root;

		QStringList lst = category.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		if (lst.size() == 0)
			lst << category;

		for (int i = 0; i < lst.size(); ++i) {
			const QString name = lst[i];
			QTreeWidgetItem* item = nullptr;

			// find child with name
			for (int j = 0; j < root->childCount(); ++j) {
				if (root->child(j)->text(0) == name) {
					item = root->child(j);
					break;
				}
			}

			if (!item) {
				item = new QTreeWidgetItem(root);
				item->setText(0, name);
				QFont font = item->font(0);
				font.setBold(true);
				item->setFont(0, font);
			}

			root = item;
		}

		return (root);
	}

	void setEditors(const QList<PropertyEditor*>& eds)
	{
		this->clear();
		editors = eds;

		for (int i = 0; i < eds.size(); ++i) {
			eds[i]->parent = this;
			QTreeWidgetItem* root = find(this->invisibleRootItem(), eds[i]->category);
			QTreeWidgetItem* item = new QTreeWidgetItem();
			root->addChild(item);
			item->setSizeHint(0, eds[i]->sizeHint());
			this->setItemWidget(item, 0, eds[i]);
		}
		this->expandAll();
		setMinimumHeight(sizeHint().height());
	}

	void updateProperties();
};

static bool IsDouble(const QVariant& v)
{
	bool ok;
	v.toDouble(&ok);
	return ok;
}

PropertyEditor::PropertyEditor(VipProcessingObject* obj, const QString& property)
  : QWidget()
  , editor(nullptr)
  , property(property)
  , object(obj)
  , parent(nullptr)
{
	this->setObjectName("_vip_PropertyEditor");

	category = obj->propertyCategory(property);
	QVariant v = obj->propertyName(property)->data().data();
	QString style_sheet = obj->propertyEditor(property);
	if (style_sheet.size()) {
		editor = VipStandardWidgets::fromStyleSheet(style_sheet);
		if (editor)
			connect(editor, SIGNAL(genericValueChanged(const QVariant&)), this, SLOT(updateProperty()));
	}
	else if (v.userType() == QMetaType::Bool) {
		editor = new VipBoolEdit();
		connect(static_cast<VipBoolEdit*>(editor), &VipBoolEdit::valueChanged, this, &PropertyEditor::updateProperty);
	}
	else if (v.canConvert<double>() && IsDouble(v)) {
		editor = new VipDoubleEdit();
		connect(static_cast<VipDoubleEdit*>(editor), &VipDoubleEdit::valueChanged, this, &PropertyEditor::updateProperty);
	}
	else if (v.canConvert<VipNDDoubleCoordinate>()) {
		editor = new VipMultiComponentDoubleEdit();
		static_cast<VipMultiComponentDoubleEdit*>(editor)->setMaxNumberOfComponents(10);
		connect(static_cast<VipMultiComponentDoubleEdit*>(editor), &VipMultiComponentDoubleEdit::valueChanged, this, &PropertyEditor::updateProperty);
	}
	else if (v.canConvert<QString>() && v.userType() != qMetaTypeId<VipNDArray>()) {
		editor = new VipLineEdit();
		connect(static_cast<VipLineEdit*>(editor), &VipLineEdit::returnPressed, this, &PropertyEditor::updateProperty);
	}

	if (editor) {
		editor->setProperty("value", obj->propertyName(property)->data().data());
		editor->setToolTip(obj->propertyDescription(property));
	}

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addWidget(new QLabel(vipSplitClassname(property) + " : "));
	if (editor)
		lay->addWidget(editor);
	lay->setContentsMargins(0, 0, 0, 0);

	// lay->addStretch();
	// lay->setSizeConstraint(QLayout::SetFixedSize);

	setLayout(lay);
}

void PropertyEditor::updateProperty()
{
	if (parent)
		parent->updateProperties();
}

void PropertyWidget::updateProperties()
{
	for (int i = 0; i < editors.size(); ++i) {
		if (editors[i]->object && editors[i]->editor) {
			if (VipProperty* p = editors[i]->object->propertyName(editors[i]->property)) {
				QVariant v = editors[i]->editor->property("value");
				if (v.userType() != 0)
					p->setData(v);
			}
		}
	}

	if (editors.size() && editors.last()->object)
		editors.last()->object->reload();
}

// Create a default editor for a VipProcessingObject.
// This editor will display one widget per processing property.
// If the processing has no property, a nullptr widget is returned.
static QWidget* defaultEditor(VipProcessingObject* obj)
{
	int count = obj->propertyCount();
	if (!count)
		return nullptr;

	QList<PropertyEditor*> editors;
	for (int i = 0; i < count; ++i) {
		VipProperty* p = obj->propertyAt(i);
		if (p->data().data().canConvert<QString>() || obj->propertyEditor(p->name()).size()) {
			PropertyEditor* edit = new PropertyEditor(obj, p->name());
			if (edit->editor)
				editors << edit;
			else
				delete edit;
		}
	}

	if (!editors.size())
		return nullptr;

	PropertyWidget* res = new PropertyWidget();

	res->setEditors(editors);
	// QVBoxLayout * lay = new QVBoxLayout();
	// for (int i = 0; i < editors.size(); ++i)
	// {
	// lay->addWidget(editors[i]);
	// editors[i]->parent = res;
	// }
	// res->editors = editors;
	// lay->setContentsMargins(0, 0, 0, 0);
	// res->setLayout(lay);
	return res;
}

class VipUniqueProcessingObjectEditor::PrivateData
{
public:
	VipProcessingObject* processingObject;
	bool isShowExactProcessingOnly;
	PrivateData()
	  : processingObject(nullptr)
	  , isShowExactProcessingOnly(true)
	{
	}
};

VipUniqueProcessingObjectEditor::VipUniqueProcessingObjectEditor(QWidget* parent)
  : QWidget(parent)
{
	m_data = new PrivateData();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setSpacing(1);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);
}

VipUniqueProcessingObjectEditor::~VipUniqueProcessingObjectEditor()
{
	delete m_data;
}

void VipUniqueProcessingObjectEditor::emitEditorVisibilityChanged()
{
	Q_EMIT editorVisibilityChanged();
}

VipProcessingObject* VipUniqueProcessingObjectEditor::processingObject() const
{
	return const_cast<VipProcessingObject*>(m_data->processingObject);
}

void VipUniqueProcessingObjectEditor::geometryChanged(QWidget* widget)
{
	while (widget) {
		if (VipUniqueProcessingObjectEditor* w = qobject_cast<VipUniqueProcessingObjectEditor*>(widget)) {
			// TEST resetSize();
			QMetaObject::invokeMethod(w, "emitEditorVisibilityChanged", Qt::DirectConnection);
			return;
		}
		widget = widget->parentWidget();
	}
}

void VipUniqueProcessingObjectEditor::setShowExactProcessingOnly(bool exact_proc)
{
	m_data->isShowExactProcessingOnly = exact_proc;
	QList<QWidget*> lines = findChildren<QWidget*>("VLine");
	QList<QWidget*> boxes = findChildren<QWidget*>("Box");
	QList<QWidget*> editors = findChildren<QWidget*>("Editor");

	// hide all lines and boxes, and all editors except the first one
	for (int i = 0; i < lines.size(); ++i)
		lines[i]->setVisible(!exact_proc);
	for (int i = 0; i < boxes.size(); ++i)
		boxes[i]->setVisible(!exact_proc);
}

bool VipUniqueProcessingObjectEditor::isShowExactProcessingOnly() const
{
	return m_data->isShowExactProcessingOnly;
}

void VipUniqueProcessingObjectEditor::tryUpdateProcessing()
{
	if (!m_data->processingObject)
		return;

	QList<QWidget*> widgets = this->findChildren<QWidget*>("_vip_PropertyEditor");
	for (int i = 0; i < widgets.size(); ++i) {
		if (static_cast<PropertyEditor*>(widgets[i])->parent) {
			static_cast<PropertyEditor*>(widgets[i])->parent->updateProperties();
			break;
		}
	}
}

#include <qplaintextedit.h>
#include <qtextedit.h>
static bool shouldAddStretch(QWidget* w)
{
	QList<QTextEdit*> edits = w->findChildren<QTextEdit*>();
	for (int i = 0; i < edits.size(); ++i)
		if (!edits[i]->isHidden() && edits[i]->maximumHeight() >= QWIDGETSIZE_MAX)
			return false;
	QList<QPlainTextEdit*> edits2 = w->findChildren<QPlainTextEdit*>();
	for (int i = 0; i < edits2.size(); ++i)
		if (!edits2[i]->isHidden() && edits2[i]->maximumHeight() >= QWIDGETSIZE_MAX)
			return false;

	return true;
}

void VipUniqueProcessingObjectEditor::removeEndStretch()
{
	// remove this stretch
	if (layout()->count()) {
		QLayoutItem* it = layout()->itemAt(layout()->count() - 1);
		if (it->spacerItem() && it->spacerItem()->sizePolicy().verticalPolicy() == QSizePolicy::Expanding) {
			layout()->takeAt(layout()->count() - 1);
			delete it;
		}
	}

	// find VipUniqueProcessingObjectEditor parent
	QWidget* parent = this->parentWidget();
	while (parent) {
		if (VipUniqueProcessingObjectEditor* p = qobject_cast<VipUniqueProcessingObjectEditor*>(parent)) {
			p->removeEndStretch();
			break;
		}
		parent = parent->parentWidget();
	}
}

bool VipUniqueProcessingObjectEditor::setProcessingObject(VipProcessingObject* obj)
{
	if (obj == m_data->processingObject)
		return false;

	m_data->processingObject = obj;

	// clear the previous editors
	QVBoxLayout* lay = static_cast<QVBoxLayout*>(layout());
	while (lay->count()) {
		QLayoutItem* item = lay->takeAt(0);
		if (item->widget()) {
			item->widget()->setAttribute(Qt::WA_DeleteOnClose);
			item->widget()->close();
		}
		delete item;
	}
	QList<QWidget*> childs = this->findChildren<QWidget*>();
	for (int i = 0; i < childs.size(); ++i) {
		childs[i]->setAttribute(Qt::WA_DeleteOnClose);
		childs[i]->close();
	}

	if (!obj)
		return true;

	// create the editors from the top level class type to VipProcessingObject, and add them
	QVector<const QMetaObject*> metas;
	const QMetaObject* meta = obj->metaObject();
	while (meta) {
		metas.append(meta);
		meta = meta->superClass();
	}

	QVector<QWidget*> editors(metas.size(), nullptr);

	const auto lst = vipFDObjectEditor().match(obj);
	for (int i = 0; i < lst.size(); ++i) {
		auto fun = lst[i];
		const QMetaObject* _meta = fun.typeList()[0].metaObject;
		QWidget* editor = fun(obj).value<QWidget*>();
		if (editor) {
			// insert the editor while taking care of the inheritance order
			int index = metas.indexOf(_meta);
			if (index >= 0)
				editors[index] = editor;
			else
				delete editor;
		}
	}

	// if no custom editor found, try to create a default one
	if (!editors[0])
		editors[0] = defaultEditor(obj);

	bool res = false;
	bool add_stretch = false;
	// now add the editors with a QGroupBox header
	for (int i = 0; i < editors.size(); ++i) {
		if (editors[i]) {
			editors[i]->setObjectName("Editor");
			editors[i]->setParent(this);
			QGroupBox* box = new QGroupBox(vipSplitClassname(metas[i]->className()));
			box->setParent(this);
			box->setObjectName("Box");
			box->setToolTip("Show/hide properties inherited from " + QString(vipSplitClassname(metas[i]->className())));
			box->setFlat(true);
			box->setCheckable(true);
			box->setChecked(true);

			QHBoxLayout* hlay = new QHBoxLayout();
			QWidget* line = VipLineWidget::createSunkenVLine();
			line->setObjectName("VLine");
			line->setParent(this);
			hlay->addWidget(line);
			// TEST: add stretch factor
			hlay->addWidget(editors[i], 1);
			hlay->setContentsMargins(2, 0, 2, 0);
			hlay->setSpacing(1);

			connect(box, SIGNAL(clicked(bool)), editors[i], SLOT(setVisible(bool)));
			connect(box, SIGNAL(clicked(bool)), line, SLOT(setVisible(bool)));
			connect(box, SIGNAL(clicked(bool)), SLOT(emitEditorVisibilityChanged()));

			// only show the first one
			if (lay->count()) {
				box->setChecked(false);
				editors[i]->hide();
				line->hide();
			}
			else {
				box->setChecked(true);
				editors[i]->show();
				line->show();
			}

			lay->addWidget(box);
			// TEST: add stretch factor
			lay->addLayout(hlay, 1);

			add_stretch = add_stretch || shouldAddStretch(editors[i]);

			res = true;
		}
	}

	// add big stretch factor at the end if necessary
	if (add_stretch)
		lay->addStretch(1000);
	else {
		removeEndStretch();
	}

	setShowExactProcessingOnly(m_data->isShowExactProcessingOnly);

	return res;
}

class VipProcessingLeafSelector::PrivateData
{
public:
	QPointer<VipProcessingObject> processing;
	QPointer<VipProcessingPool> pool;
	QMenu* menu;
};

VipProcessingLeafSelector::VipProcessingLeafSelector(QWidget* parent)
  : QToolButton(parent)
{
	m_data = new PrivateData();

	this->setText("Select a leaf processing");
	this->setToolTip("<p><b>Select an item (video, image, curve...) in the current workspace.</b></p>"
			 "\nThis will display the processings related to this item.");

	m_data->menu = new QMenu(this);
	this->setMenu(m_data->menu);
	this->setPopupMode(QToolButton::InstantPopup);
	this->setMaximumWidth(300);

	connect(m_data->menu, SIGNAL(triggered(QAction*)), this, SLOT(processingSelected(QAction*)));
	connect(m_data->menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
}

VipProcessingLeafSelector::~VipProcessingLeafSelector()
{
	delete m_data;
}

void VipProcessingLeafSelector::setProcessingPool(VipProcessingPool* pool)
{
	m_data->pool = pool;
	m_data->processing = nullptr;
}

VipProcessingPool* VipProcessingLeafSelector::processingPool() const
{
	return m_data->pool;
}

QList<VipProcessingObject*> VipProcessingLeafSelector::leafs() const
{
	if (VipProcessingPool* pool = m_data->pool) {
		QList<VipProcessingObject*> res;
		QList<VipProcessingObject*> children = pool->findChildren<VipProcessingObject*>();

		for (int i = 0; i < children.size(); ++i) {
			if (children[i]->outputCount() == 0)
				res << children[i];
		}
		return res;
	}
	return QList<VipProcessingObject*>();
}
VipProcessingObject* VipProcessingLeafSelector::processing() const
{
	return m_data->processing;
}

void VipProcessingLeafSelector::setProcessing(VipProcessingObject* proc)
{
	m_data->processing = proc;
	if (proc) {
		QString tool_tip;
		QFontMetrics m(font());
		QString text = m.elidedText(title(proc, tool_tip), Qt::ElideRight, this->maximumWidth() - 30);
		this->setText(text);
	}
	else
		this->setText("Select a leaf processing");
}

void VipProcessingLeafSelector::aboutToShow()
{
	m_data->menu->blockSignals(true);

	m_data->menu->clear();
	m_data->menu->setToolTipsVisible(true);
	QList<VipProcessingObject*> _leafs = this->leafs();
	for (int i = 0; i < _leafs.size(); ++i) {
		QString tool_tip;
		QAction* act = m_data->menu->addAction(title(_leafs[i], tool_tip));
		act->setCheckable(true);
		act->setToolTip(tool_tip);
		if (_leafs[i] == m_data->processing)
			act->setChecked(true);
		act->setProperty("processing", QVariant::fromValue(_leafs[i]));
	}

	m_data->menu->blockSignals(false);
}

void VipProcessingLeafSelector::processingSelected(QAction* act)
{
	if (VipProcessingObject* proc = act->property("processing").value<VipProcessingObject*>()) {
		if (proc != m_data->processing) {
			setProcessing(proc);
			Q_EMIT processingChanged(proc);
		}
	}
}

QString VipProcessingLeafSelector::title(VipProcessingObject* obj, QString& tool_tip) const
{
	QStringList tip_lst;
	QString res;

	if (VipDisplayObject* disp = qobject_cast<VipDisplayObject*>(obj)) {
		if (VipAbstractPlayer* pl = vipFindParent<VipAbstractPlayer>( disp->widget())) {
			tip_lst << "<b>Player</b>: " + QString::number(pl->parentId()) + " " + pl->QWidget::windowTitle();
		}
		res = disp->title();
	}
	tip_lst << "<b>Name</b>: " + vipSplitClassname(obj->objectName());
	if (res.isEmpty())
		res = vipSplitClassname(obj->objectName());
	tool_tip = tip_lst.join("<br>");
	return res;
}

static bool isProcessing(VipProcessingObject* obj)
{
	if (VipIODevice* dev = qobject_cast<VipIODevice*>(obj)) {
		if (dev->openMode() & VipIODevice::ReadOnly)
			return dev->isReading() || dev->isStreamingEnabled() || (QDateTime::currentMSecsSinceEpoch() - dev->lastProcessingTime()) < 500;
		else if (dev->openMode() & VipIODevice::WriteOnly)
			return dev->scheduledUpdates() > 0 || (QDateTime::currentMSecsSinceEpoch() - dev->lastProcessingTime()) < 500;
	}
	else {
		return obj->scheduledUpdates() > 0 || (QDateTime::currentMSecsSinceEpoch() - obj->lastProcessingTime()) < 500;
	}

	return false;
}
// static bool hasError(VipProcessingObject * obj)
// {
// if (obj->hasError())
// return true;
//
// const QList<VipErrorData> errors = obj->lastErrors();
// if (errors.size())
// return (QDateTime::currentMSecsSinceEpoch() - errors.last().msecsSinceEpoch()) < 500;
//
// return false;
// }

#include <qplaintextedit.h>
#include <qtimer.h>

class VipProcessingTooButton::PrivateData
{
public:
	QPointer<VipProcessingObject> processing;
	QPointer<VipUniqueProcessingObjectEditor> editor;

	QTimer timer;
	QToolButton reset;
	QToolButton text;
	QToolButton showError;
	QString icon;

	QPlainTextEdit errors;
	qint64 lastErrorDate;
};

VipProcessingTooButton::VipProcessingTooButton(VipProcessingObject* object)
  : QWidget()
{
	m_data = new PrivateData();
	m_data->processing = object;
	m_data->lastErrorDate = 0;

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(&m_data->reset);
	hlay->addWidget(&m_data->text);
	hlay->addWidget(&m_data->showError);
	hlay->addStretch(1);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(&m_data->errors);
	vlay->setContentsMargins(0, 0, 0, 0);

	setLayout(vlay);

	m_data->reset.setIcon(vipIcon("reset.png"));
	m_data->reset.setToolTip("Reset the processing");

	m_data->showError.setIcon(vipIcon("error.png"));
	m_data->showError.setToolTip("Show the last processing errors");
	m_data->showError.setCheckable(true);

	setMaximumHeight(30);
	if (!object->objectName().isEmpty())
		m_data->text.setText(vipSplitClassname(object->objectName()));
	else
		m_data->text.setText(vipSplitClassname(object->className()));
	// m_data->text.setStyleSheet("font-weight: bold; text-align: left;");
	m_data->text.setStyleSheet("text-align: left;");
	QFont font = m_data->text.font();
	font.setBold(true);
	m_data->text.setFont(font);

	m_data->text.setAutoRaise(true);
	m_data->text.setCheckable(true);
	m_data->text.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_data->text.setIcon(vipIcon("hidden.png"));

	QString tooltip = m_data->text.text() + " properties";
	QString data_name;

	if (object->outputCount() == 1)
		data_name = object->outputAt(0)->data().name();
	else if (object->inputCount() == 1)
		data_name = object->inputAt(0)->probe().name();
	if (data_name.isEmpty()) {
		// look for source processing to get the name
		if (object->inputCount() == 1)
			if (VipOutput* src = object->inputAt(0)->connection()->source())
				data_name = src->data().name();
	}

	tooltip += "<br><b>Processing output: </b>" + data_name;

	m_data->text.setToolTip(tooltip);

	m_data->errors.setStyleSheet("QPlainTextEdit { color: red; font:  14px; background-color:transparent;}");
	m_data->errors.hide();
	m_data->errors.setMinimumHeight(60);
	m_data->errors.setReadOnly(true);
	m_data->errors.setLineWrapMode(QPlainTextEdit::NoWrap);

	m_data->timer.setSingleShot(false);
	m_data->timer.setInterval(100);
	connect(&m_data->timer, &QTimer::timeout, this, &VipProcessingTooButton::updateText);
	m_data->timer.start();

	connect(&m_data->text, SIGNAL(clicked(bool)), this, SLOT(emitClicked(bool)), Qt::DirectConnection);
	connect(&m_data->reset, SIGNAL(clicked(bool)), this, SLOT(resetProcessing()));
	connect(&m_data->showError, SIGNAL(clicked(bool)), this, SLOT(showError(bool)));
}

VipProcessingTooButton::~VipProcessingTooButton()
{
	m_data->timer.stop();
	m_data->timer.disconnect();
	delete m_data;
}
void VipProcessingTooButton::updateText()
{
	if (VipProcessingObject* obj = m_data->processing) {
		qint64 time = obj->processingTime() / 1000000;
		QString text = vipSplitClassname(obj->objectName().isEmpty() ? obj->className() : obj->objectName());
		text += " : " + QString::number(time) + " ms";
		m_data->text.setText(text);

		bool has_error = false;
		const QList<VipErrorData> errors = obj->lastErrors();
		if (obj->hasError())
			has_error = true;
		else if (errors.size())
			has_error = (QDateTime::currentMSecsSinceEpoch() - errors.last().msecsSinceEpoch()) < 500;

		QString icon = "hidden.png";
		if (has_error)
			icon = "highlighted.png";
		else if (isProcessing(obj))
			icon = "visible.png";

		if (icon != m_data->icon) {
			m_data->icon = icon;
			m_data->text.setIcon(vipIcon(icon));
		}

		// format errors
		if (errors.size() && errors.last().msecsSinceEpoch() > m_data->lastErrorDate) {
			m_data->lastErrorDate = errors.last().msecsSinceEpoch();
			m_data->errors.clear();
			QString error_text;
			for (int i = 0; i < errors.size(); ++i) {
				VipErrorData err = errors[i];
				QString date = QDateTime::fromMSecsSinceEpoch(err.msecsSinceEpoch()).toString("yy:MM:dd-hh:mm:ss.zzz    ");
				QString err_str = err.errorString();
				QString code = QString::number(err.errorCode());
				error_text += date + err_str + " (" + code + ")\n";
			}
			m_data->errors.setPlainText(error_text);
		}
	}

	if (m_data->editor) {
		m_data->text.setChecked(m_data->editor->isVisible());
	}
}
void VipProcessingTooButton::showError(bool show_)
{
	m_data->errors.setVisible(show_);

	// change the height of the top level widget
	QWidget* w = this->parentWidget();
	while (w) {
		if (VipProcessingEditorToolWidget* editor = qobject_cast<VipProcessingEditorToolWidget*>(w)) {
			int width = editor->width();
			int height = editor->height();
			height += (show_ ? 80 : -80);
			setMaximumHeight(30 + (show_ ? 100 : 0));
			editor->widget()->setMaximumHeight(height);
			editor->resize(width, height);
			break;
		}
		w = w->parentWidget();
	}
}
QToolButton* VipProcessingTooButton::showButton() const
{
	return &m_data->text;
}
QToolButton* VipProcessingTooButton::resetButton() const
{
	return &m_data->reset;
}

void VipProcessingTooButton::setEditor(VipUniqueProcessingObjectEditor* ed)
{
	m_data->editor = ed;
}
VipUniqueProcessingObjectEditor* VipProcessingTooButton::editor() const
{
	return m_data->editor;
}

void VipProcessingTooButton::resetProcessing()
{
	if (VipProcessingObject* obj = m_data->processing) {
		obj->reset();
		obj->reload();
	}
}

/// A tool button used to show/hide the processing editos.
/// Also displays the processing time in its text.
// class ProcessingTooButton : public QToolButton
// {
// public:
// QPointer<VipProcessingObject> processing;
// QTimer timer;
//
// ProcessingTooButton(VipProcessingObject * object)
// :QToolButton(), processing(object)
// {
// setMaximumHeight(30);
// if (!object->objectName().isEmpty())
//	setText(object->objectName());
// else
//	setText(object->className());
// setStyleSheet("font-weight: bold; text-align: left;");
// setAutoRaise(true);
// setCheckable(true);
// setToolTip("Show/hide " + text() + " properties");
//
// timer.setSingleShot(false);
// timer.setInterval(500);
// connect(&timer, &QTimer::timeout, this, &ProcessingTooButton::updateText);
// timer.start();
// }
//
// ~ProcessingTooButton()
// {
// timer.stop();
// timer.disconnect();
// }
//
// void updateText()
// {
// if (processing)
// {
//	qint64 time = processing->processingTime() / 1000000;
//	QString text = processing->objectName().isEmpty() ? processing->className() : processing->objectName();
//	text += " : " + QString::number(time) + " ms";
//	this->setText(text);
// }
// }
// };

class VipMultiProcessingObjectEditor::PrivateData
{
public:
	QSplitter splitter;
	QList<VipProcessingObject*> processingObjects;
	QList<const QMetaObject*> visibleProcessings;
	QList<const QMetaObject*> hiddenProcessings;
	bool isShowExactProcessingOnly;

	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>> editors;

	PrivateData()
	  : splitter(Qt::Vertical)
	  , isShowExactProcessingOnly(true)
	{
	}
};

VipMultiProcessingObjectEditor::VipMultiProcessingObjectEditor(QWidget* parent)
  : QWidget(parent)
{
	m_data = new PrivateData();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(0);
	setLayout(lay);
}

VipMultiProcessingObjectEditor::~VipMultiProcessingObjectEditor()
{
	delete m_data;
}

void VipMultiProcessingObjectEditor::emitEditorVisibilityChanged()
{
	Q_EMIT editorVisibilityChanged();
}

bool VipMultiProcessingObjectEditor::setProcessingObjects(const QList<VipProcessingObject*>& __objs)
{
	QList<VipProcessingObject*> objs = __objs;
	if (objs.size() > __VIP_MAX_DISPLAYED_EDITORS)
		objs = objs.mid(0, __VIP_MAX_DISPLAYED_EDITORS);

	if (objs == m_data->processingObjects)
		return false;

	m_data->processingObjects = objs;
	m_data->editors.clear();

	// clear the previous editors
	QVBoxLayout* lay = static_cast<QVBoxLayout*>(layout());
	while (lay->count()) {
		QLayoutItem* item = lay->takeAt(0);
		if (item->widget()) {
			item->widget()->disconnect();
			item->widget()->close();
		}
		delete item;
	}

	// add the editors, only show the first one
	bool first = true;
	for (int i = 0; i < objs.size(); ++i) {
		VipUniqueProcessingObjectEditor* editor = new VipUniqueProcessingObjectEditor();
		editor->setShowExactProcessingOnly(m_data->isShowExactProcessingOnly);

		if (editor->setProcessingObject(objs[i])) {
			VipProcessingTooButton* button = new VipProcessingTooButton(objs[i]);
			button->setEditor(editor);

			if (first) {
				button->showButton()->setChecked(true);
				first = false;
			}
			else
				editor->hide();

			if (objs.size() == 1)
				button->hide();

			connect(button, SIGNAL(clicked(bool)), editor, SLOT(setVisible(bool)));
			connect(button, SIGNAL(clicked(bool)), this, SLOT(emitEditorVisibilityChanged()));

			QHBoxLayout* hlay = new QHBoxLayout();
			hlay->addWidget(VipLineWidget::createSunkenVLine());
			hlay->addWidget(editor);
			hlay->setContentsMargins(5, 0, 5, 0);
			QWidget* w = new QWidget();
			w->setLayout(hlay);

			lay->addWidget(button);
			lay->addWidget(w);

			m_data->editors[objs[i]] = (QPair<VipProcessingTooButton*, QWidget*>(button, editor));

			connect(editor, SIGNAL(editorVisibilityChanged()), this, SLOT(emitEditorVisibilityChanged()));
		}
		else
			delete editor;
	}

	// TEST: comment
	// lay->addStretch(2);

	setVisibleProcessings(m_data->visibleProcessings);
	setHiddenProcessings(m_data->hiddenProcessings);

	Q_EMIT processingsChanged();

	return lay->count() > 1;
}

QList<VipProcessingObject*> VipMultiProcessingObjectEditor::processingObjects() const
{
	return m_data->processingObjects;
}

VipUniqueProcessingObjectEditor* VipMultiProcessingObjectEditor::processingEditor(VipProcessingObject* obj) const
{
	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = m_data->editors.find(obj);
	if (it != m_data->editors.end()) {
		if (VipUniqueProcessingObjectEditor* ed = qobject_cast<VipUniqueProcessingObjectEditor*>(it.value().second)) //->findChild<VipUniqueProcessingObjectEditor*>())
			return ed;
	}
	return nullptr;
}

void VipMultiProcessingObjectEditor::setProcessingObjectVisible(VipProcessingObject* object, bool visible)
{
	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = m_data->editors.find(object);
	if (it != m_data->editors.end()) {
		it->first->showButton()->setChecked(visible);
		it->second->setVisible(visible);
		emitEditorVisibilityChanged();
	}
}

void VipMultiProcessingObjectEditor::setFullEditorVisible(VipProcessingObject* object, bool visible)
{
	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = m_data->editors.find(object);
	if (it != m_data->editors.end()) {
		it->first->setVisible(visible);
		it->second->setVisible(visible);
		emitEditorVisibilityChanged();
	}
}

void VipMultiProcessingObjectEditor::setShowExactProcessingOnly(bool exact_proc)
{
	m_data->isShowExactProcessingOnly = exact_proc;
	for (QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = m_data->editors.begin(); it != m_data->editors.end(); ++it) {
		if (VipUniqueProcessingObjectEditor* ed = qobject_cast<VipUniqueProcessingObjectEditor*>(it.value().second)) //->findChild<VipUniqueProcessingObjectEditor*>())
			return ed->setShowExactProcessingOnly(exact_proc);
	}
}

bool VipMultiProcessingObjectEditor::isShowExactProcessingOnly() const
{
	return m_data->isShowExactProcessingOnly;
}

static bool isSuperClass(const QMetaObject* meta, const QMetaObject* super_class)
{
	while (meta) {
		if (meta == super_class)
			return true;
		meta = meta->superClass();
	}
	return false;
}
static bool isSuperClass(const QMetaObject* meta, QList<const QMetaObject*>& super_classes)
{
	for (int i = 0; i < super_classes.size(); ++i) {
		if (isSuperClass(meta, super_classes[i]))
			return true;
	}
	return false;
}

void VipMultiProcessingObjectEditor::updateEditorsVisibility()
{
	bool no_rules = m_data->visibleProcessings.isEmpty() && m_data->hiddenProcessings.isEmpty();
	bool changed = false;
	for (QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = m_data->editors.begin(); it != m_data->editors.end(); ++it) {
		bool visible = true;
		if (!no_rules) {
			if (!m_data->visibleProcessings.isEmpty())
				visible = isSuperClass(it.key()->metaObject(), m_data->visibleProcessings);
			if (visible && !m_data->hiddenProcessings.isEmpty())
				visible = !isSuperClass(it.key()->metaObject(), m_data->hiddenProcessings);
		}
		changed = (visible != it->first->isVisible());
		it->first->setVisible(visible);
		it->second->setVisible(visible);
	}
	if (changed)
		emitEditorVisibilityChanged();
}

void VipMultiProcessingObjectEditor::setVisibleProcessings(const QList<const QMetaObject*>& proc_classes)
{
	m_data->visibleProcessings = proc_classes;
	updateEditorsVisibility();
}

void VipMultiProcessingObjectEditor::setHiddenProcessings(const QList<const QMetaObject*>& proc_classes)
{
	m_data->hiddenProcessings = proc_classes;
	updateEditorsVisibility();
}

QList<const QMetaObject*> VipMultiProcessingObjectEditor::visibleProcessings() const
{
	return m_data->visibleProcessings;
}
QList<const QMetaObject*> VipMultiProcessingObjectEditor::hiddenProcessings() const
{
	return m_data->hiddenProcessings;
}

#include <QPair>
#include <QPointer>

typedef QPair<VipMultiProcessingObjectEditor*, QPointer<VipProcessingObject>> editor_type;

class VipProcessingEditorToolWidget::PrivateData
{
public:
	VipMainWindow* mainWindow;
	VipProcessingLeafSelector* leafSelector;
	QMap<VipProcessingObject*, editor_type> editors;
	QVBoxLayout* layout;
	editor_type current_editor;
	QPointer<VipAbstractPlayer> player;

	bool isShowExactProcessingOnly;
	QList<const QMetaObject*> visibleProcessings;
	QList<const QMetaObject*> hiddenProcessings;

	VipMultiProcessingObjectEditor* findEditor(VipProcessingObject* obj)
	{
		if (editors.size() == 0)
			return nullptr;

		// first, remove null objects
		for (QMap<VipProcessingObject*, editor_type>::iterator it = editors.begin(); it != editors.end(); ++it) {
			if (!it.value().second)
				it = editors.erase(it);
			if (it == editors.end())
				break;
		}

		QMap<VipProcessingObject*, editor_type>::iterator it = editors.find(obj);
		if (it == editors.end())
			return nullptr;
		return it.value().first;
	}

	void setEditor(VipProcessingObject* obj, VipMultiProcessingObjectEditor* edit = nullptr)
	{
		// remove the current editor from the layout
		if (layout->count()) {
			QLayoutItem* item = layout->takeAt(0);
			item->widget()->hide();
		}

		// remove any preious editor for this object
		QMap<VipProcessingObject*, editor_type>::iterator it = editors.find(obj);
		if (it != editors.end()) {
			if (edit) {
				delete it.value().first;
				it->first = edit;
			}
			else
				edit = it->first;
		}
		else
			editors[obj] = editor_type(edit, obj);

		layout->addWidget(edit);
		edit->show();
		current_editor = editor_type(edit, obj);
	}

	PrivateData()
	  : current_editor(nullptr, nullptr)
	  , isShowExactProcessingOnly(true)
	{
	}
};

VipProcessingEditorToolWidget::VipProcessingEditorToolWidget(VipMainWindow* window)
  : VipToolWidgetPlayer(window)
{
	qRegisterMetaType<VipPlotItemPtr>();

	m_data = new PrivateData();
	m_data->leafSelector = new VipProcessingLeafSelector();
	m_data->mainWindow = window;

	QWidget* w = new QWidget();
	m_data->layout = new QVBoxLayout();
	m_data->layout->setContentsMargins(0, 0, 0, 0);
	w->setLayout(m_data->layout);

	QWidget* editor = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(m_data->leafSelector);
	vlay->addWidget(VipLineWidget::createHLine());
	vlay->addWidget(w);
	editor->setLayout(vlay);

	this->setWidget(editor);
	this->setWindowTitle("Edit processing");
	this->setObjectName("Edit processing");
	this->setAutomaticTitleManagement(false);

	connect(VipPlotItemManager::instance(), SIGNAL(itemClicked(VipPlotItem*, int)), this, SLOT(itemClicked(VipPlotItem*, int)), Qt::QueuedConnection);
	connect(VipPlotItemManager::instance(), SIGNAL(itemSelectionChanged(VipPlotItem*, bool)), this, SLOT(itemSelectionChangedDirect(VipPlotItem*, bool)), Qt::DirectConnection);
	connect(window->displayArea(), SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(workspaceChanged()));
	connect(m_data->leafSelector, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(setProcessingObject(VipProcessingObject*)));

	if (VipDisplayPlayerArea* area = window->displayArea()->currentDisplayPlayerArea())
		m_data->leafSelector->setProcessingPool(area->processingPool());

	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

VipProcessingEditorToolWidget::~VipProcessingEditorToolWidget()
{
	delete m_data;
}

bool VipProcessingEditorToolWidget::setPlayer(VipAbstractPlayer* player)
{
	// just for ease of use: if no processing has been selected yet, assign a processing from this player
	if (!player) {
		m_data->leafSelector->setProcessing(nullptr);
		return false;
	}
	// if (player == m_data->player)
	//	return false;

	QList<VipDisplayObject*> displays;

	// get all selected items in selection order
	QList<VipPlotItem*> items;
	if (player->plotWidget2D())
		items = vipCastItemListOrdered<VipPlotItem*>(player->plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1), QString(), 2, 2);
	for (int i = 0; i < items.size(); ++i)
		if (VipDisplayObject* d = items[i]->property("VipDisplayObject").value<VipDisplayObject*>())
			displays.append(d);

	if (displays.isEmpty()) {
		// if we are on the same player with already a valid processing object displayed and no item selected, do nothing
		if (m_data->player == player && processingObject())
			return true;
		// try to take the main display object (if any)
		if (VipDisplayObject* disp = player->mainDisplayObject())
			displays << disp;
		else
			displays = player->displayObjects();
	}
	if (displays.size()) {
		m_data->player = player;
		setProcessingObject(displays.last());
		return true;
	}

	return false;
}

void VipProcessingEditorToolWidget::setProcessingObject(VipProcessingObject* object)
{
	if (!object || object == processingObject())
		return;

	// find the best window title
	QString title;
	if (object->outputCount() == 1)
		title = object->outputAt(0)->data().name();
	else if (object->inputCount() == 1)
		title = object->inputAt(0)->probe().name();
	if (title.isEmpty()) {
		if (VipDisplayObject* disp = qobject_cast<VipDisplayObject*>(object))
			if (VipAbstractPlayer* pl =  vipFindParent<VipAbstractPlayer>(disp->widget()))
				title = QString::number(pl->parentId()) + " " + pl->QWidget::windowTitle();
	}
	if (title.isEmpty())
		title = vipSplitClassname(object->objectName());

	this->setWindowTitle("Edit processing - " + title);

	VipMultiProcessingObjectEditor* editor = m_data->findEditor(object);
	if (editor)
		m_data->setEditor(object);
	else {
		QList<VipProcessingObject*> lst;
		lst << object;
		lst += object->allSources();

		editor = new VipMultiProcessingObjectEditor();
		editor->setShowExactProcessingOnly(m_data->isShowExactProcessingOnly);
		editor->setVisibleProcessings(m_data->visibleProcessings);
		editor->setHiddenProcessings(m_data->hiddenProcessings);
		editor->setProcessingObjects(lst);

		connect(editor, SIGNAL(editorVisibilityChanged()), this, SLOT(resetSize()));
		connect(editor, SIGNAL(processingsChanged()), this, SLOT(emitProcessingsChanged()), Qt::DirectConnection);
		m_data->setEditor(object, editor);
	}

	m_data->leafSelector->setProcessingPool(object->parentObjectPool());
	m_data->leafSelector->setProcessing(object);

	emitProcessingsChanged();
	resetSize();
}

VipProcessingObject* VipProcessingEditorToolWidget::processingObject() const
{
	return m_data->current_editor.second;
}

VipMultiProcessingObjectEditor* VipProcessingEditorToolWidget::editor() const
{
	return m_data->current_editor.first;
}

VipProcessingLeafSelector* VipProcessingEditorToolWidget::leafSelector() const
{
	return m_data->leafSelector;
}

void VipProcessingEditorToolWidget::setShowExactProcessingOnly(bool exact_proc)
{
	m_data->isShowExactProcessingOnly = exact_proc;

	for (QMap<VipProcessingObject*, editor_type>::iterator it = m_data->editors.begin(); it != m_data->editors.end(); ++it) {
		it.value().first->setShowExactProcessingOnly(exact_proc);
	}
}

bool VipProcessingEditorToolWidget::isShowExactProcessingOnly() const
{
	return m_data->isShowExactProcessingOnly;
}

void VipProcessingEditorToolWidget::setVisibleProcessings(const QList<const QMetaObject*>& proc_class_names)
{
	m_data->visibleProcessings = proc_class_names;

	for (QMap<VipProcessingObject*, editor_type>::iterator it = m_data->editors.begin(); it != m_data->editors.end(); ++it) {
		it.value().first->setVisibleProcessings(proc_class_names);
	}
}
void VipProcessingEditorToolWidget::setHiddenProcessings(const QList<const QMetaObject*>& proc_class_names)
{
	m_data->hiddenProcessings = proc_class_names;

	for (QMap<VipProcessingObject*, editor_type>::iterator it = m_data->editors.begin(); it != m_data->editors.end(); ++it) {
		it.value().first->setHiddenProcessings(proc_class_names);
	}
}

QList<const QMetaObject*> VipProcessingEditorToolWidget::visibleProcessings() const
{
	return m_data->visibleProcessings;
}
QList<const QMetaObject*> VipProcessingEditorToolWidget::hiddenProcessings() const
{
	return m_data->hiddenProcessings;
}

void VipProcessingEditorToolWidget::setPlotItem(VipPlotItem* item)
{
	if (item && item->isSelected() && isVisible()) {
		VipDisplayObject* display = item->property("VipDisplayObject").value<VipDisplayObject*>();
		if (!display) {
			// special case: VipPlotShape
			if (VipPlotShape* sh = qobject_cast<VipPlotShape*>(item))
				if (VipPlotSceneModel* psm = sh->property("VipPlotSceneModel").value<VipPlotSceneModel*>())
					display = psm->property("VipDisplayObject").value<VipDisplayObject*>();
		}
		setProcessingObject(display);
		this->setWindowTitle("Edit processing - " + item->title().text());
	}
}

void VipProcessingEditorToolWidget::itemSelectionChangedDirect(VipPlotItem* item, bool selected)
{
	QMetaObject::invokeMethod(this, "itemSelectionChanged", Qt::QueuedConnection, Q_ARG(VipPlotItemPtr, VipPlotItemPtr(item)), Q_ARG(bool, selected));
}

void VipProcessingEditorToolWidget::itemSelectionChanged(VipPlotItemPtr item, bool)
{
	setPlotItem(item);
}

void VipProcessingEditorToolWidget::itemClicked(VipPlotItem* item, int button)
{
	// bool selected = item->isSelected();
	// bool visible =  isVisible();
	VipDisplayObject* display = item->property("VipDisplayObject").value<VipDisplayObject*>();

	if (item && button == VipPlotItem::LeftButton && display && isVisible()) {
		setProcessingObject(display);
		this->setWindowTitle("Edit processing - " + item->title().text());
	}
}

void VipProcessingEditorToolWidget::workspaceChanged()
{
	if (!m_data->mainWindow)
		m_data->mainWindow = vipGetMainWindow();

	if (VipDisplayPlayerArea* area = m_data->mainWindow->displayArea()->currentDisplayPlayerArea())
		m_data->leafSelector->setProcessingPool(area->processingPool());
}

VipProcessingEditorToolWidget* vipGetProcessingEditorToolWidget(VipMainWindow* window)
{
	static VipProcessingEditorToolWidget* instance = new VipProcessingEditorToolWidget(window);
	return instance;
}

#include <QBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>

class VipDeviceChoiceDialog::PrivateData
{
public:
	QLabel text;
	QTreeWidget tree;
	QList<VipIODevice*> devices;
};

VipDeviceChoiceDialog::VipDeviceChoiceDialog(QWidget* parent)
  : QDialog(parent)
{
	m_data = new PrivateData();

	QVBoxLayout* tree_lay = new QVBoxLayout();
	tree_lay->addWidget(&m_data->text);
	tree_lay->addWidget(&m_data->tree);

	m_data->tree.setItemsExpandable(false);
	m_data->tree.setRootIsDecorated(false);
	m_data->tree.setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_data->tree.setHeaderLabels(QStringList() << "Name"
						   << "Description"
						   << "Extensions");
	m_data->text.setText("Several devices can handle this format. Select one type of device to handle it.");
	m_data->text.setWordWrap(true);

	m_data->tree.header()->resizeSection(0, 200);
	m_data->tree.header()->resizeSection(1, 200);
	m_data->tree.header()->resizeSection(2, 120);

	m_data->tree.setMinimumHeight(50);
	m_data->tree.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	this->setMinimumHeight(50);
	this->setMinimumWidth(400);

	// this->setWindowFlags(this->windowFlags()|Qt::Tool|/*Qt::WindowStaysOnTopHint|*/Qt::CustomizeWindowHint|Qt::WindowCloseButtonHint);

	QFrame* frame = new QFrame(this);
	// frame->setFrameShape(QFrame::Box);
	// frame->setFrameShadow(QFrame::Plain);

	QPushButton* ok = new QPushButton("Ok", this);
	ok->setMaximumWidth(70);
	QPushButton* cancel = new QPushButton("Cancel", this);
	cancel->setMaximumWidth(70);

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addStretch(1);
	lay->addWidget(ok);
	lay->addWidget(cancel);
	lay->addStretch(1);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(tree_lay);
	vlay->addLayout(lay);
	frame->setLayout(vlay);

	QVBoxLayout* final_lay = new QVBoxLayout();
	final_lay->setContentsMargins(0, 0, 0, 0);
	final_lay->addWidget(frame);
	setLayout(final_lay);

	connect(&m_data->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(accept()));
	connect(ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
	setWindowTitle("Select device");

	this->style()->unpolish(this);
	this->style()->polish(this);
}

VipDeviceChoiceDialog::~VipDeviceChoiceDialog()
{
	delete m_data;
}

void VipDeviceChoiceDialog::setChoices(const QList<VipIODevice*>& devices)
{
	m_data->devices = devices;
	m_data->tree.clear();
	for (int i = 0; i < devices.size(); ++i) {
		QString name = vipSplitClassname(devices[i]->className());

		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, name);
		item->setText(1, devices[i]->description());
		item->setToolTip(1, devices[i]->description());
		item->setText(2, devices[i]->fileFilters());
		item->setToolTip(2, devices[i]->fileFilters());
		m_data->tree.addTopLevelItem(item);
	}

	m_data->tree.setCurrentItem(m_data->tree.topLevelItem(0));
}

void VipDeviceChoiceDialog::setPath(const QString& path)
{
	m_data->text.setText("Several devices can handle this format. Select one type of device to handle it.<br>"
			     "<b>Path:</b>" +
			     path);
}

VipIODevice* VipDeviceChoiceDialog::selection() const
{
	QList<int> indexes;
	for (int i = 0; i < m_data->tree.topLevelItemCount(); ++i)
		if (m_data->tree.topLevelItem(i)->isSelected())
			return m_data->devices[i];

	return nullptr;
}

VipIODevice* VipCreateDevice::create(const QList<VipProcessingObject::Info>& dev, const VipPath& path, bool show_device_options)
{
	QList<VipIODevice*> devices;

	// first , create the list of devices
	for (int i = 0; i < dev.size(); ++i) {
		if (VipIODevice* d = qobject_cast<VipIODevice*>(dev[i].create())) {
			// if (d->probe(path))
			devices << d;
			// else
			//	delete d;
		}
	}

	VipIODevice* result = nullptr;

	if (devices.size() > 1) {
		// create the dialog used to use the device
		VipDeviceChoiceDialog* dialog = new VipDeviceChoiceDialog(vipGetMainWindow());
		dialog->setMinimumWidth(500);
		dialog->setChoices(devices);
		dialog->setPath(path.canonicalPath());
		if (dialog->exec() == QDialog::Accepted) {
			result = dialog->selection();
		}

		delete dialog;
		if (!result)
			return nullptr;
	}
	else if (devices.size() == 1)
		result = devices.first();
	else
		return nullptr;

	if (!path.isEmpty()) {
		result->setPath(path.canonicalPath());
		result->setMapFileSystem(path.mapFileSystem());
	}

	// display device option widget
	if (show_device_options) {

		const auto lst = vipFDObjectEditor().exactMatch(result);
		if (lst.size()) {

			QWidget* editor = lst.first()(result).value<QWidget*>();
			if (editor) {
				VipGenericDialog dialog(editor, "Device options", vipGetMainWindow());
				if (dialog.exec() != QDialog::Accepted) {
					delete result;
					return nullptr;
				}
				else {
					// try to find the "apply" slot and call it
					if (editor->metaObject()->indexOfMethod("apply()") >= 0)
						QMetaObject::invokeMethod(editor, "apply");
				}
			}
		}
	}

	return result;
}

VipIODevice* VipCreateDevice::create(const VipPath& path, bool show_device_options)
{
	QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(path, QByteArray());
	return create(devices, path, show_device_options);
}
