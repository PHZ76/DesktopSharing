#ifndef FFMPEG_VIDEO_CONVERTER_H
#define FFMPEG_VIDEO_CONVERTER_H

#include <cstdint>
#include <memory>
#include "av_common.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace ffmpeg {

class VideoConverter
{
public:
	VideoConverter& operator=(const VideoConverter&) = delete;
	VideoConverter(const VideoConverter&) = delete;
	VideoConverter();
	virtual ~VideoConverter();

	bool Init(int in_width, int in_height, AVPixelFormat in_format,
		int out_width, int out_height, AVPixelFormat out_format);

	void Destroy();

	int  Convert(AVFramePtr in_frame, AVFramePtr& out_frame);

private:
	SwsContext* sws_context_ = nullptr;
	int out_width_ = 0;
	int out_height_ = 0;
	AVPixelFormat out_format_ = AV_PIX_FMT_NONE;
};

}

#endif
