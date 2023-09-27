#pragma once


#include "VipIODevice.h"


// file extension
#define IRB_FILE_EXT    "irb"

#pragma pack (1)
/**
* IRB file header
*/
struct IRB_HEADER
{
	qint8 key;
	char filetype[3];
	qint8 noth;
	char unused1[16];
	qint32 ffversion;
	qint32 indexOff;
	qint32 nrAvIndexes;
	qint32 nextIndexID;
	char unused2[27];
};

/**
* IRB frame header for version 100
*/
struct IRB_FRAME_HEADER_100
{
	quint16 val1;
	qint16 val2;
	quint16 val3;
	quint16 val4;
	quint16 val5;
	quint16 val6;
	quint16 val7;
	quint16 val8;
	quint16 val9;
	quint16 val10;
	qint32 val11;
	float val12;
	float val13;
	float val14;
	float val15;
	float val16;
	qint32 val17;
	float val18;
	float val19;
	qint16 val20;
	qint16 val21;
	float val22[40];
	qint16 val23;
	float val24;
	float val25;//64
	char val26[182];
	float val27;
	float val28;//248
	char val29[212];
	float val30;//459
	float val31; //dih80bffh
	double val32;
	qint32 val33;
	qint16 val34;
	char val35[80];
	float val36;
	float val37;
	qint16 val38;
};

/**
* IRB frame header for version 101
*/
struct IRB_FRAME_HEADER_101
{
	quint16 val1;//H
	qint16 val2;//h
	quint16 val3;//H
	quint16 val4;//H
	quint16 val5;//H
	quint16 val6;//H
	quint16 val7;//H
	quint16 val8;//H
	quint16 val9;//H
	quint16 val10;//H
	char val11[4];//4b
	float val12;//f
	float val13;//f
	float val14;//f
	float val15;//f
	float val16;//f
	char val17[4];//4b
	float val18;//f
	float val19;//f
	quint16 val20;//H
	quint16 val21;//H
	float val22[270];//270f
	float val23[6];//6f
	float val24;//f
	float val25;//f
	float val26;//f
	float val27;//f
	float val28;//f
	char val29[182];//22b20b22b22b16b32b32b16b
	float val30;//f
	float val31;//f
	char val32[228];//32b32b16b32b32b16b16b4b16b32b  --720
	char val33;//b
	char val34;//b
	quint16 val35;//H
	quint16 val36;//H
	char val37;//b
	quint16 val38;//H
	char val39[5];//5b
	float val40;//f
	float val41;//f
	double val42;//d
	float val43;//f
	qint16 val44;//h
	char val45[80];//80b
	float val46;//f
	float val47;//f
	char val48[2];//2b
};

#pragma pack()



class IRBLoader : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)
	VIP_IO(VipProperty calibration)
public:

	IRBLoader();
	~IRBLoader();

	virtual bool open(VipIODevice::OpenModes mode);
	virtual QString fileFilters() const { return "IRB video file (*.irb)"; }
	virtual void close();
	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}

	QStringList calibrations() const { return QStringList() << "Digital Level" << "Apparent T(C)"; }

protected:
	virtual bool readData(qint64 time);

private:
	bool ToTemp(unsigned short * data, int size, qint64 position, VipNDArrayType<double> & out, double * min); //temperature conversion

	IRB_HEADER 		m_header;
	qint32 			m_order;

	qint16 			m_compressed;
	quint16 		m_width;
	quint16 		m_height;
	QVector<qint32> m_datasize;
	QVector<qint32> m_data_positions;
	float  			m_lamsp;
	double 			m_pA;
	double 			m_pP;

	QVector<double> m_time;
	QVector< QVector<double> >  m_tempvals;

	VipNDArrayType<double> m_emissivity;
	VipNDArrayType<double> m_transmissivity;
};

VIP_REGISTER_QOBJECT_METATYPE(IRBLoader*)




#include "VipPlayer.h"

class CustomizeIRBVideoPlayer : public QObject
{
	Q_OBJECT
	QPointer<IRBLoader> m_device;
	QPointer<VipVideoPlayer> m_player;

public:
	CustomizeIRBVideoPlayer(VipVideoPlayer * player, IRBLoader * device);

public Q_SLOTS:
	void enableCalibration(bool);
};