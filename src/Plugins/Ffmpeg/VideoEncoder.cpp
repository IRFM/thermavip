#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <list>

#include <QFileInfo>

#include "VideoEncoder.h"


std::list<int> get_video_codec(std::string ext, int /*audio_codec*/)
{
	//test<int,alloc_frame> *r = new test<int,alloc_frame>();

	std::list<int> res;

	AVOutputFormat * temp= av_oformat_next(nullptr);
	while(temp != nullptr )
	{
		if( temp->extensions != nullptr && /*temp->audio_codec == audio_codec  && */(std::string(temp->extensions).find(ext.c_str()) != std::string::npos ))
		{
			res.push_back(temp->video_codec);

		}

		temp = temp->next;
	}
	return res;
}

VideoEncoder::VideoEncoder()
{
    ;
	init();
	m_file_open = false;
	m_filename = "";
	m_width = 400;
	m_height = 400;
	m_fps = 25;
	m_frame_pos = 0;
	m_time_pos = 0;
	m_frame_rate = 20000000;
}

VideoEncoder::VideoEncoder(const std::string & name, int width, int height, double fps, double rate, int codec_id)
{
	init();
	Open(name,width,height,fps,rate,codec_id);
}


VideoEncoder::~VideoEncoder()
{
	Close();
}

void VideoEncoder::init()
{
	fmt = nullptr;
	oc = nullptr;
	video_str = nullptr;
	picture = nullptr;
	tmp_picture = nullptr;
	rgb8_picture = nullptr;
	img_convert_context = nullptr;
	additional_GIF_context = nullptr;
	video_outbuf = nullptr;
	vc = nullptr;
}



void VideoEncoder::Open(const std::string & name, int width, int height, double fps, double rate, int codec_id)
{
	Close();

	m_width = width;
	m_height = height;
	m_fps = fps;
	m_frame_pos = 0;
	m_time_pos = 0;
	m_frame_rate = rate;

	m_total_frame = 0;
	m_total_time = 0;
	m_file_open = true;

	m_filename = name;


	const char* filename = name.c_str();

    AVPixelFormat dest_pxl_fmt = AV_PIX_FMT_YUV420P;
    AVPixelFormat src_pxl_fmt  = AV_PIX_FMT_RGB24;

    int sws_flags = SWS_FAST_BILINEAR;//interpolation method (keeping same size images for now)



    fmt = nullptr;
#if LIBAVFORMAT_VERSION_MAJOR > 52
    fmt = av_guess_format(nullptr,filename,nullptr);
#else
	fmt = guess_format(nullptr,filename,nullptr);
#endif

	if (!fmt) {
		//unable to retrieve output format
		Close();
		throw std::runtime_error("Could not determine format from filename");
	}

#ifdef ENABLE_H264
	if (fmt->video_codec == AV_CODEC_ID_H264)
	{
		//we need even width and height
		if(width % 2 ) 
			m_width = ++width;
		if(height % 2)
			m_height = ++height;

		vc = Init(filename, width, height, fps, rate );
		if (!vc) {
			throw std::runtime_error("Could not determine format from filename");
		}
		return;
	}
#endif

    if (!fmt){
	//default format "mpeg"
#if LIBAVFORMAT_VERSION_MAJOR > 52
		fmt = av_guess_format("mpeg",nullptr,nullptr);
#else
		fmt = guess_format("mpeg",nullptr,nullptr);
#endif

    }
    if (!fmt){
	//unable to retrieve output format
		Close();
        throw std::runtime_error("Could not determine format from filename");
    }

	if( codec_id != -1)
	{
		std::string ext = QFileInfo(QString(filename)).suffix().toStdString();
		AVOutputFormat * temp= av_oformat_next(nullptr);
		while(temp != nullptr )
		{
			if( temp->video_codec == codec_id /*&& temp->audio_codec == CODEC_ID_NONE*/  && (std::string(temp->extensions).find(ext.c_str()) != std::string::npos || codec_id == AV_CODEC_ID_RAWVIDEO) )
			{
				fmt = temp;
				//find = true;
				break;
			}
			else
				temp = temp->next;
		}

		if(temp == nullptr)
		{
			Close(true);
			throw std::runtime_error("Wrong extention for this video codec");
		}
	}

    oc = avformat_alloc_context();
    if (!oc){
	//unable to allocate format context
		Close(true);
        throw std::runtime_error("Mem allocation error for format context");
    }

	

    AVCodec *codec = nullptr;

    oc->oformat = fmt;
    strcpy(oc->filename,filename);


    //add video stream
    int stream_index = 0;
    video_str = nullptr;
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
	video_str = avformat_new_stream(oc,nullptr);
	video_str->id = stream_index;
	if (!video_str){
	    //no stream allocated
        Close(true);
	    throw std::runtime_error("Unable to create new video stream");
	}
    } else {
        Close(true);
		throw std::runtime_error("No codec identified");
    }

    AVCodecContext *c = video_str->codec;
    c->codec_id = fmt->video_codec;
