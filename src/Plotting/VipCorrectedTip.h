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

#ifndef VIP_CORRECTED_TIP_H
#define VIP_CORRECTED_TIP_H

#include "VipConfig.h"
#include <qbasictimer.h>
#include <qelapsedtimer.h>
#include <qlabel.h>
#include <qpointer.h>
#include <qtimer.h>
#include <qvariant.h>
#include <qwidget.h>

struct QEffects
{
	enum Direction
	{
		LeftScroll = 0x0001,
		RightScroll = 0x0002,
		UpScroll = 0x0004,
		DownScroll = 0x0008
	};

	typedef uint DirFlags;
};

// Internal class QAlphaWidget.
//
// The QAlphaWidget is shown while the animation lasts
// and displays the pixmap resulting from the alpha blending.

class QAlphaWidget
  : public QWidget
  , private QEffects
{
	Q_OBJECT
public:
	QAlphaWidget(QWidget* w, Qt::WindowFlags f = Qt::WindowFlags());
	~QAlphaWidget();

	void run(int time);

protected:
	void paintEvent(QPaintEvent* e) Q_DECL_OVERRIDE;
	void closeEvent(QCloseEvent*) Q_DECL_OVERRIDE;
	void alphaBlend();
	bool eventFilter(QObject*, QEvent*) Q_DECL_OVERRIDE;

protected slots:
	void render();

private:
	QPixmap pm;
	double alpha;
	QImage backImage;
	QImage frontImage;
	QImage mixedImage;
	QPointer<QWidget> widget;
	int duration;
	int elapsed;
	bool showWidget;
	QTimer anim;
	QElapsedTimer checkTime;
};

// Internal class QRollEffect
//
// The QRollEffect widget is shown while the animation lasts
// and displays a scrolling pixmap.

class QRollEffect
  : public QWidget
  , private QEffects
{
	Q_OBJECT
public:
	QRollEffect(QWidget* w, Qt::WindowFlags f, DirFlags orient);

	void run(int time);

protected:
	void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
	void closeEvent(QCloseEvent*) Q_DECL_OVERRIDE;

private slots:
	void scroll();

private:
	QPointer<QWidget> widget;

	int currentHeight;
	int currentWidth;
	int totalHeight;
	int totalWidth;

	int duration;
	int elapsed;
	bool done;
	bool showWidget;
	int orientation;

	QTimer anim;
	QElapsedTimer checkTime;

	QPixmap pm;
};

class QGraphicsDropShadowEffect;
class VipTipContainer;

/// Actual tool tip widget used within Plotting library
class VIP_PLOTTING_EXPORT VipTipLabel : public QLabel
{
	Q_OBJECT
	Q_PROPERTY(bool dropShadowEnabled READ dropShadowEnabled WRITE setDropShadowEnabled)
	Q_PROPERTY(double dropShadowOffset READ dropShadowOffset WRITE setdropShadowOffset)
	Q_PROPERTY(double dropShadowBlurRadius READ dropShadowBlurRadius WRITE setDropShadowBlurRadius)
	Q_PROPERTY(int expireTime READ expireTime WRITE setExpireTime)

	friend class VipTipContainer;
	VipTipLabel(VipTipContainer* parent = nullptr);

public:
	bool dropShadowEnabled() const;
	double dropShadowOffset() const;
	double dropShadowBlurRadius() const;
	int expireTime() const;

	virtual bool event(QEvent* evt);

public Q_SLOTS:

	void setDropShadowEnabled(bool);
	void setdropShadowOffset(double);
	void setDropShadowBlurRadius(double);
	void setExpireTime(int);

private:
	VipTipContainer* container;
	int expireTimeMs;
};

/// Tool tip container (singleton) used within Plotting library
class VIP_PLOTTING_EXPORT VipTipContainer : public QWidget
{
	Q_OBJECT
	friend class VipCorrectedTip;
	friend class VipTipLabel;

	VipTipContainer(const QString& text, QWidget* w, int msecDisplayTime, bool is_fake = false);

public:
	~VipTipContainer();
	static VipTipContainer* instance();

	bool eventFilter(QObject*, QEvent*) Q_DECL_OVERRIDE;

	QBasicTimer hideTimer, expireTimer;

	void reuseTip(const QString& text, int msecDisplayTime);
	void hideTip();
	void hideTipImmediately();
	void setTipRect(QWidget* w, const QRect& r);
	void restartExpireTimer(int msecDisplayTime);
	bool tipChanged(const QPoint& pos, const QString& text, QObject* o);
	void placeTip(const QPoint& pos, QWidget* w);
	QRect mapToScreen() const;

	static int getTipScreen(const QPoint& pos, QWidget* w);

protected:
	void timerEvent(QTimerEvent* e) Q_DECL_OVERRIDE;
	void paintEvent(QPaintEvent* e) Q_DECL_OVERRIDE;
	void mouseMoveEvent(QMouseEvent* e) Q_DECL_OVERRIDE;
	void resizeEvent(QResizeEvent* e) Q_DECL_OVERRIDE;

private:
	QGraphicsDropShadowEffect* effect;
	QWidget* styleSheetParent;
	VipTipLabel* label;
	bool fadingOut;
	bool fake;
	QPointer<QWidget> widget;
	QRect rect;
};

/// Replacement for QToolTip, correct timer issues.
/// this class should be used instead of QToolTip to display information over curves/plot items.
class VIP_PLOTTING_EXPORT VipCorrectedTip
{
	VipCorrectedTip() Q_DECL_EQ_DELETE;
	static VipTipContainer* hiddenTip();

public:
	static QRect showText(const QPoint& pos, const QString& text, QWidget* w = Q_NULLPTR);
	static QRect showText(const QPoint& pos, const QString& text, QWidget* w, const QRect& rect);
	static QRect showText(const QPoint& pos, const QString& text, QWidget* w, const QRect& rect, int msecShowTime);
	static inline void hideText() { showText(QPoint(), QString()); }
	static QRect textGeometry(const QPoint& pos, const QString& text, QWidget* w, const QRect& rect);

	static bool isVisible();
	static QString text();

	static QPalette palette();
	static void setPalette(const QPalette&);
	static QFont font();
	static void setFont(const QFont&);
};

#endif // QTOOLTIP_H
