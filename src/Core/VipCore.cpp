#include "VipCore.h"
#include "VipArchive.h"
#include "VipUniqueId.h"
#include "VipDataType.h"
#include "VipSceneModel.h"

#include <QTextStream>
#include <QMetaType>
#include <QSet>
#include <QStandardPaths>


bool vipSafeVariantSave(QDataStream &s, const QVariant & v)
{
	qint64 pos = s.device()->pos();
	quint32 typeId = v.type();
	bool fakeUserType = false;
	if (s.version() < QDataStream::Qt_5_0) {
		if (typeId == QMetaType::User) {
			typeId = 127; // QVariant::UserType had this value in Qt4
		}
		else if (typeId >= 128 - 97 && typeId <= QMetaType::LastCoreType) {
			// In Qt4 id == 128 was FirstExtCoreType. In Qt5 ExtCoreTypes set was merged to CoreTypes
			// by moving all ids down by 97.
			typeId += 97;
		}
		else if (typeId == QMetaType::QSizePolicy) {
			typeId = 75;
		}
		else if (typeId >= QMetaType::QKeySequence && typeId <= QMetaType::QQuaternion) {
			// and as a result these types received lower ids too
			typeId += 1;
		}
		else if (typeId == QMetaType::QPolygonF) {
			// This existed in Qt 4 only as a custom type
			typeId = 127;
			fakeUserType = true;
		}
	}
	s << typeId;
	if (s.version() >= QDataStream::Qt_4_2)
		s << qint8(v.userType() > 0);
	if ((uint)v.userType() >= QVariant::UserType || fakeUserType) {
		s << QMetaType::typeName(v.userType());
	}

	if (!v.userType() ) {
		if (s.version() < QDataStream::Qt_5_0)
			s << QString();
		return true;
	}

	if (!QMetaType::save(s,v.userType(), v.constData())) {
		s.device()->seek(pos);
		return false;
	}
	return true;
}


int  vipSafeVariantMapSave(QDataStream &s, const QVariantMap & c)
{
	QByteArray ar;
	QDataStream str(&ar, QIODevice::WriteOnly);
	str.setByteOrder(s.byteOrder());
	str.setFloatingPointPrecision(s.floatingPointPrecision());
	// Deserialization should occur in the reverse order.
	// Otherwise, value() will return the least recently inserted
	// value instead of the most recently inserted one.
	int count = 0;
	auto it = c.constEnd();
	auto begin = c.constBegin();
	while (it != begin) {
		--it;
		if (it.value().userType() < QVariant::UserType) {
			str << it.key() << it.value();
			++count;
		}
		else {
			//try to the save the value before
			QByteArray tmp;
			QDataStream tmp_str(&tmp, QIODevice::WriteOnly);
			tmp_str.setByteOrder(s.byteOrder());
			tmp_str.setFloatingPointPrecision(s.floatingPointPrecision());
			if (vipSafeVariantSave(tmp_str, it.value())) {
				str << it.key();
				str.writeRawData(tmp.data(), tmp.size());
				++count;
			}
		}
	}

	s << quint32(count);
	s.writeRawData(ar.data(), ar.size());
	return count;
}

VipArchive & operator<<(VipArchive & arch, const VipShape & value)
{
	arch.content("id",value.id());
	arch.content("group",value.group());
	arch.content("type",(int)value.type());
	arch.content("attributes",value.attributes());
	if(value.type() == VipShape::Path)
		arch.content("path",value.shape());
	else if(value.type() == VipShape::Polygon)
		arch.content("polygon",VipPointVector(value.polygon()));
	else if(value.type() == VipShape::Polyline)
		arch.content("polyline",VipPointVector(value.polyline()));
	else if(value.type() == VipShape::Point)
		arch.content("point",VipPoint(value.point()));
	arch.content("isPolygonBased", value.isPolygonBased());
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipShape & value)
{
	value.setId(arch.read("id").toInt());
	value.setGroup(arch.read("group").toString());
	int type = arch.read("type").toInt();

	arch.save();
	bool isPolygonBased = arch.read("isPolygonBased").toBool();
	arch.restore();

	value.setAttributes(arch.read("attributes").value<QVariantMap>());
	if(type == VipShape::Path)
		value.setShape(arch.read("path").value<QPainterPath>(), VipShape::Path, isPolygonBased);
	else if(type == VipShape::Polygon)
		value.setPolygon((arch.read("polygon").value<VipPointVector>().toPointF()));
	else if(type == VipShape::Polyline)
		value.setPolyline((arch.read("polyline").value<VipPointVector>().toPointF()));
	else if(type == VipShape::Point)
		value.setPoint(arch.read("point").value<VipPoint>());
	return arch;
}

