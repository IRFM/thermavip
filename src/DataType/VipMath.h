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

#ifndef VIP_MATH_H
#define VIP_MATH_H

#include <cmath>
#include <complex>
#include <limits>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <qmath.h>
#include <QPointF>

#include "VipConfig.h"
#include "VipComplex.h"
#include "VipSIMD.h"

// prefetching
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
#define VIP_PREFETCH(p) __builtin_prefetch(reinterpret_cast<const char*>(p))
#elif defined(__SSE2__)
#define VIP_PREFETCH(p) _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0)
#else
#define VIP_PREFETCH(p)
#endif

/// \addtogroup DataType
/// @{

/// Returns true if value is NaN, false otherwise.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipIsNan(T) noexcept
{
	return false;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipIsNan(T v) noexcept
{
	return std::isnan(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipIsNan(const std::complex<T>& c) noexcept
{
	return vipIsNan(c.real()) || vipIsNan(c.imag());
}

/// Returns true if value is positive or negative infinit, false otherwise.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipIsInf(T) noexcept
{
	return false;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipIsInf(T v) noexcept
{
	return std::isinf(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipIsInf(const std::complex<T>& c) noexcept
{
	return vipIsInf(c.real()) || vipIsInf(c.imag());
}

/// Floor given value.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipFloor(T v) noexcept
{
	return v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipFloor(T v) noexcept
{
	return std::floor(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipFloor(const std::complex<T>& c) noexcept
{
	return std::complex<T>(vipFloor(c.real()), vipFloor(c.imag()));
}

/// Ceil given value.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipCeil(T v) noexcept
{
	return v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipCeil(T v) noexcept
{
	return std::ceil(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipCeil(const std::complex<T>& c) noexcept
{
	return std::complex<T>(vipCeil(c.real()), vipCeil(c.imag()));
}

/// Round given value to the nearest integer.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipRound(T v) noexcept
{
	return v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipRound(T v) noexcept
{
	return std::round(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipRound(const std::complex<T>& c) noexcept
{
	return std::complex<T>(vipRound(c.real()), vipRound(c.imag()));
}

/// Returns absolute value of given value.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipAbs(T v) noexcept
{
	return v < 0 ? -v : v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipAbs(T v) noexcept
{
	return std::abs(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipAbs(const std::complex<T>& c) noexcept
{
	return std::abs(c);
}

Q_DECL_CONSTEXPR static inline double vipNan() noexcept
{
	return std::numeric_limits<double>::quiet_NaN();
}
Q_DECL_CONSTEXPR static inline long double vipLNan() noexcept
{
	return std::numeric_limits<long double>::quiet_NaN() ;
}

static inline double vipFrexp10(double arg, int* exp) 
{
	*exp = (arg == 0) ? 0 : (int)(1 + std::log10(qAbs(arg)));
	return arg * pow(10, -(*exp));
}

namespace Vip
{
	/// Convert degree to radian
	const double ToRadian = 0.01745329251994329577;
	/// Convert radian to degree
	const double ToDegree = 57.2957795130823208768;
}

#if defined(_MSC_VER)
// Microsoft says:
//
// Define _USE_MATH_DEFINES before including math.h to expose these macro
// definitions for common math constants.  These are placed under an #ifdef
// since these commonly-defined names are not part of the C/C++ standards.
#if !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES 1
#endif
#endif

#include <qmath.h>
#include <qpoint.h>

#ifndef LOG10_2
#define LOG10_2 0.30102999566398119802 // log10(2)
#endif

#ifndef LOG10_3
#define LOG10_3 0.47712125471966243540 // log10(3)
#endif

#ifndef LOG10_5
#define LOG10_5 0.69897000433601885749 // log10(5)
#endif

#ifndef M_2PI
#define M_2PI 6.28318530717958623200 // 2 pi
#endif

#ifndef LOG_MIN
//! Mininum value for logarithmic scales
#define LOG_MIN 1.0e-100
#endif

#ifndef LOG_MAX
//! Maximum value for logarithmic scales
#define LOG_MAX 1.0e100
#endif

namespace detail
{
	template<class T1, class T2>
	struct NumericMixFloating
	{
		static const bool value = std::is_arithmetic<T1>::value && std::is_arithmetic<T2>::value && (std::is_floating_point<T1>::value || std::is_floating_point<T2>::value);
	};
}

/// Compare 2 arithmetic values for equality.
/// For floating point values, check that their difference is within a few epsilons.
/// This function works for complex values and VipNDArray as well.
template<class T1, class T2>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T1>::value && std::is_integral<T2>::value, bool>::type vipFuzzyCompare(T1 v1, T2 v2)
{
	return v1 == v2;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
// suppress "conditional expression is constant" warning
#endif

template<class T1, class T2>
static inline typename std::enable_if<detail::NumericMixFloating<T1, T2>::value, bool>::type vipFuzzyCompare(T1 v1, T2 v2) noexcept
{
	if VIP_CONSTEXPR (std::is_same<T1, long double>::value || std::is_same<T2, long double>::value)
		return (qAbs((long double)v1 - (long double)v2) * 100000000000000. <= qMin(qAbs((long double)v1), qAbs((long double)v2)));
	else if VIP_CONSTEXPR (std::is_same<T1, double>::value || std::is_same<T2, double>::value)
		return (qAbs((double)v1 - (double)v2) * 1000000000000. <= qMin(qAbs((double)v1), qAbs((double)v2)));
	else
		return (qAbs((float)v1 - (float)v2) * 100000.f <= qMin(qAbs((float)v1), qAbs((float)v2)));
}
template<class T>
static inline bool vipFuzzyCompare(const std::complex<T>& v1, std::complex<T>& v2) noexcept
{
	return vipFuzzyCompare(v1.real(), v2.real()) && vipFuzzyCompare(v1.imag(), v2.imag());
}

/// Check if given value is null.
/// For floating point values, check that the value is within a few epsilons.
/// This function works for complex values as well.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, bool>::type vipFuzzyIsNull(T d) noexcept
{
	return d == 0;
}
template<class T>
static inline typename std::enable_if<std::is_floating_point<T>::value, bool>::type vipFuzzyIsNull(T d) noexcept
{
	if VIP_CONSTEXPR (std::is_same<T, float>::value)
		return qAbs(d) <= 0.00001f;
	else if VIP_CONSTEXPR (std::is_same<T, double>::value)
		return qAbs(d) <= 0.000000000001;
	else
		return qAbs(d) <= 0.00000000000001L;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template<class T>
Q_DECL_CONSTEXPR static inline bool vipFuzzyIsNull(const std::complex<T>& v) noexcept
{
	return vipFuzzyIsNull(v.real()) && vipFuzzyIsNull(v.imag());
}

/// Returns -1, 0 or 1 depending on the sign of \a d.
/// This function works for VipNDArray and functor expressions as well.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_arithmetic<T>::value, int>::type vipSign(T d) noexcept
{
	return (int)(T(0) < d) - (int)(d < T(0));
}

/// Round long double value to the nearest integer
Q_DECL_CONSTEXPR static inline qint64 vipRound64(long double d) noexcept
{
	return d >= (long double)0.0 ? qint64(d + (long double)0.5) : qint64(d - (long double)(qint64(d - 1)) + (long double)0.5) + qint64(d - (long double)1);
}

/// Overload qRound for long double values
Q_DECL_CONSTEXPR static inline int qRound(long double d) noexcept
{
	return d >= (long double)0.0 ? int(d + (long double)0.5) : int(d - (long double)(int(d - 1)) + (long double)0.5) + int(d - (long double)1);
}

/// Overload qRound64 for long double values
Q_DECL_CONSTEXPR static inline qint64 qRound64(long double d) noexcept
{
	return d >= (long double)0.0 ? qint64(d + (long double)0.5) : qint64(d - (long double)(qint64(d - 1)) + (long double)0.5) + qint64(d - (long double)1);
}

Q_DECL_CONSTEXPR static inline qint64 qFuzzyIsNull(long double d) noexcept
{
	return qAbs(d) <= 0.00000000000001L;
}

/// \brief Compare 2 values, relative to an interval
///
/// Values are "equal", when :
/// \f$\cdot value2 - value1 <= abs(intervalSize * 10e^{-6})\f$
///
/// \param value1 First value to compare
/// \param value2 Second value to compare
/// \param intervalSize interval size
///
/// \return 0: if equal, -1: if value2 > value1, 1: if value1 > value2
static inline int vipFuzzyCompare(double value1, double value2, double intervalSize) noexcept
{
	const double eps = qAbs(1.0e-6 * intervalSize);

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}
static inline int vipFuzzyCompare(long double value1, long double value2, long double intervalSize) noexcept
{
	const long double eps = sizeof(long double) > sizeof(double) ? qAbs(1.0e-8L * intervalSize) : qAbs(1.0e-6L * intervalSize);

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}
static inline int qFuzzyCompare(long double value1, long double value2, long double intervalSize) noexcept
{
	const long double eps = sizeof(long double) > sizeof(double) ? qAbs(1.0e-8L * intervalSize) : qAbs(1.0e-6L * intervalSize);

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}
static inline int qFuzzyCompare(long double value1, long double value2) noexcept
{
	const long double eps = sizeof(long double) > sizeof(double) ? 1.0e-8L : 1.0e-6L;

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
static inline bool qFuzzyCompare(const QPointF& value1, const QPointF& value2) noexcept
{
	return qFuzzyCompare(value1.x(),value2.x()) && qFuzzyCompare(value1.y(),value2.y());
}
#endif

static inline bool vipFuzzyGreaterOrEqual(double d1, double d2) noexcept
{
	return (d1 >= d2) || vipFuzzyCompare(d1, d2);
}
inline bool vipFuzzyGreaterOrEqual(long double d1, long double d2) noexcept
{
	return (d1 >= d2) || vipFuzzyCompare(d1, d2);
}

static inline bool vipFuzzyLessOrEqual(double d1, double d2) noexcept
{
	return (d1 <= d2) || vipFuzzyCompare(d1, d2);
}
static inline bool vipFuzzyLessOrEqual(long double d1, long double d2) noexcept
{
	return (d1 <= d2) || vipFuzzyCompare(d1, d2);
}

#if defined(__SIZEOF_INT128__)

VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh) noexcept
{
	const unsigned __int128 r = static_cast<unsigned __int128>(m1) * m2;

	*rh = static_cast<uint64_t>(r >> 64);
	*rl = static_cast<uint64_t>(r);
}

#define VIP_HAS_FAST_UMUL128 1

#elif (defined(__IBMC__) || defined(__IBMCPP__)) && defined(__LP64__)

VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh) noexcept
{
	*rh = __mulhdu(m1, m2);
	*rl = m1 * m2;
}

#define VIP_HAS_FAST_UMUL128 1

#elif defined(_MSC_VER) && (defined(_M_ARM64) || (defined(_M_X64) && defined(__INTEL_COMPILER)))

#include <intrin.h>

VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh) noexcept
{
	*rh = __umulh(m1, m2);
	*rl = m1 * m2;
}

#define VIP_HAS_FAST_UMUL128 1

#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IA64))

#include <intrin.h>
#pragma intrinsic(_umul128)

static VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh) noexcept
{
	*rl = _umul128(m1, m2, rh);
}

#define VIP_HAS_FAST_UMUL128 1

#else // defined( _MSC_VER )

// _umul128() code for 32-bit systems, adapted from Hacker's Delight,
// Henry S. Warren, Jr.

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)

#include <intrin.h>
#pragma intrinsic(__emulu)
#define __VIP_EMULU(x, y) __emulu(x, y)

#else // defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

#define __VIP_EMULU(x, y) ((uint64_t)(x) * (y))

#endif // defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

static inline void vipUmul128(const uint64_t u, const uint64_t v, uint64_t* const rl, uint64_t* const rh) noexcept
{
	*rl = u * v;

	const uint32_t u0 = static_cast<uint32_t>(u);
	const uint32_t v0 = static_cast<uint32_t>(v);
	const uint64_t w0 = __VIP_EMULU(u0, v0);
	const uint32_t u1 = static_cast<uint32_t>(u >> 32);
	const uint32_t v1 = static_cast<uint32_t>(v >> 32);
	const uint64_t t = __VIP_EMULU(u1, v0) + static_cast<uint32_t>(w0 >> 32);
	const uint64_t w1 = __VIP_EMULU(u0, v1) + static_cast<uint32_t>(t);

	*rh = __VIP_EMULU(u1, v1) + static_cast<uint32_t>(w1 >> 32) + static_cast<uint32_t>(t >> 32);
}

#endif




#ifdef __GNUC__
#define GNUC_PREREQ(x, y) (__GNUC__ > x || (__GNUC__ == x && __GNUC_MINOR__ >= y))
#else
#define GNUC_PREREQ(x, y) 0
#endif

#ifdef __clang__
#define CLANG_PREREQ(x, y) (__clang_major__ > (x) || (__clang_major__ == (x) && __clang_minor__ >= (y)))
#else
#define CLANG_PREREQ(x, y) 0
#endif

#if (_MSC_VER < 1900) && !defined(__cplusplus)
#define inline __inline
#endif

#if (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64))
#define X86_OR_X64
#endif

#if GNUC_PREREQ(4, 2) || __has_builtin(__builtin_popcount)
#define HAVE_BUILTIN_POPCOUNT
#endif

#if GNUC_PREREQ(4, 2) || CLANG_PREREQ(3, 0)
#define HAVE_ASM_POPCNT
#endif

#if defined(X86_OR_X64) && (defined(HAVE_ASM_POPCNT) || defined(_MSC_VER))
#define HAVE_POPCNT
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <immintrin.h>
#include <intrin.h>
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <nmmintrin.h>
#endif


namespace detail
{
	// Define default popcount functions

	// This uses fewer arithmetic operations than any other known
	// implementation on machines with fast multiplication.
	// It uses 12 arithmetic operations, one of which is a multiply.
	// http://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation
	VIP_ALWAYS_INLINE auto popcnt64(std::uint64_t x) -> unsigned
	{
		std::uint64_t m1 = 0x5555555555555555LL;
		std::uint64_t m2 = 0x3333333333333333LL;
		std::uint64_t m4 = 0x0F0F0F0F0F0F0F0FLL;
		std::uint64_t h01 = 0x0101010101010101LL;

		x -= (x >> 1) & m1;
		x = (x & m2) + ((x >> 2) & m2);
		x = (x + (x >> 4)) & m4;

		return (x * h01) >> 56;
	}
	VIP_ALWAYS_INLINE auto popcnt32(uint32_t i) -> unsigned
	{
		i = i - ((i >> 1) & 0x55555555);		// add pairs of bits
		i = (i & 0x33333333) + ((i >> 2) & 0x33333333); // quads
		i = (i + (i >> 4)) & 0x0F0F0F0F;		// groups of 8
		return (i * 0x01010101) >> 24;			// horizontal sum of bytes
	}
}

#if defined(HAVE_ASM_POPCNT) && defined(__x86_64__)

#define SEQ_HAS_ASM_POPCNT

VIP_ALWAYS_INLINE auto vipPopCount64(std::uint64_t x) -> unsigned
{
	__asm__("popcnt %1, %0" : "=r"(x) : "0"(x));
	return static_cast<unsigned>(x);
}

VIP_ALWAYS_INLINE auto vipPopCount32(uint32_t x) -> unsigned
{
	return detail::popcnt32(x);
}

#elif defined(HAVE_ASM_POPCNT) && defined(__i386__)

#define SEQ_HAS_ASM_POPCNT

VIP_ALWAYS_INLINE unsigned vipPopCount32(uint32_t x)
{
	__asm__("popcnt %1, %0" : "=r"(x) : "0"(x));
	return x;
}

VIP_ALWAYS_INLINE unsigned vipPopCount64(std::uint64_t x)
{
	return vipPopCount32((uint32_t)x) + vipPopCount32((uint32_t)(x >> 32));
}

#elif defined(_MSC_VER) && defined(_M_X64)

#define SEQ_HAS_BUILTIN_POPCNT

VIP_ALWAYS_INLINE unsigned vipPopCount64(std::uint64_t x)
{
	return (unsigned)_mm_popcnt_u64(x);
}

VIP_ALWAYS_INLINE unsigned vipPopCount32(uint32_t x)
{
	return (unsigned)_mm_popcnt_u32(x);
}

#elif defined(_MSC_VER) && defined(_M_IX86)

#define SEQ_HAS_BUILTIN_POPCNT

VIP_ALWAYS_INLINE unsigned vipPopCount64(std::uint64_t x)
{
	return _mm_popcnt_u32((uint32_t)x) + _mm_popcnt_u32((uint32_t)(x >> 32));
}
VIP_ALWAYS_INLINE unsigned vipPopCount32(uint32_t x)
{
	return _mm_popcnt_u32(x);
}

/* non x86 CPUs */
#elif defined(HAVE_BUILTIN_POPCOUNT)

#define SEQ_HAS_BUILTIN_POPCNT

VIP_ALWAYS_INLINE std::uint64_t vipPopCount64(std::uint64_t x)
{
	return __builtin_popcountll(x);
}
VIP_ALWAYS_INLINE uint32_t vipPopCount32(uint32_t x)
{
	return __builtin_popcount(x);
}

/* no hardware POPCNT,
 * use pure integer algorithm */
#else

VIP_ALWAYS_INLINE std::uint64_t vipPopCount64(std::uint64_t x)
{
	return detail::popcnt64(x);
}
VIP_ALWAYS_INLINE uint32_t vipPopCount32(uint32_t x)
{
	return detail::popcnt32(x);
}

#endif

VIP_ALWAYS_INLINE auto vipPopCount8(unsigned char value) -> unsigned
{
	static const unsigned char ones[256] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3,
						 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4,
						 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1,
						 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5,
						 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5,
						 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8 };
	return ones[value];
}
VIP_ALWAYS_INLINE auto vipPopCount16(unsigned short value) -> unsigned
{
#ifdef _MSC_VER
	return __popcnt16(value);
#else
	return vipPopCount8(value & 0xFF) + vipPopCount8(value >> 8);
#endif
}

///
/// @function unsigned vipPopCount16(unsigned short value)
/// @brief Returns the number of set bits in \a value.
///

///
/// @function unsigned vipPopCount32(unsigned int value)
/// @brief Returns the number of set bits in \a value.
///

///
/// @function unsigned vipPopCount64(unsigned long long value)
/// @brief Returns the number of set bits in \a value.
///

#if defined(_MSC_VER) || ((defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))))
#define SEQ_HAS_BUILTIN_BITSCAN
#endif

VIP_ALWAYS_INLINE auto vipBitScanForward8(std::uint8_t val) -> unsigned int
{
	static const std::uint8_t scan_forward_8[] = { 8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1,
						       0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0,
						       1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7,
						       0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0,
						       2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1,
						       0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };
	return scan_forward_8[val];
}
VIP_ALWAYS_INLINE auto vipBitScanReverse8(std::uint8_t val) -> unsigned int
{
	static const std::uint8_t scan_reverse_8[] = { 8, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
						       5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
						       6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7,
						       7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
						       7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
						       7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };
	return scan_reverse_8[val];
}

/// @brief Returns the lowest set bit index in \a val
/// Undefined if val==0.
VIP_ALWAYS_INLINE auto vipBitScanForward32(std::uint32_t val) -> unsigned int
{
#if defined(_MSC_VER) /* Visual */
	unsigned long r = 0;
	_BitScanForward(&r, val);
	return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) /* Use GCC Intrinsic */
	return __builtin_ctz(val);
#else								     /* Software version */
	static const int MultiplyDeBruijnBitPosition[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
	return MultiplyDeBruijnBitPosition[((uint32_t)((val & -val) * 0x077CB531U)) >> 27];
#endif
}

/// @brief Returns the highest set bit index in \a val
/// Undefined if val==0.
VIP_ALWAYS_INLINE auto vipBitScanReverse32(std::uint32_t val) -> unsigned int
{
#if defined(_MSC_VER) /* Visual */
	unsigned long r = 0;
	_BitScanReverse(&r, val);
	return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) /* Use GCC Intrinsic */
	return 31 - __builtin_clz(val);
#else								     /* Software version */
	static const unsigned int pos[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
	// does not work for 0
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v = (v >> 1) + 1;
	return pos[(v * 0x077CB531UL) >> 27];
#endif
}

/// @brief Returns the lowest set bit index in \a bb.
/// Developed by Kim Walisch (2012).
/// Undefined if bb==0.
VIP_ALWAYS_INLINE auto vipBitScanForward64(std::uint64_t bb) noexcept -> unsigned
{
#if defined(_MSC_VER) && defined(_WIN64)
	unsigned long r = 0;
	_BitScanForward64(&r, bb);
	return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3)))
	return __builtin_ctzll(bb);
#else
	static const unsigned forward_index64[64] = { 0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61, 54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4, 62,
						      46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45, 25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5, 63 };
	const std::uint64_t debruijn64 = std::int64_t(0x03f79d71b4cb0a89);
	return forward_index64[((bb ^ (bb - 1)) * debruijn64) >> 58];
#endif
}

/// @brief Returns the highest set bit index in \a bb.
/// Developed by Kim Walisch, Mark Dickinson.
/// Undefined if bb==0.
VIP_ALWAYS_INLINE auto vipBitScanReverse64(std::uint64_t bb) noexcept -> unsigned
{
#if (defined(_MSC_VER) && defined(_WIN64)) //|| defined(__MINGW64_VERSION_MAJOR)
	unsigned long r = 0;
	_BitScanReverse64(&r, bb);
	return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3)))
	return 63 - __builtin_clzll(bb);
#else
	static const unsigned backward_index64[64] = { 0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61, 54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4, 62,
						       46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45, 25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5, 63 };
	const std::uint64_t debruijn64 = std::int64_t(0x03f79d71b4cb0a89);
	// assert(bb != 0);
	bb |= bb >> 1;
	bb |= bb >> 2;
	bb |= bb >> 4;
	bb |= bb >> 8;
	bb |= bb >> 16;
	bb |= bb >> 32;
	return backward_index64[(bb * debruijn64) >> 58];
#endif
}




// for complex types, we are missing a few operators, so define them
inline complex_f operator*(double v, const complex_f& c) noexcept
{
	return complex_f(c.real() * v, c.imag() * v);
}
inline complex_d operator*(float v, const complex_d& c) noexcept
{
	return complex_d(c.real() * v, c.imag() * v);
}
inline complex_f operator*(const complex_f& c, double v) noexcept
{
	return complex_f(c.real() * v, c.imag() * v);
}
inline complex_d operator*(const complex_d& c, float v) noexcept
{
	return complex_d(c.real() * v, c.imag() * v);
}

/// @}
// end DataType

#endif
