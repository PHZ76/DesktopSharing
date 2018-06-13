#ifndef FFMPEG_RTMP_PUSHER_H
#define FFMPEG_RTMP_PUSHER_H

#include <string>
#include <memory>
#include <cstdint>
#include <mutex>
#include "xop/Timestamp.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

class RtmpPusher
{
public:
	RtmpPusher();
	~RtmpPusher();

	bool openUrl(const char* url);
	void close();

	bool addStream(AVCodecContext * codecCtx); // 编码器信息

	bool isConnected() const 
	{ return _isConnected; }

	bool pushFrame(int chn, AVPacket *pkt);

private:
	bool writeHeader();
	std::string getError(int errnum);

	std::string _url;
	AVFormatContext* _formatCtx = nullptr;

	xop::Timestamp _startTimePoint; //时间戳
    
	bool _isConnected = false;
	bool _hasHeader = false;
	std::mutex _mutex;
};

#endif

