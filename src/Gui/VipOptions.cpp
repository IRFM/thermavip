/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#include "VipCore.h"
#include "VipDisplayArea.h"
#include "VipEnvironment.h"
#include "VipLogging.h"
#include "VipOptions.h"
#include "VipStandardEditors.h"
#include "VipStandardWidgets.h"

QGroupBox* VipPageOption::createOptionGroup(const QString& label)
{
	QGroupBox* res = new QGroupBox(label);
	res->setFlat(true);
	QFont f = res->font();
	f.setBold(true);
	res->setFont(f);
	return res;
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
	QPushButton* ok;
	QPushButton* cancel;
	QVBoxLayout* page_lay;
};

VipOptions::VipOptions(QWidget* parent)
  : QDialog(parent)
{
	m_data = new PrivateData();

	this->setObjectName("Preferences");
	this->setWindowTitle("Preferences");
	this->resize(800, 600);

	m_data->page_browser = new VipPageItems();
	m_data->splitter = new QSplitter();
	m_data->ok = new QPushButton();
	m_data->cancel = new QPushButton();

	m_data->page_browser->header()->hide();
	m_data->page_browser->setAcceptDrops(true);
	m_data->page_browser->setFrameShape(QFrame::Box);
	m_data->page_browser->setIndentation(10);
	m_data->page_browser->setMinimumWidth(200);

	// create page widget
	QWidget* page = new QWidget();
	m_data->page_lay = new QVBoxLayout();
	page->setLayout(m_data->page_lay);

	// create splitter
	m_data->splitter->addWidget(m_data->page_browser);
	m_data->splitter->addWidget(page);
	m_data->splitter->setStretchFactor(1, 1);

	// create button layout
	QHBoxLayout* button_lay = new QHBoxLayout();
	button_lay->addStretch(1);
	button_lay->addWidget(m_data->ok);
	button_lay->addWidget(m_data->cancel);
	button_lay->setContentsMargins(0, 2, 2, 2);

	// create final layout
	QVBoxLayout* final_lay = new QVBoxLayout();
	final_lay->addWidget(m_data->splitter);
	final_lay->addWidget(VipLineWidget::createSunkenHLine());
	final_lay->addLayout(button_lay);
	// final_lay->setContentsMargins(0, 0, 0, 0);

	setLayout(final_lay);

	m_data->ok->setText("Ok");
	m_data->cancel->setText("Cancel");

	connect(m_data->ok, SIGNAL(clicked(bool)), this, SLOT(ok()));
	connect(m_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(m_data->cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
	connect(m_data->page_browser, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(itemClicked(QTreeWidgetItem*, int)));
}

VipOptions::~VipOptions()
{
	delete m_data;
}

void VipOptions::ok()
{
	for (QMap<QTreeWidgetItem*, QScrollArea*>::iterator it = m_data->pages.begin(); it != m_data->pages.end(); ++it) {
		VipPageOption* page = qobject_cast<VipPageOption*>(it.value()->widget());
		page->applyPage();
	}
}

bool VipOptions::hasPage(VipPageOption* page) const
{
	for (QMap<QTreeWidgetItem*, QScrollArea*>::const_iterator it = m_data->pages.begin(); it != m_data->pages.end(); ++it)
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

	// create the page leaf inside m_data->page_browser
	QTreeWidgetItem* current = m_data->page_browser->invisibleRootItem();

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
			item->setText(0, path[i]);
			current->addChild(item);
			current = item;
			// set its icon
			if (i == path.size() - 1 && !icon.isNull())
				item->setIcon(0, icon);
		}
		else
			current = found;
	}

	QScrollArea* area = new QScrollArea();
	area->setWidgetResizable(true);
	area->setWidget(page);
	m_data->pages[current] = area;
	m_data->page_lay->addWidget(area);
	area->hide();

	// set the focus to the first page
	if (m_data->pages.size() > 0 && !m_data->current)
		setCurrentPage(qobject_cast<VipPageOption*>(m_data->pages.begin().value()->widget()));

	return true;
}

void VipOptions::setTreeWidth(int w)
{
	m_data->splitter->setSizes(QList<int>() << w << (width() - w));
}

