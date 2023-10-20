#include "PyProcess.h"
#include "VipNDArrayImage.h"
#include "VipCore.h"
#include "VipNetwork.h"
#include "VipLogging.h"
#include "IPython.h"

#include <qendian.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qcoreapplication.h>
#include <qfileinfo.h>
#include <qdir.h>

static int readSize(QIODevice * d)
{
	int size = 0;
	d->read((char*)(&size), 4);
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
	size = qFromLittleEndian(size);
#endif
	return size;
}

/*static int readSize(VipNetworkConnection * d)
{
	int size = 0;
	QByteArray t = d->read(4);
	if(t.size() == 4)
		memcpy(&size, t.data(), 4);
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
	size = qFromLittleEndian(size);
#endif
	return size;
}*/

static QByteArray readData(QIODevice * d, int size)
{
	while(d->bytesAvailable() == 0)
		d->waitForReadyRead(1);

	QByteArray res = d->read(size);
	while(res.size() != size)
	{
		if (res.size() == 0)
			return res;
		while (d->bytesAvailable() == 0)
			d->waitForReadyRead(1);
		res += d->read(size - res.size());
	}
	return res;
}

/*static QByteArray readData(VipNetworkConnection * d, int size)
{
	QByteArray res = d->read(size);
	while (res.size() != size)
	{
		if (res.size() == 0)
			return res;
		d->waitForReadyRead(1);
		res += d->read(size - res.size());
	}
	return res;
}*/

static int cnumptToQt(char type)
{
	switch (type)
	{
	case '?': return QMetaType::Bool;
	case 'b': return QMetaType::SChar;
	case 'B': return QMetaType::UChar;
	case 'h': return QMetaType::Short;
	case 'H': return QMetaType::UShort;
	case 'i': return QMetaType::Int;
	case 'I': return QMetaType::UInt;
	case 'l': return QMetaType::Long;
	case 'L': return QMetaType::ULong;
	case 'q': return QMetaType::LongLong;
	case 'Q': return QMetaType::ULongLong;
	case 'f': return QMetaType::Float;
	case 'd': return QMetaType::Double;
	case 'S': return QMetaType::QByteArray;
	case 'U': return QMetaType::QString;
	case 'F': return qMetaTypeId<complex_f>();
	case 'D': return qMetaTypeId<complex_d>();
	}
	return 0;
}

static char cqtToNumpy(int type)
{
	switch (type)
	{
	case  QMetaType::Bool: return '?';
	case QMetaType::SChar: return 'b';
	case QMetaType::Char: return 'b';
	case QMetaType::UChar: return 'B';
	case QMetaType::Short: return 'h';
	case QMetaType::UShort: return 'H';
	case QMetaType::Int: return 'i';
	case QMetaType::UInt: return 'I';
	case QMetaType::Long: return 'l';
	case QMetaType::ULong: return 'L';
	case QMetaType::LongLong: return 'q';
	case QMetaType::ULongLong: return 'Q';
	case QMetaType::Float: return 'f';
	case QMetaType::Double: return 'd';
	case QMetaType::QByteArray: return 'S';
	case QMetaType::QString: return 'U';
	}
	if (type == qMetaTypeId<complex_f>())
		return 'F';
	else if (type == qMetaTypeId<complex_d>())
		return 'D';
		
	return 0;
}

template< class T>
static QByteArray fromT(T v)
{
	QByteArray b;
	QDataStream str(&b, QIODevice::WriteOnly);
	str.setByteOrder(QDataStream::LittleEndian);
	str.setFloatingPointPrecision(QDataStream::DoublePrecision);
	str << v;
	str.unsetDevice();
	return b;
}
template< class T>
static T toT(const QByteArray & ar)
{
	QDataStream str(&ar);
	T val;
	str.setByteOrder(QDataStream::LittleEndian);
	str.setFloatingPointPrecision(QDataStream::DoublePrecision);
	str >> val;
	return val;
}

