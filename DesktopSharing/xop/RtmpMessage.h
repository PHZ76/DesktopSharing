#ifndef RTMP_MESSAGE_H
#define RTMP_MESSAGE_H

#include <cstdint>
#include <memory>

namespace xop {

// chunk header: basic_header + rtmp_message_header 
struct RtmpMessageHeader
{
	uint8_t timestamp[3];
	uint8_t length[3];
	uint8_t type_id;
	uint8_t stream_id[4]; /* –°∂À∏Ò Ω */
};

struct RtmpMessage
{
	uint32_t timestamp = 0;
	uint32_t length = 0;
	uint8_t  type_id = 0;
	uint32_t stream_id = 0;
	uint32_t extend_timestamp = 0;

	uint64_t _timestamp = 0;
	uint8_t  codecId = 0;

	uint8_t  csid = 0;
	uint32_t index = 0;
	std::shared_ptr<char> payload = nullptr;

	void Clear()
	{
		index = 0;
		timestamp = 0;
		extend_timestamp = 0;
	}

	bool IsCompleted() const 
	{
		if (index == length && payload != nullptr) {
			return true;
		}

		return false;
	}

	RtmpMessage &operator=(const RtmpMessage& rtmp_msg) 
	{
		if (this != &rtmp_msg) {
			this->timestamp = rtmp_msg.timestamp;
			this->length = rtmp_msg.length;
			this->type_id = rtmp_msg.type_id;
			this->stream_id = rtmp_msg.stream_id;
			this->extend_timestamp = rtmp_msg.extend_timestamp;
			this->_timestamp = rtmp_msg._timestamp;
			this->codecId = rtmp_msg.codecId;
			this->csid = rtmp_msg.csid;
			this->index = rtmp_msg.index;
			this->payload.reset(new char[this->length]);
			memcpy(this->payload.get(), rtmp_msg.payload.get(), this->length);
		}

		return *this;
	}
};

}

#endif