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

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <thread>

#include "VipCore.h"
#include "VipDisplayObject.h"
#include "VipSleep.h"
#include "VipUniqueId.h"
#include "VipXmlArchive.h"

#include <QCoreApplication>
#include <QWaitCondition>
#include <QGraphicsView>
#include <qtimer.h>

namespace Vip
{
	namespace detail
	{
		struct ItemAndData
		{
			VipDisplayObject* item;
			VipAnyDataList data;
		};

		// Small class used to speedup plot items display
		// by gathering calls to VipDisplayObject::display()
		// and unloading the main event loop.
		class ItemDirtyNotifier
		{
			bool pendingDirty{ false };
			QMutex lock;
			QVector<ItemAndData> dirtyItems;

		public:
			// Mark the item as dirty.
			// Only goes through the event loop if it the first one
			// on its plotting area to be marked as dirty.
			VIP_ALWAYS_INLINE void markDirty(VipDisplayObject* item, const VipAnyDataList& data)
			{
				QMutexLocker ll(&lock);
				dirtyItems.push_back(ItemAndData{ item, data });
				if (!pendingDirty) {
					pendingDirty = true;
					QMetaObject::invokeMethod(item, "display", Qt::QueuedConnection, Q_ARG(VipAnyDataList, data));
				}
			}

			// Retrieve/clear dirty items
			VIP_ALWAYS_INLINE QVector<ItemAndData> dirtItems()
			{
				QMutexLocker ll(&lock);
				QVector<ItemAndData> res = std::move(dirtyItems);
				pendingDirty = false;
				return res;
			}
		};
	}

}


class VipDisplayObject::PrivateData
{
public:
	PrivateData()
	  : displayInProgress(false)
	  , isDestruct(false)
	  , formattingEnabled(true)
	  , visible(true)
	  , first(true)
	  , updateOnHidden(false)
	  , previousDisplayTime(0)
	  , lastTitleUpdate(0)
	  , lastVisibleUpdate(0)
	{
	}

	std::atomic<bool> displayInProgress;
	bool isDestruct;
	bool formattingEnabled;
	bool visible;
	bool first;
	bool updateOnHidden;
	QString playerTitle; // update parent VipAbstractPlayer title
	qint64 previousDisplayTime;
	qint64 lastTitleUpdate;
	qint64 lastVisibleUpdate;

	QPointer<VipAbstractPlotArea> area;

	//VipSpinlock lock;
	//std::condition_variable_any cond;
	QMutex lock;
	QWaitCondition cond;
};

VipDisplayObject::VipDisplayObject(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	this->setScheduleStrategies(Asynchronous);
	inputAt(0)->setListType(VipDataList::FIFO, VipDataList::None);
	propertyAt(0)->setData(1);
}

VipDisplayObject::~VipDisplayObject()
{
	d_data->isDestruct = true;
	if (inputAt(0)->connection()->source()) {
		this->wait();
	}
}

void VipDisplayObject::checkVisibility()
{
	d_data->visible = isVisible();
}

static void processEvents()
{
	// Wait for the event loop to process events
	static QMutex mutex;

	qint64 start = QDateTime::currentMSecsSinceEpoch();
	bool r = mutex.tryLock(100);
	if (!r) 
		// Wait at most 100 ms
		return;
	
	std::lock_guard<QMutex> lock(mutex, std::adopt_lock_t{});
	qint64 el = QDateTime::currentMSecsSinceEpoch() - start;
	if (el < 5) {
		// Short time to acquire the lock: process events
		vipProcessEvents(nullptr, 100);
	}

	// Long time to acquire the lock:
	// another thread is currently waiting for the event loop
}

