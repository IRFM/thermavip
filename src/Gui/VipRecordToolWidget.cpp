/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipRecordToolWidget.h"
#include "VipDisplayArea.h"
#include "VipDisplayObject.h"
#include "VipDragWidget.h"
#include "VipGenericDevice.h"
#include "VipLogging.h"
#include "VipPlayer.h"
#include "VipPlotItem.h"
#include "VipProcessingObjectEditor.h"
#include "VipProgress.h"
#include "VipSet.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointer>
#include <QRadioButton>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

#include <iostream>

static void getNames(VipBaseDragWidget* player, VipPlotItem* item, QString& text, QString& tool_tip)
{
	QString player_name = player ? player->windowTitle() : QString();
	QString item_name = item->title().text();
	QString class_name = vipSplitClassname(item->metaObject()->className());
	text = item_name.size() ? item_name : player_name;
	tool_tip = "<div style = \"white-space:nowrap;\"><b>Item: </b>" + item_name + "<br><b>Player: </b>" + player_name + "<br><b>Type: </b> " + class_name + "</div>";
}

static VipOutput* ressourceSourceObject(VipDisplayObject* disp)
{
	if (!disp)
		return nullptr;

	VipProcessingObjectList src = disp->allSources();
	QList<VipIODevice*> devices = src.findAll<VipIODevice*>();

	// check if all source devices are Resource ones
	bool all_resource = true;
	for (int i = 0; i < devices.size(); ++i) {
		if (devices[i]->deviceType() != VipIODevice::Resource) {
			all_resource = false;
			break;
		}
	}

	if (all_resource) {
		if (VipOutput* out = disp->inputAt(0)->connection()->source())
			return out;
	}

	return nullptr;
}

class VipRecordToolBar::PrivateData
{
public:
	QToolButton* selectItems;
	QMenu* selectItemsMenu;
	VipFileName* filename;
	QToolButton* record;

	QAction* record_movie;
	QAction* record_signals;
};

VipRecordToolBar::VipRecordToolBar(VipRecordToolWidget* tool)
  : VipToolWidgetToolBar(tool)
{
	setObjectName("Record tool bar");
	setWindowTitle("Record tool bar");

	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->selectItems = new QToolButton(this);
	d_data->selectItemsMenu = new QMenu(d_data->selectItems);
	d_data->filename = new VipFileName();
	d_data->record = new QToolButton();

	d_data->selectItems->setAutoRaise(true);
	d_data->selectItems->setText("Record...");
	d_data->selectItems->setMenu(d_data->selectItemsMenu);
	d_data->selectItems->setPopupMode(QToolButton::InstantPopup);
	d_data->selectItems->setToolTip("Select signals to record or video to create");
	d_data->selectItems->setToolTip("<b>Shortcut:</b> select signals to record or video to create.<br><br>To see all recording features, click on the left icon.");

	d_data->selectItemsMenu->setToolTipsVisible(true);
	d_data->selectItemsMenu->setStyleSheet("QMenu::item{ margin-left : 10px; padding-left: 20px; padding-top :2px ; padding-right: 20px; padding-bottom: 2px; }"
					       "QMenu::item:enabled {margin-left: 20px;}"
					       "QMenu::item:disabled {margin-left: 10px; padding-top: 5px; padding-bottom: 5px; font: italic;}"
					       "QMenu::item:disabled:checked {background: #007ACC; color: white;}");

	d_data->filename->setMaximumWidth(200);
	d_data->filename->setFilename(tool->recordWidget()->filename());
	d_data->filename->setFilters(tool->recordWidget()->filenameWidget()->filters());
	d_data->filename->setMode(tool->recordWidget()->filenameWidget()->mode());
	d_data->filename->setDefaultPath(tool->recordWidget()->filenameWidget()->defaultPath());
	d_data->filename->setDefaultOpenDir(tool->recordWidget()->filenameWidget()->defaultOpenDir());
	d_data->filename->edit()->setPlaceholderText("Output filename");

	d_data->record->setAutoRaise(true);
	d_data->record->setCheckable(true);
	d_data->record->setIcon(vipIcon("RECORD.png"));
	d_data->record->setToolTip("Launch recording");

	addWidget(d_data->selectItems);
	addWidget(d_data->filename);
	addWidget(d_data->record);
	setIconSize(QSize(18, 18));

	connect(d_data->selectItemsMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));
	connect(d_data->selectItemsMenu, SIGNAL(triggered(QAction*)), this, SLOT(itemSelected(QAction*)));
	connect(d_data->filename, SIGNAL(changed(const QString&)), this, SLOT(updateRecorder()));
	connect(d_data->record, SIGNAL(clicked(bool)), this, SLOT(updateRecorder()));
	connect(tool->recordWidget()->filenameWidget(), SIGNAL(changed(const QString&)), this, SLOT(updateWidget()));
	connect(tool->recordWidget()->record(), SIGNAL(clicked(bool)), this, SLOT(updateWidget()));
	connect(tool->recordWidget(), SIGNAL(recordingChanged(bool)), this, SLOT(updateWidget()));
}

VipRecordToolBar::~VipRecordToolBar()
{
}

VipRecordToolWidget* VipRecordToolBar::toolWidget() const
{
	return static_cast<VipRecordToolWidget*>(VipToolWidgetToolBar::toolWidget());
}

VipFileName* VipRecordToolBar::filename() const
{
	return d_data->filename;
}

QToolButton* VipRecordToolBar::record() const
{
	return d_data->record;
}

void VipRecordToolBar::updateRecorder()
{
	if (toolWidget() && toolWidget()->recordWidget()->filename() != this->filename()->filename())
		toolWidget()->setFilename(this->filename()->filename());
	if (toolWidget() && toolWidget()->recordWidget()->record()->isChecked() != d_data->record->isChecked())
		toolWidget()->recordWidget()->enableRecording(d_data->record->isChecked());
}

void VipRecordToolBar::execMenu()
{
	d_data->selectItemsMenu->exec();
}

void VipRecordToolBar::updateWidget()
{
	d_data->record->blockSignals(true);
	d_data->filename->blockSignals(true);

	d_data->filename->setFilename(toolWidget()->recordWidget()->filename());
	d_data->filename->setFilters(toolWidget()->recordWidget()->filenameWidget()->filters());
	d_data->filename->setMode(toolWidget()->recordWidget()->filenameWidget()->mode());
	d_data->filename->setDefaultPath(toolWidget()->recordWidget()->filenameWidget()->defaultPath());
	d_data->filename->setDefaultOpenDir(toolWidget()->recordWidget()->filenameWidget()->defaultOpenDir());
	d_data->record->setChecked(toolWidget()->recordWidget()->record()->isChecked());

	d_data->record->blockSignals(false);
	d_data->filename->blockSignals(false);
}

void VipRecordToolBar::setDisplayPlayerArea(VipDisplayPlayerArea*) {}

