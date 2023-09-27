#ifndef VIP_NDARRAY_H
#define VIP_NDARRAY_H

#include <QDateTime>
#include <QMetaType>
#include <QVariant>

#include "VipArrayBase.h"
#include "VipConfig.h"
#include "VipHybridVector.h"
#include "VipIterator.h"
#include "VipLongDouble.h"

/// \addtogroup DataType
/// @{

namespace detail
{
	template<class Dst, class T>
	struct ProcessList
	{
		static constexpr int count = 0;

		static void shape(VipNDArrayShape&, const T&) { ; }

		static int process(Dst dst, int index, int size, const T& value)
		{
			if (index < size)
				dst[index] = value;
			return index + 1;
		}
	};
	template<class Dst, class T>
	struct ProcessList<Dst, std::initializer_list<T>>
	{
		static constexpr int count = ProcessList<Dst, T>::count + 1;

		static void shape(VipNDArrayShape& sh, const std::initializer_list<T>& init)
		{
			sh.push_back(init.end() - init.begin());
			ProcessList<Dst, T>::shape(sh, *init.begin());
		}

		static int process(Dst dst, int index, int size, const std::initializer_list<T>& value)
		{
			for (const T* it = value.begin(); it != value.end(); ++it) {
				index = ProcessList<Dst, T>::process(dst, index, size, *it);
			}
			return index;
		}
	};
}

/// Base class for N dimensions array.
class VIP_DATA_TYPE_EXPORT VipNDArrayBase : public detail::NullOperand
{
private:
	SharedHandle m_handle;

protected:
	VipNDArrayBase();
	VipNDArrayBase(const SharedHandle& h)
	  : m_handle(h)
	{
	}
	VipNDArrayBase(VipNDArrayBase&& other) noexcept
	  : m_handle(std::move(other.m_handle))
	{
	}

public:
	~VipNDArrayBase();
	/// Returns true if the array is a view (it will never perform a deep copy of the data)
	VIP_ALWAYS_INLINE bool isView() const { return m_handle->handleType() == VipNDArrayHandle::View; }
	VipNDArrayShape viewStart() const;
	/// Returns the internal VipNDArrayHandle
	VIP_ALWAYS_INLINE const VipNDArrayHandle* handle() const noexcept { return m_handle.data(); }
	/// Returns the internal VipNDArrayHandle.
	VIP_ALWAYS_INLINE VipNDArrayHandle* handle() { return m_handle.data(); }
	/// Returns the internal VipNDArrayHandle
	VIP_ALWAYS_INLINE const VipNDArrayHandle* constHandle() const noexcept { return m_handle.data(); }

	VIP_ALWAYS_INLINE void* opaqueData() { return m_handle->opaque; }
	VIP_ALWAYS_INLINE const void* opaqueData() const noexcept { return m_handle->opaque; }

	/// Returns the internal VipNDArrayHandle
	VIP_ALWAYS_INLINE const SharedHandle& sharedHandle() const noexcept { return m_handle; }
	/// Set the internal VipNDArrayHandle
	VIP_ALWAYS_INLINE void setSharedHandle(const SharedHandle& other) { m_handle = other; }
	/// Detaches the array.
	/// If multiple arrays share the same #VipNDArrayHandle, performs a deep copy of the structure.
	void detach() { m_handle.detach(); }

	VipNDArrayBase& operator=(VipNDArrayBase&& other) noexcept
	{
		m_handle = std::move(other.m_handle);
		return *this;
	}
};

template<class T, int NDims>
class VipNDArrayType;
template<class T, int NDims>
class VipNDArrayTypeView;

