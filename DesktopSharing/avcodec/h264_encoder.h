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

	virtual AVPacketPtr Encode(const uint8_t *bgra_image, uint32_t width, uint32_t height, uint64_t pts = 0);

	virtual void ForceIDR();
	virtual void SetBitrate(uint32_t bitrate_kbps);

private:
	SwsContext* sws_context_ = nullptr;
	//AVFrame* yuv_frame_ = nullptr;
	uint32_t  pts_ = 0;
	bool force_idr_ = false;
};

}

#endif
