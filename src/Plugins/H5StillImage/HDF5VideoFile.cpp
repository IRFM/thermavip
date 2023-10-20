#include "HDF5VideoFile.h"
#include "H5DeviceDriver.h"
#include <QDateTime>
#include <hdf5.h>
#include <limits>


static herr_t walk_errors(unsigned, const H5E_error2_t *err_desc,
	void *client_data)
{
	*static_cast<QString*>(client_data) = err_desc->desc;
	return -1;
}

static QString H5ErrorString()
{
	hid_t err = H5Eget_current_stack();
	if (err > 0)
	{
		QString desc;
		H5Ewalk(err, H5E_WALK_UPWARD, walk_errors, &desc);
		H5Eclose_stack(err);
		return desc;
	}
	return QString();
}

qint64 qtToHDF5(int qt_type)
{
	switch (qt_type)
	{
	case QMetaType::Bool: return H5T_NATIVE_UINT8;
	case QMetaType::Int: return H5T_NATIVE_INT32;
	case QMetaType::UInt: return H5T_NATIVE_UINT32;
	case QMetaType::Double: return H5T_NATIVE_DOUBLE;
	case QMetaType::Float: return H5T_NATIVE_FLOAT;
	case QMetaType::Long: return H5T_NATIVE_LONG;
	case QMetaType::ULong: return H5T_NATIVE_ULONG;
	case QMetaType::LongLong: return H5T_NATIVE_INT64;
	case QMetaType::ULongLong: return H5T_NATIVE_UINT64;
	case QMetaType::Short: return H5T_NATIVE_INT16;
	case QMetaType::UShort: return H5T_NATIVE_UINT16;
	case QMetaType::Char: return H5T_NATIVE_INT8;
	case QMetaType::UChar: return H5T_NATIVE_UINT8;
	case QMetaType::SChar: return H5T_NATIVE_INT8;
	default: break;
	}

	if (qt_type > QMetaType::User) //consider short_float
		return H5T_NATIVE_FLOAT;
	return 0;
}

int HDF5ToQt(qint64 hdf5_type)
{
	if (H5Tequal(hdf5_type, H5T_NATIVE_INT32)>0) return QMetaType::Int;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT32)>0) return QMetaType::UInt;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_DOUBLE)>0) return QMetaType::Double;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_FLOAT)>0)return QMetaType::Float;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_LONG)>0) return QMetaType::Long;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_ULONG)>0) return QMetaType::ULong;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_INT64)>0)return QMetaType::LongLong;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT64)>0) return QMetaType::ULongLong;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_INT16)>0)return QMetaType::Short;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT16)>0)return QMetaType::UShort;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_INT8)>0)return QMetaType::Char;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT8)>0) return QMetaType::UChar;

	else if (H5Tequal(hdf5_type, H5T_STD_I32BE)>0) return QMetaType::Int;
	else if (H5Tequal(hdf5_type, H5T_STD_U32BE)>0) return QMetaType::UInt;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F64BE)>0) return QMetaType::Double;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F32BE)>0)return QMetaType::Float;
	else if (H5Tequal(hdf5_type, H5T_STD_I64BE)>0)return QMetaType::LongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_U64BE)>0) return QMetaType::ULongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_I16BE)>0)return QMetaType::Short;
	else if (H5Tequal(hdf5_type, H5T_STD_U16BE)>0)return QMetaType::UShort;

	else if (H5Tequal(hdf5_type, H5T_STD_I32LE)>0) return QMetaType::Int;
	else if (H5Tequal(hdf5_type, H5T_STD_U32LE)>0) return QMetaType::UInt;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F64LE)>0) return QMetaType::Double;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F32LE)>0)return QMetaType::Float;
	else if (H5Tequal(hdf5_type, H5T_STD_I64LE)>0)return QMetaType::LongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_U64LE)>0) return QMetaType::ULongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_I16LE)>0)return QMetaType::Short;
	else if (H5Tequal(hdf5_type, H5T_STD_U16LE)>0)return QMetaType::UShort;

	else return 0;
}



struct hdf5id
{
protected:
	hdf5id(const hdf5id&);
	hdf5id & operator=(const hdf5id&);
public:
	hid_t id;
	bool own;
	hdf5id(hid_t _id = -1) :id(_id), own(true) {}
	operator hid_t() const { return id; }
	virtual void clear() {}
	virtual ~hdf5id() { clear(); }
};

struct hfile : hdf5id
{
	hfile(hid_t id = -1) :hdf5id(id) {}
	hfile & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Fclose(id); id = -1; }
	virtual ~hfile() { clear(); }
};

struct hset : hdf5id
{
	hset(hid_t id = -1) :hdf5id(id) {}
	hset & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Dclose(id); id = -1; }
	virtual ~hset() { clear(); }
};

struct hspace : hdf5id
{
	hspace(hid_t id = -1) :hdf5id(id) {}
	hspace & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Sclose(id); id = -1; }
	virtual ~hspace() { clear(); }
};

struct hprop : hdf5id
{
	hprop(hid_t id = -1) :hdf5id(id) {}
	hprop & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Pclose(id); id = -1; }
	virtual ~hprop() { clear(); }
};

struct hattr : hdf5id
{
	hattr(hid_t id = -1) :hdf5id(id) {}
	hattr & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Aclose(id); id = -1; }
	virtual ~hattr() { clear(); }
};

struct htype : hdf5id
{
	htype(hid_t id = -1) :hdf5id(id) {}
	htype & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Tclose(id); id = -1; }
	virtual ~htype() { clear(); }
};

struct hgroup : hdf5id
{
	hgroup(hid_t id = -1) :hdf5id(id) {}
	hgroup & operator=(hid_t _id) { clear(); this->id = _id; return *this; }
	virtual void clear() { if (id > 0 && own) H5Gclose(id); id = -1; }
	virtual ~hgroup() { clear(); }
};


struct DynAttribute
{
	QVector<int> intAttribute;
	QVector<double> doubleAttribute;
	QVector<qint64> int64Attribute;
	int type;
	DynAttribute() : type(0) {}
};


