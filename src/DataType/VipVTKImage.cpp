#include <QFile>
#include <QVector>
#include <QTextStream>
#include <QImage>

#include <vtkXMLImageDataReader.h>
#include <vtkXMLImageDataWriter.h>
#include <vtkScalarsToColors.h>
#include <vtkImageResize.h>
#include <vtkImageSincInterpolator.h>
#include <vtkMath.h>
#include <cmath>

#include "VipVTKImage.h"
#include "VipVTKObject.h"




vtkSmartPointer<vtkImageData>  VipVTKImage::createVTKImage(int w, int h, double value , int type)
{
	vtkSmartPointer<vtkImageData> res = vtkSmartPointer<vtkImageData>::New();
	res->SetDimensions(w,h,1);
	//res->SetNumberOfScalarComponents(1);
	//res->SetScalarType(type);
	res->AllocateScalars(type,1);

	for(int y=0; y < h; ++y)
	{
		for(int x=0;x<w;++x) 
		{
			res->SetScalarComponentFromDouble(x,y,0,0,value);
		}
	}

	return res;
}

vtkSmartPointer<vtkImageData> VipVTKImage::createVTKImage(int w, int h, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	vtkSmartPointer<vtkImageData> res = vtkSmartPointer<vtkImageData>::New();
	res->SetDimensions(w,h,1);
	//res->SetNumberOfScalarComponents(4);
	//res->SetScalarTypeToUnsignedChar();
	res->AllocateScalars(VTK_UNSIGNED_CHAR,4);

	for(int y=0; y < h; ++y)
	{
		for(int x=0;x<w;++x)
		{
			unsigned char * pixels = static_cast<unsigned char*>(res->GetScalarPointer(x,y,0)) ;
			pixels[0] = r;
			pixels[1] = g;
			pixels[2] = b;
			pixels[3] = a;
		}
	}

	return res;
}




static bool isLineEmpty(const QString & line)
{
	if(line.isEmpty())
		return true;

	for(int i=0; i< line.size(); ++i)
		if( line[i] != ' ' && line[i] != '\t')
			return false;

	return true;
}



vtkSmartPointer<vtkImageData> VipVTKImage::loadImageFileToVTK(const QString& filename)
{
	QImage img;
	if(img.load(filename))
	{
		img = img.convertToFormat(QImage::Format_ARGB32);
		vtkSmartPointer<vtkImageData> res = createVTKImage(img.width(),img.height(),0,0,0,0);
		const uint * ptr = reinterpret_cast<const uint*>(img.constBits());
		for(int y=0; y < img.height(); ++y)
		{
			for(int x=0;x<img.width();++x, ++ptr)
			{
				unsigned char * pixels = static_cast<unsigned char*>(res->GetScalarPointer(x,y,0)) ;
				pixels[0] = qRed(*ptr);
				pixels[1] = qGreen(*ptr);
				pixels[2] = qBlue(*ptr);
				pixels[3] = qAlpha(*ptr);
			}
		}

		return res;
	}
	else if(QFileInfo(filename).suffix() == "vti")
	{
		vtkXMLImageDataReader * reader = vtkXMLImageDataReader::New();
		reader->SetFileName(filename.toLatin1().data());
		reader->Update();
		vtkSmartPointer<vtkImageData> res = reader->GetOutput();
		reader->Delete();
		return res;
	}
	else
	{
		//try to read ascii file
		QFile in(filename);
		if(!in.open(QFile::ReadOnly|QFile::Text))
			return vtkSmartPointer<vtkImageData>();

		qint64 pos = in.pos();

		//try to find the header size
		QString line;
		int header_line = 0;
		while(!in.atEnd())
		{
			line = in.readLine();

			double value;
			QTextStream temp(&line);
			temp >> value;
			if(temp.status() == QTextStream::Ok) break;
			header_line++;

		}

		//read header
		in.seek(pos);
		for(int i=0; i<header_line; ++i)
			in.readLine();

		//find number of lines
		QVector<double> vect;
		vect.reserve(1000);
		int lines = 0;
		if( !in.atEnd() )
		{
			while( true )
			{
				line = in.readLine();
				if( ! isLineEmpty(line) )
				{
					++lines;
					//read data and copy them into the vector
					QTextStream stream(&line);
					while(true)
					{
						double value;
						stream >> value;
						if(stream.status() == QTextStream::Ok)
							vect.push_back(value);
						else
							break;
					}

				}
				else
					break;
			}
		}
		else
			return vtkSmartPointer<vtkImageData>();

		if(vect.size() == 0 )
			return vtkSmartPointer<vtkImageData>();

		int column = vect.size() / (lines);

		vtkSmartPointer<vtkImageData> res = createVTKImage(column,lines,0);
		int i=0;
		for(int y=0; y < lines; ++y)
			{
				for(int x=0;x<column;++x,++i)
				{
					*static_cast<double*>(res->GetScalarPointer(x,y,0)) = vect[i];
				}
			}

		return res;
	}

	return vtkSmartPointer<vtkImageData>();;
}

