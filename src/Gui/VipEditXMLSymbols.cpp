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

#include "VipEditXMLSymbols.h"
#include "VipDisplayArea.h"
#include "VipEnvironment.h"
#include "VipProgress.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QSpinBox>
#include <qboxlayout.h>
#include <qradiobutton.h>

static VipEditXMLSymbols* defaultEditXMLSymbols()
{
	return new VipEditXMLSymbols();
}

static std::function<VipBaseEditXMLSymbols*()> _edit_xml = defaultEditXMLSymbols;

void vipSetBaseEditXMLSymbols(const std::function<VipBaseEditXMLSymbols*()>& fun)
{
	_edit_xml = fun;
}

struct SymbolWidget : public QWidget
{
	QLabel title;
	QCheckBox select;
	QSpinBox group;
	VipEditableArchiveSymbol symbol;

	SymbolWidget()
	{
		select.setText("");
		QHBoxLayout* hlay = new QHBoxLayout();

		hlay->addWidget(&select);
		hlay->addWidget(&title);
		hlay->addWidget(VipLineWidget::createVLine());
		hlay->addWidget(new QLabel("Group:"));
		hlay->addWidget(&group);
		hlay->addStretch();
		hlay->setSizeConstraint(QLayout::SetFixedSize);

		setLayout(hlay);

		group.setRange(1, 100);
		group.setToolTip("Symbols with the same group value will be edited together ");
	}
};

class VipEditXMLSymbols::PrivateData
{
public:
	QList<VipEditableArchiveSymbol> symbols;
	QListWidget list;
};

VipEditXMLSymbols::VipEditXMLSymbols(QWidget* parent)
  : VipBaseEditXMLSymbols(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->list.setSelectionMode(QListWidget::ExtendedSelection);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->list);
	lay->setContentsMargins(0, 0, 0, 0);

	setLayout(lay);
}

VipEditXMLSymbols::~VipEditXMLSymbols()
{
}

void VipEditXMLSymbols::setEditableSymbols(const QList<VipEditableArchiveSymbol>& symbols)
{
	d_data->symbols = symbols;
	d_data->list.clear();

	for (int i = 0; i < symbols.size(); ++i) {
		SymbolWidget* w = new SymbolWidget();
		w->title.setText("<b>" + symbols[i].name + "</b>: " + symbols[i].default_value);
		// w->title.setText(symbols[i].info);
		w->title.setToolTip(symbols[i].info);
		w->group.setValue(qMax(1, symbols[i].id));
		w->symbol = symbols[i];

		connect(&w->select, SIGNAL(clicked(bool)), this, SLOT(selectionChanged(bool)));
		connect(&w->group, SIGNAL(valueChanged(int)), this, SLOT(valueChanged(int)));
		// QLabel * w = new QLabel("ceci est un test");

		QListWidgetItem* item = new QListWidgetItem(&d_data->list);
		item->setSizeHint(w->sizeHint());
		d_data->list.addItem(item);
		d_data->list.setItemWidget(item, w);
	}
}

const QList<VipEditableArchiveSymbol>& VipEditXMLSymbols::editableSymbols() const
{
	return d_data->symbols;
}

void VipEditXMLSymbols::applyToArchive(VipXArchive& arch)
{
	QList<VipEditableArchiveSymbol> symbols;
	for (int i = 0; i < d_data->list.count(); ++i) {
		SymbolWidget* w = static_cast<SymbolWidget*>(d_data->list.itemWidget(d_data->list.item(i)));
		if (w->select.isChecked()) {
			VipEditableArchiveSymbol symbol = w->symbol;
			symbol.id = w->group.value();
			symbols.append(symbol);
		}
	}

	arch.setEditableSymbols(symbols);
}

void VipEditXMLSymbols::selectionChanged(bool checked)
{
	QList<QListWidgetItem*> items = d_data->list.selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		SymbolWidget* w = static_cast<SymbolWidget*>(d_data->list.itemWidget(items[i]));
		if (w->select.isChecked() != checked) {
			w->select.blockSignals(true);
			w->select.setChecked(checked);
			w->select.blockSignals(false);
		}
	}
}

