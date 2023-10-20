/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_ARRAY_BASE_H
#define VIP_ARRAY_BASE_H

#include <type_traits>

#include <QSharedDataPointer>
#include <QTextStream>

#include "VipConfig.h"
#include "VipHybridVector.h"
#include "VipInternalConvert.h"
#include "VipIterator.h"
#include "VipLongDouble.h"
#include "VipRgb.h"
#include "VipUtils.h"

namespace Vip
{
	/// Interpolation type for #VipNDArray::resize function and #vipTransform
	enum InterpolationType
	{
		NoInterpolation,     //! No interpolation
		LinearInterpolation, //! Linear interpolation
		CubicInterpolation   //! Cubic interpolation
	};

	/// Data access type for VipNDArray inheriting classes and functors
	enum AccessType
	{
		Flat = 0x01,	 //! access by flat position with operator[](int index)
		Position = 0x02, //! access by N-D position with operator()(const VectorType & pos)
		Cwise = 0x04	 //! coefficient wise access supporting multi-threading
	};
}

/// Custom deleter for VipNDArrayHandle
typedef std::function<void(void*)> vip_deleter_type;

namespace detail
{

	/// Very base class for all ND arrays and functor expressions
	struct NullOperand
	{
		typedef NullType value_type;
		static constexpr int access_type = 0;
		static constexpr bool valid = true;
		int dataType() const;
		const VipNDArrayShape& shape() const;
		bool isEmpty() const;
		bool isUnstrided() const;

		// internal use only
		void unref() const {}
	};

	/// Base class for functor expressions
	struct BaseExpression : NullOperand
	{
	};

	/// Very base class for reduction algorithms
	struct BaseReductor
	{
	};
}

/// Check whether template parameter is a functor expression
template<class T>
struct VipIsExpression
{
	static const bool value = std::is_base_of<detail::BaseExpression, T>::value;
};

/// Check whether template parameter is a reduction algorithm
template<class T>
struct VipIsReductor
{
	static const bool value = std::is_base_of<detail::BaseReductor, T>::value;
};

struct VipNDArrayHandle;
/// Returns the global NullHandle pointer
VIP_DATA_TYPE_EXPORT VipNDArrayHandle* vipNullHandlePtr();
/// Returns true if given metatype is arithmetic
VIP_DATA_TYPE_EXPORT bool vipIsArithmetic(uint data_type);
/// Returns true if given metatype is a complex number (either complex_d or complex_f)
VIP_DATA_TYPE_EXPORT bool vipIsComplex(uint data_type);
/// Returns true if metatype \a from can be converted to \a to using the internal cast function
VIP_DATA_TYPE_EXPORT bool vipCanConvertStdTypes(uint from, uint to);
/// Returns true if metatype \a from can be converted to \a to based on QVariant::canConvert
VIP_DATA_TYPE_EXPORT bool vipCanConvert(uint from, uint to);

/// VipNDArrayHandle is the base structure used by #VipNDArray, and provides an abstract interface to manipulate N-dimensions arrays.
/// Since #VipNDArray uses Copy On Write (COW), VipNDArrayHandle is a shared data structure.
struct VIP_DATA_TYPE_EXPORT VipNDArrayHandle : public QSharedData
{
	/// The VipNDArrayHandle type, as returned by #VipNDArrayHandle::handleType.
	/// We use high values to avoid overlappings with Qt meta type ids.
	enum HandleType
	{
		Null = 10000,
		Standard,
		MultiArray,
		Image,
		View,
		User = 10010,
	};

	// The array shape
	VipNDArrayShape shape;
	// the array strides
	VipNDArrayShape strides;
	// the array flat size
	int size;
	// the array data
	void* opaque;
	// custom deleter function, only used with StdHandle
	vip_deleter_type deleter;

	VipNDArrayHandle();
	/// Destructor.
	///  Should destroy the underlying opaque data (if any)
	virtual ~VipNDArrayHandle();

	/// Returns the handle type (one of HandleType enum)
	virtual int handleType() const = 0;
	/// Returns a copy of this handle, copying the internal data
	virtual VipNDArrayHandle* copy() const = 0;
	/// Returns the data pointer at given position.
	///  This function should only return a non nullptr value if the array is based on a dynamic flat array.
	virtual void* dataPointer(const VipNDArrayShape&) const = 0;
	/// Realloc the data with a new shape
	virtual bool realloc(const VipNDArrayShape&) = 0;
	/// Change the shape of the array without touching to the data
	virtual bool reshape(const VipNDArrayShape& new_shape) = 0;

