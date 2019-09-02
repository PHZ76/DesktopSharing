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
	HttpFlvConnection(RtmpServer *rtmpServer, TaskScheduler* taskScheduler, SOCKET sockfd);
	~HttpFlvConnection();

	bool hasFlvHeader() const 
	{ return m_hasFlvHeader; }
	
	bool sendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize);

private:
	friend class RtmpSession;

	bool onRead(BufferReader& buffer);
	void onClose();
	
	void sendFlvHeader();
	int  sendFlvTag(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize);

	RtmpServer *m_rtmpServer = nullptr;
	std::string m_streamPath;

	std::shared_ptr<char> m_avcSequenceHeader;
	std::shared_ptr<char> m_aacSequenceHeader;
	uint32_t m_avcSequenceHeaderSize = 0;
	uint32_t m_aacSequenceHeaderSize = 0;
	bool m_hasKeyFrame = false;
	bool m_hasFlvHeader = false;

	const uint8_t FLV_TAG_TYPE_AUDIO = 0x8;
	const uint8_t FLV_TAG_TYPE_VIDEO = 0x9;
};

};

#endif
