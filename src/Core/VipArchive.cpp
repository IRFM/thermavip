/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipArchive.h"
#include "VipCore.h"

class VipArchive::PrivateData
{
public:
	Flag flag;
	SupportedOperations operations;
	OpenMode io_mode;
	QString version;
	VipFunctionDispatcher<2> fastTypesS;
	VipFunctionDispatcher<2> fastTypesD;

	struct Parameters
	{
		Parameters(const QStringList& position = QStringList(), const QString& errorString = QString(), int errorCode = 0, const QMap<QByteArray, bool>& attributes = QMap<QByteArray, bool>())
		  : position(position)
		  , errorString(errorString)
		  , errorCode(errorCode)
		  , readMode(Forward)
		  , attributes(attributes)
		{
		}
		QStringList position;
		QString errorString;
		int errorCode;
		ReadMode readMode;
		QMap<QByteArray, bool> attributes;
	};

	Parameters parameters;
	QList<Parameters> saved;

	PrivateData()
	  : flag(Text)
	  , io_mode(NotOpen)
	{
	}
};

VipArchive::VipArchive(Flag flag, SupportedOperations op)
  : QObject()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->flag = flag;
	d_data->operations = op;
	d_data->io_mode = NotOpen;
	d_data->parameters.errorCode = 0;
}

VipArchive::~VipArchive()
{
}

void VipArchive::setReadMode(ReadMode mode)
{
	if (mode == Backward && (!(d_data->operations & ReadBackward)))
		return;

	d_data->parameters.readMode = mode;
}

VipArchive::ReadMode VipArchive::readMode() const
{
	return d_data->parameters.readMode;
}

void VipArchive::setError(const QString& error, int code)
{
	d_data->parameters.errorString = error;
	d_data->parameters.errorCode = code;
}

void VipArchive::resetError()
{
	d_data->parameters.errorString.clear();
	d_data->parameters.errorCode = 0;
}

QString VipArchive::errorString() const
{
	return d_data->parameters.errorString;
}

int VipArchive::errorCode() const
{
	return d_data->parameters.errorCode;
}

bool VipArchive::hasError() const
{
	return d_data->parameters.errorCode != 0;
}

VipArchive::SupportedOperations VipArchive::supportedOperations() const
{
	return d_data->operations;
}

VipArchive::Flag VipArchive::flag() const
{
	return d_data->flag;
}

VipArchive::OpenMode VipArchive::mode() const
{
	return d_data->io_mode;
}

bool VipArchive::isOpen() const
{
	return mode() != NotOpen;
}

void VipArchive::setMode(OpenMode m)
{
	d_data->io_mode = m;
}

VipArchive::operator const void*() const
{
	if (isOpen() && !this->hasError())
		return this;
	else
		return nullptr;
}

unsigned VipArchive::save()
{
	if (mode() != Read)
		return 0;
	this->doSave();
	d_data->saved.append(d_data->parameters);
	return d_data->saved.size();
}
void VipArchive::restore()
{
	if (mode() != Read)
		return;
	if (d_data->saved.size()) {
		this->doRestore();
		d_data->parameters = d_data->saved.back();
		d_data->saved.pop_back();
	}
}
void VipArchive::restore(unsigned id) 
{
	if (mode() != Read)
		return;
	while (d_data->saved.size() >= id) {
		if (d_data->saved.size()) {
			this->doRestore();
			d_data->parameters = d_data->saved.back();
			d_data->saved.pop_back();
		}
	}
}

VipArchive& VipArchive::comment(const QString& cdata)
{
	if (d_data->operations & Comment) {
		resetError();
		this->doComment(const_cast<QString&>(cdata));
	}
	return *this;
}

VipArchive& VipArchive::start(const QString& name, const QVariantMap& metadata)
{
	if (mode() == Write && name.isEmpty()) {
		setError("Cannot write an empty Start object");
		return *this;
	}
	resetError();
	QVariantMap map;
	this->doStart(const_cast<QString&>(name), const_cast<QVariantMap&>(metadata), true);
	if (!hasError())
		d_data->parameters.position.append(name);
	return *this;
}

