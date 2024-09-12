#include "IRBLoader.h"
#include "VipMath.h"


// System includes
#include <iostream>

// Library includes
#include <QFileInfo>
#include <QtEndian>
#include <QDateTime>
#include <QVector>
#include <qmath.h>


struct IRB_BLOCK_INFO
{
	qint32 frames, next_iblock, nr_iblock;
	QVector<qint32> d_Ptr_out, d_size_out, fr_nr_out, i_vers_out;
};

template < class T>
T get(const void * dat) {
	T res;
	memcpy(&res, dat, sizeof(T));
	return res; 
}

template< class T>
QList<int> where(const QVector<T> & a, T val) {
	QList<int> res;
	for (int i = 0; i< a.size(); ++i)
		if (a[i] == val)
			res.append(i);
	return res;
}

template< class T>
QList<int> wherele(const QVector<T> & a, T val) {
	QList<int> res;
	for (int i = 0; i< a.size(); ++i)
		if (a[i] <= val)
			res.append(i);
	return res;
}

template< class T>
QList<int> wherege(const QVector<T> & a, T val) {
	QList<int> res;
	for (int i = 0; i< a.size(); ++i)
		if (a[i] >= val)
			res.append(i);
	return res;
}

template< class T>
QList<int> where(const T* a,int size, T val) {
	QList<int> res;
	const T * data = a;
	for (int i = 0; i< size; ++i)
		if (data[i] == val)
			res.append(i);
	return res;
}

template< class T>
QList<int> wherele(const T* a, int size, T val) {
	QList<int> res;
	const T * data = a;
	for (int i = 0; i< size; ++i)
		if (data[i] <= val)
			res.append(i);
	return res;
}

template< class T>
QList<int> wherege(const T* a, int size, T val) {
	QList<int> res;
	const T * data = a;
	for (int i = 0; i< size; ++i)
		if (data[i] >= val)
			res.append(i);
	return res;
}

template< class T>
void Print(const QVector<T> & vect, const QString & before = QString())
{
	std::cout << before.toLatin1().data() << " ";
	for (int i = 0; i< vect.size(); ++i)
		std::cout << vect[i] << " ";
	std::cout << std::endl;
}

template< class T>
void Print(const QList<T> & vect, const QString & before = QString())
{
	std::cout << before.toLatin1().data() << " ";
	for (int i = 0; i< vect.size(); ++i)
		std::cout << vect[i] << " ";
	std::cout << std::endl;
}

