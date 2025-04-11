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

#include "VipPlotRasterData.h"
#include "VipAxisColorMap.h"
#include "VipMultiNDArray.h"
#include "VipNDArray.h"
#include "VipNDArrayImage.h"
#include "VipPainter.h"
#include "VipSliderGrip.h"

#include <limits>

#include <qapplication.h>
#include <qthread.h>

template<class T>
inline bool is_valid(T)
{
	return true;
}
template<>
inline bool is_valid(float value)
{
	return !vipIsNan(value) && !std::isinf(value);
}
template<>
inline bool is_valid(double value)
{
	return !vipIsNan(value) && !std::isinf(value);
}
template<>
inline bool is_valid(long double value)
{
	return !vipIsNan(value) && !std::isinf(value);
}

template<class T, bool is_signed = std::is_signed<T>::value>
struct CastValue
{
	static T cast(vip_double value)
	{
		if (value > std::numeric_limits<T>::max())
			return std::numeric_limits<T>::max();
		if (value < -std::numeric_limits<T>::max())
			return -std::numeric_limits<T>::max();
		else
			return (T)value;
	}
};
template<class T>
struct CastValue<T, false>
{
	static T cast(vip_double value)
	{
		if (value > std::numeric_limits<T>::max())
			return std::numeric_limits<T>::max();
		else if (value < 0)
			return (T)0;
		else
			return (T)value;
	}
};

template<class T>
inline T cast(vip_double value)
{
	return CastValue<T>::cast(value);
}

/*#include "immintrin.h"

// Add missing defines with msvc
#ifdef _MSC_VER

#ifdef __AVX2__
// AVX2
#elif defined(__AVX__)
// AVX
#elif (defined(_M_AMD64) || defined(_M_X64))
// SSE2 x64
#define __SSE2__
#elif _M_IX86_FP == 2
// SSE2 x32
#define __SSE2__
#elif _M_IX86_FP == 1
// SSE x32
#define __SSE__
#else
// nothing
#endif

#endif
*/

template<class T>
VipInterval computeBounds(const T* ptr, int size, const VipInterval& interval)
{
	const T* data = ptr;
	const T* end = ptr + size;

	if (interval == Vip::InfinitInterval) {

		T min = 0;
		T max = 0;

		for (; data != end; ++data) {
			if (is_valid(*data)) {
				min = max = *data;
				break;
			}
		}

		for (; data != end; ++data) {
			if (is_valid(*data)) {
				if (*data < min)
					min = *data;
				else if (*data > max)
					max = *data;
			}
		}

		return VipInterval(min, max);
	}
	else {
		double min = 0;
		double max = 0;
		// get the first value
		for (; data != end; ++data) {
			if (is_valid(*data)) {
				if (interval.contains(*data)) {
					min = max = *data;
					break;
				}
			}
		}

		T i_min = cast<T>(qMin(interval.minValue(), interval.maxValue()));
		T i_max = cast<T>(qMax(interval.minValue(), interval.maxValue()));

		if (interval.borderFlags() == VipInterval::IncludeBorders) {
			for (; data != end; ++data) {
				if (is_valid(*data) && *data >= i_min && *data <= i_max) {
					if (*data < min)
						min = *data;
					else if (*data > max)
						max = *data;
				}
			}
		}
		else if (interval.borderFlags() == VipInterval::ExcludeBorders) {
			for (; data != end; ++data) {
				if (is_valid(*data) && *data > i_min && *data < i_max) {
					if (*data < min)
						min = *data;
					else if (*data > max)
						max = *data;
				}
			}
		}
		else if (interval.borderFlags() & VipInterval::ExcludeMinimum) {
			for (; data != end; ++data) {
				if (is_valid(*data) && *data > i_min && *data <= i_max) {
					if (*data < min)
						min = *data;
					else if (*data > max)
						max = *data;
				}
			}
		}
		else if (interval.borderFlags() & VipInterval::ExcludeMaximum) {
			for (; data != end; ++data) {
				if (is_valid(*data) && *data >= i_min && *data < i_max) {
					if (*data < min)
						min = *data;
					else if (*data > max)
						max = *data;
				}
			}
		}
		return VipInterval(min, max);
	}
}

static VipInterval vipArrayMinMax(const void* ptr, int data_type, int size, const VipInterval& interval)
{
	switch (data_type) {
		case QMetaType::Bool:
			return computeBounds((const unsigned char*)ptr, size, interval);
		case QMetaType::Char:
			return computeBounds((const char*)ptr, size, interval);
		case QMetaType::UChar:
			return computeBounds((const unsigned char*)ptr, size, interval);
		case QMetaType::SChar:
			return computeBounds((const char*)ptr, size, interval);
		case QMetaType::UShort:
			return computeBounds((const quint16*)ptr, size, interval);
		case QMetaType::Short:
			return computeBounds((const qint16*)ptr, size, interval);
		case QMetaType::UInt:
			return computeBounds((const quint32*)ptr, size, interval);
		case QMetaType::Int:
			return computeBounds((const qint32*)ptr, size, interval);
		case QMetaType::ULong:
			return computeBounds((const unsigned long*)ptr, size, interval);
		case QMetaType::Long:
			return computeBounds((const long*)ptr, size, interval);
		case QMetaType::ULongLong:
			return computeBounds((const quint64*)ptr, size, interval);
		case QMetaType::LongLong:
			return computeBounds((const qint64*)ptr, size, interval);
		case QMetaType::Float:
			return computeBounds((const float*)ptr, size, interval);
		case QMetaType::Double:
			return computeBounds((const double*)ptr, size, interval);
		default: {
			if (data_type == qMetaTypeId<long double>())
				return computeBounds((const long double*)ptr, size, interval);
			VipNDArray ar = VipNDArray::makeView(ptr, data_type, vipVector(size)).convert<double>();
			if (ar.isEmpty())
				return VipInterval();
			return computeBounds((const double*)ar.constData(), ar.size(), interval);
		}
	}
}

