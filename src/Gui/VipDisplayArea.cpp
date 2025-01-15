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

#include "VipDisplayArea.h"
#include "VipAbout.h"
#include "VipAnnotationEditor.h"
#include "VipAxisColorMap.h"
#include "VipCommandOptions.h"
#include "VipDisplayObject.h"
#include "VipDragWidget.h"
#include "VipDrawShape.h"
#include "VipEditXMLSymbols.h"
#include "VipEnvironment.h"
#include "VipFileSystem.h"
#include "VipGui.h"
#include "VipIODevice.h"
#include "VipLogConsole.h"
#include "VipOptions.h"
#include "VipPlayWidget.h"
#include "VipPlayer.h"
#include "VipPlugin.h"
#include "VipProcessingObjectEditor.h"
#include "VipProcessingObjectInfo.h"
#include "VipProgress.h"
#include "VipRecordToolWidget.h"
#include "VipSet.h"
#include "VipStandardEditors.h"
#include "VipStandardWidgets.h"
#include "VipToolWidget.h"
#include "VipUniqueId.h"
#include "VipUpdate.h"
#include "VipWidgetResizer.h"
#include "VipXmlArchive.h"
#include "VipSearchLineEdit.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QProgressBar>
#include <QScreen>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <qclipboard.h>
#include <qlabel.h>
#include <qprintdialog.h>
#include <qprinter.h>
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#include <QDesktopWidget>
#endif
#include <QShortcut>

#ifdef VIP_WITH_VTK
#include <vtkFileOutputWindow.h>
#include <vtkOutputWindow.h>
#include "VipFieldOfViewEditor.h"
#include "VipVTKPlayer.h"
#endif

class VipDisplayTabBar::PrivateData
{
public:
	PrivateData()
	  : tabWidget(nullptr)
	  , timer(nullptr)
	  , dragIndex(-1)
	  , closeIcon(vipIcon("close.png"))
	  , floatIcon(vipIcon("pin.png"))
	  , hoverCloseIcon(vipIcon("close.png"))
	  , hoverFloatIcon(vipIcon("pin.png"))
	  , selectedCloseIcon(vipIcon("close.png"))
	  , selectedFloatIcon(vipIcon("pin.png"))
	  , hoverIndex(-1)
	  , streamingButtonEnabled(true)
	  , dirtyStreamingButton(false)
	{
	}
	VipDisplayTabWidget* tabWidget;
	QTimer* timer;
	int dragIndex;

	QIcon closeIcon;
	QIcon floatIcon;
	QIcon hoverCloseIcon;
	QIcon hoverFloatIcon;
	QIcon selectedCloseIcon;
	QIcon selectedFloatIcon;
	int hoverIndex;
	bool streamingButtonEnabled;
	bool dirtyStreamingButton;
};

VipDisplayTabBar::VipDisplayTabBar(VipDisplayTabWidget* parent)
  : QTabBar(parent)
{
	// add the tab that allows to append new tabs
	setAcceptDrops(true);
	// setMaximumHeight(30);

	setIconSize(QSize(18, 18));
	setMouseTracking(true);

	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->tabWidget = parent;
	d_data->timer = new QTimer(this);
	d_data->timer->setSingleShot(true);
	d_data->timer->setInterval(500);
	connect(d_data->timer, SIGNAL(timeout()), this, SLOT(dragLongEnough()));
	connect(this, SIGNAL(currentChanged(int)), this, SLOT(updateIcons()));

	// parent->addTab(new QWidget(), "+");
	// addTab("+");//vipIcon("add_page.png"),QString());
}

VipDisplayTabBar::~VipDisplayTabBar()
{
	d_data->timer->stop();
}

QIcon VipDisplayTabBar::closeIcon() const
{
	return d_data->closeIcon;
}
void VipDisplayTabBar::setCloseIcon(const QIcon& i)
{
	d_data->closeIcon = i;
	updateIcons();
}

QIcon VipDisplayTabBar::floatIcon() const
{
	return d_data->floatIcon;
}
void VipDisplayTabBar::setFloatIcon(const QIcon& i)
{
	d_data->floatIcon = i;
	updateIcons();
}

QIcon VipDisplayTabBar::hoverCloseIcon() const
{
	return d_data->hoverCloseIcon;
}
void VipDisplayTabBar::setHoverCloseIcon(const QIcon& i)
{
	d_data->hoverCloseIcon = i;
	updateIcons();
}

QIcon VipDisplayTabBar::hoverFloatIcon() const
{
	return d_data->hoverFloatIcon;
}
void VipDisplayTabBar::setHoverFloatIcon(const QIcon& i)
{
	d_data->hoverFloatIcon = i;
	updateIcons();
}

QIcon VipDisplayTabBar::selectedCloseIcon() const
{
	return d_data->selectedCloseIcon;
}
void VipDisplayTabBar::setSelectedCloseIcon(const QIcon& i)
{
	d_data->selectedCloseIcon = i;
	updateIcons();
}

QIcon VipDisplayTabBar::selectedFloatIcon() const
{
	return d_data->selectedFloatIcon;
}
void VipDisplayTabBar::setSelectedFloatIcon(const QIcon& i)
{
	d_data->selectedFloatIcon = i;
	updateIcons();
}

VipDisplayTabWidget* VipDisplayTabBar::displayTabWidget() const
{
	return const_cast<VipDisplayTabWidget*>(d_data->tabWidget);
}

// QToolBar * VipDisplayTabBar::tabButtons(int index) const
//  {
//  if (index < 0)
//  index = this->currentIndex();
//  if (index < 0)
//  return nullptr;
//  return qobject_cast<QToolBar*>(tabButton(index, QTabBar::RightSide));
//  }
//
//  QToolButton * VipDisplayTabBar::floatButton(int index) const
//  {
//  if (index < 0)
//  index = this->currentIndex();
//  if (index < 0)
//  return nullptr;
//
//  QWidget * w = tabButton(index, QTabBar::RightSide);
//  return w->findChild<QToolButton*>("float_workspace");
//  }
//
//  QToolButton * VipDisplayTabBar::closeButton(int index) const
//  {
//  if (index < 0)
//  index = this->currentIndex();
//  if (index < 0)
//  return nullptr;
//
//  QWidget * w = tabButton(index, QTabBar::RightSide);
//  return w->findChild<QToolButton*>("close_workspace");
//  }
//
//  QToolButton * VipDisplayTabBar::streamingButton(int index) const
//  {
//  if (index < 0)
//  index = this->currentIndex();
//  if (index < 0)
//  return nullptr;
//
//  QWidget * w = tabButton(index, QTabBar::LeftSide);
//  return qobject_cast<QToolButton*>(w);
//  }

void VipDisplayTabBar::setStreamingEnabled(bool enable)
{
	if (d_data->streamingButtonEnabled != enable) {
		d_data->streamingButtonEnabled = enable;
		updateStreamingButton();
	}
}
bool VipDisplayTabBar::streamingButtonEnabled() const
{
	return d_data->streamingButtonEnabled;
}

void VipDisplayTabBar::enableStreaming()
{
	//
	// Enable/disable streaming for the player area
	//

	if (sender())
		if (VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(sender()->property("widget").value<QWidget*>())) {
			VipProcessingPool* pool = area->processingPool();
			pool->setStreamingEnabled(!pool->isStreamingEnabled());
			updateStreamingButton();
		}
}

// static QToolButton * findButton(const QList<QWidget*> & ws, const QString & name)
//  {
//  for (int i = 0; i < ws.size(); ++i)
//  if (ws[i]->objectName() == name)
//	if (QToolButton * b = qobject_cast<QToolButton*>(ws[i]))
//		return b;
//  return nullptr;
//  }

void VipDisplayTabBar::updateStreamingButtonDelayed()
{
	if (!d_data->dirtyStreamingButton) {
		d_data->dirtyStreamingButton = true;
		QMetaObject::invokeMethod(this, "updateStreamingButton", Qt::QueuedConnection);
	}
}

void VipDisplayTabBar::updateStreamingButton()
{
	d_data->dirtyStreamingButton = false;
	// update ALL streaming buttons
	VipDisplayArea* area = nullptr;
	QWidget* w = parentWidget();
	while (w) {
		if ((area = qobject_cast<VipDisplayArea*>(w)))
			break;
		w = w->parentWidget();
	}
	if (!area)
		return;

	// reset last tab ('+' tab)
	this->setTabButton(this->count() - 1, QTabBar::LeftSide, nullptr);
	this->setTabButton(this->count() - 1, QTabBar::RightSide, nullptr);

	for (int i = 0; i < area->count(); ++i) {
		VipDisplayPlayerArea* a = area->displayPlayerArea(i);
		// Reset left and right tab widgets, as they might be strucked on the last tab (with the '+')
		if (!a->leftTabWidget())
			a->setLeftTabWidget(new QToolBar());
		else
			a->setLeftTabWidget(a->leftTabWidget());
		if (a->rightTabWidget())
			a->setRightTabWidget(a->rightTabWidget());
		a->leftTabWidget()->setIconSize(QSize(18, 18));
		QToolButton* stream = a->leftTabWidget()->findChild<QToolButton*>("stream_workspace");
		VipProcessingPool* pool = a->processingPool();
		if (!pool->hasSequentialDevice() || !streamingButtonEnabled()) {
			if (stream)
				delete stream;
		}
		else {
			if (!stream) {
				stream = new QToolButton();
				stream->setProperty("widget", QVariant::fromValue(a));
				stream->setIcon(vipIcon("streaming_on.png"));
				stream->setAutoRaise(true);
				stream->setToolTip("Start/stop streaming for this workspace");
				stream->setStyleSheet("QToolButton {background-color : transparent;} QToolButton:hover{background-color: #3399FF;}");
				stream->setMaximumWidth(18);
				stream->setObjectName("stream_workspace");
				a->leftTabWidget()->addWidget(stream);
				a->leftTabWidget()->setMinimumSize(a->leftTabWidget()->sizeHint());
				a->setLeftTabWidget(a->leftTabWidget());
				stream->show();
				connect(stream, SIGNAL(clicked(bool)), this, SLOT(enableStreaming()));
			}
			if (pool->isStreamingEnabled())
				stream->setIcon(vipIcon("stop.png"));
			else
				stream->setIcon(vipIcon("play.png"));
		}
	}
}

void VipDisplayTabBar::tabInserted(int index)
{
	if (index < count() - 1) {
		VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(displayTabWidget()->widget(index));
		if (area) {

			if (!area->rightTabWidget())
				area->setRightTabWidget(new QToolBar());

			if (!area->rightTabWidget()->findChild<QToolButton*>("close_workspace")) {

				QToolButton* tool = new QToolButton();
				QAction* tool_action = area->rightTabWidget()->addWidget(tool);
				tool->setProperty("action", QVariant::fromValue(tool_action));

				tool->setIcon(vipIcon("additional.png"));
				tool->setToolTip("Save as image or session, or print current workspace");
				tool->setAutoRaise(true);
				tool->setMenu(new QMenu());
				tool->setPopupMode(QToolButton::InstantPopup);
				tool->setObjectName("_vip_DisplayPlayerAreaTools");

				QObject::connect(tool->menu()->addAction("Save workspace as image..."), SIGNAL(triggered(bool)), area, SLOT(saveImage()));
				QObject::connect(tool->menu()->addAction("Save workspace as session..."), SIGNAL(triggered(bool)), area, SLOT(saveSession()));
				tool->menu()->addSeparator();
				QObject::connect(tool->menu()->addAction("Copy workspace image to clipboard"), SIGNAL(triggered(bool)), area, SLOT(copyImage()));
				QObject::connect(tool->menu()->addAction("Copy workspace session to clipboard"), SIGNAL(triggered(bool)), area, SLOT(copySession()));

				QAction* change_orientation = area->rightTabWidget()->addAction(vipIcon("refresh.png"), "Change workspace orientation");
				change_orientation->setObjectName("change_orientation");
				QObject::connect(change_orientation, SIGNAL(triggered(bool)), area, SLOT(changeOrientation()));

				QToolButton* float_workspace = new QToolButton();
				float_workspace->setProperty("widget", QVariant::fromValue(displayTabWidget()->widget(index)));
				float_workspace->setIcon(floatIcon());
				float_workspace->setAutoRaise(true);
				float_workspace->setToolTip("Set workspace floating");
				float_workspace->setStyleSheet("QToolButton {background-color : transparent;} QToolButton:hover{background-color: #3399FF;}");
				// float_workspace->setMaximumSize(16, 16);
				float_workspace->setMaximumWidth(18);
				float_workspace->setObjectName("float_workspace");

				// set the close button
				QToolButton* close_workspace = new QToolButton();
				close_workspace->setProperty("widget", QVariant::fromValue(displayTabWidget()->widget(index)));
				close_workspace->setIcon(closeIcon());
				close_workspace->setAutoRaise(true);
				close_workspace->setToolTip("Close workspace");
				close_workspace->setStyleSheet("QToolButton {background-color : transparent;} QToolButton:hover{background-color: #3399FF;}");
				// close_workspace->setMaximumSize(16, 16);
				close_workspace->setMaximumWidth(18);
				close_workspace->setObjectName("close_workspace");

				area->rightTabWidget()->setIconSize(QSize(16, 16));
				QAction* float_action = area->rightTabWidget()->addWidget(float_workspace);
				QAction* close_action = area->rightTabWidget()->addWidget(close_workspace);

				float_workspace->setProperty("action", QVariant::fromValue(float_action));
				close_workspace->setProperty("action", QVariant::fromValue(close_action));

				connect(close_workspace, SIGNAL(clicked(bool)), this, SLOT(closeTab()));
				connect(float_workspace, SIGNAL(clicked(bool)), this, SLOT(floatTab()));
			}
		}
	}

	if (currentIndex() == count() - 1 && count() > 1)
		setCurrentIndex(count() - 2);

	updateIcons();
}

void VipDisplayTabBar::leaveEvent(QEvent*)
{
	d_data->hoverIndex = -1;
	updateIcons();
}

void VipDisplayTabBar::mouseMoveEvent(QMouseEvent* event)
{
	// if(event->buttons())
	QTabBar::mouseMoveEvent(event);
	if (event->button() == Qt::NoButton && tabAt(event->VIP_EVT_POSITION()) != d_data->hoverIndex) {
		d_data->hoverIndex = tabAt(event->VIP_EVT_POSITION());
		updateIcons();
	}
}

void VipDisplayTabBar::mouseDoubleClickEvent(QMouseEvent* evt)
{
	if ((evt->buttons() & Qt::RightButton)) {
		QTabBar::mouseDoubleClickEvent(evt);
		return;
	}

	int index = tabAt(evt->VIP_EVT_POSITION());
	if (index < 0)
		return;
	VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(displayTabWidget()->widget(index));
	if (!area)
		return;
	displayTabWidget()->setProperty("_vip_index", index);
	displayTabWidget()->renameWorkspace();
}

void VipDisplayTabBar::mousePressEvent(QMouseEvent* event)
{
	// id we press on the last tab, insert a new one
	if (tabAt(event->VIP_EVT_POSITION()) == count() - 1) {
		displayTabWidget()->displayArea()->addWidget(new VipDisplayPlayerArea());
	}
	else {
		QTabBar::mousePressEvent(event);
	}
}

void VipDisplayTabBar::mouseReleaseEvent(QMouseEvent* event)
{
	QTabBar::mouseReleaseEvent(event);
	// make sure the last tab is the "+" one
	for (int i = 0; i < count(); ++i) {
		if (tabText(i) == "+" && i != count() - 1) {
			// reorder
			moveTab(i, count() - 1);
			break;
		}
	}
}

void VipDisplayTabBar::dragEnterEvent(QDragEnterEvent* evt)
{
	evt->accept();
	d_data->dragIndex = tabAt(evt->VIP_EVT_POSITION());
}

void VipDisplayTabBar::dragMoveEvent(QDragMoveEvent* evt)
{
	d_data->dragIndex = tabAt(evt->VIP_EVT_POSITION());
	d_data->timer->stop();
	d_data->timer->start();
}

void VipDisplayTabBar::dragLeaveEvent(QDragLeaveEvent*)
{
	d_data->dragIndex = -1;
	d_data->timer->stop();
}

void VipDisplayTabBar::dragLongEnough()
{
	int index = d_data->dragIndex;
	if (index >= 0) {
		if (index < count() - 1) {
			this->setCurrentIndex(index);
		}
		else {
			displayTabWidget()->displayArea()->addWidget(new VipDisplayPlayerArea());
		}
	}
}

void VipDisplayTabBar::closeTab()
{
	QWidget* w = sender()->property("widget").value<QWidget*>();
	int index = displayTabWidget()->indexOf(w);
	if (index >= 0)
		displayTabWidget()->closeTab(index);
	else if (w) {
		// close the current workspace
		delete w;
	}
}

void VipDisplayTabBar::floatTab()
{
	if (VipDisplayPlayerArea* area = sender()->property("widget").value<VipDisplayPlayerArea*>())
		area->setFloating(true);
}

void VipDisplayTabBar::updateIcons()
{
	int current = currentIndex();
	int hover = d_data->hoverIndex;
	for (int i = 0; i < count(); ++i) {
		VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(this->displayTabWidget()->widget(i));
		if (QWidget* buttons = tabButton(i, QTabBar::RightSide)) {
			// QToolBar* bar = qobject_cast<QToolBar*>(buttons);
			QToolButton* _close = buttons->findChild<QToolButton*>("close_workspace");
			QToolButton* _float = buttons->findChild<QToolButton*>("float_workspace");

			if (i == current) {
				if (_close)
					_close->setIcon(selectedCloseIcon());
				if (_float)
					_float->setIcon(selectedFloatIcon());
			}
			else if (i == hover) {
				if (_close)
					_close->setIcon(hoverCloseIcon());
				if (_float)
					_float->setIcon(hoverFloatIcon());
			}
			else {
				if (_close)
					_close->setIcon(closeIcon());
				if (_float)
					_float->setIcon(floatIcon());
			}

			if (area) {
				bool close_visible = area->testSupportedOperation(VipDisplayPlayerArea::Closable);
				bool float_visible = area->testSupportedOperation(VipDisplayPlayerArea::Floatable);

				if (_close)
					_close->property("action").value<QAction*>()->setVisible(close_visible);
				if (_float)
					_float->property("action").value<QAction*>()->setVisible(float_visible);
			}
		}
	}
}

VipDisplayTabWidget::VipDisplayTabWidget(QWidget* parent)
  : QTabWidget(parent)
{
	setTabBar(new VipDisplayTabBar(this));
	displayTabBar()->setIconSize(QSize(16, 16));
	setMovable(true);
	addTab(new QWidget(), "+");
	connect(this, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
}

VipDisplayTabBar* VipDisplayTabWidget::displayTabBar() const
{
	return static_cast<VipDisplayTabBar*>(tabBar());
}

VipDisplayArea* VipDisplayTabWidget::displayArea() const
{
	QWidget* w = parentWidget();
	while (w) {
		if (VipDisplayArea* area = qobject_cast<VipDisplayArea*>(w))
			return area;
		w = w->parentWidget();
	}
	return nullptr;
}

void VipDisplayTabWidget::closeTab(int index)
{
	bool destroy_current_tab = vipGetMainWindow()->displayArea()->displayTabWidget()->currentIndex() == index;
	if (destroy_current_tab)
		vipGetMainWindow()->setCurrentTabDestroy(true);

	QWidget* widget = this->widget(index);
	removeTab(index);
	delete widget;

	if (this->count() > 1)
		this->setCurrentIndex(this->count() - 2);

	if (destroy_current_tab)
		vipGetMainWindow()->setCurrentTabDestroy(false);
}

void VipDisplayTabWidget::tabChanged(int index)
{
	if (VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(this->widget(index)))
		area->setFocus(true);
}

void VipDisplayTabWidget::closeAllTab()
{
	while (count() > 1) { // 1 because of the last '+' tab
		closeTab(0);
	}
}
void VipDisplayTabWidget::closeAllButTab()
{
	int index = property("_vip_index").toInt();
	if (index >= 0 && index < count()) {
		QWidget* w = widget(index);
		while (count() > 2) { // 2 because of the last '+' tab
			for (int i = 0; i < count(); ++i)
				if (widget(i) != w && widget(i)) {
					closeTab(i);
					break;
				}
		}
	}
}

void VipDisplayTabWidget::closeTab()
{
	int index = property("_vip_index").toInt();
	if (index >= 0 && index < count())
		closeTab(index);
}

void VipDisplayTabWidget::makeFloat(bool enable)
{
	int index = property("_vip_index").toInt();
	if (index >= 0 && index < count())
		if (VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(widget(index)))
			area->setFloating(enable);
}

struct EditWksTitle : QLineEdit
{
	EditWksTitle(QWidget* parent)
	  : QLineEdit(parent)
	{
		connect(this, SIGNAL(returnPressed()), this, SLOT(deleteLater()));
	}
	virtual void focusOutEvent(QFocusEvent*)
	{
		Q_EMIT returnPressed();
		this->deleteLater();
	}
};
static QPointer<EditWksTitle> _wks_title_editor;

void VipDisplayTabWidget::renameWorkspace()
{
	int index = property("_vip_index").toInt();
	if (index >= 0 && index < count()) {
		if (VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(widget(index))) {
			QRect r = this->tabBar()->tabRect(index);
			_wks_title_editor = new EditWksTitle(this->tabBar());
			//_wks_title_editor->resize(d_data->player->width(), _wks_title_editor->height());
			_wks_title_editor->setText(area->windowTitle());
			_wks_title_editor->setSelection(0, _wks_title_editor->text().size());
			_wks_title_editor->setGeometry(r);
			_wks_title_editor->raise();
			_wks_title_editor->show();
			_wks_title_editor->setFocus();
			connect(_wks_title_editor.data(), SIGNAL(returnPressed()), this, SLOT(finishEditingTitle()));
		}
	}
}

void VipDisplayTabWidget::finishEditingTitle()
{
	int index = property("_vip_index").toInt();
	if (index >= 0 && index < count() && _wks_title_editor) {
		if (VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(widget(index))) {
			if (_wks_title_editor->text().size()) {
				area->setWindowTitle(_wks_title_editor->text());
				area->setProperty("_vip_customTitle", true);
			}
		}
	}
}

void VipDisplayTabWidget::mouseDoubleClickEvent(QMouseEvent* evt)
{
	if (!(evt->buttons() & Qt::RightButton)) {
		QTabWidget::mouseDoubleClickEvent(evt);
		return;
	}
	renameWorkspace();
}

void VipDisplayTabWidget::mousePressEvent(QMouseEvent* evt)
{
	if (!(evt->buttons() & Qt::RightButton)) {
		QTabWidget::mousePressEvent(evt);
		return;
	}

	int index = this->tabBar()->tabAt(evt->VIP_EVT_POSITION());
	if (index < 0)
		return;
	VipDisplayPlayerArea* area = qobject_cast<VipDisplayPlayerArea*>(widget(index));
	if (!area)
		return;
	setProperty("_vip_index", index);

	QMenu menu;
	connect(menu.addAction("Edit workspace title..."), SIGNAL(triggered(bool)), this, SLOT(renameWorkspace()));
	menu.addSeparator();
	QAction* _float = menu.addAction("Make workspace floating");
	_float->setCheckable(true);
	_float->setChecked(area->isFloating());
	connect(_float, SIGNAL(triggered(bool)), this, SLOT(makeFloat(bool)));
	connect(menu.addAction("Close workspace"), SIGNAL(triggered(bool)), this, SLOT(closeTab()));
	menu.addSeparator();
	connect(menu.addAction("Close all workspaces"), SIGNAL(triggered(bool)), this, SLOT(closeAllTab()));
	connect(menu.addAction("Close all BUT this"), SIGNAL(triggered(bool)), this, SLOT(closeAllButTab()));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	menu.exec(evt->globalPos());
#else
	menu.exec(evt->globalPosition().toPoint());
#endif
}

class VipPlayerAreaTitleBar::PrivateData
{
public:
	QPoint pt, previous_pos;
	QAction* icon;
	QAction* title;
	QLabel* titleLabel;
	QWidget* additionals;
	QHBoxLayout* additionals_layout;
	QAction* spacer;
	QAction* minimal;
	QAction* pin;
	QAction* minimizeButton;
	QAction* maximizeButton;
	QAction* closeButton;
	VipDisplayPlayerArea* playerArea;
	QPalette palette;
};

VipPlayerAreaTitleBar::VipPlayerAreaTitleBar(VipDisplayPlayerArea* win)
  : QToolBar(win)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	setIconSize(QSize(18, 18));

	d_data->playerArea = win;
	d_data->palette = this->palette();

	QLabel* _icon = new QLabel();
	_icon->setPixmap(vipPixmap("thermavip.png").scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	d_data->icon = addWidget(_icon);
	_icon->setStyleSheet("QLabel {background-color: transparent;}");

	d_data->titleLabel = new QLabel(" Thermavip");
	d_data->title = addWidget(d_data->titleLabel);
	d_data->titleLabel->setStyleSheet("QLabel {background-color: transparent;}");

	QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	d_data->spacer = addWidget(empty);
	empty->setStyleSheet("QToolBar {background-color: transparent;} QToolButton {background-color: transparent;} QWidget {background-color: transparent;}");

	d_data->additionals = new QWidget();
	d_data->additionals_layout = new QHBoxLayout();
	d_data->additionals->setLayout(d_data->additionals_layout);
	d_data->additionals_layout->setContentsMargins(0, 0, 0, 0);
	addWidget(d_data->additionals);

	d_data->pin = addAction(vipIcon("pin.png"), "Set floating");
	addSeparator();
	d_data->minimizeButton = addAction(vipIcon("minimize.png"), tr("Minimize window"));
	d_data->maximizeButton = addAction(vipIcon("maximize.png"), tr("Maximize window"));
	d_data->closeButton = addAction(vipIcon("close.png"), tr("Close window"));

	QList<QToolButton*> buttons = this->findChildren<QToolButton*>();
	for (int i = 0; i < buttons.size(); ++i)
		buttons[i]->setStyleSheet("QToolButton {background-color: transparent;}");

	d_data->playerArea->installEventFilter(this);

	connect(d_data->maximizeButton, SIGNAL(triggered(bool)), this, SLOT(maximizeOrShowNormal()));
	connect(d_data->minimizeButton, SIGNAL(triggered(bool)), d_data->playerArea, SLOT(showMinimized()));
	connect(d_data->closeButton, SIGNAL(triggered(bool)), d_data->playerArea, SLOT(close()));
	connect(d_data->pin, SIGNAL(triggered(bool)), this, SLOT(setFloating(bool)));
}

VipPlayerAreaTitleBar::~VipPlayerAreaTitleBar() {}

void VipPlayerAreaTitleBar::setTitle(const QString& title)
{
	setWindowTitle(title);
	d_data->titleLabel->setText(title);
}

bool VipPlayerAreaTitleBar::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::WindowStateChange) {

		if (d_data->playerArea->isMaximized()) {
			d_data->maximizeButton->setText(tr("Restore"));
			d_data->maximizeButton->setIcon(vipIcon("restore.png"));
		}
		else {
			d_data->maximizeButton->setText(tr("Maximize"));
			d_data->maximizeButton->setIcon(vipIcon("maximize.png"));
		}
	}
	return false;
}

