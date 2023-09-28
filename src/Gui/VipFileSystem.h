#ifndef VIP_FILE_SYSTEM_H
#define VIP_FILE_SYSTEM_H

#include "VipMapFileSystem.h"
#include "VipCore.h"

#include <qtreewidget.h>
#include <qmutex.h>
#include <qfileiconprovider.h>


class VIP_GUI_EXPORT VipIconProvider
{
public:
	VipIconProvider();
	~VipIconProvider();
	QFileIconProvider* provider() const;
	QIcon iconPath(const VipPath &path) const;

private:
	class PrivateData;
	PrivateData * m_data;
};


/// \a VipAbstractFileSystem that represents the physical hard drive
class VIP_GUI_EXPORT VipFileSystem : public VipPhysicalFileSystem
{
	Q_OBJECT
public:
	virtual QIcon iconPath(const VipPath &path) const;

private:
	VipIconProvider m_provider;
};

VIP_REGISTER_QOBJECT_METATYPE(VipFileSystem*)


#ifdef _WIN32
/// \a VipAbstractFileSystem that represents the physical hard drive
class VIP_GUI_EXPORT VipPSFTPFileSystem : public VipSFTPFileSystem
{
	Q_OBJECT
public:
	virtual QIcon iconPath(const VipPath& path) const;

private:
	VipIconProvider m_provider;
};

VIP_REGISTER_QOBJECT_METATYPE(VipPSFTPFileSystem*)

#endif


class VIP_GUI_EXPORT VipMapFileSystemEditor : public QWidget
{
	Q_OBJECT
	QPointer<VipMapFileSystem> m_fs;

public:
	VipMapFileSystemEditor(QWidget * parent = NULL)
		:QWidget(parent)
	{}

	void setMapFileSystem(VipMapFileSystem * fs) {
		m_fs = fs;
		updateEditor();
	}
	VipMapFileSystem * mapFileSystem() const {
		return m_fs;
	}

public Q_SLOTS :
	virtual void updateEditor() = 0;
	virtual void apply() = 0;
};
/// VipFileSystemManager provides functions for the creation and edition of VipMapFileSystem objects.
/// You should create one VipFileSystemManager class for each VipMapFileSystem type, and register it
/// with #VipFileSystemManager::registerManager().
///
/// VipFileSystemManager is used in VipFileSystemWidget to connect to distant file systems.
class VIP_GUI_EXPORT VipFileSystemManager
{
public:
	virtual ~VipFileSystemManager() {}
	///Returns the actual VipMapFileSystem derived class name that this VipFileSystemManager manages
	virtual const char * className() const = 0;
	///Returns the file system short description
	virtual QString name() const = 0;
	///Creates an editor to edit the VipMapFileSystem object
	virtual VipMapFileSystemEditor * edit(VipMapFileSystem *) const = 0;
	///Creates a new instance of the filesystem
	virtual VipMapFileSystem * create() const = 0;

	static void registerManager(VipFileSystemManager *);
	static QList<VipFileSystemManager*> managers();
};





class VipMapFileSystemTree;

/// \internal \a VipMapFileSystemTreeItem represents an item in a #VipMapFileSystemTree.
class VipMapFileSystemTreeItem : public QTreeWidgetItem
{
public:
	///Construct from a #VipPath
	VipMapFileSystemTreeItem(const VipPath & path, VipMapFileSystemTree * tree, bool fake = false);
	~VipMapFileSystemTreeItem();
	///Returns the internal #VipPath
	VipPath path() const;
	///Returns the parent #VipMapFileSystemTree
	VipMapFileSystemTree * tree() const;
	///Returns all children paths. This function do not recompute the directory's content
	/// and only use the actual QTreeWidgetItem children.
	VipPathList childrenPaths() const;

	///Update the item's content. Automatically called when the item is expanded.
	void updateContent();

	void setAttributes(const QVariantMap & attrs);
	bool setChildren(const VipPathList & children);

	void setCustomFileItem(bool);
	bool customFileItem() const;
	void setCustomDirItem(bool);
	bool customDirItem() const;
	bool isCustom() const {
		return customFileItem() || customDirItem();
	}

