#ifndef _AAC_ENCODER_H
#define _AAC_ENCODER_H

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
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

struct AudioConfig
{
	uint32_t channels = 2;
	uint32_t samplerate = 44100;
	uint32_t bitrate = 64000;
	uint32_t frameLength = 1024;
};


class AACEncoder
{
public:
	AACEncoder & operator=(const AACEncoder &) = delete;
	AACEncoder(const AACEncoder &) = delete;
	static AACEncoder& instance();
	~AACEncoder();

	bool init(struct AudioConfig ac);
	void exit();

	AVCodecContext* getAVCodecContext() const
	{ return _aCodecCtx; }

	AVPacket* encodeAudio(const uint8_t *pcm);

private:
	AACEncoder();

	bool _isInitialized = false;
	AudioConfig _audioConfig;

	AVCodecContext *_aCodecCtx = nullptr;
	SwrContext* _swrCtx = nullptr;
	AVFrame* _pcmFrame = nullptr;
	AVPacket* _aPkt = nullptr;
	uint32_t _aPts = 0;
};

#endif