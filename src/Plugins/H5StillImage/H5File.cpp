#include "H5File.h"
#include "H5DeviceDriver.h"
#include "HDF5VideoFile.h"
#include "VipLogging.h"

#include <qfileinfo.h>

extern "C" {
#include <hdf5.h>
}

static hid_t qtToHDF5(int qt_type, int& dims)
{
	dims = 1;
	switch (qt_type) {
		case QMetaType::Bool:
			return H5T_NATIVE_UINT8;
		case QMetaType::Int:
			return H5T_NATIVE_INT32;
		case QMetaType::UInt:
			return H5T_NATIVE_UINT32;
		case QMetaType::Double:
			return H5T_NATIVE_DOUBLE;
		case QMetaType::Float:
			return H5T_NATIVE_FLOAT;
		case QMetaType::Long:
			return H5T_NATIVE_LONG;
		case QMetaType::LongLong:
			return H5T_NATIVE_INT64;
		case QMetaType::ULongLong:
			return H5T_NATIVE_UINT64;
		case QMetaType::Short:
			return H5T_NATIVE_INT16;
		case QMetaType::UShort:
			return H5T_NATIVE_UINT16;
		case QMetaType::Char:
			return H5T_NATIVE_INT8;
		case QMetaType::UChar:
			return H5T_NATIVE_UINT8;
		case QMetaType::SChar:
			return H5T_NATIVE_INT8;
		default: {
			if (qt_type == qMetaTypeId<complex_d>()) {
				dims = 2;
				return H5T_NATIVE_DOUBLE;
			}
			else if (qt_type == qMetaTypeId<complex_f>()) {
				dims = 2;
				return H5T_NATIVE_FLOAT;
			}
			else if (qt_type == qMetaTypeId<QImage>()) {
				dims = 4;
				return H5T_NATIVE_UINT8;
			}
			else if (qt_type == qMetaTypeId<QPixmap>()) {
				dims = 4;
				return H5T_NATIVE_UINT8;
			}
		}
	}

	if (qt_type > QMetaType::User) // consider short_float
		return H5T_NATIVE_FLOAT;
	return 0;
}