static QByteArray fromNDArray(const VipNDArray & ar)
{
	//qint64 st = QDateTime::currentMSecsSinceEpoch();

	if (ar.dataType() == qMetaTypeId<QImage>()) {
		//serialize a QImage as an array of dtype UChar and shape (height,width,4)
		QImage img = vipToImage(ar);
		//swap byte order
/*#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
		uint* p = (uint*)img.constBits();
		int size = img.width()*img.height();
		for (int i = 0; i < size; ++i) 
			p[i] = qFromBigEndian(p[i]);
#endif*/
		VipNDArray tmp(QMetaType::UChar, (VipNDArrayShape(ar.shape()) << 3));
		unsigned char * data = (unsigned char*)tmp.data();
		const uint * bits = (const uint * )img.constBits();
		int size = img.width()*img.height();
		for (int i = 0; i < size; ++i) {
			data[0] = qRed(bits[i]);
			data[1] = qGreen(bits[i+1]);
			data[2] = qBlue(bits[i+2]);
			data += 3;
		}
		return fromNDArray(tmp);
	}

	QByteArray b;
	QDataStream str(&b, QIODevice::WriteOnly);
	str.setByteOrder(QDataStream::LittleEndian);
	char type = cqtToNumpy(ar.dataType());
	str.writeRawData(&type,1);
	str << ar.shapeCount();
	for (int i = 0; i < ar.shapeCount(); ++i)
		str << ar.shape(i);

	//ar.handle()->ostream(VipNDArrayShape(0, ar.shapeCount()), ar.shape(), str);
	str.writeRawData((const char*)ar.constData(), ar.size()*ar.dataSize());

	str.unsetDevice();

	//qint64 el  = QDateTime::currentMSecsSinceEpoch() - st;
	//vip_debug("fromNDArray: %i\n", (int)el);
	return  b;
}
VipNDArray toNDArray(const QByteArray & ar, int *len = nullptr)
{
	try {
		//qint64 st = QDateTime::currentMSecsSinceEpoch();

		QDataStream str(ar);
		str.setByteOrder(QDataStream::LittleEndian);

		char type = 0;
		int shape_count = 0;
		VipNDArrayShape shape;

		str.readRawData(&type, 1);
		int qtype = cnumptToQt(type);
		if (qtype == 0)
			return VipNDArray();

		str >> shape_count;
		shape.resize(shape_count);
		int full_size = 1;
		for (int i = 0; i < shape_count; ++i)
		{
			str >> shape[i];
			full_size *= shape[i];
		}

		if (len) 
			*len = 5 + shape_count * 4 + full_size * QMetaType(qtype).sizeOf();

		//QImage type
		if (/*(qtype == QMetaType::UChar || qtype == QMetaType::Char) &&*/ shape_count == 3 && (shape[2] == 3 || shape[2] == 4)) {

			//convert to uchar
			const unsigned char * data = (const unsigned char*)ar.data() + 1 + 4 * (shape_count + 1);
			VipNDArray uch;
			if (qtype != QMetaType::UChar && qtype != QMetaType::SChar && qtype != QMetaType::Char)
			{
				uch = VipNDArray::makeView(data, shape).toUInt8();
				data = (const unsigned char*)uch.constData();
			}

			QImage img(shape[1], shape[0], QImage::Format_ARGB32);
			int size = shape[1] * shape[0];
			uint * pix = (uint*)img.bits();
			

			if (shape[2] == 4) {
				for (int i = 0; i < size; ++i) {
					pix[i] = qRgba(data[1], data[2], data[3], data[0]);
					data += 4;
				}
			}
			else {
				for (int i = 0; i < size; ++i) {
					pix[i] = qRgb(data[0], data[1], data[2]);
					data += 3;
				}
			}
			return vipToArray(img);
		}

		VipNDArray res(qtype,shape);
		
		//res.handle()->istream(VipNDArrayShape(0, shape_count), res.shape(), str);
		memcpy(res.data(), ar.data() + 1 + 4 + shape_count * 4, full_size*res.dataSize());

		//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		//vip_debug("to NDArray: %i\n", (int)el);

		return res;
	}
	catch (...) {
		return VipNDArray();
	}
}


