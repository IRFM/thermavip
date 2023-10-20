#include "VipNDArrayImage.h"
#include "VipDataType.h"
#include <QPainter>
#include <QTextStream>

namespace detail
{

	struct QRgbToString
	{
		QString operator()(QRgb value) const {
			QString res;
			QTextStream str(&res,QIODevice::WriteOnly);
			str << VipRGB(value);
			return res;
		}
	};

	struct QRgbToByteArray
	{
		QByteArray operator()(QRgb value) const {
			QByteArray res;
			QTextStream str(&res,QIODevice::WriteOnly);
			str << VipRGB(value);
			return res;
		}
	};



	static bool export_image_data(const QImage* this_device, int this_type, const VipNDArrayShape & this_shape, const VipNDArrayShape & this_start,
		QImage* other_device, int other_type, const VipNDArrayShape & other_shape, const VipNDArrayShape & other_start)
	{

		if(this_type != qMetaTypeId<QImage>())
		{
			return false;
		}
		else if(other_type != qMetaTypeId<QImage>())
		{
			return false;
		}

		//check for direct copy (no sub window)
		if(vipVector(this_device->height(),this_device->width()) == this_shape &&
			vipVector(other_device->height(),other_device->width()) == other_shape &&
			vipVector(0,0) == this_start &&
			vipVector(0,0) == other_start )
		{
			*other_device = *(this_device);
		}
		//use QPainter to perform the copy
		else
		{

			QPoint this_start_pt(  this_start[1], this_start[0] );
			QPoint other_start_pt(  other_start[1], other_start[0] );

			QRect target( other_start_pt , QSize( other_shape[1],other_shape[0] ));
			QRect source( this_start_pt , QSize( this_shape[1],this_shape[0] ));

			QPainter painter(other_device);
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			//painter.fillRect(target,Qt::white);

			painter.drawImage(target, *this_device ,source);
		}

		return true;
	}


	struct ImageHandle : public VipNDArrayHandle
	{
		//always owns the image
		ImageHandle() {}
		virtual QPaintDevice * painteDevice() = 0;
		virtual const QPaintDevice * painteDevice() const = 0;
	};


	struct QImageNDFxTable: public ImageHandle
	{
		QImage * image() const {return const_cast<QImage*>(static_cast<const QImage*>(opaque));}
		QImageNDFxTable() { opaque = nullptr; }
		QImageNDFxTable(const QImageNDFxTable & other)
		:ImageHandle()
		{
			if(other.image())
				opaque = new QImage(*other.image());

			shape = other.shape;
			strides = other.strides;
			size = other.size;
		}

		virtual QImageNDFxTable * copy() const { return new QImageNDFxTable(*this); }
		virtual void * dataPointer(const VipNDArrayShape & ) const { return nullptr; }
		virtual int handleType() const { return Image; }
		virtual QPaintDevice * painteDevice() {return image();}
		virtual const QPaintDevice * painteDevice() const {return image();}

		//should destroy the underlying data only if size != 0
		virtual ~QImageNDFxTable() {
			if(image())
			{
				delete image();
				opaque = nullptr;
			}
		}

		virtual bool realloc(const VipNDArrayShape & sh)
		{
			if(image())
				delete image();
			opaque = new QImage(sh[1],sh[0],QImage::Format_ARGB32);
			shape = vipVector(image()->height(),image()->width());
			size = vipComputeDefaultStrides<Vip::FirstMajor>(shape,strides);
			return true;
		}

		virtual bool reshape(const VipNDArrayShape & new_shape)
		{
			if(!image())
				opaque = new QImage(new_shape[1],new_shape[0],QImage::Format_ARGB32);
			else
				*image() = image()->scaled(new_shape[1],new_shape[0]);

			shape = new_shape;
			size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides);
			return true;
		}

