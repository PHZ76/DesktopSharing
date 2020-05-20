#include "HttpFlvServer.h"
#include "RtmpServer.h"
#include "net/SocketUtil.h"
#include "net/Logger.h"

using namespace xop;

HttpFlvServer::HttpFlvServer(xop::EventLoop* event_loop)
	: TcpServer(event_loop)
{

}

HttpFlvServer::~HttpFlvServer()
{

}

void HttpFlvServer::Attach(std::shared_ptr<RtmpServer> rtmp_server)
{
	std::lock_guard<std::mutex> locker(mutex_);
	rtmp_server_ = rtmp_server;
}

TcpConnection::Ptr HttpFlvServer::OnConnect(SOCKET sockfd)
{
	auto rtmp_server = rtmp_server_.lock();
	if (rtmp_server) {
		return std::make_shared<HttpFlvConnection>(rtmp_server, event_loop_->GetTaskScheduler().get(), sockfd);
	}
	return nullptr;
}

