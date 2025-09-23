#include <QFileInfo>
#include <QSharedPointer>
#include <QMutex>

#include "VipProgress.h"
#include "VipConcatenateVideos.h"
#include "VipLogging.h"



class VipConcatenateVideos::PrivateData
{
public:
	
	QVector<Frame> frames;
	QMap<QString, QSharedPointer<VipIODevice>> suffix_templates;
	bool bufferize = false;
	QRecursiveMutex mutex;
};

VipConcatenateVideos::VipConcatenateVideos(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	propertyAt(0)->setData(0);
	propertyAt(1)->setData(std::numeric_limits<double>::infinity());
	propertyAt(2)->setData(1);
	propertyAt(3)->setData(true);
}

VipConcatenateVideos::~VipConcatenateVideos() {}

void VipConcatenateVideos::setSourceProperty(const char* name, const QVariant& value)
{
	VipIODeviceSPtr prev;
	for (auto& frame : d_data->frames) {
		if (frame.device) {
			if (frame.device != prev) {
				prev = frame.device;
				prev->setSourceProperty(name, value);
			}
		}
	}
}

bool VipConcatenateVideos::probe(const QString& filename, const QByteArray&) const
{
	// filename is a list of files sepârated by a ';'
	auto lst = filename.split(";");
	// check that at least on file exists
	for (const QString& f : lst)
		if (QFileInfo(f).exists())
			return true;
	return false;
}

void VipConcatenateVideos::setSuffixTemplate(const QString& suffix, VipIODevice* device)
{
	d_data->suffix_templates[suffix.toLower()] = QSharedPointer<VipIODevice>(device);
}

bool VipConcatenateVideos::open(VipIODevice::OpenModes mode)
{
	close();
	d_data->frames.clear();
	resetError();

	double start_time = (propertyAt(0)->value<double>() * 1000000000LL);
	double end_time = (propertyAt(1)->value<double>() * 1000000000LL);
	d_data->bufferize = propertyAt(3)->value<bool>();
	if (start_time > end_time) {
		setError("Invalid start/end times");
		return false;
	}
	qint64 skip = propertyAt(2)->value<qint64>();
	if (skip < 1)
		skip = 1;

	QString p = removePrefix(path());
	const QStringList lst = p.split(";", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.isEmpty())
		return false;

	VipProgress progress;
	progress.setRange(0, lst.size());
	progress.setCancelable(true);
	progress.setModal(true);

	std::atomic<qint64> sampling(0);
	
	for (qsizetype i = 0; i < lst.size(); i += 4) {
		
		progress.setValue(i);
		progress.setText("<b>Process</b> " + QFileInfo(lst[i]).fileName());

		if (progress.canceled())
			break;

		int end = i + 4;
		if (end > lst.size())
			end = lst.size();

		QVector<FrameVector> devices(end);

		

	#pragma omp parallel for
		for (int j = i; j < end; ++j) {
			const QString& fname = lst[j];
			if (QFileInfo(fname).exists()) {

				bool have_template = false;
				QSharedPointer<VipIODevice> template_device;
				QMap<QString, QSharedPointer<VipIODevice>>::iterator it = d_data->suffix_templates.find(QFileInfo(fname).suffix().toLower());
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
					QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(fname, QByteArray());
					if (devices.size())
						device = qobject_cast<VipIODevice*>(devices.first().create());
				}

				if (!device)
					continue;

				device->setMapFileSystem(this->mapFileSystem());
				// copy the parameters from the template device
				if (template_device)
					template_device->copyParameters(device);

				device->setPath(fname);
				if (!device->open(VipIODevice::ReadOnly)) {
					delete device;
					continue;
				}

				// find start/end positions
				qint64 start_pos = 0;
				if (start_time != -std::numeric_limits<double>::infinity())
					start_pos = device->timeToPos((qint64)start_time + device->firstTime());
				qint64 end_pos = device->size();
				if (end_time != std::numeric_limits<double>::infinity())
					end_pos = device->timeToPos((qint64)end_time + device->firstTime());
				if (start_pos >= device->size() || end_pos <= 0) {
					delete device;
					continue;
				}

				qint64 sampling_time = device->estimateSamplingTime();
				QVector<VipAnyData> buffer;
				if (d_data->bufferize) {
					for (qint64 p = start_pos; p < end_pos; p += skip) {
						device->read(device->posToTime(p));
						buffer.push_back(device->outputAt(0)->data());
					}
					device->close();
					delete device;
					device = nullptr;
				}
				else
					device->moveToThread(this->thread());

				
				QSharedPointer<VipIODevice> sdev(device);
				
				FrameVector& frames = devices[j - i];

				// add frames
				size_t buf_pos = 0;
				for (qint64 p = start_pos; p < end_pos; p += skip, ++buf_pos) {
					if (d_data->bufferize)
						frames.push_back(Frame{ sdev, fname, p, std::move(buffer[buf_pos]), buffer[buf_pos].time() });
					else
						frames.push_back(Frame{ sdev, fname, p, VipAnyData(), device->posToTime(p) });
				}

				// switch to relative times
				for (qsizetype f = 0; f < frames.size(); ++f)
					frames[f].time -= frames[0].time;

				//set min sampling time
				while (true){
					auto val = sampling.load();
					if ((val == 0 || val > sampling_time) && sampling_time != 0) {
						if (sampling.compare_exchange_strong(val, sampling_time))
							break;
					}
					else
						break;
				}
				
			}
		}

		for (const auto& fr : devices) {
			d_data->frames.append(fr);
		}
	}

	if (d_data->frames.isEmpty())
		return false;

	if (sampling == 0 || sampling == VipInvalidTime)
		sampling = 20000000LL;

	// Set frame times
	qint64 last_time = 0;
	VipTimestamps timestamps;
	QString prev;

	timestamps.push_back(d_data->frames.first().time);
	for (qsizetype i = 1; i < d_data->frames.size(); ++i) {
	
		Frame& fr = d_data->frames[i];
		if (fr.time == 0) {
			last_time = d_data->frames[i - 1].time + sampling.load();
		}
		fr.time += last_time;
		timestamps.push_back(fr.time);
	}

	// Compute name
	QString name = QFileInfo(lst.first()).absoluteDir().absolutePath();
	name = name.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts).last();
	setAttribute("Name", name);

	setTimestamps(timestamps);
	setOpenMode(ReadOnly);
	return true;
}

