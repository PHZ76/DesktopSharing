#include "aac_encoder.h"
#include "net/log.h"

using namespace ffmpeg;

bool AACEncoder::Init(AVConfig& audio_config)
{
	if (is_initialized_) {
		Destroy();
	}

	av_config_ = audio_config;

	AVCodec *codec = nullptr;
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec) {
		LOG("AAC Encoder not found.\n");
		Destroy();
		return false;
	}

	codec_context_ = avcodec_alloc_context3(codec);
	if (!codec_context_) {
		LOG("avcodec_alloc_context3() failed.");
		Destroy();
		return false;
	}

	codec_context_->sample_rate = av_config_.audio.samplerate;
	codec_context_->sample_fmt = AV_SAMPLE_FMT_FLTP;
	codec_context_->channels = av_config_.audio.channels;
	codec_context_->channel_layout = av_get_default_channel_layout(av_config_.audio.channels);
	codec_context_->bit_rate = av_config_.audio.bitrate;
	//codec_context_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(codec_context_, codec, NULL) != 0) {
		LOG("avcodec_open2() failed.\n");
		Destroy();
		return false;
	}

	is_initialized_ = true;
	return true;
}

void AACEncoder::Destroy()
{
	if (is_initialized_) {
		if (swr_context_) {
			swr_free(&swr_context_);
			swr_context_ = nullptr;
		}

		if (pcm_frame_) {
			av_frame_free(&pcm_frame_);
			pcm_frame_ = nullptr;
		}

		if (codec_context_) {
			avcodec_close(codec_context_);
			avcodec_free_context(&codec_context_);
			codec_context_ = nullptr;
		}

		is_initialized_ = false;
	}
}

uint32_t AACEncoder::GetFrameSamples()
{
	if (is_initialized_) {
		return codec_context_->frame_size;
	}

	return 0;
}

AVPacketPtr AACEncoder::Encode(const uint8_t *pcm_s16, int samples)
{
	if (samples != codec_context_->frame_size) {
		return nullptr;
	}

	if (!swr_context_) {
		swr_context_ = swr_alloc_set_opts(swr_context_, codec_context_->channel_layout, codec_context_->sample_fmt,
			codec_context_->sample_rate, av_get_default_channel_layout(av_config_.audio.channels),
			av_config_.audio.format, av_config_.audio.samplerate, 0, 0);
		int ret = swr_init(swr_context_);
		if (ret != 0) {
			LOG("swr_init() failed.\n");
			return nullptr;
		}

		if (!pcm_frame_) {
			pcm_frame_ = av_frame_alloc();
			pcm_frame_->format = codec_context_->sample_fmt;
			pcm_frame_->channels = codec_context_->channels;
			pcm_frame_->channel_layout = codec_context_->channel_layout;
			pcm_frame_->nb_samples = codec_context_->frame_size;
			ret = av_frame_get_buffer(pcm_frame_, 0);
			if (ret < 0) {
				LOG("av_frame_get_buffer() failed.\n");
				return nullptr;
			}
		}
	}

	const uint8_t *data[AV_NUM_DATA_POINTERS] = { 0 };
	data[0] = (uint8_t *)pcm_s16;
	data[1] = NULL;
	int len = swr_convert(swr_context_, pcm_frame_->data, pcm_frame_->nb_samples,
							data, pcm_frame_->nb_samples);

	if (len < 0) {
		LOG("swr_convert() failed.\n");
		return nullptr;
	}

	pcm_frame_->pts = pts_++;

	int ret = avcodec_send_frame(codec_context_, pcm_frame_);
	if (ret != 0) {
		return nullptr;
	}

	AVPacketPtr av_packet_(av_packet_alloc());

	ret = avcodec_receive_packet(codec_context_, av_packet_.get());
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return nullptr;
	}
	else if (ret < 0) {
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return av_packet_;
}