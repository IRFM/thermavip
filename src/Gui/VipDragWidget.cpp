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

#include "VipDragWidget.h"
#include "VipCore.h"
#include "VipGui.h"
#include "VipLogging.h"
#include "VipUniqueId.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QCursor>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif
#include <QDrag>
#include <QDragEnterEvent>
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSizeGrip>
#include <QSplitter>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QtMath>

static QMap<QWidget*, QSharedPointer<VipDragWidgetHandler>> handlers;

VipDragWidgetHandler::VipDragWidgetHandler()
  : QObject()
  , d_parent(nullptr)
  , d_focus(nullptr)
{
}

QWidget* VipDragWidgetHandler::parentWidget()
{
	return qobject_cast<QWidget*>(d_parent.data());
}

VipDragWidget* VipDragWidgetHandler::focusWidget()
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(d_focus.data());
	// ensure that the focus widget belong to this handler
	if (w && topLevelMultiDragWidgets().indexOf(w->topLevelMultiDragWidget()) < 0) {
		d_focus = nullptr;
		return nullptr;
	}
	return w;
}

QList<VipMultiDragWidget*> VipDragWidgetHandler::topLevelMultiDragWidgets()
{
	return vipListCast<VipMultiDragWidget*>(d_widgets);
}

QList<VipBaseDragWidget*> VipDragWidgetHandler::baseDragWidgets()
{
	QList<VipBaseDragWidget*> res;
	for (int w = 0; w < d_widgets.size(); ++w) {
		if (VipMultiDragWidget* mdrag = d_widgets[w]) {
			res << mdrag;
			res.append(mdrag->findChildren<VipBaseDragWidget*>());
		}
	}

	return res;
}

VipMultiDragWidget* VipDragWidgetHandler::maximizedMultiDragWidgets()
{
	for (int i = 0; i < d_widgets.size(); ++i)
		if (VipMultiDragWidget* mdrag = d_widgets[i])
			if (mdrag->isMaximized())
				return mdrag;
	return nullptr;
}

VipDragWidgetHandler* VipDragWidgetHandler::find(QWidget* parent)
{
	QSharedPointer<VipDragWidgetHandler>& handler = handlers[parent];
	if (!handler) {
		handler = QSharedPointer<VipDragWidgetHandler>(new VipDragWidgetHandler());
		handler->d_parent = parent;
	}
	return handler.data();
}

VipDragWidgetHandler* VipDragWidgetHandler::find(VipBaseDragWidget* widget)
{
	return find(widget->topLevelParent());
}

void VipDragWidgetHandler::remove(VipMultiDragWidget* top_level)
{
	for (QMap<QWidget*, QSharedPointer<VipDragWidgetHandler>>::iterator it = handlers.begin(); it != handlers.end(); ++it) {
		if (it.value()->d_widgets.removeOne(top_level)) {
			Q_EMIT it.value()->removed(top_level);
		}
	}
}

void VipDragWidgetHandler::setParent(VipMultiDragWidget* top_level, QWidget* parent)
{
	// remove this top level VipMultiDragWidget to all handlers
	remove(top_level);
	// add it to the right handler
	VipDragWidgetHandler* handle = VipDragWidgetHandler::find(parent);
	handle->d_widgets.append(top_level);

	// find the best position
	if (top_level->pos() == QPoint(0, 0))
		top_level->move(0, 0);

	// set the focus to one of the child VipDragWidget
	if (handle->focusWidget() && top_level->isAncestorOf(handle->focusWidget())) {
	}
	else if (VipDragWidget* ws = top_level->findChild<VipDragWidget*>()) {
		ws->setFocusWidget();
	}

	Q_EMIT handle->added(top_level);
}

static void minimizeDragWidget(VipBaseDragWidget* w, bool minimize)
{
	if (VipDragWidget* d = qobject_cast<VipDragWidget*>(w)){
		if (minimize) {
			if (d->property("_vip_minimizeWidget").value<QWidget*>() == nullptr) {

				// make sure the grand parent is a VipDragTabWidget
				if (w->parentWidget())
					if (!qobject_cast<VipDragTabWidget*>(w->parentWidget()->parentWidget()))
						return;

				new VipMinimizeWidget(d);
				// Pass the focus to another drag widget
				if (VipMultiDragWidget* mw = w->topLevelMultiDragWidget()) {
					QList<VipDragWidget*> ws = mw->findChildren<VipDragWidget*>();
					for (int i = 0; i < ws.size(); ++i)
						if (ws[i] != w && !ws[i]->isMinimized()) {
							ws[i]->setFocus();
							break;
						}
				}
			}
		}
		else {
			d->setProperty("_vip_minimizeWidget", QVariant());
			if (VipMinimizeWidget* m = d->parentWidget()->parentWidget()->findChild<VipMinimizeWidget*>()) {
				d->show();
				m->deleteLater();
			}
		}
	}
}

class VipBaseDragWidget::PrivateData
{
public:
	int id;
	VisibilityState visibility;
	Operations operations;
	QPoint mousePress;
	Qt::MouseButton mouseButton;
	bool destroy;

	PrivateData()
	  : mouseButton(Qt::NoButton)
	  , destroy(false)
	{
	}
};

VipBaseDragWidget::VipBaseDragWidget(QWidget* parent)
  : QFrame(parent)
  , VipRenderObject(this)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->visibility = Normal;
	d_data->operations = AllOperations;

	d_data->id = VipUniqueId::id(static_cast<VipBaseDragWidget*>(this));
	connect(this, &VipBaseDragWidget::windowTitleChanged, this, &VipBaseDragWidget::addIdToTitle);
	connect(VipUniqueId::typeId<VipBaseDragWidget>(), &VipTypeId::idChanged, this, &VipBaseDragWidget::addIdToTitle);

	this->setProperty("showIdInTitle", true);
	// setWindowTitle("multi display");
	setWindowIcon(QIcon());

	this->setAttribute(Qt::WA_DeleteOnClose);
	this->setFocusPolicy(Qt::StrongFocus);
	this->setAutoFillBackground(true);
	this->setAcceptDrops(true);
}

VipBaseDragWidget::~VipBaseDragWidget()
{
	d_data->destroy = true;
}

VipMultiDragWidget* VipBaseDragWidget::parentMultiDragWidget() const
{
	QWidget* p = parentWidget();
	while (p) {
		if (VipMultiDragWidget* m = qobject_cast<VipMultiDragWidget*>(p))
			return m;
		p = p->parentWidget();
	}
	return nullptr;
}

VipMultiDragWidget* VipBaseDragWidget::topLevelMultiDragWidget() const
{
	VipMultiDragWidget* top = parentMultiDragWidget();
	while (top) {
		VipMultiDragWidget* tmp = top->parentMultiDragWidget();
		if (!tmp)
			return top;

		top = tmp;
	}

	return top;
}

VipMultiDragWidget* VipBaseDragWidget::validTopLevelMultiDragWidget() const
{
	VipMultiDragWidget* top = topLevelMultiDragWidget();
	if (!top)
		return qobject_cast<VipMultiDragWidget*>(const_cast<VipBaseDragWidget*>(this));
	else
		return top;
}

QWidget* VipBaseDragWidget::topLevelParent()
{
	VipMultiDragWidget* top_level = topLevelMultiDragWidget();
	if (top_level)
		return top_level->topLevelParent();
	else
		return this->parentWidget();
}

bool VipBaseDragWidget::isTopLevel() const
{
	VipMultiDragWidget* top = parentMultiDragWidget();
	if (!top)
		return true;
	else if (top->count() == 1 && top->isTopLevel())
		return true;
	else
		return false;
}

VipBaseDragWidget::VisibilityState VipBaseDragWidget::visibility() const
{
	return d_data->visibility;
}

bool VipBaseDragWidget::isMaximized() const
{
	if (d_data->visibility == Maximized)
		return true;
	else if (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && this->parentMultiDragWidget()->visibility() == Maximized)
		return true;
	else if (qobject_cast<const VipMultiDragWidget*>(this)) {
		const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
		VipBaseDragWidget* base = w->widget(0, 0, 0);
		if (w->count() == 1 && base && base->visibility() == Maximized)
			return true;
	}

	return false;
}

bool VipBaseDragWidget::isMinimized() const
{
	if (d_data->visibility == Minimized)
		return true;
	else if (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && this->parentMultiDragWidget()->visibility() == Minimized)
		return true;
	else if (qobject_cast<const VipMultiDragWidget*>(this)) {
		const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
		VipBaseDragWidget* base = w->widget(0, 0, 0);
		if (w->count() == 1 && base && base->visibility() == Minimized)
			return true;
	}

	return false;
}

bool VipBaseDragWidget::isDropable() const
{
	if (!this->testSupportedOperation(Drop) || (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && !this->testSupportedOperation(Drop)))
		return false;
	else {
		if (qobject_cast<const VipMultiDragWidget*>(this)) {
			const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
			VipBaseDragWidget* base = w->widget(0, 0, 0);
			if (w->count() == 1 && base && !base->testSupportedOperation(Drop))
				return false;
		}
		return true;
	}
}

bool VipBaseDragWidget::isMovable() const
{
	if (!this->testSupportedOperation(Move) || (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && !this->testSupportedOperation(Move)))
		return false;
	else {
		if (qobject_cast<const VipMultiDragWidget*>(this)) {
			const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
			VipBaseDragWidget* base = w->widget(0, 0, 0);
			if (w->count() == 1 && base && !base->testSupportedOperation(Move))
				return false;
		}
		return true;
	}
}

bool VipBaseDragWidget::supportMaximize() const
{
	if (!this->testSupportedOperation(Maximize) || (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && !this->testSupportedOperation(Maximize)))
		return false;
	else {
		if (qobject_cast<const VipMultiDragWidget*>(this)) {
			const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
			VipBaseDragWidget* base = w->widget(0, 0, 0);
			if (w->count() == 1 && base && !base->testSupportedOperation(Maximize))
				return false;
		}
		return true;
	}
}

bool VipBaseDragWidget::supportMinimize() const
{
	if (!this->testSupportedOperation(Minimize) || (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && !this->testSupportedOperation(Minimize)))
		return false;
	else {
		if (qobject_cast<const VipMultiDragWidget*>(this)) {
			const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
			VipBaseDragWidget* base = w->widget(0, 0, 0);
			if (w->count() == 1 && base && !base->testSupportedOperation(Minimize))
				return false;
		}
		return true;
	}
}

bool VipBaseDragWidget::supportClose() const
{
	if (!this->testSupportedOperation(Closable) ||
	    (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && !this->parentMultiDragWidget()->testSupportedOperation(Closable)))
		return false;
	else {
		if (qobject_cast<const VipMultiDragWidget*>(this)) {
			const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
			VipBaseDragWidget* base = w->widget(0, 0, 0);
			if (w->count() == 1 && base && !base->testSupportedOperation(Closable))
				return false;
		}
		return true;
	}
}

bool VipBaseDragWidget::supportReceiveDrop() const
{
	if (!this->testSupportedOperation(ReceiveDrop) || (this->parentMultiDragWidget() && this->parentMultiDragWidget()->count() == 1 && !this->testSupportedOperation(ReceiveDrop)))
		return false;
	else {
		if (qobject_cast<const VipMultiDragWidget*>(this)) {
			const VipMultiDragWidget* w = static_cast<const VipMultiDragWidget*>(this);
			VipBaseDragWidget* base = w->widget(0, 0, 0);
			if (w->count() == 1 && base && !base->testSupportedOperation(ReceiveDrop))
				return false;
		}
		return true;
	}
}

// void VipBaseDragWidget::draw(QPainter * p)
// {
// QList<VipDragWidget*> children  = findChildren<VipDragWidget*>();
// if(VipDragWidget * w = qobject_cast<VipDragWidget*>(this))
// children << w;
//
// for(int i=0; i < children.size(); ++i)
// {
// if(children[i]->isVisible())
// {
//	QWidget * w = children[i]->widget();
//	QPoint pos = w->mapTo(this,QPoint(0,0));
//	w->render(p,pos,QRegion(),QWidget::DrawChildren);
// }
// }
// }

void VipBaseDragWidget::setInternalVisibility(VisibilityState state)
{
	if (state != d_data->visibility) {

		d_data->visibility = state;
		if (state == Maximized)
			this->setWindowState(Qt::WindowMaximized);
		else if (state == Minimized)
			this->setWindowState(Qt::WindowMinimized);
		else
			this->setWindowState(Qt::WindowNoState);

		Q_EMIT visibilityChanged(state);
		if (VipMultiDragWidget* w = validTopLevelMultiDragWidget())
			Q_EMIT VipDragWidgetHandler::find(w->parentWidget())->visibilityChanged(this);
	}
}

void VipBaseDragWidget::setVisibility(VisibilityState state)
{
	// if(state != d_data->visibility)
	{
		if (state == Normal)
			this->showNormal();
		else if (state == Maximized)
			this->showMaximized();
		else
			this->showMinimized();

		d_data->visibility = state;
	}
}

