#include "RtmpSession.h"
#include "RtmpConnection.h"
#include "HttpFlvConnection.h"

using namespace xop;

RtmpSession::RtmpSession()
{
    
}

RtmpSession::~RtmpSession()
{
    
}

void RtmpSession::sendMetaData(AmfObjects& metaData)
{ 
    std::lock_guard<std::mutex> lock(m_mutex);    
    
	for (auto iter = m_rtmpClients.begin(); iter != m_rtmpClients.end(); )
    {
        auto conn = iter->second.lock(); 
        if (conn == nullptr) 
        {
			m_rtmpClients.erase(iter++);
        }
        else
        {	
            if(conn->isPlayer())
            {               
				conn->sendMetaData(metaData);
            }
			iter++;
        }
    }
} 

void RtmpSession::sendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);    

    for (auto iter = m_rtmpClients.begin(); iter != m_rtmpClients.end(); )
    {
        auto conn = iter->second.lock(); 
        if (conn == nullptr) // conn disconect
        {
			m_rtmpClients.erase(iter++);
        }
        else
        {	
            if(conn->isPlayer())
            {               
				conn->sendMediaData(type, timestamp, data, size);
            }
			iter++;
        }
    }

	for (auto iter = m_httpClients.begin(); iter != m_httpClients.end(); )
	{
		auto conn = iter->second.lock();
		if (conn == nullptr) // conn disconect
		{
			m_httpClients.erase(iter++);
		}
		else
		{
			if (!conn->hasFlvHeader())
			{
				conn->sendMediaData(RTMP_AVC_SEQUENCE_HEADER, 0, m_avcSequenceHeader, m_avcSequenceHeaderSize);
				conn->sendMediaData(RTMP_AAC_SEQUENCE_HEADER, 0, m_aacSequenceHeader, m_aacSequenceHeaderSize);
			}

			conn->sendMediaData(type, timestamp, data, size);
			iter++;
		}
	}

	return;
}

void RtmpSession::addRtmpClient(std::shared_ptr<RtmpConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);   
	m_rtmpClients[conn->fd()] = conn;
    if(conn->isPublisher())
    {
		m_publisher = conn;
        m_hasPublisher = true;
    }
	return;
}

void RtmpSession::removeRtmpClient(std::shared_ptr<RtmpConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);    
    if(conn->isPublisher())
    {
        m_hasPublisher = false;
    }
	m_rtmpClients.erase(conn->fd());
}

void RtmpSession::addHttpClient(std::shared_ptr<HttpFlvConnection> conn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_httpClients[conn->fd()] = conn;
}

void RtmpSession::removeHttpClient(std::shared_ptr<HttpFlvConnection> conn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_httpClients.erase(conn->fd());
}

void addHttpClient(std::shared_ptr<RtmpConnection> conn)
{

}

void removeHttpClient(std::shared_ptr<RtmpConnection> conn)
{

}

int RtmpSession::getClients()
{
    std::lock_guard<std::mutex> lock(m_mutex);

	int clients = 0;

	for (auto iter : m_rtmpClients)
	{
		auto conn = iter.second.lock();
		if (conn != nullptr) 
		{
			clients += 1;
		}
	}

	for (auto iter : m_httpClients)
	{
		auto conn = iter.second.lock();
		if (conn != nullptr)
		{
			clients += 1;
		}
	}

    return clients;
}

std::shared_ptr<RtmpConnection> RtmpSession::getPublisher()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_publisher.lock();
}
