#include "VipArchive.h"
#include "VipCore.h"

class VipArchive::PrivateData
{
public:
	Flag	flag;
	SupportedOperations operations;
	OpenMode 	io_mode;
	QString version;
	VipFunctionDispatcher<2> fastTypesS;
	VipFunctionDispatcher<2> fastTypesD;

	struct Parameters
	{
		Parameters(const QStringList & position = QStringList(), const QString & errorString = QString(), int errorCode = 0, const QMap<QByteArray, bool> & attributes = QMap<QByteArray, bool>())
		:position(position),errorString(errorString),errorCode(errorCode), readMode(Forward), attributes(attributes){}
		QStringList position;
		QString		errorString;
		int			errorCode;
		ReadMode readMode;
		QMap<QByteArray, bool> attributes;
	};

	Parameters parameters;
	QList<Parameters> saved;

	PrivateData(): flag(Text), io_mode(NotOpen) {}
};


VipArchive::VipArchive(Flag flag, SupportedOperations op)
: QObject()
{
	m_data = new PrivateData();
	m_data->flag = flag;
	m_data->operations = op;
	m_data->io_mode = NotOpen;
	m_data->parameters.errorCode = 0;
}

VipArchive::~VipArchive()
{
	delete m_data;
}

void VipArchive::setReadMode(ReadMode mode)
{
	if(mode == Backward && (!(m_data->operations & ReadBackward)))
		return;

	m_data->parameters.readMode = mode;
}

VipArchive::ReadMode VipArchive::readMode() const
{
	return m_data->parameters.readMode;
}

void VipArchive::setError(const QString & error, int code )
{
	m_data->parameters.errorString = error;
	m_data->parameters.errorCode = code;
}

void VipArchive::resetError()
{
	m_data->parameters.errorString.clear();
	m_data->parameters.errorCode = 0;
}

QString VipArchive::errorString() const
{
	return m_data->parameters.errorString;
}

int VipArchive::errorCode() const
{
	return m_data->parameters.errorCode;
}

bool VipArchive::hasError() const
{
	return m_data->parameters.errorCode != 0;
}

VipArchive::SupportedOperations VipArchive::supportedOperations() const
{
	return m_data->operations;
}

VipArchive::Flag VipArchive::flag() const
{
	return m_data->flag;
}

VipArchive::OpenMode VipArchive::mode() const
{
	return m_data->io_mode;
}

bool VipArchive::isOpen() const
{
	return mode() != NotOpen;
}

void VipArchive::setMode(OpenMode m)
{
	m_data->io_mode = m;
}

VipArchive::operator const void*() const
{
	if( isOpen() && !this->hasError())
		return this;
	else
		return NULL;
}

void VipArchive::save()
{
	m_data->saved.append(m_data->parameters);
}
void VipArchive::restore()
{
	if(m_data->saved.size())
	{
		m_data->parameters = m_data->saved.back();
		m_data->saved.pop_back();
	}
}

VipArchive & VipArchive::comment(const QString & cdata)
{
	if(m_data->operations & Comment)
	{
		resetError();
		this->doComment(const_cast<QString&>(cdata));
	}
	return *this;
}

VipArchive & VipArchive::start(const QString & name, const QVariantMap & metadata)
{
	if(mode() == Write && name.isEmpty())
	{
		setError("Cannot write an empty Start object");
		return *this;
	}
	resetError();
	QVariantMap map;
	this->doStart(const_cast<QString&>(name),const_cast<QVariantMap&>(metadata),true);
	if(!hasError())
		m_data->parameters.position.append(name);
	return *this;
}

VipArchive & VipArchive::start(const QString & name)
{
	resetError();
	QVariantMap map;
	this->doStart(const_cast<QString&>(name),map,false);
	if(!hasError())
		m_data->parameters.position.append(name);
	return *this;
}

VipArchive & VipArchive::end()
{
	if(mode() == Write && m_data->parameters.position.size() == 0)
	{
		setError("end(): no related start()");
		return *this;
	}
	resetError();
	this->doEnd();
	if(!hasError())
		m_data->parameters.position.pop_back();
	return *this;
}

QStringList VipArchive::currentPosition() const
{
	return m_data->parameters.position;
}

