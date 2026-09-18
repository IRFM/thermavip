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

#include <chrono>
#include <condition_variable>
#include <vector>

#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QMetaProperty>
#include <QPointer>
#include <QReadWriteLock>
#include <QStringList>
#include <qcoreapplication.h>

#include "VipArchive.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipProcessingObject.h"
#include <cmath>
#include "VipSleep.h"
#include "VipTextOutput.h"
#include "VipUniqueId.h"
#include "VipXmlArchive.h"

namespace
{
	// The value read is passed to QThread::setPriority, so anything outside the
	// enumeration is refused rather than converted.
	QThread::Priority toThreadPriority(qint32 value)
	{
		switch (value) {
			case QThread::IdlePriority:
			case QThread::LowestPriority:
			case QThread::LowPriority:
			case QThread::NormalPriority:
			case QThread::HighPriority:
			case QThread::HighestPriority:
			case QThread::TimeCriticalPriority:
			case QThread::InheritPriority:
				return static_cast<QThread::Priority>(value);
			default:
				return QThread::InheritPriority;
		}
	}
}

// The two maps used to be aliased through a reinterpret_cast, which is undefined
// between distinct class types and wrote arbitrary stream values into an enum.
QDataStream& operator<<(QDataStream& str, const PriorityMap& map)
{
	str << static_cast<quint32>(map.size());
	for (PriorityMap::const_iterator it = map.begin(); it != map.end(); ++it)
		str << it.key() << static_cast<qint32>(it.value());
	return str;
}
QDataStream& operator>>(QDataStream& str, PriorityMap& map)
{
	map.clear();
	quint32 count = 0;
	str >> count;
	for (quint32 i = 0; i < count; ++i) {
		QString name;
		qint32 priority = QThread::InheritPriority;
		str >> name >> priority;
		if (str.status() != QDataStream::Ok)
			break;
		map.insert(name, toThreadPriority(priority));
	}
	return str;
}

QStringList VipAnyData::mergeAttributes(const QVariantMap& attrs)
{
	QStringList res;
	for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		QVariantMap::const_iterator found = m_attributes.find(it.key());
		if (found == m_attributes.end() || it.value() != found.value()) {
			m_attributes[it.key()] = it.value();
			res << it.key();
		}
	}
	return res;
}

qint64 VipAnyData::memoryFootprint() const
{
	return sizeof(qint64) * 2 + vipGetMemoryFootprint(d_data) + vipGetMemoryFootprint(QVariant::fromValue(m_attributes));
}

// make VipAnyData and VipErrorData serializable

VipArchive& operator<<(VipArchive& stream, const VipAnyData& any)
{
	stream.content("source", any.source()).content("time", any.time()).content("attributes", any.attributes());

	// the 'skip_data' attribute is used to disable serialize/deserialize of the QVariant data
	if (!stream.attribute("skip_data", false))
		stream.content("data", any.data());

	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipAnyData& any)
{
	any.setSource(stream.read("source").toLongLong());
	any.setTime(stream.read("time").toLongLong());
	any.setAttributes(stream.read("attributes").value<QVariantMap>());

	if (!stream.attribute("skip_data", false))
		any.setData(stream.read("data"));
	return stream;
}

QDataStream& operator<<(QDataStream& stream, const VipAnyData& any)
{
	return stream << any.source() << any.time() << any.attributes() << any.data();
}

QDataStream& operator>>(QDataStream& stream, VipAnyData& any)
{
	qint64 source = 0;
	qint64 time = VipInvalidTime;
	QVariantMap attributes;
	QVariant data;

	stream >> source >> time >> attributes >> data;

	any.setSource(source);
	any.setTime(time);
	any.setAttributes(attributes);
	any.setData(data);
	return stream;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<QVariantMap>();
	vipRegisterArchiveStreamOperators<VipAnyData>();

	qRegisterMetaType<VipAnyData>();
	qRegisterMetaType<VipAnyDataList>();
	return 0;
}
static int _regiterStreamOperators = vipStaticInit("vipAddInitializationFunction(registerStreamOperators)", []() { vipAddInitializationFunction(registerStreamOperators); });

const std::shared_ptr<const VipErrorData>& VipErrorHandler::_null_error()
{
	static const std::shared_ptr<const VipErrorData> inst = std::make_shared<const VipErrorData>();
	return inst;
}

class VipConnection::PrivateData
{
public:
	PrivateData()
	  : parent(nullptr)
	  , io(nullptr)
	  , openMode(UnknownConnection)
	{
	}

	// generic parameters
	VipProcessingObject* parent;
	VipProcessingIO* io;
	QByteArray parentClassName; // for debug purpose only

	// input/output parameters
	QString io_name;
	QString address;
	IOType openMode;
	// Guards the vector below, which the data path walks by index while the
	// editing of the graph empties or reallocates it. Every function takes the
	// lock of one connection at a time and never holds it while calling into
	// another object, so there is no order to invert.
	mutable VipSpinlock lock;
	VipConnectionVector connections;
};

VipConnection::VipConnection()
{
	VIP_CREATE_PRIVATE_DATA();
}

VipConnection::~VipConnection()
{
	// Qualified, and without setOpenMode(): a virtual call here would resolve to
	// this class, and the signal would reach a half destroyed object. Each
	// derived class closes what it owns in its own destructor.
	VipConnection::doClearConnection();
	d_data->address.clear();
	{
		VipUniqueLock<VipSpinlock> locker(d_data->lock);
		d_data->connections.clear();
	}
	d_data->openMode = UnknownConnection;
}

VipProcessingIO* VipConnection::parentProcessingIO() const
{
	return const_cast<VipProcessingIO*>(d_data->io);
}

void VipConnection::setParentProcessingObject(VipProcessingObject* proc, VipProcessingIO* io)
{
	d_data->parent = proc;
	d_data->io = io;
	d_data->parentClassName = proc ? QByteArray(proc->metaObject()->className()) : QByteArray();
}

VipProcessingObject* VipConnection::parentProcessingObject() const
{
	return const_cast<VipProcessingObject*>(d_data->parent);
}

VipConnectionVector VipConnection::connectionsCopy() const
{
	VipUniqueLock<VipSpinlock> locker(d_data->lock);
	return d_data->connections;
}

VipOutput* VipConnection::source() const
{
	VipConnectionPtr first;
	{
		VipUniqueLock<VipSpinlock> locker(d_data->lock);
		if (d_data->connections.size())
			first = d_data->connections.first();
	}
	if (first)
		if (VipProcessingIO* io = first->parentProcessingIO())
			return io->toOutput();
	return nullptr;
}


template<class T>
static qsizetype indexOfSharedVector(const QVector<QSharedPointer<T>>& vec, const T* val) noexcept
{
	for (qsizetype i = 0; i < vec.size(); ++i)
		if (vec[i].get() == val)
			return i;
	return -1;
}


void VipConnection::setupConnection(const QString& addr, const VipConnectionPtr& con)
{
	resetError();
	d_data->address = addr;

	// The former peers are taken aside first, then detached one at a time under
	// their own lock: this walked and mutated two vectors with none.
	VipConnectionVector previous;
	{
		VipUniqueLock<VipSpinlock> locker(d_data->lock);
		previous = d_data->connections;
		if (con)
			d_data->connections = VipConnectionVector() << con;
		else
			d_data->connections.clear();
	}

	for (int i = 0; i < previous.size(); ++i) {
		VipUniqueLock<VipSpinlock> locker(previous[i]->d_data->lock);
		qsizetype index = indexOfSharedVector(previous[i]->d_data->connections, this);
		if (index >= 0)
			previous[i]->d_data->connections.remove(index);
	}
}

bool VipConnection::openConnection(IOType type)
{
	resetError();
	doOpenConnection(type);
	if (openMode() > 0) {
		Q_EMIT connectionOpened(parentProcessingIO(), openMode(), d_data->address);
		return true;
	}
	return false;
}

QString VipConnection::removeClassNamePrefix(const QString& addr) const
{
	QString prefix = this->metaObject()->className();
	prefix += ":";
	if (addr.startsWith(prefix)) {
		QString res = addr;
		res.remove(0, prefix.length());
		return res;
	}
	return addr;
}

bool VipConnection::sendData(const VipAnyData& data)
{
	resetError();
	this->doSendData(data);
	if (!hasError()) {
		Q_EMIT dataSent(parentProcessingIO(), data);
		return true;
	}
	return false;
}

void VipConnection::clearConnection()
{
	resetError();
	doClearConnection();
	d_data->address.clear();
	{
		VipUniqueLock<VipSpinlock> locker(d_data->lock);
		d_data->connections.clear();
	}
	setOpenMode(UnknownConnection);
}

VipConnection::IOType VipConnection::openMode()
{
	return d_data->openMode;
}

void VipConnection::setOpenMode(IOType mode)
{
	int tmp = d_data->openMode;
	d_data->openMode = mode;

	if (tmp != UnknownConnection && mode == UnknownConnection)
		Q_EMIT connectionClosed(parentProcessingIO());
}

// The address of the output end of a connection, empty when that end is not
// attached to a processing object.
static QString outputConnectionAddress(const VipConnectionPtr& out)
{
	if (!out)
		return QString();
	VipProcessingObject* processing = out->parentProcessingObject();
	VipProcessingIO* io = out->parentProcessingIO();
	if (!processing || !io)
		return QString();
	if (VipProcessingPool* pool = processing->parentObjectPool())
		return "VipConnection:" + pool->objectName() + ";" + processing->objectName() + ";" + io->name();
	return "VipConnection:" + processing->objectName() + ";" + io->name();
}

// Name of the processing a connection belongs to, for logging.
static QString connectionProcessingName(const VipConnection* connection)
{
	if (connection)
		if (VipProcessingIO* io = connection->parentProcessingIO())
			if (VipProcessingObject* processing = io->parentProcessing())
				return processing->objectName();
	return QStringLiteral("<unattached connection>");
}

QString VipConnection::address() const
{
	// recompute the address if needed ( the VipProcessingObject name might have changed in the meantime)
	if (d_data->openMode == InputConnection) {
		// build connection from given VipConnection instances
		VipConnectionPtr back;
		{
			VipUniqueLock<VipSpinlock> locker(d_data->lock);
			if (d_data->connections.size())
				back = d_data->connections.back();
		}
		if (back) {
			// use the last (probably unique) connection which is the output
			const QString addr = outputConnectionAddress(back);
			if (!addr.isEmpty())
				const_cast<QString&>(d_data->address) = addr;
		}
	}
	return d_data->address;
}

QList<VipInput*> VipConnection::sinks() const
{
	QList<VipInput*> res;
	const VipConnectionVector connections = connectionsCopy();
	for (int i = 0; i < connections.size(); ++i)
		if (VipProcessingIO* io = connections[i]->parentProcessingIO())
			if (VipInput* in = io->toInput())
				res << in;
	return res;
}

QList<UniqueProcessingIO*> VipConnection::allSinks() const
{
	QList<UniqueProcessingIO*> res;
	const VipConnectionVector connections = connectionsCopy();
	for (int i = 0; i < connections.size(); ++i)
		if (VipProcessingIO* io = connections[i]->parentProcessingIO()) {
			if (VipInput* in = io->toInput())
				res << in;
			else if (VipProperty* p = io->toProperty())
				res << p;
		}
	return res;
}

void VipConnection::receiveData(const VipAnyData& data)
{
	VipProcessingIO* io = parentProcessingIO();
	if (!io)
		return;
	io->setData(data);
	Q_EMIT dataReceived(io, data);
}

void VipConnection::removeProcessingPoolFromAddress()
{
	if (!d_data->address.isEmpty()) {
		QString addr = removeClassNamePrefix(d_data->address);
		QStringList lst = addr.split(";");
		if (lst.size() == 3) {
			d_data->address = "VipConnection:" + lst[1] + ";" + lst[2];
		}
	}
}

void VipConnection::doOpenConnection(IOType type)
{
	if (type == InputConnection) {
		// build connection from given VipConnection instances
		VipConnectionPtr out;
		{
			VipUniqueLock<VipSpinlock> locker(d_data->lock);
			if (d_data->connections.size())
				// use the last (probably unique) connection which is the output
				out = d_data->connections.back();
		}
		if (out) {
			VipConnectionPtr in = sharedFromThis();

			// add this connection to output connection vector, under its own lock
			{
				VipUniqueLock<VipSpinlock> locker(out->d_data->lock);
				if (out->d_data->connections.indexOf(in) < 0)
					out->d_data->connections.append(in);
			}

			// save processing pool name if possible
			const QString addr = outputConnectionAddress(out);
			if (!addr.isEmpty())
				d_data->address = addr;
			{
				VipUniqueLock<VipSpinlock> locker(d_data->lock);
				d_data->connections = VipConnectionVector() << out;
			}
			this->setOpenMode(InputConnection);
		}
		// for Inputs only, build from an address: 'VipConnection:processing_name;processing_io_name'
		else if (d_data->address.length()) {
			QString addr = removeClassNamePrefix(d_data->address);
			QStringList lst = addr.split(";");
			VipProcessingObject* owner = parentProcessingObject();
			QObject* pool = owner ? owner->parentObjectPool() : nullptr;
			if (lst.size() == 3) {
				// When loading a player session, processing objects are first inserted in a temporary pool set as parent,
				// so use this pool and not the one given in the connection name
				if (!pool || !pool->property("_vip_useParentPool").toBool())
					pool = VipProcessingPool::findPool(lst[0]);
				lst = lst.mid(1);
			}

			if (!pool && owner) {
				// Use the parent object (like a VipProcessingBlock)
				pool = owner->parent();
			}

			if (pool && lst.size() == 2) {

				VipProcessingObject* dst = pool->findChild<VipProcessingObject*>(lst[0]);
				if (dst) {
					if (VipOutput* _output = dst->outputName(lst[1])) {
						VipConnectionPtr in = sharedFromThis();
						VipConnectionPtr out = _output->connection();

						{
							VipUniqueLock<VipSpinlock> locker(out->d_data->lock);
							if (out->d_data->connections.indexOf(in) < 0)
								out->d_data->connections.append(in);
						}

						const QString out_addr = outputConnectionAddress(out);
						if (!out_addr.isEmpty())
							d_data->address = out_addr;
						{
							VipUniqueLock<VipSpinlock> locker(d_data->lock);
							d_data->connections = VipConnectionVector() << out;
						}
						this->setOpenMode(InputConnection);
						return;
					}
				}
				else {
					VIP_LOG_ERROR("Cannot retrieve processing object with address ", d_data->address);
				}
			}

			const QString processing_name = connectionProcessingName(this);
			VIP_LOG_ERROR("Wrong connection format for " + processing_name + ", address: " + d_data->address);
			setError("Wrong connection format for " + processing_name, VipProcessingObject::ConnectionNotOpen);
			this->setOpenMode(UnknownConnection);
		}
	}
	else {
		this->setOpenMode(OutputConnection);
	}
}

void VipConnection::doSendData(const VipAnyData& data)
{
	// One copy taken under the lock, then the sends. Walking the member by index
	// let the editing of the graph empty it between the bound check and the read.
	const VipConnectionVector connections = connectionsCopy();
	for (int i = 0; i < connections.size(); ++i) {
		connections[i]->receiveData(data);
	}
}

void VipConnection::doClearConnection()
{
	//VipConnectionPtr con = sharedFromThis();
	VipConnectionVector connections;
	{
		VipUniqueLock<VipSpinlock> locker(d_data->lock);
		connections = d_data->connections;
		d_data->connections.clear();
	}

	for (int i = 0; i < connections.size(); ++i) {
		// Hold the peer: the callback below can drop the last reference to it.
		VipConnectionPtr peer = connections[i];

		bool removed = false;
		bool empty = false;
		{
			VipUniqueLock<VipSpinlock> locker(peer->d_data->lock);
			qsizetype index = indexOfSharedVector(peer->d_data->connections, this);
			if (index >= 0) {
				peer->d_data->connections.remove(index);
				removed = true;
				empty = peer->d_data->connections.isEmpty();
			}
		}

		if (removed) {
			auto* p = peer->d_data->parent;
			// The parent is only set by setParentProcessingObject: a connection
			// that was never attached has none. Called outside the lock, since it
			// walks back into the processing that owns the peer.
			if (p && empty)
				p->receiveConnectionClosed(peer->d_data->io);

			peer->checkClosedConnections();
		}
	}

	setOpenMode(UnknownConnection);
}

void VipConnection::checkClosedConnections()
{
	VipUniqueLock<VipSpinlock> locker(d_data->lock);
	const bool empty = d_data->connections.size() == 0;
	locker.unlock();
	if (empty)
		setOpenMode(UnknownConnection);
	else
		Q_EMIT connectionClosed(parentProcessingIO());
}

VipConnectionPtr VipConnection::newConnection()
{
	return VipConnectionPtr(new VipConnection());
}

VipConnectionPtr VipConnection::buildConnection(IOType, const QString& address, const VipConnectionPtr& con)
{
	if (con) {
		VipConnectionPtr c = newConnection();
		c->setupConnection(address, con);
		return c;
	}

	// look for the pattern 'connection_name:'

	int index = address.indexOf(":");
	if (index >= 0) {
		const QByteArray class_name = address.mid(0, index).toLatin1() + "*";

		// The address comes from a session file. The class it names used to be
		// instantiated straight away: the constructor of any registered type ran,
		// and when the cast below gave nothing the object was simply leaked. Ask
		// the type system what the class is before building one.
		const QMetaType type(vipIdFromName(class_name));
		const QMetaObject* meta = type.metaObject();
		if (!meta || !meta->inherits(&VipConnection::staticMetaObject)) {
			VIP_LOG_ERROR("Refused a connection address naming " + QString::fromLatin1(class_name) + ", which is not a connection");
			return VipConnectionPtr();
		}

		QVariant v = vipCreateVariant(class_name.data());
		VipConnectionPtr c(v.value<VipConnection*>());
		if (c) {
			c->setupConnection(address);
			return c;
		}
	}

	return VipConnectionPtr();
}

class VipProcessingIO::PrivateData
{
public:
	PrivateData()
	  : type(TypeInput)
	  , enable(true)
	  , parent(nullptr)
	{
	}
	Type type;
	bool enable;
	QString name;
	VipProcessingObject* parent;
};

VipProcessingIO::VipProcessingIO(Type t, const QString& name)
  : d_data(new PrivateData())
{
	d_data->type = t;
	d_data->name = name;
}

VipProcessingIO::VipProcessingIO(const VipProcessingIO& other)
  : d_data(other.d_data)
{
}

VipProcessingIO::~VipProcessingIO() {}

void VipProcessingIO::setEnabled(bool enable)
{
	d_data->enable = enable;
}

bool VipProcessingIO::isEnabled() const
{
	return d_data->enable;
}

VipInput* VipProcessingIO::toInput() const
{
	if (d_data->type == TypeInput)
		return static_cast<VipInput*>(const_cast<VipProcessingIO*>(this));
	else
		return nullptr;
}

VipMultiInput* VipProcessingIO::toMultiInput() const
{
	if (d_data->type == TypeMultiInput)
		return static_cast<VipMultiInput*>(const_cast<VipProcessingIO*>(this));
	else
		return nullptr;
}

VipProperty* VipProcessingIO::toProperty() const
{
	if (d_data->type == TypeProperty)
		return static_cast<VipProperty*>(const_cast<VipProcessingIO*>(this));
	else
		return nullptr;
}

VipMultiProperty* VipProcessingIO::toMultiProperty() const
{
	if (d_data->type == TypeMultiProperty)
		return static_cast<VipMultiProperty*>(const_cast<VipProcessingIO*>(this));
	else
		return nullptr;
}

VipOutput* VipProcessingIO::toOutput() const
{
	if (d_data->type == TypeOutput)
		return static_cast<VipOutput*>(const_cast<VipProcessingIO*>(this));
	else
		return nullptr;
}

VipMultiOutput* VipProcessingIO::toMultiOutput() const
{
	if (d_data->type == TypeMultiOutput)
		return static_cast<VipMultiOutput*>(const_cast<VipProcessingIO*>(this));
	else
		return nullptr;
}

VipProcessingIO::Type VipProcessingIO::type() const
{
	return d_data->type;
}

QString VipProcessingIO::name() const
{
	return d_data->name;
}

VipProcessingObject* VipProcessingIO::parentProcessing() const
{
	return const_cast<VipProcessingObject*>(d_data->parent);
}

void VipProcessingIO::setName(const QString& name)
{
	if (d_data->parent) {
		if (d_data->type == TypeOutput || d_data->type == TypeMultiOutput)
			d_data->name = d_data->parent->generateUniqueOutputName(*this, name);
		else
			d_data->name = d_data->parent->generateUniqueInputName(*this, name);
	}
	else
		d_data->name = name;
}

/* void VipProcessingIO::setData(const VipAny& any)
{
	setData(VipAnyData(any.variant, VipInvalidTime));
}*/

void VipProcessingIO::setData(const QVariant& data, qint64 time)
{
	setData(VipAnyData(data, time));
}

void VipProcessingIO::setData(QVariant&& data, qint64 time)
{
	setData(VipAnyData(std::move(data), time));
}

void VipProcessingIO::setParentProcessing(VipProcessingObject* parent)
{
	d_data->parent = parent;
	setName(d_data->name);
}

void VipProcessingIO::dirtyParentProcessingIO(VipProcessingIO* io)
{
	if (d_data->parent)
		d_data->parent->dirtyProcessingIO(io);
}

