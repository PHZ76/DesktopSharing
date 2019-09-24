#ifndef DESKTOP_SHARING_H
#define DESKTOP_SHARING_H

#include <mutex>
#include "xop/RtspServer.h"
#include "xop/RtspPusher.h"
#include "xop/RtmpPublisher.h"
#include "net/Timer.h"
#include "H264Encoder.h"
#include "AACEncoder.h"
#include "nvenc.h"
#include "ScreenCapture/DXGIScreenCapture.h"

struct AVConfig
{
	uint32_t bitrate = 2000000;
	uint32_t framerate = 25;
	uint32_t gop = 25;

	std::string codec = "h264"; // [software codec: "h264"]  [hardware codec: "h264_nvenc"]
};

class DesktopSharing
{
public:
	DesktopSharing & operator=(const DesktopSharing &) = delete;
	DesktopSharing(const DesktopSharing &) = delete;
	static DesktopSharing& instance();
	~DesktopSharing();

	bool init(AVConfig *config);
	void exit();

	void start();
	void stop();

	void startRtspServer(std::string suffix = "live", uint16_t rtspPort = 554);
	void startRtspPusher(const char* url);
	void startRtmpPusher(const char* url);

private:
	DesktopSharing();
	void pushAudio();
	void pushVideo();

	std::string _ip;

	bool _isInitialized = false;
	bool _isRunning = false;
	uint32_t _clients = 0;
	xop::MediaSessionId _sessionId = 0;
	std::shared_ptr<xop::EventLoop> _eventLoop = nullptr;
	std::shared_ptr<xop::RtspServer> _rtspServer = nullptr;	
	std::shared_ptr<xop::RtspPusher> _rtspPusher = nullptr;
	std::shared_ptr<xop::RtmpPublisher> _rtmpPublisher = nullptr;
	AVConfig _avconfig;
	AudioConfig _audioConfig;
	VideoConfig _videoConfig;
	void *_nvenc_data = nullptr;
	std::mutex _mutex;
	std::shared_ptr<std::thread> _videoThread;
	std::shared_ptr<std::thread> _audioThread;

	DXGIScreenCapture _screenCapture;
};

#endif

