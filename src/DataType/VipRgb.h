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

#ifndef VIP_RGB_H
#define VIP_RGB_H

#include <type_traits>

#include <QColor>
#include <QMetaType>
#include <QtGlobal>

#include "VipMath.h"

/// @brief RGB color structure with alpha channel
/// @tparam T
template<class T>
struct VipRgb
{
	typedef T value_type;
	T b;
	T g;
	T r;
	T a;

	VipRgb() noexcept
	  : b(0)
	  , g(0)
	  , r(0)
	  , a(0)
	{
	}
	VipRgb(T r, T g, T b, T a = (T)255) noexcept
	  : b(b)
	  , g(g)
	  , r(r)
	  , a(a)
	{
	}
	VipRgb(const QColor& c) noexcept
	  : b(c.blue())
	  , g(c.green())
	  , r(c.red())
	  , a(c.alpha())
	{
	}
	VipRgb(QRgb c) noexcept
	  : b(qBlue(c))
	  , g(qGreen(c))
	  , r(qRed(c))
	  , a(qAlpha(c))
	{
	}
	VipRgb(const VipRgb& other) noexcept
	  : b(other.b)
	  , g(other.g)
	  , r(other.r)
	  , a(other.a)
	{
	}
	template<class U>
	VipRgb(const VipRgb<U>& other) noexcept
	  : b(other.b)
	  , g(other.g)
	  , r(other.r)
	  , a(other.a)
	{
	}

	template<class U>
	VipRgb<U> clamp(U min, U max) const noexcept
	{
		return VipRgb((U)r < min ? min : ((U)r > max ? max : (U)r),
			      (U)g < min ? min : ((U)g > max ? max : (U)g),
			      (U)b < min ? min : ((U)b > max ? max : (U)b),
			      (U)a < min ? min : ((U)a > max ? max : (U)a));
	}

	/// @brief Convert to QRgb
	QRgb toQRgb() const noexcept
	{
		VipRgb<quint8> tmp = clamp<quint8>(0, 255);
		return reinterpret_cast<QRgb&>(tmp);
	}
	/// @brief Convert to QColor
	QColor toQColor() const noexcept { return QColor::fromRgba(toQRgb()); }

	operator QRgb() const noexcept { return toQRgb(); }
	operator QColor() const noexcept { return toQRgb(); }
};

/// @brief Specialization of VipRgb for quint8
/// Can be safely reinterpreted from/to QRgb. Used to manipulate inplace QImage with format QImage::Format_ARGB32.
template<>
struct alignas(alignof(QRgb)) VipRgb<quint8>
{
	typedef quint8 value_type;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	quint8 b{ 0 };
	quint8 g{ 0 };
	quint8 r{ 0 };
	quint8 a{ 0 };
#else
	quint8 a{ 0 };
	quint8 r{ 0 };
	quint8 g{ 0 };
	quint8 b{ 0 };
#endif

	VipRgb() noexcept {}

	VipRgb(quint8 r, quint8 g, quint8 b, quint8 a = 255) noexcept
	  :
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	  b(b)
	  , g(g)
	  , r(r)
	  , a(a)
#else
	  a(a)
	  , r(r)
	  , g(g)
	  , b(b)
#endif
	{
	}

	VipRgb(const QColor& c) noexcept
	  :
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	  b(c.blue())
	  , g(c.green())
	  , r(c.red())
	  , a(c.alpha())
#else
	  a(c.alpha())
	  , r(c.red())
	  , g(c.green())
	  , b(c.blue())
#endif
	{
	}

	VipRgb(QRgb c) noexcept
	{
		*(QRgb*)this = c;
	}

	VipRgb(const VipRgb& other) noexcept = default;