void VipBaseDragWidget::showMaximized()
{
	if (!supportMaximize())
		return;

	if (visibility() == Minimized) {
		this->setInternalVisibility(Maximized);
		minimizeDragWidget(this, false);
	}

	this->setInternalVisibility(Maximized);

	if (VipMultiDragWidget* w = parentMultiDragWidget()) {

		if (w->count() == 1) {
			w->showMaximized();
		}
		else {
			w->hideAllExcept(this);
		}
	}

	if (qobject_cast<VipDragWidget*>(this))
		static_cast<VipDragWidget*>(this)->setFocusWidget();
}

void VipBaseDragWidget::showMinimized()
{
	if (!supportMinimize())
		return;

	if (isMaximized())
		showNormal();
	this->setInternalVisibility(Minimized);
	minimizeDragWidget(this, true);
}

void VipBaseDragWidget::showNormal()
{
	if (visibility() == Minimized) {
		this->setInternalVisibility(Normal);
		minimizeDragWidget(this, false);
		return;
	}

	this->setInternalVisibility(Normal);

	if (VipMultiDragWidget* w = parentMultiDragWidget()) {

		if (w->count() == 1)
			w->showNormal();
		else
			w->showAll();
	}

	if (qobject_cast<VipDragWidget*>(this))
		static_cast<VipDragWidget*>(this)->setFocusWidget();
}

void VipBaseDragWidget::changeEvent(QEvent* evt)
{
	if (evt->type() == QEvent::WindowStateChange) {
		if (isMaximized()) {
			if (visibility() != Maximized)
				setVisibility(Maximized);
		}
		else if (isMinimized()) {
			if (visibility() != Minimized)
				setVisibility(Minimized);
		}
		else {
			if (visibility() != Normal)
				setVisibility(Normal);
		}
	}
	return QFrame::changeEvent(evt);
}

void VipBaseDragWidget::closeEvent(QCloseEvent* evt)
{
	evt->ignore();

	if (!testSupportedOperation(Closable))
		return;

	VipMultiDragWidget* w = parentMultiDragWidget();
	if (w) {
		if (w->count() == 1) {
			if (!w->testSupportedOperation(Closable))
				return;
			w->close();
		}
		else {
			this->deleteLater();
			if (VipMultiDragWidget* _w = qobject_cast<VipMultiDragWidget*>(this))
				if (_w->isTopLevel())
					Q_EMIT VipDragWidgetHandler::find(parentWidget())->closed(_w);
		}
	}
	else {
		this->deleteLater();
		if (VipMultiDragWidget* _w = qobject_cast<VipMultiDragWidget*>(this))
			if (_w->isTopLevel())
				Q_EMIT VipDragWidgetHandler::find(parentWidget())->closed(_w);
	}
}

/* void VipBaseDragWidget::Close()
{
	if(!testSupportedOperation(Closable))
		return;

	VipMultiDragWidget * w = parentMultiDragWidget();
	if(w)
	{
		if(w->count() == 1)
		{
			if(!w->testSupportedOperation(Closable))
					return;
			w->Close();
		}
		else
		{
			this->deleteLater();
			if (VipMultiDragWidget * w = qobject_cast<VipMultiDragWidget*>(this))
				if (w->isTopLevel())
					Q_EMIT VipDragWidgetHandler::find(parentWidget())->closed(w);
		}
	}
	else {
		this->deleteLater();
		if (VipMultiDragWidget * w = qobject_cast<VipMultiDragWidget*>(this))
			if (w->isTopLevel())
				Q_EMIT VipDragWidgetHandler::find(parentWidget())->closed(w);
	}
}*/

QPoint VipBaseDragWidget::topLevelPos()
{
	QWidget* top_level = topLevelParent();

	if (top_level)
		return top_level->mapFromGlobal(QCursor::pos());
	else
		return QCursor::pos();
}

void VipBaseDragWidget::setSupportedOperations(Operations ops)
{
	if (ops != d_data->operations) {
		d_data->operations = ops;
		Q_EMIT operationsChanged(d_data->operations);
	}
}

void VipBaseDragWidget::setSupportedOperation(Operation op, bool on)
{
	if (d_data->operations.testFlag(op) != on) {
		if (on)
			d_data->operations |= op;
		else
			d_data->operations &= ~op;

		Q_EMIT operationsChanged(d_data->operations);

		// for VipMultiDragWidget having one child, force the child to emit this signal to update its tool bar
		if (VipMultiDragWidget* multi = qobject_cast<VipMultiDragWidget*>(this))
			if (multi->count() == 1)
				if (VipBaseDragWidget* w = multi->widget(0, 0, 0))
					Q_EMIT w->operationsChanged(w->supportedOperations());
	}
}

bool VipBaseDragWidget::testSupportedOperation(Operation op) const
{
	return d_data->operations.testFlag(op);
}

VipBaseDragWidget::Operations VipBaseDragWidget::supportedOperations() const
{
	return d_data->operations;
}

VipBaseDragWidget* VipBaseDragWidget::fromChild(QWidget* child)
{
	while (child) {
		if (qobject_cast<VipBaseDragWidget*>(child))
			return static_cast<VipBaseDragWidget*>(child);
		child = child->parentWidget();
	}
	return nullptr;
}

bool VipBaseDragWidget::dragThisWidget(QObject* watched, const QPoint& mouse_pos)
{
	// cannot move/drag a maximized/minimized widget
	if (this->isMinimized())
		return false;

	if (qobject_cast<VipDragWidget*>(this))
		static_cast<VipDragWidget*>(this)->setFocusWidget();

	if (!isMovable())
		return false;

	// start dragging the widget

	QDrag drag(this);
	VipBaseDragWidgetMimeData* mimeData = new VipBaseDragWidgetMimeData();
	mimeData->setData("application/dragwidget", QByteArray::number((qint64)this));
	mimeData->dragWidget = this;

	// drag the right VipBaseDragWidget
	if (qobject_cast<VipMultiDragWidget*>(this))
		if (static_cast<VipMultiDragWidget*>(this)->count() == 1) {
			VipBaseDragWidget* base = static_cast<VipMultiDragWidget*>(this)->widget(0, 0, 0);
			if (base)
				mimeData->dragWidget = base;
		}

	// hide the widget to drag, but display its content through a pixmap
	QPointer<VipBaseDragWidget> to_hide;
	if (parentMultiDragWidget() && parentMultiDragWidget()->count() == 1)
		to_hide = parentMultiDragWidget();
	else
		to_hide = this;

	QPoint global = static_cast<QWidget*>(watched)->mapToGlobal(mouse_pos);
	QPoint pos = to_hide->mapFromGlobal(global); // QCursor::pos());

	QPixmap pixmap(to_hide->size());
	pixmap.fill(QColor(Qt::transparent));
	QPainter painter(&pixmap);
	painter.setOpacity(0.5);
	to_hide->render(&painter);
	to_hide->hide();

	drag.setMimeData(mimeData);
	drag.setPixmap(pixmap);
	drag.setHotSpot(pos);

	// while moving the VipMultiDragWidget, emi the signal VipDragWidgetHandler::moving() every 50 ms
	QTimer timer;
	timer.setSingleShot(false);
	timer.setInterval(50);
	connect(&timer, &QTimer::timeout, std::bind(&VipDragWidgetHandler::moving, VipDragWidgetHandler::find(to_hide->parentWidget()), qobject_cast<VipMultiDragWidget*>(to_hide)));
	timer.start();

	QWidget* prev_top_level = topLevelParent();

	Qt::DropAction dropAction = drag.exec();
	bool no_drop = (dropAction == Qt::IgnoreAction || !drag.target());

	d_data->mousePress = QPoint(0, 0);

	timer.stop();

	if (!isDropable())
		no_drop = true;

	// reset focus to this VipDragWidget
	if (qobject_cast<VipDragWidget*>(this))
		static_cast<VipDragWidget*>(this)->setFocusWidget();

	int distance = (this->mapFromGlobal(QCursor::pos()) - pos).manhattanLength();
	if (distance < 50) {
		to_hide->show();
		this->validTopLevelMultiDragWidget()->raise();
		return true;
	}

	// this widget has been moved and not dropped
	if (no_drop && parentMultiDragWidget()) {
		// the widget is the only VipBaseDragWidget of parent VipMultiDragWidget: move the VipMultiDragWidget
		if (parentMultiDragWidget()->count() == 1) {
			// special case: we drop the VipMultiDragWidget on a different widget: reparent
			if (QWidget* parent = qobject_cast<QWidget*>(drag.target()))
				if (parent != prev_top_level && qobject_cast<VipViewportArea*>(parent)) {
					if (parentMultiDragWidget()->supportReparent(parent))
						parentMultiDragWidget()->setParent(parent);
				}

			parentMultiDragWidget()->move(topLevelPos() - pos);
			parentMultiDragWidget()->show();
		}
		// extract the VipBaseDragWidget from its parent
		else {
			// this widget is a VipMultiDragWidget itself, just reparent and move it
			if (qobject_cast<VipMultiDragWidget*>(this)) {
				this->setParent(topLevelParent());
				this->move(topLevelPos() - pos);
				this->show();
				this->raise();
			}
			// it is a VipDragWidget, insert it into a new VipMultiDragWidget
			else {
				// VipViewportArea * area = qobject_cast<VipViewportArea*>(drag.target());
				//  if (area && area->findChildren<VipMultiDragWidget*>().isEmpty())
				//  //drop on empty VipViewportArea: allowed
				//  ;
				// Check if we can extract a drag widget from its parent to make it free
				if (!this->testSupportedOperation(DragWidgetExtract) || (parentMultiDragWidget() && !parentMultiDragWidget()->testSupportedOperation(DragWidgetExtract))) {
					this->show();
					return true;
				}

				QSize size = this->size();
				VipMultiDragWidget* new_widget = parentMultiDragWidget()->create(topLevelParent());
				new_widget->setWidget(0, 0, this);
				new_widget->move(topLevelPos() - pos);
				new_widget->show();
				new_widget->resize(size);
			}
		}
	}
	else if (no_drop) {
		// special case: we drop the VipMultiDragWidget on a different widget: reparent
		if (QWidget* parent = qobject_cast<QWidget*>(drag.target()))
			if (parent != prev_top_level) {
				if (static_cast<VipMultiDragWidget*>(this)->supportReparent(parent))
					this->setParent(parent);
			}

		this->move(topLevelPos() - pos);
	}

	this->validTopLevelMultiDragWidget()->raise();
	this->validTopLevelMultiDragWidget()->show();
	// to_hide->show();

	return true;
}

bool VipBaseDragWidget::event(QEvent* event)
{
	if (event->type() == QEvent::ParentChange) {
		// update the VipDragWidgetHandler handlers
		if (qobject_cast<VipMultiDragWidget*>(this)) {
			if (VipMultiDragWidget* top_level = (this)->validTopLevelMultiDragWidget()) {

				VipDragWidgetHandler::setParent(top_level, top_level->parentWidget());
			}
		}
	}

	return QFrame::event(event);
}

void VipBaseDragWidget::dragEnterEvent(QDragEnterEvent* evt)
{
	// QFrame::dragEnterEvent(evt);
	evt->acceptProposedAction();
}

void VipBaseDragWidget::dropEvent(QDropEvent* evt)
{
	// QFrame::dropEvent(evt);
	evt->ignore();
}

bool VipBaseDragWidget::isDestroying() const
{
	const VipBaseDragWidget* w = this;
	while (w) {
		if(!w->d_data)
			return true;
		if (w->d_data->destroy)
			return true;
		w = qobject_cast<const VipBaseDragWidget*>(w->parentWidget());
	}
	return false;
}

void VipBaseDragWidget::setShowIDInTitle(bool enable)
{
	this->setProperty("showIdInTitle", enable);
	setWindowTitle(this->title());
}

bool VipBaseDragWidget::showIdInTitle() const
{
	return this->property("showIdInTitle").toBool();
}

void VipBaseDragWidget::addIdToTitle()
{
	if (!this->property("showIdInTitle").toBool()) {
		QString t = title();
		if (this->windowTitle() != t)
			this->setWindowTitle(t);
		return;
	}

	int new_id = VipUniqueId::id<VipBaseDragWidget>(this);
	QString t = QString::number(new_id) + "-" + title();
	if (t.size() > 16)
		t = t.mid(0, 16) + "...";
	d_data->id = new_id;
	if (this->windowTitle() != t)
		this->setWindowTitle(t);
}

void VipBaseDragWidget::setTitleWithId(const QString & text)
{
	if (!this->property("showIdInTitle").toBool()) {
		if (this->windowTitle() != text)
			this->setWindowTitle(text);
		return;
	}

	int new_id = VipUniqueId::id<VipBaseDragWidget>(this);
	QString t = QString::number(new_id) + "-" + text;
	if (t.size() > 16)
		t = t.mid(0, 16) + "...";
	d_data->id = new_id;
	if (this->windowTitle() != t)
		this->setWindowTitle(t);
}

