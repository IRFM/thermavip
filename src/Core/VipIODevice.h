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

#ifndef VIP_IO_DEVICE_H
#define VIP_IO_DEVICE_H

#include <limits>

#include <QFileInfo>
#include <QIODevice>
#include <QMetaObject>
#include <QPointer>
#include <QRect>

#include "VipMapFileSystem.h"
#include "VipNDArrayImage.h"
#include "VipProcessingObject.h"
#include "VipTimestamping.h"

/// \addtogroup Core
/// @{

/// The VipIODevice class is the base interface class of all I/O processing objects.
///
/// VipIODevice provides both a common implementation and an abstract interface for devices that support reading and writing of VipAnyData.
/// VipIODevice is abstract and can not be instantiated, but it is common to use the interface it defines to provide device-independent I/O features.
///
/// A VipIODevice can be of 3 types:
/// - VipIODevice::Temporal: it defines a start and end time, a time window and might have a notion of position. A Temporal devices support both input and output operations.
/// - VipIODevice::Sequential: input data can arrive at any time. It does not have a notion of time window or position. Sequential devices only support input operations. They are usually suited for
/// streaming operations.
/// - VipIODevice::Resource: the device holds one unique data. It does not have a notion of time and position at all. Resource devices are suited for non temporal data, like for instance an image file
/// on the drive.
///
/// A read-only VipIODevice should defines one or more outputs (see VipProcessingObject for more details).
/// A write-only VipIODevice should defines one or more inputs.
///
/// To open a VipIODevice, you should usually follow these steps:
/// - Set all necessary parameters
/// - Set the path using VipIODevice::setPath() (for instance for file based devices) or set the QIODevice (VipIODevice::setDevice())
/// - Call VipIODevice::open() with the right open mode (usually VipIODevice::ReadOnly or VipIODevice::WriteOnly).
///
/// If the VipIODevice is intended to work on files, you should call VipIODevice::setPath() or VipIODevice::setDevice() prior to open the VipIODevice.
/// If using VipIODevice::setPath(), the VipIODevice::open() override should call VipIODevice::createDevice() to build a suitable QIODevice based on
/// the set VipMapFileSystemPtr object (if any).
///
/// WriteOnly devices behave like VipProcessingObject: you should reimplement the VipProcessingObject::apply() function and call VipProcessingObject::update() to write the data
/// (for non Asynchronous processing, otherwise the update() function is called automatically).
///
/// ReadOnly, Temporal or Resource devices don't care about the apply() function (that's why an empty implementation of apply() is provided) and must reimplement VipIODevice::readData() instead.
/// Sequential devices do not need to reimplement on of these functions. Instead, they must reimplement VipIODevice::enableStreaming() to start/stop sending output data.
///
class VIP_CORE_EXPORT VipIODevice : public VipProcessingObject
{
	Q_OBJECT
	friend class VipProcessingPool;

public:
	/// Device open mode
	enum OpenModeFlag
	{
		NotOpen = 0x0000,
		ReadOnly = 0x0001,
		WriteOnly = 0x0002,
		ReadWrite = ReadOnly | WriteOnly,
		Append = 0x0004
	};
	Q_DECLARE_FLAGS(OpenModes, OpenModeFlag)

	/// Device type
	enum DeviceType
	{
		Temporal,   //! A temporal device with a valid time window
		Sequential, //! A temporal device with no valid time window (data arrive when they arrive)
		Resource    //! A non temporal, unique ressource,
	};

	/// Remove a device's class name prefix to a given path (look for the string 'device_name:' and remove it).
	static QString removePrefix(const QString& path, const QString& prefix);

	//
	// Functions common to all VipIODevice classes
	//

	VipIODevice(QObject* parent = nullptr);
	virtual ~VipIODevice();
	/// Return true if the device can handle given format based on a path and/or the first read bytes.
	///  Default implementation just check if the path starts with 'class_name:' .
	virtual bool probe(const QString& filename, const QByteArray& first_bytes = QByteArray()) const
	{
		Q_UNUSED(first_bytes);
		return filename.startsWith(className() + ":");
	}

	/// Return the device date in nanoseconds since Epoch (if any).
	qint64 date() const { return attribute("Date").toLongLong(); }
	/// Return the device author.
	QString author() const { return attribute("Author").toString(); }
	/// Return the device title. Default behavior returns the device path.
	QString name() const { return attribute("Name").toString(); }
	/// Return comments associated to the device (if any).
	QString comment() const { return attribute("Comment").toString(); }

	/// Return the device supported modes (read/write operations).
	virtual VipIODevice::OpenModes supportedModes() const = 0;
	/// Open the device in given mode. Returns true on success, false otherwise.
	///  This function should call #VipIODevice::setOpenMode with the right mode in case of success.
	virtual bool open(VipIODevice::OpenModes) = 0;
	/// Return the device type.
	/// Sequential devices, as opposed to a random-access devices, have no concept of a start, an end, a size, or a current position, and they do not support seeking. You can only read from the
	/// device when it reports that data is available. Devices based on regular files do support random access. They have both a size and a current position, and they also support seeking
	/// backwards and forwards in the data stream. Default implementation returns Temporal.
	virtual DeviceType deviceType() const = 0;

	/// Close the device. When subclassing VipIODevice, you must call VipIODevice::close() at the start of your function to ensure integrity with VipIODevice's built-in function.
	///  You should also call this function in your destructor.
	///  This function does 5 things:
	///  <ul>
	///  <li>reset the size of the device to 0,</li>
	///  <li>set the open mode to NotOpen,</li>
	///  <li>close the internal QIODevice returned by #VipIODevice::device() (if any),</li>
	///  <li>delete the internal QIODevice returned by #VipIODevice::device() if it is a children of this VipIODevice and this function is called from this object thread,</li>
	///  <li>set the internal QIODevice to nullptr</li>
	///  </ul>
	virtual void close();

	/// Return true is the device is currently opened
	bool isOpen() const { return openMode() != VipIODevice::NotOpen; }
	/// Return the opened mode
	VipIODevice::OpenModes openMode() const;
	/// Return the path of the device.
	///  \sa setPath()
	QString path() const;
	/// Return  the full path of the device ('device_name:path').
	QString fullPath() const { return QString(this->metaObject()->className()) + ":" + removePrefix(path()); }
	/// Returns the QIODevice associated to this VipIODevice.
	///  \sa setDevice()
	QIODevice* device() const;

	/// Returns the VipMapFileSystemPtr associated to this VipIODevice.
	///  \sa setMapFileSystem
	VipMapFileSystemPtr mapFileSystem() const;

	//
	// Functions for temporal (random-access) devices
	//

	/// For open Temporal (random-access) devices, this function returns the size of the device.
	///  For sequential devices or if the size cannot be computed, returns a negative value.
	///  If the device is closed, the size returned will not reflect the actual size of the device.
	///  Note that some temporal devices might not have a notion of size or position.
	qint64 size() const;

