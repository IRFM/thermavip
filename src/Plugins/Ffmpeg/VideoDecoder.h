#ifndef VIDEODECODER_H
#define VIDEODECODER_H



#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C" {
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <sstream>
#include <cmath>

#include <QImage>
#include <QColor>
#include <QString>
#include <qmap.h>

/*
struct MediaStream
{
	boost::shared_ptr<AVFormatContext> pFormatCtx;
	AVCodecContext  *pCodecCtx;
	AVCodec         *pCodec;
	int				streamIndex;

	virtual ~MediaStream();
	virtual bool Open();
	virtual void Close();
	virtual bool Seek(double time);
	int Type();
};
*/

class VideoDecoder
{
      public:

		  static QStringList list_devices();

             VideoDecoder();
             VideoDecoder(const std::string & name);
			 ~VideoDecoder();
             void Open(const std::string & name, AVInputFormat * format = NULL, AVDictionary ** options = NULL);
			 void Open(const QString & name, const QString & format, const QMap<QString, QString> & options);
             void Close();

			 bool IsSequential() const;

			 //Ffmpeg main struct
			 AVFormatContext* GetContext() const {return pFormatCtx;}

			 //renvoie l'image actuelle
			 const QImage & GetCurrentFrame();

			 const QImage & GetFrameByTime( double time );
			 const QImage & GetFrameByNumber( int num );

			 //se deplace a l'image suivante, renvoie false s'il n'y en a pas
			 bool MoveNextFrame();

             //position actuelle (en s), peut etre approximatif
			 double GetTimePos() const;

			 //position actuelle (en frame), peut etre approximatif
			 long int GetCurrentFramePos()const;

             //duree totale (en s)
			 double GetTotalTime()const;

             //largeur
			 int GetWidth()const;

			 //hauteur
             int GetHeight()const;

			 //frame per second
             double GetFps()const;

			 //echantillonage (octets/s)
             double GetRate()const;

			 //eventuel offset si le film a ete enregistre avec wolff
			 double GetOffset() const {return m_offset;}

			 //nom du fichier ouvert
             std::string GetFileName() const;

			 //pixel type as ffmpeg enum AVPixelFormat
			 int pixelType() const;

			 uint64_t LastReadDTS() const { return m_last_dts; }

             //se deplace au temps donne (en s), peut etre approximatif
             void SeekTime(double time);
			 void SeekTime2(double time);
			 void SeekFrame(int pos);

      protected:

			 bool computeNextFrame();
			 double getTime();
			 void toRGB( AVFrame * frame );
			 void free_packet();

			 uint64_t		m_last_dts;
			 QImage			m_image;
			 double			m_current_time;
			 std::string	m_filename;
			 int			m_width;
			 int			m_height;
			 double			m_fps;
			 double			m_tech;
			 long int		m_frame_pos;
			 double			m_time_pos;
			 double			m_offset;
			 double			m_total_time;
			 bool			m_file_open;
			 bool			m_is_packet;

			 //variables ffmpeg
			AVFormatContext *pFormatCtx;
			int             videoStream;
			AVCodecContext  *pCodecCtx;
			AVCodec         *pCodec;
			AVFrame         *pFrame;
			AVFrame         *pFrameRGB;
			AVPacket        packet;
			int             frameFinished;
			int             numBytes;
			uint8_t			*buffer;
			SwsContext		*pSWSCtx;


};



#endif
