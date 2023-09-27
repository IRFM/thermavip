#include "MPEGSaver.h"
#include "VipLogging.h"
#include "VideoEncoder.h"

MPEGSaver::MPEGSaver(QObject * parent)
	:VipIODevice(parent)
{
	m_encoder = new VideoEncoder();
}

MPEGSaver::~MPEGSaver()
{
	close();
	delete m_encoder;
}

qint32 MPEGSaver::fullFrameWidth() const { return m_encoder->GetWidth(); }
qint32 MPEGSaver::fullFrameHeight() const { return m_encoder->GetHeight(); }

bool MPEGSaver::open(VipIODevice::OpenModes mode)
{
	if (mode & VipIODevice::ReadOnly)
		return false;

	if(this->isOpen())
		this->close();

	try{
		
		this->setOpenMode(mode);
		this->setSize(0);
		return true;

	}
	catch(const std::exception & e)
	{
		setError(e.what());
		return false;
	}

	return false;
}

void MPEGSaver::close()
{
	m_encoder->Close();
	setOpenMode(NotOpen);
}

void MPEGSaver::apply()
{
	VipAnyData in = inputAt(0)->data();
	VipNDArray ar = in.data().value<VipNDArray>();
	if (ar.isEmpty())
	{
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	QImage img = vipToImage(ar);
	if (img.isNull())
	{
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	if (!m_encoder->IsOpen())
	{
		try {
			m_info.width = img.width();
			m_info.height = img.height();
			m_encoder->Open(this->removePrefix(path()).toLatin1().data(), m_info.width, m_info.height, m_info.fps, m_info.rate , m_info.codec_id);
		}
		catch (const std::exception & e)
		{
			setError(e.what());
			return;
		}
	}

	if (img.width() != fullFrameWidth() || img.height() != fullFrameHeight())
		img = img.scaled(fullFrameWidth(), fullFrameHeight(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

	try
	{
		if (!m_encoder->AddFrame(img))
		{
			setError("unable to add image to video");
			return;
		}
	}
	catch (const std::exception & e)
	{
		setError(e.what());
		return;
	}

	this->setSize(this->size() + 1);
}

qint64 MPEGSaver::estimateFileSize() const
{
	return m_encoder->fileSize();
}

void MPEGSaver::setAdditionalInfo(const MPEGIODeviceHandler & info)
{
	m_info = info;
}

MPEGIODeviceHandler MPEGSaver::additionalInfo() const
{
	return m_info;
}










class H264Capture
{
public:

	H264Capture() {
		oformat = NULL;
		ofctx = NULL;
		videoStream = NULL;
		videoFrame = NULL;
		swsCtx = NULL;
		frameCounter = 0;
	}
	~H264Capture() {
		Free();
	}
	bool Init(const char * filename, int width, int height, int fpsrate, int crf, const char * preset);
	bool AddFrame(const VipNDArray & image);
	bool Finish();
	bool Initialized() const {
		return oformat
			!= NULL;
	}
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
	int crf;
	void Free();
	bool Remux();
};





using namespace std;
#define CALC_FFMPEG_VERSION(a,b,c) ( a<<16 | b<<8 | c )

AVOutputFormat* find_FFV1_format()
{
	av_register_all();
	AVOutputFormat * f = av_oformat_next(NULL);
	while (f) {
		if(f->video_codec == AV_CODEC_ID_FFV1)
			return f;
		f = av_oformat_next(f);
	}
	return NULL;
}

static AVCodec *  _ffv1 = NULL;
static AVCodecContext * _ffv1_ctx = NULL;
static void init_ffv1()
{
	if (!_ffv1) {
		_ffv1 = avcodec_find_encoder(AV_CODEC_ID_FFV1);
		_ffv1_ctx = avcodec_alloc_context3(_ffv1);
	}
}
static
inline int _opencv_ffmpeg_av_image_get_buffer_size(enum AVPixelFormat pix_fmt, int width, int height)
{
#if LIBAVUTIL_BUILD >= (LIBAVUTIL_VERSION_MICRO >= 100 \
    ? CALC_FFMPEG_VERSION(51, 63, 100) : CALC_FFMPEG_VERSION(54, 6, 0))
	return av_image_get_buffer_size(pix_fmt, width, height, 1);
#else
	return avpicture_get_size(pix_fmt, width, height);
#endif
};
static
inline void _opencv_ffmpeg_av_packet_unref(AVPacket *pkt)
{
#if LIBAVCODEC_BUILD >= (LIBAVCODEC_VERSION_MICRO >= 100 \
    ? CALC_FFMPEG_VERSION(55, 25, 100) : CALC_FFMPEG_VERSION(55, 16, 0))
	av_packet_unref(pkt);
#else
	av_free_packet(pkt);
#endif
};
static
inline void _opencv_ffmpeg_av_image_fill_arrays(void *frame, uint8_t *ptr, enum AVPixelFormat pix_fmt, int width, int height)
{
#if LIBAVUTIL_BUILD >= (LIBAVUTIL_VERSION_MICRO >= 100 \
    ? CALC_FFMPEG_VERSION(51, 63, 100) : CALC_FFMPEG_VERSION(54, 6, 0))
	av_image_fill_arrays(((AVFrame*)frame)->data, ((AVFrame*)frame)->linesize, ptr, pix_fmt, width, height, 1);
#else
	avpicture_fill((AVPicture*)frame, ptr, pix_fmt, width, height);
#endif
};
/**
* the following function is a modified version of code
* found in ffmpeg-0.4.9-pre1/output_example.c
*/
static AVFrame * icv_alloc_picture_FFMPEG(int pix_fmt, int width, int height, bool alloc)
{
	AVFrame * picture;
	uint8_t * picture_buf = 0;
	int size;

#if LIBAVCODEC_BUILD >= (LIBAVCODEC_VERSION_MICRO >= 100 \
    ? CALC_FFMPEG_VERSION(55, 45, 101) : CALC_FFMPEG_VERSION(55, 28, 1))
	picture = av_frame_alloc();
#else
	picture = avcodec_alloc_frame();
#endif
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	size = _opencv_ffmpeg_av_image_get_buffer_size((AVPixelFormat)pix_fmt, width, height);
	if (alloc) {
		picture_buf = (uint8_t *)malloc(size);
		if (!picture_buf)
		{
			av_free(picture);
			return NULL;
		}
		_opencv_ffmpeg_av_image_fill_arrays(picture, picture_buf,
			(AVPixelFormat)pix_fmt, width, height);
	}

	return picture;
}
static int icv_av_write_frame_FFMPEG(AVCodecContext *c, AVFrame * picture, std::ofstream & fout)
{

	int ret = -1;
	{
		/* encode the image */
		AVPacket pkt;
		av_init_packet(&pkt);
#if LIBAVCODEC_BUILD >= CALC_FFMPEG_VERSION(54, 1, 0)
		int got_output = 0;
		pkt.data = NULL;
		pkt.size = 0;
		ret = avcodec_encode_video2(c, &pkt, picture, &got_output);
		if (ret < 0)
			;
		else if (got_output) {
			fout.write((char*)pkt.data, pkt.size);
		}
		else
			ret = -1;

		
#endif
	}
	return ret;
}
//static std::ofstream fout;
static AVFrame * frame = NULL;