void VipPlayerAreaTitleBar::maximizeOrShowNormal()
{
	if (d_data->playerArea->isMaximized())
		d_data->playerArea->showNormal();
	else
		d_data->playerArea->showMaximized();
}

void VipPlayerAreaTitleBar::mouseDoubleClickEvent(QMouseEvent*)
{
	maximizeOrShowNormal();
}

void VipPlayerAreaTitleBar::mousePressEvent(QMouseEvent* evt)
{
	d_data->pt = d_data->playerArea->mapToParent(evt->VIP_EVT_POSITION());
	d_data->previous_pos = d_data->playerArea->pos();
}

void VipPlayerAreaTitleBar::mouseReleaseEvent(QMouseEvent*)
{
	d_data->pt = QPoint();
}

void VipPlayerAreaTitleBar::mouseMoveEvent(QMouseEvent* evt)
{
	if (d_data->pt != QPoint()) {
		QPoint diff = d_data->playerArea->mapToParent(evt->VIP_EVT_POSITION()) - d_data->pt;
		d_data->playerArea->move(d_data->previous_pos + diff);
	}
}

void VipPlayerAreaTitleBar::setFloating(bool pin)
{
	d_data->playerArea->setFloating(pin);

	if (!pin) {
		d_data->pin->setIcon(vipIcon("pin.png"));
		d_data->pin->setToolTip("Set floating");
	}
	else {
		d_data->pin->setIcon(vipIcon("unpin.png"));
		d_data->pin->setToolTip("Attach to main window");
	}
}

bool VipPlayerAreaTitleBar::isFloating() const
{
	return d_data->playerArea->isFloating();
}

void VipPlayerAreaTitleBar::setFocus(bool f)
{
	if (d_data->playerArea->hasFocus() != f)
		d_data->playerArea->setFocus(f);
	this->style()->unpolish(this);
	this->style()->polish(this);
}

bool VipPlayerAreaTitleBar::hasFocus() const
{
	return d_data->playerArea->hasFocus();
}

QAction* VipPlayerAreaTitleBar::floatAction() const
{
	return d_data->pin;
}

QAction* VipPlayerAreaTitleBar::closeAction() const
{
	return d_data->closeButton;
}

QList<QWidget*> VipPlayerAreaTitleBar::additionalWidgets() const
{
	QList<QWidget*> res;
	for (int i = 0; i < d_data->additionals_layout->count(); ++i)
		if (d_data->additionals_layout->itemAt(i)->widget())
			res.append(d_data->additionals_layout->itemAt(i)->widget());
	return res;
}
void VipPlayerAreaTitleBar::setAdditionalWidget(const QList<QWidget*>& ws)
{
	while (d_data->additionals_layout->count())
		delete d_data->additionals_layout->takeAt(0);
	for (int i = 0; i < ws.size(); ++i) {
		d_data->additionals_layout->addWidget(ws[i]);
		ws[i]->show();
	}
}

class GlobalColorScaleWidget
  : public QWidget
  , public VipRenderObject
{
public:
	GlobalColorScaleWidget(QWidget* parent = nullptr)
	  : QWidget(parent)
	  , VipRenderObject(this)
	{
	}

protected:
	virtual void startRender(VipRenderState& st)
	{
		// hide tool bar
		if (QToolBar* bar = findChild<QToolBar*>())
			bar->hide();
		// set scale colors to black
		if (VipScaleWidget* sc = findChild<VipScaleWidget*>()) {

			sc->setProperty("_vip_styleSheet", QVariant::fromValue(sc->scale()->styleSheet()));
			sc->scale()->setStyleSheet("VipAbstractScale{"
						   "title-color: black;"
						   "label-color: black;"
						   "}");

			// hide grips
			VipAxisColorMap* a = static_cast<VipAxisColorMap*>(sc->scale());
			st.state(this)["grip1"] = a->grip1()->isVisible();
			st.state(this)["grip2"] = a->grip2()->isVisible();
			a->grip1()->setVisible(false);
			a->grip2()->setVisible(false);
		}
	}

	virtual void endRender(VipRenderState& st)
	{
		if (QToolBar* bar = findChild<QToolBar*>())
			bar->show();
		if (VipScaleWidget* sc = findChild<VipScaleWidget*>()) {

			VipStyleSheet ss = sc->property("_vip_styleSheet").value<VipStyleSheet>();
			sc->setProperty("_vip_styleSheet", QVariant());
			sc->scale()->setStyleSheet(ss);

			VipAxisColorMap* a = static_cast<VipAxisColorMap*>(sc->scale());
			a->grip1()->setVisible(st.state(this)["grip1"].toBool());
			a->grip2()->setVisible(st.state(this)["grip2"].toBool());
		}
	}

	virtual bool renderObject(QPainter* p, const QPointF& pos, bool)
	{
		if (isVisible()) {
			if (VipScaleWidget* sc = findChild<VipScaleWidget*>()) {
				QRectF target = sc->geometry();
				target.moveTopLeft(pos);
				sc->render(p, target);
			}
		}
		return false;
	}
};

static int _max_multi_width = 3;

class VipDisplayPlayerArea::PrivateData
{
public:
	PrivateData()
	  : playWidget(nullptr)
	  , pool(nullptr)
	  , floating(false)
	  , id(0)
	  , maxColumns(_max_multi_width)
	  , useGlobalColorMap(false)
	  , operations(AllOperations)
	  , dirtyColorMap(false)
	  , leftTabWidget(nullptr)
	  , rightTabWidget(nullptr)
	{
	}
	VipDragWidgetArea* dragWidgetArea;
	VipPlayWidget* playWidget;
	VipScaleWidget* colorMap;
	VipAxisColorMap* colorMapAxis;
	QToolBar* colorMapBar;
	GlobalColorScaleWidget* colorMapWidget;

	QWidget* topWidget;
	VipProcessingPool* pool;
	VipPlayerAreaTitleBar* titleBar;
	QSplitter* splitter;

	QPointer<VipDisplayArea> parentArea;
	bool floating;
	int id;
	int maxColumns;
	bool useGlobalColorMap;
	QString colorMapTitle;
	Qt::WindowFlags standardFlags;
	Operations operations;

	bool dirtyColorMap;
	VipColorScaleButton* scale;
	QAction* auto_scale;
	QAction* fit_to_grip;
	QAction* histo_scale;

	QToolBar* leftTabWidget;
	QToolBar* rightTabWidget;

	QPointer<VipMultiDragWidget> mainDragWidget;
};

VipDisplayPlayerArea::VipDisplayPlayerArea(QWidget* parent)
  : QWidget(parent)
{
	this->setAttribute(Qt::WA_DeleteOnClose);

	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->playWidget = new VipPlayWidget();
	d_data->playWidget->hide();
	d_data->pool = nullptr;
	d_data->standardFlags = this->windowFlags();
	d_data->dragWidgetArea = new VipDragWidgetArea();

	VipProcessingPool* p = new VipProcessingPool(this);
	p->setMaxReadThreadCount(QThread::idealThreadCount() / 2);
	setProcessingPool(p);

	VipImageArea2D area;
	d_data->colorMap = new VipScaleWidget(
	  d_data->colorMapAxis = area.createColorMap(VipAxisBase::Right, VipInterval(0, 100), VipLinearColorMap::createColorMap(VipGuiDisplayParamaters::instance()->playerColorScale())));
	// d_data->colorMapAxis->setColorBarWidth(12);
	d_data->colorMapAxis->grip1()->setHandleDistance(0);
	d_data->colorMapAxis->grip2()->setHandleDistance(0);
	d_data->colorMapAxis->setUseBorderDistHintForLayout(true);
	d_data->colorMapAxis->scaleDraw()->setTicksPosition(VipScaleDraw::TicksOutside);
	d_data->colorMapAxis->setFlatHistogramStrength(VipGuiDisplayParamaters::instance()->flatHistogramStrength());
	d_data->colorMapBar = new QToolBar();
	d_data->colorMapBar->setIconSize(QSize(16, 16));
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setSpacing(0);
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->addWidget(d_data->colorMapBar);
	vlay->addWidget(d_data->colorMap);
	d_data->colorMapWidget = new GlobalColorScaleWidget();
	d_data->colorMapWidget->setLayout(vlay);
	d_data->colorMapWidget->setMaximumWidth(100);
	// d_data->colorMap->setStyleSheet("VipScaleWidget {background: red;}");
	// d_data->colorMapWidget->setStyleSheet("QWidget{background: green;}");

	d_data->colorMapAxis->grip1()->setImage(vipPixmap("slider_handle.png").toImage());
	d_data->colorMapAxis->grip2()->setImage(vipPixmap("slider_handle.png").toImage());

	VipGuiDisplayParamaters::instance()->apply(d_data->colorMap);

	d_data->auto_scale = d_data->colorMapBar->addAction(vipIcon("scaleauto.png"), "Toogle auto scaling");
	d_data->auto_scale->setCheckable(true);
	d_data->auto_scale->setChecked(d_data->colorMapAxis->isAutoScale());
	d_data->fit_to_grip = d_data->colorMapBar->addAction(vipIcon("fit_to_scale.png"), "Fit color scale to grips");
	d_data->histo_scale = d_data->colorMapBar->addAction(vipIcon("scalehisto.png"), "Adjust color scale to have the best dynamic");
	d_data->histo_scale->setCheckable(true);
	d_data->histo_scale->setChecked(d_data->colorMapAxis->useFlatHistogram());
	// QAction * scale_params = d_data->colorMapBar->addAction(vipIcon("scaletools.png"), "Display color scale parameters");
	d_data->scale = new VipColorScaleButton();
	d_data->scale->setColorPalette(static_cast<VipLinearColorMap*>(d_data->colorMapAxis->colorMap())->type());
	d_data->colorMapBar->addWidget(d_data->scale);

	connect(d_data->auto_scale, SIGNAL(triggered(bool)), this, SLOT(setAutomaticColorScale(bool)));
	connect(d_data->fit_to_grip, SIGNAL(triggered(bool)), this, SLOT(fitColorScaleToGrips()));
	connect(d_data->histo_scale, SIGNAL(triggered(bool)), this, SLOT(setFlatHistogramColorScale(bool)));
	// QObject::connect(scale_params, SIGNAL(triggered(bool)), player, SLOT(showColorScaleParameters()));
	connect(d_data->scale, SIGNAL(colorPaletteChanged(int)), this, SLOT(setColorMap(int)));
	connect(d_data->colorMapAxis, &VipAxisColorMap::valueChanged, this, std::bind(&VipDisplayPlayerArea::setAutomaticColorScale, this, false));
	connect(d_data->colorMapAxis, SIGNAL(mouseButtonDoubleClick(VipAbstractScale*, VipPlotItem::MouseButton, double)), this, SLOT(editColorMap()));

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setSpacing(0);
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(d_data->dragWidgetArea);
	hlay->addWidget(d_data->colorMapWidget);
	QWidget* w = new QWidget();
	w->setLayout(hlay);
	d_data->colorMapWidget->hide();

	d_data->splitter = new QSplitter(Qt::Vertical);
	d_data->splitter->addWidget(w);
	d_data->splitter->addWidget(d_data->playWidget);
	d_data->splitter->setHandleWidth(0);
	d_data->splitter->setChildrenCollapsible(false);

	d_data->titleBar = new VipPlayerAreaTitleBar(this);
	d_data->titleBar->hide();

	d_data->topWidget = new QWidget();
	d_data->topWidget->hide();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(d_data->titleBar);
	lay->addWidget(d_data->topWidget);
	lay->addWidget(d_data->splitter, 1);
	lay->setSpacing(0);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	VipDragWidgetHandler* handler = VipDragWidgetHandler::find(dragWidgetArea()->widget());
	connect(handler, SIGNAL(added(VipMultiDragWidget*)), this, SLOT(added(VipMultiDragWidget*)));
	connect(handler, SIGNAL(contentChanged(VipMultiDragWidget*)), this, SLOT(contentChanged(VipMultiDragWidget*)));

	// VipDragWidgetHandler * handler = VipDragWidgetHandler::find(dragWidgetArea()->widget());
	connect(handler, SIGNAL(minimized(VipMultiDragWidget*)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
	connect(handler, SIGNAL(maximized(VipMultiDragWidget*)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
	connect(handler, SIGNAL(restored(VipMultiDragWidget*)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
	connect(handler, SIGNAL(visibilityChanged(VipBaseDragWidget*)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));

	connect(this, SIGNAL(windowTitleChanged(const QString&)), d_data->titleBar, SLOT(setTitle(const QString&)));

	connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(focusChanged(QWidget*, QWidget*)));

	connect(d_data->dragWidgetArea, SIGNAL(textDropped(const QStringList&, const QPoint&)), this, SLOT(textDropped(const QStringList&, const QPoint&)));
	connect(d_data->dragWidgetArea, SIGNAL(mouseReleased(int)), this, SLOT(receiveMouseReleased(int)));

	setUseGlobalColorMap(VipGuiDisplayParamaters::instance()->globalColorScale());
}

VipDisplayPlayerArea::~VipDisplayPlayerArea()
{
	if (VipDragWidgetHandler* handler = VipDragWidgetHandler::find(dragWidgetArea()->widget())) {
		handler->disconnect();
		// disconnect(handler, SIGNAL(minimized(VipMultiDragWidget *)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
		//  disconnect(handler, SIGNAL(maximized(VipMultiDragWidget *)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
		//  disconnect(handler, SIGNAL(restored(VipMultiDragWidget *)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
		//  disconnect(handler, SIGNAL(visibilityChanged(VipBaseDragWidget *)), playWidget()->area(), SLOT(defferedUpdateProcessingPool()));
		// disconnect(handler, SIGNAL(focusChanged(VipDragWidget *, VipDragWidget *)), this, SLOT(computeFocusWidget()));
	}
	disconnect(this, SIGNAL(windowTitleChanged(const QString&)), d_data->titleBar, SLOT(setTitle(const QString&)));
	disconnect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(focusChanged(QWidget*, QWidget*)));

	if (d_data->parentArea)
		d_data->parentArea->removeWidget(this);
}

void VipDisplayPlayerArea::relayoutColorMap()
{
	QWidget* w = d_data->splitter->widget(0);
	QHBoxLayout* hlay = static_cast<QHBoxLayout*>(w->layout());
	if (hlay->count() == 1) {
		hlay->addWidget(d_data->colorMapWidget);
	}
}

VipScaleWidget* VipDisplayPlayerArea::colorMapScaleWidget() const
{
	return d_data->colorMap;
}
VipAxisColorMap* VipDisplayPlayerArea::colorMapAxis() const
{
	return d_data->colorMapAxis;
}
QWidget* VipDisplayPlayerArea::colorMapWidget() const
{
	return d_data->colorMapWidget;
}
QToolBar* VipDisplayPlayerArea::colorMapToolBar() const
{
	return d_data->colorMapBar;
}

bool VipDisplayPlayerArea::automaticColorScale() const
{
	return d_data->colorMapAxis->isAutoScale();
}
bool VipDisplayPlayerArea::isFlatHistogramColorScale() const
{
	return d_data->colorMapAxis->useFlatHistogram();
}
int VipDisplayPlayerArea::colorMap() const
{
	return static_cast<VipLinearColorMap*>(d_data->colorMapAxis->colorMap())->type();
}

int VipDisplayPlayerArea::maxColumns() const
{
	return d_data->maxColumns;
}

void VipDisplayPlayerArea::setMaxColumns(int m)
{
	d_data->maxColumns = m;
}

void VipDisplayPlayerArea::saveImage()
{
	VipDisplayPlayerArea* area = this;
	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	VipMultiDragWidget* mdrag = area->mainDragWidget(QList<QWidget*>(), false);
	if (!mdrag)
		return;
	int supported_formats = VipRenderObject::supportedVectorFormats();
	QString filters = VipImageWriter().fileFilters() + ";;PDF file (*.pdf)";
	if (supported_formats & VipRenderObject::PS)
		filters += ";;PS file(*.ps)";
	if (supported_formats & VipRenderObject::EPS)
		filters += ";;EPS file(*.eps)";
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save image as", filters);
	if (!filename.isEmpty()) {
		QFileInfo info(filename);
		if (info.suffix().compare("pdf", Qt::CaseInsensitive) == 0) {
			VipRenderObject::saveAsPdf(mdrag, filename, nullptr);
		}
		else if (info.suffix().compare("ps", Qt::CaseInsensitive) == 0 || info.suffix().compare("eps", Qt::CaseInsensitive) == 0) {
			VipRenderObject::saveAsPs(mdrag, filename);
		}
		else {
			VipRenderState state;
			VipRenderObject::startRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);

			vipProcessEvents();

			bool use_transparency = (QFileInfo(filename).suffix().compare("png", Qt::CaseInsensitive) == 0);

			QPixmap pixmap(/*area->dragWidgetArea()->viewport()->size()*/ mdrag->size());
			if (use_transparency)
				pixmap.fill(QColor(255, 255, 255, 1)); // Qt::transparent);
			else
				pixmap.fill(QColor(255, 255, 255));

			QPainter p(&pixmap);
			p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

			// vipFDAboutToRender().callAllMatch(this->widget());

			VipRenderObject::renderObject(/*area->dragWidgetArea()->widget()*/ mdrag, &p, QPoint(), true, false);

			VipRenderObject::endRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);

			if (!pixmap.save(filename)) {
				VIP_LOG_ERROR("Failed to save image " + filename);
			}
			else {
				VIP_LOG_INFO("Saved image in " + filename);
			}
		}
	}
}

void VipDisplayPlayerArea::print()
{
	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	VipMultiDragWidget* mdrag = this->mainDragWidget(QList<QWidget*>(), false);
	if (!mdrag)
		return;

	QPrinter printer(QPrinter::HighResolution);

	QRect bounding(QPoint(0, 0), mdrag->size());
	// get bounding rect in millimeters
	QScreen* screen = qApp->primaryScreen();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	int thisScreen = QApplication::desktop()->screenNumber(mdrag);
#else
	int thisScreen = qApp->screens().indexOf(mdrag->screen());
	if (thisScreen < 0)
		thisScreen = 0;
#endif
	if (thisScreen >= 0)
		screen = qApp->screens()[thisScreen];

	QSizeF screen_psize = screen->physicalSize();
	QSize screen_size = screen->size();
	qreal mm_per_pixel_x = screen_psize.width() / screen_size.width();
	qreal mm_per_pixel_y = screen_psize.height() / screen_size.height();
	QSizeF paper_size(bounding.width() * mm_per_pixel_x, bounding.height() * mm_per_pixel_y);

	// printer.setPageSize(QPrinter::Custom);
	// printer.setPaperSize(paper_size, QPrinter::Millimeter);
	// printer.setPageMargins(0, 0, 0, 0, QPrinter::Millimeter);
	printer.setPageSize(QPageSize(paper_size, QPageSize::Millimeter));
	printer.setResolution(600);

	QPrintDialog printDialog(&printer, nullptr);
	if (printDialog.exec() == QDialog::Accepted) {

		VipRenderState state;
		VipRenderObject::startRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);

		vipProcessEvents();

		QPainter p(&printer);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
		// this->draw(&p);
		VipRenderObject::renderObject(/*area->dragWidgetArea()->widget()*/ mdrag, &p, QPoint(), true, false);

		VipRenderObject::endRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);
	}
}
void VipDisplayPlayerArea::saveSession()
{
	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	VipExportSessionWidget* edit = new VipExportSessionWidget(nullptr, true);
	VipGenericDialog dialog(edit, "Save current workspace");
	if (dialog.exec() == QDialog::Accepted)
		edit->exportSession();
}
void VipDisplayPlayerArea::copyImage()
{
	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	VipDisplayPlayerArea* area = this;
	VipMultiDragWidget* mdrag = area->mainDragWidget(QList<QWidget*>(), false);
	if (!mdrag)
		return;

	VipRenderState state;
	VipRenderObject::startRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);

	vipProcessEvents();

	QPixmap pixmap(/*area->dragWidgetArea()->viewport()->size()*/ mdrag->size());
	pixmap.fill(QColor(255, 255, 255));

	{
		QPainter p(&pixmap);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

		VipRenderObject::renderObject(/*area->dragWidgetArea()->widget()*/ mdrag, &p, QPoint(), true, false);

		VipRenderObject::endRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);
	}

	pixmap = vipRemoveColoredBorder(pixmap, QColor(255, 255, 255));
	qApp->clipboard()->setPixmap(pixmap);
}
void VipDisplayPlayerArea::copySession()
{
	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	QString path = QDir::tempPath();
	path.replace("\\", "/");
	if (!path.endsWith("/"))
		path += "/";
	QString filename = this->windowTitle();
	filename.replace(" ", "_");
	filename.replace(".", "_");
	filename.replace(";", "_");
	filename.replace("*", "_");

	filename = path + filename + ".session";

	if (QFileInfo(filename).exists()) {
		if (!QFile::remove(filename)) {
			VIP_LOG_ERROR("Unable to create session file: output file already exists and cannot be removed");
			return;
		}
	}

	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	VipExportSessionWidget* edit = new VipExportSessionWidget(nullptr, true);
	edit->setFilename(filename);
	edit->exportSession();
	delete edit;

	if (QFileInfo(filename).exists()) {
		QMimeData* mime = new QMimeData();
		mime->setUrls(QList<QUrl>() << QUrl::fromLocalFile(filename));
		qApp->clipboard()->setMimeData(mime);
	}
}
void VipDisplayPlayerArea::changeOrientation()
{
	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);
	VipMultiDragWidget* mdrag = this->mainDragWidget(QWidgetList(), false);
	if (!mdrag)
		return;

	Qt::Orientation ori = mdrag->orientation();
	if (ori == Qt::Vertical)
		mdrag->setOrientation(Qt::Horizontal);
	else
		mdrag->setOrientation(Qt::Vertical);
}

void VipDisplayPlayerArea::setFlatHistogramColorScale(bool enable)
{
	d_data->colorMapAxis->setUseFlatHistogram(enable);
	QList<VipVideoPlayer*> players = d_data->dragWidgetArea->findChildren<VipVideoPlayer*>();
	for (int i = 0; i < players.size(); ++i)
		players[i]->spectrogram()->update();
	d_data->histo_scale->blockSignals(true);
	d_data->histo_scale->setChecked(enable);
	d_data->histo_scale->blockSignals(false);
}