	/// Set the device #VipTimestampingFilter
	virtual void setTimestampingFilter(const VipTimestampingFilter& filter);
	/// Returns the device #VipTimestampingFilter
	const VipTimestampingFilter& timestampingFilter() const;
	/// Resets the timestamping filter
	void resetTimestampingFilter();
	/// Transforms a time based on the timestamping filter. If no timestamping filter is set, returns \a time.
	///  If \a inside is not nullptr, it is set to true if \a time is a valid time (inside the time ranges), false otherwise. Indeed,
	///  invTransformTime will always return a valid time value, and will select the closest valid time if \a time is outisde the time ranges.
	///  If \a exact_time is not nullptr, it will be set to true if given time is an exact valid timestamp for this device.
	qint64 transformTime(qint64 time, bool* inside = nullptr, bool* exact_time = nullptr) const;
	/// Returns the inverse transform of a time based on the timestamping filter. If no timestamping filter is set, returns \a time.
	///  If \a inside is not nullptr, it is set to true if \a time is a valid time (inside the time ranges), false otherwise. Indeed,
	///  invTransformTime will always return a valid time value, and will select the closest valid time if \a time is outisde the time ranges.
	qint64 invTransformTime(qint64 time, bool* inside = nullptr, bool* exact_time = nullptr) const;

	/// Return the time window for Temporal devices, taking into account the timestamping filter.
	VipTimeRangeList timeWindow() const;
	// Returns the first time for Temporal devices, taking into account the timestamping filter.
	qint64 firstTime() const;
	// Returns the last time for Temporal devices, taking into account the timestamping filter.
	qint64 lastTime() const;
	// Returns the time limits for Temporal devices, taking into account the timestamping filter.
	VipTimeRange timeLimits() const;
	/// Transforms given position to its time for Temporal devices. If the device does not have a notion of position, returns VipInvalidTime.
	qint64 posToTime(qint64 pos) const;
	/// Transforms given time to its position for Temporal devices. If the device does not have a notion of position, returns VipInvalidTime.
	qint64 timeToPos(qint64 time) const;

	/// Returns an estimation of the device sampling time, or VipInvalidTime if the sampling time cannot be deduced.
	/// This function relies on VipIODevice::firstTime and VipIODevice::nextTime.
	qint64 estimateSamplingTime() const;

	//
	// Functions for all read only devices
	//

	/// Return the next valid time after given time.
	/// This function returns lastTime() if \a time is >= lastTime(). In case of error, VipInvalidTime is returned.
	qint64 nextTime(qint64 time) const;

	/// Returns the previous valid time after given time.
	/// This function returns firstTime() if \a time is <= firstTime(). In case of error, VipInvalidTime is returned.
	qint64 previousTime(qint64 time) const;

	/// Returns the closest valid time around given time.
	/// This function returns firstTime() if \a time is <= firstTime() or lastTime() if \a time >= lastTime(). In case of error, VipInvalidTime is returned.
	qint64 closestTime(qint64 time) const;

	/// Returns the current read time.
	/// Reimplemented from VipProcessingObject.
	virtual qint64 time() const;
	/// Return the current read position, if the device has a notion of position.
	qint64 position() const { return timeToPos(time()); }

	/// Read the closest data with a timestamp equal or under given time (forward == true) or equal or above given time (forward == false).
	///  Return false if no data has been loaded.
	///  Set \a force to true if you request a time == time(), otherwise the data won't be loaded again.
	bool read(qint64 time, bool force = false);

	/// Returns true if the device is currently reading a data.
	virtual bool isReading() const;

	/// Absolute date time (milliseconds since Epoch) of the last processing (call to readData())
	virtual qint64 lastProcessingTime() const;

	/// For file based devices, returns the file filter.
	///  Example: "Image files (*.bmp *.png *.jpg)'
	virtual QString fileFilters() const { return QString(); }

	/// Returns true if given filename match the file filters (as returned by VipIODevice::fileFilters())
	bool supportFilename(const QString& fname) const;

	/// Returns true if streaming is enabled
	bool isStreamingEnabled() const;

	/// Reimplemented from VipProcessingObject. Dirty the parent processing pool time window since disabled devices are not used to compute the processing pool time window.
	virtual void setEnabled(bool);

	/// Elapsed time between 2 calls of VipIODevice::read() (for read-only devices) or time of the last VipIODevice::apply() (for write-only devices) in ns
	virtual qint64 processingTime() const;

	/// Returns an estimation of the input/output file size in Bytes (if any).
	/// Returns -1 on error or if the size cannot be estimated.
	virtual qint64 estimateFileSize() const { return QFileInfo(removePrefix(path())).size(); }

	/// @brief Returns all read devices that can handle given path and/or first bytes
	static QList<VipProcessingObject::Info> possibleReadDevices(const VipPath& path, const QByteArray& first_bytes, const QVariant& out_value = QVariant());
	/// @brief Returns all read devices that can handle given path and input data
	static QList<VipProcessingObject::Info> possibleWriteDevices(const VipPath& path, const QVariantList& input_data);
	/// @brief Returns the file filters (as returned by #VipIODevice::fileFilters)  for the read devices that can handle given path and/or first bytes
	static QStringList possibleReadFilters(const VipPath& path, const QByteArray& first_bytes, const QVariant& out_value = QVariant());
	/// @brief Returns the file filters (as returned by #VipIODevice::fileFilters)  for the write devices that can handle given path and input data
	static QStringList possibleWriteFilters(const VipPath& path, const QVariantList& input_data);
	/// @brief Unregister a VipIODevice type in order to NOT be visible by calls to possibleReadDevices() and possibleWriteDevices()
	static void unregisterDeviceForPossibleReadWrite(int id);

public Q_SLOTS:

	/// Reimplemented from VipProcessingObject.
	/// For read only devices, reload the data at current time.
	virtual bool reload();
	/// Reimplemented from VipProcessingObject. Save the current state
	virtual void save();
	/// Reimplemented from VipProcessingObject. Restore a previously saved state
	virtual void restore();

	///	Set the VipIODevice path and return true on success, false otherwise.
	/// When subclassing VipIODevice, you must call VipIODevice::setPath() at the start of your function to ensure integrity with VipIODevice's built-in path.
	virtual bool setPath(const QString& path);
	/// Set the VipIODevice device. When subclassing VipIODevice, you must call VipIODevice::setDevice() at the start of your function to ensure integrity with VipIODevice's built-in device.
	/// VipIODevice does NOT take ownership of the QIODevice.
	virtual void setDevice(QIODevice* device);
	/// Set the VipMapFileSystemPtr (might be null).
	/// When subclassing VipIODevice, you must call VipIODevice::setMapFileSystem() at the start of your function to ensure integrity with VipIODevice's built-in device.
	///
	/// The VipMapFileSystemPtr is internally used to build a QIODevice using VipIODevice::createDevice() member.
	/// Setting a custom VipMapFileSystemPtr  allows to open a VipIODevice on a virtual file system like FTP one.
	///
	virtual void setMapFileSystem(const VipMapFileSystemPtr& map);

