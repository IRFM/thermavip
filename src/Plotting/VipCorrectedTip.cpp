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

#include "VipCorrectedTip.h"

#ifdef Q_DEAD_CODE_FROM_QT4_MAC
#include <private/qcore_mac_p.h>
#endif

#include "qapplication.h"
#include "qdebug.h"
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "qdesktopwidget.h"
#endif
#include "qelapsedtimer.h"
#include "qevent.h"
#include "qimage.h"
#include "qpainter.h"
#include "qpixmap.h"
#include "qpointer.h"
#include "qscreen.h"
#include "qtimer.h"
#include <qapplication.h>
#include <qevent.h>
#include <qlabel.h>
#include <qpointer.h>
#include <qscreen.h>
#include <qstyle.h>
#include <qstyleoption.h>
#include <qstylepainter.h>
#include <qtextdocument.h>
#include <qtimer.h>
#include <qtooltip.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "qdesktopwidget.h"
#endif

static QAlphaWidget* q_blend = 0;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
static QWidget* effectParent(const QWidget* w)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	const int screenNumber = w ? QApplication::desktop()->screenNumber(w) : 0;
#else
	const int screenNumber = w ? QGuiApplication::screens().indexOf(w->screen()) : 0;
#endif
	QT_WARNING_PUSH // ### Qt 6: Find a replacement for QDesktopWidget::screen()
	  QT_WARNING_DISABLE_DEPRECATED return QApplication::desktop()
	    ->screen(screenNumber);
	QT_WARNING_POP
}
#endif

// Constructs a QAlphaWidget.
QAlphaWidget::QAlphaWidget(QWidget* w, Qt::WindowFlags f)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  : QWidget(effectParent(w), f)
#else
  : QWidget(nullptr, f)
#endif
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	if (w)
		this->setScreen(w->screen());
#endif
#ifndef Q_OS_WIN
	setEnabled(false);
#endif
	setAttribute(Qt::WA_NoSystemBackground, true);
	widget = w;
	alpha = 0;
}

QAlphaWidget::~QAlphaWidget()
{
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
	// Restore user-defined opacity value
	if (widget)
		widget->setWindowOpacity(1);
#endif
}

// \reimp
void QAlphaWidget::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.drawPixmap(0, 0, pm);
}

// Starts the alphablending animation.
// The animation will take about \a time ms
void QAlphaWidget::run(int time)
{
	duration = time;

	if (duration < 0)
		duration = 150;

	if (!widget)
		return;

	elapsed = 0;
	checkTime.start();

	showWidget = true;
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
	qApp->installEventFilter(this);
	widget->setWindowOpacity(0.0);
	widget->show();
	connect(&anim, SIGNAL(timeout()), this, SLOT(render()));
	anim.start(1);
#else
	// This is roughly equivalent to calling setVisible(true) without actually showing the widget
	widget->setAttribute(Qt::WA_WState_ExplicitShowHide, true);
	widget->setAttribute(Qt::WA_WState_Hidden, false);

	qApp->installEventFilter(this);

	move(widget->geometry().x(), widget->geometry().y());
	resize(widget->size().width(), widget->size().height());

	frontImage = widget->grab().toImage();
	if (this->screen())
		backImage = this->screen()->grabWindow(0, widget->geometry().x(), widget->geometry().y(), widget->geometry().width(), widget->geometry().height()).toImage();
	/* backImage = QGuiApplication::primaryScreen()
		      ->grabWindow(QApplication::desktop()->winId(), widget->geometry().x(), widget->geometry().y(), widget->geometry().width(), widget->geometry().height())
		      .toImage();
	*/
	if (!backImage.isNull() && checkTime.elapsed() < duration / 2) {
		mixedImage = backImage.copy();
		pm = QPixmap::fromImage(mixedImage);
		show();
		setEnabled(false);

		connect(&anim, SIGNAL(timeout()), this, SLOT(render()));
		anim.start(1);
	}
	else {
		duration = 0;
		render();
	}
#endif
}

