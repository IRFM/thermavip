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

/*=========================================================================

  Program:   Visualization Toolkit
  Module:    VipVTKWidget.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifdef _MSC_VER
// Disable warnings that Qt headers give.
#pragma warning(disable : 4127)
#pragma warning(disable : 4512)
#endif

#include "VipVTKWidget.h"
#include "p_QVTKBridge.h"
#include "VipVTKGraphicsView.h"

#include <QApplication>
#include <QMouseEvent>
#include <QResizeEvent>
#include <qmutex.h>

#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkInteractorStyleTrackballCamera.h"
#include <vtkCamera.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkCamera.h>
#include <vtkRendererCollection.h>

#include "VipCore.h"
#include "VipLogging.h"

#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
#include "vtkTDxUnixDevice.h"
#endif


class VipVTKWidget::PrivateData
{
public:
	// the vtk render window
	// TEST
	vtkSmartPointer</* vtkGenericOpenGLRenderWindow*/ vtkRenderWindow> RenWin;
	//bool UseTDx{ false };
	bool IgnoreMouse{ false }; // For click simulation
	bool CameraUserMoved{ false };

	std::unique_ptr<QVTKInteractorAdapter> IrenAdapter;
	vtkSmartPointer<vtkEventQtSlotConnect> Connect{ vtkSmartPointer<vtkEventQtSlotConnect>::New() };

	// Protect rendering while potentially modifying camera
	QRecursiveMutex DisplayMutex;
};


VipVTKWidget::VipVTKWidget(QWidget* p)
  : QOpenGLWidget(p)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->IrenAdapter.reset(new QVTKInteractorAdapter(this));
	d_data->Connect = vtkSmartPointer<vtkEventQtSlotConnect>::New();
	this->setMouseTracking(true);
	this->InitInteractor();
}


