#include "VipSIMD.h"

#include <mutex>

#ifdef _MSC_VER

#include "Windows.h"
//  Windows
#define cpuid(info, x) __cpuidex(info, x, 0)

#else

#if defined(_M_ARM64) || defined(__arm__) || defined(__ARM_NEON__)
inline void cpuid(int info[4], int InfoType)
{
	info[0] = info[1] = info[2] = info[3] = 0;
	(void)InfoType;
}
#else
//  GCC Intrinsics
#include <cpuid.h>
inline void cpuid(int info[4], int InfoType)
{
	__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#endif

#endif

// Undef min and max defined in Windows.h
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace detail
{
	void compute_cpu_feature(VipCPUFeatures& features, bool& initialized)
	{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif
		static std::mutex mutex;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
		mutex.lock();
		initialized = true;

		int info[4];
		cpuid(info, 0);
		int nIds = info[0];

		cpuid(info, static_cast<int>(0x80000000));
		unsigned nExIds = static_cast<unsigned>(info[0]);

		//  Detect Features
		if (nIds >= 0x00000001) {
			cpuid(info, 0x00000001);
			features.HAS_MMX = (info[3] & (1 << 23)) != 0;
			features.HAS_SSE = (info[3] & (1 << 25)) != 0;
			features.HAS_SSE2 = (info[3] & (1 << 26)) != 0;
			features.HAS_SSE3 = (info[2] & (1 << 0)) != 0;

			features.HAS_SSSE3 = (info[2] & (1 << 9)) != 0;
			features.HAS_SSE41 = (info[2] & (1 << 19)) != 0;
			features.HAS_SSE42 = (info[2] & (1 << 20)) != 0;
			features.HAS_AES = (info[2] & (1 << 25)) != 0;

			features.HAS_AVX = (info[2] & (1 << 28)) != 0;
			features.HAS_FMA3 = (info[2] & (1 << 12)) != 0;

			features.HAS_RDRAND = (info[2] & (1 << 30)) != 0;
		}
		if (nIds >= 0x00000007) {
			cpuid(info, 0x00000007);
			features.HAS_AVX2 = (info[1] & (1 << 5)) != 0;

			features.HAS_BMI1 = (info[1] & (1 << 3)) != 0;
			features.HAS_BMI2 = (info[1] & (1 << 8)) != 0;
			features.HAS_ADX = (info[1] & (1 << 19)) != 0;
			features.HAS_SHA = (info[1] & (1 << 29)) != 0;
			features.HAS_PREFETCHWT1 = (info[2] & (1 << 0)) != 0;

			features.HAS_AVX512F = (info[1] & (1 << 16)) != 0;
			features.HAS_AVX512CD = (info[1] & (1 << 28)) != 0;
			features.HAS_AVX512PF = (info[1] & (1 << 26)) != 0;
			features.HAS_AVX512ER = (info[1] & (1 << 27)) != 0;
			features.HAS_AVX512VL = (info[1] & static_cast<int>(1U << 31U)) != 0;
			features.HAS_AVX512BW = (info[1] & (1 << 30)) != 0;
			features.HAS_AVX512DQ = (info[1] & (1 << 17)) != 0;
			features.HAS_AVX512IFMA = (info[1] & (1 << 21)) != 0;
			features.HAS_AVX512VBMI = (info[2] & (1 << 1)) != 0;
		}
		if (nExIds >= 0x80000001) {
			cpuid(info, static_cast<int>(0x80000001));
			features.HAS_x64 = (info[3] & (1 << 29)) != 0;
			features.HAS_ABM = (info[2] & (1 << 5)) != 0;
			features.HAS_SSE4a = (info[2] & (1 << 6)) != 0;
			features.HAS_FMA4 = (info[2] & (1 << 16)) != 0;
			features.HAS_XOP = (info[2] & (1 << 11)) != 0;
		}

		mutex.unlock();
	}
}
