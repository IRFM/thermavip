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

#ifndef VIP_P_CUSTOM_INTERFACE_H
#define VIP_P_CUSTOM_INTERFACE_H

#include "VipPlayer.h"

class VipDragWidget;

struct Anchor
{
	Vip::Side side;
	VipPlotCanvas* canvas;
	QRect highlight;
	QString text;
	Anchor()
	  : side(Vip::NoSide)
	  , canvas(nullptr)
	{
	}
};

/// @brief Small tool bar to navigate through maximized players
class NavigatePlayers : public QToolBar
{
	Q_OBJECT
public:
	NavigatePlayers(VipDragWidget* p);
	~NavigatePlayers();

	VipDragWidget* parentDragWidget() const;
	VipDragWidget* next() const;
	VipDragWidget* prev() const;

protected:
	virtual bool eventFilter(QObject*, QEvent* evt);

private Q_SLOTS:
	void goNext();
	void goPrev();
	void visibilityChanged();
	void updatePos();

private:
	class PrivateData;
	PrivateData* m_data;
};

class BaseCustomPlayer : public QObject
{
	Q_OBJECT
public:
	BaseCustomPlayer(VipAbstractPlayer* pl)
	  : QObject(pl)
	{
		if (pl->plotWidget2D())
			connect(pl->plotWidget2D(), SIGNAL(viewportChanged(QWidget*)), this, SLOT(updateViewport(QWidget*)));
	}
	virtual VipDragWidget* dragWidget() const = 0;

public Q_SLOTS:
	virtual void closePlayer();
	virtual void maximizePlayer();
	virtual void minimizePlayer();
	virtual void updateViewport(QWidget* viewport) {}
};

class CustomWidgetPlayer : public BaseCustomPlayer
{
	Q_OBJECT
public:
	CustomWidgetPlayer(VipWidgetPlayer* player);
	~CustomWidgetPlayer();

	virtual bool eventFilter(QObject* w, QEvent* evt);
	virtual VipDragWidget* dragWidget() const;
private Q_SLOTS:
	void reorganizeCloseButton();

private:
	Anchor anchor(const QPoint& viewport_pos, const QMimeData* mime);

	class PrivateData;
	PrivateData* m_data;
};

class BaseCustomPlayer2D : public BaseCustomPlayer
{
	Q_OBJECT
public:
	BaseCustomPlayer2D(VipPlayer2D* player)
	  : BaseCustomPlayer(player)
	{
	}

	VipPlayer2D* player() const { return qobject_cast<VipPlayer2D*>(parent()); }
	QPointF scenePos(const QPoint& viewport_pos) const;
	QGraphicsItem* firstVisibleItem(const QPointF& scene_pos) const;

public Q_SLOTS:
	virtual void unselectAll();
	virtual void updateTitle();
	virtual void editTitle();
};

class CustomizeVideoPlayer : public BaseCustomPlayer2D
{
	Q_OBJECT
public:
	CustomizeVideoPlayer(VipVideoPlayer* player);
	~CustomizeVideoPlayer();
	virtual bool eventFilter(QObject* w, QEvent* evt);
	virtual VipDragWidget* dragWidget() const;
	virtual void updateViewport(QWidget* viewport);

public:
	QToolButton* maximizeButton() const;
	QToolButton* minimizeButton() const;
	QToolButton* closeButton() const;

private Q_SLOTS:
	void endRender();
	void reorganizeCloseButton();
	void addToolBarWidget(QWidget* w);

private:
	Anchor anchor(const QPoint& viewport_pos, const QMimeData* mime);
	class PrivateData;
	PrivateData* m_data;
};

class CustomizePlotPlayer : public BaseCustomPlayer2D
{
	Q_OBJECT
public:
	CustomizePlotPlayer(VipPlotPlayer* player);
	~CustomizePlotPlayer();

	virtual bool eventFilter(QObject* w, QEvent* evt);
	virtual VipDragWidget* dragWidget() const;
	virtual void updateViewport(QWidget* viewport);
private Q_SLOTS:
	void titleChanged();
	void finishEditingTitle();
	void reorganizeCloseButtons();
	void closeCanvas();

private:
	Anchor anchor(const QPoint& viewport_pos, const QMimeData* mime);

	class PrivateData;
	PrivateData* m_data;
};

#endif