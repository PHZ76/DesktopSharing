#ifndef DESKTOP_SHARING_H
#define DESKTOP_SHARING_H

#include <mutex>
#include "xop/RtspServer.h"
#include "xop/RtspPusher.h"
#include "net/Timer.h"
#include "H264Encoder.h"
#include "AACEncoder.h"
#include "RtmpPusher.h"

class DesktopSharing
{
public:
	DesktopSharing & operator=(const DesktopSharing &) = delete;
	DesktopSharing(const DesktopSharing &) = delete;
	static DesktopSharing& instance();
	~DesktopSharing();

	bool init(std::string suffix="live", uint16_t rtspPort=554);
	void exit();

	void start();
	void stop();

    // 推流测试接口
	void startRtspPusher(const char* url);
	void startRtmpPusher(const char* url);

private:
	DesktopSharing();
	void pushAudio();
	void pushVideo();

	std::string _ip;
	std::string _rtspSuffix;

	bool _isInitialized = false;
	bool _isRunning = false;
	uint32_t _clients = 0;
	xop::MediaSessionId _sessionId = 0;
	std::shared_ptr<xop::EventLoop> _eventLoop = nullptr;
	std::shared_ptr<xop::RtspServer> _rtspServer = nullptr;	
	std::shared_ptr<xop::RtspPusher> _rtspPusher = nullptr;
	std::shared_ptr<RtmpPusher> _rtmpPusher = nullptr;
	AudioConfig _audioConfig;
	VideoConfig _videoConfig;

	std::mutex _mutex;
	std::shared_ptr<std::thread> _videoThread;
	std::shared_ptr<std::thread> _audioThread;
};

#endif

