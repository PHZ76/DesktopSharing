#include "h264_encoder.h"
#include "net/log.h"

using namespace ffmpeg;

bool H264Encoder::Init(AVConfig& video_config)
{
	if (is_initialized_) {
		Destroy();
	}

	av_config_ = video_config;

	av_log_set_level(1);

	AVCodec *codec = nullptr;
	codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	//codec = avcodec_find_encoder_by_name("h264_nvenc");
	if (!codec) {
		LOG("H.264 Encoder not found.\n");
		Destroy();
		return false;
	}

	codec_contex_ = avcodec_alloc_context3(codec);
	if (!codec_contex_) {
		LOG("avcodec_alloc_context3() failed.");
		Destroy();
		return false;
	}
	
	codec_contex_->width = av_config_.video.width;
	codec_contex_->height = av_config_.video.height;
	codec_contex_->time_base = { 1,  (int)av_config_.video.framerate };
	codec_contex_->framerate = { (int)av_config_.video.framerate, 1 };
	codec_contex_->gop_size = av_config_.video.gop;
	codec_contex_->max_b_frames = 0;
	codec_contex_->pix_fmt = AV_PIX_FMT_YUV420P;

	codec_contex_->bit_rate = av_config_.video.bitrate;
	codec_contex_->rc_min_rate = av_config_.video.bitrate;
	codec_contex_->rc_max_rate = av_config_.video.bitrate;
	codec_contex_->rc_buffer_size = (int)codec_contex_->bit_rate;

	codec_contex_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	//codec_contex_->flags |= AV_CODEC_FLAG2_LOCAL_HEADER;

	if (codec->id == AV_CODEC_ID_H264) {
		av_opt_set(codec_contex_->priv_data, "preset", "ultrafast", 0); //ultrafast 
	}

	av_opt_set(codec_contex_->priv_data, "tune", "zerolatency", 0);

	if (avcodec_open2(codec_contex_, codec, NULL) != 0) {
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
		if (sws_contex_) {
			sws_freeContext(sws_contex_);
			sws_contex_ = nullptr;
		}

		if (yuv_frame_) {
			av_frame_free(&yuv_frame_);
			yuv_frame_ = nullptr;
		}

		if (codec_contex_) {
			avcodec_close(codec_contex_);
			avcodec_free_context(&codec_contex_);
			codec_contex_ = nullptr;
		}
		
		pts_ = 0;
		is_initialized_ = false;
	}
}

AVPacketPtr H264Encoder::Encode(const uint8_t *image, uint32_t width, uint32_t height, uint64_t pts)
{
	VideoConfig& h264_config = av_config_.video;

	if (sws_contex_ == nullptr) {
		sws_contex_ = sws_getContext(width, height, h264_config.format, h264_config.width, h264_config.height,
			AV_PIX_FMT_YUV420P, SWS_BICUBIC,NULL, NULL, NULL);
		if (sws_contex_ == nullptr) {
			LOG("sws_getContext() failed, fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
				av_get_pix_fmt_name(AV_PIX_FMT_RGBA), width, width,
				av_get_pix_fmt_name(AV_PIX_FMT_YUV420P), h264_config.width, h264_config.height);
			return nullptr;
		}
	}

	if (yuv_frame_ == nullptr) {
		yuv_frame_ = av_frame_alloc();
		yuv_frame_->format = AV_PIX_FMT_YUV420P;
		yuv_frame_->width = h264_config.width;
		yuv_frame_->height = h264_config.height;
		yuv_frame_->pts = 0;
		if (av_frame_get_buffer(yuv_frame_, 32) != 0) {
			LOG("av_frame_get_buffer() failed.\n");
			return nullptr;
		}
	}

	uint8_t* data[AV_NUM_DATA_POINTERS] = { 0 };
	data[0] = (uint8_t*)image;
	int insize[AV_NUM_DATA_POINTERS] = { 0 };
	insize[0] = width * 4;
	int outHeight = sws_scale(sws_contex_, data, insize, 0, height,yuv_frame_->data, yuv_frame_->linesize);
	if (outHeight < 0) {
		LOG("sws_scale() failed.\n");
		return nullptr;
	}

	if (pts != 0) {
		yuv_frame_->pts = pts;	
	}
	else {
		yuv_frame_->pts = pts_++;
	}

	if (avcodec_send_frame(codec_contex_, yuv_frame_) < 0) {
		LOG("avcodec_send_frame() failed.\n");
		return nullptr;
	}

	AVPacketPtr av_packet_(av_packet_alloc());

	int ret = avcodec_receive_packet(codec_contex_, av_packet_.get());
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return nullptr;
	}
	else if (ret < 0) {
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return av_packet_;
}


