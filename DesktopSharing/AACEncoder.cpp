#include "AACEncoder.h"
#include "net/log.h"

#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avutil.lib")

AACEncoder::AACEncoder()
{

}

AACEncoder::~AACEncoder()
{

}

AACEncoder& AACEncoder::instance()
{
	static AACEncoder s_encoder;
	return s_encoder;
}

bool AACEncoder::init(struct AudioConfig ac)
{
	if (_isInitialized)
	{
		this->exit();
	}

	_audioConfig = ac;

	AVCodec *codec = nullptr;
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec)
	{
		LOG("AAC Encoder not found.\n");
		return false;
	}

	_aCodecCtx = avcodec_alloc_context3(codec);
	if (!_aCodecCtx)
	{
		LOG("avcodec_alloc_context3() failed.");
		return false;
	}

	_aCodecCtx->sample_rate = _audioConfig.samplerate;
	_aCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
	_aCodecCtx->channels = _audioConfig.channels;
	_aCodecCtx->channel_layout = av_get_default_channel_layout(_audioConfig.channels);
	_aCodecCtx->bit_rate = _audioConfig.bitrate;
	//_aCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(_aCodecCtx, codec, NULL) != 0)
	{
		LOG("avcodec_open2() failed.\n");
		return false;
	}

	_aPkt = av_packet_alloc();
	_isInitialized = true;
	return true;
}

void AACEncoder::exit()
{
	_isInitialized = false;
	_aPts = 0;

	if (_swrCtx)
	{
		swr_free(&_swrCtx);
		_swrCtx = nullptr;
	}

	if (_pcmFrame)
	{
		av_frame_free(&_pcmFrame);
		_pcmFrame = nullptr;
	}

	if (_aPkt)
	{
		av_packet_free(&_aPkt);
		_aPkt = nullptr;
	}

	if (_aCodecCtx)
	{
		avcodec_close(_aCodecCtx);
		avcodec_free_context(&_aCodecCtx);
		_aCodecCtx = nullptr;
	}
}

uint32_t AACEncoder::getFrameSamples()
{
	if (!_isInitialized)
	{
		return 0;
	}

	return _aCodecCtx->frame_size;
}

AVPacket* AACEncoder::encodeAudio(const uint8_t *pcm, int samples)
{
	if (samples != _aCodecCtx->frame_size)
	{
		return NULL;
	}

	if (!_swrCtx)
	{
		_swrCtx = swr_alloc_set_opts(_swrCtx, _aCodecCtx->channel_layout, _aCodecCtx->sample_fmt,
			_aCodecCtx->sample_rate, av_get_default_channel_layout(_audioConfig.channels),
			(AVSampleFormat)AV_SAMPLE_FMT_S16, _audioConfig.samplerate, 0, 0);
		int ret = swr_init(_swrCtx);
		if (ret != 0)
		{
			LOG("swr_init() failed.\n");
			return nullptr;
		}

		if (!_pcmFrame)
		{
			_pcmFrame = av_frame_alloc();
			_pcmFrame->format = _aCodecCtx->sample_fmt;
			_pcmFrame->channels = _aCodecCtx->channels;
			_pcmFrame->channel_layout = _aCodecCtx->channel_layout;
			_pcmFrame->nb_samples = _aCodecCtx->frame_size;
			ret = av_frame_get_buffer(_pcmFrame, 0);
			if (ret < 0)
			{
				LOG("av_frame_get_buffer() failed.\n");
				return nullptr;
			}
		}
	}

	const uint8_t *data[AV_NUM_DATA_POINTERS] = { 0 };
	data[0] = (uint8_t *)pcm;
	data[1] = NULL;
	int len = swr_convert(_swrCtx, _pcmFrame->data, _pcmFrame->nb_samples,
							data, _pcmFrame->nb_samples);

	if (len < 0)
	{
		LOG("swr_convert() failed.\n");
		return nullptr;
	}

	_pcmFrame->pts = _aPts++;

	int ret = avcodec_send_frame(_aCodecCtx, _pcmFrame);
	if (ret != 0)
	{
		return nullptr;
	}

	av_packet_unref(_aPkt);

	ret = avcodec_receive_packet(_aCodecCtx, _aPkt);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	{
		return nullptr;
	}
	else if (ret < 0)
	{
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return _aPkt;
}