class ArrayConverter : public VipRasterConverter
{
	VipNDArray array;
	QPointF position;
	VipInterval minmax;
	VipInterval validity;

public:
	const VipNDArray& getArray() const { return array; }
	const QPointF& getPosition() const { return position; }

	void setArray(const VipNDArray& ar)
	{
		if (vipIsMultiNDArray(ar)) {
			const VipMultiNDArray tmp(ar);
			array = tmp.array(tmp.currentArray());
		}
		else
			array = ar;
		minmax = VipInterval();
		validity = VipInterval();
	}

	void setPosition(const QPointF& pos) { position = pos; }

	virtual QRectF boundingRect() const
	{
		if (array.isEmpty() || array.shapeCount() != 2)
			return QRectF(position, QSizeF(0,0));
		else
			return QRectF(position, QSizeF(array.shape(1), array.shape(0)));
	}

	virtual int dataType() const
	{
		if (!array.isNull())
			return array.dataType();
		else
			return 0;
	}

	virtual void extract(const QRectF& rect, VipNDArray* out_array, QRectF* out_rect) const
	{
		if (array.isEmpty()) {
			if (out_rect)
				*out_rect = QRectF();
			return;
		}

		if (position == QPointF(0, 0) && rect.contains(QRectF(0, 0, array.shape(1), array.shape(0)))) {
			if (out_rect)
				*out_rect = boundingRect();
			*out_array = array;
			return;
		}

		// this is the rect in image coordinates
		QRectF im_rect = rect.translated(-position);
		// we want a QRect that bounds given image rect
		im_rect.setLeft(floor(im_rect.left()));
		im_rect.setTop(floor(im_rect.top()));
		im_rect.setRight(ceil(im_rect.right()));
		im_rect.setBottom(ceil(im_rect.bottom()));
		// bounds image rect to array shape
		if (im_rect.left() < 0)
			im_rect.setLeft(0);
		if (im_rect.top() < 0)
			im_rect.setTop(0);
		if (im_rect.right() > array.shape(1))
			im_rect.setRight(array.shape(1));
		if (im_rect.bottom() > array.shape(0))
			im_rect.setBottom(array.shape(0));

		if (out_rect)
			*out_rect = im_rect.translated(position);

		if (!im_rect.isValid())
			return;

		VipNDArrayShape shape = vipVector((int)im_rect.height(), (int)im_rect.width());
		if (out_array->isNull() || out_array->shape() != shape || out_array->dataType() != array.dataType())
			*out_array = VipNDArray(array.dataType(), shape);
		if (out_array->canConvert<QImage>())
			out_array->fill(QColor(Qt::transparent));

		array.mid(vipVector((int)im_rect.top(), (int)im_rect.left()), vipVector((int)im_rect.height(), (int)im_rect.width())).convert(*out_array);
	}

	virtual QVariant pick(const QPointF& pos) const
	{
		QPoint array_pos = (pos - position).toPoint();
		if (array.shapeCount() == 2 && array_pos.x() >= 0 && array_pos.y() >= 0 && array_pos.x() < array.shape(1) && array_pos.y() < array.shape(0))
			return array(vipVector(array_pos.y(), array_pos.x()));
		return QVariant();
	}

	virtual VipInterval bounds(const VipInterval& interval) const
	{
		if (!minmax.isValid() || interval != validity) {
			const_cast<ArrayConverter*>(this)->minmax = computeInternalBounds(interval);
			const_cast<ArrayConverter*>(this)->validity = interval;
		}
		return minmax;
	}

	VipInterval computeInternalBounds(const VipInterval& interval) const
	{
		if (array.isEmpty())
			return VipInterval();

		int size = array.size();
		const void* ptr = array.constData();
		return vipArrayMinMax(ptr, array.dataType(), size, interval);
	}

