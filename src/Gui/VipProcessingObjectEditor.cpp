/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include "VipQuiver.h"
#include "VipStandardProcessing.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"
#include "VipTimer.h"
#include "VipXmlArchive.h"
#include "VipSet.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QGraphicsSceneMouseEvent>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QRadioButton>
#include <QToolButton>
#include <qtimer.h>

#include <vector>

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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->tdisplays.setText("Operation on data:");

	d_data->lineBefore = VipLineWidget::createHLine();
	d_data->lineAfter = VipLineWidget::createHLine();

	d_data->players.setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
	d_data->players.setMaximumWidth(200);
	d_data->displays.setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
	d_data->displays.setMaximumWidth(200);

	QGridLayout* lay = new QGridLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->lineBefore, 0, 0, 1, 2);
	lay->addWidget(new QLabel("Dynamic operation:"), 1, 0);
	lay->addWidget(&d_data->dynamic, 1, 1);
	lay->addWidget(new QLabel("Operation on player:"), 2, 0);
	lay->addWidget(&d_data->players, 2, 1);
	lay->addWidget(&d_data->tdisplays, 3, 0);
	lay->addWidget(&d_data->displays, 3, 1);
	lay->addWidget(d_data->lineAfter, 4, 0, 1, 2);

	d_data->tdisplays.hide();
	d_data->displays.hide();

	d_data->dynamic.setToolTip("If checked, the operation will be performed on the current image from the selected player.<br>"
				   "Otherwise the operation will always be performed on the same data (image or curve). You can reset this processing to change the data.");
	d_data->players.setToolTip("Apply the operation on selected player: add, subtract, multiply or divide this data (image or curve) with the selected player's data.");
	d_data->displays.setToolTip("<b>There are several items in this player</b><br>Select the item to apply the operation on");
	setLayout(lay);

	connect(&d_data->players, SIGNAL(openPopup()), this, SLOT(showPlayers()));
	connect(&d_data->dynamic, SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(&d_data->players, SIGNAL(activated(int)), this, SLOT(apply()));
	connect(&d_data->displays, SIGNAL(openPopup()), this, SLOT(showDisplays()));
	connect(&d_data->displays, SIGNAL(activated(int)), this, SLOT(apply()));
}

VipOtherPlayerDataEditor::~VipOtherPlayerDataEditor()
{
}
// void VipOtherPlayerDataEditor::displayVLines(bool before, bool after)
// {
// d_data->lineBefore->setVisible(before);
// d_data->lineAfter->setVisible(after);
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

	d_data->players.blockSignals(true);
	d_data->players.clear();
	QWidget* w = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (w) {

		int current_index = -1;
		int current_id = d_data->data.otherPlayerId();

		// compute the list of all VipVideoPlayer in the current workspace
		QList<VipPlayer2D*> instances = w->findChildren<VipPlayer2D*>();

		// remove the player containing this VipOperationBetweenPlayers

		std::vector<player_id> players;
		for (int i = 0; i < instances.size(); ++i) {
			VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(instances[i]);
			QString title = parent ? parent->windowTitle() : instances[i]->windowTitle();
			// d_data->players.addItem(title, VipUniqueId::id(instances[i]));
			players.push_back(player_id(VipUniqueId::id(instances[i]), title));
			if (current_id == VipUniqueId::id(instances[i]))
				current_index = d_data->players.count() - 1;
		}
		std::sort(players.begin(), players.end());
		for (size_t i = 0; i < players.size(); ++i) {
			d_data->players.addItem(players[i].title, players[i].id);
			if (current_id == players[i].id)
				current_index = d_data->players.count() - 1;
		}
		if (current_index >= 0)
			d_data->players.setCurrentIndex(current_index);
	}
	d_data->players.blockSignals(false);
}

void VipOtherPlayerDataEditor::showDisplays()
{
	d_data->displays.clear();
	QVariant player = d_data->players.currentData();
	if (player.userType() != QMetaType::Int)
		return;

	int current_id = d_data->data.otherDisplayIndex();
	int current_index = -1;

	d_data->displays.blockSignals(true);
	VipPlayer2D* pl = VipUniqueId::find<VipPlayer2D>(player.toInt());
	if (pl) {
		QList<VipDisplayObject*> displays = pl->displayObjects();
		for (int i = 0; i < displays.size(); ++i) {
			QString text = displays[i]->title();
			d_data->displays.addItem(text, VipUniqueId::id(displays[i]));
			if (VipUniqueId::id(displays[i]) == current_id)
				current_index = d_data->displays.count() - 1;
		}

		d_data->displays.setVisible(displays.size() > 1);
		d_data->tdisplays.setVisible(displays.size() > 1);
	}
	if (current_index >= 0)
		d_data->displays.setCurrentIndex(current_index);

	d_data->displays.blockSignals(false);
}

VipOtherPlayerData VipOtherPlayerDataEditor::value() const
{
	return d_data->data;
}

void VipOtherPlayerDataEditor::setValue(const VipOtherPlayerData& _data)
{
	d_data->data = _data;

	d_data->dynamic.blockSignals(true);
	d_data->dynamic.setChecked(_data.isDynamic());
	d_data->dynamic.blockSignals(false);
	showPlayers();
	showDisplays();

	apply();
}

void VipOtherPlayerDataEditor::apply()
{

	QVariant player = d_data->players.currentData();
	if (player.userType() != QMetaType::Int)
		return;

	VipPlayer2D* pl = VipUniqueId::find<VipPlayer2D>(player.toInt());
	if (pl) {
		VipDisplayObject* display = nullptr;
		QList<VipDisplayObject*> displays = pl->displayObjects();
		if (displays.size() > 1) {
			display = VipUniqueId::find<VipDisplayObject>(d_data->displays.currentData().toInt());
			if (!display)
				showDisplays();
		}
		else if (displays.size() == 1)
			display = displays.front();

		d_data->displays.setVisible(displays.size() > 1);
		d_data->tdisplays.setVisible(displays.size() > 1);

		if (display) {
			// set all properties
			VipOutput* out = display->inputAt(0)->connection()->source();
			VipProcessingObject* proc = out->parentProcessing();
			bool isDynamic = d_data->dynamic.isChecked();
			// bool isSame = (isDynamic == d_data->data.isDynamic() && proc == d_data->data.processing() && proc->indexOf(out) == d_data->data.outputIndex() &&
			// d_data->data.otherPlayerId() == player.toInt() && d_data->data.otherDisplayIndex() == VipUniqueId::id(display));
			d_data->data = VipOtherPlayerData(isDynamic, proc, d_data->data.parentProcessingObject(), proc->indexOf(out), player.toInt(), VipUniqueId::id(display));

			// if (!isSame)
			Q_EMIT valueChanged(QVariant::fromValue(d_data->data));
		}
	}
}



class Vip2DDataEditor::PrivateData
{
public:
	VipOtherPlayerData data;
	QRadioButton* editArray;
	QRadioButton* editPlayer;

	QCheckBox* shouldResizeArray;

	Vip2DArrayEditor* editor;
	VipOtherPlayerDataEditor* player;

	QWidget* lineBefore;
	QWidget* lineAfter;
};

Vip2DDataEditor::Vip2DDataEditor()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->editArray = new QRadioButton("Create a 1D/2D array");
	d_data->editArray->setToolTip("<b>Manually create a 1D/2D array</b><br>This is especially usefull for convolution functions.");
	d_data->editPlayer = new QRadioButton("Take the data from another player");
	d_data->editPlayer->setToolTip("<b>Selecte a data (image, curve,...) from another player</b>");
	d_data->shouldResizeArray = new QCheckBox("Resize array to the current data shape");
	d_data->shouldResizeArray->setToolTip("This usefull if you apply a processing/filter that only works on 2 arrays having the same shape.\n"
					      "Selecting this option ensures you that given image/curve will be resized to the right dimension.");
	d_data->editor = new Vip2DArrayEditor();
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

Vip2DDataEditor::~Vip2DDataEditor() {}

VipOtherPlayerData Vip2DDataEditor::value() const
{
	VipOtherPlayerData res;
	if (d_data->editArray->isChecked())
		res = VipOtherPlayerData(VipAnyData(QVariant::fromValue(d_data->editor->array()), 0));
	else
		res = d_data->player->value();
	res.setShouldResizeArray(d_data->shouldResizeArray->isChecked());
	return res;
}

void Vip2DDataEditor::setValue(const VipOtherPlayerData& data)
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

void Vip2DDataEditor::displayVLines(bool before, bool after)
{
	d_data->lineBefore->setVisible(before);
	d_data->lineAfter->setVisible(after);
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
	VIP_CREATE_PRIVATE_DATA(d_data);

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
}

VipOutput* VipFindDataButton::selectedData() const
{
	if (d_data->processing && d_data->index < d_data->processing->outputCount())
		return d_data->processing->outputAt(d_data->index);
	return nullptr;
}

void VipFindDataButton::setSelectedData(VipOutput* output)
{
	d_data->processing = output ? output->parentProcessing() : nullptr;
	d_data->index = d_data->processing ? d_data->processing->indexOf(output) : 0;
	if (!output) {
		this->setText("No data selected");
		this->setToolTip("Select a processing output");
		return;
	}

	QList<VipDisplayObject*> disp = vipListCast<VipDisplayObject*>(d_data->processing->allSinks());
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
	Q_EMIT selectionChanged(d_data->processing, d_data->index);
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
	d_data->processing = act->property("processing").value<VipProcessingObject*>();
	d_data->index = act->property("output").value<int>();
	if (d_data->processing && d_data->index < d_data->processing->outputCount()) {
		setToolTip(act->property("tool_tip").toString());
		setText(act->property("text").toString());

		Q_EMIT selectionChanged(d_data->processing, d_data->index);
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QGroupBox* box = new QGroupBox("Processing inputs");
	// box->setFlat(true);

	QVBoxLayout* blay = new QVBoxLayout();
	blay->addWidget(&d_data->inputList);
	box->setLayout(blay);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(&d_data->editor);
	vlay->addWidget(box);
	// vlay->addWidget(&d_data->inputList);
	setLayout(vlay);

	d_data->inputList.setToolTip("Setup processing inputs");
}

VipEditDataFusionProcessing::~VipEditDataFusionProcessing()
{
}

void VipEditDataFusionProcessing::setDataFusionProcessing(VipBaseDataFusion* p)
{
	// set the processing and update the input list accordingly

	d_data->editor.setProcessingObject(p);
	d_data->processing = p;

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

	d_data->inputList.clear();
	for (int i = 0; i < inputs.size(); ++i) {
		QListWidgetItem* item = new QListWidgetItem(&d_data->inputList);
		item->setSizeHint(inputs[i]->sizeHint());
		d_data->inputList.addItem(item);
		d_data->inputList.setItemWidget(item, inputs[i]);
	}
}

VipBaseDataFusion* VipEditDataFusionProcessing::dataFusionProcessing() const
{
	return d_data->processing;
}

int VipEditDataFusionProcessing::indexOfInput(QObject* obj)
{
	for (int i = 0; i < d_data->inputList.count(); ++i) {
		if (&static_cast<InputWidget*>(d_data->inputList.itemWidget(d_data->inputList.item(i)))->add == obj ||
		    &static_cast<InputWidget*>(d_data->inputList.itemWidget(d_data->inputList.item(i)))->remove == obj)
			return i;
	}
	return 0;
}

void VipEditDataFusionProcessing::addInput()
{
	// add input and reset processing
	if (d_data->processing)
		d_data->processing->topLevelInputAt(0)->toMultiInput()->insert(indexOfInput(sender()) + 1);
	setDataFusionProcessing(d_data->processing);
}
void VipEditDataFusionProcessing::removeInput()
{
	// remove input and reset processing
	if (d_data->processing)
		d_data->processing->topLevelInputAt(0)->toMultiInput()->removeAt(indexOfInput(sender()));
	setDataFusionProcessing(d_data->processing);
}

void VipEditDataFusionProcessing::updateProcessing()
{
	// get the list of VipOutput
	QList<VipOutput*> outputs;
	for (int i = 0; i < d_data->inputList.count(); ++i) {
		InputWidget* input = static_cast<InputWidget*>(d_data->inputList.itemWidget(d_data->inputList.item(i)));
		if (VipOutput* out = input->data.selectedData()) {
			outputs.append(out);
		}
		else
			return;
	}
	// set the outputs to the processing inputs
	if (d_data->processing) {
		d_data->processing->topLevelInputAt(0)->toMultiInput()->resize(outputs.size());
		for (int i = 0; i < outputs.size(); ++i) {
			d_data->processing->inputAt(i)->setConnection(outputs[i]);
			d_data->processing->inputAt(i)->buffer()->clear();
			d_data->processing->inputAt(i)->setData(outputs[i]->data());
		}
	}
	d_data->processing->reload();
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->oneInput);
	lay->addWidget(&d_data->multiInput);
	lay->addWidget(&d_data->enable);
	lay->setSpacing(0);
	setLayout(lay);

	d_data->oneInput.setText("Run when at least one input is new");
	d_data->oneInput.setToolTip("The processing will be triggered at each new input data");
	d_data->multiInput.setText("Run when all inputs are new");
	d_data->multiInput.setToolTip("The processing will be triggered when all input data are new");
	d_data->enable.setText("Enable the processing");

	connect(&d_data->oneInput, SIGNAL(clicked(bool)), this, SLOT(updateProcessingObject()));
	connect(&d_data->multiInput, SIGNAL(clicked(bool)), this, SLOT(updateProcessingObject()));
	connect(&d_data->enable, SIGNAL(clicked(bool)), this, SLOT(updateProcessingObject()));
}

