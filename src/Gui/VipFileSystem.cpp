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

#include "VipFileSystem.h"
#include "VipCore.h"
#include "VipEnvironment.h"
#include "VipGui.h"
#include "VipLogging.h"
#include "VipMimeData.h"
#include "VipProgress.h"
#include "VipSet.h"
#include "VipStandardEditors.h"
#include "VipDisplayArea.h"
#include "VipSearchLineEdit.h"

#include <QApplication>
#include <QFileIconProvider>
#include <QTemporaryDir>
#include <qclipboard.h>
#include <qcoreapplication.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qfilesystemmodel.h>
#include <qgridlayout.h>
#include <qheaderview.h>
#include <qlist.h>
#include <qmenu.h>
#include <qprocess.h>
#include <qset.h>
#include <qstandardpaths.h>
#include <qstorageinfo.h>
#include <qthread.h>

typedef QList<int> IntList;
Q_DECLARE_METATYPE(IntList)

static int initIntList()
{
	qRegisterMetaTypeStreamOperators<IntList>("IntList");
	qRegisterMetaType<IntList>();
	return 0;
}
static int _initIntList = initIntList();

class VipIconProvider::PrivateData
{
public:
	QIcon dirIcon;
	QIcon driveIcon;
	QMap<QString, QIcon> fileIcons;
	QFileIconProvider provider;
};

VipIconProvider::VipIconProvider()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipIconProvider::~VipIconProvider()
{
}

QFileIconProvider* VipIconProvider::provider() const
{
	return &d_data->provider;
}

/*static bool checkIconValid(const QIcon& icon)
{
	if (icon.isNull())
		return false;
	QPixmap pix = icon.pixmap(8, 8);
	if (pix.isNull())
		return false;
	QImage img = pix.toImage().convertToFormat(QImage::Format_ARGB32);
	if (img.isNull())
		return false;

	const uint* pixels = (const uint*)img.constBits();
	for (int i = 0; i < 64; ++i) {
		if (qAlpha(pixels[i]) != 0)
			return true;
	}
	return false;
}*/

static bool isDrive(const VipPath& path, const QFileInfo& info)
{
#ifdef WIN32
	if (info.isRoot())
		return true;
	int idx = path.canonicalPath().indexOf(":");
	if (idx < 0)
		return false;
	return idx == 1 && path.canonicalPath().size() <= 3;
#endif
	return false;
}

QIcon VipIconProvider::iconPath(const VipPath& path) const
{
	QFileInfo info(path.canonicalPath());
	if (info.isDir()) {
		if (isDrive(path, info)) {
			if (d_data->driveIcon.isNull()){
				const_cast<QIcon&>(d_data->driveIcon) = d_data->provider.icon(info).pixmap(QSize(20,20));
				if(d_data->driveIcon.isNull()){
					vip_debug("Null icon for %s\n",path.canonicalPath().toLatin1().data());
				}
			}
			if (d_data->driveIcon.isNull())
				const_cast<QIcon&>(d_data->driveIcon) = vipIcon("open_dir.png");
			return d_data->driveIcon;
		}
		//if (d_data->dirIcon.isNull())
		//	const_cast<QIcon&>(d_data->dirIcon) = d_data->provider.icon(info).pixmap(1);
		if (d_data->dirIcon.isNull()){
			const_cast<QIcon&>(d_data->dirIcon) = d_data->provider.icon(QFileInfo(QCoreApplication::applicationDirPath())).pixmap(QSize(20,20));
			vip_debug("Null icon for %s\n",path.canonicalPath().toLatin1().data());
		}
		if (d_data->dirIcon.isNull())
			const_cast<QIcon&>(d_data->dirIcon) = vipIcon("open_dir.png");
		return d_data->dirIcon;
	}

	QString suffix = info.suffix();
	auto it = d_data->fileIcons.find(suffix);
	if (it != d_data->fileIcons.end())
		return it.value();

	// Convert icon to pixmap to avoid "Warning: SHGetFileInfo() timed out for..."
	QIcon ic = d_data->provider.icon(info).pixmap(1);
	if (!ic.isNull())
		return const_cast<QMap<QString, QIcon>&>(d_data->fileIcons)[suffix] = ic;

	if (!info.exists())
	{
		// create the file icon if it does  not exist
		QTemporaryDir dir;
		if (dir.isValid()) {
			QFile file(dir.path() + "/" + info.fileName());
			if (file.open(QFile::WriteOnly)) {
				file.close();
				QIcon new_icon = d_data->provider.icon(QFileInfo(file.fileName())).pixmap(1);
				//if (!new_icon.isNull()) {
					new_icon = QIcon(new_icon.pixmap(30, 30));
					const_cast<QMap<QString, QIcon>&>(d_data->fileIcons)[suffix] = new_icon;
					return new_icon;
				//}
			}
		}
	}
	return const_cast<QMap<QString, QIcon>&>(d_data->fileIcons)[suffix] = ic;
}

QIcon VipFileSystem::iconPath(const VipPath& path) const
{
	// Do NOT use QFileIconProvider if a network issue was detected (mounted network drive that cannot be reconnected)
	// as it causes lots of troubles and freezes
	if (VipPhysicalFileSystem::has_network_issues())
		return QIcon();
	return m_provider.iconPath(path);
}

#ifdef _WIN32

QIcon VipPSFTPFileSystem::iconPath(const VipPath& path) const
{
	// Do NOT use QFileIconProvider if a network issue was detected (mounted network drive that cannot be reconnected)
	// as it causes lots of troubles and freezes

	if (path.isDir())
		return m_provider.provider()->icon(QFileIconProvider::Folder);
	else {
		return m_provider.iconPath(path);
		/*QString tmp = vipGetTempDirectory();
		if (!tmp.endsWith("/"))
			tmp += "/";

		QString suffix = QFileInfo(path.canonicalPath()).suffix();
		QString fname = tmp + "unused." + suffix;
		if (!QFileInfo(fname).exists()) {
			QFile out(fname);
			out.open(QFile::WriteOnly);
		}
		return m_provider.iconPath(VipPath(fname));*/
	}
}

class PSFTPFileSystemEditor : public VipMapFileSystemEditor
{
public:
	QLineEdit* edit;
	QLineEdit* password;
	PSFTPFileSystemEditor()
	  : VipMapFileSystemEditor()
	{
		edit = new QLineEdit();
		edit->setPlaceholderText("Remote address: 'user@address'");
		password = new QLineEdit();
		password->setPlaceholderText("User password");
		password->setEchoMode(QLineEdit::Password);
		QGridLayout* lay = new QGridLayout();
		lay->addWidget(new QLabel("Address"), 0, 0);
		lay->addWidget(edit, 0, 1);
		lay->addWidget(new QLabel("Password"), 1, 0);
		lay->addWidget(password, 1, 1);
		setLayout(lay);
	}

	virtual void updateEditor() { edit->setText(mapFileSystem()->address()); }
	virtual void apply()
	{
		mapFileSystem()->setPassword(password->text().toLatin1());
		mapFileSystem()->open(edit->text().toLatin1());
	}
};

class PSFTPFileManager : public VipFileSystemManager
{
public:
	virtual const char* className() const { return "VipPSFTPFileSystem"; }
	virtual QString name() const { return "SFTP connection"; }
	virtual VipMapFileSystemEditor* edit(VipMapFileSystem*) const { return new PSFTPFileSystemEditor(); }
	virtual VipMapFileSystem* create() const { return new VipPSFTPFileSystem(); };
};

static int register_psftp()
{
	// Check for psftp process
	{
		QProcess pr;
		// pr.start("psftp -h");
		pr.start("psftp", QStringList() << "-h");
		bool ok = pr.waitForStarted();
		pr.waitForFinished();
		if (!ok)
			return 0;
	}
	VipFileSystemManager::registerManager(new PSFTPFileManager());
	return 0;
}
static int _register_psftp = register_psftp();

#endif //_WIN32

static QList<QSharedPointer<VipFileSystemManager>>& get_managers()
{
	static QList<QSharedPointer<VipFileSystemManager>> inst;
	return inst;
}

void VipFileSystemManager::registerManager(VipFileSystemManager* manager)
{
	get_managers().append(QSharedPointer<VipFileSystemManager>(manager));
}
QList<VipFileSystemManager*> VipFileSystemManager::managers()
{
	QList<VipFileSystemManager*> res;
	for (int i = 0; i < get_managers().size(); ++i)
		res.append(get_managers()[i].data());
	return res;
}

VipMapFileSystemTreeItem::VipMapFileSystemTreeItem(const VipPath& path, VipMapFileSystemTree* tree, bool fake)
  : QTreeWidgetItem()
  , m_path(path)
  , m_need_full_update(false)
  , m_need_attribute_update(false)
  , m_fake(fake)
  , m_custom_dir(false)
  , m_custom_file(false)
{
	if (!m_fake)
		tree->addItem(this);
	this->setToolTip(0, path.canonicalPath());
}

VipMapFileSystemTreeItem::~VipMapFileSystemTreeItem()
{
	if (tree() && !m_fake)
		tree()->removeItem(this);
}

void VipMapFileSystemTreeItem::setCustomFileItem(bool custom)
{
	m_custom_file = custom;
}
bool VipMapFileSystemTreeItem::customFileItem() const
{
	return m_custom_file;
}
void VipMapFileSystemTreeItem::setCustomDirItem(bool custom)
{
	m_custom_dir = custom;
}
bool VipMapFileSystemTreeItem::customDirItem() const
{
	return m_custom_dir;
}

bool VipMapFileSystemTreeItem::operator<(const QTreeWidgetItem& other) const
{
	int column = treeWidget()->sortColumn();

	Qt::SortOrder order = treeWidget()->header()->sortIndicatorOrder();
	{
		// special case when sorting by name, we must keep the 'Shortcuts' item at the top
		if (!this->parent() && this->text(0) == "Shortcuts")
			return order == Qt::AscendingOrder;
		else if (!other.parent() && other.text(0) == "Shortcuts")
			return order != Qt::AscendingOrder;
		;
	}

	if (treeWidget()->headerItem()->text(column) == "Size")
		return this->data(0, Qt::UserRole).toLongLong() < other.data(0, Qt::UserRole).toLongLong();

	QString t1 = text(column);
	bool ok1 = false;
	double v1 = t1.toDouble(&ok1);

	QString t2 = other.text(column);
	bool ok2 = false;
	double v2 = t2.toDouble(&ok1);

	if (ok1 && ok2)
		return v1 < v2;
	else
		return t1 < t2;
}

void VipMapFileSystemTreeItem::setAttributes(const QVariantMap& attrs)
{
	if (isCustom())
		return;

	m_path.setAttributes(attrs);
	// update file attributes
	setIcon(0, tree()->mapFileSystem()->iconPath(m_path));
	if (!path().isEmpty())
		setText(0, QFileInfo(m_path.canonicalPath()).fileName());
	else if (m_path.mapFileSystem())
		setText(0, m_path.mapFileSystem()->objectName());

	const QStringList std = tree()->mapFileSystem()->standardAttributes();
	QString size, date;

	this->setData(0, Qt::UserRole, 0);

	for (int i = 0; i < std.size(); ++i) {
		QVariant value = attrs[std[i]];
		if (value.userType() != 0) {
			QString text = value.toString();

			// custom behavior for size
			if (std[i] == "Size") {
				bool ok = true;
				qint64 s = text.toLongLong(&ok);
				this->setData(0, Qt::UserRole, s);
				if (ok) {
					if (s > 1000000000)
						text = QString::number(s / 1000000000.0, 'g', 4) + " GB";
					else if (s > 1000000)
						text = QString::number(s / 1000000.0, 'g', 4) + " MB";
					else if (s > 1000)
						text = QString::number(s / 1000.0, 'g', 4) + " KB";
					else
						text += " B";
				}
				size = text;
			}

			if (value.userType() == QMetaType::QDateTime)
				text = value.toDateTime().toString("yyyy/MM/dd hh:mm:ss");
			if (std[i] == "Last modified")
				date = text;
			setText(i + 1, text);
		}
	}

	if (m_path.isDir())
		this->setToolTip(0, "<b>Path: </b>" + path().canonicalPath() + "<br><b>Last modified: </b>" + date);
	else
		this->setToolTip(0, "<b>Path: </b>" + path().canonicalPath() + "<br><b>Size: </b>" + size + "<br><b>Last modified: </b>" + date);
}