/// \a VipNDArray represents a N-dimension array of any data type. It uses internally implicit sharing (Copy On Write) based on the #VipNDArrayHandle structure
/// to reduce memory usage and to avoid the needless copying of data.
///
/// All relevant data (data pointer, array size, shape) are internally stored in a shared pointer of #VipNDArrayHandle. You can use #vipCreateArrayHandle
/// to create such object for a specific data type, and #vipRegisterArrayType to register a #SharedHandle for a specific type.
///
/// The shape of a VipNDArray is represented through a #VipNDArrayShape object. To create #VipNDArrayShape objects, you can use the helper function #vipVector.
///
/// \a VipNDArray stores the data in row major order in a linear memory storage. For a 2D array, the height is the dimension of index 0, the width is the dimension of index 1.
/// VipNDArray can also store QImage objects. See #vipToArray, #vipToImage and #vipIsImageArray for more details.
class VIP_DATA_TYPE_EXPORT VipNDArray : public VipNDArrayBase
{
	void import(const void* ptr, int data_type, const VipNDArrayShape& shape);

public:
	friend VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream&, const VipNDArray&);
	friend VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream&, VipNDArray&);

	static const int access_type = Vip::Position;
	typedef VipNDArray view_type;

	/// Supported file formats to save/load VipNDArray
	enum FileFormat
	{
		Text,	   //! Save/load using QTextStream (up to 2D array)
		Binary,	   //! Save/load using QDataStream
		Image,	   //! Save/load using QImage::load
		AutoDetect //! Find the format based on filename: image suffixes like '*.png' will use QImage, *.txt uses QTextStream, other suffixes uses QDataStream
	};
	/// Returns a view on given VipNDArray
	static VipNDArray makeView(const VipNDArray& other);
	/// Returns a view on given array
	static VipNDArray makeView(void* ptr, int data_type, const VipNDArrayShape& shape, const VipNDArrayShape& strides = VipNDArrayShape());
	/// Returns a view on given array
	template<class T>
	static VipNDArray makeView(const T* ptr, const VipNDArrayShape& shape, const VipNDArrayShape& strides = VipNDArrayShape())
	{
		return makeView(const_cast<void*>((const void*)ptr), qMetaTypeId<T>(), shape, strides);
	}

	/// Construct a null VipNDArray object (isNull() == true)
	VipNDArray();
	/// Copy constructor
	VipNDArray(const VipNDArray& other)
	  : VipNDArrayBase(other.sharedHandle())
	{
	}
	/// Move constructor
	VipNDArray(VipNDArray&& other) noexcept
	  : VipNDArrayBase(std::move(other))
	{
	}
	/// Create a VipNDArray from a #SharedHandle
	VipNDArray(const SharedHandle& handle)
	  : VipNDArrayBase(handle)
	{
	}
	/// Create and allocate a VipNDArray of data type \a data_type and shape \a shape
	VipNDArray(int data_type, const VipNDArrayShape& shape);
	/// Create and allocate a VipNDArray of data type \a data_type and shape \a shape.
	/// Performs a deep copy of the data pointer \a ptr into the array
	VipNDArray(const void* ptr, int data_type, const VipNDArrayShape& shape);
	/// Load array from file.
	/// \sa #VipNDArray::load
	VipNDArray(const char* filename, FileFormat format = AutoDetect)
	  : VipNDArray()
	{
		load(filename, format);
	}
	/// Load array from a QIODevice.
	/// \sa #VipNDArray::load
	VipNDArray(QIODevice* device, FileFormat format = AutoDetect)
	  : VipNDArray()
	{
		load(device, format);
	}
	/// Create and allocate a VipNDArray of template data type \a T and shape \a shape.
	/// Performs a deep copy of the data pointer \a ptr into the array
	template<class T>
	VipNDArray(const T* ptr, const VipNDArrayShape& shape)
	  : VipNDArrayBase(vipCreateArrayHandle<T>(VipNDArrayShape()))
	{
		import(ptr, qMetaTypeId<T>(), shape);
	}

	/// Create a VipNDArray of template data type \a T and shape \a shape.
	/// The VipNDArray takes ownership of \a ptr and will delete it either with the custom deleter or with delete[].
	template<class T>
	VipNDArray(T* ptr, const VipNDArrayShape& shape, const vip_deleter_type& del)
	  : VipNDArrayBase(vipCreateArrayHandle<T>(ptr, shape, del))
	{
	}

	template<class T>
	VipNDArray(const T& expression, typename std::enable_if<VipIsExpression<T>::value, void>::type* = NULL);

	template<class T>
	VipNDArray(std::initializer_list<T> init)
	  : VipNDArray()
	{
		using init_type = std::initializer_list<T>;
		VipNDArrayShape shape;
		detail::ProcessList<T*, init_type>::shape(shape, init);
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		detail::ProcessList<T*, init_type>::process(static_cast<T*>(data()), 0, size(), init);
	}

	template<class T>
	VipNDArray(std::initializer_list<std::initializer_list<T>> init)
	  : VipNDArray()
	{
		using init_type = std::initializer_list<std::initializer_list<T>>;
		VipNDArrayShape shape;
		detail::ProcessList<T*, init_type>::shape(shape, init);
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		detail::ProcessList<T*, init_type>::process(static_cast<T*>(data()), 0, size(), init);
	}

	template<class T>
	VipNDArray(std::initializer_list<std::initializer_list<std::initializer_list<T>>> init)
	  : VipNDArray()
	{
		using init_type = std::initializer_list<std::initializer_list<std::initializer_list<T>>>;
		VipNDArrayShape shape;
		detail::ProcessList<T*, init_type>::shape(shape, init);
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		detail::ProcessList<T*, init_type>::process(static_cast<T*>(data()), 0, size(), init);
	}

	/// Returns true if the inner stride is 1
	VIP_ALWAYS_INLINE bool innerUnstrided() const { return strides().last() == 1; }
	/// Returns true is the N-D array is untridded (one chunk of contiguous memory)
	bool isUnstrided() const;

	/// Returns the number of dimensions
	VIP_ALWAYS_INLINE int shapeCount() const noexcept { return handle()->shape.size(); }
	/// Returns the array full shape
	VIP_ALWAYS_INLINE const VipNDArrayShape& shape() const noexcept { return handle()->shape; }
	/// Returns the dimension at index \a i
	VIP_ALWAYS_INLINE int shape(int i) const noexcept { return handle()->shape[i]; }
	/// Returns the array strides
	VIP_ALWAYS_INLINE const VipNDArrayShape& strides() const noexcept { return handle()->strides; }
	/// Returns the stride at index \a i
	VIP_ALWAYS_INLINE int stride(int i) const noexcept { return handle()->strides[i]; }
	/// Returns the array size (multiplication of all dimensions)
	VIP_ALWAYS_INLINE int size() const noexcept { return handle()->size; }
	/// Returns the array data type size.
	/// For instance, it will return 4 for 32bits integer arrays and 8 for double arrays.
	int dataSize() const { return handle()->dataSize(); }
	/// Returns the data type identifier based on qt meta type system.
	/// For instance, it will return QMetaType::Int for 32bits integer arrays.
	int dataType() const { return handle()->dataType(); }
	/// Returns the data type name
	const char* dataName() const { return handle()->dataName(); }
	/// Returns true if the array is null (null data type)
	bool isNull() const { return !handle() || handle()->handleType() == VipNDArrayHandle::Null || handle()->dataType() == 0; }
	/// Returns true if the array is empy (null data type or null size)
	bool isEmpty() const { return isNull() || !handle()->size || !handle()->opaque; }

	/// Returns the underlying data pointer
	VIP_ALWAYS_INLINE void* data() { return handle()->opaque; }

	/// Returns the underlying data pointer
	VIP_ALWAYS_INLINE const void* data() const { return handle()->opaque; }

	/// Returns the underlying data pointer
	VIP_ALWAYS_INLINE const void* constData() const { return handle()->opaque; }

	/// Returns true if this array can be converted into another array of data type \a out_type
	bool canConvert(int out_type) const;
	/// Returns true if this array can be converted into another array of data type \a T
	template<class T>
	bool canConvert() const
	{
		return canConvert(qMetaTypeId<T>());
	}

	/// Fill the array with given value. Returns true on success.
	bool fill(const QVariant& value);

	/// Returns a deep copy of this array
	VipNDArray copy() const;
	/// Returns a deep copy of this array if it is not unstridded, or a new reference to this array
	VipNDArray dense() const
	{
		if (!isUnstrided())
			return copy();
		return *this;
	}

	/// Load a ND array from a file based on given file format
	bool load(const char* filename, FileFormat format = AutoDetect);
	/// Load a ND array from a QIODevice based on given file format.
	/// If \a format is \a AutoDetect, this function uses binary format.
	bool load(QIODevice* device, FileFormat format);

	/// Save array to file based on given file format
	bool save(const char* filename, FileFormat format = AutoDetect);
	/// Save array to a QIODevice based on given file format.
	/// If \a format is \a AutoDetect, this function uses binary format.
	bool save(QIODevice* device, FileFormat format);

	/// Convert this array to given data type
	VipNDArray convert(int out_type) const;

	/// Convert this array to given data type
	template<class T, int NDims = Vip::None>
	VipNDArrayType<T, NDims> convert() const;

	/// Copy the content of another array into this one
	bool import(const VipNDArray& other);

	/// Copy this array content into another already allocated array of possibly different data type.
	bool convert(VipNDArray& other) const;

	/// Returns true if the data type is numerical (integer or floating point types)
	bool isNumeric() const
	{
		return dataType() == QMetaType::Bool || dataType() == QMetaType::Char || dataType() == QMetaType::SChar || dataType() == QMetaType::UChar || dataType() == QMetaType::UShort ||
		       dataType() == QMetaType::Short || dataType() == QMetaType::UInt || dataType() == QMetaType::Int || dataType() == QMetaType::ULong || dataType() == QMetaType::Long ||
		       dataType() == QMetaType::ULongLong || dataType() == QMetaType::LongLong || dataType() == QMetaType::Double || dataType() == QMetaType::Float ||
		       dataType() == qMetaTypeId<long double>();
	}
	/// Returns true if the data type is a signed/unsigned integer type
	bool isIntegral() const
	{
		return dataType() == QMetaType::Bool || dataType() == QMetaType::Char || dataType() == QMetaType::SChar || dataType() == QMetaType::UChar || dataType() == QMetaType::UShort ||
		       dataType() == QMetaType::Short || dataType() == QMetaType::UInt || dataType() == QMetaType::Int || dataType() == QMetaType::ULong || dataType() == QMetaType::Long ||
		       dataType() == QMetaType::ULongLong || dataType() == QMetaType::LongLong;
	}
	/// Returns true if the data type is complex (float or double)
	bool isComplex() const { return dataType() == qMetaTypeId<complex_f>() || dataType() == qMetaTypeId<complex_d>(); }

	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::Char)
	VipNDArray toInt8() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::UChar)
	VipNDArray toUInt8() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::Short)
	VipNDArray toInt16() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::UShort)
	VipNDArray toUInt16() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::Int)
	VipNDArray toInt32() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::UInt)
	VipNDArray toUInt32() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::LongLong)
	VipNDArray toInt64() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::ULongLong)
	VipNDArray toUInt64() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::Float)
	VipNDArray toFloat() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::Double)
	VipNDArray toDouble() const;
	/// Convenient function, equivalent to #VipNDArray::convert( qMetaTypeId<complex_f>())
	VipNDArray toComplexFloat() const;
	/// Convenient function, equivalent to #VipNDArray::convert( qMetaTypeId<complex_d>())
	VipNDArray toComplexDouble() const;
	/// Convenient function, equivalent to #VipNDArray::convert( QMetaType::String)
	VipNDArray toString() const;

	VipNDArray toReal() const;
	VipNDArray toImag() const;
	VipNDArray toArgument() const;
	VipNDArray toAmplitude() const;

	/// Returns a view on a subpart of this array.
	/// If \a shape is empty, a view on the whole array array starting to \a pos is returned.
	/// This function takes care of boundary checks.
	VipNDArray mid(const VipNDArrayShape& pos, const VipNDArrayShape& shape = VipNDArrayShape()) const;

	/// Swap this array content with another
	void swap(VipNDArray& other);

	/// Change this array shape without changing the data content.
	/// This only works if the array flat size does not change, and if the array is unstridded.
	/// Returns true on success, false otherwise.
	bool reshape(const VipNDArrayShape& new_shape);

	/// Resize this array into another one of possibly different data type.
	/// The \a type parameter specifies the interpolation used for resizing.
	/// An empty array cannot be resized.
	bool resize(VipNDArray& dst, Vip::InterpolationType type = Vip::NoInterpolation) const;
	/// Resize this array and return the result array.
	/// The \a type parameter specifies the interpolation used for resizing.
	/// An empty array cannot be resized.
	VipNDArray resize(const VipNDArrayShape& new_shape, Vip::InterpolationType type = Vip::NoInterpolation) const;
	/// Reset the array with a new shape. This will, if necessary, deallocate the previous data, reallocate a new data array with given shape
	/// and leave the internal data uninitialized.
	/// This function keeps the data type and, if possible, the handle type.
	/// Null arrays (with a null data type) and views cannot be reseted.
	bool reset(const VipNDArrayShape& new_shape);

	/// Reset the array with a new shape and type. This will, if necessary, deallocate the previous data, reallocate a new data array with given shape
	/// and leave the internal data uninitialized.
	/// Null arrays (with a null data type) and views cannot be reseted.
	bool reset(const VipNDArrayShape& new_shape, int data_type);

	/// Clear the array. Equivalent to setSharedHandle(vipNullHandle()).
	/// This is reimplemented in VipNDArrayType to keep a handle with the right data type.
	void clear();

	/// Returns the value at \a position.
	/// \a position can be either a dynamic or static #VipHybridVector (see also #vipVector).
	template<class ShapeType>
	QVariant value(const ShapeType& position) const
	{
		return handle()->toVariant(position);
	}

	/// Returns the value at \a position.
	/// \a position can be either a dynamic or static #VipHybridVector (see also #vipvector).
	template<class ShapeType>
	void setValue(const ShapeType& position, const QVariant& val)
	{
		return handle()->fromVariant(position, val);
	}

	/// Returns the value at \a position.
	/// \a position can be either a dynamic or static #VipHybridVector (see also #vipvector).
	template<class ShapeType>
	QVariant operator()(const ShapeType& position) const
	{
		return value(position);
	}

	/// Copy operator.
	/// This array will contain a new reference to \a other.
	VipNDArray& operator=(const VipNDArray& other);
	/// Move operator.
	VipNDArray& operator=(VipNDArray&& other) noexcept
	{
		static_cast<VipNDArrayBase&>(*this) = std::move(other);
		return *this;
	}

	template<class T>
	typename std::enable_if<VipIsExpression<T>::value, VipNDArray&>::type operator=(const T& other);

	template<class T>
	VipNDArray& operator=(std::initializer_list<T> init)
	{
		using init_type = std::initializer_list<T>;
		VipNDArrayShape shape;
		detail::ProcessList<T*, init_type>::shape(shape, init);
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		detail::ProcessList<T*, init_type>::process(static_cast<T*>(data()), 0, size(), init);
		return *this;
	}

	template<class T>
	VipNDArray& operator=(std::initializer_list<std::initializer_list<T>> init)
	{
		using init_type = std::initializer_list<std::initializer_list<T>>;
		VipNDArrayShape shape;
		detail::ProcessList<T*, init_type>::shape(shape, init);
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		detail::ProcessList<T*, init_type>::process(static_cast<T*>(data()), 0, size(), init);
		return *this;
	}

	template<class T>
	VipNDArray& operator=(std::initializer_list<std::initializer_list<std::initializer_list<T>>> init)
	{
		using init_type = std::initializer_list<std::initializer_list<std::initializer_list<T>>>;
		VipNDArrayShape shape;
		detail::ProcessList<T*, init_type>::shape(shape, init);
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		detail::ProcessList<T*, init_type>::process(static_cast<T*>(data()), 0, size(), init);
		return *this;
	}
};

