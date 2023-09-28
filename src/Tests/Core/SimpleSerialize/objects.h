#pragma once

#include "VipFunctional.h"
#include "VipArchive.h"

/// @brief Base class with an integer attribute
class BaseClass : public QObject
{
	Q_OBJECT

public:
	int ivalue;

	BaseClass(int v = 0)
	  : ivalue(v)
	{
	}
};
// Register BaseClass to the Qt metatype system as well as the thermavip layer
VIP_REGISTER_QOBJECT_METATYPE(BaseClass*)


/// @brief Derived class with a double attribute
class DerivedClass : public BaseClass
{
	Q_OBJECT

public:
	double dvalue;

	DerivedClass(int iv = 0, double dv = 0.)
	  : BaseClass(iv)
	  , dvalue(dv)
	{
	}
};
// Register DerivedClass to the Qt metatype system as well as the thermavip layer
VIP_REGISTER_QOBJECT_METATYPE(DerivedClass*)



// define serialization function for both classes


inline VipArchive& operator<<(VipArchive& arch, const BaseClass* o)
{
	return arch.content("ivalue", o->ivalue);
}
inline VipArchive& operator>>(VipArchive& arch, BaseClass* o)
{
	return arch.content("ivalue", o->ivalue);
}

inline VipArchive& operator<<(VipArchive& arch, const DerivedClass* o)
{
	return arch.content("dvalue", o->dvalue);
}
inline VipArchive& operator>>(VipArchive& arch, DerivedClass* o)
{
	return arch.content("dvalue", o->dvalue);
}

