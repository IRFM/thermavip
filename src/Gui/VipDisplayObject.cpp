
#ifdef _WIN32
#include "Windows.h"
#endif

#include <cstring>
#include <chrono>
#include <thread>
#include <cmath>


#include "VipDisplayObject.h"
#include "VipDragWidget.h"
#include "VipIODevice.h"
#include "VipExtractStatistics.h"
#include "VipPlayer.h"
#include "VipProgress.h"
#include "VipLogging.h"
#include "VipTimer.h"
#include "VipStandardProcessing.h"
#include "VipDisplayArea.h"
#include "VipProcessingObjectEditor.h"
#include "VipSleep.h"

#include <QGraphicsView>
#include <QCoreApplication>
#include <qlistwidget.h>
#include <QReadWriteLock>



static void preciseSleepSeconds(double seconds) {
	// see https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
	using namespace std;
	using namespace std::chrono;

	thread_local double estimate = 4e-3;
	thread_local double mean = 4e-3;
	thread_local double m2 = 0;
	thread_local int64_t count = 1;

	while (seconds > estimate) {

		auto start = high_resolution_clock::now();
		this_thread::sleep_for(milliseconds(1));
		auto end = high_resolution_clock::now();

		double observed = (end - start).count() / 1e9;
		seconds -= observed;

		++count;
		double delta = observed - mean;
		mean += delta / count;
		m2 += delta * (observed - mean);
		double stddev = sqrt(m2 / (count - 1));
		estimate = mean + stddev;
	}

	// spin lock
	auto start = high_resolution_clock::now();
	while ((high_resolution_clock::now() - start).count() / 1e9 < seconds) {
		std::this_thread::yield();

	}
}


#ifdef _WIN32

static const double _display_wait_time = 3e-3;

// Windows sleep in 100ns units
void preciseSleep(double seconds) {
	// Needed but why?


	LONGLONG ns = seconds * 1e7;
	// Declarations
	HANDLE timer;   // Timer handle
	LARGE_INTEGER li;   // Time defintion
	// Create timer
	if (!(timer = CreateWaitableTimer(NULL, TRUE, NULL)))
		return preciseSleepSeconds(seconds);
	// Set timer properties
	li.QuadPart = -ns;
	if (!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)) {
		CloseHandle(timer);
		return preciseSleepSeconds(seconds);
	}
	// Start & wait for timer
	WaitForSingleObject(timer, INFINITE);
	// Clean resources
	CloseHandle(timer);
	// Slept without problems
}

#else

static const double _display_wait_time = 1e-4;

void preciseSleep(double seconds) {
	return preciseSleepSeconds(seconds);
}

#endif



class VipDisplayObject::PrivateData
{
public:
	PrivateData() :
		displayInProgress(false),
		isDestruct(false),
		formattingEnabled(true),
		visible(true),
		first(true),
		previousDisplayTime(0),
		lastTitleUpdate(0),
		lastVisibleUpdate(0){}

	std::atomic<bool> displayInProgress;
	bool isDestruct;
	bool formattingEnabled;
	bool visible;
	bool first;
	QString playerTitle; //update parent VipAbstractPlayer title
	qint64 previousDisplayTime;
	qint64 lastTitleUpdate;
	qint64 lastVisibleUpdate;

	//VipSpinlock lock;
	//std::condition_variable_any cond;
};

VipDisplayObject::VipDisplayObject(QObject * parent )
:VipProcessingObject(parent)
{
	m_data = new PrivateData();

	this->setScheduleStrategies(Asynchronous);
	inputAt(0)->setListType(VipDataList::FIFO ,VipDataList::Number,10);
	propertyAt(0)->setData(1);
}