	/// Retruns the new opaque value for given pos. With standard dense array this is equivalent to advancing to data pointer by an offset.
	virtual void* opaqueForPos(void* op, const VipNDArrayShape& // pos
	) const
	{
		return op;
	}

	/// Resize a subpart of the array in a destination handle using given interpolation type
	virtual bool resize(const VipNDArrayShape& start,
			    const VipNDArrayShape& shape,
			    VipNDArrayHandle* dst,
			    Vip::InterpolationType,
			    const VipNDArrayShape& out_start,
			    const VipNDArrayShape& out_shape) const = 0;
	/// Returns the data type name (like "int" or "double")
	virtual const char* dataName() const = 0;
	/// Returns the pixel data size (returns 4 for integer 32bits, 8 for double type, etc.)
	virtual int dataSize() const = 0;
	/// Returns the pixel data type (like QMetaType::Int or qMetaTypeId<complex_f>())
	virtual int dataType() const = 0;

	/// Returns true if this handle can export its data into an array of given pixel type.
	virtual bool canExport(int type) const = 0;
	/// Returns true if this handle can import into its data from an array of given pixel type.
	virtual bool canImport(int type) const = 0;

	/// Export this handle data into another one.
	///  \param this_shape shape of the data block to export
	///  \param this_start start position of the data block to export
	///  \param dst destination handle
	///  \param dst_shape destination block shape
	///  \param dst_start destination block start position
	///  \return true on success, false otherwise
	virtual bool exportData(const VipNDArrayShape& this_start,
				const VipNDArrayShape& this_shape,
				VipNDArrayHandle* dst,
				const VipNDArrayShape& dst_start,
				const VipNDArrayShape& dst_shape) const = 0;

	/// Import a block of data from another handle.
	///  \param this_shape shape of the destination data block
	///  \param this_start start position of the destination data block
	///  \param src source handle
	///  \param src_shape source block shape
	///  \param src_start source block start position
	///  \return true on success, false otherwise
	virtual bool importData(const VipNDArrayShape& this_start,
				const VipNDArrayShape& this_shape,
				const VipNDArrayHandle* src,
				const VipNDArrayShape& src_shape,
				const VipNDArrayShape& src_start) = 0;

	/// Fill a subpart of the array with given value.
	///  You do not need to check boundaries.
	///  Returns true on success, false otherwise
	virtual bool fill(const VipNDArrayShape& start, const VipNDArrayShape& shape, const QVariant&) = 0;
	/// Returns the pixel data at given position
	virtual QVariant toVariant(const VipNDArrayShape& sh) const = 0;
	/// Set the pixel data at given position
	virtual void fromVariant(const VipNDArrayShape& sh, const QVariant& val) = 0;
	/// Save a subpart of the data in a QDataStream.
	///  Only the raw data needs to be saved, not the shape or strides.
	///  You do not need to check boundaries.
	virtual QDataStream& ostream(const VipNDArrayShape& start, const VipNDArrayShape& shape, QDataStream&) const = 0;
	/// Import a subpart of the data from a QDataStream
	/// The array shape, strides and size are supposed to be valid when this function is called, and the data already allocated.
	/// You do not need to check boundaries.
	virtual QDataStream& istream(const VipNDArrayShape& start, const VipNDArrayShape& shape, QDataStream&) = 0;
	/// Export a block of data into a QTextStream with given separator between each pixel data.
	/// You do not need to check boundaries.
	virtual QTextStream& oTextStream(const VipNDArrayShape& start, const VipNDArrayShape& shape, QTextStream& stream, const QString& separator) const = 0;
};

namespace detail
{