VipArchive& VipArchive::start(const QString& name)
{
	resetError();
	QVariantMap map;
	this->doStart(const_cast<QString&>(name), map, false);
	if (!hasError())
		d_data->parameters.position.append(name);
	return *this;
}

VipArchive& VipArchive::end()
{
	if (mode() == Write && d_data->parameters.position.size() == 0) {
		setError("end(): no related start()");
		return *this;
	}
	resetError();
	this->doEnd();
	if (!hasError())
		d_data->parameters.position.pop_back();
	return *this;
}

QStringList VipArchive::currentPosition() const
{
	return d_data->parameters.position;
}

void VipArchive::setAttribute(const char* name, bool value)
{
	d_data->parameters.attributes[name] = value;
}

bool VipArchive::hasAttribue(const char* name) const
{
	return d_data->parameters.attributes.find(name) != d_data->parameters.attributes.end();
}

bool VipArchive::attribute(const char* name, bool _default) const
{
	QMap<QByteArray, bool>::const_iterator it = d_data->parameters.attributes.find(name);
	if (it != d_data->parameters.attributes.end())
		return it.value();
	else
		return _default;
}

void VipArchive::setVersion(const QString& version)
{
	d_data->version = version;
}
QString VipArchive::version() const
{
	return d_data->version;
}

QVariant VipArchive::read(const QString& name, QVariantMap& metadata)
{
	if (mode() == Read) {
		QVariant value;
		content(name, value, metadata);
		return value;
	}
	return QVariant();
}

QVariant VipArchive::read(const QString& name)
{
	if (mode() == Read) {
		QVariant value;
		content(name, value);
		return value;
	}
	return QVariant();
}

QVariant VipArchive::read()
{
	if (mode() == Read) {
		QVariant value;
		content(value);
		return value;
	}
	return QVariant();
}

VipFunctionDispatcher<2>& VipArchive::serializeDispatcher()
{
	static VipFunctionDispatcher<2> s_dispatcher;
	return s_dispatcher;
}

VipFunctionDispatcher<2>& VipArchive::deserializeDispatcher()
{
	static VipFunctionDispatcher<2> s_dispatcher;
	return s_dispatcher;
}

VipFunctionDispatcher<2>::function_list_type VipArchive::serializeFunctions(const QVariant& value)
{
	VipFunctionDispatcher<2>::function_list_type lst;
	if (d_data->fastTypesS.count())
		lst = d_data->fastTypesS.match(value);
	if (!lst.size())
		lst = serializeDispatcher().match(value);
	return lst;
}

VipFunctionDispatcher<2>::function_list_type VipArchive::deserializeFunctions(const QVariant& value)
{
	VipFunctionDispatcher<2>::function_list_type lst;
	if (d_data->fastTypesD.count())
		lst = d_data->fastTypesD.match(value);
	if (!lst.size())
		lst = deserializeDispatcher().match(value);
	return lst;
}

void VipArchive::registerFastType(int type)
{
	VipTypeList lst;
	lst.append(VipType(type));
	lst.append(VipType(qMetaTypeId<VipBinaryArchive*>()));
	VipFunctionDispatcher<2>::function_list_type serialize_lst = serializeDispatcher().match(lst);
	VipFunctionDispatcher<2>::function_list_type deserialize_lst = deserializeDispatcher().match(lst);

	d_data->fastTypesS.append(serialize_lst);
	d_data->fastTypesD.append(deserialize_lst);
}

void VipArchive::copyFastTypes(VipArchive& other)
{
	other.d_data->fastTypesS = d_data->fastTypesS;
	other.d_data->fastTypesD = d_data->fastTypesD;
}

VipArchive& operator<<(VipArchive& arch, const QVariant& value)
{
	return arch.content(value);
}

VipArchive& operator>>(VipArchive& arch, QVariant& value)
{
	return arch.content(value);
}

QVariantMap vipEditableSymbol(const QString& comment, const QString& style_sheet)
{
	QVariantMap res;
	res["content_editable"] = comment;
	res["style_sheet"] = style_sheet;
	res["editable_id"] = 0;
	return res;
}

