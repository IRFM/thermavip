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

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QTextStream>

#include "VipNDArray.h"
#include "VipNDArrayImage.h"

bool vipIsNullArray(const VipNDArray& ar) noexcept
{
	return vipIsNullArray(ar.handle());
}
bool vipIsNullArray(const VipNDArrayHandle* h) noexcept
{
	return !h || h->handleType() == VipNDArrayHandle::Null || h->dataType() == 0;
}

VipNDArrayHandle::VipNDArrayHandle() noexcept
  : size(0)
  , opaque(nullptr)
{
	// add_handle(this);
}

VipNDArrayShape VipNDArray::viewStart() const noexcept
{
	return handle()->handleType() == VipNDArrayHandle::View ? static_cast<const detail::ViewHandle*>(handle())->start : VipNDArrayShape(handle()->shape.size(), 0);
}

VipNDArray VipNDArray::makeView(const VipNDArray& other)
{
	if (other.isView())
		return other;
	if (other.handle()->handleType() != VipNDArrayHandle::Standard)
		return VipNDArray();
	detail::ViewHandle* h = new detail::ViewHandle(other.sharedHandle(), VipNDArrayShape(other.shapeCount(), 0), other.shape());
	return VipNDArray(SharedHandle(h));
}

VipNDArray VipNDArray::makeView(void* ptr, int data_type, const VipNDArrayShape& shape, const VipNDArrayShape& strides)
{
	detail::ViewHandle* h = new detail::ViewHandle(ptr, data_type, shape, strides);
	if (h->handle->handleType() == VipNDArrayHandle::Standard)
		return VipNDArray(SharedHandle(h));
	else
		delete h;
	return VipNDArray();
}

VipNDArray::VipNDArray() noexcept
  : VipNDArray(vipNullHandle())
{
}

VipNDArray::VipNDArray(VipNDArray&& other) noexcept
  : m_handle(std::move(other.sharedHandle()))
{
	other.setSharedHandle(vipNullHandle());
}

VipNDArray::VipNDArray(int data_type, const VipNDArrayShape& shape)
  : VipNDArray(vipCreateArrayHandle(VipNDArrayHandle::Standard, data_type, shape))
{
}

void VipNDArray::import(const void* ptr, int data_type, const VipNDArrayShape& shape)
{
	SharedHandle h = vipCreateArrayHandle(VipNDArrayHandle::Standard, data_type, const_cast<void*>(ptr), shape, vip_deleter_type());
	if (!h)
		return;

	this->setSharedHandle(h);
	this->detach(); // detach, allocate and copy input data
	h->opaque = nullptr;
}

VipNDArray::VipNDArray(const void* ptr, int data_type, const VipNDArrayShape& shape)
{
	import(ptr, data_type, shape);
}

bool VipNDArray::isUnstrided() const noexcept
{
	bool unstrided = false;
	vipShapeToSize(shape(), strides(), &unstrided);
	return unstrided;
}

bool VipNDArray::canConvert(int out_type) const noexcept
{
	if (handle()->canExport(out_type))
		return true;
	else {
		const SharedHandle h = detail::getHandle(VipNDArrayHandle::Standard, out_type);
		if (!vipIsNullArray(h.data()))
			return h->canImport(this->dataType());
		else
			return false;
	}
}

bool VipNDArray::fill(const QVariant& value)
{
	return handle()->fill(VipNDArrayShape(shapeCount(), 0), shape(), value);
}

VipNDArray VipNDArray::copy() const
{
	VipNDArray res(dataType(), shape());
	convert(res);
	return res;
}

bool VipNDArray::load(const char* filename, FileFormat format)
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
			qsizetype htype, dtype;
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
			if (!fin.open(QFile::ReadOnly | QFile::Text))
				return false;
		}
		return load(&fin, format);
	}
}

#include <qfileinfo.h>
#include <qimagereader.h>

