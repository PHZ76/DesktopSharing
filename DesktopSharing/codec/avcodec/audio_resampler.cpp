#include "audio_resampler.h"
#include "av_common.h"

using namespace ffmpeg;

Resampler::Resampler()
{

}

Resampler::~Resampler()
{
	Destroy();
}

bool Resampler::Init(int in_samplerate, int in_channels, AVSampleFormat in_format,
	int out_samplerate, int out_channels, AVSampleFormat out_format)
{
	if (swr_context_ == nullptr) {
		int64_t in_channels_layout = av_get_default_channel_layout(in_channels);
		int64_t out_channels_layout = av_get_default_channel_layout(out_channels);

		swr_context_ = swr_alloc();

		av_opt_set_int(swr_context_, "in_channel_layout", in_channels_layout, 0);
		av_opt_set_int(swr_context_, "in_sample_rate", in_samplerate, 0);
		av_opt_set_sample_fmt(swr_context_, "in_sample_fmt", in_format, 0);

		av_opt_set_int(swr_context_, "out_channel_layout", out_channels_layout, 0);
		av_opt_set_int(swr_context_, "out_sample_rate", out_samplerate, 0);
		av_opt_set_sample_fmt(swr_context_, "out_sample_fmt", out_format, 0);


		int ret = swr_init(swr_context_);
		if (ret < 0) {
			AV_LOG(ret, "swr_init() failed.");
			return false;
		}

		in_samplerate_ = in_samplerate;
		in_channels_ = in_channels;
		in_format_ = in_format;
		in_bits_per_sample_ = av_get_bytes_per_sample(in_format_);
		out_samplerate_ = out_samplerate;
		out_channels_ = out_channels;
		out_format_ = out_format;
		out_bits_per_sample_ = av_get_bytes_per_sample(out_format_);

		return true;
	}

	return false;
}

void Resampler::Destroy()
{
	if (swr_context_ != nullptr) {
		if (swr_is_initialized(swr_context_)) {
			swr_close(swr_context_);
		}

		swr_free(&swr_context_);
		swr_context_ = nullptr;		
	}

	if (convert_buffer_) {
		av_free(convert_buffer_);
		convert_buffer_ = nullptr;
	}
}

int Resampler::Convert(AVFramePtr in_frame, AVFramePtr& out_frame)
{
	if (swr_context_ == nullptr) {
		return -1;
	}

	out_frame.reset(av_frame_alloc(), [](AVFrame* ptr) {
		av_frame_free(&ptr); 
	});

	out_frame->sample_rate = out_samplerate_;
	out_frame->format = out_format_;
	out_frame->channels = out_channels_;
	out_frame->nb_samples = (int)av_rescale_rnd(in_frame->nb_samples, out_frame->sample_rate, in_frame->sample_rate, AV_ROUND_UP);
	out_frame->pts = out_frame->pkt_dts = in_frame->pts;

	if (av_frame_get_buffer(out_frame.get(), 0) != 0) {
		return -1;
	}

	int len = swr_convert(swr_context_, (uint8_t**)&out_frame->data, out_frame->nb_samples, (const uint8_t**)in_frame->data, in_frame->nb_samples);
	if (len < 0) {
		out_frame = nullptr;
		AV_LOG(len, "swr_convert() failed.");
		return - 1;
	}

	return len;
}