QString VipBaseDragWidget::title() const
{
	// title start iwth 'id- '
	QString t = this->windowTitle();
	int index = t.indexOf("-");
	if (index < 0)
		return t;

	bool ok;
	int read_id = t.mid(0, index).toInt(&ok);
	if (ok && read_id == d_data->id) {
		return t.mid(index + 1);
	}
	return t;
}

class VipDragWidget::PrivateData
{
public:
	PrivateData()
	  : widget(nullptr)
	  , focus(false)
	{
	}
	QWidget* widget;
	bool focus;
};

VipDragWidget::VipDragWidget(QWidget* parent)
  : VipBaseDragWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	setProperty("has_focus", false);
	style()->unpolish(this);
	style()->polish(this);
}

VipDragWidget::~VipDragWidget()
{
	this->VipBaseDragWidget::d_data->destroy = true;
	VipMultiDragWidget* top_level = this->topLevelMultiDragWidget();

	if (d_data->focus && top_level)
		top_level->passFocus();
	if (top_level)
		Q_EMIT VipDragWidgetHandler::find(top_level)->contentChanged(top_level);
}

VipDragWidget* VipDragWidget::next() const
{
	VipMultiDragWidget* mw = this->parentMultiDragWidget();
	bool takeNext = false;
	int x = 0, y = 0;

	for (;;) {
	start_loop:
		if (!mw)
			return nullptr;

		for (y = 0; y < mw->mainCount(); ++y) {
			for (x = 0; x < mw->subCount(y); ++x) {
				auto* t = mw->tabWidget(y, x);
				for (int i = 0; i < t->count(); ++i) {

					QWidget* tw = t->widget(i);
					if (takeNext) {
						if (VipBaseDragWidget* b = qobject_cast<VipBaseDragWidget*>(tw)) {

							if (VipDragWidget* w = qobject_cast<VipDragWidget*>(b)) {
								if (!w->isMinimized())
									return w;
							}
							else if (VipMultiDragWidget* _mw = qobject_cast<VipMultiDragWidget*>(b)) {
								// returns the first VipDragWidget in this multi drag widget
								if (VipDragWidget* w = _mw->firstVisibleDragWidget())
									return w;
							}
						}
					}
					else {
						if (tw == this)
							takeNext = true;
						else {
							if (VipMultiDragWidget* _mw = qobject_cast<VipMultiDragWidget*>(tw)) {
								if (_mw->findChildren<const VipDragWidget*>().indexOf(this) >= 0) {
									mw = _mw;
									goto start_loop;
								}
							}
						}
					}
				}
			}
		}

		mw = mw->parentMultiDragWidget();
	}
	return nullptr;
}

VipDragWidget* VipDragWidget::prev() const
{
	VipMultiDragWidget* mw = this->parentMultiDragWidget();
	bool takeNext = false;
	int x = 0, y = 0;

	for (;;) {
	start_loop:
		if (!mw)
			return nullptr;

		for (y = mw->mainCount() - 1; y >= 0; --y) {
			for (x = mw->subCount(y) - 1; x >= 0; --x) {
				auto* t = mw->tabWidget(y, x);
				for (int i = t->count() - 1; i >= 0; --i) {

					QWidget* tw = t->widget(i);
					if (takeNext) {
						if (VipBaseDragWidget* b = qobject_cast<VipBaseDragWidget*>(tw)) {

							if (VipDragWidget* w = qobject_cast<VipDragWidget*>(b)) {
								if (!w->isMinimized())
									return w;
							}
							else if (VipMultiDragWidget* _mw = qobject_cast<VipMultiDragWidget*>(b)) {
								// returns the first VipDragWidget in this multi drag widget
								if (VipDragWidget* w = _mw->lastVisibleDragWidget())
									return w;
							}
						}
					}
					else {
						if (tw == this)
							takeNext = true;
						else {
							if (VipMultiDragWidget* _mw = qobject_cast<VipMultiDragWidget*>(tw)) {
								if (_mw->findChildren<const VipDragWidget*>().indexOf(this) >= 0) {
									mw = _mw;
									goto start_loop;
								}
							}
						}
					}
				}
			}
		}

		mw = mw->parentMultiDragWidget();
	}
	return nullptr;
}

void VipDragWidget::setFocusWidget()
{

	VipMultiDragWidget* top_level = this->topLevelMultiDragWidget();
	if (top_level) {
		VipDragWidgetHandler* handler = VipDragWidgetHandler::find(top_level->parentWidget());
		if (handler->d_focus != this) {
			VipDragWidget* old_focus = handler->d_focus;
			VipDragWidget* new_focus = this;

			// remove focus to all linked VipDragWidget
			QList<VipBaseDragWidget*> drags = handler->baseDragWidgets();
			for (int i = 0; i < drags.size(); ++i) {
				if (VipDragWidget* drag = qobject_cast<VipDragWidget*>(drags[i])) {
					if (drag->parentMultiDragWidget()->VipBaseDragWidget::d_data->destroy)
						continue;

					// TEST
					if (!drag->property("has_focus").toBool())
						continue;

					drag->d_data->focus = false;
					drag->setProperty("has_focus", false);
					drag->style()->unpolish(drags[i]);
					drag->style()->polish(drags[i]);
				}
			}

			new_focus->d_data->focus = true;
			handler->d_focus = new_focus;
			if (this->widget())
				this->widget()->setFocus(Qt::MouseFocusReason);

			Q_EMIT handler->focusChanged(old_focus, new_focus);
		}
	}

	if (VipMultiDragWidget* w = this->parentMultiDragWidget())
		if (w->VipBaseDragWidget::d_data->destroy)
			return;

	if (property("has_focus").toBool())
		return;

	this->d_data->focus = true;

	setProperty("has_focus", true);
	style()->unpolish(this);
	style()->polish(this);
}

void VipDragWidget::relayout()
{
	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(1);
	if (d_data->widget)
		lay->addWidget(d_data->widget);
	else
		lay->addStretch(1);

	if (layout())
		delete layout();

	this->setLayout(lay);
}

bool VipDragWidget::isFocusWidget() const
{
	return d_data->focus;
}

QWidget* VipDragWidget::widget() const
{
	return const_cast<VipDragWidget*>(this)->d_data->widget;
}

void VipDragWidget::setWidget(QWidget* widget)
{
	if (d_data->widget) {
		disconnect(d_data->widget, SIGNAL(windowTitleChanged(const QString&)), this, SLOT(titleChanged()));
		disconnect(d_data->widget, SIGNAL(windowIconChanged(const QIcon&)), this, SLOT(titleChanged()));
		d_data->widget->close();
		d_data->widget->deleteLater();
	}

	d_data->widget = widget;
	if (d_data->widget && !d_data->widget->windowTitle().isEmpty())
		this->setWindowTitle(d_data->widget->windowTitle());

	if (d_data->widget) {
		d_data->widget->setFocusPolicy(Qt::StrongFocus);
		connect(d_data->widget, SIGNAL(windowTitleChanged(const QString&)), this, SLOT(titleChanged()));
		connect(d_data->widget, SIGNAL(windowIconChanged(const QIcon&)), this, SLOT(titleChanged()));

		vipSetDragWidget().callAllMatch(this, widget);
	}

	if (VipMultiDragWidget* w = this->validTopLevelMultiDragWidget()) {
		Q_EMIT VipDragWidgetHandler::find(w)->contentChanged(w);
	}
	relayout();

	// at this point, the style sheet is reapplied, we need to reset the GUI parameters
	if (!VipGuiDisplayParamaters::instance()->inSessionLoading())
		VipGuiDisplayParamaters::instance()->apply(widget);
}

void VipDragWidget::titleChanged()
{
	if (d_data->widget) {
		if (!d_data->widget->windowTitle().isEmpty())
			this->setTitleWithId(d_data->widget->windowTitle());
		if (!d_data->widget->windowIcon().isNull())
			this->setWindowIcon(d_data->widget->windowIcon());
	}
}

QSize VipDragWidget::sizeHint() const
{
	if (!d_data->widget)
		return VipBaseDragWidget::sizeHint();

	QSize res = d_data->widget->sizeHint();
	return res;
}

VipDragTabWidget::VipDragTabWidget(QWidget* parent)
  : QTabWidget(parent)
{
	TabBar()->hide();
	this->setAutoFillBackground(true);
}
VipDragTabWidget ::~VipDragTabWidget()
{
	blockSignals(true);
}

QTabBar* VipDragTabWidget::TabBar() const
{
	return this->tabBar();
}

QSize VipDragTabWidget::sizeHint() const
{
	if (count() == 0)
		return QTabWidget::sizeHint();

	QSize size(0, 0);
	for (int i = 0; i < count(); ++i) {
		QSize tmp = widget(i)->sizeHint();
		size.setWidth(qMax(size.width(), tmp.width()));
		size.setHeight(qMax(size.height(), tmp.height()));
	}
	return size;
}

void VipDragTabWidget::tabInserted(int index)
{
	Q_UNUSED(index)
	if (count() > 1)
		TabBar()->show();
	else
		TabBar()->hide();
}
void VipDragTabWidget::tabRemoved(int index)
{
	Q_UNUSED(index)
	if (count() > 1)
		TabBar()->show();
	else
		TabBar()->hide();
}

class VipMinimizeWidget::PrivateData
{
public:
	QPointer<VipBaseDragWidget> dragWidget;
	Qt::Orientation orientation;
	QPixmap close;
	QPixmap wPixmap;
	int maxExtent;
	bool inside;

	QColor background;
	QColor backgroundHover;
	QColor closeBackground;
	QColor closeBackgroundHover;

	PrivateData()
	  : dragWidget(nullptr)
	  , orientation(Qt::Vertical)
	  , maxExtent(20)
	  , inside(false)
	{
		QIcon c = vipIcon("close.png");
		close = c.pixmap(c.actualSize(QSize(100, 100)));
		background = QColor(230, 230, 230);
		backgroundHover = Qt::lightGray;
		closeBackground = QColor(200, 200, 200);
		closeBackgroundHover = Qt::lightGray;
	}
};

