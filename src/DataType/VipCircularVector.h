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

#ifndef VIP_CIRCULAR_VECTOR_H
#define VIP_CIRCULAR_VECTOR_H

#include "VipConfig.h"
#include "VipMath.h"
#include "VipSpan.h"
#include <QTypeInfo>
#include <qlist.h>
#include <qvector.h>
#include <qshareddata.h>
#include <qdatastream.h>

#include <utility>
#include <type_traits>
#include <iterator>
#include <algorithm>

namespace Vip
{
	enum Ownership
	{
		SharedOwnership,
		StrongOwnership
	};
}
namespace detail
{
	// Convenient random access iterator on a constant value
	template<class T>
	class cvalue_iterator
	{
		using alloc_traits = std::allocator_traits<std::allocator<T>>;

	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = typename alloc_traits::difference_type;
		using size_type = typename alloc_traits::size_type;
		using pointer = typename alloc_traits::const_pointer;
		using reference = const value_type&;

		explicit cvalue_iterator(size_type _pos)
		  : pos(_pos)
		{
		}
		cvalue_iterator(size_type _pos, const T& _value)
		  : pos(_pos)
		  , value(_value)
		{
		}
		auto operator*() const noexcept -> reference { return value; }
		auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
		auto operator++() noexcept -> cvalue_iterator&
		{
			++pos;
			return *this;
		}
		auto operator++(int) noexcept -> cvalue_iterator
		{
			cvalue_iterator _Tmp = *this;
			++(*this);
			return _Tmp;
		}
		auto operator--() noexcept -> cvalue_iterator&
		{
			--pos;
			return *this;
		}
		auto operator--(int) noexcept -> cvalue_iterator
		{
			cvalue_iterator _Tmp = *this;
			--(*this);
			return _Tmp;
		}
		auto operator==(const cvalue_iterator& other) const noexcept -> bool { return pos == other.pos; }
		auto operator!=(const cvalue_iterator& other) const noexcept -> bool { return pos != other.pos; }
		auto operator+=(difference_type diff) noexcept -> cvalue_iterator&
		{
			pos += diff;
			return *this;
		}
		auto operator-=(difference_type diff) noexcept -> cvalue_iterator&
		{
			pos -= diff;
			return *this;
		}
		auto operator[](difference_type diff) const noexcept -> const value_type& { return value; }

		T value;
		size_type pos;
	};

