#include "VipMultiNDArray.h"
#include <typeinfo>
#include <qdatastream.h>


static bool isVipMultiNDArrayHandle(const SharedHandle & h)
{
	return typeid(*h) == typeid(detail::MultiNDArrayHandle&);
}

bool vipIsMultiNDArray(const VipNDArray & ar)
{
	return isVipMultiNDArrayHandle(ar.sharedHandle());
}



namespace detail
{

	MultiNDArrayHandle::MultiNDArrayHandle()
		:VipNDArrayHandle(), currentHandle(vipNullHandle())
	{
	}

	MultiNDArrayHandle::MultiNDArrayHandle(const MultiNDArrayHandle & h)
		: VipNDArrayHandle()
	{
		arrays = h.arrays;
		current = h.current;
		//currentHandle = h.currentHandle;
		setCurrentArray(current);
	}

	void MultiNDArrayHandle::setCurrentArray(const QString& name)
	{
		QMap<QString, VipNDArray>::iterator it = arrays.find(name);
		if (it != arrays.end())
		{
			const VipNDArray & ar = it.value();
			if (ar.sharedHandle() != currentHandle)
			{
				this->opaque = ar.handle()->opaque;//const_cast<void*>(ar.data());
				this->shape = ar.shape();
				this->size = ar.size();
				this->strides = ar.strides();
				currentHandle = ar.sharedHandle();
			}
			current = name;
		}
		else
		{
			this->opaque = NULL;
			this->shape.clear();
			this->size = 0;
			this->strides.clear();
			currentHandle = vipNullHandle();
			current.clear();
		}
	}

	void MultiNDArrayHandle::addArray(const QString & name, const VipNDArray & ar)
	{
		arrays[name] = ar;
		if (current.isEmpty() || current == name)
			setCurrentArray(name);
	}

	void MultiNDArrayHandle::removeArray(const QString & name)
	{
		QMap<QString, VipNDArray>::iterator it = arrays.find(name);
		if (it != arrays.end())
		{
			arrays.erase(it);
			if (current == name)
			{
				if (arrays.size())
					setCurrentArray(arrays.begin().key());
				else
					setCurrentArray(QString());
			}
		}
	}


	QDataStream & MultiNDArrayHandle::ostream(const VipNDArrayShape & _start, const VipNDArrayShape & _shape, QDataStream & o) const
	{
		o << current;
		o << arrays.size();
		for (QMap<QString, VipNDArray>::const_iterator it = arrays.begin(); it != arrays.end(); ++it)
		{
			o << it.key() << it.value().mid(_start, _shape);
		}
		return o << arrays;
	}

	QDataStream & MultiNDArrayHandle::istream(const VipNDArrayShape &, const VipNDArrayShape &, QDataStream & i)
	{
		QString c;
		arrays.clear();
		int _size = 0;
		i >> c >> _size;
		for (int s = 0; s < _size; ++s)
		{
			QString name;
			VipNDArray ar;
			i >> name >> ar;
			arrays[name] = ar;
		}
		setCurrentArray(c);
		return i;
	}

}

//register the handle type
int reg = vipRegisterArrayType(VipNDArrayHandle::MultiArray, 0, SharedHandle(new detail::MultiNDArrayHandle()));



///Default constructor
VipMultiNDArray::VipMultiNDArray()
	:VipNDArray(SharedHandle(new detail::MultiNDArrayHandle()))
{}

///Construct from a VipNDArray
VipMultiNDArray::VipMultiNDArray(const VipNDArray & ar)
	:VipNDArray(SharedHandle(new detail::MultiNDArrayHandle()))
{
	VipMultiNDArray::operator=(ar);
}

detail::MultiNDArrayHandle * VipMultiNDArray::handle()
{
	return static_cast<detail::MultiNDArrayHandle*>(this->VipNDArray::handle());
}

const detail::MultiNDArrayHandle * VipMultiNDArray::handle() const
{
	return static_cast<const detail::MultiNDArrayHandle*>(this->VipNDArray::handle());
}


///Copy operator
VipMultiNDArray & VipMultiNDArray::operator=(const VipNDArray & other)
{
	VipNDArray::operator=(other);
	return *this;
}

///Reimplemented from #VipNDArray::setSharedHandle.
/// Setting a null handle with vipNullHandle() will remove all arrays
void VipMultiNDArray::setSharedHandle(const SharedHandle & other)
{
	if (isVipMultiNDArrayHandle(other))
	{
		VipNDArray::setSharedHandle(other);
		return;
	}

	if (other == vipNullHandle())
	{
		handle()->arrays.clear();
		handle()->current.clear();
	}

	handle()->currentHandle = other;
	handle()->opaque = handle()->currentHandle->opaque;
	handle()->size = handle()->currentHandle->size;
	handle()->shape = handle()->currentHandle->shape;
	handle()->strides = handle()->currentHandle->strides;

}


void VipMultiNDArray::addArray(const QString & name, const VipNDArray & array)
{
	handle()->addArray(name, array);
}

void VipMultiNDArray::removeArray(const QString & name)
{
	handle()->removeArray(name);
}

int VipMultiNDArray::arrayCount() const
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
VipNDArray VipMultiNDArray::array(const QString & name) const
{
	QMap<QString, VipNDArray>::const_iterator it = handle()->arrays.find(name);
	if (it != handle()->arrays.end())
		return it.value();
	else
		return VipNDArray();
}
const QMap<QString, VipNDArray> & VipMultiNDArray::namedArrays() const
{
	return handle()->arrays;
}
void VipMultiNDArray::setNamedArrays(const QMap<QString, VipNDArray> & ars)
{
	handle()->arrays.clear();
	for(QMap<QString, VipNDArray>::const_iterator it = ars.begin(); it != ars.end(); ++it)
		handle()->addArray(it.key(), it.value());
}
void VipMultiNDArray::setCurrentArray(const QString & name)
{
	handle()->setCurrentArray(name);
}
const QString & VipMultiNDArray::currentArray() const
{
	return handle()->current;
}
