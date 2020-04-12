#include "RtmpPublisher.h"
#include "net/Logger.h"
#include "net/log.h"

using namespace xop;

RtmpPublisher::RtmpPublisher(xop::EventLoop *loop)
	: m_eventLoop(loop)
{

}

RtmpPublisher::~RtmpPublisher()
{

}

std::shared_ptr<RtmpPublisher> RtmpPublisher::create(xop::EventLoop* loop)
{
	std::shared_ptr<RtmpPublisher> publisher(new RtmpPublisher(loop));
	return publisher;
}

int RtmpPublisher::setMediaInfo(MediaInfo mediaInfo)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_mediaIinfo = mediaInfo;

	if (m_mediaIinfo.audioCodecId == RTMP_CODEC_ID_AAC)
	{
		if (m_mediaIinfo.audioSpecificConfigSize > 0)
		{
			m_aacSequenceHeaderSize = m_mediaIinfo.audioSpecificConfigSize + 2;
			m_aacSequenceHeader.reset(new char[m_aacSequenceHeaderSize]);
			uint8_t *data = (uint8_t *)m_aacSequenceHeader.get();
			uint8_t soundRate = 3; //for aac awlays 3
			uint8_t soundSize = 1; //0:8bit , 1:16bit
			uint8_t soundType = 1; //for aac awlays 1

			// audio tag data
			m_audioTag = data[0] = (((RTMP_CODEC_ID_AAC & 0xf) << 4) | ((soundRate & 0x3) << 2) | ((soundSize & 0x1) << 1) | (soundType & 0x1));

			// aac packet type 
			data[1] = 0; // 0: aac sequence header, 1: aac raw data

			memcpy(data+2, m_mediaIinfo.audioSpecificConfig.get(), m_mediaIinfo.audioSpecificConfigSize);

			// 11 90 -- 48000 2, 12 10 -- 44100 2
			uint32_t samplingFrequencyIndex = ((data[2] & 0x07) << 1) | ((data[3] & 0x80) >> 7);
			uint32_t channel = (data[3] & 0x78) >> 3;
			m_mediaIinfo.audioChannel = channel;
			m_mediaIinfo.audioSamplerate = kSamplingFrequency[samplingFrequencyIndex];
		}
		else
		{
			m_mediaIinfo.audioCodecId = 0;
		}
	}

	if (m_mediaIinfo.videoCodecId == RTMP_CODEC_ID_H264)
	{
		if (m_mediaIinfo.spsSize > 0 && m_mediaIinfo.pps > 0)
		{
			m_avcSequenceHeader.reset(new char[4096]);
			uint8_t *data = (uint8_t *)m_avcSequenceHeader.get();
			uint32_t index = 0;

			data[index++] = 0x17; // 1:keyframe  7:avc
			data[index++] = 0;    // 0: avc sequence header

			data[index++] = 0;
			data[index++] = 0;
			data[index++] = 0;

			// AVCDecoderConfigurationRecord
			data[index++] = 0x01; // configurationVersion
			data[index++] = m_mediaIinfo.sps.get()[1]; // AVCProfileIndication
			data[index++] = m_mediaIinfo.sps.get()[2]; // profile_compatibility
			data[index++] = m_mediaIinfo.sps.get()[3]; // AVCLevelIndication
			data[index++] = 0xff; // lengthSizeMinusOne

			// sps nums
			data[index++] = 0xE1; //&0x1f

			// sps data length
			data[index++] = m_mediaIinfo.spsSize >> 8;
			data[index++] = m_mediaIinfo.spsSize & 0xff;
			// sps data
			memcpy(data + index, m_mediaIinfo.sps.get(), m_mediaIinfo.spsSize);
			index += m_mediaIinfo.spsSize;

			// pps nums
			data[index++] = 0x01; //&0x1f
			 // pps data length
			data[index++] = m_mediaIinfo.ppsSize >> 8;
			data[index++] = m_mediaIinfo.ppsSize & 0xff;
			// sps data
			memcpy(data + index, m_mediaIinfo.pps.get(), m_mediaIinfo.ppsSize);
			index += m_mediaIinfo.ppsSize;

			m_avcSequenceHeaderSize = index;
		}
		else
		{
			m_mediaIinfo.videoCodecId = 0;
		}
	}

	return 0;
}

int RtmpPublisher::openUrl(std::string url, int msec, std::string& status)
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
	} while (!m_rtmpConn->IsClosed() && !m_rtmpConn->isPublishing() && timeout>0);
	
	status = m_rtmpConn->getStatus();
	if (!m_rtmpConn->isPublishing())
	{
		std::shared_ptr<RtmpConnection> rtmpConn = m_rtmpConn;
		SOCKET sockfd = rtmpConn->GetSocket();
		m_taskScheduler->AddTriggerEvent([sockfd, rtmpConn]() {
			rtmpConn->Disconnect();
		});
		m_rtmpConn = nullptr;
		return -1;
	}

	m_videoTimestamp = 0;
	m_audioTimestamp = 0;
	m_hasKeyFrame = true;
	if (m_mediaIinfo.videoCodecId == RTMP_CODEC_ID_H264)
	{
		m_hasKeyFrame = false;
	}

	return 0;
}