	// virtual bool applyColorMap(const VipColorMap * map, const VipInterval & interval, const QRect & src, QImage & out, int num_threads) const
	// {
	// if (out.format() != QImage::Format_ARGB32 && out.format() != QImage::Format_ARGB32_Premultiplied)
	// return false;
	// if (out.isNull())
	// return false;
	// if (array.isEmpty())
	// return false;
	//
	// QRect src_rect = src & QRect(0, 0, array.shape(1), array.shape(0));
	// if (src_rect.isEmpty())
	// return false;
	//
	// switch (array.dataType())
	// {
	// case QMetaType::UChar: return ::applyColorMap(map, interval, src_rect, array.shape(1), (quint8*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::Char: return ::applyColorMap(map, interval, src_rect, array.shape(1), (qint8*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::SChar: return ::applyColorMap(map, interval, src_rect, array.shape(1), (qint8*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::UShort: return ::applyColorMap(map, interval, src_rect, array.shape(1), (quint16*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::Short: return ::applyColorMap(map, interval, src_rect, array.shape(1), (qint16*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::UInt: return ::applyColorMap(map, interval, src_rect, array.shape(1), (quint32*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::Int: return ::applyColorMap(map, interval, src_rect, array.shape(1), (qint32*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::ULong: return ::applyColorMap(map, interval, src_rect, array.shape(1), (unsigned long*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::Long: return ::applyColorMap(map, interval, src_rect, array.shape(1), (long*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::ULongLong: return ::applyColorMap(map, interval, src_rect, array.shape(1), (quint64*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::LongLong: return ::applyColorMap(map, interval, src_rect, array.shape(1), (qint64*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::Double: return ::applyColorMap(map, interval, src_rect, array.shape(1), (double*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// case QMetaType::Float: return ::applyColorMap(map, interval, src_rect, array.shape(1), (float*)array.constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads); break;
	// default:
	// {
	// if(array.canConvert<double>())
	//	return ::applyColorMap(map, interval, src_rect, array.shape(1), (double*)array.toDouble().constData(), (QRgb*)out.bits(), out.width(), out.height(), num_threads);
	// break;
	// }
	// }
	// return false;
	// }
};

class VipRasterData::PrivateData
{
public:
	PrivateData()
	  : converter(nullptr)
	  , mTime(QDateTime::currentMSecsSinceEpoch())
	  , isArray(false)
	  , deleteConverter(true)
	{
	}

	virtual ~PrivateData() noexcept
	{
		if (converter && deleteConverter)
			delete converter;
	}

	VipRasterConverter* converter;
	VipNDArray array;
	QRectF rect;
	QRectF outRect;
	qint64 mTime;
	bool isArray;
	bool deleteConverter;
};

template<class Converter>
class VipRasterDataEmbedConverter
	: public VipRasterData::PrivateData
{
public:
	Converter c;
	VipRasterDataEmbedConverter()
	  : VipRasterData::PrivateData()
	{
		this->converter = &c;
		this->deleteConverter = false;
	}
};


VipRasterData::VipRasterData() {}

VipRasterData::VipRasterData(const VipNDArray& ar, const QPointF& p)
  : d_data(new VipRasterDataEmbedConverter<ArrayConverter>())
{
	static_cast<ArrayConverter*>(d_data->converter)->setArray(ar);
	static_cast<ArrayConverter*>(d_data->converter)->setPosition(p);
	d_data->isArray = true;
}

VipRasterData::VipRasterData(const QImage& image, const QPointF& p)
  : d_data(new VipRasterDataEmbedConverter<ArrayConverter>())
{
	static_cast<ArrayConverter*>(d_data->converter)->setArray(vipToArray(image));
	static_cast<ArrayConverter*>(d_data->converter)->setPosition(p);
	d_data->isArray = true;
}

VipRasterData::VipRasterData(const QPixmap& pixmap, const QPointF& p)
  : d_data(new VipRasterDataEmbedConverter<ArrayConverter>())
{
	static_cast<ArrayConverter*>(d_data->converter)->setArray(vipToArray(pixmap.toImage()));
	static_cast<ArrayConverter*>(d_data->converter)->setPosition(p);
	d_data->isArray = true;
}

VipRasterData::VipRasterData(VipRasterConverter* converter)
  : d_data(new PrivateData())
{
	d_data->converter = converter;
}

qint64 VipRasterData::modifiedTime() const
{
	return d_data ? d_data->mTime : 0;
}

int VipRasterData::dataType() const
{
	return d_data ? d_data->converter->dataType() : 0;
}

bool VipRasterData::isArray() const
{
	return d_data ? d_data->isArray : false;
}

QRectF VipRasterData::boundingRect() const
{
	return d_data ? d_data->converter->boundingRect() : QRectF();
}

VipInterval VipRasterData::bounds(const VipInterval& interval) const
{
	return d_data ? d_data->converter->bounds(interval) : VipInterval();
}

VipNDArray VipRasterData::extract(const QRectF& rect, QRectF* out_rect) const
{
	if (!d_data || !rect.isValid()) {
		if (out_rect)
			*out_rect = QRectF();
		return VipNDArray();
	}

	if (rect != d_data->rect) {
		QRectF temp;
		if (!out_rect)
			out_rect = &temp;

		d_data->converter->extract(rect, &d_data->array, out_rect);
		d_data->rect = rect;
		d_data->outRect = *out_rect;
	}
	else if (out_rect) {
		*out_rect = d_data->outRect;
	}

	if (!d_data->outRect.isValid())
		return VipNDArray();

	return d_data->array;
}

QVariant VipRasterData::pick(const QPointF& pos) const
{
	return d_data ? d_data->converter->pick(pos) : QVariant();
}

static VipNDArray convertToArray(const VipRasterData& data)
{
	return const_cast<VipRasterData&>(data).extract(data.boundingRect());
}

static int registerConverter()
{
	QMetaType::registerConverter<VipRasterData, VipNDArray>(convertToArray);
	return 0;
}
static int _registerConverter = registerConverter();

