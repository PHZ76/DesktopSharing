#ifndef XOP_RTMP_SESSION_H
#define XOP_RTMP_SESSION_H

#include "net/Socket.h"
#include "amf.h"
#include <memory>
#include <mutex>

namespace xop
{
    
class RtmpConnection;
class HttpFlvConnection;

class RtmpSession
{
public:
    using Ptr = std::shared_ptr<RtmpSession>;
    
    RtmpSession();
    ~RtmpSession();

    void setMetaData(AmfObjects metaData)
    { m_metaData = metaData; }
    
	void setAvcSequenceHeader(std::shared_ptr<char> avcSequenceHeader, uint32_t avcSequenceHeaderSize)
	{
		m_avcSequenceHeader = avcSequenceHeader;
		m_avcSequenceHeaderSize = avcSequenceHeaderSize;
	}

	void setAacSequenceHeader(std::shared_ptr<char> aacSequenceHeader, uint32_t aacSequenceHeaderSize)
	{
		m_aacSequenceHeader = aacSequenceHeader;
		m_aacSequenceHeaderSize = aacSequenceHeaderSize;
	}

    AmfObjects getMetaData() const 
    { return m_metaData; }   

    void addRtmpClient(std::shared_ptr<RtmpConnection> conn);
    void removeRtmpClient(std::shared_ptr<RtmpConnection> conn);
	void addHttpClient(std::shared_ptr<HttpFlvConnection> conn);
	void removeHttpClient(std::shared_ptr<HttpFlvConnection> conn);
    int  getClients();
        
    void sendMetaData(AmfObjects& metaData);
    void sendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size);
    
    bool isPublishing() const 
    { return m_hasPublisher; }
    
	std::shared_ptr<RtmpConnection> getPublisher();

private:        
    std::mutex m_mutex;
    AmfObjects m_metaData;
    bool m_hasPublisher = false;
	std::weak_ptr<RtmpConnection> m_publisher;
    std::unordered_map<SOCKET, std::weak_ptr<RtmpConnection>> m_rtmpClients;
	std::unordered_map<SOCKET, std::weak_ptr<HttpFlvConnection>> m_httpClients;

	std::shared_ptr<char> m_avcSequenceHeader;
	std::shared_ptr<char> m_aacSequenceHeader;
	uint32_t m_avcSequenceHeaderSize = 0;
	uint32_t m_aacSequenceHeaderSize = 0;
};

}

#endif