void VipDisplayObject::apply()
{
	if (d_data->isDestruct)
		return;

	// check display visibility every 200ms
	qint64 time = QDateTime::currentMSecsSinceEpoch();
	if (time - d_data->lastVisibleUpdate > 200) {
		d_data->lastVisibleUpdate = time;
		if (QThread::currentThread() == QCoreApplication::instance()->thread())
			checkVisibility();
		else
			QMetaObject::invokeMethod(this, "checkVisibility", Qt::QueuedConnection);
	}
	if (!d_data->visible && !d_data->updateOnHidden) {
		// clear input buffer
		inputAt(0)->allData();
		return;
	}
	if (!isEnabled() || !inputAt(0)->hasNewData())
		return;

	const VipAnyDataList buffer = inputAt(0)->allData();
	d_data->displayInProgress = true;

	if (!this->prepareForDisplay(buffer)) {

		if ((QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()))
			//display in the GUI thread
			this->display(buffer);
		else {

			// Try to gather several items for display() call 
			auto notifier = d_data->area ? d_data->area->notifier() : Vip::detail::ItemDirtyNotifierPtr();
			if (notifier)
				notifier->markDirty(this, buffer);
			else
				QMetaObject::invokeMethod(this, "display", Qt::QueuedConnection, Q_ARG(VipAnyDataList, buffer));

			// Wait for the display to end while processing events from the main event loop.
			// This ensures that, whatever the display rate, the GUI remains responsive.
			std::lock_guard<QMutex> ll(d_data->lock);
			while (d_data->displayInProgress.load(std::memory_order_relaxed) && !d_data->isDestruct) {
				bool ret = d_data->cond.wait(&d_data->lock, 5);
				qint64 current = QDateTime::currentMSecsSinceEpoch();
				if ((current - time) > 50) {
					processEvents();
					break;
				}
				else if (!ret && buffer.size() > 1)
					processEvents();
			}
		}
	}
	else {
		Q_EMIT displayed(buffer);
		d_data->displayInProgress = false;
	}
}

static QWidget* findWidgetWith_automaticWindowTitle(QWidget* w)
{
	while (w) {

		if (w->metaObject()->indexOfProperty("automaticWindowTitle") >= 0)
			return w;
		w = w->parentWidget();
	}
	return nullptr;
}

void VipDisplayObject::display(const VipAnyDataList& data)
{
	if (d_data->isDestruct)
		return;

	QVector<Vip::detail::ItemAndData> items;
	if (auto * a = d_data->area.data())
		if (auto notifier = a->notifier())
			// Get all dirty items
			items = notifier->dirtItems();
	
	const Vip::detail::ItemAndData one{ this, data };
	const Vip::detail::ItemAndData* items_p = &one;
	qsizetype count = 1;

	if (!items.isEmpty()) {
		count = items.size();
		items_p = items.constData();
	}

	for (qsizetype i = 0; i < count; ++i) {
		// Process all dirty items in one loop

		const VipAnyDataList& dat = items_p[i].data;
		VipDisplayObject* disp = const_cast <VipDisplayObject*>(items_p[i].item);

		// update parent VipAbstractPlayer title every 500 ms (no need for more in case of streaming)
		qint64 time = QDateTime::currentMSecsSinceEpoch();
		if (time - d_data->lastTitleUpdate > 500) {
			disp->d_data->lastTitleUpdate = time;
			const VipAnyData data = dat.size() ? dat.back() : VipAnyData();
			if (data.hasAttribute("Name") || data.hasAttribute("PlayerName")) {
				QString title = data.name();
				QString title2 = data.attribute("PlayerName").toString();
				if (!title2.isEmpty())
					title = title2;
				if (disp->d_data->playerTitle != title) {
					QWidget* player = findWidgetWith_automaticWindowTitle(widget());
					if (player && !title.isEmpty()) {
						if (player->property("automaticWindowTitle").toBool()) {
							// vip_debug("set window title\n");
							QMetaObject::invokeMethod(player, "setWindowTitle", Qt::AutoConnection, Q_ARG(QString, title));
						}
						disp->d_data->playerTitle = title;
					}
				}
			}
		}

		disp->displayData(dat);
		Q_EMIT disp->displayed(dat);

		disp->d_data->displayInProgress = false;
		disp->d_data->cond.notify_one();
	}
}

bool VipDisplayObject::displayInProgress() const
{
	return d_data->displayInProgress.load(std::memory_order_relaxed);
}

void VipDisplayObject::setFormattingEnabled(bool enable)
{
	d_data->formattingEnabled = enable;
}
bool VipDisplayObject::formattingEnabled() const
{
	return d_data->formattingEnabled;
}

void VipDisplayObject::setUpdateOnHidden(bool enable)
{
	d_data->updateOnHidden = enable;
}
bool VipDisplayObject::updateOnHidden() const
{
	return d_data->updateOnHidden;
}


VipFunctionDispatcher<2>& VipFDDisplayObjectSetItem()
{
	static VipFunctionDispatcher<2> inst;
	return inst;
}


class VipDisplayPlotItem::PrivateData
{
public:
	PrivateData()
	  : itemSuppressable(false)
	  , last_format(0)
	{
		format_timer.setSingleShot(true);
	}
	bool itemSuppressable;
	VipLazyPointer item;

