#include "VipFunctional.h"
#include <qmutex.h>
namespace details
{
	static QMutex & registerMutex() {
		static QMutex inst(QMutex::Recursive);
		return inst;
	}
	static QMap<int, create_fun> & variantCreator() {
		static QMap<int, create_fun> inst;
		return inst;
	}
	void registerCreateVariant(int metatype, create_fun fun)
	{
		QMutexLocker lock(&registerMutex());
		variantCreator()[metatype] = fun;
	}
}

QVariant vipCreateVariant(int id)
{
	QMutexLocker lock(&details::registerMutex());
	QMap<int, details::create_fun>::const_iterator it = details::variantCreator().find(id);
	if (it != details::variantCreator().end())
		return it.value()();
	return QVariant(id,nullptr);
}

void vipReleaseVariant(const QVariant & v)
{
	//delete contained pointer if necessary
	if (QObject * obj = v.value<QObject*>())
		delete obj;
}


QList<int> vipUserTypes(int type)
{
	QList<int> types;
	int id = QMetaType::User;
	//int type = qMetaTypeId<T>();
	if(!type)
		return types;


	while(id)
	{
		//const char *name = QMetaType::typeName(id);
		if(QMetaType::isRegistered(id) )
		{
			if(vipIsConvertible(VipType(id),VipType(type)))
				types << id;
			++id;

		}
		else
		{
			id = 0;
		}
	}

	return types;
}