#include <QBuffer>
#include <QFile>
#include <QtEndian>

static bool toByteArray(const QVariant& v, QDataStream& stream)
{
	if (v.userType() == QMetaType::QByteArray) {
		QByteArray array = v.value<QByteArray>();
		stream.writeRawData(array.data(), array.size());
		return true;
	}
	else if (v.userType() == QMetaType::QString) {
		QByteArray latin = v.toString().toLatin1();
		stream << latin.size();
		stream.writeRawData(latin.data(), latin.size());
		return true;
	}
	else if (v.userType() == qMetaTypeId<QVariantMap>()) {
		QByteArray res;
		QDataStream str(&res, QIODevice::WriteOnly);
		vipSafeVariantMapSave(str, v.value<QVariantMap>());
		return true;
	}
	else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		return QMetaType::save(stream, v.userType(), v.data());
#else
		return QMetaType(v.userType()).save(stream, v.data());
#endif
	}
}

static bool fromByteArray(QDataStream& stream, QVariant& v, qsizetype max_size)
{
	if (v.userType() == QMetaType::QByteArray) {
		QByteArray ar(max_size, 0);
		stream.readRawData(ar.data(), max_size);
		v = QVariant::fromValue(ar);
		return true;
	}
	else if (v.userType() == QMetaType::QString) {
		qsizetype size;
		stream >> size;
		QByteArray ar(size, 0);
		stream.readRawData(ar.data(), size);
		v = QString(ar);
		return true;
	}
	else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		return QMetaType::load(stream, v.userType(), v.data());
#else
		return QMetaType(v.userType()).load(stream, v.data());
#endif
	}
}

#define WRITE_DEVICE(data, size)                                                                                                                                                                       \
	if (m_device->write(data, size) != size) {                                                                                                                                                     \
		this->setError("Cannot write data to the device");                                                                                                                                     \
		return;                                                                                                                                                                                \
	}

#define WRITE_DEVICE_INTEGER(value)                                                                                                                                                                    \
	{                                                                                                                                                                                              \
		qsizetype unused_tmp = qToLittleEndian(value);                                                                                                                                            \
		if (m_device->write(reinterpret_cast<char*>(&unused_tmp), sizeof(unused_tmp)) != sizeof(unused_tmp)) {                                                                                 \
			this->setError("Cannot write data to the device");                                                                                                                             \
			return;                                                                                                                                                                        \
		}                                                                                                                                                                                      \
	}

#define READ_DEVICE(data, size, m_device, return_value)                                                                                                                                                \
	if (m_device->read(data, size) != size) {                                                                                                                                                      \
		this->setError("Cannot read data from the device");                                                                                                                                    \
		return return_value;                                                                                                                                                                   \
	}

#define READ_DEVICE_INTEGER(value, m_device, return_value)                                                                                                                                             \
	{                                                                                                                                                                                              \
		qsizetype unused_tmp = 0;                                                                                                                                                                 \
		if (m_device->read(reinterpret_cast<char*>(&unused_tmp), (sizeof(unused_tmp))) != sizeof(unused_tmp)) {                                                                                \
			this->setError("Cannot read data from the device");                                                                                                                            \
			return return_value;                                                                                                                                                           \
		}                                                                                                                                                                                      \
		value = qFromLittleEndian(unused_tmp);                                                                                                                                                 \
	}

#define READ_DEVICE_INTEGER_BACKWARD(value, m_device, return_value)                                                                                                                                    \
	{                                                                                                                                                                                              \
		qsizetype unused_tmp = 0;                                                                                                                                                                 \
		if (!m_device->seek(m_device->pos() - sizeof(qsizetype))) {                                                                                                                                            \
			this->setError("Cannot read data from the device");                                                                                                                            \
			return return_value;                                                                                                                                                           \
		}                                                                                                                                                                                      \
		if (m_device->read(reinterpret_cast<char*>(&unused_tmp), (sizeof(unused_tmp))) != sizeof(unused_tmp)) {                                                                                \
			this->setError("Cannot read data from the device");                                                                                                                            \
			return return_value;                                                                                                                                                           \
		}                                                                                                                                                                                      \
		value = qFromLittleEndian(unused_tmp);                                                                                                                                                 \
	}

