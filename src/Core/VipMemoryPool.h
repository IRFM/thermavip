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

#ifndef VIP_MEMORY_POOL_H
#define VIP_MEMORY_POOL_H

#include "VipLock.h"
#include "VipConfig.h"

/// @brief Returns the OS allocation granularity in bytes.
/// The allocation granularity is usually equals to the page size,
/// except on Windows where it is greater.
VIP_CORE_EXPORT size_t vipOSAllocationGranularity() noexcept;

/// @brief Returns the OS page size in bytes
VIP_CORE_EXPORT size_t vipOSPageSize() noexcept;

/// @brief Allocate pages
VIP_CORE_EXPORT void* vipOSAllocatePages(size_t pages) noexcept;

/// @brief Deallocate pages
/// Release pages on Windows, munmap otherwise.
VIP_CORE_EXPORT bool vipOSFreePages(void* p, size_t pages) noexcept;

// Forward declaration
template<class T, class Lock>
class VipMemoryPool;

/// @brief Null lock that can be used with VipMemoryPool
/// for monothreaded allocation/deallocation
class VipNullLock
{
public:
	void lock() {}
	void unlock() {}
};

namespace detail
{

	struct alignas(std::uint64_t) SmallBlockHeader
	{
		std::uint16_t tail = 0;
		std::uint16_t first_free = 0;
		std::uint16_t objects = 0;
		std::uint16_t tail_end = 0;
	};

	/// Contiguous block of memory used to allocate chunks for a specific size class.
	///
	/// TinyBlockPool uses a singly linked list of free slots combined with a bump pointer.
	///
	/// TinyBlockPool is always aligned on a power of 2 to remove
	/// the need for an allocation header and reduce the memory footprint of small allocations.
	///
	template<class T, class Lock>
	struct alignas(alignof(T) > alignof(uint64_t) ? alignof(T) : alignof(uint64_t)) TinyBlockPool
	{
		using value_type = T;
		using parent_type = VipMemoryPool<T, Lock>;

		union Union
		{
			uint64_t address;
			alignas(T) char data[sizeof(T)];
		};

		SmallBlockHeader header;
		TinyBlockPool* left{ nullptr };	     // Linked list of free TinyBlockPool
		TinyBlockPool* right{ nullptr };     // Linked list of free TinyBlockPool
		TinyBlockPool* left_all{ nullptr };  // Linked list of TinyBlockPool
		TinyBlockPool* right_all{ nullptr }; // Linked list of TinyBlockPool
		parent_type* parent{ nullptr };	     // Parent VipMemoryPool

		static constexpr unsigned sizeof_T = sizeof(Union);

		// Default ctor
		TinyBlockPool() noexcept
		{
			left = right = this;
			left_all = right_all = this;
		}

		// Construct from parent pool
		TinyBlockPool(parent_type* p, unsigned max_objects) noexcept
		  : parent(p)
		{
			this->header.tail = sizeof(TinyBlockPool);
			this->header.first_free = (this->header.tail);
			this->header.tail_end = this->header.tail + (uint16_t)(max_objects * sizeof_T);
		}

		// Support for linked list of TinyBlockPool
		void insert(TinyBlockPool* l, TinyBlockPool* r) noexcept
		{
			this->left = l;
			this->right = r;
			l->right = r->left = (this);
		}
		void insert_all(TinyBlockPool* l, TinyBlockPool* r) noexcept
		{
			this->left_all = l;
			this->right_all = r;
			l->right_all = r->left_all = (this);
		}
		void remove() noexcept
		{
			// MICRO_ASSERT_DEBUG(this->left != nullptr, "");
			this->left->right = this->right;
			this->right->left = this->left;
			this->left = this->right = nullptr;
		}
		void remove_all() noexcept
		{
			// MICRO_ASSERT_DEBUG(this->left != nullptr, "");
			this->left_all->right_all = this->right_all;
			this->right_all->left_all = this->left_all;
			this->left_all = this->right_all = nullptr;
		}

		bool end() const noexcept { return right == this; }
		bool end_all() const noexcept { return right_all == this; }

		VIP_ALWAYS_INLINE char* start() noexcept { return (char*)(this); }

