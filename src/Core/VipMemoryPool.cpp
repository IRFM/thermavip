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



#if defined(_WIN32)

extern "C" {
#include "Windows.h"
#include "Psapi.h"
}
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)

#include "VipMemoryPool.h"

namespace detail
{
	static inline SYSTEM_INFO build_sys_infos()
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return si;
	}
	static inline const SYSTEM_INFO& sys_infos()
	{
		static SYSTEM_INFO inst = build_sys_infos();
		return inst;
	}

}
size_t vipOSAllocationGranularity() noexcept
{
	return detail::sys_infos().dwAllocationGranularity;
}

size_t vipOSPageSize() noexcept
{
	return detail::sys_infos().dwPageSize;
}

void* vipOSAllocatePages(size_t pages) noexcept
{
	return VirtualAlloc(nullptr, pages * vipOSPageSize(), MEM_COMMIT, PAGE_READWRITE);
}

bool vipOSFreePages(void* p, size_t pages) noexcept
{
	int r = VirtualFree(p, 0, MEM_RELEASE);
	return r != 0;
}

#else // (Linux, macOSX, BSD, Illumnos, Haiku, DragonFly, etc.)

#include "VipMemoryPool.h"

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE // ensure mmap flags and syscall are defined
#endif

#if defined(__sun)
// illumos provides new mman.h api when any of these are defined
// otherwise the old api based on caddr_t which predates the void pointers one.
// stock solaris provides only the former, chose to atomically to discard those
// flags only here rather than project wide tough.
#undef _XOPEN_SOURCE
#undef _POSIX_C_SOURCE
#endif

extern "C" {

#include <sys/mman.h>	  // mmap
#include <sys/resource.h> //getrusage
#include <unistd.h>	  // sysconf
#if defined(__linux__)
#include <fcntl.h>
#include <features.h>
#if defined(__GLIBC__)
#include <linux/mman.h> // linux mmap flags
#else
#include <sys/mman.h>
#endif
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_IOS_IPHONE && !TARGET_IOS_SIMULATOR
#include <mach/vm_statistics.h>
#endif
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/param.h>
#if __FreeBSD_version >= 1200000
#include <sys/cpuset.h>
#include <sys/domainset.h>
#endif
#include <sys/sysctl.h>
#endif

#if !defined(__HAIKU__) && !defined(__APPLE__) && !defined(__CYGWIN__)
#include <sys/syscall.h>
#endif
}

namespace detail
{

	/*static inline int unix_madvise(void* addr, size_t size, int advice)
	{
#if defined(__sun)
		return madvise((caddr_t)addr, size, advice); // Solaris needs cast (issue #520)
#else
		return madvise(addr, size, advice);
#endif
	}*/
}

size_t vipOSPageSize() noexcept
{
	static size_t psize = (size_t)sysconf(_SC_PAGESIZE);
	return psize;
}

void* vipOSAllocatePages(size_t pages) noexcept
{
	size_t len = pages * vipOSPageSize();
	void* p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return p;
}

bool vipOSFreePages(void* p, size_t pages) noexcept
{
	return (munmap(p, pages * vipOSPageSize()) != -1);
}

size_t vipOSAllocationGranularity() noexcept
{
	return vipOSPageSize();
}

#endif
