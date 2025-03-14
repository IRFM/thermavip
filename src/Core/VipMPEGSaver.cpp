#include "VipMPEGSaver.h"
#include "VipLogging.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifndef INT64_C
#define INT64_C(c) (c##LL)
#define UINT64_C(c) (c##ULL)
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

#include <fstream>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <string>

#include <QFileInfo>
#include <QImage>

class VideoCapture
{
public:
	VideoCapture()
	{
		oformat = nullptr;
		ofctx = nullptr;
		videoStream = nullptr;
		videoFrame = nullptr;
		swsCtx = nullptr;
		frameCounter = 0;
	}

	~VideoCapture() { Free(); }
	bool Init(const char* filename, int width, int height, int fpsrate, double bitrate, int threads = 1);
	bool AddFrame(uint8_t* data);
	bool AddFrame(const QImage& image);
	bool Finish();
	std::string TmpName() const { return tmp_name; }

private:
	std::string fname;
	std::string tmp_name;
	AVOutputFormat* oformat;
	AVFormatContext* ofctx;
	AVStream* videoStream;
	AVFrame* videoFrame;
	AVCodec* codec;
	AVCodecContext* cctx;
	SwsContext* swsCtx;
	std::vector<uint8_t> img;
	int frameCounter;
	AVPixelFormat file_format;
	int fps;

	void Free();
	bool Remux();
};

inline VideoCapture* Init(const char* filename, int width, int height, int fps, double bitrate, int threads)
{
	VideoCapture* vc = new VideoCapture();
	if (vc->Init(filename, width, height, fps, bitrate, threads))
		return vc;
	return nullptr;
};
inline bool AddFrame(uint8_t* data, VideoCapture* vc)
{
	return vc->AddFrame(data);
}
inline bool AddFrame(const QImage& image, VideoCapture* vc)
{
	return vc->AddFrame(image);
}
inline bool Finish(VideoCapture* vc)
{
	bool res = vc->Finish();
	delete vc;
	return res;
}

/// @brief Helper class for video encoding
class VideoEncoder
{
public:
	VideoEncoder();
	VideoEncoder(const std::string& name, int width, int height, double fps, double bitrate, int codec_id = -1);
	~VideoEncoder();
	void Open(const std::string& name, int width, int height, double fps, double bitrate, int codec_id = -1);
	void Close(bool debug = false);

	bool IsOpen() const { return m_file_open; }

	bool AddFrame(const QImage& image);

	// position actuelle (en s), peut etre approximatif
	double GetCurrentTimePos() const { return m_time_pos; }

	// position actuelle (en frame), peut etre approximatif
	long int GetCurrentFramePos() const;

	// temps total
	double GetTotalTime() const { return m_total_time; }

	// nb total de frame
	long int GetTotalFrame() const { return m_total_frame; }

	// largeur
	int GetWidth() const;

	// hauteur
	int GetHeight() const;

	// frame per second
	double GetFps() const;

	// echantillonage (octets/s)
	double GetRate() const;

	// nom du fichier ouvert
	std::string GetFileName() const;

	void SetSize(int width, int height)
	{
		m_width = width;
		m_height = height;
	}

	void SetFps(double fps) { m_fps = fps; }

	void SetRate(double bitrate) { m_frame_rate = bitrate; }

	void SetThreads(int th) { m_threads = th; }
	int GetThreads() const { return m_threads; }

	qint64 fileSize() const;

protected:
	std::string m_filename;
	int m_width;
	int m_height;
	int m_threads{ 1 };
	double m_fps;
	long int m_frame_pos;
	double m_time_pos;
	double m_frame_rate;
	long int m_total_frame;
	double m_total_time;
	bool m_file_open;

	// variables ffmpeg
	AVOutputFormat* fmt;
	AVFormatContext* oc;
	AVStream* video_str;
	AVCodecContext* context{ nullptr };
	AVFrame *picture, *tmp_picture, *rgb8_picture;
	SwsContext* img_convert_context;
	SwsContext* additional_GIF_context;
	VideoCapture* vc; // for h264 only
	AVFrame* convert(const QImage& image);

	void init();
	AVFrame* alloc_picture(int width, int height, enum AVPixelFormat pix);
};





