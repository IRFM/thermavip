#include <QDataStream>
#include <QTextStream>
#include <QByteArray>
#include <QFile>


#include "VipNDArray.h"
#include "VipNDArrayImage.h"








//#include <qmutex.h>
// static QMutex mutex;
// static int count = 0;
// static double size_mb = 0;
// static QList<VipNDArrayHandle*> & _handles() {
// static QList<VipNDArrayHandle*> h;
// return h;
// }
// static void add_handle(VipNDArrayHandle * h)
// {
// QMutexLocker l(&mutex);
// ++count;
// _handles().append(h);
// }
// static void rm_handle(VipNDArrayHandle* h)
// {
// QMutexLocker l(&mutex);
// --count;
// _handles().removeOne(h);
// double s = 0;
// for (int i = 0; i < _handles().size(); ++i) {
// if(!dynamic_cast<detail::ViewHandle*>(_handles()[i]))
// s += _handles()[i]->dataSize() * _handles()[i]->size;
// }
// //printf("%i arrays, s= %f MB\n", count, (double)s / 1000000);
// }
//
// int vipNDArrayCount()
// {
// return -1;// count;
// }

bool vipIsNullArray(const VipNDArray & ar)
{
	return vipIsNullArray(ar.handle());
}
bool vipIsNullArray(const VipNDArrayHandle * h)
{
	return !h || h->handleType() == VipNDArrayHandle::Null || h->dataType() == 0;
}



VipNDArrayHandle::VipNDArrayHandle() : size(0), opaque(nullptr){
	//add_handle(this);

}

	//should destroy the underlying data only if own = true
VipNDArrayHandle::~VipNDArrayHandle() {

}

VipNDArrayBase::VipNDArrayBase()
{
}

VipNDArrayBase::~VipNDArrayBase()
{
}

VipNDArrayShape VipNDArrayBase::viewStart() const
{
	return handle()->handleType() == VipNDArrayHandle::View ? static_cast<const detail::ViewHandle*>(handle())->start : VipNDArrayShape(handle()->shape.size(), 0);
}



VipNDArray VipNDArray::makeView(const VipNDArray & other)
{
	if (other.isView())
		return other;
	if (other.handle()->handleType() != VipNDArrayHandle::Standard)
		return VipNDArray();
	detail::ViewHandle * h = new detail::ViewHandle(other.sharedHandle(), VipNDArrayShape(other.shapeCount(), 0), other.shape());
	return VipNDArray(SharedHandle(h));
}

VipNDArray VipNDArray::makeView(void * ptr, int data_type, const VipNDArrayShape & shape, const VipNDArrayShape & strides)
{
	detail::ViewHandle * h = new detail::ViewHandle(ptr, data_type, shape, strides);
	if (h->handle->handleType() == VipNDArrayHandle::Standard)
		return VipNDArray(SharedHandle(h));
	else
		delete h;
	return VipNDArray();
}


VipNDArray::VipNDArray()
:VipNDArrayBase(vipNullHandle())
{
}


VipNDArray::VipNDArray(int data_type, const VipNDArrayShape & shape)
:VipNDArrayBase(vipCreateArrayHandle(VipNDArrayHandle::Standard, data_type,shape))
{
}

void VipNDArray::import(const void * ptr, int data_type, const VipNDArrayShape & shape)
{
	SharedHandle h = vipCreateArrayHandle(VipNDArrayHandle::Standard, data_type,const_cast<void*>(ptr),shape,vip_deleter_type());
	if(!h)
		return;

	this->setSharedHandle(h);
	this->detach(); //detach, allocate and copy input data
	h->opaque = nullptr;
}

VipNDArray::VipNDArray(const void * ptr, int data_type, const VipNDArrayShape & shape)
{
	import(ptr,data_type,shape);
}

bool VipNDArray::isUnstrided() const
{
	bool unstrided = false;
	vipShapeToSize(shape(), strides(), &unstrided);
	return unstrided;
}

bool VipNDArray::canConvert(int out_type) const
{
	if(handle()->canExport(out_type))
		return true;
	else
	{
		const SharedHandle h = detail::getHandle(VipNDArrayHandle::Standard, out_type);
		if(!vipIsNullArray(h.data()))
			return h->canImport(this->dataType());
		else
			return false;
	}
}

bool VipNDArray::fill(const QVariant & value)
{
	return handle()->fill(VipNDArrayShape(shapeCount(),0), shape(),value);
}

VipNDArray VipNDArray::copy() const
{
	VipNDArray res(dataType(),shape());
	convert(res);
	return res;
}


bool VipNDArray::load(const char * filename, FileFormat format)
{
	QImage tmp;
	if (format == AutoDetect) {
		tmp = QImage(filename);
		if (!tmp.isNull())
			format = Image;
		else {
			QFile fin(filename);
			if (!fin.open(QFile::ReadOnly))
				return false;
			QDataStream str(&fin);
			int htype, dtype;
			str >> htype;
			str >> dtype;
			SharedHandle h = vipCreateArrayHandle(htype, dtype);
			if (h != vipNullHandle())
				format = Binary;
			else
				format = Text;
		}
	}

	if (format == Image) {
		if (tmp.isNull()) {
			tmp = QImage(filename);
			if (tmp.isNull())
				return false;
		}
		*this = vipToArray(tmp);
		return true;
	}
	else {
		QFile fin(filename);
		if (format == Binary) {
			if (!fin.open(QFile::ReadOnly))
				return false;
		}
		else {
			if (!fin.open(QFile::ReadOnly|QFile::Text))
				return false;
		}
		return load(&fin, format);
	}
}

#include <qimagereader.h>
#include <qfileinfo.h>

