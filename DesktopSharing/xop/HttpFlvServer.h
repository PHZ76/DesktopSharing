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
	HttpFlvServer(xop::EventLoop* event_loop);
	~HttpFlvServer();

	void Attach(std::shared_ptr<RtmpServer> rtmp_server);

private:
	TcpConnection::Ptr OnConnect(SOCKET sockfd);

	std::mutex mutex_;
	std::weak_ptr<RtmpServer> rtmp_server_;
};

}


#endif
