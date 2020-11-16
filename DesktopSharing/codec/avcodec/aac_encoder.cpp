#include "aac_encoder.h"
#include "av_common.h"

using namespace ffmpeg;

AACEncoder::AACEncoder()
{

}

AACEncoder::~AACEncoder()
{
	Destroy();
}

bool AACEncoder::Init(AVConfig& audio_config)
{
	if (is_initialized_) {
		return false;
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

	audio_resampler_.reset(new Resampler());
	if (!audio_resampler_->Init(av_config_.audio.samplerate, av_config_.audio.channels, 
								av_config_.audio.format, av_config_.audio.samplerate, 
								av_config_.audio.channels, AV_SAMPLE_FMT_FLTP)) {		
		LOG("Audio resampler init failed.\n");
		Destroy();
		return false;
	}

	is_initialized_ = true;
	return true;
}

void AACEncoder::Destroy()
{
	if (audio_resampler_) {
		audio_resampler_->Destroy();
		audio_resampler_.reset();
	}

	if (codec_context_) {
		avcodec_close(codec_context_);
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
	}

	pts_ = 0;
	is_initialized_ = false;
}

uint32_t AACEncoder::GetFrameSamples()
{
	if (is_initialized_) {
		return codec_context_->frame_size;
	}

	return 0;
}

AVPacketPtr AACEncoder::Encode(const uint8_t* pcm, int samples)
{
	AVFramePtr in_frame(av_frame_alloc(), [](AVFrame* ptr) {
		av_frame_free(&ptr);
	});

	in_frame->sample_rate = codec_context_->sample_rate;
	in_frame->format = AV_SAMPLE_FMT_FLT;
	in_frame->channels = codec_context_->channels;
	in_frame->channel_layout = codec_context_->channel_layout;
	in_frame->nb_samples = samples;
	in_frame->pts = pts_;
	in_frame->pts = av_rescale_q(pts_, { 1, codec_context_->sample_rate }, codec_context_->time_base);
	pts_ += in_frame->nb_samples;

	if (av_frame_get_buffer(in_frame.get(), 0) < 0) {
		LOG("av_frame_get_buffer() failed.\n");
		return nullptr;
	}

	int bytes_per_sample = av_get_bytes_per_sample(av_config_.audio.format);
	if (bytes_per_sample == 0) {
		return nullptr;
	}

	memcpy(in_frame->data[0], pcm, bytes_per_sample * in_frame->channels * samples);

	AVFramePtr fltp_frame = nullptr;
	if (audio_resampler_->Convert(in_frame, fltp_frame) <= 0) {
		return nullptr;
	}

	int ret = avcodec_send_frame(codec_context_, fltp_frame.get());
	if (ret != 0) {
		return nullptr;
	}
	
	AVPacketPtr av_packet(av_packet_alloc(), [](AVPacket* ptr) {av_packet_free(&ptr);});
	av_init_packet(av_packet.get());

	ret = avcodec_receive_packet(codec_context_, av_packet.get());
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return nullptr;
	}
	else if (ret < 0) {
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return av_packet;
}