/// Returns true is given array is null (ar.isNull() == true)
VIP_DATA_TYPE_EXPORT bool vipIsNullArray(const VipNDArray& ar);
/// Returns true is given array handle is null
VIP_DATA_TYPE_EXPORT bool vipIsNullArray(const VipNDArrayHandle* h);

/// \a VipNDArrayType is a #VipNDArray with a static data type and storing internally its data in a contiguous array.
/// It provides a STL container like interface.
template<class T, int NDims = Vip::None>
class VipNDArrayType : public VipNDArray
{

public:
	// type definitions
	typedef T value_type;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;
	typedef int size_type;
	typedef int difference_type;
	typedef VipNDArrayTypeView<T, NDims> view_type;
	typedef VipNDArrayType<T, NDims> this_type;

	static const int access_type = Vip::Flat | Vip::Position | Vip::Cwise;
	static const int ndims = NDims;

	using VipNDArray::shape;

	/// Default constructor, create an empty VipNDArrayType
	VipNDArrayType()
	  : VipNDArray(vipCreateArrayHandle<T>())
	{
	}

	VipNDArrayType(const SharedHandle& handle)
	  : VipNDArray(handle)
	{
		if (handle->dataType() != qMetaTypeId<T>() || handle->handleType() != VipNDArrayHandle::Standard)
			clear();
	}

