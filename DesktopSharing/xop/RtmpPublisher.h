#ifndef XOP_RTMP_PUBLISHER_H
#define XOP_RTMP_PUBLISHER_H

#include <string>
#include <mutex>
#include "RtmpConnection.h"
#include "net/EventLoop.h"
#include "net/Timestamp.h"

namespace xop
{

class RtmpPublisher : public Rtmp
{
public:
	RtmpPublisher(xop::EventLoop *loop);
	~RtmpPublisher();

	int setMediaInfo(MediaInfo mediaInfo);

	int openUrl(std::string url, int msec = 0);
	void close();
	bool isConnected();

	int pushVideoFrame(uint8_t *data, uint32_t size); /* (sps pps)idr frame or p frame */

	int pushAudioFrame(uint8_t *data, uint32_t size);

private:
	friend class RtmpConnection;

	RtmpPublisher() {}
	bool isKeyFrame(uint8_t *data, uint32_t size);

	xop::EventLoop *m_eventLoop;
	std::mutex m_mutex;
	std::shared_ptr<RtmpConnection> m_rtmpConn;

	MediaInfo m_mediaIinfo;
	std::shared_ptr<char> m_avcSequenceHeader;
	std::shared_ptr<char> m_aacSequenceHeader;
	uint32_t m_avcSequenceHeaderSize = 0;
	uint32_t m_aacSequenceHeaderSize = 0;
	uint8_t m_audioTag = 0;
	bool m_hasKeyFrame = false;
	xop::Timestamp m_timestamp;
	uint64_t m_videoTimestamp = 0;
	uint64_t m_audioTimestamp = 0;

	const uint32_t kSamplingFrequency[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0};
};

}

#endif

