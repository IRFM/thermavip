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

#ifndef VIP_TEXT_OUTPUT_H
#define VIP_TEXT_OUTPUT_H

#include <iosfwd>
#include <iostream>

#include <QTextStream>
#include <QIODevice>

#include "VipConfig.h"

/// \addtogroup Core
/// @{

// Qt does not define insertion/extraction operators for type bool and std::string
inline QTextStream& operator<<(QTextStream& stream, bool value)
{
	return stream << (int)value;
}

inline QTextStream& operator>>(QTextStream& stream, bool& value)
{
	int c;
	stream >> c;
	value = c;
	return stream;
}

inline QTextStream& operator<<(QTextStream& stream, const std::string& value)
{
	return stream << value.c_str();
}

inline QTextStream& operator>>(QTextStream& stream, std::string& value)
{
	QString str;
	stream >> str;
	value = str.toLatin1().data();
	;
	return stream;
}

// insertion/extraction operators for QString and stl stream
inline std::ostream& operator<<(std::ostream& stream, const QString& value)
{
	return stream << value.toLatin1().data();
}

inline std::istream& operator>>(std::istream& stream, QString& value)
{
	std::string temp;
	stream >> temp;
	value = temp.c_str();
	return stream;
}

// define a warning output stream
namespace std
{
	extern ostream cwarn;
}

/// Specific std::streambuf redirecting outputs to a QTextStream
class VIP_CORE_EXPORT VipStreambufToQTextStream : public std::streambuf
{
	QTextStream* m_stream;
	QMutex m_mutex;
	bool m_enable;

public:
	typedef std::streambuf::traits_type::int_type int_type;
	VipStreambufToQTextStream(QTextStream* stream);

	void setEnabled(bool enable);
	bool isEnabled() const { return m_enable; }

protected:
	virtual std::streamsize xsputn(const char* s, std::streamsize n);
	virtual int_type overflow(int_type c = std::streambuf::traits_type::eof());
};

/// Specific QIODevice redirecting outputs to a std::streambuf
class VIP_CORE_EXPORT VipIODeviceToStreambuf : public QIODevice
{
	std::ostream* m_stream;
	bool m_enable;

public:
	VipIODeviceToStreambuf(std::ostream* stream);

	virtual bool atEnd() const;
	virtual void close();
	virtual bool isSequential() const;
	virtual bool open(OpenMode mode);
	virtual qint64 pos() const;
	virtual bool reset();
	virtual bool seek(qint64 pos);
	virtual qint64 size() const;

	void setEnabled(bool enable);
	bool isEnabled() const { return m_enable; }

protected:
	virtual qint64 readData(char* data, qint64 maxSize);
	virtual qint64 writeData(const char* data, qint64 maxSize);
};

/// Return a more readable version of given string which represents a class name, and remove the starting 'Vip' prefix (if any).
/// For instance 'VipIODeviceToStreambuf' will become 'IO Device To Streambuf'
/// 'vipSplitClassname' will become 'Split Classname'
VIP_CORE_EXPORT QString vipSplitClassname(const QString& str);

/// @}
// end Core

#endif
