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

	static std::shared_ptr<RtmpClient> create(xop::EventLoop* loop);
	~RtmpClient();

	void setFrameCB(const FrameCallback& cb);
	int openUrl(std::string url, int msec, std::string& status);
	void close();
	bool isConnected();

private:
	friend class RtmpConnection;

	RtmpClient(xop::EventLoop *loop);

	xop::EventLoop *m_eventLoop = nullptr;
	TaskScheduler *m_taskScheduler = nullptr;
	std::mutex m_mutex;
	std::shared_ptr<RtmpConnection> m_rtmpConn;
	FrameCallback m_frameCB;
};

}

#endif