class UniqueProcessingIO::PrivateData
{
public:
	~PrivateData()
	{
		if (connection) {
			connection->clearConnection();
		}
	}
	VipConnectionPtr connection;
};

UniqueProcessingIO::UniqueProcessingIO(Type t, const QString& name)
  : VipProcessingIO(t, name)
  , d_data(new PrivateData())
{
	// make sure the connection is ALWAYS valid
	setConnection(VipConnection::newConnection());
}

UniqueProcessingIO::UniqueProcessingIO(const UniqueProcessingIO& other)
  : VipProcessingIO(other)
  , d_data(other.d_data)
{
	// setParentProcessing(other.parentProcessing());
	// setConnection(other.connection());
}

UniqueProcessingIO::~UniqueProcessingIO()
{
	//clearConnection();
}

void UniqueProcessingIO::setParentProcessing(VipProcessingObject* parent)
{
	VipProcessingIO::setParentProcessing(parent);
	if (d_data->connection)
		setConnection(d_data->connection);
}

void UniqueProcessingIO::setConnection(const VipConnectionPtr& c)
{
	// make sure the connection is ALWAYS valid
	Q_ASSERT(c);

	if (d_data->connection && parentProcessing()) {
		QObject::disconnect(d_data->connection.data(), SIGNAL(error(QObject*, const VipErrorData&)), parentProcessing(), SLOT(emitError(QObject*, const VipErrorData&)));
		QObject::disconnect(
		  d_data->connection.data(), SIGNAL(connectionOpened(VipProcessingIO*, int, const QString&)), parentProcessing(), SLOT(receiveConnectionOpened(VipProcessingIO*, int, const QString&)));
		QObject::disconnect(d_data->connection.data(), SIGNAL(connectionClosed(VipProcessingIO*)), parentProcessing(), SLOT(receiveConnectionClosed(VipProcessingIO*)));
		QObject::disconnect(
		  d_data->connection.data(), SIGNAL(dataReceived(VipProcessingIO*, const VipAnyData&)), parentProcessing(), SLOT(receiveDataReceived(VipProcessingIO*, const VipAnyData&)));
		QObject::disconnect(d_data->connection.data(), SIGNAL(dataSent(VipProcessingIO*, const VipAnyData&)), parentProcessing(), SLOT(receiveDataSent(VipProcessingIO*, const VipAnyData&)));
	}
	d_data->connection = c;
	if (c) {
		d_data->connection->setParentProcessingObject(parentProcessing(), this);
		if (parentProcessing()) {
			QObject::connect(
			  d_data->connection.data(), SIGNAL(error(QObject*, const VipErrorData&)), parentProcessing(), SLOT(emitError(QObject*, const VipErrorData&)), Qt::DirectConnection);
			QObject::connect(d_data->connection.data(),
					 SIGNAL(connectionOpened(VipProcessingIO*, int, const QString&)),
					 parentProcessing(),
					 SLOT(receiveConnectionOpened(VipProcessingIO*, int, const QString&)),
					 Qt::DirectConnection);
			QObject::connect(
			  d_data->connection.data(), SIGNAL(connectionClosed(VipProcessingIO*)), parentProcessing(), SLOT(receiveConnectionClosed(VipProcessingIO*)), Qt::DirectConnection);
			QObject::connect(d_data->connection.data(),
					 SIGNAL(dataReceived(VipProcessingIO*, const VipAnyData&)),
					 parentProcessing(),
					 SLOT(receiveDataReceived(VipProcessingIO*, const VipAnyData&)),
					 Qt::DirectConnection);
			QObject::connect(d_data->connection.data(),
					 SIGNAL(dataSent(VipProcessingIO*, const VipAnyData&)),
					 parentProcessing(),
					 SLOT(receiveDataSent(VipProcessingIO*, const VipAnyData&)),
					 Qt::DirectConnection);

			// for standard VipConnection type, directly open the connection
			// if(strcmp(d_data->connection->metaObject()->className(),"VipConnection") == 0)
			// {
			// if( type() < TypeOutput)
			// d_data->connection->openConnection(VipConnection::InputConnection);
			// else
			// d_data->connection->openConnection(VipConnection::OutputConnection);
			// }
		}
	}
}

bool UniqueProcessingIO::setConnection(UniqueProcessingIO* dst)
{
	Q_ASSERT(dst);

	if (parentProcessing() && parentProcessing() == dst->parentProcessing()) {
		VIP_LOG_WARNING("Trying to connect a source and sink that belong to the same processing object");
		return false;
	}

	VipConnectionPtr dst_con = dst->connection();
	VipConnectionPtr this_con = this->connection();

	// only reset the connections if they are not bare VipConnection type
	if (strcmp(dst_con->metaObject()->className(), "VipConnection") != 0)
		dst_con = VipConnection::newConnection();

	if (strcmp(this_con->metaObject()->className(), "VipConnection") != 0)
		this_con = VipConnection::newConnection();

	if (type() < TypeOutput) {
		this_con->setupConnection(QString(), dst_con);
	}
	else {
		dst_con->setupConnection(QString(), this_con);
	}

	this->setConnection(this_con);
	dst->setConnection(dst_con);

	// for standard VipConnection type, directly open the connection
	if (type() < TypeOutput) {
		this_con->openConnection(VipConnection::InputConnection);
		dst_con->openConnection(VipConnection::OutputConnection);
	}
	else {
		this_con->openConnection(VipConnection::OutputConnection);
		dst_con->openConnection(VipConnection::InputConnection);
	}

	if (VipProperty* p = this->toProperty())
		if (VipOutput* out = dst->toOutput())
			p->setData(out->data());

	if (VipProperty* p = dst->toProperty())
		if (VipOutput* out = this->toOutput())
			p->setData(out->data());

	return true;
}

bool UniqueProcessingIO::setConnection(const QString& address, const VipConnectionPtr& con)
{
	VipConnectionPtr c;
	if (type() < TypeOutput)
		c = VipConnection::buildConnection(VipConnection::InputConnection, address, con);
	else
		c = VipConnection::buildConnection(VipConnection::OutputConnection, address, con);

	if (c) {
		setConnection(c);
		return true;
	}
	else
		return false;
}

VipConnectionPtr UniqueProcessingIO::connection() const
{
	return d_data->connection;
}

VipOutput* UniqueProcessingIO::source() const
{
	return d_data->connection ? d_data->connection->source() : nullptr;
}

void UniqueProcessingIO::clearConnection()
{
	if (d_data->connection)
		d_data->connection->clearConnection();
}

VipInput::VipInput(const QString& name, VipProcessingObject* parent)
  : UniqueProcessingIO(TypeInput, name)
  , m_input_list(new VipFIFOList())
{
	setParentProcessing(parent);
}

VipInput::VipInput(const VipInput& other)
  : UniqueProcessingIO(other)
  , m_input_list(other.m_input_list)
{
}

void VipInput::setData(VipAnyData&& data)
{
	// only accept the data if parent processing is enabled
	if (VipProcessingObject* proc = parentProcessing()) {
		if (proc->isEnabled() && isEnabled()) {
			if (!(proc->scheduleStrategies() & VipProcessingObject::Asynchronous)) {
				// synchrone mode: only 1 data allowed in the input buffer
				m_input_list->reset(std::move(data));
			}
			else {
				int previous_size = 0;
				int current_size = m_input_list->push(std::move(data), &previous_size);

				// only update the parent processing if:
				// - the input size has been increased
				if ((previous_size != current_size))
					proc->update();

				if ((previous_size >= current_size)) {
					// the input data has been dropped, print debug info if required
					if (proc->isLogErrorEnabled(VipProcessingObject::InputBufferFull)) {
						const QString log = QString("drop input data, buffer size = ") + QString::number(m_input_list->remaining()) + QString(", buffer memory footprint = ") +
								    QString::number(m_input_list->memoryFootprint());

						proc->setError(log, VipProcessingObject::InputBufferFull);
					}
				}
			}
		}
	}
}

void VipInput::setData(const VipAnyData& data)
{
	// only accept the data if parent processing is enabled
	if (VipProcessingObject* proc = parentProcessing()) {
		if (proc->isEnabled() && isEnabled()) {
			if (!(proc->scheduleStrategies() & VipProcessingObject::Asynchronous)) {
				// synchrone mode: only 1 data allowed in the input buffer
				m_input_list->reset(data);
			}
			else {
				int previous_size = 0;
				int current_size = m_input_list->push(data, &previous_size);

				// only update the parent processing if:
				// - the input size has been increased
				if (VIP_LIKELY(previous_size != current_size))
					proc->update();

				if (VIP_UNLIKELY(previous_size >= current_size)) {
					// the input data has been dropped, print debug info if required
					if (proc->isLogErrorEnabled(VipProcessingObject::InputBufferFull)) {
						const QString log = QString("drop input data, buffer size = ") + QString::number(m_input_list->remaining()) + QString(", buffer memory footprint = ") +
								    QString::number(m_input_list->memoryFootprint());

						proc->setError(log, VipProcessingObject::InputBufferFull);
					}
				}
			}
		}
	}
}

void VipInput::setListType(VipDataList::Type t, int list_limit_type, int max_list_size, int max_memory_size)
{
	if (t == VipDataList::FIFO)
		m_input_list = QSharedPointer<VipDataList>(new VipFIFOList());
	else if (t == VipDataList::LIFO)
		m_input_list = QSharedPointer<VipDataList>(new VipLIFOList());
	else if (t == VipDataList::LastAvailable)
		m_input_list = QSharedPointer<VipDataList>(new VipLastAvailableList());

	m_input_list->setListLimitType(list_limit_type);
	m_input_list->setMaxListSize(max_list_size);
	m_input_list->setMaxListMemory(max_memory_size);
}

void VipInput::setListType(VipDataList* lst)
{
	m_input_list.reset(lst);
}

VipMultiInput::VipMultiInput(const QString& name, VipProcessingObject* parent)
  : VipMultipleProcessingIO(TypeMultiInput, name)
  , m_type(VipDataList::FIFO)
  , m_list_limit_type(VipProcessingManager::listLimitType())
  , m_max_list_size(VipProcessingManager::maxListSize())
  , m_max_list_memory(VipProcessingManager::maxListMemory())
{
	setParentProcessing(parent);
}

VipMultiInput::VipMultiInput(const VipMultiInput& other)
  : VipMultipleProcessingIO(other)
  , m_type(other.m_type)
  , m_list_limit_type(other.m_list_limit_type)
  , m_max_list_size(other.m_max_list_size)
  , m_max_list_memory(other.m_max_list_memory)
{
}

void VipMultiInput::setListType(VipDataList::Type t, int list_limit_type, int max_list_size, int max_memory_size)
{
	for (int i = 0; i < count(); ++i)
		at(i)->setListType(t, list_limit_type, max_list_size, max_memory_size);

	m_type = t;
	m_list_limit_type = list_limit_type;
	m_max_list_size = max_list_size;
	m_max_list_memory = max_memory_size;
}

void VipMultiInput::added(VipInput* input)
{
	input->setListType(m_type, m_list_limit_type, m_max_list_size, m_max_list_memory);
}

VipOutput::VipOutput(const QString& name, VipProcessingObject* parent)
  : UniqueProcessingIO(TypeOutput, name)
  , d_data(new VipAnyData())
  , m_bufferize_outputs(false)
{
	setParentProcessing(parent);
}

VipOutput::VipOutput(const VipOutput& other)
  : UniqueProcessingIO(other)
  , d_data(other.d_data)
  , m_buffer(other.m_buffer)
  , m_bufferize_outputs(other.m_bufferize_outputs)
{
}

VipOutput& VipOutput::operator=(const VipOutput& other)
{
	if (this == &other)
		return *this;
	static_cast<UniqueProcessingIO&>(*this) = static_cast<const UniqueProcessingIO&>(other);
	// The current data was the one member left behind, so an assigned output kept
	// serving its own; the sibling class assigns it.
	d_data = other.d_data;
	m_bufferize_outputs = other.m_bufferize_outputs;
	m_buffer = other.m_buffer;
	return *this;
}

void VipOutput::setBufferDataEnabled(bool enable)
{
	if (m_bufferize_outputs != enable) {
		m_bufferize_outputs = enable;
		if (!enable) {
			VipUniqueLock<VipSpinlock> lock(m_buffer_lock);
			m_buffer.clear();
		}
	}
}
bool VipOutput::bufferDataEnabled() const
{
	return m_bufferize_outputs;
}
VipAnyDataList VipOutput::clearBufferedData()
{
	VipAnyDataList res;
	VipUniqueLock<VipSpinlock> lock(m_buffer_lock);
	res = m_buffer;
	m_buffer.clear();
	return res;
}

int VipOutput::bufferDataSize()
{
	VipUniqueLock<VipSpinlock> lock(m_buffer_lock);
	return m_buffer.size();
}

VipAnyData VipOutput::data() const
{
	VipUniqueLock<VipSpinlock> lock(const_cast<VipSpinlock&>(m_data_lock));
	return *d_data;
}

void VipOutput::setData(const VipAnyData& d)
{
	{
		VipUniqueLock<VipSpinlock> lock(m_data_lock);
		*d_data = d;
	}
	if (isEnabled()) {
		// The datum sent is the one received, not a re-read of the shared member:
		// another thread setting this output between the two used to make this call
		// send its datum instead of ours. Sending happens outside the lock, since it
		// reaches arbitrary code downstream.
		VipAnyData sent = d;
		if (VipProcessingObject* obj = parentProcessing())
			obj->setOutputDataTime(sent);
		connection()->sendData(sent);
		if (m_custom_sender)
			m_custom_sender(sent);
		if (m_bufferize_outputs) {
			VipUniqueLock<VipSpinlock> lock(m_buffer_lock);
			m_buffer.push_back(d);
		}
	}
}

VipMultiOutput::VipMultiOutput(const QString& name, VipProcessingObject* parent)
  : VipMultipleProcessingIO(TypeMultiOutput, name)
{
	setParentProcessing(parent);
}

VipMultiOutput::VipMultiOutput(const VipMultiOutput& other)
  : VipMultipleProcessingIO(other)
{
}

VipProperty::VipProperty(const QString& name, VipProcessingObject* parent)
  : UniqueProcessingIO(TypeProperty, name)
  , d_data(new VipAnyData())
{
	setParentProcessing(parent);
}

VipProperty::VipProperty(const VipProperty& other)
  : UniqueProcessingIO(other)
  , d_data(other.d_data)
{
}

VipProperty& VipProperty::operator=(const VipProperty& other)
{
	static_cast<UniqueProcessingIO&>(*this) = static_cast<const UniqueProcessingIO&>(other);
	d_data = other.d_data;
	return *this;
}
VipAnyData VipProperty::data() const
{
	VipUniqueLock<VipSpinlock> lock(const_cast<VipSpinlock&>(m_lock));
	return *d_data;
}

void VipProperty::setData(const VipAnyData& d)
{
	{
		VipUniqueLock<VipSpinlock> lock(m_lock);
		*d_data = d;
	}
	// only emit signal if parent processing is enabled
	if (parentProcessing() && parentProcessing()->isEnabled() && isEnabled()) {
		parentProcessing()->emitProcessingChanged();
	}
}

VipMultiProperty::VipMultiProperty(const QString& name, VipProcessingObject* parent)
  : VipMultipleProcessingIO(TypeMultiProperty, name)
{
	setParentProcessing(parent);
}

VipMultiProperty::VipMultiProperty(const VipMultiProperty& other)
  : VipMultipleProcessingIO(other)
{
}

#define LOCK(mutex) QMutexLocker lock_m(const_cast<QMutex*>(&mutex));
#define SPIN_LOCK(mutex) VipUniqueLock<VipSpinlock> lock_p(const_cast<VipSpinlock&>(mutex));

// We gather all static variables related to VipProcessingObject inside VipProcessingManager
class VipProcessingManager::PrivateData
{
public:
	PrivateData() = default;

	// errors is a copy, taken in the initialiser list: filling _log_errors in the
	// body left the active set empty, so no error code was ever logged.
	static QSet<int> defaultLogErrors()
	{
		return QSet<int>{ VipProcessingObject::RuntimeError,	   VipProcessingObject::WrongInput,	VipProcessingObject::WrongInputNumber,
				  VipProcessingObject::ConnectionNotOpen, VipProcessingObject::DeviceNotOpen, VipProcessingObject::IOError };
	}

	// global default values
	int _list_limit_type = VipDataList::MemorySize;
	int _max_list_size = INT_MAX;
	qint64 _max_list_memory = 50000000;
	QSet<int> _log_errors = defaultLogErrors();
	bool _lock_list_manager = false;

	QMutex mutex;
	int list_limit_type = VipDataList::MemorySize;
	int max_list_size = INT_MAX;
	qint64 max_list_memory = 50000000;
	ErrorCodes errors = defaultLogErrors();
	PriorityMap priorities;
	QList<VipDataList*> instances;
	QList<VipProcessingObject*> processingInstances;

	// additional VipProcessingObject info
	QMultiMap<int, VipProcessingObject::Info> _infos;
	int _obj_types = 0;
	int _obj_infos = 0;
	int _dirty_objects = 1;
	QRecursiveMutex _additional_info_mutex;
	QList<const VipProcessingObject*> _allObjects;
};

VipProcessingManager::~VipProcessingManager() {}

void VipProcessingManager::setDefaultPriority(QThread::Priority priority, const QMetaObject* meta)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);
		instance().d_data->priorities[meta->className()] = priority;
	}
	applyAll();
	Q_EMIT instance().changed();
}
int VipProcessingManager::defaultPriority(const QMetaObject* meta)
{
	QMutexLocker lock(&instance().d_data->mutex);
	PriorityMap::const_iterator it = instance().d_data->priorities.constFind(meta->className());
	if (it != instance().d_data->priorities.constEnd())
		return it.value();
	return QThread::InheritPriority;
}
void VipProcessingManager::setDefaultPriorities(const PriorityMap& prio)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);
		instance().d_data->priorities = prio;
	}
	applyAll();
	Q_EMIT instance().changed();
}
PriorityMap VipProcessingManager::defaultPriorities()
{
	return instance().d_data->priorities;
}

static QThread::Priority findPriority(const PriorityMap& prio, VipProcessingObject* obj)
{
	const QMetaObject* meta = obj->metaObject();
	while (meta) {
		PriorityMap::const_iterator it = prio.find(meta->className());
		if (it != prio.end())
			return (QThread::Priority)it.value();
		meta = meta->superClass();
	}
	return QThread::InheritPriority;
}

void VipProcessingManager::applyAll()
{
	// The lock is taken here, to read the registers and the defaults, and released
	// before any of the objects below is touched: this function calls into other
	// objects and emits, and holding the mutex across that invites an inversion
	// with whatever lock those objects take. The callers no longer hold it.
	QList<VipDataList*> all;
	QList<VipProcessingObject*> procs;
	int previous_limit_type, previous_max_size, limit_type, max_size;
	qint64 previous_max_memory, max_memory;
	ErrorCodes previous_errors, errors;
	PriorityMap priorities;
	{
		QMutexLocker lock(&instance().d_data->mutex);
		all = instance().d_data->instances;
		procs = instance().d_data->processingInstances;
		previous_limit_type = instance().d_data->_list_limit_type;
		previous_max_size = instance().d_data->_max_list_size;
		previous_max_memory = instance().d_data->_max_list_memory;
		previous_errors = instance().d_data->_log_errors;
		limit_type = instance().d_data->list_limit_type;
		max_size = instance().d_data->max_list_size;
		max_memory = instance().d_data->max_list_memory;
		errors = instance().d_data->errors;
		priorities = instance().d_data->priorities;
	}

	for (int i = 0; i < all.size(); ++i) {
		// only apply the parameters if they are the default ones
		VipDataList* lst = all[i];
		if (lst->listLimitType() == previous_limit_type && lst->maxListSize() == previous_max_size && lst->maxListMemory() == previous_max_memory) {
			all[i]->setListLimitType(limit_type);
			all[i]->setMaxListSize(max_size);
			all[i]->setMaxListMemory(max_memory);
		}
	}

	for (int i = 0; i < procs.size(); ++i) {
		// only apply the parameters if they are the default ones
		VipProcessingObject* proc = procs[i];
		if (proc->isBeingDestroyed())
			continue;
		if (proc->logErrors() == previous_errors) {
			proc->setLogErrors(errors);
		}

		// set priority
		if (proc->priority() == QThread::InheritPriority) {
			proc->setPriority(findPriority(priorities, proc));
		}
	}

	QMutexLocker lock(&instance().d_data->mutex);
	instance().d_data->_log_errors = errors;
	instance().d_data->_list_limit_type = limit_type;
	instance().d_data->_max_list_size = max_size;
	instance().d_data->_max_list_memory = max_memory;
}

void VipProcessingManager::setLogErrorEnabled(int error_code, bool enable)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);
		if (enable) {
			instance().d_data->errors.insert(error_code);
		}
		else {
			instance().d_data->errors.remove(error_code);
		}
	}
	Q_EMIT instance().changed();
}

bool VipProcessingManager::isLogErrorEnabled(int error)
{
	QMutexLocker lock(&instance().d_data->mutex);
	bool found = (instance().d_data->errors.find(error) != instance().d_data->errors.end());
	return found;
}

