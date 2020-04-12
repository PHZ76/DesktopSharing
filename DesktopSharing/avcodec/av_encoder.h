#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include <cstdint>
#include <memory>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#include <libavutil/fifo.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libavutil/opt.h>
}

namespace ffmpeg {

struct AVFrameDeleter {
	void operator()(AVFrame* ptr) const { av_frame_free(&ptr); }
};

struct AVPacketDeleter {
	void operator()(AVPacket* ptr) const { av_packet_free(&ptr); }
};

typedef std::unique_ptr<AVPacket, AVPacketDeleter> AVPacketPtr;
typedef std::unique_ptr<AVFrame, AVFrameDeleter> AVFramePtr;

struct VideoConfig
{
	uint32_t width = 1920;
	uint32_t height = 1080;
	uint32_t bitrate = 4000000;
	uint32_t framerate = 25;
	uint32_t gop = 25;
	AVPixelFormat format = AV_PIX_FMT_BGRA;
};

struct AudioConfig
{
	uint32_t channels = 2;
	uint32_t samplerate = 48000;
	uint32_t bitrate = 16000 * 4;
	AVSampleFormat format = AV_SAMPLE_FMT_S16;
};

struct AVConfig
{
	VideoConfig video;
	AudioConfig audio;
};

class Encoder
{
public:
	Encoder & operator=(const Encoder &) = delete;
	virtual ~Encoder() {};

	virtual bool Init(AVConfig& config) = 0;
	virtual void Destroy() = 0;

	AVCodecContext* GetAVCodecContext() const {
		return codec_contex_;
	}

protected:
	bool is_initialized_ = false;
	AVConfig av_config_;
	AVCodecContext *codec_contex_ = nullptr;
};

}

#endif