/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, L�o Dubus, Erwan Grelier
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

  Copyright 2004 Sandia Corporation.
  Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
  license for use of this work by or on behalf of the
  U.S. Government. Redistribution and use in source and binary forms, with
  or without modification, are permitted provided that this Notice and any
  statement of authorship are reproduced on all copies.

=========================================================================*/

/*========================================================================
 For general information about using VTK and Qt, see:
 http://www.trolltech.com/products/3rdparty/vtksupport.html
=========================================================================*/

/*========================================================================
 !!! WARNING for those who want to contribute code to this file.
 !!! If you use a commercial edition of Qt, you can modify this code.
 !!! If you use an open source version of Qt, you are free to modify
 !!! and use this code within the guidelines of the GPL license.
 !!! Unfortunately, you cannot contribute the changes back into this
 !!! file.  Doing so creates a conflict between the GPL and BSD-like VTK
 !!! license.
=========================================================================*/

#include "p_QVTKBridge.h"
#include "vtkCallbackCommand.h"

#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
#include "vtkTDxWinDevice.h"
#endif

#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
#include "vtkTDxMacDevice.h"
#endif

#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
#include "vtkTDxUnixDevice.h"
#endif

#include "vtkCommand.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "VipConfig.h"

#include <QEvent>
#include <QResizeEvent>
#include <QSignalMapper>
#include <QTimer>
#include <qmetaobject.h>
#include <qmutex.h>
#include <qobject.h>

#include <map>
#include <vector>


// function to get VTK keysyms from ascii characters
static const char* ascii_to_key_sym(int);
// function to get VTK keysyms from Qt keys
static const char* qt_key_to_key_sym(Qt::Key);


QVTKInteractorAdapter::QVTKInteractorAdapter(QObject* parentObject)
  : QObject(parentObject)
{
}

QVTKInteractorAdapter::~QVTKInteractorAdapter() {}

