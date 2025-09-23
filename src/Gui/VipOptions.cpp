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

#include <QBoxLayout>
#include <QDesktopServices>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTreeWidget>
#include <qgroupbox.h>
#include <qicon.h>
#include <qscrollarea.h>
#include <qplaintextedit.h>
#include <qtextedit.h>

#include "VipCore.h"
#include "VipDisplayArea.h"
#include "VipEnvironment.h"
#include "VipLogging.h"
#include "VipOptions.h"
#include "VipStandardEditors.h"
#include "VipStandardWidgets.h"
#include "VipSearchLineEdit.h"
#include "VipProcessingObjectEditor.h"
#include "VipPlayer.h"

#ifdef VIP_WITH_VTK
#include "VipVTKPlayer.h"
#endif



QGroupBox* VipPageOption::createOptionGroup(const QString& label)
{
	QGroupBox* res = new QGroupBox(label);
	res->setFlat(true);
	QFont f = res->font();
	f.setBold(true);
	res->setFont(f);
	return res;
}

VipPageItems::VipPageItems(QWidget* parent )
  : QTreeWidget(parent)
{
	QFont f;
	f.setBold(true);
	setFont(f);
}


class VipOptions::PrivateData
{
public:
	PrivateData()
	  : current(nullptr)
	{
	}
	QMap<QTreeWidgetItem*, QScrollArea*> pages;
	VipPageOption* current;
	QTreeWidget* page_browser;
	QSplitter* splitter;
	QPushButton* apply;
	QVBoxLayout* page_lay;
};

