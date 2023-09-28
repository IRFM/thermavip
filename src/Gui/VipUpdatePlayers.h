#pragma once

#include "VipPlayer.h"

/**
VipUpdateVideoPlayer is used to modify a VipVideoPlayer's content by adding new button for basic image processing.
The buttons are automatically shown if the player displays a valid image.

VipUpdateVideoPlayer add the following controls:
- Vertical reflection
- Horizontal reflection
- Rotate 90 left
- Rotate 90 right
- Rotate 180
- VipImageCrop on area
- Display local minimum/maximum

The first 6 processings rely on the (possibly not existing) source VipProcessingList of this player.
The VipImageCrop also relies on specific VipPlotAreaFilter (VipDrawCropArea) to draw the cropping area.

Local minimum/maximum are displayed for the whole image or for selected shapes using VipPlotMarker class.

VipUpdateVideoPlayer is a good example on how to modify a VipVideoPlayer through a foreign class.
*/
class VipUpdateVideoPlayer : public QObject
{
	Q_OBJECT

public:
	VipUpdateVideoPlayer(VipVideoPlayer * player);

	static int registerClass();

public Q_SLOTS:
	void rotate90Left();
	void rotate90Right();
	void rotate180();
	void mirrorVertical();
	void mirrorHorizontal();
	void removeLastCrop();
	void removeAllCrops();
	void setMarkersEnabled(bool);
	void setDisplayMarkerPos(bool);
	void saveROIInfos();
private Q_SLOTS:
	void newPlayerImage();
	void cropStarted();
	void cropEnded();
	void cropAdded(const QPointF & start, const QPointF & end);
	void updateMarkers();
	
private:
	QPointer<VipVideoPlayer> m_player;
	QToolBar * m_toolBar;
	QToolButton * m_crop;
	QToolButton * m_local_minmax;
	QAction * m_toolBarAction;
	QAction * m_minmaxPos;

	QList<VipPlotMarker*> m_minMarkers;
	QList<VipPlotMarker*> m_maxMarkers;
	bool m_displayMarkerPos;
	VipNDArray m_buffer;
};


/**
A VipPlotAreaFilter used to draw a cropping region
*/
class VipDrawCropArea : public VipPlotAreaFilter
{
	Q_OBJECT
public:
	QPointF begin;
	QPointF end;
	QCursor cursor;

	VipDrawCropArea(VipAbstractPlotArea * area);
	~VipDrawCropArea();
	virtual QRectF  boundingRect() const;
	virtual QPainterPath shape() const;
	virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
	virtual bool sceneEvent(QEvent * event);

	/**
	Filter QApplication event to detect mouse event.
	If a mouse press event is detected outside the QGraphicsView of the VipVideoPlayer, this filter is automatically destroyed.
	*/
	virtual bool	eventFilter(QObject *watched, QEvent *event);

Q_SIGNALS:
	/**
	Signal emitted when a valid cropping area has been drawn.
	This will automatically destriy this VipDrawCropArea object.
	*/
	void cropCreated(const QPointF & start, const QPointF & end);
};

 

#include <qtimer.h>

/// @brief Add additional controls to VipPlotPlayer instance.
///
/// Like VipUpdateVideoPlayer, VipUpdatePlotPlayer add
/// additional graphical options to a VipVideoPlayer:
/// -	Display minimum/maximum marker over 2d curves
/// -	Possibility to compute the distance between 2 points of a curve
/// 
class VipUpdatePlotPlayer : public QObject
{
	Q_OBJECT

public:
	VipUpdatePlotPlayer(VipPlotPlayer * player);
	~VipUpdatePlotPlayer();
	static int registerClass();
public Q_SLOTS:
	void startDistance(bool start);
	void endDistance();
	void distanceCreated(const VipPoint & start, const VipPoint & end);

	void setMarkersEnabled(bool);
	void setDisplayMarkerPos(bool);

private Q_SLOTS:
	void updateMarkers();
	void stopMarkers(VipAbstractPlayer*);
private:

	QPointer<VipPlotPlayer> m_player;
	QAction * m_drawDist;
	QMap<VipPlotItem*,QList<VipPlotMarker*> > m_minMarkers;
	QMap<VipPlotItem*, QList<VipPlotMarker*> > m_maxMarkers;
	QToolButton * m_local_minmax;
	QAction * m_minmaxPos;
	QTimer m_updateTimer;
};

/**
A VipPlotAreaFilter used to draw a line between 2 points
*/
class VipDrawDistance2Points : public VipPlotAreaFilter
{
	Q_OBJECT
public:
	VipPoint begin;
	VipPoint end;
	QCursor cursor;
	VipPlotItem * startItem;
	VipPlotItem * endItem;
	VipPlotItem * hover;
	QPointF hoverPt;

	QLineF computeLine() const;

	VipDrawDistance2Points(VipAbstractPlotArea * area);
	~VipDrawDistance2Points();
	virtual QRectF  boundingRect() const;
	virtual QPainterPath shape() const;
	virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
	virtual bool sceneEvent(QEvent * event);

	/**
	Filter QApplication event to detect mouse event.
	If a mouse press event is detected outside the QGraphicsView of the VipVideoPlayer, this filter is automatically destroyed.
	*/
	virtual bool	eventFilter(QObject *watched, QEvent *event);

Q_SIGNALS:
	/**
	Signal emitted when a valid cropping area has been drawn.
	This will automatically destriy this VipDrawCropArea object.
	*/
	void distanceCreated(const VipPoint & start, const VipPoint & end);
};