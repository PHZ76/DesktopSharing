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

void RtmpSession::SendMetaData(AmfObjects& metaData)
{ 
    std::lock_guard<std::mutex> lock(mutex_);    
    
	for (auto iter = rtmp_clients_.begin(); iter != rtmp_clients_.end(); ) {
        auto conn = iter->second.lock(); 
        if (conn == nullptr) {
			rtmp_clients_.erase(iter++);
        }
        else {	
            if(conn->IsPlayer()) {               
				conn->SendMetaData(metaData);
            }
			iter++;
        }
    }
} 

void RtmpSession::SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size)
{
    std::lock_guard<std::mutex> lock(mutex_);    

	if (this->max_gop_cache_len_ > 0) {
		this->SaveGop(type, timestamp, data, size);
	}

    for (auto iter = rtmp_clients_.begin(); iter != rtmp_clients_.end(); )
    {
        auto conn = iter->second.lock(); 
        if (conn == nullptr) {
			rtmp_clients_.erase(iter++);
        }
        else {	
            if(conn->IsPlayer()) {   
				if (!conn->IsPlaying()) {
					conn->SendMetaData(meta_data_);
					conn->SendMediaData(RTMP_AVC_SEQUENCE_HEADER, 0, this->avc_sequence_header_, this->avc_sequence_header_size_);
					conn->SendMediaData(RTMP_AAC_SEQUENCE_HEADER, 0, this->aac_sequence_header_, this->aac_sequence_header_size_);
					
					if (gop_cache_.size() > 0) {						
						auto gop = gop_cache_.begin()->second;
						for (auto iter : *gop) {
							if (iter->type == RTMP_VIDEO) {								
								conn->SendVideoData(iter->timestamp, iter->data, iter->size);
							}
							else if (iter->type == RTMP_AUDIO) {
								conn->SendAudioData(iter->timestamp, iter->data, iter->size);
							}
						}
					}
				}
				conn->SendMediaData(type, timestamp, data, size);
            }
			iter++;
        }
    }

	for (auto iter = http_clients_.begin(); iter != http_clients_.end(); )
	{
		auto conn = iter->second.lock();
		if (conn == nullptr) { // conn disconect 
			http_clients_.erase(iter++);
		}
		else {
			if (!conn->IsPlaying()) {
				conn->SendMediaData(RTMP_AVC_SEQUENCE_HEADER, 0, avc_sequence_header_, avc_sequence_header_size_);
				conn->SendMediaData(RTMP_AAC_SEQUENCE_HEADER, 0, aac_sequence_header_, aac_sequence_header_size_);

				if (gop_cache_.size() > 0) {
					auto gop = gop_cache_.begin()->second;
					for (auto iter : *gop) {
						if (iter->type == RTMP_VIDEO) {
							conn->SendMediaData(RTMP_VIDEO, iter->timestamp, iter->data, iter->size);
						}
						else if (iter->type == RTMP_AUDIO) {
							conn->SendMediaData(RTMP_AUDIO, iter->timestamp, iter->data, iter->size);
						}
					}

					conn->ResetKeyFrame();
				}
			}

			conn->SendMediaData(type, timestamp, data, size);
			iter++;
		}
	}

	return;
}

