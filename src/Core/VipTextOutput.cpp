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

#include "VipTextOutput.h"

VipStreambufToQTextStream::VipStreambufToQTextStream(QTextStream* stream)
  : std::streambuf()
  , m_stream(stream)
  , m_enable(true)
{
}

void VipStreambufToQTextStream::setEnabled(bool enable)
{
	QMutexLocker loxk(&m_mutex);
	m_enable = enable;
}

std::streamsize VipStreambufToQTextStream::xsputn(const char* s, std::streamsize n)
{
	if (m_enable) {
		QMutexLocker loxk(&m_mutex);
		(*m_stream) << QString(std::string(s, n).c_str());
		m_stream->flush();
		return n;
	}
	else
		return n;
}

VipStreambufToQTextStream::int_type VipStreambufToQTextStream::overflow(int_type c)
{
	if (m_enable) {
		QMutexLocker loxk(&m_mutex);
		(*m_stream) << QString(1, QChar(c));
		m_stream->flush();
		return c;
	}
	else
		return c;
}

VipIODeviceToStreambuf::VipIODeviceToStreambuf(std::ostream* buf)
  : QIODevice()
  , m_stream(buf)
  , m_enable(true)
{
	this->setOpenMode(ReadWrite);
	this->open(ReadWrite);
}

bool VipIODeviceToStreambuf::atEnd() const
{
	return true;
}

void VipIODeviceToStreambuf::close() {}

bool VipIODeviceToStreambuf::isSequential() const
{
	return false;
}

bool VipIODeviceToStreambuf::open(OpenMode)
{
	return true;
}

qint64 VipIODeviceToStreambuf::pos() const
{
	return 0;
}

bool VipIODeviceToStreambuf::reset()
{
	return true;
}

bool VipIODeviceToStreambuf::seek(qint64)
{
	return true;
}

qint64 VipIODeviceToStreambuf::size() const
{
	return 0;
}

qint64 VipIODeviceToStreambuf::readData(char*, qint64)
{
	return 0;
}

qint64 VipIODeviceToStreambuf::writeData(const char* data, qint64 maxSize)
{
	if (m_enable) {
		(*m_stream) << std::string(data, maxSize);
		m_stream->flush();
		return maxSize;
	}
	else {
		return maxSize;
	}
}

void VipIODeviceToStreambuf::setEnabled(bool enable)
{
	m_enable = enable;
}

QString vipSplitClassname(const QString& str)
{
	QString res;
	bool previous_was_upper = true;
	for (int i = 0; i < str.size(); ++i) {
		// split on '_'
		if (str[i] == '_') {
			res += ' ';
			previous_was_upper = false;
			continue;
		}

		// case vipTest
		if (str[i].isUpper() && !previous_was_upper)
			res += ' ';

		res += str[i];

		// case VIPTest
		if (str[i].isLower()) {
			if (i > 1 && str[i - 1].isUpper() && str[i - 2].isUpper())
				res.insert(res.size() - 2, " ");
		}

		previous_was_upper = str[i].isUpper();
	}

	if (res.startsWith("vip ", Qt::CaseInsensitive))
		res = res.mid(4);

	// remove starting and trailing spaces
	while (res.startsWith(" "))
		res = res.mid(1);
	while (res.startsWith("\t"))
		res = res.mid(1);
	while (res.startsWith("\n"))
		res = res.mid(1);

	while (res.endsWith(" "))
		res = res.mid(0, res.length() - 1);
	while (res.endsWith("\t"))
		res = res.mid(0, res.length() - 1);
	while (res.endsWith("\n"))
		res = res.mid(0, res.length() - 1);

	// remove namespace
	int index = res.indexOf("::");
	if (index >= 0)
		res = res.mid(index + 2);

	return res;
}
