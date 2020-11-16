#include "video_converter.h"

using namespace ffmpeg;

VideoConverter::VideoConverter()
{

}

VideoConverter::~VideoConverter()
{
	Destroy();
}

bool VideoConverter::Init(int in_width, int in_height, AVPixelFormat in_format,
	int out_width, int out_height, AVPixelFormat out_format)
{
	if (sws_context_) {
		return false;
	}

	if (sws_context_ == nullptr) {
		sws_context_ = sws_getContext(
			in_width, in_height, in_format,out_width, 
			out_height, out_format, 
			SWS_BICUBIC, NULL, NULL, NULL);

		out_width_ = out_width;
		out_height_ = out_height;
		out_format_ = out_format;
		return sws_context_ != nullptr;
	}
	return false;
}

void VideoConverter::Destroy()
{
	if (sws_context_) {
		sws_freeContext(sws_context_);
		sws_context_ = nullptr;
	}
}

int VideoConverter::Convert(AVFramePtr in_frame, AVFramePtr& out_frame)
{
	if (!sws_context_) {
		return -1;
	}

	out_frame.reset(av_frame_alloc(), [](AVFrame* ptr) {
		av_frame_free(&ptr);
	});

	out_frame->width = out_width_;
	out_frame->height = out_height_;
	out_frame->format = out_format_;
	out_frame->pts = in_frame->pts;
	out_frame->pkt_dts = in_frame->pkt_dts;

	if (av_frame_get_buffer(out_frame.get(), 32) != 0) {
		return -1;
	}

	int out_height = sws_scale(sws_context_, in_frame->data, in_frame->linesize, 0, in_frame->height,
		out_frame->data, out_frame->linesize);
	if (out_height < 0) {
		return -1;
	}

	return out_height;
}