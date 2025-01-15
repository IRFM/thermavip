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

#include "VipInternalConvert.h"
#include "VipIterator.h"

#include <QVariant>

#define CONVERT_TR(from, to, transform) vipArrayTransform(static_cast<const from*>(i_data), i_shape, i_strides, static_cast<to*>(o_data), o_shape, o_strides, transform);

namespace detail
{
	/// Simple cast functor
	template<class OUTTYPE>
	struct SimpleCastTransform
	{
		template<class INTYPE>
		OUTTYPE operator()(const INTYPE& v) const
		{
			return static_cast<OUTTYPE>(v);
		}
	};

	bool convert(const void* i_data,
		     uint i_type,
		     const VipNDArrayShape& i_shape,
		     const VipNDArrayShape& i_strides,
		     void* o_data,
		     uint o_type,
		     const VipNDArrayShape& o_shape,
		     const VipNDArrayShape& o_strides)
	{
		Q_ASSERT(i_data);
		Q_ASSERT(o_data);
		Q_ASSERT(i_type != (uint)o_type);

		switch ((o_type)) {

			case QMetaType::QString: {
				switch (i_type) {
					case QMetaType::Char:
						CONVERT_TR(char, QString, ToQStringTransform());
						return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, QString, ToQStringTransform());
						return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, QString, ToQStringTransform());
						return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, QString, ToQStringTransform());
						return true;
					case QMetaType::Long:
						CONVERT_TR(long, QString, ToQStringTransform());
						return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, QString, ToQStringTransform());
						return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, QString, ToQStringTransform());
						return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, QString, ToQStringTransform());
						return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, QString, ToQStringTransform());
						return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, QString, ToQStringTransform());
						return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, QString, ToQStringTransform());
						return true;
					case QMetaType::Float:
						CONVERT_TR(float, QString, ToQStringTransform());
						return true;
					case QMetaType::Double:
						CONVERT_TR(double, QString, ToQStringTransform());
						return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, QString, ToQStringTransform());
						return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, QString, ToQStringTransform());
						return true;
					default:
						break;
				}

				if (i_type == (uint)qMetaTypeId<VipRGB>()) {
					CONVERT_TR(VipRGB, QString, ToQStringTransform());
					return true;
				}
				if (i_type == (uint)qMetaTypeId<complex_d>()) {
					CONVERT_TR(complex_d, QString, ToQStringTransform());
					return true;
				}
				else if (i_type == (uint)qMetaTypeId<complex_f>()) {
					CONVERT_TR(complex_f, QString, ToQStringTransform());
					return true;
				}
				else if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, QString, ToQStringTransform());
					return true;
				}
				return false;
			} break;

			case QMetaType::QByteArray: {
				switch (i_type) {
					case QMetaType::Char:
						CONVERT_TR(char, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::Long:
						CONVERT_TR(long, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::Float:
						CONVERT_TR(float, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::Double:
						CONVERT_TR(double, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, QByteArray, ToQByteArrayTransform());
						return true;
					case QMetaType::QString:
						CONVERT_TR(QString, QByteArray, ToQByteArrayTransform());
						return true;
					default:
						break;
				}

				if (i_type == (uint)qMetaTypeId<VipRGB>()) {
					CONVERT_TR(VipRGB, QByteArray, ToQByteArrayTransform());
					return true;
				}
				if (i_type == (uint)qMetaTypeId<complex_d>()) {
					CONVERT_TR(complex_d, QByteArray, ToQByteArrayTransform());
					return true;
				}
				else if (i_type == (uint)qMetaTypeId<complex_f>()) {
					CONVERT_TR(complex_f, QByteArray, ToQByteArrayTransform());
					return true;
				}
				else if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, QString, ToQByteArrayTransform());
					return true;
				}

				return false;
			} break;

			case QMetaType::Char: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, char, ToNumericTransform<char>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, char, ToNumericTransform<char>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, char, ToNumericTransform<char>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, char, ToNumericTransform<char>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, char, ToNumericTransform<char>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, char, ToNumericTransform<char>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, char, ToNumericTransform<char>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, char, ToNumericTransform<char>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, char, ToNumericTransform<char>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, char, ToNumericTransform<char>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, char, ToNumericTransform<char>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, char, ToNumericTransform<char>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, char, ToNumericTransform<char>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, char, ToNumericTransform<char>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, char, ToNumericTransform<char>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, char, ToNumericTransform<char>());
					return true;
				}
				return false;
			} break;

			case QMetaType::UChar: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, quint8, ToNumericTransform<quint8>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, quint8, ToNumericTransform<quint8>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, quint8, ToNumericTransform<quint8>());
					return true;
				}
				return false;
			} break;

			case QMetaType::Short: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, qint16, ToNumericTransform<qint16>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, qint16, ToNumericTransform<qint16>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, qint16, ToNumericTransform<qint16>());
					return true;
				}
				return false;
			} break;

			case QMetaType::UShort: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, quint16, ToNumericTransform<quint16>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, quint16, ToNumericTransform<quint16>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, quint16, ToNumericTransform<quint16>());
					return true;
				}
				return false;
			} break;

			case QMetaType::Int: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, qint32, ToNumericTransform<qint32>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, qint32, ToNumericTransform<qint32>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, qint32, ToNumericTransform<qint32>());
					return true;
				}
				return false;
			} break;

			case QMetaType::UInt: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, quint32, ToNumericTransform<quint32>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, quint32, ToNumericTransform<quint32>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, quint32, ToNumericTransform<quint32>());
					return true;
				}
				return false;
			} break;

			case QMetaType::Long: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, long, ToNumericTransform<long>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, long, ToNumericTransform<long>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, long, ToNumericTransform<long>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, long, ToNumericTransform<long>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, long, ToNumericTransform<long>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, long, ToNumericTransform<long>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, long, ToNumericTransform<long>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, long, ToNumericTransform<long>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, long, ToNumericTransform<long>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, long, ToNumericTransform<long>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, long, ToNumericTransform<long>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, long, ToNumericTransform<long>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, long, ToNumericTransform<long>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, long, ToNumericTransform<long>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, long, ToNumericTransform<long>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, long, ToNumericTransform<long>());
					return true;
				}
				return false;
			} break;

			case QMetaType::ULong: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, unsigned long, ToNumericTransform<unsigned long>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, unsigned long, ToNumericTransform<unsigned long>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, unsigned long, ToNumericTransform<unsigned long>());
					return true;
				}
				return false;
			} break;

			case QMetaType::LongLong: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, qint64, ToNumericTransform<qint64>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, qint64, ToNumericTransform<qint64>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, qint64, ToNumericTransform<qint64>());
					return true;
				}
				return false;
			} break;

			case QMetaType::ULongLong: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, quint64, ToNumericTransform<quint64>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, quint64, ToNumericTransform<quint64>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, quint64, ToNumericTransform<quint64>());
					return true;
				}
				return false;
			} break;

			case QMetaType::Float: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, float, ToNumericTransform<float>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, float, ToNumericTransform<float>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, float, ToNumericTransform<float>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, float, ToNumericTransform<float>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, float, ToNumericTransform<float>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, float, ToNumericTransform<float>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, float, ToNumericTransform<float>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, float, ToNumericTransform<float>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, float, ToNumericTransform<float>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, float, ToNumericTransform<float>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, float, ToNumericTransform<float>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, float, ToNumericTransform<float>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, float, ToNumericTransform<float>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, float, ToNumericTransform<float>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, float, ToNumericTransform<float>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, float, ToNumericTransform<float>());
					return true;
				}
				return false;
			} break;

			case QMetaType::Double: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, double, ToNumericTransform<double>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, double, ToNumericTransform<double>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, double, ToNumericTransform<double>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, double, ToNumericTransform<double>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, double, ToNumericTransform<double>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, double, ToNumericTransform<double>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, double, ToNumericTransform<double>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, double, ToNumericTransform<double>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, double, ToNumericTransform<double>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, double, ToNumericTransform<double>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, double, ToNumericTransform<double>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, double, ToNumericTransform<double>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, double, ToNumericTransform<double>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, double, ToNumericTransform<double>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, double, ToNumericTransform<double>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, double, ToNumericTransform<double>());
					return true;
				}
				return false;
			} break;

			case QMetaType::Bool: {
				switch (i_type) {
					case QMetaType::QString:
						CONVERT_TR(QString, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::Double:
						CONVERT_TR(double, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, bool, ToNumericTransform<bool>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, bool, ToNumericTransform<bool>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, bool, ToNumericTransform<bool>());
					return true;
				}
				return false;
			} break;

			default:
				break;
		}

		if (o_type == (uint)qMetaTypeId<VipRGB>()) {
			switch (i_type) {
				case QMetaType::QByteArray:
					CONVERT_TR(QByteArray, VipRGB, ToRGB()) return true;
				case QMetaType::QString:
					CONVERT_TR(QString, VipRGB, ToRGB()) return true;
				default:
					return false;
			}
		}
		else if (o_type == (uint)qMetaTypeId<complex_f>()) {
			if (i_type == (uint)qMetaTypeId<complex_d>()) {
				CONVERT_TR(complex_d, complex_f, SimpleCastTransform<complex_f>()) return true;
			}
			else {
				switch (i_type) {
					case QMetaType::Double:
						CONVERT_TR(double, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, complex_f, SimpleCastTransform<complex_f>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, complex_f, ToNumericTransform<complex_f>()) return true;
					case QMetaType::QString:
						CONVERT_TR(QString, complex_f, ToNumericTransform<complex_f>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, complex_f, ToNumericTransform<complex_f>());
					return true;
				}
				return false;
			}
		}
		else if (o_type == (uint)qMetaTypeId<complex_d>()) {
			if (i_type == (uint)qMetaTypeId<complex_f>()) {
				CONVERT_TR(complex_f, complex_d, SimpleCastTransform<complex_d>()) return true;
			}
			else {
				switch (i_type) {
					case QMetaType::Double:
						CONVERT_TR(double, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::Float:
						CONVERT_TR(float, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::Char:
						CONVERT_TR(char, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::SChar:
						CONVERT_TR(qint8, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::UChar:
						CONVERT_TR(quint8, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::Int:
						CONVERT_TR(qint32, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::LongLong:
						CONVERT_TR(qint64, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::Short:
						CONVERT_TR(qint16, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::Long:
						CONVERT_TR(long, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::UInt:
						CONVERT_TR(quint32, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::ULongLong:
						CONVERT_TR(quint64, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::UShort:
						CONVERT_TR(quint16, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::ULong:
						CONVERT_TR(unsigned long, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::Bool:
						CONVERT_TR(bool, complex_d, SimpleCastTransform<complex_d>()) return true;
					case QMetaType::QByteArray:
						CONVERT_TR(QByteArray, complex_d, ToNumericTransform<complex_d>()) return true;
					case QMetaType::QString:
						CONVERT_TR(QString, complex_d, ToNumericTransform<complex_d>()) return true;
					default:
						break;
				}
				if (i_type == (uint)qMetaTypeId<long double>()) {
					CONVERT_TR(long double, complex_d, ToNumericTransform<complex_d>());
					return true;
				}
				return false;
			}
		}
		else if (o_type == (uint)qMetaTypeId<long double>()) {
			switch (i_type) {
				case QMetaType::Double:
					CONVERT_TR(double, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::Float:
					CONVERT_TR(float, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::Char:
					CONVERT_TR(char, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::SChar:
					CONVERT_TR(qint8, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::UChar:
					CONVERT_TR(quint8, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::Int:
					CONVERT_TR(qint32, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::LongLong:
					CONVERT_TR(qint64, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::Short:
					CONVERT_TR(qint16, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::Long:
					CONVERT_TR(long, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::UInt:
					CONVERT_TR(quint32, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::ULongLong:
					CONVERT_TR(quint64, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::UShort:
					CONVERT_TR(quint16, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::ULong:
					CONVERT_TR(unsigned long, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::Bool:
					CONVERT_TR(bool, long double, SimpleCastTransform<long double>()) return true;
				case QMetaType::QByteArray:
					CONVERT_TR(QByteArray, long double, ToNumericTransform<long double>()) return true;
				case QMetaType::QString:
					CONVERT_TR(QString, long double, ToNumericTransform<long double>()) return true;
				default:
					return false;
			}
		}

		return false;
	}

}
