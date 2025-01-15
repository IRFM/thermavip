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

#include "VipLongDouble.h"
#include <QLocale>
#include <complex>

static std::locale toStdLocale(const QLocale& l)
{
	QByteArray name = l.name().toLatin1();
	try {
		std::locale res(name.data());
		return res;
	}
	catch (...) {
		try {
			// get first '_', replace by '-'
			int index = name.indexOf('_');
			if (index >= 0)
				name[index] = '-';
			std::locale res(name.data());
			return res;
		}
		catch (...) {
			return std::locale();
		}
	}
	VIP_UNREACHABLE();
	// return std::locale();
}

QString vipLongDoubleToString(const vip_long_double v)
{
	std::ostringstream ss;
	ss << std::setprecision(std::numeric_limits<vip_long_double>::digits10 + 1) << v;
	return QString(ss.str().c_str());
}
QByteArray vipLongDoubleToByteArray(const vip_long_double v)
{
	std::ostringstream ss;
	ss << std::setprecision(std::numeric_limits<vip_long_double>::digits10 + 1) << v;
	return QByteArray(ss.str().c_str());
}
vip_long_double vipLongDoubleFromString(const QString& str, bool* ok)
{
	std::istringstream ss(str.toLatin1().data());
	vip_long_double res;
	ss >> res;
	if (ok)
		*ok = !ss.fail();
	return res;
}
vip_long_double vipLongDoubleFromByteArray(const QByteArray& str, bool* ok)
{
	std::istringstream ss(str.data());
	vip_long_double res;
	ss >> res;
	if (ok)
		*ok = !ss.fail();
	return res;
}

static QLocale _locale = QLocale::system();

QString vipLongDoubleToStringLocale(const vip_long_double v, const QLocale& l)
{
	std::ostringstream ss;
	if (l.language() != //_locale.language()
	    QLocale::C)
		ss.imbue(toStdLocale(l));
	ss << std::setprecision(std::numeric_limits<vip_long_double>::digits10 + 1) << v;
	return QString(ss.str().c_str());
}
QByteArray vipLongDoubleToByteArrayLocale(const vip_long_double v, const QLocale& l)
{
	std::ostringstream ss;
	if (l.language() != //_locale.language()
	    QLocale::C)
		ss.imbue(toStdLocale(l));
	ss << std::setprecision(std::numeric_limits<vip_long_double>::digits10 + 1) << v;
	return QByteArray(ss.str().c_str());
}

// #include <Windows.h>
//
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <ostream>
// #include <iterator>
//
// using namespace std;
//
// vector<wstring> locals;
//
// BOOL CALLBACK MyFuncLocaleEx(LPWSTR pStr, DWORD dwFlags, LPARAM lparam)
// {
// locals.push_back(pStr);
// return TRUE;
// }
//
// int printlocales()
// {
// EnumSystemLocalesEx(MyFuncLocaleEx, LOCALE_ALL, nullptr, nullptr);
//
// for (vector<wstring>::const_iterator str = locals.begin(); str != locals.end(); ++str)
// wcout << *str << endl;
//
// wcout << "Total " << locals.size() << " locals found." << endl;
// wcout.flush();
// return 0;
// }

vip_long_double vipLongDoubleFromStringLocale(const QString& str, const QLocale& l, bool* ok)
{
	// printlocales();
	// vip_debug("locale '%s'\n", l.name().toLatin1().data());
	std::istringstream ss(str.toLatin1().data());
	if (l.language() != //_locale.language()
	    QLocale::C)
		ss.imbue(toStdLocale(l)); // std::locale(l.name().toLatin1().data()));
	vip_long_double res;
	ss >> res;
	if (ok)
		*ok = !ss.fail();
	return res;
}
vip_long_double vipLongDoubleFromByteArrayLocale(const QByteArray& str, const QLocale& l, bool* ok)
{
	std::istringstream ss(str.data());
	if (l.language() != QLocale::C) //_locale.language())
		ss.imbue(toStdLocale(l));
	vip_long_double res;
	ss >> res;
	if (ok)
		*ok = !ss.fail();
	return res;
}
QTextStream& operator<<(QTextStream& s, vip_long_double v)
{
	std::ostringstream ss;
	if (s.locale().language() != QLocale::C)
		ss.imbue(toStdLocale(s.locale()));
	ss << std::setprecision(std::numeric_limits<vip_long_double>::digits10 + 1) << v;
	return s << ss.str().c_str();
}
QTextStream& operator>>(QTextStream& s, vip_long_double& v)
{
	QString word;
	s >> word;
	v = vipLongDoubleFromStringLocale(word, s.locale());
	return s;
}