	/// A null #VipNDArrayHandle, used for empty #VipNDArray.
	struct NullHandle : public VipNDArrayHandle
	{
		NullHandle() {}
		NullHandle(const NullHandle&) {}
		virtual NullHandle* copy() const { return (NullHandle*)vipNullHandlePtr(); }
		virtual void* dataPointer(const VipNDArrayShape&) const { return nullptr; }
		virtual int handleType() const { return Null; }
		virtual bool reshape(const VipNDArrayShape&) { return false; }
		virtual bool realloc(const VipNDArrayShape&) { return false; }
		virtual bool resize(const VipNDArrayShape&, const VipNDArrayShape&, VipNDArrayHandle*, Vip::InterpolationType, const VipNDArrayShape&, const VipNDArrayShape&) const { return false; }
		virtual const char* dataName() const { return nullptr; }
		virtual int dataSize() const { return 1; }
		virtual int dataType() const { return 0; }
		virtual bool canExport(int) const { return false; }
		virtual bool canImport(int) const { return false; }
		virtual bool exportData(const VipNDArrayShape&, const VipNDArrayShape&, VipNDArrayHandle*, const VipNDArrayShape&, const VipNDArrayShape&) const { return false; }
		virtual bool importData(const VipNDArrayShape&, const VipNDArrayShape&, const VipNDArrayHandle*, const VipNDArrayShape&, const VipNDArrayShape&) { return false; }
		virtual bool fill(const VipNDArrayShape&, const VipNDArrayShape&, const QVariant&) { return false; }
		virtual QVariant toVariant(const VipNDArrayShape&) const { return QVariant(); }
		virtual void fromVariant(const VipNDArrayShape&, const QVariant&) {}
		virtual QDataStream& ostream(const VipNDArrayShape&, const VipNDArrayShape&, QDataStream& o) const { return o; }
		virtual QDataStream& istream(const VipNDArrayShape&, const VipNDArrayShape&, QDataStream& i) { return i; }
		virtual QTextStream& oTextStream(const VipNDArrayShape&, const VipNDArrayShape&, QTextStream& stream, const QString&) const { return stream; }
	};

	/// A shared pointer class using Copy On Write, very close to QSharedDataPointer.
	///  The managed type must provide the copy() member function.
	template<class U>
	class SharedDataPointer
	{
		U* d;

	public:
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
		VIP_ALWAYS_INLINE void detach()
		{
			if (d->ref.load() != 1)
				detach_helper();
		}
#else
		VIP_ALWAYS_INLINE void detach()
		{
			if (d->ref.loadRelaxed() != 1)
				detach_helper();
		}
#endif
		VIP_ALWAYS_INLINE U& operator*()
		{
			detach();
			return *d;
		}
		VIP_ALWAYS_INLINE const U& operator*() const noexcept
		{
			return *d;
		}
		VIP_ALWAYS_INLINE U* operator->()
		{
			detach();
			return d;
		}
		VIP_ALWAYS_INLINE const U* operator->() const noexcept
		{
			return d;
		}
		VIP_ALWAYS_INLINE operator U*()
		{
			detach();
			return d;
		}
		VIP_ALWAYS_INLINE operator const U*() const noexcept
		{
			return d;
		}
		VIP_ALWAYS_INLINE U* data()
		{
			detach();
			return d;
		}
		VIP_ALWAYS_INLINE const U* data() const noexcept
		{
			return d;
		}
		VIP_ALWAYS_INLINE const U* constData() const noexcept
		{
			return d;
		}
		VIP_ALWAYS_INLINE bool unique() const noexcept
		{
			return static_cast<int>(d->ref) == 1;
		}

		VIP_ALWAYS_INLINE bool operator==(const SharedDataPointer& other) const noexcept
		{
			return d == other.d;
		}
		VIP_ALWAYS_INLINE bool operator!=(const SharedDataPointer& other) const noexcept
		{
			return d != other.d;
		}

		inline SharedDataPointer()
		  : d(nullptr)
		{
		}
		inline ~SharedDataPointer()
		{
			if (d && d->handleType() != VipNDArrayHandle::Null) {
				if (!d->ref.deref())
					delete d;
			}
		}

		template<class T>
		explicit SharedDataPointer(T* data) noexcept
		  : d(data)
		{
			if (d)
				d->ref.ref();
		}
		inline SharedDataPointer(const SharedDataPointer& o) noexcept
		  : d(o.d)
		{
			if (d)
				d->ref.ref();
		}
		inline SharedDataPointer(SharedDataPointer&& o) noexcept
		  : d(o.d)
		{
			o.d = vipNullHandlePtr();
		}
		inline SharedDataPointer& operator=(const SharedDataPointer& o)
		{
			if (o.d != d) {
				if (o.d)
					o.d->ref.ref();
				U* old = d;
				d = o.d;
				if (old && old->handleType() != VipNDArrayHandle::Null && !old->ref.deref())
					delete old;
			}
			return *this;
		}
		inline SharedDataPointer& operator=(SharedDataPointer&& o) noexcept
		{
			std::swap(d, o.d);
			return *this;
		}

		inline bool operator!() const noexcept
		{
			return !d;
		}

		inline void swap(SharedDataPointer& other) noexcept
		{
			std::swap(d, other.d);
		}

