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

#include <QChildEvent>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSet>
#include <QSharedPointer>
#include <QTextStream>
#include <QTimer>

#include <cmath>
#include <unordered_set>

#include "VipCore.h"
#include "VipDataType.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipProgress.h"
#include "VipSet.h"
#include "VipSleep.h"
#include "VipTextOutput.h"
#include "VipHash.h"

// class ReadThread : public QThread
// {
// VipIODevice * device;
// QList< QList<VipAnyData> > buffers;
// qint64 time;
// bool forward;
// QMutex mutex;
// QMutex read_mutex;
//
// public:
// ReadThread() : device(nullptr), time(VipInvalidTime), forward(true)
// {}
//
// void start(VipIODevice * dev)
// {
// stop();
// device = dev;
// QThread::start();
// }
//
// void stop()
// {
// device = nullptr;
// wait();
// }
//
// void push(const VipAnyData & any, int output_index)
// {
// if (!any.isEmpty())
// {
//	QMutexLocker lock(&mutex);
//	while (buffers.size() <= output_index)
//		buffers.append(QList<VipAnyData>());
//	buffers[output_index].append(any);
//	if (buffers[output_index].size() > 2)
//	{
//		buffers[output_index].pop_front();
//	}
// }
// }
//
// VipAnyData pop(qint64 t, int output_index)
// {
// QMutexLocker lock(&mutex);
// if (buffers.size() <= output_index)
//	return VipAnyData();
//
// QList<VipAnyData> & lst = buffers[output_index];
// for (int i = 0; i < lst.size(); ++i)
// {
//	if (lst[i].time() == t)
//	{
//		VipAnyData res = lst[i];
//		lst.removeAt(i);
//		return res;
//	}
// }
//
// //compute the new strategy
// forward = (t > time || time == VipInvalidTime);
//
// for (int o = 0; o < buffers.size(); ++o)
// {
//	QList<VipAnyData> & lst = buffers[o];
//	for (int i = 0; i < lst.size(); ++i)
//	{
//		if (forward && lst[i].time() <= t)
//		{
//			lst.removeAt(i); --i;
//		}
//		else if (!forward && lst[i].time() >= t)
//		{
//			lst.removeAt(i); --i;
//		}
//	}
// }
//
// time = t;
//
// return VipAnyData();
// }
//
// virtual void run()
// {
// while (VipIODevice * dev = device)
// {
//	//only work for read only opened temporal devices
//	if ((dev->openMode() & VipIODevice::ReadOnly) && dev->deviceType() == VipIODevice::Temporal)
//	{
//		//do nothing if a buffer has a size == 2
//		bool full_buffer = false;
//		{
//			QMutexLocker lock(&mutex);
//			for (int o = 0; o < buffers.size(); ++o)
//			{
//				QList<VipAnyData> & lst = buffers[o];
//				if (lst.size() == 2)
//				{
//					full_buffer = true;
//					break;
//				}
//			}
//		}
//		if (full_buffer)
//		{
//			QThread::msleep(2);
//			continue;
//		}
//
//		//read the next or previous data data
//
//		QMutexLocker lock(&read_mutex);
//		if (time != VipInvalidTime)
//		{
//			qint64 new_time = forward ? device->nextTime(time) : device->previousTime(time);
//			QMap<int, VipAnyData> data;
//		}
//
//	}
// }
// }
// };

struct LockBool
{
	bool* value;
	LockBool(bool* v)
	  : value(v)
	{
		*v = true;
	}
	~LockBool() { *value = false; }
};

class VipIODevice::PrivateData
{
public:
	struct Parameters
	{
		VipTimestampingFilter filter;
		bool streamingEnabled;
		Parameters(const VipTimestampingFilter& filter = VipTimestampingFilter(), bool streamingEnabled = false)
		  : filter(filter)
		  , streamingEnabled(streamingEnabled)
		{
		}
	};

	PrivateData()
	  : mode(NotOpen)
	  , size(VipInvalidPosition)
	  , device(nullptr)
	  , readTime(VipInvalidTime)
	  , lastReadTime(0)
	  , elapsed_time(0)
	  , is_reading(false)
	  , lastTimeValid(true)
	{
	}

	QString path;
	VipIODevice::OpenModes mode;
	qint64 size;
	QPointer<QIODevice> device;
	VipMapFileSystemPtr map;
	qint64 readTime;
	qint64 lastReadTime;

	// measure frame rate
	qint64 elapsed_time;
	bool is_reading;
	bool lastTimeValid;

	Parameters parameters;
	QList<Parameters> savedParameters;

	QRecursiveMutex readMutex;
};

VipIODevice::VipIODevice(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipIODevice::~VipIODevice()
{
	VipIODevice::close();
	emitDestroyed();
}

void VipIODevice::close()
{
	// for write only devices, wait for the input data to be consumed
	if (openMode() & WriteOnly) {
		bool is_enabled = isEnabled();
		setEnabled(false);
		wait(false);
		this->setOpenMode(VipIODevice::NotOpen);
		setEnabled(is_enabled);
	}
	else {
		setStreamingEnabled(false);
		this->setOpenMode(VipIODevice::NotOpen);
	}

	if (device()) {
		device()->close();
		if (device()->parent() == this && this->thread() == QThread::currentThread())
			delete device();
		setDevice(nullptr);
	}
	d_data->size = 0;
}

void VipIODevice::save()
{
	VipProcessingObject::save();
	d_data->savedParameters.append(PrivateData::Parameters(this->timestampingFilter(), this->isStreamingEnabled()));
}

VipIODevice::OpenModes VipIODevice::openMode() const
{
	return d_data->mode;
}

QString VipIODevice::path() const
{
	return d_data->path;
}

QIODevice* VipIODevice::device() const
{
	return d_data->device.data();
}

void VipIODevice::setDevice(QIODevice* device)
{
	d_data->device = device;
}

VipMapFileSystemPtr VipIODevice::mapFileSystem() const
{
	return d_data->map;
}

void VipIODevice::setMapFileSystem(const VipMapFileSystemPtr& map)
{
	d_data->map = map;
}

QString VipIODevice::removePrefix(const QString& path, const QString& prefix)
{
	QString m_path(path);
	int pos = path.indexOf(prefix);
	if (pos == 0) {
		m_path = path.mid(prefix.length() + 1, path.length());
	}

	return m_path;
}

qint64 VipIODevice::size() const
{
	if (deviceType() == Sequential)
		return VipInvalidPosition;
	else if (deviceType() == Resource)
		return 1;
	else
		return d_data->size;
}

bool VipIODevice::setSize(qint64 size)
{
	if ((deviceType() == Sequential) || (deviceType() == Resource))
		return false;
	else
		d_data->size = size;
	return true;
}

void VipIODevice::setTime(qint64 time)
{
	if (d_data->readTime != time) {
		d_data->readTime = time;
		Q_EMIT timeChanged(time);
	}
}

void VipIODevice::emitTimestampingFilterChanged()
{
	Q_EMIT timestampingFilterChanged();
}

void VipIODevice::emitTimestampingChanged()
{
	Q_EMIT timestampingChanged();
}

QIODevice* VipIODevice::createDevice(const QString& path, QIODevice::OpenMode mode)
{
	if (device() && device()->openMode() == mode) {
		return device();
	}
	else if (mapFileSystem()) {
		QIODevice* dev = mapFileSystem()->open(VipPath(path), mode);
		if (dev && dev->isOpen()) {
			// be sure that the device has the same thread as the parent VipIODevice, or CRASH!
			if (QThread::currentThread() != this->thread())
				dev->moveToThread(this->thread());

			dev->setParent(const_cast<VipIODevice*>(this));
			setDevice(dev);
			return dev;
		}
		else if (dev)
			delete dev;
	}
	else {
		QFile* file = new QFile(path);
		// be sure that the device has the same thread as the parent VipIODevice, or CRASH!
		if (QThread::currentThread() != this->thread())
			file->moveToThread(this->thread());
		file->setParent(this);
		if (file->open(mode)) {
			setDevice(file);
			return file;
		}
		else
			delete file;
	}
	return nullptr;
}

bool VipIODevice::setPath(const QString& path)
{
	d_data->path = this->removePrefix(path);
	setAttribute("Name", QFileInfo(path).fileName());
	return true;
}

void VipIODevice::setTimestampingFilter(const VipTimestampingFilter& filter)
{
	d_data->parameters.filter = filter;
	d_data->parameters.filter.setInputTimeRangeList(this->computeTimeWindow());
	// if (filter.isEmpty())
	//	this->setProperty("_vip_timestampingFilter", QVariant());
	// else
	//	this->setProperty("_vip_timestampingFilter", QVariant::fromValue(filter));
	emitTimestampingFilterChanged();
}

void VipIODevice::resetTimestampingFilter()
{
	if (!d_data->parameters.filter.isEmpty()) {
		// this->setProperty("_vip_timestampingFilter", QVariant());
		d_data->parameters.filter.reset();
		emitTimestampingFilterChanged();
	}
}

qint64 VipIODevice::transformTime(qint64 time, bool* inside, bool* exact_time) const
{
	qint64 res = time;
	if (inside)
		*inside = true;
	if (exact_time)
		*exact_time = true;

	if (!d_data->parameters.filter.isEmpty() && time != VipInvalidTime)
		res = d_data->parameters.filter.transform(time, inside);
	else if (time != VipInvalidTime) {
		// case no filter: we must return a valid time
		res = computeClosestTime(time);
		if (inside)
			*inside = vipIsInside(this->computeTimeWindow(), time);
		if (exact_time)
			*exact_time = (res == time);
	}
	return res;
}

qint64 VipIODevice::invTransformTime(qint64 time, bool* inside, bool* exact_time) const
{
	qint64 res = time;
	if (inside)
		*inside = true;
	if (exact_time)
		*exact_time = true;

	if (!d_data->parameters.filter.isEmpty() && time != VipInvalidTime)
		res = d_data->parameters.filter.invTransform(time, inside);
	else if (time != VipInvalidTime) {
		// case no filter: we must return a valid time
		res = computeClosestTime(time);
		if (inside)
			*inside = vipIsInside(this->computeTimeWindow(), time);
		if (exact_time) {
			*exact_time = (res == time);
		}
	}
	return res;
}

const VipTimestampingFilter& VipIODevice::timestampingFilter() const
{
	return d_data->parameters.filter;
}

VipTimeRangeList VipIODevice::timeWindow() const
{
	if (!d_data->parameters.filter.isEmpty())
		return d_data->parameters.filter.outputTimeRangeList();
	else
		return computeTimeWindow();
}

qint64 VipIODevice::firstTime() const
{
	const VipTimeRangeList lst = timeWindow();
	if (lst.size())
		return lst.first().first;
	return VipInvalidTime;
}

qint64 VipIODevice::lastTime() const
{
	const VipTimeRangeList lst = timeWindow();
	if (lst.size())
		return lst.last().second;
	return VipInvalidTime;
}

VipTimeRange VipIODevice::timeLimits(const VipTimeRangeList& window) const
{
	const VipTimeRangeList lst = window;
	if (lst.size())
		return VipTimeRange(lst.first().first, lst.last().second);
	return VipTimeRange(VipInvalidTime, VipInvalidTime);
}

VipTimeRange VipIODevice::timeLimits() const
{
	const VipTimeRangeList lst = timeWindow();
	if (lst.size())
		return VipTimeRange(lst.first().first, lst.last().second);
	return VipTimeRange(VipInvalidTime, VipInvalidTime);
}

qint64 VipIODevice::posToTime(qint64 pos) const
{
	if (pos < 0)
		pos = 0;
	else if (pos >= size())
		pos = size() - 1;
	return transformTime(computePosToTime(pos));
}

qint64 VipIODevice::timeToPos(qint64 time) const
{
	VipTimeRange range = timeLimits();
	if (time < range.first)
		time = range.first;
	else if (time > range.second)
		time = range.second;
	time = invTransformTime(time);
	return computeTimeToPos(time);
}

qint64 VipIODevice::estimateSamplingTime() const
{
	qint64 first = this->firstTime();
	qint64 next = this->nextTime(first);
	if (first == VipInvalidTime || next == VipInvalidTime || first == next)
		return VipInvalidTime;
	return next - first;
}

qint64 VipIODevice::nextTime(qint64 time) const
{
	time = invTransformTime(time);
	time = computeClosestTime(time);
	time = computeNextTime(time);
	return transformTime(time);
}

qint64 VipIODevice::previousTime(qint64 time) const
{
	time = invTransformTime(time);
	time = computeClosestTime(time);
	time = computePreviousTime(time);
	return transformTime(time);
}

qint64 VipIODevice::closestTime(qint64 time) const
{
	time = invTransformTime(time);
	time = computeClosestTime(time);
	return transformTime(time);
}

qint64 VipIODevice::time() const
{
	if (d_data->readTime == VipInvalidTime) {
		return firstTime();
	}
	return d_data->readTime;
}

qint64 VipIODevice::processingTime() const
{
	return d_data->elapsed_time * 1000000;
}

bool VipIODevice::reload()
{
	if (!isOpen() || !isEnabled())
		return false;

	QMutexLocker locker(&d_data->readMutex);
	LockBool lock(&d_data->is_reading);

	if (deviceType() == Resource) {
		d_data->lastReadTime = vipGetMilliSecondsSinceEpoch();
		bool res = readData(time());
		d_data->elapsed_time = vipGetMilliSecondsSinceEpoch() - d_data->lastReadTime;
		return res;
	}
	else if (deviceType() == Sequential) {
		if (this->parentObjectPool() && this->parentObjectPool()->isStreamingEnabled())
			return false;
		// for sequential device, just reset its outputs
		for (int i = 0; i < outputCount(); ++i)
			outputAt(i)->setData(outputAt(i)->data());
		return outputCount() > 0;
	}

	qint64 t = time();
	if (t == VipInvalidTime)
		t = firstTime();

	// read the data
	d_data->lastReadTime = vipGetMilliSecondsSinceEpoch();
	bool res = readData(invTransformTime(t));
	d_data->elapsed_time = vipGetMilliSecondsSinceEpoch() - d_data->lastReadTime;
	return res;
}

bool VipIODevice::isReading() const
{
	return d_data->is_reading;
}

bool VipIODevice::readCurrentData()
{
	if (deviceType() == Sequential)
		return read(vipGetNanoSecondsSinceEpoch());
	return false;
}

qint64 VipIODevice::lastProcessingTime() const
{
	return d_data->lastReadTime;
}

bool VipIODevice::readInvalidTime(qint64)
{
	return false;
}

bool VipIODevice::read(qint64 time, bool force)
{
	if (!isOpen() || !isEnabled())
		return false;

	QMutexLocker locker(&d_data->readMutex);
	LockBool lock(&d_data->is_reading);

	qint64 current_time = vipGetMilliSecondsSinceEpoch();

	if (deviceType() == Resource) {
		return readData(time);
	}
	else if (deviceType() == Sequential) {
		if (time != d_data->readTime) {
			d_data->readTime = time;
			d_data->lastReadTime = current_time;
			// read the data, only emit the timeChanged() signal if a data has been read
			bool res = readData(time);
			d_data->elapsed_time = vipGetMilliSecondsSinceEpoch() - current_time;
			if (res) {
				Q_EMIT timeChanged(time);
				return true;
			}
			return false;
		}
		return false;
	}

	// Temporal device:

	if (time == VipInvalidTime)
		return false;

	const VipTimeRangeList lst = computeTimeWindow();
	if (lst.isEmpty())
		return false;

	bool inside = true, exact_time = true;
	qint64 time_transform = time;
	time = invTransformTime(time, &inside, &exact_time);
	qint64 closest = computeClosestTime(time);

	if (!exact_time || !inside) {
		bool _inside;
		qint64 real_time = d_data->parameters.filter.invTransform(time_transform, &_inside);
		if (readInvalidTime(real_time))
			return true;
	}

	VipTimeRange r(lst.first().first, lst.last().second);
	if ((r.first != VipInvalidTime && closest < r.first) || (r.second != VipInvalidTime && closest > r.second)) {
		return false;
	}

	time = closest;
	if (time_transform != d_data->readTime || force) {
		d_data->readTime = time_transform;
		d_data->lastReadTime = current_time;
		Q_EMIT timeChanged(time_transform);

		// read the data
		bool res = readData(time);
		d_data->elapsed_time = vipGetMilliSecondsSinceEpoch() - current_time;
		return res;
	}

	return true;
}

bool VipIODevice::setStreamingEnabled(bool enable)
{
	if (!isOpen())
		return false;

	if (enable != d_data->parameters.streamingEnabled) {
		if (enableStreaming(enable)) {
			d_data->parameters.streamingEnabled = enable;
			if (enable)
				Q_EMIT streamingStarted();
			else
				Q_EMIT streamingStopped();
			Q_EMIT streamingChanged(enable);
		}
	}
	return d_data->parameters.streamingEnabled == enable;
}

bool VipIODevice::startStreaming()
{
	return setStreamingEnabled(true);
}

bool VipIODevice::stopStreaming()
{
	return setStreamingEnabled(false);
}

bool VipIODevice::supportFilename(const QString& fname) const
{
	QString suffix = QFileInfo(fname).suffix();
	if (suffix.isEmpty() || fname.isEmpty())
		return false;
	QRegExp exp("\\b" + suffix + "\\b", Qt::CaseInsensitive);
	return exp.indexIn(this->fileFilters()) >= 0;
}

bool VipIODevice::isStreamingEnabled() const
{
	return d_data->parameters.streamingEnabled;
}

static std::unordered_set<int> _unregistered_ids;
static VipSpinlock _unregistered_lock;
static bool isUnregistered(int id)
{
	VipUniqueLock<VipSpinlock> lock(_unregistered_lock);
	return _unregistered_ids.find(id) != _unregistered_ids.end();
}

void VipIODevice::unregisterDeviceForPossibleReadWrite(int id)
{
	if (id <= 0)
		return;

	QObject* obj = vipCreateVariant(id).value<QObject*>();
	if (obj) {
		if (!qobject_cast<VipIODevice*>(obj)) {
			delete obj;
			return;
		}
		delete obj;

		VipUniqueLock<VipSpinlock> lock(_unregistered_lock);
		_unregistered_ids.insert(id);
	}
}

QList<VipProcessingObject::Info> VipIODevice::possibleReadDevices(const VipPath& path, const QByteArray& first_bytes, const QVariant& out_value)
{
	QMultiMap<QString, VipProcessingObject::Info> tmp = VipProcessingObject::validProcessingObjects<VipIODevice*>(QVariantList(), -1);
	QList<VipProcessingObject::Info> res;
	// QList<int> devices = vipUserTypes<VipIODevice*>();

	QString prefix;

	// try to find the device name in the path, taking care of possible namespaces in the name
	{
		QString _tmp = path.canonicalPath();
		_tmp.replace("::", "--");
		int index = _tmp.indexOf(":");
		if (index >= 0)
			prefix = path.canonicalPath().mid(0, index);
	}

	for (QMultiMap<QString, VipProcessingObject::Info>::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
		if (isUnregistered(it.value().metatype))
			continue;
		VipIODevice* device = qobject_cast<VipIODevice*>(it.value().create());

		if (device) {
			if ((device->supportedModes() & VipIODevice::ReadOnly)) {
				// check the output
				bool accept_output = (out_value.userType() == 0);
				if (!accept_output) {
					for (int o = 0; o < device->outputCount(); ++o) {
						QVariant v = device->outputAt(o)->data().data();
						if (v.canConvert(VIP_META(out_value.userType())) || v.userType() == 0) {
							accept_output = true;
							break;
						}
					}
				}

				if (accept_output) {
					if (!prefix.isEmpty() && (device->metaObject()->className() == prefix || device->info().classname == prefix)) {
						res.clear();
						res.append(device->info());
						delete device;
						break;
					}
					device->setMapFileSystem(path.mapFileSystem());
					if (device->probe(path.canonicalPath(), first_bytes) || (path.isEmpty() && first_bytes.isEmpty())) {
						res.append(device->info());
					}
				}
			}

			delete device;
		}
	}
	return res;
}

QList<VipProcessingObject::Info> VipIODevice::possibleWriteDevices(const VipPath& path, const QVariantList& input_data)
{
	QList<VipProcessingObject::Info> res;
	QMultiMap<QString, VipProcessingObject::Info> tmp = VipProcessingObject::validProcessingObjects<VipIODevice*>(input_data, 0);

	int index = path.canonicalPath().indexOf(":");
	QString prefix;
	if (index >= 0)
		prefix = path.canonicalPath().mid(0, index);

	for (QMultiMap<QString, VipProcessingObject::Info>::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
		if (isUnregistered(it.value().metatype))
			continue;

		VipIODevice* device = qobject_cast<VipIODevice*>(it.value().create());
		if (device) {
			device->setMapFileSystem(path.mapFileSystem());
		}
		if (device && (device->supportedModes() & VipIODevice::WriteOnly) &&
		    (path.isEmpty() || device->probe(path.canonicalPath(), QByteArray()) || (!prefix.isEmpty() && device->metaObject()->className() == prefix))) {
			// if there are multiple input data, check that the device input is a VipMultiInput
			if (!device->topLevelInputAt(0)->toMultiInput() && input_data.size() > 1) {
				delete device;
				continue;
			}

			// check that the device accept all possible input data
			if (device->topLevelInputCount() > 0) {
				bool accept_all = true;
				for (int v = 0; v < input_data.size(); ++v) {
					if (!device->acceptInput(0, input_data[v])) {
						accept_all = false;
						break;
					}
				}

				if (accept_all)
					res << device->info();
			}
		}

		if (device)
			delete device;
	}
	return res;
}

QStringList VipIODevice::possibleReadFilters(const VipPath& path, const QByteArray& first_bytes, const QVariant& out_value)
{
	QList<VipProcessingObject::Info> devices = possibleReadDevices(path, first_bytes, out_value);
	QSet<QString> filters;

	for (int i = 0; i < devices.size(); ++i) {
		VipIODevice* dev = qobject_cast<VipIODevice*>(devices[i].create());
		if (dev) {
			QString filter = dev->fileFilters();
			if (!filter.isEmpty())
				filters.insert(filter);

			delete dev;
		}
	}

	QStringList res = filters.values();
	res.sort();
	return res;
}

QStringList VipIODevice::possibleWriteFilters(const VipPath& path, const QVariantList& input_data)
{
	QList<VipProcessingObject::Info> devices = possibleWriteDevices(path, input_data);
	QSet<QString> filters;

	for (int i = 0; i < devices.size(); ++i) {
		VipIODevice* dev = qobject_cast<VipIODevice*>(devices[i].create());
		if (dev) {
			QString filter = dev->fileFilters();
			if (!filter.isEmpty())
				filters.insert(filter);

			delete dev;
		}
	}

	QStringList res = filters.values();
	res.sort();
	return res;
}

VipFileHandler::VipFileHandler()
  : VipIODevice()
{
}
VipFileHandler::~VipFileHandler()
{
	close();
}
bool VipFileHandler::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::ReadOnly)
		return false;

	QString p = this->path();
	QString error;
	bool res = this->open(p, &error);
	if (!res)
		setError(error);
	if (res)
		setOpenMode(mode);
	return res;
}