bool H5File::createFile(const QString& out_file, const QStringList& names, const QList<VipNDArray>& arrays)
{
	if (names.size() != arrays.size() || arrays.isEmpty())
		return false;

	hid_t file = H5Fcreate(out_file.toLatin1().data(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	if (file < 0) {
		VIP_LOG_ERROR("Cannot create output file " + QFileInfo(out_file).fileName());
		return false;
	}

	for (int i = 0; i < arrays.size(); ++i) {
		const VipNDArray& ar = arrays[i];
		const QString& name = names[i];
		int last_dims = 0;
		hid_t data_type = qtToHDF5(arrays[i].dataType(), last_dims);
		if (names[i].isEmpty() || data_type == 0 || arrays[i].isEmpty() || arrays[i].shapeCount() != 2) {
			VIP_LOG_WARNING("Cannot save array '" + names[i] + "'");
			continue;
		}

		int dim_count = last_dims == 1 ? 2 : 3;
		// create image dataset, alloc space for 1 image
		hsize_t dims[3] = { (hsize_t)ar.shape(0), (hsize_t)ar.shape(1), (hsize_t)last_dims };
		hid_t space = H5Screate_simple(dim_count, dims, dims);

		// Create the  dataset.
		hid_t set = H5Dcreate(file, name.toLatin1().data(), data_type, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if (set < 0) {
			VIP_LOG_WARNING("Cannot create data set for array '" + name + "'");
			continue;
		}

		// write the image

		// select hyperslab
		hsize_t offset[3] = { (hsize_t)0, (hsize_t)0, (hsize_t)0 };
		// space = H5Dget_space(set);
		H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);

		// define memory space
		hid_t memspace = H5Screate_simple(dim_count, dims, nullptr);

		// write data
		herr_t status = 0;
		if (ar.dataType() == qMetaTypeId<QImage>() || ar.dataType() == qMetaTypeId<QPixmap>()) {
			const QImage img = vipToImage(ar);
			status = H5Dwrite(set, data_type, memspace, space, H5P_DEFAULT, img.bits());
		}
		else {
			status = H5Dwrite(set, data_type, memspace, space, H5P_DEFAULT, ar.data());
		}

		if (status != 0) {
			VIP_LOG_ERROR("Cannot write image '" + name + "'");
			continue;
		}

		// close all
		H5Sclose(space);
		H5Sclose(memspace);
		H5Dclose(set);
	}

	H5Fclose(file);
	return true;
}

bool H5File::readFile(const QString& in_file, QStringList& names, QList<VipNDArray>& arrays)
{
	hid_t file = H5Fopen(in_file.toLatin1().data(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if (file < 0)
		return false;
	return readFile(file, names, arrays);
}


bool readDataSet(hid_t file, const QByteArray & name,  QStringList& names, QList<VipNDArray>& arrays) 
{
	// we found a dataset, open it and retrieve its dimensions
	hid_t set = H5Dopen(file, name.data(), H5P_DEFAULT);
	hid_t space = H5Dget_space(set);
	bool res = false;

	// int qt_type = 0;
	if (hid_t type = H5Dget_type(set)) {
		// if (int t = HDF5ToQt(type))
		//	qt_type = t;
		H5Tclose(type);
	}
	// TODO: use qt_type

	hsize_t dims[32];
	int rank = H5Sget_simple_extent_ndims(space);
	H5Sget_simple_extent_dims(space, dims, nullptr);
	if (rank == 2) {
		// 3 dimensions, might be an image dataset
		if (dims[0] > 0 && dims[1] > 0) {
			// store the data in a double image
			VipNDArray array = VipNDArray(QMetaType::Double, vipVector(dims[0], dims[1]));
			void* ptr = array.data();
			hid_t data_type = H5T_NATIVE_DOUBLE;

			hsize_t offset[2] = { 0, 0 };
			H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);

			// define memory space
			hid_t mem = H5Screate_simple(2, dims, nullptr);

			// read data
			herr_t status = H5Dread(set, data_type, mem, space, H5P_DEFAULT, ptr);

			H5Sclose(mem);

			if (status != 0) {
				VIP_LOG_WARNING("Cannot read data set '" + name + "'");
			}
			else {
				names.append(QString(name.data()));
				arrays.append(array);
				res = true;
			}
		}
	}
	else if (rank == 3) {
		if (dims[2] == 2) {
			// complex image
			VipNDArray array = VipNDArray(qMetaTypeId<complex_d>(), vipVector(dims[0], dims[1]));
			void* ptr = array.data();
			hid_t data_type = H5T_NATIVE_DOUBLE;

			hsize_t offset[3] = { 0, 0, 0 };
			H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);

			// define memory space
			hid_t mem = H5Screate_simple(3, dims, nullptr);

			// read data
			herr_t status = H5Dread(set, data_type, mem, space, H5P_DEFAULT, ptr);

			H5Sclose(mem);

			if (status != 0) {
				VIP_LOG_WARNING("Cannot read data set '" + name + "'");
			}
			else {
				names.append(QString(name.data()));
				arrays.append(array);
				res = true;
			}
		}
		else if (dims[2] == 4) {
			// ARGB image
			QImage img(dims[1], dims[0], QImage::Format_ARGB32);
			hid_t data_type = H5T_NATIVE_UINT8;

			hsize_t offset[3] = { 0, 0, 0 };
			H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);

			// define memory space
			hid_t mem = H5Screate_simple(3, dims, nullptr);

			// read data
			herr_t status = H5Dread(set, data_type, mem, space, H5P_DEFAULT, img.bits());

			H5Sclose(mem);

			if (status != 0) {
				VIP_LOG_WARNING("Cannot read data set '" + name + "'");
			}
			else {
				names.append(QString(name.data()));
				arrays.append(vipToArray(img));
				res = true;
			}
		}
	}

	H5Sclose(space);
	H5Dclose(set);
	return res;
}


bool readGroup(hid_t file, const QByteArray& name, QStringList& names, QList<VipNDArray>& arrays)
{
	hid_t group = H5Gopen(file, name.data(), 0);
	if (group <= 0)
		return false;

	H5G_info_t oinfo;
	herr_t ret = H5Gget_info(group, &oinfo);
	if (ret != 0) {
		H5Gclose(group);
		return false;
	}

	for (int i = 0; i < (int)oinfo.nlinks; ++i) {
		QByteArray name(50, 0);
		int size = H5Gget_objname_by_idx(group, i, name.data(), name.size());
		if (size > name.size()) {
			name = QByteArray(size, 0);
			H5Gget_objname_by_idx(group, i, name.data(), name.size());
		}
		H5G_obj_t type = H5Gget_objtype_by_idx(group, i);

		if (type == H5G_GROUP) {
			readGroup(group, name, names, arrays);
		}
		else if (type == H5G_DATASET) {
			readDataSet(group, name, names, arrays);
		}
		else
			bool stop = true;
	}

	H5Gclose(group);
	return true;
}

bool H5File::readFile(qint64 f_handle, QStringList& names, QList<VipNDArray>& arrays)
{
	// find all the image dataset names
	hid_t file = f_handle;

	H5G_info_t oinfo;
	herr_t ret = H5Gget_info(file, &oinfo);
	if (ret == 0) {
		for (int i = 0; i < (int)oinfo.nlinks; ++i) {
			QByteArray name(50, 0);
			int size = H5Gget_objname_by_idx(file, i, name.data(), name.size());
			if (size > name.size()) {
				name = QByteArray(size, 0);
				H5Gget_objname_by_idx(file, i, name.data(), name.size());
			}
			H5G_obj_t type = H5Gget_objtype_by_idx(file, i);

			if (type == H5G_GROUP) {
				readGroup(file, name, names, arrays);
			}
			else if (type == H5G_DATASET) {

				readDataSet(file, name, names, arrays);
			}
			else
				bool stop = true;
		}
	}

	H5Fclose(file);
	return arrays.size();
}

H5StillImageReader::H5StillImageReader()
{
	setOpenMode(NotOpen);
}

bool H5StillImageReader::open(VipIODevice::OpenModes mode)
{
	if (!(mode & ReadOnly))
		return false;

	m_array = VipMultiNDArray(); //.clear();
	QString file = removePrefix(path());
	QIODevice* dev = createDevice(file, QIODevice::ReadOnly);
	if (!dev)
		return false;
	QList<VipNDArray> arrays;
	QStringList names;
	if (dev && H5File::readFile(H5OpenQIODevice(dev), names, arrays)) {
		setOpenMode(mode);
		setDevice(nullptr);
		delete dev;
		if (arrays.size() == 1) {
			this->setData(QVariant::fromValue(VipNDArray(arrays.first())));
		}
		else {
			for (int i = 0; i < arrays.size(); ++i)
				m_array.addArray(names[i], arrays[i]);
			this->setData(QVariant::fromValue(VipNDArray(m_array)));
		}
		return true;
	}

	if (dev) {
		setDevice(nullptr);
		// delete dev;
	}

	return false;
}
/*
QStringList H5StillImageReader::availableImageNames() const
{
	return m_names;
}
QList<VipNDArray> H5StillImageReader::availableImages() const
{
	return m_arrays;
}
QString H5StillImageReader::currentImageName() const
{
	return m_current;
}
VipNDArray H5StillImageReader::currentImage() const
{
	return this->data().value<VipNDArray>();
}
void H5StillImageReader::setCurrentImageName(const QString & name)
{
	setData(QVariant());
	m_current = name;
	for (int i = 0; i < m_names.size(); ++i)
	{
		if (m_names[i] == name)
		{
			setData(QVariant::fromValue(m_arrays[i]));
			emitProcessingChanged();
			break;
		}
	}
}

*/

H5StillImageWriter::H5StillImageWriter() {}

H5StillImageWriter::~H5StillImageWriter()
{
	close();
}

bool H5StillImageWriter::open(VipIODevice::OpenModes mode)
{
	if (!(mode & WriteOnly))
		return false;

	setOpenMode(mode);
	return true;
}

void H5StillImageWriter::close()
{
	// actually write the data
	if (m_data.size())
		H5File::createFile(removePrefix(path()), m_data.keys(), m_data.values());
}

void H5StillImageWriter::apply()
{
	// add all available data into m_data, using the VipAnyData name as key
	while (inputAt(0)->hasNewData()) {
		VipAnyData any = inputAt(0)->data();
		VipNDArray ar = any.value<VipNDArray>();
		if (!ar.isEmpty() && ar.shapeCount() == 2) {
			if (vipIsMultiNDArray(ar)) {
				const QMap<QString, VipNDArray> arrays = VipMultiNDArray(ar).namedArrays();
				for (QMap<QString, VipNDArray>::const_iterator it = arrays.begin(); it != arrays.end(); ++it)
					m_data[it.key()] = it.value();
			}
			else {
				QString name = any.name();
				if (name.isEmpty())
					name = "unnamed_image";
				m_data[name] = ar;
			}
		}
	}
}
