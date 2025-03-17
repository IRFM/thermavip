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

#include "VipH5Archive.h"
#include "VipNDArray.h"
#include "VipVectors.h"
#include "VipNDArrayImage.h"
#include "VipH5DeviceDriver.h"
#include "VipTimestamping.h"
#include "VipCore.h"
#include <QBuffer>
#include <QFile>

#include <set>
#include <vector>
extern "C" {
#include <hdf5.h>
}

static hid_t qtToHDF5(int qt_type)
{
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
		case QMetaType::ULong:
			return H5T_NATIVE_ULONG;
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
		default:
			return 0;
	}
	return 0;
}

int HDF5ToQt(qint64 hdf5_type)
{
	if (H5Tequal(hdf5_type, H5T_NATIVE_INT32) > 0)
		return QMetaType::Int;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT32) > 0)
		return QMetaType::UInt;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_DOUBLE) > 0)
		return QMetaType::Double;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_FLOAT) > 0)
		return QMetaType::Float;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_LONG) > 0)
		return QMetaType::Long;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_ULONG) > 0)
		return QMetaType::ULong;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_INT64) > 0)
		return QMetaType::LongLong;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT64) > 0)
		return QMetaType::ULongLong;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_INT16) > 0)
		return QMetaType::Short;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT16) > 0)
		return QMetaType::UShort;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_INT8) > 0)
		return QMetaType::Char;
	else if (H5Tequal(hdf5_type, H5T_NATIVE_UINT8) > 0)
		return QMetaType::UChar;

	else if (H5Tequal(hdf5_type, H5T_STD_I32BE) > 0)
		return QMetaType::Int;
	else if (H5Tequal(hdf5_type, H5T_STD_U32BE) > 0)
		return QMetaType::UInt;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F64BE) > 0)
		return QMetaType::Double;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F32BE) > 0)
		return QMetaType::Float;
	else if (H5Tequal(hdf5_type, H5T_STD_I64BE) > 0)
		return QMetaType::LongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_U64BE) > 0)
		return QMetaType::ULongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_I16BE) > 0)
		return QMetaType::Short;
	else if (H5Tequal(hdf5_type, H5T_STD_U16BE) > 0)
		return QMetaType::UShort;

	else if (H5Tequal(hdf5_type, H5T_STD_I32LE) > 0)
		return QMetaType::Int;
	else if (H5Tequal(hdf5_type, H5T_STD_U32LE) > 0)
		return QMetaType::UInt;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F64LE) > 0)
		return QMetaType::Double;
	else if (H5Tequal(hdf5_type, H5T_IEEE_F32LE) > 0)
		return QMetaType::Float;
	else if (H5Tequal(hdf5_type, H5T_STD_I64LE) > 0)
		return QMetaType::LongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_U64LE) > 0)
		return QMetaType::ULongLong;
	else if (H5Tequal(hdf5_type, H5T_STD_I16LE) > 0)
		return QMetaType::Short;
	else if (H5Tequal(hdf5_type, H5T_STD_U16LE) > 0)
		return QMetaType::UShort;

	else
		return 0;
}

/// Small class wrapping a hid_t in e ref counted way
class H5Object
{
public:
	/// HDF5 Object type
	enum Type
	{
		File,
		Set,
		Group,
		Space,
		Prop,
		Attr,
		H5Type
	};

private:
	struct Id
	{
		hid_t id;
		Type type;
		VIP_ALWAYS_INLINE ~Id()
		{
			if (id <= 0)
				return;
			switch (type) {
				case File:
					H5Fclose(id);
					break;
				case Set:
					H5Dclose(id);
					break;
				case Group:
					H5Gclose(id);
					break;
				case Space:
					H5Sclose(id);
					break;
				case Prop:
					H5Pclose(id);
					break;
				case Attr:
					H5Aclose(id);
					break;
				case H5Type:
					H5Tclose(id);
					break;
				default:
					break;
			}
		}
	};
	QSharedPointer<Id> d_id;

public:
	VIP_ALWAYS_INLINE H5Object() {}
	VIP_ALWAYS_INLINE H5Object(hid_t id, Type type)
	  : d_id(new Id{ id, type })
	{
	}
	VIP_DEFAULT_MOVE(H5Object);
	VIP_ALWAYS_INLINE operator hid_t() const noexcept
	{ 
		hid_t i = id();
		return i < 0 ? 0 : i;
	}
	VIP_ALWAYS_INLINE hid_t id() const noexcept { return d_id ? d_id->id : -1; }
	VIP_ALWAYS_INLINE Type type() const noexcept { return d_id ? d_id->type : File; }
	VIP_ALWAYS_INLINE bool isNull() const noexcept { return !d_id || d_id->id < 0; }
};

static H5Object stringType(const QByteArray& utf8)
{
	H5Object type(H5Tcopy(H5T_C_S1), H5Object::H5Type); 
	H5Tset_cset(type, H5T_CSET_UTF8);
	H5Tset_size(type, (size_t)utf8.size() + 1);
	return type;
}