void VipRecordToolBar::updateMenu()
{
	QString current_player = toolWidget()->currentPlayer();
	QList<VipPlotItem*> current_items = toolWidget()->selectedItems();

	d_data->selectItemsMenu->blockSignals(true);
	d_data->selectItemsMenu->clear();

	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		d_data->record_movie = d_data->selectItemsMenu->addAction("Create a video from player...");
		d_data->record_movie->setCheckable(true);
		d_data->record_movie->setChecked(toolWidget()->recordType() == VipRecordToolWidget::Movie);
		d_data->record_movie->setEnabled(false);

		// add all VipBaseDragWidget titles
		QList<VipBaseDragWidget*> players;

		QList<VipBaseDragWidget*> pls = area->findChildren<VipBaseDragWidget*>();
		for (int i = 0; i < pls.size(); ++i) {
			// only add the VipBaseDragWidget with a visible header
			// if (pls[i]->Header() && pls[i]->Header()->isVisible())
			{
				QAction* act = d_data->selectItemsMenu->addAction(pls[i]->windowTitle());
				act->setProperty("is_player", true);
				act->setCheckable(true);
				bool check = toolWidget()->recordType() != VipRecordToolWidget::Movie ? false : pls[i]->windowTitle() == current_player;
				act->setChecked(check);
				vip_debug("checked: %i\n", (int)check);
			}
		}

		d_data->selectItemsMenu->addSeparator();
		d_data->record_signals = d_data->selectItemsMenu->addAction("...Or record one or more signals:");
		d_data->record_signals->setCheckable(true);
		d_data->record_signals->setChecked(toolWidget()->recordType() != VipRecordToolWidget::Movie);
		d_data->record_signals->setEnabled(false);

		// add all possible plot items
		QList<QAction*> items = VipPlotItemSelector::createActions(VipPlotItemSelector::possibleItems(area), d_data->selectItemsMenu);
		for (int i = 0; i < items.size(); ++i) {
			d_data->selectItemsMenu->addAction(items[i]);
			items[i]->setCheckable(true);
			items[i]->setChecked(current_items.indexOf(items[i]->property("VipPlotItem").value<VipPlotItem*>()) >= 0);
			items[i]->setProperty("is_player", false);
		}
	}

	d_data->selectItemsMenu->blockSignals(false);
}

void VipRecordToolBar::itemSelected(QAction* act)
{
	bool is_player = act->property("is_player").toBool();

	if (is_player) {
		// this is a VipBaseDragWidget to save a movie
		if (act->isChecked()) {
			toolWidget()->setRecordType(VipRecordToolWidget::Movie);
			toolWidget()->setCurrentPlayer(act->text());

			// uncheck all other players
			QList<QAction*> acts = d_data->selectItemsMenu->actions();
			d_data->selectItemsMenu->blockSignals(true);
			for (int i = 0; i < acts.size(); ++i)
				if (acts[i]->property("is_player").toBool())
					if (acts[i] != act)
						acts[i]->setChecked(false);
			d_data->selectItemsMenu->blockSignals(false);
		}
	}
	else {
		toolWidget()->setRecordType(VipRecordToolWidget::SignalArchive);

		// add or remove the selected plot item
		if (act->isChecked())
			toolWidget()->addPlotItem(act->property("VipPlotItem").value<VipPlotItem*>());
		else
			toolWidget()->removePlotItem(act->property("VipPlotItem").value<VipPlotItem*>());

		QMetaObject::invokeMethod(this, "execMenu", Qt::QueuedConnection);
	}
}

class VipPlotItemSelector::PrivateData
{
public:
	QPointer<VipProcessingObject> processing;
	VipRecordToolWidget* parent;
	QMenu* menu;
};