static IRB_BLOCK_INFO read_blocks(QIODevice * device, qint32 reord, qint32 /*next_iblock*/, qint32 nr_iblock)
{
	//read the information
	QVector<qint32>   type_of_block(nr_iblock, 0);
	QVector<qint32>   d_Ptr(nr_iblock, 0);
	QVector<qint32>   d_size(nr_iblock, 0);
	QVector<qint32>   fr_nr(nr_iblock, 0);
	QVector<qint32>   i_vers(nr_iblock, 0);

	for (int i = 0; i< nr_iblock; ++i)
	{
		//TODO: swap endian needs to be implemented here if reord = 1
		char temp[32];
		device->read(temp, 32);
		qint16 us[2];
		qint32 in[7];
		memcpy(us, temp, 4);
		memcpy(in, &temp[4], 28);

		if (reord == 1)
		{
			us[0] = qFromBigEndian(us[0]);
			us[1] = qFromBigEndian(us[1]);
			for (int j = 0; j< 7; ++j)
				in[j] = qFromBigEndian(in[j]);
		}

		type_of_block[i] = us[0];
		d_Ptr[i] = in[2];
		d_size[i] = in[3];
		fr_nr[i] = in[1];
		i_vers[i] = in[0];

		//std::cout<<us[0]<<" "<<us[1]<<" "<<in[0]<<" "<<in[1]<<" "<<in[2]<<" "<<in[3]<<" "<<in[4]<<" "<<in[5]<<" "<<in[6]<<" "<<std::endl;
	}

	/*Print(type_of_block,"type_of_block ");
	Print(d_Ptr,"d_Ptr ");
	Print(d_size,"d_size ");
	Print(fr_nr,"fr_nr ");
	Print(i_vers,"i_vers ");*/

	//check if more blocks exist

	QList<int>   ib_ind = where(type_of_block, 4);
	QList<int>    fr_ind = where(type_of_block, 1);
	//print 'rb_fr_ind:',fr_ind
	qint32 ic = type_of_block.count(4);
	qint32 count = type_of_block.count(1);
	IRB_BLOCK_INFO out;
	//print 'count =',count
	if (ic >= 1)
	{
		//print'#there are still some datablocks'
		out.next_iblock = d_Ptr[ib_ind[0]];
		out.nr_iblock = d_size[ib_ind[0]] / 32;
	}
	else
	{
		//print'#all datablocks are found'
		out.next_iblock = 0;
		out.nr_iblock = 0;
	}



	if (count >= 1)
	{
		//print len(fr_ind)

		//for i in xrange(len(fr_ind)):  # fehler !!!! hier wird nur das letzte element genommen
		for (int i = 0; i< fr_ind.size(); ++i)
		{
			out.d_Ptr_out.push_back(d_Ptr[fr_ind[i]]);
			out.d_size_out.push_back(d_size[fr_ind[i]]);
			out.fr_nr_out.push_back(fr_nr[fr_ind[i]]);
			out.i_vers_out.push_back(i_vers[fr_ind[i]]);
			out.frames = count;
		}
	}
	else
	{
		out.frames = 0;
		out.d_Ptr_out = d_Ptr;
		out.d_size_out = d_size;
		out.fr_nr_out = fr_nr;
		out.i_vers_out = i_vers;
	}
	/*print 'd_Ptr_out:', d_Ptr_out
	"""
	input variables are not changed, therefore everything must be given back
	"""
	#print 'End of read blocks------------------------'
	*/


	return out;
}



IRBLoader::IRBLoader()
{
	outputAt(0)->setData(VipNDArray());
	propertyAt(0)->setData(1);
}
IRBLoader::~IRBLoader()
{
	close();
}

/*static float ReverseFloat(const float inFloat)
{
	float retVal;
	char *floatToConvert = (char*)& inFloat;
	char *returnFloat = (char*)& retVal;

	// swap the bytes into a temporary buffer
	returnFloat[0] = floatToConvert[3];
	returnFloat[1] = floatToConvert[2];
	returnFloat[2] = floatToConvert[1];
	returnFloat[3] = floatToConvert[0];

	return retVal;
}*/

