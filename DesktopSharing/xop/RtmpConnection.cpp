#include "RtmpConnection.h"
#include "RtmpServer.h"
#include "RtmpPublisher.h"
#include "RtmpClient.h"
#include "net/Logger.h"
#include <random>

using namespace xop;

RtmpConnection::RtmpConnection(std::shared_ptr<RtmpServer> rtmp_server, TaskScheduler *task_scheduler, SOCKET sockfd)
	: RtmpConnection(task_scheduler, sockfd, rtmp_server.get())
{
	handshake_.reset(new RtmpHandshake(RtmpHandshake::HANDSHAKE_C0C1));
	rtmp_server_ = rtmp_server;
	connection_mode_ = RTMP_SERVER;
}

RtmpConnection::RtmpConnection(std::shared_ptr<RtmpPublisher> rtmp_publisher, TaskScheduler *task_scheduler, SOCKET sockfd)
	: RtmpConnection(task_scheduler, sockfd, rtmp_publisher.get())
{
	handshake_.reset(new RtmpHandshake(RtmpHandshake::HANDSHAKE_S0S1S2));
	rtmp_publisher_ = rtmp_publisher;
	connection_mode_ = RTMP_PUBLISHER;
}

RtmpConnection::RtmpConnection(std::shared_ptr<RtmpClient> rtmp_client, TaskScheduler *task_scheduler, SOCKET sockfd)
	: RtmpConnection(task_scheduler, sockfd, rtmp_client.get())
{
	handshake_.reset(new RtmpHandshake(RtmpHandshake::HANDSHAKE_S0S1S2));
	rtmp_client_ = rtmp_client;
	connection_mode_ = RTMP_CLIENT;
}

RtmpConnection::RtmpConnection(TaskScheduler *task_scheduler, SOCKET sockfd, Rtmp* rtmp)
	: TcpConnection(task_scheduler, sockfd)
	, task_scheduler_(task_scheduler)
	, channel_(new Channel(sockfd))
	, rtmp_chunk_(new RtmpChunk)
	, connection_state_(HANDSHAKE)
{
	peer_bandwidth_ = rtmp->GetPeerBandwidth();
	acknowledgement_size_ = rtmp->GetAcknowledgementSize();
	max_gop_cache_len_ = rtmp->GetGopCacheLen();
	max_chunk_size_ = rtmp->GetChunkSize();
	stream_path_ = rtmp->GetStreamPath();
	stream_name_ = rtmp->GetStreamName();
	app_ = rtmp->GetApp();

	this->SetReadCallback([this](std::shared_ptr<TcpConnection> conn, xop::BufferReader& buffer) {
		return this->OnRead(buffer);
	});

	this->SetCloseCallback([this](std::shared_ptr<TcpConnection> conn) {
		this->OnClose();
	});
}

RtmpConnection::~RtmpConnection()
{
    
}

bool RtmpConnection::OnRead(BufferReader& buffer)
{   
	bool ret = true;

	if (handshake_->IsCompleted()) {
		ret = HandleChunk(buffer);
	}
	else {
		std::shared_ptr<char> res(new char[4096]);
		int res_size = handshake_->Parse(buffer, res.get(), 4096);
		if (res_size < 0) {
			ret = false;
		}

		if (res_size > 0) {
			this->Send(res.get(), res_size);
		}

		if (handshake_->IsCompleted()) {
			if(buffer.ReadableBytes() > 0) {
				ret = HandleChunk(buffer);
			}

			if (connection_mode_ == RTMP_PUBLISHER || 
				connection_mode_ == RTMP_CLIENT) {
				this->SetChunkSize();
				this->Connect();
			}
		}
	}

	return ret;
}

void RtmpConnection::OnClose()
{
	if (connection_mode_ == RTMP_SERVER) {
		this->HandleDeleteStream();
	}
	else if (connection_mode_ == RTMP_PUBLISHER) {
		this->DeleteStream();
	}
}

