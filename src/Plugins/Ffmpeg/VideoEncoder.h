#ifndef VIDEO_VideoEncoder_H
#define VIDEO_VideoEncoder_H

#pragma warning(disable : 4996)

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/avfft.h>

#include <libavdevice/avdevice.h>

#include <libavfilter/avfilter.h>
//#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include <libavformat/avformat.h>
#include <libavformat/avio.h>

	// libav resample

#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/file.h>
#include <libswscale/swscale.h>

#include <math.h>
#include <string.h>
}

#include <iostream>
#include <fstream>
#include <list>


#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <string> 




class VideoCapture
{
public:

	VideoCapture() {
		oformat = NULL;
		ofctx = NULL;
		videoStream = NULL;
		videoFrame = NULL;
		swsCtx = NULL;
		frameCounter = 0;
	}

	~VideoCapture() {
		Free();
	}

	bool Init(const char * filename, int width, int height, int fpsrate, double bitrate);

	bool AddFrame(uint8_t *data);
	bool AddFrame(const QImage & image);

	bool Finish();

	std::string TmpName() const { return tmp_name; }
private:

	std::string fname;
	std::string tmp_name;

	AVOutputFormat *oformat;
	AVFormatContext *ofctx;

	AVStream *videoStream;
	AVFrame *videoFrame;

	AVCodec *codec;
	AVCodecContext *cctx;

	SwsContext *swsCtx;

	std::vector<uint8_t> img;

	int frameCounter;
	AVPixelFormat file_format;

	int fps;

	void Free();

	bool Remux();
};




inline VideoCapture* Init(const char * filename, int width, int height, int fps, double bitrate) {
	VideoCapture *vc = new VideoCapture();
	if (vc->Init(filename, width, height, fps, bitrate))
		return vc;
	return NULL;
};
inline bool AddFrame(uint8_t *data, VideoCapture *vc) {
	return vc->AddFrame(data);
}
inline bool AddFrame(const QImage & image, VideoCapture *vc) {
	return vc->AddFrame(image);
}
inline bool Finish(VideoCapture *vc) {
	bool res = vc->Finish();
	delete vc;
	return res;
}



#include <QImage>


class VideoEncoder
{
public:
        VideoEncoder();
        VideoEncoder(const std::string & name, int width, int height, double fps, double bitrate, int codec_id = -1);
		~VideoEncoder();
        void Open(const std::string & name, int width, int height, double fps, double bitrate, int codec_id = -1);
        void Close(bool debug = false);

		bool IsOpen() const { return m_file_open; }

		bool AddFrame(const QImage & image);

        //position actuelle (en s), peut etre approximatif
		double GetCurrentTimePos() const {return m_time_pos; }

		//position actuelle (en frame), peut etre approximatif
		long int GetCurrentFramePos() const;

		//temps total
		double GetTotalTime() const {return m_total_time; }

		//nb total de frame
		long int GetTotalFrame() const {return m_total_frame;}

        //largeur
		int GetWidth() const;

		//hauteur
        int GetHeight() const;

		//frame per second
        double GetFps() const;

		//echantillonage (octets/s)
        double GetRate() const;

		//nom du fichier ouvert
        std::string GetFileName() const;

        void SetSize(int width, int height)
		{
			m_width = width;
			m_height = height;
		}

		void SetFps(double fps)
		{
			m_fps = fps;
		}

		void SetRate(double bitrate)
		{
			m_frame_rate = bitrate;
		}

		qint64 fileSize() const;

protected:

		std::string	m_filename;
		int			m_width;
		int			m_height;
		double			m_fps;
		long int		m_frame_pos;
		double			m_time_pos;
		double			m_frame_rate;
		long int		m_total_frame;
		double			m_total_time;
		bool			m_file_open;

		//variables ffmpeg
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVStream *video_str;
	AVFrame *picture, *tmp_picture, *rgb8_picture;
	SwsContext *img_convert_context;
	SwsContext *additional_GIF_context;
	uint8_t *video_outbuf;
	int video_outbuf_size;

	VideoCapture  * vc; //for h264 only

	AVFrame* convert(const QImage & image);

	void init();
	AVFrame *alloc_picture(int width, int height, enum AVPixelFormat pix);


};








#endif
