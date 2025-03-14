#include "VipMPEGLoader.h"


#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifndef INT64_C
#define INT64_C(c) (c##LL)
#define UINT64_C(c) (c##ULL)
#endif

extern "C" {
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <sstream>
#include <stdexcept>
#include <cmath>
#include <fstream>

#include <QImage>
#include <QColor>
#include <QString>
#include <qmap.h>
#include <QFileInfo>
#include <QDateTime>
#include <qfile.h>





/// @brief Helper class for video decoding
class VideoDecoder
{
public:
	static QStringList list_devices();

	VideoDecoder();
	VideoDecoder(const std::string& name);
	~VideoDecoder();
	void Open(const std::string& name, AVInputFormat* format = nullptr, AVDictionary** options = nullptr);
	void Open(const QString& name, const QString& format, const QMap<QString, QString>& options);
	void Close();

	bool IsSequential() const;

	AVFormatContext* GetContext() const { return pFormatCtx; }

	const QImage& GetCurrentFrame();
	const QImage& GetFrameByTime(double time);
	const QImage& GetFrameByNumber(qint64 num);

	bool MoveNextFrame();

	double GetTimePos() const;
	qint64 GetCurrentFramePos() const;

	double GetTotalTime() const;

	int GetWidth() const;
	int GetHeight() const;
	double GetFps() const;
	double GetRate() const;
	double GetOffset() const { return m_offset; }

	std::string GetFileName() const;

	// pixel type as ffmpeg enum AVPixelFormat
	int pixelType() const;

	uint64_t LastReadDTS() const { return m_last_dts; }

	// se deplace au temps donne (en s), peut etre approximatif
	void SeekTime(double time);
	void SeekTime2(double time);
	void SeekFrame(qint64 pos);

protected:
	double getTime();
	void toRGB(AVFrame* frame);

	uint64_t m_last_dts;
	QImage m_image;
	double m_current_time;
	std::string m_filename;
	int m_width;
	int m_height;
	int m_skip_packet;
	double m_fps;
	bool m_use_dts{ true };
	double m_tech;
	qint64 m_frame_pos;
	double m_time_pos;
	double m_offset;
	double m_total_time;
	bool m_file_open;
	bool m_is_packet;

	// variables ffmpeg
	AVFormatContext* pFormatCtx;
	int videoStream;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame* pFrame;
	AVFrame* pFrameRGB;
	SwsContext* pSWSCtx;
};


int init_libavcodec()
{
	avdevice_register_all();
	avformat_network_init();
	return 0;
}
static int init = init_libavcodec();



static QString _stderr;

void log_to_array(void*, int, const char* fmt, va_list vl)
{
	_stderr += QString::vasprintf(fmt, vl);
	vip_debug(fmt, vl);
}

QStringList VideoDecoder::list_devices()
{
	// freopen("tmp.txt", "w", stderr);
	_stderr.clear();
	av_log_set_callback(log_to_array);

	AVFormatContext* formatC = avformat_alloc_context();
	AVDictionary* options = nullptr;
	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat* iformat = (AVInputFormat*)av_find_input_format("dshow");
	avformat_open_input(&formatC, "video=dummy", iformat, &options);
	av_log_set_callback(av_log_default_callback);
	// if (ret < 0)
	//	return QStringList();

	// freopen("CON", "w", stderr);

	// QFile file("tmp.txt");
	// file.open(QFile::ReadOnly | QFile::Text);
	QString ar = _stderr; // file.readAll();

	QStringList res;
	QStringList lines = ar.split("\n", VIP_SKIP_BEHAVIOR::SkipEmptyParts);

	for (int i = 0; i < lines.size(); ++i) {
		QString line = lines[i];
		if (lines[i].contains("audio", Qt::CaseInsensitive))
			continue;

		if (lines[i].contains("Alternative name"))
			continue;

		int index1 = lines[i].indexOf('"');
		int index2 = lines[i].indexOf('"', index1 + 1);
		if (index1 >= 0) {
			index1++;
			QString name = lines[i].mid(index1, index2 - index1);
			res << name;
		}
	}

	return res;
}

VideoDecoder::VideoDecoder()
{
	m_last_dts = 0;
	m_file_open = false;
	m_is_packet = false;
	pFormatCtx = nullptr;
	pCodecCtx = nullptr;
	pCodec = nullptr;
	pFrame = nullptr;
	pFrameRGB = nullptr;
	pSWSCtx = nullptr;
}

VideoDecoder::VideoDecoder(const std::string& name)
{
	m_last_dts = 0;
	pFormatCtx = nullptr;
	pCodecCtx = nullptr;
	pCodec = nullptr;
	pFrame = nullptr;
	pFrameRGB = nullptr;
	pSWSCtx = nullptr;
	m_is_packet = false;

	Open(name);
}

VideoDecoder::~VideoDecoder()
{
	Close();
}

void VideoDecoder::Open(const QString& name, const QString& format, const QMap<QString, QString>& options)
{
	AVDictionary* opt = nullptr;
	for (QMap<QString, QString>::const_iterator it = options.begin(); it != options.end(); ++it) {
		av_dict_set(&opt, it.key().toLatin1().data(), it.value().toLatin1().data(), 0);
	}
	// av_dict_set(&options, "video_size", "640x480", 0);
	// av_dict_set(&options, "r", "25", 0);

	AVInputFormat* iformat = (AVInputFormat*)av_find_input_format(format.toLatin1().data());
	AVDictionary** iopt = opt ? &opt : nullptr;

	Open(name.toLatin1().data(), iformat, iopt);
}

void VideoDecoder::Open(const std::string& name, AVInputFormat* iformat, AVDictionary** options)
{
	m_file_open = true;
	AVDictionary* opts = nullptr;
	if (options)
		opts = *options;
	if (name.find(".sdp") != std::string::npos || name.find(".SDP") != std::string::npos || name.find("udp://") != std::string::npos || name.find("rtp://") != std::string::npos ||
	    name.find("rtps://") != std::string::npos || name.find("http://") != std::string::npos || name.find("https://") != std::string::npos) {
		// iformat = av_find_input_format("sdp");
		av_dict_set(&opts, "protocol_whitelist", "file,udp,rtp,http,https,tcp,tls,crypto,httpproxy", 0);
	}

	// Open video file
#if LIBAVFORMAT_VERSION_MAJOR > 52
	int err = avformat_open_input(&pFormatCtx, name.c_str(), iformat, &opts);

#else
	int err = av_open_input_file(&pFormatCtx, name.c_str(), nullptr, 0, nullptr);
#endif
	if (err != 0) {
		char error[1000];
		memset(error, 0, sizeof(error));
		av_strerror(err, error, sizeof(error));
		vip_debug("ffmpeg error: %s\n", error);
		throw std::runtime_error((std::string("Couldn't open file '") + name + "'").c_str());
	}

	// Retrieve stream information
	// TEST
	if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
		throw std::runtime_error("Couldn't find stream information");

	std::vector<AVStream*> streams;
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
		streams.push_back(pFormatCtx->streams[i]);

	// Find the first video stream
	videoStream = -1;

//#define XSTR(x) STR(x)
//#define STR(x) #x
//#pragma message "LIBAVFORMAT_VERSION_MAJOR: " XSTR(LIBAVFORMAT_VERSION_MAJOR)

#if LIBAVFORMAT_VERSION_MAJOR > 58
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
#else

	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
#if LIBAVFORMAT_VERSION_MAJOR > 52
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
#else
		if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
#endif
		{
			videoStream = i;
			break;
		}
#endif

	if (videoStream == -1)
		throw std::runtime_error("Didn't find a video stream");

	pCodec = (AVCodec*)avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
	if (!pCodec)
		throw std::runtime_error("Codec not found");
	pCodecCtx = avcodec_alloc_context3(pCodec);
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);

	// Open codec
	int res = avcodec_open2(pCodecCtx, pCodec, nullptr);
	if (res < 0)
		throw std::runtime_error("Could not open codec");

	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if (!pFrameRGB)
		throw std::runtime_error("Error in avcodec_alloc_frame()");
	pFrameRGB->format = AV_PIX_FMT_RGB24;
	pFrameRGB->width = pCodecCtx->width;
	pFrameRGB->height = pCodecCtx->height;
	if ((err = av_frame_get_buffer(pFrameRGB, 32)) < 0) {
		av_frame_free(&pFrameRGB);
		throw std::runtime_error("Failed to allocate picture");
	}

	// Initialize Context
	if (pCodecCtx->pix_fmt == AV_PIX_FMT_NONE)
		pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pSWSCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

	// Allocate video frame
	pFrame = av_frame_alloc();
	if (!pFrame)
		throw std::runtime_error("Error in avcodec_alloc_frame()");
	pFrame->format = pCodecCtx->pix_fmt;
	pFrame->width = pCodecCtx->width;
	pFrame->height = pCodecCtx->height;
	if ((err = av_frame_get_buffer(pFrame, 32)) < 0) {
		av_frame_free(&pFrame);
		throw std::runtime_error("Failed to allocate picture");
	}

	m_width = pCodecCtx->width;
	m_height = pCodecCtx->height;
	m_image = QImage(m_width, m_height, QImage::Format_ARGB32);

	double fps1 = pFormatCtx->streams[videoStream]->r_frame_rate.num * 1.0 / pFormatCtx->streams[videoStream]->r_frame_rate.den * 1.0;
	m_fps = pFormatCtx->streams[videoStream]->avg_frame_rate.num * 1.0 / pFormatCtx->streams[videoStream]->avg_frame_rate.den * 1.0;
	m_use_dts = fabs(m_fps - fps1) < 1;
	m_tech = 1 / fps1;
	m_fps = fps1;

	m_frame_pos = 0;
	m_time_pos = 0;

	m_total_time = pFormatCtx->duration / AV_TIME_BASE;
	if (m_total_time < 0.01 && !IsSequential())
		m_total_time = getTime();

	m_offset = 0;

	MoveNextFrame();

	if (!IsSequential())
		SeekTime(0);

	m_filename = name;
}

