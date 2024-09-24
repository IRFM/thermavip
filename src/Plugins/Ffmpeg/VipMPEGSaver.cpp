#include "VipMPEGSaver.h"
#include "VipLogging.h"
#include "p_VideoEncoder.h"

VipMPEGSaver::VipMPEGSaver(QObject* parent)
  : VipIODevice(parent)
{
	m_encoder = new VideoEncoder();
}

VipMPEGSaver::~VipMPEGSaver()
{
	close();
	delete m_encoder;
}

qint32 VipMPEGSaver::fullFrameWidth() const
{
	return m_encoder->GetWidth();
}
qint32 VipMPEGSaver::fullFrameHeight() const
{
	return m_encoder->GetHeight();
}

bool VipMPEGSaver::open(VipIODevice::OpenModes mode)
{
	if (mode & VipIODevice::ReadOnly)
		return false;

	if (this->isOpen())
		this->close();

	try {

		this->setOpenMode(mode);
		this->setSize(0);
		return true;
	}
	catch (const std::exception& e) {
		setError(e.what());
		return false;
	}
	VIP_UNREACHABLE();
	// return false;
}

void VipMPEGSaver::close()
{
	m_encoder->Close();
	setOpenMode(NotOpen);
}

void VipMPEGSaver::apply()
{
	VipAnyData in = inputAt(0)->data();
	VipNDArray ar = in.data().value<VipNDArray>();
	if (ar.isEmpty()) {
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	QImage img = vipToImage(ar);
	if (img.isNull()) {
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	if (!m_encoder->IsOpen()) {
		try {
			m_info.width = img.width();
			m_info.height = img.height();
			m_encoder->Open(this->removePrefix(path()).toLatin1().data(), m_info.width, m_info.height, m_info.fps, m_info.rate, m_info.codec_id);
		}
		catch (const std::exception& e) {
			setError(e.what());
			return;
		}
	}

	if (img.width() != fullFrameWidth() || img.height() != fullFrameHeight())
		img = img.scaled(fullFrameWidth(), fullFrameHeight(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

	try {
		if (!m_encoder->AddFrame(img)) {
			setError("unable to add image to video");
			return;
		}
	}
	catch (const std::exception& e) {
		setError(e.what());
		return;
	}

	this->setSize(this->size() + 1);
}

qint64 VipMPEGSaver::estimateFileSize() const
{
	return m_encoder->fileSize();
}

void VipMPEGSaver::setAdditionalInfo(const VipMPEGIODeviceHandler& info)
{
	m_info = info;
}

VipMPEGIODeviceHandler VipMPEGSaver::additionalInfo() const
{
	return m_info;
}

void VipMPEGSaver::setThreads(int th)
{
	m_encoder->SetThreads(th);
}
int VipMPEGSaver::threads() const
{
	return m_encoder->GetThreads();
}
