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

#ifndef VIP_LONG_DOUBLE_H
#define VIP_LONG_DOUBLE_H

#include <qdatastream.h>
#include <qmetatype.h>
#include <qpoint.h>
#include <qstring.h>
#include <qtextstream.h>
#include <qvector.h>

#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

#include "VipConfig.h"
#include "VipMath.h"

/// \addtogroup DataType
/// @{

/// long double type, always 8 bytes with msvc, usually 16 bytes on other platforms
typedef long double vip_long_double;
Q_DECLARE_METATYPE(vip_long_double)

/// Byte swap long double
inline vip_long_double vipSwapLongDouble(const vip_long_double v) noexcept
{
	vip_long_double r = v;
	char* raw = (char*)(&r);
	for (unsigned i = 0; i < sizeof(vip_long_double) / 2; ++i)
		std::swap(raw[i], raw[sizeof(vip_long_double) - i - 1]);
	return r;
}

/// Convert long double value to string with full precision
VIP_DATA_TYPE_EXPORT QString vipLongDoubleToString(const vip_long_double v);
/// Convert long double value to byte array with full precision
VIP_DATA_TYPE_EXPORT QByteArray vipLongDoubleToByteArray(const vip_long_double v);
/// Extract long double from string
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromString(const QString& str, bool* ok = nullptr);
/// Extract long double from QByteArray
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromByteArray(const QByteArray& str, bool* ok = nullptr);
/// Convert long double to string with full precision using given locale
VIP_DATA_TYPE_EXPORT QString vipLongDoubleToStringLocale(const vip_long_double v, const QLocale&);
/// Convert long double to byte array with full precision using given locale
VIP_DATA_TYPE_EXPORT QByteArray vipLongDoubleToByteArrayLocale(const vip_long_double v, const QLocale&);
/// Extract long double from string using given locale
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromStringLocale(const QString& str, const QLocale&, bool* ok = nullptr);
/// Extract long double from byte array using given locale
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromByteArrayLocale(const QByteArray& str, const QLocale&, bool* ok = nullptr);
/// Write long double to QTextStream
VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, vip_long_double v);
/// Read long double from QTextStream
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, vip_long_double& v);

/// Write several long double values iterating over \a values by \a step until \a size is reached.
/// Each value is separated by \a sep.
VIP_DATA_TYPE_EXPORT void vipWriteNLongDouble(QTextStream& s, const vip_long_double* values, int size, int step, const char* sep);
/// Read up to \a max_count long double from QTextStream
VIP_DATA_TYPE_EXPORT int vipReadNLongDouble(QTextStream& s, vip_long_double* values, int max_count);
/// Read up to \a max_count long double from QTextStream
VIP_DATA_TYPE_EXPORT int vipReadNLongDouble(QTextStream& s, QVector<vip_long_double>& values, int max_count);

inline double vipLELongDoubleToDouble(const unsigned char x[10]) noexcept
{
	// https://stackoverflow.com/questions/2963055/msvc-win32-convert-extended-precision-float-80-bit-to-double-64-bit

	int exponent = (((x[9] << 8) | x[8]) & 0x7FFF);
	std::uint64_t mantissa = ((std::uint64_t)x[7] << 56) | ((std::uint64_t)x[6] << 48) | ((std::uint64_t)x[5] << 40) | ((std::uint64_t)x[4] << 32) | ((std::uint64_t)x[3] << 24) |
				 ((std::uint64_t)x[2] << 16) | ((std::uint64_t)x[1] << 8) | (std::uint64_t)x[0];
	unsigned char d[8] = { 0 };
	double result;

	d[7] = x[9] & 0x80; // Set sign.

	if ((exponent == 0x7FFF) || (exponent == 0)) {
		// Infinite, NaN or denormal
		if (exponent == 0x7FFF) {
			// Infinite or NaN
			d[7] |= 0x7F;
			d[6] = 0xF0;
		}
		else {
			// Otherwise it's denormal. It cannot be represented as double. Translate as singed zero.
			memcpy(&result, d, 8);
			return result;
		}
	}
	else {
		// Normal number.
		exponent = exponent - 0x3FFF + 0x03FF; //< exponent for double precision.

		if (exponent <= -52) //< Too small to represent. Translate as (signed) zero.
		{
			memcpy(&result, d, 8);
			return result;
		}
		else if (exponent < 0) {
			// Denormal, exponent bits are already zero here.
		}
		else if (exponent >= 0x7FF) //< Too large to represent. Translate as infinite.
		{
			d[7] |= 0x7F;
			d[6] = 0xF0;
			memset(d, 0x00, 6);
			memcpy(&result, d, 8);
			return result;
		}
		else {
			// Representable number
			d[7] |= (exponent & 0x7F0) >> 4;
			d[6] |= (exponent & 0xF) << 4;
		}
	}
	// Translate mantissa.

	mantissa >>= 11;

	if (exponent < 0) {
		// Denormal, further shifting is required here.
		mantissa >>= (-exponent + 1);
	}

	d[0] = mantissa & 0xFF;
	d[1] = (mantissa >> 8) & 0xFF;
	d[2] = (mantissa >> 16) & 0xFF;
	d[3] = (mantissa >> 24) & 0xFF;
	d[4] = (mantissa >> 32) & 0xFF;
	d[5] = (mantissa >> 40) & 0xFF;
	d[6] |= (mantissa >> 48) & 0x0F;

	memcpy(&result, d, 8);
	return result;
}

