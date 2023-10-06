#include "VipCompressor.h"
#include "VipArchive.h"
#include "VipMultiNDArray.h"


#include <qbuffer.h>
#include <qdatastream.h>


/* #if defined (__GNUC__)
#include "zlib.h"
#define z_uLongf unsigned long
#else
#include "QtZlib/zlib.h"
#endif
*/
#define UNCOMPRESS_ASSERT(classname, value, error ) \
if(!(value)) { \
	setError((QString(classname) + ":" + QString(error))); \
	return QVariant(); \
}




/* bool vipIsUncompressed(const uchar* data, int size)
{
	char tmp[40];
	z_uLongf len = 40;
	int err = uncompress((uchar*)tmp, &len, data, size);
	if (err == Z_OK || err == Z_BUF_ERROR)
		return false;
	return true;
}

bool vipIsQtUncompressed(const QByteArray & ar)
{
	if (ar.size() < 4)
		return true;

	return vipIsUncompressed((uchar*)(ar.data() + 4), ar.size() - 4);
}*/




VipCompressor::VipCompressor(QObject * parent)
	:VipProcessingObject(parent)
{
	this->propertyAt(0)->setData(1);
}

VipCompressor::~VipCompressor()
{
	wait();
}

void VipCompressor::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
	{
		setError("empty input data", VipProcessingObject::WrongInput);
		return;
	}

	if (mode() == Compress)
	{
		QVariant data = any.data();
		any.setData(QVariant());

		QByteArray compressed_data = this->compressVariant(data);
		if (hasError())
			return;

		qint32 size = compressed_data.size();
		compressed_data = QByteArray((char*)(&size), sizeof(size)) + compressed_data;

		QBuffer buffer(&compressed_data);
		buffer.open(QBuffer::WriteOnly);// | QBuffer::Append);
		buffer.seek(buffer.size());



		VipBinaryArchive arch(&buffer);
		//write the VipAnyData without any data
		arch << any;

		VipAnyData out = create(QVariant(compressed_data));
		outputAt(0)->setData(out);
	}
	else
	{
		if (any.data().userType() != QMetaType::QByteArray)
		{
			setError("input data is not a byte array (uncompress)", VipProcessingObject::WrongInput);
			return;
		}

		QByteArray data = any.value<QByteArray>();
		any.setData(QVariant());

		QBuffer buffer(&data);
		buffer.open(QBuffer::ReadOnly);
		qint32 size = 0;
		buffer.read((char*)(&size), sizeof(size));

		QByteArray raw = buffer.read(size);

		bool need_more = false;
		QVariant v = this->uncompressVariant(raw, need_more);

		if (hasError() || need_more)
			return;

		VipBinaryArchive arch(&buffer);
		arch >> any;
		any.setData(v);
		outputAt(0)->setData(any);
	}
}





QByteArray VipGzipCompressor::compressVariant(const QVariant & value)
{
	QByteArray res;
	qint32 id = 0;
	int level = propertyAt(1)->value<int>();
	QBuffer buffer(&res);
	buffer.open(QBuffer::WriteOnly);

	if (value.userType() == QMetaType::QString)
	{
		const QString str = value.toString();
		id = QMetaType::QString;
		buffer.write((char*)(&id), sizeof(id));
		res = qCompress((const uchar*)str.data(), str.size(), level);
	}
	else if (value.userType() == QMetaType::QByteArray)
	{
		id = QMetaType::QByteArray;
		buffer.write((char*)(&id), sizeof(id));
		res = qCompress(value.toByteArray(), level);
	}
	else if (value.userType() == qMetaTypeId<complex_f>())
	{
		id = qMetaTypeId<complex_f>();
		buffer.write((char*)(&id), sizeof(id));
		complex_f tmp = value.value<complex_f>();
		res = qCompress((const uchar*)(&tmp),sizeof(complex_f), level);
	}
	else if (value.userType() == qMetaTypeId<complex_d>())
	{
		id = qMetaTypeId<complex_d>();
		buffer.write((char*)(&id), sizeof(id));
		complex_d tmp = value.value<complex_d>();
		res = qCompress((const uchar*)(&tmp), sizeof(complex_d), level);
	}
	else if (value.userType() == qMetaTypeId<VipIntervalSample>())
	{
		id = qMetaTypeId<VipIntervalSample>();
		buffer.write((char*)(&id), sizeof(id));
		VipIntervalSample tmp = value.value<VipIntervalSample>();
		res = qCompress((const uchar*)(&tmp), sizeof(VipIntervalSample), level);
	}
	else if (value.userType() == qMetaTypeId<QPointF>())
	{
		id = qMetaTypeId<QPointF>();
		buffer.write((char*)(&id), sizeof(id));
		QPointF tmp = value.value<QPointF>();
		res = qCompress((const uchar*)(&tmp), sizeof(QPointF), level);
	}
	else if (value.userType() == qMetaTypeId<QPoint>())
	{
		id = qMetaTypeId<QPoint>();
		buffer.write((char*)(&id), sizeof(id));
		QPoint tmp = value.value<QPoint>();
		res = qCompress((const uchar*)(&tmp), sizeof(QPoint), level);
	}
	else if (value.userType() == qMetaTypeId<VipPointVector>())
	{
		id = qMetaTypeId<VipPointVector>();
		buffer.write((char*)(&id), sizeof(id));
		const VipPointVector tmp = value.value<VipPointVector>();
		res = qCompress((const uchar*)(tmp.data()), tmp.size() * sizeof(QPointF), level);
	}
	else if (value.userType() == qMetaTypeId<VipIntervalSampleVector>())
	{
		id = qMetaTypeId<VipPointVector>();
		buffer.write((char*)(&id), sizeof(id));
		const VipIntervalSampleVector tmp = value.value<VipIntervalSampleVector>();
		res = qCompress((const uchar*)(tmp.data()), tmp.size() * sizeof(VipIntervalSample), level);
	}
	else if (value.userType() == qMetaTypeId<VipNDArray>())
	{
		id = qMetaTypeId<VipNDArray>();
		buffer.write((char*)(&id), sizeof(id));
		const VipNDArray array = value.value<VipNDArray>();
		QMap<QString, VipNDArray> arrays;

		if (vipIsMultiNDArray(array))
			arrays = VipMultiNDArray(array).namedArrays();
		else
			arrays[QString()] = array;

		qint32 count = arrays.size();

		//write the number of arrays
		buffer.write((char*)(&count), sizeof(count));

		for (QMap<QString, VipNDArray>::const_iterator it = arrays.begin(); it != arrays.end(); ++it)
		{
			//save the name
			qint32 size = it.key().size();
			buffer.write((char*)(&size), sizeof(size));
			buffer.write(it.key().toLatin1().data(), size);

			//save the array in a temporary byte array
			QByteArray raw_array;
			{
				QDataStream stream(&raw_array, QIODevice::WriteOnly);
				stream << it.value();
			}

			QByteArray compressed = qCompress(raw_array, level);

			size = compressed.size();
			buffer.write((char*)(&size), sizeof(size));
			buffer.write(compressed);
		}
	}
	else if (value.canConvert<double>())
	{
		id = QMetaType::Double;
		buffer.write((char*)(&id), sizeof(id));
		double tmp = value.toDouble();
		res = qCompress((const uchar*)(&tmp),  sizeof(double), level);
	}
	else if(value.userType() != 0)
	{
		setError("unsupported type (" + QString(value.typeName()) + ")", VipProcessingObject::WrongInput);
	}

	return res;
}