// \reimp
bool QAlphaWidget::eventFilter(QObject* o, QEvent* e)
{
	switch (e->type()) {
		case QEvent::Move:
			if (o != widget)
				break;
			move(widget->geometry().x(), widget->geometry().y());
			update();
			break;
		case QEvent::Hide:
		case QEvent::Close:
			if (o != widget)
				break;
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
			showWidget = false;
			render();
			break;
		case QEvent::KeyPress: {
			QKeyEvent* ke = (QKeyEvent*)e;
			if (ke->matches(QKeySequence::Cancel)) {
				showWidget = false;
			}
			else {
				duration = 0;
			}
			render();
			break;
		}
		default:
			break;
	}
	return QWidget::eventFilter(o, e);
}

// \reimp
void QAlphaWidget::closeEvent(QCloseEvent* e)
{
	e->accept();
	if (!q_blend)
		return;

	showWidget = false;
	render();

	QWidget::closeEvent(e);
}

// Render alphablending for the time elapsed.
//
// Show the blended widget and free all allocated source
// if the blending is finished.
void QAlphaWidget::render()
{
	int tempel = checkTime.elapsed();
	if (elapsed >= tempel)
		elapsed++;
	else
		elapsed = tempel;

	if (duration != 0)
		alpha = tempel / double(duration);
	else
		alpha = 1;

#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
	if (alpha >= 1 || !showWidget) {
		anim.stop();
		qApp->removeEventFilter(this);
		widget->setWindowOpacity(1);
		q_blend = 0;
		deleteLater();
	}
	else {
		widget->setWindowOpacity(alpha);
	}
#else
	if (alpha >= 1 || !showWidget) {
		anim.stop();
		qApp->removeEventFilter(this);

		if (widget) {
			if (!showWidget) {
#ifdef Q_OS_WIN
				setEnabled(true);
				setFocus();
#endif // Q_OS_WIN
				widget->hide();
			}
			else {
				// Since we are faking the visibility of the widget
				// we need to unset the hidden state on it before calling show
				widget->setAttribute(Qt::WA_WState_Hidden, true);
				widget->show();
				lower();
			}
		}
		q_blend = 0;
		deleteLater();
	}
	else {
		alphaBlend();
		pm = QPixmap::fromImage(mixedImage);
		repaint();
	}
#endif // defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
}

// Calculate an alphablended image.
void QAlphaWidget::alphaBlend()
{
	const int a = qRound(alpha * 256);
	const int ia = 256 - a;

	const int sw = frontImage.width();
	const int sh = frontImage.height();
	const int bpl = frontImage.bytesPerLine();
	switch (frontImage.depth()) {
		case 32: {
			uchar* mixed_data = mixedImage.bits();
			const uchar* back_data = backImage.bits();
			const uchar* front_data = frontImage.bits();

			for (int sy = 0; sy < sh; sy++) {
				quint32* mixed = (quint32*)mixed_data;
				const quint32* back = (const quint32*)back_data;
				const quint32* front = (const quint32*)front_data;
				for (int sx = 0; sx < sw; sx++) {
					quint32 bp = back[sx];
					quint32 fp = front[sx];

					mixed[sx] = qRgb((qRed(bp) * ia + qRed(fp) * a) >> 8, (qGreen(bp) * ia + qGreen(fp) * a) >> 8, (qBlue(bp) * ia + qBlue(fp) * a) >> 8);
				}
				mixed_data += bpl;
				back_data += bpl;
				front_data += bpl;
			}
		}
		default:
			break;
	}
}

static QRollEffect* q_roll = 0;

// Construct a QRollEffect widget.
QRollEffect::QRollEffect(QWidget* w, Qt::WindowFlags f, DirFlags orient)
  : QWidget(0, f)
  , orientation(orient)
{
#ifndef Q_OS_WIN
	setEnabled(false);
#endif

	widget = w;
	Q_ASSERT(widget);

	setAttribute(Qt::WA_NoSystemBackground, true);

	if (widget->testAttribute(Qt::WA_Resized)) {
		totalWidth = widget->width();
		totalHeight = widget->height();
	}
	else {
		totalWidth = widget->sizeHint().width();
		totalHeight = widget->sizeHint().height();
	}

	currentHeight = totalHeight;
	currentWidth = totalWidth;

	if (orientation & (RightScroll | LeftScroll))
		currentWidth = 0;
	if (orientation & (DownScroll | UpScroll))
		currentHeight = 0;

	pm = widget->grab();
}

