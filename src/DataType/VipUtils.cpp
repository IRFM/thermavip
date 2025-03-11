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

#include "VipUtils.h"
#include "VipNDArray.h"
#include "VipSceneModel.h"

#include <QDataStream>
#include <QTextStream>

// helper macro to read data from a QTextSTream
#define READ_CHAR(str, ch)                                                                                                                                                                             \
	{                                                                                                                                                                                              \
		QChar tmp;                                                                                                                                                                             \
		while ((str >> tmp).status() == QTextStream::Ok && tmp != ch && (tmp == ' ' || tmp == '\t')) {                                                                                         \
		}                                                                                                                                                                                      \
		if (tmp != ch) {                                                                                                                                                                       \
			str.setStatus(QTextStream::ReadCorruptData);                                                                                                                                   \
			return str;                                                                                                                                                                    \
		}                                                                                                                                                                                      \
		if (str.status() != QTextStream::Ok)                                                                                                                                                   \
			return str;                                                                                                                                                                    \
	}

#define READ_VALUE(str, value)                                                                                                                                                                         \
	if ((str >> value).status() != QTextStream::Ok)                                                                                                                                                \
		return str;

template<class T>
QDataStream& write(QDataStream& str, const std::complex<T>& c)
{
	return str << c.real() << c.imag();
}

template<class T>
QDataStream& read(QDataStream& str, std::complex<T>& c)
{
	double r, i;
	str >> r >> i;
	c = std::complex<T>(r, i);
	return str;
}

template<class T>
QTextStream& write(QTextStream& str, const std::complex<T>& c)
{
	return str << "( " << c.real() << " + " << c.imag() << "j ) ";
}

template<class T>
QTextStream& read(QTextStream& str, std::complex<T>& c)
{
	double r, i;
	READ_CHAR(str, '(');
	READ_VALUE(str, r);
	READ_CHAR(str, '+');
	READ_VALUE(str, i);
	READ_CHAR(str, 'j');
	READ_CHAR(str, ')');

	c = std::complex<T>(r, i);
	return str;
}

QDataStream& operator<<(QDataStream& stream, const complex_f& p)
{
	return write(stream, p);
}
QDataStream& operator>>(QDataStream& stream, complex_f& p)
{
	return read(stream, p);
}

QDataStream& operator<<(QDataStream& stream, const complex_d& p)
{
	return write(stream, p);
}
QDataStream& operator>>(QDataStream& stream, complex_d& p)
{
	return read(stream, p);
}

QDataStream& operator<<(QDataStream& s, const VipComplexPoint& c)
{
	return s << c.x() << c.y();
}
QDataStream& operator>>(QDataStream& s, VipComplexPoint& c)
{
	unsigned LD_support = s.device()->property("_vip_LD").toUInt();
	c.rx() = vipReadLEDouble(LD_support, s);
	return s >> c.ry();
}

QDataStream& operator<<(QDataStream& stream, const VipInterval& p)
{
	return stream << p.minValue() << p.maxValue();
}
QDataStream& operator>>(QDataStream& stream, VipInterval& p)
{
	vip_double min, max;
	unsigned LD_support = stream.device()->property("_vip_LD").toUInt();
	min = vipReadLEDouble(LD_support, stream);
	max = vipReadLEDouble(LD_support, stream);
	p = VipInterval(min, max);
	return stream;
}

QDataStream& operator<<(QDataStream& str, const VipRGB& v)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	str << (QRgb)v;
#else
	str << qbswap((QRgb)v);
#endif
	return str;
}
QDataStream& operator>>(QDataStream& str, VipRGB& v)
{
	str >> (QRgb&)v;
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
	v = qbswap((QRgb&)v);
#endif
	return str;
}

QDataStream& operator<<(QDataStream& o, const VipLongPoint& pt)
{
	return o << pt.x() << pt.y();
}
QDataStream& operator>>(QDataStream& i, VipLongPoint& pt)
{
	unsigned LD_support = i.device()->property("_vip_LD").toUInt();
	pt.rx() = vipReadLELongDouble(LD_support, i);
	pt.ry() = vipReadLELongDouble(LD_support, i);
	return i;
}

#if VIP_USE_LONG_DOUBLE == 0
QDataStream& operator<<(QDataStream& s, const VipPoint& c)
{
	return s << c.x() << c.y();
}
QDataStream& operator>>(QDataStream& s, VipPoint& c)
{
	return s >> c.rx() >> c.ry();
}
#endif

QDataStream& operator<<(QDataStream& stream, const VipIntervalSample& p)
{
	return stream << p.interval << p.value;
}
QDataStream& operator>>(QDataStream& stream, VipIntervalSample& p)
{
	unsigned LD_support = stream.device()->property("_vip_LD").toUInt();
	p.interval.setMinValue(vipReadLEDouble(LD_support, stream));
	p.interval.setMaxValue(vipReadLEDouble(LD_support, stream));
	p.value = vipReadLEDouble(LD_support, stream);
	return stream;
}

QTextStream& operator<<(QTextStream& stream, const complex_f& p)
{
	return write(stream, p);
}
QTextStream& operator>>(QTextStream& stream, complex_f& p)
{
	return read(stream, p);
}

QTextStream& operator<<(QTextStream& stream, const complex_d& p)
{
	return write(stream, p);
}
QTextStream& operator>>(QTextStream& stream, complex_d& p)
{
	return read(stream, p);
}

QTextStream& operator<<(QTextStream& stream, const QColor& p)
{
	return stream << "[ " << (int)p.alpha() << " , " << (int)p.red() << " , " << (int)p.green() << " , " << (int)p.blue() << " ] ";
}

QTextStream& operator>>(QTextStream& str, QColor& p)
{
	int a, r, g, b;
	READ_CHAR(str, '[');
	READ_VALUE(str, a);
	READ_CHAR(str, ',');
	READ_VALUE(str, r);
	READ_CHAR(str, ',');
	READ_VALUE(str, g);
	READ_CHAR(str, ',');
	READ_VALUE(str, b);
	READ_CHAR(str, ']');
	p = QColor(r, g, b, a);
	return str;
}

QTextStream& operator<<(QTextStream& stream, const VipRGB& p)
{
	return stream << "[" << (int)p.a << "," << (int)p.r << "," << (int)p.g << "," << (int)p.b << "]";
}
QTextStream& operator>>(QTextStream& str, VipRGB& p)
{
	int a, r, g, b;
	READ_CHAR(str, '[');
	READ_VALUE(str, a);
	READ_CHAR(str, ',');
	READ_VALUE(str, r);
	READ_CHAR(str, ',');
	READ_VALUE(str, g);
	READ_CHAR(str, ',');
	READ_VALUE(str, b);
	READ_CHAR(str, ']');
	p = VipRGB(r, g, b, a);
	return str;
}

QTextStream& operator<<(QTextStream& stream, const VipInterval& p)
{
	return stream << "[ " << p.minValue() << "," << p.maxValue() << " ] ";
}

QTextStream& operator>>(QTextStream& str, VipInterval& p)
{
	double min, max;
	READ_CHAR(str, '[');
	READ_VALUE(str, min);
	READ_CHAR(str, ',');
	READ_VALUE(str, max);
	READ_CHAR(str, ']');
	p = VipInterval(min, max);
	return str;
}

QTextStream& operator<<(QTextStream& stream, const VipIntervalSample& p)
{
	return stream << "[ " << p.interval.minValue() << "," << p.interval.maxValue() << "," << p.value << " ] ";
}

QTextStream& operator>>(QTextStream& str, VipIntervalSample& p)
{
	double min, max, value;
	READ_CHAR(str, '[');
	READ_VALUE(str, min);
	READ_CHAR(str, ',');
	READ_VALUE(str, max);
	READ_CHAR(str, ',');
	READ_VALUE(str, value);
	READ_CHAR(str, ']');
	p = VipIntervalSample(min, max, value);
	return str;
}

