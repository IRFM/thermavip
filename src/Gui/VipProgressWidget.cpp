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

#include "VipProgressWidget.h"
#include "VipToolWidget.h"
#include "VipDisplayArea.h"

#include <QProgressBar>
#include <QGridLayout>
#include <QEvent>


class VipProgressWidgetInternal::PrivateData
{
public:
	QLabel text;
	QProgressBar progressBar;
	QToolButton cancel;
	QPointer<VipProgress> progress;
	VipProgressWidget* widget;
};


VipProgressWidgetInternal::VipProgressWidgetInternal(VipProgress* p, VipProgressWidget* widget, QWidget* parent )
	: QFrame(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->progress = p;
	d_data->widget = widget;

	QGridLayout* lay = new QGridLayout();
	lay->addWidget(&d_data->text, 0, 0);
	lay->addWidget(&d_data->progressBar, 1, 0);
	lay->addWidget(&d_data->cancel, 0, 1);

	setLayout(lay);
	lay->setContentsMargins(2, 2, 2, 2);

	d_data->text.setWordWrap(true);

	d_data->cancel.setAutoRaise(true);
	d_data->cancel.setToolTip("Stop this operation");
	d_data->cancel.setIcon(vipIcon("cancel.png"));
	d_data->cancel.hide();

	d_data->progressBar.setRange(0, 100);
	d_data->progressBar.setMaximumHeight(20);
	d_data->progressBar.setMinimumHeight(20);
	setProgressBarVisible(false);
	d_data->progressBar.hide();

	if (p)
		connect(&d_data->cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
	if (widget)
		connect(&d_data->cancel, SIGNAL(clicked(bool)), widget, SLOT(cancelRequested()));

	this->setMinimumWidth(300);
}

VipProgressWidgetInternal::~VipProgressWidgetInternal() {}

void VipProgressWidgetInternal::setProgress(VipProgress* p)
{
	if (p != d_data->progress) {
		if (d_data->progress)
			disconnect(&d_data->cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
		d_data->progress = p;
		if (d_data->progress)
			connect(&d_data->cancel, SIGNAL(clicked(bool)), p, SLOT(cancelRequested()));
	}
}

void VipProgressWidgetInternal::setProgressBarVisible(bool visible)
{
	if (visible != d_data->progressBar.isVisible()) {
		d_data->progressBar.setVisible(visible);
		QGridLayout* lay = static_cast<QGridLayout*>(layout());
		if (visible) {
			lay->removeWidget(&d_data->cancel);
			lay->addWidget(&d_data->cancel, 1, 1);
		}
		else {
			lay->removeWidget(&d_data->cancel);
			lay->addWidget(&d_data->cancel, 0, 1);
		}
	}
}

bool VipProgressWidgetInternal::progressBarVisible() const
{
	return d_data->progressBar.isVisible();
}


class VipProgressWidget::PrivateData
{
public:
	QList<VipProgressWidgetInternal*> progresses;
	QList<VipProgressWidgetInternal*> reuse;
	QLabel status;
	QSet<VipProgressWidgetInternal*> modalWidgets;
	QBoxLayout* layout;
	QWidget* central;
	QPointer < VipDisplayPlayerArea> parent;

	VipProgressWidgetInternal* find(VipProgress* p) const
	{
		for (int i = 0; i < progresses.size(); ++i)
			if (progresses[i]->d_data->progress == p)
				return progresses[i];
		return nullptr;
	}
};

VipProgressWidget::VipProgressWidget(VipDisplayPlayerArea* lock_parent, QThread* for_thread)
  : QLabel(lock_parent)
{
	
	lock_parent->setProgressWidget(this);

	VIP_CREATE_PRIVATE_DATA();
	d_data->parent = lock_parent;

	d_data->central = new QWidget(this);
	d_data->status.setText("No operation to display at this time");

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&d_data->status);
	lay->setSpacing(2);

	d_data->central->setLayout(d_data->layout = lay);

	d_data->reuse.append(new VipProgressWidgetInternal(nullptr, this));
	d_data->layout->addWidget(d_data->reuse.last());
	d_data->reuse.last()->hide();
	d_data->central->resize(300, 100);

	lock_parent->installEventFilter(this);

	//this->setStyleSheet("VipProgressWidget {background-color: rgba(0,0,0,50);}");
	this->resize(lock_parent->size());
	this->show();
	this->raise();
	recomputeCentralWidgetSize();

	if (for_thread)
		setThread( for_thread);
}

VipProgressWidget::~VipProgressWidget()
{
	parent()->removeEventFilter(this);
	vipGetMultiProgressWidget()->removeProgressWidget(this);
}

void VipProgressWidget::setThread(QThread* th)
{
	vipGetMultiProgressWidget()->addProgressWidget(this, th);
}

bool VipProgressWidget::eventFilter(QObject* obj, QEvent* evt)
{
	if (evt->type() == QEvent::Resize) {
		this->resize(parentWidget()->size());
		recomputeCentralWidgetSize();
	}
	return false;
}

void VipProgressWidget::recomputeCentralWidgetSize()
{
	QSize s = size();
	if (s.width() > 300)
		s.setWidth(300);
	s.setHeight(100);

	QPoint pt;
	pt.setX((width() - s.width()) / 2);
	pt.setY((height() - s.height()) / 2);

	d_data->central->move(pt);
	d_data->central->resize(s);
}

void VipProgressWidget::addProgress(QObjectPointer ptr)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		VipProgressWidgetInternal* w = d_data->reuse.size() ? d_data->reuse.first() : new VipProgressWidgetInternal(p, this);
		w->setProgress(p);

		if (d_data->reuse.size())
			d_data->reuse.removeAt(0);
		else
			d_data->layout->addWidget(w);

		d_data->status.hide();
		d_data->progresses.append(w);
		d_data->progresses.back()->d_data->progressBar.setRange(0, 100); // TEST// p->min(), p->max());
		d_data->progresses.back()->d_data->text.setText(p->text());

		d_data->progresses.back()->show();
		recomputeCentralWidgetSize();
	}
}

