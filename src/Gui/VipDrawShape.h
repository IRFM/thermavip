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

#ifndef DRAW_SHAPE_H
#define DRAW_SHAPE_H

#include <QMenu>
#include <QToolButton>

#include "VipMultiNDArray.h"
#include "VipPlayer.h"
#include "VipPlotShape.h"
#include "VipPlotWidget2D.h"
#include "VipSceneModel.h"
#include "VipToolWidget.h"

/// \addtogroup Gui
///  @{

/// \class VipDrawGraphicsShape
/// \brief Base class used to draw graphics shapes on a VipPlotArea2D.
class VIP_GUI_EXPORT VipDrawGraphicsShape : public VipPlotAreaFilter
{
	Q_OBJECT

	QString m_group;
	QPointer<VipPlotSceneModel> m_scene_model;
	QPointer<VipPlotPlayer> m_player;
	VipShape m_shape;

public:
	/// Construct from a shape group
	VipDrawGraphicsShape(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawGraphicsShape(VipPlotPlayer* player, const QString& group = QString());

	void reset(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	void reset(VipPlotPlayer* player, const QString& group = QString());
	void resetPlayer(VipPlayer2D* player, const QString& group = QString());

	/// Returns the last drawn shape.
	VipShape lastShape() const;

	VipPlotSceneModel* plotSceneModel() const;

	QList<VipAbstractScale*> sceneModelScales() const;

	/// Returns the shape group.
	QString group() const;

	/// Set the drawn shape group.
	void setGroup(const QString& group);

	virtual QRectF boundingRect() const { return shape().boundingRect(); }

protected:
	/// If the filter is initialized with a player and NOT a VipPlotSceneModel, try to find the plot scene model that contains given scene position.
	/// If found, set the internal VipPlotSceneModel to this one.
	void findPlotSceneModel(const QPointF& scene_pos);

	/// Set the last drawn shape. If isAutoAddShape() returns true, the shape will be added to the scene model.
	void setLastShape(const VipShape& sh);
};

class VIP_GUI_EXPORT VipDrawShapeRect : public VipDrawGraphicsShape
{
public:
	VipDrawShapeRect(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawShapeRect(VipPlotPlayer* player, const QString& group = QString());
	~VipDrawShapeRect();

	virtual QRectF boundingRect() const;
	virtual QPainterPath shape() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool sceneEvent(QEvent* event);
	virtual VipShape createShape();

protected:
	QPointF m_begin;
	QPointF m_end;
};

class VIP_GUI_EXPORT VipDrawShapeEllipse : public VipDrawShapeRect
{
public:
	VipDrawShapeEllipse(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawShapeEllipse(VipPlotPlayer* player, const QString& group = QString());

	virtual QPainterPath shape() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual VipShape createShape();
};

class VIP_GUI_EXPORT VipDrawShapePoint : public VipDrawGraphicsShape
{
public:
	VipDrawShapePoint(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawShapePoint(VipPlotPlayer* player, const QString& group = QString());

	virtual QPainterPath shape() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool sceneEvent(QEvent* event);
};

/// \class VipDrawShapePolygon
/// \brief Event filter used to draw a polygon on a SceneModel2D
class VIP_GUI_EXPORT VipDrawShapePolygon : public VipDrawGraphicsShape
{
public:
	VipDrawShapePolygon(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawShapePolygon(VipPlotPlayer* player, const QString& group = QString());
	~VipDrawShapePolygon();
	virtual QPainterPath shape() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool sceneEvent(QEvent* event);
	virtual VipShape createShape(const QPolygonF& poly);

	virtual bool eventFilter(QObject*, QEvent*);

protected:
	bool stopOnKeyPress();

	QPolygonF m_polygon;
	QPointF m_pos;
	QPointer<VipAbstractPlotArea> m_area;
};

class VIP_GUI_EXPORT VipDrawShapePolyline : public VipDrawShapePolygon
{
public:
	VipDrawShapePolyline(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawShapePolyline(VipPlotPlayer* player, const QString& group = QString());

	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual VipShape createShape(const QPolygonF& poly);
	bool sceneEvent(QEvent* event);
};

class VIP_GUI_EXPORT VipDrawShapeMask : public VipDrawGraphicsShape
{
public:
	VipDrawShapeMask(VipPlotSceneModel* plotSceneModel, const QString& group = QString());
	VipDrawShapeMask(VipPlotPlayer* player, const QString& group = QString());

	virtual QPainterPath shape() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual bool sceneEvent(QEvent* event);
	virtual VipShape createShape(const QPolygonF& poly);

protected:
	QPolygonF m_polygon;
};

/// Extract statistical informations from a list of VipShape and a 2D player.
/// Signature: QString (VipPlayer2D * , const VipShapeList&)
/// It can be used to display, for instance, the minimum/maximum/mean pixel values inside Regions Of Intersets in an image.
/// The text result can have a HTML formatting.
VIP_GUI_EXPORT VipFunctionDispatcher<2>& vipFDShapeStatistics();

class VIP_GUI_EXPORT ShowHideGroups : public QWidget
{
	Q_OBJECT

public:
	ShowHideGroups(QWidget* parent = nullptr);
	~ShowHideGroups();

	void computeGroups(const QList<VipPlotSceneModel*>& models);

	QStringList availableGroups() const;
	QStringList visibleGroups() const;
	QStringList hiddenGroups() const;
	// QString currentGroup() const;

public Q_SLOTS:
	void showAll();
	void hideAll();
	// void setCurrentGroup(const QString & group);

Q_SIGNALS:
	void changed();

private Q_SLOTS:
	void checked();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipPlayer2D;
class VIP_GUI_EXPORT VipSceneModelEditor : public QWidget
{
	Q_OBJECT

public:
	VipSceneModelEditor(QWidget* parent = nullptr);
	~VipSceneModelEditor();

	void setPlayer(VipPlayer2D*);
	void updateSceneModels();
	VipMultiNDArray createH5ShapeAttributes(const QVariant& background);
	VipShapeList openShapes(const QString& filename, VipPlayer2D* pl, bool remove_old = true);
	VipPlotSceneModel* lastSelected() const;
	QGraphicsScene* scene() const;
	VipPlayer2D* player() const;

Q_SIGNALS:
	void sceneModelChanged();

private Q_SLOTS:
	void emitSceneModelChanged();
	void selectionChanged(VipPlotItem* item);
	// void findSelectedSceneModel();
	// QString updateGroupList();
	void resetPlayer();
	void uniteShapes();
	void intersectShapes();
	void subtractShapes();
	void addProperty();
	void removeProperty();

	void saveShapes();
	void saveShapesAttribute();
	void saveShapesImage();
	void openShapes();
	void saveH5ShapesAttribute();

	void selectUnselectAll();
	void deleteSelected();

	void recomputeAttributes();

private:
	// void populateStatistics(VipPlayer2D *, const VipShapeList & shapes);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VIP_GUI_EXPORT VipShapeButton : public QToolButton
{
	Q_OBJECT

public:
	VipShapeButton(QWidget* draw_area, QWidget* parent = nullptr);
	~VipShapeButton();

	bool eventFilter(QObject* watched, QEvent* event);

protected Q_SLOTS:
	void started();
	void finished();
	void buttonClicked(bool);

protected:
	QWidget* m_draw_area;
};

class VipSceneModelWidgetPlayer;
class VIP_GUI_EXPORT ShapeToolBar : public VipToolWidgetToolBar
{
public:
	QToolButton* addShape;
	QMenu* addMenu;
	ShapeToolBar(VipSceneModelWidgetPlayer* tool);
};

class VIP_GUI_EXPORT VipSceneModelWidgetPlayer : public VipToolWidgetPlayer
{
	Q_OBJECT

public:
	VipSceneModelWidgetPlayer(VipMainWindow*);
	virtual bool setPlayer(VipAbstractPlayer*);

	virtual ShapeToolBar* toolBar();

	VipSceneModelEditor* editor() const { return m_editor; }
	/// Returns a button that can be embeded in a video player to draw ROI as shortcut
	VipShapeButton* createPlayerButton(VipAbstractPlayer*);

public Q_SLOTS:
	void resetPlayer();

private Q_SLOTS:
	void addRect();
	void addEllipse();
	void addPolygon();
	void addPolyline();
	void addMask();
	void addPixel();
	void stopAddingShape();
	void addShapeClicked(bool);

Q_SIGNALS:
	void stopShape();

protected:
	virtual void keyPressEvent(QKeyEvent* evt);

private:
	VipSceneModelEditor* m_editor;
	QToolButton* m_addShape;
	QMenu* m_addMenu;
	ShapeToolBar* m_toolBar;
	QPointer<VipPlayer2D> m_player;
	QPointer<VipAbstractPlotArea> m_area;
	QPointer<VipDrawGraphicsShape> m_draw;

	typedef void (VipSceneModelWidgetPlayer::*shapeFun)();
	shapeFun m_last_shape;
};

/// Returns the global ROI editor tool widget
VIP_GUI_EXPORT VipSceneModelWidgetPlayer* vipGetSceneModelWidgetPlayer(VipMainWindow* window = nullptr);

/// Manage the scene model states on all players.
/// This class is used to implement a undo/redo system on the edition of Regions of Intereset.
class VIP_GUI_EXPORT VipSceneModelState : public QObject
{
	Q_OBJECT
	VipSceneModelState();

public:
	~VipSceneModelState();
	static VipSceneModelState* instance();

	void connectSceneModel(VipPlotSceneModel*);
	void disconnectSceneModel(VipPlotSceneModel*);

	/// Save the state of a VipPlotSceneModel as a QByteArray (includes scene model and selection status)
	QByteArray saveState(VipPlotSceneModel* sm);
	/// Restore the state of a VipPlotSceneModel
	bool restoreState(VipPlotSceneModel* psm, const QByteArray& ar);

	/// Undo the last action on given player and scene model
	void undo(VipPlayer2D* player, VipPlotSceneModel* sm);
	/// Undo the last undone action on given player and scene model
	void redo(VipPlayer2D* player, VipPlotSceneModel* sm);

	void pushState(VipPlayer2D* player, VipPlotSceneModel* sm, const QByteArray& state = QByteArray());

	QPair<VipPlayer2D*, VipPlotSceneModel*> currentSceneModel() const;

public Q_SLOTS:
	/// Undo last action on current player and scene model
	void undo();
	/// Redo last undone action on current player and scene model
	void redo();

Q_SIGNALS:
	void undoDone();
	void redoDone();

private Q_SLOTS:
	void receivedAboutToChange();

private:
	void cleanStates();
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
/// end Gui

#endif