QTextStream& operator<<(QTextStream& stream, const VipIntervalSampleVector& p)
{
	stream.setRealNumberPrecision(17);
	for (qsizetype i = 0; i < p.size(); ++i)
		stream << p[i];
	return stream;
}

QTextStream& operator>>(QTextStream& stream, VipIntervalSampleVector& p)
{
	while (true) {
		qsizetype pos = stream.pos();
		VipIntervalSample sample;
		stream >> sample;
		if (stream.status() == QTextStream::Ok)
			p.append(sample);
		else {
			stream.resetStatus();
			stream.seek(pos);
			break;
		}
	}
	return stream;
}

QTextStream& operator<<(QTextStream& s, const QPointF& c)
{
	return s << "[" << c.x() << " , " << c.y() << "] ";
}

QTextStream& operator>>(QTextStream& str, QPointF& c)
{
	READ_CHAR(str, '[');
	READ_VALUE(str, c.rx());
	READ_CHAR(str, ',');
	READ_VALUE(str, c.ry());
	READ_CHAR(str, ']');
	return str;
}

QTextStream& operator<<(QTextStream& s, const VipLongPoint& c)
{
	return s << "[" << c.x() << " , " << c.y() << "] ";
}
QTextStream& operator>>(QTextStream& s, VipLongPoint& c)
{
	READ_CHAR(s, '[');
	READ_VALUE(s, c.rx());
	READ_CHAR(s, ',');
	READ_VALUE(s, c.ry());
	READ_CHAR(s, ']');
	return s;
}

#if VIP_USE_LONG_DOUBLE == 0
QTextStream& operator<<(QTextStream& s, const VipPoint& c)
{
	return s << "[" << c.x() << " , " << c.y() << "] ";
}
QTextStream& operator>>(QTextStream& s, VipPoint& c)
{
	READ_CHAR(s, '[');
	READ_VALUE(s, c.rx());
	READ_CHAR(s, ',');
	READ_VALUE(s, c.ry());
	READ_CHAR(s, ']');
	return s;
}
#endif

QTextStream& operator<<(QTextStream& s, const VipComplexPoint& c)
{
	return s << "[" << c.x() << " , " << c.y() << "] ";
}
QTextStream& operator>>(QTextStream& str, VipComplexPoint& c)
{
	READ_CHAR(str, '[');
	READ_VALUE(str, c.rx());
	READ_CHAR(str, ',');
	str >> c.ry();
	READ_CHAR(str, ']');
	return str;
}

QTextStream& operator<<(QTextStream& stream, const VipPointVector& p)
{
	stream.setRealNumberPrecision(17);
	// first write the x in a line, then the y

	if (sizeof(vip_double) != sizeof(double)) {

		// this is faster than writting each value through QTextStream
		vipWriteNLongDouble(stream, p, "\t", [](const auto& v) { return v.x(); });
		stream << "\n";
		vipWriteNLongDouble(stream, p, "\t", [](const auto& v) { return v.y(); });
		stream << "\n";
	}
	else {
		for (qsizetype i = 0; i < p.size(); ++i)
			stream << (double)p[i].x() << "\t";
		stream << "\n";
		for (qsizetype i = 0; i < p.size(); ++i)
			stream << (double)p[i].y() << "\t";
		stream << "\n";
	}
	return stream;
}

QTextStream& operator>>(QTextStream& stream, VipPointVector& p)
{
	QString line1, line2;
	line1 = stream.readLine();
	line2 = stream.readLine();

	if (stream.status() != QTextStream::Ok)
		return stream;

	QTextStream sline1(&line1, QIODevice::ReadOnly);
	QTextStream sline2(&line2, QIODevice::ReadOnly);

	if (sizeof(vip_double) != sizeof(double)) {
		QVector<vip_long_double> values1, values2;
		vipReadNLongDoubleAppend(sline1, values1, INT_MAX);
		vipReadNLongDoubleAppend(sline2, values2, INT_MAX);
		if (values1.size() != values2.size()) {
			stream.setStatus(QTextStream::ReadCorruptData);
			return stream;
		}
		p.resize(values1.size());
		for (qsizetype i = 0; i < p.size(); ++i)
			p[i] = VipPoint(values1[i], values2[i]);
	}
	else {
		while (true) {
			double x, y;
			sline1 >> x;
			sline2 >> y;
			if (sline1.status() != QTextStream::Ok || sline2.status() != QTextStream::Ok)
				break;
			else
				p.append(VipPoint(x, y));
		}
	}

	return stream;
}

QTextStream& operator<<(QTextStream& stream, const VipComplexPointVector& p)
{
	stream.setRealNumberPrecision(17);
	// first write the x in a line, then the y

	for (qsizetype i = 0; i < p.size(); ++i)
		stream << p[i].x() << "\t";
	stream << "\n";
	for (qsizetype i = 0; i < p.size(); ++i)
		stream << p[i].y() << "\t";
	stream << "\n";
	return stream;
}
QTextStream& operator>>(QTextStream& stream, VipComplexPointVector& p)
{
	QString line1, line2;

	line1 = stream.readLine();
	line2 = stream.readLine();

	if (stream.status() != QTextStream::Ok)
		return stream;

	QTextStream sline1(&line1, QIODevice::ReadOnly);
	QTextStream sline2(&line2, QIODevice::ReadOnly);

	while (true) {
		VipComplexPoint pt;
		sline1 >> pt.rx();
		sline2 >> pt.ry();
		if (sline1.status() != QTextStream::Ok || sline2.status() != QTextStream::Ok)
			break;
		else
			p.append(pt);
	}

	return stream;
}

#include "VipNDArrayImage.h"

QDataStream& operator<<(QDataStream& stream, const VipNDArray& ar)
{
	stream << ar.handle()->handleType();
	stream << ar.dataType();
	stream << ar.shape();
	ar.handle()->ostream(VipNDArrayShape(ar.shapeCount(), 0), ar.shape(), stream);
	return stream;
}

QDataStream& operator>>(QDataStream& stream, VipNDArray& ar)
{
	ar.clear();

	int handle_type;
	int data_type;
	VipNDArrayShape shape;

	stream >> handle_type;
	if (handle_type >= 10000) {
		// this is the new format with a handle type
		stream >> data_type;
		stream >> shape;
	}
	else {
		// this the old format, without a handle type
		data_type = handle_type;
		handle_type = VipNDArrayHandle::Standard;
		stream >> shape;
	}

	SharedHandle h = vipCreateArrayHandle(handle_type, data_type, shape);
	if (vipIsNullArray(h.constData()))
		return stream;

	h->size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, h->strides);
	h->istream(VipNDArrayShape(shape.size(), 0), shape, stream);
	ar = VipNDArray(h);
	return stream;
}

QDataStream& operator<<(QDataStream& s, const VipPointVector& c)
{
	s << c.size();
	for (qsizetype i = 0; i < c.size(); ++i)
		s << c[i];
	return s;
}
QDataStream& operator>>(QDataStream& s, VipPointVector& c)
{
	qsizetype size;
	s >> size;
	c.resize(size);

	unsigned LD_support = s.device()->property("_vip_LD").toUInt();
	for (qsizetype i = 0; i < size; ++i) {
		c[i].rx() = vipReadLEDouble(LD_support, s);
		c[i].ry() = vipReadLEDouble(LD_support, s);
	}
	return s;
}

QDataStream& operator<<(QDataStream& s, const VipComplexPointVector& c)
{
	s << c.size();
	for (qsizetype i = 0; i < c.size(); ++i)
		s << c[i];
	return s;
}
QDataStream& operator>>(QDataStream& s, VipComplexPointVector& c)
{
	qsizetype size;
	s >> size;
	c.resize(size);

	unsigned LD_support = s.device()->property("_vip_LD").toUInt();
	for (qsizetype i = 0; i < size; ++i) {
		c[i].rx() = vipReadLEDouble(LD_support, s);
		s >> c[i].ry();
	}
	return s;
}

