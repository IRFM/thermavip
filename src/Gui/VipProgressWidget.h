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

#ifndef VIP_PROGRESS_WIDGET_H
#define VIP_PROGRESS_WIDGET_H

#include "VipStandardWidgets.h"
#include "VipDisplayArea.h"
#include "VipProgress.h"
#include "VipMimeData.h"
#include "VipIODevice.h"

#include <QThread>
#include <QApplication>

class VipProgressWidget;

/// @brief Internal widget used by VipProgressWidget.
/// Can be modified through style sheets.
class VipProgressWidgetInternal : public QFrame
{
	Q_OBJECT
	friend class VipProgressWidget;

public:
	VipProgressWidgetInternal(VipProgress* p, VipProgressWidget* widget, QWidget* parent = nullptr);
	~VipProgressWidgetInternal();

private:
	void setProgress(VipProgress* p);
	void setProgressBarVisible(bool visible);
	bool progressBarVisible() const;

	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief A progress bar displayed on top of a VipDisplayPlayerArea (workspace)
///
/// VipProgressWidget is always created as a child of a  VipProgressWidget.
/// It will then cover the full workspace area in order to block all user inputs,
/// and display one or more progress bars.
/// 
/// VipProgressWidget is created by passing a thread identifier corresponding 
/// to an asynchronous operation. All VipProgress instances created within this thread
/// will be displayed in the VipProgressWidget instead of the main progress manager.
/// 
/// This is a way to only block the workspace when doing a workspace-only related feature
/// (like video time trace extraction) instead of blocking the full UI.
/// 
/// A VipProgressWidget can only be created using VipProgressWidget::async() static member.
/// 
class VipProgressWidget : public QLabel
{
	Q_OBJECT

	VipProgressWidget(VipDisplayPlayerArea* lock_parent, QThread* for_thread = nullptr);

	struct Cleanup
	{
		template<class... Args>
		void operator()(Args... args) const noexcept{}
	};
public:
	
	
	/// @brief Launch an async operation and block the parent workspace during this operation.
	/// @param locked_widget 
	/// @param in_thread asynchronous operation to be perform in a new thread
	/// @param gui_thread operation to be performed in the main thread after the async one.
	/// This operation takes as parameter the result of the async one.
	/// @param cl Cleanup function, only called if the parent workspace is destroyed during the operation.
	/// @return created thread
	/// 
	/// VipProgressWidget::async will launch an asynchronous operation (in_thread) in a new thread, 
	/// and another operation (gui_thread) in the main thread when the async one is finished.
	/// 
	/// In the meantime, the passed workspace is locked with a VipProgressWidget displayed on top of it.
	/// 
	/// This function handle potential workspace destruction during the asynchronous operation.
	/// 
	template<class F1, class F2, class Clean >
	static QThread* async(QPointer<VipDisplayPlayerArea> locked_widget, F1 in_thread, F2 gui_thread, Clean cl )
	{
		VipProgressWidget* progress = new VipProgressWidget(locked_widget);

		auto fun = [in_thread, gui_thread, cl, progress, locked_widget]() {
			QMutexLocker lock(locked_widget->closeMutex());
			if constexpr (std::is_void_v<decltype(in_thread())>) {
				in_thread();
				auto f2 = [gui_thread, cl, progress, locked_widget]() {
					auto* tmp = qobject_cast<VipDisplayPlayerArea*>(locked_widget.data());
					if (tmp)
						gui_thread();
					else
						cl();
					progress->deleteLater();
				};
				if (locked_widget )
					QMetaObject::invokeMethod(locked_widget, f2, Qt::QueuedConnection);
			}
			else {
				auto ret = in_thread();
				auto f2 = [ret, gui_thread, cl, progress, locked_widget]() {
					auto* tmp = qobject_cast<VipDisplayPlayerArea*>(locked_widget.data());
					if (tmp)
						gui_thread(ret);
					else
						cl(ret);
					progress->deleteLater();
				};
				
				if (locked_widget )
					QMetaObject::invokeMethod(locked_widget, f2, Qt::QueuedConnection);
			}
		};
		QThread* th = QThread::create(fun);
		progress->setThread(th);

		QObject::connect(th, &QThread::finished, th, &QThread::deleteLater);
		th->start();
		return th;
	}
	template<class F1, class F2>
	static QThread* async(QPointer<VipDisplayPlayerArea> locked_widget, F1 in_thread, F2 gui_thread)
	{
		return async(locked_widget, in_thread, gui_thread, Cleanup{});
	}

	~VipProgressWidget();

public Q_SLOTS:
	void addProgress(QObjectPointer);
	void removeProgress(QObjectPointer);
	void setText(QObjectPointer, const QString& text);
	void setValue(QObjectPointer, int value);
	void setCancelable(QObjectPointer, bool cancelable);
	void setModal(QObjectPointer, bool ) {} // does nothing, we are not modal by definition
	void cancelRequested();

protected:
	bool eventFilter(QObject* obj, QEvent* evt);

private:
	void recomputeCentralWidgetSize();
	void setThread(QThread*);

	VIP_DECLARE_PRIVATE_DATA();
};


/// @brief Launch an asynchronous operation when dropping a QMimeData on given target.
/// Returns true if the mime data is a VipAsyncMimeDataLazyEvaluation and the mime data
/// is a (sub)child of a VipDisplayPlayerArea (workspace), returns false otherwise.
/// 
template<class T, class F>
bool vipHandleAsyncDrop(T* target_handler, F handle_drop, const QMimeData* mime)
{
	if (!target_handler)
		return false;

	if (auto* m = qobject_cast<const VipAsyncMimeDataLazyEvaluation*>(mime)) {
		if (auto* wks = VipDisplayPlayerArea::fromChild(m)) {

			QPointer<T> handler = target_handler;
			auto coord = m->coordinateSystemType();
			QPointer<VipDisplayPlayerArea> pwks = wks;

			auto fun1 = m->function();
			auto fun2 = [handle_drop, handler, pwks, coord](const VipProcessingObjectList& proc) {
				if (!handler || !pwks) {
					qDeleteAll(proc);
					return;
				}

				// Since the processings where created in another thread,
				// they do not have a parent. Therefore, reset their parents
				// to the workspace processing pool.
				for (VipProcessingObject* p : proc) {
					auto src = p->allSources();
					p->moveToThread(qApp->thread());
					p->setParent(pwks->processingPool());
					for (VipProcessingObject* s : src) {
						s->moveToThread(qApp->thread());
						s->setParent(pwks->processingPool());
					}
				}

				VipMimeDataProcessingObjectList lst(coord, pwks);
				lst.setProcessing(proc);
				//handler->dropMimeData(&lst);
				handle_drop(handler.data(), &lst);
			};

			VipProgressWidget::async(wks, fun1, fun2, [](const auto& procs) { qDeleteAll(procs); });
			return true;
		}
	}

	return false;
}

template<class T>
bool vipHandleAsyncDrop(T* target_handler, const QMimeData* mime)
{
	return vipHandleAsyncDrop(target_handler, [](T* handler, const QMimeData* m) { handler->dropMimeData(m); }, mime);
}

#endif