inline QDataStream& vipWriteLELongDouble(QDataStream& s, vip_long_double v)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	if (s.byteOrder() == QDataStream::BigEndian)
		v = vipSwapLongDouble(v);
#else
	if (s.byteOrder() == QDataStream::LittleEndian)
		v = vipSwapLongDouble(v);
#endif
	s.writeRawData((const char*)(&v), sizeof(v));
	return s;
}

inline QDataStream& vipReadLELongDouble(QDataStream& s, vip_long_double& v)
{
	s.readRawData((char*)&v, sizeof(vip_long_double));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	if (s.byteOrder() == QDataStream::BigEndian)
		v = vipSwapLongDouble(v);
#else
	if (s.byteOrder() == QDataStream::LittleEndian)
		v = vipSwapLongDouble(v);
#endif
	return s;
}

/// Write long double to QDataStream
VIP_ALWAYS_INLINE QDataStream& operator<<(QDataStream& s, vip_long_double v)
{
	return vipWriteLELongDouble(s, v);
}
/// Read long double from QDataStream
VIP_ALWAYS_INLINE QDataStream& operator>>(QDataStream& s, vip_long_double& v)
{
	return vipReadLELongDouble(s, v);
}

/// Point class with an interface similar to QPointF.
/// Supports coordinates of type float, double and long double.
template<class T>
class VipFloatPoint
{
public:
	Q_DECL_CONSTEXPR VipFloatPoint() noexcept;
	Q_DECL_CONSTEXPR VipFloatPoint(const QPoint& p) noexcept;
	Q_DECL_CONSTEXPR VipFloatPoint(const QPointF& p) noexcept;
	Q_DECL_CONSTEXPR VipFloatPoint(T xpos, T ypos) noexcept;
	template<class U>
	VipFloatPoint(const VipFloatPoint<U>& other) noexcept
	  : xp(other.x())
	  , yp(other.y())
	{
	}

	Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE T manhattanLength() const noexcept;

	VIP_ALWAYS_INLINE bool isNull() const noexcept;

	Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE T x() const noexcept;
	Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE T y() const noexcept;
	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE void setX(T x) noexcept;
	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE void setY(T y) noexcept;

	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE T& rx() noexcept;
	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE T& ry() noexcept;

	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint& operator+=(const VipFloatPoint& p) noexcept;
	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint& operator-=(const VipFloatPoint& p) noexcept;
	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint& operator*=(T c) noexcept;
	Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint& operator/=(T c) noexcept;

	Q_DECL_CONSTEXPR static VIP_ALWAYS_INLINE T dotProduct(const VipFloatPoint& p1, const VipFloatPoint& p2) noexcept { return p1.xp * p2.xp + p1.yp * p2.yp; }

	QPoint toPoint() const noexcept;
	Q_DECL_CONSTEXPR QPointF toPointF() const noexcept;

