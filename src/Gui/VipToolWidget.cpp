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

#include "VipToolWidget.h"
#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipProgress.h"

#include <QApplication>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QScrollArea>
#include <QToolButton>
#include <qapplication.h>
#include <qlabel.h>
#include <qscreen.h>
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#include <QDesktopWidget>
#endif

#include "VipDragWidget.h"
#include "VipWidgetResizer.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
static bool isTopLevel(const QWidget* w)
{
	return w->isTopLevel();
}
#else
static bool isTopLevel(const QWidget* w)
{
	return w->isWindow();
}
#endif

class VipToolWidgetResizer : public VipWidgetResizer
{
public:
	VipToolWidgetResizer(QWidget* parent)
	  : VipWidgetResizer(parent)
	{
	}

protected:
	virtual bool isTopLevelWidget(const QPoint& screen_pos = QPoint()) const
	{
		if (!isTopLevel(parent()))
			return false;
		if (screen_pos == QPoint())
			return true;

		QWidget* under_mouse = QApplication::widgetAt(screen_pos);
		while (under_mouse) {
			if (isTopLevel(under_mouse) && under_mouse == parent())
				return true;
			else if (isTopLevel(under_mouse) && under_mouse != vipGetMainWindow())
				return false;
			under_mouse = under_mouse->parentWidget();
		}
		return true;
	}
};

class NoSizeLable : public QLabel
{
public:
	NoSizeLable(QWidget* parent = nullptr)
	  : QLabel(parent)
	{
		setMinimumWidth(10);
	}

	virtual QSize sizeHint() const { return QSize(); }
};

class VipToolWidgetTitleBar::PrivateData
{
public:
	QLabel* icon;
	QLabel* label;
	QToolBar* bar;
	QToolButton* floating;
	QToolButton* close;
	QToolButton* restore;
	QToolButton* maximize;
	QColor patternColor;
	bool displayWindowIcon;
};

