#pragma once

#include "avcodec/h264_encoder.h"
#include "NvCodec/nvenc.h"
#include "QsvCodec/QsvEncoder.h"
#include <string>

class H264Encoder
{
public:
	H264Encoder& operator=(const H264Encoder&) = delete;
	H264Encoder(const H264Encoder&) = delete;
	H264Encoder();
	virtual ~H264Encoder();

	void SetCodec(std::string codec);

	bool Init(int framerate, int bitrate_kbps, int format, int width, int height);
	void Destroy();

	int Encode(uint8_t* in_buffer, uint32_t in_width, uint32_t in_height,
			   uint32_t image_size, std::vector<uint8_t>& out_frame);

	int GetSequenceParams(uint8_t* out_buffer, int out_buffer_size);

private:
	bool IsKeyFrame(const uint8_t* data, uint32_t size);

	std::string codec_;
	ffmpeg::AVConfig encoder_config_;
	void* nvenc_data_ = nullptr;
	QsvEncoder qsv_encoder_;
	ffmpeg::H264Encoder h264_encoder_;
};