bool QVTKInteractorAdapter::ProcessEvent(QEvent* e, vtkRenderWindowInteractor* iren)
{
	if (iren == NULL || e == NULL)
		return false;

	const QEvent::Type t = e->type();

	if (t == QEvent::Resize) {
		QResizeEvent* e2 = static_cast<QResizeEvent*>(e);
		QSize size = e2->size();
		iren->SetSize(size.width(), size.height());
		return true;
	}

	if (t == QEvent::FocusIn) {
		// For 3DConnexion devices:
		QVTKInteractor* qiren = QVTKInteractor::SafeDownCast(iren);
		if (qiren) {
			qiren->StartListening();
		}
		return true;
	}

	if (t == QEvent::FocusOut) {
		// For 3DConnexion devices:
		QVTKInteractor* qiren = QVTKInteractor::SafeDownCast(iren);
		if (qiren) {
			qiren->StopListening();
		}
		return true;
	}

	// the following events only happen if the interactor is enabled
	if (!iren->GetEnabled())
		return false;

	if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonRelease || t == QEvent::MouseButtonDblClick || t == QEvent::MouseMove) {
		QMouseEvent* e2 = static_cast<QMouseEvent*>(e);

		// give interactor the event information
		iren->SetEventInformationFlipY(e2->VIP_EVT_POSITION().x(),
					       e2->VIP_EVT_POSITION().y(),
					       (e2->modifiers() & Qt::ControlModifier) > 0 ? 1 : 0,
					       (e2->modifiers() & Qt::ShiftModifier) > 0 ? 1 : 0,
					       0,
					       e2->type() == QEvent::MouseButtonDblClick ? 1 : 0);

		if (t == QEvent::MouseMove) {
			iren->InvokeEvent(vtkCommand::MouseMoveEvent, e2);
		}
		else if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonDblClick) {
			switch (e2->button()) {
				case Qt::LeftButton:
					iren->InvokeEvent(vtkCommand::LeftButtonPressEvent, e2);
					break;

				case Qt::MiddleButton:
					iren->InvokeEvent(vtkCommand::MiddleButtonPressEvent, e2);
					break;

				case Qt::RightButton:
					iren->InvokeEvent(vtkCommand::RightButtonPressEvent, e2);
					break;

				default:
					break;
			}
		}
		else if (t == QEvent::MouseButtonRelease) {
			switch (e2->button()) {
				case Qt::LeftButton:
					iren->InvokeEvent(vtkCommand::LeftButtonReleaseEvent, e2);
					break;

				case Qt::MiddleButton:
					iren->InvokeEvent(vtkCommand::MiddleButtonReleaseEvent, e2);
					break;

				case Qt::RightButton:
					iren->InvokeEvent(vtkCommand::RightButtonReleaseEvent, e2);
					break;

				default:
					break;
			}
		}
		return true;
	}

	if (t == QEvent::Enter) {
		iren->InvokeEvent(vtkCommand::EnterEvent, e);
		return true;
	}

	if (t == QEvent::Leave) {
		iren->InvokeEvent(vtkCommand::LeaveEvent, e);
		return true;
	}

	if (t == QEvent::KeyPress || t == QEvent::KeyRelease) {
		QKeyEvent* e2 = static_cast<QKeyEvent*>(e);

		// get key and keysym information
		int ascii_key = e2->text().length() ? e2->text().unicode()->toLatin1() : 0;
		const char* keysym = ascii_to_key_sym(ascii_key);
		if (!keysym) {
			// get virtual keys
			keysym = qt_key_to_key_sym(static_cast<Qt::Key>(e2->key()));
		}

		if (!keysym) {
			keysym = "None";
		}

		// give interactor event information
		iren->SetKeyEventInformation((e2->modifiers() & Qt::ControlModifier), (e2->modifiers() & Qt::ShiftModifier), ascii_key, e2->count(), keysym);

		if (t == QEvent::KeyPress) {
			// invoke vtk event
			iren->InvokeEvent(vtkCommand::KeyPressEvent, e2);

			// invoke char event only for ascii characters
			if (ascii_key) {
				iren->InvokeEvent(vtkCommand::CharEvent, e2);
			}
		}
		else {
			iren->InvokeEvent(vtkCommand::KeyReleaseEvent, e2);
		}
		return true;
	}

	if (t == QEvent::Wheel) {
		QWheelEvent* e2 = static_cast<QWheelEvent*>(e);

		iren->SetEventInformationFlipY(e2->VIP_EVT_POSITION().x(), e2->VIP_EVT_POSITION().y(), (e2->modifiers() & Qt::ControlModifier) > 0 ? 1 : 0, (e2->modifiers() & Qt::ShiftModifier) > 0 ? 1 : 0);

		// invoke vtk event
		// if delta is positive, it is a forward wheel event
		if (e2->angleDelta().y() > 0) {
			iren->InvokeEvent(vtkCommand::MouseWheelForwardEvent, e2);
		}
		else {
			iren->InvokeEvent(vtkCommand::MouseWheelBackwardEvent, e2);
		}
		return true;
	}

	if (t == QEvent::ContextMenu) {
		QContextMenuEvent* e2 = static_cast<QContextMenuEvent*>(e);

		// give interactor the event information
		iren->SetEventInformationFlipY(e2->x(), e2->y(), (e2->modifiers() & Qt::ControlModifier) > 0 ? 1 : 0, (e2->modifiers() & Qt::ShiftModifier) > 0 ? 1 : 0);

		// invoke event and pass qt event for additional data as well
		iren->InvokeEvent(QVTKInteractor::ContextMenuEvent, e2);

		return true;
	}

	if (t == QEvent::DragEnter) {
		QDragEnterEvent* e2 = static_cast<QDragEnterEvent*>(e);

		// invoke event and pass qt event for additional data as well
		iren->InvokeEvent(QVTKInteractor::DragEnterEvent, e2);

		return true;
	}

	if (t == QEvent::DragLeave) {
		QDragLeaveEvent* e2 = static_cast<QDragLeaveEvent*>(e);

		// invoke event and pass qt event for additional data as well
		iren->InvokeEvent(QVTKInteractor::DragLeaveEvent, e2);

		return true;
	}

	if (t == QEvent::DragMove) {
		QDragMoveEvent* e2 = static_cast<QDragMoveEvent*>(e);

		// give interactor the event information
		iren->SetEventInformationFlipY(e2->VIP_EVT_POSITION().x(), e2->VIP_EVT_POSITION().y());

		// invoke event and pass qt event for additional data as well
		iren->InvokeEvent(QVTKInteractor::DragMoveEvent, e2);
		return true;
	}

	if (t == QEvent::Drop) {
		QDropEvent* e2 = static_cast<QDropEvent*>(e);

		// give interactor the event information
		iren->SetEventInformationFlipY(e2->VIP_EVT_POSITION().x(), e2->VIP_EVT_POSITION().y());

		// invoke event and pass qt event for additional data as well
		iren->InvokeEvent(QVTKInteractor::DropEvent, e2);
		return true;
	}

	return false;
}