	//
	// Functions for Sequential devices
	//

	/// For Sequential devices, enable/disable streaming.
	bool setStreamingEnabled(bool enabled);
	bool startStreaming();
	bool stopStreaming();

	/// Read the data at the current time, expressed in nanoseconds since Epoch. Only meaningful for Sequential devices.
	/// This function could be usefull for Sequential devices that just call readCurrentData() periodically and reimplement readData().
	bool readCurrentData();

	/// Remove the current device's name prefix to a given path (look for the string 'device_name:' and remove it).
	QString removePrefix(const QString& path) const { return removePrefix(path, QString(this->metaObject()->className())); }

Q_SIGNALS:

	/// This signal is emitted when the timestamping filter is changed
	void timestampingFilterChanged();
	/// Emit this signal if, for any reason, you change the timestamps of your data while the device is open
	void timestampingChanged();
	/// @brief Emitted when the VipIODevice time changed
	void timeChanged(qint64);
	/// @brief Emitted when the VipIODevice was successfully opened
	void opened();
	/// @brief Emitted when the VipIODevice was closed
	void closed();
	/// @brief Emitted when the device was opened (true as parameter) or closed (false as parameter)
	void openModeChanged(bool);
	/// @brief Emitted when streaming was started for Sequential devices
	void streamingStarted();
	/// @brief Emitted when streaming was stopped for Sequential devices
	void streamingStopped();
	/// @brief Emitted when streaming status changed for Sequential devices
	void streamingChanged(bool);

protected:
	/// For Temporal devices, return the next valid time after given time. Default implementation uses the notion of position and size.
	/// If the device has no notion of size and position, you should reimplement this function.
	/// This function should return the last valid time if \a time is >= last valid time. In case of error, VipInvalidTime should be returned.
	virtual qint64 computeNextTime(qint64 time) const
	{
		qint64 pos = computeTimeToPos(time);
		return computePosToTime(pos + 1);
	}

	/// For Temporal devices, return the previous valid time after given time. Default implementation uses the notion of position and size.
	/// If the device has no notion of size and position, you should reimplement this function.
	/// This function should return the first if \a time is <= first time. In case of error, VipInvalidTime should be returned.
	virtual qint64 computePreviousTime(qint64 time) const
	{
		qint64 pos = computeTimeToPos(time);
		return computePosToTime(qMax(pos - 1, qint64(0)));
	}

	/// For Temporal devices, return the closest valid time around given time. Default implementation uses the notion of position and size.
	/// If the device has no notion of size and position, you should reimplement this function.
	/// This function should return the first time if \a time is <= first time, or last time if \a time >= last time. In case of error, VipInvalidTime should be returned.
	virtual qint64 computeClosestTime(qint64 time) const
	{
		qint64 pos = computeTimeToPos(time);
		return computePosToTime(pos);
	}

	/// For Temporal devices, convert given position to its closest time (in nanoseconds).
	virtual qint64 computePosToTime(qint64) const { return VipInvalidTime; }

	/// For Temporal devices, convert given time (in nanoseconds) to its closest position.
	virtual qint64 computeTimeToPos(qint64) const { return VipInvalidPosition; }

	/// For Temporal devices, returns the temporal window.
	virtual VipTimeRangeList computeTimeWindow() const { return VipTimeRangeList(); }

	/// For Sequential device, enable/disable data streaming
	virtual bool enableStreaming(bool) { return false; }

	/// Reimplement this function for Temporal ReadOnly devices. Read the data at given time, and set the corresponding output(s).
	/// Returns true on success, false otherwise.
	virtual bool readData(qint64) { return false; }

	/// For read-only temporal device, this function is called each time the VipIODevice::read() member is called on an invalid time (for instance outside the device time window).
	/// If this member returns false (default), VipIODevice::read() will keep going and eventually call VipIODevice::readData() on the closest valid time.
	/// If this member returns true,  VipIODevice::read() immediatly exits and returns true.
	virtual bool readInvalidTime(qint64 time);

	/// Reimplement this function for WriteOnly devices. Inherited from VipProcessingObject, called whenever a new input data is available (depending on the schedule strategy).
	virtual void apply() {}

	/// Reimplemented from VipProcessingObject.
	/// Set an output data time using VipIOdevice::time() member. The output data time is not necessarily the one given as argument of VipIODevice::readData().
	/// Indeed, the device might have a timestamping filter, and the output time must reflect that.
	virtual void setOutputDataTime(VipAnyData& data);

	/// Set the open mode of the device.
	/// When subclassing VipIODevice, this function should be used in the VipIODevice::open() override.
	/// If setOpenMode() is overloaded, you must call VipIODevice::setOpenMode() at the start of your function to ensure integrity with VipIODevice's built-in mode.
	virtual void setOpenMode(VipIODevice::OpenModes mode);

	/// Set the device size and return true on success, false otherwise.
	/// For Sequential devices, this function does nothing and returns false.
	/// When subclassing VipIODevice, you must call VipIODevice::setSize() at the start of your function to ensure integrity with VipIODevice's built-in size.
	virtual bool setSize(qint64 size);

	VipTimeRange timeLimits(const VipTimeRangeList& window) const;

	/// Helper function to create a QIOdevice instance for this VipIODevice.
	/// Create and open a QIOdevice based on \a mode, \a path, internal QIODevice set with VipIODevice::setDevice() and internal VipMapFileSystem set with VipIODevice::setMapFileSystem().
	/// The rules are the following:
	/// <ul>
	/// <li>If a device is already set with VipIODevice::setDevice() and opened with the right open mode, just return it</li>
	/// <li>If a VipMapFileSystem set with VipIODevice::setMapFileSystem(), use it and \a path to create the device. The device's parent is set to this.</li>
	/// <li>Create and open a new QFile. The device's parent is set to this.</li>
	/// </ul>
	/// The user must check if returned device is not nullptr.
	/// If this function succeeds, the internal device will be set to the new one.
	/// Returns a null device on error.
	QIODevice* createDevice(const QString& path, QIODevice::OpenMode mode);

	/// @brief Emit the signal timestampingFilterChanged()
	void emitTimestampingFilterChanged();
	/// @brief Emit the signal timestampingChanged()
	void emitTimestampingChanged();

private:
	void setTime(qint64 time);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipIODevice::OpenModes)
VIP_REGISTER_QOBJECT_METATYPE(VipIODevice*)