VipMinimizeWidget::VipMinimizeWidget(VipBaseDragWidget* widget)
  : QFrame(widget->parentWidget()->parentWidget())
  , VipRenderObject(this)
{

	// QWidget* p = this->parentWidget();
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->dragWidget = widget;
	widget->setProperty("_vip_minimizeWidget", QVariant::fromValue((QWidget*)this));

	QString title = d_data->dragWidget->windowTitle();
	if (VipDragWidget* d = qobject_cast<VipDragWidget*>(d_data->dragWidget))
		title = d->widget()->windowTitle();

	if (d_data->wPixmap.isNull()) {
		VipText t("<div>" + title + "</div>");
		const int w = t.textSize().width();
		const int h = ((double)d_data->dragWidget->height() / d_data->dragWidget->width()) * w;
		// draw player pixmap
		d_data->wPixmap = QPixmap(d_data->dragWidget->width(), d_data->dragWidget->height());
		{
			QPainter p(&d_data->wPixmap);
			d_data->dragWidget->render(&p, QPoint(), QRegion(), QWidget::DrawChildren);
		}
		d_data->wPixmap = d_data->wPixmap.scaled(QSize(w, h), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		setToolTip(title + "<br>" + vipToHtml(d_data->wPixmap, "align='middle'"));
	}

	widget->hide();
	move(0, 0);
	reorganize();
	show();
	parentWidget()->installEventFilter(this);
	setMouseTracking(true);
}

VipMinimizeWidget::~VipMinimizeWidget()
{
	if (QWidget* w = parentWidget()) {
		w->removeEventFilter(this);
		w->setMaximumHeight(16777215);
		w->setMaximumWidth(16777215);
	}
}

QColor VipMinimizeWidget::background() const
{
	return d_data->background;
}
void VipMinimizeWidget::setBackground(const QColor& c)
{
	d_data->background = c;
	update();
}
QColor VipMinimizeWidget::backgroundHover() const
{
	return d_data->backgroundHover;
}
void VipMinimizeWidget::setBackgroundHover(const QColor& c)
{
	d_data->backgroundHover = c;
	update();
}
QColor VipMinimizeWidget::closeBackground() const
{
	return d_data->closeBackground;
}
void VipMinimizeWidget::setCloseBackground(const QColor& c)
{
	d_data->closeBackground = c;
	update();
}
QColor VipMinimizeWidget::closeBackgroundHover() const
{
	return d_data->closeBackgroundHover;
}
void VipMinimizeWidget::setCloseBackgroundHover(const QColor& c)
{
	d_data->closeBackgroundHover = c;
	update();
}

int VipMinimizeWidget::extent() const
{
	return d_data->maxExtent;
}
void VipMinimizeWidget::setExtent(int ext)
{
	d_data->maxExtent = ext;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void VipMinimizeWidget::enterEvent(QEvent*)
#else
void VipMinimizeWidget::enterEvent(QEnterEvent*)
#endif
{
	d_data->inside = true;
	update();
}
void VipMinimizeWidget::leaveEvent(QEvent*)
{
	d_data->inside = false;
	update();
}

void VipMinimizeWidget::startRender(VipRenderState&)
{
	parentWidget()->hide();
}
void VipMinimizeWidget::endRender(VipRenderState&)
{
	parentWidget()->show();
}

bool VipMinimizeWidget::event(QEvent* evt)
{
	return QFrame::event(evt);
}

void VipMinimizeWidget::mouseMoveEvent(QMouseEvent*)
{
	d_data->inside = true;
	update();
}

void VipMinimizeWidget::mousePressEvent(QMouseEvent*)
{
	bool inside_close = false;
	QPoint p = this->mapFromGlobal(QCursor::pos());
	if (d_data->orientation == Qt::Vertical && p.y() < d_data->maxExtent)
		inside_close = true;
	else if (d_data->orientation == Qt::Horizontal && p.x() < d_data->maxExtent)
		inside_close = true;
	if (inside_close) {
		d_data->dragWidget->deleteLater();
		return;
	}

	parentWidget()->removeEventFilter(this);
	parentWidget()->setMaximumHeight(16777215);
	parentWidget()->setMaximumWidth(16777215);
	if (d_data->dragWidget)
		d_data->dragWidget->show();
	vipProcessEvents(nullptr, 100);
	d_data->dragWidget->showNormal();
	d_data->dragWidget->setFocus();

	if (VipMultiDragWidget* w = d_data->dragWidget->topLevelMultiDragWidget())
		QMetaObject::invokeMethod(w, "reorganizeMinimizedChildren");
}

void VipMinimizeWidget::paintEvent(QPaintEvent* evt)
{
	if (!d_data->dragWidget)
		return;

	QFrame::paintEvent(evt);
	VipText text;

	QString title = d_data->dragWidget->windowTitle();
	if (VipDragWidget* d = qobject_cast<VipDragWidget*>(d_data->dragWidget))
		title = d->widget()->windowTitle();

	text.setText(title);
	text.setTextPen(QPen(vipWidgetTextBrush(this).color()));

	bool inside_close = false;
	if (d_data->inside) {
		QPoint p = this->mapFromGlobal(QCursor::pos());
		if (d_data->orientation == Qt::Vertical && p.y() < d_data->maxExtent)
			inside_close = true;
		else if (d_data->orientation == Qt::Horizontal && p.x() < d_data->maxExtent)
			inside_close = true;
	}
	if (d_data->inside) {
		if (inside_close)
			text.setBackgroundBrush(d_data->background);
		else
			text.setBackgroundBrush(d_data->backgroundHover);
	}
	else
		text.setBackgroundBrush(d_data->background);

	QPainter p(this);

	// draw text
	if (d_data->orientation == Qt::Horizontal) {
		text.draw(&p, QRectF(0, 0, width(), height()));
	}
	else {
		QRectF r(0, 0, height(), width());
		p.setTransform(QTransform().translate(width(), 0).rotate(90));
		text.draw(&p, r);
	}
	p.resetTransform();

	// draw close button
	QBrush close_back = d_data->closeBackground;
	if (d_data->inside && inside_close)
		close_back.setColor(d_data->closeBackgroundHover);
	if (d_data->orientation == Qt::Vertical) {
		p.fillRect(QRect(0, 0, width(), width()), close_back);
		QPoint pos((width() - d_data->close.width()) / 2, (width() - d_data->close.width()) / 2);
		p.drawPixmap(pos, d_data->close);
	}
	else {
		p.fillRect(QRect(0, 0, height(), height()), close_back);
		QPoint pos((height() - d_data->close.height()) / 2, (height() - d_data->close.height()) / 2);
		p.drawPixmap(pos, d_data->close);
	}

	// draw transparent border of 1px
	/*QPen pen = Qt::transparent;
	p.setPen(Qt::transparent);
	p.setBrush(QBrush());
	p.setCompositionMode(QPainter::CompositionMode_Source);
	p.drawRect(QRect(0, 0, width() - 1, height() - 1));*/
}

bool VipMinimizeWidget::eventFilter(QObject*, QEvent* evt)
{
	switch (evt->type()) {
		case QEvent::MouseButtonPress: {
			this->mousePressEvent(static_cast<QMouseEvent*>(evt));
			return true;
		}
		case QEvent::MouseMove:
			d_data->inside = true;
			break;
		case QEvent::Enter:
			d_data->inside = true;
			break;
		case QEvent::Leave:
			d_data->inside = false;
			break;
		case QEvent::Resize:
			reorganize();
			break;
		default:
			break;
	}
	return false;
}

void VipMinimizeWidget::reorganize()
{
	if (!d_data->dragWidget->isMinimized())
		return;

	// get parent multi drag widget
	VipMultiDragWidget* m = d_data->dragWidget->parentMultiDragWidget();
	if (!m)
		return;

	QPoint pos = m->indexOf(d_data->dragWidget);
	m->subSplitter(pos.y());
	int count = 0;
	// count visible widgets
	for (int i = 0; i < m->subCount(pos.y()); ++i)
		if (VipBaseDragWidget* b = m->widget(pos.y(), i, 0))
			if (b != d_data->dragWidget && !b->isHidden()) {
				++count;
			}

	if ((count && m->orientation() == Qt::Vertical) || (count == 0 && m->orientation() == Qt::Horizontal)) {
		// there is at least one other visible drag widget in this row, organize minimized widgets vertically
		setMaximumWidth(d_data->maxExtent);
		setMaximumHeight(16777215);
		parentWidget()->setMaximumWidth(d_data->maxExtent);
		parentWidget()->setMaximumHeight(16777215);
		resize(d_data->maxExtent, parentWidget()->height());
		d_data->orientation = Qt::Vertical;
	}
	else {
		setMaximumHeight(d_data->maxExtent);
		setMaximumWidth(16777215);
		parentWidget()->setMaximumHeight(d_data->maxExtent);
		parentWidget()->setMaximumWidth(16777215);
		resize(parentWidget()->width(), d_data->maxExtent);
		d_data->orientation = Qt::Horizontal;
	}
	update();
}

/// \internal
/// Curstom QTabWidget class that automatically hide/show the tab bar according the to the number of pages.

VipDragWidgetHandle::VipDragWidgetHandle(VipMultiDragWidget* multiDragWidget, Qt::Orientation orientation, QSplitter* parent)
  : QSplitterHandle(orientation, parent)
  , multiDragWidget(multiDragWidget)
  , maxWidth(5)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setAcceptDrops(true);
	this->setAutoFillBackground(true);

	if (splitter()->orientation() == Qt::Vertical)
		setMaximumHeight(maxWidth);
	else
		setMaximumWidth(maxWidth);

	show();
}

Qt::Alignment VipDragWidgetHandle::HandleAlignment()
{
	if (splitter()->orientation() == Qt::Vertical && splitter()->indexOf(this) == 0)
		return Qt::AlignTop;
	else if (splitter()->orientation() == Qt::Vertical && splitter()->indexOf(this) == splitter()->count() - 1)
		return Qt::AlignBottom;
	else if (splitter()->orientation() == Qt::Horizontal && splitter()->indexOf(this) == 0)
		return Qt::AlignLeft;
	else if (splitter()->orientation() == Qt::Horizontal && splitter()->indexOf(this) == splitter()->count() - 1)
		return Qt::AlignRight;
	else
		return Qt::Alignment();
}

void VipDragWidgetHandle::setMaximumHandleWidth(int w)
{
	maxWidth = w;
	if (splitter()->orientation() == Qt::Vertical)
		setMaximumHeight(maxWidth);
	else
		setMaximumWidth(maxWidth);
}
int VipDragWidgetHandle::maximumHandleWidth() const
{
	return maxWidth;
}

QSize VipDragWidgetHandle::sizeHint() const
{
	// reset maximum width/height
	VipDragWidgetHandle* _this = const_cast<VipDragWidgetHandle*>(this);
	if (this->orientation() == Qt::Horizontal) {
		_this->setMaximumWidth(maxWidth);
		_this->setMaximumHeight(16777215);
	}
	else {
		_this->setMaximumWidth(16777215);
		_this->setMaximumHeight(maxWidth);
	}
	return QSplitterHandle::sizeHint();
}

///\internal
/// Compute the new splitter sizes when adding a widget.
/// new_widget_size is the size of the newly added widget.
/// This function only make sense when the splitter does not change its size (when the top level drag widget is maximized for instance).
static QList<int> addNewSplitterSize(QSplitter* s, int index, int* new_widget_size = nullptr)
{
	int width = s->orientation() == Qt::Horizontal ? s->width() : s->height();
	// compute new size ratio for each widget
	QVector<double> sizes(s->count());
	double sum = 0;
	for (int i = 0; i < sizes.size(); ++i) {
		double w = 0;
		if (i < index)
			w = Qt::Horizontal ? s->widget(i)->width() : s->widget(i)->height();
		else if (i > index)
			w = Qt::Horizontal ? s->widget(i - 1)->width() : s->widget(i - 1)->height();
		w = (w / width) * ((s->count() - 1) / (double)(s->count()));
		sizes[i] = w;
		if (w)
			sum += w;
	}
	// average ratio
	double avg = sum / (s->count() - 1);

	if (new_widget_size)
		*new_widget_size = width * avg;

	sizes[index] = avg;
	QList<int> res;
	for (int i = 0; i < sizes.size(); ++i)
		res.append(sizes[i] * width);
	res.append(0);
	return res;
}

bool VipDragWidgetHandle::dropMimeData(const QMimeData* mime)
{
	// check that this widget accept drop
	if (!multiDragWidget->supportReceiveDrop())
		return false;

	bool maximized = false;
	VipBaseDragWidget* widget = nullptr;
	if (VipMultiDragWidget* top_level = multiDragWidget->validTopLevelMultiDragWidget()) {
		maximized = top_level->isMaximized();
		widget = top_level->createFromMimeData(mime);
		if (widget && !widget->isDropable())
			return false;
	}
	if (!widget)
		return false;

	QList<int> sizes;
	if (maximized)
		sizes = addNewSplitterSize(splitter(), splitter()->indexOf(this));

	if (multiDragWidget->orientation() == Qt::Vertical) {

		if (splitter()->orientation() == Qt::Vertical) {
			int index = splitter()->indexOf(this);
			if (!multiDragWidget->insertMain(index, widget))
				return false;
		}
		else {
			int h_index = splitter()->indexOf(this);
			int v_index = multiDragWidget->mainSplitter()->indexOf(splitter());
			if (!multiDragWidget->insertSub(v_index, h_index, widget))
				return false;
		}
	}
	else {
		if (splitter()->orientation() == Qt::Horizontal) {
			int index = splitter()->indexOf(this);
			if (!multiDragWidget->insertMain(index, widget))
				return false;
		}
		else {
			int h_index = splitter()->indexOf(this);
			int v_index = multiDragWidget->mainSplitter()->indexOf(splitter());
			if (!multiDragWidget->insertSub(v_index, h_index, widget))
				return false;
		}
	}

	if (sizes.size())
		splitter()->setSizes(sizes);

	return true;
}

void VipDragWidgetHandle::paintEvent(QPaintEvent*)
{
	QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	opt.init(this);
#else
	opt.initFrom(this);
#endif
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

VipDragWidgetSplitter::VipDragWidgetSplitter(VipMultiDragWidget* multiDragWidget, Qt::Orientation orientation, QWidget* parent)
  : QSplitter(orientation, parent)
  , multiDragWidget(multiDragWidget)
  , maxWidth(5)
{
	this->setAttribute(Qt::WA_DeleteOnClose);
	this->setAutoFillBackground(true);
}

void VipDragWidgetSplitter::setMaximumHandleWidth(int w)
{
	if (maxWidth != w) {
		maxWidth = w;
		for (int i = 0; i <= this->count(); ++i)
			if (this->handle(i))
				static_cast<VipDragWidgetHandle*>(this->handle(i))->setMaximumHandleWidth(maxWidth);
	}
}
int VipDragWidgetSplitter::maximumHandleWidth() const
{
	return maxWidth;
}

QSplitterHandle* VipDragWidgetSplitter::createHandle()
{
	VipDragWidgetHandle* res = new VipDragWidgetHandle(multiDragWidget, this->orientation(), this);
	res->setMaximumHandleWidth(maximumHandleWidth());
	return res;
}

void VipDragWidgetSplitter::paintEvent(QPaintEvent*)
{
	QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	opt.init(this);
#else
	opt.initFrom(this);
#endif
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void VipDragWidgetSplitter::childEvent(QChildEvent* evt)
{
	QSplitter::childEvent(evt);

	if (!qobject_cast<QSplitterHandle*>(evt->child()))
		emitChildChanged(this, qobject_cast<QWidget*>(evt->child()), evt->added());
}

VipDragRubberBand::VipDragRubberBand(QWidget* parent)
  : QRubberBand(Rectangle, parent)
{
	options.shape = Rectangle;
	options.opaque = false;
	initStyleOption(&options);
	pen = QPen(Qt::green, 2);
}

void VipDragRubberBand::setBorderColor(const QColor& c)
{
	pen.setColor(c);
}
QColor VipDragRubberBand::borderColor() const
{
	return pen.color();
}

void VipDragRubberBand::setBorderWidth(double w)
{
	pen.setWidthF(w);
}
double VipDragRubberBand::borderWidth() const
{
	return pen.widthF();
}

void VipDragRubberBand::paintEvent(QPaintEvent*)
{
	// QRubberBand::paintEvent(evt);
	QPainter p(this);
	p.setPen(pen);
	QColor c = VipGuiDisplayParamaters::instance()->defaultPlayerBackgroundColor(); //.darker(120);
	c.setAlpha(150);
	p.setBrush(QBrush(c)); // Qt::NoBrush);
	QRect r(0, 0, width(), height());
	p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 2, 2);

	VipText t = this->text;
	t.setTextPen(QPen(VipGuiDisplayParamaters::instance()->defaultPlayerTextColor()));
	if (!t.isEmpty()) {
		QSize s = t.textSize().toSize();
		if (s.width() < r.width()) {
			t.setAlignment(Qt::AlignCenter);
			t.draw(&p, r);
		}
		else if (s.width() < r.height()) {
			QTransform tr;
			tr.translate(r.center().x(), r.center().y());
			tr.rotate(-90);
			p.setTransform(tr);
			t.setAlignment(Qt::AlignCenter);
			r = QRect(0, 0, r.height(), r.width());
			r.moveCenter(QPoint(0, 0));
			t.draw(&p, r);
		}
	}
}

class VipMultiDragWidget::PrivateData
{
public:

	QWidget* header;
	QSplitter* v_splitter;
	// VipDragWidgetSizeGrip * grip;
	QGridLayout* grid;

	// for maximize/minimize/restore
	QRect geometry; // save the geometry before minimizing/maximizing

	Qt::Orientation orientation;

	QPointer<VipBaseDragWidget> lastAdded;
	bool extra;
	int maxWidth;
};

static VipMultiDragWidget::reparent_function _reparent_function;
static std::function<void(VipMultiDragWidget*)> _on_multi_drag_widget_created;

VipMultiDragWidget::VipMultiDragWidget(QWidget* parent)
  : VipBaseDragWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->header = nullptr;
	d_data->v_splitter = nullptr;
	d_data->extra = true;
	d_data->maxWidth = 5;
	d_data->orientation = Qt::Vertical;

	this->setAttribute(Qt::WA_DeleteOnClose);
	this->setFrameShape(QFrame::StyledPanel);

	this->setAutoFillBackground(true);

	d_data->v_splitter = new VipDragWidgetSplitter(this, Qt::Vertical, this);
	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(0);

	d_data->grid = new QGridLayout();
	d_data->grid->setContentsMargins(0, 0, 0, 0);
	d_data->grid->setSpacing(0);
	d_data->grid->addWidget(d_data->v_splitter, 10, 10);

	lay->addLayout(d_data->grid);
	setLayout(lay);

	d_data->v_splitter->addWidget(createHSplitter());
	QWidget* bottom = new QWidget();
	d_data->v_splitter->addWidget(bottom);
	bottom->hide();
	d_data->v_splitter->handle(0)->show();

	// d_data->grip = new VipDragWidgetSizeGrip(this);
	// d_data->grip->resize(10,10);
	// d_data->grip->setWindowFlags(d_data->grip->windowFlags()|Qt::WindowStaysOnTopHint);

	// this->SetHeader(topTitle());

	connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(focusChanged(QWidget*, QWidget*)));

	// update the VipDragWidgetHandler handlers
	VipMultiDragWidget* top_level = static_cast<VipMultiDragWidget*>(this)->validTopLevelMultiDragWidget();
	if (top_level)
		VipDragWidgetHandler::setParent(top_level, top_level->parentWidget());

	this->setMinimumSize(QSize(200, 200));

	connect(d_data->v_splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(receivedSplitterMoved(int, int)));

	if (_on_multi_drag_widget_created)
		_on_multi_drag_widget_created(this);
}

