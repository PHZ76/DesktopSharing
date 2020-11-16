#include "h264_encoder.h"
#include "av_common.h"

#define USE_LIBYUV 0
#if USE_LIBYUV
#include "libyuv.h"
#endif

using namespace ffmpeg;

bool H264Encoder::Init(AVConfig& video_config)
{
	if (is_initialized_) {
		Destroy();
	}

	av_config_ = video_config;

	//av_log_set_level(AV_LOG_DEBUG);

	AVCodec *codec = nullptr;
	//codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	codec = avcodec_find_encoder_by_name("libx264");
	if (!codec) {
		LOG("H.264 Encoder not found.\n");
		Destroy();
		return false;
	}

	codec_context_ = avcodec_alloc_context3(codec);
	if (!codec_context_) {
		LOG("avcodec_alloc_context3() failed.");
		Destroy();
		return false;
	}
	
	codec_context_->width = av_config_.video.width;
	codec_context_->height = av_config_.video.height;
	codec_context_->time_base = { 1,  (int)av_config_.video.framerate };
	codec_context_->framerate = { (int)av_config_.video.framerate, 1 };
	codec_context_->gop_size = av_config_.video.gop;
	codec_context_->max_b_frames = 0;
	codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;

	// rc control mode: abr
	codec_context_->bit_rate = av_config_.video.bitrate;
	
	// cbr mode config
	codec_context_->rc_min_rate = av_config_.video.bitrate;
	codec_context_->rc_max_rate = av_config_.video.bitrate;
	codec_context_->rc_buffer_size = (int)av_config_.video.bitrate;

	codec_context_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (codec->id == AV_CODEC_ID_H264) {
		av_opt_set(codec_context_->priv_data, "preset", "ultrafast", 0); //ultrafast 
	}

	av_opt_set(codec_context_->priv_data, "tune", "zerolatency", 0);
	av_opt_set_int(codec_context_->priv_data, "forced-idr", 1, 0);
	av_opt_set_int(codec_context_->priv_data, "avcintra-class", -1, 0);
	
	if (avcodec_open2(codec_context_, codec, NULL) != 0) {
		LOG("avcodec_open2() failed.\n");
		Destroy();
		return false;
	}

	in_width_ = av_config_.video.width;
	in_height_ = av_config_.video.height;
	is_initialized_ = true;
	return true;
}

void H264Encoder::Destroy()
{
	if (video_converter_) {
		video_converter_->Destroy();
		video_converter_.reset();
	}

	if (codec_context_) {
		avcodec_close(codec_context_);
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
	}
		
	in_width_ = 0;
	in_height_ = 0;
	pts_ = 0;
	is_initialized_ = false;
}

AVPacketPtr H264Encoder::Encode(const uint8_t *image, uint32_t width, uint32_t height, uint32_t image_size, uint64_t pts)
{
	if (!is_initialized_) {
		return nullptr;
	}

	if (width != in_width_ || height != av_config_.video.height || !video_converter_) {
		in_width_ = width;
		in_height_ = height;

		video_converter_.reset(new ffmpeg::VideoConverter());
		if (!video_converter_->Init(in_width_, in_height_, (AVPixelFormat)av_config_.video.format,
									codec_context_->width, codec_context_->height, codec_context_->pix_fmt)) {
			video_converter_.reset();
			return nullptr;
		}
	}

	ffmpeg::AVFramePtr in_frame(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });
	in_frame->width = in_width_;
	in_frame->height = in_height_;
	in_frame->format = av_config_.video.format;
	if (av_frame_get_buffer(in_frame.get(), 32) != 0) {
		return nullptr;
	}

	//int pic_size = av_image_get_buffer_size(av_config_.video.format, in_width_, in_height_, in_frame->linesize[0]);
	//if (pic_size <= 0) {
	//	return nullptr;
	//}

	memcpy(in_frame->data[0], image, image_size);

//#if USE_LIBYUV	
	//int result = libyuv::ARGBToI420(bgra_image, width * 4,
	//	yuv_frame->data[0], yuv_frame->linesize[0],
	//	yuv_frame->data[1], yuv_frame->linesize[1],
	//	yuv_frame->data[2], yuv_frame->linesize[2],
	//	width, height);

	//if (result != 0) {
	//	LOG("libyuv::ARGBToI420() failed.\n");
	//	return nullptr;
	//}
//#else
	AVFramePtr yuv_frame = nullptr;
	if (video_converter_->Convert(in_frame, yuv_frame) <= 0) {
		return nullptr;
	}
//#endif

	if (pts >= 0) {
		yuv_frame->pts = pts;
	}
	else {
		yuv_frame->pts = pts_++;
	}

	yuv_frame->pict_type = AV_PICTURE_TYPE_NONE;
	if (force_idr_) {
		yuv_frame->pict_type = AV_PICTURE_TYPE_I;
		force_idr_ = false;
	}

	if (avcodec_send_frame(codec_context_, yuv_frame.get()) < 0) {
		LOG("avcodec_send_frame() failed.\n");
		return nullptr;
	}

	AVPacketPtr av_packet(av_packet_alloc(), [](AVPacket* ptr) {
		av_packet_free(&ptr);
	});
	av_init_packet(av_packet.get());

	int ret = avcodec_receive_packet(codec_context_, av_packet.get());
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return nullptr;
	}
	else if (ret < 0) {
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return av_packet;
}

void H264Encoder::ForceIDR()
{
	if (codec_context_) {
		force_idr_ = true;		
	}
}

void H264Encoder::SetBitrate(uint32_t bitrate_kbps)
{
	if (codec_context_) {
		int64_t out_val = 0;
		av_opt_get_int(codec_context_->priv_data, "avcintra-class", 0, &out_val);
		if (out_val < 0) {
			codec_context_->bit_rate = bitrate_kbps * 1000;
			codec_context_->rc_min_rate = codec_context_->bit_rate;
			codec_context_->rc_max_rate = codec_context_->bit_rate;
			codec_context_->rc_buffer_size = (int)codec_context_->bit_rate;
		}	
	}
}