class HDF5VideoReader::PrivateData
{
public:
	QSize imageSize;
	QString imagesName;
	QString attributesName;

	qint64 count;
	qint64 pos;
	double fps;
	bool isTC;

	hfile file;
	hspace space;
	hset set;

	hset t_set;
	hspace t_space;

	hset a_set;
	hspace a_space;

	hgroup dynAttributes;
	QStringList dynSetNames;
	QList<DynAttribute> dynAttributeValues;

	PrivateData()
		:count(0), pos(0), fps(0), isTC(false){}
};


HDF5VideoReader::HDF5VideoReader(QObject * parent)
	:VipTimeRangeBasedGenerator(parent)
{
	m_data = new PrivateData();
	outputAt(0)->setData(VipNDArray());
}

HDF5VideoReader::~HDF5VideoReader()
{
	close();
	delete m_data;
}

bool HDF5VideoReader::open(VipIODevice::OpenModes mode)
{
	m_data->imagesName.clear();
	m_data->attributesName.clear();
	m_data->file.clear();
	m_data->set.clear();
	m_data->space.clear();
	m_data->t_set.clear();
	m_data->t_space.clear();
	m_data->a_set.clear();
	m_data->a_space.clear();
	m_data->imageSize = QSize(0, 0);
	m_data->pos = 0;
	m_data->count = 0;
	m_data->dynAttributeValues.clear();
	m_data->dynAttributes.clear();
	m_data->dynSetNames.clear();

	setOpenMode(NotOpen);

	QString filename = removePrefix(path());

	if (mode != VipIODevice::ReadOnly)
		return false;

	//m_data->file = H5OpenQIODevice(createDevice(filename, QIODevice::ReadOnly));
	m_data->file = H5Fopen(filename.toLatin1().data(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if (m_data->file.id < 0)
	{
		m_data->file = H5OpenQIODevice(createDevice(filename, QIODevice::ReadOnly));
	}

	if (m_data->file.id < 0)
	{
		close();
		return false;
	}

	//look for dynamic attributes
	m_data->dynAttributes = H5Gopen1(m_data->file, "dynamic_attributes");
	if (m_data->dynAttributes.id > 0)
	{
		//look for all datasets
		H5G_info_t oinfo;
		herr_t ret = H5Gget_info(m_data->dynAttributes, &oinfo);

		if (ret == 0)
		{
			for (int i = 0; i < (int)oinfo.nlinks; ++i)
			{
				QByteArray name(50, 0);
				int size = H5Gget_objname_by_idx(m_data->dynAttributes, i, name.data(), name.size());
				if (size > name.size())
				{
					name = QByteArray(size, 0);
					H5Gget_objname_by_idx(m_data->dynAttributes, i, name.data(), name.size());
				}
				H5G_obj_t type = H5Gget_objtype_by_idx(m_data->dynAttributes, i);

				if (type == H5G_DATASET)
				{
					hset set = H5Dopen(m_data->dynAttributes, name.data(), H5P_DEFAULT);
					hspace space = H5Dget_space(set);

					hsize_t dims[32];
					int rank = H5Sget_simple_extent_ndims(space);
					if (rank != 1)
						continue;
					H5Sget_simple_extent_dims(space, dims, nullptr);
					if (dims[0] == 0)
						continue;

					htype atype = H5Dget_type(set);
					htype   atype_mem = H5Tget_native_type(atype, H5T_DIR_ASCEND);
					hsize_t offset[1] = { (hsize_t)0 };
					DynAttribute attribute;
					if (H5Tequal(atype_mem.id, H5T_NATIVE_INT32) > 0)
					{
						attribute.intAttribute.resize(dims[0]);
						attribute.type = QVariant::Int;
						//read the whole timestamp vector

						H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);
						hspace mem = H5Screate_simple(1, dims, nullptr);
						herr_t status = H5Dread(set, atype_mem.id, mem, space, H5P_DEFAULT, attribute.intAttribute.data());
						if (status == 0)
						{
							m_data->dynAttributeValues.append(attribute);
							m_data->dynSetNames.append(name);
						}
					}
					else if (H5Tequal(atype_mem.id, H5T_NATIVE_INT64) > 0)
					{
						attribute.int64Attribute.resize(dims[0]);
						attribute.type = QVariant::LongLong;
						//read the whole timestamp vector

						H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);
						hspace mem = H5Screate_simple(1, dims, nullptr);
						herr_t status = H5Dread(set, atype_mem.id, mem, space, H5P_DEFAULT, attribute.int64Attribute.data());
						if (status == 0)
						{
							m_data->dynAttributeValues.append(attribute);
							m_data->dynSetNames.append(name);
						}
					}
					else if (H5Tequal(atype_mem.id, H5T_NATIVE_DOUBLE) > 0)
					{
						attribute.doubleAttribute.resize(dims[0]);
						attribute.type = QVariant::Double;
						//read the whole timestamp vector

						H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);
						hspace mem = H5Screate_simple(1, dims, nullptr);
						herr_t status = H5Dread(set, atype_mem.id, mem, space, H5P_DEFAULT, attribute.doubleAttribute.data());
						if (status == 0)
						{
							m_data->dynAttributeValues.append(attribute);
							m_data->dynSetNames.append(name);
						}
					}
				}
			}
		}
	}

	//find the image dataset name
	QString images_dataset;
	{
		H5G_info_t oinfo;
		herr_t ret = H5Gget_info(m_data->file, &oinfo);

		if (ret == 0)
		{
			for (int i = 0; i < (int)oinfo.nlinks; ++i)
			{
				QByteArray name(50, 0);
				int size = H5Gget_objname_by_idx(m_data->file, i, name.data(), name.size());
				if (size > name.size())
				{
					name = QByteArray(size, 0);
					H5Gget_objname_by_idx(m_data->file, i, name.data(), name.size());
				}
				H5G_obj_t type = H5Gget_objtype_by_idx(m_data->file, i);
				name = name.mid(0, size);
				if (type == H5G_DATASET)
				{
					//we found a dataset, open it and retrieve its dimensions
					hset set = H5Dopen(m_data->file, name.data(), H5P_DEFAULT);
					hspace space = H5Dget_space(set);

					hsize_t dims[32];
					int rank = H5Sget_simple_extent_ndims(space);
					H5Sget_simple_extent_dims(space, dims, nullptr);
					if (rank == 3)
					{
						//3 dimensions, might be an image dataset
						if (dims[2] > 0 && dims[1] > 1 && dims[0] > 1)
						{
							if (name == "image_error")
								continue;
							//1 image with width and height of at least 2 pixels: that's it
							images_dataset = (QString(name.data()));
							break;
						}
					}
				}
			}
		}
	}

	//open the images dataset
	if (images_dataset.isEmpty())
	{
		close();
		return false;
	}

	m_data->imagesName = images_dataset;


	m_data->space.clear();
	m_data->set.clear();
	m_data->set = H5Dopen(m_data->file, images_dataset.toLatin1().data(), H5P_DEFAULT);
	m_data->space = H5Dget_space(m_data->set);

	int rank;
	hsize_t dims[32];

	rank = H5Sget_simple_extent_ndims(m_data->space);
	H5Sget_simple_extent_dims(m_data->space, dims, nullptr);

	if (rank != 3)
	{
		close();
		return false;
	}

	m_data->count = dims[2];
	m_data->imageSize = QSize(dims[1], dims[0]);
	if (m_data->imageSize.width() == 0 || m_data->imageSize.height() == 0)
	{
		close();
		return false;
	}

	//open and read the timestamps dataset
	VipTimestamps timestamps(m_data->count, 0);

	m_data->t_set = H5Dopen(m_data->file, "timestamps", H5P_DEFAULT);
	m_data->t_space = H5Dget_space(m_data->t_set);
	rank = H5Sget_simple_extent_ndims(m_data->t_space);
	H5Sget_simple_extent_dims(m_data->t_space, dims, nullptr);
	if (rank != 1 || dims[0] != (unsigned)m_data->count)
	{
		//create dumy timestamps
		for (int i = 0; i < timestamps.size(); ++i)
			timestamps[i] = i;
		this->setTimestamps(timestamps);
	}
	else
	{
		//read the whole timestamp vector
		hsize_t t_dim[1] = { (hsize_t)m_data->count }, t_offset[1] = { (hsize_t)0 };
		hspace t_space = H5Dget_space(m_data->t_set);
		H5Sselect_hyperslab(t_space, H5S_SELECT_SET, t_offset, nullptr, t_dim, nullptr);
		hspace t_mem = H5Screate_simple(1, t_dim, nullptr);
		H5Dread(m_data->t_set, H5T_NATIVE_INT64, t_mem, t_space, H5P_DEFAULT, timestamps.data());

		//update timestamps unit if necessary
		/*if (m_data->count)
		{
		//if stored timestamps are bigger than current date, they are in nanoseconds, don't touch
		if (timestamps.first() > QDateTime::currentMSecsSinceEpoch())
		{
		}
		else
		{
		//convert from milli to nano seconds
		for (int i = 0; i < timestamps.size(); ++i)
		timestamps[i] *= qint64(1000000);
		}
		}*/

		this->setTimestamps(timestamps);
	}

	if (timestamps.size() && timestamps.first() != 0)
		this->setAttribute("Date", QDateTime::fromMSecsSinceEpoch(timestamps.first() / 1000000).toString("dd.MM.yyyy hh.mm.ss.zzz"));

	//update fps (in images/s)
	qint64 sampling = std::numeric_limits<qint64>::max();;
	for (int i = 1; i < m_data->count; ++i)
		sampling = qMin(sampling, timestamps[i] - timestamps[i - 1]);

	if (sampling)
		m_data->fps = (1.0 / sampling) * 1000000000.0;

	//load attributes(if any)
	AutoFindAttributesName();

	if (filename.contains("_temp_")) //for W7X movies, dirty way
		setAttribute("ZUnit", "Temperature (K)");

	if (timestamps.size())
		readData(timestamps.first());

	setOpenMode(mode);
	return true;
}

