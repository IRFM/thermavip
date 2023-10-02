#include "VipGenericDevice.h"
#include "VipProcessingObjectEditor.h"
#include "VipStandardWidgets.h"
#include "VipDisplayArea.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSet>


class VipGenericRecorder::PrivateData
{
public:
	PrivateData() : recorder(NULL), datePrefix("dd.MM.yyyy_hh.mm.ss.zzz_"), hasDatePrefix(false), recorderAvailableDataOnOpen(true), recordedSize(0){}
	VipIODevice * recorder;
	QString datePrefix;
	bool hasDatePrefix;
	bool recorderAvailableDataOnOpen;
	qint64 recordedSize;
	QVariantList probeInputs;
};

VipGenericRecorder::VipGenericRecorder(QObject * parent )
:VipIODevice(parent)
{
	m_data = new PrivateData();
	this->setEnabled(false);
	this->setScheduleStrategy(AcceptEmptyInput);
	this->topLevelInputAt(0)->toMultiInput()->setListType(VipDataList::FIFO, VipDataList::None);
}

VipGenericRecorder::~VipGenericRecorder()
{
	close();
	if(m_data->recorder)
		delete m_data->recorder;
	delete m_data;
}

qint64 VipGenericRecorder::estimateFileSize() const
{
	return m_data->recorder ? m_data->recorder->estimateFileSize() : -1;
}

 bool VipGenericRecorder::probe(const QString &filename, const QByteArray & ) const
 {
	 return fileFilters().contains(QFileInfo(filename).suffix(), Qt::CaseInsensitive);
 }

 void VipGenericRecorder::setProbeInputs(const QVariantList & lst)
 {
	 m_data->probeInputs = lst;
 }

 bool VipGenericRecorder::setPath(const QString & path)
 {
	 VipIODevice::setPath(path);
	 if(!m_data->recorder || !m_data->recorder->probe(path))
	 {
		 if(m_data->recorder)
		 {
			 delete m_data->recorder;
			 m_data->recorder = NULL;
		 }

		 //fill the list of input data
		 QVariantList lst;
		for(int i=0; i < inputCount(); ++i)
		{
			VipAnyData any = inputAt(i)->data();
			 if(!any.isEmpty())
				 lst.append(any.data());
		}

		if (lst.size() != inputCount() && m_data->probeInputs.size() == inputCount())
			lst = m_data->probeInputs;

		//no input data, keep searching!
		//for (int i = 0; i < inputCount(); ++i)
		// {
		// if (VipOutput * out = inputAt(i)->connection()->source()) {
		// VipAnyData any = out->data();
		// if (!any.isEmpty())
		//	lst.append(any.data());
		// }
		// }

		if (lst.size() != inputCount())
			lst.clear();

		 m_data->recorder  = VipCreateDevice::create( VipIODevice::possibleWriteDevices(path,lst));
		 if(!m_data->recorder)
			 return false;

//		 m_data->recorder->setMultiSave(this->multiSave());
		 m_data->recorder->setScheduleStrategy(AcceptEmptyInput);

		 int input_count = inputCount();
		 if(input_count == 0)
		 {
			 setError("Inputs number should greater than 0", VipProcessingObject::WrongInputNumber);
			 return false;
		 }

		if(VipMultiInput * inputs = m_data->recorder->topLevelInputAt(0)->toMultiInput())
			inputs->resize(input_count);
		else if(input_count != m_data->recorder->inputCount())
		{
			setError("input count mismatch", VipProcessingObject::WrongInputNumber);
			return false;
		}
	 }

	 return m_data->recorder->setPath(path);
 }

 void VipGenericRecorder::setRecorderAvailableDataOnOpen(bool enable)
 {
	 m_data->recorderAvailableDataOnOpen = enable;
 }

 bool VipGenericRecorder::open(VipIODevice::OpenModes mode)
 {
	 if (!m_data->recorder)
		 return false;


	 close();
	 this->setRecordedSize(0);

	 if (inputCount() != m_data->recorder->inputCount())
		 return false;


	 m_data->recorder->setPath(generateFilename());


	 if(mode != WriteOnly)
		 return false;

	 if( m_data->recorder->open(mode) )
	 {
		 if (m_data->recorderAvailableDataOnOpen)
		 {
			 //add already available data and apply
			 for (int i = 0; i < inputCount(); ++i)
			 {
				 if (VipOutput * out = this->inputAt(i)->connection()->source())
				 {
					 VipAnyData any = out->data();
					 if (!any.isEmpty())
						 m_data->recorder->inputAt(i)->setData(any);
				 }
			 }
			 m_data->recorder->update();
		 }

		//remove all buffered input data
		for (int i = 0; i < inputCount(); ++i)
		{
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
	 if(openMode() == NotOpen)
		 this->open(WriteOnly);
 }

 void VipGenericRecorder::setRecordedSize(qint64 bytes)
 {
	 m_data->recordedSize = bytes;
 }

 qint64 VipGenericRecorder::recordedSize() const
 {
	 return m_data->recordedSize;
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
 	//retrieve the list of data that will be saved

	//retrieve the list of input data
	for(int i=0; i < inputCount(); ++i)
	{
		VipAnyData any = inputAt(i)->data();
		if(!any.isEmpty())
			data.append(any.data());
	}

 	// find the devices that can save these data
 	QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(QString(),data);
 	QStringList res;
 	for(int i=0; i < devices.size(); ++i)
 	{
 		VipIODevice * dev = qobject_cast<VipIODevice*>(devices[i].create());//vipCreateVariant(devices[i]).value<VipIODevice*>();
 		QString filters = dev->fileFilters();
 		if(!filters.isEmpty())
 			res.append(filters);
 		delete dev;
 	}

 	//make unique, sort and return
 	res = res.toSet().toList();
 	return res.join(";;");
 }

 void VipGenericRecorder::setRecorder(VipIODevice * device)
 {
	 //close the previous recorder
	 close();
	 if (m_data->recorder)
		 delete m_data->recorder;

	 m_data->recorder = device;
	 if (device)
	 {
		 if (device->inputCount())
			 this->topLevelInputAt(0)->toMultiInput()->resize(device->inputCount());
		 else if (this->inputCount() && device->topLevelInputAt(0)->toMultiInput())
			 device->topLevelInputAt(0)->toMultiInput()->resize(inputCount());
		 else
		 {
			 setError("Wrong device input count", VipProcessingObject::WrongInputNumber);
		 }
	 }
 }

 VipIODevice * VipGenericRecorder::recorder() const
 {
	 return m_data->recorder;
 }

 void VipGenericRecorder::close()
 {
	 this->setEnabled(false);
	 if (openMode() != NotOpen)
	 {
		 wait();
		 if (m_data->recorder)
			 m_data->recorder->close();
		 setOpenMode(VipIODevice::NotOpen);
		 setSize(0);
	 }
 }

 void VipGenericRecorder::setDatePrefix(const QString & date_prefix)
 {
	 m_data->datePrefix = date_prefix;
	 emitProcessingChanged();
 }

 void VipGenericRecorder::setHasDatePrefix(bool enable)
 {
	 m_data->hasDatePrefix = enable;
	 emitProcessingChanged();
 }

 QString VipGenericRecorder::datePrefix() const
 {
	 return m_data->datePrefix;
 }

 bool VipGenericRecorder::hasDatePrefix() const
 {
	 return m_data->hasDatePrefix;
 }

 QString VipGenericRecorder::generateFilename() const
 {
	 if (!hasDatePrefix())
		 return this->path();

	 QString path;

	 QFileInfo info(this->path().replace("\\", "/"));
	 QString fileName = info.fileName();

	 //remove the date prefix if possible
	 QString date_prefix = datePrefix();
	 if (date_prefix.size() <= fileName.size())
	 {
		 QDateTime time = QDateTime::fromString(fileName.mid(0, date_prefix.size()), date_prefix);
		 if (time.isValid())
		 {
			 //a date was found: extract the filename without the date
			 fileName = fileName.mid(date_prefix.size());
		 }
	 }

	 //get the canonical path
	 QString canonical_path = this->path().remove("/" + info.fileName());

	QString prefix = QDateTime::currentDateTime().toString(datePrefix());
	path = canonical_path + "/" + prefix + fileName;
	return path;
}

 void VipGenericRecorder::resetRecorderParameters()
 {
	 if (m_data->recorder && !isOpen() )
	 {
		 VipFunctionDispatcher<1>::function_list_type lst = vipFDObjectEditor().exactMatch(m_data->recorder);
		 if (lst.size())
		 {
			 QWidget * editor = lst.first()(m_data->recorder).value<QWidget*>();
			 if (editor)
			 {
				 VipGenericDialog dialog(editor, "Device options", vipGetMainWindow());
				 if (dialog.exec() != QDialog::Accepted)
				 {
				 }
				 else
				 {
					 //try to find the "apply" slot and call it
					 if (editor->metaObject()->indexOfMethod("apply()") >= 0)
						 QMetaObject::invokeMethod(editor, "apply");
				 }
			 }
		 }
	 }
 }

 void VipGenericRecorder::apply()
 {
	 if(!isOpen() || !m_data->recorder)
		 return;


	 bool has_new_data = true;

	 while(has_new_data)
	 {
		 has_new_data = false;
		 qint64 bytes = 0;
		 for(int i=0; i < inputCount(); ++i)
		 {
			if(inputAt(i)->hasNewData())
			{
				 VipAnyData any = inputAt(i)->data();
				 m_data->recorder->inputAt(i)->setData(any);
				 bytes += any.memoryFootprint();
				 has_new_data = true;
			}
		 }

		 if(has_new_data)
			 m_data->recorder->update();

		 this->setSize(m_data->recorder->size());
		 this->setRecordedSize(recordedSize() + bytes);
	 }
 }




#include <QCheckBox>
#include <QLineEdit>
#include <QToolButton>
#include <QPointer>
#include <QBoxLayout>
#include <QTimer>

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

 VipRecordWidget::VipRecordWidget(InfosLocation loc , QWidget * parent )
 :QWidget(parent)
 {
	 m_data = new PrivateData();
	 m_data->previousBytes = 0;
	 m_data->recordInfos = FramesAndInputSize;
	 m_data->startTime = 0;

	 QHBoxLayout * hlay = new QHBoxLayout();
	 hlay->addWidget(&m_data->addDate);
	 hlay->addWidget(&m_data->date);

	 QHBoxLayout * hlay2 = new QHBoxLayout();
	 hlay2->addWidget(&m_data->filename);
	 hlay2->addWidget(&m_data->record);
	 hlay2->addWidget(&m_data->suspend);
	 hlay2->addWidget(&m_data->resetParameters);
	 hlay2->setSpacing(2);

	 QVBoxLayout * vlay = new QVBoxLayout();
	 vlay->addLayout(hlay);
	 vlay->addLayout(hlay2);
	 hlay->setContentsMargins(0, 0, 0, 0);
	 hlay2->setContentsMargins(0, 0, 0, 0);
	 vlay->setContentsMargins(0, 0, 0, 0);

	 if (loc == Bottom) {
		 vlay->addWidget(&m_data->info);
		 setLayout(vlay);
	 }
	 else {
		 QHBoxLayout * l = new QHBoxLayout();
		 l->setContentsMargins(0, 0, 0, 0);
		 l->addLayout(vlay);
		 l->addWidget(&m_data->info);
		 setLayout(l);
	 }

	 m_data->addDate.setText("Add date prefix");
	 m_data->addDate.setToolTip("If checked, add the recording date to the output file name");
	 m_data->addDate.setChecked(false);
	 m_data->date.setToolTip("Date format");
	 m_data->date.setText("yyyy.MM.dd_hh.mm.ss.zzz_");
	 m_data->date.hide();
	 m_data->record.setToolTip("Start/Stop recording");
	 m_data->record.setIcon(vipIcon("record.png"));
	 m_data->record.setCheckable(true);
	 m_data->record.setAutoRaise(true);
	 m_data->suspend.setToolTip("Suspend/resume recording");
	 m_data->suspend.setIcon(vipIcon("pause.png"));
	 m_data->suspend.setAutoRaise(true);
	 m_data->suspend.setCheckable(true);
	 m_data->suspend.hide();
	 m_data->resetParameters.setToolTip("Reset/Modify the recording parameters");
	 m_data->resetParameters.setIcon(vipIcon("reset.png"));
	 m_data->resetParameters.setAutoRaise(true);
	 m_data->resetParameters.setVisible(false);
	 m_data->filename.setMode(VipFileName::Save);
	 m_data->filename.setFilters(VipGenericRecorder().fileFilters());
	 m_data->filename.setTitle("Record in file...");
	 m_data->filename.edit()->setPlaceholderText("Output file");

	 m_data->timer.setSingleShot(false);
	 m_data->timer.setInterval(200);

	 connect(&m_data->addDate,SIGNAL(clicked(bool)),this,SLOT(updateDeviceFromWidget()));
	 connect(&m_data->addDate, SIGNAL(clicked(bool)), &m_data->date, SLOT(setVisible(bool)));
	 connect(&m_data->record,SIGNAL(clicked(bool)),this,SLOT(setRecording(bool)));
	 connect(&m_data->suspend, SIGNAL(clicked(bool)), this, SLOT(suspend(bool)));
	 connect(&m_data->resetParameters, SIGNAL(clicked(bool)), this, SLOT(resetParameters()));
	 connect(&m_data->filename,SIGNAL(changed(const QString &)),this,SLOT(updateDeviceFromWidget()));
	 connect(&m_data->timer,SIGNAL(timeout()),this,SLOT(updateRecordInfo()),Qt::QueuedConnection);
 }

 VipRecordWidget::~VipRecordWidget()
 {
	 m_data->timer.stop();
	 delete m_data;
 }


 void VipRecordWidget::setRecordInfos(RecordInfos infos)
 {
	 m_data->recordInfos = infos;
 }
 VipRecordWidget::RecordInfos VipRecordWidget::recordInfos() const
 {
	 return m_data->recordInfos;
 }

 void VipRecordWidget::setDateOptionsVisible(bool vis)
 {
	 if (vis)
		 m_data->date.setVisible(m_data->addDate.isChecked());
	 else
		 m_data->date.setVisible(false);
	 m_data->addDate.setVisible(vis);
 }
 bool VipRecordWidget::dateOptionsVisible() const
 {
	 return !m_data->addDate.isHidden();
 }

 void VipRecordWidget::setDatePrefixEnabled(bool enable)
 {
	 m_data->addDate.blockSignals(true);
	 m_data->addDate.setChecked(enable);
	 m_data->addDate.blockSignals(false);
	 if (dateOptionsVisible())
		 m_data->date.setVisible(enable);
 }
 bool VipRecordWidget::datePrefixEnabled() const
 {
	 return m_data->addDate.isChecked();
 }
 void VipRecordWidget::setDatePrefix(const QString & prefix)
 {
	 m_data->date.setText(prefix);
 }
 QString VipRecordWidget::datePrefix() const
 {
	 return m_data->date.text();
 }

 void VipRecordWidget::resetParameters()
 {
	 if (m_data->recorder)
		 m_data->recorder->resetRecorderParameters();
 }

void VipRecordWidget::updateRecordInfo()
{
	if(VipGenericRecorder * recorder = m_data->recorder)
	{
		qint64 count = recorder->size();
		if(count != VipInvalidPosition)
		{
			if (m_data->recordInfos == FramesAndInputSize)
			{
				double bytes = recorder->recordedSize() / 1000;
				double bytes_since_timeout = bytes - m_data->previousBytes;
				m_data->previousBytes = bytes;
				double rate = (bytes_since_timeout / m_data->timer.interval()) * 1000.;
				QString rate_unit = " KB/s";
				QString bytes_unit = " KB";
				if (rate > 1000)
				{
					rate /= 1000;
					rate_unit = " MB/s";
				}
				if (bytes > 1000)
				{
					bytes /= 1000;
					bytes_unit = " MB";
				}

				qint64 out_bytes = 0;
				QString out_bytes_unit = " KB";
				if (VipIODevice * io = recorder->recorder())
					if (QIODevice * dev = io->device())
					{
						out_bytes = dev->size() / 1000.0;
						if (out_bytes > 1000)
						{
							out_bytes /= 1000;
							out_bytes_unit = " MB";
						}
					}

				QString text = "<b>" + QString::number(count) + "</b> frames, recorder data = <b>" + QString::number(bytes) + "</b>" + bytes_unit + "<br>Rate = <b>"
					+ QString::number(rate) + "</b>" + rate_unit;

				if (out_bytes != 0)
					text += "<br>File size = <b>" + QString::number(out_bytes) + "</b>" + out_bytes_unit;

				m_data->info.setText(text);
			}
			else
			{
				qint64 size = m_data->recorder->estimateFileSize();
				qint64 duration_milli = QDateTime::currentMSecsSinceEpoch() - m_data->startTime;
				QTime time(0,0);
				time = time.addSecs(duration_milli / 1000);
				QString res = time.toString("hh:mm:ss");

				if (size < 1000000000LL)
					res += ", " + QString::number(size / 1000000) + "MB";
				else
					res += ", " + QString::number(size / 1000000000.0) + "GB";
				m_data->info.setText(res);
			}
		}
		else
			m_data->info.setText(QString());
	}
}

void VipRecordWidget::updateFileFilters(const QString & filters)
{
	m_data->filename.setFilters(filters);
}
void VipRecordWidget::updateFileFilters()
{
	updateFileFilters(QVariantList());
}

void VipRecordWidget::setFilename(const QString & filename)
{
	m_data->filename.edit()->setText(filename);
}

void VipRecordWidget::enableRecording(bool record)
{
	if (record)
		startRecording();
	else
		stopRecording();
}

QString VipRecordWidget::updateFileFilters(const QVariantList & data, VipFileName * filename)
{
	QString filters;
	if (data.size())
	{
		// find the devices that can save these data
		QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(QString(), data);
		QStringList res;
		for (int i = 0; i < devices.size(); ++i)
		{
			VipIODevice * dev = qobject_cast<VipIODevice*>(devices[i].create());//vipCreateVariant(devices[i]).value<VipIODevice*>();
			QString filters = dev->fileFilters();
			if (!filters.isEmpty())
				res.append(filters);
			delete dev;
		}

		//make unique, sort and return
		res = res.toSet().toList();
		filters = res.join(";;");
	}

	if(filename)
		filename->setFilters(filters);
	return filters;
}

QString VipRecordWidget::updateFileFilters(const QVariantList & data)
{
	if(data.size())
		return updateFileFilters(data,&m_data->filename);
	else if (m_data->recorder)
	{
		QString filters = m_data->recorder->fileFilters();
		m_data->filename.setFilters(filters);
		return filters;
	}
	return QString();
}

bool VipRecordWidget::canDisplayRecorderParametersEditor() const
{
	if (m_data->recorder && m_data->recorder->recorder())
	{
		if (!m_data->recorder->isOpen())
		{
			VipFunctionDispatcher<1>::function_list_type lst = vipFDObjectEditor().exactMatch(m_data->recorder->recorder());
			return lst.size() > 0;
		}
	}
	return false;
}

void VipRecordWidget::setGenericRecorder(VipGenericRecorder * recorder)
{
	if(m_data->recorder)
	{
		disconnect(m_data->recorder,SIGNAL(opened()),this,SLOT(updateWidgetFromDevice()));
		disconnect(m_data->recorder,SIGNAL(closed()),this,SLOT(updateWidgetFromDevice()));
		disconnect(m_data->recorder, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(updateWidgetFromDevice()));
		m_data->recorder->close();
	}

	m_data->recorder = recorder;
	if(recorder)
	{
		connect(m_data->recorder,SIGNAL(opened()),this,SLOT(updateWidgetFromDevice()),Qt::QueuedConnection);
		connect(m_data->recorder,SIGNAL(closed()),this,SLOT(updateWidgetFromDevice()),Qt::QueuedConnection);
		connect(m_data->recorder, SIGNAL(processingChanged(VipProcessingObject*)), this, SLOT(updateWidgetFromDevice()), Qt::QueuedConnection);
		updateWidgetFromDevice();
	}

	m_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());
}

VipGenericRecorder * VipRecordWidget::genericRecorder() const
{
	return m_data->recorder;
}

QToolButton * VipRecordWidget::record() const
{
	return const_cast<QToolButton*>(&m_data->record);
}

VipFileName * VipRecordWidget::filenameWidget() const
{
	return &m_data->filename;
}

QString VipRecordWidget::path() const
{
	if (VipGenericRecorder * recorder = m_data->recorder)
		return recorder->path();
	return QString();
}

QString VipRecordWidget::filename() const
{
	return m_data->filename.filename();
}

void VipRecordWidget::startRecording()
{
	m_data->record.blockSignals(true);
	m_data->record.setChecked(true);
	m_data->record.blockSignals(false);
	m_data->suspend.show();

	if (!m_data->recorder)
		return;

	if (VipGenericRecorder * recorder = m_data->recorder)
	{
		recorder->setHasDatePrefix(m_data->addDate.isChecked());
		recorder->setDatePrefix(m_data->date.text());
		recorder->setPath(m_data->filename.filename());
		recorder->open(VipIODevice::WriteOnly);
		m_data->previousBytes = 0;
		m_data->startTime = QDateTime::currentMSecsSinceEpoch();
	}
}

void VipRecordWidget::stopRecording()
{
	m_data->timer.stop();
	m_data->record.blockSignals(true);
	m_data->record.setChecked(false);
	m_data->record.blockSignals(false);
	m_data->suspend.hide();

	if(VipGenericRecorder * recorder = m_data->recorder)
		recorder->close();

	m_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());
}