void VipProcessingManager::setLogErrors(const QSet<int>& errors)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);

		if (errors.contains(0)) {
			// Patch (3.10.0): handle the old AllErrorsExcept (value 0)
			QSet<int> errs = errors;
			errs.remove(0);
			instance().d_data->errors.clear();
			instance().d_data->errors << VipProcessingObject::RuntimeError << VipProcessingObject::WrongInput << VipProcessingObject::InputBufferFull
						  << VipProcessingObject::WrongInputNumber << VipProcessingObject::ConnectionNotOpen << VipProcessingObject::DeviceNotOpen
						  << VipProcessingObject::IOError;
			for (auto it = errs.begin(); it != errs.end(); ++it)
				instance().d_data->errors.remove(*it);
		}
		else
			instance().d_data->errors = errors;
	}
	// Outside the lock: a receiver of this signal may well come back into the
	// manager.
	Q_EMIT instance().changed();
}

QSet<int> VipProcessingManager::logErrors()
{
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->errors;
}

void VipProcessingManager::setLocked(bool locked)
{
	QMutexLocker lock(&instance().d_data->mutex);
	instance().d_data->_lock_list_manager = locked;
}

void VipProcessingManager::setListLimitType(int type)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);
		instance().d_data->list_limit_type = type;
	}
	applyAll();
	Q_EMIT instance().changed();
}

void VipProcessingManager::setMaxListSize(int size)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);
		instance().d_data->max_list_size = size;
	}
	applyAll();
	Q_EMIT instance().changed();
}

void VipProcessingManager::setMaxListMemory(qint64 size)
{
	{
		QMutexLocker lock(&instance().d_data->mutex);
		instance().d_data->max_list_memory = size;
	}
	applyAll();
	Q_EMIT instance().changed();
}

int VipProcessingManager::listLimitType()
{
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->list_limit_type;
}
int VipProcessingManager::maxListSize()
{
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->max_list_size;
}
qint64 VipProcessingManager::maxListMemory()
{
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->max_list_memory;
}

QList<VipDataList*> VipProcessingManager::dataListInstances()
{
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->instances;
}

QList<VipProcessingObject*> VipProcessingManager::processingObjectInstances()
{
	QMutexLocker lock(&instance().d_data->mutex);
	return instance().d_data->processingInstances;
}

VipProcessingManager& VipProcessingManager::instance()
{
	static VipProcessingManager instance;
	return instance;
}

VipProcessingManager::VipProcessingManager()
{
	VIP_CREATE_PRIVATE_DATA();
}

void VipProcessingManager::add(VipDataList* lst)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->instances.append(lst);
}

void VipProcessingManager::remove(VipDataList* lst)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->instances.removeOne(lst);
}

void VipProcessingManager::add(VipProcessingObject* obj)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->processingInstances.append(obj);
}
void VipProcessingManager::remove(VipProcessingObject* obj)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->processingInstances.removeOne(obj);
}

VipDataList::VipDataList()
  : m_max_size(VipProcessingManager::maxListSize())
  , m_max_memory(VipProcessingManager::maxListMemory())
  , m_data_limit_type(VipProcessingManager::listLimitType())
{
	VipProcessingManager::instance().add(this);
}

VipDataList::~VipDataList()
{
	VipProcessingManager::instance().remove(this);
}

#define _SPINLOCKER() VipUniqueLock<VipSpinlock> _lock(const_cast<VipSpinlock&>(m_mutex))
#define _SHAREDSPINLOCKER() VipUniqueLock<VipSpinlock> _lock(const_cast<VipSpinlock&>(m_mutex))

VipFIFOList::VipFIFOList()
  : VipDataList()
{
}

int VipFIFOList::push(const VipAnyData& data, int* previous)
{
	_SPINLOCKER();

	if (previous)
		*previous = (int)m_list.size();
	m_list.push_back(data);

	if (const int limits = listLimitType()) {
		if (limits & Number) {
			while (static_cast<int>(m_list.size()) > this->maxListSize())
				m_list.pop_front();
		}
		if (limits & MemorySize) {
			// The sum overflows before the cap is reached as soon as the buffer
			// holds more than two gigabytes, and a negative sum never satisfies
			// the test: nothing was evicted at all.
			qsizetype i = 0;
			qint64 size = 0;
			for (i = (qsizetype)m_list.size() - 1; i >= 0; --i) {
				size += m_list[i].memoryFootprint();
				if (size >= maxListMemory())
					break;
			}
			if (i >= 0)
				m_list.erase(m_list.begin(), m_list.begin() + i);
		}
	}
	return (int)m_list.size();
}

int VipFIFOList::push(VipAnyData&& data, int* previous)
{
	_SPINLOCKER();

	if (previous)
		*previous = (int)m_list.size();
	m_list.push_back(std::move(data));

	if (const int limits = listLimitType()) {
		if (limits & Number) {
			while (static_cast<int>(m_list.size()) > this->maxListSize())
				m_list.pop_front();
		}
		if (limits & MemorySize) {

			// The sum overflows before the cap is reached as soon as the buffer
			// holds more than two gigabytes, and a negative sum never satisfies
			// the test: nothing was evicted at all.
			qsizetype i = 0;
			qint64 size = 0;
			for (i = (qsizetype)m_list.size() - 1; i >= 0; --i) {
				size += m_list[i].memoryFootprint();
				if (size >= maxListMemory())
					break;
			}
			if (i >= 0) {
				m_list.erase(m_list.begin(), m_list.begin() + i);
			}
		}
	}
	return (int)m_list.size();
}

void VipFIFOList::reset(const VipAnyData& data)
{
	_SPINLOCKER();
	if (m_list.size() == 1) {
		m_list.back() = data;
	}
	else {
		m_list.clear();
		m_list.push_back(data);
	}
}
void VipFIFOList::reset(VipAnyData&& data)
{
	_SPINLOCKER();
	if (m_list.size() == 1) {
		m_list.back() = std::move(data);
	}
	else {
		m_list.clear();
		m_list.push_back(std::move(data));
	}
}

VipAnyData VipFIFOList::next()
{
	_SPINLOCKER();
	if (m_list.size() > 0) {
		m_last = std::move(m_list.front());
		m_list.pop_front();
	}
	return m_last;
}

bool VipFIFOList::readAll(VipAnyDataList& lst)
{
	_SPINLOCKER();
	if (m_list.size() > 0) {
		lst.resize(m_list.size());
		m_last = m_list.back();
		auto it = lst.begin();
		m_list.for_each(0, m_list.size(), [&it](auto& v) { *it++ = std::move(v); });
		// std::move(m_list.begin(), m_list.end(), lst.begin());
		m_list.clear();
		return true;
	}
	return false;
}

VipAnyData VipFIFOList::probe()
{
	_SHAREDSPINLOCKER();
	if (m_list.size() > 0)
		return m_list.front();
	else
		return m_last;
}

qint64 VipFIFOList::time() const
{
	_SHAREDSPINLOCKER();
	if (!m_list.size() && !m_last.isValid())
		return VipInvalidTime;
	else if (m_list.size() > 0)
		return m_list.front().time();
	else
		return m_last.time();
}

bool VipFIFOList::empty() const
{
	_SHAREDSPINLOCKER();
	return !m_list.size() && !m_last.isValid();
}

int VipFIFOList::remaining() const
{
	_SHAREDSPINLOCKER();
	return (int)m_list.size();
}

bool VipFIFOList::hasNewData() const
{
	_SHAREDSPINLOCKER();
	return m_list.size() > 0;
}

int VipFIFOList::status() const
{
	_SHAREDSPINLOCKER();
	return m_list.size() > 0 ? (int)m_list.size() : m_last.isValid() ? 0 : -1;
}

qint64 VipFIFOList::memoryFootprint() const
{
	_SHAREDSPINLOCKER();
	qint64 size = 0;
	for (size_t i = 0; i < (size_t)m_list.size(); ++i)
		size += m_list[i].memoryFootprint();
	return size;
}

void VipFIFOList::clear()
{
	_SPINLOCKER();
	m_list.clear();
}

VipLIFOList::VipLIFOList()
  : VipDataList()
{
}

int VipLIFOList::push(const VipAnyData& data, int* previous)
{
	_SPINLOCKER();

	if (previous)
		*previous = (int)m_list.size();

	m_list.push_back(data);

	if (listLimitType() & Number) {
		while (static_cast<int>(m_list.size()) > this->maxListSize())
			m_list.pop_back();
	}
	if (listLimitType() & MemorySize) {
		qsizetype i = 0;
		qint64 size = 0;
		for (i = 0; i < (qsizetype)m_list.size(); ++i) {
			size += m_list[i].memoryFootprint();
			if (size >= maxListMemory())
				break;
		}
		if (i < static_cast<qsizetype>(m_list.size()))
			m_list.erase(m_list.begin() + i + 1, m_list.end());
		// m_list = m_list.mid(0, i + 1);
	}
	return static_cast<int>(m_list.size());
}

int VipLIFOList::push(VipAnyData&& data, int* previous)
{
	_SPINLOCKER();

	if (previous)
		*previous = (int)m_list.size();

	m_list.push_back(std::move(data));

	if (listLimitType() & Number) {
		while (static_cast<int>(m_list.size()) > this->maxListSize())
			m_list.pop_back();
	}
	if (listLimitType() & MemorySize) {
		qsizetype i = 0;
		qint64 size = 0;
		for (i = 0; i < (qsizetype)m_list.size(); ++i) {
			size += m_list[i].memoryFootprint();
			if (size >= maxListMemory())
				break;
		}
		if (i < static_cast<qsizetype>(m_list.size()))
			m_list.erase(m_list.begin() + i + 1, m_list.end());
		// m_list = m_list.mid(0, i + 1);
	}
	return static_cast<int>(m_list.size());
}

void VipLIFOList::reset(const VipAnyData& data)
{
	_SPINLOCKER();
	if (m_list.size() == 1) {
		m_list.back() = data;
	}
	else {
		m_list.clear();
		m_list.push_back(data);
	}
}
void VipLIFOList::reset(VipAnyData&& data)
{
	_SPINLOCKER();
	if (m_list.size() == 1) {
		m_list.back() = std::move(data);
	}
	else {
		m_list.clear();
		m_list.push_back(std::move(data));
	}
}

bool VipLIFOList::empty() const
{
	_SHAREDSPINLOCKER();
	return !m_list.size() && !m_last.isValid();
}

bool VipLIFOList::hasNewData() const
{
	_SHAREDSPINLOCKER();
	return m_list.size() > 0;
}

int VipLIFOList::status() const
{
	_SHAREDSPINLOCKER();
	return m_list.size() > 0 ? (int)m_list.size() : m_last.isValid() ? 0 : -1;
}

VipAnyData VipLIFOList::next()
{
	_SHAREDSPINLOCKER();
	if (m_list.size() > 0) {
		m_last = m_list.back();
		m_list.pop_back();
	}

	return m_last;
}

bool VipLIFOList::readAll(VipAnyDataList& lst)
{
	{
		_SPINLOCKER();
		if (m_list.size() > 0) {
			lst.resize(m_list.size());
			std::move(m_list.begin(), m_list.end(), lst.begin());
			std::reverse(lst.begin(), lst.begin() + m_list.size());
			m_list.clear();
			return true;
		}
	}
	return false;
}

VipAnyData VipLIFOList::probe()
{
	_SHAREDSPINLOCKER();
	if (m_list.size() > 0)
		return m_list.back();
	else
		return m_last;
}

qint64 VipLIFOList::time() const
{
	_SHAREDSPINLOCKER();
	if (!m_list.size() && !m_last.isValid())
		return VipInvalidTime;
	else if (m_list.size() > 0)
		return m_list.back().time();
	else
		return m_last.time();
}

qint64 VipLIFOList::memoryFootprint() const
{
	_SHAREDSPINLOCKER();
	qint64 size = 0;
	for (int i = 0; i < static_cast<int>(m_list.size()); ++i)
		size += m_list[i].memoryFootprint();
	return size;
}
void VipLIFOList::clear()
{
	_SPINLOCKER();
	m_list.clear();
}

VipLastAvailableList::VipLastAvailableList()
  : VipDataList()
  , m_has_new_data(false)
{
}

int VipLastAvailableList::push(const VipAnyData& data, int* previous)
{
	_SPINLOCKER();
	if (previous)
		*previous = m_has_new_data;
	d_data = data;
	m_has_new_data = (true);
	return 1;
}
int VipLastAvailableList::push(VipAnyData&& data, int* previous)
{
	_SPINLOCKER();
	if (previous)
		*previous = m_has_new_data;
	d_data = std::move(data);
	m_has_new_data = (true);
	return 1;
}

void VipLastAvailableList::reset(const VipAnyData& data)
{
	_SPINLOCKER();
	d_data = data;
	m_has_new_data = (true);
}
void VipLastAvailableList::reset(VipAnyData&& data)
{
	_SPINLOCKER();
	d_data = std::move(data);
	m_has_new_data = (true);
}

bool VipLastAvailableList::empty() const
{
	_SHAREDSPINLOCKER();
	return !m_has_new_data && !d_data.isValid();
}

bool VipLastAvailableList::hasNewData() const
{
	_SHAREDSPINLOCKER();
	return m_has_new_data;
}

int VipLastAvailableList::status() const
{
	_SHAREDSPINLOCKER();
	return (!m_has_new_data && !d_data.isValid()) ? -1 : m_has_new_data;
}

VipAnyData VipLastAvailableList::next()
{
	_SHAREDSPINLOCKER();
	m_has_new_data = (false);
	return d_data;
}

bool VipLastAvailableList::readAll(VipAnyDataList& lst)
{
	{
		_SPINLOCKER();
		if (m_has_new_data) {
			lst.resize(1);
			lst[0] = d_data;
			// The base documents this as reading and removing; next(), just above,
			// clears the flag and this did not, so hasNewData() stayed true and every
			// caller read the same datum for ever.
			m_has_new_data = (false);
			return true;
		}
	}
	return false;
}

VipAnyData VipLastAvailableList::probe()
{
	_SHAREDSPINLOCKER();
	return d_data;
}

qint64 VipLastAvailableList::time() const
{
	_SHAREDSPINLOCKER();
	if (!m_has_new_data && !d_data.isValid())
		return VipInvalidTime;
	else
		return d_data.time();
}

qint64 VipLastAvailableList::memoryFootprint() const
{
	_SHAREDSPINLOCKER();
	if (m_has_new_data)
		return d_data.memoryFootprint();
	else
		return 0;
}

void VipLastAvailableList::clear()
{
	_SHAREDSPINLOCKER();
	m_has_new_data = false;
}

class MyLock
{
	std::atomic<bool> d_lock{ false };
	bool d_has_mutex{ false };
	std::mutex d_mutex;

public:
	MyLock() noexcept = default;
	MyLock(MyLock const&) = delete;
	MyLock& operator=(MyLock const&) = delete;

	void lock() noexcept
	{
		if (!d_lock.exchange(true, std::memory_order_acquire))
			return;
		d_mutex.lock();
		d_has_mutex = true;
		d_lock.store(true, std::memory_order_relaxed);
	}

	bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed); }
	bool try_lock() noexcept
	{
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !d_lock.load(std::memory_order_relaxed) && !d_lock.exchange(true, std::memory_order_acquire);
	}

	void unlock() noexcept
	{
		if (d_has_mutex) {
			d_has_mutex = false;
			d_mutex.unlock();
		}
		d_lock.store(false, std::memory_order_release);
	}

	template<class Rep, class Period>
	bool try_lock_for(const std::chrono::duration<Rep, Period>& duration) noexcept
	{
		return try_lock_until(std::chrono::system_clock::now() + duration);
	}

	template<class Clock, class Duration>
	bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint) noexcept
	{
		unsigned count = 0;
		for (;;) {
			if (!d_lock.exchange(true, std::memory_order_acquire))
				return true;

			while (d_lock.load(std::memory_order_relaxed)) {
				if (std::chrono::system_clock::now() > timePoint)
					return false;
				std::this_thread::yield();

				if (++count > 64) {
					if (d_mutex.try_lock()) {
						d_has_mutex = true;
						d_lock.store(true, std::memory_order_relaxed);
						return true;
					}
				}
			}
		}
	}
};
struct SpinLocker
{
	using UniqueLock = std::unique_lock<MyLock>;
	MyLock d_lock;
	std::condition_variable_any d_cond;

	UniqueLock lock() { return UniqueLock{ d_lock }; }
	UniqueLock adopt_lock() { return UniqueLock{ d_lock, std::adopt_lock_t{} }; }
	bool try_lock_for(unsigned ms) { return d_lock.try_lock_for(std::chrono::milliseconds(ms)); }
	void notify_all() { d_cond.notify_all(); }
	void wait(UniqueLock&) { d_cond.wait(d_lock); }
	void wait_for(UniqueLock&, unsigned ms) { d_cond.wait_for(d_lock, std::chrono::milliseconds(ms)); }
};
struct StdMutexLocker
{
	using UniqueLock = std::unique_lock<std::mutex>;
	std::mutex d_lock;
	std::condition_variable d_cond;

	UniqueLock lock() { return UniqueLock{ d_lock }; }
	UniqueLock adopt_lock() { return UniqueLock{ d_lock, std::adopt_lock_t{} }; }
	bool try_lock_for(unsigned ms)
	{
		auto tp = std::chrono::system_clock::now() + std::chrono::milliseconds(ms);
		for (;;) {
			if (d_lock.try_lock())
				return true;
			if (std::chrono::system_clock::now() > tp)
				return false;
			std::this_thread::yield();
		}
	}
	void notify_all() { d_cond.notify_all(); }
	void wait(UniqueLock& ul) { d_cond.wait(ul); }
	void wait_for(UniqueLock& ul, unsigned ms) { d_cond.wait_for(ul, std::chrono::milliseconds(ms)); }
};
struct MutexLocker
{
	using UniqueLock = std::unique_lock<QMutex>;
	QMutex d_lock;
	QWaitCondition d_cond;

	UniqueLock lock() { return UniqueLock{ d_lock }; }
	UniqueLock adopt_lock() { return UniqueLock{ d_lock, std::adopt_lock_t{} }; }
	bool try_lock_for(unsigned ms) { return d_lock.try_lock_for(std::chrono::milliseconds(ms)); }

	void notify_all() { d_cond.notify_all(); }
	void wait(UniqueLock&) { d_cond.wait(&d_lock); }
	void wait_for(UniqueLock&, unsigned ms) { d_cond.wait(&d_lock, ms); }
};

/// @brief TaskPool is a thread that asynchronously executes VipProcessingObject::run().
///
/// When a VipProcessingObject is Asynchronous, it uses internally a TaskPool to schedule its processing.
/// QThreadPool is not used due to observed dead locks in certain situations.
///
class TaskPool
{
	struct Thread : QThread
	{
		TaskPool* parent;
		Thread(TaskPool* p)
		  : parent(p)
		{
		}

	protected:
		virtual void run() { parent->run(); }
	};

	using LockType = StdMutexLocker; // StdMutexLocker; // MutexLocker;
	using UniqueLock = typename LockType::UniqueLock;
	LockType lock;

	std::atomic<int> m_run;
	// How many scheduled tasks the pool has been asked to drop.
	std::atomic<int> m_clear{ 0 };
	VipProcessingObject* m_parent;
	Thread m_thread;
	// Atomic: written by the destructor, read in the loop of the pool thread. A
	// plain bool leaves the thread free never to observe the write, and the wait
	// below then never returns.
	std::atomic<bool> m_stop{ false };
	void atomWait(UniqueLock& ll, int milli);

protected:
	void run();

public:
	/// @brief Construct from a parent object and a thread priority
	TaskPool(VipProcessingObject* parent = nullptr, QThread::Priority p = QThread::InheritPriority);
	~TaskPool();

	/// @brief Schedule a VipProcessingObject launch
	VIP_ALWAYS_INLINE void push()
	{
		if (m_run.fetch_add(1) == 0)
			lock.notify_all();
	}

	void setPriority(QThread::Priority p) { m_thread.setPriority(p); }
	QThread::Priority priority() const { return m_thread.priority(); }

	/// @brief Wait until the task list is empty or until timeout
	bool waitForDone(int milli = -1);

	/// @brief Returns the number of remaining scheduled QRunnable, include the one being currently executed (if any)
	int remaining() const;

	/// @brief remove all pending tasks
	void clear();
	void clearNoLock() { clear(); }
};

void TaskPool::run()
{
	// notify that the thread started
	{
		auto ll = lock.lock();
		lock.notify_all();
	}

	auto ll = lock.lock();

	while (!m_stop) {

		// wait for incoming runnable objects.
		// takes at most 15 ms to stop if required.
		while (m_run.load(std::memory_order_relaxed) == 0 && !m_stop) {
			lock.wait_for(ll, 15);
			lock.notify_all();
		}

		int count = m_run.load(std::memory_order_relaxed);
		const int saved = count;
		// The tasks to drop are those that were scheduled when the request came
		// in, never what has been pushed since.
		count -= qMin(count, m_clear.exchange(0));
		if (!m_stop) {
			// Released for the whole execution. This mutex guards the counters and
			// the condition, nothing else; holding it here made every waitForDone()
			// wait for the processing to end instead of for the queue to empty.
			ll.unlock();
			while (!m_stop && count-- > 0) {
				// A request that arrives while the batch runs drops the rest of it.
				if (m_clear.exchange(0))
					break;
				try {
					// Avoid exiting task pool thread on unhandled exception.
					// The lock is taken per call rather than around the loop, so
					// that the signal below is emitted without it.
					{
						SPIN_LOCK(m_parent->runLock());
						m_parent->runNoLock();
					}
					if (VipProcessingObject* o = qobject_cast<VipProcessingObject*>(m_parent))
						o->emitProcessingDone();
				}
				catch (const std::exception& e) {
					if (VipProcessingObject* o = qobject_cast<VipProcessingObject*>(m_parent))
						o->setError("Unhandled exception: " + QString(e.what()));
					else
						qWarning() << "Unhandled exception: " << e.what() << "\n";
				}
				catch (...) {
					if (VipProcessingObject* o = qobject_cast<VipProcessingObject*>(m_parent))
						o->setError("Unhandled unknown exception");
					else
						qWarning() << "Unhandled unknown exception\n";
				}
			}
			ll.lock();
		}
		// Only the batch counted at the start is consumed, whether it ran to the
		// end or was dropped. Resetting the counter to zero also threw away
		// everything another thread had pushed while the batch ran.
		m_run.fetch_sub(saved);

		lock.notify_all();
	}

	lock.notify_all();
}

