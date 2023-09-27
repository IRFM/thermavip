#include <stdexcept>
#include <fstream>
#include "VideoDecoder.h"
#include <qfile.h>

static int ReadFunc(void* ptr, uint8_t* buf, int buf_size)
{
	QIODevice* pStream = reinterpret_cast<QIODevice*>(ptr);
	return pStream->read((char*)buf, buf_size);
}

// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE
int64_t SeekFunc(void* ptr, int64_t pos, int whence)
{
	//int ss = SEEK_SET;
	//int sc = SEEK_CUR;
	//int SE = SEEK_END;


	QIODevice* pStream = reinterpret_cast<QIODevice*>(ptr);

	long long res;
	if (whence == AVSEEK_SIZE) {
		res= pStream->size();
	}
	else if (whence == SEEK_SET) {
		pStream->seek(pos);
		res= pStream->pos();
	}
	else if (whence == SEEK_CUR) {
		pStream->seek(pos + pStream->pos());
		res= pStream->pos();
	}
	else {
		pStream->seek(pStream->size() - pos);
		res= pStream->pos();
	}
	return res;
}



int init_libavcodec()
{
	av_register_all();
	avcodec_register_all();
	avdevice_register_all();
	avfilter_register_all();
	avformat_network_init();
	//avcodec_init ();
	return 0;
}
static int init = init_libavcodec();

//static AVPacket flush_pkt = {0,0,(uint8_t*)"FLUSH",0};

static void destruct( AVPacket * pkt)
{
	pkt->data=NULL;
	pkt->size=0;
}

static void init_packet( AVPacket * pkt)
{
	pkt->pts   = 0;
    pkt->dts   = 0;
    pkt->pos   = -1;
    pkt->duration = 0;
    pkt->flags = 0;
    pkt->stream_index = 0;
}

#include <qfile.h>

static QString _stderr;

void log_to_array(void * 	,
	int 	,
	const char * 	fmt,
	va_list 	vl
)
{
	_stderr += QString::vasprintf(fmt, vl);
}

QStringList VideoDecoder::list_devices()
{
	//freopen("tmp.txt", "w", stderr);
	_stderr.clear();
	av_log_set_callback(log_to_array);

	AVFormatContext *formatC = avformat_alloc_context();
	AVDictionary* options = NULL;
	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat *iformat = av_find_input_format("dshow");
	int ret = avformat_open_input(&formatC, "video=dummy", iformat, &options);
	if (ret < 0)
		return QStringList();
	av_log_set_callback(av_log_default_callback);
	//freopen("CON", "w", stderr);

	//QFile file("tmp.txt");
	//file.open(QFile::ReadOnly | QFile::Text);
	QString ar = _stderr;// file.readAll();

	QStringList res;
	QStringList lines = ar.split("\n", QString::SkipEmptyParts);

	for (int i = 1; i < lines.size(); ++i)
	{
		if (lines[i].contains("audio devices"))
			break;

		if (lines[i].contains("Alternative name"))
			continue;

		int index1 = lines[i].indexOf('"');
		int index2 = lines[i].indexOf('"',index1+1);
		if (index1 >= 0)
		{
			index1++;
			QString name = lines[i].mid(index1, index2-index1);
			res << name;
		}
	}

	return res;
}


void VideoDecoder::free_packet()
{
 if(packet.data != NULL && packet.size > 0)
	 //if(strcmp((const char*)packet.data,"")!=0)
		av_free(packet.data);

 packet.size = 0;
 packet.data = 0;
 init_packet(&packet);
}

VideoDecoder::VideoDecoder()
{
	m_last_dts = 0;
	m_file_open = false;
	m_is_packet = false;
	pFormatCtx = NULL;
	pCodecCtx = NULL;
	pCodec = NULL;
	pFrame = NULL;
	pFrameRGB = NULL;
	buffer = NULL;
	pSWSCtx = NULL;

}

VideoDecoder::VideoDecoder(const std::string & name)
{
	m_last_dts = 0;
	pFormatCtx = NULL;
	pCodecCtx = NULL;
	pCodec = NULL;
	pFrame = NULL;
	pFrameRGB = NULL;
	buffer = NULL;
	pSWSCtx = NULL;
	m_is_packet = false;

	Open(name);

}


