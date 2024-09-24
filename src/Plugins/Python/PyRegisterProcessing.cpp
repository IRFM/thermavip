#include "PyRegisterProcessing.h"
#include "PyProcessing.h"
#include "PySignalFusionProcessing.h"

#include "VipXmlArchive.h"
#include "VipStandardWidgets.h"

#include <qtreewidget.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qplaintextedit.h>
#include <qmessagebox.h>
#include <qgridlayout.h>



template< class ProcessingType>
QMultiMap<QString, VipProcessingObject::Info> _validProcessingObjects(const QVariantList & lst, int output_count, VipProcessingObject::DisplayHint maxDisplayHint)
{
	QMultiMap<QString, VipProcessingObject::Info> res;
	const QList<const VipProcessingObject*> all = VipProcessingObject::allObjects();

	for (int i = 0; i < all.size(); ++i)
	{
		const VipProcessingObject * obj = all[i];
		if (!qobject_cast<ProcessingType>((VipProcessingObject*)obj))
			continue;

		if (lst.isEmpty() && output_count < 0) {
			res.insert(obj->category(), obj->info());
			continue;
		}

		//First check the input and output count just based on the meta object, without actually creating an instance of VipProcessingObject
		const QMetaObject * meta = obj->metaObject();
		if (!meta)
			continue;

		int in_count, out_count;
		VipProcessingObject::IOCount(meta, &in_count, nullptr, &out_count);
		if (lst.size() && !in_count)
			continue;
		else if (output_count && !out_count)
			continue;
		if (lst.size() > 1 && obj->displayHint() == VipProcessingObject::InputTransform)
			continue;

		VipProcessingObject::Info info = obj->info();
		if (info.displayHint > maxDisplayHint)
			continue;

		if (lst.size() == 0) {
			if (output_count < 0)
				res.insert(obj->category(), info);
			else if (output_count == 0) {
				if (obj->outputCount() == 0)
					res.insert(obj->category(), info);
			}
			else if (obj->outputCount() == output_count) {
				res.insert(obj->category(), info);
			}
			else {
				VipMultiOutput * out = obj->topLevelOutputAt(0)->toMultiOutput();
				if (out && out->minSize() <= output_count && out->maxSize() >= output_count)
					res.insert(obj->category(), info);
			}
		}
		else {
			if (obj->topLevelInputCount() > 0)
				if (VipMultiInput * multi = obj->topLevelInputAt(0)->toMultiInput())
				{
					if (multi->minSize() > lst.size() || lst.size() > multi->maxSize()) {
						//min/max size of VipMultiInput not compatible with the input list
						continue;
					}
					multi->resize(lst.size());
				}

			if (lst.size() == obj->inputCount()) {
				bool accept_all = true;
				for (int j = 0; j < lst.size(); ++j) {
					if (!obj->acceptInput(j, lst[j]) && lst[j].userType() != 0) {
						accept_all = false;
						break;
					}
				}
				if (accept_all) {
					if (output_count < 0)
						res.insert(obj->category(), info);
					else if (output_count == 0) {
						if (obj->outputCount() == 0)
							res.insert(obj->category(), info);
					}
					else if (obj->outputCount() == output_count) {
						res.insert(obj->category(), info);
					}
					else {
						VipMultiOutput * out = obj->topLevelOutputAt(0)->toMultiOutput();
						if (out && out->minSize() <= output_count && out->maxSize() >= output_count)
							res.insert(obj->category(), info);
					}
				}
			}
		}
	}


	return res;
}



