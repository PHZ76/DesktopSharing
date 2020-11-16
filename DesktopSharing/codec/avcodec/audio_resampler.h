#ifndef AUDIO_RESAMPLE_H
#define AUDIO_RESAMPLE_H

extern "C" {
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
}

#include <cstdint>
#include <memory>

namespace ffmpeg {

class Resampler
{
public:
	using AVFramePtr = std::shared_ptr<AVFrame>;

	Resampler& operator=(const Resampler&) = delete;
	Resampler(const Resampler&) = delete;
	Resampler();
	virtual ~Resampler();

	bool Init(int in_samplerate, int in_channels, AVSampleFormat in_format, 
		int out_samplerate, int out_channels, AVSampleFormat out_format);

	void Destroy();

	int  Convert(AVFramePtr in_frame, AVFramePtr& out_frame);

private:
	SwrContext* swr_context_ = nullptr;
	uint8_t** dst_buf_ = nullptr;

	int in_samplerate_ = 0;
	int in_channels_ = 0;
	int in_bits_per_sample_ = 0;
	AVSampleFormat in_format_ = AV_SAMPLE_FMT_NONE;

	int out_samplerate_ = 0;
	int out_channels_ = 0;
	int out_bits_per_sample_ = 0;
	AVSampleFormat out_format_ = AV_SAMPLE_FMT_NONE;

	int convert_buffer_size_ = 0;
	uint8_t* convert_buffer_ = nullptr;
};

}

#endif