VideoEncoder::VideoEncoder()
{
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

VideoEncoder::VideoEncoder(const std::string& name, int width, int height, double fps, double rate, int codec_id)
{
	init();
	Open(name, width, height, fps, rate, codec_id);
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
	vc = nullptr;
}

void VideoEncoder::Open(const std::string& name, int width, int height, double fps, double rate, int codec_id)
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
	AVPixelFormat src_pxl_fmt = AV_PIX_FMT_RGB24;

	int sws_flags = SWS_FAST_BILINEAR; // interpolation method (keeping same size images for now)

	fmt = nullptr;
#if LIBAVFORMAT_VERSION_MAJOR > 52
	fmt = (AVOutputFormat*)av_guess_format(nullptr, filename, nullptr);
#else
	fmt = guess_format(nullptr, filename, nullptr);
#endif

	if (!fmt) {
		// unable to retrieve output format
		Close();
		throw std::runtime_error("Could not determine format from filename");
	}

#ifdef ENABLE_H264
	if (fmt->video_codec == AV_CODEC_ID_H264) {
		// we need even width and height
		if (width % 2)
			m_width = ++width;
		if (height % 2)
			m_height = ++height;

		vc = Init(filename, width, height, fps, rate, m_threads);
		if (!vc) {
			throw std::runtime_error("Could not determine format from filename");
		}
		return;
	}
#endif

	if (!fmt) {
		// default format "mpeg"
#if LIBAVFORMAT_VERSION_MAJOR > 52
		fmt = (AVOutputFormat*)av_guess_format("mpeg", nullptr, nullptr);
#else
		fmt = guess_format("mpeg", nullptr, nullptr);
#endif
	}
	if (!fmt) {
		// unable to retrieve output format
		Close();
		throw std::runtime_error("Could not determine format from filename");
	}

	if (codec_id != -1) {
		std::string ext = QFileInfo(QString(filename)).suffix().toStdString();
		AVOutputFormat* temp = nullptr;
#if LIBAVFORMAT_VERSION_MAJOR > 58
		void* opaque = NULL;
		while ((temp = (AVOutputFormat*)av_muxer_iterate(&opaque))) {
			if (temp->video_codec == codec_id /*&& temp->audio_codec == CODEC_ID_NONE*/ &&
			    (std::string(temp->extensions).find(ext.c_str()) != std::string::npos || codec_id == AV_CODEC_ID_RAWVIDEO)) {
				fmt = (AVOutputFormat*)temp;
				break;
			}
		}
#else

		temp = av_oformat_next(nullptr);
		while (temp != nullptr) {
			if (temp->video_codec == codec_id /*&& temp->audio_codec == CODEC_ID_NONE*/ &&
			    (std::string(temp->extensions).find(ext.c_str()) != std::string::npos || codec_id == AV_CODEC_ID_RAWVIDEO)) {
				fmt = temp;
				// find = true;
				break;
			}
			else
				temp = temp->next;
		}
#endif
		if (temp == nullptr) {
			Close(true);
			throw std::runtime_error("Wrong extention for this video codec");
		}
	}

#if LIBAVFORMAT_VERSION_MAJOR > 58
	int err = 0;
	if ((err = avformat_alloc_output_context2(&oc, fmt, NULL, filename) < 0)) {
		Close(true);
		throw std::runtime_error("Failed to allocate output context");
	}
#else
	oc = avformat_alloc_context();
	if (!oc) {
		// unable to allocate format context
		Close(true);
		throw std::runtime_error("Mem allocation error for format context");
	}

	oc->oformat = fmt;
	strcpy(oc->filename, filename);
#endif

	// open codecs and alloc buffers
	AVCodec* codec = (AVCodec*)avcodec_find_encoder(fmt->video_codec);
	if (!codec) {
		// unable to find codec
		Close(true);
		throw std::runtime_error("No codec found");
	}

	// add video stream
	int stream_index = 0;
	video_str = nullptr;
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		video_str = avformat_new_stream(oc, codec);

		if (!video_str) {
			// no stream allocated
			Close(true);
			throw std::runtime_error("Unable to create new video stream");
		}

		video_str->id = stream_index;
		// video_str->time_base = { 1, (int)m_fps };
		// video_str->time_base = { 1, (int)((1/m_fps)*AV_TIME_BASE) };
		// video_str->avg_frame_rate = { (int)m_fps,1 };
		// video_str->r_frame_rate = { (int)m_fps, 1 };
		// video_str->sample_aspect_ratio = { 1, 1 };
		video_str->codecpar->codec_id = codec->id;
		video_str->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
		video_str->codecpar->width = width;
		video_str->codecpar->height = height;
	}
	else {
		Close(true);
		throw std::runtime_error("No codec identified");
	}

	AVCodecContext* c = context = avcodec_alloc_context3(codec);
	if (!c) {
		Close(true);
		throw std::runtime_error("Failed to allocate codec context");
	}

	c->codec_id = fmt->video_codec;