void VipVTKWidget::InitInteractor()
{
	QPoint glob1010 = this->mapToGlobal(QPoint(10, 10));
	QPoint glob1111 = this->mapToGlobal(QPoint(11, 11));
	QMouseEvent press(QEvent::MouseButtonPress, QPoint(10, 10), glob1010, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	QMouseEvent move(QEvent::MouseButtonPress, QPoint(11, 11), glob1111, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	QMouseEvent release(QEvent::MouseButtonPress, QPoint(11, 11), glob1111, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(&press, d_data->RenWin->GetInteractor());
		d_data->IrenAdapter->ProcessEvent(&move, d_data->RenWin->GetInteractor());
		d_data->IrenAdapter->ProcessEvent(&release, d_data->RenWin->GetInteractor());
	}
}

/*! destructor */

VipVTKWidget::~VipVTKWidget()
{
	// get rid of the VTK window
	this->setRenderWindow(nullptr);
}

// ----------------------------------------------------------------------------
/* void VipVTKWidget::setUseTDx(bool useTDx)
{
	if (useTDx != d_data->UseTDx) {
		d_data->UseTDx = useTDx;
		if (d_data->UseTDx) {
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
			QByteArray theSignal = QMetaObject::normalizedSignature("CreateDevice(vtkTDxDevice *)");
			if (QApplication::instance()->metaObject()->indexOfSignal(theSignal) != -1) {
				QObject::connect(QApplication::instance(), SIGNAL(CreateDevice(vtkTDxDevice*)), this, SLOT(setDevice(vtkTDxDevice*)));
			}
			else {
				vtkGenericWarningMacro("Missing signal CreateDevice on QApplication. 3DConnexion device will not work. Define it or derive your QApplication from QVTKApplication.");
			}
#endif
		}
	}
}

// ----------------------------------------------------------------------------
bool VipVTKWidget::useTDx() const
{
	return d_data->UseTDx;
}*/


/*! get the render window
 */
/* vtkGenericOpenGLRenderWindow**/vtkRenderWindow* VipVTKWidget::renderWindow()
{
	if (!d_data->RenWin) {
		// create a default vtk window
		vtkGenericOpenGLRenderWindow* win = vtkGenericOpenGLRenderWindow::New();
		this->setRenderWindow(win);
		win->Delete();
	}

	return d_data->RenWin;
}

/*! set the render window
  this will bind a VTK window with the Qt window
  it'll also replace an existing VTK window
*/
void VipVTKWidget::setRenderWindow(/* vtkGenericOpenGLRenderWindow*/vtkRenderWindow * w)
{
	// do nothing if we don't have to
	if (w == d_data->RenWin) {
		return;
	}

	// unregister previous window
	if (d_data->RenWin) {

		d_data->RenWin->Finalize();
		if (d_data->RenWin->IsA("vtkGenericOpenGLRenderWindow")) {
			vtkGenericOpenGLRenderWindow* ren = static_cast<vtkGenericOpenGLRenderWindow*>(d_data->RenWin.GetPointer());
			ren->SetMapped(0);
		}
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::WindowMakeCurrentEvent, this, SLOT(MakeCurrent()));
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::WindowIsCurrentEvent, this, SLOT(IsCurrent(vtkObject*, unsigned long, void*, void*)));
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::WindowFrameEvent, this, SLOT(Frame()));
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::StartEvent, this, SLOT(Start()));
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::EndEvent, this, SLOT(End()));
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::WindowIsDirectEvent, this, SLOT(IsDirect(vtkObject*, unsigned long, void*, void*)));
		d_data->Connect->Disconnect(d_data->RenWin, vtkCommand::WindowSupportsOpenGLEvent, this, SLOT(SupportsOpenGL(vtkObject*, unsigned long, void*, void*)));
	}

	// now set the window
	d_data->RenWin = w;

	if (d_data->RenWin) {
		d_data->RenWin->SetMultiSamples(4);
		d_data->RenWin->LineSmoothingOn();
		d_data->RenWin->PolygonSmoothingOn();
		d_data->RenWin->PointSmoothingOn();

		// if it is mapped somewhere else, unmap it
		d_data->RenWin->Finalize();
		if (d_data->RenWin->IsA("vtkGenericOpenGLRenderWindow")) {
			vtkGenericOpenGLRenderWindow* ren = static_cast<vtkGenericOpenGLRenderWindow*>(d_data->RenWin.GetPointer());
			ren->SetMapped(1);
		}

		// tell the vtk window what the size of this window is
		d_data->RenWin->SetSize(this->width(), this->height());
		d_data->RenWin->SetPosition(this->x(), this->y());

		// if an interactor wasn't provided, we'll make one by default
		if (!d_data->RenWin->GetInteractor()) {
			// create a default interactor
			QVTKInteractor* iren = QVTKInteractor::New();
			//iren->SetUseTDx(d_data->UseTDx);
			d_data->RenWin->SetInteractor(iren);
			iren->Initialize();

			// now set the default style
			vtkInteractorStyle* s = vtkInteractorStyleTrackballCamera::New();
			iren->SetInteractorStyle(s);

			iren->Delete();
			s->Delete();
		}
		
		// tell the interactor the size of this window
		d_data->RenWin->GetInteractor()->SetSize(this->width(), this->height());

		d_data->Connect->Connect(d_data->RenWin, vtkCommand::WindowMakeCurrentEvent, this, SLOT(MakeCurrent()));
		d_data->Connect->Connect(d_data->RenWin, vtkCommand::WindowIsCurrentEvent, this, SLOT(IsCurrent(vtkObject*, unsigned long, void*, void*)));
		d_data->Connect->Connect(d_data->RenWin, vtkCommand::WindowFrameEvent, this, SLOT(Frame()));
		d_data->Connect->Connect(d_data->RenWin, vtkCommand::StartEvent, this, SLOT(Start()));
		d_data->Connect->Connect(d_data->RenWin, vtkCommand::EndEvent, this, SLOT(End()));
		d_data->Connect->Connect(d_data->RenWin, vtkCommand::WindowIsDirectEvent, this, SLOT(IsDirect(vtkObject*, unsigned long, void*, void*)));
		d_data->Connect->Connect(d_data->RenWin, vtkCommand::WindowSupportsOpenGLEvent, this, SLOT(SupportsOpenGL(vtkObject*, unsigned long, void*, void*)));
	}
}

