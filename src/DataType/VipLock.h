/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

	void lock() noexcept
	{
		for (;;) {
			// Optimistically assume the lock is free on the first try
			if (!d_lock.exchange(true, std::memory_order_acquire))
				return;

			// Wait for lock to be released without generating cache misses
			while (d_lock.load(std::memory_order_relaxed))
				// Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
				// hyper-threads
				std::this_thread::yield();
		}
	}

	bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed); }
	bool try_lock() noexcept
	{
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !d_lock.load(std::memory_order_relaxed) && !d_lock.exchange(true, std::memory_order_acquire);
	}

	void unlock() noexcept { d_lock.store(false, std::memory_order_release); }

	template<class Rep, class Period>
	bool try_lock_for(const std::chrono::duration<Rep, Period>& duration) noexcept
	{
		return try_lock_until(std::chrono::system_clock::now() + duration);
	}

	template<class Clock, class Duration>
	bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint) noexcept
	{
		for (;;) {
			if (!d_lock.exchange(true, std::memory_order_acquire))
				return true;

			while (d_lock.load(std::memory_order_relaxed)) {
				if (std::chrono::system_clock::now() > timePoint)
					return false;
				std::this_thread::yield();
			}
		}
	}
};

/// @brief An unfaire read-write VipSpinlock class that favors write operations
///
template<class LockType = std::int32_t>
class VipSharedSpinner
{
	static_assert(std::is_signed<LockType>::value, "VipSharedSpinner only supports signed atomic types!");
	using lock_type = LockType;
	static constexpr lock_type write = 1;
	static constexpr lock_type need_lock = 2;
	static constexpr lock_type read = 4;

	bool try_lock(lock_type& expect) noexcept
	{
		if (!d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel)) {
			d_lock.fetch_or(expect = need_lock, std::memory_order_release);
			return false;
		}
		return true;
	}
	void yield() noexcept { std::this_thread::yield(); }

	std::atomic<lock_type> d_lock;

public:
	constexpr VipSharedSpinner() noexcept
	  : d_lock(0)
	{
	}
	VipSharedSpinner(VipSharedSpinner const&) = delete;
	VipSharedSpinner& operator=(VipSharedSpinner const&) = delete;

	void lock() noexcept
	{
		lock_type expect = 0;
		while ((!try_lock(expect)))
			yield();
	}
	void unlock() noexcept { d_lock.fetch_and(~(write | need_lock), std::memory_order_release); }
	void lock_shared() noexcept
	{
		while ((!try_lock_shared()))
			yield();
	}
	void unlock_shared() noexcept { d_lock.fetch_add(-read, std::memory_order_release); }
	// Attempt to acquire writer permission. Return false if we didn't get it.
	bool try_lock() noexcept
	{
		if (d_lock.load(std::memory_order_relaxed) & (need_lock | write))
			return false;
		lock_type expect = 0;
		return d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel);
	}
	bool try_lock_shared() noexcept
	{
		if (!(d_lock.load(std::memory_order_relaxed) & (need_lock | write))) {
			if ((!(d_lock.fetch_add(read, std::memory_order_acquire) & (need_lock | write))))
				return true;
			d_lock.fetch_add(-read, std::memory_order_release);
		}
		return false;
	}
};

using VipSharedSpinlock = VipSharedSpinner<>;

template<class Lock>
class VipUniqueLock
{
	Lock* d_lock;

public:
	VipUniqueLock(Lock& l)
	  : d_lock(&l)
	{
		l.lock();
	}
	~VipUniqueLock() { d_lock->unlock(); }
};

template<class Lock>
class VipSharedLock
{
	Lock* d_lock;

public:
	VipSharedLock(Lock& l)
	  : d_lock(&l)
	{
		l.lock_shared();
	}
	~VipSharedLock() { d_lock->unlock_shared(); }
};

#endif
