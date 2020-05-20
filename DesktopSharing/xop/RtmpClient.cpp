#include "RtmpClient.h"
#include "net/Logger.h"

using namespace xop;

RtmpClient::RtmpClient(xop::EventLoop *event_loop)
	: event_loop_(event_loop)
{

}

RtmpClient::~RtmpClient()
{

}

std::shared_ptr<RtmpClient> RtmpClient::Create(xop::EventLoop* event_loop)
{
	std::shared_ptr<RtmpClient> client(new RtmpClient(event_loop));
	return client;
}

void RtmpClient::SetFrameCB(const FrameCallback& cb)
{
	std::lock_guard<std::mutex> lock(mutex_);
	frame_cb_ = cb;
}

int RtmpClient::OpenUrl(std::string url, int msec, std::string& status)
{
	std::lock_guard<std::mutex> lock(mutex_);

	static xop::Timestamp tp;
	int timeout = msec;
	if (timeout <= 0) {
		timeout = 5000;
	}

	tp.reset();

	if (this->ParseRtmpUrl(url) != 0) {
		LOG_INFO("[RtmpPublisher] rtmp url(%s) was illegal.\n", url.c_str());
		return -1;
	}

	//LOG_INFO("[RtmpPublisher] ip:%s, port:%hu, stream path:%s\n", ip_.c_str(), port_, stream_path_.c_str());

	if (rtmp_conn_ != nullptr) {
		std::shared_ptr<RtmpConnection> rtmp_conn = rtmp_conn_;
		SOCKET sockfd = rtmp_conn->GetSocket();
		task_scheduler_->AddTriggerEvent([sockfd, rtmp_conn]() {
			rtmp_conn->Disconnect();
		});
		rtmp_conn_ = nullptr;
	}

	TcpSocket tcp_socket;
	tcp_socket.Create();
	if (!tcp_socket.Connect(ip_, port_, timeout)) {
		tcp_socket.Close();
		return -1;
	}

	task_scheduler_ = event_loop_->GetTaskScheduler().get();
	rtmp_conn_.reset(new RtmpConnection(shared_from_this(), task_scheduler_, tcp_socket.GetSocket()));
	task_scheduler_->AddTriggerEvent([this]() {
		if (frame_cb_) {
			rtmp_conn_->setPlayCB(frame_cb_);
		}
		rtmp_conn_->Handshake();
	});

	timeout -= (int)tp.elapsed();
	if (timeout < 0) {
		timeout = 1000;
	}

	do
	{
		xop::Timer::Sleep(100);
		timeout -= 100;
	} while (!rtmp_conn_->IsClosed() && !rtmp_conn_->IsPlaying() && timeout > 0);

	status = rtmp_conn_->GetStatus();
	if (!rtmp_conn_->IsPlaying()) {
		std::shared_ptr<RtmpConnection> rtmp_conn = rtmp_conn_;
		SOCKET sockfd = rtmp_conn->GetSocket();
		task_scheduler_->AddTriggerEvent([sockfd, rtmp_conn]() {
			rtmp_conn->Disconnect();
		});
		rtmp_conn_ = nullptr;
		return -1;
	}

	return 0;
}

void RtmpClient::Close()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (rtmp_conn_ != nullptr) {
		std::shared_ptr<RtmpConnection> rtmp_conn = rtmp_conn_;
		SOCKET sockfd = rtmp_conn->GetSocket();
		task_scheduler_->AddTriggerEvent([sockfd, rtmp_conn]() {
			rtmp_conn->Disconnect();
		});
	}
}

bool RtmpClient::IsConnected()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (rtmp_conn_ != nullptr) {
		return (!rtmp_conn_->IsClosed());
	}
	return false;
}