void VipProgressWidget::removeProgress(QObjectPointer ptr)
{
	VipProgress* p = qobject_cast<VipProgress*>(ptr.data());

	// remove all invalid progress bar
	for (int i = 0; i < d_data->progresses.size(); ++i) {
		VipProgressWidgetInternal* w = d_data->progresses[i];
		if (w->d_data->progress == p || !w->d_data->progress) {
			d_data->modalWidgets.remove(w);
			d_data->progresses.removeAt(i);
			w->d_data->progressBar.hide();
			w->d_data->progressBar.setValue(0);
			w->d_data->text.setText(QString());

			if (d_data->reuse.indexOf(w) < 0)
				d_data->reuse.append(w);
			w->hide();

			i--;
		}
	}

	// update status text visibility
	d_data->status.setVisible(d_data->progresses.size() == 0);

	if (d_data->progresses.size() == 0) {
		this->hide();
	}
	recomputeCentralWidgetSize();
}
void VipProgressWidget::setText(QObjectPointer ptr, const QString& text)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (VipProgressWidgetInternal* w = d_data->find(p)) {
			if (text.size() && w->d_data->text.isHidden()) {
				w->d_data->text.show();
			}

			w->d_data->text.setText(text);
			recomputeCentralWidgetSize();
		}
	}
}

void VipProgressWidget::setValue(QObjectPointer ptr, int value)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (VipProgressWidgetInternal* w = d_data->find(p)) {
			if (w->d_data->progressBar.isHidden()) {
				w->setProgressBarVisible(true);
				recomputeCentralWidgetSize();
			}
			w->d_data->progressBar.setValue(value);
		}
	}
}

void VipProgressWidget::setCancelable(QObjectPointer ptr, bool cancelable)
{
	if (VipProgress* p = qobject_cast<VipProgress*>(ptr.data())) {
		if (VipProgressWidgetInternal* w = d_data->find(p)) {
			w->d_data->cancel.setVisible(cancelable);
			recomputeCentralWidgetSize();
		}
	}
}

void VipProgressWidget::cancelRequested()
{
	// make sure to cancel all sub operations
	// bool start_cancel = false;
	for (int i = 0; i < d_data->progresses.size(); ++i) {
		// TEST: comment
		// if (start_cancel) {
		if (d_data->progresses[i]->d_data->progress)
			d_data->progresses[i]->d_data->progress->cancelRequested();
		//}
		// else if (sender() == &d_data->progresses[i]->cancel) {
		//	start_cancel = true;
		//}
	}
}