	QString fxUnit;
	QString fyUnit;
	QString fTitle;
	QString fzUnit;

	QPointer<VipPlotItem> format_item;
	VipAnyData format_any;
	QTimer format_timer;
	qint64 last_format;
};

VipDisplayPlotItem::VipDisplayPlotItem(QObject* parent)
  : VipDisplayObject(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	connect(&d_data->format_timer, SIGNAL(timeout()), this, SLOT(internalFormatItem()));
}

VipDisplayPlotItem::~VipDisplayPlotItem()
{
	VipPlotItem* c = d_data->item.data<VipPlotItem>();
	if (c)
		c->setProperty("VipDisplayObject", QVariant());
}

VipPlotItem* VipDisplayPlotItem::item() const
{
	bool found = false;
	VipPlotItem* item = d_data->item.data<VipPlotItem>(&found);
	if (found) {
		// First access to the item: initialize
		QMetaObject::invokeMethod(const_cast<VipDisplayPlotItem*>(this), "setItemProperty", Qt::AutoConnection);
		connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		connect(item, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(axesChanged(VipPlotItem*)));
		const_cast < VipDisplayPlotItem*>(this)->axesChanged(item);
		item->setItemAttribute(VipPlotItem::IsSuppressable, d_data->itemSuppressable);

		// This function ight not be called from the main thread, so use delayed call
		if (QThread::currentThread() != qApp->thread()) {

			VipDisplayPlotItem* _this = const_cast<VipDisplayPlotItem*>(this);
			QObjectPointer display(_this);
			QObjectPointer plot(item);
			QMetaObject::invokeMethod(
			  _this,
			  [display,plot]() {
				  if (display && plot && static_cast<VipDisplayObject*>(display.get())->widget())
					  VipFDDisplayObjectSetItem().callAllMatch(display.get(), plot.get());
			  },
			  Qt::QueuedConnection);
		}
		else if (this->widget())
			VipFDDisplayObjectSetItem().callAllMatch(this, item);
	}
	return item;
}

void VipDisplayPlotItem::setItemProperty()
{
	if (VipPlotItem* it = this->item())
		it->setProperty("VipDisplayObject", QVariant::fromValue(this));
}

void VipDisplayPlotItem::formatItemIfNecessary(VipPlotItem* item, const VipAnyData& any)
{
	qint64 current = QDateTime::currentMSecsSinceEpoch();
	if (current - d_data->last_format > 500) {
		// it's been a long time, let's format!
		formatItem(item, any);
	}
	else {
		d_data->format_item = item;
		d_data->format_any = any;
		// restart timer
		d_data->format_timer.start(500);
	}
}

void VipDisplayPlotItem::internalFormatItem()
{
	if (VipPlotItem* it = d_data->format_item)
		formatItem(it, d_data->format_any);
	d_data->last_format = QDateTime::currentMSecsSinceEpoch();
}

VipPlotItem* VipDisplayPlotItem::takeItem()
{
	if (VipPlotItem* it = this->item()) {
		disconnect(it, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()));
		disconnect(it, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		disconnect(it, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(axesChanged(VipPlotItem*)));
		it->setProperty("VipDisplayObject", QVariant());
		d_data->item.setData<VipPlotItem>(nullptr);
		return it;
	}
	return nullptr;
}

void VipDisplayPlotItem::setItem(VipPlotItem* item)
{

	if (VipPlotItem* it = this->item()) {
		disconnect(it, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()));
		disconnect(it, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		disconnect(it, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(axesChanged(VipPlotItem*)));
		delete it;
	}

	d_data->item.setData(item);
	if (item) {
		item->setProperty("VipDisplayObject", QVariant::fromValue(this));
		connect(item, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()), Qt::DirectConnection);
		connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		connect(item, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(axesChanged(VipPlotItem*)));

		item->setItemAttribute(VipPlotItem::IsSuppressable, d_data->itemSuppressable);
		axesChanged(item);

		if (widget())
			VipFDDisplayObjectSetItem().callAllMatch(this, item);
	}
}


void VipDisplayPlotItem::axesChanged(VipPlotItem* it)
{
	if (auto* a = it->area()) {
		this->VipDisplayObject::d_data->area = a;
		auto notifier = a->notifier();
		if (!notifier)
			a->setNotifier(Vip::detail::ItemDirtyNotifierPtr::create());
		
	}
	this->checkVisibility();
}