bool VipNDArray::load(QIODevice *device, FileFormat format)
{
	if (format == AutoDetect)
		format = Binary;

	if (format == Image) {
		QImageReader reader(device);
		QImage img;
		if (!reader.read(&img))
			return false;
		*this = vipToArray(img);
		return true;
	}
	else if (format == Text) {
		QTextStream str(device);
		VipNDArray ar;
		str >> ar;
		if (ar.isNull())
			return false;
		*this = ar;
		return true;
	}
	else {
		QDataStream str(device);
		VipNDArray ar;
		str >> ar;
		if (ar.isNull())
			return false;
		*this = ar;
		return true;
	}
}
bool VipNDArray::save(const char * filename, FileFormat format )
{
	if (format == AutoDetect) {
		QString suffix = QFileInfo(QString(filename)).suffix();
		if (suffix.compare("png", Qt::CaseInsensitive) == 0 ||
			suffix.compare("jpg", Qt::CaseInsensitive) == 0 ||
			suffix.compare("jpeg", Qt::CaseInsensitive) == 0 ||
			suffix.compare("bmp", Qt::CaseInsensitive) == 0 ||
			suffix.compare("gif", Qt::CaseInsensitive) == 0 ||
			suffix.compare("pbm", Qt::CaseInsensitive) == 0 ||
			suffix.compare("pgm", Qt::CaseInsensitive) == 0 ||
			suffix.compare("ppm", Qt::CaseInsensitive) == 0 ||
			suffix.compare("xbm", Qt::CaseInsensitive) == 0 ||
			suffix.compare("xpm", Qt::CaseInsensitive) == 0)
			format = Image;
		else if (suffix.compare("txt", Qt::CaseInsensitive) == 0 ||
			suffix.compare("text", Qt::CaseInsensitive) == 0)
			format = Text;
		else
			format = Binary;
	}
	QFile fin(filename);
	if (format == Text) {
		if (!fin.open(QFile::WriteOnly | QFile::Text))
			return false;
	}
	else {
		if (!fin.open(QFile::WriteOnly))
			return false;
	}
	return save(&fin, format);
}
bool VipNDArray::save(QIODevice * device, FileFormat format)
{
	if (format == AutoDetect)
		format = Binary;

	if (format == Image) {
		QImage tmp = vipToImage(*this);
		if (tmp.isNull())
			return false;
		return tmp.save(device);
	}
	else if (format == Text) {
		QTextStream str(device);
		str << *this;
		if (str.status() != QTextStream::Ok)
			return false;
		return true;
	}
	else {
		QDataStream str(device);
		str << *this;
		if (str.status() != QDataStream::Ok)
			return false;
		return true;
	}
}

VipNDArray VipNDArray::convert(int out_type) const
{
	//try to avoid a copy if possible
	if (out_type == dataType())
	{
		if(handle()->handleType() == VipNDArrayHandle::Standard)
			return *this;
		else if (isView())
		{
			//check unstrided Standard view
			const detail::ViewHandle * h = static_cast<const detail::ViewHandle*>(handle());
			if (h->handle->handleType() == VipNDArrayHandle::Standard)
			{
				bool unstrided = false;
				vipShapeToSize(shape(),strides(), &unstrided);
				if (unstrided)
					return *this;
			}
		}
	}

	VipNDArray res(out_type, shape());

	if(handle()->canExport(out_type))
	{
		handle()->exportData(VipNDArrayShape(shapeCount(), 0),shape(),res.handle(),VipNDArrayShape(shapeCount(),0), shape());
		return res;
	}
	else if(res.handle()->canImport(dataType()))
	{
		res.handle()->importData(VipNDArrayShape(shapeCount(),0), shape(), handle(), viewStart(), shape() );
		return res;
	}
	else
		return VipNDArray();
}

bool VipNDArray::import(const VipNDArray & other)
{
	if (isEmpty() || other.isEmpty())
		return false;

	if (handle()->canImport(other.dataType()))
		return handle()->importData(VipNDArrayShape(shapeCount(), 0), shape(), other.handle(), other.viewStart(), other.shape());
	else if (other.handle()->canExport(dataType()))
		return other.handle()->exportData(VipNDArrayShape(shapeCount(), 0), other.shape(), handle(), viewStart(), shape());
	else
		return false;
}


bool VipNDArray::convert(VipNDArray & other) const
{
	if (isEmpty() || other.isEmpty())
		return false;

	if (handle()->canExport(other.dataType()))
		return handle()->exportData(VipNDArrayShape(shapeCount(), 0), shape(), other.handle(), other.viewStart(), other.shape());
	else if (other.handle()->canImport(dataType()))
		return other.handle()->importData(VipNDArrayShape(shapeCount(), 0), other.shape(), handle(), viewStart(), shape());
	else
		return false;
}

VipNDArray VipNDArray::toInt8() const
{
	return convert(qMetaTypeId<qint8> ());
}

VipNDArray VipNDArray::toUInt8() const
{
	return convert(qMetaTypeId<quint8> ());
}

VipNDArray VipNDArray::toInt16() const
{
	return convert(qMetaTypeId<qint16> ());
}

VipNDArray VipNDArray::toUInt16() const
{
	return convert(qMetaTypeId<quint16> ());
}

VipNDArray VipNDArray::toInt32() const
{
	return convert(qMetaTypeId<qint32> ());
}

VipNDArray VipNDArray::toUInt32() const
{
	return convert(qMetaTypeId<quint32> ());
}

VipNDArray VipNDArray::toInt64() const
{
	return convert(qMetaTypeId<qint64> ());
}

VipNDArray VipNDArray::toUInt64() const
{
	return convert(qMetaTypeId<quint64> ());
}

VipNDArray VipNDArray::toFloat() const
{
	return convert(qMetaTypeId<float> ());
}

VipNDArray VipNDArray::toDouble() const
{
	return convert(qMetaTypeId<double> ());
}

VipNDArray VipNDArray::toComplexFloat() const
{
	return convert(qMetaTypeId<complex_f> ());
}

VipNDArray VipNDArray::toComplexDouble() const
{
	return convert(qMetaTypeId<complex_d> ());
}

VipNDArray VipNDArray::toString() const
{
	return convert(qMetaTypeId<QString> ());
}

struct ToReal {
	template< class T> double operator()(T val) const { return val.real(); }
};
struct ToImag {
	template< class T> double operator()(T val) const { return val.imag(); }
};
struct ToAmplitude {
	template< class T> double operator()(T val) const { return std::abs(val); }
};
struct ToArgument {
	template< class T> double operator()(T val) const { return std::arg(val); }
};

VipNDArray VipNDArray::toReal() const
{
	if (dataType() == qMetaTypeId<complex_d>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		vipArrayTransform(ptr + vipFlatOffset<false>(strides(), viewStart()), shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToReal());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_f* ptr = static_cast<const complex_f*>(handle()->opaque);
		vipArrayTransform(ptr + vipFlatOffset<false>(strides(), viewStart()), shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToReal());
		return out;
	}
	else
	{
		return toDouble();
	}
}
VipNDArray VipNDArray::toImag() const
{
	if (dataType() == qMetaTypeId<complex_d>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView()) ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToImag());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_f* ptr = static_cast<const complex_f*>(handle()->opaque);
		if (isView()) ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToImag());
		return out;
	}
	else
	{
		VipNDArray out(QMetaType::Double, shape());
		out.fill(0);
		return out;
	}
}

VipNDArray VipNDArray::toArgument() const
{
	if (dataType() == qMetaTypeId<complex_d>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView()) ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToArgument());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_f* ptr = static_cast<const complex_f*>(handle()->opaque);
		if (isView()) ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToArgument());
		return out;
	}
	else
	{
		VipNDArray out(QMetaType::Double, shape());
		out.fill(0);
		return out;
	}
}
VipNDArray VipNDArray::toAmplitude() const
{
	if (dataType() == qMetaTypeId<complex_d>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView()) ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToAmplitude());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>())
	{
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView()) ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(),
			static_cast<double*>(out.data()), out.shape(), out.strides(), ToAmplitude());
		return out;
	}
	else
	{
		return toDouble();
	}
}