VipToolWidgetTitleBar::VipToolWidgetTitleBar(VipToolWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->icon = new QLabel(this);
	d_data->label = new NoSizeLable(this);
	d_data->bar = new QToolBar();
	d_data->floating = new QToolButton();
	d_data->close = new QToolButton();
	d_data->maximize = new QToolButton();
	d_data->restore = new QToolButton();
	d_data->patternColor = Qt::gray;
	d_data->displayWindowIcon = false;

	d_data->bar->setIconSize(QSize(18, 18));
	d_data->bar->setAutoFillBackground(false);

	d_data->label->setIndent(5);
	d_data->label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	d_data->label->setAttribute(Qt::WA_TransparentForMouseEvents);
	d_data->label->setFocusPolicy(Qt::NoFocus);

	d_data->floating->setAutoRaise(true);
	d_data->floating->setCheckable(true);
	d_data->floating->setIcon(vipIcon("pin.png"));
	d_data->floating->setToolTip("Make panel floating");
	d_data->floating->setMaximumSize(18, 18);

	d_data->close->setAutoRaise(true);
	d_data->close->setIcon(vipIcon("close.png"));
	d_data->close->setToolTip("Close");
	d_data->close->setMaximumSize(18, 18);

	d_data->restore->setAutoRaise(true);
	d_data->restore->setIcon(vipIcon("restore.png"));
	d_data->restore->setToolTip("Restore");
	d_data->restore->setMaximumSize(18, 18);
	d_data->restore->hide();

	d_data->maximize->setAutoRaise(true);
	d_data->maximize->setIcon(vipIcon("maximize.png"));
	d_data->maximize->setToolTip("Maximize");
	d_data->maximize->setMaximumSize(18, 18);
	d_data->maximize->hide();

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addItem(new QSpacerItem(3, 3));
	lay->addWidget(d_data->icon);
	lay->addItem(new QSpacerItem(3, 3));
	lay->addWidget(d_data->label);
	lay->addWidget(d_data->bar);
	lay->addStretch(1);
	lay->addWidget(d_data->restore);
	lay->addWidget(d_data->maximize);
	lay->addWidget(d_data->floating);
	lay->addWidget(d_data->close);
	lay->setSpacing(0);
	lay->setContentsMargins(0, 3, 2, 3);
	setLayout(lay);

	d_data->label->setMaximumWidth(350);
	d_data->label->setText(parent->windowTitle());
	QSize s = parent->windowIcon().actualSize(QSize(100, 100));
	if (!s.isEmpty())
		d_data->icon->setPixmap(parent->windowIcon().pixmap(s).scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	d_data->icon->setVisible(false); // !parent->windowIcon().isNull());

	// this->setMaximumHeight(30);
	this->setFocusPolicy(Qt::ClickFocus);

	connect(parent, SIGNAL(allowedAreasChanged(Qt::DockWidgetAreas)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(featuresChanged(QDockWidget::DockWidgetFeatures)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(topLevelChanged(bool)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(visibilityChanged(bool)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(windowTitleChanged(const QString&)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(windowIconChanged(const QIcon&)), this, SLOT(updateTitleAndPosition()));

	connect(d_data->restore, SIGNAL(clicked(bool)), parent, SLOT(showNormal()));
	connect(d_data->maximize, SIGNAL(clicked(bool)), parent, SLOT(floatWidget()));
	connect(d_data->maximize, SIGNAL(clicked(bool)), parent, SLOT(showMaximized()));
	connect(d_data->restore, SIGNAL(clicked(bool)), this, SLOT(updateTitleAndPosition()));
	connect(d_data->maximize, SIGNAL(clicked(bool)), this, SLOT(updateTitleAndPosition()));
	connect(d_data->floating, SIGNAL(clicked(bool)), parent, SLOT(setFloatingTool(bool)));
	connect(d_data->close, SIGNAL(clicked(bool)), parent, SLOT(close()));

	// this->setMinimumHeight(30);
}

VipToolWidgetTitleBar::~VipToolWidgetTitleBar()
{
}

VipToolWidget* VipToolWidgetTitleBar::parent() const
{
	return qobject_cast<VipToolWidget*>(parentWidget());
}

QToolBar* VipToolWidgetTitleBar::toolBar() const
{
	return d_data->bar;
}

QIcon VipToolWidgetTitleBar::closeButton() const
{
	return d_data->close->icon();
}

QIcon VipToolWidgetTitleBar::floatButton() const
{
	return d_data->floating->icon();
}

QColor VipToolWidgetTitleBar::textColor() const
{
	return d_data->label->palette().color(QPalette::WindowText);
}

QColor VipToolWidgetTitleBar::patternColor() const
{
	return d_data->patternColor;
}

bool VipToolWidgetTitleBar::displayWindowIcon() const
{
	return d_data->displayWindowIcon;
}

void VipToolWidgetTitleBar::setPatternColor(const QColor& c)
{
	d_data->patternColor = c;
	update();
}

void VipToolWidgetTitleBar::setDisplayWindowIcon(bool enable)
{
	d_data->displayWindowIcon = enable;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	const QPixmap pix = d_data->icon->pixmap() ? *d_data->icon->pixmap() : QPixmap();
#else
	const QPixmap pix = d_data->icon->pixmap(Qt::ReturnByValue);
#endif
	d_data->icon->setVisible(enable && !pix.isNull());
}

void VipToolWidgetTitleBar::VipToolWidgetTitleBar::setCloseButton(const QIcon& icon)
{
	d_data->close->setIcon(icon);
}

void VipToolWidgetTitleBar::setFloatButton(const QIcon& icon)
{
	d_data->floating->setIcon(icon);
}

void VipToolWidgetTitleBar::setTextColor(const QColor& c)
{
	QPalette p = d_data->label->palette();
	p.setColor(QPalette::WindowText, c);
	d_data->label->setPalette(p);
}

QIcon VipToolWidgetTitleBar::maximizeButton() const
{
	return d_data->maximize->icon();
}
QIcon VipToolWidgetTitleBar::restoreButton() const
{
	return d_data->restore->icon();
}

void VipToolWidgetTitleBar::setMaximizeButton(const QIcon& i)
{
	d_data->maximize->setIcon(i);
}
void VipToolWidgetTitleBar::setRestoreButton(const QIcon& i)
{
	d_data->restore->setIcon(i);
}

void VipToolWidgetTitleBar::updateTitle()
{
	if (VipToolWidget* tool = parent()) {
		QFontMetrics m(d_data->label->font());
		int width = this->width();
		// if (width < 200)
		//	width = 200;
		QString text = m.elidedText(tool->windowTitle(),
					    Qt::ElideRight, // d_data->label->maximumWidth()
					    width - 45);

		d_data->label->setText(text);
		d_data->label->setToolTip(tool->windowTitle());
		this->setToolTip(tool->windowTitle());
	}
}

void VipToolWidgetTitleBar::updateTitleAndPosition()
{
	if (VipToolWidget* tool = parent()) {
		updateTitle();
		if (d_data->displayWindowIcon) {
			QSize s = tool->windowIcon().actualSize(QSize(100, 100));
			if (!s.isEmpty())
				d_data->icon->setPixmap(tool->windowIcon().pixmap(s).scaled(22, 22,Qt::KeepAspectRatio,Qt::SmoothTransformation));
			d_data->icon->setVisible(!tool->windowIcon().isNull());
		}

		d_data->floating->blockSignals(true);
		d_data->floating->setChecked(tool->isFloating());
		d_data->floating->blockSignals(false);

		d_data->close->setVisible(tool->features() & QDockWidget::DockWidgetClosable);
		d_data->floating->setVisible(tool->features() & QDockWidget::DockWidgetFloatable);

		if (!tool->keepFloatingUserSize()) {
			d_data->maximize->hide();
			d_data->restore->hide();
		}
		else {
			d_data->restore->setVisible(tool->isMaximized());
			d_data->maximize->setVisible(!tool->isMaximized());
		}
	}
}

void VipToolWidgetTitleBar::paintEvent(QPaintEvent*)
{
	QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	opt.init(this);
#else
	opt.initFrom(this);
#endif
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

	QWidget* endw = nullptr;
	if (d_data->restore->isVisible())
		endw = d_data->restore;
	else if (d_data->maximize->isVisible())
		endw = d_data->maximize;
	else if (d_data->floating->isVisible())
		endw = d_data->floating;
	else
		endw = d_data->close;

	int start = d_data->bar->mapToParent(QPoint(d_data->bar->width(), 0)).x() + 5;
	int end = endw->pos().x() - 2;
	int h = 8;

	QBrush brush(d_data->patternColor, Qt::Dense6Pattern);
	QRect area;
	if (parent())
		area = QRect(start, height() / 2 - h / 2, end - start, h);

	QPainter painter(this);
	painter.setBrush(brush);
	painter.setPen(Qt::NoPen);
	painter.drawRect(area);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void VipToolWidgetTitleBar::enterEvent(QEvent*)
#else
void VipToolWidgetTitleBar::enterEvent(QEnterEvent*)
#endif
{
	if (VipToolWidget* tool = parent())
		tool->setStyleProperty("hasHover", true);
}
void VipToolWidgetTitleBar::leaveEvent(QEvent*)
{
	if (VipToolWidget* tool = parent())
		tool->setStyleProperty("hasHover", true);
}
void VipToolWidgetTitleBar::resizeEvent(QResizeEvent*)
{
	updateTitle();
}

void VipToolWidgetScrollArea::resizeEvent(QResizeEvent* evt)
{
	QScrollArea::resizeEvent(evt);
}

bool VipToolWidgetScrollArea::floatingTool() const
{
	return static_cast<VipToolWidget*>(parentWidget())->isFloating();
}
void VipToolWidgetScrollArea::setFloatingTool(bool f)
{
	static_cast<VipToolWidget*>(parentWidget())->setFloating(f);
}

void VipToolWidgetToolBar::setEnabled(bool enable)
{
	if (m_toolWidget->action()) {
		// the action cannot be disabled, we still want to be able to show/hide the tool widget

		QList<QWidget*> widgets = this->findChildren<QWidget*>();
		foreach (QWidget* w, widgets)
			w->setEnabled(enable);
		if (QWidget* w = this->widgetForAction(m_toolWidget->action()))
			w->setEnabled(true);
	}
	else
		QToolBar::setEnabled(enable);
}

void VipToolWidgetToolBar::showEvent(QShowEvent* evt)
{

	QWidget::showEvent(evt);
}

class VipToolWidget::PrivateData
{
public:
	bool enableOpacityChange;
	bool resetSizeRequest;
	bool keepFloatingUserSize;
	bool firstShow;
	QPointer <QScrollArea> scroll;
	VipToolWidgetResizer* resizer;
	QPointer<QAction> action;
	QPointer<QAbstractButton> button;
	QSize size;
	QCursor cursor;
	PrivateData()
	  : enableOpacityChange(false)
	  , resetSizeRequest(false)
	  , keepFloatingUserSize(false)
	  , firstShow(true)
	  , scroll(nullptr)
	  , action(nullptr)
	{
	}
};

VipToolWidget::VipToolWidget(VipMainWindow* window)
  : QDockWidget(window)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	this->setWindowFlags(this->windowFlags() | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint );
	this->setAttribute(Qt::WA_TranslucentBackground);
	this->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	this->resize(20, 20);
	this->setFloating(true);
	this->setTitleBarWidget(new VipToolWidgetTitleBar(this));

	d_data->scroll = new VipToolWidgetScrollArea();
	d_data->scroll->setWidgetResizable(true);
	QDockWidget::setWidget(d_data->scroll);

	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(resetSize())); // , Qt::QueuedConnection);
	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(polish()));
	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(floatingChanged(bool)));

	connect(window->displayArea(), SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(displayPlayerAreaChanged()));
	connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(focusChanged(QWidget*, QWidget*)));
	QMetaObject::invokeMethod(this, "displayPlayerAreaChanged", Qt::QueuedConnection);

	setStyleProperty("hasFocus", false);
	setStyleProperty("isFloating", isFloating());
	setStyleProperty("hasHover", false);

	d_data->resizer = new VipToolWidgetResizer(this);

	this->setStyleSheet("VipToolWidget {border-radius: 3px;}");
}

VipToolWidget::~VipToolWidget()
{
	QApplication::instance()->removeEventFilter(this);
}

VipToolWidgetTitleBar* VipToolWidget::titleBarWidget() const
{
	return static_cast<VipToolWidgetTitleBar*>(QDockWidget::titleBarWidget());
}

void VipToolWidget::setWidget(QWidget* widget, Qt::Orientation)
{
	if (!d_data->scroll) {
		d_data->scroll = new VipToolWidgetScrollArea();
		d_data->scroll->setWidgetResizable(true);
		QDockWidget::setWidget(d_data->scroll);
	}
	d_data->scroll->setWidget(widget);
	widget->show();
}

QWidget* VipToolWidget::widget() const
{
	return d_data->scroll ? d_data->scroll->widget() : QDockWidget::widget();
}

QWidget* VipToolWidget::takeWidget()
{
	if (d_data->scroll)
		return d_data->scroll->takeWidget();
	return nullptr;
}

QScrollArea* VipToolWidget::scrollArea() const
{
	return d_data->scroll;
}

VipWidgetResizer* VipToolWidget::resizer() const
{
	return d_data->resizer;
}

void VipToolWidget::setEnableOpacityChange(bool enable)
{
	if (!enable)
		this->setWindowOpacity(1);

	d_data->enableOpacityChange = enable;
}

bool VipToolWidget::enableOpacityChange() const
{
	return d_data->enableOpacityChange;
}

void VipToolWidget::setVisibleInternal(bool vis)
{
	setVisible(vis);
	if (vis)
		raise();
}

void VipToolWidget::setAction(QAction* action, bool take_icon)
{
	if (d_data->action) {
		disconnect(d_data->action, SIGNAL(triggered(bool)), this, SLOT(setVisibleInternal(bool)));
	}

	d_data->action = action;
	if (action) {
		action->setObjectName(this->objectName());
		action->setCheckable(true);
		action->setChecked(this->isVisible());
		connect(d_data->action, SIGNAL(triggered(bool)), this, SLOT(setVisibleInternal(bool)));

		if (take_icon) {
			QIcon ic = action->icon();
			if (!ic.isNull()) {
				this->setWindowIcon(ic);
				this->setDisplayWindowIcon(true);
			}
		}
	}
}

QAction* VipToolWidget::action() const
{
	return d_data->action;
}

void VipToolWidget::setButton(QAbstractButton* button, bool take_icon )
{
	if (d_data->button) {
		disconnect(d_data->button, SIGNAL(clicked(bool)), this, SLOT(setVisible(bool)));
	}

	d_data->button = button;
	if (button) {
		button->setCheckable(true);
		button->setChecked(this->isVisible());
		connect(d_data->button, SIGNAL(clicked(bool)), this, SLOT(setVisible(bool)));

		if (take_icon) {
			QIcon ic = button->icon();
			if (!ic.isNull()) {
				this->setWindowIcon(ic);
				this->setDisplayWindowIcon(true);
			}
		}
	}
}
QAbstractButton* VipToolWidget::button() const
{
	return d_data->button;
}

bool VipToolWidget::displayWindowIcon() const
{
	return titleBarWidget()->property("displayWindowIcon").toBool();
}

void VipToolWidget::setDisplayWindowIcon(bool enable)
{
	titleBarWidget()->setProperty("displayWindowIcon", enable);
}

void VipToolWidget::setKeepFloatingUserSize(bool enable)
{
	d_data->keepFloatingUserSize = enable;
	QMetaObject::invokeMethod(titleBarWidget(), "updateTitleAndPosition");
}

bool VipToolWidget::keepFloatingUserSize() const
{
	return d_data->keepFloatingUserSize;
}

void VipToolWidget::raise()
{
	QWidget::raise();
	// setFocus();
}

void VipToolWidget::setFocus()
{
	QWidget::setFocus();
	setStyleProperty("hasFocus", true);
}

void VipToolWidget::setStyleProperty(const char* name, bool value)
{
	bool this_current = this->property(name).value<bool>();
	bool bar_current = this->titleBarWidget()->property(name).value<bool>();
	bool scroll_current = d_data->scroll ? d_data->scroll->property(name).value<bool>() : false;
	if (this_current != value || bar_current != value || scroll_current != value) {

		this->setProperty(name, value);
		this->titleBarWidget()->setProperty(name, value);
		if (d_data->scroll)
			d_data->scroll->setProperty(name, value);
		polish();
	}
}

QSize VipToolWidget::sizeHint() const
{
	if (d_data->size.isEmpty())
		return d_data->size;
	else
		return QDockWidget::sizeHint();
}
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void VipToolWidget::enterEvent(QEvent*)
#else
void VipToolWidget::enterEvent(QEnterEvent*)
#endif
{
	setStyleProperty("hasHover", true);
	if (d_data->enableOpacityChange)
		this->setWindowOpacity(1);
	raise();
}

void VipToolWidget::leaveEvent(QEvent*)
{
	setStyleProperty("hasHover", false);
	if (d_data->enableOpacityChange)
		this->setWindowOpacity(0.7);
}

void VipToolWidget::closeEvent(QCloseEvent* evt)
{
	this->hide();
	evt->ignore();
}

void VipToolWidget::displayPlayerAreaChanged()
{
	this->setDisplayPlayerArea(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea());
	if (VipToolWidgetToolBar* bar = this->toolBar())
		bar->setDisplayPlayerArea(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea());
}

void VipToolWidget::focusChanged(QWidget* // old
				 ,
				 QWidget* now)
{
	while (now) {
		if (now == this || now == this->titleBarWidget()) {
			setStyleProperty("hasFocus", true);
			return;
		}
		now = now->parentWidget();
	}
	if (windowModality() != Qt::ApplicationModal)
		setStyleProperty("hasFocus", false);
}

// void VipToolWidget::paintEvent(QPaintEvent * evt)
// {
// QDockWidget::paintEvent(evt);
// QStyleOption opt;
// opt.init(this);
// QPainter p(this);
// style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
// }

void VipToolWidget::showEvent(QShowEvent*)
{
	if (d_data->action) {
		d_data->action->blockSignals(true);
		d_data->action->setChecked(true);
		d_data->action->blockSignals(false);
	}
	if (d_data->button) {
		d_data->button->blockSignals(true);
		d_data->button->setChecked(true);
		d_data->button->blockSignals(false);
	}

	// if the dock widget is tabified with other dock widgets, raise it to be sure it is set as the current tab
	if (QMainWindow* p = qobject_cast<QMainWindow*>(parentWidget())) {
		QList<QDockWidget*> tabified = p->tabifiedDockWidgets(this);
		if (tabified.size())
			this->raise();
	}

	resetSize();
	setFocus();

	// change screen if necessary
	if (d_data->firstShow && isFloating()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		int screen = qApp->desktop()->screenNumber(this);
		int main_screen = qApp->desktop()->screenNumber(parentWidget());
#else
		int screen = qApp->screens().indexOf(this->screen());
		int main_screen = qApp->screens().indexOf(parentWidget()->screen());
#endif
		if (screen != main_screen && screen >= 0 && main_screen >= 0) {
			// compute offset
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
			QPoint offset = this->pos() - qApp->desktop()->screenGeometry(screen).topLeft();
			offset += qApp->desktop()->screenGeometry(main_screen).topLeft();
#else
			QPoint offset = this->pos() - qApp->screens()[screen]->availableGeometry().topLeft(); // qApp->desktop()->screenGeometry(screen).topLeft();
			offset += qApp->screens()[main_screen]->availableGeometry().topLeft();		      // qApp->desktop()->screenGeometry(main_screen).topLeft();
#endif
			this->move(offset);
		}
	}
	d_data->firstShow = false;

	if(isFloating())
		raise();
}

void VipToolWidget::hideEvent(QHideEvent*)
{

	if (d_data->action) {
		d_data->action->blockSignals(true);
		d_data->action->setChecked(false);
		d_data->action->blockSignals(false);
	}
	if (d_data->button) {
		d_data->button->blockSignals(true);
		d_data->button->setChecked(false);
		d_data->button->blockSignals(false);
	}
}

void VipToolWidget::floatingChanged(bool)
{
	// if (floating)
	//  setWindowFlags((Qt::WindowFlags)(flags | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Window));
	//  else
	//  setWindowFlags((Qt::WindowFlags)flags);
}

void VipToolWidget::resetSize()
{
	if (!d_data->resetSizeRequest && isVisible()) {
		d_data->resetSizeRequest = true;
		QMetaObject::invokeMethod(this, "internalResetSize", Qt::QueuedConnection);
	}
}

void VipToolWidget::internalResetSize()
{
	d_data->resetSizeRequest = false;

	// if(isFloating())
	//  {
	//  d_data->scroll->setFrameShape(QFrame::StyledPanel);
	//  }
	//  else
	//  {
	//  d_data->scroll->setFrameShape(QFrame::NoFrame);
	//  }

	if (QWidget* w = widget()) {
		// now resize the VipToolWidget, but make sure we stay in the desktop boundaries

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		QDesktopWidget* desktop = qApp->desktop();
		int screen = desktop->screenNumber(this);
		QRect d_rect = desktop->availableGeometry(screen);
		QRect this_rect(this->mapToGlobal(QPoint(0, 0)), w->sizeHint() + QSize(25, 25));
		this_rect = this_rect & d_rect;
#else
		// Qt6 way
		QRect d_rect = this->screen() ? this->screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
		QRect this_rect(this->mapToGlobal(QPoint(0, 0)), w->sizeHint() + QSize(25, 25));
		this_rect = this_rect & d_rect;
#endif
		if (isFloating()) {
			if (!keepFloatingUserSize()) {
				w->resize(w->sizeHint());
				QSize s = this_rect.size();
				resize(s + QSize(0, titleBarWidget()->sizeHint().height()));
				d_data->size = s;
			}
			raise();
		}
		else {
			w->resize(scrollArea()->size());
			// d_data->size = QSize();
			//  Qt::DockWidgetArea area = vipGetMainWindow()->dockWidgetArea(this);
			//  if( (area & Qt::LeftDockWidgetArea) || (area & Qt::RightDockWidgetArea) )
			//  resize(width(), qMax(height(),this_rect.height()));
			//  else
			//  resize(qMax(width(),this_rect.width()),height());
		}
	}
}

void VipToolWidget::resizeEvent(QResizeEvent* evt)
{
	QDockWidget::resizeEvent(evt);

	// int left = this->titleBarWidget()->mapToParent(QPoint(0, 0)).x();
	//  int right = this->titleBarWidget()->mapToParent(QPoint(titleBarWidget()->width(), 0)).x();
	//  int top = this->titleBarWidget()->mapToParent(QPoint(0, 0)).y();
	//  int bottom = this->titleBarWidget()->mapToParent(QPoint(height(), 0)).y();
	//
	// setMask(QRegion(QRect(left+5, top+5, right - left - 5, bottom - top - 5)));
}

void VipToolWidget::moveEvent(QMoveEvent* evt)
{
	// if (isMaximized())
	//  {
	//  showNormal();
	//  QMetaObject::invokeMethod(titleBarWidget(), "updateTitleAndPosition");
	//  }
	QDockWidget::moveEvent(evt);
}

bool VipToolWidget::floatingTool() const
{
	return isFloating();
}
void VipToolWidget::setFloatingTool(bool f)
{
	setFloating(f);
}

void VipToolWidget::polish()
{
	setProperty("isFloating", isFloating());
	this->titleBarWidget()->setProperty("isFloating", isFloating());

	bool is_floating = property("isFloating").toBool();
	bool is_hover = property("hasHover").toBool();
	bool is_focus = property("hasFocus").toBool();

	int status = 0; // no hover or floating or focus
	if (is_focus)
		status = 1; // focus
	else {
		if (is_hover)
			status = 2; // hover
		else if (is_floating)
			status = 3; // floating
	}
	this->setProperty("status", status);
	if (d_data->scroll)
	d_data->scroll->setProperty("status", status);
	titleBarWidget()->setProperty("status", status);
	// vip_debug("set %s: %i\n", this->windowTitle().toLatin1().data(), status);

	this->style()->unpolish(this);
	this->style()->polish(this);
	if (d_data->scroll) {

		d_data->scroll->style()->unpolish(d_data->scroll);
		d_data->scroll->style()->polish(d_data->scroll);
	}
	titleBarWidget()->style()->unpolish(titleBarWidget());
	titleBarWidget()->style()->polish(titleBarWidget());
	this->update();
}

VipToolWidgetPlayer::VipToolWidgetPlayer(VipMainWindow* window)
  : VipToolWidget(window)
  , m_automaticTitleManagement(true)
{
	setWindowTitle("Edit plot items");
	setObjectName("Edit plot items");

	connect(window->displayArea(), SIGNAL(focusWidgetChanged(VipDragWidget*)), this, SLOT(focusWidgetChanged(VipDragWidget*)));
}

VipAbstractPlayer* VipToolWidgetPlayer::currentPlayer() const
{
	return const_cast<VipAbstractPlayer*>(m_player.data());
}

void VipToolWidgetPlayer::setAutomaticTitleManagement(bool enable)
{
	m_automaticTitleManagement = enable;
}
bool VipToolWidgetPlayer::automaticTitleManagement() const
{
	return m_automaticTitleManagement;
}

void VipToolWidgetPlayer::setDisplayPlayerArea(VipDisplayPlayerArea* area)
{
	if (area && area != m_area) {
		m_area = area;
		focusWidgetChanged(area->dragWidgetHandler()->focusWidget());
	}
}

void VipToolWidgetPlayer::showEvent(QShowEvent* evt)
{
	if (widget())
		widget()->setEnabled(m_player);

	bool ok = this->setPlayer(m_player);
	if (VipToolWidgetToolBar* bar = toolBar()) {
		if (ok) {
			bar->setEnabled(m_player);
			bar->setPlayer(m_player);
		}
		else {
			bar->setEnabled(false);
			bar->setPlayer(nullptr);
		}
	}
	if (widget())
		widget()->setEnabled(ok);

	if (m_player && m_automaticTitleManagement) {
		// set the window title
		QString title = this->windowTitle();
		if (!title.isEmpty()) {
			QStringList lst = title.split(" - ");
			title = lst.first() + " - " + m_player->windowTitle();
		}
		else
			title = (m_player->windowTitle());
		this->setWindowTitle(title);
	}

	return VipToolWidget::showEvent(evt);
}

void VipToolWidgetPlayer::focusWidgetChanged(VipDragWidget* w)
{
	if (!w)
		m_player = nullptr;
	else {
		QList<VipAbstractPlayer*> players = w->findChildren<VipAbstractPlayer*>();
		if (!players.size())
			m_player = nullptr;
		else
			m_player = players.back();
	}

	// if (isHidden()) {
	//  //explicitly hidden
	//  setPlayer(nullptr);
	//  if (widget())
	//  widget()->setEnabled(false);
	//  if (VipToolWidgetToolBar* bar = toolBar()) {
	//  bar->setEnabled(false);
	//  bar->setPlayer(nullptr);
	//  }
	//  }
	//  else
	{
		if (widget())
			widget()->setEnabled(m_player);

		bool ok = m_player;
		if ((ok = this->setPlayer(m_player)) && m_player && m_automaticTitleManagement) {
			QString title = this->windowTitle();
			if (!title.isEmpty()) {
				QStringList lst = title.split(" - ");
				title = lst.first() + " - " + m_player->windowTitle();
			}
			else
				title = (m_player->windowTitle());

			this->setWindowTitle(title);
		}
		if (!ok) {
			if (widget())
				widget()->setEnabled(false);
		}

		if (VipToolWidgetToolBar* bar = toolBar()) {
			if (ok) {
				bar->setEnabled(m_player);
				bar->setPlayer(m_player);
			}
			else {
				bar->setEnabled(false);
				bar->setPlayer(nullptr);
			}
		}
	}

	resetSize();
}

#include "VipStandardWidgets.h"
#include <QBoxLayout>

class VipPlotToolWidgetPlayer::PrivateData
{
public:
	QPointer<QGraphicsScene> scene;
	QPointer<VipAbstractPlayer> player;
};

VipPlotToolWidgetPlayer::VipPlotToolWidgetPlayer(VipMainWindow* window)
  : VipToolWidgetPlayer(window)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	this->setEnableOpacityChange(true);
	this->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	this->setAllowedAreas(Qt::NoDockWidgetArea);
	this->setFloating(true);
}

VipPlotToolWidgetPlayer::~VipPlotToolWidgetPlayer()
{
	if (d_data->scene)
		d_data->scene->removeEventFilter(this);
}

bool VipPlotToolWidgetPlayer::setPlayer(VipAbstractPlayer* pl)
{
	if (pl == d_data->player)
		return pl;
	d_data->player = pl;

	if (d_data->scene)
		d_data->scene->removeEventFilter(this);
	d_data->scene = nullptr;

	if (!pl)
		return false;

	d_data->scene = pl->plotWidget2D() ? pl->plotWidget2D()->scene() : nullptr;
	if (d_data->scene)
		d_data->scene->installEventFilter(this);

	if (QWidget* w = vipObjectEditor(QVariant::fromValue(pl))) {
		// we need to reset the internal widget, otherwise the new size won't be correct
		//(this size is updated when the widget is first shown)
		if (widget())
			delete widget();

		QWidget* p = new QWidget();
		QHBoxLayout* l = new QHBoxLayout();
		l->addWidget(w);
		p->setLayout(l);
		setWidget(p);

		connect(w, SIGNAL(itemChanged(QGraphicsObject*)), this, SLOT(resetSize()));
		connect(w, SIGNAL(abstractPlayerChanged(VipAbstractPlayer*)), this, SLOT(resetSize()));
	}
	return true;
}

void VipPlotToolWidgetPlayer::setItem(QGraphicsObject* item)
{
	if (item && item->scene()) {
		// unselect all items
		QList<QGraphicsItem*> items = item->scene()->items();
		for (int i = 0; i < items.size(); ++i)
			if (items[i]->isSelected())
				items[i]->setSelected(false);

		// select the argument item
		item->setSelected(true);

		// set the player
		if (VipAbstractPlayer* player = VipAbstractPlayer::findAbstractPlayer(item)) {
			if (currentPlayer() != player)
				setPlayer(player);
			if (widget() && !widget()->isEnabled())
				widget()->setEnabled(true);
		}
		else {
			if (QWidget* w = vipObjectEditor(QVariant::fromValue(item))) {
				// we need to reset the internal widget, otherwise the new size won't be correct
				//(this size is updated when the widget is first shown)
				if (widget())
					delete widget();

				QWidget* p = new QWidget();
				QHBoxLayout* l = new QHBoxLayout();
				l->addWidget(w);
				p->setLayout(l);
				setWidget(p);
			}
		}
	}
}

bool VipPlotToolWidgetPlayer::eventFilter(QObject*, QEvent* evt)
{
	// filter scene events to show this tool widget on double click
	if (evt->type() == QEvent::GraphicsSceneMouseDoubleClick) {

		// set the default player's items if needed
		if (VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(d_data->player)) {
			QList<QGraphicsItem*> items = pl->plotWidget2D()->scene()->selectedItems();
			if (items.isEmpty()) {
				if (QGraphicsObject* obj = pl->defaultEditableObject()) {
					obj->setSelected(true);
				}
			}
			else
				setItem(items.last()->toGraphicsObject());
		}

		this->setVisible(true);
		this->raise();
		this->resetSize();
	}
	return false;
}

VipPlotToolWidgetPlayer* vipGetPlotToolWidgetPlayer(VipMainWindow* window)
{
	static VipPlotToolWidgetPlayer* win = new VipPlotToolWidgetPlayer(window);
	return win;
}

struct ProgressWidget : public QFrame
{
	QLabel text;
	QProgressBar progressBar;
	QToolButton cancel;
	QPointer<VipProgress> progress;
	VipMultiProgressWidget* widget;

	ProgressWidget(VipProgress* p, VipMultiProgressWidget* widget, QWidget* parent = nullptr)
	  : QFrame(parent)
	  , progress(p)
	  , widget(widget)
	{
		QGridLayout* lay = new QGridLayout();
		lay->addWidget(&text, 0, 0);
		lay->addWidget(&progressBar, 1, 0);
		lay->addWidget(&cancel, 0, 1);

		setLayout(lay);
		lay->setContentsMargins(2, 2, 2, 2);

		text.setWordWrap(true);

		cancel.setAutoRaise(true);
		cancel.setToolTip("Stop this operation");
		cancel.setIcon(vipIcon("cancel.png"));
		cancel.hide();

		progressBar.setRange(0, 100);
		progressBar.setMaximumHeight(20);
		progressBar.setMinimumHeight(20);
		setProgressBarVisible(false);
		progressBar.hide();

		if (p)
			connect(&cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
		if (widget)
			connect(&cancel, SIGNAL(clicked(bool)), widget, SLOT(cancelRequested()));

		this->setMinimumWidth(300);

		// setStyleSheet(" QFrame { border: 1px solid #CCCCCC;}");
	}

	void setProgress(VipProgress* p)
	{
		if (p != progress) {
			if (progress)
				disconnect(&cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
			progress = p;
			if (progress)
				connect(&cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
		}
	}

	void setProgressBarVisible(bool visible)
	{
		if (visible != progressBar.isVisible()) {
			progressBar.setVisible(visible);
			QGridLayout* lay = static_cast<QGridLayout*>(layout());
			if (visible) {
				lay->removeWidget(&cancel);
				lay->addWidget(&cancel, 1, 1);
			}
			else {
				lay->removeWidget(&cancel);
				lay->addWidget(&cancel, 0, 1);
			}
		}
	}

	bool progressBarVisible() const { return progressBar.isVisible(); }
};

#include <qtimer.h>

class VipMultiProgressWidget::PrivateData
{
public:
	QList<ProgressWidget*> progresses;
	QList<ProgressWidget*> reuse;
	QLabel status;
	QSet<ProgressWidget*> modalWidgets;
	QTimer modalTimer;
	QBoxLayout* layout;
	bool changeModality;
	bool isModal{false}; 

	ProgressWidget* find(VipProgress* p) const
	{
		for (int i = 0; i < progresses.size(); ++i)
			if (progresses[i]->progress == p)
				return progresses[i];
		return nullptr;
	}
};

VipMultiProgressWidget::VipMultiProgressWidget(VipMainWindow* window)
  : VipToolWidget(window)
{
	setWindowTitle("Operations");
	setObjectName("Operations");

	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->status.setText("No operation to display at this time");

	d_data->modalTimer.setSingleShot(true);
	d_data->modalTimer.setInterval(100);
	d_data->changeModality = false;

	connect(&d_data->modalTimer, SIGNAL(timeout()), this, SLOT(updateModality()), Qt::QueuedConnection);

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->status);
	lay->setSpacing(2);

	QWidget* w = new QWidget();
	w->setLayout(lay);
	this->setWidget(w);
	d_data->layout = lay;
	// this->setMinimumWidth(300);
	this->resize(300, 100);

	d_data->reuse.append(new ProgressWidget(nullptr, this));
	d_data->layout->addWidget(d_data->reuse.last());
	d_data->reuse.last()->hide();

	VipProgress::setProgressManager(this);

	// center the widget on the main screen
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	QRect rect = QApplication::desktop()->screenGeometry();
#else
	QRect rect = (window && window->screen()) ? window->screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
#endif
	this->move(rect.x() + rect.width() / 2 - this->width() / 2, rect.y() + rect.height() / 2 - this->height() / 2);
}

VipMultiProgressWidget::~VipMultiProgressWidget()
{
	VipProgress::resetProgressManager();
	changeModality(Qt::NonModal);
	this->disconnect();
	vipProcessEvents();
}

QMultiMap<QString, int> VipMultiProgressWidget::currentProgresses() const
{
	QMultiMap<QString, int> res;
	for (int i = 0; i < d_data->progresses.size(); ++i) {
		if (d_data->progresses[i]->isVisible()) {
			int value = -1;
			if (d_data->progresses[i]->progressBar.isVisible())
				value = d_data->progresses[i]->progressBar.value();
			QString text = d_data->progresses[i]->text.text();
			res.insert(text, value);
		}
	}
	return res;
}

void VipMultiProgressWidget::closeEvent(QCloseEvent* evt)
{
	evt->ignore();
	if (this->windowModality() != Qt::ApplicationModal)
		this->hide();
}

void VipMultiProgressWidget::showEvent(QShowEvent* evt)
{
	if (isFloating()) {
		// center the widget on the main screen
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		QRect rect = QApplication::desktop()->screenGeometry();
#else
		QRect rect = this->screen() ? this->screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
#endif
		this->move(rect.x() + rect.width() / 2 - this->width() / 2, rect.y() + rect.height() / 2 - this->height() / 2);
	}

	if (d_data->changeModality) {
		//QDockWidget::showEvent(evt);
		setFocus();
	}
	else
		VipToolWidget::showEvent(evt);
}

static bool isModalWidget(QWidget * w)
{
	while(w){
		if(w->windowModality() == Qt::ApplicationModal)
			return true;
		w = w->parentWidget();
	} 
	return false;
}


bool VipMultiProgressWidget::eventFilter(QObject * obj, QEvent * evt)
{
	// Simulate a (kinf of) modal widget by filtering events at application level.
	// On used on non Windows plateform that do not necessarly support changing 
	// widget modality several times.
	switch(evt->type()){
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseMove:
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		case QEvent::HoverEnter:
		case QEvent::HoverLeave:
		case QEvent::HoverMove:
		case QEvent::TouchBegin:
		case QEvent::TouchCancel:
		case QEvent::TouchEnd:
		case QEvent::TouchUpdate:
		case QEvent::Wheel:
		case QEvent::FocusIn:
		case QEvent::Enter:
		{
			if(QWidget * w = qobject_cast<QWidget*>(obj)){
				if(this->isAncestorOf(w) || isModalWidget(w) )
					return false;
				else
					return true;
			}
			break;
		}	
		case QEvent::Shortcut:
		case QEvent::ShortcutOverride:
			return true;
		default:
		break;
	}
	return false;
}


void VipMultiProgressWidget::changeModality(Qt::WindowModality modality)
{
#ifdef WIN32
	if(this->windowModality() != modality){ 
		this->hide();
		d_data->changeModality = true;
		this->setWindowModality(modality);
		this->show();
		d_data->changeModality = false;
	}

#else

	bool requestModal = modality == Qt::ApplicationModal;
	if(requestModal != d_data->isModal)
	{
		if(requestModal){
			qApp->installEventFilter(this);
			vipGetMainWindow()->installEventFilter(this);
		} 
		else{
			qApp->removeEventFilter(this);
			vipGetMainWindow()->removeEventFilter(this);
		} 
		d_data->isModal = requestModal;
	}	
#endif	
}

void VipMultiProgressWidget::updateScrollBars()
{
	this->scrollArea()->setVerticalScrollBarPolicy(isFloating() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
	this->scrollArea()->setHorizontalScrollBarPolicy(isFloating() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

void VipMultiProgressWidget::updateModality()
{
	if (!d_data->modalWidgets.size()) {
		// remove modal attribute
		changeModality(Qt::NonModal);
	}
	else if (this->windowModality() != Qt::ApplicationModal) {
		// try to set modal attribute
		if (QApplication::activeModalWidget() && QApplication::activeModalWidget() != this) {
			// there is already a modal widget: do not set modal attribute, try later
			d_data->modalTimer.start();
		}
		else {
			changeModality(Qt::ApplicationModal);
		}
	}
}

void VipMultiProgressWidget::cancelRequested()
{
	// make sure to cancel all sub operations
	bool start_cancel = false;
	for (int i = 0; i < d_data->progresses.size(); ++i) {
		if (start_cancel) {
			if (d_data->progresses[i]->progress)
				d_data->progresses[i]->progress->cancelRequested();
		}
		else if (sender() == &d_data->progresses[i]->cancel) {
			start_cancel = true;
		}
	}
}

void VipMultiProgressWidget::addProgress(QObjectPointer ptr)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		ProgressWidget* w = d_data->reuse.size() ? d_data->reuse.first() : new ProgressWidget(p, this);
		w->setProgress(p);

		if (d_data->reuse.size())
			d_data->reuse.removeAt(0);
		else
			d_data->layout->addWidget(w);

		d_data->status.hide();
		d_data->progresses.append(w);
		d_data->progresses.back()->progressBar.setRange(p->min(), p->max());
		d_data->progresses.back()->text.setText(p->text());

		d_data->progresses.back()->show();
		this->show();
		this->resetSize();
		this->raise();
	}
}

void VipMultiProgressWidget::removeProgress(QObjectPointer ptr)
{
	VipProgress* p = qobject_cast<VipProgress*>(ptr.data());

	// remove all invalid progress bar
	for (int i = 0; i < d_data->progresses.size(); ++i) {
		ProgressWidget* w = d_data->progresses[i];
		if (w->progress == p || !w->progress) {
			d_data->modalWidgets.remove(w);
			d_data->progresses.removeAt(i);
			w->progressBar.hide();
			w->progressBar.setValue(0);
			w->text.setText(QString());

			if (d_data->reuse.indexOf(w) < 0)
				d_data->reuse.append(w);
			w->hide();

			i--;
		}
	}

	// update status text visibility
	d_data->status.setVisible(d_data->progresses.size() == 0);

	// update this window modality
	updateModality();

	if (d_data->progresses.size() == 0) {
		this->hide();
	}
}

void VipMultiProgressWidget::setText(QObjectPointer ptr, const QString& text)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (ProgressWidget* w = d_data->find(p)) {
			bool reset_size = p->isModal();
			if (text.size() && w->text.isHidden()) {
				reset_size = true;
				w->text.show();
			}

			w->text.setText(text);
			if (reset_size)
				this->resetSize();

			if (this->windowModality() == Qt::ApplicationModal)
				setStyleProperty("hasFocus", true);
		}
	}
}

void VipMultiProgressWidget::setValue(QObjectPointer ptr, int value)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (ProgressWidget* w = d_data->find(p)) {
			if (w->progressBar.isHidden()) {
				w->setProgressBarVisible(true);
				this->resetSize();
			}
			w->progressBar.setValue(value);

			if (this->windowModality() == Qt::ApplicationModal) {
			
				setFocus();
				if (isFloating()) {
					this->showAndRaise();
				}
			}
		}
	}
}