QScrollArea* VipOptions::areaForPage(VipPageOption* page) const
{
	for (QMap<QTreeWidgetItem*, QScrollArea*>::const_iterator it = m_data->pages.begin(); it != m_data->pages.end(); ++it)
		if (qobject_cast<VipPageOption*>(it.value()->widget()) == page)
			return it.value();
	return nullptr;
}

void VipOptions::setCurrentPage(VipPageOption* page)
{
	if (page && page != m_data->current) {
		if (!hasPage(page))
			return;

		QTreeWidgetItem* item = m_data->pages.key(areaForPage(page));

		for (QMap<QTreeWidgetItem*, QScrollArea*>::const_iterator it = m_data->pages.begin(); it != m_data->pages.end(); ++it) {
			it.value()->setVisible(qobject_cast<VipPageOption*>(it.value()->widget()) == page);
		}

		m_data->page_browser->blockSignals(true);
		m_data->page_browser->setCurrentItem(item);
		m_data->page_browser->blockSignals(false);
		m_data->current = page;
	}
}

void VipOptions::itemClicked(QTreeWidgetItem* item, int)
{
	if (!item)
		return;

	// find item
	QMap<QTreeWidgetItem*, QScrollArea*>::iterator it = m_data->pages.find(item);
	if (it == m_data->pages.end()) {
		// if item is not inside m_data->pages, try set the focus to its first child
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
		dialog->addPage("General display", new AppearanceSettings());
		dialog->addPage("Rendering modes", new RenderingSettings());
		dialog->addPage("Environment", new EnvironmentSettings());
		dialog->addPage("Processings", new ProcessingSettings());
		dialog->setTreeWidth(150);
	}
	return dialog;
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

	QGroupBox* players;
	QComboBox* colorMaps;
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
	m_data = new PrivateData();

	m_data->general = createGroup("General appearance");
	m_data->skins = new QComboBox();
	m_data->skins->addItems((QStringList() << "Default") + vipAvailableSkins());

	m_data->colorPalette = new QComboBox();
	QImage palette = vipPixmap("palette.png").toImage().convertToFormat(QImage::Format_ARGB32);
	if (!palette.isNull()) {
		m_data->colorPalette->addItem("Default", 0);
		m_data->colorPalette->addItem(applyFactor(palette, 60), "Very dark", 60);
		m_data->colorPalette->addItem(applyFactor(palette, 80), "Dark", 80);
		m_data->colorPalette->addItem(QPixmap::fromImage(palette), "Standard", 100);
		m_data->colorPalette->addItem(applyFactor(palette, 120), "Light", 120);
		m_data->colorPalette->addItem(applyFactor(palette, 140), "Very light", 140);
	}

	m_data->skins->setToolTip("Global Thermavip color theme");
	m_data->colorPalette->setToolTip("Color palette used for curves and histograms");

	QGridLayout* glay = new QGridLayout();
	glay->addWidget(new QLabel("Color theme"), 0, 0, Qt::AlignLeft);
	glay->addWidget(m_data->skins, 0, 1, Qt::AlignLeft);

	glay->addWidget(new QLabel("Item color palette"), 1, 0, Qt::AlignLeft);
	glay->addWidget(m_data->colorPalette, 1, 1, Qt::AlignLeft);

	m_data->general->setLayout(glay);

	m_data->players = createGroup("Video player display");
	m_data->colorMaps = new QComboBox();
	m_data->showScale = new QToolButton();

	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Autumn, QSize(20, 20)), "Autumn");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Bone, QSize(20, 20)), "Bone");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::BuRd, QSize(20, 20)), "BuRd");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Cool, QSize(20, 20)), "Cool");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Copper, QSize(20, 20)), "Copper");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Gray, QSize(20, 20)), "Gray");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Hot, QSize(20, 20)), "Hot");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Hsv, QSize(20, 20)), "Hsv");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Jet, QSize(20, 20)), "Jet");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Fusion, QSize(20, 20)), "Fusion");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Pink, QSize(20, 20)), "Pink");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Rainbow, QSize(20, 20)), "Rainbow");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Spring, QSize(20, 20)), "Spring");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Summer, QSize(20, 20)), "Summer");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Sunset, QSize(20, 20)), "Sunset");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::White, QSize(20, 20)), "White");
	m_data->colorMaps->addItem(colorMapPixmap(VipLinearColorMap::Winter, QSize(20, 20)), "Winter");
	m_data->colorMaps->setToolTip("Color scale used for videos (like infrared ones)");
	m_data->showScale->setToolTip("Show/hide axes for video players");
	m_data->showScale->setIcon(vipIcon("show_axes.png"));
	m_data->showScale->setAutoRaise(true);
	m_data->showScale->setCheckable(true);

	QGridLayout* dlay = new QGridLayout();
	dlay->addWidget(new QLabel());
	dlay->addWidget(new QLabel("Video color scale"), 0, 0, Qt::AlignLeft);
	dlay->addWidget(m_data->colorMaps, 0, 1, Qt::AlignLeft);
	dlay->addWidget(new QLabel("Show/hide axes"), 1, 0, Qt::AlignLeft);
	dlay->addWidget(m_data->showScale, 1, 1, Qt::AlignLeft);
	m_data->players->setLayout(dlay);

	m_data->roi = createGroup("Regions Of Interest");
	QGridLayout* lay = new QGridLayout();
	int row = -1;

	m_data->title = new VipTextWidget();
	lay->addWidget(new QLabel("Shape text style"), ++row, 0);
	lay->addWidget(m_data->title, row, 1);
	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	m_data->pen = new VipPenButton();
	lay->addWidget(new QLabel("Border pen"), ++row, 0);
	lay->addWidget(m_data->pen, row, 1);

	m_data->brush = new VipPenButton();
	lay->addWidget(new QLabel("Background brush"), ++row, 0);
	lay->addWidget(m_data->brush, row, 1);

	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	m_data->titleVisible = new QCheckBox();
	m_data->groupVisible = new QCheckBox();
	m_data->idVisible = new QCheckBox();
	m_data->innerPixels = new QCheckBox();
	lay->addWidget(m_data->titleVisible, ++row, 0, 1, 2);
	lay->addWidget(m_data->groupVisible, ++row, 0, 1, 2);
	lay->addWidget(m_data->idVisible, ++row, 0, 1, 2);
	lay->addWidget(m_data->innerPixels, ++row, 0, 1, 2);

	m_data->roi->setLayout(lay);

	m_data->title->edit()->hide();
	m_data->pen->setMode(VipPenButton::Pen);
	m_data->brush->setMode(VipPenButton::Brush);

	m_data->titleVisible->setText("Display shapes title");
	m_data->groupVisible->setText("Display shapes group");
	m_data->idVisible->setText("Display shapes identifier");
	m_data->innerPixels->setText("Display exact pixels");

	m_data->displayTimeOffset.setText("Display time offset from left date");
	m_data->displayTimeOffset.setToolTip("<div>For plot players only, if the time scale displays a time since Epoch, select the behavior of the scale values."
					     "If checked, displayed labels represent the time offset since the scale start value."
					     "Otherwise, displayed labels represent the time offset since the start of the left most curve.</div>");
	m_data->displayTimeInteger.setText("Display time as integer");
	m_data->displayTimeInteger.setToolTip("<div>If checked, time values in plot players will be displayed as integer, without precision loss</div>");
	m_data->displayAbsoluteDate.setText("Display absolute date time");
	m_data->displayAbsoluteDate.setToolTip("<div>If checked, time labels in plot players will display absolute date time</div>");
	m_data->displayNormal.setText("Display standard time values");
	m_data->displayNormal.setToolTip("<div>If checked, time labels in plot players will be displayed as floating point values</div>");

	QVBoxLayout* tlay = new QVBoxLayout();
	tlay->addWidget(&m_data->displayTimeOffset);
	tlay->addWidget(&m_data->displayTimeInteger);
	tlay->addWidget(&m_data->displayAbsoluteDate);
	tlay->addWidget(&m_data->displayNormal);
	tlay->setContentsMargins(5, 5, 5, 5);

	m_data->plots = createGroup("Default plot settings");
	QVBoxLayout* play = new QVBoxLayout();
	play->setContentsMargins(0, 0, 0, 0);
	play->addLayout(tlay);
	play->addWidget(&m_data->settings);
	m_data->plots->setLayout(play);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(m_data->general);
	vlay->addWidget(m_data->players);
	vlay->addWidget(m_data->plots);
	vlay->addWidget(m_data->roi);
	vlay->addStretch(1);
	setLayout(vlay);

	connect(m_data->skins, SIGNAL(currentIndexChanged(int)), this, SLOT(skinChanged()));
	//	connect(&VipDefaultSceneModelDisplayOptions::instance(), SIGNAL(changed()), this, SLOT(updatePage()), Qt::QueuedConnection);
	updatePage();
}