bool RtmpConnection::HandleChunk(BufferReader& buffer)
{
	int ret = -1;
	
	do
	{
		RtmpMessage rtmp_msg;
		ret = rtmp_chunk_->Parse(buffer, rtmp_msg);
		if (ret >= 0) {
			if (rtmp_msg.IsCompleted()) {
				if (!HandleMessage(rtmp_msg)) {
					return false;
				}
			}

			if (ret == 0) {
				break;
			}
		}
		else if (ret < 0) {
			return false;
		}
	} while (buffer.ReadableBytes() > 0);

	return true;
}

bool RtmpConnection::HandleMessage(RtmpMessage& rtmp_msg)
{
    bool ret = true;  
    switch(rtmp_msg.type_id)
    {        
        case RTMP_VIDEO:
            ret = HandleVideo(rtmp_msg);
            break;
        case RTMP_AUDIO:
            ret = HandleAudio(rtmp_msg);
            break;
        case RTMP_INVOKE:
            ret = HandleInvoke(rtmp_msg);
            break;
        case RTMP_NOTIFY:
            ret = HandleNotify(rtmp_msg);
            break;
        case RTMP_FLEX_MESSAGE:
			LOG_INFO("unsupported rtmp flex message.\n");
			ret = false;
            break;            
        case RTMP_SET_CHUNK_SIZE:           
			rtmp_chunk_->SetInChunkSize(ReadUint32BE(rtmp_msg.payload.get()));
            break;
		case RTMP_BANDWIDTH_SIZE:
			break;
        case RTMP_FLASH_VIDEO:
			LOG_INFO("unsupported rtmp flash video.\n");
			ret = false;
            break;    
        case RTMP_ACK:
            break;            
        case RTMP_ACK_SIZE:
            break;
        case RTMP_USER_EVENT:
            break;
        default:
			LOG_INFO("unkonw message type : %d\n", rtmp_msg.type_id);
            break;
    }

	if (!ret) {
		//printf("rtmp_msg.type_id:%x\n", rtmp_msg.type_id);
	}
		
    return ret;
}

bool RtmpConnection::HandleInvoke(RtmpMessage& rtmp_msg)
{   
    bool ret  = true;
    amf_decoder_.reset();
  
	int bytes_used = amf_decoder_.decode((const char *)rtmp_msg.payload.get(), rtmp_msg.length, 1);
	if (bytes_used < 0) {
		return false;
	}

    std::string method = amf_decoder_.getString();
	//LOG_INFO("[Method] %s\n", method.c_str());

	if (connection_mode_ == RTMP_PUBLISHER || connection_mode_ == RTMP_CLIENT) {
		bytes_used += amf_decoder_.decode(rtmp_msg.payload.get() + bytes_used, rtmp_msg.length - bytes_used);
		if (method == "_result") {
			ret = HandleResult(rtmp_msg);
		}
		else if (method == "onStatus") {
			ret = HandleOnStatus(rtmp_msg);
		}
	}
	else if (connection_mode_ == RTMP_SERVER) {
		if(rtmp_msg.stream_id == 0) {
			bytes_used += amf_decoder_.decode(rtmp_msg.payload.get()+bytes_used, rtmp_msg.length-bytes_used);
			if(method == "connect") {            
				ret = HandleConnect();
			}
			else if(method == "createStream") {      
				ret = HandleCreateStream();
			}
		}
		else if(rtmp_msg.stream_id == stream_id_) {
			bytes_used += amf_decoder_.decode((const char *)rtmp_msg.payload.get()+bytes_used, rtmp_msg.length-bytes_used, 3);
			stream_name_ = amf_decoder_.getString();
			stream_path_ = "/" + app_ + "/" + stream_name_;
        
			if((int)rtmp_msg.length > bytes_used) {
				bytes_used += amf_decoder_.decode((const char *)rtmp_msg.payload.get()+bytes_used, rtmp_msg.length-bytes_used);                      
			}
              
			if(method == "publish") {            
				ret = HandlePublish();
			}
			else if(method == "play") {          
				ret = HandlePlay();
			}
			else if(method == "play2") {         
				ret = HandlePlay2();
			}
			else if(method == "DeleteStream") {
				ret = HandleDeleteStream();
			}
			else if (method == "releaseStream") {

			}
		}
	}

    return ret;
}