	/// Construct from a VipNDArray
	VipNDArrayType(const VipNDArray& ar)
	  : VipNDArray(vipCreateArrayHandle<T>())
	{
		*this = ar.convert(qMetaTypeId<T>()).dense();
	}
	/// Move constructor
	VipNDArrayType(VipNDArrayType&& other) noexcept
	  : VipNDArray(std::move(other))
	{
	}
	/// Copy constructor
	VipNDArrayType(const VipNDArrayType& other) noexcept
	  : VipNDArray(other)
	{
	}
	/// Copy constructor
	template<int ONDims>
	VipNDArrayType(const VipNDArrayType<T, ONDims>& ar)
	  : VipNDArray(ar)
	{
	}

	/// Allocate the array with given \a shape
	VipNDArrayType(const VipNDArrayShape& shape)
	  : VipNDArray(vipCreateArrayHandle<T>(shape))
	{
	}

	/// Allocate the array with given \a shape and performs a deep copy of \a ptr
	VipNDArrayType(const T* ptr, const VipNDArrayShape& shape)
	  : VipNDArray(ptr, shape)
	{
	}

	/// Takes ownership of \a ptr. It will be deleted with delete[] or the custom deleter if not null.
	VipNDArrayType(T* ptr, const VipNDArrayShape& shape, const vip_deleter_type& del)
	  : VipNDArray(ptr, shape, del)
	{
	}