QByteArray variantToBytes(const QVariant & obj)
{
	QByteArray res;
	switch (obj.userType())
	{
	case QMetaType::Char:
	case QMetaType::SChar:
	case QMetaType::UChar:
	case QMetaType::Short:
	case QMetaType::UShort:
	case QMetaType::Int:
	case QMetaType::UInt:
	case QMetaType::Bool:
		res = fromT((int)PY_CODE_INT) + fromT(obj.toInt());
		break;
	
	case QMetaType::Long:
	case QMetaType::ULong:
	case QMetaType::LongLong:
	case QMetaType::ULongLong:
		res = fromT((int)PY_CODE_LONG) + fromT(obj.toLongLong());
		break;
	case QMetaType::Float:
	case QMetaType::Double:
		res = fromT((int)PY_CODE_DOUBLE)+ fromT(obj.toDouble());
		break;
	case QMetaType::QString:
		{
			QString str = obj.toString();
			res = fromT((int)PY_CODE_STRING) + fromT((int)(2 * str.size())) + QByteArray((char*)str.data(), 2 * str.size());
		}
		break;
	
	case QMetaType::QByteArray:
		{
		QByteArray ar = obj.toByteArray();
		res = fromT((int)PY_CODE_BYTES) + fromT((int)(ar.size())) + ar;
		}
		break;
	case 0:
		res = fromT((int)PY_CODE_NONE);
		break;
	}

	//other types (complex , list, array,...)
	if (res.isEmpty())
	{
		if (obj.userType() == qMetaTypeId<complex_d>() || obj.userType() == qMetaTypeId<complex_f>())
		{
			complex_d comp = obj.value<complex_d>();
			res = fromT((int)PY_CODE_COMPLEX) + fromT(comp.real()) + fromT(comp.imag());
		}
		else if (obj.userType() == qMetaTypeId<QVariantList>())
		{
			QVariantList lst = obj.value<QVariantList>();
			int count = 0;
			for (int i = 0; i < lst.size(); ++i)
			{
				QByteArray tmp = variantToBytes(lst[i]);
				if (tmp.size())
				{
					res += tmp;
					++count;
				}
			}
			res = fromT((int)PY_CODE_LIST) +  fromT((int)count) + res;
		}
		else if (obj.userType() == qMetaTypeId<QStringList>())
		{
			QStringList lst = obj.value<QStringList>();
			int count = 0;
			for (int i = 0; i < lst.size(); ++i)
			{
				QByteArray tmp = variantToBytes(lst[i]);
				if (tmp.size())
				{
					res += tmp;
					++count;
				}
			}
			res = fromT((int)PY_CODE_LIST) + fromT((int)count)+ res;
		}
		else if (obj.userType() == qMetaTypeId<QVariantMap>())
		{
			QVariantMap map = obj.value<QVariantMap>();
			int count = 0;
			for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it)
			{
				QByteArray key = variantToBytes(it.key());
				QByteArray value = variantToBytes(it.value());
				if (value.size())
				{
					res += key;
					res += value;
					++count;
				}
			}
			res = fromT((int)PY_CODE_DICT) + fromT((int)count) + res;
		}
		else if (obj.userType() == qMetaTypeId<VipPointVector>())
		{
			VipPointVector vec = obj.value<VipPointVector>();
			//array of 2 rows (x and y)
			VipNDArrayType<double> ar(vipVector(2, vec.size()));
			for (int i = 0; i < vec.size(); ++i)
			{
				ar(vipVector(0, i)) = vec[i].x();
				ar(vipVector(1, i)) = vec[i].y();
			}
			res = fromNDArray((ar));
			res = fromT((int)PY_CODE_POINT_VECTOR) + fromT((int)PY_CODE_NDARRAY) + res;
		}
		else if (obj.userType() == qMetaTypeId<VipComplexPointVector>())
		{
			VipComplexPointVector vec = obj.value<VipComplexPointVector>();
			//array of 2 rows (x and y)
			VipNDArrayType<complex_d> ar(vipVector(2, vec.size()));
			for (int i = 0; i < vec.size(); ++i)
			{
				ar(vipVector(0, i)) = vec[i].x();
				ar(vipVector(1, i)) = vec[i].y();
			}
			res = fromNDArray(ar);
			res = fromT((int)PY_CODE_COMPLEX_POINT_VECTOR) + fromT((int)PY_CODE_NDARRAY) + res;
		}
		else if (obj.userType() == qMetaTypeId<VipIntervalSampleVector>())
		{
			VipIntervalSampleVector vec = obj.value<VipIntervalSampleVector>();
			//list of 2 arrays: values and intervals
			VipNDArrayType<double> values(vipVector(vec.size()));
			VipNDArrayType<double> intervals(vipVector(vec.size() * 2));
			for (int i = 0; i < vec.size(); ++i)
			{
				values(vipVector(i)) = vec[i].value;
				intervals(vipVector(i * 2)) = vec[i].interval.minValue();
				intervals(vipVector(i * 2 + 1)) = vec[i].interval.maxValue();
			}
			QVariantList tmp;
			tmp.append(QVariant::fromValue(VipNDArray(values)));
			tmp.append(QVariant::fromValue(VipNDArray(intervals)));
			res = variantToBytes(QVariant::fromValue(tmp));
			res = fromT((int)PY_CODE_INTERVAL_SAMPLE_VECTOR) +  res;
		}
		else if (obj.userType() == qMetaTypeId<VipNDArray>())
		{
			const VipNDArray info = obj.value<VipNDArray>();
			res = fromNDArray(info);
			res = fromT((int)PY_CODE_NDARRAY) + res;
		}
		
	}

	return res;
}


#define _CHECK_ERROR() \
	if (str.status() != QDataStream::Ok) {\
	if (len) *len = -1;\
	return QVariant::fromValue(PyError("Unable to interpret object", QString(), QString(), 0));\
	}

