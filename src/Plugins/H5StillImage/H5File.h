#ifndef H5_FILE_H
#define H5_FILE_H


#include <qstring.h>
#include "H5StillImageConfig.h"
#include "VipMultiNDArray.h"
#include "VipIODevice.h"

class H5_STILL_IMAGE_EXPORT H5File
{
public:
	static bool createFile(const QString & out_file, const QStringList & names, const QList<VipNDArray> & arrays); 

	static bool readFile(qint64 file, QStringList& names, QList<VipNDArray> & arrays);
	static bool readFile(const QString & in_file, QStringList& names, QList<VipNDArray> & arrays);
};

/**
H5 still image reader.
This reader scans the full input H5 file to detect all valid images and load them all.
You can get all available images through #H5StillImageReader::availableImageNames() and #H5StillImageReader::availableImages().
To change the output image, call #H5StillImageReader::setCurrentImageName() and reload the device.

The class also defines the QObject property "availableImageNames", a QStringList containing all available images.
*/
class H5_STILL_IMAGE_EXPORT H5StillImageReader : public VipAnyResource
{
	Q_OBJECT
	Q_CLASSINFO("description", "Read a H5 file containing one or multiple images in separate data set")
	Q_CLASSINFO("category", "reader")

public:
	H5StillImageReader();
	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual QString fileFilters() const { return "H5 file (*.h5)"; }
	virtual bool open(VipIODevice::OpenModes mode);

private:
	VipMultiNDArray m_array;
};

//VIP_REGISTER_QOBJECT_METATYPE(H5StillImageReader*)

class H5_STILL_IMAGE_EXPORT H5StillImageWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)
	Q_CLASSINFO("description", "Write a H5 file containing one or multiple images in separate data set")
	Q_CLASSINFO("category", "writer")

public:
	H5StillImageWriter();
	~H5StillImageWriter();

	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual QString fileFilters() const { return "H5 file (*.h5)"; }
	virtual bool acceptInput(int, const QVariant & v) const { return (v.userType() == qMetaTypeId<VipNDArray>()); }
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

protected:
	virtual void apply();

private:
	QMap<QString,VipNDArray> m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(H5StillImageWriter*)


#endif