VipOptions::VipOptions(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	this->setObjectName("Preferences");
	this->setWindowTitle("Preferences");
	this->resize(800, 600);

	d_data->page_browser = new VipPageItems();
	d_data->splitter = new QSplitter();
	d_data->apply = new QPushButton();
	d_data->apply->setText("Apply changes");


	d_data->page_browser->header()->hide();
	d_data->page_browser->setAcceptDrops(true);
	d_data->page_browser->setFrameShape(QFrame::NoFrame);
	d_data->page_browser->setIndentation(10);
	d_data->page_browser->setMinimumWidth(200);
	d_data->page_browser->setIconSize(QSize(22, 22));
	d_data->page_browser->setStyleSheet("QTreeWidget{background: transparent;}");


	// create button layout
	QVBoxLayout* button_lay = new QVBoxLayout();
	button_lay->setSpacing(8);
	QLabel* cat = new QLabel("Categories");
	QFont f = cat->font();
	f.setBold(true);
	d_data->apply->setFont(f);
	f.setPointSize(f.pointSize() + 4);
	cat->setFont(f);

	button_lay->addWidget(cat);
	button_lay->addWidget(d_data->page_browser,1);
	button_lay->addWidget(d_data->apply);
	QWidget* categories = new QWidget();
	categories->setLayout(button_lay);



	// create page widget
	QWidget* page = new QWidget();
	d_data->page_lay = new QVBoxLayout();
	page->setLayout(d_data->page_lay);

	// create splitter
	d_data->splitter->addWidget(categories);
	d_data->splitter->addWidget(page);
	d_data->splitter->setStretchFactor(1, 1);

	
	// create final layout
	QVBoxLayout* final_lay = new QVBoxLayout();
	final_lay->addWidget(d_data->splitter);
	
	setLayout(final_lay);

	connect(d_data->apply, SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(d_data->page_browser, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(itemClicked(QTreeWidgetItem*, int)));
}

VipOptions::~VipOptions()
{
}

void VipOptions::apply()
{
	for (QMap<QTreeWidgetItem*, QScrollArea*>::iterator it = d_data->pages.begin(); it != d_data->pages.end(); ++it) {
		VipPageOption* page = qobject_cast<VipPageOption*>(it.value()->widget());
		page->applyPage();
	}
}

bool VipOptions::hasPage(VipPageOption* page) const
{
	for (QMap<QTreeWidgetItem*, QScrollArea*>::const_iterator it = d_data->pages.begin(); it != d_data->pages.end(); ++it)
		if (qobject_cast<VipPageOption*>(it.value()->widget()) == page)
			return true;
	return false;
}

bool VipOptions::addPage(const QString& category, VipPageOption* page, const QIcon& icon)
{
	if (hasPage(page))
		return false;

	QStringList path = category.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (path.size() == 0)
		return false;

	// create the page leaf inside d_data->page_browser
	QTreeWidgetItem* current = d_data->page_browser->invisibleRootItem();

	for (int i = 0; i < path.size(); ++i) {
		QTreeWidgetItem* found = nullptr;
		for (int child = 0; child < current->childCount(); ++child) {
			if (current->child(child)->text(0) == path[i]) {
				found = current->child(child);
				// if the last leaf already exists, return false
				if (i == path.size() - 1)
					return false;
				break;
			}
		}

		if (!found) {
			// item not found, add it
			QTreeWidgetItem* item = new QTreeWidgetItem();
			QString name = path[i];
			item->setText(0, name);
			current->addChild(item);
			current = item;
			// set its icon
			if (i == path.size() - 1 && !icon.isNull())
				item->setIcon(0, icon);

			if (i == path.size() - 1) {

				// Map this entry
				QString shortcut = name;
				if (!name.endsWith("options", Qt::CaseInsensitive))
					shortcut += " options";
				VipShortcutsHelper::registerShorcut(shortcut, [name]() { 
					vipGetOptions()->setCurrentPage(name);
					//vipGetOptions()->exec();
					vipDisplayOptions();
				});
			}
		}
		else
			current = found;
	}

	QScrollArea* area = new QScrollArea();
	area->setMaximumWidth(700);
	area->setMinimumWidth(700);
	area->setWidgetResizable(true);
	area->setWidget(page);
	d_data->pages[current] = area;
	d_data->page_lay->addWidget(area,0,Qt::AlignHCenter);
	area->hide();

	// set the focus to the first page
	if (d_data->pages.size() > 0 && !d_data->current)
		setCurrentPage(qobject_cast<VipPageOption*>(d_data->pages.begin().value()->widget()));


	return true;
}

void VipOptions::setTreeWidth(int w)
{
	d_data->splitter->setSizes(QList<int>() << w << (width() - w));
}

QScrollArea* VipOptions::areaForPage(VipPageOption* page) const
{
	for (QMap<QTreeWidgetItem*, QScrollArea*>::const_iterator it = d_data->pages.begin(); it != d_data->pages.end(); ++it)
		if (qobject_cast<VipPageOption*>(it.value()->widget()) == page)
			return it.value();
	return nullptr;
}

void VipOptions::setCurrentPage(const QString& category)
{
	QTreeWidgetItem* item = nullptr;
	QScrollArea* area = nullptr;

	for (auto it = d_data->pages.begin(); it != d_data->pages.end(); ++it)
	{
		QString name = it.key()->text(0);
		bool found = name == category;
		it.value()->setVisible(found);
		if (found) {
			area = it.value();
			item = it.key();
		}
	}
	if (item) {
		d_data->page_browser->blockSignals(true);
		d_data->page_browser->setCurrentItem(item);
		d_data->page_browser->blockSignals(false);
		d_data->current = static_cast<VipPageOption*>(area->widget());
	}
}

void VipOptions::setCurrentPage(VipPageOption* page)
{
	if (page && page != d_data->current) {
		if (!hasPage(page))
			return;

		QTreeWidgetItem* item = d_data->pages.key(areaForPage(page));

		for (QMap<QTreeWidgetItem*, QScrollArea*>::const_iterator it = d_data->pages.begin(); it != d_data->pages.end(); ++it) {
			it.value()->setVisible(qobject_cast<VipPageOption*>(it.value()->widget()) == page);
		}

		d_data->page_browser->blockSignals(true);
		d_data->page_browser->setCurrentItem(item);
		d_data->page_browser->blockSignals(false);
		d_data->current = page;
	}
}

void VipOptions::itemClicked(QTreeWidgetItem* item, int)
{
	if (!item)
		return;

	// find item
	QMap<QTreeWidgetItem*, QScrollArea*>::iterator it = d_data->pages.find(item);
	if (it == d_data->pages.end()) {
		// if item is not inside d_data->pages, try set the focus to its first child
		if (item->childCount() > 0)
			itemClicked(item->child(0), 0);
		return;
	}

	setCurrentPage(qobject_cast<VipPageOption*>(it.value()->widget()));
}

VipOptions* vipGetOptions()
{
	static VipOptions* dialog = nullptr;
	if (!dialog) {
		dialog = new VipOptions();
		dialog->addPage("General display", new AppearanceSettings(), vipIcon("ROI.png"));
		dialog->addPage("Rendering modes", new RenderingSettings(), vipIcon("RENDERING.png"));
		dialog->addPage("Environment", new EnvironmentSettings(), vipIcon("BROWSER.png"));
		dialog->addPage("Processing and devices", new ProcessingSettings(), vipIcon("PROCESSING.png"));
#ifdef VIP_WITH_VTK
		dialog->addPage("3D Objects", new VipVTKPlayerOptionPage(), vipIcon("CAD.png"));
#endif
		dialog->setTreeWidth(150);
	}
	return dialog;
}


class OptionWidget : public QWidget
{
public:
	OptionWidget()
		: QWidget()
	{
		QHBoxLayout* lay = new QHBoxLayout();
		lay->addWidget(vipGetOptions());
		setLayout(lay);
		vipGetOptions()->show();
	}
	~OptionWidget() 
	{
		vipGetOptions()->hide();
		vipGetOptions()->setParent(nullptr);
	}
};

void vipDisplayOptions()
{
	auto tab = vipGetMainWindow()->displayArea()->displayTabWidget();
	for (int i = 0; i < tab->count(); ++i) {
		if (VipDisplayPlayerArea* a = qobject_cast<VipDisplayPlayerArea*>(tab->widget(i))) {
			if ( a->findChild<VipOptions*>()) {
				tab->setCurrentIndex(i);
				return;
			}
		}
	}

	OptionWidget* opt = new OptionWidget();
	VipWidgetPlayer* pl = new VipWidgetPlayer();
	pl->setWidget(opt);
	VipDisplayPlayerArea* a = new VipDisplayPlayerArea();
	a->setWindowTitle("Preferences");
	
	VipDragWidget* w = new VipDragWidget();
	w->setWidget(pl);

	a->mainDragWidget()->mainResize(1);
	a->mainDragWidget()->subResize(0, 1);
	a->mainDragWidget()->setWidget(0, 0, w);

	vipGetMainWindow()->displayArea()->addWidget(a);
}



#include "VipGui.h"
#include "VipPainter.h"
#include "VipPlayer.h"
#include "VipStandardWidgets.h"
#include <qcheckbox.h>

static QGroupBox* createGroup(const QString& label)
{
	QGroupBox* res = new QGroupBox(label);
	res->setFlat(true);
	QFont f = res->font();
	f.setBold(true);
	res->setFont(f);
	return res;
}

class AppearanceSettings::PrivateData
{
public:
	QGroupBox* general;
	QComboBox* skins;
	QComboBox* colorPalette;
	VipTextWidget *editorFont;

	QGroupBox* players;
	VipColorScaleButton* colorMaps;
	QToolButton* showScale;

	QGroupBox* roi;
	VipTextWidget* title;
	VipPenButton* pen;
	VipPenButton* brush;
	QCheckBox* titleVisible;
	QCheckBox* groupVisible;
	QCheckBox* idVisible;
	QCheckBox* innerPixels;

	QGroupBox* plots;
	QCheckBox displayTimeOffset;
	QRadioButton displayTimeInteger;
	QRadioButton displayAbsoluteDate;
	QRadioButton displayNormal;

	VipDefaultPlotAreaSettings settings;
};
AppearanceSettings::AppearanceSettings(QWidget* parent)
  : VipPageOption(parent)
{
	this->setWindowTitle("General appearance");
	VIP_CREATE_PRIVATE_DATA();

	d_data->general = createGroup("General appearance");
	d_data->skins = new QComboBox();
	d_data->skins->addItems((QStringList() << "Default") + vipAvailableSkins());

	d_data->colorPalette = new QComboBox();
	QImage palette = vipPixmap("palette.png").toImage().convertToFormat(QImage::Format_ARGB32);
	if (!palette.isNull()) {
		d_data->colorPalette->addItem("Default", 0);
		d_data->colorPalette->addItem(applyFactor(palette, 60), "Very dark", 60);
		d_data->colorPalette->addItem(applyFactor(palette, 80), "Dark", 80);
		d_data->colorPalette->addItem(QPixmap::fromImage(palette), "Standard", 100);
		d_data->colorPalette->addItem(applyFactor(palette, 120), "Light", 120);
		d_data->colorPalette->addItem(applyFactor(palette, 140), "Very light", 140);
	}
	d_data->skins->setToolTip("Global Thermavip color theme");
	d_data->colorPalette->setToolTip("Color palette used for curves and histograms");

	d_data->editorFont = new VipTextWidget();
	d_data->editorFont->setEditableContent(VipTextWidget::Font);
	d_data->editorFont->setToolTip("Select font used by consoles, shells, editors...");

	QGridLayout* glay = new QGridLayout();
	glay->addWidget(new QLabel("Color theme"), 0, 0, Qt::AlignLeft);
	glay->addWidget(d_data->skins, 0, 1, Qt::AlignLeft);

	glay->addWidget(new QLabel("Item color palette"), 1, 0, Qt::AlignLeft);
	glay->addWidget(d_data->colorPalette, 1, 1, Qt::AlignLeft);

	glay->addWidget(new QLabel("Consoles/Editors font"), 2, 0, Qt::AlignLeft);
	glay->addWidget(d_data->editorFont, 2, 1, Qt::AlignLeft);
	VipText t;
	t.setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());
	d_data->editorFont->setText(t);

	d_data->general->setLayout(glay);

	d_data->players = createGroup("Video player display");
	d_data->colorMaps = new VipColorScaleButton();
	d_data->showScale = new QToolButton();
	d_data->colorMaps->setToolTip("Color scale used for videos (like infrared ones)");
	d_data->showScale->setToolTip("Show/hide axes for video players");
	d_data->showScale->setIcon(vipIcon("show_axes.png"));
	d_data->showScale->setAutoRaise(true);
	d_data->showScale->setCheckable(true);

	QGridLayout* dlay = new QGridLayout();
	dlay->addWidget(new QLabel());
	dlay->addWidget(new QLabel("Video color scale"), 0, 0, Qt::AlignLeft);
	dlay->addWidget(d_data->colorMaps, 0, 1, Qt::AlignLeft);
	dlay->addWidget(new QLabel("Show/hide axes"), 1, 0, Qt::AlignLeft);
	dlay->addWidget(d_data->showScale, 1, 1, Qt::AlignLeft);
	d_data->players->setLayout(dlay);

	d_data->roi = createGroup("Regions Of Interest");
	QGridLayout* lay = new QGridLayout();
	int row = -1;

	d_data->title = new VipTextWidget();
	lay->addWidget(new QLabel("Shape text style"), ++row, 0);
	lay->addWidget(d_data->title, row, 1);
	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	d_data->pen = new VipPenButton();
	lay->addWidget(new QLabel("Border pen"), ++row, 0);
	lay->addWidget(d_data->pen, row, 1);

	d_data->brush = new VipPenButton();
	lay->addWidget(new QLabel("Background brush"), ++row, 0);
	lay->addWidget(d_data->brush, row, 1);

	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	d_data->titleVisible = new QCheckBox();
	d_data->groupVisible = new QCheckBox();
	d_data->idVisible = new QCheckBox();
	d_data->innerPixels = new QCheckBox();
	lay->addWidget(d_data->titleVisible, ++row, 0, 1, 2);
	lay->addWidget(d_data->groupVisible, ++row, 0, 1, 2);
	lay->addWidget(d_data->idVisible, ++row, 0, 1, 2);
	lay->addWidget(d_data->innerPixels, ++row, 0, 1, 2);

	d_data->roi->setLayout(lay);

	d_data->title->edit()->hide();
	d_data->pen->setMode(VipPenButton::Pen);
	d_data->brush->setMode(VipPenButton::Brush);

	d_data->titleVisible->setText("Display shapes title");
	d_data->groupVisible->setText("Display shapes group");
	d_data->idVisible->setText("Display shapes identifier");
	d_data->innerPixels->setText("Display exact pixels");

	d_data->displayTimeOffset.setText("Display time offset from left date");
	d_data->displayTimeOffset.setToolTip("<div>For plot players only, if the time scale displays a time since Epoch, select the behavior of the scale values."
					     "If checked, displayed labels represent the time offset since the scale start value."
					     "Otherwise, displayed labels represent the time offset since the start of the left most curve.</div>");
	d_data->displayTimeInteger.setText("Display time as integer");
	d_data->displayTimeInteger.setToolTip("<div>If checked, time values in plot players will be displayed as integer, without precision loss</div>");
	d_data->displayAbsoluteDate.setText("Display absolute date time");
	d_data->displayAbsoluteDate.setToolTip("<div>If checked, time labels in plot players will display absolute date time</div>");
	d_data->displayNormal.setText("Display standard time values");
	d_data->displayNormal.setToolTip("<div>If checked, time labels in plot players will be displayed as floating point values</div>");

	QVBoxLayout* tlay = new QVBoxLayout();
	tlay->addWidget(&d_data->displayTimeOffset);
	tlay->addWidget(&d_data->displayTimeInteger);
	tlay->addWidget(&d_data->displayAbsoluteDate);
	tlay->addWidget(&d_data->displayNormal);
	tlay->setContentsMargins(5, 5, 5, 5);

	d_data->plots = createGroup("Default plot settings");
	QVBoxLayout* play = new QVBoxLayout();
	play->setContentsMargins(0, 0, 0, 0);
	play->addLayout(tlay);
	play->addWidget(&d_data->settings);
	d_data->plots->setLayout(play);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->general);
	vlay->addWidget(d_data->players);
	vlay->addWidget(d_data->plots);
	vlay->addWidget(d_data->roi);
	vlay->addStretch(1);
	setLayout(vlay);

	connect(d_data->skins, SIGNAL(currentIndexChanged(int)), this, SLOT(skinChanged()));
	//	connect(&VipDefaultSceneModelDisplayOptions::instance(), SIGNAL(changed()), this, SLOT(updatePage()), Qt::QueuedConnection);
	updatePage();
}

