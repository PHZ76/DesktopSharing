#include "DesktopSharing.h"
#include "AudioCapture.h"
#include "VideoCapture.h"
#include "net/NetInterface.h"
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

bool DesktopSharing::init(std::string suffix, uint16_t rtspPort)
{
	std::lock_guard<std::mutex> locker(_mutex);
	if (_isInitialized)
		return false;

	_ip = xop::NetInterface::getLocalIPAddress();
	_rtspSuffix = suffix;
	_rtspServer.reset(new xop::RtspServer(_eventLoop.get(), _ip, rtspPort));
	xop::MediaSession* session = xop::MediaSession::createNew(_rtspSuffix);

	VideoCapture& videoCpature = VideoCapture::instance();
	_videoConfig.framerate = 25;
	_videoConfig.bitrate = 2000000;
	_videoConfig.gop = 10;
	if (videoCpature.init(_videoConfig.framerate))
	{		
		_videoConfig.width = videoCpature.width();
		_videoConfig.height = videoCpature.height();
		if (H264Encoder::instance().init(_videoConfig))
		{
			session->addMediaSource(xop::channel_0, xop::H264Source::createNew());
		}
		else
		{
			videoCpature.exit();
		}
	}

	AudioCapture& audioCapture = AudioCapture::instance();
	_audioConfig.samplerate = 44100;
	_audioConfig.channels = 2;
	if (audioCapture.init(_audioConfig.samplerate, _audioConfig.channels))
	{
		if (AACEncoder::instance().init(_audioConfig))
		{
			session->addMediaSource(xop::channel_1, xop::AACSource::createNew(_audioConfig.samplerate, _audioConfig.channels, false));
		}
		else
		{
			audioCapture.exit();
		}
	}

	session->setNotifyCallback([this](xop::MediaSessionId sessionId, uint32_t numClients)
	{
		this->_clients = numClients; 
	});

	 _sessionId = _rtspServer->addMeidaSession(session);
	_isInitialized = true;
	return true;
}

void DesktopSharing::exit()
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (_isInitialized)
	{
		if (_isRunning)
			stop();

		_isInitialized = false;
		VideoCapture::instance().exit();
		H264Encoder::instance().exit();
		AudioCapture::instance().exit();
		AACEncoder::instance().exit();		
	}

	if (_rtspPusher && _rtspPusher->isConnected())
	{
		_rtspPusher->close();
	}

	if (_rtmpPusher && _rtmpPusher->isConnected())
	{
		_rtmpPusher->close();
	}
}

void DesktopSharing::start()
{
	if (!_isInitialized || _isRunning)
		return;

	if (VideoCapture::instance().start())
	{
		if (VideoCapture::instance().isCapturing())
		{
			_videoThread.reset(new std::thread(&DesktopSharing::pushVideo, this));
			_isRunning = true;
		}
	}

	if (AudioCapture::instance().start())
	{
		if (AudioCapture::instance().isCapturing())
		{
			_audioThread.reset(new std::thread(&DesktopSharing::pushAudio, this));
			_isRunning = true;
		}
	}
	
	if (_isRunning)
	{
		std::cout << "rtsp://" << _ip << "/" << _rtspSuffix <<std::endl;
		_eventLoop->loop();
	}
}

void DesktopSharing::startRtspPusher(const char* url)
{
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

void DesktopSharing::stop()
{
	if (VideoCapture::instance().isCapturing() && _isRunning)
	{
		_isRunning = false;
		_videoThread->join();
	}

	if (AudioCapture::instance().isCapturing())
	{
		_isRunning = false;
		_audioThread->join();
	}

	_eventLoop->quit();
}

void DesktopSharing::pushVideo()
{
	xop::Timer timer;
	timer.setEventCallback([&timer, this] {
		while (this->_isRunning)
		{
			//if (this->_clients > 0)
			{
				RGBAFrame frame(0);
				if (VideoCapture::instance().getFrame(frame))
				{
					AVPacket* pkt = H264Encoder::instance().encodeVideo(frame.data.get(), frame.width, frame.height);
					if (pkt)
					{
						xop::AVFrame vidoeFrame(pkt->size+1024);
						vidoeFrame.size = 0;
						vidoeFrame.type = xop::VIDEO_FRAME_P;
						vidoeFrame.timestamp = xop::H264Source::getTimeStamp();
						if (pkt->data[4] == 0x65) //0x67:sps ,0x65:IDR
						{
							// 编码器使用了AV_CODEC_FLAG_GLOBAL_HEADER, 这里需要添加sps, pps
							uint8_t* extraData = H264Encoder::instance().getAVCodecContext()->extradata;
							uint8_t extraDatasize = H264Encoder::instance().getAVCodecContext()->extradata_size;
							memcpy(vidoeFrame.buffer.get()+ vidoeFrame.size, extraData+4, extraDatasize-4); // +4去掉H.264起始码
							vidoeFrame.size += (extraDatasize - 4);
							vidoeFrame.type = xop::VIDEO_FRAME_I;

							memcpy(vidoeFrame.buffer.get() + vidoeFrame.size, pkt->data, pkt->size); 
							vidoeFrame.size += pkt->size;
						}
						else
						{
							memcpy(vidoeFrame.buffer.get() + vidoeFrame.size, pkt->data+4, pkt->size-4); // +4去掉H.264起始码
							vidoeFrame.size += (pkt->size-4);
						}

						// 本地RTSP视频转发
						if (this->_clients > 0)
						{
							_rtspServer->pushFrame(_sessionId, xop::channel_0, vidoeFrame);
						}

						// RTSP视频推流
						if (_rtspPusher && _rtspPusher->isConnected())
						{
							_rtspPusher->pushFrame(_sessionId, xop::channel_0, vidoeFrame);
						}
						
						// RTMP视频推流
						if (_rtmpPusher && _rtmpPusher->isConnected())
						{						
							_rtmpPusher->pushFrame(xop::channel_0, pkt);
						}
					}					
				}
			}
		}
		return false;
		timer.stop();
	});

	timer.start(1000*1000/_videoConfig.framerate, true);	
	VideoCapture::instance().stop();
}

void DesktopSharing::pushAudio()
{
	xop::Timer timer;
	timer.setEventCallback([&timer, this] {
		while (this->_isRunning)
		{
			//if (this->_clients > 0)
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

						// RTSP音频转发
						if (this->_clients > 0)
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
					}
				}
			}
		}
		timer.stop();
		return false;
	});

	timer.start(10000, true);	
	AudioCapture::instance().stop();
}