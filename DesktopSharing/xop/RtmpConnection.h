#ifndef XOP_RTMP_CONNECTION_H
#define XOP_RTMP_CONNECTION_H

#include "net/EventLoop.h"
#include "net/TcpConnection.h"
#include "amf.h"
#include "rtmp.h"
#include "RtmpChunk.h"
#include "RtmpHandshake.h"
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
		HANDSHAKE,
		START_CONNECT,
		START_CREATE_STREAM,
		START_DELETE_STREAM,
		START_PLAY,
		START_PUBLISH,
	};

	enum ConnectionMode
	{
		RTMP_SERVER,
		RTMP_PUBLISHER,
		RTMP_CLIENT
	};

    RtmpConnection(std::shared_ptr<RtmpServer> server, TaskScheduler* scheduler, SOCKET sockfd);
	RtmpConnection(std::shared_ptr<RtmpPublisher> publisher, TaskScheduler* scheduler, SOCKET sockfd);
	RtmpConnection(std::shared_ptr<RtmpClient> client, TaskScheduler* scheduler, SOCKET sockfd);
    virtual ~RtmpConnection();

    std::string GetStreamPath() const
    { return stream_path_; }

    std::string GetStreamName() const
    { return stream_name_; }

    std::string GetApp() const
    { return app_; }

    AmfObjects GetMetaData() const 
    { return meta_data_; }

    bool IsPlayer() const 
    { return connection_state_ == START_PLAY; }

    bool IsPublisher() const 
    { return connection_state_ == START_PUBLISH; }
    
	bool IsPlaying() const
	{ return is_playing_; }

	bool IsPublishing() const
	{ return is_publishing_; }

	std::string GetStatus()
	{ 
		if (status_ == "") {
			return "unknown error";
		}
		return status_; 
	}

private:
    friend class RtmpSession;
	friend class RtmpServer;
	friend class RtmpPublisher;
	friend class RtmpClient;

	RtmpConnection(TaskScheduler *scheduler, SOCKET sockfd, Rtmp* rtmp);

    bool OnRead(BufferReader& buffer);
    void OnClose();

    bool HandleChunk(BufferReader& buffer);
    bool HandleMessage(RtmpMessage& rtmp_msg);
    bool HandleInvoke(RtmpMessage& rtmp_msg);
    bool HandleNotify(RtmpMessage& rtmp_msg);
    bool HandleVideo(RtmpMessage& rtmp_msg);
    bool HandleAudio(RtmpMessage& rtmp_msg);

	bool Handshake();
	bool Connect();
	bool CretaeStream();
	bool Publish();
	bool Play();
	bool DeleteStream();

    bool HandleConnect();
    bool HandleCreateStream();
    bool HandlePublish();
    bool HandlePlay();
    bool HandlePlay2();
    bool HandleDeleteStream();
	bool HandleResult(RtmpMessage& rtmp_msg);
	bool HandleOnStatus(RtmpMessage& rtmp_msg);

    void SetPeerBandwidth();
    void SendAcknowledgement();
    void SetChunkSize();
	void setPlayCB(const PlayCallback& cb);

    bool SendInvokeMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payload_size);
    bool SendNotifyMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payload_size);   
    bool SendMetaData(AmfObjects metaData);
	bool IsKeyFrame(std::shared_ptr<char> payload, uint32_t payload_size);
    bool SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);
	bool SendVideoData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);
	bool SendAudioData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size);
    void SendRtmpChunks(uint32_t csid, RtmpMessage& rtmp_msg);

	std::weak_ptr<RtmpServer> rtmp_server_;
	std::weak_ptr<RtmpPublisher> rtmp_publisher_;
	std::weak_ptr<RtmpClient> rtmp_client_;

	std::shared_ptr<RtmpHandshake> handshake_;
	std::shared_ptr<RtmpChunk> rtmp_chunk_;
	ConnectionMode connection_mode_;
	ConnectionState connection_state_;

	TaskScheduler *task_scheduler_;
	std::shared_ptr<xop::Channel> channel_;

	uint32_t peer_bandwidth_ = 5000000;
	uint32_t acknowledgement_size_ = 5000000;
	uint32_t max_chunk_size_ = 128;
	uint32_t max_gop_cache_len_ = 0;
	uint32_t stream_id_ = 0;
	uint32_t number_ = 0;
	std::string app_;
	std::string stream_name_;
	std::string stream_path_;
	std::string status_;

	AmfObjects meta_data_;
	AmfDecoder amf_decoder_;
	AmfEncoder amf_encoder_;

	bool is_playing_ = false;
	bool is_publishing_ = false;
	bool has_key_frame_ = false;
	std::shared_ptr<char> avc_sequence_header_;
	std::shared_ptr<char> aac_sequence_header_;
	uint32_t avc_sequence_header_size_ = 0;
	uint32_t aac_sequence_header_size_ = 0;
	PlayCallback play_cb_;
};
      
}

#endif
