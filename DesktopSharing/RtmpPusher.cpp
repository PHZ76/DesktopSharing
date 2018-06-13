#include "RtmpPusher.h"

#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avutil.lib")  
#pragma comment (lib, "avcodec.lib")  

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[DEBUG] [%s:%s:%d] " format "", \
    __FILE__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}

RtmpPusher::RtmpPusher()
{

}

RtmpPusher::~RtmpPusher()
{
	this->close();
}

bool RtmpPusher::openUrl(const char* url)
{
	int ret = avformat_alloc_output_context2(&_formatCtx, 0, "flv", url);
	if (ret != 0)
	{
		LOG("%s\n", getError(ret).c_str());
		return false;
	}

    _url = url;
	_isConnected = true;
	return true;
}

void RtmpPusher::close()
{
	if (_formatCtx)
	{
		avformat_close_input(&_formatCtx);
		_formatCtx = nullptr;
	}

	_isConnected = false;
	_hasHeader = false;
}

bool RtmpPusher::addStream(AVCodecContext* codecCtx)
{
	if (!_formatCtx || !codecCtx)
		return false;

	AVStream *stream = avformat_new_stream(_formatCtx, codecCtx->codec);
	if (!stream)
	{
		LOG("Failed allocating output stream.\n");
		return false;
	}

	avcodec_parameters_from_context(stream->codecpar, codecCtx);
	
	return true;
}

bool RtmpPusher::pushFrame(int chn, AVPacket *pkt)
{
	if (!_isConnected)
		return false;
	
	std::lock_guard<std::mutex> locker(_mutex);

	if (!_hasHeader)
	{
		if (!writeHeader())
		{
			return false;
		}

		_startTimePoint.reset();
	}

	pkt->stream_index = chn; 
	pkt->pts = pkt->dts = pkt->duration = _startTimePoint.elapsed();

	int ret = av_interleaved_write_frame(_formatCtx, pkt);
	if (ret != 0)
	{
		LOG("%s\n", getError(ret).c_str());
		return false;
	}

	return true;
}

bool RtmpPusher::writeHeader()
{
	int ret = avio_open(&_formatCtx->pb, _url.c_str(), AVIO_FLAG_WRITE);
	if (ret != 0)
	{
		LOG("%s\n", getError(ret).c_str());
		return false;
	}

	ret = avformat_write_header(_formatCtx, NULL);
	if (ret != 0)
	{
		LOG("%s\n", getError(ret).c_str());
		return false;
	}

	_hasHeader = true;
	return true;
}

std::string RtmpPusher::getError(int errnum)
{
	char buf[1024] = { 0 };
	av_strerror(errnum, buf, sizeof(buf) - 1);
	return std::string(buf);
}