VipProcessingObjectEditor::~VipProcessingObjectEditor()
{
}

void VipProcessingObjectEditor::setProcessingObject(VipProcessingObject* obj)
{
	d_data->proc = obj;
	if (obj) {
		d_data->oneInput.blockSignals(true);
		d_data->multiInput.blockSignals(true);
		d_data->enable.blockSignals(true);

		d_data->multiInput.setChecked(obj->testScheduleStrategy(VipProcessingObject::AllInputs));
		d_data->enable.setChecked(obj->isEnabled());

		d_data->oneInput.blockSignals(false);
		d_data->multiInput.blockSignals(false);
		d_data->enable.blockSignals(false);
	}
}

void VipProcessingObjectEditor::updateProcessingObject()
{
	if (d_data->proc) {
		d_data->proc->setEnabled(d_data->enable.isChecked());
		d_data->proc->setScheduleStrategy(VipProcessingObject::AllInputs, d_data->multiInput.isChecked());
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->openRead);
	lay->addWidget(&d_data->openWrite);
	lay->addWidget(&d_data->info);
	lay->setSpacing(0);
	setLayout(lay);

	d_data->openRead.setText("Open the device in Read mode");
	d_data->openRead.setToolTip("Open/close the device in Read mode");
	d_data->openWrite.setText("Open the device in Write mode");
	d_data->openWrite.setToolTip("Open/close the device in Write mode");

	connect(&d_data->openRead, SIGNAL(clicked(bool)), this, SLOT(updateIODevice()));
	connect(&d_data->openWrite, SIGNAL(clicked(bool)), this, SLOT(updateIODevice()));
}

VipIODeviceEditor::~VipIODeviceEditor()
{
}

void VipIODeviceEditor::setIODevice(VipIODevice* obj)
{
	d_data->device = obj;
	if (obj) {
		d_data->openRead.blockSignals(true);
		d_data->openWrite.blockSignals(true);

		d_data->openRead.setChecked(obj->openMode() & VipIODevice::ReadOnly);
		d_data->openWrite.setChecked(obj->openMode() & VipIODevice::WriteOnly);
		d_data->openRead.setVisible(obj->supportedModes() & VipIODevice::ReadOnly);
		d_data->openWrite.setVisible(obj->supportedModes() & VipIODevice::WriteOnly);

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

		d_data->info.setText(text.join("<br>"));
		d_data->info.setToolTip(text.join("<br>"));
		d_data->openRead.blockSignals(false);
		d_data->openWrite.blockSignals(false);
	}
}

void VipIODeviceEditor::updateIODevice()
{
	if (d_data->device) {
		if (d_data->openRead.isChecked() && !(d_data->device->openMode() & VipIODevice::ReadOnly)) {
			d_data->device->close();
			d_data->device->open(VipIODevice::ReadOnly);
		}
		else if (!d_data->openRead.isChecked() && (d_data->device->openMode() & VipIODevice::ReadOnly))
			d_data->device->close();

		if (d_data->openWrite.isChecked() && !(d_data->device->openMode() & VipIODevice::WriteOnly)) {
			d_data->device->close();
			d_data->device->open(VipIODevice::WriteOnly);
		}
		else if (!d_data->openWrite.isChecked() && (d_data->device->openMode() & VipIODevice::WriteOnly))
			d_data->device->close();
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
		// QListWidgetItem * item = itemAt(evt->VIP_EVT_POSITION());
		// if(item)
		// {
		// QRect rect = this->visualItemRect(item);
		// insertPos = this->indexFromItem(item).row();
		// if(evt->VIP_EVT_POSITION().y() > rect.center().y())
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
		QListWidgetItem* item = itemAt(evt->VIP_EVT_POSITION());
		if (item) {
			QRect rect = this->visualItemRect(item);
			insertPos = this->indexFromItem(item).row();
			if (evt->VIP_EVT_POSITION().y() > rect.center().y())
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			menu.exec(evt->screenPos().toPoint());
#else
			menu.exec(evt->globalPosition().toPoint());
#endif
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->editor = new VipUniqueProcessingObjectEditor();
	d_data->editor->setShowExactProcessingOnly(true);

	d_data->user_types = vipUserTypes();
	d_data->list = new ListWidget(this);
	d_data->tree = new VipProcessingObjectMenu();
	connect(d_data->tree, SIGNAL(selected(const VipProcessingObject::Info&)), this, SLOT(addSelectedProcessing()));

	d_data->list->setDragDropMode(QAbstractItemView::InternalMove);
	d_data->list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->list->setDragDropOverwriteMode(false);
	d_data->list->setDefaultDropAction(Qt::TargetMoveAction);
	d_data->list->setToolTip("Stack of processing");

	d_data->toolBar = new QToolBar();
	d_data->toolBar->setIconSize(QSize(18, 18));

	d_data->addProcessing = new QToolButton();
	d_data->addProcessing->setAutoRaise(true);
	d_data->addProcessing->setMenu(d_data->tree);
	d_data->addProcessing->setPopupMode(QToolButton::InstantPopup);
	d_data->addProcessing->setIcon(vipIcon("PROCESSING.png"));
	d_data->addProcessing->setText("Add a processing");
	d_data->addProcessing->setToolTip("<b>Add a new processing into the processing list</b><br>The processing will be added at the end of the list. Use the mouse to change the processing order.");
	d_data->addProcessing->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d_data->addProcessing->setIconSize(QSize(18, 18));
	d_data->toolBar->addWidget(d_data->addProcessing);
	connect(d_data->addProcessing, SIGNAL(clicked(bool)), this, SLOT(updateProcessingTree()));
	connect(d_data->tree, SIGNAL(aboutToShow()), this, SLOT(updateProcessingTree()), Qt::DirectConnection);

	QAction* showList = d_data->toolBar->addAction(vipIcon("down.png"), "Show/Hide processing list");
	showList->setCheckable(true);
	showList->setChecked(true);
	connect(showList, SIGNAL(triggered(bool)), d_data->list, SLOT(setVisible(bool)));

	d_data->toolBar->addSeparator();
	QAction* copy = d_data->toolBar->addAction(vipIcon("copy.png"), "Copy selected processing");
	QAction* cut = d_data->toolBar->addAction(vipIcon("cut.png"), "Cut selected processing");
	d_data->toolBar->addSeparator();
	QAction* paste = d_data->toolBar->addAction(vipIcon("paste.png"), "Paste copied processing.\nNew processing will be inserted before the selected one");

	connect(copy, SIGNAL(triggered(bool)), this, SLOT(copySelection()));
	connect(cut, SIGNAL(triggered(bool)), this, SLOT(cutSelection()));
	connect(paste, SIGNAL(triggered(bool)), this, SLOT(pasteCopiedItems()));

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->addWidget(d_data->toolBar);
	vlay->addWidget(d_data->list);
	// TEST: add stretch factor
	vlay->addWidget(d_data->editor, 1);

	setLayout(vlay);

	connect(d_data->list, SIGNAL(itemSelectionChanged()), this, SLOT(selectedItemChanged()));
	connect(d_data->list, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)));

	d_data->timer.setSingleShot(true);
	d_data->timer.setInterval(500);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(resetProcessingList()));
}

VipProcessingListEditor::~VipProcessingListEditor()
{
	disconnect(&d_data->timer, SIGNAL(timeout()), this, SLOT(resetProcessingList()));
	d_data->timer.stop();
}

void VipProcessingListEditor::selectedItemChanged()
{
	QList<QListWidgetItem*> items = d_data->list->selectedItems();
	if (items.size()) {
		VipProcessingObject* obj = static_cast<ProcessingListWidgetItem*>(items.back())->processing;
		if (obj) {
			d_data->editor->setProcessingObject(obj);
			Q_EMIT selectionChanged(d_data->editor);
			d_data->editor->show();
			VipUniqueProcessingObjectEditor::geometryChanged(d_data->editor->parentWidget());
		}
	}
}

void VipProcessingListEditor::clearEditor()
{
	d_data->editor->setProcessingObject(nullptr);
}

void VipProcessingListEditor::resetProcessingList()
{
	// if(isVisible())
	setProcessingList(processingList());
}

void VipProcessingListEditor::updateProcessingTree()
{
	if (d_data->processingList) {
		// create a list of all VipProcessingObject that can be inserted into a VipProcessingList
		QVariantList lst;
		lst.append(d_data->processingList->inputAt(0)->probe().data());

		QList<int> current_types = vipUserTypes();
		if (d_data->infos.isEmpty() || current_types != d_data->user_types) {
			d_data->user_types = current_types;
			d_data->infos = VipProcessingObject::validProcessingObjects(lst, 1, VipProcessingObject::InputTransform).values();

			// only keep the Info with inputTransformation to true
			for (int i = 0; i < d_data->infos.size(); ++i) {
				if (d_data->infos[i].displayHint != VipProcessingObject::InputTransform) {
					d_data->infos.removeAt(i);
					--i;
				}
			}
		}

		d_data->tree->setProcessingInfos(d_data->infos);
	}
}

void VipProcessingListEditor::setProcessingList(VipProcessingList* lst)
{
	if (d_data->processingList)
		disconnect(d_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &d_data->timer, SLOT(start()));

	d_data->list->clear();
	d_data->processingList = lst;

	// clean the hidden processing list
	for (int i = 0; i < d_data->hidden.size(); ++i) {
		if (!d_data->hidden[i]) {
			d_data->hidden.removeAt(i);
			--i;
		}
	}

	if (lst) {
		this->setObjectName(lst->objectName());
		connect(d_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &d_data->timer, SLOT(start()));

		QList<VipProcessingObject*> objects = lst->processings();
		for (int i = 0; i < objects.size(); ++i) {
			d_data->list->addItem(new ProcessingListWidgetItem(objects[i]));
			if (d_data->hidden.indexOf(objects[i]) >= 0 || !objects[i]->isVisible())
				d_data->list->item(d_data->list->count() - 1)->setHidden(true);
		}
	}

	// reset the ListWidget size
	d_data->list->setMaximumHeight(d_data->list->count() * 30 + 30);
	VipUniqueProcessingObjectEditor::geometryChanged(this);

	// update the possible VipProcessingObject
	updateProcessingTree();
}

VipProcessingList* VipProcessingListEditor::processingList() const
{
	return d_data->processingList;
}

VipUniqueProcessingObjectEditor* VipProcessingListEditor::editor() const
{
	return d_data->editor;
}

QToolButton* VipProcessingListEditor::addProcessingButton() const
{
	return d_data->addProcessing;
}

QListWidget* VipProcessingListEditor::list() const
{
	return d_data->list;
}

void VipProcessingListEditor::addProcessings(const QList<VipProcessingObject::Info>& infos)
{
	if (!d_data->processingList) {
		VIP_LOG_ERROR("No processing list available");
		return;
	}
	d_data->processingList->blockSignals(true);

	QList<VipProcessingObject*> added;
	for (int i = 0; i < infos.size(); ++i) {
		VipProcessingObject* obj = infos[i].create();
		if (obj) {
			added.append(obj);
			d_data->processingList->append(obj);
		}
	}

	d_data->processingList->blockSignals(false);

	if (added.size()) {
		setProcessingList(d_data->processingList);
		d_data->processingList->reload();

		// select the added VipProcessingObject
		for (int i = 0; i < added.size(); ++i) {
			d_data->list->item(d_data->list->find(added[i]))->setSelected(true);
		}
	}

	// d_data->processingList->blockSignals(false);
	d_data->addProcessing->menu()->hide();

	// TODO: reemit signal for other editors
	disconnect(d_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &d_data->timer, SLOT(start()));
	d_data->processingList->emitProcessingChanged();
	connect(d_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &d_data->timer, SLOT(start()));
}

