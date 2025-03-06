#include "VipConfig.h"
#include "VipMath.h"
#include <QTypeInfo>
#include <qshareddata.h>

#include <utility>
#include <type_traits>

namespace detail
{
	/// @brief Simply call p->~T(), used as a replacement to std::allocator::destroy() which was removed in C++20
	template<class T>
	VIP_ALWAYS_INLINE void destroy_ptr(T* p) noexcept
	{
		p->~T();
	}

	/// @brief Simply call new (p) T(...), used as a replacement to std::allocator::construct() which was removed in C++20
	template<class T, class... Args>
	VIP_ALWAYS_INLINE void construct_ptr(T* p, Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
	{
		new (p) T(std::forward<Args>(args)...);
	}

	template<class T>
	void destroy_range(T* p, qsizetype size)
	{
		for (qsizetype i = 0; i < size; ++i)
			destroy_ptr(p + i);
	}
	template<class T>
	void move_construct_range(T* dst, T* src, qsizetype size)
	{
		qsizetype i = 0;
		try {
			for (; i < size; ++i) {
				construct_ptr(dst + i, std::move(src[i]));
			}
		}
		catch (...) {
			for (qsizetype j = 0; j < i; ++j)
				destroy_ptr(dst + j);
			throw;
		}
	}

	// Circular buffer class used internally by tiered_vector
	// Actuall data are located in the same memory block and starts at start_data_T
	template<class T>
	struct CircularBuffer : public QSharedData
	{
		static constexpr bool relocatable = QTypeInfo<T>::isRelocatable;

		qsizetype size;	     // buffer size
		qsizetype capacity1; // buffer max size -1
		qsizetype capacity;  // buffer max size
		qsizetype begin;     // begin index of data
		T* buffer;

		// Initialize from a maximum size
		CircularBuffer(qsizetype max_size = 0)
		  : size(0)
		  , capacity1(max_size - 1)
		  , capacity(max_size)
		  , begin(0)
		  , buffer(nullptr)
		{
			VIP_ASSERT_DEBUG(max_size >= 0, "");
			if (max_size) {
				buffer = (T*)malloc((size_t)max_size * sizeof(T));
				if (!buffer)
					throw std::bad_alloc();
			}
		}
		// Initialize from a maximum size and a default value.
		// This will set the buffer size to its maximum.
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
				for (qsizetype j = 0; j < i; ++j)
					destroy_ptr(buffer + j);
				size = 0;
				throw;
			}
		}

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
				for (qsizetype j = 0; j < i; ++j)
					destroy_ptr(buffer + j);
				size = 0;
				throw;
			}
		}

		~CircularBuffer() noexcept
		{
			if (!std::is_trivially_destructible<T>::value) {
				for (qsizetype i = 0; i < size; ++i)
					destroy_ptr(&(*this)[i]);
			}
		}

		CircularBuffer& operator=(CircularBuffer&& other)
		{
			std::swap(size, other.size);
			std::swap(capacity, other.capacity);
			std::swap(capacity1, other.capacity1);
			std::swap(begin, other.begin);
			std::swap(buffer, other.buffer);
			return *this;
		}

		void relocate(CircularBuffer* dst) noexcept(std::is_nothrow_move_constructible<T>::value || relocatable)
		{
			qsizetype stop = first_stop();
			qsizetype first_range = (stop - begin);
			qsizetype remaining = size - first_range;
			if (relocatable) {
				memmove(dst->buffer, buffer + begin, (size_t)first_range * sizeof(T));
				if (remaining)
					memmove(dst->buffer + first_range, buffer, (size_t)remaining * sizeof(T));
			}
			else {
				move_construct_range(dst->buffer, buffer + begin, first_range);
				if (remaining)
					move_construct_range(dst->buffer + first_range, buffer, remaining);
			}
			dst->size = this->size;
		}

		template<class Fun>
		void for_each(Fun&& f)
		{
			qsizetype stop = first_stop();
			for (qsizetype i = begin; i != stop; ++i)
				f(buffer[i]);
			qsizetype first_range = (stop - begin);
			qsizetype i = 0;
			while (first_range++ != size)
				f(buffer[i++]);
		}

		// Pointer on the first data
		VIP_ALWAYS_INLINE auto begin_ptr() noexcept -> T* { return buffer + ((begin)&capacity1); }
		VIP_ALWAYS_INLINE auto begin_ptr() const noexcept -> const T* { return buffer + ((begin)&capacity1); }
		// Pointer on the last data
		VIP_ALWAYS_INLINE auto last_ptr() noexcept -> T* { return (buffer + ((begin + size - 1) & capacity1)); }
		VIP_ALWAYS_INLINE auto last_ptr() const noexcept -> const T* { return (buffer + ((begin + size - 1) & capacity1)); }
		// Index of the first data
		VIP_ALWAYS_INLINE auto begin_index() const noexcept -> qsizetype { return (begin)&capacity1; }
		// Index of the first stop (either at size or max size)
		VIP_ALWAYS_INLINE auto first_stop() const noexcept -> qsizetype
		{
			qsizetype p = begin_index();
			return (p + size) > max_size() ? (max_size()) : (p + size);
		}
		// Index of the second stop (either at size or max size)
		VIP_ALWAYS_INLINE auto second_stop() const noexcept -> qsizetype
		{
			qsizetype p = begin_index();
			return (p + size) > max_size() ? (((p + size) & capacity1)) : (p + size);
		}
		// Buffer maximum size
		VIP_ALWAYS_INLINE auto max_size() const noexcept -> qsizetype { return capacity; }
		// Check if full
		VIP_ALWAYS_INLINE auto isFull() const noexcept -> bool { return size == max_size(); }

		// Element access.
		VIP_ALWAYS_INLINE auto operator[](qsizetype index) noexcept -> T& { return at(index); }
		VIP_ALWAYS_INLINE auto operator[](qsizetype index) const noexcept -> const T& { return at(index); }
		VIP_ALWAYS_INLINE auto at(qsizetype index) noexcept -> T& { return buffer[((begin + index) & (capacity1))]; }
		VIP_ALWAYS_INLINE auto at(qsizetype index) const noexcept -> const T& { return buffer[((begin + index) & (capacity1))]; }

		// Front/back access
		VIP_ALWAYS_INLINE auto front() noexcept -> T& { return buffer[begin]; }
		VIP_ALWAYS_INLINE auto front() const noexcept -> const T& { return buffer[begin]; }
		VIP_ALWAYS_INLINE auto back() noexcept -> T& { return (*this)[size - 1]; }
		VIP_ALWAYS_INLINE auto back() const noexcept -> const T& { return (*this)[size - 1]; }

		// Resize buffer
		void resize(qsizetype s)
		{
			if (s < size) {
				if (!std::is_trivially_destructible<T>::value) {
					for (qsizetype i = s; i < size; ++i)
						destroy_ptr(&(*this)[i]);
				}
			}
			else if (s > size) {
				qsizetype i = size;
				// Make sure to destroy constructed values in case of exception
				try {
					for (; i < s; ++i)
						construct_ptr(&(*this)[i]);
				}
				catch (...) {
					for (qsizetype j = size; j < i; ++j)
						destroy_ptr(&(*this)[j]);
					throw;
				}
			}
			size = s;
		}
		void resize(qsizetype s, const T& value)
		{
			if (s < size) {
				if (!std::is_trivially_destructible<T>::value) {
					for (qsizetype i = s; i < size; ++i)
						destroy_ptr(&at(i));
				}
			}
			else if (s > size) {
				qsizetype i = size;
				// Make sure to destroy constructed values in case of exception
				try {
					for (; i < s; ++i)
						construct_ptr(&at(i), value);
				}
				catch (...) {
					for (qsizetype j = size; j < i; ++j)
						destroy_ptr(&at(j));
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
		VIP_ALWAYS_INLINE auto emplace_back(Args&&... args) -> T*
		{
			// only works for non full array
			T* ptr = &at(size);
			// Might throw, which is fine
			construct_ptr(ptr, std::forward<Args>(args)...);
			++size;
			return ptr;
		}

		template<class... Args>
		VIP_ALWAYS_INLINE auto try_emplace_back_safe(Args&&... args) -> T*
		{
			std::atomic<qsizetype>& _size = reinterpret_cast<std::atomic<qsizetype>&>(size);
			qsizetype prev = _size.fetch_add(1);
			if (prev > size)
				return nullptr;
			// only works for non full array
			T* ptr = &at(prev);
			// Might throw, which is fine
			try {

				construct_ptr(ptr, std::forward<Args>(args)...);
			}
			catch (...) {
				--size;
				throw;
			}
			return ptr;
		}

		template<class... Args>
		auto emplace_front(Args&&... args) -> T*
		{
			// only works for non full array
			if (--begin < 0)
				begin = capacity1;
			try {
				construct_ptr(buffer + begin, std::forward<Args>(args)...);
			}
			catch (...) {
				begin = (begin + 1) & capacity1;
				throw;
			}
			++size;
			return buffer + begin;
		}

		// Pushing front while poping back value
		// Might throw, but leave the buffer in a valid state
		auto push_front_pop_back(T&& value) noexcept(std::is_nothrow_move_assignable<T>::value) -> T
		{
			// Only works for filled array
			T res = std::move(back());
			if (--begin < 0)
				begin = capacity1;
			buffer[begin] = std::move(value);
			return (res);
		}
		auto push_front_pop_back(const T& value) noexcept(std::is_nothrow_move_assignable<T>::value) -> T
		{
			// Only works for filled array
			T res = std::move(back());
			if (--begin < 0)
				begin = capacity1;
			buffer[begin] = (value);
			return (res);
		}

		// Pushing front while poping back
		// Might throw, but leave the buffer in a valid state since it is already full
		auto push_back_pop_front(T&& value) noexcept(std::is_nothrow_move_assignable<T>::value) -> T
		{
			// Only works for filled array
			T res = std::move(front());
			begin = (begin + 1) & (capacity1);
			(*this)[size - 1] = std::move(value);
			return (res);
		}
		auto push_back_pop_front(const T& value) noexcept(std::is_nothrow_move_assignable<T>::value) -> T
		{
			// Only works for filled array
			T res = std::move(front());
			begin = (begin + 1) & (capacity1);
			(*this)[size - 1] = (value);
			return (res);
		}

		// Pop back/front
		void pop_back() noexcept
		{
			if (!std::is_trivially_destructible<T>::value)
				destroy_ptr(&back());
			--size;
		}
		VIP_ALWAYS_INLINE void pop_front() noexcept
		{
			if (!std::is_trivially_destructible<T>::value)
				destroy_ptr(buffer + begin);
			if (++begin == capacity)
				begin = 0;
			--size;
		}
		VIP_ALWAYS_INLINE T pop_front_return() noexcept
		{
			T r = std::move(buffer[begin]);
			if (++begin == capacity)
				begin = 0;
			--size;
			return std::move(r);
		}

		// Pop front n values
		void pop_front_n(qsizetype n) noexcept
		{
			for (qsizetype i = 0; i != n; ++i)
				pop_front();
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
						begin = capacity1;
					construct_ptr(buffer + begin);
					++size;
				}
			}
			catch (...) {
				begin = (begin + 1) & capacity1;
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

			if (!relocatable) {
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
				if (!relocatable)
					*ptr1 = std::move(*(buffer + capacity1));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer + capacity1), sizeof(T));
				ptr1 = (buffer + capacity1);
				stop = &at(pos);
				if (!relocatable) {
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
			// T* ptr2 = ptr1 + 1;
			T* stop = buffer + ((begin + pos - 1) & capacity1); //&at(pos);
			if (stop < ptr1)
				stop = buffer + capacity1;
			if (!relocatable) {
				while (ptr1 < stop) {
					*ptr1 = std::move(ptr1[1]);
					++ptr1;
				}
			}
			else {
				memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T));
				ptr1 = stop;
			}
			if (ptr1 != buffer + ((begin + pos - 1) & capacity1)) {
				if (!relocatable)
					*ptr1 = std::move(*(buffer));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer), sizeof(T));
				ptr1 = buffer;
				stop = &at(pos - 1);
				if (!relocatable) {
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
			if (!relocatable)
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
			if (!relocatable)
				construct_ptr(&(*this)[begin ? -1 : capacity1]);
			// move elems before pos
			if (--begin < 0)
				begin = capacity1;
			++size;
			// Might throw, fine since no more values are constructed/destructed
			move_left_1(pos + 1);
			// Non optimized way:
			// for (qsizetype i = 0; i < pos -1; ++i)
			//	(*this)[i] = std::move((*this)[i + 1]);
		}

		// Insert value at given position. Only works if the buffer is not full.
		template<class... Args>
		auto emplace(qsizetype pos, Args&&... args) -> T*
		{
			VIP_ASSERT_DEBUG(size != capacity, "cannot insert in a full circular buffer");
			if (pos > size / 2) {
				move_right(pos);
			}
			else {
				move_left(pos);
			}

			T* res = &(*this)[pos];
			if (relocatable) {
				try {
					construct_ptr(res, std::forward<Args>(args)...);
				}
				catch (...) {
					// No choice but to destroy all values after pos and reduce the size in order to leave the buffer in a valid state
					for (int i = pos + 1; i < size; ++i)
						destroy_ptr(&(*this)[i]);
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
				stop = buffer + capacity1;
			if (!relocatable) {
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
				if (!relocatable)
					*ptr1 = std::move(*(buffer));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer), sizeof(T));
				ptr1 = buffer;
				stop = &at(size);
				if (!relocatable) {
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
			if (!relocatable) {
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
				if (!relocatable)
					*ptr1 = std::move(*(buffer + capacity1));
				else
					memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer + capacity1), sizeof(T));
				ptr1 = (buffer + capacity1);
				stop = &at(0);
				if (!relocatable) {
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
			if (relocatable)
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
				if (!relocatable)
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
				if (!relocatable)
					destroy_ptr(&(*this)[0]);
				begin = (begin + 1) & (capacity1); // increment begin
			}
		}
	};

}

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

	Data* data;
	difference_type pos;

	VIP_ALWAYS_INLINE VipCircularVectorConstIterator() noexcept {}
	VIP_ALWAYS_INLINE VipCircularVectorConstIterator(const Data* d, difference_type p) noexcept
	  : data(const_cast<Data*>(d))
	  , pos(p)
	{
	}
	VipCircularVectorConstIterator(const VipCircularVectorConstIterator&) noexcept = default;
	VipCircularVectorConstIterator& operator=(const VipCircularVectorConstIterator&) noexcept = default;

	VIP_ALWAYS_INLINE auto operator++() noexcept -> VipCircularVectorConstIterator&
	{
		VIP_ASSERT_DEBUG(pos < static_cast<difference_type>(data->size), "increment iterator already at the end");
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
		VIP_ASSERT_DEBUG(pos > 0, "increment iterator already at the end");
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
		VIP_ASSERT_DEBUG(pos >= 0 && pos < static_cast<difference_type>(data->size), "dereference invalid iterator");
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

// Deque iterator class
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
	VipCircularVectorIterator(const VipCircularVectorIterator&) noexcept = default;
	VipCircularVectorIterator& operator=(const VipCircularVectorIterator&) noexcept = default;

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
VIP_ALWAYS_INLINE auto operator+(const VipCircularVectorIterator<BucketMgr>& it, typename VipCircularVectorIterator<BucketMgr>::difference_type diff) noexcept -> VipCircularVectorIterator<BucketMgr>
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
VIP_ALWAYS_INLINE auto operator-(const VipCircularVectorIterator<BucketMgr>& it, typename VipCircularVectorIterator<BucketMgr>::difference_type diff) noexcept -> VipCircularVectorIterator<BucketMgr>
{
	VipCircularVectorIterator<BucketMgr> res = it;
	res -= diff;
	return res;
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator-(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept ->
  typename VipCircularVectorConstIterator<BucketMgr>::difference_type
{
	return it1.pos - it2.pos;
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator==(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept -> bool
{
	return it1.pos == it2.pos;
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator!=(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept -> bool
{
	return it1.pos != it2.pos;
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator<(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept -> bool
{
	return (it1.pos) < (it2.pos);
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator>(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept -> bool
{
	return (it1.pos) > (it2.pos);
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator<=(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept -> bool
{
	return it1.pos <= it2.pos;
}
template<class BucketMgr>
VIP_ALWAYS_INLINE auto operator>=(const VipCircularVectorConstIterator<BucketMgr>& it1, const VipCircularVectorConstIterator<BucketMgr>& it2) noexcept -> bool
{
	return it1.pos >= it2.pos;
}

template<class T>
class VipCircularVector
{
	using Data = detail::CircularBuffer<T>;
	using DataPtr = QSharedDataPointer<Data>;
	DataPtr d_data;

	VIP_ALWAYS_INLINE bool hasData() const noexcept { return d_data.constData() != nullptr; }

	template<class... Args>
	VIP_ALWAYS_INLINE Data* make_data(Args&&... args)
	{
		Data* d;
		if (!hasData())
			d_data.reset(d = new Data(std::forward<Args>(args)...));
		else
			d = d_data.get();
		return d;
	}
	VIP_ALWAYS_INLINE const Data* constData() const noexcept { return d_data.constData(); }
	VIP_ALWAYS_INLINE const Data* data() const noexcept { return d_data.data(); }
	VIP_ALWAYS_INLINE Data* data() const noexcept { return d_data.data(); }
	VIP_ALWAYS_INLINE bool full() const noexcept { return !d_data || d_data->size == d_data->capacity; }

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

public:
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using const_pointer = const T*;
	using const_reference = const T&;
	using size_type = qsizetype;
	using difference_type = qsizetype;
	using iterator = VipCircularVectorIterator<Data>;
	using const_iterator = VipCircularVectorConstIterator<Data>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

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
	template<class Iter>
	VipCircularVector(Iter first, Iter last)
	{
		if (std::iterator_traits<Iter>::is_random_access) {
			qsizetype size = (qsizetype)std::distance(first, last);
			make_data(capacityForSize(size), size);
			std::copy(first, last, begin());
		}
		else {
			make_data();
			for (; first != last; ++first)
				push_back(*first);
		}
	}
	VipCircularVector(std::initializer_list<T> init)
	  : VipCircularVector(init.begin(), init.end())
	{
	}
	~VipCircularVector() noexcept { clear(); }

	void clear() noexcept
	{
		if (hasData())
			d_data->resize(0);
	}
	void shrink_to_fit()
	{
		if (hasData()) {
			if (d_data->capacity != capacityForSize(size())) {
				DataPtr d(new Data(capacityForSize(size())));
				if (d_data)
					d_data->relocate(&d);
				d_data = d;
			}
		}
	}
	VIP_ALWAYS_INLINE qsizetype max_size() const noexcept { return std::numeric_limits<difference_type>::max(); }
	VIP_ALWAYS_INLINE bool empty() const noexcept { return !d_data || d_data->size == 0; }
	VIP_ALWAYS_INLINE qsizetype size() const noexcept { return d_data ? d_data->size : 0; }

	VIP_ALWAYS_INLINE T& operator[](qsizetype i) noexcept { return d_data->at(i); }
	VIP_ALWAYS_INLINE const T& operator[](qsizetype i) const noexcept { return d_data->at(i); }

	VIP_ALWAYS_INLINE T& front() noexcept { return d_data->front(); }
	VIP_ALWAYS_INLINE const T& front() const noexcept { return d_data->front(); }

	VIP_ALWAYS_INLINE T& back() noexcept { return d_data->front(); }
	VIP_ALWAYS_INLINE const T& back() const noexcept { return d_data->front(); }

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

	template<class Fun>
	void for_each(Fun&& fun)
	{
		if (hasData())
			d_data->for_each(fun);
	}

	void resize(qsizetype new_size) { make_data()->resize(new_size); }
	void resize(qsizetype new_size, const T& v) { make_data()->resize(new_size, v); }

	void resize_front(qsizetype new_size) { make_data()->resize_front(new_size); }
	void resize_front(qsizetype new_size, const T& v) { make_data()->resize_front(new_size, v); }

	void swap(VipCircularVector& other) noexcept { d_data.swap(other.d_data); }

	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace_back(Args&&... args)
	{
		if (full()) {
			DataPtr d(new Data(capacityForSize(size() + 1)));
			if (d_data)
				d_data->relocate(&d);
			d_data = d;
		}
		return *d_data->emplace_back(std::forward<Args>(args)...);
	}
	VIP_ALWAYS_INLINE void push_back(const T& v) { emplace_back(v); }
	VIP_ALWAYS_INLINE void push_back(T&& v) { emplace_back(std::move(v)); }
	VIP_ALWAYS_INLINE void append(const T& v) { emplace_back(v); }
	VIP_ALWAYS_INLINE void append(T&& v) { emplace_back(std::move(v)); }

	template<class... Args>
	VIP_ALWAYS_INLINE T& emplace_front(Args&&... args)
	{
		if (full()) {
			DataPtr d(new Data(capacityForSize(size() + 1)));
			if (d_data)
				d_data->relocate(&d);
			d_data = d;
		}
		return *d_data->emplace_front(std::forward<Args>(args)...);
	}
	VIP_ALWAYS_INLINE void push_front(const T& v) { emplace_front(v); }
	VIP_ALWAYS_INLINE void push_front(T&& v) { emplace_front(std::move(v)); }

	VIP_ALWAYS_INLINE void pop_back()
	{
		VIP_ASSERT_DEBUG(size() > 0, "");
		d_data->pop_back();
	}
	VIP_ALWAYS_INLINE void pop_front()
	{
		VIP_ASSERT_DEBUG(size() > 0, "");
		d_data->pop_front();
	}

	template<class... Args>
	T& emplace(size_type pos, Args&&... args)
	{
		return *make_data()->insert(pos, std::forward<Args>(args)...);
	}
	void insert(size_type pos, const T& value) { emplace(pos, value); }
	void insert(size_type pos, T&& value) { emplace(pos, std::move(value)); }
	iterator insert(const_iterator it, const T& value) -> iterator
	{
		insert(it.pos, value);
		return begin() + it.pos;
	}

	/// @brief Insert \a value before \a it using move semantic.
	/// @param it iterator within the tiered_vector
	/// @param value element to insert
	/// Basic exception guarantee.
	auto insert(const_iterator it, T&& value) -> iterator
	{
		size_type pos = it.absolutePos();
		insert(pos, std::move(value));
		return iterator_at(pos);
	}
};
