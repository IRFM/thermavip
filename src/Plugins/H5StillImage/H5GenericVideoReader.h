#ifndef H5_GENERIC_VIDEO_READER_H
#define H5_GENERIC_VIDEO_READER_H

#include "HDF5VideoFile.h"
#include "H5File.h"

class H5_STILL_IMAGE_EXPORT H5GenericVideoReader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT

	VIP_IO(VipOutput image)
	Q_CLASSINFO("description", "'*.h5' video/image file loader. Uses W7-X HDF5 video format")
	Q_CLASSINFO("category", "reader/video")
public:

	H5GenericVideoReader(QObject * parent = NULL);
	~H5GenericVideoReader();

	HDF5VideoReader * videoReader() const;
	H5StillImageReader * imageReader() const;
	HDF5_ECRHVideoReader * ecrhVideoReader() const;

	virtual QString fileFilters() const { return "H5 video/image file (*.h5)"; }
	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();

protected:
	virtual bool readData(qint64 time);

private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(H5GenericVideoReader*)

#endif