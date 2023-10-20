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

#include <QDrag>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QMouseEvent>

#include "VipProcessingObject.h"
#include "VipProcessingObjectTree.h"
#include "VipTextOutput.h"

struct TreeWidgetItem : public QTreeWidgetItem
{
	VipProcessingObject::Info info;
};

static TreeWidgetItem* FromInfo(const VipProcessingObject::Info& info)
{
	if (!info.metatype)
		return nullptr;

	TreeWidgetItem* item = new TreeWidgetItem();
	item->info = info;

	QString name = vipSplitClassname(info.classname);

	item->setIcon(0, QIcon(info.icon));
	item->setText(0, name);
	item->setToolTip(0, info.description);
	// item->setToolTip(1,info.description);
	item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
	return item;
}

static TreeWidgetItem* CreateNode(const QString& name)
{
	TreeWidgetItem* item = new TreeWidgetItem();
	item->setIcon(0, vipIcon("components.png"));
	item->setText(0, name);
	QFont font;
	font.setBold(true);
	item->setFont(0, font);
	item->setFlags(item->flags() & (~Qt::ItemIsSelectable));
	return item;
}

static TreeWidgetItem* FindChild(QTreeWidgetItem* item, const QString& name)
{
	for (int i = 0; i < item->childCount(); ++i)
		if (item->child(i)->text(0) == name)
			return static_cast<TreeWidgetItem*>(item->child(i));

	return nullptr;
}

class VipProcessingObjectTree::PrivateData
{
public:
	QList<VipProcessingObject::Info> infos;
};

VipProcessingObjectTree::VipProcessingObjectTree(QWidget* parent)
  : QTreeWidget(parent)
{
	m_data = new PrivateData();

	// behavior
	this->setSelectionMode(QAbstractItemView::ExtendedSelection);
	// this->setColumnCount (2);
	// this->setHeaderLabels(QStringList()<<""<<"Name");
	this->header()->hide();
	this->setAcceptDrops(true);
	this->setFrameShape(QFrame::NoFrame);
	this->setIndentation(10);

	connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(resetSize()));
	connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), this, SLOT(resetSize()));
}

VipProcessingObjectTree::~VipProcessingObjectTree()
{
	delete m_data;
}

void VipProcessingObjectTree::setProcessingInfos(const QList<VipProcessingObject::Info>& infos)
{
	if (m_data->infos != infos) {
		this->clear();
		m_data->infos = infos;

		QTreeWidgetItem* top_level = this->invisibleRootItem();

		QMap<QString, VipProcessingObject::Info> sorted;

		for (int i = 0; i < infos.size(); ++i) {
			VipProcessingObject::Info info(infos[i]);
			QString name = vipSplitClassname(info.classname);
			if (info.classname != "ProcessingObjectPool") {
				sorted[info.category + "/" + name] = info;
			}
		}

		for (QMap<QString, VipProcessingObject::Info>::iterator it = sorted.begin(); it != sorted.end(); ++it) {
			QStringList lst = it.key().split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);

			QTreeWidgetItem* item = top_level;
			for (int i = 0; i < lst.size() - 1; ++i) {
				QTreeWidgetItem* temp = FindChild(item, lst[i]);
				if (!temp) {
					temp = CreateNode(lst[i]);
					item->addChild(temp);
					temp->setExpanded(true);
				}
				item = temp;
			}

			QTreeWidgetItem* proc = FromInfo(it.value());
			item->addChild(proc);
		}

		resetSize();
	}
}

QSize VipProcessingObjectTree::itemSizeHint(QTreeWidgetItem* item)
{
	if (!item->isHidden()) {
		int h = 0, w = 0;
		for (int i = 0; i < this->columnCount(); ++i) {
			w += this->sizeHintForColumn(i);
			h = qMax(h, this->rowHeight(indexFromItem(item, i)) + 3);
		}

		if (item->isExpanded()) {
			for (int i = 0; i < item->childCount(); ++i) {
				QSize s = itemSizeHint(item->child(i));
				h += s.height();
				w = qMax(w, s.width());
			}
		}
		return QSize(w, h);
	}
	return QSize(0, 0);
}

void VipProcessingObjectTree::resetSize()
{
	int h = 0, w = 0;
	for (int i = 0; i < topLevelItemCount(); ++i) {
		QSize tmp = itemSizeHint(this->topLevelItem(i));
		h += tmp.height();
		w = qMax(w, tmp.width());
	}

	setMinimumHeight(h);
	resize(w, h);
}

const QList<VipProcessingObject::Info>& VipProcessingObjectTree::processingInfos() const
{
	return m_data->infos;
}

QList<VipProcessingObject::Info> VipProcessingObjectTree::selectedProcessingInfos() const
{
	QList<VipProcessingObject::Info> res;
	QList<QTreeWidgetItem*> selected = selectedItems();
	for (int i = 0; i < selected.size(); ++i) {
		VipProcessingObject::Info info = static_cast<TreeWidgetItem*>(selected[i])->info;
		if (info.metatype)
			res.append(info);
	}
	return res;
}