void VipDisplayPlotItem::formatItem(VipPlotItem* item, const VipAnyData& data, bool force)
{
	static const QString XUnit = "XUnit";
	static const QString YUnit = "YUnit";
	static const QString ZUnit = "ZUnit";
	static const QString stylesheet = "stylesheet";

	if (!formattingEnabled())
		return;

	if (!force && (item->axes().size() < 2 || !item->axes().constFirst() || !item->axes().constLast()))
		return;

	const QVariantMap this_attrs = this->attributes();
	QVariantMap attrs = data.attributes();
	// merge attributes
	for (QVariantMap::const_iterator it = this_attrs.begin(); it != this_attrs.end(); ++it)
		attrs.insert(it.key(), it.value());

	QVariantMap::const_iterator name = attrs.end();
	QVariantMap::const_iterator xunit = name;
	QVariantMap::const_iterator yunit = name;
	QVariantMap::const_iterator zunit = name;

	// apply style sheet
	QVariantMap::const_iterator st = attrs.find(stylesheet);
	if (st != attrs.end()) {
		QString _stylesheet = st.value().toString();
		if (!_stylesheet.isEmpty())
			item->setStyleSheet(_stylesheet);
	}

	// set the item name
	// if (st_keys.find("title") == st_keys.end() )
	{
		//'fixed_title' is an item property containing the item's title defined by the user through the user interface.
		// if defined, it cannot be changed.
		name = attrs.find("Name");
		if (name != attrs.end()) {
			QString n = name.value().toString();
			if (n != d_data->fTitle) { //(item->title().isEmpty() || item->title().text() != n) && !n.isEmpty())
				item->setTitle(VipText(n, item->title().textStyle()));
				d_data->fTitle = n;
			}
		}
	}

	// set the item's x and y unit
	// if(st_keys.find("axisunit[0]") == st_keys.end() )
	{
		xunit = attrs.find(XUnit);
		if (xunit != attrs.end()) {
			VipText t = item->axisUnit(0);
			QString xu = xunit.value().toString();
			if (t.isEmpty() && xu != d_data->fxUnit) {
				item->setAxisUnit(0, VipText(xu, t.textStyle()));
				d_data->fxUnit = xu;
			}
		}
	}
	// if (st_keys.find("axisunit[1]") == st_keys.end() )
	{
		yunit = attrs.find(YUnit);
		if (yunit != attrs.end()) {
			VipText t = item->axisUnit(1);
			QString yu = yunit.value().toString();
			if (t.isEmpty() && yu != d_data->fyUnit) {
				item->setAxisUnit(1, VipText(yu, t.textStyle()));
				d_data->fyUnit = yu;
			}
		}
	}

	// set the color map unit
	if (item->colorMap()) {
		{
			zunit = attrs.find(ZUnit);
			if (zunit != attrs.end()) {
				VipText t = item->colorMap()->title();
				QString new_title = zunit.value().toString();
				if (t.text() != new_title && new_title != d_data->fzUnit) {
					// if(VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(this->widget()))
					//	pl->viewer()->area()->colorMapAxis()->setTitle(VipText(new_title, t.textStyle()));
					// else
					item->colorMap()->setTitle(VipText(new_title, t.textStyle()));
					d_data->fzUnit = new_title;
				}
			}
		}
	}

	// set the other attributes
	for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		if (it.key().endsWith("Unit")) {
			if (it.key() == XUnit || it.key() == YUnit || it.key() == ZUnit || it.key() == stylesheet)
				continue;
		}

		item->setProperty(it.key().toLatin1().data(), it.value());
	}

	// d_data->formatted = true;
}

QWidget* VipDisplayPlotItem::widget() const
{
	VipPlotItem* it = item();
	if (!it)
		return nullptr;

	QWidget* w = it->view();
	return w;
}

bool VipDisplayPlotItem::displayInProgress() const
{
	if (VipPlotItem* it = item())
		return it->updateInProgress();
	return false;
}

QString VipDisplayPlotItem::title() const
{
	if (VipPlotItem* it = item())
		return it->title().text();
	return QString();
}

static bool isHidden(QWidget* w)
{
	// Check recursively if widget or one of its parent is hidden.
	// This function stops at the first VipDisplayPlayerArea.
	while (w) {
		if (w->isHidden()) {
			return true;
		}
		w = w->parentWidget();
	}
	return false;
}