void HDF5VideoReader::close()
{
	m_data->imagesName.clear();
	m_data->attributesName.clear();
	m_data->file.clear();
	m_data->set.clear();
	m_data->space.clear();
	m_data->t_set.clear();
	m_data->t_space.clear();
	m_data->a_set.clear();
	m_data->a_space.clear();
	m_data->imageSize = QSize(0, 0);
	m_data->pos = 0;
	m_data->count = 0;
	m_data->dynAttributeValues.clear();
	m_data->dynAttributes.clear();
	m_data->dynSetNames.clear();
	setOpenMode(NotOpen);
	VipIODevice::close();
}

QSize HDF5VideoReader::imageSize() const
{
	return m_data->imageSize;
}

QString HDF5VideoReader::imageDataSet() const
{
	return m_data->imagesName;
}

QString HDF5VideoReader::attributeDataSet() const
{
	return m_data->attributesName;
}

bool HDF5VideoReader::AutoFindAttributesName()
{

	H5G_info_t oinfo;

	herr_t ret = H5Gget_info(m_data->file, &oinfo);

	if (ret != 0)
		return false;

	for (int i = 0; i < (int)oinfo.nlinks; ++i)
	{
		QByteArray name(50, 0);
		int size = H5Gget_objname_by_idx(m_data->file, i, name.data(), name.size());
		if (size > name.size())
		{
			name = QByteArray(size, 0);
			H5Gget_objname_by_idx(m_data->file, i, name.data(), name.size());
		}
		H5G_obj_t type = H5Gget_objtype_by_idx(m_data->file, i);

		if (type == H5G_DATASET)
		{
			//we found a dataset, check for attributes
			QVariantMap attributes = ReadAttributes(name.data());
			if (attributes.size())
			{
				this->mergeAttributes(attributes);
				m_data->attributesName = name;
				return true;
			}
		}

	}


	return false;
}

