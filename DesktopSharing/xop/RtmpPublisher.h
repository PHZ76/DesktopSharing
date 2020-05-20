#ifndef XOP_RTMP_PUBLISHER_H
#define XOP_RTMP_PUBLISHER_H

#include <string>
#include <mutex>
#include "RtmpConnection.h"
#include "net/EventLoop.h"
#include "net/Timestamp.h"

namespace xop
{

class RtmpPublisher : public Rtmp, public std::enable_shared_from_this<RtmpPublisher>
{
public:
	static std::shared_ptr<RtmpPublisher> Create(xop::EventLoop* loop);
	~RtmpPublisher();

	int SetMediaInfo(MediaInfo media_info);

	int  OpenUrl(std::string url, int msec, std::string& status);
	void Close();

	bool IsConnected();

	int PushVideoFrame(uint8_t *data, uint32_t size); /* (sps pps)idr frame or p frame */
	int PushAudioFrame(uint8_t *data, uint32_t size);

private:
	friend class RtmpConnection;

	RtmpPublisher(xop::EventLoop *event_loop);
	bool IsKeyFrame(uint8_t* data, uint32_t size);

	xop::EventLoop *event_loop_ = nullptr;
	TaskScheduler *task_scheduler_ = nullptr;
	std::mutex mutex_;
	std::shared_ptr<RtmpConnection> rtmp_conn_;

	MediaInfo media_info_;
	std::shared_ptr<char> avc_sequence_header_;
	std::shared_ptr<char> aac_sequence_header_;
	uint32_t avc_sequence_header_size_ = 0;
	uint32_t aac_sequence_header_size_ = 0;
	uint8_t audio_tag_ = 0;
	bool has_key_frame_ = false;
	xop::Timestamp timestamp_;
	uint64_t video_timestamp_ = 0;
	uint64_t audio_timestamp_ = 0;

	const uint32_t kSamplingFrequency[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0};
};

}

#endif