AppearanceSettings::~AppearanceSettings()
{
	delete m_data;
}

void AppearanceSettings::skinChanged()
{
	if (m_data->skins->currentText().contains("gray", Qt::CaseInsensitive)) {
		m_data->colorPalette->setCurrentText("Standard");
	}
	else if (m_data->skins->currentText().contains("dark", Qt::CaseInsensitive)) {
		m_data->colorPalette->setCurrentText("Light");
	}
}

void AppearanceSettings::updatePage()
{
	// General settings
	int index = m_data->skins->findText(VipCoreSettings::instance()->skin());
	if (index >= 0)
		m_data->skins->setCurrentIndex(index);
	else
		m_data->skins->setCurrentIndex(0);

	int current = VipGuiDisplayParamaters::instance()->itemPaletteFactor();
	if (current == 0)
		m_data->colorPalette->setCurrentIndex(0);
	else if (current == 60)
		m_data->colorPalette->setCurrentIndex(1);
	else if (current == 80)
		m_data->colorPalette->setCurrentIndex(2);
	else if (current == 100)
		m_data->colorPalette->setCurrentIndex(3);
	else if (current == 120)
		m_data->colorPalette->setCurrentIndex(4);
	else if (current == 140)
		m_data->colorPalette->setCurrentIndex(5);

	// Video player settings
	m_data->colorMaps->setCurrentIndex(VipGuiDisplayParamaters::instance()->playerColorScale());
	m_data->showScale->setChecked(VipGuiDisplayParamaters::instance()->videoPlayerShowAxes());

	// ROI settings
	m_data->brush->setBrush(VipGuiDisplayParamaters::instance()->shapeBackgroundBrush());
	m_data->pen->setPen(VipGuiDisplayParamaters::instance()->shapeBorderPen());
	m_data->title->hide();
	// m_data->title->setText(VipText("", VipDefaultSceneModelDisplayOptions::instance().textStyle()));
	m_data->groupVisible->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::Group);
	m_data->idVisible->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::Id);
	m_data->titleVisible->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::Title);
	m_data->innerPixels->setChecked(VipGuiDisplayParamaters::instance()->shapeDrawComponents() & VipPlotShape::FillPixels);

	m_data->displayTimeOffset.setChecked(VipGuiDisplayParamaters::instance()->displayTimeOffset());

	if (VipGuiDisplayParamaters::instance()->displayType() == VipValueToTime::Integer)
		m_data->displayTimeInteger.setChecked(true);
	else if (VipGuiDisplayParamaters::instance()->displayType() == VipValueToTime::AbsoluteDateTime)
		m_data->displayAbsoluteDate.setChecked(true);
	else
		m_data->displayNormal.setChecked(true);

	m_data->settings.setDefaultCurve(VipGuiDisplayParamaters::instance()->defaultCurve());
	m_data->settings.setDefaultPlotArea(VipGuiDisplayParamaters::instance()->defaultPlotArea());
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