bool IRBLoader::open(VipIODevice::OpenModes mode)
{
	if (!(mode & ReadOnly))
		return false;

	try {

		QIODevice * device = this->createDevice(removePrefix(path()), QIODevice::ReadOnly);
		if (!device)
			return false;

		quint32 file_size = device->size();

		//Read file header
		if (device->read((char*)&m_header, sizeof(m_header)) != sizeof(m_header))
			throw std::runtime_error("");

		if (QByteArray(m_header.filetype, 3) != QByteArray("IRB"))
			throw std::runtime_error("");


		//this test is do determine if we have big or small endian
		if (m_header.ffversion != 100 && m_header.ffversion != 101)
			m_order = 1;
		else
			m_order = 0;

		//Now we beginn reading the i_blocks wich contain the information where the data is
		qint32 next_iblock = m_header.indexOff;// the first block is the 0 Block
		qint32 nr_iblock = m_header.nrAvIndexes;
		qint32 frames = 0;

		QVector<qint32> d_Ptr, d_size, fr_nr, i_vers;

		while (next_iblock != 0)
		{
			qint32 controll_iblock = next_iblock;
			device->seek(next_iblock);
			//The read_blocks() function gives the Informationen where the data is and how many data there is
			IRB_BLOCK_INFO output = read_blocks(device, m_order, next_iblock, nr_iblock);
			qint32 dframes = output.frames;
			QVector<qint32> dd_Ptr = output.d_Ptr_out;
			QVector<qint32> dd_size = output.d_size_out;
			QVector<qint32> dfr_nr = output.fr_nr_out;
			QVector<qint32> di_vers = output.i_vers_out;
			next_iblock = output.next_iblock;
			nr_iblock = output.nr_iblock;

			if (frames == 0) //  # only do the for the first frame/i-block
			{
				d_Ptr = dd_Ptr;
				d_size = dd_size;
				fr_nr = dfr_nr;
				i_vers = di_vers;
			}
			else // # for every other iblock the read out from read_block is appended to the arrays
			{
				d_Ptr = d_Ptr + (dd_Ptr);
				d_size = d_size + dd_size;
				fr_nr = fr_nr + dfr_nr;
				i_vers = i_vers + di_vers;
			}

			frames = frames + dframes;
			if ((((qint64)controll_iblock > (qint64)next_iblock) && ((qint64)next_iblock != 0)) || ((qint64)next_iblock >= (qint64)file_size))
			{
				//print ' There is a bug in the file stopping'
				//print " Stopping at frame:", frames
				next_iblock = 0;
				/*
				This is only a bug in the file the programm will proceed normaly and the already found frames can be used
				*/
			}
		}


		/*
		Find the iblocks that have the same type with data in them
		*/
		QList<int> jj = where(i_vers, i_vers[0]);

		QVector<qint32> data_Ptr(jj.size(), 0);
		QVector<qint32> data_size(jj.size(), 0);
		QVector<qint32> frame_nr(jj.size(), 0);
		QVector<qint32> i_version(jj.size(), 0);

		for (int i = 0; i< jj.size(); ++i)
		{
			data_Ptr[i] = d_Ptr[jj[i]];
			data_size[i] = d_size[jj[i]];
			frame_nr[i] = fr_nr[jj[i]];
			i_version[i] = i_vers[jj[i]];
		}


		//check if the frames are in the right order
		QVector<qint32> temp = frame_nr;
		temp.insert(0, temp.back());
		temp.pop_back();

		QVector<qint32> delta(temp.size());
		for (int i = 0; i< delta.size(); ++i)
			delta[i] = frame_nr[i] - temp[i];

		if (delta[0] == 1 - frames)
			delta[0] = 1;

		QList<int> dInd = wherele(delta, 0);

		if (dInd.size() >= 1)
		{    //going to cut off the wrong frames at dInd
			for (int i = 0; i< dInd[1] - 1; ++i)
			{
				data_Ptr.pop_back();
				data_size.pop_back();
				frame_nr.pop_back();
				i_version.pop_back();
			}

			frames = frame_nr.size();
		}

		/*
		Reading the header for each frame
		*/

		qint32 frameheadersize;
		if (i_version[0] == 100)
			frameheadersize = 744;
		else  //i_version[0]=101
			frameheadersize = 1728;


		m_time = QVector<double>(frames, 0);
		m_tempvals = QVector< QVector<double> >(frames, QVector<double>(270, 0.0));

		if (i_version[0] == 100)
		{
			for (int i = 0; i< frames; ++i)
			{
				IRB_FRAME_HEADER_100 header;
				device->seek(data_Ptr[i]);
				if (device->read((char*)&header, frameheadersize) != frameheadersize)
					throw std::runtime_error("");

				m_time[i] = header.val22[26] / 1000.0;
				m_tempvals[i].resize(39);
				std::copy(header.val22, header.val22 + 39, m_tempvals[i].begin());

				if (i == 0)
				{
					m_compressed = header.val2;
					m_width = header.val3;
					m_height = header.val4;
					m_datasize = data_size;
					m_lamsp = header.val18;
					m_pA = 1.0 / (qExp(14388 / m_lamsp / header.val14) - 1);
					m_pP = 1.0 / (qExp(14388 / m_lamsp / header.val16) - 1);
				}
			}
		}
		else //version 101
		{
			for (int i = 0; i< frames; ++i)
			{
				IRB_FRAME_HEADER_101 header;
				device->seek(data_Ptr[i]);
				if (device->read((char*)&header, frameheadersize) != frameheadersize)
					throw std::runtime_error("");

				
				quint32 * t2 = (uint*)(&header.val43);

				m_time[i] = *t2 * 1000000;//header.val43 / 1000.0;
				m_tempvals[i].resize(270);
				std::copy(header.val22, header.val22 + 270, m_tempvals[i].begin());

				if (i == 0)
				{
					m_compressed = header.val2;
					m_width = header.val3;
					m_height = header.val4;
					m_datasize = data_size;
					m_lamsp = header.val18;

					m_pA = 1.0 / (qExp(14388 / m_lamsp / header.val14) - 1);
					m_pP = 1.0 / (qExp(14388 / m_lamsp / header.val16) - 1);
				}
			}
		}

		//correct the timestamps
		for (int i = 1; i< m_time.size() - 1; ++i)
		{
			if (m_time[i] < m_time[i - 1])
				m_time[i] = (m_time[i - 1] + m_time[i + 1]) / 2;
		}

		//set the timestamps;
		QVector<qint64> times(m_time.size(),0);
		for (int i = 0; i < m_time.size(); ++i)
			times[i] = m_time[i];
		setTimestamps(times,false);

		m_data_positions = data_Ptr;
		for (int i = 0; i< m_data_positions.size(); ++i) m_data_positions[i] += frameheadersize;

		//reset emissivity and transmissivity
		//MultiArray<double> m_emissivity;
		m_emissivity = VipNDArrayType<double>(vipVector(m_height,m_width));
		m_emissivity.fill(0.8);
		m_transmissivity = VipNDArrayType<double>(vipVector(m_height, m_width));
		m_transmissivity.fill(0.7);

		setOpenMode(mode);
		readData(0);
		return true;

	}//end try block
	catch (const std::exception & /*e*/)
	{
		return false;
	}
	return false;
}