#if LIBAVFORMAT_VERSION_MAJOR > 52
	c->codec_type = AVMEDIA_TYPE_VIDEO;
#else
	c->codec_type = CODEC_TYPE_VIDEO;
#endif
	c->bit_rate = (int64_t)m_frame_rate;
	c->width = width;
	c->height = height;
	c->time_base = { 1, (int)m_fps };
	// c->time_base.num = 1;
	// c->time_base.den = m_fps;
	// c->time_base = { 1, (int)((1. / m_fps) * AV_TIME_BASE) };
	c->gop_size = 12;
	c->pix_fmt = dest_pxl_fmt;

	// TEST GIF
	if (c->codec_id == AV_CODEC_ID_GIF) {
		c->pix_fmt = AV_PIX_FMT_RGB8;
	}

	if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
		c->max_b_frames = 2;
	if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
		c->mb_decision = 2;

	int threads = m_threads;
	if (threads <= 0)
		threads = 1;
	else if (threads > 12)
		threads = 12;
	std::string threadCount = QByteArray::number(threads).data();
	av_opt_set(c->priv_data, "threads", threadCount.c_str(), AV_OPT_SEARCH_CHILDREN);
	av_opt_set(c, "threads", threadCount.c_str(), AV_OPT_SEARCH_CHILDREN);

	if ((err = avcodec_open2(c, codec, nullptr)) < 0) {
		Close(true);
		throw std::runtime_error("Unable to open codec");
	}
	video_str->codecpar->format = c->pix_fmt;

	img_convert_context = nullptr;
	img_convert_context = sws_getContext(c->width, c->height, src_pxl_fmt, c->width, c->height, AV_PIX_FMT_YUV420P, sws_flags, nullptr, nullptr, nullptr);
	additional_GIF_context = nullptr;
	if (c->codec_id == AV_CODEC_ID_GIF) {
		// GIF only, we need to convert from YUV420 to RGB8

		additional_GIF_context = sws_getContext(c->width, c->height, AV_PIX_FMT_YUV420P, c->width, c->height, c->pix_fmt, sws_flags, nullptr, nullptr, nullptr);

		rgb8_picture = alloc_picture(c->width, c->height, c->pix_fmt);
		if (!rgb8_picture) {
			Close(true);
			throw std::runtime_error("Could not allocate picture\n");
		}
	}

	picture = alloc_picture(c->width, c->height, dest_pxl_fmt);
	if (!picture) {
		Close(true);
		throw std::runtime_error("Could not allocate picture\n");
	}

	tmp_picture = alloc_picture(c->width, c->height, src_pxl_fmt);
	if (!picture) {
		Close(true);
		throw std::runtime_error("Could not allocate picture\n");
	}

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE))
		if (avio_open(&oc->pb, m_filename.c_str(), AVIO_FLAG_WRITE) < 0) {
			Close(true);
			throw std::runtime_error("Could not open the file");
		}

	/* write the stream header, if any */
	if (avformat_write_header(oc, nullptr) != 0)
		throw std::runtime_error("Unable to write header");
	;
}