VipArchive & operator<<(VipArchive & arch, const VipSceneModel & value)
{
	arch.content("scene_id",VipUniqueId::id(value.shapeSignals()));
	//new in 2.2.17
	arch.content("attributes", value.attributes());
	//
	QStringList groups = value.groups();
	for(int g=0; g < groups.size(); ++g)
	{
		QList<VipShape> shapes = value.shapes(groups[g]);
		for(int i=0; i < shapes.size(); ++i)
			arch.content(shapes[i]);
	}
	return arch;
}

VipArchive & operator>>(VipArchive & arch, VipSceneModel & value)
{
	int id = arch.read("scene_id").value<int>();
	if (arch.hasError())
		return arch;
	VipUniqueId::setId(value.shapeSignals(),id);

	//new in 2.2.17
	arch.save();
	QVariantMap attrs;
	if (arch.content("attributes", attrs)) {
		value.setAttributes(attrs);
	}
	else
		arch.restore();
	//

	while(arch)
	{
		VipShape sh =arch.read().value<VipShape>();
		if(arch)
			value.add(sh.group(),sh);
	}
	arch.resetError();
	return arch;
}


VipArchive & operator<<(VipArchive & arch, const VipSceneModelList & value)
{
	arch.content("count", value.size());
	for (int i = 0; i < value.size(); ++i)
		arch.content(value[i]);
	return arch;
}
VipArchive & operator>>(VipArchive & arch, VipSceneModelList & value)
{
	value.clear();
	int count = arch.read("count").toInt();
	for (int i = 0; i < count; ++i) {
		VipSceneModel sm = arch.read().value<VipSceneModel>();
		if (arch)
			value.push_back(sm);
		else
			break;
	}
	return arch;
}

#include "VipXmlArchive.h"
#include <QGuiApplication>
#include <QClipboard>

void vipCopyObjectsToClipboard(const QVariantList & lst)
{
	if (lst.size())
	{
		VipXOStringArchive arch;
		arch.start("Clipboard");
		for (int i = 0; i < lst.size(); ++i)
			arch.content(lst[i]);
		arch.end();

		QClipboard *clipboard = QGuiApplication::clipboard();
		clipboard->setText(arch.toString());
	}
}

QVariantList vipCreateFromClipboard()
{
	QClipboard *clipboard = QGuiApplication::clipboard();
	QString text = clipboard->text();

	VipXIStringArchive arch(text);
	if(arch.start("Clipboard"))
	{
		QVariantList res;
		while(arch)
		{
			QVariant tmp = arch.read();
			if(arch)
				res.append(tmp);
			else
				break;
		}
		return res;
	}

return QVariantList();
}



QDataStream& operator<<(QDataStream& stream, const VipErrorData& data)
{
	return stream << data.errorString() << data.errorCode();
}

QDataStream& operator>>(QDataStream& stream, VipErrorData& data)
{
	QString error;
	int code;
	stream >> error >> code;
	data = VipErrorData(error, code);
	return stream;
}

//register functions

static int registerConversionFunctions()
{
	qRegisterMetaType<QObjectPointer>("QObjectPointer");
	qRegisterMetaType<VipErrorData>("VipErrorData");
	qRegisterMetaTypeStreamOperators<VipErrorData>("VipErrorData");

	qRegisterMetaType <VipFunctionObject>("VipFunctionObject");

	//register serialization functions for VipShape and VipSceneModel
	vipRegisterArchiveStreamOperators<VipShape>();
	vipRegisterArchiveStreamOperators<VipSceneModel>();
	vipRegisterArchiveStreamOperators<VipSceneModelList>();
	return 0;
}

