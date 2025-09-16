#include <QFileInfo>
#include <QSharedPointer>


#include "VipProgress.h"
#include "VipConcatenateVideos.h"
#include "VipLogging.h"

using VipIODeviceSPtr =  QSharedPointer<VipIODevice>;

struct Frame
{
	VipIODeviceSPtr device;
	qint64 pos;
};

class VipConcatenateVideos::PrivateData
{
public:
	QVector<VipIODeviceSPtr> devices;
	QVector<Frame> frames;
	QVector<qint64> timestamps;
	QMap<QString, QSharedPointer<VipIODevice>> suffix_templates;
};

VipConcatenateVideos::VipConcatenateVideos(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	propertyAt(0)->setData(0);
	propertyAt(1)->setData(std::numeric_limits<double>::infinity());
	propertyAt(2)->setData(1);
}

VipConcatenateVideos::~VipConcatenateVideos()
{
}

void VipConcatenateVideos::setSourceProperty(const char* name, const QVariant& value)
{
	for (VipIODeviceSPtr& d : d_data->devices)
		d->setSourceProperty(name, value);
}

bool VipConcatenateVideos::probe(const QString& filename, const QByteArray&) const
{
	// filename is a list of files sepârated by a ';'
	auto lst = filename.split(";");
	//check that at least on file exists
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
	d_data->devices.clear();
	d_data->frames.clear();
	d_data->timestamps.clear();
	resetError();

	double start_time = (propertyAt(0)->value<double>() * 1000000000LL);
	double end_time = (propertyAt(1)->value<double>() * 1000000000LL);
	if (start_time > end_time) {
		setError("Invalid start/end times");
		return false;
	}
	qint64 skip = propertyAt(2)->value<qint64>();
	if (skip < 1)
		skip = 1;

	QString p = removePrefix(path());
	QStringList lst = p.split(";", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.isEmpty())
		return false;

	VipProgress progress;
	progress.setRange(0, lst.size());
	progress.setCancelable(true);
	progress.setModal(true);

	qint64 time = 0;

	for (qsizetype i = 0; i < lst.size(); ++i) {
		const QString& fname = lst[i];
		progress.setValue(i);
		progress.setText("<b>Process</b> " + QFileInfo(fname).fileName());

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

			QSharedPointer<VipIODevice> sdev(device);
			d_data->devices.append(sdev);

			qint64 sampling = device->estimateSamplingTime();
			if (sampling == VipInvalidTime || sampling == 0)
				sampling = 20000000;//20ms

			// add frames
			for (qint64 p = start_pos; p < end_pos; p += skip) {
				d_data->frames.push_back(Frame{ sdev, p });
				d_data->timestamps.push_back(time);
				time += sampling;
			}
		}


	}

	if (d_data->timestamps.isEmpty())
		return false;

	setTimestamps(d_data->timestamps);
	setOpenMode(ReadOnly);
	return true;
}

bool VipConcatenateVideos::readData(qint64 time)
{
	qint64 pos = computeTimeToPos(time);
	if (pos < 0)
		pos = 0;
	else if (pos >= size())
		pos = size() - 1;

	auto& frame = d_data->frames[pos];
	qint64 ftime = frame.device->posToTime(frame.pos);
	frame.device->read(ftime);

	VipAnyData any = frame.device->outputAt(0)->data();
	if (any.isEmpty())
		return false;

	any.mergeAttributes(this->attributes());
	any.setAttribute("Sub-video name", QFileInfo(frame.device->path()).fileName());
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
	else if (sort == UseTrailingNumber )
	{
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