void vipWriteNLongDouble(QTextStream& s, const vip_long_double* values, int count, int step, const char* sep)
{
	std::ostringstream ss;
	if (s.locale().language() != QLocale::C)
		ss.imbue(toStdLocale(s.locale()));
	ss << std::setprecision(std::numeric_limits<vip_long_double>::digits10 + 1);
	for (int i = 0; i < count; i += step) {
		ss << values[i] << sep;
	}
	s << ss.str().c_str();
}
int vipReadNLongDouble(QTextStream& s, vip_long_double* values, int max_count)
{
	// qint64 pos = s.pos();
	std::string str = s.readAll().toLatin1().data();
	std::istringstream ss(str);
	if (s.locale().language() != //_locale.language()
	    QLocale::C)
		ss.imbue(toStdLocale(s.locale()));
	int i = 0;
	for (; i < max_count; ++i)
		if (!(ss >> values[i]))
			break;

	s.seek(ss.tellg());
	return i;
}
int vipReadNLongDouble(QTextStream& s, QVector<vip_long_double>& values, int max_count)
{
	// qint64 pos = s.pos();
	std::string str = s.readAll().toLatin1().data();
	std::istringstream ss(str);
	if (s.locale().language() != //_locale.language()
	    QLocale::C)
		ss.imbue(toStdLocale(s.locale()));
	int i = 0;
	for (; i < max_count; ++i) {
		vip_long_double v;
		if (!(ss >> v))
			break;
		values.append(v);
	}

	s.seek(ss.tellg());
	return i;
}

template<class T>
static vip_long_double toLongDouble(T v)
{
	return (vip_long_double)v;
}
template<class T>
static T fromLongDouble(vip_long_double v)
{
	return (T)v;
}

static vip_long_double LongDoubleFromString(const QString& str)
{
	return vipLongDoubleFromString(str, nullptr);
}
static vip_long_double LongDoubleFromByteArray(const QByteArray& str)
{
	return vipLongDoubleFromByteArray(str, nullptr);
}

static int registerLongDouble()
{
	qRegisterMetaType<VipPoint>();
#if VIP_USE_LONG_DOUBLE == 0
	qRegisterMetaType<VipLongPoint>();
#endif
	qRegisterMetaType<vip_long_double>();

	#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaTypeStreamOperators<vip_long_double>("vip_long_double");
	#endif

	QMetaType::registerConverter<vip_long_double, signed char>(fromLongDouble<signed char>);
	QMetaType::registerConverter<vip_long_double, char>(fromLongDouble<char>);
	QMetaType::registerConverter<vip_long_double, quint8>(fromLongDouble<quint8>);
	QMetaType::registerConverter<vip_long_double, qint16>(fromLongDouble<qint16>);
	QMetaType::registerConverter<vip_long_double, quint16>(fromLongDouble<quint16>);
	QMetaType::registerConverter<vip_long_double, qint32>(fromLongDouble<qint32>);
	QMetaType::registerConverter<vip_long_double, quint32>(fromLongDouble<quint32>);
	QMetaType::registerConverter<vip_long_double, qint64>(fromLongDouble<qint64>);
	QMetaType::registerConverter<vip_long_double, quint64>(fromLongDouble<quint64>);
	QMetaType::registerConverter<vip_long_double, float>(fromLongDouble<float>);
	QMetaType::registerConverter<vip_long_double, double>(fromLongDouble<double>);
	QMetaType::registerConverter<vip_long_double, QString>(vipLongDoubleToString);
	QMetaType::registerConverter<vip_long_double, QByteArray>(vipLongDoubleToByteArray);

	QMetaType::registerConverter<signed char, vip_long_double>(toLongDouble<signed char>);
	QMetaType::registerConverter<char, vip_long_double>(toLongDouble<char>);
	QMetaType::registerConverter<quint8, vip_long_double>(toLongDouble<quint8>);
	QMetaType::registerConverter<qint16, vip_long_double>(toLongDouble<qint16>);
	QMetaType::registerConverter<quint16, vip_long_double>(toLongDouble<quint16>);
	QMetaType::registerConverter<qint32, vip_long_double>(toLongDouble<qint32>);
	QMetaType::registerConverter<quint32, vip_long_double>(toLongDouble<quint32>);
	QMetaType::registerConverter<qint64, vip_long_double>(toLongDouble<qint64>);
	QMetaType::registerConverter<quint64, vip_long_double>(toLongDouble<quint64>);
	QMetaType::registerConverter<float, vip_long_double>(toLongDouble<float>);
	QMetaType::registerConverter<double, vip_long_double>(toLongDouble<double>);
	QMetaType::registerConverter<QString, vip_long_double>(LongDoubleFromString);
	QMetaType::registerConverter<QByteArray, vip_long_double>(LongDoubleFromByteArray);
	return 0;
}
static int _registerLongDouble = registerLongDouble();
