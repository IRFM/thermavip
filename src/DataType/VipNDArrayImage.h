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

#ifndef VIP_NDARRAY_IMAGE_H
#define VIP_NDARRAY_IMAGE_H

#include "VipNDArray.h"

#include <QImage>

/// \addtogroup DataType
/// @{

/// Returns an array containing a QImage.
/// VipNDArray::dataType() will return qMetaTypeId<QImage>(). The image is always converted to the format QImage::Format_ARGB32.
VIP_DATA_TYPE_EXPORT VipNDArray vipToArray(const QImage& image);
/// Returns the QImage object contained in \a array if the array contains a QImage.
/// Returns a null image if the array does not contain a QImage.
/// Returned image always has the format QImage::Format_ARGB32.
VIP_DATA_TYPE_EXPORT QImage vipToImage(const VipNDArray& array);
/// Returns true if \a array contains a QImage.
VIP_DATA_TYPE_EXPORT bool vipIsImageArray(const VipNDArray& ar);

/// @brief Specialization of VipNDArrayTypeView for VipRGB
template<qsizetype NDims>
class VipNDArrayTypeView<VipRGB, NDims> : public VipNDArray
{
	VIP_ALWAYS_INLINE const void* _p() const noexcept { return static_cast<const detail::ViewHandle*>(constHandle())->ptr; }

public:
	// type definitions
	typedef VipRGB value_type;
	typedef VipRGB& reference;
	typedef const VipRGB& const_reference;
	typedef qsizetype size_type;
	typedef VipNDSubArrayIterator<VipRGB, NDims> iterator;
	typedef VipNDSubArrayConstIterator<VipRGB, NDims> const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	static const qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
	static const qsizetype ndims = NDims;

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

	VipNDArrayTypeView(const QImage& img)
	  : VipNDArray()
	{
		if (!img.isNull()) {
			img = const_cast<QImage&>(img.convertToFormat(QImage::Format_ARGB32));
			setSharedHandle(VipNDArray::makeView((VipRGB*)(const_cast<QImage&>(img).bits()), vipVector(img.height(), img.width())).sharedHandle());
		}
	}

	VipNDArrayTypeView(VipRGB* ptr, const VipNDArrayShape& shape)
	  : VipNDArray()
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape).sharedHandle());
	}

	VipNDArrayTypeView(VipRGB* ptr, const VipNDArrayShape& shape, const VipNDArrayShape& strides)
	  : VipNDArray()
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape, strides).sharedHandle());
	}

	// Reimplement shape() and strides()
	const VipCoordinate<NDims>& shape() const { return reinterpret_cast<const VipCoordinate<NDims>&>(VipNDArray::shape()); }
	const VipCoordinate<NDims>& strides() const { return reinterpret_cast<const VipCoordinate<NDims>&>(VipNDArray::strides()); }

	bool reset(const VipNDArray& ar) { return importArray(ar); }
	bool reset(VipRGB* ptr, const VipNDArrayShape& shape)
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape).sharedHandle());
		return true;
	}
	bool reset(VipRGB* ptr, const VipNDArrayShape& shape, const VipNDArrayShape& strides)
	{
		setSharedHandle(VipNDArray::makeView(ptr, shape, strides).sharedHandle());
		return true;
	}

	VIP_ALWAYS_INLINE VipRGB* ptr() noexcept { return static_cast<VipRGB*>(const_cast<void*>(_p())); }
	VIP_ALWAYS_INLINE const VipRGB* ptr() const noexcept { return static_cast<const VipRGB*>(_p()); }

	/// Returns the data pointer
	VIP_ALWAYS_INLINE VipRGB* data() noexcept { return ptr(); }
	/// Returns the data pointer
	VIP_ALWAYS_INLINE const VipRGB* data() const noexcept { return ptr(); }
	VIP_ALWAYS_INLINE const VipRGB* constData() const noexcept { return ptr(); }

	template<class ShapeType>
	VIP_ALWAYS_INLINE VipRGB* ptr(const ShapeType& position) noexcept
	{
		return ptr() + vipFlatOffset<false>(strides(), position);
	}
	template<class ShapeType>
	VIP_ALWAYS_INLINE const VipRGB* ptr(const ShapeType& position) const noexcept
	{
		return ptr() + vipFlatOffset<false>(strides(), position);
	}

	template<class ShapeType>
	VIP_ALWAYS_INLINE const VipRGB& operator()(const ShapeType& position) const noexcept
	{
		return *(ptr(position));
	}

	template<class ShapeType>
	VIP_ALWAYS_INLINE VipRGB& operator()(const ShapeType& position) noexcept
	{
		return *(ptr(position));
	}

	// Convenient access operators for 1D -> 3D
	VIP_ALWAYS_INLINE const VipRGB& operator()(qsizetype x) const noexcept { return *(ptr() + x * stride(0)); }

	VIP_ALWAYS_INLINE VipRGB& operator()(qsizetype x) noexcept { return *(ptr() + x * stride(0)); }

	VIP_ALWAYS_INLINE const VipRGB& operator()(qsizetype y, qsizetype x) const noexcept { return *(ptr() + y * stride(0) + x * stride(1)); }

	VIP_ALWAYS_INLINE VipRGB& operator()(qsizetype y, qsizetype x) noexcept { return *(ptr() + y * stride(0) + x * stride(1)); }

	VIP_ALWAYS_INLINE const VipRGB& operator()(qsizetype z, qsizetype y, qsizetype x) const noexcept { return *(ptr() + z * stride(0) + y * stride(1) + x * stride(2)); }

	VIP_ALWAYS_INLINE VipRGB& operator()(qsizetype z, qsizetype y, qsizetype x) noexcept { return *(ptr() + z * stride(0) + y * stride(1) + x * stride(2)); }

	/// Flat access operator.
	/// Be aware of unwanted consequences when used on strided vies!
	VIP_ALWAYS_INLINE VipRGB& operator[](qsizetype index) noexcept { return ptr()[index]; }
	VIP_ALWAYS_INLINE const VipRGB& operator[](qsizetype index) const noexcept { return ptr()[index]; }

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

	template<class T>
	typename std::enable_if<VipIsExpression<T>::value, VipNDArrayTypeView&>::type operator=(const T& other);

protected:
	bool importArray(const VipNDArray& other)
	{
		if (other.dataType() == qMetaTypeId<QImage>()) {
			if (other.handle()->handleType() == VipNDArrayHandle::View) {
				const detail::ViewHandle* h = static_cast<const detail::ViewHandle*>(other.handle());
				VipRGB* bits = ((VipRGB*)((QImage*)(h->opaque))->bits()) + vipFlatOffset<false>(h->strides, h->start);
				return reset(bits, h->shape, h->strides);
			}
			else {
				const VipNDArrayHandle* h = other.handle();
				VipRGB* bits = (VipRGB*)((QImage*)(h->opaque))->bits();
				return reset(bits, h->shape, h->strides);
			}
		}
		else {
			if (other.dataType() != qMetaTypeId<VipRGB>())
				return false; // wrong data type
			setSharedHandle(VipNDArray::makeView(other).sharedHandle());
			return true;
		}
	}
};

template<qsizetype NDims>
template<class T>
typename std::enable_if<VipIsExpression<T>::value, VipNDArrayTypeView<VipRGB, NDims>&>::type VipNDArrayTypeView<VipRGB, NDims>::operator=(const T& other)
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
