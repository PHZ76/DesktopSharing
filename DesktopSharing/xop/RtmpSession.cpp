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

	if (this->m_maxGopCacheLen > 0)
	{
		this->saveGop(type, timestamp, data, size);
	}

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
				if (!conn->isPlaying())
				{
					conn->sendMetaData(m_metaData);
					conn->sendMediaData(RTMP_AVC_SEQUENCE_HEADER, 0, this->m_avcSequenceHeader, this->m_avcSequenceHeaderSize);
					conn->sendMediaData(RTMP_AAC_SEQUENCE_HEADER, 0, this->m_aacSequenceHeader, this->m_aacSequenceHeaderSize);
					
					if (m_gopCache.size() > 0)
					{						
						auto gop = m_gopCache.begin()->second;
						for (auto iter : *gop)
						{
							if (iter->type == RTMP_VIDEO)
							{								
								conn->sendVideoData(iter->timestamp, iter->data, iter->size);
							}
							else if (iter->type == RTMP_AUDIO)
							{
								conn->sendAudioData(iter->timestamp, iter->data, iter->size);
							}
						}
					}
				}
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
			if (!conn->isPlaying())
			{
				conn->sendMediaData(RTMP_AVC_SEQUENCE_HEADER, 0, m_avcSequenceHeader, m_avcSequenceHeaderSize);
				conn->sendMediaData(RTMP_AAC_SEQUENCE_HEADER, 0, m_aacSequenceHeader, m_aacSequenceHeaderSize);

				if (m_gopCache.size() > 0)
				{
					auto gop = m_gopCache.begin()->second;
					for (auto iter : *gop)
					{
						if (iter->type == RTMP_VIDEO)
						{
							conn->sendMediaData(RTMP_VIDEO, iter->timestamp, iter->data, iter->size);
						}
						else if (iter->type == RTMP_AUDIO)
						{
							conn->sendMediaData(RTMP_AUDIO, iter->timestamp, iter->data, iter->size);
						}
					}

					conn->resetKeyFrame();
				}
			}

			conn->sendMediaData(type, timestamp, data, size);
			iter++;
		}
	}

	return;
}

void RtmpSession::saveGop(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size)
{
	uint8_t *payload = (uint8_t *)data.get();
	uint8_t frameType = 0;
	uint8_t codecId = 0;
	std::shared_ptr<AVFrame> avFrame = nullptr;
	std::shared_ptr<std::list<AVFramePtr>> gop = nullptr;
	if (m_gopCache.size() > 0)
	{
		gop = m_gopCache[m_gopIndex];
	}

	if (type == RTMP_VIDEO)
	{
		frameType = (payload[0] >> 4) & 0x0f;
		codecId = payload[0] & 0x0f;
		if (frameType == 1 && codecId == RTMP_CODEC_ID_H264)
		{
			if (payload[1] == 1)
			{
				if (m_maxGopCacheLen > 0)
				{
					if (m_gopCache.size() == 2)
					{
						m_gopCache.erase(m_gopCache.begin());
					}
					m_gopIndex += 1;
					gop.reset(new std::list<AVFramePtr>);
					m_gopCache[m_gopIndex] = gop;
					avFrame.reset(new AVFrame);
				}
			}
		}
		else if (codecId == RTMP_CODEC_ID_H264 && gop != nullptr)
		{
			if (m_maxGopCacheLen > 0 && gop->size() >= 1 && gop->size() < m_maxGopCacheLen)
			{
				avFrame.reset(new AVFrame);
			}
		}
	}
	else if (type == RTMP_AUDIO && gop != nullptr)
	{
		uint8_t soundFormat = (payload[0] >> 4) & 0x0f;
		uint8_t soundSize = (payload[0] >> 1) & 0x01;
		uint8_t soundRate = (payload[0] >> 2) & 0x03;

		if (soundFormat == RTMP_CODEC_ID_AAC)
		{			
			if (m_maxGopCacheLen > 0 && gop->size() >= 2 && gop->size() < m_maxGopCacheLen)
			{
				if (timestamp > 0)
				{
					avFrame.reset(new AVFrame);
				}
			}
		}
	}

	if (avFrame != nullptr && gop != nullptr)
	{
		avFrame->type = type;
		avFrame->timestamp = timestamp;		
		avFrame->size = size;
		avFrame->data.reset(new char[size]);
		memcpy(avFrame->data.get(), data.get(), size);
		gop->push_back(avFrame);
	}
}

void RtmpSession::addRtmpClient(std::shared_ptr<RtmpConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);   
	m_rtmpClients[conn->GetSocket()] = conn;
    if(conn->isPublisher())
    {
		m_avcSequenceHeader = nullptr;
		m_aacSequenceHeader = nullptr;
		m_avcSequenceHeaderSize = 0;
		m_aacSequenceHeaderSize = 0;
		m_gopCache.clear();
		m_gopIndex = 0;		
        m_hasPublisher = true;
		m_publisher = conn;
    }
	return;
}

void RtmpSession::removeRtmpClient(std::shared_ptr<RtmpConnection> conn)
{
    std::lock_guard<std::mutex> lock(m_mutex);    
    if(conn->isPublisher())
    {
		m_avcSequenceHeader = nullptr;
		m_aacSequenceHeader = nullptr;
		m_avcSequenceHeaderSize = 0;
		m_aacSequenceHeaderSize = 0;
		m_gopCache.clear();
		m_gopIndex = 0;
        m_hasPublisher = false;
    }
	m_rtmpClients.erase(conn->GetSocket());
}

void RtmpSession::addHttpClient(std::shared_ptr<HttpFlvConnection> conn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_httpClients[conn->GetSocket()] = conn;
}

void RtmpSession::removeHttpClient(std::shared_ptr<HttpFlvConnection> conn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_httpClients.erase(conn->GetSocket());
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
