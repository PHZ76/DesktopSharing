#include "HttpFlvConnection.h"
#include "RtmpServer.h"
#include "net/Logger.h"

using namespace xop;

HttpFlvConnection::HttpFlvConnection(RtmpServer *rtmpServer, TaskScheduler* taskScheduler, SOCKET sockfd)
	: TcpConnection(taskScheduler, sockfd)
	, m_rtmpServer(rtmpServer)
	, m_taskScheduler(taskScheduler)
{
	this->SetReadCallback([this](std::shared_ptr<TcpConnection> conn, xop::BufferReader& buffer) {
		return this->onRead(buffer);
	});

	this->SetCloseCallback([this](std::shared_ptr<TcpConnection> conn) {
		this->onClose();
	});
}

HttpFlvConnection::~HttpFlvConnection()
{

}

bool HttpFlvConnection::onRead(BufferReader& buffer)
{
	if (buffer.FindLastCrlfCrlf() == nullptr)
	{
		return (buffer.ReadableBytes() >= 4096) ? false : true;
	}

	const char *firstCrlf = buffer.FindFirstCrlf();
	if (firstCrlf == nullptr)
	{
		return false;
	}

	std::string buf(buffer.Peek(), firstCrlf - buffer.Peek());

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
		this->Send(httpFlvHeader.c_str(), (uint32_t)httpFlvHeader.size());

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
			task_scheduler_->AddTimer([sessionPtr, conn] {
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

	m_isPlaying = true;

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

	auto conn = std::dynamic_pointer_cast<HttpFlvConnection>(shared_from_this());
	m_taskScheduler->AddTriggerEvent([conn, type, timestamp, payload, payloadSize] {		
		if (type == RTMP_VIDEO)
		{
			if (!conn->m_hasKeyFrame)
			{
				uint8_t frameType = (payload.get()[0] >> 4) & 0x0f;
				uint8_t codecId = payload.get()[0] & 0x0f;
				if (frameType == 1 && codecId == RTMP_CODEC_ID_H264)
				{
					conn->m_hasKeyFrame = true;
				}
				else
				{
					return ;
				}
			}

			if (!conn->m_hasFlvHeader)
			{
				conn->sendFlvHeader();
				conn->sendFlvTag(conn->FLV_TAG_TYPE_VIDEO, 0, conn->m_avcSequenceHeader, conn->m_avcSequenceHeaderSize);
				conn->sendFlvTag(conn->FLV_TAG_TYPE_AUDIO, 0, conn->m_aacSequenceHeader, conn->m_aacSequenceHeaderSize);
			}

			conn->sendFlvTag(conn->FLV_TAG_TYPE_VIDEO, timestamp, payload, payloadSize);
		}
		else if (type == RTMP_AUDIO)
		{
			if (!conn->m_hasKeyFrame && conn->m_avcSequenceHeaderSize>0)
			{
				return ;
			}

			if (!conn->m_hasFlvHeader)
			{
				conn->sendFlvHeader();
				conn->sendFlvTag(conn->FLV_TAG_TYPE_AUDIO, 0, conn->m_aacSequenceHeader, conn->m_aacSequenceHeaderSize);
			}

			conn->sendFlvTag(conn->FLV_TAG_TYPE_AUDIO, timestamp, payload, payloadSize);
		}
	});

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

	this->Send(flvHeader, 9);
	char previousTagSize[4] = { 0x0, 0x0, 0x0, 0x0 };
	this->Send(previousTagSize, 4);

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
	WriteUint24BE(tagHeader + 1, payloadSize);
	tagHeader[4] = (timestamp >> 16) & 0xff;
	tagHeader[5] = (timestamp >> 8) & 0xff;
	tagHeader[6] = timestamp & 0xff;
	tagHeader[7] = (timestamp >> 24) & 0xff;

	WriteUint32BE(previousTagSize, payloadSize + 11);

	this->Send(tagHeader, 11);
	this->Send(payload.get(), payloadSize);
	this->Send(previousTagSize, 4);

	return 0;
}