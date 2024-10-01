/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_PROCESSING_OBJECT_H
#define VIP_PROCESSING_OBJECT_H

#include <deque>
#include <type_traits>

#include <QIcon>
#include <QMap>
#include <QMetaProperty>
#include <QMutex>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>
#include <QThread>
#include <QVariant>
#include <QWaitCondition>

#include "VipArchive.h"
#include "VipCore.h"
#include "VipDataType.h"
#include "VipFunctional.h"
#include "VipLock.h"
#include "VipProcessingHelper.h"
#include "VipTimestamping.h"

/// \addtogroup Core
/// @{

/// @brief VipAnyData represents any kind of data exchanged by VipProcessingObject instances.
/// It holds a QVariant data (usually containing a VipNDArray, a VipPointVector, a numerical value, etc.),
/// a timestamp (expressed in nanoseconds), a source (the VipProcessingObject that creates it) and any number of attributes.
///
/// To create a VipAnyData from a QVariant object within a VipProcessingObject, you should use the function VipProcessingObject::create().
/// It will create the VipAnyData filled with the variant data, the timestamp, the source and the attributes (inherited from the VipProcessingObject).
///
class VIP_CORE_EXPORT VipAnyData
{
	qint64 m_source;
	qint64 m_time;
	QVariantMap m_attributes;
	QVariant d_data;

public:
	VipAnyData();
	VipAnyData(const QVariant& data, qint64 time = 0);
	VipAnyData(QVariant&& data, qint64 time = 0);
	VipAnyData(const VipAnyData&) = default;
	VipAnyData& operator=(const VipAnyData&) = default;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
#endif
	VipAnyData(VipAnyData&&) = default;
	VipAnyData& operator=(VipAnyData&&) = default;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

	/// @brief Set the data attributes.
	/// You can set any kind of attribute that can be used for display purpose, for processing or as simple annotations.
	/// A few standard attribute names are usually used:
	/// - "Name": a QString object containing the data name. It is usually inherited from the VipProcessecingObject which generates the data.
	/// - "XUnit": a QString object containing the data x unit (if any)
	/// - "YUnit": a QString object containing the data y unit (if any)
	/// - "ZUnit": a QString object containing the data z unit (if any). For image data, the ZUnit will be used for the color scale title.
	/// - "stylesheet": a VipPlotItem stylesheet applied through the leaf VipDisplayObject
	///
	void setAttributes(const QVariantMap& attrs) { m_attributes = attrs; }
	void setAttribute(const QString& name, const QVariant& value) { m_attributes[name] = value; }
	const QVariantMap& attributes() const { return m_attributes; }
	QVariant attribute(const QString& attr) const { return m_attributes[attr]; }
	bool hasAttribute(const QString& attr) const { return m_attributes.find(attr) != m_attributes.end(); }
	QStringList mergeAttributes(const QVariantMap& attrs);

	void setName(const QString& name) { setAttribute("Name", name); }
	void setXUnit(const QString& unit) { setAttribute("XUnit", unit); }
	void setYUnit(const QString& unit) { setAttribute("YUnit", unit); }
	void setZUnit(const QString& unit) { setAttribute("ZUnit", unit); }

	QString name() const { return m_attributes["Name"].toString(); }
	QString xUnit() const { return m_attributes["XUnit"].toString(); }
	QString yUnit() const { return m_attributes["YUnit"].toString(); }
	QString zUnit() const { return m_attributes["ZUnit"].toString(); }

	void setData(const QVariant& data) { d_data = data; }
	void setData(QVariant&& data) { 
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
#endif
		d_data = std::move(data); 
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	}
	const QVariant& data() const { return d_data; }

	void setTime(qint64 time) { m_time = time; }
	qint64 time() const { return m_time; }

	/// Set the source, that is the #VipProcessingObject that creates this VipAnyData.
	/// By default the source is zero. It is usually set to the VipProcessingObject address.
	void setSource(qint64 source) { m_source = source; }
	template<class T>
	void setSource(const T* source)
	{
		m_source = reinterpret_cast<std::uintptr_t>(source);
	}
	qint64 source() const { return m_source; }

	bool isEmpty() const { return d_data.userType() == 0; }
	bool isValid() const { return !isEmpty(); }
	template<class T>
	T value() const
	{
		return d_data.value<T>();
	}

	/// Returns an approximation of the memory footprint for this object.
	int memoryFootprint() const;
};

typedef QList<VipAnyData> VipAnyDataList;

Q_DECLARE_METATYPE(VipAnyData)
Q_DECLARE_METATYPE(VipAnyDataList)
Q_DECLARE_METATYPE(QVariantMap)

/// @brief Serialize a VipAnyData into a VipArchive.
/// These operators react to the VipArchive property 'skip_data' (true or false) which tells if the variant data of the VipAnyData should be read/write or not.
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive&, const VipAnyData& any);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive&, VipAnyData& any);

VIP_CORE_EXPORT QDataStream& operator<<(QDataStream&, const VipAnyData& any);
VIP_CORE_EXPORT QDataStream& operator>>(QDataStream&, VipAnyData& any);

/// @brief Base class for objects providing an error handling system.
///
/// It provides functions to set the current error, reset it, check for error and emit a signal whenever an error is set.
/// VipErrorHandler is thread safe, which is required for VipProcessingObject.
///
class VIP_CORE_EXPORT VipErrorHandler : public QObject
{
	Q_OBJECT

public:
	VipErrorHandler(QObject* parent = nullptr);
	~VipErrorHandler();
	/// @brief Set the current error status.
	/// The error will be redirected to the parent VipProcessingObject.
	void setError(const QString& error, int code = -1);
	void setError(const VipErrorData&);
	void setError(VipErrorData&& err);
	/// @brief Resets the current error status.
	void resetError();
	/// @brief Return the last error.
	VipErrorData error() const;
	/// @brief Returns the last error string
	QString errorString() const;
	/// @brief Returns the last error code, ot 0 if no error occured.
	int errorCode() const;
	/// @brief Returns true if an error occurred during the last operation.
	bool hasError() const;

protected:
	virtual void newError(const VipErrorData&) {}

public Q_SLOTS:
	virtual void emitError(QObject*, const VipErrorData&);

Q_SIGNALS:
	/// This signal is emitted by the function setError()
	void error(QObject*, const VipErrorData&);

private:
	std::atomic<VipErrorData*> d_data;
};

class VipDataList;
class VipProcessingObject;

typedef QMap<QString, QThread::Priority> PriorityMap;
Q_DECLARE_METATYPE(PriorityMap)

/// VipProcessingManager manages the default configuration of all new VipDataList instances and VipProcessingObject instances.
/// When a VipDataList is created, its data limit type, max list size and max memory size are set respectively to
/// VipProcessingManager::listLimitType(), VipProcessingManager::maxListSize() and VipProcessingManager::maxListMemory().
/// By default, the limit type is set to VipDataList::MemorySize, the max list size is set to MAX_INT and the maximum memory size is set to 50MB.
///
/// You can change these parameters for all VipDataList instances using VipProcessingManager::setListLimitType(), VipProcessingManager::setMaxListSize()
/// and VipProcessingManager::setMaxListMemory().
class VIP_CORE_EXPORT VipProcessingManager : public QObject
{
	Q_OBJECT
	friend class VipDataList;
	friend class VipProcessingObject;
	friend void serialize_VipDataListManager(VipArchive& arch);

public:
	~VipProcessingManager();

	static VipProcessingManager& instance();

	static void setDefaultPriority(QThread::Priority priority, const QMetaObject* meta);
	static int defaultPriority(const QMetaObject* meta);
	static void setDefaultPriorities(const PriorityMap&);
	static PriorityMap defaultPriorities();

	/// Set the list limit type for all existing and future VipDataList instances
	static void setListLimitType(int type);
	/// Set the list maximum size for all existing and future VipDataList instances
	static void setMaxListSize(int size);
	/// Set the list maximum memory footprint for all existing and future VipDataList instances
	static void setMaxListMemory(int size);

	/// Returns the current default list limit type
	static int listLimitType();
	/// Returns the current default maximum list size
	static int maxListSize();
	/// Returns the current default maximum memory footprint
	static int maxListMemory();

	/// Error management for all processings
	static void setLogErrorEnabled(int error_code, bool enable);
	/// Error management for all processings
	static bool isLogErrorEnabled(int error);
	/// Error management for all processings
	static void setLogErrors(const QSet<int>& errors);
	/// Error management for all processings
	static QSet<int> logErrors();

	/// Lock the VipProcessingManager settings so that it cannot be loaded from a session file. The parameters can still be modified through the
	/// related functions. This is usefull when setting the parameters in a plugin and you don't want them to be overwritten when loading the last session.
	static void setLocked(bool locked);

	/// Returns all instances of VipDataList
	static QList<VipDataList*> dataListInstances();

	/// Returns all instances of VipProcessingObject
	static QList<VipProcessingObject*> processingObjectInstances();

Q_SIGNALS:
	void changed();

private:
	static void applyAll();
	VipProcessingManager();
	void add(VipDataList*);
	void remove(VipDataList*);
	void add(VipProcessingObject*);
	void remove(VipProcessingObject*);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief VipDataList is an abstract class representing a list of VipAnyData going to the input of a VipProcessingObject.
///
/// When a VipProcessingObject is Asynchronous, incoming input data are stored in a VipDataList before being processed by the VipProcessingObject::apply() function.
/// VipDataList defines functions to control which data has to be sent first to the VipProcessingObject, and to check if new data are available or not.
///
class VIP_CORE_EXPORT VipDataList
{
	int m_max_size;
	int m_max_memory;
	int m_data_limit_type;

public:
	/// The type of VipDataList
	enum Type
	{
		NULLType,     //! nullptr list
		FIFO,	      //! A First In First Out list
		LIFO,	      //! A Last In First Out list
		LastAvailable //! Always returns the last data
	};

	/// This enum describes which kind of unit is used to determine the input list maximum size.
	/// This is currently only used by VipFIFOList and VipLIFOList
	enum DataLimitType
	{
		None = 0,	  //! The list does not have a maximum size
		Number = 0x01,	  //! The list maximum size is determine by the number of data in it
		MemorySize = 0x02 //! The list maximum size is determine by the total memory footprint of the data in it
	};

	VipDataList();
	virtual ~VipDataList();
	/// Add a new data to the list, returns the new number of data in the list
	virtual int push(const VipAnyData&, int* previous = nullptr) = 0;
	virtual int push(VipAnyData&&, int* previous = nullptr) = 0;
	/// Clear the list content and add a new input
	virtual void reset(const VipAnyData&) = 0;
	virtual void reset(VipAnyData&&) = 0;
	/// Returns the next data from the list. This function must remove this data from the list.
	/// This function should ALWAYS return the next data, even if there no new data available.
	virtual VipAnyData next() = 0;
	/// Returns the next data from the list, without removing it from the list.
	virtual VipAnyData probe() = 0;
	/// Returns all next data, and remove them from the list.
	/// This function returns the next data in the order they should be read, from list beginning to list end.
	virtual VipAnyDataList allNext() = 0;
	/// Return the next data time
	virtual qint64 time() const = 0;
	/// Returns true if the list is empty, false otherwise.
	/// The list is only empty at the very beginning, until the first data is pushed.
	virtual bool empty() const = 0;
	/// Retruns true if a new data is available
	virtual bool hasNewData() const = 0;
	/// @brief Returns the list size if the VipDataList is not empty, -1 otherwise
	virtual int status() const = 0;
	/// Returns the list type
	virtual VipDataList::Type listType() const = 0;
	/// Retruns the number of remaining data
	virtual int remaining() const = 0;
	/// Remove all new data
	virtual void clear() = 0;

	/// Returns the memory footprint of the data list
	virtual int memoryFootprint() const = 0;

	/// Set the maximum list size
	void setMaxListSize(int size) { m_max_size = size; }
	/// Set the maximum list memory footprint
	void setMaxListMemory(int memory) { m_max_memory = memory; }

	/// Set the list limit type (combination of Number and MemorySize, or None)
	void setListLimitType(int type) { m_data_limit_type = type; }
	/// Returns the list limit type
	int listLimitType() const { return m_data_limit_type; }