void VipDisplayPlayerArea::setAutomaticColorScale(bool auto_scale)
{
	d_data->colorMapAxis->setAutoScale(auto_scale);
	d_data->auto_scale->blockSignals(true);
	d_data->auto_scale->setChecked(auto_scale);
	d_data->auto_scale->blockSignals(false);
	if (auto_scale) {
		QList<VipVideoPlayer*> players = d_data->dragWidgetArea->findChildren<VipVideoPlayer*>();
		for (int i = 0; i < players.size(); ++i)
			players[i]->spectrogram()->update();
	}
}

void VipDisplayPlayerArea::setColorMap(int map)
{
	bool is_flat_histo = isFlatHistogramColorScale();
	d_data->colorMapAxis->setColorMap(VipLinearColorMap::StandardColorMap(map));
	setFlatHistogramColorScale(is_flat_histo);
}
void VipDisplayPlayerArea::fitColorScaleToGrips()
{
	VipInterval inter = d_data->colorMapAxis->gripInterval();
	d_data->colorMapAxis->setAutoScale(false);
	d_data->colorMapAxis->divideAxisScale(inter.minValue(), inter.maxValue());
}
void VipDisplayPlayerArea::internalLayoutColorMapDelay()
{
	if (!d_data->dirtyColorMap) {
		d_data->dirtyColorMap = true;
		QMetaObject::invokeMethod(this, "internalLayoutColorMap", Qt::QueuedConnection);
	}
}
void VipDisplayPlayerArea::internalLayoutColorMap()
{
	layoutColorMap();
}
void VipDisplayPlayerArea::layoutColorMap(const QList<VipVideoPlayer*>& pls)
{
	d_data->dirtyColorMap = false;
	if (d_data->useGlobalColorMap) {
		QList<VipVideoPlayer*> players = pls;
		if (players.isEmpty())
			players = d_data->dragWidgetArea->findChildren<VipVideoPlayer*>();

		// set title
		QStringList lst;
		for (int i = 0; i < players.size(); ++i)
			lst.append(players[i]->viewer()->area()->colorMapAxis()->title().text());
		lst = vipToSet(lst).values();
		QString title = lst.size() == 1 ? lst.first() : QString();
		if (title != d_data->colorMapTitle) {
			colorMapAxis()->setTitle(title);
			d_data->colorMapTitle = title;
		}

		QSize s = d_data->colorMapBar->sizeHint();
		d_data->colorMapWidget->setMaximumWidth(qMax((int)colorMapAxis()->extentForLength(1), s.width()));

		QMetaObject::invokeMethod(d_data->colorMapAxis->grip1(), "updatePosition", Qt::QueuedConnection);
		QMetaObject::invokeMethod(d_data->colorMapAxis->grip2(), "updatePosition", Qt::QueuedConnection);
	}
}

void VipDisplayPlayerArea::setColorMapToPlayer(VipVideoPlayer* pl, bool enable)
{
	// check nothing to do
	bool enable_on_player = pl->spectrogram()->colorMap() != pl->viewer()->area()->colorMapAxis();
	if (enable_on_player && pl->spectrogram()->colorMap() != d_data->colorMapAxis)
		enable_on_player = !enable;
	if (enable == enable_on_player)
		return;

	// disconnect signal
	disconnect(pl->viewer()->area()->colorMapAxis(), SIGNAL(titleChanged(VipText)), 0, 0);

	if (enable) {
		pl->spectrogram()->setColorMap(d_data->colorMapAxis);
		// reconnect signal
		connect(pl->viewer()->area()->colorMapAxis(), SIGNAL(titleChanged(VipText)), this, SLOT(internalLayoutColorMapDelay()));
	}
	else
		pl->spectrogram()->setColorMap(pl->viewer()->area()->colorMapAxis());

	if (enable)
		pl->setColorScaleVisible(false);
	else
		pl->setColorScaleVisible(!pl->isColorImage());
	pl->setColorMapOptionsVisible(!enable);
	internalLayoutColorMapDelay();
}

void VipDisplayPlayerArea::setUseGlobalColorMap(bool enable)
{
	if (enable != d_data->useGlobalColorMap) {
		d_data->useGlobalColorMap = enable;
		QList<VipVideoPlayer*> pls = d_data->dragWidgetArea->findChildren<VipVideoPlayer*>();
		for (int i = 0; i < pls.size(); ++i)
			setColorMapToPlayer(pls[i], enable);
		d_data->colorMapWidget->setVisible(enable);
		if (enable) {
			QMetaObject::invokeMethod(d_data->colorMapAxis->grip1(), "updatePosition", Qt::QueuedConnection);
			QMetaObject::invokeMethod(d_data->colorMapAxis->grip2(), "updatePosition", Qt::QueuedConnection);
		}
	}
}
bool VipDisplayPlayerArea::useGlobalColorMap() const
{
	return d_data->useGlobalColorMap;
}

void VipDisplayPlayerArea::editColorMap()
{
	vipGetPlotToolWidgetPlayer()->setItem(d_data->colorMapAxis);
	vipGetPlotToolWidgetPlayer()->show();
	vipGetPlotToolWidgetPlayer()->raise();
	vipGetPlotToolWidgetPlayer()->setWindowTitle("Edit workspace color map");
}

/* class ManageMainWidget : public QObject
{
public:
	ManageMainWidget(VipMultiDragWidget*);
	~ManageMainWidget();

	VipMultiDragWidget* parent() const;

	virtual bool eventFilter(QObject* w, QEvent* evt);

	void maximizeWorkspace();
};

ManageMainWidget::ManageMainWidget(VipMultiDragWidget* w)
  : QObject(w)
{
	w->installEventFilter(this);
	setObjectName("_vip_ManageMainWidget");
}
ManageMainWidget::~ManageMainWidget() {}

VipMultiDragWidget* ManageMainWidget::parent() const
{
	return static_cast<VipMultiDragWidget*>(QObject::parent());
}

bool ManageMainWidget::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::Resize) {
		if (QWidget* p = parent()->parentWidget()) {
			// get the tool button
			QToolButton* b = p->findChild<QToolButton*>("_vip_maximize");
			if (!b) {
				b = new QToolButton(p);
				b->setAutoRaise(true);
				b->setIcon(vipIcon("show_normal.png"));
				b->setToolTip("Maximize workspace");
				b->setObjectName("_vip_maximize");
			}
			disconnect(b, &QToolButton::clicked, this, &ManageMainWidget::maximizeWorkspace);
			connect(b, &QToolButton::clicked, this, &ManageMainWidget::maximizeWorkspace);

			if (parent()->size() == p->size())
				b->hide();
			else {
				b->show();
				b->raise();
			}
		}
	}
	else if (evt->type() == QEvent::Destroy || evt->type() == QEvent::DeferredDelete) {
		if (QWidget* p = parent()->parentWidget()) {
			if (QToolButton* b = p->findChild<QToolButton*>("_vip_maximize")) {
				b->hide();
			}
		}
	}
	return false;
}
void ManageMainWidget::maximizeWorkspace()
{
	if (QWidget* p = parent()->parentWidget()) {
		parent()->move(0, 0);
		parent()->resize(p->size());
	}
}*/

VipMultiDragWidget* VipDisplayPlayerArea::mainDragWidget(const QWidgetList& widgets, bool create_if_null)
{
	// get the main MultiDragWidget for this area
	VipMultiDragWidget* main = d_data->mainDragWidget; // this->property("_vip_main_multi_drag").value<MultiDragWidget>();
	if (!main) {
		// use first found one (if any)
		main = this->dragWidgetArea()->widget()->findChild<VipMultiDragWidget*>(QString(), Qt::FindDirectChildrenOnly);
		if (!main) {
			// try to use one of the widgets
			QList<VipMultiDragWidget*> lst = vipListCast<VipMultiDragWidget*>(widgets);
			if (lst.size())
				main = lst.first();
		}

		if (!create_if_null && !main)
			return nullptr;

		// create it
		if (!main)
			main = new VipMultiDragWidget();

		// if (!main->findChild<ManageMainWidget*>())
		//	new ManageMainWidget(main);

		// area->addWidget(main);

		main->setParent(this->dragWidgetArea()->widget());
		main->show();
		main->move(0, 0);
		main->raise();
		main->setWindowTitle("Full workspace");

		main->setSupportedOperation(VipDragWidget::Maximize, true);
		main->showMaximized();

		// area->setProperty("_vip_main_multi_drag", QVariant::fromValue(main));
		d_data->mainDragWidget = main;

		// set the global color map, and make sure to remove it on delete
		main->mainSplitterLayout()->addWidget(this->colorMapWidget(), 10, 11);
		QObject::connect(main, SIGNAL(widgetDestroyed(VipMultiDragWidget*)), this, SLOT(relayoutColorMap()));
	}
	return main;
}

void VipDisplayPlayerArea::setSupportedOperation(Operation attribute, bool on)
{
	if (bool(d_data->operations & attribute) == on)
		return;

	if (on)
		d_data->operations |= attribute;
	else
		d_data->operations &= ~attribute;

	setInternalOperations();
}

bool VipDisplayPlayerArea::testSupportedOperation(Operation attribute) const
{
	return d_data->operations & attribute;
}

void VipDisplayPlayerArea::setSupportedOperations(Operations attributes)
{
	if (d_data->operations != attributes) {
		d_data->operations = attributes;
		setInternalOperations();
	}
}

VipDisplayPlayerArea::Operations VipDisplayPlayerArea::supportedOperations() const
{
	return d_data->operations;
}

void VipDisplayPlayerArea::setInternalOperations()
{
	QToolBar* b = rightTabWidget();
	if (!b)
		setRightTabWidget(b = new QToolBar());
	if (QToolButton* _close = b->findChild<QToolButton*>("close_workspace"))
		_close->property("action").value<QAction*>()->setVisible(testSupportedOperation(Closable));
	if (QToolButton* _float = b->findChild<QToolButton*>("float_workspace"))
		_float->property("action").value<QAction*>()->setVisible(testSupportedOperation(Floatable));
	setRightTabWidget(b);
	b->show();

	titleBar()->closeAction()->setVisible(testSupportedOperation(Closable));
	titleBar()->floatAction()->setVisible(testSupportedOperation(Floatable));
}

VipPlayerAreaTitleBar* VipDisplayPlayerArea::titleBar() const
{
	return d_data->titleBar;
}

VipDisplayTabWidget* VipDisplayPlayerArea::parentTabWidget() const
{
	QWidget* p = parentWidget();
	while (p) {
		if (VipDisplayTabWidget* t = qobject_cast<VipDisplayTabWidget*>(p))
			return t;
		p = p->parentWidget();
	}
	return nullptr;
}

QWidget* VipDisplayPlayerArea::topWidget() const
{
	return d_data->topWidget;
}

VipDragWidgetArea* VipDisplayPlayerArea::dragWidgetArea() const
{
	return const_cast<VipDragWidgetArea*>(d_data->dragWidgetArea);
}

VipDragWidgetHandler* VipDisplayPlayerArea::dragWidgetHandler() const
{
	return VipDragWidgetHandler::find(d_data->dragWidgetArea->widget());
}

QToolBar* VipDisplayPlayerArea::leftTabWidget() const
{
	return d_data->leftTabWidget;
}
QToolBar* VipDisplayPlayerArea::takeLeftTabWidget()
{
	d_data->leftTabWidget->hide();
	d_data->leftTabWidget->setParent(nullptr);
	return d_data->leftTabWidget;
}
void VipDisplayPlayerArea::setLeftTabWidget(QToolBar* w)
{
	if (w != d_data->leftTabWidget)
		if (d_data->leftTabWidget)
			delete d_data->leftTabWidget;
	d_data->leftTabWidget = w;
	if (w) {
		if (VipDisplayTabWidget* d = this->parentTabWidget()) {
			int index = d->indexOf(this);
			if (index >= 0) {
				w->setParent(d->tabBar());
				w->setMinimumSize(w->sizeHint());
				d->tabBar()->setTabButton(index, QTabBar::LeftSide, w);
				w->show();
			}
		}
		else if (isFloating()) {
			QList<QWidget*> adds;
			if (d_data->leftTabWidget)
				adds << d_data->leftTabWidget;
			if (d_data->rightTabWidget)
				adds << d_data->rightTabWidget;
			d_data->titleBar->setAdditionalWidget(adds);
		}
	}
}
QToolBar* VipDisplayPlayerArea::rightTabWidget() const
{
	return d_data->rightTabWidget;
}
QToolBar* VipDisplayPlayerArea::takeRightTabWidget()
{
	d_data->rightTabWidget->hide();
	d_data->rightTabWidget->setParent(nullptr);
	return d_data->rightTabWidget;
}
void VipDisplayPlayerArea::setRightTabWidget(QToolBar* w)
{
	if (w != d_data->rightTabWidget)
		if (d_data->rightTabWidget)
			delete d_data->rightTabWidget;
	d_data->rightTabWidget = w;
	if (w) {
		if (VipDisplayTabWidget* d = this->parentTabWidget()) {
			int index = d->indexOf(this);
			if (index >= 0) {
				w->setParent(d->tabBar());
				w->setMinimumSize(w->sizeHint());
				d->tabBar()->setTabButton(index, QTabBar::RightSide, w);
				w->show();
			}
		}
		else if (isFloating()) {
			QList<QWidget*> adds;
			if (d_data->leftTabWidget)
				adds << d_data->leftTabWidget;
			if (d_data->rightTabWidget)
				adds << d_data->rightTabWidget;
			d_data->titleBar->setAdditionalWidget(adds);
		}
	}
}

VipPlayWidget* VipDisplayPlayerArea::playWidget() const
{
	return const_cast<VipPlayWidget*>(d_data->playWidget);
}

void VipDisplayPlayerArea::reloadPool()
{
	if (d_data->pool)
		d_data->pool->reload();
}

void VipDisplayPlayerArea::closeEvent(QCloseEvent*)
{
	if (d_data->parentArea)
		d_data->parentArea->removeWidget(this);
}

void VipDisplayPlayerArea::changeEvent(QEvent*)
{
	if (isFloating()) {
		// when minimizing a floating player area, reset the standard title bar
		if (isMinimized()) {
			this->setWindowFlags(d_data->standardFlags | Qt::Window);
		}
		else {
			this->setWindowFlags(Qt::CustomizeWindowHint | Qt::Window);
			this->show();
		}
	}
}

bool VipDisplayPlayerArea::isFloating() const
{
	return d_data->floating;
}

bool VipDisplayPlayerArea::hasFocus() const
{
	if (d_data->parentArea)
		return d_data->parentArea->currentDisplayPlayerArea() == this;
	return false;
}

void VipDisplayPlayerArea::setFocus(bool f)
{
	if (f)
		d_data->parentArea->setCurrentDisplayPlayerArea(this);

	if (d_data->parentArea && f != hasFocus()) {
		if (!f) {
			// set the focus to another player area if not already the case
			if (d_data->parentArea->currentDisplayPlayerArea() == this) {
				int count = d_data->parentArea->count();
				for (int i = 0; i < count; ++i)
					if (d_data->parentArea->widget(i) != this) {
						d_data->parentArea->setCurrentDisplayPlayerArea(d_data->parentArea->widget(i));
						break;
					}
			}
		}
	}

	// repolish the title bar
	d_data->titleBar->setFocus(f);
	this->style()->unpolish(this);
	this->style()->polish(this);
}

#include <qwidgetaction.h>

static QAction* actionForWidget(QToolBar* bar, QWidget* w)
{
	const QList<QAction*> acts = bar->actions();
	for (int i = 0; i < acts.size(); ++i)
		// if (acts[i]->isWidgetType())
		if (QWidgetAction* a = qobject_cast<QWidgetAction*>(acts[i]))
			if (a->defaultWidget() == w)
				return acts[i];
	return nullptr;
}

void VipDisplayPlayerArea::setFloating(bool pin)
{
	if (pin != d_data->floating) {
		d_data->floating = pin;
		d_data->titleBar->setFloating(pin);
		d_data->titleBar->setVisible(pin);

		if (pin) {
			// add the tab buttons to the title bar
			if (VipDisplayTabWidget* p = this->parentTabWidget()) {
				int index = p->indexOf(this);
				if (index >= 0) {
					QList<QWidget*> ws;
					if (leftTabWidget()) {
						leftTabWidget()->setProperty("_vip_pos", QString("left"));
						ws << leftTabWidget();
						p->tabBar()->setTabButton(index, QTabBar::LeftSide, nullptr);
						leftTabWidget()->setParent(nullptr);
						leftTabWidget()->show();
					}
					if (rightTabWidget()) {
						rightTabWidget()->setProperty("_vip_pos", QString("right"));
						ws << rightTabWidget();
						p->tabBar()->setTabButton(index, QTabBar::RightSide, nullptr);
						rightTabWidget()->setParent(nullptr);
						rightTabWidget()->show();

						// hide float and close buttons
						actionForWidget(rightTabWidget(), rightTabWidget()->findChild<QToolButton*>("float_workspace"))->setVisible(false);
						actionForWidget(rightTabWidget(), rightTabWidget()->findChild<QToolButton*>("close_workspace"))->setVisible(false);
						rightTabWidget()->setMinimumSize(rightTabWidget()->sizeHint());
					}

					d_data->titleBar->setAdditionalWidget(ws);
				}
			}

			vipProcessEvents(nullptr);

			this->setParent(vipGetMainWindow());
			// this->setParent(nullptr);
			this->setWindowFlags(Qt::CustomizeWindowHint | Qt::Window);

			vipProcessEvents(nullptr);

			show();
			// change the current tab widget
			if (d_data->parentArea && d_data->parentArea->displayTabWidget()->count() > 1)
				d_data->parentArea->displayTabWidget()->setCurrentIndex(d_data->parentArea->displayTabWidget()->count() - 2);
		}
		else if (d_data->parentArea) {
			this->setWindowFlags(d_data->standardFlags);
			d_data->parentArea->addWidget(this);

			vipProcessEvents(nullptr);

			// add back the tab buttons and remove them from the title bar
			if (VipDisplayTabWidget* p = this->parentTabWidget()) {
				int index = p->indexOf(this);
				if (index >= 0) {

					// show again float and close buttons
					actionForWidget(rightTabWidget(), rightTabWidget()->findChild<QToolButton*>("float_workspace"))->setVisible(true);
					actionForWidget(rightTabWidget(), rightTabWidget()->findChild<QToolButton*>("close_workspace"))->setVisible(true);

					setLeftTabWidget(d_data->leftTabWidget);
					setRightTabWidget(d_data->rightTabWidget);
				}
			}

			// change the current tab widget
			if (d_data->parentArea)
				d_data->parentArea->displayTabWidget()->setCurrentIndex(d_data->parentArea->displayTabWidget()->indexOf(this));
		}
		vipProcessEvents();
	}
}

void VipDisplayPlayerArea::setId(int id)
{
	d_data->id = id;
}

int VipDisplayPlayerArea::id() const
{
	return d_data->id;
}

void VipDisplayPlayerArea::setProcessingPool(VipProcessingPool* pool)
{
	if (pool == d_data->pool)
		return;

	if (d_data->pool && this->parentTabWidget()) {
		disconnect(processingPool(), SIGNAL(objectRemoved(QObject*)), this, SLOT(updateStreamingButton()));
		disconnect(processingPool(), SIGNAL(streamingChanged(bool)), this, SLOT(updateStreamingButton()));
	}

	if (d_data->pool) {
		d_data->playWidget->setProcessingPool(nullptr);
		delete d_data->pool;
		d_data->pool = nullptr;
	}

	if (pool) {
		d_data->pool = pool;
		d_data->playWidget->setProcessingPool(pool);
		setPoolToPlayers();
		connect(processingPool(), SIGNAL(objectRemoved(QObject*)), this, SLOT(updateStreamingButton()), Qt::QueuedConnection);
		connect(processingPool(), SIGNAL(streamingChanged(bool)), this, SLOT(updateStreamingButton()), Qt::QueuedConnection);

		connect(processingPool(), SIGNAL(playingStarted()), this, SLOT(emitPlayingStarted()), Qt::DirectConnection);
		connect(processingPool(), SIGNAL(playingStopped()), this, SLOT(emitPlayingStopped()), Qt::DirectConnection);
		connect(processingPool(), SIGNAL(playingAdvancedOneFrame()), this, SLOT(emitPlayingAdvancedOneFrame()), Qt::DirectConnection);
	}

	if (this->parentTabWidget())
		this->parentTabWidget()->displayTabBar()->updateStreamingButton();
}

void VipDisplayPlayerArea::updateStreamingButton()
{
	vipGetMainWindow()->displayArea()->displayTabWidget()->displayTabBar()->updateStreamingButton();
	// if (this->parentTabWidget())
	//	this->parentTabWidget()->displayTabBar()->updateStreamingButton();
}

void VipDisplayPlayerArea::receiveMouseReleased(int button)
{
	if (button == VipPlotItem::RightButton) {
		QMenu menu;
		connect(menu.addAction(vipIcon("open_file.png"), "Open any files..."), SIGNAL(triggered(bool)), vipGetMainWindow(), SLOT(openFiles()));
		connect(menu.addAction(vipIcon("open_dir.png"), "Open a directory..."), SIGNAL(triggered(bool)), vipGetMainWindow(), SLOT(openDir()));
		if (VipPlotItemClipboard::supportSourceItems()) {
			menu.addSeparator();
			connect(menu.addAction(vipIcon("paste.png"), "Paste items"), SIGNAL(triggered(bool)), this, SLOT(pasteItems()));
		}
		menu.exec(QCursor::pos());
	}
}

void VipDisplayPlayerArea::pasteItems()
{
	d_data->dragWidgetArea->dropMimeData(VipPlotItemClipboard::mimeData(), d_data->dragWidgetArea->mapFromGlobal(QCursor::pos()));
}

VipProcessingPool* VipDisplayPlayerArea::processingPool() const
{
	return const_cast<VipProcessingPool*>(d_data->pool);
}

VipDisplayPlayerArea* VipDisplayPlayerArea::fromChildWidget(QWidget* child)
{
	QWidget* tmp = child;

	while (tmp && !qobject_cast<VipDisplayPlayerArea*>(tmp))
		tmp = tmp->parentWidget();

	return qobject_cast<VipDisplayPlayerArea*>(tmp);
}

static void restore_widget(VipBaseDragWidget* main)
{
	if (VipMultiDragWidget* m = qobject_cast<VipMultiDragWidget*>(main)) {
		for (int y = 0; y < m->mainCount(); ++y)
			for (int x = 0; x < m->subCount(y); ++x) {
				if (VipBaseDragWidget* d = m->widget(y, x, 0))
					restore_widget(d);
			}
	}
	else {
		if (main->isMaximized() && main->parentMultiDragWidget()->count() > 1)
			main->showNormal();
	}
}

void VipDisplayPlayerArea::addWidget(VipBaseDragWidget* widget)
{
	VipMultiDragWidget* main = this->mainDragWidget(QList<QWidget*>() << widget);

	if (main == widget)
		// already added
		return;

	restore_widget(main);

	VipMultiDragWidget* multi = qobject_cast<VipMultiDragWidget*>(widget);

	if (qobject_cast<VipDragWidget*>(widget) || (multi && multi->count() == 1)) {
		// add it like any other widget

		int max_cols = this->maxColumns();

		if (multi) {
			multi->hide();
			widget = multi->widget(0, 0, 0);
			// widget->setParent(nullptr);
			multi->deleteLater();
		}

		if (main->mainCount()) {
			int width = main->subCount(main->mainCount() - 1);
			if (width < max_cols) {
				// add new column
				main->subResize(main->mainCount() - 1, width + 1);
				main->setWidget(main->mainCount() - 1, width, widget);
			}
			else {
				// add new row
				main->mainResize(main->mainCount() + 1);
				main->subResize(main->mainCount() - 1, 1);
				main->setWidget(main->mainCount() - 1, 0, widget);
			}
		}
		else {
			// first widget
			main->setWidget(0, 0, widget);
		}
	}
	else {

		// this is a multi drag widget, add it in a new row
		main->mainResize(main->mainCount() + 1);
		main->subResize(main->mainCount() - 1, 1);
		main->setWidget(main->mainCount() - 1, 0, widget);
	}
}

void VipDisplayPlayerArea::added(VipMultiDragWidget*)
{
	setPoolToPlayers();

	// update the play widget
	d_data->playWidget->updatePlayer();

	vipGetMainWindow()->displayArea()->displayTabWidget()->displayTabBar()->updateStreamingButton();
	// if (VipDisplayTabWidget * t = parentTabWidget())
	//	t->displayTabBar()->updateStreamingButton();
}

void VipDisplayPlayerArea::contentChanged(VipMultiDragWidget*)
{
	setPoolToPlayers();
}