bool H264Capture::Init(const char * filename, int width, int height, int fpsrate, int crf, const char * preset) {

	fname = filename;
	fps = fpsrate;
	tmp_name = fname ;

	//TEST ffv1
	/*if (!_ffv1)
		init_ffv1();
	fout.close();
	fout.open((std::string(filename) + ".ffv1").c_str(), std::ios::binary);
	frame = icv_alloc_picture_FFMPEG(AV_PIX_FMT_GRAY16LE, width, height,true);*/


	if (width % 2) width++;
	if (height % 2) height++;

	int err;
	if (!(oformat = av_guess_format(NULL, tmp_name.c_str(), NULL))) {
		printf("Failed to define output format %i\n", 0);
		return false;
	}

	if ((err = avformat_alloc_output_context2(&ofctx, oformat, NULL, tmp_name.c_str()) < 0)) {
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
	this->file_format = AV_PIX_FMT_YUV444P; //AV_PIX_FMT_YUV420P;

	videoStream->codecpar->codec_id = oformat->video_codec;
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	videoStream->codecpar->width = width;
	videoStream->codecpar->height = height;
	videoStream->codecpar->format = file_format;
	//videoStream->codecpar->bit_rate = 20000000 ;

	avcodec_parameters_to_context(cctx, videoStream->codecpar);
	cctx->time_base = { 1, fps };
	cctx->max_b_frames = 6;
	cctx->gop_size = 20;
	if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264 || videoStream->codecpar->codec_id == AV_CODEC_ID_H265) {
		int r1 = av_opt_set(cctx->priv_data, "preset", preset, AV_OPT_SEARCH_CHILDREN);
		int r2 = av_opt_set(cctx->priv_data, "crf",QByteArray::number(crf).data(), AV_OPT_SEARCH_CHILDREN);
		int r3 = av_opt_set(cctx->priv_data, "qp", "0", AV_OPT_SEARCH_CHILDREN);

		//int r5 = av_opt_set(cctx->priv_data, "lossless", "1", AV_OPT_SEARCH_CHILDREN);
		/*int r4 = av_opt_set(cctx->priv_data, "q", "0", AV_OPT_SEARCH_CHILDREN);
		int r5 = av_opt_set(cctx->priv_data, "lossless", "1", AV_OPT_SEARCH_CHILDREN);
		int r6 = av_opt_set(cctx->priv_data, "lossless=1", "1", AV_OPT_SEARCH_CHILDREN);
		int r7 = av_opt_set(cctx->priv_data, "--lossless", "1", AV_OPT_SEARCH_CHILDREN);*/
		bool stop = true;
	}
	if (ofctx->oformat->flags & AVFMT_GLOBALHEADER) {
		cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	avcodec_parameters_from_context(videoStream->codecpar, cctx);

	if ((err = avcodec_open2(cctx, codec, NULL)) < 0) {
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

	if ((err = avformat_write_header(ofctx, NULL)) < 0) {
		printf("Failed to write header %i\n", err);
		Free();
		return false;
	}

	av_dump_format(ofctx, 0, tmp_name.c_str(), 1);
	return true;
}

bool H264Capture::AddFrame(const VipNDArray & ar) {

	//TEST ffv1
	//_opencv_ffmpeg_av_image_fill_arrays(frame, (uint8_t*)ar.data(), (AVPixelFormat) frame->format, ar.shape(1), ar.shape(0));
	//icv_av_write_frame_FFMPEG(_ffv1_ctx, frame, fout);


	qint64 st = QDateTime::currentMSecsSinceEpoch();
	if (ar.isEmpty() || ar.shapeCount() != 2 || ar.dataSize() != 2)
		return false;
	const VipNDArray tmp = ar.shape() == vipVector(cctx->height, cctx->width) ? ar : ar.resize(vipVector(cctx->height, cctx->width));
	const unsigned short * data = (const unsigned short*)tmp.constData();

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

	/*std::vector<uint8_t> raw(tmp.size() * 3);
	for (int i = 0; i < tmp.size(); ++i) {
		int id = i * 3;
		raw[id] = data[i] & 0xFF;
		raw[id +1] = data[i]>>8;
		raw[id + 2] = 0;
	}

	if (!swsCtx) {
		swsCtx = sws_getContext(cctx->width, cctx->height, AV_PIX_FMT_RGB24, cctx->width, cctx->height, file_format, SWS_BICUBIC, 0, 0, 0);
	}

	{
		int inLinesize[1] = { 3 * cctx->width };
		// From RGB to YUV
		uint8_t * _raw = raw.data();
		sws_scale(swsCtx, (const uint8_t * const *)&_raw, inLinesize, 0, cctx->height, videoFrame->data, videoFrame->linesize);
	}*/


	memset(videoFrame->data[0], 0, videoFrame->linesize[0]*cctx->height);
	memset(videoFrame->data[1], 0, videoFrame->linesize[1] *cctx->height);
	memset(videoFrame->data[2], 0, videoFrame->linesize[2] *cctx->height);
	for (int y = 0; y < cctx->height; ++y) {
		uchar * d0 = videoFrame->data[0] + y*videoFrame->linesize[0];
		uchar * d1 = videoFrame->data[1] + y*videoFrame->linesize[1];
		uchar * d2 = videoFrame->data[2] + y*videoFrame->linesize[2];
		for (int i = 0; i < cctx->width; ++i) {
			d1[i] = data[i + y * cctx->width] & 0xFF;
			d2[i] = data[i + y * cctx->width] >> 8;
		}
	}

	videoFrame->pts = frameCounter;
	videoFrame->pkt_dts = frameCounter;
	videoFrame->pkt_pts = frameCounter;
	videoFrame->pkt_duration = 1;
	frameCounter++;

	if ((err = avcodec_send_frame(cctx, videoFrame)) < 0) {
		printf("Failed to send frame %i\n", err);
		return false;
	}

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if (avcodec_receive_packet(cctx, &pkt) == 0) {
		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.duration = 1;
		av_interleaved_write_frame(ofctx, &pkt);
		av_packet_unref(&pkt);
	}

	qint64 el = QDateTime::currentMSecsSinceEpoch()-st;
	printf("%i\n", (int)el);

	return true;
}

bool H264Capture::Finish() {

	//TEST ffv1
	//fout.close();

	//DELAYED FRAMES
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	for (;;) {
		avcodec_send_frame(cctx, NULL);
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

void H264Capture::Free() {
	if (videoFrame) {
		av_frame_free(&videoFrame);
		videoFrame = NULL;
	}
	if (cctx) {
		avcodec_free_context(&cctx);
	}
	if (ofctx) {
		avformat_free_context(ofctx);
		ofctx = NULL;
	}
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = NULL;
	}
}


bool H264Capture::Remux() {
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	int err;
	bool res = true;
	AVPacket videoPkt;
	int ts = 0;
	AVStream *inVideoStream = 0;
	AVStream *outVideoStream = 0;
	std::string outtmp = fname +".mp4";

 	if ((err = avformat_open_input(&ifmt_ctx, fname.c_str(), 0, 0)) < 0) {
		printf("Failed to open input file for remuxing %i\n", err);
		res = false;
		goto end;
	}
	if ((err = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf("Failed to retrieve input stream information %i\n", err);
		res = false;
		goto end;
	}



	if ((err = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outtmp.c_str()))) {
		printf("Failed to allocate output context %i\n", err);
		res = false;
		goto end;
	}

	inVideoStream = ifmt_ctx->streams[0];
	outVideoStream = avformat_new_stream(ofmt_ctx, NULL);
	if (!outVideoStream) {
		printf("Failed to allocate output video stream %i\n", 0);
		res = false;
		goto end;
	}
	outVideoStream->time_base = { 1, fps };
	avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
	outVideoStream->codecpar->codec_tag = 0;

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&ofmt_ctx->pb, outtmp.c_str(), AVIO_FLAG_WRITE)) < 0) {
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
		videoPkt.duration = 12800;// av_rescale_q(videoPkt.duration, inVideoStream->time_base, outVideoStream->time_base);
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
	if (!QFile::remove(fname.c_str()))
		res = false;
	else if (!QFile::rename(outtmp.c_str(), fname.c_str()))
		res = false;

	return res;
}









bool remuxH264Bitstream(const char * input, const char * output, int fps) {
	AVFormatContext* ifmt_ctx = NULL, * ofmt_ctx = NULL;
	int err;
	bool res = true;
	AVPacket videoPkt;
	int ts = 0;
	AVStream* inVideoStream = 0;
	AVStream* outVideoStream = 0;

	if ((err = avformat_open_input(&ifmt_ctx, input, 0, 0)) < 0) {
		printf("Failed to open input file for remuxing %i\n", err);
		res = false;
		goto end;
	}
	if ((err = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf("Failed to retrieve input stream information %i\n", err);
		res = false;
		goto end;
	}



	if ((err = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output))) {
		printf("Failed to allocate output context %i\n", err);
		res = false;
		goto end;
	}

	inVideoStream = ifmt_ctx->streams[0];
	outVideoStream = avformat_new_stream(ofmt_ctx, NULL);
	if (!outVideoStream) {
		printf("Failed to allocate output video stream %i\n", 0);
		res = false;
		goto end;
	}
	outVideoStream->time_base = { 1, fps };
	//outVideoStream->avg_frame_rate = ofmt_ctx->
	avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
	outVideoStream->codecpar->codec_tag = 0;

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&ofmt_ctx->pb, output, AVIO_FLAG_WRITE)) < 0) {
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
		videoPkt.duration = 12800;// av_rescale_q(videoPkt.duration, inVideoStream->time_base, outVideoStream->time_base);
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
	
	return res;
}