bool VipDisplayPlotItem::isVisible() const
{
	if (VipPlotItem* it = item()) {
		if (!it->isVisible())
			return false;

		QWidget* player = widget();
		if (!player) {
			auto* view = it->view();
			if (view)
				return !isHidden(view);
			return false;
		}
		else if (isHidden(player))
			return false;
		else if (!player->isEnabled())
			return false;

		// if the top level VipMultiDragWidget is minimized or another top level VipMultiDragWidget is maximized, return false
		/* if (VipBaseDragWidget* bd = VipBaseDragWidget::fromChild(player))
		{
			if (bd->isHidden())
				return false;
			if(VipMultiDragWidget * mw = bd->validTopLevelMultiDragWidget())
			{
				if(mw->isMinimized())
					return false;
				else if(VipMultiDragWidget * maximized = VipDragWidgetHandler::find(mw->parentWidget())->maximizedMultiDragWidgets())
					if(maximized != mw)
						return false;
			}
		}*/

		return true;
	}
	return false;
}

void VipDisplayPlotItem::setItemSuppressable(bool enable)
{
	d_data->itemSuppressable = enable;
	if (!d_data->item.isEmpty())
		item()->setItemAttribute(VipPlotItem::IsSuppressable, enable);
}

bool VipDisplayPlotItem::itemSuppressable() const
{
	return d_data->itemSuppressable;
}

class VipDisplayCurve::PrivateData
{
public:
	PrivateData()
	  : formated(false)
	  , is_full_vector(false)
	{
	}
	VipExtractComponent extract;
	bool formated;
	bool is_full_vector;
};

VipDisplayCurve::VipDisplayCurve(QObject* parent)
  : VipDisplayPlotItem(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	setItem(new VipPlotCurve());
	item()->setAutoMarkDirty(false);
	this->propertyName("Sliding_time_window")->setData(-1.);
}

VipDisplayCurve::~VipDisplayCurve()
{
}

VipExtractComponent* VipDisplayCurve::extractComponent() const
{
	return const_cast<VipExtractComponent*>(&d_data->extract);
}

bool VipDisplayCurve::acceptInput(int, const QVariant& v) const
{
	return v.canConvert<VipPointVector>() || v.canConvert<VipComplexPointVector>() || v.canConvert<VipComplexPoint>() || v.canConvert<double>() || v.canConvert<VipPoint>();
}

void VipDisplayCurve::setItem(VipPlotItem* _it)
{
	VipPlotCurve* it = qobject_cast<VipPlotCurve*>(_it);
	if (it && it != item()) {
		//disconnect(item(), SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(axesChanged(VipPlotItem*)));
		it->setAutoMarkDirty(false);
		VipDisplayPlotItem::setItem(it);
		//connect(it, SIGNAL(axesChanged(VipPlotItem*)), this, SLOT(axesChanged(VipPlotItem*)));
	}
}

bool VipDisplayCurve::receiveStreamingData() const
{
	return !d_data->is_full_vector;
}