/// VipImageData gather drawing information required by a VipPlotRasterData object.
/// It contains:
/// <ul>
/// <li> The image to draw as a QImage</li>
/// <li> The input array rectangle that this image represents in the VipRasterData</li>
/// <li> The destination polygon where the image should be drawn.</li>
/// </ul>
class VIP_PLOTTING_EXPORT VipImageData
{
public:
	VipImageData(const QImage& img = QImage(), const QRectF& array_rect = QRectF(), const QPolygonF& dst = QPolygonF());

	void setImage(const QImage& img);
	void setArrayRect(const QRectF&);
	void setSrcImageRect(const QRectF&);
	void setDstPolygon(const QPolygonF&);

	const QImage& image() const;
	const QImage& constImage() const;
	QImage& image();
	const QRectF& arrayRect() const;
	const QRectF& srcImageRect() const;
	const QPolygonF& dstPolygon() const;
	bool isEmpty() const;

private:
	class PrivateData : public QSharedData
	{
	public:
		PrivateData();
		PrivateData(const PrivateData& other);
		QImage image;
		QRectF array_rect;
		QRectF srcImageRect;
		QPolygonF dst;
	};

	QSharedDataPointer<PrivateData> d_data;
};

VipImageData::VipImageData(const QImage& img, const QRectF& array_rect, const QPolygonF& dst)
  : d_data(new PrivateData())
{
	d_data->image = img;
	d_data->array_rect = array_rect;
	d_data->dst = dst;
}

void VipImageData::setImage(const QImage& img)
{
	d_data->image = img;
}
void VipImageData::setArrayRect(const QRectF& r)
{
	d_data->array_rect = r;
}
void VipImageData::setSrcImageRect(const QRectF& r)
{
	d_data->srcImageRect = r;
}
void VipImageData::setDstPolygon(const QPolygonF& d)
{
	d_data->dst = d;
}

const QImage& VipImageData::image() const
{
	return d_data->image;
}
const QImage& VipImageData::constImage() const
{
	return d_data->image;
}
QImage& VipImageData::image()
{
	return d_data->image;
}
const QRectF& VipImageData::arrayRect() const
{
	return d_data->array_rect;
}
const QRectF& VipImageData::srcImageRect() const
{
	return d_data->srcImageRect;
}
const QPolygonF& VipImageData::dstPolygon() const
{
	return d_data->dst;
}

bool VipImageData::isEmpty() const
{
	return d_data->image.isNull() || d_data->dst.isEmpty();
}

VipImageData::PrivateData::PrivateData() {}

VipImageData::PrivateData::PrivateData(const PrivateData& other)
  : QSharedData()
{
	image = other.image;
	array_rect = other.array_rect;
	srcImageRect = other.srcImageRect;
	dst = other.dst;
}

class VipPlotRasterData::PrivateData
{
public:
	PrivateData()
	  : opacityFactor(0.5)
	  , empty_data(true)
	  , borderPen(Qt::NoPen)
	  , modifiedTime(0)
	{
	}

	VipNDArray temporaryArray;
	VipImageData imageData;
	VipImageData bypassImageData;
	QImage superimposeImage;
	QImage backgroundImage;
	double opacityFactor;
	bool empty_data;

	VipInterval dataValidInterval;
	VipInterval dataInterval;

	QRectF imageRect;

	QPen borderPen;

	qint64 modifiedTime;
	QRectF modifiedRect;
};

static int registerRasterDataKeyWords()
{
	VipKeyWords keys;
	keys["superimpose-opacity"] = VipParserPtr(new DoubleParser());
	vipSetKeyWordsForClass(&VipPlotRasterData::staticMetaObject, keys);
	return 0;
}
static int _registerRasterDataKeyWords = registerRasterDataKeyWords();

VipPlotRasterData::VipPlotRasterData(const VipText& title)
  : VipPlotItemDataType(title)
  , d_data(new PrivateData())
{
	this->setItemAttribute(VisibleLegend, false);
	this->setItemAttribute(ClipToScaleRect, false);
	this->setSelectedPen(Qt::NoPen);
}

VipPlotRasterData::~VipPlotRasterData()
{
}

bool VipPlotRasterData::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	if (strcmp(name, "superimpose-opacity") == 0) {
		setSuperimposeOpacity(value.toDouble());
		return true;
	}
	return VipPlotItemData::setItemProperty(name, value, index);
}

void VipPlotRasterData::setBorderPen(const QPen& pen)
{
	d_data->borderPen = pen;
	emitItemChanged();
}

const QPen& VipPlotRasterData::borderPen() const
{
	return d_data->borderPen;
}

QList<VipInterval> VipPlotRasterData::plotBoundingIntervals() const
{
	return VipInterval::fromRect(d_data->imageRect);
}

VipInterval VipPlotRasterData::plotInterval(const VipInterval& interval) const
{
	if (d_data->dataInterval.isValid() && d_data->dataValidInterval == interval)
		return d_data->dataInterval;
	Locker lock(dataLock());
	const_cast<VipPlotRasterData*>(this)->d_data->dataValidInterval = interval;
	return const_cast<VipPlotRasterData*>(this)->d_data->dataInterval = data().value<VipRasterData>().bounds(interval);
}

