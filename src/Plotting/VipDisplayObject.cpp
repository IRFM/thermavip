#include <cstring>
#include <chrono>
#include <thread>
#include <condition_variable>

#include "VipDisplayObject.h"
#include "VipXmlArchive.h"
#include "VipCore.h"
#include "VipUniqueId.h"
#include "VipSleep.h"

#include <QGraphicsView>
#include <QCoreApplication>
#include <qtimer.h>



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

	VipSpinlock lock;
	std::condition_variable_any cond;
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
	if (!isEnabled()  || !inputAt(0)->hasNewData())
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
				vipProcessEvents(NULL, 100);
				break;
			}
		}
	}
	else {
		Q_EMIT displayed(buffer);
		m_data->displayInProgress = false;
	}
	

	

}

static QWidget* findWidgetWith_automaticWindowTitle(QWidget* w) {
	
	while (w) {

		if (w->metaObject()->indexOfProperty("automaticWindowTitle") >= 0)
			return w;
		w = w->parentWidget();
	}
	return nullptr;
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
				QWidget* player = findWidgetWith_automaticWindowTitle(widget());
				if (player && !title.isEmpty()){
					if (player->property("automaticWindowTitle").toBool()) {
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
					//if(VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(this->widget()))
					//	pl->viewer()->area()->colorMapAxis()->setTitle(VipText(new_title, t.textStyle()));
					//else
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

QWidget* VipDisplayPlotItem::widget() const
{
	VipPlotItem * it = item();
	if(!it)
		return NULL;

	QWidget * w = it->view();
	return w;
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
	while (w ) {
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

		QWidget * player = widget();
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
	return v.canConvert<VipPointVector>() || v.canConvert<VipComplexPointVector>() || v.canConvert<VipComplexPoint>() || v.canConvert<double>() || v.canConvert<VipPoint>();
}

void VipDisplayCurve::setItem(VipPlotCurve* it)
{
	if (it && it != item()) {
		it->setAutoMarkDirty(false);
		VipDisplayPlotItem::setItem(it);
	}
}

bool VipDisplayCurve::prepareForDisplay(const VipAnyDataList & lst)
{
	if (VipPlotCurve * curve = item())
	{
		//create the curve
		VipPointVector vector;
		VipComplexPointVector cvector;

		m_data->is_full_vector = false;
		for (int i = 0; i < lst.size(); ++i)
		{
			const VipAnyData & any = lst[i];
			const QVariant & v = any.data();

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
				//if (vector.isEmpty())
				//	vector = curve->rawData();
				vector.append(v.value<VipPoint>());
			}
			else if (v.userType() == qMetaTypeId<complex_d>())
			{
				cvector.append(VipComplexPoint(any.time(), v.value<complex_d>()));
			}
			else if (v.canConvert(QMetaType::Double) && any.time() != VipInvalidTime)
			{
				//if (vector.isEmpty())
				//	vector = curve->rawData();
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
			const VipAnyData & data = lst.back();
			const QVariant & v = data.data();
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
			const QVariant & v = data.last().data();
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
			const VipAnyData & data = lst.back();

			formatItemIfNecessary(curve, data);
		}
	}
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









VipPlotItem * vipCopyPlotItem(const VipPlotItem* item)
{
	VipXOStringArchive arch;
	arch.content("item", QVariant::fromValue(const_cast<VipPlotItem*>(item)));

	VipXIStringArchive iarch(arch.toString());
	iarch.setProperty("_vip_no_id_or_scale", true);
	return iarch.read("item").value<VipPlotItem*>();
}

QByteArray vipSavePlotItemState(const VipPlotItem* item)
{
	VipXOStringArchive arch; 
	arch.content("item", QVariant::fromValue(const_cast<VipPlotItem*>(item)));
	return arch.toString().toLatin1();
}

bool vipRestorePlotItemState(VipPlotItem* item, const QByteArray & state)
{
	VipXIStringArchive iarch(state);
	iarch.setProperty("_vip_no_id_or_scale", true);
	return iarch.content("item", item);
}

VipArchive & operator<<(VipArchive & arch, const VipPlotItem * value)
{
	arch.content("id", VipUniqueId::id(value))
		.content("title", value->title())
		.content("attributes", (int)value->itemAttributes())
		.content("renderHints", (int)value->renderHints())
		.content("compositionMode", (int)value->compositionMode())
		.content("selectedPen", value->selectedPen())
		.content("axisUnits", value->axisUnits())
		.content("visible", value->isVisible());

	// save text style and color palette (4.2.0)
	arch.content("testStyle", value->textStyle());
	arch.content("colorPalette", value->colorPalette());

	//save the color map
	if (value->colorMap())
		//new in 2.2.17: save id as a VipAbstractScale instead of VipAxisColorMap
		arch.content("colorMap", VipUniqueId::id<VipAbstractScale>(value->colorMap()));
	else
		arch.content("colorMap", 0);

	//save the axes
	arch.content("coordinateSystem", (int)value->coordinateSystemType());
	QList<VipAbstractScale*> scales = value->axes();
	arch.content("axisCount", scales.size());
	for (int i = 0; i < scales.size(); ++i)
		arch.content("axisId", VipUniqueId::id(scales[i]));

	//save the properties
	QList<QByteArray> names = value->dynamicPropertyNames();
	QVariantMap properties;
	for (int i = 0; i < names.size(); ++i)
		if (!names[i].startsWith("_q_"))
		{
			QVariant v = value->property(names[i]);
			if (v.userType() > 0 && v.userType() < QMetaType::User)
			{
				properties[names[i]] = v;
			}
		}

	arch.content("properties", properties);

	//save the additional texts
	const QMap<int, VipPlotItem::ItemText> texts = value->texts();

	arch.content("textCount", texts.size());
	arch.start("texts");
	for (QMap<int, VipPlotItem::ItemText>::const_iterator it = texts.begin(); it != texts.end(); ++it)
	{
		arch.content("text", it.value().text);
		arch.content("position", (int)it.value().position);
		arch.content("alignment", (int)it.value().alignment);
	}
	arch.end();

	
	arch.content("styleSheet", value->styleSheetString());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotItem * value)
{
	int id = arch.read("id").value<int>();
	if (!arch.property("_vip_no_id_or_scale").toBool())
		VipUniqueId::setId(value, id);
	value->setTitle(arch.read("title").value<VipText>());
	value->setItemAttributes(VipPlotItem::ItemAttributes(arch.read("attributes").value<int>()));
	value->setRenderHints(QPainter::RenderHints(arch.read("renderHints").value<int>()));
	value->setCompositionMode(QPainter::CompositionMode(arch.read("compositionMode").value<int>()));
	value->setSelectedPen(arch.read("selectedPen").value<QPen>());
	QList<VipText> units = arch.read("axisUnits").value<QList<VipText> >();
	for (int i = 0; i < units.size(); ++i)
		value->setAxisUnit(i, units[i]);
	value->setVisible(arch.read("visible").toBool());

	// read text style and color palette (4.2.0)
	VipTextStyle style;
	VipColorPalette palette;
	arch.save();
	arch.content("testStyle", style);
	if (arch.content("colorPalette", palette)) {
		value->setTextStyle(style);
		value->setColorPalette(palette);
	}
	else
		arch.restore();

	//load the color map
	id = arch.read("colorMap").toInt();
	if (id && !arch.property("_vip_no_id_or_scale").toBool())
	{
		//new in 2.2.17: interpret id as a VipAbstractScale instead of VipAxisColorMap
		VipAxisColorMap * axis = qobject_cast<VipAxisColorMap*>(VipUniqueId::find<VipAbstractScale>(id));
		if (!axis)
			axis = VipUniqueId::find<VipAxisColorMap>(id);
		if (axis)
			value->setColorMap(axis);
	}

	//try to set the axes
	int coordinateSystem = arch.read("coordinateSystem").toInt();
	int count = arch.read("axisCount").toInt();
	if (count)
	{
		QList<VipAbstractScale*> scales;
		for (int i = 0; i < count; ++i)
		{
			VipAbstractScale * scale = VipUniqueId::find<VipAbstractScale>(arch.read("axisId").toInt());
			scales.append(scale);
		}
		if (!arch.property("_vip_no_id_or_scale").toBool())
			value->setAxes(scales, (VipCoordinateSystem::Type)coordinateSystem);
	}

	arch.save();
	QVariantMap properties;
	if (arch.content("properties", properties))
	{
		for (QVariantMap::const_iterator it = properties.begin(); it != properties.end(); ++it)
			value->setProperty(it.key().toLatin1().data(), it.value());
	}
	else
		arch.restore();

	//read additional texts
	count = arch.read("textCount").toInt();
	if (count && (bool)arch.start("texts"))
	{
		while (arch) {
			VipText text = arch.read("text").value<VipText>();
			Vip::RegionPositions position = (Vip::RegionPositions)arch.read("position").toInt();
			Qt::Alignment alignment = (Qt::Alignment)arch.read("alignment").toInt();

			if (arch) {
				value->addText(text, position, alignment);
			}
		}
		arch.end();
	}
	arch.resetError();

	

	arch.save();
	QString st;
	if (arch.content("styleSheet", st))
		value->setStyleSheet(st);
	else
		arch.restore();

	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotItemData * value)
{
	//arch.content("maxSampleCount", value->maxSampleCount());
	QVariant v = value->data();
	if (v.userType() == qMetaTypeId<VipPointVector>()) {
		//for VipPointVector only, downsample to 100 points to avoid having too big session files
		const VipPointVector pts = v.value<VipPointVector>();
		if (pts.size() > 100) {
			double step = pts.size() / 100.0;
			VipPointVector tmp;
			for (double s = 0; s < pts.size(); s += step) {
				int index = (int)s;
				tmp.push_back(pts[index]);
			}
			v = vipToVariant(tmp);
		}
	}
	arch.content("data", v);
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotItemData * value)
{
	//value->setMaxSampleCount(arch.read("maxSampleCount").toInt());
	value->setData(arch.read("data"));
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotCurve * value)
{
	arch.content("legendAttributes", (int)value->legendAttributes());
	arch.content("curveAttributes", (int)value->curveAttributes());
	arch.content("boxStyle", value->boxStyle());
	arch.content("baseline", value->baseline());
	arch.content("curveStyle", (int)value->style());
	if (value->symbol())
		arch.content("symbol", *value->symbol());
	else
		arch.content("symbol", VipSymbol());
	arch.content("symbolVisible", value->symbolVisible());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotCurve * value)
{
	value->setLegendAttributes(VipPlotCurve::LegendAttributes(arch.read("legendAttributes").value<int>()));
	value->setCurveAttributes(VipPlotCurve::CurveAttributes(arch.read("curveAttributes").value<int>()));
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setBaseline(arch.read("baseline").value<double>());
	value->setStyle(VipPlotCurve::CurveStyle(arch.read("curveStyle").value<int>()));
	value->setSymbol(new VipSymbol(arch.read("symbol").value<VipSymbol>()));
	value->setSymbolVisible(arch.read("symbolVisible").toBool());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotHistogram * value)
{
	arch.content("boxStyle", value->boxStyle())
		.content("textPosition", (int)value->textPosition())
		.content("textDistance", value->textDistance())
		.content("text", value->text())
		.content("baseline", value->baseline())
		.content("style", (int)value->style());

	// new in 4.2.0
	
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textAlignment", value->textAlignment());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());

	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotHistogram * value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	value->setBaseline(arch.read("baseline").value<double>());
	value->setStyle((VipPlotHistogram::HistogramStyle)arch.read("style").value<int>());

	arch.save();

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	if (arch) {

		value->setTextTransform(textTransform, textTransformReference);
		value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
		value->setTextDistance(arch.read("textDistance").value<double>());
		value->setText(arch.read("text").value<VipText>());
	}
	else
		arch.restore();

	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotGrid * value)
{
	arch.content("minorPen", value->minorPen());
	arch.content("majorPen", value->majorPen());
	arch.content("_vip_customDisplay", value->property("_vip_customDisplay").toInt());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotGrid * value)
{
	value->setMinorPen(arch.read("minorPen").value<QPen>());
	value->setMajorPen(arch.read("majorPen").value<QPen>());
	//new in 2.2.18
	int _vip_customDisplay;
	if (arch.content("_vip_customDisplay", _vip_customDisplay))
		value->setProperty("_vip_customDisplay", _vip_customDisplay);
	else
		arch.restore();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotCanvas * value)
{
	arch.content("boxStyle", value->boxStyle());
	//new in 2.2.18
	arch.content("_vip_customDisplay", value->property("_vip_customDisplay").toInt());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotCanvas * value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	//new in 2.2.18
	int _vip_customDisplay;
	if (arch.content("_vip_customDisplay", _vip_customDisplay))
		value->setProperty("_vip_customDisplay", _vip_customDisplay);
	else
		arch.restore();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotMarker * value)
{
	arch.content("lineStyle", (int)value->lineStyle())
		.content("linePen", value->linePen())
		.content("label", value->label())
		.content("labelAlignment", (int)value->labelAlignment())
		.content("labelOrientation", (int)value->labelOrientation())
		.content("spacing", value->spacing());
	if (value->symbol())
		return arch.content("symbol", *value->symbol());
	else
		return arch.content("symbol", VipSymbol());
}

VipArchive & operator>>(VipArchive & arch, VipPlotMarker * value)
{
	value->setLineStyle((VipPlotMarker::LineStyle)arch.read("lineStyle").value<int>());
	value->setLinePen(arch.read("linePen").value<QPen>());
	value->setLabel(arch.read("label").value<VipText>());
	value->setLabelAlignment((Qt::AlignmentFlag)arch.read("labelAlignment").value<int>());
	value->setLabelOrientation((Qt::Orientation)arch.read("labelOrientation").value<int>());
	value->setSpacing(arch.read("spacing").value<double>());
	value->setSymbol(new VipSymbol(arch.read("symbol").value<VipSymbol>()));
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotRasterData *)
{
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotRasterData *)
{
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotSpectrogram * value)
{
	arch.content("defaultContourPen", value->defaultContourPen());
	arch.content("ignoreAllVerticesOnLevel", value->ignoreAllVerticesOnLevel());
	QList< vip_double > levels = value->contourLevels();
	for (int i = 0; i < levels.size(); ++i)
		arch.content("level", levels[i]);
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotSpectrogram * value)
{
	value->setDefaultContourPen(arch.read("defaultContourPen").value<QPen>());
	value->setIgnoreAllVerticesOnLevel(arch.read("ignoreAllVerticesOnLevel").value<bool>());
	QList< vip_double > levels;
	while (true)
	{
		QVariant tmp = arch.read();
		if (tmp.userType() == 0)
			break;
		levels.append(tmp.toDouble());
	}
	value->setContourLevels(levels);
	arch.resetError();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotShape * value)
{
	arch.content("dawComponents", (int)value->dawComponents());
	arch.content("textStyle", value->textStyle());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("adjustTextColor", (int)value->adjustTextColor());

	// new in 4.2.0
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());

	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipPlotShape * value)
{
	value->setDrawComponents((VipPlotShape::DrawComponents)arch.read("dawComponents").value<int>());
	value->setTextStyle(arch.read("textStyle").value<VipTextStyle>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());
	value->setTextAlignment((Qt::AlignmentFlag)arch.read("textAlignment").value<int>());
	arch.save();
	value->setAdjustTextColor(arch.read("adjustTextColor").value<bool>());
	if (!arch)
		arch.restore();
	else {
	
		arch.save();
		QTransform textTransform = arch.read("textTransform").value<QTransform>();
		QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
		if (arch) {

			value->setTextTransform(textTransform, textTransformReference);
			value->setTextDistance(arch.read("textDistance").value<double>());
			value->setText(arch.read("text").value<VipText>());
		}
		else
			arch.restore();
	}
	arch.resetError();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipPlotSceneModel * value)
{
	//mark internal shapes as non serializable, they will recreated when reloading the VipPlotSceneModel
	for (int i = 0; i < value->count(); ++i) {
		if (VipPlotShape * sh = qobject_cast<VipPlotShape*>(value->at(i))) {
			sh->setProperty("_vip_no_serialize", true);
			if (VipResizeItem * re = (sh->property("VipResizeItem").value<VipResizeItemPtr>()))
				re->setProperty("_vip_no_serialize", true);
		}
	}

	return arch.content("mode", (int)value->mode()).content("sceneModel", value->sceneModel());
}

VipArchive & operator>>(VipArchive & arch, VipPlotSceneModel * value)
{
	value->setMode((VipPlotSceneModel::Mode)arch.read("mode").toInt());
	value->setSceneModel(arch.read("sceneModel").value<VipSceneModel>());
	return arch;
}



VipArchive& operator<<(VipArchive& arch, const VipPlotBarChart* value)
{
	arch.content("boxStyle", value->boxStyle());
	arch.content("valueType", (int)value->valueType());
	arch.content("baseline", value->baseline());
	arch.content("spacing", value->spacing());
	arch.content("spacingUnit", (int)value->spacingUnit());
	arch.content("barWidth", value->barWidth());
	arch.content("barWidthUnit", (int)value->barWidthUnit());
	arch.content("style", (int)value->style());

	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());
	arch.content("barNames", value->barNames());
	return arch;
}
VipArchive& operator>>(VipArchive& arch, VipPlotBarChart* value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setValueType((VipPlotBarChart::ValueType)arch.read("valueType").value<int>());
	value->setBaseline(arch.read("baseline").value<double>());
	double spacing = arch.read("spacing").value<double>();
	int spacingUnit = arch.read("spacingUnit").value<int>();
	value->setSpacing(spacing, (VipPlotBarChart::WidthUnit)spacingUnit);
	double barWidth = arch.read("barWidth").value<double>();
	int barWidthUnit = arch.read("barWidthUnit").value<int>();
	value->setBarWidth(barWidth, (VipPlotBarChart::WidthUnit)barWidthUnit);
	value->setStyle((VipPlotBarChart::Style) arch.read("style").value<int>());
	value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	value->setTextTransform(textTransform, textTransformReference);
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	value->setBarNames(arch.read("barNames").value<VipTextList>());

	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotQuiver* value)
{
	arch.content("quiverPath", value->quiverPath());
	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotQuiver* value)
{
	value->setQuiverPath(arch.read("quiverPath").value<VipQuiverPath>());
	value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	value->setTextTransform(textTransform, textTransformReference);
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipPlotScatter* value)
{
	arch.content("sizeUnit",(int) value->sizeUnit());
	arch.content("useValueAsSize", value->useValueAsSize());
	arch.content("symbol", value->symbol());
	
	arch.content("textAlignment", (int)value->textAlignment());
	arch.content("textPosition", (int)value->textPosition());
	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotScatter* value)
{
	value->setSizeUnit((VipPlotScatter::SizeUnit)arch.read("sizeUnit").value<int>());
	value->setUseValueAsSize(arch.read("useValueAsSize").value<bool>());
	value->setSymbol(arch.read("symbol").value<VipSymbol>());

	value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	value->setTextTransform(textTransform, textTransformReference);
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	return arch;
}









static DoubleVector toDoubleVector(const DoubleList & lst)
{
	DoubleVector res;
	res.resize(lst.size());
	for (int i = 0; i < lst.size(); ++i)
		res[i] = lst[i];
	return res;
}

VipArchive & operator<<(VipArchive & arch, const VipScaleDiv & value)
{
	arch.content("MinorTicks", value.ticks(VipScaleDiv::MinorTick));
	arch.content("MediumTick", value.ticks(VipScaleDiv::MediumTick));
	arch.content("MajorTick", value.ticks(VipScaleDiv::MajorTick));
	arch.content("lowerBound", value.lowerBound());
	arch.content("upperBound", value.upperBound());
	return arch;
}

VipArchive & operator >> (VipArchive & arch, VipScaleDiv & value)
{
	value.setTicks(VipScaleDiv::MinorTick, arch.read("MinorTicks").value<DoubleVector>());
	value.setTicks(VipScaleDiv::MediumTick, arch.read("MediumTick").value<DoubleVector>());
	value.setTicks(VipScaleDiv::MajorTick, arch.read("MajorTick").value<DoubleVector>());
	value.setLowerBound(arch.read("lowerBound").toDouble());
	value.setUpperBound(arch.read("upperBound").toDouble());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAbstractScale * value)
{
	arch.content("id", VipUniqueId::id(value));
	arch.content("boxStyle", value->boxStyle());
	arch.content("isAutoScale", value->isAutoScale());
	arch.content("title", value->title());
	arch.content("majorTextStyle", value->textStyle(VipScaleDiv::MajorTick));
	arch.content("mediumTextStyle", value->textStyle(VipScaleDiv::MediumTick));
	arch.content("minorTextStyle", value->textStyle(VipScaleDiv::MinorTick));
	arch.content("majorTransform", value->labelTransform(VipScaleDiv::MajorTick));
	arch.content("mediumTransform", value->labelTransform(VipScaleDiv::MediumTick));
	arch.content("minorTransform", value->labelTransform(VipScaleDiv::MinorTick));
	arch.content("isDrawTitleEnabled", value->isDrawTitleEnabled());
	arch.content("startBorderDist", value->startBorderDist());
	arch.content("endBorderDist", value->endBorderDist());
	arch.content("startMinBorderDist", value->startMinBorderDist());
	arch.content("endMinBorderDist", value->endMinBorderDist());
	arch.content("startMaxBorderDist", value->startMaxBorderDist());
	arch.content("endMaxBorderDist", value->endMaxBorderDist());
	arch.content("margin", value->margin());
	arch.content("spacing", value->spacing());
	arch.content("isScaleInverted", value->isScaleInverted());
	arch.content("maxMajor", value->maxMajor());
	arch.content("maxMinor", value->maxMinor());
	//new in 3.0.1
	arch.content("autoExponent", value->constScaleDraw()->valueToText()->automaticExponent());
	arch.content("minLabelSize", value->constScaleDraw()->valueToText()->maxLabelSize());
	arch.content("exponent", value->constScaleDraw()->valueToText()->exponent());

	arch.content("scaleDiv", value->scaleDiv());
	arch.content("renderHints", (int)value->renderHints());
	arch.content("visible", (int)value->isVisible());
	//save the y scale engine type
	arch.content("yScaleEngine", value->scaleEngine() ? (int)value->scaleEngine()->scaleType() : 0);

	arch.content("styleSheet", value->styleSheetString());

	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipAbstractScale * value)
{
	VipUniqueId::setId(value, arch.read("id").toInt());
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setAutoScale(arch.read("isAutoScale").value<bool>());
	value->setTitle(arch.read("title").value<VipText>());
	value->setTextStyle(arch.read("majorTextStyle").value<VipTextStyle>(), VipScaleDiv::MajorTick);
	value->setTextStyle(arch.read("mediumTextStyle").value<VipTextStyle>(), VipScaleDiv::MediumTick);
	value->setTextStyle(arch.read("minorTextStyle").value<VipTextStyle>(), VipScaleDiv::MinorTick);
	value->setLabelTransform(arch.read("majorTransform").value<QTransform>(), VipScaleDiv::MajorTick);
	value->setLabelTransform(arch.read("mediumTransform").value<QTransform>(), VipScaleDiv::MediumTick);
	value->setLabelTransform(arch.read("minorTransform").value<QTransform>(), VipScaleDiv::MinorTick);
	value->enableDrawTitle(arch.read("isDrawTitleEnabled").value<bool>());
	double startBorderDist = arch.read("startBorderDist").value<double>();
	double endBorderDist = arch.read("endBorderDist").value<double>();
	value->setBorderDist(startBorderDist, endBorderDist);
	double startMinBorderDist = arch.read("startMinBorderDist").value<double>();
	double endMinBorderDist = arch.read("endMinBorderDist").value<double>();
	value->setMinBorderDist(startMinBorderDist, endMinBorderDist);
	double startMaxBorderDist = arch.read("startMaxBorderDist").value<double>();
	double endMaxBorderDist = arch.read("endMaxBorderDist").value<double>();
	value->setMaxBorderDist(startMaxBorderDist, endMaxBorderDist);
	value->setMargin(arch.read("margin").value<double>());
	value->setSpacing(arch.read("spacing").value<double>());
	value->setScaleInverted(arch.read("isScaleInverted").value<bool>());
	value->setMaxMajor(arch.read("maxMajor").value<int>());
	value->setMaxMinor(arch.read("maxMinor").value<int>());

	//new in 3.0.1
	arch.save();
	bool autoExponent = false;
	int minLabelSize = 0, exponent=0;
	if (arch.content("autoExponent", autoExponent)) {
		arch.content("minLabelSize", minLabelSize);
		arch.content("exponent", exponent);
		value->scaleDraw()->valueToText()->setAutomaticExponent(autoExponent);
		value->scaleDraw()->valueToText()->setMaxLabelSize(minLabelSize);
		value->scaleDraw()->valueToText()->setExponent(exponent);
	}
	else
		arch.restore();

	value->setScaleDiv(arch.read("scaleDiv").value<VipScaleDiv>());
	value->setRenderHints((QPainter::RenderHints)arch.read("renderHints").value<int>());
	value->setVisible(arch.read("visible").toBool());
	int engine = arch.read("yScaleEngine").toInt();
	if (!value->scaleEngine() || engine != value->scaleEngine()->scaleType()) {
		if (engine == VipScaleEngine::Linear) value->setScaleEngine(new VipLinearScaleEngine());
		else if (engine == VipScaleEngine::Log10) value->setScaleEngine(new VipLog10ScaleEngine());
	}
	
	arch.resetError();

	
	arch.save();
	QString st;
	if (arch.content("styleSheet", st)) {
		if (!st.isEmpty())
			value->setStyleSheet(st);
	}
	else
		arch.restore();

	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAxisBase * value)
{
	arch.content("isMapScaleToScene", value->isMapScaleToScene());
	arch.content("isTitleInverted", value->isTitleInverted());
	arch.content("titleInside", value->titleInside());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipAxisBase * value)
{
	value->setMapScaleToScene(arch.read("isMapScaleToScene").value<bool>());
	value->setTitleInverted(arch.read("isTitleInverted").value<bool>());
	arch.save();
	//since 2.2.18
	bool titleInside;
	if (arch.content("titleInside", titleInside))
		value->setTitleInside(titleInside);
	else
		arch.restore();
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipColorMap * value)
{
	arch.content("format", (int)value->format());
	arch.content("externalValue", (int)value->externalValue());
	arch.content("externalColor", value->externalColor());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipColorMap * value)
{
	value->setFormat((VipColorMap::Format)arch.read("format").value<int>());
	VipColorMap::ExternalValue ext_value = (VipColorMap::ExternalValue)arch.read("externalValue").value<int>();
	QRgb ext_color = arch.read("externalColor").value<int>();
	value->setExternalValue(ext_value, ext_color);
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipLinearColorMap * value)
{
	arch.content("type", (int)value->type());
	return arch.content("gradientStops", value->gradientStops());
}

VipArchive & operator>>(VipArchive & arch, VipLinearColorMap * value)
{
	value->setType((VipLinearColorMap::StandardColorMap)arch.read("type").value<int>());
	value->setGradientStops(arch.read("gradientStops").value<QGradientStops>());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAlphaColorMap * value)
{
	return arch.content("color", value->color());
}

VipArchive & operator>>(VipArchive & arch, VipAlphaColorMap * value)
{
	value->setColor(arch.read("color").value<QColor>());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipAxisColorMap * value)
{
	arch.content("gripInterval", value->gripInterval());
	arch.content("colorMap", value->colorMap());
	arch.content("isColorBarEnabled", value->isColorBarEnabled());
	arch.content("colorBarWidth", value->colorBarWidth());
	arch.content("colorMapInterval", value->colorMapInterval());

	//since 2.2.18
	arch.content("hasAutoScaleMax", value->hasAutoScaleMax());
	arch.content("autoScaleMax", value->autoScaleMax());
	arch.content("hasAutoScaleMin", value->hasAutoScaleMin());
	arch.content("autoScaleMin", value->autoScaleMin());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipAxisColorMap * value)
{
	VipInterval inter = arch.read("gripInterval").value<VipInterval>();
	value->setColorMap(inter, arch.read("colorMap").value<VipColorMap*>());
	value->setGripInterval(inter);
	value->setColorBarEnabled(arch.read("isColorBarEnabled").value<bool>());
	value->setColorBarWidth(arch.read("colorBarWidth").value<double>());
	value->setColorMapInterval(arch.read("colorMapInterval").value<VipInterval>());

	//since 2.2.18
	bool hasAutoScaleMax, hasAutoScaleMin;
	vip_double autoScaleMax, autoScaleMin;
	arch.save();
	if (arch.content("hasAutoScaleMax", hasAutoScaleMax)) {
		arch.content("autoScaleMax", autoScaleMax);
		arch.content("hasAutoScaleMin", hasAutoScaleMin);
		arch.content("autoScaleMin", autoScaleMin);
		value->setHasAutoScaleMax(hasAutoScaleMax);
		value->setHasAutoScaleMin(hasAutoScaleMin);
		value->setAutoScaleMax(autoScaleMax);
		value->setAutoScaleMin(autoScaleMin);
	}
	else
		arch.restore();

	return arch;
}



VipArchive & operator<<(VipArchive & arch, const VipPlotArea2D * value)
{
	arch.content("leftAxis", value->leftAxis());
	arch.content("rightAxis", value->rightAxis());
	arch.content("topAxis", value->topAxis());
	arch.content("bottomAxis", value->bottomAxis());
	arch.content("leftAxisVisible", value->leftAxis()->isVisible());
	arch.content("rightAxisVisible", value->rightAxis()->isVisible());
	arch.content("topAxisVisible", value->topAxis()->isVisible());
	arch.content("bottomAxisVisible", value->bottomAxis()->isVisible());
	arch.content("grid", value->grid());
	arch.content("canvas", value->canvas());
	//since 2.2.18
	arch.content("title", value->titleAxis());
	return arch;
}
VipArchive & operator>>(VipArchive & arch, VipPlotArea2D * value)
{
	arch.content("leftAxis", value->leftAxis());
	arch.content("rightAxis", value->rightAxis());
	arch.content("topAxis", value->topAxis());
	arch.content("bottomAxis", value->bottomAxis());
	value->leftAxis()->setVisible(arch.read("leftAxisVisible").toBool());
	value->rightAxis()->setVisible(arch.read("rightAxisVisible").toBool());
	value->topAxis()->setVisible(arch.read("topAxisVisible").toBool());
	value->bottomAxis()->setVisible(arch.read("bottomAxisVisible").toBool());
	arch.content("grid", value->grid());
	arch.content("canvas", value->canvas());
	//since 2.2.18
	arch.save();
	if (!arch.content("title", value->titleAxis()))
		arch.restore();

	return arch;
}


static int registerStreamOperators2()
{

	qRegisterMetaType<DoubleList>("DoubleList");
	qRegisterMetaTypeStreamOperators<DoubleList>();
	qRegisterMetaType<DoubleVector>("DoubleVector");
	qRegisterMetaTypeStreamOperators<DoubleVector>();
	QMetaType::registerConverter<DoubleList, DoubleVector>(toDoubleVector);

	vipRegisterArchiveStreamOperators<VipScaleDiv>();
	vipRegisterArchiveStreamOperators<VipPlotItem*>();
	vipRegisterArchiveStreamOperators<VipPlotItemData*>();
	vipRegisterArchiveStreamOperators<VipPlotCurve*>();
	vipRegisterArchiveStreamOperators<VipPlotHistogram*>();
	vipRegisterArchiveStreamOperators<VipPlotGrid*>();
	vipRegisterArchiveStreamOperators<VipPlotCanvas*>();
	vipRegisterArchiveStreamOperators<VipPlotMarker*>();
	vipRegisterArchiveStreamOperators<VipPlotQuiver*>();
	vipRegisterArchiveStreamOperators<VipPlotScatter*>();
	vipRegisterArchiveStreamOperators<VipPlotBarChart*>();
	vipRegisterArchiveStreamOperators<VipPlotRasterData*>();
	vipRegisterArchiveStreamOperators<VipPlotSpectrogram*>();
	vipRegisterArchiveStreamOperators<VipPlotShape*>();
	vipRegisterArchiveStreamOperators<VipPlotSceneModel*>();
	vipRegisterArchiveStreamOperators<VipAbstractScale*>();
	vipRegisterArchiveStreamOperators<VipAxisBase*>();
	vipRegisterArchiveStreamOperators<VipColorMap*>();
	vipRegisterArchiveStreamOperators<VipLinearColorMap*>();
	vipRegisterArchiveStreamOperators<VipAlphaColorMap*>();
	vipRegisterArchiveStreamOperators<VipAxisColorMap*>();
	vipRegisterArchiveStreamOperators<VipPlotArea2D*>();

	return 0;
}

static int _registerStreamOperators2 = vipPrependInitializationFunction(registerStreamOperators2);