VipNDArray VipNDArray::mid(const VipNDArrayShape & pos, const VipNDArrayShape & shape ) const
{
	if (isEmpty())
		return VipNDArray();

	VipNDArrayShape _pos = pos;
	VipNDArrayShape _shape = shape;
	for (int i = 0; i < _pos.size(); ++i)
	{
		if (_pos[i] < 0) _pos[i] = 0;
		else if (_pos[i] >= this->shape(i)) _pos[i] = this->shape(i) - 1;
	}
	for (int i = _pos.size(); i < shapeCount(); ++i)
		_pos.push_back(0);

	for (int i = 0; i < _shape.size(); ++i)
	{
		if (_shape[i] < 0) _shape[i] = 0;
		else if (_shape[i] + _pos[i] > this->shape(i) ) _shape[i] = this->shape(i) - _pos[i];
	}
	for (int i = _shape.size(); i < shapeCount(); ++i)
		_shape.push_back(this->shape(i) - _pos[i]);

	if (isView())
	{
		//work on a new ViewHandle
		detail::ViewHandle * h = new detail::ViewHandle(*static_cast<const detail::ViewHandle*>(handle()),_pos,_shape);
		//h->start = h->start + _pos;
		//h->shape = _shape;
		return VipNDArray(SharedHandle(h));
	}
	else
	{
		detail::ViewHandle * h = new detail::ViewHandle(sharedHandle(), _pos, _shape);
		return VipNDArray(SharedHandle(h));
	}
}


void VipNDArray::swap(VipNDArray & other)
{
	qSwap(*this,other);
}



bool VipNDArray::reshape(const VipNDArrayShape & new_shape)
{
	if (isView() || !isUnstrided() || size() != vipShapeToSize(new_shape))
		return false;
	return handle()->reshape(new_shape);
}

bool VipNDArray::resize(VipNDArray & dst, Vip::InterpolationType type) const
{
	if(isEmpty() || dst.isEmpty())
		return false;

	if (shape() != dst.shape())
	{
		return handle()->resize(VipNDArrayShape(shapeCount(), 0), shape(), dst.handle(), type,dst.viewStart(),dst.shape());
	}
	else
		return convert(dst);
}

VipNDArray VipNDArray::resize(const VipNDArrayShape & new_shape, Vip::InterpolationType type) const
{
	if(isEmpty())
		return VipNDArray();

	if(shape() == new_shape)
		return *this;

	VipNDArray res(dataType(),new_shape);
	handle()->resize(VipNDArrayShape(shapeCount(), 0), shape(), res.handle(),type, VipNDArrayShape(shapeCount(), 0), new_shape);
	return res;
}

bool VipNDArray::reset(const VipNDArrayShape & new_shape)
{
	//cannot reset if the data type is not already set
	if (dataType() == 0)
		return false;

	//just return true if the shape is the same
	if (shape() == new_shape)
		return true;
	if (isView())
		return false;

	int type = this->constHandle()->handleType();
	if (type == VipNDArrayHandle::Null)
	{
		type = VipNDArrayHandle::Standard;
		setSharedHandle(vipCreateArrayHandle(type, dataType(), new_shape));
		return constHandle()->handleType() != VipNDArrayHandle::Null;
	}
	else
	{
		if (!this->handle()->realloc(new_shape))
			return false;
	}

	return constHandle()->handleType() != VipNDArrayHandle::Null;
}

bool VipNDArray::reset(const VipNDArrayShape & new_shape, int data_type)
{
	//if (!isNull())
 {
		if (shape() == new_shape && dataType() == data_type)
			return true;
		if (isView())
			return false;
	}

	setSharedHandle(vipCreateArrayHandle(VipNDArrayHandle::Standard, data_type, new_shape));
	return constHandle()->handleType() != VipNDArrayHandle::Null;
}


void VipNDArray::clear()
{
	setSharedHandle(vipNullHandle());
}

VipNDArray & VipNDArray::operator=(const VipNDArray & other)
{
	setSharedHandle(other.sharedHandle());
	return *this;
}







#include "VipMath.h"
#include "VipNDArrayImage.h"

template< class T, class U>
void applyWarping(const T * src, U * dst, int w, int h, U background, const QPointF* warping)
{
	int i = 0;
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x, ++i)
		{
			const QPointF src_pt = warping[i];
			if (!vipIsNan(src_pt.x()))
			{
				const int leftCellEdge = src_pt.x();
				int rightCellEdge = (src_pt.x()) + 1;
				if (rightCellEdge == w)
					rightCellEdge = leftCellEdge;
				const int topCellEdge = src_pt.y();
				int bottomCellEdge = (src_pt.y() + 1);
				if (bottomCellEdge == h)
					bottomCellEdge = topCellEdge;
				const T p1 = src[bottomCellEdge*w + leftCellEdge];
				const T p2 = src[topCellEdge*w + leftCellEdge];
				const T p3 = src[bottomCellEdge*w + rightCellEdge];
				const T p4 = src[topCellEdge*w + rightCellEdge];
				const double u = (src_pt.x() - leftCellEdge);// / (rightCellEdge - leftCellEdge);
				const double v = (bottomCellEdge - src_pt.y());// / (topCellEdge - bottomCellEdge);
				U value = (p1*(1 - v) + p2*v)*(1 - u) + (p3*(1 - v) + p4*v)*u;
				dst[i] = value;
			}
			else
				dst[i] = background;

		}
	}
}

inline void applyWarpingRGB(const QRgb * src, QRgb * dst, int w, int h, QRgb background, const QPointF* warping)
{
	int i = 0;
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x, ++i)
		{
			const QPointF src_pt = warping[i];
			if (!vipIsNan(src_pt.x()))
			{
				const int leftCellEdge = src_pt.x();
				int rightCellEdge = (src_pt.x()) + 1;
				if (rightCellEdge == w)
					rightCellEdge = leftCellEdge;
				const int topCellEdge = src_pt.y();
				int bottomCellEdge = (src_pt.y() + 1);
				if (bottomCellEdge == h)
					bottomCellEdge = topCellEdge;
				const QRgb p1 = src[bottomCellEdge*w + leftCellEdge];
				const QRgb p2 = src[topCellEdge*w + leftCellEdge];
				const QRgb p3 = src[bottomCellEdge*w + rightCellEdge];
				const QRgb p4 = src[topCellEdge*w + rightCellEdge];
				const double u = (src_pt.x() - leftCellEdge);/// (rightCellEdge - leftCellEdge);
				const double v = (bottomCellEdge - src_pt.y());/// (topCellEdge - bottomCellEdge);


				int a = (qAlpha(p1) * (1 - v) + qAlpha(p2) * v)*(1 - u) + (qAlpha(p3) * (1 - v) + qAlpha(p4) * v)*u;
				int r = (qRed(p1) * (1 - v) + qRed(p2) * v)*(1 - u) + (qRed(p3) * (1 - v) + qRed(p4) * v)*u;
				int g = (qGreen(p1) * (1 - v) + qGreen(p2) * v)*(1 - u) + (qGreen(p3) * (1 - v) + qGreen(p4) * v)*u;
				int b = (qBlue(p1) * (1 - v) + qBlue(p2) * v)*(1 - u) + (qBlue(p3) * (1 - v) + qBlue(p4) * v)*u;

				dst[i] = qRgba(r, g, b, a);
			}
			else
				dst[i] = background;

		}
	}
}