QStringList VipVTKImage::imageSuffixes()
{
	return QStringList()<<"bmp"<<"png"<<"jpg"<<"jpeg"<<"tif"<<"tiff"<<"txt"<<"vti";
}

QStringList VipVTKImage::imageFilters()
{
	return QStringList()<<"*.bmp"<<"*.png"<<"*.jpg"<<"*.jpeg"<<"*.tif"<<"*.tiff"<<"*.txt"<<"*.vti";
}


VipVTKImage::VipVTKImage()
{
}

VipVTKImage::VipVTKImage(int w, int h, double value, int type)
{
	d_image =  (createVTKImage(w,h,value,type));
	VIP_VTK_OBSERVER(d_image);
}

VipVTKImage::VipVTKImage(int w, int h, uint pixel)
{
	d_image = (createVTKImage(w, h, qRed(pixel), qGreen(pixel), qBlue(pixel), qAlpha(pixel)));
	VIP_VTK_OBSERVER(d_image);
}


VipVTKImage::VipVTKImage(vtkImageData * img)
  : d_image(img)
{
	VIP_VTK_OBSERVER(d_image);
}

VipVTKImage::VipVTKImage(const QString & filename)
{
	d_image = (loadImageFileToVTK(filename));
	VIP_VTK_OBSERVER(d_image);
	if(!isNull())
	{
		d_info = QFileInfo(filename);
		d_name = d_info.fileName();
	}
}

int VipVTKImage::width() const
{
	if(d_image)
		return d_image->GetDimensions()[0];
	else
		return 0;
}

int VipVTKImage::height() const
{
	if(d_image)
		return d_image->GetDimensions()[1];
	else
		return 0;
}

int VipVTKImage::type() const
{
	return d_image->GetScalarType();
}

#include <fstream>

bool VipVTKImage::save(const QString & filename)
{
	if(isNull())
		return false;

	d_info = QFileInfo(filename);

	if(d_info.suffix() == "txt")
	{
		std::ofstream fout(filename.toLatin1().data());
		if(!fout)
			return false;

		for(int y=0; y < height(); ++y)
		{
			for(int x=0; x < width(); ++x)
			{
				fout<< this->doublePixelAt(x,y) << "\t";
			}

			fout << std::endl;
		}

		d_name = (d_info.fileName());
		return true;
	}
	else if(d_info.suffix() == "vti")
	{
		vtkXMLImageDataWriter * writer = vtkXMLImageDataWriter::New();
		writer->SetFileName(filename.toLatin1().data());
		writer->SetInputData(d_image);
		if( writer->Write())
		{
			d_name = (d_info.fileName());
			return true;
		}
	}
	else if(isRGBA())
	{
		QImage img = toQImage();
		if( img.save(filename))
		{
			d_name = (d_info.fileName());
			return true;
		}
	}

	d_info = QFileInfo();
	return false;
}

const QString & VipVTKImage::name() const
{
	return d_name;
}

const QFileInfo & VipVTKImage::info() const
{
	return d_info;
}

vtkImageData * VipVTKImage::image() const
{
	return const_cast<vtkSmartPointer<vtkImageData>&>(d_image).GetPointer();
}

bool VipVTKImage::isNull() const
{
	return !d_image.GetPointer();
}

bool VipVTKImage::isRGBA() const
{
	return d_image && image()->GetScalarType() == VTK_UNSIGNED_CHAR && image()->GetNumberOfScalarComponents() == 4;
}

void VipVTKImage::setOrigin(const QPoint & pt)
{
	if(d_image)
		d_image->SetOrigin(pt.x(),pt.y(),0);
}

QPoint VipVTKImage::origin() const
{
	if(d_image)
		return QPoint(qRound(d_image->GetOrigin()[0]),qRound(d_image->GetOrigin()[1]));
	else
		return QPoint(0,0);
}

uint VipVTKImage::RGBAPixelAt(int x, int y) const
{
	unsigned char * p = static_cast<unsigned char*>(d_image->GetScalarPointer(x,y,0)) ;
	return qRgba(p[0],p[1],p[2],p[3]);
}

double VipVTKImage::doublePixelAt(int x, int y) const
{
	return d_image->GetScalarComponentAsDouble(x,y,0,0);
}

void VipVTKImage::setRGBAPixelAt(int x, int y, uint pixel)
{
	unsigned char * p = static_cast<unsigned char*>(d_image->GetScalarPointer(x,y,0)) ;
	p[0] = qRed(pixel);
	p[1] = qGreen(pixel);
	p[2] = qBlue(pixel);
	p[3] = qAlpha(pixel);
}