struct ArraySpace
{
	H5Object space;
	hid_t type;
	const void* data;
};
static ArraySpace arraySpaceAndData(const VipNDArray& ar)
{
	ArraySpace res;
	
	if (vipIsImageArray(ar)) {
		int dim_count = 3;
		hsize_t dims[3] = { (hsize_t)ar.shape(0), (hsize_t)ar.shape(1), 4 };
		res.space = H5Object(H5Screate_simple(dim_count, dims, dims), H5Object::Space);
		res.data = vipToImage(ar).constBits();
		res.type = qtToHDF5(QMetaType::UChar);
	}
	else if (vipIsComplex(ar.dataType())) {
		int dim_count = ar.shapeCount() + 1;
		hsize_t dims[VIP_MAX_DIMS + 1];
		for (int i = 0; i < dim_count-1; ++i)
			dims[i] = (hsize_t)ar.shape(i);
		dims[dim_count - 1] = 2;
		res.space = H5Object(H5Screate_simple(dim_count, dims, dims), H5Object::Space);
		res.data = ar.data();
		if (ar.dataType() == qMetaTypeId<complex_f>())
			res.type = qtToHDF5(QMetaType::Float);
		else
			res.type = qtToHDF5(QMetaType::Double);
	}
	else {
		int dim_count = ar.shapeCount();
		hsize_t dims[VIP_MAX_DIMS];
		for (int i = 0; i < dim_count; ++i)
			dims[i] = (hsize_t)ar.shape(i);
		res.space = H5Object(H5Screate_simple(dim_count, dims, dims), H5Object::Space);
		res.data = ar.data();
		res.type = qtToHDF5(ar.dataType());
	}
	return res;
}

static bool writeAttribute(hid_t loc, const char* name, const QVariant& value)
{
	if (vipIsArithmetic(value.userType())) {
		auto type = qtToHDF5(value.userType());
		if (!type)
			return false;
		H5Object space(H5Screate(H5S_SCALAR), H5Object::Space);
		H5Object attr(H5Acreate(loc, name, type, space, H5P_DEFAULT, H5P_DEFAULT), H5Object::Attr);
		if (!attr)
			return false;
		if (H5Awrite(attr.id(), type, value.data()) != 0)
			return false;
		return true;
	}

	if (value.userType() == QMetaType::QString || value.userType() == QMetaType::QByteArray) {

		if (value.userType() == QMetaType::QString)
		{
			QByteArray utf8 = value.toString().toUtf8();
			H5Object space(H5Screate(H5S_SCALAR), H5Object::Space);
			if (!space)
				return false;
			H5Object type = stringType(utf8);
			H5Object attr(H5Acreate(loc, name, type, space, H5P_DEFAULT, H5P_DEFAULT), H5Object::Attr);
			if (H5Awrite(attr.id(), type, utf8.data()) != 0)
				return false;
			return true;
		}

		const QByteArray val = (value.userType() == QMetaType::QString) ? value.toString().toLatin1() : value.toByteArray();
		auto type = H5T_NATIVE_UINT8;
		int dim_count = 1;
		hsize_t dims[1] = { (hsize_t)val.size() };
		H5Object space(H5Screate_simple(dim_count, dims, dims), H5Object::Space);
		if (!space)
			return false;
		H5Object attr(H5Acreate(loc, name, type, space.id(), H5P_DEFAULT, H5P_DEFAULT), H5Object::Attr);
		if (!attr)
			return false;
		if (H5Awrite(attr.id(), type, val.data()) != 0)
			return false;
		return true;
	}

	// Convert to VipNDArray
	VipNDArray ar = value.value<VipNDArray>().dense();
	if (ar.isNull())
		return false;
	ArraySpace aspace = arraySpaceAndData(ar);
	if (!aspace.space)
		return false;
	H5Object attr(H5Acreate(loc, name, aspace.type, aspace.space, H5P_DEFAULT, H5P_DEFAULT), H5Object::Attr);
	if (!attr)
		return false;
	if (H5Awrite(attr.id(), aspace.type, aspace.data) != 0)
		return false;
	return true;
}

QVariant readAttribute(hid_t id, const char* attr_name)
{
	H5Object attr(H5Aopen(id, attr_name, H5P_DEFAULT), H5Object::Attr);
	if (!attr)
		return QVariant();
	H5Object type(H5Aget_type(attr), H5Object::H5Type);
	if (!type)
		return QVariant();
	H5Object space(H5Aget_space(attr), H5Object::Space);
	if (!space)
		return QVariant();

	H5T_class_t type_class = H5Tget_class(type);
	if (type_class == H5T_STRING) {
		H5Object atype_mem(H5Tget_native_type(type, H5T_DIR_ASCEND),H5Object::H5Type);
		int _size = (int)H5Tget_size(atype_mem);
		QByteArray data(_size, 0);
		herr_t _ret = H5Aread(attr, atype_mem, data.data());
		if (_ret == 0) {
			if (data.size() && data[data.size() - 1] == (char)0)
				data.chop(1);
			return QVariant(QString::fromUtf8(data));
		}
		return QVariant();
	}

	int qt_type = HDF5ToQt(type);
	hsize_t dims[32];
	int rank = H5Sget_simple_extent_ndims(space);
	H5Sget_simple_extent_dims(space, dims, nullptr);

	if (!vipIsArithmetic(qt_type))
		return QVariant();
	if (rank > VIP_MAX_DIMS)
		return QVariant();

	// strings
	if (qt_type == QMetaType::UChar && rank == 1) {
		QByteArray ar((int)dims[0],(char)0);
		H5Aread(attr, type, ar.data());
		return QVariant::fromValue(ar);
	}

	if ((rank == 1 && dims[0] == 1) || rank == 0) {
		qint64 data = 0;
		QVariant v = vipFromVoid(qt_type, &data);
		H5Aread(attr, type, (void*)v.data());
		return v;
	}

	if (rank == 3 && dims[2] == 4 && qt_type == QMetaType::UChar) {
		// RGB image
		QImage img((int)dims[1], (int)dims[0], QImage::Format_ARGB32);
		H5Aread(attr, type, (void*)img.bits());
		return QVariant::fromValue(vipToArray(img));
	}

	// Generic ND array
	VipNDArrayShape sh;
	sh.resize(rank);
	for (int i = 0; i < rank; ++i)
		sh[i] = (qsizetype)dims[i];
	VipNDArray ar(qt_type, sh);
	H5Aread(attr, type, ar.data());
	return QVariant::fromValue(ar);
}