	private:
		void detach_helper()
		{
			if (d->handleType() != VipNDArrayHandle::Null) {
				U* x = d->copy();
				x->ref.ref();
				if (!d->ref.deref())
					delete d;
				d = x;
			}
		}
	};
}

/// Shared pointer type of #VipNDArrayHandle using COW
typedef detail::SharedDataPointer<VipNDArrayHandle> SharedHandle;

namespace detail
{

	// helper struct
	// special treatment for char type which is not handled properly by QDataStream
	template<class U>
	struct Ostream
	{
		QDataStream* stream;
		U operator()(const U& value) const
		{
			(*stream) << value;
			return value;
		}
	};
	template<>
	struct Ostream<char>
	{
		QDataStream* stream;
		char operator()(char value) const
		{
			(*stream) << (qint8)value;
			return value;
		}
	};
	template<>
	struct Ostream<long>
	{
		QDataStream* stream;
		long operator()(long value) const
		{
			(*stream) << (qint32)value;
			return value;
		}
	};
	template<>
	struct Ostream<unsigned long>
	{
		QDataStream* stream;
		unsigned long operator()(unsigned long value) const
		{
			(*stream) << (quint32)value;
			return value;
		}
	};

	template<class U>
	struct Istream
	{
		QDataStream* stream;
		U operator()(const U&) const
		{
			U tmp;
			(*stream) >> tmp;
			return tmp;
		}
	};
	template<>
	struct Istream<char>
	{
		QDataStream* stream;
		char operator()(const char&) const
		{
			qint8 tmp;
			(*stream) >> tmp;
			return tmp;
		}
	};
	template<>
	struct Istream<long>
	{
		QDataStream* stream;
		long operator()(const long&) const
		{
			int tmp;
			(*stream) >> tmp;
			return tmp;
		}
	};
	template<>
	struct Istream<unsigned long>
	{
		QDataStream* stream;
		unsigned long operator()(const unsigned long&) const
		{
			quint32 tmp;
			(*stream) >> tmp;
			return tmp;
		}
	};

	template<>
	struct Istream<long double>
	{
		QDataStream* stream;
		int LD_support;
		long double operator()(const long double&) const { return vipReadLELongDouble(LD_support, *stream); }
	};

	/// The standard #VipNDArrayHandle for dense array
	template<class T>
	struct StdHandle : public VipNDArrayHandle
	{
		bool own; // tells if the data pointer should be deleted

		void deleteInternal()
		{
			if (opaque && own) {
				if (deleter)
					deleter(opaque);
				else
					delete[] static_cast<T*>(opaque);
			}
			opaque = nullptr;
		}

		StdHandle()
		  : own(true)
		{
		}
		StdHandle(const StdHandle& other)
		  : own(true)
		{
			shape = other.shape;
			strides = other.strides;
			size = other.size;
			opaque = other.opaque;
			if (size) {
				opaque = new T[size];
				std::copy(static_cast<const T*>(other.opaque), static_cast<const T*>(other.opaque) + size, static_cast<T*>(opaque));
			}
		}

		virtual StdHandle* copy() const { return new StdHandle(*this); }

		virtual void* dataPointer(const VipNDArrayShape& pos) const { return pos.isEmpty() ? (T*)opaque : (T*)opaque + vipFlatOffset<false>(strides, pos); }

		virtual ~StdHandle() { deleteInternal(); }

		virtual int handleType() const { return Standard; }

		virtual bool reshape(const VipNDArrayShape& new_shape)
		{
			shape = new_shape;
			size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides);
			return true;
		}

