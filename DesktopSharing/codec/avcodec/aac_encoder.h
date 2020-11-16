#ifndef _AAC_ENCODER_H
#define _AAC_ENCODER_H

#include <cstdint>
#include <memory>
#include "av_encoder.h"
#include "audio_resampler.h"

namespace ffmpeg {

class AACEncoder : public Encoder
{
public:
	AACEncoder();
	virtual ~AACEncoder();

	virtual bool Init(AVConfig& audio_config);
	virtual void Destroy();

	uint32_t GetFrameSamples();

	AVPacketPtr Encode(const uint8_t *pcm, int samples);

private:
	std::unique_ptr<Resampler> audio_resampler_;
	int64_t pts_ = 0;
};

}

#endif