VideoDecoder::~VideoDecoder()
{
	Close();
}

void VideoDecoder::Open(const QString & name, const QString & format, const QMap<QString, QString> & options)
{
	AVDictionary* opt = NULL;
	for (QMap<QString, QString>::const_iterator it = options.begin(); it != options.end(); ++it)
	{
		av_dict_set(&opt, it.key().toLatin1().data(), it.value().toLatin1().data(), 0);
	}
	//av_dict_set(&options, "video_size", "640x480", 0);
	//av_dict_set(&options, "r", "25", 0);

	AVInputFormat *iformat = av_find_input_format(format.toLatin1().data());
	AVDictionary **iopt = opt ? &opt : NULL;

	Open(name.toLatin1().data(), iformat, iopt);
}

void VideoDecoder::Open(const std::string & name, AVInputFormat * iformat , AVDictionary ** options)
{
	/*AVFormatContext *formatC = NULL;// avformat_alloc_context();
	AVDictionary* options = NULL;
	// set input resolution
	av_dict_set(&options, "video_size", "640x480", 0);
	av_dict_set(&options, "r", "25", 0);

	AVInputFormat *iformat = av_find_input_format("dshow");
	int ret = avformat_open_input(&formatC, "video=Lenovo EasyCamera", iformat, &options);
	char msg[1000]; memset(msg, 0, 1000);
	av_strerror(ret,msg,1000);*/

	/*unsigned char * buffer = (unsigned char *)av_malloc(6000000);
	QFile * fin = new QFile(name.c_str());
	fin->open(QFile::ReadOnly);
	AVIOContext* pIOCtx = avio_alloc_context(buffer, 6000000,  // internal Buffer and its size
		0,                  // bWriteable (1=true,0=false) 
		fin,          // user data ; will be passed to our callback functions
		ReadFunc,
		0,                  // Write callback function (not used in this example) 
		SeekFunc);
	pFormatCtx = (AVFormatContext*)avformat_alloc_context();//av_malloc(sizeof(AVFormatContext));
	pFormatCtx->pb = pIOCtx;*/


	m_file_open = true;
	AVDictionary* opts = NULL;
	if (options)
		opts = *options;
	if (name.find(".sdp") != std::string::npos || name.find(".SDP") != std::string::npos || 
		name.find("udp://") != std::string::npos || name.find("rtp://") != std::string::npos ||
		name.find("rtps://") != std::string::npos || name.find("http://") != std::string::npos ||
		name.find("https://") != std::string::npos) {
		//iformat = av_find_input_format("sdp");
		av_dict_set(&opts, "protocol_whitelist", "file,udp,rtp,http,https,tcp,tls,crypto,httpproxy", 0);
	}

	init_packet(&packet);
	packet.data = NULL;
    // Open video file
#if LIBAVFORMAT_VERSION_MAJOR > 52
	int err = avformat_open_input(&pFormatCtx,
		name.c_str(),
		iformat,
		&opts);
		
#else
	int err = av_open_input_file(&pFormatCtx, name.c_str(), NULL, 0, NULL);
#endif
	if (err != 0) {
		char error[1000]; memset(error, 0, sizeof(error));
		av_strerror(err, error, sizeof(error));
		printf("ffmpeg error: %s\n", error);
		throw std::runtime_error((std::string("Couldn't open file '") + name + "'").c_str());
	}
	
    // Retrieve stream information
	//TEST
    if(avformat_find_stream_info(pFormatCtx,NULL)<0)
        throw std::runtime_error("Couldn't find stream information");

	std::vector<AVStream*> streams;
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
		streams.push_back(pFormatCtx->streams[i]);

    // Find the first video stream
    videoStream=-1;
    for(unsigned int i=0; i<pFormatCtx->nb_streams; i++)
#if LIBAVFORMAT_VERSION_MAJOR > 52
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
#else
		if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO)