// class VipThreadPool : public QObject
// {
// QList<QSharedPointer<VipTaskPool> > m_pools;
// QMutex m_mutex;
//
// VipTaskPool * freeTask() {
// for (int i = 0; i < m_pools.size(); ++i) {
//	if(m_pools[i]->remaining())
// }
// }
//
// public:
// VipThreadPool(QObject * parent = nullptr)
// :QObject(parent), m_mutex(QMutex::Recursive)
// {
//
// }
// }

class PlayThread : public QThread
{
public:
	VipProcessingPool* pool;
	PlayThread(VipProcessingPool* p)
	  : pool(p)
	{
	}
	virtual void run() { pool->runPlay(); }
	static void msleep(unsigned long msecs) { QThread::msleep(msecs); }
};

template<class Fun>
struct CallbackObject : public QObject
{
	Fun callback;
};

class VipProcessingPool::PrivateData
{
public:
	struct Parameters
	{
		// list of VipProcessingObject, only used for save() and restore()
		VipProcessingObjectList objects;

		bool enableMissFrames;
		double speed;
		RunMode mode;
		qint64 begin_time;
		qint64 end_time;
		qint64 time;

		QSharedPointer<int> maxListSize;
		QSharedPointer<int> maxListMemory;
		QSharedPointer<int> listLimitType;
		QSharedPointer<ErrorCodes> logErrors;

		Parameters(bool enableMissFrames = true,
			   double speed = 1,
			   RunMode mode = RunMode(),
			   qint64 begin_time = VipInvalidTime,
			   qint64 end_time = VipInvalidTime,
			   qint64 time = VipInvalidTime,
			   QSharedPointer<int> maxListSize = QSharedPointer<int>(),
			   QSharedPointer<int> maxListMemory = QSharedPointer<int>(),
			   QSharedPointer<int> listLimitType = QSharedPointer<int>(),
			   QSharedPointer<ErrorCodes> logErrors = QSharedPointer<ErrorCodes>())
		  : enableMissFrames(enableMissFrames)
		  , speed(speed)
		  , mode(mode)
		  , begin_time(begin_time)
		  , end_time(end_time)
		  , time(time)
		  , maxListSize(maxListSize)
		  , maxListMemory(maxListMemory)
		  , listLimitType(listLimitType)
		  , logErrors(logErrors)
		{
		}
	};

	PrivateData(VipProcessingPool* parent)
	  : run(false)
	  , has_sequential(false)
	  , has_temporal(false)
	  , dirty_time_window(true)
	  , device_type(Resource)
	  , dirty_children(nullptr)
	  , thread(parent)
	  , maxReadThreadCount(0)
	  , readMaxFPS(100)
	  , min_ms(10)
	{
	}

	Parameters parameters;
	QList<Parameters> savedParameters;

	bool run;
	bool has_sequential;
	bool has_temporal;
	VipTimeRangeList time_window;
	VipTimeRangeList time_window_no_limits;
	std::atomic<bool> dirty_time_window;
	DeviceType device_type;
	QObject* dirty_children;
	QVector<VipIODevice*> read_devices; // use a vector to disable COW (very minor optimization, mainly for openmp)
	QRecursiveMutex device_mutex;
	PlayThread thread;
	// QSharedPointer<VipThreadPool> readPool;

	QMap<int, VipProcessingPool::callback_function> playCallbacks;
	QList<QPointer<CallbackObject<VipProcessingPool::read_data_function>>> readCallbacks;

	QTimer streamingTimer;
	int maxReadThreadCount;
	int readMaxFPS;
	double min_ms;
};

// we need VipProcessingPool::PrivateData to be definied to implement VipIODevice::setEnabled
void VipIODevice::setEnabled(bool enable)
{
	if (enable != isEnabled()) {
		// dirty the parent processing pool time window since disabled devices are not used to compute the time window
		if (VipProcessingPool* pool = qobject_cast<VipProcessingPool*>(parent())) {
			pool->d_data->dirty_time_window = true;
			pool->emitProcessingChanged();
		}

		VipProcessingObject::setEnabled(enable);
	}
}

void VipIODevice::restore()
{
	bool enabled = isEnabled();

	if (d_data->savedParameters.size()) {
		d_data->parameters = d_data->savedParameters.back();
		d_data->savedParameters.pop_back();
	}
	VipProcessingObject::restore();

	if (isEnabled() != enabled) {
		// Enable changed during the restore operation: tells to the parent processing pool to recompute its time window
		if (VipProcessingPool* pool = qobject_cast<VipProcessingPool*>(parent())) {
			pool->d_data->dirty_time_window = true;
			pool->emitProcessingChanged();
		}
	}
}

void VipIODevice::setOutputDataTime(VipAnyData& data)
{
	if (deviceType() == Temporal)
		data.setTime(time());
}

void VipIODevice::setOpenMode(VipIODevice::OpenModes mode)
{
	if (mode != d_data->mode) {
		if ((mode & ReadOnly) && d_data->mode == NotOpen) {
			// we open a read-only device that was previously closed: reset the read time
			d_data->readTime = VipInvalidTime;
		}

		d_data->mode = mode;
		if (mode != NotOpen) {
			// dirty the parent processing pool time window since opening the devicemight change it
			if (VipProcessingPool* pool = qobject_cast<VipProcessingPool*>(parent())) {
				pool->d_data->dirty_time_window = true;
				pool->emitProcessingChanged();
			}

			if (this->deviceType() == Temporal) {
				// for temporal devices, set the duration and size as attributes
				if (size() != VipInvalidPosition)
					this->setAttribute("Size", size());
				if (firstTime() != VipInvalidTime && lastTime() != VipInvalidTime) {
					qint64 duration = lastTime() - firstTime();
					QString attr;
					if (duration < 1000)
						attr = QString::number(duration) + " ns";
					else if (duration < 1000000)
						attr = QString::number(duration / 1000.0) + " us";
					else if (duration < 1000000000)
						attr = QString::number(duration / 1000000.0) + " ms";
					else
						attr = QString::number(duration / 1000000000.0) + " s";
					setAttribute("Duration", attr);
				}
			}

			Q_EMIT opened();
		}
		else
			Q_EMIT closed();
		Q_EMIT openModeChanged(mode != NotOpen);
	}
}

static QList<VipProcessingPool*>& getPools()
{
	static QList<VipProcessingPool*> inst;
	return inst;
}
static QMutex& getPoolsMutex()
{
	static QMutex inst;
	return inst;
}
static QString generatePoolObjectName()
{
	//
	// Generate a unique object name for a new processing pool
	//
	QMutexLocker lock(&getPoolsMutex());
	QList<VipProcessingPool*>& pools = getPools();
	QString res;
	// set a unique name to this pool
	for (int i = 0; i < pools.size(); ++i) {
		const QString name = "VipProcessingPool" + QString::number(i + 1);
		// check if this name exists
		bool exists = false;
		for (int j = 0; j < pools.size(); ++j)
			if (pools[j]->objectName() == name) {
				exists = true;
				break;
			}
		if (!exists) {
			res = name;
			break;
		}
	}
	if (res.isEmpty()) {
		res = ("VipProcessingPool" + QString::number(pools.size() + 1));
	}
	return res;
}
static void setPoolObjectName(VipProcessingPool* pool, const QString& name)
{
	QMutexLocker lock(&getPoolsMutex());
	QList<VipProcessingPool*>& pools = getPools();

	VipProcessingPool* found = nullptr;

	for (int i = 0; i < pools.size(); ++i) {
		if (pool != pools[i] && pools[i]->objectName() == name) {
			found = pools[i];
			break;
		}
	}

	pool->setObjectName(name);
	if (found) {
		getPoolsMutex().unlock();
		found->setObjectName(generatePoolObjectName());
		getPoolsMutex().lock();
	}
}

template<class T>
static QList<T> findDirectChildren(const QObject* obj)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	return obj->findChildren<T>(QAnyStringView(), Qt::FindDirectChildrenOnly);
#else
	return obj->findChildren<T>(QString(), Qt::FindDirectChildrenOnly);
#endif
}
 


VipProcessingPool::VipProcessingPool(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data,this);
	this->setOpenMode(ReadOnly);

	d_data->streamingTimer.setSingleShot(false);
	d_data->streamingTimer.setInterval(100);
	connect(&d_data->streamingTimer, SIGNAL(timeout()), this, SLOT(checkForStreaming()), Qt::DirectConnection);

	// set a unique name to this pool
	setObjectName(generatePoolObjectName());
	QMutexLocker lock(&getPoolsMutex());
	getPools().push_back(this);
}

VipProcessingPool::~VipProcessingPool()
{
	{
		QMutexLocker lock(&getPoolsMutex());
		getPools().removeOne(this);
	}

	stop();
	setStreamingEnabled(false);
	close();
	d_data->streamingTimer.stop();
	setEnabled(false);
	wait();

	// remove callback
	for (int i = 0; i < d_data->readCallbacks.size(); ++i)
		if (d_data->readCallbacks[i])
			d_data->readCallbacks[i]->deleteLater();
}

QList<VipProcessingPool*> VipProcessingPool::pools()
{
	QMutexLocker lock(&getPoolsMutex());
	QList<VipProcessingPool*> res = getPools();
	res.detach();
	return res;
}

VipProcessingPool* VipProcessingPool::findPool(const QString& name)
{
	QMutexLocker lock(&getPoolsMutex());
	QList<VipProcessingPool*>& pools = getPools();
	for (int i = 0; i < pools.size(); ++i)
		if (pools[i]->objectName() == name)
			return pools[i];
	return nullptr;
}

void VipProcessingPool::save()
{
	QMutexLocker lock(&d_data->device_mutex);

	VipIODevice::save();
	d_data->savedParameters.append(PrivateData::Parameters(d_data->parameters.enableMissFrames,
							       playSpeed(),
							       modes(),
							       d_data->parameters.begin_time,
							       d_data->parameters.end_time,
							       time(),
							       d_data->parameters.maxListSize,
							       d_data->parameters.maxListMemory,
							       d_data->parameters.listLimitType,
							       d_data->parameters.logErrors));
	d_data->savedParameters.back().objects = findDirectChildren<VipProcessingObject*> (this);
	d_data->savedParameters.back().objects.save();
}

void VipProcessingPool::restore()
{
	QMutexLocker lock(&d_data->device_mutex);

	if (d_data->savedParameters.size()) {
		d_data->parameters = d_data->savedParameters.back();
		d_data->savedParameters.pop_back();
	}
	d_data->parameters.objects.restore();
	VipIODevice::restore();
	applyLimitsToChildren();

	if (d_data->parameters.time != VipInvalidTime)
		this->read(d_data->parameters.time);
}

void VipProcessingPool::setEnabled(bool enabled)
{
	QMutexLocker lock(&d_data->device_mutex);

	VipIODevice::setEnabled(enabled);
	VipProcessingObjectList lst = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->setEnabled(enabled);
}

const QVector<VipIODevice*>& VipProcessingPool::readDevices() const
{
	return d_data->read_devices;
}

double VipProcessingPool::playSpeed() const
{
	return d_data->parameters.speed;
}

VipProcessingPool::DeviceType VipProcessingPool::deviceType() const
{
	// computeChildren();
	return d_data->device_type;
}

bool VipProcessingPool::seek(qint64 time)
{
	return read(time);
}

bool VipProcessingPool::seekPos(qint64 pos)
{
	if (size() > 0)
		return read(posToTime(pos));
	else
		return 0;
}

void VipProcessingPool::setTimeLimitsEnable(bool enable)
{
	this->setMode(UseTimeLimits, enable);
}

void VipProcessingPool::setPlaySpeed(double speed)
{
	d_data->parameters.speed = speed;
	emitProcessingChanged();
}

void VipProcessingPool::setModes(VipProcessingPool::RunMode mode)
{
	d_data->parameters.mode = mode;
	d_data->dirty_time_window = true;
	emitProcessingChanged();
}

VipProcessingPool::RunMode VipProcessingPool::modes() const
{
	return d_data->parameters.mode;
}

void VipProcessingPool::setMode(RunModeFlag m, bool on)
{
	if (d_data->parameters.mode.testFlag(m) != on) {
		if (on)
			d_data->parameters.mode |= m;
		else
			d_data->parameters.mode &= ~m;

		d_data->dirty_time_window = true;
		emitProcessingChanged();
	}
}

bool VipProcessingPool::testMode(RunModeFlag m) const
{
	return d_data->parameters.mode.testFlag(m);
}

void VipProcessingPool::setMaxListSize(int size)
{
	if (size >= 0)
		d_data->parameters.maxListSize.reset(new int(size));
	else
		d_data->parameters.maxListSize.reset();
	applyLimitsToChildren();
}

void VipProcessingPool::setMaxListMemory(int memory)
{
	if (memory >= 0)
		d_data->parameters.maxListMemory.reset(new int(memory));
	else
		d_data->parameters.maxListMemory.reset();
	applyLimitsToChildren();
}

void VipProcessingPool::setListLimitType(int type)
{
	if (type >= 0)
		d_data->parameters.listLimitType.reset(new int(type));
	else
		d_data->parameters.listLimitType.reset();
	applyLimitsToChildren();
}

int VipProcessingPool::listLimitType() const
{
	return d_data->parameters.listLimitType ? *d_data->parameters.listLimitType : VipProcessingManager::listLimitType();
}
int VipProcessingPool::maxListSize() const
{
	return d_data->parameters.maxListSize ? *d_data->parameters.maxListSize : VipProcessingManager::maxListSize();
}
int VipProcessingPool::maxListMemory() const
{
	return d_data->parameters.maxListMemory ? *d_data->parameters.maxListMemory : VipProcessingManager::maxListMemory();
}
bool VipProcessingPool::hasMaxListSize() const
{
	return d_data->parameters.maxListSize.data();
}
bool VipProcessingPool::hasMaxListMemory() const
{
	return d_data->parameters.maxListMemory.data();
}
bool VipProcessingPool::hasListLimitType() const
{
	return d_data->parameters.listLimitType.data();
}

void VipProcessingPool::clearInputBuffers()
{
	QMutexLocker lock(&d_data->device_mutex);
	QList<VipProcessingObject*> objects = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < objects.size(); ++i) {
		objects[i]->clearInputBuffers();
	}
}

void VipProcessingPool::resetProcessing()
{
	QMutexLocker lock(&d_data->device_mutex);

	QList<VipProcessingObject*> objects = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->reset();
}

bool VipProcessingPool::hasSequentialDevice() const
{
	return d_data->has_sequential;
}

bool VipProcessingPool::hasTemporalDevice() const
{
	return d_data->has_temporal;
}

QList<VipIODevice*> VipProcessingPool::ioDevices(VipIODevice::DeviceType type, bool should_be_opened)
{
	computeChildren();

	QMutexLocker lock(&d_data->device_mutex);

	QList<VipIODevice*> res;
	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		if (VipIODevice* dev = d_data->read_devices[i])
			if (dev->deviceType() == type) {
				if (!should_be_opened || dev->isOpen())
					res << dev;
			}
	}
	return res;
}

bool VipProcessingPool::isPlaying() const
{
	return d_data->run;
}

qint64 VipProcessingPool::stopBeginTime() const
{
	return d_data->parameters.begin_time;
}

qint64 VipProcessingPool::stopEndTime() const
{
	return d_data->parameters.end_time;
}

bool VipProcessingPool::missFramesEnabled() const
{
	return d_data->parameters.enableMissFrames;
}

void VipProcessingPool::setMissFramesEnabled(bool enable)
{
	d_data->parameters.enableMissFrames = enable;
}

void VipProcessingPool::setLogErrorEnabled(int error_code, bool enable)
{
	computeChildren();
	QMutexLocker lock(&d_data->device_mutex);
	VipProcessingObject::setLogErrorEnabled(error_code, enable);
	d_data->parameters.logErrors.reset(new ErrorCodes(this->logErrors()));
	QList<VipProcessingObject*> objects = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->setLogErrorEnabled(error_code, enable);
}