		virtual bool realloc(const VipNDArrayShape& new_shape)
		{
			if (opaque) {
				deleteInternal();
			}

			shape = new_shape;
			size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides);
			if (size)
				opaque = new T[size];
			// remove deleter
			deleter = vip_deleter_type();
			return true;
		}
		virtual void* opaqueForPos(void* op, const VipNDArrayShape& pos) const { return static_cast<T*>(op) + vipFlatOffset<false>(strides, pos); }
		virtual bool resize(const VipNDArrayShape& start,
				    const VipNDArrayShape& shape,
				    VipNDArrayHandle* dst,
				    Vip::InterpolationType type,
				    const VipNDArrayShape& out_start,
				    const VipNDArrayShape& out_shape) const;
		virtual const char* dataName() const { return QMetaType::typeName(qMetaTypeId<T>()); }
		virtual int dataSize() const { return sizeof(T); }
		virtual int dataType() const { return qMetaTypeId<T>(); }
		virtual bool canExport(int data_type) const { return vipCanConvertStdTypes(qMetaTypeId<T>(), data_type) || vipCanConvert(qMetaTypeId<T>(), data_type); }
		virtual bool canImport(int data_type) const { return vipCanConvertStdTypes(data_type, qMetaTypeId<T>()) || vipCanConvert(data_type, qMetaTypeId<T>()); }

		virtual bool exportData(const VipNDArrayShape& this_start,
					const VipNDArrayShape& this_shape,
					VipNDArrayHandle* dst,
					const VipNDArrayShape& dst_start,
					const VipNDArrayShape& dst_shape) const
		{
			if (!dst->opaque || !opaque)
				return false;
			if (!vipCompareShapeSize(this_shape, dst_shape))
				return false;

			int this_type = qMetaTypeId<T>();
			int other_type = dst->dataType();
			if (other_type == this_type) {
				return vipArrayTransform(static_cast<const T*>(opaque) + vipFlatOffset<false>(strides, this_start),
							 this_shape,
							 strides,
							 static_cast<T*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start),
							 dst_shape,
							 dst->strides,
							 VipNullTransform());
			}

			if (vipCanConvertStdTypes(this_type, other_type)) // conversion of "standard" types (numeric, QString, QByteArray, complex)
				return convert(static_cast<const T*>(opaque) + vipFlatOffset<false>(strides, this_start),
					       this_type,
					       this_shape,
					       strides,
					       static_cast<quint8*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start) * dst->dataSize(),
					       other_type,
					       dst_shape,
					       dst->strides);
			else // any other conversion supported by the meta type system
				return vipArrayTransformVoid(static_cast<const T*>(opaque) + vipFlatOffset<false>(strides, this_start),
							     this_type,
							     this_shape,
							     strides,
							     static_cast<quint8*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start) * dst->dataSize(),
							     other_type,
							     dst_shape,
							     dst->strides);
		}

		virtual bool importData(const VipNDArrayShape& this_start,
					const VipNDArrayShape& this_shape,
					const VipNDArrayHandle* src,
					const VipNDArrayShape& src_start,
					const VipNDArrayShape& src_shape)
		{
			if (!src->opaque || !opaque)
				return false;
			if (!vipCompareShapeSize(this_shape, src_shape))
				return false;

			int this_type = qMetaTypeId<T>();
			int other_type = src->dataType();
			if (other_type == this_type) {
				return vipArrayTransform(static_cast<const T*>(src->opaque) + vipFlatOffset<false>(src->strides, src_start),
							 src_shape,
							 src->strides,
							 static_cast<T*>(opaque) + vipFlatOffset<false>(strides, this_start),
							 this_shape,
							 strides,
							 VipNullTransform());
			}

			if (vipCanConvertStdTypes(other_type, this_type)) // conversion of "standard" types (numeric, QString, QByteArray, complex)
				return convert(static_cast<const quint8*>(src->opaque) + vipFlatOffset<false>(src->strides, src_start) * src->dataSize(),
					       other_type,
					       src_shape,
					       src->strides,
					       static_cast<T*>(opaque) + vipFlatOffset<false>(strides, this_start),
					       this_type,
					       this_shape,
					       strides);
			else
				return vipArrayTransformVoid(static_cast<const quint8*>(src->opaque) + vipFlatOffset<false>(src->strides, src_start) * src->dataSize(),
							     other_type,
							     src_shape,
							     src->strides,
							     static_cast<T*>(opaque) + vipFlatOffset<false>(strides, this_start),
							     this_type,
							     this_shape,
							     strides);
		}

		virtual bool fill(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, const QVariant& value)
		{
			if (!value.canConvert<T>())
				return false;

			vipInplaceArrayTransform(static_cast<T*>(opaque) + vipFlatOffset<false>(strides, _start), _shape, strides, VipFillTransform<T>(value.value<T>()));
			return true;
		}

		T* ptr(const VipNDArrayShape& sh) { return ((T*)opaque) + vipFlatOffset<false>(strides, sh); }

		virtual QVariant toVariant(const VipNDArrayShape& sh) const { return QVariant::fromValue(*const_cast<StdHandle*>(this)->ptr(sh)); }

		virtual void fromVariant(const VipNDArrayShape& sh, const QVariant& val) { this->ptr(sh)[0] = val.value<T>(); }

		virtual QDataStream& ostream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QDataStream& o) const
		{
			Ostream<T> out;
			out.stream = &o;
			// be carefull of the storage of float/double data type
			if VIP_CONSTEXPR (std::is_same<double, T>::value || std::is_same<complex_d, T>::value)
				o.setFloatingPointPrecision(QDataStream::DoublePrecision);
			else
				o.setFloatingPointPrecision(QDataStream::SinglePrecision);
			vipInplaceArrayTransform(static_cast<T*>(opaque) + vipFlatOffset<false>(strides, _start), _shape, strides, out);
			return o;
		}

		virtual QDataStream& istream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QDataStream& i)
		{
			if (std::is_same<long double, T>::value) {
				// long double is a special case
				Istream<long double> in;
				in.stream = &i;
				in.LD_support = i.device()->property("_vip_LD").toUInt();
				i.setFloatingPointPrecision(QDataStream::DoublePrecision);
				vipInplaceArrayTransform(static_cast<long double*>(opaque) + vipFlatOffset<false>(strides, _start), _shape, strides, in);
			}
			else {
				Istream<T> in;
				in.stream = &i;
				// be carefull of the storage of float/double data type
				if VIP_CONSTEXPR (std::is_same<double, T>::value || std::is_same<complex_d, T>::value)
					i.setFloatingPointPrecision(QDataStream::DoublePrecision);
				else
					i.setFloatingPointPrecision(QDataStream::SinglePrecision);
				vipInplaceArrayTransform(static_cast<T*>(opaque) + vipFlatOffset<false>(strides, _start), _shape, strides, in);
			}
			return i;
		}

		virtual QTextStream& oTextStream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QTextStream& stream, const QString& separator) const
		{
			struct Otextstream
			{
				QTextStream* stream;
				QString sep;
				T operator()(const T& value) const
				{
					(*stream) << value << sep;
					return value;
				}
			};
			Otextstream ou;
			ou.stream = &stream;
			ou.sep = separator;
			vipInplaceArrayTransform(static_cast<T*>(opaque) + vipFlatOffset<false>(strides, _start), _shape, strides, ou);

			// VipNDSubArrayIterator<T> begin(shape, strides, static_cast<T*>(opaque) + vipFlatOffset<false>(strides, start));
			//  VipNDSubArrayIterator<T> end(shape, strides, static_cast<T*>(opaque) + vipFlatOffset<false>(strides, start), false);
			//  for (; begin != end; ++begin)
			//  stream << *begin << separator;
			// if(shape.size() == 1) {
			//  for(int x=start[0]; x < start[0] + shape[0]; ++x)
			//  stream<< *(static_cast<T*>(opaque) + vipFlatOffset<false>(strides, vipVector(x))) << separator;
			//  }
			//  else if (shape.size() == 2) {
			//  for(int y = start[0]; y < start[0] + shape[0]; ++y)
			//  for (int x = start[1]; x < start[1] + shape[1]; ++x)
			//	stream << *(static_cast<T*>(opaque) + vipFlatOffset<false>(strides, vipVector(y,x))) << separator;
			//  }
			return stream;
		}
	};

	VIP_DATA_TYPE_EXPORT SharedHandle getHandle(int handle_type, int metatype);
}

