#ifndef XOP_HTTP_FLV_SERVER_H
#define XOP_HTTP_FLV_SERVER_H

#include "net/TcpServer.h"
#include "HttpFlvConnection.h"
#include <mutex>

namespace xop
{
class RtmpServer;

class HttpFlvServer : public TcpServer
{
public:
	HttpFlvServer(xop::EventLoop *loop, std::string ip, uint16_t port = 8000);
	~HttpFlvServer();

	void attach(RtmpServer *rtmpServer);

private:
	TcpConnection::Ptr newConnection(SOCKET sockfd);

	std::mutex m_mutex;
	RtmpServer *m_rtmpServer = nullptr;
	xop::EventLoop *m_eventLoop = nullptr;
};

}


#endif
