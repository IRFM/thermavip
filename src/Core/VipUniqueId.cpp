#include "VipUniqueId.h"

#include <QMap>
#include <QPointer>
#include <QVariant>
#include <QMutex>
#include <QSharedPointer>


class VipTypeId::PrivateData
{
public:
	PrivateData() : mutex(QMutex::Recursive) {}
	const QMetaObject * metaobject;
	QMap<int,QPointer<QObject> > ids;
	QMap<QObject*, int> objects_to_id;
	QMutex mutex;
};

VipTypeId::VipTypeId()
{
	m_data = new PrivateData();
}

VipTypeId::~VipTypeId()
{
	{
	QMutexLocker lock(&m_data->mutex);
	m_data->ids.clear();
	m_data->objects_to_id.clear();
	}
	delete m_data;
}


int VipTypeId::findNextId()
{
	int id = m_data->ids.size() + 1;
	int i = 0;

	QMap<int, QPointer<QObject> >::iterator it = m_data->ids.begin();
	while (it != m_data->ids.end())
	{
		//remove entry if object has been deleted
		if (!it.value())
		{
			it = m_data->ids.erase(it);
			id = m_data->ids.size() + 1;
			continue;
		}
		else if (it.key() != i + 1)
		{
			id = i + 1;
			break;
		}

		++it;
		++i;
	}

	return id;
}



QList<QObject*> VipTypeId::objects() const
{
	QMutexLocker lock(&m_data->mutex);
	const QList<QPointer<QObject> > lst =m_data->ids.values();
	QList<QObject*> res;
	for(int i=0; i < lst.size(); ++i)
		if(QObject * tmp = lst[i])
			res.append(tmp);
	return res;
}

QObject * VipTypeId::find(int id) const
{
	QMutexLocker lock(&m_data->mutex);
	QMap<int,QPointer<QObject> >::const_iterator it = m_data->ids.find(id);
	if(it != m_data->ids.end())
		return it.value();
	return nullptr;
}

int VipTypeId::setId(const QObject * obj, int id )
{
	if(id < 0 || ! obj)
		return 0;

	QObject * object = const_cast<QObject*>(obj);

	QMutexLocker lock(&m_data->mutex);

	QMap<QObject *, int >::iterator it = m_data->objects_to_id.find(object);
	if (it == m_data->objects_to_id.end()) {
		//new object
		connect(object, SIGNAL(destroyed(QObject*)), this, SLOT(objectDestroyed(QObject*)),Qt::DirectConnection);
	}

	//nullptr id: return the current one or create a new one
	if (id == 0) {
		//existing object, try to return the current id
		if (it != m_data->objects_to_id.end())
			return it.value();

		//find next id
		int next = findNextId();

		//add object
		m_data->ids[next] = object;
		m_data->objects_to_id[object] = next;
		Q_EMIT idChanged(object, next);
		return next;
	}

	//set the id
	int current_id = (it == m_data->objects_to_id.end()) ? 0 : it.value();
	if (id == current_id)
		//same id, nothing to do
		return id;

	QMap<int, QPointer<QObject> >::iterator previous = m_data->ids.find(id);
	if (previous == m_data->ids.end()) {
		//this is is available, just set it

		if (it != m_data->objects_to_id.end()) {
			//remove first
			int key = it.value();
			m_data->objects_to_id.erase(it);
			m_data->ids.remove(key);
		}
		m_data->objects_to_id.insert(object, id);
		m_data->ids.insert(id, object);
		Q_EMIT idChanged(object, id);
		return id;
	}
	else {
		//find next id
		int next = findNextId();

		if (it != m_data->objects_to_id.end()) {
			//remove this object
			int key = it.value();
			m_data->objects_to_id.erase(it);
			m_data->ids.remove(key);
		}
		//remove previous
		QObject * prev = previous.value();
		m_data->objects_to_id.remove(prev);
		m_data->ids.erase(previous);

		//add this object
		m_data->objects_to_id.insert(object, id);
		m_data->ids.insert(id, object);
		Q_EMIT idChanged(object, id);

		//add previously found
		m_data->objects_to_id.insert(prev, next);
		m_data->ids.insert(next, prev);
		Q_EMIT idChanged(prev, next);
		return id;
	}
}

int VipTypeId::id(const QObject * object) const
{
	return const_cast<VipTypeId*>(this)->setId(object,0);
}

