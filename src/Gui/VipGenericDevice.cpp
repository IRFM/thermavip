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

#include "VipGenericDevice.h"
#include "VipDisplayArea.h"
#include "VipProcessingObjectEditor.h"
#include "VipSet.h"
#include "VipStandardWidgets.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSet>

class VipGenericRecorder::PrivateData
{
public:
	PrivateData()
	  : recorder(nullptr)
	  , datePrefix("dd.MM.yyyy_hh.mm.ss.zzz_")
	  , hasDatePrefix(false)
	  , recorderAvailableDataOnOpen(true)
	  , stopStreamingOnClose(false)
	  , recordedSize(0)
	{
	}
	VipIODevice* recorder;
	QString datePrefix;
	bool hasDatePrefix;
	bool recorderAvailableDataOnOpen;
	bool stopStreamingOnClose;
	qint64 recordedSize;
	QVariantList probeInputs;
};

VipGenericRecorder::VipGenericRecorder(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	this->setEnabled(false);
	this->setScheduleStrategy(AcceptEmptyInput);
	this->topLevelInputAt(0)->toMultiInput()->setListType(VipDataList::FIFO, VipDataList::None);
}

VipGenericRecorder::~VipGenericRecorder()
{
	close();
	if (d_data->recorder)
		delete d_data->recorder;
}

qint64 VipGenericRecorder::estimateFileSize() const
{
	return d_data->recorder ? d_data->recorder->estimateFileSize() : -1;
}

bool VipGenericRecorder::probe(const QString& filename, const QByteArray&) const
{
	return fileFilters().contains(QFileInfo(filename).suffix(), Qt::CaseInsensitive);
}

void VipGenericRecorder::setProbeInputs(const QVariantList& lst)
{
	d_data->probeInputs = lst;
}

bool VipGenericRecorder::setPath(const QString& path)
{
	VipIODevice::setPath(path);
	if (!d_data->recorder || !d_data->recorder->probe(path)) {
		if (d_data->recorder) {
			delete d_data->recorder;
			d_data->recorder = nullptr;
		}

		// fill the list of input data
		QVariantList lst;
		for (int i = 0; i < inputCount(); ++i) {
			VipAnyData any = inputAt(i)->data();
			if (!any.isEmpty())
				lst.append(any.data());
		}

		if (lst.size() != inputCount() && d_data->probeInputs.size() == inputCount())
			lst = d_data->probeInputs;

		// no input data, keep searching!
		// for (int i = 0; i < inputCount(); ++i)
		//  {
		//  if (VipOutput * out = inputAt(i)->connection()->source()) {
		//  VipAnyData any = out->data();
		//  if (!any.isEmpty())
		//	lst.append(any.data());
		//  }
		//  }

		if (lst.size() != inputCount())
			lst.clear();

		d_data->recorder = VipCreateDevice::create(VipIODevice::possibleWriteDevices(path, lst));
		if (!d_data->recorder)
			return false;

		//		 d_data->recorder->setMultiSave(this->multiSave());
		d_data->recorder->setScheduleStrategy(AcceptEmptyInput);

		int input_count = inputCount();
		if (input_count == 0) {
			setError("Inputs number should greater than 0", VipProcessingObject::WrongInputNumber);
			return false;
		}

		if (VipMultiInput* inputs = d_data->recorder->topLevelInputAt(0)->toMultiInput())
			inputs->resize(input_count);
		else if (input_count != d_data->recorder->inputCount()) {
			setError("input count mismatch", VipProcessingObject::WrongInputNumber);
			return false;
		}
	}

	return d_data->recorder->setPath(path);
}

void VipGenericRecorder::setRecorderAvailableDataOnOpen(bool enable)
{
	d_data->recorderAvailableDataOnOpen = enable;
}