bool PyRegisterProcessing::saveCustomProcessings(const QList<VipProcessingObject::Info> & infos)
{
	//generate xml file contianing the processings
	VipXOStringArchive arch;
	arch.start("processings");
	for (int i = 0; i < infos.size(); ++i) {
		if (PySignalFusionProcessingPtr ptr = infos[i].init.value<PySignalFusionProcessingPtr>()) {
			arch.content("name", infos[i].classname);
			arch.content("category", infos[i].category);
			arch.content("description", infos[i].description);
			arch.content(ptr.data());
		}
		else if (PyProcessingPtr ptr = infos[i].init.value<PyProcessingPtr>()) {
			arch.content("name", infos[i].classname);
			arch.content("category", infos[i].category);
			arch.content("description", infos[i].description);
			arch.content(ptr.data());
		}
	}
	arch.end();

	//write registered processing to custom_data_fusion.xml
	QString proc_file = vipGetPythonDirectory() + "custom_python_processing.xml";
	QFile out(proc_file);
	if (out.open(QFile::WriteOnly | QFile::Text)) {
		out.write(arch.toString().toLatin1());
		return true;
	}
	return false;
}

QList<VipProcessingObject::Info> PyRegisterProcessing::customProcessing()
{
	//get additional PySignalFusionProcessing
	QList<VipProcessingObject::Info> infos1 = VipProcessingObject::additionalInfoObjects(qMetaTypeId<PySignalFusionProcessing*>());

	//get additional PyProcessing
	QList<VipProcessingObject::Info> infos2 = VipProcessingObject::additionalInfoObjects(qMetaTypeId<PyProcessing*>());
	//remove PyProcessing that rely on Python files
	for (int i = 0; i < infos2.size(); ++i) {
		if (!infos2[i].init.value<PyProcessingPtr>()) {
			infos2.removeAt(i);
			--i;
		}
	}

	return infos1 + infos2;
}

bool PyRegisterProcessing::saveCustomProcessings()
{
	return saveCustomProcessings(customProcessing());
}


int PyRegisterProcessing::loadCustomProcessings(bool overwrite)
{
	//returns the number of read procesings, or -1 on error

	QFile in(vipGetPythonDirectory() + "custom_python_processing.xml");
	if (!in.open(QFile::ReadOnly | QFile::Text))
		return 0; //no file, no processings

	QString content = in.readAll();
	VipXIStringArchive arch(content);
	arch.start("processings");

	int res = 0;
	while (true) {
		QString name = arch.read("name").toString();
		QString category = arch.read("category").toString();
		QString description = arch.read("description").toString();
		if (!arch)
			break;
		QVariant proc = arch.read();
		if (!arch || proc.userType() == 0)
			break;

		VipProcessingObject::Info info;
		info.classname = name;
		info.category = category.split("/",QString::SkipEmptyParts).join("/");
		info.description = description;
		

		if (PySignalFusionProcessingPtr ptr = PySignalFusionProcessingPtr(proc.value<PySignalFusionProcessing*>())) {
			info.init = QVariant::fromValue(ptr);
			info.displayHint = VipProcessingObject::DisplayOnSameSupport;
			info.metatype = qMetaTypeId<PySignalFusionProcessing*>();
		}
		else if (PyProcessingPtr ptr2 = PyProcessingPtr(proc.value<PyProcessing*>())) {
			info.init = QVariant::fromValue(ptr2);
			info.displayHint = VipProcessingObject::InputTransform;
			info.metatype = qMetaTypeId<PyProcessing*>();
		}
		else //error while loading processing, return -1
			return -1;

		//check if already registerd
		if (!overwrite) {
			bool found = false;
			QList<VipProcessingObject::Info> infos = VipProcessingObject::additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i) {
				if (infos[i].classname == name && infos[i].category == info.category) {
					found = true;
					break;
				}
			}
			if (found)
				continue;
		}
		VipProcessingObject::registerAdditionalInfoObject(info);
		++res;
	}


	//QMultiMap<QString, VipProcessingObject::Info> infos =  _validProcessingObjects<PyProcessing*>(QVariantList() << QVariant::fromValue(VipPointVector()), 1, VipProcessingObject::InputTransform);

	return res;
}


