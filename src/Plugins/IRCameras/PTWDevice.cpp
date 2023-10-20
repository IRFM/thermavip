
// Libraries includes
#include <QFileInfo>
#include <QList>
#include <QFile>

#include "PTWDevice.h"
#include "VipLogging.h"

#pragma pack(1)
typedef struct
{
    char            Signature[5];       /* 0d000 */
    char            Version[5];         /* 0d005 */
    char            FinDeFichier;       /* 0d010 */
    quint32 TailleHeaderFilm;   /* 0d011 */
    quint32 TailleHeaderFrame;  /* 0d015 */
    quint32 TailleBloc;         /* 0d019 */
    quint32 TailleTrame;        /* 0d023 */
    quint32 NombreTrame;        /* 0d027 */
    quint32 NumeroTrame;        /* 0d031 */
    quint16 Annee;              /* 0d035 */
    char            Jour;               /* 0d037 */
    char            Mois;               /* 0d038 */
    char            Minute;             /* 0d039 */
    char            Heure;              /* 0d040 */
    char            Centieme;           /* 0d041 */
    char            Seconde;            /* 0d042 */
    char            Millieme;           /* 0d043 */
    char            Camera[20];         /* 0d044 */
    char            Lens[20];           /* 0d064 */
    char            Filter[20];         /* 0d084 */
    char            Aperture[20];       /* Od104 */
    char            UnusedStuff[253];   /* 0d124 */
    quint16 NombreColonne;      /* 0d377 */
    quint16 NombreLigne;        /* 0d379 */
} T_PTW_FILE_HEADER;


typedef struct
{
    char            Reserve0[80];
    char            Minute;
    char            Heure;
    char            Centieme;
    char            Seconde;
    char            Reserve1[76];
    char            Millieme;
    quint16 Millionieme;
    char            UnusedStuff[115];   /* 0d163 */
    quint16 bWnd;               /* 0d278 */
    qint16  nWndLeft;
	qint16  nWndTop;
	qint16  nWndWidth;
	qint16  nWndHeight;
} T_PTW_FRAME_HEADER;
#pragma pack()




PTWDevice::PTWDevice()
	:VipTimeRangeBasedGenerator()
{
	outputAt(0)->setData(VipNDArray());
}


PTWDevice::~PTWDevice()
{
	close();
}

void PTWDevice::close()
{
	VipIODevice::close();
}


/**
 *
 *
 */