static int _registerConversionFunctions = registerConversionFunctions();



#include <QMutex>
#include <QThread>
#include <QCoreApplication>

static QObject* object()
{
	static QObject inst;
	if(inst.thread() != QCoreApplication::instance()->thread())
	{
		inst.moveToThread(QCoreApplication::instance()->thread());
	}
	return &inst;
}
// bool vipProcessEvents(bool * keep_going, int milli )
// {
// struct Event : QEvent
// {
// QSharedPointer<bool> alive;
// Event(QSharedPointer<bool> v):QEvent(QEvent::MaxUser), alive(v) {*alive = true;}
// ~Event() {*alive = false;}
// };
//
// QThread * current = QThread::currentThread();
// QCoreApplication * inst = QCoreApplication::instance();
//
// if (!inst)
// return false;
//
// //mutex used to avoid recursive call (we MUST avoid them)
// static QMutex mutex(QMutex::Recursive);
// static QList<QThread*> threads;
// {
// QMutexLocker lock(&mutex);
//
// //recursive call: return
// if (threads.indexOf(current) >= 0 && (current != inst->thread() || milli < 0))
//	return false;
//
// threads.append(current);
// }
//
//
// //keep the GUI responsive: wait for pending events to be processed
// QSharedPointer<bool> alive(new bool());
// QCoreApplication::instance()->postEvent(object(),new Event(alive));
//
// bool result = true;
// qint64 start = QDateTime::currentMSecsSinceEpoch();
// if(current == QCoreApplication::instance()->thread())
// {
// while (*alive &&(!keep_going || *keep_going == true) )
// {
//	//use QCoreApplication::processEvents when called from the main thread
//	QCoreApplication::processEvents(QEventLoop::AllEvents,milli > 0 ? milli : 5);
//	if(milli > 0)
//	{
//		//check for maximum time
//		qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;
//		if(elapsed >= milli)
//		{
//			QCoreApplication::removePostedEvents(object(),QEvent::MaxUser);
//			result = false;
//			break;
//		}
//	}
// }
// }
// else
// {
// while(*alive && (!keep_going || *keep_going == true) )
// {
//	if(milli > 0)
//	{
//		//check for maximum time
//		qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;
//		if(elapsed >= milli)
//		{
//			QCoreApplication::removePostedEvents(object(),QEvent::MaxUser);
//			result = false;
//			break;
//		}
//	}
//
//	QThread::msleep(1);
// }
// }
//
// QMutexLocker lock(&mutex);
// threads.removeOne(current);
//
// return result;
// }


