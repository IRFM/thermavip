#include "MPEGLoader.h"
#include "VideoDecoder.h"

#include <QFileInfo>
#include <QDateTime>

MPEGLoader::MPEGLoader(QObject * parent)
	:VipTimeRangeBasedGenerator(parent), m_thread(this), m_last_dts(0),m_sampling_time(0), m_count(0)
{
	this->outputAt(0)->setData(QVariant::fromValue(vipToArray(QImage(10, 10, QImage::Format_ARGB32))));
	m_decoder = new VideoDecoder();
}

MPEGLoader::~MPEGLoader()
{
	close();
	delete m_decoder;
}

qint32 MPEGLoader::fullFrameWidth() const { return m_decoder->GetWidth(); }
qint32 MPEGLoader::fullFrameHeight() const { return m_decoder->GetHeight(); }

QStringList MPEGLoader::listDevices()
{
	return VideoDecoder::list_devices();
}

static QStringList _open_devices;
QMutex _open_devices_mutex;

bool MPEGLoader::open(const QString & name, const QString & format, const QMap<QString, QString> & options)
{
	if (this->isOpen())
	{
		this->stopStreaming();
		m_decoder->Close();
		setOpenMode(NotOpen);
		m_count = 0;
	}

	try {

		//compute the new path for this device, which concatenate all parameters
		QString new_path = name + "|" + format;
		printf("%s\n", new_path.toLatin1().data());
		{
			QMutexLocker lock(&_open_devices_mutex);
			if (_open_devices.contains(new_path)) {
				setError("Device " + new_path + " already opened");
				return false;
			}
			_open_devices.push_back(new_path);
			m_device_path = new_path;
		}

		for (QMap<QString, QString>::const_iterator it = options.begin(); it != options.end(); ++it)
		{
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
		if (deviceType() == Sequential)
		{
			out.setTime(vipGetNanoSecondsSinceEpoch());
			out.setAttribute("Number", 0);
		}
		outputAt(0)->setData(out);

		return true;

	}
	catch (const std::exception & e)
	{
		setError(e.what());
		return false;
	}
	VIP_UNREACHABLE();
	//return false;

}

bool MPEGLoader::open(VipIODevice::OpenModes mode)
{
	if (mode != VipIODevice::ReadOnly)
		return false;

	if (this->isOpen())
	{
		this->stopStreaming();
		m_decoder->Close();
		setOpenMode(NotOpen);
		m_count = 0;
	}

	QString file = this->removePrefix(path());

	if (file.contains("|"))
	{
		// Try to open the path as a sequential device
		QStringList lst = file.split("|");
		if (lst.size() >= 2)
		{
			QString name = lst[0];
			QString format = lst[1];
			QMap<QString, QString> options;
			for (int i = 2; i < lst.size(); i += 2)
			{
				options[lst[i]] = lst[i + 1];
			}
			if (open(name, format, options))
			{
				setOpenMode(ReadOnly);
				return true;
			}
		}
	}


	try{
		m_decoder->Close();
		m_decoder->Open(file.toLatin1().data());
		m_sampling_time = 1.0/ m_decoder->GetFps();
		qint64 size = m_decoder->GetTotalTime() * m_decoder->GetFps();


		this->setTimeWindows(0, size, qint64(m_sampling_time * qint64(1000000000)));
		
		

		QFileInfo info(file);

		this->setAttribute("Date",info.lastModified ().toString ());
		this->setOpenMode(mode);

		m_decoder->MoveNextFrame();
		VipAnyData out = create(QVariant::fromValue(fromImage(m_decoder->GetCurrentFrame())));
		if (deviceType() == Sequential)
		{
			out.setTime(vipGetNanoSecondsSinceEpoch());
			out.setAttribute("Number", 0);
		}
		outputAt(0)->setData(out);
		setOpenMode(ReadOnly);

		return true;

	}
	catch (const std::exception & e)
	{
		setError(e.what());
		return false;
	}

	return false;

}

VipNDArray MPEGLoader::fromImage(const QImage & img) const
{
	if (m_decoder->pixelType() == AV_PIX_FMT_GRAY16LE || m_decoder->pixelType() == AV_PIX_FMT_GRAY16BE) {

		VipNDArrayType<unsigned short> res(vipVector(img.height(), img.width()));
		const uint * pix = (const uint *)img.bits();
		for(int y=0; y < img.height(); ++y)
			for (int x = 0; x < img.width(); ++x) {
				unsigned char r = qRed(pix[x + y*img.width()]);
				unsigned char g = qGreen(pix[x + y*img.width()]);
				res(y, x) = r | (g << 8);
			}
		return res;
	}
	else {
		return vipToArray(img);
	}
}

void MPEGLoader::close()
{
	this->stopStreaming();
	m_decoder->Close();
	setOpenMode(NotOpen);
	m_count = 0;

	QMutexLocker lock(&_open_devices_mutex);
	_open_devices.removeOne(m_device_path);
	m_device_path.clear();
}

MPEGLoader::DeviceType MPEGLoader::deviceType() const
{
	if (isOpen() && m_decoder->IsSequential())
		return Sequential;
	else
		return Temporal;
}

bool MPEGLoader::readData(qint64 time)
{ 
	try{
		//temporal device (mpeg file)
		if (deviceType() == Temporal)
		{
			const QImage img = m_decoder->GetFrameByTime(time*0.000000001);
			VipNDArray ar = fromImage(img);

			VipAnyData out = create(QVariant::fromValue(ar));
			outputAt(0)->setData(out);
			return true;
		}
		else //sequential device (example: web cam)
		{
			if (m_decoder->MoveNextFrame())
			{
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
	catch(const std::exception & e)
	{
		setError(e.what());
		return false;
	}
}

bool MPEGLoader::enableStreaming(bool enable)
{
	m_thread.stop = true;
	m_thread.wait();

	if (enable)
	{
		m_count = 0;
		m_thread.start();
	}
	return true;
}




























class VideoGrabber
{
public:
	VideoGrabber();
	VideoGrabber(const std::string & name);
	~VideoGrabber();
	void Open(const std::string & name, AVInputFormat * format = nullptr, AVDictionary ** options = nullptr);
	void Open(const QString & name, const QString & format, const QMap<QString, QString> & options);
	void Close();

	//Ffmpeg main struct
	AVFormatContext* GetContext() const { return pFormatCtx; }
	//renvoie l'image actuelle
	const VipNDArray & GetCurrentFrame();
	const VipNDArray & GetFrameByTime(double time);
	const VipNDArray & GetFrameByNumber(int num);
	/**
	Go to next frame and returns the packet dts or -1 on error.
	If target_dts is -1, the image is uncompressed and read.
	If target_dts is not -1, the image is only read and uncompressed only if read dts is < target_dts
	*/
	size_t MoveNextFrame(size_t target_dts = -1);
	//position actuelle (en s), peut etre approximatif
	double GetTimePos() const;
	//position actuelle (en frame), peut etre approximatif
	long int GetCurrentFramePos()const;
	int GetFrameCount() const { return m_frame_count; }
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
	double GetOffset() const { return m_offset; }
	//nom du fichier ouvert
	std::string GetFileName() const;
	//se deplace au temps donne (en s), peut etre approximatif
	void SeekTime(double time);
	void SeekFrame(int pos);

protected:

	bool computeNextFrame();
	double getTime();
	VipNDArray toArray(AVFrame * frame);
	void free_packet();

	VipNDArray		m_image;
	double			m_current_time;
	std::string	m_filename;
	int			m_width;
	int			m_height;
	double			m_fps;
	double			m_tech;
	int				m_frame_pos;
	int				m_frame_count;
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
	SwsContext		*pSWSCtx;
	uint8_t			*buffer;
	int				numBytes;
	AVPacket        packet;
	int             frameFinished;
};





#include <stdexcept>
#include <fstream>
#include <qfile.h>

static int __ReadFunc(void* ptr, uint8_t* buf, int buf_size)
{
	QIODevice* pStream = reinterpret_cast<QIODevice*>(ptr);
	return pStream->read((char*)buf, buf_size);
}

// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE
int64_t __SeekFunc(void* ptr, int64_t pos, int whence)
{
	
	QIODevice* pStream = reinterpret_cast<QIODevice*>(ptr);

	long long res;
	if (whence == AVSEEK_SIZE) {
		res = pStream->size();
	}
	else if (whence == SEEK_SET) {
		pStream->seek(pos);
		res = pStream->pos();
	}
	else if (whence == SEEK_CUR) {
		pStream->seek(pos + pStream->pos());
		res = pStream->pos();
	}
	else {
		pStream->seek(pStream->size() - pos);
		res = pStream->pos();
	}
	return res;
}


static void __destruct(AVPacket * pkt){
	pkt->data = nullptr;
	pkt->size = 0;
}
static void __init_packet(AVPacket * pkt){
	pkt->pts = 0;
	pkt->dts = 0;
	pkt->pos = -1;
	pkt->duration = 0;
	pkt->flags = 0;
	pkt->stream_index = 0;
}
/*static QString _stderr;

void log_to_array(void * 	avcl,
	int 	level,
	const char * 	fmt,
	va_list 	vl
)
{
	_stderr += QString::vasprintf(fmt, vl);
}*/
void VideoGrabber::free_packet()
{
	if (packet.data != nullptr && packet.size > 0)
		//if(strcmp((const char*)packet.data,"")!=0)
		av_free(packet.data);

	packet.size = 0;
	packet.data = 0;
	__init_packet(&packet);
}

VideoGrabber::VideoGrabber()
{
	m_file_open = false;
	m_is_packet = false;
	pFormatCtx = nullptr;
	pCodecCtx = nullptr;
	pCodec = nullptr;
	pFrame = nullptr;
	pSWSCtx = nullptr;
	buffer = nullptr;
	pFrameRGB = nullptr;
}
VideoGrabber::VideoGrabber(const std::string & name)
{
	pFormatCtx = nullptr;
	pCodecCtx = nullptr;
	pCodec = nullptr;
	pFrame = nullptr;
	pSWSCtx = nullptr;
	pFrameRGB = nullptr;
	buffer = nullptr;
	m_is_packet = false;
	Open(name);
}
VideoGrabber::~VideoGrabber()
{
	Close();
}

void VideoGrabber::Open(const QString & name, const QString & format, const QMap<QString, QString> & options)
{
	AVDictionary* opt = nullptr;
	for (QMap<QString, QString>::const_iterator it = options.begin(); it != options.end(); ++it)
	{
		av_dict_set(&opt, it.key().toLatin1().data(), it.value().toLatin1().data(), 0);
	}
	
	AVInputFormat *iformat = av_find_input_format(format.toLatin1().data());
	AVDictionary **iopt = opt ? &opt : nullptr;

	Open(name.toLatin1().data(), iformat, iopt);
}

void VideoGrabber::Open(const std::string & name, AVInputFormat * iformat, AVDictionary ** options)
{
	/*AVFormatContext *formatC = nullptr;// avformat_alloc_context();
	AVDictionary* options = nullptr;
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

	__init_packet(&packet);
	packet.data = nullptr;
	// Open video file
#if LIBAVFORMAT_VERSION_MAJOR > 52
	if (avformat_open_input(&pFormatCtx,
		name.c_str(),
		iformat,
		options) != 0)
		throw std::runtime_error((std::string("Couldn't open file '") + name + "'").c_str());
#else
	if (av_open_input_file(&pFormatCtx, name.c_str(), nullptr, 0, nullptr) != 0)
		throw std::runtime_error((std::string("Couldn't open file '") + name + "'").c_str());
#endif

	// Retrieve stream information
	//TEST
	if (avformat_find_stream_info(pFormatCtx, nullptr)<0)
		throw std::runtime_error("Couldn't find stream information");

	// Find the first video stream
	videoStream = -1;
	for (unsigned int i = 0; i<pFormatCtx->nb_streams; i++)
#if LIBAVFORMAT_VERSION_MAJOR > 52
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
#else
		if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
#endif
		{
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		throw std::runtime_error("Didn't find a video stream");

	//pFormatCtx->cur_st = pFormatCtx->streams[0];


	// Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == nullptr)
		throw std::runtime_error("Codec not found");

	// Open codec
	int res = avcodec_open2(pCodecCtx, pCodec, nullptr);
	if (res<0)
		throw std::runtime_error("Could not open codec");

	// Allocate video frame
	pFrame = av_frame_alloc();
	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == nullptr)
		throw std::runtime_error("Error in avcodec_alloc_frame()");
	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == nullptr)
		throw std::runtime_error("Error in avcodec_alloc_frame()");

	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
		pCodecCtx->height);

	buffer = (uint8_t*)av_malloc(numBytes);

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
		pCodecCtx->width, pCodecCtx->height);

	//Initialize Context
	if (pCodecCtx->pix_fmt == AV_PIX_FMT_NONE)
		pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pSWSCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);


	m_width = pCodecCtx->width;
	m_height = pCodecCtx->height;
	m_image = VipNDArray(QMetaType::UShort,vipVector(m_height,m_width));

	m_fps = pFormatCtx->streams[videoStream]->r_frame_rate.num*1.0 / pFormatCtx->streams[videoStream]->r_frame_rate.den*1.0;
	m_frame_count = pFormatCtx->streams[videoStream]->nb_frames;
	m_tech = 1 / m_fps;

	m_frame_pos = 0;
	m_time_pos = 0;

	m_total_time = (double)pFormatCtx->duration / AV_TIME_BASE;
	//if (m_total_time < 0.01 )
	//	m_total_time = getTime();

	m_offset = 0;
	//SeekTime(0);
	//MoveNextFrame();
	m_filename = name;
}


double VideoGrabber::getTime()
{
	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	int count = 0;
	while (av_read_frame(pFormatCtx, &packet) == 0)
	{
		if (packet.stream_index == videoStream)
			count++;
	}

	av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
	return count / m_fps;
}
void VideoGrabber::Close()
{
	if (m_file_open)
	{
		// Free the DRGBPixel image
		if (pFrameRGB != nullptr)
		{
			av_free((AVPicture*)pFrameRGB);
		}

		// Free the YUV frame
		if (pFrame != nullptr)
		{
			av_free((AVPicture*)pFrame);
		}

		// Close the codec
		if (pCodecCtx != nullptr && pCodec != nullptr) avcodec_close(pCodecCtx);

		// Close the video file
		if (pFormatCtx != nullptr) avformat_close_input(&pFormatCtx);

		if (pSWSCtx != nullptr) sws_freeContext(pSWSCtx);

		if (packet.data)
			av_free_packet(&packet);
	}

	pFormatCtx = nullptr;
	pCodecCtx = nullptr;
	pCodec = nullptr;
	pFrame = nullptr;
	pFrameRGB = nullptr;
	//buffer = nullptr;
	pSWSCtx = nullptr;
	m_file_open = false;
}


const VipNDArray &  VideoGrabber::GetCurrentFrame()
{
	return m_image;
}


VipNDArray VideoGrabber::toArray(AVFrame * frame)
{
	//convert to rgb
	//int r = sws_scale(pSWSCtx, frame->data, frame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

	VipNDArray res(QMetaType::UShort, vipVector(frame->height, frame->width));
	unsigned short * data = (unsigned short*)res.data();

	for (int y = 0; y < frame->height; ++y) {
		uchar * d1 = frame->data[1] + y*frame->linesize[1];
		uchar * d2 = frame->data[2] + y*frame->linesize[2];
		for (int i = 0; i < frame->width; ++i) {
			data[i + y * frame->width] = d1[i] | (d2[i] << 8);
		}
	}
	/*for (int y = 0; y<pCodecCtx->height; y++)
		for (int x = 0; x<pCodecCtx->width; x++, ++data)
		{
			*data = (pFrameRGB->data[0] + y*pFrameRGB->linesize[0])[x * 3] | ((pFrameRGB->data[0] + y*pFrameRGB->linesize[0])[x * 3 +1] << 8);
		}*/
	
	return res;
}


size_t VideoGrabber::MoveNextFrame(size_t target_dts )
{
	size_t res = static_cast<size_t>(-1);

	if (!m_is_packet)
	{
		if (packet.data && packet.size >0)
			av_free_packet(&packet);
		//pFormatCtx->cur_pkt.destruct=nullptr;
		if (av_read_frame(pFormatCtx, &packet) < 0)
			return static_cast<size_t>(-1);
		res = packet.dts;
	}

	m_is_packet = false;

	while (packet.stream_index != videoStream)
	{
		av_free_packet(&packet);
		//pFormatCtx->cur_pkt.destruct=nullptr;
		if (av_read_frame(pFormatCtx, &packet) < 0)
			return static_cast<size_t>(-1);
		res = packet.dts;
	}

	if (target_dts != -1) {
		if (res >= target_dts) {
			m_is_packet = true;
			return res;
		}
	}

	int ffinish = 1;

	while (ffinish != 0)
	{

#if LIBAVFORMAT_VERSION_MAJOR > 52
		/* int ret =*/ avcodec_decode_video2(pCodecCtx,
			pFrame,
			&ffinish,
			&packet);
#else
		int ret = avcodec_decode_video(pCodecCtx, pFrame, &ffinish, packet.data, packet.size);
#endif

		// Did we get a video frame?
		if (ffinish != 0)
		{
			if (target_dts == -1) {
				//convert image
				if (pFrame->data)
				{
					m_image = toArray(pFrame);
				}
			}

			break;


		}
		else
		{
			packet.stream_index = -1;
			while (packet.stream_index != videoStream)
			{
				av_free_packet(&packet);
				//pFormatCtx->cur_pkt.destruct=nullptr;
				if (av_read_frame(pFormatCtx, &packet) < 0)
					return static_cast<size_t>(-1);
				res = packet.dts;

				if (target_dts != -1) {
					if (res >= target_dts) {
						m_is_packet = true;
						return res;
					}
				}
			}

		}
	}

	av_free_packet(&packet);
	m_is_packet = false;
	m_frame_pos++;
	
	return res;



}

void VideoGrabber::SeekFrame(int frame)
{
	if (m_frame_pos == frame)
		return;
	int seek_frame = frame * 12800;
	int ret = av_seek_frame(pFormatCtx, videoStream, seek_frame, AVSEEK_FLAG_BACKWARD);
	//avcodec_flush_buffers(pFormatCtx->streams[videoStream]->codec);
	if (ret < 0)
		return;

	while (true) {
		size_t dts = MoveNextFrame(seek_frame);
		if (dts == -1)
			return;
		if (dts >= seek_frame)
			break;
	}
	m_frame_pos = frame;
}

const VipNDArray &  VideoGrabber::GetFrameByNumber(int number)
{
	if (number + 1 == m_frame_pos)
		return m_image;
	if (number != m_frame_pos)
		SeekFrame(number);
	MoveNextFrame();
	return m_image;

}

int VideoGrabber::GetWidth()const { return m_width; }
int VideoGrabber::GetHeight()const { return m_height; }
std::string VideoGrabber::GetFileName() const { return m_filename; }
long int VideoGrabber::GetCurrentFramePos()const { return m_frame_pos; }
double VideoGrabber::GetFps()const { return m_fps; }