bool VipNDArray::load(QIODevice* device, FileFormat format)
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
bool VipNDArray::save(const char* filename, FileFormat format)
{
	if (format == AutoDetect) {
		QString suffix = QFileInfo(QString(filename)).suffix();
		if (suffix.compare("png", Qt::CaseInsensitive) == 0 || suffix.compare("jpg", Qt::CaseInsensitive) == 0 || suffix.compare("jpeg", Qt::CaseInsensitive) == 0 ||
		    suffix.compare("bmp", Qt::CaseInsensitive) == 0 || suffix.compare("gif", Qt::CaseInsensitive) == 0 || suffix.compare("pbm", Qt::CaseInsensitive) == 0 ||
		    suffix.compare("pgm", Qt::CaseInsensitive) == 0 || suffix.compare("ppm", Qt::CaseInsensitive) == 0 || suffix.compare("xbm", Qt::CaseInsensitive) == 0 ||
		    suffix.compare("xpm", Qt::CaseInsensitive) == 0)
			format = Image;
		else if (suffix.compare("txt", Qt::CaseInsensitive) == 0 || suffix.compare("text", Qt::CaseInsensitive) == 0)
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
bool VipNDArray::save(QIODevice* device, FileFormat format)
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
	// try to avoid a copy if possible
	if (out_type == dataType()) {
		if (handle()->handleType() == VipNDArrayHandle::Standard)
			return *this;
		else if (isView()) {
			// check unstrided Standard view
			const detail::ViewHandle* h = static_cast<const detail::ViewHandle*>(handle());
			if (h->handle->handleType() == VipNDArrayHandle::Standard) {
				bool unstrided = false;
				vipShapeToSize(shape(), strides(), &unstrided);
				if (unstrided)
					return *this;
			}
		}
	}

	VipNDArray res(out_type, shape());

	if (handle()->canExport(out_type)) {
		if(handle()->exportData(VipNDArrayShape(shapeCount(), 0), shape(), res.handle(), VipNDArrayShape(shapeCount(), 0), shape()))
			return res;
	}
	else if (res.handle()->canImport(dataType())) {
		if(res.handle()->importData(VipNDArrayShape(shapeCount(), 0), shape(), handle(), viewStart(), shape()))
			return res;
	}
	return VipNDArray();
}

bool VipNDArray::import(const VipNDArray& other)
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

bool VipNDArray::convert(VipNDArray& other) const
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
	return convert(qMetaTypeId<qint8>());
}

VipNDArray VipNDArray::toUInt8() const
{
	return convert(qMetaTypeId<quint8>());
}

VipNDArray VipNDArray::toInt16() const
{
	return convert(qMetaTypeId<qint16>());
}

VipNDArray VipNDArray::toUInt16() const
{
	return convert(qMetaTypeId<quint16>());
}

VipNDArray VipNDArray::toInt32() const
{
	return convert(qMetaTypeId<qint32>());
}

VipNDArray VipNDArray::toUInt32() const
{
	return convert(qMetaTypeId<quint32>());
}

VipNDArray VipNDArray::toInt64() const
{
	return convert(qMetaTypeId<qint64>());
}

VipNDArray VipNDArray::toUInt64() const
{
	return convert(qMetaTypeId<quint64>());
}

VipNDArray VipNDArray::toFloat() const
{
	return convert(qMetaTypeId<float>());
}

VipNDArray VipNDArray::toDouble() const
{
	return convert(qMetaTypeId<double>());
}

VipNDArray VipNDArray::toComplexFloat() const
{
	return convert(qMetaTypeId<complex_f>());
}

VipNDArray VipNDArray::toComplexDouble() const
{
	return convert(qMetaTypeId<complex_d>());
}

VipNDArray VipNDArray::toString() const
{
	return convert(qMetaTypeId<QString>());
}

struct ToReal
{
	template<class T>
	double operator()(T val) const
	{
		return val.real();
	}
};
struct ToImag
{
	template<class T>
	double operator()(T val) const
	{
		return val.imag();
	}
};
struct ToAmplitude
{
	template<class T>
	double operator()(T val) const
	{
		return std::abs(val);
	}
};
struct ToArgument
{
	template<class T>
	double operator()(T val) const
	{
		return std::arg(val);
	}
};