	template<class T>
	auto operator+(const cvalue_iterator<T>& it, typename cvalue_iterator<T>::difference_type diff) noexcept -> cvalue_iterator<T>
	{
		cvalue_iterator<T> res = it;
		return res += diff;
	}
	template<class T>
	auto operator-(const cvalue_iterator<T>& it, typename cvalue_iterator<T>::difference_type diff) noexcept -> cvalue_iterator<T>
	{
		cvalue_iterator<T> res = it;
		return res -= diff;
	}
	template<class T>
	auto operator-(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> typename cvalue_iterator<T>::difference_type
	{
		return it1.pos - it2.pos;
	}
	template<class T>
	auto operator<(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos < it2.pos;
	}
	template<class T>
	auto operator>(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos > it2.pos;
	}
	template<class T>
	auto operator<=(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos <= it2.pos;
	}
	template<class T>
	auto operator>=(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos >= it2.pos;
	}

	// Simply call p->~T(), used as a replacement to std::allocator::destroy() which was removed in C++20
	template<class T>
	VIP_ALWAYS_INLINE void destroy_ptr(T* p) noexcept
	{
		p->~T();
	}

	// Simply call new (p) T(...), used as a replacement to std::allocator::construct() which was removed in C++20
	template<class T, class... Args>
	VIP_ALWAYS_INLINE void construct_ptr(T* p, Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
	{
		new (p) T(std::forward<Args>(args)...);
	}

	// Destroy range
	template<class T>
	void destroy_range_ptr(T* p, qsizetype size) noexcept
	{
		if VIP_CONSTEXPR (!std::is_trivially_destructible<T>::value)
			for (qsizetype i = 0; i < size; ++i)
				destroy_ptr(p + i);
	}

	template<class T, bool NoExcept = std::is_nothrow_move_constructible<T>::value>
	struct Mover
	{
		static void move_construct(T* dst, T* src, qsizetype size)
		{
			qsizetype i = 0;
			try {
				for (; i < size; ++i)
					construct_ptr(dst + i, std::move(src[i]));
			}
			catch (...) {
				destroy_range_ptr(dst, i);
				throw;
			}
		}
	};
	template<class T>
	struct Mover<T, true>
	{
		static void move_construct(T* dst, T* src, qsizetype size) noexcept
		{
			for (qsizetype i = 0; i < size; ++i)
				construct_ptr(dst + i, std::move(src[i]));
		}
	};

	// Move construct range, taking care of potential exceptions throw in constructor
	template<class T>
	void move_construct_range(T* dst, T* src, qsizetype size) noexcept(std::is_nothrow_move_constructible<T>::value)
	{
		Mover<T>::move_construct(dst, src, size);
	}

	// Similar to QSharedDataPointer but lighter and INLINED!!
	template<typename T, Vip::Ownership>
	class COWPointer
	{
	public:
		COWPointer() noexcept {}
		~COWPointer() noexcept
		{
			if (d && !d->deref())
				delete d;
		}
		explicit COWPointer(T* data) noexcept
		  : d(data)
		{
			if (d)
				d->ref();
		}
		void reset(T* ptr = nullptr) noexcept
		{
			if (ptr != d) {
				if (ptr)
					ptr->ref();
				T* old = std::exchange(d, ptr);
				if (old && !old->deref())
					delete old;
			}
		}
		COWPointer(const COWPointer& o) noexcept
		  : d(o.d)
		{
			if (d)
				d->ref();
		}
		COWPointer(COWPointer&& o) noexcept
		  : d(std::exchange(o.d, nullptr))
		{
		}
		COWPointer& operator=(T* o) noexcept
		{
			reset(o);
			return *this;
		}
		COWPointer& operator=(const COWPointer& o) noexcept
		{
			reset(o.d);
			return *this;
		}
		COWPointer& operator=(COWPointer&& other) noexcept
		{
			COWPointer moved(std::move(other));
			swap(moved);
			return *this;
		}
		VIP_ALWAYS_INLINE T* detach()
		{
			if (d && d->load() != 1)
				detach_helper();
			return d;
		}
		VIP_ALWAYS_INLINE T* operator->() { return detach(); }
		VIP_ALWAYS_INLINE const T* operator->() const noexcept { return d; }
		VIP_ALWAYS_INLINE T* data() { return detach(); }
		VIP_ALWAYS_INLINE const T* data() const noexcept { return d; }
		VIP_ALWAYS_INLINE const T* constData() const noexcept { return d; }
		VIP_ALWAYS_INLINE operator bool() const noexcept { return d != nullptr; }
		VIP_ALWAYS_INLINE bool operator!() const noexcept { return d == nullptr; }
		VIP_ALWAYS_INLINE void swap(COWPointer& other) noexcept { std::swap(d, other.d); }

	private:
		void detach_helper()
		{
			T* x = new T(*d);
			x->ref();
			if (!d->deref())
				delete d;
			d = x;
		}
		T* d{ nullptr };
	};

	template<typename T>
	class COWPointer<T, Vip::StrongOwnership>
	{
	public:
		COWPointer() noexcept {}
		~COWPointer() noexcept
		{
			if (d)
				delete d;
		}
		explicit COWPointer(T* data) noexcept
		  : d(data)
		{
		}
		COWPointer(const COWPointer& o) noexcept
		  : d(o.d ? new T(*o.d) : nullptr)
		{
		}
		COWPointer(COWPointer&& o) noexcept
		  : d(std::exchange(o.d, nullptr))
		{
		}
		COWPointer& operator=(const COWPointer& o) noexcept
		{
			if (d)
				delete d;
			d = nullptr;
			d = (o.d ? new T(*o.d) : nullptr);
			return *this;
		}
		COWPointer& operator=(COWPointer&& o) noexcept
		{
			std::swap(d, o.d);
			return *this;
		}
		void reset(T* od) noexcept
		{
			if (d)
				delete d;
			d = od;
		}
		VIP_ALWAYS_INLINE void detach() noexcept {}
		VIP_ALWAYS_INLINE T* operator->() noexcept { return d; }
		VIP_ALWAYS_INLINE const T* operator->() const noexcept { return d; }
		VIP_ALWAYS_INLINE T* data() noexcept { return d; }
		VIP_ALWAYS_INLINE const T* data() const noexcept { return d; }
		VIP_ALWAYS_INLINE const T* constData() const noexcept { return d; }
		VIP_ALWAYS_INLINE operator bool() const noexcept { return d != nullptr; }
		VIP_ALWAYS_INLINE bool operator!() const noexcept { return d == nullptr; }
		VIP_ALWAYS_INLINE void swap(COWPointer& o) noexcept { std::swap(d, o.d); }

	private:
		T* d{ nullptr };
	};

	template<class T, Vip::Ownership>
	struct BaseCircularBuffer
	{
		std::atomic<qsizetype> cnt{ 0 }; // reference count
		VIP_ALWAYS_INLINE void ref() noexcept { cnt.fetch_add(1); }
		VIP_ALWAYS_INLINE bool deref() noexcept { return cnt.fetch_sub(1, std::memory_order_acq_rel) != (qsizetype)1; }
		VIP_ALWAYS_INLINE qsizetype load() const noexcept { return cnt.load(std::memory_order_relaxed); }
	};
	template<class T>
	struct BaseCircularBuffer<T, Vip::StrongOwnership>
	{
	};

	// Circular buffer class used internally by VipCircularVector
	template<class T, Vip::Ownership O>
	struct CircularBuffer : public BaseCircularBuffer<T, O>
	{
		static constexpr bool relocatable = QTypeInfo<T>::isRelocatable;
		using value_type = T;

		qsizetype begin;	  // begin index of data
		qsizetype size;		  // number of elements
		const qsizetype capacity; // buffer capacity

		T* buffer; // actual values

		// Initialize from a maximum capacity
		CircularBuffer(qsizetype max_size = 0)
		  : begin(0)
		  , size(0)
		  , capacity(max_size)
		  , buffer(nullptr)
		{
			VIP_ASSERT_DEBUG(max_size >= 0, "");
			if (max_size) {
				buffer = (T*)malloc((size_t)max_size * sizeof(T));
				if (!buffer)
					throw std::bad_alloc();
			}
		}
		// Initialize from a maximum capacity, a size and a default value.
		template<class... Args>
		CircularBuffer(qsizetype max_size, qsizetype current_size, Args&&... args)
		  : CircularBuffer(max_size)
		{
			size = current_size;
			// initialize full buffer
			qsizetype i = 0;
			try {
				for (; i < current_size; ++i)
					construct_ptr(buffer + i, std::forward<Args>(args)...);
			}
			catch (...) {
				destroy_range_ptr(buffer, i);
				size = 0;
				throw;
			}
		}

		// Copy ctor
		CircularBuffer(const CircularBuffer& other)
		  : CircularBuffer(other.capacity)
		{
			size = other.size;
			// initialize full buffer
			qsizetype i = 0;
			try {
				for (; i < size; ++i)
					construct_ptr(buffer + i, other[i]);
			}
			catch (...) {
				destroy_range_ptr(buffer, i);
				size = 0;
				throw;
			}
		}

		~CircularBuffer() noexcept
		{
			destroy_range(0, size);
			if (buffer)
				free(buffer);
		}

		VIP_ALWAYS_INLINE qsizetype mask() const noexcept { return capacity - 1; }

		// Relocate to dst.
		// Called just before destroying this.
		void relocate(CircularBuffer* dst) noexcept(std::is_nothrow_move_constructible<T>::value || relocatable)
		{
			qsizetype stop = (begin + size) > capacity ? (capacity) : (begin + size);
			qsizetype first_range = (stop - begin);
			qsizetype remaining = size - first_range;
			if VIP_CONSTEXPR (relocatable) {
				memmove(static_cast<void*>(dst->buffer), buffer + begin, (size_t)first_range * sizeof(T));
				if (remaining)
					memmove(static_cast<void*>(dst->buffer + first_range), buffer, (size_t)remaining * sizeof(T));
			}
			else {
				move_construct_range(dst->buffer, buffer + begin, first_range);
				if (remaining)
					move_construct_range(dst->buffer + first_range, buffer, remaining);
			}
			dst->size = this->size;
			if VIP_CONSTEXPR (relocatable)
				this->size = 0;
		}

		std::pair<VipSpan<T>, VipSpan<T>> spans(qsizetype first, qsizetype last) noexcept
		{
			if (first == last)
				return std::pair<VipSpan<T>, VipSpan<T>>();
			std::pair<VipSpan<T>, VipSpan<T>> res;
			qsizetype idx_first = (begin + first) & mask();
			qsizetype idx_last = (begin + last) & mask();
			qsizetype first_stop = idx_first < idx_last ? idx_last : capacity;
			res.first = VipSpan<T>(buffer + idx_first, first_stop - idx_first);
			if (idx_first >= idx_last)
				res.second = VipSpan<T>(buffer, idx_last);
			return res;
		}
		std::pair<VipSpan<const T>, VipSpan<const T>> cspans(qsizetype first, qsizetype last) const noexcept { return const_cast<CircularBuffer*>(this)->spans(first, last); }

		void destroy_range(qsizetype first, qsizetype last) noexcept
		{
			if VIP_CONSTEXPR (!std::is_trivially_destructible<T>::value) {
				auto s = spans(first, last);
				for (T& v : s.first)
					destroy_ptr(&v);
				for (T& v : s.second)
					destroy_ptr(&v);
			}
		}

		// Element access.
		VIP_ALWAYS_INLINE auto operator[](qsizetype index) noexcept -> T& { return at(index); }
		VIP_ALWAYS_INLINE auto operator[](qsizetype index) const noexcept -> const T& { return at(index); }
		VIP_ALWAYS_INLINE auto at(qsizetype index) noexcept -> T& { return buffer[((begin + index) & (mask()))]; }
		VIP_ALWAYS_INLINE auto at(qsizetype index) const noexcept -> const T& { return buffer[((begin + index) & (mask()))]; }

		// Front/back access
		VIP_ALWAYS_INLINE auto front() noexcept -> T& { return buffer[begin]; }
		VIP_ALWAYS_INLINE auto front() const noexcept -> const T& { return buffer[begin]; }
		VIP_ALWAYS_INLINE auto back() noexcept -> T& { return (*this)[size - 1]; }
		VIP_ALWAYS_INLINE auto back() const noexcept -> const T& { return (*this)[size - 1]; }

		// Resize buffer
		void resize(qsizetype s)
		{
			if (s < size) {
				destroy_range(s, size);
			}
			else if (s > size) {
				qsizetype i = size;
				// Make sure to destroy constructed values in case of exception
				try {
					for (; i < s; ++i)
						construct_ptr(&(*this)[i]);
				}
				catch (...) {
					destroy_range(size, i);
					throw;
				}
			}
			size = s;
		}
		void resize(qsizetype s, const T& value)
		{
			if (s < size) {
				destroy_range(s, size);
			}
			else if (s > size) {
				qsizetype i = size;
				// Make sure to destroy constructed values in case of exception
				try {
					for (; i < s; ++i)
						construct_ptr(&at(i), value);
				}
				catch (...) {
					destroy_range(size, i);
					throw;
				}
			}
			size = s;
		}
		void resize_front(qsizetype s)
		{
			if (s < size) {
				pop_front_n(size - s);
			}
			else if (s > size) {
				push_front_n(s - size);
			}
		}
		void resize_front(qsizetype s, const T& value)
		{
			if (s < size) {
				pop_front_n(size - s);
			}
			else if (s > size) {
				push_front_n(s - size, value);
			}
		}

		template<class... Args>
		VIP_ALWAYS_INLINE T* emplace_back(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
		{
			// only works for non full array
			T* ptr = &at(size);
			// Might throw, which is fine
			construct_ptr(ptr, std::forward<Args>(args)...);
			++size;
			return ptr;
		}

		template<class... Args>
		VIP_ALWAYS_INLINE T* emplace_front(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
		{
			// only works for non full array
			qsizetype loc = (begin - 1) & mask();
			construct_ptr(buffer + loc, std::forward<Args>(args)...);
			begin = loc;
			++size;
			return buffer + begin;
		}

		// Pop back/front
		VIP_ALWAYS_INLINE void pop_back() noexcept
		{
			if VIP_CONSTEXPR (!std::is_trivially_destructible<T>::value)
				destroy_ptr(&back());
			--size;
		}
		VIP_ALWAYS_INLINE T pop_back_return() noexcept
		{
			T r = std::move(back());
			--size;
			return std::move(r);
		}

		VIP_ALWAYS_INLINE void pop_front() noexcept
		{
			if VIP_CONSTEXPR (!std::is_trivially_destructible<T>::value)
				destroy_ptr(buffer + begin);
			begin = (begin + 1) & mask();
			--size;
		}
		VIP_ALWAYS_INLINE T pop_front_return() noexcept
		{
			T r = std::move(buffer[begin]);
			begin = (begin + 1) & mask();
			--size;
			return std::move(r);
		}

		// Pop front n values
		void pop_front_n(qsizetype n) noexcept
		{
			destroy_range(0, n);
			size -= n;
			begin = (begin + n) & mask();
		}
		// Push front n copies of value
		template<class U>
		void push_front_n(qsizetype n, const U& value)
		{
			for (qsizetype i = 0; i < n; ++i)
				push_front(value);
		}
		// Push front n values
		void push_front_n(qsizetype n)
		{
			try {
				for (qsizetype i = 0; i < n; ++i) {
					if (--begin < 0)
						begin = mask();
					construct_ptr(buffer + begin);
					++size;
				}
			}
			catch (...) {
				begin = (begin + 1) & mask();
				throw;
			}
		}

		// Move buffer content toward the right by 1 element
		// Might throw
		void move_right_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
		{
			// starting from pos, move elements toward the end
			T* ptr1 = &at(size - 1);
			T* stop = &at(pos);
			if (stop > ptr1)
				stop = buffer;

			if VIP_CONSTEXPR (!relocatable) {
				while (ptr1 > stop) {
					ptr1[0] = std::move(ptr1[-1]);
					--ptr1;
				}
			}
			else {
				memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T));
				ptr1 = stop;
			}

			if (ptr1 != &at(pos)) {
				if VIP_CONSTEXPR (!relocatable)
					*ptr1 = std::move(*(buffer + mask()));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer + mask()), sizeof(T));
				ptr1 = (buffer + mask());
				stop = &at(pos);
				if VIP_CONSTEXPR (!relocatable) {
					while (ptr1 > stop) {
						ptr1[0] = std::move(ptr1[-1]);
						--ptr1;
					}
				}
				else {
					memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T));
				}
			}
		}
		// Move buffer content toward the left by 1 element
		// Might throw
		void move_left_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
		{
			// starting from pos, move elements toward the beginning
			T* ptr1 = &at(0);
			T* stop = buffer + ((begin + pos - 1) & mask()); //&at(pos);
			if (stop < ptr1)
				stop = buffer + mask();
			if VIP_CONSTEXPR (!relocatable) {
				while (ptr1 < stop) {
					*ptr1 = std::move(ptr1[1]);
					++ptr1;
				}
			}
			else {
				memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T));
				ptr1 = stop;
			}
			if (ptr1 != buffer + ((begin + pos - 1) & mask())) {
				if VIP_CONSTEXPR (!relocatable)
					*ptr1 = std::move(*(buffer));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer), sizeof(T));
				ptr1 = buffer;
				stop = &at(pos - 1);
				if VIP_CONSTEXPR (!relocatable) {
					while (ptr1 < stop) {
						*ptr1 = std::move(ptr1[1]);
						++ptr1;
					}
				}
				else {
					memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T));
				}
			}
		}

		// Move buffer content toward the left by 1 element
		// Might throw
		void move_right(qsizetype pos) noexcept((std::is_nothrow_move_assignable<T>::value && std::is_nothrow_default_constructible<T>::value) || relocatable)
		{
			if VIP_CONSTEXPR (!relocatable)
				construct_ptr(&(*this)[size]);
			// move elems after pos
			++size;
			// Might throw, fine since no more values are constructed/destructed
			move_right_1(pos);
			// Non optimized way:
			// for (qsizetype i = size - 1; i > pos; --i)
			//	(*this)[i] = std::move((*this)[i - 1]);
		}
		void move_left(qsizetype pos) noexcept((std::is_nothrow_move_assignable<T>::value && std::is_nothrow_default_constructible<T>::value) || relocatable)
		{
			if VIP_CONSTEXPR (!relocatable)
				construct_ptr(&(*this)[begin ? -1 : mask()]);
			// move elems before pos
			if (--begin < 0)
				begin = mask();
			++size;
			// Might throw, fine since no more values are constructed/destructed
			move_left_1(pos + 1);
			// Non optimized way:
			// for (qsizetype i = 0; i < pos -1; ++i)
			//	(*this)[i] = std::move((*this)[i + 1]);
		}

		// Insert value at given position. Only works if the buffer is not full.
		template<class... Args>
		T* emplace(qsizetype pos, Args&&... args)
		{
			VIP_ASSERT_DEBUG(size != capacity, "cannot insert in a full circular buffer");
			if (pos > size / 2) {
				move_right(pos);
			}
			else {
				move_left(pos);
			}

			T* res = &(*this)[pos];
			if VIP_CONSTEXPR (relocatable) {
				try {
					construct_ptr(res, std::forward<Args>(args)...);
				}
				catch (...) {
					// No choice but to destroy all values after pos and reduce the size in order to leave the buffer in a valid state
					destroy_range(pos + 1, size);
					size = pos;
					throw;
				}
			}
			else {
				// For non relocatable types, use move assignment to provide basic exception guarantee
				*res = T(std::forward<Args>(args)...);
			}

			return res;
		}

		void move_erase_right_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
		{
			// starting from pos, move elements toward the end
			T* ptr1 = &at(pos);
			T* stop = &at(size);
			if (stop < ptr1)
				stop = buffer + mask();
			if VIP_CONSTEXPR (!relocatable) {
				while (ptr1 < stop) {
					*ptr1 = std::move(ptr1[1]);
					++ptr1;
				}
			}
			else {
				memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T));
				ptr1 = stop;
			}
			if (ptr1 != &at(size)) {
				if VIP_CONSTEXPR (!relocatable)
					*ptr1 = std::move(*(buffer));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer), sizeof(T));
				ptr1 = buffer;
				stop = &at(size);
				if VIP_CONSTEXPR (!relocatable) {
					while (ptr1 < stop) {
						*ptr1 = std::move(ptr1[1]);
						++ptr1;
					}
				}
				else {
					memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T));
				}
			}
		}
		void move_erase_left_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
		{
			// starting from pos, move elements toward the beginning
			T* ptr1 = &at(pos);
			T* stop = &at(0);
			if (stop > ptr1)
				stop = buffer;
			if VIP_CONSTEXPR (!relocatable) {
				while (ptr1 > stop) {
					*ptr1 = std::move(ptr1[-1]);
					--ptr1;
				}
			}
			else {
				memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T));
				ptr1 = stop;
			}

			if (ptr1 != &at(0)) {
				if VIP_CONSTEXPR (!relocatable)
					*ptr1 = std::move(*(buffer + mask()));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer + mask()), sizeof(T));
				ptr1 = (buffer + mask());
				stop = &at(0);
				if VIP_CONSTEXPR (!relocatable) {
					while (ptr1 > stop) {
						*ptr1 = std::move(ptr1[-1]);
						--ptr1;
					}
				}
				else {
					memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T));
				}
			}
		}

		// Erase value at given position.
		void erase(qsizetype pos)
		{
			// for relocatable type, destroy value at pos since it will be memcpied over
			if VIP_CONSTEXPR (relocatable && !std::is_trivially_destructible<T>::value)
				destroy_ptr(&(*this)[pos]);
			if (pos > size / 2) {
				// move elems after pos
				try {
					--size;
					move_erase_right_1(pos);
				}
				catch (...) {
					++size;
					throw;
				}
				if VIP_CONSTEXPR (!relocatable && !std::is_trivially_destructible<T>::value)
					destroy_ptr(&(*this)[size]);
			}
			else {
				// move elems after pos
				try {
					--size; // decrement size
					move_erase_left_1(pos);
				}
				catch (...) {
					++size;
					throw;
				}
				if VIP_CONSTEXPR (!relocatable && !std::is_trivially_destructible<T>::value)
					destroy_ptr(&(*this)[0]);
				begin = (begin + 1) & (mask()); // increment begin
			}
		}
	};

	// Const iterator for VipCircularVector
	template<class Data>
	struct VipCircularVectorConstIterator
	{
		using value_type = typename Data::value_type;
		using iterator_category = std::random_access_iterator_tag;
		using size_type = qsizetype;
		using difference_type = qsizetype;
		using pointer = const value_type*;
		using reference = const value_type&;

		const Data* data;
		difference_type pos;

		VIP_ALWAYS_INLINE VipCircularVectorConstIterator() noexcept {}
		VIP_ALWAYS_INLINE VipCircularVectorConstIterator(const Data* d, difference_type p) noexcept
		  : data(d)
		  , pos(p)
		{
		}
		// We could use default copy constructor, but it turns out that msvc do not properly inline it in most situation!
		VIP_ALWAYS_INLINE VipCircularVectorConstIterator(const VipCircularVectorConstIterator& it) noexcept
		  : data(it.data)
		  , pos(it.pos)
		{
		}
		VIP_ALWAYS_INLINE VipCircularVectorConstIterator& operator=(const VipCircularVectorConstIterator& it) noexcept
		{
			data = it.data;
			pos = it.pos;
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator++() noexcept -> VipCircularVectorConstIterator&
		{
			VIP_ASSERT_DEBUG(data && pos < (data->size), "increment iterator already at the end");
			++pos;
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator++(int) noexcept -> VipCircularVectorConstIterator
		{
			VipCircularVectorConstIterator _Tmp = *this;
			++(*this);
			return _Tmp;
		}
		VIP_ALWAYS_INLINE auto operator--() noexcept -> VipCircularVectorConstIterator&
		{
			VIP_ASSERT_DEBUG(data && pos > 0, "increment iterator already at the end");
			--pos;
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator--(int) noexcept -> VipCircularVectorConstIterator
		{
			VipCircularVectorConstIterator _Tmp = *this;
			--(*this);
			return _Tmp;
		}
		VIP_ALWAYS_INLINE auto operator*() const noexcept -> reference
		{
			VIP_ASSERT_DEBUG(data && pos >= 0 && pos < (data->size), "dereference invalid iterator");
			return data->at(pos);
		}
		VIP_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
		VIP_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> VipCircularVectorConstIterator&
		{
			pos += diff;
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> VipCircularVectorConstIterator&
		{
			pos -= diff;
			return *this;
		}
	};

	// Iterator for VipCircularVector
	template<class Data>
	struct VipCircularVectorIterator : public VipCircularVectorConstIterator<Data>
	{
		using base_type = VipCircularVectorConstIterator<Data>;
		using value_type = typename Data::value_type;
		using size_type = qsizetype;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type = qsizetype;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using reference = value_type&;
		using const_reference = const value_type&;

		VIP_ALWAYS_INLINE VipCircularVectorIterator() noexcept {}
		VIP_ALWAYS_INLINE VipCircularVectorIterator(const Data* d, qsizetype p) noexcept
		  : base_type(d, p)
		{
		}
		VIP_ALWAYS_INLINE VipCircularVectorIterator(const VipCircularVectorConstIterator<Data>& it) noexcept
		  : base_type(it)
		{
		}
		VIP_ALWAYS_INLINE VipCircularVectorIterator(const VipCircularVectorIterator& it) noexcept
		  : base_type(it)
		{
		}
		VIP_ALWAYS_INLINE VipCircularVectorIterator& operator=(const VipCircularVectorIterator& it) noexcept { return static_cast<VipCircularVectorIterator&>(base_type::operator=(it)); }
		VIP_ALWAYS_INLINE auto operator*() noexcept -> reference { return const_cast<reference>(base_type::operator*()); }
		VIP_ALWAYS_INLINE auto operator->() noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
		VIP_ALWAYS_INLINE auto operator*() const noexcept -> reference { return const_cast<reference>(base_type::operator*()); }
		VIP_ALWAYS_INLINE auto operator->() const noexcept -> const_pointer { return std::pointer_traits<const_pointer>::pointer_to(**this); }
		VIP_ALWAYS_INLINE auto operator++() noexcept -> VipCircularVectorIterator&
		{
			base_type::operator++();
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator++(int) noexcept -> VipCircularVectorIterator
		{
			VipCircularVectorIterator _Tmp = *this;
			base_type::operator++();
			return _Tmp;
		}
		VIP_ALWAYS_INLINE auto operator--() noexcept -> VipCircularVectorIterator&
		{
			base_type::operator--();
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator--(int) noexcept -> VipCircularVectorIterator
		{
			VipCircularVectorIterator _Tmp = *this;
			base_type::operator--();
			return _Tmp;
		}
		VIP_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> VipCircularVectorIterator&
		{
			base_type::operator+=(diff);
			return *this;
		}
		VIP_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> VipCircularVectorIterator&
		{
			base_type::operator-=(diff);
			return *this;
		}
	};

	template<class BucketMgr>
	VIP_ALWAYS_INLINE auto operator+(const VipCircularVectorConstIterator<BucketMgr>& it,
					 typename VipCircularVectorConstIterator<BucketMgr>::difference_type diff) noexcept -> VipCircularVectorConstIterator<BucketMgr>
	{
		VipCircularVectorConstIterator<BucketMgr> res = it;
		res += diff;
		return res;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE auto operator+(const VipCircularVectorIterator<BucketMgr>& it,
					 typename VipCircularVectorIterator<BucketMgr>::difference_type diff) noexcept -> VipCircularVectorIterator<BucketMgr>
	{
		VipCircularVectorIterator<BucketMgr> res = it;
		res += diff;
		return res;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE auto operator-(const VipCircularVectorConstIterator<BucketMgr>& it,
					 typename VipCircularVectorConstIterator<BucketMgr>::difference_type diff) noexcept -> VipCircularVectorConstIterator<BucketMgr>
	{
		VipCircularVectorConstIterator<BucketMgr> res = it;
		res -= diff;
		return res;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE auto operator-(const VipCircularVectorIterator<BucketMgr>& it,
					 typename VipCircularVectorIterator<BucketMgr>::difference_type diff) noexcept -> VipCircularVectorIterator<BucketMgr>
	{
		VipCircularVectorIterator<BucketMgr> res = it;
		res -= diff;
		return res;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE qsizetype operator-(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return it1.pos - it2.pos;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE bool operator==(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return it1.pos == it2.pos;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE bool operator!=(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return it1.pos != it2.pos;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE bool operator<(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return (it1.pos) < (it2.pos);
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE bool operator>(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return (it1.pos) > (it2.pos);
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE bool operator<=(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return it1.pos <= it2.pos;
	}
	template<class BucketMgr>
	VIP_ALWAYS_INLINE bool operator>=(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept
	{
		return it1.pos >= it2.pos;
	}

	// Check if class is constructible with provided arguments

	template<class type, class... Args>
	class constructible_from
	{
		template<class C>
		static C arg();

		template<typename U>
		static std::true_type constructible_test(U*, decltype(U(arg<Args>()...))* = 0);
		static std::false_type constructible_test(...);

	public:
		using result = decltype(constructible_test(static_cast<type*>(nullptr)));
	};

	template<class type, class iter>
	struct constructible_from_iterators : constructible_from<type, iter, iter>::result
	{
	};

	template<class Vector, class To, bool FromIters = constructible_from_iterators<To, typename Vector::const_iterator>::value>
	struct ConvertCircularVector
	{
		static To convert(const Vector& v) { return To(v.begin(), v.end()); }
		static To move(Vector& v) { return To(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end())); }
	};
	template<class Vector, class To>
	struct ConvertCircularVector<Vector, To, false>
	{
		static To convert(const Vector& v)
		{
			To res;
			for (auto it = v.begin(); it != v.end(); ++it)
				res.push_back(*it);
			return res;
		}
		static To move(Vector& v)
		{
			To res;
			auto end = v.end();
			for (auto it = v.begin(); it != end; ++it)
				res.push_back(std::move(*it));
			return res;
		}
	};

	// Check for input iterator
	template<typename Iterator>
	using IfIsInputIterator = typename std::enable_if<std::is_convertible<typename std::iterator_traits<Iterator>::iterator_category, std::input_iterator_tag>::value, bool>::type;
}

/// @brief Circular buffer (or ring buffer) class.
///
/// VipCircularVector is a circular buffer-like class with an interface
/// similar to QList, QVector or std::deque, and using Copy On Write
/// like all Qt containers.
///
/// Unlike traditional circular buffer implementations, VipCircularVector
/// is not limited to a predifined capacity and will grow on insertion
/// using a power of 2 growing strategy.
///
/// Its is the container of choice for queues as it will almost always
/// outperform std::deque or QVector (Qt6 version) for back and front operations.
///
/// Like QVector, VipCircularVector never reduces its memory footprint except
/// when calling shrink_to_fit() or on copy assignment.
///
template<class T, Vip::Ownership O = Vip::SharedOwnership>
class VipCircularVector
{
	using Data = detail::CircularBuffer<T, O>;
	using DataPtr = detail::COWPointer<Data, O>;
	DataPtr d_data;

	VIP_ALWAYS_INLINE bool hasData() const noexcept { return d_data.constData() != nullptr; }

	template<class... Args>
	VIP_ALWAYS_INLINE Data* make_data(Args&&... args)
	{
		Data* d;
		if (!hasData())
			d_data.reset(d = new Data(std::forward<Args>(args)...));
		else
			d = d_data.data();
		return d;
	}
	VIP_ALWAYS_INLINE const Data* constData() const noexcept { return d_data.constData(); }
	VIP_ALWAYS_INLINE const Data* data() const noexcept { return d_data.data(); }
	VIP_ALWAYS_INLINE Data* dataNoDetach() noexcept { return const_cast<Data*>(constData()); }
	VIP_ALWAYS_INLINE Data* data() { return d_data.data(); }
	VIP_ALWAYS_INLINE bool full() const noexcept { return !d_data || d_data->size == d_data->capacity; }

	Data* adjust_capacity_for_size(qsizetype size, bool allow_shrink = false)
	{
		d_data.detach();
		qsizetype new_capacity = capacityForSize(size);
		if (!constData() || dataNoDetach()->capacity != new_capacity) {
			if (constData() && dataNoDetach()->capacity > new_capacity && !allow_shrink)
				return dataNoDetach();
			Data* res = nullptr;
			std::unique_ptr<Data> tmp(res = new Data(new_capacity));
			if (d_data)
				d_data->relocate(res);
			d_data.reset(tmp.release());
			return res;
		}
		return dataNoDetach();
	}

	qsizetype capacityForSize(qsizetype size) const noexcept
	{
		if (size <= 0)
			return 0;
		// next power of 2
		auto s = 1ull << vipBitScanReverse64((uint64_t)size);
		if (s < (uint64_t)size)
			s = s << 1;
		return s;
	}

	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace_back_no_detach(Args&&... args)
	{
		if (full())
			adjust_capacity_for_size(size() + 1);
		return *dataNoDetach()->emplace_back(std::forward<Args>(args)...);
	}
	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace_front_no_detach(Args&&... args)
	{
		if (full())
			adjust_capacity_for_size(size() + 1);
		return *dataNoDetach()->emplace_front(std::forward<Args>(args)...);
	}
	VIP_ALWAYS_INLINE void detach() { d_data.detach(); }

	// Insert range for non random-access iterators
	template<class Iter, class Cat>
	void insert_cat(qsizetype pos, Iter first, Iter last, Cat /*unused*/)
	{
		VIP_ASSERT_DEBUG(pos <= size(), "invalid insert position");
		if (first == last)
			return;

		if (pos < size() / 2) {
			// push front values
			qsizetype prev_size = size();
			// Might throw, fine
			for (; first != last; ++first)
				emplace_front_no_detach(*first);

			qsizetype num = size() - prev_size;
			// Might throw, fine
			std::reverse(begin(), begin() + num); // flip new stuff in place
			std::rotate(begin(), begin() + num, begin() + (num + pos));
		}
		else {
			// push back, might throw
			qsizetype prev_size = size();
			for (; first != last; ++first)
				emplace_back_no_detach(*first);
			// Might throw, fine
			std::rotate(begin() + pos, begin() + prev_size, end());
		}
	}
	// Insert range for random-access iterators
	template<class Iter>
	void insert_cat(qsizetype pos, Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
	{
		VIP_ASSERT_DEBUG(pos <= size(), "invalid insert position");
		if (first == last)
			return;

		if (pos < size() / 2) {
			qsizetype to_insert = static_cast<qsizetype>(last - first);
			// Might throw, fine
			resize_front(size() + to_insert);
			iterator beg = begin();
			// Might throw, fine
			for_each(to_insert, to_insert + pos, [&](T& v) {
				*beg = std::move(v);
				++beg;
			});
			for_each(pos, pos + to_insert, [&](T& v) { v = *first++; });
		}
		else {
			// Might throw, fine
			qsizetype to_insert = static_cast<qsizetype>(last - first);
			resize(size() + to_insert);
			std::move_backward(begin() + pos, begin() + static_cast<qsizetype>(size() - to_insert), end());
			std::copy(first, last, begin() + (pos));
		}
	}

	template<class Iter>
	static constexpr bool isRandomAccess(Iter)
	{
		return std::is_same<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>::value;
	}

public:
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using const_pointer = const T*;
	using const_reference = const T&;
	using size_type = qsizetype;
	using difference_type = qsizetype;
	using iterator = detail::VipCircularVectorIterator<Data>;
	using const_iterator = detail::VipCircularVectorConstIterator<Data>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	using span_type = VipSpan<T>;
	using const_span_type = VipSpan<const T>;
	using spans_type = std::pair<span_type, span_type>;
	using const_spans_type = std::pair<const_span_type, const_span_type>;

	VipCircularVector() noexcept {}
	VipCircularVector(const VipCircularVector&) noexcept = default;
	VipCircularVector(VipCircularVector&&) noexcept = default;
	VipCircularVector& operator=(const VipCircularVector&) = default;
	VipCircularVector& operator=(VipCircularVector&&) = default;

	VipCircularVector(qsizetype size)
	  : d_data(new Data(capacityForSize(size), size))
	{
	}
	VipCircularVector(qsizetype size, const T& value)
	  : d_data(new Data(capacityForSize(size), size, value))
	{
	}
	template<class Iter, detail::IfIsInputIterator<Iter> = true>
	VipCircularVector(Iter first, Iter last)
	{
		if (isRandomAccess(first)) {
			qsizetype size = (qsizetype)std::distance(first, last);
			make_data(capacityForSize(size), size);
			std::copy(first, last, begin());
		}
		else {
			for (; first != last; ++first) {
				emplace_back_no_detach(*first);
			}
		}
	}
	VipCircularVector(std::initializer_list<T> init)
	  : VipCircularVector(init.begin(), init.end())
	{
	}
	template<class U>
	VipCircularVector(const QVector<U>& v)
	  : VipCircularVector(v.begin(), v.end())
	{
	}
	~VipCircularVector() noexcept = default;

	template<class Container>
	Container convertTo() const
	{
		return detail::ConvertCircularVector<VipCircularVector, Container>::convert(*this);
	}
	template<class Container>
	Container moveTo()
	{
		auto r = detail::ConvertCircularVector<VipCircularVector, Container>::move(*this);
		clear();
		return r;
	}
	QVector<T> toVector() const { return convertTo<QVector<T>>(); }
	QList<T> toList() const { return convertTo<QList<T>>(); }

	void clear() noexcept
	{
		if (hasData()) 
			d_data = DataPtr();
	}
	void shrink_to_fit()
	{
		if (hasData())
			adjust_capacity_for_size(size(), true);
	}
	void reserve(qsizetype new_capacity) { adjust_capacity_for_size(new_capacity); }

	VIP_ALWAYS_INLINE qsizetype max_size() const noexcept { return std::numeric_limits<difference_type>::max(); }
	VIP_ALWAYS_INLINE bool empty() const noexcept { return !d_data || d_data->size == 0; }
	VIP_ALWAYS_INLINE bool isEmpty() const noexcept { return !d_data || d_data->size == 0; }
	VIP_ALWAYS_INLINE qsizetype size() const noexcept { return d_data ? d_data->size : 0; }

	VIP_ALWAYS_INLINE T& operator[](qsizetype i) noexcept { return d_data->at(i); }
	VIP_ALWAYS_INLINE const T& operator[](qsizetype i) const noexcept { return d_data->at(i); }

	VIP_ALWAYS_INLINE T& at(size_type pos)
	{
		if (pos >= size())
			throw std::out_of_range("");
		return (d_data->at(pos));
	}
	VIP_ALWAYS_INLINE const T& at(size_type pos) const
	{
		if (pos >= size())
			throw std::out_of_range("");
		return (d_data->at(pos));
	}

	VIP_ALWAYS_INLINE T& front() noexcept { return d_data->front(); }
	VIP_ALWAYS_INLINE const T& front() const noexcept { return d_data->front(); }
	VIP_ALWAYS_INLINE T& first() noexcept { return d_data->front(); }
	VIP_ALWAYS_INLINE const T& first() const noexcept { return d_data->front(); }

	VIP_ALWAYS_INLINE T& back() noexcept { return d_data->back(); }
	VIP_ALWAYS_INLINE const T& back() const noexcept { return d_data->back(); }
	VIP_ALWAYS_INLINE T& last() noexcept { return d_data->back(); }
	VIP_ALWAYS_INLINE const T& last() const noexcept { return d_data->back(); }

	VIP_ALWAYS_INLINE iterator begin() noexcept { return iterator(data(), 0); }
	VIP_ALWAYS_INLINE const_iterator begin() const noexcept { return const_iterator(data(), 0); }
	VIP_ALWAYS_INLINE const_iterator cbegin() const noexcept { return const_iterator(data(), 0); }

	VIP_ALWAYS_INLINE iterator end() noexcept { return iterator(data(), size()); }
	VIP_ALWAYS_INLINE const_iterator end() const noexcept { return const_iterator(data(), size()); }
	VIP_ALWAYS_INLINE const_iterator cend() const noexcept { return const_iterator(data(), size()); }

	VIP_ALWAYS_INLINE reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	VIP_ALWAYS_INLINE const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	VIP_ALWAYS_INLINE const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

	VIP_ALWAYS_INLINE reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
	VIP_ALWAYS_INLINE const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	VIP_ALWAYS_INLINE const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

	VIP_ALWAYS_INLINE spans_type spans(qsizetype first, qsizetype last) noexcept { return d_data->spans(first, last); }
	VIP_ALWAYS_INLINE spans_type spans() noexcept { return d_data->spans(0, size()); }
	VIP_ALWAYS_INLINE const_spans_type spans(qsizetype first, qsizetype last) const noexcept { return d_data->cspans(first, last); }
	VIP_ALWAYS_INLINE const_spans_type spans() const noexcept { return d_data->cspans(0, size()); }

	template<class Fun>
	void for_each(qsizetype first, qsizetype last, Fun&& f) noexcept(noexcept(f(std::declval<T&>())))
	{
		auto s = spans(first, last);
		for (T& v : s.first)
			f(v);
		for (T& v : s.second)
			f(v);
	}
	template<class Fun>
	void for_each(qsizetype first, qsizetype last, Fun&& f) const noexcept(noexcept(f(std::declval<const T&>())))
	{
		auto s = spans(first, last);
		for (const T& v : s.first)
			f(v);
		for (const T& v : s.second)
			f(v);
	}

	void resize(qsizetype new_size) { adjust_capacity_for_size(new_size)->resize(new_size); }
	void resize(qsizetype new_size, const T& v) { adjust_capacity_for_size(new_size)->resize(new_size, v); }

	void resize_front(qsizetype new_size) { adjust_capacity_for_size(new_size)->resize_front(new_size); }
	void resize_front(qsizetype new_size, const T& v) { adjust_capacity_for_size(new_size)->resize_front(new_size, v); }

	void swap(VipCircularVector& other) noexcept { d_data.swap(other.d_data); }

	VipCircularVector mid(qsizetype start, qsizetype len = -1) const
	{
		VIP_ASSERT_DEBUG(start >= 0 && start <= size(), "");
		len = len == -1 ? size() - start : len;
		VIP_ASSERT_DEBUG(start + len <= size(), "");
		if (start == 0 && len == size())
			return *this;
		VipCircularVector res(len);
		auto data = res.d_data->buffer;
		for_each(start, start + len, [&data](const auto& v) { *data++ = v; });
		return res;
	}

	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace_back(Args&&... args)
	{
		detach();
		return emplace_back_no_detach(std::forward<Args>(args)...);
	}
	VIP_ALWAYS_INLINE void push_back(const T& v) { emplace_back(v); }
	VIP_ALWAYS_INLINE void push_back(T&& v) { emplace_back(std::move(v)); }
	VIP_ALWAYS_INLINE void append(const T& v) { emplace_back(v); }
	VIP_ALWAYS_INLINE void append(T&& v) { emplace_back(std::move(v)); }
	void append(const VipCircularVector& v) { insert(cend(), v.begin(), v.end()); }
	void append(VipCircularVector&& v) { insert(cend(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end())); }

	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace_front(Args&&... args)
	{
		detach();
		return emplace_front_no_detach(std::forward<Args>(args)...);
	}
	VIP_ALWAYS_INLINE void push_front(const T& v) { emplace_front(v); }
	VIP_ALWAYS_INLINE void push_front(T&& v) { emplace_front(std::move(v)); }

	VIP_ALWAYS_INLINE void pop_back()
	{
		VIP_ASSERT_DEBUG(size() > 0, "");
		d_data->pop_back();
	}
	VIP_ALWAYS_INLINE T pop_back_return()
	{
		VIP_ASSERT_DEBUG(size() > 0, "");
		return d_data->pop_back_return();
	}
	VIP_ALWAYS_INLINE void pop_front()
	{
		VIP_ASSERT_DEBUG(size() > 0, "");
		d_data->pop_front();
	}
	VIP_ALWAYS_INLINE T pop_front_return()
	{
		VIP_ASSERT_DEBUG(size() > 0, "");
		return d_data->pop_front_return();
	}

	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace(size_type pos, Args&&... args)
	{
		if (pos == 0)
			return emplace_front(std::forward<Args>(args)...);
		if (pos == size() - 1)
			return emplace_front(std::forward<Args>(args)...);
		if (full())
			adjust_capacity_for_size(size() + 1);
		return *d_data->emplace(pos, std::forward<Args>(args)...);
	}
	VIP_ALWAYS_INLINE void insert(size_type pos, const T& value) { emplace(pos, value); }
	VIP_ALWAYS_INLINE void insert(size_type pos, T&& value) { emplace(pos, std::move(value)); }

	template<class... Args>
	VIP_ALWAYS_INLINE iterator emplace(const_iterator it, Args&&... args)
	{
		emplace(it.pos, std::forward<Args>(args)...);
		return (iterator)cbegin() + it.pos;
	}
	VIP_ALWAYS_INLINE iterator insert(const_iterator it, const T& value) { return emplace(it, value); }
	VIP_ALWAYS_INLINE iterator insert(const_iterator it, T&& value) { return emplace(it, std::move(value)); }

	template<class Iter>
	void insert(size_type pos, Iter first, Iter last)
	{
		detach();
		insert_cat(pos, first, last, typename std::iterator_traits<Iter>::iterator_category());
	}
	template<class Iter>
	iterator insert(const_iterator it, Iter first, Iter last)
	{
		detach();
		insert_cat(it.pos, first, last, typename std::iterator_traits<Iter>::iterator_category());
		return (iterator)cbegin() + it.pos;
	}
	void insert(size_type pos, std::initializer_list<T> ilist) { return insert(pos, ilist.begin(), ilist.end()); }
	iterator insert(const_iterator pos, std::initializer_list<T> ilist) { return insert(pos, ilist.begin(), ilist.end()); }
	void insert(size_type pos, size_type count, const T& value) { insert(pos, detail::cvalue_iterator<T>(0, value), detail::cvalue_iterator<T>(count, value)); }
	iterator insert(const_iterator pos, size_type count, const T& value) { return insert(pos, detail::cvalue_iterator<T>(0, value), detail::cvalue_iterator<T>(count, value)); }

	VIP_ALWAYS_INLINE void erase(size_type pos)
	{
		VIP_ASSERT_DEBUG(pos < size(), "erase: invalid position");
		if (pos == 0)
			return pop_front();
		if (pos == size() - 1)
			return pop_back();
		d_data->erase(pos);
	}
	VIP_ALWAYS_INLINE iterator erase(const_iterator it)
	{
		erase(it.pos);
		return (iterator)cbegin() + it.pos;
	}
	void erase(size_type first, size_type last)
	{
		VIP_ASSERT_DEBUG(first <= last, "erase: invalid positions");
		VIP_ASSERT_DEBUG(last <= size(), "erase: invalid last position");
		if (first == last)
			return;
		size_type space_before = first;
		size_type space_after = size() - last;

		if (space_before < space_after) {
			auto b = begin();
			std::move_backward(b, b + first, b + last);
			resize_front(size() - (last - first));
		}
		else {
			// std::move(begin() + last, end(), begin() + first);
			iterator it = begin() + static_cast<difference_type>(first);
			for_each(last, size(), [&](T& v) {
				*it = std::move(v);
				++it;
			});
			resize(size() - (last - first));
		}
	}
	iterator erase(const_iterator first, const_iterator last)
	{
		erase(first.pos, last.pos);
		return (iterator)cbegin() + first.pos;
	}

	template<class Iter, detail::IfIsInputIterator<Iter> = true>
	VipCircularVector& assign(Iter first, Iter last)
	{
		if (isRandomAccess(first)) {
			resize(std::distance(first, last));
			std::copy(first, last, begin());
		}
		else {
			clear();
			detach();
			for (; first != last; ++first)
				emplace_back_no_detach(*first);
		}
		return *this;
	}
	VipCircularVector& assign(size_type count, const T& value) { return assign(detail::cvalue_iterator<T>(0, value), detail::cvalue_iterator<T>(count, value)); }
	VipCircularVector& assign(std::initializer_list<T> ilist) { return assign(ilist.begin(), ilist.end()); }

	template<class U>
	VipCircularVector& operator<<(U&& value)
	{
		emplace_back(std::forward<U>(value));
		return *this;
	}
	VipCircularVector& operator+=(const VipCircularVector& other)
	{
		append(other);
		return *this;
	}
	VipCircularVector& operator+=(VipCircularVector&& other)
	{
		append(std::move(other));
		return *this;
	}
};

template<class T, Vip::Ownership O>
QDataStream& operator<<(QDataStream& s, const VipCircularVector<T, O>& c)
{
	s << (qint64)c.size();
	if (s.status() != QDataStream::Ok)
		return s;
	for (const T& t : c) {
		s << t;
		if (s.status() != QDataStream::Ok)
			break;
	}

	return s;
}

template<class T, Vip::Ownership O>
QDataStream& operator>>(QDataStream& s, VipCircularVector<T, O>& c)
{
	c.clear();
	qint64 size = 0;
	s >> size;
	if (s.status() != QDataStream::Ok)
		return s;
	qsizetype n = (qsizetype)size;
	c.reserve(n);
	for (qsizetype i = 0; i < n; ++i) {
		T t;
		s >> t;
		if (s.status() != QDataStream::Ok) {
			c.clear();
			break;
		}
		c.append(t);
	}

	return s;
}

#ifdef VIP_GENERATE_TEST_FUNCTIONS

#include <qdatetime.h>
#include <iostream>
#include <random>
namespace detail
{
	static inline qint64& time_val()
	{
		static qint64 time = 0;
		return time;
	}
	static inline void tick()
	{
		time_val() = QDateTime::currentMSecsSinceEpoch();
	}
	static inline qint64 tock()
	{
		return QDateTime::currentMSecsSinceEpoch() - time_val();
	}
	template<class T>
	static inline T makeT(qsizetype v)
	{
		return static_cast<T>(v);
	}
	template<>
	static inline std::string makeT(qsizetype v)
	{
		char snum[30];
		// Convert 123 to string [buf]
		ltoa(v, snum, 10);
		return std::string(snum) + "this is a test!!!!!!";
	}
	template<class C1, class C2>
	static bool equals(const C1& c1, const C2& c2)
	{
		if (c1.size() != c2.size())
			return false;
		for (qsizetype i = 0; i < c1.size(); ++i)
			if (c1[i] != c2[i])
				return false;
		return true;
	}
	static inline void test(bool cond)
	{
		if (!cond)
			throw std::runtime_error("");
	}
	template<class C>
	static inline C make(qsizetype loop_count)
	{
		using type = typename C::value_type;
		C res;
		for (qsizetype i = 0; i < loop_count; ++i)
			res.push_back(makeT<type>(i));
		return res;
	}

	template<class T, qsizetype loop_count = 1000000>
	static inline void testVipCircularVector()
	{
		using CircularVector = VipCircularVector<T>;
		qint64 el = 0;
		{
			CircularVector v3 = { makeT<T>(0), makeT<T>(1), makeT<T>(2), makeT<T>(3) };
			v3.pop_front();
			v3.push_back(makeT<T>(0));
			v3.for_each(0, v3.size(), [](T& v) { std::cout << v << std::endl; });
		}
		{
			// test constructors
			CircularVector v;
			CircularVector v2 = v;
			CircularVector v3 = { makeT<T>(0), makeT<T>(1), makeT<T>(2) };
			QVector<T> _v3 = { makeT<T>(0), makeT<T>(1), makeT<T>(2) };
			test(equals(v3, _v3));

			CircularVector v4(10);
			QVector<T> _v4(10);

			CircularVector v5(10ull, makeT<T>(1));
			QVector<T> _v5(10ull, makeT<T>(1));
			test(equals(v5, _v5));
			// test convert/move
			test(equals(_v5, v5.convertTo<QVector<T>>()));
			test(equals(_v5, v5.moveTo<QVector<T>>()));
		}

		{
			// test push_back/front
			CircularVector v;
			QVector<T> _v;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.push_back(makeT<T>(i));
			el = tock();
			std::cout << "QVector<T> push_back " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.push_back(makeT<T>(i));
			el = tock();
			std::cout << "Circular<T> push_back " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.push_front(makeT<T>(i));
			el = tock();
			std::cout << "QVector<T> push_front " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.push_front(makeT<T>(i));
			el = tock();
			std::cout << "Circular<T> push_front " << el << std::endl;

			test(equals(v, _v));

			v.emplace_back(makeT<T>(0));
			v.append(makeT<T>(0));
			v.pop_back();
			v.pop_back();
			v.emplace_back(makeT<T>(0));
			v.pop_back_return();
			test(equals(v, _v));

			v.emplace_front(makeT<T>(0));
			v.pop_front();
			v.emplace_front(makeT<T>(0));
			v.pop_front_return();
			test(equals(v, _v));

			// test pop_back/front
			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.pop_back();
			el = tock();
			std::cout << "QVector<T> pop_back " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.pop_back();
			el = tock();
			std::cout << "Circular<T> pop_back " << el << std::endl;

			test(equals(v, _v));

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.pop_front();
			el = tock();
			std::cout << "QVector<T> pop_front " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.pop_front();
			el = tock();
			std::cout << "Circular<T> pop_front " << el << std::endl;

			test(equals(v, _v));

			// test insert at the back/front
			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.insert(_v.end(), makeT<T>(i));
			el = tock();
			std::cout << "QVector<T> insert back " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.insert(v.end(), makeT<T>(i));
			el = tock();
			std::cout << "Circular<T> insert back " << el << std::endl;
			test(equals(v, _v));

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.insert(_v.begin(), makeT<T>(i));
			el = tock();
			std::cout << "QVector<T> insert front " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.insert(v.begin(), makeT<T>(i));
			el = tock();
			std::cout << "Circular<T> insert front " << el << std::endl;
			test(equals(v, _v));

			// test clear
			v.clear();
			_v.clear();
			test(equals(v, _v));

			// test assign
			QVector<T> tmp = { makeT<T>(0), makeT<T>(1), makeT<T>(2) };
			v.assign(tmp.begin(), tmp.end());
			_v = QVector<T>(tmp.begin(), tmp.end());
			test(equals(v, _v));
			v.assign(10, makeT<T>(1));
			_v.assign(10, makeT<T>(1));
			test(equals(v, _v));

			v.clear();
			_v.clear();
			test(equals(v, _v));

			// test insert middle
			srand(0);
			qsizetype rng[8] = { (rand() & 7), -(rand() & 7), (rand() & 7), -(rand() & 7), (rand() & 7), -(rand() & 7), (rand() & 7), -(rand() & 7) };
			tick();
			for (qsizetype i = 0; i < loop_count / 100; ++i) {
				qsizetype pos = _v.size() / 2 + rng[i & 7];
				if (pos < 0)
					pos = 0;
				if (pos > _v.size())
					pos = _v.size();
				_v.insert(pos, makeT<T>(i));
				// v.insert(pos, makeT<T>(i));
				// test(equals(v, _v));
			}
			el = tock();
			std::cout << "QVector<T> insert middle " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count / 100; ++i) {
				qsizetype pos = v.size() / 2 + rng[i & 7];
				if (pos < 0)
					pos = 0;
				if (pos > v.size())
					pos = v.size();
				v.insert(pos, makeT<T>(i));
			}
			el = tock();
			std::cout << "Circular<T> insert middle " << el << std::endl;
			test(equals(v, _v));

			// test erase left side
			tick();
			_v.erase(_v.begin() + _v.size() / 8, _v.begin() + _v.size() / 2);
			el = tock();
			std::cout << "QVector<T> erase range left " << el << std::endl;

			tick();
			v.erase(v.begin() + v.size() / 8, v.begin() + v.size() / 2);
			el = tock();
			std::cout << "Circular<T> erase range left " << el << std::endl;
			test(equals(v, _v));

			// test erase right side
			tick();
			_v.erase(_v.begin() + _v.size() / 2 + _v.size() / 8, _v.begin() + _v.size() - _v.size() / 8);
			el = tock();
			std::cout << "QVector<T> erase range right " << el << std::endl;

			tick();
			v.erase(v.begin() + v.size() / 2 + v.size() / 8, v.begin() + v.size() - v.size() / 8);
			el = tock();
			std::cout << "Circular<T> erase range right " << el << std::endl;
			test(equals(v, _v));

			_v = make<QVector<T>>(loop_count);
			v = make<CircularVector>(loop_count);
			test(equals(v, _v));

			// test erase from back/front
			//  test pop_back/front
			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.erase(_v.begin());
			el = tock();
			std::cout << "QVector<T> erase begin " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.erase(v.begin());
			el = tock();
			std::cout << "Circular<T> erase begin " << el << std::endl;

			_v = QVector<T>(loop_count, makeT<T>(1));
			v = CircularVector(loop_count, makeT<T>(1));
			test(equals(v, _v));

			// test erase from back/front
			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				_v.erase(_v.end() - 1);
			el = tock();
			std::cout << "QVector<T> erase end " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count; ++i)
				v.erase(v.end() - 1);
			el = tock();
			std::cout << "Circular<T> erase end " << el << std::endl;

			_v = QVector<T>(loop_count / 100, makeT<T>(1));
			v = CircularVector(loop_count / 100, makeT<T>(1));
			test(equals(v, _v));

			// test erase middle
			tick();
			for (qsizetype i = 0; i < loop_count / 1000; ++i) {
				qsizetype pos = _v.size() / 2 + rng[i & 7];
				if (pos < 0)
					pos = 0;
				if (pos >= _v.size())
					pos = _v.size() - 1;
				_v.erase(_v.cbegin() + pos);
			}
			el = tock();
			std::cout << "QVector<T> erase middle " << el << std::endl;

			tick();
			for (qsizetype i = 0; i < loop_count / 1000; ++i) {
				qsizetype pos = v.size() / 2 + rng[i & 7];
				if (pos < 0)
					pos = 0;
				if (pos >= v.size())
					pos = v.size() - 1;
				v.erase(v.cbegin() + pos);
			}
			el = tock();
			std::cout << "Circular<T> erase middle " << el << std::endl;
			test(equals(v, _v));

			// test shuffle
			_v = make<QVector<T>>(loop_count);
			v = make<CircularVector>(loop_count);
			test(equals(v, _v));

			tick();
			std::shuffle(_v.begin(), _v.end(), std::default_random_engine(0));
			el = tock();
			std::cout << "QVector<T> shuffle " << el << std::endl;

			tick();
			std::shuffle(v.begin(), v.end(), std::default_random_engine(0));
			el = tock();
			std::cout << "Circular<T> shuffle " << el << std::endl;
			test(equals(v, _v));

			// test sort
			tick();
			std::sort(_v.begin(), _v.end());
			el = tock();
			std::cout << "QVector<T> sort " << el << std::endl;

			tick();
			std::sort(v.begin(), v.end());
			el = tock();
			std::cout << "Circular<T> sort " << el << std::endl;
			test(equals(v, _v));
		}
	}
}

#endif

#endif