AppearanceSettings::~AppearanceSettings()
{
}

void AppearanceSettings::skinChanged()
{
	if (d_data->skins->currentText().contains("gray", Qt::CaseInsensitive)) {
		d_data->colorPalette->setCurrentText("Standard");
	}
	else if (d_data->skins->currentText().contains("dark", Qt::CaseInsensitive)) {
		d_data->colorPalette->setCurrentText("Light");
	}
}

void AppearanceSettings::updatePage()
{
	// General settings
	int index = d_data->skins->findText(VipCoreSettings::instance()->skin());
	if (index >= 0)
		d_data->skins->setCurrentIndex(index);
	else
		d_data->skins->setCurrentIndex(0);

	int current = VipGuiDisplayParamaters::instance()->itemPaletteFactor();
	if (current == 0)
		d_data->colorPalette->setCurrentIndex(0);
	else if (current == 60)
		d_data->colorPalette->setCurrentIndex(1);
	else if (current == 80)
		d_data->colorPalette->setCurrentIndex(2);
	else if (current == 100)
		d_data->colorPalette->setCurrentIndex(3);
	else if (current == 120)
		d_data->colorPalette->setCurrentIndex(4);
	else if (current == 140)
		d_data->colorPalette->setCurrentIndex(5);

	VipText t;
	t.setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());
	d_data->editorFont->setText(t);

	// Video player settings
	d_data->colorMaps->setColorPaletteName(VipGuiDisplayParamaters::instance()->playerColorScale());
	d_data->showScale->setChecked(VipGuiDisplayParamaters::instance()->videoPlayerShowAxes());

	// ROI settings
	d_data->brush->setBrush(VipGuiDisplayParamaters::instance()->shapeBackgroundBrush());
	d_data->pen->setPen(VipGuiDisplayParamaters::instance()->shapeBorderPen());
	d_data->title->hide();
	// d_data->title->setText(VipText("", VipDefaultSceneModelDisplayOptions::instance().textStyle()));
	d_data->groupVisible->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::Group);
	d_data->idVisible->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::Id);
	d_data->titleVisible->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::Title);
	d_data->innerPixels->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::FillPixels);

	d_data->displayTimeOffset.setChecked(VipGuiDisplayParamaters::instance()->displayTimeOffset());

	if (VipGuiDisplayParamaters::instance()->displayType() == VipValueToTime::Integer)
		d_data->displayTimeInteger.setChecked(true);
	else if (VipGuiDisplayParamaters::instance()->displayType() == VipValueToTime::AbsoluteDateTime)
		d_data->displayAbsoluteDate.setChecked(true);
	else
		d_data->displayNormal.setChecked(true);

	d_data->settings.setDefaultCurve(VipGuiDisplayParamaters::instance()->defaultCurve());
	d_data->settings.setDefaultPlotArea(VipGuiDisplayParamaters::instance()->defaultPlotArea());
}

