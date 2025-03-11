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

#ifndef VIP_SPAN_H
#define VIP_SPAN_H

#include "VipConfig.h"

#include <type_traits>
#include <iterator>
#include <stdexcept>

namespace Vip
{
	constexpr qsizetype DynamicExtent = -1;
}

namespace detail
{
	template<qsizetype Extent>
	class BaseSpan
	{
	public:
		static constexpr qsizetype size() noexcept { return Extent; }
		constexpr BaseSpan(qsizetype = 0) noexcept {}
	};
	template<>
	class BaseSpan<-1>
	{
		qsizetype d_size{ 0 };

	public:
		constexpr qsizetype size() const noexcept { return d_size; }
		constexpr BaseSpan(qsizetype size = 0) noexcept
		  : d_size(size)
		{
		}
	};
}

/// @brief Class similar to C++20 std::span with a limited API
/// @tparam T value type
/// @tparam Extent static extent or Vip::DynamicExtent
template<class T, qsizetype Extent = Vip::DynamicExtent>
class VipSpan : public detail::BaseSpan<Extent>
{
	template<class U, qsizetype E>
	friend class VipSpan;

	using Base = detail::BaseSpan<Extent>;
	T* d_data{ nullptr };

public:
	static constexpr qsizetype extent = Extent;
	using value_type = typename std::remove_cv<T>::type;
	using element_type = T;
	using pointer = T*;
	using const_pointer = const value_type*;
	using reference = T&;
	using const_reference = const value_type&;
	using size_type = qsizetype;
	using difference_type = qsizetype;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr VipSpan() noexcept = default;
	constexpr VipSpan(const VipSpan&) noexcept = default;
	constexpr VipSpan& operator=(const VipSpan&) noexcept = default;
	template<class U, qsizetype E>
	constexpr VipSpan(const VipSpan<U, E>& s) noexcept
	  : Base(s.size())
	  , d_data(s.d_data)
	{
	}
	constexpr VipSpan(T* data) noexcept
	  : Base()
	  , d_data(data)
	{
		static_assert(Extent != Vip::DynamicExtent, "VipSpan constructor needs a size");
	}
	constexpr VipSpan(T* data, qsizetype size) noexcept
	  : Base(size)
	  , d_data(data)
	{
		static_assert(Extent == Vip::DynamicExtent, "VipSpan constructor has static size");
	}
	using Base::size;
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(T); }
	constexpr bool empty() const noexcept { return size() == 0; }
	constexpr bool isEmpty() const noexcept { return size() == 0; }

	iterator begin() noexcept { return d_data; }
	iterator end() noexcept { return d_data + size(); }
	const_iterator begin() const noexcept { return d_data; }
	const_iterator end() const noexcept { return d_data + size(); }
	const_iterator cbegin() const noexcept { return d_data; }
	const_iterator cend() const noexcept { return d_data + size(); }
	reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
	reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

	constexpr const_pointer data() const noexcept { return d_data; }
	constexpr pointer data() noexcept { return d_data; }

	constexpr const_reference front() const noexcept { return *d_data; }
	constexpr reference front() noexcept { return *d_data; }
	constexpr const_reference back() const noexcept { return d_data[size() - 1]; }
	constexpr reference back() noexcept { return d_data[size() - 1]; }

	constexpr const_reference operator[](size_type idx) const noexcept { return d_data[idx]; }
	constexpr reference operator[](size_type idx) noexcept { return d_data[idx]; }

	constexpr const_reference at(size_type idx) const
	{
		if (idx >= size())
			throw std::out_of_range("");
		return d_data[idx];
	}
	constexpr reference at(size_type idx)
	{
		if (idx >= size())
			throw std::out_of_range("");
		return d_data[idx];
	}

	template<qsizetype Count>
	constexpr VipSpan<element_type, Count> first() const
	{
		return VipSpan<element_type, Count>(const_cast<T*>(d_data));
	}
	constexpr VipSpan<element_type, Vip::DynamicExtent> first(size_type Count) const { return VipSpan<element_type, Vip::DynamicExtent>(const_cast<T*>(d_data), Count); }
	template<qsizetype Count>
	constexpr VipSpan<element_type, Count> last() const
	{
		return VipSpan<element_type, Count>(const_cast<T*>(d_data) + size() - Count);
	}
	constexpr VipSpan<element_type, Vip::DynamicExtent> last(size_type Count) const { return VipSpan<element_type, Vip::DynamicExtent>(const_cast<T*>(d_data) + size() - Count, Count); }
};

#endif