QVariant bytesToVariant(const QByteArray & ar, int * len)
{
	QDataStream str(ar);
	str.setByteOrder(QDataStream::LittleEndian);
	int code = 0;
	str >> code;

	_CHECK_ERROR();

	if (code == PY_CODE_INT) {
		int res = 0;
		str >> res;
		_CHECK_ERROR();
		if (len) *len = 4 + 4;
		return QVariant::fromValue(res);
	}
	else if (code == PY_CODE_LONG) {
		qint64 res = 0;
		str >> res;
		_CHECK_ERROR();
		if (len) *len = 4 + 8;
		return QVariant::fromValue(res);
	}
	else if (code == PY_CODE_DOUBLE) {
		double res = 0;
		str >> res;
		_CHECK_ERROR();
		if (len) *len = 4 + 8;
		return QVariant::fromValue(res);
	}
	else if (code == PY_CODE_COMPLEX) {
		double real = 0, imag=0;
		str >> real >> imag;
		_CHECK_ERROR();
		if (len) *len = 4 + 16;
		return QVariant::fromValue(complex_d(real,imag));
	}
	else if (code == PY_CODE_BYTES) {
		int l;
		str >> l;
		_CHECK_ERROR();
		QByteArray res(l, 0);
		str.readRawData(res.data(), l);
		_CHECK_ERROR();
		if (len) *len = 4 + 4 + l;
		return QVariant::fromValue(res);
	}
	else if (code == PY_CODE_STRING) {
		int l;
		str >> l;
		_CHECK_ERROR();
		QString res(l/2,0);
		str.readRawData((char*)res.data(), l);
		_CHECK_ERROR();
		if (len) *len = 4 + 4 + l;
		//swap bytes
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
		for (int i = 0; i < res.size(); ++i)
			res[i] = QChar(qFromLittleEndian(res[i].unicode()));
#endif
		return QVariant::fromValue(res);
	}

	else if (code == PY_CODE_LIST) {
		int count = 0;
		str >> count;
		_CHECK_ERROR();
		int start = 8;
		bool all_string = true;
		bool all_flat_array = true;
		QVariantList lst;
		for (int i = 0; i < count; ++i) {
			int l;
			QVariant v = bytesToVariant(ar.mid(start), &l);
			if (v.userType() == 0 || l < 0) {
				if (len) *len = -1;
				return QVariant::fromValue(PyError("Unable to interpret object of type 'list'", QString(), QString(), 0));
			}
			start += l;
			const VipNDArray tmp = v.value<VipNDArray>();
			all_string &= (v.userType() == QMetaType::QString);
			all_flat_array &= !tmp.isNull() && !tmp.isComplex() && tmp.shapeCount() == 1 && tmp.canConvert<double>();
			lst.append(v);
		}

		if (len) *len = start;
		//check if we can convert to QStringList
		if (all_string)
		{
			QStringList res;
			for (int i = 0; i < lst.size(); ++i) res.append(lst[i].toString());
			return QVariant::fromValue(res);
		}
		//check if we can convert to VipIntervalSampleVector
		else if (count == 2 && all_flat_array)
		{
			VipNDArrayType<double> values = lst[0].value<VipNDArray>();
			VipNDArrayType<double> intervals = lst[1].value<VipNDArray>();
			if (values.size() * 2 == intervals.size())
			{
				VipIntervalSampleVector res;
				for (int i = 0; i < values.size(); ++i)
					res.push_back(VipIntervalSample(values[i], intervals[i * 2], intervals[i * 2 + 1]));
				return QVariant::fromValue(res);
			}
		}
		return QVariant::fromValue(lst);
	}
	else if (code == PY_CODE_DICT) {
		int count = 0;
		str >> count;
		_CHECK_ERROR();
		int start = 8;
		QVariantMap map;
		for (int i = 0; i < count; ++i) {
			int l;
			QVariant key = bytesToVariant(ar.mid(start), &l);
			if (key.userType() == 0 || l < 0) {
				if (len) *len = -1;
				return QVariant::fromValue(PyError("Unable to interpret object of type 'dict'", QString(), QString(), 0));
			}
			start += l;
			QVariant value = bytesToVariant(ar.mid(start), &l);
			if (value.userType() == 0 || l < 0) {
				if (len) *len = -1;
				return QVariant::fromValue(PyError("Unable to interpret object of type 'dict'", QString(), QString(), 0));
			}
			start += l;
			map[key.toString()] = value;
		}

		if (len) *len = start;
		return QVariant::fromValue(map);
	}
	else if (code == PY_CODE_POINT_VECTOR) {
		int l = 0;
		QVariant tmp = bytesToVariant(ar.mid(4), &l);
		if (tmp.userType() != qMetaTypeId<VipNDArray>() || l < 0) {
			if (len) *len = -1;
			return QVariant::fromValue(PyError("Unable to interpret object of type 'point vector'", QString(), QString(), 0));
		}
		if (len) *len = l + 4;
		//tmp is a VipNDArray that can be converted to VipPointVector
		return QVariant::fromValue(tmp.value<VipPointVector>());
	}
	else if (code == PY_CODE_COMPLEX_POINT_VECTOR) {
		int l = 0;
		QVariant tmp = bytesToVariant(ar.mid(4), &l);
		if (tmp.userType() != qMetaTypeId<VipNDArray>() || l < 0) {
			if (len) *len = -1;
			return QVariant::fromValue(PyError("Unable to interpret object of type 'complex point vector'", QString(), QString(), 0));
		}
		if (len) *len = l + 4;
		//tmp is a VipNDArray that can be converted to VipComplexPointVector
		return QVariant::fromValue(tmp.value<VipComplexPointVector>());
	}
	else if (code == PY_CODE_INTERVAL_SAMPLE_VECTOR) {
		int l = 0;
		QVariant tmp = bytesToVariant(ar.mid(4), &l);
		if (tmp.userType() != qMetaTypeId<VipNDArray>() || l < 0) {
			if (len) *len = -1;
			return QVariant::fromValue(PyError("Unable to interpret object of type 'interval sample vector'", QString(), QString(), 0));
		}
		if (len) *len = l + 4;
		//at this point, tmp should already be converted to VipIntervalSampleVector by the PY_CODE_LIST decoder
		return tmp;
	}
	else if (code == PY_CODE_NDARRAY) {
		int l = 0;
		VipNDArray tmp = toNDArray(ar.mid(4), &l);
		if (l <= 0 || tmp.isNull()) {
			if (len) *len = -1;
			return QVariant::fromValue(PyError("Unable to interpret object of type ndarray", QString(), QString(), 0));
		}

		if (len) *len = 4 + l;

		/*if (vipIsImageArray(tmp))
			return QVariant::fromValue(tmp);

		//check if we can convert to VipPointVector
		if (!tmp.isComplex() && tmp.shapeCount() == 2 && tmp.shape(0) == 2 && tmp.canConvert<double>())
			return QVariant::fromValue(QVariant::fromValue(tmp).value<VipPointVector>());
		else if (tmp.isComplex() && tmp.shapeCount() == 2 && tmp.shape(0) == 2)
			return QVariant::fromValue(QVariant::fromValue(tmp).value<VipComplexPointVector>());
		else*/
			return QVariant::fromValue(tmp);
	}
	else if (code == PY_CODE_NONE)
	{
		if (len) *len = 4;
		return QVariant();
	}
	else if (code == PY_CODE_ERROR)
	{
		int l = 0;
		QString trace = bytesToVariant(ar.mid(4), &l).toString();
		if (len) *len = 4 + l;
		return QVariant::fromValue(PyError(trace, QString(), QString(), 0));
	}

	if (len)
		*len = -1;
	return QVariant::fromValue(PyError("Unable to interpret object", QString(), QString(), 0));
}