void PyRegisterProcessing::openProcessingManager()
{
	PySignalFusionProcessingManager * m = new PySignalFusionProcessingManager();
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






class PyApplyToolBar::PrivateData
{
public:
	QPushButton *apply;
	QToolButton *save;
	QToolButton *manage;
};
PyApplyToolBar::PyApplyToolBar(QWidget * parent)
	:QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->apply = new QPushButton();
	d_data->apply->setText("Update/Apply processing");
	d_data->apply->setToolTip("<b>Update/Apply the processing</b><br>"
		"Use this button to reapply the processing if you modified the Python scripts, the output signal title or the signal unit.");
	d_data->save = new QToolButton();
	d_data->save->setAutoRaise(true);
	d_data->save->setIcon(vipIcon("save.png"));
	d_data->save->setToolTip("<b>Register this processing</b><br>Register this processing and save it into your session.<br>This new processing will be available through the processing menu shortcut.");
	d_data->manage = new QToolButton();
	d_data->manage->setIcon(vipIcon("tools.png"));
	d_data->manage->setToolTip("<b>Manage registered processing</b><br>Manage (edit/suppress) the processing that you already registered within your session.");
	QHBoxLayout * blay = new QHBoxLayout();
	blay->setContentsMargins(0, 0, 0, 0);
	blay->setSpacing(0);
	blay->addWidget(d_data->apply);
	blay->addWidget(d_data->save);
	blay->addWidget(d_data->manage);
	setLayout(blay);
}
PyApplyToolBar::~PyApplyToolBar()
{
}

QPushButton *PyApplyToolBar::applyButton() const
{
	return d_data->apply;
}
QToolButton * PyApplyToolBar::registerButton() const
{
	return d_data->save;
}
QToolButton * PyApplyToolBar::manageButton() const
{
	return d_data->manage;
}



class PySignalFusionProcessingManager::PrivateData
{
public:
	QGroupBox * createWidget;
	QLineEdit *name;
	QLineEdit *category;
	QPlainTextEdit *description;

	QGroupBox *editWidget;
	QTreeWidget *procList;
	QPlainTextEdit * procDescription;
	PySignalFusionProcessingEditor * procEditor;
	PyProcessingEditor * pyEditor;
};


PySignalFusionProcessingManager::PySignalFusionProcessingManager(QWidget * parent)
	:QWidget(parent)
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

	QGridLayout * glay = new QGridLayout();
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
	d_data->procList->setHeaderLabels(QStringList() << "Name" << "Category");
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
	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addWidget(d_data->procList);
	vlay->addWidget(d_data->procDescription);
	vlay->addWidget(d_data->procEditor);
	vlay->addWidget(d_data->pyEditor);
	d_data->editWidget->setLayout(vlay);
	d_data->procEditor->setEnabled(false);
	d_data->procDescription->setEnabled(false);
	d_data->procDescription->setMaximumHeight(120);
	d_data->pyEditor->hide();

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(d_data->createWidget);
	lay->addWidget(d_data->editWidget);
	setLayout(lay);

	connect(d_data->procList, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(itemClicked(QTreeWidgetItem *, int)));
	connect(d_data->procList, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(itemDoubleClicked(QTreeWidgetItem *, int)));
	connect(d_data->procList, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showMenu(const QPoint &)));
	connect(d_data->procList, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
	connect(d_data->procDescription, SIGNAL(textChanged()), this, SLOT(descriptionChanged()));
}

PySignalFusionProcessingManager::~PySignalFusionProcessingManager()
{
}

QString PySignalFusionProcessingManager::name() const { return d_data->name->text(); }
QString PySignalFusionProcessingManager::category() const { return d_data->category->text(); }
QString PySignalFusionProcessingManager::description() const { return d_data->description->toPlainText(); }