#if LIBAVFORMAT_VERSION_MAJOR > 52
    c->codec_type = AVMEDIA_TYPE_VIDEO;
#else
	c->codec_type = CODEC_TYPE_VIDEO;
#endif
    c->bit_rate = m_frame_rate;
    c->width = width;
    c->height = height;
    c->time_base.num = 1;
    c->time_base.den = m_fps;
    c->gop_size = 12;
    c->pix_fmt = dest_pxl_fmt;

	//TEST GIF
	if (c->codec_id == AV_CODEC_ID_GIF)
	{
		c->pix_fmt = AV_PIX_FMT_RGB8;
	}

    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
	c->max_b_frames = 2;
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        c->mb_decision = 2;

    /*if (av_set_parameters(oc,nullptr) < 0){
        //parameters not properly set
        Close(true);
        throw std::runtime_error("Parameters for avcodec not properly set");
    }*/


    //open codecs and alloc buffers
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec){
	//unable to find codec
        Close(true);
        throw std::runtime_error("No codec found");
    }

	/*if (c->codec_id == AV_CODEC_ID_H264)
	{
		//default settings for x264
		c->me_range = 16;
		c->max_qdiff = 4;
		c->qmin = 10;
		c->qmax = 51;
		c->qcompress = 0.6;

		video_str->codecpar->codec_id = AV_CODEC_ID_H264;
		video_str->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
		video_str->codecpar->width = width;
		video_str->codecpar->height = height;
		video_str->codecpar->format = AV_PIX_FMT_YUV420P;
		video_str->codecpar->bit_rate = rate ;
		video_str->time_base = { 1, (int)fps };
		c->pix_fmt = AV_PIX_FMT_YUV420P;

		c->time_base = { 1, (int)fps };
		c->max_b_frames = 2;
		c->gop_size = (int)fps;
		int r = av_opt_set(c, "preset", "ultrafast", 0);
		
		if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
		avcodec_parameters_from_context(video_str->codecpar, c);
	}*/


	if (avcodec_open2(c,codec,nullptr) < 0){
        //fail to open codec
		video_str->codec = nullptr;
		Close(true);
		throw std::runtime_error("Unable to open codec");
	}

	

	img_convert_context = nullptr;
    img_convert_context = sws_getContext(c->width,c->height,src_pxl_fmt,
                                         c->width,c->height, AV_PIX_FMT_YUV420P,sws_flags,nullptr,nullptr,nullptr);
	additional_GIF_context = nullptr;
	if (c->codec_id == AV_CODEC_ID_GIF)
	{
		//GIF only, we need to convert from YUV420 to RGB8

		additional_GIF_context = sws_getContext(c->width, c->height, AV_PIX_FMT_YUV420P,
			c->width, c->height, c->pix_fmt, sws_flags, nullptr, nullptr, nullptr);

		rgb8_picture = alloc_picture(c->width, c->height, c->pix_fmt);
		if (!rgb8_picture) {
			Close(true);
			throw std::runtime_error("Could not allocate picture\n");
		}
	}

	//if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) 
	{
		/* allocate output buffer */
		/* XXX: API change will be done */
		/* buffers passed into lav* can be allocated any way you prefer,
		   as long as they're aligned enough for the architecture, and
		   they're freed appropriately (such as using av_free for buffers
		   allocated with av_malloc) */
		video_outbuf_size = 2000000;
		video_outbuf = (uint8_t*) av_malloc(video_outbuf_size);
	}

	picture = alloc_picture(c->width, c->height, dest_pxl_fmt);
	if (!picture) {
		Close(true);
		throw std::runtime_error("Could not allocate picture\n");
	}

	tmp_picture = alloc_picture(c->width, c->height,src_pxl_fmt);
	if (!picture) {
		Close(true);
		throw std::runtime_error("Could not allocate picture\n");
	}





    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE))
        if (avio_open(&oc->pb, m_filename.c_str(), AVIO_FLAG_WRITE) < 0)
		{
			Close(true);
			throw std::runtime_error("Could not open the file");
		}


    /* write the stream header, if any */
    if( avformat_write_header(oc,nullptr) != 0)
		throw std::runtime_error("Unable to write header");;

}