QDataStream& operator<<(QDataStream& s, const VipIntervalSampleVector& c)
{
	s << c.size();
	for (qsizetype i = 0; i < c.size(); ++i)
		s << c[i];
	return s;
}
QDataStream& operator>>(QDataStream& s, VipIntervalSampleVector& c)
{
	qsizetype size;
	s >> size;
	c.resize(size);

	unsigned LD_support = s.device()->property("_vip_LD").toUInt();
	for (qsizetype i = 0; i < size; ++i) {
		c[i].interval.setMinValue(vipReadLEDouble(LD_support, s));
		c[i].interval.setMaxValue(vipReadLEDouble(LD_support, s));
		c[i].value = (vipReadLEDouble(LD_support, s));
	}
	return s;
}

static bool isLineEmpty(const QString& line)
{
	if (line.isEmpty())
		return true;

	for (qsizetype i = 0; i < line.size(); ++i)
		if (line[(QString::size_type)i] != ' ' && line[(QString::size_type)i] != '\t')
			return false;

	return true;
}

QTextStream& operator>>(QTextStream& str, VipNDArray& ar)
{
	// try to read the header size
	QString line;
	while (str.status() == QTextStream::Ok) {
		line = str.readLine();

		QString start;
		QTextStream temp(&line);
		temp >> start;
		if (start.startsWith('#') || start.startsWith('/') || start.startsWith('*') || start.startsWith('C')) {
		}
		else
			break;
	}

	// try to find the data type

	int data_type = 0;
	void* vector = nullptr;
	{
		QTextStream temp(&line);
		char tmp[1000];
		void* ptr = tmp;
		if ((temp >> *static_cast<VipRGB*>(ptr)).status() == QTextStream::Ok) {
			data_type = qMetaTypeId<VipRGB>();
			vector = new QVector<VipRGB>();
		}
		else {
			temp.resetStatus();
			temp.seek(0);
			if ((temp >> *static_cast<complex_d*>(ptr)).status() == QTextStream::Ok) {
				data_type = qMetaTypeId<complex_d>();
				vector = new QVector<complex_d>();
			}
			else {
				temp.resetStatus();
				temp.seek(0);
				if ((temp >> *static_cast<double*>(ptr)).status() == QTextStream::Ok) {
					data_type = qMetaTypeId<double>();
					vector = new QVector<double>();
				}
			}
		}
	}

	if (data_type == 0) {
		str.setStatus(QTextStream::ReadCorruptData);
		return str;
	}

	// read line by line in vector
	qsizetype line_count = 0;
	qsizetype previous_column = 0;
	while (true) {
		if (isLineEmpty(line))
			break;

		qsizetype column_count = 0;

		QTextStream line_stream(&line);
		if (data_type == qMetaTypeId<VipRGB>()) {
			VipRGB tmp;
			QVector<QRgb>& v = *static_cast<QVector<QRgb>*>(vector);

			while ((line_stream >> tmp).status() == QTextStream::Ok) {
				v.append(tmp);
				column_count++;
			}

			if (!line_stream.atEnd()) {
				delete static_cast<QVector<QRgb>*>(vector);
				ar.clear();
				str.setStatus(QTextStream::ReadCorruptData);
				return str;
			}
		}
		else if (data_type == qMetaTypeId<complex_d>()) {
			complex_d tmp;
			QVector<complex_d>& v = *static_cast<QVector<complex_d>*>(vector);

			while ((line_stream >> tmp).status() == QTextStream::Ok) {
				v.append(tmp);
				column_count++;
			}

			if (!line_stream.atEnd()) {
				delete static_cast<QVector<complex_d>*>(vector);
				ar.clear();
				str.setStatus(QTextStream::ReadCorruptData);
				return str;
			}
		}
		if (data_type == qMetaTypeId<double>()) {
			double tmp;
			QVector<double>& v = *static_cast<QVector<double>*>(vector);

			while ((line_stream >> tmp).status() == QTextStream::Ok) {
				v.append(tmp);
				column_count++;
			}

			if (!line_stream.atEnd()) {
				delete static_cast<QVector<double>*>(vector);
				ar.clear();
				str.setStatus(QTextStream::ReadCorruptData);
				return str;
			}
		}

		// check for valid number of columns
		if (previous_column != 0 && previous_column != column_count) {
			str.setStatus(QTextStream::ReadCorruptData);
			if (data_type == QMetaType::Double)
				delete static_cast<QVector<double>*>(vector);
			else if (data_type == qMetaTypeId<complex_d>())
				delete static_cast<QVector<complex_d>*>(vector);
			else
				delete static_cast<QVector<QRgb>*>(vector);
			return str;
		}

		previous_column = column_count;
		line_count++;
		line = str.readLine();
	}

	// check for valid number of lines
	if (line_count == 0) {
		str.setStatus(QTextStream::ReadCorruptData);
		return str;
	}

	const VipNDArrayShape sh = vipVector(line_count, previous_column);
	// create the array
	if (data_type == QMetaType::Double) {
		QVector<double>* v = static_cast<QVector<double>*>(vector);
		if (ar.shape() != sh) {
			int dtype = ar.dataType();
			if (dtype == 0)
				dtype = QMetaType::Double;
			if (!ar.reset(sh, dtype)) {
				delete v;
				return str;
			}
		}
		VipNDArrayTypeView<double>(v->data(), sh).convert(ar);
		delete v;
	}
	else if (data_type == qMetaTypeId<complex_d>()) {
		QVector<complex_d>* v = static_cast<QVector<complex_d>*>(vector);
		if (ar.shape() != sh) {
			int dtype = ar.dataType();
			if (dtype == 0)
				dtype = qMetaTypeId<complex_d>();
			if (!ar.reset(sh, dtype)) {
				delete v;
				return str;
			}
		}
		ar = VipNDArray(data_type, vipVector(line_count, previous_column));
		VipNDArrayTypeView<complex_d>(v->data(), sh).convert(ar);
		delete v;
	}
	else {
		// for VipRGB, create a QImage
		QImage img(previous_column, line_count, QImage::Format_ARGB32);
		QVector<QRgb>* v = static_cast<QVector<QRgb>*>(vector);
		std::copy(v->begin(), v->end(), reinterpret_cast<QRgb*>(img.bits()));
		ar = vipToArray(img);
		delete v;
	}
	return str;
}

QTextStream& operator<<(QTextStream& stream, const VipNDArray& ar)
{
	stream.setRealNumberPrecision(17);

	// VipNDArray array(ar);
	//
	// if(ar.dataType() != qMetaTypeId<QString>())
	// array = ar.convert(qMetaTypeId<QString>());
	//
	// if(array.shapeCount() == 1)
	// {
	// VipNDArrayShape pos(1);
	// for(qsizetype i=0; i < array.size(); ++i)
	// {
	// pos[0]=i;
	// stream << *static_cast<const QString*>(array.data(pos));
	// stream << "\t";
	// }
	//
	// }
	// else if(ar.shapeCount()==2)
	// {
	// VipNDArrayShape pos(2);
	// for(qsizetype y=0; y < ar.shape(0); ++y)
	// {
	// pos[0]=y;
	// for(qsizetype x=0; x< ar.shape(1); ++x)
	// {
	// pos[1] = x;
	// stream << *static_cast<const QString*>(array.data(pos));
	// stream << "\t";
	// }
	//
	// stream << "\n";
	// }
	// }
	// else
	// {
	// stream.setStatus(QTextStream::ReadCorruptData);
	// }

	if (ar.shapeCount() == 1) {
		ar.handle()->oTextStream(vipVector(0), ar.shape(), stream, "\t");
		stream << "\n";
	}
	else if (ar.shapeCount() == 2) {
		for (qsizetype y = 0; y < ar.shape(0); ++y) {
			ar.handle()->oTextStream(vipVector(y, 0), vipVector(1, ar.shape(1)), stream, "\t");
			stream << "\n";
		}
	}
	else {
		stream.setStatus(QTextStream::ReadCorruptData);
	}

	return stream;
}