	virtual bool operator<(const QTreeWidgetItem &other)const;


private:
	VipPath m_path;
	VipPathList m_children;
	bool m_need_full_update;
	bool m_need_attribute_update;
	bool m_fake;
	bool m_custom_dir;
	bool m_custom_file;
	QMutex m_mutex;
};

class VipMapFileSystemTreeDirItem : public QObject, public VipMapFileSystemTreeItem
{
	Q_OBJECT
public:
	VipMapFileSystemTreeDirItem(const VipPath & path, VipMapFileSystemTree * tree);
	~VipMapFileSystemTreeDirItem();
};

/// \a VipMapFileSystemTree represents a file system based on a #VipMapFileSystem.
///
/// Calling #VipMapFileSystemTree::setMapFileSystem will clear the tree and create the right header based on VipMapFileSystem::
class VIP_GUI_EXPORT VipMapFileSystemTree : public QTreeWidget
{
	Q_OBJECT
	friend class VipMapFileSystemTreeItem;
	friend class VipMapFileSystemTreeDirItem;
	friend class VipMapFileSystemTreeUpdate;

public:
	enum Operation
	{
		None = 0x000,
		Copy = 0x001,
		Move = 0x002,
		Delete = 0x004,
		DropTopLevel = 0x008
	};
	Q_DECLARE_FLAGS(Operations, Operation);

	enum ItemType
	{
		AllItems,
		CustomItemsOnly,
		NoCustomItems
	};

	VipMapFileSystemTree(QWidget * parent = NULL);
	~VipMapFileSystemTree();

	/// Set the #VipMapFileSystem to map.
	/// This will clear the tree and create the header columns.
	void setMapFileSystem(VipMapFileSystemPtr);
	VipMapFileSystemPtr mapFileSystem() const;

	void setSupportedOperations(Operations op);
	void setSupportedOperation(Operation op, bool enable = true);
	Operations supportedOperations() const;
	bool testOperation(Operation op) const;

	void setEnableOverwrite(bool enable);
	bool enableOverwrite() const;

	/// Set the visible file suffixes
	void setVisibleSuffixes(const QStringList & suffixes);
	QStringList visibleSuffixes() const;

	void setRefreshTimeout(int msecs);
	int refreshTimeout() const;

	void setRefreshEnabled(bool enable);
	bool refreshEnabled() const;

	void setSelectedPaths(const VipPathList & lst, ItemType type = AllItems);
	VipPathList selectedPaths(ItemType type = AllItems) const;

	void setPathExpanded(const VipPath& path, bool expanded, ItemType type = AllItems);
	void setExpandedPaths(const VipPathList & path, ItemType type = AllItems);
	VipPathList expandedPaths(ItemType type = AllItems) const;
	VipPathList topLevelPaths() const;

	VipPath pathForItem(QTreeWidgetItem * item) const;
	QList<QTreeWidgetItem *> itemsForPath(const VipPath & path, ItemType type = AllItems);
	QTreeWidgetItem * addTopLevelPath(const VipPath & path);
	void addTopLevelPaths(const VipPathList & paths);

	QList<int> columnWidths() const;
	void setColumnWidths(const QList<int> &);

public Q_SLOTS:

	bool move(const VipPathList & paths, const VipPath & dst_folder);
	bool copy(const VipPathList & paths, const VipPath & dst_folder);
	bool remove(const VipPathList & paths);
	bool copy(const VipPathList & paths);
	bool cut(const VipPathList & paths);
	bool paste(const VipPath & dst_folder);

	bool copySelection();
	bool cutSelection();
	bool pasteSelection();

	void unselectAll();

protected:

	virtual void mousePressEvent(QMouseEvent * evt);
	virtual void mouseReleaseEvent(QMouseEvent * evt);
	virtual void mouseMoveEvent(QMouseEvent * evt);
	virtual void dropEvent(QDropEvent * evt);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dragMoveEvent(QDragMoveEvent *event);
	virtual void keyPressEvent(QKeyEvent * evt);