bool VipDisplayCurve::prepareForDisplay(const VipAnyDataList& lst)
{
	if (VipPlotCurve* curve = item()) {
		// create the curve
		VipPointVector vector;
		VipComplexPointVector cvector;

		d_data->is_full_vector = false;
		for (int i = 0; i < lst.size(); ++i) {
			const VipAnyData& any = lst[i];
			const QVariant& v = any.data();

			if (v.userType() == qMetaTypeId<VipPointVector>()) {
				vector = v.value<VipPointVector>();
				d_data->is_full_vector = true;
			}
			else if (v.userType() == qMetaTypeId<VipComplexPointVector>()) {
				cvector = v.value<VipComplexPointVector>();
				d_data->is_full_vector = true;
			}
			else if (v.userType() == qMetaTypeId<VipPoint>()) {
				// if (vector.isEmpty())
				//	vector = curve->rawData();
				vector.append(v.value<VipPoint>());
			}
			else if (v.userType() == qMetaTypeId<complex_d>()) {
				cvector.append(VipComplexPoint(any.time(), v.value<complex_d>()));
			}
			else if (v.canConvert(VIP_META(QMetaType::Double)) && any.time() != VipInvalidTime) {
				// if (vector.isEmpty())
				//	vector = curve->rawData();
				vector.append(QPointF(any.time(), v.toDouble()));
			}
		}

		// convert complex to double
		QString component;
		if (cvector.size()) {
			d_data->extract.inputAt(0)->setData(cvector);
			d_data->extract.update();
			vector = d_data->extract.outputAt(0)->data().value<VipPointVector>();
		}
		else if (d_data->extract.supportedComponents().size() > 1) {
			// reset the component extractor
			d_data->extract.resetSupportedComponents();
		}

		double window = propertyAt(1)->value<double>();

		curve->updateSamples([&](VipPointVector& vec) {
			if (d_data->is_full_vector)
				vec = vector;
			else if(vector.size()){
				//remove all data with a time greater than sample
				qsizetype erase_from = vec.size();
				for (qsizetype i = vec.size() - 1; i >= 0; --i) {
					if (vec[i].x() >= vector.first().x())
						erase_from = i;
					else
						break;
				}
				vec.erase(vec.begin() + erase_from, vec.end());
				vec.append(vector);
			}
			//TEST: apply windowing to ALL signal
			if (window > 0 && vec.size() /*&& !d_data->is_full_vector*/) {
				// convert to nanoseconds
				window *= 1000000000;
				for (int i = 0; i < vec.size(); ++i) {
					double range = vec.last().x() - vec[i].x();
					if (range < window) {
						if (i != 0)
							// vec = vec.mid(i);
							vec.erase(vec.begin(), vec.begin() + i);
						break;
					}
				}
			}
		});

		/* if (window > 0 && vector.size() && !d_data->is_full_vector)
		{
			//convert to nanoseconds
			window *= 1000000000;
			for (int i = 0; i < vector.size(); ++i)
			{
				double range = vector.last().x() - vector[i].x();
				if (range < window)
				{
					if (i != 0)
						vector = vector.mid(i);
					break;
				}
			}
		}

		curve->setRawData(vector);*/
	}
	return false;
}

void VipDisplayCurve::displayData(const VipAnyDataList& lst)
{
	if (VipPlotCurve* curve = item()) {
		curve->markDirty();

		// format the item
		if (lst.size() && (!d_data->formated || d_data->is_full_vector)) {
			formatItemIfNecessary(curve, lst.back());
			d_data->formated = true;
		}
		else if (lst.size()) {
			// minimal formatting, just check the x unit to detect time values...
			const QVariantMap attrs = lst.back().attributes();
			QVariantMap::const_iterator xunit = attrs.find("XUnit");
			if (xunit != attrs.end() && curve->axes().size()) {
				VipText t = curve->axisUnit(0);
				curve->setAxisUnit(0, VipText(xunit.value().toString(), t.textStyle()));
			}

			//...and check the title
			if (curve->title().isEmpty())
				curve->setTitle(attrs["Name"].toString());
		}
	}
}

VipDisplaySceneModel::VipDisplaySceneModel(QObject* parent)
  : VipDisplayPlotItem(parent)
{
	setItemSuppressable(false);
	setItem(new VipPlotSceneModel());
	item()->setBrush("All", QBrush(QColor(255, 0, 0, 70)));
	item()->setDrawComponents("All", VipPlotShape::Border | VipPlotShape::Background | VipPlotShape::Id | VipPlotShape::Group);
	item()->setZValue(1000);
	item()->setIgnoreStyleSheet(true);
}

bool VipDisplaySceneModel::acceptInput(int, const QVariant& v) const
{
	return v.canConvert<VipSceneModel>();
}

void VipDisplaySceneModel::setTransform(const QTransform& tr)
{
	if (!m_transform.isIdentity()) {
		QTransform inv = m_transform.inverted();
		item()->sceneModel().transform(inv);
	}
	m_transform = tr;
	item()->sceneModel().transform(tr);
}
QTransform VipDisplaySceneModel::transform() const
{
	return m_transform;
}

bool VipDisplaySceneModel::prepareForDisplay(const VipAnyDataList& lst)
{
	if (VipPlotSceneModel* curve = item()) {
		// create the curve
		if (lst.size()) {
			// display the last data
			const VipAnyData& data = lst.back();
			const QVariant& v = data.data();
			if (v.userType() == qMetaTypeId<VipSceneModel>()) {
				const VipSceneModel src = v.value<VipSceneModel>();

				// do not apply twice the transform
				if ((void*)src != (void*)curve->sceneModel()) {
					VipSceneModel copy = src.copy();

					if (!m_transform.isIdentity())
						copy.transform(m_transform);
					// curve->sceneModel().reset(copy);
					curve->resetContentWith(copy);
				}
			}
			else if (v.userType() == qMetaTypeId<VipShape>()) {
				const VipShape src = v.value<VipShape>();

				// do not apply twice the transform
				if (curve->sceneModel().indexOf(src.group(), src) < 0) {

					curve->sceneModel().clear();
					VipShape copy = src.copy();
					if (!m_transform.isIdentity())
						copy.transform(m_transform);
					// curve->sceneModel().add(copy);
					VipSceneModel tmp;
					tmp.add(copy);
					curve->resetContentWith(tmp);
				}
			}
		}
	}
	return false;
}

