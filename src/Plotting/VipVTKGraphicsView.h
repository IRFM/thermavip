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

#ifndef VIP_VTK_GRAPHICS_VIEW_H
#define VIP_VTK_GRAPHICS_VIEW_H


#include <QGraphicsView>
#include <QResizeEvent>
#include <QGraphicsWidget>
#include <QGraphicsLinearLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QSizePolicy>
#include <QToolBar>
#include <QLabel>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkLookupTable.h>
#include <vtkScalarBarActor.h>
#include <vtkProperty2D.h>
#include <vtkCubeAxesActor.h>
//#include <vtkGridAxes3DActor.h>
#include <vtkOrientationMarkerWidget.h>

#include "VipVTKWidget.h"
#include "p_VTKOffscreenExtractContour.h"
#include "VipDisplayVTKObject.h"

#include "VipPlotWidget2D.h"


class VipPlotVTKObject;
class VipVTKImage;
class VTK3DPlayer;
class VipVTKGraphicsView;
class OffscreenExtractShapeStatistics;

/// @brief Semi transparent information widget displayed on top of a VipVTKGraphicsView
///
class VIP_PLOTTING_EXPORT VipInfoWidget : public QLabel
{
	Q_OBJECT

	VipVTKGraphicsView * view;
	QPoint last;
	QString lastDescription;

public:
	VipInfoWidget(VipVTKGraphicsView * view);

public Q_SLOTS:
	void updateDisplayInfo(const QPoint & pt = QPoint(-1,-1));

protected:
	virtual void wheelEvent(QWheelEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
};


/// @brief A VipImageWidget2D used to display VTK 3D objects.
/// 
/// VipVTKGraphicsView is a VipImageWidget2D used to display 3D objects 
/// based on VTK library. Its internal viewport is a VipVTKWidget object.
/// 
/// The viewport can be manipulated in a pure VTK way based on
/// the vtkRenderer(s) and vtkRenderWindow (see VipVTKGraphicsView::renderer()
/// and VipVTKGraphicsView::renderWindow()).
/// 
/// VipVTKGraphicsView also provides convenient ways to render VipVTKObject objects:
/// -	3D objects are added automatically through VipPlotVTKObject::setAxes()
/// -	An OffscreenExtractContour object will automatically extract the shape of
///		each VipVTKObject to handle mouse selection, display a contour polygon,
///		and display information on cells/points under the mouse.
/// -	Each VipVTKObject is associated to a VipPlotVTKObject. This allow VipVTKGraphicsView
///		to behave like any plotting widget in the thermavip SDK, and works with the
///		processing pipeline features.
/// -	A VipVTKGraphicsView can render both 3D VTK based scenes as well as 2D plotting
///		based on thermavip Plotting library.
/// 
/// VipVTKGraphicsView is the plotting widget for VTK3DPlayer class.
/// 
class VIP_PLOTTING_EXPORT VipVTKGraphicsView : public VipImageWidget2D
{
	Q_OBJECT
	friend class VipInfoWidget;

  public:

    VipVTKGraphicsView();
    ~VipVTKGraphicsView();

    /// @brief Returns the viewport
    VipVTKWidget * widget() const;
    /// @brief Returns the lookup table used to render point/cell attributes
    vtkLookupTable * table() const;
	/// @brief Returns the vtkCubeAxesActor
	vtkCubeAxesActor* cubeAxesActor() const;
    /// @brief Returns the scalar bar used to encapsulate the lookup table
    vtkScalarBarActor * scalarBar() const;
	/// @brief Returns the annotation legend used to display fields attributes
	VipBorderLegend * annotationLegend() const;
	/// @brief Returns the OffscreenExtractContour object used to compute
	/// 3D objects shapes and extract cell/points attributes under the mouse.
	OffscreenExtractContour * contours() const;
	/// @brief Returns the statistics extractor (currently unused)
	OffscreenExtractShapeStatistics * statistics() const;
	/// @brief Returns the overlayed infos widget
	VipInfoWidget * infos() const;

    /// @brief Returns true if information tracking of cells/points under the mouse is enabled
    bool trackingEnabled() const;

	/// @brief Find attribute bounds for given attribute name and component
	/// @return true is computation was successfull, false if attribute could not be found
    bool findPointAttributeBounds(const VipVTKObjectList & objs, VipVTKObject::AttributeType type, const QString& attribute, int component, double* min, double* max) const;