QVariantMap HDF5VideoReader::ReadAttributes(const QString & dataset_name) const
{
	hid_t set_id = H5Dopen(m_data->file, dataset_name.toLatin1().data(), H5P_DEFAULT);

	if (set_id < 0)
		return QVariantMap();

	hset tmp_set;
	tmp_set.id = set_id;

	QVariantMap res;
#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) >= 112
	H5O_info1_t oinfo;
#else
	H5O_info_t oinfo;
#endif

#if (H5_VERS_MAJOR*100 + H5_VERS_MINOR) <= 110
	herr_t ret = H5Oget_info(set_id, &oinfo);
#else
	herr_t ret = H5Oget_info2(set_id, &oinfo, H5O_INFO_ALL);
#endif
	if (ret != 0)
		return res;

	for (unsigned i = 0; i < (unsigned)oinfo.num_attrs; i++)
	{
		hattr attr = H5Aopen_by_idx(set_id, ".", H5_INDEX_CRT_ORDER, H5_ITER_INC, (hsize_t)i, H5P_DEFAULT, H5P_DEFAULT);
		QByteArray name(50, 0);
		ssize_t size = H5Aget_name(attr, name.size(), name.data());
		if (size >= name.size())
		{
			name = QByteArray(size, 0);
			H5Aget_name(attr, name.size(), name.data());
		}

		QString attribute_name = QString(name);
		QVariant attribute_value;
		//read the attributes value

		/*	hattr attr = H5Aopen_name(m_data->a_set,attribute_name.toLatin1().data());
		if(attr.id < 0)
		continue;
		*/
		htype atype = H5Aget_type(attr);
		H5T_class_t type_class = H5Tget_class(atype);

		if (type_class == H5T_STRING)
		{
			htype   atype_mem = H5Tget_native_type(atype, H5T_DIR_ASCEND);
			int _size = (int)H5Tget_size(atype_mem);
			QByteArray data(_size, 0);
			herr_t _ret = H5Aread(attr, atype_mem, data.data());
			if (_ret == 0)
			{
				attribute_value = QVariant(QString(data));
			}
		}
		else if (type_class == H5T_INTEGER)
		{
			qint64 value;
			herr_t _ret = H5Aread(attr, H5T_NATIVE_INT64, &value);
			if (_ret == 0)
			{
				attribute_value = QVariant(value);
			}
		}
		else if (type_class == H5T_FLOAT)
		{
			double value;
			herr_t _ret = H5Aread(attr, H5T_NATIVE_DOUBLE, &value);
			if (_ret == 0)
			{
				attribute_value = QVariant(value);
			}
		}

		if (attribute_value.userType() != 0)
			res.insert(attribute_name, attribute_value);

	}

	return res;
}

bool HDF5VideoReader::readData(qint64 time)
{
	qint64 pos = this->computeTimeToPos(time);
	if (pos < 0 || pos >= this->size())
		return false;

	int qt_type = QMetaType::Double;
	if (hid_t type = H5Dget_type(m_data->set))
	{
		H5T_order_t order = H5Tget_order(type);
		if (order == H5T_ORDER_LE)
			vip_debug("Little endian order \n");

		if (int t = HDF5ToQt(type))
			qt_type = t;
		H5Tclose(type);
	}

	//store the data in a double image
	VipNDArray ar(qt_type, vipVector(m_data->imageSize.height(), m_data->imageSize.width()));
	void * data = ar.data();
	hid_t data_type = qtToHDF5(ar.dataType());

	hsize_t dims[3] = { (hsize_t)m_data->imageSize.height(),(hsize_t)m_data->imageSize.width(),1 };

	//select hyperslab
	hspace space = H5Dget_space(m_data->set);
	hsize_t offset[3] = { 0,0,(hsize_t)pos };
	H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);


	//define memory space
	hspace mem = H5Screate_simple(3, dims, nullptr);

	//read data
	herr_t status = H5Dread(m_data->set, data_type, mem, space, H5P_DEFAULT, data);

	if (status != 0)
		return false;

	VipAnyData out = create(QVariant::fromValue(ar));

	//set the dynamic attributes
	for (int i = 0; i < m_data->dynAttributeValues.size(); ++i)
	{
		QString name = m_data->dynSetNames[i];
		DynAttribute attrs = m_data->dynAttributeValues[i];
		QVariant value;
		if (attrs.type == QVariant::Int && pos < attrs.intAttribute.size())
			value = attrs.intAttribute[pos];
		else if (attrs.type == QVariant::LongLong && pos < attrs.int64Attribute.size())
			value = attrs.int64Attribute[pos];
		else if (attrs.type == QVariant::Double && pos < attrs.doubleAttribute.size())
			value = attrs.doubleAttribute[pos];
		if (value.userType() != 0)
			out.setAttribute(name, value);
	}

	out.setTime(time);
	outputAt(0)->setData(out);

	return true;
}












class HDF5_ECRHVideoReader::PrivateData
{
public:
	QSize imageSize;
	QString imagesName;
	QString attributesName;

	qint64 count;
	qint64 pos;
	double fps;

	hfile file;
	hspace space;
	hset set;

	PrivateData()
		:count(0), pos(0), fps(0) {}
};


HDF5_ECRHVideoReader::HDF5_ECRHVideoReader(QObject * parent)
	:VipTimeRangeBasedGenerator(parent)
{
	m_data = new PrivateData();
	outputAt(0)->setData(VipNDArray());
}

HDF5_ECRHVideoReader::~HDF5_ECRHVideoReader()
{
	close();
	delete m_data;
}