void PySignalFusionProcessingManager::setName(const QString & name) { d_data->name->setText(name); }
void PySignalFusionProcessingManager::setCategory(const QString & cat) { d_data->category->setText(cat); }
void PySignalFusionProcessingManager::setDescription(const QString & desc) { d_data->description->setPlainText(desc); }

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
	//get all processing infos and copy them to d_data->processings
	QList<VipProcessingObject::Info> infos = PyRegisterProcessing::customProcessing();

	//we set valid inputs to the processing in order to be applied properly
	VipPointVector v(100);
	for (int i = 0; i < 100; ++i) v[i] = VipPoint(i * 1000, i * 1000);

	//update tree widget
	d_data->procList->clear();
	for (int i = 0; i < infos.size(); ++i) {
		VipProcessingObject::Info info = infos[i];
		VipProcessingObject * tmp = nullptr;
		//copy the init member
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
			init->propertyName("output_unit")->setData(p->propertyName("output_unit")->data());
			info.init = QVariant::fromValue(init);
			tmp = init.data();
		}
		else 
			continue;

		//set processing inputs
		for (int j = 0; j < tmp->inputCount(); ++j) {
			VipAnyData any; any.setData(QVariant::fromValue(v));
			any.setName("Input " + QString::number(j));
			tmp->inputAt(j)->setData(any);
		}

		//create tree item
		QTreeWidgetItem * item = new QTreeWidgetItem();
		item->setText(0, info.classname);
		item->setText(1, info.category);
		item->setToolTip(0, info.description);
		item->setToolTip(1, info.description);
		QFont f = item->font(0); f.setBold(true); item->setFont(0, f);
		item->setData(0, 1000, QVariant::fromValue(info));
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		d_data->procList->addTopLevelItem(item);
	}
	

	if (d_data->procList->topLevelItemCount() == 0) {
		//No additional processing: add a dummy item
		QTreeWidgetItem * item = new QTreeWidgetItem();
		item->setText(0, "No registered processing available");
		item->setToolTip(0, "No registered processing available");
		d_data->procList->addTopLevelItem(item);
	}
	else {
		//select the first one
		d_data->procList->topLevelItem(0)->setSelected(true);
	}

}

void PySignalFusionProcessingManager::itemDoubleClicked(QTreeWidgetItem * item, int column)
{
	d_data->procList->editItem(item, column);
}

bool PySignalFusionProcessingManager::applyChanges()
{
	//remove all registered PySignalFusionProcessing
	QList<VipProcessingObject::Info > infos = PyRegisterProcessing::customProcessing();
	for (int i = 0; i < infos.size(); ++i)
		VipProcessingObject::removeInfoObject(infos[i]);

	
	//add new ones
	infos.clear();
	for (int i = 0; i < d_data->procList->topLevelItemCount(); ++i) {
		//get the internal VipProcessingObject::Info object and set its classname and category (might have been modified by the user)
		VipProcessingObject::Info info = d_data->procList->topLevelItem(i)->data(0, 1000).value<VipProcessingObject::Info>();
		info.classname = d_data->procList->topLevelItem(i)->text(0);
		info.category = d_data->procList->topLevelItem(i)->text(1);
		if (info.metatype) {
			VipProcessingObject::registerAdditionalInfoObject(info);
			infos.append(info);
		}
	}

	//save processings
	return PyRegisterProcessing::saveCustomProcessings(infos);
}

void PySignalFusionProcessingManager::removeSelection()
{
	//remove selected registered processings
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		delete items[i];
	}
}

void PySignalFusionProcessingManager::keyPressEvent(QKeyEvent * evt)
{
	if (evt->key() == Qt::Key_Delete)
		removeSelection();
}

void PySignalFusionProcessingManager::showMenu(const QPoint & )
{
	//QMenu menu;
}

void PySignalFusionProcessingManager::descriptionChanged()
{
	//udpate selected processing description
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

void PySignalFusionProcessingManager::itemClicked(QTreeWidgetItem * item, int)
{
	QList<QTreeWidgetItem*> items = d_data->procList->selectedItems();

	d_data->procDescription->setEnabled(items.size() == 1);

	QTreeWidgetItem * selected = nullptr;
	if (item->isSelected())
		selected = item;
	else if (items.size())
		selected = items.last();

	d_data->procEditor->setEnabled(selected);
	d_data->pyEditor->setEnabled(selected);

	if (selected) {
		VipProcessingObject::Info info = selected->data(0, 1000).value<VipProcessingObject::Info>();
		if (info.metatype) {
			//set description
			d_data->procDescription->setPlainText(info.description);

			//set the processing editor
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