static void chunckedWrite(QIODevice * dev, const char* data, int size)
{
	/*dev->write(data ,size);
	dev->waitForBytesWritten(30000);
	return;*/

	int chunck_size = 2048 * 32;
	int chuncks = size / chunck_size;
	for (int i = 0; i < chuncks; ++i)
	{
		dev->write(data + i*chunck_size, chunck_size);
		dev->waitForBytesWritten(30000);
	}
	int end = chunck_size*chuncks;
	if (end != size)
	{
		dev->write(data + end, size - end);
		dev->waitForBytesWritten(30000);
	}
}


/*void test_serialize()
{
	VipNDArrayType<double> ar(vipVector(6)); ar = 1, 2, 3, 4, 5, 6;
	QVariantMap map; map["r1"] = 2; map["r2"] = QString("toto"); map["r3"] = QByteArray("tutu");
	VipPointVector pts = VipPointVector() << QPointF(0, 0) << QPointF(2, 2) << QPointF(3, 4);
	VipComplexPointVector cpts = VipComplexPointVector() << VipComplexPoint(0, complex_d(1, 2)) << VipComplexPoint(3, complex_d(4, 5));

	QByteArray r = variantToBytes(2);
	r += variantToBytes(3.4);
	r += variantToBytes(QString("toto"));
	r += variantToBytes(QByteArray("tutu"));
	r += variantToBytes(QVariant::fromValue(complex_d(3.2, 7.8)));
	r += variantToBytes(QVariant::fromValue(QVariantList() << 2 << QString("toto") << QByteArray("tutu")));
	r += variantToBytes(QVariant::fromValue(VipNDArray(ar)));
	r += variantToBytes(QVariant::fromValue(map));
	r += variantToBytes(QVariant::fromValue(pts));
	r += variantToBytes(QVariant::fromValue(cpts));

	int l = 0;
	int start = 0;

	int i = bytesToVariant(r.mid(start), &l).toInt();
	start += l;

	double d = bytesToVariant(r.mid(start), &l).toDouble();
	start += l;

	QString s = bytesToVariant(r.mid(start), &l).toString();
	start += l;
	vip_debug("'%s'\n", s.toLatin1().data());

	QByteArray b = bytesToVariant(r.mid(start), &l).toByteArray();
	start += l;
	vip_debug("'%s'\n", b.data());

	complex_d c = bytesToVariant(r.mid(start), &l).value<complex_d>();
	start += l;

	QVariantList lst = bytesToVariant(r.mid(start), &l).value<QVariantList>();
	start += l;
	for (int i = 0; i < lst.size(); ++i)
		vip_debug("%s\n", lst[i].toString().toLatin1().data());

	ar = bytesToVariant(r.mid(start), &l).value<VipNDArray>();
	start += l;
	vip_debug("shape count: %i\n", ar.shapeCount());
	vip_debug("shape 0: %i\n", ar.shape(0));
	for (int i = 0; i < ar.shape(0); ++i)
		vip_debug("%f\n", ar.value(vipVector(i)).toDouble());

	map = bytesToVariant(r.mid(start), &l).value<QVariantMap>();
	start += l;
	for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it)
		vip_debug("'%s' : '%s'\n", it.key().toLatin1().data(), it.value().toString().toLatin1().data());

	pts = bytesToVariant(r.mid(start), &l).value<VipPointVector>();
	start += l;
	for (int i = 0; i < pts.size(); ++i)
		vip_debug("%f : %f\n", (double)pts[i].x(), (double)pts[i].y());

	cpts = bytesToVariant(r.mid(start), &l).value<VipComplexPointVector>();
	start += l;
	for (int i = 0; i < cpts.size(); ++i)
		vip_debug("%f : %f, %f\n", (double)cpts[i].x(), (double)cpts[i].y().real(), (double)cpts[i].y().imag());
}
*/








struct PyProcRunnable;
struct PyProcRunThread : public QThread
{
	friend struct PyProcRunnable;

	struct RunResult {
		PyProcess::command_type c;
		QVariant res;
	};

	int state; // 0: not running, 1: running, 2: error
	QProcess *process;
	VipNetworkConnection * connection;
	PyProcess * local;
	QList<PyProcRunnable*> runnables;
	QList<RunResult> results;
	QMutex mutex;
	QWaitCondition cond;
	PyProcRunnable * current;
	int id;

	PyProcRunThread() :state(0),process(nullptr), local(nullptr), current(nullptr), id(1) {}


	PyProcess::command_type add(PyProcRunnable *r);
	void runOneLoop(PyProcess *);

	void setResult(PyProcess::command_type c, const QVariant & v)
	{
		//set the result for given command
		//the result list cannot exceed 20 elements (20 parallel python codes)
		RunResult res;
		res.c = c;
		res.res = v;
		QMutexLocker locker(&mutex);
		results.push_back(res);
		while (results.size() > 20)
			results.pop_front();
	}

	QVariant getResult(PyProcess::command_type c)
	{
		QMutexLocker locker(&mutex);
		for (QList<RunResult>::iterator it = results.begin(); it != results.end(); ++it)
		{
			if (it->c == c)
			{
				QVariant res = it->res;
				results.erase(it);
				return res;
			}
		}
		return QVariant();
	}

	int findIndex(PyProcess::command_type c);

	bool waitForRunnable(PyProcess::command_type c, unsigned long time = ULONG_MAX);