	/// Allocate the array with given \a shape and inner stride and performs a deep copy of \a ptr
	VipNDArrayType(const T* ptr, int inner_stride, const VipNDArrayShape& shape)
	  : VipNDArray(vipCreateArrayHandle<T>(shape))
	{
		const T* end = ptr + size() * inner_stride;
		T* self = (T*)sharedHandle()->opaque;
		for (; ptr != end; ptr += inner_stride)
			*(self++) = *ptr;
	}

	template<class U>
	VipNDArrayType(const U& expression, typename std::enable_if<VipIsExpression<U>::value, void>::type* = NULL);

	VipNDArrayType(std::initializer_list<T> init)
	  : VipNDArray(init)
	{
		static_assert((NDims == Vip::None) || (NDims == 1), "Invalid number of dimension");
	}
	VipNDArrayType(std::initializer_list<std::initializer_list<T>> init)
	  : VipNDArray(init)
	{
		static_assert((NDims == Vip::None) || (NDims == 2), "Invalid number of dimension");
	}
	VipNDArrayType(std::initializer_list<std::initializer_list<std::initializer_list<T>>> init)
	  : VipNDArray(init)
	{
		static_assert((NDims == Vip::None) || (NDims == 3), "Invalid number of dimension");
	}

	VIP_ALWAYS_INLINE bool isUnstrided() const { return true; }

	/// Returns the data pointer
	VIP_ALWAYS_INLINE T* ptr()
	{
		return (T*)opaqueData(); // static_cast<T*>(handle()->opaque)
		;
	}
	/// Returns the data pointer
	VIP_ALWAYS_INLINE const T* ptr() const noexcept
	{
		return (const T*)opaqueData(); // static_cast<const T*>(constHandle()->opaque)
		;
	}

	/// Returns the data pointer
	VIP_ALWAYS_INLINE T* data() { return static_cast<T*>(handle()->opaque); }
	/// Returns the data pointer
	VIP_ALWAYS_INLINE const T* data() const noexcept { return static_cast<const T*>(handle()->opaque); }
	VIP_ALWAYS_INLINE const T* constData() const noexcept { return static_cast<const T*>(handle()->opaque); }

	/// Returns the data pointer at a specific position
	template<class ShapeType>
	VIP_ALWAYS_INLINE T* ptr(const ShapeType& position)
	{
		return ptr() + vipFlatOffset<true>(strides(), position);
	}
	/// Returns the data pointer at a specific position
	template<class ShapeType>
	VIP_ALWAYS_INLINE const T* ptr(const ShapeType& position) const noexcept
	{
		return ptr() + vipFlatOffset<true>(strides(), position);
	}

	// Reimplement shape() and strides()
	const VipHybridVector<int, NDims>& shape() const { return reinterpret_cast<const VipHybridVector<int, NDims>&>(VipNDArray::shape()); }
	const VipHybridVector<int, NDims>& strides() const { return reinterpret_cast<const VipHybridVector<int, NDims>&>(VipNDArray::strides()); }

	// iterator support

	/// Returns an iterator pointing to the data pointer beginning
	VIP_ALWAYS_INLINE iterator begin() { return ptr(); }
	/// Returns a const_iterator pointing to the data pointer beginning
	VIP_ALWAYS_INLINE const_iterator begin() const noexcept { return ptr(); }
	/// Returns an iterator pointing to the data pointer end
	VIP_ALWAYS_INLINE iterator end() { return ptr() + size(); }
	/// Returns a const_iterator pointing to the data pointer end
	VIP_ALWAYS_INLINE const_iterator end() const noexcept { return ptr() + size(); }

	/// Returns a const reference of the data at given \a position
	template<class ShapeType>
	VIP_ALWAYS_INLINE const_reference operator()(const ShapeType& position) const noexcept
	{
		return ptr()[vipFlatOffset<true>(strides(), position)];
	}