	/// Returns the maximum list size
	int maxListSize() const { return m_max_size; }
	/// Returns the maximum list memory footprint in bytes
	int maxListMemory() const { return m_max_memory; }
};

/// @brief A FIFO, thread safe VipDataList
class VIP_CORE_EXPORT VipFIFOList : public VipDataList
{
	// std::deque is more performant (even on msvc!) for fifo structure than QList
	std::deque<VipAnyData> m_list;
	VipAnyData m_last;
	VipSpinlock m_mutex;

public:
	VipFIFOList();
	virtual int push(const VipAnyData&, int* previous = nullptr);
	virtual int push(VipAnyData&&, int* previous = nullptr);
	virtual void reset(const VipAnyData&);
	virtual void reset(VipAnyData&&);
	virtual VipAnyData next();
	virtual VipAnyDataList allNext();
	virtual VipAnyData probe();
	virtual qint64 time() const;
	virtual bool empty() const;
	virtual bool hasNewData() const;
	virtual int status() const;
	virtual VipDataList::Type listType() const { return FIFO; }
	virtual int remaining() const;
	virtual int memoryFootprint() const;
	virtual void clear();
};

/// @brief A LIFO, thread safe VipDataList
class VIP_CORE_EXPORT VipLIFOList : public VipDataList
{
	std::deque<VipAnyData> m_list;
	VipAnyData m_last;
	VipSpinlock m_mutex;

public:
	VipLIFOList();
	virtual int push(const VipAnyData&, int* previous = nullptr);
	virtual int push(VipAnyData&&, int* previous = nullptr);
	virtual void reset(const VipAnyData&);
	virtual void reset(VipAnyData&&);
	virtual VipAnyData next();
	virtual VipAnyDataList allNext();
	virtual VipAnyData probe();
	virtual qint64 time() const;
	virtual bool empty() const;
	virtual bool hasNewData() const;
	virtual int status() const;
	virtual VipDataList::Type listType() const { return LIFO; }
	virtual int remaining() const { return (int)m_list.size(); }
	virtual int memoryFootprint() const;
	virtual void clear();
};

/// @brief A thread safe VipDataList that only stores and returns the last incoming data
class VIP_CORE_EXPORT VipLastAvailableList : public VipDataList
{
	VipAnyData d_data;
	bool m_has_new_data;
	VipSpinlock m_mutex;

public:
	VipLastAvailableList();
	virtual int push(const VipAnyData&, int* previous = nullptr);
	virtual int push(VipAnyData&&, int* previous = nullptr);
	virtual void reset(const VipAnyData&);
	virtual void reset(VipAnyData&&);
	virtual VipAnyData next();
	virtual VipAnyDataList allNext();
	virtual VipAnyData probe();
	virtual qint64 time() const;
	virtual bool empty() const;
	virtual bool hasNewData() const;
	virtual int status() const;
	virtual VipDataList::Type listType() const { return LastAvailable; }
	virtual int remaining() const { return hasNewData() ? 1 : 0; }
	virtual int memoryFootprint() const;
	virtual void clear();
};

// Forward declarations
class UniqueProcessingIO;
class VipInput;
class VipMultiInput;
class VipProperty;
class VipMultiProperty;
class VipOutput;
class VipMultiOutput;
class VipProcessingIO;
class VipConnection;
template<class TYPE>
class VipMultipleProcessingIO;
class VipProcessingObject;
class VipIODevice;
class VipProcessingPool;

// Aliases

using VipConnectionPtr = QSharedPointer<VipConnection>;
using VipConnectionVector = QVector<VipConnectionPtr>;
using VipIODeviceList = QList<VipIODevice*>;

/// @brief Manage a connection between 2 VipProcessingIO objects
///
/// It could be a direct connection (sending a data through VipConnection::sendData() that directly calls VipConnection::receiveData()
/// of the destination connection) or any kind of network/shared memory based connection.
///
/// VipConnection is the core feature that enables VipProcessingObject instances to exchange data. The default behavior is the direct connection,
/// which directly send VipAnyData instances from one VipConnection to another. It can be reimplemented to provide new ways of exchangind data,
/// like through TCP or D-Bus protocols. Any additional VipConnection inheriting classes should be declared using VIP_REGISTER_QOBJECT_METATYPE or VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE.
///
/// VipConnection objects should always be manipulated through shared pointer.
///
class VIP_CORE_EXPORT VipConnection
  : public VipErrorHandler
  , public QEnableSharedFromThis<VipConnection>
{
	Q_OBJECT

	friend class UniqueProcessingIO;

public:
	/// The connection type.
	enum IOType
	{
		UnknownConnection = 0,	//! Unknown connection type (i.e not open)
		InputConnection = 0x01, //! VipInput connection type, data are received through #VipConnection::receiveData().
		OutputConnection = 0x02 //! VipOutput connection type, data are sent through #VipConnection::sendData().
	};

	VipConnection();
	virtual ~VipConnection();

	/// @brief Prepare a new VipInput or VipOutput connection based on its address.
	///
	/// The connection address might contains the prefix 'class_name:', use VipConnection::removeClassNamePrefix()
	/// to remove it.
	void setupConnection(const QString& address, const VipConnectionPtr& con = VipConnectionPtr());
	/// @brief For VipOutput connections only, returns its address.
	QString address() const;

	/// @brief Open the connection
	///
	/// This should be called after setupConnection().
	/// For VipOutput connection, this should set the connection to a listening state.
	/// For VipInput connection, this establish the concrete connection to a VipOutput object.
	/// This will internally call VipConnection::doOpenConnection().
	bool openConnection(IOType type);

	/// @brief Send a data (only for VipOutput connection).
	/// This will internally call VipConnection::doSendData().
	bool sendData(const VipAnyData& data);

	/// @brief Clear the connection.
	///
	/// This will internally call VipConnection::doClearConnection().
	void clearConnection();

	/// @brief Returns the current open mode, or 0 if the connection is not opened.
	IOType openMode();

	/// Returns the parent VipProcessingObject
	VipProcessingObject* parentProcessingObject() const;

	/// Returns the parent VipProcessingIO (VipInput or VipOutput)
	VipProcessingIO* parentProcessingIO() const;

	/// @brief Returns the source of this connection if this is an input connection
	VipOutput* source() const;

	/// @brief Returns the VipInput sinks of this connection if this is an output connection
	QList<VipInput*> sinks() const;
	/// @brief Returns the all sinks of this connection (VipInput and VipProperty) if this is an output connection
	QList<UniqueProcessingIO*> allSinks() const;

	/// @brief Returns the supported connection types for this connection.
	virtual int supportedModes() const { return InputConnection | OutputConnection; }

	/// Create a new VipConnection object.
	static VipConnectionPtr newConnection();
	/// Create and setup a new VipConnection object (or inheriting VipConnection) based on the connection type, destination address and optional VipConnection object.
	///
	/// If the destination VipConnection is provided, a direct connection will be established.
	///
	/// If the address starts with the pattern 'class_name:', this function will try to create a connection of the given type and call #VipConnection::setConnection().  If the connection setup
	/// fails, an empty connection is returned.
	///
	/// If the prefix 'class_name:' is not found, this function will try every VipConnection inheriting classes until one of them returns true on #VipConnection::setConnection().
	///
	/// This does not open the connection, but only prepare it.
	static VipConnectionPtr buildConnection(IOType mode, const QString& address, const VipConnectionPtr& con = VipConnectionPtr());

public Q_SLOTS:

	/// This function must be called when receiving a new data.
	/// It calls #VipProcessingIO::setData() on parent VipProcessingIO.
	void receiveData(const VipAnyData& data);

	/// The connection address usually contains the parent processing pool name.
	/// This function removes it to make the connection independant from the processing pool.
	virtual void removeProcessingPoolFromAddress();

protected:
	/// Set the internal open mode.
	/// Should be called by #VipConnection::doOpenConnection().
	void setOpenMode(IOType);

	/// If given string contains the prefix 'class_name:', this function removes it.
	QString removeClassNamePrefix(const QString& addr) const;

	/// Open the connection. Called by #VipConnection::openConnection().
	/// Default behavior open a direct connection.
	virtual void doOpenConnection(IOType type);
	/// Send a data. Called by #VipConnection::sendData().
	/// Default behavior directly send the data to destination VipConnection objects.
	virtual void doSendData(const VipAnyData& data);
	/// Reset the connection.
	virtual void doClearConnection();
	/// @brief Set the parent VipProcessingObject and corresponding VipProcessingIO (either a VipInput or VipOutput).
	/// Should not be called directly.
	void setParentProcessingObject(VipProcessingObject*, VipProcessingIO* io);

protected Q_SLOTS:

	/// This function should be called whenever a connection have been closed. Its only purpose is to reset the open mode
	/// when all connection have been closed by calling setOpenMode(UnknownConnection). You should reimplement it when
	/// creating a new type of connection.
	virtual void checkClosedConnections();

Q_SIGNALS:

	/// Emitted when a connection is successfully opened
	void connectionOpened(VipProcessingIO* io, int type, const QString& address);
	void connectionClosed(VipProcessingIO* io);
	/// Emitted when a new data is received
	void dataReceived(VipProcessingIO* io, const VipAnyData& data);
	/// Emitted when a data is sent
	void dataSent(VipProcessingIO* io, const VipAnyData& data);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipConnection*)

/// @brief Abstract base class for inputs, outputs and properties of VipProcessingObject.
///
/// A VipProcessingIO always has a unique name within a VipProcessingObject (see VipProcessingIO::name()).
/// The parent VipProcessingObject can be queried through VipProcessingIO::parentProcessing().
///
/// You should no care about creating yourself VipProcessingIO instances and setting their names or parents,
/// this is automatically handled by the parent VipProcessingObject.
///
/// A VipProcessingIO object could be either a VipInput, VipMultiInput, VipOutput, VipMultiOutput, VipProperty or VipMultiProperty.
///
/// VipProcessingIO uses shared ownership.
///
class VIP_CORE_EXPORT VipProcessingIO
{
public:
	/// The processing I/O type
	enum Type
	{
		TypeInput,	   //! This processing I/O is a #VipInput
		TypeMultiInput,	   //! This processing I/O is a #VipMultiInput
		TypeProperty,	   //! This processing I/O is a #VipProperty
		TypeMultiProperty, //! This processing I/O is a #VipMultiProperty
		TypeOutput,	   //! This processing I/O is a #VipOutput
		TypeMultiOutput	   //! This processing I/O is a #VipMultiOutput
	};
	VipProcessingIO(Type t, const QString& name);
	VipProcessingIO(const VipProcessingIO& other);
	virtual ~VipProcessingIO();

	/// @brief Convert this VipProcessingIO to an Input if this is a VipInput instance, returns nullptr otherwise.
	VipInput* toInput() const;
	/// @brief Convert this VipProcessingIO to an Input if this is a VipMultiInput instance, returns nullptr otherwise.
	VipMultiInput* toMultiInput() const;
	/// @brief Convert this VipProcessingIO to an Input if this is an VipProperty instance, returns nullptr otherwise.
	VipProperty* toProperty() const;
	/// @brief Convert this VipProcessingIO to an Input if this is an VipMultiProperty instance, returns nullptr otherwise.
	VipMultiProperty* toMultiProperty() const;
	/// @brief Convert this VipProcessingIO to an Input if this is an VipOutput instance, returns nullptr otherwise.
	VipOutput* toOutput() const;
	/// @brief Convert this VipProcessingIO to an Input if this is an VipMultiOutput instance, returns nullptr otherwise.
	VipMultiOutput* toMultiOutput() const;
	/// @brief Convert this VipProcessingIO to TYPE if possible, returns nullptr otherwise.
	template<class TYPE>
	TYPE* to() const
	{
		return nullptr;
	}

	/// @brief Returns the processing I/O type
	Type type() const;
	/// @brief Returns the processing I/O name
	QString name() const;
	/// @brief Returns the parent VipProcessingObject
	VipProcessingObject* parentProcessing() const;