QRectF VipPlotRasterData::imageBoundingRect() const
{
	return d_data->imageRect;
}

QRectF VipPlotRasterData::boundingRect() const
{
	return sceneMap()->transform(VipInterval::toRect(VipAbstractScale::scaleIntervals(axes())) & d_data->imageRect).boundingRect();
}

const QImage& VipPlotRasterData::image() const
{
	return d_data->imageData.constImage();
}

QPainterPath VipPlotRasterData::shape() const
{
	QPainterPath p;
	QRectF rect = boundingRect().adjusted(-1, -1, 1, 1);
	p.addRect(rect);
	return p;
}

// QPainterPath VipPlotRasterData::shapeFromCoordinateSystem(const VipCoordinateSystemPtr & m) const
//  {
//  QRectF rect = VipInterval::toRect(VipAbstractScale::scaleIntervals(axes())) & d_data->imageRect;
//  rect.adjust(-1,-1,1,1);
//  rect = d_data->imageRect & rect;
//  QPolygonF dst = m->transform(rect);
//  QPainterPath p;
//  p.addPolygon(dst);
//  return p;
//  }

void VipPlotRasterData::drawSelected(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	QRectF rect = VipInterval::toRect(VipAbstractScale::scaleIntervals(axes()));
	QPolygonF poly = m->transform(rect);

	draw(painter, m);

	// draw a border around the image

	painter->setPen(this->selectedPen());
	VipPainter::drawPolygon(painter, poly);
}

/* static void convertToOpenGL(const QImage& src, QImage& dst)
{

	if (dst.size() != src.size()) {
		dst = QImage(src.width(), src.height(), QImage::Format_ARGB32);
	}
	// dst.bits();
	memcpy((void*)dst.bits(), src.bits(), src.width() * src.height() * 4);
	return;

	const int width = src.width();
	const int height = src.height();
	const uint* p = (const uint*)src.scanLine(src.height() - 1);
	uint* q = (uint*)dst.scanLine(0);

	if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
		for (int i = 0; i < height; ++i) {
			const uint* end = p + width;
			while (p < end) {
				*q = (*p << 8) | ((*p >> 24) & 0xff);
				p++;
				q++;
			}
			p -= 2 * width;
		}
	}
	else {
		for (int i = 0; i < height; ++i) {
			const uint* end = p + width;
			while (p < end) {
				*q = ((*p << 16) & 0xff0000) | ((*p >> 16) & 0xff) | (*p & 0xff00ff00);
				p++;
				q++;
			}
			p -= 2 * width;
		}
	}
}*/

#include <qopenglcontext.h>
#include <qopenglfunctions.h>

void VipPlotRasterData::drawBackground(QPainter* painter, const VipCoordinateSystemPtr& m, const QRectF& rect, const QPolygonF& dst) const
{
	(void)m;
	// draw the background image
	if (!d_data->backgroundImage.isNull()) {
		// compute the source rect
		QRectF im_rect = this->rawData().boundingRect();
		double factor_x = d_data->backgroundImage.width() / im_rect.width();
		double factor_y = d_data->backgroundImage.height() / im_rect.height();
		QRectF src_rect(QPoint(rect.left() * factor_x, rect.top() * factor_y), QPoint(rect.right() * factor_x, rect.bottom() * factor_y));
		VipPainter::drawImage(painter, dst, d_data->backgroundImage, src_rect);
	}
}

void VipPlotRasterData::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{
	QRectF rect;
	QPolygonF dst;
	VipImageData bypass = d_data->bypassImageData;
	bool use_bypass = !bypass.isEmpty() && computeArrayRect(this->rawData()) == bypass.arrayRect();

	if (!use_bypass) {
		if (VipPlotItemData::data().isNull() && !d_data->imageData.constImage().isNull()) {
			// directly draw imageData
			QRectF src = d_data->imageRect & VipInterval::toRect(VipAbstractScale::scaleIntervals(axes()));
			dst = m->transform(src);
			rect = d_data->imageRect;
			d_data->imageData.setArrayRect(rect);
			d_data->imageData.setSrcImageRect(src);
			d_data->imageData.setDstPolygon(dst);
			painter->setRenderHint(QPainter::SmoothPixmapTransform, renderHints() & QPainter::Antialiasing);
			drawBackground(painter, m, rect, dst);
			VipPainter::drawImage(painter, d_data->imageData.dstPolygon(), d_data->imageData.constImage(), d_data->imageData.srcImageRect());
		}
		else {
			VipInterval inter;
			if (colorMap())
				inter = colorMap()->gripInterval();
			if (computeImage(this->rawData(), inter, m, d_data->temporaryArray, d_data->imageData)) {
				painter->setRenderHint(QPainter::SmoothPixmapTransform, renderHints() & QPainter::Antialiasing);
				rect = d_data->imageData.arrayRect();
				dst = d_data->imageData.dstPolygon();
				drawBackground(painter, m, rect, dst);
				VipPainter::drawImage(painter, d_data->imageData.dstPolygon(), d_data->imageData.constImage(), d_data->imageData.srcImageRect());
			}
			else
				return;
		}
	}
	else {
		rect = bypass.arrayRect();
		dst = bypass.dstPolygon();
		drawBackground(painter, m, rect, dst);
		painter->setRenderHint(QPainter::SmoothPixmapTransform, renderHints() & QPainter::Antialiasing);
		VipPainter::drawImage(painter, bypass.dstPolygon(), bypass.constImage(), bypass.srcImageRect());
		d_data->bypassImageData = VipImageData();
	}

	// draw the superimpose image
	if (!d_data->superimposeImage.isNull()) {
		painter->save();

		// compute the source rect
		QRectF im_rect = this->rawData().boundingRect();
		double factor_x = d_data->superimposeImage.width() / im_rect.width();
		double factor_y = d_data->superimposeImage.height() / im_rect.height();
		QRectF src_rect(QPoint(rect.left() * factor_x, rect.top() * factor_y), QPoint(rect.right() * factor_x, rect.bottom() * factor_y));

		painter->setOpacity(d_data->opacityFactor);
		painter->setRenderHint(QPainter::SmoothPixmapTransform, renderHints() & QPainter::Antialiasing);
		VipPainter::drawImage(painter, dst, d_data->superimposeImage, src_rect);

		painter->restore();
	}

	// draw the border pen
	if (d_data->borderPen.color() != Qt::transparent && d_data->borderPen.style() != Qt::NoPen) {
		rect = rect.adjusted(0.1, 0.1, -0.1, -0.1);
		QPolygonF poly = m->transform(rect);
		painter->setBrush(QBrush());
		d_data->borderPen.setWidth(2);
		painter->setPen(d_data->borderPen);
		painter->drawPolygon(poly);
	}

	Q_EMIT const_cast<VipPlotRasterData*>(this)->imageDrawn();
}

