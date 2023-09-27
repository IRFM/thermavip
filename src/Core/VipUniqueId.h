#ifndef VIP_UNIQUE_ID_H
#define VIP_UNIQUE_ID_H

#include <QObject>
#include <QPointer>
#include "VipConfig.h"

/// \addtogroup Core
/// @{

///\internal Internal class used buy #VipUniqueId
class VIP_CORE_EXPORT VipTypeId : public QObject
{
	Q_OBJECT

public:
	friend class VipUniqueId;

	VipTypeId();
	~VipTypeId();

	QList<QObject*> objects() const;
	QObject * find(int id) const;

	int setId(const QObject * object, int id = 0);
	int id(const QObject * object) const;

	void removeId(QObject*);

Q_SIGNALS:
	void idChanged(QObject*, int );

private Q_SLOTS:
	void objectDestroyed(QObject*);

private:
	int findNextId();
	class PrivateData;
	PrivateData * m_data;
};

/// \a VipUniqueId provides a way to attribute a unique identifier (through the program) to QObject inheriting instances.
///
/// An object will have a different id depending on which superclass we address it through.
/// Let's say we have an inheritance tree of the type QObject -> MyBaseClass -> MyDerivedClass.
/// Then VipUniqueId::id<QObject>() might be different than VipUniqueId::id<MyBaseClass>() and VipUniqueId::id<MyDerivedClass>().
/// The id is only guaranteed to be unique within a class type.
/// In any case, calling #VipUniqueId::id on a QObject pointer will set the id for the object actual class and all super classes that declare the Q_OBJECT macro.
///
/// You can change the id of an object for a specific class type through #VipUniqueId::setId. If the new id is already used by an object, this object's id will be reset
/// to a new unique one.
///
/// Object ids start at 1 and are incremented for each new object.
///
/// All functions of \a VipUniqueId are thread safe.
class VIP_CORE_EXPORT VipUniqueId : public QObject
{
	Q_OBJECT

public:
	friend class VipTypeId;

	~VipUniqueId();

	///Returns the id of given object for given template class type
	template< class T>
	static int id(const T * obj) {return registerMetaType(&T::staticMetaObject, obj);}

	///Set the id of a given object for a specific template class
	template< class T>
	static int setId(T * obj, int id) { return registerMetaType(&T::staticMetaObject, obj, id); }

	///Find an object of given id for a specific template class
	template< class T>
	static T* find(int id) { return static_cast<T*>(typeId<T>()->find(id));}

	///Return the list of objects that can be casted to the template type
	template< class T>
	static QList<T*> objects() {
		QList<QObject*> objects = typeId<T>()->objects();
		QList<T*> result;
		for(int i=0; i < objects.size(); ++i)
			result.append(static_cast<T*>(objects[i]));
		return result;
	}

	///Return the list of objects that inherits given QMetaObject
	static QList<QObject*> objects(const QMetaObject * metaobject) {
		return typeId(metaobject)->objects();
	}


	///Returns the #VipTypeId object for a specific QMetaObject.
	static VipTypeId* typeId(const QMetaObject * metaobject);

	///Returns the #VipTypeId object for a specific template class type.
	template< class T>
	static VipTypeId* typeId() {return typeId(&T::staticMetaObject);}

private:
	VipUniqueId();
	static VipUniqueId & instance();
	static int registerMetaType(const QMetaObject * metaobject, const QObject * obj , int id=0);

	class PrivateData;
	PrivateData * m_data;
};


/// VipLazyPointer is a kind of pointer to QObject.
///
/// VipLazyPointer holds a pointer to QObject and can be initialized through a QObject pointer or an object identifier (see #VipUniqueId for more details).
/// As its name implies, whene initialized with an object identifier, VipLazyPointer does not know the underlying QObject. It's only
/// when querying the object through #VipLazyPointer::data that the QObject is searched and returned (if found).
///
/// VipLazyPointer is usefull to serialize QObject objects that refer to each other.
/// 
class VipLazyPointer
{
	int m_id;
	const QMetaObject * m_meta;
	QPointer<QObject> m_object;
public:

	VipLazyPointer(int id = 0)
	:m_id(id), m_meta(NULL){}
	template< class T>
	VipLazyPointer(T * obj)
	:m_id(VipUniqueId::id(obj)), m_meta(&T::staticMetaObject), m_object(obj) {}

	/// Returns the pointer identifier
	int id() const {
		if (m_object && m_meta)
			return const_cast<int&>(m_id) = VipUniqueId::typeId(m_meta)->id(m_object.data());
		return m_id;
	}

	/// Tells if the lazy pointer is empty.
	/// An empty pointer means that the pointer has not been queried yet with data(), or that
	/// the pointer has been queried and does not exist yet.
	bool isEmpty() const
	{
		return !m_object;
	}

	template< class T>
	void setData(T * obj)
	{
		m_id= VipUniqueId::id(obj);
		m_object = obj;
		m_meta = &T::staticMetaObject;
	}

	template< class T>
	T * data(bool * just_found = NULL) const
	{
		if (m_meta)
			return qobject_cast<T*>(const_cast<QObject*>(m_object.data()));
		else
		{
			VipLazyPointer * _this = const_cast<VipLazyPointer*>(this);
			T * tmp = VipUniqueId::find<T>(m_id);
			_this->m_object = tmp;
			if (just_found)
				*just_found = tmp;
			if (tmp)
				_this->m_meta = &T::staticMetaObject;
			return tmp;
		}
	}
};


#include "VipSceneModel.h"

/// VipLazySceneModel is a lazy pointer of VipSceneModel
class VipLazySceneModel
{
	VipLazyPointer m_pointer;
	VipSceneModel m_scene;
public:

	VipLazySceneModel(int id = 0):
	m_pointer(id) {}
	VipLazySceneModel(const VipSceneModel & sm)
	:m_pointer(sm.shapeSignals()), m_scene(sm) {}
	VipLazySceneModel(const VipLazySceneModel & other)
	:m_pointer(other.m_pointer), m_scene(other.m_scene) {}

	int id() const {return m_pointer.id();}

	bool isEmpty() const
	{
		return m_pointer.isEmpty();
	}

	/// Returns true of a scene model can be found
	bool hasSceneModel() const;

	void setSceneModel(const VipSceneModel& sm);

	VipSceneModel sceneModel() const;

	VipLazySceneModel & operator=(const VipLazySceneModel & other) {
		m_pointer = other.m_pointer;
		m_scene = other.m_scene;
		return *this;
	}

	VipLazySceneModel & operator=(const VipSceneModel & other) {
		setSceneModel(other);
		return *this;
	}

	operator VipSceneModel() const {return sceneModel();}
};


Q_DECLARE_METATYPE(VipLazyPointer)
Q_DECLARE_METATYPE(VipLazySceneModel)

class QDataStream;
VIP_CORE_EXPORT QDataStream & operator<<(QDataStream & arch, const VipLazyPointer & value);
VIP_CORE_EXPORT QDataStream & operator>>(QDataStream & arch, VipLazyPointer & value);

VIP_CORE_EXPORT QDataStream & operator<<(QDataStream & arch, const VipLazySceneModel & value);
VIP_CORE_EXPORT QDataStream & operator>>(QDataStream & arch, VipLazySceneModel & value);


/// @}
//end Core

#endif
