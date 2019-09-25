#include "DesktopSharing.h"
#include "AudioCapture.h"
#include "net/NetInterface.h"
#include "net/Timestamp.h"
#include "xop/RtspServer.h"
#include "xop/H264Parser.h"

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

bool DesktopSharing::init(AVConfig *config)
{
	std::lock_guard<std::mutex> locker(_mutex);

	if (_isInitialized)
		return false;

	_avconfig = *config;

	/* video config */
	_videoConfig.framerate = _avconfig.framerate;
	_videoConfig.bitrate = _avconfig.bitrate;
	_videoConfig.gop = _avconfig.gop;

	if (_screenCapture.init() < 0)
	{
		return false;
	}

	if (AudioCapture::instance().init() < 0)
	{
		return false;
	}

	/* audio config */
	_audioConfig.samplerate = AudioCapture::instance().getSamplerate();
	_audioConfig.channels = AudioCapture::instance().getChannels();

	_videoConfig.width = _screenCapture.getWidth();
	_videoConfig.height = _screenCapture.getHeight();

	if (_avconfig.codec == "h264_nvenc")
	{
		if (nvenc_info.is_supported())
		{
			_nvenc_data = nvenc_info.create();
		}

		if (_nvenc_data != nullptr)
		{
			encoder_config nvenc_config;
			nvenc_config.codec = "h264";
			nvenc_config.format = DXGI_FORMAT_B8G8R8A8_UNORM;
			nvenc_config.width = _videoConfig.width;
			nvenc_config.height = _videoConfig.height;
			nvenc_config.framerate = _videoConfig.framerate;
			nvenc_config.gop = _videoConfig.gop;
			if (!nvenc_info.init(_nvenc_data, &nvenc_config))
			{
				nvenc_info.destroy(&_nvenc_data);
				_nvenc_data = nullptr;
			}
		}
	}
	
	if(_nvenc_data == nullptr)
	{
		if (!H264Encoder::instance().init(_videoConfig))
		{
			return false;
		}
	}

	if (!AACEncoder::instance().init(_audioConfig))
	{
		return false;
	}

	std::thread t([this] {
		_eventLoop->loop();
	});
	t.detach();

	_isInitialized = true;
	return true;
}

void DesktopSharing::exit()
{
	if (_isRunning)
	{
		this->stop();
	}

	if (_isInitialized)
	{
		_isInitialized = false;
		_screenCapture.exit();
		H264Encoder::instance().exit();
		AudioCapture::instance().exit();
		AACEncoder::instance().exit();	

		if (_nvenc_data != nullptr)
		{
			nvenc_info.destroy(&_nvenc_data);
			_nvenc_data = nullptr;
		}
	}

	if (_rtspPusher!=nullptr && _rtspPusher->isConnected())
	{
		_rtspPusher->close();
		_rtspPusher = nullptr;
	}

	if (_rtmpPublisher != nullptr && _rtmpPublisher->isConnected())
	{
		_rtmpPublisher->close();
		_rtmpPublisher = nullptr;
	}

	if (_rtspServer != nullptr)
	{
		_eventLoop->quit();
		std::this_thread::sleep_for(std::chrono::seconds(1));
		_rtspServer->removeMeidaSession(_sessionId);
		_rtspServer = nullptr;
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

	if (AudioCapture::instance().start() == 0)
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
		return ;
	}

	_ip = "0.0.0.0"; //xop::NetInterface::getLocalIPAddress(); //"127.0.0.1";
	_rtspServer.reset(new xop::RtspServer(_eventLoop.get(), _ip, rtspPort));
	xop::MediaSession* session = xop::MediaSession::createNew(suffix);
	session->addMediaSource(xop::channel_0, xop::H264Source::createNew());
	session->addMediaSource(xop::channel_1, xop::AACSource::createNew(_audioConfig.samplerate, _audioConfig.channels, false));
	session->setNotifyCallback([this](xop::MediaSessionId sessionId, uint32_t numClients) {
		this->_clients = numClients;
	});

	_sessionId = _rtspServer->addMeidaSession(session);
	std::cout << "RTSP URL: " << "rtsp://" << xop::NetInterface::getLocalIPAddress() << ":" << std::to_string(rtspPort) << "/" << suffix << std::endl;
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

	_rtmpPublisher.reset(new xop::RtmpPublisher(_eventLoop.get()));

	xop::MediaInfo mediaInfo;
	uint8_t* extradata = AACEncoder::instance().getAVCodecContext()->extradata;
	uint8_t extradata_size = AACEncoder::instance().getAVCodecContext()->extradata_size;

	mediaInfo.audioSpecificConfigSize = extradata_size;
	mediaInfo.audioSpecificConfig.reset(new uint8_t[mediaInfo.audioSpecificConfigSize]);
	memcpy(mediaInfo.audioSpecificConfig.get(), extradata, extradata_size);

	uint8_t extradataBuf[1024];
	if (_nvenc_data != nullptr)
	{
		extradata_size = nvenc_info.get_sequence_params(_nvenc_data, extradataBuf, 1024);
		extradata = extradataBuf;
	}
	else
	{
		extradata = H264Encoder::instance().getAVCodecContext()->extradata;
		extradata_size = H264Encoder::instance().getAVCodecContext()->extradata_size;
	}

	xop::Nal sps = xop::H264Parser::findNal(extradata, extradata_size);
	if (sps.first != nullptr && sps.second != nullptr && *sps.first == 0x67)
	{
		mediaInfo.spsSize = sps.second - sps.first + 1;
		mediaInfo.sps.reset(new uint8_t[mediaInfo.spsSize]);
		memcpy(mediaInfo.sps.get(), sps.first, mediaInfo.spsSize);

		xop::Nal pps = xop::H264Parser::findNal(sps.second, extradata_size - (sps.second - extradata));
		if (pps.first != nullptr && pps.second != nullptr && *pps.first == 0x68)
		{
			mediaInfo.ppsSize = pps.second - pps.first + 1;
			mediaInfo.pps.reset(new uint8_t[mediaInfo.ppsSize]);
			memcpy(mediaInfo.pps.get(), pps.first, mediaInfo.ppsSize);
		}
	}

	_rtmpPublisher->setMediaInfo(mediaInfo);

	if (_rtmpPublisher->openUrl(url, 2000) < 0)
	{
		std::cout << "Open url " << url << " failed." << std::endl;
		return;
	}

	std::cout << "Push rtmp stream to " << url << " ..." << std::endl;
}