void VipDisplayPlayerArea::setPoolToPlayers()
{
	// set the processing pool to all players.
	// also make sur that all VipIODevice have the processing pool as parent

	QList<VipAbstractPlayer*> players = findChildren<VipAbstractPlayer*>();
	QSet<VipIODevice*> devices;

	for (int i = 0; i < players.size(); ++i) {
		players[i]->setProcessingPool(processingPool());
		QList<VipDisplayObject*> displays = players[i]->displayObjects();
		for (int j = 0; j < displays.size(); ++j) {
			// ALWAYS check for parent first, as setting the parent event to the same value can trigger weird bugs.
			// Last bug:
			// This function is called when a player is destroyed. This will reset the parent to all remaining players and display objects,
			// event if they are already rights. And this trigger (sometimes) a display glitch where the video players are not updated anymore (???),
			// and the play widget itself might display update glitches.
			if (displays[j]->parent() != this->processingPool())
				displays[j]->setParent(this->processingPool());
			devices.unite(vipToSet(vipListCast<VipIODevice*>(displays[j]->allSources())));
		}
	}

	for (QSet<VipIODevice*>::iterator it = devices.begin(); it != devices.end(); ++it)
		if ((*it)->parent() != this->processingPool())
			(*it)->setParent(this->processingPool());
}

void VipDisplayPlayerArea::textDropped(const QStringList& lst, const QPoint&)
{
	VipPathList paths;
	for (int i = 0; i < lst.size(); ++i)
		paths.append(VipPath(lst[i], false));
	vipGetMainWindow()->openPaths(paths, nullptr, this);
}

void VipDisplayPlayerArea::focusChanged(QWidget* old_w, QWidget* new_w)
{
	Q_UNUSED(old_w)

	// raise the right VipMultiDragWidget
	if (this->isAncestorOf(new_w)) {
		// We set the focus if:
		//  -the mouse is inside the player area or the player area is floating
		//  -there are no modal widget (like a dialog box)
		QPoint pos = QCursor::pos();
		pos = this->mapFromGlobal(pos);
		if (this->isFloating() || QRect(0, 0, width(), height()).contains(pos)) {
			if (!QApplication::activeModalWidget())
				setFocus(true);
		}
	}
}

void VipDisplayPlayerArea::showEvent(QShowEvent* // evt
)
{
	// just make sure that the play widget is updated once after the plugins are loaded
	playWidget()->updatePlayer();

	// if (VipDisplayTabWidget * t = parentTabWidget())
	//	t->displayTabBar()->updateStreamingButton();
	vipGetMainWindow()->displayArea()->displayTabWidget()->displayTabBar()->updateStreamingButtonDelayed();
}

static std::function<QVariantMap(const QString&)> _wks_generate_editable_symbol;
void VipDisplayPlayerArea::setWorkspaceTitleEditable(const std::function<QVariantMap(const QString&)>& generate_editable_symbol)
{
	_wks_generate_editable_symbol = generate_editable_symbol;
}

VipArchive& operator<<(VipArchive& ar, const VipDisplayPlayerArea* area)
{
	if (_wks_generate_editable_symbol) {
		QVariantMap map = _wks_generate_editable_symbol(area->windowTitle());
		if (map.size()) {
			ar.content("WorkspaceTitle", area->windowTitle(), map);
		}
		else
			ar.content("WorkspaceTitle", area->windowTitle());
	}
	else
		ar.content("WorkspaceTitle", area->windowTitle());

	// ar.content("WorkspaceTitle", area->windowTitle());
	ar.content("floating", area->isFloating());
	ar.content("geometry", area->geometry());

	// since 2.2.18
	ar.content("useGlobalColorMap", area->useGlobalColorMap());
	ar.start("colorMap");
	ar.content(area->colorMapAxis());
	ar.end();

	ar.start("players");
	// save the players
	QList<VipMultiDragWidget*> widgets = area->dragWidgetHandler()->topLevelMultiDragWidgets();
	for (int i = 0; i < widgets.size(); ++i)
		ar.content(widgets[i]);
	ar.end();

	// save the processing pool
	ar.content(area->processingPool());

	// save the play widget
	ar.content(area->playWidget());

	vipSaveCustomProperties(ar, area);

	return ar;
}

VipArchive& operator>>(VipArchive& ar, VipDisplayPlayerArea* area)
{
	ar.save();
	QString title = ar.read("WorkspaceTitle").toString();
	if (title.isEmpty()) {
		ar.restore();
		title = ar.read("title").toString();
	}
	if (!title.isEmpty() && !title.startsWith("Workspace "))
		area->setWindowTitle(title);

	area->setFloating(ar.read("floating").toBool());
	area->setGeometry(ar.read("geometry").toRect());

	// since 2.2.18
	bool useGlobalColorMap = false;
	bool hasUseGlobalColorMap = false;
	ar.save();
	if (ar.content("useGlobalColorMap", useGlobalColorMap)) {
		hasUseGlobalColorMap = true;
		ar.start("colorMap");
		ar.content(area->colorMapAxis());
		ar.end();
	}
	else
		ar.restore();

	ar.start("players");
	// load the players
	int count = 0;
	while (true) {
		VipMultiDragWidget* widget = ar.read().value<VipMultiDragWidget*>();
		if (widget) {
			QRect geometry = widget->geometry();
			// widget->setParent(area->dragWidgetArea()->widget());
			area->addWidget(widget);
			widget->setGeometry(geometry);
			++count;
		}
		else
			break;
	}
	ar.resetError();
	ar.end();

	// load the processing pool
	ar.content(area->processingPool());

	// load the play widget
	ar.content(area->playWidget());

	vipLoadCustomProperties(ar, area);

	// Now, we need to call again the playerCreated() function for all players to trigger again the vipFDPlayerCreated() dispatcher.
	// Indeed, when loading a session, the dispatcher is first called on a non connected player which might be an issue for some plugins.
	QList<VipPlayer2D*> players = area->dragWidgetArea()->findChildren<VipPlayer2D*>();
	for (int i = 0; i < players.size(); ++i)
		QMetaObject::invokeMethod(players[i], "playerCreated", Qt::QueuedConnection);

	// global color scale
	if (hasUseGlobalColorMap)
		area->setUseGlobalColorMap(useGlobalColorMap);

	// reset processing pool
	VipProcessingPool* pool = area->processingPool();
	qint64 time = pool->time();

	QMetaObject::invokeMethod(area->playWidget()->area(), "updateProcessingPool", Qt::QueuedConnection);
	QMetaObject::invokeMethod(area->playWidget()->area(), "setTime", Qt::QueuedConnection, Q_ARG(double, (double)time));
	// we still need to reload the processing pool to update Resource devices
	QMetaObject::invokeMethod(pool, "reload", Qt::QueuedConnection);

	return ar;
}

class VipDisplayArea::PrivateData
{
public:
	PrivateData()
	  : tabWidget(nullptr)
	  , focus(nullptr)
	{
	}
	VipDisplayTabWidget* tabWidget;
	VipDragWidget* focus;
	QPointer<VipDisplayPlayerArea> current;
	QList<VipDisplayPlayerArea*> workspaces;
};

VipDisplayArea::VipDisplayArea(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->tabWidget = new VipDisplayTabWidget();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(d_data->tabWidget);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	addWidget(new VipDisplayPlayerArea());

	// connect(d_data->tabWidget,SIGNAL(currentChanged(int)),this,SLOT(computeFocusWidget()));
	connect(d_data->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(computeFocusWidget()));
	connect(this, SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(computeFocusWidget()));
	connect(d_data->tabWidget->tabBar(), SIGNAL(tabMoved(int, int)), this, SLOT(tabMoved(int, int)));
	computeFocusWidget();
}

VipDisplayArea::~VipDisplayArea()
{
	for (int i = 0; i < count(); ++i)
		if (VipDisplayPlayerArea* area = displayPlayerArea(i)) {
			area->d_data->parentArea = nullptr;
			area->disconnect();
			if (VipDragWidgetHandler* handler = area->dragWidgetHandler())
				disconnect(handler, SIGNAL(focusChanged(VipDragWidget*, VipDragWidget*)), this, SLOT(computeFocusWidget()));
		}
}

void VipDisplayArea::computeFocusWidget()
{
	// make sure all VipDragWidgetHandler are connected
	for (int i = 0; i < count(); ++i) {
		if (VipDisplayPlayerArea* area = displayPlayerArea(i)) {
			VipDragWidgetHandler* handler = area->dragWidgetHandler();
			disconnect(handler, SIGNAL(focusChanged(VipDragWidget*, VipDragWidget*)), this, SLOT(computeFocusWidget()));
			connect(handler, SIGNAL(focusChanged(VipDragWidget*, VipDragWidget*)), this, SLOT(computeFocusWidget()));
		}
	}

	// retrieve the focus widget
	// VipDisplayPlayerArea * area = currentDisplayPlayerArea();//qobject_cast<VipDisplayPlayerArea*>()//d_data->tabWidget->widget(d_data->tabWidget->currentIndex()));
	if (VipDisplayPlayerArea* area = currentDisplayPlayerArea()) {
		VipDragWidget* focus = area->dragWidgetHandler()->focusWidget();
		if (focus != d_data->focus) {
			d_data->focus = focus;
			Q_EMIT focusWidgetChanged(focus);
		}
	}
}

void VipDisplayArea::widgetClosed(VipMultiDragWidget* w)
{
	if (w && w->isTopLevel()) {
		// find parent VipDisplayPlayerArea
		VipDisplayPlayerArea* p = nullptr;
		QWidget* widget = w->parentWidget();
		while (widget) {
			if ((p = qobject_cast<VipDisplayPlayerArea*>(widget)))
				break;
			widget = widget->parentWidget();
		}
		Q_EMIT topLevelWidgetClosed(p, w);
		QMetaObject::invokeMethod(this, "computeFocusWidget", Qt::QueuedConnection);
	}
}

void VipDisplayArea::tabMoved(int from, int to)
{
	(void)from;
	(void)to;
	// recompute workspaces indexes
	QList<VipDisplayPlayerArea*> areas;
	for (int i = 0; i < displayTabWidget()->count(); ++i) {
		if (VipDisplayPlayerArea* a = qobject_cast<VipDisplayPlayerArea*>(displayTabWidget()->widget(i)))
			areas.append(a);
	}

	QList<VipDisplayPlayerArea*> floating;
	for (int i = 0; i < d_data->workspaces.size(); ++i) {
		if (areas.indexOf(d_data->workspaces[i]) < 0)
			floating.append(d_data->workspaces[i]);
	}

	d_data->workspaces = areas + floating;
}

VipDisplayTabWidget* VipDisplayArea::displayTabWidget() const
{
	return const_cast<VipDisplayTabWidget*>(d_data->tabWidget);
}

int VipDisplayArea::count() const
{
	return d_data->workspaces.size();
}

VipDisplayPlayerArea* VipDisplayArea::widget(int index) const
{
	return d_data->workspaces[index];
}

VipDisplayPlayerArea* VipDisplayArea::displayPlayerArea(int index) const
{
	return widget(index);
}

VipDisplayPlayerArea* VipDisplayArea::currentDisplayPlayerArea() const
{
	return d_data->current;
}

VipDragWidgetArea* VipDisplayArea::dragWidgetArea(int index) const
{
	VipDisplayPlayerArea* area = displayPlayerArea(index);
	if (area)
		return area->dragWidgetArea();
	return nullptr;
}

VipPlayWidget* VipDisplayArea::playWidget(int index) const
{
	VipDisplayPlayerArea* area = displayPlayerArea(index);
	if (area)
		return area->playWidget();
	return nullptr;
}

VipDragWidget* VipDisplayArea::focusWidget() const
{
	if (VipDisplayPlayerArea* area = currentDisplayPlayerArea()) {
		QList<VipDragWidget*> widgets = area->findChildren<VipDragWidget*>();
		for (int i = 0; i < widgets.size(); ++i) {
			if (widgets[i]->isFocusWidget())
				return widgets[i];
		}
	}
	return nullptr;
}

QString VipDisplayArea::generateWorkspaceName() const
{
	QMap<int, int> ids;
	for (int i = 0; i < count(); ++i) {
		if (widget(i)->windowTitle().startsWith("Workspace ")) {
			int id = widget(i)->windowTitle().split(" ").last().toInt();
			ids.insert(id, id);
		}
	}

	int id = 1;
	for (QMap<int, int>::iterator it = ids.begin(); it != ids.end(); ++it, ++id) {
		if (it.key() != id)
			return "Workspace " + QString::number(id);
	}

	return "Workspace " + QString::number(count() + 1);
}

void VipDisplayArea::addWidget(VipDisplayPlayerArea* widget)
{
	QString title = widget->windowTitle().isEmpty() ? generateWorkspaceName() : widget->windowTitle();
	widget->setWindowTitle(title);
	widget->d_data->parentArea = this;
	widget->setId(count() + 1);

	if (d_data->workspaces.indexOf(widget) < 0) {
		d_data->workspaces.append(widget);
		connect(widget, SIGNAL(playingStarted()), this, SLOT(emitPlayingStarted()), Qt::DirectConnection);
		connect(widget, SIGNAL(playingStopped()), this, SLOT(emitPlayingStopped()), Qt::DirectConnection);
		connect(widget, SIGNAL(playingAdvancedOneFrame()), this, SLOT(emitPlayingAdvancedOneFrame()), Qt::DirectConnection);
		connect(widget, SIGNAL(windowTitleChanged(const QString&)), this, SLOT(titleChanged(const QString&)));
		connect(widget->dragWidgetHandler(), SIGNAL(closed(VipMultiDragWidget*)), this, SLOT(widgetClosed(VipMultiDragWidget*)));
	}

	if (displayTabWidget()->indexOf(widget) < 0) {
		displayTabWidget()->insertTab(displayTabWidget()->count() - 1, widget, title);
		displayTabWidget()->setCurrentIndex(displayTabWidget()->count() - 2);
	}

	setCurrentDisplayPlayerArea(widget);
	widget->setInternalOperations();

	Q_EMIT displayPlayerAreaAdded(widget);
}

void VipDisplayArea::titleChanged(const QString& title)
{
	VipDisplayPlayerArea* s = qobject_cast<VipDisplayPlayerArea*>(sender());
	if (s) {
		int index = displayTabWidget()->indexOf(s);
		if (index >= 0) {
			displayTabWidget()->setTabText(index, title);
		}
	}
}

void VipDisplayArea::removeWidget(VipDisplayPlayerArea* widget)
{
	d_data->workspaces.removeOne(widget);

	Q_EMIT displayPlayerAreaRemoved(widget);
}

void VipDisplayArea::clear()
{
	while (count() > 0) {
		d_data->workspaces.first()->deleteLater();
		d_data->workspaces.pop_front();
		QCoreApplication::processEvents();
	}
}

void VipDisplayArea::nextWorkspace()
{
	int idx = d_data->tabWidget->currentIndex();
	if (d_data->tabWidget->count() > 1)
		idx = (idx + 1) % (d_data->tabWidget->count() - 1);
	if (idx < d_data->tabWidget->count())
		d_data->tabWidget->setCurrentIndex(idx);
}

void VipDisplayArea::previousWorkspace()
{
	int idx = d_data->tabWidget->currentIndex();
	--idx;
	if (idx < 0)
		idx = (d_data->tabWidget->count() - 2);
	if (idx >= 0 && idx < d_data->tabWidget->count())
		d_data->tabWidget->setCurrentIndex(idx);
}

void VipDisplayArea::resetItemSelection()
{
	for (int i = 0; i < count(); ++i) {
		VipDisplayPlayerArea* area = widget(i);
		VipPlayer2D::resetSelection(area);
	}
}

void VipDisplayArea::setCurrentDisplayPlayerArea(VipDisplayPlayerArea* area)
{
	if (d_data->current != area) {
		d_data->current = area;

		// remove the focus to all other area
		for (int i = 0; i < count(); ++i)
			if (widget(i) != area)
				widget(i)->setFocus(false);

		int index = d_data->tabWidget->indexOf(area);
		if (index >= 0 && index != d_data->tabWidget->currentIndex())
			d_data->tabWidget->setCurrentIndex(index);
		// TEST: inside if
		Q_EMIT currentDisplayPlayerAreaChanged(area);
	}
}

void VipDisplayArea::setStreamingEnabled(bool enable)
{
	this->displayTabWidget()->displayTabBar()->setStreamingEnabled(enable);
}
bool VipDisplayArea::streamingButtonEnabled() const
{
	return this->displayTabWidget()->displayTabBar()->streamingButtonEnabled();
}

VipArchive& operator<<(VipArchive& ar, const VipDisplayArea* area)
{
	for (int i = 0; i < area->count(); ++i) {
		ar.content(area->widget(i));
	}
	return ar;
}

VipArchive& operator>>(VipArchive& ar, VipDisplayArea* area)
{
	area->clear();
	while (true) {
		if (VipDisplayPlayerArea* parea = ar.read().value<VipDisplayPlayerArea*>()) {
			QString title = parea->windowTitle();
			bool floating = parea->isFloating();
			QRect geometry = parea->geometry();

			area->addWidget(parea);

			if (!title.isEmpty())
				parea->setWindowTitle(title);
			parea->setFloating(false);
			parea->setFloating(floating);
			parea->setGeometry(geometry);
		}
		else
			break;
	}
	ar.resetError();
	return ar;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipDisplayPlayerArea*>();
	vipRegisterArchiveStreamOperators<VipDisplayArea*>();
	return 0;
}

static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);

static bool customSupportReparent(VipMultiDragWidget* drag, QWidget* new_parent)
{
	// we can change the widget's parent if it gather ALL VipDisplayObject instances used to display the data of its VipIODevice

	// first, fins all the devices
	QList<VipAbstractPlayer*> players = drag->findChildren<VipAbstractPlayer*>();
	QSet<VipProcessingObject*> sources;
	QSet<VipDisplayObject*> displays_in_players;
	QSet<VipDisplayObject*> displays_sinks;

	// compute all the VipDisplayObject of these players and all the source VipIODevice
	for (int i = 0; i < players.size(); ++i) {
		QList<VipDisplayObject*> displays = players[i]->displayObjects();
		//
		// displays_in_players += ( displays.toSet() );
		for (int j = 0; j < displays.size(); ++j) {
			QSet<VipProcessingObject*> tmp_sources = vipToSet(displays[j]->allSources());
			if (tmp_sources.size())
				displays_in_players.insert(displays[j]);
			sources += tmp_sources;
		}
	}

	if (displays_in_players.size() == 0)
		return true;

	QSet<VipIODevice*> source_devices = vipListCast<VipIODevice*>(sources);

	// then, compute all sink VipDisplayObject
	for (QSet<VipIODevice*>::iterator it = source_devices.begin(); it != source_devices.end(); ++it) {
		displays_sinks += vipToSet(vipListCast<VipDisplayObject*>((*it)->allSinks()));
	}

	bool res = (displays_in_players == displays_sinks);

	if (res) {
		// if we can move the VipDisplayObject instances, then move all source VipIODevice in the new VipProcessingPool
		if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChildWidget(new_parent)) {
			if (VipProcessingPool* pool = area->processingPool()) {
				sources += vipListCast<VipProcessingObject*>(displays_in_players);
				for (QSet<VipProcessingObject*>::iterator it = sources.begin(); it != sources.end(); ++it)
					(*it)->setParent(pool);
			}
		}
	}

	return res;
}

VipIconBar::VipIconBar(VipMainWindow* win)
  : QToolBar(win)
  , mainWindow(win)
{
	setIconSize(QSize(18, 18));

	// add 5px space
	QWidget* space = new QWidget();
	space->setMinimumWidth(5);
	space->setStyleSheet("QWidget {background: transparent;}");
	addWidget(space);

	// add Thermavip icon
	labelIcon = new QLabel();
	labelIcon->setPixmap(vipPixmap("thermavip.png").scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	this->icon = addWidget(labelIcon);

	// add 5px space
	space = new QWidget();
	space->setMinimumWidth(5);
	space->setStyleSheet("QWidget {background: transparent;}");
	addWidget(space);

	// add Thermavip title and version
	if (!vipEditionVersion().isEmpty())
		this->titleLabel = new QLabel(" Thermavip - " + vipEditionVersion() + QString(" - v" VIP_VERSION " "));
	else
		this->titleLabel = new QLabel(" Thermavip - v" VIP_VERSION " ");
	this->title = addWidget(titleLabel);

	// add update progress bar
	this->updateProgress = new QProgressBar();
	this->update = addWidget(this->updateProgress);
	this->updateProgress->setRange(0, 100);
	this->updateProgress->setTextVisible(true);
	this->updateProgress->setFormat("Updating...");
	this->updateProgress->setAlignment(Qt::AlignCenter);
	this->updateProgress->setMaximumSize(QSize(90, 20));
	this->updateProgress->setValue(0);
	this->update->setVisible(false);
	this->updateProgress->setToolTip("An update is currently in progress");

	updateIconAction = addAction(vipIcon("update.png"), "<b>Update available</b><br>A Thermavip update is available, and you need to restart to install it.<br>Restart Thermavip?");
	updateIconAction->setVisible(false);

	// add 20px space
	space = new QWidget();
	space->setMinimumWidth(20);
	space->setStyleSheet("QWidget {background: transparent;}");
	addWidget(space);

	connect(updateIconAction, SIGNAL(triggered(bool)), mainWindow, SLOT(restart()));
	connect(mainWindow, SIGNAL(windowTitleChanged(const QString&)), titleLabel, SLOT(setText(const QString&)));
}

VipIconBar::~VipIconBar() {}

void VipIconBar::setTitleIcon(const QPixmap& pix)
{
	labelIcon->setPixmap(pix);
}

QPixmap VipIconBar::titleIcon() const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	return labelIcon->pixmap() ? *labelIcon->pixmap() : QPixmap();
#else
	return labelIcon->pixmap(Qt::ReturnByValue);
#endif
}

void VipIconBar::updateTitle()
{
	if (!customTitle.isEmpty()) {
		this->titleLabel->setText(customTitle);
	}
	else {
		// add Thermavip title and version
		if (!vipEditionVersion().isEmpty())
			this->titleLabel->setText(" Thermavip - " + vipEditionVersion() + QString(" - v" VIP_VERSION " "));
		else
			this->titleLabel->setText(" Thermavip - v" VIP_VERSION " ");
	}
}

void VipIconBar::setMainTitle(const QString& _title)
{
	customTitle = _title;
	updateTitle();
}
QString VipIconBar::mainTitle() const
{
	return this->titleLabel->text();
}

#include <QMouseEvent>

static void showNormalOrMaximize(VipMainWindow* win)
{
	if (!win)
		return;

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	int screen = qApp->desktop()->screenNumber(win);
	QRect screen_rect = qApp->desktop()->screenGeometry(screen);
#else
	QRect screen_rect;
	if (QScreen* screen = win->screen())
		screen_rect = screen->availableGeometry();
	else
		screen_rect = QGuiApplication::primaryScreen()->availableGeometry();
	int screen = qApp->screens().indexOf(win->screen());
#endif

	if (win->isMaximized()) {
		// if maximized but not centered, re-maximize again
		if (win->pos() != screen_rect.topLeft()) {
			win->showNormal();
			win->showMaximized();
			win->setProperty("screen", screen);
		}
		else {
			win->setProperty("was_maximized", false);
			win->showNormal();

			// check if we need to change the screen
			// int new_screen = qApp->desktop()->screenNumber(win);
			//  if (new_screen != win->property("screen").toInt() && win->property("screen").userType() != 0) {
			//  //change screen
			//  QPoint topleft = win->pos() - qApp->desktop()->screenGeometry(new_screen).topLeft();
			//  win->move(screen_rect.left() + topleft.x(), screen_rect.top() + topleft.y());
			//  }
		}
	}
	else {
		// win->showNormal();
		win->move(screen_rect.left(), screen_rect.top());
		win->showMaximized();
		win->setProperty("screen", screen);
	}
}

void VipIconBar::mouseDoubleClickEvent(QMouseEvent*)
{
	showNormalOrMaximize(mainWindow);
}

void VipIconBar::mousePressEvent(QMouseEvent* evt)
{
	this->pt = mainWindow->mapToParent(evt->VIP_EVT_POSITION());
	this->previous_pos = mainWindow->pos();
}

void VipIconBar::mouseReleaseEvent(QMouseEvent*)
{
	this->pt = QPoint();
}

void VipIconBar::mouseMoveEvent(QMouseEvent* evt)
{
	if (this->pt != QPoint() //&& !mainWindow->isMaximized()
	) {
		// if (mainWindow->isMaximized()) {
		//  mainWindow->setProperty("was_maximized", false);
		//  mainWindow->showNormal();
		//  }
		QPoint diff = mainWindow->mapToParent(evt->VIP_EVT_POSITION()) - this->pt;
		mainWindow->move(this->previous_pos + diff);
	}
}

void VipIconBar::setUpdateProgress(int value)
{
	this->updateProgress->setValue(value);
	this->update->setVisible(true);
}

static QList<std::function<void(QMenu*)>> _extend_help;

void vipExtendHelpMenu(const std::function<void(QMenu*)>& fun)
{
	_extend_help.append(fun);
}