VipPath VipMapFileSystemTreeItem::path() const
{
	if (!m_path.mapFileSystem() && tree())
		const_cast<VipPath&>(m_path).setMapFileSystem(tree()->mapFileSystem());

	return m_path;
}

VipMapFileSystemTree* VipMapFileSystemTreeItem::tree() const
{
	return static_cast<VipMapFileSystemTree*>(QTreeWidgetItem::treeWidget());
}

VipPathList VipMapFileSystemTreeItem::childrenPaths() const
{
	VipPathList res;
	for (int i = 0; i < childCount(); ++i) {
		VipMapFileSystemTreeItem* it = static_cast<VipMapFileSystemTreeItem*>(child(i));
		if (!it->m_fake)
			res << it->path();
	}
	return res;
}

bool VipMapFileSystemTreeItem::setChildren(const VipPathList& _children)
{
	if (isCustom())
		return false;

	QMutexLocker lock(&m_mutex);
	m_need_full_update = false;
	m_need_attribute_update = false;

	if (vipToSet(m_children) != vipToSet(_children)) {
		m_need_full_update = true;
	}
	else {
		// compare attributes
		for (int i = 0; i < _children.size(); ++i) {
			if (_children[i] != m_children[i]) {
				m_need_attribute_update = true;
				break;
			}
		}
	}
	m_children = _children;

	return m_need_full_update || m_need_attribute_update;
}

void VipMapFileSystemTreeItem::updateContent()
{
	if (isCustom()) {
		return;
	}

	VipMapFileSystemTreeItem* item = this;
	if (!item->tree())
		return;

	VipPath p = item->path();
	VipMapFileSystem* m = item->tree()->mapFileSystem().data();

	if (item->isExpanded()) {
		// update children

		if (m_need_attribute_update) {
			QMutexLocker lock(&m_mutex);
			// same children paths: just update the infos
			for (int i = 0; i < item->childCount(); ++i) {
				VipMapFileSystemTreeItem* it = static_cast<VipMapFileSystemTreeItem*>(item->child(i));
				int index = m_children.indexOf(it->path());
				if (index >= 0)
					it->setAttributes(m_children[index].attributes());
			}
			m_need_attribute_update = false;
		}
		else if (m_need_full_update) {
			// remove all children, but keep expanded the expanded ones
			QStringList expanded;
			for (int i = 0; i < item->childCount(); ++i)
				if (item->child(i)->isExpanded())
					expanded.append(static_cast<VipMapFileSystemTreeItem*>(item->child(i))->path().canonicalPath());

			while (item->childCount())
				item->removeChild(item->child(0));

			// add the new paths
			VipPathList tmp;
			int count = 0;
			{
				QMutexLocker lock(&m_mutex);
				tmp = m_children;
				count = tmp.size();
				m_need_full_update = false;
			}
			for (int i = 0; i < tmp.size(); ++i) {
				VipMapFileSystemTreeItem* child;
				if (tmp[i].isDir())
					child = new VipMapFileSystemTreeDirItem(tmp[i], item->tree());
				else
					child = new VipMapFileSystemTreeItem(tmp[i], item->tree());
				item->addChild(child);
				child->setAttributes(tmp[i].attributes());
				if (tmp[i].isDir()) // for directories, add a fake children
				{
					child->addChild(new VipMapFileSystemTreeItem(VipPath(), item->tree(), true));
					// expand
					if (expanded.indexOf(tmp[i].canonicalPath()) >= 0)
						child->setExpanded(true);
				}
			}

			if (!count)
				item->setExpanded(false);

			if (!item->childCount())
				item->addChild(new VipMapFileSystemTreeItem(VipPath(), item->tree(), true));
		}
		else {
			int count = 0;
			{
				QMutexLocker lock(&m_mutex);
				count = m_children.size();
			}
			if (!count)
				item->setExpanded(false);

			if (!item->childCount())
				item->addChild(new VipMapFileSystemTreeItem(VipPath(), item->tree(), true));
		}
	}
	else if (item->childCount() == 0 && item->path().isDir()) // for directories, add a fake children
	{
		item->addChild(new VipMapFileSystemTreeItem(VipPath(), item->tree(), true));
		// update file attributes
		item->setIcon(0, m->iconPath(item->path()));
		QString name = QFileInfo(item->path().canonicalPath()).fileName();
		if (name.isEmpty())
			name = item->path().canonicalPath();
		if (name.isEmpty())
			name = m->objectName();
		item->setText(0, name);
	}
}

VipMapFileSystemTreeDirItem::VipMapFileSystemTreeDirItem(const VipPath& path, VipMapFileSystemTree* tree)
  : QObject()
  , VipMapFileSystemTreeItem(path, tree)
{
	tree->addDirItem(this);
}
VipMapFileSystemTreeDirItem::~VipMapFileSystemTreeDirItem()
{
	if (tree())
		tree()->removeDirItem(this);
}

class VipMapFileSystemTreeUpdate : public QThread
{
public:
	VipMapFileSystemTree* tree;
	QList<QPointer<VipMapFileSystemTreeDirItem>> items;
	int sleepTime;
	std::atomic<bool> trigger;
	QMutex mutex;

	VipMapFileSystemTreeUpdate(VipMapFileSystemTree* tree)
	  : QThread()
	  , tree(tree)
	  , sleepTime(5000)
	  , trigger(false)
	{
	}

	void addItem(VipMapFileSystemTreeDirItem* item)
	{
		QMutexLocker lock(&mutex);
		items.append(QPointer<VipMapFileSystemTreeDirItem>(item));
	}

	void removeItem(VipMapFileSystemTreeDirItem* item)
	{
		if (item) {
			QMutexLocker lock(&mutex);
			items.removeOne(QPointer<VipMapFileSystemTreeDirItem>(item));
		}
	}

	void triggerUpdate() { trigger.store(true); }

	virtual void run()
	{
		bool expired = false;
		while (VipMapFileSystemTree* t = tree) {

			if (!expired || tree->isVisible()) {
				QMutexLocker lock(&mutex);
				for (int i = 0; i < items.size(); ++i) {
					if (VipMapFileSystemTreeDirItem* it = items[i]) {
						if (it->isExpanded()) {
							VipPathList lst = t->listDirContent(it->path());
							if (it->setChildren(lst)) {
								QMetaObject::invokeMethod(t, "updateDirContent", Qt::QueuedConnection, Q_ARG(QObjectPointer, QObjectPointer(it)));
							}
						}
					}
					else {
						items.removeAt(i);
						--i;
					}
				}
			}

			qint64 time = QDateTime::currentMSecsSinceEpoch();
			while (QDateTime::currentMSecsSinceEpoch() - time < sleepTime) {
				if (trigger.load(std::memory_order_relaxed) || !tree) {
					trigger.store(false);
					break;
				}
				QThread::msleep(20);
			}
			expired = (QDateTime::currentMSecsSinceEpoch() - time) >= sleepTime;
		}
	}
};

#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QMutex>
#include <QScrollBar>
#include <QTimer>

class VipMapFileSystemTree::PrivateData
{
public:
	enum Operation
	{
		Copy,
		Cut
	};

	PrivateData()
	  : map(nullptr)
	  , inside_scroll_bar(false)
	  , enableOverwrite(false)
	  , operation(Copy)
	  , operations(VipMapFileSystem::All)
	{
	}
	VipMapFileSystemPtr map;
	QStringList suffixes;
	bool inside_scroll_bar;
	bool enableOverwrite;
	VipPathList clipboard;
	Operation operation;
	QPoint press_position;
	VipMapFileSystemTree::Operations operations;
	VipMapFileSystemTreeUpdate* update;
	VipMapFileSystemTreeItem* shorcuts{ nullptr };

	// all items sortd by path
	QMultiMap<QString, VipMapFileSystemTreeItem*> items;
};

VipMapFileSystemTree::VipMapFileSystemTree(QWidget* parent)
  : QTreeWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->update = new VipMapFileSystemTreeUpdate(this);

	this->setSortingEnabled(true);
	this->sortByColumn(0, Qt::AscendingOrder);
	this->setSelectionMode(QTreeWidget::ExtendedSelection);
	this->setDragEnabled(true);
	this->setDragEnabled(true);
	this->setDragDropMode(QAbstractItemView::DragDrop);
	this->setAcceptDrops(true);

	connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(itemExpanded(QTreeWidgetItem*)));

	d_data->update->start();
}

VipMapFileSystemTree::~VipMapFileSystemTree()
{
	d_data->update->tree = nullptr;
	d_data->update->wait();
	// clear before deleting the thread
	clear();
	delete d_data->update;
}

void VipMapFileSystemTree::setMapFileSystem(VipMapFileSystemPtr map)
{
	if (map != d_data->map) {

		d_data->map = map;
		clear();

		if (d_data->map) {
			// set the headers based on the VipMapFileSystem standard attributes
			setHeaderLabels((QStringList() << "Name") + map->standardAttributes());
			this->setColumnWidth(0, 200);

			// setSupportedOperations(d_data->operations & map->supportedOperations());
			if (!(map->supportedOperations() & VipMapFileSystem::OpenWrite) || !(map->supportedOperations() & VipMapFileSystem::Rename) ||
			    !(map->supportedOperations() & VipMapFileSystem::CopyFile))
				setSupportedOperations(Operations());
		}
	}
}

VipMapFileSystemPtr VipMapFileSystemTree::mapFileSystem() const
{
	return d_data->map;
}

void VipMapFileSystemTree::setSupportedOperations(VipMapFileSystemTree::Operations op)
{
	d_data->operations = op;
}

void VipMapFileSystemTree::setSupportedOperation(Operation op, bool on)
{
	if (d_data->operations.testFlag(op) != on) {
		if (on)
			d_data->operations |= op;
		else
			d_data->operations &= ~op;
	}
}

VipMapFileSystemTree::Operations VipMapFileSystemTree::supportedOperations() const
{
	return d_data->operations;
}

bool VipMapFileSystemTree::testOperation(Operation op) const
{
	return d_data->operations.testFlag(op);
}

void VipMapFileSystemTree::setEnableOverwrite(bool enable)
{
	d_data->enableOverwrite = enable;
}

bool VipMapFileSystemTree::enableOverwrite() const
{
	return d_data->enableOverwrite;
}

VipPath VipMapFileSystemTree::pathForItem(QTreeWidgetItem* item) const
{
	return static_cast<VipMapFileSystemTreeItem*>(item)->path();
}

// QList<QTreeWidgetItem *> VipMapFileSystemTree::itemsForPath(const VipPath & path, ItemType type)
//  {
//  typedef QMultiMap<QString, VipMapFileSystemTreeItem*>::iterator iterator;
//  QPair<iterator, iterator> range = d_data->items.equal_range(path.canonicalPath());
//
//  QList<QTreeWidgetItem *> res;
//  for (iterator it = range.first; it != range.second; ++it)
//  {
//  if(type == AllItems)
//  res.append(it.value());
//  else {
//  //find top level parent
//  QTreeWidgetItem * top = it.value();
//  while (indexOfTopLevelItem(top) < 0)
//  top = top->parent();
//
//  if(type == CustomItemsOnly && static_cast<VipMapFileSystemTreeItem*>(top)->isCustom())
//  res.append(it.value());
//  else if (type == NoCustomItems && !static_cast<VipMapFileSystemTreeItem*>(top)->isCustom())
//  res.append(it.value());
//  }
//  }
//  return res;
//  }