/// Returns a NullHandle
VIP_DATA_TYPE_EXPORT SharedHandle vipNullHandle();
/// Register a new #SharedHandle class for given type, or a NullHandle if the type was not registered.
VIP_DATA_TYPE_EXPORT int vipRegisterArrayType(int handleType, int metaType, const SharedHandle& handle);
/// Returns a SharedHandle object already allocated for given type, or a NullHandle if the type was not registered.
VIP_DATA_TYPE_EXPORT SharedHandle vipCreateArrayHandle(int handleType, int metaType, const VipNDArrayShape& shape);
/// Returns a SharedHandle for given type with the fields \a opaque, \a shape, \a size and \ strides already set , or a NullHandle if the type was not registered.
VIP_DATA_TYPE_EXPORT SharedHandle vipCreateArrayHandle(int handleType, int metaType, void* ptr, const VipNDArrayShape& shape, const vip_deleter_type& del);
/// Returns a SharedHandle for given type, or a NullHandle if the type was not registered.
VIP_DATA_TYPE_EXPORT SharedHandle vipCreateArrayHandle(int handleType, int metaType);

/// Returns a SharedHandle for given type.Register it if it has not been registered yet.
template<class T>
SharedHandle vipCreateArrayHandle()
{
	SharedHandle handle = vipCreateArrayHandle(VipNDArrayHandle::Standard, qMetaTypeId<T>());
	if (handle == vipNullHandle()) {
		vipRegisterArrayType(VipNDArrayHandle::Standard, qMetaTypeId<T>(), SharedHandle(new detail::StdHandle<T>()));
		handle = vipCreateArrayHandle(VipNDArrayHandle::Standard, qMetaTypeId<T>());
	}
	return handle;
}