void VipTypeId::objectDestroyed(QObject* object)
{
	QMutexLocker lock(&m_data->mutex);
	QMap<QObject*, int>::iterator it = m_data->objects_to_id.find(object);
	if (it != m_data->objects_to_id.end()) {
		int id = it.value();
		m_data->objects_to_id.erase(it);
		m_data->ids.remove(id);
	}
}

void VipTypeId::removeId(QObject* object)
{
	QMutexLocker lock(&m_data->mutex);
	QMap<QObject*, int>::iterator it = m_data->objects_to_id.find(object);
	if (it != m_data->objects_to_id.end()) {
		int id = it.value();
		m_data->objects_to_id.erase(it);
		m_data->ids.remove(id);
		disconnect(object, SIGNAL(destroyed(QObject*)), this, SLOT(objectDestroyed(QObject*)));
	}
}



struct TypeIdPtr
{
	QSharedPointer<VipTypeId> type;
	TypeIdPtr() : type(new VipTypeId()) {}
};

class VipUniqueId::PrivateData
{
public:
	PrivateData() : mutex(QMutex::Recursive) {}
	QMap<const QMetaObject*,TypeIdPtr> ids;
	QMutex mutex;
};

VipUniqueId & VipUniqueId::instance()
{
	static VipUniqueId inst;
	return inst;
}

VipUniqueId::VipUniqueId()
{
	m_data = new PrivateData();
}

VipUniqueId::~VipUniqueId()
{
	{
		QMutexLocker lock(&m_data->mutex);
		m_data->ids.clear();
	}
	delete m_data;
}

VipTypeId* VipUniqueId::typeId(const QMetaObject * metaobject)
{
	QMutexLocker lock(&instance().m_data->mutex);
	VipTypeId * t = instance().m_data->ids[metaobject].type.data();
	t->m_data->metaobject = metaobject;
	return t;
}

int VipUniqueId::registerMetaType(const QMetaObject * metaobject, const QObject * obj , int id)
{
	if(!obj)
		return 0;
	QMutexLocker lock(&instance().m_data->mutex);
	return instance().m_data->ids[metaobject].type->setId(obj, id);
}






bool VipLazySceneModel::hasSceneModel() const
{
	VipShapeSignals* ss = m_pointer.data<VipShapeSignals>();
	if (ss)
	{
		if (ss != m_scene.shapeSignals())
			const_cast<VipSceneModel&>(m_scene) = ss->sceneModel();
		return true;
	}
	else
		return false;
}

void VipLazySceneModel::setSceneModel(const VipSceneModel& sm)
{
	m_pointer.setData(sm.shapeSignals());
	m_scene = sm;
}

VipSceneModel VipLazySceneModel::sceneModel() const
{
	hasSceneModel();
	return m_scene;
}


#include <QDataStream>

QDataStream & operator<<(QDataStream & stream, const VipLazyPointer & value) {return stream << value.id();}
QDataStream & operator>>(QDataStream & stream, VipLazyPointer & value) {
	int id = 0;
	stream >> id;
	value = VipLazyPointer(id);
	return stream;
}

QDataStream & operator<<(QDataStream & stream, const VipLazySceneModel & value) {return stream << value.id();}
QDataStream & operator>>(QDataStream & stream, VipLazySceneModel & value) {
	int id = 0;
	stream >> id;
	value = VipLazySceneModel(id);
	return stream;
}

#include "VipArchive.h"

static VipArchive & operator<<(VipArchive & arch, const VipLazyPointer & ptr)
{
	return arch.content("id", ptr.id());
}

static VipArchive & operator>>(VipArchive & arch, VipLazyPointer & ptr)
{
	ptr = VipLazyPointer(arch.read("id").toInt());
	return arch;
}

static VipArchive & operator<<(VipArchive & arch, const VipLazySceneModel & ptr)
{
	return arch.content("id", ptr.id());
}

static VipArchive & operator>>(VipArchive & arch, VipLazySceneModel & ptr)
{
	ptr = VipLazySceneModel(arch.read("id").toInt());
	return arch;
}

static int registerStreamOperators()
{
	qRegisterMetaTypeStreamOperators<VipLazyPointer>("VipLazyPointer");
	qRegisterMetaTypeStreamOperators<VipLazySceneModel>("VipLazySceneModel");
	vipRegisterArchiveStreamOperators<VipLazyPointer>();
	vipRegisterArchiveStreamOperators<VipLazySceneModel>();
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();