QTreeWidgetItem* VipMapFileSystemTree::internalItemForPath(QTreeWidgetItem* root, const QStringList& sub_paths, const QString& _prefix)
{
	QString subpath;
	QTreeWidgetItem* item = root;

	QString prefix = _prefix;
	if (!prefix.endsWith("/"))
		prefix += "/";

	// if(root != invisibleRootItem())
	//  vip_debug("root: '%s'\n", pathForItem(root).canonicalPath().toLatin1().data());
	//  for (int i = 0; i < sub_paths.size(); ++i)
	//  vip_debug("%i: '%s'\n", i, sub_paths[i].toLatin1().data());

	for (int i = 0; i < sub_paths.size(); ++i) {
		if (i == 0) {
			if (sub_paths[i].isEmpty())
				subpath = "/";
			else
				subpath = sub_paths[0];
		}
		else if (subpath != "/")
			subpath += "/" + sub_paths[i];
		else
			subpath += sub_paths[i];

		QString to_compare = subpath;
		// if (!to_compare.endsWith("/"))
		//	to_compare += "/";
		if (to_compare.endsWith("/"))
			to_compare = to_compare.mid(0, to_compare.size() - 1);

		if (!prefix.isEmpty()) {
			if (to_compare.startsWith("/"))
				to_compare = prefix + to_compare.mid(1);
			else
				to_compare = prefix + to_compare;
		}

		// vip_debug("to_compare: '%s'\n", to_compare.toLatin1().data());

		QTreeWidgetItem* found = nullptr;
		for (int c = 0; c < item->childCount(); ++c) {
			VipPath p = pathForItem(item->child(c));
			// vip_debug("p: '%s'\n", p.canonicalPath().toLatin1().data());
			if (((p.canonicalPath().startsWith(to_compare) && to_compare != "/") || p.canonicalPath() == subpath || (p.canonicalPath().isEmpty() && subpath == "/")) &&
			    (p.isDir() || i == sub_paths.size() - 1)) {
				found = item->child(c);
				if (!found->isExpanded() && p.isDir())
					found->setExpanded(true);
				break;
			}
		}

		if (!found)
			return nullptr;
		item = found;
		if (i == sub_paths.size() - 1)
			return item;
	}

	return nullptr;
}

QList<QTreeWidgetItem*> VipMapFileSystemTree::itemsForPath(const VipPath& path, ItemType type)
{
	if (!d_data->map)
		return QList<QTreeWidgetItem*>();

	// Check if this is a top level custom item
	if ((type == AllItems || type == CustomItemsOnly) && !path.canonicalPath().contains("/")) {
		QList<QTreeWidgetItem*> items = this->findItems(path.filePath(), Qt::MatchCaseSensitive, 0);
		if (items.size())
			return items;
	}

	// Check if given path exists
	if (!d_data->map->exists(path) && !d_data->map->hasError())
		return QList<QTreeWidgetItem*>();

	QList<QTreeWidgetItem*> res;

	// Check in shortcuts
	if (type == AllItems || type == CustomItemsOnly) {
		for (int i = 0; i < topLevelItemCount(); ++i) {
			VipMapFileSystemTreeItem* top = static_cast<VipMapFileSystemTreeItem*>(topLevelItem(i));
			if (top->isCustom()) {
				// check for starting path in custom item children
				for (int j = 0; j < top->childCount(); ++j) {
					VipMapFileSystemTreeItem* child = static_cast<VipMapFileSystemTreeItem*>(top->child(j));
					if (path.canonicalPath().startsWith(child->path().canonicalPath())) {
						// found!
						// expand if necessary
						if (!child->isExpanded() && child->path().isDir())
							child->setExpanded(true);

						if (path.canonicalPath() == child->path().canonicalPath())
							res += child;
						else {
							if (QTreeWidgetItem* it =
							      internalItemForPath(child,
										  path.canonicalPath().mid(child->path().canonicalPath().size()).split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts),
										  child->path().canonicalPath()))
								res += it;
						}
					}
				}
			}
		}
	}

	// Check custom only
	if (type != CustomItemsOnly) {
		if (path.canonicalPath() == "/") {
			// if (QTreeWidgetItem* it = internalItemForPath(invisibleRootItem(), QStringList() <<("/")))
			//	res += it;
			for (int i = 0; i < invisibleRootItem()->childCount(); ++i)
				if (static_cast<VipMapFileSystemTreeItem*>(invisibleRootItem()->child(i))->path().canonicalPath() == "/")
					res += invisibleRootItem()->child(i);
		}
		else {

			if (QTreeWidgetItem* it = internalItemForPath(invisibleRootItem(), path.canonicalPath().split("/")))
				res += it;
		}
	}

	return res;
}

QTreeWidgetItem* VipMapFileSystemTree::addTopLevelPath(const VipPath& path)
{
	if (d_data->map) {
		if (d_data->map->exists(path) || path.isEmpty()) {
			VipMapFileSystemTreeItem* item;
			if (path.isDir())
				item = new VipMapFileSystemTreeDirItem(path, this);
			else
				item = new VipMapFileSystemTreeItem(path, this);

			this->addTopLevelItem(item);
			if (!path.isDir())
				item->setAttributes(path.attributes());
			else
				item->updateContent();
			return item;
		}
	}
	return nullptr;
}

void VipMapFileSystemTree::addTopLevelPaths(const VipPathList& paths)
{
	for (int i = 0; i < paths.size(); ++i)
		addTopLevelPath(paths[i]);
}

QList<int> VipMapFileSystemTree::columnWidths() const
{
	QList<int> res;
	for (int i = 0; i < columnCount(); ++i)
		res.append(this->columnWidth(i));
	return res;
}

void VipMapFileSystemTree::setColumnWidths(const QList<int>& widths)
{
	int count = qMin(widths.size(), columnCount());
	for (int i = 0; i < count; ++i)
		this->setColumnWidth(i, widths[i]);
}

