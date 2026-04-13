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

#include "VipMultiNDArray.h"
#include <qdatastream.h>
#include <typeinfo>

static bool isVipMultiNDArrayHandle(const VipSharedHandle& h)
{
	return h && h->handleType() == VipNDArrayHandle::MultiArray;
}

bool vipIsMultiNDArray(const VipNDArray& ar)
{
	return isVipMultiNDArrayHandle(ar.sharedHandle());
}

namespace detail
{
	VipNDArray* MultiNDArrayHandle::null_array()
	{
		static VipNDArray null;
		return &null;
	}

	MultiNDArrayHandle::MultiNDArrayHandle(const MultiNDArrayHandle& h)
	  : VipNDArrayHandle()
	{
		arrays = h.arrays;
		current = h.current;
		setCurrentArray(current);
	}

	void MultiNDArrayHandle::setCurrentArray(const QString& name)
	{
		QMap<QString, VipNDArray>::iterator it = arrays.find(name);
		if (it != arrays.end()) {
			const VipNDArray& ar = it.value();
			if (&ar != currentArray) {
				this->opaque = ar.handle()->opaque; // const_cast<void*>(ar.data());
				this->shape = ar.shape();
				this->size = ar.size();
				this->strides = ar.strides();
				currentArray = const_cast<VipNDArray*>(&ar);
			}
			current = name;
		}
		else {
			this->opaque = nullptr;
			this->shape.clear();
			this->size = 0;
			this->strides.clear();
			currentArray = null_array();
			current.clear();
		}
	}

	bool MultiNDArrayHandle::realloc(const VipNDArrayShape& sh)
	{
		return currentArray->handle()->realloc(sh);
	}
	bool MultiNDArrayHandle::reshape(const VipNDArrayShape& sh)
	{
		return currentArray->handle()->reshape(sh);
	}

	bool MultiNDArrayHandle::importData(const VipNDArrayShape& this_shape,
						  const VipNDArrayShape& this_start,
						  const VipNDArrayHandle* src,
						  const VipNDArrayShape& src_shape,
						  const VipNDArrayShape& src_start)
	{
		return currentArray->handle()->importData(this_shape, this_start, src, src_shape, src_start);
	}

	bool MultiNDArrayHandle::fill(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, const QVariant& v)
	{
		return currentArray->handle()->fill(_start, _shape, v);
	}

	void MultiNDArrayHandle::fromVariant(const VipNDArrayShape& sh, const QVariant& v)
	{
		return currentArray->handle()->fromVariant(sh, v);
	}

	void MultiNDArrayHandle::addArray(const QString& name, const VipNDArray& ar)
	{
		arrays[name] = ar;
		if (current.isEmpty() || current == name)
			setCurrentArray(name);
	}

	void MultiNDArrayHandle::removeArray(const QString& name)
	{
		QMap<QString, VipNDArray>::iterator it = arrays.find(name);
		if (it != arrays.end()) {
			arrays.erase(it);
			if (current == name) {
				if (arrays.size())
					setCurrentArray(arrays.begin().key());
				else
					setCurrentArray(QString());
			}
		}
	}

	QDataStream& MultiNDArrayHandle::ostream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QDataStream& o) const
	{
		o << current;
		o << arrays.size();
		for (QMap<QString, VipNDArray>::const_iterator it = arrays.begin(); it != arrays.end(); ++it) {
			o << it.key() << it.value().mid(_start, _shape);
		}
		return o << arrays;
	}

	QDataStream& MultiNDArrayHandle::istream(const VipNDArrayShape&, const VipNDArrayShape&, QDataStream& i)
	{
		QString c;
		arrays.clear();
		qsizetype _size = 0;
		i >> c >> _size;
		for (qsizetype s = 0; s < _size; ++s) {
			QString name;
			VipNDArray ar;
			i >> name >> ar;
			arrays[name] = ar;
		}
		setCurrentArray(c);
		return i;
	}

}

// register the handle type
int reg = vipRegisterArrayType(VipNDArrayHandle::MultiArray, 0, VipSharedHandle(new detail::MultiNDArrayHandle()));

/// Default constructor
VipMultiNDArray::VipMultiNDArray()
  : VipNDArray(VipSharedHandle(new detail::MultiNDArrayHandle()))
{
}

/// Construct from a VipNDArray
VipMultiNDArray::VipMultiNDArray(const VipNDArray& ar)
  : VipNDArray(vipIsMultiNDArray(ar) ? ar.sharedHandle() : VipSharedHandle(new detail::MultiNDArrayHandle()))
{
}

detail::MultiNDArrayHandle* VipMultiNDArray::handle()
{
	return static_cast<detail::MultiNDArrayHandle*>(this->VipNDArray::handle());
}

const detail::MultiNDArrayHandle* VipMultiNDArray::handle() const
{
	return static_cast<const detail::MultiNDArrayHandle*>(this->VipNDArray::handle());
}

/// Copy operator
VipMultiNDArray& VipMultiNDArray::operator=(const VipNDArray& other)
{
	setSharedHandle(other.sharedHandle());
	return *this;
}

/// Reimplemented from #VipNDArray::setSharedHandle.
///  Setting a null handle with vipNullHandle() will remove all arrays
void VipMultiNDArray::setSharedHandle(const VipSharedHandle& other)
{
	if (isVipMultiNDArrayHandle(other)) {
		VipNDArray::setSharedHandle(other);
		return;
	}
	else if (other == vipNullHandle()) {
		handle()->arrays.clear();
		handle()->current.clear();
		handle()->setCurrentArray({});
	}
}

bool VipMultiNDArray::addArray(const QString& name, const VipNDArray& array)
{
	if (array.isNull())
		return false;
	handle()->addArray(name, array);
	return true;
}

void VipMultiNDArray::removeArray(const QString& name)
{
	handle()->removeArray(name);
}

qsizetype VipMultiNDArray::arrayCount() const noexcept
{
	return handle()->arrays.size();
}
QStringList VipMultiNDArray::arrayNames() const
{
	return handle()->arrays.keys();
}
QList<VipNDArray> VipMultiNDArray::arrays() const
{
	return handle()->arrays.values();
}
const VipNDArray& VipMultiNDArray::array(const QString& name) const noexcept
{
	QMap<QString, VipNDArray>::const_iterator it = handle()->arrays.find(name);
	if (it != handle()->arrays.end())
		return it.value();
	else
		return *detail::MultiNDArrayHandle::null_array();
}
const QMap<QString, VipNDArray>& VipMultiNDArray::namedArrays() const noexcept
{
	return handle()->arrays;
}
void VipMultiNDArray::setNamedArrays(const QMap<QString, VipNDArray>& ars)
{
	handle()->arrays.clear();
	for (QMap<QString, VipNDArray>::const_iterator it = ars.begin(); it != ars.end(); ++it)
		handle()->addArray(it.key(), it.value());
}
void VipMultiNDArray::setCurrentArray(const QString& name)
{
	handle()->setCurrentArray(name);
}
const QString& VipMultiNDArray::currentArrayName() const noexcept
{
	return handle()->current;
}

const VipNDArray& VipMultiNDArray::currentArray() const noexcept
{
	return *handle()->currentArray;
}