void VipArchive::setAttribute(const char * name, bool value)
{
	m_data->parameters.attributes[name] = value;
}

bool VipArchive::hasAttribue(const char * name) const
{
	return m_data->parameters.attributes.find(name) != m_data->parameters.attributes.end();
}

bool VipArchive::attribute(const char *name, bool _default) const
{
	QMap<QByteArray, bool>::const_iterator it = m_data->parameters.attributes.find(name);
	if (it != m_data->parameters.attributes.end())
		return it.value();
	else
		return _default;
}

void VipArchive::setVersion(const QString& version)
{
	m_data->version = version;
}
QString VipArchive::version() const
{
	return m_data->version;
}

QVariant VipArchive::read(const QString & name,  QVariantMap & metadata)
{
	if(mode() == Read)
	{
		QVariant value;
		content(name,value,metadata);
		return value;
	}
	return QVariant();
}

QVariant VipArchive::read(const QString & name)
{
	if(mode() == Read)
	{
		QVariant value;
		content(name,value);
		return value;
	}
	return QVariant();
}

QVariant VipArchive::read()
{
	if(mode() == Read)
	{
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
	if(m_data->fastTypesS.count())
		lst = m_data->fastTypesS.match(value);
	if(!lst.size())
		lst = serializeDispatcher().match(value);
	return lst;
}

VipFunctionDispatcher<2>::function_list_type VipArchive::deserializeFunctions(const QVariant& value)
{
	VipFunctionDispatcher<2>::function_list_type lst;
	if (m_data->fastTypesD.count())
		lst = m_data->fastTypesD.match(value);
	if(!lst.size())
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

	m_data->fastTypesS.append(serialize_lst);
	m_data->fastTypesD.append(deserialize_lst);
}

void VipArchive::copyFastTypes(VipArchive & other)
{
	other.m_data->fastTypesS = m_data->fastTypesS;
	other.m_data->fastTypesD = m_data->fastTypesD;
}




VipArchive & operator<<(VipArchive & arch, const QVariant & value)
{
	return arch.content(value);
}

VipArchive & operator>>(VipArchive & arch, QVariant & value)
{
	return arch.content(value);
}





QVariantMap vipEditableSymbol(const QString & comment, const QString & style_sheet)
{
	QVariantMap res;
	res["content_editable"] = comment;
	res["style_sheet"] = style_sheet;
	res["editable_id"] = 0;
	return res;
}





#include <QtEndian>
#include <QBuffer>
#include <QFile>

static bool toByteArray(const QVariant & v, QDataStream & stream)
{
	if (v.userType() == QMetaType::QByteArray)
	{
		QByteArray array = v.value<QByteArray>();
		stream.writeRawData(array.data(), array.size());
		return true;
	}
	else if (v.userType() == QMetaType::QString)
	{
		QByteArray latin = v.toString().toLatin1();
		stream << latin.size();
		stream.writeRawData(latin.data(), latin.size());
		return true;
	}
	else if (v.userType() == qMetaTypeId<QVariantMap>())
	{
		QByteArray res;
		QDataStream stream(&res, QIODevice::WriteOnly);
		vipSafeVariantMapSave(stream, v.value<QVariantMap>());
		return true;
	}
	else
	{
		return QMetaType::save(stream, v.userType(), v.data());
	}
}

static bool fromByteArray(QDataStream & stream, QVariant & v, int max_size)
{
	if (v.userType() == QMetaType::QByteArray)
	{
		QByteArray ar(max_size, 0);
		stream.readRawData(ar.data(), max_size);
		v = QVariant::fromValue(ar);
		return true;
	}
	else if (v.userType() == QMetaType::QString)
	{
		int size;
		stream >> size;
		QByteArray ar(size, 0);
		stream.readRawData(ar.data(), size);
		v = QString(ar);
		return true;
	}
	else
	{
		return QMetaType::load(stream, v.userType(), v.data());
	}
}


#define WRITE_DEVICE(data,size) \
	if(m_device->write(data,size) != size) \
	{ this->setError("Cannot write data to the device"); return ;}

#define WRITE_DEVICE_INTEGER(value) \
		{int tmp = qToLittleEndian(value);\
		if(m_device->write(reinterpret_cast<char*>(&tmp),sizeof(tmp)) != sizeof(tmp)) \
		{ this->setError("Cannot write data to the device"); return ;}}

#define READ_DEVICE(data,size, m_device,return_value) \
		if(m_device->read(data,size) != size) \
		{this->setError("Cannot read data from the device"); return return_value;}

#define READ_DEVICE_INTEGER(value,m_device,return_value) \
		{ int tmp = 0;\
		if(m_device->read(reinterpret_cast<char*>(&tmp),(sizeof(tmp))) != sizeof(tmp)) \
		{this->setError("Cannot read data from the device"); return return_value;} \
		value = qFromLittleEndian(tmp);}

#define READ_DEVICE_INTEGER_BACKWARD(value,m_device,return_value) \
		{ int tmp = 0;\
		if(!m_device->seek(m_device->pos() - 4)) {this->setError("Cannot read data from the device"); return return_value;}  \
		if(m_device->read(reinterpret_cast<char*>(&tmp),(sizeof(tmp))) != sizeof(tmp)) \
		{this->setError("Cannot read data from the device"); return return_value;} \
		value = qFromLittleEndian(tmp);}


VipBinaryArchive::VipBinaryArchive()
	:VipArchive(Binary, ReadBackward), m_device(NULL)
{}

VipBinaryArchive::VipBinaryArchive(QIODevice * d)
	: VipArchive(Binary, ReadBackward), m_device(NULL)
{
	setDevice(d);
}

VipBinaryArchive::VipBinaryArchive(QByteArray * a, QIODevice::OpenMode mode)
	: VipArchive(Binary, ReadBackward), m_device(NULL)
{
	QBuffer * buf = new QBuffer(a, this);
	buf->open(mode);
	setDevice(buf);
}

VipBinaryArchive::VipBinaryArchive(const QByteArray & a)
	:VipArchive(Binary, ReadBackward), m_device(NULL)
{
	QBuffer * buf = new QBuffer(this);
	buf->setData(a);
	buf->open(QBuffer::ReadOnly);
	setDevice(buf);
}

VipBinaryArchive::VipBinaryArchive(const QString & filename, QIODevice::OpenMode mode)
	:VipArchive(Binary, ReadBackward), m_device(NULL)
{
	QFile * file = new QFile(this);
	file->setFileName(filename);
	file->open(mode);
	setDevice(file);
}

void	VipBinaryArchive::setDevice(QIODevice * d)
{
	if (d != m_device)
	{
		if (m_device)
		{
			m_device->close();
			if (m_device->parent() == this)
				delete m_device;
			m_device = NULL;
			setMode(NotOpen);
		}

		if (d)
		{
			m_device = d;
			if (d->openMode() & QIODevice::ReadOnly)
				setMode(Read);
			else if (d->openMode() & QIODevice::WriteOnly)
				setMode(Write);
		}
	}
}

QIODevice * VipBinaryArchive::device() const
{
	return const_cast<QIODevice*>(m_device);
}

void VipBinaryArchive::close()
{
	setDevice(NULL);
}

void VipBinaryArchive::save()
{
	qint64 pos = m_device ? m_device->pos() : 0;
	m_saved_pos.append(pos);
	VipArchive::save();
}

void VipBinaryArchive::restore()
{
	if (m_saved_pos.size())
	{
		qint64 pos = m_saved_pos.last();
		m_saved_pos.pop_back();
		if (m_device)
			m_device->seek(pos);
	}
	VipArchive::restore();
}

void VipBinaryArchive::doStart(QString & name, QVariantMap &, bool)
{
	if (mode() == Write)
	{
		//start is defined with a -1 and its name, and ends with & -1
		WRITE_DEVICE_INTEGER(-1);
		WRITE_DEVICE_INTEGER(name.size());
		WRITE_DEVICE(name.toLatin1().data(), name.size());
		WRITE_DEVICE_INTEGER(-1);
	}
	else if (mode() == Read)
	{
		//skip data until we find one with the right name
		while (true)
		{
			qint64 pos = m_device->pos();
			//TEST qint64
qint32 full_size = 0;
			QByteArray object_name;

			//read the value size;
			READ_DEVICE_INTEGER(full_size,m_device,);

			if (full_size == -2)
			{
				m_device->seek(pos);
				setError("No start tag found");
				break;
			}

			//read the name
			int size;
			READ_DEVICE_INTEGER(size, m_device,);
			object_name.resize(size);
			READ_DEVICE(object_name.data(), size, m_device, );
			READ_DEVICE_INTEGER(size, m_device,);

			if (full_size == -1)
			{
				if (!name.isEmpty() && object_name == name) //start name found, break
					break;
				else if (name.isEmpty()) //empt name: set name and break
				{
					name = object_name;
					break;
				}
			}
			else //otherwise, go to next data
			{
				m_device->seek(pos + 8 + full_size);
			}
		}
	}
}

void VipBinaryArchive::doEnd()
{
	if (mode() == Write)
	{
		//start is defined with a -2
		WRITE_DEVICE_INTEGER(-2);
	}
	else if (mode() == Read)
	{
		int level = 0;
		//skip data until we find a end tag at the right level
		while (true)
		{
			qint64 pos = m_device->pos();
			//TEST qint64
qint32 full_size = 0;

			//read the value size;
			READ_DEVICE_INTEGER(full_size, m_device,);

			if (full_size == -1) //start tag: increase level
				level++;
			if (full_size == -2)
			{
				if (level == 0)
					break;
				else
					--level;
			}
			else //otherwise, go to next data
			{
				m_device->seek(pos + 4 + full_size);
			}
		}
	}
}

QByteArray VipBinaryArchive::readBinary(const QString & n)
{
	resetError();
	if (mode() != Read)
		return QByteArray();

	QString & name = const_cast<QString&>(n);

	qint64 current_pos = m_device->pos();

	int full_size;
	int name_size = 0;
	QByteArray object_name;

	//skip data until we find one with the right name
	while (true)
	{
		qint64 pos = m_device->pos();

		//read the value size;
		if (readMode() == Forward)
		{
			READ_DEVICE_INTEGER(full_size, m_device,QByteArray());
		}
		else
		{
			//read the size, go back at the beginning of the data
			READ_DEVICE_INTEGER_BACKWARD(full_size, m_device, QByteArray());
			if (full_size > 0)
				m_device->seek(m_device->pos() - 4 - full_size);
		}

		if (full_size == -1 || full_size == -2) //start or end tag found: stop
		{
			m_device->seek(pos);
			setError("Cannot find content");
			return QByteArray();;
		}

		//read the name
		READ_DEVICE_INTEGER(name_size, m_device, QByteArray());
		object_name.resize(name_size);
		READ_DEVICE(object_name.data(), name_size, m_device, QByteArray());

		if (name.isEmpty())
		{
			name = object_name;
			break;
		}
		else if (object_name == name)
		{
			break;
		}
		else
		{
			if (readMode() == Forward)
				m_device->seek(pos + 8 + full_size);
			else
				m_device->seek(pos - 8 - full_size);

			current_pos = m_device->pos();
		}
	}

	QByteArray res = m_device->read(full_size - name_size - 4);

	//go to the next data
	if (readMode() == Forward) m_device->seek(current_pos + 8 + full_size);
	else  m_device->seek(current_pos - 8 - full_size);

	return res;
}


QVariant VipBinaryArchive::deserialize(const QByteArray & ar)
{
	QBuffer buffer(const_cast<QByteArray*>(&ar));
	buffer.open(QBuffer::ReadOnly);

	//read the type name
	QByteArray type_name;
	int size;
	if (buffer.read((char*)(&size), sizeof(size)) != sizeof(size))
		return QVariant();
	type_name.resize(size);
	if (buffer.read(type_name.data(), type_name.size()) != type_name.size())
		return QVariant();

	//create the value if necessary
	QVariant value = vipCreateVariant(type_name.data());
	if (!value.isValid())
	{
		setError("Cannot create QVariant value with type name ='" + type_name + "'");
		return QVariant();
	}

	bool serialized = false;
	VipBinaryArchive bin;
	bin.setDevice(&buffer);
	if (value.userType() >= QMetaType::User || value.userType() == QMetaType::QVariantMap)
	{
		//use the dispatcher approach
		VipFunctionDispatcher<2>::function_list_type lst = deserializeFunctions(value);
		if (lst.size())
		{
			for (int i = 0; i < lst.size(); ++i)
			{
				value = lst[i](value, &bin);
				if (hasError())
				{
					return QVariant();
				}
			}
			serialized = true;
		}
	}

	if (!serialized)
	{
		//use the standard approach
		int to_read = ar.size() - size - 4;
		QDataStream str(&buffer);
		buffer.seek(size + 4);
		str.setByteOrder(QDataStream::LittleEndian);
		if (!fromByteArray(str, value, to_read))
		{
			setError("Cannot create QVariant value with type name ='" + type_name + "'");
			return QVariant();
		}
	}
	return value;
}


void VipBinaryArchive::doContent(QString & name, QVariant & value, QVariantMap & metadata, bool read_metadata)
{
	Q_UNUSED(metadata);
	Q_UNUSED(read_metadata);

	if (mode() == Write)
	{
		QByteArray to_write;
		QDataStream stream(&to_write, QIODevice::WriteOnly);
		stream.setByteOrder(QDataStream::LittleEndian);

		//write the name
		stream << name.size();
		stream.writeRawData(name.toLatin1().data(), name.size());

		//special case: we are writing the content of a non sequential and opened QIODevice:
		//treat it as a QByteArray. The goal here is to avoid going through en intermediate buffer
		//and avoid a potentially HUGE copy that cound end up in a bad_alloc() exception

		QIODevice * device = value.value<QIODevice*>();
		bool use_device = (device && (device->openMode() & QIODevice::ReadOnly) && !device->isSequential());

		//write the type_name
		const char * type_name = use_device ? "QByteArray" : value.typeName();
		quint32 size = (quint32)( type_name ? strlen(type_name) : 0);
		stream << size;
		if(type_name)
			stream.writeRawData(type_name, size);

		//write the value content

		bool serialized = false;
		if (!use_device && (value.userType() >= QMetaType::User || value.userType() == QMetaType::QVariantMap))
		{
			//use the serialize dispatcher, but not in the QIODevice case

			VipFunctionDispatcher<2>::function_list_type lst = serializeFunctions(value);
			if (lst.size())
			{
				//call all serialize functions in a new archive
				VipBinaryArchive arch(&to_write, QIODevice::WriteOnly);
				this->copyFastTypes(arch);
				arch.device()->seek(to_write.size());
				for (int i = 0; i < lst.size(); ++i)
				{
					lst[i](value, &arch);
					if (arch.hasError())
						break;
				}
				serialized = true;
			}
		}

		if (!serialized)
		{
			if(use_device)
			{
				//write the input device content

				quint32 size = device->size() + to_write.size();
				WRITE_DEVICE_INTEGER(size);
				WRITE_DEVICE(to_write.data(), to_write.size());

				//write the device content by chunks of 10k
				QByteArray tmp(10000, 0);
				quint32 tot_size = 0;
				while (true) {
					quint32 read = device->read(tmp.data(), tmp.size());
					tot_size += read;
					WRITE_DEVICE(tmp.data(), read);
					if (read != (uint)tmp.size())
						break;
				}

				WRITE_DEVICE_INTEGER(size);
			}
			else
			{
				qint64 p = m_device->pos();
				//we need to write 4 bytes (size) + content of to_write
				WRITE_DEVICE_INTEGER(quint32(0));
				WRITE_DEVICE(to_write.data(), to_write.size());

				//then write serialized content
				QDataStream str(m_device);
				str.setByteOrder(QDataStream::LittleEndian);
				//m_device->seek(p + 4 + to_write.size());
				toByteArray(value, str);

				quint32 written_size = m_device->pos() - p - 4;
				//write end size
				WRITE_DEVICE_INTEGER(written_size);
				//write start size
				m_device->seek(p);
				WRITE_DEVICE_INTEGER(written_size);
				//go to end device
				m_device->seek(m_device->size());//p + 4 + written_size);



				//convert to QByteArray and write to device

				//QDataStream str(&to_write, QIODevice::WriteOnly);
				// str.setByteOrder(QDataStream::LittleEndian);
				// str.device()->seek(to_write.size());
				// toByteArray(value, str);
//
				// //write the data size, the data itself and once again the data size.
				// //a frame STARTS and ENDS with the data size
				// WRITE_DEVICE_INTEGER(to_write.size());
				// WRITE_DEVICE(to_write.data(), to_write.size());
				// WRITE_DEVICE_INTEGER(to_write.size());
			}
		}
		else
		{
			//write serialized data to device

			//write the data size, the data itself and once again the data size.
			//a frame STARTS and ENDS with the data size
			WRITE_DEVICE_INTEGER(to_write.size());
			WRITE_DEVICE(to_write.data(), to_write.size());
			WRITE_DEVICE_INTEGER(to_write.size());
		}
	}
	else if (mode() == Read)
	{
		qint64 current_pos = m_device->pos();

		int full_size;
		QByteArray object_name;

		//skip data until we find one with the right name
		while (true)
		{
			qint64 pos = m_device->pos();

			//read the value size;
			if (readMode() == Forward)
			{
				READ_DEVICE_INTEGER(full_size, m_device, );
			}
			else
			{
				//read the size, go back at the beginning of the data
				READ_DEVICE_INTEGER_BACKWARD(full_size, m_device, );
				if (full_size > 0)
					m_device->seek(m_device->pos() - 4 - full_size);
			}

			if (full_size == -1 || full_size == -2) //start or end tag found: stop
			{
				m_device->seek(pos);
				setError("Cannot find content");
				return;
			}

			//read the name
			int size;
			READ_DEVICE_INTEGER(size, m_device, );
			object_name.resize(size);
			READ_DEVICE(object_name.data(), size, m_device, );

			if (name.isEmpty())
			{
				name = object_name;
				break;
			}
			else if (object_name == name)
			{
				break;
			}
			else
			{
				if (readMode() == Forward)
					m_device->seek(pos + 8 + full_size);
				else
					m_device->seek(pos - 8 - full_size);

				current_pos = m_device->pos();
			}
		}

		//read the type name
		QByteArray type_name;
		int size;
		READ_DEVICE_INTEGER(size, m_device, );
		type_name.resize(size);
		READ_DEVICE(type_name.data(), size, m_device, );
		//const char * tp = type_name.data();
		//create the value if necessary
		if (!value.isValid())
		{
			value = vipCreateVariant(type_name.data());
			if (!value.isValid() && type_name.size())
			{
				setError("Cannot create QVariant value with type name ='" + type_name + "'");

				//skip the whole value
				if (readMode() == Forward) m_device->seek(current_pos + 8 + full_size);
				else  m_device->seek(current_pos - 8 - full_size);
				return;
			}
		}

		bool serialized = false;
		if (value.userType() >= QMetaType::User || value.userType() == QMetaType::QVariantMap)
		{
			//use the dispatcher approach
			VipFunctionDispatcher<2>::function_list_type lst = deserializeFunctions(value);
			if (lst.size())
			{
				//before calling the deserialize functions, save the read mode and reset it after
				ReadMode mode = readMode();
				setReadMode(Forward);

				for (int i = 0; i < lst.size(); ++i)
				{
					value = lst[i](value, this);
					if (hasError())
					{
						break;
						//skip the whole value
						if (readMode() == Forward) m_device->seek(current_pos + 8 + full_size);
						else  m_device->seek(current_pos - 8 - full_size);
						setReadMode(mode);
						return;
					}
				}

				setReadMode(mode);
				serialized = true;
			}
		}

		if (!serialized)
		{
			//use the standard approach
			int to_read = readMode() == Forward ? current_pos + 4 + full_size - m_device->pos() : current_pos - m_device->pos() - 4;
			QDataStream str(m_device);
			str.setByteOrder(QDataStream::LittleEndian);
			if (value.userType() && !fromByteArray(str, value, to_read))
			{
				setError("Cannot create QVariant value with type name ='" + type_name + "'");

				//skip the whole value
				if (readMode() == Forward) m_device->seek(current_pos + 8 + full_size);
				else  m_device->seek(current_pos - 8 - full_size);
				return;
			}
		}

		//read the end data size
		READ_DEVICE_INTEGER(size, m_device, );

		//go to the next data
		if (readMode() == Forward) m_device->seek(current_pos + 8 + full_size);
		else  m_device->seek(current_pos - 8 - full_size);


	}//end mode() == Read
}