VipPlotItemSelector::VipPlotItemSelector(VipRecordToolWidget* parent)
  : QToolButton(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->parent = parent;

	this->setText("Select a signal to record");
	this->setToolTip("<b> Select one or more signals (video, curve,...) you want to record</b><br>"
			 "If you select several signals, only the ARCH format (*.arch files) will be able to record them in a single archive.");
	d_data->menu = new QMenu(this);
	d_data->menu->setToolTipsVisible(true);
	this->setMenu(d_data->menu);
	this->setPopupMode(QToolButton::InstantPopup);

	connect(d_data->menu, SIGNAL(triggered(QAction*)), this, SLOT(processingSelected(QAction*)));
	connect(d_data->menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
}

VipPlotItemSelector::~VipPlotItemSelector()
{
}

VipProcessingPool* VipPlotItemSelector::processingPool() const
{
	return d_data->parent->processingPool();
}

QList<VipPlotItem*> VipPlotItemSelector::possibleItems(VipDisplayPlayerArea* area, const QList<VipPlotItem*>& current_items)
{
	QList<VipPlotItem*> res;
	if (area) {
		QList<VipAbstractPlayer*> players = area->findChildren<VipAbstractPlayer*>();

		for (int i = 0; i < players.size(); ++i) {
			QList<VipDisplayObject*> disp = players[i]->displayObjects();
			for (int d = 0; d < disp.size(); ++d) {
				if (VipDisplayPlotItem* di = qobject_cast<VipDisplayPlotItem*>(disp[d]))
					if (VipPlotItem* item = di->item())
						if (!item->title().isEmpty())
							if (current_items.indexOf(item) < 0)
								if (res.indexOf(item) < 0)
									res.append(item);
			}
		}
	}
	return res;
}

QList<VipPlotItem*> VipPlotItemSelector::possibleItems() const
{
	return possibleItems(d_data->parent->area(), d_data->parent->selectedItems());
}

QList<QAction*> VipPlotItemSelector::createActions(const QList<VipPlotItem*>& items, QObject* parent)
{
	QList<QAction*> res;
	for (int i = 0; i < items.size(); ++i) {
		QString text, tool_tip;
		VipAbstractPlayer* pl = VipAbstractPlayer::findAbstractPlayer(items[i]);
		VipBaseDragWidget* w = pl ? VipBaseDragWidget::fromChild(pl) : nullptr;
		getNames(w, items[i], text, tool_tip);
		QAction* act = new QAction(text, parent);
		act->setToolTip(tool_tip);
		act->setProperty("VipPlotItem", QVariant::fromValue(items[i]));
		res.append(act);
	}
	return res;
}

void VipPlotItemSelector::aboutToShow()
{
	d_data->menu->blockSignals(true);

	d_data->menu->clear();
	QList<VipPlotItem*> _leafs = this->possibleItems();
	QList<QAction*> actions = createActions(this->possibleItems(), d_data->menu);

	for (int i = 0; i < actions.size(); ++i) {
		d_data->menu->addAction(actions[i]);
		// QString text, tool_tip;
		//  VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(_leafs[i]);
		//  VipBaseDragWidget * w = pl ? VipBaseDragWidget::fromChild(pl) : nullptr;
		//  getNames(w, _leafs[i], text, tool_tip);
		//  QAction * act = d_data->menu->addAction(text);
		//  act->setToolTip(tool_tip);
		//  act->setProperty("VipPlotItem", QVariant::fromValue(_leafs[i]));
	}

	d_data->menu->blockSignals(false);
}

void VipPlotItemSelector::processingSelected(QAction* act)
{
	if (VipPlotItem* item = act->property("VipPlotItem").value<VipPlotItem*>()) {
		Q_EMIT itemSelected(item);
	}
}

class PlotListWidgetItem : public QListWidgetItem
{
public:
	PlotListWidgetItem(VipBaseDragWidget* player, VipPlotItem* item)
	  : QListWidgetItem(nullptr, QListWidgetItem::UserType)
	  , player(player)
	  , item(item)
	{
		QString text, tool_tip;
		getNames(player, item, text, tool_tip);
		setText(text);
		setToolTip(tool_tip);
	}

	QPointer<VipBaseDragWidget> player;
	QPointer<VipPlotItem> item;
};

class RecordListWidget : public QListWidget
{
	VipRecordToolWidget* record;
	QTimer timer;

public:
	int find(VipPlotItem* it)
	{
		for (int i = 0; i < count(); ++i)
			if (static_cast<PlotListWidgetItem*>(item(i))->item == it)
				return i;

		return -1;
	}

	RecordListWidget(VipRecordToolWidget* record)
	  : QListWidget()
	  , record(record)
	{
		timer.setSingleShot(false);
		timer.setInterval(500);
		connect(&timer, &QTimer::timeout, this, std::bind(&RecordListWidget::testItems, this));
		timer.start();
	}
	~RecordListWidget()
	{
		timer.stop();
		timer.disconnect();
	}

protected:
	void testItems()
	{
		bool has_delete = false;
		for (int i = 0; i < count(); ++i) {
			PlotListWidgetItem* it = static_cast<PlotListWidgetItem*>(item(i));
			if (!it->item || !it->player) {
				delete takeItem(i);
				--i;
				has_delete = true;
			}
		}
		if (has_delete)
			record->updateFileFiltersAndDevice();
	}
	virtual void keyPressEvent(QKeyEvent* evt)
	{
		if (evt->key() == Qt::Key_Delete) {
			qDeleteAll(this->selectedItems());
			record->updateFileFiltersAndDevice();
		}
		else if (evt->key() == Qt::Key_A && (evt->modifiers() & Qt::ControlModifier)) {
			for (int i = 0; i < count(); ++i)
				item(i)->setSelected(true);
			record->updateFileFiltersAndDevice();
		}
	}
};

class SkipFrame::PrivateData
{
public:
	QLabel text;
	QSpinBox frames;
};

SkipFrame::SkipFrame(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->text.setText("Take one frame out of");
	d_data->frames.setRange(1, INT_MAX);
	d_data->frames.setValue(1);

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addWidget(&d_data->text);
	lay->addWidget(&d_data->frames);
	setLayout(lay);
}
SkipFrame::~SkipFrame()
{
}

int SkipFrame::value()
{
	return d_data->frames.value();
}

void SkipFrame::setValue(int v)
{
	if (v != value())
		d_data->frames.setValue(v);
}
void SkipFrame::reset()
{
	setValue(1);
}

class VipRecordToolWidget::PrivateData
{
public:
	PrivateData()
	  : isRecording(false)
	  , recordType(SignalArchive)
	  , sourceWidget(nullptr)
	  , flag(VipIODevice::Resource)
	{
	}

	QRadioButton* saveMovie;
	QRadioButton* saveSignals;
	RecordListWidget* itemList;
	VipPlotItemSelector* itemSelector;
	QList<QPointer<VipBaseDragWidget>> playerlist;
	VipComboBox* players;
	VipDoubleEdit* samplingTime;
	SkipFrame* skipFrames;
	QWidget* samplingWidget;
	QLabel* playerPreview;

	VipRecordWidget* recordWidget;
	VipPenButton* backgroundColorButton;
	QCheckBox* transparentBackground;
	QCheckBox* recordSceneOnly;
	QWidget* streamingOptions;

	// buffer options
	QSpinBox* maxBufferSize;
	QSpinBox* maxBufferMemSize;
	QWidget* bufferOptions;

	QTimer timer;
	bool isRecording;
	RecordType recordType;

	// saving objects
	VipBaseDragWidget* sourceWidget;
	VipGenericRecorder* recorder;
	QList<VipIODevice*> sourceDevices;
	VipProcessingObjectList independantResourceProcessings;
	QList<QPointer<VipDisplayObject>> sourceDisplayObjects;
	VipProcessingObjectList sources;
	VipIODevice::DeviceType flag;
	VipRenderState state;
	QPixmap pixmap;

	QPointer<VipDisplayPlayerArea> area;
	QPointer<VipProcessingPool> pool;

	QPointer<VipRecordToolBar> toolBar;
};

VipRecordToolWidget::VipRecordToolWidget(VipMainWindow* window)
  : VipToolWidget(window)
{
	// setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	this->setAllowedAreas(Qt::NoDockWidgetArea);

	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->recorder = new VipGenericRecorder(this);
	// d_data->recorder->setMultiSave(true);
	d_data->itemSelector = new VipPlotItemSelector(this);
	d_data->itemList = new RecordListWidget(this);

	d_data->samplingTime = new VipDoubleEdit();
	d_data->saveMovie = new QRadioButton();
	d_data->transparentBackground = new QCheckBox();
	d_data->recordSceneOnly = new QCheckBox();
	d_data->backgroundColorButton = new VipPenButton();
	d_data->players = new VipComboBox();
	d_data->samplingWidget = new QWidget();
	d_data->playerPreview = new QLabel();
	d_data->saveSignals = new QRadioButton();
	d_data->skipFrames = new SkipFrame();
	d_data->recordWidget = new VipRecordWidget();
	d_data->recordWidget->filenameWidget()->edit()->setPlaceholderText("Output filename");

	d_data->maxBufferSize = new QSpinBox();
	d_data->maxBufferSize->setRange(1, 100000);
	d_data->maxBufferSize->setSuffix(" inputs");
	d_data->maxBufferSize->setToolTip("Maximum pending input data (data waiting to be saved)");
	d_data->maxBufferMemSize = new QSpinBox();
	d_data->maxBufferMemSize->setRange(1, 10000);
	d_data->maxBufferMemSize->setToolTip("Maximum pending input data size in MB (data waiting to be saved)");
	d_data->maxBufferMemSize->setSuffix(" MB");
	d_data->bufferOptions = new QWidget();
	QGridLayout* glay = new QGridLayout();
	glay->setSpacing(1);
	glay->setContentsMargins(0, 0, 0, 0);
	glay->addWidget(new QLabel("Max input count"), 0, 0);
	glay->addWidget(d_data->maxBufferSize, 0, 1);
	glay->addWidget(new QLabel("Max input size (MB)"), 1, 0);
	glay->addWidget(d_data->maxBufferMemSize, 1, 1);
	d_data->bufferOptions->setLayout(glay);
	d_data->bufferOptions->setVisible(false); // for streaming only

	QHBoxLayout* sampling_lay = new QHBoxLayout();
	sampling_lay->addWidget(d_data->samplingTime);
	sampling_lay->addWidget(new QLabel(" ms"));
	sampling_lay->setContentsMargins(0, 0, 0, 0);
	d_data->samplingWidget->setLayout(sampling_lay);
	d_data->samplingWidget->setVisible(false); // for streaming only

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(d_data->saveMovie);

	QHBoxLayout* back = new QHBoxLayout();
	back->setContentsMargins(0, 0, 0, 0);
	back->addWidget(d_data->transparentBackground);
	back->addWidget(d_data->backgroundColorButton);
	back->addStretch(1);

	lay->addLayout(back);
	lay->addWidget(d_data->recordSceneOnly);
	lay->addWidget(d_data->players);
	lay->addWidget(d_data->samplingWidget);
	lay->addWidget(d_data->skipFrames);
	lay->addWidget(d_data->playerPreview);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(d_data->saveSignals);
	lay->addWidget(d_data->itemSelector);
	lay->addWidget(d_data->itemList);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(d_data->bufferOptions);
	lay->addWidget(d_data->recordWidget);

	lay->addStretch(1);

	QWidget* w = new QWidget(this);
	w->setLayout(lay);
	this->setWidget(w);

	d_data->saveMovie->setText("Create a movie");
	d_data->saveMovie->setToolTip("Record a movie of type MPG, AVI, MP4,...\n"
				      "Select the player you wish to record from the list");
	d_data->saveSignals->setText("Record one or more raw signals");
	d_data->saveSignals->setToolTip("Record an archive of type ARCH, TXT,...\n"
					"Select the different plot items you wish\nto record from the available players.");
	d_data->players->setToolTip("Select a player to save");
	d_data->itemList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->itemList->setToolTip("List of signals to record");
	// d_data->playerPreview.setScaledContents(true);
	d_data->playerPreview->setToolTip("Player preview");
	d_data->samplingTime->setValue(20);
	d_data->samplingTime->setToolTip("Movie sampling time (save an image every sampling time ms)");
	d_data->backgroundColorButton->setMode(VipPenButton::Color);
	d_data->backgroundColorButton->setPen(QPen(QColor(255, 255, 255)));
	d_data->backgroundColorButton->setText("Select images background color");
	d_data->transparentBackground->setText("Background color ");
	d_data->transparentBackground->setChecked(true);

	d_data->recordSceneOnly->setText("Save player spectrogram only");
	d_data->recordSceneOnly->setToolTip("Selecting this option will ony save the spectrogram<br> with its exact geometry, without the color scale");
	d_data->recordSceneOnly->setChecked(false);

	d_data->recordWidget->setGenericRecorder(d_data->recorder);
	d_data->recorder->setRecorderAvailableDataOnOpen(false);

	d_data->saveSignals->setChecked(true);
	d_data->players->hide();
	d_data->playerPreview->hide();
	d_data->transparentBackground->hide();
	d_data->recordSceneOnly->hide();
	d_data->backgroundColorButton->hide();
	d_data->samplingWidget->hide();
	d_data->skipFrames->hide();
	d_data->itemList->hide();

	d_data->maxBufferSize->setValue(INT_MAX);
	d_data->maxBufferMemSize->setValue(500);
	d_data->recorder->topLevelInputAt(0)->toMultiInput()->setListType(VipDataList::FIFO, VipDataList::MemorySize, INT_MAX, 500000000);

	connect(VipPlotItemManager::instance(), SIGNAL(itemClicked(const VipPlotItemPointer&, int)), this, SLOT(itemClicked(const VipPlotItemPointer&, int)));
	connect(d_data->itemSelector, SIGNAL(itemSelected(VipPlotItem*)), this, SLOT(addPlotItem(VipPlotItem*)));
	connect(d_data->saveMovie, SIGNAL(clicked(bool)), this, SLOT(recordTypeChanged()));
	connect(d_data->saveSignals, SIGNAL(clicked(bool)), this, SLOT(recordTypeChanged()));
	connect(d_data->players, SIGNAL(openPopup()), this, SLOT(displayAvailablePlayers()));
	connect(d_data->players, SIGNAL(currentTextChanged(const QString&)), this, SLOT(playerSelected()));
	connect(d_data->maxBufferMemSize, SIGNAL(valueChanged(int)), this, SLOT(updateBuffer()));
	connect(d_data->maxBufferSize, SIGNAL(valueChanged(int)), this, SLOT(updateBuffer()));
	connect(d_data->recorder, SIGNAL(openModeChanged(bool)), this, SLOT(launchRecord(bool)), Qt::QueuedConnection);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(timeout()), Qt::QueuedConnection);
	// connect(&d_data->itemList,SIGNAL(currentItemChanged(QListWidgetItem *,QListWidgetItem *)),this,SLOT(updateFileFiltersAndDevice()));

	setObjectName("Record tools");
	setWindowTitle("Recording tools");
	resetSize();
}

