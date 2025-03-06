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

#include <QMap>

#include "VipArrayBase.h"
#include "VipComplex.h"
#include "VipIterator.h"

bool vipIsArithmetic(int type) noexcept
{
	return type == QMetaType::Bool || type == QMetaType::Char || type == QMetaType::SChar || type == QMetaType::UChar || type == QMetaType::Short || type == QMetaType::UShort ||
	       type == QMetaType::Int || type == QMetaType::UInt || type == QMetaType::Long || type == QMetaType::ULong || type == QMetaType::LongLong || type == QMetaType::ULongLong ||
	       type == QMetaType::Float || type == QMetaType::Double || type == qMetaTypeId<long double>();
}

bool vipIsComplex(int type) noexcept
{
	return type == qMetaTypeId<complex_d>() || type == qMetaTypeId<complex_f>();
}

bool vipCanConvertStdTypes(int from, int to) noexcept
{
	if (from == to)
		return true;

	if (vipIsArithmetic(from))
		return vipIsArithmetic(to) || vipIsComplex(to) || to == QMetaType::QString || to == QMetaType::QByteArray;

	if (vipIsComplex(from))
		return vipIsComplex(to) || to == QMetaType::QString || to == QMetaType::QByteArray;

	if (from == QMetaType::QString)
		return vipIsArithmetic(to) || vipIsComplex(to) || to == QMetaType::QString || to == QMetaType::QByteArray || to == qMetaTypeId<VipRGB>();

	if (from == QMetaType::QByteArray)
		return vipIsArithmetic(to) || vipIsComplex(to) || to == QMetaType::QString || to == QMetaType::QByteArray || to == qMetaTypeId<VipRGB>();

	if (from == qMetaTypeId<VipRGB>())
		return (to == QMetaType::QString || to == QMetaType::QByteArray);

	return false;
}

bool vipCanConvert(int from, int to)
{
	QVariant v_from = vipFromVoid(from, nullptr);
	bool res = v_from.canConvert(VIP_META(to));

	// delete potential the QObject that was created
	if (QObject* obj = v_from.value<QObject*>())
		delete obj;

	return res;
}

static SharedHandle nullHandle = SharedHandle(new detail::NullHandle());

static QMap<int, QMap<int, SharedHandle>>& tables()
{
	static QMap<int, QMap<int, SharedHandle>> instances_;

	if (!instances_.size()) {
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
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<QString>()] = SharedHandle(new detail::StdHandle<QString>());
		instances_[VipNDArrayHandle::Standard][qMetaTypeId<VipRGB>()] = SharedHandle(new detail::StdHandle<VipRGB>());
	}

	return instances_;
}

namespace detail
{
	SharedHandle getHandle(int handle_type, int metatype)
	{
		QMap<int, QMap<int, SharedHandle>>::iterator it_type = tables().find(handle_type);
		if (it_type != tables().end()) {
			QMap<int, SharedHandle>::iterator it_meta = it_type.value().find(metatype);
			if (it_meta == it_type.value().end()) {
				if (it_type.value().size() == 1 && it_type.value().begin().key() == 0)
					return it_type.value().begin().value();
			}
			else
				return it_meta.value();
		}
		return vipNullHandle();
	}
}

VipNDArrayHandle* vipNullHandlePtr() noexcept
{
	static detail::NullHandle handle;
	return &handle;
}

SharedHandle vipNullHandle() noexcept
{
	static SharedHandle h = SharedHandle(vipNullHandlePtr());
	return h;
}

int vipRegisterArrayType(int hanndle_type, int metaType, const SharedHandle& handle)
{
	tables()[hanndle_type][metaType] = handle;
	return 0;
}

SharedHandle vipCreateArrayHandle(int hanndle_type, int metaType, const VipNDArrayShape& shape)
{
	SharedHandle handle = detail::getHandle(hanndle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();
	handle.detach();
	if (shape.size())
		handle->realloc(shape);
	return handle;
}

SharedHandle vipCreateArrayHandle(int hanndle_type, int metaType, void* ptr, const VipNDArrayShape& shape, const vip_deleter_type& del)
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