QPixmap AppearanceSettings::colorMapPixmap(int color_map, const QSize& size)
{
	VipLinearColorMap* map = VipLinearColorMap::createColorMap(VipLinearColorMap::StandardColorMap(color_map));
	if (map) {
		QPixmap pix(size);
		QPainter p(&pix);
		VipScaleMap sc;
		sc.setScaleInterval(0, size.height());
		VipPainter::drawColorBar(&p, *map, VipInterval(0, size.height()), sc, Qt::Vertical, QRectF(0, 0, size.width(), size.height()));
		delete map;
		return pix;
	}
	return QPixmap();
}

void AppearanceSettings::applyPage()
{
	// General settings
	if (m_data->skins->currentText() != "Default")
		VipCoreSettings::instance()->setSkin(m_data->skins->currentText());
	else
		VipCoreSettings::instance()->setSkin("gray");

	VipGuiDisplayParamaters::instance()->setItemPaletteFactor(m_data->colorPalette->currentData().toInt());

	// Video player settings
	VipGuiDisplayParamaters::instance()->setPlayerColorScale(VipLinearColorMap::StandardColorMap(m_data->colorMaps->currentIndex()));
	VipGuiDisplayParamaters::instance()->setVideoPlayerShowAxes(m_data->showScale->isChecked());

	// ROI settings
	// disconnect(&VipDefaultSceneModelDisplayOptions::instance(), SIGNAL(changed()), this, SLOT(updatePage()));

	QBrush brush = m_data->brush->pen().brush();
	QPen pen = m_data->pen->pen();
	VipTextStyle style = m_data->title->getText().textStyle();
	VipPlotShape::DrawComponents cmp = VipPlotShape::Background | VipPlotShape::Border;
	if (m_data->groupVisible->isChecked())
		cmp |= VipPlotShape::Group;
	if (m_data->idVisible->isChecked())
		cmp |= VipPlotShape::Id;
	if (m_data->titleVisible->isChecked())
		cmp |= VipPlotShape::Title;
	if (m_data->innerPixels->isChecked())
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

	m_data->settings.applyToCurve(VipGuiDisplayParamaters::instance()->defaultCurve());
	m_data->settings.applyToArea(VipGuiDisplayParamaters::instance()->defaultPlotArea());

	if (m_data->settings.shouldApplyToAllPlayers()) {
		QList<VipPlotPlayer*> pls = vipGetMainWindow()->displayArea()->findChildren<VipPlotPlayer*>();
		for (int i = 0; i < pls.size(); ++i) {
			VipGuiDisplayParamaters::instance()->apply(pls[i]);
			// m_data->settings.applyToArea(qobject_cast<VipPlotArea2D*>(pls[i]->plotWidget2D()->area()));
			//
			// QList<VipPlotCurve*> curves = pls[i]->plotWidget2D()->area()->findItems<VipPlotCurve*>();
			// for (int j = 0; j < curves.size(); ++j)
			// m_data->settings.applyToCurve(curves[j]);

			pls[i]->valueToTimeButton()->setDisplayTimeOffset(m_data->displayTimeOffset.isChecked());
			if (m_data->displayTimeInteger.isChecked())
				pls[i]->valueToTimeButton()->setDisplayType(VipValueToTime::Integer);
			else if (m_data->displayAbsoluteDate.isChecked())
				pls[i]->valueToTimeButton()->setDisplayType(VipValueToTime::AbsoluteDateTime);
			else
				pls[i]->valueToTimeButton()->setDisplayType(VipValueToTime::Double);
		}
	}

	m_data->settings.setShouldApplyToAllPlayers(false);

	VipGuiDisplayParamaters::instance()->setDisplayTimeOffset(m_data->displayTimeOffset.isChecked());
	if (m_data->displayTimeInteger.isChecked())
		VipGuiDisplayParamaters::instance()->setDisplayType(VipValueToTime::Integer);
	else if (m_data->displayAbsoluteDate.isChecked())
		VipGuiDisplayParamaters::instance()->setDisplayType(VipValueToTime::AbsoluteDateTime);
	else
		VipGuiDisplayParamaters::instance()->setDisplayType(VipValueToTime::Double);

	VipGuiDisplayParamaters::instance()->save();
	VipCoreSettings::instance()->save(vipGetDataDirectory() + "core_settings.xml");
}