bool RtmpConnection::HandleNotify(RtmpMessage& rtmp_msg)
{   
    amf_decoder_.reset();
    int bytes_used = amf_decoder_.decode((const char *)rtmp_msg.payload.get(), rtmp_msg.length, 1);
    if(bytes_used < 0) {
        return false;
    }
        
    if(amf_decoder_.getString() == "@setDataFrame")
    {
        amf_decoder_.reset();
        bytes_used = amf_decoder_.decode((const char *)rtmp_msg.payload.get()+bytes_used, rtmp_msg.length-bytes_used, 1);
        if(bytes_used < 0) {           
            return false;
        }
       
        if(amf_decoder_.getString() == "onMetaData") {
            amf_decoder_.decode((const char *)rtmp_msg.payload.get()+bytes_used, rtmp_msg.length-bytes_used);
            meta_data_ = amf_decoder_.getObjects();
            
			auto server = rtmp_server_.lock();
			if (!server) {
				return false;
			}

            auto session = server->GetSession(stream_path_);
            if(session) {
				session->SetMetaData(meta_data_);
				session->SendMetaData(meta_data_);
            }
        }
    }

    return true;
}

bool RtmpConnection::HandleVideo(RtmpMessage& rtmp_msg)
{  
	uint8_t type = RTMP_VIDEO;
	uint8_t *payload = (uint8_t *)rtmp_msg.payload.get();
	uint32_t length = rtmp_msg.length;
	uint8_t frame_type = (payload[0] >> 4) & 0x0f;
	uint8_t codec_id = payload[0] & 0x0f;

	if (connection_mode_ == RTMP_CLIENT) {
		if (is_playing_ && connection_state_ == START_PLAY) {
			if (play_cb_) {
				play_cb_(payload, length, codec_id, (uint32_t)rtmp_msg._timestamp);
			}			
		}
	}
	else if(connection_mode_ == RTMP_SERVER)
	{
		auto server = rtmp_server_.lock();
		if (!server) {
			return false;
		}

		auto session = server->GetSession(stream_path_);
		if (session == nullptr) {
			return false;
		}

		if (frame_type == 1 && codec_id == RTMP_CODEC_ID_H264) {
			if (payload[1] == 0) {
				avc_sequence_header_size_ = length;
				avc_sequence_header_.reset(new char[length]);
				memcpy(avc_sequence_header_.get(), rtmp_msg.payload.get(), length);
				session->SetAvcSequenceHeader(avc_sequence_header_, avc_sequence_header_size_);
				type = RTMP_AVC_SEQUENCE_HEADER;
			}
		}

		session->SendMediaData(type, rtmp_msg._timestamp, rtmp_msg.payload, rtmp_msg.length);
	}

    return true;
}

