#include "RtmpChunk.h"
#include "rtmp.h"

using namespace xop;

RtmpChunk::RtmpChunk()
{
	state_ = PARSE_HEADER;
	chunk_stream_id_ = -1;
	stream_id_ = kDefaultStreamId;
}

RtmpChunk::~RtmpChunk()
{

}

int RtmpChunk::Parse(BufferReader& in_buffer, RtmpMessage& out_rtmp_msg)
{
	int ret = 0;

	if (!in_buffer.ReadableBytes()) {
		return 0;
	}

	if (state_ == PARSE_HEADER) {
		ret = ParseChunkHeader(in_buffer);
	}
	else if (state_ == PARSE_BODY)
	{
		ret = ParseChunkBody(in_buffer);
		if (ret > 0 && chunk_stream_id_ >= 0) {
			auto& rtmp_msg = rtmp_messages_[chunk_stream_id_];

			if (rtmp_msg.index == rtmp_msg.length) {
				if (rtmp_msg.timestamp >= 0xffffff) {
					rtmp_msg._timestamp += rtmp_msg.extend_timestamp;
				}
				else {
					rtmp_msg._timestamp += rtmp_msg.timestamp;
				}

				out_rtmp_msg = rtmp_msg;
				chunk_stream_id_ = -1;
				rtmp_msg.Clear();
			}
		}
	}

	return ret;
}

int RtmpChunk::ParseChunkHeader(BufferReader& buffer)
{
	uint32_t bytes_used = 0;
	uint8_t* buf = (uint8_t*)buffer.Peek();
	uint32_t buf_size = buffer.ReadableBytes();

	uint8_t flags = buf[bytes_used];
	bytes_used += 1;

	uint8_t csid = flags & 0x3f; // chunk stream id
	if (csid == 0) { // csid [64, 319] 
		if (buf_size < (bytes_used + 2)) {
			return 0;
		}

		csid += buf[bytes_used] + 64;
		bytes_used += 1;
	}
	else if (csid == 1) { // csid [64, 65599]
		if (buf_size < (3 + bytes_used)) {
			return 0;
		}
		csid += buf[bytes_used + 1] * 255 + buf[bytes_used] + 64;
		bytes_used += 2;
	}

	uint8_t fmt = (flags >> 6); // message header type
	if (fmt >= 4) {
		return -1;
	}

	uint32_t header_len = kChunkMessageHeaderLen[fmt]; // basic_header + message_header 
	if (buf_size < (header_len + bytes_used)) {
		return 0;
	}

	RtmpMessageHeader header;
	memset(&header, 0, sizeof(RtmpMessageHeader));
	memcpy(&header, buf + bytes_used, header_len);
	bytes_used += header_len;

	auto& rtmp_msg = rtmp_messages_[csid];
	chunk_stream_id_ = rtmp_msg.csid = csid;

	if (fmt == RTMP_CHUNK_TYPE_0 || fmt == RTMP_CHUNK_TYPE_1) {
		uint32_t length = ReadUint24BE((char*)header.length);
		if (rtmp_msg.length != length || !rtmp_msg.payload) {
			rtmp_msg.length = length;
			rtmp_msg.payload.reset(new char[rtmp_msg.length]);
		}
		rtmp_msg.index = 0;
		rtmp_msg.type_id = header.type_id;
	}

	if (fmt == RTMP_CHUNK_TYPE_0) {
		rtmp_msg.stream_id = ReadUint24LE((char*)header.stream_id);
	}

	uint32_t timestamp = ReadUint24BE((char*)header.timestamp);
	uint32_t extend_timestamp = 0;
	if (timestamp >= 0xffffff || rtmp_msg.timestamp >= 0xffffff) {
		if (buf_size < (4 + bytes_used)) {
			return 0;
		}
		extend_timestamp = ReadUint32BE((char*)buf + bytes_used);
		bytes_used += 4;
	}

	if (rtmp_msg.index == 0) { // first chunk
		if (fmt == RTMP_CHUNK_TYPE_0) {
			// absolute timestamp 
			rtmp_msg._timestamp = 0;
			rtmp_msg.timestamp = timestamp;
			rtmp_msg.extend_timestamp = extend_timestamp;
		}
		else {
			// relative timestamp (timestamp delta)
			if (rtmp_msg.timestamp >= 0xffffff) {
				rtmp_msg.extend_timestamp += extend_timestamp;
			}
			else {
				rtmp_msg.timestamp += timestamp;
			}
		}
	}

	state_ = PARSE_BODY;
	buffer.Retrieve(bytes_used);
	return bytes_used;
}