void VipProcessingPool::setLogErrors(const QSet<int>& errors)
{
	computeChildren();
	QMutexLocker lock(&d_data->device_mutex);
	VipProcessingObject::setLogErrors(errors);
	d_data->parameters.logErrors.reset(new ErrorCodes(errors));
	QList<VipProcessingObject*> objects = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < objects.size(); ++i)
		objects[i]->setLogErrors(errors);
}

void VipProcessingPool::resetLogErrors()
{
	QMutexLocker lock(&d_data->device_mutex);
	d_data->parameters.logErrors.reset();
}

bool VipProcessingPool::hasLogErrors() const
{
	return d_data->parameters.logErrors.data();
}

void VipProcessingPool::setReadMaxFPS(int fps)
{
	if (!fps)
		fps = INT_MAX;
	d_data->readMaxFPS = fps;
	d_data->min_ms = (1. / fps) * 1000;
}
int VipProcessingPool::readMaxFPS() const
{
	return d_data->readMaxFPS;
}

bool VipProcessingPool::reload()
{
	if (!isEnabled())
		return false;
	if (isPlaying())
		return false;

	// stop playing first
	stop();

	computeChildren();
	QMutexLocker lock(&d_data->device_mutex);

	bool res = false;

	// only reload the current data for non sequential devices

	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		if ((d_data->read_devices[i]->openMode() & VipIODevice::ReadOnly) && d_data->read_devices[i]->deviceType() != Sequential)
			if (d_data->read_devices[i]->read(time(), true)) // reload())
				res = true;
	}

	// emit this signal to update the play widget (if any)
	Q_EMIT timeChanged(time());

	return res;
}

int VipProcessingPool::maxReadThreadCount() const
{
	return d_data->maxReadThreadCount;
}
void VipProcessingPool::setMaxReadThreadCount(int count)
{
	if (count < 0)
		count = 0;
	d_data->maxReadThreadCount = count;
}

QList<VipProcessingObject*> VipProcessingPool::leafs(bool children_only) const
{
	QMutexLocker lock(&d_data->device_mutex);

	QSet<VipProcessingObject*> layer = vipToSet(vipListCast<VipProcessingObject*>(d_data->read_devices));
	QSet<VipProcessingObject*> all;
	QList<VipProcessingObject*> res;

	while (layer.size()) {
		QSet<VipProcessingObject*> tmp = layer;
		layer.clear();

		for (QSet<VipProcessingObject*>::iterator it = tmp.begin(); it != tmp.end(); ++it) {
			if (!all.contains(*it)) {
				all.insert(*it);
				if ((*it)->parent() == this || !children_only) {
					QList<VipProcessingObject*> outs = (*it)->directSinks();
					if (!outs.size())
						res << (*it);
					else {
						layer.unite(vipToSet(outs));
					}
				}
			}
		}
	}

	return res;
}

bool VipProcessingPool::readData(qint64 time)
{
	if (d_data->dirty_children)
		computeChildren();

	QMutexLocker lock(&d_data->device_mutex);

	// call callback functions
	for (int i = 0; i < d_data->readCallbacks.size(); ++i) {
		if (!d_data->readCallbacks[i]) {
			d_data->readCallbacks.removeAt(i);
			--i;
		}
		else
			d_data->readCallbacks[i]->callback(time);
	}

	std::vector<VipIODevice*> devices;
	devices.reserve(d_data->read_devices.size());

	for (VipIODevice* dev : d_data->read_devices)
		if ((dev->openMode() & VipIODevice::ReadOnly) && dev->deviceType() == Temporal && dev->isEnabled()) {
			devices.push_back(dev);
		}

	if (devices.size() > 1 && this->maxReadThreadCount() > 1) {

		std::atomic<int> res{ 0 };
		int thread_count = std::min(this->maxReadThreadCount(), (int)QThread::idealThreadCount());
		thread_count = std::min(thread_count, (int)devices.size());

#pragma omp parallel for shared(res) num_threads(thread_count)
		for (int i = 0; i < (int)devices.size(); ++i)
			res += (int)devices[i]->read(time, true);

		return res > 0;
	}
	else {
		int res = 0;
		for (size_t i = 0; i < devices.size(); ++i)
			res += (int)devices[i]->read(time, true);

		return res > 0;
	}
}

bool VipProcessingPool::enableStreaming(bool enable)
{
	computeChildren();

	// reset();

	QMutexLocker lock(&d_data->device_mutex);

	bool res = true;
	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		if ((d_data->read_devices[i]->deviceType() == VipIODevice::Sequential))
			if (!d_data->read_devices[i]->setStreamingEnabled(enable)) {
				res = false;
				if (enable) {
					// make sure to stop streaming on all devices
					for (int j = i - 1; j >= 0; --j) {
						if ((d_data->read_devices[j]->deviceType() == VipIODevice::Sequential))
							d_data->read_devices[j]->setStreamingEnabled(false);
					}
				}
				break;
			}
	}

	if (enable)
		d_data->streamingTimer.start();
	else
		d_data->streamingTimer.stop();

	emitProcessingChanged();
	return res;
}

qint64 VipProcessingPool::computeNextTime(qint64 from_time) const
{
	const_cast<VipProcessingPool*>(this)->computeChildren();

	QMutexLocker lock(&d_data->device_mutex);

	qint64 time = VipInvalidTime;

	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		if ((d_data->read_devices[i]->openMode() & VipIODevice::ReadOnly) && d_data->read_devices[i]->isEnabled()) {
			qint64 t = d_data->read_devices[i]->nextTime(from_time);
			if (t != VipInvalidTime && (t < time || time == VipInvalidTime) && t > from_time) //(t <= time || time == from_time))
				time = t;
		}
	}

	return time;
}

qint64 VipProcessingPool::computePreviousTime(qint64 from_time) const
{
	const_cast<VipProcessingPool*>(this)->computeChildren();

	QMutexLocker lock(&d_data->device_mutex);

	qint64 time = VipInvalidTime;

	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		if ((d_data->read_devices[i]->openMode() & VipIODevice::ReadOnly) && d_data->read_devices[i]->isEnabled()) {
			qint64 t = d_data->read_devices[i]->previousTime(from_time);
			if (t != VipInvalidTime && (t > time || time == VipInvalidTime) && t < from_time) //(t >= time || time == from_time))
				time = t;
		}
	}

	return time;
}

void VipProcessingPool::setTimestampingFilter(const VipTimestampingFilter& filter)
{
	Q_UNUSED(filter)
}

qint64 VipProcessingPool::computeClosestTime(qint64 from_time) const
{
	const_cast<VipProcessingPool*>(this)->computeTimeWindow();
	VipTimeRange range = timeLimits(d_data->time_window);
	return computeClosestTime(from_time, range);
}

qint64 VipProcessingPool::closestTimeNoLimits(qint64 from_time) const
{
	const_cast<VipProcessingPool*>(this)->computeTimeWindow();
	VipTimeRange range = timeLimits(d_data->time_window_no_limits);
	return computeClosestTime(from_time, range);
}

qint64 VipProcessingPool::computeClosestTime(qint64 from_time, const VipTimeRange& time_limits) const
{
	const_cast<VipProcessingPool*>(this)->computeChildren();

	QMutexLocker lock(&d_data->device_mutex);

	VipTimeRange range = time_limits;
	if (from_time < range.first)
		from_time = range.first;
	else if (from_time > range.second)
		from_time = range.second;

	qint64 time = VipInvalidTime;

	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		VipIODevice* d = d_data->read_devices[i];
		if ((d->openMode() & VipIODevice::ReadOnly) && d->isEnabled()) {
			qint64 t = d->closestTime(from_time);
			if (time == VipInvalidTime && t != VipInvalidTime)
				time = t;
			if (t != VipInvalidTime && (qAbs(from_time - t) < qAbs(from_time - time))) {
				// qint64 unused = d->closestTime(from_time);
				time = t;
			}
		}
	}

	if (time == VipInvalidTime)
		return from_time;
	else
		return time;
}

qint64 VipProcessingPool::computePosToTime(qint64 pos) const
{
	// use the first temporal device
	QMutexLocker lock(&d_data->device_mutex);
	if (size() > 0) {
		for (int i = 0; i < d_data->read_devices.size(); ++i) {
			if (d_data->read_devices[i]->deviceType() == VipIODevice::Temporal && d_data->read_devices[i]->size() > 1 && d_data->read_devices[i]->isEnabled())
				return d_data->read_devices[i]->posToTime(pos);
		}
	}

	return VipInvalidPosition;
}

qint64 VipProcessingPool::computeTimeToPos(qint64 time) const
{
	// use the first temporal device
	QMutexLocker lock(&d_data->device_mutex);
	if (size() > 0) {
		for (int i = 0; i < d_data->read_devices.size(); ++i) {
			if (d_data->read_devices[i]->deviceType() == VipIODevice::Temporal && d_data->read_devices[i]->size() > 1 && d_data->read_devices[i]->isEnabled())
				return d_data->read_devices[i]->timeToPos(time);
		}
	}

	return VipInvalidPosition; // VipIODevice::timeToPos(time);
}

VipTimeRangeList VipProcessingPool::computeTimeWindow() const
{
	const_cast<VipProcessingPool*>(this)->computeChildren();
	if (d_data->dirty_time_window) {
		QMutexLocker lock(&d_data->device_mutex);

		// Corrected bug when opening multiple players that trigger a recomputing of the time window
		if (!d_data->dirty_time_window)
			return d_data->time_window;

		const_cast<VipProcessingPool*>(this)->computeDeviceType();
		VipProcessingPool* _this = const_cast<VipProcessingPool*>(this);
		_this->d_data->time_window.clear();
		VipIODevice* temporal_device = nullptr;
		int temporal_device_count = 0;
		if (d_data->read_devices.size()) {
			for (int i = 0; i < d_data->read_devices.size(); ++i) {
				VipIODevice* dev = d_data->read_devices[i];
				// only compute the time window with temporal devices of size != 1
				if (!dev->isEnabled() || !dev->isOpen() || dev->deviceType() != Temporal || dev->size() == 1)
					continue;

				_this->d_data->time_window += dev->timeWindow();
				temporal_device_count++;
				temporal_device = dev;
			}
			_this->d_data->time_window = vipReorder(d_data->time_window, Vip::Ascending, true);
		}

		_this->d_data->dirty_time_window = false;

		// vipClamp to time limits if necessary
		if (d_data->parameters.mode & UseTimeLimits) {
			qint64 start = stopBeginTime();
			if (start == VipInvalidTime)
				start = firstTime();

			qint64 end = stopEndTime();
			if (end == VipInvalidTime)
				end = lastTime();

			_this->d_data->time_window_no_limits = _this->d_data->time_window;
			_this->d_data->time_window = vipClamp(_this->d_data->time_window, start, end);
		}

		// set size
		_this->setSize(VipInvalidPosition);
		if (temporal_device_count == 1)
			_this->setSize(temporal_device->size());
	}
	return d_data->time_window;
}

void VipProcessingPool::clear()
{
	computeChildren();

	QMutexLocker lock(&d_data->device_mutex);

	QList<VipProcessingObject*> lst = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < lst.size(); ++i)
		delete lst[i];

	d_data->read_devices.clear();
	emitProcessingChanged();
}

void VipProcessingPool::close()
{
	computeChildren();
	QMutexLocker lock(&d_data->device_mutex);

	for (int i = 0; i < d_data->read_devices.size(); ++i)
		d_data->read_devices[i]->close();
	emitProcessingChanged();
}

VipProcessingObjectList VipProcessingPool::processing(const QString& inherit_class_name) const
{
	const_cast<VipProcessingPool*>(this)->computeChildren();
	QMutexLocker lock(&d_data->device_mutex);

	VipProcessingObjectList lst = findDirectChildren<VipProcessingObject*>(this);
	if (inherit_class_name.isEmpty())
		return lst;

	VipProcessingObjectList res;
	for (int i = 0; i < lst.size(); ++i) {
		VipProcessingObject* obj = lst[i];
		const QMetaObject* meta = obj->metaObject();
		while (meta && meta->className() != inherit_class_name) {
			meta = meta->superClass();
		}

		if (meta)
			res << obj;
	}
	return res;
}

bool VipProcessingPool::open(VipIODevice::OpenModes mode)
{
	if ((mode & VipIODevice::ReadOnly) && (mode & VipIODevice::WriteOnly)) {
		// cannot call VipProcessingPool::open with VipIODevice::ReadOnly|VipIODevice::WriteOnly
		return false;
	}
	computeChildren();
	QMutexLocker lock(&d_data->device_mutex);

	bool res = true;

	QList<VipIODevice*> devices = findDirectChildren<VipIODevice*>(this);

	for (int i = 0; i < devices.size(); ++i)
		if (devices[i]->supportedModes() & mode) {
			if (!devices[i]->open(mode))
				res = false;
		}

	return res;
}

void VipProcessingPool::emitObjectAdded(QObjectPointer obj)
{
	if (obj)
		Q_EMIT objectAdded(obj);

	// reset the current time if needed

	if (VipIODevice* device = qobject_cast<VipIODevice*>(obj.data()))
		if (device->openMode() == VipIODevice::ReadOnly) {
			if (time() == VipInvalidTime)
				device->read(device->firstTime());
			else if (time() < device->firstTime())
				this->read(device->firstTime());
			else if (time() > device->lastTime())
				this->read(device->lastTime());
			else
				device->read(time());
		}
}

void VipProcessingPool::checkForStreaming()
{
	this->computeChildren();

	// check that streaming is still going on, and disable it if not
	bool no_streaming = true;
	{
		QMutexLocker lock(&d_data->device_mutex);
		for (int i = 0; i < d_data->read_devices.size(); ++i) {
			if (d_data->read_devices[i]->deviceType() == Sequential && d_data->read_devices[i]->isOpen() && d_data->read_devices[i]->isStreamingEnabled()) {
				no_streaming = false;
				break;
			}
		}
	}

	if (no_streaming)
		this->setStreamingEnabled(false);
}

void VipProcessingPool::childEvent(QChildEvent* event)
{
	if (event->removed()) {
		//TEST: do not stop streaming when removing a child
		//this->stop();
		//this->setStreamingEnabled(false);
	}

	// we might need to process the previously added child before processing this new one
	if(d_data->dirty_children)
		this->computeChildren();

	d_data->dirty_children = event->child();
	d_data->dirty_time_window = true;

	if (event->added()) {
		QMetaObject::invokeMethod(this, "emitObjectAdded", Qt::QueuedConnection, Q_ARG(QObjectPointer, QObjectPointer(event->child()))); // Q_EMIT objectAdded(event->child());

		if (event->child()->metaObject()->indexOfSignal("connectionOpened(VipProcessingIO*,int,QString)") >= 0) {
			connect(event->child(), SIGNAL(connectionOpened(VipProcessingIO*, int, const QString&)), this, SLOT(receiveConnectionOpened(VipProcessingIO*, int, const QString&)));
			connect(event->child(), SIGNAL(connectionClosed(VipProcessingIO*)), this, SLOT(receiveConnectionClosed(VipProcessingIO*)));
		}

		if (VipIODevice* d = qobject_cast<VipIODevice*>(d_data->dirty_children)) {
			// Start device streaming if necessary
			if (this->isStreamingEnabled() && d->deviceType() == VipIODevice::Sequential)
				d->setStreamingEnabled(true);
		}

	}
	else if (event->removed()) {
		if (event->child()->metaObject()->indexOfSignal("connectionOpened(VipProcessingIO*,int,QString)") >= 0) {
			disconnect(event->child(), SIGNAL(connectionOpened(VipProcessingIO*, int, const QString&)), this, SLOT(receiveConnectionOpened(VipProcessingIO*, int, const QString&)));
			disconnect(event->child(), SIGNAL(connectionClosed(VipProcessingIO*)), this, SLOT(receiveConnectionClosed(VipProcessingIO*)));
		}

		Q_EMIT objectRemoved(event->child());
	}
}


void VipProcessingPool::applyLimitsToChildren()
{
	QMutexLocker lock(&d_data->device_mutex);

	if (!hasLogErrors() && !hasMaxListSize() && !hasMaxListMemory() && !hasListLimitType())
		return;

	QList<VipProcessingObject*> objects = findDirectChildren<VipProcessingObject*>(this);

	int max_list_size = maxListSize();
	int max_list_memory = maxListMemory();
	int list_limit_type = listLimitType();

	for (int i = 0; i < objects.size(); ++i) {

		VipProcessingObject* obj = objects[i];
		if (hasLogErrors() && obj->logErrors() != this->logErrors())
			obj->setLogErrors(this->logErrors());

		for (int in = 0; in < obj->inputCount(); ++in) {
			VipInput* input = obj->inputAt(in);
			if (hasMaxListSize())
				input->buffer()->setMaxListSize(max_list_size);
			if (hasMaxListMemory())
				input->buffer()->setMaxListMemory(max_list_memory);
			if (hasListLimitType())
				input->buffer()->setListLimitType(list_limit_type);
		}
	}
}
void VipProcessingPool::computeChildren()
{
	if (!d_data->dirty_children)
		return;

	// Workaround to avoid potential crash when computeChildren() is called from within VipProcessingObject::run().
	// This happened when opening a JSON file from the event dashboard.
	if (QThread::currentThread() != QCoreApplication::instance()->thread())
		return;

	QMutexLocker lock(&d_data->device_mutex);

	// retrieve read only devices
	d_data->read_devices = findDirectChildren<VipIODevice*>(this).toVector();
	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		VipIODevice* dev = d_data->read_devices[i];
		if (!(dev->openMode() & VipIODevice::ReadOnly) && !(dev->supportedModes() & VipIODevice::ReadOnly)) {
			d_data->read_devices.removeAt(i);
			--i;
		}
	}

	if (this->children().indexOf(d_data->dirty_children) >= 0) {
		// in case of VipIODevice added, also add all the sinks without parents
		if (qobject_cast<VipIODevice*>(d_data->dirty_children)) {
			VipProcessingObjectList lst = static_cast<VipIODevice*>(d_data->dirty_children)->allSinks();
			for (int i = 0; i < lst.size(); ++i) {
				if (!lst[i]->parent())
					lst[i]->setParent(this);
			}
		}

		// make the new children has a unique name
		if (qobject_cast<VipProcessingObject*>(d_data->dirty_children)) {
			VipProcessingObject* new_child = static_cast<VipProcessingObject*>(d_data->dirty_children);
			if (new_child->objectName().isEmpty())
				new_child->setObjectName(QString(new_child->info().classname));

			VipProcessingObjectList lst = findDirectChildren<VipProcessingObject*>(this);
			lst.removeOne(new_child);
			if (lst.size() == 0) {
				if (new_child->objectName().isEmpty())
					new_child->setObjectName(QString(new_child->info().classname));
			}
			else {
				VipProcessingObject* found = lst.findOne<VipProcessingObject*>(new_child->objectName());
				int count = 1;
				while (found) {
					QString name = QString(new_child->info().classname) + "_" + QString::number(count);
					new_child->setObjectName(name);
					found = lst.findOne<VipProcessingObject*>(name);
					++count;
				}
			}
		}

		applyLimitsToChildren();
	}

	// connect VipIODevice objects timestampingChanged() signal to keep tracks of timestamping filters
	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		disconnect(d_data->read_devices[i], SIGNAL(timestampingFilterChanged()), this, SLOT(childTimestampingFilterChanged()));
		connect(d_data->read_devices[i], SIGNAL(timestampingFilterChanged()), this, SLOT(childTimestampingFilterChanged()), Qt::DirectConnection);

		disconnect(d_data->read_devices[i], SIGNAL(timestampingChanged()), this, SLOT(childTimestampingChanged()));
		connect(d_data->read_devices[i], SIGNAL(timestampingChanged()), this, SLOT(childTimestampingChanged()), Qt::DirectConnection);
	}

	computeDeviceType();

	// notify that the time window has changed
	d_data->dirty_children = nullptr;
}

