#ifndef FFMPEG_H264_ENCODER_H
#define FFMPEG_H264_ENCODER_H

#include <cstdint>
#include "av_encoder.h"

namespace ffmpeg {

class H264Encoder : public Encoder
{
public:
	virtual bool Init(AVConfig& video_config);
	virtual void Destroy();

	AVPacketPtr Encode(const uint8_t *image, uint32_t width, uint32_t height, uint64_t pts = 0);

private:
	SwsContext* sws_contex_ = nullptr;
	AVFrame* yuv_frame_ = nullptr;
	uint32_t  pts_ = 0;
};

}

#endif