int RtmpChunk::ParseChunkBody(BufferReader& buffer)
{
	uint32_t bytes_used = 0;
	uint8_t* buf = (uint8_t*)buffer.Peek();
	uint32_t buf_size = buffer.ReadableBytes();

	if (chunk_stream_id_ < 0) {
		return -1;
	}

	auto& rtmp_msg = rtmp_messages_[chunk_stream_id_];
	uint32_t chunk_size = rtmp_msg.length - rtmp_msg.index;

	if (chunk_size > in_chunk_size_) {
		chunk_size = in_chunk_size_;
	}

	if (buf_size < (chunk_size + bytes_used)) {
		return 0;
	}

	if (rtmp_msg.index + chunk_size > rtmp_msg.length) {
		return -1;
	}

	memcpy(rtmp_msg.payload.get() + rtmp_msg.index, buf + bytes_used, chunk_size);
	bytes_used += chunk_size;
	rtmp_msg.index += chunk_size;

	if (rtmp_msg.index >= rtmp_msg.length || 
		rtmp_msg.index%in_chunk_size_ == 0) {
		state_ = PARSE_HEADER;
	}

	buffer.Retrieve(bytes_used);
	return bytes_used;
}

int RtmpChunk::CreateBasicHeader(uint8_t fmt, uint32_t csid, char* buf)
{
	int len = 0;

	if (csid >= 64 + 255) {
		buf[len++] = (fmt << 6) | 1;
		buf[len++] = (csid - 64) & 0xFF;
		buf[len++] = ((csid - 64) >> 8) & 0xFF;
	}
	else if (csid >= 64) {
		buf[len++] = (fmt << 6) | 0;
		buf[len++] = (csid - 64) & 0xFF;
	}
	else {
		buf[len++] = (fmt << 6) | csid;
	}
	return len;
}

int RtmpChunk::CreateMessageHeader(uint8_t fmt, RtmpMessage& rtmp_msg, char* buf)
{
	int len = 0;

	if (fmt <= 2) {
		if (rtmp_msg._timestamp < 0xffffff) {
			WriteUint24BE((char*)buf, (uint32_t)rtmp_msg._timestamp);
		}
		else {
			WriteUint24BE((char*)buf, 0xffffff);
		}
		len += 3;
	}

	if (fmt <= 1) {
		WriteUint24BE((char*)buf + len, rtmp_msg.length);
		len += 3;
		buf[len++] = rtmp_msg.type_id;
	}

	if (fmt == 0) {
		WriteUint32LE((char*)buf + len, rtmp_msg.stream_id);
		len += 4;
	}

	return len;
}

int RtmpChunk::CreateChunk(uint32_t csid, RtmpMessage& rtmp_msg, char* buf, uint32_t buf_size)
{
	uint32_t buf_offset = 0, payload_offset = 0;
	uint32_t capacity = rtmp_msg.length + rtmp_msg.length / out_chunk_size_ * 5;
	if (buf_size < capacity) {
		return -1;
	}

	buf_offset += CreateBasicHeader(0, csid, buf + buf_offset); //first chunk
	buf_offset += CreateMessageHeader(0, rtmp_msg, buf + buf_offset);
	if (rtmp_msg._timestamp >= 0xffffff) {
		WriteUint32BE((char*)buf + buf_offset, (uint32_t)rtmp_msg._timestamp);
		buf_offset += 4;
	}

	while (rtmp_msg.length > 0)
	{
		if (rtmp_msg.length > out_chunk_size_) {
			memcpy(buf + buf_offset, rtmp_msg.payload.get() + payload_offset, out_chunk_size_);
			payload_offset += out_chunk_size_;
			buf_offset += out_chunk_size_;
			rtmp_msg.length -= out_chunk_size_;

			buf_offset += CreateBasicHeader(3, csid, buf + buf_offset);
			if (rtmp_msg._timestamp >= 0xffffff) {
				WriteUint32BE(buf + buf_offset, (uint32_t)rtmp_msg._timestamp);
				buf_offset += 4;
			}
		}
		else {
			memcpy(buf + buf_offset, rtmp_msg.payload.get() + payload_offset, rtmp_msg.length);
			buf_offset += rtmp_msg.length;
			rtmp_msg.length = 0;
			break;
		}
	}

	return buf_offset;
}