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

#ifndef VIP_INTERNAL_CONVERT_H
#define VIP_INTERNAL_CONVERT_H

#include <cfloat>
#include <complex>
#include <type_traits>

#include <qstring.h>
#include <qtextstream.h>

#include "VipComplex.h"
#include "VipHybridVector.h"
#include "VipLongDouble.h"
#include "VipRgb.h"
#include "VipUtils.h"

/// \addtogroup DataType
/// @{

/// Type trait for std::complex types
template<class T>
struct is_complex : std::false_type
{
};

template<class T>
struct is_complex<std::complex<T>> : std::true_type
{
};

/// Type trait for QString
template<class T>
struct is_qstring : std::false_type
{
};

template<>
struct is_qstring<QString> : std::true_type
{
};

/// Type trait for QByteArray
template<class T>
struct is_qbytearray : std::false_type
{
};

template<>
struct is_qbytearray<QByteArray> : std::true_type
{
};

/// Type trait for QString or QByteArray
template<class T>
struct is_q_string_or_bytearray
{
	static const bool value = is_qstring<T>::value || is_qbytearray<T>::value;
};

/// Type trait chacking if T1() < T2() is a valid operation
template<class T1, class T2, class = void>
struct has_lesser_operator : std::false_type
{
};
template<class T1, class T2>
struct has_lesser_operator<T1, T2, std::void_t<decltype(T1() < T2())>> : std::true_type
{
};

/// Type trait chacking if T1() > T2() is a valid operation
template<class T1, class T2, class = void>
struct has_greater_operator : std::false_type
{
};
template<class T1, class T2>
struct has_greater_operator<T1, T2, std::void_t<decltype(T1() > T2())>> : std::true_type
{
};

/// @}
// end DataType

namespace detail
{
	///\internal null type used for functor expressions
	struct NullType
	{
	};

	/// \internal Convert any object to its string representation based on \a QTextStream.
	template<class T>
	QString typeToString(const T& value)
	{
		QByteArray res;
		QTextStream stream(&res, QIODevice::WriteOnly);
		stream << value;
		stream.flush();
		return QString(res);
	}

	/// \internal Convert a string to any object type based on \a QTextStream.
	template<class T>
	T stringToType(const QString& str)
	{
		T res;
		QTextStream stream(const_cast<QString*>(&str), QIODevice::ReadOnly);
		stream >> res;
		return res;
	}

	/// \internal Convert any object to its string representation based on \a QTextStream.
	template<class T>
	QByteArray typeToByteArray(const T& value)
	{
		QByteArray res;
		QTextStream stream(&res, QIODevice::WriteOnly);
		stream << value;
		stream.flush();
		return res;
	}

	/// \internal Convert a string to any object type based on \a QTextStream.
	template<class T>
	T byteArrayToType(const QByteArray& str)
	{
		T res;
		QTextStream stream(const_cast<QByteArray*>(&str), QIODevice::ReadOnly);
		stream >> res;
		return res;
	}

	///\internal
	struct ToQStringTransform
	{
		template<class INTYPE>
		static QString apply(const INTYPE& d)
		{
			return QString::number(d);
		}
		static QString apply(long double d) { return vipLongDoubleToString(d); }
		static QString apply(double d) { return QString::number(d, 'g', FLT_DIG); }
		static QString apply(float d) { return QString::number(d, 'g', FLT_DIG); }
		static QString apply(bool d) { return QLatin1String(d ? "true" : "false"); }
		static QString apply(const QByteArray& ar) { return QString(ar); }
		static const QString& apply(const QString& ar) { return ar; }

		template<class T>
		static QString apply(const std::complex<T>& d)
		{
			return typeToString<std::complex<T>>(d);
		}
		static QString apply(const VipRGB& c) { return typeToString<VipRGB>(c); }

		template<class T>
		QString operator()(const T& src) const
		{
			return apply(src);
		}
	};

	///\internal
	struct ToQByteArrayTransform
	{
		template<class INTYPE>
		static QByteArray apply(const INTYPE& d)
		{
			return QByteArray::number(d);
		}
		static QByteArray apply(const long int& d) { return QByteArray::number((qint64)d); }
		static QByteArray apply(const long unsigned int& d) { return QByteArray::number((quint64)d); }
		static QByteArray apply(long double d) { return vipLongDoubleToByteArray(d); }
		static QByteArray apply(double d) { return QByteArray::number(d, 'g', FLT_DIG); }
		static QByteArray apply(float d) { return QByteArray::number(d, 'g', FLT_DIG); }
		static QByteArray apply(bool d) { return QByteArray(d ? "true" : "false"); }
		static QByteArray apply(const QString& str) { return str.toLatin1(); }
		const QByteArray& apply(const QByteArray& str) { return str; }
		template<class T>
		static QByteArray apply(const std::complex<T>& d)
		{
			return typeToByteArray<std::complex<T>>(d);
		}
		static QByteArray apply(const VipRGB& c) { return typeToByteArray<VipRGB>(c); }