		virtual bool resize(const VipNDArrayShape & _start, const VipNDArrayShape & _shape, VipNDArrayHandle * dst, Vip::InterpolationType type, const VipNDArrayShape & out_start, const VipNDArrayShape & out_shape ) const
		{
			if(!image())
				return false;

			if(dst->dataType() == qMetaTypeId<QImage>())
			{
				QRect source = QRect(_start[1], _start[0], _shape[1], _shape[0]);
				QRect dest = QRect(out_start[1], out_start[0], out_shape[1], out_shape[0]);
				QPainter painter(static_cast<ImageHandle*>(dst)->painteDevice());
				painter.setRenderHint(QPainter::SmoothPixmapTransform,type != Vip::NoInterpolation);
				painter.drawImage(dest, *image(),source);
				return true;
			}
			else if(dst->dataType() == qMetaTypeId<VipRGB>())
			{
				QImage temp;
				QImage src = image()->copy(_start[1], _start[0], _shape[1], _shape[0]);
				QRect out = QRect(out_start[1], out_start[0], out_shape[1], out_shape[0]);
				if(type == Vip::NoInterpolation)
					temp = src.scaled(out.width(), out.height(),Qt::IgnoreAspectRatio,Qt::FastTransformation).convertToFormat(QImage::Format_ARGB32);
				else
					temp = src.scaled(out.width(), out.height(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

				VipRGB* ptr = static_cast<VipRGB*>(dst->opaque) + vipFlatOffset<false>(dst->strides, out_start);

				return vipArrayTransform(reinterpret_cast<const QRgb*>(temp.bits()),_shape, strides,
					(QRgb*)ptr,out_shape,dst->strides,VipNullTransform());
			}

			return false;
		}

		virtual const char* dataName() const {return "QImage";}
		virtual int dataSize() const {return sizeof(QRgb);}
		virtual int dataType() const {return qMetaTypeId<QImage>();}

		virtual bool canExport(int data_type) const {return qMetaTypeId<QImage>() == data_type ||
				data_type == qMetaTypeId<QString>() || data_type == qMetaTypeId<QByteArray>() || data_type == qMetaTypeId<VipRGB>() || data_type == qMetaTypeId<QRgb>() ;}
		virtual bool canImport(int data_type) const {return data_type == qMetaTypeId<VipRGB>();}

		virtual bool exportData(const VipNDArrayShape & this_start, const VipNDArrayShape & this_shape, VipNDArrayHandle * dst, const VipNDArrayShape & dst_start, const VipNDArrayShape & dst_shape) const
		{
			if (this_shape != dst_shape)
				return false;

			int out_type = dst->dataType();
			if(qMetaTypeId<QImage>() == out_type )
			{
				return export_image_data(image(),qMetaTypeId<QImage>(),this_shape,this_start,
						static_cast<QImageNDFxTable*>(dst)->image(),dst->dataType(),dst_shape,dst_start);
			}
			else if(out_type == qMetaTypeId<VipRGB>())
			{
				return vipArrayTransform(reinterpret_cast<const QRgb*>(image()->bits())+ vipFlatOffset<false>(strides, this_start), this_shape, strides,
						static_cast<VipRGB*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start),dst_shape,dst->strides,ToRGB());
			}
			else if(out_type == qMetaTypeId<QRgb>())
			{
				return vipArrayTransform(reinterpret_cast<const QRgb*>(image()->bits())+ vipFlatOffset<false>(strides, this_start), this_shape, strides,
						static_cast<QRgb*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start),dst_shape,dst->strides, VipNullTransform());
			}
			else if(out_type == qMetaTypeId<QString>())
			{
				return vipArrayTransform(reinterpret_cast<const QRgb*>(image()->bits())+ vipFlatOffset<false>(strides, this_start), this_shape, strides,
						static_cast<QString*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start),dst_shape,dst->strides,QRgbToString());
			}
			else if(out_type == qMetaTypeId<QByteArray>())
			{
				return vipArrayTransform(reinterpret_cast<const QRgb*>(image()->bits())+ vipFlatOffset<false>(strides, this_start), this_shape, strides,
						static_cast<QByteArray*>(dst->opaque) + vipFlatOffset<false>(dst->strides, dst_start),dst_shape,dst->strides,QRgbToByteArray());
			}
			return false;
		}
		virtual bool importData(const VipNDArrayShape & this_start, const VipNDArrayShape & this_shape, const VipNDArrayHandle * src, const VipNDArrayShape & src_start, const VipNDArrayShape & src_shape)
		{
			if (src->dataType() != qMetaTypeId<VipRGB>())
				return false;

			return vipArrayTransform(reinterpret_cast<const QRgb*>(src->opaque) + vipFlatOffset<false>(src->strides, src_start), src_shape, src->strides,
				reinterpret_cast<QRgb*>(image()->bits()) + vipFlatOffset<false>(strides, this_start), this_shape, strides, VipNullTransform());
		}

		virtual bool fill(const VipNDArrayShape & _start, const VipNDArrayShape & _shape, const QVariant & value)
		{
			if(!value.canConvert<QColor>())
				return false;

			QPainter p(image());
			p.setPen(Qt::NoPen);
			p.setBrush(value.value<QColor>());
			p.setCompositionMode(QPainter::CompositionMode_Source);
			p.drawRect(QRect(_start[1], _start[0], _shape[1], _shape[0]));
			return true;
		}
		virtual QVariant toVariant(const VipNDArrayShape & sh) const
		{
			int pos = 0;
			if (sh.size() == 1) pos = sh[0] * image()->width();
			else if (sh.size() > 1) pos = sh[0] * image()->width() + sh[1];
			QRgb rgb = ((QRgb*)image()->bits())[pos];
			return QVariant::fromValue(VipRGB(qRed(rgb),qGreen(rgb),qBlue(rgb),qAlpha(rgb)));
		}

		virtual void fromVariant(const VipNDArrayShape & sh, const QVariant & val)
		{
			int pos = 0;
			if (sh.size() == 1) pos = sh[0] * image()->width();
			else if (sh.size() > 1) pos = sh[0] * image()->width() + sh[1];
			((QRgb*)image()->bits())[pos] = val.value<VipRGB>();
		}

		virtual QDataStream & ostream(const VipNDArrayShape & _start, const VipNDArrayShape & _shape, QDataStream & o) const
		{
			o << image()->copy(_start[1],_start[0],_shape[1],_shape[0]);
			return o;
		}

		virtual QDataStream & istream(const VipNDArrayShape & _start, const VipNDArrayShape & _shape, QDataStream & i)
		{
			QImage tmp;
			i >> tmp;

			QPainter p(image());
			p.drawImage(QPoint(_start[1], _start[0]), tmp.scaled(_shape[1],_shape[0]));

			return i;
		}

		virtual QTextStream & oTextStream(const VipNDArrayShape & _start, const VipNDArrayShape & _shape, QTextStream & stream, const QString & separator) const
		{
			const VipRGB* rgb = reinterpret_cast<const VipRGB*>(image()->bits());// +shape[1] + shape[0] * image()->width();
			for(int y=_start[0]; y < _start[0]+_shape[0]; ++y)
				for (int x = _start[1]; x < _start[1]+_shape[1]; ++x)
				{
					const VipRGB value = rgb[x + y*image()->width()];
					stream << value << separator;
				}

			return stream;
		}
	};



}//end detail






