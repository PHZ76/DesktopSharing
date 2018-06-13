#ifndef _H264_ENCODER_H
#define _H264_ENCODER_H

#include <cstdint>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#include <libavutil/fifo.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libavutil/opt.h>
}

struct VideoConfig
{
	uint32_t width = 1920;
	uint32_t height = 1080;
	uint32_t bitrate = 4000000;
	uint32_t framerate = 25;
	uint32_t gop = 25;
};

class H264Encoder 
{
public:
	H264Encoder & operator=(const H264Encoder &) = delete;
	H264Encoder(const H264Encoder &) = delete;
	static H264Encoder& Instance();
	~H264Encoder();

	bool Init(struct VideoConfig vc);
	void Exit();

	AVCodecContext* getAVCodecContext() const
	{ return _vCodecCtx; }

	AVPacket* EncodeVideo(const uint8_t *rgba, uint32_t width, uint32_t height, uint64_t pts = 0);

private:
	H264Encoder();

	bool _isInitialized = false;
	struct VideoConfig _videoConfig;

	AVCodecContext *_vCodecCtx = nullptr;
	SwsContext* _swsCtx = nullptr;
	AVFrame* _yuvFrame = nullptr;
	AVPacket* _vPkt = nullptr;
	uint32_t  _pts = 0;
};

#endif