void VipProcessingPool::computeDeviceType()
{
	// compute the device type
	d_data->has_temporal = false;
	d_data->has_sequential = false;
	for (int i = 0; i < d_data->read_devices.size(); ++i) {
		if (!d_data->read_devices[i]->isEnabled())
			continue;

		// VipIODevice* d = d_data->read_devices[i];
		if (d_data->read_devices[i]->deviceType() == Temporal)
			d_data->has_temporal = true;
		else if (d_data->read_devices[i]->deviceType() == Sequential)
			d_data->has_sequential = true;
	}

	DeviceType saved = d_data->device_type;

	// Temporal has the priority
	if (d_data->has_temporal)
		d_data->device_type = Temporal;
	// then Sequential
	else if (d_data->has_sequential)
		d_data->device_type = Sequential;
	// then Resource
	else
		d_data->device_type = Resource;

	if (saved != d_data->device_type) {
		Q_EMIT deviceTypeChanged();
	}

	// set size
	this->setSize(VipInvalidPosition);
	if (d_data->read_devices.size() == 1)
		if (d_data->read_devices.first()->size() != VipInvalidPosition)
			this->setSize(d_data->read_devices.first()->size());
}

void VipProcessingPool::setStopBeginTime(qint64 begin)
{
	if (begin != d_data->parameters.begin_time) {
		d_data->parameters.begin_time = begin;
		if (d_data->parameters.end_time != VipInvalidTime && d_data->parameters.end_time < d_data->parameters.begin_time)
			qSwap(d_data->parameters.begin_time, d_data->parameters.end_time);

		if (testMode(UseTimeLimits))
			d_data->dirty_time_window = true;
		emitProcessingChanged();
	}
}

void VipProcessingPool::setStopEndTime(qint64 end)
{
	if (d_data->parameters.end_time != end) {
		d_data->parameters.end_time = end;
		if (d_data->parameters.end_time != VipInvalidTime && d_data->parameters.end_time < d_data->parameters.begin_time)
			qSwap(d_data->parameters.begin_time, d_data->parameters.end_time);

		if (testMode(UseTimeLimits))
			d_data->dirty_time_window = true;
		emitProcessingChanged();
	}
}

void VipProcessingPool::setRepeat(bool enable)
{
	this->setMode(Repeat, enable);
}

void VipProcessingPool::play()
{
	computeChildren();
	if (!isPlaying()) {
		d_data->run = true;
		d_data->thread.start();
		emitProcessingChanged();
	}
}

void VipProcessingPool::playForward()
{
	setMode(Backward, false);
	play();
}

void VipProcessingPool::playBackward()
{
	setMode(Backward, true);
	play();
}

void VipProcessingPool::first()
{
	stop();
	// reset();
	this->seek(this->firstTime());
}

void VipProcessingPool::last()
{
	stop();
	this->seek(this->lastTime());
}

void VipProcessingPool::stop()
{
	if (isPlaying()) {

		d_data->run = false;
		// wait for the thread to finish
		while (d_data->thread.isRunning()) {
			if (QThread::currentThread() == qApp->thread())
				QCoreApplication::processEvents();
			else
				vipProcessEvents(nullptr, 10);
		}

		// d_data->thread.wait();
		emitProcessingChanged();
	}
}

bool VipProcessingPool::next()
{
	stop();
	return read(nextTime(time()));
}

bool VipProcessingPool::previous()
{
	stop();
	return read(previousTime(time()));
}

void VipProcessingPool::openReadDeviceAndConnections()
{
	this->computeChildren();
	QList<VipProcessingObject*> objects = findDirectChildren<VipProcessingObject*>(this);

	for (int i = 0; i < objects.size(); ++i) {
		if (VipIODevice* dev = qobject_cast<VipIODevice*>(objects[i]))
			if ((dev->supportedModes() & VipIODevice::ReadOnly) && !dev->isOpen())
				dev->open(VipIODevice::ReadOnly);

		objects[i]->openAllConnections();
	}
}

void VipProcessingPool::enableExcept(const VipProcessingObjectList& lst)
{
	VipProcessingObjectList children = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < children.size(); ++i)
		children[i]->setEnabled(true);
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->setEnabled(false);
}

void VipProcessingPool::disableExcept(const VipProcessingObjectList& lst)
{
	VipProcessingObjectList children = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < children.size(); ++i)
		children[i]->setEnabled(false);
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->setEnabled(true);
}

void VipProcessingPool::wait()
{
	// stop the playing and for all processing to be done
	stop();
	setStreamingEnabled(false);
	VipProcessingObjectList lst = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->wait();
}

bool VipProcessingPool::wait(uint msecs)
{
	qint64 start = QDateTime::currentMSecsSinceEpoch();

	// stop the playing and for all processing to be done
	stop();
	setStreamingEnabled(false);

	qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;
	if (elapsed > msecs)
		return false;

	qint64 remaining = msecs - elapsed;
	VipProcessingObjectList lst = findDirectChildren<VipProcessingObject*>(this);
	for (int i = 0; i < lst.size(); ++i) {
		lst[i]->wait(true, remaining);
		elapsed = QDateTime::currentMSecsSinceEpoch() - start;
		if (elapsed > msecs)
			return false;
		remaining = msecs - elapsed;
	}
	return true;
}

void VipProcessingPool::childTimestampingChanged()
{
	d_data->dirty_time_window = true;
	computeDeviceType();
	emitTimestampingChanged();
	emitProcessingChanged();
	// Reload all devices
	this->reload();
}

void VipProcessingPool::childTimestampingFilterChanged()
{
	d_data->dirty_time_window = true;
	emitTimestampingFilterChanged();
	emitProcessingChanged();
	// Reload all devices
	this->reload();
}

int VipProcessingPool::addPlayCallbackFunction(const callback_function& callback)
{
	int i = 0;
	for (QMap<int, callback_function>::iterator it = d_data->playCallbacks.begin(); it != d_data->playCallbacks.end(); ++it, ++i) {
		if (it.key() != i)
			break;
	}
	if (i == d_data->playCallbacks.size() - 1)
		++i;
	d_data->playCallbacks[i] = callback;
	return i;
}

void VipProcessingPool::removePlayCallbackFunction(int id)
{
	QMap<int, callback_function>::iterator it = d_data->playCallbacks.find(id);
	if (it != d_data->playCallbacks.end())
		d_data->playCallbacks.erase(it);
}

QObject* VipProcessingPool::addReadDataCallback(const read_data_function& callback)
{
	QMutexLocker lock(&d_data->device_mutex);

	// remove null callbacks
	for (int i = 0; i < d_data->readCallbacks.size(); ++i) {
		if (!d_data->readCallbacks[i]) {
			d_data->readCallbacks.removeAt(i);
			--i;
		}
	}
	CallbackObject<VipProcessingPool::read_data_function>* c = new CallbackObject<VipProcessingPool::read_data_function>();
	c->callback = callback;
	d_data->readCallbacks.append(c);
	return c;
}

void VipProcessingPool::removeReadDataCallback(QObject* obj)
{
	QMutexLocker lock(&d_data->device_mutex);
	// remove null callbacks
	for (int i = 0; i < d_data->readCallbacks.size(); ++i) {
		if (!d_data->readCallbacks[i] || d_data->readCallbacks[i] == obj) {
			d_data->readCallbacks.removeAt(i);
			--i;
		}
	}
}

void VipProcessingPool::runPlay()
{
	// Qt::HANDLE id = QThread::currentThreadId();
	// qint64 _id = (qint64)id;

	// retrieve the list of final (no output) processing objects
	VipProcessingObjectList objects = leafs(false);

	d_data->run = true;

	qint64 _time = vipGetMilliSecondsSinceEpoch();
	qint64 start_time = time();
	double speed = d_data->parameters.speed;

	// call the callback functions
	for (QMap<int, callback_function>::iterator it = d_data->playCallbacks.begin(); it != d_data->playCallbacks.end(); ++it)
		it.value()(StartPlaying);

	Q_EMIT playingStarted();

	qint64 elapsed = 0;
	//qint64 prev_elapsed = 0;
	qint64 st = 0, el = 0;

	while (d_data->run) {

		// follow play speed
		if ((d_data->parameters.mode & UsePlaySpeed)) {

			if (speed != d_data->parameters.speed) {
				start_time = time();
				speed = d_data->parameters.speed;
				_time = QDateTime::currentMSecsSinceEpoch();
				elapsed = 0;
			}
			else { // compute elapsed time since run started
				//prev_elapsed = elapsed;
				elapsed = (QDateTime::currentMSecsSinceEpoch() - _time) * 1000000 * d_data->parameters.speed; // elapsed time in nano seconds
			}

			// compute the current time
			qint64 current_time;
			if (d_data->parameters.mode & Backward)
				current_time = start_time - elapsed;
			else
				current_time = start_time + elapsed;

			bool ignore_sleep = false;
			if (current_time > lastTime() && !(d_data->parameters.mode & Backward) && time() >= lastTime()) {
				//...in forward mode
				if (d_data->parameters.mode & Repeat) {
					current_time = firstTime();
					_time = vipGetMilliSecondsSinceEpoch();
					start_time = current_time;
					ignore_sleep = true;
				}
				else
					d_data->run = false;
			}
			else if (current_time < firstTime() && (d_data->parameters.mode & Backward) && time() <= firstTime()) {
				//...in backward mode
				if (d_data->parameters.mode & Repeat) {
					current_time = lastTime();
					_time = vipGetMilliSecondsSinceEpoch();
					start_time = current_time;
					ignore_sleep = true;
				}
				else
					d_data->run = false;
			}

			if (!ignore_sleep) {
				qint64 pool_time = this->closestTime(this->time());
				if (!(d_data->parameters.mode & Backward)) {
					qint64 next = pool_time > this->time() ? pool_time : this->nextTime(pool_time);
					if (next != VipInvalidTime) {
						if (next > current_time) {
							vipSleep(1);
							continue;
						}
						if (!d_data->parameters.enableMissFrames)
							current_time = next;
					}
				}
				else {
					qint64 prev = pool_time < this->time() ? pool_time : this->previousTime(pool_time);
					if (prev != VipInvalidTime) {
						if (prev < current_time) {
							vipSleep(1);
							continue;
						}
						if (!d_data->parameters.enableMissFrames)
							current_time = prev;
					}
				}
			}

			// qint64 st = QDateTime::currentMSecsSinceEpoch();
			// read data
			if (d_data->run && !read(current_time, !(d_data->parameters.mode & Backward))) {
				VIP_LOG_ERROR("fail read " + QString::number(current_time));
				d_data->run = false;
			}

			// qint64 el = QDateTime::currentMSecsSinceEpoch()-st;
			// vip_debug("read: %i\n", (int)el);

			// d_data->thread.msleep(1);
		}
		else // goes as fast as possible
		{
			// in backward
			if (d_data->parameters.mode & Backward) {
				st = QDateTime::currentMSecsSinceEpoch();
				if (!read(previousTime(time())))
					d_data->run = false;
			}
			// in forward
			else {
				st = QDateTime::currentMSecsSinceEpoch();
				if (!read(nextTime(time())))
					d_data->run = false;
			}

			// forward
			if (!(d_data->parameters.mode & Backward)) {
				if (time() >= lastTime()) {
					// repeat mode
					if (d_data->parameters.mode & Repeat) {
						d_data->run = true;
						read(firstTime());
					}
					else
						d_data->run = false;
				}
			}
			else // backward
			{
				if (time() <= firstTime()) {
					// repeat mode
					if (d_data->parameters.mode & Repeat) {
						d_data->run = true;
						read(lastTime());
					}
					else
						d_data->run = false;
				}
			}
		}

		// wait for all the final processing objects
		if (d_data->run) {
			// qint64 st = QDateTime::currentMSecsSinceEpoch();
			for (int i = 0; i < objects.size(); ++i)
				if (objects[i])
					objects[i]->wait();
			// process events in order to avoid GUI freeze
			vipProcessEvents(&d_data->run);

			if (!(d_data->parameters.mode & UsePlaySpeed)) {

				el = QDateTime::currentMSecsSinceEpoch() - st;
				if (d_data->run && d_data->min_ms && el < d_data->min_ms) {
					// vip_debug("sleep for %f\n", d_data->min_ms - el);
					vipSleep(d_data->min_ms - el);
				}
			}

			// check if we still have valid temporal devices, stop otherwise
			bool has_temporal_device = false;
			{
				QMutexLocker lock(&d_data->device_mutex);

				for (int i = 0; i < d_data->read_devices.size(); ++i)
					if (VipIODevice* d = d_data->read_devices[i])
						if (d->isOpen() && d->deviceType() == Temporal) {
							has_temporal_device = true;
							break;
						}
			}
			if (!has_temporal_device)
				d_data->run = false;

			if (d_data->run) {
				// call the callback functions
				for (QMap<int, callback_function>::iterator it = d_data->playCallbacks.begin(); it != d_data->playCallbacks.end(); ++it)
					if (!it.value()(Playing))
						d_data->run = false;
			}
			// qint64 el = QDateTime::currentMSecsSinceEpoch()-st;
			// vip_debug("update: %i\n", (int)el);
			Q_EMIT playingAdvancedOneFrame();
		}
	}

	// call the callback functions
	for (QMap<int, callback_function>::iterator it = d_data->playCallbacks.begin(); it != d_data->playCallbacks.end(); ++it)
		it.value()(StopPlaying);

	Q_EMIT playingStopped();

	emitProcessingChanged();
}

class VipTimeRangeBasedGenerator::PrivateData
{
public:
	PrivateData()
	  : step_size(0)
	  , full_size(0)
	{
	}
	qint64 step_size;
	VipTimeRangeList ranges;
	QList<qint64> sizes;
	qint64 full_size;
	QVector<qint64> timestamps;
};

VipTimeRangeBasedGenerator::VipTimeRangeBasedGenerator(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipTimeRangeBasedGenerator::~VipTimeRangeBasedGenerator()
{
}

qint64 VipTimeRangeBasedGenerator::samplingTime() const
{
	return d_data->step_size;
}

const QVector<qint64>& VipTimeRangeBasedGenerator::timestamps() const
{
	return d_data->timestamps;
}

void VipTimeRangeBasedGenerator::setTimeWindows(const VipTimeRange& range, qint64 size)
{
	// generate the timestamps
	if (size == 0)
		return;

	if (size == 1) {
		setTimestamps(QVector<qint64>() << range.first);
	}
	else {
		QVector<qint64> times(size);
		vip_double sampling = (range.second - range.first) / vip_double(size - 1);
		for (int i = 0; i < size; ++i) {
			times[i] = range.first + sampling * i;
		}

		setTimestamps(times);
	}
}

void VipTimeRangeBasedGenerator::setTimeWindows(qint64 start, qint64 size, qint64 sampling)
{
	qint64 end = start + (size - 1) * sampling;
	d_data->ranges = VipTimeRangeList() << VipTimeRange(start, end);
	d_data->full_size = size;
	d_data->sizes = QList<qint64>() << size;
	d_data->timestamps.clear();
	if (size)
		d_data->step_size = sampling; //(end-start)/size;
	else
		d_data->step_size = 0;
	this->setSize(size);

	VipTimestampingFilter filter = this->timestampingFilter();
	filter.setInputTimeRangeList(d_data->ranges);
	this->setTimestampingFilter(filter);
	this->emitTimestampingChanged();
}

void VipTimeRangeBasedGenerator::setTimeWindows(const VipTimeRangeList& _ranges, qint64 _step_size)
{
	d_data->ranges = _ranges;
	d_data->step_size = _step_size;
	d_data->sizes.clear();
	d_data->timestamps.clear();
	d_data->full_size = 0;

	// compute the sizes
	for (int i = 0; i < d_data->ranges.size(); ++i) {
		d_data->sizes.append(qAbs(d_data->ranges[i].second - d_data->ranges[i].first) / d_data->step_size + 1);
		d_data->full_size += d_data->sizes.back();
	}

	this->setSize(d_data->full_size);

	VipTimestampingFilter filter = this->timestampingFilter();
	filter.setInputTimeRangeList(d_data->ranges);
	this->setTimestampingFilter(filter);
	this->emitTimestampingChanged();
}

void VipTimeRangeBasedGenerator::setTimestamps(const QVector<qint64>& timestamps, bool enable_multiple_time_range)
{
	d_data->ranges.clear();
	d_data->step_size = 0;
	d_data->sizes.clear();
	d_data->timestamps = timestamps;
	d_data->full_size = 0;

	if (timestamps.size()) {
		d_data->full_size = timestamps.size();
		d_data->sizes = QList<qint64>() << timestamps.size();
		d_data->ranges = VipTimeRangeList() << VipTimeRange(timestamps.first(), timestamps.last());
		if (timestamps.size() > 1)
			d_data->step_size = timestamps[1] - timestamps[0];
	}

	if (timestamps.size() > 1 && enable_multiple_time_range) {
		// find the minimum sampling time
		qint64 sampling = std::numeric_limits<qint64>::max();
		for (int i = 1; i < timestamps.size(); ++i) {
			qint64 tmp = qMin(sampling, timestamps[i] - timestamps[i - 1]);
			if (tmp != 0)
				sampling = tmp;
		}

		// reconstruct the time ranges.
		// we consider that a gap > 3*sampling is enough to start a new range (more than 3 consecutive miss frames).
		VipTimeRangeList ranges;
		VipTimeRange current(timestamps.first(), timestamps.first());

		d_data->sizes.clear();
		d_data->sizes.append(1);

		for (int i = 1; i < timestamps.size(); ++i) {
			qint64 gap = timestamps[i] - current.second;
			if (gap > 4 * sampling) {
				ranges << current;
				current = VipTimeRange(timestamps[i], timestamps[i]);
				d_data->sizes.append(1);
			}
			else {
				current.second = timestamps[i];
				d_data->sizes.last()++;
			}
		}
		ranges << current;
		d_data->step_size = sampling;
		d_data->ranges = ranges;
	}

	this->setSize(timestamps.size());

	VipTimestampingFilter filter = this->timestampingFilter();
	filter.setInputTimeRangeList(d_data->ranges);
	this->setTimestampingFilter(filter);
	this->emitTimestampingChanged();
}

void VipTimeRangeBasedGenerator::setTimestampsWithSampling(const QVector<qint64>& timestamps, qint64 sampling)
{
	d_data->ranges.clear();
	d_data->step_size = 0;
	d_data->sizes.clear();
	d_data->timestamps = timestamps;
	d_data->full_size = 0;

	if (timestamps.size()) {
		d_data->full_size = timestamps.size();
		d_data->sizes = QList<qint64>() << timestamps.size();
		d_data->ranges = VipTimeRangeList() << VipTimeRange(timestamps.first(), timestamps.last());
		if (timestamps.size() > 1)
			d_data->step_size = timestamps[1] - timestamps[0];
	}

	if (timestamps.size() > 1) {

		// reconstruct the time ranges.
		// we consider that a gap > 3*sampling is enough to start a new range (more than 3 consecutive miss frames).
		VipTimeRangeList ranges;
		VipTimeRange current(timestamps.first(), timestamps.first());

		d_data->sizes.clear();
		d_data->sizes.append(1);

		for (int i = 1; i < timestamps.size(); ++i) {
			qint64 gap = timestamps[i] - current.second;
			if ((double)gap > 1.5 * sampling) {
				ranges << current;
				current = VipTimeRange(timestamps[i], timestamps[i]);
				d_data->sizes.append(1);
			}
			else {
				current.second = timestamps[i];
				d_data->sizes.last()++;
			}
		}
		ranges << current;
		d_data->step_size = sampling;
		d_data->ranges = ranges;
	}

	this->setSize(timestamps.size());

	VipTimestampingFilter filter = this->timestampingFilter();
	filter.setInputTimeRangeList(d_data->ranges);
	this->setTimestampingFilter(filter);
	this->emitTimestampingChanged();
}

qint64 VipTimeRangeBasedGenerator::computePosToTime(qint64 pos) const
{
	if (d_data->timestamps.size()) {
		if (pos < 0)
			return d_data->timestamps.first();
		else if (pos >= d_data->timestamps.size())
			return d_data->timestamps.last();
		else
			return d_data->timestamps[pos];
	}

	qint64 cum_pos = 0;
	for (int i = 0; i < d_data->ranges.size(); ++i) {
		if (pos < d_data->sizes[i] + cum_pos) {
			qint64 p = pos - cum_pos;
			return d_data->ranges[i].first + p * d_data->step_size;
		}

		cum_pos += d_data->sizes[i];
	}
	return VipInvalidTime;
}

qint64 VipTimeRangeBasedGenerator::computeTimeToPos(qint64 time) const
{
	if (d_data->timestamps.size()) {
		if (time <= d_data->timestamps.first())
			return 0;
		else if (time >= d_data->timestamps.last())
			return d_data->timestamps.size() - 1;

		qint64 avg_sampling = 1;
		if (d_data->timestamps.size() > 1)
			avg_sampling = (d_data->timestamps.last() - d_data->timestamps.first()) / (d_data->timestamps.size() - 1);

		qint64 start_index = (time - d_data->timestamps.first()) / avg_sampling;

		if (start_index < 0)
			return 0;
		else if (start_index >= d_data->timestamps.size())
			return d_data->timestamps.size() - 1;

		if (d_data->timestamps[start_index] > time) {
			// go before
			for (int i = start_index - 1; i >= 0; --i) {
				if (d_data->timestamps[i] < time) {
					return (time - d_data->timestamps[i] < d_data->timestamps[i + 1] - time) ? i : i + 1;
				}
			}
		}
		else {
			// go after
			for (int i = start_index + 1; i < d_data->timestamps.size(); ++i) {
				if (d_data->timestamps[i] > time) {
					return (time - d_data->timestamps[i - 1] < d_data->timestamps[i] - time) ? i - 1 : i;
				}
			}
		}
		return VipInvalidPosition;
	}

	qint64 cum_pos = 0;
	for (int i = 0; i < d_data->ranges.size(); ++i) {
		if (time <= d_data->ranges[i].second) {
			return cum_pos + qRound64((time - d_data->ranges[i].first) / vip_double(d_data->step_size));
		}

		cum_pos += d_data->sizes[i];
	}

	return VipInvalidTime;
}

VipTimeRangeList VipTimeRangeBasedGenerator::computeTimeWindow() const
{
	return d_data->ranges;
}

#include "VipNDArray.h"

VipTextFileReader::VipTextFileReader(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
  , m_samplingTime(10)
  , m_type(Unknown)
{
	this->topLevelOutputAt(0)->toMultiOutput()->add();
	this->outputAt(0)->setData(VipNDArray());
}

VipTextFileReader::~VipTextFileReader()
{
	close();
}

VipTextFileReader::DeviceType VipTextFileReader::deviceType() const
{
	return m_arrays.size() == 1 ? Resource : Temporal;
}

bool VipTextFileReader::probe(const QString& filename, const QByteArray& first_bytes) const
{
	QString file(removePrefix(filename));
	return (QFileInfo(file).suffix().compare("txt", Qt::CaseInsensitive) == 0) || VipIODevice::probe(filename, first_bytes);
}

void VipTextFileReader::setSamplingTime(qint64 time)
{
	m_samplingTime = time;
	if (this->size()) {
		this->setTimeWindows(0, size(), m_samplingTime);
	}
}

qint64 VipTextFileReader::samplingTime() const
{
	return m_samplingTime;
}

void VipTextFileReader::setType(Type type)
{
	m_type = type;
}

VipTextFileReader::Type VipTextFileReader::type() const
{
	return m_type;
}

bool VipTextFileReader::reload()
{
	// if (isOpen())
	// open(openMode());
	return VipIODevice::reload();
}

bool VipTextFileReader::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::ReadOnly)
		return false;

	QString p = removePrefix(path());
	QStringList lst = p.split(";");
	if (lst.size() == 2) {
		p = lst[0];
		if (lst[1] == "Image")
			m_type = Image;
		else if (lst[1] == "XYXYColumn")
			m_type = XYXYColumn;
		else if (lst[1] == "XYYYColumn")
			m_type = XYYYColumn;
		else if (lst[1] == "XYXYRow")
			m_type = XYXYRow;
		else if (lst[1] == "XYYYRow")
			m_type = XYYYRow;
	}
	if (m_type == Unknown)
		m_type = Image;

	// open the file
	if (!createDevice(p, QFile::ReadOnly | QFile::Text))
		return false;

	// read all arrays it contains;
	m_arrays.clear();
	QTextStream stream(device());
	while (true) {
		VipNDArray ar;
		stream >> ar;
		if (!ar.isEmpty())
			m_arrays.append(ar);
		else
			break;
	}

	this->setTimeWindows(0, m_arrays.size(), m_samplingTime);
	if (m_arrays.size()) {
		// this->setAttribute("Date",QFileInfo(file).lastModified().toString());
		this->setOpenMode(ReadOnly);
		// read the first one
		return readData(0);
	}
	return false;
}