void VipProcessingObjectTree::mouseMoveEvent(QMouseEvent* event)
{
	QTreeWidget::mouseMoveEvent(event);

	QList<QTreeWidgetItem*> selected = selectedItems();
	if (selected.size() > 0 && (event->buttons() & Qt::LeftButton)) {
		QDrag drag(this);
		QMimeData* mime = new QMimeData();
		QStringList names;
		for (int i = 0; i < selected.size(); ++i)
			names << selected[i]->text(0);
		mime->setData("processing/processing-list", names.join("\n").toLatin1());
		drag.setMimeData(mime);
		drag.exec();
	}

	QTreeWidget::mouseMoveEvent(event);
}

void VipProcessingObjectTree::dragMoveEvent(QDragMoveEvent* event)
{
	event->accept();
}

VipProcessingObjectTreeMenu::VipProcessingObjectTreeMenu(QWidget* parent)
  : VipDragMenu(parent)
{
	setWidget(new VipProcessingObjectTree());
}

VipProcessingObjectTree* VipProcessingObjectTreeMenu::processingTree() const
{
	return qobject_cast<VipProcessingObjectTree*>(this->widget());
}

class VipProcessingObjectMenu::PrivateData
{
public:
	QList<VipProcessingObject::Info> infos;
	VipProcessingObject::Info last;
	QList<QAction*> actions;
};

VipProcessingObjectMenu::VipProcessingObjectMenu(QWidget* parent)
  : VipDragMenu(parent)
{
	m_data = new PrivateData();

	this->setToolTipsVisible(true);

	connect(this, SIGNAL(triggered(QAction*)), this, SLOT(selected(QAction*)));
	connect(this, SIGNAL(hovered(QAction*)), this, SLOT(hover(QAction*)));
}

VipProcessingObjectMenu::~VipProcessingObjectMenu()
{
	delete m_data;
}

void VipProcessingObjectMenu::setProcessingInfos(const QList<VipProcessingObject::Info>& infos)
{
	this->clear();
	m_data->actions.clear();

	if (infos.isEmpty())
		return;

	QMap<QString, VipProcessingObject::Info> sorted;

	// find a common prefix (if any)
	QStringList common_prefix = infos[0].category.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	for (int i = 0; i < infos.size(); ++i) {
		QStringList lst = infos[i].category.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		int j = 0;
		int s = qMin(lst.size(), common_prefix.size());
		for (; j < s; ++j) {
			if (lst[j] != common_prefix[j])
				break;
		}
		common_prefix = common_prefix.mid(0, j);
	}
	QString prefix = common_prefix.join("/");

	for (int i = 0; i < infos.size(); ++i) {
		VipProcessingObject::Info info(infos[i]);
		QString name = vipSplitClassname(info.classname);
		if (info.classname != "VipProcessingPool") {
			QString full_name = info.category + "/" + name;
			full_name.remove(prefix);
			sorted[full_name] = info;
		}
	}

	QMap<QString, VipDragMenu*> menus;
	menus[""] = this;

	QFont f = font();
	f.setBold(true);

	for (QMap<QString, VipProcessingObject::Info>::iterator it = sorted.begin(); it != sorted.end(); ++it) {
		QString fullname = it.key();
		QStringList categories = fullname.split("/");

		VipDragMenu* menu = this;
		QString current;
		for (int i = 0; i < categories.size() - 1; ++i) {
			current += current.isEmpty() ? categories[i] : "/" + categories[i];
			QMap<QString, VipDragMenu*>::iterator found = menus.find(current);
			if (found == menus.end()) {
				VipDragMenu* prev = menu;
				menus[current] = menu = new VipDragMenu(prev);
				menu->setTitle(categories[i]);
				prev->addMenu(menu)->setFont(f);
			}
			else
				menu = found.value();
		}
		QAction* act = menu->addAction(categories.last());
		act->setProperty("Info", QVariant::fromValue(it.value()));
		act->setToolTip(it.value().description);
		act->setFont(QFont());
		m_data->actions.append(act);
	}

	m_data->infos = infos;
}

const QList<VipProcessingObject::Info>& VipProcessingObjectMenu::processingInfos() const
{
	return m_data->infos;
}

VipProcessingObject::Info VipProcessingObjectMenu::selectedProcessingInfo() const
{
	return m_data->last;
}

QList<QAction*> VipProcessingObjectMenu::processingActions() const
{
	return m_data->actions;
}

void VipProcessingObjectMenu::selected(QAction* act)
{
	// we use the property '_vip_notrigger' to disable an action
	if (!act->property("_vip_notrigger").toBool()) {
		m_data->last = act->property("Info").value<VipProcessingObject::Info>();
		Q_EMIT selected(selectedProcessingInfo());
	}
}

void VipProcessingObjectMenu::hover(QAction*)
{
	// if (act->menu() && !act->menu()->isVisible())
	//  {
	//  int x = act->menu()->parentWidget()->mapToGlobal(QPoint(act->menu()->parentWidget()->width(),0)).x();
	//  int y = QCursor::pos().y();
	//  act->menu()->popup(QPoint(x,y));
	//  }
}