	/// Returns a reference of the data at given \a position
	template<class ShapeType>
	VIP_ALWAYS_INLINE reference operator()(const ShapeType& position)
	{
		return ptr()[vipFlatOffset<true>(strides(), position)];
	}

	// Convenient access operators for 1D -> 3D
	VIP_ALWAYS_INLINE const T& operator()(int x) const noexcept
	{
		if (NDims == 1)
			return ptr()[x];
		else
			return ptr()[x * stride(0)];
	}

	VIP_ALWAYS_INLINE T& operator()(int x)
	{
		if (NDims == 1)
			return ptr()[x];
		else
			return ptr()[x * stride(0)];
	}

	VIP_ALWAYS_INLINE const T& operator()(int y, int x) const noexcept
	{
		if (NDims == 2)
			return ptr()[y * stride(0) + x];
		else
			return ptr()[y * stride(0) + x * stride(1)];
	}

	VIP_ALWAYS_INLINE T& operator()(int y, int x)
	{
		if (NDims == 2)
			return ptr()[y * stride(0) + x];
		else
			return ptr()[y * stride(0) + x * stride(1)];
	}

	VIP_ALWAYS_INLINE const T& operator()(int z, int y, int x) const noexcept
	{
		if (NDims == 3)
			return ptr()[z * stride(0) + y * stride(1) + x];
		else
			return ptr()[z * stride(0) + y * stride(1) + x * stride(2)];
	}

	VIP_ALWAYS_INLINE T& operator()(int z, int y, int x)
	{
		if (NDims == 3)
			return ptr()[z * stride(0) + y * stride(1) + x];
		else
			return ptr()[z * stride(0) + y * stride(1) + x * stride(2)];
	}

	/// Flat access operator
	VIP_ALWAYS_INLINE T& operator[](int index) { return ptr()[index]; }
	VIP_ALWAYS_INLINE const T& operator[](int index) const noexcept { return ptr()[index]; }

	bool reset(const VipNDArrayShape& shape)
	{
		setSharedHandle(vipCreateArrayHandle<T>(shape));
		return true;
	}
	bool reset(const T* ptr, const VipNDArrayShape& shape)
	{
		import(ptr, qMetaTypeId<T>(), shape);
		return true;
	}

	/// Copy operator
	VipNDArrayType& operator=(const VipNDArray& other)
	{
		VipNDArray::operator=(other.convert(qMetaTypeId<T>()).dense());
		return *this;
	}

	/// Copy operator
	VipNDArrayType& operator=(const VipNDArrayType& other)
	{
		VipNDArray::operator=(other);
		return *this;
	}
	/// Move operator
	VipNDArrayType& operator=(VipNDArrayType&& other) noexcept
	{
		static_cast<VipNDArray&>(*this) = std::move(other);
		return *this;
	}

	template<class U>
	typename std::enable_if<VipIsExpression<U>::value, VipNDArrayType<T, NDims>&>::type operator=(const U& other);

	VipNDArrayType& operator=(std::initializer_list<T> init)
	{
		static_assert((NDims == Vip::None) || (NDims == 1), "Invalid number of dimension");
		static_cast<VipNDArray&>(*this) = init;
		return *this;
	}
	VipNDArrayType& operator=(std::initializer_list<std::initializer_list<T>> init)
	{
		static_assert((NDims == Vip::None) || (NDims == 2), "Invalid number of dimension");
		static_cast<VipNDArray&>(*this) = init;
		return *this;
	}
	VipNDArrayType& operator=(std::initializer_list<std::initializer_list<std::initializer_list<T>>> init)
	{
		static_assert((NDims == Vip::None) || (NDims == 3), "Invalid number of dimension");
		static_cast<VipNDArray&>(*this) = init;
		return *this;
	}
};

template<class T, int NDims>
VipNDArrayType<T, NDims> VipNDArray::convert() const
{
	return VipNDArrayType<T, NDims>(convert(qMetaTypeId<T>()));
}

/// \a VipNDArrayTypeView is a view on a #VipNDArray with a static data type and storing internally its data in a contiguous array.
/// It provides a STL container like interface.
///
/// Note that VipNDArrayTypeView breaks the Copy On Write rule and always works on the pointer or VipNDArray given in the constructor,
/// regardless of the reference counting.
///
/// Since the view might have non regular strides, the iterator type is different than #VipNDArrayType and is slightly slower.
template<class T, int NDims = Vip::None>
class VipNDArrayTypeView : public VipNDArray
{
	VIP_ALWAYS_INLINE const void* _p() const noexcept { return static_cast<const detail::ViewHandle*>(constHandle())->ptr; }

public:
	// type definitions
	typedef T value_type;
	typedef T& reference;
	typedef const T& const_reference;
	typedef int size_type;

	typedef VipNDSubArrayIterator<T, NDims> iterator;
	typedef VipNDSubArrayConstIterator<T, NDims> const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	static const int access_type = Vip::Flat | Vip::Position | Vip::Cwise;
	static const int ndims = NDims;
	using VipNDArray::shape;