	void run()
	{
		if (PyProcess * tmp = local)
		{
			{
				QMutexLocker locker(&mutex);

				process = new QProcess();
				connection = new VipNetworkConnection();

				//For windows only, modify PATH in case of anaconda installation
				QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
				
#ifdef _WIN32
				// For windows, we must add some paths to PATH in case of anaconda install
				// First, we need the python path
				QProcess p;
				p.start("python -c \"import sys; print(sys.executable)\"");
				p.waitForStarted();
				p.waitForFinished();
				QByteArray ar = p.readAllStandardOutput();
				if (!ar.isEmpty()) {
					vip_debug("found Python at %s\n", ar.data());
					QString pdir = QFileInfo(QString(ar)).absolutePath();
					QStringList lst;
					lst << pdir + "/Library/bin" <<
						pdir + "/bin" <<
						pdir + "/condabin" <<
						pdir + "/Scripts";

					QString path = env.value("PATH");
					if (!path.endsWith(";"))
						path += ";";
					path = lst.join(";");
					env.insert("PATH", path);
				}
#endif

				process->setProcessEnvironment(env);

				//Change the process working directory as it might find Qt dlls belonging to Thermavip that will conflict with PyQt or PySide
				QString current = QDir::currentPath();
#ifdef _WIN32
				QDir::setCurrent(env.value("USERPROFILE"));
#else
				QDir::setCurrent(env.value("HOME"));
#endif
				QString pyfile = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python/pyprocess.py";
				process->start(tmp->interpreter() + "  " + pyfile, QIODevice::ReadWrite);
				if (!process->waitForStarted(5000) || process->state() != QProcess::Running)
				{
					QDir::setCurrent(current);
					VIP_LOG_ERROR("Error while launching Python process: " + process->errorString());
					state = 2;
					delete process;
					delete connection;
					process = nullptr;
					connection = nullptr;
					return;
				}
				//Reset working directory
				QDir::setCurrent(current);

				state = 1;
			}

			/*while (true)
			{
				connection->connectToHost("127.0.0.1", 5100);
				if (connection->waitForConnected(1000))
					break;
			}*/

			
			while (PyProcess * loc = local)
			{
				runOneLoop(loc);
				QThread::msleep(1);
				//exit loop if necessary
				if (state == 0 || process->state() != QProcess::Running)
					break;
			}

			if (!process->errorString().isEmpty())
			{
				vip_debug("%s\n", process->errorString().toLatin1().data());
			}
			QByteArray ar = process->readAllStandardOutput() + process->readAllStandardError();
			if (!ar.isEmpty())
			{
				vip_debug("%s\n", ar.data());
			}

			QMutexLocker locker(&mutex);

			state = 0;
			delete process;
			delete connection;
			process = nullptr;
			connection = nullptr;
			tmp->emitFinished();
		}
		
	}
};

struct PyProcRunnable
{
	enum Instruction
	{
		EXEC_CODE,
		EVAL_CODE,
		SEND_OBJECT,
		RETRIEVE_OBJECT,
		STOP
	};

	

	QString string;
	QVariant object;
	Instruction type;
	PyProcess::command_type id;
	PyProcRunThread * runThread;

	PyProcRunnable(PyProcRunThread * runThread)
		:type(EXEC_CODE), id(0), runThread(runThread) {}

