#include "HttpFlvConnection.h"
#include "RtmpServer.h"
#include "net/Logger.h"

using namespace xop;

HttpFlvConnection::HttpFlvConnection(std::shared_ptr<RtmpServer> rtmp_server, TaskScheduler* scheduler, SOCKET sockfd)
	: TcpConnection(scheduler, sockfd)
	, rtmp_server_(rtmp_server)
	, task_scheduler_(scheduler)
{
	this->SetReadCallback([this](std::shared_ptr<TcpConnection> conn, xop::BufferReader& buffer) {
		return this->OnRead(buffer);
	});

	this->SetCloseCallback([this](std::shared_ptr<TcpConnection> conn) {
		this->OnClose();
	});
}

HttpFlvConnection::~HttpFlvConnection()
{

}

bool HttpFlvConnection::OnRead(BufferReader& buffer)
{
	if (buffer.FindLastCrlfCrlf() == nullptr) {
		return (buffer.ReadableBytes() >= 4096) ? false : true;
	}

	const char* last_crlf_crlf = buffer.FindLastCrlfCrlf();
	if (last_crlf_crlf == nullptr) {
		return true;
	}

	std::string buf(buffer.Peek(), last_crlf_crlf - buffer.Peek());
	buffer.RetrieveUntil(last_crlf_crlf + 4);

	auto pos1 = buf.find("GET");
	auto pos2 = buf.find(".flv");
	if (pos1 == std::string::npos || pos2 == std::string::npos) {
		return false;
	}

	stream_path_ = buf.substr(pos1 + 3, pos2 - 3).c_str();

	pos1 = stream_path_.find_first_not_of(" ");
	pos2 = stream_path_.find_last_not_of(" ");
	stream_path_ = stream_path_.substr(pos1, pos2);
	//printf("%s\n", stream_path_.c_str());

	auto rtmp_server = rtmp_server_.lock();
	if (rtmp_server) {
		std::string http_header = "HTTP/1.1 200 OK\r\nContent-Type: video/x-flv\r\n\r\n";
		this->Send(http_header.c_str(), (uint32_t)http_header.size());

		auto session = rtmp_server->GetSession(stream_path_);
		if (session != nullptr) {
			session->AddHttpClient(std::dynamic_pointer_cast<HttpFlvConnection>(shared_from_this()));
		}
	}
	else {
		return false;
	}
	
	return true;
}

void HttpFlvConnection::OnClose()
{
	auto rtmp_server = rtmp_server_.lock();
	if (rtmp_server != nullptr) {
		auto session = rtmp_server->GetSession(stream_path_);
		if (session != nullptr) {
			auto conn = std::dynamic_pointer_cast<HttpFlvConnection>(shared_from_this());
			task_scheduler_->AddTimer([session, conn] {
				session->RemoveHttpClient(conn);
				return false;
			}, 1);
		}
	}
}


bool HttpFlvConnection::SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size)
{	 
	if (payload_size == 0) {
		return false;
	}

	is_playing_ = true;

	if (type == RTMP_AVC_SEQUENCE_HEADER) {
		avc_sequence_header_ = payload;
		avc_sequence_header_size_ = payload_size;
		return true;
	}
	else if (type == RTMP_AAC_SEQUENCE_HEADER) {
		aac_sequence_header_ = payload;
		aac_sequence_header_size_ = payload_size;
		return true;
	}

	auto conn = std::dynamic_pointer_cast<HttpFlvConnection>(shared_from_this());
	task_scheduler_->AddTriggerEvent([conn, type, timestamp, payload, payload_size] {		
		if (type == RTMP_VIDEO) {
			if (!conn->has_key_frame_) {
				uint8_t frame_type = (payload.get()[0] >> 4) & 0x0f;
				uint8_t codec_id = payload.get()[0] & 0x0f;
				if (frame_type == 1 && codec_id == RTMP_CODEC_ID_H264) {
					conn->has_key_frame_ = true;
				}
				else {
					return ;
				}
			}

			if (!conn->has_flv_header_) {
				conn->SendFlvHeader();
				conn->SendFlvTag(conn->FLV_TAG_TYPE_VIDEO, 0, conn->avc_sequence_header_, conn->avc_sequence_header_size_);
				conn->SendFlvTag(conn->FLV_TAG_TYPE_AUDIO, 0, conn->aac_sequence_header_, conn->aac_sequence_header_size_);
			}

			conn->SendFlvTag(conn->FLV_TAG_TYPE_VIDEO, timestamp, payload, payload_size);
		}
		else if (type == RTMP_AUDIO) {
			if (!conn->has_key_frame_ && conn->avc_sequence_header_size_>0) {
				return ;
			}

			if (!conn->has_flv_header_) {
				conn->SendFlvHeader();
				conn->SendFlvTag(conn->FLV_TAG_TYPE_AUDIO, 0, conn->aac_sequence_header_, conn->aac_sequence_header_size_);
			}

			conn->SendFlvTag(conn->FLV_TAG_TYPE_AUDIO, timestamp, payload, payload_size);
		}
	});

	return true;
}

void HttpFlvConnection::SendFlvHeader()
{
	char flv_header[9] = { 0x46, 0x4c, 0x56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x09 };

	if (avc_sequence_header_size_ > 0) {
		flv_header[4] |= 0x1;
	}

	if (aac_sequence_header_size_ > 0)  {
		flv_header[4] |= 0x4;
	}

	this->Send(flv_header, 9);
	char previous_tag_size[4] = { 0x0, 0x0, 0x0, 0x0 };
	this->Send(previous_tag_size, 4);

	has_flv_header_ = true;
}

int HttpFlvConnection::SendFlvTag(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size)
{
	if (payload_size == 0) {
		return -1;
	}

	char tag_header[11] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	char previous_tag_size[4] = { 0x0, 0x0, 0x0, 0x0 };

	tag_header[0] = type;
	WriteUint24BE(tag_header + 1, payload_size);
	tag_header[4] = (timestamp >> 16) & 0xff;
	tag_header[5] = (timestamp >> 8) & 0xff;
	tag_header[6] = timestamp & 0xff;
	tag_header[7] = (timestamp >> 24) & 0xff;

	WriteUint32BE(previous_tag_size, payload_size + 11);

	this->Send(tag_header, 11);
	this->Send(payload.get(), payload_size);
	this->Send(previous_tag_size, 4);

	return 0;
}