	/// @brief Compute XYZ bounds based on visible objects
	void computeVisualBounds(double bounds[6]);
	double* computeVisualBounds();

    /// @brief Returns all VipPlotVTKObject attached to this widget
    PlotVipVTKObjectList objects() const;
	/// @brief Returns all selected VipPlotVTKObject attached to this widget
	PlotVipVTKObjectList selectedObjects() const;
	/// @brief Returns all VipPlotVTKObject that have a field attribute at given component equal to value
	PlotVipVTKObjectList find(const QString& attribute, int component, const QString& value) const;
    /// @brief Returns all VipPlotVTKObject with given name (in the sense of VipVTKObject::dataName())
    PlotVipVTKObjectList find(const QString& name) const;
	/// @brief Returns the first VipPlotVTKObject with given name (in the sense of VipVTKObject::dataName())
	VipPlotVTKObject* objectByName(const QString& name) const;

	/// @brief Returns the underlying vtkRenderWindow
	vtkRenderWindow* renderWindow();
	/// @brief Returns the main renderer (layer 0)
	vtkRenderer * renderer();
	/// @brief Returns the full list of renderers (layers 0 to 9)
	const QList<vtkRenderer*>& renderers() const;

	/// @brief Returns the current active camera as a VipFieldOfView object
	VipFieldOfView currentCamera() const;

    /// @brief Returns the window's content as a VipVTKImage using vtkWindowToImageFilter processing.
    /// @param magnifier magnifier parameter passed to vtkWindowToImageFilter
    /// @param bounds viewport bounds passed to vtkWindowToImageFilter
    /// @param input_buffer_type buffer type passed to vtkWindowToImageFilter::SetInputBufferType
    /// @return VipVTKImage object
    VipVTKImage imageContent(int magnifier = 1, double * bounds = nullptr, int input_buffer_type = VTK_RGBA);

	/// @brief Returns the window's content as a QImage using renderObject()
	/// @param bounds viewport bounds
	/// @return QImage object
	QImage widgetContent(double * bounds);

	/// @brief Transform XYZ coordinates to view coordinates
	QPoint transformToView(double * pt);
	/// @brief Transform XYZ coordinates to view coordinates
	QPointF transformToDoubleView(double * pt);

	/// @brief Transform view coordinates to XYZ coordinates
	QPointF transformToWorldXY(const QPoint & pt, double z = 0);
	/// @brief Transform view coordinates to XYZ coordinates
	QPointF transformToWorldYZ(const QPoint & pt, double x = 0);
	/// @brief Transform view coordinates to XYZ coordinates
	QPointF transformToWorldXZ(const QPoint & pt, double y = 0);

	/// @brief Set the renderer background color
	void setBackgroundColor(const QColor & color);
	QColor backgroundColor() const;

	/// @brief Returns true if the orientation marker widget is visible.
	bool orientationMarkerWidgetVisible() const;
	/// @brief Returns true if lighting is enabled.
	bool lighting() const;
	/// @brief Returns true if resetting the camera on new object is enabled.
	bool resetCameraEnabled() const;
	/// @brief Returns true if the vtkCubeAxesActor is visible.
	bool axesVisible() const;

	/// @brief Reimplemented from VipImageWidget2D
	virtual bool renderObject(QPainter * p, const QPointF & pos, bool draw_background);
	/// @brief Reimplemented from VipImageWidget2D
	virtual void startRender(VipRenderState & state);
	/// @brief Reimplemented from VipImageWidget2D
	virtual	void endRender(VipRenderState & state);

	/// @brief Set a source property.
	/// A source property is nothing more than a QObject dynamic property, but it will also be propagated to all VipDisplayObject inside
	/// this VipVTKGraphicsView using #VipProcessingObject::setSourceProperty. This is convinent way to define and propagate a property which is global to this viewer.
	/// For instance, this used to set a global GPS reference in order to compute 3D object coordinates based on a same reference.
	/// If this function is reimplemented, the new implementation sould call the base class implementation to ensure VipVTKGraphicsView's built-in integrity.
	virtual void setSourceProperty(const char * name, const QVariant & value);
	QList<QByteArray> sourceProperties() const;

	/// @brief Compute the 3D visible bounds of all displayed objects
	void computeBounds(double *bounds);

public Q_SLOTS:

