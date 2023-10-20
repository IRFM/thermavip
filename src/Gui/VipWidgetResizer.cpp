#include "VipWidgetResizer.h"


#include <qapplication.h>
#include <qtimer.h>
#include <qevent.h>
#include <qdesktopwidget.h>





class VipWidgetResizerHandler : public QObject
{
public:
	QList<VipWidgetResizer*> resizers;
	QWidget* grabber;

	VipWidgetResizerHandler(QObject* parent)
	  : QObject(parent)
	  , grabber(nullptr)
	{
	}
	void addResizer(VipWidgetResizer* r)
	{
		if (resizers.isEmpty())
			QApplication::instance()->installEventFilter(this);
		resizers.append(r);
	}
	void removeResizer(VipWidgetResizer* r)
	{
		resizers.removeOne(r);
		if (resizers.isEmpty())
			QApplication::instance()->removeEventFilter(this);
	}
	bool eventFilter(QObject* watched, QEvent* event)
	{

		bool used = event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseMove || event->type() == QEvent::Hide;

		if (!used)
			return false;

		bool res = false;
		bool has_cursor = false;
		for (QList<VipWidgetResizer*>::iterator it = resizers.begin(); it != resizers.end(); ++it) {
			if (!(*it)->isEnabled())
				continue;

			res = res || (*it)->filter(watched, event);
			has_cursor = has_cursor || (*it)->hasCustomCursor();
		}
		if (!has_cursor && QGuiApplication::overrideCursor())
			QGuiApplication::restoreOverrideCursor();
		return res;
	}
};

static VipWidgetResizerHandler* handler()
{
	static VipWidgetResizerHandler* h = new VipWidgetResizerHandler(qApp);
	return h;
}

class VipWidgetResizer::PrivateData
{
public:
	PrivateData()
	  : inner_detect(5)
	  , outer_detect(10)
	  , cursor_count(0)
	  , enable(true)
	  , custom_cursor(false)
	  , enableOutsideParent(true)
	{
	}
	QPoint mousePressGlobal;
	QPoint mousePress;
	QTimer timer;
	int inner_detect;
	int outer_detect;
	int cursor_count;
	bool enable;
	bool custom_cursor;
	bool enableOutsideParent;
};

void VipWidgetResizer::addCursor()
{
	m_data->cursor_count++;
}

bool VipWidgetResizer::hasCustomCursor() const
{
	return m_data->custom_cursor;
}

void VipWidgetResizer::removeCursors()
{
	while (m_data->cursor_count) {
		QGuiApplication::restoreOverrideCursor();
		--m_data->cursor_count;
		// vipProcessEvents();
	}
	m_data->custom_cursor = false;
}

VipWidgetResizer::VipWidgetResizer(QWidget* parent)
  : QObject(parent)
{
	m_data = new PrivateData();

	// QApplication::instance()->installEventFilter(this);
	m_data->timer.setSingleShot(true);
	m_data->timer.setInterval(300);
	connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(updateCursor()));

	handler()->addResizer(this);
}

VipWidgetResizer::~VipWidgetResizer()
{
	handler()->removeResizer(this);
	// QApplication::instance()->removeEventFilter(this);
	m_data->timer.stop();
	disconnect(&m_data->timer, SIGNAL(timeout()), this, SLOT(updateCursor()));
	delete m_data;
}

void VipWidgetResizer::updateCursor()
{
	bool remove_cursor = false;
	if (!parent())
		remove_cursor = true;
	if (!parent()->isVisible())
		remove_cursor = true;
	if (!isTopLevelWidget())
		remove_cursor = true;
	if (remove_cursor)
		removeCursors();
}

QWidget* VipWidgetResizer::parent() const
{
	return static_cast<QWidget*>(QObject::parent());
}

void VipWidgetResizer::setBounds(int inner_detect, int outer_detect)
{
	m_data->inner_detect = inner_detect;
	m_data->outer_detect = outer_detect;
}

int VipWidgetResizer::innerDetect() const
{
	return m_data->inner_detect;
}
int VipWidgetResizer::outerDetect() const
{
	return m_data->outer_detect;
}

void VipWidgetResizer::setEnabled(bool enable)
{
	if (enable != m_data->enable) {
		m_data->enable = enable;
		removeCursors();
	}
}
bool VipWidgetResizer::isEnabled() const
{
	return m_data->enable;
}

void VipWidgetResizer::enableOutsideParent(bool enable)
{
	if (enable != m_data->enableOutsideParent) {
		m_data->enableOutsideParent = enable;
	}
}
bool VipWidgetResizer::outsideParentEnabled() const
{
	return m_data->enableOutsideParent;
}

