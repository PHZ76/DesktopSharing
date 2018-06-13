#include "H264Encoder.h"
#include "xop/log.h"

#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"avutil.lib")

using namespace std;

H264Encoder::H264Encoder()
{

}

H264Encoder::~H264Encoder()
{

}

H264Encoder& H264Encoder::Instance()
{
	static H264Encoder s_encoder;
	return s_encoder;
}


bool H264Encoder::Init(struct VideoConfig vc)
{
	if (_isInitialized)
	{
		Exit();
	}

	_videoConfig = vc;

	AVCodec *codec = nullptr;
	codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		LOG("H.264 Encoder not found.\n");
		return false;
	}

	_vCodecCtx = avcodec_alloc_context3(codec);
	if (!_vCodecCtx)
	{
		LOG("avcodec_alloc_context3() failed.");
		return false;
	}

	_vCodecCtx->bit_rate = _videoConfig.bitrate;
	_vCodecCtx->width = _videoConfig.width;
	_vCodecCtx->height = _videoConfig.height;
	_vCodecCtx->time_base = { 1,  (int)_videoConfig.framerate };
	_vCodecCtx->framerate = { (int)_videoConfig.framerate, 1 };
	_vCodecCtx->gop_size = _videoConfig.gop;
	_vCodecCtx->max_b_frames = 0;
	_vCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

	_vCodecCtx->rc_min_rate = 0;
	_vCodecCtx->rc_max_rate = _vCodecCtx->bit_rate; // vbv
	_vCodecCtx->rc_buffer_size = _vCodecCtx->bit_rate;

	_vCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	//_vCodecCtx->flags |= AV_CODEC_FLAG2_LOCAL_HEADER;

	if (codec->id == AV_CODEC_ID_H264)
	{
		av_opt_set(_vCodecCtx->priv_data, "preset", "ultrafast", 0); //ultrafast 
	}

	av_opt_set(_vCodecCtx->priv_data, "tune", "zerolatency", 0);

	if (avcodec_open2(_vCodecCtx, codec, NULL) != 0)
	{
		LOG("avcodec_open2() failed.\n");
		return false;
	}

	_vPkt = av_packet_alloc();
	_isInitialized = true;

	return true;
}

void H264Encoder::Exit()
{
	_isInitialized = false;
	_pts = 0;

	if (_swsCtx)
	{
		sws_freeContext(_swsCtx);
		_swsCtx = nullptr;
	}

	if (_yuvFrame)
	{
		av_frame_free(&_yuvFrame);
		_yuvFrame = nullptr;
	}

	if (_vPkt)
	{
		av_packet_free(&_vPkt);
		_vPkt = nullptr;
	}

	if (_vCodecCtx)
	{
		avcodec_close(_vCodecCtx);
		avcodec_free_context(&_vCodecCtx);
		_vCodecCtx = nullptr;
	}
}

AVPacket* H264Encoder::EncodeVideo(const uint8_t *rgba, uint32_t width, uint32_t height, uint64_t pts)
{
	if (_swsCtx == nullptr)
	{
		_swsCtx = sws_getContext(width, height, AV_PIX_FMT_BGRA,_videoConfig.width, _videoConfig.height,
								AV_PIX_FMT_YUV420P, SWS_BICUBIC,NULL, NULL, NULL);
		if (_swsCtx == nullptr)
		{
			LOG("sws_getContext() failed, fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
				av_get_pix_fmt_name(AV_PIX_FMT_RGBA), width, width,
				av_get_pix_fmt_name(AV_PIX_FMT_YUV420P), _videoConfig.width, _videoConfig.height);
			return nullptr;
		}
	}

	if (_yuvFrame == nullptr)
	{
		_yuvFrame = av_frame_alloc();
		_yuvFrame->format = AV_PIX_FMT_YUV420P;
		_yuvFrame->width = _videoConfig.width;
		_yuvFrame->height = _videoConfig.height;
		_yuvFrame->pts = 0;
		if (av_frame_get_buffer(_yuvFrame, 32) != 0)
		{
			LOG("av_frame_get_buffer() failed.\n");
			return nullptr;
		}
	}

	uint8_t* data[AV_NUM_DATA_POINTERS] = { 0 };
	data[0] = (uint8_t*)rgba;
	int insize[AV_NUM_DATA_POINTERS] = { 0 };
	insize[0] = width * 4;
	int outHeight = sws_scale(_swsCtx, data, insize, 0, height,_yuvFrame->data, _yuvFrame->linesize);
	if (outHeight < 0)
	{
		LOG("sws_scale() failed.\n");
		return nullptr;
	}

	if (pts != 0)
	{
		_yuvFrame->pts = pts;	
	}
	else
	{
		_yuvFrame->pts = _pts++;
	}

	if (avcodec_send_frame(_vCodecCtx, _yuvFrame) < 0)
	{
		LOG("avcodec_send_frame() failed.\n");
		return nullptr;
	}

	av_packet_unref(_vPkt);

	int ret = avcodec_receive_packet(_vCodecCtx, _vPkt);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	{
		return nullptr;
	}
	else if (ret < 0)
	{
		LOG("avcodec_receive_packet() failed.");
		return nullptr;
	}

	return _vPkt;
}