/// VipFileHandler is a VipIODevice without input/output used to manage files
/// that do not represent a sequence, a temporal device or a resource.
///
/// For instance, we want to be able to open Python files in the code editor from the Thermavip tool bar.
/// Python files do not fit within the VipIODevice architecture, but we need the "Python files (*.py)" file filter
/// when opening the file dialog manager. In this case, the VipFileHandler::open function will open the Python file
/// in the Python editor.
///
/// If you want to handle the case where a file is being dropped on a player, or to create a new player from the file,
/// use the #vipFDCreatePlayersFromProcessing dispatcher.
class VIP_CORE_EXPORT VipFileHandler : public VipIODevice
{
	Q_OBJECT
	Q_CLASSINFO("category", "File Handlers")
public:
	VipFileHandler();
	~VipFileHandler();

	/// We set the device type to Resource to avoid triggering timestamping features.
	virtual DeviceType deviceType() const { return Resource; }
	virtual VipIODevice::OpenModes supportedModes() const { return VipIODevice::ReadOnly; }
	virtual bool open(VipIODevice::OpenModes mode);

	/// Open the input file.
	virtual bool open(const QString& path, QString* error) = 0;
};
VIP_REGISTER_QOBJECT_METATYPE(VipFileHandler*)

/// @brief VipProcessingPool is VipIODevice without any inputs nor outputs, and is used to control the simultaneous playing of several VipIODevice.
///
/// VipProcessingPool should be the parent object of read only VipIODevice objects. VipProcessingPool itself is a VipIODevice which type (Temporal, Sequential or Resource) depends
/// on its children VipIODevice. The time window of a VipProcessingPool is the union of its children time windows.
///
/// Calling VipProcessingPool::nextTime, VipProcessingPool::previousTime or VipProcessingPool::closestTime will return the required time taking into account
/// all read-only opened child devices.
///
/// Calling VipProcessingPool::read will read the data at given time for all read-only opened child devices.
///
/// Disabled children VipIODevice objects are excluded from the time related functions of VipProcessingPool.
///
/// Archiving
/// ---------
///
/// If you wish to save/restore the state of a full processing pipeline, you should store it inside a VipProcessingPool object
/// by setting all VipProcessingObject's parents to the pool.
///
/// When archiving the VipProcessingPool, all its children will be archived as well, including their connection status and all
/// their properties.
///
/// VipProcessingPool ensures that all children VipProcessingObject have a unique name, which is mandatory for archiving. That's why
/// in Thermavip all processing pipelines are manipulated through a VipProcessingPool object.
///
///
class VIP_CORE_EXPORT VipProcessingPool : public VipIODevice
{
	Q_OBJECT
	Q_PROPERTY(double playSpeed READ playSpeed WRITE setPlaySpeed)
	Q_PROPERTY(int maxListSize READ maxListSize WRITE setMaxListSize)
	Q_PROPERTY(int maxListMemory READ maxListMemory WRITE setMaxListMemory)
	Q_PROPERTY(qint64 stopBeginTime READ stopBeginTime WRITE setStopBeginTime)
	Q_PROPERTY(qint64 stopEndTime READ stopEndTime WRITE setStopEndTime)
	Q_PROPERTY(bool missFramesEnabled READ missFramesEnabled WRITE setMissFramesEnabled)

	friend class PlayThread;
	friend class VipIODevice;

public:
	/// Playing flags
	enum RunModeFlag
	{
		UsePlaySpeed = 0x0001,	//! Play using the current play speed
		Repeat = 0x0002,	//! Play repeat
		UseTimeLimits = 0x0004, //! Use the current time limits
		Backward = 0x0008,	//! Play in backward
	};
	Q_DECLARE_FLAGS(RunMode, RunModeFlag)

	/// State value passed to callback functions
	enum State
	{
		StartPlaying,
		StopPlaying,
		Playing
	};
	/// @brief Callback functions called on start/stop playing and while playing
	typedef std::function<bool(State)> callback_function;
	typedef std::function<void(qint64)> read_data_function;

	/// Returns all currently available processing pools.
	/// This function is thread safe and can be called at any time.
	static QList<VipProcessingPool*> pools();
	static VipProcessingPool* findPool(const QString& name);

	VipProcessingPool(QObject* parent = nullptr);
	virtual ~VipProcessingPool();

	/// Returns all children read only VipIODevice
	const QVector<VipIODevice*>& readDevices() const;

	/// Returns the play speed (real speed == 1)
	double playSpeed() const;
	/// Returns true if playing is in progress
	bool isPlaying() const;
	/// Returns the first time limit set with #VipProcessingPool::setStopBeginTime
	qint64 stopBeginTime() const;
	/// Returns the last time limit set with #VipProcessingPool::setStopEndTime
	qint64 stopEndTime() const;

	/// @brief Enable/disable missing frames when using a play speed
	bool missFramesEnabled() const;

	/// Returns true if the VipProcessingPool has a Sequential child
	bool hasSequentialDevice() const;
	/// Returns true if the VipProcessingPool has a Temporal child
	bool hasTemporalDevice() const;

	/// Returns all children VipIODevice with given device type.
	/// If should_be_opened is true, only returns open deivces.
	QList<VipIODevice*> ioDevices(VipIODevice::DeviceType type, bool should_be_opened = true);

	/// Returns the VipProcessingPool type.
	///  If it has at least one Temporal device, the VipProcessingPool is Temporal.
	///  Otherwise, if it has at least on Sequential device, the VipProcessingPool is Sequential.
	///  Otherwise, the  VipProcessingPool is a Resource.
	virtual DeviceType deviceType() const;
	/// Returns the supported mode (a VipProcessingPool is considered as read only)
	virtual VipIODevice::OpenModes supportedModes() const { return VipIODevice::ReadOnly; }
	/// Open all children VipIODevice with given mode.
	///  Returns true is all opening succeded, false if at least one failed.
	virtual bool open(VipIODevice::OpenModes mode);
	/// Close children VipIODevice
	virtual void close();
	/// Returns all children #VipProcessingObjectList inheriting given class
	virtual VipProcessingObjectList processing(const QString& inherit_class_name = QString()) const;

	/// VipProcessingPool does not support currently timestamping filter
	virtual void setTimestampingFilter(const VipTimestampingFilter& filter);

	/// Set the playing mode
	void setModes(RunMode mode);
	void setMode(RunModeFlag, bool on = true);
	bool testMode(RunModeFlag) const;
	RunMode modes() const;