	/// @brief Set the processing I/O data.
	///
	/// The behavior is different according to the VipProcessingIO type:
	/// - Input: add the data to the internal VipDataList. If the parent VipProcessingObject is Asynchronous, this will call VipProcessingObject::update() and, depending on its schedule
	/// strategy, it might trigger the processing.
	/// - Output: set the parent VipProcessingObject output. This is usually called from within VipProcessingObject::apply(). If the output is connected to another VipProcessingObject's input, set
	/// the input's data.
	/// - Property: set the property data. This will never trigger the parent  VipProcessingObject.
	virtual void setData(const VipAnyData&) = 0;
	virtual void setData(VipAnyData&& any) { setData(static_cast<const VipAnyData&>(any)); }
	void setData(VipAnyData& any) { setData(static_cast<const VipAnyData&>(any)); }
	template<class T>
	void setData(T&& data)
	{
		setData(VipAnyData(QVariant::fromValue(std::forward<T>(data))));
	}

	void setData(const QVariant& data, qint64 time);
	void setData(QVariant&& data, qint64 time);
	/// @brief Enable or disable the processing I/O.
	/// A disable processing I/O does not accept data and cannot trigger a VipProcessingObject
	void setEnabled(bool enable);
	bool isEnabled() const;

	// Set the processing I/O name. You should not call this function yourself.
	virtual void setName(const QString& name);

	/// @brief Reset the connection
	virtual void clearConnection() = 0;

	/// @brief Returns the processing I/O data
	virtual VipAnyData data() const = 0;

	/// @brief Set the parent VipProcessingObject. You should not call this function yourself.
	virtual void setParentProcessing(VipProcessingObject* parent);

	bool operator==(const VipProcessingIO& other) { return d_data == other.d_data; }
	bool operator!=(const VipProcessingIO& other) { return d_data != other.d_data; }

protected:
	void dirtyParentProcessingIO(VipProcessingIO* io);

private:
	class PrivateData;
	QSharedPointer<PrivateData> d_data;
};

/// @brief A VipProcessingIO with a VipConnectionPtr.
///
/// UniqueProcessingIO is the base class for VipInput, VipOutput and VipProperty.
///
/// By default, the internal VipConnectionPtr is a direct connection.
///
class VIP_CORE_EXPORT UniqueProcessingIO : public VipProcessingIO
{
	template<class T>
	friend class VipMultipleProcessingIO;
	friend class VipProcessingObject;

public:
	UniqueProcessingIO(Type t, const QString& name);
	UniqueProcessingIO(const UniqueProcessingIO& other);
	virtual ~UniqueProcessingIO();
	void setConnection(const VipConnectionPtr& con);
	bool setConnection(UniqueProcessingIO* dst);
	bool setConnection(const QString& address, const VipConnectionPtr& con = VipConnectionPtr());
	VipConnectionPtr connection() const;
	VipOutput* source() const;
	virtual void clearConnection();
	virtual void setParentProcessing(VipProcessingObject* parent);

private:
	class PrivateData;
	QSharedPointer<PrivateData> d_data;
};

/// @brief A VipProcessingIO that is a container for any number of VipProcessingIO pointers.
///
template<class TYPE>
class VipMultipleProcessingIO : public VipProcessingIO
{
public:
	typedef QVector<TYPE*> VectorType;

	VipMultipleProcessingIO(Type t, const QString& name)
	  : VipProcessingIO(t, name)
	  , d_data(new PrivateData())
	{
	}
	VipMultipleProcessingIO(const VipMultipleProcessingIO& other)
	  : VipProcessingIO(other)
	  , d_data(other.d_data)
	{
	}
	~VipMultipleProcessingIO() {}

	/// @brief Returns the vector of VipProcessingIO pointers
	const VectorType& vector() const { return d_data->vector; }
	/// @brief Returns the number of VipProcessingIO this VipMultipleProcessingIO conatins
	int count() const { return d_data->vector.size(); }

	/// @brief Set the maximum number of VipProcessingIO this object can contain
	void setMaxSize(int size)
	{
		d_data->max_size = size;
		if (count() > size)
			resize(size);
	}
	/// @brief Returns the maximum number of VipProcessingIO this object can contain
	int maxSize() const { return d_data->max_size; }

	/// @brief Set the minimum number of VipProcessingIO this object can contain
	void setMinSize(int size)
	{
		if (size < 0)
			size = 0;
		d_data->min_size = size;
		if (count() < size)
			resize(size);
	}
	/// @brief Returns the minimum number of VipProcessingIO this object can contain
	int minSize() const { return d_data->min_size; }

	/// @brief Insert a VipProcessingIO at given position
	/// @param pos insert position
	/// @param val VipProcessingIO object which is copied
	/// @return true on success, false otherwise
	bool insert(int pos, const TYPE& val)
	{
		if (count() >= d_data->max_size)
			return false;
		TYPE* t = new TYPE(val);
		t->setParentProcessing(parentProcessing());
		d_data->vector.insert(pos, t);
		added(d_data->vector.back());
		dirtyParentProcessingIO(this);
		return true;
	}
	/// @brief Insert a VipProcessingIO at given position
	/// @param pos insert position
	/// @return true on success, false otherwise
	bool insert(int pos) { return insert(pos, TYPE(name())); }

	/// @brief Set the processing IO at given position.
	/// Resize the internal IO vector if needed.
	/// Return true on success, false otherwise.
	bool setAt(int pos, const TYPE& val)
	{
		if (pos >= d_data->max_size || pos < 0)
			return false;
		if (pos >= count())
			resize(pos + 1);
		*d_data->vector[pos] = val;
		added(d_data->vector[pos]);
		dirtyParentProcessingIO(this);
		return true;
	}

	/// @brief Add a VipProcessingIO
	bool add(const TYPE& val) { return insert(d_data->vector.size(), val); }
	/// @brief Add a new VipProcessingIO with given name
	bool add(const QString& name) { return add(TYPE(name)); }
	/// Add a new VipProcessingIO that inherits this VipMultipleProcessingIO name
	bool add() { return add(TYPE(name())); }
	/// Resize this VipMultipleProcessingIO
	bool resize(int size)
	{
		if (size < d_data->min_size || size > d_data->max_size)
			return false;
		if (size < 0)
			size = 0;
		if (size < count()) {
			for (int i = size; i < count(); ++i)
				delete d_data->vector[i];
			d_data->vector.resize(size);
			dirtyParentProcessingIO(this);
		}
		else if (size > count()) {
			for (int i = count(); i < size; ++i)
				add();
			dirtyParentProcessingIO(this);
		}
		return true;
	}
	/// Remove the VipProcessingIO at given index
	bool removeAt(int index)
	{
		if (count() <= d_data->min_size)
			return false;
		delete d_data->vector[index];
		d_data->vector.removeAt(index);
		dirtyParentProcessingIO(this);
		return true;
	}
	/// Clear the VipMultipleProcessingIO
	bool clear()
	{
		if (d_data->min_size > 0)
			return false;
		while (d_data->vector.size()) {
			d_data->vector[0]->clearConnection();
			delete d_data->vector[0];
			d_data->vector.removeAt(0);
		}
		dirtyParentProcessingIO(this);
		return true;
	}
	/// Returns the index of the VipProcessingIO with given name
	int indexOf(const QString& name) const
	{
		for (int i = 0; i < d_data->vector.size(); ++i)
			if (d_data->vector[i]->name() == name)
				return i;
		return -1;
	}
	/// Returns the VipProcessingIO at given index
	TYPE* at(int index) { return d_data->vector[index]; }
	/// Returns the VipProcessingIO at given index
	const TYPE* at(int index) const { return d_data->vector[index]; }
	/// Reimplemented from #VipProcessingIO::data().
	/// Returns the the data of the last VipProcessingIO.
	virtual VipAnyData data() const { return d_data->vector.back()->data(); }
	/// Reimplemented from #VipProcessingIO::setData().
	/// Set the data of the last VipProcessingIO.
	virtual void setData(const VipAnyData& d) { d_data->vector.back()->setData(d); }
	/// Clear all VipProcessingIO connections
	virtual void clearConnection()
	{
		for (int i = 0; i < d_data->vector.size(); ++i)
			d_data->vector[i]->clearConnection();
	}
	/// Set the parent VipProcessingObject. You should not call this function yourself.
	virtual void setParentProcessing(VipProcessingObject* parent)
	{
		VipProcessingIO::setParentProcessing(parent);
		for (int i = 0; i < d_data->vector.size(); ++i)
			d_data->vector[i]->setParentProcessing(parent);
		dirtyParentProcessingIO(this);
	}

protected:
	virtual void added(TYPE*) {}

private:
	class PrivateData
	{
	public:
		PrivateData()
		  : min_size(0)
		  , max_size(INT_MAX)
		{
		}
		~PrivateData()
		{
			for (int i = 0; i < vector.size(); ++i) {
				vector[i]->clearConnection();
				delete vector[i];
			}
		}
		VectorType vector;
		int min_size;
		int max_size;
	};
	QSharedPointer<PrivateData> d_data;
};

/// A #VipProcessingObject input.
/// To declare an input to a VipProcessingObject, use the Q_PROPERTY macro. Example:
/// \code
/// class MyImageProcessing : public VipProcessingObject
/// {
///	Q_OBJECT
///	Q_PROPERTY(VipInput image_input) //declare an input called 'image_input'
///	Q_PROPERTY(VipOutput image_result) //declare an output called 'image_result'
///	//
///	//...
/// };
/// \endcode
///
/// VipInput internally stores the input data in a VipDataList. Each call to #VipInput::data() (usually from within the parent VipProcessingObject)
/// will return the next available input data (and remove it from the VipDataList)
///
/// To retrieve a VipInput from its parent VipProcessingObject, use VipProcessingObject::inputAt() or  VipProcessingObject::inputName().
///
class VIP_CORE_EXPORT VipInput : public UniqueProcessingIO
{
	QSharedPointer<VipDataList> m_input_list;

public:
	VipInput(const QString& name = QString(), VipProcessingObject* parent = nullptr);
	VipInput(const VipInput& other);

	/// Call VipDataList::probe()
	VipAnyData probe() const;
	/// Reimplemented from #VipProcessingIO::data(). Call #VipDataList::next()
	virtual VipAnyData data() const;
	VipAnyDataList allData() const;
	/// Reimplemented from #VipProcessingIO::setData(). Add a new data to the internal VipDataList.
	virtual void setData(const VipAnyData&);
	virtual void setData(VipAnyData&&);
	using VipProcessingIO::setData;

	/// Change the internal VipDataList type
	void setListType(VipDataList::Type, int list_limit_type = VipDataList::None, int max_list_size = INT_MAX, int max_memory_size = INT_MAX);
	/// Returns the internal VipDataList type
	VipDataList::Type listType() const { return m_input_list->listType(); }
	/// Returns the internal VipDataList
	QSharedPointer<VipDataList> buffer() const { return m_input_list; }
	/// Returns the list maximum size
	int maxListSize() const { return m_input_list->maxListSize(); }
	/// Returns the list maximum memory size
	int maxListMemory() const { return m_input_list->maxListMemory(); }
	/// Returns the list limit type
	int listLimitType() const { return m_input_list->listLimitType(); }
	/// Retruns true if the VipInput has a new data available, false otherwise
	bool hasNewData() const { return m_input_list->hasNewData(); }
	/// See VipDataList::status()
	int status() const { return m_input_list->status(); }
	/// Returns true if the VipDataList is empty
	bool empty() const { return m_input_list->empty(); }
	/// Returns the time of the last input data
	qint64 time() const { return m_input_list->time(); }
};

/// \a VipMultiInput is container for multiple #VipInput.
/// To declare multi  input to a VipProcessingObject, use the Q_PROPERTY macro. Example:
/// \code
/// class MultiplyInputs : public VipProcessingObject
/// {
///	Q_OBJECT
///	Q_PROPERTY(VipMultiInput inputs) //declare a multi input called 'inputs'
///	Q_PROPERTY(VipOutput output) //declare an output called 'output'
///	//
///	//...
///
/// public:
///	MultiplyInputs(QObject * parent = nullptr)
///	:VipProcessingObject(parent)
///	{
///		//add 3 inputs
///		topLevelInputAt(0)->toMultiInput()->resize(3);
///	}
///
/// protected:
///	virtual void apply()
///	{
///		//compute the output by multiplying all inputs
///		double result = 1;
///		for(int i=0; i < inputCount(); ++i)
///		{
///			result *= inputAt(i)->data().value<double>();
///		}
///		//set the output value
///		outputAt(0)->setData(create(result));
///	}
/// };
/// \endcode
///
/// By default a VipMultiInput is empty. use VipMultiInput::add or VipMultiInput::resize to add VipInput.
///
class VIP_CORE_EXPORT VipMultiInput : public VipMultipleProcessingIO<VipInput>
{
public:
	VipMultiInput(const QString& name = QString(), VipProcessingObject* parent = nullptr);
	VipMultiInput(const VipMultiInput& other);
	/// Set the list type for all VipInput
	void setListType(VipDataList::Type, int list_limit_type = VipDataList::None, int max_list_size = INT_MAX, int max_memory_size = INT_MAX);

protected:
	virtual void added(VipInput* input);

private:
	VipDataList::Type m_type;
	int m_list_limit_type;
	int m_max_list_size;
	int m_max_list_memory;
};

/// \a VipOutput represents an output of a VipProcessingObject.
/// See VipMultiInput for an example code.
///
class VIP_CORE_EXPORT VipOutput : public UniqueProcessingIO
{
	QSharedPointer<VipAnyData> d_data;
	QList<VipAnyData> m_buffer;
	VipSpinlock m_buffer_lock;
	bool m_bufferize_outputs;

public:
	VipOutput(const QString& name = QString(), VipProcessingObject* parent = nullptr);
	VipOutput(const VipOutput& other);