#endif
        {
            videoStream=i;
            break;
        }
    if(videoStream==-1)
        throw std::runtime_error("Didn't find a video stream");

	//pFormatCtx->cur_st = pFormatCtx->streams[0];


    // Get a pointer to the codec context for the video stream
    pCodecCtx=pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL)
        throw std::runtime_error("Codec not found");

    // Open codec
	int res = avcodec_open2(pCodecCtx, pCodec,NULL);
    if(res<0)
        throw std::runtime_error("Could not open codec");

    // Allocate video frame
    pFrame= av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameRGB= av_frame_alloc();
    if(pFrameRGB==NULL)
        throw std::runtime_error("Error in avcodec_alloc_frame()");

    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
        pCodecCtx->height);

	buffer = (uint8_t*)av_malloc(numBytes);

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
        pCodecCtx->width, pCodecCtx->height);

	//Initialize Context
	if (pCodecCtx->pix_fmt == AV_PIX_FMT_NONE)
		pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pSWSCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	m_width = pCodecCtx->width;
	m_height = pCodecCtx->height;
	m_image = QImage(m_width, m_height, QImage::Format_ARGB32);

	m_fps = pFormatCtx->streams[videoStream]->r_frame_rate.num*1.0 / pFormatCtx->streams[videoStream]->r_frame_rate.den*1.0;
	m_tech = 1/m_fps;

	m_frame_pos = 0;
	m_time_pos = 0;

	m_total_time = pFormatCtx->duration/AV_TIME_BASE;
	if(m_total_time < 0.01 && !IsSequential())
		m_total_time = getTime();

	m_offset = 0;

	MoveNextFrame();

	if(!IsSequential())
		SeekTime(0);

	m_filename = name;

}


double VideoDecoder::getTime()
{
	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	int count = 0;
	while( av_read_frame( pFormatCtx, &packet ) == 0 )
	{
		if(packet.stream_index == videoStream)
			count++;
	}

	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);

	return count/m_fps;
}

bool VideoDecoder::IsSequential() const
{
	if (pFormatCtx && videoStream >= 0)
		return pFormatCtx->streams[videoStream]->duration < 0 && pFormatCtx->duration < 0;
	return false;
}

void VideoDecoder::Close()
{
	if(m_file_open)
	{


		// Free the DRGBPixel image
		if(pFrameRGB!=NULL)
		{
			av_free((AVPicture*)pFrameRGB);
		}

		// Free the YUV frame
		if(pFrame!=NULL)
		{
			av_free((AVPicture*)pFrame);
		}

		// Close the codec
		if(pCodecCtx!=NULL && pCodec != NULL) avcodec_close(pCodecCtx);

		// Close the video file
		if(pFormatCtx!=NULL) avformat_close_input(&pFormatCtx);

		if(pSWSCtx!=NULL) sws_freeContext(pSWSCtx);

		if(packet.data)
			av_free_packet(&packet);
	}



	pFormatCtx = NULL;
	pCodecCtx = NULL;
	pCodec = NULL;
	pFrame = NULL;
	pFrameRGB = NULL;
	//buffer = NULL;
	pSWSCtx = NULL;

	m_file_open = false;


	m_file_open = false;



}


const QImage &  VideoDecoder::GetCurrentFrame()
{
	return m_image;
}

int VideoDecoder::pixelType() const {
	if (pCodecCtx)
		return pCodecCtx->pix_fmt;
	return 0;
}

void VideoDecoder::toRGB( AVFrame * frame )
{
	//m_last_dts = frame->pkt_dts;
	//int pix1 = frame->data[0][0] | (frame->data[0][1] << 8);
	//int pix2 = frame->data[0][0] | (frame->data[0][1] << 8);
	if (pCodecCtx->pix_fmt == AV_PIX_FMT_GRAY16LE || pCodecCtx->pix_fmt == AV_PIX_FMT_GRAY16BE) {
		//encode 16 bits in R and G
		unsigned char r, g, b = 0;
		uint * data = (uint*)m_image.bits();
		for (int y = 0; y<m_height; y++)
			for (int x = 0; x<m_width; x++, ++data)
			{
				r = (frame->data[0] + y*frame->linesize[0])[x * 2];
				g = (frame->data[0] + y*frame->linesize[0])[x * 2 + 1];
				*data = qRgb(r, g, b);
			}
	}
	else {
		unsigned char r, g, b;
		uint * data = (uint*)m_image.bits();
		for (int y = 0; y<m_height; y++)
			for (int x = 0; x<m_width; x++, ++data)
			{
				r = (frame->data[0] + y*frame->linesize[0])[x * 3];
				g = (frame->data[0] + y*frame->linesize[0])[x * 3 + 1];
				b = (frame->data[0] + y*frame->linesize[0])[x * 3 + 2];
				//m_image.setPixel( x,y, qRgb(r,g,b) );
				*data = qRgb(r, g, b);
			}
	}
	
}