// ***** keysym stuff below  *****

static const char* AsciiToKeySymTable[] = { 0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    "Tab",
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    "space",
					    "exclam",
					    "quotedbl",
					    "numbersign",
					    "dollar",
					    "percent",
					    "ampersand",
					    "quoteright",
					    "parenleft",
					    "parenright",
					    "asterisk",
					    "plus",
					    "comma",
					    "minus",
					    "period",
					    "slash",
					    "0",
					    "1",
					    "2",
					    "3",
					    "4",
					    "5",
					    "6",
					    "7",
					    "8",
					    "9",
					    "colon",
					    "semicolon",
					    "less",
					    "equal",
					    "greater",
					    "question",
					    "at",
					    "A",
					    "B",
					    "C",
					    "D",
					    "E",
					    "F",
					    "G",
					    "H",
					    "I",
					    "J",
					    "K",
					    "L",
					    "M",
					    "N",
					    "O",
					    "P",
					    "Q",
					    "R",
					    "S",
					    "T",
					    "U",
					    "V",
					    "W",
					    "X",
					    "Y",
					    "Z",
					    "bracketleft",
					    "backslash",
					    "bracketright",
					    "asciicircum",
					    "underscore",
					    "quoteleft",
					    "a",
					    "b",
					    "c",
					    "d",
					    "e",
					    "f",
					    "g",
					    "h",
					    "i",
					    "j",
					    "k",
					    "l",
					    "m",
					    "n",
					    "o",
					    "p",
					    "q",
					    "r",
					    "s",
					    "t",
					    "u",
					    "v",
					    "w",
					    "x",
					    "y",
					    "z",
					    "braceleft",
					    "bar",
					    "braceright",
					    "asciitilde",
					    "Delete",
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0,
					    0 };

const char* ascii_to_key_sym(int i)
{
	if (i >= 0) {
		return AsciiToKeySymTable[i];
	}
	return 0;
}

#define QVTK_HANDLE(x, y)                                                                                                                                                                              \
	case x:                                                                                                                                                                                        \
		ret = y;                                                                                                                                                                               \
		break;