VipDisplayObject::~VipDisplayObject()
{
	m_data->isDestruct = true;
	if (inputAt(0)->connection()->source())
	{
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


	//check display visibility every 200ms
	qint64 time = QDateTime::currentMSecsSinceEpoch();
	if (time - m_data->lastVisibleUpdate > 200) {
		m_data->lastVisibleUpdate = time;
		if (QThread::currentThread() == QCoreApplication::instance()->thread())
			checkVisibility();
		else
			QMetaObject::invokeMethod(this, "checkVisibility", Qt::QueuedConnection);
	}
	if (!m_data->visible) {
		//clear input buffer
		inputAt(0)->allData();
		return;
	}

	bool stream = false;
	if (VipProcessingPool * p = parentObjectPool())
		stream = (p->deviceType() == VipIODevice::Sequential);


	//lock the enable mutex, and check if the processing object is enabled
	{
		if (!isEnabled())
			return;

		if (!inputAt(0)->hasNewData())
		{
			return;
		}

		const VipAnyDataList buffer = inputAt(0)->allData();
		m_data->displayInProgress = true;

		if (!this->prepareForDisplay(buffer)) {

			// display in the GUI thread or not
			if ((QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()))
				this->display(buffer);
			else {
				QMetaObject::invokeMethod(this, "display", Qt::QueuedConnection, Q_ARG(VipAnyDataList, buffer));
			}
		}
	}

	//wait for the display to end
	
	{
		// The condition based approach is is in thoery better.
		// But tests show that pooling approach increase the global GUI responsiveness
		//VipUniqueLock<VipSpinlock> ll(m_data->lock);
		 while (m_data->displayInProgress.load(std::memory_order_relaxed) && !m_data->isDestruct) {
			vipSleep(3);
			// m_data->cond.wait_for(m_data->lock, std::chrono::milliseconds(1));
			//if (!m_data->displayInProgress.load(std::memory_order_relaxed) || m_data->isDestruct)
			//	break;
			qint64 current = QDateTime::currentMSecsSinceEpoch();
			if ((current - time) > 50) {
				vipProcessEvents(NULL, 100);
				break;
			}
		}
		
	}

}


void VipDisplayObject::display( const VipAnyDataList & dat)
{
	if (m_data->isDestruct)
		return;

	//update parent VipAbstractPlayer title every 500 ms (no need for more in case of streaming)
	qint64 time = QDateTime::currentMSecsSinceEpoch();
	if (time - m_data->lastTitleUpdate > 500){
		m_data->lastTitleUpdate = time;
		const VipAnyData data = dat.size() ? dat.back() : VipAnyData();
		if (data.hasAttribute("Name") || data.hasAttribute("PlayerName")){
			QString title = data.name();
			QString title2 = data.attribute("PlayerName").toString();
			if (!title2.isEmpty())
				title = title2;
			if (m_data->playerTitle != title){
				VipAbstractPlayer * player = widget();
				if (player && !title.isEmpty()){
					if (player->automaticWindowTitle()) {
						//printf("set window title\n");
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
	//m_data->cond.notify_one();
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





class VipDisplayPlotItem::PrivateData
{
public:
	PrivateData() : itemSuppressable(false), last_format(0){
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

VipDisplayPlotItem::VipDisplayPlotItem(QObject * parent)
:VipDisplayObject(parent)
{
	m_data = new PrivateData();
	connect(&m_data->format_timer, SIGNAL(timeout()), this, SLOT(internalFormatItem()));
}

VipDisplayPlotItem::~VipDisplayPlotItem()
{
	VipPlotItem * c = m_data->item.data<VipPlotItem>();
	if(c)
		c->setProperty("VipDisplayObject",QVariant());
	delete m_data;
}

VipPlotItem* VipDisplayPlotItem::item() const
{
	bool found = false;
	VipPlotItem * item = m_data->item.data<VipPlotItem>(&found);
	if (found)
	{
		QMetaObject::invokeMethod(const_cast<VipDisplayPlotItem*>(this), "setItemProperty", Qt::AutoConnection);
		connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		item->setItemAttribute(VipPlotItem::IsSuppressable, m_data->itemSuppressable);
	}
	return item;
}

void VipDisplayPlotItem::setItemProperty()
{
	if (VipPlotItem * it = this->item())
		it->setProperty("VipDisplayObject", QVariant::fromValue(this));
}

void VipDisplayPlotItem::formatItemIfNecessary(VipPlotItem * item, const VipAnyData & any)
{
	qint64 current = QDateTime::currentMSecsSinceEpoch();
	if (current - m_data->last_format > 500) {
		//it's been a long time, let's format!
		formatItem(item, any);
	}
	else {
		m_data->format_item = item;
		m_data->format_any = any;
		//restart timer
		m_data->format_timer.start(500);
	}
}

void VipDisplayPlotItem::internalFormatItem()
{
	if(VipPlotItem * it = m_data->format_item)
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

void VipDisplayPlotItem::setItem(VipPlotItem * item)
{

	if(VipPlotItem * it = this->item())
	{
		disconnect(it, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()));
		disconnect(it, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		delete it;
	}

	m_data->item.setData(item);
	if(item)
	{
		item->setProperty("VipDisplayObject",QVariant::fromValue(this));
		connect(item, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(disable()),Qt::DirectConnection);
		connect(item,SIGNAL(destroyed(QObject*)),this,SLOT(deleteLater()));

		item->setItemAttribute(VipPlotItem::IsSuppressable, m_data->itemSuppressable);
	}
}

void VipDisplayPlotItem::formatItem(VipPlotItem * item, const VipAnyData & data, bool force )
{
	static const QString XUnit = "XUnit";
	static const QString YUnit = "YUnit";
	static const QString ZUnit = "ZUnit";
	static const QString stylesheet = "stylesheet";

	if (!formattingEnabled())
		return;

	if(!force && (item->axes().size() < 2 || !item->axes().constFirst() || !item->axes().constLast()))
		return;

	const QVariantMap this_attrs = this->attributes();
	QVariantMap attrs = data.attributes();
	//merge attributes
	for (QVariantMap::const_iterator it = this_attrs.begin(); it != this_attrs.end(); ++it)
		attrs.insert(it.key(), it.value());


	QVariantMap::const_iterator name = attrs.end();
	QVariantMap::const_iterator xunit = name;
	QVariantMap::const_iterator yunit = name;
	QVariantMap::const_iterator zunit = name;

	//apply style sheet
	QVariantMap::const_iterator  st = attrs.find(stylesheet);
	if (st != attrs.end() ) {
		QString _stylesheet = st.value().toString();
		if(!_stylesheet.isEmpty())
			item->setStyleSheet(_stylesheet);
	}


	//set the item name
	//if (st_keys.find("title") == st_keys.end() )
	{
		//'fixed_title' is an item property containing the item's title defined by the user through the user interface.
		//if defined, it cannot be changed.
		name = attrs.find("Name");
		if (name != attrs.end())
		{
			QString n = name.value().toString();
			if (n != m_data->fTitle) {//(item->title().isEmpty() || item->title().text() != n) && !n.isEmpty())
				item->setTitle(VipText(n, item->title().textStyle()));
				m_data->fTitle = n;
			}
		}
	}

	//set the item's x and y unit
	//if(st_keys.find("axisunit[0]") == st_keys.end() )
	{
		xunit = attrs.find(XUnit);
		if (xunit != attrs.end())
		{
			VipText t = item->axisUnit(0);
			QString xu = xunit.value().toString();
			if (t.isEmpty() && xu != m_data->fxUnit) {
				item->setAxisUnit(0, VipText(xu, t.textStyle()));
				m_data->fxUnit = xu;
			}
		}
	}
	//if (st_keys.find("axisunit[1]") == st_keys.end() )
	{
		yunit = attrs.find(YUnit);
		if (yunit != attrs.end())
		{
			VipText t = item->axisUnit(1);
			QString yu = yunit.value().toString();
			if (t.isEmpty() && yu != m_data->fyUnit) {
				item->setAxisUnit(1, VipText(yu, t.textStyle()));
				m_data->fyUnit = yu;
			}
		}
	}



	//set the color map unit
	if (item->colorMap()  )
	{
		{
			zunit = attrs.find(ZUnit);
			if (zunit != attrs.end())
			{
				VipText t = item->colorMap()->title();
				QString new_title = zunit.value().toString();
				if (t.text() != new_title && new_title != m_data->fzUnit) {
					if(VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(this->widget()))
						pl->viewer()->area()->colorMapAxis()->setTitle(VipText(new_title, t.textStyle()));
					else
						item->colorMap()->setTitle(VipText(new_title, t.textStyle()));
					m_data->fzUnit = new_title;
				}
			}
		}
	}
	
	//set the other attributes
	for(QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
	{
		if (it.key().endsWith("Unit")) {
			if (it.key() == XUnit || it.key() == YUnit || it.key() == ZUnit || it.key() == stylesheet)
				continue;
		}

		item->setProperty(it.key().toLatin1().data(), it.value());
	}

	//m_data->formatted = true;
}

VipAbstractPlayer* VipDisplayPlotItem::widget() const
{
	VipPlotItem * it = item();
	if(!it)
		return NULL;

	QWidget * w = it->view();
	while(w)
	{
		if(qobject_cast<VipAbstractPlayer*>(w))
			return static_cast<VipAbstractPlayer*>(w);
		w = w ->parentWidget();
	}
	return NULL;
}

bool VipDisplayPlotItem::displayInProgress() const
{
	if(VipPlotItem * it = item())
		return it->updateInProgress();
	return false;
}

QString VipDisplayPlotItem::title() const
{
	if (VipPlotItem * it = item())
		return it->title().text();
	return QString();
}

static bool isHidden(QWidget * w)
{
	//Check recursively if widget or one of its parent is hidden.
	//This function stops at the first VipDisplayPlayerArea.
	while (w && !qobject_cast<VipDisplayPlayerArea*>(w)) {
		if (w->isHidden()) {
			return true;
		}
		w = w->parentWidget();
	}
	return false;
}

bool VipDisplayPlotItem::isVisible() const
{
	if(VipPlotItem * it = item())
	{
		if(!it->isVisible())
			return false;

		VipAbstractPlayer * player = widget();
		if(!player) {
			auto* view = it->view();
			if (view)
				return !isHidden(view);
			return false;
		}
		else if(isHidden(player))
			return false;
		else if (!player->isEnabled())
			return false;

		//if the top level VipMultiDragWidget is minimized or another top level VipMultiDragWidget is maximized, return false
		if(VipBaseDragWidget * bd = VipBaseDragWidget::fromChild(player))
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
		}

		return true;
	}
	return false;
}

void VipDisplayPlotItem::setItemSuppressable(bool enable)
{
	m_data->itemSuppressable = enable;
	if(!m_data->item.isEmpty())
		item()->setItemAttribute(VipPlotItem::IsSuppressable, enable);
}

bool VipDisplayPlotItem::itemSuppressable() const
{
	return m_data->itemSuppressable;
}




class VipDisplayCurve::PrivateData
{
public:
	PrivateData() : formated(false), is_full_vector(false){}
	VipExtractComponent extract;
	bool formated;
	bool is_full_vector;
};

VipDisplayCurve::VipDisplayCurve(QObject * parent )
:VipDisplayPlotItem(parent)
{
	m_data = new PrivateData();
	setItem( new VipPlotCurve());
	item()->setAutoMarkDirty(false);
	this->propertyName("Sliding_time_window")->setData(-1.);
}

VipDisplayCurve::~VipDisplayCurve()
{
	delete m_data;
}

VipExtractComponent * VipDisplayCurve::extractComponent() const
{
	return const_cast<VipExtractComponent*>(&m_data->extract);
}

bool VipDisplayCurve::acceptInput(int , const QVariant & v) const
{
	return v.canConvert<VipPointVector>() || v.canConvert<VipComplexPointVector>() || v.canConvert<VipComplexPoint>() || v.canConvert<double>();
}

bool VipDisplayCurve::prepareForDisplay(const VipAnyDataList & lst)
{
	if (VipPlotCurve * curve = item())
	{
		//create the curve
		VipPointVector vector;// = curve->rawData();
		VipComplexPointVector cvector;

		m_data->is_full_vector = false;
		for (int i = 0; i < lst.size(); ++i)
		{
			const VipAnyData any = lst[i];
			QVariant v = any.data();

			if (v.userType() == qMetaTypeId<VipPointVector>())
			{
				vector = v.value<VipPointVector>();
				m_data->is_full_vector = true;
			}
			else if (v.userType() == qMetaTypeId<VipComplexPointVector>())
			{
				cvector = v.value<VipComplexPointVector>();
				m_data->is_full_vector = true;
			}
			else if (v.userType() == qMetaTypeId<VipPoint>()) {
				if (vector.isEmpty())
					vector = curve->rawData();
				vector.append(v.value<VipPoint>());
			}
			else if (v.userType() == qMetaTypeId<complex_d>())
			{
				cvector.append(VipComplexPoint(any.time(), v.value<complex_d>()));
			}
			else if (v.canConvert(QMetaType::Double) && any.time() != VipInvalidTime)
			{
				if (vector.isEmpty())
					vector = curve->rawData();
				vector.append(QPointF(any.time(), v.toDouble()));
			}
		}

		//convert complex to double
		QString component;
		if (cvector.size())
		{
			m_data->extract.inputAt(0)->setData(cvector);
			m_data->extract.update();
			vector = m_data->extract.outputAt(0)->data().value<VipPointVector>();
		}
		else if (m_data->extract.supportedComponents().size() > 1)
		{
			//reset the component extractor
			m_data->extract.resetSupportedComponents();
		}

		double window = propertyAt(1)->value<double>();
		if (window > 0 && vector.size() && !m_data->is_full_vector)
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

		curve->setRawData(vector);
	}
	return false;
}

void VipDisplayCurve::displayData(const VipAnyDataList & lst)
{
	if (VipPlotCurve * curve = item())
	{
		curve->markDirty();

		//format the item
		if (lst.size() && (!m_data->formated || m_data->is_full_vector))
		{
			formatItemIfNecessary(curve, lst.back());
			m_data->formated = true;
		}
		else if (lst.size())
		{
			//minimal formatting, just check the x unit to detect time values...
			const QVariantMap attrs = lst.back().attributes();
			QVariantMap::const_iterator xunit = attrs.find("XUnit");
			if (xunit != attrs.end() && curve->axes().size())
			{
				VipText t = curve->axisUnit(0);
				curve->setAxisUnit(0, VipText(xunit.value().toString(), t.textStyle()));
			}

			//...and check the title
			if (curve->title().isEmpty())
				curve->setTitle(attrs["Name"].toString());
		}
	}
}





VipDisplayHistogram::VipDisplayHistogram(QObject * parent )
:VipDisplayPlotItem(parent)
{
	setItem( new VipPlotHistogram());
	item()->setAutoMarkDirty(false);
}

bool VipDisplayHistogram::acceptInput(int , const QVariant & v) const
{
	return v.canConvert<VipIntervalSampleVector>() || v.canConvert<VipIntervalSample>();
}

bool VipDisplayHistogram::prepareForDisplay(const VipAnyDataList & lst)
{
	if (VipPlotHistogram * curve = item())
	{
		//create the curve
		VipIntervalSampleVector vector = curve->rawData();

		for (int i = 0; i < lst.size(); ++i)
		{
			const VipAnyData any = lst[i];
			QVariant v = any.data();

			if (v.userType() == qMetaTypeId<VipIntervalSampleVector>())
				vector = v.value<VipIntervalSampleVector>();
			else if (v.userType() == qMetaTypeId<VipIntervalSample>())
				vector.append(v.value<VipIntervalSample>());
		}

		curve->setRawData(vector);
	}
	return false;
}

void VipDisplayHistogram::displayData(const VipAnyDataList & lst)
{	
	if(VipPlotHistogram * curve = item())
	{
		curve->markDirty();
		//format the item
		if (lst.size())
			formatItemIfNecessary(curve, lst.back());
	}
}



VipDisplaySceneModel::VipDisplaySceneModel(QObject * parent )
:VipDisplayPlotItem(parent)
{
	setItemSuppressable(false);
	setItem( new VipPlotSceneModel());
	item()->setBrush("All", QBrush(QColor(255, 0, 0, 70)));
	item()->setDrawComponents("All", VipPlotShape::Border| VipPlotShape::Background| VipPlotShape::Id| VipPlotShape::Group);
	item()->setZValue(1000);
	item()->setIgnoreStyleSheet(true);
}

bool VipDisplaySceneModel::acceptInput(int , const QVariant & v) const
{
	return v.canConvert<VipSceneModel>() ;
}

void VipDisplaySceneModel::setTransform(const QTransform & tr)
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
			const VipAnyData data = lst.back();

			QVariant v = data.data();
			if (v.userType() == qMetaTypeId<VipSceneModel>()) {
				const VipSceneModel src = v.value<VipSceneModel>();

				// do not apply twice the transform
				if ((void*)src != (void*)curve->sceneModel()) {
					VipSceneModel copy = src.copy();
				
					if (!m_transform.isIdentity())
						copy.transform(m_transform);
					//curve->sceneModel().reset(copy);
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
					//curve->sceneModel().add(copy);
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
	PrivateData() :paintTime(0) {}
	VipExtractComponent extract;
	VipNDArray tmpArray;
	QMutex mutex;
	qint64 paintTime;
};

VipDisplayImage::VipDisplayImage(QObject * parent )
:VipDisplayPlotItem(parent)
{
	m_data = new PrivateData();
	setItem( new VipPlotSpectrogram());
	item()->setSelectedPen(Qt::NoPen);
	item()->setAutoMarkDirty(false);
}

VipDisplayImage::~VipDisplayImage()
{
	delete m_data;
}

bool VipDisplayImage::acceptInput(int , const QVariant & v) const
{
	return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<VipRasterData>();
}

QSize VipDisplayImage::sizeHint() const
{
	if (VipPlotSpectrogram * curve = item()) {
		curve->dataLock()->lock();
		QSize res =  curve->rawData().boundingRect().size().toSize();
		curve->dataLock()->unlock();
		return res;
	}
	else
		return QSize();
}

VipExtractComponent * VipDisplayImage::extractComponent() const
{
	return const_cast<VipExtractComponent*>(&m_data->extract);
}

bool VipDisplayImage::canDisplayImageAsIs(const VipNDArray & ar)
{
	return !ar.isNull() && ar.shapeCount() == 2 && !ar.isComplex();
}

bool VipDisplayImage::prepareForDisplay(const VipAnyDataList & data)
{
	if (VipPlotSpectrogram * curve = qobject_cast<VipPlotSpectrogram*>(VipDisplayPlotItem::item()))
	{

		if (data.size())
		{
			//curve->markDirty();
			QVariant v = data.last().data();
			if (v.userType() == qMetaTypeId<VipNDArray>()) 
			{
				const VipNDArray ar = v.value<VipNDArray>();
				m_data->extract.inputAt(0)->setData(ar);
				m_data->extract.update();
				curve->setData(m_data->extract.outputAt(0)->data().data());
				
			}
			else if (v.userType() == qMetaTypeId<VipRasterData>())
			{
				const VipRasterData raster = v.value<VipRasterData>();
				curve->setRawData(raster);
			}
		}
	}
	return false;
}

void VipDisplayImage::displayData(const VipAnyDataList & lst)
{
	if(VipPlotSpectrogram * curve = item())
	{
		
		if (lst.size())
		{
			//display the last data
			const VipAnyData data = lst.back();

			formatItemIfNecessary(curve, data);
		}
	}
}









VipFunctionDispatcher<3> & vipFDCreateDisplayFromData()
{
	static VipFunctionDispatcher<3> disp;
	return disp;
}

VipDisplayObject * vipCreateDisplayFromData(const VipAnyData & any, VipAbstractPlayer * pl )
{
	VipDisplayObject * res = NULL;
	const auto lst = vipFDCreateDisplayFromData().exactMatch(any.data(),pl);
	if(lst.size())
		res= lst.back()(any.data(),pl,any).value<VipDisplayObject*>();
	else
	{
		//default behavior: handle VipPointVector, VipIntervalSampleVector and VipNDArray
		if(any.data().userType() == qMetaTypeId<VipPointVector>())
		{
			VipDisplayCurve * curve = new VipDisplayCurve();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(any.data());
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(),any,true);
			res = curve;
		}
		else if(any.data().canConvert<double>())
		{
			VipDisplayCurve * curve = new VipDisplayCurve();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(QVariant::fromValue(VipPointVector()));
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(),any, true);
			res = curve;
		}
		else if(any.data().userType() == qMetaTypeId<VipIntervalSampleVector>())
		{
			VipDisplayHistogram * curve = new VipDisplayHistogram();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(any.data());
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(),any, true);
			res = curve;
		}
		else if(any.data().canConvert<VipIntervalSample>())
		{
			VipDisplayHistogram * curve = new VipDisplayHistogram();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(QVariant::fromValue(VipIntervalSampleVector()<< any.data().value<VipIntervalSample>()));
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(),any, true);
			res = curve;
		}
		else if(any.data().userType() == qMetaTypeId<VipNDArray>())
		{
			VipDisplayImage * curve = new VipDisplayImage();
			curve->inputAt(0)->setData(any);
			curve->item()->setRawData(VipRasterData(any.data().value<VipNDArray>(),QPointF(0,0)));
			curve->formatItem(curve->item(),any, true);
			res = curve;
		}
		else if(any.data().userType() == qMetaTypeId<VipSceneModel>())
		{
			VipDisplaySceneModel * curve = new VipDisplaySceneModel();
			curve->inputAt(0)->setData(any);
			curve->item()->setSceneModel(any.data().value<VipSceneModel>());
			res = curve;
		}
		else if(any.data().userType() == qMetaTypeId<VipShape>())
		{
			VipDisplaySceneModel * curve = new VipDisplaySceneModel();
			curve->inputAt(0)->setData(any);
			curve->item()->sceneModel().add("All", any.data().value<VipShape>());
			res = curve;
		}
	}
	if (VipDisplayPlotItem * it = qobject_cast<VipDisplayPlotItem*>(res)) {
		//set the _vip_created property to tell that the item can trigger an axis creation
		it->item()->setProperty("_vip_created", true);
	}
	return res;
}







VipFunctionDispatcher<4> & vipFDCreatePlayersFromData()
{
	static VipFunctionDispatcher<4> disp;
	return disp;
}



static void setProcessingPool(VipProcessingObject * obj, VipProcessingPool * pool)
{
	if (!pool || !obj) return;
	obj->setParent(pool);
	QList<VipProcessingObject*> sources = obj->allSources();
	for (int i = 0; i < sources.size(); ++i)
		//TOCHECK: check processing pool before
		if(!sources[i]->parentObjectPool())
			sources[i]->setParent(pool);
}

QList<VipAbstractPlayer*> vipCreatePlayersFromData(const VipAnyData & any, VipAbstractPlayer * pl, VipOutput * src, QObject * target, QList<VipDisplayObject*> * doutputs)
{
	const auto lst = vipFDCreatePlayersFromData().exactMatch(any.data(), pl);
	if (lst.size()) {
		QList<VipDisplayObject*> existing;
		if (pl)
			existing = pl->displayObjects();
		QList<VipAbstractPlayer*> res = lst.back()(any.data(), pl, any, target);
		if (doutputs) {
			for (int i = 0; i < res.size(); ++i) {
				//build the list of created display objects
				QList<VipDisplayObject*> disps = res[i]->displayObjects();
				for (int j = 0; j < disps.size(); ++j)
					if (existing.indexOf(disps[j]) < 0)
						*doutputs << disps[j];
			}
		}
		return res;
	}

	if (any.isEmpty())
		return QList<VipAbstractPlayer*>();

	VipDisplayObject * display = vipCreateDisplayFromData(any, pl);
	if(pl)
		setProcessingPool(display, pl->processingPool());

	if (display)
	{

		VipProcessingObject * source = NULL;
		if (src)
			source = src->parentProcessing();
		if (source)
			display->setParent(source->parent());
		VipIODevice * device = qobject_cast<VipIODevice*>(source);

		if (device)
		{
			//for numerical values, insert a VipNumericValueToPointVector before the processing list
			bool is_numeric = false;
			src->data().data().toDouble(&is_numeric);
			if (is_numeric)
			{
				VipNumericValueToPointVector * ConvertToPointVector = new VipNumericValueToPointVector(device->parent());
				ConvertToPointVector->setScheduleStrategies(VipProcessingObject::Asynchronous);
				ConvertToPointVector->setDeleteOnOutputConnectionsClosed(true);
				ConvertToPointVector->inputAt(0)->setConnection(src);
				src = ConvertToPointVector->outputAt(0);
			}

			//insert a VipProcessingList in between the device output and the VipDisplayObject
			VipProcessingList * _lst = new VipProcessingList();
			_lst->setParent(device->parent());
			_lst->setScheduleStrategies(VipProcessingObject::Asynchronous);

			src->setConnection(_lst->inputAt(0));
			_lst->outputAt(0)->setConnection(display->inputAt(0));

			//add an VipExtractComponent in the list for VipNDArray only
			if (any.data().userType() == qMetaTypeId<VipNDArray>())
			{
				VipExtractComponent * extract = new VipExtractComponent();
				//extract->setProperty("_vip_hidden",true)
				extract->setVisible(false);
				_lst->append(extract);
				//vipGetProcessingEditorToolWidget()->setProcessingObject(display);
				//if (VipProcessingListEditor * editor = vipGetProcessingEditorToolWidget()->editor()->processingEditor<VipProcessingListEditor*>(_lst))
				//	editor->setProcessingVisible(_lst->at(_lst->size() - 1), false);
			}


			//if the device is destroyed, destroy everything
			_lst->setDeleteOnOutputConnectionsClosed(true);
			device->setDeleteOnOutputConnectionsClosed(true);
		}
		else if(src)
		{
			src->setConnection(display->inputAt(0));
		}

		QList<VipAbstractPlayer*> res = vipCreatePlayersFromProcessing(display, pl,NULL, target);
		if (!res.size())
		{
			//we cannot insert the VipDisplayObject in an existing player: delete it
			if(device)
				device->setDeleteOnOutputConnectionsClosed(false);
			display->deleteLater();
		}
		if (doutputs)
			*doutputs << display;
		return res;
	}

	return QList<VipAbstractPlayer*>();

}








VipFunctionDispatcher<4> & vipFDCreatePlayersFromProcessing()
{
	static VipFunctionDispatcher<4> disp;
	return disp;
}





QList<VipAbstractPlayer*> vipCreatePlayersFromProcessing(VipProcessingObject * disp, VipAbstractPlayer * pl, VipOutput * output, QObject * target, QList<VipDisplayObject*> * doutputs)
{
	if (!disp)
		return QList<VipAbstractPlayer*>();

	const auto lst = vipFDCreatePlayersFromProcessing().match(disp,pl);
	if (lst.size()) {
		QList<VipAbstractPlayer*> res = lst.back()(disp, pl, output, target);
		QList<VipDisplayObject*> existing;
		if (pl && doutputs)
			existing = pl->displayObjects();
		if (doutputs) {
			for (int i = 0; i < res.size(); ++i) {
				//build the list of created display objects
				QList<VipDisplayObject*> disps = res[i]->displayObjects();
				for (int j = 0; j < disps.size(); ++j)
					if (existing.indexOf(disps[j]) < 0)
						*doutputs << disps[j];
			}
		}
		return res;
	}

	//default behavior

	//default behavior: handle VipDisplayCurve, VipDisplayImage, VipDisplayHistogram, VipVideoPlayer and VipPlotPlayer
	if(VipDisplayCurve * curve = qobject_cast<VipDisplayCurve*>(disp))
	{
		if(VipPlotPlayer * player = qobject_cast<VipPlotPlayer*>(pl) )
		{
			setProcessingPool(curve, player->processingPool());
			if (VipPlotItem * item = qobject_cast<VipPlotItem*>(target))
				curve->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
			else
				curve->item()->setAxes(player->defaultXAxis(), player->defaultYAxis(), player->defaultCoordinateSystem());
			if (doutputs) *doutputs << curve;
			return QList<VipAbstractPlayer*>()<< pl;
		}
		else if(!pl)
		{
			VipPlotPlayer * plot_player = new VipPlotPlayer();
			plot_player->setWindowTitle(curve->item()->title().text());
			curve->item()->setAxes(plot_player->defaultXAxis(), plot_player->defaultYAxis(), plot_player->defaultCoordinateSystem());
			if (doutputs) *doutputs << curve;
			return QList<VipAbstractPlayer*>()<< plot_player;
		}
		else
			return QList<VipAbstractPlayer*>();
	}
	else if(VipDisplayHistogram * hist = qobject_cast<VipDisplayHistogram*>(disp))
	{
		if(VipPlotPlayer * player = qobject_cast<VipPlotPlayer*>(pl) )
		{
			setProcessingPool(hist, player->processingPool());
			if (VipPlotItem * item = qobject_cast<VipPlotItem*>(target))
				hist->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
			else
				hist->item()->setAxes(player->defaultXAxis(), player->defaultYAxis(), player->defaultCoordinateSystem());
			if (doutputs) *doutputs << hist;
			return QList<VipAbstractPlayer*>()<< pl;
		}
		else if(!pl)
		{
			VipPlotPlayer * plot_player = new VipPlotPlayer();
			plot_player->setWindowTitle(hist->item()->title().text());
			hist->item()->setAxes(plot_player->defaultXAxis(), plot_player->defaultYAxis(), plot_player->defaultCoordinateSystem());
			if (doutputs) *doutputs << hist;
			return QList<VipAbstractPlayer*>()<< plot_player;
		}
		else
			return QList<VipAbstractPlayer*>();
	}
	else if(VipDisplayImage * img = qobject_cast<VipDisplayImage*>(disp))
	{
		// if(VipVideoPlayer * player = qobject_cast<VipVideoPlayer*>(pl))
		// {
		// player->setSpectrogram(img->item());
		// return QList<VipAbstractPlayer*>()<< player;
		// }
		// else 
		if(pl) //we cannot add a VipDisplayImage in an existing player which is not a VipVideoPlayer
				return QList<VipAbstractPlayer*>();
		else
		{
			VipVideoPlayer * player = new VipVideoPlayer();
			player->setSpectrogram(img->item());
			player->setWindowTitle(img->item()->title().text());
			if (doutputs) *doutputs << img;
			return QList<VipAbstractPlayer*>()<< player;
		}
	}
	else if(VipDisplaySceneModel * scene_model = qobject_cast<VipDisplaySceneModel*>(disp))
	{
		//a VipDisplaySceneModel can be displays in both VipVideoPlayer and VipPlotPlayer. Default player type is VipVideoPlayer
		if(VipPlayer2D * player = qobject_cast<VipPlayer2D*>(pl))
		{
			if (VipVideoPlayer * video = qobject_cast<VipVideoPlayer*>(pl))
			{
				setProcessingPool(scene_model, player->processingPool());
				scene_model->item()->setMode(VipPlotSceneModel::Fixed);
				scene_model->item()->setAxes(video->viewer()->area()->bottomAxis(), video->viewer()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
				if (doutputs) *doutputs << scene_model;
				return QList<VipAbstractPlayer*>() << player;
			}
			else if (VipPlotPlayer * plot = qobject_cast<VipPlotPlayer*>(pl))
			{
				setProcessingPool(scene_model, player->processingPool());
				scene_model->item()->setMode(VipPlotSceneModel::Fixed);
				if (VipPlotItem * item = qobject_cast<VipPlotItem*>(target))
					scene_model->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
				else
					scene_model->item()->setAxes(plot->defaultXAxis(), plot->defaultYAxis(), plot->defaultCoordinateSystem());
				if (doutputs) *doutputs << scene_model;
				return QList<VipAbstractPlayer*>() << player;
			}
			else
				return QList<VipAbstractPlayer*>();
		}
		else
		{
			VipVideoPlayer * res = new VipVideoPlayer();
			res->setWindowTitle(scene_model->item()->title().text());
			scene_model->item()->setAxes(res->viewer()->area()->bottomAxis(), res->viewer()->area()->leftAxis(),VipCoordinateSystem::Cartesian);
			if (doutputs) *doutputs << scene_model;
			return QList<VipAbstractPlayer*>()<< res;
		}
	}
	else if(VipDisplayPlotItem * plot_item = qobject_cast<VipDisplayPlotItem*>(disp))
	{
		//any other VipDisplayPlotItem
		if(VipPlotPlayer * player = qobject_cast<VipPlotPlayer*>(pl) )
		{
			setProcessingPool(plot_item, player->processingPool());
			if (VipPlotItem * item = qobject_cast<VipPlotItem*>(target))
				plot_item->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
			else
				plot_item->item()->setAxes(player->defaultXAxis(), player->defaultYAxis(),player->defaultCoordinateSystem());
			if (doutputs) *doutputs << plot_item;
			return QList<VipAbstractPlayer*>()<< pl;
		}
		else if(VipVideoPlayer * video = qobject_cast<VipVideoPlayer*>(pl))
		{
			setProcessingPool(plot_item, video->processingPool());
			plot_item->item()->setAxes(video->viewer()->area()->bottomAxis(), video->viewer()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
			if (doutputs) *doutputs << plot_item;
			return QList<VipAbstractPlayer*>()<< pl;
		}
		else
		{
			VipPlotPlayer * res = new VipPlotPlayer();
			res->setWindowTitle(plot_item->item()->title().text());
			plot_item->item()->setAxes(res->defaultXAxis(), res->defaultYAxis(),res->defaultCoordinateSystem());
			if (doutputs) *doutputs << plot_item;
			return QList<VipAbstractPlayer*>()<< res;
		}
	}


	//special case: device having no output
	if(disp->topLevelOutputCount() == 0)
		return QList<VipAbstractPlayer*>();


	//compute all VipOutput object to use
	QVector<VipOutput*> proc_outputs;
	if(!output)
	{
		for(int i=0; i < disp->outputCount(); ++i)
		{
			if(disp->outputAt(i)->isEnabled())
				proc_outputs.append( disp->outputAt(i));
		}
	}
	else
		proc_outputs.append(output);


	//the VipDisplayObject we are going to create
	QList<VipDisplayObject*> displayObjects;
	//all the inputs from the device
	QVector<VipAnyData> outputs(proc_outputs.size());

	//try to read the device until we've got all outputs
	VipIODevice * device = qobject_cast<VipIODevice*>(disp);
	if(!device)
		device = disp->parentObjectPool();

	//directly read the processing outputs
	bool all_outputs = true;
	for(int i=0; i < proc_outputs.size(); ++i)
	{
		outputs[i] = proc_outputs[i]->data();
		if(outputs[i].isEmpty())
			all_outputs = false;
	}

	//if some outputs are still empty, read the VipProcessingPool until we have valid outputs
	if(device && !all_outputs)
	{
		qint64 time = device->firstTime();
		while(true)
		{
			device->read(time);

			bool has_all_output = true;
			for(int i=0; i < proc_outputs.size(); ++i)
			{
				proc_outputs[i]->parentProcessing()->wait();
				outputs[i] = proc_outputs[i]->data();
				if(outputs[i].isEmpty())
					has_all_output = false;
			}


			if(has_all_output)
				break;

			qint64 next = device->nextTime(time);
			if(time == next || next == VipInvalidTime)
				break;
			time = next;
		}
	}


	QList<VipAbstractPlayer*> players;

	//we've got all possible outputs, create the corresponding players
	for(int i=0; i < outputs.size(); ++i)
	{
		if(outputs[i].isEmpty())
			continue;


		QList<VipAbstractPlayer*> tmp;

		if(pl) //if a player is given, try to insert the data inside it
			tmp = vipCreatePlayersFromData(outputs[i], pl, proc_outputs[i],target,doutputs);
		else if (!players.size()) //no created players: create a new one
			tmp = vipCreatePlayersFromData(outputs[i], NULL, proc_outputs[i], target, doutputs);
		else //otherwise, try to insert it into an existing one
		{
			for (int p = 0; p < players.size(); ++p)
			{
				tmp = vipCreatePlayersFromData(outputs[i], players[p], proc_outputs[i], target, doutputs);
				if (tmp.size())
					break;
			}

			if(!tmp.size()) //we cannot insert it: try to create a new player
				tmp = vipCreatePlayersFromData(outputs[i], NULL, proc_outputs[i], target, doutputs);
		}

		players += tmp;

		//remove duplicates
		players = vipListUnique(players);
	}

	if(device)//VipProcessingPool * pool = disp->parentObjectPool())
		device->reload();//read(pool->time());

	return players;
}




QList<VipAbstractPlayer *> vipCreatePlayersFromProcessings(const QList<VipProcessingObject *> & procs, VipAbstractPlayer * pl, QObject * target, QList<VipDisplayObject*>* doutputs)
{
	if(!procs.size())
		return QList<VipAbstractPlayer *>();

	if(pl)
	{
		for(int i=0; i < procs.size(); ++i)
		{
			QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(procs[i],pl,NULL, target,doutputs);
			if(!players.size())
				return QList<VipAbstractPlayer *>();
		}
		return QList<VipAbstractPlayer *>()<<pl;
	}
	else
	{
		//create the new players
		QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(procs.first(),NULL,NULL, target, doutputs);

		for(int i=1; i < procs.size(); ++i)
		{
			//try to insert the VipDisplayObject into an existing player
			bool inserted = false;
			for(int p = 0; p < players.size(); ++p)
			{
				QList<VipAbstractPlayer*> tmp = vipCreatePlayersFromProcessing(procs[i],players[p],NULL, target, doutputs);
				if(tmp.size() )
				{
					inserted = true;
					break;
				}
			}

			//if not inserted, create a new player (if possible)
			if(!inserted)
			{
				QList<VipAbstractPlayer*> tmp = vipCreatePlayersFromProcessing(procs[i],NULL,NULL, target, doutputs);
				if(tmp.size())
				{
					players.append(tmp);
				}
			}
		}

		//make unique while keeping the order
		return vipListUnique(players);
	}
}

#include "VipProcessingObjectEditor.h"

QList<VipAbstractPlayer *> vipCreatePlayersFromStringList(const QStringList & lst, VipAbstractPlayer * player, QObject * target, QList<VipDisplayObject*> * doutputs)
{
	QList<VipProcessingObject*> devices;

	VipProgress p;
	p.setModal(true);
	p.setCancelable(true);
	if(lst.size() > 1)
		p.setRange(0, lst.size() );

	for (int i = 0; i < lst.size(); ++i)
	{
		VipIODevice * device = VipCreateDevice::create(lst[i]);
		if (device)
		{
			device->setPath(lst[i]);

			QString name = device->removePrefix(device->name());
			p.setText("<b>Open</b> " + QFileInfo(name).fileName());
			if (lst.size() > 1)
				p.setValue(i);

			//allow VipIODevice to display a progress bar
			device->setProperty("_vip_enableProgress", true);

			if (device->open(VipIODevice::ReadOnly))
			{
				devices << device;
				VIP_LOG_INFO("Open " + lst[i]);
			}
			else
				delete device;
		}

		if (p.canceled())
			break;
	}

	QList<VipAbstractPlayer *> res =  vipCreatePlayersFromProcessings(devices, player, target, doutputs);
	if (res.isEmpty()) {
		//delete devices
		for (int i = 0; i < devices.size(); ++i)
			delete devices[i];
	}
	return res;
}



QList<VipAbstractPlayer *> vipCreatePlayersFromPaths(const VipPathList & paths, VipAbstractPlayer * player, QObject * target, QList<VipDisplayObject*>* doutputs)
{
	QList<VipProcessingObject*> devices;

	VipProgress p;
	p.setModal(true);
	p.setCancelable(true);
	p.setRange(0, paths.size());

	for (int i = 0; i < paths.size(); ++i)
	{
		p.setText("Open <b>" + paths[i].canonicalPath() + "</b>");
		VipIODevice * device = VipCreateDevice::create(paths[i]);
		if (device)
		{
			device->setPath(paths[i].canonicalPath());
			device->setMapFileSystem(paths[i].mapFileSystem());

			if (device->open(VipIODevice::ReadOnly))
			{
				devices << device;
				VIP_LOG_INFO("Open " + paths[i].canonicalPath());
			}
			else
			{
				delete device;
			}
		}

		if (p.canceled())
			break;
	}

	QList<VipAbstractPlayer *> res= vipCreatePlayersFromProcessings(devices, player,target, doutputs);
	if (res.isEmpty()) {
		//delete devices
		for (int i = 0; i < devices.size(); ++i)
			delete devices[i];
	}
	return res;
}




VipProcessingObject * vipCreateProcessing(VipOutput* output, const VipProcessingObject::Info & info)
{
	VipProcessingPool * pool = NULL;
	VipProcessingList * lst = NULL;
	VipProcessingObject * res = info.create();
	if(!res)
		return NULL;
	//check output/input
	if(res->outputCount() == 0) goto error;
	if(res->inputCount() != 1) {
		if(VipMultiInput * multi = res->topLevelInputAt(0)->toMultiInput()){
			if(!multi->resize(1)) goto error;
		}
		else goto error;
	}
	//connect inputs
	if(!pool) pool = output->parentProcessing()->parentObjectPool();
	output->setConnection(res->inputAt(0));
	res->inputAt(0)->setData(output->data());

	res->setDeleteOnOutputConnectionsClosed(true);
	res->setParent(pool);
	lst = new VipProcessingList();
	lst->setDeleteOnOutputConnectionsClosed(true);
	lst->setParent(pool);
	lst->inputAt(0)->setConnection(res->outputAt(0));

	//run the processing at least once to have a valid output
	lst->update();
	if(res->hasError())
		goto error;

	res->setScheduleStrategy(VipProcessingObject::Asynchronous);
	lst->setScheduleStrategy(VipProcessingObject::Asynchronous);
	return lst;

error:
	if(res) delete res;
	if(lst) delete lst;
	return NULL;
}

VipProcessingObject * vipCreateDataFusionProcessing(const QList<VipOutput*> & outputs, const VipProcessingObject::Info & info)
{
	VipProcessingPool * pool = NULL;
	VipProcessingList * lst = NULL;
	VipProcessingObject * res = info.create();
	if(!res)
		return NULL;

	//check input count
	if(res->inputCount() != outputs.size()){
		if(VipMultiInput * multi = res->topLevelInputAt(0)->toMultiInput()){
			if(!multi->resize(outputs.size())) goto error;
		}
		else goto error;
	}
	//check output
	if(res->outputCount() == 0) goto error;

	//connect inputs
	for(int i=0; i < outputs.size(); ++i) {
		if(!pool) pool = outputs[i]->parentProcessing()->parentObjectPool();
		outputs[i]->setConnection(res->inputAt(i));
		res->inputAt(i)->setData(outputs[i]->data());
	}

	res->setDeleteOnOutputConnectionsClosed(true);
	res->setParent(pool);
	lst = new VipProcessingList();
	lst->setDeleteOnOutputConnectionsClosed(true);
	lst->setParent(pool);
	lst->inputAt(0)->setConnection(res->outputAt(0));

	//run the processing at least once to have a valid output
	lst->update();
	if(res->hasError())
		goto error;

	res->setScheduleStrategy(VipProcessingObject::Asynchronous);
	lst->setScheduleStrategy(VipProcessingObject::Asynchronous);
	return lst;

error:
	if(res) delete res;
	if(lst) delete lst;
	return NULL;
}

VipProcessingObject * vipCreateDataFusionProcessing(const QList<VipPlotItem*> & items, const VipProcessingObject::Info & info)
{
	QList<VipOutput*> procs;
	for(int i=0; i < items.size(); ++i) {
		if(VipDisplayObject * disp = items[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			if(VipOutput * out = disp->inputAt(0)->connection()->source())
				procs.append(out);
		}
		else return NULL;
	}
	if(procs.size() != items.size())
		return NULL;
	return vipCreateDataFusionProcessing(procs,info);

}





VipArchive & operator<<(VipArchive & stream, const VipDisplayObject * )
{
	//return stream.content("displayInGuiThread",r->displayInGuiThread());
	return stream;
}

VipArchive & operator>>(VipArchive & stream, VipDisplayObject * )
{
	//r->setDisplayInGuiThread(stream.read("displayInGuiThread").value<bool>());
	return stream;
}


VipArchive & operator<<(VipArchive & stream, const VipDisplayPlotItem * r)
{
	return stream.content("item",r->m_data->item).content("itemSuppressable",r->itemSuppressable());
}

VipArchive & operator>>(VipArchive & stream, VipDisplayPlotItem * r)
{
	r->m_data->item = stream.read("item").value<VipLazyPointer>();
	r->setItemSuppressable(stream.read("itemSuppressable").value<bool>());
	return stream;
}



static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipDisplayObject*>();
	vipRegisterArchiveStreamOperators<VipDisplayPlotItem*>();
	return 0;
}

static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);