bool VipGenericRecorder::open(VipIODevice::OpenModes mode)
{
	if (!d_data->recorder)
		return false;

	close();
	this->setRecordedSize(0);

	if (inputCount() != d_data->recorder->inputCount())
		return false;

	d_data->recorder->setPath(generateFilename());

	if (mode != WriteOnly)
		return false;

	if (d_data->recorder->open(mode)) {
		if (d_data->recorderAvailableDataOnOpen) {
			// add already available data and apply
			for (int i = 0; i < inputCount(); ++i) {
				if (VipOutput* out = this->inputAt(i)->connection()->source()) {
					VipAnyData any = out->data();
					if (!any.isEmpty())
						d_data->recorder->inputAt(i)->setData(any);
				}
			}
			d_data->recorder->update();
		}

		// remove all buffered input data
		for (int i = 0; i < inputCount(); ++i) {
			while (inputAt(i)->hasNewData())
				inputAt(i)->data();
		}

		setOpenMode(mode);
		this->setEnabled(true);
		return true;
	}

	return false;
}

void VipGenericRecorder::openDeviceIfNotOpened()
{
	if (openMode() == NotOpen)
		this->open(WriteOnly);
}

void VipGenericRecorder::setRecordedSize(qint64 bytes)
{
	d_data->recordedSize = bytes;
}

qint64 VipGenericRecorder::recordedSize() const
{
	return d_data->recordedSize;
}

void VipGenericRecorder::setOpened(bool open)
{
	if (open)
		this->open(WriteOnly);
	else
		this->close();
}

QString VipGenericRecorder::fileFilters() const
{
	QVariantList data;
	// retrieve the list of data that will be saved

	// retrieve the list of input data
	for (int i = 0; i < inputCount(); ++i) {
		VipAnyData any = inputAt(i)->data();
		if (!any.isEmpty())
			data.append(any.data());
	}

	// find the devices that can save these data
	QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(QString(), data);
	QStringList res;
	for (int i = 0; i < devices.size(); ++i) {
		VipIODevice* dev = qobject_cast<VipIODevice*>(devices[i].create()); // vipCreateVariant(devices[i]).value<VipIODevice*>();
		QString filters = dev->fileFilters();
		if (!filters.isEmpty())
			res.append(filters);
		delete dev;
	}

	// make unique, sort and return
	res = vipToSet(res).values();
	return res.join(";;");
}

void VipGenericRecorder::setRecorder(VipIODevice* device)
{
	// close the previous recorder
	close();
	if (d_data->recorder)
		delete d_data->recorder;

	d_data->recorder = device;
	if (device) {
		if (device->inputCount())
			this->topLevelInputAt(0)->toMultiInput()->resize(device->inputCount());
		else if (this->inputCount() && device->topLevelInputAt(0)->toMultiInput())
			device->topLevelInputAt(0)->toMultiInput()->resize(inputCount());
		else {
			setError("Wrong device input count", VipProcessingObject::WrongInputNumber);
		}
	}
}

VipIODevice* VipGenericRecorder::recorder() const
{
	return d_data->recorder;
}

void VipGenericRecorder::setStopStreamingOnClose(bool enable)
{
	d_data->stopStreamingOnClose = enable;
}
bool VipGenericRecorder::stopStreamingOnClose() const
{
	return d_data->stopStreamingOnClose;
}

void VipGenericRecorder::close()
{
	this->setEnabled(false);

	bool stop_streaming = stopStreamingOnClose();
	bool is_streaming = false;
	VipIODevice* pool = nullptr;

	if (stop_streaming) {
		// detect if we are streaming
		if (pool = parentObjectPool()) {
			is_streaming = pool->isStreamingEnabled();
		}
		else {
			// find sources
			QList<VipIODevice*> dev = vipListCast<VipIODevice*>(this->allSources());
			// find the pool
			for (VipIODevice* d : dev) {
				if (pool = d->parentObjectPool())
					break;
			}
			if (pool)
				is_streaming = pool->isStreamingEnabled();
		}
	}

	if (openMode() != NotOpen) {

		if (stop_streaming && is_streaming)
			pool->setStreamingEnabled(false);

		wait();

		if (stop_streaming && is_streaming)
			pool->setStreamingEnabled(true);

		if (d_data->recorder)
			d_data->recorder->close();
		setOpenMode(VipIODevice::NotOpen);
		setSize(0);
	}
}

void VipGenericRecorder::setDatePrefix(const QString& date_prefix)
{
	d_data->datePrefix = date_prefix;
	emitProcessingChanged();
}

