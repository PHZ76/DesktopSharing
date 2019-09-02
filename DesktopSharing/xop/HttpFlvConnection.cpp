#define _CRT_SECURE_NO_WARNINGS
#include "HttpFlvConnection.h"
#include "RtmpServer.h"
#include "net/Logger.h"

using namespace xop;

HttpFlvConnection::HttpFlvConnection(RtmpServer *rtmpServer, TaskScheduler* taskScheduler, SOCKET sockfd)
	: TcpConnection(taskScheduler, sockfd)
	, m_rtmpServer(rtmpServer)
{
	this->setReadCallback([this](std::shared_ptr<TcpConnection> conn, xop::BufferReader& buffer) {
		return this->onRead(buffer);
	});

	this->setCloseCallback([this](std::shared_ptr<TcpConnection> conn) {
		this->onClose();
	});
}

HttpFlvConnection::~HttpFlvConnection()
{

}

bool HttpFlvConnection::onRead(BufferReader& buffer)
{
	if (buffer.findLastCrlfCrlf() == nullptr)
	{
		return (buffer.readableBytes() >= 4096) ? false : true;
	}

	const char *firstCrlf = buffer.findFirstCrlf();
	if (firstCrlf == nullptr)
	{
		return false;
	}

	std::string buf(buffer.peek(), firstCrlf - buffer.peek());
	//printf("%s\n", buf.c_str());

	auto pos1 = buf.find("GET");
	auto pos2 = buf.find(".flv");
	if (pos1 == std::string::npos || pos2 == std::string::npos)
	{
		return false;
	}

	m_streamPath = buf.substr(pos1 + 3, pos2 - 3).c_str();

	pos1 = m_streamPath.find_first_not_of(" ");
	pos2 = m_streamPath.find_last_not_of(" ");
	m_streamPath = m_streamPath.substr(pos1, pos2);
	//printf("%s\n", m_streamPath.c_str());

	if (m_rtmpServer != nullptr)
	{
		std::string httpFlvHeader = "HTTP/1.1 200 OK\r\nContent-Type: video/x-flv\r\n\r\n";
		this->send(httpFlvHeader.c_str(), (uint32_t)httpFlvHeader.size());

		auto sessionPtr = m_rtmpServer->getSession(m_streamPath);
		if (sessionPtr != nullptr)
		{
			sessionPtr->addHttpClient(std::dynamic_pointer_cast<HttpFlvConnection>(shared_from_this()));
		}
	}
	
	return true;
}

void HttpFlvConnection::onClose()
{
	if (m_rtmpServer != nullptr)
	{
		auto sessionPtr = m_rtmpServer->getSession(m_streamPath);
		if (sessionPtr != nullptr)
		{
			auto conn = std::dynamic_pointer_cast<HttpFlvConnection>(shared_from_this());
			_taskScheduler->addTimer([sessionPtr, conn] {
				sessionPtr->removeHttpClient(conn);
				return false;
			}, 1);
		}
	}
}


bool HttpFlvConnection::sendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize)
{
	if (payloadSize == 0)
	{
		return false;
	}

	if (type == RTMP_AVC_SEQUENCE_HEADER)
	{
		m_avcSequenceHeader = payload;
		m_avcSequenceHeaderSize = payloadSize;
		return true;
	}
	else if (type == RTMP_AAC_SEQUENCE_HEADER)
	{
		m_aacSequenceHeader = payload;
		m_aacSequenceHeaderSize = payloadSize;
		return true;
	}
	else if (type == RTMP_VIDEO)
	{
		if (!m_hasKeyFrame)
		{
			uint8_t frameType = (payload.get()[0] >> 4) & 0x0f;
			uint8_t codecId = payload.get()[0] & 0x0f;
			if (frameType == 1 && codecId == RTMP_CODEC_ID_H264)
			{
				m_hasKeyFrame = true;
			}
			else
			{
				return true;
			}
		}

		if (!m_hasFlvHeader)
		{
			this->sendFlvHeader();
			this->sendFlvTag(FLV_TAG_TYPE_VIDEO, 0, m_avcSequenceHeader, m_avcSequenceHeaderSize);
			this->sendFlvTag(FLV_TAG_TYPE_AUDIO, 0, m_aacSequenceHeader, m_aacSequenceHeaderSize);
		}

		this->sendFlvTag(FLV_TAG_TYPE_VIDEO, timestamp, payload, payloadSize);
	}
	else if (type == RTMP_AUDIO)
	{
		if (!m_hasKeyFrame)
		{
			return true;
		}

		this->sendFlvTag(FLV_TAG_TYPE_AUDIO, timestamp, payload, payloadSize);
	}
	
	return true;
}

void HttpFlvConnection::sendFlvHeader()
{
	char flvHeader[9] = { 0x46, 0x4c, 0x56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x09 };

	if (m_avcSequenceHeaderSize > 0)
	{
		flvHeader[4] |= 0x1;
	}

	if (m_aacSequenceHeaderSize > 0) 
	{
		flvHeader[4] |= 0x4;
	}

	this->send(flvHeader, 9);
	char previousTagSize[4] = { 0x0, 0x0, 0x0, 0x0 };
	this->send(previousTagSize, 4);

	m_hasFlvHeader = true;
}

int HttpFlvConnection::sendFlvTag(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize)
{
	if (payloadSize == 0)
	{
		return -1;
	}

	char tagHeader[11] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	char previousTagSize[4] = { 0x0, 0x0, 0x0, 0x0 };

	tagHeader[0] = type;
	writeUint24BE(tagHeader + 1, payloadSize);
	tagHeader[4] = (timestamp >> 16) & 0xff;
	tagHeader[5] = (timestamp >> 8) & 0xff;
	tagHeader[6] = timestamp & 0xff;
	tagHeader[7] = (timestamp >> 24) & 0xff;

	writeUint32BE(previousTagSize, payloadSize + 11);

	this->send(tagHeader, 11);
	this->send(payload.get(), payloadSize);
	this->send(previousTagSize, 4);

	return 0;
}