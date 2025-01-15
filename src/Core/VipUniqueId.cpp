/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipUniqueId.h"

#include <QMap>
#include <QMutex>
#include <QPointer>
#include <QSharedPointer>
#include <QVariant>

class VipTypeId::PrivateData
{
public:
	PrivateData()
	{
	}
	const QMetaObject* metaobject;
	QMap<int, QPointer<QObject>> ids;
	QMap<QObject*, int> objects_to_id;
	QRecursiveMutex mutex;
};

VipTypeId::VipTypeId()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipTypeId::~VipTypeId()
{
	{
		QMutexLocker lock(&d_data->mutex);
		d_data->ids.clear();
		d_data->objects_to_id.clear();
	}
}

int VipTypeId::findNextId()
{
	int id = d_data->ids.size() + 1;
	int i = 0;

	QMap<int, QPointer<QObject>>::iterator it = d_data->ids.begin();
	while (it != d_data->ids.end()) {
		// remove entry if object has been deleted
		if (!it.value()) {
			it = d_data->ids.erase(it);
			id = d_data->ids.size() + 1;
			continue;
		}
		else if (it.key() != i + 1) {
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
	QMutexLocker lock(&d_data->mutex);
	const QList<QPointer<QObject>> lst = d_data->ids.values();
	QList<QObject*> res;
	for (int i = 0; i < lst.size(); ++i)
		if (QObject* tmp = lst[i])
			res.append(tmp);
	return res;
}

QObject* VipTypeId::find(int id) const
{
	QMutexLocker lock(&d_data->mutex);
	QMap<int, QPointer<QObject>>::const_iterator it = d_data->ids.find(id);
	if (it != d_data->ids.end())
		return it.value();
	return nullptr;
}

int VipTypeId::setId(const QObject* obj, int id)
{
	if (id < 0 || !obj)
		return 0;

	QObject* object = const_cast<QObject*>(obj);

	QMutexLocker lock(&d_data->mutex);

	QMap<QObject*, int>::iterator it = d_data->objects_to_id.find(object);
	if (it == d_data->objects_to_id.end()) {
		// new object
		connect(object, SIGNAL(destroyed(QObject*)), this, SLOT(objectDestroyed(QObject*)), Qt::DirectConnection);
	}

	// nullptr id: return the current one or create a new one
	if (id == 0) {
		// existing object, try to return the current id
		if (it != d_data->objects_to_id.end())
			return it.value();

		// find next id
		int next = findNextId();

		// add object
		d_data->ids[next] = object;
		d_data->objects_to_id[object] = next;
		Q_EMIT idChanged(object, next);
		return next;
	}

	// set the id
	int current_id = (it == d_data->objects_to_id.end()) ? 0 : it.value();
	if (id == current_id)
		// same id, nothing to do
		return id;

	QMap<int, QPointer<QObject>>::iterator previous = d_data->ids.find(id);
	if (previous == d_data->ids.end()) {
		// this is is available, just set it

		if (it != d_data->objects_to_id.end()) {
			// remove first
			int key = it.value();
			d_data->objects_to_id.erase(it);
			d_data->ids.remove(key);
		}
		d_data->objects_to_id.insert(object, id);
		d_data->ids.insert(id, object);
		Q_EMIT idChanged(object, id);
		return id;
	}
	else {
		// find next id
		int next = findNextId();

		if (it != d_data->objects_to_id.end()) {
			// remove this object
			int key = it.value();
			d_data->objects_to_id.erase(it);
			d_data->ids.remove(key);
		}
		// remove previous
		QObject* prev = previous.value();
		d_data->objects_to_id.remove(prev);
		d_data->ids.erase(previous);

		// add this object
		d_data->objects_to_id.insert(object, id);
		d_data->ids.insert(id, object);
		Q_EMIT idChanged(object, id);

		// add previously found
		d_data->objects_to_id.insert(prev, next);
		d_data->ids.insert(next, prev);
		Q_EMIT idChanged(prev, next);
		return id;
	}
}

int VipTypeId::id(const QObject* object) const
{
	return const_cast<VipTypeId*>(this)->setId(object, 0);
}

void VipTypeId::objectDestroyed(QObject* object)
{
	QMutexLocker lock(&d_data->mutex);
	QMap<QObject*, int>::iterator it = d_data->objects_to_id.find(object);
	if (it != d_data->objects_to_id.end()) {
		int id = it.value();
		d_data->objects_to_id.erase(it);
		d_data->ids.remove(id);
	}
}

void VipTypeId::removeId(QObject* object)
{
	QMutexLocker lock(&d_data->mutex);
	QMap<QObject*, int>::iterator it = d_data->objects_to_id.find(object);
	if (it != d_data->objects_to_id.end()) {
		int id = it.value();
		d_data->objects_to_id.erase(it);
		d_data->ids.remove(id);
		disconnect(object, SIGNAL(destroyed(QObject*)), this, SLOT(objectDestroyed(QObject*)));
	}
}

struct TypeIdPtr
{
	QSharedPointer<VipTypeId> type;
	TypeIdPtr()
	  : type(new VipTypeId())
	{
	}
};

class VipUniqueId::PrivateData
{
public:
	QMap<const QMetaObject*, TypeIdPtr> ids;
	QRecursiveMutex mutex;
};

VipUniqueId& VipUniqueId::instance()
{
	static VipUniqueId inst;
	return inst;
}

VipUniqueId::VipUniqueId()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipUniqueId::~VipUniqueId()
{
	{
		QMutexLocker lock(&d_data->mutex);
		d_data->ids.clear();
	}
}

VipTypeId* VipUniqueId::typeId(const QMetaObject* metaobject)
{
	QMutexLocker lock(&instance().d_data->mutex);
	VipTypeId* t = instance().d_data->ids[metaobject].type.data();
	t->d_data->metaobject = metaobject;
	return t;
}

int VipUniqueId::registerMetaType(const QMetaObject* metaobject, const QObject* obj, int id)
{
	if (!obj)
		return 0;
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->ids[metaobject].type->setId(obj, id);
}

bool VipLazySceneModel::hasSceneModel() const
{
	VipShapeSignals* ss = m_pointer.data<VipShapeSignals>();
	if (ss) {
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

QDataStream& operator<<(QDataStream& stream, const VipLazyPointer& value)
{
	return stream << value.id();
}
QDataStream& operator>>(QDataStream& stream, VipLazyPointer& value)
{
	int id = 0;
	stream >> id;
	value = VipLazyPointer(id);
	return stream;
}

QDataStream& operator<<(QDataStream& stream, const VipLazySceneModel& value)
{
	return stream << value.id();
}
QDataStream& operator>>(QDataStream& stream, VipLazySceneModel& value)
{
	int id = 0;
	stream >> id;
	value = VipLazySceneModel(id);
	return stream;
}

#include "VipArchive.h"

static VipArchive& operator<<(VipArchive& arch, const VipLazyPointer& ptr)
{
	return arch.content("id", ptr.id());
}

static VipArchive& operator>>(VipArchive& arch, VipLazyPointer& ptr)
{
	ptr = VipLazyPointer(arch.read("id").toInt());
	return arch;
}

static VipArchive& operator<<(VipArchive& arch, const VipLazySceneModel& ptr)
{
	return arch.content("id", ptr.id());
}

static VipArchive& operator>>(VipArchive& arch, VipLazySceneModel& ptr)
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