bool vipApplyDeformation(const VipNDArray & in, const QPointF * deformation, VipNDArray & out, const QVariant & background)
{
	if (in.isEmpty())
		return false;

	if (in.canConvert<double>() && background.canConvert<double>())
	{
		if (out.dataType() != QMetaType::Double || out.shape() != in.shape())
			out = VipNDArray(QMetaType::Double, in.shape());

		VipNDArray input = in.toDouble();
		applyWarping((double*)input.data(), (double*)out.data(), input.shape(1), input.shape(0), background.toDouble(), deformation);
		return true;
	}
	else if (in.canConvert<complex_d>() && background.canConvert<complex_d>())
	{
		if (out.dataType() != qMetaTypeId<complex_d>() || out.shape() != in.shape())
			out = VipNDArray(qMetaTypeId<complex_d>(), in.shape());

		VipNDArray input = in.toComplexDouble();
		applyWarping((complex_d*)input.data(), (complex_d*)out.data(), input.shape(1), input.shape(0), background.value<complex_d>(), deformation);
		return true;
	}
	else if (vipIsImageArray(in) && (background.canConvert<QRgb>() || background.canConvert<QColor>() || background.canConvert<VipRGB>()))
	{
		QRgb back = 0;
		if (background.canConvert<QRgb>())
			back = background.toUInt();
		else if (background.canConvert<VipRGB>())
			back = background.value<VipRGB>();
		else
			back = background.value<QColor>().rgba();

		QImage imin = vipToImage(in);
		QImage imout = QImage(imin.width(), imin.height(), QImage::Format_ARGB32);

		QRgb * in_p = (QRgb*)imin.bits();
		QRgb * out_p = (QRgb*)imout.bits();

		applyWarpingRGB(in_p, out_p, in.shape(1), in.shape(0), back, deformation);

		out = vipToArray(imout);
		return true;
	}
	return false;
}



int vipHigherArrayType(int t1, int t2)
{

	static bool init = false;
	static QMap<int, int> type_to_level;
	if (!init) {
		init = true;
		int level = 0;
		type_to_level[QMetaType::Bool] = level++; type_to_level[QMetaType::UChar] = level++; type_to_level[QMetaType::SChar] = level++;
		type_to_level[QMetaType::Char] = level++; type_to_level[QMetaType::UShort] = level++; type_to_level[QMetaType::Short] = level++;
		type_to_level[QMetaType::UInt] = level++; type_to_level[QMetaType::Int] = level++; type_to_level[QMetaType::ULong] = level++;
		type_to_level[QMetaType::Long] = level++; type_to_level[QMetaType::ULongLong] = level++; type_to_level[QMetaType::LongLong] = level++;
		type_to_level[QMetaType::Float] = level++; type_to_level[QMetaType::Double] = level++; type_to_level[qMetaTypeId<long double>()] = level++;
		type_to_level[qMetaTypeId<complex_f>()] = level++; type_to_level[qMetaTypeId<complex_d>()] = level++;
	}

	if (t1 == t2)
		return t1;

	QMap<int, int>::const_iterator it1 = type_to_level.find(t1);
	if (it1 != type_to_level.end())
	{
		QMap<int, int>::const_iterator it2 = type_to_level.find(t2);
		if (it2 != type_to_level.end())
			return it1.value() > it2.value() ? t1 : t2;
	}

	bool t1_to_t2 = VipNDArray(t1, VipNDArrayShape()).canConvert(t2);
	bool t2_to_t1 = VipNDArray(t2, VipNDArrayShape()).canConvert(t1);
	if (!t1_to_t2 && !t2_to_t1) {
		if (t1 == qMetaTypeId<VipRGB>() || t2 == qMetaTypeId<VipRGB>())
			return qMetaTypeId<VipRGB>();
		return 0;
	}
	else if (t1_to_t2 && !t2_to_t1)
		return t2;
	else if (!t1_to_t2 && t2_to_t1)
		return t1;
	else
		return QMetaType(t1).sizeOf() > QMetaType(t2).sizeOf() ? t1 : t2;
}

int vipHigherArrayType(const QVector<VipNDArray> & in)
{
	int dtype = 0;
	for (int i = 0; i < in.size(); ++i)
	{
		if (dtype == 0)
			dtype = in[i].dataType();
		else {
			if (dtype == in[i].dataType())
				continue;
			else {
				if (!(dtype = vipHigherArrayType(dtype, in[i].dataType())))
					return false;
			}
		}
	}
	return dtype;
}


static bool isUnder(int t1, int t2) {
	return vipHigherArrayType(t1, t2) == t2;
}

int vipHigherArrayType(int dtype, const QList<int> & possible_types)
{
	//sort possible types
	QList<int> types = possible_types;
	std::sort(types.begin(), types.end(), isUnder);

	int res = 0;
	for (int i = 0; i < types.size(); ++i)
	{
		int tmp = vipHigherArrayType(types[i], dtype);
		if ((res == 0 && tmp != 0) || tmp != 0)
			res = types[i];
		if (res && (res == dtype || vipHigherArrayType(res, dtype) == res))
			break;
	}
	return res;
}

int vipHigherArrayType(const QVector<VipNDArray> & in, const QList<int> & possible_types)
{
	int dtype = vipHigherArrayType(in);
	if (dtype == 0)
		return 0;
	return vipHigherArrayType(dtype, possible_types);
}

bool vipConvertToHigherType(const QVector<VipNDArray> & in, QVector<VipNDArray> & out)
{
	int dtype = vipHigherArrayType(in);
	if (dtype == 0)
		return false;
	if (out.size() != in.size())
		out.resize(in.size());

	for (int i = 0; i < in.size(); ++i)
	{
		if (out[i].shape() == in[i].shape() && out[i].dataType() == dtype)
			in[i].convert(out[i]);
		else
			out[i] = in[i].convert(dtype);
	}
	return true;
}

bool vipConvertToType(const QVector<VipNDArray> & in, QVector<VipNDArray> & out, int dtype)
{
	if (out.size() != in.size())
		out.resize(in.size());
	for (int i = 0; i < in.size(); ++i)
	{
		if (out[i].shape() == in[i].shape() && out[i].dataType() == dtype)
			in[i].convert(out[i]);
		else
			out[i] = in[i].convert(dtype);
	}
	return true;
}

