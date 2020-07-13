#include "GDIScreenCapture.h"
#include <Windows.h>

GDIScreenCapture::GDIScreenCapture()
{
	avdevice_register_all();	
}

GDIScreenCapture::~GDIScreenCapture()
{

}

bool GDIScreenCapture::Init()
{
	if (is_initialized_) {
		return true;
	}

	AVDictionary *options = nullptr;
	av_dict_set_int(&options, "framerate", framerate_, AV_DICT_MATCH_CASE);
	av_dict_set_int(&options, "draw_mouse", 1, AV_DICT_MATCH_CASE);

	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);
	char video_size[20] = { 0 };
	snprintf(video_size, sizeof(video_size), "%dx%d", width, height);
	av_dict_set(&options, "video_size", video_size, 1);

	input_format_ = av_find_input_format("gdigrab");
	if (!input_format_) {
		printf("[GDIScreenCapture] Gdigrab not found.\n");
		return false;
	}

	format_context_ = avformat_alloc_context();
	if (avformat_open_input(&format_context_, "desktop", input_format_, &options) != 0) {
		printf("[GDIScreenCapture] Open input failed.");
		return false;
	}

	if (avformat_find_stream_info(format_context_, nullptr) < 0) {
		printf("[GDIScreenCapture] Couldn't find stream info.\n");
		avformat_close_input(&format_context_);
		format_context_ = nullptr;
		return false;
	}

	int video_index = -1;

	for (unsigned int i = 0; i < format_context_->nb_streams; i++) {
		if (format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_index = i;
		}
	}

	if (video_index < 0) {
		printf("[GDIScreenCapture] Couldn't find video stream.\n");
		avformat_close_input(&format_context_);
		format_context_ = nullptr;
		return false;
	}

	AVCodec* codec = avcodec_find_decoder(format_context_->streams[video_index]->codecpar->codec_id);
	if (!codec) {
		avformat_close_input(&format_context_);
		format_context_ = nullptr;
		return false;
	}

	codec_context_ = avcodec_alloc_context3(codec);
	if (!codec_context_) {
		return false;
	}

	avcodec_parameters_to_context(codec_context_, format_context_->streams[video_index]->codecpar);
	if (avcodec_open2(codec_context_, codec, nullptr) != 0) {
		avcodec_close(codec_context_);
		codec_context_ = nullptr;
		avformat_close_input(&format_context_);
		format_context_ = nullptr;
		return false;
	}

	video_index_ = video_index;
	is_initialized_ = true;
	StartCapture();
	return true;
}

bool GDIScreenCapture::Destroy()
{
	if (is_initialized_) {
		StopCapture();		

		if (codec_context_) {
			avcodec_close(codec_context_);
			codec_context_ = nullptr;
		}

		if (format_context_) {
			avformat_close_input(&format_context_);
			format_context_ = nullptr;
		}
		
		input_format_ = nullptr;
		video_index_ = -1;
		is_initialized_ = false;
		return true;
	}
	
	return false;
}

bool GDIScreenCapture::StartCapture()
{
	if (is_initialized_ && !is_started_) {
		is_started_ = true;
		thread_ptr_.reset(new std::thread([this] {
			while (is_started_) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000 / framerate_));
				AquireFrame();
			}
		}));

		return true;
	}

	return false;
}

void GDIScreenCapture::StopCapture()
{
	if (is_started_) {
		is_started_ = false;
		thread_ptr_->join();
		thread_ptr_.reset();

		std::lock_guard<std::mutex> locker(mutex_);
		image_.reset();
		image_size_ = 0;
		width_ = 0;
		height_ = 0;
	}
}

bool GDIScreenCapture::AquireFrame()
{
	if (!is_started_) {
		return false;
	}

	std::shared_ptr<AVFrame> av_frame(av_frame_alloc(), [](AVFrame *ptr) { av_frame_free(&ptr); });
	std::shared_ptr<AVPacket> av_packet(av_packet_alloc(), [](AVPacket *ptr) { av_packet_free(&ptr); });

	int ret = av_read_frame(format_context_, av_packet.get());
	if (ret < 0) {
		return false;
	}

	if (av_packet->stream_index == video_index_) {
		Decode(av_frame.get(), av_packet.get());
	}

	av_packet_unref(av_packet.get());
	return true;
}

bool GDIScreenCapture::Decode(AVFrame* av_frame, AVPacket* av_packet)
{
	int ret = avcodec_send_packet(codec_context_, av_packet);
	if (ret < 0) {
		return false;
	}

	if (ret >= 0) {
		ret = avcodec_receive_frame(codec_context_, av_frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return true;
		}

		if (ret < 0) {
			return false;
		}

		std::lock_guard<std::mutex> locker(mutex_);
		image_size_ = av_frame->pkt_size;
		image_.reset(new uint8_t[image_size_]);
		memcpy(image_.get(), av_frame->data[0], image_size_);
		width_ = av_frame->width;
		height_ = av_frame->height;

		av_frame_unref(av_frame);
	}

	return true;
}

bool GDIScreenCapture::CaptureFrame(std::vector<uint8_t>& image, uint32_t& width, uint32_t& height)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!is_started_) {
		image.clear();
		return false;
	}

	if (image_ == nullptr || image_size_ == 0) {
		image.clear();
		return false;
	}

	if (image.capacity() < image_size_) {
		image.reserve(image_size_);
	}

	image.assign(image_.get(), image_.get() + image_size_);
	width = width_;
	height = height_;
	return true;
}

uint32_t GDIScreenCapture::GetWidth()  const
{
	return width_;
}

uint32_t GDIScreenCapture::GetHeight() const
{
	return height_;
}

bool GDIScreenCapture::CaptureStarted() const
{
	return is_started_;
}