int vipProcessEvents(bool * keep_going, int milli)
{
	struct Event : QEvent
	{
		QSharedPointer<bool> alive;
		Event(QSharedPointer<bool> v) :QEvent(QEvent::MaxUser), alive(v) { *alive = true; }
		~Event() { *alive = false; }
	};

	static QMutex mutex(QMutex::Recursive);
	static QThread *thread_processing = NULL;
	static bool processing_result = false;
	static bool main_thread_processing = false;

	QThread * current_thread = QThread::currentThread();
	QCoreApplication * application = QCoreApplication::instance();
	if (!application)
		return -1;

	//we are in the main thread and this function is already applying from the main thread: recursive call, return
	if (current_thread == application->thread() && main_thread_processing)
		return -3;

	if (current_thread == application->thread())
	{
		{
			QMutexLocker lock(&mutex);
			thread_processing = current_thread;
			processing_result = false;
			main_thread_processing = true;
		}

		int sleep_time = milli > 0 ? milli : 5;
		qint64 start = QDateTime::currentMSecsSinceEpoch();
		QSharedPointer<bool> alive(new bool());
		QCoreApplication::instance()->postEvent(object(), new Event(alive));

		int res = 0;
		while (*alive && (!keep_going || *keep_going == true))
		{
			//use QCoreApplication::processEvents when called from the main thread
			QCoreApplication::processEvents(QEventLoop::AllEvents, sleep_time);

			qint64 cur_time = QDateTime::currentMSecsSinceEpoch();
			qint64 full_elapsed = cur_time - start;
			if (milli > 0 && full_elapsed >= milli)
			{
				//timeout
				processing_result = false;
				res = -2;
				break;
			}
		}
		{
			QMutexLocker lock(&mutex);
			thread_processing = NULL;
			main_thread_processing = false;
		}
		if (keep_going && *keep_going == false)
			res = -1;
		return res;
	}
	else
	{
		//if there is already a thread calling this function, wait for it to finish
		if (thread_processing)
		{
			qint64 start = QDateTime::currentMSecsSinceEpoch();
			while (!processing_result && thread_processing)
			{
				//check for maximum time
				if (milli > 0)
					if ((QDateTime::currentMSecsSinceEpoch() - start) >= milli)
						return -2;
				if (keep_going && *keep_going == false)
					return -1;
				QThread::msleep(1);
			}
			if (processing_result)
				return 0;
		}

		//now wait for the event loop to process a custom event
		{
			QMutexLocker lock(&mutex);
			thread_processing = current_thread;
			processing_result = false;
		}

		QSharedPointer<bool> alive(new bool());
		QCoreApplication::instance()->postEvent(object(), new Event(alive));
		qint64 start = QDateTime::currentMSecsSinceEpoch();
		int res = 0;

		while (*alive && (!keep_going || *keep_going == true))
		{
			if (milli > 0)
			{
				//check for maximum time
				qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;
				if (elapsed >= milli)
				{
					res = -2;
					break;
				}
			}

			QThread::msleep(1);
		}
		{
			QMutexLocker lock(&mutex);
			processing_result = !(*alive);
			thread_processing = NULL;
		}
		if (keep_going && *keep_going == false)
			res = -1;
		return res;
	}

	return -3;
}


static QList<VipFunction<0>>& init_functions()
{
	static QList<VipFunction<0>> instance;
	return instance;
}

static QList<VipFunction<0>>& uninit_functions()
{
	static QList<VipFunction<0>> instance;
	return instance;
}

bool vipAddInitializationFunction(const VipFunction<0>& fun)
{
	init_functions().append(fun);
	return true;
}

bool vipAddInitializationFunction(void(*fun)())
{
	VipFunction<0> f(fun);
	init_functions().append(f);
	return true;
}

bool vipAddInitializationFunction(int(*fun)())
{
	VipFunction<0> f(fun);
	init_functions().append(f);
	return true;
}



bool vipPrependInitializationFunction(const VipFunction<0>& fun)
{
	init_functions().insert(0,fun);
	return true;
}

bool vipPrependInitializationFunction(void (*fun)())
{
	VipFunction<0> f(fun);
	init_functions().insert(0, f);
	return true;
}

bool vipPrependInitializationFunction(int (*fun)())
{
	VipFunction<0> f(fun);
	init_functions().insert(0, f);
	return true;
}


bool vipAddUninitializationFunction(const VipFunction<0>& fun)
{
	uninit_functions().append(fun);
	return true;
}

bool vipAddUninitializationFunction(void(*fun)())
{
	VipFunction<0> f(fun);
	uninit_functions().append(f);
	return true;
}

bool vipAddUninitializationFunction(int(*fun)())
{
	VipFunction<0> f(fun);
	uninit_functions().append(f);
	return true;
}

void vipExecInitializationFunction()
{
	for (int i = 0; i < init_functions().size(); ++i)
		init_functions()[i]();
}

void vipExecUnitializationFunction()
{
	for (int i = 0; i < uninit_functions().size(); ++i)
		uninit_functions()[i]();
}






struct TerminationFunction
{
	QList<VipFunction<0>> functions;
	TerminationFunction() {}
	~TerminationFunction() {
		for (int i = 0; i < functions.size(); ++i)
			functions[i]();
	}
};
static TerminationFunction _termination;

bool vipAddTerminationFunction(const VipFunction<0>& fun)
{
	VipFunction<0> f(fun);
	_termination.functions.append(f);
	return true;
}
bool vipAddTerminationFunction(void(*fun)())
{
	VipFunction<0> f(fun);
	_termination.functions.append(f);
	return true;
}