double VideoDecoder::getTime()
{
	//AVPacket p;
	//av_init_packet(&p);
	AVPacket* p = av_packet_alloc();
	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	int count = 0;
	while (av_read_frame(pFormatCtx, p) == 0) {
		if (p->stream_index == videoStream)
			count++;
		av_packet_unref(p);
	}

	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	//if (p.buf)
	//	av_packet_unref(&p);
	av_packet_unref(p);
	av_packet_free(&p);
	return count / m_fps;
}

bool VideoDecoder::IsSequential() const
{
	if (pFormatCtx && videoStream >= 0)
		return pFormatCtx->streams[videoStream]->duration < 0 && pFormatCtx->duration < 0;
	return false;
}

void VideoDecoder::Close()
{
	if (m_file_open) {
		// Free the DRGBPixel image
		if (pFrameRGB) {
			av_frame_free(&pFrameRGB);
		}

		// Free the YUV frame
		if (pFrame) {
			av_frame_free(&pFrame);
		}

		// Close the codec
		if (pCodecCtx != nullptr && pCodec != nullptr)
			avcodec_free_context(&pCodecCtx);

		// Close the video file
		if (pFormatCtx != nullptr)
			avformat_close_input(&pFormatCtx);

		if (pSWSCtx != nullptr)
			sws_freeContext(pSWSCtx);
	}

	pFormatCtx = nullptr;
	pCodecCtx = nullptr;
	pCodec = nullptr;
	pFrame = nullptr;
	pFrameRGB = nullptr;
	pSWSCtx = nullptr;
	m_file_open = false;
}

