#pragma once

#include "VipFunctional.h"

/// @brief Base class with an integer attribute
class BaseClass : public QObject
{
	Q_OBJECT

public:
	int ivalue;

	BaseClass(int v)
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

	DerivedClass(int iv, double dv)
	  : BaseClass(iv)
	  , dvalue(dv)
	{
	}
};
// Register DerivedClass to the Qt metatype system as well as the thermavip layer
VIP_REGISTER_QOBJECT_METATYPE(DerivedClass*)



// define serialization function for both classes


VipArchive& operator<<(VipArchive& arch, const BaseClass* o)
{
	return arch.content("ivalue", o->ivalue);
}
VipArchive& operator>>(VipArchive& arch, BaseClass* o)
{
	return arch.content("ivalue", o->ivalue);
}

VipArchive& operator<<(VipArchive& arch, const DerivedClass* o)
{
	return arch.content("dvalue", o->dvalue);
}
VipArchive& operator>>(VipArchive& arch, DerivedClass* o)
{
	return arch.content("dvalue", o->dvalue);
}




QObject* getDerivedObject(int iv, double dv)
{
	return new DerivedClass(iv, dv);
}