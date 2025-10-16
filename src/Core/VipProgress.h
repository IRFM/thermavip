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

#ifndef VIP_PROGRESS_H
#define VIP_PROGRESS_H

#include "VipConfig.h"
#include "VipCore.h"
#include <QObject>

/// \addtogroup Core
/// @{

class VipProgress;

/// Default progress manager for VipProgress.
/// Any progress manager set with VipProgress::setProgressManager must define the same functions as slots.
/// When using Thermavip GUI (basically all the time), the progress manager is set to the VipMultiProgressWidget tool widget.
class VIP_CORE_EXPORT DefaultProgressManager : public QObject
{
	Q_OBJECT
public:
	DefaultProgressManager(QObject* parent = nullptr)
	  : QObject(parent)
	{
	}

public Q_SLOTS:
	void addProgress(QObjectPointer) {}
	void removeProgress(QObjectPointer) {}
	void setText(QObjectPointer, const QString& text) { Q_UNUSED(text) }
	void setValue(QObjectPointer, int value) { Q_UNUSED(value) }
	void setCancelable(QObjectPointer, bool cancelable) { Q_UNUSED(cancelable) }
	void setModal(QObjectPointer, bool modal) { Q_UNUSED(modal) }
};

/// VipProgress is used to display an operation progress.
/// Each you define a time consuming operation (for instance extracting the maximum pixel value in ROI inside a whole movie), you should use
/// a VipProgress object to notify the current operation progress.
///
/// Use VipProgress::setRange to define steps in your operation and VipProgress::setValue to notify the current progress with the range.
/// Use VipProgress::setText to provide an status information on the current operation.
/// Use VipProgress::setCancelable if the operation can be interrupted midway.
/// Use VipProgress::setModal if the operation should block all user inputs.
///
/// All call to VipProgress's functions are redirected to the current progress manager as returned by VipProgress::progressManager.
/// Any progress manager set with VipProgress::setProgressManager must define the same interface as DefaultProgressManager.
/// When using Thermavip' GUI (almost all the times), the progress manager is set to the VipMultiProgressWidget tool widget  which displays a progress bar and optionally a cancel button.
///
/// VipProgress can be used within any thread. You should not worry about updating the VipMultiProgressWidget widget or blocking/overloading the GUI event loop,
/// the class takes care of this internally.
///
/// You can create and use several VipProgress simultaneously in the same and/or different threads.
class VIP_CORE_EXPORT VipProgress : public QObject
{
	Q_OBJECT

public:
	VipProgress(double min = 0, double max = 100, const QString& text = QString());
	~VipProgress();

	/// Returns the progress minimum range value
	double min() const;
	/// Returns the progress maximum range value
	double max() const;
	/// Returns the current progress text
	QString text() const;
	/// Returns the progress value
	double value() const;
	/// Returns true if the current operation is cancelable
	bool isCancelable() const;
	/// Returns true if the current operation is modal
	bool isModal() const;
	/// Returns true if the current operation has been canceled by the user
	bool canceled() const;

	/// Set the progress manager. It must follow the same class pattern as DefaultProgressManager.
	static void setProgressManager(QObject* manager);
	static void resetProgressManager();
	/// Returns the progress manager
	static QObject* progressManager();

public Q_SLOTS:
	/// Set the current operation progress range
	void setRange(double min, double max);
	/// Set the operation status text
	void setText(const QString& text);
	/// Set the current progress value
	void setValue(double value);
	/// Set the current operation cancelable or not
	void setCancelable(bool cancelable);
	/// Set the current operation modal or not
	void setModal(bool modal);

	void cancelRequested();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// @}
// end Core

#endif