/// Returns an already allocated SharedHandle for given type. Register it if it has not been registered yet.
template<class T>
SharedHandle vipCreateArrayHandle(const VipNDArrayShape& shape)
{
	SharedHandle handle = vipCreateArrayHandle(VipNDArrayHandle::Standard, qMetaTypeId<T>(), shape);
	if (handle == vipNullHandle()) {
		vipRegisterArrayType(VipNDArrayHandle::Standard, qMetaTypeId<T>(), SharedHandle(new detail::StdHandle<T>()));
		handle = vipCreateArrayHandle(VipNDArrayHandle::Standard, qMetaTypeId<T>(), shape);
	}
	return handle;
}

/// Returns a SharedHandle for given type with the fields \a opaque, \a shape, \a size already set. Register it if it has not been registered yet.
template<class T>
SharedHandle vipCreateArrayHandle(T* ptr, const VipNDArrayShape& shape, const vip_deleter_type& del)
{
	SharedHandle handle = ::vipCreateArrayHandle(VipNDArrayHandle::Standard, qMetaTypeId<T>(), (void*)ptr, shape, del);
	if (handle == vipNullHandle()) {
		vipRegisterArrayType(VipNDArrayHandle::Standard, qMetaTypeId<T>(), SharedHandle(new detail::StdHandle<T>()));
		handle = vipCreateArrayHandle(VipNDArrayHandle::Standard, qMetaTypeId<T>(), (void*)ptr, shape, del);
	}
	return handle;
}

namespace detail
{
	/// A VipNDArrayHandle that provides a view on another VipNDArrayHandle
	struct ViewHandle : public VipNDArrayHandle
	{
		const SharedHandle handle;
		VipNDArrayShape start;
		bool pointerView; // tells if this handle is a pointer view (work on data that do not belong to another VipNDArray)
		void* ptr;	  // pointer to the actual data start for the view

		ViewHandle(const ViewHandle& other)
		  : handle(other.handle)
		  , start(other.start)
		  , pointerView(other.pointerView)
		  , ptr(other.ptr)
		{
			opaque = other.opaque;
			strides = other.strides;
			size = other.size;
			shape = other.shape;
		}
		ViewHandle(const ViewHandle& other, const VipNDArrayShape& st, const VipNDArrayShape& sh)
		  : handle(other.handle)
		  , pointerView(other.pointerView)
		{
			start = other.start + st;
			opaque = other.opaque;
			ptr = other.opaqueForPos(other.handle->opaque, start);
			strides = other.strides;
			size = vipShapeToSize(sh);
			shape = sh;
		}
		ViewHandle(const SharedHandle& other, const VipNDArrayShape& start, const VipNDArrayShape& sh)
		  : handle(other)
		  , start(start)
		  , pointerView(false)
		{
			shape = sh;
			ptr = other->opaqueForPos(other->opaque, start);
			opaque = other->opaque;
			strides = other->strides;
			size = vipShapeToSize(sh);
		}
		ViewHandle(void* ptr, int type, const VipNDArrayShape& sh, const VipNDArrayShape& _strides)
		  : handle(vipCreateArrayHandle(Standard, type, ptr, sh, vip_deleter_type()))
		  , pointerView(true)
		{
			opaque = this->ptr = handle->opaque;
			strides = _strides;
			size = handle->size;
			shape = handle->shape;
			start = VipNDArrayShape(sh.size(), 0);
			if (strides.isEmpty())
				strides = handle->strides;
			else
				const_cast<VipNDArrayHandle*>(handle.data())->strides = strides;
		}