bool HDF5_ECRHVideoReader::open(VipIODevice::OpenModes mode)
{
	m_data->imagesName.clear();
	m_data->attributesName.clear();
	m_data->file.clear();
	m_data->set.clear();
	m_data->space.clear();
	m_data->imageSize = QSize(0, 0);
	m_data->pos = 0;
	m_data->count = 0;

	setOpenMode(NotOpen);

	QString filename = removePrefix(path());

	if (mode != VipIODevice::ReadOnly)
		return false;

	m_data->file = H5OpenQIODevice(createDevice(filename, QIODevice::ReadOnly));
	//m_data->file =H5Fopen  (filename.toLatin1().data(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if (m_data->file.id < 0)
	{
		close();
		return false;
	}

	//look for content

	QString module_name;
	bool has_data = false;
	H5G_info_t oinfo;
	herr_t ret = H5Gget_info(m_data->file, &oinfo);


	if (ret == 0)
	{
		for (int i = 0; i < (int)oinfo.nlinks; ++i)
		{
			QByteArray name(50, 0);
			int size = H5Gget_objname_by_idx(m_data->file, i, name.data(), name.size());
			if (size > name.size())
			{
				name = QByteArray(size, 0);
				H5Gget_objname_by_idx(m_data->file, i, name.data(), name.size());
			}
			name = name.mid(0, size);
			H5G_obj_t type = H5Gget_objtype_by_idx(m_data->file, i);

			if (type == H5G_GROUP)
			{
				if (name.startsWith("Module"))
				{
					module_name = name;
				}
				else if (name == "data")
				{
					has_data = true;
				}
			}
		}
	}

	if (module_name.isEmpty() || !has_data)
	{
		setError("Wrong H5 format");
		close();
		return false;
	}


	QString images_dataset = "/data/" + module_name;
	QString timestamps_dataset = "/data/timestamps";

	//open the image data set
	m_data->space.clear();
	m_data->set.clear();
	m_data->set = H5Dopen(m_data->file, images_dataset.toLatin1().data(), H5P_DEFAULT);
	m_data->space = H5Dget_space(m_data->set);

	int full_rank;
	hsize_t full_dims[32];

	full_rank = H5Sget_simple_extent_ndims(m_data->space);
	H5Sget_simple_extent_dims(m_data->space, full_dims, nullptr);

	if (full_rank != 3)
	{
		close();
		return false;
	}

	m_data->count = full_dims[2];
	m_data->imageSize = QSize(full_dims[1], full_dims[0]);
	if (m_data->imageSize.width() == 0 || m_data->imageSize.height() == 0)
	{
		close();
		return false;
	}

	//open timestamps
	hset t_set = H5Dopen(m_data->file, timestamps_dataset.toLatin1().data(), H5P_DEFAULT);
	hspace t_space = H5Dget_space(t_set);
	full_rank = H5Sget_simple_extent_ndims(t_space);
	H5Sget_simple_extent_dims(t_space, full_dims, nullptr);
	if (full_rank != 1 || full_dims[0] != (unsigned)m_data->count)
	{
		close();
		return false;
	}
	else
	{
		//read the whole timestamp vector
		hsize_t t_dim[1] = { (hsize_t)m_data->count }, t_offset[1] = { (hsize_t)0 };
		hspace _t_space = H5Dget_space(t_set);
		H5Sselect_hyperslab(_t_space, H5S_SELECT_SET, t_offset, nullptr, t_dim, nullptr);
		hspace t_mem = H5Screate_simple(1, t_dim, nullptr);
		QVector<qint64> timestamps(m_data->count);
		H5Dread(t_set, H5T_NATIVE_INT64, t_mem, _t_space, H5P_DEFAULT, timestamps.data());
		this->setTimestamps(timestamps);

		if (timestamps.size() && timestamps.first() != 0)
			this->setAttribute("Date", QDateTime::fromMSecsSinceEpoch(timestamps.first() / 1000000).toString("dd.MM.yyyy hh.mm.ss.zzz"));
		//update fps (in images/s)
		qint64 sampling = std::numeric_limits<qint64>::max();;
		for (int i = 1; i < m_data->count; ++i)
			sampling = qMin(sampling, timestamps[i] - timestamps[i - 1]);

		if (sampling)
			m_data->fps = (1.0 / sampling) * 1000000000.0;
	}

	//open parameters

	hgroup params = H5Gopen(m_data->file, ("/" + module_name + "/parms").toLatin1().data(), H5P_DEFAULT);
	if (params.id < 0)
		params = H5Gopen(m_data->file, ("/" + module_name + "/params").toLatin1().data(), H5P_DEFAULT);
	if (params.id >= 0)
	{
		QVariantMap attributes;
		herr_t _ret = H5Gget_info(params, &oinfo);

		if (_ret == 0)
		{
			for (int i = 0; i < (int)oinfo.nlinks; ++i)
			{
				QByteArray name(50, 0);
				int size = H5Gget_objname_by_idx(params, i, name.data(), name.size());
				if (size > name.size())
				{
					name = QByteArray(size, 0);
					H5Gget_objname_by_idx(params, i, name.data(), name.size());
				}
				H5G_obj_t type = H5Gget_objtype_by_idx(params, i);
				name = name.mid(0, size);

				if (type == H5G_DATASET)
				{
					hset set = H5Dopen(params, name.data(), H5P_DEFAULT);
					hspace space = H5Dget_space(set);

					if (set.id < 0 || space.id < 0)
						continue;
					hsize_t dims[32];
					int rank = H5Sget_simple_extent_ndims(space);
					if (rank != 0)
						continue;
					H5Sget_simple_extent_dims(space, dims, nullptr);


					htype atype = H5Dget_type(set);
					htype   atype_mem = H5Tget_native_type(atype, H5T_DIR_ASCEND);
					hsize_t offset[1] = { (hsize_t)0 };

					if (H5Tequal(atype_mem.id, H5T_NATIVE_INT32) > 0)
					{
						qint32 value = 0;
						dims[0] = 1;
						H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);
						hspace mem = H5Screate_simple(1, dims, nullptr);
						herr_t status = H5Dread(set, atype_mem.id, mem, space, H5P_DEFAULT, &value);
						if (status == 0)
							attributes[name] = value;
					}
					else if (H5Tequal(atype_mem.id, H5T_NATIVE_INT64) > 0)
					{
						qint64 value = 0;
						dims[0] = 1;
						H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);
						hspace mem = H5Screate_simple(1, dims, nullptr);
						herr_t status = H5Dread(set, atype_mem.id, mem, space, H5P_DEFAULT, &value);
						if (status == 0)
							attributes[name] = value;
					}
					else if (H5Tequal(atype_mem.id, H5T_NATIVE_DOUBLE) > 0)
					{
						double value = 0;
						dims[0] = 1;
						H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);
						hspace mem = H5Screate_simple(1, dims, nullptr);
						herr_t status = H5Dread(set, atype_mem.id, mem, space, H5P_DEFAULT, &value);
						if (status == 0)
							attributes[name] = value;
					}
					else
					{
						H5T_class_t type_class = H5Tget_class(atype_mem);
						if (type_class == H5T_STRING)
						{
							/*int size = H5Tget_size(atype_mem);
							if (size)
							{
							QByteArray data(size, 0);
							herr_t status = H5Dread(set, atype_mem, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());
							if (status == 0)
							attributes[name] = QString(data);
							}*/
						}
					}


					/*int eq1 = H5Tequal(atype_mem.id, H5T_VARIABLE);
					int eq2 = H5Tequal(atype_mem.id, H5T_FORTRAN_S1);
					int eq3 = H5Tequal(atype_mem.id, H5T_STRING);
					bool stop = true;*/
				}
			}

			this->setAttributes(attributes);
		}
	}

	readData(firstTime());
	setOpenMode(mode);
	return true;
}