// \reimp
void QRollEffect::paintEvent(QPaintEvent*)
{
	int x = orientation & RightScroll ? qMin(0, currentWidth - totalWidth) : 0;
	int y = orientation & DownScroll ? qMin(0, currentHeight - totalHeight) : 0;

	QPainter p(this);
	p.drawPixmap(x, y, pm);
}

// \reimp
void QRollEffect::closeEvent(QCloseEvent* e)
{
	e->accept();
	if (done)
		return;

	showWidget = false;
	done = true;
	scroll();

	QWidget::closeEvent(e);
}

// Start the animation.
//
// The animation will take about \a time ms, or is
// calculated if \a time is negative
void QRollEffect::run(int time)
{
	if (!widget)
		return;

	duration = time;
	elapsed = 0;

	if (duration < 0) {
		int dist = 0;
		if (orientation & (RightScroll | LeftScroll))
			dist += totalWidth - currentWidth;
		if (orientation & (DownScroll | UpScroll))
			dist += totalHeight - currentHeight;
		duration = qMin(qMax(dist / 3, 50), 120);
	}

	connect(&anim, SIGNAL(timeout()), this, SLOT(scroll()));

	move(widget->geometry().x(), widget->geometry().y());
	resize(qMin(currentWidth, totalWidth), qMin(currentHeight, totalHeight));

	// This is roughly equivalent to calling setVisible(true) without actually showing the widget
	widget->setAttribute(Qt::WA_WState_ExplicitShowHide, true);
	widget->setAttribute(Qt::WA_WState_Hidden, false);

	show();
	setEnabled(false);

	showWidget = true;
	done = false;
	anim.start(1);
	checkTime.start();
}

// Roll according to the time elapsed.
void QRollEffect::scroll()
{
	if (!done && widget) {
		int tempel = checkTime.elapsed();
		if (elapsed >= tempel)
			elapsed++;
		else
			elapsed = tempel;

		if (currentWidth != totalWidth) {
			currentWidth = totalWidth * (elapsed / duration) + (2 * totalWidth * (elapsed % duration) + duration) / (2 * duration);
			// equiv. to int((totalWidth*elapsed) / duration + 0.5)
			done = (currentWidth >= totalWidth);
		}
		if (currentHeight != totalHeight) {
			currentHeight = totalHeight * (elapsed / duration) + (2 * totalHeight * (elapsed % duration) + duration) / (2 * duration);
			// equiv. to int((totalHeight*elapsed) / duration + 0.5)
			done = (currentHeight >= totalHeight);
		}
		done = (currentHeight >= totalHeight) && (currentWidth >= totalWidth);

		int w = totalWidth;
		int h = totalHeight;
		int x = widget->geometry().x();
		int y = widget->geometry().y();

		if (orientation & RightScroll || orientation & LeftScroll)
			w = qMin(currentWidth, totalWidth);
		if (orientation & DownScroll || orientation & UpScroll)
			h = qMin(currentHeight, totalHeight);

		setUpdatesEnabled(false);
		if (orientation & UpScroll)
			y = widget->geometry().y() + qMax(0, totalHeight - currentHeight);
		if (orientation & LeftScroll)
			x = widget->geometry().x() + qMax(0, totalWidth - currentWidth);
		if (orientation & UpScroll || orientation & LeftScroll)
			move(x, y);

		resize(w, h);
		setUpdatesEnabled(true);
		repaint();
	}
	if (done || !widget) {
		anim.stop();
		if (widget) {
			if (!showWidget) {
#ifdef Q_OS_WIN
				setEnabled(true);
				setFocus();
#endif
				widget->hide();
			}
			else {
				// Since we are faking the visibility of the widget
				// we need to unset the hidden state on it before calling show
				widget->setAttribute(Qt::WA_WState_Hidden, true);
				widget->show();
				lower();
			}
		}
		q_roll = 0;
		deleteLater();
	}
}

/// Scroll widget \a w in \a time ms. \a orient may be 1 (vertical), 2
/// (horizontal) or 3 (diagonal).
void qScrollEffect(QWidget* w, QEffects::DirFlags orient = QEffects::DownScroll, int time = -1)
{
	if (q_roll) {
		q_roll->deleteLater();
		q_roll = 0;
	}

	if (!w)
		return;

	QApplication::sendPostedEvents(w, QEvent::Move);
	QApplication::sendPostedEvents(w, QEvent::Resize);
	Qt::WindowFlags flags = Qt::ToolTip;

	// those can be popups - they would steal the focus, but are disabled
	q_roll = new QRollEffect(w, flags, orient);
	q_roll->run(time);
}