//creer un AVFrame valide de dimension width, height, en RGB
AVFrame *VideoEncoder::alloc_picture(int width, int height, enum AVPixelFormat pix)
{
    AVFrame *pict;
    uint8_t *picture_buf;
    int size;

    pict = av_frame_alloc();
    if (!pict)
        return nullptr;
    size = avpicture_get_size(pix, width, height);
    picture_buf = (uint8_t*) av_malloc(size);
    if (!picture_buf) {
        av_free(pict);
        return nullptr;
    }
    avpicture_fill((AVPicture *)pict, picture_buf,
                   pix, width, height);
	pict->width = width;
	pict->height = height;
	pict->format = pix;
    return pict;
}



void VideoEncoder::Close(bool debug)
{

	m_file_open = false;

#ifdef ENABLE_H264
	if (vc) {
		Finish(vc);
		vc = nullptr;
		return;
	}
#endif


    if (video_str)
	{
		if(!debug)
			avcodec_close(video_str->codec);
		video_str = nullptr;
	}

   if(picture)
   {
	    av_free(picture->data[0]);
		av_free(picture);
		picture = nullptr;
   }

   if(tmp_picture)
   {
	    av_free(tmp_picture->data[0]);
		av_free(tmp_picture);
		tmp_picture = nullptr;
   }

   if (rgb8_picture)
   {
	   av_free(rgb8_picture->data[0]);
	   av_free(rgb8_picture);
	   rgb8_picture = nullptr;
   }

   if(video_outbuf)
   {
	   av_free(video_outbuf);
	   video_outbuf = nullptr;
   }

   if (img_convert_context)
   {
	   sws_freeContext(img_convert_context);
	   img_convert_context = nullptr;
   }

   if (additional_GIF_context)
   {
	   sws_freeContext(additional_GIF_context);
	   additional_GIF_context = nullptr;
   }

   if(oc)
   {
	   if(!debug)
		   av_write_trailer(oc);

	   for(unsigned int i = 0; i < oc->nb_streams; i++)
	   {
			av_freep(oc->streams[i]);
	   }

	   if (!(fmt->flags & AVFMT_NOFILE))
			if(!debug) avio_close(oc->pb);

	   //av_free(oc);
//	   av_close_input_file(oc);
	   if (oc)
	   {
		   avformat_free_context(oc);
		   oc = nullptr;
	   }
	   
   }

}


qint64 VideoEncoder::fileSize() const
{
	if (vc) {
		return QFileInfo(vc->TmpName().c_str()).size();
	}
	else {
		return QFileInfo(m_filename.c_str()).size();
	}
}