VipNDArray VipNDArray::toReal() const
{
	if (dataType() == qMetaTypeId<complex_d>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		vipArrayTransform(ptr + vipFlatOffset<false>(strides(), viewStart()), shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToReal());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_f* ptr = static_cast<const complex_f*>(handle()->opaque);
		vipArrayTransform(ptr + vipFlatOffset<false>(strides(), viewStart()), shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToReal());
		return out;
	}
	else {
		return toDouble();
	}
}
VipNDArray VipNDArray::toImag() const
{
	if (dataType() == qMetaTypeId<complex_d>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView())
			ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToImag());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_f* ptr = static_cast<const complex_f*>(handle()->opaque);
		if (isView())
			ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToImag());
		return out;
	}
	else {
		VipNDArray out(QMetaType::Double, shape());
		out.fill(0);
		return out;
	}
}

VipNDArray VipNDArray::toArgument() const
{
	if (dataType() == qMetaTypeId<complex_d>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView())
			ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToArgument());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_f* ptr = static_cast<const complex_f*>(handle()->opaque);
		if (isView())
			ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToArgument());
		return out;
	}
	else {
		VipNDArray out(QMetaType::Double, shape());
		out.fill(0);
		return out;
	}
}
VipNDArray VipNDArray::toAmplitude() const
{
	if (dataType() == qMetaTypeId<complex_d>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView())
			ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToAmplitude());
		return out;
	}
	else if (dataType() == qMetaTypeId<complex_f>()) {
		VipNDArray out(QMetaType::Double, shape());
		const complex_d* ptr = static_cast<const complex_d*>(handle()->opaque);
		if (isView())
			ptr += vipFlatOffset<false>(strides(), viewStart());
		vipArrayTransform(ptr, shape(), strides(), static_cast<double*>(out.data()), out.shape(), out.strides(), ToAmplitude());
		return out;
	}
	else {
		return toDouble();
	}
}

VipNDArray VipNDArray::mid(const VipNDArrayShape& pos, const VipNDArrayShape& shape) const
{
	if (isEmpty())
		return VipNDArray();

	VipNDArrayShape _pos = pos;
	VipNDArrayShape _shape = shape;
	for (qsizetype i = 0; i < _pos.size(); ++i) {
		if (_pos[i] < 0)
			_pos[i] = 0;
		else if (_pos[i] >= this->shape(i))
			_pos[i] = this->shape(i) - 1;
	}
	for (qsizetype i = _pos.size(); i < shapeCount(); ++i)
		_pos.push_back(0);

	for (qsizetype i = 0; i < _shape.size(); ++i) {
		if (_shape[i] < 0)
			_shape[i] = 0;
		else if (_shape[i] + _pos[i] > this->shape(i))
			_shape[i] = this->shape(i) - _pos[i];
	}
	for (qsizetype i = _shape.size(); i < shapeCount(); ++i)
		_shape.push_back(this->shape(i) - _pos[i]);

	if (isView()) {
		// work on a new ViewHandle
		detail::ViewHandle* h = new detail::ViewHandle(*static_cast<const detail::ViewHandle*>(handle()), _pos, _shape);
		// h->start = h->start + _pos;
		// h->shape = _shape;
		return VipNDArray(SharedHandle(h));
	}
	else {
		detail::ViewHandle* h = new detail::ViewHandle(sharedHandle(), _pos, _shape);
		return VipNDArray(SharedHandle(h));
	}
}

void VipNDArray::swap(VipNDArray& other) noexcept
{
	qSwap(*this, other);
}

bool VipNDArray::reshape(const VipNDArrayShape& new_shape)
{
	if (isView() || !isUnstrided() || size() != vipShapeToSize(new_shape))
		return false;
	return handle()->reshape(new_shape);
}

