#include "VipEditXMLSymbols.h"
#include "VipStandardWidgets.h"
#include "VipEnvironment.h"
#include "VipDisplayArea.h"
#include "VipProgress.h"
#include "VipTextOutput.h"

#include <QListWidget>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <qboxlayout.h>
#include <qradiobutton.h>


static VipEditXMLSymbols * defaultEditXMLSymbols()
{
	return new VipEditXMLSymbols();
}

static std::function<VipBaseEditXMLSymbols*()> _edit_xml = defaultEditXMLSymbols;

void vipSetBaseEditXMLSymbols(const std::function<VipBaseEditXMLSymbols*()> & fun)
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
		QHBoxLayout * hlay = new QHBoxLayout();

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

VipEditXMLSymbols::VipEditXMLSymbols(QWidget * parent)
	:VipBaseEditXMLSymbols(parent)
{
	m_data = new PrivateData();
	m_data->list.setSelectionMode(QListWidget::ExtendedSelection);

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(&m_data->list);
	lay->setContentsMargins(0, 0, 0, 0);

	setLayout(lay);
}

VipEditXMLSymbols::~VipEditXMLSymbols()
{
	delete m_data;
}

void VipEditXMLSymbols::setEditableSymbols(const QList<VipEditableArchiveSymbol> & symbols)
{
	m_data->symbols = symbols;
	m_data->list.clear();

	for (int i = 0; i < symbols.size(); ++i)
	{
		SymbolWidget * w = new SymbolWidget();
		w->title.setText("<b>" + symbols[i].name + "</b>: " + symbols[i].default_value);
		//w->title.setText(symbols[i].info);
		w->title.setToolTip(symbols[i].info);
		w->group.setValue(qMax(1, symbols[i].id));
		w->symbol = symbols[i];

		connect(&w->select, SIGNAL(clicked(bool)), this, SLOT(selectionChanged(bool)));
		connect(&w->group, SIGNAL(valueChanged(int)), this, SLOT(valueChanged(int)));
		//QLabel * w = new QLabel("ceci est un test");

		QListWidgetItem * item = new QListWidgetItem(&m_data->list);
		item->setSizeHint(w->sizeHint());
		m_data->list.addItem(item);
		m_data->list.setItemWidget(item,w);
	}
}

const QList<VipEditableArchiveSymbol> & VipEditXMLSymbols::editableSymbols() const
{
	return m_data->symbols;
}

void VipEditXMLSymbols::applyToArchive(VipXArchive & arch)
{
	QList<VipEditableArchiveSymbol> symbols;
	for (int i = 0; i < m_data->list.count(); ++i)
	{
		SymbolWidget * w = static_cast<SymbolWidget*>(m_data->list.itemWidget(m_data->list.item(i)));
		if (w->select.isChecked())
		{
			VipEditableArchiveSymbol symbol = w->symbol;
			symbol.id = w->group.value();
			symbols.append(symbol);
		}
	}

	arch.setEditableSymbols(symbols);
}

void VipEditXMLSymbols::selectionChanged(bool checked)
{
	QList<QListWidgetItem*> items = m_data->list.selectedItems();
	for (int i = 0; i < items.size(); ++i)
	{
		SymbolWidget * w = static_cast<SymbolWidget*>(m_data->list.itemWidget(items[i]));
		if (w->select.isChecked() != checked)
		{
			w->select.blockSignals(true);
			w->select.setChecked(checked);
			w->select.blockSignals(false);
		}
	}
}