	template<class U>
	VipRgb(const VipRgb<U>& other) noexcept
	  :
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	  b(other.b)
	  , g(other.g)
	  , r(other.r)
	  , a(other.a)
#else
	  a(other.a)
	  , r(other.r)
	  , g(other.g)
	  , b(other.b)
#endif
	{
		// r = other.r < (U)0 ? (quint8)0 : (other.r > (U)255 ? (quint8)255 : (quint8)other.r);
		//  g = other.g < (U)0 ? (quint8)0 : (other.g > (U)255 ? (quint8)255 : (quint8)other.g);
		//  b = other.b < (U)0 ? (quint8)0 : (other.b > (U)255 ? (quint8)255 : (quint8)other.b);
		//  a = other.a < (U)0 ? (quint8)0 : (other.a > (U)255 ? (quint8)255 : (quint8)other.a);
	}

	template<class U>
	VipRgb<U> clamp(U min, U max) const noexcept
	{
		return VipRgb((U)r < min ? min : ((U)r > max ? max : (U)r),
			      (U)g < min ? min : ((U)g > max ? max : (U)g),
			      (U)b < min ? min : ((U)b > max ? max : (U)b),
			      (U)a < min ? min : ((U)a > max ? max : (U)a));
	}

	QRgb toQRgb() const noexcept
	{
		return reinterpret_cast<const QRgb&>(*this);
	}
	QColor toQColor() const noexcept
	{
		return QColor::fromRgba(toQRgb());
	}

	VipRgb& operator=(const VipRgb& other) noexcept
	{
		// memcpy(this, &other, sizeof(VipRgb));
		*(QRgb*)(this) = *(const QRgb*)&other;
		return *this;
	}

	template<class U>
	VipRgb& operator=(const VipRgb<U>& other) noexcept
	{
		r = other.r; // < (U)0 ? (quint8)0 : (other.r > (U)255 ? (quint8)255 : (quint8)other.r);
		g = other.g; // < (U)0 ? (quint8)0 : (other.g > (U)255 ? (quint8)255 : (quint8)other.g);
		b = other.b; // < (U)0 ? (quint8)0 : (other.b > (U)255 ? (quint8)255 : (quint8)other.b);
		a = other.a; // < (U)0 ? (quint8)0 : (other.a > (U)255 ? (quint8)255 : (quint8)other.a);
		return *this;
	}

	operator QRgb() const noexcept
	{
		return reinterpret_cast<const QRgb&>(*this);
		;
	}
	operator QColor() const noexcept
	{
		return toQColor();
	}
};

typedef VipRgb<quint8> VipRGB;
Q_DECLARE_METATYPE(VipRGB)

template<class T>
struct is_rgb : std::false_type
{
};

template<class T>
struct is_rgb<VipRgb<T>> : std::true_type
{
};
//
// Operator overloads
//

template<class T, class U>
VipRgb<T>& operator+=(VipRgb<T>& t, const VipRgb<U>& v) noexcept
{
	t.r += v.r;
	t.g += v.g;
	t.b += v.b;
	t.a += v.a;
	return t;
}
template<class T, class U>
typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>&>::type operator+=(VipRgb<T>& t, const U& v) noexcept
{
	t.r += v;
	t.g += v;
	t.b += v;
	t.a += v;
	return t;
}

template<class T, class U>
VipRgb<T>& operator-=(VipRgb<T>& t, const VipRgb<U>& v) noexcept
{ 
	t.r -= v.r;
	t.g -= v.g;
	t.b -= v.b;
	t.a -= v.a;
	return t;
}
template<class T, class U>
typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>&>::type operator-=(VipRgb<T>& t, const U& v) noexcept
{
	t.r -= v;
	t.g -= v;
	t.b -= v;
	t.a -= v;
	return t;
}

template<class T, class U>
VipRgb<T>& operator*=(VipRgb<T>& t, const VipRgb<U>& v) noexcept
{
	t.r *= v.r;
	t.g *= v.g;
	t.b *= v.b;
	t.a *= v.a;
	return t;
}
template<class T, class U>
typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>&>::type operator*=(VipRgb<T>& t, const U& v) noexcept
{
	t.r *= v;
	t.g *= v;
	t.b *= v;
	t.a *= v;
	return t;
}