	VipOutput& operator=(const VipOutput&);

	/// Reimplemented from #VipProcessingIO::data. Returns the output data
	virtual VipAnyData data() const;
	/// Reimplemented from #VipProcessingIO::setData. Set the output data, and set the data to all connected inputs.
	virtual void setData(const VipAnyData&);
	using VipProcessingIO::setData;

	/// Enable/disable data buffering (disabled by default)
	/// If enabled, calling VipOutput::setData() will buffer the VipAnyData
	/// in a internal list.
	/// Use clearBufferedData() to retrieved buffered data and clear the internal list.
	void setBufferDataEnabled(bool);
	bool bufferDataEnabled() const;

	/// Returns the size of internal list of data
	int bufferDataSize();

	/// Returns buffered data and clear the internal list of buffered data.
	/// This function is thread safe.
	QList<VipAnyData> clearBufferedData();

	// shortcut functions
	qint64 time() const { return data().time(); }
	template<class T>
	T value() const
	{
		return data().value<T>();
	}
};

/// \a VipMultiOutput is container for multiple #VipOutput.
/// To declare multi  output to a VipProcessingObject, use the Q_PROPERTY macro. Example:
/// \code
/// class IncrementInput : public VipProcessingObject
/// {
///	Q_OBJECT
///	Q_PROPERTY(VipInput input_value) //declare an input called 'input_value'
///	Q_PROPERTY(VipMultiOutput outputs) //declare a multi output called 'outputs'
///	//
///	//...
///
/// public:
///	IncrementInput(QObject * parent = nullptr)
///	:VipProcessingObject(parent)
///	{
///		//add 3 outputs
///		topLevelInputAt(0)->toMultiInput()->resize(3);
///	}
///
/// protected:
///	virtual void apply()
///	{
///		//compute the output by multiplying all inputs
///		int value = inputAt(0)->data().value<int>();
///
///		//set the outputs
///		for(int i=0; i < outputCount(); ++i)
///		{
///		outputAt(i)->setData(create(value + 1 + i));
///		}
///	}
/// };
/// \endcode
///
/// By default a VipMultiOutput is empty. use #VipMultiOutput::add or #VipMultiOutput::resize to add VipOutput.
class VIP_CORE_EXPORT VipMultiOutput : public VipMultipleProcessingIO<VipOutput>
{
public:
	VipMultiOutput(const QString& name = QString(), VipProcessingObject* parent = nullptr);
	VipMultiOutput(const VipMultiOutput& other);
};

/// \a VipProperty represents a VipProcessingObject property.
/// A property is equivalent to an input, except that setting the property it will never trigger the parent VipProcessingObject.
/// Furthermore, it does not store its data as a VipDataList but as a signle VipAnyData. Querying the data with #VipProperty::data() will always return the last available one.
/// It can be connected to a #VipOutput like a #VipInput.
class VIP_CORE_EXPORT VipProperty : public UniqueProcessingIO
{
	QSharedPointer<VipAnyData> d_data;
	VipSpinlock m_lock;

public:
	VipProperty(const QString& name = QString(), VipProcessingObject* parent = nullptr);
	VipProperty(const VipProperty& other);
	VipProperty& operator=(const VipProperty&);
	/// Reimplemented from #VipProcessingIO::data. Returns the property data.
	virtual VipAnyData data() const;
	/// Reimplemented from #VipProcessingIO::setData. Set the property data.
	virtual void setData(const VipAnyData&);
	using VipProcessingIO::setData;

	// shortcut functions
	qint64 time() const { return data().time(); }
	template<class T>
	T value() const
	{
		return data().value<T>();
	}
};

/// \a VipMultiProperty is container for multiple VipProperty.
/// Its behavior is close to #VipMultiInput, with the distinctions of VipProperty.
///
class VIP_CORE_EXPORT VipMultiProperty : public VipMultipleProcessingIO<VipProperty>
{
public:
	VipMultiProperty(const QString& name = QString(), VipProcessingObject* parent = nullptr);
	VipMultiProperty(const VipMultiProperty& other);
};

template<>
inline VipInput* VipProcessingIO::to() const
{
	return toInput();
}
template<>
inline VipMultipleProcessingIO<VipInput>* VipProcessingIO::to() const
{
	return toMultiInput();
}
template<>
inline VipMultiInput* VipProcessingIO::to() const
{
	return toMultiInput();
}
template<>
inline VipOutput* VipProcessingIO::to() const
{
	return toOutput();
}
template<>
inline VipMultipleProcessingIO<VipOutput>* VipProcessingIO::to() const
{
	return toMultiOutput();
}
template<>
inline VipMultiOutput* VipProcessingIO::to() const
{
	return toMultiOutput();
}
template<>
inline VipProperty* VipProcessingIO::to() const
{
	return toProperty();
}
template<>
inline VipMultipleProcessingIO<VipProperty>* VipProcessingIO::to() const
{
	return toMultiProperty();
}
template<>
inline VipMultiProperty* VipProcessingIO::to() const
{
	return toMultiProperty();
}

//
// Declare pointers to store processing I/O into QVariant objects if needed
//
Q_DECLARE_METATYPE(VipInput*)
Q_DECLARE_METATYPE(VipMultiInput*)
Q_DECLARE_METATYPE(VipProperty*)
Q_DECLARE_METATYPE(VipMultiProperty*)
Q_DECLARE_METATYPE(VipOutput*)
Q_DECLARE_METATYPE(VipMultiOutput*)

//
// Declare I/O classes as they are used to create processing inputs/outputs/properties
//
Q_DECLARE_METATYPE(VipInput)
Q_DECLARE_METATYPE(VipMultiInput)
Q_DECLARE_METATYPE(VipProperty)
Q_DECLARE_METATYPE(VipMultiProperty)
Q_DECLARE_METATYPE(VipOutput)
Q_DECLARE_METATYPE(VipMultiOutput)

/// @brief VipProcessingObject is the base class for all processing object based on inputs, outputs and properties (basically any kind of processing).
///
/// When subclassing VipProcessingObject, you can declare any number of inputs, outputs or properties (see VipInput, VipMultiInput, VipOutput, VipMultiOutput, VipProperty and VipMultiProperty
/// for more details). There definition is based on the Qt meta object system through the macro Q_PROPERTY (see example code above). You can defines additional informations for a VipProcessingObject
/// (icon, description, category) using the Qt Q_CLASSINFO macro. You can also provide a description for all inputs/outputs/properties using the Q_CLASSINFO macro.
///
/// To retrieve a processing input, use VipProcessingObject::inputAt() or VipProcessingObject::inputName(). To retrieve a property, use VipProcessingObject::propertyAt() or
/// VipProcessingObject::propertyName(). To retrieve an output or set an output data, use  VipProcessingObject::outputAt() or VipProcessingObject::outputName().
///
/// The processing itself is done in the reimplementation of the VipProcessingObject::apply() function. The processing is applied using the function VipProcessingObject::update().
/// The processing will only be applied if at least one or all the inputs are new, depending on the schedule strategy.
///
/// Like VipAnyData, VipProcessingObject can defines any kind/number of attributes (\sa setAttributes(), attributes(), setAttribute(), attribute()).
/// These attributes are always copied to the output values (this is the purpose of the VipProcessingObject::create() member).
///
/// Basic usage:
///
/// \code
///
/// class FactorMultiplication : public VipProcessingObject
/// {
///	Q_OBJECT
///	VIP_IO(VipInput input_value) //define the input of name 'input_value'
///	VIP_IO(VipOutput output_value) //define the output of name 'output_value'
///	VIP_IO(VipProperty factor) //define the property of name 'factor'
///
///	Q_CLASSINFO("description","Multiply an input numerical value by a factor given as property") //define the description of the processing
///	Q_CLASSINFO("icon","path_to_icon") //define an icon to the processing
///	Q_CLASSINFO("category","Miscellaneous/operation") //define a processing category with a slash separator
///
///	Q_CLASSINFO("input_value","The numerical value that will be multiplied")
///	Q_CLASSINFO("factor","The multiplication factor")
///	Q_CLASSINFO("output_value","The result of the multiplication between the input value and the property factor")
///
/// public:
///	FactorMultiplication(QObject * parent = nullptr)
///	:VipProcessingObject(parent)
///	{
///		//set a default multiplication factor of 1
///		this->propertyAt(0)->setData(1.0);
///	}
///
/// protected:
///	virtual void apply()
///	{
///		//compute the output value
///		double value = inputAt(0)->data().value<double>() * propertyAt(0)->data().value<double>();
///		//set the output value
///		outputAt(0)->setData( create(value) );
///	}
/// };
///
/// //use case
/// //create 2 FactorMultiplication processing with a multiplication factor of 2 and 3
/// FactorMultiplication processing1;
/// processing1.propertyAt(0)->setData(2.0);
///
/// FactorMultiplication processing2;
/// processing2.propertyAt(0)->setData(3.0);
///
/// //connect the first processing output to the seconds processing input
/// processing1.outputAt(0)->setConnection(processing2.inputAt(0)
///
/// //set the input value
/// processing1.inputAt(0)->setData(1);
///
/// //Since we are not in Asynchronous mode, the result is not computed until we call the update() function
/// processing2.update();
///
/// //print the result (1*2*3 = 6)
/// std::cout<< processing2.outputAt(0)->data().value<double>() << std::endl;
///
/// \endcode
///
///
/// Inputs, outputs, properties
/// ---------------------------
///
/// A VipProcessingObject can define any number of inputs (VipInput), outputs (VipOutput) and properties (VipProperty),
/// using Qt Q_PROPERTY macro or the VIP_IO one (see example above).
///
/// Each processing input can be connected to another processing output using VipProcessingIO::setConnection() function.
/// An input can be connected to any number of output, but an output can be connected to only one input.
///
/// Processing inputs ar simply set using VipProcessingIO::setData() (see example above). If the processing is asynchronous,
/// this will trigger the processing based on its schedule strategy. If the processing is synchronous, an explicit call
/// to update() is required to apply the processing.
///
/// A property (VipProperty object) is similar to a VipInput and can be connected to another processing output, but
/// setting a property will never trigger a processing (as opposed to an input). Furtheremore, properties do not use
/// input buffers as opposed to VipInput class.
///
/// For synchronous processing, calling update() on a leaf processing will recursively update its source processing.
///
///
/// Scheduling strategies
/// ---------------------
///
/// A VipProcessingObject can be either synchronous (default) or asynchronous. Use VipProcessingObject::setScheduleStrategy()
/// or VipProcessingObject::setScheduleStrategies() to modify the processing scheduling strategy.
///
/// Synchronous processing are not automatically executed and require explicit calls to VipprocessingObject::update(),
/// which will internally call VipProcessingObject::apply() in the calling thread (default) or through the internal
/// TaskPool if NoThread strategy is unset.
///
/// Asynchronous processing (using Asynchronous flag) are automatically executed in the internal task pool when new input data are set, based
/// on the strategy flags. If the processing execution is not fast enough compared to the pace at which its inputs
/// are set, the input data are buffered in each VipInput buffer (VipDataList object) and processing executions are
/// scheduled inside the internal task pool. By default, each input uses a VipFIFOList, but the buffer type can be set
/// to VipLIFOList or VipLastAvailableList (no buffering, always use the last data).
///
/// Each input buffer can have a maximum size based either on a number of inputs or on the size in Bytes of buffered
/// inputs. Use VipDataList::setMaxListSize(), VipDataList::setMaxListMemory() and VipDataList::setListLimitType()
/// to control this behavior. Use VipProcessingManager to change the behavior of ALL existing input buffers and to ALL future ones.
///
/// VipProcessingObject is mostly used to build huge asynchronous processing pipelines.
///
///
/// Archiving
/// ---------
///
/// By default, all VipProcessingObject can be archived to VipArchive object. This is used within thermavip to build session files,
/// but also for copying VipProcessingObject objects.
///
/// The default serialization function save the processing VipProperty data, the connection status, the schedule strategies, the processing attributes,
/// and most other VipProcessingObject properties.
///
/// It is possible to define additional serialization functions for your class using vipRegisterArchiveStreamOperators().
///
///
/// Error handling
/// --------------
///
/// A processing that must notify an error in the VipProcessingObject::apply() implementation should use the VipProcessingObject::setError() member.
/// This will emit the signal error(QObject*, const VipErrorData&), and will print an error message if logging is enabled for the corresponding error code
/// (see VipProcessingObject::setLogErrorEnabled()).
///
/// The error code is reseted before each call to VipProcessingObject::apply().
///
///
/// Thread safety
/// -------------
///
/// VipProcessingObject class is NOT thread safe, while all members are reentrant.
///
/// Only a few members are thread safe (mandatory for asynchronous strategy):
/// -	All error related functions: setError(), resetError()...
/// -	All introspection functions: className(), description(), inputDescription(), inputNames()...
/// -	All I/O access members: topLevelInputAt(), inputAt(), outputAt() ...
/// -	Adding/retrieving data from VipInput/VipOutput/VipProperty objects, like inputAt(0)->setData(my_data), clearInputBuffers()...
/// -	Calls to setEnabled(), isEnabled(), setVisible(), isVisible()
/// -	Calls to update() and reset()
///
///
class VIP_CORE_EXPORT VipProcessingObject : public VipErrorHandler
{
	Q_OBJECT

