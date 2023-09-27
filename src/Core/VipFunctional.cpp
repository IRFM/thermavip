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

//return false if equal, true otherwise
bool VipFunctionDispatcher::nonConvertible(const VipTypeList & t1, const VipTypeList & t2) const
{
	for(int i = 0; i < m_argCount; ++i)
	{
		if(t1.size() <= i || t2.size() <= i ||
			!vipIsConvertible( t1[i], t2[i]) )
			return true;
	}
	return false;
}

bool VipFunctionDispatcher::exactEqual(const VipTypeList & t1, const VipTypeList & t2) const
{
	for(int i = 0; i < m_argCount; ++i)
	{
		if(t1.size() <= i || t2.size() <= i || t1[i].id != t2[i].id)
			return false;
	}
	return true;
}

const VipFunctionDispatcher::FunctionList & VipFunctionDispatcher::functions() const {return m_functions;}

int VipFunctionDispatcher::arity() const {return m_argCount;}

VipFunctionDispatcher::FunctionList VipFunctionDispatcher::match(const VipTypeList & lst) const
{
	FunctionList res;
	for(FunctionList::const_iterator it = m_functions.begin(); it != m_functions.end(); ++it)
	{
		if(!nonConvertible(lst,it->typeList()))
			res << *it;
	}

	return res;
}

VipFunctionDispatcher::FunctionList VipFunctionDispatcher::match(const VipAny & v1, const VipAny & v2, const VipAny & v3, const VipAny & v4, const VipAny & v5) const
{
	VipTypeList lst;
	lst << v1.type()<< v2.type() << v3.type() << v4.type() << v5.type();
	return match(lst);
}

VipFunctionDispatcher::FunctionList VipFunctionDispatcher::exactMatch(const VipTypeList & lst) const
{
	FunctionList res;
	for(FunctionList::const_iterator it = m_functions.begin(); it != m_functions.end(); ++it)
	{
		if(exactEqual(lst,it->typeList()))
			res << *it;
	}

	return res;
}

VipFunctionDispatcher::FunctionList VipFunctionDispatcher::exactMatch(const VipAny & v1, const VipAny & v2, const VipAny & v3, const VipAny & v4, const VipAny & v5) const
{
	VipTypeList lst;
	lst << v1.type()<< v2.type() << v3.type() << v4.type() << v5.type();

	//for (int i = 0; i < m_argCount; ++i)
	// printf("%s, ", lst[i].name);
	// printf("\n");
	// for (int i = 0; i < m_functions.size(); ++i) {
	// VipTypeList tmp = m_functions[i].typeList();
	// for (int j = 0; j < m_argCount; ++j)
	// printf("%s, ", tmp[j].name);
	// printf("\n");
	// }
	// printf("\n");

	return exactMatch(lst);
}

VipAny VipFunctionDispatcher::callOneMatch(const VipAny & v1 , const VipAny & v2 , const VipAny & v3 , const VipAny & v4 , const VipAny & v5 ) const
{
	FunctionList lst = match(v1,v2,v3,v4,v5);
	if(lst.size())
	{
		return lst.back()(v1,v2,v3,v4,v5);
	}

	return VipAny();
}

VipAny VipFunctionDispatcher::callOneExactMatch(const VipAny & v1 , const VipAny & v2 , const VipAny & v3 , const VipAny & v4 , const VipAny & v5 ) const
{
	FunctionList lst = exactMatch(v1,v2,v3,v4,v5);
	if(lst.size())
	{
		return lst.back()(v1,v2,v3,v4,v5);
	}

	return VipAny();
}

QList<VipAny> VipFunctionDispatcher::callAllMatch(const VipAny & v1 , const VipAny & v2 , const VipAny & v3 , const VipAny & v4 , const VipAny & v5 ) const
{
	FunctionList lst = match(v1,v2,v3,v4,v5);
	QList<VipAny> res;
	for(int i=0; i < lst.size(); ++i)
	{
		res << lst[i](v1,v2,v3,v4,v5);
	}

	return res;
}

QList<VipAny> VipFunctionDispatcher::callAllExactMatch(const VipAny & v1 , const VipAny & v2 , const VipAny & v3 , const VipAny & v4 , const VipAny & v5 ) const
{
	FunctionList lst = exactMatch(v1,v2,v3,v4,v5);
	QList<VipAny> res;
	for(int i=0; i < lst.size(); ++i)
	{
		res << lst[i](v1,v2,v3,v4,v5);
	}

	return res;
}

//equivalent to callOneExactMatch
VipAny VipFunctionDispatcher::operator()(const VipAny & v1 , const VipAny & v2 , const VipAny & v3 , const VipAny & v4 , const VipAny & v5 ) const
{
	return callOneExactMatch(v1,v2,v3,v4,v5);
}

void VipFunctionDispatcher::append(const VipFunction & fun)
{
	m_functions.append(fun);
}

void VipFunctionDispatcher::append(const FunctionList & funs)
{
	m_functions.append(funs);
}

void VipFunctionDispatcher::remove(const VipFunction & fun)
{
	m_functions.removeAll(fun);
}

void VipFunctionDispatcher::remove(const FunctionList & lst)
{
	for(int i=0; i < lst.size(); ++i)
		remove(lst[i]);
}

void VipFunctionDispatcher::clear()
{
	m_functions.clear();
}