void VipGenericRecorder::setHasDatePrefix(bool enable)
{
	d_data->hasDatePrefix = enable;
	emitProcessingChanged();
}

QString VipGenericRecorder::datePrefix() const
{
	return d_data->datePrefix;
}

bool VipGenericRecorder::hasDatePrefix() const
{
	return d_data->hasDatePrefix;
}

QString VipGenericRecorder::generateFilename() const
{
	if (!hasDatePrefix())
		return this->path();

	QString path;

	QFileInfo info(this->path().replace("\\", "/"));
	QString fileName = info.fileName();

	// remove the date prefix if possible
	QString date_prefix = datePrefix();
	if (date_prefix.size() <= fileName.size()) {
		QDateTime time = QDateTime::fromString(fileName.mid(0, date_prefix.size()), date_prefix);
		if (time.isValid()) {
			// a date was found: extract the filename without the date
			fileName = fileName.mid(date_prefix.size());
		}
	}

	// get the canonical path
	QString canonical_path = this->path().remove("/" + info.fileName());

	QString prefix = QDateTime::currentDateTime().toString(datePrefix());
	path = canonical_path + "/" + prefix + fileName;
	return path;
}

void VipGenericRecorder::resetRecorderParameters()
{
	if (d_data->recorder && !isOpen()) {
		VipFunctionDispatcher<1>::function_list_type lst = vipFDObjectEditor().exactMatch(d_data->recorder);
		if (lst.size()) {
			QWidget* editor = lst.first()(d_data->recorder).value<QWidget*>();
			if (editor) {
				VipGenericDialog dialog(editor, "Device options", vipGetMainWindow());
				if (dialog.exec() != QDialog::Accepted) {
				}
				else {
					// try to find the "apply" slot and call it
					if (editor->metaObject()->indexOfMethod("apply()") >= 0)
						QMetaObject::invokeMethod(editor, "apply");
				}
			}
		}
	}
}

void VipGenericRecorder::apply()
{
	if (!isOpen() || !d_data->recorder)
		return;

	bool has_new_data = true;

	while (has_new_data) {
		has_new_data = false;
		qint64 bytes = 0;
		for (int i = 0; i < inputCount(); ++i) {
			if (inputAt(i)->hasNewData()) {
				VipAnyData any = inputAt(i)->data();
				d_data->recorder->inputAt(i)->setData(any);
				bytes += any.memoryFootprint();
				has_new_data = true;
			}
		}

		if (has_new_data)
			d_data->recorder->update();

		this->setSize(d_data->recorder->size());
		this->setRecordedSize(recordedSize() + bytes);
	}
}

#include <QBoxLayout>
#include <QCheckBox>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>
#include <QToolButton>

class VipRecordWidget::PrivateData
{
public:
	VipFileName filename;
	QCheckBox addDate;
	QLineEdit date;
	QToolButton record;
	QToolButton suspend;
	QToolButton resetParameters;
	QLabel info;
	QPointer<VipGenericRecorder> recorder;
	QTimer timer;
	VipRecordWidget::RecordInfos recordInfos;
	qint64 startTime;
	double previousBytes;
};