template<class T, class U>
VipRgb<T>& operator/=(VipRgb<T>& t, const VipRgb<U>& v) noexcept
{
	t.r /= v.r;
	t.g /= v.g;
	t.b /= v.b;
	t.a /= v.a;
	return t;
}
template<class T, class U>
typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>&>::type operator/=(VipRgb<T>& t, const U& v) noexcept
{
	t.r /= v;
	t.g /= v;
	t.b /= v;
	t.a /= v;
	return t;
}

template<class T, class U>
inline VipRgb<decltype(T() + U())> operator+(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<decltype(T() + U())>(v1.r + v2.r, v1.g + v2.g, v1.b + v2.b, v1.a + v2.a);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() + U())>::type> operator+(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<decltype(T() + U())>(v1.r + v2, v1.g + v2, v1.b + v2, v1.a + v2);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() + U())>::type> operator+(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<decltype(T() + U())>(v1.r + v2, v1.g + v2, v1.b + v2, v1.a + v2);
}

template<class T, class U>
inline VipRgb<decltype(T() - U())> operator-(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<decltype(T() - U())>(v1.r - v2.r, v1.g - v2.g, v1.b - v2.b, v1.a - v2.a);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() - U())>::type> operator-(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<decltype(T() - U())>(v1.r - v2, v1.g - v2, v1.b - v2, v1.a - v2);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() - U())>::type> operator-(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<decltype(T() - U())>(v1.r - v2, v1.g - v2, v1.b - v2, v1.a - v2);
}

template<class T, class U>
inline VipRgb<decltype(T() * U())> operator*(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<decltype(T() * U())>(v1.r * v2.r, v1.g * v2.g, v1.b * v2.b, v1.a * v2.a);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() * U())>::type> operator*(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<decltype(T() * U())>(v1.r * v2, v1.g * v2, v1.b * v2, v1.a * v2);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() * U())>::type> operator*(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<decltype(T() * U())>(v1.r * v2, v1.g * v2, v1.b * v2, v1.a * v2);
}

template<class T, class U>
inline VipRgb<decltype(T() / U())> operator/(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<decltype(T() / U())>(v1.r / v2.r, v1.g / v2.g, v1.b / v2.b, v1.a / v2.a);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() / U())>::type> operator/(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<decltype(T() / U())>(v1.r / v2, v1.g / v2, v1.b / v2, v1.a / v2);
}

template<class T, class U>
inline VipRgb<decltype(T() % U())> operator%(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<decltype(T() % U())>(v1.r % v2.r, v1.g % v2.g, v1.b % v2.b, v1.a % v2.a);
}
template<class T, class U>
inline VipRgb<typename std::enable_if<std::is_arithmetic<U>::value, decltype(T() % U())>::type> operator%(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<decltype(T() % U())>(v1.r % v2, v1.g % v2, v1.b % v2, v1.a % v2);
}