VipMultiDragWidget::~VipMultiDragWidget()
{
	this->VipBaseDragWidget::d_data->destroy = true;
	Q_EMIT widgetDestroyed(this);

	passFocus();

	VipDragWidgetHandler::remove(this);

	QList<QTabWidget*> lst = findChildren<QTabWidget*>();
	for (int i = 0; i < lst.size(); ++i)
		disconnect(lst[i], SIGNAL(currentChanged(int)), this, SLOT(updateContent()));

	// be sure that there are no more event that this widget should handle (like updateContent())
	QCoreApplication::removePostedEvents(this);
}

void VipMultiDragWidget::setReparentFunction(reparent_function fun)
{
	_reparent_function = fun;
}
VipMultiDragWidget::reparent_function VipMultiDragWidget::reparentFunction()
{
	return _reparent_function;
}

void VipMultiDragWidget::onCreated(const std::function<void(VipMultiDragWidget*)>& fun)
{
	_on_multi_drag_widget_created = fun;
}

Qt::Orientation VipMultiDragWidget::orientation() const
{
	return d_data->orientation;
}
void VipMultiDragWidget::setOrientation(Qt::Orientation ori)
{
	if (d_data->orientation != ori) {
		d_data->orientation = ori;

		// change main splitter orientation
		d_data->v_splitter->setOrientation(ori);
		for (int i = 0; i < d_data->v_splitter->count() - 1; ++i) {
			static_cast<QSplitter*>(d_data->v_splitter->widget(i))->setOrientation(ori == Qt::Vertical ? Qt::Horizontal : Qt::Vertical);
		}
	}
}

void VipMultiDragWidget::setMaximumHandleWidth(int w)
{
	if (d_data->maxWidth != w) {
		d_data->maxWidth = w;
		static_cast<VipDragWidgetSplitter*>(mainSplitter())->setMaximumHandleWidth(w);
		for (int y = 0; y < mainCount(); ++y)
			static_cast<VipDragWidgetSplitter*>(subSplitter(y))->setMaximumHandleWidth(w);
	}
}
int VipMultiDragWidget::maximumHandleWidth() const
{
	return d_data->maxWidth;
}

void VipMultiDragWidget::passFocus()
{
	// if a child  VipDragWidget has the focus, pass it to another VipDragWidget (if possible)
	VipDragWidgetHandler* handler = VipDragWidgetHandler::find(this->validTopLevelMultiDragWidget()->parentWidget());
	if (handler->focusWidget()) {
		if (this->isAncestorOf(handler->focusWidget())) {
			// bool focus_passed = false;
			QList<VipBaseDragWidget*> lst = handler->baseDragWidgets();
			for (int i = 0; i < lst.size(); ++i)
				if (qobject_cast<VipDragWidget*>(lst[i]) && !lst[i]->isDestroying()) //&& !this->isAncestorOf(lst[i]))
				{
					static_cast<VipDragWidget*>(lst[i])->setFocusWidget();
					// focus_passed = true;
					break;
				}
		}
	}
}

void VipMultiDragWidget::resizeBest()
{
	QList<int> heights;
	int tot_height = 0;
	int tot_width = 0;
	QList<QList<int>> widths;
	for (int y = 0; y < this->mainCount(); ++y) {
		int max_h = 0;
		int _tot_width = 0;
		widths.append(QList<int>());
		for (int x = 0; x < this->subCount(y); ++x) {
			QSize s = this->tabWidget(y, x)->sizeHint();
			max_h = qMax(max_h, s.height());
			_tot_width += s.width();
			widths.last().append(s.width());
		}
		tot_width = qMax(tot_width, _tot_width);
		tot_height += max_h;
		heights.append(max_h);
	}
	this->resize(tot_width, tot_height);
	this->mainSplitter()->setSizes(heights);
	for (int y = 0; y < this->mainCount(); ++y) {
		this->subSplitter(y)->setSizes(widths[y]);
	}
}

void VipMultiDragWidget::setFocusWidget()
{
	VipDragWidgetHandler* handler = VipDragWidgetHandler::find(this->validTopLevelMultiDragWidget()->parentWidget());

	// check if the focus widget is a child of this widget
	if (QWidget* w = handler->focusWidget()) {
		while (w) {
			w = w->parentWidget();
			if (w == this)
				return;
		}
	}

	if (VipBaseDragWidget* w = this->widget(0, 0, 0))
		w->setFocusWidget();
}

QTabWidget* VipMultiDragWidget::createTabWidget()
{
	VipDragTabWidget* tab = new VipDragTabWidget();
	tab->setDocumentMode(true);
	tab->setStyleSheet("QTabWidget::pane { margin: 0px,0px,0px,0px }");
	connect(tab, SIGNAL(currentChanged(int)), this, SLOT(updateContent()), Qt::QueuedConnection);
	onTabWidgetCreated(tab);
	return tab;
}

QSplitter* VipMultiDragWidget::createHSplitter()
{
	Qt::Orientation orientation = this->orientation();
	if (orientation == Qt::Vertical)
		orientation = Qt::Horizontal;
	else
		orientation = Qt::Vertical;
	VipDragWidgetSplitter* h_splitter = new VipDragWidgetSplitter(const_cast<VipMultiDragWidget*>(this), orientation);
	h_splitter->addWidget(createTabWidget());
	QWidget* right = new QWidget();
	h_splitter->addWidget(right);
	right->hide();
	h_splitter->handle(0)->show();
	onSplitterCreated(h_splitter);
	connect(h_splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(receivedSplitterMoved(int, int)));
	return h_splitter;
}

void VipMultiDragWidget::setInternalVisibility(VisibilityState state)
{
	VipBaseDragWidget::setInternalVisibility(state);

	if (state == Minimized)
		this->setMinimumSize(QSize());
	else
		this->setMinimumSize(QSize(200, 200));

	// only propagate visibility if there is one child VipBaseDragWidget
	if (count() == 1) {
		this->widget(0, 0, 0)->setInternalVisibility(state);
	}
}

void VipMultiDragWidget::showMaximized()
{
	// if(visibility() == Maximized)
	//  return;

	if (!supportMaximize())
		return;

	if (visibility() == Minimized) {
		this->setInternalVisibility(Maximized);
		minimizeDragWidget(this, false);
	}

	// save current geometry
	QRect geom;
	if (this->visibility() != Minimized)
		geom = this->geometry();
	else
		// first, restore state
		showNormal();

	if (!isTopLevel()) {
		VipBaseDragWidget::showMaximized();
	}
	else {
		d_data->geometry = geom.isValid() ? geom : geometry();
		if (this->parentWidget()) {
			this->move(0, 0);
			this->resize(this->parentWidget()->size());
		}

		this->setInternalVisibility(Maximized);

		Q_EMIT VipDragWidgetHandler::find(this->parentWidget())->maximized(this);
	}
}

void VipMultiDragWidget::showMinimized()
{
	if (!supportMinimize())
		return;

	if (visibility() != Minimized) {
		if (isMaximized())
			showNormal();
		this->setInternalVisibility(Minimized);
		minimizeDragWidget(this, true);
		return;
	}

	// save current geometry
	QRect geom;
	if (this->visibility() != Minimized)
		geom = this->geometry();

	// first, restore state
	showNormal();

	if (!isTopLevel()) {
		VipBaseDragWidget::showMinimized();
	}
	else {
		d_data->geometry = geom.isValid() ? geom : geometry();
		if (this->parentWidget()) {
			this->setInternalVisibility(Minimized);
			this->reorganizeMinimizedChildren();
		}

		passFocus();
		Q_EMIT VipDragWidgetHandler::find(this->parentWidget())->minimized(this);
	}
}

void VipMultiDragWidget::showNormal()
{
	if (visibility() == Minimized) {
		this->setInternalVisibility(Normal);
		minimizeDragWidget(this, false);
		return;
	}

	if (!isTopLevel()) {
		VipBaseDragWidget::showNormal();
	}
	else {
		if (this->parentWidget()) {
			if (d_data->geometry.isValid())
				this->setGeometry(d_data->geometry); // set the old geometry
		}

		this->setInternalVisibility(Normal);

		Q_EMIT VipDragWidgetHandler::find(this->parentWidget())->restored(this);
	}
}

QSize VipMultiDragWidget::sizeHint() const
{
	QSize res(0, 0);
	QSize empty;

	for (int y = 0; y < mainCount(); ++y) {
		int sum_width = 0;
		int max_height = 0;
		for (int x = 0; x < subCount(y); ++x) {
			QSize tmp = tabWidget(y, x)->sizeHint();
			if (tmp.width() != empty.width())
				sum_width += tmp.width();
			else
				sum_width += 300;
			if (tmp.height() != empty.height())
				max_height = qMax(max_height, tabWidget(y, x)->sizeHint().height());
		}

		if (max_height == 0)
			max_height = 300;
		res.setWidth(qMax(sum_width, res.width()));
		res.setHeight(res.height() + max_height);
	}

	return res;
}