void VipProcessingListEditor::selectObject(VipProcessingObject* obj)
{
	d_data->list->clearSelection();
	for (int i = 0; i < d_data->list->count(); ++i) {
		ProcessingListWidgetItem* item = static_cast<ProcessingListWidgetItem*>(d_data->list->item(i));
		if (item->processing == obj) {
			// d_data->list->setItemSelected(item, true);
			item->setSelected(true);
			break;
		}
	}
}

QListWidgetItem* VipProcessingListEditor::item(VipProcessingObject* obj) const
{
	for (int i = 0; i < d_data->list->count(); ++i) {
		ProcessingListWidgetItem* item = static_cast<ProcessingListWidgetItem*>(d_data->list->item(i));
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
	QList<QListWidgetItem*> items = d_data->list->selectedItems();
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
	QList<QListWidgetItem*> items = d_data->list->selectedItems();
	copySelection();
	if (d_data->processingList)
		for (int i = 0; i < items.size(); ++i) {
			d_data->processingList->remove(static_cast<ProcessingListWidgetItem*>(items[i])->processing.data());
		}
}

void VipProcessingListEditor::pasteCopiedItems()
{
	if (d_data->processingList) {
		QList<QListWidgetItem*> items = d_data->list->selectedItems();
		int index = items.size() ? d_data->list->row(items.last()) : -1;

		QList<VipProcessingObject*> procs = copiedProcessing();
		if (index < 0) {
			for (int i = 0; i < procs.size(); ++i)
				d_data->processingList->append(procs[i]);
		}
		else {
			for (int i = 0; i < procs.size(); ++i, ++index)
				d_data->processingList->insert(index, procs[i]);
		}
	}
}

void VipProcessingListEditor::setProcessingVisible(VipProcessingObject* obj, bool visible)
{
	if (!visible && d_data->hidden.indexOf(obj) < 0)
		d_data->hidden.append(obj);
	else if (visible)
		d_data->hidden.removeOne(obj);

	for (int i = 0; i < d_data->list->count(); ++i) {
		ProcessingListWidgetItem* item = static_cast<ProcessingListWidgetItem*>(d_data->list->item(i));
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
	if (!d_data->processingList)
		return;

	QList<VipProcessingObject::Info> infos = QList<VipProcessingObject::Info>() << d_data->tree->selectedProcessingInfo();
	addProcessings(infos);
}

void VipProcessingListEditor::itemChanged(QListWidgetItem* item)
{
	ProcessingListWidgetItem* it = static_cast<ProcessingListWidgetItem*>(item);
	if (it->processing)
		it->processing->setEnabled(item->checkState() == Qt::Checked);
	if (d_data->processingList)
		d_data->processingList->reload();
}

void VipProcessingListEditor::updateProcessingList()
{
	if (!d_data->processingList)
		return;

	d_data->processingList->blockSignals(true);

	// update the VipProcessingList after a change in the ListWidget (deletetion or drag/drop)

	// first, remove all VipProcessingObject for the VipProcessingList without deleting them
	QList<VipProcessingObject*> removed;
	while (d_data->processingList->size() > 0)
		removed.append(d_data->processingList->take(0));

	// now add all items from the ListWidget
	for (int i = 0; i < d_data->list->count(); ++i) {
		d_data->processingList->append(static_cast<ProcessingListWidgetItem*>(d_data->list->item(i))->processing);
	}

	// delete orphan objects
	for (int i = 0; i < removed.size(); ++i)
		if (d_data->processingList->indexOf(removed[i]) < 0)
			delete removed[i];

	// for the update of the VipProcessingList
	d_data->processingList->update(true);

	// reset the ListWidget size
	d_data->list->setMaximumHeight(d_data->list->count() * 30 + 30);
	VipUniqueProcessingObjectEditor::geometryChanged(this);

	d_data->processingList->blockSignals(false);

	// TODO: reemit signal for other editors, but not this one
	disconnect(d_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &d_data->timer, SLOT(start()));
	d_data->processingList->emitProcessingChanged();
	connect(d_data->processingList, SIGNAL(processingChanged(VipProcessingObject*)), &d_data->timer, SLOT(start()));
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&d_data->method);
	lay->addLayout(d_data->procsLayout = new QHBoxLayout());
	d_data->procsLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	d_data->methods = new QMenu(&d_data->method);
	d_data->method.setMenu(d_data->methods);
	d_data->method.setPopupMode(QToolButton::InstantPopup);
	d_data->method.setToolTip("<b>Select the split method</b><br>The input data will be splitted in several components according to given method."
				  "You can then add different processings for each component. Each component will be merged back to construct the output data.");

	connect(d_data->methods, SIGNAL(aboutToShow()), this, SLOT(computeMethods()));
	connect(d_data->methods, SIGNAL(triggered(QAction*)), this, SLOT(newMethod(QAction*)));
}

VipSplitAndMergeEditor::~VipSplitAndMergeEditor()
{
}

void VipSplitAndMergeEditor::computeMethods()
{
	setProcessing(processing());
}

void VipSplitAndMergeEditor::setProcessing(VipSplitAndMerge* proc)
{
	d_data->proc = proc;
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
		d_data->methods->blockSignals(true);
		d_data->methods->clear();
		for (int i = 0; i < methods.size(); ++i) {
			QAction* act = d_data->methods->addAction(methods[i]);
			act->setCheckable(true);
			if (methods[i] == proc->method())
				act->setChecked(true);
		}
		if (proc->method().isEmpty()) {
			d_data->method.setText("No splitting/merging applied");
		}
		else
			d_data->method.setText(proc->method());
		d_data->methods->blockSignals(false);

		// if (methods.size() && methods.indexOf(proc->method()) < 0)
		// {
		// //the proc does not have a method set, set it!
		// proc->setMethod(methods.first())
		// }

		// create the editors
		if (d_data->editors.size() != proc->componentCount()) {
			// delete all editors
			for (int i = 0; i < d_data->editors.size(); ++i)
				delete d_data->editors[i];
			d_data->editors.clear();

			// delete all proc editors
			for (int i = 0; i < d_data->procEditors.size(); ++i)
				delete d_data->procEditors[i];
			d_data->procEditors.clear();

			for (int i = 0; i < proc->componentCount(); ++i) {
				d_data->editors.append(new VipProcessingListEditor());
				d_data->procsLayout->addWidget(d_data->editors.back());

				d_data->procEditors.append(d_data->editors.back()->editor());
				this->layout()->addWidget(d_data->procEditors.back());
				d_data->procEditors.back()->hide();

				connect(d_data->editors.back(), SIGNAL(selectionChanged(VipUniqueProcessingObjectEditor*)), this, SLOT(receiveSelectionChanged(VipUniqueProcessingObjectEditor*)));
			}
		}

		// customize editors
		QStringList components = proc->components();
		if (components.size() == proc->componentCount()) {
			for (int i = 0; i < d_data->editors.size(); ++i) {
				d_data->editors[i]->addProcessingButton()->setText(components[i]);
				d_data->editors[i]->addProcessingButton()->setToolTip("Add processing for '" + components[i] + "' component'");
			}

			// set the processing list
			for (int i = 0; i < d_data->editors.size(); ++i)
				d_data->editors[i]->setProcessingList(proc->componentProcessings(i));
		}
	}
}

VipSplitAndMerge* VipSplitAndMergeEditor::processing() const
{
	return d_data->proc;
}

void VipSplitAndMergeEditor::newMethod(QAction* act)
{
	QList<QAction*> actions = d_data->methods->actions();
	// uncheck other actions
	d_data->methods->blockSignals(true);
	for (int i = 0; i < actions.size(); ++i)
		actions[i]->setChecked(act == actions[i]);
	d_data->methods->blockSignals(false);
	d_data->method.setText(act->text());

	// set the processing method, and reset the processing to update the display
	if (d_data->proc) {
		d_data->proc->setMethod(act->text());
		setProcessing(d_data->proc);
	}
}

void VipSplitAndMergeEditor::receiveSelectionChanged(VipUniqueProcessingObjectEditor* ed)
{
	// a processing has been selected in on of the editors

	for (int i = 0; i < d_data->procEditors.size(); ++i) {
		// hide all processing editors except the one that is shown
		d_data->procEditors[i]->setVisible(d_data->procEditors[i] == ed);
	}

	if (VipProcessingListEditor* editor = qobject_cast<VipProcessingListEditor*>(sender())) {
		// unselect all rpocessing for all other VipProcessingListEditor
		for (int i = 0; i < d_data->editors.size(); ++i) {
			if (d_data->editors[i] != editor) {
				QListWidget* list = d_data->editors[i]->list();
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->components.setToolTip("Select the component to extract");
	d_data->components.setEditable(false);
	// d_data->components.QComboBox::setEditText("Invariant");
	d_data->components.setSizeAdjustPolicy(QComboBox::AdjustToContents);
	d_data->components.setMinimumWidth(100);
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&d_data->components);
	hlay->setContentsMargins(0, 0, 0, 0);
	setLayout(hlay);

	connect(&d_data->components, SIGNAL(currentTextChanged(const QString&)), this, SLOT(updateExtractComponent()));
	connect(&d_data->components, SIGNAL(openPopup()), this, SLOT(updateComponentChoice()));
}

VipExtractComponentEditor::~VipExtractComponentEditor()
{
}

void VipExtractComponentEditor::setExtractComponent(VipExtractComponent* extract)
{
	if (d_data->extractComponent) {
		disconnect(d_data->extractComponent, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(extractComponentChanged()));
	}

	d_data->extractComponent = extract;
	if (extract) {
		QString comp = d_data->extractComponent->propertyAt(0)->data().value<QString>();

		// TEST: remove Invariant
		// if (comp.isEmpty())
		//	comp = "Invariant";
		updateComponentChoice();
		d_data->components.blockSignals(true);
		if (d_data->components.findText(comp) < 0) {
			d_data->components.addItem(comp);
		}
		d_data->components.setCurrentText(comp);
		d_data->components.blockSignals(false);
		connect(extract, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(extractComponentChanged()));
	}
}

int VipExtractComponentEditor::componentCount() const
{
	return d_data->components.count();
}

QComboBox* VipExtractComponentEditor::choices() const
{
	return &d_data->components;
}

void VipExtractComponentEditor::updateComponentChoice()
{
	if (d_data->extractComponent) {
		d_data->components.blockSignals(true);

		QStringList components = d_data->extractComponent->supportedComponents();
		if (components != d_data->components.items()) {
			// update the components

			d_data->components.clear();
			d_data->components.addItems(components);
			// set the default component
			QString default_component = d_data->extractComponent->defaultComponent();
			if (default_component.isEmpty()) {
				d_data->components.setCurrentIndex(0);
			}
			else
				d_data->components.setCurrentText(d_data->extractComponent->defaultComponent());
		}

		QString comp = d_data->extractComponent->propertyAt(0)->data().value<QString>();
		// TEST: remove Invariant
		if (!components.size() && (comp.isEmpty() //|| comp == "Invariant"
					   )) {
			if (d_data->components.isVisible())
				d_data->components.hide();
		}
		else if (d_data->components.isHidden()) {
			d_data->components.show();
		}

		if (!comp.isEmpty()) {
			if (comp != d_data->components.currentText()) {
				d_data->components.setCurrentText(comp);
			}
		}
		else {
			comp = d_data->components.currentText();
			d_data->extractComponent->propertyAt(0)->setData(comp);
		}

		d_data->components.blockSignals(false);
	}
}

void VipExtractComponentEditor::updateExtractComponent()
{
	if (d_data->extractComponent) {
		int index = d_data->components.currentIndex();
		QStringList choices = d_data->extractComponent->supportedComponents();
		if (index < choices.size()) {
			// set the new component
			d_data->extractComponent->propertyAt(0)->setData(choices[index]);
			d_data->extractComponent->reload();
			Q_EMIT componentChanged(choices[index]);
		}
	}
}

void VipExtractComponentEditor::extractComponentChanged()
{
	QString comp = d_data->extractComponent->propertyAt(0)->data().value<QString>();
	d_data->components.blockSignals(true);
	d_data->components.setCurrentText(comp);
	d_data->components.blockSignals(false);
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->types.setToolTip("Select the output type");
	d_data->types.setEditable(false);

	for (QMap<int, QPair<int, QString>>::const_iterator it = __conversions().begin(); it != __conversions().end(); ++it)
		d_data->types.addItem(it.value().second);

	d_data->types.QComboBox::setCurrentIndex(0);
	d_data->types.setSizeAdjustPolicy(QComboBox::AdjustToContents);
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&d_data->types);
	hlay->setContentsMargins(0, 0, 0, 0);
	setLayout(hlay);

	connect(&d_data->types, SIGNAL(currentTextChanged(const QString&)), this, SLOT(updateConversion()));
}

VipConvertEditor::~VipConvertEditor()
{
}

VipConvert* VipConvertEditor::convert() const
{
	return d_data->convert;
}

void VipConvertEditor::setConvert(VipConvert* tr)
{
	if (d_data->convert) {
		disconnect(d_data->convert, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(convertChanged()));
	}

	d_data->convert = tr;
	if (tr) {
		int type = d_data->convert->propertyAt(0)->data().value<int>();
		d_data->types.blockSignals(true);
		d_data->types.setCurrentIndex(__indexForType(type));
		d_data->types.blockSignals(false);

		connect(tr, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(conversionChanged()));
	}
}

QComboBox* VipConvertEditor::types() const
{
	return &d_data->types;
}

void VipConvertEditor::conversionChanged()
{
	if (d_data->convert) {
		int type = d_data->convert->propertyAt(0)->data().value<int>();
		d_data->types.blockSignals(true);
		d_data->types.setCurrentIndex(__indexForType(type));
		d_data->types.blockSignals(false);
	}
}

void VipConvertEditor::updateConversion()
{
	if (d_data->convert) {
		int index = d_data->types.currentIndex();
		int type = __conversions()[index].first;

		// set the new type
		d_data->convert->propertyAt(0)->setData(type);
		d_data->convert->reload();
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&d_data->editor);

	QHBoxLayout* th_lay = new QHBoxLayout();
	th_lay->setContentsMargins(0, 0, 0, 0);

	lay->addLayout(th_lay);
	setLayout(lay);

	d_data->editor.setToolTip("Select a component to display");
	d_data->editor.hide();
	this->setToolTip("Select a component to display");
	connect(&d_data->editor, SIGNAL(componentChanged(const QString&)), this, SLOT(updateDisplayImage()));
	connect(&d_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));

	d_data->updateTimer.setSingleShot(false);
	d_data->updateTimer.setInterval(500);
	d_data->updateTimer.start();
}