VipCloseBar::VipCloseBar(VipMainWindow* win)
  : QToolBar(win)
  , mainWindow(win)
{
	setIconSize(QSize(18, 18));

	QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	this->spacer = addWidget(empty);
	// this->setMaximumWidth(500);

	this->hasFrame = VipCommandOptions::instance().count("frame") > 0;

	// create spin box for max number of columns
	maxCols = new QSpinBox();
	maxCols->setRange(1, 10);
	maxCols->setObjectName("_vip_maxCols");
	maxCols->setValue(3);
	maxCols->setToolTip("Define the maximum number of columns when adding a new player");
	QObject::connect(maxCols, SIGNAL(valueChanged(int)), mainWindow, SLOT(setMaxColumnsForWorkspace(int)));
	maxColsAction = addWidget(maxCols);

	maximize = addAction(vipIcon("show_normal.png"), "<b>Maximize workspaces</b><br>Maximize workspaces by hiding all surrounding tool widgets");
	maximize->setCheckable(true);
	QObject::connect(maximize, SIGNAL(triggered(bool)), mainWindow, SLOT(maximizeWorkspaces(bool)));

	this->toolsButton = new QToolButton();
	toolsButton->setAutoRaise(true);
	toolsButton->setIcon(vipIcon("scaletools2.png"));
	toolsButton->setToolTip("Options");
	toolsButton->setMenu(new QMenu());
	toolsButton->setPopupMode(QToolButton::InstantPopup);
	QObject::connect(toolsButton->menu(), SIGNAL(aboutToShow()), this, SLOT(computeToolsMenu()));
	addWidget(toolsButton);
	computeToolsMenu();

	// setup the help button
	helpButton = new QToolButton();
	helpButton->setIcon(vipIcon("help.png"));
	helpButton->setToolTip("Help");
	helpButton->setAutoRaise(true);

	QMenu* menu = new QMenu(helpButton);
	helpButton->setMenu(menu);
	helpButton->setPopupMode(QToolButton::InstantPopup);
	this->help = addWidget(helpButton);
	addSeparator();

	this->minimizeButton = addAction(vipIcon("minimize.png"), tr("Minimize window"));
	this->maximizeButton = addAction(vipIcon("maximize.png"), tr("Maximize window"));
	this->closeButton = addAction(vipIcon("close.png"), tr("Close window"));

	stateTimer.setSingleShot(false);
	stateTimer.setInterval(100);
	connect(&stateTimer, SIGNAL(timeout()), this, SLOT(computeWindowState()));

	connect(menu, SIGNAL(aboutToShow()), this, SLOT(computeHelpMenu()));
	connect(this->maximizeButton, SIGNAL(triggered(bool)), this, SLOT(maximizeOrShowNormal()));
	connect(this->minimizeButton, SIGNAL(triggered(bool)), mainWindow, SLOT(showMinimized()));
	connect(this->closeButton, SIGNAL(triggered(bool)), mainWindow, SLOT(close()));

	// add a button to display global options to the toolbar
	//connect(toolsButton, SIGNAL(clicked(bool)), mainWindow, SLOT(showOptions()));
}

VipCloseBar::~VipCloseBar()
{
	stateTimer.stop();
	disconnect(&stateTimer, SIGNAL(timeout()), this, SLOT(computeWindowState()));
}

void VipCloseBar::computeHelpMenu()
{
	QMenu* menu = helpButton->menu();
	menu->clear();

	QAction* _help = menu->addAction(vipIcon("help.png"), "Thermavip help...");
	connect(_help, SIGNAL(triggered(bool)), mainWindow, SLOT(showHelp()));

	// find all folders in the help directory
	QStringList help_dirs = QDir("help").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (int i = 0; i < help_dirs.size(); ++i) {
		if (help_dirs[i].startsWith("_")) {
			help_dirs.removeAt(i);
			--i;
		}
	}
	if (help_dirs.size()) {
		for (int i = 0; i < help_dirs.size(); ++i) {
			QAction* a = menu->addAction(vipIcon("help.png"), help_dirs[i] + " help...");
			connect(a, SIGNAL(triggered(bool)), mainWindow, SLOT(showHelpCustom()));
		}
	}

	// additional entries
	for (int i = 0; i < _extend_help.size(); ++i)
		_extend_help[i](menu);

	menu->addSeparator();
	QAction* about = menu->addAction(tr("About Thermavip..."));
	connect(about, SIGNAL(triggered(bool)), mainWindow, SLOT(aboutDialog()));
}

static void selectLegendPosition(QAction* a)
{
	if (QMenu* menu = qobject_cast<QMenu*>(a->parent())) {
		int index = menu->actions().indexOf(a);
		if (index >= 0) {
			VipGuiDisplayParamaters::instance()->setLegendPosition((Vip::PlayerLegendPosition)index);
		}
	}
}
static void setColorMap(QAction* a)
{
	VipGuiDisplayParamaters::instance()->setPlayerColorScale((VipLinearColorMap::StandardColorMap)a->property("index").toInt());
}

void VipCloseBar::computeToolsMenu()
{
	computeToolsMenu(toolsButton);
}

void VipCloseBar::computeToolsMenu(QToolButton * button)
{
	/*
	video player:
	- show axes
	- default color map
	plot player:
	- legend position
	- grid visible
	- time marker always visible
	- autoscale all
	- title inside
	*/

	QMenu* menu = button->menu();
	if (!menu) {
		button->setMenu(new QMenu());
		button->setPopupMode(QToolButton::InstantPopup);
		menu = button->menu();
	}
	menu->clear();

	// global options
	QAction* all_title_vis = menu->addAction("All: title visible");
	all_title_vis->setCheckable(true);
	all_title_vis->setChecked(VipGuiDisplayParamaters::instance()->titleVisible());
	QObject::connect(all_title_vis, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(setTitleVisible(bool)));

	{
		// title font and color

		QWidgetAction* all_title_style = new QWidgetAction(menu);
		QWidget* w = new QWidget();
		QLabel* text = new QLabel("All: title font and color");
		text->setFont(all_title_vis->font());
		text->setMargin(0);
		VipTextWidget* tw = new VipTextWidget();
		tw->edit()->hide();
		VipText tmp;
		tmp.setTextStyle(VipGuiDisplayParamaters::instance()->titleTextStyle());
		tw->setText(tmp);
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->setContentsMargins(0, 0, 0, 0);
		hlay->addWidget(text);
		hlay->addWidget(tw);
		w->setLayout(hlay);
		all_title_style->setDefaultWidget(w);
		menu->addAction(all_title_style);
		QObject::connect(tw, SIGNAL(changed(const VipText&)), VipGuiDisplayParamaters::instance(), SLOT(setTitleTextStyle2(const VipText&)));
	}
	{
		// scale font and color

		QWidgetAction* scale_style = new QWidgetAction(menu);
		QWidget* w = new QWidget();
		QLabel* text = new QLabel("All: scales/legends font and color");
		text->setFont(scale_style->font());
		text->setMargin(0);
		VipTextWidget* tw = new VipTextWidget();
		tw->edit()->hide();
		VipText tmp;
		tmp.setTextStyle(VipGuiDisplayParamaters::instance()->defaultTextStyle());
		tw->setText(tmp);
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->setContentsMargins(0, 0, 0, 0);
		hlay->addWidget(text);
		hlay->addWidget(tw);
		w->setLayout(hlay);
		scale_style->setDefaultWidget(w);
		menu->addAction(scale_style);
		QObject::connect(tw, SIGNAL(changed(const VipText&)), VipGuiDisplayParamaters::instance(), SLOT(setDefaultTextStyle2(const VipText&)));
	}
	menu->addSeparator();

	QAction* show_axes = menu->addAction(vipIcon("show_axes.png"), "Videos: show axises");
	show_axes->setCheckable(true);
	show_axes->setChecked(VipGuiDisplayParamaters::instance()->videoPlayerShowAxes());

	QPixmap pix = VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Jet, QSize(20, 16), QPen());
	QAction* colormap = menu->addAction(pix, "Videos: color scale");
	colormap->setMenu(VipColorScaleButton::generateColorScaleMenu());
	for (int i = 0; i < colormap->menu()->actions().size(); ++i) {
		colormap->menu()->actions()[i]->setCheckable(true);
		colormap->menu()->actions()[i]->setProperty("index", i);
	}
	colormap->menu()->actions()[VipGuiDisplayParamaters::instance()->playerColorScale()]->setChecked(true);

	QAction* global_color_map = menu->addAction(vipIcon("colormap.png"), "Videos: use global colormap");
	global_color_map->setCheckable(true);
	global_color_map->setChecked(VipGuiDisplayParamaters::instance()->globalColorScale());

	QAction* hist_strength = menu->addAction(pix, "Videos: flat histogram strength");
	hist_strength->setMenu(new QMenu());
	for (int i = 1; i < 6; ++i) {
		QAction* a = nullptr;
		if (i == 1)
			a = hist_strength->menu()->addAction("very light");
		if (i == 2)
			a = hist_strength->menu()->addAction("light");
		if (i == 3)
			a = hist_strength->menu()->addAction("medium");
		if (i == 4)
			a = hist_strength->menu()->addAction("strong");
		if (i == 5)
			a = hist_strength->menu()->addAction("very strong");
		if (a) {
			a->setProperty("strength", i);
			a->setCheckable(true);
			a->setChecked(i == VipGuiDisplayParamaters::instance()->flatHistogramStrength());
			QObject::connect(a, SIGNAL(triggered(bool)), mainWindow, SLOT(setFlatHistogramStrength()));
		}
	}

	menu->addSeparator();

	QMenu* legendMenu = new QMenu();
	legendMenu->addAction("Hide legend");
	legendMenu->addAction(vipIcon("blegend.png"), "Show legend bottom");
	legendMenu->addAction(vipIcon("inner_tllegend.png"), "Show inner legend top left");
	legendMenu->addAction(vipIcon("inner_trlegend.png"), "Show inner legend top right");
	legendMenu->addAction(vipIcon("inner_bllegend.png"), "Show inner legend bottom left");
	legendMenu->addAction(vipIcon("inner_brlegend.png"), "Show inner legend bottom right");
	for (int i = 0; i < legendMenu->actions().size(); ++i) {
		legendMenu->actions()[i]->setCheckable(true);
		legendMenu->actions()[i]->setProperty("position", i);
	}
	legendMenu->actions()[VipGuiDisplayParamaters::instance()->legendPosition()]->setChecked(true);
	QAction* legend = menu->addAction("Plots: legend position");
	legend->setMenu(legendMenu);

	QAction* grid = menu->addAction(vipIcon("show_axes.png"), "Plots: show grid");
	grid->setCheckable(true);
	grid->setChecked(VipGuiDisplayParamaters::instance()->defaultPlotArea()->grid()->isVisible());
	QAction* time_marker = menu->addAction(vipIcon("time.png"), "Plots: show time marker");
	time_marker->setCheckable(true);
	time_marker->setChecked(VipGuiDisplayParamaters::instance()->alwaysShowTimeMarker());
	QAction* title = menu->addAction("Plots: title inside");
	title->setCheckable(true);
	title->setChecked(VipGuiDisplayParamaters::instance()->defaultPlotArea()->titleAxis()->titleInside());
	QAction* autoscale = menu->addAction(vipIcon("axises.png"), "Plots: autoscale all");

	menu->addSeparator();
	QObject::connect(menu->addAction("Preferences..."), SIGNAL(triggered(bool)), mainWindow, SLOT(showOptions()));
	QObject::connect(global_color_map, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(setGlobalColorScale(bool)));
	QObject::connect(show_axes, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(setVideoPlayerShowAxes(bool)));
	QObject::connect(colormap->menu(), &QMenu::triggered, menu, setColorMap);
	QObject::connect(legendMenu, &QMenu::triggered, menu, selectLegendPosition);
	QObject::connect(grid, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(setPlotGridVisible(bool)));
	QObject::connect(time_marker, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(setAlwaysShowTimeMarker(bool)));
	QObject::connect(autoscale, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(autoScaleAll()));
	QObject::connect(title, SIGNAL(triggered(bool)), VipGuiDisplayParamaters::instance(), SLOT(setPlotTitleInside(bool)));
}

void VipCloseBar::startDetectState()
{
	stateTimer.start();
	// restore cursor that might have been screwed up during the session loading
	// QGuiApplication::restoreOverrideCursor();
}

static qint64 __last_change = 0;
void VipCloseBar::onMaximized()
{
	if (QDateTime::currentMSecsSinceEpoch() - __last_change < 1000)
		return;

	{
		if (!hasFrame)
			mainWindow->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
		this->maximizeButton->setText(tr("Restore"));
		this->maximizeButton->setIcon(vipIcon("restore.png"));
		if (!mainWindow->isVisible())
			mainWindow->showMaximized();
		__last_change = QDateTime::currentMSecsSinceEpoch();
	}
}

void VipCloseBar::onRestored()
{
	if (QDateTime::currentMSecsSinceEpoch() - __last_change < 1000)
		return;

	if (!hasFrame)
		mainWindow->setWindowFlags(mainWindow->windowFlags() & (~Qt::FramelessWindowHint));
	this->maximizeButton->setText(tr("Maximize"));
	this->maximizeButton->setIcon(vipIcon("maximize.png"));
	mainWindow->show();
	__last_change = QDateTime::currentMSecsSinceEpoch();
}

void VipCloseBar::onMinimized() {}

void VipCloseBar::computeWindowState()
{
	// Recompute window state based on its visibility state
	if (!mainWindow)
		return;

#ifdef _WIN32
	static bool was_maximized_once = false;
	// Windows only, do nothing but reset the icons
	if (mainWindow->isMaximized() || mainWindow->isFullScreen()) {
		this->maximizeButton->setText(tr("Restore"));
		this->maximizeButton->setIcon(vipIcon("restore.png"));
		was_maximized_once = true;
		if (!(mainWindow->windowFlags() & Qt::FramelessWindowHint)) {
			if (!hasFrame)
				mainWindow->setWindowFlags(mainWindow->windowFlags() | Qt::FramelessWindowHint);
			mainWindow->show();
		}
	}
	else {
		this->maximizeButton->setText(tr("Maximize"));
		this->maximizeButton->setIcon(vipIcon("maximize.png"));
		if ((mainWindow->windowFlags() & Qt::FramelessWindowHint) && was_maximized_once) {
			if (!hasFrame)
				mainWindow->setWindowFlags(mainWindow->windowFlags() & (~Qt::FramelessWindowHint));
			mainWindow->show();
		}
	}
#else

	return;//TEST
	int st = mainWindow->windowState();
	int state = mainWindow->property("visibility_state").toInt();
	bool was_maximized = mainWindow->property("was_maximized").toBool();

	if ((st & Qt::WindowMinimized) && state != Qt::WindowMinimized) {
		mainWindow->setProperty("visibility_state", (int)Qt::WindowMinimized);
	}
	else if ((st & Qt::WindowMaximized) && !(st & Qt::WindowMinimized) && state != Qt::WindowMaximized) {
		mainWindow->setProperty("was_maximized", true);
		mainWindow->setProperty("visibility_state", (int)Qt::WindowMaximized);
		if (!hasFrame)
			mainWindow->setWindowFlags(Qt::FramelessWindowHint);
		this->maximizeButton->setText(tr("Restore"));
		this->maximizeButton->setIcon(vipIcon("restore.png"));
		if (!mainWindow->isVisible())
			mainWindow->show();
	}
	else if ((st == Qt::WindowNoState) && state != Qt::WindowNoState) {
		if (was_maximized) {
			mainWindow->showMaximized();
			return;
		}
		mainWindow->setProperty("was_maximized", false);
		mainWindow->setProperty("visibility_state", (int)Qt::WindowNoState);
		if (!hasFrame)
			mainWindow->setWindowFlags(mainWindow->windowFlags() & (~Qt::FramelessWindowHint));
		this->maximizeButton->setText(tr("Maximize"));
		this->maximizeButton->setIcon(vipIcon("maximize.png"));
		if (state != -1)
			mainWindow->show();
	}
#endif
}

void VipCloseBar::maximizeOrShowNormal()
{
	showNormalOrMaximize(mainWindow);
}

struct UpdateThread : QThread
{
	VipMainWindow* mainWindow;
	VipUpdate* update;
	UpdateThread(VipMainWindow* win)
	  : mainWindow(win)
	{
	}

	virtual void run()
	{
		update = new VipUpdate();
		connect(update, SIGNAL(updateProgressed(int)), mainWindow->iconBar()->updateProgress, SLOT(setValue(int)));
		while (VipMainWindow* w = mainWindow) {

			bool downloaded = false;
			if (update->process()->state() != QProcess::Running && update->hasUpdate("./", &downloaded) > 0) // QFileInfo(vipAppCanonicalPath()).canonicalPath(),&downloaded) > 0)
			{
				if (!downloaded) {
					QMetaObject::invokeMethod(w->iconBar()->updateIconAction, "setVisible", Qt::QueuedConnection, Q_ARG(bool, false));
					QMetaObject::invokeMethod(w->iconBar()->update, "setVisible", Qt::QueuedConnection, Q_ARG(bool, true));
					update->startDownload("./"); // QFileInfo(vipAppCanonicalPath()).canonicalPath());
				}
				else
					QMetaObject::invokeMethod(w->iconBar()->updateIconAction, "setVisible", Qt::QueuedConnection, Q_ARG(bool, true));
			}

			if (update->process()->state() != QProcess::Running)
				QMetaObject::invokeMethod(w->iconBar()->update, "setVisible", Qt::QueuedConnection, Q_ARG(bool, false));

			for (int i = 0; i < 50; ++i) {
				qint64 st = QDateTime::currentMSecsSinceEpoch();
				update->process()->waitForFinished(200);
				qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
				int sleep = 200 - el;
				QThread::msleep(sleep > 0 ? sleep : 0);
				if (!mainWindow)
					break;
			}
		}
		delete update;
	}
};

class VipMainWindow::PrivateData
{
public:
	PrivateData()
	  : sessionSavingEnabled(true)
	  , left(nullptr)
	  , right(nullptr)
	  , bottom(nullptr)
	  , currentTabDestroy(false)
	  , loadSession(false)
	{
	}
	QToolBar fileToolBar;
	QToolButton fileButton;
	QToolButton generate;
	QAction* generateAction;
	VipDragMenu* generateMenu;
	QMenu* fileMenu;
	QMenu* sessionMenu;
	QToolButton dirButton;
	QToolButton saveButton;
	QAction* processing_view;
	QAction* saveSessionAction;
	bool sessionSavingEnabled;

	QToolBar toolsToolBar;
	// VipTitleBar *closeToolBar;
	VipIconBar* iconBar;
	VipCloseBar* closeBar;
	VipDisplayArea* displayArea;
	VipSearchLineEdit* searchLineEdit{ nullptr };
	QWidget* searchWidget;

	QToolBar *left, *right, *bottom, *top;
	VipShowWidgetOnHover* showTabBar;

	UpdateThread* updateThread;

	QTimer fileTimer;
	bool currentTabDestroy;
	bool loadSession;
	bool hasFrame;
};

VipMainWindow::VipMainWindow()
  : QMainWindow()
{
	// VipGuiDisplayParamaters::instance(this);

	this->setAttribute(Qt::WA_DeleteOnClose);
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->hasFrame = VipCommandOptions::instance().count("frame") > 0;

	setMargin(0);

	d_data->fileToolBar.setObjectName("File tool bar");
	d_data->fileToolBar.setWindowTitle(tr("File tool bar"));
	d_data->fileToolBar.setMovable(false);

	d_data->toolsToolBar.setObjectName("Tool widgets bar");
	d_data->toolsToolBar.setWindowTitle(tr("Tool widgets bar"));
	d_data->toolsToolBar.setIconSize(QSize(20, 20));
	d_data->toolsToolBar.setStyleSheet("QToolBar{spacing: 10px;}");
	d_data->toolsToolBar.setMovable(false);
	// Add space
	QWidget* tools_spacer = new QWidget();
	tools_spacer->setMaximumWidth(20);
	tools_spacer->setMinimumWidth(20);
	d_data->toolsToolBar.addWidget(tools_spacer);

	d_data->displayArea = new VipDisplayArea();

	setCentralWidget(d_data->displayArea);

	d_data->fileToolBar.setIconSize(QSize(20, 20));
	d_data->fileButton.setToolTip(tr("<b>Open any files...</b><p>Open any kind of file (videos, signals, previous session,...) supported by Thermavip</p>"));
	d_data->fileButton.setIcon(vipIcon("open_file.png"));
	d_data->fileToolBar.addWidget(&d_data->fileButton);
	connect(&d_data->fileButton, SIGNAL(clicked(bool)), this, SLOT(openFiles()));

	d_data->fileMenu = new QMenu(&d_data->fileButton);
	d_data->fileButton.setMenu(d_data->fileMenu);
	d_data->fileButton.setPopupMode(QToolButton::MenuButtonPopup);
	d_data->sessionMenu = new QMenu(&d_data->fileButton);
	QAction* session_menu_action = d_data->fileMenu->addMenu(d_data->sessionMenu);
	session_menu_action->setText(tr("Available sessions"));
	connect(d_data->sessionMenu, SIGNAL(aboutToShow()), this, SLOT(computeSessions()));
	connect(d_data->sessionMenu, SIGNAL(triggered(QAction*)), this, SLOT(sessionTriggered(QAction*)));

	d_data->dirButton.setToolTip(tr("<b>Open a directory...</b><p>Open all the files in a directory and interpret them as separate data or a single data stream</p>"));
	d_data->dirButton.setIcon(vipIcon("open_dir.png"));
	QAction* a = d_data->fileToolBar.addWidget(&d_data->dirButton);
	a->setObjectName("DirButton");
	connect(&d_data->dirButton, SIGNAL(clicked(bool)), this, SLOT(openDir()));

	d_data->saveButton.setToolTip(tr("<b>Save current session...</b>"
					 "<br>Save the whole Thermavip session or only the current Workspace<br>"
					 "<b>F5:</b> fast session saving<br><b>F9:</b> fast session loading"));
	d_data->saveButton.setIcon(vipIcon("save.png"));
	d_data->saveSessionAction = d_data->fileToolBar.addWidget(&d_data->saveButton);
	connect(&d_data->saveButton, SIGNAL(clicked(bool)), this, SLOT(saveSession()));

	d_data->generate.setIcon(vipIcon("generate_signals.png"));
	//d_data->generate.setText("Generate");
	d_data->generate.setToolTip("Generate a signal, a sequential video device,...");
	//d_data->generate.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d_data->generate.setPopupMode(QToolButton::InstantPopup);
	d_data->generateMenu = new VipDragMenu(&d_data->generate);
	d_data->generateMenu->setToolTipsVisible(true);
	d_data->generate.setMenu(d_data->generateMenu);
	d_data->generateAction = d_data->fileToolBar.addWidget(&d_data->generate);
	d_data->generateAction->setObjectName("GenerateButton");

	// Add space
	/*QWidget* spacer = new QWidget();
	spacer->setMaximumWidth(10);
	spacer->setMinimumWidth(10);
	d_data->fileToolBar.addWidget(spacer);*/
	// Add stretch
	QWidget* left_stretch = new QWidget();
	left_stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QWidget* right_stretch = new QWidget();
	right_stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	d_data->fileToolBar.addWidget(left_stretch);

	d_data->searchWidget = new QWidget();
	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	d_data->searchWidget->setLayout(hlay);
	// Add search line edit
	d_data->searchLineEdit = new VipSearchLineEdit();
	d_data->searchLineEdit->setMinimumWidth(600);
	d_data->searchLineEdit->setMinimumHeight(20);
	
	//d_data->searchLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	d_data->fileToolBar.addWidget(d_data->searchWidget);
	d_data->fileToolBar.addWidget(right_stretch);
	d_data->fileToolBar.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	// Add next/prev workspace buttons
	QToolButton* prev = new QToolButton();
	prev->setIcon(vipIcon("prev_workspace.png"));
	prev->setToolTip("Previous workspace");
	connect(prev, SIGNAL(clicked(bool)), displayArea(), SLOT(previousWorkspace()));
	QToolButton* next = new QToolButton();
	next->setIcon(vipIcon("next_workspace.png"));
	next->setToolTip("Next workspace");
	connect(next, SIGNAL(clicked(bool)), displayArea(), SLOT(nextWorkspace()));

	hlay->addWidget(prev);
	hlay->addWidget(next);
	hlay->addWidget(d_data->searchLineEdit);
	

	d_data->iconBar = new VipIconBar(this);
	d_data->iconBar->setMovable(false);
	d_data->iconBar->setObjectName("Icon bar");
	d_data->iconBar->setWindowTitle("Icon bar");

	d_data->closeBar = new VipCloseBar(this);
	d_data->closeBar->setMovable(false);
	d_data->closeBar->setObjectName("Close bar");
	d_data->closeBar->setWindowTitle("Close bar");

	this->addToolBar(Qt::TopToolBarArea, d_data->iconBar);
	this->addToolBar(Qt::TopToolBarArea, &d_data->fileToolBar);
	this->addToolBar(Qt::LeftToolBarArea, &d_data->toolsToolBar);
	//this->addToolBar(Qt::TopToolBarArea, d_data->closeBar);
	//TEST
	d_data->fileToolBar.addWidget(d_data->closeBar);

	d_data->showTabBar = new VipShowWidgetOnHover(this);
	d_data->showTabBar->setShowWidget(displayArea()->displayTabWidget()->tabBar());
	d_data->showTabBar->setHoverWidgets(QList<QWidget*>() << &d_data->fileToolBar << d_data->iconBar);
	d_data->showTabBar->setEnabled(false);

	setMargin(8);
	setObjectName("MainWindow");

	d_data->fileTimer.setSingleShot(false);
	d_data->fileTimer.setInterval(200);
	connect(&d_data->fileTimer, SIGNAL(timeout()), this, SLOT(openSharedMemoryFiles()), Qt::QueuedConnection);
	d_data->fileTimer.start();

	d_data->updateThread = nullptr;

	if (!d_data->hasFrame)
		setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

	connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(applicationStateChanged(Qt::ApplicationState)));
	connect(displayArea(), SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(tabChanged()));