using Content = QPair<QByteArray, H5Object::Type>;
using GroupContent = QVector<Content >;

static herr_t iterate(hid_t group, const char* name, const H5L_info_t* info, void* op_data)
{
	GroupContent& res = *static_cast<GroupContent*>(op_data);

	H5G_stat_t stat;
	/* herr_t err =*/ H5Gget_objinfo(group, name, false, &stat);
	auto h5type = H5Object::File;
	if (stat.type == H5G_DATASET)
		h5type = H5Object::Set;
	else if ((int)stat.type == H5G_GROUP || (int)info->type == H5G_LINK)
		h5type = H5Object::Group;
	else if (stat.type == H5G_TYPE)
		h5type = H5Object::H5Type;
	if (h5type != H5Object::File) {
		res.push_back( { name, h5type } );
	}
	return 0;
}


static GroupContent listGroupContent(QIODevice * device, const QByteArray & name, const H5Object& obj)
{
	GroupContent res;
	if (!obj)
		return res;
	if (obj.type() == H5Object::Group || obj.type() == H5Object::File) {

		if (!name.isEmpty() && name != "/") {
			// For non root, try to use H5Literate
			// with H5_INDEX_CRT_ORDER to read
			// group content by creation order.
			auto status = H5Literate(obj,
						 H5_INDEX_CRT_ORDER, // Note this argument
						 H5_ITER_INC,
						 NULL,
						 iterate,
						 &res);
			if(status == 0)
				return res;
		}

		H5G_info_t oinfo;
		herr_t ret = H5Gget_info(obj.id(), &oinfo);
		if (ret != 0)
			return res;

		// Read group content with H5Gget_objname_by_idx().
		// We use the underlying device position to assess
		// the creation order (not always valid).
		std::vector<QPair<qint64, Content>> content;
		for (int i = 0; i < (int)oinfo.nlinks; ++i) {

			QByteArray name(50, 0);
			int size = H5Gget_objname_by_idx(obj.id(), i, name.data(), name.size());
			if (size > name.size()) {
				name = QByteArray(size, 0);
				H5Gget_objname_by_idx(obj.id(), i, name.data(), name.size());
			}
			name.resize(size);

			H5G_stat_t stat; 
			herr_t err = H5Gget_objinfo(obj, name.data(), false, &stat);
			auto pos = device->pos();

			if (err == 0) {
				H5Object::Type h5type = H5Object::File;
				if (stat.type == H5G_DATASET)
					h5type = H5Object::Set;
				else if (stat.type == H5G_GROUP || stat.type == H5G_LINK)
					h5type = H5Object::Group;
				else if (stat.type == H5G_TYPE)
					h5type = H5Object::H5Type;
				if (h5type != H5Object::File) {
					content.push_back({ pos, { name, h5type } });
				}
			}

		}
		std::sort(content.begin(), content.end(), [](const auto& l, const auto& r) { return l.first < r.first; });
		res.resize(content.size());
		for (size_t i = 0; i < content.size(); ++i)
			res[(qsizetype)i] = std::move(content[i].second);
	}
	return res;
}

static QVector<QByteArray> listAttributes(const H5Object& obj, const char * obj_name)
{
	QVector<QByteArray> res;
	
	hsize_t count = (hsize_t)H5Aget_num_attrs(obj.id());	
	for (hsize_t i = 0; i < count; ++i) {
		QByteArray name(50, 0);
		int size = H5Aget_name_by_idx(obj.id(), obj_name, H5_INDEX_CRT_ORDER, H5_ITER_INC, i, name.data(), name.size(), H5P_DEFAULT);
		if (size > 0) {
			if (size > name.size()) {
				name.resize(size);
				H5Aget_name_by_idx(obj.id(), obj_name, H5_INDEX_CRT_ORDER, H5_ITER_INC, i, name.data(), name.size(), H5P_DEFAULT);
			}
			name.resize(size);
			res.push_back(name);
		}
	}
	return res;
}


