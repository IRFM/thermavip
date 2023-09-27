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
	constexpr VipSpinlock()
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

	bool try_lock(lock_type& expect)
	{
		if (!d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel)) {
			d_lock.fetch_or(expect = need_lock, std::memory_order_release);
			return false;
		}
		return true;
	}
	void yield() { std::this_thread::yield(); }

	std::atomic<lock_type> d_lock;

public:
	constexpr VipSharedSpinner()
		: d_lock(0)
	{
	}
	VipSharedSpinner(VipSharedSpinner const&) = delete;
	VipSharedSpinner& operator=(VipSharedSpinner const&) = delete;

	void lock()
	{
		lock_type expect = 0;
		while ((!try_lock(expect)))
			yield();
	}
	void unlock()
	{
		d_lock.fetch_and(~(write | need_lock), std::memory_order_release);
	}
	void lock_shared()
	{
		while ((!try_lock_shared()))
			yield();
	}
	void unlock_shared()
	{
		d_lock.fetch_add(-read, std::memory_order_release);
	}
	// Attempt to acquire writer permission. Return false if we didn't get it.
	bool try_lock()
	{
		if (d_lock.load(std::memory_order_relaxed) & (need_lock | write))
			return false;
		lock_type expect = 0;
		return d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel);
	}
	bool try_lock_shared()
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
	Lock * d_lock;

public:
	VipUniqueLock(Lock& l)
	  : d_lock(&l)
	{
		l.lock();
	}
	~VipUniqueLock() 
	{ 
		d_lock->unlock();
	}
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