bool VipMapFileSystemTree::move(const VipPathList& paths, const VipPath& dst_folder)
{
	if (!testOperation(Move) || !mapFileSystem())
		return false;

	if (!dst_folder.isDir())
		return false;

	if (!aboutToMove(paths, dst_folder))
		return false;

	// check that the move is possible: the list cannot contains a lower path

	vip_debug("dst: %s\n", dst_folder.canonicalPath().toLatin1().data());
	for (int i = 0; i < paths.size(); ++i)
		if (paths[i].isDir() && dst_folder.canonicalPath().startsWith(paths[i].canonicalPath())) {
			QMessageBox::warning(nullptr, "Unsupported operation", "Cannot move selected paths");
			return false;
		}
		else {
			vip_debug("src: %s\n", paths[i].canonicalPath().toLatin1().data());
		}

	VipPathList files, dirs;
	for (int i = 0; i < paths.size(); ++i) {
		if (paths[i].isDir())
			dirs << paths[i];
		else
			files << paths[i];
	}

	// perform the move by starting with the files
	VipProgress progress;
	progress.setRange(0, paths.size());
	progress.setValue(0);
	progress.setCancelable(true);
	progress.setModal(true);

	int count = 1;
	for (int i = 0; i < files.size(); ++i, ++count) {
		if (progress.canceled())
			break;
		progress.setValue(count);
		if (!mapFileSystem()->move(files[i], VipPath(dst_folder.canonicalPath() + "/" + files[i].lastPath()), enableOverwrite(), &progress)) {
			VIP_LOG_WARNING("Failed to move '" + files[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");
			return false;
		}
		else
			VIP_LOG_INFO("Move '" + files[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");
	}
	for (int i = 0; i < dirs.size(); ++i, ++count) {
		// complete the dst dir name
		if (progress.canceled())
			break;
		VipProgress pr;
		if (!mapFileSystem()->move(dirs[i], VipPath(dst_folder.canonicalPath() + "/" + dirs[i].lastPath(), true), enableOverwrite(), &pr)) {
			VIP_LOG_WARNING("Failed to move '" + dirs[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");
			return false;
		}
		else
			VIP_LOG_INFO("Move '" + dirs[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");

		progress.setValue(count);
	}

	d_data->update->triggerUpdate();
	return true;
}

bool VipMapFileSystemTree::copy(const VipPathList& paths, const VipPath& dst_folder)
{
	if (!testOperation(Copy) || !mapFileSystem())
		return false;

	if (!dst_folder.isDir())
		return false;

	if (!aboutToCopy(paths, dst_folder))
		return false;

	// check that the copy is possible: the list cannot contains a lower path

	vip_debug("dst: %s\n", dst_folder.canonicalPath().toLatin1().data());
	for (int i = 0; i < paths.size(); ++i)
		if (paths[i].isDir() && dst_folder.canonicalPath().startsWith(paths[i].canonicalPath())) {
			QMessageBox::warning(nullptr, "Unsupported operation", "Cannot copy selected paths");
			return false;
		}
		else {
			vip_debug("src: %s\n", paths[i].canonicalPath().toLatin1().data());
		}

	VipPathList files, dirs;
	for (int i = 0; i < paths.size(); ++i) {
		if (paths[i].isDir())
			dirs << paths[i];
		else
			files << paths[i];
	}

	// perform the copy by starting with the files
	VipProgress progress;
	progress.setRange(0, paths.size());
	progress.setValue(0);
	progress.setCancelable(true);
	progress.setModal(true);

	int count = 1;
	for (int i = 0; i < files.size(); ++i, ++count) {
		if (progress.canceled())
			break;
		progress.setValue(count);
		if (!mapFileSystem()->copy(files[i], VipPath(dst_folder.canonicalPath() + "/" + files[i].lastPath()), enableOverwrite(), &progress)) {
			VIP_LOG_WARNING("Failed to copy '" + files[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");
			return false;
		}
		else
			VIP_LOG_INFO("Copy '" + files[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");
	}
	for (int i = 0; i < dirs.size(); ++i, ++count) {
		// complete the dst dir name
		if (progress.canceled())
			break;
		VipProgress pr;
		if (!mapFileSystem()->copy(dirs[i], VipPath(dst_folder.canonicalPath() + "/" + dirs[i].lastPath(), true), enableOverwrite(), &pr)) {
			VIP_LOG_WARNING("Failed to copy '" + dirs[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");
			return false;
		}
		else
			VIP_LOG_INFO("Copy '" + dirs[i].canonicalPath() + "' to '" + dst_folder.canonicalPath() + "'");

		progress.setValue(count);
	}

	d_data->update->triggerUpdate();
	return true;
}

void VipMapFileSystemTree::removeSelection()
{
	auto lst = this->selectedItems();
	for (int i = 0; i < lst.size(); ++i) {
	
		auto* parent = lst[i]->parent();
		if (invisibleRootItem()->indexOfChild(parent) >= 0) {
			//parent is a top level item
			
			if (!static_cast<VipMapFileSystemTreeItem*>(parent)->path().canonicalPath().contains("/")) {
				// This is a custom item
				delete parent->takeChild(parent->indexOfChild(lst[i]));
			}
		}

	}
}

bool VipMapFileSystemTree::remove(const VipPathList& _paths)
{
	VipPathList paths = _paths;

	// we can safely remove items whose parent is custom
	for (int i = 0; i < paths.size(); ++i) {
		QList<QTreeWidgetItem*> items = this->itemsForPath(paths[i], CustomItemsOnly);
		for (int j = 0; j < items.size(); ++j) {
			if (QTreeWidgetItem* p = items[j]->parent()) {
				VipMapFileSystemTreeItem* pi = static_cast<VipMapFileSystemTreeItem*>(p);
				if (pi->isCustom()) {
					// remove this item
					removeItem(items[j]);
					p->removeChild(items[j]);
					--j;
				}
			}
		}
	}

	if (!testOperation(Delete) || !mapFileSystem())
		return false;

	if (!aboutToRemove(paths))
		return false;

	VipPathList files, dirs;
	for (int i = 0; i < paths.size(); ++i) {
		if (paths[i].isDir())
			dirs << paths[i];
		else
			files << paths[i];
	}

	VipProgress progress;
	progress.setRange(0, paths.size());
	progress.setValue(0);
	progress.setCancelable(true);
	progress.setModal(true);

	int count = 1;
	for (int i = 0; i < files.size(); ++i, ++count) {
		if (progress.canceled())
			break;
		progress.setValue(count);
		if (!mapFileSystem()->remove(files[i])) {
			VIP_LOG_WARNING("Failed to remove '" + files[i].canonicalPath() + "'");
			return false;
		}
		else
			VIP_LOG_INFO("Remove '" + files[i].canonicalPath() + "'");
	}
	for (int i = 0; i < dirs.size(); ++i, ++count) {
		// complete the dst dir name
		if (progress.canceled())
			break;
		VipProgress pr;
		if (!mapFileSystem()->remove(dirs[i])) {
			VIP_LOG_WARNING("Failed to remove '" + dirs[i].canonicalPath() + "'");
			return false;
		}
		else
			VIP_LOG_INFO("Remove '" + dirs[i].canonicalPath() + "'");

		progress.setValue(count);
	}
	d_data->update->triggerUpdate();
	return true;
}

bool VipMapFileSystemTree::copy(const VipPathList& paths)
{
	if (!testOperation(Copy) || !mapFileSystem())
		return false;

	d_data->clipboard = paths;
	d_data->operation = PrivateData::Copy;
	return true;
}

bool VipMapFileSystemTree::cut(const VipPathList& paths)
{
	if (!testOperation(Move) || !mapFileSystem())
		return false;

	d_data->clipboard = paths;
	d_data->operation = PrivateData::Cut;
	return true;
}

bool VipMapFileSystemTree::paste(const VipPath& dst_folder)
{
	if (!mapFileSystem())
		return false;

	if (d_data->clipboard.size()) {
		if (d_data->operation == PrivateData::Copy)
			return copy(d_data->clipboard, dst_folder);
		else {
			VipPathList tmp = d_data->clipboard;
			d_data->clipboard.clear();
			return move(tmp, dst_folder);
		}
	}
	return true;
}

bool VipMapFileSystemTree::copySelection()
{
	return copy(selectedPaths());
}

bool VipMapFileSystemTree::cutSelection()
{
	return cut(selectedPaths());
}

bool VipMapFileSystemTree::pasteSelection()
{
	VipPathList lst = selectedPaths();
	if (lst.size() == 1) {
		VipPath dst = lst.first();
		if (!dst.isDir())
			dst = dst.parent();
		return paste(dst);
	}
	return false;
}

void VipMapFileSystemTree::addSelectionToShortcuts()
{
	addToShortcuts(selectedPaths());
}

void VipMapFileSystemTree::unselectAll()
{
	unselectHelper(this->invisibleRootItem());
}

void VipMapFileSystemTree::unselectHelper(QTreeWidgetItem* item)
{
	item->setSelected(false);
	for (int i = 0; i < item->childCount(); ++i)
		unselectHelper(item->child(i));
}

void VipMapFileSystemTree::copySelectedPathToClipboard()
{
	VipPathList lst = selectedPaths();
	if (lst.size() == 1) {
		QApplication::clipboard()->setText(lst.first().canonicalPath());
	}
}

void VipMapFileSystemTree::mouseReleaseEvent(QMouseEvent* evt)
{
	QTreeWidget::mouseReleaseEvent(evt);
	if (evt->button() == Qt::RightButton)
		rightClick();
}

void VipMapFileSystemTree::mousePressEvent(QMouseEvent* evt)
{
	QScrollBar* v_slider = this->verticalScrollBar();
	QScrollBar* h_slider = this->horizontalScrollBar();
	d_data->inside_scroll_bar = (v_slider->isVisible() && QRectF(QPoint(0, 0), v_slider->size()).contains(v_slider->mapFromGlobal(QCursor::pos()))) ||
				    (h_slider->isVisible() && QRectF(QPoint(0, 0), h_slider->size()).contains(h_slider->mapFromGlobal(QCursor::pos())));

	d_data->press_position = evt->pos();
	QTreeWidget::mousePressEvent(evt);
}

void VipMapFileSystemTree::mouseMoveEvent(QMouseEvent* evt)
{
	if (d_data->inside_scroll_bar) {
		QTreeWidget::mouseMoveEvent(evt);
		return;
	}

	if (!mapFileSystem())
		return;

	if ((evt->pos() - d_data->press_position).manhattanLength() < 5) {
		QTreeWidget::mouseMoveEvent(evt);
		return;
	}

	// if (!mapFileSystem() || !(mapFileSystem()->supportedOperations() & VipFileSystem::Rename))
	//  return;

	// if (!d_data->eventFromViewport)
	//  {
	//  QTreeWidget::mouseMoveEvent(evt);
	//  return;
	//  }

	if (evt->buttons() & Qt::LeftButton) {
		QList<QTreeWidgetItem*> items = this->selectedItems();

		// check if on item is a top level item
		for (int i = 0; i < items.size(); ++i) {
			QTreeWidgetItem* it = items[i];
			while (indexOfTopLevelItem(it) < 0)
				it = it->parent();
			if (it == items[i]) {
				// Cannot move top level item
				QTreeWidget::mouseMoveEvent(evt);
				return;
			}
		}

		// check weather on of the top level parent is a custom dir
		VipMapFileSystemTreeDirItem* top = nullptr;
		for (int i = 0; i < items.size(); ++i) {
			QTreeWidgetItem* it = items[i];
			while (indexOfTopLevelItem(it) < 0)
				it = it->parent();
			if (static_cast<VipMapFileSystemTreeItem*>(it)->customDirItem()) {
				top = static_cast<VipMapFileSystemTreeDirItem*>(it);
				break;
			}
		}

		VipPathList lst = selectedPaths();
		for (int i = 0; i < lst.size(); ++i)
			vip_debug("%s\n", lst[i].canonicalPath().toLatin1().data());
		vip_debug("\n");
		if (!lst.isEmpty()) {
			QDrag drag(this);
			VipMimeDataMapFile* mime = new VipMimeDataMapFile();
			mime->setPaths(lst);
			mime->setData("application/x-qabstractitemmodeldatalist", QByteArray());
			mime->setData("VipMimeDataMapFile", QByteArray());
			if (top)
				mime->setProperty("TopCustom", QVariant::fromValue(top));
			drag.setMimeData(mime);
			drag.exec();

			d_data->update->triggerUpdate();
		}
	}
}

void VipMapFileSystemTree::dragEnterEvent(QDragEnterEvent* event)
{
	const QMimeData* mime = event->mimeData();
	if (mime->hasUrls()) {
		event->acceptProposedAction();
	}
	else
		QTreeWidget::dragEnterEvent(event);
}

void VipMapFileSystemTree::dragMoveEvent(QDragMoveEvent* event)
{
	const QMimeData* mime = event->mimeData();
	if (mime->hasUrls()) {
		event->acceptProposedAction();
	}
	else
		QTreeWidget::dragMoveEvent(event);
}

VipMapFileSystemTreeItem* VipMapFileSystemTree::shortcutsItem() const
{
	if (d_data->shorcuts)
		return d_data->shorcuts;
	for (int i = 0; i < topLevelItemCount(); ++i) {
		auto* it = topLevelItem(i);
		if (it->text(0) == "Shortcuts")
			return const_cast<PrivateData*>(d_data.get())->shorcuts = static_cast<VipMapFileSystemTreeItem*>(it);
	}
	return nullptr;
}

void VipMapFileSystemTree::addToShortcuts(const VipPathList& lst)
{
	VipMapFileSystemTreeItem* shortcuts = shortcutsItem();
	if (!shortcuts)
		return;

	VipPathList already_there = shortcuts->childrenPaths();

	for (int i = 0; i < lst.size(); ++i) {
		vip_debug("lst: '%s'\n", lst[i].canonicalPath().toLatin1().data());

		if (already_there.indexOf(lst[i]) >= 0)
			continue;

		VipMapFileSystemTreeItem* child = lst[i].isDir() ? new VipMapFileSystemTreeDirItem(lst[i], this) : new VipMapFileSystemTreeItem(lst[i], this);
		child->setFlags(child->flags() & (~Qt::ItemIsDragEnabled));
		shortcuts->addChild(child);

		if (!lst[i].isDir())
			child->setAttributes(lst[i].attributes());
		else
			child->updateContent();
	}
}

void VipMapFileSystemTree::dropEvent(QDropEvent* evt)
{
	VipPathList lst;
	if (evt->mimeData()->hasFormat("VipMimeDataMapFile"))
		lst = static_cast<const VipMimeDataMapFile*>(evt->mimeData())->paths();
	else if (evt->mimeData()->hasUrls()) {
		QTreeWidgetItem* it = this->itemAt(evt->pos());
		QTreeWidgetItem* top = it;
		while (indexOfTopLevelItem(top) < 0)
			top = top->parent();

		if (it->text(0) == "Shortcuts" && top == it) {
			// add paths to shortcuts
			VipPathList already_there = static_cast<VipMapFileSystemTreeItem*>(top)->childrenPaths();
			QList<QUrl> urls = evt->mimeData()->urls();
			VipPathList _lst;
			for (int i = 0; i < urls.size(); ++i) {
				QString path = urls[i].toString();
				path.replace("\\", "/");
				path.remove("file:///");
				path = QFileInfo(path).canonicalFilePath();
				_lst.append(VipPath(path, QFileInfo(path).isDir()));
			}

			for (int i = 0; i < _lst.size(); ++i) {
				vip_debug("_lst: '%s'\n", _lst[i].canonicalPath().toLatin1().data());

				if (already_there.indexOf(_lst[i]) >= 0)
					continue;

				VipMapFileSystemTreeItem* child = _lst[i].isDir() ? new VipMapFileSystemTreeDirItem(_lst[i], this) : new VipMapFileSystemTreeItem(_lst[i], this);
				child->setFlags(child->flags() & (~Qt::ItemIsDragEnabled));
				top->addChild(child);

				if (!_lst[i].isDir())
					child->setAttributes(_lst[i].attributes());
				else
					child->updateContent();
			}
			return;
		}

		// highlight the dropped file or directory
		QString path = evt->mimeData()->urls().last().toString();
		path.replace("\\", "/");
		path.remove("file:///");
		setPathExpanded(VipPath(path), true);
		QList<QTreeWidgetItem*> items = itemsForPath(VipPath(path), NoCustomItems);
		if (items.size()) {
			items.first()->setSelected(true);
			scrollToItem(items.first());
		}
		return;
	}

	QTreeWidgetItem* dst = this->itemAt(evt->pos());
	evt->accept();

	// get the top level item
	// vip_debug("dst: '%s'\n", static_cast<VipMapFileSystemTreeItem*>(dst)->path().canonicalPath().toLatin1().data());
	QTreeWidgetItem* top = dst;
	while (indexOfTopLevelItem(top) < 0)
		top = top->parent();

	// For now, only accept drops in the 'Shortcuts' item
	if (top != dst || top->text(0) != "Shortcuts")
		return;

	VipPathList already_there = static_cast<VipMapFileSystemTreeItem*>(top)->childrenPaths();

	// check if we are dropping in a custom item
	if (static_cast<VipMapFileSystemTreeItem*>(top)->isCustom()) {

		VipMapFileSystemTreeDirItem* src_top_dir = evt->mimeData()->property("TopCustom").value<VipMapFileSystemTreeDirItem*>();
		// do NOT accept drag & drop from->to the same custom item
		if (src_top_dir == top && top->text(0) != "Shortcuts")
			return;

		// just add the paths in the custom item;
		for (int i = 0; i < lst.size(); ++i) {
			// vip_debug("lst: '%s'\n", lst[i].canonicalPath().toLatin1().data());

			if (already_there.indexOf(lst[i]) >= 0)
				continue;

			VipMapFileSystemTreeItem* child = lst[i].isDir() ? new VipMapFileSystemTreeDirItem(lst[i], this) : new VipMapFileSystemTreeItem(lst[i], this);
			child->setFlags(child->flags() & (~Qt::ItemIsDragEnabled));
			top->addChild(child);

			if (!lst[i].isDir())
				child->setAttributes(lst[i].attributes());
			else
				child->updateContent();
		}
	}

	if (lst.size() && dst) {
		VipMapFileSystemTreeItem* item = static_cast<VipMapFileSystemTreeItem*>(dst);
		VipPath path = item->path();
		if (!item->path().isDir())
			path = path.parent();

		move(lst, path);
	}
	else if (!dst && lst.size() && (d_data->operations & DropTopLevel)) {
		this->addTopLevelPaths(lst);
	}
}

void VipMapFileSystemTree::keyPressEvent(QKeyEvent* evt)
{
	evt->ignore();
	if (evt->key() == Qt::Key_Delete) {
		//remove(selectedPaths());
		removeSelection();
		evt->accept();
	}
	else if (evt->key() == Qt::Key_X && (evt->modifiers() & Qt::CTRL)) {
		cut(selectedPaths());
		evt->accept();
	}
	else if (evt->key() == Qt::Key_C && (evt->modifiers() & Qt::CTRL)) {
		copy(selectedPaths());
		evt->accept();
	}
	else if (evt->key() == Qt::Key_V && (evt->modifiers() & Qt::CTRL)) {
		VipPathList lst = selectedPaths();
		if (lst.size() == 0) {
			QMessageBox::warning(nullptr, "Paste files", "No destination folder selected");
			return;
		}
		if (lst.size() > 1) {
			QMessageBox::warning(nullptr, "Paste files", "Unauthorized operation");
			return;
		}

		VipPath dst = lst.first();
		if (!dst.isDir())
			dst = dst.parent();

		paste(dst);
		evt->accept();
	}
	else if (evt->key() == Qt::Key_Enter || evt->key() == Qt::Key_Return) {
		auto lst = selectedItems();
		VipPathList to_open;
		for (int i = 0; i < lst.size(); ++i) {
			VipMapFileSystemTreeItem* it = static_cast<VipMapFileSystemTreeItem*>(lst[i]);
			if (it->path().isDir())
				it->setExpanded(true);
			else
				to_open.push_back(it->path());
		}
		vipGetMainWindow()->openPaths(to_open,nullptr);
		evt->accept();
	}
}

bool VipMapFileSystemTree::aboutToCopy(const VipPathList&, const VipPath&)
{
	return true;
}

bool VipMapFileSystemTree::aboutToMove(const VipPathList&, const VipPath&)
{
	return QMessageBox::question(nullptr, "Move selection", "Do you want to move selection ?", QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

bool VipMapFileSystemTree::aboutToRemove(const VipPathList&)
{
	return QMessageBox::question(nullptr, "Delete selection", "Do you want to remove selection ?", QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

bool VipMapFileSystemTree::rightClick()
{
	QMenu menu;
	VipPathList lst = selectedPaths();

	if (lst.size() == 1) {
		connect(menu.addAction("Copy full path to clipboard"), SIGNAL(triggered(bool)), this, SLOT(copySelectedPathToClipboard()));
		menu.addSeparator();
	}

	connect(menu.addAction(vipIcon("copy.png"), "Copy selection"), SIGNAL(triggered(bool)), this, SLOT(copySelection()));

	if (testOperation(Move))
		connect(menu.addAction(vipIcon("cut.png"), "Cut selection"), SIGNAL(triggered(bool)), this, SLOT(cutSelection()));

	if (lst.size() == 1 && d_data->clipboard.size()) {
		menu.addSeparator();
		connect(menu.addAction(vipIcon("paste.png"), "Paste"), SIGNAL(triggered(bool)), this, SLOT(pasteSelection()));
	}

	menu.addSeparator();
	connect(menu.addAction("Add to shortcuts"), SIGNAL(triggered(bool)), this, SLOT(addSelectionToShortcuts()));

	menu.exec(QCursor::pos());
	return true;
}

void VipMapFileSystemTree::setVisibleSuffixes(const QStringList& suffixes)
{
	VipPathList expanded = this->expandedPaths(NoCustomItems);
	VipPathList custom_expanded = this->expandedPaths(CustomItemsOnly);

	int pos = verticalScrollBar()->value();

	d_data->suffixes = suffixes;
	for (int i = 0; i < topLevelItemCount(); ++i)
		static_cast<VipMapFileSystemTreeItem*>(topLevelItem(i))->updateContent();

	this->setExpandedPaths(expanded, NoCustomItems);
	this->setExpandedPaths(custom_expanded, CustomItemsOnly);

	verticalScrollBar()->setValue(pos);
}

QStringList VipMapFileSystemTree::visibleSuffixes() const
{
	return d_data->suffixes;
}

void VipMapFileSystemTree::setRefreshTimeout(int msecs)
{
	d_data->update->sleepTime = msecs;
}

int VipMapFileSystemTree::refreshTimeout() const
{
	return d_data->update->sleepTime;
}

void VipMapFileSystemTree::setRefreshEnabled(bool enable)
{
	if (enable && !refreshEnabled()) {
		d_data->update->tree = this;
		d_data->update->start();
	}
	else if (!enable && refreshEnabled()) {
		d_data->update->tree = nullptr;
		d_data->update->wait();
	}
}
bool VipMapFileSystemTree::refreshEnabled() const
{
	return d_data->update->isRunning();
}

VipPathList VipMapFileSystemTree::selectedPaths(ItemType type) const
{
	QList<QTreeWidgetItem*> items = this->selectedItems();
	VipPathList res;
	for (int i = 0; i < items.size(); ++i) {
		VipMapFileSystemTreeItem* it = static_cast<VipMapFileSystemTreeItem*>(items[i]);
		if (type == AllItems)
			res << it->path();
		else {
			// get top level parent
			QTreeWidgetItem* top = it;
			while (indexOfTopLevelItem(top) < 0)
				top = top->parent();

			VipMapFileSystemTreeItem* _top = static_cast<VipMapFileSystemTreeItem*>(top);
			if (type == CustomItemsOnly && _top->isCustom())
				res << it->path();
			else if (type == NoCustomItems && !_top->isCustom())
				res << it->path();
		}
	}
	return res;
}

void VipMapFileSystemTree::setSelectedPaths(const VipPathList& lst, ItemType type)
{
	for (int i = 0; i < lst.size(); ++i) {
		QList<QTreeWidgetItem*> items = itemsForPath(lst[i], type);
		for (int j = 0; j < items.size(); ++j)
			items[j]->setSelected(true);
	}
}

void VipMapFileSystemTree::setPathExpanded(const VipPath& path, bool expanded, ItemType type)
{
	QList<QTreeWidgetItem*> found = itemsForPath(path, type);
	for (int it = 0; it != found.size(); ++it) {
		VipMapFileSystemTreeItem* item = static_cast<VipMapFileSystemTreeItem*>(found[it]);
		if (item) {
			if (!expanded) {
				if (item->path().isDir())
					item->setExpanded(false);
				else if (QTreeWidgetItem* p = item->parent())
					p->setExpanded(false);
			}
			else {
				// build the list of path from root -> item
				VipPathList items;

				QTreeWidgetItem* to_expand = item;
				while (to_expand) {
					items.append(static_cast<VipMapFileSystemTreeItem*>(to_expand)->path());
					to_expand = to_expand->parent();
				}

				// expand from root to item
				for (int i = items.size() - 1; i >= 0; --i) {
					QList<QTreeWidgetItem*> found_items = itemsForPath(items[i], type);
					for (int j = 0; j < found_items.size(); ++j)
						found_items[j]->setExpanded(true);
				}
			}
		}
	}
}

void VipMapFileSystemTree::setExpandedPaths(const VipPathList& path, ItemType type)
{
	unexpandAll(this->invisibleRootItem(), type);

	for (int i = 0; i < path.size(); ++i) {
		setPathExpanded(path[i], true, type);
	}
}

VipPathList VipMapFileSystemTree::expandedPaths(ItemType type) const
{
	VipPathList res;

	for (int i = 0; i < topLevelItemCount(); ++i) {
		VipMapFileSystemTreeItem* top = static_cast<VipMapFileSystemTreeItem*>(topLevelItem(i));
		if ((type == AllItems) || (type == CustomItemsOnly && top->isCustom()) || (type == NoCustomItems && !top->isCustom())) {
			VipPathList tmp;
			expandedHelper(static_cast<VipMapFileSystemTreeItem*>(topLevelItem(i)), tmp);
			res += tmp;
		}
	}

	return res;
}

VipPathList VipMapFileSystemTree::topLevelPaths() const
{
	VipPathList res;
	for (int i = 0; i < topLevelItemCount(); ++i)
		res << pathForItem(topLevelItem(i));
	return res;
}

void VipMapFileSystemTree::expandedHelper(QTreeWidgetItem* it, VipPathList& lst) const
{
	VipMapFileSystemTreeItem* item = static_cast<VipMapFileSystemTreeItem*>(it);
	if (item->path().isDir() && item->isExpanded()) {
		int index = lst.indexOf(item->path().parent());
		if (index >= 0)
			lst.removeAt(index);
		lst.append(item->path());

		for (int i = 0; i < item->childCount(); ++i)
			if (item->child(i)->isExpanded()) {
				expandedHelper(static_cast<VipMapFileSystemTreeItem*>(item->child(i)), lst);
			}
	}
}

void VipMapFileSystemTree::updateDirContent(const QObjectPointer& ptr)
{
	if (ptr) {
		VipMapFileSystemTreeDirItem* item = static_cast<VipMapFileSystemTreeDirItem*>(ptr.data());
		item->updateContent();
	}
}

void VipMapFileSystemTree::addDirItem(QTreeWidgetItem* item)
{
	d_data->update->addItem(static_cast<VipMapFileSystemTreeDirItem*>(item));
}

void VipMapFileSystemTree::removeDirItem(QTreeWidgetItem* item)
{
	d_data->update->removeItem(static_cast<VipMapFileSystemTreeDirItem*>(item));
}

void VipMapFileSystemTree::addItem(QTreeWidgetItem* item)
{
	VipMapFileSystemTreeDirItem* it = static_cast<VipMapFileSystemTreeDirItem*>(item);
	d_data->items.insert(it->path().canonicalPath(), it);
}

void VipMapFileSystemTree::removeItem(QTreeWidgetItem* item)
{
	VipMapFileSystemTreeDirItem* it = static_cast<VipMapFileSystemTreeDirItem*>(item);
	QMultiMap<QString, VipMapFileSystemTreeItem*>::iterator iter = d_data->items.find(it->path().canonicalPath());
	for (; iter != d_data->items.end(); ++iter) {
		if (iter.value() == it) {
			d_data->items.erase(iter);
			return;
		}
	}
}

void VipMapFileSystemTree::unexpandAll(QTreeWidgetItem* item, ItemType type)
{
	if (item != invisibleRootItem()) {
		VipMapFileSystemTreeItem* it = static_cast<VipMapFileSystemTreeItem*>(item);
		if (type == NoCustomItems && it->isCustom())
			return;
		else if (type == CustomItemsOnly && !it->isCustom())
			return;
	}
	item->setExpanded(false);
	for (int i = 0; i < item->childCount(); ++i)
		unexpandAll(item->child(i), type);
}

VipPathList VipMapFileSystemTree::listDirContent(const VipPath& path)
{
	return filterSuffixes(mapFileSystem()->list(path));
}

VipPathList VipMapFileSystemTree::filterSuffixes(const VipPathList& paths) const
{
	// filter with suffixes
	const QStringList suffixes = visibleSuffixes();
	if (suffixes.isEmpty())
		return paths;
	else {
		VipPathList res;
		for (int i = 0; i < paths.size(); ++i)
			if (suffixes.contains(QFileInfo(paths[i].canonicalPath()).suffix(), Qt::CaseInsensitive) || paths[i].isDir())
				res.append(paths[i]);
		return res;
	}
}

void VipMapFileSystemTree::itemExpanded(QTreeWidgetItem* item)
{
	VipMapFileSystemTreeItem* it = static_cast<VipMapFileSystemTreeItem*>(item);

	if (!it->isCustom()) {
		VipPathList children = listDirContent(it->path());
		it->setChildren(VipPathList());
		it->setChildren(children);
		it->updateContent();
	}
	else {
		// custom paths, we need to handle filters manually
		const QStringList suffixes = visibleSuffixes();

		for (int i = 0; i < it->childCount(); ++i) {
			VipMapFileSystemTreeItem* child = static_cast<VipMapFileSystemTreeItem*>(it->child(i));
			// if (suffixes.isEmpty())
			//	child->setHidden(false);
			// else
			{
				child->setHidden(!(suffixes.contains(QFileInfo(child->path().canonicalPath()).suffix(), Qt::CaseInsensitive) || child->path().isDir()));
			}
		}
	}
}

#include <qboxlayout.h>
#include <qlineedit.h>
#include <qsplitter.h>
#include <qtoolbar.h>

#include "VipDisplayArea.h"
#include "VipIODevice.h"

QStringList fileFilters()
{
	QStringList filters;
	if (filters.isEmpty()) {

		filters << "Session file (*.session)";

		filters += VipIODevice::possibleReadFilters(QString(), QByteArray());
		// create the All files filter
		QString all_filters;
		for (int i = 0; i < filters.size(); ++i) {
			int index1 = filters[i].indexOf('(');
			int index2 = filters[i].indexOf(')', index1);
			if (index1 >= 0 && index2 >= 0) {
				all_filters += filters[i].mid(index1 + 1, index2 - index1 - 1) + " ";
			}
		}

		if (all_filters.size())
			filters.prepend("All files (" + all_filters + ")");
	}
	return filters;
}

QStringList suffixesFromFilter(const QString& filter)
{
	// construct suffixes
	int index_start = filter.indexOf("(");
	int end_index = filter.lastIndexOf(")");
	if (index_start >= 0 && end_index >= 0) {
		QString str = filter.mid(index_start + 1, end_index - index_start - 1);
		str.replace(" ", "");
		str.replace(".", "");
		return (str.split("*", VIP_SKIP_BEHAVIOR::SkipEmptyParts));
	}
	return QStringList();
}

/// @brief Session parameters
struct PendingFileSystemSession
{
	QByteArray splitterState;
	VipPathList shortcuts;
	VipPathList shortcutsExpanded;
	VipPathList normalExpanded;
	int v_scrollbar;
	VipPathList shortcutsSelection;
	VipPathList normalSelection;
	QList<int> header_sizes;
};

Q_DECLARE_METATYPE(PendingFileSystemSession)

static void applyPendingFileSystemSession(VipFileSystemWidget* w, const PendingFileSystemSession& session)
{
	w->splitter()->restoreState(session.splitterState);

	// make unique
	VipPathList shortcuts = vipToSet(session.shortcuts).values();

	// clear previous shorcuts
	QTreeWidgetItem* top = w->tree()->property("_vip_shortcuts").value<VipMapFileSystemTreeDirItem*>();
	while (top->childCount())
		delete top->takeChild(0);

	for (int i = 0; i < shortcuts.size(); ++i) {
		VipMapFileSystemTreeItem* child = shortcuts[i].isDir() ? new VipMapFileSystemTreeDirItem(shortcuts[i], w->tree()) : new VipMapFileSystemTreeItem(shortcuts[i], w->tree());
		child->setFlags(child->flags() & (~Qt::ItemIsDragEnabled));
		top->addChild(child);

		if (!shortcuts[i].isDir())
			child->setAttributes(shortcuts[i].attributes());
		else
			child->updateContent();
	}

	if (top->childCount() == 0) {
		// If the 'Shortcuts' is empty, at least add the home path, as it can be quite hard
		// to find it on a massively multi-user unix environment.
		VipMapFileSystemTreeDirItem* home = new VipMapFileSystemTreeDirItem(VipPath(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first(), true), w->tree());
		home->setFlags(home->flags() & (~Qt::ItemIsDragEnabled));
		top->addChild(home);
		home->updateContent();
	}

	VipPathList shortcutsExpanded = session.shortcutsExpanded;
	VipPathList normalExpanded = session.normalExpanded;
	w->tree()->setExpandedPaths(shortcutsExpanded, VipMapFileSystemTree::CustomItemsOnly);
	w->tree()->setExpandedPaths(normalExpanded, VipMapFileSystemTree::NoCustomItems);
	w->tree()->verticalScrollBar()->setValue(session.v_scrollbar);

	VipPathList shortcutsSelection = session.shortcutsSelection;
	VipPathList normalSelection = session.normalSelection;
	// w->tree()->setSelectedPaths(shortcutsSelection, VipMapFileSystemTree::CustomItemsOnly);
	// w->tree()->setSelectedPaths(normalSelection, VipMapFileSystemTree::NoCustomItems);
	w->tree()->setColumnWidths(session.header_sizes);
}

class VipFileSystemWidget::PrivateData
{
public:
	PrivateData() {}
	VipMapFileSystemTree* tree;
	VipMapFileSystemTree* searchResults;
	QLabel* searchPath;
	QLineEdit* searchPattern;
	QLineEdit* currentPath;
	QToolButton* create;
	QToolButton* filterButton;
	QToolBar* toolBar;
	QMenu* menuFilter;
	QAction* openSelectedFiles;
	QAction* openSelectedDirs;
	QAction* stopSearch;
	QAction* createButtonAction;
	QStringList possibleFilters;
	QString filter;
	QString searchDir;
	QSplitter* splitter;

	// password management
	QLineEdit* password;
};

VipFileSystemWidget::VipFileSystemWidget(QWidget*)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->tree = new VipMapFileSystemTree();
	d_data->searchResults = new VipMapFileSystemTree();
	// d_data->shortcuts = new VipMapFileSystemTree();
	d_data->searchPath = new QLabel();
	d_data->searchPattern = new QLineEdit();
	// d_data->selectionPath = new QLineEdit();
	d_data->filterButton = new QToolButton();
	d_data->toolBar = new QToolBar();
	d_data->splitter = new QSplitter(Qt::Vertical);

	d_data->toolBar->setIconSize(QSize(18, 18));

	d_data->searchPattern->setPlaceholderText("Search pattern");
	// d_data->selectionPath->setPlaceholderText("Selection address");
	d_data->searchPattern->setToolTip("Search files and directories in selected directory");
	d_data->searchPath->setWordWrap(true);
	d_data->filterButton->setAutoRaise(true);
	d_data->filterButton->setText("Filters");
	d_data->filterButton->setToolTip("File filters");
	d_data->menuFilter = new QMenu(d_data->filterButton);
	d_data->filterButton->setMenu(d_data->menuFilter);
	d_data->filterButton->setPopupMode(QToolButton::InstantPopup);

	d_data->currentPath = new QLineEdit();
	d_data->currentPath->setPlaceholderText("Enter a valid path");
	d_data->currentPath->setToolTip("Enter a valid path and press ENTER");

	QToolButton* create = new QToolButton();
	create->setAutoRaise(true);
	create->setToolTip("Connect to an existing file system");
	create->setIcon(vipIcon("new.png"));
	create->setMenu(new QMenu());
	create->setPopupMode(QToolButton::InstantPopup);
	d_data->createButtonAction = d_data->toolBar->addWidget(d_data->create = create);
	d_data->createButtonAction->setVisible(VipFileSystemManager::managers().size() > 0);
	if (d_data->createButtonAction->isVisible())
		d_data->toolBar->addSeparator();
	connect(create->menu(), SIGNAL(aboutToShow()), this, SLOT(showMenuCreate()));
	connect(create->menu(), SIGNAL(triggered(QAction*)), this, SLOT(createFileSystemRequeted(QAction*)));

	d_data->openSelectedFiles = d_data->toolBar->addAction(vipIcon("open_file.png"), "Open selected files");
	d_data->openSelectedDirs = d_data->toolBar->addAction(vipIcon("open_dir.png"), "Open selected directories");
	d_data->toolBar->addWidget(d_data->filterButton);
	d_data->toolBar->addWidget(d_data->searchPattern);
	d_data->stopSearch = d_data->toolBar->addAction(vipIcon("cancel.png"), "Stop search");

	d_data->searchResults->hide();
	d_data->searchResults->setRefreshEnabled(false);
	d_data->stopSearch->setVisible(false);

	// d_data->showShortcut = new QToolButton();
	//  d_data->showShortcut->setAutoRaise(true);
	//  d_data->showShortcut->setToolTip("Show/Hide shortcuts");
	//  d_data->showShortcut->setIcon(vipIcon("shortcuts.png"));
	//  d_data->showShortcut->setCheckable(true);
	//
	// d_data->shortcuts->hide();
	// d_data->shortcuts->setSupportedOperations(VipMapFileSystemTree::Copy | VipMapFileSystemTree::Move | VipMapFileSystemTree::Delete | VipMapFileSystemTree::DropTopLevel);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->setSpacing(2);
	vlay->addWidget(d_data->toolBar);
	vlay->addWidget(d_data->currentPath);

	vlay->addWidget(d_data->tree);
	vlay->addWidget(d_data->searchPath);
	QWidget* w = new QWidget();
	w->setLayout(vlay);

	d_data->splitter->addWidget(w);
	d_data->splitter->addWidget(d_data->searchResults);
	// d_data->splitter->addWidget(d_data->shortcuts);

	d_data->password = new QLineEdit();
	d_data->password->setEchoMode(QLineEdit::Password);
	d_data->password->setPlaceholderText("Enter password");

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(5, 5, 5, 5);
	lay->addWidget(d_data->password);
	d_data->password->hide();
	lay->addWidget(d_data->splitter);
	setLayout(lay);

	// connections
	connect(d_data->menuFilter, SIGNAL(aboutToShow()), this, SLOT(aboutToOpenFilters()));
	connect(d_data->menuFilter, SIGNAL(triggered(QAction*)), this, SLOT(filterSelected(QAction*)));
	connect(d_data->stopSearch, SIGNAL(triggered(bool)), this, SLOT(stopSearch()));
	connect(d_data->searchPattern, SIGNAL(textChanged(const QString&)), this, SLOT(startSearch()));
	connect(d_data->searchPattern, SIGNAL(returnPressed()), this, SLOT(startSearch()));
	connect(d_data->openSelectedFiles, SIGNAL(triggered(bool)), this, SLOT(openSelectedFiles()));
	connect(d_data->openSelectedDirs, SIGNAL(triggered(bool)), this, SLOT(openSelectedDirs()));
	connect(d_data->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(openSelectedFiles()));
	// connect(d_data->shortcuts, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(openSelectedFiles()));
	connect(d_data->searchResults, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(openSelectedFiles()));
	// connect(d_data->showShortcut, SIGNAL(clicked(bool)), this, SLOT(setShortcutsVisible(bool)));
	// connect(d_data->selectionPath, SIGNAL(textChanged(const QString &)), this, SLOT(setSelectedPath()));
	// connect(d_data->selectionPath, SIGNAL(returnPressed()), this, SLOT(setSelectedPath()));

	// connect(d_data->tree, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateSelectedPath(QTreeWidgetItem *, int)));
	// connect(d_data->searchResults, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateSelectedPath(QTreeWidgetItem *, int)));
	// connect(d_data->shortcuts, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateSelectedPath(QTreeWidgetItem *, int)));

	// selected an item in a VipMapFileSystemTree (tree or searchResults) will clear the selection in the other one
	connect(d_data->tree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), d_data->searchResults, SLOT(unselectAll()));
	connect(d_data->tree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), d_data->searchResults, SLOT(unselectAll()));
	connect(d_data->tree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(updateDisplayPath()));
	connect(d_data->currentPath, SIGNAL(returnPressed()), this, SLOT(fromDisplayPath()));
	// connect(d_data->tree, SIGNAL(itemClicked(QTreeWidgetItem *, int)), d_data->shortcuts, SLOT(unselectAll()));
	connect(d_data->searchResults, SIGNAL(itemClicked(QTreeWidgetItem*, int)), d_data->tree, SLOT(unselectAll()));
	// connect(d_data->searchResults, SIGNAL(itemClicked(QTreeWidgetItem *, int)), d_data->shortcuts, SLOT(unselectAll()));
	// connect(d_data->shortcuts, SIGNAL(itemClicked(QTreeWidgetItem *, int)), d_data->tree, SLOT(unselectAll()));
	// connect(d_data->shortcuts, SIGNAL(itemClicked(QTreeWidgetItem *, int)), d_data->searchResults, SLOT(unselectAll()));

	connect(d_data->password, SIGNAL(returnPressed()), this, SLOT(passwordEntered()));
}

VipFileSystemWidget::~VipFileSystemWidget()
{
	stopSearch();
}

void VipFileSystemWidget::showMenuCreate()
{
	QList<VipFileSystemManager*> ms = VipFileSystemManager::managers();
	d_data->create->menu()->clear();
	for (int i = 0; i < ms.size(); ++i) {
		d_data->create->menu()->addAction(ms[i]->name());
	}
}
void VipFileSystemWidget::createFileSystemRequeted(QAction* act)
{
	QList<VipFileSystemManager*> ms = VipFileSystemManager::managers();
	for (int i = 0; i < ms.size(); ++i) {
		if (ms[i]->name() == act->text()) {
			Q_EMIT createFileSystem(ms[i]->name());
			break;
		}
	}
}

VipMapFileSystemTree* VipFileSystemWidget::tree() const
{
	return d_data->tree;
}
VipMapFileSystemTree* VipFileSystemWidget::searchResults() const
{
	return d_data->searchResults;
}
// VipMapFileSystemTree  *VipFileSystemWidget::shortcuts() const
//  {
//  return d_data->shortcuts;
//  }
QSplitter* VipFileSystemWidget::splitter() const
{
	return d_data->splitter;
}
QLabel* VipFileSystemWidget::searchPath() const
{
	return d_data->searchPath;
}
QLineEdit* VipFileSystemWidget::searchPattern() const
{
	return d_data->searchPattern;
}
// QLineEdit *VipFileSystemWidget::selectionPath() const
//  {
//  return d_data->selectionPath;
//  }
QToolButton* VipFileSystemWidget::filterButton() const
{
	return d_data->filterButton;
}
QAction* VipFileSystemWidget::openSelectedFilesAction() const
{
	return d_data->openSelectedFiles;
}
QAction* VipFileSystemWidget::openSelectedDirsAction() const
{
	return d_data->openSelectedDirs;
}
QAction* VipFileSystemWidget::createButtonAction() const
{
	return d_data->createButtonAction;
}
const QStringList& VipFileSystemWidget::possibleFilters() const
{
	return d_data->possibleFilters;
}
const QString& VipFileSystemWidget::filter() const
{
	return d_data->filter;
}

void VipFileSystemWidget::updateDisplayPath()
{
	QList<QTreeWidgetItem*> items = d_data->tree->selectedItems();
	QString res;
	if (items.size())
		res = static_cast<VipMapFileSystemTreeItem*>(items.back())->path().canonicalPath();
	d_data->currentPath->setText(res);
}

void VipFileSystemWidget::fromDisplayPath()
{
	QString tmp = d_data->currentPath->text();
	if (!tmp.isEmpty())
		d_data->tree->setPathExpanded(VipPath(tmp), true);

	QList<QTreeWidgetItem*> found = d_data->tree->itemsForPath(VipPath(tmp));
	for (QTreeWidgetItem* it : found) {
		it->setSelected(true);
		d_data->tree->scrollToItem(it);
	}
}

void VipFileSystemWidget::startSearch()
{
	if (d_data->searchPattern->text().isEmpty()) {
		stopSearch();
		d_data->searchResults->hide();
		d_data->searchResults->clear();
		return;
	}

	if (d_data->tree->mapFileSystem()) {
		d_data->tree->mapFileSystem()->stopSearch();
		VipPathList lst = d_data->tree->selectedPaths();
		QSet<QString> dirs;
		for (int i = 0; i < lst.size(); ++i) {
			if (lst[i].isDir())
				dirs.insert(lst[i].canonicalPath());
			else
				dirs.insert(lst[i].parent().canonicalPath());
		}

		if (dirs.size() > 1) {
			d_data->searchPath->setText("Cannot search on multiple directories");
			d_data->searchPath->setStyleSheet("color = red;");
		}
		else if (dirs.size() == 0) {
			d_data->searchPath->setText("Search: no selected directory");
			d_data->searchPath->setStyleSheet("color = red;");
		}
		else if (dirs.size() == 1) {
			d_data->searchPath->setStyleSheet("color = black;");
			d_data->searchDir = *dirs.begin();

			d_data->searchResults->hide();
			d_data->searchResults->clear();
			d_data->stopSearch->setVisible(true);

			QString search = d_data->searchPattern->text();
			search.replace("\t", " ");
			QStringList _lst = search.split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
			QList<QRegExp> exps;
			for (int i = 0; i < _lst.size(); ++i) {
				exps.append(QRegExp(_lst[i]));
				exps.last().setPatternSyntax(QRegExp::Wildcard);
				exps.last().setCaseSensitivity(Qt::CaseInsensitive);
			}

			d_data->tree->mapFileSystem()->search(VipPath(d_data->searchDir, true), exps, false, QDir::AllEntries);
		}
	}
}

void VipFileSystemWidget::stopSearch()
{
	if (d_data->tree->mapFileSystem())
		d_data->tree->mapFileSystem()->stopSearch();

	d_data->stopSearch->setVisible(false);
}

void VipFileSystemWidget::setWaitForPassword()
{
	if (this->mapFileSystem()->isOpen())
		return;
	d_data->password->show();
	d_data->splitter->hide();
	static_cast<QBoxLayout*>(layout())->addStretch(1);
}

void VipFileSystemWidget::passwordEntered()
{
	VipMapFileSystemPtr sys = this->mapFileSystem();

	sys->setPassword(d_data->password->text().toLatin1());
	if (!sys->open(sys->address())) {
		d_data->password->setStyleSheet("QLineEdit{border: 1px solid red;}");
	}
	else {
		d_data->password->setStyleSheet("");
		d_data->password->hide();
		d_data->splitter->show();
		layout()->removeItem(layout()->itemAt(layout()->count() - 1));

		// add again the map file system
		this->setMapFileSystem(sys, true);

		QVariant session = property("_vip_PendingFileSystemSession");
		if (!session.isNull()) {
			applyPendingFileSystemSession(this, session.value<PendingFileSystemSession>());
		}
	}
}

void VipFileSystemWidget::aboutToOpenFilters()
{
	if (d_data->possibleFilters.isEmpty()) {
		QStringList filters = fileFilters();
		setPossibleFilters(filters);
		setFilter(filters.first());
	}
}

void VipFileSystemWidget::updateFilters()
{
	QStringList lst = fileFilters();
	setPossibleFilters(lst);
	setFilter((lst.first()));
}

void VipFileSystemWidget::setPossibleFilters(const QStringList& filters)
{
	d_data->possibleFilters = filters;
	d_data->menuFilter->clear();
	// d_data->menuFilter->addAction("No filter");
	for (int i = 0; i < filters.size(); ++i) {
		d_data->menuFilter->addAction(filters[i]);
	}
}

bool VipFileSystemWidget::setFilter(const QString& filter)
{
	d_data->filter = filter;
	d_data->filterButton->setText(filter);

	// if (filter == "No filter")
	//  {
	//  d_data->tree->setVisibleSuffixes(QStringList());
	//  //d_data->shortcuts->setVisibleSuffixes(QStringList());
	//  return true;
	//  }

	QStringList suffixes = suffixesFromFilter(filter);
	d_data->tree->setVisibleSuffixes(suffixes);
	QString text = filter;
	if (text.size() > 15)
		text = text.mid(0, 12) + "...";
	d_data->filterButton->setText(text);
	d_data->filterButton->setToolTip("File filters<br><b>Current filter</b>:" + filter);
	return true;
}

void VipFileSystemWidget::setMapFileSystem(VipMapFileSystemPtr map, bool append_root_paths)
{
	if (d_data->tree->mapFileSystem()) {
		disconnect(d_data->tree->mapFileSystem().data(), SIGNAL(found(const VipPath&)), this, SLOT(found(const VipPath&)));
		disconnect(d_data->tree->mapFileSystem().data(), SIGNAL(searchEnterPath(const VipPath&)), this, SLOT(searchEnterPath(const VipPath&)));
		disconnect(d_data->tree->mapFileSystem().data(), SIGNAL(searchStarted()), this, SLOT(searchStarted()));
		disconnect(d_data->tree->mapFileSystem().data(), SIGNAL(searchEnded()), this, SLOT(searchEnded()));
	}

	d_data->tree->setMapFileSystem(map);
	d_data->searchResults->setMapFileSystem(map);
	// d_data->shortcuts->setMapFileSystem(map);
	if (d_data->tree->mapFileSystem()) {
		connect(d_data->tree->mapFileSystem().data(), SIGNAL(found(const VipPath&)), this, SLOT(found(const VipPath&)), Qt::QueuedConnection);
		connect(d_data->tree->mapFileSystem().data(), SIGNAL(searchEnterPath(const VipPath&)), this, SLOT(searchEnterPath(const VipPath&)), Qt::QueuedConnection);
		connect(d_data->tree->mapFileSystem().data(), SIGNAL(searchStarted()), this, SLOT(searchStarted()), Qt::QueuedConnection);
		connect(d_data->tree->mapFileSystem().data(), SIGNAL(searchEnded()), this, SLOT(searchEnded()), Qt::QueuedConnection);

		if (append_root_paths) {
			d_data->tree->addTopLevelPaths(map->roots());
		}
	}
}

VipMapFileSystemPtr VipFileSystemWidget::mapFileSystem() const
{
	return d_data->tree->mapFileSystem();
}

void VipFileSystemWidget::setSupportedOperations(VipMapFileSystemTree::Operations op)
{
	d_data->tree->setSupportedOperations(op);
	// d_data->shortcuts->setSupportedOperations(op);
	d_data->searchResults->setSupportedOperations(op);
}

void VipFileSystemWidget::setSupportedOperation(VipMapFileSystemTree::Operation op, bool enable)
{
	d_data->tree->setSupportedOperation(op, enable);
	// d_data->shortcuts->setSupportedOperation(op, enable);
	d_data->searchResults->setSupportedOperation(op, enable);
}

VipMapFileSystemTree::Operations VipFileSystemWidget::supportedOperations() const
{
	return d_data->tree->supportedOperations();
}

bool VipFileSystemWidget::testOperation(VipMapFileSystemTree::Operation op) const
{
	return d_data->tree->testOperation(op);
}
// bool VipFileSystemWidget::shortcutsVisible() const
// {
// return d_data->shortcuts->isVisible();
// }

void VipFileSystemWidget::filterSelected(QAction* act)
{
	QString filter = act->text();
	setFilter(filter);
}

void VipFileSystemWidget::searchEnterPath(const VipPath& path)
{
	d_data->searchPath->setText(path.canonicalPath().remove(d_data->searchDir));
}

void VipFileSystemWidget::found(const VipPath& path)
{
	d_data->searchResults->show();
	d_data->searchResults->addTopLevelPath(path);
}

void VipFileSystemWidget::searchEnded()
{
	d_data->searchPath->setText("End of search");
	d_data->stopSearch->setVisible(false);
}

void VipFileSystemWidget::searchStarted()
{
	d_data->searchPath->setText("Search started...");
	d_data->stopSearch->setVisible(true);
}

// void VipFileSystemWidget::updateSelectedPath(QTreeWidgetItem * item, int)
//  {
//  if (item)
//  {
//  d_data->selectionPath->blockSignals(true);
//  VipPath path = d_data->tree->pathForItem(item);
//  d_data->selectionPath->setText(path.canonicalPath());
//  d_data->selectionPath->blockSignals(false);
//  }
//  }

void VipFileSystemWidget::openSelectedFiles()
{
	VipPathList lst = d_data->tree->selectedPaths() + d_data->searchResults->selectedPaths() //+ d_data->shortcuts->selectedPaths()
	  ;
	VipPathList files;
	for (int i = 0; i < lst.size(); ++i)
		if (!lst[i].isDir())
			files.append(lst[i]);

	vipGetMainWindow()->openPaths(files, nullptr);
}

void VipFileSystemWidget::openSelectedDirs()
{
	VipPathList lst = d_data->tree->selectedPaths() + d_data->searchResults->selectedPaths() //+ d_data->shortcuts->selectedPaths()
	  ;
	VipPathList dirs;
	for (int i = 0; i < lst.size(); ++i)
		if (lst[i].isDir())
			dirs.append(lst[i]);

	vipGetMainWindow()->openPaths(dirs, nullptr);
}

// void VipFileSystemWidget::setSelectedPath()
//  {
//  QString path = d_data->selectionPath->text();
//  d_data->tree->setPathExpanded(VipPath(path), true);
//  if (QTreeWidgetItem * item = d_data->tree->itemForPath(VipPath(path)))
//  {
//  item->setSelected(true);
//  d_data->tree->scrollToItem(item);
//  }
//  }
//
//  void VipFileSystemWidget::setShortcutsVisible(bool vis)
//  {
//  d_data->shortcuts->setVisible(vis);
//  d_data->showShortcut->blockSignals(true);
//  d_data->showShortcut->setChecked(vis);
//  d_data->showShortcut->blockSignals(false);
//  }

class TabWidget : public QTabWidget
{
public:
	TabWidget()
	  : QTabWidget()
	{
	}

protected:
	virtual void tabInserted(int) { tabBar()->setVisible(count() > 1); }
	virtual void tabRemoved(int) { tabBar()->setVisible(count() > 1); }
};



VipDirectoryBrowser::VipDirectoryBrowser(VipMainWindow* win)
  : VipToolWidget(win)
{
	this->setWindowTitle("File system browser");
	this->setObjectName("File system browser");
	this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	m_widget = new TabWidget();
	m_widget->setTabsClosable(true);
	connect(m_widget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

	this->setWidget(m_widget);

	VipFileSystemWidget* local_fs = addFileSystem(new VipFileSystem());
	local_fs->setSupportedOperations(VipMapFileSystemTree::None);

	m_timer = new QTimer();
	m_timer->setSingleShot(false);
	m_timer->setInterval(500);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(checkAvailableFileSystems()));
	m_timer->start();

#ifdef WIN32
	// Add shortcut for SFTP connection
	VipShortcutsHelper::registerShorcut("Connect to SFTP host...", [this]() { this->createFileSystem("SFTP connection"); });
#endif
}

VipDirectoryBrowser::~VipDirectoryBrowser()
{
	m_timer->stop();
	disconnect(m_timer, SIGNAL(timeout()), this, SLOT(checkAvailableFileSystems()));
	delete m_timer;
}

void VipDirectoryBrowser::checkAvailableFileSystems()
{
	bool has_filesystems = VipFileSystemManager::managers().size() > 0;
	for (VipFileSystemWidget* w : m_filesystems) {
		w->createButtonAction()->setVisible(has_filesystems);
	}
}

VipFileSystemWidget* VipDirectoryBrowser::currentFileSystemWidget() const
{
	return static_cast<VipFileSystemWidget*>(m_widget->currentWidget());
}

QList<VipFileSystemWidget*> VipDirectoryBrowser::fileSystemWidgets() const
{
	return m_filesystems;
}

QString VipDirectoryBrowser::fileSystemName(VipMapFileSystem* map) const
{
	if (!map)
		return QString();
	if (!map->objectName().isEmpty())
		return map->objectName();
	QList<VipFileSystemManager*> ms = VipFileSystemManager::managers();
	for (int i = 0; i < ms.size(); ++i) {
		if (ms[i]->className() == QString(map->metaObject()->className()))
			return ms[i]->name();
	}
	return QString();
}

void VipDirectoryBrowser::closeTab(int index)
{
	VipFileSystemWidget* w = static_cast<VipFileSystemWidget*>(m_widget->widget(index));
	if (w == m_filesystems.first())
		return;

	m_filesystems.removeOne(w);
	w->deleteLater();
}

void VipDirectoryBrowser::clear()
{
	for (int i = 1; i < m_filesystems.size(); ++i) {
		delete m_filesystems[i];
	}
	m_filesystems = m_filesystems.mid(0, 1);
}

VipFileSystemWidget* VipDirectoryBrowser::addFileSystem(VipMapFileSystem* m)
{
	for (int i = 0; i < m_filesystems.size(); ++i)
		if (m_filesystems[i]->mapFileSystem() == m)
			return nullptr;

	VipFileSystemWidget* w = new VipFileSystemWidget();
	w->setMapFileSystem(VipMapFileSystemPtr(m), true);

	m_filesystems.append(w);
	// open the file system if not already done
	if (!w->mapFileSystem()->isOpen())
		w->mapFileSystem()->open(w->mapFileSystem()->address());

	// add tab
	QString address = w->mapFileSystem()->address();
	if (address.isEmpty())
		address = "Local FS";
	else if (address.size() > 16)
		address = address.mid(0, 13) + "...";

	int index = m_widget->addTab(w, address);
	m_widget->setCurrentIndex(index);

	// vip_debug("add shortcut item, %i items\n",w->tree()->topLevelItemCount());
	// create shortcut item
	VipMapFileSystemTreeDirItem* shortcuts = new VipMapFileSystemTreeDirItem(VipPath("Shortcuts", true), w->tree());
	shortcuts->setCustomDirItem(true);
	shortcuts->setText(0, "Shortcuts");
	shortcuts->setToolTip(0, "User shortcuts");
	shortcuts->setIcon(0, vipIcon("shortcuts.png"));
	w->tree()->insertTopLevelItem(0, shortcuts);
	w->tree()->setProperty("_vip_shortcuts", QVariant::fromValue(shortcuts));

	connect(w, SIGNAL(createFileSystem(const QString&)), this, SLOT(createFileSystem(const QString&)));

	return w;
}

void VipDirectoryBrowser::createFileSystem(const QString& name)
{
	QList<VipFileSystemManager*> ms = VipFileSystemManager::managers();
	for (int i = 0; i < ms.size(); ++i) {
		if (ms[i]->name() == name) {
			VipMapFileSystem* sys = ms[i]->create();
			VipMapFileSystemEditor* editor = ms[i]->edit(sys);
			editor->setMapFileSystem(sys);
			if (editor) {
				VipGenericDialog dialog(editor, "Edit " + fileSystemName(sys));
				if (dialog.exec() == QDialog::Accepted)
					editor->apply();
				else {
					delete sys;
					return;
				}
			}

			if (sys->isOpen())
				this->addFileSystem(sys);
			else
				delete sys;

			break;
		}
	}
}

/* void VipDirectoryBrowser::editCurrentFileSystem()
{
	VipFileSystemWidget * w = currentFileSystemWidget();
	if (w)
	{
		QList<VipFileSystemManager*> ms = VipFileSystemManager::managers();
		for (int i = 0; i < ms.size(); ++i)
		{
			if (ms[i]->className() == QString(w->mapFileSystem()->metaObject()->className()))
			{
				VipMapFileSystemEditor * editor = ms[i]->edit(w->mapFileSystem().data());
				editor->setMapFileSystem(w->mapFileSystem().data());
				if (editor)
				{
					VipGenericDialog dialog(editor, "Edit " + fileSystemName(w->mapFileSystem().data()));
					if (dialog.exec() == QDialog::Accepted)
						editor->apply();
				}
				break;
			}
		}
	}
}
void VipDirectoryBrowser::removeCurrentFileSystem()
{
	removeFileSystemWidget(currentFileSystemWidget());
}*/

VipDirectoryBrowser* vipGetDirectoryBrowser(VipMainWindow* widget)
{
	static VipDirectoryBrowser* inst = new VipDirectoryBrowser(widget);
	return inst;
}

#include "VipArchive.h"

static VipArchive& operator<<(VipArchive& arch, const VipDirectoryBrowser* b)
{
	QList<VipFileSystemWidget*> sys = b->fileSystemWidgets();
	arch.content("count", sys.size());
	for (int i = 0; i < sys.size(); ++i) {
		arch.start("file_system");
		arch.content("map_file_system", sys[i]->mapFileSystem().data());
		arch.content("state", sys[i]->splitter()->saveState());

		VipPathList shortcuts = sys[i]->tree()->property("_vip_shortcuts").value<VipMapFileSystemTreeDirItem*>()->childrenPaths();

		arch.content("shortcuts", shortcuts);
		arch.content("shortcutsExpanded", sys[i]->tree()->expandedPaths(VipMapFileSystemTree::CustomItemsOnly));
		arch.content("normalExpanded", sys[i]->tree()->expandedPaths(VipMapFileSystemTree::NoCustomItems));
		arch.content("v_scrollbar", sys[i]->tree()->verticalScrollBar()->value());
		arch.content("shortcutsSelection", sys[i]->tree()->selectedPaths(VipMapFileSystemTree::CustomItemsOnly));
		arch.content("normalSelection", sys[i]->tree()->selectedPaths(VipMapFileSystemTree::CustomItemsOnly));
		arch.content("header_sizes", sys[i]->tree()->columnWidths());
		arch.end();
	}

	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipDirectoryBrowser* b)
{
	// FIX, but how can this be null?
	if (!b)
		return arch;

	b->clear();
	int count = arch.read("count").toInt();
	if (!arch)
		return arch;

	for (int i = 0; i < count; ++i) {
		if (!arch.start("file_system"))
			return arch;

		// create the VipMapFileSystem, open it and add it to the VipDirectoryBrowser
		VipMapFileSystem* sys = nullptr;
		VipFileSystemWidget* w = nullptr;

		if (i > 0) {
			sys = arch.read("map_file_system").value<VipMapFileSystem*>();
			if (!sys) {
				arch.end();
				continue;
			}
			w = b->addFileSystem(sys);
			if (!w) {
				delete sys;
				arch.end();
				continue;
			}
		}
		else {
			w = b->fileSystemWidgets()[0];
			sys = w->mapFileSystem().data();
		}

		PendingFileSystemSession session;
		session.splitterState = arch.read("state").toByteArray();
		session.shortcuts = arch.read("shortcuts").value<VipPathList>();
		session.shortcutsExpanded = arch.read("shortcutsExpanded").value<VipPathList>();
		session.normalExpanded = arch.read("normalExpanded").value<VipPathList>();
		session.v_scrollbar = arch.read("v_scrollbar").toInt();
		session.shortcutsSelection = arch.read("shortcutsSelection").value<VipPathList>();
		session.normalSelection = arch.read("normalSelection").value<VipPathList>();
		session.header_sizes = (arch.read("header_sizes").value<QList<int>>());

		arch.end();

		if (w->mapFileSystem()->requirePassword()) {
			qRegisterMetaType<PendingFileSystemSession>("PendingFileSystemSession");
			w->setProperty("_vip_PendingFileSystemSession", QVariant::fromValue(session));
			w->setWaitForPassword();
		}
		else
			applyPendingFileSystemSession(w, session);

		/*w->splitter()->restoreState(arch.read("state").toByteArray());

		//read shortcuts
		VipPathList shortcuts = arch.read("shortcuts").value<VipPathList>();
		//make unique
		shortcuts = QSet<VipPath>::fromList(shortcuts).toList();

		//clear previous shorcuts
		QTreeWidgetItem * top = w->tree()->property("_vip_shortcuts").value<VipMapFileSystemTreeDirItem*>();
		while (top->childCount())
			delete top->takeChild(0);

		for (int i = 0; i < shortcuts.size(); ++i)
		{
			VipMapFileSystemTreeItem * child = shortcuts[i].isDir() ?
				new VipMapFileSystemTreeDirItem(shortcuts[i], w->tree()) :
				new VipMapFileSystemTreeItem(shortcuts[i], w->tree());
			child->setFlags(child->flags() & (~Qt::ItemIsDragEnabled));
			top->addChild(child);

			if (!shortcuts[i].isDir()) child->setAttributes(shortcuts[i].attributes());
			else child->updateContent();
		}

		if (top->childCount() == 0) {
			//If the 'Shortcuts' is empty, at least add the home path, as it can be quite hard
			//to find it on a massively multi-user unix environment.
			VipMapFileSystemTreeDirItem * home =
				new VipMapFileSystemTreeDirItem(VipPath(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first(), true), w->tree());
			home->setFlags(home->flags() & (~Qt::ItemIsDragEnabled));
			top->addChild(home);
			home->updateContent();
		}

		VipPathList shortcutsExpanded = arch.read("shortcutsExpanded").value<VipPathList>();
		VipPathList normalExpanded = arch.read("normalExpanded").value<VipPathList>();
		w->tree()->setExpandedPaths(shortcutsExpanded, VipMapFileSystemTree::CustomItemsOnly);
		w->tree()->setExpandedPaths(normalExpanded, VipMapFileSystemTree::NoCustomItems);
		w->tree()->verticalScrollBar()->setValue(arch.read("v_scrollbar").toInt());

		VipPathList shortcutsSelection = arch.read("shortcutsSelection").value<VipPathList>();
		VipPathList normalSelection = arch.read("normalSelection").value<VipPathList>();
		//w->tree()->setSelectedPaths(shortcutsSelection, VipMapFileSystemTree::CustomItemsOnly);
		//w->tree()->setSelectedPaths(normalSelection, VipMapFileSystemTree::NoCustomItems);
		w->tree()->setColumnWidths(arch.read("header_sizes").value<QList<int> >());

		*/
	}

	return arch;
}

static int registerVipDirectoryBrowser()
{
	vipRegisterArchiveStreamOperators<VipDirectoryBrowser*>();
	return 0;
}

static int _registerVipDirectoryBrowser = vipAddInitializationFunction(registerVipDirectoryBrowser);