template<class T, class U>
std::complex<U> toComplex(const std::complex<T>& c)
{
	return std::complex<U>(c.real(), c.imag());
}

static VipPointVector pointVectorFromArray(const VipNDArray& ar)
{
	VipNDArray tmp = ar.convert<vip_double>();
	if (tmp.isEmpty())
		return VipPointVector();
	if (tmp.shape(0) != 2)
		return VipPointVector();

	VipNDArrayTypeView<vip_double> dar(tmp);
	VipPointVector res(dar.shape(1));
	std::vector<vip_double> x(dar.ptr(), dar.ptr() + dar.shape(1));
	std::vector<vip_double> y(dar.ptr() + dar.shape(1), dar.ptr() + dar.shape(1) * 2);
	for (qsizetype i = 0; i < dar.shape(1); ++i) {
		res[i] = VipPoint(dar(vipVector(0, i)), dar(vipVector(1, i)));
	}
	return res;
}

static VipNDArray arrayFromPointVector(const VipPointVector& vector)
{
	VipNDArrayType<vip_double> res(vipVector(2, (qsizetype)vector.size()));
	for (qsizetype i = 0; i < vector.size(); ++i) {
		res(vipVector(0, i)) = vector[i].x();
		res(vipVector(1, i)) = vector[i].y();
	}
	return res;
}

static VipComplexPointVector complexPointVectorFromArray(const VipNDArray& ar)
{
	VipNDArray tmp = ar.convert<complex_d>();
	if (tmp.isEmpty())
		return VipComplexPointVector();
	if (tmp.shape(0) != 2)
		return VipComplexPointVector();

	VipNDArrayTypeView<complex_d> dar(tmp);
	VipComplexPointVector res(dar.shape(1));
	for (qsizetype i = 0; i < dar.shape(1); ++i) {
		res[i] = VipComplexPoint(dar(vipVector(0, i)).real(), dar(vipVector(1, i)));
	}
	return res;
}

static VipNDArray arrayFromComplexPointVector(const VipComplexPointVector& vector)
{
	VipNDArrayType<complex_d> res(vipVector(2, (qsizetype)vector.size()));
	for (qsizetype i = 0; i < vector.size(); ++i) {
		res(vipVector(0, i)) = vector[i].x();
		res(vipVector(1, i)) = vector[i].y();
	}
	return res;
}

static QVariantList variantListFromIntervalSampleVector(const VipIntervalSampleVector& vec)
{
	// list of 2 arrays: values and intervals
	VipNDArrayType<vip_double> values(vipVector((qsizetype)vec.size()));
	VipNDArrayType<vip_double> intervals(vipVector((qsizetype)vec.size() * 2));
	for (qsizetype i = 0; i < vec.size(); ++i) {
		values(vipVector(i)) = vec[i].value;
		intervals(vipVector(i * 2)) = vec[i].interval.minValue();
		intervals(vipVector(i * 2 + 1)) = vec[i].interval.maxValue();
	}
	QVariantList tmp;
	tmp.append(QVariant::fromValue(VipNDArray(values)));
	tmp.append(QVariant::fromValue(VipNDArray(intervals)));
	return tmp;
}

static VipIntervalSampleVector intervalSampleVectorFromVariantList(const QVariantList& lst)
{
	if (lst.size() != 2)
		return VipIntervalSampleVector();

	const VipNDArrayType<vip_double> values = lst[0].value<VipNDArray>().convert<vip_double>();
	const VipNDArrayType<vip_double> intervals = lst[1].value<VipNDArray>().convert<vip_double>();

	if (values.isEmpty() || intervals.isEmpty())
		return VipIntervalSampleVector();

	if (values.shapeCount() != 1 || intervals.shapeCount() != 1)
		return VipIntervalSampleVector();

	if (values.shape(0) * 2 != intervals.shape(0))
		return VipIntervalSampleVector();

	VipIntervalSampleVector res;
	for (qsizetype i = 0; i < values.shape(0); ++i) {
		VipIntervalSample sample;
		sample.value = values(vipVector(i));
		sample.interval = VipInterval(intervals(vipVector(i * 2)), intervals(vipVector(i * 2 + 1)));
	}

	return res;
}

static VipNDArrayShape toVipNDArrayShape(const VipNDDoubleCoordinate& coords)
{
	return VipNDArrayShape(coords);
}

static VipNDDoubleCoordinate toVipNDDoubleCoordinate(const VipNDArrayShape& coords)
{
	return VipNDDoubleCoordinate(coords);
}

// register all possible conversion functions

template<class T>
vip_long_double toLongDouble(T v)
{
	return (vip_long_double)v;
}
template<class T>
T fromLongDouble(vip_long_double v)
{
	return (T)v;
}

#if VIP_USE_LONG_DOUBLE == 0
static VipPoint to_point(const VipLongPoint& pt)
{
	return VipPoint(pt);
}
static VipLongPoint to_l_point(const VipPoint& pt)
{
	return VipLongPoint(pt);
}
#endif