class ProcessingSettings::PrivateData
{
public:
	QGroupBox* general;
	QComboBox* procPriority;
	QComboBox* displayPriority;
	QCheckBox* printDebug;

	QGroupBox* inputs;
	QCheckBox* maxSizeEnable;
	QSpinBox* maxSize;
	QCheckBox* maxMemoryEnable;
	QSpinBox* maxMemory;
};

ProcessingSettings::ProcessingSettings(QWidget* parent)
  : VipPageOption(parent)
{
	m_data = new PrivateData();

	m_data->general = createGroup("General processing settings");
	m_data->procPriority = new QComboBox();
	m_data->displayPriority = new QComboBox();
	m_data->printDebug = new QCheckBox();

	m_data->printDebug->setText("Print debug informations");

	m_data->procPriority->setToolTip("Set the default processing thread priority used within Thermavip");
	m_data->procPriority->addItem("Idle Priority");
	m_data->procPriority->addItem("Lowest Priority");
	m_data->procPriority->addItem("Low Priority");
	m_data->procPriority->addItem("Normal Priority");
	m_data->procPriority->addItem("High Priority");
	m_data->procPriority->addItem("Highest Priority");
	m_data->procPriority->addItem("Time Critical Priority");
	m_data->procPriority->addItem("Inherit Priority");

	m_data->displayPriority->setToolTip("Set the default processing thread priority used within Thermavip for display only");
	m_data->displayPriority->addItem("Idle Priority");
	m_data->displayPriority->addItem("Lowest Priority");
	m_data->displayPriority->addItem("Low Priority");
	m_data->displayPriority->addItem("Normal Priority");
	m_data->displayPriority->addItem("High Priority");
	m_data->displayPriority->addItem("Highest Priority");
	m_data->displayPriority->addItem("Time Critical Priority");
	m_data->displayPriority->addItem("Inherit Priority");

	QGridLayout* glay = new QGridLayout();
	glay->addWidget(new QLabel("Processing thread priority"), 0, 0);
	glay->addWidget(m_data->procPriority, 0, 1);
	glay->addWidget(new QLabel("Display processing thread priority"), 1, 0);
	glay->addWidget(m_data->displayPriority, 1, 1);
	glay->addWidget(m_data->printDebug, 2, 0, 1, 2);
	m_data->general->setLayout(glay);

	m_data->inputs = createGroup("Input buffer settings");
	m_data->maxSizeEnable = new QCheckBox();
	m_data->maxSize = new QSpinBox();
	m_data->maxMemoryEnable = new QCheckBox();
	m_data->maxMemory = new QSpinBox();

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(m_data->maxSizeEnable);
	vlay->addWidget(m_data->maxSize);
	vlay->addWidget(m_data->maxMemoryEnable);
	vlay->addWidget(m_data->maxMemory);
	vlay->addWidget(m_data->printDebug);
	m_data->inputs->setLayout(vlay);

	m_data->maxSizeEnable->setText("Set maximum input buffer size");
	m_data->maxMemoryEnable->setText("Set maximum input buffer memory");
	m_data->printDebug->setText("Print debug informations");
	m_data->maxSize->setRange(1, INT_MAX);
	m_data->maxSize->setSuffix(" inputs");
	m_data->maxSize->setToolTip("Maximum number of data for each processing input");
	m_data->maxSize->setMaximumWidth(100);
	m_data->maxMemory->setRange(1, INT_MAX);
	m_data->maxMemory->setSuffix("MB");
	m_data->maxMemory->setToolTip("Maximum amount of data for each processing input");
	m_data->maxMemory->setMaximumWidth(100);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(m_data->general);
	lay->addWidget(m_data->inputs);
	lay->addStretch(1);
	setLayout(lay);

	connect(m_data->maxSizeEnable, SIGNAL(clicked(bool)), m_data->maxSize, SLOT(setEnabled(bool)));
	connect(m_data->maxMemoryEnable, SIGNAL(clicked(bool)), m_data->maxMemory, SLOT(setEnabled(bool)));
	connect(&VipProcessingManager::instance(), SIGNAL(changed()), this, SLOT(updatePage()), Qt::QueuedConnection);
	updatePage();
}

