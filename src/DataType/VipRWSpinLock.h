#pragma once

#include <atomic>
#include <thread>
// A simple, small (4-bytes), but unfair rwlock.  Use it when you want
// a nice writer and don't expect a lot of write/read contention, or
// when you need small rwlocks since you are creating a large number
// of them.
//
// Note that the unfairness here is extreme: if the lock is
// continually accessed for read, writers will never get a chance.  If
// the lock can be that highly contended this class is probably not an
// ideal choice anyway.
//
// It currently implements most of the Lockable, SharedLockable and
// UpgradeLockable concepts except the TimedLockable related locking/unlocking
// interfaces.
class VipRWSpinLock
{
	enum : int32_t
	{
		READER = 4,
		UPGRADED = 2,
		WRITER = 1
	};

public:
	constexpr VipRWSpinLock()
	  : bits_(0)
	{
	}

	VipRWSpinLock(VipRWSpinLock const&) = delete;
	VipRWSpinLock& operator=(VipRWSpinLock const&) = delete;

	// Lockable Concept
	void lock_write()
	{
		uint_fast32_t count = 0;
		while (! // LIKELY
		       (try_lock_write())) {
			if (++count > 1000) {
				std::this_thread::yield();
			}
		}
	}

	// Writer is responsible for clearing up both the UPGRADED and WRITER bits.
	void unlock_write()
	{
		static_assert(READER > WRITER + UPGRADED, "wrong bits!");
		bits_.fetch_and(~(WRITER | UPGRADED), std::memory_order_release);
	}

	// SharedLockable Concept
	void lock_read()
	{
		uint_fast32_t count = 0;
		while (! // LIKELY
		       (try_lock_read())) {
			if (++count > 1000) {
				std::this_thread::yield();
			}
		}
	}

	void unlock_read() { bits_.fetch_add(-READER, std::memory_order_release); }

	// Downgrade the lock from writer status to reader status.
	void unlock_write_and_lock_read()
	{
		bits_.fetch_add(READER, std::memory_order_acquire);
		unlock_write();
	}

	// Attempt to acquire writer permission. Return false if we didn't get it.
	bool try_lock_write()
	{
		int32_t expect = 0;
		return bits_.compare_exchange_strong(expect, WRITER, std::memory_order_acq_rel);
	}

	// Try to get reader permission on the lock. This can fail if we
	// find out someone is a writer or upgrader.
	// Setting the UPGRADED bit would allow a writer-to-be to indicate
	// its intention to write and block any new readers while waiting
	// for existing readers to finish and release their read locks. This
	// helps avoid starving writers (promoted from upgraders).
	bool try_lock_read()
	{
		// fetch_add is considerably (100%) faster than compare_exchange,
		// so here we are optimizing for the common (lock success) case.
		int32_t value = bits_.fetch_add(READER, std::memory_order_acquire);
		if ( // UNLIKELY
		  (value & (WRITER | UPGRADED))) {
			bits_.fetch_add(-READER, std::memory_order_release);
			return false;
		}
		return true;
	}

	// mainly for debugging purposes.
	int32_t bits() const { return bits_.load(std::memory_order_acquire); }

	class AcquireRead
	{
	public:
		AcquireRead(VipRWSpinLock* spinLock)
		  : m_spinLock(spinLock)
		{
			m_spinLock->lock_read();
		}

		~AcquireRead() { m_spinLock->unlock_read(); }

	private:
		VipRWSpinLock* m_spinLock;
	};
	class AcquireWrite
	{
	public:
		AcquireWrite(VipRWSpinLock* spinLock)
		  : m_spinLock(spinLock)
		{
			m_spinLock->lock_write();
		}

		~AcquireWrite() { m_spinLock->unlock_write(); }

	private:
		VipRWSpinLock* m_spinLock;
	};

private:
	std::atomic<int32_t> bits_;
};