static int registerConversionFunctions()
{
	qRegisterMetaType<VipShape>();
	qRegisterMetaType<VipSceneModel>();
	qRegisterMetaType<VipSceneModelList>();

	qRegisterMetaType<VipRectList>();
	qRegisterMetaType<VipRectFList>();
	qRegisterMetaType<VipTimestampedRectList>();
	qRegisterMetaType<VipTimestampedRectFList>();
	qRegisterMetaType<VipTimestampedRectListVector>();
	qRegisterMetaType<VipTimestampedRectFListVector>();

	qRegisterMetaType<VipNDArray>();
	qRegisterMetaType<complex_f>();
	qRegisterMetaType<complex_d>();
	qRegisterMetaType<VipPoint>();
#if VIP_USE_LONG_DOUBLE == 0
	// VipLongPoint is different than VipPoint, we need to register it
	qRegisterMetaType<VipLongPoint>();
	qRegisterMetaTypeStreamOperators<VipLongPoint>("VipLongPoint");
#endif
	qRegisterMetaType<VipNDArrayShape>();
	qRegisterMetaType<VipNDDoubleCoordinate>();
	qRegisterMetaType<VipInterval>();
	qRegisterMetaType<VipIntervalSample>();
	qRegisterMetaType<VipPointVector>();
	qRegisterMetaType<VipIntervalSampleVector>();

	qRegisterMetaType<VipComplexPoint>();
	qRegisterMetaType<VipComplexPointVector>();

	qRegisterMetaTypeStreamOperators<VipRectList>("VipRectList");
	qRegisterMetaTypeStreamOperators<VipRectFList>("VipRectFList");
	qRegisterMetaTypeStreamOperators<VipTimestampedRectList>("VipTimestampedRectList");
	qRegisterMetaTypeStreamOperators<VipTimestampedRectFList>("VipTimestampedRectFList");
	qRegisterMetaTypeStreamOperators<VipTimestampedRectListVector>("VipTimestampedRectListVector");
	qRegisterMetaTypeStreamOperators<VipTimestampedRectFListVector>("VipTimestampedRectFListVector");

	qRegisterMetaTypeStreamOperators<VipPoint>("VipPoint");
	qRegisterMetaTypeStreamOperators<VipNDArrayShape>("VipNDArrayShape");
	qRegisterMetaTypeStreamOperators<VipNDDoubleCoordinate>("VipNDDoubleCoordinate");
	qRegisterMetaTypeStreamOperators<complex_f>("complex_f");
	qRegisterMetaTypeStreamOperators<complex_d>("complex_d");
	qRegisterMetaTypeStreamOperators<VipNDArray>("VipNDArray");
	qRegisterMetaTypeStreamOperators<VipInterval>("VipInterval");
	qRegisterMetaTypeStreamOperators<VipIntervalSample>("VipIntervalSample");
	qRegisterMetaTypeStreamOperators<VipPointVector>("VipPointVector");
	qRegisterMetaTypeStreamOperators<VipIntervalSampleVector>("VipIntervalSampleVector");
	qRegisterMetaTypeStreamOperators<VipComplexPoint>("VipComplexPoint");
	qRegisterMetaTypeStreamOperators<VipComplexPointVector>("VipComplexPointVector");

	QMetaType::registerConverter<VipLongPoint, QPoint>(&VipLongPoint::toPoint);
	QMetaType::registerConverter<VipLongPoint, QPointF>(&VipLongPoint::toPointF);
	QMetaType::registerConverter<QPoint, VipLongPoint>(&VipLongPoint::fromPoint);
	QMetaType::registerConverter<QPointF, VipLongPoint>(&VipLongPoint::fromPointF);

	// TEST
	QMetaType::registerConverter<VipLongPoint, QString>(detail::typeToString<VipLongPoint>);
	QMetaType::registerConverter<QString, VipLongPoint>(detail::stringToType<VipLongPoint>);
	QMetaType::registerConverter<VipLongPoint, QByteArray>(detail::typeToByteArray<VipLongPoint>);
	QMetaType::registerConverter<QByteArray, VipLongPoint>(detail::byteArrayToType<VipLongPoint>);

#if VIP_USE_LONG_DOUBLE == 0
	// VipPoint is different than VipLongPoint, we need to register its conversion operators
	QMetaType::registerConverter<VipPoint, QPoint>(&VipPoint::toPoint);
	QMetaType::registerConverter<VipPoint, QPointF>(&VipPoint::toPointF);
	QMetaType::registerConverter<QPoint, VipPoint>(&VipPoint::fromPoint);
	QMetaType::registerConverter<QPointF, VipPoint>(&VipPoint::fromPointF);

	QMetaType::registerConverter<VipPoint, VipLongPoint>(to_l_point);
	QMetaType::registerConverter<VipLongPoint, VipPoint>(to_point);

	QMetaType::registerConverter<VipPoint, QString>(detail::typeToString<VipPoint>);
	QMetaType::registerConverter<QString, VipPoint>(detail::stringToType<VipPoint>);
	QMetaType::registerConverter<VipPoint, QByteArray>(detail::typeToByteArray<VipPoint>);
	QMetaType::registerConverter<QByteArray, VipPoint>(detail::byteArrayToType<VipPoint>);
#endif
	// END TEST

	QMetaType::registerConverter<vip_long_double, std::complex<float>>(fromLongDouble<std::complex<float>>);
	QMetaType::registerConverter<vip_long_double, std::complex<double>>(fromLongDouble<std::complex<double>>);

	QMetaType::registerConverter<VipNDArrayShape, VipNDDoubleCoordinate>(toVipNDDoubleCoordinate);
	QMetaType::registerConverter<VipNDDoubleCoordinate, VipNDArrayShape>(toVipNDArrayShape);

	QMetaType::registerConverter<VipNDArray, VipPointVector>(pointVectorFromArray);
	QMetaType::registerConverter<VipPointVector, VipNDArray>(arrayFromPointVector);

	QMetaType::registerConverter<VipNDArray, VipComplexPointVector>(complexPointVectorFromArray);
	QMetaType::registerConverter<VipComplexPointVector, VipNDArray>(arrayFromComplexPointVector);

	QMetaType::registerConverter<QVariantList, VipIntervalSampleVector>(intervalSampleVectorFromVariantList);
	QMetaType::registerConverter<VipIntervalSampleVector, QVariantList>(variantListFromIntervalSampleVector);

	QMetaType::registerConverter<VipNDArray, QString>(detail::typeToString<VipNDArray>);
	QMetaType::registerConverter<QString, VipNDArray>(detail::stringToType<VipNDArray>);

	QMetaType::registerConverter<VipNDArray, QByteArray>(detail::typeToByteArray<VipNDArray>);
	QMetaType::registerConverter<QByteArray, VipNDArray>(detail::byteArrayToType<VipNDArray>);

	QMetaType::registerConverter<complex_f, QString>(detail::typeToString<complex_f>);
	QMetaType::registerConverter<QString, complex_f>(detail::stringToType<complex_f>);

	QMetaType::registerConverter<complex_f, QByteArray>(detail::typeToByteArray<complex_f>);
	QMetaType::registerConverter<QByteArray, complex_f>(detail::byteArrayToType<complex_f>);

	QMetaType::registerConverter<complex_d, QString>(detail::typeToString<complex_d>);
	QMetaType::registerConverter<QString, complex_d>(detail::stringToType<complex_d>);

	QMetaType::registerConverter<complex_d, QByteArray>(detail::typeToByteArray<complex_d>);
	QMetaType::registerConverter<QByteArray, complex_d>(detail::byteArrayToType<complex_d>);

	QMetaType::registerConverter<complex_f, complex_d>(toComplex<float, double>);
	QMetaType::registerConverter<complex_d, complex_f>(toComplex<double, float>);

	QMetaType::registerConverter<VipInterval, QString>(detail::typeToString<VipInterval>);
	QMetaType::registerConverter<QString, VipInterval>(detail::stringToType<VipInterval>);

	QMetaType::registerConverter<VipInterval, QByteArray>(detail::typeToByteArray<VipInterval>);
	QMetaType::registerConverter<QByteArray, VipInterval>(detail::byteArrayToType<VipInterval>);

	QMetaType::registerConverter<VipIntervalSample, QString>(detail::typeToString<VipIntervalSample>);
	QMetaType::registerConverter<QString, VipIntervalSample>(detail::stringToType<VipIntervalSample>);

	QMetaType::registerConverter<VipIntervalSample, QByteArray>(detail::typeToByteArray<VipIntervalSample>);
	QMetaType::registerConverter<QByteArray, VipIntervalSample>(detail::byteArrayToType<VipIntervalSample>);

	QMetaType::registerConverter<VipPointVector, QString>(detail::typeToString<VipPointVector>);
	QMetaType::registerConverter<QString, VipPointVector>(detail::stringToType<VipPointVector>);

	QMetaType::registerConverter<VipPointVector, QByteArray>(detail::typeToByteArray<VipPointVector>);
	QMetaType::registerConverter<QByteArray, VipPointVector>(detail::byteArrayToType<VipPointVector>);

	QMetaType::registerConverter<VipIntervalSampleVector, QString>(detail::typeToString<VipIntervalSampleVector>);
	QMetaType::registerConverter<QString, VipIntervalSampleVector>(detail::stringToType<VipIntervalSampleVector>);

	QMetaType::registerConverter<VipIntervalSampleVector, QByteArray>(detail::typeToByteArray<VipIntervalSampleVector>);
	QMetaType::registerConverter<QByteArray, VipIntervalSampleVector>(detail::byteArrayToType<VipIntervalSampleVector>);

	return 0;
}

static int _registerConversionFunctions = registerConversionFunctions();