bool VideoEncoder::AddFrame(const QImage & im)
{
#ifdef ENABLE_H264
	if (vc) {
		if (!::AddFrame(im, vc))
			throw std::runtime_error("Error while writing video frame");
		return true;
	}
#endif

	if (!video_str)
		return false;

	QImage image = im;
	if(image.width() != m_width || image.height() != m_height)
		image = image.scaled(m_width,m_height,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

	m_total_frame++;
	//m_total_frame += 1/m_fps;
	m_frame_pos++;
	m_time_pos += 1/m_fps;

	AVCodecContext *c = video_str->codec;

	AVFrame * frame = convert(image);
	frame->pts = (m_frame_pos-1) * (1000.0 / (float)(m_fps));

	/*if (c->codec_id == AV_CODEC_ID_H264)
	{
		int err;
		if ((err = avcodec_send_frame(c, frame)) < 0) {
			Close();
			throw std::runtime_error("Error while writing video frame");
		}

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = nullptr;
		pkt.size = 0;

		if (avcodec_receive_packet(c, &pkt) == 0) {
			pkt.flags |= AV_PKT_FLAG_KEY;
			av_interleaved_write_frame(oc, &pkt);
			av_packet_unref(&pkt);
		}
		return true;
	}*/

	
    /* encode the image */
	int got_packet = 0;
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;    // packet data will be allocated by the encoder
	pkt.size = 0;
    int ret = avcodec_encode_video2(c, &pkt, frame,&got_packet);
    /* if zero size, it means the image was buffered */
    if (ret == 0) {
            
        pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, video_str->time_base);
        if(c->coded_frame->key_frame)
#if LIBAVFORMAT_VERSION_MAJOR > 52
            pkt.flags |= AV_PKT_FLAG_KEY;
#else
			pkt.flags |= PKT_FLAG_KEY;
#endif
        pkt.stream_index= video_str->index;
        /* write the compressed frame in the media file */
        ret = av_write_frame(oc, &pkt);
		av_free_packet(&pkt);

    } 
    
    if (ret != 0) {
		Close();
		throw std::runtime_error("Error while writing video frame");
    }

	//m_frame_pos++;

	return true;
}


//rempli picture a partir d'une QImage
AVFrame* VideoEncoder::convert(const QImage & image)
{
	uint8_t * data = tmp_picture->data[0];

	QImage temp;
	if( image.width() != m_width || image.height() != m_height )
		temp = image.scaled(m_width,m_height);
	else
		temp = image;


	int i=0;
	for(int y=0; y<m_height; y++)
	{
		QRgb * dat = reinterpret_cast<QRgb*>(temp.scanLine(y));
		for(int x=0; x<m_width; x++,i+=3)
		{
			data[i] = qRed(dat[x]);
			data[i+1] = qGreen(dat[x]);
			data[i+2] = qBlue(dat[x]);
		}
	}


	AVCodecContext *c = video_str->codec;

	if (c->codec_id == AV_CODEC_ID_GIF)
	{
		sws_scale(img_convert_context, tmp_picture->data, tmp_picture->linesize,
			0, c->height, picture->data, picture->linesize);

		sws_scale(additional_GIF_context, picture->data, picture->linesize,
			0, c->height, rgb8_picture->data, rgb8_picture->linesize);

		return rgb8_picture;
	}
	else
	{
		sws_scale(img_convert_context, tmp_picture->data, tmp_picture->linesize,
			0, c->height, picture->data, picture->linesize);

		return picture;
	}
	
}

double VideoEncoder::GetRate() const {return m_frame_rate;}

int VideoEncoder::GetWidth() const {return m_width;}

int VideoEncoder::GetHeight() const {return m_height;}

std::string VideoEncoder::GetFileName() const {return m_filename;}

long int VideoEncoder::GetCurrentFramePos() const {return m_frame_pos;}

double VideoEncoder::GetFps() const {return m_fps;}















#if defined( ENABLE_H264) && LIBAVCODEC_VERSION_MICRO >= 100 //Ffmpeg micro version starts at 100, while libav is < 100


using namespace std;