class VipH5Archive::PrivateData
{
public:
	struct Position
	{
		QByteArray name;		// full group name from root
		H5Object group;			// group object
		QByteArray last;		// last read/write position (dataset or group name)
		GroupContent content;	// group content
		std::map<QByteArray, qint64> content_names;
		QHash<QByteArray, GroupContent>* groups_contents{ nullptr };
		void populate(QIODevice * device)
		{
			if (content.isEmpty()) {
				auto it = groups_contents->find(name);
				if (it != groups_contents->end())
					content = it.value();
				else {
					content = listGroupContent(device,name, group);
					groups_contents->insert(name, content);
				}
				for (qsizetype i = 0; i < content.size(); ++i)
					content_names[content[i].first] = i;
			}
		}
		void addContent(const QByteArray& name, H5Object::Type type) 
		{ 
			content.push_back({ name, type });
			content_names[name] = content.size()-1;
		}
		QByteArray createUniqueName(const QByteArray & name) const
		{ 
			auto it = content_names.find(name);
			if (it == content_names.end())
				return name;
			// Find last id
			unsigned id = 0;
			++it;
			QByteArray prefix = name + "%";
			for (; it != content_names.end(); ++it) {
				if (!it->first.startsWith(prefix))
					break;
				id = std::max((unsigned)it->first.mid(prefix.size()).toLongLong(), id);
			}
			QByteArray id_ar = QByteArray::number(id+1);
			if (id_ar.size() != 9)
				id_ar = QByteArray(9 - id_ar.size(), (char)'0') + id_ar;
			return prefix + id_ar;
		}
		qint64 indexOf(const QByteArray& name) const noexcept{ 
			if (name.isEmpty())
				return -1;
			for (qsizetype i = 0; i < content.size(); ++i)
				if (content[i].first == name)
					return i;
			return -1;
		}
		static bool isName(const QByteArray& full_name, const QByteArray& prefix) noexcept
		{ 
			if (full_name.startsWith(prefix)) {
				if (full_name.size() == prefix.size() || prefix.isEmpty())
					return true;
				return full_name[prefix.size()] == '%';
			}
			return false;
		}
		qint64 firstOf(H5Object::Type type, const QByteArray & prefix, qint64 start = 0) const noexcept {
			for (qsizetype i = (qsizetype)start; i < content.size(); ++i)
				if (content[i].second == type && isName(content[i].first, prefix))
					return i;
			return -1;
		}
		qint64 nextDataByName(hid_t file_id, const QByteArray& dataname, qint64 start = 0) const
		{
			for (; start != (qint64)content.size(); ++start) {
					if (!dataname.isEmpty() && !isName(content[start].first , dataname))
						continue;
				if (content[start].second == H5Object::Set)
					return start;
				else {
					H5Object gr(H5Gopen1(file_id, this->name + "/" + content[start].first), H5Object::Group);
					if (!gr) 
						continue;
					QString type_name = readAttribute(gr, "type_name").toString();
					if (type_name.isEmpty()) {
						continue;
					}
					return start;
				}
			}
			return -1;
		}
	};
	using Positions = QVector<Position>;
	H5Object file;
	QIODevice* device{ nullptr };
	Positions position;
	QByteArray last_end;
	QVector<Positions> save;
	QHash<QByteArray, GroupContent> groups_contents; // in read only mode, store the groups contents to avoid recomputing it (as order is wrong the second time)
	qint64 dpos{ 0 };
};

VipH5Archive::VipH5Archive()
  : VipArchive(Binary, MetaDataOnContent | MetaDataOnNodeStart)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}
VipH5Archive::VipH5Archive(QIODevice* d)
  : VipH5Archive()
{
	open(d);
}
VipH5Archive::VipH5Archive(QByteArray* a, QIODevice::OpenMode mode)
  : VipH5Archive()
{
	open(a, mode);
}
VipH5Archive::VipH5Archive(const QByteArray& a)
  : VipH5Archive()
{
	open(a);
}
VipH5Archive::VipH5Archive(const QString& filename, QIODevice::OpenMode mode)
  : VipH5Archive()
{
	open(filename, mode);
}

VipH5Archive::~VipH5Archive()
{
	close();
}

bool VipH5Archive::open(QByteArray* a, QIODevice::OpenMode mode)
{
	close();
	QBuffer* buf = new QBuffer(a, this);
	buf->open(mode);
	return open(buf);
}

bool VipH5Archive::open(const QByteArray& a)
{
	QBuffer* buf = new QBuffer(this);
	buf->setData(a);
	buf->open(QBuffer::ReadOnly);
	return open(buf);
}
	
bool VipH5Archive::open(const QString& filename, QIODevice::OpenMode mode) 
{
	QFile* file = new QFile(filename, this);
	if (!file->open(mode)) {
		delete file;
		return false;
	}
	return open(file);
}

bool VipH5Archive::open(QIODevice* d)
{
	if (device() == d)
		return true;

	close();
	if (!d->isOpen())
		return false;
	
	d_data->device = d;
	d_data->file = H5Object(vipH5OpenQIODevice(d), H5Object::File);
	if (!d_data->file.isNull()) {
		if (d->openMode() & QIODevice::WriteOnly)
			this->setMode(Write);
		else 
			this->setMode(Read);
		// add root
		d_data->position.push_back({ "", d_data->file, QByteArray(), GroupContent() });
		d_data->position.back().groups_contents = &d_data->groups_contents;
		d_data->position.back().populate(device());

		if (mode() == Read) {
			setRange(0, d->size());
		}
		return true;
	}
	else
		d_data->device = nullptr;
	return false;
}
QIODevice* VipH5Archive::device() const
{
	return d_data->device;
}
	
void VipH5Archive::close()
{
	if (d_data->device) {
		d_data->position.clear();
		d_data->save.clear();
		d_data->file = H5Object();
		if (d_data->device->parent() == this)
			delete d_data->device;
		d_data->device = nullptr;
		setMode(NotOpen);
	}
}

void VipH5Archive::doSave()
{
	d_data->save.push_back(d_data->position);
}
void VipH5Archive::doRestore()
{
	if (d_data->save.isEmpty()) {
		setError("Unbalanced VipH5Archive::save/restore");
		return;
	}
	d_data->position = d_data->save.back();
	d_data->save.pop_back();
}