//register image types
static bool registerImageTypes()
{
	static bool to_register = true;
	if(to_register)
	{
		vipRegisterArrayType(VipNDArrayHandle::Image, qMetaTypeId<QImage>(),SharedHandle( new detail::QImageNDFxTable()));
		vipRegisterArrayType(VipNDArrayHandle::Standard, qMetaTypeId<QImage>(), SharedHandle(new detail::QImageNDFxTable()));
		to_register = false;
	}
	return true;
}


VipNDArray vipToArray(const QImage & image)
{
	//if (image.isNull())
	//	return VipNDArray();

#if !(QT_VERSION < QT_VERSION_CHECK(5, 13, 0))
	if (image.format() == QImage::Format_Grayscale16)
	{
		VipNDArrayType<unsigned short> res(vip_vector(image.height(), image.width()));
		for (int y = 0; y < image.height(); ++y) {
			memcpy(res.ptr(vip_vector(y, 0)), image.scanLine(y), image.width() * sizeof(unsigned short));
		}
		return res;
	}
#endif

	registerImageTypes();
	detail::QImageNDFxTable* h =(new detail::QImageNDFxTable());
	h->opaque = new QImage(image.convertToFormat(QImage::Format_ARGB32));
	h->shape = vipVector(image.height(),image.width());
	h->size = vipComputeDefaultStrides<Vip::FirstMajor>(h->shape,h->strides);
	return VipNDArray(SharedHandle(h));
}


QImage vipToImage(const VipNDArray & array)
{
	registerImageTypes();
	if(array.dataType() == qMetaTypeId<QImage>())
	{
		const detail::QImageNDFxTable* h = static_cast<const detail::QImageNDFxTable*>(array.handle());
		if(h->image())
			return *h->image();
	}
	else
	{
		const VipNDArray temp = array.convert(qMetaTypeId<QImage>());
		if(!temp.isNull())
		{
			const detail::QImageNDFxTable* h = static_cast<const detail::QImageNDFxTable*>(temp.handle());
			if(h->image())
				return *h->image();
		}
	}

	return QImage();
}

bool vipIsImageArray(const VipNDArray & ar)
{
	return ar.dataType() == qMetaTypeId<QImage>() ;
}
