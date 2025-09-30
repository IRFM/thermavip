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

#include "VipCore.h"
#include "VipArchive.h"
#include "VipDataType.h"
#include "VipSceneModel.h"
#include "VipUniqueId.h"
#include "VipHash.h"
#include "VipStandardProcessing.h"

#include <QMetaType>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

#include <unordered_map>
#include <thread>

// Fake QIODevice class
class FakeIODevice : public QIODevice
{
public:
	FakeIODevice(): QIODevice(){}
	
	virtual bool 	open(QIODevice::OpenMode mode)
	{
		setOpenMode(mode);
		return true;
	}	
	virtual qint64 	readData(char *data, qint64 maxSize){return maxSize;} 
	virtual qint64 	writeData(const char *data, qint64 maxSize){return maxSize;} 
};


static bool canSaveVariant(const QVariant& v) 
{
	// Check if variant can be serialized

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
	// For Qt >= 6.1, we can use QMetaType::hasRegisteredDataStreamOperators()
	QMetaType meta(v.userType());
	if (v.userType() && (v.userType() < QMetaType::User || meta.hasRegisteredDataStreamOperators())) {
		return true;
	}
	return false;
#else

	// For older Qt, no choice but to serialize the variant
	// into a QDataStream writing into a fake QIODevice.
	if(v.userType() == 0)
		return false;

	if (v.userType() < QMetaType::User)
		return true;
	
	FakeIODevice d;
	d.open(QIODevice::WriteOnly);
	QDataStream s(&d);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	bool res = QMetaType::save(s, v.userType(), v.constData()); 
#else
	bool res = QMetaType(v.userType()).save(s, v.constData());
#endif
	return res;
#endif
}

qsizetype vipSafeVariantMapSave(QDataStream& s, const QVariantMap& c)
{
	QVariantMap tmp;
	for (auto it = c.begin(); it != c.end(); ++it) {
		if (canSaveVariant(it.value()))
			tmp.insert(it.key(), it.value());
	}
	s << tmp;
	return tmp.size();
}