	template<class TYPE>
	friend class VipMultipleProcessingIO;
	friend class VipProcessingIO;
	friend class VipInput;
	friend class VipOutput;
	friend class VipProperty;
	friend class VipProcessingList;
	friend class TaskPool;
	friend VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipProcessingObject* proc);
	friend VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipProcessingObject* proc);

protected:
	// Dummy accessor used by the VIP_IO macro in order to remove moc warnings
	VipAny io() const { return VipAny(); }

public:
	/// @brief Schedule strategy. These flags modify the behavior of VipProcessingObject::update().
	enum ScheduleStrategy
	{
		/// Launch the processing if only one new input data is set (default behavior).
		OneInput = 0x00,
		/// Launch the processing if if all input data are new ones.
		AllInputs = 0x01,
		/// The processing is automatically triggered depending on the schedule strategy (OneInput or AllInputs). Otherwise, you must call the update() function yourself.
		Asynchronous = 0x02,
		/// When the processing is triggered, skip the processing if it is currently being performed. Otherwise, it will be performed after the current one finish. Only works in asynchronous
		/// mode.
		SkipIfBusy = 0x04,
		/// Update the processing even if one of the input data is empty
		AcceptEmptyInput = 0x08,
		/// Do not call the apply() function if no new input is available. Only works in synchronous mode. The update() function directly call apply() without going
		/// through the thread pool. This is the default behavior. This flag is ignored if Asynchronous is set.
		SkipIfNoInput = 0x10,
		/// Default behavior for synchronous processing. VipProcessingObject::apply() is called with the update() member if set.
		/// Otherwise, it goes through the internal task pool to ensure that VipProcessingObject::apply() is ALWAYS called from
		/// the same thread.
		NoThread = 0x20
	};
	Q_DECLARE_FLAGS(ScheduleStrategies, ScheduleStrategy);

	/// Standard error identifiers used in VipProcessingObject::setError()
	enum ErrorCode
	{
		/// Default error code for an error occuring in the VipProcessingObject::apply member
		RuntimeError = -1,
		/// An input buffer is full
		InputBufferFull = -2,
		/// An input has a wrong type or is empty
		WrongInput = -3,
		/// Invalid number of inputs (mainly for VipMultiInput)
		WrongInputNumber = -4,
		/// A UniqueProcessingIO failed to establish a connection
		ConnectionNotOpen = -5,
		/// A VipIODevice is not open
		DeviceNotOpen = -6,
		/// Error while reading/writing from/to a file or socket
		IOError = -7,
		/// Start value for user defined errors
		User = -1000
	};

	/// Hint about how the output data (if any) should be displayed based on the input one(s).
	/// Reimplement displayHint() to provide a different DisplayHint.
	enum DisplayHint
	{
		/// The processing transform its unique input and returns an output of the same type and shape.
		/// The processing output can replace the input for display.
		/// This kind of processing should have only 1 input and 1 output.
		InputTransform,
		/// The processing might create a different object type as its input(s), but can be displayed
		/// on the same support as one of its input(s).
		DisplayOnSameSupport,
		/// The processing create an output data unrelated to the input one(s), and
		/// should be displayed on a different support.
		DisplayOnDifferentSupport,
		/// Default DisplayHint
		UnknownDisplayHint
	};

	/// VipProcessingObject information structure.
	/// Used to create a VipProcessingObject object from its name.
	struct VIP_CORE_EXPORT Info
	{
		QString classname;
		QString description;
		QString category;
		QIcon icon;
		QVariant init; // passed to #VipProcessingObject::initializeProcessing(const QVariant&) when creating the object
		int metatype;
		DisplayHint displayHint;
		Info(const QString& classname = QString(), const QString& description = QString(), const QString& category = QString(), const QIcon& icon = QIcon(), int metatype = 0)
		  : classname(classname)
		  , description(description)
		  , category(category)
		  , icon(icon)
		  , metatype(metatype)
		  , displayHint(UnknownDisplayHint)
		{
		}

		VipProcessingObject* create() const;

		bool operator==(const Info& other) { return metatype == other.metatype && init == other.init; }
		bool operator!=(const Info& other) { return metatype != other.metatype || init != other.init; }
	};

	/// @brief Constructor. Usually, the parent is a VipProcessingPool object.
	VipProcessingObject(QObject* parent = nullptr);
	/// Destructor
	virtual ~VipProcessingObject();

	/// @brief Returns the parent VipProcessingPool, if any
	VipProcessingPool* parentObjectPool() const;
	/// @brief Returns the direct VipProcessingObject sources for this processing.
	/// The direct sources are all VipProcessingObject with at least on output connected to one of this processing's inputs.
	virtual QList<VipProcessingObject*> directSources() const;
	/// @brief Return all processing sources, which means the direct sources, the sources of the sources and so one.
	QList<VipProcessingObject*> allSources() const;

	/// @brief Returns the direct sinks for this VipProcessingObject.
	/// The direct sinks are all  VipProcessingObject with at least one input connected to this processing's outputs.
	virtual QList<VipProcessingObject*> directSinks() const;
	/// @brief Returns all sink for this VipProcessingObject.
	QList<VipProcessingObject*> allSinks() const;

	/// @brief Identify the full pipeline (all sinks and sources) that this processing belongs to.
	QList<VipProcessingObject*> fullPipeline() const;

	/// @brief Enable/disable an error code logging
	virtual void setLogErrorEnabled(int error_code, bool enable);
	/// @brief Returns true if an error code should be logged
	bool isLogErrorEnabled(int error) const;
	/// @brief Set all error codes that should be logged
	virtual void setLogErrors(const QSet<int>& errors);
	/// @brief Returns all error codes that should be logged
	QSet<int> logErrors() const;

	/// @brief Returns the DisplayHint for this processing.
	/// Can be overriden in sub classes to provide a different diplay hint.
	virtual VipProcessingObject::DisplayHint displayHint() const { return UnknownDisplayHint; }

	/// @brief Class introspection function.
	/// Returns the Info structure for this object.
	virtual Info info() const;

	/// @brief Class introspection function.
	/// Returns VipProcessingObject class name.
	QString className() const;
	/// @brief Class introspection function.
	/// Returns VipProcessingObject description (if any).
	QString description() const;
	/// @brief Class introspection function.
	/// Returns VipProcessingObject category (if any).
	QString category() const;
	/// @brief Class introspection function.
	/// Returns VipProcessingObject icon (if any).
	QIcon icon() const;
	/// @brief Class introspection function.
	/// Returns an input description based on its name (if any).
	virtual QString inputDescription(const QString& input) const;
	/// @brief Class introspection function.
	/// Returns an output description based on its name (if any).
	virtual QString outputDescription(const QString& output) const;
	/// @brief Class introspection function.
	/// Returns a property description based on its name (if any).
	virtual QString propertyDescription(const QString& property) const;
	/// @brief Returns the property editor (a style sheet) for given property.
	/// A property editor if a qt class info of the form "edit_MyPropertyName".
	virtual QString propertyEditor(const QString& property) const;
	/// @brief Returns the property category for given property.
	/// A property category is define through the Q_CLASSINFO macro.
	/// Example:
	/// \code
	/// ...
	/// Q_CLASSINFO("category_MyPropertyName","Optional")
	/// ..
	/// \endcode
	/// You can also use the #VIP_PROPERTY_CATEGORY macro for this.
	virtual QString propertyCategory(const QString& property) const;

	/// @brief Tells if the processing is using internally the GUI event loop.
	/// This has an impact in the way the wait() function behaves.
	/// Most processing objects do not use the event loop, that's why this function returns false by default.
	/// Within Thermavip SDK, only VipDisplayObject uses it.
	virtual bool useEventLoop() const { return false; }

	/// @brief Returns all inputs names, including the VipInput inside the VipMultiInput
	QStringList inputNames() const;
	/// @brief Returns all output names, including the VipOutput inside the VipMultiOutput
	QStringList outputNames() const;
	/// @brief Returns all properties names, including the VipProperty inside the VipMultiProperty
	QStringList propertyNames() const;
	/// @brief Returns the total number of inputs
	int inputCount() const;
	/// @brief Returns the total number of outputs
	int outputCount() const;
	/// @brief Returns the total number of properties
	int propertyCount() const;
	/// @brief Returns an input based on its name
	VipInput* inputName(const QString& input) const;
	/// @brief Returns an output based on its name
	VipOutput* outputName(const QString& output) const;
	/// @brief Returns a property based on its name
	VipProperty* propertyName(const QString& property) const;
	/// @brief Returns the input at given index
	VipInput* inputAt(int index) const;
	/// @brief Returns the output at given index
	VipOutput* outputAt(int index) const;
	/// @brief Returns the property at given index
	VipProperty* propertyAt(int index) const;

	int indexOf(VipInput*) const;
	int indexOf(VipOutput*) const;
	int indexOf(VipProperty*) const;

	/// @brief Returns the description of a top level input (declared with Q_PROPERTY, so this does not take into account
	/// the VipInput inside VipMultiInput)
	QString topLevelInputDescription(const QString& input) const;
	/// @brief Returns the description of a top level output (declared with Q_PROPERTY, so this does not take into account
	/// the VipOutput inside VipMultiOutput)
	QString topLevelOutputDescription(const QString& output) const;
	/// @brief Returns the description of a top level property (declared with Q_PROPERTY, so this does not take into account
	/// the VipProperty inside VipMultiProperty)
	QString topLevelpropertyDescription(const QString& property) const;

	/// @brief Returns the number of top level inputs (declared with Q_PROPERTY, so this does not take into account
	/// the VipInput inside VipMultiInput)
	int topLevelInputCount() const;
	/// @brief Returns the number of top level outputs (declared with Q_PROPERTY, so this does not take into account
	/// the VipOutput inside VipMultiOutput)
	int topLevelOutputCount() const;
	/// @brief Returns the number of top level properties (declared with Q_PROPERTY, so this does not take into account
	/// the VipProperty inside VipMultiProperty)
	int topLevelPropertyCount() const;
	/// @brief Returns the top level input at given index
	VipProcessingIO* topLevelInputAt(int index) const;
	/// @brief Returns the top level output at given index
	VipProcessingIO* topLevelOutputAt(int index) const;
	/// @brief Returns the top level property at given index
	VipProcessingIO* topLevelPropertyAt(int index) const;
	/// @brief Returns the top level input with given name
	VipProcessingIO* topLevelInputName(const QString& name) const;
	/// @brief Returns the top level output with given name
	VipProcessingIO* topLevelOutputName(const QString& name) const;
	/// @brief Returns the top level property with given name
	VipProcessingIO* topLevelPropertyName(const QString& name) const;

	/// Returns true if given variant is a valid value for given input index
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& // v
	) const
	{
		return true;
	}
	/// @brief Returns true if given variant is a valid value for given property index
	virtual bool acceptProperty(int // index
				    ,
				    const QVariant& // v
	) const
	{
		return true;
	}
	/// @brief Returns true if given class name is a valid type for given input index
	bool acceptInput(int index, const char* typeName) const { return acceptInput(index, vipCreateVariant(typeName)); }
	/// @brief Returns true if given class name is a valid type for given property index
	bool acceptProperty(int index, const char* typeName) const { return acceptProperty(index, vipCreateVariant(typeName)); }

	/// @brief Set a function called each time inputs/outputs/properties are initialized
	void setIOInitializeFunction(const std::function<void()>&);
	const std::function<void()>& IOInitializeFunction() const;

	/// @brief Set the priority of the internal task pool
	void setPriority(QThread::Priority);
	/// @brief Returns the priority of the internal task pool
	QThread::Priority priority() const;

	//
	// Attributes management functions
	//
	// VipProcessingObject supports setting any kind of attributes on the form key (QString) -> value (QVariant).
	// The attributes are added to the processing output (VipAnyData) attributes.
	//

	/// @brief Set the attributes for this processing
	void setAttributes(const QVariantMap& attrs);
	/// @brief Set one attribute for this processing
	void setAttribute(const QString& name, const QVariant& value);
	/// @brief Returns true if this processing defines given attribute
	bool hasAttribute(const QString& name) const;
	/// @brief Returns all attributes
	const QVariantMap& attributes() const;
	/// @brief Removes an attribute based on its name.
	/// Returns true on success, false if the attribute does not exist.
	bool removeAttribute(const QString& name);
	/// @brief Returns a specific attribute value
	QVariant attribute(const QString& attr) const;
	/// @brief Merge this processing's attributes with given ones.
	/// Returns the list of attributes that has been modified/added.
	QStringList mergeAttributes(const QVariantMap& attrs);
	/// @brief Add the attributes from \a attrs that are not already present in the VipProcessingObject.
	/// Returns the list of attributes that were added.
	QStringList addMissingAttributes(const QVariantMap& attrs);

	/// @brief Set a source property. A source property is nothing more than a QObject dynamic property, but it will also be propagated to all processing object's sources.
	/// If this function is reimplemented, the new implementation sould call the base class implementation to ensure VipProcessingObjct's built-in integrity.
	virtual void setSourceProperty(const char* name, const QVariant& value);
	/// @brief Returns the names of all source properties
	QList<QByteArray> sourceProperties() const;

	//
	// Connection management
	//

	/// @brief Disconnect all inputs
	void clearInputConnections();
	/// @brief Disconnect all outputs
	void clearOutputConnections();
	/// @brief Disconnect all properties
	void clearPropertyConnections();
	/// @brief Disconnect all connections
	void clearConnections();
	/// @brief Remove all pending input data for all VipInput objects, and remove all pending updates.
	void clearInputBuffers();

	/// @brief Setup all output connections for this processing. This function does not need to be called for direct connections.
	/// \sa VipConnection::setupConnection.
	void setupOutputConnections(const QString& address);
	/// @brief Open all input connections. This function does not need to be called for direct connections.
	/// \sa VipConnection::openConnection
	void openInputConnections();
	/// @brief Open all output connections. This function does not need to be called for direct connections.
	/// \sa VipConnection::openConnection
	void openOutputConnections();

	/// @brief Open all connections. It starts first by the outputs and then the inputs/properties.
	void openAllConnections();

	/// @brief Call VipConnection::removeProcessingPoolFromAddress() for all inputs/properties
	void removeProcessingPoolFromAddresses();

	/// @brief Set the processing schedule strategy. The schedule strategy defines the behavior of the #VipProcessingObject::update function.
	///
	/// If #VipProcessingObject::Asynchronous is set, the processing is automatically triggered (i.e. update() is called). Otherwise, you muste calle update() yourself to apply the processing.
	///
	/// When #VipProcessingObject::update is called (either automatically or manually), the processing is actually performed (through #VipProcessingObject::apply)
	/// if at least a new input is available (if #VipProcessingObject::OneInput is set) or when all inputs are new (if #VipProcessingObject::Allinputs is set).
	///
	/// If at least on input is empty (i.e. no input has EVER been received), the processing is never performed.
	/// A disabled processing is NEVER performed.
	///
	/// If #VipProcessingObject::Asynchronous is set, calling  #VipProcessingObject::update return immediately and the processing is scheduled in a #TaskPool.
	/// If Otherwise, #VipProcessingObject::update will update all source processing first, apply its processing (depending on \a AllInput or \a OneInput) and then return.
	///
	/// If #VipProcessingObject::SkipIfBusy is set, the processing won't be performed if at least one processing is scheduled or in progress.
	///
	void setScheduleStrategies(ScheduleStrategies);
	void setScheduleStrategy(ScheduleStrategy, bool on = true);
	bool testScheduleStrategy(ScheduleStrategy) const;
	ScheduleStrategies scheduleStrategies() const;

	/// @brief VipProcessingObject keeps track of its last errors through a error buffer.
	/// This parameter lets you select the maximum error buffer size.
	int errorBufferMaxSize() const;
	void setErrorBufferMaxSize(int size);
	QList<VipErrorData> lastErrors() const;

	/// @brief If deleteOnOutputConnectionsClosed is set to true, the processing will be automatically deleted (with QObject::deleteLater()) when an output connection
	/// is closed and if all other output connections are closed.
	void setDeleteOnOutputConnectionsClosed(bool);
	/// @brief Returns the deleteOnOutputConnectionsClosed flag
	bool deleteOnOutputConnectionsClosed() const;

	/// @brief See #VipProcessingObject::setVisible(bool)
	bool isVisible() const;
	/// @brief Returns true if the processing is enabled
	bool isEnabled() const;
	/// @brief Returns true if the processing is currently updating (a call to #VipProcessingObject::update() is being performed)
	bool isUpdating() const;
	/// @brief If the processing is a child of a #VipProcessingPool, return the VipProcessingPool time.
	/// Otherwise, returns the current time since Epoch in nanoseconds.
	virtual qint64 time() const;

	/// @brief Enable/disable computing time statistics (true by default)
	/// Computing time statistics must be enabled to use functions processingTime(), processingRate() and lastProcessingTime().
	/// Time statistics can be disabled to very fast processing to remove its computation overhead.
	void setComputeTimeStatistics(bool);
	bool computeTimeStatistics() const;

	/// @brief Returns the time that took the last processing in nanoseconds
	virtual qint64 processingTime() const; // time of the last apply() in ns
	/// @brief Returns the processing rate (number of applied processing per second
	double processingRate() const; // number of apply() per second
	/// @brief Absolute date time (milliseconds since Epoch) of the last processing (call to apply())
	virtual qint64 lastProcessingTime() const;

	/// @brief Emit the error() signal
	virtual void emitError(QObject*, const VipErrorData&);

	/// @brief Copy the parameters from this device to the destination device of the same type.
	/// The default implementation copies the properties (declared with Q_PROPERTY(VipProperty my_property)) and the attributes (see #VipProcessingObject::setAttributes and
	/// related functions).
	/// If you reimplement this function, you should call the parent class's version to ensure that all parameters are properly copied.
	virtual void copyParameters(VipProcessingObject* dst);

	/// @brief Initialize the processing object based on \a value (that could be basically anything).
	/// This member function is almost never used. It's goal is to provide a way to see a processing class as multiple classes
	/// through #VipProcessingObject::Info structure.
	/// Currently, only the PyProcessing class uses this feature to register multiples PyProcessing objects with different Python code
	/// that can be seen as different #VipProcessingObject::Info objects.
	virtual QVariant initializeProcessing(const QVariant& value);

	/// @brief Returns a copy of this procesing object. The copy will keep the processing connections,
	/// and a call to #openAllConnections() will reopen them.
	/// This function uses a temporary in-memory XML archive to serialize the processing and deserialize it.
	VipProcessingObject* copy() const;

	/// @brief Returns this image transform (if any) from the top left corner.
	virtual QTransform imageTransform() const;

	/// @brief Returns the combination of image transformations from the source processing to this processing.
	/// If the processing pipeline has several branches, returns a null QTransform.
	/// The returned transform origin is the top left corner of the image, and can be directly applied to the ROIs.
	QTransform globalImageTransform();

	/// @brief Returns the number of pending processing in the TaskPool
	int scheduledUpdates() const;

	/// @brief Retrieve the number of inputs, properties and/or outputs that declares a given QMetaObject.
	/// The QMetaObject must be related to a class inheriting VipProcessingObject.
	/// Multi inputs/properties/outputs are considered as a single input/propertie/output/
	static void IOCount(const QMetaObject* meta, int* inputs = nullptr, int* properties = nullptr, int* outputs = nullptr);

	/// @brief Register a #VipProcessingObject::Info object.
	/// This function should only be used whenever a processing class can be used to create different processings.
	/// Currently, only the PyProcessing class (in Python plugin) uses this feature.
	/// \sa VipProcessingObject::initialize
	static void registerAdditionalInfoObject(const VipProcessingObject::Info& info);
	static QList<VipProcessingObject::Info> additionalInfoObjects();
	static QList<VipProcessingObject::Info> additionalInfoObjects(int metatype);
	/// Remove a registered Info object based on its metatype, classname and category
	static bool removeInfoObject(const VipProcessingObject::Info& info, bool all = true);

	/// @brief Returns all VipProcessingObject type information that match given criteria:
	///
	/// The processing is convertible to \a ProcessingType,
	///
	/// If \a output_count is < 0, the number of outputs does not matter. Otherwise, the processing object must have \a output_count VipOutput or one VipMultiOutput.
	///
	/// If \a inputs is empty, the number of inputs does not matter. Otherwise, the processing object must have inputs.size() VipInputs, or one VipMultiInput.
	/// Furthermore, for all non nullptr QVariant in \a inputs, the corresponding VipInput must accept the value (through #VipProcessingObject::acceptInput);
	///
	/// The return value is a QMultiMap of processing category -> processing info.
	///
	/// This function looks for all processing registered with #VIP_REGISTER_QOBJECT_METATYPE or #VipProcessingObject::registerAdditionalInfoObject.
	template<class ProcessingType>
	static QMultiMap<QString, Info> validProcessingObjects(const QVariantList& inputs, int output_count = -1, DisplayHint maxDisplayHint = UnknownDisplayHint);
	static QMultiMap<QString, Info> validProcessingObjects(const QVariantList& inputs, int output_count = -1, DisplayHint maxDisplayHint = UnknownDisplayHint)
	{
		return validProcessingObjects<VipProcessingObject*>(inputs, output_count, maxDisplayHint);
	}
	/// @brief Finds a processing object by its class name or through its custom name as registered with registerAdditionalInfoObject(),
	/// and returns its VipProcessingObject::Info.
	/// If no processing is found, returns a VipProcessingObject::Info with a null metatype.
	static VipProcessingObject::Info findProcessingObject(const QString& name);

	/// @brief Returns a list of all possible processing objects. The list is computed once, and each time a new processing class is defined through plugins.
	static QList<const VipProcessingObject*> allObjects();