ProcessingSettings::~ProcessingSettings()
{
	delete m_data;
}

void ProcessingSettings::applyPage()
{
	disconnect(&VipProcessingManager::instance(), SIGNAL(changed()), this, SLOT(updatePage()));

	int type = 0;
	if (m_data->maxSizeEnable->isChecked())
		type |= VipDataList::Number;
	if (m_data->maxMemoryEnable->isChecked())
		type |= VipDataList::MemorySize;
	int maxSize = m_data->maxSize->value();
	int maxMemory = m_data->maxMemory->value() * 1000000;
	bool printDebug = m_data->printDebug->isChecked();

	int priority = m_data->procPriority->currentIndex();
	int displayPriority = m_data->displayPriority->currentIndex();

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
}

void ProcessingSettings::updatePage()
{
	m_data->maxSizeEnable->setChecked(VipProcessingManager::listLimitType() & VipDataList::Number);
	m_data->maxMemoryEnable->setChecked(VipProcessingManager::listLimitType() & VipDataList::MemorySize);

	m_data->maxSize->setEnabled(VipProcessingManager::listLimitType() & VipDataList::Number);
	m_data->maxMemory->setEnabled(VipProcessingManager::listLimitType() & VipDataList::MemorySize);

	m_data->maxSize->setValue(VipProcessingManager::maxListSize());
	m_data->maxMemory->setValue(VipProcessingManager::maxListMemory() / 1000000);

	m_data->procPriority->setCurrentIndex(VipProcessingManager::defaultPriority(&VipProcessingObject::staticMetaObject));
	m_data->displayPriority->setCurrentIndex(VipProcessingManager::defaultPriority(&VipDisplayObject::staticMetaObject));
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
	m_data = new PrivateData();

	m_data->data = createGroup("Thermavip data folder");
	m_data->dataDir = new QLabel(vipGetDataDirectory());
	m_data->openDataDir = new QToolButton();
	m_data->openDataDir->setAutoRaise(true);
	m_data->openDataDir->setIcon(vipIcon("open_dir.png"));
	m_data->openDataDir->setToolTip("Open data directory");

	m_data->log = createGroup("Thermavip log folder");
	m_data->logDir = new QLabel(vipGetLogDirectory());
	m_data->openLogDir = new QToolButton();
	m_data->openLogDir->setAutoRaise(true);
	m_data->openLogDir->setIcon(vipIcon("open_dir.png"));
	m_data->openLogDir->setToolTip("Open log directory");
	m_data->openLogFile = new QToolButton();
	m_data->openLogFile->setAutoRaise(true);
	m_data->openLogFile->setIcon(vipIcon("open.png"));
	m_data->openLogFile->setToolTip("Open log file");

	m_data->overwrite = new QCheckBox("Overwrite log file at each startup");
	m_data->append_date = new QCheckBox("Append startup date to log file name");

	m_data->env = new QPlainTextEdit();
	m_data->env->setPlaceholderText("Environment variables on the form \"VARIABLE_NAME1 VARIABLE_VALUE1 new_line VARIABLE_NAME2 VARIABLE_VALUE2 \"");

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(m_data->dataDir);
	hlay->addWidget(m_data->openDataDir);
	hlay->addStretch(1);
	m_data->data->setLayout(hlay);

	hlay = new QHBoxLayout();
	hlay->addWidget(m_data->logDir);
	hlay->addWidget(m_data->openLogDir);
	hlay->addWidget(m_data->openLogFile);
	hlay->addStretch(1);
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(m_data->overwrite);
	vlay->addWidget(m_data->append_date);
	m_data->log->setLayout(vlay);

	vlay = new QVBoxLayout();
	vlay->addWidget(m_data->data);
	vlay->addWidget(m_data->log);

	QGroupBox* env_box = new QGroupBox("Environment variables (thermavip.env)");
	env_box->setFlat(true);
	vlay->addWidget(env_box);
	vlay->addWidget(m_data->env, 1);

	// vlay->addStretch(1);
	setLayout(vlay);

	connect(m_data->openDataDir, SIGNAL(clicked(bool)), this, SLOT(openDataDirectory()));
	connect(m_data->openLogDir, SIGNAL(clicked(bool)), this, SLOT(openLogDirectory()));
	connect(m_data->openLogFile, SIGNAL(clicked(bool)), this, SLOT(openLogFile()));
}
EnvironmentSettings::~EnvironmentSettings()
{
	delete m_data;
}

