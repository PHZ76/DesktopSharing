#ifndef XOP_RTMP_CLIENT_H
#define XOP_RTMP_CLIENT_H

#include <string>
#include <mutex>
#include "RtmpConnection.h"
#include "net/EventLoop.h"
#include "net/Timestamp.h"

namespace xop
{

class RtmpClient : public Rtmp, public std::enable_shared_from_this<RtmpClient>
{
public:
	using FrameCallback = std::function<void(uint8_t* payload, uint32_t length, uint8_t codecId, uint32_t timestamp)>;

	static std::shared_ptr<RtmpClient> Create(xop::EventLoop* loop);
	~RtmpClient();

	void SetFrameCB(const FrameCallback& cb);
	int  OpenUrl(std::string url, int msec, std::string& status);
	void Close();
	bool IsConnected();

private:
	friend class RtmpConnection;

	RtmpClient(xop::EventLoop *event_loop);

	std::mutex mutex_;
	xop::EventLoop* event_loop_;
	xop::TaskScheduler* task_scheduler_;
	std::shared_ptr<RtmpConnection> rtmp_conn_;
	FrameCallback frame_cb_;
};

}

#endif