int VipConcatenateVideos::deviceCount() const
{
	std::lock_guard<QRecursiveMutex> lock(const_cast<QRecursiveMutex&>(d_data->mutex));
	QSet<QString> set;
	for (auto& f : d_data->frames) {
		set.insert(f.path);
	}
	return set.size();
}

QVector<VipConcatenateVideos::Frame> VipConcatenateVideos::frames() const
{
	std::lock_guard<QRecursiveMutex> lock(const_cast<QRecursiveMutex&>(d_data->mutex));
	auto res = d_data->frames;
	res.detach();
	return res;
}
void VipConcatenateVideos::setFrames(const QVector<Frame>& frs)
{
	std::lock_guard<QRecursiveMutex> lock(d_data->mutex);

	d_data->frames = frs;
	VipTimestamps timestamps;

	for (const auto& f : frs) {
		timestamps.push_back(f.time);
	}
	setTimestamps(timestamps);
}

bool VipConcatenateVideos::readData(qint64 time)
{
	std::lock_guard<QRecursiveMutex> lock(d_data->mutex);

	qint64 pos = computeTimeToPos(time);
	if (pos < 0)
		pos = 0;
	else if (pos >= size())
		pos = size() - 1;

	auto& frame = d_data->frames[pos];

	VipAnyData any;
	qint64 ftime = VipInvalidTime;
	if (d_data->bufferize) {
		any = frame.any;
		ftime = any.time();
	}
	else {

		ftime = frame.device->posToTime(frame.pos);
		frame.device->read(ftime);
		any = frame.device->outputAt(0)->data();
	}
	if (any.isEmpty())
		return false;

	any.mergeAttributes(this->attributes());
	any.setAttribute("Sub-video name", QFileInfo(frame.path).fileName());
	any.setAttribute("Sub-video frame", frame.pos);
	any.setAttribute("Sub-video time(ns)", ftime);
	any.setTime(time);
	any.setSource(this);
	outputAt(0)->setData(any);
	return true;
}

