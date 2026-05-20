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
#include <QMutexLocker>
#include <QMutex>

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
		return vipIsArithmetic(to) || vipIsComplex(to) || to == QMetaType::QString || to == QMetaType::QByteArray || to == qMetaTypeId<VipRGB>()|| to == qMetaTypeId<VipRGBf>();

	if (from == QMetaType::QByteArray)
		return vipIsArithmetic(to) || vipIsComplex(to) || to == QMetaType::QString || to == QMetaType::QByteArray || to == qMetaTypeId<VipRGB>()|| to == qMetaTypeId<VipRGBf>();

	if (VipIsRgbType(from))
		return (to == QMetaType::QString || to == QMetaType::QByteArray || VipIsRgbType(to));

	return false;
}

bool vipCanConvert(int from, int to)
{
	QVariant v_from = vipFromVoid(from, nullptr);
	bool res = v_from.canConvert(VIP_META(to));

	// delete the potential QObject that was created
	if (QObject* obj = v_from.value<QObject*>())
		delete obj;

	return res;
}

static VipSharedHandle nullHandle = VipSharedHandle(new detail::NullHandle());

static QMap<int, QMap<int, VipSharedHandle>>& tables()
{
	static QMap<int, QMap<int, VipSharedHandle>>* instances_ = new QMap<int, QMap<int, VipSharedHandle>>();
	return *instances_;
}
static QRecursiveMutex& tables_mutex()
{
	static QRecursiveMutex m;
	return m;
}

static bool initHandles()
{
	vipRegisterStandardArrayHandle<bool>();
	vipRegisterStandardArrayHandle<char>();
	vipRegisterStandardArrayHandle<signed char>();
	vipRegisterStandardArrayHandle<unsigned char>();
	vipRegisterStandardArrayHandle<qint16>();
	vipRegisterStandardArrayHandle<quint16>();
	vipRegisterStandardArrayHandle<qint32>();
	vipRegisterStandardArrayHandle<quint32>();
	vipRegisterStandardArrayHandle<qint64>();
	vipRegisterStandardArrayHandle<quint64>();
	vipRegisterStandardArrayHandle<float>();
	vipRegisterStandardArrayHandle<double>();
	vipRegisterStandardArrayHandle<long double>();
	vipRegisterStandardArrayHandle<complex_f>();
	vipRegisterStandardArrayHandle<complex_d>();
	vipRegisterStandardArrayHandle<QByteArray>();
	vipRegisterStandardArrayHandle<QString>();
	vipRegisterStandardArrayHandle<VipRGB>();
	vipRegisterStandardArrayHandle<VipRGBf>();
	return true;
}
static int _initHandles = vipStaticInit("initHandles",initHandles);

namespace detail
{
	VipSharedHandle getHandle(int handle_type, int metatype)
	{
		QMutexLocker lock(&tables_mutex());
		QMap<int, QMap<int, VipSharedHandle>>::iterator it_type = tables().find(handle_type);
		if (it_type != tables().end()) {
			QMap<int, VipSharedHandle>::iterator it_meta = it_type.value().find(metatype);
			if (it_meta == it_type.value().end()) {
				if (it_type.value().size() == 1 && it_type.value().begin().key() == 0)
					return it_type.value().begin().value();
			}
			else
				return it_meta.value();
		}
		return vipNullHandle();
	}

	bool hasStandardArrayType(int metatype)
	{
		QMutexLocker lock(&tables_mutex());
		auto it = tables().find(VipNDArrayHandle::Standard);
		if (it != tables().end()) {
			auto found = it.value().find(metatype);
			return found != it.value().end();
		}
		return false;
	}
}

VipNDArrayHandle* vipNullHandlePtr() noexcept
{
	static detail::NullHandle handle;
	return &handle;
}

VipSharedHandle vipNullHandle() noexcept
{
	static VipSharedHandle h = VipSharedHandle(vipNullHandlePtr());
	return h;
}

int vipRegisterArrayType(int handle_type, int metaType, const VipSharedHandle& handle)
{
	QMutexLocker lock(&tables_mutex());
	tables()[handle_type][metaType] = handle;
	return 0;
}

VipSharedHandle vipCreateArrayHandle(int handle_type, int metaType, const VipNDArrayShape& shape)
{
	VipSharedHandle handle = detail::getHandle(handle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();
	handle.detach();
	if (shape.size())
		handle->realloc(shape);
	return handle;
}

VipSharedHandle vipCreateArrayHandle(int handle_type, int metaType, void* ptr, const VipNDArrayShape& shape, const VipDeleteFunction& del)
{
	VipSharedHandle handle = detail::getHandle(handle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();
	handle.detach();
	handle->opaque = ptr;
	handle->shape = shape;
	handle->size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, handle->strides);
	handle->deleter = del;
	return handle;
}

VipSharedHandle vipCreateArrayHandle(int handle_type, int metaType)
{
	VipSharedHandle handle = detail::getHandle(handle_type, metaType);
	if (handle->handleType() == VipNDArrayHandle::Null)
		return vipNullHandle();

	handle.detach();
	return handle;
}
