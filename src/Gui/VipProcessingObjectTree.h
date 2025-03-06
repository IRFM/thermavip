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

#ifndef VIP_PROCESSING_OBJECT_TREE_H
#define VIP_PROCESSING_OBJECT_TREE_H

#include <QMimeData>
#include <QTreeWidget>

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
	VipProcessingObjectTree(QWidget* parent = nullptr);
	~VipProcessingObjectTree();

	void setProcessingInfos(const QList<VipProcessingObject::Info>& infos);
	const QList<VipProcessingObject::Info>& processingInfos() const;
	QList<VipProcessingObject::Info> selectedProcessingInfos() const;

protected:
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void dragMoveEvent(QDragMoveEvent* event);

public Q_SLOTS:
	void resetSize();

private:
	QSize itemSizeHint(QTreeWidgetItem* item);
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// Display a VipProcessingObjectTree into a QMenu
class VipProcessingObjectTreeMenu : public VipDragMenu
{
public:
	VipProcessingObjectTreeMenu(QWidget* parent = nullptr);
	VipProcessingObjectTree* processingTree() const;
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
	VipProcessingObjectMenu(QWidget* parent = nullptr);
	~VipProcessingObjectMenu();

	void setProcessingInfos(const QList<VipProcessingObject::Info>& infos);
	const QList<VipProcessingObject::Info>& processingInfos() const;

	VipProcessingObject::Info selectedProcessingInfo() const;
	QList<QAction*> processingActions() const;

private Q_SLOTS:
	void selected(QAction*);
	void hover(QAction*);
Q_SIGNALS:
	void selected(const VipProcessingObject::Info& info);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
// end Gui

#endif