void VipEditXMLSymbols::valueChanged(int value)
{
	QList<QListWidgetItem*> items = d_data->list.selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		SymbolWidget* w = static_cast<SymbolWidget*>(d_data->list.itemWidget(items[i]));
		if (w->group.value() != value) {
			w->group.blockSignals(true);
			w->group.setValue(value);
			w->group.blockSignals(false);
		}
	}
}

class VipExportSessionWidget::PrivateData
{
public:
	VipFileName filename;
	QRadioButton mainWindow;
	QRadioButton currentArea;
	// QRadioButton currentPlayer;
	QGroupBox showXMLSymbols;
	VipBaseEditXMLSymbols* XMLSymbols;
};

VipExportSessionWidget::VipExportSessionWidget(QWidget* parent, bool export_current_area)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->filename.setMode(VipFileName::Save);
	d_data->filename.setFilters("Session file (*.session)");
	d_data->filename.setTitle("Session file");
	d_data->filename.setDefaultOpenDir(vipGetUserPerspectiveDirectory());
	d_data->mainWindow.setText("Export the whole session");
	d_data->currentArea.setText("Export the current workspace");
	// d_data->currentPlayer.setText("Export the selected top level player");
	d_data->showXMLSymbols.setTitle("Create an editable session file");
	d_data->showXMLSymbols.setCheckable(true);
	d_data->showXMLSymbols.setChecked(false);
	d_data->showXMLSymbols.setFlat(true);

	d_data->XMLSymbols = _edit_xml();
	d_data->XMLSymbols->hide();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->filename);
	lay->addWidget(VipLineWidget::createSunkenHLine());
	lay->addWidget(&d_data->mainWindow);
	lay->addWidget(&d_data->currentArea);
	// lay->addWidget(&d_data->currentPlayer);
	lay->addWidget(&d_data->showXMLSymbols);
	lay->addWidget(d_data->XMLSymbols);

	setLayout(lay);

	if (export_current_area)
		d_data->currentArea.setChecked(true);
	else
		d_data->mainWindow.setChecked(true);
	exportTypeChanged();

	connect(&d_data->showXMLSymbols, SIGNAL(clicked(bool)), d_data->XMLSymbols, SLOT(setVisible(bool)));
	connect(&d_data->mainWindow, SIGNAL(clicked(bool)), this, SLOT(exportTypeChanged()));
	connect(&d_data->currentArea, SIGNAL(clicked(bool)), this, SLOT(exportTypeChanged()));

	this->setMinimumWidth(300);
}

VipExportSessionWidget::~VipExportSessionWidget()
{
}

bool VipExportSessionWidget::exportMainWindow() const
{
	return d_data->mainWindow.isChecked();
}
bool VipExportSessionWidget::exportCurrentArea() const
{
	return d_data->currentArea.isChecked();
}
// bool VipExportSessionWidget::exportCurrentPlayer() const { return d_data->currentPlayer.isChecked(); }

void VipExportSessionWidget::setExportCurrentArea(bool enable)
{
	d_data->currentArea.setChecked(enable);
	exportTypeChanged();
}

QString VipExportSessionWidget::filename() const
{
	return d_data->filename.filename();
}

void VipExportSessionWidget::setFilename(const QString& filename)
{
	d_data->filename.setFilename(filename);
}

void VipExportSessionWidget::exportSession()
{
	QString filename = d_data->filename.filename();
	if (!filename.isEmpty()) {
		VipProgress progress;
		progress.setModal(true);
		progress.setText("<b>Save session in</b> " + filename + "...");

		filename.replace("\\", "/");
		if (!filename.contains("/")) {
			// The user provided a simple filename, save it into the Perspectives folder
			filename = vipGetPerspectiveDirectory() + filename;
			if (!filename.endsWith(".session"))
				filename += ".session";
		}

		VipXOfArchive arch(filename);
		int session_type = 0;
		if (exportCurrentArea())
			session_type = VipMainWindow::CurrentArea;
		// else if (exportCurrentPlayer())
		//  session_type = VipMainWindow::CurrentPlayer;

		if (vipGetMainWindow()->saveSession(arch, session_type, VipMainWindow::All))
			if (!d_data->XMLSymbols->isHidden())
				d_data->XMLSymbols->applyToArchive(arch);
	}
}