bool VipWidgetResizer::isTopLevelWidget(const QPoint& screen_pos) const
{
	//VipMultiDragWidget* p = qobject_cast<VipMultiDragWidget*>(parent());
	//if (!p || !p->isTopLevel())
	//	return false;
	QWidget* p = parent();
	if (screen_pos == QPoint())
		return true;

	const QRect r(0, 0, p->width(), p->height());
	const QRect adjusted = r.adjusted(-outerDetect(), -outerDetect(), outerDetect(), outerDetect());
	const QPoint pos = p->mapFromGlobal(screen_pos);

	// check if the mouse is not in the control area
	if (!adjusted.contains(pos) || r.adjusted(innerDetect(), innerDetect(), -innerDetect(), -innerDetect()).contains(pos))
		return false;

	if (r.contains(pos)) {
		// mouse inside widget: check that the widget under the  mouse is a children
		if (QWidget* under_mouse = QApplication::widgetAt(screen_pos)) {
			return parent()->isAncestorOf(under_mouse);
		}
		else
			return true;
	}
	else {
		// mouse outside:  check that the widget under the  mouse is NOT a children of another player
		if (QWidget* under_mouse = QApplication::widgetAt(screen_pos)) {
			while (under_mouse) {
				if (under_mouse != parent())
					return false;
				
				under_mouse = under_mouse->parentWidget();
			}
			return false;
		}
		else
			return true;
	}
}

static QSize parentSize(QWidget* w)
{
	if (!w)
		return QSize();
	else if (w->parentWidget())
		return w->parentWidget()->size();
	else {
		QDesktopWidget* d = qApp->desktop();
		return d->screenGeometry(w).size();
	}
}

QPoint VipWidgetResizer::validPosition(const QPoint& pt, bool* ok) const
{
	(void)ok;
	if (m_data->enableOutsideParent)
		return pt;
	else {
		QPoint p = pt;
		if (p.x() < 0)
			p.setX(0);
		if (p.y() < 0)
			p.setY(0);
		return p;
	}
}

QSize VipWidgetResizer::validSize(const QSize& s, bool* ok) const
{
	(void)ok;
	QSize p = parentSize(parent());
	QSize res = s;
	if (parent()->pos().x() + s.width() > p.width())
		res.setWidth(p.width() - parent()->pos().x());
	if (parent()->pos().y() + s.height() > p.height())
		res.setHeight(p.height() - parent()->pos().y());
	return res;
}