void VipMultiDragWidget::resizeEvent(QResizeEvent*)
{
	if (isTopLevel())
		Q_EMIT VipDragWidgetHandler::find(parentWidget())->geometryChanged(this);
}

bool VipMultiDragWidget::event(QEvent* event)
{
	if (event->type() == QEvent::ParentChange) {
		if (this->isTopLevel() && isMaximized()) {
			// if top level widget maximized, reset the event filter
			this->move(0, 0);
			if (parentWidget())
				this->resize(parentWidget()->size());
		}
	}

	return VipBaseDragWidget::event(event);
}

VipMultiDragWidget* VipMultiDragWidget::fromChild(QWidget* child)
{
	while (child) {
		if (qobject_cast<VipMultiDragWidget*>(child))
			return static_cast<VipMultiDragWidget*>(child);
		child = child->parentWidget();
	}
	return nullptr;
}

QGridLayout* VipMultiDragWidget::mainSplitterLayout() const
{
	// make sure the size grip remains on top;
	// QMetaObject::invokeMethod(d_data->grip,"raise", Qt::QueuedConnection);

	return const_cast<QGridLayout*>(d_data->grid);
}

QSplitter* VipMultiDragWidget::mainSplitter() const
{
	return const_cast<QSplitter*>(d_data->v_splitter);
}

QSplitter* VipMultiDragWidget::subSplitter(int y) const
{
	return static_cast<QSplitter*>(d_data->v_splitter->widget(y));
}

QTabWidget* VipMultiDragWidget::tabWidget(int y, int x) const
{
	return static_cast<QTabWidget*>(subSplitter(y)->widget(x));
}

VipDragWidgetHandle* VipMultiDragWidget::mainSplitterHandle(int y) const
{
	return static_cast<VipDragWidgetHandle*>(d_data->v_splitter->handle(y));
}

VipDragWidgetHandle* VipMultiDragWidget::subSplitterHandle(int y, int x) const
{
	return static_cast<VipDragWidgetHandle*>(subSplitter(y)->handle(x));
}

QTabWidget* VipMultiDragWidget::parentTabWidget(VipBaseDragWidget* w) const
{
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			if (tabWidget(y, x)->indexOf(w) >= 0)
				return tabWidget(y, x);
		}
	}

	return nullptr;
}

VipBaseDragWidget* VipMultiDragWidget::widget(int y, int x, int index) const
{
	if (y < mainCount()) {
		if (x < subCount(y)) {
			QTabWidget* tab = tabWidget(y, x);
			if (index < tab->count())
				return qobject_cast<VipBaseDragWidget*>(tab->widget(index));
		}
	}
	return nullptr;
}

VipDragWidget* VipMultiDragWidget::firstDragWidget() const
{
	for (int y = 0; y < mainCount(); ++y)
		for (int x = 0; x < subCount(y); ++x) {
			auto* t = tabWidget(y, x);
			if (VipBaseDragWidget* b = qobject_cast<VipBaseDragWidget*>(t->currentWidget())) {
				if (VipDragWidget* w = qobject_cast<VipDragWidget*>(b))
					return w;
				else {
					return static_cast<VipMultiDragWidget*>(b)->firstDragWidget();
				}
			}
		}
	return nullptr;
}
VipDragWidget* VipMultiDragWidget::firstVisibleDragWidget() const
{
	for (int y = 0; y < mainCount(); ++y)
		for (int x = 0; x < subCount(y); ++x) {
			auto* t = tabWidget(y, x);
			if (VipBaseDragWidget* b = qobject_cast<VipBaseDragWidget*>(t->currentWidget())) {
				if (VipDragWidget* w = qobject_cast<VipDragWidget*>(b)) {
					if (!w->isMinimized())
						return w;
				}
				else {
					return static_cast<VipMultiDragWidget*>(b)->firstDragWidget();
				}
			}
		}
	return nullptr;
}
VipDragWidget* VipMultiDragWidget::lastDragWidget() const
{
	for (int y = mainCount() - 1; y >= 0; --y)
		for (int x = subCount(y) - 1; x >= 0; --x) {
			auto* t = tabWidget(y, x);
			if (VipBaseDragWidget* b = qobject_cast<VipBaseDragWidget*>(t->currentWidget())) {
				if (VipDragWidget* w = qobject_cast<VipDragWidget*>(b))
					return w;
				else {
					return static_cast<VipMultiDragWidget*>(b)->lastDragWidget();
				}
			}
		}
	return nullptr;
}
VipDragWidget* VipMultiDragWidget::lastVisibleDragWidget() const
{
	for (int y = mainCount() - 1; y >= 0; --y)
		for (int x = subCount(y) - 1; x >= 0; --x) {
			auto* t = tabWidget(y, x);
			if (VipBaseDragWidget* b = qobject_cast<VipBaseDragWidget*>(t->currentWidget())) {
				if (VipDragWidget* w = qobject_cast<VipDragWidget*>(b)) {
					if (!w->isMinimized())
						return w;
				}
				else {
					return static_cast<VipMultiDragWidget*>(b)->lastDragWidget();
				}
			}
		}
	return nullptr;
}

// QWidget * VipMultiDragWidget::GetSizeGrip() const
//  {
//  return const_cast<VipDragWidgetSizeGrip*>(d_data->grip);
//  }

int VipMultiDragWidget::mainCount() const
{
	return d_data->v_splitter->count() - 1;
}

int VipMultiDragWidget::subCount(int y) const
{
	return subSplitter(y)->count() - 1;
}

int VipMultiDragWidget::maxWidth(int* row, int* row_count) const
{
	int res = 0;
	if (row)
		*row = 0;
	for (int i = 0; i < mainCount(); ++i) {
		int w = subCount(i);
		if (w > res) {
			res = w;
			if (row)
				*row = i;
		}
	}

	if (row_count) {
		*row_count = 0;
		for (int i = 0; i < mainCount(); ++i)
			if (subCount(i) == res)
				++(*row_count);
	}
	return res;
}

int VipMultiDragWidget::count() const
{
	int size = 0;
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			// size += tabWidget(y,x)->count();
			// TEST
			QTabWidget* tab = tabWidget(y, x);
			for (int i = 0; i < tab->count(); ++i)
				if (qobject_cast<VipBaseDragWidget*>(tab->widget(i)))
					++size;
		}
	}

	return size;
}

QPoint VipMultiDragWidget::indexOf(VipBaseDragWidget* w, int* index) const
{
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			int i = tabWidget(y, x)->indexOf(w);
			if (i >= 0) {
				if (index)
					*index = i;
				return QPoint(x, y);
			}
		}
	}

	return QPoint(-1, -1);
}

void VipMultiDragWidget::mainResize(int new_size, VipMultiDragWidget::VerticalSide side)
{
	int height = mainCount();
	if (height == new_size)
		return;

	bool maximized = this->isMaximized() || (validTopLevelMultiDragWidget() && validTopLevelMultiDragWidget()->isMaximized());
	QList<int> sizes;
	if (maximized)
		sizes = addNewSplitterSize(d_data->v_splitter, side == Bottom ? mainCount() : 0);

	// remove bottom widget
	QWidget* bottom = d_data->v_splitter->widget(mainCount());
	bottom->setParent(nullptr);

	if (new_size < height) {
		if (side == VipMultiDragWidget::Bottom) {
			while (d_data->v_splitter->count() > new_size) {
				QWidget* w = d_data->v_splitter->widget(d_data->v_splitter->count() - 1);
				w->setParent(nullptr);
				w->close();
			}
		}
		else {
			while (d_data->v_splitter->count() > new_size) {
				QWidget* w = d_data->v_splitter->widget(0);
				w->setParent(nullptr);
				w->close();
			}
		}
	}
	else if (new_size > height) {
		if (side == VipMultiDragWidget::Bottom) {
			while (d_data->v_splitter->count() < new_size)
				d_data->v_splitter->insertWidget(d_data->v_splitter->count(), createHSplitter());
		}
		else {
			while (d_data->v_splitter->count() < new_size)
				d_data->v_splitter->insertWidget(0, createHSplitter());
		}
	}

	d_data->v_splitter->addWidget(bottom);

	if (sizes.size())
		d_data->v_splitter->setSizes(sizes);
	// bottom->hide();
}

void VipMultiDragWidget::subResize(int y, int new_size, VipMultiDragWidget::HorizontalSide side)
{
	QSplitter* h_splitter = subSplitter(y);
	int width = subCount(y);
	if (width == new_size)
		return;

	bool maximized = this->isMaximized() || (validTopLevelMultiDragWidget() && validTopLevelMultiDragWidget()->isMaximized());
	QList<int> sizes;
	if (maximized)
		sizes = addNewSplitterSize(h_splitter, side == Right ? subCount(y) : 0);

	// remove right widget
	QWidget* right = h_splitter->widget(width);
	right->setParent(nullptr);

	if (new_size < width) {
		if (side == VipMultiDragWidget::Right) {
			while (h_splitter->count() > new_size) {
				QWidget* w = h_splitter->widget(h_splitter->count() - 1);
				w->setParent(nullptr);
				w->close();
			}
		}
		else {
			while (h_splitter->count() > new_size) {
				QWidget* w = h_splitter->widget(0);
				w->setParent(nullptr);
				w->close();
			}
		}
	}
	else if (new_size > width) {
		if (side == VipMultiDragWidget::Right) {
			while (h_splitter->count() < new_size)
				h_splitter->insertWidget(h_splitter->count(), createTabWidget());
		}
		else {
			while (h_splitter->count() < new_size)
				h_splitter->insertWidget(0, createTabWidget());
		}
	}

	h_splitter->addWidget(right);
	right->hide();

	if (sizes.size())
		h_splitter->setSizes(sizes);
}

void VipMultiDragWidget::swapWidgets(VipDragWidget* from, VipDragWidget* to)
{
	QPoint ifrom = indexOf(from);
	QPoint ito = indexOf(to);
	if (ifrom == QPoint(-1, -1) || ito == QPoint(-1, -1))
		return;

	QTabWidget* tfrom = tabWidget(ifrom.y(), ifrom.x());
	QTabWidget* tto = tabWidget(ito.y(), ito.x());
	tfrom->removeTab(tfrom->indexOf(from));
	tto->removeTab(tto->indexOf(to));

	tfrom->addTab(to, to->windowIcon(), to->windowTitle());
	tto->addTab(from, from->windowIcon(), from->windowTitle());
}

void VipMultiDragWidget::setWidget(int y, int x, VipBaseDragWidget* widget, bool update_content)
{
	QTabWidget* tab = tabWidget(y, x);
	tab->addTab(widget, widget->windowIcon(), widget->windowTitle());
	widget->setFocusWidget();
	d_data->lastAdded = widget;
	if (update_content) {
		// only update the internal structur if required
		this->updateContent();
	}

	// make sure to apply the parameters
	VipMultiDragWidget* top_level = (this)->validTopLevelMultiDragWidget();
	if (top_level) {
		VipDragWidgetHandler* handler = VipDragWidgetHandler::find(top_level->parentWidget());
		Q_EMIT handler->contentChanged(top_level);
	}
}

void VipMultiDragWidget::updateSizes(bool enable_resize)
{
	if (enable_resize) {
		if (this->count() <= 1)
			resize(sizeHint());
		else if (d_data->lastAdded) {
			// recompute the width OR the height depending on where was added the last widget with setWidget()
			QPoint pos = this->indexOf(d_data->lastAdded);
			if (pos != QPoint(-1, -1)) {
				int _height = mainCount();
				int _width = subCount(pos.y());

				if (_width == 1) {
					// we added the widget in a new row: recompute only the height.
					// compute the average height
					double h = 0;
					for (int y = 0; y < _height; ++y)
						h += subSplitter(y)->height();
					h /= (_height - 1);
					this->resize(width(), (h + 5) * mainCount());
				}
				else {
					int row_count;
					int max_width = maxWidth(nullptr, &row_count);
					// we added the widget in an existing row: only recompute the width IF this is the row with the most elements
					if (_width == max_width && row_count == 1) {
						// compute the average width for this row
						double w = 0;
						for (int x = 0; x < _width; ++x)
							w += tabWidget(pos.y(), x)->width();
						w /= (_width - 1);
						this->resize((w)*_width, height());
					}
				}
			}
			d_data->lastAdded = nullptr;
		}
	}

	QList<int> h_sizes;
	int h_total_size = 0;

	// try to resize splitters correcly
	for (int y = 0; y < mainCount(); ++y) {
		QSplitter* splitter = this->subSplitter(y);
		QList<int> sizes;
		int total_size = 0;
		for (int x = 0; x < subCount(y); ++x) {
			QTabWidget* tab = tabWidget(y, x);

			// apply recursively
			for (int i = 0; i < tab->count(); ++i)
				if (qobject_cast<VipMultiDragWidget*>(tab->widget(i)))
					static_cast<VipMultiDragWidget*>(tab->widget(i))->updateSizes(false);

			QSize s = tab->sizeHint();
			sizes << s.width();
			total_size += s.width();
		}

		double factor = splitter->width() / double(total_size);
		for (int i = 0; i < sizes.size(); ++i)
			sizes[i] = sizes[i] * factor;

		splitter->setSizes(sizes);

		QSize s = splitter->sizeHint();
		h_sizes << s.height();
		h_total_size += s.height();
	}

	double factor = d_data->v_splitter->height() / double(h_total_size);
	for (int i = 0; i < h_sizes.size(); ++i)
		h_sizes[i] = h_sizes[i] * factor;
	d_data->v_splitter->setSizes(h_sizes);

	// TEST: comment this
	// vipProcessEvents(nullptr, 500);
}

