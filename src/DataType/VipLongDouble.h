#pragma once

#include <qmetatype.h>
#include <qstring.h>
#include <qdatastream.h>
#include <qtextstream.h>
#include <qpoint.h>
#include <qvector.h>

#include <sstream>
#include <iomanip>
#include <limits>
#include <cstdint>

#include "VipMath.h"
#include "VipConfig.h"


/// \addtogroup DataType
/// @{

/// long double type, always 8 bytes with msvc, usually 16 bytes on other platforms
typedef long double vip_long_double;
Q_DECLARE_METATYPE(vip_long_double)

/// Byte swap long double
inline vip_long_double vipSwapLongDouble(const vip_long_double v)
{
	vip_long_double r = v;
	char * raw = (char*)(&r);
	for (unsigned i = 0; i < sizeof(vip_long_double) / 2; ++i)
		std::swap(raw[i], raw[sizeof(vip_long_double) - i - 1]);
	return r;
}

/// Convert long double value to string with full precision
VIP_DATA_TYPE_EXPORT QString vipLongDoubleToString(const vip_long_double v);
/// Convert long double value to byte array with full precision
VIP_DATA_TYPE_EXPORT QByteArray vipLongDoubleToByteArray(const vip_long_double v);
/// Extract long double from string
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromString(const QString & str, bool *ok = NULL);
/// Extract long double from QByteArray
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromByteArray(const QByteArray & str, bool *ok=NULL);
/// Convert long double to string with full precision using given locale
VIP_DATA_TYPE_EXPORT QString vipLongDoubleToStringLocale(const vip_long_double v, const QLocale &);
/// Convert long double to byte array with full precision using given locale
VIP_DATA_TYPE_EXPORT QByteArray vipLongDoubleToByteArrayLocale(const vip_long_double v, const QLocale &);
/// Extract long double from string using given locale
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromStringLocale(const QString & str, const QLocale &, bool *ok = NULL);
/// Extract long double from byte array using given locale
VIP_DATA_TYPE_EXPORT vip_long_double vipLongDoubleFromByteArrayLocale(const QByteArray & str, const QLocale &, bool *ok = NULL);
/// Write long double to QTextStream
VIP_DATA_TYPE_EXPORT QTextStream & operator<<(QTextStream & s, vip_long_double v);
/// Read long double from QTextStream
VIP_DATA_TYPE_EXPORT QTextStream & operator>>(QTextStream & s, vip_long_double & v);

/// Write several long double values iterating over \a values by \a step until \a size is reached.
/// Each value is separated by \a sep.
VIP_DATA_TYPE_EXPORT void vipWriteNLongDouble(QTextStream & s, const vip_long_double * values, int size, int step , const char * sep);
/// Read up to \a max_count long double from QTextStream
VIP_DATA_TYPE_EXPORT int vipReadNLongDouble(QTextStream & s, vip_long_double * values, int max_count);
/// Read up to \a max_count long double from QTextStream
VIP_DATA_TYPE_EXPORT int vipReadNLongDouble(QTextStream & s, QVector<vip_long_double> & values, int max_count);