// creer un AVFrame valide de dimension width, height, en RGB
AVFrame* VideoEncoder::alloc_picture(int width, int height, enum AVPixelFormat pix)
{
	AVFrame* pict = av_frame_alloc();
	if (!pict)
		return nullptr;
	pict->format = pix;
	pict->width = width;
	pict->height = height;
	int err = av_frame_get_buffer(pict, 32);
	if (err < 0) {
		av_frame_free(&pict);
		return nullptr;
	}
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

	if (context) {
		// if (!debug)
		avcodec_free_context(&context);
		context = nullptr;
		video_str = nullptr;
	}

	if (picture) {
		av_frame_free(&picture);
		picture = nullptr;
	}

	if (tmp_picture) {
		av_frame_free(&tmp_picture);
		tmp_picture = nullptr;
	}

	if (rgb8_picture) {
		av_frame_free(&rgb8_picture);
		rgb8_picture = nullptr;
	}

	if (img_convert_context) {
		sws_freeContext(img_convert_context);
		img_convert_context = nullptr;
	}

	if (additional_GIF_context) {
		sws_freeContext(additional_GIF_context);
		additional_GIF_context = nullptr;
	}

	if (oc) {
		if (!debug)
			av_write_trailer(oc);

		for (unsigned int i = 0; i < oc->nb_streams; i++) {
			av_freep(oc->streams[i]);
		}

		if (!(fmt->flags & AVFMT_NOFILE))
			if (!debug)
				avio_close(oc->pb);

		avformat_free_context(oc);
		oc = nullptr;
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

bool VideoEncoder::AddFrame(const QImage& im)
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

	av_frame_make_writable(picture);
	if (rgb8_picture)
		av_frame_make_writable(rgb8_picture);

	const QImage image = (im.width() != m_width || im.height() != m_height) ? im.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : im;
	AVFrame* frame = convert(image);
	frame->pts = (m_frame_pos) * (1000.0 / (float)(m_fps));

	int err = avcodec_send_frame(context, frame);
	if (err < 0) {
		return false;
	}

	AVPacket* pkt = av_packet_alloc();

	if (avcodec_receive_packet(context, pkt) == 0) {
		// pkt.flags |= AV_PKT_FLAG_KEY;
		pkt->duration = 1;
		av_interleaved_write_frame(oc, pkt);
	}
	av_packet_unref(pkt);
	av_packet_free(&pkt);
	m_total_frame++;
	m_frame_pos++;
	m_time_pos += 1 / m_fps;
	return true;
}

AVFrame* VideoEncoder::convert(const QImage& image)
{
	const QImage temp = (image.width() != m_width || image.height() != m_height) ? image.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : image;

	for (int y = 0; y < m_height; y++) {
		uint8_t* data = tmp_picture->data[0] + y * tmp_picture->linesize[0];
		const QRgb* dat = reinterpret_cast<const QRgb*>(temp.scanLine(y));
		int i = 0;
		for (int x = 0; x < m_width; x++, i += 3) {
			data[i] = qRed(dat[x]);
			data[i + 1] = qGreen(dat[x]);
			data[i + 2] = qBlue(dat[x]);
		}
	}
	sws_scale(img_convert_context, tmp_picture->data, tmp_picture->linesize, 0, context->height, picture->data, picture->linesize);
	if (context->codec_id == AV_CODEC_ID_GIF) {
		sws_scale(additional_GIF_context, picture->data, picture->linesize, 0, context->height, rgb8_picture->data, rgb8_picture->linesize);
		return rgb8_picture;
	}
	return picture;
}

double VideoEncoder::GetRate() const
{
	return m_frame_rate;
}

int VideoEncoder::GetWidth() const
{
	return m_width;
}

int VideoEncoder::GetHeight() const
{
	return m_height;
}

std::string VideoEncoder::GetFileName() const
{
	return m_filename;
}

long int VideoEncoder::GetCurrentFramePos() const
{
	return m_frame_pos;
}

double VideoEncoder::GetFps() const
{
	return m_fps;
}

#if defined(ENABLE_H264) && LIBAVCODEC_VERSION_MICRO >= 100 // Ffmpeg micro version starts at 100, while libav is < 100

using namespace std;

bool VideoCapture::Init(const char* filename, int width, int height, int fpsrate, double bitrate, int threads)
{

	fname = filename;
	fps = fpsrate;
	tmp_name = fname + ".h264";

	int err;

	if (!(oformat = (AVOutputFormat*)av_guess_format(nullptr, tmp_name.c_str(), nullptr))) {
		vip_debug("Failed to define output format %i\n", 0);
		return false;
	}

	if ((err = avformat_alloc_output_context2(&ofctx, oformat, nullptr, tmp_name.c_str()) < 0)) {
		vip_debug("Failed to allocate output context %i\n", err);
		Free();
		return false;
	}

	if (!(codec = (AVCodec*)avcodec_find_encoder(oformat->video_codec))) {
		vip_debug("Failed to find encoder %i\n", 0);
		Free();
		return false;
	}

	if (!(videoStream = avformat_new_stream(ofctx, codec))) {
		vip_debug("Failed to create new stream %i\n", 0);
		Free();
		return false;
	}

	if (!(cctx = avcodec_alloc_context3(codec))) {
		vip_debug("Failed to allocate codec context %i\n", 0);
		Free();
		return false;
	}

	videoStream->time_base = { 1, fps };

	// TEST
	this->file_format = AV_PIX_FMT_YUV420P;

	videoStream->codecpar->codec_id = oformat->video_codec;
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	videoStream->codecpar->width = width;
	videoStream->codecpar->height = height;
	videoStream->codecpar->format = file_format;
	videoStream->codecpar->bit_rate = bitrate;

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

	if (threads <= 0)
		threads = 1;
	else if (threads > 12)
		threads = 12;
	std::string threadCount = QByteArray::number(threads).data();
	av_opt_set(cctx->priv_data, "threads", threadCount.c_str(), AV_OPT_SEARCH_CHILDREN);
	av_opt_set(cctx, "threads", threadCount.c_str(), AV_OPT_SEARCH_CHILDREN);

	avcodec_parameters_from_context(videoStream->codecpar, cctx);

	if ((err = avcodec_open2(cctx, codec, nullptr)) < 0) {
		vip_debug("Failed to open codec %i\n", err);
		Free();
		return false;
	}

	if (!(oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&ofctx->pb, tmp_name.c_str(), AVIO_FLAG_WRITE)) < 0) {
			vip_debug("Failed to open file %i\n", err);
			Free();
			return false;
		}
	}

	if ((err = avformat_write_header(ofctx, nullptr)) < 0) {
		vip_debug("Failed to write header %i\n", err);
		Free();
		return false;
	}

	av_dump_format(ofctx, 0, tmp_name.c_str(), 1);
	return true;
}
#include <qdatetime.h>
bool VideoCapture::AddFrame(const QImage& image)
{

	qint64 st = QDateTime::currentMSecsSinceEpoch();

	const QImage temp = (image.width() != cctx->width || image.height() != cctx->height) ? image.scaled(cctx->width, cctx->height) : image;
	if ((int)img.size() != (int)(cctx->width * cctx->height * 3))
		img.resize(cctx->width * cctx->height * 3);

	int i = 0;
	for (int y = 0; y < cctx->height; y++) {
		const QRgb* dat = reinterpret_cast<const QRgb*>(temp.scanLine(y));
		for (int x = 0; x < cctx->width; x++, i += 3) {
			img[i] = qRed(dat[x]);
			img[i + 1] = qGreen(dat[x]);
			img[i + 2] = qBlue(dat[x]);
		}
	}
	qint64 el1 = QDateTime::currentMSecsSinceEpoch() - st;
	bool res = AddFrame(img.data());
	qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st;
	vip_debug("encode: %i, %i\n", (int)el1, (int)el2);
	return res;
}