VipExtractComponentEditor* VipDisplayImageEditor::editor() const
{
	return &d_data->editor;
}

VipDisplayImageEditor::~VipDisplayImageEditor()
{
	d_data->updateTimer.stop();
	disconnect(&d_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));
}

void VipDisplayImageEditor::setDisplayImage(VipDisplayImage* d)
{
	d_data->display = d;
	if (d) {
		d_data->editor.setExtractComponent(d->extractComponent());
	}
}

VipDisplayImage* VipDisplayImageEditor::displayImage() const
{
	return d_data->display;
}

void VipDisplayImageEditor::updateDisplayImage()
{
	if (d_data->display) {
		VipAnyData any = d_data->display->inputAt(0)->data();
		d_data->display->inputAt(0)->setData(any);
	}
}

void VipDisplayImageEditor::updateExtractorVisibility()
{
	if (d_data->display)
		d_data->editor.setVisible(VipGenericExtractComponent::HasComponents(d_data->display->inputAt(0)->probe().value<VipNDArray>()));
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&d_data->editor);
	setLayout(lay);

	d_data->editor.setToolTip("Select a component to display");
	d_data->editor.hide();
	this->setToolTip("Select a component to display");
	connect(&d_data->editor, SIGNAL(componentChanged(const QString&)), this, SLOT(updateDisplayCurve()));
	connect(&d_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));

	d_data->updateTimer.setSingleShot(false);
	d_data->updateTimer.setInterval(500);
	d_data->updateTimer.start();
}

VipDisplayCurveEditor::~VipDisplayCurveEditor()
{
	d_data->updateTimer.stop();
	disconnect(&d_data->updateTimer, SIGNAL(timeout()), this, SLOT(updateExtractorVisibility()));
}

void VipDisplayCurveEditor::setDisplay(VipDisplayCurve* d)
{
	d_data->display = d;
	if (d) {
		d_data->editor.setExtractComponent(d->extractComponent());
	}
}

VipDisplayCurve* VipDisplayCurveEditor::display() const
{
	return d_data->display;
}

void VipDisplayCurveEditor::updateDisplayCurve()
{
	if (d_data->display) {
		VipAnyData any = d_data->display->inputAt(0)->data();
		d_data->display->inputAt(0)->setData(any);
	}
}

void VipDisplayCurveEditor::updateExtractorVisibility()
{
	if (d_data->display) {
		VipAnyData any = d_data->display->inputAt(0)->probe();
		if (any.data().userType() == qMetaTypeId<VipComplexPoint>() || any.data().userType() == qMetaTypeId<VipComplexPointVector>())
			d_data->editor.setVisible(true);
		else
			d_data->editor.setVisible(false);
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addWidget(&d_data->box);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	d_data->box.setToolTip("Select the input number");

	connect(&d_data->box, SIGNAL(currentIndexChanged(int)), this, SLOT(updateSwitch()));
	connect(&d_data->box, SIGNAL(openPopup()), this, SLOT(resetSwitch()));
}

VipSwitchEditor::~VipSwitchEditor()
{
}

void VipSwitchEditor::resetSwitch()
{
	setSwitch(d_data->sw);
}

void VipSwitchEditor::setSwitch(VipSwitch* sw)
{
	d_data->sw = sw;

	if (sw) {
		d_data->box.blockSignals(true);

		d_data->box.clear();
		for (int i = 0; i < sw->inputCount(); ++i) {
			VipAnyData any = sw->inputAt(i)->probe();
			if (any.name().isEmpty())
				d_data->box.addItem(QString::number(i));
			else
				d_data->box.addItem(any.name());
		}

		d_data->box.setCurrentIndex(sw->propertyAt(0)->data().value<int>());
		d_data->box.blockSignals(false);
	}
}

void VipSwitchEditor::updateSwitch()
{
	if (d_data->sw) {
		d_data->sw->propertyAt(0)->setData(d_data->box.currentIndex());
		d_data->sw->update(true);
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QGridLayout* lay = new QGridLayout();
	lay->addWidget(&d_data->clampMax, 0, 0);
	lay->addWidget(&d_data->max, 0, 1);
	lay->addWidget(&d_data->clampMin, 1, 0);
	lay->addWidget(&d_data->min, 1, 1);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	d_data->clampMax.setText("Clamp max value");
	d_data->max.setToolTip("Max value");
	d_data->clampMin.setText("Clamp min value");
	d_data->max.setToolTip("Min value");

	connect(&d_data->clampMax, SIGNAL(clicked(bool)), this, SLOT(updateClamp()));
	connect(&d_data->clampMin, SIGNAL(clicked(bool)), this, SLOT(updateClamp()));
	connect(&d_data->max, SIGNAL(valueChanged(double)), this, SLOT(updateClamp()));
	connect(&d_data->min, SIGNAL(valueChanged(double)), this, SLOT(updateClamp()));
}

VipClampEditor::~VipClampEditor()
{
}

void VipClampEditor::setClamp(VipClamp* c)
{
	d_data->clamp = c;
	if (c) {
		double min = c->propertyAt(0)->data().value<double>();
		double max = c->propertyAt(1)->data().value<double>();
		d_data->clampMax.blockSignals(true);
		d_data->clampMax.setChecked(max == max);
		d_data->clampMax.blockSignals(false);
		d_data->clampMin.blockSignals(true);
		d_data->clampMin.setChecked(min == min);
		d_data->clampMin.blockSignals(false);

		d_data->max.blockSignals(true);
		d_data->max.setValue(max);
		d_data->max.blockSignals(false);
		d_data->min.blockSignals(true);
		d_data->min.setValue(min);
		d_data->min.blockSignals(false);
	}
}

VipClamp* VipClampEditor::clamp() const
{
	return d_data->clamp;
}

void VipClampEditor::updateClamp()
{
	if (d_data->clamp) {
		if (d_data->clampMax.isChecked())
			d_data->clamp->propertyAt(1)->setData(d_data->max.value());
		else
			d_data->clamp->propertyAt(1)->setData(vipNan());

		if (d_data->clampMin.isChecked())
			d_data->clamp->propertyAt(0)->setData(d_data->min.value());
		else
			d_data->clamp->propertyAt(0)->setData(vipNan());

		d_data->clamp->reload();
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->label);
	lay->addWidget(&d_data->image);
	lay->addWidget(&d_data->xyxy_column);
	lay->addWidget(&d_data->xyyy_column);
	lay->addWidget(&d_data->xyxy_row);
	lay->addWidget(&d_data->xyyy_row);
	setLayout(lay);

	d_data->label.setText("<b>Interpret text file as:</b>");
	d_data->image.setText("An image sequence");
	d_data->xyxy_column.setText("Columns of X1 Y1...Xn Yn");
	d_data->xyyy_column.setText("Columns of X Y1...Yn");
	d_data->xyxy_row.setText("Rows of X1 Y1...Xn Yn");
	d_data->xyyy_row.setText("Rows of X Y1...Yn");
	d_data->image.setChecked(true);

	connect(&d_data->image, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&d_data->xyxy_column, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&d_data->xyyy_column, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&d_data->xyxy_row, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
	connect(&d_data->xyyy_row, SIGNAL(clicked(bool)), this, SLOT(updateTextFileReader()));
}

VipTextFileReaderEditor::~VipTextFileReaderEditor()
{
}

void VipTextFileReaderEditor::setTextFileReader(VipTextFileReader* reader)
{
	d_data->reader = reader;
	if (reader) {
		if (reader->type() == VipTextFileReader::Image)
			d_data->image.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYXYColumn)
			d_data->xyxy_column.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYYYColumn)
			d_data->xyyy_column.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYXYRow)
			d_data->xyxy_row.setChecked(true);
		else if (reader->type() == VipTextFileReader::XYYYRow)
			d_data->xyyy_row.setChecked(true);
		else
			d_data->image.setChecked(true);
	}
}

void VipTextFileReaderEditor::updateTextFileReader()
{
	if (d_data->reader) {
		if (d_data->image.isChecked())
			d_data->reader->setType(VipTextFileReader::Image);
		if (d_data->xyxy_column.isChecked())
			d_data->reader->setType(VipTextFileReader::XYXYColumn);
		if (d_data->xyyy_column.isChecked())
			d_data->reader->setType(VipTextFileReader::XYYYColumn);
		if (d_data->xyxy_row.isChecked())
			d_data->reader->setType(VipTextFileReader::XYXYRow);
		if (d_data->xyyy_row.isChecked())
			d_data->reader->setType(VipTextFileReader::XYYYRow);
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&d_data->digits_label);
	hlay->addWidget(&d_data->digits);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->label);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&d_data->stack);
	lay->addWidget(&d_data->multi_file);
	lay->addLayout(hlay);
	setLayout(lay);

	d_data->label.setText("<b>File saving options</b><br><b>Warning:</b>These options are only useful for temporal sequences.");
	d_data->stack.setText("Stack the data in the same file");
	d_data->stack.setToolTip("For temporal sequences, all data (images, curves,...) will be saved in the same file with a blank line separator.");
	d_data->multi_file.setText("Create one file per data");
	d_data->multi_file.setToolTip("For temporal sequences, all data (images, curves,...) will be saved in separate files. All files will end with a unique number starting to 0.");
	d_data->digits.setValue(4);
	d_data->digits.setToolTip("Each file name will end by a number\nincremented for each new data.\nSet the number digits.");
	d_data->digits.hide();
	d_data->digits_label.hide();
	d_data->digits.setRange(1, 8);
	d_data->stack.setChecked(true);
	d_data->digits_label.setText("Digit number");

	connect(&d_data->stack, SIGNAL(clicked(bool)), this, SLOT(updateTextFileWriter()));
	connect(&d_data->multi_file, SIGNAL(clicked(bool)), this, SLOT(updateTextFileWriter()));
	connect(&d_data->digits, SIGNAL(valueChanged(int)), this, SLOT(updateTextFileWriter()));
}

VipTextFileWriterEditor::~VipTextFileWriterEditor()
{
}

void VipTextFileWriterEditor::setTextFileWriter(VipTextFileWriter* writer)
{
	d_data->writer = writer;
	if (writer) {
		if (writer->type() == VipTextFileWriter::MultipleFiles)
			d_data->multi_file.setChecked(true);
		else
			d_data->stack.setChecked(true);
		d_data->digits.setVisible(d_data->multi_file.isChecked());
		d_data->digits_label.setVisible(d_data->multi_file.isChecked());
	}
}