	void run(PyProcess * local)
	{
		
		if (type == STOP)
		{
			//send the stop process code
			QByteArray stop_code(1, 'q');
			//local->connection()->write(stop_code);
			chunckedWrite(local->process(), stop_code.data(), stop_code.size());
			//local->process()->write(stop_code);
			//local->process()->waitForBytesWritten();
			local->process()->waitForFinished(2000);
			if (local->process()->state() == QProcess::Running)
				local->process()->kill();
			return;
		}
		else
		{
			if (type == EXEC_CODE)
			{
				//send code to execute
				QByteArray ar(1, 'e');
				QByteArray code = variantToBytes(string);
				ar += fromT(code.size()) + code;
				if (code.isEmpty()) {
					runThread->setResult(id, QVariant::fromValue(PyError("Error while serializing string")));
					return;
				}
				//local->connection()->write(ar);
				//local->connection()->waitForBytesWritten();
				chunckedWrite(local->process(), ar.data(), ar.size());
				//local->process()->write(ar);
				//local->process()->waitForBytesWritten();
			}
			else if (type == EVAL_CODE)
			{
				//send code to execute
				QByteArray ar(1, 'c');
				QByteArray code = variantToBytes(string);
				ar += fromT(code.size()) + code;
				if (code.isEmpty()) {
					runThread->setResult(id, QVariant::fromValue(PyError("Error while serializing string")));
					return;
				}
				//local->connection()->write(ar);
				//local->connection()->waitForBytesWritten();
				chunckedWrite(local->process(), ar.data(), ar.size());
				//local->process()->write(ar);
				//local->process()->waitForBytesWritten();
			}
			else if (type == SEND_OBJECT)
			{
				//send object
				//qint64 st = QDateTime::currentMSecsSinceEpoch();
				QByteArray ar(1, 'r');
				QByteArray key = variantToBytes(string);
				QByteArray value = variantToBytes(object);
				ar += fromT(key.size() + value.size()) + key + value;
				if (key.isEmpty()) {
					runThread->setResult(id, QVariant::fromValue(PyError("Error while serializing string")));
					return;
				}
				if (value.isEmpty()) {
					runThread->setResult(id, QVariant::fromValue(PyError("Error while serializing object of type " + QString(object.typeName()))));
					return;
				}
				//local->process()->write(ar);
				//local->process()->waitForBytesWritten();
				chunckedWrite(local->process(), ar.data(), ar.size());
				//local->connection()->write(ar);
				//local->connection()->waitForBytesWritten();
				
				//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
				//vip_debug("send object: %i\n", (int)el);
			}
			else
			{
				//send object name to retrieve
				QByteArray ar(1, 's');
				QByteArray key = variantToBytes(string);
				ar += fromT(key.size()) + key;
				if (key.isEmpty()) {
					runThread->setResult(id, QVariant::fromValue(PyError("Error while serializing string")));
					return;
				}
				//local->connection()->write(ar);
				//local->connection()->waitForBytesWritten();
				chunckedWrite(local->process(), ar.data(), ar.size());
				//local->process()->write(ar);
				//local->process()->waitForBytesWritten();
			}

			

			//wait for the result, an input request, or a stdout/stderr
			while (local->process()->state() == QProcess::Running && local->state() != 0) 
			{
				char code = 0;
				if (local->process()->bytesAvailable() > 0 || local->process()->waitForReadyRead(10))
				{
					QByteArray t = local->process()->read(1);
					if (t.size()) code = t[0];

					if (code == 'i') {
						//the code requested a user input
						//there should not be anymore input data
						if (local->process()->bytesAvailable() > 0)
						{
							//flush
							local->process()->readAll();
							continue;
						}
						local->__waitForInput();
					}
					else if (code == 'o') {
						//read stdout
						int s = readSize(local->process());
						QByteArray ar = local->process()->read(s);
						int len = 0;
						ar = bytesToVariant(ar,&len).toString().toLatin1();
						if(len > 0)
							local->__addStandardOutput(ar);
						else
							//flush
							local->process()->readAll();
					}
					else if (code == 'e') {
						//read stderr
						int s = readSize(local->process());
						QByteArray ar = local->process()->read(s);
						int len = 0;
						ar = bytesToVariant(ar,&len).toString().toLatin1();
						if (len > 0)
							local->__addStandardError(ar);
						else
							//flush
							local->process()->readAll();
					}
					else if (code == 'b' || code == 'x') {
						//read result
						int s = readSize(local->process());
						QByteArray ar = readData(local->process(), s);
						//QByteArray ar = local->process()->read(s);
						int len = 0;
						runThread->setResult(id, bytesToVariant(ar,&len));
						if(len < 0)
							//flush
							local->process()->readAll();
						break;
					}
					else {
						//flush
						local->process()->readAll();
						break;
					}
				}
			}
		}
		
	}
};


PyProcess::command_type PyProcRunThread::add(PyProcRunnable *r)
{
	//add the PyProcRunnable object and compute its ID
	QMutexLocker locker(&mutex);
	r->id = id;
	runnables << r;
	int res = id;
	++id;
	if (id == 20)
	{
		id = 1;
	}
	//remove possible result with this id
	for (int i = 0; i < results.size(); ++i)
		if ((int)results[i].c == id) {
			results.removeAt(i);
			break;
		}
	return res;
}

int PyProcRunThread::findIndex(PyProcess::command_type c)
{
	//find command index
	int index = -1;
	for (int i = 0; i < runnables.size(); ++i)
		if (runnables[i]->id == c)
		{
			index = i;
			break;
		}
	return index;
}

bool PyProcRunThread::waitForRunnable(PyProcess::command_type c, unsigned long time)
{
	QMutexLocker locker(&mutex);

	int index = findIndex(c);

	qint64 start = QDateTime::currentMSecsSinceEpoch();

	//wait for it
	while (index >= 0 || (current && current->id == c))
	{
		if (!local || !local->isRunning())
			return false;
		if (!cond.wait(&mutex, 5))
		{
			qint64 el = QDateTime::currentMSecsSinceEpoch() - start;
			if ((unsigned long)el >= time)
				return false;
		}
		index = findIndex(c);
	}

	return true;
}

void PyProcRunThread::runOneLoop(PyProcess * loc)
{
	while (runnables.size())
	{
		{
			QMutexLocker locker(&mutex);
			current = runnables.front();
			runnables.pop_front();
		}
		current->run(loc);
		PyProcRunnable * run = current;
		QMutexLocker locker(&mutex);
		current = nullptr;
		delete run;
		cond.wakeAll();
	}
}


class PyProcess::PrivateData
{
public:
	PrivateData(): out_mutex(QMutex::Recursive) {
		interpreter = "python";
	}

	PyProcRunThread runThread;
	QString interpreter;

	QByteArray input;
	QByteArray std_output;
	QByteArray std_error;
	QMutex line_mutex;
	QWaitCondition line_cond;

	QMutex out_mutex;
};


PyProcess::PyProcess(QObject * parent)
	:PyIOOperation(parent)
{
	d_data = new PrivateData();
}

PyProcess::PyProcess(const QString & pyprocess, QObject * parent )
	:PyIOOperation(parent)
{
	d_data = new PrivateData();
	d_data->interpreter = pyprocess;
}

void PyProcess::setInterpreter(const QString & name)
{
	d_data->interpreter = name;
}
QString PyProcess::interpreter() const
{
	return d_data->interpreter;
}

void PyProcess::startInteractiveInterpreter()
{
	this->execCode(
		"import sys\n"
		"def _prompt(text=''):\n"
		"  sys.stdout.write(text)\n"
		"  return sys.stdin.readline()\n"
		"\n"
		"import code;code.interact(None,_prompt,globals())"
	);
}