const QImage& VideoDecoder::GetCurrentFrame()
{
	return m_image;
}

int VideoDecoder::pixelType() const
{
	if (pCodecCtx)
		return pCodecCtx->pix_fmt;
	return 0;
}

void VideoDecoder::toRGB(AVFrame* frame)
{
	// m_last_dts = frame->pkt_dts;
	// int pix1 = frame->data[0][0] | (frame->data[0][1] << 8);
	// int pix2 = frame->data[0][0] | (frame->data[0][1] << 8);
	if (pCodecCtx->pix_fmt == AV_PIX_FMT_GRAY16LE || pCodecCtx->pix_fmt == AV_PIX_FMT_GRAY16BE) {
		// encode 16 bits in R and G
		unsigned char r, g, b = 0;
		uint* data = (uint*)m_image.bits();
		for (int y = 0; y < m_height; y++)
			for (int x = 0; x < m_width; x++, ++data) {
				r = (frame->data[0] + y * frame->linesize[0])[x * 2];
				g = (frame->data[0] + y * frame->linesize[0])[x * 2 + 1];
				*data = qRgb(r, g, b);
			}
	}
	else {
		unsigned char r, g, b;
		uint* data = (uint*)m_image.bits();
		for (int y = 0; y < m_height; y++)
			for (int x = 0; x < m_width; x++, ++data) {
				r = (frame->data[0] + y * frame->linesize[0])[x * 3];
				g = (frame->data[0] + y * frame->linesize[0])[x * 3 + 1];
				b = (frame->data[0] + y * frame->linesize[0])[x * 3 + 2];
				// m_image.setPixel( x,y, qRgb(r,g,b) );
				*data = qRgb(r, g, b);
			}
	}
}

