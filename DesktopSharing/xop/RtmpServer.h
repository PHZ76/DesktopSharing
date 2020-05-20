#ifndef XOP_RTMP_SERVER_H
#define XOP_RTMP_SERVER_H

#include <string>
#include <mutex>
#include "rtmp.h"
#include "RtmpSession.h"
#include "net/TcpServer.h"

namespace xop
{

class RtmpServer : public TcpServer, public Rtmp, public std::enable_shared_from_this<RtmpServer>
{
public:
	static std::shared_ptr<RtmpServer> Create(xop::EventLoop* event_loop);
    ~RtmpServer();
       
private:
	friend class RtmpConnection;
	friend class HttpFlvConnection;

	RtmpServer(xop::EventLoop *event_loop);
	void AddSession(std::string stream_path);
	void RemoveSession(std::string stream_path);

	RtmpSession::Ptr GetSession(std::string stream_path);
	bool HasSession(std::string stream_path);
	bool HasPublisher(std::string stream_path);

    virtual TcpConnection::Ptr OnConnect(SOCKET sockfd);
    
	xop::EventLoop *event_loop_;
    std::mutex mutex_;
    std::unordered_map<std::string, RtmpSession::Ptr> rtmp_sessions_; 
}; 
    
}

#endif