/// Fade in widget \a w in \a time ms.
void qFadeEffect(QWidget* w, int time = -1)
{
	if (q_blend) {
		q_blend->deleteLater();
		q_blend = 0;
	}

	if (!w)
		return;

	QApplication::sendPostedEvents(w, QEvent::Move);
	QApplication::sendPostedEvents(w, QEvent::Resize);

	Qt::WindowFlags flags = Qt::ToolTip;

	// those can be popups - they would steal the focus, but are disabled
	q_blend = new QAlphaWidget(w, flags);

	q_blend->run(time);
}

#include <qboxlayout.h>
#include <qgraphicseffect.h>

VipTipLabel::VipTipLabel(VipTipContainer* parent)
  : QLabel(parent)
  , container(parent)
  , expireTimeMs(-1)
{
	// setAttribute(Qt::WA_TransparentForMouseEvents);
}

bool VipTipLabel::dropShadowEnabled() const
{
	return container->effect->isEnabled();
}

double VipTipLabel::dropShadowOffset() const
{
	return container->effect->xOffset();
}

double VipTipLabel::dropShadowBlurRadius() const
{
	return container->effect->blurRadius();
}

int VipTipLabel::expireTime() const
{
	return expireTimeMs;
}

void VipTipLabel::setDropShadowEnabled(bool enable)
{
	container->effect->setEnabled(enable);
}
void VipTipLabel::setdropShadowOffset(double offset)
{
	container->effect->setXOffset(offset);
	container->effect->setYOffset(offset);
}
void VipTipLabel::setDropShadowBlurRadius(double radius)
{
	container->effect->setBlurRadius(radius);
}
void VipTipLabel::setExpireTime(int time_ms)
{
	expireTimeMs = time_ms;
}

bool VipTipLabel::event(QEvent* evt)
{
	if (evt->type() == QEvent::MouseMove)
		return false;
	return QLabel::event(evt);
}

VipTipContainer* _instance = 0;

VipTipContainer::VipTipContainer(const QString& text, QWidget* w, int msecDisplayTime, bool is_fake)
  : QWidget(w, Qt::ToolTip | Qt::BypassGraphicsProxyWidget | Qt::FramelessWindowHint)
  , styleSheetParent(0)
  , fake(is_fake)
  , widget(0)
{
	label = new VipTipLabel(const_cast<VipTipContainer*>(this));
	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(label);
	vlay->setContentsMargins(0, 0, 10, 10);
	setLayout(vlay);

	setMouseTracking(true);
	setAttribute(Qt::WA_TranslucentBackground, true);

	if (!is_fake) {
		delete _instance;
		_instance = this;

		label->setAutoFillBackground(true);

		label->setFont(QToolTip::font());
		label->setFrameStyle(QFrame::Box);
		label->setFrameShadow(QFrame::Plain);

		QPalette sample_palette = label->palette();
		sample_palette.setColor(QPalette::Window, Qt::white);
		sample_palette.setColor(QPalette::WindowText, QColor(70, 70, 70));
		label->setPalette(sample_palette);
		label->ensurePolished();
	}

	label->setMargin(3 + style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, 0, this));
	label->setAlignment(Qt::AlignLeft);
	label->setIndent(1);

	if (!is_fake) {
		qApp->installEventFilter(this);
		label->setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, 0, this) / 255.0);
		label->setMouseTracking(true);
		fadingOut = false;

		effect = new QGraphicsDropShadowEffect;
		effect->setOffset(5);
		effect->setBlurRadius(20);
		label->setGraphicsEffect(effect);
		vlay->setContentsMargins(0, 0, effect->xOffset(), effect->yOffset());

		label->style()->unpolish(this);
		label->style()->polish(this);
	}

	reuseTip(text, msecDisplayTime);
}

void VipTipContainer::restartExpireTimer(int msecDisplayTime)
{
	if (fake)
		return;

	int time = 10000 + 40 * qMax(0, label->text().length() - 100);
	if (msecDisplayTime > 0) {
		time = msecDisplayTime;
	}
	if (label->expireTime() > 0) {
		time = label->expireTime();
	}
	expireTimer.start(time, this);
	hideTimer.stop();
}