QPixmap AppearanceSettings::applyFactor(const QImage& img, int factor)
{
	QImage res = img;
	uint* bits = (uint*)res.bits();
	int size = res.width() * res.height();
	for (int i = 0; i < size; ++i) {
		bits[i] = QColor::fromRgba(bits[i]).lighter(factor).rgba();
	}
	return QPixmap::fromImage(res);
}

void AppearanceSettings::applyPage()
{
	// General settings
	if (d_data->skins->currentText() != "Default")
		VipCoreSettings::instance()->setSkin(d_data->skins->currentText());
	else
		VipCoreSettings::instance()->setSkin("gray");

	VipGuiDisplayParamaters::instance()->setItemPaletteFactor(d_data->colorPalette->currentData().toInt());

	// Apply editor font to all QTextEdit and QPlainTextEdit
	QFont f = d_data->editorFont->getText().font();
	VipGuiDisplayParamaters::instance()->setDefaultEditorFont(f);
	QList<QTextEdit*> edits = vipGetMainWindow()->findChildren<QTextEdit*>();
	for (QTextEdit* e : edits)
		e->setFont(f);
	QList<QPlainTextEdit*> plain_edits = vipGetMainWindow()->findChildren<QPlainTextEdit*>();
	for (QPlainTextEdit* e : plain_edits)
		e->setFont(f);

	// Video player settings
	VipGuiDisplayParamaters::instance()->setPlayerColorScale(d_data->colorMaps->colorPaletteName());
	VipGuiDisplayParamaters::instance()->setVideoPlayerShowAxes(d_data->showScale->isChecked());

	// ROI settings
	// disconnect(&VipDefaultSceneModelDisplayOptions::instance(), SIGNAL(changed()), this, SLOT(updatePage()));

	QBrush brush = d_data->brush->pen().brush();
	QPen pen = d_data->pen->pen();
	VipTextStyle style = d_data->title->getText().textStyle();
	VipPlotShape::DrawComponents cmp = VipPlotShape::Background | VipPlotShape::Border;
	if (d_data->groupVisible->isChecked())
		cmp |= VipPlotShape::Group;
	if (d_data->idVisible->isChecked())
		cmp |= VipPlotShape::Id;
	if (d_data->titleVisible->isChecked())
		cmp |= VipPlotShape::Title;
	if (d_data->innerPixels->isChecked())
		cmp |= VipPlotShape::FillPixels;

	if (cmp != VipGuiDisplayParamaters::instance()->shapeDrawComponents() || brush != VipGuiDisplayParamaters::instance()->shapeBackgroundBrush() ||
	    pen != VipGuiDisplayParamaters::instance()->shapeBorderPen() //||
									 // style != VipDefaultSceneModelDisplayOptions::instance().textStyle()
	) {
		VipGuiDisplayParamaters::instance()->setShapeBackgroundBrush(brush);
		VipGuiDisplayParamaters::instance()->setShapeBorderPen(pen);
		// VipDefaultSceneModelDisplayOptions::instance().setTextStyle(style);
		VipGuiDisplayParamaters::instance()->setShapeDrawComponents(cmp);
	}

	// connect(&VipDefaultSceneModelDisplayOptions::instance(), SIGNAL(changed()), this, SLOT(updatePage()), Qt::QueuedConnection);

	// Plot settings

	d_data->settings.applyToCurve(VipGuiDisplayParamaters::instance()->defaultCurve());
	d_data->settings.applyToArea(VipGuiDisplayParamaters::instance()->defaultPlotArea());

	if (d_data->settings.shouldApplyToAllPlayers()) {
		QList<VipPlotPlayer*> pls = vipGetMainWindow()->displayArea()->findChildren<VipPlotPlayer*>();
		for (int i = 0; i < pls.size(); ++i) {
			VipGuiDisplayParamaters::instance()->apply(pls[i]);
			// d_data->settings.applyToArea(qobject_cast<VipPlotArea2D*>(pls[i]->plotWidget2D()->area()));
			//
			// QList<VipPlotCurve*> curves = pls[i]->plotWidget2D()->area()->findItems<VipPlotCurve*>();
			// for (int j = 0; j < curves.size(); ++j)
			// d_data->settings.applyToCurve(curves[j]);

			pls[i]->valueToTimeButton()->setDisplayTimeOffset(d_data->displayTimeOffset.isChecked());
			if (d_data->displayTimeInteger.isChecked())
				pls[i]->valueToTimeButton()->setDisplayType(VipValueToTime::Integer);
			else if (d_data->displayAbsoluteDate.isChecked())
				pls[i]->valueToTimeButton()->setDisplayType(VipValueToTime::AbsoluteDateTime);
			else
				pls[i]->valueToTimeButton()->setDisplayType(VipValueToTime::Double);
		}
	}

	d_data->settings.setShouldApplyToAllPlayers(false);

	VipGuiDisplayParamaters::instance()->setDisplayTimeOffset(d_data->displayTimeOffset.isChecked());
	if (d_data->displayTimeInteger.isChecked())
		VipGuiDisplayParamaters::instance()->setDisplayType(VipValueToTime::Integer);
	else if (d_data->displayAbsoluteDate.isChecked())
		VipGuiDisplayParamaters::instance()->setDisplayType(VipValueToTime::AbsoluteDateTime);
	else
		VipGuiDisplayParamaters::instance()->setDisplayType(VipValueToTime::Double);

	VipGuiDisplayParamaters::instance()->save();
	VipCoreSettings::instance()->save(vipGetDataDirectory() + "core_settings.xml");
}