VipRecordToolWidget::~VipRecordToolWidget()
{
}

VipVideoPlayer* VipRecordToolWidget::selectedVideoPlayer() const
{
	if (qobject_cast<VipDragWidget*>(d_data->sourceWidget))
		return d_data->sourceWidget->findChild<VipVideoPlayer*>();
	return nullptr;
}

bool VipRecordToolWidget::updateFileFiltersAndDevice(bool build_connections, bool close_device)
{
	// when selecting a new VipPlotItem or a new VipAbstractPlayer, update the ReordWidget file filters.
	// Also update the GenericDevice by setting its input.
	// Finally, save the sources VipIODevice and VipDisplayObject.
	// This way, when launching the recording, everything will be ready for the saving.

	
	if (close_device)
		d_data->recorder->close();

	// disconnect all inputs
	for (int i = 0; i < d_data->recorder->inputCount(); ++i)
		d_data->recorder->inputAt(i)->clearConnection();

	d_data->recordWidget->record()->setEnabled(false);
	if (toolBar())
		toolBar()->record()->setEnabled(false);

	d_data->sourceDisplayObjects.clear();
	d_data->sourceDevices.clear();
	d_data->independantResourceProcessings.clear();
	d_data->sources.clear();
	d_data->sourceWidget = nullptr;

	// first, retrieve the sources VipDisplayObject

	if (recordType() == Movie) {
		if (d_data->players->currentIndex() < 0)
			return false;

		d_data->sourceWidget = d_data->playerlist[d_data->players->currentIndex()];
		if (d_data->sourceWidget) {
			QList<VipAbstractPlayer*> players = d_data->sourceWidget->findChildren<VipAbstractPlayer*>();
			for (int i = 0; i < players.size(); ++i) {
				QList<VipDisplayObject*> displays = players[i]->displayObjects();
				for (int j = 0; j < displays.size(); ++j)
					d_data->sourceDisplayObjects << displays[j];
			}
		}
		else
			return false;
	}
	else {
		for (int i = 0; i < d_data->itemList->count(); ++i) {
			VipPlotItem* item = static_cast<PlotListWidgetItem*>(d_data->itemList->item(i))->item;
			if (item) {
				if (VipDisplayObject* disp = item->property("VipDisplayObject").value<VipDisplayObject*>())
					if (disp->inputAt(0)->connection()->source())
						d_data->sourceDisplayObjects << disp;
			}
		}
	}

	if (!d_data->sourceDisplayObjects.size())
		return false;

	// get the sources.
	// just in case, we also need the processings AFTER the sources, because a
	// few processings work in-place (they just modify their input data, for instance in the Tokida plugin).
	// This also means that we need the sources of these additional processings (arf...)
	// To sumurize, we need ALL processings involved somehow in the selected processings pipelines.
	d_data->sourceDevices.clear();
	for (int i = 0; i < d_data->sourceDisplayObjects.size(); ++i) {
		VipProcessingObjectList pipeline = d_data->sourceDisplayObjects[i]->fullPipeline();
		QList<VipIODevice*> devices;
		// bool have_no_resource = false;
		for (int p = 0; p < pipeline.size(); ++p) {
			if (VipIODevice* dev = qobject_cast<VipIODevice*>(pipeline[p])) {
				// only consider read only devices
				if ((dev->openMode() & VipIODevice::ReadOnly)) {
					devices.append(dev);
					d_data->sources.append(dev);
					// if (dev->deviceType() != VipIODevice::Resource)
					//	have_no_resource = true;
				}
			}
			else if (!qobject_cast<VipDisplayObject*>(pipeline[p])) {
				d_data->sources.append(pipeline[p]);
			}
		}

		d_data->sourceDevices += devices;
		// if (!have_no_resource)
		//  d_data->independantResourceProcessings += devices;
	}

	if (!d_data->sourceDevices.size())
		return false;

	// reset inputs
	d_data->recorder->topLevelInputAt(0)->toMultiInput()->clear();
	if (recordType() == SignalArchive)
		d_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(d_data->sourceDisplayObjects.size());
	else
		d_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(1);

	// now, update the VipRecordWidget file filters
	QVariantList lst;
	if (d_data->recordType == Movie) {
		// set a QImage input data to the VipGenericRecorder and update the file filters
		d_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(1);
		lst.append(QVariant::fromValue(vipToArray(QImage())));
		d_data->recorder->setProbeInputs(lst);
	}
	else {
		// use the selected VipPlotItem
		for (int i = 0; i < d_data->itemList->count(); ++i) {
			if (VipPlotItem* item = static_cast<PlotListWidgetItem*>(d_data->itemList->item(i))->item)
				if (VipDisplayObject* display = item->property("VipDisplayObject").value<VipDisplayObject*>())
					if (VipOutput* output = display->inputAt(0)->connection()->source())
						lst.append(output->data().data());
		}

		d_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(lst.size());
		d_data->recorder->setProbeInputs(lst);
	}

	// update the file filters of the VipRecordWidget. This will also clear the previously added input data
	QString filters = d_data->recordWidget->updateFileFilters(lst);
	if (toolBar())
		toolBar()->filename()->setFilters(filters);

	// find the source VipIODevice type.
	// we cannot mix Sequential and Temporal devices

	d_data->flag = VipIODevice::Resource;
	for (int i = 0; i < d_data->sourceDevices.size(); ++i) {
		VipIODevice::DeviceType tmp = d_data->sourceDevices[i]->deviceType();
		if (tmp == VipIODevice::Temporal) {
			if (d_data->flag == VipIODevice::Sequential) {
				VIP_LOG_ERROR("cannot mix sequential and temporal devices");
				return false;
			}
			d_data->flag = VipIODevice::Temporal;
		}
		else if (tmp == VipIODevice::Sequential) {
			if (d_data->flag == VipIODevice::Temporal) {
				VIP_LOG_ERROR("cannot mix sequential and temporal devices");
				return false;
			}
			d_data->flag = VipIODevice::Sequential;
		}
	}

	// Finally, setup the input connections
	// connect all signals to record to the recorder inputs

	if (build_connections) {
		if (recordType() == SignalArchive) {
			for (int i = 0; i < d_data->sourceDisplayObjects.size(); ++i)
				if (VipDisplayObject* disp = d_data->sourceDisplayObjects[i])
					if (VipOutput* out = disp->inputAt(0)->connection()->source())
						out->setConnection(d_data->recorder->inputAt(i));
		}

		if (d_data->flag == VipIODevice::Sequential)
			d_data->recorder->setScheduleStrategy(VipProcessingObject::Asynchronous, true);
		else
			d_data->recorder->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	}

	// we reach the end: enable the recording
	d_data->recordWidget->record()->setEnabled(true);
	d_data->recordWidget->suspend()->setVisible((d_data->flag == VipIODevice::Sequential) && !close_device);
	if (toolBar())
		toolBar()->record()->setEnabled(true);

	d_data->recordSceneOnly->setVisible(d_data->recordType == Movie && selectedVideoPlayer());

	// show streaming options
	d_data->samplingWidget->setVisible(d_data->recordType == Movie && d_data->flag == VipIODevice::Sequential);
	d_data->skipFrames->setVisible(d_data->recordType == Movie && d_data->flag != VipIODevice::Sequential);
	d_data->bufferOptions->setVisible(d_data->recordType == SignalArchive && d_data->flag == VipIODevice::Sequential);

	return true;
}