PyProcess::~PyProcess()
{
	stop();
	delete d_data;
}

bool PyProcess::start()
{
	if (!d_data->runThread.local)
	{
		d_data->runThread.state = 0;
		d_data->runThread.local = this;
		d_data->runThread.start();
		//wait for started
		while (!d_data->runThread.state)
			QThread::msleep(1);
		//check running state
		if (d_data->runThread.state != 1)
			return false;
		emitStarted();
	}

	//add the Python folder in Thermavip directory
	QString pypath = (QFileInfo(qApp->arguments()[0]).canonicalPath() + "/Python");
	wait(execCode(("import sys;sys.path.append('" + pypath + "');sys.path.append('./Python')")));
	wait(execCode("import Thermavip; Thermavip.setSharedMemoryName('" + pyGlobalSharedMemoryName() + "');"));

	return true;
}

void PyProcess::stop(bool wait)
{
	//stop process
	if (process() && process()->state() == QProcess::Running)
	{
		PyProcRunnable * r = new PyProcRunnable(&d_data->runThread);
		r->type = PyProcRunnable::STOP;
		d_data->runThread.add(r);
	}

	d_data->runThread.state = 0;
	
	//stop thread
	if (d_data->runThread.local)
	{
		d_data->runThread.local = nullptr;
		if(wait)
			d_data->runThread.QThread::wait();
	}
}



bool PyProcess::isRunning() const
{
	return process() && process()->state() == QProcess::Running && d_data->runThread.isRunning();
}

QThread * PyProcess::thread() const
{
	return &d_data->runThread;
}

QProcess * PyProcess::process() const
{
	return d_data->runThread.process;
}

VipNetworkConnection * PyProcess::connection() const
{
	return d_data->runThread.connection;
}

int PyProcess::state() const
{
	return d_data->runThread.state;
}

void PyProcess::__addStandardOutput(const QByteArray & data)
{
	QMutexLocker lock(&d_data->out_mutex);
	d_data->std_output += data;
	emitReadyReadStandardOutput();
}

void PyProcess::__addStandardError(const QByteArray & data)
{
	QMutexLocker lock(&d_data->out_mutex);
	d_data->std_error += data;
	emitReadyReadStandardError();
}

void PyProcess::__waitForInput()
{
	QMutexLocker lock(&d_data->line_mutex);
	d_data->input.clear();

	while (d_data->input.isEmpty() && process() && process()->state() == QProcess::Running && d_data->runThread.state != 0)
	{
		d_data->runThread.runOneLoop(this);
		d_data->line_cond.wait(&d_data->line_mutex, 1);
	}

	if (process() && process()->state() == QProcess::Running && d_data->runThread.state != 0 && !d_data->input.isEmpty())
	{
		//write user input
		QByteArray ar = variantToBytes(d_data->input);
		ar = 'i' + fromT(ar.size()) + ar;
		process()->write(ar);
		process()->waitForBytesWritten();
		d_data->input.clear();
	}
}

QByteArray PyProcess::readAllStandardOutput()
{
	QMutexLocker lock(&d_data->out_mutex);
	QByteArray out = d_data->std_output;
	d_data->std_output.clear();
	return out;
}

QByteArray PyProcess::readAllStandardError()
{
	QMutexLocker lock(&d_data->out_mutex);
	QByteArray err = d_data->std_error;
	d_data->std_error.clear();
	return err;
}

qint64 PyProcess::write(const QByteArray & data)
{
	QMutexLocker lock(&d_data->line_mutex);
	d_data->input = data;
	d_data->line_cond.wakeAll();
	return data.size();
}

QVariant PyProcess::evalCode(const QString & code, bool * ok)
{
	if (!isRunning())
		if (!start()) {
			if (ok) *ok = false;
			return QVariant::fromValue(PyError("Cannot start PyProcess"));
		}
	PyProcRunnable * r = new PyProcRunnable(&d_data->runThread);
	r->type = PyProcRunnable::EVAL_CODE;
	r->string = code;
	QVariant res = wait( d_data->runThread.add(r) );
	if (ok) 
		*ok = res.userType() != qMetaTypeId<PyError>();
	return res;
}

PyProcess::command_type PyProcess::execCode(const QString & code)
{
	PyProcRunnable * r = new PyProcRunnable(&d_data->runThread);
	r->type = PyProcRunnable::EXEC_CODE;
	r->string = code;
	return d_data->runThread.add(r);
}

PyProcess::command_type PyProcess::sendObject(const QString & name, const QVariant & obj)
{
	PyProcRunnable * r = new PyProcRunnable(&d_data->runThread);
	r->type = PyProcRunnable::SEND_OBJECT;
	r->string = name;
	r->object = obj;
	return d_data->runThread.add(r);
}

PyProcess::command_type PyProcess::retrieveObject(const QString & name)
{
	PyProcRunnable * r = new PyProcRunnable(&d_data->runThread);
	r->type = PyProcRunnable::RETRIEVE_OBJECT;
	r->string = name;
	return d_data->runThread.add(r);
}

QVariant PyProcess::wait(command_type command, int milli)
{
	bool res = d_data->runThread.waitForRunnable(command, milli > 0 ? (unsigned long)milli : ULONG_MAX);
	if (!res)
		return QVariant::fromValue(PyError(QString("Timeout")));
	return d_data->runThread.getResult(command);
}