public Q_SLOTS:

	/// @brief Initialize again the processing. This is only called setting the parent #VipProcessingPool's time to its first time.
	void reset();
	/// @brief Save the current processing state (enable/disable, schedule strategy and deleteOnOutputConnectionsClosed flag).
	/// This function can be reimplemented to save additional parameters, and the reimplementation must call the base version.
	virtual void save();
	/// @brief Restore a previously saved state. Each call to restore() must match a previous call to save().
	virtual void restore();
	/// @brief Enable/disable the processing. A disable processing does not accept inputs and is never triggered.
	virtual void setEnabled(bool);
	void disable() { setEnabled(false); }
	void enable() { setEnabled(true); }

	/// @brief Set the processing visibility.
	/// This property is used for display only, when the processing is inside a VipProcessingList that is edited.
	virtual void setVisible(bool);

	/// @brief Update the processing object.
	///
	/// This will trigger the processing if required, based on the scheduleStrategies():
	/// -	If OneInput is set, only launch the processing if at least one of its input is new (always launch if force_run is true)
	/// -	If  AllInputs is set, only launch the processing if all inputs are new (always launch if force_run is true)
	/// -	If AcceptEmptyInput is NOT set, do not launch the processing if at least one input is empty (always launch if force_run is true)
	/// -	If SkipIfBusy is set, only launch the processing if is not pending tasks are scheduled or under execution
	/// -	If Asynchronous is set, the task is scheduled and will be executed when possible. Otherwise, the task is immediatly
	/// executed. If NoThread is set, the task is executed in the camlling thread, otherwise it is executed through the task pool.
	///
	/// This function should only be called for synchronous  processings, as it is automatically called for asynchronous ones
	/// when filling their inputs.
	///
	bool update(bool force_run = false);
	/// @brief Reload the processing. This is equivalent to update(true).
	virtual bool reload();
	/// @brief Wait until there are no more scheduled tasks or until timeout (only meaningful with VipProcessingObject::Asynchronous)
	bool wait(bool wait_for_sources = true, int max_milli_time = -1);