void RtmpPublisher::close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_rtmpConn != nullptr)
	{		
		std::shared_ptr<RtmpConnection> rtmpConn = m_rtmpConn;
		SOCKET sockfd = rtmpConn->GetSocket();
		m_taskScheduler->AddTriggerEvent([sockfd, rtmpConn]() {
			rtmpConn->Disconnect();
		});
		m_rtmpConn = nullptr;
		m_videoTimestamp = 0;
		m_audioTimestamp = 0;
		m_hasKeyFrame = false;
	}
}

bool RtmpPublisher::isConnected()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_rtmpConn != nullptr)
	{
		return (!m_rtmpConn->IsClosed());
	}

	return false;
}

bool RtmpPublisher::isKeyFrame(uint8_t *data, uint32_t size)
{
	int startCode = 0;
	if (data[0] == 0 && data[1] == 0 && data[2] == 1)
	{
		startCode = 3;
	}
	else if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1)
	{
		startCode = 4;
	}

	int type = data[startCode] & 0x1f;
	if (type == 5 || type == 7) /* sps_pps_idr or idr */
	{
		return true;
	}
	
	return false;
}

int RtmpPublisher::pushVideoFrame(uint8_t *data, uint32_t size)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_rtmpConn == nullptr || m_rtmpConn->IsClosed() || size <= 5)
	{
		return -1;
	}

	if (m_mediaIinfo.videoCodecId == RTMP_CODEC_ID_H264)
	{
		if (!m_hasKeyFrame)
		{

			if (this->isKeyFrame(data, size))
			{
				m_hasKeyFrame = true;
				m_timestamp.reset();
				//m_taskScheduler->addTriggerEvent([=]() {
					m_rtmpConn->sendVideoData(0, m_avcSequenceHeader, m_avcSequenceHeaderSize);
					m_rtmpConn->sendAudioData(0, m_aacSequenceHeader, m_aacSequenceHeaderSize);
				//});
			}
			else
			{
				return 0;
			}
		}

		uint64_t timestamp = m_timestamp.elapsed();
		//uint64_t timestamp_delta = 0;
		//if (timestamp < m_videoTimestamp)
		//{
		//	timestamp = m_videoTimestamp;
		//}
		//timestamp_delta = timestamp - m_videoTimestamp;
		//m_videoTimestamp = timestamp;

		std::shared_ptr<char> payload(new char[size + 4096]);
		uint32_t payloadSize = 0;

		uint8_t *buffer = (uint8_t *)payload.get();
		uint32_t index = 0;
		buffer[index++] = this->isKeyFrame(data, size) ? 0x17: 0x27;
		buffer[index++] = 1;

		buffer[index++] = 0;
		buffer[index++] = 0;
		buffer[index++] = 0;

		buffer[index++] = (size >> 24) & 0xff;
		buffer[index++] = (size >> 16) & 0xff;
		buffer[index++] = (size >> 8) & 0xff;
		buffer[index++] = (size) & 0xff;

		memcpy(buffer + index, data, size);
		index += size;
		payloadSize = index;
		//m_taskScheduler->addTriggerEvent([=]() {
			m_rtmpConn->sendVideoData(timestamp, payload, payloadSize);
		//});
	}

	return 0;
}

int RtmpPublisher::pushAudioFrame(uint8_t *data, uint32_t size)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_rtmpConn == nullptr || m_rtmpConn->IsClosed() || size <= 0)
	{
		return -1;
	}

	if (m_hasKeyFrame && m_mediaIinfo.audioCodecId == RTMP_CODEC_ID_AAC)
	{
		uint64_t timestamp = m_timestamp.elapsed();
		//uint64_t timestamp_delta = 0;
		//if (timestamp < m_audioTimestamp)
		//{
		//	timestamp = m_audioTimestamp;
		//}
		//timestamp_delta = timestamp - m_audioTimestamp;
		//m_audioTimestamp = timestamp;
		
		uint32_t payloadSize = size + 2;
		std::shared_ptr<char> payload(new char[size + 2]);
		payload.get()[0] = m_audioTag;
		payload.get()[1] = 1; // 0: aac sequence header, 1: aac raw data
		memcpy(payload.get() + 2, data, size);
		//m_taskScheduler->addTriggerEvent([=]() {
			m_rtmpConn->sendAudioData(timestamp, payload, payloadSize);
		//});
	}

	return 0;
}