bool VideoCapture::Init(const char * filename, int width, int height, int fpsrate, double bitrate) {

	fname = filename;
	fps = fpsrate;
	tmp_name = fname + ".h264";

	int err;

	if (!(oformat = av_guess_format(nullptr, tmp_name.c_str(), nullptr))) {
		printf("Failed to define output format %i\n", 0);
		return false;
	}

	if ((err = avformat_alloc_output_context2(&ofctx, oformat, nullptr, tmp_name.c_str()) < 0)) {
		printf("Failed to allocate output context %i\n", err);
		Free();
		return false;
	}

	if (!(codec = avcodec_find_encoder(oformat->video_codec))) {
		printf("Failed to find encoder %i\n", 0);
		Free();
		return false;
	}

	if (!(videoStream = avformat_new_stream(ofctx, codec))) {
		printf("Failed to create new stream %i\n", 0);
		Free();
		return false;
	}

	if (!(cctx = avcodec_alloc_context3(codec))) {
		printf("Failed to allocate codec context %i\n", 0);
		Free();
		return false;
	}

	videoStream->time_base = { 1, fps };

	//TEST
	this->file_format = AV_PIX_FMT_YUV420P;

	videoStream->codecpar->codec_id = oformat->video_codec;
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	videoStream->codecpar->width = width;
	videoStream->codecpar->height = height;
	videoStream->codecpar->format = file_format;
	videoStream->codecpar->bit_rate = bitrate ;
	
	avcodec_parameters_to_context(cctx, videoStream->codecpar);
	cctx->time_base = { 1, fps };
	cctx->max_b_frames = 2;
	cctx->gop_size = 12;
	if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264) {
		av_opt_set(cctx->priv_data, "preset", "faster", 0);
	}
	else if (videoStream->codecpar->codec_id == AV_CODEC_ID_H265) {
		av_opt_set(cctx->priv_data, "preset", "ultrafast", AV_OPT_SEARCH_CHILDREN); /// hq ll lossless losslesshq
		av_opt_set(cctx->priv_data, "profile", "main", AV_OPT_SEARCH_CHILDREN);
		cctx->gop_size = 12;
		cctx->max_b_frames = 2;
		cctx->pix_fmt = AV_PIX_FMT_YUV420P;
		cctx->width = width;
		cctx->height = height;
	}

	if (ofctx->oformat->flags & AVFMT_GLOBALHEADER) {
		cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	avcodec_parameters_from_context(videoStream->codecpar, cctx);

	if ((err = avcodec_open2(cctx, codec, nullptr)) < 0) {
		printf("Failed to open codec %i\n", err);
		Free();
		return false;
	}

	if (!(oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&ofctx->pb, tmp_name.c_str(), AVIO_FLAG_WRITE)) < 0) {
			printf("Failed to open file %i\n", err);
			Free();
			return false;
		}
	}

	if ((err = avformat_write_header(ofctx, nullptr)) < 0) {
		printf("Failed to write header %i\n", err);
		Free();
		return false;
	}

	av_dump_format(ofctx, 0, tmp_name.c_str(), 1);
	return true;
}
#include <qdatetime.h>
bool VideoCapture::AddFrame(const QImage & image) {

	qint64 st = QDateTime::currentMSecsSinceEpoch();

	QImage temp;
	if (image.width() != cctx->width || image.height() != cctx->height)
		temp = image.scaled(cctx->width, cctx->height);
	else
		temp = image;

	if ((int)img.size() != (int)(cctx->width * cctx->height*3))
		img.resize(cctx->width * cctx->height*3);

	int i = 0;
	for (int y = 0; y < cctx->height; y++)
	{
		QRgb * dat = reinterpret_cast<QRgb*>(temp.scanLine(y));
		for (int x = 0; x < cctx->width; x++, i += 3)
		{
			img[i] = qRed(dat[x]);
			img[i + 1] = qGreen(dat[x]);
			img[i + 2] = qBlue(dat[x]);
		}
	}
	qint64 el1 = QDateTime::currentMSecsSinceEpoch() - st;
	bool res = AddFrame(img.data());
	qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st;
	printf("encode: %i, %i\n", (int)el1, (int)el2);
	return res;
}

bool VideoCapture::AddFrame(uint8_t *data) {

	int err;
	if (!videoFrame) {

		videoFrame = av_frame_alloc();
		videoFrame->format = file_format;
		videoFrame->width = cctx->width;
		videoFrame->height = cctx->height;

		if ((err = av_frame_get_buffer(videoFrame, 32)) < 0) {
			printf("Failed to allocate picture %i\n", err);
			return false;
		}
	}


	if (!swsCtx) {
		swsCtx = sws_getContext(cctx->width, cctx->height, AV_PIX_FMT_RGB24, cctx->width, cctx->height, file_format, SWS_BICUBIC, 0, 0, 0);
	}
	
	{
		int inLinesize[1] = { 3 * cctx->width };
		// From RGB to YUV
		sws_scale(swsCtx, (const uint8_t * const *)&data, inLinesize, 0, cctx->height, videoFrame->data, videoFrame->linesize);
	}

	videoFrame->pts = frameCounter++;

	if ((err = avcodec_send_frame(cctx, videoFrame)) < 0) {
		printf("Failed to send frame %i\n", err);
		return false;
	}

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;

	if (avcodec_receive_packet(cctx, &pkt) == 0) {
		pkt.flags |= AV_PKT_FLAG_KEY;
		av_interleaved_write_frame(ofctx, &pkt);
		av_packet_unref(&pkt);
	}

	return true;
}