/*! get the Qt/VTK interactor
 */
QVTKInteractor* VipVTKWidget::interactor()
{
	return QVTKInteractor ::SafeDownCast(this->renderWindow()->GetInteractor());
}

void VipVTKWidget::Start()
{
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);
	makeCurrent();
	if (d_data->RenWin->IsA("vtkGenericOpenGLRenderWindow")) {
		vtkGenericOpenGLRenderWindow* ren = static_cast<vtkGenericOpenGLRenderWindow*>(d_data->RenWin.GetPointer());
		ren->PushState();
		ren->OpenGLInitState();
	}
	
}

void VipVTKWidget::End()
{
	if (d_data->RenWin->IsA("vtkGenericOpenGLRenderWindow")) {
		vtkGenericOpenGLRenderWindow* ren = static_cast<vtkGenericOpenGLRenderWindow*>(d_data->RenWin.GetPointer());
		ren->PopState();
	}
}

void VipVTKWidget::initializeGL()
{
	QOpenGLContext* ctx = this->context();
	if (!ctx)
		VIP_LOG_ERROR("OpenGL context not initialized");
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);

	if (!d_data->RenWin) {
		return;
	}
	if (d_data->RenWin->IsA("vtkGenericOpenGLRenderWindow")) {
		vtkGenericOpenGLRenderWindow* ren = static_cast<vtkGenericOpenGLRenderWindow*>(d_data->RenWin.GetPointer());
		ren->OpenGLInitContext();
	}

	glEnable(GL_MULTISAMPLE);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);
}

/*! handle resize event
 */
