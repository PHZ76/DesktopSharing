#ifndef XOP_RTMP_SESSION_H
#define XOP_RTMP_SESSION_H

#include "net/Socket.h"
#include "amf.h"
#include <memory>
#include <mutex>
#include <list>

namespace xop
{
    
class RtmpConnection;
class HttpFlvConnection;

class RtmpSession
{
public:
	using Ptr = std::shared_ptr<RtmpSession>;

	RtmpSession();
	virtual ~RtmpSession();

	void SetMetaData(AmfObjects metaData)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		meta_data_ = metaData;
	}

	void SetAvcSequenceHeader(std::shared_ptr<char> avcSequenceHeader, uint32_t avcSequenceHeaderSize)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		avc_sequence_header_ = avcSequenceHeader;
		avc_sequence_header_size_ = avcSequenceHeaderSize;
	}

	void SetAacSequenceHeader(std::shared_ptr<char> aacSequenceHeader, uint32_t aacSequenceHeaderSize)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		aac_sequence_header_ = aacSequenceHeader;
		aac_sequence_header_size_ = aacSequenceHeaderSize;
	}

	AmfObjects GetMetaData()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return meta_data_;
	}

	void AddRtmpClient(std::shared_ptr<RtmpConnection> conn);
	void RemoveRtmpClient(std::shared_ptr<RtmpConnection> conn);
	void AddHttpClient(std::shared_ptr<HttpFlvConnection> conn);
	void RemoveHttpClient(std::shared_ptr<HttpFlvConnection> conn);
	int  GetClients();
	
	void SendMetaData(AmfObjects& metaData);
	void SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size);

	std::shared_ptr<RtmpConnection> GetPublisher();

	void SetGopCache(uint32_t cacheLen)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		max_gop_cache_len_ = cacheLen;
	}

	void SaveGop(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size);

private:        

    std::mutex mutex_;
    AmfObjects meta_data_;
    bool has_publisher_ = false;
	std::weak_ptr<RtmpConnection> publisher_;
    std::unordered_map<SOCKET, std::weak_ptr<RtmpConnection>> rtmp_clients_;
	std::unordered_map<SOCKET, std::weak_ptr<HttpFlvConnection>> http_clients_;

	std::shared_ptr<char> avc_sequence_header_;
	std::shared_ptr<char> aac_sequence_header_;
	uint32_t avc_sequence_header_size_ = 0;
	uint32_t aac_sequence_header_size_ = 0;
	uint64_t gop_index_ = 0;
	uint32_t max_gop_cache_len_ = 0;

	struct AVFrame {
		uint8_t  type = 0;
		uint64_t timestamp = 0;
		uint32_t size = 0;
		std::shared_ptr<char> data = nullptr;
	};
	typedef std::shared_ptr<AVFrame> AVFramePtr;
	std::map<uint64_t, std::shared_ptr<std::list<AVFramePtr>>> gop_cache_;
};

}

#endif