TaskPool::TaskPool(VipProcessingObject* parent, QThread::Priority p)
  : m_run(0)
  , m_parent(parent)
  , m_thread(this)
  , m_stop(false)
{
	auto ll = lock.lock();
	m_thread.start(p);
	lock.wait(ll);
}

TaskPool::~TaskPool()
{
	{
		// Under the lock, so that a thread about to wait cannot miss it.
		auto ll = lock.lock();
		m_stop = true;
	}
	lock.notify_all();

	// The task in flight has no cancellation point, so this can only wait for it.
	// Destroying the thread while it still runs would abort, so the bound below is
	// a diagnostic, not a way out.
	if (!m_thread.wait(30000)) {
		qWarning() << "Processing thread still running after 30s, still waiting";
		m_thread.wait();
	}
}

void TaskPool::atomWait(UniqueLock& ll, int milli)
{
	// Wait one time at most milli ms.
	lock.wait_for(ll, milli);
}

bool TaskPool::waitForDone(int milli_time)
{
	// A sleeping wait on the condition, not a poll. The loop used to spin in
	// try_lock_for on a mutex the pool thread never released while a task ran, so
	// the caller burnt a core, often the one of the interface. The stop flag is
	// the abandon condition the unbounded branch was missing.
	auto ll = lock.lock();

	if (milli_time < 0) {
		while (remaining() > 0 && !m_stop)
			atomWait(ll, 15);
		return remaining() == 0;
	}

	const qint64 start = vipGetMilliSecondsSinceEpoch();
	while (remaining() > 0 && !m_stop) {
		const qint64 left = milli_time - (vipGetMilliSecondsSinceEpoch() - start);
		if (left <= 0)
			return false;
		atomWait(ll, static_cast<int>(qMin<qint64>(left, 15)));
	}
	return remaining() == 0;
}

int TaskPool::remaining() const
{
	return m_run.load(std::memory_order_relaxed);
}

void TaskPool::clear()
{
	// What is scheduled right now, and only that. A flag was armed instead, which
	// dropped whatever batch it happened to land on: a task pushed between the
	// request and the batch was lost with it.
	m_clear.store(m_run.load(std::memory_order_relaxed));
}

class VipProcessingObject::PrivateData
{
public:
	struct Parameters
	{
		VipProcessingObject::ScheduleStrategies schedule_strategies;
		// Atomic, rather than plain bools reinterpreted as atomics at each access:
		// reading a bool through an lvalue of another type is undefined, and the
		// copies below went through the plain type anyway, which cancelled what the
		// reinterpretation was for. The copies transfer them by load and store.
		std::atomic<bool> visible;
		std::atomic<bool> enable;
		bool deleteOnOutputConnectionsClosed;
		int errorBufferMaxSize;
		// attributes
		QVariantMap attributes;
		Parameters(VipProcessingObject::ScheduleStrategies schedule_strategies = OneInput | NoThread,
			   bool visible = true,
			   bool enable = true,
			   bool deleteOnOutputConnectionsClosed = false,
			   int errorBufferMaxSize = 3,
			   const QVariantMap& attrs = QVariantMap())
		  : schedule_strategies(schedule_strategies)
		  , visible(visible)
		  , enable(enable)
		  , deleteOnOutputConnectionsClosed(deleteOnOutputConnectionsClosed)
		  , errorBufferMaxSize(errorBufferMaxSize)
		  , attributes(attrs)
		{
		}
		Parameters(const Parameters& other)
		  : schedule_strategies(other.schedule_strategies)
		  , visible(other.visible.load())
		  , enable(other.enable.load())
		  , deleteOnOutputConnectionsClosed(other.deleteOnOutputConnectionsClosed)
		  , errorBufferMaxSize(other.errorBufferMaxSize)
		  , attributes(other.attributes)
		{
		}
		Parameters& operator=(const Parameters& other)
		{
			if (this != &other) {
				schedule_strategies = other.schedule_strategies;
				visible.store(other.visible.load());
				enable.store(other.enable.load());
				deleteOnOutputConnectionsClosed = other.deleteOnOutputConnectionsClosed;
				errorBufferMaxSize = other.errorBufferMaxSize;
				attributes = other.attributes;
			}
			return *this;
		}
	};

	PrivateData()
	  : pool(nullptr)
	  , processingTime(0)
	  , lastProcessingDate(0)
	  , emit_destroy(false)
	  , inImageTransformChanged(false)
	  , computeTimeStatistics(true)
	  , thread_priority(0)
	  , destruct(false)
	  , initializeIO(0)
	  , dirtyIO(true)
	  , parentList(nullptr)
	  , processingRate(0)
	  , processingCount(0)
	  , lastTime(VipInvalidTime)
	{
		logErrors = VipProcessingManager::logErrors();
	}

	// A blocking mutex, not a spinlock: the section it guards runs the whole
	// processing in the calling thread, so a second caller used to burn a core
	// for as long as the processing lasted.
	QMutex update_mutex;
	// Counts the calls to update() in flight. The lock above is released before
	// the wait for the result, this is not: it is what isUpdating() answers.
	std::atomic<int> updating{ 0 };
	VipSpinlock run_mutex;
	VipSpinlock error_mutex;
	VipSpinlock init_lock;
	std::atomic<TaskPool*> pool;
	qint64 processingTime;
	qint64 lastProcessingDate;
	bool emit_destroy;
	bool inImageTransformChanged;
	bool computeTimeStatistics;
	// error management
	QList<VipErrorData> errors;

	Parameters parameters;
	QList<Parameters> savedParameters;
	int thread_priority;
	bool destruct;
	// Set by runNoLock, consumed by emitProcessingDone: the signal must not go out
	// while the lock that serialises the run is held.
	bool processingDonePending = false;

	// inputs, outputs and properties
	// Atomic: the fast path of initialize() reads them without the lock that the
	// slow path writes them under, which is a race and a decision taken on a value
	// being changed.
	std::atomic<int> initializeIO;
	std::vector<std::unique_ptr<VipProcessingIO>> inputs;
	std::vector<std::unique_ptr<VipProcessingIO>> outputs;
	std::vector<std::unique_ptr<VipProcessingIO>> properties;
	std::function<void()> onInitIO;

	// flatten representations
	std::atomic<bool> dirtyIO;
	// Here, use std::vector to avoid going through the shared counter
	std::vector<VipInput*> flatInputs;
	std::vector<VipOutput*> flatOutputs;
	std::vector<VipProperty*> flatProperties;

	// parent VipProcessingList if any
	VipProcessingList* parentList;

	Info info;

	// frame rate
	double processingRate;
	int processingCount;
	qint64 lastTime;

	QSet<int> logErrors;

	std::atomic<bool> fromArchive{ false };

	TaskPool* createPoolInternal(VipProcessingObject* _this)
	{
		TaskPool* p = nullptr;
		QThread::Priority prio = thread_priority == 0 ? (QThread::Priority)VipProcessingManager::instance().defaultPriority(_this->metaObject()) : (QThread::Priority)thread_priority;
		TaskPool* _new = new TaskPool(_this, prio);

		if (!pool.compare_exchange_strong(p, _new)) {
			delete _new;
			return p;
		}
		return _new;
	}

	VIP_ALWAYS_INLINE TaskPool* createPool(VipProcessingObject* _this)
	{
		TaskPool* p = pool.load(std::memory_order_relaxed);
		if (VIP_UNLIKELY(!p))
			return createPoolInternal(_this);
		return p;
	}
	VIP_ALWAYS_INLINE TaskPool* getPool() noexcept { return pool.load(std::memory_order_relaxed); }
	VIP_ALWAYS_INLINE const TaskPool* getPool() const noexcept { return pool.load(std::memory_order_relaxed); }
};

VipProcessingObject* VipProcessingObject::Info::create() const
{
	if (VipProcessingObject* obj = vipCreateVariant(metatype).value<VipProcessingObject*>()) {
		// pass the init object to the initialize member.
		// This is only used if a same processing class can perform different actions (and be seen as different processings) depending on
		// the init parameter.
		// For instance multiple PyProcessing objects can have different python code and be seen as multiple processings represented by different Info structure.
		obj->initializeProcessing(init);
		obj->d_data->info = *this;
		return obj;
	}
	return nullptr;
}

VipProcessingObject::VipProcessingObject(QObject* parent)
  : VipErrorHandler()
{
	VIP_CREATE_PRIVATE_DATA();
		
	this->setParent(parent);

	VipProcessingManager::instance().add(this);
	VipUniqueId::id<VipProcessingObject>(this);
}

VipProcessingObject::~VipProcessingObject()
{
	d_data->destruct = true;
	emitDestroyed();

	VipProcessingManager::instance().remove(this);

	// block the arrival of new data
	for (size_t i = 0; i < d_data->flatInputs.size(); ++i)
		d_data->flatInputs[i]->setEnabled(false);

	// wait for all remaining processing and delete the task pool
	if (TaskPool* p = d_data->getPool()) {
		p->clear();
		p->waitForDone();
		/* if (p->thread() == this->thread())
			delete p;
		else
			p->deleteLater();*/
		delete p;
	}

	d_data->inputs.clear();
}

void VipProcessingObject::save()
{
	d_data->savedParameters.append(PrivateData::Parameters(scheduleStrategies(), isProcessingVisible(), isEnabled(), deleteOnOutputConnectionsClosed(), errorBufferMaxSize(), attributes()));
}

void VipProcessingObject::restore()
{
	if (d_data->savedParameters.size()) {
		d_data->parameters = d_data->savedParameters.back();
		d_data->savedParameters.pop_back();

		// reset the schedule strategies to create the task pool if necessary
		ScheduleStrategies st = d_data->parameters.schedule_strategies;
		d_data->parameters.schedule_strategies = ScheduleStrategies();
		blockSignals(true);
		setScheduleStrategies(st);
		blockSignals(false);
	}

	// clear all input buffers
	for (int i = 0; i < inputCount(); ++i) {
		inputAt(i)->buffer()->clear();
	}

	emitProcessingChanged();
}

void VipProcessingObject::dirtyProcessingIO(VipProcessingIO* io)
{
	d_data->dirtyIO = true;
	Q_EMIT IOChanged(io);

	// Only an input or a property can change the set of sources; an output cannot.
	// The argument names the I/O that changed and was used for the signal alone, so
	// a full descent of the upstream graph ran for every output added too, and
	// resizing a multi-output runs one per element.
	if (io && (io->type() == VipProcessingIO::TypeOutput || io->type() == VipProcessingIO::TypeMultiOutput))
		return;

	// send the source properties to the sources
	QList<QByteArray> names = sourceProperties();
	for (int i = 0; i < names.size(); ++i)
		this->setSourceProperty(names[i].data(), this->property(names[i].data()));
}

namespace
{
	// Set of nodes already reached by the current descent, held for the outermost
	// call only. Without it, two processings that are sources of each other
	// recursed until the stack ran out. One set per kind of descent, so that a
	// descent started inside another does not read the marks of the first.
	template<int Kind>
	struct VisitedSet
	{
		static QSet<const VipProcessingObject*>*& current()
		{
			static thread_local QSet<const VipProcessingObject*>* set = nullptr;
			return set;
		}
		QSet<const VipProcessingObject*> own;
		const bool outermost;
		VisitedSet()
		  : outermost(current() == nullptr)
		{
			if (outermost)
				current() = &own;
		}
		~VisitedSet()
		{
			if (outermost)
				current() = nullptr;
		}
	};

	using VisitedSources = VisitedSet<0>;
	using VisitedUpdates = VisitedSet<1>;
}

void VipProcessingObject::setSourceProperty(const char* name, const QVariant& value)
{
	VisitedSources visited;
	if (VisitedSources::current()->contains(this))
		return;
	VisitedSources::current()->insert(this);

	this->setProperty(name, value);
	this->setProperty((QByteArray("__source_") + name).data(), value);
	QList<VipProcessingObject*> sources = this->directSources();
	for (int i = 0; i < sources.size(); ++i)
		if (sources[i] != d_data->parentList)
			sources[i]->setSourceProperty(name, value);

	emitProcessingChanged();
}

QList<QByteArray> VipProcessingObject::sourceProperties() const
{
	QList<QByteArray> res;
	QList<QByteArray> names = this->dynamicPropertyNames();
	for (int i = 0; i < names.size(); ++i)
		if (names[i].startsWith("__source_"))
			res << names[i].mid(9);

	return res;
}

template<class TYPE>
static QStringList names(const std::vector<TYPE*>& io)
{
	QStringList res;
	for (size_t i = 0; i < io.size(); ++i)
		res << io[i]->name();
	return res;
}

template<class TYPE>
static TYPE* name(const std::vector<TYPE*>& io, const QString& name)
{
	for (size_t i = 0; i < io.size(); ++i)
		if (name == io[i]->name())
			return io[i];
	return nullptr;
}

template<class TYPE>
static std::vector<TYPE*> flatten(const std::vector<std::unique_ptr<VipProcessingIO>>& io)
{
	std::vector<TYPE*> flat;
	for (size_t i = 0; i < io.size(); ++i) {
		if (VipMultipleProcessingIO<TYPE>* multi = io[i]->to<VipMultipleProcessingIO<TYPE>>()) {
			for (int c = 0; c < multi->count(); ++c)
				flat.push_back(multi->at(c));
		}
		else
			flat.push_back(io[i]->to<TYPE>());
	}
	return flat;
}

void VipProcessingObject::internalInitIO(bool force) const
{
	// The callback is run once the lock is released. Building a multi input under
	// it marks the object dirty, which emits and walks the sources, which comes
	// back here on the same thread: this lock does not nest, and that froze.
	std::function<void()> callback;
	{
		VipUniqueLock<VipSpinlock> locker(const_cast<VipSpinlock&>(d_data->init_lock));
		if (force || !d_data->initializeIO || d_data->dirtyIO || d_data->initializeIO != metaObject()->propertyCount()) {

			const QMetaObject* meta = metaObject();
			VipProcessingObject* _this = const_cast<VipProcessingObject*>(this);

			for (; d_data->initializeIO < meta->propertyCount(); ++d_data->initializeIO) {
				const int i = d_data->initializeIO;
				const int type = meta->property(i).userType();

				if (type == qMetaTypeId<VipInput>())
					_this->d_data->inputs.push_back(std::unique_ptr<VipProcessingIO>(new VipInput(meta->property(i).name(), _this)));
				else if (type == qMetaTypeId<VipMultiInput>())
					_this->d_data->inputs.push_back(std::unique_ptr<VipProcessingIO>(new VipMultiInput(meta->property(i).name(), _this)));
				else if (type == qMetaTypeId<VipProperty>())
					_this->d_data->properties.push_back(std::unique_ptr<VipProcessingIO>(new VipProperty(meta->property(i).name(), _this)));
				else if (type == qMetaTypeId<VipMultiProperty>())
					_this->d_data->properties.push_back(std::unique_ptr<VipProcessingIO>(new VipMultiProperty(meta->property(i).name(), _this)));
				else if (type == qMetaTypeId<VipOutput>())
					_this->d_data->outputs.push_back(std::unique_ptr<VipProcessingIO>(new VipOutput(meta->property(i).name(), _this)));
				else if (type == qMetaTypeId<VipMultiOutput>())
					_this->d_data->outputs.push_back(std::unique_ptr<VipProcessingIO>(new VipMultiOutput(meta->property(i).name(), _this)));
			}

			_this->d_data->flatInputs = flatten<VipInput>(d_data->inputs);
			_this->d_data->flatOutputs = flatten<VipOutput>(d_data->outputs);
			_this->d_data->flatProperties = flatten<VipProperty>(d_data->properties);

			for (size_t i = 0; i < d_data->flatInputs.size(); ++i)
				_this->d_data->flatInputs[i]->setParentProcessing(_this);

			for (size_t i = 0; i < d_data->flatOutputs.size(); ++i)
				_this->d_data->flatOutputs[i]->setParentProcessing(_this);

			for (size_t i = 0; i < d_data->flatProperties.size(); ++i)
				_this->d_data->flatProperties[i]->setParentProcessing(_this);

			_this->d_data->dirtyIO = false;

			callback = _this->d_data->onInitIO;
		}
	}

	if (callback)
		callback();
}

void VipProcessingObject::setIOInitializeFunction(const std::function<void()>& f)
{
	d_data->onInitIO = f;
}
const std::function<void()>& VipProcessingObject::IOInitializeFunction() const
{
	return d_data->onInitIO;
}

void VipProcessingObject::initialize(bool force) const
{
	if (force || !d_data->initializeIO || d_data->dirtyIO || d_data->initializeIO != metaObject()->propertyCount())
		internalInitIO(force);
}

VipProcessingPool* VipProcessingObject::parentObjectPool() const
{
	return qobject_cast<VipProcessingPool*>(parent());
}

QList<VipProcessingObject*> VipProcessingObject::directSources() const
{
	initialize();
	QList<VipProcessingObject*> res;
	for (size_t s = 0; s < d_data->flatInputs.size(); ++s)
		if (VipOutput* out = d_data->flatInputs[s]->source())
			if (VipProcessingObject* obj = out->parentProcessing())
				if (res.indexOf(obj) < 0 && obj != this)
					res.append(obj);
	return res;
}

QList<VipProcessingObject*> VipProcessingObject::allSources() const
{
	QList<VipProcessingObject*> res = directSources();
	QList<VipProcessingObject*> to_inspect = res;

	while (to_inspect.size()) {
		const QList<VipProcessingObject*> tmp = to_inspect;
		to_inspect.clear();
		for (QList<VipProcessingObject*>::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
			const QList<VipProcessingObject*> src = (*it)->directSources();
			for (QList<VipProcessingObject*>::const_iterator itsrc = src.begin(); itsrc != src.end(); ++itsrc)
				if (*itsrc != this && res.indexOf(*itsrc) < 0) {
					to_inspect.append(*itsrc);
					res.append(*itsrc);
				}
		}
	}
	return res;
}

QList<VipProcessingObject*> VipProcessingObject::directSinks() const
{
	initialize();
	QList<VipProcessingObject*> res;
	for (size_t s = 0; s < d_data->flatOutputs.size(); ++s)
		if (VipOutput* out = d_data->flatOutputs[s]) {
			const QList<VipInput*> ins = out->connection()->sinks();
			for (QList<VipInput*>::const_iterator it = ins.begin(); it != ins.end(); ++it)
				if (VipProcessingObject* obj = (*it)->parentProcessing())
					if (res.indexOf(obj) < 0 && obj != this)
						res.append(obj);
		}
	return res;
}

QList<VipProcessingObject*> VipProcessingObject::allSinks() const
{
	QList<VipProcessingObject*> res = directSinks();
	QList<VipProcessingObject*> to_inspect = res;

	while (to_inspect.size()) {
		const QList<VipProcessingObject*> tmp = to_inspect;
		to_inspect.clear();
		for (QList<VipProcessingObject*>::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
			const QList<VipProcessingObject*> src = (*it)->directSinks();
			for (QList<VipProcessingObject*>::const_iterator itsrc = src.begin(); itsrc != src.end(); ++itsrc)
				if (*itsrc != this && res.indexOf(*itsrc) < 0) {
					to_inspect.append(*itsrc);
					res.append(*itsrc);
				}
		}
	}
	return res;
}

QList<VipProcessingObject*> VipProcessingObject::fullPipeline() const
{
	QList<VipProcessingObject*> sources = this->allSources();
	const QList<VipProcessingObject*> sinks = this->allSinks();
	std::reverse(sources.begin(), sources.end());

	QList<VipProcessingObject*> res = sources;
	res.append(const_cast<VipProcessingObject*>(this));
	res += sinks;
	return res;
}

qint64 VipProcessingObject::time() const
{
	if (VipProcessingPool* pool = parentObjectPool()) {
		qint64 t = pool->time();
		if (t != VipInvalidTime)
			return t;
	}

	return vipGetNanoSecondsSinceEpoch();
}

void VipProcessingObject::setComputeTimeStatistics(bool enable)
{
	if (d_data->computeTimeStatistics != enable) {
		d_data->computeTimeStatistics = enable;
		d_data->lastTime = VipInvalidTime;
	}
}
bool VipProcessingObject::computeTimeStatistics() const
{
	return d_data->computeTimeStatistics;
}

qint64 VipProcessingObject::processingTime() const
{
	return d_data->processingTime;
}

QString VipProcessingObject::className() const
{
	return metaObject()->className();
}