void VipMultiProgressWidget::setCancelable(QObjectPointer ptr, bool cancelable)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (ProgressWidget* w = d_data->find(p))
			w->cancel.setVisible(cancelable);
	}
}

void VipMultiProgressWidget::setModal(QObjectPointer ptr, bool modal)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (ProgressWidget* w = d_data->find(p)) {
			// set modal
			if (modal && !d_data->modalWidgets.contains(w)) {
				d_data->modalWidgets.insert(w);
				updateModality();

				// center the widget inside its parent (if any)
				if (parentWidget() && parentWidget()->isVisible()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
					QRect rect;
					int screen = qApp->desktop()->screenNumber(this->parentWidget());
					if (this->parentWidget())
						rect = this->parentWidget()->geometry();
					else
						rect = QApplication::desktop()->screenGeometry(screen);
#else
					// Qt6 way
					QRect rect;
					if (this->parentWidget())
						rect = this->parentWidget()->geometry();
					else {
						if (this->screen())
							rect = this->screen()->availableGeometry();
						else
							rect = QGuiApplication::primaryScreen()->availableGeometry();
					}
#endif
					this->move(rect.x() + rect.width() / 2 - this->width() / 2, rect.y() + rect.height() / 2 - this->height() / 2);
					if (this->windowModality() == Qt::ApplicationModal && isFloating()) {
						
						this->showAndRaise();
					}
				}
				else {

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
					QRect rect;
					rect = QApplication::desktop()->screenGeometry(0);
#else
					// center the widget on the main screen
					QRect rect;
					if (this->screen())
						rect = this->screen()->availableGeometry();
					else
						rect = QGuiApplication::primaryScreen()->availableGeometry();
#endif
					this->move(rect.x() + rect.width() / 2 - this->width() / 2, rect.y() + rect.height() / 2 - this->height() / 2);
				}
			}
			else if (!modal && d_data->modalWidgets.contains(w)) {
				d_data->modalWidgets.remove(w);
				updateModality();
			}
		}

	}
}

VipMultiProgressWidget* vipGetMultiProgressWidget(VipMainWindow* window)
{
	static VipMultiProgressWidget* widget = new VipMultiProgressWidget(window);
	return widget;
}