inline double vipLELongDoubleToDouble(const unsigned char x[10])
{
	//https://stackoverflow.com/questions/2963055/msvc-win32-convert-extended-precision-float-80-bit-to-double-64-bit

	int exponent = (((x[9] << 8) | x[8]) & 0x7FFF);
	std::uint64_t mantissa =
		((std::uint64_t)x[7] << 56) | ((std::uint64_t)x[6] << 48) | ((std::uint64_t)x[5] << 40) | ((std::uint64_t)x[4] << 32) |
		((std::uint64_t)x[3] << 24) | ((std::uint64_t)x[2] << 16) | ((std::uint64_t)x[1] << 8) | (std::uint64_t)x[0];
	unsigned char d[8] = { 0 };
	double result;

	d[7] = x[9] & 0x80; // Set sign.

	if ((exponent == 0x7FFF) || (exponent == 0))
	{
		// Infinite, NaN or denormal
		if (exponent == 0x7FFF)
		{
			// Infinite or NaN
			d[7] |= 0x7F;
			d[6] = 0xF0;
		}
		else
		{
			// Otherwise it's denormal. It cannot be represented as double. Translate as singed zero.
			memcpy(&result, d, 8);
			return result;
		}
	}
	else
	{
		// Normal number.
		exponent = exponent - 0x3FFF + 0x03FF; //< exponent for double precision.

		if (exponent <= -52)  //< Too small to represent. Translate as (signed) zero.
		{
			memcpy(&result, d, 8);
			return result;
		}
		else if (exponent < 0)
		{
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
		else
		{
			// Representable number
			d[7] |= (exponent & 0x7F0) >> 4;
			d[6] |= (exponent & 0xF) << 4;
		}
	}
	// Translate mantissa.

	mantissa >>= 11;

	if (exponent < 0)
	{
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


inline QDataStream & vipWriteLELongDouble(QDataStream & s, vip_long_double v)
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

inline QDataStream & vipReadLELongDouble(QDataStream & s, vip_long_double & v)
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
inline QDataStream & operator<<(QDataStream & s, vip_long_double v)
{
	return vipWriteLELongDouble(s, v);
}
/// Read long double from QDataStream
inline QDataStream & operator>>(QDataStream & s, vip_long_double & v)
{
	return vipReadLELongDouble(s, v);
}







/// Point class with an interface similar to QPointF.
/// Supports coordinates of type float, double and long double.
template< class T>
class VipFloatPoint
{
public:
    Q_DECL_CONSTEXPR VipFloatPoint();
    Q_DECL_CONSTEXPR VipFloatPoint(const QPoint &p);
	Q_DECL_CONSTEXPR VipFloatPoint(const QPointF &p);
    Q_DECL_CONSTEXPR VipFloatPoint(T xpos, T ypos);
    template< class U>
    VipFloatPoint(const VipFloatPoint<U> & other) : xp(other.x()), yp(other.y()) {}

    Q_DECL_CONSTEXPR inline T manhattanLength() const;

    inline bool isNull() const;

    Q_DECL_CONSTEXPR inline T x() const;
    Q_DECL_CONSTEXPR inline T y() const;
    Q_DECL_RELAXED_CONSTEXPR inline void setX(T x);
    Q_DECL_RELAXED_CONSTEXPR inline void setY(T y);

    Q_DECL_RELAXED_CONSTEXPR inline T &rx();
    Q_DECL_RELAXED_CONSTEXPR inline T &ry();

    Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint &operator+=(const VipFloatPoint &p);
    Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint &operator-=(const VipFloatPoint &p);
    Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint &operator*=(T c);
    Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint &operator/=(T c);

    Q_DECL_CONSTEXPR static inline T dotProduct(const VipFloatPoint &p1, const VipFloatPoint &p2)
    { return p1.xp * p2.xp + p1.yp * p2.yp; }

    QPoint toPoint() const;
	Q_DECL_CONSTEXPR QPointF toPointF() const;

	operator QPointF() const { return toPointF(); }
	template< class U>
	operator VipFloatPoint<U>() const { return VipFloatPoint<U>(*this); }

	static QPoint toPoint(const VipFloatPoint & pt) {return pt.toPoint();}
	static QPointF toPointF(const VipFloatPoint & pt) { return pt.toPointF(); }
	static VipFloatPoint fromPoint(const QPoint & pt) {return VipFloatPoint	(pt.x(), pt.y());}
	static VipFloatPoint fromPointF(const QPointF & pt) { return VipFloatPoint(pt.x(), pt.y()); }

private:
    T xp;
    T yp;
};




//****************************************************************************
// VipFloatPoint inline functions
//*****************************************************************************
template<class T>
Q_DECL_CONSTEXPR inline VipFloatPoint<T>::VipFloatPoint() : xp(0), yp(0) { }
template<class T>
Q_DECL_CONSTEXPR inline VipFloatPoint<T>::VipFloatPoint(T xpos, T ypos) : xp(xpos), yp(ypos) { }
template<class T>
Q_DECL_CONSTEXPR inline VipFloatPoint<T>::VipFloatPoint(const QPoint &p) : xp(p.x()), yp(p.y()) { }
template<class T>
Q_DECL_CONSTEXPR inline VipFloatPoint<T>::VipFloatPoint(const QPointF &p) : xp(p.x()), yp(p.y()) { }
template<class T>
Q_DECL_CONSTEXPR inline T VipFloatPoint<T>::manhattanLength() const
{
    return qAbs(x())+qAbs(y());
}
template<class T>
inline bool VipFloatPoint<T>::isNull() const
{
    return xp == (T)0 && yp == (T)0;
}
template<class T>
Q_DECL_CONSTEXPR inline T VipFloatPoint<T>::x() const
{
    return xp;
}
template<class T>
Q_DECL_CONSTEXPR inline T VipFloatPoint<T>::y() const
{
    return yp;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline void VipFloatPoint<T>::setX(T xpos)
{
    xp = xpos;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline void VipFloatPoint<T>::setY(T ypos)
{
    yp = ypos;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline T &VipFloatPoint<T>::rx()
{
    return xp;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline T &VipFloatPoint<T>::ry()
{
    return yp;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint<T> &VipFloatPoint<T>::operator+=(const VipFloatPoint<T> &p)
{
    xp+=p.xp;
    yp+=p.yp;
    return *this;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint<T> &VipFloatPoint<T>::operator-=(const VipFloatPoint<T> &p)
{
    xp-=p.xp; yp-=p.yp; return *this;
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint<T> &VipFloatPoint<T>::operator*=(T c)
{
    xp*=c; yp*=c; return *this;
}


template<class T>
inline bool operator==(const VipFloatPoint<T> &p1, const VipFloatPoint<T> &p2)
{
    return qFuzzyIsNull(p1.x() - p2.x()) && qFuzzyIsNull(p1.y() - p2.y());
}
template<class T>
inline bool operator!=(const VipFloatPoint<T> &p1, const VipFloatPoint<T> &p2)
{
    return !qFuzzyIsNull(p1.x() - p2.x()) || !qFuzzyIsNull(p1.y() - p2.y());
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator+(const VipFloatPoint<T> &p1, const VipFloatPoint<T> &p2)
{
    return VipFloatPoint<T>(p1.x()+p2.x(), p1.y()+p2.y());
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator-(const VipFloatPoint<T> &p1, const VipFloatPoint<T> &p2)
{
    return VipFloatPoint<T>(p1.x()-p2.x(), p1.y()-p2.y());
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator*(const VipFloatPoint<T> &p, T c)
{
    return VipFloatPoint<T>(p.x()*c, p.y()*c);
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator*(T c, const VipFloatPoint<T> &p)
{
    return VipFloatPoint<T>(p.x()*c, p.y()*c);
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator+(const VipFloatPoint<T> &p)
{
    return p;
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator-(const VipFloatPoint<T> &p)
{
    return VipFloatPoint<T>(-p.x(), -p.y());
}
template<class T>
Q_DECL_RELAXED_CONSTEXPR inline VipFloatPoint<T> &VipFloatPoint<T>::operator/=(T divisor)
{
    xp/=divisor;
    yp/=divisor;
    return *this;
}
template<class T>
Q_DECL_CONSTEXPR inline const VipFloatPoint<T> operator/(const VipFloatPoint<T> &p, T divisor)
{
    return VipFloatPoint<T>(p.x()/divisor, p.y()/divisor);
}
template<class T>
inline QPoint VipFloatPoint<T>::toPoint() const
{
    return QPoint(qRound(xp), qRound(yp));
}
template<class T>
Q_DECL_CONSTEXPR inline QPointF VipFloatPoint<T>::toPointF() const
{
	return QPointF((xp), (yp));
}





#ifdef VIP_USE_LONG_DOUBLE

typedef vip_long_double vip_double;
typedef VipFloatPoint<vip_long_double> VipLongPoint;
typedef VipFloatPoint<vip_long_double> VipPoint;

Q_DECLARE_TYPEINFO(VipPoint, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(VipPoint)

static constexpr quint32 vip_LD_support = ((1U << 31U) | sizeof(long double));

#else

typedef double vip_double;
typedef VipFloatPoint<vip_long_double> VipLongPoint;
typedef VipFloatPoint<vip_double> VipPoint;

Q_DECLARE_TYPEINFO(VipLongPoint, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(VipLongPoint)
Q_DECLARE_TYPEINFO(VipPoint, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(VipPoint)

static constexpr quint32 vip_LD_support = sizeof(long double);

#endif

/// Read a vip_double from a QDataStream.
/// \a LD_support describe the way the vip_double was stored (with long double support or not and with the sizeof long double).
/// \a LD_support correspond to the \a vip_LD_support value when the data was stored.
inline vip_double vipReadLEDouble(unsigned LD_support , QDataStream & stream)
{
	//Check whever the double was saved with long double support and grab the long double size
	unsigned has_LD = LD_support & (1U << 31U);
	vip_double v;

	if (has_LD) { //was saved as long double
		unsigned LD_size = LD_support & ~(1U << 31U);
#ifdef VIP_USE_LONG_DOUBLE
		if (LD_size == sizeof(long double)) {
			//read raw long double
			vipReadLELongDouble(stream, v);
		}
		else
#endif
		if (LD_size == 8) {
			//read a double (size_of_longDouble == 0 means that this is a older Thermavip version using double)
			double val;
			stream >> val;
			v = val;
		}
		else {//case where read long double is of size 16 (or 12) and this platform long double is size 8
			  //read a 80 bits long double as double
			  //the long double must have been stored in little endian
			unsigned char data[16];
			stream.readRawData((char*)data, LD_size);
			v = (vipLELongDoubleToDouble(data));
		}
	}
	else {
		//was saved as double
		double val;
		stream >> val;
		v = val;
	}
	return v;
}


/// Read a vip_long_double from a QDataStream.
/// \a LD_support describe the way the vip_long_double was stored (with long double support or not and with the sizeof long double).
/// \a LD_support corresponds to the \a vip_LD_support value when the data was stored.
inline vip_long_double vipReadLELongDouble(unsigned LD_support, QDataStream & stream)
{
	//Check whever the double was saved with long double support and grab the long double size
	unsigned LD_size = LD_support & ~(1U << 31U);
	vip_long_double v;

	{ //was saved as long double
		if (LD_size == sizeof(long double)) {
			//read raw long double
			vipReadLELongDouble(stream, v);
		}
		else if (LD_size == 8) {
			//read a double (size_of_longDouble == 0 means that this is a older Thermavip version using double)
			double val;
			stream >> val;
			v = val;
		}
		else {//case where read long double is of size 16 (or 12) and this platform long double is size 8
				//read a 80 bits long double as double
				//the long double must have been stored in little endian
			unsigned char data[16];
			stream.readRawData((char*)data, LD_size);
			v = (vipLELongDoubleToDouble(data));
		}
	}
	return v;
}




/// @}
//end DataType