bool RtmpConnection::HandleAudio(RtmpMessage& rtmp_msg)
{
	uint8_t type = RTMP_AUDIO;
	uint8_t *payload = (uint8_t *)rtmp_msg.payload.get();
	uint32_t length = rtmp_msg.length;
	uint8_t sound_format = (payload[0] >> 4) & 0x0f;
	//uint8_t sound_size = (payload[0] >> 1) & 0x01;
	//uint8_t sound_rate = (payload[0] >> 2) & 0x03;
	uint8_t codec_id = payload[0] & 0x0f;

	if (connection_mode_ == RTMP_CLIENT) {
		if (connection_state_ == START_PLAY && is_playing_) {
			if (play_cb_) {
				play_cb_(payload, length, codec_id, (uint32_t)rtmp_msg._timestamp);
			}
		}
	}
	else
	{
		auto server = rtmp_server_.lock();
		if (!server) {
			return false;
		}

		auto session = server->GetSession(stream_path_);
		if (session == nullptr) {
			return false;
		}

		if (sound_format == RTMP_CODEC_ID_AAC && payload[1] == 0) {
			aac_sequence_header_size_ = rtmp_msg.length;
			aac_sequence_header_.reset(new char[rtmp_msg.length]);
			memcpy(aac_sequence_header_.get(), rtmp_msg.payload.get(), rtmp_msg.length);
			session->SetAacSequenceHeader(aac_sequence_header_, aac_sequence_header_size_);
			type = RTMP_AAC_SEQUENCE_HEADER;
		}

		session->SendMediaData(type, rtmp_msg._timestamp, rtmp_msg.payload, rtmp_msg.length);
	}

    return true;
}


bool RtmpConnection::Handshake()
{
	uint32_t req_size = 1 + 1536; //COC1  
	std::shared_ptr<char> req(new char[req_size]);
	handshake_->BuildC0C1(req.get(), req_size);
	this->Send(req.get(), req_size);
	return true;
}

bool RtmpConnection::Connect()
{
	AmfObjects objects;
	amf_encoder_.reset();

	amf_encoder_.encodeString("connect", 7);
	amf_encoder_.encodeNumber((double)(++number_));
	objects["app"] = AmfObject(app_);
	objects["type"] = AmfObject(std::string("nonprivate"));

	if (connection_mode_ == RTMP_PUBLISHER) {
		auto publisher = rtmp_publisher_.lock();
		if (!publisher) {
			return false;
		}
		objects["swfUrl"] = AmfObject(publisher->GetSwfUrl());
		objects["tcUrl"] = AmfObject(publisher->GetTcUrl());
	}
	else if (connection_mode_ == RTMP_CLIENT)
	{
		auto client = rtmp_client_.lock();
		if (!client) {
			return false;
		}
		objects["swfUrl"] = AmfObject(client->GetSwfUrl());
		objects["tcUrl"] = AmfObject(client->GetTcUrl());
	}

	amf_encoder_.encodeObjects(objects);
	connection_state_ = START_CONNECT;
	SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
	return true;
}

bool RtmpConnection::CretaeStream()
{
	AmfObjects objects;
	amf_encoder_.reset();

	amf_encoder_.encodeString("createStream", 12);
	amf_encoder_.encodeNumber((double)(++number_));
	amf_encoder_.encodeObjects(objects);

	connection_state_ = START_CREATE_STREAM;
	SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
	return true;
}

bool RtmpConnection::Publish()
{
	AmfObjects objects;
	amf_encoder_.reset();

	amf_encoder_.encodeString("publish", 7);
	amf_encoder_.encodeNumber((double)(++number_));
	amf_encoder_.encodeObjects(objects);
	amf_encoder_.encodeString(stream_name_.c_str(), (int)stream_name_.size());

	connection_state_ = START_PUBLISH;
	SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
	return true;
}

bool RtmpConnection::Play()
{
	AmfObjects objects;
	amf_encoder_.reset();

	amf_encoder_.encodeString("play", 4);
	amf_encoder_.encodeNumber((double)(++number_));
	amf_encoder_.encodeObjects(objects);
	amf_encoder_.encodeString(stream_name_.c_str(), (int)stream_name_.size());

	connection_state_ = START_PLAY;
	SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
	return true;
}

bool RtmpConnection::DeleteStream()
{
	AmfObjects objects;
	amf_encoder_.reset();

	amf_encoder_.encodeString("DeleteStream", 12);
	amf_encoder_.encodeNumber((double)(++number_));
	amf_encoder_.encodeObjects(objects);
	amf_encoder_.encodeNumber(stream_id_);

	connection_state_ = START_DELETE_STREAM;
	SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
	return true;
}

