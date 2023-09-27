#include "VipTextOutput.h"


VipStreambufToQTextStream::VipStreambufToQTextStream( QTextStream * stream)
:std::streambuf(),m_stream(stream),m_enable(true)
{}

void VipStreambufToQTextStream::setEnabled(bool enable)
{
	QMutexLocker loxk(&m_mutex);
	m_enable = enable;
}

std::streamsize VipStreambufToQTextStream::xsputn ( const char * s, std::streamsize n )
{
	if(m_enable)
	{
		QMutexLocker loxk(&m_mutex);
		(*m_stream)<< QString(std::string(s,n).c_str());
		m_stream->flush();
		return n;
	}
	else
		return n;
}

VipStreambufToQTextStream::int_type VipStreambufToQTextStream::overflow ( int_type c  )
{
	if(m_enable)
	{
		QMutexLocker loxk(&m_mutex);
		(*m_stream)<< QString(1,c);
		m_stream->flush();
		return c;
	}
	else
		return c;
}




VipIODeviceToStreambuf::VipIODeviceToStreambuf(std::ostream* buf)
:QIODevice(),m_stream(buf),m_enable(true)
{
	this->setOpenMode(ReadWrite);
	this->open(ReadWrite);
}

bool 	VipIODeviceToStreambuf::atEnd () const
{
	return true;
}

void 	VipIODeviceToStreambuf::close ()
{}

bool 	VipIODeviceToStreambuf::isSequential () const
{
	return false;
}

bool 	VipIODeviceToStreambuf::open ( OpenMode  )
{
	return true;
}

qint64 	VipIODeviceToStreambuf::pos () const
{
	return 0;
}

bool 	VipIODeviceToStreambuf::reset ()
{
	return true;
}

bool 	VipIODeviceToStreambuf::seek ( qint64  )
{
	return true;
}

qint64 	VipIODeviceToStreambuf::size () const
{
	return 0;
}

qint64 	VipIODeviceToStreambuf::readData ( char * , qint64  )
{
	return 0;
}

qint64 	VipIODeviceToStreambuf::writeData ( const char * data, qint64 maxSize )
{
	if(m_enable)
	{
		(*m_stream)<< std::string(data,maxSize);
		m_stream->flush();
		return maxSize;
	}
	else
	{
		return maxSize;
	}
}

void VipIODeviceToStreambuf::setEnabled(bool enable)
{
	m_enable = enable;
}



QString vipSplitClassname(const QString & str)
{
	QString res;
	bool previous_was_upper = true;
	for (int i = 0; i < str.size(); ++i)
	{
		//split on '_'
		if (str[i] == '_')
		{
			res += ' ';
			previous_was_upper = false;
			continue;
		}

		//case vipTest
		if (str[i].isUpper() && !previous_was_upper)
			res += ' ';

		res += str[i];

		//case VIPTest
		if (str[i].isLower())
		{
			if (i > 1 && str[i - 1].isUpper() && str[i - 2].isUpper())
				res.insert(res.size() - 2, " ");
		}

		previous_was_upper = str[i].isUpper();
	}

	if (res.startsWith("vip ", Qt::CaseInsensitive))
		res = res.mid(4);

	//remove starting and trailing spaces
	while (res.startsWith(" "))
		res = res.mid(1);
	while (res.startsWith("\t"))
		res = res.mid(1);
	while (res.startsWith("\n"))
		res = res.mid(1);

	while (res.endsWith(" "))
		res = res.mid(0,res.length()-1);
	while (res.endsWith("\t"))
		res = res.mid(0, res.length() - 1);
	while (res.endsWith("\n"))
		res = res.mid(0, res.length() - 1);

	//remove namespace
	int index = res.indexOf("::");
	if (index >= 0)
		res = res.mid(index + 2);

	return res;
}