void VipEditXMLSymbols::valueChanged(int value)
{
	QList<QListWidgetItem*> items = m_data->list.selectedItems();
	for (int i = 0; i < items.size(); ++i)
	{
		SymbolWidget * w = static_cast<SymbolWidget*>(m_data->list.itemWidget(items[i]));
		if (w->group.value() != value)
		{
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
	//QRadioButton currentPlayer;
	QGroupBox showXMLSymbols;
	VipBaseEditXMLSymbols * XMLSymbols;
};

VipExportSessionWidget::VipExportSessionWidget(QWidget * parent, bool export_current_area)
	:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->filename.setMode(VipFileName::Save);
	m_data->filename.setFilters("Session file (*.session)");
	m_data->filename.setTitle("Session file");
	m_data->filename.setDefaultOpenDir(vipGetUserPerspectiveDirectory());
	m_data->mainWindow.setText("Export the whole session");
	m_data->currentArea.setText("Export the current workspace");
	//m_data->currentPlayer.setText("Export the selected top level player");
	m_data->showXMLSymbols.setTitle("Create an editable session file");
	m_data->showXMLSymbols.setCheckable(true);
	m_data->showXMLSymbols.setChecked(false);
	m_data->showXMLSymbols.setFlat(true);

	m_data->XMLSymbols = _edit_xml();
	m_data->XMLSymbols->hide();

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(&m_data->filename);
	lay->addWidget(VipLineWidget::createSunkenHLine());
	lay->addWidget(&m_data->mainWindow);
	lay->addWidget(&m_data->currentArea);
	//lay->addWidget(&m_data->currentPlayer);
	lay->addWidget(&m_data->showXMLSymbols);
	lay->addWidget(m_data->XMLSymbols);

	setLayout(lay);

	if (export_current_area)
		m_data->currentArea.setChecked(true);
	else
		m_data->mainWindow.setChecked(true);
	exportTypeChanged();

	connect(&m_data->showXMLSymbols, SIGNAL(clicked(bool)), m_data->XMLSymbols, SLOT(setVisible(bool)));
	connect(&m_data->mainWindow, SIGNAL(clicked(bool)), this, SLOT(exportTypeChanged()));
	connect(&m_data->currentArea, SIGNAL(clicked(bool)),this, SLOT(exportTypeChanged()));

	this->setMinimumWidth(300);
}

VipExportSessionWidget::~VipExportSessionWidget()
{
	delete m_data;
}

bool VipExportSessionWidget::exportMainWindow() const {return m_data->mainWindow.isChecked();}
bool VipExportSessionWidget::exportCurrentArea() const { return m_data->currentArea.isChecked(); }
//bool VipExportSessionWidget::exportCurrentPlayer() const { return m_data->currentPlayer.isChecked(); }

void VipExportSessionWidget::setExportCurrentArea(bool enable)
{
	m_data->currentArea.setChecked(enable);
	exportTypeChanged();
}

QString VipExportSessionWidget::filename() const
{
	return m_data->filename.filename();
}

void VipExportSessionWidget::setFilename(const QString& filename)
{
	m_data->filename.setFilename(filename);
}

void VipExportSessionWidget::exportSession()
{
	QString filename = m_data->filename.filename();
	if (!filename.isEmpty())
	{
		VipProgress progress;
		progress.setModal(true);
		progress.setText("<b>Save session in</b> " + filename + "...");

		filename.replace("\\", "/");
		if (!filename.contains("/")) {
			// The user provided a simple filename, save it into the Perspectives folder
			filename  = vipGetPerspectiveDirectory() + filename;
			if (!filename.endsWith(".session"))
				filename += ".session";
		}

		VipXOfArchive arch(filename);
		int session_type = 0;
		if (exportCurrentArea())
			session_type = VipMainWindow::CurrentArea;
		//else if (exportCurrentPlayer())
		// session_type = VipMainWindow::CurrentPlayer;

		if(vipGetMainWindow()->saveSession(arch, session_type, VipMainWindow::All))
			if(!m_data->XMLSymbols->isHidden())
				m_data->XMLSymbols->applyToArchive(arch);
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
	//remove duplicates (editable symbols with the same name at the same location)
	QSet<QString> locs;
	for (int i = lst.size() - 1; i >= 0; --i)
	{
		QString location = lst[i].location;
		//remove the last '#' and number
		int index = location.lastIndexOf("#");
		if (index >= 0)
			location = location.mid(0, index);

		if (!locs.contains(location))
			locs.insert(location);
		else
		{
			lst.removeAt(i);
		}
	}

	m_data->XMLSymbols->setEditableSymbols(lst);
}




class VipImportSessionWidget::PrivateData
{
public:
	QVBoxLayout * layout;
	QMap<QWidget *, QList<VipEditableArchiveSymbol> > widgets;
};

VipImportSessionWidget::VipImportSessionWidget(QWidget * parent)
	:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->layout = new QVBoxLayout();
	m_data->layout->setSpacing(0);
	this->setLayout(m_data->layout);

	this->setMinimumWidth(300);
}

VipImportSessionWidget::~VipImportSessionWidget()
{
	delete m_data;
}

bool VipImportSessionWidget::hasEditableContent(VipXArchive & arch)
{
	QList<VipEditableArchiveSymbol> s = arch.editableSymbols();
	for (int i = 0; i < s.size(); ++i)
		if (s[i].id > 0)
			return true;
	return false;
}

void VipImportSessionWidget::importArchive(VipXArchive & arch)
{
	//frist, remove previous widgets
	while (m_data->layout->count())
		delete m_data->layout->takeAt(0);
	m_data->widgets.clear();

	//add the new widgets
	QList<VipEditableArchiveSymbol> s = arch.editableSymbols();

	//sort symbols by index
	QMap<int, QList<VipEditableArchiveSymbol> > symbols;
	for (int i = 0; i < s.size(); ++i)
	{
		if (s[i].id > 0)
			symbols[s[i].id].append(s[i]);
	}

	//create widgets
	int row = 0;
	for (QMap<int, QList<VipEditableArchiveSymbol> >::iterator it = symbols.begin(); it != symbols.end(); ++it, ++row)
	{
		const QList<VipEditableArchiveSymbol> & lst = it.value();
		QWidget * editor = VipStandardWidgets::fromStyleSheet(lst.first().style_sheet);
		if (!editor)
		{
			editor = new VipLineEdit();
		}
		editor->setProperty("value", lst.first().default_value);
		editor->setToolTip(lst.first().info);

		m_data->widgets[editor] = lst;

		//add to the grid
		if (it != symbols.begin())
			m_data->layout->addSpacing(5);
		m_data->layout->addWidget(new QLabel("<b>&#9660; " + lst.first().info + "</b>"));
		m_data->layout->addWidget(editor);

	}

	if (symbols.size() == 1)
	{
		setWindowTitle(symbols.first().first().info);
	}
}

void VipImportSessionWidget::applyToArchive(VipXArchive & arch)
{
	QDomNode topnode = arch.topNode();

	for (QMap<QWidget *, QList<VipEditableArchiveSymbol> >::iterator it = m_data->widgets.begin(); it != m_data->widgets.end(); ++it)
	{
		QVariant value = it.key()->property("value");
		QList<VipEditableArchiveSymbol> lst = it.value();
		for (int i = 0; i < lst.size(); ++i)
		{
			QDomElement node = lst[i].locationToNode(lst[i].location, topnode).toElement();
			if(!node.isNull())
				VipXArchive::setContent(node, value.toString());
		}
	}
}