void IRBLoader::close()
{
	VipIODevice::close();
}


bool IRBLoader::readData(qint64 time)
{
	qint64 pos = this->computeTimeToPos(time);
	if (pos < 0)
		pos = 0;
	else if (pos >= size())
		pos = size() - 1;


	VipNDArrayType<unsigned short> image(vipVector(m_height,m_width));
	qint32 length = m_width*m_height;

	if (m_compressed == 1)
	{
		//print 'read data compressed'
		//smallarray=array.array('B',[0,]*length)
		//print datapositions_i,length
		device()->seek(m_data_positions[pos]);
		QVector<quint8> smallarray(length);
		device()->read((char*)&smallarray[0], length);

		QVector<quint8> bigarray = smallarray;
		qint32 totalnumb = 0;
		qint32 totalnumsoll = length;
		//print smallarray.max(),smallarray.argmax(),smallarray[4990:5010]
		//print 'reading bigarray'
		QVector<quint8> temp(2);
		while (totalnumb < totalnumsoll)
		{
			device()->read((char*)&temp[0], 2);
			quint8 numb = temp[0];
			//print numb,numb+totalnumb, totalnumsoll
			quint8 val = temp[1];
			//print len(bigarray), totalnumb+numb
			for (int i = totalnumb; i< totalnumb + numb; ++i)
			{
				bigarray[i] = val;
			}

			totalnumb = totalnumb + numb;
		}
		//print totalnumb, totalnumsoll
		//print bigarray.max(),bigarray.argmax()
		// End while
		//print len(dataarray)
		//print 'writing dataarray'

		unsigned short * data = (unsigned short *)image.data();
		for (int i = 0; i< length; ++i)
			data[i] = smallarray[i] + (bigarray[i] * 256);

	}
	else
	{
		//print 'read data uncompressed'
		device()->seek(m_data_positions[pos]);
		//temp_dataarray=datei_zeiger.read(len(dataarray))
		//print dataarray.size
		//print 'unpack data'
		//dataarray=[s.unpack('B',temp_dataarray[i])[0] for i in xrange(length)]
		//print length*2
		device()->read((char*)image.data(), length * 2);
		//print dataarray.min(),dataarray.max()
		//print 'dataarray:',dataarray
		//print len(dataarray)
	}

	VipNDArray ar = image;
	if (propertyAt(0)->value<bool>())
	{
		VipNDArrayType<double> tmp(image.shape());

		double min;
		ToTemp((unsigned short*)ar.data(), ar.size(), pos,tmp,&min);
		double * ptr = tmp.ptr();
		for (int i = 0; i < tmp.size(); ++i)
			if (vipIsNan(ptr[i]))
				ptr[i] = min;

		//replace NaN values by the min value
		ar = tmp;
	}
	
	VipAnyData out = create(QVariant::fromValue(ar));
	out.setTime(time);
	out.setZUnit(calibrations()[propertyAt(0)->value<bool>()]);
	outputAt(0)->setData(out);

	return true;
}
/*
void IRBLoader::SetSceneModelValues()
{
	if (GetSceneModel() && !m_dl)
	{
		m_emissivity.View().Fill(1.0);
		m_transmissivity.View().Fill(1.0);
		ExportProperty(GetSceneModel(), "emissivity", m_emissivity.View());
		ExportProperty(GetSceneModel(), "transmissivity", m_transmissivity.View());
	}

}

void IRBLoader::SetSceneModelValuesUpdate()
{
	if (GetSceneModel() && !m_dl)
	{
		m_emissivity.View().Fill(1.0);
		m_transmissivity.View().Fill(1.0);
		ExportProperty(GetSceneModel(), "emissivity", m_emissivity.View());
		ExportProperty(GetSceneModel(), "transmissivity", m_transmissivity.View());

		emit ReadyRead();
	}
}


void  IRBLoader::SetSceneModel(SceneModel2D * scene_model)
{
	if (GetSceneModel())
	{
		disconnect(GetSceneModel(), SIGNAL(GraphicsShapeMoved()), this, SLOT(SetSceneModelValues()));
		disconnect(GetSceneModel(), SIGNAL(PropertyUpdated()), this, SLOT(SetSceneModelValuesUpdate()));
		connect(GetSceneModel(), SIGNAL(GraphicsShapeAdded()), this, SLOT(SetSceneModelValuesUpdate()));
		connect(GetSceneModel(), SIGNAL(GraphicsShapeRemoved()), this, SLOT(SetSceneModelValuesUpdate()));
	}

	VideoDevice::SetSceneModel(scene_model);

	if (GetSceneModel())
	{
		connect(GetSceneModel(), SIGNAL(GraphicsShapeMoved()), this, SLOT(SetSceneModelValues()), Qt::DirectConnection);
		connect(GetSceneModel(), SIGNAL(PropertyUpdated()), this, SLOT(SetSceneModelValuesUpdate()), Qt::DirectConnection);
		connect(GetSceneModel(), SIGNAL(GraphicsShapeAdded()), this, SLOT(SetSceneModelValuesUpdate()), Qt::DirectConnection);
		connect(GetSceneModel(), SIGNAL(GraphicsShapeRemoved()), this, SLOT(SetSceneModelValuesUpdate()), Qt::DirectConnection);
	}

	SetSceneModelValues();
}
*/

