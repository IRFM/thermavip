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

#include "VipRIRDevice.h"
#include "VipLibRIR.h"

VipRIRDevice::VipRIRDevice(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
  , m_file(0)
  , m_count(0)
{
	this->outputAt(0)->setData(VipNDArray());
	this->propertyAt(0)->setData(-1);
	this->propertyName("BadPixels")->setData(false);
}

VipRIRDevice::~VipRIRDevice()
{
	VipRIRDevice::close();
}

bool VipRIRDevice::open(VipIODevice::OpenModes mode)
{
	if (!(mode & ReadOnly))
		return false;

	if (!VipLibRIR::instance())
		return false;

	QString p = removePrefix(path());
	p.replace("\\", "/");

	// Check if using a custom file system
	VipMapFileSystemPtr map_fs = this->mapFileSystem();
	if (map_fs) {
		QIODevice* dev = map_fs->open(VipPath(p), QIODevice::ReadOnly);
		if (dev) {
			if (QFile* f = qobject_cast<QFile*>(dev)) {
				p = f->fileName();
				setPath(p);
			}
			delete dev;
		}
	}

	m_file = VipLibRIR::instance()->open_camera_file(p.toLatin1().data(), nullptr);
	if (!m_file) {
		return false;
	}

	this->setAttributes(VipLibRIR::instance()->getGlobalAttributesAsString(m_file));
	if (attribute("Name").toString().isEmpty()) {
		QString pa = removePrefix(path());
		setAttribute("Name", QFileInfo(pa).fileName());
	}

	// set transform for WA camera
	QString view = attribute("view").toString();
	if (view.isEmpty())
		view = attribute("Camera").toString();
	if (view.isEmpty())
		view = attribute("Identifier").toString();
	if (view.isEmpty()) {
		// Use the 'Name' attribute to get pulse and view
		QStringList lst = attribute("Name").toString().split("_");
		if (lst.size() > 2) {
			bool ok;
			lst[0].toDouble(&ok);
			if (ok) {
				setAttribute("Pulse", lst[0].toDouble());
				setAttribute("Camera", view = lst[1]);
			}
		}
	}
	setAttribute("Camera", view);

	// WEST specific management
	QString device = attribute("Device").toString();
	if (device.isEmpty()) {
		if (view.contains("DIVQ") || view.contains("WAQ") || view.contains("LHQ") || view.contains("ICRQ") || view.contains("HRQ"))
			setAttribute("Device", "WEST");
	}

	m_count = VipLibRIR::instance()->get_image_count(m_file);
	int w, h;
	if (VipLibRIR::instance()->get_image_size(m_file, &w, &h) < 0)
		return false;
	m_size = QSize(w, h);

	QVector<qint64> times;
	times.resize(m_count);
	for (int i = 0; i < times.size(); ++i) {
		if (VipLibRIR::instance()->get_image_time(m_file, i, &times[i]) < 0)
			return false;
	}

	setTimestamps(times);

	// retrieve the calibrations
	int calib_count;
	if (VipLibRIR::instance()->supported_calibrations(m_file, &calib_count) == 0) {
		for (int i = 0; i < calib_count; ++i) {
			char name[100];
			memset(name, 0, sizeof(name));
			VipLibRIR::instance()->calibration_name(m_file, i, name);
			m_calibrations.append(QString(name));
		}
		if (m_calibrations.size() <= 1) {
			// only one unit: look for unit in attributes
			QString unit = attribute("Unit").toString();
			if (unit.isEmpty())
				unit = "Temperature (C)";
			m_calibrations = QStringList() << unit;
		}

		int current_calibration = propertyAt(0)->value<int>();
		if (current_calibration < 0)
			// set the highest calibration (temperature)
			this->propertyAt(0)->setData(m_calibrations.size() - 1);

		// set calibrations as property
		this->setProperty("Calibrations", QVariant::fromValue(m_calibrations));
	}
	else {
		close();
		return false;
	}

	if (times.size())
		readData(times.first());

	setOpenMode(mode);
	return true;
}

QString VipRIRDevice::fileName() const
{
	if (m_file == 0)
		return QString();

	char dst[1000];
	if (VipLibRIR::instance()->get_filename(m_file, dst) != 0)
		return QString();
	return QString(dst);
}

int VipRIRDevice::getRawValue(int x, int y)
{
	if (m_file && isOpen() && x >= 0 && y >= 0 && x < m_size.width() && y < m_size.height()) {
		ushort value;
		VipLibRIR::instance()->get_last_image_raw_value(m_file, x, y, &value);
		return value;
	}
	return -1;
}

void VipRIRDevice::close()
{
	if (m_file) {
		VipLibRIR::instance()->close_camera(m_file);
		m_file = 0;
	}
	VipIODevice::close();
}

bool VipRIRDevice::readData(qint64 time)
{

	// set bad pixels
	int bp = VipLibRIR::instance()->bad_pixels_enabled(m_file);
	if ((bool)bp != propertyName("BadPixels")->value<bool>()) {
		VipLibRIR::instance()->enable_bad_pixels(m_file, propertyName("BadPixels")->value<bool>());
	}

	qint64 pos = this->computeTimeToPos(time);
	VipNDArrayType<unsigned short> ar(vipVector(m_size.height(), m_size.width()));

	int calib = propertyAt(0)->data().value<int>();
	if (calib < 0 || calib >= m_calibrations.size())
		return false;

	if (VipLibRIR::instance()->load_image(m_file, pos, calib, ar.ptr()) == 0) {
		QVariantMap attributes = VipLibRIR::instance()->getAttributes(m_file);

		QString device = attribute("Device").toString();
		QString camera = attribute("Camera").toString();
		if (device == "WEST" && calib != 0) {
			ar = ar.mid(vipVector(0, 0), vipVector(ar.shape(0) - 3, ar.shape(1))).copy();
		}

		// rotate left if necessary
		if (device == "WEST" && camera.contains("WAQ")) {
			VipNDArrayType<unsigned short> out(vipVector(ar.shape(1), ar.shape(0)));
			const VipNDArrayType<unsigned short>& in = ar;
			for (int y = 0; y < in.shape(0); ++y)
				for (int x = 0; x < in.shape(1); ++x)
					out(out.shape(0) - x - 1, y) = in(y, x);
			ar = out;
		}

		VipAnyData any = create(QVariant::fromValue(VipNDArray(ar)));
		any.setTime(time);
		any.setZUnit(m_calibrations[calib]);
		any.mergeAttributes(attributes);
		outputAt(0)->setData(any);

		return true;
	}
	return false;
}




VipRIRRecorder::VipRIRRecorder(QObject* parent)
  : VipIODevice(parent)
  , m_video(0)
{
	propertyAt(0)->setData(8);
	propertyAt(1)->setData(0);
	propertyAt(2)->setData(0);
}
	
VipRIRRecorder::~VipRIRRecorder()
{
	VipRIRRecorder::close();
}

void VipRIRRecorder::close()
{
	if (m_video > 0)
		VipLibRIR::instance()->h264_close_file(m_video);
	m_video = 0;
	VipIODevice::close();
}

bool VipRIRRecorder::open(VipIODevice::OpenModes modes) 
{
	close();
	if (modes != WriteOnly)
		return false;

	QString p = removePrefix(path());
	QFile out(p);
	if (!out.open(QFile::WriteOnly))
		return false;
	setOpenMode(modes);
	return true;
}

void VipRIRRecorder::apply()
{
	while (inputAt(0)->hasNewData()) {
	
		VipAnyData in = inputAt(0)->data();
		const VipNDArray ar = in.value<VipNDArray>().toUInt16();
		if (ar.shapeCount() != 2) {
			setError("Wrong shape count");
			return;
		}

		if (m_video == 0) {
			//initialize
			QString p = removePrefix(path());
			m_shape = ar.shape();
			m_video = VipLibRIR::instance()->h264_open_file(p.toLatin1().data(), m_shape[1], m_shape[0], m_shape[0]);
			if (m_video <= 0) {
				m_video = 0;
				setError("Unable to open output file");
				return;
			}

			m_attrs = in.attributes();

			QByteArray keys, values;
			QVector<int> key_lens, value_lens;
			for (auto it = m_attrs.begin(); it != m_attrs.end(); ++it) {
				QByteArray key = it.key().toLatin1();
				QByteArray v = it.value().toByteArray();
				keys += key;
				values += v;
				key_lens += key.size();
				value_lens += v.size();
			}

			VipLibRIR::instance()->h264_set_global_attributes(m_video, key_lens.size(), keys.data(), key_lens.data(), values.data(), value_lens.data());

			int compression = propertyAt(0)->value<int>();
			if (compression < 0)
				compression = 0;
			if (compression > 8)
				compression = 8;

			VipLibRIR::instance()->h264_set_parameter(m_video, "compressionLevel", QByteArray::number(compression));
			VipLibRIR::instance()->h264_set_parameter(m_video, "lowValueError", QByteArray::number(propertyAt(1)->value<int>()));
			VipLibRIR::instance()->h264_set_parameter(m_video, "highValueError", QByteArray::number(propertyAt(2)->value<int>()));
			VipLibRIR::instance()->h264_set_parameter(m_video, "threads","4");
			VipLibRIR::instance()->h264_set_parameter(m_video, "slices", "4");
		}
	
		if (ar.shape() != m_shape) {
			setError("Wrong input image shape");
			return;
		}

		// ComputeAttributes
		QVariantMap attrs = in.attributes();
		for (auto it = attrs.begin(); it != attrs.end();) {
			auto found = m_attrs.find(it.key());
			if (found != m_attrs.end() && found.value().toByteArray() == it.value().toByteArray())
				it = attrs.erase(it);
			else
				++it;
		}
		QByteArray keys, values;
		QVector<int> key_lens, value_lens;
		for (auto it = attrs.begin(); it != attrs.end(); ++it) {
			QByteArray key = it.key().toLatin1();
			QByteArray v = it.value().toByteArray();
			keys += key;
			values += v;
			key_lens += key.size();
			value_lens += v.size();
		}

		int low_error = propertyAt(1)->value<int>();
		int high_error = propertyAt(2)->value<int>();
		int ret = 0;
		if (low_error == 0 && high_error == 0)
			ret = VipLibRIR::instance()->h264_add_image_lossless(m_video, (unsigned short*)ar.data(), in.time(), attrs.size(), keys.data(), key_lens.data(), values.data(), value_lens.data());
		else
			ret =
			  VipLibRIR::instance()->h264_add_image_lossy(m_video, (unsigned short*)ar.data(), in.time(), attrs.size(), keys.data(), key_lens.data(), values.data(), value_lens.data());

		if (ret < 0) {
			setError("Unable to write image");
			return;
		}
	}
}







#include "VipStandardWidgets.h"
#include <qboxlayout.h>

class VipRIRDeviceEditor::PrivateData
{
public:
	QToolButton* badPixels;
	QComboBox* calibrations;

	QPointer<VipRIRDevice> device;

	QAction* badPixelsAction;
	QAction* calibrationsAction;
};

VipRIRDeviceEditor::VipRIRDeviceEditor(VipVideoPlayer* player)
  : QObject(player)
{
	m_data = new PrivateData();

	m_data->badPixels = new QToolButton();
	m_data->calibrations = new QComboBox();

	m_data->badPixelsAction = player->toolBar()->addWidget(m_data->badPixels);
	m_data->calibrationsAction = player->toolBar()->addWidget(m_data->calibrations);

	m_data->badPixels->setAutoRaise(false);
	m_data->badPixels->setText("BP");
	m_data->badPixels->setCheckable(true);
	m_data->badPixels->setToolTip("Remove bad pixels");

	m_data->calibrations->setToolTip("Select calibration");

	connect(m_data->calibrations, SIGNAL(currentIndexChanged(int)), this, SLOT(updateDevice()));
	connect(m_data->badPixels, SIGNAL(clicked(bool)), this, SLOT(setBadPixels(bool)));
}

VipRIRDeviceEditor::~VipRIRDeviceEditor()
{
	delete m_data;
}

void VipRIRDeviceEditor::setDevice(VipRIRDevice* dev)
{
	m_data->device = nullptr;
	if (dev) {
		QStringList calibs = dev->calibrations();
		bool has_calibrations = calibs.size() > 1;

		m_data->calibrationsAction->setVisible(has_calibrations);

		if (has_calibrations) {

			m_data->calibrations->blockSignals(true);
			m_data->calibrations->clear();
			m_data->calibrations->addItems(calibs);
			int calib = dev->propertyAt(0)->value<int>();
			if (calib >= 0 && calib < calibs.size())
				m_data->calibrations->setCurrentIndex(calib);

			m_data->calibrations->blockSignals(false);
		}

		m_data->device = dev;

		// bad pixels
		m_data->badPixels->setVisible(true);
		m_data->badPixels->blockSignals(true);
		m_data->badPixels->setChecked(dev->propertyName("BadPixels")->value<bool>());
		m_data->badPixels->blockSignals(false);
	}
}

VipRIRDevice* VipRIRDeviceEditor::device() const
{
	return m_data->device;
}

void VipRIRDeviceEditor::setBadPixels(bool enable)
{
	if (m_data->device) {
		m_data->device->propertyName("BadPixels")->setData(enable);
		m_data->device->reload();
	}
}

void VipRIRDeviceEditor::updateDevice()
{
	if (m_data->device) {
		int calib = m_data->device->propertyAt(0)->value<int>();
		int new_calib = m_data->calibrations->currentIndex();

		if (new_calib != calib) {
			m_data->device->propertyAt(0)->setData(new_calib);
			m_data->device->reload();
			Q_EMIT deviceUpdated();
		}
	}
}



#include <qgridlayout.h>

class VipRIRRecorderEditor::PrivateData
{
public:
	QSpinBox compression;
	QSpinBox lowError;
	QSpinBox highError;
	QPointer<VipRIRRecorder> device;
};

VipRIRRecorderEditor::VipRIRRecorderEditor(QWidget* parent)
  : QWidget(parent)
{
	m_data = new PrivateData();

	QGridLayout* lay = new QGridLayout();

	lay->addWidget(new QLabel("Compression level"), 0, 0);
	lay->addWidget(&m_data->compression, 0, 1);

	lay->addWidget(new QLabel("Low temperature error"), 1, 0);
	lay->addWidget(&m_data->lowError, 1, 1);

	lay->addWidget(new QLabel("High temperature error"), 2, 0);
	lay->addWidget(&m_data->highError, 2, 1);

	setLayout(lay);

	m_data->compression.setRange(0, 8);
	m_data->compression.setValue(0);

	m_data->lowError.setRange(0, 10);
	m_data->lowError.setValue(0);

	m_data->highError.setRange(0, 10);
	m_data->highError.setValue(0);

	connect(&m_data->compression, SIGNAL(valueChanged(int)), this, SLOT(updateDevice()));
	connect(&m_data->lowError, SIGNAL(valueChanged(int)), this, SLOT(updateDevice()));
	connect(&m_data->highError, SIGNAL(valueChanged(int)), this, SLOT(updateDevice()));
}
	
VipRIRRecorderEditor::~VipRIRRecorderEditor()
{
	delete m_data;
}

void VipRIRRecorderEditor::setDevice(VipRIRRecorder* dev) {
	if (dev != m_data->device) {
		m_data->device = dev;
		if (dev) {
			m_data->compression.blockSignals(true);
			m_data->lowError.blockSignals(true);
			m_data->highError.blockSignals(true);

			m_data->compression.setValue(dev->propertyAt(0)->value<int>());
			m_data->lowError.setValue(dev->propertyAt(1)->value<int>());
			m_data->highError.setValue(dev->propertyAt(2)->value<int>());

			m_data->compression.blockSignals(false);
			m_data->lowError.blockSignals(false);
			m_data->highError.blockSignals(false);
		}
	}
}
VipRIRRecorder* VipRIRRecorderEditor::device() const
{
	return m_data->device;
}

void VipRIRRecorderEditor::updateDevice()
{
	if (VipRIRRecorder* r = m_data->device) {
		r->propertyAt(0)->setData(m_data->compression.value());
		r->propertyAt(1)->setData(m_data->lowError.value());
		r->propertyAt(2)->setData(m_data->highError.value());
	}
}









CustomizeRIRVideoPlayer::CustomizeRIRVideoPlayer(VipVideoPlayer* player, VipRIRDevice* device)
  : QObject(player)
  , m_device(device)
  , m_player(player)
{
	if (device) {
		player->setProperty("VipRIRDevice", true);
		player->toolBar()->addSeparator();
		m_options = new VipRIRDeviceEditor(player);
		m_options->setDevice(device);

		// player->spectrogram()->setToolTipText("<b>X</b>: #avalue0%i<br><b>Y</b>: #avalue1%i<br><b>Value</b>: #value<br>#dINFOS");
	}
}

static void displayVipRIRDeviceOptions(VipVideoPlayer* player)
{
	if (player->property("VipRIRDevice").toBool())
		return;

	if (VipDisplayObject* display = player->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>()) {
		QList<VipProcessingObject*> src = display->allSources();

		// find the source VipRIRDevice
		QList<VipRIRDevice*> devices = vipListCast<VipRIRDevice*>(src);
		if (devices.size() == 1) {
			new CustomizeRIRVideoPlayer(player, devices.first());
		}
	}
}


static QWidget* editRIRRecorder(VipRIRRecorder* r) {
	VipRIRRecorderEditor* ed = new VipRIRRecorderEditor();
	ed->setDevice(r);
	return ed;
}

static int registerEditor()
{
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(displayVipRIRDeviceOptions);
	vipFDObjectEditor().append<QWidget*(VipRIRRecorder*)>(editRIRRecorder);
	return 0;
}

static bool _registerEditors = vipAddInitializationFunction(registerEditor);