void VipRecordToolWidget::setRecordType(RecordType type)
{
	if (type != d_data->recordType) {
		d_data->itemList->setVisible(d_data->itemList->count() && type == SignalArchive);
		d_data->itemSelector->setVisible(type == SignalArchive);
		d_data->players->setVisible(type == Movie);
		d_data->playerPreview->setVisible(type == Movie);
		d_data->backgroundColorButton->setVisible(type == Movie);
		d_data->transparentBackground->setVisible(type == Movie);
		d_data->recordSceneOnly->setVisible(type == Movie && selectedVideoPlayer());
		d_data->saveMovie->blockSignals(true);
		d_data->saveSignals->blockSignals(true);
		d_data->saveMovie->setChecked(type == Movie);
		d_data->saveSignals->setChecked(type != Movie);
		d_data->saveMovie->blockSignals(false);
		d_data->saveSignals->blockSignals(false);

		d_data->recordType = type;

		// d_data->samplingWidget->setVisible(type == Movie);
		// d_data->bufferOptions->setVisible(type == SignalArchive);

		resetSize();
	}

	updateFileFiltersAndDevice();
}

void VipRecordToolWidget::updateBuffer()
{
	if (!d_data->recorder->isOpen())
		d_data->recorder->topLevelInputAt(0)->toMultiInput()->setListType(
		  VipDataList::FIFO, VipDataList::MemorySize | VipDataList::Number, d_data->maxBufferSize->value(), d_data->maxBufferMemSize->value() * 1000000);
}

VipRecordToolWidget::RecordType VipRecordToolWidget::recordType() const
{
	return d_data->recordType;
}

void VipRecordToolWidget::recordTypeChanged()
{
	if (d_data->saveSignals->isChecked()) {
		setRecordType(SignalArchive);
	}
	else {
		setRecordType(Movie);
	}

	this->resetSize();
}

void VipRecordToolWidget::setDisplayPlayerArea(VipDisplayPlayerArea* area)
{
	static QMap<VipDisplayPlayerArea*, QList<QPointer<VipPlotItem>>> area_items;

	if (area == d_data->area)
		return;

	// save the content of the item list for the previous area
	if (d_data->area) {
		area_items[d_data->area].clear();
		for (int i = 0; i < d_data->itemList->count(); ++i) {
			PlotListWidgetItem* it = static_cast<PlotListWidgetItem*>(d_data->itemList->item(i));
			if (it->item)
				area_items[d_data->area].append(it->item.data());
		}
	}
	// set the items that were saved for the new area
	QList<QPointer<VipPlotItem>> items = area_items[area];
	while (d_data->itemList->count())
		delete d_data->itemList->takeItem(0);
	for (int i = 0; i < items.size(); ++i) {
		VipAbstractPlayer* pl = VipAbstractPlayer::findAbstractPlayer(items[i]);
		VipBaseDragWidget* w = pl ? VipBaseDragWidget::fromChild(pl) : nullptr;
		d_data->itemList->addItem(new PlotListWidgetItem(w, items[i]));
	}

	d_data->area = area;
	if (area)
		d_data->pool = area->processingPool();
	else
		d_data->pool = nullptr;
}

VipRecordToolBar* VipRecordToolWidget::toolBar()
{
	// Disable for now
	return nullptr;
	/* if (!d_data->toolBar)
	{
		d_data->toolBar = new VipRecordToolBar(this);
	}
	return d_data->toolBar;*/
}

VipDisplayPlayerArea* VipRecordToolWidget::area() const
{
	return d_data->area;
}

VipProcessingPool* VipRecordToolWidget::processingPool() const
{
	return d_data->pool;
}

QList<VipPlotItem*> VipRecordToolWidget::selectedItems() const
{
	QList<VipPlotItem*> items;
	for (int i = 0; i < d_data->itemList->count(); ++i) {
		PlotListWidgetItem* it = static_cast<PlotListWidgetItem*>(d_data->itemList->item(i));
		if (it->item)
			items << it->item.data();
	}
	return items;
}

VipPlotItemSelector* VipRecordToolWidget::leafSelector() const
{
	return d_data->itemSelector;
}

void VipRecordToolWidget::setBackgroundColor(const QColor& c)
{
	d_data->backgroundColorButton->setPen(QPen(c));
}

QColor VipRecordToolWidget::backgroundColor() const
{
	QColor c = d_data->transparentBackground->isChecked() ? d_data->backgroundColorButton->pen().color() : QColor(255, 255, 255, 1);
	if (c.alpha() == 0)
		c.setAlpha(1);
	return c;
}

void VipRecordToolWidget::displayAvailablePlayers()
{
	displayAvailablePlayers(isVisible());
}

