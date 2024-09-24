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
	VIP_CREATE_PRIVATE_DATA(d_data);
	outputAt(0)->setData(VipNDArray());
}

H5GenericVideoReader::~H5GenericVideoReader()
{
	d_data->still_reader.close();
	d_data->video_reader.close();
	d_data->ecrh_reader.close();
	close();
}

HDF5VideoReader * H5GenericVideoReader::videoReader() const
{
	return const_cast<HDF5VideoReader*>(&d_data->video_reader);
}

H5StillImageReader * H5GenericVideoReader::imageReader() const
{
	return const_cast<H5StillImageReader*>(&d_data->still_reader);
}

HDF5_ECRHVideoReader * H5GenericVideoReader::ecrhVideoReader() const
{
	return const_cast<HDF5_ECRHVideoReader*>(&d_data->ecrh_reader);
}

bool H5GenericVideoReader::open(VipIODevice::OpenModes mode)
{
	//close();

	if (!(mode & VipIODevice::ReadOnly))
		return false;

	QIODevice * dev = createDevice(path(), QIODevice::ReadOnly);
	if (!dev)
		return false;

	d_data->still_reader.setAttributes(this->attributes());
	d_data->video_reader.setAttributes(this->attributes());
	d_data->ecrh_reader.setAttributes(this->attributes());

	d_data->video_reader.setDevice(nullptr);
	d_data->ecrh_reader.setDevice(nullptr);

	d_data->still_reader.setDevice(dev);
	if (d_data->still_reader.open(mode))
	{
		VipAnyData any = d_data->still_reader.outputAt(0)->data();
		any.setSource((qint64)this);
		outputAt(0)->setData(any);
		this->setTimestamps(VipTimestamps() << 0);
		setOpenMode(mode);
		return true;
	}
	else
	{
		d_data->still_reader.setDevice(nullptr);
		d_data->ecrh_reader.setDevice(nullptr);

		delete dev;
		setDevice(nullptr);
		dev = createDevice(path(), QIODevice::ReadOnly);
		if (!dev)
			return false;

		d_data->video_reader.setPath(path());
		d_data->video_reader.setDevice(dev);
		if (d_data->video_reader.open(mode))
		{
			VipAnyData any = d_data->video_reader.outputAt(0)->data();
			any.setSource((qint64)this);
			outputAt(0)->setData(any);
			this->setTimestamps(d_data->video_reader.timestamps());
			setOpenMode(mode);
			return true;
		}
		
		else
		{
			d_data->still_reader.setDevice(nullptr);
			d_data->video_reader.setDevice(nullptr);

			delete dev;
			setDevice(nullptr);
			dev = createDevice(path(), QIODevice::ReadOnly);
			if (!dev)
				return false;

			d_data->ecrh_reader.setDevice(dev);
			if (d_data->ecrh_reader.open(mode))
			{
				VipAnyData any = d_data->ecrh_reader.outputAt(0)->data();
				any.setSource((qint64)this);
				outputAt(0)->setData(any);
				this->setTimestamps(d_data->ecrh_reader.timestamps());
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
	d_data->still_reader.setDevice(nullptr);
	d_data->video_reader.setDevice(nullptr);*/
	d_data->still_reader.close();
	d_data->video_reader.close();
	d_data->ecrh_reader.close();
	VipTimeRangeBasedGenerator::close();
}

bool H5GenericVideoReader::readData(qint64 time)
{
	if (d_data->still_reader.isOpen())
	{
		VipAnyData any = d_data->still_reader.outputAt(0)->data();
		any.setSource((qint64)this);
		any.mergeAttributes(this->attributes());
		outputAt(0)->setData(any);
		return true;
	}
	else if (d_data->video_reader.isOpen())
	{
		if (d_data->video_reader.read(time))
		{
			VipAnyData any = d_data->video_reader.outputAt(0)->data();
			any.setSource((qint64)this);
			any.mergeAttributes(this->attributes());
			outputAt(0)->setData(any);
			return true;
		}
	}
	else if (d_data->ecrh_reader.isOpen())
	{
		if (d_data->ecrh_reader.read(time))
		{
			VipAnyData any = d_data->ecrh_reader.outputAt(0)->data();
			any.setSource((qint64)this);
			any.mergeAttributes(this->attributes());
			outputAt(0)->setData(any);
			return true;
		}
	}
	return false;
}