VipNDArray vipExtractXValues(const VipPointVector& samples)
{
	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(samples.size()));
	vip_double* ptr = (vip_double*)res.data();
	for (qsizetype i = 0; i < samples.size(); ++i)
		ptr[i] = samples[i].x();
	return res;
}
VipNDArray vipExtractYValues(const VipPointVector& samples)
{
	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(samples.size()));
	vip_double* ptr = (vip_double*)res.data();
	for (qsizetype i = 0; i < samples.size(); ++i)
		ptr[i] = samples[i].y();
	return res;
}
VipNDArray vipExtractXValues(const VipComplexPointVector& samples)
{
	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(samples.size()));
	vip_double* ptr = (vip_double*)res.data();
	for (qsizetype i = 0; i < samples.size(); ++i)
		ptr[i] = samples[i].x();
	return res;
}
VipNDArray vipExtractYValues(const VipComplexPointVector& samples)
{
	VipNDArray res(qMetaTypeId<complex_d>(), vipVector(samples.size()));
	complex_d* ptr = (complex_d*)res.data();
	for (qsizetype i = 0; i < samples.size(); ++i)
		ptr[i] = samples[i].y();
	return res;
}

VipPointVector vipCreatePointVector(const VipNDArray& x, const VipNDArray& y)
{
	VipNDArray tx = x.convert<vip_double>();
	VipNDArray ty = y.convert<vip_double>();
	if (tx.size() != ty.size() || tx.shapeCount() != ty.shapeCount() || tx.shapeCount() != 1)
		return VipPointVector();
	VipPointVector res(ty.size());

	const vip_double* _x = (const vip_double*)tx.constData();
	const vip_double* _y = (const vip_double*)ty.constData();
	for (qsizetype i = 0; i < res.size(); ++i)
		res[i] = VipPoint(_x[i], _y[i]);
	return res;
}
VipComplexPointVector vipCreateComplexPointVector(const VipNDArray& x, const VipNDArray& y)
{
	VipNDArray tx = x.convert<vip_double>();
	VipNDArray ty = y.toComplexDouble();
	if (tx.size() != ty.size() || tx.shapeCount() != ty.shapeCount() || tx.shapeCount() != 1)
		return VipComplexPointVector();
	VipComplexPointVector res(ty.size());

	const vip_double* _x = (const vip_double*)tx.constData();
	const complex_d* _y = (const complex_d*)ty.constData();
	for (qsizetype i = 0; i < res.size(); ++i)
		res[i] = VipComplexPoint(_x[i], _y[i]);
	return res;
}
bool vipSetYValues(VipPointVector& samples, const VipNDArray& y)
{
	VipNDArray ty = y.convert<vip_double>();
	if (ty.shapeCount() != 1 || ty.size() != samples.size())
		return false;
	const vip_double* _y = (const vip_double*)ty.constData();
	for (qsizetype i = 0; i < samples.size(); ++i)
		samples[i].setY(_y[i]);
	return true;
}
bool vipSetYValues(VipComplexPointVector& samples, const VipNDArray& y)
{
	VipNDArray ty = y.toComplexDouble();
	if (ty.shapeCount() != 1 || ty.size() != samples.size())
		return false;
	const complex_d* _y = (const complex_d*)ty.constData();
	for (qsizetype i = 0; i < samples.size(); ++i)
		samples[i].setY(_y[i]);
	return true;
}

VipComplexPointVector vipToComplexPointVector(const VipPointVector& samples)
{
	VipComplexPointVector res(samples.size());
	for (qsizetype i = 0; i < samples.size(); ++i) {
		res[i].setX(samples[i].x());
		res[i].setY(samples[i].y());
	}
	return res;
}

template<class T, class U, class PA, class PB>
bool __vipResampleVectors(QVector<T>& a, QVector<U>& b, ResampleStrategies s, const PA& padd_a, const PB& padd_b)
{
	if (a.size() == 0 || b.size() == 0)
		return false;

	if (s & ResampleIntersection) {
		// null intersection
		if (a.last().x() < b.first().x())
			return false;
		else if (a.first().x() > b.last().x())
			return false;

		// clamp lower boundary
		qsizetype i = 0;
		while (i + 1 < a.size() && a[i].x() < b.first().x() && a[i + 1].x() < b.first().x())
			++i;
		a = a.mid(i);
		i = 0;
		while (i < b.size() && b[i].x() < a.first().x() && b[i + 1].x() < a.first().x())
			++i;
		b = b.mid(i);

		// clamp higher boundary
		i = a.size() - 1;
		while (i > 0 && a[i].x() > b.last().x() && a[i - 1].x() > b.last().x())
			--i;
		a = a.mid(0, i + 1);
		i = b.size() - 1;
		while (i > 0 && b[i].x() > a.last().x() && b[i - 1].x() > a.last().x())
			--i;
		b = b.mid(0, i + 1);
	}
	else {
		// padd
		QVector<T> prev_a, next_a;
		QVector<U> prev_b, next_b;

		// add missing points at the beginning
		qsizetype i = 0;
		while (i + 1 < a.size() && a[i].x() < b.first().x() && a[i + 1].x() < b.first().x()) {
			if (s & ResamplePadd0)
				prev_b.append(U(a[i].x(), padd_b));
			else
				prev_b.append(U(a[i].x(), b.first().y()));
			++i;
		}
		i = 0;
		while (i < b.size() && b[i].x() < a.first().x() && b[i + 1].x() < a.first().x()) {
			if (s & ResamplePadd0)
				prev_a.append(T(b[i].x(), padd_a));
			else
				prev_a.append(T(b[i].x(), a.first().y()));
			++i;
		}

		// add missing points at the end
		i = a.size() - 1;
		while (i > 0 && a[i].x() > b.last().x() && a[i - 1].x() > b.last().x()) {
			if (s & ResamplePadd0)
				next_b.append(U(a[i].x(), padd_b));
			else
				next_b.append(U(a[i].x(), b.last().y()));
			--i;
		}
		i = b.size() - 1;
		while (i > 0 && b[i].x() > a.last().x() && b[i - 1].x() > a.last().x()) {
			if (s & ResamplePadd0)
				next_a.append(T(b[i].x(), padd_a));
			else
				next_a.append(T(b[i].x(), a.last().y()));
			--i;
		}

		a = prev_a + a + next_a;
		b = prev_b + b + next_b;
	}

	QVector<T> ra;
	QVector<U> rb;
	typename QVector<T>::const_iterator ita = a.begin();
	typename QVector<U>::const_iterator itb = b.begin();

	while (ita != a.end() && itb != b.end()) {
		// same x value, keep both
		if (ita->x() == itb->x()) {
			ra.append(*ita);
			rb.append(*itb);
			++ita;
			++itb;
		}
		else if (ita->x() < itb->x()) {
			// catch up ita
			const U prev_b = (itb == b.begin() ? *itb : *(itb - 1));
			const U next_b = *itb;
			do {
				// keep a values, create new b values
				ra.append(*ita);
				U new_b(ita->x(), 0);
				if (s & ResampleInterpolation) {
					// create b value by interpoling closest 2 points
					if (prev_b.x() == next_b.x())
						new_b.setY(next_b.y());
					else {
						double factor = (ita->x() - prev_b.x()) / (next_b.x() - prev_b.x());
						new_b.setY((1 - factor) * prev_b.y() + factor * next_b.y());
					}
				}
				else {
					// create b value by picking the closest b value
					new_b.setY(fabs(prev_b.x() - ita->x()) < fabs(next_b.x() - ita->x()) ? prev_b.y() : next_b.y());
				}
				rb.append(new_b);
				++ita;
			} while (ita != a.end() && ita->x() < itb->x());
		}
		else {
			// catch up itb
			const T prev_a = (ita == a.begin() ? *ita : *(ita - 1));
			const T next_a = *ita;
			do {
				// keep a values, create new b values
				rb.append(*itb);
				T new_a(itb->x(), 0);
				if (s & ResampleInterpolation) {
					// create b value by interpoling closest 2 points
					if (prev_a.x() == next_a.x())
						new_a.setY(next_a.y());
					else {
						double factor = (itb->x() - prev_a.x()) / (next_a.x() - prev_a.x());
						new_a.setY((1 - factor) * prev_a.y() + factor * next_a.y());
					}
				}
				else {
					// create b value by picking the closest b value
					new_a.setY(fabs(prev_a.x() - itb->x()) < fabs(next_a.x() - itb->x()) ? prev_a.y() : next_a.y());
				}
				ra.append(new_a);
				++itb;
			} while (itb != b.end() && itb->x() < ita->x());
		}
	}

	// last values
	// if(itb != b.end()) {
	//  rb.append(*itb);
	//  ra.append(T(itb->x(),a.last().y()));
	//  }
	//  if(ita != a.end()) {
	//  ra.append(*ita);
	//  rb.append(U(ita->x(),b.last().y()));
	//  }

	a = ra;
	b = rb;
	return true;
}