#ifdef _MSC_VER
	new VipWidgetResizer(this);
#endif

	// For compatibility with previous versions, regiter aliases for VipCustomDragWidget and VipCustomMultiDragWidget,
	// otherwise old session files won't load anymore
	qRegisterMetaType<VipDragWidget*>("VipCustomDragWidget*");
	qRegisterMetaType<VipMultiDragWidget*>("VipCustomMultiDragWidget*");

	// register the custom reparent function for VipMultiDragWidget
	VipMultiDragWidget::setReparentFunction(customSupportReparent);

	// Add finalizitation function
	vipAddGuiInitializationFunction([this]() { this->finalizeToolsToolBar(); });
}

VipMainWindow::~VipMainWindow()
{
	Q_EMIT aboutToClose();

	if (d_data->updateThread) {
		d_data->updateThread->mainWindow = nullptr;
		d_data->updateThread->wait();
		delete d_data->updateThread;
	}
	d_data->fileTimer.stop();
	disconnect(&d_data->fileTimer, SIGNAL(timeout()), this, SLOT(openSharedMemoryFiles()));

	d_data.reset();

	QCoreApplication::quit();
}

VipShowWidgetOnHover* VipMainWindow::showTabBar() const
{
	return d_data->showTabBar;
}

static QList<QPointer<QDockWidget>> _toolState;
static QWidget* _lastModalWidget = nullptr;
void VipMainWindow::openSharedMemoryFiles()
{
	// open possibles files
	bool new_workspace = false;
	QStringList files = VipFileSharedMemory::instance().retrieveFilesToOpen(&new_workspace);
	if (files.size()) {
		if (new_workspace) {
			vipGetMainWindow()->displayArea()->addWidget(new VipDisplayPlayerArea());
		}
		vipGetMainWindow()->openPaths(files);
		if (vipGetMainWindow()->isMinimized())
			vipGetMainWindow()->setWindowState(vipGetMainWindow()->windowState() & (~Qt::WindowMinimized | Qt::WindowActive));
		vipGetMainWindow()->raiseOnTop();
	}

	// show/hide generate menu
	d_data->generateAction->setVisible(d_data->generateMenu->actions().size());

	// change the window title
	iconBar()->updateTitle();

	// multi-screen only
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	if (qApp->desktop()->screenCount() > 1)
#else

	if (qApp->screens().size() > 1)
#endif
	{
		// raise modal widget when the application is active to display it on top of dock widgets
		if (QGuiApplication::applicationState() == Qt::ApplicationActive) {

			if (QWidget* w = qApp->activeModalWidget()) {

				if (w != _lastModalWidget) {
					QList<QDockWidget*> ws = this->findChildren<QDockWidget*>();
					for (int i = 0; i < ws.size(); ++i) {
						if (ws[i]->isFloating() && ws[i]->isVisible() && ws[i] != vipGetMultiProgressWidget()) {
							ws[i]->hide();
							_toolState.push_back(ws[i]);
						}
					}
					_lastModalWidget = w;
				}
			}
			else if (_lastModalWidget) {
				for (int i = 0; i < _toolState.size(); ++i) {
					if (_toolState[i])
						_toolState[i]->show();
				}
				_lastModalWidget = nullptr;
				_toolState.clear();
			}
		}
	}
}

void VipMainWindow::computeSessions()
{
	QString dir = vipGetUserPerspectiveDirectory();
	QFileInfoList lst = QDir(dir).entryInfoList(QDir::Files);
	d_data->sessionMenu->clear();
	QStringList files;
	for (int i = 0; i < lst.size(); ++i) {
		if (lst[i].suffix() == "session") {
			d_data->sessionMenu->addAction(lst[i].baseName());
		}
	}
}
void VipMainWindow::sessionTriggered(QAction* act)
{
	QString file = vipGetUserPerspectiveDirectory() + act->text() + ".session";
	if (QFileInfo(file).exists())
		loadSession(file);
}

QAction* VipMainWindow::addToolWidget(VipToolWidget* widget, const QIcon& icon, const QString& text, bool set_tool_icon)
{
	if (!widget->toolBar()) {
		QAction* act = d_data->toolsToolBar.addAction(icon, text);
		if (set_tool_icon) {
			widget->setWindowIcon(icon);
			widget->setDisplayWindowIcon(true);
		}
		widget->setAction(act);
		return act;
	}
	else {
		QToolBar* bar = widget->toolBar();
		bar->setWindowTitle(widget->windowTitle());
		QAction* act = new QAction(icon, text, bar);
		bar->insertAction(bar->actions().size() ? bar->actions().first() : nullptr, act);
		if (set_tool_icon) {
			widget->setWindowIcon(icon);
			widget->setDisplayWindowIcon(true);
		}
		widget->setAction(act);
		this->addToolBar(Qt::TopToolBarArea, bar);
		return act;
	}
}

void VipMainWindow::init()
{
	// Add shortcuts
	new QShortcut(QKeySequence(Qt::Key_F5), this, SLOT(autoSave()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_F9), this, SLOT(autoLoad()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_F11), this,  SLOT(toogleFullScreen()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_Escape), this,  SLOT(exitFullScreen()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_Space), this,  SLOT(startStopPlaying()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_Right), this,  SLOT(nextTime()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_Left), this,  SLOT(previousTime()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence("Alt+right" /* QKeyCombination(Qt::ALT, Qt::Key_Right)*/), this, SLOT(forward10Time()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence("Alt+left" /*QKeyCombination(Qt::ALT, Qt::Key_Left)*/), this, SLOT(backward10Time()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_PageUp), this,  SLOT(firstTime()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence(Qt::Key_PageDown), this,  SLOT(lastTime()), nullptr, Qt::ApplicationShortcut);
	//new QShortcut(QKeySequence("Ctrl+F"), this,  SLOT(focusToSearchLine()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence("Ctrl+T"), this,  SLOT(newWorkspace()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence("Ctrl+W"), this, SLOT(closeWorkspace()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence("Ctrl+right" /*QKeyCombination(Qt::CTRL, Qt::Key_Right)*/), this,  SLOT(nextWorkspace()), nullptr, Qt::ApplicationShortcut);
	new QShortcut(QKeySequence("Ctrl+left" /*QKeyCombination(Qt::CTRL, Qt::Key_Left)*/), this,  SLOT(previousWorkspace()), nullptr, Qt::ApplicationShortcut);

	// Add dock widgets

	addDockWidget(Qt::LeftDockWidgetArea, vipGetPlotToolWidgetPlayer(this));
	addDockWidget(Qt::LeftDockWidgetArea, vipGetProcessingObjectInfo(this));
	addDockWidget(Qt::RightDockWidgetArea, vipGetSceneModelWidgetPlayer(this));
	addDockWidget(Qt::BottomDockWidgetArea, vipGetMultiProgressWidget(this));
	addDockWidget(Qt::BottomDockWidgetArea, vipGetRecordToolWidget(this));
	addDockWidget(Qt::BottomDockWidgetArea, vipGetProcessingEditorToolWidget(this));
	addDockWidget(Qt::RightDockWidgetArea, vipGetConsoleWidget(this));
	addDockWidget(Qt::RightDockWidgetArea, vipGetDirectoryBrowser(this));
	addDockWidget(Qt::RightDockWidgetArea, vipGetAnnotationToolWidget(this));

	vipGetPlotToolWidgetPlayer(this)->setFloating(true);
	vipGetPlotToolWidgetPlayer(this)->hide();
	vipGetProcessingObjectInfo(this)->setFloating(true);
	vipGetProcessingObjectInfo(this)->hide();
	vipGetSceneModelWidgetPlayer(this)->setFloating(true);
	vipGetSceneModelWidgetPlayer(this)->hide();
	vipGetMultiProgressWidget(this)->setFloating(true);
	vipGetMultiProgressWidget(this)->hide();
	vipGetRecordToolWidget(this)->setFloating(true);
	vipGetRecordToolWidget(this)->hide();
	vipGetProcessingEditorToolWidget(this)->setFloating(true);
	vipGetProcessingEditorToolWidget(this)->hide();
	vipGetConsoleWidget(this)->hide();
	// vipGetDirectoryBrowser(this)->setFloating(true);
	vipGetDirectoryBrowser(this)->hide();
	vipGetAnnotationToolWidget()->setFloating(true);
	vipGetAnnotationToolWidget()->hide();

	QAction* edit =
	  d_data->toolsToolBar.addAction(vipIcon("edit.png"), tr("<b>Edit plot items</b><p>Edit axes, labels, color bar, etc.<br>Double click on an item to directly open this panel.</p>"));
	vipGetPlotToolWidgetPlayer(this)->setAction(edit);
	// TEST: hide edit action
	edit->setVisible(false);

	QAction* infos = d_data->toolsToolBar.addAction(
	  vipIcon("INFOS.png"),
	  tr("<b>Player properties</b><p>Dynamically display available information related to a movie, a signal, etc.<br>Click on an item (image, curve) to display its information.</p>"));
	vipGetProcessingObjectInfo(this)->setWindowIcon(vipIcon("INFOS.png"));
	vipGetProcessingObjectInfo(this)->setAction(infos);

	QAction* proc = d_data->toolsToolBar.addAction(
	  vipIcon("PROCESSING.png"), tr("<b>Edit processing</b><p>Edit all processings related to a signal.<br>Click on an item (image, curve) to edit the processings leading to this item.</p>"));
	vipGetProcessingEditorToolWidget(this)->setAction(proc);

	// QAction * progress = d_data->toolsToolBar.addAction(vipIcon("progress.png"),tr("<b>Show/Hide current operations</b><p>Displays operations in progress<p>"));
	// vipGetMultiProgressWidget(this)->setAction(progress);

	QAction* console = d_data->toolsToolBar.addAction(vipIcon("LOG.png"), "<b>Show/Hide the console</b><p>The console displays information on the program workflow</p>");
	vipGetConsoleWidget(this)->setAction(console);

	QAction* dir = d_data->toolsToolBar.addAction(vipIcon("BROWSER.png"), "<b>Show/Hide directory browser</b><p>Displays a directory/file browser</p>");
	vipGetDirectoryBrowser(this)->setAction(dir);

	addToolWidget(vipGetSceneModelWidgetPlayer(this),
		      vipIcon("ROI.png"),
		      tr("<b>Edit Regions Of Interest</b><p>Create Regions Of Interest (ROIs), edit them, display image statistics inside ROIs, etc.</p>"),
		      true);
	addToolWidget(
	  vipGetRecordToolWidget(this), vipIcon("RECORD.png"), tr("<b>Record signals or movies</b><p>Record any kind of signal in an archive, or create a video from a player</p>"), true);

	// Add VTK stuff
#ifdef VIP_WITH_VTK

	vtkFileOutputWindow* w = vtkFileOutputWindow::New();
	w->SetFileName("vtk_errors.txt");
	vtkOutputWindow::SetInstance(w);
	w->Delete();

	vipGetFOVSequenceEditorTool(this)->setAllowedAreas(Qt::NoDockWidgetArea);
	this->addDockWidget(Qt::LeftDockWidgetArea, vipGetFOVSequenceEditorTool(this));

	QAction* vtk_browser = d_data->toolsToolBar.addAction(vipIcon("RENDERING.png"), "<b>Show/Hide 3D object browser</b>");
	vipGetVTKPlayerToolWidget(this)->setAction(vtk_browser);
	this->addDockWidget(Qt::LeftDockWidgetArea, vipGetVTKPlayerToolWidget(this));
#endif

	
	// Add shortcuts
	VipShortcutsHelper::registerShorcut("Open files...", [this]() { this->openFiles(); });
	VipShortcutsHelper::registerShorcut("Open directory...", [this]() { this->openDir(); });
	VipShortcutsHelper::registerShorcut("About...", [this]() { this->aboutDialog(); });
	VipShortcutsHelper::registerShorcut("Preferences...", [this]() { this->showOptions(); });
	VipShortcutsHelper::registerShorcut("Save current session...", [this]() { this->saveSession(); });
	VipShortcutsHelper::registerShorcut("Options...", [this]() { this->showOptions(); });
	VipShortcutsHelper::registerShorcut("Help...", [this]() { this->showHelp(); });

	closeBar()->startDetectState();
}

void VipMainWindow::setMainTitle(const QString& title)
{
	d_data->iconBar->setMainTitle(title);
}
QString VipMainWindow::mainTitle() const
{
	return d_data->iconBar->mainTitle();
}

VipDisplayArea* VipMainWindow::displayArea() const
{
	return d_data->displayArea;
}

QToolBar* VipMainWindow::fileToolBar() const
{
	return &d_data->fileToolBar;
}

QMenu* VipMainWindow::fileMenu() const
{
	return d_data->fileMenu;
}

QMenu* VipMainWindow::generateMenu() const
{
	return d_data->generateMenu;
}

QToolButton* VipMainWindow::generateButton() const
{
	return &d_data->generate;
}

QToolBar* VipMainWindow::toolsToolBar() const
{
	return &d_data->toolsToolBar;
}

VipIconBar* VipMainWindow::iconBar() const
{
	return d_data->iconBar;
}

VipCloseBar* VipMainWindow::closeBar() const
{
	return d_data->closeBar;
}

// VipTitleBar * VipMainWindow::titleBar() const
//  {
//  return nullptr;// d_data->closeToolBar;
//  }

bool VipMainWindow::saveSession(const QString& filename, int session_type, int session_content, const QByteArray& state)
{
	VipXOfArchive arch(filename);
	if (!arch)
		return false;

	// save the state before showing the progress widget
	QByteArray tools_state = state;
	// if (tools_state.isEmpty())
	//  tools_state = saveState();

	VIP_LOG_INFO("Save session in " + filename + "...");
	VipProgress progress;
	progress.setModal(true);
	progress.setText("<b>Save session in</b> " + QFileInfo(filename).fileName() + "...");

	return saveSession(arch, session_type, session_content, tools_state);
}

bool VipMainWindow::saveSession(VipXOArchive& arch, int session_type, int session_content, const QByteArray& state)
{
	if (workspacesMaximized()) {
		maximizeWorkspaces(false);
	}

	QByteArray tools_state = state;
	if (tools_state.isEmpty())
		tools_state = saveState();

	QVariantMap metadata;
	metadata["session_type"] = session_type;
	arch.start("VipSession", metadata);

	// save the Thermavip version
	arch.content("version", QString(VIP_VERSION));

	if ((session_content & MainWindowState) && session_type == MainWindow) {
		// save the state
		arch.content("maximized", this->isMaximized());
		arch.content("size", this->size());
		arch.content("state", tools_state);
		// new in 2.2.17
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		int screen = qApp->desktop()->screenNumber(this);
#else
		int screen = qApp->screens().indexOf(this->screen());
		if (screen < 0)
			screen = 0;
#endif
		arch.content("screen", screen);
		arch.content("DirectoryBrowser", vipGetDirectoryBrowser());
		arch.content("LogConsole", vipGetConsoleWidget());
	}

	if ((session_content & Plugins) && session_type == MainWindow) {
		// save the plugins
		arch.start("Plugins");
		QList<VipPluginInterface*> plugins = VipLoadPlugins::instance().loadedPlugins();
		QStringList names = VipLoadPlugins::instance().loadedPluginNames();
		for (int i = 0; i < names.size(); ++i) {
			arch.start(names[i]);
			plugins[i]->save(arch);
			arch.end();
		}
		arch.end();
	}

	if ((session_content & Settings) && session_type == MainWindow) {
		// save the settings
		arch.start("Settings");
		vipSaveSettings(arch);
		arch.end();
	}

	if ((session_content & DisplayAreas) && session_type == MainWindow) {
		// save the VipDisplayPlayerArea
		arch.start("DisplayPlayerAreas");
		arch.content(displayArea());
		arch.end();
	}

	if (session_type == CurrentArea && displayArea()->currentDisplayPlayerArea()) {
		arch.start("CurrentArea");
		arch.content(displayArea()->currentDisplayPlayerArea());
		arch.end();
	}
	// else if (session_type == CurrentPlayer && displayArea()->currentDisplayPlayerArea())
	//  {
	//  if(VipDragWidget * focus_widget = displayArea()->currentDisplayPlayerArea()->dragWidgetHandler()->focusWidget())
	//  if (VipMultiDragWidget * multi = focus_widget->topLevelMultiDragWidget())
	//  {
	//  arch.start("CurrentPlayer");
	//  arch.content(multi);
	//  arch.end();
	//  }
	//  }

	arch.end();

	VIP_LOG_INFO("Done");

	return true;
}

void VipMainWindow::restoreDockState(const QByteArray& state)
{
	QList<QDockWidget*> widgets = findChildren<QDockWidget*>();
	for (int i = 0; i < widgets.size(); ++i)
		widgets[i]->hide();
	this->restoreState(state);
}

bool VipMainWindow::loadSessionFallback(const QString& filename, const QString& fallback, VipProgress* progress)
{
	if (!loadSessionShowProgress(filename, progress))
		return loadSessionShowProgress(fallback, progress);
	return true;
}

bool VipMainWindow::loadSession(const QString& filename)
{
	VipProgress progress;
	return loadSessionShowProgress(filename, &progress);
}

static bool isVersionValid(const QString& minimal, const QString& file_version)
{
	QStringList l1 = minimal.split(".");
	QStringList l2 = file_version.split(".");

	if (l1.size() != l2.size() || l1.isEmpty())
		return false;

	for (int i = 0; i < l1.size(); ++i) {
		bool ok1, ok2;
		int i1 = l1[i].toInt(&ok1);
		int i2 = l2[i].toInt(&ok2);
		if (!ok1 || !ok2)
			return false;

		if (i2 > i1)
			return true;
		else if (i2 < i1)
			return false;
	}
	return true;
}

struct LockBool
{
	bool* value;
	LockBool(bool* v)
	  : value(v)
	{
		*v = true;
	}
	~LockBool() { *value = false; }
};

bool VipMainWindow::isLoadingSession()
{
	return d_data->loadSession;
}

struct InSessionLoading
{
	InSessionLoading() { VipGuiDisplayParamaters::instance()->setInSessionLoading(true); }
	~InSessionLoading() { VipGuiDisplayParamaters::instance()->setInSessionLoading(false); }
};

bool VipMainWindow::loadSessionShowProgress(const QString& filename, VipProgress* progress)
{
	InSessionLoading inSessionLoading;
	LockBool lock(&d_data->loadSession);
	VipXIfArchive arch(filename);
	if (!arch) {
		return false;
	}
	// load the version number
	arch.save();
	arch.start("VipSession");
	QString ver = arch.read("version").toString();
	if (ver.isEmpty()) {
		VIP_LOG_ERROR("Cannot load session file: cannot find version number");
		return false;
	}
	if (!isVersionValid(VIP_MINIMAL_SESSION_VERSION, ver)) {
		VIP_LOG_ERROR("Cannot load session file: wrong version number");
		return false;
	}
	arch.restore();
	arch.setVersion(ver);

	// display editable content
	if (VipImportSessionWidget::hasEditableContent(arch)) {
		VipImportSessionWidget* edit = new VipImportSessionWidget();
		edit->importArchive(arch);
		QString title = (!edit->windowTitle().isEmpty()) ? " - " + edit->windowTitle() : QString();
		VipGenericDialog dialog(edit, "Load session content" + title);
		if (dialog.exec() == QDialog::Accepted) {
			edit->applyToArchive(arch);
		}
		else
			return false;
	}

	VIP_LOG_INFO("Load session " + filename);

	if (progress) {
		progress->setModal(true);
		progress->setText("<b>Load session </b> " + QFileInfo(filename).fileName() + "...");

		// display the laoding progress
		connect(&arch, SIGNAL(rangeUpdated(double, double)), progress, SLOT(setRange(double, double)), Qt::DirectConnection);
		connect(&arch, SIGNAL(valueUpdated(double)), progress, SLOT(setValue(double)), Qt::DirectConnection);
		arch.setAutoRangeEnabled(true);
	}

	QVariantMap metadata;
	int session_type = MainWindow;
	arch.start("VipSession", metadata);
	if (metadata.contains("session_type"))
		session_type = metadata["session_type"].toInt();

	if (session_type == DragWidget) {
		if (!displayArea()->currentDisplayPlayerArea()) {
			VipDisplayPlayerArea* a = new VipDisplayPlayerArea();
			displayArea()->addWidget(a);
		}
		VipBaseDragWidget* w = vipLoadBaseDragWidget(arch, displayArea()->currentDisplayPlayerArea());
		Q_EMIT sessionLoaded();
		return w != nullptr;
	}

	// load state
	arch.save();
	bool maximized = arch.read("maximized").toBool();
	QSize s = arch.read("size").toSize();
	QVariant state = arch.read("state");

	// new in 2.2.17
	arch.save();
	int screen = -1;
	if (arch.content("screen", screen)) {
	}
	else
		arch.restore();

	if (state.userType() == 0)
		arch.restore();
	else {
		vipGetConsoleWidget()->removeConsole();
		// The state before 5.0.0 is invalid
		if (isVersionValid("5.0.0", ver))
			this->restoreState(state.toByteArray());
		vipGetConsoleWidget()->resetConsole();

		// DirectoryBrowser is not always present
		arch.save();
		if (VipDirectoryBrowser* browser = vipGetDirectoryBrowser()) {
			// TEST
			//QVariant v = vipToVariant(browser);
			//VipDirectoryBrowser* tmp = v.value<VipDirectoryBrowser*>();
			
			if (!arch.content("DirectoryBrowser", browser))
				arch.restore();
		}

		arch.content("LogConsole", vipGetConsoleWidget());
	}

	// load plugins
	arch.save();
	if (arch.start("Plugins")) {
		while (true) {
			QString name;
			if (arch.start(name)) {
				VipPluginInterface* interface = VipLoadPlugins::instance().find(name);
				if (interface)
					interface->restore(arch);

				arch.end();
			}
			else
				break;
		}
		arch.end();
	}
	else
		arch.restore();

	// load settings
	arch.save();
	if (arch.start("Settings")) {
		vipRestoreSettings(arch);
		arch.end();
	}
	else
		arch.restore();

	QList<VipDisplayPlayerArea*> workspaces;

	// load VipDisplayPlayerArea
	arch.save();
	if (arch.start("DisplayPlayerAreas")) {
		arch.content(displayArea());
		arch.end();
		for (int i = 0; i < displayArea()->count(); ++i)
			workspaces << displayArea()->displayPlayerArea(i);
	}
	else
		arch.restore();

	if (session_type == CurrentArea) {
		if (arch.start("CurrentArea")) {
			// This is a session file containing only a VipDisplayPlayerArea.
			// Therefore, we need to remove the processing pool name from the connections
			//(no more 'Wrong connection format for address...' because the VipProcessingPool is called 'VipProcessingPool2')

			arch.setProperty("_vip_removeProcessingPoolFromAddresses", true);
			VipDisplayPlayerArea* area = arch.read().value<VipDisplayPlayerArea*>();
			arch.setProperty("_vip_removeProcessingPoolFromAddresses", QVariant());
			if (area) {
				displayArea()->addWidget(area);
				if (workspaces.indexOf(area) < 0)
					workspaces << area;
			}
			arch.end();
		}
		else
			arch.restore();
	}

	arch.end();

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	if (screen >= 0 && screen < qApp->desktop()->screenCount()) {
		QRect s_geom = qApp->desktop()->screenGeometry(screen);
		int current_screen = qApp->desktop()->screenNumber(this);
		if (maximized && current_screen != screen)
			this->setGeometry(s_geom);
		else
			this->move(s_geom.topLeft());
	}
#else

	if (screen >= 0 && screen < qApp->screens().size()) {
		QRect s_geom = qApp->screens()[screen]->availableGeometry();
		int current_screen = this->screen() ? qApp->screens().indexOf(this->screen()) : -1; // qApp->desktop()->screenNumber(this);
		if (maximized && current_screen != screen)
			this->setGeometry(s_geom);
		else
			this->move(s_geom.topLeft());
	}
#endif

	if (state.userType() != 0) {
		// restore the size only if available
		if (maximized) {
			this->showMaximized();
		}
		else {
			this->showNormal();
			this->resize(s);
		}
	}

	if (vipGetMultiProgressWidget()->isFloating())
		vipGetMultiProgressWidget()->hide();

	if (VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea())
		if (area->processingPool()) {
			// reload the processing pool to make sure all processing has there data set
			area->processingPool()->reload();
			area->processingPool()->wait();
		}

	if (session_type == MainWindow) {
		// reopen all connections, in case some processings are connected to another processing's parent pool
		for (int i = 0; i < displayArea()->count(); ++i)
			displayArea()->widget(i)->processingPool()->openReadDeviceAndConnections();
	}

	for (int i = 0; i < workspaces.size(); ++i)
		Q_EMIT workspaceLoaded(workspaces[i]);
	Q_EMIT sessionLoaded();

	VIP_LOG_INFO("Done");
	return true;
}

void VipMainWindow::resetStyleSheet()
{
	// reapply the style sheet
	// TODO : correct crash on linux!
	// QString style = qApp->styleSheet();
	//  qApp->setStyleSheet("");
	//  qApp->setStyleSheet(style);
	//  this->style()->unpolish(this);
	//  this->style()->polish(this);
}



void VipMainWindow::applicationStateChanged(Qt::ApplicationState state)
{

	// multi-screen only
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	if (qApp->desktop()->screenCount() > 1) {
#else
	if (qApp->screens().size() > 1) {
#endif
		QList<QDockWidget*> ws = this->findChildren<QDockWidget*>();

		for (int i = 0; i < ws.size(); ++i) {
			if (ws[i]->isFloating() && ws[i] != vipGetMultiProgressWidget()) {
				bool vis = ws[i]->isVisible();
				if (state == Qt::ApplicationInactive)
					ws[i]->setProperty("_vip_visible", vis);
				if (state == Qt::ApplicationActive)
					ws[i]->setWindowFlags(ws[i]->windowFlags() | Qt::WindowStaysOnTopHint);
				else
					ws[i]->setWindowFlags((Qt::WindowFlags)(ws[i]->windowFlags() & (~Qt::WindowStaysOnTopHint)));

				if (state == Qt::ApplicationActive) {
					ws[i]->setVisible(vis || ws[i]->property("_vip_visible").toBool());
					if (ws[i]->isVisible())
						ws[i]->raise();
				}
			}
		}
	}
	else {
#ifndef _WIN32
		// linux only
		if (state == Qt::ApplicationActive) {
			// raise all visible floating widgets
			QList<QDockWidget*> ws = this->findChildren<QDockWidget*>();
			for (int i = 0; i < ws.size(); ++i) {
				if (ws[i]->isVisible() && ws[i]->isFloating())
					ws[i]->raise();
			}
		}

#endif
	}
}

void VipMainWindow::setFlatHistogramStrength()
{
	if (!sender())
		return;
	int str = sender()->property("strength").toInt();
	VipGuiDisplayParamaters::instance()->setFlatHistogramStrength(str);
}

void VipMainWindow::tabChanged()
{
	if (VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea()) {
		int num_cols = area->maxColumns();
		if (num_cols != closeBar()->maxCols->value())
			closeBar()->maxCols->setValue(num_cols);
	}
}

void VipMainWindow::finalizeToolsToolBar() 
{
	// Add stretch
	QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	d_data->toolsToolBar.addWidget(empty);

	QToolButton* tools = new QToolButton();
	tools->setIcon(vipIcon("additional.png"));
	tools->setToolTip("<b>Global options and preferences");
	d_data->closeBar->computeToolsMenu(tools);

	d_data->toolsToolBar.addWidget(tools);

	connect(tools->menu(), &QMenu::aboutToShow, this, [this, tools]() { this->d_data->closeBar->computeToolsMenu(tools); });
}

void VipMainWindow::showEvent(QShowEvent* // evt
)
{
}

void VipMainWindow::keyPressEvent(QKeyEvent* evt)
{
	// Handle CTRL+F through keyPressEvent instead
	// of QShortcut as multiple windows use this 
	// shortcut.
	if (evt->key() == Qt::Key_F && (evt->modifiers() & Qt::CTRL))
	{
		focusToSearchLine();
		evt->accept();
	}
	evt->ignore();
}

void VipMainWindow::startStopPlaying()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	if (area->processingPool()->deviceType() == VipIODevice::Temporal) {
		if (area->processingPool()->isPlaying())
			area->processingPool()->stop();
		else
			area->processingPool()->play();
	}
	else if (area->processingPool()->deviceType() == VipIODevice::Sequential) {
		if (area->processingPool()->isStreamingEnabled())
			area->processingPool()->setStreamingEnabled(false);
		else
			area->processingPool()->setStreamingEnabled(true);
	}
}

void VipMainWindow::nextTime()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	area->processingPool()->next();
}
void VipMainWindow::previousTime()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	area->processingPool()->previous();
}
void VipMainWindow::firstTime()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	area->processingPool()->first();
}
void VipMainWindow::lastTime()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	area->processingPool()->last();
}
void VipMainWindow::forward10Time()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	qint64 time = area->processingPool()->time();
	// advanced by 10% of total time range
	qint64 first = area->processingPool()->firstTime();
	qint64 last = area->processingPool()->lastTime();
	qint64 step = (last - first) * 0.1;
	area->processingPool()->seek(time + step);
}
void VipMainWindow::backward10Time()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	qint64 time = area->processingPool()->time();
	// advanced by 10% of total time range
	qint64 first = area->processingPool()->firstTime();
	qint64 last = area->processingPool()->lastTime();
	qint64 step = (last - first) * 0.1;
	area->processingPool()->seek(time - step);
}
void VipMainWindow::nextWorkspace()
{
	displayArea()->nextWorkspace();
}
void VipMainWindow::previousWorkspace()
{
	displayArea()->previousWorkspace();
}
void VipMainWindow::newWorkspace()
{
	displayArea()->addWidget(new VipDisplayPlayerArea());
}
void VipMainWindow::closeWorkspace()
{
	VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return;
	
	delete area;
	if (!displayArea()->currentDisplayPlayerArea()) {
		auto* tab = displayArea()->displayTabWidget();
		if (tab->count() > 1)
			tab->setCurrentIndex(tab->count()-2);
	}
}