QByteArray VipH5Archive::currentGroup() const noexcept
{
	return d_data->position.back().name;
}
QByteArray VipH5Archive::lastEndGroup() const noexcept
{
	return d_data->last_end;
}
VipH5Archive::Content VipH5Archive::currentGroupContent() const
{
	const auto vec = d_data->position.back().content;
	VipH5Archive::Content res;
	for (qsizetype i = 0; i < vec.size(); ++i) {
		auto t = vec[i].second;
		Type type = None;
		if (t == H5Object::Group)
			type = VipH5Archive::Group;
		else if (t == H5Object::Set)
			type = VipH5Archive::DataSet;
		if (type != None) {
			res.push_back( Object(vec[i].first, type));
		}
	}
	return res;
}
QByteArray VipH5Archive::lastRead() const noexcept
{
	return d_data->position.back().last;
}

void VipH5Archive::doStart(QString& name, QVariantMap& metadata, bool read_metadata)
{
	// Start a group
	QByteArray gr_name = d_data->position.back().name + "/" + name.toLatin1();
	
	if (mode() == Write) {

		// Ensure the group name is unique
		QByteArray unique_name = d_data->position.back().createUniqueName(name.toLatin1());
		if (unique_name.size() != name.size()) {
			name = unique_name;
			gr_name = d_data->position.back().name + "/" + unique_name;
		}

		hid_t group_creation_plist;
		group_creation_plist = H5Pcreate(H5P_GROUP_CREATE);
		/* herr_t status =*/ H5Pset_link_creation_order(group_creation_plist, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED);
		// Favor compact group
		/* status =*/ H5Pset_link_phase_change(group_creation_plist, 100, 100);

		// Create group
		H5Object gr(H5Gcreate(d_data->file.id(), gr_name.data(), H5P_DEFAULT, group_creation_plist, H5P_DEFAULT), H5Object::Group);
		H5Pclose(group_creation_plist);

		if (!gr) {
			setError("Cannot create H5 group " + gr_name);
			return;
		}
		d_data->position.back().addContent( name.toLatin1(), H5Object::Group );
		d_data->position.push_back(PrivateData::Position{ gr_name, gr, QByteArray(), GroupContent() });
		d_data->position.back().groups_contents = &d_data->groups_contents;

		if (metadata.size()) {
			// Write metadata
			for (auto it = metadata.begin(); it != metadata.end(); ++it)
				writeAttribute(gr.id(), it.key().toLatin1().data(), it.value());
		}
	}
	else {

		// open next group
		PrivateData::Position& pos = d_data->position.back();
		qint64 idx = -1;

		QByteArray bname = name.toLatin1();
		idx = pos.firstOf(H5Object::Group, bname, pos.indexOf(pos.last) + 1);
		if (idx < 0) {
			setError("Unable to open next H5 group");
			return;
		}
		
		gr_name = pos.name + "/" + pos.content[idx].first;
		if (name.isEmpty()) {
			const QByteArray ar = pos.content[idx].first;
			int index = ar.indexOf('%');
			if (index < 0)
				name = ar;
			else
				name = ar.mid(0, index);
		}
		
		H5Object gr(H5Gopen1(d_data->file.id(), gr_name.data()), H5Object::Group);
		if (!gr) {
			setError("Cannot open H5 group " + gr_name);
			return;
		}
		d_data->position.push_back(PrivateData::Position{ gr_name, gr, QByteArray(), GroupContent() });
		d_data->position.back().groups_contents = &d_data->groups_contents;
		d_data->position.back().populate(device());

		if (read_metadata) {
			QByteArray gr_bname = gr_name;
			auto lst = listAttributes(gr, gr_bname.data());
			for (const QByteArray attrname : lst) {
				QVariant v = readAttribute(gr.id(), attrname.data());
				if (v.userType() != 0)
					metadata.insert(attrname, v);
			}
		}
		if (device()->pos() > d_data->dpos)
			setValue(d_data->dpos = device()->pos());
	}
}
void VipH5Archive::doEnd() 
{
	if (d_data->position.size() <= 1) {
		setError("Cannot close the root group");
		return;
	}

	// find group name
	auto idx = d_data->position.back().name.lastIndexOf("/");
	if (idx < 0) {
		setError("Invalid group name: " + d_data->position.back().name);
		return;
	}
	QByteArray current_name = d_data->position.back().name.mid(idx + 1);
	d_data->last_end = d_data->position.back().name;
	d_data->position.pop_back();
	d_data->position.back().last = current_name;
	
}

#define ERROR(str)    \
	{	setError(str);\
		return;\
	}



