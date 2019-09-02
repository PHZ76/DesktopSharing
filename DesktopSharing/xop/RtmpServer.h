#ifndef XOP_RTMP_SERVER_H
#define XOP_RTMP_SERVER_H

#include <string>
#include <mutex>
#include "rtmp.h"
#include "RtmpSession.h"
#include "net/TcpServer.h"

namespace xop
{

class RtmpServer : public TcpServer, public Rtmp
{
public:
    RtmpServer(xop::EventLoop *loop, std::string ip, uint16_t port = 1935);
    ~RtmpServer();
       
private:
	friend class RtmpConnection;
	friend class HttpFlvConnection;

	void addSession(std::string streamPath);
	void removeSession(std::string streamPath);

	RtmpSession::Ptr getSession(std::string streamPath);
	bool hasSession(std::string streamPath);
	bool hasPublisher(std::string streamPath);

    virtual TcpConnection::Ptr newConnection(SOCKET sockfd);
    
	xop::EventLoop *m_eventLoop = nullptr;
    std::mutex m_mutex;
    std::unordered_map<std::string, RtmpSession::Ptr> m_rtmpSessions; 
}; 
    
}

#endif