QString VipProcessingObject::description() const
{
	return info().description;
}

QString VipProcessingObject::category() const
{
	return info().category;
}

QIcon VipProcessingObject::icon() const
{
	return info().icon;
}

VipProcessingObject::Info VipProcessingObject::info() const
{
	if (d_data->info.metatype != 0)
		return d_data->info;

	Info res;
	res.metatype = vipMetaTypeFromQObject(this); 
	res.classname = className();
	res.displayHint = this->displayHint();

	for (int i = 0; i < this->metaObject()->classInfoCount(); ++i) {
		if (this->metaObject()->classInfo(i).name() == QByteArray("icon"))
			res.icon = QIcon(QString(this->metaObject()->classInfo(i).value()));
		else if (this->metaObject()->classInfo(i).name() == QByteArray("category"))
			res.category = this->metaObject()->classInfo(i).value();
		else if (this->metaObject()->classInfo(i).name() == QByteArray("description"))
			res.description = this->metaObject()->classInfo(i).value();
	}
	return const_cast<Info&>(d_data->info) = res;
}

QStringList VipProcessingObject::inputNames() const
{
	initialize();
	return names<VipInput>(d_data->flatInputs);
}

QStringList VipProcessingObject::outputNames() const
{
	initialize();
	return names<VipOutput>(d_data->flatOutputs);
}

QStringList VipProcessingObject::propertyNames() const
{
	initialize();
	return names<VipProperty>(d_data->flatProperties);
}

int VipProcessingObject::inputCount() const
{
	initialize();
	return static_cast<int>(d_data->flatInputs.size());
}

int VipProcessingObject::outputCount() const
{
	initialize();
	return static_cast<int>(d_data->flatOutputs.size());
}

int VipProcessingObject::propertyCount() const
{
	initialize();
	return static_cast<int>(d_data->flatProperties.size());
}

int VipProcessingObject::topLevelInputCount() const
{
	initialize();
	return static_cast<int>(d_data->inputs.size());
}

int VipProcessingObject::topLevelOutputCount() const
{
	initialize();
	return static_cast<int>(d_data->outputs.size());
}

int VipProcessingObject::topLevelPropertyCount() const
{
	initialize();
	return static_cast<int>(d_data->properties.size());
}

VipProcessingIO* VipProcessingObject::topLevelInputAt(int i) const
{
	initialize();
	// Out of range gives nullptr, the contract the accessors by name already hold.
	// The index came straight from the caller and a negative one, once cast to
	// size_t, addresses far past the end.
	if (i < 0 || static_cast<size_t>(i) >= d_data->inputs.size())
		return nullptr;
	return d_data->inputs[static_cast<size_t>(i)].get();
}

VipProcessingIO* VipProcessingObject::topLevelOutputAt(int i) const
{
	initialize();
	if (i < 0 || static_cast<size_t>(i) >= d_data->outputs.size())
		return nullptr;
	return d_data->outputs[static_cast<size_t>(i)].get();
}

VipProcessingIO* VipProcessingObject::topLevelPropertyAt(int i) const
{
	initialize();
	if (i < 0 || static_cast<size_t>(i) >= d_data->properties.size())
		return nullptr;
	return d_data->properties[static_cast<size_t>(i)].get();
}

VipProcessingIO* VipProcessingObject::topLevelInputName(const QString& name) const
{
	initialize();
	for (size_t i = 0; i < d_data->inputs.size(); ++i)
		if (d_data->inputs[i]->name() == name)
			return d_data->inputs[i].get();
	return nullptr;
}

VipProcessingIO* VipProcessingObject::topLevelOutputName(const QString& name) const
{
	initialize();
	for (size_t i = 0; i < d_data->outputs.size(); ++i)
		if (d_data->outputs[i]->name() == name)
			return d_data->outputs[i].get();
	return nullptr;
}

VipProcessingIO* VipProcessingObject::topLevelPropertyName(const QString& name) const
{
	initialize();
	for (size_t i = 0; i < d_data->properties.size(); ++i)
		if (d_data->properties[i]->name() == name)
			return d_data->properties[i].get();
	return nullptr;
}

VipInput* VipProcessingObject::inputName(const QString& input) const
{
	initialize();
	return name<VipInput>(d_data->flatInputs, input);
}

VipOutput* VipProcessingObject::outputName(const QString& output) const
{
	initialize();
	return name<VipOutput>(d_data->flatOutputs, output);
}

VipProperty* VipProcessingObject::propertyName(const QString& property) const
{
	initialize();
	return name<VipProperty>(d_data->flatProperties, property);
}

VipInput* VipProcessingObject::inputAt(int index) const
{
	initialize();
	if (index < 0 || static_cast<size_t>(index) >= d_data->flatInputs.size())
		return nullptr;
	return d_data->flatInputs[static_cast<size_t>(index)];
}

VipOutput* VipProcessingObject::outputAt(int index) const
{
	initialize();
	if (index < 0 || static_cast<size_t>(index) >= d_data->flatOutputs.size())
		return nullptr;
	return d_data->flatOutputs[static_cast<size_t>(index)];
}

VipProperty* VipProcessingObject::propertyAt(int index) const
{
	initialize();
	if (index < 0 || static_cast<size_t>(index) >= d_data->flatProperties.size())
		return nullptr;
	return d_data->flatProperties[static_cast<size_t>(index)];
}

QString VipProcessingObject::propertyEditor(const QString& property) const
{
	QString full_name = "edit_" + property;

	const QMetaObject* meta = this->metaObject();
	for (int i = 0; i < meta->classInfoCount(); ++i) {
		if (meta->classInfo(i).name() == full_name)
			return QString(meta->classInfo(i).value());
	}

	return QString();
}

QString VipProcessingObject::propertyCategory(const QString& property) const
{
	QString full_name = "category_" + property;

	const QMetaObject* meta = this->metaObject();
	for (int i = 0; i < meta->classInfoCount(); ++i) {
		if (meta->classInfo(i).name() == full_name)
			return QString(meta->classInfo(i).value());
	}

	return QString();
}

int VipProcessingObject::indexOf(const VipInput* p) const
{
	initialize();
	for (size_t i = 0; i < d_data->flatInputs.size(); ++i)
		if (d_data->flatInputs[i] == p)
			return static_cast<int>(i);
	return -1;
}
int VipProcessingObject::indexOf(const VipOutput* p) const
{
	initialize();
	for (size_t i = 0; i < d_data->flatOutputs.size(); ++i)
		if (d_data->flatOutputs[i] == p)
			return static_cast<int>(i);
	return -1;
}
int VipProcessingObject::indexOf(const VipProperty* p) const
{
	initialize();
	for (size_t i = 0; i < d_data->flatProperties.size(); ++i)
		if (d_data->flatProperties[i] == p)
			return static_cast<int>(i);
	return -1;
}

QString VipProcessingObject::description(const QString& name) const
{
	const QMetaObject* meta = this->metaObject();
	for (int i = 0; i < meta->classInfoCount(); ++i) {
		if (meta->classInfo(i).name() == name)
			return QString(meta->classInfo(i).value());
	}

	return QString();
}

QString VipProcessingObject::inputDescription(const QString& input) const
{
	return description(input);
}

QString VipProcessingObject::outputDescription(const QString& output) const
{
	return description(output);
}

QString VipProcessingObject::propertyDescription(const QString& property) const
{
	return description(property);
}

QString VipProcessingObject::generateUniqueOutputName(const VipProcessingIO& io, const QString& name)
{

	QStringList found;
	for (size_t i = 0; i < d_data->outputs.size(); ++i) {
		if (VipOutput* out = d_data->outputs[i]->toOutput()) {
			if (*out != io && out->name().startsWith(name))
				found << out->name();
		}
		else if (VipMultiOutput* mout = d_data->outputs[i]->toMultiOutput()) {
			for (int o = 0; o < mout->count(); ++o)
				if (*mout->at(o) != io && mout->at(o)->name().startsWith(name))
					found << mout->at(o)->name();
		}
	}

	QString res = name;
	int count = 1;
	while (found.indexOf(res) >= 0) {
		res = name + "_" + QString::number(count);
		++count;
	}

	return res;
}

QString VipProcessingObject::generateUniqueInputName(const VipProcessingIO& io, const QString& name)
{
	QStringList found;
	for (size_t i = 0; i < d_data->inputs.size(); ++i) {
		if (VipInput* in = d_data->inputs[i]->toInput()) {
			if (*in != io && in->name().startsWith(name))
				found << in->name();
		}
		else if (VipMultiInput* min = d_data->inputs[i]->toMultiInput()) {
			for (int o = 0; o < min->count(); ++o)
				if (*min->at(o) != io && min->at(o)->name() == name)
					found << min->at(o)->name();
		}
	}

	QString res = name;
	int count = 1;
	while (found.indexOf(res) >= 0) {
		res = name + "_" + QString::number(count);
		++count;
	}

	return res;
}

void VipProcessingObject::setPriority(QThread::Priority p)
{
	if (p == QThread::IdlePriority)
		// IdlePriority not valid
		return;

	d_data->thread_priority = (int)p;
	if (TaskPool* pool = d_data->getPool()) {
		if (p == QThread::InheritPriority)
			p = this->thread()->priority();
		if (p != QThread::InheritPriority)
			pool->setPriority(p);
	}
}

QThread::Priority VipProcessingObject::priority() const
{
	if (TaskPool* p = d_data->getPool())
		return p->priority();
	return d_data->thread_priority == 0 ? QThread::InheritPriority : (QThread::Priority)d_data->thread_priority;
}

void VipProcessingObject::setAttributes(const QVariantMap& attrs)
{
	d_data->parameters.attributes = attrs;
	emitProcessingChanged();
}

void VipProcessingObject::setAttribute(const QString& name, const QVariant& value)
{
	d_data->parameters.attributes[name] = value;
	emitProcessingChanged();
}

bool VipProcessingObject::removeAttribute(const QString& name)
{
	QVariantMap::iterator it = d_data->parameters.attributes.find(name);
	if (it != d_data->parameters.attributes.end()) {
		d_data->parameters.attributes.erase(it);
		emitProcessingChanged();
		return true;
	}
	return false;
}

const QVariantMap& VipProcessingObject::attributes() const
{
	return d_data->parameters.attributes;
}

QVariant VipProcessingObject::attribute(const QString& attr) const
{
	return d_data->parameters.attributes[attr];
}

bool VipProcessingObject::hasAttribute(const QString& name) const
{
	return d_data->parameters.attributes.find(name) != d_data->parameters.attributes.end();
}

QStringList VipProcessingObject::mergeAttributes(const QVariantMap& attrs)
{
	QStringList res;
	for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		QVariantMap::const_iterator found = d_data->parameters.attributes.find(it.key());
		if (found == d_data->parameters.attributes.end() || it.value() != found.value()) {
			d_data->parameters.attributes[it.key()] = it.value();
			res << it.key();
		}
	}
	return res;
}

QStringList VipProcessingObject::addMissingAttributes(const QVariantMap& attrs)
{
	QStringList res;
	for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		QVariantMap::const_iterator found = d_data->parameters.attributes.find(it.key());
		if (found == d_data->parameters.attributes.end()) {
			d_data->parameters.attributes[it.key()] = it.value();
			res << it.key();
		}
	}
	return res;
}

void VipProcessingObject::copyParameters(VipProcessingObject* dst)
{
	// set the attributes
	dst->mergeAttributes(this->attributes());

	// set the properties
	for (int i = 0; i < dst->propertyCount(); ++i) {
		if (VipProperty* prop = this->propertyName(dst->propertyAt(i)->name())) {
			dst->propertyAt(i)->setData(prop->data());
		}
	}
}

QVariant VipProcessingObject::initializeProcessing(const QVariant&)
{
	return QVariant();
}

VipProcessingObject* VipProcessingObject::copy() const
{
	// serialize processing
	VipXOStringArchive off;
	off.content("processing", vipVariantFromQObject((this)));

	// deserialize processing
	VipXIStringArchive iff(off.toString());
	return iff.read("processing").value<VipProcessingObject*>();
}

QTransform VipProcessingObject::imageTransform() const
{
	if (inputCount() != 1 || outputCount() != 1)
		return QTransform();

	// get the images before and after processing
	const VipNDArray before = this->inputAt(0)->probe().data().value<VipNDArray>();
	const VipNDArray after = this->outputAt(0)->data().value<VipNDArray>();
	// check image validity
	if (before.isEmpty() || after.isEmpty() || before.shapeCount() != 2 || after.shapeCount() != 2)
		return QTransform();

	bool from_center = true;

	QTransform img = this->imageTransform(&from_center);
	if (img.isIdentity())
		return img;

	QTransform tr;
	if (from_center) {
		// A matrix that cannot be inverted gives the identity back and raises a
		// flag nobody read: the composition below then reported a transform that
		// does not describe what the processing did, and the regions of interest
		// were placed with it.
		bool invertible = false;
		QTransform inv = img.inverted(&invertible);
		if (!invertible) {
			VIP_LOG_WARNING("Image transform of " + objectName() + " cannot be inverted: no transform is reported");
			return QTransform();
		}
		QPointF translate_back;
		translate_back = inv.map(QPointF(after.shape(1) / 2., after.shape(0) / 2.));

		// the global transform is considered to start at the middle of the image
		tr.translate(-before.shape(1) / 2., -before.shape(0) / 2.);
		tr *= img;
		tr.translate(translate_back.x(), translate_back.y());
	}
	else {
		tr = img;
	}
	return tr;
}

QTransform VipProcessingObject::globalImageTransform()
{
	QList<VipProcessingObject*> inspected;
	inspected.append(this);

	VipProcessingObject* current = this;
	while (current) {
		// get all sources processing and sources objects
		VipProcessingObject* src = nullptr;
		if (current->inputCount() > 1)
			return QTransform();
		else if (current->inputCount() == 1) {
			src = current->inputAt(0)->connection()->source() ? current->inputAt(0)->connection()->source()->parentProcessing() : nullptr;
		}
		else
			break;

		if (!src || inspected.indexOf(src) >= 0)
			break;
		else {
			inspected.append(current = src);
		}
	}

	QTransform tr;
	for (int i = inspected.size() - 1; i >= 0; --i) {
		// leaf processing do not have image transforms
		if (inspected[i]->outputCount() > 0) {
			// wait for the processing to update
			inspected[i]->wait(true,100);
			tr *= inspected[i]->imageTransform();
		}
	}
	return tr;
}

void VipProcessingObject::clearInputBuffers()
{
	initialize();
	for (size_t i = 0; i < d_data->flatInputs.size(); ++i)
		d_data->flatInputs[i]->buffer()->clear();

	if (TaskPool* p = d_data->getPool())
		p->clear();
}

void VipProcessingObject::clearInputConnections()
{
	initialize();
	// clear input connections
	for (int i = 0; i < topLevelInputCount(); ++i)
		topLevelInputAt(i)->clearConnection();

	emitProcessingChanged();
}

void VipProcessingObject::clearOutputConnections()
{
	initialize();
	// clear output connections
	for (int i = 0; i < topLevelOutputCount(); ++i)
		topLevelOutputAt(i)->clearConnection();

	emitProcessingChanged();
}

void VipProcessingObject::clearPropertyConnections()
{
	initialize();
	// clear property connections
	for (int i = 0; i < topLevelPropertyCount(); ++i)
		topLevelPropertyAt(i)->clearConnection();

	emitProcessingChanged();
}

void VipProcessingObject::clearConnections()
{
	this->blockSignals(true);
	clearInputConnections();
	clearOutputConnections();
	clearPropertyConnections();
	this->blockSignals(false);
	emitProcessingChanged();
}

void VipProcessingObject::setupOutputConnections(const QString& address)
{
	initialize();
	for (int i = 0; i < outputCount(); ++i)
		outputAt(i)->setConnection(address);

	emitProcessingChanged();
}

// Opens one end, reporting what happened instead of dropping it.
static bool openIOConnection(UniqueProcessingIO* io, VipConnection::IOType type, const QString& processing)
{
	if (!io)
		return false;
	VipConnectionPtr connection = io->connection();
	if (!connection) {
		VIP_LOG_ERROR("No connection for " + processing + "/" + io->name());
		return false;
	}
	if (connection->openConnection(type))
		return true;
	//VIP_LOG_ERROR("Cannot open connection for " + processing + "/" + io->name() + ", address: " + connection->address());
	return false;
}

bool VipProcessingObject::openInputConnections()
{
	bool ok = true;
	for (int i = 0; i < inputCount(); ++i)
		ok = openIOConnection(inputAt(i), VipConnection::InputConnection, objectName()) && ok;
	for (int i = 0; i < propertyCount(); ++i)
		ok = openIOConnection(propertyAt(i), VipConnection::InputConnection, objectName()) && ok;
	emitProcessingChanged();
	return ok;
}

bool VipProcessingObject::openOutputConnections()
{
	bool ok = true;
	for (int i = 0; i < outputCount(); ++i)
		ok = openIOConnection(outputAt(i), VipConnection::OutputConnection, objectName()) && ok;
	emitProcessingChanged();
	return ok;
}

bool VipProcessingObject::openAllConnections()
{
	// open the outputs first
	bool ok = openOutputConnections();
	// then open the inputs/properties
	return openInputConnections() && ok;
}

void VipProcessingObject::removeProcessingPoolFromAddresses()
{
	for (int i = 0; i < inputCount(); ++i)
		if (inputAt(i)->connection())
			inputAt(i)->connection()->removeProcessingPoolFromAddress();
	for (int i = 0; i < propertyCount(); ++i)
		if (propertyAt(i)->connection())
			propertyAt(i)->connection()->removeProcessingPoolFromAddress();
}

VipProcessingObject::ScheduleStrategies VipProcessingObject::scheduleStrategies() const
{
	return d_data->parameters.schedule_strategies;
}

void VipProcessingObject::setScheduleStrategies(ScheduleStrategies st)
{
	if (st != d_data->parameters.schedule_strategies) {
		d_data->parameters.schedule_strategies = st;

		// create the task pool if needed
		/* if (!(st & NoThread) || (st & Asynchronous)) {
			if (!d_data->pool)
				d_data->pool = new TaskPool(this, (QThread::Priority)VipProcessingManager::instance().defaultPriority(metaObject()));
		}*/

		// we clear the input buffers since switching from synchrone to asynchrone processing
		// when having one input buffered might induce a delay in the processing (always one input late)
		clearInputBuffers();
		emitProcessingChanged();
	}
}

void VipProcessingObject::setScheduleStrategy(ScheduleStrategy s, bool on)
{
	if (d_data->parameters.schedule_strategies.testFlag(s) != on) {
		if (on) {
			d_data->parameters.schedule_strategies |= s;
		}
		else {
			d_data->parameters.schedule_strategies &= ~s;
		}

		// create the task pool if needed
		/* if (!(d_data->parameters.schedule_strategies & NoThread) || (d_data->parameters.schedule_strategies & Asynchronous)) {
			if (!d_data->pool)
				d_data->pool = new TaskPool(this, (QThread::Priority)VipProcessingManager::instance().defaultPriority(metaObject()));
		}*/
		clearInputBuffers();
		emitProcessingChanged();
	}
}

bool VipProcessingObject::testScheduleStrategy(ScheduleStrategy s) const
{
	return d_data->parameters.schedule_strategies.testFlag(s);
}

int VipProcessingObject::errorBufferMaxSize() const
{
	SPIN_LOCK(d_data->error_mutex);
	return d_data->parameters.errorBufferMaxSize;
}

void VipProcessingObject::setErrorBufferMaxSize(int size)
{
	SPIN_LOCK(d_data->error_mutex);
	d_data->parameters.errorBufferMaxSize = size;
	while (d_data->errors.size() > size && d_data->errors.size())
		d_data->errors.pop_front();
}

QList<VipErrorData> VipProcessingObject::lastErrors() const
{
	SPIN_LOCK(d_data->error_mutex);
	QList<VipErrorData> res = d_data->errors;
	res.detach();
	return res;
}

void VipProcessingObject::setDeleteOnOutputConnectionsClosed(bool enable)
{
	d_data->parameters.deleteOnOutputConnectionsClosed = enable;
	emitProcessingChanged();
}

bool VipProcessingObject::deleteOnOutputConnectionsClosed() const
{
	return d_data->parameters.deleteOnOutputConnectionsClosed;
}



void VipProcessingObject::setEnabled(bool enable)
{
	bool expect = !enable;
	if (d_data->parameters.enable.compare_exchange_weak(expect, enable)) {
		emitProcessingChanged();
	}
}

void VipProcessingObject::setProcessingVisible(bool vis)
{
	bool expect = !vis;
	if (d_data->parameters.visible.compare_exchange_weak(expect, vis)) {
		emitProcessingChanged();
	}
}

bool VipProcessingObject::isProcessingVisible() const
{
	return d_data->parameters.visible.load(std::memory_order_relaxed);
}

bool VipProcessingObject::isEnabled() const
{
	return d_data->parameters.enable.load(std::memory_order_relaxed);
}

