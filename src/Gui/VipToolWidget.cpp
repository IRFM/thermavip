#include "VipToolWidget.h"
#include "VipPlayer.h"
#include "VipDisplayArea.h"
#include "VipProgress.h"

#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QGridLayout>
#include <QBoxLayout>
#include <QPointer>
#include <QApplication>
#include <QScrollArea>
#include <qapplication.h>
#include <qlabel.h>
#include <qscreen.h>
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#include <QDesktopWidget>
#endif

#include "VipDragWidget.h"
#include "VipWidgetResizer.h"



class VipToolWidgetResizer : public VipWidgetResizer
{
public:
	VipToolWidgetResizer(QWidget * parent) : VipWidgetResizer(parent)
	{}
protected:
	virtual bool isTopLevelWidget(const QPoint & screen_pos = QPoint()) const
	{
		if (!parent()->isTopLevel())
			return false;
		if (screen_pos == QPoint())
			return true;

		QWidget * under_mouse = QApplication::widgetAt(screen_pos);
		while (under_mouse) {
			if (under_mouse->isTopLevel() && under_mouse == parent())
				return true;
			else if (under_mouse->isTopLevel() && under_mouse != vipGetMainWindow())
				return false;
			under_mouse = under_mouse->parentWidget();
		}
		return true;
	}
};




class NoSizeLable : public QLabel
{
public:
	NoSizeLable(QWidget * parent = NULL)
		:QLabel(parent)
	{
		setMinimumWidth(10);
	}

	virtual QSize sizeHint() const {
		return QSize();
	}
};

class VipToolWidgetTitleBar::PrivateData
{
public:
	QLabel *icon;
	QLabel *label;
	QToolBar * bar;
	QToolButton *floating;
	QToolButton *close;
	QToolButton * restore;
	QToolButton * maximize;
	QColor patternColor;
	bool displayWindowIcon;
};