bool VipNDArray::resize(VipNDArray& dst, Vip::InterpolationType type) const
{
	if (isEmpty() || dst.isEmpty())
		return false;

	if (shape() != dst.shape()) {
		return handle()->resize(VipNDArrayShape(shapeCount(), 0), shape(), dst.handle(), type, dst.viewStart(), dst.shape());
	}
	else
		return convert(dst);
}

VipNDArray VipNDArray::resize(const VipNDArrayShape& new_shape, Vip::InterpolationType type) const
{
	if (isEmpty())
		return VipNDArray();

	if (shape() == new_shape)
		return *this;

	VipNDArray res(dataType(), new_shape);
	handle()->resize(VipNDArrayShape(shapeCount(), 0), shape(), res.handle(), type, VipNDArrayShape(shapeCount(), 0), new_shape);
	return res;
}

bool VipNDArray::reset(const VipNDArrayShape& new_shape)
{
	// cannot reset if the data type is not already set
	if (dataType() == 0)
		return false;

	// just return true if the shape is the same
	if (shape() == new_shape)
		return true;
	if (isView())
		return false;

	int type = this->constHandle()->handleType();
	if (type == VipNDArrayHandle::Null) {
		type = VipNDArrayHandle::Standard;
		setSharedHandle(vipCreateArrayHandle(type, dataType(), new_shape));
		return constHandle()->handleType() != VipNDArrayHandle::Null;
	}
	else {
		if (!this->handle()->realloc(new_shape))
			return false;
	}

	return constHandle()->handleType() != VipNDArrayHandle::Null;
}

bool VipNDArray::reset(const VipNDArrayShape& new_shape, int data_type)
{
	// if (!isNull())
	{
		if (shape() == new_shape && dataType() == data_type)
			return true;
		if (isView())
			return false;
	}

	setSharedHandle(vipCreateArrayHandle(VipNDArrayHandle::Standard, data_type, new_shape));
	return constHandle()->handleType() != VipNDArrayHandle::Null;
}

void VipNDArray::clear() noexcept
{
	setSharedHandle(vipNullHandle());
}



#include "VipMath.h"
#include "VipNDArrayImage.h"

template<class T, class U>
void applyWarping(const T* src, U* dst, qsizetype w, qsizetype h, U background, const QPointF* warping)
{
	qsizetype i = 0;
	for (qsizetype y = 0; y < h; ++y) {
		for (qsizetype x = 0; x < w; ++x, ++i) {
			const QPointF src_pt = warping[i];
			if (!vipIsNan(src_pt.x())) {
				const qsizetype leftCellEdge = src_pt.x();
				qsizetype rightCellEdge = (src_pt.x()) + 1;
				if (rightCellEdge == w)
					rightCellEdge = leftCellEdge;
				const qsizetype topCellEdge = src_pt.y();
				qsizetype bottomCellEdge = (src_pt.y() + 1);
				if (bottomCellEdge == h)
					bottomCellEdge = topCellEdge;
				const T p1 = src[bottomCellEdge * w + leftCellEdge];
				const T p2 = src[topCellEdge * w + leftCellEdge];
				const T p3 = src[bottomCellEdge * w + rightCellEdge];
				const T p4 = src[topCellEdge * w + rightCellEdge];
				const double u = (src_pt.x() - leftCellEdge);	// / (rightCellEdge - leftCellEdge);
				const double v = (bottomCellEdge - src_pt.y()); // / (topCellEdge - bottomCellEdge);
				U value = (p1 * (1 - v) + p2 * v) * (1 - u) + (p3 * (1 - v) + p4 * v) * u;
				dst[i] = value;
			}
			else
				dst[i] = background;
		}
	}
}