bool VideoDecoder::MoveNextFrame()
{
	try{

	if(!m_is_packet)
	{
		if(packet.data && packet.size >0)
			av_free_packet(&packet);
		//pFormatCtx->cur_pkt.destruct=NULL;
		if(av_read_frame(pFormatCtx, &packet) < 0)
			return false;
	}

	/*if(packet.stream_index == 1) {
		std::ofstream fout(("C:/Users/VM213788/Desktop/Fluke/img" + QString::number(packet.pts)).toLatin1().data(), std::ios::binary);
		fout.write((char*)packet.data, packet.size);
	}*/

	m_is_packet = false;

	while(packet.stream_index!=videoStream)
	{
		av_free_packet(&packet);
		//pFormatCtx->cur_pkt.destruct=NULL;
		if(av_read_frame(pFormatCtx, &packet) < 0)
			return false;

	}

	m_last_dts = packet.dts;

		int ffinish = 1;

		while( ffinish != 0 )
		{

#if LIBAVFORMAT_VERSION_MAJOR > 52
			int ret = avcodec_decode_video2(	pCodecCtx,
												pFrame,
												&ffinish,
												&packet );
#else
			int ret = avcodec_decode_video(pCodecCtx, pFrame, &ffinish,packet.data, packet.size);
#endif

			// Did we get a video frame?
			if(ffinish != 0)
			{
				//Convert YUV->DRGBPixel
				if(pFrame->data)
				{
					if (pCodecCtx->pix_fmt != AV_PIX_FMT_GRAY16LE && pCodecCtx->pix_fmt != AV_PIX_FMT_GRAY16BE) {
						sws_scale(pSWSCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
						toRGB(pFrameRGB);
					}
					else {
						toRGB(pFrame);
					}
				}

				break;


			}
			else
			{
				packet.stream_index = -1;
				while(packet.stream_index!=videoStream)
				{
					av_free_packet(&packet);
					//pFormatCtx->cur_pkt.destruct=NULL;
					if(av_read_frame(pFormatCtx, &packet) < 0)
						return false;

				}
			}
		}




	av_free_packet(&packet);
	m_is_packet = false;
	m_frame_pos++;
	m_time_pos = m_frame_pos*1.0 *m_tech;


	}
	catch(std::exception & )
	{
		return false;
	}

	return true;



}



double VideoDecoder::GetRate() const
{
	if(m_file_open)
		return pFormatCtx->bit_rate;
	else
		return 0;
}


void VideoDecoder::SeekTime2(double )
{

}


void VideoDecoder::SeekTime(double time)
{
	
	//pFormatCtx->cur_pkt.destruct=NULL;
	if( time < 0) time = 0;

	if (false)
	{
		
			int frameIndex = time * 25;
			// Seek is done on packet dts
			int64_t target_dts_usecs = (int64_t)round(frameIndex * (double)pFormatCtx->streams[videoStream]->r_frame_rate.den / pFormatCtx->streams[videoStream]->r_frame_rate.num * AV_TIME_BASE);
			// Remove first dts: when non zero seek should be more accurate
			auto first_dts_usecs = pFormatCtx->streams[videoStream]->first_dts < 0 ? 0 :
				(int64_t)round(pFormatCtx->streams[videoStream]->first_dts * (double)pFormatCtx->streams[videoStream]->time_base.num / pFormatCtx->streams[videoStream]->time_base.den * AV_TIME_BASE);
			target_dts_usecs += first_dts_usecs;
			int rv = av_seek_frame(pFormatCtx, -1, target_dts_usecs, AVSEEK_FLAG_BACKWARD);
			if (rv < 0)
				return;

			avcodec_flush_buffers(pFormatCtx->streams[videoStream]->codec);		

	}
	else
	{
		double seek_target = (time)* AV_TIME_BASE;

		if (time == 0)
		{
			int ret = av_seek_frame(pFormatCtx, videoStream, seek_target, AVSEEK_FLAG_BACKWARD);
			avcodec_flush_buffers(pFormatCtx->streams[videoStream]->codec);
			return;
		}



		AVRational r; r.num = 1; r.den = AV_TIME_BASE;
		seek_target = av_rescale_q(seek_target, r, pFormatCtx->streams[videoStream]->time_base);

		//pFormatCtx->cur_pkt.destruct=NULL;
		int ret = av_seek_frame(pFormatCtx, videoStream, seek_target, AVSEEK_FLAG_BACKWARD);
		avcodec_flush_buffers(pFormatCtx->streams[videoStream]->codec);
	}
	double TARGET_PTS = (time) * AV_TIME_BASE -m_tech;

	//pCodecCtx->hurry_up = 1;
	double MyPts = 0;
	m_is_packet = false;

	do {

	//pFormatCtx->cur_pkt.destruct=NULL;
	int test = av_read_frame( pFormatCtx, &packet );
	frameFinished = 1;

	if(test < 0)
		break;

	if( packet.stream_index == videoStream )
	{

#if LIBAVFORMAT_VERSION_MAJOR > 52
			int ret = avcodec_decode_video2(	pCodecCtx,
												pFrame,
												&frameFinished,
												&packet );
#else
			int ret = avcodec_decode_video(pCodecCtx, pFrame, &frameFinished,packet.data, packet.size);
#endif


		if( frameFinished )
		{
			if(packet.pts < 0.0) packet.pts = packet.dts;

				MyPts = av_rescale(packet.pts,
				AV_TIME_BASE * (int64_t) pFormatCtx->streams[videoStream]->time_base.num,
				pFormatCtx->streams[videoStream]->time_base.den );

			// Once we pass the target point, break from the loop
			if( MyPts >= TARGET_PTS)
			{
				m_is_packet = true;
				break;
			}
		}

	}
	av_free_packet(&packet);



	} while(1);
	//pCodecCtx->hurry_up = 0;

	m_time_pos = time;
	m_frame_pos = time*m_fps;

}


const QImage &  VideoDecoder::GetFrameByTime( double time )
{
	
	/*SeekTime(time);
	int number = static_cast<int>(floor( time*m_fps + 0.5));
	if(number == m_frame_pos)
		return m_image;

	MoveNextFrame();
	return m_image;*/
	int number = static_cast<int>(floor(time*m_fps + 0.5));
	return GetFrameByNumber(number);
}

const QImage &  VideoDecoder::GetFrameByNumber( int number )
{
	if(number+1 == m_frame_pos)
		return m_image;

	if(number != m_frame_pos)
		SeekTime(number/m_fps);

	MoveNextFrame();

	return m_image;

}

void VideoDecoder::SeekFrame(int pos)
{

	if(pos == m_frame_pos) return;

	if( /*(pos-m_frame_pos) > 100 ||*/ pos<m_frame_pos)
	{
		double TARGET_PTS = 0;//(pos/m_fps) * AV_TIME_BASE;
		av_seek_frame( pFormatCtx, videoStream, TARGET_PTS, AVSEEK_FLAG_BACKWARD );

		m_frame_pos = 0;
	}


	while( ++m_frame_pos < pos)
	{
		if( av_read_frame(pFormatCtx, &packet) < 0 )
			return;

		if(packet.stream_index != videoStream) m_frame_pos--;

		av_free_packet(&packet);

	}

	m_frame_pos--;
	m_time_pos =  m_frame_pos*1.0 / m_fps;

}


double VideoDecoder::GetTotalTime() const
{
	return m_total_time;
}


int VideoDecoder::GetWidth()const{return m_width;}

int VideoDecoder::GetHeight()const{return m_height;}

std::string VideoDecoder::GetFileName() const {return m_filename;}

double VideoDecoder::GetTimePos()const{return m_time_pos;}

long int VideoDecoder::GetCurrentFramePos()const{return m_frame_pos;}

double VideoDecoder::GetFps()const{return m_fps;}