QString VipRecordToolWidget::currentPlayer() const
{
	return d_data->players->currentText();
}

void VipRecordToolWidget::setCurrentPlayer(const QString& player)
{
	if (d_data->players->count() == 0)
		this->displayAvailablePlayers();
	d_data->players->setCurrentText(player);
}

void VipRecordToolWidget::setFilename(const QString& filename)
{
	d_data->recordWidget->setFilename(filename);
	if (toolBar())
		toolBar()->filename()->setFilename(filename);
}

QString VipRecordToolWidget::filename() const
{
	return d_data->recordWidget->filename();
}

void VipRecordToolWidget::setRecordSceneOnly(bool enable)
{
	d_data->recordSceneOnly->setChecked(enable);
}
bool VipRecordToolWidget::recordSceneOnly() const
{
	return d_data->recordSceneOnly->isChecked();
}

VipRecordWidget* VipRecordToolWidget::recordWidget() const
{
	return d_data->recordWidget;
}

void VipRecordToolWidget::displayAvailablePlayers(bool update_player_pixmap)
{
	// update the combo box which displays the list of available players
	QString current_text = d_data->players->currentText();

	d_data->players->blockSignals(true);
	d_data->players->clear();
	d_data->playerlist.clear();
	VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	QList<VipBaseDragWidget*> players;

	if (area)
		players = area->findChildren<VipBaseDragWidget*>();

	for (int i = 0; i < players.size(); ++i) {
		// only add the VipBaseDragWidget with a visible header
		// if(players[i]->Header() && players[i]->Header()->isVisible())
		{
			d_data->players->addItem(players[i]->windowTitle());
			d_data->playerlist.append(players[i]);
		}
	}

	d_data->players->setCurrentText(current_text);
	d_data->players->blockSignals(false);

	if (update_player_pixmap)
		playerSelected();
	else
		updateFileFiltersAndDevice();
}

