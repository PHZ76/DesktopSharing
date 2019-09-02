#ifndef XOP_RTMP_H
#define XOP_RTMP_H

#include <cstdint>
#include <memory>

#define RTMP_VERSION           0x3

#define RTMP_SET_CHUNK_SIZE    0x1 //设置块大小
#define RTMP_AOBRT_MESSAGE     0X2 //终止消息
#define RTMP_ACK               0x3 //确认
#define RTMP_USER_EVENT        0x4 //用户控制消息
#define RTMP_ACK_SIZE          0x5 //窗口大小确认
#define RTMP_BANDWIDTH_SIZE    0x6 //设置对端带宽
#define RTMP_AUDIO             0x08
#define RTMP_VIDEO             0x09
#define RTMP_FLEX_MESSAGE      0x11 //amf3
#define RTMP_NOTIFY            0x12
#define RTMP_INVOKE            0x14 //amf0
#define RTMP_FLASH_VIDEO       0x16

#define RTMP_CHUNK_TYPE_0      0 // 11
#define RTMP_CHUNK_TYPE_1      1 // 7
#define RTMP_CHUNK_TYPE_2      2 // 3
#define RTMP_CHUNK_TYPE_3      3 // 0

#define RTMP_CHUNK_CONTROL_ID  2 
#define RTMP_CHUNK_INVOKE_ID   3
#define RTMP_CHUNK_AUDIO_ID    4
#define RTMP_CHUNK_VIDEO_ID    5
#define RTMP_CHUNK_DATA_ID     6

#define RTMP_CODEC_ID_H264     7
#define RTMP_CODEC_ID_AAC      10
#define RTMP_CODEC_ID_G711A    7
#define RTMP_CODEC_ID_G711U    8

#define RTMP_AVC_SEQUENCE_HEADER  0x18
#define RTMP_AAC_SEQUENCE_HEADER  0x19

namespace xop
{

struct MediaInfo
{
	uint8_t  videoCodecId = RTMP_CODEC_ID_H264;
	uint8_t  videoFramerate = 0;
	uint32_t videoWidth = 0;
	uint32_t videoHeight = 0;
	std::shared_ptr<uint8_t> sps;
	std::shared_ptr<uint8_t> pps;
	std::shared_ptr<uint8_t> sei;
	uint32_t spsSize = 0;
	uint32_t ppsSize = 0;
	uint32_t seiSize = 0;

	uint8_t  audioCodecId = RTMP_CODEC_ID_AAC;
	uint32_t audioChannel = 0;
	uint32_t audioSamplerate = 0;
	uint32_t audioFrameLen = 0;
	std::shared_ptr<uint8_t>  audioSpecificConfig;
	uint32_t audioSpecificConfigSize = 0;
};

class Rtmp
{
public:
	virtual ~Rtmp() {};

	void setChunkSize(uint32_t size)
	{
		if (size > 0 && size <= 60000)
		{
			m_maxChunkSize = size;
		}
	}

	void setGopCache(uint32_t len = 10000)
	{
		m_maxGopCacheLen = len;
	}

	void setPeerBandwidth(uint32_t size)
	{
		m_peerBandwidth = size;
	}

protected:

	uint32_t getChunkSize() const 
	{
		return m_maxChunkSize;
	}

	uint32_t getGopCacheLen() const
	{
		return m_maxGopCacheLen;
	}

	uint32_t getAcknowledgementSize() const
	{
		return m_acknowledgementSize;
	}

	uint32_t getPeerBandwidth() const
	{
		return m_peerBandwidth;
	}

	virtual int parseRtmpUrl(std::string url)
	{
		char ip[100] = { 0 };
		char streamPath[500] = { 0 };
		char app[100] = { 0 };
		char streamName[400] = { 0 };
		uint16_t port = 0;

		if (strstr(url.c_str(), "rtmp://") == NULL)
		{
			return -1;
		}

#if defined(__linux) || defined(__linux__)
		if (sscanf(url.c_str() + 7, "%[^:]:%hu/%s", ip, &port, streamPath) == 3)
#elif defined(WIN32) || defined(_WIN32)
		if (sscanf_s(url.c_str() + 7, "%[^:]:%hu/%s", ip, 100, &port, streamPath, 500) == 3)
#endif
		{
			m_port = port;
		}
#if defined(__linux) || defined(__linux__)
		else if (sscanf(url.c_str() + 7, "%[^/]/%s", ip, streamPath) == 2)
#elif defined(WIN32) || defined(_WIN32)
		else if (sscanf_s(url.c_str() + 7, "%[^/]/%s", ip, 100, streamPath, 500) == 2)
#endif
		{
			m_port = 1935;
		}
		else
		{
			return -1;
		}

		m_ip = ip;
		m_streamPath += "/";
		m_streamPath += streamPath;
		m_url = url;

#if defined(__linux) || defined(__linux__)
		if (sscanf(m_streamPath.c_str(), "/%[^/]/%s", app, streamName) != 2)
#elif defined(WIN32) || defined(_WIN32)
		if (sscanf_s(m_streamPath.c_str(), "/%[^/]/%s", app, 100, streamName, 400) != 2)
#endif
		{
			return -1;
		}

		m_app = app;
		m_streamName = streamName;
		m_swfUrl = m_tcUrl = std::string("rtmp://") + m_ip + ":" + std::to_string(m_port) + "/" + m_app;
		return 0;
	}

	std::string getUrl() const
	{
		return m_url;
	}

	std::string getStreamPath() const
	{
		return m_streamPath;
	}

	std::string getApp() const
	{
		return m_app;
	}

	std::string getStreamName() const
	{
		return m_streamName;
	}

	std::string getSwfUrl() const
	{
		return m_swfUrl;
	}

	std::string getTcUrl() const
	{
		return m_tcUrl;
	}


	uint16_t m_port = 1935;
	std::string m_url;
	std::string m_tcUrl, m_swfUrl;
	std::string m_ip;
	std::string m_app;
	std::string m_streamName;
	std::string m_streamPath;

	uint32_t m_peerBandwidth = 5000000;
	uint32_t m_acknowledgementSize = 5000000;
	uint32_t m_maxChunkSize = 128;
	uint32_t m_maxGopCacheLen = 0;
};

}

#endif
 