VipToolWidgetTitleBar::VipToolWidgetTitleBar(VipToolWidget * parent)
	:QWidget(parent)
{
	m_data = new PrivateData();
	m_data->icon = new QLabel(this);
	m_data->label = new NoSizeLable(this);
	m_data->bar = new QToolBar();
	m_data->floating = new QToolButton();
	m_data->close = new QToolButton();
	m_data->maximize = new QToolButton();
	m_data->restore = new QToolButton();
	m_data->patternColor = Qt::gray;
	m_data->displayWindowIcon = false;

	m_data->bar->setIconSize(QSize(18, 18));
	m_data->bar->setAutoFillBackground(false);

	m_data->label->setIndent(5);
	m_data->label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	m_data->label->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_data->label->setFocusPolicy(Qt::NoFocus);

	m_data->floating->setAutoRaise(true);
	m_data->floating->setCheckable(true);
	m_data->floating->setIcon(vipIcon("pin.png"));
	m_data->floating->setToolTip("Make panel floating");
	m_data->floating->setMaximumSize(18, 18);

	m_data->close->setAutoRaise(true);
	m_data->close->setIcon(vipIcon("close.png"));
	m_data->close->setToolTip("Close");
	m_data->close->setMaximumSize(18, 18);

	m_data->restore->setAutoRaise(true);
	m_data->restore->setIcon(vipIcon("restore.png"));
	m_data->restore->setToolTip("Restore");
	m_data->restore->setMaximumSize(18, 18);
	m_data->restore->hide();

	m_data->maximize->setAutoRaise(true);
	m_data->maximize->setIcon(vipIcon("maximize.png"));
	m_data->maximize->setToolTip("Maximize");
	m_data->maximize->setMaximumSize(18, 18);
	m_data->maximize->hide();

	QHBoxLayout * lay = new QHBoxLayout();
	lay->addItem(new QSpacerItem(3, 3));
	lay->addWidget(m_data->icon);
	lay->addItem(new QSpacerItem(3, 3));
	lay->addWidget(m_data->label);
	lay->addWidget(m_data->bar);
	lay->addStretch(1);
	lay->addWidget(m_data->restore);
	lay->addWidget(m_data->maximize);
	lay->addWidget(m_data->floating);
	lay->addWidget(m_data->close);
	lay->setSpacing(0);
	lay->setContentsMargins(0, 3, 2, 3);
	setLayout(lay);

	m_data->label->setMaximumWidth(350);
	m_data->label->setText(parent->windowTitle());
	QSize s = parent->windowIcon().actualSize(QSize(100, 100));
	if (!s.isEmpty())
		m_data->icon->setPixmap(parent->windowIcon().pixmap(s));
	m_data->icon->setVisible(false);// !parent->windowIcon().isNull());

									//this->setMaximumHeight(30);
	this->setFocusPolicy(Qt::ClickFocus);

	connect(parent, SIGNAL(allowedAreasChanged(Qt::DockWidgetAreas)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(featuresChanged(QDockWidget::DockWidgetFeatures)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(topLevelChanged(bool)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(visibilityChanged(bool)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(updateTitleAndPosition()));
	connect(parent, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(updateTitleAndPosition()));

	connect(m_data->restore, SIGNAL(clicked(bool)), parent, SLOT(showNormal()));
	connect(m_data->maximize, SIGNAL(clicked(bool)), parent, SLOT(floatWidget()));
	connect(m_data->maximize, SIGNAL(clicked(bool)), parent, SLOT(showMaximized()));
	connect(m_data->restore, SIGNAL(clicked(bool)), this, SLOT(updateTitleAndPosition()));
	connect(m_data->maximize, SIGNAL(clicked(bool)), this, SLOT(updateTitleAndPosition()));
	connect(m_data->floating, SIGNAL(clicked(bool)), parent, SLOT(setFloatingTool(bool)));
	connect(m_data->close, SIGNAL(clicked(bool)), parent, SLOT(close()));

	//this->setMinimumHeight(30);
}

VipToolWidgetTitleBar::~VipToolWidgetTitleBar()
{
	delete m_data;
}

VipToolWidget * VipToolWidgetTitleBar::parent() const
{
	return qobject_cast<VipToolWidget*>(parentWidget());
}

QToolBar * VipToolWidgetTitleBar::toolBar() const
{
	return m_data->bar;
}

QIcon VipToolWidgetTitleBar::closeButton() const
{
	return m_data->close->icon();
}

QIcon VipToolWidgetTitleBar::floatButton() const
{
	return m_data->floating->icon();
}

QColor VipToolWidgetTitleBar::textColor() const
{
	return m_data->label->palette().color(QPalette::WindowText);
}

QColor VipToolWidgetTitleBar::patternColor() const
{
	return m_data->patternColor;
}

bool VipToolWidgetTitleBar::displayWindowIcon() const
{
	return m_data->displayWindowIcon;
}

void VipToolWidgetTitleBar::setPatternColor(const QColor & c)
{
	m_data->patternColor = c;
	update();
}

void VipToolWidgetTitleBar::setDisplayWindowIcon(bool enable)
{
	m_data->displayWindowIcon = enable;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	const QPixmap pix = m_data->icon->pixmap() ? *m_data->icon->pixmap() : QPixmap();
#else
	const QPixmap pix = m_data->icon->pixmap(Qt::ReturnByValue);
#endif
	m_data->icon->setVisible(enable && !pix.isNull());
}

void VipToolWidgetTitleBar::VipToolWidgetTitleBar::setCloseButton(const QIcon & icon)
{
	m_data->close->setIcon(icon);
}

void VipToolWidgetTitleBar::setFloatButton(const QIcon & icon)
{
	m_data->floating->setIcon(icon);
}

void VipToolWidgetTitleBar::setTextColor(const QColor & c)
{
	QPalette p = m_data->label->palette();
	p.setColor(QPalette::WindowText, c);
	m_data->label->setPalette(p);
}

QIcon VipToolWidgetTitleBar::maximizeButton() const
{
	return m_data->maximize->icon();
}
QIcon VipToolWidgetTitleBar::restoreButton() const
{
	return m_data->restore->icon();
}

void VipToolWidgetTitleBar::setMaximizeButton(const QIcon & i)
{
	m_data->maximize->setIcon(i);
}
void VipToolWidgetTitleBar::setRestoreButton(const QIcon & i)
{
	m_data->restore->setIcon(i);
}

void VipToolWidgetTitleBar::updateTitle()
{
	if (VipToolWidget * tool = parent())
	{
		QFontMetrics m(m_data->label->font());
		int width = this->width();
		//if (width < 200)
		//	width = 200;
		QString text = m.elidedText(tool->windowTitle(), Qt::ElideRight, //m_data->label->maximumWidth()
width - 45);

		m_data->label->setText(text);
		m_data->label->setToolTip(tool->windowTitle());
		this->setToolTip(tool->windowTitle());
	}

}

void VipToolWidgetTitleBar::updateTitleAndPosition()
{
	if (VipToolWidget * tool = parent())
	{
		updateTitle();
		if (m_data->displayWindowIcon)
		{
			QSize s = tool->windowIcon().actualSize(QSize(100, 100));
			if (!s.isEmpty())
				m_data->icon->setPixmap(tool->windowIcon().pixmap(s));
			m_data->icon->setVisible(!tool->windowIcon().isNull());
		}

		m_data->floating->blockSignals(true);
		m_data->floating->setChecked(tool->isFloating());
		m_data->floating->blockSignals(false);

		m_data->close->setVisible(tool->features() & QDockWidget::DockWidgetClosable);
		m_data->floating->setVisible(tool->features() & QDockWidget::DockWidgetFloatable);

		if (!tool->keepFloatingUserSize())
		{
			m_data->maximize->hide();
			m_data->restore->hide();
		}
		else
		{
			m_data->restore->setVisible(tool->isMaximized());
			m_data->maximize->setVisible(!tool->isMaximized());
		}
	}
}

void VipToolWidgetTitleBar::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

	QWidget * endw = NULL;
	if (m_data->restore->isVisible())
		endw = m_data->restore;
	else if (m_data->maximize->isVisible())
		endw = m_data->maximize;
	else if(m_data->floating->isVisible())
		endw = m_data->floating;
	else
		endw = m_data->close;

	int start = m_data->bar->mapToParent(QPoint(m_data->bar->width(), 0)).x() + 5;
	int end = endw->pos().x() - 2;
	int h = 8;

	QBrush brush(m_data->patternColor, Qt::Dense6Pattern);
	QRect area;
	if (parent())
		area = QRect(start, height() / 2 - h / 2, end - start, h);

	QPainter painter(this);
	painter.setBrush(brush);
	painter.setPen(Qt::NoPen);
	painter.drawRect(area);
}

void VipToolWidgetTitleBar::enterEvent(QEvent *)
{
	if (VipToolWidget * tool = parent())
		tool->setStyleProperty("hasHover", true);
}
void VipToolWidgetTitleBar::leaveEvent(QEvent *)
{
	if (VipToolWidget * tool = parent())
		tool->setStyleProperty("hasHover", true);
}
void VipToolWidgetTitleBar::resizeEvent(QResizeEvent *)
{
	updateTitle();
}


void VipToolWidgetScrollArea::resizeEvent(QResizeEvent * evt)
{
	QScrollArea::resizeEvent(evt);
}



void VipToolWidgetToolBar::setEnabled(bool enable)
{
	if (m_toolWidget->action())
	{
		//the action cannot be disabled, we still want to be able to show/hide the tool widget

		QList<QWidget*> widgets = this->findChildren<QWidget*>();
		foreach(QWidget * w, widgets) w->setEnabled(enable);
		if (QWidget * w = this->widgetForAction(m_toolWidget->action()))
			w->setEnabled(true);
	}
	else
		QToolBar::setEnabled(enable);
}

void VipToolWidgetToolBar::showEvent(QShowEvent *evt)
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
	QScrollArea * scroll;
	VipToolWidgetResizer * resizer;
	QPointer<QAction> action;
	QPointer<QAbstractButton> button;
	QSize size;
	QCursor cursor;
	PrivateData() : enableOpacityChange(false), resetSizeRequest(false), keepFloatingUserSize(false), firstShow(true), scroll(NULL), action(NULL) {}
};

VipToolWidget::VipToolWidget(VipMainWindow * window)
	:QDockWidget(window)
{
	m_data = new PrivateData();

	this->setWindowFlags(this->windowFlags() | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint);
	this->setAttribute(Qt::WA_TranslucentBackground);
	this->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	this->resize(20, 20);
	this->setFloating(true);
	this->setTitleBarWidget(new VipToolWidgetTitleBar(this));

	m_data->scroll = new VipToolWidgetScrollArea();
	m_data->scroll->setWidgetResizable(true);
	QDockWidget::setWidget(m_data->scroll);

	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(resetSize()));// , Qt::QueuedConnection);
	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(polish()));
	connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(floatingChanged(bool)));

	connect(window->displayArea(), SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(displayPlayerAreaChanged()));
	connect(QApplication::instance(), SIGNAL(focusChanged(QWidget *, QWidget *)), this, SLOT(focusChanged(QWidget *, QWidget *)));
	QMetaObject::invokeMethod(this, "displayPlayerAreaChanged", Qt::QueuedConnection);

	setStyleProperty("hasFocus", false);
	setStyleProperty("isFloating", isFloating());
	setStyleProperty("hasHover", false);

	m_data->resizer = new VipToolWidgetResizer(this);

	this->setStyleSheet("VipToolWidget {border-radius: 3px;}");
}