	virtual bool aboutToCopy(const VipPathList & lst, const VipPath & dst);
	virtual bool aboutToMove(const VipPathList & lst, const VipPath & dst);
	virtual bool aboutToRemove(const VipPathList & lst);
	virtual bool rightClick();

private Q_SLOTS:
	void itemExpanded(QTreeWidgetItem *);
	void updateDirContent(const QObjectPointer & ptr);
	void copySelectedPathToClipboard();

private:
	VipPathList listDirContent(const VipPath & path);
	VipPathList filterSuffixes(const VipPathList & paths) const;
	void expandedHelper(QTreeWidgetItem * item, VipPathList & lst) const;
	void unexpandAll(QTreeWidgetItem * item, ItemType type = AllItems);
	void unselectHelper(QTreeWidgetItem * item);
	QTreeWidgetItem * internalItemForPath(QTreeWidgetItem * root, const QStringList & sub_paths, const QString & prefix = QString());

	void addDirItem(QTreeWidgetItem*);
	void removeDirItem(QTreeWidgetItem*);
	void addItem(QTreeWidgetItem*);
	void removeItem(QTreeWidgetItem*);

	class PrivateData;
	PrivateData * m_data;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipMapFileSystemTree::Operations)



class QLabel;
class QLineEdit;
class QToolButton;
class QSplitter;

class VIP_GUI_EXPORT VipFileSystemWidget : public QWidget
{
	Q_OBJECT

public:
	VipFileSystemWidget(QWidget * parent = NULL);
	~VipFileSystemWidget();

	VipMapFileSystemTree  *tree() const;
	VipMapFileSystemTree  *searchResults() const;
	//VipMapFileSystemTree  *shortcuts() const;
	QSplitter * splitter() const;
	QLabel *searchPath() const;
	QLineEdit *searchPattern() const;
	//QLineEdit *selectionPath() const;
	QToolButton * filterButton() const;
	QAction * openSelectedFilesAction() const;
	QAction * openSelectedDirsAction() const;
	QAction* createButtonAction() const;
	const QStringList & possibleFilters() const;
	const QString & filter() const;

	void setMapFileSystem(VipMapFileSystemPtr map, bool append_root_paths);
	VipMapFileSystemPtr mapFileSystem() const;

	void setSupportedOperations(VipMapFileSystemTree::Operations op);
	void setSupportedOperation(VipMapFileSystemTree::Operation op, bool enable = true);
	VipMapFileSystemTree::Operations supportedOperations() const;
	bool testOperation(VipMapFileSystemTree::Operation op) const;

	//bool shortcutsVisible() const;

Q_SIGNALS:
	void createFileSystem(const QString&);

public Q_SLOTS:
	void startSearch();
	void stopSearch();
	void setWaitForPassword();

	void setPossibleFilters(const QStringList & filters);
	bool setFilter(const QString & filter);

	void openSelectedFiles();
	void openSelectedDirs();

	//void setShortcutsVisible(bool);

	// Call once at startup to define the possible file filters
	void updateFilters();

private Q_SLOTS:
	void filterSelected(QAction*);
	void searchEnterPath(const VipPath &);
	void found(const VipPath &);
	void searchEnded();
	void searchStarted();
	void passwordEntered();
	void aboutToOpenFilters();
	void updateDisplayPath();
	void fromDisplayPath();
	void showMenuCreate();
	void createFileSystemRequeted(QAction*);

private:
	class PrivateData;
	PrivateData * m_data;
};



#include "VipToolWidget.h"

class QToolBar;

class VIP_GUI_EXPORT VipDirectoryBrowser : public VipToolWidget
{
	Q_OBJECT
public:
	VipDirectoryBrowser(VipMainWindow * w = NULL);
	~VipDirectoryBrowser();

	VipFileSystemWidget * currentFileSystemWidget() const;
	QList<VipFileSystemWidget*> fileSystemWidgets() const;
	VipFileSystemWidget * addFileSystem(VipMapFileSystem *);

	void clear();

private Q_SLOTS:
	void createFileSystem(const QString &);
	void closeTab(int);
	void checkAvailableFileSystems();

private:
	QString fileSystemName(VipMapFileSystem*) const;

	QTimer *m_timer;
	QList<VipFileSystemWidget*> m_filesystems;
	QTabWidget * m_widget;
};

VIP_GUI_EXPORT VipDirectoryBrowser* vipGetDirectoryBrowser(VipMainWindow * widget= NULL);

VIP_REGISTER_QOBJECT_METATYPE(VipDirectoryBrowser*)

#endif