	/// Set the maximum input buffer size for all current and future processings in this pool.
	///  Set a negative value to reset the max list size (#VipDataListManager will be used)
	void setMaxListSize(int size);
	/// Set the maximum list memory footprint for all current and future processings in this pool.
	///  Set a negative value to reset the max list memory size (#VipDataListManager will be used)
	void setMaxListMemory(int memory);
	/// Set the list limit type (combination of Number and MemorySize, or None) for all current and future processings in this pool.
	///  Set a negative value to reset the limit type (#VipDataListManager will be used)
	void setListLimitType(int type);
	/// Returns the list limit type
	int listLimitType() const;
	/// Returns the maximum list size
	int maxListSize() const;
	/// Returns the maximum list memory footprint in bytes
	int maxListMemory() const;

	bool hasMaxListSize() const;
	bool hasMaxListMemory() const;
	bool hasListLimitType() const;

	/// Clear the input buffers of all children processing objects, as well as all pending apply()
	void clearInputBuffers();

	/// Save the state of the processing pool and all its children
	virtual void save();
	/// Restore the state of the processing pool and all its children
	virtual void restore();
	/// Enable/disable the processing pool and all its children
	virtual void setEnabled(bool);
	/// Reload all read only children devices
	virtual bool reload();

	virtual void setLogErrorEnabled(int error_code, bool enable);
	virtual void setLogErrors(const QSet<int>& errors);
	void resetLogErrors();
	bool hasLogErrors() const;

	int readMaxFPS() const;

	int maxReadThreadCount() const;

	/// Returns all leaf processings for this processing pool.
	///  If \a children_only is false, this function might look for processings that are not children of this processing pool.
	QList<VipProcessingObject*> leafs(bool children_only = true) const;

	/// Add a callback function that will be called on playing functions.
	/// The function is called in the playing thread:
	/// - At the begining of playing (argument is StartPlaying)
	/// - After each frame (argument is Playing)
	/// - At the end of playing (argument is StopPlaying)
	/// Returns the callback id.
	int addPlayCallbackFunction(const callback_function& callback);
	/// Remove a callback function previously registered with #addPlayCallbackFunction()
	void removePlayCallbackFunction(int);

	/// Add a callback function called before each call to #VipProcessingPool::read() member.
	/// This callback is not guaranteed to be called from the main thread.
	/// Returns the callback id.
	QObject* addReadDataCallback(const read_data_function& callback);
	/// Remove a callback function previously registered with #addReadDataCallback()
	void removeReadDataCallback(QObject*);

public Q_SLOTS:
	/// Same thing as #VipProcessingObject::read. Set the time of all children read only devices.
	bool seek(qint64 time);
	/// Try to set the position of all children read only devices.
	bool seekPos(qint64 pos);
	/// Set the play speed
	void setPlaySpeed(double speed);
	/// Set the stop begin time. By default, the processing pool does not have a stop begin time and uses firstTime() and lastTime() as time boundaries.
	void setStopBeginTime(qint64 time);
	/// Set the stop end time. By default, the processing pool does not have a stop end time and uses firstTime() and lastTime() as time boundaries.
	void setStopEndTime(qint64 time);
	/// Enable/disable time limits. This equivalent to setMode(UseTimeLimits,enable);
	void setTimeLimitsEnable(bool enable);
	/// Enable/disable play repeat. This equivalent to setMode(Repeat,enable);
	void setRepeat(bool enable);
	/// @brief Enable/disable missing frames when using a play speed
	void setMissFramesEnabled(bool);
	/// Launch playing
	void play();
	/// Launch playing in forward mode. this is equivalent to:
	/// \code
	/// setMode(Backward,false);
	/// play();
	/// \endcode
	void playForward();
	/// Launch playing in backward mode. this is equivalent to:
	/// \code
	/// setMode(Backward,true);
	/// play();
	/// \endcode
	void playBackward();
	/// Go to the first time and initialize all children processing objects
	void first();
	/// Go to the last time and initialize all children processing objects
	void last();
	/// Stop the playing
	void stop();
	/// Delete all children processing objects
	void clear();
	/// Go to the next time
	bool next();
	/// Go to the previous time
	bool previous();
	/// Stop playing and wait for all children processing objects to finish their processings
	void wait();
	/// Stop playing and wait for all children processing objects to finish their processings
	bool wait(uint msecs);

	void setReadMaxFPS(int fps);

	/// Open all children read-only devices and create processing connections.
	/// You might need to call this when adding a new processing pipeline that has just been deserialized.
	void openReadDeviceAndConnections();

	/// Enable all children processing objects except given ones
	void enableExcept(const VipProcessingObjectList& lst);
	/// Disable all children processing objects except given ones
	void disableExcept(const VipProcessingObjectList& lst);

	void setMaxReadThreadCount(int);

	qint64 closestTimeNoLimits(qint64 from_time) const;

private Q_SLOTS:
	void childTimestampingFilterChanged();
	void childTimestampingChanged();
	void emitObjectAdded(QObjectPointer);
	void checkForStreaming();

Q_SIGNALS:

	// emit these signals when a children is added or removed
	void objectAdded(QObject*);
	void objectRemoved(QObject*);

	/// Emitted when the processing pool type changed.
	/// \sa #VipIODevice::deviceType()
	void deviceTypeChanged();

	void playingStarted();
	void playingAdvancedOneFrame();
	void playingStopped();

protected:
	qint64 computeClosestTime(qint64 from_time, const VipTimeRange& time_limits) const;

	/// Returns the next valid time after \a time. Uses all children read only VipIODevice to find
	/// the closest time above  \a time.
	virtual qint64 computeNextTime(qint64 from_time) const;
	/// Returns the previous valid time before \a time. Uses all children read only VipIODevice to find
	/// the closest time under  \a time.
	virtual qint64 computePreviousTime(qint64 from_time) const;
	/// Returns the valid time closest to \a time.
	virtual qint64 computeClosestTime(qint64 time) const;
	virtual VipTimeRangeList computeTimeWindow() const;
	virtual qint64 computePosToTime(qint64 pos) const;
	virtual qint64 computeTimeToPos(qint64 time) const;
	virtual bool readData(qint64 time);
	virtual bool enableStreaming(bool);
	virtual void childEvent(QChildEvent* event);
	/// Reset all children VipProcessingObject.
	virtual void resetProcessing();

private:
	void runPlay();
	void computeChildren();
	void computeDeviceType();
	void applyLimitsToChildren();

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipProcessingPool::RunMode)
VIP_REGISTER_QOBJECT_METATYPE(VipProcessingPool*)

/// \a VipAnyResource is a read only #VipIODevice representing a unique resource of the type #VipAnyData.
/// Its device type is #VipIODevice::Resource.
class VIP_CORE_EXPORT VipAnyResource : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipOutput data)

public:
	VipAnyResource(QObject* parent = nullptr)
	  : VipIODevice(parent)
	{
		this->setOpenMode(ReadOnly);
	}

	virtual bool open(VipIODevice::OpenModes) { return true; }
	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Resource; }

	void setData(const QVariant& data)
	{
		d_data = data;
		VipAnyData out = create(d_data);
		outputAt(0)->setData(out);
	}
	const QVariant& data() const { return d_data; }