static QMap<QString, VipFunctionObject> _functions;

bool VipFunctionObject::isValid() const
{
	return (bool)function;
}

bool vipRegisterFunction(const VipFunctionObject &fun)
{
	if (fun.isValid()) {
		_functions[fun.name] = fun;
		return true;
	}
	return false;
}
bool vipRegisterFunction(const VipFunctionObject::function_type& fun, const QString& name, const QString& description, bool mainThread)
{
	if (fun) {
		VipFunctionObject f;
		f.function = fun;
		f.name = name;
		f.description = description;
		f.mainThread = mainThread;
		_functions[name] = f;
		return true;
	}
	return false;
}

VipFunctionObject vipFindFunction(const QString & name)
{
	QMap<QString, VipFunctionObject>::const_iterator it = _functions.find(name);
	if (it != _functions.end())
		return it.value();
	return VipFunctionObject();
}

QList<VipFunctionObject> vipAllFunctions()
{
	return _functions.values();
}






typedef void(*archive_fun)(VipArchive &);
typedef QList<QPair<archive_fun, archive_fun> > archive_fun_list;
static archive_fun_list _archive_fun_list;

void vipRegisterSettingsArchiveFunctions(void(*save)(VipArchive &), void(*restore)(VipArchive&))
{
	_archive_fun_list.append(QPair<archive_fun, archive_fun>(save, restore));
}

void vipSaveSettings(VipArchive & arch)
{
	for (int i = 0; i < _archive_fun_list.size(); ++i)
	{
		_archive_fun_list[i].first(arch);
	}
}

void vipRestoreSettings(VipArchive & arch)
{
	for (int i = 0; i < _archive_fun_list.size(); ++i)
	{
		_archive_fun_list[i].second(arch);
	}
}

void vipSaveCustomProperties(VipArchive & arch, const QObject * obj)
{
	//save all properties starting with "_vip_custom"
	arch.start("custom_properties");
	QList<QByteArray> properties = obj->dynamicPropertyNames();
	for (int i = 0; i < properties.size(); ++i) {
		const QByteArray name = properties[i];
		if (name.startsWith("_vip_custom")) {
			arch.content("name", QString(name));
			arch.content("value", obj->property(name.data()));
		}
	}
	arch.end();
}

QList<QByteArray> vipLoadCustomProperties(VipArchive & arch, QObject * obj)
{
	QList<QByteArray> res;

	arch.save();
	if (arch.start("custom_properties")) {
		while (true) {
			QString name;
			QVariant value;
			name = arch.read("name").toString();
			value = arch.read("value");
			if (!arch)
				break;
			obj->setProperty(name.toLatin1().data(), value);
			res << name.toLatin1();
		}

		arch.end();
	}
	else
		arch.restore();
	return res;
}



VipCoreSettings::VipCoreSettings()
	:m_log_overwrite(false), m_log_date(false)
{}

VipCoreSettings * VipCoreSettings::instance()
{
	static VipCoreSettings inst;
	return &inst;
}

void VipCoreSettings::setLogFileOverwrite(bool enable)
{
	m_log_overwrite = enable;
}
bool VipCoreSettings::logFileOverwrite() const
{
	return m_log_overwrite;
}

void VipCoreSettings::setLogFileDate(bool enable)
{
	m_log_date = enable;
}
bool VipCoreSettings::logFileDate() const
{
	return m_log_date;
}

void  VipCoreSettings::setSkin(const QString & skin)
{
	m_skin = skin;
}
QString  VipCoreSettings::skin() const
{
	return m_skin;
}

bool  VipCoreSettings::save(VipArchive & ar)
{
	if (ar.start("VipCoreSettings"))
	{
		ar.content("logFileOverwrite", VipCoreSettings::instance()->logFileOverwrite());
		ar.content("logFileDate", VipCoreSettings::instance()->logFileDate());
		ar.content("skin", VipCoreSettings::instance()->skin());
		ar.end();
		return ar;
	}
	return false;
}
bool  VipCoreSettings::save(const QString & file)
{
	VipXOfArchive ar;
	if (!ar.open(file))
		return false;
	return save(ar);
}

