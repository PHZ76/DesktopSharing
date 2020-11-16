#ifndef FFMPEG_H264_ENCODER_H
#define FFMPEG_H264_ENCODER_H

#include <cstdint>
#include "av_encoder.h"
#include "video_converter.h"

namespace ffmpeg {

class H264Encoder : public Encoder
{
public:
	virtual bool Init(AVConfig& video_config);
	virtual void Destroy();

	virtual AVPacketPtr Encode(const uint8_t *image, uint32_t width, uint32_t height, uint32_t image_size, uint64_t pts = 0);

	virtual void ForceIDR();
	virtual void SetBitrate(uint32_t bitrate_kbps);

private:
	int64_t pts_ = 0;
	std::unique_ptr<VideoConverter> video_converter_;
	uint32_t in_width_  = 0;
	uint32_t in_height_ = 0;
	bool force_idr_ = false;
};

}

#endif