VipRecordWidget::VipRecordWidget(InfosLocation loc, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->previousBytes = 0;
	d_data->recordInfos = FramesAndInputSize;
	d_data->startTime = 0;

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(&d_data->addDate);
	hlay->addWidget(&d_data->date);

	QHBoxLayout* hlay2 = new QHBoxLayout();
	hlay2->addWidget(&d_data->filename);
	hlay2->addWidget(&d_data->record);
	hlay2->addWidget(&d_data->suspend);
	hlay2->addWidget(&d_data->resetParameters);
	hlay2->setSpacing(2);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addLayout(hlay2);
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay2->setContentsMargins(0, 0, 0, 0);
	vlay->setContentsMargins(0, 0, 0, 0);

	if (loc == Bottom) {
		vlay->addWidget(&d_data->info);
		setLayout(vlay);
	}
	else {
		QHBoxLayout* l = new QHBoxLayout();
		l->setContentsMargins(0, 0, 0, 0);
		l->addLayout(vlay);
		l->addWidget(&d_data->info);
		setLayout(l);
	}

	d_data->addDate.setText("Add date prefix");
	d_data->addDate.setToolTip("If checked, add the recording date to the output file name");
	d_data->addDate.setChecked(false);
	d_data->date.setToolTip("Date format");
	d_data->date.setText("yyyy.MM.dd_hh.mm.ss.zzz_");
	d_data->date.hide();
	d_data->record.setToolTip("Start/Stop recording");
	d_data->record.setIcon(vipIcon("record.png"));
	d_data->record.setCheckable(true);
	d_data->record.setAutoRaise(true);
	d_data->suspend.setToolTip("Suspend/resume recording");
	d_data->suspend.setIcon(vipIcon("pause.png"));
	d_data->suspend.setAutoRaise(true);
	d_data->suspend.setCheckable(true);
	d_data->suspend.hide();
	d_data->resetParameters.setToolTip("Reset/Modify the recording parameters");
	d_data->resetParameters.setIcon(vipIcon("reset.png"));
	d_data->resetParameters.setAutoRaise(true);
	d_data->resetParameters.setVisible(false);
	d_data->filename.setMode(VipFileName::Save);
	d_data->filename.setFilters(VipGenericRecorder().fileFilters());
	d_data->filename.setTitle("Record in file...");
	d_data->filename.edit()->setPlaceholderText("Output file");

	d_data->timer.setSingleShot(false);
	d_data->timer.setInterval(200);

	connect(&d_data->addDate, SIGNAL(clicked(bool)), this, SLOT(updateDeviceFromWidget()));
	connect(&d_data->addDate, SIGNAL(clicked(bool)), &d_data->date, SLOT(setVisible(bool)));
	connect(&d_data->record, SIGNAL(clicked(bool)), this, SLOT(setRecording(bool)));
	connect(&d_data->suspend, SIGNAL(clicked(bool)), this, SLOT(suspend(bool)));
	connect(&d_data->resetParameters, SIGNAL(clicked(bool)), this, SLOT(resetParameters()));
	connect(&d_data->filename, SIGNAL(changed(const QString&)), this, SLOT(updateDeviceFromWidget()));
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(updateRecordInfo()), Qt::QueuedConnection);
}

VipRecordWidget::~VipRecordWidget()
{
	d_data->timer.stop();
}

void VipRecordWidget::setRecordInfos(RecordInfos infos)
{
	d_data->recordInfos = infos;
}
VipRecordWidget::RecordInfos VipRecordWidget::recordInfos() const
{
	return d_data->recordInfos;
}

void VipRecordWidget::setDateOptionsVisible(bool vis)
{
	if (vis)
		d_data->date.setVisible(d_data->addDate.isChecked());
	else
		d_data->date.setVisible(false);
	d_data->addDate.setVisible(vis);
}
bool VipRecordWidget::dateOptionsVisible() const
{
	return !d_data->addDate.isHidden();
}

void VipRecordWidget::setDatePrefixEnabled(bool enable)
{
	d_data->addDate.blockSignals(true);
	d_data->addDate.setChecked(enable);
	d_data->addDate.blockSignals(false);
	if (dateOptionsVisible())
		d_data->date.setVisible(enable);
}
bool VipRecordWidget::datePrefixEnabled() const
{
	return d_data->addDate.isChecked();
}
void VipRecordWidget::setDatePrefix(const QString& prefix)
{
	d_data->date.setText(prefix);
}
QString VipRecordWidget::datePrefix() const
{
	return d_data->date.text();
}

void VipRecordWidget::resetParameters()
{
	if (d_data->recorder)
		d_data->recorder->resetRecorderParameters();
}