void VipTextFileWriterEditor::updateTextFileWriter()
{
	if (d_data->writer) {
		if (d_data->multi_file.isChecked())
			d_data->writer->setType(VipTextFileWriter::MultipleFiles);
		else
			d_data->writer->setType(VipTextFileWriter::StackData);
		d_data->digits.setVisible(d_data->multi_file.isChecked());
		d_data->digits_label.setVisible(d_data->multi_file.isChecked());
		d_data->writer->setDigitsNumber(d_data->digits.value());
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->label);
	lay->addWidget(&d_data->stack);
	lay->addWidget(&d_data->multi_file);
	lay->addWidget(&d_data->digits);
	setLayout(lay);

	d_data->label.setText("<b>File saving options</b>");
	d_data->stack.setText("Stack the images in the same file");
	d_data->multi_file.setText("Create one file per image");
	d_data->digits.setValue(4);
	d_data->digits.setToolTip("Each file name will end by a number\nincremented for each new image.\nSet the number digits.");
	d_data->digits.hide();
	d_data->digits.setRange(1, 8);
	d_data->stack.setChecked(true);

	connect(&d_data->stack, SIGNAL(clicked(bool)), this, SLOT(updateImageWriter()));
	connect(&d_data->multi_file, SIGNAL(clicked(bool)), this, SLOT(updateImageWriter()));
	connect(&d_data->digits, SIGNAL(valueChanged(int)), this, SLOT(updateImageWriter()));
}

VipImageWriterEditor::~VipImageWriterEditor()
{
}

void VipImageWriterEditor::setImageWriter(VipImageWriter* writer)
{
	d_data->writer = writer;
	if (writer) {
		if (writer->type() == VipImageWriter::MultipleImages)
			d_data->multi_file.setChecked(true);
		else
			d_data->stack.setChecked(true);
		d_data->digits.setVisible(d_data->multi_file.isChecked());
	}
}

void VipImageWriterEditor::updateImageWriter()
{
	if (d_data->writer) {
		if (d_data->multi_file.isChecked())
			d_data->writer->setType(VipImageWriter::MultipleImages);
		else
			d_data->writer->setType(VipImageWriter::StackImages);
		d_data->digits.setVisible(d_data->multi_file.isChecked());
		d_data->writer->setDigitsNumber(d_data->digits.value());
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&d_data->resampleText);
	hlay->addWidget(&d_data->resample);
	hlay->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* hlay2 = new QHBoxLayout();
	hlay2->addWidget(&d_data->useFixValue);
	hlay2->addWidget(&d_data->fixValue);
	hlay2->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(10, 0, 0, 0);
	vlay->addWidget(&d_data->useBounds);
	vlay->addLayout(hlay2);

	QHBoxLayout* hlay3 = new QHBoxLayout();
	hlay3->addWidget(&d_data->saveAsCSVText);
	hlay3->addWidget(&d_data->saveAsCSV);
	hlay3->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->addLayout(hlay);
	layout->addLayout(vlay);
	layout->addWidget(VipLineWidget::createHLine());
	layout->addLayout(hlay3);
	setLayout(layout);

	d_data->resampleText.setText("Resample using input signals");
	d_data->resampleText.setToolTip("When saving multiple signals, they will be resampled to contain the same number of points with the same X values.\n"
					"The time interval used for resampling can be computed either with the union or intersection of input signals.");
	d_data->resample.addItem("union");
	d_data->resample.addItem("intersection");
	d_data->resample.setCurrentText("intersection");
	d_data->resample.setToolTip(d_data->resampleText.toolTip());

	d_data->useBounds.setChecked(true);
	d_data->useBounds.setText("Use closest value");
	d_data->useBounds.setToolTip("For 'union' only, the resampling algorithm might need to create new values outside the signal bounds.\n"
				     "Select this option to always pick the closest  available value.");
	d_data->useFixValue.setText("Use fixed value");
	d_data->useFixValue.setToolTip("For 'union' only, the resampling algorithm might need to create new values outside the signal bounds.\n"
				       "Select this option to set the new points to a fixed value.");
	d_data->fixValue.setValue(0);

	d_data->saveAsCSVText.setText("Select file format");
	d_data->saveAsCSVText.setToolTip("Select 'CSV' to create a real CSV file with the signals units.\n"
					 "Select 'TEXT' to save the raw signals in ascii format, without additional metadata.");
	d_data->saveAsCSV.addItem("CSV");
	d_data->saveAsCSV.addItem("TEXT");
	d_data->saveAsCSV.setToolTip(d_data->saveAsCSVText.toolTip());

	connect(&d_data->resample, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
	connect(&d_data->resample, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCSVWriter()));
	connect(&d_data->useBounds, SIGNAL(clicked(bool)), this, SLOT(updateCSVWriter()));
	connect(&d_data->useFixValue, SIGNAL(clicked(bool)), this, SLOT(updateCSVWriter()));
	connect(&d_data->fixValue, SIGNAL(valueChanged(double)), this, SLOT(updateCSVWriter()));
	connect(&d_data->saveAsCSV, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCSVWriter()));

	updateWidgets();
}

VipCSVWriterEditor::~VipCSVWriterEditor()
{
}

void VipCSVWriterEditor::updateWidgets()
{
	if (d_data->resample.currentText() == "union") {
		d_data->useBounds.setEnabled(true);
		d_data->useFixValue.setEnabled(true);
		d_data->fixValue.setEnabled(true);
	}
	else {
		d_data->useBounds.setEnabled(false);
		d_data->useFixValue.setEnabled(false);
		d_data->fixValue.setEnabled(false);
	}
}

void VipCSVWriterEditor::setCSVWriter(VipCSVWriter* w)
{
	d_data->processing = w;
	if (w) {
		if (w->resampleMode() & ResampleIntersection)
			d_data->resample.setCurrentText("intersection");
		else
			d_data->resample.setCurrentText("union");

		d_data->fixValue.setValue(w->paddValue());
		d_data->useFixValue.setChecked(w->resampleMode() & ResamplePadd0);
		if (w->writeTextFile())
			d_data->saveAsCSV.setCurrentText("TEXT");
		else
			d_data->saveAsCSV.setCurrentText("CSV");
	}
}

void VipCSVWriterEditor::updateCSVWriter()
{
	if (d_data->processing) {
		int r = ResampleInterpolation;
		if (d_data->resample.currentText() == "intersection")
			r |= ResampleIntersection;
		else
			r |= ResampleUnion;
		if (d_data->useFixValue.isChecked()) {
			r |= ResamplePadd0;
			d_data->processing->setPaddValue(d_data->fixValue.value());
		}
		d_data->processing->setResampleMode(r);
		d_data->processing->setWriteTextFile(d_data->saveAsCSV.currentText() == "TEXT");
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	// create the options for when the device is closed

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->file_suffixes);
	lay->addWidget(&d_data->recursive);

	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("File count"));
		hlay->addWidget(&d_data->file_count);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}

	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("File start"));
		hlay->addWidget(&d_data->file_start);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}

	lay->addWidget(&d_data->alphabetical_order);
	lay->addWidget(&d_data->independent_data);
	lay->addWidget(&d_data->sequence_of_data);

	QGroupBox* images = new QGroupBox("Video file options");
	images->setFlat(true);
	lay->addWidget(images);

	lay->addWidget(&d_data->fixed_size);
	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("Width"));
		hlay->addWidget(&d_data->width);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}
	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->addWidget(new QLabel("Height"));
		hlay->addWidget(&d_data->height);
		hlay->setContentsMargins(0, 0, 0, 0);
		lay->addLayout(hlay);
	}
	lay->addWidget(&d_data->smooth);
	d_data->closedOptions = new QWidget();
	d_data->closedOptions->setLayout(lay);

	d_data->file_suffixes.setToolTip("Supported extensions with comma separators");
	d_data->file_suffixes.setPlaceholderText("Supported extensions");

	d_data->recursive.setText("Read subdirectories");

	d_data->file_count.setRange(-1, 1000000);
	d_data->file_count.setValue(-1);
	d_data->file_count.setToolTip("Set the maximum file number\n(-1 means all files in the directory)");

	d_data->file_start.setRange(0, 1000000);
	d_data->file_start.setValue(0);
	d_data->file_start.setToolTip("Set start file index\n(all files before the index are skipped)");

	d_data->alphabetical_order.setText("Sort files alphabetically");
	d_data->alphabetical_order.setChecked(true);

	d_data->independent_data.setText("Read as independent data files");
	d_data->sequence_of_data.setText("Read as a sequence of data files");
	d_data->independent_data.setChecked(true);

	d_data->fixed_size.setText("Use a fixed size");
	d_data->fixed_size.setChecked(false);
	d_data->fixed_size.setToolTip("All images are resized with given size");

	d_data->width.setRange(2, 5000);
	d_data->width.setValue(320);
	d_data->width.setToolTip("Image width");
	d_data->width.setEnabled(false);

	d_data->height.setRange(2, 5000);
	d_data->height.setValue(320);
	d_data->height.setToolTip("Image width");
	d_data->height.setEnabled(false);

	d_data->smooth.setText("Smooth resize");
	d_data->smooth.setChecked(false);
	d_data->smooth.setEnabled(false);

	connect(&d_data->fixed_size, SIGNAL(clicked(bool)), &d_data->width, SLOT(setEnabled(bool)));
	connect(&d_data->fixed_size, SIGNAL(clicked(bool)), &d_data->height, SLOT(setEnabled(bool)));
	connect(&d_data->fixed_size, SIGNAL(clicked(bool)), &d_data->smooth, SLOT(setEnabled(bool)));

	d_data->file_suffixes.setFocus();

	// create the options for when the device is opened
	d_data->openedOptions = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(&d_data->applyToAllDevices);
	d_data->openedOptions->setLayout(vlay);

	d_data->applyToAllDevices.setText("Apply to all devices");
	connect(&d_data->applyToAllDevices, SIGNAL(clicked(bool)), this, SLOT(apply()));

	// create the final layout
	QVBoxLayout* final_lay = new QVBoxLayout();
	final_lay->addWidget(d_data->closedOptions);
	final_lay->addWidget(d_data->openedOptions);
	setLayout(final_lay);
}

VipDirectoryReaderEditor::~VipDirectoryReaderEditor()
{
}

void VipDirectoryReaderEditor::setDirectoryReader(VipDirectoryReader* reader)
{
	d_data->reader = reader;
	if (reader) {
		d_data->closedOptions->setVisible(!reader->isOpen());
		d_data->openedOptions->setVisible(reader->isOpen());

		// options when the device is closed
		if (!reader->isOpen()) {
			d_data->file_suffixes.setText(reader->supportedSuffixes().join(","));
			d_data->recursive.setChecked(reader->recursive());
			d_data->file_count.setValue(reader->fileCount());
			d_data->file_start.setValue(reader->fileStart());
			d_data->alphabetical_order.setChecked(reader->alphabeticalOrder());
			d_data->sequence_of_data.setChecked(reader->type() == VipDirectoryReader::SequenceOfData);
			d_data->fixed_size.setChecked(reader->fixedSize() != QSize());
			if (d_data->fixed_size.isChecked()) {
				d_data->width.setValue(reader->fixedSize().width());
				d_data->height.setValue(reader->fixedSize().height());
			}
			d_data->smooth.setChecked(reader->smoothResize());
		}
		// options when the device is already opened
		else {
			// remove the previous editors
			while (d_data->editors.size())
				if (d_data->editors.begin().value())
					delete d_data->editors.begin().value();
			d_data->editors.clear();

			// compute all device types and add the editors
			for (int i = 0; i < reader->deviceCount(); ++i) {
				VipIODevice* dev = reader->deviceAt(i);
				const QMetaObject* meta = dev->metaObject();
				if (d_data->editors.find(meta) == d_data->editors.end()) {
					// create the editor
					VipUniqueProcessingObjectEditor* editor = new VipUniqueProcessingObjectEditor();
					if (editor->setProcessingObject(dev)) {
						// add the editor to the layout if setProcessingObject is successfull
						d_data->editors[meta] = editor;
						static_cast<QVBoxLayout*>(d_data->openedOptions->layout())->insertWidget(0, editor);
					}
					else {
						// do not add the editor to the layout, but add the meta object to editors to avoid this meta object for next devices
						delete editor;
						d_data->editors[meta] = nullptr;
					}
				}
			}
		}
	}
}