bool RtmpConnection::HandleConnect()
{
    if(!amf_decoder_.hasObject("app")) {
        return false;
    }

    AmfObject amfObj = amf_decoder_.getObject("app");
    app_ = amfObj.amf_string;
    if(app_ == "") {
        return false;
    }

    SendAcknowledgement();
    SetPeerBandwidth();   
    SetChunkSize();

    AmfObjects objects;    
    amf_encoder_.reset();
    amf_encoder_.encodeString("_result", 7);
    amf_encoder_.encodeNumber(amf_decoder_.getNumber());

    objects["fmsVer"] = AmfObject(std::string("FMS/4,5,0,297"));
    objects["capabilities"] = AmfObject(255.0);
    objects["mode"] = AmfObject(1.0);
    amf_encoder_.encodeObjects(objects);
    objects.clear();
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetConnection.Connect.Success"));
    objects["description"] = AmfObject(std::string("Connection succeeded."));
    objects["objectEncoding"] = AmfObject(0.0);
    amf_encoder_.encodeObjects(objects);  

    SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
    return true;
}

bool RtmpConnection::HandleCreateStream()
{ 
	int stream_id = rtmp_chunk_->GetStreamId();

	AmfObjects objects;
	amf_encoder_.reset();
	amf_encoder_.encodeString("_result", 7);
	amf_encoder_.encodeNumber(amf_decoder_.getNumber());
	amf_encoder_.encodeObjects(objects);
	amf_encoder_.encodeNumber(stream_id);

	SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());
	stream_id_ = stream_id;
	return true;
}

bool RtmpConnection::HandlePublish()
{
    LOG_INFO("[Publish] app: %s, stream name: %s, stream path: %s\n", app_.c_str(), stream_name_.c_str(), stream_path_.c_str());

	auto server = rtmp_server_.lock();
	if (!server) {
		return false;
	}

    AmfObjects objects; 
    amf_encoder_.reset();
    amf_encoder_.encodeString("onStatus", 8);
    amf_encoder_.encodeNumber(0);
    amf_encoder_.encodeObjects(objects);

    bool is_error = false;

    if(server->HasPublisher(stream_path_)) {
		is_error = true;
        objects["level"] = AmfObject(std::string("error"));
        objects["code"] = AmfObject(std::string("NetStream.Publish.BadName"));
        objects["description"] = AmfObject(std::string("Stream already publishing."));
    }
    else if(connection_state_ == START_PUBLISH) {
		is_error = true;
        objects["level"] = AmfObject(std::string("error"));
        objects["code"] = AmfObject(std::string("NetStream.Publish.BadConnection"));
        objects["description"] = AmfObject(std::string("Connection already publishing."));
    }
    /* else if(0)  {
        // 认证处理 
    } */
    else {
        objects["level"] = AmfObject(std::string("status"));
        objects["code"] = AmfObject(std::string("NetStream.Publish.Start"));
        objects["description"] = AmfObject(std::string("Start publising."));
		server->AddSession(stream_path_);
    }

    amf_encoder_.encodeObjects(objects);     
    SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size());

    if(is_error) {
        // Close ?
    }
    else {
        connection_state_ = START_PUBLISH;
		is_publishing_ = true;
    }

    auto session = server->GetSession(stream_path_);
    if(session) {
		session->SetGopCache(max_gop_cache_len_);
		session->AddRtmpClient(std::dynamic_pointer_cast<RtmpConnection>(shared_from_this()));
    }        

    return true;
}