VipBinaryArchive::VipBinaryArchive()
  : VipArchive(Binary, ReadBackward)
  , m_device(nullptr)
{
}

VipBinaryArchive::VipBinaryArchive(QIODevice* d)
  : VipArchive(Binary, ReadBackward)
  , m_device(nullptr)
{
	setDevice(d);
}

VipBinaryArchive::VipBinaryArchive(QByteArray* a, QIODevice::OpenMode mode)
  : VipArchive(Binary, ReadBackward)
  , m_device(nullptr)
{
	QBuffer* buf = new QBuffer(a, this);
	buf->open(mode);
	setDevice(buf);
}

VipBinaryArchive::VipBinaryArchive(const QByteArray& a)
  : VipArchive(Binary, ReadBackward)
  , m_device(nullptr)
{
	QBuffer* buf = new QBuffer(this);
	buf->setData(a);
	buf->open(QBuffer::ReadOnly);
	setDevice(buf);
}

VipBinaryArchive::VipBinaryArchive(const QString& filename, QIODevice::OpenMode mode)
  : VipArchive(Binary, ReadBackward)
  , m_device(nullptr)
{
	QFile* file = new QFile(this);
	file->setFileName(filename);
	file->open(mode);
	setDevice(file);
}

void VipBinaryArchive::setDevice(QIODevice* d)
{
	if (d != m_device) {
		if (m_device) {
			m_device->close();
			if (m_device->parent() == this)
				delete m_device;
			m_device = nullptr;
			setMode(NotOpen);
		}

		if (d) {
			m_device = d;
			if (d->openMode() & QIODevice::ReadOnly)
				setMode(Read);
			else if (d->openMode() & QIODevice::WriteOnly)
				setMode(Write);
		}
	}
}

QIODevice* VipBinaryArchive::device() const
{
	return const_cast<QIODevice*>(m_device);
}

void VipBinaryArchive::close()
{
	setDevice(nullptr);
}

void VipBinaryArchive::doSave()
{
	qint64 pos = m_device ? m_device->pos() : 0;
	m_saved_pos.append(pos);
}

void VipBinaryArchive::doRestore()
{
	if (m_saved_pos.size()) {
		qint64 pos = m_saved_pos.last();
		m_saved_pos.pop_back();
		if (m_device)
			m_device->seek(pos);
	}
}

void VipBinaryArchive::doStart(QString& name, QVariantMap&, bool)
{
	if (mode() == Write) {
		// start is defined with a -1 and its name, and ends with & -1
		WRITE_DEVICE_INTEGER(-1);
		WRITE_DEVICE_INTEGER(name.size());
		WRITE_DEVICE(name.toLatin1().data(), name.size());
		WRITE_DEVICE_INTEGER(-1);
	}
	else if (mode() == Read) {
		// skip data until we find one with the right name
		while (true) {
			qint64 pos = m_device->pos();
			// TEST qint64
			qsizetype full_size = 0;
			QByteArray object_name;

			// read the value size;
			READ_DEVICE_INTEGER(full_size, m_device, );

			if (full_size == -2) {
				m_device->seek(pos);
				setError("No start tag found");
				break;
			}

			// read the name
			qsizetype size;
			READ_DEVICE_INTEGER(size, m_device, );
			object_name.resize(size);
			READ_DEVICE(object_name.data(), size, m_device, );
			READ_DEVICE_INTEGER(size, m_device, );

			if (full_size == -1) {
				if (!name.isEmpty() && object_name == name) // start name found, break
					break;
				else if (name.isEmpty()) // empt name: set name and break
				{
					name = object_name;
					break;
				}
			}
			else // otherwise, go to next data
			{
				m_device->seek(pos + sizeof(qsizetype)*2 + full_size);
			}
		}
	}
}