VipToolWidget::~VipToolWidget()
{
	QApplication::instance()->removeEventFilter(this);

	delete m_data;
}

VipToolWidgetTitleBar * VipToolWidget::titleBarWidget() const
{
	return static_cast<VipToolWidgetTitleBar*>(QDockWidget::titleBarWidget());
}

void VipToolWidget::setWidget(QWidget * widget, Qt::Orientation)
{
	m_data->scroll->setWidget(widget);
	widget->show();
}

QWidget * VipToolWidget::widget() const
{
	return m_data->scroll->widget();
}

QScrollArea * VipToolWidget::scrollArea() const
{
	return m_data->scroll;
}

VipWidgetResizer * VipToolWidget::resizer() const
{
	return m_data->resizer;
}

void VipToolWidget::setEnableOpacityChange(bool enable)
{
	if (!enable)
		this->setWindowOpacity(1);

	m_data->enableOpacityChange = enable;
}

bool VipToolWidget::enableOpacityChange() const
{
	return m_data->enableOpacityChange;
}

void VipToolWidget::setVisibleInternal(bool vis)
{
	setVisible(vis);
	if (vis)
		raise();
}

void VipToolWidget::setAction(QAction * action)
{
	if (m_data->action)
	{
		disconnect(m_data->action, SIGNAL(triggered(bool)), this, SLOT(setVisibleInternal(bool)));
	}

	m_data->action = action;
	if (action)
	{
		action->setObjectName(this->objectName());
		action->setCheckable(true);
		action->setChecked(this->isVisible());
		connect(m_data->action, SIGNAL(triggered(bool)), this, SLOT(setVisibleInternal(bool)));
	}
}