void EnvironmentSettings::applyPage()
{
	VipCoreSettings::instance()->setLogFileDate(m_data->append_date->isChecked());
	VipCoreSettings::instance()->setLogFileOverwrite(m_data->overwrite->isChecked());

	VipCoreSettings::instance()->save(vipGetDataDirectory() + "core_settings.xml");

	// Save env. variables
	QString env_file = vipGetDataDirectory() + "thermavip.env";
	QFile fin(env_file);
	if (fin.open(QFile::WriteOnly | QFile::Text)) {
		fin.write(m_data->env->toPlainText().toLatin1());
	}
	else
		VIP_LOG_ERROR("Unable to create file ", env_file);
}

void EnvironmentSettings::updatePage()
{
	m_data->append_date->setChecked(VipCoreSettings::instance()->logFileDate());
	m_data->overwrite->setChecked(VipCoreSettings::instance()->logFileOverwrite());

	m_data->env->clear();
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
			m_data->env->setPlainText(fin.readAll());
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
	m_data = new PrivateData();

	m_data->vdirect = new QRadioButton("Direct rendering");
	m_data->vdirect->setToolTip("Direct CPU based rendering (default), usually the fastest");

	m_data->vpureGPU = new QRadioButton("Opengl rendering");
	m_data->vpureGPU->setToolTip("Direct opengl based rendering.\nDoes not work with multiple threads.");

	m_data->vGPUThreaded = new QRadioButton("Threaded opengl rendering");
	m_data->vGPUThreaded->setToolTip("Threaded opengl based rendering");

	m_data->pdirect = new QRadioButton("Direct rendering");
	m_data->pdirect->setToolTip("Direct CPU based rendering (default), usually the fastest");

	m_data->ppureGPU = new QRadioButton("Opengl rendering");
	m_data->ppureGPU->setToolTip("Direct opengl based rendering.\nDoes not work with multiple threads.");

	m_data->pGPUThreaded = new QRadioButton("Threaded opengl rendering");
	m_data->pGPUThreaded->setToolTip("Threaded opengl based rendering");

	QGroupBox* video = createGroup("Video rendering mode");
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(m_data->vdirect);
	vlay->addWidget(m_data->vpureGPU);
	vlay->addWidget(m_data->vGPUThreaded);
	video->setLayout(vlay);

	QGroupBox* plot = createGroup("Plot rendering mode");
	QVBoxLayout* play = new QVBoxLayout();
	play->addWidget(m_data->pdirect);
	play->addWidget(m_data->ppureGPU);
	play->addWidget(m_data->pGPUThreaded);
	plot->setLayout(play);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(video);
	lay->addWidget(plot);
	lay->addStretch(1);
	setLayout(lay);
}
RenderingSettings::~RenderingSettings()
{
	delete m_data;
}