static QVariant toNDArrayOrBytes(const QVariant& v)
{
	if (v.userType() == QMetaType::QString || v.userType() == QMetaType::QByteArray)
		return v;

	if (v.userType() == qMetaTypeId<complex_d>()) {
		complex_d c = v.value<complex_d>();
		return QVariant::fromValue(VipNDArray({ c.real(), c.imag() }));
	}

	if (v.userType() == qMetaTypeId<complex_f>()) {
		complex_f c = v.value<complex_f>();
		return QVariant::fromValue(VipNDArray({ c.real(), c.imag() }));
	}

	if (v.userType() == qMetaTypeId<VipPointVector>()) {
		return QVariant::fromValue(v.value<VipNDArray>());
	}

	if (v.userType() == qMetaTypeId<VipInterval>()) {
		VipInterval c = v.value<VipInterval>();
		return QVariant::fromValue(VipNDArray({ c.minValue(), c.maxValue() }));
	}

	if (v.userType() == qMetaTypeId<VipTimeRange>()) {
		VipTimeRange c = v.value<VipTimeRange>();
		return QVariant::fromValue(VipNDArray({ c.first, c.second }));
	}

	if (v.userType() == qMetaTypeId<VipNDArray>() ) {
		VipNDArray ar = v.value<VipNDArray>();
		if (!vipIsImageArray(ar))
			return QVariant::fromValue(ar.dense());
		return QVariant::fromValue(VipNDArray(VipNDArrayTypeView<uchar>((uchar*)vipToImage(ar).constBits(), vipVector(ar.shape(0), ar.shape(1), 4))));
	}

	QByteArray ar;
	{
		QDataStream str(&ar, QIODevice::WriteOnly);
		str.setByteOrder(QDataStream::LittleEndian);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		if (!QMetaType::save(str, v.userType(), v.constData())) 
#else
		if (!QMetaType(v.userType()).save(str, v.constData())) 
#endif
			return QVariant();

		if (str.status() != QDataStream::Ok)
			return QVariant();
	}
	return QVariant::fromValue(ar);
}

void VipH5Archive::doComment(QString& text)
{
	content("Comment", text);
}

static H5Object compactLayout()
{
	H5Object plist(H5Pcreate(H5P_DATASET_CREATE), H5Object::Prop);
	H5Pset_layout(plist, H5D_COMPACT);
	return plist;
}
static H5Object defaultProp()
{
	H5Object plist(H5P_DEFAULT, H5Object::Prop);
	return plist;
}