bool RtmpConnection::HandlePlay()
{
	LOG_INFO("[Play] app: %s, stream name: %s, stream path: %s\n", app_.c_str(), stream_name_.c_str(), stream_path_.c_str());

	auto server = rtmp_server_.lock();
	if (!server) {
		return false;
	}

    AmfObjects objects; 
    amf_encoder_.reset(); 
    amf_encoder_.encodeString("onStatus", 8);
    amf_encoder_.encodeNumber(0);
    amf_encoder_.encodeObjects(objects);
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetStream.Play.Reset"));
    objects["description"] = AmfObject(std::string("Resetting and playing stream."));
    amf_encoder_.encodeObjects(objects);   
    if(!SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size())) {
        return false;
    }

    objects.clear(); 
    amf_encoder_.reset(); 
    amf_encoder_.encodeString("onStatus", 8);
    amf_encoder_.encodeNumber(0);    
    amf_encoder_.encodeObjects(objects);
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetStream.Play.Start"));
    objects["description"] = AmfObject(std::string("Started playing."));   
    amf_encoder_.encodeObjects(objects);
    if(!SendInvokeMessage(RTMP_CHUNK_INVOKE_ID, amf_encoder_.data(), amf_encoder_.size())) {
        return false;
    }

    amf_encoder_.reset(); 
    amf_encoder_.encodeString("|RtmpSampleAccess", 17);
    amf_encoder_.encodeBoolean(true);
    amf_encoder_.encodeBoolean(true);
    if(!this->SendNotifyMessage(RTMP_CHUNK_DATA_ID, amf_encoder_.data(), amf_encoder_.size())) {
        return false;
    }
             
    connection_state_ = START_PLAY; 
    
    auto session = server->GetSession(stream_path_);
    if(session) {
		session->AddRtmpClient(std::dynamic_pointer_cast<RtmpConnection>(shared_from_this()));
    }  
    
    return true;
}

bool RtmpConnection::HandlePlay2()
{
	HandlePlay();
    //printf("[Play2] stream path: %s\n", stream_path_.c_str());
    return false;
}

bool RtmpConnection::HandleDeleteStream()
{
	auto server = rtmp_server_.lock();
	if (!server) {
		return false;
	}

    if(stream_path_ != "") {
        auto session = server->GetSession(stream_path_);
        if(session != nullptr) {   
			auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
			task_scheduler_->AddTimer([session, conn] {
				session->RemoveRtmpClient(conn);
				return false;
			}, 1);
        }  

		is_playing_ = false;
		is_publishing_ = false;
		has_key_frame_ = false;
		rtmp_chunk_->Clear();
    }

	return true;
}

bool RtmpConnection::HandleResult(RtmpMessage& rtmp_msg)
{
	bool ret = false;

	if (connection_state_ == START_CONNECT) {
		if (amf_decoder_.hasObject("code")) {
			AmfObject amfObj = amf_decoder_.getObject("code");
			if (amfObj.amf_string == "NetConnection.Connect.Success") {
				CretaeStream();
				ret = true;
			}
		}
	}
	else if (connection_state_ == START_CREATE_STREAM) {
		if (amf_decoder_.getNumber() > 0) {
			stream_id_ = (uint32_t)amf_decoder_.getNumber();
			if (connection_mode_ == RTMP_PUBLISHER) {
				this->Publish();
			}
			else if (connection_mode_ == RTMP_CLIENT) {
				this->Play();
			}

			ret = true;
		}
	}

	return ret;
}

bool RtmpConnection::HandleOnStatus(RtmpMessage& rtmp_msg)
{
	bool ret = true;

	if (connection_state_ == START_PUBLISH || connection_state_ == START_PLAY) {		
		if (amf_decoder_.hasObject("code"))
		{
			AmfObject amfObj = amf_decoder_.getObject("code");
			status_ = amfObj.amf_string;
			if (connection_mode_ == RTMP_PUBLISHER) {
				if (status_ == "NetStream.Publish.Start") {
					is_publishing_ = true;					
				}		
				else if(status_ == "NetStream.publish.Unauthorized"
						|| status_ == "NetStream.Publish.BadConnection" /*"Connection already publishing"*/
						|| status_ == "NetStream.Publish.BadName")      /*Stream already publishing*/
				{
					ret = false;
				}
			}
			else if (connection_mode_ == RTMP_CLIENT) {			
				if (/*amfObj.amf_string == "NetStream.Play.Reset" || */
					status_ == "NetStream.Play.Start") {
					is_playing_ = true;
				}
				else if(status_ == "NetStream.play.Unauthorized"
						|| status_ == "NetStream.Play.UnpublishNotify"  /*"stream is now unpublished."*/
						|| status_ == "NetStream.Play.BadConnection")   /*"Connection already playing"*/
				{
					ret = false;
				}
			}
		}
	}

	if (connection_state_ == START_DELETE_STREAM) {
		if (amf_decoder_.hasObject("code")) {
			AmfObject amfObj = amf_decoder_.getObject("code");
			if (amfObj.amf_string != "NetStream.Unpublish.Success") {
				ret = false;
			}
		}
	}

	return ret;
}

