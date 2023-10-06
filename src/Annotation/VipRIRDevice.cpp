#include "VipRIRDevice.h"
#include "VipLibRIR.h"


VipRIRDevice::VipRIRDevice(QObject * parent)
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
	close();
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

	m_file = VipLibRIR::instance()->open_camera_file(p.toLatin1().data(), NULL);
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
	m_data->device = NULL;
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
		
		if (new_calib != calib ) {
			m_data->device->propertyAt(0)->setData(new_calib);
			m_data->device->reload();
			Q_EMIT deviceUpdated();
		}
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
			
		//player->spectrogram()->setToolTipText("<b>X</b>: #avalue0%i<br><b>Y</b>: #avalue1%i<br><b>Value</b>: #value<br>#dINFOS");
	}

}




static void displayVipRIRDeviceOptions(VipVideoPlayer* player)
{
	if (player->property("VipRIRDevice").toBool())
		return;

	if (VipDisplayObject* display = player->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>()) 
	{
		QList<VipProcessingObject*> src = display->allSources();

		// find the source VipRIRDevice
		QList<VipRIRDevice*> devices = vipListCast<VipRIRDevice*>(src);
		if (devices.size() == 1) {
			new CustomizeRIRVideoPlayer(player, devices.first());
		}
	}
}


static int registerEditor()
{
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(displayVipRIRDeviceOptions);
	return 0;
}

static bool _registerEditors = vipAddInitializationFunction(registerEditor);
