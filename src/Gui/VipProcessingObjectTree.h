#ifndef PROCESSING_OBJECT_TREE_H
#define PROCESSING_OBJECT_TREE_H

#include <QTreeWidget>
#include <QMimeData>

#include "VipProcessingObject.h"
#include "VipStandardWidgets.h"

/// \addtogroup Gui
/// @{

// A tree widget representing a list of VipProcessingObject classes sorted by category and names.
// The selection can be retrieved with #VipProcessingObjectTree::selectedProcessingInfos() member function.
// The items are dragable, and the resulting mime data has the category 'processing/processing-list' and the data is a string containing all selected class names separated by a new line ('\n').
class VIP_GUI_EXPORT VipProcessingObjectTree : public QTreeWidget
{
	Q_OBJECT
public:
	VipProcessingObjectTree(QWidget * parent = NULL);
	~VipProcessingObjectTree();

	void setProcessingInfos(const QList<VipProcessingObject::Info> & infos);
	const QList<VipProcessingObject::Info> & processingInfos() const;
	QList<VipProcessingObject::Info> selectedProcessingInfos() const;

protected:
	virtual void	mouseMoveEvent(QMouseEvent * event);
	virtual void	dragMoveEvent(QDragMoveEvent * event);

public Q_SLOTS:
	void resetSize();

private:
	QSize itemSizeHint(QTreeWidgetItem * item);
	class PrivateData;
	PrivateData * m_data;
};

/// Display a VipProcessingObjectTree nto a QMenu
class VipProcessingObjectTreeMenu : public VipDragMenu
{
public:
	VipProcessingObjectTreeMenu(QWidget * parent = NULL);
	VipProcessingObjectTree * processingTree() const;
};

/// A menu that displays a list of processings sorted by category.
/// Each QAction contains the property "Info" that holds a #VipProcessingObject::Info.
/// All QAction representing a processing can be accessed through #VipProcessingObjectMenu::processingActions() member function.
///
/// It possible, for each action, to disable the signal selected(const VipProcessingObject::Info &) be setting its property
/// '_vip_notrigger' to true.
class VipProcessingObjectMenu : public VipDragMenu
{
	Q_OBJECT

public:
	VipProcessingObjectMenu(QWidget * parent = NULL);
	~VipProcessingObjectMenu();

	void setProcessingInfos(const QList<VipProcessingObject::Info> & infos);
	const QList<VipProcessingObject::Info> & processingInfos() const;

	VipProcessingObject::Info selectedProcessingInfo() const;
	QList<QAction*> processingActions() const;

private Q_SLOTS:
	void selected(QAction*);
	void hover(QAction* );
Q_SIGNALS:
	void selected(const VipProcessingObject::Info & info);

private:
	class PrivateData;
	PrivateData * m_data;
};

/// @}
//end Gui

#endif