bool VipProcessingObject::update(bool force_run)
{
	// The task pool outlives the derived parts of the object: nothing may be
	// submitted once the destructor has started.
	if (VIP_UNLIKELY(d_data->destruct))
		return false;

	// Exit if disabled
	if (VIP_UNLIKELY(!isEnabled()))
		return false;

	// The descent into the sources below happens under the lock taken here. On a
	// graph where two processings are sources of each other, it came back to this
	// object on the same thread, onto a lock it already held, and the process
	// stopped there.
	VisitedUpdates visited;
	if (VisitedUpdates::current()->contains(this))
		return false;
	VisitedUpdates::current()->insert(this);

	// Make sure the inputs/outputs/properties are correctly initialized
	initialize();

	// Make sure update() cannot be called simultaneously from different threads
	QMutexLocker update_locker(&d_data->update_mutex);

	d_data->updating.fetch_add(1);
	struct ClearUpdating
	{
		std::atomic<int>& count;
		~ClearUpdating() { count.fetch_sub(1); }
	} clear_updating{ d_data->updating };

	// First step: if the schedule strategy is not Asynchronous, update first the source processings.
	if (!(d_data->parameters.schedule_strategies & Asynchronous)) {

		for (const VipInput* in : d_data->flatInputs)
			if (VipOutput* out = in->source())
				if (VipProcessingObject* o = out->parentProcessing())
					if (o != d_data->parentList)
						o->update();
	}

	// Check that all inputs are non empty
	if (!force_run) {

		const bool no_empy_input = !(d_data->parameters.schedule_strategies & AcceptEmptyInput);
		const bool all_new = d_data->parameters.schedule_strategies & AllInputs;
		int new_count = 0;
		for (const VipInput* in : d_data->flatInputs) {
			int status = in->status();
			if (status == -1 && no_empy_input)
				return false;
			if (status <= 0 && all_new)
				return false;
			if (status > 0)
				new_count++;
		}
		if (new_count == 0)
			return false;
	}

	// If SkipIfBusy is set, returns if a processing is scheduled
	if ((d_data->parameters.schedule_strategies & SkipIfBusy))
		if (TaskPool* p = d_data->getPool())
			if (p->remaining() > 0)
				return false;

	if (!(d_data->parameters.schedule_strategies & Asynchronous)) {
		// Synchronous processing
		if (d_data->parameters.schedule_strategies & NoThread)
			// Launch in the calling thread
			this->run();
		else {

			// Just add the processing into the task pool.
			// Even in synchroneous mode, launch the processing through the task pool.
			// This ensures that the processing always runs in the same thread (which might be of importance for a few processings).
			d_data->createPool(this)->push();
			// Released before the wait: this is a spinlock, and the wait lasts as
			// long as the processing, so anything else calling update() on this
			// object burnt a core for that whole time.
			update_locker.unlock();
			wait(false); // wait for the result
		}
	}
	else
		// Asynchronous processing: add to the task pool
		d_data->createPool(this)->push();

	return true;
}

bool VipProcessingObject::reload()
{
	if (this->scheduledUpdates() < 2 && !this->isUpdating())
		return this->update(true);
	return false;
}

void VipProcessingObject::apply() {}

void VipProcessingObject::resetProcessing() {}

void VipProcessingObject::reset()
{
	SPIN_LOCK(d_data->run_mutex);
	this->resetError();
	this->resetProcessing();
}

bool VipProcessingObject::isUpdating() const
{
	return d_data->updating.load(std::memory_order_relaxed) > 0;
}

bool VipProcessingObject::wait(bool wait_for_sources, int max_milli_time)
{
	// wait might be called while the object is not fully destroyed, so use destruct variable
	if (d_data->destruct)
		return false;

	qint64 start = QDateTime::currentMSecsSinceEpoch();

	if (wait_for_sources) {
		// wait for the sources, following their execution order (backward for the list)
		QList<VipProcessingObject*> sources = allSources();
		if (sources.size()) {
			QList<VipProcessingObject*>::iterator it = sources.end();
			while (it != sources.begin()) {
				--it;
				if (max_milli_time > 0) {
					qint64 remaining = max_milli_time - (QDateTime::currentMSecsSinceEpoch() - start);
					if (remaining < 0)
						return false;
					(*it)->wait(false, remaining);
				}
				else
					(*it)->wait(false);
			}
		}
	}

	// Do not basically wait on the task pool.
	// Since some processing might rely on the main event loop (mainly display processings),
	// wait while processing the event loop to avoid a deadlock when calling this function from the main thread.

	const bool use_event_loop = this->useEventLoop();

	// The turns below re-enter the event loop, so a slot can start destroying this
	// object while the wait is still on the stack; destruction was only tested on
	// entry. And a wait with no deadline is a wait that may never end, which is
	// the default. Both are now checked on every turn.
	const QPointer<VipProcessingObject> alive(this);
	const qint64 default_max_milli_time = 30000;
	const qint64 deadline = start + (max_milli_time > 0 ? max_milli_time : default_max_milli_time);

	// Since only display processings use the event loop, start waiting for the processing 10 ms before going throught the event loop
	if (TaskPool* p = d_data->getPool()) {

		// Special case: the processing needs the event loop, and we are waiting from within the GUI thread
		while (use_event_loop && QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
			if (alive.isNull() || d_data->destruct)
				return false;
			if (!this->scheduledUpdates())
				break;
			if (vipProcessEvents(nullptr, 20) == -3)
				break;
			if (QDateTime::currentMSecsSinceEpoch() > deadline)
				return false;
		}

		// The pool below belongs to this object: it must still be there.
		if (alive.isNull() || d_data->destruct)
			return false;

		if (!p->waitForDone(10)) {
			if (QCoreApplication::instance() && use_event_loop) {
				for (;;) {
					if (alive.isNull() || d_data->destruct)
						return false;
					if (!this->scheduledUpdates())
						break;
					// stop waiting if vipProcessEvents is called recursively
					if (vipProcessEvents(nullptr, 2) == -3)
						break;
					if (QDateTime::currentMSecsSinceEpoch() > deadline)
						return false;
				}
			}
			else {
				const qint64 remaining = deadline - QDateTime::currentMSecsSinceEpoch();
				if (remaining < 0)
					return false;
				p->waitForDone(static_cast<int>(remaining));
			}
		}
	}
	else if (VipIODevice* dev = qobject_cast<VipIODevice*>(this)) {
		// Wait for the device to read its current data. The device offers nothing
		// to block on, so this polls; what it must not do is poll for ever when no
		// deadline was asked for.
		const qint64 time = dev->time();
		while (dev->isReading() && dev->time() == time) {
			if (QDateTime::currentMSecsSinceEpoch() > deadline) {
				if (max_milli_time <= 0)
					VIP_LOG_WARNING("Device still reading after 30s, stop waiting for it");
				return false;
			}
			vipSleep(1);
		}
	}

	return true;
}

VipAnyData VipProcessingObject::create(const QVariant& data, const QVariantMap& initial_attributes) const
{
	VipAnyData any(data, time());
	any.setSource((qint64)this);
	if (initial_attributes.size()) {
		any.setAttributes(initial_attributes);
		any.mergeAttributes(d_data->parameters.attributes);
	}
	else
		any.setAttributes(d_data->parameters.attributes);
	return any;
}

double VipProcessingObject::processingRate() const
{
	return d_data->processingRate;
}

int VipProcessingObject::scheduledUpdates() const
{
	if (TaskPool* p = d_data->getPool())
		return p->remaining();
	return 0;
}

bool VipProcessingObject::isFromArchive() const noexcept
{
	return d_data->fromArchive.load(std::memory_order_relaxed);
}

void VipProcessingObject::setFromArchive(bool from_archive) noexcept
{
	d_data->fromArchive.store(from_archive);
}

void VipProcessingObject::emitProcessingChanged()
{
	Q_EMIT processingChanged(this);
}

void VipProcessingObject::emitImageTransformChanged()
{
	// avoid recursion
	if (!d_data->inImageTransformChanged) {
		d_data->inImageTransformChanged = true;
		Q_EMIT imageTransformChanged(this);

		// propagate the signal to all sinks
		for (int o = 0; o < outputCount(); ++o) {
			QList<VipInput*> inputs = outputAt(o)->connection()->sinks();
			for (int i = 0; i < inputs.size(); ++i)
				inputs[i]->parentProcessing()->emitImageTransformChanged();
		}

		d_data->inImageTransformChanged = false;
	}
}

void VipProcessingObject::emitDestroyed()
{
	if (!d_data->emit_destroy) {
		d_data->emit_destroy = true;
		Q_EMIT destroyed(this);
	}
}

qint64 VipProcessingObject::lastProcessingTime() const
{
	return d_data->lastProcessingDate;
}

void VipProcessingObject::excludeFromProcessingRateComputation()
{
	--d_data->processingCount;
}

VipAnyDataList VipProcessingObject::allInputs()
{
	return VipAnyDataList();
}

void VipProcessingObject::run()
{
	{
		// lock to avoid concurrent calls
		SPIN_LOCK(d_data->run_mutex);
		runNoLock();
	}
	// Outside the lock: the signal reaches a processing list in a direct
	// connection, which then takes the mutex of the list, while the other order
	// is taken by the list applying its pipeline. Two threads closed the cycle,
	// and the spinlock spins rather than blocks.
	emitProcessingDone();
}
bool VipProcessingObject::isBeingDestroyed() const noexcept
{
	return d_data->destruct;
}
VipSpinlock& VipProcessingObject::runLock() noexcept
{
	return d_data->run_mutex;
}
void VipProcessingObject::runNoLock()
{
	if (VIP_UNLIKELY(d_data->destruct))
		return;

	if (testScheduleStrategy(SkipIfNoInput)) {
		// if the processing has no new input, skip it
		bool has_input = false;
		for (const VipInput* in : d_data->flatInputs)
			if (in->hasNewData()) {
				has_input = true;
				break;
			}
		if (!has_input) {
			// no new input: just remove all scheduled tasks
			if (TaskPool* p = d_data->getPool())
				p->clearNoLock();
			return;
		}
	}

	resetError();

	qint64 time = 0;
	const bool time_stats = computeTimeStatistics();
	if (time_stats) {

		time = QDateTime::currentMSecsSinceEpoch();
		d_data->lastProcessingDate = time;

		if (d_data->lastTime == VipInvalidTime)
			d_data->lastTime = time;
		else {
			if (time - d_data->lastTime > 500) {

				d_data->processingRate = 1000.0 * static_cast<double>(d_data->processingCount + 1) / (time - d_data->lastTime);
				d_data->processingCount = 0;
				d_data->lastTime = time;
			}
			d_data->processingCount++;
		}
	}
	this->apply();
	if (time_stats)
		d_data->processingTime = (QDateTime::currentMSecsSinceEpoch() - time) * 1000000;
	else
		d_data->processingTime = 0;
	// The signal belongs outside the lock the callers hold around this: they emit
	// it themselves, once this returns.
	d_data->processingDonePending = true;
}

void VipProcessingObject::emitProcessingDone()
{
	if (!d_data->processingDonePending)
		return;
	d_data->processingDonePending = false;
	Q_EMIT processingDone(this, d_data->processingTime);
}

void VipProcessingObject::receiveConnectionOpened(VipProcessingIO* io, int type, const QString& address)
{
	Q_EMIT connectionOpened(io, type, address);
}

void VipProcessingObject::receiveConnectionClosed(VipProcessingIO* io)
{
	Q_EMIT connectionClosed(io);

	if (d_data->parameters.deleteOnOutputConnectionsClosed && !d_data->destruct) {
		// check all output connections. If they are all closed, close the VipProcessingIO

		bool found = false;
		for (VipOutput* out : d_data->flatOutputs) {
			if (out == io) {
				found = true;
				break;
			}
		}

		if (found) {
			bool all_closed = true;

			for (size_t o = 0; o < d_data->outputs.size(); ++o) {
				if (VipOutput* out = d_data->outputs[o]->toOutput()) {
					if (out->connection()->openMode() != VipConnection::UnknownConnection && !out->connection()->sinks().isEmpty())
						all_closed = false;
				}
				else if (VipMultiOutput* mout = d_data->outputs[o]->toMultiOutput()) {
					for (int m = 0; m < mout->count(); ++m) {
						VipOutput* out = mout->at(m);
						if (out->connection()->openMode() != VipConnection::UnknownConnection && !out->connection()->sinks().isEmpty()) {
							all_closed = false;
							break;
						}
					}
				}

				if (!all_closed)
					break;
			}

			if (all_closed)
				this->deleteLater();
		}
	}
}

void VipProcessingObject::receiveDataReceived(VipProcessingIO* io, const VipAnyData& data)
{
	Q_EMIT dataReceived(io, data);
}

void VipProcessingObject::receiveDataSent(VipProcessingIO* io, const VipAnyData& data)
{
	Q_EMIT dataSent(io, data);
}

// The set of codes to log is written from the thread that configures and read
// from the thread that processes, and a QSet is not thread safe: a read during a
// rehash walks a table being rebuilt. The lock that already protects the error
// buffer covers it now; it stopped one member short.
void VipProcessingObject::setLogErrorEnabled(int error_code, bool enable)
{
	SPIN_LOCK(d_data->error_mutex);
	if (enable) {
		d_data->logErrors.insert(error_code);
	}
	else {
		d_data->logErrors.remove(error_code);
	}
}
bool VipProcessingObject::isLogErrorEnabled(int error) const
{
	SPIN_LOCK(d_data->error_mutex);
	bool found = (d_data->logErrors.find(error) != d_data->logErrors.end());
	return found;
}

void VipProcessingObject::setLogErrors(const QSet<int>& errors)
{
	SPIN_LOCK(d_data->error_mutex);
	d_data->logErrors = errors;
}
QSet<int> VipProcessingObject::logErrors() const
{
	SPIN_LOCK(d_data->error_mutex);
	return d_data->logErrors;
}

void VipProcessingObject::newError(const VipErrorData& error)
{
	// Read through the accessor, which takes the lock: the buffer below is
	// protected two lines later, the set was not.
	if (isLogErrorEnabled(error.errorCode())) {
		VIP_LOG_ERROR("(" + vipSplitClassname(this->objectName()) + ") " + error.errorString());
	}

	SPIN_LOCK(d_data->error_mutex);
	d_data->errors.append(error);
	while (d_data->errors.size() > d_data->parameters.errorBufferMaxSize && d_data->errors.size())
		d_data->errors.pop_front();
}

void VipProcessingObject::emitError(QObject* obj, const VipErrorData& err)
{
	VipErrorHandler::emitError(obj, err);

	// force the parent to reemit the error
	if (this->parentObjectPool())
		this->parentObjectPool()->emitError(obj, err);
}

void VipProcessingObject::registerAdditionalInfoObject(const VipProcessingObject::Info& info)
{
	QMutexLocker lock(&VipProcessingManager::instance().d_data->_additional_info_mutex);
	VipProcessingManager::instance().d_data->_infos.insert(info.metatype, info);
	VipProcessingManager::instance().d_data->_dirty_objects = 1;
}
QList<VipProcessingObject::Info> VipProcessingObject::additionalInfoObjects()
{
	QMutexLocker lock(&VipProcessingManager::instance().d_data->_additional_info_mutex);
	return VipProcessingManager::instance().d_data->_infos.values();
}
QList<VipProcessingObject::Info> VipProcessingObject::additionalInfoObjects(int metatype)
{
	QMutexLocker lock(&VipProcessingManager::instance().d_data->_additional_info_mutex);
	return VipProcessingManager::instance().d_data->_infos.values(metatype);
}
bool VipProcessingObject::removeInfoObject(const VipProcessingObject::Info& info, bool all)
{
	QMutexLocker lock(&VipProcessingManager::instance().d_data->_additional_info_mutex);
	QPair<QMultiMap<int, VipProcessingObject::Info>::iterator, QMultiMap<int, VipProcessingObject::Info>::iterator> range =
	  VipProcessingManager::instance().d_data->_infos.equal_range(info.metatype);
	bool erase = false;
	for (QMultiMap<int, VipProcessingObject::Info>::iterator it = range.first; it != range.second;) {
		QMultiMap<int, VipProcessingObject::Info>::iterator tmp = it++;
		if (tmp.value().classname == info.classname && tmp.value().category == info.category) {
			VipProcessingManager::instance().d_data->_infos.erase(tmp);
			erase = true;
			if (!all)
				break;
		}
	}
	VipProcessingManager::instance().d_data->_dirty_objects = 1;
	return erase;
}

QList<const VipProcessingObject*> VipProcessingObject::allObjects()
{

	// current number of object types (metatype id) and infos (as returned by additionalInfoObjects())
	QMutexLocker lock(&VipProcessingManager::instance().d_data->_additional_info_mutex);

	QList<int> types = vipUserTypes<VipProcessingObject*>();
	QList<VipProcessingObject::Info> additionals = additionalInfoObjects();
	if (types.size() == VipProcessingManager::instance().d_data->_obj_types && additionals.size() == VipProcessingManager::instance().d_data->_obj_infos &&
	    !VipProcessingManager::instance().d_data->_dirty_objects)
		return VipProcessingManager::instance().d_data->_allObjects;

	VipProcessingManager::instance().d_data->_obj_types = types.size();
	VipProcessingManager::instance().d_data->_obj_infos = additionals.size();
	VipProcessingManager::instance().d_data->_dirty_objects = 0;

	// The list owns these model objects and nothing else refers to them: the
	// callers only read their metadata. Clearing it alone leaked them all on
	// every rebuild, that is on every plugin load.
	qDeleteAll(VipProcessingManager::instance().d_data->_allObjects);
	VipProcessingManager::instance().d_data->_allObjects.clear();
	int count = types.size() + additionals.size();
	for (int i = 0; i < count; ++i) {
		// get the metatype
		int type = 0;
		if (i < types.size())
			type = types[i];
		else
			type = additionals[i - types.size()].metatype;

		if (type == 0)
			continue;
		// remove the VipProcessingPool from the result
		if (strcmp(vipTypeName(type), "VipProcessingPool*") == 0)
			continue;
		// create the object and initialize it
		VipProcessingObject* obj = nullptr;
		if (i >= types.size())
			obj = additionals[i - types.size()].create(); // easy way
		else {
			obj = vipCreateVariant(type).value<VipProcessingObject*>();
			if (obj) {
				VipProcessingObject* tmp = obj->info().create();
				delete obj;
				obj = tmp;
			}
			else
				continue;
		}
		// Info::create() returns nullptr as soon as its metatype is no longer
		// instantiable, which happens once a plugin is unloaded since the
		// registered infos are never purged.
		if (!obj)
			continue;
		VipProcessingManager::instance().d_data->_allObjects.append(obj);

		// unlock the mutex: VipProcessingManager and VipUniqueId are already protected
		VipProcessingManager::instance().d_data->_additional_info_mutex.unlock();
		// remove from the VipProcessingManager
		VipProcessingManager::instance().remove(obj);
		// remove from the list of ids
		VipTypeId* id = VipUniqueId::typeId(obj->metaObject());
		if (id)
			id->removeId(obj);
		VipProcessingManager::instance().d_data->_additional_info_mutex.lock();
	}
	return VipProcessingManager::instance().d_data->_allObjects;
}

void VipProcessingObject::IOCount(const QMetaObject* meta, int* inputs, int* properties, int* outputs)
{
	if (inputs)
		*inputs = 0;
	if (properties)
		*properties = 0;
	if (outputs)
		*outputs = 0;

	if (!meta)
		return;

	for (int i = 0; i < meta->propertyCount(); ++i) {
		const int type = meta->property(i).userType();

		if (inputs) {
			if (type == qMetaTypeId<VipInput>()) {
				++(*inputs);
				continue;
			}
			else if (type == qMetaTypeId<VipMultiInput>()) {
				++(*inputs);
				continue;
			}
		}
		if (properties) {
			if (type == qMetaTypeId<VipProperty>()) {
				++(*properties);
				continue;
			}
			else if (type == qMetaTypeId<VipMultiProperty>()) {
				++(*properties);
				continue;
			}
		}
		if (outputs) {
			if (type == qMetaTypeId<VipOutput>()) {
				++(*outputs);
				continue;
			}
			else if (type == qMetaTypeId<VipMultiOutput>()) {
				++(*outputs);
				continue;
			}
		}
	}
}

VipProcessingObject::Info VipProcessingObject::findProcessingObject(const QString& name)
{
	QString class_name(name);
	if (class_name.endsWith("*"))
		class_name.remove("*");

	const QList<const VipProcessingObject*> all = allObjects();

	for (int i = 0; i < all.size(); ++i) {
		if (class_name == all[i]->className())
			return all[i]->info();
	}
	return VipProcessingObject::Info();
}

QList<VipProcessingObject*> VipProcessingObjectList::copy(VipProcessingPool* dst)
{
	if (!dst)
		return QList<VipProcessingObject*>();

	// serialize pipeline
	VipXOStringArchive off;
	off.start("pipeline");
	for (int i = 0; i < size(); ++i) {
		if (VipProcessingObject* obj = at(i))
			off.content("processing", QVariant::fromValue(obj));
	}
	off.end();

	// deserialize pipeline
	QList<VipProcessingObject*> new_pipeline;
	VipXIStringArchive iff(off.toString());
	iff.start("pipeline");
	while (true) {
		if (VipProcessingObject* obj = iff.read("processing").value<VipProcessingObject*>())
			new_pipeline.append(obj);
		else
			break;
	}

	if (new_pipeline.size() == size()) {

		// add in a new processing pool to build the connections
		VipProcessingPool tmp;
		for (int i = 0; i < new_pipeline.size(); ++i) {
			new_pipeline[i]->setParent(&tmp);
			// we MUST remove the processing pool name from the address, otherwise the connections will be established with the source processing pool!
			new_pipeline[i]->removeProcessingPoolFromAddresses();
		}
		tmp.openReadDeviceAndConnections();
		tmp.reload();

		// add new pipeline to target processing pool
		for (int i = 0; i < new_pipeline.size(); ++i)
			new_pipeline[i]->setParent(dst);
	}
	else {
		// delete all
		for (int i = 0; i < new_pipeline.size(); ++i)
			delete new_pipeline[i];
		new_pipeline.clear();
	}

	return new_pipeline;
}