void VipH5Archive::doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata) 
{
	hid_t id = d_data->position.back().group.id();
	QByteArray bname = name.toLatin1();
	H5Object set;
	H5Object gr;

	if (mode() == Write)
	{
		if (name.isEmpty()) {
			// default name
			bname = "object";
			name = bname;
		}
		bname = d_data->position.back().createUniqueName(bname);
		

		if (vipIsArithmetic(value.userType())) {
			// Arithmetic object
			auto type = qtToHDF5(value.userType());
			H5Object space(H5Screate(H5S_SCALAR), H5Object::Space);
			H5Object layout = compactLayout();
			set = H5Object(H5Dcreate(id, bname.data(), type, space, H5P_DEFAULT, layout, H5P_DEFAULT), H5Object::Set);
			if (!set)
				ERROR("Failed to create dataset");
			if (H5Dwrite(set, type, space, space, H5P_DEFAULT, value.data()) < 0)
				ERROR("Failed to write dataset");
			goto finish;
		}

		if (value.userType() == QMetaType::QString) {
			// String object stored as utf8 string scalar
			QByteArray utf8 = value.toString().toUtf8();
			H5Object space(H5Screate(H5S_SCALAR), H5Object::Space);
			if (!space)
				ERROR("Failed to create dataspace");

			H5Object type = stringType(utf8);
			H5Object layout = utf8.size() < (1 << 15) ? compactLayout() : defaultProp();
			set = H5Object(H5Dcreate(id, bname.data(), type, space, H5P_DEFAULT, layout, H5P_DEFAULT), H5Object::Set);
			
			if (H5Dwrite(set, type, space, space, H5P_DEFAULT, utf8.data()) < 0)
				ERROR("Failed to write dataset");
			goto finish;
		}

		if (value.userType() >= QMetaType::User) {
			// Custom type: try to use the serialize dispatcher
			const VipFunctionDispatcher<2>::function_list_type lst = serializeFunctions(value);
			if (lst.size()) {
				// call all serialize functions in a new group
				QString classname = VipAny(value).type().name;
				if (classname.isEmpty())
					ERROR("Unknown object type");

				QVariantMap map;
				map["type_name"] = classname;
				this->start(bname, map);
				for (qsizetype i = 0; i < lst.size(); ++i) {
					lst[i](value, this);
					if (hasError())
						break;
				}
				this->end();
				goto finish;
			}
		}

		// Convert object to QByteArray or VipNDArray (if possible)
		QVariant val = toNDArrayOrBytes(value);

		if (val.isNull()) {
			// Object that cannot be saved to bytes: write an empty byte array
			int dim_count = 1;
			auto type = qtToHDF5(QMetaType::UChar);
			hsize_t dims[1] = { (hsize_t)0 };
			H5Object space(H5Screate_simple(dim_count, dims, dims), H5Object::Space);
			if (!space)
				ERROR("Failed to create dataspace");
			set = H5Object(H5Dcreate(id, bname.data(), type, space.id(), H5P_DEFAULT, compactLayout(), H5P_DEFAULT), H5Object::Set);
			if (!set)
				ERROR("Failed to create dataset");
			if (H5Dwrite(set, type, space, space, H5P_DEFAULT, nullptr) < 0)
				ERROR("Failed to write dataset");
			// write 'type' attribute
			if (!writeAttribute(set, "type_name", QByteArray(value.typeName())))
				ERROR("Failed to write the 'type' attribute to dataset");

			goto finish;
		}

		if (val.userType()  == QMetaType::QByteArray) {
			const QByteArray ar = val.toByteArray();
			int dim_count = 1;
			auto type = qtToHDF5(QMetaType::UChar);
			hsize_t dims[1] = { (hsize_t)ar.size() };
			H5Object space(H5Screate_simple(dim_count, dims, dims), H5Object::Space);
			if (!space)
				ERROR("Failed to create dataspace");
			H5Object layout = ar.size() < (1 << 15) ? compactLayout() : defaultProp();
			set = H5Object(H5Dcreate(id, bname.data(), type, space.id(), H5P_DEFAULT, layout, H5P_DEFAULT), H5Object::Set);
			if (!set)
				ERROR("Failed to create dataset");
			if (H5Dwrite(set, type, space, space, H5P_DEFAULT, ar.data()) < 0)
				ERROR("Failed to write dataset");
			// write 'type_name' attribute
			if(!writeAttribute(set, "type_name", QByteArray(value.typeName())))
				ERROR("Failed to write the 'type' attribute to dataset");

			goto finish;
		}
		else {
			// VipNDArray
			const VipNDArray ar = val.value<VipNDArray>();
			ArraySpace aspace = arraySpaceAndData(ar);
			if (!aspace.space)
				ERROR("Failed to create dataspace");
			H5Object layout = (ar.size()*ar.dataSize()) < (1 << 15) ? compactLayout() : defaultProp();
			set = H5Object(H5Dcreate(id, bname.data(), aspace.type, aspace.space, H5P_DEFAULT, layout, H5P_DEFAULT), H5Object::Set);
			if (!set)
				ERROR("Failed to create dataset");
			if (H5Dwrite(set, aspace.type, aspace.space, aspace.space, H5P_DEFAULT, aspace.data) < 0)
				ERROR("Failed to write dataset");

			// write 'type_name' attribute
			if (!writeAttribute(set, "type_name", QByteArray(value.typeName())))
				ERROR("Failed to write the 'type' attribute to dataset");

			goto finish;
		}
	}
	else {

		// open next dataset
		PrivateData::Position& pos = d_data->position.back();

		qint64 idx = -1;
		if (!bname.contains('%')) {

			idx = pos.nextDataByName(d_data->file, bname, pos.indexOf(pos.last) + 1);
			if (idx < 0) {
				// Here we want a silent error (no string)
				// as this is often used to read an unkown number of objects
				setError(QString());
				return;
			}
			bname = pos.content[idx].first;
			if (name.isEmpty()) {
				int index = bname.indexOf('%');
				if (index < 0)
					name = bname;
				else
					name = bname.mid(0, index);
			}
		}
		else
		{
			// object name contains '%': this is a raw name, don't try to add a '%' ourselves
			if (bname.lastIndexOf('%') == bname.size() - 1) {
				// Ending by '%' : just remove the '%'
				bname.chop(1);
			}
			auto it = pos.content_names.find(bname);
			if (it == pos.content_names.end())
				ERROR("Object name not found: " + bname);

			idx = it->second;
		}

		if (pos.content[idx].second == H5Object::Group)
		{
			// Data is stored as a group with an attribute 'type_name'
			QByteArray gr_name = pos.name + "/" + bname;

			// add a trailing '%' if necessary
			this->start(bname);
			if (hasError())
				return;
			gr = d_data->position.back().group;
			QByteArray type_name = readAttribute(gr, "type_name").value<QString>().toLatin1();
			int type_id = vipIdFromName(type_name.data());
			if (type_name.isEmpty() || type_id == 0) {
				this->end();
				ERROR("Invalide group type name: " + type_name);
			}

			if (!value.isValid()) {
				value = vipCreateVariant(type_name.data());
				if (!value.isValid() || ((QMetaType(value.userType()).flags() & QMetaType::PointerToQObject) && value.value<QObject*>() == nullptr)) {
					this->end();
					ERROR("Cannot create QVariant value with type name ='" + type_name + "'");
				}
			}
			const VipFunctionDispatcher<2>::function_list_type lst = deserializeFunctions(value);
			if (lst.size()) {
				for (int i = 0; i < lst.size(); ++i) {
					value = lst[i](value, this);
					if (hasError())
						break;
				}
			}
			this->end();
			goto finish;
		}

		set = H5Object(H5Dopen(id, bname.data(), H5P_DEFAULT), H5Object::Set);
		if (!set)
			ERROR("Failed to open dataset");
		H5Object type(H5Dget_type(set), H5Object::H5Type);
		if (!type)
			ERROR("Failed to read dataset type");
		H5Object space(H5Dget_space(set), H5Object::Space);
		if (!space)
			ERROR("Failed to read dataset space");

		// utf8 string
		H5T_class_t type_class = H5Tget_class(type);
		if (type_class == H5T_STRING) {
			H5Object atype_mem(H5Tget_native_type(type, H5T_DIR_ASCEND), H5Object::H5Type);
			int _size = (int)H5Tget_size(atype_mem);
			QByteArray data(_size, 0);
			if (H5Dread(set, type, space, space, H5P_DEFAULT, (void*)data.data()) != 0)
				ERROR("Unable to read dataset");
			if (data.size() && data[data.size() - 1] == (char)0)
				data.chop(1);
			value = QVariant::fromValue(QString::fromUtf8(data));
			goto finish;
		}

		int qt_type = HDF5ToQt(type);
		hsize_t dims[32];
		int rank = H5Sget_simple_extent_ndims(space);
		//TODO: handle rank > 32
		H5Sget_simple_extent_dims(space, dims, nullptr);

		if (!vipIsArithmetic(qt_type))
			ERROR("Invalid dataset type: " + QString::number(type));
		if (rank > VIP_MAX_DIMS)
			ERROR("Unsupported rank: " + QString::number(rank));

		// numeric type
		if ((rank == 1 && dims[0] == 1) || rank == 0) {
			qint64 data = 0;
			QVariant v = vipFromVoid(qt_type, &data);
			if (H5Dread(set, type, space, space, H5P_DEFAULT, (void*)v.data()) != 0)
				ERROR("Unable to read dataset");
			value = v;
			goto finish;
		}

		// read the 'type_name' attribute
		QByteArray type_name = readAttribute(set, "type_name").toByteArray();
		int type_id = vipIdFromName(type_name.data());
		if (type_name.isEmpty() || type_id == 0) {
			//ERROR("Invalide dataset type name: " + type_name);

			// No type id: use plain VipNDArray
			type_id = qMetaTypeId<VipNDArray>();
			type_name = "VipNDArray";
		}
			

		// string type
		if (type_id == QMetaType::QString || type_id == QMetaType::QByteArray) {
			if (rank != 1)
				ERROR("Invalide dataset rank for string/bytes type: " + QString::number(rank));
			QByteArray ar((int)dims[0], (char)0);
			if (H5Dread(set, type, space, space, H5P_DEFAULT, (void*)ar.data()) != 0)
				ERROR("Unable to read dataset");
			value = QVariant::fromValue(ar);
			goto finish;
		}

		if (type_id == qMetaTypeId<complex_f>() || type_id == qMetaTypeId<complex_d>() || type_id == qMetaTypeId<VipNDArray>() || type_id == qMetaTypeId<VipInterval>() ||
		    type_id == qMetaTypeId<VipTimeRange>() ||
		    type_id == qMetaTypeId<VipPointVector>()) {
			
			// Generic ND array
			VipNDArrayShape sh;
			sh.resize(rank);
			for (int i = 0; i < rank; ++i)
				sh[i] = (qsizetype)dims[i];
			VipNDArray ar(qt_type, sh);
			if (H5Dread(set, type, space, space, H5P_DEFAULT, (void*)ar.data()) != 0)
				ERROR("Unable to read dataset array");

			if (type_id == qMetaTypeId<complex_f>()) {
				if (ar.shapeCount() > 1 || ar.shape(0) != 2)
					ERROR("Invalide dataset rank for type: " + type_name);
				const VipNDArrayType<float> far = ar;
				value = QVariant::fromValue(complex_f(far[0], far[1]));
				goto finish;
			}

			if (type_id == qMetaTypeId<complex_d>()) {
				if (ar.shapeCount() > 1 || ar.shape(0) != 2)
					ERROR("Invalide dataset rank for type: " + type_name);
				const VipNDArrayType<double> far = ar;
				value = QVariant::fromValue(complex_d(far[0], far[1]));
				goto finish;
			}

			if (type_id == qMetaTypeId<VipInterval>()) {
				if (ar.shapeCount() > 1 || ar.shape(0) != 2)
					ERROR("Invalide dataset rank for type: " + type_name);
				const VipNDArrayType<double> far = ar;
				value = QVariant::fromValue(VipInterval(far[0], far[1]));
				goto finish;
			}

			if (type_id == qMetaTypeId<VipTimeRange>()) {
				if (ar.shapeCount() > 1 || ar.shape(0) != 2)
					ERROR("Invalide dataset rank for type: " + type_name);
				const VipNDArrayType<qint64> far = ar;
				value = QVariant::fromValue(VipTimeRange(far[0], far[1]));
				goto finish;
			}

			if (type_id == qMetaTypeId<VipPointVector>()) {
				value = QVariant::fromValue(QVariant::fromValue(ar).value<VipPointVector>());
				goto finish;
			}

			if (ar.shapeCount() == 3 && ar.shape(2) == 4 && qt_type == QMetaType::UChar) {
				QImage img(ar.shape(1), ar.shape(0), QImage::Format_ARGB32);
				memcpy(img.bits(), ar.constData(), (size_t)(ar.shape(0) * ar.shape(1) * 4));
				value = QVariant::fromValue(vipToArray(img));
				goto finish;
			}

			value = QVariant::fromValue(ar);
			goto finish;
		}

		if (rank != 1)
			ERROR("Invalid rank for dataset type " + type_name);
		
		// read bytes
		QByteArray ar((int)dims[0], (char)0);
		if (H5Dread(set, type, space, space, H5P_DEFAULT, (void*)ar.data()) != 0)
			ERROR("Unable to read dataset");
		if (ar.size()) {

			value = QVariant();
			QDataStream str(ar);
			str.setByteOrder(QDataStream::LittleEndian);
			value = vipCreateVariant(type_name.data());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			if (!QMetaType::load(str, value.userType(), value.data()))
#else
			if (!QMetaType(value.userType()).load(str, value.data()))
#endif
				ERROR("Unable to interpret dataset bytes as " + type_name);
			// str >> value;
			if (value.userType() == 0)
				ERROR("Unable to interpret dataset bytes as " + type_name);
		}
		else
			value = vipCreateVariant(type_name.data());
		goto finish;
	}

finish:
	if (mode() == Write) {
		// Add new content
		d_data->position.back().addContent( bname, H5Object::Set );
		for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
			writeAttribute(set, it.key().toLatin1().data(), it.value());
	}
	else {
		if (read_metadata) {
			hid_t i = set;
			const auto lst = listAttributes(i ? set : gr, (d_data->position.back().name + "/" + bname).data());
			for (const QByteArray& attr : lst) {
				QVariant v = readAttribute(i ? set : gr, attr.data());
				if (v.userType())
					metadata[attr] = v;
			}
		}
		if (device()->pos() > d_data->dpos)
			setValue(d_data->dpos = device()->pos());
	}
	// update last position
	d_data->position.back().last = bname;
}