bool RtmpConnection::SendMetaData(AmfObjects metaData)
{
    if(this->IsClosed()) {
        return false;
    }

	if (metaData.size() == 0) {
		return false;
	}

    amf_encoder_.reset(); 
    amf_encoder_.encodeString("onMetaData", 10);
    amf_encoder_.encodeECMA(metaData);
    if(!this->SendNotifyMessage(RTMP_CHUNK_DATA_ID, amf_encoder_.data(), amf_encoder_.size())) {
        return false;
    }

    return true;
}

void RtmpConnection::SetPeerBandwidth()
{
    std::shared_ptr<char> data(new char[5]);
    WriteUint32BE(data.get(), peer_bandwidth_);
    data.get()[4] = 2;
    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_BANDWIDTH_SIZE;
    rtmp_msg.payload = data;
    rtmp_msg.length = 5;
    SendRtmpChunks(RTMP_CHUNK_CONTROL_ID, rtmp_msg);
}

void RtmpConnection::SendAcknowledgement()
{
    std::shared_ptr<char> data(new char[4]);
    WriteUint32BE(data.get(), acknowledgement_size_);

    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_ACK_SIZE;
    rtmp_msg.payload = data;
    rtmp_msg.length = 4;
    SendRtmpChunks(RTMP_CHUNK_CONTROL_ID, rtmp_msg);
}

void RtmpConnection::SetChunkSize()
{
	rtmp_chunk_->SetOutChunkSize(max_chunk_size_);
    std::shared_ptr<char> data(new char[4]);
    WriteUint32BE((char*)data.get(), max_chunk_size_);

    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_SET_CHUNK_SIZE;
    rtmp_msg.payload = data;
    rtmp_msg.length = 4;
    SendRtmpChunks(RTMP_CHUNK_CONTROL_ID, rtmp_msg);
}

void RtmpConnection::setPlayCB(const PlayCallback& cb)
{
	play_cb_ = cb;
}

bool RtmpConnection::SendInvokeMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payload_size)
{
    if(this->IsClosed()) {
        return false;
    }

    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_INVOKE;
    rtmp_msg.timestamp = 0;
    rtmp_msg.stream_id = stream_id_;
    rtmp_msg.payload = payload;
    rtmp_msg.length = payload_size; 
    SendRtmpChunks(csid, rtmp_msg);  
    return true;
}

bool RtmpConnection::SendNotifyMessage(uint32_t csid, std::shared_ptr<char> payload, uint32_t payload_size)
{
    if(this->IsClosed()) {
        return false;
    }

    RtmpMessage rtmp_msg;
    rtmp_msg.type_id = RTMP_NOTIFY;
    rtmp_msg.timestamp = 0;
    rtmp_msg.stream_id = stream_id_;
    rtmp_msg.payload = payload;
    rtmp_msg.length = payload_size; 
    SendRtmpChunks(csid, rtmp_msg);  
    return true;
}

bool RtmpConnection::IsKeyFrame(std::shared_ptr<char> payload, uint32_t payload_size)
{
	uint8_t frame_type = (payload.get()[0] >> 4) & 0x0f;
	uint8_t codec_id = payload.get()[0] & 0x0f;
	return (frame_type == 1 && codec_id == RTMP_CODEC_ID_H264);
}