class VipProcessingList::PrivateData
{
public:
	PrivateData()
	  : isApplying(false)
	  , useEventLoop(false)
	  , lastTime(VipInvalidTime)
	{
	}
	QList<VipProcessingObject*> objects;
	QList<VipProcessingObject*> directSources;
	// Atomic and tested with an exchange: the flag is read by the thread of a child
	// processing and written by the thread applying the list, and a plain read could
	// see false just after another thread set it.
	std::atomic<bool> isApplying;
	bool useEventLoop;
	qint64 lastTime;
	QRecursiveMutex mutex;
	QString overrideName;
	QTransform transform;
};

VipProcessingList::VipProcessingList(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA();
}

VipProcessingList::~VipProcessingList()
{
	setEnabled(false);
	try {
		wait(false);
	}
	catch (const std::exception& e) {
		VIP_LOG_ERROR("VipProcessingList: " + QString(e.what()));
	}
	catch (...) {
	}
	// Take the objects out before destroying them: a callback triggered by one
	// destruction used to see the ones already destroyed still in the container.
	const QList<VipProcessingObject*> objects = std::move(d_data->objects);
	d_data->objects.clear();
	qDeleteAll(objects);
}

void VipProcessingList::computeParams()
{
	d_data->useEventLoop = false;
	// d_data->directSources = VipProcessingObject::directSources();
	for (int i = 0; i < d_data->objects.size(); ++i) {
		// d_data->directSources += d_data->objects[i]->directSources();
		if (!d_data->useEventLoop && d_data->objects[i]->useEventLoop()) {
			d_data->useEventLoop = true;
			break;
		}
	}
}

const QList<VipProcessingObject*>& VipProcessingList::processings() const
{
	return d_data->objects;
}

QTransform VipProcessingList::computeTransform()
{
	QTransform tr;
	for (int i = 0; i < d_data->objects.size(); ++i) {
		if (d_data->objects[i]->hasError())
			break;
		if (d_data->objects[i]->isEnabled())
			tr *= d_data->objects[i]->imageTransform();
	}
	return tr;
}

bool VipProcessingList::append(VipProcessingObject* obj)
{
	return insert(size(), obj);
}

bool VipProcessingList::remove(VipProcessingObject* obj)
{
	QMutexLocker lock(&d_data->mutex);

	int index = d_data->objects.indexOf(obj);
	if (index >= 0) {
		delete take(index);
		d_data->transform = computeTransform();
		emitImageTransformChanged();
		emitProcessingChanged();
		return true;
	}
	return false;
}

bool VipProcessingList::insert(int index, VipProcessingObject* obj)
{
	QList<QByteArray> names;
	{
		QMutexLocker lock(&d_data->mutex);

		// The position is public input: QList::insert past the end is undefined.
		index = qBound(0, index, static_cast<int>(d_data->objects.size()));

		if (d_data->objects.indexOf(obj) >= 0)
			return false;

		// make sur the object has at least on input and one output
		if (obj->inputCount() == 0 && obj->topLevelInputCount() && obj->topLevelInputAt(0)->toMultiInput())
			obj->topLevelInputAt(0)->toMultiInput()->resize(1);
		if (obj->outputCount() == 0 && obj->topLevelOutputCount() && obj->topLevelOutputAt(0)->toMultiOutput())
			obj->topLevelOutputAt(0)->toMultiOutput()->resize(1);

		if (obj->inputCount() < 1 || obj->outputCount() < 1)
			return false;

		obj->d_data->parentList = this;
		obj->setProperty("VipProcessingList", QVariant::fromValue(this));
		obj->setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::NoThread);

		// set the first input data
		VipAnyData any;
		if (index - 1 >= 0 && index - 1 < d_data->objects.size())
			any = d_data->objects[index - 1]->outputAt(0)->data();
		else
			any = inputAt(0)->probe();

		obj->inputAt(0)->setData(any);
		obj->inputAt(0)->data();
		obj->setParent(nullptr);

		names = sourceProperties();

		connect(obj, SIGNAL(processingDone(VipProcessingObject*, qint64)), this, SLOT(receivedProcessingDone(VipProcessingObject*, qint64)), Qt::DirectConnection);

		d_data->objects.insert(index, obj);
		d_data->transform = computeTransform();
		computeParams();
	}

	// Outside the lock: setSourceProperty is virtual and reimplemented outside
	// this file, plugins included, and the two signals below reach direct
	// connections, so their slots run here as well. None of that belongs under
	// the mutex that every reader of the list waits on.
	for (int i = 0; i < names.size(); ++i)
		obj->setSourceProperty(names[i].data(), this->property(names[i].data()));

	emitImageTransformChanged();
	emitProcessingChanged();

	return true;
}

int VipProcessingList::indexOf(VipProcessingObject* obj) const
{
	QMutexLocker lock(&d_data->mutex);
	return d_data->objects.indexOf(obj);
}

VipProcessingObject* VipProcessingList::at(int i) const
{
	QMutexLocker lock(&d_data->mutex);
	if (i < 0 || i >= d_data->objects.size())
		return nullptr;
	return const_cast<VipProcessingObject*>(d_data->objects[i]);
}

VipProcessingObject* VipProcessingList::take(int i)
{
	VipProcessingObject* obj = nullptr;
	QList<QByteArray> names;
	{
		QMutexLocker lock(&d_data->mutex);

		if (i < 0 || i >= d_data->objects.size())
			return nullptr;
		obj = d_data->objects[i];
		d_data->objects.removeOne(obj);
		obj->d_data->parentList = nullptr;
		obj->setProperty("VipProcessingList", QVariant());

		names = sourceProperties();

		disconnect(obj, SIGNAL(processingDone(VipProcessingObject*, qint64)), this, SLOT(receivedProcessingDone(VipProcessingObject*, qint64)));

		d_data->transform = computeTransform();
		computeParams();
	}

	// remove the source properties from the VipProcessingObject, outside the lock
	for (const QByteArray& name : names)
		obj->setSourceProperty(name.data(), QVariant());

	emitImageTransformChanged();
	emitProcessingChanged();
	return obj;
}

int VipProcessingList::size() const
{
	QMutexLocker lock(&d_data->mutex);
	return d_data->objects.size();
}

void VipProcessingList::setOverrideName(const QString& name)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->overrideName = name;
}
QString VipProcessingList::overrideName() const
{
	QMutexLocker lock(&d_data->mutex);
	return d_data->overrideName;
}

void VipProcessingList::setSourceProperty(const char* name, const QVariant& value)
{
	VipProcessingObject::setSourceProperty(name, value);

	QList<VipProcessingObject*> objects;
	{
		QMutexLocker lock(&d_data->mutex);
		objects = d_data->objects;
	}
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->setProperty(name, value);
}

// static void safeLock(QMutex * mutex)
// {
// //we need to acquire the mutex lock while avoiding to block the main event loop.
// //this is because when the processing list is applied, the mutex is locked and some processings
// //might use and wait for the main event loop. This is the case for some python processing that interact with the GUI
// //(like several functions of the Thermavip Python module).
// while (!mutex->tryLock()) {
// if (QThread::currentThread() == QCoreApplication::instance()->thread())
//	vipProcessEvents(nullptr, 100);
// }
// }

QList<VipProcessingObject*> VipProcessingList::directSources() const
{
	QList<VipProcessingObject*> res = VipProcessingObject::directSources();

	// A snapshot taken under the lock, then walked without it. Every other method
	// of this class holds the mutex over this container; this one walked it while
	// another thread inserted or removed. The lock is not held during the calls
	// below, which reach other processings and would invite an inversion.
	QList<VipProcessingObject*> objects;
	{
		QMutexLocker lock(&d_data->mutex);
		objects = d_data->objects;
	}

	for (int i = 0; i < objects.size(); ++i) {
		if (!objects[i])
			continue;
		const QList<VipProcessingObject*> tmp = objects[i]->directSources();
		for (QList<VipProcessingObject*>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
			if (res.indexOf(*it) < 0 && *it != this)
				res += *it;
	}
	return res;
}

QTransform VipProcessingList::imageTransform(bool* from_center) const
{
	*from_center = false;
	return d_data->transform;
}

void VipProcessingList::receivedProcessingDone(VipProcessingObject* obj, qint64)
{
	// VipProcessingObject * obj = qobject_cast<VipProcessingObject*>(sender());
	if (obj && !d_data->isApplying.load()) {
		applyFrom(obj);
	}
}

bool VipProcessingList::useEventLoop() const
{
	return d_data->useEventLoop;
}

void VipProcessingList::applyFrom(VipProcessingObject* obj)
{
	qint64 st = vipGetNanoSecondsSinceEpoch();

	// The flag, not the mutex, is what keeps two pipelines from overlapping.
	// Exchanged, not tested then set: it is read from the thread of a child
	// processing and written from the thread applying the list, and a plain read
	// could see it false just after another thread had set it.
	bool expected = false;
	if (!d_data->isApplying.compare_exchange_strong(expected, true))
		return;

	// Cleared on every way out, including the early return below.
	struct ClearApplying
	{
		std::atomic<bool>& flag;
		~ClearApplying() { flag.store(false); }
	} clear_applying{ d_data->isApplying };

	// The mutex is taken to read the container and the parameters, and released
	// before the pipeline runs. It used to be held for the whole run, and a
	// processing that pumps the event loop then blocked every other thread that
	// only wanted to look at the list: that is why the locking of directSources()
	// had been commented out rather than fixed.
	QList<VipProcessingObject*> objects;
	QString overrideName;
	qint64 lastTime;
	QTransform previousTransform;
	{
		QMutexLocker lock(&d_data->mutex);
		computeParams();
		objects = d_data->objects;
		overrideName = d_data->overrideName;
		lastTime = d_data->lastTime;
		previousTransform = d_data->transform;
	}

	if (!objects.size()) {
		VipAnyData data = inputAt(0)->data();
		VipAnyData out = create(data.data(), data.attributes());
		{
			QMutexLocker lock(&d_data->mutex);
			d_data->lastTime = data.time();
		}
		out.setTime(data.time());
		if (!overrideName.isEmpty())
			out.setName(overrideName);
		outputAt(0)->setData(out);
		return;
	}

	int index = -1;
	if (obj) {
		index = objects.indexOf(obj);
		// find an enabled processing
		while (index >= 0 && !objects[index]->isEnabled())
			--index;
	}

	// avoid sending the processingDone() signal
	// for (int i = 0; i < d_data->objects.size(); ++i)
	//	d_data->objects[i]->blockSignals(true);

	VipAnyData data;
	if (index < 0) {
		data = inputAt(0)->data();
		if (objects[0]->isEnabled()) {
			objects[0]->inputAt(0)->setData(data);
			objects[0]->update(true);

			if (objects[0]->hasError()) {
				if (objects[0]->lastErrors().size())
					this->setError(objects[0]->lastErrors().last());
			}
			else {
				VipAnyData tmp = objects[0]->outputAt(0)->data();
				data.mergeAttributes(tmp.attributes());
				data.setData(tmp.data());
			}
		}
		lastTime = data.time();
		QMutexLocker lock(&d_data->mutex);
		d_data->lastTime = lastTime;
	}
	else {
		VipAnyData tmp = objects[index]->outputAt(0)->data();
		data.mergeAttributes(tmp.attributes());
		data.setData(tmp.data());
		data.setTime(lastTime);
	}

	index = qMax(index, 0);

	const VipNDArray src_ar = objects.size() ? objects.first()->inputAt(0)->probe().value<VipNDArray>() : VipNDArray();

	bool need_compute_transform = !src_ar.isEmpty() && src_ar.shapeCount() == 2;

	if (!objects[index]->hasError()) {
		for (int i = index + 1; i < objects.size(); ++i) {
			if (!objects[i]->isEnabled())
				continue;
			objects[i]->inputAt(0)->setData(data);
			objects[i]->update(true);

			if (objects[i]->hasError()) {
				if (objects[i]->lastErrors().size())
					this->setError(objects[i]->lastErrors().last());
				break;
			}

			VipAnyData tmp = objects[i]->outputAt(0)->data();
			data.mergeAttributes(tmp.attributes());
			data.setData(tmp.data());
		}
	}

	QTransform tr;
	if (need_compute_transform) {
		// compute the list image transform
		QMutexLocker lock(&d_data->mutex);
		tr = computeTransform();
	}

	// for (int i = 0; i < d_data->objects.size(); ++i)
	//	d_data->objects[i]->blockSignals(false);

	// vip_debug("1 %s name: %s\n",objectName().toLatin1().data(), attribute("Name").toString().toLatin1().data());
	VipAnyData out = create(data.data(), data.attributes());
	out.setTime(data.time());
	if (!overrideName.isEmpty())
		out.setName(overrideName);
	// vip_debug("2 %s name: %s\n", objectName().toLatin1().data(), out.name().toLatin1().data());

	outputAt(0)->setData(out);

	if (tr != previousTransform) {
		{
			QMutexLocker lock(&d_data->mutex);
			d_data->transform = tr;
		}
		emitImageTransformChanged();
	}

	if (obj) {
		// this function wasn't called from apply(), the signal processingDone() is not emitted, so emit it
		qint64 elapsed = vipGetNanoSecondsSinceEpoch() - st;
		Q_EMIT processingDone(this, elapsed);
	}
}

void VipProcessingList::apply()
{
	applyFrom(nullptr);
}

void VipProcessingList::resetProcessing()
{
	QMutexLocker lock(&d_data->mutex);
	for (int i = 0; i < d_data->objects.size(); ++i) {
		d_data->objects[i]->reset();
	}
}

QList<VipProcessingObject::Info> VipProcessingList::validProcessingObjects(const QVariant& input_type)
{
	QList<VipProcessingObject::Info> infos = VipProcessingObject::validProcessingObjects(QVariantList() << input_type, 1).values();

	// only keep the Info with inputTransformation to true
	for (int i = 0; i < infos.size(); ++i) {
		if (infos[i].displayHint != InputTransform) {
			infos.removeAt(i);
			--i;
		}
	}
	return infos;
}

class VipSceneModelBasedProcessing::PrivateData
{
public:
	QPointer<VipShapeSignals> shapeSignals;
	VipLazySceneModel lazyScene;

	// Raw VipSceneModel and VipShape, only used when identifying the scene model and/or shape based on VipLazySceneModel failed
	VipSceneModel rawScene;

	QReadWriteLock shapeLock;
	VipShape dirtyShape;
	bool reloadOnSceneChanges{ false };
	MergeStrategy mergeStrategy{ NoMerge };
	QTransform shapeTransform;
};

VipSceneModelBasedProcessing::VipSceneModelBasedProcessing(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	if (VipProcessingIO* io = this->topLevelPropertyName("shape_ids"))
		if (VipMultiProperty* mp = io->toMultiProperty())
			mp->resize(1);
}

VipSceneModelBasedProcessing::~VipSceneModelBasedProcessing()
{
	if (d_data->shapeSignals) {
		disconnect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(dirtyShape()));
		disconnect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
	}
}

void VipSceneModelBasedProcessing::dirtyShape()
{
	QWriteLocker lock(&d_data->shapeLock);
	d_data->dirtyShape = VipShape();
}

void VipSceneModelBasedProcessing::setMergeStrategy(MergeStrategy st)
{
	d_data->mergeStrategy = st;
	dirtyShape();
}

VipSceneModelBasedProcessing::MergeStrategy VipSceneModelBasedProcessing::mergeStrategy() const
{
	return d_data->mergeStrategy;
}

VipSceneModel VipSceneModelBasedProcessing::sceneModel()
{

	VipSceneModel sm;
	bool found = false;

	// Check if the first property is connected to a source, and grab the scenemodel from this source
	if (VipOutput* src = propertyAt(0)->connection()->source()) {
		// An output can outlive the processing that owns it, and a connection can
		// name a source that has none.
		if (VipProcessingObject* parent = src->parentProcessing())
			parent->wait();
		QVariant v = src->data().data();
		if (v.userType() == qMetaTypeId<VipSceneModel>()) {
			sm = v.value<VipSceneModel>();
			found = true;
		}
	}

	if (!found) {
		// Check if the first property is directly a VipSceneModel
		QVariant v = propertyAt(0)->data().data();
		if (v.userType() == qMetaTypeId<VipSceneModel>()) {
			sm = v.value<VipSceneModel>();
			found = true;
		}
		// Check if the first property is directly a VipLazySceneModel
		else if (v.userType() == qMetaTypeId<VipLazySceneModel>()) {
			if (d_data->lazyScene.isEmpty()) {
				d_data->lazyScene = v.value<VipLazySceneModel>();
				if (d_data->lazyScene.hasSceneModel()) {
					sm = d_data->lazyScene.sceneModel();
					found = true;
				}
			}
			else {
				sm = d_data->lazyScene.sceneModel();
				found = true;
			}
		}
	}

	if (!found) {
		// if not found, look for the dynamic property "VipSceneModel"
		QVariant v = this->property("VipSceneModel");
		if (v.userType() == qMetaTypeId<VipSceneModel>()) {
			sm = v.value<VipSceneModel>();
			found = true;
		}
	}

	// we did not find a scene model: return the raw scene (if any)
	if (!found)
		return d_data->rawScene;

	// Connect the VipShapeSignals to the reload() slot. This function is called
	// from the threads that process, so the swap is done under the lock that
	// already guards the neighbouring state, and the connections are made once it
	// is released.
	QPointer<VipShapeSignals> previous;
	VipShapeSignals* current = sm.shapeSignals();
	{
		QWriteLocker lock(&d_data->shapeLock);
		if (current == d_data->shapeSignals)
			return sm;
		previous = d_data->shapeSignals;
		d_data->shapeSignals = current;
	}

	if (previous) {
		disconnect(previous, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(dirtyShape()));
		disconnect(previous, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
	}
	if (current) {
		if (d_data->reloadOnSceneChanges)
			connect(current, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
		connect(current, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(dirtyShape()));
	}

	return sm;
}

void VipSceneModelBasedProcessing::setReloadOnSceneChanges(bool enable)
{
	if (d_data->reloadOnSceneChanges != enable) {
		if (d_data->shapeSignals) {
			if (!enable)
				disconnect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
			else
				connect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
		}
		d_data->reloadOnSceneChanges = enable;
	}
}
bool VipSceneModelBasedProcessing::reloadOnSceneChanges() const
{
	return d_data->reloadOnSceneChanges;
}

void VipSceneModelBasedProcessing::setShapeTransform(const QTransform& tr)
{
	d_data->shapeTransform = tr;
	dirtyShape();
}
QTransform VipSceneModelBasedProcessing::shapeTransform() const
{
	return d_data->shapeTransform;
}

static VipShapeList applyTr(const VipShapeList& input, const QTransform& tr)
{
	VipShapeList res;
	for (int i = 0; i < input.size(); ++i) {
		VipShape s = input[i].copy();
		if (!tr.isIdentity())
			s.transform(tr);
		res.push_back(s);
	}
	return res;
}

VipShapeList VipSceneModelBasedProcessing::shapes()
{
	// first check if the first property is directly a VipShape, in which case we use it
	QVariant v = this->propertyAt(0)->data().data();
	if (v.userType() == qMetaTypeId<VipShape>()) {
		VipShape sh = v.value<VipShape>();
		VipSceneModel sm = sh.parent();
		if (!sm.isNull() && sm.shapeSignals() != d_data->shapeSignals) {
			if (d_data->shapeSignals) {
				disconnect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(dirtyShape()));
				disconnect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
			}
			if ((d_data->shapeSignals = sm.shapeSignals())) {
				if (d_data->reloadOnSceneChanges)
					connect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(reload()));
				connect(d_data->shapeSignals, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(dirtyShape()));
			}
		}
		return applyTr(VipShapeList() << sh, d_data->shapeTransform);
	}

	VipSceneModel sm = sceneModel();

	// then use the shape_id property on the VipSceneModel
	// By name, not by position: a session file controls the size of the
	// shape_ids multi-property, and an emptied one used to make propertyAt(1)
	// index past the end of the flattened property vector.
	VipProperty* ids_prop = this->propertyName("shape_ids");
	if (!ids_prop)
		return VipShapeList();

	QString shape_id = ids_prop->data().value<QString>();
	if (!shape_id.isEmpty()) {
		if (sm.hasGroup(shape_id))
			return applyTr(sm.shapes(shape_id), d_data->shapeTransform);
		else
			return applyTr(VipShapeList() << sm.find(shape_id), d_data->shapeTransform);
	}

	// get the shapes
	QStringList ids = ids_prop->data().value<QStringList>();
	if (!ids.size())
		return VipShapeList();

	VipShapeList shapes;
	for (int i = 0; i < ids.size(); ++i) {
		VipShape tmp = sm.find(ids[i]);
		if (!tmp.isNull())
			shapes.append(tmp);
	}
	return applyTr(shapes, d_data->shapeTransform);
}

