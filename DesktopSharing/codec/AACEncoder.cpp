#include "AACEncoder.h"

AACEncoder::AACEncoder()
{

}

AACEncoder::~AACEncoder()
{

}

bool AACEncoder::Init(int samplerate, int channel, int format, int bitrate_kbps)
{
	if (aac_encoder_.GetAVCodecContext()) {
		return false;
	}

	ffmpeg::AVConfig encoder_config;
	encoder_config.audio.samplerate = samplerate_ = samplerate;
	encoder_config.audio.bitrate = bitrate_ = bitrate_kbps * 1000;
	encoder_config.audio.channels = channel_ = channel;
	encoder_config.audio.format = format_ = (AVSampleFormat)format;

	if (!aac_encoder_.Init(encoder_config)) {
		return false;
	}

	return true;
}

void AACEncoder::Destroy()
{
	samplerate_ = 0;
	channel_ = 0;
	bitrate_ = 0;
	format_ = AV_SAMPLE_FMT_NONE;
	aac_encoder_.Destroy();
}

int AACEncoder::GetFrames()
{
	if (!aac_encoder_.GetAVCodecContext()) {
		return -1;
	}

	return aac_encoder_.GetFrameSamples();
}

int AACEncoder::GetSamplerate()
{
	return samplerate_;
}

int AACEncoder::GetChannel()
{
	return channel_;
}

int AACEncoder::GetSpecificConfig(uint8_t* buf, int max_buf_size)
{
	AVCodecContext* condec_context = aac_encoder_.GetAVCodecContext();
	if (!condec_context) {
		return -1;
	}

	if (max_buf_size < condec_context->extradata_size) {
		return -1;
	}

	memcpy(buf, condec_context->extradata, condec_context->extradata_size);
	return condec_context->extradata_size;
}

ffmpeg::AVPacketPtr AACEncoder::Encode(const uint8_t* pcm, int samples)
{
	if (!aac_encoder_.GetAVCodecContext()) {
		return nullptr;
	}

	return aac_encoder_.Encode(pcm, samples);
}