bool VideoCapture::AddFrame(uint8_t* data)
{

	int err;
	if (!videoFrame) {

		videoFrame = av_frame_alloc();
		videoFrame->format = file_format;
		videoFrame->width = cctx->width;
		videoFrame->height = cctx->height;

		if ((err = av_frame_get_buffer(videoFrame, 32)) < 0) {
			vip_debug("Failed to allocate picture %i\n", err);
			return false;
		}
	}

	if (!swsCtx) {
		swsCtx = sws_getContext(cctx->width, cctx->height, AV_PIX_FMT_RGB24, cctx->width, cctx->height, file_format, SWS_BICUBIC, 0, 0, 0);
	}

	{
		int inLinesize[1] = { 3 * cctx->width };
		// From RGB to YUV
		sws_scale(swsCtx, (const uint8_t* const*)&data, inLinesize, 0, cctx->height, videoFrame->data, videoFrame->linesize);
	}

	videoFrame->pts = frameCounter++;

	if ((err = avcodec_send_frame(cctx, videoFrame)) < 0) {
		vip_debug("Failed to send frame %i\n", err);
		return false;
	}

	AVPacket* pkt = av_packet_alloc();

	if (avcodec_receive_packet(cctx, pkt) == 0) {
		av_interleaved_write_frame(ofctx, pkt);
		
	}
	av_packet_unref(pkt);
	av_packet_free(&pkt);
	return true;
}

bool VideoCapture::Finish()
{
	// DELAYED FRAMES
	AVPacket* pkt = av_packet_alloc();
	
	for (;;) {
		avcodec_send_frame(cctx, nullptr);
		if (avcodec_receive_packet(cctx, pkt) == 0) {
			av_interleaved_write_frame(ofctx, pkt);
			av_packet_unref(pkt);
		}
		else {
			break;
		}
	}

	av_write_trailer(ofctx);
	if (!(oformat->flags & AVFMT_NOFILE)) {
		int err = avio_close(ofctx->pb);
		if (err < 0) {
			vip_debug("Failed to close file %i\n", err);
		}
	}

	av_packet_unref(pkt);
	av_packet_free(&pkt);
	Free();

	return Remux();
}