void VipVTKImage::setDoublePixelAt(int x, int y, double value)
{
	d_image->SetScalarComponentFromDouble(x,y,0,0,value);
}

VipVTKImage VipVTKImage::zoom(double zoom_factor) const
{
	if(isNull())
		return VipVTKImage();

	double src_x=0, src_y=0;
	double dst_x = 0, dst_y = 0;
	double inv_zoom = 1/zoom_factor;

	if(zoom_factor > 1)
	{
		src_x = (width() - width()/zoom_factor)/2;
		src_y = (height() - height()/zoom_factor)/2;
	}
	else
	{
		dst_x = (width() - width()*zoom_factor)/2;
		dst_y = (height() - height()*zoom_factor)/2;
	}

	VipVTKImage res;

	if(isRGBA())
	{
		res = VipVTKImage(width(),height(),0);

		for(double x = dst_x; x < width()-dst_x; ++x)
			for(double y = dst_y; y < height()-dst_y; ++y)
			{
				uint val = this->RGBAPixelAt((int)(src_x + (x-dst_x)*inv_zoom), (int)(src_y + (y-dst_y)*inv_zoom) );
				res.setRGBAPixelAt((int)(x),(int)(y),val);
			}
	}
	else
	{
		res = VipVTKImage(width(),height(),0,type());

		for(double x = dst_x; x < width()-dst_x; ++x)
			for(double y = dst_y; y < height()-dst_y; ++y)
			{
				double val = this->doublePixelAt((int)(src_x + (x-dst_x)*inv_zoom), (int)(src_y + (y-dst_y)*inv_zoom) );
				res.setDoublePixelAt((int)(x),(int)(y),val);
			}
	}

	double offset_x = d_image->GetOrigin()[0]/zoom_factor;
	double offset_y = d_image->GetOrigin()[1]/zoom_factor;
	res.d_image->SetOrigin(offset_x,offset_y,0);
	return res;
}

VipVTKImage VipVTKImage::mirrored(bool horizontal , bool vertical ) const
{
	if(isNull())
		return VipVTKImage();

	VipVTKImage res;

	if(isRGBA())
	{
		res = VipVTKImage(width(),height(),0);

		for(int x=0; x < width(); ++x)
			for(int y=0; y < height(); ++y)
			{
				int new_x = horizontal ? width()-x-1 : x;
				int new_y = vertical ? height()-y-1 : y;
				uint val = this->RGBAPixelAt(new_x,new_y );
				res.setRGBAPixelAt(x,y,val);
			}
	}
	else
	{
		res = VipVTKImage(width(),height(),0,type());

		for(int x=0; x < width(); ++x)
			for(int y=0; y < height(); ++y)
			{
				int new_x = horizontal ? width()-x-1 : x;
				int new_y = vertical ? height()-y-1 : y;
				double val = this->doublePixelAt(new_x,new_y );
				res.setDoublePixelAt(x,y,val);
			}
	}

	return res;
}

VipVTKImage VipVTKImage::scaled(int width, int height, bool interpolate) const
{
	if(isNull())
		return VipVTKImage();

	vtkImageResize * resize = vtkImageResize ::New();
	resize->SetOutputDimensions(width,height,-1);
	resize->SetInterpolate(interpolate);
	resize->SetInputData(d_image);
	resize->Update();
	VipVTKImage res(resize->GetOutput());
	resize->Delete();

	return res;
}

QImage VipVTKImage::toQImage(vtkScalarsToColors *  s) const
{
	if(isNull())
		return QImage();

	if(isRGBA())
	{
		int w = width();
		int h = height();
		QImage img(w,h,QImage::Format_ARGB32);
		uint * ptr = reinterpret_cast<uint*>(img.bits());
		for(int y=0; y<h; ++y)
			for(int x=0; x<w; ++x)
			{
				*ptr = this->RGBAPixelAt(x,y);
				++ptr;
			}
		return img;
	}
	else if(s)
	{
		int w = width();
		int h = height();
		QImage img(w,h,QImage::Format_ARGB32);
		uint * ptr = reinterpret_cast<uint*>(img.bits());
		for(int y=0; y<h; ++y)
			for(int x=0; x<w; ++x)
			{
				double val = this->doublePixelAt(x,y);
				if(vtkMath::IsNan(val))
				{
					*ptr = qRgba(0,0,0,0);
				}
				else
				{
					double rgb[3];
					s->GetColor(val,rgb);
					*ptr = qRgb(rgb[0]*255,rgb[1]*255,rgb[2]*255);
				}
				++ptr;
			}
		return img;
	}

	return QImage();
}