void VipBinaryArchive::doEnd()
{
	if (mode() == Write) {
		// start is defined with a -2
		WRITE_DEVICE_INTEGER(-2);
	}
	else if (mode() == Read) {
		qsizetype level = 0;
		// skip data until we find a end tag at the right level
		while (true) {
			qint64 pos = m_device->pos();
			// TEST qint64
			qsizetype full_size = 0;

			// read the value size;
			READ_DEVICE_INTEGER(full_size, m_device, );

			if (full_size == -1) // start tag: increase level
				level++;
			if (full_size == -2) {
				if (level == 0)
					break;
				else
					--level;
			}
			else // otherwise, go to next data
			{
				m_device->seek(pos + sizeof(qsizetype) + full_size);
			}
		}
	}
}

QByteArray VipBinaryArchive::readBinary(const QString& n)
{
	resetError();
	if (mode() != Read)
		return QByteArray();

	QString& name = const_cast<QString&>(n);

	qint64 current_pos = m_device->pos();

	qsizetype full_size;
	qsizetype name_size = 0;
	QByteArray object_name;

	// skip data until we find one with the right name
	while (true) {
		qint64 pos = m_device->pos();

		// read the value size;
		if (readMode() == Forward) {
			READ_DEVICE_INTEGER(full_size, m_device, QByteArray());
		}
		else {
			// read the size, go back at the beginning of the data
			READ_DEVICE_INTEGER_BACKWARD(full_size, m_device, QByteArray());
			if (full_size > 0)
				m_device->seek(m_device->pos() - sizeof(qsizetype) - full_size);
		}

		if (full_size == -1 || full_size == -2) // start or end tag found: stop
		{
			m_device->seek(pos);
			setError("Cannot find content");
			return QByteArray();
			;
		}

		// read the name
		READ_DEVICE_INTEGER(name_size, m_device, QByteArray());
		object_name.resize(name_size);
		READ_DEVICE(object_name.data(), name_size, m_device, QByteArray());

		if (name.isEmpty()) {
			name = object_name;
			break;
		}
		else if (object_name == name) {
			break;
		}
		else {
			if (readMode() == Forward)
				m_device->seek(pos + sizeof(qsizetype)*2 + full_size);
			else
				m_device->seek(pos - sizeof(qsizetype)*2 - full_size);

			current_pos = m_device->pos();
		}
	}

	QByteArray res = m_device->read(full_size - name_size - sizeof(qsizetype));

	// go to the next data
	if (readMode() == Forward)
		m_device->seek(current_pos + sizeof(qsizetype)*2 + full_size);
	else
		m_device->seek(current_pos - sizeof(qsizetype)*2 - full_size);

	return res;
}

QVariant VipBinaryArchive::deserialize(const QByteArray& ar)
{
	QBuffer buffer(const_cast<QByteArray*>(&ar));
	buffer.open(QBuffer::ReadOnly);

	// read the type name
	QByteArray type_name;
	qsizetype size;
	if (buffer.read((char*)(&size), sizeof(size)) != sizeof(size))
		return QVariant();
	type_name.resize(size);
	if (buffer.read(type_name.data(), type_name.size()) != type_name.size())
		return QVariant();

	// create the value if necessary
	QVariant value = vipCreateVariant(type_name.data());
	if (!value.isValid()) {
		setError("Cannot create QVariant value with type name ='" + type_name + "'");
		return QVariant();
	}

	bool serialized = false;
	VipBinaryArchive bin;
	bin.setDevice(&buffer);
	if (value.userType() >= QMetaType::User || value.userType() == QMetaType::QVariantMap) {
		// use the dispatcher approach
		VipFunctionDispatcher<2>::function_list_type lst = deserializeFunctions(value);
		if (lst.size()) {
			for (qsizetype i = 0; i < lst.size(); ++i) {
				value = lst[i](value, &bin);
				if (hasError()) {
					return QVariant();
				}
			}
			serialized = true;
		}
	}

	if (!serialized) {
		// use the standard approach
		qsizetype to_read = ar.size() - size - sizeof(qsizetype);
		QDataStream str(&buffer);
		buffer.seek(size + sizeof(qsizetype));
		str.setByteOrder(QDataStream::LittleEndian);
		if (!fromByteArray(str, value, to_read)) {
			setError("Cannot create QVariant value with type name ='" + type_name + "'");
			return QVariant();
		}
	}
	return value;
}