class ProcessingSettings::PrivateData
{
public:
	QCheckBox* resetRemember;

	QComboBox* procPriority;
	QComboBox* displayPriority;
	QCheckBox* printDebug;

	QCheckBox* maxSizeEnable;
	QSpinBox* maxSize;
	QCheckBox* maxMemoryEnable;
	QSpinBox* maxMemory;

	QDoubleSpinBox* timeWindow;
	QCheckBox* applyToAll;
};

ProcessingSettings::ProcessingSettings(QWidget* parent)
  : VipPageOption(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->resetRemember = new QCheckBox("Reset remembered choices");
	d_data->resetRemember->setToolTip("Reset association of file suffix -> device type.\nReset default device parameters.");

	d_data->procPriority = new QComboBox();
	d_data->displayPriority = new QComboBox();
	d_data->printDebug = new QCheckBox();
	d_data->printDebug->setText("Print debug informations");

	d_data->procPriority->setToolTip("Set the default processing thread priority used within Thermavip");
	d_data->procPriority->addItem("Idle Priority");
	d_data->procPriority->addItem("Lowest Priority");
	d_data->procPriority->addItem("Low Priority");
	d_data->procPriority->addItem("Normal Priority");
	d_data->procPriority->addItem("High Priority");
	d_data->procPriority->addItem("Highest Priority");
	d_data->procPriority->addItem("Time Critical Priority");
	d_data->procPriority->addItem("Inherit Priority");

	d_data->displayPriority->setToolTip("Set the default processing thread priority used within Thermavip for display only");
	d_data->displayPriority->addItem("Idle Priority");
	d_data->displayPriority->addItem("Lowest Priority");
	d_data->displayPriority->addItem("Low Priority");
	d_data->displayPriority->addItem("Normal Priority");
	d_data->displayPriority->addItem("High Priority");
	d_data->displayPriority->addItem("Highest Priority");
	d_data->displayPriority->addItem("Time Critical Priority");
	d_data->displayPriority->addItem("Inherit Priority");

	QGridLayout* glay = new QGridLayout();
	glay->addWidget(new QLabel("Processing thread priority"), 0, 0);
	glay->addWidget(d_data->procPriority, 0, 1);
	glay->addWidget(new QLabel("Display processing thread priority"), 1, 0);
	glay->addWidget(d_data->displayPriority, 1, 1);
	glay->addWidget(d_data->printDebug, 2, 0, 1, 2);

	d_data->maxSizeEnable = new QCheckBox();
	d_data->maxSize = new QSpinBox();
	d_data->maxMemoryEnable = new QCheckBox();
	d_data->maxMemory = new QSpinBox();

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->maxSizeEnable);
	vlay->addWidget(d_data->maxSize);
	vlay->addWidget(d_data->maxMemoryEnable);
	vlay->addWidget(d_data->maxMemory);
	vlay->addWidget(d_data->printDebug);

	d_data->maxSizeEnable->setText("Set maximum input buffer size");
	d_data->maxMemoryEnable->setText("Set maximum input buffer memory");
	d_data->printDebug->setText("Print debug informations");
	d_data->maxSize->setRange(1, INT_MAX);
	d_data->maxSize->setSuffix(" inputs");
	d_data->maxSize->setToolTip("Maximum number of data for each processing input");
	d_data->maxSize->setMaximumWidth(100);
	d_data->maxMemory->setRange(1, INT_MAX);
	d_data->maxMemory->setSuffix("MB");
	d_data->maxMemory->setToolTip("Maximum amount of data for each processing input");
	d_data->maxMemory->setMaximumWidth(100);


	d_data->timeWindow = new QDoubleSpinBox();
	d_data->timeWindow->setRange(-1, 100000);
	d_data->timeWindow->setSingleStep(1.);
	d_data->timeWindow->setValue(VipNumericValueToPointVector::defaultSlidingTimeWindow());
	d_data->applyToAll = new QCheckBox("Apply to all");
	QGridLayout* tlay = new QGridLayout();
	tlay->addWidget(new QLabel("Streaming curves time window"), 0, 0);
	tlay->addWidget(d_data->timeWindow, 0, 1);
	tlay->addWidget(d_data->applyToAll, 2, 0, 1, 2);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(d_data->resetRemember);
	lay->addWidget(createGroup("General processing settings"));
	lay->addLayout(glay);
	lay->addWidget(createGroup("Input buffer settings"));
	lay->addLayout(vlay);
	lay->addWidget(createGroup("Streaming curves"));
	lay->addLayout(tlay);
	lay->addStretch(1);
	setLayout(lay);

	connect(d_data->maxSizeEnable, SIGNAL(clicked(bool)), d_data->maxSize, SLOT(setEnabled(bool)));
	connect(d_data->maxMemoryEnable, SIGNAL(clicked(bool)), d_data->maxMemory, SLOT(setEnabled(bool)));
	connect(&VipProcessingManager::instance(), SIGNAL(changed()), this, SLOT(updatePage()), Qt::QueuedConnection);
	updatePage();
}

