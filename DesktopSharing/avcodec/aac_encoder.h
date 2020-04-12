#ifndef _AAC_ENCODER_H
#define _AAC_ENCODER_H

#include <cstdint>
#include "av_encoder.h"

namespace ffmpeg {

class AACEncoder : public Encoder
{
public:
	virtual bool Init(AVConfig& audio_config);
	virtual void Destroy();

	uint32_t GetFrameSamples();

	AVPacketPtr Encode(const uint8_t *pcm, int samples);

private:
	SwrContext* swr_context_ = nullptr;
	AVFrame* pcm_frame_ = nullptr;
	uint32_t pts_ = 0;
};

}

#endif