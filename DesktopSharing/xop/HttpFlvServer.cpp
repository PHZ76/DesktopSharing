#include "HttpFlvServer.h"
#include "RtmpServer.h"
#include "net/SocketUtil.h"
#include "net/Logger.h"

using namespace xop;

HttpFlvServer::HttpFlvServer(xop::EventLoop *loop)
	: TcpServer(loop)
{

}

HttpFlvServer::~HttpFlvServer()
{

}

void HttpFlvServer::attach(RtmpServer *rtmpServer)
{
	std::lock_guard<std::mutex> locker(m_mutex);
	m_rtmpServer = rtmpServer;
}

TcpConnection::Ptr HttpFlvServer::OnConnect(SOCKET sockfd)
{
	return std::make_shared<HttpFlvConnection>(m_rtmpServer, event_loop_->GetTaskScheduler().get(), sockfd);
}