QAction* VipToolWidget::action() const
{
	return m_data->action;
}

void VipToolWidget::setButton(QAbstractButton * button)
{
	if (m_data->button)
	{
		disconnect(m_data->button, SIGNAL(clicked(bool)), this, SLOT(setVisible(bool)));
	}

	m_data->button = button;
	if (button)
	{
		button->setCheckable(true);
		button->setChecked(this->isVisible());
		connect(m_data->button, SIGNAL(clicked(bool)), this, SLOT(setVisible(bool)));
	}
}
QAbstractButton* VipToolWidget::button() const
{
	return m_data->button;
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
	m_data->keepFloatingUserSize = enable;
	QMetaObject::invokeMethod(titleBarWidget(), "updateTitleAndPosition");
}

bool VipToolWidget::keepFloatingUserSize() const
{
	return m_data->keepFloatingUserSize;
}

void VipToolWidget::raise()
{
	QWidget::raise();
	//setFocus();
}

void VipToolWidget::setFocus()
{
	QWidget::setFocus();
	setStyleProperty("hasFocus", true);
}

void VipToolWidget::setStyleProperty(const char * name, bool value)
{
	bool this_current = this->property(name).value<bool>();
	bool bar_current = this->titleBarWidget()->property(name).value<bool>();
	bool scroll_current = m_data->scroll->property(name).value<bool>();
	if (this_current != value || bar_current != value || scroll_current != value)
	{
		this->setProperty(name, value);
		this->titleBarWidget()->setProperty(name, value);
		m_data->scroll->setProperty(name, value);
		polish();
	}
}

QSize	VipToolWidget::sizeHint() const
{
	if (m_data->size.isEmpty())
		return m_data->size;
	else
		return QDockWidget::sizeHint();
}

void VipToolWidget::enterEvent(QEvent *)
{
	setStyleProperty("hasHover", true);
	if (m_data->enableOpacityChange)
		this->setWindowOpacity(1);
	raise();
}

void VipToolWidget::leaveEvent(QEvent *)
{
	setStyleProperty("hasHover", false);
	if (m_data->enableOpacityChange)
		this->setWindowOpacity(0.7);
}

void VipToolWidget::closeEvent(QCloseEvent * evt)
{
	this->hide();
	evt->ignore();
}

void VipToolWidget::displayPlayerAreaChanged()
{
	this->setDisplayPlayerArea(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea());
	if (VipToolWidgetToolBar * bar = this->toolBar())
		bar->setDisplayPlayerArea(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea());
}