const char* qt_key_to_key_sym(Qt::Key i)
{
	const char* ret = 0;
	switch (i) {
		// Cancel
		QVTK_HANDLE(Qt::Key_Backspace, "BackSpace")
		QVTK_HANDLE(Qt::Key_Tab, "Tab")
		QVTK_HANDLE(Qt::Key_Backtab, "Tab")
		// QVTK_HANDLE(Qt::Key_Clear, "Clear")
		QVTK_HANDLE(Qt::Key_Return, "Return")
		QVTK_HANDLE(Qt::Key_Enter, "Return")
		QVTK_HANDLE(Qt::Key_Shift, "Shift_L")
		QVTK_HANDLE(Qt::Key_Control, "Control_L")
		QVTK_HANDLE(Qt::Key_Alt, "Alt_L")
		QVTK_HANDLE(Qt::Key_Pause, "Pause")
		QVTK_HANDLE(Qt::Key_CapsLock, "Caps_Lock")
		QVTK_HANDLE(Qt::Key_Escape, "Escape")
		QVTK_HANDLE(Qt::Key_Space, "space")
		// QVTK_HANDLE(Qt::Key_Prior, "Prior")
		// QVTK_HANDLE(Qt::Key_Next, "Next")
		QVTK_HANDLE(Qt::Key_End, "End")
		QVTK_HANDLE(Qt::Key_Home, "Home")
		QVTK_HANDLE(Qt::Key_Left, "Left")
		QVTK_HANDLE(Qt::Key_Up, "Up")
		QVTK_HANDLE(Qt::Key_Right, "Right")
		QVTK_HANDLE(Qt::Key_Down, "Down")

		// Select
		// Execute
		QVTK_HANDLE(Qt::Key_SysReq, "Snapshot")
		QVTK_HANDLE(Qt::Key_Insert, "Insert")
		QVTK_HANDLE(Qt::Key_Delete, "Delete")
		QVTK_HANDLE(Qt::Key_Help, "Help")
		QVTK_HANDLE(Qt::Key_0, "0")
		QVTK_HANDLE(Qt::Key_1, "1")
		QVTK_HANDLE(Qt::Key_2, "2")
		QVTK_HANDLE(Qt::Key_3, "3")
		QVTK_HANDLE(Qt::Key_4, "4")
		QVTK_HANDLE(Qt::Key_5, "5")
		QVTK_HANDLE(Qt::Key_6, "6")
		QVTK_HANDLE(Qt::Key_7, "7")
		QVTK_HANDLE(Qt::Key_8, "8")
		QVTK_HANDLE(Qt::Key_9, "9")
		QVTK_HANDLE(Qt::Key_A, "a")
		QVTK_HANDLE(Qt::Key_B, "b")
		QVTK_HANDLE(Qt::Key_C, "c")
		QVTK_HANDLE(Qt::Key_D, "d")
		QVTK_HANDLE(Qt::Key_E, "e")
		QVTK_HANDLE(Qt::Key_F, "f")
		QVTK_HANDLE(Qt::Key_G, "g")
		QVTK_HANDLE(Qt::Key_H, "h")
		QVTK_HANDLE(Qt::Key_I, "i")
		QVTK_HANDLE(Qt::Key_J, "h")
		QVTK_HANDLE(Qt::Key_K, "k")
		QVTK_HANDLE(Qt::Key_L, "l")
		QVTK_HANDLE(Qt::Key_M, "m")
		QVTK_HANDLE(Qt::Key_N, "n")
		QVTK_HANDLE(Qt::Key_O, "o")
		QVTK_HANDLE(Qt::Key_P, "p")
		QVTK_HANDLE(Qt::Key_Q, "q")
		QVTK_HANDLE(Qt::Key_R, "r")
		QVTK_HANDLE(Qt::Key_S, "s")
		QVTK_HANDLE(Qt::Key_T, "t")
		QVTK_HANDLE(Qt::Key_U, "u")
		QVTK_HANDLE(Qt::Key_V, "v")
		QVTK_HANDLE(Qt::Key_W, "w")
		QVTK_HANDLE(Qt::Key_X, "x")
		QVTK_HANDLE(Qt::Key_Y, "y")
		QVTK_HANDLE(Qt::Key_Z, "z")
		// KP_0 - KP_9
		QVTK_HANDLE(Qt::Key_Asterisk, "asterisk")
		QVTK_HANDLE(Qt::Key_Plus, "plus")
		// bar
		QVTK_HANDLE(Qt::Key_Minus, "minus")
		QVTK_HANDLE(Qt::Key_Period, "period")
		QVTK_HANDLE(Qt::Key_Slash, "slash")
		QVTK_HANDLE(Qt::Key_F1, "F1")
		QVTK_HANDLE(Qt::Key_F2, "F2")
		QVTK_HANDLE(Qt::Key_F3, "F3")
		QVTK_HANDLE(Qt::Key_F4, "F4")
		QVTK_HANDLE(Qt::Key_F5, "F5")
		QVTK_HANDLE(Qt::Key_F6, "F6")
		QVTK_HANDLE(Qt::Key_F7, "F7")
		QVTK_HANDLE(Qt::Key_F8, "F8")
		QVTK_HANDLE(Qt::Key_F9, "F9")
		QVTK_HANDLE(Qt::Key_F10, "F10")
		QVTK_HANDLE(Qt::Key_F11, "F11")
		QVTK_HANDLE(Qt::Key_F12, "F12")
		QVTK_HANDLE(Qt::Key_F13, "F13")
		QVTK_HANDLE(Qt::Key_F14, "F14")
		QVTK_HANDLE(Qt::Key_F15, "F15")
		QVTK_HANDLE(Qt::Key_F16, "F16")
		QVTK_HANDLE(Qt::Key_F17, "F17")
		QVTK_HANDLE(Qt::Key_F18, "F18")
		QVTK_HANDLE(Qt::Key_F19, "F19")
		QVTK_HANDLE(Qt::Key_F20, "F20")
		QVTK_HANDLE(Qt::Key_F21, "F21")
		QVTK_HANDLE(Qt::Key_F22, "F22")
		QVTK_HANDLE(Qt::Key_F23, "F23")
		QVTK_HANDLE(Qt::Key_F24, "F24")
		QVTK_HANDLE(Qt::Key_NumLock, "Num_Lock")
		QVTK_HANDLE(Qt::Key_ScrollLock, "Scroll_Lock")

		default:
			break;
	}
	return ret;
}