bool IRBLoader::ToTemp(unsigned short * data, int /*size*/, qint64 position, VipNDArrayType<double> & _out, double * min)
{
	double *out = _out.ptr();
	double *em = m_emissivity.ptr();
	double *tm = m_transmissivity.ptr();
	qint32 length = m_width*m_height;
	QVector<quint8> msb(length, 0);
	QVector<quint8> lsb(length, 0);
	for (int i = 0; i< length; ++i)
	{
		msb[i] = data[i] / 256;
		lsb[i] = data[i] % 256;
	}

	QVector<double> & tempval = m_tempvals[position];
	for (int i = 0; i< length; ++i)
		out[i] = (tempval[msb[i]] + (tempval[msb[i] + 1] - tempval[msb[i]])*float(lsb[i]) / 256) - 273.15;

	/*
	If the maximum value for msb is reached lsb needs to be set to 0 or temps over the maximum detectable temp will be calculated
	*/

	QList<int> overflow = where(msb, *std::max_element(msb.begin(), msb.end()));
	if (overflow.size() > 0)
	{
		for (int i = 0; i<overflow.size(); ++i)
			lsb[overflow[i]] = 0;
	}

	double max = *std::max_element(out, out + length);
	QList<int> tolowvalues = wherele(_out.ptr(),_out.size(), -100.0);
	for (int i = 0; i< tolowvalues.size(); ++i)
		out[tolowvalues[i]] = max;

	tolowvalues = wherele(_out.ptr(), _out.size(), -10.0);
	for (int i = 0; i< tolowvalues.size(); ++i)
		out[tolowvalues[i]] = 0.0;

	double min_val = DBL_MAX;
	for (int i = 0; i< length; ++i)
	{
		double p1 = 1 / (qExp(14388.0 / m_lamsp / (out[i] + 273.15)) - 1);
		double p2 = (p1 - (1.0 - em[i])*tm[i] * m_pA - (1.0 - tm[i])*m_pP) / em[i] / tm[i];
		//double chier = qLn(1 / p2 + 1);
		//double chier2 = 14388.0 / m_lamsp / chier;
		//double temp = chier2 - 273.15;
		out[i] = 14388.0 / m_lamsp / qLn(1 / p2 + 1) - 273.15;
		if (!vipIsNan(out[i]))
			min_val = qMin(min_val, out[i]);
		//bool stop = false;
	}
	if (min) *min = min_val;

	return true;
}