		template<class T>
		QByteArray operator()(const T& src) const
		{
			return apply(src);
		}
	};

	///\internal
	template<class Res>
	struct ToNumericTransform
	{
		template<class T>
		static Res apply(const T& d)
		{
			return static_cast<Res>(d);
		}
		static Res apply(const QString& d)
		{
			if (std::is_same<Res, float>::value)
				return d.toFloat();
			else if (std::is_same<Res, double>::value)
				return d.toDouble();
			else if (std::is_same<Res, long double>::value)
				return vipLongDoubleFromString(d);
			else if (std::is_same<Res, bool>::value) {
				QString str = d.toLower();
				return !(str == QString("0") || str == QString("false") || str.isEmpty());
			}
			return d.toLongLong();
		}
		static Res apply(const QByteArray& d)
		{
			if (std::is_same<Res, float>::value)
				return d.toFloat();
			else if (std::is_same<Res, double>::value)
				return d.toDouble();
			else if (std::is_same<Res, long double>::value)
				return vipLongDoubleFromByteArray(d);
			else if (std::is_same<Res, bool>::value) {
				QString str = d.toLower();
				return !(str == QString("0") || str == QString("false") || str.isEmpty());
			}
			return d.toLongLong();
		}
		template<class T>
		Res operator()(const T& src) const
		{
			return apply(src);
		}
	};

	///\internal
	template<>
	struct ToNumericTransform<complex_f>
	{
		template<class T>
		static complex_f apply(const T& d)
		{
			return static_cast<complex_f>(d);
		}
		static complex_f apply(const QString& d) { return stringToType<complex_f>(d); }
		static complex_f apply(const QByteArray& d) { return byteArrayToType<complex_f>(d); }
		template<class T>
		complex_f operator()(const T& src) const
		{
			return apply(src);
		}
	};

	///\internal
	template<>
	struct ToNumericTransform<complex_d>
	{
		template<class T>
		static complex_d apply(const T& d)
		{
			return static_cast<complex_d>(d);
		}
		static complex_d apply(const QString& d) { return stringToType<complex_d>(d); }
		static complex_d apply(const QByteArray& d) { return byteArrayToType<complex_d>(d); }
		template<class T>
		complex_d operator()(const T& src) const
		{
			return apply(src);
		}
	};

	///\internal
	struct ToRGB
	{
		static VipRGB apply(const QString& d) { return stringToType<VipRGB>(d); }
		static VipRGB apply(const QByteArray& d) { return byteArrayToType<VipRGB>(d); }
		static VipRGB apply(QRgb value) { return VipRGB(value); }
		template<class T>
		static VipRGB apply(const T&)
		{
			return VipRGB();
		}
		template<class T>
		VipRGB operator()(const T& src) const
		{
			return apply(src);
		}
	};

	/// \internal Conversion structure used by the whole DataType library through #vipCast
	template<class D, class S, class = void>
	struct Convert
	{
		static const bool valid = true;
		static D apply(const S& src) { return static_cast<D>(src); }
	};

	// conversion to complex
	template<class T, class S>
	struct Convert<std::complex<T>, S>
	{
		static const bool valid = true;
		static std::complex<T> apply(const S& src) { return ToNumericTransform<std::complex<T>>::apply(src); }
	};
	template<class T, class U>
	struct Convert<std::complex<T>, std::complex<U>>
	{
		static const bool valid = true;
		static std::complex<T> apply(const std::complex<U>& src) { return src; }
	};
	// complex to something: disable
	template<class D, class T>
	struct Convert<D, std::complex<T>>
	{
		static const bool valid = false;
		static D apply(const std::complex<T>&) { return D(); }
	};
	// complex to string
	template<class T>
	struct Convert<QString, std::complex<T>>
	{
		static const bool valid = true;
		static QString apply(const std::complex<T>& src) { return ToQStringTransform::apply(src); }
	};
	// complex to bytearray
	template<class T>
	struct Convert<QByteArray, std::complex<T>>
	{
		static const bool valid = true;
		static QByteArray apply(const std::complex<T>& src) { return ToQByteArrayTransform::apply(src); }
	};