bool VipMultiDragWidget::insertSub(int y, int x, VipBaseDragWidget* widget)
{
	QSplitter* h_splitter = subSplitter(y);
	// check
	if (x < h_splitter->count()) {
		if (QTabWidget* tab = qobject_cast<QTabWidget*>(h_splitter->widget(x)))
			if (tab->indexOf(widget) >= 0)
				return false;
	}

	QTabWidget* tab = createTabWidget();
	h_splitter->insertWidget(x, tab);
	setWidget(y, x, widget);
	return true;
}

bool VipMultiDragWidget::insertMain(int y, VipBaseDragWidget* widget)
{
	if (y < d_data->v_splitter->count()) {
		// check if widget is already at the right location
		if (QSplitter* splitter = qobject_cast<QSplitter*>(d_data->v_splitter->widget(y))) {
			for (int i = 0; i < splitter->count(); ++i)
				if (QTabWidget* tab = qobject_cast<QTabWidget*>(splitter->widget(i)))
					if (tab->indexOf(widget) >= 0)
						return false;
		}
	}
	QSplitter* h_splitter = createHSplitter();
	d_data->v_splitter->insertWidget(y, h_splitter);
	setWidget(y, 0, widget);
	return true;
}

void VipMultiDragWidget::hideAllExcept(VipBaseDragWidget* widget)
{
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			QTabWidget* tab = this->tabWidget(y, x);
			int index = tab->indexOf(widget);
			if (index < 0) {
				// check that widgets all support HideOnMaximize
				bool support_hide = true;
				for (int i = 0; i < tab->count(); ++i) {
					if (VipBaseDragWidget* w = qobject_cast<VipBaseDragWidget*>(tab->widget(i))) {

						if (w->testSupportedOperation(VipBaseDragWidget::NoHideOnMaximize)) {
							support_hide = false;
							break;
						}
						else if (w->visibility() == VipBaseDragWidget::Maximized)
							w->setInternalVisibility(VipBaseDragWidget::Normal);
					}
				}
				if (support_hide)
					tab->hide();
			}
			else {
				tab->show();
				tab->setCurrentIndex(index);
			}
		}
	}
}

void VipMultiDragWidget::showAll()
{
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			QTabWidget* tab = this->tabWidget(y, x);
			tab->show();
		}
	}
}

void VipMultiDragWidget::reorganizeGrid()
{
	int h = d_data->v_splitter->height() - (d_data->v_splitter->count() - 1) * 5;
	int row_h = qRound((double)h / mainCount());

	QList<int> v_sizes;
	for (int i = 0; i < mainCount(); ++i)
		v_sizes.append(row_h);
	d_data->v_splitter->setSizes(v_sizes);

	for (int i = 0; i < mainCount(); ++i) {
		QSplitter* hsplitter = this->subSplitter(i);
		int w = hsplitter->width() - (hsplitter->count() - 1) * 5;
		int col_w = qRound((double)w / subCount(i));
		QList<int> h_sizes;
		for (int j = 0; j < subCount(i); ++j)
			h_sizes.append(col_w);
		hsplitter->setSizes(h_sizes);
	}
}

void VipMultiDragWidget::startRender(VipRenderState& state)
{
	// remove borders
	state.state(this)["style_sheet"] = this->styleSheet();
	this->setStyleSheet("VipMultiDragWidget {border: 0 px;}");

	// call startRender() for all sub VipBaseDragWidget
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			VipDragTabWidget* tab = static_cast<VipDragTabWidget*>(tabWidget(y, x));
			if (tab->count() > 1)
				tab->TabBar()->hide();
			for (int i = 0; i < tab->count(); ++i) {
				if (VipBaseDragWidget* w = widget(y, x, i))
					w->startRender(state);
			}
		}
	}
}

void VipMultiDragWidget::endRender(VipRenderState& state)
{
	// reset borders
	this->setStyleSheet(state.state(this)["style_sheet"].value<QString>());

	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {
			VipDragTabWidget* tab = static_cast<VipDragTabWidget*>(tabWidget(y, x));
			if (tab->count() > 1)
				tab->TabBar()->show();
			for (int i = 0; i < tabWidget(y, x)->count(); ++i) {
				if (VipBaseDragWidget* w = widget(y, x, i))
					w->endRender(state);
			}
		}
	}
}

void VipMultiDragWidget::updateContent()
{
	bool content_changed = false;
	// remove empty tab widget
	for (int y = 0; y < mainCount(); ++y) {
		for (int x = 0; x < subCount(y); ++x) {

			if (tabWidget(y, x)->count() == 0) {
				delete tabWidget(y, x);
				content_changed = true;
				--x;
			}
		}

		if (subSplitter(y)->count() == 1) {
			delete subSplitter(y);
			content_changed = true;
			--y;
		}
	}

	if (validTopLevelMultiDragWidget()->visibility() == Normal)
		validTopLevelMultiDragWidget()->updateSizes(true);

	// make sure all splitter handles are visible.
	// this should not be necessary, but somehow the style sheet mess up with
	// the splitter visibility.
	for (int y = 0; y < mainCount(); ++y)
		d_data->v_splitter->handle(y)->QSplitterHandle::setVisible(true);
	for (int y = 0; y < mainCount(); ++y) {
		QSplitter* splitter = subSplitter(y);
		for (int x = 0; x <= subCount(y); ++x)
			splitter->handle(x)->QSplitterHandle::setVisible(true);
	}

	if (d_data->v_splitter->count() == 1)
		this->deleteLater();
	else {

		// show/hide title widget
		if (d_data->header) {
			if (count() > 1)
				d_data->header->show();
			else
				d_data->header->hide();
		}
	}

	Q_EMIT contentChanged();

	if (content_changed) {
		// if we removed a tab, emit the signal contentChanged
		if (VipMultiDragWidget* top_level = (this)->validTopLevelMultiDragWidget()) {
			VipDragWidgetHandler* handler = VipDragWidgetHandler::find(top_level->parentWidget());
			// update focus widget
			if (!handler->focusWidget()) {
				for (int y = 0; y < mainCount(); ++y)
					for (int x = 0; x < subCount(y); ++x) {
						if (widget(y, x, 0)) {
							widget(y, x, 0)->setFocusWidget();
							break;
						}
					}
			}
			Q_EMIT handler->contentChanged(top_level);
		}
	}
}

void VipMultiDragWidget::receivedSplitterMoved(int pos, int index)
{
	Q_EMIT splitterMoved(qobject_cast<QSplitter*>(sender()), pos, index);
}

void VipMultiDragWidget::reorganizeMinimizedChildren()
{
	QList<VipMinimizeWidget*> minimized = validTopLevelMultiDragWidget()->findChildren<VipMinimizeWidget*>();

	for (VipMinimizeWidget* m : minimized)
		m->reorganize();
}

void VipMultiDragWidget::focusChanged(QWidget* old_w, QWidget* new_w)
{
	Q_UNUSED(old_w)

	// raise the right VipMultiDragWidget
	if (this->isAncestorOf(new_w)) {
		VipMultiDragWidget* top_level = this->validTopLevelMultiDragWidget();
		top_level->raise();

		// find the VipDragWidget just above the focus widget
		while (new_w) {
			if (qobject_cast<VipDragWidget*>(new_w)) {
				// we found it: remove the focus to all related VipDragWidget and set the focus to this VipDragWidget, emit focusChanged()
				static_cast<VipDragWidget*>(new_w)->setFocusWidget();
				break;
			}
			else
				new_w = new_w->parentWidget();
		}
	}
}

VipMultiDragWidget* VipMultiDragWidget::create(QWidget* parent) const
{
	VipMultiDragWidget* res = new VipMultiDragWidget(parent);
	res->setSupportedOperations(this->supportedOperations());

	return res;
}

bool VipMultiDragWidget::supportReparent(QWidget* new_parent)
{
	if (_reparent_function)
		return _reparent_function(this, new_parent);
	return true;
}

VipBaseDragWidget* VipMultiDragWidget::createFromMimeData(const QMimeData* mime_data)
{
	if (mime_data->hasFormat("application/dragwidget")) {
		// check that the widget supports Drop operation
		const VipBaseDragWidgetMimeData* mime = static_cast<const VipBaseDragWidgetMimeData*>(mime_data);
		if (!mime->dragWidget->isDropable())
			return nullptr;
		else
			return mime->dragWidget;
	}
	else {
		QMimeData* mime = const_cast<QMimeData*>(mime_data);
		const auto lst = vipDropMimeData().match(mime, this);
		if (lst.size())
			return lst.back()(mime, this).value<VipBaseDragWidget*>();
	}
	return nullptr;
}

bool VipMultiDragWidget::supportDrop(const QMimeData* mime_data)
{
	if (mime_data->hasFormat("application/dragwidget")) {
		const VipBaseDragWidgetMimeData* mime = static_cast<const VipBaseDragWidgetMimeData*>(mime_data);
		return mime->dragWidget->isDropable();
	}
	else {
		QMimeData* mime = const_cast<QMimeData*>(mime_data);
		const auto lst = vipAcceptDragMimeData().match(mime, this);
		if (lst.size())
			return lst.back()(mime, this).value<bool>();
	}
	return false;
}

void VipMultiDragWidget::moveEvent(QMoveEvent* event)
{
	VipBaseDragWidget::moveEvent(event);

	// make sure that any drag/drop/optional widget deletion operations has been performed before emitting the signal
	// QCoreApplication::processEvents();
	if (isTopLevel())
		Q_EMIT VipDragWidgetHandler::find(parentWidget())->geometryChanged(this);
}

void VipMultiDragWidget::closeEvent(QCloseEvent* evt)
{
	if (isTopLevel())
		Q_EMIT VipDragWidgetHandler::find(parentWidget())->closed(this);
	VipBaseDragWidget::closeEvent(evt);
}




VipViewportArea::VipViewportArea()
  : QWidget()
  , VipRenderObject(this)
{
	this->setAcceptDrops(true);
	this->setObjectName("viewport_area");
}

void VipViewportArea::dragEnterEvent(QDragEnterEvent* evt)
{
	if (evt->mimeData()->data("application/dragwidget").size())
		evt->acceptProposedAction();
	else {
		QMimeData* mime = const_cast<QMimeData*>(evt->mimeData());
		const auto lst = vipAcceptDragMimeData().match(mime, this);
		if (lst.size() && lst.back()(mime, this).value<bool>())
			evt->acceptProposedAction();
		else if (evt->mimeData()->hasUrls())
			evt->acceptProposedAction();
	}
}

void VipViewportArea::dragMoveEvent(QDragMoveEvent* evt)
{
	if (evt->mimeData()->data("application/dragwidget").size())
		evt->acceptProposedAction();
	else {
		QMimeData* mime = const_cast<QMimeData*>(evt->mimeData());
		const auto lst = vipAcceptDragMimeData().match(mime, this);
		if (lst.size() && lst.back()(mime, this).value<bool>())
			evt->acceptProposedAction();
		else if (mime->hasUrls())
			evt->acceptProposedAction();
	}
}

void VipViewportArea::dropMimeData(const QMimeData* mimeData, const QPoint& pos)
{
	// check that we drop something else than the usual VipBaseDragWidget, and handle it
	QMimeData* mime = const_cast<QMimeData*>(mimeData);
	const auto lst = vipDropMimeData().match(mime, this);
	if (lst.size()) {
		if (VipBaseDragWidget* widget = lst.back()(mime, this).value<VipBaseDragWidget*>()) {
			VipDragWidgetArea* area = VipDragWidgetArea::fromChildWidget(this);
			if (qobject_cast<VipMultiDragWidget*>(widget)) {
				widget->setParent(this);
				widget->move(pos);
				widget->show();
			}
			else {
				VipMultiDragWidget* top_level = area->createMultiDragWidget();
				top_level->setWidget(0, 0, widget);
				top_level->setParent(this);
				top_level->show();
				top_level->move(pos);
			}
		}
	}
	else if (mime->hasUrls()) {
		// use the standard handing: retrieve the text from the mime data and emit VipDragWidgetArea::textDropped

		// find the parent VipDragWidgetArea
		VipDragWidgetArea* area = nullptr;
		QWidget* parent = parentWidget();
		while (parent) {
			if (VipDragWidgetArea* tmp = qobject_cast<VipDragWidgetArea*>(parent)) {
				area = tmp;
				break;
			}
			parent = parent->parentWidget();
		}

		QList<QUrl> urls = mime->urls();
		QStringList files;
		for (int i = 0; i < urls.size(); ++i) {
			files << urls[i].toString();
			files.last().remove("file:///");
			vip_debug("%s\n", files.last().toLatin1().data());
		}

		if (area)
			Q_EMIT area->textDropped(files, pos);
	}

	update();
}