bool VipWidgetResizer::filter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::Hide && watched == parent())
		m_data->custom_cursor = false;
	if (!event->spontaneous())
		return false;
	if (!watched || !watched->isWidgetType())
		return false;
	if (handler()->grabber && handler()->grabber != parent())
		return false;
	if (!parent()->isVisible())
		return false;
	if (!isTopLevelWidget())
		return false;

	// filter app events for resizeing
	if (event->type() == QEvent::Hide) {
		removeCursors();
		return false;
	}
	if (event->type() == QEvent::MouseMove) {
		m_data->custom_cursor = false;

		QPoint screen = static_cast<QMouseEvent*>(event)->screenPos().toPoint();
		QPoint p = parent()->mapFromGlobal(screen);
		// if a widget is above this widget, ignore
		if (handler()->grabber != parent()) {
			if (!isTopLevelWidget(screen)) {
				removeCursors();
				return false;
			}
		}

		if (QRect(0, 0, parent()->width(), parent()->height()).adjusted(-m_data->outer_detect, -m_data->outer_detect, m_data->outer_detect, m_data->outer_detect).contains(p)) {
			if (m_data->mousePressGlobal == QPoint()) {
				// set the cursor
				if (p.y() > parent()->height() - m_data->inner_detect && p.x() > parent()->width() - m_data->inner_detect) {
					// bottom right
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeFDiagCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 5);
					addCursor();
					m_data->timer.start();
				}
				else if (p.y() < m_data->inner_detect && p.x() < m_data->inner_detect) {
					// top left
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeFDiagCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 6);
					addCursor();
					m_data->timer.start();
				}
				else if (p.y() < m_data->inner_detect && p.x() > parent()->width() - m_data->inner_detect) {
					// top  right
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeBDiagCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 7);
					addCursor();
					m_data->timer.start();
				}
				else if (p.y() > parent()->height() - m_data->inner_detect && p.x() <= m_data->inner_detect) {
					// bottom left
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeBDiagCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 8);
					addCursor();
					m_data->timer.start();
				}
				else if (p.x() <= m_data->inner_detect) {
					// left area
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeHorCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 1);
					addCursor();
					m_data->timer.start();
				}
				else if (p.x() > parent()->width() - m_data->inner_detect) {
					// right
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeHorCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 2);
					addCursor();
					m_data->timer.start();
				}
				else if (p.y() < m_data->inner_detect) {
					// top
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeVerCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 3);
					addCursor();
					m_data->timer.start();
				}
				else if (p.y() > parent()->height() - m_data->inner_detect) {
					// bottom
					QGuiApplication::setOverrideCursor(QCursor(Qt::SizeVerCursor));
					m_data->custom_cursor = true;
					this->setProperty("area", 4);
					addCursor();
					m_data->timer.start();
				}
				else if (m_data->mousePressGlobal == QPoint()) {
					removeCursors();
					this->setProperty("area", 0);
					return false;
				}
			}
			else if (QRect(m_data->inner_detect, m_data->inner_detect, parent()->width() - m_data->inner_detect * 2, parent()->height() - m_data->inner_detect * 2).contains(p) &&
				 m_data->mousePressGlobal == QPoint()) {
				removeCursors();
				this->setProperty("area", 0);
			}
		}
		else if (m_data->mousePressGlobal == QPoint()) {
			removeCursors();
			this->setProperty("area", 0);
		}

		if (m_data->mousePressGlobal != QPoint()) {
			int area = property("area").toInt();
			QPoint diff = p - m_data->mousePress;

			switch (area) {
				case 0:
					return false;
				case 1:
					// left
					// if (m_data->enableOutsideParent || parent()->pos().x() + diff.x() >= 0) {
					// parent()->resize(parent()->width() - diff.x(), parent()->height());
					// parent()->move(parent()->pos().x() + diff.x(), parent()->pos().y());
					// }
					parent()->move(validPosition(QPoint(parent()->pos().x() + diff.x(), parent()->pos().y())));
					parent()->resize(validSize(QSize(parent()->width() - diff.x(), parent()->height())));
					break;
				case 2:
					// right
					// if (m_data->enableOutsideParent || parent()->width() + diff.x() + parent()->pos().x()  <= parentSize(parent()).width()) {
					// parent()->resize(parent()->width() + diff.x(), parent()->height());
					// m_data->mousePress = p;
					// }
					parent()->resize(validSize(QSize(parent()->width() + diff.x(), parent()->height())));
					m_data->mousePress = p;
					break;
				case 3:
					// top
					// if (m_data->enableOutsideParent || parent()->pos().y() + diff.y() >= 0) {
					// parent()->resize(parent()->width(), parent()->height() - diff.y());
					// parent()->move(parent()->pos().x(), parent()->pos().y() + diff.y());
					// }
					parent()->move(validPosition(QPoint(parent()->pos().x(), parent()->pos().y() + diff.y())));
					parent()->resize(validSize(QSize(parent()->width(), parent()->height() - diff.y())));
					break;
				case 4:
					// bottom
					// if (m_data->enableOutsideParent || parent()->height() + diff.y() + parent()->pos().y() <= parentSize(parent()).height()) {
					// parent()->resize(parent()->width(), parent()->height() + diff.y());
					// m_data->mousePress = p;
					// }
					parent()->resize(validSize(QSize(parent()->width(), parent()->height() + diff.y())));
					m_data->mousePress = p;
					break;
				case 5:
					// bottom right
					// parent()->resize(parent()->width() + diff.x(), parent()->height() + diff.y());
					parent()->resize(validSize(QSize(parent()->width() + diff.x(), parent()->height() + diff.y())));
					m_data->mousePress = p;
					break;
				case 6:
					// top left
					// parent()->resize(parent()->width() - diff.x(), parent()->height() - diff.y());
					// parent()->move(parent()->pos() + diff);
					parent()->move(validPosition(QPoint(parent()->pos() + diff)));
					parent()->resize(validSize(QSize(parent()->width() - diff.x(), parent()->height() - diff.y())));
					m_data->mousePress = p - diff;
					break;
				case 7:
					// top right
					parent()->move(validPosition(QPoint(parent()->pos().x(), (parent()->pos() + diff).y())));
					parent()->resize(validSize(QSize(parent()->width() + diff.x(), parent()->height() - diff.y())));
					m_data->mousePress = QPoint(p.x(), (p - diff).y());
					break;
				case 8:
					// bottom left
					parent()->move(validPosition(QPoint((parent()->pos() + diff).x(), parent()->pos().y())));
					parent()->resize(validSize(QSize(parent()->width() - diff.x(), parent()->height() + diff.y())));
					m_data->mousePress = QPoint((p - diff).x(), p.y());
					break;
			}
			return true;
		}

		return false;
	}
	else if (event->type() == QEvent::MouseButtonPress) {
		if (static_cast<QMouseEvent*>(event)->buttons() & Qt::LeftButton)
			if (property("area").toInt()) {
				QPoint screen = static_cast<QMouseEvent*>(event)->screenPos().toPoint();
				if (!isTopLevelWidget(screen))
					return false;

				m_data->mousePressGlobal = screen;
				m_data->mousePress = parent()->mapFromGlobal(m_data->mousePressGlobal);
				parent()->raise();
				handler()->grabber = parent();
				return true;
			}
	}
	else if (event->type() == QEvent::MouseButtonRelease) {
		bool res = m_data->mousePressGlobal != QPoint();
		m_data->mousePressGlobal = QPoint();
		m_data->mousePress = QPoint();
		handler()->grabber = nullptr;
		setProperty("area", 0);
		return res;
	}

	return false;
}