	// disable for VipRGB
	template<class S>
	struct Convert<VipRGB, S, typename std::enable_if<!is_complex<S>::value && !is_rgb<S>::value, void>::type> // disable conversion to complex as it is already covered
	{
		static const bool valid = false;
		static VipRGB apply(const S&) { return VipRGB(); }
	};
	template<class T, class U>
	struct Convert<VipRgb<T>, VipRgb<U>>
	{
		static const bool valid = true;
		static VipRgb<T> apply(const VipRgb<U>& v) { return v; }
	};
	// enable QString to VipRGB
	template<>
	struct Convert<VipRGB, QString>
	{
		static const bool valid = true;
		static VipRGB apply(const QString& s) { return ToRGB::apply(s); }
	};
	// enable QByteArray to VipRGB
	template<>
	struct Convert<VipRGB, QByteArray>
	{
		static const bool valid = true;
		static const VipRGB apply(const QByteArray& s) { return ToRGB::apply(s); }
	};
	// enable QRgb to VipRGB
	template<>
	struct Convert<VipRGB, QRgb>
	{
		static const bool valid = true;
		static const VipRGB apply(const QRgb& s) { return ToRGB::apply(s); }
	};
	// convertion to string
	template<class S>
	struct Convert<QString, S>
	{
		static const bool valid = true;
		static QString apply(const S& s) { return ToQStringTransform::apply(s); }
	};
	// convertion to bytearray
	template<class S>
	struct Convert<QByteArray, S>
	{
		static const bool valid = true;
		static QString apply(const S& s) { return ToQByteArrayTransform::apply(s); }
	};

	template<class D>
	struct Convert<D, NullType>
	{
		static const bool valid = true;
		static D apply(const NullType&) { return D(); }
	};
	template<class S>
	struct Convert<NullType, S>
	{
		static const bool valid = true;
		static NullType apply(const S&) { return NullType(); }
	};
	template<>
	struct Convert<NullType, NullType>
	{
		static const bool valid = true;
		static NullType apply(const NullType&) { return NullType(); }
	};

	template<class In, class Out>
	struct Converter
	{
		static Out apply(const In& v) { return v; }
	};
	template<>
	struct Converter<complex_f, complex_d>
	{
		static complex_d apply(const complex_f& v) { return complex_d(v.real(), v.imag()); }
	};
	template<>
	struct Converter<complex_d, complex_f>
	{
		static complex_f apply(const complex_d& v) { return complex_f(v.real(), v.imag()); }
	};
	template<class Out>
	struct Converter<complex_d, Out>
	{
		static Out apply(const complex_d& v) { return v.real(); }
	};
	template<class Out>
	struct Converter<complex_f, Out>
	{
		static Out apply(const complex_f& v) { return v.real(); }
	};

	///\internal
	template<class From, class To>
	void convertVoid(const void* ptr, void* dst, int, int size)
	{
		const From* in = (const From*)ptr;
		To* out = (To*)(dst);
		for (int i = 0; i < size; ++i)
			out[i] = Converter<From, To>::apply(in[i]);
	}

	///\internal
	template<class To>
	void genericConverterVoid(const void* ptr, void* dst, int data_type, int size)
	{
		int out_type = qMetaTypeId<To>();
		int in_size = QMetaType(data_type).sizeOf();
		for (int i = 0; i < size; ++i) {
			QMetaType::convert(ptr, data_type, dst, out_type);
			ptr = (uchar*)(ptr) + in_size;
			dst = (uchar*)(dst) + sizeof(To);
		}
	}

	///\internal
	template<class To>
	struct GetConverterFunction
	{
		typedef void (*cast)(const void*, void*, int, int);
		static cast get(int data_type)
		{
			if (data_type == QMetaType::Char)
				return convertVoid<char, To>;
			else if (data_type == QMetaType::SChar)
				return convertVoid<qint8, To>;
			else if (data_type == QMetaType::UChar)
				return convertVoid<quint8, To>;
			else if (data_type == QMetaType::Short)
				return convertVoid<qint16, To>;
			else if (data_type == QMetaType::UShort)
				return convertVoid<quint16, To>;
			else if (data_type == QMetaType::Int)
				return convertVoid<qint32, To>;
			else if (data_type == QMetaType::UInt)
				return convertVoid<quint32, To>;
			else if (data_type == QMetaType::Long)
				return convertVoid<long, To>;
			else if (data_type == QMetaType::ULong)
				return convertVoid<unsigned long, To>;
			else if (data_type == QMetaType::LongLong)
				return convertVoid<qint64, To>;
			else if (data_type == QMetaType::ULongLong)
				return convertVoid<quint64, To>;
			else if (data_type == QMetaType::Float)
				return convertVoid<float, To>;
			else if (data_type == QMetaType::Double)
				return convertVoid<double, To>;
			else if (data_type == qMetaTypeId<long double>())
				return convertVoid<long double, To>;
			else if (data_type == qMetaTypeId<complex_f>())
				return convertVoid<complex_f, To>;
			else if (data_type == qMetaTypeId<complex_d>())
				return convertVoid<complex_d, To>;
			else
				return genericConverterVoid<To>;
		}
	};

	/// \internal
	/// Convert input possibly strided data to possibly strided array.
	/// Returns false if the conversion is not possible.
	VIP_DATA_TYPE_EXPORT bool convert(const void* i_data,
					  uint i_type,
					  const VipNDArrayShape& i_shape,
					  const VipNDArrayShape& i_strides,
					  void* o_data,
					  uint o_type,
					  const VipNDArrayShape& o_shape,
					  const VipNDArrayShape& o_strides);

}

Q_DECLARE_METATYPE(detail::NullType)

#endif