QVTKInteractorInternal::QVTKInteractorInternal(QVTKInteractor* p)
  : Parent(p)
{
	this->SignalMapper = new QSignalMapper(this);
	QObject::connect(this->SignalMapper, SIGNAL(mapped(int)), this, SLOT(TimerEvent(int)));
}

QVTKInteractorInternal::~QVTKInteractorInternal() {}

void QVTKInteractorInternal::TimerEvent(int id)
{
	Parent->TimerEvent(id);
}

/*! allocation method for Qt/VTK interactor
 */
vtkStandardNewMacro(QVTKInteractor);

/*! constructor for Qt/VTK interactor
 */
QVTKInteractor::QVTKInteractor()
{
	this->Internal = new QVTKInteractorInternal(this);

#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
	this->Device = vtkTDxWinDevice::New();
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
	this->Device = vtkTDxMacDevice::New();
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
	this->Device = 0;
#endif
}

void QVTKInteractor::Initialize()
{
#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
	if (this->UseTDx) {
		// this is QWidget::winId();
		HWND hWnd = static_cast<HWND>(this->GetRenderWindow()->GetGenericWindowId());
		if (!this->Device->GetInitialized()) {
			this->Device->SetInteractor(this);
			this->Device->SetWindowHandle(hWnd);
			this->Device->Initialize();
		}
	}
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
	if (this->UseTDx) {
		if (!this->Device->GetInitialized()) {
			this->Device->SetInteractor(this);
			// Do not initialize the device here.
		}
	}
#endif
	this->Initialized = 1;
	this->Enable();
} 

#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
// ----------------------------------------------------------------------------
vtkTDxUnixDevice* QVTKInteractor::GetDevice()
{
	return this->Device;
}

// ----------------------------------------------------------------------------
void QVTKInteractor::SetDevice(vtkTDxDevice* device)
{
	if (this->Device != device) {
		this->Device = static_cast<vtkTDxUnixDevice*>(device);
	}
}
#endif

/*! start method for interactor
 */
void QVTKInteractor::Start()
{
	vtkErrorMacro(<< "QVTKInteractor cannot control the event loop.");
}

/*! terminate the application
 */
void QVTKInteractor::TerminateApp()
{
	// we are in a GUI so let's terminate the GUI the normal way
	// qApp->exit();
}

// ----------------------------------------------------------------------------
void QVTKInteractor::StartListening()
{
#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
	if (this->Device->GetInitialized() && !this->Device->GetIsListening()) {
		this->Device->StartListening();
	}
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
	if (this->UseTDx && !this->Device->GetInitialized()) {
		this->Device->Initialize();
	}
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
	if (this->UseTDx && this->Device != 0) {
		this->Device->SetInteractor(this);
	}
#endif
}

// ----------------------------------------------------------------------------
void QVTKInteractor::StopListening()
{
#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
	if (this->Device->GetInitialized() && this->Device->GetIsListening()) {
		this->Device->StopListening();
	}
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
	if (this->UseTDx && this->Device->GetInitialized()) {
		this->Device->Close();
	}
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
	if (this->UseTDx && this->Device != 0) {
		// this assumes that a outfocus event is emitted prior
		// a infocus event on another widget.
		this->Device->SetInteractor(0);
	}
#endif
}

