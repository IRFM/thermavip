#ifndef HDF5_VIDEO_FILE_H
#define HDF5_VIDEO_FILE_H


#include "VipIODevice.h"
#include "VipNDArray.h"
#include "H5StillImageConfig.h"

H5_STILL_IMAGE_EXPORT qint64 qtToHDF5(int qt_type);
H5_STILL_IMAGE_EXPORT int HDF5ToQt(qint64 hdf5_type);

class H5_STILL_IMAGE_EXPORT HDF5VideoReader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT

	VIP_IO(VipOutput image)
	Q_CLASSINFO("description", "'*.h5' video file loader. Uses W7-X HDF5 video format")
	Q_CLASSINFO("category","reader/video")
public:

	HDF5VideoReader(QObject * parent = NULL);
	~HDF5VideoReader();

	virtual QString fileFilters() const {return "H5 video file (*.h5)" ;}
	virtual bool probe(const QString &filename, const QByteArray & ) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

	QSize imageSize() const;
	QString imageDataSet() const;
	QString attributeDataSet() const;

protected:
	virtual bool readData(qint64 time);

private:
	bool AutoFindAttributesName();
	QVariantMap ReadAttributes(const QString & name) const;

	class PrivateData;
	PrivateData * m_data;
};

//VIP_REGISTER_QOBJECT_METATYPE(HDF5VideoReader*)



class H5_STILL_IMAGE_EXPORT HDF5_ECRHVideoReader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT

		VIP_IO(VipOutput image)
		Q_CLASSINFO("description", "'*.h5' video file loader. Uses W7-X ECRH HDF5 video format")
		Q_CLASSINFO("category", "reader/video")
public:

	HDF5_ECRHVideoReader(QObject * parent = NULL);
	~HDF5_ECRHVideoReader();

	virtual QString fileFilters() const { return "H5 video file (*.h5)"; }
	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

	QSize imageSize() const;
	
protected:
	virtual bool readData(qint64 time);

private:
	class PrivateData;
	PrivateData * m_data;
};



class H5_STILL_IMAGE_EXPORT HDF5VideoWriter : public VipIODevice
{
	Q_OBJECT

	VIP_IO(VipInput image)
	Q_CLASSINFO("description", "'*.H5' video file writer. Uses W7-X HDF5 video format")
	Q_CLASSINFO("category","writer/video");

public:

	HDF5VideoWriter(QObject * parent = NULL);
	~HDF5VideoWriter();

	void setImagesName(const QString & name);
	const QString & imagesName() const;

	void setPixelType(int);
	int pixelType() const;

	void setImageSize(const QSize & size);
	const QSize & imageSize() const;

	void setDynamicAttributeNames(const QStringList& names, const QString & dynamicAttributeGroup = QString());
	QStringList dynamicAttributeNames() const;

	void setRecordAllDynamicAttributes(bool enable, const QString & dynamicAttributeGroup = QString());
	bool recordAllDynamicAttributes() const;


	virtual QString fileFilters() const {return "H5 video file (*.h5)" ;}
	virtual bool probe(const QString &filename, const QByteArray & ) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool acceptInput(int , const QVariant & v) const {return v.userType() == qMetaTypeId<VipNDArray>() && vipIsArithmetic(v.value<VipNDArray>().dataType());}

	virtual VipIODevice::OpenModes supportedModes() const {return WriteOnly;}
	virtual VipIODevice::DeviceType deviceType() const {return Temporal;}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

protected:
	virtual void apply();

private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(HDF5VideoWriter*)

#endif