QVariant VipGzipCompressor::uncompressVariant(const QByteArray & raw_data, bool & need_more)
{
	need_more = false;

	if (raw_data.isEmpty())
		return QVariant();

	if (raw_data.size() < 4)
	{
		setError("unable to uncompress input data");
		return QVariant();
	}

	QBuffer buffer(const_cast<QByteArray*>(&raw_data));
	buffer.open(QBuffer::ReadOnly);

	qint32 id = 0;
	buffer.read((char*)(&id), sizeof(id));


	if (id == QMetaType::QString)
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		int size = tmp.size() / 3;
		return QVariant(QString((const QChar*)(tmp.data()), size));
	}
	else if (id == QMetaType::QByteArray)
	{
		return QVariant(qUncompress(buffer.readAll()));
	}
	else if (id == qMetaTypeId<complex_f>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(complex_f), "wrong input format (decompression)");
		complex_f res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<complex_d>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(complex_d), "wrong input format (decompression)");
		complex_d res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == QMetaType::Double)
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(double), "wrong input format (decompression)");
		double res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<QPointF>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(QPointF), "wrong input format (decompression)");
		QPointF res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<QPoint>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(QPoint), "wrong input format (decompression)");
		QPoint res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<VipIntervalSample>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(VipIntervalSample), "wrong input format (decompression)");
		VipIntervalSample res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<VipIntervalSample>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		UNCOMPRESS_ASSERT("VipGzipCompressor", tmp.size() >= (int)sizeof(VipIntervalSample), "wrong input format (decompression)");
		VipIntervalSample res;
		memcpy(&res, tmp.data(), sizeof(res));
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<VipPointVector>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		int size = tmp.size() / sizeof(QPointF);
		VipPointVector res(size);
		std::copy((const QPointF*)(tmp.data()), (const QPointF*)(tmp.data()) + size, res.begin());
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<VipIntervalSampleVector>())
	{
		QByteArray tmp = qUncompress(buffer.readAll());
		int size = tmp.size() / sizeof(VipIntervalSample);
		VipIntervalSampleVector res(size);
		std::copy((const VipIntervalSample*)(tmp.data()), (const VipIntervalSample*)(tmp.data()) + size, res.begin());
		return QVariant::fromValue(res);
	}
	else if (id == qMetaTypeId<VipNDArray>())
	{
		qint32 count = 0;
		buffer.read((char*)(&count), sizeof(count));

		QMap<QString, VipNDArray> arrays;

		for (int i = 0; i < count; ++i)
		{
			qint32 size = 0;
			buffer.read((char*)(&size), sizeof(size));
			QString name = buffer.read(size);

			size = 0;
			buffer.read((char*)(&size), sizeof(size));
			QByteArray raw = qUncompress(buffer.read(size));

			VipNDArray ar;
			{
				QDataStream stream(raw);
				stream >> ar;
			}
			arrays[name] = ar;
		}

		if (arrays.size() == 1 && arrays.begin().key().isEmpty())
			return QVariant::fromValue(arrays.begin().value());
		else
		{
			VipMultiNDArray res;
			res.setNamedArrays(arrays);
			return QVariant::fromValue(VipNDArray(res));
		}
	}
	else if (id == 0)
	{
		return QVariant();
	}
	else
	{
		setError("unknown input type (uncompress)");
		return QVariant();
	}
}
