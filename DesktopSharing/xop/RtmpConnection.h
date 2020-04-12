#ifndef XOP_RTMP_CONNECTION_H
#define XOP_RTMP_CONNECTION_H

#include "net/EventLoop.h"
#include "net/TcpConnection.h"
#include "amf.h"
#include "rtmp.h"
#include <vector>

namespace xop
{

class RtmpServer;
class RtmpPublisher;
class RtmpClient;

class RtmpConnection : public TcpConnection
{
public:    
	using PlayCallback = std::function<void(uint8_t* payload, uint32_t length, uint8_t codecId, uint32_t timestamp)>;

    enum ConnectionState
    {
        HANDSHAKE_C0C1, 
		HANDSHAKE_S0S1S2,
        HANDSHAKE_C2,
        HANDSHAKE_COMPLETE,
		START_CONNECT,
		START_CREATE_STREAM,
		START_DELETE_STREAM,
        START_PLAY,
        START_PUBLISH,
    };
      
	enum ChunkParseState
	{
		PARSE_HEADER, 
		PARSE_BODY, 
	};
  
	enum ConnectionMode
	{
		RTMP_SERVER, 
		RTMP_PUBLISHER,
		RTMP_CLIENT
	};

    RtmpConnection(std::shared_ptr<RtmpServer> rtmpServer, TaskScheduler* taskScheduler, SOCKET sockfd);
	RtmpConnection(std::shared_ptr<RtmpPublisher> rtmpPublisher, TaskScheduler *taskScheduler, SOCKET sockfd);
	RtmpConnection(std::shared_ptr<RtmpClient> rtmpClient, TaskScheduler *taskScheduler, SOCKET sockfd);
    ~RtmpConnection();

    std::string getStreamPath() const
    { return m_streamPath; }

    std::string getStreamName() const
    { return m_streamName; }

    std::string getApp() const
    { return m_app; }

    AmfObjects getMetaData() const 
    { return m_metaData; }

    bool isPlayer() const 
    { return m_connState == START_PLAY; }

    bool isPublisher() const 
    { return m_connState == START_PUBLISH; }
    
	bool isPlaying() const
	{ return m_isPlaying; }

	bool isPublishing() const
	{ return m_isPublishing; }

	std::string getStatus()
	{ 
		if (m_status == "")
		{
			return "unknown error";
		}
		return m_status; 
	}

private:
    friend class RtmpSession;
	friend class RtmpServer;
	friend class RtmpPublisher;
	friend class RtmpClient;

	RtmpConnection(TaskScheduler *taskScheduler, SOCKET sockfd);

    bool onRead(BufferReader& buffer);
    void onClose();

	int parseChunkHeader(BufferReader& buffer);
	int parseChunkBody(BufferReader& buffer);

	bool handshake();
    bool handleHandshake(BufferReader& buffer);
    bool handleChunk(BufferReader& buffer);
    bool handleMessage(RtmpMessage& rtmpMsg);
    bool handleInvoke(RtmpMessage& rtmpMsg);
    bool handleNotify(RtmpMessage& rtmpMsg);
    bool handleVideo(RtmpMessage& rtmpMsg);
    bool handleAudio(RtmpMessage& rtmpMsg);

	bool connect();
	bool cretaeStream();
	bool publish();
	bool play();
	bool deleteStream();

    bool handleConnect();
    bool handleCreateStream();
    bool handlePublish();
    bool handlePlay();
    bool handlePlay2();
    bool handDeleteStream();
	bool handleResult(RtmpMessage& rtmpMsg);
	bool handleOnStatus(RtmpMessage& rtmpMsg);

    void setPeerBandwidth();
    void sendAcknowledgement();
    void setChunkSize();
	void setPlayCB(const PlayCallback& cb);

    bool sendInvokeMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payloadSize);
    bool sendNotifyMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payloadSize);   
    bool sendMetaData(AmfObjects metaData);
	bool isKeyFrame(std::shared_ptr<char> payload, uint32_t payloadSize);
    bool sendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize);
	bool sendVideoData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize);
	bool sendAudioData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payloadSize);
    void sendRtmpChunks(uint32_t csid, RtmpMessage& rtmpMsg);
    int createChunkBasicHeader(uint8_t fmt, uint32_t csid, char* buf);
    int createChunkMessageHeader(uint8_t fmt, RtmpMessage& rtmpMsg, char* buf);   

	std::weak_ptr<RtmpServer> m_rtmpServer;
	std::weak_ptr<RtmpPublisher> m_rtmpPublisher;
	std::weak_ptr<RtmpClient> m_rtmpClient;

	ConnectionMode m_connMode = RTMP_SERVER;
	TaskScheduler *m_taskScheduler;
	std::shared_ptr<xop::Channel> m_channelPtr;

	uint32_t m_peerBandwidth = 5000000;
	uint32_t m_acknowledgementSize = 5000000;
	uint32_t m_maxChunkSize = 128;
	uint32_t m_maxGopCacheLen = 0;
	uint32_t m_inChunkSize = 128;
	uint32_t m_outChunkSize = 128;
	uint32_t m_streamId = 0;
	uint32_t m_number = 0;
	std::string m_app;
	std::string m_streamName;
	std::string m_streamPath;
	std::string m_status;
	AmfObjects m_metaData;
	AmfDecoder m_amfDec;
	AmfEncoder m_amfEnc;
	std::map<int, RtmpMessage> m_rtmpMessasges;	
	ConnectionState m_connState = HANDSHAKE_C0C1;
	ChunkParseState m_chunkParseState = PARSE_HEADER;
	int m_chunkStreamId = 0;

	bool m_isPlaying = false;
	bool m_isPublishing = false;
	bool m_hasKeyFrame = false;
	std::shared_ptr<char> m_avcSequenceHeader;
	std::shared_ptr<char> m_aacSequenceHeader;
	uint32_t m_avcSequenceHeaderSize = 0;
	uint32_t m_aacSequenceHeaderSize = 0;
	PlayCallback m_playCB;

	const uint32_t kStreamId = 1;
	const int kChunkMessageLen[4] = { 11, 7, 3, 0 };
};
      
}

#endif