void HDF5_ECRHVideoReader::close()
{
	m_data->imagesName.clear();
	m_data->attributesName.clear();
	m_data->file.clear();
	m_data->set.clear();
	m_data->space.clear();
	m_data->imageSize = QSize(0, 0);
	m_data->pos = 0;
	m_data->count = 0;
	setOpenMode(NotOpen);
	VipIODevice::close();
}

QSize HDF5_ECRHVideoReader::imageSize() const
{
	return m_data->imageSize;
}


bool HDF5_ECRHVideoReader::readData(qint64 time)
{
	qint64 pos = this->computeTimeToPos(time);
	if (pos < 0 || pos >= this->size())
		return false;

	int qt_type = QMetaType::Double;
	if (hid_t type = H5Dget_type(m_data->set))
	{
		if (int t = HDF5ToQt(type))
			qt_type = t;
		H5Tclose(type);
	}

	//store the data in a double image
	VipNDArray ar(qt_type, vipVector(m_data->imageSize.height(), m_data->imageSize.width()));
	void * data = ar.data();
	hid_t data_type = qtToHDF5(ar.dataType());

	hsize_t dims[3] = { (hsize_t)m_data->imageSize.height(),(hsize_t)m_data->imageSize.width(),1 };

	//select hyperslab
	hspace space = H5Dget_space(m_data->set);
	hsize_t offset[3] = { 0,0,(hsize_t)pos };
	H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr, dims, nullptr);


	//define memory space
	hspace mem = H5Screate_simple(3, dims, nullptr);

	//read data
	herr_t status = H5Dread(m_data->set, data_type, mem, space, H5P_DEFAULT, data);

	if (status != 0)
		return false;

	VipAnyData out = create(QVariant::fromValue(ar));

	out.setTime(time);
	outputAt(0)->setData(out);

	return true;
}









class HDF5VideoWriter::PrivateData
{
public:
	int pixelType;
	int computedPixelType;
	QSize imageSize;
	QSize computedImageSize;
	QString imagesName;

	hfile file;
	hspace space;
	hset set;

	hset t_set;
	hspace t_space;

	hset a_set;
	hspace a_space;

	QStringList dynamicAttributeNames;
	QString dynamicAttributeGroup;
	bool recordAllDynamicAttributes;
	hgroup dynAttributes;
	hset dynSets[100];
	QVector<int> dynSetTypes;
	QStringList dynSetNames;

	PrivateData()
		:pixelType(0), computedPixelType(0), imagesName("images"), recordAllDynamicAttributes(0) {}
};

HDF5VideoWriter::HDF5VideoWriter(QObject * parent)
	:VipIODevice(parent)
{
	m_data = new PrivateData();
	this->setRecordAllDynamicAttributes(true);
}

HDF5VideoWriter::~HDF5VideoWriter()
{
	close();
	delete m_data;
}

void HDF5VideoWriter::setImagesName(const QString & name)
{
	m_data->imagesName = name;
	emitProcessingChanged();
}

const QString & HDF5VideoWriter::imagesName() const
{
	return m_data->imagesName;
}

void HDF5VideoWriter::setPixelType(int type)
{
	m_data->pixelType = type;
	emitProcessingChanged();
}

int HDF5VideoWriter::pixelType() const
{
	return m_data->pixelType;
}

void HDF5VideoWriter::setDynamicAttributeNames(const QStringList& names, const QString & dynamicAttributeGroup)
{
	m_data->dynamicAttributeNames = names;
	if (!dynamicAttributeGroup.isEmpty())
		m_data->dynamicAttributeGroup = dynamicAttributeGroup;
}

QStringList HDF5VideoWriter::dynamicAttributeNames() const
{
	return m_data->dynamicAttributeNames;
}

void HDF5VideoWriter::setRecordAllDynamicAttributes(bool enable, const QString & dynamicAttributeGroup)
{
	m_data->recordAllDynamicAttributes = enable;
	if (!dynamicAttributeGroup.isEmpty())
		m_data->dynamicAttributeGroup = dynamicAttributeGroup;
}
bool HDF5VideoWriter::recordAllDynamicAttributes() const
{
	return m_data->recordAllDynamicAttributes;
}

void HDF5VideoWriter::setImageSize(const QSize & size)
{
	m_data->imageSize = size;
	emitProcessingChanged();
}

const QSize & HDF5VideoWriter::imageSize() const
{
	return m_data->imageSize;
}

void HDF5VideoWriter::close()
{
	VipIODevice::close();
	m_data->file.clear();
	m_data->set.clear();
	m_data->space.clear();
	m_data->t_set.clear();
	m_data->t_space.clear();
	m_data->a_set.clear();
	m_data->a_space.clear();
	for (int i = 0; i < 100; ++i)
		m_data->dynSets[i].clear();
	m_data->dynAttributes.clear();
	m_data->dynSetTypes.clear();
	m_data->dynSetNames.clear();
}