static const char * compressionToPreset(int level) {
	if (level <= 0) return "ultrafast";
	else if (level == 1) return "superfast";
	else if (level == 2) return "veryfast";
	else if (level == 3) return "faster";
	else if (level == 4) return "fast";
	else if (level == 5) return "medium";
	else if (level == 6) return "slow";
	else if (level == 7) return "slower";
	else return "veryslow";
}


class IR_H264_Saver::PrivateData
{
public:
	H264Capture * encoder;
	int lossyLevel;
	int compressionLevel;
	PrivateData() : encoder(NULL), lossyLevel(0), compressionLevel(0){}
};

IR_H264_Saver::IR_H264_Saver(QObject * parent)
	:VipIODevice(parent)
{
	m_data = new PrivateData();
}

IR_H264_Saver::~IR_H264_Saver()
{
	close();
	delete m_data;
}

void IR_H264_Saver::setLossyLevel(int level)
{
	m_data->lossyLevel = level;
}
void IR_H264_Saver::setCompressionLevel(int clevel)
{
	m_data->compressionLevel = clevel;;
}
int IR_H264_Saver::lossyLevel() const
{
	return m_data->lossyLevel;
}
int IR_H264_Saver::compressionLevel() const
{
	return m_data->compressionLevel;
}

bool IR_H264_Saver::open(VipIODevice::OpenModes mode)
{
	close();

	if (!(mode & VipIODevice::WriteOnly))
		return false;

	m_data->encoder = new H264Capture();
	setSize(0);
	setOpenMode(mode);
	return true;
}

