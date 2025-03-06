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

// .NAME vtkEventQtSlotConnect - Manage connections between VTK events and Qt slots.
// .SECTION Description
// vtkEventQtSlotConnect provides a way to manage connections between VTK events
// and Qt slots.
// Qt slots to connect with must have one of the following signatures:
// - MySlot()
// - MySlot(vtkObject* caller)
// - MySlot(vtkObject* caller, unsigned long vtk_event)
// - MySlot(vtkObject* caller, unsigned long vtk_event, void* client_data)
// - MySlot(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data)
// - MySlot(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data, vtkCommand*)

#ifndef VIP_QVTK_BRIDGE
#define VIP_QVTK_BRIDGE

#include "vtkCommand.h" // for event defines
#include "vtkObject.h"
#include "vtkTDxConfigure.h" // defines VTK_USE_TDX
#include <vtkRenderWindowInteractor.h>

#include "qobject.h"

#include <map>

#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
class vtkTDxWinDevice;
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
class vtkTDxMacDevice;
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
class vtkTDxDevice;
class vtkTDxUnixDevice;
#endif

class QVTKInteractorInternal;
class vtkRenderWindowInteractor;
class vtkCallbackCommand;
class vtkEventQtSlotConnect;
class QEvent;
class QSignalMapper;

// .NAME QVTKInteractor - An interactor for the QVTKWidget.
// .SECTION Description
// QVTKInteractor is an interactor for a QVTKWiget.

class QVTKInteractor : public vtkRenderWindowInteractor
{
public:
	static QVTKInteractor* New();
	vtkTypeMacro(QVTKInteractor, vtkRenderWindowInteractor);

	// Description:
	// Enum for additional event types supported.
	// These events can be picked up by command observers on the interactor
	enum vtkCustomEvents
	{
		ContextMenuEvent = vtkCommand::UserEvent + 100,
		DragEnterEvent,
		DragMoveEvent,
		DragLeaveEvent,
		DropEvent
	};

	// Description:
	// Overloaded terminiate app, which does nothing in Qt.
	// Use qApp->exit() instead.
	virtual void TerminateApp();

	// Description:
	// Overloaded start method does nothing.
	// Use qApp->exec() instead.
	virtual void Start();
	virtual void Initialize();

	// Description:
	// Start listening events on 3DConnexion device.
	virtual void StartListening();

	// Description:
	// Stop listening events on 3DConnexion device.
	virtual void StopListening();

	// timer event slot
	virtual void TimerEvent(int timerId);

#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
	virtual vtkTDxUnixDevice* GetDevice();
	virtual void SetDevice(vtkTDxDevice* device);
#endif

protected:
	// constructor
	QVTKInteractor();
	// destructor
	~QVTKInteractor();

	// create a Qt Timer
	virtual int InternalCreateTimer(int timerId, int timerType, unsigned long duration);
	// destroy a Qt Timer
	virtual int InternalDestroyTimer(int platformTimerId);
#if defined(VTK_USE_TDX) && defined(Q_WS_WIN)
	vtkTDxWinDevice* Device;
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_MAC)
	vtkTDxMacDevice* Device;
#endif
#if defined(VTK_USE_TDX) && defined(Q_WS_X11)
	vtkTDxUnixDevice* Device;
#endif

private:
	QVTKInteractorInternal* Internal;

	// unimplemented copy
	QVTKInteractor(const QVTKInteractor&);
	// unimplemented operator=
	void operator=(const QVTKInteractor&);
};

// .NAME QVTKInteractorAdapter - A QEvent translator.
// .SECTION Description
// QVTKInteractorAdapter translates QEvents and send them to a
// vtkRenderWindowInteractor.
class QVTKInteractorAdapter : public QObject
{
	Q_OBJECT
public:
	// Description:
	// Constructor: takes QObject parent
	QVTKInteractorAdapter(QObject* parent = nullptr);

	// Description:
	// Destructor
	~QVTKInteractorAdapter();

	// Description:
	// Process a QEvent and send it to the interactor
	// returns whether the event was recognized and processed
	bool ProcessEvent(QEvent* e, vtkRenderWindowInteractor* iren);
};