Q_SIGNALS:

	/// Emitted when an input/output/property connection is opened
	void connectionOpened(VipProcessingIO* io, int mode, const QString& address);
	/// Emitted when an input/output/property connection is closed
	void connectionClosed(VipProcessingIO* io);
	/// Emitted when an input receive a new data
	void dataReceived(VipProcessingIO* io, const VipAnyData& data);
	/// Emitted when an output send a data
	void dataSent(VipProcessingIO* io, const VipAnyData& data);
	/// Emitted when an input/output/property changed
	void IOChanged(VipProcessingIO* io);
	/// Emitted when a processing has been performed with its computation time in nanoseconds
	void processingDone(VipProcessingObject* object, qint64 nano_seconds);
	/// Emitted when the processing parameters changed
	void processingChanged(VipProcessingObject* object);
	/// Emitted when the image transform for this processing changed
	void imageTransformChanged(VipProcessingObject* object);
	/// Emitted at the beginning of the destructor.
	/// You can call emitDestroyed() at the beginning of your destructor to emit this signal. In this case, it won't be emitted again in VipProcessingObject's destructor.
	void destroyed(VipProcessingObject*);

protected:
	/// @brief Apply the processing. Reimplement this function within your own processing.
	/// Default implementation does nothing.
	///
	/// VipProcessingObject will make sure that apply() and resetProcessing() will be called synchronously.
	virtual void apply();

	/// @brief Reset the processing.
	/// Default implementation does nothing.
	///
	/// VipProcessingObject will make sure that apply() and resetProcessing() will be called synchronously.
	virtual void resetProcessing();

	/// @brief For image processing only, returns the related QTransform.
	/// This is only required when a processing represents an image transformation (like rotations, mirroring, cropping...)
	/// that should be applied to the Regions Of Interest.
	/// The returned transform origin is considered to be the center of the image if
	/// \a from_center is set to true, otherwise from the top left corner.
	virtual QTransform imageTransform(bool* from_center) const
	{
		*from_center = false;
		return QTransform();
	}

	/// @brief Create a VipAnyData containing given data. You should always use this function in #VipProcessingObject::apply() since it take care of setting the data attributes, time and source.
	/// You might need to change the time value since it it always better to set the output data time to one of the input data time (instead of #VipProcessingObject::time() which
	/// only returns the parent #VipProcessingPool time).
	///
	/// If \a initial_attributes is not empty, output data attributes will be initialized with \a initial_attributes and then merged with the processing object own attributes.
	virtual VipAnyData create(const QVariant& data, const QVariantMap& initial_attributes = QVariantMap()) const;

	/// @brief When setting an output data with "outputAt(i)->setData()" , this function will be called to set the actual outut data time.
	/// It is only overriden by #VipIODevice that must take care of possible timestamping filter.
	virtual void setOutputDataTime(VipAnyData& // data
	)
	{
	}

	virtual void newError(const VipErrorData& error);

	void excludeFromProcessingRateComputation();

public Q_SLOTS:
	/// Emit the signal #VipProcessingPool::processingChanged. Use this function in derived classes when changing a parameter.
	void emitProcessingChanged();

protected Q_SLOTS:
	/// Emit the signal #VipProcessingPool::imageTransformChanged. Use this function in derived class when any action
	/// (like changing a property) modify the image transform. Note that calling this function will propagate the imageTransformChanged() signal
	/// to all the processing sinks that will also emit the signal.
	void emitImageTransformChanged();

	/// Emitted destroyed(VipProcessingObject*).
	/// You can call emitDestroyed() at the beginning of your destructor to emit this signal. In this case, it won't be emitted again in VipProcessingObject's destructor.
	void emitDestroyed();

private Q_SLOTS:

	void receiveConnectionOpened(VipProcessingIO* io, int type, const QString& address);
	void receiveConnectionClosed(VipProcessingIO* io);
	void receiveDataReceived(VipProcessingIO* io, const VipAnyData& data);
	void receiveDataSent(VipProcessingIO* io, const VipAnyData& data);

private:
	/// Initialize the processing object. You should not need to call this function yourself.
	void initialize(bool force = false) const;
	void internalInitIO(bool force) const;
	QString description(const QString& input) const;

	void dirtyProcessingIO(VipProcessingIO* io);
	/// Generate a unique output name. Tou should not need to call this function yourself.
	QString generateUniqueOutputName(const VipProcessingIO& out, const QString& name);
	/// Generate a unique input name. Tou should not need to call this function yourself.
	QString generateUniqueInputName(const VipProcessingIO& in, const QString& name);

	void run();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

template<class ProcessingType>
QMultiMap<QString, VipProcessingObject::Info> VipProcessingObject::validProcessingObjects(const QVariantList& lst, int output_count, DisplayHint maxDisplayHint)
{
	QMultiMap<QString, VipProcessingObject::Info> res;
	const QList<const VipProcessingObject*> all = allObjects();

	for (int i = 0; i < all.size(); ++i) {
		const VipProcessingObject* obj = all[i];
		const QMetaObject* meta = obj->metaObject();
		if (!qobject_cast<ProcessingType>((VipProcessingObject*)obj))
			continue;

		if (lst.isEmpty() && output_count < 0) {
			res.insert(obj->category(), obj->info());
			continue;
		}

		// First check the input and output count just based on the meta object, without actually creating an instance of VipProcessingObject
		// const QMetaObject * meta = obj->metaObject();
		if (!meta)
			continue;

		int in_count, out_count;
		VipProcessingObject::IOCount(meta, &in_count, nullptr, &out_count);
		if (lst.size() && !in_count)
			continue;
		else if (output_count && !out_count)
			continue;
		if (lst.size() > 1 && obj->displayHint() == InputTransform)
			continue;

		Info info = obj->info();
		if (info.displayHint > maxDisplayHint)
			continue;

		if (lst.size() == 0) {
			if (output_count < 0)
				res.insert(obj->category(), info);
			else if (output_count == 0) {
				if (obj->outputCount() == 0)
					res.insert(obj->category(), info);
			}
			else if (obj->outputCount() == output_count) {
				res.insert(obj->category(), info);
			}
			else {
				VipMultiOutput* out = obj->topLevelOutputAt(0)->toMultiOutput();
				if (out && out->minSize() <= output_count && out->maxSize() >= output_count)
					res.insert(obj->category(), info);
			}
		}
		else {
			if (obj->topLevelInputCount() > 0)
				if (VipMultiInput* multi = obj->topLevelInputAt(0)->toMultiInput()) {
					if (multi->minSize() > lst.size() || lst.size() > multi->maxSize()) {
						// min/max size of VipMultiInput not compatible with the input list
						continue;
					}
					multi->resize(lst.size());
				}

			if (lst.size() == obj->inputCount()) {
				bool accept_all = true;
				for (int j = 0; j < lst.size(); ++j) {
					if (!obj->acceptInput(j, lst[j]) && lst[j].userType() != 0) {
						accept_all = false;
						break;
					}
				}
				if (accept_all) {
					if (output_count < 0)
						res.insert(obj->category(), info);
					else if (output_count == 0) {
						if (obj->outputCount() == 0)
							res.insert(obj->category(), info);
					}
					else if (obj->outputCount() == output_count) {
						res.insert(obj->category(), info);
					}
					else {
						VipMultiOutput* out = obj->topLevelOutputAt(0)->toMultiOutput();
						if (out && out->minSize() <= output_count && out->maxSize() >= output_count)
							res.insert(obj->category(), info);
					}
				}
			}
		}
	}

	return res;
}

using ErrorCodes = QSet<int>;
Q_DECLARE_METATYPE(ErrorCodes)
Q_DECLARE_OPERATORS_FOR_FLAGS(VipProcessingObject::ScheduleStrategies)
Q_DECLARE_METATYPE(VipProcessingObject::Info)
using VipProcessingObjectInfoList = QList<VipProcessingObject::Info>;
Q_DECLARE_METATYPE(VipProcessingObjectInfoList);
VIP_REGISTER_QOBJECT_METATYPE(VipProcessingObject*)

inline uint qHash(const QPointer<VipProcessingObject>& key, uint seed = 0)
{
	return qHash(key.data(), seed);
}

/// @brief List of VipProcessingObject with additional convenient functions
class VIP_CORE_EXPORT VipProcessingObjectList : public QList<QPointer<VipProcessingObject>>
{
public:
	/// @brief Default constructor
	VipProcessingObjectList()
	  : QList<QPointer<VipProcessingObject>>()
	{
	}
	/// @brief Construct from a QList<QPointer<VipProcessingObject>>
	VipProcessingObjectList(const QList<QPointer<VipProcessingObject>>& other)
	  : QList<QPointer<VipProcessingObject>>(other)
	{
	}
	/// @brief Construct from a QList
	template<class T>
	VipProcessingObjectList(const QList<T*>& other)
	{
		for (auto it = other.begin(); it != other.end(); ++it)
			if (*it)
				append(*it);
	}
	/// @brief Construct from a QList
	template<class T>
	VipProcessingObjectList(const QList<QPointer<T>>& other)
	{
		for (typename QList<QPointer<T>>::const_iterator it = other.begin(); it != other.end(); ++it)
			if (*it)
				append(it->data());
	}

	/// @brief Find the first object of given type and with given name, or null on failure
	template<class T>
	T findOne(const QString& name = QString()) const
	{
		for (const_iterator it = begin(); it != end(); ++it) {
			if (T tmp = qobject_cast<T>(*it)) {
				if (name.isEmpty() || tmp->objectName() == name)
					return tmp;
			}
		}
		return nullptr;
	}

