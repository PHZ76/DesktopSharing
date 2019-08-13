#include "DesktopSharing.h"
#include "AudioCapture.h"
#include "net/NetInterface.h"
#include "net/Timestamp.h"
#include "xop/RtspServer.h"

DesktopSharing::DesktopSharing()
	: _eventLoop(new xop::EventLoop)
{
	
}

DesktopSharing::~DesktopSharing()
{

}

DesktopSharing& DesktopSharing::instance()
{
	static DesktopSharing s_ds;
	return s_ds;
}

bool DesktopSharing::init()
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (_isInitialized)
		return false;

	/* video config */
	_videoConfig.framerate = 25;
	_videoConfig.bitrate = 4000000;
	_videoConfig.gop = 25;

	/* audio config */
	_audioConfig.samplerate = 44100;
	_audioConfig.channels = 2;

	if (_screenCapture.init() < 0)
	{
		return false;
	}

	if (!AudioCapture::instance().init(_audioConfig.samplerate, _audioConfig.channels))
	{
		return false;
	}

	_videoConfig.width = _screenCapture.getWidth();
	_videoConfig.height = _screenCapture.getHeight();

	if (!H264Encoder::instance().init(_videoConfig))
	{
		return false;
	}

	if (!AACEncoder::instance().init(_audioConfig))
	{
		return false;
	}

	_isInitialized = true;
	return true;
}

void DesktopSharing::exit()
{
	if (_isRunning)
	{
		this->stop();
	}

	std::lock_guard<std::mutex> locker(_mutex);

	if (_isInitialized)
	{
		_isInitialized = false;
		_screenCapture.exit();
		H264Encoder::instance().exit();
		AudioCapture::instance().exit();
		AACEncoder::instance().exit();		
	}

	if (_rtspPusher!=nullptr && _rtspPusher->isConnected())
	{
		_rtspPusher->close();
	}

	if (_rtmpPusher != nullptr && _rtmpPusher->isConnected())
	{
		_rtmpPusher->close();
	}

	if (_rtspServer != nullptr)
	{
		_eventLoop->quit();
		std::this_thread::sleep_for(std::chrono::seconds(1));
		_rtspServer->removeMeidaSession(_sessionId);
		_rtspServer.reset();
	}
}

void DesktopSharing::start()
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (!_isInitialized || _isRunning)
		return;
	
	_isRunning = true;

	if (_screenCapture.start() == 0)
	{
		if (_screenCapture.isCapturing())
		{
			_videoThread.reset(new std::thread(&DesktopSharing::pushVideo, this));
		}
	}

	if (AudioCapture::instance().start())
	{
		if (AudioCapture::instance().isCapturing())
		{
			_audioThread.reset(new std::thread(&DesktopSharing::pushAudio, this));
		}
	}
}

void DesktopSharing::stop()
{
	std::lock_guard<std::mutex> locker(_mutex);
	if (_isRunning)
	{
		_isRunning = false;
		if (_screenCapture.isCapturing())
		{
			_screenCapture.stop();
			_videoThread->join();
		}

		if (AudioCapture::instance().isCapturing())
		{
			AudioCapture::instance().stop();
			_audioThread->join();
		}	
	}
}

void DesktopSharing::startRtspServer(std::string suffix, uint16_t rtspPort)
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (!_isInitialized)
	{
		return;
	}

	if (_rtspServer != nullptr)
	{
		return;
	}

	_ip = xop::NetInterface::getLocalIPAddress();
	_rtspServer.reset(new xop::RtspServer(_eventLoop.get(), _ip, rtspPort));
	xop::MediaSession* session = xop::MediaSession::createNew(suffix);
	session->addMediaSource(xop::channel_0, xop::H264Source::createNew());
	session->addMediaSource(xop::channel_1, xop::AACSource::createNew(_audioConfig.samplerate, _audioConfig.channels, false));
	session->setNotifyCallback([this](xop::MediaSessionId sessionId, uint32_t numClients) {
		this->_clients = numClients;
	});

	_sessionId = _rtspServer->addMeidaSession(session);
	std::thread t([this] {
		_eventLoop->loop();
	});
	t.detach();

	std::cout << "RTSP URL: " << "rtsp://" << _ip << ":" << std::to_string(rtspPort) << "/" << suffix << std::endl;
}

void DesktopSharing::startRtspPusher(const char* url)
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (!_isInitialized)
	{
		return;
	}

	if (_rtspPusher && _rtspPusher->isConnected())
	{
		_rtspPusher->close();
	}

	_rtspPusher.reset(new xop::RtspPusher(_eventLoop.get()));
	xop::MediaSession *session = xop::MediaSession::createNew();
	session->addMediaSource(xop::channel_0, xop::H264Source::createNew());
	session->addMediaSource(xop::channel_1, xop::AACSource::createNew(_audioConfig.samplerate, _audioConfig.channels, false));

	if (_rtspPusher->addMeidaSession(session) > 0)
	{
		if (_rtspPusher->openUrl(url) != 0)
		{
			_rtspPusher = nullptr;
			std::cout << "Open " << url << " failed." << std::endl;
			return;
		}		
	}

	std::cout << "Push rtsp stream to " << url << " ..." << std::endl;
}