bool  VipCoreSettings::restore(VipArchive & ar)
{
	if (ar.start("VipCoreSettings"))
	{
		VipCoreSettings::instance()->setLogFileOverwrite(ar.read("logFileOverwrite").value<bool>());
		VipCoreSettings::instance()->setLogFileDate(ar.read("logFileDate").value<bool>());
		VipCoreSettings::instance()->setSkin(ar.read("skin").toString());
		ar.end();
		return ar;
	}
	return false;
}
bool  VipCoreSettings::restore(const QString & file)
{
	VipXIfArchive ar;
	if (!ar.open(file))
		return false;
	return restore(ar);
}





static QMap<int, int(*)(int, const QVariant &)> _mem_functions;

int vipGetMemoryFootprint(const QVariant & v)
{
	int type = v.userType();

	if (type == 0)
		return 0;

	//standard types
	switch (type)
	{
	case QMetaType::Bool:
	case QMetaType::UChar:
	case QMetaType::Char:
	case QMetaType::SChar: return 1;
	case QMetaType::UShort:
	case QMetaType::Short: return 2;
	case QMetaType::UInt:
	case QMetaType::Int:
	case QMetaType::Float: return 4;
	case QMetaType::ULongLong:
	case QMetaType::LongLong:
	case QMetaType::Double: return 8;
	case QMetaType::QChar: return sizeof(QChar);
	case QMetaType::QString: return sizeof(QChar)*v.toString().size();
	case QMetaType::QByteArray: return v.toByteArray().size();
	case QMetaType::Long: return sizeof(long);
	case QMetaType::ULong: return sizeof(unsigned long);
	case QMetaType::QDate: return sizeof(QDate);
	case QMetaType::QSize: return sizeof(QSize);
	case QMetaType::QSizeF: return sizeof(QSizeF);
	case QMetaType::QTime: return sizeof(QTime);
	case QMetaType::QPolygon: return v.value<QPolygon>().size()*sizeof(QPoint);
	case QMetaType::QPolygonF: return v.value<QPolygonF>().size() * sizeof(QPointF);
	case QMetaType::QPoint:	return sizeof(QPoint);
	case QMetaType::QPointF:	return sizeof(QPointF);
	case QMetaType::QRect:	return sizeof(QRect);
	case QMetaType::QRectF:	return sizeof(QRectF);
	case QMetaType::QColor:	return sizeof(QColor);
	case QMetaType::QVariantMap:
	{
		int size = 0;
		const QVariantMap map = v.value<QVariantMap>();
		for (QVariantMap::const_iterator it = map.begin(); it != map.end(); ++it)
			size += vipGetMemoryFootprint(it.value()) + it.key().size() * sizeof(QChar);
		return size;
	}
	case QMetaType::QVariantList:
	{
		int size = 0;
		const QVariantList map = v.value<QVariantList>();
		for (QVariantList::const_iterator it = map.begin(); it != map.end(); ++it)
			size += vipGetMemoryFootprint(*it);
		return size;
	}
	default:
		break;
	}

	//custom types defined by thermavip
	if (type == qMetaTypeId<VipNDArray>())
	{
		const VipNDArray ar = v.value<VipNDArray>();
		if (vipIsImageArray(ar))
			return ar.size() * 4;
		return ar.size()*ar.dataSize();
	}
	//else if (type == qMetaTypeId<VipRasterData>())
	// {
	// const VipNDArray ar = v.value<VipNDArray>();
	// if (vipIsImageArray(ar))
	// return ar.size() * 4;
	// return ar.size()*ar.dataSize();
	// }
	else if (type == qMetaTypeId<complex_f>())
		return sizeof(complex_f);
	else if (type == qMetaTypeId<complex_d>())
		return sizeof(complex_d);
	else if (type == qMetaTypeId<VipInterval>())
		return sizeof(VipInterval);
	else if (type == qMetaTypeId<VipIntervalSample>())
		return sizeof(VipIntervalSample);
	else if (type == qMetaTypeId<VipPointVector>())
		return v.value<VipPointVector>().size() * sizeof(VipPoint);
	else if (type == qMetaTypeId<VipIntervalSampleVector>())
		return v.value<VipIntervalSampleVector>().size() * sizeof(VipIntervalSample);

	QMap<int, int(*)(int, const QVariant &)>::iterator it = _mem_functions.find(type);
	if (it != _mem_functions.end())
		return it.value()(type, v);

	return 0;
}