/*! handle timer event
 */
void QVTKInteractor::TimerEvent(int timerId)
{
	if (!this->GetEnabled()) {
		return;
	}
	this->InvokeEvent(vtkCommand::TimerEvent, (void*)&timerId);

	if (this->IsOneShotTimer(timerId)) {
		this->DestroyTimer(timerId); // 'cause our Qt timers are always repeating
	}
}

/*! constructor
 */
QVTKInteractor::~QVTKInteractor()
{
	delete this->Internal;
#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
	this->Device->Delete();
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
	this->Device->Delete();
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
	this->Device = 0;
#endif
}

/*! create Qt timer with an interval of 10 msec.
 */
int QVTKInteractor::InternalCreateTimer(int timerId, int vtkNotUsed(timerType), unsigned long duration)
{
	QTimer* timer = new QTimer(this->Internal);
	timer->start(duration);
	this->Internal->SignalMapper->setMapping(timer, timerId);
	QObject::connect(timer, SIGNAL(timeout()), this->Internal->SignalMapper, SLOT(map()));
	int platformTimerId = timer->timerId();
	this->Internal->Timers.insert(QVTKInteractorInternal::TimerMap::value_type(platformTimerId, timer));
	return platformTimerId;
}

/*! destroy timer
 */
int QVTKInteractor::InternalDestroyTimer(int platformTimerId)
{
	QVTKInteractorInternal::TimerMap::iterator iter = this->Internal->Timers.find(platformTimerId);
	if (iter != this->Internal->Timers.end()) {
		iter->second->stop();
		iter->second->deleteLater();
		this->Internal->Timers.erase(iter);
		return 1;
	}
	return 0;
}

// constructor
vtkQtConnection::vtkQtConnection(vtkEventQtSlotConnect* owner)
  : Owner(owner)
{
	this->Callback = vtkCallbackCommand::New();
	this->Callback->SetCallback(vtkQtConnection::DoCallback);
	this->Callback->SetClientData(this);
	this->VTKObject = 0;
	this->QtObject = 0;
	this->ClientData = 0;
	this->VTKEvent = vtkCommand::NoEvent;
}

// destructor, disconnect if necessary
vtkQtConnection::~vtkQtConnection()
{
	if (this->VTKObject) {
		this->VTKObject->RemoveObserver(this->Callback);
		// Qt takes care of disconnecting slots
	}
	this->Callback->Delete();
}

void vtkQtConnection::DoCallback(vtkObject* vtk_obj, unsigned long event, void* client_data, void* call_data)
{
	vtkQtConnection* conn = static_cast<vtkQtConnection*>(client_data);
	conn->Execute(vtk_obj, event, call_data);
}

// callback from VTK to emit signal
void vtkQtConnection::Execute(vtkObject* caller, unsigned long e, void* call_data)
{
	if (e != vtkCommand::DeleteEvent || (e == vtkCommand::DeleteEvent && this->VTKEvent == vtkCommand::DeleteEvent)) {
		emit EmitExecute(caller, e, ClientData, call_data, this->Callback);
	}

	if (e == vtkCommand::DeleteEvent) {
		this->Owner->Disconnect(this->VTKObject, this->VTKEvent, this->QtObject, this->QtSlot.toLatin1().data(), this->ClientData);
	}
}

bool vtkQtConnection::IsConnection(vtkObject* vtk_obj, unsigned long e, const QObject* qt_obj, const char* slot, void* client_data)
{
	if (this->VTKObject != vtk_obj)
		return false;

	if (e != vtkCommand::NoEvent && e != this->VTKEvent)
		return false;

	if (qt_obj && qt_obj != this->QtObject)
		return false;

	if (slot && this->QtSlot != slot)
		return false;

	if (client_data && this->ClientData != client_data)
		return false;

	return true;
}