template<class T, class U>
inline VipRgb<bool> operator&&(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<bool>(v1.r && v2.r, v1.g && v2.g, v1.b && v2.b, v1.a && v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator&&(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<bool>(v1.r && v2, v1.g && v2, v1.b && v2, v1.a && v2);
}
template<class U, class T>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator&&(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<bool>(v1.r && v2, v1.g && v2, v1.b && v2, v1.a && v2);
}

template<class T, class U>
inline VipRgb<bool> operator||(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<bool>(v1.r || v2.r, v1.g || v2.g, v1.b || v2.b, v1.a || v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator||(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<bool>(v1.r || v2, v1.g || v2, v1.b || v2, v1.a || v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator||(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<bool>(v1.r || v2, v1.g || v2, v1.b || v2, v1.a || v2);
}

template<class T, class U>
inline VipRgb<bool> operator<(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<bool>(v1.r < v2.r, v1.g < v2.g, v1.b < v2.b, v1.a < v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator<(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<bool>(v1.r < v2, v1.g < v2, v1.b < v2, v1.a < v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator<(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<bool>(v2 < v1.r, v2 < v1.g, v2 < v1.b, v2 < v1.a);
}

template<class T, class U>
inline VipRgb<bool> operator<=(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<bool>(v1.r <= v2.r, v1.g <= v2.g, v1.b <= v2.b, v1.a <= v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator<=(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<bool>(v1.r <= v2, v1.g <= v2, v1.b <= v2, v1.a <= v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator<=(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<bool>(v2 <= v1.r, v2 <= v1.g, v2 <= v1.b, v2 <= v1.a);
}

template<class T, class U>
inline VipRgb<bool> operator>(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<bool>(v1.r > v2.r, v1.g > v2.g, v1.b > v2.b, v1.a > v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator>(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<bool>(v1.r > v2, v1.g > v2, v1.b > v2, v1.a > v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator>(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<bool>(v2 > v1.r, v2 > v1.g, v2 > v1.b, v2 > v1.a);
}

template<class T, class U>
inline VipRgb<bool> operator>=(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return VipRgb<bool>(v1.r >= v2.r, v1.g >= v2.g, v1.b >= v2.b, v1.a >= v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator>=(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<bool>(v1.r >= v2, v1.g >= v2, v1.b >= v2, v1.a >= v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<bool>>::type operator>=(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<bool>(v2 >= v1.r, v2 >= v1.g, v2 >= v1.b, v2 >= v1.a);
}

template<class T, class U>
inline bool operator==(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return (v1.r == v2.r && v1.g == v2.g && v1.b == v2.b && v1.a == v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, bool>::type operator==(const VipRgb<T>& v1, const U& v2) noexcept
{
	return (v1.r == v2 && v1.g == v2 && v1.b == v2 && v1.a == v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, bool>::type operator==(const U& v2, const VipRgb<T>& v1) noexcept
{
	return bool(v2 == v1.r, v2 == v1.g, v2 == v1.b, v2 == v1.a);
}

template<class T, class U>
inline bool operator!=(const VipRgb<T>& v1, const VipRgb<U>& v2) noexcept
{
	return (v1.r != v2.r && v1.g != v2.g && v1.b != v2.b && v1.a != v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, bool>::type operator!=(const VipRgb<T>& v1, const U& v2) noexcept
{
	return (v1.r != v2 && v1.g != v2 && v1.b != v2 && v1.a != v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, bool>::type operator!=(const U& v2, const VipRgb<T>& v1) noexcept
{
	return bool(v2 != v1.r, v2 != v1.g, v2 != v1.b, v2 != v1.a);
}

template<class T>
inline VipRgb<T> operator!(const VipRgb<T>& v) noexcept
{
	return VipRgb<T>(!v.r, !v.g, !v.b, !v.a);
}

template<class T>
inline VipRgb<T> operator&(const VipRgb<T>& v1, const VipRgb<T>& v2) noexcept
{
	return VipRgb<T>(v1.r & v2.r, v1.g & v2.g, v1.b & v2.b, v1.a & v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>>::type operator&(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<T>(v1.r & v2, v1.g & v2, v1.b & v2, v1.a & v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>>::type operator&(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<T>(v1.r & v2, v1.g & v2, v1.b & v2, v1.a & v2);
}

inline VipRgb<quint8> operator&(const VipRgb<quint8>& v1, const VipRgb<quint8>& v2) noexcept
{
	return VipRgb<quint8>((const QRgb&)v1 & (const QRgb&)v2);
}

template<class T>
inline VipRgb<T> operator|(const VipRgb<T>& v1, const VipRgb<T>& v2) noexcept
{
	return VipRgb<T>(v1.r | v2.r, v1.g | v2.g, v1.b | v2.b, v1.a | v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>>::type operator|(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<T>(v1.r | v2, v1.g | v2, v1.b | v2, v1.a | v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>>::type operator|(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<T>(v1.r | v2, v1.g | v2, v1.b | v2, v1.a | v2);
}

inline VipRgb<quint8> operator|(const VipRgb<quint8>& v1, const VipRgb<quint8>& v2) noexcept
{
	return VipRgb<quint8>((const QRgb&)v1 | (const QRgb&)v2);
}

template<class T>
inline VipRgb<T> operator^(const VipRgb<T>& v1, const VipRgb<T>& v2) noexcept
{
	return VipRgb<T>(v1.r ^ v2.r, v1.g ^ v2.g, v1.b ^ v2.b, v1.a ^ v2.a);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>>::type operator^(const VipRgb<T>& v1, const U& v2) noexcept
{
	return VipRgb<T>(v1.r ^ v2, v1.g ^ v2, v1.b ^ v2, v1.a ^ v2);
}
template<class T, class U>
inline typename std::enable_if<std::is_arithmetic<U>::value, VipRgb<T>>::type operator^(const U& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<T>(v1.r ^ v2, v1.g ^ v2, v1.b ^ v2, v1.a ^ v2);
}

inline VipRgb<quint8> operator^(const VipRgb<quint8>& v1, const VipRgb<quint8>& v2) noexcept
{
	return VipRgb<quint8>((const QRgb&)v1 ^ (const QRgb&)v2);
}

template<class T>
inline VipRgb<T> operator~(const VipRgb<T>& v) noexcept
{
	return VipRgb<T>(~v.r, ~v.g, ~v.b, ~v.a);
}
inline VipRgb<quint8> operator~(const VipRgb<quint8>& v) noexcept
{
	return (const VipRgb<quint8>&)(~(const QRgb&)v);
}

template<class T>
Q_DECL_CONSTEXPR static inline bool vipIsNan(const VipRgb<T>& v) noexcept
{
	return vipIsNan(v.r) || vipIsNan(v.g) || vipIsNan(v.b) || vipIsNan(v.a);
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipIsInf(const VipRgb<T>& v) noexcept
{
	return vipIsInf(v.r) || vipIsInf(v.g) || vipIsInf(v.b) || vipIsInf(v.a);
}
template<class T>
Q_DECL_CONSTEXPR static inline VipRgb<T> vipFloor(const VipRgb<T>& v) noexcept
{
	return VipRgb<T>(vipFloor(v.r), vipFloor(v.g), vipFloor(v.b), vipFloor(v.a));
}
template<class T>
Q_DECL_CONSTEXPR static inline VipRgb<T> vipCeil(const VipRgb<T>& v) noexcept
{
	return VipRgb<T>(vipCeil(v.r), vipCeil(v.g), vipCeil(v.b), vipCeil(v.a));
}
template<class T>
Q_DECL_CONSTEXPR static inline VipRgb<T> vipRound(const VipRgb<T>& v) noexcept
{
	return VipRgb<T>(vipRound(v.r), vipRound(v.g), vipRound(v.b), vipRound(v.a));
}
template<class T>
Q_DECL_CONSTEXPR static inline VipRgb<T> vipAbs(const VipRgb<T>& v) noexcept
{
	return VipRgb<T>(vipAbs(v.r), vipAbs(v.g), vipAbs(v.b), vipAbs(v.a));
}
template<class T>
Q_DECL_CONSTEXPR static inline bool vipFuzzyCompare(const VipRgb<T>& v1, VipRgb<T>& v2) noexcept
{
	return vipFuzzyCompare(v1.r, v2.r) && vipFuzzyCompare(v1.g, v2.g) && vipFuzzyCompare(v1.b, v2.b) && vipFuzzyCompare(v1.a, v2.a);
}

template<class T>
inline VipRgb<T> vipMin(const VipRgb<T>& v1, const VipRgb<T>& v2) noexcept
{
	return VipRgb<T>(std::min(v1.r, v2.r), std::min(v1.g, v2.g), std::min(v1.b, v2.b), std::min(v1.a, v2.a));
}
template<class T>
inline VipRgb<T> vipMin(const VipRgb<T>& v1, const T& v2) noexcept
{
	return VipRgb<T>(std::min(v1.r, v2), std::min(v1.g, v2), std::min(v1.b, v2), std::min(v1.a, v2));
}
template<class T, class U>
inline VipRgb<T> vipMin(const T& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<T>(std::min(v1.r, v2), std::min(v1.g, v2), std::min(v1.b, v2), std::min(v1.a, v2));
}

template<class T>
inline VipRgb<T> vipMax(const VipRgb<T>& v1, const VipRgb<T>& v2) noexcept
{
	return VipRgb<T>(std::max(v1.r, v2.r), std::max(v1.g, v2.g), std::max(v1.b, v2.b), std::max(v1.a, v2.a));
}
template<class T>
inline VipRgb<T> vipMax(const VipRgb<T>& v1, const T& v2) noexcept
{
	return VipRgb<T>(std::max(v1.r, v2), std::max(v1.g, v2), std::max(v1.b, v2), std::max(v1.a, v2));
}
template<class T, class U>
inline VipRgb<T> vipMax(const T& v2, const VipRgb<T>& v1) noexcept
{
	return VipRgb<T>(std::max(v1.r, v2), std::max(v1.g, v2), std::max(v1.b, v2), std::max(v1.a, v2));
}

template<class T>
VipRgb<T> vipClamp(const VipRgb<T>& v, const VipRgb<T>& mi, const VipRgb<T>& ma) noexcept
{
	return VipRgb<T>(v.r < mi ? mi : (v.r > ma ? ma : v.r), v.g < mi ? mi : (v.g > ma ? ma : v.g), v.b < mi ? mi : (v.b > ma ? ma : v.b), v.a < mi ? mi : (v.a > ma ? ma : v.a));
}

template<class T>
VipRgb<T> vipReplaceNan(const VipRgb<T>& v, const VipRgb<T>& m) noexcept
{
	return VipRgb<T>(vipIsNan(v.r) ? m.r : v.r, vipIsNan(v.g) ? m.g : v.g, vipIsNan(v.b) ? m.b : v.b, vipIsNan(v.a) ? m.a : v.a);
}

template<class T>
VipRgb<T> vipReplaceInf(const VipRgb<T>& v, const VipRgb<T>& m) noexcept
{
	return VipRgb<T>(vipIsInf(v.r) ? m.r : v.r, vipIsInf(v.g) ? m.g : v.g, vipIsInf(v.b) ? m.b : v.b, vipIsInf(v.a) ? m.a : v.a);
}

template<class T>
VipRgb<T> vipReplaceNanInf(const VipRgb<T>& v, const VipRgb<T>& m) noexcept
{
	return VipRgb<T>(
	  vipIsNan(v.r) || vipIsInf(v.r) ? m.r : v.r, vipIsNan(v.g) || vipIsInf(v.g) ? m.g : v.g, vipIsNan(v.b) || vipIsInf(v.b) ? m.b : v.b, vipIsNan(v.a) || vipIsInf(v.a) ? m.a : v.a);
}

template<class T, class U, class V>
VipRgb<decltype(U() + V())> vipWhere(const VipRgb<T>& cond, const VipRgb<U>& v1, const VipRgb<V>& v2) noexcept
{
	return VipRgb<decltype(U() + V())>(cond.r ? v1.r : v2.r, cond.g ? v1.g : v2.g, cond.b ? v1.b : v2.b, cond.a ? v1.a : v2.a);
}

#endif