ProcessingSettings::~ProcessingSettings()
{
}

void ProcessingSettings::applyPage()
{
	disconnect(&VipProcessingManager::instance(), SIGNAL(changed()), this, SLOT(updatePage()));

	int type = 0;
	if (d_data->maxSizeEnable->isChecked())
		type |= VipDataList::Number;
	if (d_data->maxMemoryEnable->isChecked())
		type |= VipDataList::MemorySize;
	int maxSize = d_data->maxSize->value();
	int maxMemory = d_data->maxMemory->value() * 1000000;
	bool printDebug = d_data->printDebug->isChecked();

	int priority = d_data->procPriority->currentIndex();
	int displayPriority = d_data->displayPriority->currentIndex();

	// bool printDebugEnabled = VipProcessingManager::isLogErrorEnabled(VipProcessingObject::InputBufferFull);

	if (type != VipProcessingManager::listLimitType() || maxSize != VipProcessingManager::maxListSize() || maxMemory != VipProcessingManager::maxListMemory() ||
	    printDebug != VipProcessingManager::isLogErrorEnabled(VipProcessingObject::InputBufferFull)) {
		VipProcessingManager::setListLimitType(type);
		VipProcessingManager::setMaxListSize(maxSize);
		VipProcessingManager::setMaxListMemory(maxMemory);
		VipProcessingManager::setLogErrorEnabled(VipProcessingObject::InputBufferFull, printDebug);
	}
	if (priority >= 0)
		VipProcessingManager::setDefaultPriority((QThread::Priority)priority, &VipProcessingObject::staticMetaObject);
	if (displayPriority >= 0)
		VipProcessingManager::setDefaultPriority((QThread::Priority)displayPriority, &VipDisplayObject::staticMetaObject);

	connect(&VipProcessingManager::instance(), SIGNAL(changed()), this, SLOT(updatePage()), Qt::QueuedConnection);

	if (d_data->resetRemember->isChecked())
		VipRememberDeviceOptions::instance().clearAll();

	// Apply sliding time window
	VipNumericValueToPointVector::setDefaultSlidingTimeWindow(d_data->timeWindow->value());
	if (d_data->applyToAll->isChecked()) {
		auto lst = VipUniqueId::objects<VipPlotPlayer>();
		for (auto* p : lst)
			p->setSlidingTimeWindow(d_data->timeWindow->value());
	}
}