int vipRegisterMemoryFootprintFunction(int metatype_id, int(*fun)(int, const QVariant &))
{
	_mem_functions[metatype_id] = fun;
	return metatype_id;
}



static qint64 defaultNanoSecondsSinceEpoch()
{
	return QDateTime::currentMSecsSinceEpoch() * 1000000;
}

typedef qint64(*time_function)();
static time_function _time_function = defaultNanoSecondsSinceEpoch;
static time_function _ms_time_function = QDateTime::currentMSecsSinceEpoch;

void vipSetTimeFunction(qint64(*fun)())
{
	_time_function = fun;
}
void vipSetMsTimeFunction(qint64(*fun)())
{
	_ms_time_function = fun;
}

qint64 vipGetNanoSecondsSinceEpoch()
{
	return _time_function();
}

qint64 vipGetMilliSecondsSinceEpoch()
{
	return _ms_time_function ? _ms_time_function() : _time_function() / 1000000;
}




static QStringList _icon_paths = QStringList()<<"icons/";

QPixmap vipPixmap(const QString & name)
{
	for (int i = 0; i < _icon_paths.size(); ++i)
	{
		QPixmap icon = QPixmap(_icon_paths[i] + name);
		if (!icon.isNull())
			return icon;
	}

	return QPixmap();
}

QIcon vipIcon(const QString & name)
{
	QPixmap pix = vipPixmap(name);
	if (pix.isNull())
		return QIcon();
	else
		return QIcon(pix);
}

void vipAddIconPath(const QString & path)
{
	QString p = path;
	p.replace("\\", "/");
	if (!p.endsWith("/"))
		p += "/";

	_icon_paths.append(p);
}

void vipAddFrontIconPath(const QString & path)
{
	QString p = path;
	p.replace("\\", "/");
	if (!p.endsWith("/"))
		p += "/";

	_icon_paths.push_front(p);
}

void vipSetIconPaths(const QStringList & paths)
{
	_icon_paths = paths;
	for (int i = 0; i < _icon_paths.size(); ++i)
	{
		QString p = _icon_paths[i];
		p.replace("\\", "/");
		if (!p.endsWith("/"))
			p += "/";
		_icon_paths[i] = p;
	}
}


qint64 vipBuildTime()
{
	static qint64 time = 0;
	if (time == 0)
	{
		QString date = (__DATE__ + QString(" ") + QString(__TIME__));
		date.replace(":", " ");
		QStringList lst = date.split(" ", QString::SkipEmptyParts);
		date = lst.join(" ");
		if (lst.size() == 6)
		{
			QString pattern;
			pattern += QString("M").repeated(lst[0].size()) + " ";
			pattern += QString("d").repeated(lst[1].size()) + " ";
			pattern += QString("y").repeated(lst[2].size()) + " ";
			pattern += QString("h").repeated(lst[3].size()) + " ";
			pattern += QString("m").repeated(lst[4].size()) +" ";
			pattern += QString("s").repeated(lst[5].size());

			QLocale locale(QLocale::English, QLocale::UnitedStates);
			time = locale.toDateTime(date, pattern).toMSecsSinceEpoch();
		}

	}
	return time;
}


static QString _editionVersion;
void vipSetEditionVersion(const QString & ver)
{
	_editionVersion = ver;
}
QString vipEditionVersion()
{
	return _editionVersion;
}


static QString _app_name;
void vipSetAppCanonicalPath(const QString & name)
{
	_app_name = name;
}
QString vipAppCanonicalPath()
{
	return _app_name;
}

QString vipUserName()
{
	QString homePath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first();
	homePath.replace("\\", "/");
	QString username = homePath.split("/").last();
	return username;
}