void VipRecordToolWidget::playerSelected()
{
	// a player is selected through the combo box: display its content in a QLabel
	if (d_data->players->currentIndex() < 0 || d_data->players->currentIndex() >= d_data->playerlist.size())
		return;

	VipBaseDragWidget* player = d_data->playerlist[d_data->players->currentIndex()];
	if (player) {
		QPixmap pixmap(player->size());
		if (pixmap.width() == 0 || pixmap.height() == 0)
			return;

		pixmap.fill(Qt::transparent);
		//{
		// QPainter p(&pixmap);
		// p.setCompositionMode(QPainter::CompositionMode_Clear);
		// p.fillRect(0, 0, pixmap.width(), pixmap.height(), QColor(0, 0, 0, 0));
		// }

		VipRenderObject::startRender(player, d_data->state);
		vipProcessEvents();

		{
			QPainter p(&pixmap);
			// player->draw(&p);
			VipRenderObject::renderObject(player, &p, QPoint(0, 0), true, false);
		}

		VipRenderObject::endRender(player, d_data->state);

		int max_dim = qMax(pixmap.width(), pixmap.height());
		double factor = 300.0 / max_dim;
		pixmap = pixmap.scaled(pixmap.width() * factor, pixmap.height() * factor, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

		d_data->playerPreview->setPixmap(pixmap);
		this->resetSize();
	}

	updateFileFiltersAndDevice();
}

bool VipRecordToolWidget::addPlotItem(VipPlotItem* item)
{
	if (d_data->itemList->find(item) >= 0)
		return false;

	VipAbstractPlayer* pl = VipAbstractPlayer::findAbstractPlayer(item);
	VipBaseDragWidget* w = pl ? VipBaseDragWidget::fromChild(pl) : nullptr;
	// only add if the VipPlotItem * is related to a VipDisplayObject
	if (item->property("VipDisplayObject").value<VipDisplayObject*>()) {
		d_data->itemList->addItem(new PlotListWidgetItem(w, item));

		d_data->itemList->setMinimumHeight(30 * d_data->itemList->count());
		d_data->itemList->setVisible(d_data->itemList->count());

		// update the file filters
		updateFileFiltersAndDevice();

		this->resetSize();
		return true;
	}
	return false;
}

bool VipRecordToolWidget::removePlotItem(VipPlotItem* item)
{
	int row = d_data->itemList->find(item);
	if (row >= 0) {
		delete d_data->itemList->takeItem(row);
		// update the file filters
		updateFileFiltersAndDevice();
		return true;
	}
	return false;
}

void VipRecordToolWidget::itemClicked(const VipPlotItemPointer& plot_item, int button)
{
	if (d_data->recordType == SignalArchive && isVisible()) {
		// add the plot item from the list if: this is a left click, the item is selected and not already added to the list
		if (plot_item && button == VipPlotItem::LeftButton && plot_item->isSelected() && d_data->itemList->find(plot_item) < 0) {
			addPlotItem(plot_item);
		}
	}
}

void VipRecordToolWidget::timeout()
{
	if (recordType() == Movie && d_data->sourceWidget && d_data->recorder->isOpen()) {
		// qint64 time = QDateTime::currentMSecsSinceEpoch();
		// check that at least one source VipIODevice have streaming enabled
		bool has_streaming = false;
		for (int i = 0; i < d_data->sourceDevices.size(); ++i)
			if (d_data->sourceDevices[i]->isStreamingEnabled()) {
				has_streaming = true;
				break;
			}

		if (!has_streaming)
			return;

		// vipProcessEvents();
		if (!d_data->sourceWidget)
			return;

		QSize size = d_data->sourceWidget->size();
		if (size != d_data->pixmap.size())
			d_data->pixmap = QPixmap(size);

		QColor c = backgroundColor();
		d_data->pixmap.fill(c);

		{
			QPainter p(&d_data->pixmap);
			p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
			VipRenderObject::renderObject(d_data->sourceWidget, &p, QPoint(0, 0), true, false);
		}

		VipAnyData any(QVariant::fromValue(vipToArray(d_data->pixmap.toImage())), QDateTime::currentMSecsSinceEpoch() * 1000000);
		d_data->recorder->inputAt(0)->setData(any);
		// d_data->output->update();
	}
}
// void VipRecordToolWidget::setPlayerToDevice()
// {
// //set the output device player
// if (qobject_cast<VipDragWidget*>(d_data->sourceWidget))
// if (VipAbstractPlayer * player = d_data->sourceWidget->findChild<VipAbstractPlayer*>())
// {
//	if(d_data->recorder && d_data->recorder->recorder())
//	d_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
// }
// }

void VipRecordToolWidget::launchRecord(bool launch)
{
	if (!launch) {
		// stop the timer
		d_data->timer.stop();
		vipProcessEvents();

		// d_data->recordWidget.stopRecording();
		d_data->recorder->close();
		updateFileFiltersAndDevice();
		d_data->recorder->setEnabled(false);

		// end saving: cleanup
		if (this->recordType() == Movie)
			VipRenderObject::endRender(d_data->sourceWidget, d_data->state);

		// TOCHECK:
		// We comment this as it disable the possibility to reuse the saver parameters
		// d_data->recorder->setRecorder(nullptr);

		// d_data->sourceWidget = nullptr;
		return;
	}

	if (d_data->recordWidget->path().isEmpty())
		return launchRecord(false);

	// actually build the connections
	updateFileFiltersAndDevice(true, false);

	// check that the selected display objects are still valid
	for (int i = 0; i < d_data->sourceDisplayObjects.size(); ++i) {
		if (!d_data->sourceDisplayObjects[i]) {
			VIP_LOG_ERROR("Unable to record: one or more selected items have been closed");
			return launchRecord(false);
		}
	}

	if (!d_data->sourceDevices.size())
		return launchRecord(false);

	VipProcessingPool* pool = d_data->sourceDevices.first()->parentObjectPool();
	if (!pool)
		return launchRecord(false);

	if (recordType() == Movie) {
		if (!d_data->sourceWidget) {
			VIP_LOG_ERROR("No valid selected player for video saving");
			return launchRecord(false);
		}
		// for a movie, prepare the source widget for rendering
		VipRenderObject::startRender(d_data->sourceWidget, d_data->state);
		vipProcessEvents();
	}

	// set the output device player
	if (qobject_cast<VipDragWidget*>(d_data->sourceWidget)) {
		if (VipAbstractPlayer* player = d_data->sourceWidget->findChild<VipAbstractPlayer*>()) {
			d_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
		}
	}
	else {
		if (d_data->itemList->count() == 1) {
			PlotListWidgetItem* item = static_cast<PlotListWidgetItem*>(d_data->itemList->item(0));
			VipAbstractPlayer* player = nullptr;
			if (d_data->sourceWidget)
				player = d_data->sourceWidget->findChild<VipAbstractPlayer*>();
			if (!player)
				player = VipAbstractPlayer::findAbstractPlayer(item->item);
			d_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
		}
	}

	// for temporal devices, save the archive right now
	if (d_data->flag == VipIODevice::Temporal || d_data->flag == VipIODevice::Resource) {

		// now, save the current VipProcessingPool state, because we are going to modify it heavily
		pool->save();
		d_data->sources.save();
		VipProcessingObjectList(d_data->sourceDisplayObjects).save();

		// disable all processing except the sources, remove the Automatic flag from the sources
		if (recordType() == SignalArchive) {
			pool->blockSignals(true);
			pool->disableExcept(d_data->sources);
			foreach (VipProcessingObject* obj, d_data->sources) {
				obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
			}
		}
		// if saving a movie, we must enable the VipDisplayObject and set everything to Automatic
		else {
			pool->disableExcept(d_data->sources);
			foreach (VipProcessingObject* obj, d_data->sources) {
				obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
			}

			for (int i = 0; i < d_data->sourceDisplayObjects.size(); ++i) {
				d_data->sourceDisplayObjects[i]->setEnabled(true);
				d_data->sourceDisplayObjects[i]->setScheduleStrategy(VipProcessingObject::Asynchronous, true);
			}
		}

		VipProgress progress;
		progress.setModal(true);
		progress.setCancelable(true);
		progress.setText("<b>Saving</b> " + QFileInfo(d_data->recorder->path()).fileName() + "...");

		qint64 time = pool->firstTime();
		qint64 end_time = pool->lastTime();
		// qint64 current_time = pool->time();
		progress.setRange(time / 1000000.0, end_time / 1000000.0);

		// movie sampling time (default: 20ms)
		qint64 movie_sampling_time = d_data->samplingTime->value() * 1000000;
		qint64 previous_time = VipInvalidTime;

		d_data->recorder->setEnabled(true);

		VipProcessingObjectList leafs = pool->leafs(false);
		leafs.save();
		leafs.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		if (recordType() == SignalArchive) {
			// disable the display objects among the leafs
			leafs.findAllProcessings<VipDisplayObject*>().setEnabled(false);
		}

		// We must distinguish 2 specific cases:
		//  - if the pool is a Resource, we just call pool->reload() once
		//  - if the pool is Temporal, but with the same (or invalid) start and end time. In this case, just like Resource, we call pool->reload() once.
		bool save_resource = (d_data->flag == VipIODevice::Resource) || (time == VipInvalidTime || time == end_time);

		if (!save_resource && recordType() == SignalArchive) {
			// When recording a temporal signal archive, we might have Resource input devices that is NOT, IN ANY WAY, linked to a temporal device.
			// In this case, the data from these devices won't be recorder.
			// So first, we reload these devices to record their data.

			for (int i = 0; i < d_data->sourceDisplayObjects.size(); ++i) {
				if (VipOutput* out = ressourceSourceObject(d_data->sourceDisplayObjects[i])) {
					d_data->recorder->inputAt(i)->setData(out->data());
					d_data->recorder->update();
				}
			}

			// for (int i = 0; i < d_data->independantResourceProcessings.size(); ++i)
			//  {
			//  d_data->independantResourceProcessings[i]->reload();
			//  leafs.update(d_data->recorder);
			//  //update the recorder last
			//  d_data->recorder->update();
			//  }
		}

		QPen pen;
		bool show_axes = true;
		if (recordSceneOnly() && selectedVideoPlayer()) {
			pen = selectedVideoPlayer()->spectrogram()->borderPen();
			selectedVideoPlayer()->spectrogram()->setBorderPen(Qt::NoPen);
			show_axes = selectedVideoPlayer()->isShowAxes();
			selectedVideoPlayer()->showAxes(false);
		}

		int skip = d_data->skipFrames->value();
		int skip_count = 0;

		while ((time != VipInvalidTime && time <= end_time) || save_resource) {

			progress.setValue(time / 1000000.0);

			if (save_resource)
				pool->reload();
			else
				pool->read(time, true);

			if (recordType() == SignalArchive) {
				leafs.update(d_data->recorder);
				// update the recorder last
				d_data->recorder->update();
			}
			else if (previous_time == VipInvalidTime || (time - previous_time) >= movie_sampling_time) {
				leafs.update(d_data->recorder);
				// wait for displays
				for (int i = 0; i < d_data->sourceDisplayObjects.size(); ++i)
					d_data->sourceDisplayObjects[i]->update();
				vipProcessEvents();

				if (++skip_count == skip) {
					skip_count = 0;
					if (recordSceneOnly() && selectedVideoPlayer()) {
						VipAbstractPlotWidget2D* plot = d_data->sourceWidget->findChild<VipAbstractPlotWidget2D*>();
						VipPlotSpectrogram* spec = selectedVideoPlayer()->spectrogram();
						spec->setBorderPen(Qt::NoPen);
						QRectF scene_rect = spec->mapToScene(spec->sceneMap()->clipPath(spec)).boundingRect();
						QRect view_rect = plot->mapFromScene(scene_rect).boundingRect();

						QSize size = view_rect.size();
						if (size != d_data->pixmap.size())
							d_data->pixmap = QPixmap(size);

						QColor c = backgroundColor();
						d_data->pixmap.fill(c);

						{
							QPainter p(&d_data->pixmap);
							p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
							VipRenderObject::renderObject(plot, &p, -view_rect.topLeft(), true, false);
						}

						VipAnyData any(QVariant::fromValue(vipToArray(d_data->pixmap.toImage())), time);
						d_data->recorder->inputAt(0)->setData(any);
						d_data->recorder->update();
					}
					else {
						QSize size = d_data->sourceWidget->size();
						if (size != d_data->pixmap.size())
							d_data->pixmap = QPixmap(size);

						QColor c = backgroundColor();
						d_data->pixmap.fill(c);

						{
							QPainter p(&d_data->pixmap);
							p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
							VipRenderObject::renderObject(d_data->sourceWidget, &p, QPoint(), true, false);
						}

						VipAnyData any(QVariant::fromValue(vipToArray(d_data->pixmap.toImage())), time);
						d_data->recorder->inputAt(0)->setData(any);
						d_data->recorder->update();
					}
				}
			}

			qint64 current = time;
			time = pool->nextTime(time);
			if (time == current || progress.canceled())
				break;

			save_resource = false;
		}

		if (recordSceneOnly() && selectedVideoPlayer()) {
			selectedVideoPlayer()->spectrogram()->setBorderPen(pen);
			selectedVideoPlayer()->showAxes(show_axes);
		}

		leafs.restore();

		VipProcessingObjectList(d_data->sourceDisplayObjects).restore();
		d_data->sources.restore();
		pool->restore();

		pool->blockSignals(false);

		launchRecord(false);

		// TEST: restart indefinitly the saving
		// QMetaObject::invokeMethod(d_data->recordWidget.record(), "setChecked", Qt::QueuedConnection, Q_ARG(bool, true));
		// QMetaObject::invokeMethod(&d_data->recordWidget, "setRecording", Qt::QueuedConnection, Q_ARG(bool, true));
	}
	else {
		// Sequential device
		if (recordType() == Movie) {
			d_data->timer.setInterval(d_data->samplingTime->value());
			d_data->timer.setSingleShot(false);
			d_data->timer.start();
		}
	}
}

VipRecordToolWidget* vipGetRecordToolWidget(VipMainWindow* window)
{
	static VipRecordToolWidget* instance = new VipRecordToolWidget(window);
	return instance;
}

#include <qapplication.h>

class VipRecordWidgetButton::PrivateData
{
public:
	VipFileName filename;
	VipPenButton backgroundColorButton;
	QCheckBox transparentBackground;
	QSpinBox frequency;
	QPointer<VipBaseDragWidget> widget;
	QPointer<VipGenericRecorder> recorder;
	QTimer timer;
	QPixmap pixmap;
	VipRenderState state;
	bool ready;
};

VipRecordWidgetButton::VipRecordWidgetButton(VipBaseDragWidget* widget, QWidget* parent)
  : QToolButton(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->ready = false;
	d_data->widget = widget;

	QWidget* w = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&d_data->backgroundColorButton);
	hlay->addWidget(&d_data->transparentBackground);
	hlay->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* hlay2 = new QHBoxLayout();
	hlay2->addWidget(new QLabel("Record frequency"));
	hlay2->addWidget(&d_data->frequency);
	hlay2->setContentsMargins(0, 0, 0, 0);

	vlay->addLayout(hlay);
	vlay->addLayout(hlay2);
	vlay->addWidget(&d_data->filename);
	w->setLayout(vlay);

	VipDragMenu* menu = new VipDragMenu();
	menu->setWidget(w);
	menu->setMinimumWidth(200);

	this->setMenu(menu);
	this->setPopupMode(QToolButton::MenuButtonPopup);
	this->setCheckable(true);
	this->setIcon(vipIcon("record_icon.png"));

	d_data->filename.setMode(VipFileName::Save);
	QVariantList data_list;
	data_list.append(QVariant::fromValue(vipToArray(QImage(10, 10, QImage::Format_ARGB32))));
	QString filters;
	// find the devices that can save these data
	QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(QString(), data_list);
	QStringList res;
	for (int i = 0; i < devices.size(); ++i) {
		VipIODevice* dev = qobject_cast<VipIODevice*>(devices[i].create()); // vipCreateVariant(devices[i]).value<VipIODevice*>();
		QString fs = dev->fileFilters();
		if (!fs.isEmpty())
			res.append(fs);
		delete dev;
	}

	// make unique, sort and return
	res = vipToSet(res).values();
	filters = res.join(";;");
	d_data->filename.setFilters(filters);

	d_data->backgroundColorButton.setMode(VipPenButton::Color);
	d_data->backgroundColorButton.setPen(QPen(QColor(230, 231, 232)));
	d_data->backgroundColorButton.setText("Select images background color");
	d_data->transparentBackground.setText("Background color ");
	d_data->transparentBackground.setChecked(true);
	d_data->recorder = new VipGenericRecorder(this);
	d_data->recorder->setRecorderAvailableDataOnOpen(false);
	d_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(1);
	d_data->recorder->topLevelInputAt(0)->toMultiInput()->setListType(VipDataList::FIFO, VipDataList::MemorySize, INT_MAX, 500000000);
	d_data->recorder->setScheduleStrategies(VipProcessingObject::Asynchronous);

	d_data->frequency.setRange(0, 100);
	d_data->frequency.setValue(15);
	d_data->frequency.setToolTip("Record frequence in Frame Per Second");

	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(newImage()));
	connect(&d_data->filename, SIGNAL(changed(const QString&)), this, SLOT(filenameChanged()));
	connect(this, SIGNAL(clicked(bool)), this, SLOT(setStarted(bool)));

	this->setToolTip("<b>Start/stop recording this widget content in a file</b><br>Use the right arrow to set the output filename and other options.<br>"
			 "Use <b>SPACE</b> key to stop recording.");

	qApp->installEventFilter(this);
}

