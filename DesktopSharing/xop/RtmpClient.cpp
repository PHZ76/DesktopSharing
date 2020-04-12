#include "RtmpClient.h"
#include "net/Logger.h"

using namespace xop;

RtmpClient::RtmpClient(xop::EventLoop *loop)
	: m_eventLoop(loop)
{

}

RtmpClient::~RtmpClient()
{

}

std::shared_ptr<RtmpClient> RtmpClient::create(xop::EventLoop* loop)
{
	std::shared_ptr<RtmpClient> client(new RtmpClient(loop));
	return client;
}

void RtmpClient::setFrameCB(const FrameCallback& cb)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_frameCB = cb;
}

int RtmpClient::openUrl(std::string url, int msec, std::string& status)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	static xop::Timestamp tp;
	int timeout = msec;
	if (timeout <= 0)
	{
		timeout = 10000;
	}

	tp.reset();

	if (this->parseRtmpUrl(url) != 0)
	{
		LOG_INFO("[RtmpPublisher] rtmp url(%s) was illegal.\n", url.c_str());
		return -1;
	}

	//LOG_INFO("[RtmpPublisher] ip:%s, port:%hu, stream path:%s\n", m_ip.c_str(), m_port, m_streamPath.c_str());

	if (m_rtmpConn != nullptr)
	{
		std::shared_ptr<RtmpConnection> rtmpConn = m_rtmpConn;
		SOCKET sockfd = rtmpConn->GetSocket();
		m_taskScheduler->AddTriggerEvent([sockfd, rtmpConn]() {
			rtmpConn->Disconnect();
		});
		m_rtmpConn = nullptr;
	}

	TcpSocket tcpSocket;
	tcpSocket.Create();
	if (!tcpSocket.Connect(m_ip, m_port, timeout))
	{
		tcpSocket.Close();
		return -1;
	}

	m_taskScheduler = m_eventLoop->GetTaskScheduler().get();
	m_rtmpConn.reset(new RtmpConnection(shared_from_this(), m_taskScheduler, tcpSocket.GetSocket()));
	m_taskScheduler->AddTriggerEvent([this]() {
		if (m_frameCB)
		{
			m_rtmpConn->setPlayCB(m_frameCB);
		}
		m_rtmpConn->handshake();
	});

	timeout -= (int)tp.elapsed();
	if (timeout < 0)
	{
		timeout = 1000;
	}

	do
	{
		xop::Timer::Sleep(100);
		timeout -= 100;
	} while (!m_rtmpConn->IsClosed() && !m_rtmpConn->isPlaying() && timeout > 0);

	status = m_rtmpConn->getStatus();
	if (!m_rtmpConn->isPlaying())
	{
		std::shared_ptr<RtmpConnection> rtmpConn = m_rtmpConn;
		SOCKET sockfd = rtmpConn->GetSocket();
		m_taskScheduler->AddTriggerEvent([sockfd, rtmpConn]() {
			rtmpConn->Disconnect();
		});
		m_rtmpConn = nullptr;
		return -1;
	}

	return 0;
}

void RtmpClient::close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_rtmpConn != nullptr)
	{
		std::shared_ptr<RtmpConnection> rtmpConn = m_rtmpConn;
		SOCKET sockfd = rtmpConn->GetSocket();
		m_taskScheduler->AddTriggerEvent([sockfd, rtmpConn]() {
			rtmpConn->Disconnect();
		});
	}
}

bool RtmpClient::isConnected()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_rtmpConn != nullptr)
	{
		return (!m_rtmpConn->IsClosed());
	}

	return false;
}