void VipBinaryArchive::doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata)
{
	Q_UNUSED(metadata);
	Q_UNUSED(read_metadata);

	if (mode() == Write) {
		QByteArray to_write;
		QDataStream stream(&to_write, QIODevice::WriteOnly);
		stream.setByteOrder(QDataStream::LittleEndian);

		// write the name
		stream << name.size();
		stream.writeRawData(name.toLatin1().data(), name.size());

		// special case: we are writing the content of a non sequential and opened QIODevice:
		// treat it as a QByteArray. The goal here is to avoid going through en intermediate buffer
		// and avoid a potentially HUGE copy that cound end up in a bad_alloc() exception

		QIODevice* device = value.value<QIODevice*>();
		bool use_device = (device && (device->openMode() & QIODevice::ReadOnly) && !device->isSequential());

		// write the type_name
		const char* type_name = use_device ? "QByteArray" : value.typeName();
		qsizetype size = (qsizetype)(type_name ? strlen(type_name) : 0);
		stream << size;
		if (type_name)
			stream.writeRawData(type_name, size);

		// write the value content

		bool serialized = false;
		if (!use_device && (value.userType() >= QMetaType::User || value.userType() == QMetaType::QVariantMap)) {
			// use the serialize dispatcher, but not in the QIODevice case

			VipFunctionDispatcher<2>::function_list_type lst = serializeFunctions(value);
			if (lst.size()) {
				// call all serialize functions in a new archive
				VipBinaryArchive arch(&to_write, QIODevice::WriteOnly);
				this->copyFastTypes(arch);
				arch.device()->seek(to_write.size());
				for (qsizetype i = 0; i < lst.size(); ++i) {
					lst[i](value, &arch);
					if (arch.hasError())
						break;
				}
				serialized = true;
			}
		}

		if (!serialized) {
			if (use_device) {
				// write the input device content

				qsizetype _size = device->size() + to_write.size();
				WRITE_DEVICE_INTEGER(_size);
				WRITE_DEVICE(to_write.data(), to_write.size());

				// write the device content by chunks of 10k
				QByteArray tmp(10000, 0);
				qsizetype tot_size = 0;
				while (true) {
					qsizetype read = device->read(tmp.data(), tmp.size());
					tot_size += read;
					WRITE_DEVICE(tmp.data(), read);
					if (read != (uint)tmp.size())
						break;
				}

				WRITE_DEVICE_INTEGER(_size);
			}
			else {
				qint64 p = m_device->pos();
				// we need to write sizeof(qsizetype) bytes (size) + content of to_write
				WRITE_DEVICE_INTEGER(qsizetype(0));
				WRITE_DEVICE(to_write.data(), to_write.size());

				// then write serialized content
				QDataStream str(m_device);
				str.setByteOrder(QDataStream::LittleEndian);
				// m_device->seek(p + sizeof(qsizetype) + to_write.size());
				toByteArray(value, str);

				qsizetype written_size = m_device->pos() - p - sizeof(qsizetype);
				// write end size
				WRITE_DEVICE_INTEGER(written_size);
				// write start size
				m_device->seek(p);
				WRITE_DEVICE_INTEGER(written_size);
				// go to end device
				m_device->seek(m_device->size()); // p + sizeof(qsizetype) + written_size);

				// convert to QByteArray and write to device

				// QDataStream str(&to_write, QIODevice::WriteOnly);
				//  str.setByteOrder(QDataStream::LittleEndian);
				//  str.device()->seek(to_write.size());
				//  toByteArray(value, str);
				//
				// //write the data size, the data itself and once again the data size.
				// //a frame STARTS and ENDS with the data size
				// WRITE_DEVICE_INTEGER(to_write.size());
				// WRITE_DEVICE(to_write.data(), to_write.size());
				// WRITE_DEVICE_INTEGER(to_write.size());
			}
		}
		else {
			// write serialized data to device

			// write the data size, the data itself and once again the data size.
			// a frame STARTS and ENDS with the data size
			WRITE_DEVICE_INTEGER(to_write.size());
			WRITE_DEVICE(to_write.data(), to_write.size());
			WRITE_DEVICE_INTEGER(to_write.size());
		}
	}
	else if (mode() == Read) {
		qint64 current_pos = m_device->pos();

		qsizetype full_size;
		QByteArray object_name;

		// skip data until we find one with the right name
		while (true) {
			qint64 pos = m_device->pos();

			// read the value size;
			if (readMode() == Forward) {
				READ_DEVICE_INTEGER(full_size, m_device, );
			}
			else {
				// read the size, go back at the beginning of the data
				READ_DEVICE_INTEGER_BACKWARD(full_size, m_device, );
				if (full_size > 0)
					m_device->seek(m_device->pos() - sizeof(qsizetype) - full_size);
			}

			if (full_size == -1 || full_size == -2) // start or end tag found: stop
			{
				m_device->seek(pos);
				setError("Cannot find content");
				return;
			}

			// read the name
			qsizetype size;
			READ_DEVICE_INTEGER(size, m_device, );
			object_name.resize(size);
			READ_DEVICE(object_name.data(), size, m_device, );

			if (name.isEmpty()) {
				name = object_name;
				break;
			}
			else if (object_name == name) {
				break;
			}
			else {
				if (readMode() == Forward)
					m_device->seek(pos + sizeof(qsizetype)*2 + full_size);
				else
					m_device->seek(pos - sizeof(qsizetype)*2 - full_size);

				current_pos = m_device->pos();
			}
		}

		// read the type name
		QByteArray type_name;
		qsizetype size;
		READ_DEVICE_INTEGER(size, m_device, );
		type_name.resize(size);
		READ_DEVICE(type_name.data(), size, m_device, );
		// const char * tp = type_name.data();
		// create the value if necessary
		if (!value.isValid()) {
			value = vipCreateVariant(type_name.data());
			if (!value.isValid() && type_name.size()) {
				setError("Cannot create QVariant value with type name ='" + type_name + "'");

				// skip the whole value
				if (readMode() == Forward)
					m_device->seek(current_pos + sizeof(qsizetype)*2 + full_size);
				else
					m_device->seek(current_pos - sizeof(qsizetype)*2 - full_size);
				return;
			}
		}

		bool serialized = false;
		if (value.userType() >= QMetaType::User || value.userType() == QMetaType::QVariantMap) {
			// use the dispatcher approach
			VipFunctionDispatcher<2>::function_list_type lst = deserializeFunctions(value);
			if (lst.size()) {
				// before calling the deserialize functions, save the read mode and reset it after
				ReadMode mode = readMode();
				setReadMode(Forward);

				for (qsizetype i = 0; i < lst.size(); ++i) {
					value = lst[i](value, this);
					if (hasError()) {
						// break;
						// skip the whole value
						if (readMode() == Forward)
							m_device->seek(current_pos + sizeof(qsizetype)*2 + full_size);
						else
							m_device->seek(current_pos - sizeof(qsizetype)*2 - full_size);
						setReadMode(mode);
						return;
					}
				}

				setReadMode(mode);
				serialized = true;
			}
		}

		if (!serialized) {
			// use the standard approach
			qsizetype to_read = readMode() == Forward ? current_pos + sizeof(qsizetype) + full_size - m_device->pos() : current_pos - m_device->pos() - sizeof(qsizetype);
			QDataStream str(m_device);
			str.setByteOrder(QDataStream::LittleEndian);
			if (value.userType() && !fromByteArray(str, value, to_read)) {
				setError("Cannot create QVariant value with type name ='" + type_name + "'");

				// skip the whole value
				if (readMode() == Forward)
					m_device->seek(current_pos + sizeof(qsizetype)*2 + full_size);
				else
					m_device->seek(current_pos - sizeof(qsizetype)*2 - full_size);
				return;
			}
		}

		// read the end data size
		READ_DEVICE_INTEGER(size, m_device, );

		// go to the next data
		if (readMode() == Forward)
			m_device->seek(current_pos + sizeof(qsizetype)*2 + full_size);
		else
			m_device->seek(current_pos - sizeof(qsizetype)*2 - full_size);

	} // end mode() == Read
}