QVector<VipNDArray> vipConvertToHigherType(const QVector<VipNDArray> & in)
{
	QVector<VipNDArray> res;
	if (vipConvertToHigherType(in, res))
		return res;
	else
		return QVector<VipNDArray>();
}


bool vipConvertToHigherType(const QVector<VipNDArray> & in, QVector<VipNDArray> & out, const QList<int> & possible_types)
{
	int dtype = vipHigherArrayType(in, possible_types);
	if (dtype == 0)
		return false;

	if (out.size() != in.size())
		out.resize(in.size());

	for (int i = 0; i < in.size(); ++i)
	{
		if (out[i].shape() == in[i].shape() && out[i].dataType() == dtype)
			in[i].convert(out[i]);
		else
			out[i] = in[i].convert(dtype);
	}
	return true;
}

QVector<VipNDArray> vipConvertToHigherType(const QVector<VipNDArray> & in, const QList<int> & possible_types)
{
	QVector<VipNDArray> res;
	if (vipConvertToHigherType(in, res,possible_types))
		return res;
	else
		return QVector<VipNDArray>();
}




#ifdef _COMPILE_TEST

template< class T, class Fun>
void apply_fun2(VipNDArray & out, const VipNDArray & s1, const VipNDArray & s2, Fun fun)
{
#define BUFF_SIZE (1024)
	T * p1 = new T[BUFF_SIZE];
	T * p2 = new T[BUFF_SIZE];
	T * dst = (T*)out.constHandle()->dataPointer(VipNDArrayShape());

	int chunks = out.size() / BUFF_SIZE;
	int end = chunks * BUFF_SIZE;
	int rem = out.size() - end;

	for (int i = 0; i < end; i += BUFF_SIZE)
	{

		//copy s1 to p1
		if (s1.dataType() == QMetaType::Int) {
			std::copy((int*)s1.handle()->dataPointer(VipNDArrayShape()) + i, (int*)s1.handle()->dataPointer(VipNDArrayShape()) + i+ BUFF_SIZE,
				p1);
		}
		if (s2.dataType() == QMetaType::Int) {
			std::copy((int*)s2.handle()->dataPointer(VipNDArrayShape()) + i, (int*)s2.handle()->dataPointer(VipNDArrayShape()) + i + BUFF_SIZE,
				p2);
		}
		T * d = dst + i;
		for(int j=0; j < BUFF_SIZE; ++j)
			d[j] = fun(p1[j], p2[j]);

	}

	if (rem) {
		//copy s1 to p1
		if (s1.dataType() == QMetaType::Int) {
			std::copy((int*)s1.handle()->dataPointer(VipNDArrayShape()) + end, (int*)s1.handle()->dataPointer(VipNDArrayShape()) + out.size(),
				p1);
		}
		if (s2.dataType() == QMetaType::Int) {
			std::copy((int*)s2.handle()->dataPointer(VipNDArrayShape()) + end, (int*)s2.handle()->dataPointer(VipNDArrayShape()) + out.size(),
				p2);
		}
		T * d = dst + end;
		for (int j = 0; j < rem; ++j)
			d[j] = fun(p1[j], p2[j]);
	}

	delete[] p1;
	delete[] p2;
}


#include "VipEval.h"
#include "VipStack.h"
#include "VipNDArrayOperations.h"
#include "VipReduction.h"
#include "VipNDRect.h"


#include "VipRgb.h"
#include "VipSceneModel.h"
#include "VipConvolve.h"
#include "VipTransform.h"
#include "VipTranspose.h"
#include "VipAccumulate.h"
#include "VipFunction.h"

#include <stdlib.h>
#include <time.h>

int mul(int a,int b, int c) { return a * b*c; }
inline double intToDouble(const void * src) {return *((int*)src);};
inline double doubleToDouble(const void * src) {return *((double*)src);};