/// \internal
/// Iter over the times of a VipPointVector or VipComplexPointVector
struct TimeIterator
{
	const bool is_point_vector{ true };
	const void* data{ nullptr };
	qsizetype pos{ 0 };

	TimeIterator() noexcept = default;
	TimeIterator(const VipPointVector& v, bool begin) noexcept
	  : is_point_vector(true)
	  , data(&v)
	  , pos(begin ? 0 : v.size())
	{
	}
	TimeIterator(const VipComplexPointVector& v, bool begin) noexcept
	  : is_point_vector(false)
	  , data(&v)
	  , pos(begin ? 0 : v.size())
	{
	}

	vip_double prevTime() const noexcept { return is_point_vector ? (*static_cast<const VipPointVector*>(data))[pos - 1].x() : (*static_cast<const VipComplexPointVector*>(data))[pos - 1].x(); }
	vip_double operator*() const noexcept { return is_point_vector ? (*static_cast<const VipPointVector*>(data))[pos].x() : (*static_cast<const VipComplexPointVector*>(data))[pos].x(); }
	TimeIterator& operator++() noexcept
	{
		++pos;
		return *this;
	}
	TimeIterator operator++(int) noexcept
	{
		TimeIterator retval = *this;
		++(*this);
		return retval;
	}
	TimeIterator& operator+=(qsizetype offset) noexcept
	{
		pos += offset;
		return *this;
	}
	TimeIterator& operator-=(qsizetype offset) noexcept
	{
		pos -= offset;
		return *this;
	}
	TimeIterator& operator--() noexcept
	{
		--pos;
		return *this;
	}
	TimeIterator operator--(int) noexcept
	{
		TimeIterator retval = *this;
		--(*this);
		return retval;
	}
	bool operator==(const TimeIterator& other) const noexcept { return data == other.data && pos == other.pos; }
	bool operator!=(const TimeIterator& other) const noexcept { return !(*this == other); }
};

/// \internal
/// Extract the time vector for several VipPointVector and VipComplexPointVector.
/// Returns a growing time vector containing all different time values of given VipPointVector and VipComplexPointVector samples,
/// respecting the given resample strategy.
template<class Vector1, class Vector2>
static QVector<vip_double> vipExtractTimes(const QVector<Vector1>& vectors, const QVector<Vector2>& cvectors, ResampleStrategies s)
{
	if (vectors.isEmpty() && cvectors.isEmpty())
		return QVector<vip_double>();
	else if (vectors.size() == 1 && cvectors.isEmpty()) {
		QVector<vip_double> times(vectors.first().size());
		for (qsizetype i = 0; i < times.size(); ++i)
			times[i] = vectors.first()[i].x();
		return times;
	}
	else if (cvectors.size() == 1 && vectors.isEmpty()) {
		QVector<vip_double> times(cvectors.first().size());
		for (qsizetype i = 0; i < times.size(); ++i)
			times[i] = cvectors.first()[i].x();
		return times;
	}

	QVector<vip_double> res;
	res.reserve(vectors.size() ? vectors.first().size() : cvectors.first().size());

	// create our time iterators
	QVector<TimeIterator> iters, ends;
	for (qsizetype i = 0; i < vectors.size(); ++i) {
		// search for nan time value
		const Vector1& v = vectors[i];
		qsizetype p = 0;
		for (; p < v.size(); ++p) {
			if (vipIsNan(v[p].x()))
				break;
		}
		if (p != v.size()) {
			// split the vector in 2 vectors
			iters.append(TimeIterator(vectors[i], true));
			ends.append(TimeIterator(vectors[i], true));
			ends.last() += p;
			iters.append(TimeIterator(vectors[i], true));
			iters.last() += p + 1;
			ends.append(TimeIterator(vectors[i], false));
		}
		else {
			iters.append(TimeIterator(vectors[i], true));
			ends.append(TimeIterator(vectors[i], false));
		}
	}
	for (qsizetype i = 0; i < cvectors.size(); ++i) {
		// search for nan time value
		const Vector2& v = cvectors[i];
		qsizetype p = 0;
		for (; p < v.size(); ++p) {
			if (vipIsNan(v[p].x()))
				break;
		}
		if (p != v.size()) {
			// split the vector in 2 vectors
			iters.append(TimeIterator(cvectors[i], true));
			ends.append(TimeIterator(cvectors[i], true));
			ends.last() += p;
			iters.append(TimeIterator(cvectors[i], true));
			iters.last() += p + 1;
			ends.append(TimeIterator(cvectors[i], false));
		}
		else {
			iters.append(TimeIterator(cvectors[i], true));
			ends.append(TimeIterator(cvectors[i], false));
		}
	}

	if (s & ResampleIntersection) {
		// resample on intersection:
		// find the intersection time range
		vip_double start = 0, end = -1;
		for (qsizetype i = 0; i < vectors.size(); ++i) {
			if (end < start) { // init
				start = vectors[i].first().x();
				end = vectors[i].last().x();
			}
			else { // intersection
				if (vectors[i].last().x() < start)
					return res; // null intersection
				else if (vectors[i].first().x() > end)
					return res; // null intersection
				start = qMax(start, vectors[i].first().x());
				end = qMin(end, vectors[i].last().x());
			}
		}
		for (qsizetype i = 0; i < cvectors.size(); ++i) {
			if (end < start) { // init
				start = cvectors[i].first().x();
				end = cvectors[i].last().x();
			}
			else { // intersection
				if (cvectors[i].last().x() < start)
					return res; // null intersection
				else if (cvectors[i].first().x() > end)
					return res; // null intersection
				start = qMax(start, cvectors[i].first().x());
				end = qMin(end, cvectors[i].last().x());
			}
		}

		// reduce the iterator ranges
		for (qsizetype i = 0; i < iters.size(); ++i) {
			while (*iters[i] < start)
				++iters[i];
			if (ends[i].prevTime() > end) {
				--ends[i];
				while (*ends[i] > end)
					--ends[i];
			}
		}
	}

	while (iters.size()) {
		// find minimum time among all time vectors
		vip_double min_time = *iters.first();
		for (qsizetype i = 1; i < iters.size(); ++i)
			min_time = qMin(min_time, *iters[i]);

		// increment each iterator equal to min_time
		for (qsizetype i = 0; i < iters.size(); ++i) {
			if (iters[i] != ends[i])
				if (*iters[i] == min_time)
					if (++iters[i] == ends[i]) {
						iters.removeAt(i);
						ends.removeAt(i);
						--i;
					}
		}
		res.append(min_time);
	}

	return res;
}

