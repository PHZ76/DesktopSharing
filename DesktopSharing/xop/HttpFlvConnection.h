#ifndef XOP_HTTP_FLV_CONNECTION_H
#define XOP_HTTP_FLV_CONNECTION_H

#include "net/EventLoop.h"
#include "net/TcpConnection.h"

namespace xop
{

class RtmpServer;

class HttpFlvConnection : public TcpConnection
{
public:
	HttpFlvConnection(std::shared_ptr<RtmpServer> rtmp_server, TaskScheduler* taskScheduler, SOCKET sockfd);
	virtual ~HttpFlvConnection();

	bool HasFlvHeader() const 
	{ return has_flv_header_; }
	
	bool IsPlaying() const
	{ return is_playing_; }

	bool SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);

	void ResetKeyFrame()
	{ has_key_frame_ = false; }

private:
	friend class RtmpSession;

	bool OnRead(BufferReader& buffer);
	void OnClose();
	
	void SendFlvHeader();
	int  SendFlvTag(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);

	std::weak_ptr<RtmpServer> rtmp_server_;
	TaskScheduler* task_scheduler_ = nullptr;
	std::string stream_path_;

	std::shared_ptr<char> avc_sequence_header_;
	std::shared_ptr<char> aac_sequence_header_;
	uint32_t avc_sequence_header_size_ = 0;
	uint32_t aac_sequence_header_size_ = 0;
	bool has_key_frame_ = false;
	bool has_flv_header_ = false;
	bool is_playing_ = false;

	const uint8_t FLV_TAG_TYPE_AUDIO = 0x8;
	const uint8_t FLV_TAG_TYPE_VIDEO = 0x9;
};

};

#endif