VipShape VipSceneModelBasedProcessing::shape()
{
	// returns the dirt shape if valid (combination of several shapes)
	{
		QReadLocker lock(&d_data->shapeLock);
		if (!d_data->dirtyShape.isNull())
			return d_data->dirtyShape;
	}

	VipShapeList shapes = this->shapes();
	if (shapes.isEmpty())
		return VipShape();
	else if (shapes.size() == 1 || d_data->mergeStrategy == NoMerge)
		return shapes.last();
	else {
		// merge shapes
		VipShape res = shapes.first();
		for (int i = 1; i < shapes.size(); ++i) {
			if (d_data->mergeStrategy == MergeUnion)
				res.unite(shapes[i]);
			else
				res.intersect(shapes[i]);
		}
		QWriteLocker lock(&d_data->shapeLock);
		d_data->dirtyShape = res;
		return res;
	}
}

void VipSceneModelBasedProcessing::setSceneModel(const VipSceneModel& scene, const QString& identifier)
{
	this->propertyAt(0)->setData(VipAnyData(QVariant::fromValue(VipLazySceneModel(scene)), VipInvalidTime));
	if (!identifier.isEmpty()) {
		// By name, like the reader: the size of the shape_ids multi-property is
		// not fixed by the class.
		if (VipProperty* ids_prop = this->propertyName("shape_ids"))
			ids_prop->setData(identifier);
	}
	d_data->rawScene = scene;
	d_data->lazyScene = VipLazySceneModel(scene);
	dirtyShape();
}

void VipSceneModelBasedProcessing::setSceneModel(const VipSceneModel& scene, const VipShapeList& shapes)
{
	QStringList ids;
	for (int i = 0; i < shapes.size(); ++i)
		if (shapes[i].parent() == scene)
			ids.append(shapes[i].identifier());

	setSceneModel(scene, ids);
}

void VipSceneModelBasedProcessing::setSceneModel(const VipSceneModel& scene, const QStringList& identifiers)
{
	this->propertyAt(0)->setData(VipAnyData(QVariant::fromValue(VipLazySceneModel(scene)), VipInvalidTime));
	if (VipProperty* ids_prop = this->propertyName("shape_ids"))
		ids_prop->setData(VipAnyData(QVariant::fromValue(identifiers), VipInvalidTime));

	d_data->rawScene = scene;
	d_data->lazyScene = VipLazySceneModel(scene);
	dirtyShape();
}

void VipSceneModelBasedProcessing::setShape(const VipShape& shape)
{
	dirtyShape();
	if (VipSceneModel model = shape.parent()) {
		QString identifier = shape.identifier();
		setSceneModel(model, identifier);
	}
	else {
		this->propertyAt(0)->setData(VipAnyData(QVariant::fromValue(shape), VipInvalidTime));
		QWriteLocker lock(&d_data->shapeLock);
		d_data->dirtyShape = shape;
	}
}

void VipSceneModelBasedProcessing::setFixedShape(const VipShape& shape)
{
	dirtyShape();
	this->propertyAt(0)->setData(VipAnyData(QVariant::fromValue(shape), VipInvalidTime));
	QWriteLocker lock(&d_data->shapeLock);
	d_data->dirtyShape = shape;
}

void VipMultiInputToOne::apply()
{
	// first, grab all the input data
	QMultiMap<qint64, VipAnyData> data;
	int count = inputCount();
	for (int i = 0; i < count; ++i) {
		VipInput* input = inputAt(i);
		while (input->hasNewData()) {
			VipAnyData any = input->data();
			data.insert(any.time(), any);
		}
	}

	// send the data by increasing time order
	for (QMultiMap<qint64, VipAnyData>::const_iterator it = data.begin(); it != data.end(); ++it) {
		outputAt(0)->setData(it.value());
	}
}

VipSwitch::VipSwitch(QObject* parent)
  : VipProcessingObject(parent)
{
	// initialize the property to 0
	propertyAt(0)->setData(0);

	this->setScheduleStrategy(AcceptEmptyInput);
}

void VipSwitch::apply()
{
	// get the input index
	int index = propertyAt(0)->data().value<int>();

	if (index < 0 || index >= inputCount()) {
		setError("VipSwitch: wrong input number", VipProcessingObject::WrongInputNumber);
		return;
	}

	bool set_output = false;
	for (int i = 0; i < inputCount(); ++i) {
		if (inputAt(i)->hasNewData()) {
			VipAnyData any = inputAt(i)->data();
			if (index == i) {
				outputAt(0)->setData(any);
				set_output = true;
			}
		}
	}

	if (!set_output)
		this->excludeFromProcessingRateComputation();
}

void VipExtractAttribute::apply()
{
	VipAnyData in = inputAt(0)->data();
	QString attribute = propertyAt(0)->value<QString>();
	QVariant value = in.attribute(attribute);
	if (value.userType() != 0) {
		if (propertyAt(1)->value<bool>()) {
			bool ok;
			value = QVariant(ToDouble(value, &ok));
			if (!ok) {
				setError("cannot convert attribute value to double");
				return;
			}
		}
		// if given attribute is non null, send it to the output
		VipAnyData out = create(value);
		out.setTime(in.time());
		outputAt(0)->setData(out);
	}
	else {
		setError("wrong attribute name");
		return;
	}
}

double VipExtractAttribute::ToDouble(const QVariant& var, bool* ok)
{
	// The whole string has to be a number, not merely start with one. The text
	// stream this used to rely on consumes the longest numeric prefix and still
	// reports success, so "12abc" produced 12 and "1,5" produced 1, both flagged
	// valid and sent downstream as measurements. Non finite values are refused
	// for the same reason.
	bool work = false;
	const double res = var.toDouble(&work);
	if (work && std::isfinite(res)) {
		if (ok)
			*ok = true;
		return res;
	}

	// Surrounding whitespace stays tolerated, nothing else.
	bool converted = false;
	const double parsed = var.toString().trimmed().toDouble(&converted);
	if (converted && std::isfinite(parsed)) {
		if (ok)
			*ok = true;
		return parsed;
	}

	if (ok)
		*ok = false;
	return 0;
}

#include "VipArchive.h"

VipArchive& operator<<(VipArchive& arch, const UniqueProcessingIO& p)
{
	arch.content("name", p.name());
	arch.content("enabled", p.isEnabled());
	return arch.content("connection", p.connection()->address());
}

VipArchive& operator>>(VipArchive& stream, UniqueProcessingIO& p)
{
	QString name = stream.read("name").toString();
	if (!stream.hasError()) {
		p.setName(name);
		p.setEnabled(stream.read("enabled").toBool());
		p.setConnection(stream.read("connection").toString());
	}
	return stream;
}

VipArchive& operator<<(VipArchive& arch, const VipInput& input)
{
	return arch << static_cast<const UniqueProcessingIO&>(input);
}
VipArchive& operator>>(VipArchive& stream, VipInput& input)
{
	return stream >> static_cast<UniqueProcessingIO&>(input);
}

VipArchive& operator<<(VipArchive& stream, const VipOutput& output)
{
	return stream << static_cast<const UniqueProcessingIO&>(output);
}
VipArchive& operator>>(VipArchive& stream, VipOutput& output)
{
	return stream >> static_cast<UniqueProcessingIO&>(output);
}

VipArchive& operator<<(VipArchive& stream, const VipProperty& property)
{
	stream << static_cast<const UniqueProcessingIO&>(property);
	return stream.content("value", property.data().data());
}

VipArchive& operator>>(VipArchive& stream, VipProperty& property)
{
	stream >> static_cast<UniqueProcessingIO&>(property);
	property.setData(VipAnyData(stream.read("value"), VipInvalidTime));
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipMultiInput& minput)
{
	stream.content("count", minput.count());
	stream.content("multi_input_name", minput.name());
	for (int i = 0; i < minput.count(); ++i)
		stream.content(*minput.at(i));
	return stream;
}

// An element count read from a session file is untrusted input: it is used
// directly as a loop bound and, in one case, as an index. Nothing downstream
// bounds it, and the per-element cost is an allocation plus a registration in a
// global list, so a crafted file exhausts memory with no message. This is a
// plausibility cap, not a format limit: no legitimate processing declares
// thousands of inputs.
static constexpr int vipMaxSerializedCount = 4096;

static bool vipReadCount(VipArchive& stream, const char* name, int& count)
{
	count = 0;
	if (!stream.content(name, count))
		return false;
	if (count < 0 || count > vipMaxSerializedCount) {
		stream.setError(QString("unexpected element count in archive: %1").arg(count));
		count = 0;
		return false;
	}
	return true;
}

VipArchive& operator>>(VipArchive& stream, VipMultiInput& minput)
{
	QString name;
	int count = 0;
	vipReadCount(stream, "count", count);
	stream.content("multi_input_name", name);
	minput.setName(name);
	minput.clear();
	for (int i = 0; i < count && stream; ++i) {
		VipInput input;
		stream.content(input);
		minput.setAt(i, input);
	}
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipMultiOutput& moutput)
{
	stream.content("count", moutput.count());
	stream.content("multi_output_name", moutput.name());
	for (int i = 0; i < moutput.count(); ++i)
		stream.content(*moutput.at(i));
	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipMultiOutput& moutput)
{
	QString name;
	int count = 0;
	vipReadCount(stream, "count", count);
	stream.content("multi_output_name", name);
	moutput.setName(name);
	moutput.clear();
	for (int i = 0; i < count && stream; ++i) {
		VipOutput output;
		stream.content(output);
		moutput.add(output);
	}
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipMultiProperty& mproperty)
{
	stream.content("count", mproperty.count());
	stream.content("multi_property_name", mproperty.name());
	for (int i = 0; i < mproperty.count(); ++i)
		stream.content(*mproperty.at(i));
	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipMultiProperty& mproperty)
{
	QString name;
	int count = 0;
	const bool countRead = vipReadCount(stream, "count", count);
	stream.content("multi_property_name", name);
	mproperty.setName(name);
	// Clearing before the count is known let an absent or corrupt block empty
	// the multi-property, which permanently removes the flattened properties
	// the object was built with. Leave it untouched instead.
	if (!countRead)
		return stream;
	mproperty.clear();
	for (int i = 0; i < count && stream; ++i) {
		VipProperty property;
		stream.content(property);
		mproperty.add(property);
	}
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipProcessingObject* r)
{
	stream.content("processing_name", r->objectName());
	stream.content("attributes", r->attributes());
	stream.content("scheduleStrategies", (int)r->scheduleStrategies());
	stream.content("isEnabled", r->isEnabled());
	stream.content("isVisible", r->isProcessingVisible());
	stream.content("deleteOnOutputConnectionsClosed", r->deleteOnOutputConnectionsClosed());

	// added in 2.2.14
	// we need to save the init parameter to retrieve it at reloading
	VipProcessingObject::Info info = r->info();
	stream.content("registered", QString::number(info.metatype) + info.category + info.classname);

	vipSaveCustomProperties(stream, r);

	// save general informations on the processing
	// stream.start("infos");
	//
	// //save inputs;
	// QList<int> inputs;
	// for(int i=0; i < r->topLevelInputCount(); ++i)
	// inputs.z
	//
	// stream.end();

	int icount = r->topLevelInputCount();
	int ocount = r->topLevelOutputCount();
	int pcount = r->topLevelPropertyCount();

	for (int i = 0; i < icount; ++i) {
		VipProcessingIO* p = r->topLevelInputAt(i);
		if (VipMultiInput* mi = p->toMultiInput())
			stream.content(*mi);
		else
			stream.content(*p->toInput());
	}

	for (int i = 0; i < ocount; ++i) {
		VipProcessingIO* p = r->topLevelOutputAt(i);
		if (VipMultiOutput* mo = p->toMultiOutput())
			stream.content(*mo);
		else
			stream.content(*p->toOutput());
	}

	for (int i = 0; i < pcount; ++i) {
		VipProcessingIO* p = r->topLevelPropertyAt(i);
		if (VipMultiProperty* mp = p->toMultiProperty())
			stream.content(*mp);
		else
			stream.content(*p->toProperty());
	}

	return stream;
}

// Only the declared flags, so a value from a file cannot set bits the enumeration
// does not define.
static VipProcessingObject::ScheduleStrategies toScheduleStrategies(int value)
{
	const int declared = VipProcessingObject::AllInputs | VipProcessingObject::Asynchronous | VipProcessingObject::SkipIfBusy | VipProcessingObject::AcceptEmptyInput |
			     VipProcessingObject::SkipIfNoInput | VipProcessingObject::NoThread;
	return static_cast<VipProcessingObject::ScheduleStrategies>(value & declared);
}

VipArchive& operator>>(VipArchive& stream, VipProcessingObject* r)
{
	// Read first, apply second: the connections used to be dropped on the first
	// line, so a truncated archive left the object disconnected and half set.
	QString name;
	stream.content("processing_name", name);
	const QVariantMap attributes = stream.read("attributes").value<QVariantMap>();
	const int strategies = stream.read("scheduleStrategies").toInt();
	const bool is_enabled = stream.read("isEnabled").toBool();
	const bool is_visible = stream.read("isVisible").toBool();
	const bool delete_on_closed = stream.read("deleteOnOutputConnectionsClosed").toBool();

	if (stream.hasError()) {
		stream.resetError();
		return stream;
	}

	r->clearConnections();
	r->setObjectName(name);

	// set the attributes, but keep the 'Name' one (used in VipIODevice)
	const QString kept_name = r->attribute("Name").toString();
	r->setAttributes(attributes);
	if (!kept_name.isEmpty())
		r->setAttribute("Name", kept_name);

	r->setScheduleStrategies(toScheduleStrategies(strategies));
	r->setEnabled(is_enabled);
	r->setProcessingVisible(is_visible);
	r->setDeleteOnOutputConnectionsClosed(delete_on_closed);

	// added in 2.2.14
	// find the registered processing info (if any)
	stream.save();
	QString registered;
	if (stream.content("registered", registered)) {
		// The save point is dropped, not rewound: leaving it on the stack made the
		// next restore() of an enclosing reader rewind to this position.
		stream.discardSave();
		if (registered.size()) {
			// find the corresponding info object
			const QList<VipProcessingObject::Info> infos = VipProcessingObject::additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i) {
				if (QString::number(infos[i].metatype) + infos[i].category + infos[i].classname == registered) {
					r->d_data->info = infos[i];
					break;
				}
			}
		}
	}
	else
		stream.restore();

	vipLoadCustomProperties(stream, r);

	int icount = r->topLevelInputCount();
	int ocount = r->topLevelOutputCount();
	int pcount = r->topLevelPropertyCount();

	for (int i = 0; i < icount; ++i) {
		VipProcessingIO* p = r->topLevelInputAt(i);
		if (VipMultiInput* mi = p->toMultiInput()) {
			stream.content(*mi);
		}
		else
			stream.content(*p->toInput());
	}

	for (int i = 0; i < ocount; ++i) {
		VipProcessingIO* p = r->topLevelOutputAt(i);
		if (VipMultiOutput* mo = p->toMultiOutput()) {
			stream.content(*mo);
		}
		else
			stream.content(*p->toOutput());
	}

	for (int i = 0; i < pcount; ++i) {
		VipProcessingIO* p = r->topLevelPropertyAt(i);
		stream.save();
		if (VipMultiProperty* mp = p->toMultiProperty()) {
			stream.content(*mp);
		}
		else
			stream.content(*p->toProperty());

		if (stream.hasError())
			stream.restore();
	}

	// Mark the object as coming from a file. Its properties are now whatever the
	// file said, and some processings turn a property into executable code.
	r->setFromArchive(true);

	// initialize
	r->initialize(true);
	stream.resetError();
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipProcessingList* lst)
{
	QList<int> indexes;
	// find object that can be serialized
	for (int i = 0; i < lst->size(); ++i) {
		if (lst->at(i)->property("_vip_no_serialize").toBool())
			continue;
		QString classname = lst->at(i)->metaObject()->className() + QString("*");
		VipProcessingObject* obj = vipCreateVariant(classname.toLatin1().data()).value<VipProcessingObject*>();
		if (obj) {
			indexes.append(i);
			delete obj;
		}
	}

	stream.content("count", indexes.size());
	for (int i = 0; i < indexes.size(); ++i) {
		stream.content(lst->at(indexes[i]));
	}
	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipProcessingList* lst)
{
	// Read as a 64 bit value: the writer stores a container size, so narrowing to
	// int before the check would let a huge count wrap into a small one.
	const qlonglong count = stream.read("count").value<qlonglong>();
	if (count < 0 || count > vipMaxSerializedCount) {
		stream.setError(QString("unexpected processing count in archive: %1").arg(count));
		return stream;
	}
	for (qlonglong i = 0; i < count && stream; ++i) {
		VipProcessingObject* obj = stream.read().value<VipProcessingObject*>();
		if (obj)
			lst->append(obj);
	}
	return stream;
}

void serialize_VipDataListManager(VipArchive& arch)
{
	if (arch.mode() == VipArchive::Read) {
		if (arch.start("VipProcessingManager")) {

			arch.save();
			int arrayThreads = 1;
			if (arch.content("arrayThreads", arrayThreads))
				vipSetIterateThreadCount(arrayThreads);
			else
				arch.restore();

			int limit_type = arch.read("listLimitType").toInt();
			int max_list_size = arch.read("maxListSize").toInt();
			qint64 max_memory = arch.read("maxListMemory").toLongLong();
			QSet<int> logErrors = arch.read("logErrors").value<QSet<int>>();
			PriorityMap prio = arch.read("priorities").value<PriorityMap>();
			bool has_error = arch.hasError();
			arch.resetError();

			// A truncated archive gives five default values, and applying four of
			// them anyway reset the whole process to a zero list limit and an
			// empty priority map.
			if (!has_error && !VipProcessingManager::instance().d_data->_lock_list_manager) {
				// These values come from a file and are applied to every input queue
				// of the process and to the priority of every processing thread. They
				// were passed on as they stood: a limit type outside the enumeration
				// switched eviction off and every queue then grew without bound, a
				// size of zero emptied each queue on every push, and a priority
				// outside the enumeration reached QThread::setPriority.
				const int known_limits = VipDataList::None | VipDataList::Number | VipDataList::MemorySize;
				if ((limit_type & ~known_limits) == 0 && max_list_size > 0 && max_memory > 0) {
					VipProcessingManager::setListLimitType(limit_type);
					VipProcessingManager::setMaxListSize(max_list_size);
					VipProcessingManager::setMaxListMemory(max_memory);
				}
				else
					VIP_LOG_WARNING("Refused the queue limits of the session: they are outside what the program accepts");

				VipProcessingManager::setLogErrors(logErrors);

				PriorityMap accepted;
				for (PriorityMap::const_iterator it = prio.begin(); it != prio.end(); ++it) {
					switch (it.value()) {
						case QThread::IdlePriority:
						case QThread::LowestPriority:
						case QThread::LowPriority:
						case QThread::NormalPriority:
						case QThread::HighPriority:
						case QThread::HighestPriority:
						case QThread::TimeCriticalPriority:
						case QThread::InheritPriority:
							accepted.insert(it.key(), it.value());
							break;
						default:
							VIP_LOG_WARNING("Refused the thread priority of the session for " + it.key() + ": it is not one of the values the program defines");
							break;
					}
				}
				VipProcessingManager::setDefaultPriorities(accepted);
			}

			arch.end();
		}
	}
	else if (arch.mode() == VipArchive::Write) {
		if (arch.start("VipProcessingManager")) {
			arch.content("arrayThreads", vipIterateThreadCount());
			arch.content("listLimitType", VipProcessingManager::listLimitType());
			arch.content("maxListSize", VipProcessingManager::maxListSize());
			arch.content("maxListMemory", VipProcessingManager::maxListMemory());
			arch.content("logErrors", VipProcessingManager::logErrors());
			arch.content("priorities", VipProcessingManager::defaultPriorities());

			arch.end();
		}
	}
}

static int registerSerializeOperators()
{
	qRegisterMetaType<PriorityMap>();
	qRegisterMetaType<ErrorCodes>();
	qRegisterMetaType<VipProcessingObject::Info>();
	qRegisterMetaType<VipProcessingObjectInfoList>();

	qRegisterMetaType<VipInput*>();
	qRegisterMetaType<VipMultiInput*>();
	qRegisterMetaType<VipProperty*>();
	qRegisterMetaType<VipMultiProperty*>();
	qRegisterMetaType<VipOutput*>();
	qRegisterMetaType<VipMultiOutput*>();

	qRegisterMetaTypeStreamOperators<PriorityMap>();
	qRegisterMetaTypeStreamOperators<ErrorCodes>();
	vipRegisterArchiveStreamOperators<VipInput>();
	vipRegisterArchiveStreamOperators<VipMultiInput>();
	vipRegisterArchiveStreamOperators<VipOutput>();
	vipRegisterArchiveStreamOperators<VipMultiOutput>();
	vipRegisterArchiveStreamOperators<VipProperty>();
	vipRegisterArchiveStreamOperators<VipMultiProperty>();
	vipRegisterArchiveStreamOperators<VipProcessingObject*>();
	vipRegisterArchiveStreamOperators<VipProcessingList*>();
	vipRegisterSettingsArchiveFunctions(serialize_VipDataListManager, serialize_VipDataListManager);
	return 0;
}

// Initialization function should be called first
static int _registerSerializeOperators = vipStaticInit("vipPrependInitializationFunction(registerSerializeOperators)", []() { vipPrependInitializationFunction(registerSerializeOperators); });