protected:
	virtual bool readData(qint64)
	{
		VipAnyData out = create(d_data);
		outputAt(0)->setData(out);
		return true;
	}

private:
	QVariant d_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipAnyResource*)

VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipIODevice* d);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipIODevice* d);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipAnyResource* d);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipAnyResource* d);
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipProcessingPool* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipProcessingPool* r);

/// \a VipTimeRangeBasedGenerator is a helper base class for Temporal #VipIODevice having a notion of size and position.
/// Use #VipTimeRangeBasedGenerator::setTimeWindows or #VipTimeRangeBasedGenerator::setTimestamps in the derive class to generate the time window.
class VIP_CORE_EXPORT VipTimeRangeBasedGenerator : public VipIODevice
{
	Q_OBJECT
	friend VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipTimeRangeBasedGenerator* r);
	friend VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipTimeRangeBasedGenerator* r);

public:
	VipTimeRangeBasedGenerator(QObject* parent = nullptr);
	~VipTimeRangeBasedGenerator();

	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Temporal; }

	/// Generate the device time window based on the \a start time, the number of samples (\a size) and a \a sampling time.
	///  This function should be called in the #VipIODevice::open overload of derived class.
	void setTimeWindows(qint64 start, qint64 size, qint64 sampling);

	/// Generate the device time window based on a time range and number of samples.
	///  This function should be called in the #VipIODevice::open overload of derived class.
	void setTimeWindows(const VipTimeRange& range, qint64 size);

	/// Generate the device time window based on a list of time \a ranges and a \a sampling time.
	///  This function should be called in the #VipIODevice::open overload of derived class.
	void setTimeWindows(const VipTimeRangeList& ranges, qint64 sampling);

	/// Generate the device time window using the timestamps of each samples.
	///  This function should be called in the #VipIODevice::open overload of derived class.
	///  If \a enable_multiple_time_range is true, this function will compute the minimum sampling time
	///  based on \a timestamps, and might create multiple time ranges if the time between 2 consecutive
	///  images is way above this sampling time.
	void setTimestamps(const QVector<qint64>& timestamps, bool enable_multiple_time_range = true);

	void setTimestampsWithSampling(const QVector<qint64>& timestamps, qint64 sampling);

	/// Returns the sampling time
	qint64 samplingTime() const;

	/// Returns the timestamps of each sample if #VipTimeRangeBasedGenerator::setTimestamps has been used.
	const QVector<qint64>& timestamps() const;

protected:
	virtual qint64 computePosToTime(qint64 pos) const;
	virtual qint64 computeTimeToPos(qint64 time) const;
	virtual VipTimeRangeList computeTimeWindow() const;

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipTimeRangeBasedGenerator*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipTimeRangeBasedGenerator* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipTimeRangeBasedGenerator* r);

/// \a VipTextFileReader is a read only Temporal device that reads text (ascii) files.
/// The content of the file can be interpreted as:
/// - A sequence of images (row major) with each row separated by a newline, and each image separated with an empty line.
/// - A sequence of curves organized in columns (X1 Y1 ... Xn Yn), and separated by empty line.
/// - A sequence of curves organized in columns (X1 Y1 Y2... Yn), and separated by empty lines
/// - A sequence of curves organized in rows (X1 | Y1 |...| Xn | Yn), and separated by empty lines
/// - A sequence of curves organized in rows (X1 | Y1 |...| Yn), and separated by empty lines
class VIP_CORE_EXPORT VipTextFileReader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipMultiOutput output)
	Q_CLASSINFO("description", "Read a text file and interpret it as a serie of images or curves")
	Q_CLASSINFO("category", "reader")

public:
	/// The file type
	enum Type
	{
		Unknown,
		Image,
		XYXYColumn,
		XYYYColumn,
		XYXYRow,
		XYYYRow
	};

	VipTextFileReader(QObject* parent = nullptr);
	~VipTextFileReader();

	/// Set the sampling time (time between each sample separated by empty lines)
	void setSamplingTime(qint64 time);
	/// Returns the sampling time
	qint64 samplingTime() const;

	/// Set the file type. Call this function before calling #VipIODevice::open. Default type is \a Image.
	void setType(Type type);
	/// Returns the file type
	Type type() const;

	virtual void copyParameters(VipProcessingObject* dst)
	{
		if (VipTextFileReader* d = qobject_cast<VipTextFileReader*>(dst)) {
			VipTimeRangeBasedGenerator::copyParameters(dst);
			d->setSamplingTime(samplingTime());
			d->setType(type());
		}
	}

	virtual DeviceType deviceType() const;
	virtual bool probe(const QString& filename, const QByteArray& first_bytes) const;
	virtual bool open(VipIODevice::OpenModes mode);
	virtual void close();
	virtual bool reload();

	virtual QString fileFilters() const { return "Raw images or plots (*.txt)"; }

protected:
	virtual bool readData(qint64 time);

private:
	qint64 m_samplingTime;
	QList<VipNDArray> m_arrays;
	Type m_type;
};

VIP_REGISTER_QOBJECT_METATYPE(VipTextFileReader*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipTextFileReader* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipTextFileReader* r);

/// \a VipTextFileWriter is a write only device used to write any kind of data in text (ascii) files.
/// For each new input data (inputAt(0)->setData()), the behavior can be one of the following:
/// - Overwrite the current file with the new data (#VipTextFileWriter::ReplaceFile)
/// - Append the data in the current file with an empty line separator (#VipTextFileWriter::StackData)
/// - Create a new file (in the same directory) containing the data with an incremented filename (#VipTextFileWriter::MultipleFiles)
///
/// To write a VipAnyData in the text file, the QVariant data (#VipAnyData::data) must be convertible to QString. Only the data is written,
/// not the time, attributes and so on.
class VIP_CORE_EXPORT VipTextFileWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)
	Q_CLASSINFO("description", "Write into text file a serie of images or curves")
	Q_CLASSINFO("category", "writer")
public:
	/// Saving type
	enum Type
	{
		ReplaceFile,  //! each new input data replace the previously saved one
		StackData,    //! each new data is directly appended into the same file
		MultipleFiles //! each new data is saved in the same directory with an incremented filename
	};

	VipTextFileWriter(QObject* parent = nullptr);
	~VipTextFileWriter();

	/// Set the saving type
	void setType(Type);
	/// Returns the saving type
	Type type() const;

	/// For #VipTextFileWriter::MultipleFiles, each new filename ends with an incremented value (starting 0) defined on given number of digits.
	void setDigitsNumber(int num);
	/// Returns the number of digits for #VipTextFileWriter::MultipleFiles.
	int digitsNumber() const;

	virtual void copyParameters(VipProcessingObject* dst)
	{
		if (VipTextFileWriter* d = qobject_cast<VipTextFileWriter*>(dst)) {
			VipIODevice::copyParameters(dst);
			d->setDigitsNumber(digitsNumber());
			d->setType(type());
		}
	}

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int, const QVariant& v) const { return v.canConvert<QString>(); }
	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Raw images or plots (*.txt)"; }
	virtual void close();

	static QString formatDigit(int num, int digits);
	static QString nextFileName(const QString& path, int& number, int digits); // for multiple files only