void VipViewportArea::dropEvent(QDropEvent* evt)
{
	dropMimeData(evt->mimeData(), evt->VIP_EVT_POSITION());
}

VipDragWidgetArea::VipDragWidgetArea(QWidget* parent)
  : QWidget(parent)
{
	/* setWidget*/(d_area = new VipViewportArea());
	d_area->setParent(this);
	d_area->move(0, 0);
	d_area->resize(this->size());
	
	connect(VipDragWidgetHandler::find(widget()), SIGNAL(geometryChanged(VipMultiDragWidget*)), this, SLOT(recomputeSize()), Qt::QueuedConnection);
	connect(VipDragWidgetHandler::find(widget()), SIGNAL(moving(VipMultiDragWidget*)), this, SLOT(moving(VipMultiDragWidget*)), Qt::QueuedConnection);

	// disable scroll bars
	//setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	d_area->installEventFilter(this);
}

VipDragWidgetArea::~VipDragWidgetArea()
{
	d_area->removeEventFilter(this);
}

bool VipDragWidgetArea::eventFilter(QObject*, QEvent* event)
{
	if (event->type() == QEvent::MouseButtonPress) {
		Q_EMIT mousePressed(((int)static_cast<QMouseEvent*>(event)->button()));
	}
	else if (event->type() == QEvent::MouseButtonRelease) {
		Q_EMIT mouseReleased(((int)static_cast<QMouseEvent*>(event)->button()));
	}
	else if (event->type() == QEvent::KeyPress) {
		// ignore key event to propagate arrow press (which are, by default, catch by QScrollArea
		event->ignore();
		return true;
	}
	return false;
}

void VipDragWidgetArea::resizeEvent(QResizeEvent* evt)
{
	d_area->move(0, 0);
	d_area->resize(this->size());
	recomputeSize();
	//QScrollArea::resizeEvent(evt);
}

void VipDragWidgetArea::keyPressEvent(QKeyEvent* evt)
{
	// ignore key event to propagate arrow press (which are, by default, catch by QScrollArea
	evt->ignore();
}

VipDragWidgetArea* VipDragWidgetArea::fromChildWidget(QWidget* child)
{
	QWidget* tmp = child;

	while (tmp && !qobject_cast<VipDragWidgetArea*>(tmp))
		tmp = tmp->parentWidget();

	return qobject_cast<VipDragWidgetArea*>(tmp);
}

void VipDragWidgetArea::recomputeSize()
{
	// compute the union of all geometry
	VipDragWidgetHandler* handler = VipDragWidgetHandler::find(d_area);
	QList<VipMultiDragWidget*> mdrags = handler->topLevelMultiDragWidgets();
	QRect rect;
	QList<VipMultiDragWidget*> maximized;

	for (int i = 0; i < mdrags.size(); ++i) {
		VipMultiDragWidget* mw = mdrags[i];

		if (mw->isMaximized())
			maximized.append(mw);

		if (!mw->isMinimized()) {
			QRect geom = mdrags[i]->geometry();
			rect |= geom;
		}
	}

	// position offset to apply to each top level VipMultiDragWidget
	QPoint offset(qMax(0, -rect.left()), qMax(0, -rect.top()));
	if (offset != QPoint(0, 0)) {
		for (int i = 0; i < mdrags.size(); ++i)
			mdrags[i]->move(mdrags[i]->pos() + offset);

		return recomputeSize();
	}

	if (maximized.size()) {
		
		d_area->move(0, 0);
		d_area->resize(size());
		// TODO: also resize the maximized VipMultiDragWidget
		for (int i = 0; i < maximized.size(); ++i) {
			maximized[i]->move(0, 0);
			maximized[i]->resize(size());
		}
	}
	else {
		/* if (width() >= rect.right())
			this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		else
			this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

		if (height() >= rect.bottom())
			this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		else
			this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		*/
		// compute the preferred size
		QSize preferred(qMax(rect.right(), width()), qMax(rect.bottom(), height()));
		d_area->resize(preferred);
	}
}

void VipDragWidgetArea::dropMimeData(const QMimeData* mime, const QPoint& pos)
{
	this->widget()->dropMimeData(mime, pos);
}

void VipDragWidgetArea::moving(VipMultiDragWidget* widget)
{
	Q_UNUSED(widget)

	// get the position in this widget coordinate system, and move the scroll bars if we are close to a border
	//QPoint pos = this->mapFromGlobal(QCursor::pos());
	//int vipDistance = 50;

	/* if (pos.x() < vipDistance) {
		// left border
		this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - 10);
	}
	else if (pos.x() > width() - vipDistance) {
		// right border
		this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() + 10);
	}

	if (pos.y() < vipDistance) {
		// top border
		this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - 10);
	}
	else if (pos.y() > height() - vipDistance) {
		// bottom border
		this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() + 10);
	}*/
}

VipFunctionDispatcher<2>& vipAcceptDragMimeData()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& vipDropMimeData()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& vipSetDragWidget()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

//
// VipMultiDragWidget * target_parent = target->parentMultiDragWidget();
// VipMultiDragWidget * drag_parent = drag->parentMultiDragWidget();
// if (!drag_parent)
// drag_parent = qobject_cast<VipMultiDragWidget*>(drag);
// if (!drag_parent) {
// //ERROR
// return;
// }
// //save current position of drag parent
// QPoint drag_pos = drag_parent->pos();
// VipBaseDragWidget * to_move = nullptr;
//
// //is the target a top level VipMultiDragWidget?
// if (target_parent->isTopLevel()) {
// if (target_parent->count() == 1) {
//	//move
//	drag_parent->setParent(target_parent->parent());
//	drag_parent->move(target_parent->pos());
//	to_move = target_parent;
// }
// else {
//	QPoint p = target_parent->indexOf(target);
//	target_parent->subSplitter(p.y())->insr
// }
//
// }
// }

#include "VipArchive.h"
#include "VipUniqueId.h"
#include <qtextstream.h>

VipArchive& operator<<(VipArchive& ar, VipBaseDragWidget* w)
{
	// save the title without the unique id
	QString title = w->windowTitle();
	QTextStream str(&title, QIODevice::ReadOnly);
	int id;
	str >> id;
	if (str.status() == QTextStream::Ok && id == VipUniqueId::id(w) && str.read(1) == " ")
		title = str.readAll();

	ar.content("id", VipUniqueId::id<VipBaseDragWidget>(w));
	ar.content("title", w->windowTitle());
	ar.content("operations", (int)w->supportedOperations());
	ar.content("visibility", (int)w->visibility());
	return ar;
}

VipArchive& operator>>(VipArchive& ar, VipBaseDragWidget* w)
{
	VipUniqueId::setId<VipBaseDragWidget>(w, ar.read("id").toInt());
	w->setWindowTitle(ar.read("title").toString());
	w->setSupportedOperations((VipBaseDragWidget::Operations)ar.read("operations").toInt());
	if (!w->parentMultiDragWidget())
		w->setInternalVisibility((VipBaseDragWidget::VisibilityState)ar.read("visibility").toInt());
	else
		w->setVisibility((VipBaseDragWidget::VisibilityState)ar.read("visibility").toInt());
	return ar;
}

VipArchive& operator<<(VipArchive& ar, VipDragWidget* w)
{
	ar.content(w->widget());
	return ar;
}

VipArchive& operator>>(VipArchive& ar, VipDragWidget* w)
{
	QWidget* widget = ar.read().value<QWidget*>();
	if (widget)
		w->setWidget(widget);
	return ar;
}

VipArchive& operator<<(VipArchive& ar, VipMultiDragWidget* w)
{
	ar.content("pos", w->pos());
	ar.content("size", w->size());
	ar.content("saved_geometry", w->d_data->geometry);
	ar.content("state", w->mainSplitter()->saveState());
	ar.content("height", w->mainCount());
	ar.content("visibility", (int)w->visibility());
	ar.content("orientation", (int)w->orientation());

	for (int h = 0; h < w->mainCount(); ++h) {
		ar.start("row");

		ar.content("state", w->subSplitter(h)->saveState());
		ar.content("width", w->subCount(h));
		for (int i = 0; i < w->subCount(h); ++i) {
			QTabWidget* tab = w->tabWidget(h, i);

			ar.start("tab");
			ar.content("count", tab->count());
			ar.content("current", tab->currentIndex());

			for (int t = 0; t < tab->count(); ++t)
				ar.content(tab->widget(t));

			ar.end();
		}

		ar.end();
	}
	return ar;
}

VipArchive& operator>>(VipArchive& ar, VipMultiDragWidget* w)
{
	QPoint pos = (ar.read("pos").value<QPoint>());
	QSize size = (ar.read("size").value<QSize>());
	QRect saved_geometry = ar.read("saved_geometry").toRect();
	// w->resize(size);
	QByteArray hstate = ar.read("state").toByteArray();
	int height = ar.read("height").toInt();
	VipMultiDragWidget::VisibilityState visibility = (VipMultiDragWidget::VisibilityState)ar.read("visibility").toInt();

	int orientation = 0;
	ar.save();
	if (ar.content("orientation", orientation))
		w->setOrientation((Qt::Orientation)orientation);
	else
		ar.restore();

	// save all visibility sates, and reapply them after loading
	QMap<VipBaseDragWidget*, VipBaseDragWidget::VisibilityState> visibility_states;

	for (int h = 0; h < height; ++h) {
		ar.start("row");

		QByteArray wstate = ar.read("state").toByteArray();
		int width = ar.read("width").toInt();

		w->mainResize(h + 1);
		for (int i = 0; i < width; ++i) {
			w->subResize(h, i + 1);
			ar.start("tab");
			int count = ar.read("count").toInt();
			int current = ar.read("current").toInt();
			QTabWidget* tab = w->tabWidget(h, i);

			// block the tab signals to avoid calling updateContent() which might destroy the tab if empty
			tab->blockSignals(true);
			for (int t = 0; t < count; ++t) {
				VipBaseDragWidget* widget = ar.read().value<VipBaseDragWidget*>();
				if (widget) {
					w->setWidget(h, i, widget);
					visibility_states[widget] = widget->visibility();
				}
				else {
					VIP_LOG_ERROR(ar.errorString());
					vip_debug("%s\n", ar.errorString().toLatin1().data());
				}
			}
			tab->setCurrentIndex(current);
			tab->blockSignals(false);
			ar.end();
		}
		w->subSplitter(h)->restoreState(wstate);

		ar.end();
	}
	w->mainSplitter()->restoreState(hstate);

	// reapply visibility
	for (QMap<VipBaseDragWidget*, VipBaseDragWidget::VisibilityState>::iterator it = visibility_states.begin(); it != visibility_states.end(); ++it) {
		// if (it.value() == VipBaseDragWidget::Minimized)
		//  QMetaObject::invokeMethod(it.key(), "showMinimized", Qt::QueuedConnection);
		//  else if (it.value() == VipBaseDragWidget::Maximized)
		//  QMetaObject::invokeMethod(it.key(), "showMaximized", Qt::QueuedConnection);
		//  else
		//  QMetaObject::invokeMethod(it.key(), "showNormal", Qt::QueuedConnection);
		VipBaseDragWidget* k = it.key();
		VipBaseDragWidget::VisibilityState v = it.value();
		k->setVisibility(v);
	}

	// resize using QueuedConnection because updateContent() function is already scheduled and will use sizeHint()
	if ((VipMultiDragWidget::VisibilityState)visibility == VipMultiDragWidget::Minimized) {
		w->move(saved_geometry.topLeft());
		QMetaObject::invokeMethod(w, "setSize", Qt::QueuedConnection, Q_ARG(QSize, saved_geometry.size()));
		QMetaObject::invokeMethod(w, "showMinimized", Qt::QueuedConnection);
	}
	else if ((VipMultiDragWidget::VisibilityState)visibility == VipMultiDragWidget::Maximized) {
		w->move(saved_geometry.topLeft());
		QMetaObject::invokeMethod(w, "setSize", Qt::QueuedConnection, Q_ARG(QSize, saved_geometry.size()));
		QMetaObject::invokeMethod(w, "showMaximized", Qt::QueuedConnection);
	}
	else {
		w->move(pos);
		QMetaObject::invokeMethod(w, "setSize", Qt::QueuedConnection, Q_ARG(QSize, size));
	}

	QMetaObject::invokeMethod(w, "reorganizeMinimizedChildren", Qt::QueuedConnection);

	return ar;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipBaseDragWidget*>();
	vipRegisterArchiveStreamOperators<VipDragWidget*>();
	vipRegisterArchiveStreamOperators<VipMultiDragWidget*>();
	return 0;
}
static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);
