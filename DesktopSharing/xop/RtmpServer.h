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
	static std::shared_ptr<RtmpServer> create(xop::EventLoop* loop);
    ~RtmpServer();
       
private:
	friend class RtmpConnection;
	friend class HttpFlvConnection;

	RtmpServer(xop::EventLoop *loop);
	void addSession(std::string streamPath);
	void removeSession(std::string streamPath);

	RtmpSession::Ptr getSession(std::string streamPath);
	bool hasSession(std::string streamPath);
	bool hasPublisher(std::string streamPath);

    virtual TcpConnection::Ptr OnConnect(SOCKET sockfd);
    
	xop::EventLoop *m_eventLoop = nullptr;
    std::mutex m_mutex;
    std::unordered_map<std::string, RtmpSession::Ptr> m_rtmpSessions; 
}; 
    
}

#endif