protected:
	virtual void apply();

private:
	int m_number;
	int m_digits;
	Type m_type;
};

VIP_REGISTER_QOBJECT_METATYPE(VipTextFileWriter*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipTextFileWriter* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipTextFileWriter* r);

/// \a is a read only Temporal device able to read most of the standard image formats (PNG, BMP, JPEG,...).
/// It can also read stacked images in the same file.
class VIP_CORE_EXPORT VipImageReader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	Q_CLASSINFO("description", "Read an standard image file. Multiple images can be stacked in a single file.")
	Q_CLASSINFO("category", "reader")

public:
	VipImageReader(QObject* parent = nullptr);
	~VipImageReader();

	/// Set the sampling time between each image (if the file contains more than 1 image)
	void setSamplingTime(qint64 time);
	/// Returns the sampling time
	qint64 samplingTime() const;

	virtual void copyParameters(VipProcessingObject* dst)
	{
		if (VipImageReader* d = qobject_cast<VipImageReader*>(dst)) {
			VipTimeRangeBasedGenerator::copyParameters(dst);
			d->setSamplingTime(samplingTime());
		}
	}

	virtual DeviceType deviceType() const;
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool open(VipIODevice::OpenModes mode);
	virtual bool reload();

	virtual QString fileFilters() const { return "Image file (*.png *.bmp *.jpg *.jpeg *.gif *.giff *.pbm *.pgm *.ppm *.xpm *.xbm *.svg)"; }

protected:
	virtual bool readData(qint64 time);

private:
	qint64 m_samplingTime;
	QList<VipNDArray> m_arrays;
};

VIP_REGISTER_QOBJECT_METATYPE(VipImageReader*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipImageReader* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipImageReader* r);

/// A write only device used to create image files of most of the standard image formats (PNG, BMP, JPEG,...).
/// It can stack multiple images in the same file.
///
/// Like #VipTextFileWriter, 3 different behaviors are possible for each new image:
/// - Overwrite the current file with the new image (#VipImageWriter::ReplaceImage)
/// - Append the image in the current file (#VipImageWriter::StackImages)
/// - Create a new file (in the same directory) containing the data with an incremented filename (#VipImageWriter::MultipleImages)
///
/// Only the image is saved, not the image time, attributes and so on.
class VIP_CORE_EXPORT VipImageWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)
	Q_CLASSINFO("description", "Write one or more images into a file using a standard image format")
	Q_CLASSINFO("category", "writer")

public:
	/// The saving type
	enum Type
	{
		ReplaceImage,  //! each new input image replace the previously saved one
		StackImages,   //! each new image is directly appended into the same file
		MultipleImages //! each new image is saved in the same directory with an incremented filename
	};

	VipImageWriter(QObject* parent = nullptr);
	~VipImageWriter();

	/// Set the saving type
	void setType(Type);
	/// Returns the saving type
	Type type() const;

	/// For #VipImageWriter::MultipleImages, set the digit number (see also #VipTextFileWriter::setDigitsNumber)
	void setDigitsNumber(int num);
	/// Returns the digit number used with #VipImageWriter::MultipleImages
	int digitsNumber() const;

	virtual void copyParameters(VipProcessingObject* dst)
	{
		if (VipImageWriter* d = qobject_cast<VipImageWriter*>(dst)) {
			VipIODevice::copyParameters(dst);
			d->setType(type());
			d->setDigitsNumber(digitsNumber());
		}
	}

	virtual bool acceptInput(int, const QVariant& v) const
	{
		if (v.userType() == qMetaTypeId<VipNDArray>()) {
			VipNDArray ar = v.value<VipNDArray>();
			return vipIsImageArray(ar);
		}
		return false;
	}

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Image file (*.png *.bmp *.jpg *.jpeg *.ppm *.tiff *.tif *.xbm *.xpm)"; }
	virtual void close();

protected:
	virtual void apply();

private:
	int m_number;
	int m_digits;
	Type m_type;
};

VIP_REGISTER_QOBJECT_METATYPE(VipImageWriter*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipImageWriter* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipImageWriter* r);

/// Read a CSV file saved with VIPCSVWriter
class VIP_CORE_EXPORT VipCSVReader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipMultiOutput output)
	Q_CLASSINFO("description", "Read CSV file containing one or more signals")
	Q_CLASSINFO("category", "reader")

public:
	VipCSVReader(QObject* parent = nullptr);
	~VipCSVReader();

	virtual DeviceType deviceType() const { return Resource; }
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool open(VipIODevice::OpenModes mode);

	virtual QString fileFilters() const { return "CSV file (*.csv)"; }

protected:
	virtual bool readData(qint64 time);

private:
	QList<VipAnyData> m_signals;
};

VIP_REGISTER_QOBJECT_METATYPE(VipCSVReader*)

/// Write-only device that writes one or more VipPointVector in a CSV file (comma separated values).
/// If more than one VipPointVector is given as input, the vectors are resampled using #vipResampleTemporalVectors.
class VIP_CORE_EXPORT VipCSVWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipMultiInput inputs)
	Q_CLASSINFO("description", "Write one or more signals into a CSV file")
	Q_CLASSINFO("category", "writer")

public:
	VipCSVWriter(QObject* parent = nullptr);
	~VipCSVWriter();

	/// Set whether the output should be formatted as a raw text file:
	/// no header, no separator specifier and use dot to represent float number instead of comma.
	void setWriteTextFile(bool);
	bool writeTextFile() const;

	void setResampleMode(int r);
	int resampleMode() const;

	void setPaddValue(double value);
	double paddValue() const;

	virtual bool acceptInput(int, const QVariant& v) const { return v.userType() == qMetaTypeId<VipPointVector>(); }

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "CSV file (*.csv)"; }

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipCSVWriter*)

class VIP_CORE_EXPORT VipDirectoryReader : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipMultiOutput output)
	Q_CLASSINFO("description", "Read all files in a directory")
	Q_CLASSINFO("category", "reader")