bool PTWDevice::open(VipIODevice::OpenModes mode)
{
	if(! (mode & VipIODevice::ReadOnly))
		return false;

	QIODevice * d = createDevice(this->removePrefix(path()), QIODevice::ReadOnly);
	if (!d)
		return false;

    std::vector<QString>    v_SubString;

    // Save signalString
    m_s_OpenString = this->removePrefix(path());

    // get length of file:
    int MovieSize = d->size();
    if( MovieSize <= 0 )
    {
        VIP_LOG_ERROR("Error reading file : " + m_s_OpenString + " size" );
        return false;
    }
    else
    {
        m_ui32_MovieSize = MovieSize;
    }

    // Allocate space for file header
    T_PTW_FILE_HEADER   PTWFileHeader;
    // Read file header
    d->seek( 0 );
	if (d->read(reinterpret_cast<char*>(&PTWFileHeader), sizeof(T_PTW_FILE_HEADER)) != sizeof(T_PTW_FILE_HEADER))
		return false;
    //
    m_width = PTWFileHeader.NombreColonne;
    m_height = PTWFileHeader.NombreLigne;
    m_FileHeaderSize = PTWFileHeader.TailleHeaderFilm;
    m_FrameHeaderSize = PTWFileHeader.TailleHeaderFrame;
    m_ui32_NbFrames = PTWFileHeader.NombreTrame;
    m_bytePerPixel = 2;

	T_PTW_FILE_HEADER * ptPTWFileHeader = &PTWFileHeader;

    vip_debug( "\nSignature : %s\n", ptPTWFileHeader->Signature );
    vip_debug( "Version : %s\n", ptPTWFileHeader->Version );
    vip_debug( "FinDeFichier : %c\n", ptPTWFileHeader->FinDeFichier );
    vip_debug( "TailleHeaderFilm : %d\n", ptPTWFileHeader->TailleHeaderFilm );
    vip_debug( "TailleHeaderFrame : %d\n", ptPTWFileHeader->TailleHeaderFrame );
    vip_debug( "TailleBloc : %d\n", ptPTWFileHeader->TailleBloc );
    vip_debug( "TailleTrame : %d\n", ptPTWFileHeader->TailleTrame );
    vip_debug( "NombreTrame : %d\n", ptPTWFileHeader->NombreTrame );
    vip_debug( "NumeroTrame : %d\n", ptPTWFileHeader->NumeroTrame );
    vip_debug( "Annee : %hd\n", ptPTWFileHeader->Annee );
    vip_debug( "Jour : %hhd\n", ptPTWFileHeader->Jour );
    vip_debug( "Mois : %hhd\n", ptPTWFileHeader->Mois );
    vip_debug( "Minute : %hhd\n", ptPTWFileHeader->Minute );
    vip_debug( "Heure : %hhd\n", ptPTWFileHeader->Heure );
    vip_debug( "Centieme : %hhd\n", ptPTWFileHeader->Centieme );
    vip_debug( "Seconde : %hhd\n", ptPTWFileHeader->Seconde );
    vip_debug( "Millieme : %hhd\n", ptPTWFileHeader->Millieme );
    vip_debug( "Camera : %s\n", ptPTWFileHeader->Camera );
    vip_debug( "Lens : %s\n", ptPTWFileHeader->Lens );
    vip_debug( "Filter : %s\n", ptPTWFileHeader->Filter );
    vip_debug( "Aperture : %s\n", ptPTWFileHeader->Aperture );
    vip_debug( "NombreColonne : %hu\n", ptPTWFileHeader->NombreColonne );
    vip_debug( "NombreLigne : %hu\n", ptPTWFileHeader->NombreLigne );


    // Allocate space for frame header
    T_PTW_FRAME_HEADER   PTWFrameHeader;

    // Resize the vector of date to fit the number of frames of the video
	QVector<qint64> times;
	times.resize(m_ui32_NbFrames);
    // Save time vector values for the current loader
    for( unsigned int i=0; i<m_ui32_NbFrames; i++ )
    {
        // Read each frame header
       // d->seek(  PTWFileHeader.TailleHeaderFilm + i * (PTWFileHeader.TailleHeaderFrame + m_width * m_height * m_bytePerPixel ) + sizeof(PTWFrameHeader.Reserve0) );
        //d->read(   reinterpret_cast<char*>(&PTWFrameHeader.Minute),
        //                83 /*depuis Minute(inclus) jusqu'a Millionieme(inclus)*/ );
        d->seek(PTWFileHeader.TailleHeaderFilm + i * (PTWFileHeader.TailleHeaderFrame + m_width * m_height * m_bytePerPixel) );
        d->read(reinterpret_cast<char*>(&PTWFrameHeader),
                            sizeof(PTWFrameHeader));

        // Save the timestamp
		times[i] =   (PTWFrameHeader.Seconde +
                                PTWFrameHeader.Centieme / 100. +
                                PTWFrameHeader.Millieme / 1000. +
                                PTWFrameHeader.Millionieme / 1000000.) * 1000000000.;
    }

	setTimestamps(times);
	setAttribute("Date", QString::number(PTWFileHeader.Annee) + " " + QString::number((short)(PTWFileHeader.Mois)) + " " + QString::number((short)(PTWFileHeader.Jour)));
	setAttribute("Camera", QString(PTWFileHeader.Camera));
	setAttribute("Lens", QString(PTWFileHeader.Lens));
	setAttribute("Filter", QString(PTWFileHeader.Filter));
	setAttribute("Aperture", QString(PTWFileHeader.Aperture));
	setAttribute("UnusedStuff", QString(PTWFileHeader.UnusedStuff));

	readData(computePosToTime(0));

    this->setOpenMode(mode);
    return true;
}



bool PTWDevice::readData(qint64 time)
{
    // Check input arguments
	qint64 pos = computeTimeToPos(time);
	if (pos >= size()) pos = size() - 1;
	if (pos < 0) pos = 0;
    
	VipNDArray ar;
	if (m_bytePerPixel == 0)
		return false;
	else if (m_bytePerPixel == 1)
		ar = VipNDArray(QMetaType::UChar, vipVector(m_height, m_width));
	else if (m_bytePerPixel == 2)
		ar = VipNDArray(QMetaType::UShort, vipVector(m_height, m_width));
	else if (m_bytePerPixel == 4)
		ar = VipNDArray(QMetaType::UInt, vipVector(m_height, m_width));
	else
		return false;
	

	qint64 offset = (m_FileHeaderSize + m_FrameHeaderSize + pos * (m_width * m_height * m_bytePerPixel + m_FrameHeaderSize));

	// Set the read cursor before the frame
	device()->seek(offset);
	// Read image
	device()->read((char*)ar.data(),m_width * m_height * m_bytePerPixel);
    
	VipAnyData out = create(QVariant::fromValue(ar));
	out.setTime(time);
	//out.setZUnit(calibrations()[propertyAt(0)->value<bool>()]);
	outputAt(0)->setData(out);
    return true;
}

