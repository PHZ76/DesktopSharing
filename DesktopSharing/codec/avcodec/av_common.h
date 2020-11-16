#ifndef FFMPEG_COMMON_H
#define FFMPEG_COMMON_H

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/error.h"
}
#include <cstdio>
#include <memory>

namespace ffmpeg {

using AVPacketPtr = std::shared_ptr<AVPacket>;
using AVFramePtr  = std::shared_ptr<AVFrame>;

}

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%d] " format " \n", \
   __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}

#define AV_LOG(code, format, ...)  	\
{								\
	char buf[1024] = { 0 };		\
	av_strerror(code, buf, 1023); \
    fprintf(stderr, "[%s:%d] " format " - %s. \n", \
   __FUNCTION__ , __LINE__, ##__VA_ARGS__, buf);     \
}

#endif