		~ViewHandle()
		{
			if (pointerView) // for pointer view, the handle must not delete its data
				const_cast<VipNDArrayHandle*>(handle.data())->opaque = nullptr;
		}
		virtual ViewHandle* copy() const { return new ViewHandle(*this); }
		virtual void* dataPointer(const VipNDArrayShape& pos) const { return handle->dataPointer(pos + start); }
		virtual bool reshape(const VipNDArrayShape&) { return false; }
		virtual bool realloc(const VipNDArrayShape&) { return false; }
		virtual int handleType() const { return View; }
		virtual void* opaqueForPos(void* op, const VipNDArrayShape& pos) const { return handle->opaqueForPos(op, pos); }
		virtual bool resize(const VipNDArrayShape& _start,
				    const VipNDArrayShape& _shape,
				    VipNDArrayHandle* dst,
				    Vip::InterpolationType type,
				    const VipNDArrayShape& out_start,
				    const VipNDArrayShape& out_shape) const
		{
			return handle->resize(_start + this->start, _shape, dst, type, out_start, out_shape);
		}
		virtual const char* dataName() const { return handle->dataName(); }
		virtual int dataSize() const { return handle->dataSize(); }
		virtual int dataType() const { return handle->dataType(); }
		virtual bool canExport(int type) const { return handle->canExport(type); }
		virtual bool canImport(int type) const { return handle->canImport(type); }
		virtual bool exportData(const VipNDArrayShape& this_start,
					const VipNDArrayShape& this_shape,
					VipNDArrayHandle* dst,
					const VipNDArrayShape& dst_start,
					const VipNDArrayShape& dst_shape) const
		{
			return handle->exportData(this_start + start, this_shape, dst, dst_start, dst_shape);
		}
		virtual bool importData(const VipNDArrayShape& this_start,
					const VipNDArrayShape& this_shape,
					const VipNDArrayHandle* src,
					const VipNDArrayShape& src_start,
					const VipNDArrayShape& src_shape)
		{
			return const_cast<VipNDArrayHandle*>(handle.data())->importData(this_start + start, this_shape, src, src_start, src_shape);
		}

		virtual bool fill(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, const QVariant& value)
		{
			return const_cast<VipNDArrayHandle*>(handle.data())->fill(_start + this->start, _shape, value);
		}
		virtual QVariant toVariant(const VipNDArrayShape& pos) const { return handle->toVariant(pos + start); }
		virtual void fromVariant(const VipNDArrayShape& pos, const QVariant& v) { return const_cast<VipNDArrayHandle*>(handle.data())->fromVariant(pos + start, v); }
		virtual QDataStream& ostream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QDataStream& o) const { return handle->ostream(this->start + _start, _shape, o); }
		virtual QDataStream& istream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QDataStream& i)
		{
			return const_cast<VipNDArrayHandle*>(handle.data())->istream(_start + this->start, _shape, i);
		}
		virtual QTextStream& oTextStream(const VipNDArrayShape& _start, const VipNDArrayShape& _shape, QTextStream& stream, const QString& sep) const
		{
			return handle->oTextStream(this->start + _start, _shape, stream, sep);
		}
	};

}

struct VipNDArrayHandle;
namespace detail
{
	// export this function in VipResize.cpp to avoid VipNDArray object exceeding max object size with mscv
	VIP_DATA_TYPE_EXPORT bool vipResizeArray(const VipNDArrayHandle* src, VipNDArrayHandle* dst, Vip::InterpolationType type);
}

namespace detail
{
	template<class T>
	bool StdHandle<T>::resize(const VipNDArrayShape& _start,
				  const VipNDArrayShape& _shape,
				  VipNDArrayHandle* dst,
				  Vip::InterpolationType type,
				  const VipNDArrayShape& out_start,
				  const VipNDArrayShape& out_shape) const
	{
		if (!opaque || !dst->opaque)
			return false;
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

		StdHandle<T> handle_src;
		handle_src.opaque = (T*)((void*)opaque) + vipFlatOffset<false>(strides, _start);
		handle_src.shape = _shape;
		handle_src.strides = strides;
		handle_src.size = vipShapeToSize(_shape);

		void* saveOpaque = dst->opaque;
		VipNDArrayShape saveShape = dst->shape;
		VipNDArrayShape saveStrides = dst->strides;
		int saveSize = dst->size;

		dst->opaque = ((uchar*)dst->opaque) + vipFlatOffset<false>(dst->strides, out_start) * dst->dataSize();
		dst->shape = out_shape;
		dst->strides = dst->strides;
		dst->size = vipShapeToSize(out_shape);

		bool res = vipResizeArray(&handle_src, dst, type);
		handle_src.opaque = (void*)nullptr;
		dst->opaque = saveOpaque;
		dst->shape = saveShape;
		dst->strides = saveStrides;
		dst->size = saveSize;
		return res;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	}

}

#endif