#include <qmessagebox.h>
void VipDirectoryReaderEditor::apply()
{
	if (!d_data->reader)
		return;

	VipDirectoryReader* r = d_data->reader;

	if (!r->isOpen()) {
		// set the options for closed VipDirectoryReader
		r->setSupportedSuffixes(d_data->file_suffixes.text());
		r->setRecursive(d_data->recursive.isChecked());
		r->setFileCount(d_data->file_count.value());
		r->setFileStart(d_data->file_start.value());
		r->setAlphabeticalOrder(d_data->alphabetical_order.isChecked());
		r->setType(d_data->sequence_of_data.isChecked() ? VipDirectoryReader::SequenceOfData : VipDirectoryReader::IndependentData);
		if (d_data->fixed_size.isChecked())
			r->setFixedSize(QSize(d_data->width.value(), d_data->height.value()));
		r->setSmoothResize(d_data->smooth.isChecked());

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
			if (d_data->editors.find(meta) != d_data->editors.end()) {
				if (VipUniqueProcessingObjectEditor* editor = d_data->editors[meta]) {
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QGridLayout* lay = new QGridLayout();

	lay->addWidget(new QLabel("Operator:"), 0, 0);
	lay->addWidget(&d_data->operation, 0, 1);
	lay->addWidget(&d_data->editor, 1, 0, 1, 2);

	d_data->operation.setToolTip("Select the operation to perform (addition, subtraction, multiplication, division, or binary operation)");
	d_data->operation.addItems(QStringList() << "+"
						 << "-"
						 << "*"
						 << "/"
						 << "&"
						 << "|"
						 << "^");
	d_data->operation.setCurrentIndex(1);

	setLayout(lay);

	connect(&d_data->editor, SIGNAL(valueChanged(const QVariant&)), this, SLOT(apply()));
	connect(&d_data->operation, SIGNAL(currentIndexChanged(int)), this, SLOT(apply()));
}

VipOperationBetweenPlayersEditor::~VipOperationBetweenPlayersEditor()
{
}

void VipOperationBetweenPlayersEditor::setProcessing(VipOperationBetweenPlayers* proc)
{
	if (proc != d_data->processing) {
		d_data->processing = proc;

		if (proc) {
			d_data->editor.blockSignals(true);
			d_data->editor.setValue(proc->propertyAt(1)->value<VipOtherPlayerData>());
			d_data->editor.blockSignals(false);

			d_data->operation.blockSignals(true);
			d_data->operation.setCurrentText(proc->propertyName("Operator")->value<QString>());
			d_data->operation.blockSignals(false);
		}
	}
}

void VipOperationBetweenPlayersEditor::apply()
{
	if (VipOperationBetweenPlayers* proc = d_data->processing) {
		proc->propertyAt(0)->setData(d_data->operation.currentText());
		proc->propertyAt(1)->setData(d_data->editor.value());
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			menu.exec(evt->globalPos());
#else
			menu.exec(evt->globalPosition().toPoint());
#endif
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
			QListWidgetItem * item = itemAt(evt->VIP_EVT_POSITION());
			if (item)
			{
				QRect rect = this->visualItemRect(item);
				insertPos = this->indexFromItem(item).row();
				if (evt->VIP_EVT_POSITION().y() > rect.center().y())
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->trs = new TrListWidget(this);
	QVBoxLayout* l = new QVBoxLayout();
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(&d_data->interp);
	l->addWidget(&d_data->size);
	l->addWidget(&d_data->back);
	l->addWidget(d_data->trs);
	setLayout(l);

	d_data->interp.setText("Linear interpolation");
	d_data->interp.setToolTip("Apply a linear interpolation to the output image");
	d_data->size.setText("Output size fit the transform size");
	d_data->size.setToolTip("If checked, the output image size will be computed based on the transformation in order to contain the whole image.\n"
				"Otherwise, the output image size is the same as the input one.");
	d_data->back.setToolTip("Background value.\nFor numerical image, just enter an integer or floating point value.\n"
				"For complex image, enter a complex value on the form '(x+yj)'.\n"
				"For color image, enter a ARGB value on the form '[A,R,G,B]'.\n"
				"Press ENTER to validate.");
	d_data->back.setText("0");
	d_data->trs->setToolTip("Consecutive image transforms.\n"
				"Right click to add or remove a tranform.");
	// d_data->trs.installEventFilter(this);
	d_data->trs->setDragDropMode(QAbstractItemView::InternalMove);
	d_data->trs->setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->trs->setDragDropOverwriteMode(false);
	d_data->trs->setDefaultDropAction(Qt::TargetMoveAction);
	d_data->trs->setViewMode(QListView::ListMode);

	connect(&d_data->interp, SIGNAL(clicked(bool)), this, SLOT(updateProcessing()));
	connect(&d_data->size, SIGNAL(clicked(bool)), this, SLOT(updateProcessing()));
	connect(&d_data->back, SIGNAL(returnPressed()), this, SLOT(updateProcessing()));
}
VipGenericImageTransformEditor::~VipGenericImageTransformEditor()
{
}

void VipGenericImageTransformEditor::setProcessing(VipGenericImageTransform* p)
{
	if (p != d_data->proc) {
		d_data->proc = p;
		updateWidget();
	}
}
VipGenericImageTransform* VipGenericImageTransformEditor::processing() const
{
	return d_data->proc;
}

void VipGenericImageTransformEditor::updateProcessing()
{
	if (!d_data->proc)
		return;

	// build transform
	TransformList trs;
	for (int i = 0; i < d_data->trs->count(); ++i) {
		TrListWidgetItem* it = static_cast<TrListWidgetItem*>(d_data->trs->item(i));
		trs << Transform(it->type, it->x->value(), it->y ? it->y->value() : 0);
	}

	d_data->proc->propertyAt(0)->setData(QVariant::fromValue(trs));
	d_data->proc->propertyAt(1)->setData(d_data->interp.isChecked() ? (int)Vip::LinearInterpolation : (int)Vip::NoInterpolation);
	d_data->proc->propertyAt(2)->setData(d_data->size.isChecked() ? (int)Vip::TransformBoundingRect : (int)Vip::SrcSize);

	QString back = d_data->back.text();
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
		d_data->back.setStyleSheet("QLineEdit {border:1px solid red;}");
	else {
		d_data->back.setStyleSheet("");
		d_data->proc->propertyAt(3)->setData(value);
	}

	d_data->proc->reload();
}

void VipGenericImageTransformEditor::updateWidget()
{
	const TransformList trs = d_data->proc->propertyAt(0)->value<TransformList>();
	int interp = d_data->proc->propertyAt(1)->value<int>();
	int size = d_data->proc->propertyAt(2)->value<int>();
	QVariant back = d_data->proc->propertyAt(3)->value<QVariant>();

	d_data->interp.setChecked(interp != Vip::NoInterpolation);
	d_data->size.setChecked(size == Vip::TransformBoundingRect);
	d_data->back.setText(back.toString());
	d_data->trs->clear();

	for (int i = 0; i < trs.size(); ++i) {
		TrListWidgetItem* it = new TrListWidgetItem(d_data->trs, trs[i].type);
		it->x->setValue(trs[i].x);
		if (it->y)
			it->y->setValue(trs[i].y);
	}

	recomputeSize();
}

void VipGenericImageTransformEditor::recomputeSize()
{
	d_data->trs->setMaximumHeight(d_data->trs->count() * 30 + 30);
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
	/*TrListWidgetItem * it =*/new TrListWidgetItem(d_data->trs, type);
	recomputeSize();
}
void VipGenericImageTransformEditor::removeSelectedTransform()
{
	QList<QListWidgetItem*> items = d_data->trs->selectedItems();
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
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	this->setItemAttribute(VipPlotItem::HasLegendIcon, false);
	this->setItemAttribute(VipPlotItem::AutoScale, false);
	this->setItemAttribute(VipPlotItem::IsSuppressable, false);
	this->setRenderHints(QPainter::Antialiasing);

	d_data->quiver.setPen(QColor(Qt::red));
	d_data->quiver.setStyle(VipQuiverPath::EndArrow);
	d_data->quiver.setLength(VipQuiverPath::End, 8);
	d_data->quiver.setExtremityBrush(VipQuiverPath::End, QBrush(Qt::red));
	d_data->quiver.setExtremityPen(VipQuiverPath::End, QColor(Qt::red));
	d_data->symbol.setStyle(VipSymbol::Cross);
	d_data->symbol.setPen(QColor(Qt::black));
	d_data->symbol.setSize(QSizeF(7, 7));
}

PlotWarpingPoints::~PlotWarpingPoints()
{
}

void PlotWarpingPoints::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	DeformationField field = rawData();
	for (int i = 0; i < field.size(); ++i) {
		if (field[i].first == field[i].second) {
			painter->setRenderHint(QPainter::Antialiasing, false);
			d_data->symbol.drawSymbol(painter, m->transform(field[i].first));
		}
		else {
			painter->setRenderHint(QPainter::Antialiasing, true);
			d_data->quiver.draw(painter, QLineF(m->transform(field[i].first), m->transform(field[i].second)));
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
	return const_cast<VipQuiverPath&>(d_data->quiver);
}

VipSymbol& PlotWarpingPoints::symbol() const
{
	return const_cast<VipSymbol&>(d_data->symbol);
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
	VIP_CREATE_PRIVATE_DATA(d_data);

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
				const auto warp = vipToPointF( d_data->warping->warping());
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
			QVector<QPointF> warp(size);
			in.read((char*)warp.data(), in.size());
			d_data->warping->setWarping(vipToPointVector( warp));
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

	VipShapeList to = player->plotSceneModel()->sceneModel().shapes("Points");
	VipShapeList from = d_data->warping->sceneModel().shapes("Points");

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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setSpacing(1);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);
}

VipUniqueProcessingObjectEditor::~VipUniqueProcessingObjectEditor()
{
}

void VipUniqueProcessingObjectEditor::emitEditorVisibilityChanged()
{
	Q_EMIT editorVisibilityChanged();
}

VipProcessingObject* VipUniqueProcessingObjectEditor::processingObject() const
{
	return const_cast<VipProcessingObject*>(d_data->processingObject);
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
	d_data->isShowExactProcessingOnly = exact_proc;
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
	return d_data->isShowExactProcessingOnly;
}

void VipUniqueProcessingObjectEditor::tryUpdateProcessing()
{
	if (!d_data->processingObject)
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
	if (obj == d_data->processingObject)
		return false;

	d_data->processingObject = obj;

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

	setShowExactProcessingOnly(d_data->isShowExactProcessingOnly);

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
	VIP_CREATE_PRIVATE_DATA(d_data);

	this->setText("Select a leaf processing");
	this->setToolTip("<p><b>Select an item (video, image, curve...) in the current workspace.</b></p>"
			 "\nThis will display the processings related to this item.");

	d_data->menu = new QMenu(this);
	this->setMenu(d_data->menu);
	this->setPopupMode(QToolButton::InstantPopup);
	this->setMaximumWidth(300);

	connect(d_data->menu, SIGNAL(triggered(QAction*)), this, SLOT(processingSelected(QAction*)));
	connect(d_data->menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
}

VipProcessingLeafSelector::~VipProcessingLeafSelector()
{
}

void VipProcessingLeafSelector::setProcessingPool(VipProcessingPool* pool)
{
	d_data->pool = pool;
	d_data->processing = nullptr;
}

VipProcessingPool* VipProcessingLeafSelector::processingPool() const
{
	return d_data->pool;
}

QList<VipProcessingObject*> VipProcessingLeafSelector::leafs() const
{
	if (VipProcessingPool* pool = d_data->pool) {
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
	return d_data->processing;
}

void VipProcessingLeafSelector::setProcessing(VipProcessingObject* proc)
{
	d_data->processing = proc;
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
	d_data->menu->blockSignals(true);

	d_data->menu->clear();
	d_data->menu->setToolTipsVisible(true);
	QList<VipProcessingObject*> _leafs = this->leafs();
	for (int i = 0; i < _leafs.size(); ++i) {
		QString tool_tip;
		QAction* act = d_data->menu->addAction(title(_leafs[i], tool_tip));
		act->setCheckable(true);
		act->setToolTip(tool_tip);
		if (_leafs[i] == d_data->processing)
			act->setChecked(true);
		act->setProperty("processing", QVariant::fromValue(_leafs[i]));
	}

	d_data->menu->blockSignals(false);
}

void VipProcessingLeafSelector::processingSelected(QAction* act)
{
	if (VipProcessingObject* proc = act->property("processing").value<VipProcessingObject*>()) {
		if (proc != d_data->processing) {
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
		if (VipAbstractPlayer* pl = vipFindParent<VipAbstractPlayer>(disp->widget())) {
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->processing = object;
	d_data->lastErrorDate = 0;

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(&d_data->reset);
	hlay->addWidget(&d_data->text);
	hlay->addWidget(&d_data->showError);
	hlay->addStretch(1);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(&d_data->errors);
	vlay->setContentsMargins(0, 0, 0, 0);

	setLayout(vlay);

	d_data->reset.setIcon(vipIcon("reset.png"));
	d_data->reset.setToolTip("Reset the processing");

	d_data->showError.setIcon(vipIcon("error.png"));
	d_data->showError.setToolTip("Show the last processing errors");
	d_data->showError.setCheckable(true);

	setMaximumHeight(30);
	if (!object->objectName().isEmpty())
		d_data->text.setText(vipSplitClassname(object->objectName()));
	else
		d_data->text.setText(vipSplitClassname(object->className()));
	// d_data->text.setStyleSheet("font-weight: bold; text-align: left;");
	d_data->text.setStyleSheet("text-align: left;");
	QFont font = d_data->text.font();
	font.setBold(true);
	d_data->text.setFont(font);

	d_data->text.setAutoRaise(true);
	d_data->text.setCheckable(true);
	d_data->text.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d_data->text.setIcon(vipIcon("hidden.png"));

	QString tooltip = d_data->text.text() + " properties";
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

	d_data->text.setToolTip(tooltip);

	d_data->errors.setStyleSheet("QPlainTextEdit { color: red; font:  14px; background-color:transparent;}");
	d_data->errors.hide();
	d_data->errors.setMinimumHeight(60);
	d_data->errors.setReadOnly(true);
	d_data->errors.setLineWrapMode(QPlainTextEdit::NoWrap);

	d_data->timer.setSingleShot(false);
	d_data->timer.setInterval(100);
	connect(&d_data->timer, &QTimer::timeout, this, &VipProcessingTooButton::updateText);
	d_data->timer.start();

	connect(&d_data->text, SIGNAL(clicked(bool)), this, SLOT(emitClicked(bool)), Qt::DirectConnection);
	connect(&d_data->reset, SIGNAL(clicked(bool)), this, SLOT(resetProcessing()));
	connect(&d_data->showError, SIGNAL(clicked(bool)), this, SLOT(showError(bool)));
}

VipProcessingTooButton::~VipProcessingTooButton()
{
	d_data->timer.stop();
	d_data->timer.disconnect();
}
void VipProcessingTooButton::updateText()
{
	if (VipProcessingObject* obj = d_data->processing) {
		qint64 time = obj->processingTime() / 1000000;
		QString text = vipSplitClassname(obj->objectName().isEmpty() ? obj->className() : obj->objectName());
		text += " : " + QString::number(time) + " ms";
		d_data->text.setText(text);

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

		if (icon != d_data->icon) {
			d_data->icon = icon;
			d_data->text.setIcon(vipIcon(icon));
		}

		// format errors
		if (errors.size() && errors.last().msecsSinceEpoch() > d_data->lastErrorDate) {
			d_data->lastErrorDate = errors.last().msecsSinceEpoch();
			d_data->errors.clear();
			QString error_text;
			for (int i = 0; i < errors.size(); ++i) {
				VipErrorData err = errors[i];
				QString date = QDateTime::fromMSecsSinceEpoch(err.msecsSinceEpoch()).toString("yy:MM:dd-hh:mm:ss.zzz    ");
				QString err_str = err.errorString();
				QString code = QString::number(err.errorCode());
				error_text += date + err_str + " (" + code + ")\n";
			}
			d_data->errors.setPlainText(error_text);
		}
	}

	if (d_data->editor) {
		d_data->text.setChecked(d_data->editor->isVisible());
	}
}
void VipProcessingTooButton::showError(bool show_)
{
	d_data->errors.setVisible(show_);

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
	return &d_data->text;
}
QToolButton* VipProcessingTooButton::resetButton() const
{
	return &d_data->reset;
}

void VipProcessingTooButton::setEditor(VipUniqueProcessingObjectEditor* ed)
{
	d_data->editor = ed;
}
VipUniqueProcessingObjectEditor* VipProcessingTooButton::editor() const
{
	return d_data->editor;
}

void VipProcessingTooButton::resetProcessing()
{
	if (VipProcessingObject* obj = d_data->processing) {
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
	VIP_CREATE_PRIVATE_DATA(d_data);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(0);
	setLayout(lay);
}

VipMultiProcessingObjectEditor::~VipMultiProcessingObjectEditor()
{
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

	if (objs == d_data->processingObjects)
		return false;

	d_data->processingObjects = objs;
	d_data->editors.clear();

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
		editor->setShowExactProcessingOnly(d_data->isShowExactProcessingOnly);

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

			d_data->editors[objs[i]] = (QPair<VipProcessingTooButton*, QWidget*>(button, editor));

			connect(editor, SIGNAL(editorVisibilityChanged()), this, SLOT(emitEditorVisibilityChanged()));
		}
		else
			delete editor;
	}

	// TEST: comment
	// lay->addStretch(2);

	setVisibleProcessings(d_data->visibleProcessings);
	setHiddenProcessings(d_data->hiddenProcessings);

	Q_EMIT processingsChanged();

	return lay->count() > 1;
}

QList<VipProcessingObject*> VipMultiProcessingObjectEditor::processingObjects() const
{
	return d_data->processingObjects;
}

VipUniqueProcessingObjectEditor* VipMultiProcessingObjectEditor::processingEditor(VipProcessingObject* obj) const
{
	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = d_data->editors.find(obj);
	if (it != d_data->editors.end()) {
		if (VipUniqueProcessingObjectEditor* ed = qobject_cast<VipUniqueProcessingObjectEditor*>(it.value().second)) //->findChild<VipUniqueProcessingObjectEditor*>())
			return ed;
	}
	return nullptr;
}

void VipMultiProcessingObjectEditor::setProcessingObjectVisible(VipProcessingObject* object, bool visible)
{
	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = d_data->editors.find(object);
	if (it != d_data->editors.end()) {
		it->first->showButton()->setChecked(visible);
		it->second->setVisible(visible);
		emitEditorVisibilityChanged();
	}
}

void VipMultiProcessingObjectEditor::setFullEditorVisible(VipProcessingObject* object, bool visible)
{
	QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = d_data->editors.find(object);
	if (it != d_data->editors.end()) {
		it->first->setVisible(visible);
		it->second->setVisible(visible);
		emitEditorVisibilityChanged();
	}
}

void VipMultiProcessingObjectEditor::setShowExactProcessingOnly(bool exact_proc)
{
	d_data->isShowExactProcessingOnly = exact_proc;
	for (QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = d_data->editors.begin(); it != d_data->editors.end(); ++it) {
		if (VipUniqueProcessingObjectEditor* ed = qobject_cast<VipUniqueProcessingObjectEditor*>(it.value().second)) //->findChild<VipUniqueProcessingObjectEditor*>())
			return ed->setShowExactProcessingOnly(exact_proc);
	}
}

bool VipMultiProcessingObjectEditor::isShowExactProcessingOnly() const
{
	return d_data->isShowExactProcessingOnly;
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
	bool no_rules = d_data->visibleProcessings.isEmpty() && d_data->hiddenProcessings.isEmpty();
	bool changed = false;
	for (QMap<VipProcessingObject*, QPair<VipProcessingTooButton*, QWidget*>>::iterator it = d_data->editors.begin(); it != d_data->editors.end(); ++it) {
		bool visible = true;
		if (!no_rules) {
			if (!d_data->visibleProcessings.isEmpty())
				visible = isSuperClass(it.key()->metaObject(), d_data->visibleProcessings);
			if (visible && !d_data->hiddenProcessings.isEmpty())
				visible = !isSuperClass(it.key()->metaObject(), d_data->hiddenProcessings);
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
	d_data->visibleProcessings = proc_classes;
	updateEditorsVisibility();
}

void VipMultiProcessingObjectEditor::setHiddenProcessings(const QList<const QMetaObject*>& proc_classes)
{
	d_data->hiddenProcessings = proc_classes;
	updateEditorsVisibility();
}

QList<const QMetaObject*> VipMultiProcessingObjectEditor::visibleProcessings() const
{
	return d_data->visibleProcessings;
}
QList<const QMetaObject*> VipMultiProcessingObjectEditor::hiddenProcessings() const
{
	return d_data->hiddenProcessings;
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
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->leafSelector = new VipProcessingLeafSelector();
	d_data->mainWindow = window;

	QWidget* w = new QWidget();
	d_data->layout = new QVBoxLayout();
	d_data->layout->setContentsMargins(0, 0, 0, 0);
	w->setLayout(d_data->layout);

	QWidget* editor = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->leafSelector);
	vlay->addWidget(VipLineWidget::createHLine());
	vlay->addWidget(w);
	editor->setLayout(vlay);

	this->setWidget(editor);
	this->setWindowTitle("Edit processing");
	this->setObjectName("Edit processing");
	this->setAutomaticTitleManagement(false);

	connect(VipPlotItemManager::instance(), SIGNAL(itemClicked(const VipPlotItemPointer&, int)), this, SLOT(itemClicked(const VipPlotItemPointer&, int)), Qt::QueuedConnection);
	connect(
	  VipPlotItemManager::instance(), SIGNAL(itemSelectionChanged(const VipPlotItemPointer&, bool)), this, SLOT(itemSelectionChangedDirect(const VipPlotItemPointer&, bool)), Qt::DirectConnection);
	connect(window->displayArea(), SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(workspaceChanged()));
	connect(d_data->leafSelector, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(setProcessingObject(VipProcessingObject*)));

	if (VipDisplayPlayerArea* area = window->displayArea()->currentDisplayPlayerArea())
		d_data->leafSelector->setProcessingPool(area->processingPool());

	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

VipProcessingEditorToolWidget::~VipProcessingEditorToolWidget()
{
}

bool VipProcessingEditorToolWidget::setPlayer(VipAbstractPlayer* player)
{
	// just for ease of use: if no processing has been selected yet, assign a processing from this player
	if (!player) {
		d_data->leafSelector->setProcessing(nullptr);
		return false;
	}
	// if (player == d_data->player)
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
		if (d_data->player == player && processingObject())
			return true;
		// try to take the main display object (if any)
		if (VipDisplayObject* disp = player->mainDisplayObject())
			displays << disp;
		else
			displays = player->displayObjects();
	}
	if (displays.size()) {
		d_data->player = player;
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
			if (VipAbstractPlayer* pl = vipFindParent<VipAbstractPlayer>(disp->widget()))
				title = QString::number(pl->parentId()) + " " + pl->QWidget::windowTitle();
	}
	if (title.isEmpty())
		title = vipSplitClassname(object->objectName());

	this->setWindowTitle("Edit processing - " + title);

	VipMultiProcessingObjectEditor* editor = d_data->findEditor(object);
	if (editor)
		d_data->setEditor(object);
	else {
		QList<VipProcessingObject*> lst;
		lst << object;
		lst += object->allSources();

		editor = new VipMultiProcessingObjectEditor();
		editor->setShowExactProcessingOnly(d_data->isShowExactProcessingOnly);
		editor->setVisibleProcessings(d_data->visibleProcessings);
		editor->setHiddenProcessings(d_data->hiddenProcessings);
		editor->setProcessingObjects(lst);

		connect(editor, SIGNAL(editorVisibilityChanged()), this, SLOT(resetSize()));
		connect(editor, SIGNAL(processingsChanged()), this, SLOT(emitProcessingsChanged()), Qt::DirectConnection);
		d_data->setEditor(object, editor);
	}

	d_data->leafSelector->setProcessingPool(object->parentObjectPool());
	d_data->leafSelector->setProcessing(object);

	emitProcessingsChanged();
	resetSize();
}

VipProcessingObject* VipProcessingEditorToolWidget::processingObject() const
{
	return d_data->current_editor.second;
}

VipMultiProcessingObjectEditor* VipProcessingEditorToolWidget::editor() const
{
	return d_data->current_editor.first;
}

VipProcessingLeafSelector* VipProcessingEditorToolWidget::leafSelector() const
{
	return d_data->leafSelector;
}

void VipProcessingEditorToolWidget::setShowExactProcessingOnly(bool exact_proc)
{
	d_data->isShowExactProcessingOnly = exact_proc;

	for (QMap<VipProcessingObject*, editor_type>::iterator it = d_data->editors.begin(); it != d_data->editors.end(); ++it) {
		it.value().first->setShowExactProcessingOnly(exact_proc);
	}
}

bool VipProcessingEditorToolWidget::isShowExactProcessingOnly() const
{
	return d_data->isShowExactProcessingOnly;
}

void VipProcessingEditorToolWidget::setVisibleProcessings(const QList<const QMetaObject*>& proc_class_names)
{
	d_data->visibleProcessings = proc_class_names;

	for (QMap<VipProcessingObject*, editor_type>::iterator it = d_data->editors.begin(); it != d_data->editors.end(); ++it) {
		it.value().first->setVisibleProcessings(proc_class_names);
	}
}
void VipProcessingEditorToolWidget::setHiddenProcessings(const QList<const QMetaObject*>& proc_class_names)
{
	d_data->hiddenProcessings = proc_class_names;

	for (QMap<VipProcessingObject*, editor_type>::iterator it = d_data->editors.begin(); it != d_data->editors.end(); ++it) {
		it.value().first->setHiddenProcessings(proc_class_names);
	}
}

QList<const QMetaObject*> VipProcessingEditorToolWidget::visibleProcessings() const
{
	return d_data->visibleProcessings;
}
QList<const QMetaObject*> VipProcessingEditorToolWidget::hiddenProcessings() const
{
	return d_data->hiddenProcessings;
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

void VipProcessingEditorToolWidget::itemSelectionChangedDirect(const VipPlotItemPointer& item, bool selected)
{
	QMetaObject::invokeMethod(this, "itemSelectionChanged", Qt::QueuedConnection, Q_ARG(VipPlotItemPointer, VipPlotItemPointer(item)), Q_ARG(bool, selected));
}

void VipProcessingEditorToolWidget::itemSelectionChanged(const VipPlotItemPointer& item, bool)
{
	if(item)
		setPlotItem(item);
}

void VipProcessingEditorToolWidget::itemClicked(const VipPlotItemPointer& item, int button)
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
	if (!d_data->mainWindow)
		d_data->mainWindow = vipGetMainWindow();

	if (VipDisplayPlayerArea* area = d_data->mainWindow->displayArea()->currentDisplayPlayerArea())
		d_data->leafSelector->setProcessingPool(area->processingPool());
}

VipProcessingEditorToolWidget* vipGetProcessingEditorToolWidget(VipMainWindow* window)
{
	static VipProcessingEditorToolWidget* instance = new VipProcessingEditorToolWidget(window);
	return instance;
}




class VipRememberDeviceOptions::PrivateData
{
public:
	QMap<QString, QString> suffixToDeviceType;
	QMap<QString, VipIODevice*> deviceOptions;
};

VipRememberDeviceOptions::VipRememberDeviceOptions() 
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}
VipRememberDeviceOptions::~VipRememberDeviceOptions() 
{
	clearAll();
}

/// Returns a map of 'file_suffix' -> 'class_name'
const QMap<QString, QString>& VipRememberDeviceOptions::suffixToDeviceType() const
{
	return d_data->suffixToDeviceType;
}
void VipRememberDeviceOptions::setSuffixToDeviceType(const QMap<QString, QString>& m)
{
	d_data->suffixToDeviceType = m;
}
void VipRememberDeviceOptions::addSuffixAndDeviceType(const QString& suffix, const QString& device_type)
{
	d_data->suffixToDeviceType.insert(suffix, device_type);
}
void VipRememberDeviceOptions::clearSuffixAndDeviceType()
{
	d_data->suffixToDeviceType.clear();
}
QString VipRememberDeviceOptions::deviceTypeForSuffix(const QString& suffix) const
{
	auto it = d_data->suffixToDeviceType.find(suffix);
	if (it == d_data->suffixToDeviceType.end())
		return QString();
	return it.value();
}
VipIODevice* VipRememberDeviceOptions::deviceForSuffix(const QString& suffix) const
{
	QString type = deviceTypeForSuffix(suffix);
	if (type.isEmpty())
		return nullptr;
	return vipCreateVariant(((type.toLatin1()) + "*").data()).value<VipIODevice*>();
}

/// Returns a map of 'class_name' -> 'VipIODevice*'
const QMap<QString, VipIODevice*>& VipRememberDeviceOptions::deviceOptions() const
{
	return d_data->deviceOptions;
}
void VipRememberDeviceOptions::setDeviceOptions(const QMap<QString, VipIODevice*>& m)
{
	d_data->deviceOptions = m;
}
void VipRememberDeviceOptions::addDeviceOptions(const QString& device_type, VipIODevice* device)
{
	d_data->deviceOptions.insert(device_type, (device));
}
bool VipRememberDeviceOptions::addDeviceOptionsCopy(const VipIODevice* src_device)
{
	VipIODevice* copy=(vipCreateVariant((QByteArray(src_device->metaObject()->className()) + "*").data()).value<VipIODevice*>());
	if (!copy)
		return false;
	const_cast<VipIODevice*>(src_device)->copyParameters(copy);
	d_data->deviceOptions.insert(src_device->metaObject()->className(), copy);
	return true;
}
void VipRememberDeviceOptions::clearDeviceOptions()
{
	for (auto it = d_data->deviceOptions.begin(); it != d_data->deviceOptions.end(); ++it)
		delete it.value();
	d_data->deviceOptions.clear();
}
bool VipRememberDeviceOptions::applyDefaultOptions(VipIODevice* device)
{
	auto it = d_data->deviceOptions.find(device->metaObject()->className());
	if (it == d_data->deviceOptions.end())
		return false;
	it.value()->copyParameters(device);
	return true;
}
void VipRememberDeviceOptions::clearAll()
{
	clearSuffixAndDeviceType();
	clearDeviceOptions();
}

VipRememberDeviceOptions& VipRememberDeviceOptions::instance()
{
	static VipRememberDeviceOptions inst;
	return inst;
}





#include <QBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>


class VipSelectDeviceParameters::PrivateData
{
public:
	QWidget* editor;
	VipIODevice* device;
	QCheckBox remember;
};

VipSelectDeviceParameters::VipSelectDeviceParameters(VipIODevice* device, QWidget* editor, QWidget* parent)
  : QDialog(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	setWindowTitle("Edit " + vipSplitClassname(device->metaObject()->className()));

	d_data->device = device;
	d_data->editor = editor;

	QPushButton* ok = new QPushButton("Ok", this);
	ok->setMaximumWidth(70);
	QPushButton* cancel = new QPushButton("Cancel", this);
	cancel->setMaximumWidth(70);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addStretch(1);
	hlay->addWidget(ok);
	hlay->addWidget(cancel);
	hlay->addStretch(1);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(editor);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&d_data->remember);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addLayout(hlay);
	setLayout(lay);

	d_data->remember.setText("Remember my choices");

	connect(ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
}
	
VipSelectDeviceParameters::~VipSelectDeviceParameters() {}

bool VipSelectDeviceParameters::remember() const
{
	return d_data->remember.isChecked();
}



class VipDeviceChoiceDialog::PrivateData
{
public:
	QLabel text;
	QTreeWidget tree;
	QCheckBox remember;
	QList<VipIODevice*> devices;
};

VipDeviceChoiceDialog::VipDeviceChoiceDialog(QWidget* parent)
  : QDialog(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	QVBoxLayout* tree_lay = new QVBoxLayout();
	tree_lay->addWidget(&d_data->text);
	tree_lay->addWidget(&d_data->tree);

	d_data->tree.setItemsExpandable(false);
	d_data->tree.setRootIsDecorated(false);
	d_data->tree.setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->tree.setHeaderLabels(QStringList() << "Name"
						   << "Description"
						   << "Extensions");
	d_data->text.setText("Several devices can handle this format. Select one type of device to handle it.");
	d_data->text.setWordWrap(true);

	d_data->tree.header()->resizeSection(0, 200);
	d_data->tree.header()->resizeSection(1, 200);
	d_data->tree.header()->resizeSection(2, 120);

	d_data->tree.setMinimumHeight(50);
	d_data->tree.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	this->setMinimumHeight(50);
	this->setMinimumWidth(400);

	d_data->remember.setText("Remember my choice");

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
	vlay->addWidget(&d_data->remember);
	vlay->addLayout(lay);
	frame->setLayout(vlay);

	QVBoxLayout* final_lay = new QVBoxLayout();
	final_lay->setContentsMargins(0, 0, 0, 0);
	final_lay->addWidget(frame);
	setLayout(final_lay);

	connect(&d_data->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(accept()));
	connect(ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
	setWindowTitle("Select device");

	this->style()->unpolish(this);
	this->style()->polish(this);
}

VipDeviceChoiceDialog::~VipDeviceChoiceDialog()
{
}

void VipDeviceChoiceDialog::setChoices(const QList<VipIODevice*>& devices)
{
	d_data->devices = devices;
	d_data->tree.clear();
	for (int i = 0; i < devices.size(); ++i) {
		QString name = vipSplitClassname(devices[i]->className());

		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, name);
		item->setText(1, devices[i]->description());
		item->setToolTip(1, devices[i]->description());
		item->setText(2, devices[i]->fileFilters());
		item->setToolTip(2, devices[i]->fileFilters());
		d_data->tree.addTopLevelItem(item);
	}

	d_data->tree.setCurrentItem(d_data->tree.topLevelItem(0));
}

void VipDeviceChoiceDialog::setPath(const QString& path)
{
	d_data->text.setText("Several devices can handle this format. Select one type of device to handle it.<br>"
			     "<b>Path:</b>" +
			     path);
}

VipIODevice* VipDeviceChoiceDialog::selection() const
{
	QList<int> indexes;
	for (int i = 0; i < d_data->tree.topLevelItemCount(); ++i)
		if (d_data->tree.topLevelItem(i)->isSelected())
			return d_data->devices[i];

	return nullptr;
}

bool VipDeviceChoiceDialog::remember() const
{
	return d_data->remember.isChecked();
}

VipIODevice* VipCreateDevice::create(const QList<VipProcessingObject::Info>& dev, const VipPath& path, bool show_device_options)
{
	using DevicePtr = std::unique_ptr<VipIODevice>;

	DevicePtr result( VipRememberDeviceOptions::instance().deviceForSuffix(QFileInfo(path.canonicalPath()).suffix()));
	if (!result) {
		
		std::vector<DevicePtr> hold_devices;
		QList<VipIODevice*> devices;
		VipIODevice* found = nullptr;

		// first , create the list of devices
		for (int i = 0; i < dev.size(); ++i) {
			if (VipIODevice* d = qobject_cast<VipIODevice*>(dev[i].create())) {
				hold_devices.push_back( DevicePtr(d));
				devices.push_back(d);
			}
		}

		bool remember_device_type = false;
		if (devices.size() > 1) {
			// create the dialog used to use the device
			VipDeviceChoiceDialog* dialog = new VipDeviceChoiceDialog(vipGetMainWindow());
			dialog->setMinimumWidth(500);
			dialog->setChoices(devices);
			dialog->setPath(path.canonicalPath());
			if (dialog->exec() == QDialog::Accepted) {
				found = dialog->selection();
				remember_device_type = dialog->remember();
			}

			delete dialog;
			if (!found)
				return nullptr;
		}
		else if (devices.size() == 1)
			found = devices.first();
		else
			return nullptr;

		for (auto & d : hold_devices) {
			if (d.get() == found) {
				result = std::move(d);
				break;
			}
		}
		
		if (!path.isEmpty()) {

			// Remember user choice
			if (remember_device_type)
				VipRememberDeviceOptions::instance().addSuffixAndDeviceType(QFileInfo(path.canonicalPath()).suffix(), result->metaObject()->className());
		}
	}

	result->setPath(path.canonicalPath());
	result->setMapFileSystem(path.mapFileSystem());

	bool apply_options = VipRememberDeviceOptions::instance().applyDefaultOptions(result.get());

	// display device option widget
	if (!apply_options && show_device_options) {

		const auto lst = vipFDObjectEditor().exactMatch(result.get());
		if (lst.size()) {

			QWidget* editor = lst.first()(result.get()).value<QWidget*>();
			if (editor) {
				VipSelectDeviceParameters dialog(result.get(), editor, vipGetMainWindow());
				if (dialog.exec() != QDialog::Accepted) {
					return nullptr;
				}
				else {
					// try to find the "apply" slot and call it
					if (editor->metaObject()->indexOfMethod("apply()") >= 0)
						QMetaObject::invokeMethod(editor, "apply");
				}
				if (dialog.remember()) {
					VipRememberDeviceOptions::instance().addDeviceOptionsCopy(result.get());
				}
			}
		}
	}

	return result.release();
}

VipIODevice* VipCreateDevice::create(const VipPath& path, bool show_device_options)
{
	QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(path, QByteArray());
	return create(devices, path, show_device_options);
}