VipRecordWidgetButton::~VipRecordWidgetButton()
{
	if (qApp)
		qApp->removeEventFilter(this);
	disconnect(&d_data->timer, SIGNAL(timeout()), this, SLOT(newImage()));
	setStarted(false);
}

bool VipRecordWidgetButton::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::KeyPress) {
		QKeyEvent* key = static_cast<QKeyEvent*>(evt);
		if (key->key() == Qt::Key_Space) {
			setStarted(false);
		}
	}
	return false;
}

QString VipRecordWidgetButton::filename() const
{
	return d_data->filename.filename();
}
QColor VipRecordWidgetButton::backgroundColor() const
{
	QColor c = d_data->transparentBackground.isChecked() ? d_data->backgroundColorButton.pen().color() : QColor(255, 255, 255, 1);
	if (c.alpha() == 0)
		c.setAlpha(1);
	return c;
}
int VipRecordWidgetButton::frequency() const
{
	return d_data->frequency.value();
}

void VipRecordWidgetButton::filenameChanged()
{
	d_data->ready = d_data->recorder->setPath(filename());
}

void VipRecordWidgetButton::setStarted(bool enable)
{
	if (enable) {
		d_data->timer.setInterval(1000 / frequency());
		d_data->timer.setSingleShot(false);
		if (!d_data->widget || !d_data->ready || !d_data->recorder->open(VipIODevice::WriteOnly)) {
			VIP_LOG_ERROR("unable to start recording: wrong output format");
			this->blockSignals(true);
			this->setChecked(false);
			this->blockSignals(false);
			return;
		}

		d_data->state = VipRenderState();
		VipRenderObject::startRender(d_data->widget, d_data->state);
		d_data->timer.start();
		Q_EMIT started();
	}
	else {
		if (d_data->recorder->isOpen() || d_data->timer.isActive()) {
			d_data->timer.stop();
			VipRenderObject::endRender(d_data->widget, d_data->state);
			d_data->recorder->wait();
			d_data->recorder->close();
		}
		this->blockSignals(true);
		this->setChecked(false);
		this->blockSignals(false);
		Q_EMIT stopped();
	}
}

void VipRecordWidgetButton::newImage()
{
	if (d_data->widget && d_data->recorder->isOpen()) {

		QSize size = d_data->widget->size();
		if (size != d_data->pixmap.size())
			d_data->pixmap = QPixmap(size);

		QColor c = backgroundColor();
		d_data->pixmap.fill(c);

		{
			QPainter p(&d_data->pixmap);
			p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
			VipRenderObject::renderObject(d_data->widget, &p, QPoint(0, 0), true, false);
		}

		VipAnyData any(QVariant::fromValue(vipToArray(d_data->pixmap.toImage())), QDateTime::currentMSecsSinceEpoch() * 1000000);
		d_data->recorder->inputAt(0)->setData(any);
	}
}