	VipNDArrayTypeView()
	  : VipNDArray()
	{
	}
	VipNDArrayTypeView(const VipNDArray& ar)
	  : VipNDArray()
	{
		if (!importArray(ar))
			setSharedHandle(vipNullHandle());
	}
	VipNDArrayTypeView(const VipNDArrayTypeView& other)
	  : VipNDArray(other)
	{
	}
	VipNDArrayTypeView(VipNDArrayTypeView&& other) noexcept
	  : VipNDArray(std::move(other))
	{
	}

	VipNDArrayTypeView(T* ptr, const VipNDArrayShape& shape)
	  : VipNDArray()
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape).sharedHandle());
	}
	VipNDArrayTypeView(T* ptr, const VipNDArrayShape& shape, const VipNDArrayShape& strides)
	  : VipNDArray()
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape, strides).sharedHandle());
	}

	bool reset(const VipNDArray& ar) { return importArray(ar); }
	bool reset(T* ptr, const VipNDArrayShape& shape)
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape).sharedHandle());
		return true;
	}
	bool reset(T* ptr, const VipNDArrayShape& shape, const VipNDArrayShape& strides)
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape, strides).sharedHandle());
		return true;
	}

	VIP_ALWAYS_INLINE T* ptr() noexcept { return static_cast<T*>(const_cast<void*>(_p())); }
	VIP_ALWAYS_INLINE const T* ptr() const noexcept { return static_cast<const T*>(_p()); }

	/// Returns the data pointer
	VIP_ALWAYS_INLINE T* data() noexcept { return ptr(); }
	/// Returns the data pointer
	VIP_ALWAYS_INLINE const T* data() const noexcept { return ptr(); }
	VIP_ALWAYS_INLINE const T* constData() const noexcept { return ptr(); }

	// Reimplement shape() and strides()
	const VipHybridVector<int, NDims>& shape() const noexcept { return reinterpret_cast<const VipHybridVector<int, NDims>&>(VipNDArray::shape()); }
	const VipHybridVector<int, NDims>& strides() const noexcept { return reinterpret_cast<const VipHybridVector<int, NDims>&>(VipNDArray::strides()); }

	template<class ShapeType>
	VIP_ALWAYS_INLINE T* ptr(const ShapeType& position) noexcept
	{
		return ptr() + vipFlatOffset<false>(strides(), position);
	}
	template<class ShapeType>
	VIP_ALWAYS_INLINE const T* ptr(const ShapeType& position) const noexcept
	{
		return ptr() + vipFlatOffset<false>(strides(), position);
	}

	template<class ShapeType>
	VIP_ALWAYS_INLINE const T& operator()(const ShapeType& position) const noexcept
	{
		return *(ptr(position));
	}
	template<class ShapeType>
	VIP_ALWAYS_INLINE T& operator()(const ShapeType& position) noexcept
	{
		return *(ptr(position));
	}
	// Convenient access operators for 1D -> 3D
	VIP_ALWAYS_INLINE const T& operator()(int x) const noexcept { return ptr()[x * stride(0)]; }
	VIP_ALWAYS_INLINE T& operator()(int x) noexcept { return ptr()[x * stride(0)]; }
	VIP_ALWAYS_INLINE const T& operator()(int y, int x) const noexcept { return ptr()[y * stride(0) + x * stride(1)]; }
	VIP_ALWAYS_INLINE T& operator()(int y, int x) noexcept { return ptr()[y * stride(0) + x * stride(1)]; }
	VIP_ALWAYS_INLINE const T& operator()(int z, int y, int x) const noexcept { return ptr()[z * stride(0) + y * stride(1) + x * stride(2)]; }
	VIP_ALWAYS_INLINE T& operator()(int z, int y, int x) noexcept { return ptr()[z * stride(0) + y * stride(1) + x * stride(2)]; }

	/// Flat access operator.
	/// Be aware of unwanted consequences when used on strided views!
	VIP_ALWAYS_INLINE T& operator[](int index) noexcept { return ptr()[index]; }
	VIP_ALWAYS_INLINE const T& operator[](int index) const noexcept { return ptr()[index]; }

	VIP_ALWAYS_INLINE iterator begin() noexcept { return iterator(shape(), strides(), ptr(), size()); }
	VIP_ALWAYS_INLINE const_iterator begin() const noexcept { return const_iterator(shape(), strides(), ptr(), size()); }
	VIP_ALWAYS_INLINE const_iterator cbegin() const noexcept { return const_iterator(shape(), strides(), ptr(), size()); }
	VIP_ALWAYS_INLINE iterator end() noexcept { return iterator(shape(), strides(), ptr(), size(), size()); }
	VIP_ALWAYS_INLINE const_iterator end() const noexcept { return const_iterator(shape(), strides(), ptr(), size(), size()); }
	VIP_ALWAYS_INLINE const_iterator cend() const noexcept { return const_iterator(shape(), strides(), ptr(), size(), size()); }

	VIP_ALWAYS_INLINE reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	VIP_ALWAYS_INLINE const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	VIP_ALWAYS_INLINE const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
	VIP_ALWAYS_INLINE reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
	VIP_ALWAYS_INLINE const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	VIP_ALWAYS_INLINE const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

	VipNDArrayTypeView& operator=(const VipNDArray& other)
	{
		if (!import(other))
			setSharedHandle(vipNullHandle());
		return *this;
	}

	template<class U>
	typename std::enable_if<VipIsExpression<U>::value, VipNDArrayTypeView<T, NDims>&>::type operator=(const U& other);