void VipTipContainer::reuseTip(const QString& text, int msecDisplayTime)
{
	label->setWordWrap(Qt::mightBeRichText(text));
	label->setText(text);
	QFontMetrics fm(font());
	QSize extra(1, 0);
	// Make it look good with the default ToolTip font on Mac, which has a small descent.
	if (fm.descent() == 2 && fm.ascent() >= 11)
		++extra.rheight();
	QMargins m = layout()->contentsMargins();
	resize(label->sizeHint() + extra + QSize(m.left() + m.right(), m.top() + m.bottom()));
	label->resize(label->sizeHint());
	restartExpireTimer(msecDisplayTime);
}

void VipTipContainer::paintEvent(QPaintEvent* ev)
{
	QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	opt.init(this);
#else
	opt.initFrom(this);
#endif
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
	QWidget::paintEvent(ev);
}

void VipTipContainer::resizeEvent(QResizeEvent* e)
{
	QStyleHintReturnMask frameMask;
	QStyleOption option;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	option.init(this);
#else
	option.initFrom(this);
#endif
	if (style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask))
		setMask(frameMask.region);

	QWidget::resizeEvent(e);
}

void VipTipContainer::mouseMoveEvent(QMouseEvent* e)
{
	if (_instance != this)
		return;
	// forward to parent
	if (parentWidget()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		QPoint global = e->screenPos().toPoint();
#else
		QPoint global = e->globalPosition().toPoint();
#endif
		QPoint pos = parentWidget()->mapFromGlobal(global);
		QMouseEvent evt(e->type(), pos, global, e->button(), e->buttons(), e->modifiers());
		QCoreApplication::sendEvent(parent(), &evt);
	}

	if (_instance != this)
		return;

	if (rect.isNull())
		return;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QPoint pos = e->globalPos();
#else
	QPoint pos = e->globalPosition().toPoint();
#endif
	if (widget)
		pos = widget->mapFromGlobal(pos);
	if (!rect.contains(pos))
		hideTip();
	QWidget::mouseMoveEvent(e);
}

VipTipContainer::~VipTipContainer()
{
	_instance = 0;
}

VipTipContainer* VipTipContainer::instance()
{
	return _instance;
}

void VipTipContainer::hideTip()
{
	if (fake)
		return;

	if (!hideTimer.isActive())
		hideTimer.start(300, this);
}

void VipTipContainer::hideTipImmediately()
{
	if (fake)
		return;

	close(); // to trigger QEvent::Close which stops the animation
	deleteLater();
}

void VipTipContainer::setTipRect(QWidget* w, const QRect& r)
{
	if (!r.isNull() && !w)
		qWarning("VipCorrectedTip::setTipRect: Cannot pass null widget if rect is set");
	else {
		widget = w;
		rect = r;
	}
}

void VipTipContainer::timerEvent(QTimerEvent* e)
{
	if (fake)
		return;

	if (e->timerId() == hideTimer.timerId() || e->timerId() == expireTimer.timerId()) {
		hideTimer.stop();
		expireTimer.stop();
		hideTipImmediately();
	}
}

bool VipTipContainer::eventFilter(QObject* o, QEvent* e)
{
	switch (e->type()) {
#ifdef Q_DEAD_CODE_FROM_QT4_MAC
		case QEvent::KeyPress:
		case QEvent::KeyRelease: {
			int key = static_cast<QKeyEvent*>(e)->key();
			Qt::KeyboardModifiers mody = static_cast<QKeyEvent*>(e)->modifiers();
			if (!(mody & Qt::KeyboardModifierMask) && key != Qt::Key_Shift && key != Qt::Key_Control && key != Qt::Key_Alt && key != Qt::Key_Meta)
				hideTip();
			break;
		}
#endif
		case QEvent::Leave:
			hideTip();
			break;

#if defined(Q_OS_QNX) // On QNX the window activate and focus events are delayed and will appear
		      // after the window is shown.
		case QEvent::WindowActivate:
		case QEvent::FocusIn:
			return false;
		case QEvent::WindowDeactivate:
			if (o != this)
				return false;
			hideTipImmediately();
			break;
		case QEvent::FocusOut:
			if (reinterpret_cast<QWindow*>(o) != windowHandle())
				return false;
			hideTipImmediately();
			break;
#else
		case QEvent::WindowActivate:
		case QEvent::WindowDeactivate:
		case QEvent::FocusIn:
		case QEvent::FocusOut:
#endif
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::Wheel:
			hideTipImmediately();
			break;

		case QEvent::MouseMove:
			if (o == widget && !rect.isNull() && !rect.contains(static_cast<QMouseEvent*>(e)->pos()))
				hideTip();
		default:
			break;
	}
	return false;
}