void VipRecordWidget::updateRecordInfo()
{
	if (VipGenericRecorder* recorder = d_data->recorder) {
		qint64 count = recorder->size();
		if (count != VipInvalidPosition) {
			if (d_data->recordInfos == FramesAndInputSize) {
				double bytes = recorder->recordedSize() / 1000;
				double bytes_since_timeout = bytes - d_data->previousBytes;
				d_data->previousBytes = bytes;
				double rate = (bytes_since_timeout / d_data->timer.interval()) * 1000.;
				QString rate_unit = " KB/s";
				QString bytes_unit = " KB";
				if (rate > 1000) {
					rate /= 1000;
					rate_unit = " MB/s";
				}
				if (bytes > 1000) {
					bytes /= 1000;
					bytes_unit = " MB";
				}

				qint64 out_bytes = 0;
				QString out_bytes_unit = " KB";
				if (VipIODevice* io = recorder->recorder())
					if (QIODevice* dev = io->device()) {
						out_bytes = dev->size() / 1000.0;
						if (out_bytes > 1000) {
							out_bytes /= 1000;
							out_bytes_unit = " MB";
						}
					}

				QString text = "<b>" + QString::number(count) + "</b> frames, recorder data = <b>" + QString::number(bytes) + "</b>" + bytes_unit + "<br>Rate = <b>" +
					       QString::number(rate) + "</b>" + rate_unit;

				if (out_bytes != 0)
					text += "<br>File size = <b>" + QString::number(out_bytes) + "</b>" + out_bytes_unit;

				d_data->info.setText(text);
			}
			else {
				qint64 size = d_data->recorder->estimateFileSize();
				qint64 duration_milli = QDateTime::currentMSecsSinceEpoch() - d_data->startTime;
				QTime time(0, 0);
				time = time.addSecs(duration_milli / 1000);
				QString res = time.toString("hh:mm:ss");

				if (size < 1000000000LL)
					res += ", " + QString::number(size / 1000000) + "MB";
				else
					res += ", " + QString::number(size / 1000000000.0) + "GB";
				d_data->info.setText(res);
			}
		}
		else
			d_data->info.setText(QString());
	}
}

void VipRecordWidget::updateFileFilters(const QString& filters)
{
	d_data->filename.setFilters(filters);
}
void VipRecordWidget::updateFileFilters()
{
	updateFileFilters(QVariantList());
}

void VipRecordWidget::setFilename(const QString& filename)
{
	d_data->filename.edit()->setText(filename);
}

void VipRecordWidget::enableRecording(bool record)
{
	if (record)
		startRecording();
	else
		stopRecording();
}

QString VipRecordWidget::updateFileFilters(const QVariantList& data, VipFileName* filename)
{
	QString filters;
	if (data.size()) {
		// find the devices that can save these data
		QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(QString(), data);
		QStringList res;
		for (int i = 0; i < devices.size(); ++i) {
			VipIODevice* dev = qobject_cast<VipIODevice*>(devices[i].create()); // vipCreateVariant(devices[i]).value<VipIODevice*>();
			QString _filters = dev->fileFilters();
			if (!_filters.isEmpty())
				res.append(_filters);
			delete dev;
		}

		// make unique, sort and return
		res = vipToSet(res).values();
		filters = res.join(";;");
	}

	if (filename)
		filename->setFilters(filters);
	return filters;
}

QString VipRecordWidget::updateFileFilters(const QVariantList& lst)
{
	if (lst.size())
		return updateFileFilters(lst, &d_data->filename);
	else if (d_data->recorder) {
		QString filters = d_data->recorder->fileFilters();
		d_data->filename.setFilters(filters);
		return filters;
	}
	return QString();
}

bool VipRecordWidget::canDisplayRecorderParametersEditor() const
{
	if (d_data->recorder && d_data->recorder->recorder()) {
		if (!d_data->recorder->isOpen()) {
			VipFunctionDispatcher<1>::function_list_type lst = vipFDObjectEditor().exactMatch(d_data->recorder->recorder());
			return lst.size() > 0;
		}
	}
	return false;
}

void VipRecordWidget::setGenericRecorder(VipGenericRecorder* recorder)
{
	if (d_data->recorder) {
		disconnect(d_data->recorder, SIGNAL(opened()), this, SLOT(updateWidgetFromDevice()));
		disconnect(d_data->recorder, SIGNAL(closed()), this, SLOT(updateWidgetFromDevice()));
		disconnect(d_data->recorder, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(updateWidgetFromDevice()));
		d_data->recorder->close();
	}

	d_data->recorder = recorder;
	if (recorder) {
		connect(d_data->recorder, SIGNAL(opened()), this, SLOT(updateWidgetFromDevice()), Qt::QueuedConnection);
		connect(d_data->recorder, SIGNAL(closed()), this, SLOT(updateWidgetFromDevice()), Qt::QueuedConnection);
		connect(d_data->recorder, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(updateWidgetFromDevice()), Qt::QueuedConnection);
		updateWidgetFromDevice();
	}

	d_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());
}