protected:
	bool importArray(const VipNDArray& other)
	{
		if (other.dataType() != qMetaTypeId<T>())
			return false; // wrong data type
		setSharedHandle(VipNDArray::makeView(other).sharedHandle());
		return true;
	}
};

Q_DECLARE_METATYPE(VipNDArray);

/// Returns the higher level type between \a t1 and \a t2.
/// Higher level usually means which type should be use in a numerical operation. For instance, when multiplying an integer with
/// a floating point value, the result should be a floating point value. This also means that a complex type (#complex_f or #complex_d)
/// will always superseed any other numeric type.
///
/// For non numeric/complex types, this function returns the type with the biggest size using QMetaType(type).sizeOf().
///
/// This function returns 0 if \a t1 is not convertible to \a t2 AND \a t2 not convertible to \a t1 (incompatible type).
VIP_DATA_TYPE_EXPORT int vipHigherArrayType(int t1, int t2);
/// Returns the higher level type among given VipNDArray.
/// Higher level usually means which type should be use in a numerical operation. For instance, when multiplying an integer with
/// a floating point value, the result should be a floating point value. This also means that a complex type (#complex_f or #complex_d)
/// will always superseed any other numeric type.
///
/// For non numeric/complex types, this function returns the type with the biggest size using QMetaType(type).sizeOf().
///
/// This function returns 0 if at least one data type is not compatible (or convertible) with the others.
VIP_DATA_TYPE_EXPORT int vipHigherArrayType(const QVector<VipNDArray>& in);
/// Returns the higher level type among \a possible_types that type can be converted to.
/// If several types within \a possible_types are higher than \a type, this function returns the one which is just above \a type.
/// Returns 0 if \a type cannot be converted to any data type within \a possible_types.
VIP_DATA_TYPE_EXPORT int vipHigherArrayType(int type, const QList<int>& possible_types);
/// Returns the higher level type among \a possible_types thaht array data types can be converted to.
/// This function first retrieve the higher level type within \a in, and then uses #vipHigherArrayType(int type, const QList<int> & possible_types)
/// to find the right type.
VIP_DATA_TYPE_EXPORT int vipHigherArrayType(const QVector<VipNDArray>& in, const QList<int>& possible_types);

VIP_DATA_TYPE_EXPORT bool vipConvertToHigherType(const QVector<VipNDArray>& in, QVector<VipNDArray>& out);
VIP_DATA_TYPE_EXPORT bool vipConvertToType(const QVector<VipNDArray>& in, QVector<VipNDArray>& out, int dtype);
VIP_DATA_TYPE_EXPORT QVector<VipNDArray> vipConvertToHigherType(const QVector<VipNDArray>& in);

VIP_DATA_TYPE_EXPORT bool vipConvertToHigherType(const QVector<VipNDArray>& in, QVector<VipNDArray>& out, const QList<int>& possible_types);
VIP_DATA_TYPE_EXPORT QVector<VipNDArray> vipConvertToHigherType(const QVector<VipNDArray>& in, const QList<int>& possible_types);

VIP_DATA_TYPE_EXPORT bool vipApplyDeformation(const VipNDArray& in, const QPointF* deformation, VipNDArray& out, const QVariant& background);

#include "VipEval.h"

template<class T>
VipNDArray::VipNDArray(const T& expression, typename std::enable_if<VipIsExpression<T>::value, void>::type*)
  : VipNDArrayBase(vipNullHandle())
{
	if (reset(expression.shape(), expression.dataType()))
		if (!vipEval(*this, expression))
			clear();
}
template<class T>
typename std::enable_if<VipIsExpression<T>::value, VipNDArray&>::type VipNDArray::operator=(const T& other)
{
	const int dtype = other.dataType();
	const VipNDArrayShape sh = other.shape();
	if (dtype == dataType() && sh == shape()) {
		if (!vipEval(*this, other))
			clear();
		return *this;
	}

	if (!this->reset(sh, dtype)) {
		clear();
		return *this;
	}

	if (!vipEval(*this, other))
		clear();
	return *this;
}

template<class U, int NDims>
template<class T>
VipNDArrayType<U, NDims>::VipNDArrayType(const T& expression, typename std::enable_if<VipIsExpression<T>::value, void>::type*)
  : VipNDArray()
{
	if (reset(expression.shape()))
		if (!vipEval(*this, expression))
			clear();
}

template<class U, int NDims>
template<class T>
typename std::enable_if<VipIsExpression<T>::value, VipNDArrayType<U, NDims>&>::type VipNDArrayType<U, NDims>::operator=(const T& other)
{
	const VipNDArrayShape sh = other.shape();
	if (sh != shape()) {
		if (!reset(sh)) {
			clear();
			return *this;
		}
	}
	if (!vipEval(*this, other)) {
		clear();
	}
	return *this;
}

template<class U, int NDims>
template<class T>
typename std::enable_if<VipIsExpression<T>::value, VipNDArrayTypeView<U, NDims>&>::type VipNDArrayTypeView<U, NDims>::operator=(const T& other)
{
	const VipNDArrayShape sh = other.shape();
	if (sh != shape()) {
		clear();
		return *this;
	}
	if (!vipEval(*this, other)) {
		clear();
	}
	return *this;
}

/// @}
// end DataType

#endif