CustomizeIRBVideoPlayer::CustomizeIRBVideoPlayer(VipVideoPlayer * player, IRBLoader * device)
	:QObject(player), m_device(device), m_player(player)
{
	player->toolBar()->addSeparator();
	QAction * temperature = player->toolBar()->addAction("T(C)");
	temperature->setCheckable(true);
	temperature->setChecked(device->propertyAt(0)->value<bool>());
	temperature->setToolTip("<b>Apply calibration</b><br>Convert to temperature (degrees Celsius) or to W / (m2 sr)");
	
	player->toolBar()->widgetForAction(temperature)->setStyleSheet("QToolButton {color:#3399FF; font:bold;}");

	connect(temperature, SIGNAL(triggered(bool)), this, SLOT(enableCalibration(bool)));
}

void CustomizeIRBVideoPlayer::enableCalibration(bool enable)
{
	if (m_device)
	{
		m_device->propertyAt(0)->setData((int)enable);
		m_device->reload();
	}
}


static void displayIRBDevice(VipVideoPlayer * player)
{
	if (VipDisplayObject * display = player->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>())
	{
		//find the source FLIRRadiometricJpeg
		QList<IRBLoader*> devices = vipListCast<IRBLoader*>(display->allSources());
		if (devices.size())
		{
			new CustomizeIRBVideoPlayer(player, devices.first());
		}
	}
}


static int registerEditors()
{
	vipFDPlayerCreated().append<void(VipVideoPlayer *)>(displayIRBDevice);
	return 0;
}
static int _registerEditors = registerEditors();