VipGenericRecorder* VipRecordWidget::genericRecorder() const
{
	return d_data->recorder;
}

QToolButton* VipRecordWidget::record() const
{
	return const_cast<QToolButton*>(&d_data->record);
}

VipFileName* VipRecordWidget::filenameWidget() const
{
	return &d_data->filename;
}

QString VipRecordWidget::path() const
{
	if (VipGenericRecorder* recorder = d_data->recorder)
		return recorder->path();
	return QString();
}

QString VipRecordWidget::filename() const
{
	return d_data->filename.filename();
}

void VipRecordWidget::startRecording()
{
	d_data->record.blockSignals(true);
	d_data->record.setChecked(true);
	d_data->record.blockSignals(false);
	d_data->suspend.show();

	if (!d_data->recorder)
		return;

	if (VipGenericRecorder* recorder = d_data->recorder) {
		recorder->setHasDatePrefix(d_data->addDate.isChecked());
		recorder->setDatePrefix(d_data->date.text());
		recorder->setPath(d_data->filename.filename());
		recorder->open(VipIODevice::WriteOnly);
		d_data->previousBytes = 0;
		d_data->startTime = QDateTime::currentMSecsSinceEpoch();
	}
}

void VipRecordWidget::stopRecording()
{
	d_data->timer.stop();
	d_data->record.blockSignals(true);
	d_data->record.setChecked(false);
	d_data->record.blockSignals(false);
	d_data->suspend.hide();

	if (VipGenericRecorder* recorder = d_data->recorder)
		recorder->close();

	d_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());
}

void VipRecordWidget::setRecording(bool record)
{
	if (record && d_data->record.isChecked()) {
		if (filename().isEmpty()) {
			d_data->record.blockSignals(true);
			d_data->record.setChecked(false);
			d_data->record.blockSignals(false);
			return;
		}
		startRecording();
	}
	else if (!record && !d_data->record.isChecked())
		stopRecording();
}

void VipRecordWidget::suspend(bool enable)
{
	d_data->suspend.blockSignals(true);
	if (enable)
		d_data->suspend.setIcon(vipIcon("play.png"));
	else
		d_data->suspend.setIcon(vipIcon("pause.png"));
	d_data->suspend.blockSignals(false);

	if (d_data->recorder)
		d_data->recorder->setEnabled(!enable);
}

void VipRecordWidget::updateDeviceFromWidget()
{
	d_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());
}

void VipRecordWidget::updateWidgetFromDevice()
{
	if (!d_data->recorder)
		return;

	d_data->date.blockSignals(true);
	d_data->addDate.blockSignals(true);
	d_data->record.blockSignals(true);
	d_data->filename.edit()->blockSignals(true);

	d_data->date.setText(d_data->recorder->datePrefix());
	if (dateOptionsVisible())
		d_data->date.setVisible(d_data->recorder->hasDatePrefix());
	d_data->addDate.setChecked(d_data->recorder->hasDatePrefix());
	d_data->record.setChecked(d_data->recorder->isOpen());
	if (d_data->recorder->recorder())
		d_data->filename.edit()->setText(d_data->recorder->recorder()->path());
	else
		d_data->filename.edit()->setText(d_data->recorder->path());
	d_data->filename.edit()->setEnabled(!d_data->recorder->isOpen());

	d_data->filename.edit()->blockSignals(false);
	d_data->record.blockSignals(false);
	d_data->date.blockSignals(false);
	d_data->addDate.blockSignals(false);

	if (d_data->recorder->isOpen()) {
		if (!d_data->timer.isActive())
			d_data->timer.start();
	}
	else {
		d_data->timer.stop();
	}

	d_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());

	Q_EMIT recordingChanged(d_data->recorder->isOpen());
}