	operator QPointF() const noexcept { return toPointF(); }
	template<class U>
	operator VipFloatPoint<U>() const noexcept
	{
		return VipFloatPoint<U>(*this);
	}

	static QPoint toPoint(const VipFloatPoint& pt) noexcept { return pt.toPoint(); }
	static QPointF toPointF(const VipFloatPoint& pt) noexcept { return pt.toPointF(); }
	static VipFloatPoint fromPoint(const QPoint& pt) noexcept { return VipFloatPoint(pt.x(), pt.y()); }
	static VipFloatPoint fromPointF(const QPointF& pt) noexcept { return VipFloatPoint(pt.x(), pt.y()); }

private:
	T xp;
	T yp;
};

//****************************************************************************
// VipFloatPoint VIP_ALWAYS_INLINE functions
//*****************************************************************************
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>::VipFloatPoint() noexcept
  : xp(0)
  , yp(0)
{
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>::VipFloatPoint(T xpos, T ypos) noexcept
  : xp(xpos)
  , yp(ypos)
{
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>::VipFloatPoint(const QPoint& p) noexcept
  : xp(p.x())
  , yp(p.y())
{
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>::VipFloatPoint(const QPointF& p) noexcept
  : xp(p.x())
  , yp(p.y())
{
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE T VipFloatPoint<T>::manhattanLength() const noexcept
{
	return qAbs(x()) + qAbs(y());
}
template<class T>
VIP_ALWAYS_INLINE bool VipFloatPoint<T>::isNull() const noexcept
{
	return xp == (T)0 && yp == (T)0;
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE T VipFloatPoint<T>::x() const noexcept
{
	return xp;
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE T VipFloatPoint<T>::y() const noexcept
{
	return yp;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE void VipFloatPoint<T>::setX(T xpos) noexcept
{
	xp = xpos;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE void VipFloatPoint<T>::setY(T ypos) noexcept
{
	yp = ypos;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE T& VipFloatPoint<T>::rx() noexcept
{
	return xp;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE T& VipFloatPoint<T>::ry() noexcept
{
	return yp;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>& VipFloatPoint<T>::operator+=(const VipFloatPoint<T>& p) noexcept
{
	xp += p.xp;
	yp += p.yp;
	return *this;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>& VipFloatPoint<T>::operator-=(const VipFloatPoint<T>& p) noexcept
{
	xp -= p.xp;
	yp -= p.yp;
	return *this;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>& VipFloatPoint<T>::operator*=(T c) noexcept
{
	xp *= c;
	yp *= c;
	return *this;
}

template<class T>
VIP_ALWAYS_INLINE bool operator==(const VipFloatPoint<T>& p1, const VipFloatPoint<T>& p2) noexcept
{
	return qFuzzyIsNull(p1.x() - p2.x()) && qFuzzyIsNull(p1.y() - p2.y());
}
template<class T>
VIP_ALWAYS_INLINE bool operator!=(const VipFloatPoint<T>& p1, const VipFloatPoint<T>& p2) noexcept
{
	return !qFuzzyIsNull(p1.x() - p2.x()) || !qFuzzyIsNull(p1.y() - p2.y());
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator+(const VipFloatPoint<T>& p1, const VipFloatPoint<T>& p2) noexcept
{
	return VipFloatPoint<T>(p1.x() + p2.x(), p1.y() + p2.y());
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator-(const VipFloatPoint<T>& p1, const VipFloatPoint<T>& p2) noexcept
{
	return VipFloatPoint<T>(p1.x() - p2.x(), p1.y() - p2.y());
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator*(const VipFloatPoint<T>& p, T c) noexcept
{
	return VipFloatPoint<T>(p.x() * c, p.y() * c);
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator*(T c, const VipFloatPoint<T>& p) noexcept
{
	return VipFloatPoint<T>(p.x() * c, p.y() * c);
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator+(const VipFloatPoint<T>& p) noexcept
{
	return p;
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator-(const VipFloatPoint<T>& p) noexcept
{
	return VipFloatPoint<T>(-p.x(), -p.y());
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR VIP_ALWAYS_INLINE VipFloatPoint<T>& VipFloatPoint<T>::operator/=(T divisor) noexcept
{
	xp /= divisor;
	yp /= divisor;
	return *this;
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE const VipFloatPoint<T> operator/(const VipFloatPoint<T>& p, T divisor) noexcept
{
	return VipFloatPoint<T>(p.x() / divisor, p.y() / divisor);
}
template<class T>
VIP_ALWAYS_INLINE QPoint VipFloatPoint<T>::toPoint() const noexcept
{
	return QPoint(qRound(xp), qRound(yp));
}
template<class T>
Q_DECL_CONSTEXPR VIP_ALWAYS_INLINE QPointF VipFloatPoint<T>::toPointF() const noexcept
{
	return QPointF((xp), (yp));
}

#if VIP_USE_LONG_DOUBLE == 1

typedef vip_long_double vip_double;
typedef VipFloatPoint<vip_long_double> VipLongPoint;
typedef VipFloatPoint<vip_long_double> VipPoint;

VIP_IS_RELOCATABLE(VipPoint);
Q_DECLARE_METATYPE(VipPoint)

static constexpr quint32 vip_LD_support = ((1U << 31U) | sizeof(long double));

#else

typedef double vip_double;
typedef VipFloatPoint<vip_long_double> VipLongPoint;
typedef VipFloatPoint<vip_double> VipPoint;

VIP_IS_RELOCATABLE(VipLongPoint);
Q_DECLARE_METATYPE(VipLongPoint)
VIP_IS_RELOCATABLE(VipPoint);
Q_DECLARE_METATYPE(VipPoint)

static constexpr quint32 vip_LD_support = sizeof(long double);

#endif

/// Read a vip_double from a QDataStream.
/// \a LD_support describe the way the vip_double was stored (with long double support or not and with the sizeof long double).
/// \a LD_support correspond to the \a vip_LD_support value when the data was stored.
VIP_ALWAYS_INLINE vip_double vipReadLEDouble(unsigned LD_support, QDataStream& stream)
{
	// Check whever the double was saved with long double support and grab the long double size
	unsigned has_LD = LD_support & (1U << 31U);
	vip_double v;

	if (has_LD) { // was saved as long double
		unsigned LD_size = LD_support & ~(1U << 31U);
#if VIP_USE_LONG_DOUBLE == 1
		if (LD_size == sizeof(long double)) {
			// read raw long double
			vipReadLELongDouble(stream, v);
		}
		else
#endif
		  if (LD_size == 8) {
			// read a double (size_of_longDouble == 0 means that this is a older Thermavip version using double)
			double val;
			stream >> val;
			v = val;
		}
		else { // case where read long double is of size 16 (or 12) and this platform long double is size 8
		       // read a 80 bits long double as double
		       // the long double must have been stored in little endian
			unsigned char data[16];
			stream.readRawData((char*)data, LD_size);
			v = (vipLELongDoubleToDouble(data));
		}
	}
	else {
		// was saved as double
		double val;
		stream >> val;
		v = val;
	}
	return v;
}

/// Read a vip_long_double from a QDataStream.
/// \a LD_support describe the way the vip_long_double was stored (with long double support or not and with the sizeof long double).
/// \a LD_support corresponds to the \a vip_LD_support value when the data was stored.
VIP_ALWAYS_INLINE vip_long_double vipReadLELongDouble(unsigned LD_support, QDataStream& stream)
{
	// Check whever the double was saved with long double support and grab the long double size
	unsigned LD_size = LD_support & ~(1U << 31U);
	vip_long_double v;

	{ // was saved as long double
		if (LD_size == sizeof(long double)) {
			// read raw long double
			vipReadLELongDouble(stream, v);
		}
		else if (LD_size == 8) {
			// read a double (size_of_longDouble == 0 means that this is a older Thermavip version using double)
			double val;
			stream >> val;
			v = val;
		}
		else { // case where read long double is of size 16 (or 12) and this platform long double is size 8
		       // read a 80 bits long double as double
		       // the long double must have been stored in little endian
			unsigned char data[16];
			stream.readRawData((char*)data, LD_size);
			v = (vipLELongDoubleToDouble(data));
		}
	}
	return v;
}

/// @}
// end DataType

#endif