void VideoCapture::Free()
{
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

bool VideoCapture::Remux()
{
	AVFormatContext *ifmt_ctx = nullptr, *ofmt_ctx = nullptr;
	int err;
	bool res = true;
	AVPacket videoPkt;
	int ts = 0;
	AVStream* inVideoStream = 0;
	AVStream* outVideoStream = 0;

	if ((err = avformat_open_input(&ifmt_ctx, tmp_name.c_str(), 0, 0)) < 0) {
		vip_debug("Failed to open input file for remuxing %i\n", err);
		res = false;
		goto end;
	}
	if ((err = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		vip_debug("Failed to retrieve input stream information %i\n", err);
		res = false;
		goto end;
	}

	if ((err = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, fname.c_str()))) {
		vip_debug("Failed to allocate output context %i\n", err);
		res = false;
		goto end;
	}

	inVideoStream = ifmt_ctx->streams[0];
	outVideoStream = avformat_new_stream(ofmt_ctx, nullptr);
	if (!outVideoStream) {
		vip_debug("Failed to allocate output video stream %i\n", 0);
		res = false;
		goto end;
	}
	outVideoStream->time_base = { 1, fps };
	avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
	outVideoStream->codecpar->codec_tag = 0;

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&ofmt_ctx->pb, fname.c_str(), AVIO_FLAG_WRITE)) < 0) {
			vip_debug("Failed to open output file %i\n", err);
			res = false;
			goto end;
		}
	}

	if ((err = avformat_write_header(ofmt_ctx, 0)) < 0) {
		vip_debug("Failed to write header to output file %i\n", err);
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
			vip_debug("Failed to mux packet %i\n", err);
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






VipMPEGSaver::VipMPEGSaver(QObject* parent)
  : VipIODevice(parent)
{
	m_encoder = new VideoEncoder();
}

VipMPEGSaver::~VipMPEGSaver()
{
	close();
	delete m_encoder;
}

qint32 VipMPEGSaver::fullFrameWidth() const
{
	return m_encoder->GetWidth();
}
qint32 VipMPEGSaver::fullFrameHeight() const
{
	return m_encoder->GetHeight();
}

bool VipMPEGSaver::open(VipIODevice::OpenModes mode)
{
	if (mode & VipIODevice::ReadOnly)
		return false;

	if (this->isOpen())
		this->close();

	try {

		this->setOpenMode(mode);
		this->setSize(0);
		return true;
	}
	catch (const std::exception& e) {
		setError(e.what());
		return false;
	}
	VIP_UNREACHABLE();
	// return false;
}

void VipMPEGSaver::close()
{
	m_encoder->Close();
	setOpenMode(NotOpen);
}

void VipMPEGSaver::apply()
{
	VipAnyData in = inputAt(0)->data();
	VipNDArray ar = in.data().value<VipNDArray>();
	if (ar.isEmpty()) {
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	QImage img = vipToImage(ar);
	if (img.isNull()) {
		setError("Empty input image", VipProcessingObject::WrongInput);
		return;
	}

	if (!m_encoder->IsOpen()) {
		try {
			m_info.width = img.width();
			m_info.height = img.height();
			m_encoder->SetThreads(m_info.threads);
			m_encoder->Open(this->removePrefix(path()).toLatin1().data(), m_info.width, m_info.height, m_info.fps, m_info.rate, m_info.codec_id);
		}
		catch (const std::exception& e) {
			setError(e.what());
			return;
		}
	}

	if (img.width() != fullFrameWidth() || img.height() != fullFrameHeight())
		img = img.scaled(fullFrameWidth(), fullFrameHeight(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

	try {
		if (!m_encoder->AddFrame(img)) {
			setError("unable to add image to video");
			return;
		}
	}
	catch (const std::exception& e) {
		setError(e.what());
		return;
	}

	this->setSize(this->size() + 1);
}

qint64 VipMPEGSaver::estimateFileSize() const
{
	return m_encoder->fileSize();
}

void VipMPEGSaver::setAdditionalInfo(const VipMPEGIODeviceHandler& info)
{
	m_info = info;
}

VipMPEGIODeviceHandler VipMPEGSaver::additionalInfo() const
{
	return m_info;
}