void VipVTKWidget::resizeGL(int w, int h)
{
	if (!d_data->RenWin) {
		return;
	}

	d_data->RenWin->SetSize(w, h);

	// and update the interactor
	if (d_data->RenWin->GetInteractor()) {
		QResizeEvent e(QSize(w, h), QSize());
		d_data->IrenAdapter->ProcessEvent(&e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::moveEvent(QMoveEvent* e)
{
	QWidget::moveEvent(e);

	if (!d_data->RenWin) {
		return;
	}

	d_data->RenWin->SetPosition(this->x(), this->y());
}

#include <VipVTKGraphicsView.h>
/*! handle paint event
 */
void VipVTKWidget::paintGL()
{
	QMutexLocker lock(&d_data->DisplayMutex);

	// lock all VipVTKObject
	VipVTKObjectLockerList lockers;
	QWidget* p = parentWidget();
	VipVTKGraphicsView* view = nullptr;
	while (p) {
		if (qobject_cast<VipVTKGraphicsView*>(p)) {
			view = static_cast<VipVTKGraphicsView*>(p);
			break;
		}
	}
	if (view)
		lockers = vipLockVTKObjects(fromPlotVipVTKObject(view->objects()));

	vtkRenderWindowInteractor* iren = nullptr;
	if (d_data->RenWin) {
		iren = d_data->RenWin->GetInteractor();
	}

	if (!iren || !iren->GetEnabled()) {
		return;
	}

	glEnable(GL_MULTISAMPLE);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);
	iren->Render();
}

/*! handle mouse press event
 */
void VipVTKWidget::mousePressEvent(QMouseEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

/*! handle mouse move event
 */
void VipVTKWidget::mouseMoveEvent(QMouseEvent* e)
{
	if (d_data->RenWin && !d_data->IgnoreMouse) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
		if (e->buttons())
			d_data->CameraUserMoved = true;
		applyCameraToAllLayers();
	}
}

/*! handle enter event
 */
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void VipVTKWidget::enterEvent(QEvent* e)
#else
void VipVTKWidget::enterEvent(QEnterEvent* e)
#endif
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

/*! handle leave event
 */
void VipVTKWidget::leaveEvent(QEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

/*! handle mouse release event
 */
void VipVTKWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::resetCameraUserMoved()
{
	d_data->CameraUserMoved = false;
}

bool VipVTKWidget::cameraUserMoved() const
{
	return d_data->CameraUserMoved;
}


void VipVTKWidget::applyCameraToAllLayers()
{
	QMutexLocker lock(&d_data->DisplayMutex);

	//retrieve parent VipVTKGraphicsView
	VipVTKGraphicsView* parent = nullptr;
	QWidget* p = this->parentWidget();
	while (p ) {
		if ((parent = qobject_cast<VipVTKGraphicsView*>(p)))
			break;
		p = p->parentWidget();
	}
	
	if (parent) {
		auto lst = parent->renderers();
		for (int i = 1; i < lst.size(); ++i)
			lst[i]->SetActiveCamera(lst.first()->GetActiveCamera());

		parent->emitCameraUpdated();

		// find parent VTK3DPlayer
		//TODO
		/* VTK3DPlayer* player = nullptr;
		QWidget* p = parent->parentWidget();
		while (p) {
			if ((player = qobject_cast<VTK3DPlayer*>(p)))
				break;
			p = p->parentWidget();
		}
		if (player && player->isSharedCamera()) {
			// apply shared camera
			player->applyThisCameraToAll();
		}*/
		return;
	}

	vtkRendererCollection* col = renderWindow()->GetRenderers();
	vtkRenderer* ren = nullptr;

	// find the first interactive renderer
	col->InitTraversal();
	while (vtkRenderer* tmp = col->GetNextItem()) {
		if (tmp->GetInteractive()) {
			ren = tmp;
			break;
		}
	}

	// apply found camera to all other renderer
	col->InitTraversal();
	while (vtkRenderer* tmp = col->GetNextItem()) {
		if (tmp != ren) {
			tmp->SetActiveCamera(ren->GetActiveCamera()); // GetActiveCamera()->DeepCopy(ren->GetActiveCamera());
		}
	}
}

#include "VipCore.h"
void VipVTKWidget::simulateMouseClick(const QPoint& from, const QPoint& to)
{
	QPoint glob_from = this->mapToGlobal(from);
	QPoint glob_to = this->mapToGlobal(to);
	QMouseEvent press(QEvent::MouseButtonPress, from, glob_from, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	QMouseEvent move(QEvent::MouseMove, to, glob_to, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	QMouseEvent release(QEvent::MouseButtonRelease, to, glob_to, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

	d_data->IgnoreMouse = true;
	this->mousePressEvent(&press);
	d_data->IgnoreMouse = false;
	this->mouseMoveEvent(&move);
	d_data->IgnoreMouse = true;
	this->mouseReleaseEvent(&release);
	vipProcessEvents(nullptr, 10);
	d_data->IgnoreMouse = false;

	// applyCameraToAllLayers();
}

/*void VipVTKWidget::forceInteractorRefresh()
{
	return;
	 if (GetRenderWindow()->GetInteractor())
	{
		QMouseEvent press(QEvent::MouseButtonPress, QPoint(0, 0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
		QMouseEvent move(QEvent::MouseMove, QPoint(0, 0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
		QMouseEvent release(QEvent::MouseButtonRelease, QPoint(0, 0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
		QVTKInteractorAdapter irenAdapter;
		irenAdapter.ProcessEvent(&press, GetRenderWindow()->GetInteractor());
		irenAdapter.ProcessEvent(&move, GetRenderWindow()->GetInteractor());
		irenAdapter.ProcessEvent(&release, GetRenderWindow()->GetInteractor());
	}
}*/

/*! handle key press event
 */
void VipVTKWidget::keyPressEvent(QKeyEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

/*! handle key release event
 */
void VipVTKWidget::keyReleaseEvent(QKeyEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::wheelEvent(QWheelEvent* e)
{
	if (d_data->RenWin) {
		if (d_data->RenWin->GetInteractor()->GetInteractorStyle()->IsA("vtkInteractorStyle")) {
			if (e->modifiers() & Qt::ShiftModifier)
				static_cast<vtkInteractorStyle*>(d_data->RenWin->GetInteractor()->GetInteractorStyle())->SetMouseWheelMotionFactor(0.1);
			else
				static_cast<vtkInteractorStyle*>(d_data->RenWin->GetInteractor()->GetInteractorStyle())->SetMouseWheelMotionFactor(1);
		}

		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
		d_data->CameraUserMoved = true;

		// mark the camera as modified (this does not seem to be done automatically
		d_data->RenWin->GetRenderers()->InitTraversal();
		while (vtkRenderer* ren = d_data->RenWin->GetRenderers()->GetNextItem()) {
			ren->GetActiveCamera()->Modified();
			ren->Modified();
		}
		// d_data->RenWin->GetRenderers()->GetNextItem()->GetActiveCamera()->Modified();
		d_data->RenWin->Modified();

		applyCameraToAllLayers();
	}
}

void VipVTKWidget::contextMenuEvent(QContextMenuEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::dragEnterEvent(QDragEnterEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::dragMoveEvent(QDragMoveEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::dragLeaveEvent(QDragLeaveEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

void VipVTKWidget::dropEvent(QDropEvent* e)
{
	if (d_data->RenWin) {
		d_data->IrenAdapter->ProcessEvent(e, d_data->RenWin->GetInteractor());
	}
}

bool VipVTKWidget::focusNextPrevChild(bool)
{
	return false;
}

#ifdef VTK_USE_TDX
// Description:
// Receive notification of the creation of the TDxDevice
void VipVTKWidget::setDevice(vtkTDxDevice* device)
{
#ifdef Q_WS_X11
	if (this->GetInteractor()->GetDevice() != device) {
		this->GetInteractor()->SetDevice(device);
	}
#else
	(void)device; // to avoid warnings.
#endif
}
#endif

void VipVTKWidget::MakeCurrent()
{
	this->makeCurrent();
}

void VipVTKWidget::IsCurrent(vtkObject*, unsigned long, void*, void* call_data)
{
	bool* ptr = reinterpret_cast<bool*>(call_data);
	*ptr = QOpenGLContext::currentContext() == this->context();
}


void VipVTKWidget::IsDirect(vtkObject*, unsigned long, void*, void* call_data)
{
	int* ptr = reinterpret_cast<int*>(call_data);
	*ptr = true; // QGLFormat::fromSurfaceFormat(this->context()->format()).directRendering();//QGLWidget::setFormat(QGLFormat(QGL::SampleBuffers));
}

void VipVTKWidget::SupportsOpenGL(vtkObject*, unsigned long, void*, void* call_data)
{
	int* ptr = reinterpret_cast<int*>(call_data);
	*ptr = true; // QGLFormat::hasOpenGL();
}

void VipVTKWidget::Frame()
{
	if (d_data->RenWin->GetSwapBuffers()) {
		// this->context()->swapBuffers(this->context()->surface());
		// this->swapBuffers();
		update();
	}

	// This callback will call swapBuffers() for us
	// because sometimes VTK does a render without coming through this paintGL()

	// if you want paintGL to always be called for each time VTK renders
	// 1. turn off EnableRender on the interactor,
	// 2. turn off SwapBuffers on the render window,
	// 3. add an observer for the RenderEvent coming from the interactor
	// 4. implement the callback on the observer to call updateGL() on this widget
	// 5. overload VipVTKWidget::paintGL() to call d_data->RenWin->Render() instead iren->Render()
}