QStringList VipConcatenateVideos::listFiles(VipMapFileSystem* map, const QString& _dirname, const QStringList& suffixes, SortOption sort, bool recursive)
{
	QString dirname = _dirname;
	dirname.replace("\\", "/");
	if (dirname.endsWith("/"))
		dirname = dirname.mid(0, dirname.size() - 1);

	std::unique_ptr<VipMapFileSystem> _map;
	if (!map)
		_map.reset(map = new VipPhysicalFileSystem());

	// compute all files in dir
	VipPathList paths = map->list(VipPath(dirname, true), recursive);
	QStringList files;
	for (int i = 0; i < paths.size(); ++i)
		if (!paths[i].isDir())
			files << paths[i].canonicalPath();

	// remove files that do not match the filters

	if (suffixes.size()) {
		for (int i = 0; i < files.size(); ++i) {
			QString suffix = QFileInfo(files[i]).suffix();
			if (!suffixes.contains(suffix, Qt::CaseInsensitive)) {
				files.removeAt(i);
				--i;
			}
		}
	}

	// sort
	files.sort();
	if (sort == Reversed) {
		// reverse the list
		std::reverse(files.begin(), files.end());
	}
	else if (sort == UseTrailingNumber) {
		QMap<qint64, QString> sfiles;
		for (const QString& f : files) {
			int start_fname = f.lastIndexOf("/") + 1;
			int suffix_pos = f.lastIndexOf(".");
			QString basename = f.mid(start_fname, suffix_pos - start_fname); // if suffix_pos is -1, take until the string end
			basename.replace(";", "_");
			basename.replace(".", "_");
			basename.replace("-", "_");
			auto lst = basename.split("_", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
			if (lst.size()) {
				bool ok = true;
				qint64 val = lst.last().toLongLong(&ok);
				if (!ok)
					return files;
				sfiles.insert(val, f);
			}
		}
		return sfiles.values();
	}
	return files;
}



class VipConcatenateVideosManager::PrivateData
{
public:
	QPointer<VipConcatenateVideos> device;
	QList<VipConcatenateVideos::FrameVector> undoStates;
	QList<VipConcatenateVideos::FrameVector> redoStates;
};


VipConcatenateVideosManager::VipConcatenateVideosManager(VipConcatenateVideos* device)
  : QObject()
{
	VIP_CREATE_PRIVATE_DATA();
	setDevice(device);
}

VipConcatenateVideosManager::~VipConcatenateVideosManager() {}

void VipConcatenateVideosManager::setDevice(VipConcatenateVideos* device)
{
	if (device != d_data->device) {
		d_data->device = device;
		resetState();
	}
}
VipConcatenateVideos* VipConcatenateVideosManager::device() const
{
	return d_data->device;
}

int VipConcatenateVideosManager::undoCount() const
{
	return d_data->undoStates.size();
}
int VipConcatenateVideosManager::redoCount() const
{
	return d_data->redoStates.size();
}

VipConcatenateVideos::FrameVector VipConcatenateVideosManager::saveState()
{
	VipConcatenateVideos::FrameVector res;
	if (!d_data->device)
		return res;

	res = d_data->device->frames();
	d_data->undoStates.append(d_data->device->frames());
	if (d_data->undoStates.size() > 50)
		d_data->undoStates.pop_front();

	// clear redo stack
	d_data->redoStates.clear();
	return res;
}

bool VipConcatenateVideosManager::removeDeviceAtTime(qint64 time)
{
	if (!d_data->device)
		return false;
	return removeDeviceAtPos(d_data->device->timeToPos(time));
}
bool VipConcatenateVideosManager::removeDeviceAtPos(qint64 pos)
{
	if (!d_data->device)
		return false;

	if (pos < 0 || pos >= d_data->device->size())
		return false;

	auto frames = d_data->device->frames();
	QString path = frames[pos].path;
	qsizetype start = -1;
	qsizetype end = -1;

	for (qsizetype i = 0; i < frames.size(); ++i) {
		if (path == frames[i].path) {
			if (start == -1)
				start = i;
		}
		else {
			if (start == -1)
				continue;
			end = i;
			break;
		}
	}
	if (end == -1)
		end = frames.size();
	frames.erase(frames.begin() + start, frames.begin() + end);

	// find sampling time
	qint64 sampling = 0;
	for (qsizetype i = 1; i < frames.size(); ++i) {
		qint64 val = frames[i].time - frames[i - 1].time;
		if (val > 0) {
			if (val < sampling || sampling == 0)
				sampling = val;
		}
	}
	if (sampling == 0)
		sampling = 20000000LL; // 20ms

	QString prev = frames[0].path;
	qint64 offset = 0;
	for (qsizetype i = 1; i < frames.size(); ++i) {
		if (prev != frames[i].path) {
			offset = (frames[i].time - frames[i - 1].time) - sampling;
			prev = frames[i].path;
		}
		frames[i].time -= offset;
	}


	if (!frames.isEmpty()) {
		saveState();
		d_data->device->setFrames(frames);
		return true;
	}
	return false;
}

void VipConcatenateVideosManager::undo()
{
	if (!d_data->device)
		return;
	if (!d_data->undoStates.size())
		return;

	// push current state to the redo stack
	d_data->redoStates.append(d_data->device->frames());
	if (d_data->redoStates.size() > 50)
		d_data->redoStates.pop_front();

	// undo
	if (d_data->undoStates.size() > 0) {
		auto frames = d_data->undoStates.last();
		d_data->device->setFrames(frames);
		d_data->undoStates.pop_back();

		Q_EMIT undoDone();
	}
}
void VipConcatenateVideosManager::redo()
{
	// redo
	if (!d_data->device)
		return;
	if (!d_data->redoStates.size())
		return;

	// push current state to undo stack
	d_data->undoStates.append(d_data->device->frames());
	if (d_data->undoStates.size() > 50)
		d_data->undoStates.pop_front();

	auto frames = d_data->redoStates.last();
	d_data->device->setFrames(frames);
	d_data->redoStates.pop_back();

	Q_EMIT redoDone();
	
}
void VipConcatenateVideosManager::resetState()
{
	d_data->undoStates.clear();
	d_data->redoStates.clear();
}