void ProcessingSettings::updatePage()
{
	d_data->maxSizeEnable->setChecked(VipProcessingManager::listLimitType() & VipDataList::Number);
	d_data->maxMemoryEnable->setChecked(VipProcessingManager::listLimitType() & VipDataList::MemorySize);

	d_data->maxSize->setEnabled(VipProcessingManager::listLimitType() & VipDataList::Number);
	d_data->maxMemory->setEnabled(VipProcessingManager::listLimitType() & VipDataList::MemorySize);

	d_data->maxSize->setValue(VipProcessingManager::maxListSize());
	d_data->maxMemory->setValue(VipProcessingManager::maxListMemory() / 1000000);

	d_data->procPriority->setCurrentIndex(VipProcessingManager::defaultPriority(&VipProcessingObject::staticMetaObject));
	d_data->displayPriority->setCurrentIndex(VipProcessingManager::defaultPriority(&VipDisplayObject::staticMetaObject));

	d_data->timeWindow->setValue(VipNumericValueToPointVector::defaultSlidingTimeWindow());
}

class EnvironmentSettings::PrivateData
{
public:
	QGroupBox* data;
	QLabel* dataDir;
	QToolButton* openDataDir;

	QGroupBox* log;
	QLabel* logDir;
	QToolButton* openLogDir;
	QToolButton* openLogFile;
	QCheckBox* overwrite;
	QCheckBox* append_date;

	QPlainTextEdit* env;
};

EnvironmentSettings::EnvironmentSettings(QWidget* parent)
  : VipPageOption(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->data = createGroup("Thermavip data folder");
	d_data->dataDir = new QLabel(vipGetDataDirectory());
	d_data->openDataDir = new QToolButton();
	d_data->openDataDir->setAutoRaise(true);
	d_data->openDataDir->setIcon(vipIcon("open_dir.png"));
	d_data->openDataDir->setToolTip("Open data directory");

	d_data->log = createGroup("Thermavip log folder");
	d_data->logDir = new QLabel(vipGetLogDirectory());
	d_data->openLogDir = new QToolButton();
	d_data->openLogDir->setAutoRaise(true);
	d_data->openLogDir->setIcon(vipIcon("open_dir.png"));
	d_data->openLogDir->setToolTip("Open log directory");
	d_data->openLogFile = new QToolButton();
	d_data->openLogFile->setAutoRaise(true);
	d_data->openLogFile->setIcon(vipIcon("open.png"));
	d_data->openLogFile->setToolTip("Open log file");

	d_data->overwrite = new QCheckBox("Overwrite log file at each startup");
	d_data->append_date = new QCheckBox("Append startup date to log file name");

	d_data->env = new QPlainTextEdit();
	d_data->env->setPlaceholderText("Environment variables on the form \"VARIABLE_NAME1 VARIABLE_VALUE1 new_line VARIABLE_NAME2 VARIABLE_VALUE2 \"");

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(d_data->dataDir);
	hlay->addWidget(d_data->openDataDir);
	hlay->addStretch(1);
	d_data->data->setLayout(hlay);

	hlay = new QHBoxLayout();
	hlay->addWidget(d_data->logDir);
	hlay->addWidget(d_data->openLogDir);
	hlay->addWidget(d_data->openLogFile);
	hlay->addStretch(1);
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(d_data->overwrite);
	vlay->addWidget(d_data->append_date);
	d_data->log->setLayout(vlay);

	vlay = new QVBoxLayout();
	vlay->addWidget(d_data->data);
	vlay->addWidget(d_data->log);

	QGroupBox* env_box = new QGroupBox("Environment variables (thermavip.env)");
	env_box->setFlat(true);
	vlay->addWidget(env_box);
	vlay->addWidget(d_data->env, 1);

	// vlay->addStretch(1);
	setLayout(vlay);

	connect(d_data->openDataDir, SIGNAL(clicked(bool)), this, SLOT(openDataDirectory()));
	connect(d_data->openLogDir, SIGNAL(clicked(bool)), this, SLOT(openLogDirectory()));
	connect(d_data->openLogFile, SIGNAL(clicked(bool)), this, SLOT(openLogFile()));
}
EnvironmentSettings::~EnvironmentSettings()
{
}