inline void applyWarpingRGB(const QRgb* src, QRgb* dst, qsizetype w, qsizetype h, QRgb background, const QPointF* warping)
{
	qsizetype i = 0;
	for (qsizetype y = 0; y < h; ++y) {
		for (qsizetype x = 0; x < w; ++x, ++i) {
			const QPointF src_pt = warping[i];
			if (!vipIsNan(src_pt.x())) {
				const qsizetype leftCellEdge = src_pt.x();
				qsizetype rightCellEdge = (src_pt.x()) + 1;
				if (rightCellEdge == w)
					rightCellEdge = leftCellEdge;
				const qsizetype topCellEdge = src_pt.y();
				qsizetype bottomCellEdge = (src_pt.y() + 1);
				if (bottomCellEdge == h)
					bottomCellEdge = topCellEdge;
				const QRgb p1 = src[bottomCellEdge * w + leftCellEdge];
				const QRgb p2 = src[topCellEdge * w + leftCellEdge];
				const QRgb p3 = src[bottomCellEdge * w + rightCellEdge];
				const QRgb p4 = src[topCellEdge * w + rightCellEdge];
				const double u = (src_pt.x() - leftCellEdge);	/// (rightCellEdge - leftCellEdge);
				const double v = (bottomCellEdge - src_pt.y()); /// (topCellEdge - bottomCellEdge);

				int a = (qAlpha(p1) * (1 - v) + qAlpha(p2) * v) * (1 - u) + (qAlpha(p3) * (1 - v) + qAlpha(p4) * v) * u;
				int r = (qRed(p1) * (1 - v) + qRed(p2) * v) * (1 - u) + (qRed(p3) * (1 - v) + qRed(p4) * v) * u;
				int g = (qGreen(p1) * (1 - v) + qGreen(p2) * v) * (1 - u) + (qGreen(p3) * (1 - v) + qGreen(p4) * v) * u;
				int b = (qBlue(p1) * (1 - v) + qBlue(p2) * v) * (1 - u) + (qBlue(p3) * (1 - v) + qBlue(p4) * v) * u;

				dst[i] = qRgba(r, g, b, a);
			}
			else
				dst[i] = background;
		}
	}
}