		// Allocate one object
		VIP_ALWAYS_INLINE T* allocate() noexcept
		{
			// first_free is 0 when the pool is full
			if (VIP_UNLIKELY(this->header.first_free == 0))
				return nullptr;

			// MICRO_ASSERT_DEBUG(this->header.first_free < get_chunk_size(), "");
			uint64_t* res = (uint64_t*)(start() + this->header.first_free);
			// Tail case: use address bump
			if (this->header.first_free == this->header.tail) {
				unsigned new_tail = static_cast<unsigned>(this->header.tail + sizeof_T);
				// Set next address to 0 if full, new tail if not
				new_tail *= (new_tail < this->header.tail_end);
				// MICRO_ASSERT_DEBUG(new_tail <= max_objects, "");
				this->header.tail = (uint16_t)(*res = (new_tail));
			}
			this->header.first_free = (uint16_t)*res;
			// MICRO_ASSERT_DEBUG(this->header.objects < max_objects, "");
			++this->header.objects;
			return (T*)res;
		}

		// Deallocate object
		VIP_ALWAYS_INLINE bool deallocate(void* p, Lock& ll) noexcept
		{
			char* b = static_cast<char*>(p);
			auto diff = b - start();

			// Lock the parent lock for this size class
			ll.lock();

			// MICRO_ASSERT_DEBUG(this->header.first_free < get_chunk_size() && (header.first_free == 0 || header.first_free >= sizeof(TinyBlockPool) / 16), "");
			// MICRO_ASSERT_DEBUG(diff >= sizeof(TinyBlockPool) / 16 && diff < get_chunk_size(), "");

			*(uint64_t*)b = this->header.first_free;
			this->header.first_free = (uint16_t)diff;
			return --this->header.objects == 0;
		}

		VIP_ALWAYS_INLINE bool empty() const noexcept { return this->header.objects == 0; }

		VIP_ALWAYS_INLINE parent_type* get_parent() noexcept { return this->parent; }
	};

}

/// @brief A parallel memory pool class for small objects
///
/// VipMemoryPool is a thread safe memory pool that directly calls
/// OS functions to allocate/deallocate memory pages in order to
/// satisfy allocation requests.
///
/// Pages are always allocated by power of 2 for fast header retrieval,
/// which allows to store object in a contiguous manner without a per-object
/// header. This considerably reduces the memory footprint of allocated objects.
///
/// VipMemoryPool uses a standard free list mechanism for individual objects
/// as well as allocated pages, ensuring solid performances in both space
/// and time. On Windows, VipMemoryPool is typically several times faster
/// than the default memory allocator for uncontended scenarios.
///
/// Another advantage of VipMemoryPool is the possibility to wipe out previously
/// allocated memory in a single call (VipMemoryPool::clear()), without deallocating
/// individual objects. Note that VipMemoryPool destructor will always free all
/// previously allocated memory this way.
///
/// VipMemoryPool allocation and deallocation processes have a O(1) complexity,
/// modulo lock contentions.
///
/// Alignment of allocated objects is guaranteed to be at least alignof(T).
/// Note that VipMemoryPool::allocate() does NOT call the object constructor,
/// and VipMemoryPool::deallocate() does NOT call the object destructor.
///
/// Deallocating an object of type T does NOT require access to the initial
/// VipMemoryPool that performs the allocation. VipMemoryPool::deallocate()
/// is indeed a static function.
///
/// Using VipNullLock instead of VipSpinlock removes allocation/deallocation thread safety
/// but is usually faster.
///
template<class T, class Lock = VipSpinlock>
class VipMemoryPool
{
	using this_type = VipMemoryPool<T, Lock>;
	using block = detail::TinyBlockPool<T, Lock>;
	using block_it = block;
	using union_type = typename block::Union;

	static constexpr unsigned sizeof_T = sizeof(union_type);

	/// @brief Allocate from a newly created block
	VIP_NOINLINE(auto) allocate_from_new_block() noexcept -> void*
	{

		// Unlock before creating the new block
		// to avoid blocking potential deallocations
		d_data.lock.unlock();

		// Create the block
		void* pages = d_cache;
		d_cache = nullptr;
		if (!pages) {
			pages = vipOSAllocatePages(allocation_granularity() / vipOSPageSize());
			if (!pages) {
				d_data.lock.lock();
				return nullptr;
			}
		}

		block* _bl = new (pages) block(this, (allocation_granularity() - sizeof(block)) / sizeof_T);

		// Lock again, link block
		d_data.lock.lock();

		_bl->insert(static_cast<block*>(&d_data.it), d_data.it.right);
		_bl->insert_all(static_cast<block*>(&d_data.it), d_data.it.right_all);

		// MICRO_ASSERT_DEBUG(d_data.it.right != nullptr, "");
		void* r = _bl->allocate();
		return r;
	}

