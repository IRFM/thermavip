#include "VipRecordToolWidget.h"
#include "VipStandardWidgets.h"
#include "VipProcessingObjectEditor.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipDragWidget.h"
#include "VipDisplayObject.h"
#include "VipGenericDevice.h"
#include "VipProgress.h"
#include "VipPlotItem.h"
#include "VipLogging.h"
#include "VipTextOutput.h"

#include <QCoreApplication>
#include <QRadioButton>
#include <QDateTime>
#include <QListWidget>
#include <QListWidgetItem>
#include <QToolButton>
#include <QComboBox>
#include <QBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPointer>
#include <QToolTip>
#include <QKeyEvent>
#include <QCheckBox>

#include <iostream>


static void getNames(VipBaseDragWidget * player, VipPlotItem * item, QString & text, QString & tool_tip)
{
	QString player_name = player ? player->windowTitle() : QString();
	QString item_name = item->title().text();
	QString class_name = vipSplitClassname( item->metaObject()->className());
	text = item_name.size() ? item_name : player_name;
	tool_tip = "<div style = \"white-space:nowrap;\"><b>Item: </b>" + item_name + "<br><b>Player: </b>" + player_name + "<br><b>Type: </b> " + class_name +"</div>";
}


static VipOutput * ressourceSourceObject(VipDisplayObject * disp)
{
	if (!disp)
		return NULL;

	VipProcessingObjectList src = disp->allSources();
	QList<VipIODevice*> devices = src.findAll<VipIODevice*>();

	//check if all source devices are Resource ones
	bool all_resource = true;
	for (int i = 0; i < devices.size(); ++i)
	{
		if (devices[i]->deviceType() != VipIODevice::Resource)
		{
			all_resource = false;
			break;
		}
	}

	if (all_resource)
	{
		if (VipOutput * out = disp->inputAt(0)->connection()->source())
			return out;
	}

	return NULL;
}




class VipRecordToolBar::PrivateData
{
public:
	QToolButton* selectItems;
	QMenu* selectItemsMenu;
	VipFileName * filename;
	QToolButton * record;

	QAction * record_movie;
	QAction * record_signals;
};

VipRecordToolBar::VipRecordToolBar(VipRecordToolWidget * tool)
	:VipToolWidgetToolBar(tool)
{
	setObjectName("Record tool bar");
	setWindowTitle("Record tool bar");

	m_data = new PrivateData();

	m_data->selectItems = new QToolButton(this);
	m_data->selectItemsMenu = new QMenu(m_data->selectItems);
	m_data->filename = new VipFileName();
	m_data->record = new QToolButton();

	m_data->selectItems->setAutoRaise(true);
	m_data->selectItems->setText("Record...");
	m_data->selectItems->setMenu(m_data->selectItemsMenu);
	m_data->selectItems->setPopupMode(QToolButton::InstantPopup);
	m_data->selectItems->setToolTip("Select signals to record or video to create");
	m_data->selectItems->setToolTip("<b>Shortcut:</b> select signals to record or video to create.<br><br>To see all recording features, click on the left icon.");

	m_data->selectItemsMenu->setToolTipsVisible(true);
	m_data->selectItemsMenu->setStyleSheet(
		"QMenu::item{ margin-left : 10px; padding-left: 20px; padding-top :2px ; padding-right: 20px; padding-bottom: 2px; }"
		"QMenu::item:enabled {margin-left: 20px;}"
		"QMenu::item:disabled {margin-left: 10px; padding-top: 5px; padding-bottom: 5px; font: italic;}"
		"QMenu::item:disabled:checked {background: #007ACC; color: white;}"
	);

	m_data->filename->setMaximumWidth(200);
	m_data->filename->setFilename(tool->recordWidget()->filename());
	m_data->filename->setFilters(tool->recordWidget()->filenameWidget()->filters());
	m_data->filename->setMode(tool->recordWidget()->filenameWidget()->mode());
	m_data->filename->setDefaultPath(tool->recordWidget()->filenameWidget()->defaultPath());
	m_data->filename->setDefaultOpenDir(tool->recordWidget()->filenameWidget()->defaultOpenDir());
	m_data->filename->edit()->setPlaceholderText("Output filename");

	m_data->record->setAutoRaise(true);
	m_data->record->setCheckable(true);
	m_data->record->setIcon(vipIcon("record.png"));
	m_data->record->setToolTip("Launch recording");

	addWidget(m_data->selectItems);
	addWidget(m_data->filename);
	addWidget(m_data->record);
	setIconSize(QSize(18, 18));

	connect(m_data->selectItemsMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));
	connect(m_data->selectItemsMenu, SIGNAL(triggered(QAction*)), this, SLOT(itemSelected(QAction*)));
	connect(m_data->filename, SIGNAL(changed(const QString&)), this, SLOT(updateRecorder()));
	connect(m_data->record, SIGNAL(clicked(bool)), this, SLOT(updateRecorder()));
	connect(tool->recordWidget()->filenameWidget(), SIGNAL(changed(const QString&)), this, SLOT(updateWidget()));
	connect(tool->recordWidget()->record(), SIGNAL(clicked(bool)), this, SLOT(updateWidget()));
	connect(tool->recordWidget(),SIGNAL(recordingChanged(bool)), this, SLOT(updateWidget()));
}

VipRecordToolBar::~VipRecordToolBar()
{
	delete m_data;
}

VipRecordToolWidget * VipRecordToolBar::toolWidget() const
{
	return static_cast<VipRecordToolWidget*>(VipToolWidgetToolBar::toolWidget());
}

VipFileName * VipRecordToolBar::filename() const
{
	return m_data->filename;
}

QToolButton * VipRecordToolBar::record() const
{
	return m_data->record;
}

void VipRecordToolBar::updateRecorder()
{
	if (toolWidget() && toolWidget()->recordWidget()->filename() != this->filename()->filename())
		toolWidget()->setFilename(this->filename()->filename());
	if(toolWidget() && toolWidget()->recordWidget()->record()->isChecked() != m_data->record->isChecked())
		toolWidget()->recordWidget()->enableRecording(m_data->record->isChecked());
}

void VipRecordToolBar::execMenu()
{
	m_data->selectItemsMenu->exec();
}

void VipRecordToolBar::updateWidget()
{
	m_data->record->blockSignals(true);
	m_data->filename->blockSignals(true);

	m_data->filename->setFilename(toolWidget()->recordWidget()->filename());
	m_data->filename->setFilters(toolWidget()->recordWidget()->filenameWidget()->filters());
	m_data->filename->setMode(toolWidget()->recordWidget()->filenameWidget()->mode());
	m_data->filename->setDefaultPath(toolWidget()->recordWidget()->filenameWidget()->defaultPath());
	m_data->filename->setDefaultOpenDir(toolWidget()->recordWidget()->filenameWidget()->defaultOpenDir());
	m_data->record->setChecked(toolWidget()->recordWidget()->record()->isChecked());

	m_data->record->blockSignals(false);
	m_data->filename->blockSignals(false);
}

void VipRecordToolBar::setDisplayPlayerArea(VipDisplayPlayerArea * )
{

}

