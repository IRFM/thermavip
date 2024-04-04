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
#include <QGraphicsView>
#include <qtimer.h>

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

	VipSpinlock lock;
	std::condition_variable_any cond;
};

VipDisplayObject::VipDisplayObject(QObject* parent)
  : VipProcessingObject(parent)
{
	m_data = new PrivateData();

	this->setScheduleStrategies(Asynchronous);
	inputAt(0)->setListType(VipDataList::FIFO, VipDataList::Number, 10);
	propertyAt(0)->setData(1);
}

VipDisplayObject::~VipDisplayObject()
{
	m_data->isDestruct = true;
	if (inputAt(0)->connection()->source()) {
		this->wait();
	}
	delete m_data;
}

void VipDisplayObject::checkVisibility()
{
	m_data->visible = isVisible();
}

void VipDisplayObject::apply()
{
	if (m_data->isDestruct)
		return;

	// check display visibility every 200ms
	qint64 time = QDateTime::currentMSecsSinceEpoch();
	if (time - m_data->lastVisibleUpdate > 200) {
		m_data->lastVisibleUpdate = time;
		if (QThread::currentThread() == QCoreApplication::instance()->thread())
			checkVisibility();
		else
			QMetaObject::invokeMethod(this, "checkVisibility", Qt::QueuedConnection);
	}
	if (!m_data->visible && !m_data->updateOnHidden) {
		// clear input buffer
		inputAt(0)->allData();
		return;
	}
	if (!isEnabled() || !inputAt(0)->hasNewData())
		return;

	const VipAnyDataList buffer = inputAt(0)->allData();
	m_data->displayInProgress = true;

	if (!this->prepareForDisplay(buffer)) {

		// display in the GUI thread or not
		if ((QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()))
			this->display(buffer);
		else
			QMetaObject::invokeMethod(this, "display", Qt::QueuedConnection, Q_ARG(VipAnyDataList, buffer));

		// wait for the display to end

		VipUniqueLock<VipSpinlock> ll(m_data->lock);
		while (m_data->displayInProgress.load(std::memory_order_relaxed) && !m_data->isDestruct) {
			m_data->cond.wait_for(m_data->lock, std::chrono::milliseconds(5));
			qint64 current = QDateTime::currentMSecsSinceEpoch();
			if ((current - time) > 50) {
				vipProcessEvents(nullptr, 100);
				break;
			}
		}
	}
	else {
		Q_EMIT displayed(buffer);
		m_data->displayInProgress = false;
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

void VipDisplayObject::display(const VipAnyDataList& dat)
{
	if (m_data->isDestruct)
		return;

	// update parent VipAbstractPlayer title every 500 ms (no need for more in case of streaming)
	qint64 time = QDateTime::currentMSecsSinceEpoch();
	if (time - m_data->lastTitleUpdate > 500) {
		m_data->lastTitleUpdate = time;
		const VipAnyData data = dat.size() ? dat.back() : VipAnyData();
		if (data.hasAttribute("Name") || data.hasAttribute("PlayerName")) {
			QString title = data.name();
			QString title2 = data.attribute("PlayerName").toString();
			if (!title2.isEmpty())
				title = title2;
			if (m_data->playerTitle != title) {
				QWidget* player = findWidgetWith_automaticWindowTitle(widget());
				if (player && !title.isEmpty()) {
					if (player->property("automaticWindowTitle").toBool()) {
						// vip_debug("set window title\n");
						QMetaObject::invokeMethod(player, "setWindowTitle", Qt::AutoConnection, Q_ARG(QString, title));
					}
					m_data->playerTitle = title;
				}
			}
		}
	}

	displayData(dat);
	Q_EMIT displayed(dat);

	m_data->displayInProgress = false;
	m_data->cond.notify_one();
}

bool VipDisplayObject::displayInProgress() const
{
	return m_data->displayInProgress.load(std::memory_order_relaxed);
}

void VipDisplayObject::setFormattingEnabled(bool enable)
{
	m_data->formattingEnabled = enable;
}
bool VipDisplayObject::formattingEnabled() const
{
	return m_data->formattingEnabled;
}

void VipDisplayObject::setUpdateOnHidden(bool enable) {
	m_data->updateOnHidden = enable;
}
bool VipDisplayObject::updateOnHidden() const
{
	return m_data->updateOnHidden;
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
	m_data = new PrivateData();
	connect(&m_data->format_timer, SIGNAL(timeout()), this, SLOT(internalFormatItem()));
}

VipDisplayPlotItem::~VipDisplayPlotItem()
{
	VipPlotItem* c = m_data->item.data<VipPlotItem>();
	if (c)
		c->setProperty("VipDisplayObject", QVariant());
	delete m_data;
}

VipPlotItem* VipDisplayPlotItem::item() const
{
	bool found = false;
	VipPlotItem* item = m_data->item.data<VipPlotItem>(&found);
	if (found) {
		QMetaObject::invokeMethod(const_cast<VipDisplayPlotItem*>(this), "setItemProperty", Qt::AutoConnection);
		connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		item->setItemAttribute(VipPlotItem::IsSuppressable, m_data->itemSuppressable);
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
	if (current - m_data->last_format > 500) {
		// it's been a long time, let's format!
		formatItem(item, any);
	}
	else {
		m_data->format_item = item;
		m_data->format_any = any;
		// restart timer
		m_data->format_timer.start(500);
	}
}

void VipDisplayPlotItem::internalFormatItem()
{
	if (VipPlotItem* it = m_data->format_item)
		formatItem(it, m_data->format_any);
	m_data->last_format = QDateTime::currentMSecsSinceEpoch();
}

VipPlotItem* VipDisplayPlotItem::takeItem()
{
	if (VipPlotItem* it = this->item()) {
		disconnect(it, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()));
		disconnect(it, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		it->setProperty("VipDisplayObject", QVariant());
		m_data->item.setData<VipPlotItem>(nullptr);
		return it;
	}
	return nullptr;
}

void VipDisplayPlotItem::setItem(VipPlotItem* item)
{

	if (VipPlotItem* it = this->item()) {
		disconnect(it, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()));
		disconnect(it, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		delete it;
	}

	m_data->item.setData(item);
	if (item) {
		item->setProperty("VipDisplayObject", QVariant::fromValue(this));
		connect(item, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()), Qt::DirectConnection);
		connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));

		item->setItemAttribute(VipPlotItem::IsSuppressable, m_data->itemSuppressable);
	}
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
			if (n != m_data->fTitle) { //(item->title().isEmpty() || item->title().text() != n) && !n.isEmpty())
				item->setTitle(VipText(n, item->title().textStyle()));
				m_data->fTitle = n;
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
			if (t.isEmpty() && xu != m_data->fxUnit) {
				item->setAxisUnit(0, VipText(xu, t.textStyle()));
				m_data->fxUnit = xu;
			}
		}
	}
	// if (st_keys.find("axisunit[1]") == st_keys.end() )
	{
		yunit = attrs.find(YUnit);
		if (yunit != attrs.end()) {
			VipText t = item->axisUnit(1);
			QString yu = yunit.value().toString();
			if (t.isEmpty() && yu != m_data->fyUnit) {
				item->setAxisUnit(1, VipText(yu, t.textStyle()));
				m_data->fyUnit = yu;
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
				if (t.text() != new_title && new_title != m_data->fzUnit) {
					// if(VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(this->widget()))
					//	pl->viewer()->area()->colorMapAxis()->setTitle(VipText(new_title, t.textStyle()));
					// else
					item->colorMap()->setTitle(VipText(new_title, t.textStyle()));
					m_data->fzUnit = new_title;
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

	// m_data->formatted = true;
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
	m_data->itemSuppressable = enable;
	if (!m_data->item.isEmpty())
		item()->setItemAttribute(VipPlotItem::IsSuppressable, enable);
}

bool VipDisplayPlotItem::itemSuppressable() const
{
	return m_data->itemSuppressable;
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
	m_data = new PrivateData();
	setItem(new VipPlotCurve());
	item()->setAutoMarkDirty(false);
	this->propertyName("Sliding_time_window")->setData(-1.);
}

VipDisplayCurve::~VipDisplayCurve()
{
	delete m_data;
}

VipExtractComponent* VipDisplayCurve::extractComponent() const
{
	return const_cast<VipExtractComponent*>(&m_data->extract);
}

bool VipDisplayCurve::acceptInput(int, const QVariant& v) const
{
	return v.canConvert<VipPointVector>() || v.canConvert<VipComplexPointVector>() || v.canConvert<VipComplexPoint>() || v.canConvert<double>() || v.canConvert<VipPoint>();
}

void VipDisplayCurve::setItem(VipPlotCurve* it)
{
	if (it && it != item()) {
		it->setAutoMarkDirty(false);
		VipDisplayPlotItem::setItem(it);
	}
}

bool VipDisplayCurve::prepareForDisplay(const VipAnyDataList& lst)
{
	if (VipPlotCurve* curve = item()) {
		// create the curve
		VipPointVector vector;
		VipComplexPointVector cvector;

		m_data->is_full_vector = false;
		for (int i = 0; i < lst.size(); ++i) {
			const VipAnyData& any = lst[i];
			const QVariant& v = any.data();

			if (v.userType() == qMetaTypeId<VipPointVector>()) {
				vector = v.value<VipPointVector>();
				m_data->is_full_vector = true;
			}
			else if (v.userType() == qMetaTypeId<VipComplexPointVector>()) {
				cvector = v.value<VipComplexPointVector>();
				m_data->is_full_vector = true;
			}
			else if (v.userType() == qMetaTypeId<VipPoint>()) {
				// if (vector.isEmpty())
				//	vector = curve->rawData();
				vector.append(v.value<VipPoint>());
			}
			else if (v.userType() == qMetaTypeId<complex_d>()) {
				cvector.append(VipComplexPoint(any.time(), v.value<complex_d>()));
			}
			else if (v.canConvert(QMetaType::Double) && any.time() != VipInvalidTime) {
				// if (vector.isEmpty())
				//	vector = curve->rawData();
				vector.append(QPointF(any.time(), v.toDouble()));
			}
		}

		// convert complex to double
		QString component;
		if (cvector.size()) {
			m_data->extract.inputAt(0)->setData(cvector);
			m_data->extract.update();
			vector = m_data->extract.outputAt(0)->data().value<VipPointVector>();
		}
		else if (m_data->extract.supportedComponents().size() > 1) {
			// reset the component extractor
			m_data->extract.resetSupportedComponents();
		}

		double window = propertyAt(1)->value<double>();

		curve->updateSamples([&](VipPointVector& vec) {
			if (m_data->is_full_vector)
				vec = vector;
			else
				vec.append(vector);

			if (window > 0 && vec.size() && !m_data->is_full_vector) {
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

		/* if (window > 0 && vector.size() && !m_data->is_full_vector)
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
		if (lst.size() && (!m_data->formated || m_data->is_full_vector)) {
			formatItemIfNecessary(curve, lst.back());
			m_data->formated = true;
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
	m_data = new PrivateData();
	setItem(new VipPlotSpectrogram());
	item()->setSelectedPen(Qt::NoPen);
	item()->setAutoMarkDirty(false);
}

VipDisplayImage::~VipDisplayImage()
{
	delete m_data;
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
	return const_cast<VipExtractComponent*>(&m_data->extract);
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
				const VipNDArray ar = v.value<VipNDArray>();
				m_data->extract.inputAt(0)->setData(ar);
				m_data->extract.update();
				curve->setData(m_data->extract.outputAt(0)->data().data());
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
	return stream.content("item", r->m_data->item).content("itemSuppressable", r->itemSuppressable());
}

VipArchive& operator>>(VipArchive& stream, VipDisplayPlotItem* r)
{
	r->m_data->item = stream.read("item").value<VipLazyPointer>();
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