bool VipPlotRasterData::computeImage(const VipRasterData& ar, const VipInterval& interval, const VipCoordinateSystemPtr& m, VipNDArray& tmp_array, VipImageData& img) const
{
	QRectF rect;
	QRectF srcImageRect;
	QPolygonF dst;
	if (computeImage(ar, interval, m, tmp_array, img.image(), dst, rect, srcImageRect)) {
		img.setArrayRect(rect);
		img.setSrcImageRect(srcImageRect);
		img.setDstPolygon(dst);
		return true;
	}
	return false;
}

QRectF VipPlotRasterData::computeArrayRect(const VipRasterData& raster) const
{
	QRectF rect = VipInterval::toRect(VipAbstractScale::scaleIntervals(axes()));
	// rect.adjust(-1, -1, 1, 1);

	if (!raster.isEmpty())
		rect = rect & raster.boundingRect();
	else
		rect = rect & d_data->imageRect;
	if (rect.isEmpty()) {
		// null array: recompute the image rect based on the superimpose image
		rect = VipInterval::toRect(VipAbstractScale::scaleIntervals(axes()));
		rect.adjust(-1, -1, 1, 1);
		rect = QRectF(0, 0, d_data->superimposeImage.width(), d_data->superimposeImage.height()) & rect;
	}
	return rect;
}

bool VipPlotRasterData::computeImage(const VipRasterData& raster,
				     const VipInterval& interval,
				     const VipCoordinateSystemPtr& m,
				     VipNDArray& tmp_array,
				     QImage& out,
				     QPolygonF& dst,
				     QRectF& rect,
				     QRectF& src_image_rect) const
{
	rect = computeArrayRect(raster);

	// compute the destination rect
	dst = m->transform(rect);
	QRect dst_rect = dst.boundingRect().toRect();

	if (!rect.isEmpty()) {
		QRectF extracted_rect;

		if (raster.dataType() == qMetaTypeId<QImage>() || raster.dataType() == qMetaTypeId<QPixmap>()) {
			out = vipToImage(raster.extract(rect, &extracted_rect));
			// set src_image_rect, it will be directly used in VipPainter::drawImage
			src_image_rect = rect;
			src_image_rect.moveTopLeft(rect.topLeft() - extracted_rect.topLeft());
		}
		else {
			VipNDArray tmp = raster.extract(rect, &extracted_rect);
			if (tmp.isEmpty())
				return false;

			QRect im_rect(0, 0, qMin(dst_rect.width(), (int)extracted_rect.width()), qMin(dst_rect.height(), (int)extracted_rect.height()));
			if (out.width() != im_rect.width() || out.height() != im_rect.height())
				out = QImage(im_rect.width(), im_rect.height(), QImage::Format_ARGB32_Premultiplied);
			if (tmp_array.shape() != vipVector(im_rect.height(), im_rect.width()) || tmp_array.dataType() != tmp.dataType())
				tmp_array = VipNDArray(tmp.dataType(), vipVector(im_rect.height(), im_rect.width()));
			tmp.resize(tmp_array);

			if (VipAxisColorMap* axis_map = colorMap()) {
				const VipColorMap* map = axis_map->colorMap();
				map->applyColorMap(interval, tmp_array, (QRgb*)out.bits(), VIP_COLOR_MAP_THREADS);

				// set src_image_rect, it will be directly used in VipPainter::drawImage

				// since we resized the array, reflect this in the src rect
				src_image_rect = rect;
				src_image_rect.moveTopLeft(rect.topLeft() - extracted_rect.topLeft());
				double factor_h = im_rect.width() / (double)extracted_rect.width();
				double factor_v = im_rect.height() / (double)extracted_rect.height();
				src_image_rect.setLeft(src_image_rect.left() * factor_h);
				src_image_rect.setRight(src_image_rect.right() * factor_h);
				src_image_rect.setTop(src_image_rect.top() * factor_v);
				src_image_rect.setBottom(src_image_rect.bottom() * factor_v);
			}
		}

		return true;
	}

	return false;
}