VipArchive& operator<<(VipArchive& arch, const VipShape& value)
{
	arch.content("id", value.id());
	arch.content("group", value.group());
	arch.content("type", (int)value.type());
	arch.content("attributes", value.attributes());
	if (value.type() == VipShape::Path)
		arch.content("path", value.shape());
	else if (value.type() == VipShape::Polygon)
		arch.content("polygon", vipToPointVector(value.polygon()));
	else if (value.type() == VipShape::Polyline)
		arch.content("polyline", vipToPointVector(value.polyline()));
	else if (value.type() == VipShape::Point)
		arch.content("point", VipPoint(value.point()));
	arch.content("isPolygonBased", value.isPolygonBased());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipShape& value)
{
	value.setId(arch.read("id").toInt());
	value.setGroup(arch.read("group").toString());
	int type = arch.read("type").toInt();

	arch.save();
	bool isPolygonBased = arch.read("isPolygonBased").toBool();
	arch.restore();

	value.setAttributes(arch.read("attributes").value<QVariantMap>());
	if (type == VipShape::Path)
		value.setShape(arch.read("path").value<QPainterPath>(), VipShape::Path, isPolygonBased);
	else if (type == VipShape::Polygon)
		value.setPolygon(vipToPointF(arch.read("polygon").value<VipPointVector>()));
	else if (type == VipShape::Polyline)
		value.setPolyline(vipToPointF(arch.read("polyline").value<VipPointVector>()));
	else if (type == VipShape::Point)
		value.setPoint(arch.read("point").value<VipPoint>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipSceneModel& value)
{
	arch.content("scene_id", VipUniqueId::id(value.shapeSignals()));
	// new in 2.2.17
	arch.content("attributes", value.attributes());
	//
	QStringList groups = value.groups();
	for (qsizetype g = 0; g < groups.size(); ++g) {
		VipShapeList shapes = value.shapes(groups[g]);
		for (qsizetype i = 0; i < shapes.size(); ++i)
			arch.content(shapes[i]);
	}
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipSceneModel& value)
{
	int id = arch.read("scene_id").value<int>();
	if (arch.hasError())
		return arch;
	VipUniqueId::setId(value.shapeSignals(), id);

	// new in 2.2.17
	arch.save();
	QVariantMap attrs;
	if (arch.content("attributes", attrs)) {
		value.setAttributes(attrs);
	}
	else
		arch.restore();
	//

	while (arch) {
		VipShape sh = arch.read().value<VipShape>();
		if (arch)
			value.add(sh.group(), sh);
	}
	arch.resetError();
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipSceneModelList& value)
{
	arch.content("count", value.size());
	for (qsizetype i = 0; i < value.size(); ++i)
		arch.content(value[i]);
	return arch;
}
VipArchive& operator>>(VipArchive& arch, VipSceneModelList& value)
{
	value.clear();
	qsizetype count = arch.read("count").toInt();
	for (qsizetype i = 0; i < count; ++i) {
		VipSceneModel sm = arch.read().value<VipSceneModel>();
		if (arch)
			value.push_back(sm);
		else
			break;
	}
	return arch;
}

static QString polygonToJSON(const QPolygonF& poly)
{
	QString res;
	QTextStream str(&res, QIODevice::WriteOnly);
	for (qsizetype i = 0; i < poly.size(); ++i)
		str << poly[i].x() << " " << poly[i].y() << " ";
	str.flush();
	return res;
}
static QPolygonF polygonFromJSON(const QByteArray& ar)
{
	QTextStream str(ar);
	QPolygonF res;
	for (;;) {
		double x, y;
		str >> x >> y;
		if (str.status() != QTextStream::Ok)
			break;
		res.push_back(QPointF(x, y));
	}
	return res;
}

void vipShapeToJSON(QTextStream& str, const VipShape& value, const QByteArray& indent)
{
	str << indent << "{" << Qt::endl;
	str << indent << "\"id\": " << value.id() << "," << Qt::endl;
	str << indent << "\"group\": "
	    << "\"" << value.group() << "\""
	    << "," << Qt::endl;
	str << indent << "\"type\": " << (int)value.type() << "," << Qt::endl;

	QPolygonF p;
	if (value.type() == VipShape::Path)
		p = value.polygon();
	else if (value.type() == VipShape::Polygon)
		p = value.polygon();
	else if (value.type() == VipShape::Polyline)
		p = value.polyline();
	else if (value.type() == VipShape::Point)
		p << value.point();

	str << indent << "\"points\": "
	    << "\"" << polygonToJSON(p) << "\""
	    << "," << Qt::endl;
	str << indent << "\"attributes\": " << Qt::endl;
	str << indent << "\t{" << Qt::endl;

	bool has_val = false;
	const QVariantMap map = value.attributes();
	for (auto it = map.begin(); it != map.end(); ++it) {
		QString key = it.key();
		if (key.startsWith("_vip_"))
			continue;
		QVariant val = it.value();
		if (vipIsArithmetic(val.userType())) {
			if (has_val)
				str << "," << Qt::endl;
			has_val = true;
			str << indent << "\t"
			    << "\"" << key << "\": " << val.toDouble();
		}
		else if (val.canConvert<QString>()) {
			if (has_val)
				str << "," << Qt::endl;
			has_val = true;
			str << indent << "\t"
			    << "\"" << key << "\": "
			    << "\"" << val.toString() << "\"";
		}
	}
	if (has_val)
		str << Qt::endl;

	str << indent << "\t}" << Qt::endl;
	str << indent << "}" << Qt::endl;
}

void vipSceneModelToJSON(QTextStream& str, const VipSceneModel& value, const QByteArray& indent)
{
	str << indent << "{" << Qt::endl;

	QStringList groups = value.groups();
	for (qsizetype i = 0; i < groups.size(); ++i) {
		str << indent << "\"" << groups[i] << "\": " << Qt::endl;
		str << indent << "[" << Qt::endl;

		QByteArray indent2 = indent + '\t';
		const VipShapeList shapes = value.shapes(groups[i]);
		for (qsizetype j = 0; j < shapes.size(); ++j) {
			vipShapeToJSON(str, shapes[j], indent2);
			if (j != shapes.size() - 1)
				str << indent2 << "," << Qt::endl;
			else
				str << indent2 << Qt::endl;
		}

		str << indent << "]," << Qt::endl;
	}

	str << indent << "\"attributes\": " << Qt::endl;
	str << indent << "\t{" << Qt::endl;

	bool has_val = false;
	const QVariantMap map = value.attributes();
	for (auto it = map.begin(); it != map.end(); ++it) {
		QString key = it.key();
		if (key.startsWith("_vip_"))
			continue;
		QVariant val = it.value();
		if (vipIsArithmetic(val.userType())) {
			if (has_val)
				str << "," << Qt::endl;
			has_val = true;
			str << indent << "\t"
			    << "\"" << key << "\": " << val.toDouble();
		}
		else if (val.canConvert<QString>()) {
			if (has_val)
				str << "," << Qt::endl;
			has_val = true;
			str << indent << "\t"
			    << "\"" << key << "\": "
			    << "\"" << val.toString() << "\"";
		}
	}
	if (has_val)
		str << Qt::endl;

	str << indent << "\t}" << Qt::endl;
	str << indent << "}" << Qt::endl;
}

void vipSceneModelListToJSON(QTextStream& str, const VipSceneModelList& value, const QByteArray& indent)
{

	str << indent << "{" << Qt::endl;

	QByteArray indent2 = indent + '\t';

	qsizetype count = value.size();
	for (qsizetype i = 0; i < count; ++i) {
		str << indent2 << "\""
		    << "SceneModel" << QString::number(i) << "\": " << Qt::endl;
		vipSceneModelToJSON(str, value[i], indent2);
		if (i != count - 1)
			str << indent2 << "," << Qt::endl;
	}
	str << indent << "}" << Qt::endl;
}

#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>

static QVariantMap attributesFromJson(const QJsonObject& obj)
{
	QVariantMap res;
	for (auto it = obj.begin(); it != obj.end(); ++it) {
		res[it.key()] = it.value().toVariant();
	}
	return res;
}

static VipShape shapeFromJson(const QJsonObject& obj, QString& error)
{
	VipShape res;
	res.setGroup(obj.value("group").toString());
	if (!res.setId(obj.value("id").toInt())) {
		error = "invalid shape id";
		return res;
	}
	res.setAttributes(attributesFromJson(obj.value("attributes").toObject()));
	int type = obj.value("type").toInt();
	if (type == 0) {
		error = "invalid shape type";
		return res;
	}
	QString p = obj.value("points").toString();
	QPolygonF poly = polygonFromJSON(p.toLatin1());

	if (type == VipShape::Polygon || type == VipShape::Path)
		res.setPolygon(poly);
	else if (type == VipShape::Point && poly.size())
		res.setPoint(poly.first());
	else if (type == VipShape::Polyline)
		res.setPolyline(poly);

	return res;
}

static VipSceneModel sceneModelFromJson(const QJsonObject& obj, QString& error)
{
	VipSceneModel res;
	for (auto it = obj.begin(); it != obj.end(); ++it) {
		QString key = it.key();
		if (key != "attributes") {
			QJsonArray ar = it.value().toArray();
			for (qsizetype i = 0; i < ar.size(); ++i) {

				res.add(key, shapeFromJson(ar[i].toObject(), error));
				if (!error.isEmpty())
					return res;
			}
		}
		else
			res.setAttributes(attributesFromJson(it.value().toObject()));
	}
	return res;
}

VipSceneModelList vipSceneModelListFromJSON(const QByteArray& content, QString* error_str)
{
	QJsonParseError error;
	QJsonDocument loadDoc(QJsonDocument::fromJson(content, &error));
	if (error.error != QJsonParseError::NoError) {
		if (error_str)
			*error_str = error.errorString();
		return VipSceneModelList();
	}

	QJsonObject root = loadDoc.object();

	VipSceneModelList res;
	QString err;
	for (auto it = root.begin(); it != root.end(); ++it) {

		res << sceneModelFromJson(it.value().toObject(), err);
		if (!err.isEmpty()) {
			if (error_str)
				*error_str = err;
			return VipSceneModelList();
		}
	}
	return res;
}

#include "VipXmlArchive.h"
#include <QClipboard>
#include <QGuiApplication>

void vipCopyObjectsToClipboard(const QVariantList& lst)
{
	if (lst.size()) {
		VipXOStringArchive arch;
		arch.start("Clipboard");
		for (qsizetype i = 0; i < lst.size(); ++i)
			arch.content(lst[i]);
		arch.end();

		QClipboard* clipboard = QGuiApplication::clipboard();
		clipboard->setText(arch.toString());
	}
}

QVariantList vipCreateFromClipboard()
{
	QClipboard* clipboard = QGuiApplication::clipboard();
	QString text = clipboard->text();

	VipXIStringArchive arch(text);
	if (arch.start("Clipboard")) {
		QVariantList res;
		while (arch) {
			QVariant tmp = arch.read();
			if (arch)
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

// register functions

static int registerConversionFunctions()
{
	qRegisterMetaType<QObjectPointer>("QObjectPointer");
	qRegisterMetaType<VipErrorData>("VipErrorData");
	qRegisterMetaTypeStreamOperators<VipErrorData>("VipErrorData");

	qRegisterMetaType<VipFunctionObject>("VipFunctionObject");

	// register serialization functions for VipShape and VipSceneModel
	vipRegisterArchiveStreamOperators<VipShape>();
	vipRegisterArchiveStreamOperators<VipSceneModel>();
	vipRegisterArchiveStreamOperators<VipSceneModelList>();
	return 0;
}

static int _registerConversionFunctions = registerConversionFunctions();

#include <QCoreApplication>
#include <QMutex>
#include <QThread>

static QObject* object()
{
	static QObject inst;
	if (inst.thread() != QCoreApplication::instance()->thread()) {
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

int vipProcessEvents(bool* keep_going, int milli)
{
	struct Event : QEvent
	{
		QSharedPointer<bool> alive;
		Event(QSharedPointer<bool> v)
		  : QEvent(QEvent::MaxUser)
		  , alive(v)
		{
			*alive = true;
		}
		~Event() { *alive = false; }
	};

	static QRecursiveMutex mutex;
	static QThread* thread_processing = nullptr;
	static bool processing_result = false;
	static bool main_thread_processing = false;

	QThread* current_thread = QThread::currentThread();
	QCoreApplication* application = QCoreApplication::instance();
	if (!application)
		return -1;

	// we are in the main thread and this function is already applying from the main thread: recursive call, return
	if (current_thread == application->thread() && main_thread_processing)
		return -3;

	if (current_thread == application->thread()) {
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
		while (*alive && (!keep_going || *keep_going == true)) {
			// use QCoreApplication::processEvents when called from the main thread
			QCoreApplication::processEvents(QEventLoop::AllEvents, sleep_time);

			qint64 cur_time = QDateTime::currentMSecsSinceEpoch();
			qint64 full_elapsed = cur_time - start;
			if (milli > 0 && full_elapsed >= milli) {
				// timeout
				processing_result = false;
				res = -2;
				break;
			}
		}
		{
			QMutexLocker lock(&mutex);
			thread_processing = nullptr;
			main_thread_processing = false;
		}
		if (keep_going && *keep_going == false)
			res = -1;
		return res;
	}
	else {
		// if there is already a thread calling this function, wait for it to finish
		if (thread_processing) {
			qint64 start = QDateTime::currentMSecsSinceEpoch();
			while (!processing_result && thread_processing) {
				// check for maximum time
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

		// now wait for the event loop to process a custom event
		{
			QMutexLocker lock(&mutex);
			thread_processing = current_thread;
			processing_result = false;
		}

		QSharedPointer<bool> alive(new bool());
		QCoreApplication::instance()->postEvent(object(), new Event(alive));
		qint64 start = QDateTime::currentMSecsSinceEpoch();
		int res = 0;

		while (*alive && (!keep_going || *keep_going == true)) {
			if (milli > 0) {
				// check for maximum time
				qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;
				if (elapsed >= milli) {
					res = -2;
					break;
				}
			}

			QThread::msleep(1);
		}
		{
			QMutexLocker lock(&mutex);
			processing_result = !(*alive);
			thread_processing = nullptr;
		}
		if (keep_going && *keep_going == false)
			res = -1;
		return res;
	}
	VIP_UNREACHABLE();
	// return -3;
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

bool vipAddInitializationFunction(void (*fun)())
{
	VipFunction<0> f(fun);
	init_functions().append(f);
	return true;
}

bool vipAddInitializationFunction(int (*fun)())
{
	VipFunction<0> f(fun);
	init_functions().append(f);
	return true;
}

bool vipPrependInitializationFunction(const VipFunction<0>& fun)
{
	init_functions().insert(0, fun);
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

bool vipAddUninitializationFunction(void (*fun)())
{
	VipFunction<0> f(fun);
	uninit_functions().append(f);
	return true;
}

bool vipAddUninitializationFunction(int (*fun)())
{
	VipFunction<0> f(fun);
	uninit_functions().append(f);
	return true;
}

void vipExecInitializationFunction()
{
	for (qsizetype i = 0; i < init_functions().size(); ++i)
		init_functions()[i]();
}

void vipExecUnitializationFunction()
{
	for (qsizetype i = 0; i < uninit_functions().size(); ++i)
		uninit_functions()[i]();
}

#include "VipSleep.h"

std::atomic<bool>& _enableGuiInitializationFunction()
{
	static std::atomic<bool> inst;
	return inst;
}

void vipEnableGuiInitializationFunction(bool enable)
{
	_enableGuiInitializationFunction() = enable;
}

struct GuiFunctions : public QObject
{
	QMutex mutex;
	std::thread thread;
	QList<VipFunction<0>> functions;
	std::atomic<bool> stop{ false };

	static GuiFunctions& instance()
	{
		static GuiFunctions inst;
		return inst;
	}

	GuiFunctions() { thread = std::thread(std::bind(&GuiFunctions::run, this)); }
	~GuiFunctions()
	{
		stop = true;
		thread.join();
	}

	void run()
	{
		while (!QCoreApplication::instance() && !stop)
			vipSleep(1);
		if (stop)
			return;

		// wait for _enableGuiInitializationFunction to be true
		while (!_enableGuiInitializationFunction().load() && !stop)
			vipSleep(1);
		if (stop)
			return;

		QCoreApplication::instance()->postEvent(&instance(), new QEvent(QEvent::Create));
	}

	void addFunction(const VipFunction<0>& fun)
	{
		QMutexLocker lock(&mutex);
		functions.push_back(fun);
	}

	virtual bool event(QEvent* evt)
	{
		if (evt->type() != QEvent::Create)
			return false;
		QMutexLocker lock(&mutex);
		for (const VipFunction<0>& fun : functions)
			fun();
		return true;
	}
};

bool vipAddGuiInitializationFunction(int (*fun)())
{
	GuiFunctions::instance().addFunction(fun);
	return true;
}
bool vipAddGuiInitializationFunction(void (*fun)())
{
	GuiFunctions::instance().addFunction(fun);
	return true;
}
bool vipAddGuiInitializationFunction(const VipFunction<0>& fun)
{
	GuiFunctions::instance().addFunction(fun);
	return true;
}

Q_COREAPP_STARTUP_FUNCTION(vipExecInitializationFunction);

static int add_post_routine()
{
	qAddPostRoutine(vipExecUnitializationFunction);
	return 0;
}
static int _add_post_routine = add_post_routine();

struct QStringHasher
{
	size_t operator()(const QString& str) const noexcept { return vipHashBytesKomihash(str.data(), str.size() * sizeof(QChar)); }
};
static std::unordered_map<QString, VipFunctionObject, QStringHasher> _functions;

bool VipFunctionObject::isValid() const
{
	return (bool)function;
}

bool vipRegisterFunction(const VipFunctionObject& fun)
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

const VipFunctionObject &vipFindFunction(const QString& name)
{
	static const VipFunctionObject null_fun;
	auto it = _functions.find(name);
	if (it != _functions.end())
		return it->second;
	return null_fun;
}

QList<VipFunctionObject> vipAllFunctions()
{
	QList<VipFunctionObject> res;
	for (const std::pair<const QString, VipFunctionObject>& p : _functions)
		res.push_back(p.second);
	return res;
}

typedef void (*archive_fun)(VipArchive&);
typedef QList<QPair<archive_fun, archive_fun>> archive_fun_list;
static archive_fun_list _archive_fun_list;

bool vipRegisterSettingsArchiveFunctions(void (*save)(VipArchive&), void (*restore)(VipArchive&))
{
	_archive_fun_list.append(QPair<archive_fun, archive_fun>(save, restore));
	return true;
}

void vipSaveSettings(VipArchive& arch)
{
	for (qsizetype i = 0; i < _archive_fun_list.size(); ++i) {
		_archive_fun_list[i].first(arch);
	}
}

void vipRestoreSettings(VipArchive& arch)
{
	for (qsizetype i = 0; i < _archive_fun_list.size(); ++i) {
		_archive_fun_list[i].second(arch);
	}
}

void vipSaveCustomProperties(VipArchive& arch, const QObject* obj)
{
	// save all properties starting with "_vip_custom"
	arch.start("custom_properties");
	QList<QByteArray> properties = obj->dynamicPropertyNames();
	for (qsizetype i = 0; i < properties.size(); ++i) {
		const QByteArray name = properties[i];
		if (name.startsWith("_vip_custom")) {
			arch.content("name", QString(name));
			arch.content("value", obj->property(name.data()));
		}
	}
	arch.end();
}

QList<QByteArray> vipLoadCustomProperties(VipArchive& arch, QObject* obj)
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
  : m_log_overwrite(false)
  , m_log_date(false)
{
}

VipCoreSettings* VipCoreSettings::instance()
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

void VipCoreSettings::setSkin(const QString& skin)
{
	m_skin = skin;
}
QString VipCoreSettings::skin() const
{
	return m_skin;
}

bool VipCoreSettings::save(VipArchive& ar)
{
	if (ar.start("VipCoreSettings")) {
		ar.content("logFileOverwrite", VipCoreSettings::instance()->logFileOverwrite());
		ar.content("logFileDate", VipCoreSettings::instance()->logFileDate());
		ar.content("skin", VipCoreSettings::instance()->skin());
		ar.end();
		return ar;
	}
	return false;
}
bool VipCoreSettings::save(const QString& file)
{
	VipXOfArchive ar;
	if (!ar.open(file))
		return false;
	return save(ar);
}

bool VipCoreSettings::restore(VipArchive& ar)
{
	if (ar.start("VipCoreSettings")) {
		VipCoreSettings::instance()->setLogFileOverwrite(ar.read("logFileOverwrite").value<bool>());
		VipCoreSettings::instance()->setLogFileDate(ar.read("logFileDate").value<bool>());
		VipCoreSettings::instance()->setSkin(ar.read("skin").toString());
		ar.end();
		return ar;
	}
	return false;
}
bool VipCoreSettings::restore(const QString& file)
{
	VipXIfArchive ar;
	if (!ar.open(file))
		return false;
	return restore(ar);
}

static QMap<int, int (*)(int, const QVariant&)> _mem_functions;

int vipGetMemoryFootprint(const QVariant& v)
{
	int type = v.userType();

	if (type == 0)
		return 0;

	// standard types
	switch (type) {
		case QMetaType::Bool:
		case QMetaType::UChar:
		case QMetaType::Char:
		case QMetaType::SChar:
			return 1;
		case QMetaType::UShort:
		case QMetaType::Short:
			return 2;
		case QMetaType::UInt:
		case QMetaType::Int:
		case QMetaType::Float:
			return 4;
		case QMetaType::ULongLong:
		case QMetaType::LongLong:
		case QMetaType::Double:
			return 8;
		case QMetaType::QChar:
			return (int)sizeof(QChar);
		case QMetaType::QString:
			return (int)(sizeof(QChar) * v.toString().size());
		case QMetaType::QByteArray:
			return (int)v.toByteArray().size();
		case QMetaType::Long:
			return (int)sizeof(long);
		case QMetaType::ULong:
			return (int)sizeof(unsigned long);
		case QMetaType::QDate:
			return (int)sizeof(QDate);
		case QMetaType::QSize:
			return (int)sizeof(QSize);
		case QMetaType::QSizeF:
			return (int)sizeof(QSizeF);
		case QMetaType::QTime:
			return (int)sizeof(QTime);
		case QMetaType::QPolygon:
			return (int)(v.value<QPolygon>().size() * sizeof(QPoint));
		case QMetaType::QPolygonF:
			return (int)(v.value<QPolygonF>().size() * sizeof(QPointF));
		case QMetaType::QPoint:
			return (int)sizeof(QPoint);
		case QMetaType::QPointF:
			return (int)sizeof(QPointF);
		case QMetaType::QRect:
			return (int)sizeof(QRect);
		case QMetaType::QRectF:
			return (int)sizeof(QRectF);
		case QMetaType::QColor:
			return (int)sizeof(QColor);
		case QMetaType::QVariantMap: {
			int size = 0;
			const QVariantMap map = v.value<QVariantMap>();
			for (QVariantMap::const_iterator it = map.begin(); it != map.end(); ++it)
				size += (int)(vipGetMemoryFootprint(it.value()) + it.key().size() * sizeof(QChar));
			return size;
		}
		case QMetaType::QVariantList: {
			int size = 0;
			const QVariantList map = v.value<QVariantList>();
			for (QVariantList::const_iterator it = map.begin(); it != map.end(); ++it)
				size += vipGetMemoryFootprint(*it);
			return size;
		}
		default:
			break;
	}

	// custom types defined by thermavip
	if (type == qMetaTypeId<VipNDArray>()) {
		const VipNDArray ar = v.value<VipNDArray>();
		if (vipIsImageArray(ar))
			return (int)(ar.size() * 4);
		return (int)(ar.size() * ar.dataSize());
	}
	// else if (type == qMetaTypeId<VipRasterData>())
	//  {
	//  const VipNDArray ar = v.value<VipNDArray>();
	//  if (vipIsImageArray(ar))
	//  return ar.size() * 4;
	//  return ar.size()*ar.dataSize();
	//  }
	else if (type == qMetaTypeId<complex_f>())
		return (int)sizeof(complex_f);
	else if (type == qMetaTypeId<complex_d>())
		return (int)sizeof(complex_d);
	else if (type == qMetaTypeId<VipInterval>())
		return (int)sizeof(VipInterval);
	else if (type == qMetaTypeId<VipIntervalSample>())
		return (int)sizeof(VipIntervalSample);
	else if (type == qMetaTypeId<VipPointVector>())
		return (int)(v.value<VipPointVector>().size() * sizeof(VipPoint));
	else if (type == qMetaTypeId<VipIntervalSampleVector>())
		return (int)(v.value<VipIntervalSampleVector>().size() * sizeof(VipIntervalSample));

	QMap<int, int (*)(int, const QVariant&)>::iterator it = _mem_functions.find(type);
	if (it != _mem_functions.end())
		return it.value()(type, v);

	return 0;
}

int vipRegisterMemoryFootprintFunction(int metatype_id, int (*fun)(int, const QVariant&))
{
	_mem_functions[metatype_id] = fun;
	return metatype_id;
}

static qint64 defaultNanoSecondsSinceEpoch()
{
	return QDateTime::currentMSecsSinceEpoch() * 1000000;
}

typedef qint64 (*time_function)();
static time_function _time_function = defaultNanoSecondsSinceEpoch;
static time_function _ms_time_function = QDateTime::currentMSecsSinceEpoch;

void vipSetTimeFunction(qint64 (*fun)())
{
	_time_function = fun;
}
void vipSetMsTimeFunction(qint64 (*fun)())
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

static QStringList _icon_paths = QStringList() << "icons/";

QPixmap vipPixmap(const QString& name)
{
	for (qsizetype i = 0; i < _icon_paths.size(); ++i) {
		QPixmap icon = QPixmap(_icon_paths[i] + name);
		if (!icon.isNull())
			return icon;
	}

	return QPixmap();
}

QImage vipImage(const QString& name)
{
	for (qsizetype i = 0; i < _icon_paths.size(); ++i) {
		QImage icon = QImage(_icon_paths[i] + name);
		if (!icon.isNull())
			return icon;
	}

	return QImage();
}

QIcon vipIcon(const QString& name)
{
	QPixmap pix = vipPixmap(name);
	if (pix.isNull())
		return QIcon();
	else
		return QIcon(pix);
}

void vipAddIconPath(const QString& path)
{
	QString p = path;
	p.replace("\\", "/");
	if (!p.endsWith("/"))
		p += "/";

	_icon_paths.append(p);
}

void vipAddFrontIconPath(const QString& path)
{
	QString p = path;
	p.replace("\\", "/");
	if (!p.endsWith("/"))
		p += "/";

	_icon_paths.push_front(p);
}

void vipSetIconPaths(const QStringList& paths)
{
	_icon_paths = paths;
	for (qsizetype i = 0; i < _icon_paths.size(); ++i) {
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
	if (time == 0) {
		QString date = (__DATE__ + QString(" ") + QString(__TIME__));
		date.replace(":", " ");
		QStringList lst = date.split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		date = lst.join(" ");
		if (lst.size() == 6) {
			QString pattern;
			pattern += QString("M").repeated(lst[0].size()) + " ";
			pattern += QString("d").repeated(lst[1].size()) + " ";
			pattern += QString("y").repeated(lst[2].size()) + " ";
			pattern += QString("h").repeated(lst[3].size()) + " ";
			pattern += QString("m").repeated(lst[4].size()) + " ";
			pattern += QString("s").repeated(lst[5].size());

			QLocale locale(QLocale::English, QLocale::UnitedStates);
			time = locale.toDateTime(date, pattern).toMSecsSinceEpoch();
		}
	}
	return time;
}

static QString _editionVersion;
void vipSetEditionVersion(const QString& ver)
{
	_editionVersion = ver;
}
QString vipEditionVersion()
{
	return _editionVersion;
}

static QString _app_name;
void vipSetAppCanonicalPath(const QString& name)
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