// class for managing a single VTK/Qt connection
// not to be included in other projects
// only here for moc to process for vtkEventQtSlotConnect
class vtkQtConnection : public QObject
{
	Q_OBJECT

public:
	// constructor
	vtkQtConnection(vtkEventQtSlotConnect* owner);

	// destructor, disconnect if necessary
	~vtkQtConnection();

	// print function
	void PrintSelf(ostream& os, vtkIndent indent);

	// callback from VTK to emit signal
	void Execute(vtkObject* caller, unsigned long event, void* client_data);

	// set the connection
	void SetConnection(vtkObject* vtk_obj, unsigned long event, const QObject* qt_obj, const char* slot, void* client_data, float priority = 0.0, Qt::ConnectionType type = Qt::AutoConnection);

	// check if a connection matches input parameters
	bool IsConnection(vtkObject* vtk_obj, unsigned long event, const QObject* qt_obj, const char* slot, void* client_data);

	static void DoCallback(vtkObject* vtk_obj, unsigned long event, void* client_data, void* call_data);

signals:
	// the qt signal for moc to take care of
	void EmitExecute(vtkObject*, unsigned long, void* client_data, void* call_data, vtkCommand*);

protected slots:
	void deleteConnection();

protected:
	// the connection information
	vtkObject* VTKObject;
	vtkCallbackCommand* Callback;
	const QObject* QtObject;
	void* ClientData;
	unsigned long VTKEvent;
	QString QtSlot;
	vtkEventQtSlotConnect* Owner;

private:
	vtkQtConnection(const vtkQtConnection&);
	void operator=(const vtkQtConnection&);
};

// hold all the connections
class vtkQtConnections : public std::vector<vtkQtConnection*>
{
};

// manage connections between VTK object events and Qt slots
class vtkEventQtSlotConnect : public vtkObject
{
public:
	static vtkEventQtSlotConnect* New();
	vtkTypeMacro(vtkEventQtSlotConnect, vtkObject)

	  // Description:
	  // Print the current connections between VTK and Qt
	  void PrintSelf(ostream& os, vtkIndent indent);

	// Description:
	// Connect a vtk object's event with a Qt object's slot.  Multiple
	// connections which are identical are treated as separate connections.
	virtual void
	Connect(vtkObject* vtk_obj, unsigned long event, const QObject* qt_obj, const char* slot, void* client_data = nullptr, float priority = 0.0, Qt::ConnectionType type = Qt::AutoConnection);

	// Description:
	// Disconnect a vtk object from a qt object.
	// Passing no arguments will disconnect all slots maintained by this object.
	// Passing in only a vtk object will disconnect all slots from it.
	// Passing only a vtk object and event, will disconnect all slots matching
	// the vtk object and event.
	// Passing all information in will match all information.
	virtual void Disconnect(vtkObject* vtk_obj = nullptr, unsigned long event = vtkCommand::NoEvent, const QObject* qt_obj = nullptr, const char* slot = 0, void* client_data = nullptr);

	// Description:
	// Allow to query vtkEventQtSlotConnect to know if some Connect() have been
	// setup and how many.
	virtual int GetNumberOfConnections() const;

protected:
	vtkQtConnections* Connections;
	friend class vtkQtConnection;
	void RemoveConnection(vtkQtConnection*);

	vtkEventQtSlotConnect();
	~vtkEventQtSlotConnect();

private:
	// unimplemented
	vtkEventQtSlotConnect(const vtkEventQtSlotConnect&);
	void operator=(const vtkEventQtSlotConnect&);
};


// internal class, do not use
class QVTKInteractorInternal : public QObject
{
	Q_OBJECT
public:
	QVTKInteractorInternal(QVTKInteractor* p);
	~QVTKInteractorInternal();
public Q_SLOTS:
	void TimerEvent(int id);

public:
	QSignalMapper* SignalMapper;
	typedef std::map<int, QTimer*> TimerMap;
	TimerMap Timers;
	QVTKInteractor* Parent;
};


#endif
