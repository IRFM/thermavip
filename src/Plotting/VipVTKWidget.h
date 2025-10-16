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

/*=========================================================================

  Program:   Visualization Toolkit
  Module:    VipVTKWidget.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME VipVTKWidget - Display a vtkRenderWindow in a Qt's QGLWidget.
// .SECTION Description
// VipVTKWidget provides a way to display VTK data in a Qt OpenGL widget.

#ifndef VIP_VTK_WIDGET_H
#define VIP_VTK_WIDGET_H

#include "vtkSmartPointer.h"
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <qmutex.h>

#include "VipConfig.h"
#include "VipPimpl.h"

class vtkGenericOpenGLRenderWindow;
class vtkRenderWindow;
class vtkEventQtSlotConnect;
class QVTKInteractorAdapter;
class QVTKInteractor;
class vtkObject;
class vtkContextView;

//#include "vtkTDxConfigure.h" // defines VTK_USE_TDX
//#ifdef VTK_USE_TDX
//class vtkTDxDevice;
//#endif

//! VipVTKWidget displays a VTK window in a Qt window.
class VIP_PLOTTING_EXPORT VipVTKWidget : public QOpenGLWidget
{
	Q_OBJECT

	friend class VipVTKGraphicsView;

public:
	
	//! constructor
	VipVTKWidget(QWidget* parent = nullptr);
	//! destructor
	virtual ~VipVTKWidget();

	// Description:
	// Set the vtk render window, if you wish to use your own vtkRenderWindow
	virtual void setRenderWindow(/* vtkGenericOpenGLRenderWindow**/ vtkRenderWindow*);

	// Description:
	// Get the vtk render window.
	virtual /* vtkGenericOpenGLRenderWindow**/ vtkRenderWindow* renderWindow();

	// Description:
	// Get the Qt/vtk interactor that was either created by default or set by the user
	virtual QVTKInteractor* interactor();

	// Simulate a Qt move click-move-release.
	// This is sometimes the only way to properly refresh the window.
	void simulateMouseClick(const QPoint& from, const QPoint& to);

	/// @brief  Tells if the camera was moved due to a user interaction
	bool cameraUserMoved() const;

public Q_SLOTS:

	/// @brief Tells that has programatically (and not from a user interaction).
	/// This is usefull when following a dynamic camera until the user interact with the camera using the mouse,
	/// in order ot "disconnect" the camera following.
	void resetCameraUserMoved();

	/// @brief Apply the same camera to all vtkRenderer objects based on the active one (usually the first renderer)
	void applyCameraToAllLayers();

	// Description:
	// Receive notification of the creation of the TDxDevice.
	// Only relevant for Unix.
#ifdef VTK_USE_TDX
	void setDevice(vtkTDxDevice* device);
#endif

protected Q_SLOTS:
	// slot to make this vtk render window current
	virtual void MakeCurrent();
	// slot called when vtk wants to know if the context is current
	virtual void IsCurrent(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data);
	// slot called when vtk wants to frame the window
	virtual void Frame();
	// slot called when vtk wants to start the render
	virtual void Start();
	// slot called when vtk wants to end the render
	virtual void End();
	// slot called when vtk wants to know if a window is direct
	virtual void IsDirect(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data);
	// slot called when vtk wants to know if a window supports OpenGL
	virtual void SupportsOpenGL(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data);

protected:
	// overloaded initialize handler
	virtual void initializeGL();
	// overloaded resize handler
	virtual void resizeGL(int, int);
	// overloaded paint handler
	virtual void paintGL();
	// overloaded move handler
	virtual void moveEvent(QMoveEvent* event);

	// overloaded mouse press handler
	virtual void mousePressEvent(QMouseEvent* event);
	// overloaded mouse move handler
	virtual void mouseMoveEvent(QMouseEvent* event);
	// overloaded mouse release handler
	virtual void mouseReleaseEvent(QMouseEvent* event);
	// overloaded key press handler
	virtual void keyPressEvent(QKeyEvent* event);
	// overloaded key release handler
	virtual void keyReleaseEvent(QKeyEvent* event);
	// overloaded enter event
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	virtual void enterEvent(QEvent*);
#else
	virtual void enterEvent(QEnterEvent*);
#endif
	// overloaded leave event
	virtual void leaveEvent(QEvent*);
	// overload wheel mouse event
	virtual void wheelEvent(QWheelEvent*);

	// overload context menu event
	virtual void contextMenuEvent(QContextMenuEvent*);
	// overload drag enter event
	virtual void dragEnterEvent(QDragEnterEvent*);
	// overload drag move event
	virtual void dragMoveEvent(QDragMoveEvent*);
	// overload drag leave event
	virtual void dragLeaveEvent(QDragLeaveEvent*);
	// overload drop event
	virtual void dropEvent(QDropEvent*);

	// overload focus handling so tab key is passed to VTK
	virtual bool focusNextPrevChild(bool);

	void InitInteractor();

	VIP_DECLARE_PRIVATE_DATA();
};

#endif
