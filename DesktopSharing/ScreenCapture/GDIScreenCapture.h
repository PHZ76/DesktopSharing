// PHZ
// 2020-7-8

#ifndef GDI_SCREEN_CAPTURE_H
#define GDI_SCREEN_CAPTURE_H

#include "ScreenCapture.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}
#include <memory>
#include <thread>
#include <mutex>

class GDIScreenCapture : public ScreenCapture
{
public:
	GDIScreenCapture();
	virtual ~GDIScreenCapture();

	virtual bool Init();
	virtual bool Destroy();

	virtual bool CaptureFrame(std::vector<uint8_t>& image, uint32_t& width, uint32_t& height);

	virtual uint32_t GetWidth()  const;
	virtual uint32_t GetHeight() const;
	virtual bool CaptureStarted() const;

private:
	bool StartCapture();
	void StopCapture();
	bool AquireFrame();
	bool Decode(AVFrame* av_frame, AVPacket* av_packet);

	bool is_initialized_ = false;
	bool is_started_ = false;
	std::unique_ptr<std::thread> thread_ptr_;

	AVFormatContext* format_context_ = nullptr;
	AVInputFormat* input_format_ = nullptr;
	AVCodecContext* codec_context_ = nullptr;
	int video_index_ = -1;
	int framerate_ = 25;
	
	std::mutex mutex_;
	std::shared_ptr<uint8_t> image_;
	uint32_t image_size_ = 0;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
};

#endif