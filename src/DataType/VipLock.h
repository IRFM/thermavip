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

#ifndef VIP_LOCK_H
#define VIP_LOCK_H

/** @file */

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <type_traits>
#include <cstdint>

#include "VipConfig.h"

/// @brief Lightweight and fast VipSpinlock implementation based on https://rigtorp.se/VipSpinlock/
///
/// VipSpinlock is a lightweight VipSpinlock implementation following the TimedMutex requirements.
///
class VipSpinlock
{
	std::atomic<bool> d_lock;

public:
	constexpr VipSpinlock() noexcept
	  : d_lock(0)
	{
	}

	VipSpinlock(VipSpinlock const&) = delete;
	VipSpinlock& operator=(VipSpinlock const&) = delete;

	VIP_ALWAYS_INLINE void lock() noexcept
	{
		for (;;) {
			// Optimistically assume the lock is free on the first try
			if (!d_lock.exchange(true, std::memory_order_acquire))
				return;

			// Wait for lock to be released without generating cache misses
			while (d_lock.load(std::memory_order_relaxed))
				std::this_thread::yield();
		}
	}

	VIP_ALWAYS_INLINE bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed); }
	VIP_ALWAYS_INLINE bool try_lock() noexcept
	{
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !d_lock.load(std::memory_order_relaxed) && !d_lock.exchange(true, std::memory_order_acquire);
	}

	VIP_ALWAYS_INLINE void unlock() noexcept { d_lock.store(false, std::memory_order_release); }

	template<class Rep, class Period>
	bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
	{
		return try_lock_until(std::chrono::steady_clock::now() + duration);
	}

	template<class Clock, class Duration>
	bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint)
	{
		for (;;) {
			if (!d_lock.exchange(true, std::memory_order_acquire))
				return true;

			if (Clock::now() >= timePoint)
				return try_lock();

			while (d_lock.load(std::memory_order_relaxed)) {
				if (Clock::now() >= timePoint)
					return try_lock();
				std::this_thread::yield();
			}
		}
	}
};

/// @brief Recursive spinlock based on VipSpinlock
class VIP_DATA_TYPE_EXPORT VipRecursiveSpinlock
{
	VipSpinlock d_lock;
	std::atomic<std::uint64_t> d_id = 0;
	std::uint64_t d_count = 0;

	std::uint64_t thread_id() const noexcept;

public:
	VipRecursiveSpinlock() = default;
	~VipRecursiveSpinlock() noexcept = default;

	VipRecursiveSpinlock(const VipRecursiveSpinlock&) = delete;
	VipRecursiveSpinlock& operator=(const VipRecursiveSpinlock&) = delete;

	VIP_ALWAYS_INLINE void lock() noexcept
	{
		auto id = thread_id();
		if (id == d_id.load(std::memory_order_relaxed)) {
			++d_count;
		}
		else {
			d_lock.lock();
			d_id.store(id);
		}
	}

	VIP_ALWAYS_INLINE bool try_lock() noexcept
	{
		auto id = thread_id();
		if (id == d_id.load(std::memory_order_relaxed)) {
			++d_count;
			return true;
		}
		if (d_lock.try_lock()) {
			d_id.store(id);
			return true;
		}
		return false;
	}

	template<class Rep, class Period>
	bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
	{
		return try_lock_until(std::chrono::steady_clock::now() + duration);
	}

	template<class Clock, class Duration>
	bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint)
	{
		for (;;) {
			if (try_lock())
				return true;

			if (Clock::now() >= timePoint)
				return try_lock();

			std::this_thread::yield();
		}
		VIP_UNREACHABLE();
	}

	VIP_ALWAYS_INLINE void unlock() noexcept
	{
		VIP_ASSERT_DEBUG(thread_id() == d_id.load(std::memory_order_relaxed));
		if (d_count > 0) {
			--d_count;
		}
		else {
			d_id.store(0);
			d_lock.unlock();
		}
	}
};

/// @brief An unfaire read-write spinlock class
///
template<class LockType = std::uint64_t>
class VipSharedSpinner
{
	static_assert(!std::is_same_v<LockType, bool>, "VipSharedSpinner does not support bool type");
	static_assert(std::is_unsigned_v<LockType>, "VipSharedSpinner only supports unsigned atomic types");
	static_assert(std::atomic<LockType>::is_always_lock_free, "VipSharedSpinner requires lock-free atomics");

	using lock_type = LockType;
	static constexpr lock_type write = 1;
	static constexpr lock_type need_lock = 2;
	static constexpr lock_type read = 4;
	static constexpr lock_type max_read_mask = 1ull << (sizeof(lock_type) * 8u - 1u);

	bool failed_lock(lock_type& expect)
	{
		if (!(expect & (need_lock)))
			d_lock.fetch_or(need_lock, std::memory_order_release);
		expect = need_lock;
		return false;
	}
	VIP_ALWAYS_INLINE bool try_lock(lock_type& expect)
	{
		if (!d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel)) {
			return failed_lock(expect);
		}
		return true;
	}
	VIP_ALWAYS_INLINE void yield() { std::this_thread::yield(); }

	std::atomic<lock_type> d_lock;

public:
	constexpr VipSharedSpinner()
	  : d_lock(0)
	{
	}
	VipSharedSpinner(VipSharedSpinner const&) = delete;
	VipSharedSpinner& operator=(VipSharedSpinner const&) = delete;

	VIP_ALWAYS_INLINE void lock()
	{
		lock_type expect = 0;
		while (!try_lock(expect))
			yield();
	}
	VIP_ALWAYS_INLINE void unlock()
	{
		VIP_ASSERT_DEBUG(d_lock & write, "");
		d_lock.fetch_and(static_cast<lock_type>(~(write | need_lock)), std::memory_order_release);
	}
	VIP_ALWAYS_INLINE void lock_shared()
	{
		while (!try_lock_shared())
			yield();
	}
	VIP_ALWAYS_INLINE void unlock_shared()
	{
		VIP_ASSERT_DEBUG(d_lock >= read, "");
		d_lock.fetch_sub(read, std::memory_order_release);
	}
	// Attempt to acquire writer permission. Return false if we didn't get it.
	VIP_ALWAYS_INLINE bool try_lock()
	{
		if (d_lock.load(std::memory_order_relaxed) & (need_lock | write))
			return false;
		lock_type expect = 0;
		return d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel);
	}
	VIP_ALWAYS_INLINE bool try_lock_shared()
	{
		// This version might be slightly slower in some situations (low concurrency).
		// However it works for small lock type by avoiding overflows.
		if constexpr (sizeof(lock_type) < sizeof(std::uint64_t)) {
			lock_type content = d_lock.load(std::memory_order_relaxed);
			return (!(content & (need_lock | write | max_read_mask)) && d_lock.compare_exchange_strong(content, content + read));
		}
		else {
			// Version based on fetch_add
			if (!(d_lock.load(std::memory_order_relaxed) & (need_lock | write))) {
				if (!(d_lock.fetch_add(read, std::memory_order_acquire) & (need_lock | write)))
					return true;
				d_lock.fetch_sub(read, std::memory_order_release);
			}
			return false;
		}
	}
};

using VipSharedSpinlock = VipSharedSpinner<>;

#endif
