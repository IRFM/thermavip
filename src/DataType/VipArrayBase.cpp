#include <QMap>

#include "VipArrayBase.h"
#include "VipIterator.h"
#include "VipComplex.h"

bool vipIsArithmetic(uint type)
{
	return type == QMetaType::Bool ||
		type == QMetaType::Char ||
		type == QMetaType::SChar ||
		type == QMetaType::UChar ||
		type == QMetaType::Short ||
		type == QMetaType::UShort ||
		type == QMetaType::Int ||
		type == QMetaType::UInt ||
		type == QMetaType::Long ||
		type == QMetaType::ULong ||
		type == QMetaType::LongLong ||
		type == QMetaType::ULongLong ||
		type == QMetaType::Float ||
		type == QMetaType::Double ||
		type == (uint)qMetaTypeId<long double>();
}


bool vipIsComplex(uint type)
{
	return type == (uint)qMetaTypeId<complex_d>() ||
		type == (uint)qMetaTypeId<complex_f>();
}

bool vipCanConvertStdTypes(uint from, uint to)
{
	if (from == to)
		return true;

	if (vipIsArithmetic(from))
		return vipIsArithmetic(to) ||
		vipIsComplex(to) ||
		to == QVariant::String ||
		to == QMetaType::QByteArray;

	if (vipIsComplex(from))
		return vipIsComplex(to) || to == QVariant::String || to == QMetaType::QByteArray;

	if (from == QVariant::String)
		return vipIsArithmetic(to) ||
		vipIsComplex(to) ||
		to == QVariant::String ||
		to == QMetaType::QByteArray ||
		to == (uint)qMetaTypeId<VipRGB>();

	if (from == QMetaType::QByteArray)
		return vipIsArithmetic(to) ||
		vipIsComplex(to) ||
		to == QVariant::String ||
		to == QMetaType::QByteArray ||
		to == (uint)qMetaTypeId<VipRGB>();

	if (from == (uint)qMetaTypeId<VipRGB>())
		return (to == QMetaType::QString || to == QMetaType::QByteArray);

	return false;
}



bool vipCanConvert(uint from, uint to)
{
	QVariant v_from(from, nullptr);
	bool res = v_from.canConvert(to);

	//delete potential the QObject that was created
	if (QObject * obj = v_from.value<QObject*>())
		delete obj;

	return res;
}





static SharedHandle nullHandle = SharedHandle(new detail::NullHandle());

static QMap<int, QMap<int, SharedHandle> > & tables()
{
	static QMap<int, QMap<int, SharedHandle> > instances_;

	if (!instances_.size())
	{
		instances_[0].insert(0, nullHandle);
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<bool>()] = SharedHandle(new detail::StdHandle<bool>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<char>()] = SharedHandle(new detail::StdHandle<char>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<qint8>()] = SharedHandle(new detail::StdHandle<qint8>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<quint8>()] = SharedHandle(new detail::StdHandle<quint8>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<qint16>()] = SharedHandle(new detail::StdHandle<qint16>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<quint16>()] = SharedHandle(new detail::StdHandle<quint16>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<qint32>()] = SharedHandle(new detail::StdHandle<qint32>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<quint32>()] = SharedHandle(new detail::StdHandle<quint32>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<long>()] = SharedHandle(new detail::StdHandle<long>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<unsigned long>()] = SharedHandle(new detail::StdHandle<unsigned long>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<qint64>()] = SharedHandle(new detail::StdHandle<qint64>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<quint64>()] = SharedHandle(new detail::StdHandle<quint64>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<float>()] = SharedHandle(new detail::StdHandle<float>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<double>()] = SharedHandle(new detail::StdHandle<double>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<long double>()] = SharedHandle(new detail::StdHandle<long double>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<complex_f>()] = SharedHandle(new detail::StdHandle<complex_f>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<complex_d>()] = SharedHandle(new detail::StdHandle<complex_d>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<QString >()] = SharedHandle(new detail::StdHandle<QString >());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<VipRGB >()] = SharedHandle(new detail::StdHandle<VipRGB >());
	}

	return instances_;
}

namespace detail
{
	SharedHandle getHandle(int handle_type, int metatype)
	{
		QMap<int, QMap<int, SharedHandle> >::iterator it_type = tables().find(handle_type);
		if (it_type != tables().end())
		{
			QMap<int, SharedHandle>::iterator it_meta = it_type.value().find(metatype);
			if (it_meta == it_type.value().end())
			{
				if (it_type.value().size() == 1 && it_type.value().begin().key() == 0)
					return it_type.value().begin().value();
			}
			else
				return it_meta.value();
		}
		return vipNullHandle();
	}
}

VipNDArrayHandle* vipNullHandlePtr()
{
	static detail::NullHandle handle;
	return &handle;
}

SharedHandle vipNullHandle()
{
	static SharedHandle h = SharedHandle(vipNullHandlePtr());
	return h;
}

int vipRegisterArrayType(int hanndle_type, int metaType, const SharedHandle & handle)
{
	tables()[hanndle_type][metaType] = handle;
	return 0;
}

SharedHandle vipCreateArrayHandle(int hanndle_type, int metaType, const VipNDArrayShape & shape)
{
	SharedHandle handle = detail::getHandle(hanndle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();
	handle.detach();
	if (shape.size())
		handle->realloc(shape);
	return handle;
}

SharedHandle vipCreateArrayHandle(int hanndle_type, int metaType, void * ptr, const VipNDArrayShape & shape, const vip_deleter_type & del )
{
	SharedHandle handle = detail::getHandle(hanndle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();
	handle.detach();
	handle->opaque = ptr;
	handle->shape = shape;
	handle->size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, handle->strides);
	handle->deleter = del;
	return handle;
}

SharedHandle vipCreateArrayHandle(int hanndle_type, int metaType)
{
	SharedHandle handle = detail::getHandle(hanndle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();

	handle.detach();
	return handle;
}
