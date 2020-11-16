#pragma once

#include "avcodec/aac_encoder.h"
#include "avcodec/av_common.h"

class AACEncoder
{
public:
	AACEncoder& operator=(const AACEncoder&) = delete;
	AACEncoder(const AACEncoder&) = delete;
	AACEncoder();
	virtual ~AACEncoder();

	bool Init(int samplerate, int channel, int format, int bitrate_kbps);
	void Destroy();

	int GetFrames();
	int GetSamplerate();
	int GetChannel();

	int GetSpecificConfig(uint8_t* buf, int max_buf_size);

	ffmpeg::AVPacketPtr Encode(const uint8_t* pcm, int samples);

private:
	ffmpeg::AACEncoder  aac_encoder_;
	int samplerate_ = 0;
	int channel_ = 0;
	int bitrate_ = 0;
	AVSampleFormat format_ = AV_SAMPLE_FMT_NONE;
};