void DesktopSharing::pushVideo()
{
	static xop::Timestamp tp, tp2;
	
	uint32_t fps = 0;
	uint32_t msec = 1000 / _videoConfig.framerate;
	tp.reset();
	
	uint32_t bufferSize = 1920*1080*4;				    
	std::shared_ptr<uint8_t> buffer(new uint8_t[bufferSize]);

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
		uint32_t timestamp = xop::H264Source::getTimeStamp();
		
		{
			fps += 1;
			xop::AVFrame vidoeFrame;
			vidoeFrame.size = 0;

			if (_nvenc_data != nullptr)
			{
				ID3D11Device* device = nvenc_info.get_device(_nvenc_data);
				ID3D11Texture2D* texture = nvenc_info.get_texture(_nvenc_data);
				if (_screenCapture.captureFrame(device, texture) == 0)
				{
					int frameSize = nvenc_info.encode_texture(_nvenc_data, texture, buffer.get(), bufferSize);
					if (frameSize > 0)
					{
						vidoeFrame.buffer.reset(new uint8_t[frameSize]);
						vidoeFrame.type = xop::VIDEO_FRAME_P;
						vidoeFrame.timestamp = timestamp;						
						if (buffer.get()[4] == 0x67 || buffer.get()[4] == 0x65 || buffer.get()[4] == 0x6)
						{
							vidoeFrame.type = xop::VIDEO_FRAME_I;
						}

						memcpy(vidoeFrame.buffer.get(), buffer.get() + 4, frameSize - 4);
						vidoeFrame.size = frameSize - 4;
					}
				}
			}
			else 
			{
				if (_screenCapture.captureFrame(bgraData, bgraSize) == 0)
				{
					AVPacket* pkt = H264Encoder::instance().encodeVideo(bgraData.get(), _screenCapture.getWidth(), _screenCapture.getHeight());
					if (pkt)
					{
						vidoeFrame.buffer.reset(new uint8_t[pkt->size + 1024]);
						vidoeFrame.type = xop::VIDEO_FRAME_P;
						vidoeFrame.timestamp = timestamp;
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
					}
				}				
			}

			if(vidoeFrame.size > 0)
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
				if (_rtmpPublisher != nullptr && _rtmpPublisher->isConnected())
				{
					_rtmpPublisher->pushVideoFrame(vidoeFrame.buffer.get(), vidoeFrame.size);
				}
			}
		}
		
	}
}

void DesktopSharing::pushAudio()
{
	std::shared_ptr<uint8_t> pcmBuf(new uint8_t[48000 * 8]);
	uint32_t frameSamples = AACEncoder::instance().getFrameSamples();
	uint32_t channel = AudioCapture::instance().getChannels();
	uint32_t samplerate = AudioCapture::instance().getSamplerate();

	while (this->_isRunning)
	{
		uint32_t timestamp = xop::AACSource::getTimeStamp(samplerate);
	
		if (AudioCapture::instance().getSamples() >= (int)frameSamples)
		{
			if (AudioCapture::instance().readSamples(pcmBuf.get(), frameSamples) != frameSamples)
			{
				continue;
			}

			AVPacket* pkt = AACEncoder::instance().encodeAudio(pcmBuf.get(), frameSamples);
			if (pkt)
			{
				xop::AVFrame audioFrame(pkt->size);
				audioFrame.timestamp = timestamp;
				audioFrame.type = xop::AUDIO_FRAME;
				audioFrame.size = pkt->size;
				memcpy(audioFrame.buffer.get(), pkt->data, pkt->size);

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
					if (_rtmpPublisher != nullptr && _rtmpPublisher->isConnected())
					{
						_rtmpPublisher->pushAudioFrame(audioFrame.buffer.get(), audioFrame.size);
					}
				}
			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}