// set the connection
void vtkQtConnection::SetConnection(vtkObject* vtk_obj, unsigned long e, const QObject* qt_obj, const char* slot, void* client_data, float priority, Qt::ConnectionType type)
{
	// keep track of what we connected
	this->VTKObject = vtk_obj;
	this->QtObject = qt_obj;
	this->VTKEvent = e;
	this->ClientData = client_data;
	this->QtSlot = slot;

	// make a connection between this and the vtk object
	vtk_obj->AddObserver(e, this->Callback, priority);

	if (e != vtkCommand::DeleteEvent) {
		vtk_obj->AddObserver(vtkCommand::DeleteEvent, this->Callback);
	}

	// make a connection between this and the Qt object
	qt_obj->connect(this, SIGNAL(EmitExecute(vtkObject*, unsigned long, void*, void*, vtkCommand*)), slot, type);
	QObject::connect(qt_obj, SIGNAL(destroyed(QObject*)), this, SLOT(deleteConnection()));
}

void vtkQtConnection::deleteConnection()
{
	this->Owner->RemoveConnection(this);
}

void vtkQtConnection::PrintSelf(ostream& os, vtkIndent indent)
{
	if (this->VTKObject && this->QtObject) {
		os << indent << this->VTKObject->GetClassName() << ":" << vtkCommand::GetStringFromEventId(this->VTKEvent) << "  <---->  " << this->QtObject->metaObject()->className()
		   << "::" << this->QtSlot.toLatin1().data() << "\n";
	}
}

vtkStandardNewMacro(vtkEventQtSlotConnect)

  // constructor
  vtkEventQtSlotConnect::vtkEventQtSlotConnect()
{
	Connections = new vtkQtConnections;
}

vtkEventQtSlotConnect::~vtkEventQtSlotConnect()
{
	// clean out connections
	vtkQtConnections::iterator iter;
	for (iter = Connections->begin(); iter != Connections->end(); ++iter) {
		delete (*iter);
	}

	delete Connections;
}

void vtkEventQtSlotConnect::Connect(vtkObject* vtk_obj, unsigned long event, const QObject* qt_obj, const char* slot, void* client_data, float priority, Qt::ConnectionType type)
{
	if (!vtk_obj || !qt_obj) {
		vtkErrorMacro("Cannot connect NULL objects.");
		return;
	}
	vtkQtConnection* connection = new vtkQtConnection(this);
	connection->SetConnection(vtk_obj, event, qt_obj, slot, client_data, priority, type);
	Connections->push_back(connection);
}

void vtkEventQtSlotConnect::Disconnect(vtkObject* vtk_obj, unsigned long event, const QObject* qt_obj, const char* slot, void* client_data)
{
	if (!vtk_obj) {
		vtkQtConnections::iterator iter;
		for (iter = this->Connections->begin(); iter != this->Connections->end(); ++iter) {
			delete (*iter);
		}
		this->Connections->clear();
		return;
	}
	bool all_info = true;
	if (slot == NULL || qt_obj == NULL || event == vtkCommand::NoEvent)
		all_info = false;

	vtkQtConnections::iterator iter;
	for (iter = Connections->begin(); iter != Connections->end();) {
		// if information matches, remove the connection
		if ((*iter)->IsConnection(vtk_obj, event, qt_obj, slot, client_data)) {
			delete (*iter);
			iter = Connections->erase(iter);
			// if user passed in all information, only remove one connection and quit
			if (all_info)
				iter = Connections->end();
		}
		else
			++iter;
	}
}

void vtkEventQtSlotConnect::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);
	if (Connections->empty()) {
		os << indent << "No Connections\n";
	}
	else {
		os << indent << "Connections:\n";
		vtkQtConnections::iterator iter;
		for (iter = Connections->begin(); iter != Connections->end(); ++iter) {
			(*iter)->PrintSelf(os, indent.GetNextIndent());
		}
	}
}

void vtkEventQtSlotConnect::RemoveConnection(vtkQtConnection* conn)
{
	vtkQtConnections::iterator iter;
	for (iter = this->Connections->begin(); iter != this->Connections->end(); ++iter) {
		if (conn == *iter) {
			delete (*iter);
			Connections->erase(iter);
			return;
		}
	}
}

int vtkEventQtSlotConnect::GetNumberOfConnections() const
{
	return static_cast<int>(this->Connections->size());
}