void VipDisplaySceneModel::displayData(const VipAnyDataList& lst)
{
	if (VipPlotSceneModel* curve = item()) {
		if (lst.size()) {
			const VipAnyData data = lst.back();
			formatItemIfNecessary(curve, data);
		}
	}
}

class VipDisplayImage::PrivateData
{
public:
	PrivateData()
	  : paintTime(0)
	{
	}
	VipExtractComponent extract;
	VipNDArray tmpArray;
	QMutex mutex;
	qint64 paintTime;
};

VipDisplayImage::VipDisplayImage(QObject* parent)
  : VipDisplayPlotItem(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	setItem(new VipPlotSpectrogram());
	item()->setSelectedPen(Qt::NoPen);
	item()->setAutoMarkDirty(false);
}

VipDisplayImage::~VipDisplayImage()
{
}

bool VipDisplayImage::acceptInput(int, const QVariant& v) const
{
	return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<VipRasterData>();
}

QSize VipDisplayImage::sizeHint() const
{
	if (VipPlotSpectrogram* curve = item()) {
		curve->dataLock()->lock();
		QSize res = curve->rawData().boundingRect().size().toSize();
		curve->dataLock()->unlock();
		return res;
	}
	else
		return QSize();
}

VipExtractComponent* VipDisplayImage::extractComponent() const
{
	return const_cast<VipExtractComponent*>(&d_data->extract);
}

bool VipDisplayImage::canDisplayImageAsIs(const VipNDArray& ar)
{
	return !ar.isNull() && ar.shapeCount() == 2 && !ar.isComplex();
}

bool VipDisplayImage::prepareForDisplay(const VipAnyDataList& data)
{
	if (VipPlotSpectrogram* curve = qobject_cast<VipPlotSpectrogram*>(VipDisplayPlotItem::item())) {

		if (data.size()) {
			// curve->markDirty();
			const QVariant& v = data.last().data();
			if (v.userType() == qMetaTypeId<VipNDArray>()) {
				QString component = d_data->extract.propertyAt(0)->value<QString>();
				if (!component.isEmpty() && component != "Invariant") {
					const VipNDArray ar = v.value<VipNDArray>();
					d_data->extract.inputAt(0)->setData(ar);
					d_data->extract.update();
					curve->setData(d_data->extract.outputAt(0)->data().data());
				}
				else
					curve->setData(v);
			}
			else if (v.userType() == qMetaTypeId<VipRasterData>()) {
				const VipRasterData raster = v.value<VipRasterData>();
				curve->setRawData(raster);
			}
		}
	}
	return false;
}

void VipDisplayImage::displayData(const VipAnyDataList& lst)
{
	if (VipPlotSpectrogram* curve = item()) {

		if (lst.size()) {
			// display the last data
			const VipAnyData& data = lst.back();

			formatItemIfNecessary(curve, data);
		}
	}
}

VipArchive& operator<<(VipArchive& stream, const VipDisplayObject*)
{
	// return stream.content("displayInGuiThread",r->displayInGuiThread());
	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipDisplayObject*)
{
	// r->setDisplayInGuiThread(stream.read("displayInGuiThread").value<bool>());
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipDisplayPlotItem* r)
{
	return stream.content("item", r->d_data->item).content("itemSuppressable", r->itemSuppressable());
}

VipArchive& operator>>(VipArchive& stream, VipDisplayPlotItem* r)
{
	r->d_data->item = stream.read("item").value<VipLazyPointer>();
	r->setItemSuppressable(stream.read("itemSuppressable").value<bool>());
	return stream;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipDisplayObject*>();
	qRegisterMetaType<VipDisplayPlotItem*>();
	vipRegisterArchiveStreamOperators<VipDisplayObject*>();
	vipRegisterArchiveStreamOperators<VipDisplayPlotItem*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
