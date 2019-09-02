#include "HttpFlvServer.h"
#include "RtmpServer.h"
#include "net/SocketUtil.h"
#include "net/Logger.h"

using namespace xop;

HttpFlvServer::HttpFlvServer(xop::EventLoop *loop, std::string ip, uint16_t port)
	: TcpServer(loop, ip, port)
{
	if (this->start() != 0)
	{
		LOG_INFO("HTTP-FLV Server listening on %u failed.", port);
	}
	else
	{
		LOG_INFO("HTTP-FLV Server listen on %u.\n", port);
	}
}

HttpFlvServer::~HttpFlvServer()
{

}

void HttpFlvServer::attach(RtmpServer *rtmpServer)
{
	std::lock_guard<std::mutex> locker(m_mutex);
	m_rtmpServer = rtmpServer;
}

TcpConnection::Ptr HttpFlvServer::newConnection(SOCKET sockfd)
{
	return std::make_shared<HttpFlvConnection>(m_rtmpServer, _eventLoop->getTaskScheduler().get(), sockfd);
}