int VipTipContainer::getTipScreen(const QPoint& pos, QWidget* w)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	if (QApplication::desktop()->isVirtualDesktop())
		return QApplication::desktop()->screenNumber(pos);
	else
		return QApplication::desktop()->screenNumber(w);
#else
	QScreen* screen = w ? w->screen() : nullptr;
	if (!screen)
		screen = QGuiApplication::screenAt(pos);
	if (screen)
		return qApp->screens().indexOf(screen);
	return 0;
#endif
}

QRect VipTipContainer::mapToScreen() const
{
	return QRect(this->mapToGlobal(label->pos()), label->geometry().size());
}

void VipTipContainer::placeTip(const QPoint& pos, QWidget* w)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	QRect screen = QApplication::desktop()->screenGeometry(getTipScreen(pos, w));
#else
	QRect screen = qApp->screens()[getTipScreen(pos, w)]->geometry();
#endif
	QPoint p = pos;
	p += QPoint(2,
#ifdef Q_DEAD_CODE_FROM_QT4_WIN
		    21
#else
		    16
#endif
	);
	if (p.x() + this->width() > screen.x() + screen.width())
		p.rx() -= 4 + this->width();
	if (p.y() + this->height() > screen.y() + screen.height())
		p.ry() -= 24 + this->height();
	if (p.y() < screen.y())
		p.setY(screen.y());
	if (p.x() + this->width() > screen.x() + screen.width())
		p.setX(screen.x() + screen.width() - this->width());
	if (p.x() < screen.x())
		p.setX(screen.x());
	if (p.y() + this->height() > screen.y() + screen.height())
		p.setY(screen.y() + screen.height() - this->height());
	this->move(p);
}

bool VipTipContainer::tipChanged(const QPoint& pos, const QString& text, QObject* o)
{
	if (fake)
		return false;

	if (_instance->label->text() != text)
		return true;

	if (o != widget)
		return true;

	if (!rect.isNull())
		return !rect.contains(pos);
	else
		return false;
}

/// Shows \a text as a tool tip, with the global position \a pos as
/// the point of interest. The tool tip will be shown with a platform
/// specific offset from this point of interest.
///
/// If you specify a non-empty rect the tip will be hidden as soon
/// as you move your cursor out of this area.
///
/// The \a rect is in the coordinates of the widget you specify with
/// \a w. If the \a rect is not empty you must specify a widget.
/// Otherwise this argument can be 0 but it is used to determine the
/// appropriate screen on multi-head systems.
///
/// If \a text is empty the tool tip is hidden. If the text is the
/// same as the currently shown tooltip, the tip will \e not move.
/// You can force moving by first hiding the tip with an empty text,
/// and then showing the new tip at the new position.

QRect VipCorrectedTip::showText(const QPoint& pos, const QString& text, QWidget* w, const QRect& rect)
{
	return showText(pos, text, w, rect, -1);
}

VipTipContainer* VipCorrectedTip::hiddenTip()
{
	static VipTipContainer* inst = new VipTipContainer(QString(), nullptr, 0, true);
	if (inst->isVisible())
		inst->close();
	return inst;
}

QRect VipCorrectedTip::textGeometry(const QPoint& pos, const QString& text, QWidget* w, const QRect& rect)
{
	if (text.isEmpty())
		return QRect();

	hiddenTip()->reuseTip(text, 0);
	hiddenTip()->setTipRect(w, rect);
	hiddenTip()->placeTip(pos, w);
	QRect r = hiddenTip()->mapToScreen();
	return r;
}

/// \since 5.2
/// \overload
/// This is similar to VipCorrectedTip::showText(\a pos, \a text, \a w, \a rect) but with an extra parameter \a msecDisplayTime
/// that specifies how long the tool tip will be displayed, in milliseconds.