void RtmpSession::SaveGop(uint8_t type, uint64_t timestamp, std::shared_ptr<char> data, uint32_t size)
{
	uint8_t *payload = (uint8_t *)data.get();
	uint8_t frame_type = 0;
	uint8_t codec_id = 0;
	std::shared_ptr<AVFrame> av_frame = nullptr;
	std::shared_ptr<std::list<AVFramePtr>> gop = nullptr;
	if (gop_cache_.size() > 0) {
		gop = gop_cache_[gop_index_];
	}

	if (type == RTMP_VIDEO) {
		frame_type = (payload[0] >> 4) & 0x0f;
		codec_id = payload[0] & 0x0f;
		if (frame_type == 1 && codec_id == RTMP_CODEC_ID_H264) {
			if (payload[1] == 1) {
				if (max_gop_cache_len_ > 0) {
					if (gop_cache_.size() == 2) {
						gop_cache_.erase(gop_cache_.begin());
					}
					gop_index_ += 1;
					gop.reset(new std::list<AVFramePtr>);
					gop_cache_[gop_index_] = gop;
					av_frame.reset(new AVFrame);
				}
			}
		}
		else if (codec_id == RTMP_CODEC_ID_H264 && gop != nullptr) {
			if (max_gop_cache_len_ > 0 && gop->size() >= 1 && gop->size() < max_gop_cache_len_) {
				av_frame.reset(new AVFrame);
			}
		}
	}
	else if (type == RTMP_AUDIO && gop != nullptr) {
		uint8_t sound_format = (payload[0] >> 4) & 0x0f;
		//uint8_t sound_size = (payload[0] >> 1) & 0x01;
		//uint8_t sound_rate = (payload[0] >> 2) & 0x03;

		if (sound_format == RTMP_CODEC_ID_AAC) {
			if (max_gop_cache_len_ > 0 && gop->size() >= 2 && gop->size() < max_gop_cache_len_) {
				if (timestamp > 0) {
					av_frame.reset(new AVFrame);
				}
			}
		}
	}

	if (av_frame != nullptr && gop != nullptr) {
		av_frame->type = type;
		av_frame->timestamp = timestamp;		
		av_frame->size = size;
		av_frame->data.reset(new char[size]);
		memcpy(av_frame->data.get(), data.get(), size);
		gop->push_back(av_frame);
	}
}

void RtmpSession::AddRtmpClient(std::shared_ptr<RtmpConnection> conn)
{
    std::lock_guard<std::mutex> lock(mutex_);   
	rtmp_clients_[conn->GetSocket()] = conn;
    if(conn->IsPublisher()) {
		avc_sequence_header_ = nullptr;
		aac_sequence_header_ = nullptr;
		avc_sequence_header_size_ = 0;
		aac_sequence_header_size_ = 0;
		gop_cache_.clear();
		gop_index_ = 0;		
        has_publisher_ = true;
		publisher_ = conn;
    }
	return;
}

void RtmpSession::RemoveRtmpClient(std::shared_ptr<RtmpConnection> conn)
{
    std::lock_guard<std::mutex> lock(mutex_);    
    if(conn->IsPublisher()) {
		avc_sequence_header_ = nullptr;
		aac_sequence_header_ = nullptr;
		avc_sequence_header_size_ = 0;
		aac_sequence_header_size_ = 0;
		gop_cache_.clear();
		gop_index_ = 0;
        has_publisher_ = false;
    }
	rtmp_clients_.erase(conn->GetSocket());
}

void RtmpSession::AddHttpClient(std::shared_ptr<HttpFlvConnection> conn)
{
	std::lock_guard<std::mutex> lock(mutex_);
	http_clients_[conn->GetSocket()] = conn;
}

void RtmpSession::RemoveHttpClient(std::shared_ptr<HttpFlvConnection> conn)
{
	std::lock_guard<std::mutex> lock(mutex_);
	http_clients_.erase(conn->GetSocket());
}

void AddHttpClient(std::shared_ptr<RtmpConnection> conn)
{

}

void RemoveHttpClient(std::shared_ptr<RtmpConnection> conn)
{

}

int RtmpSession::GetClients()
{
    std::lock_guard<std::mutex> lock(mutex_);

	int clients = 0;
	for (auto iter : rtmp_clients_) {
		auto conn = iter.second.lock();
		if (conn != nullptr)  {
			clients += 1;
		}
	}

	for (auto iter : http_clients_) {
		auto conn = iter.second.lock();
		if (conn != nullptr) {
			clients += 1;
		}
	}

    return clients;
}

std::shared_ptr<RtmpConnection> RtmpSession::getPublisher()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return publisher_.lock();
}