void EnvironmentSettings::applyPage()
{
	VipCoreSettings::instance()->setLogFileDate(d_data->append_date->isChecked());
	VipCoreSettings::instance()->setLogFileOverwrite(d_data->overwrite->isChecked());

	VipCoreSettings::instance()->save(vipGetDataDirectory() + "core_settings.xml");

	// Save env. variables
	QString env_file = vipGetDataDirectory() + "thermavip.env";
	QFile fin(env_file);
	if (fin.open(QFile::WriteOnly | QFile::Text)) {
		fin.write(d_data->env->toPlainText().toLatin1());
	}
	else
		VIP_LOG_ERROR("Unable to create file ", env_file);
}

void EnvironmentSettings::updatePage()
{
	d_data->append_date->setChecked(VipCoreSettings::instance()->logFileDate());
	d_data->overwrite->setChecked(VipCoreSettings::instance()->logFileOverwrite());

	d_data->env->clear();
	QString env_file = vipGetDataDirectory() + "thermavip.env";
	if (!QFileInfo(env_file).exists()) {
		env_file = QFileInfo(vipAppCanonicalPath()).canonicalPath();
		env_file.replace("\\", "/");
		if (!env_file.endsWith("/"))
			env_file += "/";
		env_file += "thermavip.env";
	}
	if (QFileInfo(env_file).exists()) {
		QFile fin(env_file);
		if (fin.open(QFile::ReadOnly | QFile::Text)) {
			d_data->env->setPlainText(fin.readAll());
		}
	}
}

void EnvironmentSettings::openDataDirectory()
{
	QDesktopServices::openUrl(vipGetDataDirectory());
}
void EnvironmentSettings::openLogDirectory()
{
	QDesktopServices::openUrl(vipGetLogDirectory());
}
void EnvironmentSettings::openLogFile()
{
	QString fname = VipLogging::instance().logger()->canonicalFilePath();
	QDesktopServices::openUrl(fname);
}

class RenderingSettings::PrivateData
{
public:
	QRadioButton* vdirect;
	QRadioButton* vpureGPU;
	QRadioButton* vGPUThreaded;

	QRadioButton* pdirect;
	QRadioButton* ppureGPU;
	QRadioButton* pGPUThreaded;
};

RenderingSettings::RenderingSettings(QWidget* parent)
  : VipPageOption(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->vdirect = new QRadioButton("Direct rendering");
	d_data->vdirect->setToolTip("Direct CPU based rendering (default), usually the fastest");

	d_data->vpureGPU = new QRadioButton("Opengl rendering");
	d_data->vpureGPU->setToolTip("Direct opengl based rendering.\nDoes not work with multiple threads.");

	d_data->vGPUThreaded = new QRadioButton("Threaded opengl rendering");
	d_data->vGPUThreaded->setToolTip("Threaded opengl based rendering");

	d_data->pdirect = new QRadioButton("Direct rendering");
	d_data->pdirect->setToolTip("Direct CPU based rendering (default), usually the fastest");

	d_data->ppureGPU = new QRadioButton("Opengl rendering");
	d_data->ppureGPU->setToolTip("Direct opengl based rendering.\nDoes not work with multiple threads.");

	d_data->pGPUThreaded = new QRadioButton("Threaded opengl rendering");
	d_data->pGPUThreaded->setToolTip("Threaded opengl based rendering");

	QGroupBox* video = createGroup("Video rendering mode");
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->vdirect);
	vlay->addWidget(d_data->vpureGPU);
	vlay->addWidget(d_data->vGPUThreaded);
	video->setLayout(vlay);

	QGroupBox* plot = createGroup("Plot rendering mode");
	QVBoxLayout* play = new QVBoxLayout();
	play->addWidget(d_data->pdirect);
	play->addWidget(d_data->ppureGPU);
	play->addWidget(d_data->pGPUThreaded);
	plot->setLayout(play);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(video);
	lay->addWidget(plot);
	lay->addStretch(1);
	setLayout(lay);
}
RenderingSettings::~RenderingSettings()
{
}

void RenderingSettings::applyPage()
{
	if (d_data->vdirect->isChecked())
		VipGuiDisplayParamaters::instance()->setVideoRenderingStrategy(VipBaseGraphicsView::Raster);
	else if (d_data->vGPUThreaded->isChecked())
		VipGuiDisplayParamaters::instance()->setVideoRenderingStrategy(VipBaseGraphicsView::OpenGLThread);
	else if (d_data->vpureGPU->isChecked())
		VipGuiDisplayParamaters::instance()->setVideoRenderingStrategy(VipBaseGraphicsView::OpenGL);

	if (d_data->pdirect->isChecked())
		VipGuiDisplayParamaters::instance()->setPlotRenderingStrategy(VipBaseGraphicsView::Raster);
	else if (d_data->pGPUThreaded->isChecked())
		VipGuiDisplayParamaters::instance()->setPlotRenderingStrategy(VipBaseGraphicsView::OpenGLThread);
	else if (d_data->ppureGPU->isChecked())
		VipGuiDisplayParamaters::instance()->setPlotRenderingStrategy(VipBaseGraphicsView::OpenGL);
}
void RenderingSettings::updatePage()
{
	int st = VipGuiDisplayParamaters::instance()->videoRenderingStrategy();
	if (st == VipBaseGraphicsView::Raster)
		d_data->vdirect->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGLThread)
		d_data->vGPUThreaded->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGL)
		d_data->vpureGPU->setChecked(true);

	st = VipGuiDisplayParamaters::instance()->plotRenderingStrategy();
	if (st == VipBaseGraphicsView::Raster)
		d_data->pdirect->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGLThread)
		d_data->pGPUThreaded->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGL)
		d_data->ppureGPU->setChecked(true);
}