void VipExportSessionWidget::exportTypeChanged()
{
	int session_type = 0;
	if (exportCurrentArea())
		session_type = VipMainWindow::CurrentArea;

	VipXOStringArchive arch;
	vipGetMainWindow()->saveSession(arch, session_type, VipMainWindow::All);

	QList<VipEditableArchiveSymbol> lst = arch.editableSymbols();
	// remove duplicates (editable symbols with the same name at the same location)
	QSet<QString> locs;
	for (int i = lst.size() - 1; i >= 0; --i) {
		QString location = lst[i].location;
		// remove the last '#' and number
		int index = location.lastIndexOf("#");
		if (index >= 0)
			location = location.mid(0, index);

		if (!locs.contains(location))
			locs.insert(location);
		else {
			lst.removeAt(i);
		}
	}

	d_data->XMLSymbols->setEditableSymbols(lst);
}

class VipImportSessionWidget::PrivateData
{
public:
	QVBoxLayout* layout;
	QMap<QWidget*, QList<VipEditableArchiveSymbol>> widgets;
};

VipImportSessionWidget::VipImportSessionWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->layout = new QVBoxLayout();
	d_data->layout->setSpacing(0);
	this->setLayout(d_data->layout);

	this->setMinimumWidth(300);
}

VipImportSessionWidget::~VipImportSessionWidget()
{
}

bool VipImportSessionWidget::hasEditableContent(VipXArchive& arch)
{
	QList<VipEditableArchiveSymbol> s = arch.editableSymbols();
	for (int i = 0; i < s.size(); ++i)
		if (s[i].id > 0)
			return true;
	return false;
}

void VipImportSessionWidget::importArchive(VipXArchive& arch)
{
	// frist, remove previous widgets
	while (d_data->layout->count())
		delete d_data->layout->takeAt(0);
	d_data->widgets.clear();

	// add the new widgets
	QList<VipEditableArchiveSymbol> s = arch.editableSymbols();

	// sort symbols by index
	QMap<int, QList<VipEditableArchiveSymbol>> symbols;
	for (int i = 0; i < s.size(); ++i) {
		if (s[i].id > 0)
			symbols[s[i].id].append(s[i]);
	}

	// create widgets
	int row = 0;
	for (QMap<int, QList<VipEditableArchiveSymbol>>::iterator it = symbols.begin(); it != symbols.end(); ++it, ++row) {
		const QList<VipEditableArchiveSymbol>& lst = it.value();
		QWidget* editor = VipStandardWidgets::fromStyleSheet(lst.first().style_sheet);
		if (!editor) {
			editor = new VipLineEdit();
		}
		editor->setProperty("value", lst.first().default_value);
		editor->setToolTip(lst.first().info);

		d_data->widgets[editor] = lst;

		// add to the grid
		if (it != symbols.begin())
			d_data->layout->addSpacing(5);
		d_data->layout->addWidget(new QLabel("<b>&#9660; " + lst.first().info + "</b>"));
		d_data->layout->addWidget(editor);
	}

	if (symbols.size() == 1) {
		setWindowTitle(symbols.first().first().info);
	}
}

void VipImportSessionWidget::applyToArchive(VipXArchive& arch)
{
	QDomNode topnode = arch.topNode();

	for (QMap<QWidget*, QList<VipEditableArchiveSymbol>>::iterator it = d_data->widgets.begin(); it != d_data->widgets.end(); ++it) {
		QVariant value = it.key()->property("value");
		QList<VipEditableArchiveSymbol> lst = it.value();
		for (int i = 0; i < lst.size(); ++i) {
			QDomElement node = lst[i].locationToNode(lst[i].location, topnode).toElement();
			if (!node.isNull())
				VipXArchive::setContent(node, value.toString());
		}
	}
}