bool HDF5VideoWriter::open(VipIODevice::OpenModes mode)
{
	close();

	QString filename = removePrefix(path());
	if (filename.isEmpty())
		return false;

	if (mode != VipIODevice::WriteOnly)
		return false;


	m_data->file = H5Fcreate(filename.toLatin1().data(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	if (m_data->file.id < 0)
	{
		close();
		return false;
	}

	this->setSize(0);
	this->setOpenMode(mode);
	return true;
}

void HDF5VideoWriter::apply()
{
	if (!isOpen())
		return;

	//write all available data
	while (true)
	{
		if (!inputAt(0)->hasNewData())
			break;

		VipAnyData in = inputAt(0)->data();
		if (in.isEmpty())
			break;


		VipNDArray ar = in.value<VipNDArray>();
		if (ar.isEmpty())
			break;

		if (size() == 0)
		{
			//write header and attributes on the first image

			//compute image size
			if (m_data->imageSize == QSize())
				m_data->computedImageSize = QSize(ar.shape(1), ar.shape(0));
			else
				m_data->computedImageSize = m_data->imageSize;

			//compute pixel type
			if (m_data->pixelType == 0)
				m_data->computedPixelType = ar.dataType();
			else
				m_data->computedPixelType = m_data->pixelType;

			//create image dataset, alloc space for 1 image
			hsize_t dims[3] = { (hsize_t)m_data->computedImageSize.height(),(hsize_t)m_data->computedImageSize.width(),(hsize_t)1 };
			hsize_t maxdims[3] = { (hsize_t)m_data->computedImageSize.height(),(hsize_t)m_data->computedImageSize.width(),H5S_UNLIMITED };
			hsize_t chunkdims[3] = { (hsize_t)m_data->computedImageSize.height(),(hsize_t)m_data->computedImageSize.width(),(hsize_t)1 };

			m_data->space = H5Screate_simple(3, dims, maxdims);
			hprop prop = H5Pcreate(H5P_DATASET_CREATE);
			H5Pset_chunk(prop, 3, chunkdims);
			//Create the chunked dataset.
			m_data->set = H5Dcreate(m_data->file, m_data->imagesName.toLatin1().data(), qtToHDF5(m_data->computedPixelType), m_data->space, H5P_DEFAULT, prop, H5P_DEFAULT);


			if (m_data->set.id < 0)
			{
				close();
				setError("wrong pixel type (" + QString(QMetaType::typeName(m_data->computedPixelType)) + ") or no space left on device");
				return;
			}

			hsize_t t_dims[1] = { 1 };
			hsize_t t_maxdims[1] = { H5S_UNLIMITED };
			hsize_t t_chunkdims[1] = { 1 };

			//create timestamp dataset
			m_data->t_space = H5Screate_simple(1, t_dims, t_maxdims);
			hprop t_prop = H5Pcreate(H5P_DATASET_CREATE);
			H5Pset_chunk(t_prop, 1, t_chunkdims);
			//Create the chunked dataset.
			m_data->t_set = H5Dcreate(m_data->file, "timestamps", H5T_NATIVE_INT64, m_data->t_space, H5P_DEFAULT, t_prop, H5P_DEFAULT);
			if (m_data->t_set.id < 0)
			{
				close();
				setError("cannot create 'timestamps' data set");
				return;
			}

			//create the attributes data set (in fact the image data set) and write the attributes
			m_data->a_set = m_data->set.id;
			m_data->a_set.own = false;

			QVariantMap attrs = this->attributes();
			QVariantMap img_attrs = in.attributes();
			for (QVariantMap::const_iterator it = img_attrs.begin(); it != img_attrs.end(); ++it)
				if (it.value().type() == QVariant::String)
					attrs[it.key()] = it.value();
			//we save the device attributes, as well as the first image string attributes (they are not saved as dynamic attributes)
			for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
			{
				QVariant value = it.value();
				QString name = it.key();
				herr_t err = 0;
				if (value.type() == QVariant::Int || value.type() == QVariant::UInt || value.type() == QVariant::Bool)
				{
					int v = value.value<int>();
					hspace aid = H5Screate(H5S_SCALAR);
					hattr attr = H5Acreate2(m_data->a_set, name.toLatin1().data(), H5T_NATIVE_INT, aid, H5P_DEFAULT, H5P_DEFAULT);
					err = H5Awrite(attr, H5T_NATIVE_INT, &v);
				}
				else if (value.type() == QVariant::Double)
				{
					double v = value.value<double>();
					hspace aid = H5Screate(H5S_SCALAR);
					hattr attr = H5Acreate2(m_data->a_set, name.toLatin1().data(), H5T_NATIVE_DOUBLE, aid, H5P_DEFAULT, H5P_DEFAULT);
					err = H5Awrite(attr, H5T_NATIVE_DOUBLE, &v);
				}
				else if (value.type() == QVariant::LongLong || value.type() == QVariant::ULongLong)
				{
					qint64 v = value.value<qint64>();
					hspace aid = H5Screate(H5S_SCALAR);
					hattr attr = H5Acreate2(m_data->a_set, name.toLatin1().data(), H5T_NATIVE_INT64, aid, H5P_DEFAULT, H5P_DEFAULT);
					err = H5Awrite(attr, H5T_NATIVE_INT64, &v);
				}
				else if (value.canConvert<QByteArray>())
				{
					QByteArray v = value.value<QByteArray>();
					htype   atype = H5Tcopy(H5T_C_S1);
					H5Tset_size(atype, v.size());
					H5Tset_strpad(atype, H5T_STR_NULLTERM);
					hspace aid = H5Screate(H5S_SCALAR);
					hattr attr = H5Acreate2(m_data->a_set, name.toLatin1().data(), atype, aid, H5P_DEFAULT, H5P_DEFAULT);
					if (attr != 0)
						err = H5Awrite(attr, atype, v.data());

				}

				if (err != 0)
				{
					close();
					setError("cannot write attribute '" + name + "': " + H5ErrorString());
					return;
				}

			}//end write attributes

			 //now, write the dynmaic attributes
			 //create the dynamic attribute group
			if (m_data->recordAllDynamicAttributes || m_data->dynamicAttributeNames.size())
			{
				QString group = m_data->dynamicAttributeGroup;
				if (group.isEmpty())
					group = "dynamic_attributes";
				m_data->dynAttributes = H5Gcreate(m_data->file, ("/" + group).toLatin1().data(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
				if (m_data->dynAttributes.id <= 0)
					setError("cannot create group'" + group + "' data set");
			}
			if (m_data->dynAttributes.id >= 0)
			{
				QStringList to_record;
				if (m_data->recordAllDynamicAttributes)
					to_record = img_attrs.keys();
				else
					to_record = m_data->dynamicAttributeNames;

				if (to_record.size())
				{
					int attr_count = 0;
					for (int i = 0; i < to_record.size(); ++i)
					{
						QVariant v = img_attrs[to_record[i]];

						if (v.userType() != 0)
						{
							//create the dataset
							hsize_t t_dims[1] = { 1 };
							hsize_t t_maxdims[1] = { H5S_UNLIMITED };
							hsize_t t_chunkdims[1] = { 1 };
							int qt_type = 0;
							if (v.type() == QVariant::Int || v.type() == QVariant::UInt || v.type() == QVariant::Bool)
								qt_type = QVariant::Int;
							else if (v.type() == QVariant::Double || v.userType() == QMetaType::Float)
								qt_type = QVariant::Double;
							else if (v.type() == QVariant::LongLong || v.type() == QVariant::ULongLong)
								qt_type = QVariant::LongLong;
							if (qt_type == 0)
								continue;

							hspace t_space = H5Screate_simple(1, t_dims, t_maxdims);
							hprop t_prop = H5Pcreate(H5P_DATASET_CREATE);
							H5Pset_chunk(t_prop, 1, t_chunkdims);
							//Create the chunked dataset.
							hset t_set = H5Dcreate(m_data->dynAttributes, to_record[i].toLatin1().data(), qtToHDF5(qt_type), t_space, H5P_DEFAULT, t_prop, H5P_DEFAULT);
							if (t_set.id <= 0)
							{
								setError("cannot create '" + to_record[i] + "' data set");
								continue;
							}

							t_set.own = false;
							m_data->dynSets[attr_count].id = t_set.id;
							m_data->dynSetTypes.append(qt_type);
							m_data->dynSetNames.append(to_record[i]);
							++attr_count;
						}
					}
				}
			}

		}//end size() == 0


		 //first, write image

		hsize_t dims[3];
		hsize_t dimsext[3] = { (hsize_t)m_data->computedImageSize.height(),(hsize_t)m_data->computedImageSize.width(),1 };
		m_data->space = H5Dget_space(m_data->set);
		H5Sget_simple_extent_dims(m_data->space, dims, nullptr);
		dims[2]++;

		if (this->size() > 0)
		{
			herr_t status = H5Dset_extent(m_data->set, dims);
			if (status != 0)
			{
				close();
				this->setError("cannot write image");
				return;
			}
		}

		//select hyperslab
		hsize_t offset[3] = { (hsize_t)0,(hsize_t)0,(hsize_t)this->size() };
		hspace space = H5Dget_space(m_data->set);
		H5Sselect_hyperslab(space, H5S_SELECT_SET, offset, nullptr,
			dimsext, nullptr);

		//define memory space
		hspace memspace = H5Screate_simple(3, dimsext, nullptr);
		//write data
		herr_t status = H5Dwrite(m_data->set, qtToHDF5(ar.dataType()), memspace, space,
			H5P_DEFAULT, ar.data());

		if (status != 0)
		{
			close();
			this->setError("cannot write image, " + H5ErrorString());
			return;
		}


		hsize_t t_dims[1], t_dimsext[1] = { 1 };
		hsize_t t_offset[1] = { (hsize_t)this->size() };
		m_data->t_space = H5Dget_space(m_data->t_set);
		H5Sget_simple_extent_dims(m_data->t_space, t_dims, nullptr);
		t_dims[0]++;
		if (this->size() > 0)
		{
			H5Dset_extent(m_data->t_set, t_dims);
		}

		//write timestamp
		hspace t_space = H5Dget_space(m_data->t_set);
		H5Sselect_hyperslab(t_space, H5S_SELECT_SET, t_offset, nullptr,
			t_dimsext, nullptr);
		hspace t_memspace = H5Screate_simple(1, t_dimsext, nullptr);
		// use a timestamp in nanoseconds!
		qint64 timestamp_nano = in.time();
		herr_t t_status = H5Dwrite(m_data->t_set, H5T_NATIVE_INT64, t_memspace, t_space,
			H5P_DEFAULT, &timestamp_nano);

		//write dynamic attributes
		if (m_data->dynAttributes.id > 0)
		{
			for (int i = 0; i < m_data->dynSetNames.size(); ++i)
			{
				if (this->size() > 0)
					H5Dset_extent(m_data->dynSets[i], t_dims);

				hspace t_space = H5Dget_space(m_data->dynSets[i]);
				H5Sselect_hyperslab(t_space, H5S_SELECT_SET, t_offset, nullptr,
					t_dimsext, nullptr);
				hspace t_memspace = H5Screate_simple(1, t_dimsext, nullptr);
				herr_t t_status = -1;
				int qt_type = m_data->dynSetTypes[i];
				if (qt_type == QVariant::Int)
				{
					int value = in.attribute(m_data->dynSetNames[i]).toInt();
					t_status = H5Dwrite(m_data->dynSets[i], qtToHDF5(qt_type), t_memspace, t_space,
						H5P_DEFAULT, &value);
				}
				else if (qt_type == QVariant::Double)
				{
					double value = in.attribute(m_data->dynSetNames[i]).toDouble();
					t_status = H5Dwrite(m_data->dynSets[i], qtToHDF5(qt_type), t_memspace, t_space,
						H5P_DEFAULT, &value);
				}
				else if (qt_type == QVariant::LongLong)
				{
					qint64 value = in.attribute(m_data->dynSetNames[i]).toLongLong();
					t_status = H5Dwrite(m_data->dynSets[i], qtToHDF5(qt_type), t_memspace, t_space,
						H5P_DEFAULT, &value);
				}

				if (t_status != 0)
					setError("Cannot write attribute '" + m_data->dynSetNames[i] + "'");
			}
		}


		if (status == 0 && t_status == 0)
		{
			this->setSize(this->size() + 1);
		}
	}

}