	/// @brief Handle complex deallocation
	static VIP_NOINLINE(void) handle_deallocate(this_type* parent, block* p) noexcept
	{
		if (p->empty()) {

			// Empty block: remove it from the linked list and deallocate
			p->remove();
			p->remove_all();

			if (!parent->d_cache) {
				// Update cache
				parent->d_cache = p;
				parent->d_data.lock.unlock();
				return;
			}

			// MICRO_ASSERT_DEBUG(parent->d_data.it.right != nullptr, "");
			parent->d_data.lock.unlock();
			vipOSFreePages(p, allocation_granularity() / vipOSPageSize());
			return;
		}

		// If the block was removed from the linked list, add it back as it now provides free slot(s)
		if (!p->left) {
			p->insert(static_cast<block*>(&p->get_parent()->d_data.it), p->get_parent()->d_data.it.right);
		}
		parent->d_data.lock.unlock();
	}

	VIP_NOINLINE(void*) allocate_from_pool_list() noexcept
	{
		block* bl = d_data.it.right;
		if (bl != &d_data.it) {
			bl->remove();
			bl = d_data.it.right;
		}
		// MICRO_ASSERT_DEBUG(bl != nullptr, "");
		while ((bl != &d_data.it)) {
			void* res = bl->allocate();
			if (VIP_LIKELY(res)) {
				return res;
			}
			else {
				auto* next = bl->right;
				bl->remove();
				bl = next;
				// MICRO_ASSERT_DEBUG(bl != nullptr, "");
			}
		}
		return nullptr;
	}

	struct It
	{
		block_it it;
		Lock lock;
	};
	It d_data;
	block* d_cache = nullptr;

	static VIP_ALWAYS_INLINE size_t allocation_granularity() noexcept
	{
		static size_t res = vipOSAllocationGranularity();
		return res;
	}

public:
	static_assert(sizeof(T) < 2000, "unsupported sizeof(T)");

	/// @brief Default constructor
	VipMemoryPool() noexcept {}
	~VipMemoryPool() noexcept { clear(); }

	VipMemoryPool(const VipMemoryPool&) = delete;
	VipMemoryPool(VipMemoryPool&&) = delete;
	VipMemoryPool& operator=(const VipMemoryPool&) = delete;
	VipMemoryPool& operator=(VipMemoryPool&&) = delete;

	/// @brief Allocate object of minimum size sizeof(T).
	/// Returns nullptr on failure.
	VIP_ALWAYS_INLINE T* allocate() noexcept
	{
		std::lock_guard<Lock> ll(d_data.lock);

		void* res = d_data.it.right->allocate();
		if (VIP_LIKELY(res))
			return (T*)res;
		if ((res = allocate_from_pool_list()))
			return (T*)res;
		return (T*)allocate_from_new_block();
	}

	/// @brief Deallocate an object previously allocated by a VipMemoryPool<T>.
	static VIP_ALWAYS_INLINE void deallocate(T* ptr) noexcept
	{
		auto* p = (block*)((uintptr_t)ptr & ~((uintptr_t)allocation_granularity() - 1u));
		auto* left = p->left;
		auto* parent = p->get_parent();
		if (VIP_UNLIKELY(p->deallocate(ptr, parent->d_data.lock) || !left))
			return handle_deallocate(parent, p);
		parent->d_data.lock.unlock();
	}

	/// @brief Deallocate all previously allocated memory
	/// and reset the pool's state
	void clear() noexcept
	{
		std::lock_guard<Lock> ll(d_data.lock);

		block* bl = d_data.it.right_all;
		// unsigned count = 0;
		while ((bl != &d_data.it)) {
			//++count;
			auto* next = bl->right_all;
			vipOSFreePages(bl, allocation_granularity() / vipOSPageSize());
			bl = next;
		}

		d_data.it.right = d_data.it.left = &d_data.it;
		d_data.it.right_all = d_data.it.left_all = &d_data.it;

		if (d_cache) {
			vipOSFreePages(d_cache, allocation_granularity() / vipOSPageSize());
			d_cache = nullptr;
		}
	}
};


//VIP_CORE_EXPORT void * vipAllocate(size_t size);
//VIP_CORE_EXPORT void vipDeallocate(void*, size_t) noexcept;

#endif