template<class Point, class Vector, class U>
Vector vipResampleInternal(const Vector& sample, const QVector<vip_double>& times, ResampleStrategies s, const U& padds)
{
	Vector res(times.size());
	typename Vector::const_iterator iters = sample.begin();
	typename Vector::const_iterator ends = sample.end();

	for (qsizetype t = 0; t < times.size(); ++t) {
		const vip_double time = times[t];

		// we already reached the last sample value
		if (iters == ends) {
			if (s & ResamplePadd0)
				res[t] = (Point(time, padds));
			else
				res[t] = (Point(time, sample.last().y()));
			continue;
		}

		const Point& samp = *iters;

		// same time: just add the sample
		if (time == samp.x()) {
			res[t] = (samp);
			++iters;
		}
		// we are before the sample
		else if (time < samp.x()) {
			// sample starts after
			if (iters == sample.begin()) {
				if (s & ResamplePadd0)
					res[t] = (Point(time, padds));
				else
					res[t] = (Point(time, samp.y()));
			}
			// in between 2 values
			else {
				typename Vector::const_iterator prev = iters - 1;
				if (s & ResampleInterpolation) {
					double factor = (double)(time - prev->x()) / (double)(samp.x() - prev->x());
					res[t] = (Point(time, samp.y() * factor + (1 - factor) * prev->y()));
				}
				else {
					// take the closest value
					if (time - prev->x() < samp.x() - time)
						res[t] = (Point(time, prev->y()));
					else
						res[t] = (Point(time, samp.y()));
				}
			}
		}
		else {
			// we are after the sample, increment until this is not the case
			while (iters != ends && iters->x() < time)
				++iters;
			if (iters != ends) {
				if (iters->x() == time) {
					res[t] = (Point(time, iters->y()));
				}
				else {
					typename Vector::const_iterator prev = iters - 1;
					if (s & ResampleInterpolation) {
						// interpolation
						double factor = (double)(time - prev->x()) / (double)(iters->x() - prev->x());
						res[t] = (Point(time, iters->y() * factor + (1 - factor) * prev->y()));
					}
					else {
						// take the closest value
						if (time - prev->x() < iters->x() - time)
							res[t] = (Point(time, prev->y()));
						else
							res[t] = (Point(time, iters->y()));
					}
				}
			}
			else {
				// reach sample end
				if (s & ResamplePadd0)
					res[t] = (Point(time, padds));
				else
					res[t] = (Point(time, sample.last().y()));
			}
		}
	}
	return res;
}
// QList<VipPointVector> vipResample(const QList<VipPointVector > & _samples, ResampleStrategies s, vip_double padd)
// {
// return vipResampleInternal<VipPoint>(_samples, s, padd);
// }
// QList<VipComplexPointVector> vipResample(const QList<VipComplexPointVector > & _samples, ResampleStrategies s, complex_d padd )
// {
// return vipResampleInternal<VipComplexPoint>(_samples, s, padd);
// }

bool vipResampleVectors(VipPointVector& first, VipPointVector& second, ResampleStrategies s, vip_double padd_a, vip_double padd_b)
{
	// return __vipResampleVectors(first,second,s,padd_a,padd_b);
	QVector<vip_double> times = vipExtractTimes(QVector<VipPointVector>() << first << second, QVector<VipPointVector>(), s);
	if (times.isEmpty())
		return false;
	first = vipResampleInternal<VipPoint>(first, times, s, padd_a);
	second = vipResampleInternal<VipPoint>(second, times, s, padd_b);
	return true;
}
bool vipResampleVectors(VipComplexPointVector& first, VipComplexPointVector& second, ResampleStrategies s, complex_d padd_a, complex_d padd_b)
{
	// return __vipResampleVectors(first,second,s,padd_a,padd_b);
	QVector<vip_double> times = vipExtractTimes(QVector<VipComplexPointVector>() << first << second, QVector<VipPointVector>(), s);
	if (times.isEmpty())
		return false;
	first = vipResampleInternal<VipComplexPoint>(first, times, s, padd_a);
	second = vipResampleInternal<VipComplexPoint>(second, times, s, padd_b);
	return true;
}
bool vipResampleVectors(VipPointVector& first, VipComplexPointVector& second, ResampleStrategies s, vip_double padd_a, complex_d padd_b)
{
	// return __vipResampleVectors(first,second,s,padd_a,padd_b);
	QVector<vip_double> times = vipExtractTimes(QVector<VipPointVector>() << first, QVector<VipComplexPointVector>() << second, s);
	if (times.isEmpty())
		return false;
	first = vipResampleInternal<VipPoint>(first, times, s, padd_a);
	second = vipResampleInternal<VipComplexPoint>(second, times, s, padd_b);
	return true;
}

bool vipResampleVectors(QList<VipPointVector>& lst, ResampleStrategies s, vip_double padd)
{
	if (lst.size() == 0)
		return false;
	else if (lst.size() == 1)
		return true;

	QVector<vip_double> times = vipExtractTimes(lst.toVector(), QVector<VipPointVector>(), s);
	if (times.isEmpty())
		return false;
	for (qsizetype i = 0; i < lst.size(); ++i)
		lst[i] = vipResampleInternal<VipPoint>(lst[i], times, s, padd);
	return true;
}

bool vipResampleVectors(QList<VipPointVector>& lst, vip_double x_step, ResampleStrategies s, vip_double padd)
{
	if (lst.size() == 0)
		return false;

	QVector<vip_double> times = vipExtractTimes(lst.toVector(), QVector<VipPointVector>(), s);
	if (times.isEmpty())
		return false;

	vip_double xmin = times.first();
	vip_double xmax = times.last();
	times.clear();
	// qsizetype pts = (xmax - xmin) / x_step;
	for (vip_double v = xmin; v <= xmax; v += x_step)
		times.push_back(v);

	for (qsizetype i = 0; i < lst.size(); ++i)
		lst[i] = vipResampleInternal<VipPoint>(lst[i], times, s, padd);
	return true;
}

bool vipResampleVectors(QList<VipComplexPointVector>& lst, ResampleStrategies s, complex_d padd)
{
	if (lst.size() == 0)
		return false;
	else if (lst.size() == 1)
		return true;

	QVector<vip_double> times = vipExtractTimes(lst.toVector(), QVector<VipComplexPointVector>(), s);
	if (times.isEmpty())
		return false;
	for (qsizetype i = 0; i < lst.size(); ++i)
		lst[i] = vipResampleInternal<VipComplexPoint>(lst[i], times, s, padd);
	return true;
}
bool vipResampleVectors(QList<VipPointVector>& lst_a, QList<VipComplexPointVector>& lst_b, ResampleStrategies s, vip_double padd_a, complex_d padd_b)
{
	if (lst_a.size() == 0 && lst_b.size() == 0)
		return false;

	QVector<vip_double> times = vipExtractTimes(lst_a.toVector(), lst_b.toVector(), s);
	if (times.isEmpty())
		return false;

	for (qsizetype i = 0; i < lst_a.size(); ++i)
		lst_a[i] = vipResampleInternal<VipPoint>(lst_a[i], times, s, padd_a);
	for (qsizetype i = 0; i < lst_b.size(); ++i)
		lst_b[i] = vipResampleInternal<VipComplexPoint>(lst_b[i], times, s, padd_b);
	return true;
}

VipNDArray vipResampleVectorsAsNDArray(const QList<VipPointVector>& vectors, ResampleStrategies s, vip_double padd)
{
	QList<VipPointVector> tmp = vectors;
	if (!vipResampleVectors(tmp, s, padd))
		return VipNDArray();
	if (tmp.isEmpty())
		return VipNDArray();

	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(tmp.first().size(), tmp.size() + 1));
	vip_double* values = (vip_double*)res.data();
	qsizetype width = tmp.size() + 1;

	// copy X values;
	const VipPointVector& first = tmp.first();
	for (qsizetype i = 0; i < first.size(); ++i)
		values[i * width] = first[i].x();

	// copy all y values
	for (qsizetype j = 0; j < tmp.size(); ++j) {
		const VipPointVector& vec = tmp[j];
		qsizetype start = j + 1;
		for (qsizetype i = 0; i < vec.size(); ++i)
			values[start + i * width] = vec[i].y();
	}

	return res;
}