void VipRecordWidget::setRecording(bool record)
{
	if (record && m_data->record.isChecked())
	{
		if (filename().isEmpty())
		{
			m_data->record.blockSignals(true);
			m_data->record.setChecked(false);
			m_data->record.blockSignals(false);
			return;
		}
		startRecording();
	}
	else if(!record && !m_data->record.isChecked())
		stopRecording();
}

void VipRecordWidget::suspend(bool enable)
{
	m_data->suspend.blockSignals(true);
	if (enable) m_data->suspend.setIcon(vipIcon("play.png"));
	else m_data->suspend.setIcon(vipIcon("pause.png"));
	m_data->suspend.blockSignals(false);

	if (m_data->recorder)
		m_data->recorder->setEnabled(!enable);
}

void VipRecordWidget::updateDeviceFromWidget()
{
	m_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());
}

void VipRecordWidget::updateWidgetFromDevice()
{
	if (!m_data->recorder)
		return;

	m_data->date.blockSignals(true);
	m_data->addDate.blockSignals(true);
	m_data->record.blockSignals(true);
	m_data->filename.edit()->blockSignals(true);

	m_data->date.setText(m_data->recorder->datePrefix());
	if(dateOptionsVisible())
		m_data->date.setVisible(m_data->recorder->hasDatePrefix());
	m_data->addDate.setChecked(m_data->recorder->hasDatePrefix());
	m_data->record.setChecked(m_data->recorder->isOpen());
	if(m_data->recorder->recorder())
		m_data->filename.edit()->setText(m_data->recorder->recorder()->path());
	else
		m_data->filename.edit()->setText(m_data->recorder->path());
	m_data->filename.edit()->setEnabled(!m_data->recorder->isOpen());

	m_data->filename.edit()->blockSignals(false);
	m_data->record.blockSignals(false);
	m_data->date.blockSignals(false);
	m_data->addDate.blockSignals(false);

	if(m_data->recorder->isOpen())
	{
		if(!m_data->timer.isActive())
			m_data->timer.start();
	}
	else
	{
		m_data->timer.stop();
	}

	m_data->resetParameters.setVisible(canDisplayRecorderParametersEditor());

	Q_EMIT recordingChanged(m_data->recorder->isOpen());
}