void VipToolWidget::focusChanged(QWidget * //old
, QWidget *now)
{
	while (now)
	{
		if (now == this || now == this->titleBarWidget())
		{
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

void VipToolWidget::showEvent(QShowEvent *)
{
	if (m_data->action)
	{
		m_data->action->blockSignals(true);
		m_data->action->setChecked(true);
		m_data->action->blockSignals(false);
	}
	if (m_data->button)
	{
		m_data->button->blockSignals(true);
		m_data->button->setChecked(true);
		m_data->button->blockSignals(false);
	}

	//if the dock widget is tabified with other dock widgets, raise it to be sure it is set as the current tab
	if (QMainWindow * p = qobject_cast<QMainWindow*>(parentWidget())) {
		QList<QDockWidget *> tabified = p->tabifiedDockWidgets(this);
		if (tabified.size())
			this->raise();
	}

	resetSize();
	setFocus();

	//change screen if necessary
	if (m_data->firstShow && isFloating()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		int screen = qApp->desktop()->screenNumber(this);
		int main_screen = qApp->desktop()->screenNumber(parentWidget());
#else
		int screen = qApp->screens().indexOf(this->screen());
		int main_screen = qApp->screens().indexOf(parentWidget()->screen());
#endif
		if (screen != main_screen && screen >= 0 && main_screen >= 0) {
			//compute offset
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
			QPoint offset = this->pos() - qApp->desktop()->screenGeometry(screen).topLeft();
			offset += qApp->desktop()->screenGeometry(main_screen).topLeft();
#else
			QPoint offset = this->pos() - qApp->screens()[screen]->availableGeometry().topLeft();// qApp->desktop()->screenGeometry(screen).topLeft();
			offset += qApp->screens()[main_screen]->availableGeometry().topLeft(); // qApp->desktop()->screenGeometry(main_screen).topLeft();
#endif
			this->move(offset);
		}
	}
	m_data->firstShow = false;
}

void VipToolWidget::hideEvent(QHideEvent *)
{

	if (m_data->action)
	{
		m_data->action->blockSignals(true);
		m_data->action->setChecked(false);
		m_data->action->blockSignals(false);
	}
	if (m_data->button)
	{
		m_data->button->blockSignals(true);
		m_data->button->setChecked(false);
		m_data->button->blockSignals(false);
	}
}


void VipToolWidget::floatingChanged(bool )
{
	//if (floating)
	// setWindowFlags((Qt::WindowFlags)(flags | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Window));
	// else
	// setWindowFlags((Qt::WindowFlags)flags);
}

void VipToolWidget::resetSize()
{
	if (!m_data->resetSizeRequest && isVisible())
	{
		m_data->resetSizeRequest = true;
		QMetaObject::invokeMethod(this, "internalResetSize", Qt::QueuedConnection);
	}

}

void VipToolWidget::internalResetSize()
{
	m_data->resetSizeRequest = false;

	//if(isFloating())
	// {
	// m_data->scroll->setFrameShape(QFrame::StyledPanel);
	// }
	// else
	// {
	// m_data->scroll->setFrameShape(QFrame::NoFrame);
	// }

	if (QWidget * w = widget())
	{
		//now resize the VipToolWidget, but make sure we stay in the desktop boundaries

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
		if (isFloating())
		{
			if (!keepFloatingUserSize())
			{
				w->resize(w->sizeHint());
				QSize s = this_rect.size();
				resize(s + QSize(0, titleBarWidget()->sizeHint().height()));
				m_data->size = s;
			}
			raise();
		}
		else
		{
			w->resize(scrollArea()->size());
			//m_data->size = QSize();
			// Qt::DockWidgetArea area = vipGetMainWindow()->dockWidgetArea(this);
			// if( (area & Qt::LeftDockWidgetArea) || (area & Qt::RightDockWidgetArea) )
			// resize(width(), qMax(height(),this_rect.height()));
			// else
			// resize(qMax(width(),this_rect.width()),height());
		}
	}
}

void VipToolWidget::resizeEvent(QResizeEvent * evt)
{
	QDockWidget::resizeEvent(evt);

	//int left = this->titleBarWidget()->mapToParent(QPoint(0, 0)).x();
	// int right = this->titleBarWidget()->mapToParent(QPoint(titleBarWidget()->width(), 0)).x();
	// int top = this->titleBarWidget()->mapToParent(QPoint(0, 0)).y();
	// int bottom = this->titleBarWidget()->mapToParent(QPoint(height(), 0)).y();
//
	// setMask(QRegion(QRect(left+5, top+5, right - left - 5, bottom - top - 5)));
}

void VipToolWidget::moveEvent(QMoveEvent *evt)
{
	//if (isMaximized())
	// {
	// showNormal();
	// QMetaObject::invokeMethod(titleBarWidget(), "updateTitleAndPosition");
	// }
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

	this->style()->unpolish(this);
	this->style()->polish(this);
	m_data->scroll->style()->unpolish(m_data->scroll);
	m_data->scroll->style()->polish(m_data->scroll);
	titleBarWidget()->style()->unpolish(titleBarWidget());
	titleBarWidget()->style()->polish(titleBarWidget());
	this->update();
}



VipToolWidgetPlayer::VipToolWidgetPlayer(VipMainWindow * window)
	:VipToolWidget(window), m_automaticTitleManagement(true)
{
	setWindowTitle("Edit plot items");
	setObjectName("Edit plot items");

	connect(window->displayArea(), SIGNAL(focusWidgetChanged(VipDragWidget*)), this, SLOT(focusWidgetChanged(VipDragWidget*)));
}

VipAbstractPlayer * VipToolWidgetPlayer::currentPlayer() const
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

void VipToolWidgetPlayer::setDisplayPlayerArea(VipDisplayPlayerArea * area)
{
	if (area && area != m_area)
	{
		m_area = area;
		focusWidgetChanged(area->dragWidgetHandler()->focusWidget());
	}
}

void VipToolWidgetPlayer::showEvent(QShowEvent * evt)
{
	if (widget())
		widget()->setEnabled(m_player);

	bool ok = this->setPlayer(m_player);
	if (VipToolWidgetToolBar * bar = toolBar())
	{
		if (ok) {
			bar->setEnabled(m_player);
			bar->setPlayer(m_player);
		}
		else {
			bar->setEnabled(false);
			bar->setPlayer(NULL);
		}
	}
	if (widget())
		widget()->setEnabled(ok);


	if (m_player && m_automaticTitleManagement) {
		//set the window title
		QString title = this->windowTitle();
		if (!title.isEmpty())
		{
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
		m_player = NULL;
	else
	{
		QList<VipAbstractPlayer*> players = w->findChildren<VipAbstractPlayer*>();
		if (!players.size())
			m_player = NULL;
		else
			m_player = players.back();
	}

	//if (isHidden()) {
	// //explicitly hidden
	// setPlayer(NULL);
	// if (widget())
	// widget()->setEnabled(false);
	// if (VipToolWidgetToolBar* bar = toolBar()) {
	// bar->setEnabled(false);
	// bar->setPlayer(NULL);
	// }
	// }
	// else 
{
		if (widget())
			widget()->setEnabled(m_player);

		bool ok = m_player;
		if ((ok=this->setPlayer(m_player)) && m_player && m_automaticTitleManagement) {
			QString title = this->windowTitle();
			if (!title.isEmpty())
			{
				QStringList lst = title.split(" - ");
				title = lst.first() + " - " + m_player->windowTitle();
			}
			else
				title = (m_player->windowTitle());

			this->setWindowTitle(title);
		}
		if(!ok) {
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
				bar->setPlayer(NULL);
			}
		}
	}

	//TEST resetSize()
	//QMetaObject::invokeMethod(this,"resetSize",Qt::QueuedConnection);
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

VipPlotToolWidgetPlayer::VipPlotToolWidgetPlayer(VipMainWindow * window)
	:VipToolWidgetPlayer(window)
{
	m_data = new PrivateData();
	this->setEnableOpacityChange(true);
	this->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	this->setAllowedAreas(Qt::NoDockWidgetArea);
	this->setFloating(true);
}

VipPlotToolWidgetPlayer::~VipPlotToolWidgetPlayer()
{
	if (m_data->scene)
		m_data->scene->removeEventFilter(this);
	delete m_data;
}

bool VipPlotToolWidgetPlayer::setPlayer(VipAbstractPlayer * pl)
{
	if (pl == m_data->player)
		return pl;
	m_data->player = pl;

	if (m_data->scene)
		m_data->scene->removeEventFilter(this);
	m_data->scene = NULL;

	if (!pl)
		return false;

	m_data->scene = pl->plotWidget2D() ? pl->plotWidget2D()->scene() : NULL;
	if (m_data->scene)
		m_data->scene->installEventFilter(this);


	if (QWidget * w = vipObjectEditor(QVariant::fromValue(pl)))
	{
		//we need to reset the internal widget, otherwise the new size won't be correct
		//(this size is updated when the widget is first shown)
		if (widget())
			delete widget();

		QWidget * p = new QWidget();
		QHBoxLayout * l = new QHBoxLayout();
		l->addWidget(w);
		p->setLayout(l);
		setWidget(p);

		connect(w, SIGNAL(itemChanged(QGraphicsObject*)), this, SLOT(resetSize()));
		connect(w, SIGNAL(abstractPlayerChanged(VipAbstractPlayer*)), this, SLOT(resetSize()));
	}
	return true;
}

void VipPlotToolWidgetPlayer::setItem(QGraphicsObject * item)
{
	if (item && item->scene())
	{
		//unselect all items
		QList<QGraphicsItem*> items = item->scene()->items();
		for (int i = 0; i < items.size(); ++i)
			if (items[i]->isSelected())
				items[i]->setSelected(false);

		//select the argument item
		item->setSelected(true);

		//set the player
		if (VipAbstractPlayer * player = VipAbstractPlayer::findAbstractPlayer(item))
		{
			if (currentPlayer() != player )
				setPlayer(player);
			if (widget() && !widget()->isEnabled())
				widget()->setEnabled(true);
		}
		else {
			if (QWidget * w = vipObjectEditor(QVariant::fromValue(item))){
				//we need to reset the internal widget, otherwise the new size won't be correct
				//(this size is updated when the widget is first shown)
				if (widget())
					delete widget();

				QWidget * p = new QWidget();
				QHBoxLayout * l = new QHBoxLayout();
				l->addWidget(w);
				p->setLayout(l);
				setWidget(p);
			}
		}
	}
}

bool VipPlotToolWidgetPlayer::eventFilter(QObject *, QEvent * evt)
{
	//filter scene events to show this tool widget on double click
	if (evt->type() == QEvent::GraphicsSceneMouseDoubleClick)
	{

		//set the default player's items if needed
		if (VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(m_data->player))
		{
			QList<QGraphicsItem*> items = pl->plotWidget2D()->scene()->selectedItems();
			if (items.isEmpty())
			{
				if (QGraphicsObject * obj = pl->defaultEditableObject()) {
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

VipPlotToolWidgetPlayer * vipGetPlotToolWidgetPlayer(VipMainWindow * window)
{
	static VipPlotToolWidgetPlayer * win = new VipPlotToolWidgetPlayer(window);
	return win;
}






struct ProgressWidget : public QFrame
{
	QLabel text;
	QProgressBar progressBar;
	QToolButton cancel;
	QPointer<VipProgress> progress;
	VipMultiProgressWidget * widget;

	ProgressWidget(VipProgress * p, VipMultiProgressWidget * widget, QWidget * parent = NULL)
		:QFrame(parent), progress(p), widget(widget)
	{
		QGridLayout * lay = new QGridLayout();
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

		//setStyleSheet(" QFrame { border: 1px solid #CCCCCC;}");
	}

	void setProgress(VipProgress * p)
	{
		if (p != progress)
		{
			if (progress)
				disconnect(&cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
			progress = p;
			if (progress)
				connect(&cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
		}
	}

	void setProgressBarVisible(bool visible)
	{
		if (visible != progressBar.isVisible())
		{
			progressBar.setVisible(visible);
			QGridLayout * lay = static_cast<QGridLayout*>(layout());
			if (visible)
			{
				lay->removeWidget(&cancel);
				lay->addWidget(&cancel, 1, 1);
			}
			else
			{
				lay->removeWidget(&cancel);
				lay->addWidget(&cancel, 0, 1);
			}
		}
	}

	bool progressBarVisible() const
	{
		return progressBar.isVisible();
	}
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
	QBoxLayout *layout;
	bool changeModality;

	ProgressWidget * find(VipProgress * p) const
	{
		for (int i = 0; i < progresses.size(); ++i)
			if (progresses[i]->progress == p)
				return progresses[i];
		return NULL;
	}

};



VipMultiProgressWidget::VipMultiProgressWidget(VipMainWindow * window)
	:VipToolWidget(window)
{
	setWindowTitle("Operations");
	setObjectName("Operations");

	m_data = new PrivateData();
	m_data->status.setText("No operation to display at this time");

	m_data->modalTimer.setSingleShot(true);
	m_data->modalTimer.setInterval(100);
	m_data->changeModality = false;

	connect(&m_data->modalTimer, SIGNAL(timeout()), this, SLOT(updateModality()), Qt::QueuedConnection);

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(&m_data->status);
	lay->setSpacing(2);

	QWidget * w = new QWidget();
	w->setLayout(lay);
	this->setWidget(w);
	m_data->layout = lay;
	//this->setMinimumWidth(300);
	this->resize(300, 100);

	m_data->reuse.append(new ProgressWidget(NULL, this));
	m_data->layout->addWidget(m_data->reuse.last());
	m_data->reuse.last()->hide();

	VipProgress::setProgressManager(this);

	//center the widget on the main screen
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
	delete m_data;
}

QMultiMap<QString, int> VipMultiProgressWidget::currentProgresses() const
{
	QMultiMap<QString, int> res;
	for (int i = 0; i < m_data->progresses.size(); ++i) {
		if (m_data->progresses[i]->isVisible()) {
			int value = -1;
			if (m_data->progresses[i]->progressBar.isVisible())
				value = m_data->progresses[i]->progressBar.value();
			QString text = m_data->progresses[i]->text.text();
			res.insert(text, value);
		}
	}
	return res;
}

void VipMultiProgressWidget::closeEvent(QCloseEvent * evt)
{
	evt->ignore();
	if (this->windowModality() != Qt::ApplicationModal)
		this->hide();
}

void VipMultiProgressWidget::showEvent(QShowEvent * evt)
{
	if (isFloating()) {
		//center the widget on the main screen
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
		QRect rect = QApplication::desktop()->screenGeometry();
#else
		QRect rect = this->screen() ? this->screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
#endif
		this->move(rect.x() + rect.width() / 2 - this->width() / 2, rect.y() + rect.height() / 2 - this->height() / 2);
	}

	if (m_data->changeModality)
	{
		QDockWidget::showEvent(evt);
		setFocus();
	}
	else
		VipToolWidget::showEvent(evt);
}

void VipMultiProgressWidget::changeModality(Qt::WindowModality modality)
{
	this->hide();
	m_data->changeModality = true;
	this->setWindowModality(modality);
	this->show();
	m_data->changeModality = false;

	//TEST: comment
	//if(modality != Qt::NonModal)
	//	QCoreApplication::processEvents(QEventLoop::AllEvents);
}

void VipMultiProgressWidget::updateScrollBars()
{
	this->scrollArea()->setVerticalScrollBarPolicy(isFloating() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
	this->scrollArea()->setHorizontalScrollBarPolicy(isFloating() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

void VipMultiProgressWidget::updateModality()
{
	if (!m_data->modalWidgets.size())
	{
		//remove modal attribute
		changeModality(Qt::NonModal);
	}
	else if (this->windowModality() != Qt::ApplicationModal)
	{
		//try to set modal attribute
		if (QApplication::activeModalWidget() && QApplication::activeModalWidget() != this)
		{
			//there is already a modal widget: do not set modal attribute, try later
			m_data->modalTimer.start();
		}
		else
		{
			changeModality(Qt::ApplicationModal);
		}
	}
}

void VipMultiProgressWidget::cancelRequested()
{
	//make sure to cancel all sub operations
	bool start_cancel = false;
	for (int i = 0; i < m_data->progresses.size(); ++i)
	{
		if (start_cancel) {
			if (m_data->progresses[i]->progress)
				m_data->progresses[i]->progress->cancelRequested();
		}
		else if (sender() == &m_data->progresses[i]->cancel) {
			start_cancel = true;
		}
	}
}

void VipMultiProgressWidget::addProgress(QObjectPointer ptr)
{
	if (VipProgress * p = qobject_cast<VipProgress*>(ptr.data()))
	{
		ProgressWidget * w = m_data->reuse.size() ?
			m_data->reuse.first() :
			new ProgressWidget(p, this);
		w->setProgress(p);

		if (m_data->reuse.size())
			m_data->reuse.removeAt(0);
		else
			m_data->layout->addWidget(w);

		m_data->status.hide();
		m_data->progresses.append(w);
		m_data->progresses.back()->progressBar.setRange(p->min(), p->max());
		m_data->progresses.back()->text.setText(p->text());

		m_data->progresses.back()->show();
		this->show();
		this->resetSize();
	}
}

void VipMultiProgressWidget::removeProgress(QObjectPointer ptr)
{
	VipProgress * p = qobject_cast<VipProgress*>(ptr.data());

	//remove all invalid progress bar
	for (int i = 0; i < m_data->progresses.size(); ++i)
	{
		ProgressWidget * w = m_data->progresses[i];
		if (w->progress == p || !w->progress)
		{
			m_data->modalWidgets.remove(w);
			m_data->progresses.removeAt(i);
			w->progressBar.hide();
			w->progressBar.setValue(0);
			w->text.setText(QString());

			if (m_data->reuse.indexOf(w) < 0)
				m_data->reuse.append(w);
			w->hide();

			i--;
		}
	}

	//update status text visibility
	m_data->status.setVisible(m_data->progresses.size() == 0);

	//update this window modality
	updateModality();

	if (m_data->progresses.size() == 0) {
#ifdef _WIN32
		this->hide();
#else
		// On some linux config, only this works properly to hide the progress tool widget (??)
		QMetaObject::invokeMethod(this,"update",Qt::QueuedConnection);
		QMetaObject::invokeMethod(this,"hide",Qt::QueuedConnection);
		QMetaObject::invokeMethod(this,"update",Qt::QueuedConnection);
		QCoreApplication::processEvents();
#endif

	}
}

void VipMultiProgressWidget::setText(QObjectPointer ptr, const QString & text)
{
	if (VipProgress * p = qobject_cast<VipProgress*>(ptr.data()))
	{
		if (ProgressWidget * w = m_data->find(p))
		{
			bool reset_size = p->isModal();
			if (text.size() && w->text.isHidden())
			{
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
	if (VipProgress * p = qobject_cast<VipProgress*>(ptr.data()))
	{
		if (ProgressWidget * w = m_data->find(p))
		{
			if (w->progressBar.isHidden())
			{
				w->setProgressBarVisible(true);
				this->resetSize();
			}
			w->progressBar.setValue(value);

			if (this->windowModality() == Qt::ApplicationModal)
				setFocus();
		}
	}
}

void VipMultiProgressWidget::setCancelable(QObjectPointer ptr, bool cancelable)
{
	if (VipProgress * p = qobject_cast<VipProgress*>(ptr.data()))
	{
		if (ProgressWidget * w = m_data->find(p))
			w->cancel.setVisible(cancelable);
	}
}

void VipMultiProgressWidget::setModal(QObjectPointer ptr, bool modal)
{
	if (VipProgress * p = qobject_cast<VipProgress*>(ptr.data()))
	{
		if (ProgressWidget * w = m_data->find(p))
		{
			//set modal
			if (modal && !m_data->modalWidgets.contains(w))
			{
				m_data->modalWidgets.insert(w);
				updateModality();

				//center the widget inside its parent (if any)
				if (parentWidget() && parentWidget()->isVisible()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
					QRect rect;
					int screen = qApp->desktop()->screenNumber(this->parentWidget());
					if (this->parentWidget())
						rect = this->parentWidget()->geometry();
					else
						rect = QApplication::desktop()->screenGeometry(screen);
#else
					//Qt6 way
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
				}
				else {

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
					QRect rect;
					rect = QApplication::desktop()->screenGeometry(0);
#else
					//center the widget on the main screen
					QRect rect;
					if (this->screen())
						rect = this->screen()->availableGeometry();
					else
						rect = QGuiApplication::primaryScreen()->availableGeometry();
#endif
					this->move(rect.x() + rect.width() / 2 - this->width() / 2, rect.y() + rect.height() / 2 - this->height() / 2);
				}

			}
			else if (!modal && m_data->modalWidgets.contains(w))
			{
				m_data->modalWidgets.remove(w);
				updateModality();
			}
		}

		//TEST resetSize();
		//this->resetSize();
	}
}




VipMultiProgressWidget * vipGetMultiProgressWidget(VipMainWindow * window)
{
	static VipMultiProgressWidget* widget = new VipMultiProgressWidget(window);
	return widget;
}