static int decode(AVCodecContext* dec_ctx, AVFrame* frame, int* got_frame, AVPacket* pkt)
{
	int used = 0;
	if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		used = avcodec_send_packet(dec_ctx, pkt);
		if (used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF) {
		}
		else {
			// if (used >= 0)
			//	pkt->size = 0;
			used = avcodec_receive_frame(dec_ctx, frame);
			if (used >= 0)
				*got_frame = 1;
			//             if (used == AVERROR(EAGAIN) || used == AVERROR_EOF)
			//                 used = 0;
		}
	}
	return used;
}

bool VideoDecoder::MoveNextFrame()
{
	//AVPacket p;
	//av_init_packet(&p);
	AVPacket* p = av_packet_alloc();

	int finish = 0;
	while (finish == 0) {
		av_packet_unref(p);
		
		if (av_read_frame(pFormatCtx, p) < 0) {
			//av_init_packet(&p);
		}

		int ret = decode(pCodecCtx, pFrame, &finish, p);
		if (ret == AVERROR(EAGAIN))
			continue;
		if (ret <= 0 && pFrame->data[0] == NULL) {
			//if (p.buf) {
				av_packet_unref(p);
			av_packet_free(&p);
			//}
			m_frame_pos = -1; // in case of error, invalidate m_frame_pos to be sure to call av_seek_frame next time
			return false;
		}
	}

	// Convert YUV->DRGBPixel
	if (pFrame->data[0]) {
		if (pCodecCtx->pix_fmt != AV_PIX_FMT_GRAY16LE && pCodecCtx->pix_fmt != AV_PIX_FMT_GRAY16BE) {
			sws_scale(pSWSCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
			toRGB(pFrameRGB);
		}
		else {
			toRGB(pFrame);
		}
	}
	m_last_dts = pFrame->pkt_dts;
	m_frame_pos++;
	m_time_pos = m_frame_pos * 1.0 * m_tech;
	//if (p.buf)
		av_packet_unref(p);
	av_packet_free(&p);
	return true;
}

double VideoDecoder::GetRate() const
{
	if (m_file_open)
		return pFormatCtx->bit_rate;
	else
		return 0;
}

void VideoDecoder::SeekTime2(double time)
{
	SeekTime(time);
}

void VideoDecoder::SeekTime(double time)
{
	SeekFrame(static_cast<qint64>(floor(time * m_fps + 0.5)));
}

const QImage& VideoDecoder::GetFrameByTime(double time)
{
	qint64 number = static_cast<qint64>(floor(time * m_fps + 0.5));
	return GetFrameByNumber(number);
}

const QImage& VideoDecoder::GetFrameByNumber(qint64 number)
{
	if ((qint64)number + 1 == m_frame_pos)
		return m_image;

	if ((qint64)number != m_frame_pos)
		SeekTime(number / m_fps);

	MoveNextFrame();

	return m_image;
}

static int64_t FrameToPts(AVStream* pavStream, int64_t frame)
{
	return ((frame)*pavStream->r_frame_rate.den * pavStream->time_base.den) / (int64_t(pavStream->r_frame_rate.num) * pavStream->time_base.num);
}

void VideoDecoder::SeekFrame(qint64 pos)
{
	if (pos == 0) {
		av_seek_frame(pFormatCtx, videoStream, (int64_t)(pos) * 12800ull, AVSEEK_FLAG_BACKWARD);
		avcodec_flush_buffers(pCodecCtx);
		m_frame_pos = 0;
		m_time_pos = 0;
		return;
	}
	if (pos + 1 == m_frame_pos)
		return;

	int64_t target_dts = 0;
	if (!m_use_dts) {
		// Incoherent fps (like WEST operational camera),
		// this method gives the best result
		target_dts = (int64_t)(((int64_t)pos - 1) * AV_TIME_BASE * m_tech);
		if (av_seek_frame(pFormatCtx, -1, target_dts, AVSEEK_FLAG_BACKWARD) < 0)
			return;
	}
	else {
		// Generic (and usually valid) way to seek
		target_dts = FrameToPts(pFormatCtx->streams[videoStream], (int64_t)pos - 1);
		if (av_seek_frame(pFormatCtx, videoStream, target_dts, AVSEEK_FLAG_BACKWARD) < 0)
			return;
	}

	avcodec_flush_buffers(pCodecCtx);

	if (pos == 0)
		return;

	AVPacket* p = av_packet_alloc();
	//av_init_packet(&p);

	while (true) {
		int finish = 0;
		while (finish == 0) {
			av_packet_unref(p);
			
			if (av_read_frame(pFormatCtx, p) < 0) {
				//av_init_packet(&p);
			}

			int ret = decode(pCodecCtx, pFrame, &finish, p);
			if (ret == AVERROR(EAGAIN))
				continue;
			if (ret <= 0 && pFrame->data[0] == NULL) {
				//if (p.buf) 
				//	av_packet_unref(&p);
				av_packet_unref(p);
				av_packet_free(&p);
				
				m_frame_pos = -1; // in case of error, invalidate m_frame_pos to be sure to call av_seek_frame next time
				return;
			}
		}

		if (pFrame->pkt_dts >= target_dts) {
			break;
		}
	}
	// Convert YUV -> RGB
	if (pFrame->data[0]) {
		if (pCodecCtx->pix_fmt != AV_PIX_FMT_GRAY16LE && pCodecCtx->pix_fmt != AV_PIX_FMT_GRAY16BE) {
			sws_scale(pSWSCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
			toRGB(pFrameRGB);
		}
		else {
			toRGB(pFrame);
		}
	}
	m_frame_pos = (qint64)pos;
	m_time_pos = pos / m_fps;
	//if (p.buf)
	//	av_packet_unref(&p);
	av_packet_unref(p);
	av_packet_free(&p);
}

double VideoDecoder::GetTotalTime() const
{
	return m_total_time;
}

int VideoDecoder::GetWidth() const
{
	return m_width;
}

int VideoDecoder::GetHeight() const
{
	return m_height;
}

std::string VideoDecoder::GetFileName() const
{
	return m_filename;
}

double VideoDecoder::GetTimePos() const
{
	return m_time_pos;
}

qint64 VideoDecoder::GetCurrentFramePos() const
{
	return m_frame_pos;
}

double VideoDecoder::GetFps() const
{
	return m_fps;
}



VipMPEGLoader::VipMPEGLoader(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
  , m_thread(this)
  , m_last_dts(0)
  , m_sampling_time(0)
  , m_count(0)
{
	this->outputAt(0)->setData(QVariant::fromValue(vipToArray(QImage(10, 10, QImage::Format_ARGB32))));
	m_decoder = new VideoDecoder();
}

VipMPEGLoader::~VipMPEGLoader()
{
	close();
	delete m_decoder;
}

qint32 VipMPEGLoader::fullFrameWidth() const
{
	return m_decoder->GetWidth();
}
qint32 VipMPEGLoader::fullFrameHeight() const
{
	return m_decoder->GetHeight();
}

void VipMPEGLoader::setDrawFunction(const draw_function& f)
{
	m_draw_function = f;
}
const VipMPEGLoader::draw_function& VipMPEGLoader::drawFunction() const
{
	return m_draw_function;
}

QStringList VipMPEGLoader::listDevices()
{
	return VideoDecoder::list_devices();
}

static QStringList _open_devices;
QMutex _open_devices_mutex;

bool VipMPEGLoader::open(const QString& name, const QString& format, const QMap<QString, QString>& options)
{
	if (this->isOpen()) {
		this->stopStreaming();
		m_decoder->Close();
		setOpenMode(NotOpen);
		m_count = 0;
	}

	try {

		// compute the new path for this device, which concatenate all parameters
		QString new_path = name + "|" + format;
		vip_debug("%s\n", new_path.toLatin1().data());
		{
			QMutexLocker lock(&_open_devices_mutex);
			if (_open_devices.contains(new_path)) {
				setError("Device " + new_path + " already opened");
				return false;
			}
			_open_devices.push_back(new_path);
			m_device_path = new_path;
		}

		for (QMap<QString, QString>::const_iterator it = options.begin(); it != options.end(); ++it) {
			new_path += "|" + it.key() + "|" + it.value();
		}
		this->setPath(new_path);

		QString _name = name;
		_name.remove("video=");
		setAttribute("Name", _name);

		m_decoder->Close();
		m_decoder->Open(name, format, options);
		m_sampling_time = 1.0 / m_decoder->GetFps();
		qint64 size = m_decoder->GetTotalTime() * m_decoder->GetFps();

		this->setTimeWindows(0, size, qint64(m_sampling_time * qint64(1000000000)));
		this->setOpenMode(VipIODevice::ReadOnly);

		m_decoder->MoveNextFrame();
		VipAnyData out = create(QVariant::fromValue(fromImage(m_decoder->GetCurrentFrame())));
		if (deviceType() == Sequential) {
			out.setTime(vipGetNanoSecondsSinceEpoch());
			out.setAttribute("Number", 0);
		}
		outputAt(0)->setData(out);

		return true;
	}
	catch (const std::exception& e) {
		setError(e.what());
		return false;
	}
	VIP_UNREACHABLE();
	// return false;
}

bool VipMPEGLoader::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::ReadOnly)
		return false;

	if (this->isOpen()) {
		this->stopStreaming();
		m_decoder->Close();
		setOpenMode(NotOpen);
		m_count = 0;
	}

	QString file = this->removePrefix(path());

	if (file.contains("|")) {
		// Try to open the path as a sequential device
		QStringList lst = file.split("|");
		if (lst.size() >= 2) {
			QString name = lst[0];
			QString format = lst[1];
			QMap<QString, QString> options;
			for (int i = 2; i < lst.size(); i += 2) {
				options[lst[i]] = lst[i + 1];
			}
			if (open(name, format, options)) {
				setOpenMode(ReadOnly);
				return true;
			}
		}
	}

	try {
		m_decoder->Close();
		m_decoder->Open(file.toLatin1().data());
		m_sampling_time = 1.0 / m_decoder->GetFps();
		qint64 size = m_decoder->GetTotalTime() * m_decoder->GetFps();

		this->setTimeWindows(0, size, qint64(m_sampling_time * qint64(1000000000)));

		QFileInfo info(file);

		this->setAttribute("Date", info.lastModified().toString());
		this->setOpenMode(mode);

		m_decoder->MoveNextFrame();
		VipAnyData out = create(QVariant::fromValue(fromImage(m_decoder->GetCurrentFrame())));
		if (deviceType() == Sequential) {
			out.setTime(vipGetNanoSecondsSinceEpoch());
			out.setAttribute("Number", 0);
		}
		outputAt(0)->setData(out);
		setOpenMode(ReadOnly);

		return true;
	}
	catch (const std::exception& e) {
		setError(e.what());
		return false;
	}

	return false;
}

VipNDArray VipMPEGLoader::fromImage(const QImage& img) const
{
	if (m_decoder->pixelType() == AV_PIX_FMT_GRAY16LE || m_decoder->pixelType() == AV_PIX_FMT_GRAY16BE) {

		VipNDArrayType<unsigned short> res(vipVector(img.height(), img.width()));
		const uint* pix = (const uint*)img.bits();
		for (int y = 0; y < img.height(); ++y)
			for (int x = 0; x < img.width(); ++x) {
				unsigned char r = qRed(pix[x + y * img.width()]);
				unsigned char g = qGreen(pix[x + y * img.width()]);
				res(y, x) = r | (g << 8);
			}
		return res;
	}
	else {
		if (m_draw_function) {
			QImage tmp = img;
			m_draw_function(tmp);
			return vipToArray(tmp);
		}

		return vipToArray(img);
	}
}

void VipMPEGLoader::close()
{
	this->stopStreaming();
	m_decoder->Close();
	setOpenMode(NotOpen);
	m_count = 0;

	QMutexLocker lock(&_open_devices_mutex);
	_open_devices.removeOne(m_device_path);
	m_device_path.clear();
}

VipMPEGLoader::DeviceType VipMPEGLoader::deviceType() const
{
	if (isOpen() && m_decoder->IsSequential())
		return Sequential;
	else
		return Temporal;
}

bool VipMPEGLoader::readData(qint64 time)
{
	try {
		// temporal device (mpeg file)
		if (deviceType() == Temporal) {
			const QImage img = m_decoder->GetFrameByTime(time * 0.000000001);
			VipNDArray ar = fromImage(img);

			VipAnyData out = create(QVariant::fromValue(ar));
			outputAt(0)->setData(out);
			return true;
		}
		else // sequential device (example: web cam)
		{
			if (m_decoder->MoveNextFrame()) {
				if (m_last_dts == m_decoder->LastReadDTS())
					return false;

				m_last_dts = m_decoder->LastReadDTS();
				VipNDArray ar = fromImage(m_decoder->GetCurrentFrame());

				VipAnyData out = create(QVariant::fromValue(ar));
				out.setTime(vipGetNanoSecondsSinceEpoch());
				out.setAttribute("Number", ++m_count);
				outputAt(0)->setData(out);
				return true;
			}
			else
				return false;
		}
	}
	catch (const std::exception& e) {
		setError(e.what());
		return false;
	}
}

bool VipMPEGLoader::enableStreaming(bool enable)
{
	m_thread.stop = true;
	m_thread.wait();

	if (enable) {
		m_count = 0;
		m_thread.start();
	}
	return true;
}