void IR_H264_Saver::close()
{
	if (m_data->encoder) {
		m_data->encoder->Finish();
		delete m_data->encoder;
		m_data->encoder = NULL;
	}
}

void IR_H264_Saver::apply()
{
	VipAnyData in = inputAt(0)->data();
	VipNDArray ar = in.data().value<VipNDArray>();
	if (ar.isEmpty())
	{
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	if (!m_data->encoder->Initialized())
	{
		
		if (!m_data->encoder->Init(this->removePrefix(this->path()).toLatin1().data(), ar.shape(1), ar.shape(0), 25, lossyLevel(), compressionToPreset(compressionLevel()))) {
			setError("Unable to initialize output file");
		}
		
	}

	try
	{
		if (!m_data->encoder->AddFrame(ar))
		{
			setError("unable to add image to video");
			return;
		}
	}
	catch (const std::exception & e)
	{
		setError(e.what());
		return;
	}

	this->setSize(this->size() + 1);
}



#include <qspinbox.h>
#include <qgridlayout.h>
#include <qlabel.h>

class IR_H264_Saver_Panel::PrivateData
{
public:
	QSpinBox compression;
	QSpinBox lossy;
	QPointer<IR_H264_Saver> saver;
};
IR_H264_Saver_Panel::IR_H264_Saver_Panel()
{
	m_data = new PrivateData();

	QGridLayout * lay = new QGridLayout();
	lay->addWidget(new QLabel("Compression level"), 0, 0);
	lay->addWidget(&m_data->compression, 0, 1);
	lay->addWidget(new QLabel("Lossy level"), 1, 0);
	lay->addWidget(&m_data->lossy, 1, 1);

	m_data->compression.setRange(0, 8);
	m_data->compression.setToolTip("<b>Compression level</b><br>Affect the file size and encoding speed, but not the image quality.<br>0 is the fastest level.");
	m_data->lossy.setRange(0, 51);
	m_data->compression.setToolTip("<b>Loss level</b><br>Affect the file size and image quality.<br>0 means lossless, 51 means high level of degradation.");

	connect(&m_data->compression, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));
	connect(&m_data->lossy, SIGNAL(valueChanged(int)), this, SLOT(updateSaver()));

	setLayout(lay);
}
IR_H264_Saver_Panel::~IR_H264_Saver_Panel()
{
	delete m_data;
}

void IR_H264_Saver_Panel::setSaver(IR_H264_Saver * s) {
	m_data->saver = s;
	if (!s)
		return;
	m_data->compression.blockSignals(true);
	m_data->lossy.blockSignals(true);
	m_data->compression.setValue(s->compressionLevel());
	m_data->lossy.setValue(s->lossyLevel());
	m_data->compression.blockSignals(false);
	m_data->lossy.blockSignals(false);
}
IR_H264_Saver* IR_H264_Saver_Panel::saver() const
{
	return m_data->saver;
}

void IR_H264_Saver_Panel::updateSaver() const
{
	if (m_data->saver) {
		m_data->saver->setCompressionLevel(m_data->compression.value());
		m_data->saver->setLossyLevel(m_data->lossy.value());
	}
}


IR_H264_Saver_Panel * editIR_H264_Saver(IR_H264_Saver * s)
{
	IR_H264_Saver_Panel * panel = new IR_H264_Saver_Panel();
	panel->setSaver(s);
	return panel;
}

#include "VipStandardWidgets.h"
static int init()
{
	vipFDObjectEditor().append<IR_H264_Saver_Panel * (IR_H264_Saver * s)>(editIR_H264_Saver);
	return 0;
}
static int _init = init();