QRect VipCorrectedTip::showText(const QPoint& pos, const QString& text, QWidget* w, const QRect& rect, int msecDisplayTime)
{
	static bool first = true;
	if (first) {
		setFont(QToolTip::font());
		setPalette(QToolTip::palette());
		first = false;
	}

	if (_instance && _instance->isVisible() && (!w || w == _instance->parentWidget())) { // a tip does already exist
		if (text.isEmpty()) {							     // empty text means hide current tip
			_instance->hideTip();
			return _instance->mapToScreen();
		}
		else if (!_instance->fadingOut) {
			// If the tip has changed, reuse the one
			// that is showing (removes flickering)
			QPoint localPos = pos;
			if (w)
				localPos = w->mapFromGlobal(pos);
			if (_instance->tipChanged(localPos, text, w)) {
				_instance->reuseTip(text, msecDisplayTime);
				_instance->setTipRect(w, rect);
				_instance->placeTip(pos, w);
			}

			return _instance->mapToScreen();
		}
	}

	if (!text.isEmpty()) { // no tip can be reused, create new tip:
#ifndef Q_DEAD_CODE_FROM_QT4_WIN
		new VipTipContainer(text, w, msecDisplayTime); // sets VipTipContainer::instance to itself
#else
		// On windows, we can't use the widget as parent otherwise the window will be
		// raised when the tooltip will be shown
		new VipTipContainer(text, QApplication::desktop()->screen(VipTipContainer::getTipScreen(pos, w)), msecDisplayTime);
#endif
		_instance->setTipRect(w, rect);
		_instance->placeTip(pos, w);
		_instance->setObjectName(QLatin1String("qtooltip_label"));

#if !defined(QT_NO_EFFECTS) && !defined(Q_DEAD_CODE_FROM_QT4_MAC)
		if (QApplication::isEffectEnabled(Qt::UI_FadeTooltip))
			qFadeEffect(_instance);
		else if (QApplication::isEffectEnabled(Qt::UI_AnimateTooltip))
			qScrollEffect(_instance);
		else
			_instance->showNormal();
#else
		_instance->show();
#endif
	}

	if (_instance)
		return _instance->mapToScreen();
	return QRect();
}

/// \overload
///
/// This is analogous to calling VipCorrectedTip::showText(\a pos, \a text, \a w, QRect())

QRect VipCorrectedTip::showText(const QPoint& pos, const QString& text, QWidget* w)
{
	return VipCorrectedTip::showText(pos, text, w, QRect());
}

/// \fn void VipCorrectedTip::hideText()
/// \since 4.2
///
/// Hides the tool tip. This is the same as calling showText() with an
/// empty string.
///
/// \sa showText()

/// \since 4.4
///
/// Returns \c true if this tooltip is currently shown.
///
/// \sa showText()
bool VipCorrectedTip::isVisible()
{
	return (_instance != 0 && _instance->isVisible());
}

/// \since 4.4
///
/// Returns the tooltip text, if a tooltip is visible, or an
/// empty string if a tooltip is not visible.
QString VipCorrectedTip::text()
{
	if (_instance)
		return _instance->label->text();
	return QString();
}

Q_GLOBAL_STATIC(QPalette, tooltip_palette)

/// Returns the palette used to render tooltips.
///
/// \note Tool tips use the inactive color group of QPalette, because tool
/// tips are not active windows.
QPalette VipCorrectedTip::palette()
{
	return *tooltip_palette();
}

/// \since 4.2
///
/// Returns the font used to render tooltips.
QFont VipCorrectedTip::font()
{
	return QApplication::font("VipTipLabel");
}

/// \since 4.2
///
/// Sets the \a palette used to render tooltips.
///
/// \note Tool tips use the inactive color group of QPalette, because tool
/// tips are not active windows.
void VipCorrectedTip::setPalette(const QPalette& palette)
{
	*tooltip_palette() = palette;
	if (_instance)
		_instance->label->setPalette(palette);
}

/// \since 4.2
///
/// Sets the \a font used to render tooltips.
void VipCorrectedTip::setFont(const QFont& font)
{
	QApplication::setFont(font, "VipTipLabel");
}