void DesktopSharing::startRtmpPusher(const char* url)
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (!_isInitialized)
	{
		return;
	}

	_rtmpPusher.reset(new RtmpPusher);
	if (!_rtmpPusher->openUrl(url))
	{
		_rtspPusher = nullptr;
		std::cout << "Open " << url << " failed." << std::endl;
		return;
	}

	_rtmpPusher->addStream(H264Encoder::instance().getAVCodecContext()); // channel_0
	_rtmpPusher->addStream(AACEncoder::instance().getAVCodecContext()); // channel_1

	std::cout << "Push rtmp stream to " << url << " ..." << std::endl;
}

void DesktopSharing::pushVideo()
{
	static xop::Timestamp tp, tp2;
	
	uint32_t fps = 0;
	uint32_t msec = 1000 / _videoConfig.framerate;
	tp.reset();

	while (this->_isRunning)
	{
		if (tp2.elapsed() >= 1000)
		{
			//printf("video fps: %d\n", fps); /*编码帧率统计*/
			tp2.reset();
			fps = 0;
		}

		uint32_t delay = msec;
		uint32_t elapsed = (uint32_t)tp.elapsed(); /*编码耗时计算*/
		if (elapsed > delay)
		{
			delay = 0;
		}
		else
		{
			delay -= elapsed;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		tp.reset();

		std::shared_ptr<uint8_t> bgraData;
		uint32_t bgraSize = 0;

		if (_screenCapture.captureFrame(bgraData, bgraSize) == 0)
		{
			fps += 1;

			AVPacket* pkt = H264Encoder::instance().encodeVideo(bgraData.get(), _screenCapture.getWidth(), _screenCapture.getHeight());
			if (pkt)
			{
				xop::AVFrame vidoeFrame(pkt->size + 1024);
				vidoeFrame.size = 0;
				vidoeFrame.type = xop::VIDEO_FRAME_P;
				vidoeFrame.timestamp = xop::H264Source::getTimeStamp();
				if (pkt->data[4] == 0x65 || pkt->data[4] == 0x6) //0x67:sps ,0x65:IDR, 0x6: SEI
				{
					// 编码器使用了AV_CODEC_FLAG_GLOBAL_HEADER, 这里需要添加sps, pps
					uint8_t* extraData = H264Encoder::instance().getAVCodecContext()->extradata;
					uint8_t extraDatasize = H264Encoder::instance().getAVCodecContext()->extradata_size;
					memcpy(vidoeFrame.buffer.get() + vidoeFrame.size, extraData + 4, extraDatasize - 4); // +4去掉H.264起始码
					vidoeFrame.size += (extraDatasize - 4);
					vidoeFrame.type = xop::VIDEO_FRAME_I;

					memcpy(vidoeFrame.buffer.get() + vidoeFrame.size, pkt->data, pkt->size);
					vidoeFrame.size += pkt->size;
				}
				else
				{
					memcpy(vidoeFrame.buffer.get() + vidoeFrame.size, pkt->data + 4, pkt->size - 4); // +4去掉H.264起始码
					vidoeFrame.size += (pkt->size - 4);
				}

				if (_mutex.try_lock())
				{
					// 本地RTSP视频转发
					if (_rtspServer != nullptr && this->_clients > 0)
					{
						_rtspServer->pushFrame(_sessionId, xop::channel_0, vidoeFrame);
					}

					// RTSP视频推流
					if (_rtspPusher != nullptr && _rtspPusher->isConnected())
					{
						_rtspPusher->pushFrame(_sessionId, xop::channel_0, vidoeFrame);
					}

					// RTMP视频推流
					if (_rtmpPusher != nullptr && _rtmpPusher->isConnected())
					{
						_rtmpPusher->pushFrame(xop::channel_0, pkt);
					}

					_mutex.unlock();
				}
			}
		}
	}
}

void DesktopSharing::pushAudio()
{
	while (this->_isRunning)
	{
		PCMFrame frame(0);
		if (AudioCapture::instance().getFrame(frame))
		{
			AVPacket* pkt = AACEncoder::instance().encodeAudio(frame.data.get());
			if (pkt)
			{
				xop::AVFrame audioFrame(pkt->size);
				audioFrame.timestamp = xop::AACSource::getTimeStamp();
				audioFrame.type = xop::AUDIO_FRAME;
				memcpy(audioFrame.buffer.get(), pkt->data, pkt->size);

				if (_mutex.try_lock())
				{
					// RTSP音频转发
					if (_rtspServer != nullptr && this->_clients > 0)
					{
						_rtspServer->pushFrame(_sessionId, xop::channel_1, audioFrame);
					}

					// RTSP音频推流
					if (_rtspPusher && _rtspPusher->isConnected())
					{
						_rtspPusher->pushFrame(_sessionId, xop::channel_1, audioFrame);
					}

					// RTMP音频推流
					if (_rtmpPusher && _rtmpPusher->isConnected())
					{
						_rtmpPusher->pushFrame(xop::channel_1, pkt);
					}
					_mutex.unlock();
				}

			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}