/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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
#include <qmath.h>

#include "VipConfig.h"
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
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipIsNan(T)
{
	return false;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipIsNan(T v)
{
	return std::isnan(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipIsNan(const std::complex<T>& c)
{
	return vipIsNan(c.real()) || vipIsNan(c.imag());
}

/// Returns true if value is positive or negative infinit, false otherwise.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipIsInf(T)
{
	return false;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipIsInf(T v)
{
	return std::isinf(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipIsInf(const std::complex<T>& c)
{
	return vipIsInf(c.real()) || vipIsInf(c.imag());
}

/// Floor given value.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipFloor(T v)
{
	return v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipFloor(T v)
{
	return std::floor(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipFloor(const std::complex<T>& c)
{
	return std::complex<T>(vipFloor(c.real()), vipFloor(c.imag()));
}

/// Ceil given value.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipCeil(T v)
{
	return v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipCeil(T v)
{
	return std::ceil(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipCeil(const std::complex<T>& c)
{
	return std::complex<T>(vipCeil(c.real()), vipCeil(c.imag()));
}

/// Round given value to the nearest integer.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipRound(T v)
{
	return v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipRound(T v)
{
	return std::round(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipRound(const std::complex<T>& c)
{
	return std::complex<T>(vipRound(c.real()), vipRound(c.imag()));
}

/// Returns absolute value of given value.
/// Works for complex types, VipNDArray and functor expressions.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, T>::type vipAbs(T v)
{
	return v < 0 ? -v : v;
}
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type vipAbs(T v)
{
	return std::abs(v);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipAbs(const std::complex<T>& c)
{
	return std::abs(c);
}

Q_DECL_CONSTEXPR static inline double vipNan()
{
	return std::numeric_limits<double>::quiet_NaN();
}
Q_DECL_CONSTEXPR static inline long double vipLNan()
{
	return std::numeric_limits<long double>::quiet_NaN();
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
static inline typename std::enable_if<detail::NumericMixFloating<T1, T2>::value, bool>::type vipFuzzyCompare(T1 v1, T2 v2)
{
	if VIP_CONSTEXPR (std::is_same<T1, long double>::value || std::is_same<T2, long double>::value)
		return (qAbs((long double)v1 - (long double)v2) * 100000000000000. <= qMin(qAbs((long double)v1), qAbs((long double)v2)));
	else if VIP_CONSTEXPR (std::is_same<T1, double>::value || std::is_same<T2, double>::value)
		return (qAbs((double)v1 - (double)v2) * 1000000000000. <= qMin(qAbs((double)v1), qAbs((double)v2)));
	else
		return (qAbs((float)v1 - (float)v2) * 100000.f <= qMin(qAbs((float)v1), qAbs((float)v2)));
}
template<class T>
static inline bool vipFuzzyCompare(const std::complex<T>& v1, std::complex<T>& v2)
{
	return vipFuzzyCompare(v1.real(), v2.real()) && vipFuzzyCompare(v1.imag(), v2.imag());
}

/// Check if given value is null.
/// For floating point values, check that the value is within a few epsilons.
/// This function works for complex values as well.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_integral<T>::value, bool>::type vipFuzzyIsNull(T d)
{
	return d == 0;
}
template<class T>
static inline typename std::enable_if<std::is_floating_point<T>::value, bool>::type vipFuzzyIsNull(T d)
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
Q_DECL_CONSTEXPR static inline bool vipFuzzyIsNull(const std::complex<T>& v)
{
	return vipFuzzyIsNull(v.real()) && vipFuzzyIsNull(v.imag());
}

/// Returns -1, 0 or 1 depending on the sign of \a d.
/// This function works for VipNDArray and functor expressions as well.
template<class T>
Q_DECL_CONSTEXPR static inline typename std::enable_if<std::is_arithmetic<T>::value, int>::type vipSign(T d)
{
	return (int)(T(0) < d) - (int)(d < T(0));
}

/// Round long double value to the nearest integer
Q_DECL_CONSTEXPR static inline qint64 vipRound64(long double d)
{
	return d >= (long double)0.0 ? qint64(d + (long double)0.5) : qint64(d - (long double)(qint64(d - 1)) + (long double)0.5) + qint64(d - (long double)1);
}

/// Overload qRound for long double values
Q_DECL_CONSTEXPR static inline int qRound(long double d)
{
	return d >= (long double)0.0 ? int(d + (long double)0.5) : int(d - (long double)(int(d - 1)) + (long double)0.5) + int(d - (long double)1);
}

/// Overload qRound64 for long double values
Q_DECL_CONSTEXPR static inline qint64 qRound64(long double d)
{
	return d >= (long double)0.0 ? qint64(d + (long double)0.5) : qint64(d - (long double)(qint64(d - 1)) + (long double)0.5) + qint64(d - (long double)1);
}

Q_DECL_CONSTEXPR static inline qint64 qFuzzyIsNull(long double d)
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
static inline int vipFuzzyCompare(double value1, double value2, double intervalSize)
{
	const double eps = qAbs(1.0e-6 * intervalSize);

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}
static inline int vipFuzzyCompare(long double value1, long double value2, long double intervalSize)
{
	const long double eps = sizeof(long double) > sizeof(double) ? qAbs(1.0e-8L * intervalSize) : qAbs(1.0e-6L * intervalSize);

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}
static inline int qFuzzyCompare(long double value1, long double value2, long double intervalSize)
{
	const long double eps = sizeof(long double) > sizeof(double) ? qAbs(1.0e-8L * intervalSize) : qAbs(1.0e-6L * intervalSize);

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}
static inline int qFuzzyCompare(long double value1, long double value2)
{
	const long double eps = sizeof(long double) > sizeof(double) ? 1.0e-8L : 1.0e-6L;

	if (value2 - value1 > eps)
		return -1;

	if (value1 - value2 > eps)
		return 1;

	return 0;
}

static inline bool vipFuzzyGreaterOrEqual(double d1, double d2)
{
	return (d1 >= d2) || vipFuzzyCompare(d1, d2);
}
inline bool vipFuzzyGreaterOrEqual(long double d1, long double d2)
{
	return (d1 >= d2) || vipFuzzyCompare(d1, d2);
}

static inline bool vipFuzzyLessOrEqual(double d1, double d2)
{
	return (d1 <= d2) || vipFuzzyCompare(d1, d2);
}
static inline bool vipFuzzyLessOrEqual(long double d1, long double d2)
{
	return (d1 <= d2) || vipFuzzyCompare(d1, d2);
}

#if defined(__SIZEOF_INT128__)

VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
{
	const unsigned __int128 r = static_cast<unsigned __int128>(m1) * m2;

	*rh = static_cast<uint64_t>(r >> 64);
	*rl = static_cast<uint64_t>(r);
}

#define VIP_HAS_FAST_UMUL128 1

#elif (defined(__IBMC__) || defined(__IBMCPP__)) && defined(__LP64__)

VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
{
	*rh = __mulhdu(m1, m2);
	*rl = m1 * m2;
}

#define VIP_HAS_FAST_UMUL128 1

#elif defined(_MSC_VER) && (defined(_M_ARM64) || (defined(_M_X64) && defined(__INTEL_COMPILER)))

#include <intrin.h>

VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
{
	*rh = __umulh(m1, m2);
	*rl = m1 * m2;
}

#define VIP_HAS_FAST_UMUL128 1

#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IA64))

#include <intrin.h>
#pragma intrinsic(_umul128)

static VIP_ALWAYS_INLINE void vipUmul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
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

static inline void vipUmul128(const uint64_t u, const uint64_t v, uint64_t* const rl, uint64_t* const rh)
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

/// @}
// end DataType

#endif
