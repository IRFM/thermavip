#ifndef VIP_P_CUSTOM_INTERFACE_H
#define VIP_P_CUSTOM_INTERFACE_H


#include "VipPlayer.h"

class VipDragWidget;

struct Anchor {
	Vip::Side side;
	VipPlotCanvas * canvas;
	QRect highlight;
	QString text;
	Anchor()
		:side(Vip::NoSide), canvas(nullptr)
	{}
};

class BaseCustomPlayer : public QObject
{
	Q_OBJECT
public:
	BaseCustomPlayer(VipAbstractPlayer* pl)
		:QObject(pl)
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
	BaseCustomPlayer2D(VipPlayer2D * player)
		:BaseCustomPlayer(player)
	{}

	VipPlayer2D * player() const {
		return qobject_cast<VipPlayer2D*>(parent());
	}
	QPointF scenePos(const QPoint & viewport_pos) const;
	QGraphicsItem * firstVisibleItem(const QPointF & scene_pos) const;
	
public Q_SLOTS:
	virtual void unselectAll();
	virtual void updateTitle();
	virtual void editTitle();
};

class CustomizeVideoPlayer : public BaseCustomPlayer2D
{
	Q_OBJECT
public:
	CustomizeVideoPlayer(VipVideoPlayer * player);
	~CustomizeVideoPlayer();
	virtual bool eventFilter(QObject * w, QEvent * evt);
	virtual VipDragWidget * dragWidget() const;
	virtual void updateViewport(QWidget* viewport);

public:
	QToolButton * maximizeButton() const;
	QToolButton * minimizeButton() const;
	QToolButton * closeButton() const;

private Q_SLOTS:
	void endRender();
	void reorganizeCloseButton();
	void addToolBarWidget(QWidget * w);
private:
	Anchor anchor(const QPoint & viewport_pos, const QMimeData * mime);
	class PrivateData;
	PrivateData * m_data;
};

class CustomizePlotPlayer : public BaseCustomPlayer2D
{
	Q_OBJECT
public:
	CustomizePlotPlayer(VipPlotPlayer * player);
	~CustomizePlotPlayer();

	virtual bool eventFilter(QObject * w, QEvent * evt);
	virtual VipDragWidget * dragWidget() const;
	virtual void updateViewport(QWidget* viewport);
private Q_SLOTS:
	void titleChanged();
	void finishEditingTitle();
	void reorganizeCloseButtons();
	void closeCanvas();
private:
	Anchor anchor(const QPoint & viewport_pos, const QMimeData * mime);

	class PrivateData;
	PrivateData * m_data;
};

#endif