QList<VipText> VipPlotRasterData::legendNames() const
{
	return QList<VipText>() << title();
}

QRectF VipPlotRasterData::drawLegend(QPainter* painter, const QRectF& r, int // index
) const
{
	QRectF square = vipInnerSquare(r);
	painter->setBrush(QBrush());
	painter->setPen(QPen());
	painter->drawRect(square);
	painter->drawImage(square, d_data->imageData.constImage(), QRectF(0, 0, d_data->imageData.constImage().width(), d_data->imageData.constImage().height()));
	return square;
}

void VipPlotRasterData::updateInternal(bool dirty_color_scale)
{
	update();
	if (dirty_color_scale)
		markColorMapDirty();
}

QVariant VipPlotRasterData::data() const
{
	QVariant v = VipPlotItemData::data();
	// directly return imageData
	if (v.isNull() && !d_data->imageData.image().isNull())
		return QVariant::fromValue(VipRasterData(vipToArray(d_data->imageData.image()), d_data->imageRect.topLeft()));
	return v;
}

void VipPlotRasterData::setData(const QVariant& v)
{
	QRectF crect;
	int ctype = 0;
	bool carray = false;

	VipRasterData current = VipPlotItemData::data().value<VipRasterData>();
	if (!current.isNull()) {
		// valid current data
		crect = current.boundingRect();
		ctype = current.dataType();
		carray = current.isArray();
	}
	else if (!d_data->imageRect.isEmpty()) {
		// null current data: imageData
		crect = d_data->imageRect;
		ctype = qMetaTypeId<QImage>();
		carray = true;
	}

	VipRasterData _new;
	if (v.userType() == qMetaTypeId<VipNDArray>())
		_new = VipRasterData(v.value<VipNDArray>());
	else if (v.userType() == qMetaTypeId<QImage>())
		_new = VipRasterData(vipToArray(v.value<QImage>()));
	else
		_new = v.value<VipRasterData>();

	QRectF nrect = _new.boundingRect();

	d_data->empty_data = _new.isEmpty();

	// reset intervals
	d_data->dataInterval = d_data->dataValidInterval = VipInterval();

	if (!_new.isEmpty() && carray && _new.isArray() && ctype == _new.dataType() && crect == nrect) {

		bool update_colorscale = true;

		// direct copy

		const VipNDArray _ne = _new.extract(_new.boundingRect());
		if (_ne.dataType() == qMetaTypeId<QImage>()) {
			// direct copy to imageData
			{
				Locker locker(dataLock());
				const QImage qne = vipToImage(_ne);
				if (d_data->imageData.image().size() != qne.size()) {
					d_data->imageData = QImage(qne.width(), qne.height(), QImage::Format_ARGB32);
				}
				memcpy(d_data->imageData.image().bits(), qne.bits(), qne.width() * qne.height() * 4);
			}
			// reset internal data so that data() returns imageData
			setInternalData(QVariant());
			update_colorscale = false;
		}
		else {
			dataLock()->lock();
			const QRectF bounding = current.boundingRect();
			VipNDArray _cur = current.extract(bounding);
			// copy ND array content
			if (_cur.isUnstrided() && _ne.isUnstrided() && vipIsArithmetic(_cur.dataType()) && _cur.handle()->ref <= 3)
				memcpy((void*)_cur.data(), _ne.constData(), _ne.size() * _ne.dataSize());
			else
				_ne.convert((_cur));
			dataLock()->unlock();
			setInternalData(QVariant::fromValue(current = VipRasterData(_cur, bounding.topLeft())));

			// Optmize color map computation if the color scale only has this item
			Locker locker(dataLock());
			if (colorMap()) { //&& colorMap()->itemList().size() == 1) {
				VipInterval interval = colorMap()->gripInterval();
				if (colorMap()->isAutoScale()) {
					d_data->dataValidInterval = colorMap()->validInterval();
					dataLock()->unlock();
					interval = current.bounds(d_data->dataValidInterval);
					dataLock()->lock();
					d_data->dataInterval = interval;
				}
				// computeImage(current, interval, sceneMap(), d_data->temporaryArray, d_data->bypassImageData);
			}
		}

		// update and mark color scale as dirty if needed
		if (QThread::currentThread() == qApp->thread())
			updateInternal(update_colorscale);
		else
			QMetaObject::invokeMethod(this, "updateInternal", Qt::QueuedConnection, Q_ARG(bool, update_colorscale));
	}
	else {

		d_data->empty_data = false;
		VipPlotItemDataType::setData(QVariant::fromValue(_new));
		if (crect != nrect) {
			d_data->imageRect = nrect;
			Q_EMIT imageRectChanged(nrect);
		}

		//TEST
		bool update_colorscale = (_new.dataType() != qMetaTypeId<QImage>());
		// update and mark color scale as dirty if needed
		if (QThread::currentThread() == qApp->thread())
			updateInternal(update_colorscale);
		else
			QMetaObject::invokeMethod(this, "updateInternal", Qt::QueuedConnection, Q_ARG(bool, update_colorscale));
	}
}

