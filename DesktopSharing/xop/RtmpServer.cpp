#include "RtmpServer.h"
#include "RtmpConnection.h"
#include "net/SocketUtil.h"
#include "net/Logger.h"

using namespace xop;

RtmpServer::RtmpServer(EventLoop* event_loop)
	: TcpServer(event_loop)
	, event_loop_(event_loop)
{
	event_loop_->AddTimer([this] {
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto iter = rtmp_sessions_.begin(); iter != rtmp_sessions_.end(); ) {
			if (iter->second->GetClients() == 0) {
				rtmp_sessions_.erase(iter++);
			}
			else {
				iter++;
			}
		}
		return true;
	}, 30000);
}

RtmpServer::~RtmpServer()
{
    
}

std::shared_ptr<RtmpServer> RtmpServer::Create(xop::EventLoop* event_loop)
{
	std::shared_ptr<RtmpServer> server(new RtmpServer(event_loop));
	return server;
}

TcpConnection::Ptr RtmpServer::OnConnect(SOCKET sockfd)
{
    return std::make_shared<RtmpConnection>(shared_from_this(), event_loop_->GetTaskScheduler().get(), sockfd);
}

void RtmpServer::AddSession(std::string stream_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(rtmp_sessions_.find(stream_path) == rtmp_sessions_.end()) {
        rtmp_sessions_[stream_path] = std::make_shared<RtmpSession>();
    }
}

void RtmpServer::RemoveSession(std::string stream_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rtmp_sessions_.erase(stream_path);
}

bool RtmpServer::HasSession(std::string stream_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return (rtmp_sessions_.find(stream_path) != rtmp_sessions_.end());
}

RtmpSession::Ptr RtmpServer::GetSession(std::string stream_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(rtmp_sessions_.find(stream_path) == rtmp_sessions_.end()) {
        rtmp_sessions_[stream_path] = std::make_shared<RtmpSession>();        
    }
    
    return rtmp_sessions_[stream_path];
}

bool RtmpServer::HasPublisher(std::string stream_path)
{
    auto session = GetSession(stream_path);
    if(session == nullptr) {
       return false;
    }
    
    return (session->GetPublisher()!=nullptr);
}