public:
	/// Type of the reader.
	/// If set to SequenceOfData, all the files in the directory are considered as a temporal sequence of data (images or other data types).
	/// Otherwise, all files are considered as independent data files, the device type becomes 'Resource' and the VipMultiOutput is resized to the number of files.
	enum Type
	{
		SequenceOfData,
		IndependentData
	};

	VipDirectoryReader(QObject* parent = nullptr);
	~VipDirectoryReader();

	void setSupportedSuffixes(const QStringList& suffixes);
	void setSupportedSuffixes(const QString& suffixes);
	void setFixedSize(const QSize& s);
	void setFileCount(qint32 c = -1);
	void setFileStart(qint32 s = 0);
	void setSmoothResize(bool smooth = false);
	void setAlphabeticalOrder(bool order = true);
	void setType(Type);
	void setRecursive(bool);

	QStringList supportedSuffixes() const;
	QSize fixedSize() const;
	qint32 fileCount() const;
	qint32 fileStart() const;
	bool smoothResize() const;
	bool alphabeticalOrder() const;
	Type type() const;
	bool recursive() const;

	QStringList files() const;
	QStringList suffixes() const;

	void setSuffixTemplate(const QString& suffix, VipIODevice* device);

	/// For IndependantData only, returns the VipIODevice corresponding to a given output index
	VipIODevice* deviceFromOutput(int output_index) const;
	VipIODevice* deviceAt(int index) const;
	int deviceCount() const;

	virtual void copyParameters(VipProcessingObject* dst)
	{
		if (VipDirectoryReader* d = qobject_cast<VipDirectoryReader*>(dst)) {
			VipIODevice::copyParameters(dst);
			d->setSupportedSuffixes(supportedSuffixes());
			d->setFixedSize(fixedSize());
			d->setFileCount(fileCount());
			d->setFileStart(fileStart());
			d->setSmoothResize(smoothResize());
			d->setAlphabeticalOrder(alphabeticalOrder());
			d->setType(type());
			d->setRecursive(recursive());
		}
	}

	virtual void setSourceProperty(const char* name, const QVariant& value);

	virtual DeviceType deviceType() const;
	virtual bool setPath(const QString& dirname);
	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual bool probe(const QString& filename, const QByteArray&) const;
	virtual void close();
	virtual bool open(VipIODevice::OpenModes mode);
	virtual bool reload();

public Q_SLOTS:
	void recomputeTimestamps();

protected:
	virtual qint64 computeNextTime(qint64 time) const;
	virtual qint64 computePreviousTime(qint64 time) const;
	virtual qint64 computeClosestTime(qint64 time) const;
	virtual bool readData(qint64);
	virtual VipTimeRangeList computeTimeWindow() const;

private:
	void computeFiles();
	int closestDeviceIndex(qint64 time, qint64* closest = nullptr) const;
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
	// directory options
};

VIP_REGISTER_QOBJECT_METATYPE(VipDirectoryReader*)

class VIP_CORE_EXPORT VipShapeReader : public VipAnyResource
{
	Q_OBJECT
	Q_CLASSINFO("description", "Read a scene model from a XML or JSON file")
	Q_CLASSINFO("category", "reader")
public:
	VipShapeReader();
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);
	virtual QString fileFilters() const { return "ROI file (*.xml *.json)"; }
};

VIP_REGISTER_QOBJECT_METATYPE(VipShapeReader*)

class VIP_CORE_EXPORT VipShapeWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput scene_model)
	Q_CLASSINFO("description", "Write a scene model in a XML or JSON file")
	Q_CLASSINFO("category", "writer")
public:
	VipShapeWriter();
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);
	virtual QString fileFilters() const { return "ROI file (*.xml *.json)"; }

	virtual bool acceptInput(int, const QVariant& v) const { return v.canConvert<VipSceneModel>() || v.canConvert<VipSceneModelList>(); }
	virtual DeviceType deviceType() const { return Resource; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipShapeWriter*)


#ifdef VIP_WITH_HDF5

/// \a VipArchiveRecorder is a write only device designed to save any kind of input data in a binary format.
///
/// \a VipArchiveRecorder internally relies on a #VipBinaryArchive which basically contains stacked raw #VipAnyData. Therefore, it can save any serializable data (see #VipArchive for more details).
/// It saves the data itself along with its attributes, time, and all other informations contained in a #VipAnyData.
///
/// A file saved with \a VipArchiveRecorder always ends with a trailer structure giving informations about the number of stream and the time window.
///
/// \a VipArchiveRecorder is always a good choice to save any kind of data since it saves all associated metadata. The data can be read back in an identical shape
/// using a #VipArchiveReader.
///
/// An archive saved with \a VipArchiveRecorder has the extension .arch.
class VIP_CORE_EXPORT VipArchiveRecorder : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipMultiInput inputs)
	Q_CLASSINFO("description", "Record any type of input data into a single binary file (.arch)")
	Q_CLASSINFO("category", "writer")

public:
	/// The trailer structure saved at the end of the file
	struct Trailer
	{
		struct Source
		{
			QByteArray typeName;
			VipTimeRange limits;
			qint64 samples;
		};
		QMap<qint64, Source> sources;		  // the data type name for each source
		qint64 startTime;				  // the global start time
		qint64 endTime;					  // the global end time
		Trailer()
		  : startTime(VipInvalidTime)
		  , endTime(VipInvalidTime)
		{
		}
	};

	VipArchiveRecorder(QObject* parent = nullptr);
	~VipArchiveRecorder();

	virtual bool probe(const QString& filename, const QByteArray&) const { return QFileInfo(filename).suffix().compare("arch", Qt::CaseInsensitive) == 0 || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual QString fileFilters() const { return "Signal archive file (*.arch)"; }
	virtual void close();

	Trailer trailer() const;

protected:
	virtual void apply();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipArchiveRecorder*)

/// \a VipArchiveReader is a Temporal read only device designed to read archives saved with #VipArchiveRecorder (*.arch files).
///
/// The number of outputs (or streams) is set in the #VipArchiveReader::open function.
///
/// \a VipArchiveReader is very fast when reading the data one after the other in forward or backward mode. However, it can be slow
/// when seeking in the middle of the file.
class VIP_CORE_EXPORT VipArchiveReader : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipMultiOutput outputs)
	Q_CLASSINFO("description", "Read a binary archive (.arch) containing any number of data stream")
public:
	VipArchiveReader(QObject* parent = nullptr);
	~VipArchiveReader();

	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const;
	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual bool probe(const QString& filename, const QByteArray& first_bytes) const;
	virtual QString fileFilters() const { return "Signal archive file (*.arch)"; }
	virtual void close();
	virtual bool reload();

	VipArchiveRecorder::Trailer trailer() const;

protected:
	virtual qint64 computeNextTime(qint64 time) const;
	virtual qint64 computePreviousTime(qint64 time) const;
	virtual qint64 computeClosestTime(qint64 time) const;
	virtual bool readData(qint64);
	virtual VipTimeRangeList computeTimeWindow() const;

private Q_SLOTS:
	void bufferData();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipArchiveReader*)

#endif

/// @}
// end Core

#endif
