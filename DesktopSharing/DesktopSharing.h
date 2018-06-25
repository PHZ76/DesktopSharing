#ifndef DESKTOP_SHARING_H
#define DESKTOP_SHARING_H

#include <mutex>
#include "rtsp/RtspServer.h"
#include "rtsp/RtspPusher.h"
#include "xop/Timer.h"
#include "H264Encoder.h"
#include "AACEncoder.h"
#include "RtmpPusher.h"

class DesktopSharing
{
public:
	DesktopSharing & operator=(const DesktopSharing &) = delete;
	DesktopSharing(const DesktopSharing &) = delete;
	static DesktopSharing& Instance();
	~DesktopSharing();

	bool Init(std::string suffix="live", uint16_t rtspPort=554);
	void Exit();

	void Start();
	void Stop();

    // 推流测试接口
	void StartRtspPusher(const char* url);
	void StartRtmpPusher(const char* url);

private:
	DesktopSharing();
	void PushAudio();
	void PushVideo();

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