void RenderingSettings::applyPage()
{
	if (m_data->vdirect->isChecked())
		VipGuiDisplayParamaters::instance()->setVideoRenderingStrategy(VipBaseGraphicsView::Raster);
	else if (m_data->vGPUThreaded->isChecked())
		VipGuiDisplayParamaters::instance()->setVideoRenderingStrategy(VipBaseGraphicsView::OpenGLThread);
	else if (m_data->vpureGPU->isChecked())
		VipGuiDisplayParamaters::instance()->setVideoRenderingStrategy(VipBaseGraphicsView::OpenGL);

	if (m_data->pdirect->isChecked())
		VipGuiDisplayParamaters::instance()->setPlotRenderingStrategy(VipBaseGraphicsView::Raster);
	else if (m_data->pGPUThreaded->isChecked())
		VipGuiDisplayParamaters::instance()->setPlotRenderingStrategy(VipBaseGraphicsView::OpenGLThread);
	else if (m_data->ppureGPU->isChecked())
		VipGuiDisplayParamaters::instance()->setPlotRenderingStrategy(VipBaseGraphicsView::OpenGL);
}
void RenderingSettings::updatePage()
{
	int st = VipGuiDisplayParamaters::instance()->videoRenderingStrategy();
	if (st == VipBaseGraphicsView::Raster)
		m_data->vdirect->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGLThread)
		m_data->vGPUThreaded->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGL)
		m_data->vpureGPU->setChecked(true);

	st = VipGuiDisplayParamaters::instance()->plotRenderingStrategy();
	if (st == VipBaseGraphicsView::Raster)
		m_data->pdirect->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGLThread)
		m_data->pGPUThreaded->setChecked(true);
	else if (st == VipBaseGraphicsView::OpenGL)
		m_data->ppureGPU->setChecked(true);
}