	/// @brief Find all objects of given type and with given name, or an empty list on failure
	template<class T>
	QList<T> findAll(const QString& name = QString()) const
	{
		QList<T> res;
		for (const_iterator it = begin(); it != end(); ++it) {
			if (T tmp = qobject_cast<T>(*it)) {
				if (name.isEmpty() || tmp->objectName() == name)
					res << tmp;
			}
		}
		return res;
	}
	/// @brief Find all objects of given type and with given name, or an empty list on failure
	template<class T>
	VipProcessingObjectList findAllProcessings(const QString& name = QString()) const
	{
		VipProcessingObjectList res;
		for (const_iterator it = begin(); it != end(); ++it) {
			if (T tmp = qobject_cast<T>(*it)) {
				if (name.isEmpty() || tmp->objectName() == name)
					res << tmp;
			}
		}
		return res;
	}
	/// @brief Update all processing objects within the list
	/// The 'force' paremeter is passed to VipProcessingObject::update().
	/// if 'except' is not null, it won't be updated.
	void update(bool force = false, VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->update(force);
	}
	/// @brief Wait for all processing within the list, except the 'except' one
	void wait(VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->wait();
	}
	/// @brief Set the schedule strategy for all processing within the list, except the 'except' one
	void setScheduleStrategy(VipProcessingObject::ScheduleStrategy st, bool on, VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->setScheduleStrategy(st, on);
	}
	/// @brief Set the schedule strategies for all processing within the list, except the 'except' one
	void setScheduleStrategies(VipProcessingObject::ScheduleStrategies st, VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->setScheduleStrategies(st);
	}
	/// @brief Save all processing within the list, except the 'except' one
	void save(VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->save();
	}
	/// @brief Restore all processing within the list, except the 'except' one
	void restore(VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->restore();
	}
	/// @brief Enable/disable all processing within the list, except the 'except' one
	void setEnabled(bool enable, VipProcessingObject* except = nullptr)
	{
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				if (tmp != except)
					tmp->setEnabled(enable);
	}
	/// @brief Convert the list to a QSet and remove duplicates and numm objects (if any)
	QSet<VipProcessingObject*> toSet() const
	{
		QSet<VipProcessingObject*> res;
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				res.insert(tmp);
		return res;
	}
	/// @brief Implicit conversion to QList<VipProcessingObject*>
	operator QList<VipProcessingObject*>() const
	{
		QList<VipProcessingObject*> res;
		for (const_iterator it = begin(); it != end(); ++it)
			if (VipProcessingObject* tmp = (*it))
				res.append(tmp);
		return res;
	}

	/// @brief Copy the list of processing and insert it into given VipProcessingPool (if not null).
	///
	/// The processing objects are copied using their serialization functions and a VipArchive.
	/// The processing properties and connections are copied as well, resulting in a fully
	/// operational processing pipeline.
	///
	/// Returns the list of new processing, or an empty list on failure.
	///
	QList<VipProcessingObject*> copy(VipProcessingPool* dst = nullptr);
};

/// @brief Returns the sources of input processing objects
template<class T>
QList<VipProcessingObject*> vipProcessingSources(const QList<T*>& procs)
{
	QList<VipProcessingObject*> sources;
	for (int i = 0; i < procs.size(); ++i)
		sources += procs[i]->directSources();
	return sources;
}

/// @brief Returns ALL the sources of input processing objects
template<class T>
QList<VipProcessingObject*> vipProcessingAllSources(const QList<T*>& procs)
{
	QList<VipProcessingObject*> sources;
	for (int i = 0; i < procs.size(); ++i)
		sources += procs[i]->allSources();
	return sources;
}

/// @brief Returns the sinks of input processing objects
template<class T>
QList<VipProcessingObject*> vipProcessingSinks(const QList<T*>& procs)
{
	QList<VipProcessingObject*> sinks;
	for (int i = 0; i < procs.size(); ++i)
		sinks += procs[i]->directSinks();
	return sinks;
}

/// @brief Returns ALL the sinks of input processing objects
template<class T>
QList<VipProcessingObject*> vipProcessingAllSinks(const QList<T*>& procs)
{
	QList<VipProcessingObject*> sinks;
	for (int i = 0; i < procs.size(); ++i)
		sinks += procs[i]->allSinks();
	return sinks;
}

//
// Examples
//

/// @brief A processing object which stores a list of VipProcessingObject that will be applied consecutively.
///
/// VipProcessingList is a convenient VipProcessingObject used to stack VipProcessingObject objects that
/// will be consecutively applied.
///
/// Processing are added using append() or insert(). Added processing must have at least one input and one output.
/// Within  VipProcessingList::apply(), the VipProcessingList input is set to the first VipProcessingObject,
/// each processing are then applied consecutively,and the last VipProcessingObject output is set to the VipProcessingList output.
///
/// VipProcessingList does not support duplicate processing.
/// VipProcessingList takes ownership of the processing.
///
class VIP_CORE_EXPORT VipProcessingList : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("icon", "")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("description",
		    "Apply a list of processing to a single output, return the result."
		    "Each processing must have exactly one input and at least one ouput (the other ones are ignored)")

public:
	VipProcessingList(QObject* parent = nullptr);
	~VipProcessingList();

	/// @brief Returns the list of processing
	const QList<VipProcessingObject*>& processings() const;
	/// @brief Returns the list of processing objects that can be converted to T
	template<class T>
	QList<T> processings() const
	{
		QList<T> res;
		QList<VipProcessingObject*> lst = processings();
		for (QList<VipProcessingObject*>::const_iterator it = lst.cbegin(); it != lst.cend(); ++it)
			if (T obj = qobject_cast<T>(*it))
				res.append(obj);
		return res;
	}

	/// @brief Add a processing. VipProcessingList will take ownership of the processing.
	bool append(VipProcessingObject*);
	/// @brief Remove and delete a processing
	bool remove(VipProcessingObject*);
	/// @brief Insert a processing at given index
	bool insert(int, VipProcessingObject*);
	/// @brief Returns the index of a processing or -1 if not found
	int indexOf(VipProcessingObject*) const;
	/// @brief Returns the processing at given index
	VipProcessingObject* at(int) const;
	/// @brief Removes and returns the processing at given index
	VipProcessingObject* take(int);
	/// @brief Returns the number of processings in the processing list
	int size() const;

	/// @brief If not empty, force the output data name.
	/// Pass an empty string to remove the override name.
	void setOverrideName(const QString&);
	QString overrideName() const;

	/// @brief Reimmplemented from VipProcessingObject
	virtual void setSourceProperty(const char* name, const QVariant& value);
	/// @brief Reimmplemented from VipProcessingObject, returns this processing list direct sources as well as the internal processing direct sources (usually none)
	virtual QList<VipProcessingObject*> directSources() const;
	/// @brief Reimmplemented from VipProcessingObject, returns this processing list image transform based on internal processings
	virtual QTransform imageTransform(bool* from_center) const;
	/// @brief Reimmplemented from VipProcessingObject, returns true if at least on internal processing uses the event loop
	virtual bool useEventLoop() const;

	static QList<VipProcessingObject::Info> validProcessingObjects(const QVariant& input_type);

private Q_SLOTS:
	void receivedProcessingDone(VipProcessingObject*, qint64);

protected:
	QTransform computeTransform();
	void applyFrom(VipProcessingObject* obj = nullptr);
	virtual void apply();
	virtual void resetProcessing();

private:
	void computeParams();

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipProcessingList*)

/// @brief Base class for all VipProcessingObject that use as parameter a VipSceneModel or a VipShape.
///
/// If a VipSceneModel is a mandatory input of the processing, you should inherit VipProcessingObject instead and declare a VipSceneModel as input.
///
/// The VipSceneModel is retrieved through the function VipSceneModelBasedProcessing::sceneModel(). This function first look for the property \a scene_model, and if this property is empty,
/// the dynamic property "VipSceneModel" is returned. You can set the VipSceneModel by setting the property \a scene_model or by calling #VipSceneModelBasedProcessing::setSceneModel
///
/// \a VipSceneModelBasedProcessing can also work on a \a VipShape instead of a \a VipSceneModel. Indeed, the \a scene_model property can also hold a \a VipShape object instead of a \a VipSceneModel
/// one. Otherwise, the \a shape_id represents the shape identifier in the VipSceneModel. Use #VipSceneModelBasedProcessing::shape to retrieve the shape.
///
/// \a VipSceneModelBasedProcessing is not considered as the owner of the \a VipSceneModel. Therefore, the VipSceneModel is stored as a #VipLazySceneModel internally.
///
/// If the VipSceneModel is modified externally (for instance through the GUI), #VipProcessingObject::reload is automatically called.
class VIP_CORE_EXPORT VipSceneModelBasedProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipProperty scene_model)
	VIP_IO(VipMultiProperty shape_ids)

public:
	enum MergeStrategy
	{
		NoMerge,
		MergeUnion,
		MergeIntersection
	};

	VipSceneModelBasedProcessing(QObject* parent = nullptr);
	~VipSceneModelBasedProcessing();

	void setMergeStrategy(MergeStrategy);
	MergeStrategy mergeStrategy() const;

	/// Set the scene model by setting the 'scene_model' property, and optionally set a shape identifier
	void setSceneModel(const VipSceneModel& scene, const QString& identifier = QString());
	/// Set the scene model by setting the 'scene_model' property, and set a shape identifiers (property 'shape_ids')
	void setSceneModel(const VipSceneModel& scene, const QList<VipShape>& shapes);
	/// Set the scene model by setting the 'scene_model' property, and set a shape identifiers (property 'shape_ids')
	void setSceneModel(const VipSceneModel& scene, const QStringList& identifiers);
	/// Set the shape by setting the 'scene_model' property and the 'shape_ids' property
	void setShape(const VipShape& shape);
	/// Set the shape directly. Note that when calling this function, the shape is 'fixed',
	/// and reload() will never be called when moving the shape manually or programmatically.
	void setFixedShape(const VipShape& shape);

	/// Returns the scene model object
	VipSceneModel sceneModel();

	/// Returns the shape if a shape is set, or an empty shape.
	/// If several shapes are set and the merge strategy is set to NoMerge, returns the last shape.
	/// Otherwise, the returns the result of the merge strategy (either union or intersection of all shapes).
	/// Note that this will alwayse return a copy of the set shape that can be freely modified.
	VipShape shape();

	/// Returns all shapes set as properties without applying the merge strategy.
	/// The shape transform (if any) is applied.
	/// Note that this will alwayse return a copy of the set shape that can be freely modified.
	QList<VipShape> shapes();

	/// Enable/disable a call to VipProcessingObject::reload() whenever the a shape changes within the scene model.
	/// True by default.
	void setReloadOnSceneChanges(bool);
	bool reloadOnSceneChanges() const;

	void setShapeTransform(const QTransform& tr);
	QTransform shapeTransform() const;

private Q_SLOTS:
	void dirtyShape();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Processing taking any kind and number of inputs, and send them one by one to the unique output.
/// Since a processing input can be connected to only one output, this is a convenient way to overcome this limitation.
class VIP_CORE_EXPORT VipMultiInputToOne : public VipProcessingObject
{
	Q_OBJECT

	Q_CLASSINFO("description",
		    "Take any kind and number of inputs, and send them one by one to the unique output.\n"
		    "Since a processing input can be connected to only one output, this is a convenient way to overcome this limitation.")
	Q_CLASSINFO("category", "Miscellaneous")
	VIP_IO(VipMultiInput InputValues)
	VIP_IO(VipOutput OutputValue)

public:
	VipMultiInputToOne(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipMultiInputToOne*)

/// @brief Processing taking any kind and number of inputs, and send only one of them to the unique output
class VIP_CORE_EXPORT VipSwitch : public VipProcessingObject
{
	Q_OBJECT

	Q_CLASSINFO("description", "Take any kind and number of inputs, and send only one of them to the unique output.\n")
	Q_CLASSINFO("category", "Miscellaneous")
	VIP_IO(VipMultiInput InputValues)
	VIP_IO(VipOutput OutputValue)
	VIP_IO(VipProperty Selector)

public:
	VipSwitch(QObject* parent = nullptr);

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipSwitch*)

/// @brief Processing that extract an attribute from its input VipAnyData and send it to its output
class VIP_CORE_EXPORT VipExtractAttribute : public VipProcessingObject
{
	Q_OBJECT

	Q_CLASSINFO("description", "Extract an attribute fron the input data.\n")
	Q_CLASSINFO("category", "Miscellaneous")
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty attribute_name)
	VIP_IO(VipProperty format_to_double)

public:
	VipExtractAttribute(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		// initialize the property to an empty string
		propertyAt(0)->setData(QString());
		propertyAt(1)->setData(false);
	}

protected:
	virtual void apply();

private:
	double ToDouble(const QVariant& var, bool* ok = nullptr);
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractAttribute*)

class VipArchive;
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipInput& input);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipMultiInput& minput);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipOutput& output);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipMultiOutput& moutput);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipProperty& property);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipMultiProperty& mproperty);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipProcessingObject* proc);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipProcessingList* plist);

VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipInput& input);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipMultiInput& minput);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipOutput& output);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipMultiOutput& moutput);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipProperty& property);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipMultiProperty& mproperty);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipProcessingObject* proc);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipProcessingList* plist);

/// @}
// end Core

#endif