	/// @brief Trigger a refresh (update) of the viewport.
	/// This will update the opengl scene.
	void refresh();
	/// @brief Immediatly refresh (update) the viewport.
	void immediateRefresh();

	/// @brief Show/hide 3D axes
	void setAxesVisible(bool);

	/// @brief Disable/enable further calls to resetCamera(),
	/// which is automatically called when a 3D object is added/removed.
	void setResetCameraEnabled(bool);

	/// @brief Reset the current camera based on displayed objects bounds
	void resetCamera();

	/// @brief Set mouse tracking enabled in order to displayed CAD information
	void setTrackingEnable(bool);

	/// @brief Show/hide the orientation marker widget
	void setOrientationMarkerWidgetVisible(bool);

	/// @brief Enable/disable lighting
	void setLighting(bool);

	/// @brief Set the current active camera
	void setCurrentCamera(const VipFieldOfView& fov);

	void resetActiveCameraToIsometricView();
	void resetActiveCameraToPositiveX();
	void resetActiveCameraToNegativeX();
	void resetActiveCameraToPositiveY();
	void resetActiveCameraToNegativeY();
	void resetActiveCameraToPositiveZ();
	void resetActiveCameraToNegativeZ();
	void rotateClockwise90();
	void rotateCounterClockwise90();

	void emitDataChanged() { Q_EMIT dataChanged(); }
	void emitCameraUpdated() { Q_EMIT cameraUpdated(); }
  protected:

    virtual void mousePressEvent(QMouseEvent* event);
	// overloaded mouse move handler
	virtual void mouseMoveEvent(QMouseEvent* event);
	// overloaded mouse release handler
	virtual void mouseReleaseEvent(QMouseEvent* event);
	// overloaded key press handler
	virtual void keyPressEvent(QKeyEvent* event);
	// overloaded key release handler
	virtual void keyReleaseEvent(QKeyEvent* event);
	// overload wheel mouse event
	virtual void wheelEvent(QWheelEvent* event);
	virtual void paintEvent(QPaintEvent * evt);
	virtual void drawBackground(QPainter* p, const QRectF& vtkNotUsed(r));
	//void drawForeground(QPainter * painter, const QRectF & rect);
    void resizeEvent(QResizeEvent *event);

Q_SIGNALS:

	/// @brief This signal is emitted whenever a new VipVTKObject is added to the scene through its VipPlotVTKObject.
	void dataChanged();

	/// @brief Emitted whenever the mouse move over the widget.
	void mouseMove(const QPoint &);

	void cameraUpdated();

private Q_SLOTS:

	void computeAxesBounds();

	//color mapmanagement
	void colorMapModified();
	void colorMapDivModified();
	void computeColorMap();

	//Just call camera->Modified()
	void touchCamera();

	void propagateSourceProperties();

	void sendFakeResizeEvent();
	void initializeViewRendering();

	void applyLighting();

	void resetCamera(bool closest, double offsetRatio = 0.9);
	void resetActiveCameraToDirection(double look_x, double look_y, double look_z, double up_x, double up_y, double up_z);

private:

    VipVTKWidget* mWidget;
	VipInfoWidget * mInfos;
    vtkRenderer * mRenderer;
	QList<vtkRenderer*> mRenderers;
	QGraphicsItem* mItemUnderMouse;
	QPointer<QGraphicsObject> mObjectUnderMouse;
	VipColorPalette mPalette;
	VipBorderLegend * mAnnotationLegend;
	OffscreenExtractShapeStatistics * mStats;

    vtkSmartPointer<vtkLookupTable> mLut;
    vtkSmartPointer<vtkScalarBarActor> mScalarBar;
	vtkSmartPointer<vtkCoordinate> mCoordinates;

    vtkSmartPointer<vtkCubeAxesActor> mCubeAxesActor;
	//vtkSmartPointer<vtkGridAxes3DActor> mGridAxesActor;
	vtkSmartPointer<vtkOrientationMarkerWidget> mOrientationAxes;

    bool mTrackingEnable;
	bool mDirtyColorMapDiv;
	bool mInitialized{ false };
	bool mInRefresh{ false };
	bool mHasLight{ true };
	bool mResetCameraEnabled{ true };
	OffscreenExtractContour mContours;
};

VIP_REGISTER_QOBJECT_METATYPE(VipVTKGraphicsView*)

#endif
