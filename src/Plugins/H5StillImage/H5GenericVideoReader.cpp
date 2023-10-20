#include "H5GenericVideoReader.h"


class H5GenericVideoReader::PrivateData
{
public:
	H5StillImageReader still_reader;
	HDF5VideoReader video_reader;
	HDF5_ECRHVideoReader ecrh_reader;
};

H5GenericVideoReader::H5GenericVideoReader(QObject * parent)
	:VipTimeRangeBasedGenerator(parent)
{
	m_data = new PrivateData();
	outputAt(0)->setData(VipNDArray());
}

H5GenericVideoReader::~H5GenericVideoReader()
{
	m_data->still_reader.close();
	m_data->video_reader.close();
	m_data->ecrh_reader.close();
	close();
	delete m_data;
}

HDF5VideoReader * H5GenericVideoReader::videoReader() const
{
	return const_cast<HDF5VideoReader*>(&m_data->video_reader);
}

H5StillImageReader * H5GenericVideoReader::imageReader() const
{
	return const_cast<H5StillImageReader*>(&m_data->still_reader);
}

HDF5_ECRHVideoReader * H5GenericVideoReader::ecrhVideoReader() const
{
	return const_cast<HDF5_ECRHVideoReader*>(&m_data->ecrh_reader);
}

bool H5GenericVideoReader::open(VipIODevice::OpenModes mode)
{
	//close();

	if (!(mode & VipIODevice::ReadOnly))
		return false;

	QIODevice * dev = createDevice(path(), QIODevice::ReadOnly);
	if (!dev)
		return false;

	m_data->still_reader.setAttributes(this->attributes());
	m_data->video_reader.setAttributes(this->attributes());
	m_data->ecrh_reader.setAttributes(this->attributes());

	m_data->video_reader.setDevice(nullptr);
	m_data->ecrh_reader.setDevice(nullptr);

	m_data->still_reader.setDevice(dev);
	if (m_data->still_reader.open(mode))
	{
		VipAnyData any = m_data->still_reader.outputAt(0)->data();
		any.setSource((qint64)this);
		outputAt(0)->setData(any);
		this->setTimestamps(VipTimestamps() << 0);
		setOpenMode(mode);
		return true;
	}
	else
	{
		m_data->still_reader.setDevice(nullptr);
		m_data->ecrh_reader.setDevice(nullptr);

		delete dev;
		setDevice(nullptr);
		dev = createDevice(path(), QIODevice::ReadOnly);
		if (!dev)
			return false;

		m_data->video_reader.setPath(path());
		m_data->video_reader.setDevice(dev);
		if (m_data->video_reader.open(mode))
		{
			VipAnyData any = m_data->video_reader.outputAt(0)->data();
			any.setSource((qint64)this);
			outputAt(0)->setData(any);
			this->setTimestamps(m_data->video_reader.timestamps());
			setOpenMode(mode);
			return true;
		}
		
		else
		{
			m_data->still_reader.setDevice(nullptr);
			m_data->video_reader.setDevice(nullptr);

			delete dev;
			setDevice(nullptr);
			dev = createDevice(path(), QIODevice::ReadOnly);
			if (!dev)
				return false;

			m_data->ecrh_reader.setDevice(dev);
			if (m_data->ecrh_reader.open(mode))
			{
				VipAnyData any = m_data->ecrh_reader.outputAt(0)->data();
				any.setSource((qint64)this);
				outputAt(0)->setData(any);
				this->setTimestamps(m_data->ecrh_reader.timestamps());
				setOpenMode(mode);
				return true;
			}
			else
			{
				setDevice(nullptr);
				delete dev;
			}
		}
		
	}
	
	return false;
}

void H5GenericVideoReader::close()
{
	/*setDevice(nullptr);
	m_data->still_reader.setDevice(nullptr);
	m_data->video_reader.setDevice(nullptr);*/
	m_data->still_reader.close();
	m_data->video_reader.close();
	m_data->ecrh_reader.close();
	VipTimeRangeBasedGenerator::close();
}

bool H5GenericVideoReader::readData(qint64 time)
{
	if (m_data->still_reader.isOpen())
	{
		VipAnyData any = m_data->still_reader.outputAt(0)->data();
		any.setSource((qint64)this);
		any.mergeAttributes(this->attributes());
		outputAt(0)->setData(any);
		return true;
	}
	else if (m_data->video_reader.isOpen())
	{
		if (m_data->video_reader.read(time))
		{
			VipAnyData any = m_data->video_reader.outputAt(0)->data();
			any.setSource((qint64)this);
			any.mergeAttributes(this->attributes());
			outputAt(0)->setData(any);
			return true;
		}
	}
	else if (m_data->ecrh_reader.isOpen())
	{
		if (m_data->ecrh_reader.read(time))
		{
			VipAnyData any = m_data->ecrh_reader.outputAt(0)->data();
			any.setSource((qint64)this);
			any.mergeAttributes(this->attributes());
			outputAt(0)->setData(any);
			return true;
		}
	}
	return false;
}