void VipTextFileReader::close()
{
	if (device())
		device()->close();

	setSize(0);
	setOpenMode(NotOpen);
}

bool VipTextFileReader::readData(qint64 time)
{
	qint64 pos = 0;
	if (deviceType() == Temporal)
		pos = this->computeTimeToPos(time);

	if (pos < 0 || pos >= m_arrays.size())
		return false;

	VipNDArray array = m_arrays[pos];
	QVariantList result;

	switch (m_type) {
		case Image:
			result.append(QVariant::fromValue(array));
			break;
		case XYXYColumn: {
			// array must have an even number of column
			if (array.shape(1) % 2)
				return false;

			VipNDArrayType<vip_double> tmp = array.toDouble();
			for (int c = 0; c < tmp.shape(1); c += 2) {
				VipPointVector points;
				for (int y = 0; y < tmp.shape(0); ++y)
					points.append(VipPoint(tmp(vipVector(y, c)), tmp(vipVector(y, c + 1))));
				result.append(QVariant::fromValue(points));
			}
		} break;
		case XYYYColumn: {
			// array must have an even number of column
			if (array.shape(1) < 2)
				return false;

			VipNDArrayType<double> tmp = array.toDouble();
			for (int c = 1; c < tmp.shape(1); c++) {
				VipPointVector points;
				for (int y = 0; y < tmp.shape(0); ++y)
					points.append(VipPoint(tmp(vipVector(y, 0)), tmp(vipVector(y, c))));
				result.append(QVariant::fromValue(points));
			}
		} break;
		case XYXYRow: {
			// array must have an even number of column
			if (array.shape(0) % 2)
				return false;

			VipNDArrayType<vip_double> tmp = array.toDouble();
			for (int r = 0; r < tmp.shape(0); r += 2) {
				VipPointVector points;
				for (int x = 0; x < tmp.shape(1); ++x)
					points.append(VipPoint(tmp(vipVector(r, x)), tmp(vipVector(r + 1, x))));
				result.append(QVariant::fromValue(points));
			}
		} break;
		case XYYYRow: {
			// array must have an even number of column
			if (array.shape(0) < 2)
				return false;

			VipNDArrayType<vip_double> tmp = array.toDouble();
			for (int r = 1; r < tmp.shape(0); r++) {
				VipPointVector points;
				for (int x = 0; x < tmp.shape(1); ++x)
					points.append(VipPoint(tmp(vipVector(0, x)), tmp(vipVector(r, x))));
				result.append(QVariant::fromValue(points));
			}
		} break;
		case Unknown: {
			return false;
		}
	}

	// send the result
	this->topLevelOutputAt(0)->toMultiOutput()->resize(result.size());
	QString name = QFileInfo(this->name()).baseName();

	for (int i = 0; i < result.size(); ++i) {
		VipAnyData data = create(result[i]);
		if (result.size() > 1)
			data.setName(name + " " + QString::number(i + 1));
		else
			data.setName(name);
		this->outputAt(i)->setData(data);
	}

	return result.size();
}

VipTextFileWriter::VipTextFileWriter(QObject* parent)
  : VipIODevice(parent)
  , m_number(0)
  , m_digits(5)
  , m_type(StackData)
{
}

VipTextFileWriter::~VipTextFileWriter()
{
	close();
}

void VipTextFileWriter::setType(Type type)
{
	m_type = type;
}

VipTextFileWriter::Type VipTextFileWriter::type() const
{
	return m_type;
}

void VipTextFileWriter::setDigitsNumber(int num)
{
	m_digits = qMax(num, 1);
}

int VipTextFileWriter::digitsNumber() const
{
	return m_digits;
}

QString VipTextFileWriter::formatDigit(int num, int digits)
{
	QString res = QString::number(num);
	return QString("0").repeated(qMax(digits - res.size(), 0)) + res;
}

QString VipTextFileWriter::nextFileName(const QString& path, int& number, int digits)
{
	QFileInfo info(path);
	QString dir = info.absolutePath();
	dir.replace("\\", "/");
	if (dir.endsWith("/"))
		dir.remove(dir.size() - 1, 1);

	QString basename = info.baseName();
	QString suffix = info.suffix();

	// format the number with 5 digits
	QString num = formatDigit(number, digits);

	QString outname = dir + "/" + basename + "_" + num + "." + suffix;
	while (QFileInfo(outname).exists()) {
		num = formatDigit(++number, digits);
		outname = dir + "/" + basename + "_" + num + "." + suffix;
	}

	return outname;
}

bool VipTextFileWriter::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::WriteOnly)
		return false;

	if (m_type == StackData) {
		// only with StackImages we can use a QIODevice
		if (!createDevice(removePrefix(path()), QFile::WriteOnly))
			return false;

		setOpenMode(mode);
		setSize(0);
		return true;
	}
	// else if(m_type == MultipleImages)
	// {
	//
	// }

	setOpenMode(mode);
	setSize(0);
	return true;
}

void VipTextFileWriter::close()
{
	VipIODevice::close();
	m_number = 0;
}

void VipTextFileWriter::apply()
{
	if (!isOpen()) {
		setError("'device not open", VipProcessingObject::DeviceNotOpen);
		return;
	}

	VipAnyData any = inputAt(0)->data();

	if (any.isEmpty()) {
		setError("nullptr input data", VipProcessingObject::WrongInput);
		return;
	}

	if (!any.data().canConvert<QString>()) {
		setError("input data cannot be converted to string", VipProcessingObject::WrongInput);
		return;
	}

	QString filename = removePrefix(path());

	switch (m_type) {
		case ReplaceFile: {
			QFile fout(filename);
			if (!fout.open(QFile::WriteOnly | QFile::Text)) {
				setError("cannot open file " + filename, VipProcessingObject::IOError);
				return;
			}
			QTextStream stream(&fout);
			stream << any.data().toString() << "\n";
		} break;
		case StackData: {
			if (device()) {
				QTextStream stream(device());
				stream << any.data().toString() << "\n";
				stream.flush();
				if (QFile* file = qobject_cast<QFile*>(device()))
					file->flush();
			}
			else {
				setError("cannot save input data", VipProcessingObject::IOError);
				return;
			}
		} break;
		case MultipleFiles: {
			QString name = nextFileName(filename, m_number, m_digits);
			QFile fout(name);
			if (!fout.open(QFile::WriteOnly | QFile::Text)) {
				setError("cannot open file " + filename, VipProcessingObject::IOError);
				return;
			}
			QTextStream stream(&fout);
			stream << any.data().toString() << "\n";
		} break;
	}

	setSize(size() + 1);
}

#include <QImageReader>

VipImageReader::VipImageReader(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
  , m_samplingTime(10)
{
	this->outputAt(0)->setData(VipNDArray());
}

VipImageReader::~VipImageReader()
{
	close();
}

void VipImageReader::setSamplingTime(qint64 time)
{
	m_samplingTime = time;
	if (this->size()) {
		this->setTimeWindows(0, size(), m_samplingTime);
	}
}

qint64 VipImageReader::samplingTime() const
{
	return m_samplingTime;
}

bool VipImageReader::reload()
{
	// if (isOpen())
	// open(openMode());
	return VipIODevice::reload();
}

VipImageReader::DeviceType VipImageReader::deviceType() const
{
	return m_arrays.size() == 1 ? Resource : Temporal;
}

bool VipImageReader::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::ReadOnly)
		return false;

	// open the file
	if (!createDevice(removePrefix(path()), QFile::ReadOnly))
		return false;

	// read all arrays it contains;
	m_arrays.clear();

	while (true) {
		QByteArray format;
		QString suffix = QFileInfo(removePrefix(path())).suffix();
		if (suffix.compare("jpeg", Qt::CaseInsensitive) == 0)
			format = "JPG";
		QImageReader reader(device(), format);
		QImage img;
		if (reader.read(&img))
			m_arrays.append(vipToArray(img));
		else
			break;
	}

	this->setTimeWindows(0, m_arrays.size(), m_samplingTime);
	if (m_arrays.size()) {
		readData(computeTimeWindow().first().first);
		// this->setAttribute("Date",QFileInfo(file).lastModified().toString());
		this->setOpenMode(ReadOnly);
		return true;
	}
	return false;
}

bool VipImageReader::readData(qint64 time)
{
	if (!m_arrays.size())
		return false;

	qint64 pos = this->computeTimeToPos(time);
	if (pos < 0)
		pos = 0;
	else if (pos >= m_arrays.size())
		pos = m_arrays.size() - 1;

	// send the result
	QString name = QFileInfo(this->name()).baseName();

	VipAnyData data = create(QVariant::fromValue(m_arrays[pos]));
	data.setName(name);
	this->outputAt(0)->setData(data);

	return true;
}

VipImageWriter::VipImageWriter(QObject* parent)
  : VipIODevice(parent)
  , m_number(0)
  , m_digits(5)
  , m_type(MultipleImages)
{
}

VipImageWriter::~VipImageWriter()
{
	close();
}

void VipImageWriter::setType(Type type)
{
	m_type = type;
}

VipImageWriter::Type VipImageWriter::type() const
{
	return m_type;
}

void VipImageWriter::setDigitsNumber(int num)
{
	m_digits = qMax(num, 1);
}

int VipImageWriter::digitsNumber() const
{
	return m_digits;
}

bool VipImageWriter::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::WriteOnly)
		return false;

	if (m_type == StackImages) {
		// only with StackImages we can use a QIODevice
		if (!createDevice(removePrefix(path()), QFile::WriteOnly))
			return false;

		setOpenMode(mode);
		setSize(0);
		return true;
	}
	// else if(m_type == MultipleImages)
	// {
	//
	// }

	setOpenMode(mode);
	setSize(0);
	return true;
}

void VipImageWriter::close()
{
	VipIODevice::close();
	m_number = 0;
}

void VipImageWriter::apply()
{
	if (!isOpen()) {
		setError("device not open", VipProcessingObject::DeviceNotOpen);
		return;
	}

	VipAnyData any = inputAt(0)->data();
	QImage img = vipToImage(any.value<VipNDArray>());

	if (img.isNull()) {
		setError("nullptr input image", VipProcessingObject::WrongInput);
		return;
	}

	switch (m_type) {
		case ReplaceImage: {
			if (!img.save(removePrefix(path())))
				setError("Cannot save image in file " + removePrefix(path()), VipProcessingObject::IOError);
			else
				setSize(size() + 1);
		} break;
		case StackImages: {
			if (device()) {
				img.save(device());
				if (QFile* file = qobject_cast<QFile*>(device()))
					file->flush();
				setSize(size() + 1);
			}
			else
				setError("cannot save input image", VipProcessingObject::IOError);
		} break;
		case MultipleImages: {
			QString filename = VipTextFileWriter::nextFileName(removePrefix(path()), m_number, m_digits);
			if (!img.save(filename))
				setError("cannot save image in file " + filename, VipProcessingObject::IOError);
			else
				setSize(size() + 1);
		} break;
	}
}

#include "VipStandardProcessing.h"