bool VideoCapture::Finish() {
	//DELAYED FRAMES
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;

	for (;;) {
		avcodec_send_frame(cctx, nullptr);
		if (avcodec_receive_packet(cctx, &pkt) == 0) {
			av_interleaved_write_frame(ofctx, &pkt);
			av_packet_unref(&pkt);
		}
		else {
			break;
		}
	}

	av_write_trailer(ofctx);
	if (!(oformat->flags & AVFMT_NOFILE)) {
		int err = avio_close(ofctx->pb);
		if (err < 0) {
			printf("Failed to close file %i\n", err);
		}
	}

	Free();

	return Remux();
}

void VideoCapture::Free() {
	if (videoFrame) {
		av_frame_free(&videoFrame);
		videoFrame = nullptr;
	}
	if (cctx) {
		avcodec_free_context(&cctx);
	}
	if (ofctx) {
		avformat_free_context(ofctx);
		ofctx = nullptr;
	}
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
}

bool VideoCapture::Remux() {
	AVFormatContext *ifmt_ctx = nullptr, *ofmt_ctx = nullptr;
	int err;
	bool res = true;
	AVPacket videoPkt;
	int ts = 0;
	AVStream *inVideoStream = 0;
	AVStream *outVideoStream = 0;

	if ((err = avformat_open_input(&ifmt_ctx, tmp_name.c_str(), 0, 0)) < 0) {
		printf("Failed to open input file for remuxing %i\n", err);
		res = false;
		goto end;
	}
	if ((err = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf("Failed to retrieve input stream information %i\n", err);
		res = false;
		goto end;
	}

	
	
	if ((err = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, fname.c_str()))) {
		printf("Failed to allocate output context %i\n", err);
		res = false;
		goto end;
	}

	inVideoStream = ifmt_ctx->streams[0];
	outVideoStream = avformat_new_stream(ofmt_ctx, nullptr);
	if (!outVideoStream) {
		printf("Failed to allocate output video stream %i\n", 0);
		res = false;
		goto end;
	}
	outVideoStream->time_base = { 1, fps };
	avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
	outVideoStream->codecpar->codec_tag = 0;

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&ofmt_ctx->pb, fname.c_str(), AVIO_FLAG_WRITE)) < 0) {
			printf("Failed to open output file %i\n", err);
			res = false;
			goto end;
		}
	}

	if ((err = avformat_write_header(ofmt_ctx, 0)) < 0) {
		printf("Failed to write header to output file %i\n", err);
		res = false;
		goto end;
	}


	while (true) {
		if ((err = av_read_frame(ifmt_ctx, &videoPkt)) < 0) {
			break;
		}
		videoPkt.stream_index = outVideoStream->index;
		videoPkt.pts = ts;
		videoPkt.dts = ts;
		videoPkt.duration = av_rescale_q(videoPkt.duration, inVideoStream->time_base, outVideoStream->time_base);
		ts += videoPkt.duration;
		videoPkt.pos = -1;

		if ((err = av_interleaved_write_frame(ofmt_ctx, &videoPkt)) < 0) {
			printf("Failed to mux packet %i\n", err);
			av_packet_unref(&videoPkt);
			break;
		}
		av_packet_unref(&videoPkt);
	}

	av_write_trailer(ofmt_ctx);

	
end:
	if (ifmt_ctx) {
		avformat_close_input(&ifmt_ctx);
	}
	if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		avio_closep(&ofmt_ctx->pb);
	}
	if (ofmt_ctx) {
		avformat_free_context(ofmt_ctx);
	}
	if (!QFile::remove(tmp_name.c_str())) 
		res = false;
	
	return res;
}

#endif
