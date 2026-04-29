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

#ifndef VIP_STATIC_ND_ARRAY_H
#define VIP_STATIC_ND_ARRAY_H

#include "VipArrayBase.h"
#include "VipEval.h"

#include <utility>
#include <iterator>

namespace detail
{
	template<qsizetype Pos, qsizetype NDims, qsizetype Dim, qsizetype... Dims>
	void build_strides(VipCoordinate<NDims>& s)
	{
		if constexpr (Pos == NDims - 1)
			s[Pos] = 1;
		else {
			s[Pos] = (Dims * ...);
			build_strides<Pos + 1, NDims, Dims...>(s);
		}
	}

}


/// @brief ND array class with static shape.
/// Similar interface to VipNDArrayType.
template<class T, qsizetype... Dims>
struct VipStaticNDArray : public VipNDArrayBase<VipStaticNDArray<T, Dims...>>
{
	static constexpr qsizetype access_type = Vip::Flat | Vip::Position | Vip::Cwise;
	static constexpr qsizetype ndims = sizeof...(Dims);
	static constexpr qsizetype static_size = (... * Dims);
	static_assert(ndims <= VIP_MAX_DIMS);

	T p[static_size];

	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	VipStaticNDArray() noexcept(std::is_nothrow_default_constructible_v<T>) = default;
	VipStaticNDArray(const VipStaticNDArray&) noexcept(std::is_nothrow_copy_constructible_v<T>) = default;
	VipStaticNDArray(VipStaticNDArray&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;
	
	template<class It, std::enable_if_t<VipIsIterator_v<It>, int> = 0>
	VipStaticNDArray(It first, It last)
	{
		qsizetype len = std::min((qsizetype)std::distance(first, last), static_size);
		std::copy(first, std::next(first, len), begin());
	}
	VipStaticNDArray(std::initializer_list<T> lst) noexcept(std::is_nothrow_assignable_v<T, T>)
	  : VipStaticNDArray(lst.begin(), lst.end())
	{
	}

	template<class Other, std::enable_if_t<VipIsIterable_v<Other>, int> = 0>
	VipStaticNDArray(const Other& other)
	  : VipStaticNDArray(other.begin(), other.end())
	{
	}

	template<class Derived>
	VipStaticNDArray(const VipNDArrayBase<Derived>& other)
	{
		const Derived& d = static_cast<const Derived&>(other);
		if (d.isEmpty())
			return;
		d.convert(data(), dataType(), shape(), strides());
	}

	template<class Other, std::enable_if_t<VipIsExpression<Other>::value, int> = 0>
	VipStaticNDArray(const Other& other)
	{
		if (shape() != other.shape())
			return;
		vipEval(*this, other);
	}

	VipStaticNDArray& operator=(const VipStaticNDArray&) noexcept(std::is_nothrow_assignable_v<T, T>) = default;
	VipStaticNDArray& operator=(VipStaticNDArray&&) noexcept(std::is_nothrow_move_assignable_v<T>) = default;

	template<class Derived>
	VipStaticNDArray& operator=(const VipNDArrayBase<Derived>& other)
	{
		const Derived& d = static_cast<const Derived&>(other);
		if (!d.isEmpty())
			d.convert(data(), dataType(), shape(), strides());
		return *this;
	}

	template<class Other, std::enable_if_t<VipIsExpression<Other>::value, int> = 0>
	VipStaticNDArray& operator=(const Other& other)
	{
		if (shape() != other.shape())
			return *this;
		vipEval(*this, other);
		return *this;
	}

	constexpr qsizetype size() const noexcept { return static_size; }
	constexpr bool isEmpty() const noexcept { return false; }
	constexpr bool isUnstrided() const noexcept { return true; }
	constexpr bool innerUnstrided() const noexcept { return true; }

	auto data() noexcept { return p; }
	auto data() const noexcept { return p; }
	auto constData() const noexcept { return p; }
	auto ptr() noexcept { return p; }
	auto ptr() const noexcept { return p; }
	auto constPtr() const noexcept { return p; }

	template<qsizetype N>
	auto flatIndex(const VipCoordinate<N>& c) const noexcept
	{
		VIP_ASSERT_DEBUG(c.size() == shapeCount());
		return vipFlatOffset<true>(strides(), c);
	}

	template<qsizetype N>
	auto ptr(const VipCoordinate<N>& c) noexcept
	{
		// Note: coordinate might be incomplete
		return ptr() + vipFlatOffset<false>(strides(), c);
	}
	template<qsizetype N>
	auto ptr(const VipCoordinate<N>& c) const noexcept
	{
		// Note: coordinate might be incomplete
		return ptr() + vipFlatOffset<false>(strides(), c);
	}

	auto operator[](qsizetype i) noexcept -> reference { return p[i]; }
	auto operator[](qsizetype i) const noexcept -> const_reference { return p[i]; }

	template<qsizetype N>
	auto operator()(const VipCoordinate<N>& c) noexcept -> reference
	{
		return *ptr(c);
	}
	template<qsizetype N>
	auto operator()(const VipCoordinate<N>& c) const noexcept -> const_reference
	{
		return *ptr(c);
	}

	template<class... Coords, std::enable_if_t<VipIsAllIntegers<Coords...>::value, int> = 0>
	auto operator()(Coords... coords) const noexcept -> const_reference
	{
		return *ptr(vipVector(coords...));
	}
	template<class... Coords, std::enable_if_t<VipIsAllIntegers<Coords...>::value, int> = 0>
	auto operator()(Coords... coords) noexcept -> reference
	{
		return *ptr(vipVector(coords...));
	}

	auto begin() noexcept { return p; }
	auto begin() const noexcept { return p; }
	auto cbegin() const noexcept { return p; }
	auto end() noexcept { return p + static_size; }
	auto end() const noexcept { return p + static_size; }
	auto cend() const noexcept { return p + static_size; }

	auto rbegin() noexcept { return reverse_iterator(end()); }
	auto rbegin() const noexcept { return const_reverse_iterator(end()); }
	auto rcbegin() const noexcept { return rbegin(); }
	auto rend() noexcept { return reverse_iterator(begin()); }
	auto rend() const noexcept { return const_reverse_iterator(begin()); }
	auto rcend() const noexcept { return rend(); }

	static auto dataType() noexcept { return qMetaTypeId<T>(); }
	static auto shapeCount() noexcept { return ndims; }
	static auto shape() noexcept { return vipVector(Dims...); }
	static auto strides() noexcept
	{
		VipCoordinate<ndims> ret;
		detail::build_strides<0, ndims, Dims...>(ret);
		return ret;
	}
	auto shape(qsizetype dim) const noexcept { return shape()[dim]; }
	auto stride(qsizetype dim) const noexcept { return strides()[dim]; }

	/// @brief Copy array content to destination buffer
	/// with given shape and strides.
	bool convert(void* dst, int dst_type, const VipNDArrayShape& shape, const VipNDArrayShape& strides) const
	{
		qsizetype out_size = vipCumMultiply(shape);
		if (out_size != static_size)
			return false;

		if (dst_type == qMetaTypeId<T>())
			return vipArrayTransform(data(), this->shape(), this->strides(), (T*)dst, shape, strides, VipNullTransform{});

		return detail::convert(data(), qMetaTypeId<T>(), this->shape(), this->strides(), dst, dst_type, shape, strides);
	}

	void swap(VipStaticNDArray& other) noexcept
	{ 
		for (qsizetype i = 0; i < static_size; ++i)
			std::swap(p[i], other.p[i]);
	}

	int count(const void* handle) const noexcept { return 0; }
	bool alias(const void* p) const noexcept { return (char*)p >= (char*)begin() && (char*)p < (char*)end(); }
};

#endif