void VipRecordToolBar::updateMenu()
{
	QString current_player = toolWidget()->currentPlayer();
	QList<VipPlotItem*> current_items = toolWidget()->selectedItems();

	m_data->selectItemsMenu->blockSignals(true);
	m_data->selectItemsMenu->clear();

	if (VipDisplayPlayerArea * area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
	{
		m_data->record_movie = m_data->selectItemsMenu->addAction("Create a video from player...");
		m_data->record_movie->setCheckable(true);
		m_data->record_movie->setChecked(toolWidget()->recordType() == VipRecordToolWidget::Movie);
		m_data->record_movie->setEnabled(false);

		//add all VipBaseDragWidget titles
		QList<VipBaseDragWidget*> players;


		QList<VipBaseDragWidget*> pls = area->findChildren<VipBaseDragWidget*>();
		for (int i = 0; i < pls.size(); ++i)
		{
			//only add the VipBaseDragWidget with a visible header
			//if (pls[i]->Header() && pls[i]->Header()->isVisible())
			{
				QAction * act = m_data->selectItemsMenu->addAction(pls[i]->windowTitle());
				act->setProperty("is_player", true);
				act->setCheckable(true);
				bool check = toolWidget()->recordType() != VipRecordToolWidget::Movie ? false : pls[i]->windowTitle() == current_player;
				act->setChecked(check);
				vip_debug("checked: %i\n", (int)check);
			}
		}


		m_data->selectItemsMenu->addSeparator();
		m_data->record_signals = m_data->selectItemsMenu->addAction("...Or record one or more signals:");
		m_data->record_signals->setCheckable(true);
		m_data->record_signals->setChecked(toolWidget()->recordType() != VipRecordToolWidget::Movie);
		m_data->record_signals->setEnabled(false);

		//add all possible plot items
		QList<QAction*> items = VipPlotItemSelector::createActions( VipPlotItemSelector::possibleItems(area), m_data->selectItemsMenu);
		for (int i = 0; i < items.size(); ++i)
		{
			m_data->selectItemsMenu->addAction(items[i]);
			items[i]->setCheckable(true);
			items[i]->setChecked(current_items.indexOf(items[i]->property("VipPlotItem").value<VipPlotItem*>()) >= 0);
			items[i]->setProperty("is_player", false);
		}

	}

	m_data->selectItemsMenu->blockSignals(false);
}

void VipRecordToolBar::itemSelected(QAction* act)
{
	bool is_player = act->property("is_player").toBool();



	if (is_player)
	{
		//this is a VipBaseDragWidget to save a movie
		if (act->isChecked())
		{
			toolWidget()->setRecordType(VipRecordToolWidget::Movie);
			toolWidget()->setCurrentPlayer(act->text());

			//uncheck all other players
			QList<QAction*> acts = m_data->selectItemsMenu->actions();
			m_data->selectItemsMenu->blockSignals(true);
			for (int i = 0; i < acts.size(); ++i)
				if (acts[i]->property("is_player").toBool())
					if (acts[i] != act)
						acts[i]->setChecked(false);
			m_data->selectItemsMenu->blockSignals(false);
		}
	}
	else
	{
		toolWidget()->setRecordType(VipRecordToolWidget::SignalArchive);

		//add or remove the selected plot item
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
	VipRecordToolWidget * parent;
	QMenu * menu;
};

VipPlotItemSelector::VipPlotItemSelector(VipRecordToolWidget * parent)
	:QToolButton(parent)
{
	m_data = new PrivateData();
	m_data->parent = parent;

	this->setText("Select a signal to record");
	this->setToolTip("<b> Select one or more signals (video, curve,...) you want to record</b><br>"
		"If you select several signals, only the ARCH format (*.arch files) will be able to record them in a single archive.");
	m_data->menu = new QMenu(this);
	m_data->menu->setToolTipsVisible(true);
	this->setMenu(m_data->menu);
	this->setPopupMode(QToolButton::InstantPopup);

	connect(m_data->menu, SIGNAL(triggered(QAction*)), this, SLOT(processingSelected(QAction*)));
	connect(m_data->menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
}

VipPlotItemSelector::~VipPlotItemSelector()
{
	delete m_data;
}

VipProcessingPool * VipPlotItemSelector::processingPool() const
{
	return m_data->parent->processingPool();
}

QList<VipPlotItem*> VipPlotItemSelector::possibleItems(VipDisplayPlayerArea * area, const QList<VipPlotItem*> & current_items)
{
	QList<VipPlotItem*> res;
	if (area)
	{
		QList<VipAbstractPlayer*> players = area->findChildren<VipAbstractPlayer*>();

		for (int i = 0; i < players.size(); ++i)
		{
			QList<VipDisplayObject*> disp = players[i]->displayObjects();
			for (int d = 0; d < disp.size(); ++d)
			{
				if (VipDisplayPlotItem * di = qobject_cast<VipDisplayPlotItem*>(disp[d]))
					if (VipPlotItem * item = di->item())
						if(!item->title().isEmpty())
							if (current_items.indexOf(item) < 0)
								if(res.indexOf(item) < 0)
									res.append(item);
			}
		}
	}
	return res;
}

QList<VipPlotItem*> VipPlotItemSelector::possibleItems() const
{
	return possibleItems(m_data->parent->area(), m_data->parent->selectedItems());
}

QList<QAction*> VipPlotItemSelector::createActions(const QList<VipPlotItem*> & items, QObject * parent)
{
	QList<QAction*> res;
	for (int i = 0; i < items.size(); ++i)
	{
		QString text, tool_tip;
		VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(items[i]);
		VipBaseDragWidget * w = pl ? VipBaseDragWidget::fromChild(pl) : NULL;
		getNames(w, items[i], text, tool_tip);
		QAction * act = new QAction(text,parent);
		act->setToolTip(tool_tip);
		act->setProperty("VipPlotItem", QVariant::fromValue(items[i]));
		res.append(act);
	}
	return res;
}

void VipPlotItemSelector::aboutToShow()
{
	m_data->menu->blockSignals(true);

	m_data->menu->clear();
	QList<VipPlotItem*> _leafs = this->possibleItems();
	QList<QAction*> actions = createActions(this->possibleItems(), m_data->menu);

	for (int i = 0; i < actions.size(); ++i)
	{
		m_data->menu->addAction(actions[i]);
		//QString text, tool_tip;
		// VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(_leafs[i]);
		// VipBaseDragWidget * w = pl ? VipBaseDragWidget::fromChild(pl) : NULL;
		// getNames(w, _leafs[i], text, tool_tip);
		// QAction * act = m_data->menu->addAction(text);
		// act->setToolTip(tool_tip);
		// act->setProperty("VipPlotItem", QVariant::fromValue(_leafs[i]));
	}

	m_data->menu->blockSignals(false);
}

void VipPlotItemSelector::processingSelected(QAction * act)
{
	if (VipPlotItem * item = act->property("VipPlotItem").value<VipPlotItem*>())
	{
		Q_EMIT itemSelected(item);
	}
}



class PlotListWidgetItem : public QListWidgetItem
{
public:
	PlotListWidgetItem(VipBaseDragWidget * player, VipPlotItem * item)
	:QListWidgetItem(NULL,QListWidgetItem::UserType ), player(player), item(item)
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
	VipRecordToolWidget * record;
	QTimer timer;
public:
	int find(VipPlotItem * it)
	{
		for(int i=0; i < count(); ++i)
			if(static_cast<PlotListWidgetItem*>(item(i))->item == it)
				return i;

		return -1;
	}

	RecordListWidget(VipRecordToolWidget * record)
		: QListWidget(), record(record)
	{
		timer.setSingleShot(false);
		timer.setInterval(500);
		connect(&timer, &QTimer::timeout, this, std::bind(&RecordListWidget::testItems, this));
		timer.start();
	}
	~RecordListWidget() {
		timer.stop();
		timer.disconnect();
	}
protected:
	void testItems()
	{
		bool has_delete = false;
		for (int i = 0; i < count(); ++i) {
			PlotListWidgetItem * it = static_cast<PlotListWidgetItem*>(item(i));
			if (!it->item || !it->player) {
				delete takeItem(i);
				--i;
				has_delete = true;
			}
		}
		if(has_delete)
			record->updateFileFiltersAndDevice();
	}
	virtual void keyPressEvent(QKeyEvent * evt)
	{
		if(evt->key() == Qt::Key_Delete)
		{
			qDeleteAll(this->selectedItems());
			record->updateFileFiltersAndDevice();
		}
		else if(evt->key() == Qt::Key_A && (evt->modifiers() & Qt::ControlModifier))
		{
			for(int i=0; i < count(); ++i)
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
	:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->text.setText("Take one frame out of");
	m_data->frames.setRange(1, INT_MAX);
	m_data->frames.setValue(1);

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addWidget(&m_data->text);
	lay->addWidget(&m_data->frames);
	setLayout(lay);

}
SkipFrame::~SkipFrame()
{
	delete m_data;
}

int SkipFrame::value()
{
	return m_data->frames.value();
}

void SkipFrame::setValue(int v)
{
	if (v != value())
		m_data->frames.setValue(v);
}
void SkipFrame::reset()
{
	setValue(1);
}





class VipRecordToolWidget::PrivateData
{
public:
	PrivateData():isRecording(false),recordType(SignalArchive) , sourceWidget(NULL), flag(VipIODevice::Resource){}

	QRadioButton *saveMovie;
	QRadioButton *saveSignals;
	RecordListWidget * itemList;
	VipPlotItemSelector * itemSelector;
	QList<QPointer<VipBaseDragWidget> > playerlist;
	VipComboBox *players;
	VipDoubleEdit *samplingTime;
	SkipFrame* skipFrames;
	QWidget *samplingWidget;
	QLabel *playerPreview;

	VipRecordWidget *recordWidget;
	VipPenButton *backgroundColorButton;
	QCheckBox *transparentBackground;
	QCheckBox * recordSceneOnly;
	QWidget *streamingOptions;

	//buffer options
	QSpinBox *maxBufferSize;
	QSpinBox *maxBufferMemSize;
	QWidget *bufferOptions;

	QTimer timer;
	bool isRecording;
	RecordType recordType;

	//saving objects
	VipBaseDragWidget * sourceWidget;
	VipGenericRecorder *recorder;
	QList<VipIODevice*> sourceDevices;
	VipProcessingObjectList independantResourceProcessings;
	QList<QPointer<VipDisplayObject> > sourceDisplayObjects;
	VipProcessingObjectList sources;
	VipIODevice::DeviceType flag;
	VipRenderState state;
	QPixmap pixmap;

	QPointer<VipDisplayPlayerArea> area;
	QPointer<VipProcessingPool> pool;

	QPointer<VipRecordToolBar> toolBar;
};


VipRecordToolWidget::VipRecordToolWidget(VipMainWindow * window)
:VipToolWidget(window)
{
	//setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	this->setAllowedAreas(Qt::NoDockWidgetArea);

	m_data = new PrivateData();
	m_data->recorder = new VipGenericRecorder(this);
	//m_data->recorder->setMultiSave(true);
	m_data->itemSelector = new VipPlotItemSelector(this);
	m_data->itemList = new RecordListWidget(this);

	m_data->samplingTime = new VipDoubleEdit();
	m_data->saveMovie = new QRadioButton();
	m_data->transparentBackground = new QCheckBox();
	m_data->recordSceneOnly = new QCheckBox();
	m_data->backgroundColorButton = new VipPenButton();
	m_data->players = new VipComboBox();
	m_data->samplingWidget = new QWidget();
	m_data->playerPreview = new QLabel();
	m_data->saveSignals = new QRadioButton();
	m_data->skipFrames = new SkipFrame();
	m_data->recordWidget = new VipRecordWidget();
	m_data->recordWidget->filenameWidget()->edit()->setPlaceholderText("Output filename");

	m_data->maxBufferSize = new QSpinBox();
	m_data->maxBufferSize->setRange(1, 100000);
	m_data->maxBufferSize->setSuffix(" inputs");
	m_data->maxBufferSize->setToolTip("Maximum pending input data (data waiting to be saved)");
	m_data->maxBufferMemSize = new QSpinBox();
	m_data->maxBufferMemSize->setRange(1, 10000);
	m_data->maxBufferMemSize->setToolTip("Maximum pending input data size in MB (data waiting to be saved)");
	m_data->maxBufferMemSize->setSuffix(" MB");
	m_data->bufferOptions = new QWidget();
	QGridLayout * glay = new QGridLayout();
	glay->setSpacing(1);
	glay->setContentsMargins(0, 0, 0, 0);
	glay->addWidget(new QLabel("Max input count"), 0, 0);
	glay->addWidget(m_data->maxBufferSize, 0, 1);
	glay->addWidget(new QLabel("Max input size (MB)"), 1, 0);
	glay->addWidget(m_data->maxBufferMemSize, 1, 1);
	m_data->bufferOptions->setLayout(glay);
	m_data->bufferOptions->setVisible(false);//for streaming only

	QHBoxLayout * sampling_lay = new QHBoxLayout();
	sampling_lay->addWidget(m_data->samplingTime);
	sampling_lay->addWidget(new QLabel(" ms"));
	sampling_lay->setContentsMargins(0,0,0,0);
	m_data->samplingWidget->setLayout(sampling_lay);
	m_data->samplingWidget->setVisible(false);//for streaming only

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(m_data->saveMovie);

	QHBoxLayout * back = new QHBoxLayout();
	back->setContentsMargins(0, 0, 0, 0);
	back->addWidget(m_data->transparentBackground);
	back->addWidget(m_data->backgroundColorButton);
	back->addStretch(1);

	lay->addLayout(back);
	lay->addWidget(m_data->recordSceneOnly);
	lay->addWidget(m_data->players);
	lay->addWidget(m_data->samplingWidget);
	lay->addWidget(m_data->skipFrames);
	lay->addWidget(m_data->playerPreview);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(m_data->saveSignals);
	lay->addWidget(m_data->itemSelector);
	lay->addWidget(m_data->itemList);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(m_data->bufferOptions);
	lay->addWidget(m_data->recordWidget);

	lay->addStretch(1);

	QWidget * w = new QWidget(this);
	w->setLayout(lay);
	this->setWidget(w);

	m_data->saveMovie->setText("Create a movie");
	m_data->saveMovie->setToolTip("Record a movie of type MPG, AVI, MP4,...\n"
			"Select the player you wish to record from the list");
	m_data->saveSignals->setText("Record one or more raw signals");
	m_data->saveSignals->setToolTip("Record an archive of type ARCH, TXT,...\n"
			"Select the different plot items you wish\nto record from the available players.");
	m_data->players->setToolTip("Select a player to save");
	m_data->itemList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_data->itemList->setToolTip("List of signals to record");
	//m_data->playerPreview.setScaledContents(true);
	m_data->playerPreview->setToolTip("Player preview");
	m_data->samplingTime->setValue(20);
	m_data->samplingTime->setToolTip("Movie sampling time (save an image every sampling time ms)");
	m_data->backgroundColorButton->setMode(VipPenButton::Color);
	m_data->backgroundColorButton->setPen(QPen(QColor(255, 255, 255)));
	m_data->backgroundColorButton->setText("Select images background color");
	m_data->transparentBackground->setText("Background color ");
	m_data->transparentBackground->setChecked(true);

	m_data->recordSceneOnly->setText("Save player spectrogram only");
	m_data->recordSceneOnly->setToolTip("Selecting this option will ony save the spectrogram<br> with its exact geometry, without the color scale");
	m_data->recordSceneOnly->setChecked(false);

	m_data->recordWidget->setGenericRecorder(m_data->recorder);
	m_data->recorder->setRecorderAvailableDataOnOpen(false);

	m_data->saveSignals->setChecked(true);
	m_data->players->hide();
	m_data->playerPreview->hide();
	m_data->transparentBackground->hide();
	m_data->recordSceneOnly->hide();
	m_data->backgroundColorButton->hide();
	m_data->samplingWidget->hide();
	m_data->skipFrames->hide();
	m_data->itemList->hide();

	m_data->maxBufferSize->setValue(INT_MAX);
	m_data->maxBufferMemSize->setValue(500);
	m_data->recorder->topLevelInputAt(0)->toMultiInput()->setListType(VipDataList::FIFO, VipDataList::MemorySize, INT_MAX, 500000000);

	connect(VipPlotItemManager::instance(),SIGNAL(itemClicked(VipPlotItem* , int)),this,SLOT(itemClicked(VipPlotItem*, int)));
	connect(m_data->itemSelector, SIGNAL(itemSelected(VipPlotItem*)), this, SLOT(addPlotItem(VipPlotItem*)));
	connect(m_data->saveMovie,SIGNAL(clicked(bool)),this,SLOT(recordTypeChanged()));
	connect(m_data->saveSignals,SIGNAL(clicked(bool)),this,SLOT(recordTypeChanged()));
	connect(m_data->players,SIGNAL(openPopup()),this,SLOT(displayAvailablePlayers()));
	connect(m_data->players,SIGNAL(currentTextChanged(const QString &)),this,SLOT(playerSelected()));
	connect(m_data->maxBufferMemSize, SIGNAL(valueChanged(int)), this, SLOT(updateBuffer()));
	connect(m_data->maxBufferSize, SIGNAL(valueChanged(int)), this, SLOT(updateBuffer()));
	connect(m_data->recorder,SIGNAL(openModeChanged(bool)),this,SLOT(launchRecord(bool)), Qt::QueuedConnection);
	connect(&m_data->timer,SIGNAL(timeout()),this,SLOT(timeout()),Qt::QueuedConnection);
	//connect(&m_data->itemList,SIGNAL(currentItemChanged(QListWidgetItem *,QListWidgetItem *)),this,SLOT(updateFileFiltersAndDevice()));

	setObjectName("Record tools");
	setWindowTitle("Recording tools");
	resetSize();
}

VipRecordToolWidget::~VipRecordToolWidget()
{
	delete m_data;
}

VipVideoPlayer * VipRecordToolWidget::selectedVideoPlayer() const
{
	if(qobject_cast<VipDragWidget*>(m_data->sourceWidget))
		return m_data->sourceWidget->findChild<VipVideoPlayer*>();
	return NULL;
}

bool VipRecordToolWidget::updateFileFiltersAndDevice(bool build_connections, bool close_device)
{
	//when selecting a new VipPlotItem or a new VipAbstractPlayer, update the ReordWidget file filters.
	//Also update the GenericDevice by setting its input.
	//Finally, save the sources VipIODevice and VipDisplayObject.
	//This way, when launching the recording, everything will be ready for the saving.

	//m_data->recordWidget.blockSignals(true);
	//m_data->recordWidget.stopRecording();
	//m_data->recordWidget.blockSignals(false);
	if(close_device)
		m_data->recorder->close();

	//disconnect all inputs
	for (int i = 0; i < m_data->recorder->inputCount(); ++i)
		m_data->recorder->inputAt(i)->clearConnection();

	m_data->recordWidget->record()->setEnabled(false);
	if(toolBar())
		toolBar()->record()->setEnabled(false);

	m_data->sourceDisplayObjects.clear();
	m_data->sourceDevices.clear();
	m_data->independantResourceProcessings.clear();
	m_data->sources.clear();
	m_data->sourceWidget = NULL;

	//first, retrieve the sources VipDisplayObject

	if (recordType() == Movie)
	{
		if (m_data->players->currentIndex() < 0) return false;

		m_data->sourceWidget = m_data->playerlist[m_data->players->currentIndex()];
		if (m_data->sourceWidget)
		{
			QList<VipAbstractPlayer * > players = m_data->sourceWidget->findChildren<VipAbstractPlayer*>();
			for (int i = 0; i < players.size(); ++i)
			{
				QList<VipDisplayObject*> displays = players[i]->displayObjects();
				for (int j = 0; j < displays.size(); ++j)
					m_data->sourceDisplayObjects << displays[j];
			}
		}
		else
			return false;
	}
	else
	{
		for (int i = 0; i < m_data->itemList->count(); ++i)
		{
			VipPlotItem * item = static_cast<PlotListWidgetItem*>(m_data->itemList->item(i))->item;
			if (item)
			{
				if (VipDisplayObject* disp = item->property("VipDisplayObject").value<VipDisplayObject*>())
					if (disp->inputAt(0)->connection()->source())
						m_data->sourceDisplayObjects << disp;
			}
		}
	}

	if (!m_data->sourceDisplayObjects.size())
		return false;

	//get the sources.
	//just in case, we also need the processings AFTER the sources, because a
	//few processings work in-place (they just modify their input data, for instance in the Tokida plugin).
	//This also means that we need the sources of these additional processings (arf...)
	//To sumurize, we need ALL processings involved somehow in the selected processings pipelines.
	m_data->sourceDevices.clear();
	for (int i = 0; i < m_data->sourceDisplayObjects.size(); ++i)
	{
		VipProcessingObjectList pipeline = m_data->sourceDisplayObjects[i]->fullPipeline();
		QList<VipIODevice*> devices;
		//bool have_no_resource = false;
		for (int p = 0; p < pipeline.size(); ++p)
		{
			if (VipIODevice * dev = qobject_cast<VipIODevice*>(pipeline[p]))
			{
				//only consider read only devices
				if ((dev->openMode() & VipIODevice::ReadOnly))
				{
					devices.append(dev);
					m_data->sources.append(dev);
					//if (dev->deviceType() != VipIODevice::Resource)
					//	have_no_resource = true;
				}
			}
			else if (!qobject_cast<VipDisplayObject*>(pipeline[p]))
			{
				m_data->sources.append(pipeline[p]);
			}
		}

		m_data->sourceDevices += devices;
		//if (!have_no_resource)
		// m_data->independantResourceProcessings += devices;

	}


	if (!m_data->sourceDevices.size())
		return false;

	//reset inputs
	m_data->recorder->topLevelInputAt(0)->toMultiInput()->clear();
	if (recordType() == SignalArchive)
		m_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(m_data->sourceDisplayObjects.size());
	else
		m_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(1);

	//now, update the VipRecordWidget file filters
	QVariantList lst;
	if (m_data->recordType == Movie)
	{
		//set a QImage input data to the VipGenericRecorder and update the file filters
		m_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(1);
		lst.append(QVariant::fromValue(vipToArray(QImage())));
		m_data->recorder->setProbeInputs(lst);
	}
	else
	{
		//use the selected VipPlotItem
		for (int i = 0; i < m_data->itemList->count(); ++i)
		{
			if (VipPlotItem * item = static_cast<PlotListWidgetItem*>(m_data->itemList->item(i))->item)
				if (VipDisplayObject * display = item->property("VipDisplayObject").value<VipDisplayObject*>())
					if (VipOutput * output = display->inputAt(0)->connection()->source())
						lst.append(output->data().data());
		}

		m_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(lst.size());
		m_data->recorder->setProbeInputs(lst);
	}



	//update the file filters of the VipRecordWidget. This will also clear the previously added input data
	QString filters = m_data->recordWidget->updateFileFilters(lst);
	if (toolBar())
		toolBar()->filename()->setFilters(filters);


	//find the source VipIODevice type.
	//we cannot mix Sequential and Temporal devices

	m_data->flag = VipIODevice::Resource;
	for (int i = 0; i < m_data->sourceDevices.size(); ++i)
	{
		VipIODevice::DeviceType tmp = m_data->sourceDevices[i]->deviceType();
		if (tmp == VipIODevice::Temporal)
		{
			if (m_data->flag == VipIODevice::Sequential)
			{
				VIP_LOG_ERROR("cannot mix sequential and temporal devices");
				return false;
			}
			m_data->flag = VipIODevice::Temporal;
		}
		else if (tmp == VipIODevice::Sequential)
		{
			if (m_data->flag == VipIODevice::Temporal)
			{
				VIP_LOG_ERROR("cannot mix sequential and temporal devices");
				return false;
			}
			m_data->flag = VipIODevice::Sequential;
		}
	}

	//Finally, setup the input connections
	//connect all signals to record to the recorder inputs

	if (build_connections)
	{
		if (recordType() == SignalArchive)
		{
			for (int i = 0; i < m_data->sourceDisplayObjects.size(); ++i)
				if (VipDisplayObject * disp = m_data->sourceDisplayObjects[i])
					if (VipOutput * out = disp->inputAt(0)->connection()->source())
						out->setConnection(m_data->recorder->inputAt(i));
		}


		if (m_data->flag == VipIODevice::Sequential)
			m_data->recorder->setScheduleStrategy(VipProcessingObject::Asynchronous, true);
		else
			m_data->recorder->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	}

	//we reach the end: enable the recording
	m_data->recordWidget->record()->setEnabled(true);
	if(toolBar())
		toolBar()->record()->setEnabled(true);

	m_data->recordSceneOnly->setVisible(m_data->recordType == Movie && selectedVideoPlayer());

	//show streaming options
	m_data->samplingWidget->setVisible(m_data->recordType == Movie && m_data->flag == VipIODevice::Sequential);
	m_data->skipFrames->setVisible(m_data->recordType == Movie && m_data->flag != VipIODevice::Sequential);
	m_data->bufferOptions->setVisible(m_data->recordType == SignalArchive && m_data->flag == VipIODevice::Sequential);

	return true;
}

void VipRecordToolWidget::setRecordType(RecordType type)
{
	if(type != m_data->recordType)
	{
		m_data->itemList->setVisible(m_data->itemList->count() && type == SignalArchive);
		m_data->itemSelector->setVisible(type == SignalArchive);
		m_data->players->setVisible(type == Movie);
		m_data->playerPreview->setVisible(type == Movie);
		m_data->backgroundColorButton->setVisible(type == Movie);
		m_data->transparentBackground->setVisible(type == Movie);
		m_data->recordSceneOnly->setVisible(type == Movie && selectedVideoPlayer());
		m_data->saveMovie->blockSignals(true);
		m_data->saveSignals->blockSignals(true);
		m_data->saveMovie->setChecked(type == Movie);
		m_data->saveSignals->setChecked(type != Movie);
		m_data->saveMovie->blockSignals(false);
		m_data->saveSignals->blockSignals(false);

		m_data->recordType = type;

		//m_data->samplingWidget->setVisible(type == Movie);
		//m_data->bufferOptions->setVisible(type == SignalArchive);

		resetSize();
	}

	updateFileFiltersAndDevice();
}

void VipRecordToolWidget::updateBuffer()
{
	if(!m_data->recorder->isOpen())
		m_data->recorder->topLevelInputAt(0)->toMultiInput()->setListType(
			VipDataList::FIFO, VipDataList::MemorySize | VipDataList::Number, m_data->maxBufferSize->value(), m_data->maxBufferMemSize->value()*1000000);
}

VipRecordToolWidget::RecordType VipRecordToolWidget::recordType() const
{
	return m_data->recordType;
}

void VipRecordToolWidget::recordTypeChanged()
{
	if(m_data->saveSignals->isChecked())
	{
		setRecordType(SignalArchive);
	}
	else
	{
		setRecordType(Movie);
	}

	this->resetSize();
}

void VipRecordToolWidget::setDisplayPlayerArea(VipDisplayPlayerArea * area)
{
	static QMap< VipDisplayPlayerArea*, QList<QPointer<VipPlotItem> > > area_items;

	if (area == m_data->area)
		return;

	//save the content of the item list for the previous area
	if (m_data->area)
	{
		area_items[m_data->area].clear();
		for (int i = 0; i < m_data->itemList->count(); ++i)
		{
			PlotListWidgetItem * it = static_cast<PlotListWidgetItem*>(m_data->itemList->item(i));
			if (it->item)
				area_items[m_data->area].append(it->item.data());
		}
	}
	//set the items that were saved for the new area
	QList<QPointer<VipPlotItem> > items = area_items[area];
	while (m_data->itemList->count())
		delete m_data->itemList->takeItem(0);
	for (int i = 0; i < items.size(); ++i)
	{
		VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(items[i]);
		VipBaseDragWidget * w = pl ? VipBaseDragWidget::fromChild(pl) : NULL;
		m_data->itemList->addItem(new PlotListWidgetItem(w, items[i]));
	}

	m_data->area = area;
	if (area)
		m_data->pool = area->processingPool();
	else
		m_data->pool = NULL;

}

VipRecordToolBar * VipRecordToolWidget::toolBar()
{
	//TEST
	return NULL;
	if (!m_data->toolBar)
	{
		m_data->toolBar = new VipRecordToolBar(this);
	}
	return m_data->toolBar;
}

VipDisplayPlayerArea * VipRecordToolWidget::area() const
{
	return m_data->area;
}

VipProcessingPool * VipRecordToolWidget::processingPool() const
{
	return m_data->pool;
}

QList<VipPlotItem*> VipRecordToolWidget::selectedItems() const
{
	QList<VipPlotItem*> items;
	for (int i = 0; i < m_data->itemList->count(); ++i)
	{
		PlotListWidgetItem * it = static_cast<PlotListWidgetItem*>(m_data->itemList->item(i));
		if (it->item)
			items << it->item.data();
	}
	return items;
}

VipPlotItemSelector * VipRecordToolWidget::leafSelector() const
{
	return m_data->itemSelector;
}

void VipRecordToolWidget::setBackgroundColor(const QColor & c)
{
	m_data->backgroundColorButton->setPen(QPen(c));
}

QColor VipRecordToolWidget::backgroundColor() const
{
	QColor c = m_data->transparentBackground->isChecked() ? m_data->backgroundColorButton->pen().color() : QColor(255, 255, 255, 1);
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
	return m_data->players->currentText();
}

void VipRecordToolWidget::setCurrentPlayer(const QString & player)
{
	if (m_data->players->count() == 0)
		this->displayAvailablePlayers();
	m_data->players->setCurrentText(player);
}

void VipRecordToolWidget::setFilename(const QString & filename)
{
	m_data->recordWidget->setFilename(filename);
	if (toolBar())
		toolBar()->filename()->setFilename(filename);
}

QString VipRecordToolWidget::filename() const
{
	return m_data->recordWidget->filename();
}

void VipRecordToolWidget::setRecordSceneOnly(bool enable)
{
	m_data->recordSceneOnly->setChecked(enable);
}
bool VipRecordToolWidget::recordSceneOnly() const
{
	return m_data->recordSceneOnly->isChecked();
}

VipRecordWidget * VipRecordToolWidget::recordWidget() const
{
	return m_data->recordWidget;
}

void VipRecordToolWidget::displayAvailablePlayers(bool update_player_pixmap)
{
	//update the combo box which displays the list of available players
	QString current_text = m_data->players->currentText();

	m_data->players->blockSignals(true);
	m_data->players->clear();
	m_data->playerlist.clear();
	VipDisplayPlayerArea * area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	QList<VipBaseDragWidget*> players;

	if(area)
		players = area->findChildren<VipBaseDragWidget*>();

	for(int i=0; i < players.size(); ++i)
	{
		//only add the VipBaseDragWidget with a visible header
		//if(players[i]->Header() && players[i]->Header()->isVisible())
		{
			m_data->players->addItem(players[i]->windowTitle());
			m_data->playerlist.append(players[i]);
		}
	}

	m_data->players->setCurrentText(current_text);
	m_data->players->blockSignals(false);

	if (update_player_pixmap)
		playerSelected();
	else
		updateFileFiltersAndDevice();
}

void VipRecordToolWidget::playerSelected()
{
	//a player is selected through the combo box: display its content in a QLabel
	if(m_data->players->currentIndex() < 0 || m_data->players->currentIndex() >= m_data->playerlist.size())
		return;

	VipBaseDragWidget * player = m_data->playerlist[m_data->players->currentIndex()];
	if(player)
	{
		QPixmap pixmap(player->size());
		if(pixmap.width() == 0 || pixmap.height() == 0)
			return;

		pixmap.fill(Qt::transparent);
		//{
		// QPainter p(&pixmap);
		// p.setCompositionMode(QPainter::CompositionMode_Clear);
		// p.fillRect(0, 0, pixmap.width(), pixmap.height(), QColor(0, 0, 0, 0));
		// }


		VipRenderObject::startRender(player,m_data->state);
		vipProcessEvents();

		{
		QPainter p(&pixmap);
		//player->draw(&p);
		VipRenderObject::renderObject(player,&p, QPoint(0, 0), true, false);
		}

		VipRenderObject::endRender(player,m_data->state);

		int max_dim = qMax(pixmap.width(),pixmap.height());
		double factor = 300.0/max_dim;
		pixmap= pixmap.scaled(pixmap.width()*factor,pixmap.height()*factor, Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

		m_data->playerPreview->setPixmap(pixmap);
		this->resetSize();
	}

	updateFileFiltersAndDevice();
}

bool VipRecordToolWidget::addPlotItem(VipPlotItem * item)
{
	if (m_data->itemList->find(item) >= 0)
		return false;

	VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(item);
	VipBaseDragWidget * w = pl ? VipBaseDragWidget::fromChild(pl) : NULL;
	//only add if the VipPlotItem * is related to a VipDisplayObject
	if (item->property("VipDisplayObject").value<VipDisplayObject*>())
	{
		m_data->itemList->addItem(new PlotListWidgetItem(w, item));

		m_data->itemList->setMinimumHeight(30 * m_data->itemList->count());
		m_data->itemList->setVisible(m_data->itemList->count());

		//update the file filters
		updateFileFiltersAndDevice();

		this->resetSize();
		return true;
	}
	return false;
}

bool VipRecordToolWidget::removePlotItem(VipPlotItem * item)
{
	int row = m_data->itemList->find(item);
	if (row >= 0)
	{
		delete m_data->itemList->takeItem(row);
		//update the file filters
		updateFileFiltersAndDevice();
		return true;
	}
	return false;
}

void VipRecordToolWidget::itemClicked(VipPlotItem* plot_item, int button)
{
	if(m_data->recordType == SignalArchive && isVisible())
	{
		//add the plot item from the list if: this is a left click, the item is selected and not already added to the list
		if(plot_item && button == VipPlotItem::LeftButton && plot_item->isSelected() && m_data->itemList->find(plot_item) < 0)
		{
			addPlotItem(plot_item);
		}

	}

}


void VipRecordToolWidget::timeout()
{
	if(recordType() == Movie && m_data->sourceWidget && m_data->recorder->isOpen())
	{
		//qint64 time = QDateTime::currentMSecsSinceEpoch();
		//check that at least one source VipIODevice have streaming enabled
		bool has_streaming = false;
		for(int i=0; i < m_data->sourceDevices.size(); ++i)
			if (m_data->sourceDevices[i]->isStreamingEnabled())
			{
				has_streaming = true;
				break;
			}

		if (!has_streaming)
			return;

		//vipProcessEvents();
		if(!m_data->sourceWidget )
			return;

		QSize size = m_data->sourceWidget->size();
		if(size != m_data->pixmap.size())
			m_data->pixmap = QPixmap(size);


		QColor c = backgroundColor();
		m_data->pixmap.fill(c);

		{
		QPainter p(&m_data->pixmap);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
		VipRenderObject::renderObject(m_data->sourceWidget, &p, QPoint(0, 0), true, false);
		}

		VipAnyData any(QVariant::fromValue(vipToArray(m_data->pixmap.toImage())),QDateTime::currentMSecsSinceEpoch()*1000000);
		m_data->recorder->inputAt(0)->setData(any);
		//m_data->output->update();


	}
}
// void VipRecordToolWidget::setPlayerToDevice()
// {
// //set the output device player
// if (qobject_cast<VipDragWidget*>(m_data->sourceWidget))
// if (VipAbstractPlayer * player = m_data->sourceWidget->findChild<VipAbstractPlayer*>())
// {
//	if(m_data->recorder && m_data->recorder->recorder())
//	m_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
// }
// }


void VipRecordToolWidget::launchRecord(bool launch)
{
	if(!launch)
	{
		//stop the timer
		m_data->timer.stop();
		vipProcessEvents();

		//m_data->recordWidget.stopRecording();
		m_data->recorder->close();
		updateFileFiltersAndDevice();
		m_data->recorder->setEnabled(false);

		//end saving: cleanup
		if(this->recordType() == Movie)
			VipRenderObject::endRender(m_data->sourceWidget,m_data->state);


		//TOCHECK:
		//We comment this as it disable the possibility to reuse the saver parameters
		//m_data->recorder->setRecorder(NULL);


		//m_data->sourceWidget = NULL;
		return;
	}

	if(m_data->recordWidget->path().isEmpty())
		return launchRecord(false);

	//actually build the connections
	updateFileFiltersAndDevice(true,false);

	//check that the selected display objects are still valid
	for (int i = 0; i < m_data->sourceDisplayObjects.size(); ++i)
	{
		if (!m_data->sourceDisplayObjects[i])
		{
			VIP_LOG_ERROR("Unable to record: one or more selected items have been closed");
			return launchRecord(false);
		}
	}

	if(!m_data->sourceDevices.size())
		return launchRecord(false);


	VipProcessingPool * pool = m_data->sourceDevices.first()->parentObjectPool();
	if(!pool)
		return launchRecord(false);

	if(recordType() == Movie)
	{
		if (!m_data->sourceWidget)
		{
			VIP_LOG_ERROR("No valid selected player for video saving");
			return launchRecord(false);
		}
		//for a movie, prepare the source widget for rendering
		VipRenderObject::startRender(m_data->sourceWidget,m_data->state);
		vipProcessEvents();
	}

	//set the output device player
	if (qobject_cast<VipDragWidget*>(m_data->sourceWidget))
	{
		if (VipAbstractPlayer * player = m_data->sourceWidget->findChild<VipAbstractPlayer*>())
		{
			m_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
		}
	}
	else
	{
		if (m_data->itemList->count() == 1)
		{
			PlotListWidgetItem * item = static_cast<PlotListWidgetItem*>(m_data->itemList->item(0));
			if (VipAbstractPlayer * player = m_data->sourceWidget->findChild<VipAbstractPlayer*>())
			{
				m_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
			}
			else if (VipAbstractPlayer * player = VipAbstractPlayer::findAbstractPlayer(item->item))
			{
				m_data->recorder->recorder()->setProperty("player", QVariant::fromValue(player));
			}
		}
	}



	//for temporal devices, save the archive right now
	if(m_data->flag == VipIODevice::Temporal || m_data->flag == VipIODevice::Resource)
	{

		//now, save the current VipProcessingPool state, because we are going to modify it heavily
		pool->save();
		m_data->sources.save();
		VipProcessingObjectList(m_data->sourceDisplayObjects).save();


		//disable all processing except the sources, remove the Automatic flag from the sources
		if(recordType() == SignalArchive)
		{
			pool->blockSignals(true);
			pool->disableExcept(m_data->sources);
			foreach (VipProcessingObject * obj, m_data->sources) {obj->setScheduleStrategy(VipProcessingObject::Asynchronous,false);}
		}
		//if saving a movie, we must enable the VipDisplayObject and set everything to Automatic
		else
		{
			pool->disableExcept(m_data->sources);
			foreach (VipProcessingObject * obj, m_data->sources) {obj->setScheduleStrategy(VipProcessingObject::Asynchronous,false);}

			for(int i=0; i < m_data->sourceDisplayObjects.size(); ++i)
			{
				m_data->sourceDisplayObjects[i]->setEnabled(true);
				m_data->sourceDisplayObjects[i]->setScheduleStrategy(VipProcessingObject::Asynchronous,true);
			}
		}

		VipProgress progress;
		progress.setModal(true);
		progress.setCancelable(true);
		progress.setText("<b>Saving</b> " + QFileInfo(m_data->recorder->path()).fileName() + "...");

		qint64 time = pool->firstTime();
		qint64 end_time = pool->lastTime();
		//qint64 current_time = pool->time();
		progress.setRange(time/1000000.0,end_time/1000000.0);

		//movie sampling time (default: 20ms)
		qint64 movie_sampling_time = m_data->samplingTime->value()* 1000000;
		qint64 previous_time = VipInvalidTime;

		m_data->recorder->setEnabled(true);

		VipProcessingObjectList leafs = pool->leafs(false);
		leafs.save();
		leafs.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		if (recordType() == SignalArchive)
		{
			//disable the display objects among the leafs
			leafs.findAllProcessings<VipDisplayObject*>().setEnabled(false);
		}

		//We must distinguish 2 specific cases:
		// - if the pool is a Resource, we just call pool->reload() once
		// - if the pool is Temporal, but with the same (or invalid) start and end time. In this case, just like Resource, we call pool->reload() once.
		bool save_resource = (m_data->flag == VipIODevice::Resource) || (time == VipInvalidTime || time == end_time);

		if(!save_resource && recordType() == SignalArchive)
		{
			//When recording a temporal signal archive, we might have Resource input devices that is NOT, IN ANY WAY, linked to a temporal device.
			//In this case, the data from these devices won't be recorder.
			//So first, we reload these devices to record their data.

			for (int i = 0; i < m_data->sourceDisplayObjects.size(); ++i)
			{
				if (VipOutput * out = ressourceSourceObject(m_data->sourceDisplayObjects[i]))
				{
					m_data->recorder->inputAt(i)->setData(out->data());
					m_data->recorder->update();
				}

			}

			//for (int i = 0; i < m_data->independantResourceProcessings.size(); ++i)
			// {
			// m_data->independantResourceProcessings[i]->reload();
			// leafs.update(m_data->recorder);
			// //update the recorder last
			// m_data->recorder->update();
			// }
		}

		QPen pen;
		bool show_axes;
		if (recordSceneOnly() && selectedVideoPlayer())
		{
			pen = selectedVideoPlayer()->spectrogram()->borderPen();
			selectedVideoPlayer()->spectrogram()->setBorderPen(Qt::NoPen);
			show_axes = selectedVideoPlayer()->isShowAxes();
			selectedVideoPlayer()->showAxes(false);

		}

		int skip = m_data->skipFrames->value();
		int skip_count = 0;

		while( (time != VipInvalidTime && time <= end_time) || save_resource)
		{


			progress.setValue(time/1000000.0);

			if (save_resource)
				pool->reload();
			else
				pool->read(time,true);

			if(recordType() == SignalArchive)
			{
				leafs.update(m_data->recorder);
				//update the recorder last
				m_data->recorder->update();
			}
			else if(previous_time == VipInvalidTime || (time-previous_time) >= movie_sampling_time)
			{
				leafs.update(m_data->recorder);
				//wait for displays
				for(int i=0; i < m_data->sourceDisplayObjects.size(); ++i)
					m_data->sourceDisplayObjects[i]->update();
				vipProcessEvents();

				if (++skip_count == skip)
				{
					skip_count = 0;
					if (recordSceneOnly() && selectedVideoPlayer())
					{
						VipAbstractPlotWidget2D* plot = m_data->sourceWidget->findChild<VipAbstractPlotWidget2D*>();
						VipPlotSpectrogram* spec = selectedVideoPlayer()->spectrogram();
						spec->setBorderPen(Qt::NoPen);
						QRectF scene_rect = spec->mapToScene(spec->sceneMap()->clipPath(spec)).boundingRect();
						QRect view_rect = plot->mapFromScene(scene_rect).boundingRect();

						QSize size = view_rect.size();
						if (size != m_data->pixmap.size())
							m_data->pixmap = QPixmap(size);

						QColor c = backgroundColor();
						m_data->pixmap.fill(c);

						{
							QPainter p(&m_data->pixmap);
							p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
							VipRenderObject::renderObject(plot, &p, -view_rect.topLeft(), true, false);
						}

						VipAnyData any(QVariant::fromValue(vipToArray(m_data->pixmap.toImage())), time);
						m_data->recorder->inputAt(0)->setData(any);
						m_data->recorder->update();
					}
					else
					{
						QSize size = m_data->sourceWidget->size();
						if (size != m_data->pixmap.size())
							m_data->pixmap = QPixmap(size);

						QColor c = backgroundColor();
						m_data->pixmap.fill(c);

						{
							QPainter p(&m_data->pixmap);
							p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
							VipRenderObject::renderObject(m_data->sourceWidget, &p, QPoint(), true, false);
						}

						VipAnyData any(QVariant::fromValue(vipToArray(m_data->pixmap.toImage())), time);
						m_data->recorder->inputAt(0)->setData(any);
						m_data->recorder->update();
					}
				}
			}

			qint64 current = time;
			time = pool->nextTime(time);
			if(time == current || progress.canceled())
				break;

			save_resource = false;
		}

		if (recordSceneOnly() && selectedVideoPlayer())
		{
			selectedVideoPlayer()->spectrogram()->setBorderPen(pen);
			selectedVideoPlayer()->showAxes(show_axes);
		}


		leafs.restore();

		VipProcessingObjectList(m_data->sourceDisplayObjects).restore();
		m_data->sources.restore();
		pool->restore();

		pool->blockSignals(false);

		launchRecord(false);

		//TEST: restart indefinitly the saving
		//QMetaObject::invokeMethod(m_data->recordWidget.record(), "setChecked", Qt::QueuedConnection, Q_ARG(bool, true));
		//QMetaObject::invokeMethod(&m_data->recordWidget, "setRecording", Qt::QueuedConnection, Q_ARG(bool, true));
	}
	else
	{
		//Sequential device
		if(recordType() == Movie)
		{
			m_data->timer.setInterval(m_data->samplingTime->value());
			m_data->timer.setSingleShot(false);
			m_data->timer.start();
		}
	}
}

VipRecordToolWidget * vipGetRecordToolWidget(VipMainWindow * window)
{
	static VipRecordToolWidget * instance = new VipRecordToolWidget(window);
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

VipRecordWidgetButton::VipRecordWidgetButton(VipBaseDragWidget * widget, QWidget * parent)
	:QToolButton(parent)
{
	m_data = new PrivateData();
	m_data->ready = false;
	m_data->widget = widget;

	QWidget * w = new QWidget();
	QVBoxLayout * vlay = new QVBoxLayout();

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addWidget(&m_data->backgroundColorButton);
	hlay->addWidget(&m_data->transparentBackground);
	hlay->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout * hlay2 = new QHBoxLayout();
	hlay2->addWidget(new QLabel("Record frequency"));
	hlay2->addWidget(&m_data->frequency);
	hlay2->setContentsMargins(0, 0, 0, 0);

	vlay->addLayout(hlay);
	vlay->addLayout(hlay2);
	vlay->addWidget(&m_data->filename);
	w->setLayout(vlay);

	VipDragMenu * menu = new VipDragMenu();
	menu->setWidget(w);
	menu->setMinimumWidth(200);

	this->setMenu(menu);
	this->setPopupMode(QToolButton::MenuButtonPopup);
	this->setCheckable(true);
	this->setIcon(vipIcon("record_icon.png"));

	m_data->filename.setMode(VipFileName::Save);
	QVariantList data; data.append(QVariant::fromValue(vipToArray(QImage(10, 10, QImage::Format_ARGB32))));
	QString filters;
	// find the devices that can save these data
	QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(QString(), data);
	QStringList res;
	for (int i = 0; i < devices.size(); ++i)
	{
		VipIODevice * dev = qobject_cast<VipIODevice*>(devices[i].create());//vipCreateVariant(devices[i]).value<VipIODevice*>();
		QString filters = dev->fileFilters();
		if (!filters.isEmpty())
			res.append(filters);
		delete dev;
	}

	//make unique, sort and return
	res = res.toSet().toList();
	filters = res.join(";;");
	m_data->filename.setFilters(filters);

	m_data->backgroundColorButton.setMode(VipPenButton::Color);
	m_data->backgroundColorButton.setPen(QPen(QColor(230, 231, 232)));
	m_data->backgroundColorButton.setText("Select images background color");
	m_data->transparentBackground.setText("Background color ");
	m_data->transparentBackground.setChecked(true);
	m_data->recorder = new VipGenericRecorder(this);
	m_data->recorder->setRecorderAvailableDataOnOpen(false);
	m_data->recorder->topLevelInputAt(0)->toMultiInput()->resize(1);
	m_data->recorder->topLevelInputAt(0)->toMultiInput()->setListType(VipDataList::FIFO, VipDataList::MemorySize, INT_MAX, 500000000);
	m_data->recorder->setScheduleStrategies(VipProcessingObject::Asynchronous);

	m_data->frequency.setRange(0, 100);
	m_data->frequency.setValue(15);
	m_data->frequency.setToolTip("Record frequence in Frame Per Second");

	connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(newImage()));
	connect(&m_data->filename, SIGNAL(changed(const QString &)), this, SLOT(filenameChanged()));
	connect(this, SIGNAL(clicked(bool)), this, SLOT(setStarted(bool)));

	this->setToolTip("<b>Start/stop recording this widget content in a file</b><br>Use the right arrow to set the output filename and other options.<br>"
		"Use <b>SPACE</b> key to stop recording.");

	qApp->installEventFilter(this);
}

VipRecordWidgetButton::~VipRecordWidgetButton()
{
	if(qApp)
		qApp->removeEventFilter(this);
	disconnect(&m_data->timer, SIGNAL(timeout()), this, SLOT(newImage()));
	setStarted(false);
	delete m_data;
}

bool VipRecordWidgetButton::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::KeyPress)
	{
		QKeyEvent * key = static_cast<QKeyEvent*>(evt);
		if (key->key() == Qt::Key_Space)
		{
			setStarted(false);
		}
	}
	return false;
}

QString VipRecordWidgetButton::filename() const
{
	return m_data->filename.filename();
}
QColor VipRecordWidgetButton::backgroundColor() const
{
	QColor c = m_data->transparentBackground.isChecked() ? m_data->backgroundColorButton.pen().color() : QColor(255, 255, 255, 1);
	if (c.alpha() == 0)
		c.setAlpha(1);
	return c;
}
int VipRecordWidgetButton::frequency() const
{
	return m_data->frequency.value();
}

void VipRecordWidgetButton::filenameChanged()
{
	m_data->ready = m_data->recorder->setPath(filename());
}

void VipRecordWidgetButton::setStarted(bool enable)
{
	if (enable)
	{
		m_data->timer.setInterval(1000 / frequency());
		m_data->timer.setSingleShot(false);
		if (!m_data->widget || !m_data->ready || !m_data->recorder->open(VipIODevice::WriteOnly)) {
			VIP_LOG_ERROR("unable to start recording: wrong output format");
			this->blockSignals(true);
			this->setChecked(false);
			this->blockSignals(false);
			return;
		}

		m_data->state = VipRenderState();
		VipRenderObject::startRender(m_data->widget, m_data->state);
		m_data->timer.start();
	}
	else
	{
		if (m_data->recorder->isOpen() || m_data->timer.isActive())
		{
			m_data->timer.stop();
			VipRenderObject::endRender(m_data->widget, m_data->state);
			m_data->recorder->wait();
			m_data->recorder->close();
		}
		this->blockSignals(true);
		this->setChecked(false);
		this->blockSignals(false);
	}
}

void VipRecordWidgetButton::newImage()
{
	if (m_data->widget && m_data->recorder->isOpen())
	{

		QSize size = m_data->widget->size();
		if (size != m_data->pixmap.size())
			m_data->pixmap = QPixmap(size);


		QColor c = backgroundColor();
		m_data->pixmap.fill(c);

		{
			QPainter p(&m_data->pixmap);
			p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
			VipRenderObject::renderObject(m_data->widget, &p, QPoint(0, 0), true, false);
		}

		VipAnyData any(QVariant::fromValue(vipToArray(m_data->pixmap.toImage())), QDateTime::currentMSecsSinceEpoch() * 1000000);
		m_data->recorder->inputAt(0)->setData(any);
	}
}