typedef double(*conv)(const void*);
conv getConvert(int src) {
	if(src == qMetaTypeId<double>()) return doubleToDouble;
	else return intToDouble;
}
int test(int argc, char**argv)
{

	//void * mem = malloc(500000000);
	//for(int MSIZE = 10; MSIZE < 5000; MSIZE += 500)
	// {
	// VipMemory pool;
	// //pool.reserve(100000000);
	// size_t count = 500000000 / MSIZE;
	// std::vector<void*> mems1(count);// pool.maxObjectsForSize(MSIZE + 1));
	// std::vector<void*> mems2(count);// pool.maxObjectsForSize(MSIZE + 1));
	// std::vector<int> sizes(mems1.size());
	// srand(time(nullptr));
	// for (int i = 0; i < mems1.size(); ++i)
	// sizes[i] = (rand() % MSIZE + 1);
//
//
	// printf("%i max objects: %i\n", MSIZE, count);
//
	// qint64 sta = QDateTime::currentMSecsSinceEpoch();
	// for (int i = 0; i < mems1.size(); ++i)
	// mems2[i] = pool.allocate(sizes[i]);
	// qint64 el = QDateTime::currentMSecsSinceEpoch() - sta;
	// printf("pool.allocate: %i, %i\n", (int)el, (std::size_t)mems2.back() >> 4);
	// //printf("max free: %i %i\n", (int)pool.nodes.mostRightNode->size, pool.nodes.maximum() ? (int)pool.nodes.maximum()->size : 0);
//
	// sta = QDateTime::currentMSecsSinceEpoch();
	// for (int i = 0; i < mems1.size(); ++i)
	// mems1[i] = malloc(sizes[i]);
	// el = QDateTime::currentMSecsSinceEpoch() - sta;
	// printf("malloc: %i\n", (int)el);
//
	// sta = QDateTime::currentMSecsSinceEpoch();
	// for (int i = 0; i < mems1.size()-1; ++i)
	// pool.deallocate(mems2[i]);
	// pool.deallocate(mems2[mems2.size()-1]);
	// pool.clear();
	// el = QDateTime::currentMSecsSinceEpoch() - sta;
	// printf("pool.deallocate: %i, %i\n", (int)el, (std::size_t)mems2.back());
	// //printf("max free: %i %i\n", (int)pool.nodes.mostRightNode->size, pool.nodes.maximum()?(int)pool.nodes.maximum()->size:0);
//
	// sta = QDateTime::currentMSecsSinceEpoch();
	// for (int i = 0; i < mems1.size(); ++i)
	// free(mems1[i]);
	// el = QDateTime::currentMSecsSinceEpoch() - sta;
	// printf("free: %i\n", (int)el);
//
	// }
//
	// QMultiMap<int, QString> map;
	// map.insert(2, QString());
	// map.insert(2, QString());
	// map.erase(map.find(2));
	// int yyyy = alignof(std::max_align_t);
	// int count = 10;

	int count = 100;
	{
		VipNDArrayType<int> e(vipVector(2, 3)), ee(vipVector(3, 2));
		e.convert(ee);
		VipNDArrayType<int> rev(vipVector(3, 3)), rev2(vipVector(3, 3)), rev3(vipVector(3, 3));
		for (int i = 0; i < 9; ++i) { rev[i] = i; rev2[i] = i; rev3[i] = i; }
		int vv = vipAccumulate(VipNDArray(rev), [](int a, int b) {return a + b; }, int(0));
		rev = vipFunction(rev, rev2,rev3,&mul);
		{
			QByteArray data;
			QTextStream str(&data, QIODevice::WriteOnly);
			str << rev;//vipStack(pp, pp2, 0);
			str.flush();
			printf("%s\n", data.data());fflush(stdout);
		}
		VipNDArray r2 = vipReverse<Vip::ReverseFlat>(VipNDArray(rev),0);
		{
			QByteArray data;
			QTextStream str(&data, QIODevice::WriteOnly);
			str << r2;//vipStack(pp, pp2, 0);
			str.flush();
			printf("%s\n", data.data());fflush(stdout);
		}

		{
			VipNDArrayType<double> out(vipVector(1000, 1000));
			VipNDArrayType<int> in1(vipVector(1000, 1000));
			VipNDArrayType<int> in2(vipVector(1000, 1000));

			qint64 sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				VipNDArrayType<double> out(vipVector(1000, 1000));
								VipNDArrayType<int> in1(vipVector(1000, 1000));
								VipNDArrayType<int> in2(vipVector(1000, 1000));
				apply_fun2<double>(out, VipNDArray(in1), VipNDArray(in2), [](int a, int b) {return a*b; });
			}
			qint64 el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("apply_fun2: %i, %f\n", (int)el, out[out.size() - 1]);fflush(stdout);

			sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				//out = (in1) * (in2);
				vipEval(out,in1);
			}
			el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("eval: %i, %f\n", (int)el, out[out.size() - 1]);fflush(stdout);


			sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				VipNDArrayType<double> out(vipVector(1000, 1000));
				VipNDArrayType<int> in1(vipVector(1000, 1000));
				VipNDArrayType<int> in2(vipVector(1000, 1000));
				int sizeOf = in1.dataSize();
				const uchar * data1 = (uchar*)in1.data();
				const uchar * data2 = (uchar*)in1.data();
				register const conv co = getConvert(in1.dataType());
				register const conv co2 = getConvert(in2.dataType());
				const int s =in1.size();
				double *p=out.ptr();
				for(int i=0; i < s; ++i) {
					p[i] = co(data1 + i *sizeOf) * co2(data2 + i *sizeOf);
				}
			}
			el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("mul fun ptr: %i, %f\n", (int)el, out[out.size() - 1]);fflush(stdout);

			sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				VipNDArrayType<double> out(vipVector(1000, 1000));
				VipNDArrayType<int> in1(vipVector(1000, 1000));
				VipNDArrayType<int> in2(vipVector(1000, 1000));
				out = VipNDArray(in1) * VipNDArray(in2);
			}
			el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("mul: %i, %f\n", (int)el, out[out.size() - 1]);fflush(stdout);


		}

		{

			VipNDArrayType<int> ar(vipVector(1000, 1000));
			static const int oo = iter_detail::StaticSize<decltype(ar.shape())>::value;

			for (int i = 0; i < count; ++i) {
				for (int y = 0; y < ar.shape(0); ++y)
					for (int x = 0; x < ar.shape(1); ++x)
						ar(y, x) = ar(y, x) * ar(y, x);
			}



			qint64 sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				for (int y = 0; y < ar.shape(0); ++y)
					for (int x = 0; x < ar.shape(1); ++x)
						ar(y, x) = ar(y, x) * ar(y, x);
			}
			qint64 el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("y,x %i ms, %i\n", (int)el, ar[ar.size() - 1]);fflush(stdout);

			 sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				vip_iter(Vip::FirstMajor, vipVector<2>(ar.shape()), pos)
					ar(pos) = ar(pos) * ar(pos);
			}
			 el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("vip_iter %i ms, %i\n", (int)el, ar[ar.size() - 1]);fflush(stdout);

			sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				for (int y = 0; y < ar.shape(0); ++y)
					for (int x = 0; x < ar.shape(1); ++x)
						ar(vipVector(y, x)) = ar(vipVector(y, x)) * ar(vipVector(y, x));
			}
			el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("vipVector %i ms, %i\n", (int)el, ar[ar.size() - 1]);fflush(stdout);

			sta = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				vip_iter(Vip::FirstMajor, vipVector<2>(ar.shape()), pos)
					ar(pos) = ar(pos) * ar(pos);
			}
			el = QDateTime::currentMSecsSinceEpoch() - sta;
			printf("vip_iter %i ms, %i\n", (int)el, ar[ar.size() - 1]);fflush(stdout);

		}


		QImage img("C:/Users/VM213788/Desktop/Thermavip.png");
		VipNDArray ar("C:/Users/VM213788/Desktop/Thermavip.png");
		ar = ar.resize(vipVector(500, 500), Vip::LinearInterpolation);
		VipNDArrayTypeView<VipRGB> rgb = ar;

		QTransform tr;
		tr.rotate(40);// .scale(1, 1.2);
		auto conv = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, VipRGB(0, 0, 0));

		VipNDArrayType<VipRGB> ar2(conv.shape());

		qint64 sta = QDateTime::currentMSecsSinceEpoch();
		for(int i=0; i < count; ++i)
			ar2 = conv;
		qint64 el = QDateTime::currentMSecsSinceEpoch() - sta;
		printf("vipTransform %i ms\n", (int)el);fflush(stdout);

		QImage img3;
		sta = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			img3 = img.transformed(tr);// , Qt::SmoothTransformation);
		el = QDateTime::currentMSecsSinceEpoch() - sta;
		printf("QImage::transform %i ms\n", (int)el);fflush(stdout);


		QImage img2(ar2.shape(1), ar2.shape(0), QImage::Format_ARGB32);
		memcpy(img2.bits(), ar2.ptr(), ar2.size() * sizeof(VipRGB));
		bool ok = img2.save("C:/Users/VM213788/Desktop/Thermavip2.png");

		VipNDArrayType<double> kernel(vipVector(3, 3));
		kernel.fill(1 / 9.);
		sta = QDateTime::currentMSecsSinceEpoch();
		VipNDArray tmp;
		for (int i = 0; i < count; ++i)
			tmp = vipConvolve<Vip::Nearest>(VipNDArray(ar2), kernel, vipVector(1, 1));
		el = QDateTime::currentMSecsSinceEpoch() - sta;
		printf("convolve %i ms\n", (int)el);fflush(stdout);

		memcpy(img2.bits(), tmp.data(), tmp.size() * sizeof(VipRGB));
		img2.save("C:/Users/VM213788/Desktop/Thermavip_mean.png");

		img3.save("C:/Users/VM213788/Desktop/Thermavip3.png");
		ok;
	}

	//vipArrayStats<double, Vip::AllStats>(VipNDArray());
	{
		VipNDArrayType<complex_d> arc(vipVector(3, 3));
		for (int i = 0; i < arc.size(); ++i)arc[i] = i;

		VipArrayStats<double, Vip::AllStats> st = vipArrayStats<double, Vip::AllStats>(arc);
		VipArrayStats<double, Vip::AllStats> st2 = vipArrayStats<double, Vip::AllStats>(VipNDArray(arc));
		VipArrayStats<complex_d, Vip::AllStats> st3 = vipArrayStats<complex_d, Vip::AllStats>(VipNDArray(arc));


		bool stop = true;
	}

	bool arg1 = detail::is_valid_functor<detail::FloorFun<int> >::value;
	bool arg2 = detail::is_valid_functor<detail::FloorFun<QString> >::value;
	bool arg3 = detail::is_valid_functor<detail::FloorFun<VipNDArray > >::value;
	VipNDArrayType<int> djd;
	djd.clear();
	djd.reset(vipVector(10, 10));

	int si = sizeof(detail::SharedDataPointer<int>);
	int si2 = sizeof(VipNDArray);
	int si3 = sizeof(SharedHandle);
	int si4 = sizeof(QString);
	int si5 = sizeof(QVariant);
	int si6 = sizeof(std::vector<int>);

	{
		QFile fin("C:/Users/VM213788/Desktop/2020-01-30_09.50.08_SCD_dir32.txt");
		fin.open(QFile::ReadOnly | QFile::Text);
		VipNDArrayType<int> in;
		QTextStream str(&fin);
		str >> in;

		VipNDArray ar;
		VipShape sh(QPolygonF()<<QPointF(240,150)<<QPointF(291,153)<< QPointF(300,300));
		VipShapeStatistics st;
		qint64 sta = QDateTime::currentMSecsSinceEpoch();
		for(int i=0; i < count; ++i)
			st = sh.statistics(in , QPoint(0, 0), &ar);
		qint64 el = QDateTime::currentMSecsSinceEpoch() - sta;
		printf("VipShape::statistics: %i ms %f %f %f %f %f\n", (int)el, st.min, st.max, st.average, st.std, (double)st.pixelCount);fflush(stdout);

		VipNDArrayType<std::complex<double> > arc(vipVector(3, 3));
		for (int i = 0; i < arc.size(); ++i)arc[i] = i;
		VipArrayStats<std::complex<double>, Vip::AllStats> stc = vipArrayStats<std::complex<double>, Vip::AllStats>(arc);

		VipArrayStats<double, Vip::AllStats> st2;
		sta = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			st2 = vipArrayStats<double, Vip::AllStats>(in, vipOverRects(sh.region()));
		el = QDateTime::currentMSecsSinceEpoch() - sta;
		printf("vipArrayStats: %i ms %f %f %f %f %f\n", (int)el, st2.min, st2.max, st2.mean, st2.std, (double)st2.count);fflush(stdout);

		VipArrayStats<double, Vip::AllStats> st3;
		sta = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			st3 = sh.imageStats<double, Vip::AllStats>(in,QPoint(250,20));
		el = QDateTime::currentMSecsSinceEpoch() - sta;
		printf("vipArrayStats: %i ms %f %f %f %f %f\n", (int)el, st3.min, st3.max, st3.mean, st3.std, (double)st3.count);fflush(stdout);
	}


	{
		VipNDArrayShape shh = vipVector(2, 2);
		VipNDArray img = vipToArray(QImage(10, 10, QImage::Format_ARGB32));
		int index = 0;
		for (int y = 0; y < 10; ++y)
			for (int x = 0; x < 10; ++x, ++index) img.setValue(vipVector(y, x), QVariant::fromValue(QColor::fromRgba(index)));
		VipNDArrayTypeView<VipRGB> rgb = img.mid(vipVector(1,1));
		VipRGB * r1 = rgb.ptr();
		VipNDArrayTypeView<VipRGB> rgb2 = rgb.mid(vipVector(1, 1));
		VipRGB * r2 = rgb2.ptr();

		VipNDArray img2(qMetaTypeId<VipRGB>(), vipVector(10, 10));
		bool bb1 = img.convert(img2);
		bool bb2 = img2.convert(img);

		VipRGB a(10, 10, 10);
		VipRGB b;
		QRgb c = a;
		QColor d = a;
		VipRgb<float> e = a;
		a = a + 2;
		a = a - 2;
		a = a*2;
		a = a / 2;
		a = a%2;
		a = a&&2;
		a = a || 2;
		a = a < 2;
		a = a > 2;
		a = a <= 2;
		a = a >= 2;
		bool h = a == b;
		bool h2 = a != b;
		a = a | 2;
		a = a&2;
		a = a^2;
		a = ~a;
		a = vipMin(a, (quint8)2);
		a = vipMax(a, (quint8)2);
		a = vipFloor(a);
		bool b1 = vipIsNan(a);
		bool b2 = vipIsInf(a);
		a = vipWhere(a, b, e);

		vipMin(VipNDArrayType<VipRGB>(), VipRGB(3,3,3));

	}

	{
		VipNDArrayType<int> a(vipVector(2, 2)), b(vipVector(2, 2)), c(vipVector(2, 2));
		a = a + c;

		VipNDArray va(QMetaType::Int, vipVector(2, 2));
		va = va + VipNDArray(c);

		VipNDArray a1(qMetaTypeId<complex_f>(), vipVector(3, 3));
		VipNDArray a2(qMetaTypeId<complex_f>(), vipVector(3, 3));
		VipNDArray a3(qMetaTypeId<complex_f>(), vipVector(3, 3));
		VipNDArray a4(qMetaTypeId<int>(), vipVector(3, 3));
		vipEval(a2, a2*a3);
		vipEval(a1, a2>a3);
		vipEval(a4, a2*a3);
	}

	auto r1 = vipRectStartEnd(vipVector(0, 1), vipVector(10, 11));
	{
		int w = r1.shape(1);
		int h = r1.shape(0);
		int l = r1.start(1);
		int r = r1.end(1);
		int t = r1.start(0);
		int b = r1.end(0);
		bool stop = true;
	}
	auto r2 = vipRectStartShape(vipVector(3, 3), vipVector(10, 10));
	auto r3 = r1.intersected(r2);
	auto r4 = r1.united(r2);
	auto r5 = vipRectStartShape(vipVector(3, 3), vipVector(-10, -10)).normalized();
	auto r6 = r1.isEmpty();
	auto r7 = r1.intersected(vipRectStartEnd(vipVector(20, 20), vipVector(30, 30)));
	auto r8 = r7.isEmpty();

	int ss = sizeof(QRect);
	int al = alignof(QRect);
	int al2 = alignof(VipNDRect<2>);
	QRect rr(QPoint(0, 0), QPoint(0, 0));
	int ww = rr.width();
	int lr = rr.right();
	bool ee = rr.isEmpty();

	VipNDArrayType<int> ar(vipVector(1000, 1000));
	VipNDArrayType<int> arv(vipVector(1000, 1000));
	VipNDArrayType<int> arv2(vipVector(1000, 1000));

	VipNDArrayType<int> ar2(vipVector(999, 999));
	VipNDArrayType<int> ar3(vipVector(999, 999));

	VipNDArrayType<int> pp(vipVector(3,3)), pp2(vipVector(2,3));
	pp = 1, 2, 3, 4, 5, 6,7,8,-1;
	pp2 = 7, 8, 9, 10, 11, 12;
	VipArrayStats<float, Vip::AllStats> stats = vipArrayStats<float, Vip::AllStats>(pp, vipOverRects(QRect(0,0,2,2)));
	vipEval(pp, vipWhere(pp > 2 && pp < 7, pp, pp * 3));
	QByteArray data;
	QTextStream str(&data, QIODevice::WriteOnly);
	str << pp;//vipStack(pp, pp2, 0);
	str.flush();
	printf("%s\n", data.data());fflush(stdout);

	{
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			arv = vipTranspose(VipNDArray(ar));
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		printf("transpose: %i\n", (int)el);fflush(stdout);
	}
	{
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			arv = vipReverse<Vip::ReverseFlat>(VipNDArray(ar));
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		printf("reverse flat: %i\n", (int)el);fflush(stdout);
	}
	{
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			arv = vipReverse<Vip::ReverseAxis>(VipNDArray(ar), 0);
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		printf("reverse axis 0: %i\n", (int)el);fflush(stdout);
	}
	{
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			arv = vipReverse<Vip::ReverseAxis>(VipNDArray(ar), 1);
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		printf("reverse axis 1: %i\n", (int)el);fflush(stdout);
	}

	VipNDArrayTypeView<int> view = ar.mid(vipVector(1, 1));
	VipNDArrayTypeView<int> view2 = ar.mid(vipVector(1, 1));
	VipNDArrayTypeView<int> view3 = ar3.mid(vipVector(2, 2));
	{
		VipNDArray tmp;
		VipNDArray tmp2 = tmp;
	}

	VipNDArrayType<int> tmp = arv * 3;

	auto f1 = vipFuzzyCompare(3,3);
	auto f2 = vipFuzzyCompare(3,3.2);
	auto f3 = vipFuzzyCompare(VipNDArray(ar), VipNDArray(ar));
	auto f4 = vipFuzzyCompare(ar,ar2);
	auto f5 = vipFuzzyCompare(ar, 3.4);


	qint64 st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		view.convert(ar2);
	qint64 el = QDateTime::currentMSecsSinceEpoch()-st;
	printf("view to dense: %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		view2.convert(view);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("view to view: %i\n", (int)el);fflush(stdout);

	ar3.detach();
	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		ar2.convert(ar3);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("dense to dense: %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		memcpy((void*)ar2.constData(), ar3.constData(), ar3.size()*ar3.dataSize());
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("dense to dense memcpy: %i\n", (int)el);fflush(stdout);


	ar.detach();

	auto type = vipWhere(ar > 2 && ar < 7, ar * 2, ar * ar);
	detail::InternalCast<int, decltype(type)>::valid;
	detail::InternalCast<int, decltype(type)>::cast(type);
	detail::internal_cast<int>(decltype(type)());
	detail::is_valid_functor<decltype(detail::internal_cast<int>(decltype(type)()))> jj ;
	//test(jj);
	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		vipEval(arv, vipWhere(ar > 2 && ar < 7, ar * 2, ar * ar));
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("eval where : %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		vipEval(arv, vipReplaceNanInf(ar,3));
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("eval replace nan inf : %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		vipEval(arv, vipClamp(ar, arv,3));
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("eval clamp : %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	{
		int * ptr1 = arv.ptr();
		int * ptr2 = ar.ptr();
		for (int i = 0; i < count; ++i) {
			size_t size = ar.size();
			for (int c = 0; c < size; ++c)
				ptr1[c] = ptr2[c] < ptr1[c] ? ptr1[c] : (ptr2[c] > 3 ? 3 : ptr2[c]);
		}
	}
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("eval clamp ptr : %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		vipEval(arv, ar*arv2 +arv2 * 3 );// *3);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("eval add : %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	{
		int * ptr1 = arv.ptr();
		int * ptr2 = ar.ptr();
		int * ptr3 = arv2.ptr();
//
		const VipNDArrayType<int> _ar = ar;
		const VipNDArrayType<int> _arv = arv;
		const VipNDArrayType<int> _arv2 = arv2;
//#pragma loop(no_vector)
		for (int i = 0; i < count; ++i) {
			size_t size = ar.size();
//#pragma loop(no_vector)
			for (int c = 0; c < (int)size; ++c)
				ptr1[c] =  (ptr2[c] * ptr3[c] + ptr2[c] * 3);
		}
	}
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("eval add ptr : %i\n", (int)el);fflush(stdout);



	st = QDateTime::currentMSecsSinceEpoch();
	{
		VipArrayStats<int, Vip::Min> stats;
		for (int i = 0; i < count; ++i)
			stats = vipArrayStats<int, Vip::Min >(ar);// , vipOverRects(vipRectStartEnd(vipVector(0, 0), ar.shape())));
	}
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("reduce stats : %i\n", (int)el);fflush(stdout);



	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		view3.resize(ar2);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("resize view to dense: %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		view3.resize(view2);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("resize view to view: %i\n", (int)el);fflush(stdout);

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		ar.resize(ar3,Vip::LinearInterpolation);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("resize dense to dense: %i\n", (int)el);fflush(stdout);

	QImage img(1000, 1000, QImage::Format_ARGB32);
	QImage img2(999, 999, QImage::Format_ARGB32);
	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		img2 = img.scaled(QSize(999, 999), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("resize img fast: %i\n", (int)el);fflush(stdout);




	VipNDArrayType<int> kernel(vipVector(3, 3));

	st = QDateTime::currentMSecsSinceEpoch();
	for (int i = 0; i < count; ++i) {
		vipEval(arv, vipConvolve<Vip::Nearest>(ar, kernel, vipVector(1, 1)));
	}
	el = QDateTime::currentMSecsSinceEpoch() - st;
	printf("conv test: %i\n", (int)el);fflush(stdout);


	fflush(stdout);

	getchar();
	return 0;
}

#endif //_COMPILE_TEST