QString VipPlotRasterData::imageValue(const QPoint& im_pos) const
{
	// display the value

	QVariant value;
	dataLock()->lock();
	const VipNDArray ar = rawData().extract(rawData().boundingRect());
	if (!ar.isNull()) {
		int x = im_pos.x();
		int y = im_pos.y();
		if (ar.shape().size() == 2 && x >= 0 && y >= 0 && x < ar.shape(1) && y < ar.shape(0))
			value = ar(vipVector(y, x));
	}
	dataLock()->unlock();

	if (!value.isNull()) {
		if (value.userType() == qMetaTypeId<QColor>()) {
			QColor c = value.value<QColor>();
			return ("(ARGB) " + QString::number(c.alpha()) + ", " + QString::number(c.red()) + ", " + QString::number(c.green()) + ", " + QString::number(c.blue()));
		}
		else if (value.userType() == qMetaTypeId<VipRGB>()) {
			VipRGB c = value.value<VipRGB>();
			return ("(ARGB) " + QString::number(c.a) + ", " + QString::number(c.r) + ", " + QString::number(c.g) + ", " + QString::number(c.b));
		}
		else if (value.userType() == QMetaType::Short || value.userType() == QMetaType::UShort || value.userType() == QMetaType::Int || value.userType() == QMetaType::UInt ||
			 value.userType() == QMetaType::Long || value.userType() == QMetaType::ULong || value.userType() == QMetaType::LongLong || value.userType() == QMetaType::ULongLong) {
			return ("(" + QString(value.typeName()) + ") " + QString::number(value.toLongLong()));
		}
		else if (value.canConvert<double>()) {
			return ("(" + QString(value.typeName()) + ") " + QString::number(value.toDouble()));
		}
		else if (value.userType() == QMetaType::UChar) {
			return ("(" + QString(value.typeName()) + ") " + QString::number(value.value<unsigned char>()));
		}
		else {
			return ("(" + QString(value.typeName()) + ") " + value.toString());
		}
	}

	return QString();
}

void VipPlotRasterData::setBackgroundImage(const QImage& img)
{
	d_data->backgroundImage = img;
	bool empty_raw_data = this->rawData().isEmpty();
	if (empty_raw_data) {
		// if not data set yet, set a transparent image with the right dimension
		QImage spec(img.width(), img.height(), QImage::Format_ARGB32);
		spec.fill(Qt::transparent);
		this->setRawData(VipRasterData(spec));
		d_data->empty_data = true;
	}
	emitItemChanged();
}
const QImage& VipPlotRasterData::backgroundimage() const
{
	return d_data->backgroundImage;
}

static bool compareImage(const QImage &img1, const QImage &img2)
{
	if (img1.size() != img2.size())
		return false;
	if (img1.format() != img2.format())
		return false;

	auto s1 = img1.sizeInBytes();
	auto s2 = img2.sizeInBytes();
	if (s1 != s2)
		return false;
	if (s1 == 0)
		return true;
	const auto* p1 = img1.bits();
	const auto* p2 = img2.bits();
	return memcmp(p1, p2, (size_t)s1) == 0;
}

void VipPlotRasterData::setSuperimposeImage(const QImage& img)
{
	if (compareImage(img, d_data->superimposeImage))
		return;
	d_data->superimposeImage = img;
	bool empty_raw_data = this->rawData().isEmpty();
	if (empty_raw_data) {
		// if not data set yet, set a transparent image with the right dimension
		QImage spec(img.width(), img.height(), QImage::Format_ARGB32);
		spec.fill(Qt::transparent);
		this->setRawData(VipRasterData(spec));
		d_data->empty_data = true;
	}

	if (img.isNull()) {
		// reset superimpose image and empty spectrogram:
		// if (d_data->empty_data )
		//	this->setRawData(VipRasterData());
	}
	emitItemChanged();
}

const QImage& VipPlotRasterData::superimposeImage() const
{
	return d_data->superimposeImage;
}

void VipPlotRasterData::setSuperimposeOpacity(double factor)
{
	if (factor < 0)
		factor = 0;
	else if (factor > 1)
		factor = 1;
	d_data->opacityFactor = factor;
	emitItemChanged();
}

double VipPlotRasterData::superimposeOpacity() const
{
	return d_data->opacityFactor;
}

QString VipPlotRasterData::formatText(const QString& str, const QPointF& pos) const
{
	VipText res = VipPlotItem::formatText(str, pos);
	if (res.text().contains("#value")) {

		QPointF scale_pos = sceneMap()->invTransform(pos);
		// Lock to avoid potential crash when displaying the video and the tool tip at the same time
		dataLock()->lock();
		QPointF im_pos = (scale_pos - rawData().boundingRect().topLeft());
		QString im_value = imageValue(QPoint(im_pos.x(), im_pos.y()));
		dataLock()->unlock();

		res.replace("#value", im_value.toLatin1().data(), true);
	}
	return res.text();
}

VipArchive& operator<<(VipArchive& arch, const VipPlotRasterData*)
{
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotRasterData*)
{
	return arch;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipPlotRasterData*>();
	vipRegisterArchiveStreamOperators<VipPlotRasterData*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
