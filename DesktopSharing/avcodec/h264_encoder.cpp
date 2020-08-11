#include "h264_encoder.h"
#include "net/log.h"

#define USE_LIBYUV 1
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
	//codec_context_->flags |= AV_CODEC_FLAG2_LOCAL_HEADER;

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

	is_initialized_ = true;
	return true;
}

void H264Encoder::Destroy()
{
	if (is_initialized_) {
		if (sws_context_) {
			sws_freeContext(sws_context_);
			sws_context_ = nullptr;
		}

		//if (yuv_frame_) {
		//	av_frame_free(&yuv_frame_);
		//	yuv_frame_ = nullptr;
		//}

		if (codec_context_) {
			avcodec_close(codec_context_);
			avcodec_free_context(&codec_context_);
			codec_context_ = nullptr;
		}
		
		pts_ = 0;
		is_initialized_ = false;
	}
}

AVPacketPtr H264Encoder::Encode(const uint8_t *bgra_image, uint32_t width, uint32_t height, uint64_t pts)
{
	VideoConfig& h264_config = av_config_.video;

	AVFramePtr yuv_frame(av_frame_alloc());
	yuv_frame->format = AV_PIX_FMT_YUV420P;
	yuv_frame->width = h264_config.width;
	yuv_frame->height = h264_config.height;
	if (av_frame_get_buffer(yuv_frame.get(), 32) != 0) {
		LOG("av_frame_get_buffer() failed.\n");
		return nullptr;
	}

#if USE_LIBYUV	
	int result = libyuv::ARGBToI420(bgra_image, width * 4,
		yuv_frame->data[0], yuv_frame->linesize[0],
		yuv_frame->data[1], yuv_frame->linesize[1],
		yuv_frame->data[2], yuv_frame->linesize[2],
		width, height);

	if (result != 0) {
		LOG("libyuv::ARGBToI420() failed.\n");
		return nullptr;
	}
#else
	if (sws_context_ == nullptr) {
		sws_context_ = sws_getContext(width, height, h264_config.format, h264_config.width, h264_config.height,
			AV_PIX_FMT_YUV420P, SWS_BICUBIC,NULL, NULL, NULL);
		if (sws_context_ == nullptr) {
			LOG("sws_getContext() failed, fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
				av_get_pix_fmt_name(AV_PIX_FMT_RGBA), width, width,
				av_get_pix_fmt_name(AV_PIX_FMT_YUV420P), h264_config.width, h264_config.height);
			return nullptr;
		}
	}

	uint8_t* data[AV_NUM_DATA_POINTERS] = { 0 };
	data[0] = (uint8_t*)bgra_image;
	int insize[AV_NUM_DATA_POINTERS] = { 0 };
	insize[0] = width * 4;
	int outHeight = sws_scale(sws_context_, data, insize, 0, height, yuv_frame->data, yuv_frame->linesize);
	if (outHeight < 0) {
		LOG("sws_scale() failed.\n");
		return nullptr;
	}
#endif

	if (pts != 0) {
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

	AVPacketPtr av_packet_(av_packet_alloc());

	int ret = avcodec_receive_packet(codec_context_, av_packet_.get());
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return nullptr;
	}
	else if (ret < 0) {
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return av_packet_;
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