bool RtmpConnection::SendMediaData(uint8_t type, uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size)
{
    if(this->IsClosed()) {
        return false;
    }

	if (payload_size == 0) {
		return false;
	}

	is_playing_ = true;

	if (type == RTMP_AVC_SEQUENCE_HEADER) {
		avc_sequence_header_ = payload;
		avc_sequence_header_size_ = payload_size;
	}
	else if (type == RTMP_AAC_SEQUENCE_HEADER) {
		aac_sequence_header_ = payload;
		aac_sequence_header_size_ = payload_size;
	}

	auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
	task_scheduler_->AddTriggerEvent([conn, type, timestamp, payload, payload_size] {
		if (!conn->has_key_frame_ && conn->avc_sequence_header_size_ > 0
			&& (type != RTMP_AVC_SEQUENCE_HEADER)
			&& (type != RTMP_AAC_SEQUENCE_HEADER)) {
			if (conn->IsKeyFrame(payload, payload_size)) {
				conn->has_key_frame_ = true;
			}
			else {
				return ;
			}
		}

		RtmpMessage rtmp_msg;
		rtmp_msg._timestamp = timestamp;
		rtmp_msg.stream_id = conn->stream_id_;
		rtmp_msg.payload = payload;
		rtmp_msg.length = payload_size;

		if (type == RTMP_VIDEO || type == RTMP_AVC_SEQUENCE_HEADER) {
			rtmp_msg.type_id = RTMP_VIDEO;
			conn->SendRtmpChunks(RTMP_CHUNK_VIDEO_ID, rtmp_msg);
		}
		else if (type == RTMP_AUDIO || type == RTMP_AAC_SEQUENCE_HEADER) {
			rtmp_msg.type_id = RTMP_AUDIO;
			conn->SendRtmpChunks(RTMP_CHUNK_AUDIO_ID, rtmp_msg);
		}
	});
   
    return true;
}

bool RtmpConnection::SendVideoData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size)
{
	if (payload_size == 0) {
		return false;
	}
	
	auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
	task_scheduler_->AddTriggerEvent([conn, timestamp, payload, payload_size] {
		RtmpMessage rtmp_msg;
		rtmp_msg.type_id = RTMP_VIDEO;
		rtmp_msg._timestamp = timestamp;
		rtmp_msg.stream_id = conn->stream_id_;
		rtmp_msg.payload = payload;
		rtmp_msg.length = payload_size;
		conn->SendRtmpChunks(RTMP_CHUNK_VIDEO_ID, rtmp_msg);
	});

	return true;
}

bool RtmpConnection::SendAudioData(uint64_t timestamp, std::shared_ptr<char> payload, uint32_t payload_size)
{
	if (payload_size == 0) {
		return false;
	}

	auto conn = std::dynamic_pointer_cast<RtmpConnection>(shared_from_this());
	task_scheduler_->AddTriggerEvent([conn, timestamp, payload, payload_size] {
		RtmpMessage rtmp_msg;
		rtmp_msg.type_id = RTMP_AUDIO;
		rtmp_msg._timestamp = timestamp;
		rtmp_msg.stream_id = conn->stream_id_;
		rtmp_msg.payload = payload;
		rtmp_msg.length = payload_size;
		conn->SendRtmpChunks(RTMP_CHUNK_VIDEO_ID, rtmp_msg);
	});
	return true;
}

void RtmpConnection::SendRtmpChunks(uint32_t csid, RtmpMessage& rtmp_msg)
{    
    uint32_t capacity = rtmp_msg.length + rtmp_msg.length/ max_chunk_size_ *5 + 1024;
    std::shared_ptr<char> buffer(new char[capacity]);

	int size = rtmp_chunk_->CreateChunk(csid, rtmp_msg, buffer.get(), capacity);
	if (size > 0) {
		this->Send(buffer.get(), size);
	}
}