bool vipApplyDeformation(const VipNDArray& in, const QPointF* deformation, VipNDArray& out, const QVariant& background)
{
	if (in.isEmpty())
		return false;

	if (in.canConvert<double>() && background.canConvert<double>()) {
		if (out.dataType() != QMetaType::Double || out.shape() != in.shape())
			out = VipNDArray(QMetaType::Double, in.shape());

		VipNDArray input = in.toDouble();
		applyWarping((double*)input.data(), (double*)out.data(), input.shape(1), input.shape(0), background.toDouble(), deformation);
		return true;
	}
	else if (in.canConvert<complex_d>() && background.canConvert<complex_d>()) {
		if (out.dataType() != qMetaTypeId<complex_d>() || out.shape() != in.shape())
			out = VipNDArray(qMetaTypeId<complex_d>(), in.shape());

		VipNDArray input = in.toComplexDouble();
		applyWarping((complex_d*)input.data(), (complex_d*)out.data(), input.shape(1), input.shape(0), background.value<complex_d>(), deformation);
		return true;
	}
	else if (vipIsImageArray(in) && (background.canConvert<QRgb>() || background.canConvert<QColor>() || background.canConvert<VipRGB>())) {
		QRgb back = 0;
		if (background.canConvert<QRgb>())
			back = background.toUInt();
		else if (background.canConvert<VipRGB>())
			back = background.value<VipRGB>();
		else
			back = background.value<QColor>().rgba();

		QImage imin = vipToImage(in);
		QImage imout = QImage(imin.width(), imin.height(), QImage::Format_ARGB32);

		QRgb* in_p = (QRgb*)imin.bits();
		QRgb* out_p = (QRgb*)imout.bits();

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
		type_to_level[QMetaType::Bool] = level++;
		type_to_level[QMetaType::UChar] = level++;
		type_to_level[QMetaType::SChar] = level++;
		type_to_level[QMetaType::Char] = level++;
		type_to_level[QMetaType::UShort] = level++;
		type_to_level[QMetaType::Short] = level++;
		type_to_level[QMetaType::UInt] = level++;
		type_to_level[QMetaType::Int] = level++;
		type_to_level[QMetaType::ULong] = level++;
		type_to_level[QMetaType::Long] = level++;
		type_to_level[QMetaType::ULongLong] = level++;
		type_to_level[QMetaType::LongLong] = level++;
		type_to_level[QMetaType::Float] = level++;
		type_to_level[QMetaType::Double] = level++;
		type_to_level[qMetaTypeId<long double>()] = level++;
		type_to_level[qMetaTypeId<complex_f>()] = level++;
		type_to_level[qMetaTypeId<complex_d>()] = level++;
	}

	if (t1 == t2)
		return t1;

	QMap<int, int>::const_iterator it1 = type_to_level.find(t1);
	if (it1 != type_to_level.end()) {
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

int vipHigherArrayType(const QVector<VipNDArray>& in)
{
	int dtype = 0;
	for (qsizetype i = 0; i < in.size(); ++i) {
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

static bool isUnder(int t1, int t2)
{
	return vipHigherArrayType(t1, t2) == t2;
}

int vipHigherArrayType(int dtype, const QList<int>& possible_types)
{
	// sort possible types
	QList<int> types = possible_types;
	std::sort(types.begin(), types.end(), isUnder);

	int res = 0;
	for (int i = 0; i < types.size(); ++i) {
		int tmp = vipHigherArrayType(types[i], dtype);
		if ((res == 0 && tmp != 0) || tmp != 0)
			res = types[i];
		if (res && (res == dtype || vipHigherArrayType(res, dtype) == res))
			break;
	}
	return res;
}

int vipHigherArrayType(const QVector<VipNDArray>& in, const QList<int>& possible_types)
{
	int dtype = vipHigherArrayType(in);
	if (dtype == 0)
		return 0;
	return vipHigherArrayType(dtype, possible_types);
}

bool vipConvertToHigherType(const QVector<VipNDArray>& in, QVector<VipNDArray>& out)
{
	int dtype = vipHigherArrayType(in);
	if (dtype == 0)
		return false;
	if (out.size() != in.size())
		out.resize(in.size());

	for (qsizetype i = 0; i < in.size(); ++i) {
		if (out[i].shape() == in[i].shape() && out[i].dataType() == dtype)
			in[i].convert(out[i]);
		else
			out[i] = in[i].convert(dtype);
	}
	return true;
}

bool vipConvertToType(const QVector<VipNDArray>& in, QVector<VipNDArray>& out, int dtype)
{
	if (out.size() != in.size())
		out.resize(in.size());
	for (qsizetype i = 0; i < in.size(); ++i) {
		if (out[i].shape() == in[i].shape() && out[i].dataType() == dtype)
			in[i].convert(out[i]);
		else
			out[i] = in[i].convert(dtype);
	}
	return true;
}

QVector<VipNDArray> vipConvertToHigherType(const QVector<VipNDArray>& in)
{
	QVector<VipNDArray> res;
	if (vipConvertToHigherType(in, res))
		return res;
	else
		return QVector<VipNDArray>();
}

bool vipConvertToHigherType(const QVector<VipNDArray>& in, QVector<VipNDArray>& out, const QList<int>& possible_types)
{
	int dtype = vipHigherArrayType(in, possible_types);
	if (dtype == 0)
		return false;

	if (out.size() != in.size())
		out.resize(in.size());

	for (qsizetype i = 0; i < in.size(); ++i) {
		if (out[i].shape() == in[i].shape() && out[i].dataType() == dtype)
			in[i].convert(out[i]);
		else
			out[i] = in[i].convert(dtype);
	}
	return true;
}

QVector<VipNDArray> vipConvertToHigherType(const QVector<VipNDArray>& in, const QList<int>& possible_types)
{
	QVector<VipNDArray> res;
	if (vipConvertToHigherType(in, res, possible_types))
		return res;
	else
		return QVector<VipNDArray>();
}