VipCSVReader::VipCSVReader(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
{
}
VipCSVReader::~VipCSVReader() {}

static QStringList extractTitleAndUnit(const QString& value)
{
	if (!value.endsWith(")"))
		return QStringList();

	int i = value.size() - 2;
	int count = 0;
	for (; i >= 0; --i) {
		if (value[i] == ')')
			++count;
		else if (value[i] == '(') {
			--count;
			if (count < 0)
				break;
		}
	}

	if (i >= 0) {
		QString title = value.mid(0, i);
		QString unit = value.mid(i + 1);
		unit = unit.mid(0, unit.size() - 1);
		return QStringList() << title << unit;
	}
	return QStringList();
}

bool VipCSVReader::open(VipIODevice::OpenModes mode)
{
	if (mode & VipIODevice::ReadOnly) {
		m_signals.clear();

		QFile in(removePrefix(path()));
		if (!in.open(QFile::ReadOnly | QFile::Text))
			return false;

		QByteArray data = in.readAll();

		QTextStream stream(data);

		qint64 pos = stream.pos();
		QLocale locale = QLocale(QLocale::French);

		// read the separator line
		QString first = stream.readLine();
		QString tmp = first;

		// try to read a number from it
		double unused;
		QTextStream unused_s(first.toLatin1());
		unused_s >> unused;
		QStringList lst;
		QString separator = "\t";
		if (unused_s.status() != QTextStream::Ok) {
			// cannot read value, this is the CSV format
			if (tmp.startsWith("\"") && tmp.endsWith("\""))
				tmp = tmp.mid(1, tmp.size() - 2);
			else
				tmp.replace(" ", "");

			QString float_sep = ",";

			// check separator
			if (tmp.startsWith("sep=")) {
				separator = tmp.mid(4);
				first = stream.readLine();
			}
			else if (tmp.contains(";")) {
				// Excel CSV format using ';' separator, without a header "sep=..."
				separator = ";";
			}

			if (separator == ",")
				float_sep = ".";
			else {
				int comma_count = data.count(',');
				int point_count = data.count('.');
				if (point_count > comma_count)
					float_sep = ".";
				else
					float_sep = ",";
			}

			// QLocale locale;
			if (float_sep == ".")
				locale = QLocale(QLocale::French);
			else
				locale = QLocale(QLocale::German);

			// read the first line
			lst = first.split(separator);
			if (lst.size() < 2) {
				setError("wrong column count (must be >= 2)");
				return false;
			}

			for (int i = 0; i < lst.size(); ++i) {
				QString val = lst[i];
				// remove starting/ending spaces
				while (val.startsWith(" "))
					val = val.mid(1);
				while (val.startsWith("\t"))
					val = val.mid(1);
				while (val.endsWith(" "))
					val = val.mid(0, val.size() - 1);
				while (val.endsWith("\t"))
					val = val.mid(0, val.size() - 1);
				lst[i] = val;
			}
		}
		else {
			// read number of values
			int count = 1;
			while (true) {
				unused_s >> unused;
				if (unused_s.status() != QTextStream::Ok)
					break;
				else
					++count;
			}
			for (int i = 0; i < count; ++i)
				lst.append(QString());
			stream.seek(pos);
		}

		// read other lines
		QVector<VipPointVector> vectors(lst.size() - 1);
		while (true) {
			QString line = stream.readLine();
			line.replace(separator, " ");
			QTextStream str(line.toLatin1());
			str.setLocale(locale);

			vip_double x = 0;
			str >> x;
			if (str.status() != QTextStream::Ok)
				break;

			for (int i = 0; i < vectors.size(); ++i) {
				vip_double y;
				str >> y;
				if (str.status() == QTextStream::Ok)
					vectors[i].append(VipPoint(x, y));
				else
					break;
			}
			if (str.status() != QTextStream::Ok)
				break;
		}

		for (int i = 0; i < vectors.size(); ++i) {
			VipAnyData any(QVariant::fromValue(vectors[i]), 0);
			any.setXUnit(lst[0]);

			// QString yunit = lst[i + 1];
			// int index1 = yunit.indexOf("(");
			// int index2 = yunit.lastIndexOf(")");
			// if (index1 >= 0 && index2 > index1)
			// {
			// QString title = yunit.mid(0,index1);
			// QString unit = yunit.mid(index1 + 1, index2 - index1 - 1);
			// any.setName(title);
			// any.setYUnit(unit);
			// }
			// else
			// any.setName(yunit);
			QStringList _tmp = extractTitleAndUnit(lst[i + 1]);
			if (_tmp.isEmpty())
				any.setName(lst[i + 1]);
			else {
				any.setName(_tmp[0]);
				any.setYUnit(_tmp[1]);
			}

			m_signals.append(any);
		}

		this->topLevelOutputAt(0)->toMultiOutput()->resize(m_signals.size());
		for (int i = 0; i < m_signals.size(); ++i)
			outputAt(i)->setData(m_signals[i]);

		setOpenMode(mode);
		return true;
	}
	return false;
}

bool VipCSVReader::readData(qint64)
{
	for (int i = 0; i < m_signals.size(); ++i)
		outputAt(i)->setData(m_signals[i]);

	return false;
}

VipCSVWriter::VipCSVWriter(QObject* parent)
  : VipIODevice(parent)
{
	setPaddValue(0);
	setResampleMode(ResampleIntersection | ResampleInterpolation);
}

VipCSVWriter::~VipCSVWriter() {}

void VipCSVWriter::setWriteTextFile(bool enable)
{
	setProperty("writeTXTFile", enable);
}

bool VipCSVWriter::writeTextFile() const
{
	return property("writeTXTFile").toBool();
}

void VipCSVWriter::setResampleMode(int r)
{
	setProperty("resampleMode", r);
}
int VipCSVWriter::resampleMode() const
{
	return property("resampleMode").toInt();
}

void VipCSVWriter::setPaddValue(double value)
{
	setProperty("paddValue", value);
}
double VipCSVWriter::paddValue() const
{
	return property("paddValue").toDouble();
}

bool VipCSVWriter::open(VipIODevice::OpenModes mode)
{
	if (mode & VipIODevice::WriteOnly) {
		setOpenMode(mode);
		return true;
	}
	return false;
}

void VipCSVWriter::apply()
{
	QList<VipPointVector> vectors;
	QStringList names;
	for (int i = 0; i < inputCount(); ++i) {
		VipAnyData any = inputAt(i)->data();
		VipPointVector vec = any.value<VipPointVector>();
		if (vec.size()) {
			if (names.size() == 0 && !any.xUnit().isEmpty())
				names.append(any.xUnit());

			names.append(any.name() + "(" + any.yUnit() + ")");
			vectors.append(vec);
		}
	}

	if (vectors.size()) {
		VipNDArray ar = vipResampleVectorsAsNDArray(vectors, (ResampleStrategies)resampleMode(), paddValue());
		if (ar.isEmpty()) {
			setError("Cannot create CSV file: check that the input signals are valid and not disjoint");
			return;
		}

		QFile out(removePrefix(path()));
		if (!out.open(QFile::WriteOnly | QFile::Text)) {
			setError("Cannot open output file " + out.fileName(), VipProcessingObject::IOError);
			return;
		}

		QTextStream stream(&out);
		if (!writeTextFile()) {
			// set comma separator for float number
			// stream.setLocale(QLocale(QLocale::German));
			// write EXCEL separator
			stream << "\"sep=\t\"\n";
			// write the header
			stream << names.join("\t") << "\n";
		}
		// write the data
		int width = ar.shape(1);
		int height = ar.shape(0);
		vip_double* data = (vip_double*)ar.data();

		for (int h = 0; h < height; ++h) {
			QByteArray line;
			QTextStream s(&line, QIODevice::WriteOnly);
			for (int w = 0; w < width; ++w) {
				if (w == 0) // write the first row (usually time value) as long integer (might be nanoseconds since epoch)
					s << QString::number((qint64)data[h * width + w]);
				else {
					s << data[h * width + w];
				}
				if (w < width - 1)
					s << "\t";
				s.flush();
				if (!writeTextFile()) // use comma instead of dot for CSV format
					line.replace('.', ',');
			}
			stream << line;
			if (h < height - 1)
				stream << "\n";
		}
	}
}

class VipDirectoryReader::PrivateData
{
public:
	// options
	QStringList supported_suffixes; // supported image file extensions while reading directory (default is 'png','jpg','jpeg',...)
	QSize fixed_size;
	qint32 file_count;
	qint32 file_start;
	bool smooth_resize;
	bool alphabetical_order;
	qint64 sampling;
	Type type;
	bool recursive;
	DeviceType deviceType;

	// files
	QStringList files;
	QStringList suffixes;
	bool dirtyFiles;

	QMap<QString, QSharedPointer<VipIODevice>> suffix_templates;

	// devices
	QList<QSharedPointer<VipIODevice>> devices;
	VipTimeRangeList timestamps;

	PrivateData()
	  : file_count(-1)
	  , file_start(0)
	  , smooth_resize(false)
	  , alphabetical_order(true)
	  , sampling(1)
	  , type(IndependentData)
	  , recursive(false)
	  , deviceType(Resource)
	  , dirtyFiles(true)
	{
	}
};

VipDirectoryReader::VipDirectoryReader(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	// set one output
	this->topLevelOutputAt(0)->toMultiOutput()->add();
}

VipDirectoryReader::~VipDirectoryReader()
{
	close();
}

void VipDirectoryReader::setSupportedSuffixes(const QStringList& suffixes)
{
	d_data->supported_suffixes = suffixes;
	d_data->dirtyFiles = true;
}

void VipDirectoryReader::setSupportedSuffixes(const QString& suffixes)
{
	QString tmp = suffixes;
	tmp.replace(" ", "");
	d_data->supported_suffixes = tmp.split(",", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	d_data->dirtyFiles = true;
}

void VipDirectoryReader::setFixedSize(const QSize& size)
{
	d_data->fixed_size = size;
}

void VipDirectoryReader::setFileCount(qint32 c)
{
	d_data->file_count = c;
	d_data->dirtyFiles = true;
}

void VipDirectoryReader::setFileStart(qint32 s)
{
	d_data->file_start = s;
	d_data->dirtyFiles = true;
}

void VipDirectoryReader::setSmoothResize(bool smooth)
{
	d_data->smooth_resize = smooth;
}

void VipDirectoryReader::setAlphabeticalOrder(bool order)
{
	d_data->alphabetical_order = order;
	d_data->dirtyFiles = true;
}

void VipDirectoryReader::setType(Type t)
{
	d_data->type = t;
}

void VipDirectoryReader::setRecursive(bool r)
{
	d_data->recursive = r;
	d_data->dirtyFiles = true;
}

QStringList VipDirectoryReader::supportedSuffixes() const
{
	return d_data->supported_suffixes;
}
QSize VipDirectoryReader::fixedSize() const
{
	return d_data->fixed_size;
}
qint32 VipDirectoryReader::fileCount() const
{
	return d_data->file_count;
}
qint32 VipDirectoryReader::fileStart() const
{
	return d_data->file_start;
}
bool VipDirectoryReader::smoothResize() const
{
	return d_data->smooth_resize;
}
bool VipDirectoryReader::alphabeticalOrder() const
{
	return d_data->alphabetical_order;
}
VipDirectoryReader::Type VipDirectoryReader::type() const
{
	return d_data->type;
}
bool VipDirectoryReader::recursive() const
{
	return d_data->recursive;
}

QStringList VipDirectoryReader::files() const
{
	const_cast<VipDirectoryReader*>(this)->computeFiles();
	return d_data->files;
}

QStringList VipDirectoryReader::suffixes() const
{
	const_cast<VipDirectoryReader*>(this)->computeFiles();
	return d_data->suffixes;
}

bool VipDirectoryReader::setPath(const QString& dirname)
{
	d_data->dirtyFiles = true;
	return VipIODevice::setPath(dirname);
}

bool VipDirectoryReader::probe(const QString& filename, const QByteArray&) const
{
	if (mapFileSystem()) {
		// if we have a map file system, we consider this is a directory if it exists and cannot be opened in read only
		if (mapFileSystem()->exists(VipPath(filename, true))) {
			if (QIODevice* d = mapFileSystem()->open(VipPath(filename), QIODevice::ReadOnly)) {
				delete d;
				return false;
			}
			else
				return true;
		}
		return false;
	}
	return QFileInfo(filename).isDir() || VipIODevice::probe(filename);
}

#include <QDirIterator>
#include <qdir.h>

void VipDirectoryReader::computeFiles()
{
	if (!d_data->dirtyFiles)
		return;
	d_data->dirtyFiles = true;

	QString dirname = removePrefix(path());
	dirname.replace("\\", "/");
	if (dirname.endsWith("/"))
		dirname = dirname.mid(0, dirname.length() - 1);

	if (!mapFileSystem())
		setMapFileSystem(VipMapFileSystemPtr(new VipPhysicalFileSystem()));

	// compute all files in dir
	VipPathList paths = mapFileSystem()->list(VipPath(dirname, true), d_data->recursive);
	QStringList files;
	for (int i = 0; i < paths.size(); ++i)
		if (!paths[i].isDir())
			files << paths[i].canonicalPath();
	// QDirIterator it(dirname, QDir::Files, d_data->recursive? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
	// while (it.hasNext())
	// files.append(it.next());

	// remove files that do not match the filters

	if (d_data->supported_suffixes.size()) {
		for (int i = 0; i < files.size(); ++i) {
			QString suffix = QFileInfo(files[i]).suffix();
			if (!d_data->supported_suffixes.contains(suffix, Qt::CaseInsensitive)) {
				files.removeAt(i);
				--i;
			}
		}
	}

	// sort
	files.sort();
	if (!d_data->alphabetical_order) {
		// reverse the list
		for (int k = 0; k < (files.size() / 2); k++)
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
			files.swap(k, files.size() - (1 + k));
#else
			files.swapItemsAt(k, files.size() - (1 + k));
#endif
	}

	// crop
	files = files.mid(d_data->file_start, d_data->file_count);

	// retrieve suffixes
	QSet<QString> suffixes;
	for (int i = 0; i < files.size(); ++i) {
		QString suffix = QFileInfo(files[i]).suffix();
		suffixes.insert(suffix.toLower());
	}

	// add the dirname prefix
	// for (int i = 0; i < files.size(); ++i)
	// {
	// files[i] = dirname + "/" + files[i];
	// }

	d_data->files = files;
	d_data->suffixes = suffixes.values();
}

void VipDirectoryReader::setSuffixTemplate(const QString& suffix, VipIODevice* device)
{
	d_data->suffix_templates[suffix.toLower()] = QSharedPointer<VipIODevice>(device);
}

VipIODevice* VipDirectoryReader::deviceFromOutput(int output_index) const
{
	int count = 0;
	for (int i = 0; i < d_data->devices.size(); ++i) {
		if (output_index < count + d_data->devices[i]->outputCount())
			return d_data->devices[i].data();

		count += d_data->devices[i]->outputCount();
	}
	return nullptr;
}

VipIODevice* VipDirectoryReader::deviceAt(int index) const
{
	return d_data->devices[index].data();
}

int VipDirectoryReader::deviceCount() const
{
	return d_data->devices.size();
}

void VipDirectoryReader::setSourceProperty(const char* name, const QVariant& value)
{
	VipIODevice::setSourceProperty(name, value);
	for (int i = 0; i < d_data->devices.size(); ++i) {
		d_data->devices[i]->setSourceProperty(name, value);
	}
}

VipDirectoryReader::DeviceType VipDirectoryReader::deviceType() const
{
	return d_data->deviceType;
}

void VipDirectoryReader::close()
{
	for (int i = 0; i < d_data->devices.size(); ++i) {
		d_data->devices[i]->close();
	}

	d_data->devices.clear();
	d_data->timestamps.clear();
	d_data->sampling = VipInvalidTime;
	setOpenMode(NotOpen);
}

void VipDirectoryReader::recomputeTimestamps()
{
	d_data->sampling = VipInvalidTime;

	for (int i = 0; i < d_data->devices.size(); ++i) {
		VipIODevice* device = d_data->devices[i].data();

		// find smallest sampling time
		qint64 sampling = device->estimateSamplingTime();
		if (d_data->sampling == VipInvalidTime || (sampling != VipInvalidTime && sampling < d_data->sampling))
			d_data->sampling = sampling;
	}

	d_data->timestamps.clear();
	if (d_data->sampling == VipInvalidTime)
		d_data->sampling = 1000000; // 1s

	if (d_data->type == SequenceOfData) {
		d_data->deviceType = Temporal;
		// compute the time range for each device
		for (int i = 0; i < d_data->devices.size(); ++i) {
			VipTimeRange d_range = d_data->devices[i]->timeLimits();
			if (!d_data->timestamps.size()) {
				if (d_range.first != VipInvalidTime && d_range.second != VipInvalidTime)
					d_data->timestamps.append(d_range);
				else
					d_data->timestamps.append(VipTimeRange(0, 0));
			}
			else {
				VipTimeRange last = d_data->timestamps.last();

				if (d_range.first == VipInvalidTime || d_range.second == VipInvalidTime) {
					d_range = (VipTimeRange(last.second + d_data->sampling, last.second + d_data->sampling));
				}
				else if (d_range.first <= last.second) {
					qint64 duration = d_range.second - d_range.first;
					d_range.first = last.second + d_data->sampling;
					d_range.second = d_range.first + duration;
				}

				d_data->timestamps.append(d_range);
			}
		}
	}
	else {
		d_data->deviceType = Resource;
		// the time range window is the union of each time range
		for (int i = 0; i < d_data->devices.size(); ++i) {
			if (d_data->devices[i]->deviceType() == Temporal && d_data->devices[i]->size() != 1) {
				d_data->deviceType = Temporal;
				d_data->timestamps += (d_data->devices[i]->timeWindow());
			}
		}
		vipReorder(d_data->timestamps, Vip::Ascending, true);
	}

	emitTimestampingChanged();
}

bool VipDirectoryReader::open(VipIODevice::OpenModes mode)
{
	close();
	if (mode != ReadOnly)
		return false;

	computeFiles();
	if (!d_data->files.size()) {
		VIP_LOG_WARNING("No file matching criteria in dir '" + removePrefix(path()) + "'");
		return false;
	}

	// create the devices for each file, estimate the minimal sampling time

	VipProgress progress;
	progress.setRange(0, d_data->files.size());
	progress.setCancelable(true);
	progress.setModal(true);

	int output_count = 0;
	int max_output_per_device = 0;

	for (int i = 0; i < d_data->files.size(); ++i) {
		if (progress.canceled())
			break;

		progress.setValue(i);
		progress.setText("Read <b>" + QFileInfo(d_data->files[i]).fileName() + "</b>");

		bool have_template = false;
		QSharedPointer<VipIODevice> template_device;
		QMap<QString, QSharedPointer<VipIODevice>>::iterator it = d_data->suffix_templates.find(QFileInfo(d_data->files[i]).suffix().toLower());
		if (it != d_data->suffix_templates.end()) {
			have_template = true;
			template_device = it.value();
		}
		VipIODevice* device = nullptr;

		// create the device
		if (have_template) {
			if (!template_device)
				continue;
			device = vipCreateVariant((QByteArray(template_device->metaObject()->className()) + "*").data()).value<VipIODevice*>();
		}
		else {
			QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(d_data->files[i], QByteArray());
			if (devices.size())
				device = qobject_cast<VipIODevice*>(devices.first().create());
		}

		if (!device)
			continue;

		device->setMapFileSystem(this->mapFileSystem());

		if (VipMultiOutput* out = device->topLevelOutputAt(0)->toMultiOutput())
			out->resize(1);

		// copy the parameters from the template device
		if (template_device)
			template_device->copyParameters(device);

		vip_debug("%s\n", d_data->files[i].toLatin1().data());
		device->setPath(d_data->files[i]);
		if (!device->open(VipIODevice::ReadOnly)) {
			delete device;
			continue;
		}

		max_output_per_device = qMax(max_output_per_device, device->outputCount());

		// find smallest sampling time
		// qint64 sampling = device->estimateSamplingTime();
		// if (d_data->sampling == VipInvalidTime || (sampling != VipInvalidTime && sampling < d_data->sampling) )
		// d_data->sampling = sampling;

		d_data->devices.append(QSharedPointer<VipIODevice>(device));

		connect(device, SIGNAL(timestampingChanged()), this, SLOT(recomputeTimestamps()), Qt::DirectConnection);

		output_count += device->outputCount();
	}

	this->blockSignals(true);
	recomputeTimestamps();
	this->blockSignals(false);

	// create the ouputs and set their data
	if (d_data->type == SequenceOfData) {
		this->topLevelOutputAt(0)->toMultiOutput()->resize(max_output_per_device);
		// for each output, try to set a valid data
		for (int o = 0; o < max_output_per_device; ++o) {
			for (int i = 0; i < d_data->devices.size(); ++i) {
				if (d_data->devices[i]->outputCount() > o) {
					VipAnyData data = d_data->devices[i]->outputAt(o)->data();
					if (data.data().userType() != 0) {
						this->outputAt(o)->setData(data);
						break;
					}
				}
			}
		}
	}
	else {

		this->topLevelOutputAt(0)->toMultiOutput()->resize(output_count); // d_data->devices.size());
		int out = 0;
		for (int i = 0; i < d_data->devices.size(); ++i) {
			VipIODevice* dev = d_data->devices[i].data();
			for (int o = 0; o < dev->outputCount(); ++o, ++out)
				this->outputAt(out)->setData(dev->outputAt(o)->data());
		}
	}

	setOpenMode(ReadOnly);

	// for Resource device only, load the data
	if (d_data->deviceType == Resource)
		read(0);

	return true;
}

VipTimeRangeList VipDirectoryReader::computeTimeWindow() const
{
	// if (d_data->timestamps.size())
	// return VipTimeRangeList() << VipTimeRange(d_data->timestamps.first().first, d_data->timestamps.last().second);
	// return VipTimeRangeList();
	return d_data->timestamps;
}

int VipDirectoryReader::closestDeviceIndex(qint64 time, qint64* closest) const
{
	if (!d_data->timestamps.size())
		return -1;
	else if (time <= d_data->timestamps.first().first)
		return 0;
	else if (time >= d_data->timestamps.last().second)
		return d_data->timestamps.size() - 1;

	for (int i = 0; i < d_data->timestamps.size(); ++i) {
		if (time >= d_data->timestamps[i].first && time <= d_data->timestamps[i].second) {
			if (closest)
				*closest = time;
			return i;
		}
		else if (time < d_data->timestamps[i].first) {
			// return the closest between i and i-1
			if (i > 0) {
				qint64 diff1 = time - d_data->timestamps[i - 1].second;
				qint64 diff2 = d_data->timestamps[i].first - time;
				if (diff1 < diff2) {
					if (closest)
						*closest = d_data->timestamps[i - 1].second;
					return i - 1;
				}
				else {
					if (closest)
						*closest = d_data->timestamps[i].first;
					return i;
				}
			}
			else {
				if (closest)
					*closest = d_data->timestamps[i].first;
				return i;
			}
		}
	}

	return -1;
}

qint64 VipDirectoryReader::computeNextTime(qint64 intime) const
{
	if (intime < firstTime())
		return firstTime();
	else if (intime >= lastTime())
		return lastTime();

	if (d_data->type == IndependentData) {
		qint64 from_time = (intime);
		qint64 time = VipInvalidTime;

		for (int i = 0; i < d_data->devices.size(); ++i) {
			if ((d_data->devices[i]->openMode() & VipIODevice::ReadOnly) && d_data->devices[i]->isEnabled()) {
				qint64 t = d_data->devices[i]->nextTime(from_time);
				if (t != VipInvalidTime && (t < time || time == VipInvalidTime) && t > from_time) //(t <= time || time == from_time))
					time = t;
			}
		}

		return time;
	}
	else {
		qint64 res = VipInvalidTime;
		qint64 time = (intime);
		qint64 closest = VipInvalidTime;
		int index = closestDeviceIndex(time, &closest);
		if (index >= 0) {
			bool use_sampling = (d_data->timestamps[index].first != d_data->devices[index]->firstTime());

			qint64 time_offset_before = 0;
			if (index > 0 && use_sampling)
				time_offset_before = d_data->timestamps[index - 1].second + d_data->sampling;

			qint64 next = d_data->devices[index]->nextTime(time - time_offset_before);
			if (next == time - time_offset_before || next == VipInvalidTime) {
				if (index + 1 < d_data->timestamps.size())
					res = (d_data->timestamps[index + 1].first);
				else
					res = (d_data->timestamps.last().second);
			}
			else // we found a validtime: transform and check that the result is different than intime (might be equal due to rounding errors with time transforms)
			{
				res = (next + time_offset_before);
				if (res == intime && (index + 1) < d_data->timestamps.size())
					res = (d_data->timestamps[index + 1].first);
			}
		}
		return res;
	}
}

qint64 VipDirectoryReader::computePreviousTime(qint64 intime) const
{
	if (intime <= firstTime())
		return firstTime();
	else if (intime > lastTime())
		return lastTime();

	if (d_data->type == IndependentData) {
		qint64 from_time = (intime);
		qint64 time = VipInvalidTime;

		for (int i = 0; i < d_data->devices.size(); ++i) {
			if ((d_data->devices[i]->openMode() & VipIODevice::ReadOnly) && d_data->devices[i]->isEnabled()) {
				qint64 t = d_data->devices[i]->previousTime(from_time);
				if (t != VipInvalidTime && (t > time || time == VipInvalidTime) && t < from_time) //(t >= time || time == from_time))
					time = t;
			}
		}

		return time;
	}
	else {
		qint64 res = VipInvalidTime;
		qint64 time = (intime);
		qint64 closest = VipInvalidTime;
		int index = closestDeviceIndex(time, &closest);
		if (index >= 0) {
			bool use_sampling = (d_data->timestamps[index].first != d_data->devices[index]->firstTime());

			qint64 time_offset_before = 0;
			if (index > 0 && use_sampling)
				time_offset_before = d_data->timestamps[index - 1].second + d_data->sampling;

			qint64 previous = d_data->devices[index]->previousTime(time - time_offset_before);
			if (previous == time - time_offset_before || previous == VipInvalidTime) {
				if (index - 1 >= 0)
					res = (d_data->timestamps[index - 1].second);
				else
					res = (d_data->timestamps.first().first);
			}
			else // we found a valid time: transform and check that the result is different than intime (might be equal due to rounding errors with time transforms)
			{
				res = (previous + time_offset_before);
				if (res == intime && (index - 1) >= 0)
					res = (d_data->timestamps[index - 1].second);
			}
		}
		return res;
	}
}

qint64 VipDirectoryReader::computeClosestTime(qint64 _time) const
{
	if (_time <= firstTime())
		return firstTime();
	else if (_time >= lastTime())
		return lastTime();

	if (d_data->type == IndependentData) {

		qint64 from_time = (_time);
		qint64 time = VipInvalidTime;

		for (int i = 0; i < d_data->devices.size(); ++i) {
			if ((d_data->devices[i]->openMode() & VipIODevice::ReadOnly) && d_data->devices[i]->isEnabled()) {
				qint64 t = d_data->devices[i]->closestTime(from_time);
				if (time == VipInvalidTime && t != VipInvalidTime)
					time = t;
				if (t != VipInvalidTime && (qAbs(from_time - t) < qAbs(from_time - time)))
					time = t;
			}
		}

		if (time == VipInvalidTime)
			return from_time;
		else
			return (time);
	}
	else {
		int index = closestDeviceIndex(_time);
		if (index < 0)
			return VipInvalidTime;

		bool use_sampling = (d_data->timestamps[index].first != d_data->devices[index]->firstTime());

		qint64 time_offset_before = 0;
		if (index > 0 && use_sampling)
			time_offset_before = d_data->timestamps[index - 1].second + d_data->sampling;

		return (d_data->devices[index]->closestTime(_time - time_offset_before) + time_offset_before);
	}
}

bool VipDirectoryReader::reload()
{
	if (d_data->type == IndependentData) {
		int out_index = 0;
		for (int i = 0; i < d_data->devices.size(); ++i) {
			if (VipIODevice* dev = d_data->devices[i].data()) {
				if (dev->reload()) {
					for (int o = 0; o < dev->outputCount(); ++o) {
						VipAnyData out = dev->outputAt(o)->data();
						// keep the original name
						QString name = out.name();
						out.mergeAttributes(attributes());
						out.setName(name);
						out.setSource((qint64)this);

						// for image only
						VipNDArray ar = out.data().value<VipNDArray>();
						if (!ar.isEmpty() && d_data->fixed_size != QSize()) {
							ar = ar.resize(vipVector(d_data->fixed_size.height(), d_data->fixed_size.width()),
								       d_data->smooth_resize ? Vip::CubicInterpolation : Vip::NoInterpolation);
							out.setData(QVariant::fromValue(ar));
						}

						outputAt(out_index + o)->setData(out);
					}
				}
				out_index += dev->outputCount();
			}
		}
		return true;
	}
	else {
		for (int i = 0; i < outputCount(); ++i)
			outputAt(i)->setData(outputAt(i)->data());
		return true;
	}
	VIP_UNREACHABLE();
	// return false;
}

bool VipDirectoryReader::readData(qint64 time)
{
	if (d_data->type == IndependentData) {
		int out_index = 0;
		for (int i = 0; i < d_data->devices.size(); ++i) {
			VipIODevice* dev = d_data->devices[i].data();
			if (dev->deviceType() == Temporal) {
				if (dev->read(time, true)) {
					for (int o = 0; o < dev->outputCount(); ++o) {
						VipAnyData out = dev->outputAt(o)->data();
						// keep the original name
						QString name = out.name();
						out.mergeAttributes(attributes());
						out.setName(name);
						out.setSource((qint64)this);
						out.setTime(time);

						// for image only
						VipNDArray ar = out.data().value<VipNDArray>();
						if (!ar.isEmpty() && d_data->fixed_size != QSize()) {
							ar = ar.resize(vipVector(d_data->fixed_size.height(), d_data->fixed_size.width()),
								       d_data->smooth_resize ? Vip::CubicInterpolation : Vip::NoInterpolation);
							out.setData(QVariant::fromValue(ar));
						}

						outputAt(out_index + o)->setData(out);
					}
				}
			}
			else {
				if (dev->read(time, true)) {
					for (int o = 0; o < dev->outputCount(); ++o) {
						outputAt(out_index + o)->setData(dev->outputAt(o)->data());
					}
				}
			}

			out_index += dev->outputCount();
		}
		return true;
	}
	else // SequenceOfData
	{

		if (!d_data->timestamps.size())
			return false;

		int index = closestDeviceIndex(time);
		if (index < 0)
			return false;

		qint64 time_offset_before = 0;
		bool use_sampling = (d_data->timestamps[index].first != d_data->devices[index]->firstTime());

		if (index > 0 && use_sampling)
			time_offset_before = d_data->timestamps[index - 1].second + d_data->sampling;

		d_data->devices[index]->read(time - time_offset_before);

		for (int o = 0; o < outputCount(); ++o) {
			VipAnyData out = d_data->devices[index]->outputAt(o)->data();
			out.mergeAttributes(attributes());
			out.setSource((qint64)this);
			out.setTime(time);

			// for image only
			VipNDArray ar = out.data().value<VipNDArray>();
			if (!ar.isEmpty() && d_data->fixed_size != QSize()) {
				ar = ar.resize(vipVector(d_data->fixed_size.height(), d_data->fixed_size.width()), d_data->smooth_resize ? Vip::CubicInterpolation : Vip::NoInterpolation);
				out.setData(QVariant::fromValue(ar));
			}

			outputAt(o)->setData(out);
		}
		return true;
	}
}

#include "VipXmlArchive.h"

struct FileShapeBuffer
{
	QString fname;
	size_t hash{ 0 };
	QVariant sceneModel;
};
Q_DECLARE_METATYPE(FileShapeBuffer)

static FileShapeBuffer _shape_buffer;
static QMutex _shape_buffer_mutex;

VipShapeReader::VipShapeReader()
{
	static bool registered = false;
	if (!registered) {
		registered = true;
		qRegisterMetaType<FileShapeBuffer>();
	}

	outputAt(0)->setData(VipSceneModel());
}

bool VipShapeReader::open(VipIODevice::OpenModes mode)
{
	if (!(mode & ReadOnly))
		return false;

	QString p = removePrefix(path());
	QString suffix = QFileInfo(p).suffix();
	QByteArray content;
	{
		QFile fin(p);
		if (!fin.open(QFile::ReadOnly | QFile::Text))
			return false;
		content = fin.readAll();
	}

	size_t hash = vipHashBytes(content.data(), content.size());
	{

		// Check inside buffered scene model
		QMutexLocker lock(&_shape_buffer_mutex);
		if (_shape_buffer.fname == p && hash == _shape_buffer.hash) {
			setData(_shape_buffer.sceneModel);
			setOpenMode(ReadOnly);
			return true;
		}
	}

	if (suffix == "xml") {

		VipXIfArchive arch(removePrefix(path()));
		if (arch.isOpen()) {
			VipSceneModel model;
			VipSceneModelList lst;

			arch.save();
			if (arch.content(model)) {
				setData(QVariant::fromValue(model));
				setOpenMode(ReadOnly);

				QMutexLocker lock(&_shape_buffer_mutex);
				_shape_buffer.fname = p;
				_shape_buffer.hash = hash;
				_shape_buffer.sceneModel = QVariant::fromValue(model);
				return true;
			}
			else {
				arch.restore();
				if (arch.content(lst)) {
					setData(QVariant::fromValue(lst));
					setOpenMode(ReadOnly);

					QMutexLocker lock(&_shape_buffer_mutex);
					_shape_buffer.fname = p;
					_shape_buffer.hash = hash;
					_shape_buffer.sceneModel = QVariant::fromValue(lst);

					return true;
				}
			}
		}
	}
	else if (suffix == "json") {
		QString error;
		VipSceneModelList lst = vipSceneModelListFromJSON(content, &error);
		if (!error.isEmpty()) {
			setError(error);
			return false;
		}
		if (lst.size() == 0)
			setData(QVariant::fromValue(VipSceneModel()));
		else if (lst.size() == 1)
			setData(QVariant::fromValue(lst[0]));
		else
			setData(QVariant::fromValue(lst));

		QMutexLocker lock(&_shape_buffer_mutex);
		_shape_buffer.fname = p;
		_shape_buffer.hash = hash;
		_shape_buffer.sceneModel = data();

		setOpenMode(ReadOnly);
		return true;
	}
	return false;
}

VipShapeWriter::VipShapeWriter() {}

bool VipShapeWriter::open(VipIODevice::OpenModes mode)
{
	if (!(mode & WriteOnly))
		return false;

	QFile file(removePrefix(path()));
	if (file.open(QFile::WriteOnly)) {
		setOpenMode(WriteOnly);
		return true;
	}
	return false;
}

void VipShapeWriter::apply()
{
	if (isOpen()) {

		QString p = removePrefix(path());
		if (QFileInfo(p).suffix() == "xml") {

			VipXOfArchive arch(removePrefix(path()));
			if (arch.isOpen()) {
				VipAnyData any = inputAt(0)->data();
				if (any.data().userType() == qMetaTypeId<VipSceneModel>())
					arch.content(any.value<VipSceneModel>());
				else
					arch.content(any.value<VipSceneModelList>());
				if (!arch) {
					setError("unable to write scene model", VipProcessingObject::IOError);
				}
			}
		} 
		else if (QFileInfo(p).suffix() == "json") {

			QFile out(p);
			if (!out.open(QFile::WriteOnly | QFile::Text)) {
				setError("unable to open output file", VipProcessingObject::IOError);
				return;
			}
			VipAnyData any = inputAt(0)->data();
			QTextStream str(&out);
			if (any.data().userType() == qMetaTypeId<VipSceneModel>())
				vipSceneModelToJSON(str, any.value<VipSceneModel>());
			else
				vipSceneModelListToJSON(str, any.value<VipSceneModelList>());
		}
	}
}


#ifdef VIP_WITH_HDF5

typedef VipArchiveRecorder::Trailer ArchiveRecorderTrailer;
Q_DECLARE_METATYPE(ArchiveRecorderTrailer);


VipArchive& operator<<(VipArchive& arch, const ArchiveRecorderTrailer& trailer)
{
	arch.content("startTime", trailer.startTime).content("endTime", trailer.endTime);
	arch.start("Sources");
	for (auto it = trailer.sources.begin(); it != trailer.sources.end(); ++it) {
		arch.start(QString::number(it.key()));
		arch.content("name", QString(it.value().typeName));
		arch.content("limits", it.value().limits);
		arch.content("samples", it.value().samples);
		arch.end();
	}
	arch.end();
	return arch;
}

VipArchive& operator>>(VipArchive& arch, ArchiveRecorderTrailer& trailer)
{
	trailer.startTime = arch.read("startTime").value<qint64>();
	trailer.endTime = arch.read("endTime").value<qint64>();
	arch.start("Sources");
	arch.save();

	while (true) {
		ArchiveRecorderTrailer::Source src;
		QString source;
		if (!arch.start(source))
			break;
		
		src.typeName = arch.read("name").toString().toLatin1();
		src.limits = arch.read("limits").value<VipTimeRange>();
		src.samples = arch.read("samples").toLongLong();
		arch.end();

		trailer.sources.insert(source.toLongLong(), src);
	}

	arch.restore();
	arch.end();

	return arch;
}

static VipArchive& saveAnyData(VipArchive& stream, const VipAnyData& any)
{
	if (stream.start("data", any.attributes())) {
		stream.content("source", any.source()).content("time", any.time());
		if (!stream.attribute("skip_data", false))
			stream.content("value", any.data());
		stream.end();
	}
	return stream;
}

VipArchive& loadAnyData(VipArchive& stream, VipAnyData& any, const QString & name = QString())
{
	QVariantMap attrs;
	if (stream.start(name.isEmpty() ? QString("data") : name, attrs)) {
		any.setSource(stream.read("source").toLongLong());
		any.setTime(stream.read("time").toLongLong());
		if (!stream.attribute("skip_data", false))
			any.setData(stream.read("value"));
		any.setAttributes(attrs);
		stream.end();
	}
	return stream;
}

static int vipRegisterArchiveStreamOperators()
{
	vipRegisterArchiveStreamOperators<ArchiveRecorderTrailer>();
	return 0;
}
static int _registerArchiveStreamOperators = vipAddInitializationFunction(&vipRegisterArchiveStreamOperators);

#ifdef VIP_WITH_HDF5
#include "VipH5Archive.h"
#endif

class VipArchiveRecorder::PrivateData
{
public:
#ifdef VIP_WITH_HDF5
	VipH5Archive archive;
#else
	VipBinaryArchive archive;
#endif
	Trailer trailer;
	QMap<qint64, qint64> previousTimes;
};

VipArchiveRecorder::VipArchiveRecorder(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->archive.registerFastType(qMetaTypeId<VipAnyData>());
	d_data->archive.registerFastType(qMetaTypeId<QVariantMap>());
}

VipArchiveRecorder::~VipArchiveRecorder()
{
	close();
}

bool VipArchiveRecorder::open(VipIODevice::OpenModes mode)
{
	if (isOpen()) {
		this->wait();
		d_data->archive.content("Streams", d_data->trailer);
	}
	d_data->archive.close();
	d_data->trailer = ArchiveRecorderTrailer();
	d_data->previousTimes.clear();
	setOpenMode(NotOpen);
	this->setSize(0);

	if (mode == VipIODevice::WriteOnly) {
		if (!createDevice(removePrefix(path()), QIODevice::ReadWrite|QIODevice::Truncate))
			return false;

		if (!d_data->archive.open(device())) { 
			close();
			return false;
		}
		if (!d_data->archive.start("Content",this->attributes())) {
			close();
			return false;
		}
		
		this->setOpenMode(mode);
		return true;
	}

	return false;
}

void VipArchiveRecorder::close()
{
	if (isOpen()) {
		this->wait();
		d_data->archive.content("Streams", d_data->trailer);
		d_data->archive.end();
	}
	d_data->archive.close();
	d_data->trailer = ArchiveRecorderTrailer();
	d_data->previousTimes.clear();
	setOpenMode(NotOpen);
	this->setSize(0);
	VipIODevice::close();
}

VipArchiveRecorder::Trailer VipArchiveRecorder::trailer() const
{
	return d_data->trailer;
}

void VipArchiveRecorder::apply()
{
	int input_count = inputCount();

	// try to grab all available data
	while (true) {
		QMultiMap<qint64, VipAnyData> to_save;

		for (int i = 0; i < input_count; ++i) {
			if (inputAt(i)->hasNewData()) {
				VipAnyData data = inputAt(i)->data();
				data.setSource(i+1);
				// check that we are above the previous time for this source
				QMap<qint64, qint64>::iterator it = d_data->previousTimes.find(data.source());
				if (it == d_data->previousTimes.end() || data.time() > it.value())
					d_data->previousTimes[data.source()] = data.time();
				else
					continue;

				to_save.insert(data.time(), data);
			}
		}

		if (!to_save.size())
			break;

		// now save all the data
		for (QMultiMap<qint64, VipAnyData>::iterator it = to_save.begin(); it != to_save.end(); ++it) {
			const VipAnyData data = it.value();
			// skip empty data
			if (data.isEmpty())
				continue;

			// update trailer
			auto src = d_data->trailer.sources.find(data.source());
			if (src == d_data->trailer.sources.end()) {
				src = d_data->trailer.sources.insert(data.source(), { data.data().typeName(), VipTimeRange(data.time(), data.time()),1 });
			}
			else {
				if (data.time() < src.value().limits.first)
					src.value().limits.first = data.time();
				else if (data.time()  > src.value().limits.second )
					src.value().limits.second = data.time();
				src.value().samples++;
			}
			
			if (d_data->trailer.startTime == VipInvalidTime)
				d_data->trailer.startTime = (qint64)src.value().limits.first;
			else
				d_data->trailer.startTime = qMin(d_data->trailer.startTime, (qint64)src.value().limits.first);

			if (d_data->trailer.endTime == VipInvalidTime)
				d_data->trailer.endTime = (qint64)src.value().limits.second;
			else
				d_data->trailer.endTime = qMax(d_data->trailer.endTime, (qint64)src.value().limits.second);

			// write data
			saveAnyData(d_data->archive, data);

			this->setSize(size() + 1);
		}
	}
}

struct ArchFrame
{
	qint64 stream;
	qint64 time;
	QByteArray name;

};

class VipArchiveReader::PrivateData
{
public:
	PrivateData()
	  : time(VipInvalidTime)
	  , buffer_time(VipInvalidTime)
	{
	}

	//VipBinaryArchive archive;
	//TEST
	VipH5Archive archive;
	ArchiveRecorderTrailer trailer;

	VipArchiveReader::DeviceType device_type;

	// map of source -> index
	QMap<qint64, int> indexes;

	QMultiMap<qint64, ArchFrame> frames;
	QMultiMap<qint64, ArchFrame> resourceFrames; // frame with invalid time

	// store the last read data in a buffer to avoid reading the data from the file when reloading
	QMap<qint64, VipAnyData> buffer;

	// optimization: buffer next (or previous) data
	QMutex buffer_mutex;
	QVector<VipAnyData> buffers;
	qint64 time;
	qint64 buffer_time;
	VipTimeRangeList ranges;

	unsigned start_pos;
};

VipArchiveReader::VipArchiveReader(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->device_type = Temporal;
	// connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(bufferData()), Qt::DirectConnection);
}

VipArchiveReader::~VipArchiveReader()
{
	close();
}

void VipArchiveReader::bufferData()
{
}

bool VipArchiveReader::open(VipIODevice::OpenModes mode)
{
	d_data->archive.close();
	d_data->trailer = ArchiveRecorderTrailer();
	d_data->frames.clear();
	d_data->resourceFrames.clear();
	setOpenMode(NotOpen);

	if (mode != VipIODevice::ReadOnly)
		return false;
	
	if (!createDevice(removePrefix(path()), QFile::ReadOnly))
		return false;

	d_data->device_type = Temporal;
	if (!d_data->archive.open(device()))
		return false;
	//d_data->archive.setDevice(device());

	// Start "Content" object and read its attributes
	QVariantMap attrs;
	if (!d_data->archive.start("Content", attrs)){
		close();
		return false;
	}
	mergeAttributes(attrs);

	// Save position
	d_data->start_pos = d_data->archive.save();

	auto pos1 = d_data->archive.currentGroup();

	// Read the trailer
	if (!d_data->archive.content("Streams", d_data->trailer)) {
		close();
		return false;
	}

	auto pos2 = d_data->archive.currentGroup();
		
	// Create the outputs with a valid value
	bool all_resource = true;
	int i = 0;
	topLevelOutputAt(0)->toMultiOutput()->resize(d_data->trailer.sources.size());
	for (auto it = d_data->trailer.sources.begin(); it != d_data->trailer.sources.end(); ++it, ++i) {
		QVariant v = vipCreateVariant(it.value().typeName.data());
		outputAt(i)->setData(VipAnyData(v, VipInvalidTime));

		d_data->indexes[it.key()] = i;

		VipTimeRange range = it.value().limits;
		if (range.second - range.first != 0)
			all_resource = false;
	}

	// if all streams only have one data, set the device type to Resource
	if (all_resource)
		d_data->device_type = Resource;

	// now read all data without their content

	d_data->archive.restore(d_data->start_pos);
	auto pos3 = d_data->archive.currentGroup();
	d_data->start_pos = d_data->archive.save();
	d_data->archive.setAttribute("skip_data", true);

	qint64 count = 0;
	while (true) {
		VipAnyData any;
		loadAnyData(d_data->archive, any);
		if (any.source() != 0) {
			QByteArray dname = d_data->archive.lastEndGroup();
			int idx = dname.lastIndexOf("/");
			dname = dname.mid(idx + 1);
			d_data->frames.insert(any.time(), ArchFrame{ any.source(), any.time(), dname });
			if (d_data->trailer.sources.size() == 1)
				++count;
		}
		else
			break;
	}

	d_data->archive.setAttribute("skip_data", false);

	// affect a valid data to each output
	QSet<qint64> streams;
	for (QMultiMap<qint64, ArchFrame>::iterator it = d_data->frames.begin(); it != d_data->frames.end(); ++it) {
		if (!streams.contains(it.value().stream)) {

			d_data->archive.restore(d_data->start_pos);
			d_data->start_pos = d_data->archive.save();

			VipAnyData any;
			loadAnyData(d_data->archive, any, it.value().name);

			if (!any.isEmpty()) {
				streams.insert(it.value().stream);
				int index = d_data->indexes[it.value().stream];
				any.setSource((qint64)this);
				if (!any.hasAttribute("Name"))
					any.setAttribute("Name", this->name());
				outputAt(index)->setData(any);

				if (streams.size() == d_data->trailer.sources.size())
					break;
			}
		}
	}

	// move the frames with invalid times to resourceFrames
	typedef QMultiMap<qint64, ArchFrame>::iterator iterator;
	QPair<iterator, iterator> range = d_data->frames.equal_range(VipInvalidTime);
	for (iterator it = range.first; it != range.second; ++it)
		d_data->resourceFrames.insert(it.key(), it.value());

	while (d_data->frames.size()) {
		if (d_data->frames.begin().key() == VipInvalidTime)
			d_data->frames.erase(d_data->frames.begin());
		else
			break;
	}

	// if the device is temporal and we have resource frames, move them at the beginning of the frames
	// we should avoid mixing resource and temporal outputs in the same VipIODevice, or the outputs won't be properly saved
	if (d_data->frames.size() && d_data->resourceFrames.size() && d_data->device_type == Temporal) {
		qint64 start = d_data->frames.begin().key();
		for (iterator it = d_data->resourceFrames.begin(); it != d_data->resourceFrames.end(); ++it) {
			ArchFrame frame = it.value();
			frame.time = start;
			d_data->frames.insert(start, frame);
		}
		d_data->resourceFrames.clear();
	}

	d_data->archive.setAttribute("skip_data", false);
	d_data->buffers.resize(this->outputCount());

	
	if (count > 0) {
		setSize(count);

		// if there is only one stream, recreate the time range list
		d_data->ranges.clear();

		// find smallest sampling time
		// int i = 0;
		qint64 prev = 0;
		qint64 sampling = 0;
		for (QMultiMap<qint64, ArchFrame>::iterator it = d_data->frames.begin(); it != d_data->frames.end(); ++it /*, ++i*/) {
			if (i > 0) {
				qint64 samp = it.key() - prev;
				if (samp > 0) {
					if (sampling == 0)
						sampling = samp;
					else
						sampling = qMin(sampling, samp);
				}
			}
			prev = it.key();
		}
		if (sampling == 0) {
			// case no valid sampling time found
			d_data->ranges << VipTimeRange(d_data->trailer.startTime, d_data->trailer.endTime);
		}
		else {
			QMultiMap<qint64, ArchFrame>::iterator it = d_data->frames.begin();
			qint64 first = it.key();
			qint64 last = it.key();
			++it;
			for (; it != d_data->frames.end(); ++it) {
				if (it.key() - last < sampling * 4)
					last = it.key();
				else {
					d_data->ranges.append(VipTimeRange(first, last));
					first = last = it.key();
				}
			}
			d_data->ranges.append(VipTimeRange(first, last));
		}
	}
	else
		d_data->ranges = VipTimeRangeList() << VipTimeRange(d_data->trailer.startTime, d_data->trailer.endTime);

	this->setOpenMode(mode);
	return true;
}

VipArchiveReader::DeviceType VipArchiveReader::deviceType() const
{
	return d_data->device_type;
}

void VipArchiveReader::close()
{
	d_data->archive.close();
	d_data->trailer = ArchiveRecorderTrailer();
	d_data->frames.clear();
	d_data->resourceFrames.clear();
	VipIODevice::close();
}

bool VipArchiveReader::probe(const QString& filename, const QByteArray& first_bytes) const
{
	if (filename.startsWith(className() + ":"))
		return true;

	QFileInfo info(filename);
	if (info.suffix().compare("arch", Qt::CaseInsensitive) == 0)
		return true;

	if (first_bytes.size()) {
		VipBinaryArchive arch(first_bytes);
		VipAnyData any;
		if (arch.content(any))
			return true;
	}

	return false;
}

VipArchiveRecorder::Trailer VipArchiveReader::trailer() const
{
	return d_data->trailer;
}

VipTimeRangeList VipArchiveReader::computeTimeWindow() const
{
	return d_data->ranges;
}

qint64 VipArchiveReader::computeNextTime(qint64 time) const
{
	if (time >= d_data->trailer.endTime)
		return d_data->trailer.endTime;
	else if (time < d_data->trailer.startTime)
		return d_data->trailer.startTime;

	QMultiMap<qint64, ArchFrame>::const_iterator it = d_data->frames.upperBound(time);
	if (it != d_data->frames.end())
		return it.key();
	else
		return VipInvalidTime;
}

qint64 VipArchiveReader::computePreviousTime(qint64 time) const
{
	if (time > d_data->trailer.endTime)
		return d_data->trailer.endTime;
	else if (time <= d_data->trailer.startTime)
		return d_data->trailer.startTime;

	QMultiMap<qint64, ArchFrame>::const_iterator it = d_data->frames.lowerBound(time);
	if (it != d_data->frames.end()) {
		--it;
		if (it != d_data->frames.end())
			return it.key();
		else
			return VipInvalidTime;
	}
	else
		return VipInvalidTime;
}

qint64 VipArchiveReader::computeClosestTime(qint64 time) const
{
	if (time >= d_data->trailer.endTime)
		return d_data->trailer.endTime;
	else if (time <= d_data->trailer.startTime)
		return d_data->trailer.startTime;

	QMultiMap<qint64, ArchFrame>::const_iterator it = d_data->frames.lowerBound(time);
	if (it != d_data->frames.end()) {
		if (it.key() == time)
			return time;

		// get the previous time
		QMultiMap<qint64, ArchFrame>::const_iterator it_prev = it;
		--it_prev;
		if (it_prev == d_data->frames.end())
			return it.key();

		qint64 second = it.key();
		qint64 first = it_prev.key();
		if (qAbs(second - time) < qAbs(first - time))
			return second;
		else
			return first;
	}
	else
		return VipInvalidTime;
}

bool VipArchiveReader::readData(qint64 time)
{
	typedef QMultiMap<qint64, ArchFrame>::iterator iterator;
	QPair<iterator, iterator> range = d_data->frames.equal_range(time);

	if (range.first == range.second)
		return false;


	for (iterator it = range.first; it != range.second; ++it) {
		int output_index = d_data->indexes[it.value().stream];

		// for streams having just one data or (Resource stream), just reset the output
		VipTimeRange _range = d_data->trailer.sources[it.value().stream].limits;
		if (_range.second - _range.first == 0) {
			outputAt(output_index)->setData(outputAt(output_index)->data());
			continue;
		}

		QByteArray dname = it.value().name;
		d_data->archive.restore(d_data->start_pos);
		d_data->start_pos = d_data->archive.save();
		VipAnyData any;
		if (loadAnyData(d_data->archive, any, dname)) {
			if (!any.hasAttribute("Name"))
				any.setAttribute("Name", this->name());
			any.setSource(this);
		}

		outputAt(output_index)->setData(any);
	}
	
	d_data->time = time;
	return true;
}

bool VipArchiveReader::reload()
{
	// bool res =
	VipIODevice::reload();
	// also reload the resource data
	typedef QMultiMap<qint64, ArchFrame>::iterator iterator;
	QSet<qint64> sources;
	for (iterator it = d_data->resourceFrames.begin(); it != d_data->resourceFrames.end(); ++it) {
		if (!sources.contains(it.value().stream)) {

			VipAnyData any;
			QByteArray dname = it.value().name;
			d_data->archive.restore(d_data->start_pos);
			d_data->start_pos = d_data->archive.save();
			if (d_data->archive.content(dname, any)) {
				if (!any.hasAttribute("Name"))
					any.setAttribute("Name", this->name());
			}
			any.setSource(qint64(this));
			outputAt(d_data->indexes[it.value().stream])->setData(any);
			sources.insert(it.value().stream);
			
		}
	}
	return true;
}


#endif //VIP_WITH_HDF5


VipArchive& operator<<(VipArchive& stream, const VipIODevice* d)
{
	// serialize the path as editable for read-only devices based on a file/directory path
	bool serialized = false;
	if (d->supportedModes() & VipIODevice::ReadOnly) {
		QString path = d->removePrefix(d->path());
		if (!path.isEmpty() && (QFileInfo(path).exists() || QDir(path).exists())) {
			bool is_file = QFileInfo(path).exists();
			QString style_sheet;
			if (is_file)
				style_sheet = "VipFileName{  qproperty-mode:'Open'; qproperty-value:'" + d->path() + "' ;qproperty-title:'Open " + vipSplitClassname(d->metaObject()->className()) +
					      "' ;qproperty-filters:'" + d->fileFilters() + "' ;}";
			else
				style_sheet = "VipFileName{  qproperty-mode:'OpenDir'; qproperty-value:'" + d->path() + "' ;qproperty-title:'Open " + vipSplitClassname(d->metaObject()->className()) +
					      "' ;qproperty-filters:'" + d->fileFilters() + "' ;}";
			stream.content("path", d->path(), vipEditableSymbol("Input path", style_sheet));
			serialized = true;
		}
	}
	if (!serialized)
		stream.content("path", d->path());

	stream.content("filter", d->timestampingFilter());

	// serialize the VipMapFileSystem as a lazy pointer
	if (d->mapFileSystem())
		stream.content("mapFileSystem", d->mapFileSystem()->lazyPointer().id());
	else
		stream.content("mapFileSystem", -1);
	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipIODevice* d)
{
	d->setPath(stream.read("path").value<QString>());
	d->setTimestampingFilter(stream.read("filter").value<VipTimestampingFilter>());
	// load the VipMapFileSystem
	int id = stream.read("mapFileSystem").toInt();
	VipMapFileSystem* map = VipUniqueId::find<VipMapFileSystem>(id);
	if (map)
		d->setMapFileSystem(map->sharedPointer());
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipAnyResource* d)
{
	stream.content("data", d->data());
	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipAnyResource* d)
{
	d->setData(stream.read("data"));
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipProcessingPool* r)
{
	// new in 3.3.0
	stream.content("name", r->objectName());

	QVariantMap attributes;
	attributes["time"] = r->time();
	stream.start("processings", attributes);
	QList<VipProcessingObject*> lst = findDirectChildren<VipProcessingObject*>(r);
	for (int i = 0; i < lst.size(); ++i) {
		if (!lst[i]->property("_vip_no_serialize").toBool())
			stream.content(lst[i]);
	}
	stream.end();

	return stream;
}

VipArchive& operator>>(VipArchive& stream, VipProcessingPool* r)
{
	// new in 3.3.0
	stream.save();
	QString name;
	if (stream.content("name", name))
		setPoolObjectName(r, name);
	else
		stream.restore();

	QVariantMap attributes;
	stream.start("processings", attributes);
	// load all VipProcessingObject
	while (!stream.hasError()) {
		if (VipProcessingObject* obj = stream.read().value<VipProcessingObject*>()) {
			obj->setParent(r);

			// open the read only devices
			if (VipIODevice* device = qobject_cast<VipIODevice*>(obj)) {
				if (device->supportedModes() & VipIODevice::ReadOnly)
					device->open(VipIODevice::ReadOnly);
			}
		}
	}

	bool removeProcessingPoolFromAddresses = stream.property("_vip_removeProcessingPoolFromAddresses").toBool();

	// open all connections
	QList<VipProcessingObject*> children = findDirectChildren<VipProcessingObject*>(r);

	if (removeProcessingPoolFromAddresses) {
		for (int i = 0; i < children.size(); ++i)
			children[i]->removeProcessingPoolFromAddresses();
	}

	for (int i = 0; i < children.size(); ++i) {
		children[i]->openAllConnections();
	}

	if (attributes.find("time") != attributes.end()) {
		qint64 time = attributes["time"].toLongLong();
		r->read(time);
	}
	else
		r->reload();
	stream.resetError();
	stream.end();
	return stream;
}

VipArchive& operator<<(VipArchive& arch, const VipTimeRangeBasedGenerator* r)
{
	return arch.content("timestamps", r->timestamps()).content("timeWindow", r->computeTimeWindow()).content("stepSize", r->samplingTime());
}

VipArchive& operator>>(VipArchive& arch, VipTimeRangeBasedGenerator* r)
{
	VipTimestamps tstamps = arch.read("timestamps").value<VipTimestamps>();
	VipTimeRangeList twindow = arch.read("timeWindow").value<VipTimeRangeList>();
	qint64 stepsize = arch.read("stepSize").value<qint64>();
	if (tstamps.size())
		r->setTimestamps(tstamps);
	else
		r->setTimeWindows(twindow, stepsize);
	VipTimeRangeList lst = r->timeWindow();

	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipTextFileReader* r)
{
	return arch.content("type", (int)r->type());
}

VipArchive& operator>>(VipArchive& arch, VipTextFileReader* r)
{
	r->setType((VipTextFileReader::Type)arch.read("type").value<int>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipTextFileWriter* r)
{
	return arch.content("type", (int)r->type()).content("digits", r->digitsNumber());
}

VipArchive& operator>>(VipArchive& arch, VipTextFileWriter* r)
{
	r->setType((VipTextFileWriter::Type)arch.read("type").value<int>());
	r->setDigitsNumber((VipTextFileWriter::Type)arch.read("digits").value<int>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipImageReader* r)
{
	return arch.content("samplingTime", (int)r->samplingTime());
}

VipArchive& operator>>(VipArchive& arch, VipImageReader* r)
{
	r->setSamplingTime(arch.read("samplingTime").value<qint64>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipImageWriter* r)
{
	return arch.content("type", (int)r->type()).content("digits", r->digitsNumber());
}

VipArchive& operator>>(VipArchive& arch, VipImageWriter* r)
{
	r->setType((VipImageWriter::Type)arch.read("type").value<int>());
	r->setDigitsNumber(arch.read("digits").value<int>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipDirectoryReader* r)
{
	arch.content("supportedSuffixes", r->supportedSuffixes());
	arch.content("fixedSize", r->fixedSize());
	arch.content("fileCount", r->fileCount());
	arch.content("fileStart", r->fileStart());
	arch.content("smoothResize", r->smoothResize());
	arch.content("alphabeticalOrder", r->alphabeticalOrder());
	arch.content("type", (int)r->type());
	arch.content("recursive", r->recursive());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipDirectoryReader* r)
{
	r->setSupportedSuffixes(arch.read("supportedSuffixes").value<QStringList>());
	r->setFixedSize(arch.read("fixedSize").value<QSize>());
	r->setFileCount(arch.read("fileCount").value<int>());
	r->setFileStart(arch.read("fileStart").value<int>());
	r->setSmoothResize(arch.read("smoothResize").value<bool>());
	r->setAlphabeticalOrder(arch.read("alphabeticalOrder").value<bool>());
	r->setType(VipDirectoryReader::Type(arch.read("type").value<int>()));
	r->setRecursive(arch.read("recursive").value<bool>());
	return arch;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipIODevice*>();
	vipRegisterArchiveStreamOperators<VipProcessingPool*>();
	vipRegisterArchiveStreamOperators<VipAnyResource*>();

	vipRegisterArchiveStreamOperators<VipTimeRangeBasedGenerator*>();
	vipRegisterArchiveStreamOperators<VipTextFileReader*>();
	vipRegisterArchiveStreamOperators<VipTextFileWriter*>();
	vipRegisterArchiveStreamOperators<VipImageReader*>();
	vipRegisterArchiveStreamOperators<VipImageWriter*>();
	vipRegisterArchiveStreamOperators<VipDirectoryReader*>();

	return 0;
}

static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);