void VipMainWindow::focusToSearchLine()
{
	if (d_data->searchLineEdit) {
		d_data->searchLineEdit->selectAll();
		d_data->searchLineEdit->setFocus(Qt::OtherFocusReason);
	}
}
void VipMainWindow::toogleFullScreen()
{
	if (!isFullScreen()) {
		if (!isMaximized()) {
			showMaximized();
			vipProcessEvents();
		}
		showFullScreen();
	}
	else {
		showMaximized();
	}
}
void VipMainWindow::exitFullScreen()
{
	if (isFullScreen())
		showMaximized();
}

void VipMainWindow::setCurrentTabDestroy(bool is_destroy)
{
	d_data->currentTabDestroy = is_destroy;
}

void VipMainWindow::autoSave()
{
	saveSession(vipGetDataDirectory() + "auto_session.session");
}
void VipMainWindow::autoLoad()
{
	loadSession(vipGetDataDirectory() + "auto_session.session");
}

void VipMainWindow::closeEvent(QCloseEvent* evt)
{
	bool no_close = false;

	QList<VipAbstractPlayer*> lst = this->findChildren<VipAbstractPlayer*>();

	// only ask for saving session if there is at least one SubWindow left
	if (lst.size() > 0 && d_data->sessionSavingEnabled) {
		int res = QMessageBox::question(this, "Save session", "Do you want to save your session?", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (res == QMessageBox::Yes) {
			saveSession(vipGetDataDirectory() + "last_session.session");
		}
		else if (res == QMessageBox::No) {
			saveSession(vipGetDataDirectory() + "base_session.session", 0, MainWindowState | Plugins | Settings);
			// remove last_session file
			QFile::remove(vipGetDataDirectory() + "last_session.session");
		}
		else
			no_close = true;
	}
	else {
		saveSession(vipGetDataDirectory() + "base_session.session", 0, MainWindowState | Plugins | Settings);
		// remove last_session file
		QFile::remove(vipGetDataDirectory() + "last_session.session");
	}

	if (no_close)
		evt->ignore();
	else {
		// unload all plugins before goind through the destructor
		VipLoadPlugins::instance().unloadPlugins();

		// close all top level widgets except this one
		QWidgetList toplevels = qApp->topLevelWidgets();
		for (int i = 0; i < toplevels.size(); ++i)
			if (QWidget* w = toplevels[i])
				if (w != this) {
					w->hide();
					// toplevels[i]->close();
				}
	}
}

void VipMainWindow::showOptions()
{
	vipGetOptions()->exec();
}

bool VipMainWindow::currentTabDestroying() const
{
	return d_data->currentTabDestroy;
}

static void open_widgets(VipMainWindow* win, const QList<QWidget*>& widgets)
{
	if (!widgets.size())
		return;

	VipDragWidget* last = nullptr;
	if (VipDisplayPlayerArea* area = win->displayArea()->currentDisplayPlayerArea()) {
		// get the main MultiDragWidget for this area
		VipMultiDragWidget* main = area->mainDragWidget(widgets);
		restore_widget(main);

		for (int i = 0; i < widgets.size(); ++i) {
			if (widgets[i] == main)
				continue;

			VipDragWidget* w = new VipDragWidget();
			w->setWidget(widgets[i]);
			last = w;

			if (main->mainCount()) {

				int max_cols = area->maxColumns();

				int width = main->subCount(main->mainCount() - 1);
				if (width < max_cols) {
					// add new column
					main->subResize(main->mainCount() - 1, width + 1);
					main->setWidget(main->mainCount() - 1, width, w);
				}
				else {
					// add new row
					main->mainResize(main->mainCount() + 1);
					main->subResize(main->mainCount() - 1, 1);
					main->setWidget(main->mainCount() - 1, 0, w);
				}
			}
			else {
				// first widget
				main->setWidget(0, 0, w);
			}
		}
	}
	if (last)
		last->setFocusWidget();
}

QList<VipAbstractPlayer*> VipMainWindow::openDevices(const QList<VipIODevice*>& all_devices, VipAbstractPlayer* player, VipDisplayPlayerArea* area)
{
	if (!area)
		area = displayArea()->currentDisplayPlayerArea();
	if (!area)
		return QList<VipAbstractPlayer*>();

	QStringList paths;

	QList<VipAbstractPlayer*> res;
	// if a player is given, open the devices in this player
	if (player) {
		for (int i = 0; i < all_devices.size(); ++i) {
			all_devices[i]->setParent(area->processingPool());
			if (!vipCreatePlayersFromProcessings(QList<VipIODevice*>() << all_devices[i], player).size())
				delete all_devices[i];
			else {

				// add to paths
				paths.push_back(all_devices[i]->fullPath());

				res << player;
				// delete devices without outputs (usually VipFileHandler)
				if (all_devices[i]->topLevelOutputCount() == 0)
					delete all_devices[i];
			}
		}
	}
	else {
		for (int i = 0; i < all_devices.size(); ++i) {
			all_devices[i]->setParent(area->processingPool());

			// add to paths
			paths.push_back(all_devices[i]->fullPath());
		}
		QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessings(all_devices, nullptr);

		// check the player count
		if (players.size() > 5) {
			if (QMessageBox::warning(
			      nullptr, "Opening many players", QString("You are about to open %1 players.\nDo you wish to continue?").arg(players.size()), QMessageBox::Ok | QMessageBox::Cancel) !=
			    QMessageBox::Ok) {
				// delete the devices and players
				for (int i = 0; i < all_devices.size(); ++i)
					delete all_devices[i];
				for (int i = 0; i < players.size(); ++i)
					delete players[i];
				return QList<VipAbstractPlayer*>();
			}
		}

		// delete devices without outputs (usually VipFileHandler)
		for (int i = 0; i < all_devices.size(); ++i) {
			if (all_devices[i]->topLevelOutputCount() == 0)
				delete all_devices[i];
		}

		if (players.size())
			res = players;
		else
			return QList<VipAbstractPlayer*>();

		//Add paths to history
		VipDeviceOpenHelper::addToHistory(paths);

		open_widgets(this, vipListCast<QWidget*>(players));
	}

	return res;
}

void VipMainWindow::openPlayers(const QList<VipAbstractPlayer*> players)
{
	if (!displayArea()->currentDisplayPlayerArea())
		return;

	open_widgets(this, vipListCast<QWidget*>(players));
}

QList<VipAbstractPlayer*> VipMainWindow::openPaths(const VipPathList& paths, VipAbstractPlayer* player, VipDisplayPlayerArea* area)
{
	bool _vip_openPathShowDialogOnError = property("_vip_openPathShowDialogOnError").toBool();
	setProperty("_vip_openPathShowDialogOnError", false);

	if (!paths.size())
		return QList<VipAbstractPlayer*>();
	if (!area)
		area = displayArea()->currentDisplayPlayerArea();

	if (!area && !(paths.size() == 1 && QFileInfo(paths.first().canonicalPath()).suffix() == "session")) {
		VIP_LOG_ERROR("Cannot open paths: you need to select a valid Workspace first");
		if (_vip_openPathShowDialogOnError)
			QMessageBox::warning(nullptr, "Error", "Cannot open paths: you need to select a valid Workspace first");
		return QList<VipAbstractPlayer*>();
	}

	VipProgress progress;
	progress.setModal(true);
	progress.setRange(0, paths.size() - 1);
	progress.setCancelable(paths.size() > 1);
	progress.setText("<b>Opening...</b>");

	QStringList errors;

	QList<VipIODevice*> all_devices;

	for (int i = 0; i < paths.size(); ++i) {
		if (progress.canceled())
			break;
		progress.setValue(i);

		VipPath path = paths[i];

		if (path.isDir()) {
			progress.setText("<b>Open</b> " + QFileInfo((paths[i].canonicalPath())).fileName());
			vipProcessEvents();

			QString dirname = path.canonicalPath();
			if (!dirname.isEmpty()) {
				VipIODevice* device = VipCreateDevice::create(path);

				if (device) {
					device->setMapFileSystem(path.mapFileSystem());
					// allow VipIODevice to display a progress bar
					device->setProperty("_vip_enableProgress", true);

					if (!device->open(VipIODevice::ReadOnly)) {
						VIP_LOG_WARNING("Fail to open " + dirname + (device->errorString().size() ? ", " + device->errorString() : ""));
						if (device->errorString().size())
							errors << dirname;
						delete device;
					}
					else {
						VIP_LOG_INFO("Open path: " + dirname);
						all_devices.append(device);
					}
				}
				else
					errors << dirname;
			}
		}
		else {
			QString filename = paths[i].canonicalPath();
			// session file
			if (QFileInfo(filename).suffix() == "session") {
				progress.setText("<b>Open</b> " + QFileInfo(filename).fileName());
				vipProcessEvents();
				if (!this->loadSession(filename))
					errors << filename;
			}
			else {

				QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(paths[i], QByteArray());
				VipIODevice* dev = VipCreateDevice::create(devices, paths[i]);
				if (dev) {
					dev->setPath(filename);
					dev->setMapFileSystem(paths[i].mapFileSystem());

					QString name = dev->removePrefix(dev->name());
					name = QFileInfo(name).fileName();
					if (name.size() > 50)
						name = name.mid(0, 47) + "...";
					progress.setText("<b>Open</b> " + name);
					vipProcessEvents();

					// allow VipIODevice to display a progress bar
					dev->setProperty("_vip_enableProgress", true);

					if (dev->open(VipIODevice::ReadOnly)) {
						all_devices << dev;
						VIP_LOG_INFO("Open path: " + filename);
					}
					else {
						QString err = dev->errorString();
						if (err.size())
							errors << filename;
						delete dev;
						VIP_LOG_WARNING("Fail to open " + filename + (err.size() ? ", " + err : ""));
					}
				}
				else {
					errors << filename;
					VIP_LOG_WARNING("No suitable device found for '" + filename + "'");
				}
			}
		}
	}

	QList<VipAbstractPlayer*> res = openDevices(all_devices, player, area);

	if (_vip_openPathShowDialogOnError && errors.size()) {
		// display a dialog box
		QString file_error;
		for (int i = 0; i < errors.size(); ++i)
			file_error += "\t" + errors[i] + "\n";
		QMessageBox::warning(nullptr, "Warning", "The following paths could not be opened:\n" + file_error);
	}

	this->setWindowState(this->windowState() | Qt::WindowActive);

	return res;
}

QList<VipAbstractPlayer*> VipMainWindow::openFiles()
{
	QStringList filters;
	filters << "Session file (*.session)";
	if (d_data->displayArea->currentDisplayPlayerArea()) {
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

	QStringList filenames = VipFileDialog::getOpenFileNames(this, "Open any kind of file", filters.join(";;"));
	if (filenames.isEmpty())
		return QList<VipAbstractPlayer*>();
	VipPathList paths;
	for (int i = 0; i < filenames.size(); ++i)
		paths.append(VipPath(filenames[i], false));
	if (paths.size())
		setOpenPathShowDialogOnError(true);
	return openPaths(paths, nullptr);
}

QList<VipAbstractPlayer*> VipMainWindow::openDir()
{
	if (!displayArea()->currentDisplayPlayerArea())
		return QList<VipAbstractPlayer*>();

	QString dir = VipFileDialog::getExistingDirectory(this, "Open an existing directory");
	if (dir.isEmpty())
		return QList<VipAbstractPlayer*>();
	VipPathList paths = VipPathList() << VipPath(dir, true);
	if (paths.size())
		setOpenPathShowDialogOnError(true);
	return openPaths(paths, nullptr);
}

void VipMainWindow::restart()
{
	if (this->close())
		vipSetRestartEnabled(5000);
}

void VipMainWindow::raiseOnTop()
{
	if (parentWidget()) {
		this->raise();
		return;
	}

	// Note that raise() alone does not work
	// see https://forum.qt.io/topic/6032/bring-window-to-front-raise-show-activatewindow-don-t-work-on-windows/11
	Qt::WindowFlags eFlags = this->windowFlags();
	eFlags |= Qt::WindowStaysOnTopHint;
	this->setWindowFlags(eFlags);

	eFlags &= ~Qt::WindowStaysOnTopHint;
	this->setWindowFlags(eFlags);

	this->show();
	this->raise();
}

void VipMainWindow::setOpenPathShowDialogOnError(bool enable)
{
	this->setProperty("_vip_openPathShowDialogOnError", enable);
}

QList<VipAbstractPlayer*> VipMainWindow::openPaths(const QStringList& filenames)
{
	VipPathList paths;
	for (int i = 0; i < filenames.size(); ++i) {
		QFileInfo info(filenames[i]);
		bool is_dir = (info.exists() && info.isDir());
		paths.append(VipPath(filenames[i], is_dir));
	}
	return openPaths(paths, nullptr);
}

void VipMainWindow::saveSession()
{
	VipExportSessionWidget* edit = new VipExportSessionWidget();
	VipGenericDialog dialog(edit, "Save current session");
	if (dialog.exec() == QDialog::Accepted)
		edit->exportSession();
}

void VipMainWindow::showHelp()
{
	QString p = QFileInfo(vipAppCanonicalPath()).canonicalPath();
	p.replace("\\", "/");
	if (!p.endsWith("/"))
		p += "/";
	vip_debug("help path: '%s'\n", p.toLatin1().data());

	if (QFileInfo(p + "help/index.html").exists())
		QDesktopServices::openUrl(QUrl(QFileInfo(p + "help/index.html").canonicalFilePath()));
	else
		QDesktopServices::openUrl(QUrl(QFileInfo(p + "help/html/index.html").canonicalFilePath()));
}

void VipMainWindow::showHelpCustom()
{
	if (QAction* act = qobject_cast<QAction*>(sender())) {
		QString text = act->text();
		text.remove(" help...");
		if (QDir("help/" + text).exists()) {
			QDesktopServices::openUrl(QUrl(QFileInfo("help/" + text + "/index.html").canonicalFilePath()));
		}
	}
}

bool VipMainWindow::workspacesMaximized() const
{
	return closeBar()->maximize->isChecked();
}

QMenu* VipMainWindow::createPopupMenu()
{
	if (QMenu* menu = QMainWindow::createPopupMenu()) {
		// remove left, right and bottom tool bars
		QList<QAction*> acts = menu->actions();
		for (int i = 0; i < acts.size(); ++i) {
			if (acts[i]->text() == "left area") {
				menu->removeAction(acts[i]);
				// delete acts[i];
			}
			else if (acts[i]->text() == "right area") {
				menu->removeAction(acts[i]);
				// delete acts[i];
			}
			else if (acts[i]->text() == "bottom area") {
				menu->removeAction(acts[i]);
				// delete acts[i];
			}
			else if (acts[i]->text() == "top area") {
				menu->removeAction(acts[i]);
				// delete acts[i];
			}
		}
		return menu;
	}
	return nullptr;
}

QString VipMainWindow::customTitle() const
{
	return d_data->iconBar->customTitle;
}
void VipMainWindow::setCustomTitle(const QString& title)
{
	d_data->iconBar->customTitle = title;
	d_data->iconBar->updateTitle();
}

int VipMainWindow::margin() const
{
	return d_data->left->maximumWidth();
}

void VipMainWindow::setMargin(int m)
{
	if (!d_data->left) {
		d_data->left = new QToolBar();
		d_data->left->setObjectName("left area");
		d_data->left->setWindowTitle("left area");
		d_data->left->setMovable(false);
		d_data->left->setAllowedAreas(Qt::LeftToolBarArea);
		this->addToolBar(Qt::LeftToolBarArea, d_data->left);

		d_data->right = new QToolBar();
		d_data->right->setObjectName("right area");
		d_data->right->setWindowTitle("right area");
		d_data->right->setMovable(false);
		d_data->right->setAllowedAreas(Qt::RightToolBarArea);
		this->addToolBar(Qt::RightToolBarArea, d_data->right);

		d_data->bottom = new QToolBar();
		d_data->bottom->setObjectName("bottom area");
		d_data->bottom->setWindowTitle("bottom area");
		d_data->bottom->setMovable(false);
		d_data->bottom->setAllowedAreas(Qt::BottomToolBarArea);
		this->addToolBar(Qt::BottomToolBarArea, d_data->bottom);

		d_data->top = new QToolBar();
		d_data->top->setObjectName("top area");
		d_data->top->setWindowTitle("top area");
		d_data->top->setMovable(false);
		d_data->top->setAllowedAreas(Qt::TopToolBarArea);
		d_data->top->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
		this->addToolBar(Qt::TopToolBarArea, d_data->top);
		this->addToolBarBreak();
	}

	d_data->left->setMinimumWidth(m);
	d_data->left->setMaximumWidth(m);

	d_data->right->setMinimumWidth(m);
	d_data->right->setMaximumWidth(m);

	d_data->bottom->setMinimumHeight(m);
	d_data->bottom->setMaximumHeight(m);

	d_data->top->setMinimumHeight(m);
	d_data->top->setMaximumHeight(m);
}

void VipMainWindow::setMaxColumnsForWorkspace(int maxc)
{
	if (VipDisplayPlayerArea* area = displayArea()->currentDisplayPlayerArea()) {

		closeBar()->maxCols->blockSignals(true);
		closeBar()->maxCols->setValue(maxc);
		closeBar()->maxCols->blockSignals(false);
		area->setMaxColumns(maxc);
	}
}

void VipMainWindow::maximizeWorkspaces(bool enable)
{
	QList<QWidget*> objects = this->findChildren<QWidget*>();
	objects.append(this);

	for (int i = 0; i < objects.size(); ++i)
		vipFDSwitchToMinimalDisplay().callAllMatch(objects[i], enable);

	closeBar()->maximize->blockSignals(true);
	closeBar()->maximize->setChecked(enable);
	closeBar()->maximize->blockSignals(false);

	// create maximize button if necessary
	QToolButton* maximize_button = vipGetMainWindow()->property("_vip_maximizedButton").value<QToolButton*>();
	if (!maximize_button) {
		maximize_button = new QToolButton();
		maximize_button->setAutoRaise(true);
		maximize_button->setIcon(vipIcon("show_normal.png"));
		maximize_button->setToolTip("<b>Maximize workspaces</b><br>Maximize workspaces by hiding all surrounding tool widgets");
		maximize_button->setMaximumSize(20, 20);
		maximize_button->hide();
		maximize_button->setParent(vipGetMainWindow());
		maximize_button->move(vipGetMainWindow()->width() - maximize_button->width(), 0);
		QObject::connect(maximize_button, &QToolButton::clicked, std::bind(&VipMainWindow::maximizeWorkspaces, this, false));
		vipGetMainWindow()->setProperty("_vip_maximizedButton", QVariant::fromValue(maximize_button));
	}

	if (enable) {
		QByteArray state = vipGetMainWindow()->saveState();
		vipGetMainWindow()->setProperty("_vip_state", state);
		vipGetMainWindow()->setProperty("_vip_wmaximized", true);
		QList<VipToolWidget*> tools = vipGetMainWindow()->findChildren<VipToolWidget*>();
		for (int i = 0; i < tools.size(); ++i)
			tools[i]->hide();

		// Hide all player tool bars
		for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
			vipGetMainWindow()->displayArea()->widget(i)->topWidget()->hide();
		}

		vipGetMainWindow()->displayArea()->displayTabWidget()->tabBar()->hide();

		// all all title bars
		vipGetMainWindow()->closeBar()->hide();
		vipGetMainWindow()->fileToolBar()->hide();
		vipGetMainWindow()->iconBar()->hide();
		vipGetMainWindow()->toolsToolBar()->hide();

		maximize_button->move(vipGetMainWindow()->width() - maximize_button->width(), 0);
		maximize_button->show();
	}
	else {
		QByteArray state = vipGetMainWindow()->property("_vip_state").toByteArray();
		vipGetMainWindow()->setProperty("_vip_wmaximized", false);
		vipGetMainWindow()->restoreState(state);

		for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
			vipGetMainWindow()->displayArea()->widget(i)->topWidget()->show();
		}

		vipGetMainWindow()->displayArea()->displayTabWidget()->tabBar()->show();

		vipGetMainWindow()->closeBar()->show();
		vipGetMainWindow()->fileToolBar()->show();
		vipGetMainWindow()->iconBar()->show();
		vipGetMainWindow()->toolsToolBar()->show();

		maximize_button->hide();
	}
}

