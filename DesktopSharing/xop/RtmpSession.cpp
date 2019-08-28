#include "RtmpSession.h"

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
    if(m_clients.size() == 0)
    {
        return ;
    }
    
	for (auto iter = m_clients.begin(); iter != m_clients.end(); )
    {
        auto conn = iter->second.lock(); 
        if (conn == nullptr) 
        {
            m_clients.erase(iter++);
        }
        else
        {	
            RtmpConnection* player = (RtmpConnection*)conn.get();
            if(player->isPlayer())
            {               
                player->sendMetaData(metaData);          
            }
			iter++;
        }
    }
} 

void RtmpSession::sendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);    
    if(m_clients.size() <= 1 || size == 0)
    {
        return ;
    }

    for (auto iter = m_clients.begin(); iter != m_clients.end(); )
    {
        auto conn = iter->second.lock(); 
        if (conn == nullptr) // conn disconect
        {
            m_clients.erase(iter++);
        }
        else
        {	
            RtmpConnection* player = (RtmpConnection*)conn.get();
            if(player->isPlayer())
            {               
                player->sendMediaData(type, timestamp, data, size);
            }
			iter++;
        }
    }

	return;
}

void RtmpSession::addClient(std::shared_ptr<TcpConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);   
    m_clients[conn->fd()] = conn;   
    if(((RtmpConnection*)conn.get())->isPublisher())
    {
		m_publisher = conn;
        m_hasPublisher = true;
    }
	return;
}

void RtmpSession::removeClient(std::shared_ptr<TcpConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);    
    if(((RtmpConnection*)conn.get())->isPublisher())
    {
        m_hasPublisher = false;
    }
	m_clients.erase(conn->fd());
}

int RtmpSession::getClients()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (int)m_clients.size();
}

std::shared_ptr<TcpConnection> RtmpSession::getPublisher()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_publisher.lock();
}