void VipMainWindow::displayGraphicsProcessingPlayer()
{
	// if (VipDisplayPlayerArea * area = displayArea()->currentDisplayPlayerArea())
	{
		// VipGraphicsEditorPlayer * player = new VipGraphicsEditorPlayer();
		//  player->setWindowTitle("Workspace " + QString::number(area->id()) + " graphics processing tree");
		//  player->view()->scene()->setProcessingPool(area->processingPool());
		//  VipMultiDragWidget *w = vipCreateFromBaseDragWidget(vipCreateFromWidgets(QList<QWidget*>() << player));
		//  area->addWidget(w);
		//  w->show();
	}
}

void VipMainWindow::setSessionSavingEnabled(bool enable)
{
	if (d_data->sessionSavingEnabled != enable) {
		d_data->sessionSavingEnabled = enable;
		d_data->saveSessionAction->setVisible(enable);
	}
}

bool VipMainWindow::sessionSavingEnabled() const
{
	return d_data->sessionSavingEnabled;
}

int VipMainWindow::adjustColorPalette() const
{
	return VipGuiDisplayParamaters::instance()->itemPaletteFactor();
}

void VipMainWindow::setAdjustColorPalette(int factor)
{
	VipGuiDisplayParamaters::instance()->setItemPaletteFactor(factor);
}

void VipMainWindow::aboutDialog()
{
	VipAboutDialog dial;
	dial.exec();
}

void VipMainWindow::startUpdateThread()
{
	stopUpdateThread();
	if (!d_data->updateThread)
		d_data->updateThread = new UpdateThread(this);
	d_data->updateThread->mainWindow = this;
	d_data->updateThread->start();
}

void VipMainWindow::stopUpdateThread()
{
	if (d_data->updateThread) {
		d_data->updateThread->mainWindow = nullptr;
		d_data->updateThread->wait();
	}
}

VipMainWindow* vipGetMainWindow()
{
	static VipMainWindow* win = nullptr;
	if (!win) {
		win = new VipMainWindow();
		// init() function cannot be called from within the constructor, so just call it after
		win->metaObject()->invokeMethod(win, "init", Qt::DirectConnection);
	}
	return win;
}

VipFunctionDispatcher<1>& vipFDCreateWidgetFromIODevice()
{
	static VipFunctionDispatcher<1> disp;
	return disp;
}

VipFunctionDispatcher<2>& vipFDSwitchToMinimalDisplay()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipBaseDragWidget* vipCreateWidgetFromProcessingObject(VipProcessingObject* object)
{
	if (qobject_cast<VipIODevice*>(object)) {
		VipIODevice* device = static_cast<VipIODevice*>(object);
		VipFunctionDispatcher<1>::function_list_type lst = vipFDCreateWidgetFromIODevice().exactMatch(device);
		if (lst.size())
			return lst.back()(device);
	}

	QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(object, nullptr);
	return vipCreateFromWidgets(vipListCast<QWidget*>(players));
}

VipBaseDragWidget* vipCreateFromWidgets(const QList<QWidget*>& players)
{
	// we've got the widgets, now create the VipDragWidget objects
	QList<VipDragWidget*> dragWidgets;
	for (int i = 0; i < players.size(); ++i) {
		VipDragWidget* drag = new VipDragWidget();
		drag->setWidget(players[i]);
		dragWidgets.append(drag);
	}

	// if size > 1, reorganize into a VipMultiDragWidget
	if (dragWidgets.size() == 0)
		return nullptr;
	else if (dragWidgets.size() == 1)
		return dragWidgets.first();
	else {
		int width = std::ceil(std::sqrt((double)dragWidgets.size()));
		// int height = std::ceil(double(dragWidgets.size())/width);

		VipMultiDragWidget* drag = new VipMultiDragWidget();

		int w = 0, h = 0;
		for (int i = 0; i < dragWidgets.size(); ++i) {
			// max width: go to next row
			if (drag->subCount(h) >= width) {
				++h;
				w = 0;
			}

			if (drag->mainCount() <= h)
				drag->mainResize(h + 1);

			// width smaller than max width: resize
			if (drag->subCount(h) <= w)
				drag->subResize(h, w + 1);

			// add the drag widget
			drag->setWidget(h, w, dragWidgets[i]);
			++w;
		}

		return drag;
	}
}

VipMultiDragWidget* vipCreateFromBaseDragWidget(VipBaseDragWidget* w)
{
	if (!w)
		return nullptr;

	if (qobject_cast<VipMultiDragWidget*>(w))
		return static_cast<VipMultiDragWidget*>(w);
	else {
		VipMultiDragWidget* multi = new VipMultiDragWidget();
		multi->setWidget(0, 0, w);
		return multi;
	}
}

// Handle drag & drop of VipPlotItem
#include "VipMimeData.h"
#include "VipPlayer.h"

static bool supportPlotItem(VipPlotMimeData* mime, QWidget* drop_widget)
{
	Q_UNUSED(mime)
	Q_UNUSED(drop_widget)
	return true;
}

#include "VipMimeData.h"

static VipBaseDragWidget* drop_mime_data_widget(VipDisplayPlayerArea* area, VipBaseDragWidget* widget, QWidget* target)
{
	if (!area || !widget || !target)
		return nullptr;

	// drop on a splitter handle
	if (qobject_cast<VipDragWidgetHandle*>(target) || qobject_cast<VipMultiDragWidget*>(target)) {
		// drop a VipDragWidget: allowed
		if (qobject_cast<VipDragWidget*>(widget))
			return widget;
		else {
			// multi drag widget: open each VipDragWidget separatly
			QList<VipDragWidget*> widgets = widget->findChildren<VipDragWidget*>();
			for (int i = 0; i < widgets.size(); ++i)
				area->addWidget(widgets[i]);
			// return null to avoid inserting the multi drag widget on the handle
			return nullptr;
		}
	}
	else if (qobject_cast<VipViewportArea*>(target)) {
		// empty VipViewportArea or just a VipDragWidget: add it through the area
		if (!target->findChild<VipMultiDragWidget*>() || qobject_cast<VipDragWidget*>(widget)) {
			area->addWidget(widget);
			return nullptr;
		}
		else {
			// multi drag widget: open each VipDragWidget separatly
			QList<VipDragWidget*> widgets = widget->findChildren<VipDragWidget*>();
			for (int i = 0; i < widgets.size(); ++i)
				area->addWidget(widgets[i]);
			// return null to avoid inserting the multi drag widget on the handle
			return nullptr;
		}
	}
	return nullptr;
}

static VipBaseDragWidget* dropPlotItem(VipPlotMimeData* mime, QWidget* drop_widget)
{
	QList<VipPlotItem*> items = mime->plotData(nullptr, drop_widget);
	VipAbstractPlayer* player = nullptr;
	VipBaseDragWidget* res = nullptr;

	if (VipMimeDataCoordinateSystem* mime_c = qobject_cast<VipMimeDataCoordinateSystem*>(mime)) {
		// handle more complex VipMimeDataCoordinateSystem
		QList<VipAbstractPlayer*> players = mime_c->players();
		if (players.size()) {
			QList<VipPlotPlayer*> plots = vipListCast<VipPlotPlayer*>(players);
			if (plots.size())
				player = plots.first();

			res = vipCreateFromWidgets(vipListCast<QWidget*>(players));
			res = drop_mime_data_widget(VipDisplayPlayerArea::fromChildWidget(drop_widget), res, drop_widget);
		}
	}

	if (!items.size())
		return res;

	VipAbstractPlayer* pl = player;
	if (!pl)
		pl = VipAbstractPlayer::findAbstractPlayer(items.first());
	if (!pl)
		return nullptr;

	VipProcessingPool* src_pool = pl->processingPool();
	VipProcessingPool* dst_pool = VipMimeDataCoordinateSystem::fromWidget(drop_widget);

	// TODO: handle different dst processing pool
	// handle drop inside a plot player from a different processing pool

	if (src_pool == dst_pool) {
		// create a new player (if required) and drop items inside

		VipAbstractPlayer* _new = (pl == player ? pl : pl->createEmpty());
		QList<VipAbstractScale*> scales;
		VipCoordinateSystem::Type type = _new->plotWidget2D()->area()->standardScales(scales);

		int count = 0;

		for (int i = 0; i < items.size(); ++i) {
			// disable dropping of VipPlotSpectrogram
			if (qobject_cast<VipPlotSpectrogram*>(items[i]))
				continue;

			// use the standard approach by setting the axes
			items[i]->setParentItem(nullptr);
			if (items[i]->scene())
				items[i]->scene()->removeItem(items[i]);

			items[i]->setAxes(scales, type);
			++count;
		}

		if (count) {
			if (!res) {
				res = new VipDragWidget();
				static_cast<VipDragWidget*>(res)->setWidget(_new);

				res = drop_mime_data_widget(VipDisplayPlayerArea::fromChildWidget(drop_widget), res, drop_widget);
				res->setFocusWidget();

				return res;
			}
		}
		else {
			if (_new != player)
				delete _new; // delete newly created player
			return nullptr;
		}
	}
	else {
		// different processing pool: copy the items
		VipMimeDataDuplicatePlotItem duplicate(items);
		duplicate.plotData(nullptr, drop_widget);

		QList<VipAbstractPlayer*> players = duplicate.players();
		if (players.size()) {
			res = vipCreateFromWidgets(vipListCast<QWidget*>(players));
			res = drop_mime_data_widget(VipDisplayPlayerArea::fromChildWidget(drop_widget), res, drop_widget);
			res->setFocusWidget();
			return res;
		}
	}
	return nullptr;
}

static VipBaseDragWidget* dropMimeData(QMimeData* mime, QWidget* drop_widget)
{
	if (mime->formats().size() == 1 && mime->formats().first() == "application/dragwidget") {
		// drop a VipBaseDragWidget into another workspace
		VipBaseDragWidgetMimeData* m = static_cast<VipBaseDragWidgetMimeData*>(mime);
		if (VipDragWidget* d = qobject_cast<VipDragWidget*>(m->dragWidget)) {
			if (VipDisplayPlayerArea* current_area = VipDisplayPlayerArea::fromChildWidget(m->dragWidget))
				if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChildWidget(drop_widget)) {
					if (area != current_area) { // drop from one workspace to another
						if (VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(d->widget())) {

							QList<VipDisplayObject*> objs = pl->displayObjects();
							QList<VipProcessingObject*> srcs;
							for (int i = 0; i < objs.size(); ++i)
								srcs += objs[i]->allSources();
							QList<VipDisplayObject*> all_disps;
							for (int i = 0; i < srcs.size(); ++i)
								all_disps += vipListCast<VipDisplayObject*>(srcs[i]->allSinks());
							if (vipToSet(objs) == vipToSet(all_disps)) {
								// drop is allowed
							}
							else
								return nullptr;
						}
						if (d->isMaximized()) {
							d->showNormal();
						}
						d->setFocusWidget();
						area->addWidget(d);
					}
				}
		}
		return nullptr;
	}
	else {
		QList<QUrl> urls = mime->urls();
		QStringList paths;
		for (int i = 0; i < urls.size(); ++i) {
			if (urls[i].isLocalFile()) {
				paths.append(urls[i].toLocalFile());
			}
		}
		VipMimeDataPaths m;
		m.setPaths(paths);
		return dropPlotItem(&m, drop_widget);
	}
}

/* static void minimialDisplayMainWindow(VipMainWindow* window, bool minimal)
{
	window->showTabBar()->setEnabled(minimal);

	if (minimal) {
		// save the state
		if (window->property("save_state").userType() == 0)
			window->setProperty("save_state", window->saveState());

		// hide all dock widgets

		QList<QDockWidget*> docks = window->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
		for (int i = 0; i < docks.size(); ++i)
			docks[i]->setVisible(!minimal);

		// hide/show the tab area
		window->displayArea()->displayTabWidget()->tabBar()->setVisible(!minimal);

		if (window->displayArea()->currentDisplayPlayerArea()) {
			window->setProperty("visible_timerange", window->displayArea()->currentDisplayPlayerArea()->playWidget()->timeRangeVisible());
			window->displayArea()->currentDisplayPlayerArea()->playWidget()->setTimeRangeVisible(false);
		}
	}
	else {
		// restore the state

		QByteArray state = window->property("save_state").toByteArray();
		if (state.size())
			window->restoreState(state);
		window->setProperty("save_state", QVariant());
		window->displayArea()->displayTabWidget()->tabBar()->setVisible(true);
		if (window->displayArea()->currentDisplayPlayerArea())
			window->displayArea()->currentDisplayPlayerArea()->playWidget()->setTimeRangeVisible(window->property("visible_timerange").toBool());
	}
}*/


/* static void minimalDisplayPlayer(VipPlayer2D* player, bool minimal)
{
	if (minimal) {
		if (player->isVisible()) {
			if (VipPlotPlayer* p = qobject_cast<VipPlotPlayer*>(player)) {
				p->setProperty("legendPosition", p->legendPosition());
				p->setLegendPosition(Vip::LegendHidden);
			}
		}
	}
	else {
		if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(player)) {
			QVariant p = pl->property("legendPosition");
			if (p.userType() != 0)
				pl->setLegendPosition((Vip::PlayerLegendPosition)p.toInt());
			else
				pl->setLegendPosition(VipGuiDisplayParamaters::instance()->legendPosition());
			pl->setProperty("legendPosition", QVariant());
		}
	}
}*/

/* static void minimalDisplayVideoPlayer(VipVideoPlayer* player, bool minimal)
{
	if (minimal) {
		player->setProperty("show_axes", player->isShowAxes());
		player->showAxes(false);
	}
	else {
		QVariant p = player->property("show_axes");
		if (p.userType() != 0) {
			player->showAxes(p.toBool());
		}
		else
			player->showAxes(true);

		player->setProperty("show_axes", QVariant());
	}
}*/

VipArchive& vipSaveBaseDragWidget(VipArchive& arch, VipBaseDragWidget* w)
{
	if (!w) {
		arch.setError("nullptr VipBaseDragWidget");
		return arch;
	}

	// find all players and related processing objects
	QList<VipAbstractPlayer*> players = w->findChildren<VipAbstractPlayer*>();
	QList<VipProcessingObject*> objects;
	for (int i = 0; i < players.size(); ++i) {
		QList<VipDisplayObject*> displays = players[i]->displayObjects();
		objects += vipListCast<VipProcessingObject*>(displays);
		for (int j = 0; j < displays.size(); ++j)
			objects += displays[j]->allSources();
	}
	// make unique
	objects = vipToSet(objects).values();

	QVariantMap metadata;
	metadata["session_type"] = VipMainWindow::DragWidget;
	arch.start("VipSession", metadata);

	// save the Thermavip version
	arch.content("version", QString(VIP_VERSION));

	arch.start("BaseDragWidget");
	// new in 2.2.17
	arch.content("width", w->width());
	arch.content("height", w->height());

	// save the players
	arch.start("Widgets");
	arch.content(QVariant::fromValue(w));
	arch.end();

	// save the processings
	arch.start("Processings");

	if (objects.size() && objects.first()->parentObjectPool())
		// save the processing pool time
		arch.content("time", objects.first()->parentObjectPool()->time());
	else
		arch.content("time", 0);

	for (int i = 0; i < objects.size(); ++i) {
		if (!objects[i]->property("_vip_no_serialize").toBool())
			arch.content(objects[i]);
	}
	arch.end(); // Processings

	arch.end(); // BaseDragWidget
	arch.end(); // VipSession
	return arch;
}

VipBaseDragWidget* vipLoadBaseDragWidget(VipArchive& arch, VipDisplayPlayerArea* target)
{
	bool has_session = false;
	arch.save();
	if (arch.start("VipSession")) {
		QString ver = arch.read("version").toString();
		if (ver.isEmpty()) {
			VIP_LOG_ERROR("Cannot load session file: cannot find version number");
			return nullptr;
		}
		if (!isVersionValid(VIP_MINIMAL_SESSION_VERSION, ver)) {
			VIP_LOG_ERROR("Cannot load session file: wrong version number");
			return nullptr;
		}
		has_session = true;
	}
	else
		arch.restore();

	if (!arch.start("BaseDragWidget"))
		return nullptr;

	// new in 2.2.17
	arch.save();
	int width = 0, height = 0;
	if (!arch.content("width", width) || !arch.content("height", height))
		arch.restore();

	// load players
	arch.start("Widgets");

	VipBaseDragWidget* w = arch.read().value<VipBaseDragWidget*>();
	arch.end();
	if (!w) {
		arch.end();
		return nullptr;
	}

	// load processings
	arch.start("Processings");
	qint64 time = arch.read("time").toLongLong();
	QList<VipProcessingObject*> objects;
	while (!arch.hasError()) {
		if (VipProcessingObject* obj = arch.read().value<VipProcessingObject*>()) {
			// open the read only devices
			if (VipIODevice* device = qobject_cast<VipIODevice*>(obj)) {
				if (device->supportedModes() & VipIODevice::ReadOnly)
					device->open(VipIODevice::ReadOnly);
			}
			objects << obj;
		}
	}
	arch.end(); // end Processings
	arch.end(); // end BaseDragWidget
	if (has_session)
		arch.end(); // VipSession

	// first, add the processings in a temporary processing pool in order to open the connections
	VipProcessingPool tmp;
	// Set the property _vip_useParentPool:
	// when opening the connections, the pool name in the connection address won't be used
	tmp.setProperty("_vip_useParentPool", true);
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->setParent(&tmp);

	// open connections
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->openAllConnections();

	// set the real parent processing pool
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->setParent(target->processingPool());

	VipMultiDragWidget* m = vipCreateFromBaseDragWidget(w);
	target->addWidget(m);

	// Now, we need to call again the playerCreated() function for all players to trigger again the vipFDPlayerCreated() dispatcher.
	// Indeed, when loading a session, the dispatcher is first called on a non connected player which might be an issue for some plugins.
	QList<VipPlayer2D*> players = target->dragWidgetArea()->findChildren<VipPlayer2D*>();
	for (int i = 0; i < players.size(); ++i)
		QMetaObject::invokeMethod(players[i], "playerCreated", Qt::QueuedConnection);

	// reset processing pool
	VipProcessingPool* pool = target->processingPool();
	// qint64 time = pool->time();

	QMetaObject::invokeMethod(target->playWidget()->area(), "updateProcessingPool", Qt::QueuedConnection);
	QMetaObject::invokeMethod(target->playWidget()->area(), "setTime", Qt::QueuedConnection, Q_ARG(double, (double)time));
	// we still need to reload the processing pool to update Resource devices
	QMetaObject::invokeMethod(pool, "reload", Qt::QueuedConnection);

	if (width != 0 && height != 0 && !m->isMaximized())
		QMetaObject::invokeMethod(m, "setSize", Qt::QueuedConnection, Q_ARG(QSize, QSize(width, height)));

	return m;
}

bool vipSaveImage(VipBaseDragWidget* w)
{
	QString filename = VipFileDialog::getSaveFileName(nullptr, ("Save image as"), "Image file (*.png *.bmp *.jpg *.jpeg *.ppm *.tiff *.tif *.xbm *.xpm)");
	if (!filename.isEmpty()) {
		VipRenderState state;
		VipRenderObject::startRender(w, state);

		vipProcessEvents();

		QList<VipDragWidget*> ws = w->findChildren<VipDragWidget*>();
		for (int i = 0; i < ws.size(); ++i)
			vipFDAboutToRender().callAllMatch(ws[i]->widget());

		bool use_transparency = (QFileInfo(filename).suffix().compare("png", Qt::CaseInsensitive) == 0);

		QPixmap pixmap(w->size());
		if (use_transparency)
			pixmap.fill(QColor(255, 255, 255, 1));
		else
			pixmap.fill(QColor(255, 255, 255));

		QPainter p(&pixmap);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
		VipRenderObject::renderObject(w, &p, QPoint(), true, false);

		VipRenderObject::endRender(w, state);

		if (!pixmap.save(filename)) {
			VIP_LOG_ERROR("Failed to save image " + filename);
			return false;
		}
		else {
			VIP_LOG_INFO("Saved image in " + filename);
			return true;
		}
	}
	return false;
}

bool vipSaveSession(VipBaseDragWidget* w)
{
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save widget as", "Session file (*.session)");
	if (!filename.isEmpty()) {
		VipXOfArchive arch(filename);
		vipSaveBaseDragWidget(arch, w);
		arch.close();
		return true;
	}
	return false;
}

bool vipPrint(VipBaseDragWidget* w)
{
	QPrinter printer(QPrinter::HighResolution);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	QRect bounding(QPoint(0, 0), w->size());
	// get bounding rect in millimeters
	QScreen* screen = qApp->primaryScreen();
	int thisScreen = QApplication::desktop()->screenNumber(w);
	if (thisScreen >= 0)
		screen = qApp->screens()[thisScreen];
#else
	QRect bounding(QPoint(0, 0), w->size());
	// get bounding rect in millimeters
	QScreen* screen = qApp->primaryScreen();
	int thisScreen = qApp->screens().indexOf(w->screen());
	if (thisScreen < 0)
		thisScreen = 0;
	if (thisScreen >= 0)
		screen = qApp->screens()[thisScreen];
#endif

	QSizeF screen_psize = screen->physicalSize();
	QSize screen_size = screen->size();

	qreal mm_per_pixel_x = screen_psize.width() / screen_size.width();
	qreal mm_per_pixel_y = screen_psize.height() / screen_size.height();
	QSizeF paper_size(bounding.width() * mm_per_pixel_x, bounding.height() * mm_per_pixel_y);

	// printer.setPageSize(QPrinter::Custom);
	// printer.setPaperSize(paper_size, QPrinter::Millimeter);
	// printer.setPageMargins(0, 0, 0, 0, QPrinter::Millimeter);
	printer.setPageSize(QPageSize(paper_size, QPageSize::Millimeter));
	printer.setResolution(600);

	QPrintDialog printDialog(&printer, nullptr);
	if (printDialog.exec() == QDialog::Accepted) {

		VipRenderState state;
		VipRenderObject::startRender(w, state);

		vipProcessEvents();

		QPainter p(&printer);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
		// this->draw(&p);
		VipRenderObject::renderObject(w, &p, QPoint(), true, false);

		VipRenderObject::endRender(w, state);
		return true;
	}
	return false;
}

VipFunctionDispatcher<1>& vipFDAboutToRender()
{
	static VipFunctionDispatcher<1> inst;
	return inst;
}

static int registerFunctions()
{
	vipAcceptDragMimeData().append<bool(VipPlotMimeData*, QWidget*)>(supportPlotItem);
	vipDropMimeData().append<VipBaseDragWidget*(QMimeData*, QWidget*)>(dropMimeData);
	vipDropMimeData().append<VipBaseDragWidget*(VipPlotMimeData*, QWidget*)>(dropPlotItem);
	/*vipFDSwitchToMinimalDisplay().append<void(VipMainWindow*, bool)>(minimialDisplayMainWindow);
	vipFDSwitchToMinimalDisplay().append<void(VipPlayer2D*, bool)>(minimalDisplayPlayer);
	vipFDSwitchToMinimalDisplay().append<void(VipVideoPlayer*, bool)>(minimalDisplayVideoPlayer);
	vipFDSwitchToMinimalDisplay().append<void(VipDragWidget*, bool)>(minimalDisplayCustomDragWidget);*/
	return 0;
}

static int _registerFunctions = vipAddInitializationFunction(registerFunctions);
